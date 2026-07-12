# Staged Theorem Prover Checking Plan

This document records the current prototype direction for turning the reader and
interpreter into a theorem-prover-capable compiler without forcing every compile
or REPL query to run the full kernel checker.

This is a design note for the current `src/prototype/` implementation. Details
may change as the prototype matures.

## Current State

The current prototype reader first builds a parse AST, then compiles that AST
into an internal term/type representation that is close to a computation graph:

- top-level definitions are stored as source-spanned expectation and assignment
  entries
- parse AST nodes preserve source occurrences and are not structurally interned
- finite constructor namespaces are recorded in `prototype_type_declaration_db` as
  source-derived formation declarations used during AST-to-graph lowering
- `@{...}` is read as an anonymous type literal and can be bound by a name
- `List := \A : @ => @{...}` is represented as a lambda returning a type
- `X.Y` and `(F A).x` are parsed as namespace point-out syntax
- applications, lambdas, matches, constants, variables, type formers, and
  constructor nodes are preserved as internal graph nodes
- structurally identical term and type-expression nodes are interned so repeated
  source fragments such as `(List Bool)` can point at the same graph node after
  AST-to-graph compilation inside
  one compilation context

The prototype also has a minimal universe graph display. The graph is stored in
a separate `prototype_universe_db`; it is not part of `prototype_type_declaration_db`.

At the moment, it only records some type-generation nodes and
parameter-to-type edges, for example:

```text
universe-node #0 type Bool
universe-node #1 type (List A)
universe-node #2 parameter A : TYPE(?u0)
universe-edge #0 parameter-to-type #2 -> #1
```

This is not yet a complete universe checker. It is only the beginning of the
graph structure needed by a later checking pass.

### Type Declaration Index

`prototype_type_declaration_db` is not the type-theoretic home of types. It is a
source-derived declaration/index structure for type formation syntax:

- type former names and arities
- type parameters and their source type expressions
- constructor names, constructor order, and constructor field type expressions
- field lists used to resolve constructor namespaces and match binders

Concrete types live in the term graph as type-formation terms such as:

```text
APP(TYPE_FORMER(Bool), TELESCOPE_NIL)
APP(TYPE_FORMER(List), TELESCOPE_CONS(APP(TYPE_FORMER(Bool), TELESCOPE_NIL), TELESCOPE_NIL))
```

Classifier facts live in JudgementDB as graph-node to graph-node relations. The
type declaration index exists only because lowering and namespace lookup still
need source declaration shape. It should not be treated as a separate semantic
type database.

## Parse AST, Digest, and Graph Metadata

The parser output is not the computation graph used by evaluation or theorem
checking. Parsing is constrained by source order, unresolved names, and
programmer-facing labels. Those concerns should not be treated as core graph
semantics.

This is an intentional PL implementation boundary. The AST exists to absorb the
technical constraints of parsing source text. It is source-facing data, not the
IR that evaluation or theorem checking should operate on.

In particular, AST nodes may contain:

- unresolved source names
- namespace labels that have not been resolved
- aliases that should later collapse to graph-node labels
- source-order artifacts
- programmer-facing binder names
- AST-local binder IDs
- source spans and diagnostics metadata in a later implementation

The computation graph should not permanently inherit those parse artifacts. In
particular, source names and namespace paths should not become ordinary
computation nodes such as `CONST(A.B)`. They should be used by the digest phase
to point out graph nodes, then stored only as labels, exports, imports,
diagnostics, or unresolved-reference metadata.

The AST-local binder ID is not the same thing as the graph binder ID. The parser
allocates AST binder IDs only so that source occurrences under a lambda or match
case can be connected before the graph exists. The AST compiler is responsible
for mapping each AST binder to a fresh graph binder and rewriting bound
variable references and binder type annotations to the graph binder identity.

The current prototype therefore has an explicit AST layer:

```text
source
  -> prototype_ast_db
  -> prototype_ast_inspect_print(...)
  -> digest / prototype_compile_graph(...)
  -> prototype_term_db + compile metadata
  -> derived summaries
```

This keeps the responsibilities separated:

```text
reader.c
  Tokenize and parse source text.
  Build AST nodes and AST-local binder links.

ast_inspect.c
  Inspect the parsed AST before graph compilation.
  This is diagnostic code only; it does not allocate graph terms or graph types.

ast.c
  Digest AST definitions into graph nodes and metadata.
  Resolve local names and namespace point-out syntax where possible.
  Allocate graph binders.
  Compile AST nodes into term graph nodes.

term.c
  Store and evaluate graph terms.
```

The AST layer may contain source-level name references such as `NAME(empty)`.
During digest, top-level names are resolved through the definition table and
become direct references to the target computation node whenever possible.
For example:

```p
empty := (List Bool).nil;
y := empty;
```

should compile so that `y` points at the same computation node as `empty`, not
at a permanent `CONST(empty)` computation node.

For the current single-file compiler, names and namespace point-outs that cannot
be resolved inside the file should become resolve errors rather than graph
constants. Later multi-file compilation may record such references as imports or
unresolved-reference metadata, but they still should not become ordinary graph
nodes.

This separation is important:

```text
AST:
  Programmer syntax and unresolved labels.

Digest:
  Uses names and namespace syntax to point out graph nodes.
  Produces graph nodes plus metadata.

Computation graph:
  Computation structure only.
  Nodes connected to other nodes directly.
  Source names are not computation nodes.

Compile metadata:
  Labels, exports, imports/unresolved references, resolve errors, dependency
  information, and check status.

Derived summaries:
  Universe constraints, dependency summaries, canonical digests, and cache data.
```

This model replaces a strong `UnresolvedGraph` versus `ResolvedGraph`
distinction. The primary artifact is a compiled-file object:

```text
CompiledFileObject:
  graph
  labels
  exports
  imports/unresolved references
  resolve errors
  check status
  summaries
```

Whether a file is self-contained is metadata:

```text
self_contained := imports/unresolved references is empty
valid := resolve errors, type errors, and universe errors are empty
```

The current prototype implements the first minimal version of that metadata:

- `prototype_compile_metadata.labels` maps source-level top-level names to graph
  term IDs.
- `prototype_compile_metadata.resolve_errors` records names and namespace
  point-outs that digest could not resolve.
- successful single-file compiles report `resolve_errors=0 self_contained=yes`
  in the REPL/read-file output.
- unresolved source names are no longer lowered to graph constants; for example
  `x := y;` records `metadata resolve-error kind=name name=y` and fails the
  current single-file graph compile.
- REPL name queries use metadata labels to point out graph nodes, then evaluate
  the pointed-out graph node.

This is still not the final file-object format. It is the first implementation
step toward making source names, exports, unresolved imports, and check status
metadata around the graph rather than graph nodes.

## Core Direction

The reader should stay permissive. It should parse source code into the AST
layer even when the program may later fail type checking.

For example, this should be readable:

```p
bad := \A : @ => A A;
```

The reader and AST compiler should preserve a graph shape such as:

```text
LAMBDA(A, TYPE(?u0), APP(VAR(A), VAR(A)))
```

The checker, not the reader, should reject it if universe constraints become
inconsistent.

This separation keeps the parser simple and makes the compiler pipeline easier
to inspect.

## Computation Graph Interning

The computation graph should behave like a single-assignment graph inside one
compilation context. If the same identifier expression or application structure
appears multiple times, it should resolve to the same internal node whenever the
structure is identical.

For example, this source should not create unrelated namespace roots for each
occurrence of `List Bool`:

