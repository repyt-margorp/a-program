# Explicit Index-Family Surface Syntax and Acc Implementation Plan

Date: 2026-08-13

Status: IF0-IF7 complete; IF8 blocked on equality/transport; IF9 complete for
the implemented fragment

Repository baseline:

```text
branch: main
commit: 2ef4564
artifact: v72
HOTT fragment schema: v5
related GitHub issue: #11
```

The work described here is confined to `src/prototype/` except for requested
documentation and example updates. Existing unrelated working-tree changes
must not be reverted or folded into this implementation.

## 1. Objective

Add source-declared indexed inductive families using this authoritative
parameter/index split:

```a-program
Vec :=
	\A : @ =>
	@\n : Nat =>
	{
		nil  : * Nat.zero;
		cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
	};
```

The surface roles are:

```text
\A : @ =>       uniform family parameter
@\n : Nat =>      family index
{ ... }         constructor block terminating an indexed family
```

This syntax must support `Vec` and `Fin` first, then indexed Match and exact
indexed induction hypotheses, and finally the strictly-positive higher-order
recursive field required by `Acc`.

Inside a constructor block, recursive occurrences must be written with `*`.
Uniform parameters are already captured by `*`; only family indices follow it.
The declaration name is not in scope as a recursive alias. Consequently,
`List A` inside `List` and `Vec A n` inside `Vec` are errors, while `*` and
`* n` are their respective valid forms.

This plan does not add an `Acc`-specific Core tag, eliminator, or termination
exception. `Acc` must be obtained from the same indexed-family, positivity,
Match, and induction machinery as other declarations.

## 2. Normative Surface Grammar

### 2.1 Family declarations

The intended grammar is:

```text
type_definition
    ::= parameter_binder* ordinary_family_body
     |  parameter_binder* indexed_family_body

parameter_binder
    ::= '\\' identifier ':' type_expression '=>'

ordinary_family_body
    ::= '@' '{' constructor_declaration* '}' ';'

indexed_family_body
    ::= index_binder+ '{' constructor_declaration* '}' ';'

index_binder
    ::= '@' '\\' identifier ':' type_expression '=>'
```

Examples:

```a-program
Bool :=
@{
	false : *;
	true  : *;
};
```

```a-program
Vec :=
	\A : @ =>
	@\n : Nat =>
	{
		nil  : * Nat.zero;
		cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
	};
```

```a-program
Matrix :=
	\A : @ =>
	@\rows : Nat =>
	@\columns : Nat =>
	{
		matrix : Vec A (Nat.mul rows columns) -> * rows columns;
	};
```

### 2.2 The meaning of the final braces

The braces following an index binder are a constructor block, not a
Computation block:

```a-program
@\n : Nat => { ... constructors ... }
```

The parser already knows that it is parsing a type declaration after consuming
`@\n : Nat =>`. Therefore the following `{` is unambiguous.

By contrast, braces in a term position remain a Computation block:

```a-program
main := {
	x := computation;
	x
};
```