```p
empty := (List Bool).nil;
one := (List Bool).cons Bool.true empty;
```

After digest resolves `List` and `Bool` to graph nodes, both occurrences of the
application structure `APP(List_node, Bool_node)` should point to the same graph
node. This matters because later passes such as type-family instantiation and
universe summary generation should be able to attach facts to one graph node
rather than rediscovering the same source fragment repeatedly.

Parse AST nodes and AST type expressions are intentionally not interned. The AST
is the source-facing occurrence layer: if a programmer writes the same expression
twice, the AST should preserve both occurrences and their spans.

Interning belongs to the graph layer. Term graph construction may share
structurally identical computation nodes after names and namespaces have been
resolved. This graph-level interning is an optimization and identity policy; it
does not replace name resolution or type checking.

## Binder Identity

Bound variables should not be represented as name references. Names are
programmer-facing labels used to construct the graph; the core graph should
represent binding by direct binder identity.

The current prototype uses binder IDs for lambda-bound variables:

## Match Case Body Parsing

The prototype keeps the legacy match syntax:

```p
scrutinee @constructor binders => body
          @constructor binders => body;
```

The parser must treat each case body as one syntactic unit. In the current
grammar, an unparenthesized case body is either:

- a lambda term beginning with `\`
- an application term

This is intentional. A naked nested match is not consumed as the body of the
current case, because that creates a dangling-match ambiguity similar to
dangling `else`.

For example, nested boolean branching must be written with parentheses:

```p
xor := \a : Bool => \b : Bool =>
	a @true => (b @true => Bool.false
	              @false => Bool.true)
	  @false => (b @true => Bool.true
	               @false => Bool.false);
```

The unparenthesized form is rejected if it would produce duplicate cases in the
same match:

```p
xor := \a : Bool => \b : Bool =>
	a @true => b @true => Bool.false
	             @false => Bool.true
	  @false => b @true => Bool.true
	             @false => Bool.false;
```

This rejection is deliberately strict. It catches the most common parse hazard:
the programmer intended an inner match body, but the parser would otherwise read
the following `@false` as another case of the outer match. Parentheses are the
explicit boundary for nested match terms.

## Induction Hypothesis Syntax

The prototype supports `*Identifier` as induction-hypothesis syntax inside match
case bodies.

For example:

```p
add := \n : Nat =>
	n @zero => (\m : Nat => m)
	  @succ k => (\m : Nat => Nat.succ (*k m));