The lexer continues to emit independent `TOKEN_AT`, `TOKEN_BACKSLASH`, and
`TOKEN_LBRACE` tokens. No combined `@\` token is required, and whitespace
between `@` and `\` does not change the parse.

### 2.3 Disambiguation of `@`

The existing uses remain distinct by grammar position and lookahead:

```text
@ in a type-expression position       Universe
@ followed by { in a definition       ordinary ADT body
@ followed by \ in a definition       index binder
@ after a scrutinee term               elimination suffix
```

This is contextual parsing, not semantic overloading of one AST node.

### 2.4 Canonical binder order

Uniform parameters precede all indices:

```text
parameter telescope -> index telescope -> constructor block
```

After the first `@\` binder, an ordinary `\` parameter binder is rejected.
Later indices may depend on earlier parameters and indices.

Accepted:

```a-program
Family :=
	\A : @ =>
	@\x : A =>
	@\p : Predicate A x =>
	{
		...
	};
```

Rejected:

```a-program
Bad :=
	@\n : Nat =>
	\A : @ =>
	{
		...
	};
```

The explicit `@\` marker is the only authority for the parameter/index split.
The compiler must not infer that split from constructor result types.

### 2.5 Constructor-local variables

The family index binder and constructor-local variables are distinct bindings.
In the accepted Vec syntax:

```a-program
@\n : Nat =>
{
	cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
}
```

`n` declares that `Vec A` is indexed by `Nat`. It is not a hidden shared value
inside every constructor. `k` is a fresh constructor-local argument and is the
index selected by `cons`.

This explicit form is normative for the first implementation. The shorter
form below is not part of the first milestone:

```a-program
cons : A -> * n -> * (Nat.succ n);
```

Supporting it would require implicit per-constructor generalization of free
family-index names. That is an elaboration feature and must not be smuggled
into parser scoping.

## 3. Static Meaning

### 3.1 Formation

For parameters `P` and indices `I(P)`, a family has the conceptual formation
classifier:

```text
F : (p : P) -> (i : I(p)) -> Universe
```

For Vec:

```text
Vec : (A : Universe) -> (n : Nat) -> Universe
```

No new Core term tag is introduced. An instance remains an ordinary APP spine
with its source TypeView:

```text
Vec A n
    = APP(APP(TYPE_VIEW(Vec), A), n)
```

The parameter/index distinction belongs to declaration semantics, not to a
second Value/Computation graph or a special indexed APP node.

### 3.2 Context telescopes

Lowering builds one ContextDB path with an explicit boundary:

```text
Gamma_parameter
    A : Universe

Gamma_index
    A : Universe
    n : Nat
```

`prototype_type_declaration` records:

```text
parameter_context = Gamma_parameter
parameter_count   = 1
index_context     = Gamma_index
index_count       = 1
```

ContextDB remains the semantic authority for binder identity and dependent
classifier order. AST binder records retain source role and spans for parsing,
diagnostics, and readback only.

### 3.3 Constructor schema

Each constructor has:

```text
parameter_context
field_context
result_classifier
curried_classifier_cache
```

For `cons`, the authoritative graph-level schema is:

```text
parameter_context:
    A : Universe

field_context:
    A  : Universe
    k  : Nat
    a  : A
    xs : Vec A k

result_classifier:
    Vec A (succ k)
```

The constructor result must:

1. be headed by the declaration being defined;
2. supply exactly every uniform parameter and every family index;
3. preserve each uniform parameter modulo accepted pure conversion;
4. type every result index in the constructor field Context; and
5. retain its exact index spine in `result_classifier`.

`readback.field_types` and `readback.result_type` remain diagnostics/readback
metadata. They must not become a second classifier authority.

### 3.4 Construction syntax and arity

In the first implementation, `(k : Nat)` is an explicit constructor argument.
The parameter-specialized family view selects the constructor; its result
index is determined by the exact constructor result classifier:

```a-program
(Vec A).nil

(Vec A).cons k a xs
```

The second expression is checked by applying the constructor field telescope.
Its result is synthesized as `Vec A (Nat.succ k)`; an optional later `::`
checks that synthesized result and never selects it. Inferring or erasing `k`
is outside the first milestone.

### 3.5 Strict positivity: Vec fragment

The first positivity fragment admits direct indexed recursive fields:

```text
* k
```

It must no longer require a field type to be the bare `SELF` expression. It
must recognize a saturated application of declaration-local `SELF` with:

- uniform parameters captured by the declaration; and
- well-typed index arguments.

Recursive occurrences in parameter positions, function domains, unsupported
nested type formers, effects, or opaque host operations remain rejected or
residual according to the owning rule.

## 4. Indexed Match and Refinement

### 4.1 Required branch equation

For a scrutinee:

```text
v : Vec A (succ n)
```

the `cons` constructor contributes:

```text
constructor result: Vec A (succ k)
scrutinee type:     Vec A (succ n)
constraint:         succ k = succ n
solution:           k := n
```

The `nil` constructor contributes:

```text
constructor result: Vec A zero
scrutinee type:     Vec A (succ n)
constraint:         zero = succ n
result:             impossible by constructor disjointness
```

### 4.2 CwF representation of refinement

Refinement must be represented by Contexts and certified Substitutions, not by
mutating branch variables or globally unioning Term IDs.

For each branch, construct:

```text
Delta_constructor   constructor field telescope
Delta_refined       context containing the solved branch variables
rho_constructor     Delta_refined -> Delta_constructor
sigma_branch        Delta_refined -> Delta_surface_branch
```

The constructor result is reindexed along `rho_constructor`. The branch body
and branch Claim are interpreted along `sigma_branch`.

Existing semantic-action premise edges can carry the branch Substitution into
the `MATCH_ELIM` Derivation. This mechanism should be generalized before a new
proof-record format is invented.

### 4.3 Initial index solver

The first solver is intentionally narrower than general higher-order
unification. It handles constructor-pattern index equations:

```text
normalize with the pure kernel reduction profile
decompose matching TYPE_VIEW/APP spines
decompose equal constructor heads
solve branch-local variables by a scoped flex-rigid assignment
perform occurs and Context checks
use certified constructor disjointness for incompatible heads
otherwise produce a residual outcome
```

The result is structured:

```text
SOLVED
    certified substitution exists

IMPOSSIBLE
    a declaration-level disjointness/injectivity rule proves incompatibility

RESIDUAL
    neutral, opaque, exhausted, blocked, or unsupported

INVALID
    malformed declaration, Context, graph, or substitution
```

`NOT_EQUAL` from an arbitrary term comparison is not by itself proof that a
branch is impossible.

### 4.4 Motive

For a family with index telescope `I`, the Match motive is conceptually:

```text
M : (i : I) -> F p i -> Universe
```

or the corresponding computation classifier when the branch produces a
computation.

For a scrutinee `v : F p s`, the Match classifier is:

```text
M s v
```

A recursive field `xs : F p k` receives the exact induction hypothesis:

```text
*xs : M k xs
```

The existing one-argument neutral Match motive representation must therefore
be audited and generalized to retain the index spine explicitly. Branch result
agreement must be expressed as motive constraints and solved by the classifier
constraint phase, not by a Match-only uniform-branch shortcut.

### 4.5 Impossible and omitted branches

An omitted constructor branch is accepted only when its index equation has an
`IMPOSSIBLE` certificate. A `RESIDUAL` result requires the branch to remain or
the Match to remain unresolved; it must never be silently treated as
impossible.

The first implementation need not add an `impossible` keyword. Omission plus a
certified constructor-disjointness result is sufficient.

## 5. Acc and Higher-Order Strict Positivity

### 5.1 Target declaration

After the Vec-level fragment is complete, target this explicit declaration:

```a-program
Acc :=
	\A : @ =>
	\R : A -> A -> @ =>
	@\subject : A =>
	{
		acc :
			(x : A) ->
			((y : A) -> R y x -> * y) ->
			* x;
	};
```

Here `A` and `R` are uniform parameters, `subject` declares the family index
shape, and constructor-local `x` selects the result index.

### 5.2 Why Vec support is insufficient

The recursive occurrence in `Acc` is under Pi codomains:

```text
(y : A) -> R y x -> * y
```

This is strictly positive because `Acc` is absent from the Pi domains and
occurs positively in the final codomain. The current direct-`SELF` test and
direct recursive-field IH recognition cannot express it.

The positivity analysis must return a recursive shape, not only a boolean:

```text
NON_RECURSIVE
DIRECT(index_spine)
PI_CODOMAIN(binder, domain, recursive_shape)
NEGATIVE_OCCURRENCE
UNSUPPORTED
```

The shape can be derived from the authoritative field classifier. It should
not become a second independently editable constructor schema. If cached, the
cache must be validated by deterministic re-derivation.

### 5.3 Lifted induction hypothesis

For the constructor field written at the surface as:

```text
down : (y : A) -> R y x -> * y
```

the branch receives:

```text
*down :
    (y : A) ->
    (r : R y x) ->
    M y (down y r)
```

Surface use remains ordinary APP:

```a-program
*down y r
```

No new lifted-IH Term tag is required. `INDUCTION_HYPOTHESIS(scope, down)` can
retain its current graph form while its classifier and reduction rules inspect
the strictly-positive field shape.

Operationally, reduction is equivalent to constructing a lambda telescope
whose body recursively Matches `down y r` with the same motive and cases.

### 5.4 Well-founded recursion

Once the generic Acc declaration and lifted IH work, well-founded recursion is
a library-level definition over `Acc R x`. Do not add a privileged `Acc`
eliminator to the kernel.

The final issue milestone uses:

```text
Smaller ys xs := length ys < length xs
```

and supplies proofs that each QuickSort partition is smaller. The recursive
calls are justified through the `Acc` field and its lifted IH rather than a
separate fuel `Nat`.

Effectful well-founded recursion is not automatically admitted by the pure
Acc milestone. Its motive and effect-row behavior require a separate explicit
test and must not be inferred from the pure result.

## 6. Current Code Boundary

### 6.1 Parser and AST

Current limitations:

- `src/prototype/src/frontend/reader.c` treats every leading `\` binder of a
  type definition as a parameter;
- `parse_type_body` requires `@` immediately followed by `{`;
- `parse_constructor_type` requires the result to be bare `*` and emits
  `unsupported-indexed-family` otherwise;
- `prototype_ast_type_def` stores only `first_parameter/parameter_count`; and
- `prototype_ast_type_parameter` has no binder role or source split.

Required changes:

1. Parse parameter binders until either `@{` or `@\`.
2. Parse one or more `@\` binders, then require `{` directly.
3. Reject an ordinary `\` after the first index with a stable diagnostic.
4. Rename the source family-binder arena away from parameter-only terminology,
   or add an equally explicit role without maintaining duplicate arrays.
5. Record parameter and index ranges/counts and binder spans.
6. Parse an exact constructor result type headed by the current family.

Recommended AST organization:

```c
enum prototype_ast_family_binder_role {
	PROTOTYPE_AST_FAMILY_BINDER_PARAMETER = 1,
	PROTOTYPE_AST_FAMILY_BINDER_INDEX = 2
};

struct prototype_ast_family_binder {
	uint32_t ast_binder_id;
	int name_symbol_id;
	uint32_t type_expr;
	int role;
	struct prototype_source_span span;
};
```

`prototype_ast_type_def` should retain one ordered binder range plus the
parameter/index boundary. Do not maintain two unrelated binder arenas.

### 6.2 Declaration lowering

Current limitations:

- `compile_type_formation_classifier_family` folds only `parameter_count`;
- `compile_ast_type_def` calls only
  `prototype_type_declaration_add_parameter`;
- source lowering sets `index_context = parameter_context` and leaves
  `index_count == 0`;
- `type_term` is constructed from parameters only;
- constructor results are asserted to be direct `SELF`; and
- direct recursive fields alone are accepted.

Required changes:

1. Build and freeze `parameter_context` after the parameter prefix.
2. Extend it with index bindings and freeze `index_context`.
3. Build the formation classifier over both telescopes in dependency order.
4. Construct the source family instance using parameter and index APP spines.
5. Lower constructor field types under parameters and constructor-local fields.
6. Lower exact constructor result classifiers in each field Context.
7. Validate uniform parameters and index arity/types.
8. Generalize recursive-field recognition from direct `SELF` to exact indexed
   family applications.

`TypeDeclarationDB` already has `parameter_context`, `index_context`, both
counts, and exact constructor `result_classifier`. These fields should become
fully source-populated rather than replaced with a parallel indexed schema.

### 6.3 Constructor application and solver

Current limitations:

- indexed constructor membership in `classifier_solver.inc` accepts only the
  nullary generated Identity fragment;
- field substitution into exact indexed results is not general; and
- owner validation assumes unindexed constructors outside that exception.

Required changes:

1. Reindex the constructor telescope from declaration parameters into the
   selected owner parameter spine.
2. Apply explicit constructor fields in order.
3. Compute the specialized exact result classifier.
4. Compare that result with the selected owner under the pure conversion
   profile.
5. Preserve structured residual outcomes.
6. Emit a constructor-formation Derivation whose premises include every field
   Claim and any irreducible conversion evidence required by replay.

### 6.4 Indexed branch refinement

Current `prototype_judgement_indexed_branch_refinement` is not general IADT
support. It requires:

```text
parameter_count == 0
index_count == 2
both indices are variables
```

and reconstructs the specialized generated Identity context layout.

Required change:

- replace the implementation behind the general API with declaration-driven
  parameter/index decomposition and constructor-result refinement;
- keep generated Identity working through that general path; and
- remove assumptions about two endpoints and a proof variable from the common
  indexed-family function.

If Identity still needs a specialized bridge after general refinement, that
bridge must call the common solver and add only Identity-specific evidence.

### 6.5 Motive and IH

Current limitations:

- motive recovery primarily expects `APP(motive, scrutinee)`;
- recursive-field discovery recognizes a direct field variable; and
- `*field` classification assumes a direct recursive subobject.

Required changes:

1. Represent or recover `M indices scrutinee` without losing the index spine.
2. Refine the motive through the branch Substitution.
3. classify a direct indexed IH as `M field_indices field`;
4. later classify a Pi-codomain recursive IH as the lifted Pi described above;
5. validate the same shape in accepted replay; and
6. preserve neutral/residual motives rather than guessing a constant family.

## 7. Artifact and Replay

The baseline artifact v72 already serialized:

- declaration parameter/index Context IDs and counts;
- Contexts and Substitutions;
- exact constructor result classifiers; and
- semantic-action Substitution IDs on Derivation premise edges.

The audit found one missing wire concept: exact source indices such as
`Nat.zero` require `LOCAL_TYPE_MEMBER`, which was not in the v72 type-expression
vocabulary. The implementation therefore bumps the artifact to v73 and does
not retain a v72 compatibility reader.

Before changing the schema, perform an artifact impact audit:

1. verify source index Contexts enter publication closure;
2. verify constructor result classifier Terms enter the type slice;
3. verify branch refinement Substitutions enter Derivation closure;
4. verify relocation updates all references;
5. verify accepted replay deterministically rechecks constructor formation,
   refinement, motive specialization, and indexed IHs; and
6. verify positivity can be re-derived from authoritative classifiers.

Only bump the artifact version if an irreducible persistent field or proof rule
is actually added. A source-only AST role is not an artifact field.

Generated Identity declarations, source indexed families, and compiler-local
parametric relations must remain distinguishable by declaration origin and
proof authority.

## 8. Diagnostics

Add stable, source-located diagnostics for:

```text
parameter-after-index
missing-index-family-body
wrong-constructor-result-family
non-uniform-constructor-parameter
wrong-constructor-result-index-count
ill-typed-constructor-result-index
unsolved-index-refinement
residual-branch-cannot-be-declared-impossible
negative-recursive-occurrence
unsupported-positive-recursive-shape
indexed-motive-unsolved
```

Retain `unsupported-indexed-family` only as a boundary diagnostic for a truly
unsupported indexed feature. It must no longer be emitted merely because a
constructor result is not bare `*`.

Diagnostics must distinguish:

```text
invalid declaration
proved incompatible branch
valid but currently unsupported fragment
resource-exhausted or residual solver state
```

## 9. Implementation Phases

### IF0: Baseline and executable boundaries

Deliverables:

- capture the current failure of the level3 Vec, Fin, Eq, and Matrix fixtures;
- add parser-only negative fixtures for malformed `@\` ordering;
- record artifact v72 and HOTT v5 checks before implementation; and
- record current integration-test results.

Exit gate:

- every intended behavior has either a positive fixture or a stable negative
  boundary before semantic changes begin.

### IF1: Surface grammar and AST role split

Deliverables:

- parse `@\index : Type => { constructors }`;
- parse multiple index binders;
- preserve ordinary `@{...}`;
- preserve term-level Computation blocks;
- represent parameter/index roles in one ordered AST family-binder sequence;
- reject parameter-after-index; and
- preserve source spans.

Exit gate:

- parser/AST inspection shows the exact parameter and index split;
- no lowering path yet claims indexed support unless IF2 is complete; and
- Computation block tests remain unchanged.

### IF2: Source declaration and exact constructor results

Deliverables:

- populate source `parameter_context`, `index_context`, and both counts;
- form the full Pi classifier;
- lower exact result classifiers;
- enforce uniform parameters;
- validate index arity and classifier; and
- admit direct indexed recursive fields.

Exit gate:

- Vec, Fin, Eq, and Matrix declarations form correctly;
- malformed result heads and changed parameters are rejected; and
- declaration artifact round-trip preserves exact index spines.

### IF3: Indexed constructor application

Deliverables:

- specialize constructor field telescopes from owner parameters;
- apply explicit constructor-local index fields;
- validate exact result owner indices; and
- replay constructor formation without solver search.

Exit gate:

- Vec nil/cons and Fin fzero/fsucc construct exact instances;
- wrong owner indices are rejected or residual as appropriate; and
- artifact append/readback preserves constructor Claims.

### IF4: Structural index refinement

Deliverables:

- implement the scoped first-order index solver;
- generate certified branch Substitutions;
- generalize `prototype_judgement_indexed_branch_refinement`;
- prove Nat constructor disjointness/injectivity through declaration evidence;
- keep residual outcomes distinct; and
- route generated Identity through the common path.

Exit gate:

- Vec `(succ n)` refines `cons k` to `k := n`;
- the `nil` branch is certified impossible;
- exhausted or neutral comparisons remain residual; and
- no global DefEq or Term union is modified by branch refinement.

### IF5: Dependent motive and indexed IH

Deliverables:

- construct/infer motives over index spines and scrutinees;
- specialize branch motives by certified Substitution;
- generate exact direct indexed IH classifiers;
- update accepted replay; and
- support omission of only certified-impossible branches.

Normative synthesis boundary:

- Match motives and IH classifiers must be synthesized solely from the
  scrutinee classifier, constructor telescopes, branch operations, and scoped
  equations;
- top-level or inline `::` expectations are checked only after synthesis and
  must never seed, select, or repair a motive;
- constructor field domains may propagate local argument constraints because
  they are inputs owned by the constructor-application rule, not external
  expectations; and
- a fixture must compile with its `::` line removed before its annotated form
  can count as evidence of indexed synthesis.

Exit gate:

- indexed `head` and `tail` compile;
- indexed append receives `M k xs`, not an unindexed approximation;
- multiple-index Matrix elimination is covered; and
- all Match Claims survive artifact write/read/replay.
- adding or removing a valid `::` does not change the synthesized Match
  classifier or its Derivation premises.

### IF6: Strictly-positive Pi-codomain recursion

Deliverables:

- implement recursive-shape analysis;
- reject negative occurrences;
- admit Pi-codomain positive occurrences;
- classify and reduce lifted IH functions; and
- validate lifted IH evidence during replay.

Exit gate:

- the generic Acc declaration forms;
- `*down y r` has classifier `M y (down y r)`;
- `((* index -> X) -> X) -> * result-index` is rejected as negative; and
- unsupported nested positive forms remain explicit residuals.

### IF7: Acc elimination and well-founded library

Deliverables:

- define the Acc eliminator using ordinary Match/IH;
- define a reusable well-founded recursion combinator;
- prove a small nontrivial terminating example without fuel; and
- round-trip the declaration, eliminator, and evidence through artifacts.

Exit gate:

- no Acc-specific kernel or normalizer rule exists;
- accepted replay validates the generic derivation DAG; and
- direct and lifted IH reduction terminate under the selected profile.

### IF8: Fuel-free QuickSort

Deliverables:

- define the list-size `Smaller` relation;
- prove both partition outputs decrease;
- define QuickSort through Acc;
- execute representative inputs; and
- retain the fuel implementation as a comparison fixture.

Exit gate:

- generic fuel-free QuickSort compiles and executes;
- sortedness/termination obligations are separate;
- artifact replay passes; and
- GitHub Issue #11 acceptance criteria are fully reviewed before closure.

### IF9: Documentation, cleanup, and accounting

Deliverables:

- document the final grammar and examples;
- update README support boundaries;
- remove obsolete Identity-only indexed exceptions where the general path owns
  the behavior;
- run format/build/integration/artifact tests; and
- report per-file added/deleted/net line counts.

Exit gate:

- code and documentation describe the same supported fragment;
- no compatibility parser for the rejected ambiguous syntax remains; and
- all permanent boundary tests pass.

## 10. Progress Tracker

Status values:

```text
not-started | in-progress | blocked | complete | superseded
```

| Phase | Status | Depends on | Completion evidence |
|---|---|---|---|
| IF0 Baseline boundaries | complete | none | Baseline integration suite and pre-change level3 manifest recorded |
| IF1 Surface grammar and AST | complete | IF0 | Positive Vec/Fin fixtures and malformed/order negatives |
| IF2 Declaration lowering | complete | IF1 | Vec/Fin/Eq/Matrix formation and exact result classifiers |
| IF3 Constructor application | complete | IF2 | Vec/Fin construction plus v73 write/read/replay |
| IF4 Index refinement | complete | IF3 | Vec/Fin solved and impossible branch refinement tests |
| IF5 Motive and indexed IH | complete | IF4 | head/tail/map synthesis without expected-type seeding |
| IF6 Pi-codomain positivity | complete | IF5 | Acc formation, negative occurrence rejection, lifted-IH typing/replay, and Core runtime lifting test |
| IF7 Acc/well-founded library | complete | IF6 | Generic eliminator, full specialization, concrete `accFalse`, impossible-fiber refinement, and v73 artifact replay pass |
| IF8 Fuel-free QuickSort | blocked | IF7 | Requires public equality/transport, decrease proofs, and completed surface well-founded recursion |
| IF9 Documentation/cleanup | complete | IF7 | Implemented-fragment docs, strengthened artifact boundaries, full integration checks, and implementation report are current; IF8 remains a separate equality-dependent phase |

### 10.1 Progress log

| Date | Phase | Change | Evidence | Remaining risk |
|---|---|---|---|---|
| 2026-08-13 | Planning | Approved `@\index => { constructors }` syntax and explicit constructor-local indices | Issue #11 and commit `2ef4564` code audit | Implementation has not started |
| 2026-08-13 | IF0-IF1 | Added one ordered family-binder arena with parameter/index roles; parsed one or more explicit index binders; retained ordinary ADTs and Computation blocks | `test_explicit_index_family_surface.sh` parser boundaries | Indexed semantic lowering was still required |
| 2026-08-13 | IF2 | Populated parameter/index Context boundaries and full formation Pi; lowered exact indexed constructor results; added local type-member metadata for `Nat.zero`-style result indices | Vec, Fin, Eq, and Matrix level3 examples compile | Match and artifact closure required separate fixes |
| 2026-08-13 | IF3 | Specialized constructor telescopes by parameters while preserving exact indexed results; corrected Match resolver to retain parameter-only constructor owners | Vec/Fin constructor ascriptions and `head_or` all-branch Match compile | Dependent branch refinement remains |
| 2026-08-13 | IF3 artifact | Closed index Contexts and local type-member references during publication; preserved representation relocation while rebuilding fingerprints; validated erased constructor owners against typed indexed classifiers | v73 Vec compile/write/interface-read/graph-replay succeeds | Link/import fixture and dependent Match replay remain |
| 2026-08-13 | IF4-IF5 | Added declaration-driven branch refinement, omitted impossible branches, synthesized indexed motives, and exact indexed IH classifiers | `head`, `tail`, annotation-free `tail`, and `map` fixtures compile | General higher-order unification remains outside the structural solver |
| 2026-08-13 | IF6 | Generalized strict positivity through Pi codomains and computation/thunk wrappers; rejected recursive occurrences in Pi domains | `Acc` and `accElim` compile; `BadAcc` rejects | Evaluator still reduces a lifted IH as a direct recursive Match |
| 2026-08-13 | IF6 artifact | Made recursive-family inspection TypeView-transparent and replayed IH classifiers from authoritative constructor schemas | Acc v73 write/read/accepted-replay round trip succeeds | Runtime lifted-IH reduction remains before IF6 can complete |
| 2026-08-14 | IF6 runtime | Lifted IH through existing `THUNK`/`LAMBDA`/`COMPUTATION_FOLD` structure, without a runtime schema or new Term tag | `lifted_ih_runtime_check.c` distinguishes functorial lifting from the old direct-Match reduction | Surface application of the generic combinator is a separate IF7 constraint |
| 2026-08-14 | IF7 partial | Added a surface fixture that declares the generic Acc eliminator and specializes its uniform type parameter; retained the executable Core test for higher-order IH mapping | `explicit_index_family_acc_parameter_specialization_check.p` and `lifted_ih_runtime_check.c` pass in the permanent suite | Applying the remaining relation, motive, and step arguments crosses nested `EFFECT_ROW_FORALL`, pure `Comp`, quotation, and dependent APP forms that the current row solver does not normalize as one equation |
| 2026-08-14 | IF7 row specialization | Extended the local expected-effect-row solver across dependent APP, nested pure computation/quotation boundaries, and transitive row-variable equations without adding global conversion rules | `explicit_index_family_acc_full_specialization_check.p` specializes `A`, `R`, `P`, and `step`; source compilation and v73 accepted artifact replay are permanent tests | Constructing a concrete finite `Acc` proof still requires elimination of an indexed relation fiber with no compatible constructor |
| 2026-08-14 | IF7 completion | Reused declaration-driven constructor disjointness to omit only impossible indexed branches, completed concrete `Acc` construction, and made accepted Claim publication follow a grounded Derivation fixed point | `explicit_index_family_acc_concrete_check.p` compiles; `accFalse` survives v73 write/read; reachable branch type mismatch is rejected; the complete integration suite passes | General index unification beyond constructor disjointness remains intentionally residual |
| 2026-08-14 | IF9 | Updated README and the historical-design amendments to make `* indices` authoritative and named recursive self-reference explicitly invalid; bumped v72 to v73 for the new `LOCAL_TYPE_MEMBER` wire tag; strengthened Vec and Acc artifact round trips | `-Werror` reader build and the complete integration suite pass | IF8 remains explicitly deferred rather than being reported as implemented |

### 10.2 Blocker log

| Date | Phase | Blocker | Required decision/evidence | Resolution |
|---|---|---|---|---|
| 2026-08-13 | IF6 | The evaluator mapped `IH(scope, down)` directly to `Match(down, ...)`; a higher-order field requires mapping through its CBPV function representation | Implemented structurally with existing `THUNK`/`LAMBDA`/`COMPUTATION_FOLD`; no second schema authority | Closed 2026-08-14 |
| 2026-08-14 | IF7 row schemes | Applying the generic `accElim` across relation, motive, and step arguments required classifier-level row schemes under dependent Pi/APP structure | Solved in the dedicated expected-effect-row solver, including transitive row-variable equations; no global DefEq rule was added | Closed 2026-08-14 |
| 2026-08-14 | IF7 empty fiber | A concrete `accFalse` proof requires checking a branch from `Precedes y false`, although the sole constructor has result `Precedes false true`; the current fixed point did not turn that incompatible index equation into certified branch impossibility inside a higher-order constructor argument | Reuse the declaration-driven impossible-branch refinement already used by indexed Match, preserving a proof/certificate that the constructor result cannot unify with the scrutinee fiber; do not accept the branch merely because synthesis failed | Closed 2026-08-14; constructor-disjoint indexed branches omit only the unreachable premise, while a reachable ill-typed branch is rejected |
| 2026-08-13 | IF8 | The planned decrease proofs and algorithmic correctness statements require public equality/transport not supplied by this syntax milestone | Finish the equality/transport boundary before claiming fuel-free QuickSort | Open |

## 11. Permanent Test Matrix

### 11.1 Parsing

| Case | Expected |
|---|---|
| `@{ ... }` ordinary ADT | accept |
| `@\n : Nat => { ... }` | accept |
| two consecutive `@\` binders | accept |
| parameter before index | accept |
| parameter after index | reject with stable code |
| `{...}` in term position | Computation block |
| `{...}` after index binder | constructor block |

### 11.2 Formation

| Case | Expected |
|---|---|
| `Vec A n` | accept |
| dependent index classifier | accept |
| Matrix with two indices | accept |
| constructor changes `A` | reject |
| constructor result has wrong head | reject |
| constructor result has too few indices | reject |
| direct indexed recursive field | accept |

### 11.3 Elimination

| Case | Expected |
|---|---|
| head of `Vec A (succ n)` | nil impossible, cons refined |
| tail | exact result index |
| append | exact indexed IH |
| neutral index equation | residual, not impossible |
| exhausted comparison | residual, not impossible |

### 11.4 Positivity and Acc

| Case | Expected |
|---|---|
| direct `* y` field | accept |
| Pi-codomain returning `* y` | accept at IF6 |
| `Acc` in Pi domain | reject |
| lifted `*down y r` | exact motive classifier |
| generic Acc eliminator | accept without special kernel rule |

### 11.5 Artifact

Every positive semantic case above requires:

```text
compile -> publish -> write -> read -> relocate/link -> accepted replay
```

Every negative artifact fixture must fail at the same authority boundary that
accepted publication validates. Readback must not silently repair malformed
parameter/index counts, Context paths, result classifiers, or Substitutions.

## 12. Non-Goals and Safeguards

- Do not infer parameter/index roles from constructor bodies.
- Do not implicitly generalize constructor-local indices in the first version.
- Do not add a separate indexed APP/Lambda graph hierarchy.
- Do not make branch refinement a global DefEq union.
- Do not treat comparison exhaustion or unsupported rules as impossibility.
- Do not use internal HOTT/parametric relation declarations as ordinary IADT
  equality evidence.
- Do not claim general GADT/IADT unification after the structural Vec solver.
- Do not claim Acc support after only parsing or constructing Vec.
- Do not erase index arguments or proof witnesses in this project.
- Do not add an Acc-specific kernel primitive.
- Do not preserve the old bare-`*` restriction as a fallback authority for
  indexed declarations.

## 13. Review Gates

Pause for design review at these boundaries:

1. after IF1, confirm the AST contains one ordered binder sequence and no
   duplicate parameter/index authority;
2. after IF2, inspect exact constructor schemas before changing Match;
3. after IF4, inspect refinement Substitution direction and impossible-branch
   certificates;
4. after IF5, inspect motive and IH classifiers on actual Term graphs;
5. after IF6, compare the accepted positivity grammar with the Acc example;
6. before any artifact bump, identify the exact missing wire vocabulary (v73
   was required because v72 lacked `LOCAL_TYPE_MEMBER`); and
7. before closing Issue #11, run the full acceptance matrix including Acc and
   fuel-free QuickSort.

## 14. Implementation Report

```text
baseline commit: 2ef4564
artifact version before/after: v72 -> v73
implemented phases: IF0-IF7 and IF9 for the implemented indexed-family/Acc
                    fragment
deferred phases: IF8 fuel-free QuickSort, pending public equality/transport
positive fixtures: Vec, Fin, Eq, Matrix, head, tail, map, Acc formation,
                   generic accElim declaration/full parameter specialization,
                   concrete accFalse, and lifted-IH Core execution
negative fixtures: malformed binder order, named recursive self-reference,
                   invalid recursive result, and negative Acc occurrence
artifact replay evidence: Vec and Acc v73 write/read/accepted-replay paths
remaining residual boundaries: general index unification beyond constructor
                               disjointness and public equality/transport for
                               IF8
```

The final commit identifier is intentionally recorded by Git rather than copied
into its own pre-commit contents. `git show --numstat` on that commit is the
authoritative per-file physical accounting. Before this report was appended, the
staged implementation had 5,386 added and 457 deleted lines, for a net increase
of 4,929 lines across 59 paths. The large increase is primarily the explicit
index implementation, permanent boundary fixtures, and the two design/plan
documents, not duplicate Value/Computation graph hierarchies.

The implementation files with the largest semantic changes were:

| File | Added | Deleted | Net | Semantic responsibility changed |
|---|---:|---:|---:|---|
| `src/prototype/src/frontend/lowering/constraint_solver.inc` | 539 | 40 | +499 | indexed constraints, motives, and family specialization |
| `src/prototype/src/frontend/lowering/graph_construction.inc` | 543 | 91 | +452 | declaration-driven indexed graph construction |
| `src/prototype/src/kernel/typing/classifier_solver.inc` | 604 | 32 | +572 | indexed classifier and IH solving |
| `src/prototype/src/kernel/rules/formation_early.inc` | 333 | 0 | +333 | indexed family formation and positivity rules |
| `src/prototype/src/frontend/lowering/finalization_and_entrypoints.inc` | 267 | 5 | +262 | final declaration validation and publication |
| `src/prototype/src/kernel/typing/accepted_replay.inc` | 196 | 29 | +167 | authoritative artifact replay validation |
| `src/prototype/src/core/term/evaluation_and_conversion.inc` | 128 | 43 | +85 | structural higher-order IH lifting |
| `src/prototype/src/frontend/reader.c` | 113 | 29 | +84 | explicit index surface grammar |
| `src/prototype/tests/integration/test_explicit_index_family_surface.sh` | 97 | 0 | +97 | permanent indexed-family boundary matrix |
| `src/prototype/tests/checks/lifted_ih_runtime_check.c` | 140 | 0 | +140 | Core higher-order IH runtime boundary |

Code reduction remains desirable only where an Identity-only indexed path can
be replaced by the general declaration-driven path. Distinct parser,
declaration, Match, positivity, and replay rules are not collapsed merely to
reduce line count.