add :: Nat -> Nat -> Nat;
```

The `*k` form is not parsed as a source-name reference to `add`, and it is not
attached to the current top-level definition. It is parsed as an AST
induction-hypothesis node that must point to a match-case binder. During graph
compilation, that binder is associated with the match frame that introduced it,
so it becomes:

```text
INDUCTION_HYPOTHESIS(frame#0, VAR(k))
```

This keeps the recursive edge distinct from ordinary source-name lookup. At
evaluation time, the node expands by re-running the same match frame with its
scrutinee replaced by the structurally smaller binder argument. This matters for
definitions such as:

```p
bad := \n : Nat => \m : Nat =>
	m @zero => Nat.zero
	  @succ k => Nat.succ *k;
```

Here `*k` is the induction hypothesis for the match over `m`, not a recursive
call to the top-level definition on its first argument. Guardedness and
termination are not yet checked by a kernel pass; the current parser only
enforces that `*Identifier` refers to a match-case binder rather than an
arbitrary lambda/local variable.

The current implementation stores enough match-frame state in `term_db` for the
prototype evaluator to re-run the same match frame. This is an implementation
shortcut, not the final responsibility boundary. The match frame is primarily
elaboration/checker metadata: it records which match introduced each case binder
and which induction hypotheses refer to that frame. The long-term design should
move frame origin data, case-binder origins, and IH obligations into elaboration
or check metadata, leaving `term.c` focused on graph storage and evaluation.

The missing guardedness/termination pass must eventually validate at least:

- the referenced binder was introduced by the same match frame
- the binder corresponds to a structurally recursive constructor field
- the IH result has the expected motive/result shape for that match
- nested matches cannot accidentally use the wrong frame
- `*Identifier` usage creates an obligation rather than silently becoming an
  evaluator rewrite

Until those checks exist, `INDUCTION_HYPOTHESIS(frame, binder)` should be
treated as an unchecked elaboration artifact.

## Current Technical Debt and Responsibility Problems

This section records the current prototype risks after the AST/Graph/Metadata
split and the MatchFrame-based induction-hypothesis implementation.

## Graph Typing Prototype

The prototype now constructs initial typing information during graph
compilation. This is intentionally not a full type checker. It records
graph-node classifier relations of the form:

```text
subject : classifier
```

For example, `Bool := @{ true : *; false : *; };` produces:

```text
TYPE(Bool) : UNIVERSE(?u0)
CONSTRUCTOR(Bool.true) : TYPE(Bool)
CONSTRUCTOR(Bool.false) : TYPE(Bool)
```

and `Nat.succ : * -> *` produces:

```text
CONSTRUCTOR(Nat.succ) :
  APP(APP(PRIMITIVE(PI), TYPE(Nat)), LAMBDA(_ : TYPE(Nat) => TYPE(Nat)))
```

Internal function types are represented through `PRIMITIVE(PI)` plus ordinary
`APP` and graph-level `LAMBDA` nodes. There is no `ARROW` term node in the graph;
source `A -> B` is lowered to a non-dependent PI shape.

Lambda binder annotations are graph classifier nodes, not `prototype_type_expr`
IDs, but they are not stored inside the `LAMBDA` term node. Ordinary value-level
abstractions are represented with `LAMBDA` as a binder/body computation node:

```text
LAMBDA(x#N, body_node)
```

For example, `id := \A : @ => x : A => x;` now compiles as:

```text
LAMBDA(A#0, LAMBDA(x#1, VAR(x#1)))
```

The binder classifiers are recorded as JudgementDB relations:

```text
VAR(A#0) : UNIVERSE(?u0)
VAR(x#1) : VAR(A#0)
```

Type-forming declarations are represented internally as `TYPE_FORMER` applied
to a telescope value. The graph keeps ordinary `APP(left, right)` as a pure
binary computation node; type formation is one interpretation of `APP` when the
left side is a `TYPE_FORMER`.

The source type-formation syntax introduces a type former definition:

```p
List := \A : @ => @{
  nil  : *;
  cons : A -> * -> *;
};
```

The parser records this as a dedicated AST type-formation node, not as an
ordinary `AST_LAMBDA` whose body happens to be a type literal. Ordinary source
lambdas remain ordinary lambdas. For example:

```p
AliasList := \A : @ => List A;
```

is a normal `LAMBDA`, not a type-former definition, because it does not use the type
formation syntax. This keeps the language rule syntax-derived rather than
accidentally inferred from the body shape. In graph terms, `AliasList Bool` is
an ordinary application unless a later checker gives it a certified type-level
meaning.

AST type parameters carry their own source binder identity, symbol, and
classifier/type expression. The type-formation AST node points at the type
definition; it does not reuse match-case binder storage or carry a parallel
binder list. This keeps type-formation binders and match-pattern binders
separate at the data-structure level.

The important design rule is:

```text
APP is not tagged as value-application or type-application.
APP is just the graph edge pair left/right.
Type formation is represented by APP(TYPE_FORMER(type), telescope).
```

For example:

```text
APP(LAMBDA(...), argument)
  ordinary runtime application; runtime beta reduction is allowed

APP(TYPE_FORMER(Bool), TELESCOPE_NIL)
  zero-parameter type formation

APP(TYPE_FORMER(List),
    TELESCOPE_CONS(APP(TYPE_FORMER(Bool), TELESCOPE_NIL), TELESCOPE_NIL))
  one-parameter type formation
```

Thus `List Bool` is represented as:

```text
APP(TYPE_FORMER(List),
    TELESCOPE_CONS(
      APP(TYPE_FORMER(Bool), TELESCOPE_NIL),
      TELESCOPE_NIL))
```

The telescope terminator `TELESCOPE_NIL` is the empty telescope / unit-like
argument object. It is not the empty type: it represents "there are no more type
formation arguments". Constructors point at these type-formation applications:

```text
CONSTRUCTOR(APP(TYPE_FORMER(Bool), TELESCOPE_NIL).true)
CONSTRUCTOR(APP(TYPE_FORMER(List),
  TELESCOPE_CONS(APP(TYPE_FORMER(Bool), TELESCOPE_NIL), TELESCOPE_NIL)).nil)
```

This treats `Bool.true` as the zero-argument instance of the same general
constructor occurrence form, instead of using it as the special case that drives
the whole constructor design.

Classifier relations are stored in `typing.c` as graph-node metadata. This table
is not an isolated external oracle: `subject` and `classifier` are both graph
term IDs. The current direction is to keep computation nodes minimal and record
computed classifier relations in JudgementDB:

```text
VAR(x#N) : classifier
CONSTRUCTOR(...) : classifier
MATCH(...) : APP(motive, scrutinee)
LAMBDA(...) : PI(...)
```

`LAMBDA` and `MATCH` therefore do not store classifier pointers directly in the
term node. This keeps `APP`, `LAMBDA`, and `MATCH` as computation-graph
constructors and leaves typing/classifier facts in the relation table.

Universe placeholders are graph nodes such as `UNIVERSE(?u0)`. The
natural-number universe level is not assigned here. Future universe checking
should solve constraints over these placeholders after the checked program or
linked program context is known.

The graph compiler now creates these classifier graph nodes and JudgementDB
relations while lowering AST type declarations. `typing.c` is currently just the
classifier-relation database and printer; it no longer performs a separate
post-hoc graph mutation pass.

### JudgementDB Frontier Expansion

JudgementDB should be treated as the current frontier of known classifier
relations. A relation should be inserted only by an expansion rule that inspects
the local graph/type information needed for that rule. This avoids global
checking on every insert while still making the registration boundary explicit.

The current implementation exposes rule-shaped expansion APIs such as:

```text
expand_type_def
expand_constructor_def
expand_lambda_binder
expand_match_pattern
expand_lambda
expand_app
expand_match_motive
expand_match
```

Each expansion grows the frontier only if the subject has the expected local
shape. For example, `expand_app` requires an `APP` subject, and
`expand_constructor_def` requires a `CONSTRUCTOR` subject. These checks are still
shallow, but the API boundary is now correct: `ast.c` does not directly append
arbitrary relations with arbitrary origins.

The `origin` value is therefore provenance for the expansion rule that inserted
the relation. It is not a separate safety mechanism by itself; safety comes from
the expansion function's local checks and from later, stronger validation passes
that may re-expand or normalize from the existing graph and JudgementDB frontier.

`JudgementDelta` is not intended to be a semantic typing environment. It is a
temporary delta of graph annotations created while one compile attempt is in
progress. Successful compilation commits the delta into `JudgementDB`; failed
speculative paths rewind the delta. This keeps local graph annotation information
derivable from the graph and its annotations, while preventing failed expected
compilation paths from polluting the committed judgement frontier.

### Correction: Context Was the Wrong Problem Name

During design discussion, "missing context" was raised as if JudgementDB entries
were invalid because they are stored in a table instead of directly on term
nodes. That framing is wrong for this project.

JudgementDB entries are graph relations:

```text
subject_term_id : classifier_term_id
```

Both sides point at graph terms, so the table is graph metadata, not a separate
source-level typing environment. It is acceptable for open terms such as
`VAR(x#1) : VAR(A#0)` to appear in JudgementDB, because `x#1` and `A#0` are binder
identities in the graph, not source names to be resolved again.

The real remaining problem is not "add an explicit context object". The real
problem is validation:

```text
Does this JudgementDB relation follow from the graph constructors,
definition tables, binder identities, and reduction rules?
```

For example, the checker eventually needs to validate that:

- every binder referenced by a judgement relation was introduced by a graph binder
  construct
- binder classifier relations such as `VAR(x#1) : VAR(A#0)` are consistent with
  the lambda or match-case syntax that introduced the binder
- constructor relations such as `Nat.succ : PI(Nat, Nat)` follow from the
  constructor definition
- match relations such as `MATCH(scrutinee, cases) : APP(P, scrutinee)` follow
  from the generated motive and branch classifiers

In short: JudgementDB is allowed to be the place where classifier relations live.
The missing implementation is a JudgementDB relation validator, not a separate
context system.

Current limitations:

- graph compilation records only classifier relations that current construction
  rules can synthesize
- it does not yet infer/check top-level term definitions
- it does not yet preserve or compare top-level annotations
- induction-hypothesis nodes are typed only when the proof object carries the
  match-frame/case-field context needed to justify the `APP(motive, field)`
  classifier
- match result classifiers are recorded only when all branch classifiers are
  available bottom-up

This graph typing metadata should grow into elaboration/check metadata, not into
evaluator logic.

## Typed Terms, JudgementDB Relations, and Selective Certification

Programmer-written terms are intended to be typed. The language should not grow
an ordinary untyped-term mode for variables or expressions. In source code, a
name occurrence is valid only if it can be connected to one of the following:

- a binder with an explicit classifier, such as `x : A`
- a top-level definition with an annotation or a synthesizable classifier
- a constructor whose classifier follows from a type declaration
- a match-pattern binder whose classifier follows from the matched constructor
- a still-unresolved/free name that must later receive an expected classifier

Therefore, an apparent untyped term is not a normal runtime state. It means that
the name is free/unresolved, an expected classifier has not yet been attached, or
the current compiler implementation failed to synthesize a relation that the
language design requires. The expectation/checking layer for declarations such
as `a : A; a := x;` is not implemented yet, but the design assumption is that
the annotation supplies the expected classifier for `a` and for checking `x`.

The intended system still separates graph construction from full kernel
certification. The computation graph can be built before the full dependent
checker proves every obligation, but well-formed source terms should have
classifier relations either synthesized bottom-up or demanded by an explicit
expectation/checking request.

The current design direction is:

```text
Graph execution layer
  The graph can be evaluated using computation nodes such as APP, LAMBDA,
  CONSTRUCTOR, and MATCH.

JudgementDB relation layer
  Classifier relations that the compiler can construct from local rules are
  registered as `subject : classifier`.

Expectation/checking layer
  Source annotations such as `a : A` should become checking obligations. This
  layer is not implemented yet.

Future validation layer
  A later checker should validate that registered relations follow from graph
  constructors, binder identities, constructor definitions, reductions, and
  universe constraints.
```

For example:

```text
TYPE(Bool) : UNIVERSE(?u0)
CONSTRUCTOR(Bool.true) : TYPE(Bool)
CONSTRUCTOR(Bool.false) : TYPE(Bool)
CONSTRUCTOR(Nat.succ) : PI(TYPE(Nat), TYPE(Nat))
```

These are classifier relations generated from type and constructor definitions.
They should be available even when the surrounding graph has not been fully
validated.

The same applies to match-pattern binders. In:

```p
n @succ k => ...
```

the binder `k` is not an untyped lambda-style binder. Its classifier is
synthesized from the constructor definition and the scrutinee type. If
`Nat.succ : Nat -> Nat`, then the match pattern gives:

```text
k : TYPE(Nat)
```

This should be recorded as a classifier relation with an origin such as:

```text
origin = MatchPattern(Constructor Nat.succ, field 0)
```

Prototype status: this is now implemented for the examples handled by the
current graph compiler. During AST-to-graph compilation, the compiler records
classifier relations for:

```text
TYPE(...)                         [origin: type-def]
CONSTRUCTOR(...)                  [origin: constructor-def]
lambda binder variables           [origin: lambda-binder]
match pattern binder variables    [origin: match-pattern]
```

For parameterized constructor fields, the prototype performs the minimum
substitution needed for examples such as `(List Bool).cons`, so the head field
is recorded as `Bool` and the tail field is recorded as `List Bool` rather than
as the generic `List A`.

The difficult part is not the pattern binder. The difficult part is the type of
the whole match expression. Operationally, a match can evaluate by choosing one
branch. Statically, a dependent theorem-prover checker needs a motive/result
shape:

```text
scrutinee : I
motive : I -> Universe
match scrutinee : motive scrutinee
```

This motive is not required to execute the match node dynamically. It is required
to certify that the match is safe for all inputs and can be trusted as part of a
kernel-checked proof or statically guaranteed region.

Therefore, a well-typed match should have a result classifier. When the compiler
can construct a motive from branch classifiers, it records the match classifier
relation:

```text
P = \x : I => type-level-match(x)
MATCH(scrutinee, cases) : APP(P, scrutinee)
```

The same distinction applies to function application. A well-typed application
must satisfy the usual dependent function obligation:

```text
f : PI(a : A). B(a)
x : A
APP(f, x) : B(x)
```

Top-level annotations should be treated as expectations or certification
boundaries in a future phase, not as already-proven relations. For example:

```p
add := ...
add :: Nat -> Nat -> Nat;
```

should eventually elaborate the annotation into a graph classifier node and use
it as a checking request:

```text
check add_body against PI(Nat, PI(Nat, Nat))
```

Only after such a checker exists should annotation-derived relations be inserted
into JudgementDB. Until then, JudgementDB contains only synthesized classifier
relations, and missing relations should be treated as implementation debt or
unresolved expectation work rather than as a normal untyped-term mode.

### 1. Top-Level Type Expectations Are Preserved But Do Not Drive Synthesis

The reader now accepts a definition such as:

```p
add := ...
add :: Nat -> Nat -> Nat;
```

The `::` annotation is stored as a top-level expectation entry. It is not
lowered into a JudgementDB relation during graph construction. The current typing
pass records only classifier relations that can be synthesized from syntax and
already-known relations. The old inline `name : Type := body` form is rejected.

Current relation sources are:

- type declarations and constructors
- lambda binders
- lambda expressions whose body classifier is already known
- applications whose function has a PI classifier and whose argument
  has a compatible classifier
- match-pattern binders whose constructor field type is known

Remaining debt: top-level annotations should become checker obligations in a
later phase, not graph-construction facts.

### 2. `ast.c` Owns Too Many Compiler Responsibilities

The current `ast.c` file does all of the following:

- lowers AST nodes into graph terms
- maps AST binders to graph binders
- resolves local top-level names
- resolves namespace point-out syntax where possible
- lowers anonymous type declarations
- allocates match frames
- lowers `*Identifier` into induction-hypothesis graph nodes
- emits JudgementDB classifier relations
- emits resolve errors

Recent cleanup split the public compile driver into explicit internal phases:

```text
compile_phase_build_graph
compile_phase_resolve_pending_match_items
compile_phase_infer_pending_types
compile_phase_check_expectations
compile_phase_publish_labels
```

This keeps graph construction, constructor-label resolution, classifier
synthesis, expectation checking, and label/environment publication from being
the same immediate side effect. The implementation is still in `ast.c`, so the
file remains too broad, but the phase boundary now exists in executable code.

This is too much responsibility for one module. The intended split is:

```text
reader.c
  source -> AST

ast_inspect.c
  AST diagnostics only

digest/elaborate pass
  AST + scope + namespace syntax -> graph + elaboration metadata

term.c
  graph storage and evaluation primitives

checker pass
  type checking, IH checking, guardedness, termination, universe checking
```

Keeping all of these responsibilities in `ast.c` will make the checker harder
to reason about and will hide which phase owns each invariant.

### 3. MatchFrame Data Is Still Stored in the Term Database

`INDUCTION_HYPOTHESIS(frame#N, VAR(k))` is the right direction semantically:
`*k` is tied to the match frame that introduced `k`, not to a global function
name.

However, the frame table currently lives in `prototype_term_db` so the evaluator
can re-run a match frame. That is useful for the prototype, but conceptually the
frame table is elaboration/checker metadata. It describes origins and
obligations, not pure computation structure.

Long term, graph nodes may refer to stable frame IDs, but the frame summary
itself should probably live beside the graph as metadata.

### 4. Guardedness and Termination Are Only Syntactically Approximated

The parser currently enforces only that `*Identifier` points to a match-case
binder, not to an arbitrary lambda/local binder. That is necessary but not
sufficient.

The system still does not check that the binder is actually a structurally
recursive subterm of the scrutinee. It also does not check whether the field is a
recursive position of the relevant inductive type.

For example, a future checker needs explicit data like:

```text
match frame F:
  scrutinee = VAR(m)
  constructor = Nat.succ
  binder k = field 0
  field 0 is recursive SELF
  therefore k < m
```

Without that relation, `*k` can be evaluated operationally but has not been
validated as a safe induction hypothesis.

### 5. Constructor Field Metadata Is Too Weak for Guardedness

The type database can show constructor fields, for example `SELF`, but the
checker does not yet expose a structured summary of which fields are recursive
subterms, parameter payloads, or non-recursive indexed arguments.

Guardedness depends on this distinction. The prototype should eventually derive
and store constructor-field summaries that the IH checker can consume directly.

### 6. Match Motives Are Partially Represented

The current match representation is operational: it can reduce a scrutinee by
choosing a case. A theorem-prover-grade eliminator also needs a motive/result
shape:

```text
scrutinee : I
motive : I -> Type
case branches prove motive for each constructor
result : motive scrutinee
```

The prototype now builds a motive when every branch body has a bottom-up
classifier relation. It does not collapse identical branch classifiers into a
constant type. Instead, it records the explicit type-level match under a lambda:

```text
P :=
  LAMBDA(x : I,
    MATCH(x,
      CASE(c1 -> classifier(body1)),
      CASE(c2 -> classifier(body2))))
```

Then the value-level `MATCH` receives a classifier relation in
JudgementDB. The `MATCH` term node itself remains only the computation structure:

```text
MATCH(scrutinee, value-cases) : APP(P, scrutinee)
```

The motive body and the motive application receive a fresh universe classifier:

```text
MATCH(x, type-cases) : UNIVERSE(?u)
APP(P, scrutinee) : UNIVERSE(?u)
P : PI(x : I). UNIVERSE(?u)
```

This is intentionally not a branch-collapse optimization. Even if every branch
classifier is the same graph node, the motive remains a case split until a
normalization/checker pass decides otherwise.

Remaining debt: if any branch body lacks a bottom-up classifier relation, the
current prototype may fail to build the motive. Under the intended language
design, this should not be interpreted as a valid untyped term. It should become
one of:

- an expectation/checking obligation supplied by a source annotation
- an unresolved/free-name error
- an implementation gap in classifier synthesis

Recursive examples involving induction-hypothesis applications now receive
context-checked IH classifier proofs. For the current direct-`SELF` fragment,
`*name` is lowered only from a match-case binder that the resolver confirms is
the structurally recursive field of the active constructor. The prototype also
contains a narrow first-stage recursive motive constraint solver: it introduces
a scrutinee-independent motive classifier constraint `P_`, solves constraints
of the form `P_ = branch_classifier` when all known branch classifiers are
normalization equality, and uses the solved classifier to type the induction hypothesis until the
generated match motive is available. This is a typing-phase seed, not a
provisional `P_` term created by AST lowering.

This is still not a general dependent recursive motive solver. Motives that
need real index refinement or constraints such as `P(constructor k)` expressed
in terms of `P(k)` require a later guarded motive-constraint checker instead of
the current constant-classifier `P_` solver.

### 7. Type Expressions Still Leak Source Names

Term-level namespace point-outs have improved, but binder/type-expression debug
output still contains shapes such as:

```text
CONST(Nat)
APP(CONST(List), CONST(Bool))
```

This shows that type-expression name resolution is not yet separated as cleanly
as term graph resolution. Before kernel type checking, type expressions should
be digested into graph/type references plus metadata rather than retaining
source-name constants.

### 8. Metadata Is Not Yet Layered

The current metadata has labels and resolve errors. Future phases need several
different metadata categories:

```text
Compile metadata:
  labels, source spans, unresolved names

Elaboration metadata:
  namespace resolution, top-level annotations, match frames, case binder origins

Check metadata:
  type obligations, IH obligations, guardedness results, universe constraints
```

Putting all of this into one undifferentiated metadata struct would recreate the
same responsibility problem that currently exists in `ast.c`.

### 9. Evaluation Is Still Ahead of the Checker

The evaluator currently expands induction hypotheses directly. This is useful
for examples such as `07_add.p`, but the kernel must not treat evaluator success
as proof of guardedness or termination.

The correct staged model is:

```text
parse -> elaborate -> collect obligations -> check -> evaluate/normalize
```

The prototype currently allows:

```text
parse -> elaborate -> evaluate
```

for convenience. That shortcut must remain visibly marked as unchecked.

### 10. Single-File Resolution Is Still the Only Real Mode

The current compiler is intentionally strict for single-file mode: unresolved
names are compile errors. That is acceptable for now.

Future multi-file compilation will need compiled-file objects with exports,
imports, unresolved connector metadata, and check states. This should not be
started before the single-file resolved graph and elaboration metadata are
stable.

```text
id := \A : @ => x : A => x

term id := LAMBDA(A#0, TYPE(?u0), LAMBDA(x#1, VAR(A#0), VAR(x#1)))
```

Here `A#0` and `x#1` are binder occurrences. A variable reference points at a
binder ID, not merely at the printed name. This is intentionally different from
a source name such as `A`, which should be resolved through binder, label, or
namespace metadata before it becomes graph structure.

This gives the graph the intended separation:

```text
source-level names
  Labels used by the programmer.

binder IDs
  Internal graph identities for bound variables.

constant/name references
  External or namespace labels that still need resolver/linker treatment.
```

This is a binder-pointer style representation rather than de Bruijn indexing.
It fits the graph-oriented implementation because variable references can point
directly to the binder occurrence they depend on.

There are now two binder-id layers:

- `ast_binder_id`: source/AST identity allocated by the parser
- `binder_id`: computation-graph identity allocated by the AST compiler

The parser must not allocate graph binders. Lambda and match syntax both create
AST binders first. During AST compilation, each lambda binder and each match-case
binder is translated into a graph binder, and every `VAR(...)` that references
that AST binder is rewritten to the corresponding graph binder.

Type annotations that mention bound variables follow the same rule. For example,
in:

```p
id := \A : @ => x : A => x;
```

the parsed annotation `x : A` initially points at the AST binder for `A`. The
compiled term annotation is rewritten to the graph binder for `A`, producing a
debug shape like:

```text
LAMBDA(A#0, TYPE(?u0), LAMBDA(x#1, VAR(A#0), VAR(x#1)))
```

The current binder-ID implementation is still incomplete:

- global constants are still symbol references, not definition IDs
- alpha-equivalent lambdas are not canonicalized across different binder IDs
- serialization of binder-linked graphs has not been designed yet

Lambda binders and match-case binders now use the same binder identity concept.
For example:

```text
MATCH(VAR(lst#1), CASE(cons x#2 xs#3 -> ...))
```

Here `x#2` and `xs#3` are case-local binder occurrences, not plain source
names.

## Universe Constraint Graph

Universe checking should be implemented as a graph pass over the internal
computation graph after parsing and name resolution.

The universe graph is not the primary representation of the program. The primary
representation is the computation graph produced by compiling the parse AST. A
universe graph is a derived compile-time summary, cache, and index computed from
that graph.

In other words:

```text
source
  -> parse AST
  -> computation graph
  -> derived universe constraint summary
  -> solver/checker
```

The reader is not responsible for directly building the universe graph. The
current prototype follows this separation: files are read into the AST, the AST
compiler builds the computation graph, then `prototype_universe_collect(...)`
derives the universe summary.

The intended design is:

```text
reader.c
  Builds the parse AST.

ast.c
  Compiles the AST into the computation graph.

universe_collect.c
  Traverses the computation graph and derives universe constraints.

universe.c
  Stores derived graph summaries, supports consistency checking, and later
  supports cache/incremental-compile use cases.
```

The graph has:

- nodes for universe level variables
- nodes for type-producing expressions
- nodes for type-family applications
- nodes for binder type annotations
- nodes for type literal results
- edges for level constraints

The edge interpretation should be:

```text
edge a -> b weight 0  means  level(b) >= level(a)
edge a -> b weight 1  means  level(b) >= level(a) + 1
```

The key consistency condition is:

```text
No directed cycle may have positive total weight.
```

Equivalently:

- 0-weight cycles are allowed and mean equality of levels
- positive-weight cycles are inconsistent
- after collapsing 0-weight SCCs, positive edges must not form a cycle

This is the graph restriction that allows natural-number universe levels to be
assigned later.

## Why Acyclic DAG Is Not Quite Enough

A plain DAG condition is too strict because equal universe levels naturally form
0-weight cycles:

```text
u >= v
v >= u
```

This is valid and means `u = v`.

The invalid case is:

```text
u >= v + 1
v >= u
```

which implies:

```text
u >= u + 1
```

So the actual condition is not "the graph has no cycles"; it is "the graph has
no positive-weight cycles."

## Staged Pipeline

The intended pipeline is:

```text
read
  Parse source into source-facing AST.
  Preserve source names and namespace syntax in AST form.
  Do not perform full type checking.

inspect-ast
  Print or inspect source-facing AST definitions and type declarations.
  Do not allocate graph terms, graph type expressions, or graph type declarations.

digest
  Use source names and namespace syntax to point out graph nodes.
  Separate programmer language from computation structure.
  Produce graph nodes plus labels, exports, imports/unresolved references, and
  resolve-error metadata.

compile-graph
  Map AST-local binders to graph binders.
  Build the internal computation graph.

single-file-resolve
  In current single-file mode, require every name and namespace point-out to
  resolve inside the file.
  Treat unresolved names as errors, not as `CONST(name)` graph nodes.

collect-universe
  Traverse the internal computation graph.
  Create universe level nodes and weighted constraint edges.
  Do not evaluate program terms.

shallow-check
  Run cheap local checks.
  Reject obvious local positive cycles.
  Check simple finite types, simple type-family applications, and constructor
  namespace shape.

full-check
  Build or merge the full dependency universe graph.
  Check positive-cycle consistency.
  Run conversion, definitional equality, positivity, termination/productivity,
  and kernel proof checking as needed.
```

## Definition State

Each definition should eventually carry a checking state:

```text
unchecked
parsed
resolved
shallow_checked
universe_checked
kernel_checked
```

The REPL should demand only the state needed for the current query.

For example:

- printing a raw term only needs `parsed`
- resolving a name query needs `resolved`
- evaluating simple beta terms may need only `resolved`
- using a definition as a theorem requires `kernel_checked`

This lets the system remain a real theorem prover while avoiding full checking
for every ordinary programming interaction.

## Definition Summary

Each compiled definition should eventually export a compact summary:

```text
definition summary:
  definition name
  term graph root
  labels and exported namespace entries
  imports/unresolved references
  resolve errors
  universe variables
  local universe constraints
  required check state
  cache hash
```

The summary should be the unit of cross-file linking and cache reuse.

For theorem-prover linking, names are not enough. Exports and imports are typed
connectors: a linker must compare the name plus the expected interface, such as
kind, type, namespace role, universe constraints, and transparency. This object
file model is closer to proof-assistant interface files than to a C object file
that only links addresses.

The universe part of this summary is derived data. It should not replace the
definition's computation graph. It exists so that another file or definition can
link against the universe behavior of a definition without re-walking the full
term graph every time.

For example, a `List` definition may export:

```text
definition List:
  term root = LAMBDA(A, TYPE(?u0), TYPE((List A)))
  exports = List, (List A).nil, (List A).cons
  import refs = none
  self-contained = true
  universe summary:
    params: ?u0
    constraints:
      level((List A)) >= level(A)
      level(A) >= level((List A))
    local result:
      no positive cycle
```

When another file uses `List Bool`, the checker should be able to instantiate
the summary instead of re-analyzing the entire `List` body from scratch.

Local simplification can happen inside a definition:

- collapse 0-weight SCCs
- reject local positive cycles
- store unresolved dependency edges for later linking
- keep polymorphic universe variables in the exported summary

However, a definition should not assume that all constraints are globally closed
until its dependencies are connected.

## Global Graph Construction

The full program graph is built by connecting definition summaries.

This is where import references in compiled-file metadata become edges to actual
exported definitions. Some universe cycles may appear only after this linking
step, so local checking is not enough.

The global graph should be constructed incrementally:

1. load definition summaries
2. connect imports to compatible provider exports
3. instantiate polymorphic universe variables where needed
4. merge local constraints into the global constraint graph
5. check positive cycles only for the affected dependency slice

The goal is to avoid rebuilding the entire theorem-prover graph when only one
small definition changes.

## Practical Modes

The command-line and REPL should eventually expose modes similar to:

```text
read-only
  Parse and inspect the AST.

graph
  Parse source, compile the AST to the computation graph, and print graph state.

shallow
  Resolve names and run cheap local checks.

universe
  Build universe constraints and check consistency.

kernel
  Run full theorem-prover checking.
```

The default prototype mode can remain light while the kernel mode becomes
strict.

## Progress Ledger

This section records the current issue ledger discussed during the prototype
refactor. The list is intentionally concrete so that later changes can be judged
against the intended responsibility boundaries.

### 1. Parse AST Must Not Be the Computation Graph

Status: implemented for the current prototype scope.

Problem:

The first reader implementation treated parser output as if it were already the
computation graph. That made source-order artifacts, unresolved names, and
programmer-facing construction syntax leak into the representation intended for
evaluation and later theorem-prover checking.

Current state:

- `reader.c` builds `prototype_ast_db`.
- `prototype_read_ast_file(...)` and `prototype_read_ast_string(...)` read AST
  without compiling graph terms.
- `prototype_ast_inspect_print(...)` inspects parsed AST state.
- `prototype_compile_graph(...)` is the named graph compilation phase.
- `prototype_ast_compile_pending(...)` performs the AST-to-graph work under
  that graph compile phase.
- The document now treats AST as source-facing data, not evaluation IR.

Remaining work:

- add explicit AST source spans for diagnostics
- expose a command-line mode that stops after AST read/inspect without compiling
  the graph
- add tests that assert AST read does not allocate graph terms or graph types

### 2. Lambda and Match Binders Must Share One Binding Model

Status: implemented for graph binder identity, incomplete for full match
checking.

Problem:

Match-case variables and lambda variables were previously at risk of becoming
different concepts. That would make substitution, evaluation, alpha-equivalence,
and later kernel checking unnecessarily complicated.

Current state:

- lambda binders and match-case binders both compile to graph `binder_id`
  values
- AST match binders use `ast_binder_id`
- AST compilation maps match binders and lambda binders through the same
  `ast_binder_id -> binder_id` mapping

Remaining work:

- validate constructor arity for each match case
- validate that match cases belong to the scrutinee type namespace
- represent pattern coverage and exhaustiveness separately from evaluation
- intern `MATCH` nodes after case storage has stable graph identity

### 3. Top-Level Names Should Become Labels for Graph Nodes

Status: partially implemented, with the key remaining work now in digest
metadata.

Problem:

Programmer names such as `empty`, `x`, and `main` should point out graph nodes.
They should not remain as permanent computation nodes when the target definition
is known. For example:

```p
empty := (List Bool).nil;
y := empty;
```

should make `y` point at the same graph node as `empty` after digest, not at a
permanent `CONST(empty)` term.

Current state:

- AST `NAME(...)` references are resolved through the AST definition table during
  graph compilation where possible
- compiled definitions are cached in `prototype_ast_def.compiled_term`
- value entries in `prototype_term_db` currently act as programmer-facing labels

Remaining work:

- make label/export metadata explicit instead of overloading term values
- make single-file unresolved names compile errors
- for later multi-file mode, represent unresolved names as import metadata, not
  as ordinary graph constants
- define the exact behavior for recursion and mutually recursive definitions
- separate label tables from term storage

### 4. Repeated Source Structure Should Share Graph Nodes

Status: partially implemented.

Problem:

Repeated expressions such as `(List Bool)` should not create unrelated graph
nodes in one compilation context. Later passes should be able to attach facts to
one node rather than rediscovering equivalent source fragments repeatedly.

Current state:

- many term nodes are structurally interned
- many type-expression nodes are structurally interned
- repeated namespace expressions such as `(List Bool).nil` can reuse the same
  application structure before selecting the namespace member

Remaining work:

- intern `MATCH` nodes
- define canonical identity for type-family instantiations
- distinguish structural interning from semantic equality
- add tests that assert repeated `(List Bool)` occurrences produce the expected
  shared node IDs

### 5. Finite Types and Indexed Type Families Need One Conceptual Model

Status: conceptually decided, but namespace point-out still needs digest work.

Problem:

`Bool` and `List` should not be treated as unrelated implementation categories.
`Bool` is a finite type namespace. `List` can remain a lambda in the raw graph:
it is a value that, when applied to an argument such as `Bool`, produces a type
namespace:

```p
Bool := @{
	true : *;
	false : *;
};

List := \A : @ => @{
	nil : *;
	cons : A -> * -> *;
};
```

The namespace for constructors should therefore be produced by the resulting
type object:

```p
(List Bool).nil
(List Bool).cons
```

Current state:

- `Bool` compiles to a graph type node
- `List` compiles to `LAMBDA(A#..., TYPE(?u...), TYPE((List A)))`
- constructor namespaces can be accessed with both `Bool.true` and
  `(List Bool).nil`

Remaining work:

- make `Bool.true` digest to the actual constructor graph node
- make `(List Bool).nil` digest by evaluating/interpreting the namespace
  expression enough to point out the actual constructor graph node
- keep `List` as a lambda rather than adding a raw `TYPE_FAMILY` graph node
- record type-family namespace behavior in metadata/summary, not as source-name
  strings inside the graph

### 6. UniverseGraph Must Be Derived Data

Status: separation implemented, collector still shallow.

Problem:

The universe graph should not be built directly by the parser. It is a
compile-time summary derived from the computation graph, used for checking,
caching, and incremental linking.

Current state:

- `reader.c` no longer directly populates `prototype_universe_db`
- `prototype_universe_collect(...)` runs after reading/AST compilation
- the universe graph display is separate from `prototype_type_declaration_db`

Remaining work:

- make the collector traverse the full computation graph
- collect weighted universe constraints
- collect constraints from lambda annotations, applications, type-family
  applications, type literals, and match branches
- reject positive-weight cycles
- store per-definition universe summaries

### 7. Binder Identity Is Not Yet Canonical Equality

Status: binder identity implemented, canonical form not implemented.

Problem:

Binder identity makes graph construction and evaluation convenient, but it is
not enough for canonical equality. These two terms should eventually compare as
alpha-equivalent even if their graph binder IDs differ:

```p
\x : A => x
\y : A => y
```

Current state:

- graph variables point to graph binders, not source names
- source-level binder names are only display labels after compilation
- lambda and match binders share the same graph binder concept

Remaining work:

- add a canonical representation, likely de Bruijn-like or locally nameless
- implement conversion between binder-pointer graph form and canonical form
- define equality/hash behavior for canonical terms
- define serialization of binder-linked graphs

### 8. Type Expressions Still Cross the AST/Graph Boundary

Status: implemented for the current prototype scope.

Problem:

The parser used to construct `prototype_type_expr` entries while parsing. Bound
type variables inside annotations temporarily stored AST binder IDs in the same
field later used for graph binder IDs. That was a leftover from the earlier
design where the parser directly constructed the computation graph.

Current state:

- `reader.c` now constructs AST type expressions, not graph type expressions
- AST type expressions use AST-local binder IDs
- AST compilation converts source type expressions into graph
  `prototype_type_expr` nodes
- `@{...}` bodies are stored as AST type declarations
- AST type declarations store source-level parameters, constructors, and
  constructor field type expressions
- AST compilation creates source-derived `prototype_type_declaration` entries from those source
  definitions
- repeated AST universe variables map to the same graph universe variable inside
  one compile context
- type annotations and constructor field types are both compiled through the
  same AST-type-expression compiler
- examples `01_bool.p` through `04_match.p` compile with this separation

Remaining work:

- add tests for nested binders and shadowing in type annotations
- add tests that assert `reader.c` does not allocate graph type expressions or
  graph type declarations during parsing
- split the public API so callers can inspect parsed AST type declarations before
  AST compilation
- add diagnostics that report source locations for invalid constructor type
  expressions

### 9. Evaluation Must Not Destroy the Syntax Graph

Status: partially implemented.

Problem:

The system needs to preserve the compiled term graph while also being able to
show evaluated values. For example, querying `x` should show both the original
term and the evaluated value:

```text
term x := APP(..., CONST(Bool.true))
value x := CONST(Bool.true)
```

Current state:

- REPL/state output prints registered terms
- query output prints both term and value
- `main` evaluation prints the compiled state first, then the evaluated `main`

Remaining work:

- separate normalization/evaluation results from the original term database
- decide whether evaluated values should also be interned graph nodes
- define reduction modes for theorem-prover use
- avoid conflating interpreter evaluation with kernel conversion checking

### 10. Namespace and Linking Semantics Are Still Too String-Like

Status: unresolved.

Problem:

The current namespace constants still carry symbol-oriented identities such as
`CONST(Bool.true)` or generated display names for `(List Bool).nil`. This mixes
programmer language into the graph. It is useful for debug printing, but too
weak for a serious theorem-prover implementation.

Current state:

- `X.Y` parses as a namespace lookup
- `(F A).x` parses as lookup in a computed namespace
- debug output shows names such as `CONST(Bool.true)` and
  `CONST(((\A : TYPE(?u0) => @{(List A)}) @{Bool}).nil)`

Remaining work:

- stop compiling namespace syntax into `CONST(A.B)`-style graph nodes
- digest namespace point-out syntax to actual graph nodes when possible
- make unresolved namespace point-outs single-file compile errors
- for future multi-file mode, store unresolved namespace point-outs as import
  metadata with expected connector shape
- define export metadata for namespace members and type-produced constructors
- make linker compatibility compare name plus interface, not name alone

## Current Remaining Tasks

The current prototype can read and compile `examples/01_bool.p` through
`examples/04_match.p`, but the following tasks remain before the representation
is suitable as the basis for a theorem prover:

- split source type expressions from graph type expressions
- implement single-file Name/Namespace Digest
- replace `CONST(name)` fallbacks with label/export metadata or compile errors
- add CompiledFileObject metadata: labels, exports, imports/unresolved refs,
  resolve errors, closed/self-contained flag, and check status
- make namespace point-out independent of display strings
- make `MATCH` graph nodes internable
- add constructor arity and namespace checks for match cases
- implement full universe constraint collection from graph terms
- add weighted constraint solving with positive-cycle detection
- add per-definition summaries for incremental checking
- add canonical term representation for alpha-equivalence and stable hashing
- define serialization for binder-linked graph terms
- define strict check states: parsed, resolved, universe-checked, kernel-checked
- add tests for shadowing, repeated `(List Bool)`, alias collapse, match binders,
  and type annotation remapping

## Current Implementation Gap

The current prototype does not yet implement the full staged checker.

Missing pieces include:

- a digest/resolver pass that points names and namespace syntax to graph nodes
  instead of producing constants
- compiled-file metadata for labels, exports, imports/unresolved refs, resolve
  errors, and self-contained status
- a full universe constraint collector over all relevant terms and type
  expressions
- weighted edges for application, lambda, type-family application, and universe
  lifting
- 0-weight SCC collapsing
- positive-cycle detection
- per-definition summaries
- incremental dependency-slice checking
- a strict kernel-check state

The immediate next implementation target should be:

```text
prototype_digest_namespaces(program)
```

This pass should run as part of graph compilation for single-file mode. It
should use AST names and namespace syntax only to point out graph nodes. It
should not leave source-name constants in the graph. For now, unresolved names
or namespace point-outs should be single-file compile errors.

It should produce or update compiled-file metadata:

- labels from programmer names to graph nodes
- exports and namespace exports
- resolve errors
- a closed/self-contained flag
- placeholders for future imports/unresolved refs in multi-file mode

After that, the next substantial target is:

```text
prototype_universe_collect(program)
```

That pass should run after digest and graph compilation, traversing the internal
computation graph without evaluating it.

The current `prototype_universe_db` is already populated by
`prototype_universe_collect(...)` rather than directly by the reader. However,
the collector is still shallow: it currently derives type and parameter nodes
from `prototype_type_declaration_db`, but it does not yet walk all term/type-expression
nodes needed for complete theorem-prover universe checking.

It should collect enough information to answer:

- which definitions generate types
- which definitions generate type families
- which binders introduce fresh universe variables
- which applications instantiate type families
- which constraints are local
- which constraints depend on unresolved/global names

Only after that pass exists should the solver and staged check states be added.

## Current Def-Set Reader Boundary

Status: implemented in the prototype reader and graph compiler.

The source language is now treated as a set of top-level definitions and
declarations rather than as one accepted computation tree. The parser stores two
source-facing collections:

- external/global declarations from `name : TypeExpr;`
- type expectations from `name :: TypeExpr;`
- term assignments from `name := Term;`

The old inline form:

```text
name : TypeExpr := Term;
```

is intentionally rejected. Top-level `:` now means a body-less declaration for a
global or external name. It cannot be combined with `:=`.

The source should instead use a term assignment plus a separate expectation:

```text
name := Term;
name :: TypeExpr;
```

or an expression-level ascription:

```text
name := (Term :: TypeExpr);
```

Expression-level `term :: TypeExpr` may appear anywhere a term can appear. Its
runtime/evaluation value is the term itself; the `:: TypeExpr` part is a
compiler checking obligation. The current prototype records the AST node but
does not yet enforce the obligation.

This split is intentional:

- `:=` assignments are the source data that is lowered into the computation
  graph.
- `:` declarations introduce external/global names whose body may be supplied by
  another file, intrinsic namespace, or future linker.
- `::` expectations are checking obligations. They are not yet enforced by the
  kernel/checking pass.
- a top-level name may have at most one primary definition: either one
  `name : TypeExpr;` declaration or one `name := Term;` assignment
- multiple `name :: TypeExpr;` obligations are allowed; they are check requests,
  not primary definitions

This does not introduce a general untyped-term mode. If a top-level name appears
without a known classifier, that is either:

- a free or unresolved name, or
- an expectation/checking obligation that has not yet been certified

The long-term checker should therefore validate expectation entries against the
compiled graph and JudgementDB frontier, rather than storing annotations on graph
nodes as if they were part of evaluation.

### DefIndex and Source-Spanned Diagnostics

Status: implemented for top-level expectations and assignments.

The AST database now builds a name index while parsing. Assignment and
expectation entries remain in source-order storage for inspection, diagnostics,
and future dependency analysis, but lookup is no longer a flat last-wins scan.

The shape is:

```text
assignment_entries[]       source entries, spans, graph AST roots
expectation_entries[]      source entries, spans, type expressions
def_index[]                open-addressing hash from symbol_id to per-name lists
assignment.next_for_symbol intrusive list within assignment_entries[]
expectation.next_for_symbol intrusive list within expectation_entries[]
```

This gives the compiler two important properties:

- duplicate definitions are preserved as source data instead of overwritten
- ambiguous names do not accidentally receive the meaning of the last entry

Compile-time name resolution must go through the DefIndex. A name with exactly
one assignment is resolvable. A missing name is unresolved. A name with multiple
assignments is ambiguous. Best-effort graph compilation skips ambiguous
top-level assignments and continues compiling independent unique definitions.

Diagnostics are tied back to source spans:

- duplicate definition diagnostics use the definition entry's name span
- ambiguous reference diagnostics use the referencing AST node span
- AST nodes carry spans for source references and graph-building constructs
- AST type expressions also carry spans, so future expectation/type-expression
  diagnostics can point to the exact source occurrence

AST nodes and AST type expressions are source occurrences, not canonical graph
nodes. They should therefore not be structurally interned. Any structural
sharing or canonicalization must happen after AST-to-graph compilation.

### Compiler Code Literal Primitive

Status: first prototype implemented.

The prototype now has a compiler-domain text literal for source-like data. This
literal is intended for data in the same broad representation class as input
program text. It is not yet a target-language string, machine byte array, or
fully designed negative/opaque type. It is a compiler primitive that can later
be connected to the intrinsic `#` namespace design.

The source forms are:

```p
code := #"hello";
```

If the delimiterless form starts with a newline, that newline is considered the
literal opener and is not included in the stored text:

```p
code := #"
hello";
```

and:

```p
code := #"DELIM
hello "quoted" text
DELIM";
```

The delimiter form is:

```text
#"<delimiter>\n<body><delimiter>"
```

`delimiter` uses the same character class as an ordinary identifier:

```text
[A-Za-z_][A-Za-z0-9_]*
```

The delimiter does not need to start at the beginning of a line. This is
intentional. It lets source control the final newline directly:

```p
with_newline := #"END
abc
END";

without_newline := #"END
abcEND";
```

The lexer chooses the delimiter form only when the identifier immediately after
`#"` is followed by a newline. Otherwise the literal is delimiterless and ends
at the next `"`. The delimiterless form cannot contain a raw `"` in the body;
use a delimiter when the body needs quotes.

AST and graph lowering are deliberately small:

- AST uses `PROTOTYPE_AST_CODE_LITERAL`.
- Graph uses `PROTOTYPE_TERM_CODE_LITERAL`.
- The classifier is registered in JudgementDB as
  `CODE_LITERAL(...) : PRIMITIVE(Code)`.

The prototype also has the first negative intrinsic function:

```p
x := #.print #"ok";
```

This lowers to:

```text
APP(CONST(#.print), CODE_LITERAL("ok"))
```

with judgement facts:

```text
CODE_LITERAL("ok") : PRIMITIVE(Code)
CONST(#.print) : PRIMITIVE(Code) -> PRIMITIVE(Code)
APP(CONST(#.print), CODE_LITERAL("ok")) : PRIMITIVE(Code)
```

`#.print` is intentionally not represented as `PRIMITIVE(print)`. It is an
opaque negative intrinsic constant: the compiler knows its classifier, but the
ordinary evaluator does not reduce it. A later host/backend/runtime layer may
interpret the constant as an output operation. This keeps primitive type
constants such as `PRIMITIVE(Code)` separate from negative functions supplied by
the compiler environment.

Two conversion intrinsics are also present:

from_code := #.code_to_nat #"12";
back_to_code := #.nat_to_code from_code;
main := #.print back_to_code;
```

Their intended classifiers are:

```text
#.code_to_nat : #.Code -> #.Nat
#.nat_to_code : #.Nat -> #.Code
```

The current debug graph prints the canonical internal classifiers:

```text
CONST(#.code_to_nat) : PRIMITIVE(Code) -> APP(TYPE_FORMER(#.Nat), TELESCOPE_NIL)
CONST(#.nat_to_code) : APP(TYPE_FORMER(#.Nat), TELESCOPE_NIL) -> PRIMITIVE(Code)
```

Here `#.Nat` is not `PRIMITIVE(Nat)`. It is installed as a system-provided type
definition, equivalent to source-level code in the intrinsic namespace:

```p
#.Nat := @{
	zero : *;
	succ : * -> *;
};
```

This means a user-defined `Nat` and the system-provided `#.Nat` are distinct
types unless a later equality mechanism proves or relates them explicitly. For
example, a user may define `Nat` with different constructor names such as `s`
and `z`; that must not be silently identified with `#.Nat`.

At host-intrinsic execution time, `#.code_to_nat` accepts decimal `Code` and
builds the corresponding Peano value of `#.Nat`; `#.nat_to_code` observes a
Peano value of `#.Nat` and produces decimal `Code`.

This is a prototype primitive, not the final account of `#.Code`. The final
design still needs to decide how intrinsic namespaces, atoms, target-only
observations, and opaque negative types are represented.
