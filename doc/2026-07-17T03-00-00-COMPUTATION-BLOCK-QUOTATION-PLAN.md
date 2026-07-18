# Computation Block and Quotation Migration Plan

Date: 2026-07-17

## Status

Implemented in `src/prototype/` on 2026-07-17. The prototype build, CBPV
boundary tests, surface tests, dependent-Pi tests, shared-core occurrence
tests, and artifact flow tests pass.

The accepted examples are outside the AI write boundary and have not been
migrated. Examples 01-04 and 06 compile unchanged. Examples 05, 07, and 09
now fail intentionally at an implicit computation-to-value boundary and need
the source migrations recorded below.

This plan introduces two surface forms:

```text
{ ... }   explicit computation block
&M        quote a computation as a value
```

The purpose is to keep ordinary A Program source close to Lambda/App/Match
syntax while making the exceptional computation-to-value boundary explicit.
The raw CBPV TermDB nodes remain `RETURN`, `THUNK`, `FORCE`, `APP`, `BIND`,
`OPERATION_REQUEST`, and `HANDLE`.

## Design Decision

The surface language follows these polarity rules:

```text
value in computation position       -> implicit RETURN
computation in value position       -> error
explicit &computation               -> THUNK(computation)
block binding of a computation      -> BIND(computation, continuation)
```

There is no general implicit computation-to-value coercion. In particular,
the compiler must stop treating an arbitrary computation occurrence as either
"execute it and use its result" or "suspend it and use the computation as
data" according to whichever consumer happens to be compiling it.

The two meanings have separate source forms:

```text
{
	x := M;
	N
}

&M
```

The first executes `M : Comp(E, A)` once and binds its result `x : A`. The
second does not execute `M`; it constructs a value of type
`Thunk(Comp(E, A))`.

## Surface Semantics

### Computation block

A block is a computation expression:

```text
{
	x := M;
	y := f x;
	y
}
```

If:

```text
M   : Comp(E1, A)
f x : Comp(E2, B)
```

then the block lowers to:

```text
BIND(M, \x =>
	BIND(APP(FORCE(f), x), \y =>
		RETURN(y)))
```

and has classifier:

```text
Comp(E1 union E2, B)
```

The exact `FORCE(f)` shown above depends on whether `f` is a raw Pi
computation or a thunked function value. The existing application lowering
already distinguishes those cases. A block does not introduce a second APP
rule.

The initial block grammar is:

```text
block        := "{" block_item* terminal_term "}"
block_item   := identifier (":" type_expr)? ":=" term ";"
terminal_term := term
```

The final term is mandatory. An empty block and a block ending only in `;`
are errors in the first implementation. This avoids inventing an implicit
Unit value before the source-level Unit policy is settled.

The terminal term follows these rules:

* A terminal value `v : B` lowers to `RETURN(v)`.
* A terminal computation `M : Comp(E, B)` is used directly as the tail
  computation; it is not wrapped in another `RETURN`.
* A raw Pi computation may be the tail of a Lambda or Match branch according
  to the existing nested-function rule. The block must not force it merely
  because it is the final syntax node.

For a local assignment:

* If the right-hand side is a value, the initial implementation records a
  local value alias and emits no `BIND`.
* If the right-hand side is a returning computation, it emits
  `BIND(rhs, \name => rest)`.
* A raw function computation is a named function computation, not a returning
  computation. It must not be executed through `BIND` merely because it is
  assigned locally.

The optional annotation supplies the continuation Lambda domain directly:

```text
{
	n : Nat := *rest;
	Nat.succ n
}
```

It is not required for the current uniform IH case, but remains useful as an
explicit constraint seed when a future dependent motive leaves the result
classifier underdetermined.

This distinction is already represented by `compile_ref.polarity` and
`compile_ref.computation_kind`. The block lowering must consume those facts
rather than inspect only the shared TermDB root.

### Quotation with `&`

`&M` is computation quotation, not an address operation:

```text
M  : C                         C is a computation classifier
-----------------------------------------------------------
&M : Thunk(C)
```

For an ordinary returning computation:

```text
M  : Comp(E, A)
&M : Thunk(Comp(E, A))
```

For a raw function computation:

```text
f  : Pi(A, Comp(E, B))
&f : Thunk(Pi(A, Comp(E, B)))
```

`&M` does not memoize the result of `M`. Forcing the quoted value twice may
execute `M` twice. Execute once and share the result by binding it once in a
block.

Quotation of an existing value is an error. It is not an identity operation
and it must not silently create nested or redundant value wrappers.

The proposed precedence is unary and atom-tight:

```text
&f x       == (&f) x
&(f x)     quotes the complete application computation
&{ ... }   quotes a computation block
&(\x : A => body)
            quotes an anonymous raw function computation
```

The reader therefore needs a distinct ampersand token. `&` is not currently a
valid identifier character or token, and it does not conflict with the
existing `@{ ... }` type-formation syntax. `*` remains reserved for induction
hypothesis syntax and must not be reused as a force/dereference operator.

### Top-level computation names

A top-level assignment may continue to name a computation:

```text
module := {
	x := operation;
	x
};

module : Comp(E, A)
```

This does not make `module` a value. If `f` requires an `A`, then:

```text
f module
```

is rejected. The direct form is:

```text
{
	x := module;
	f x
}
```

If a consumer requires the computation itself as data, the source writes:

```text
consumer &module
```

### Lambda and Match bodies

Lambda and Match case bodies are computation contexts:

```text
\x : A => module

b @true  => module1
  @false => module2
```

A value body is lifted with `RETURN`. A computation body is kept as a
computation. Consequently, most ordinary source programs need neither an
explicit `return` nor an explicit computation block around a single body.

Blocks are needed when a body must name and reuse the result of an intermediate
computation:

```text
\x : A => {
	y := effectful x;
	pure_use y
}
```

## Function Values and Higher-order Application

The current implementation already has two relevant representations:

* A top-level Lambda definition is compiled as a raw Pi computation.
* A surface arrow used as an ordinary value type is compiled by
  `compile_ast_type_expr_term_with_self` to a thunk type containing a Pi whose
  codomain is a computation.

Conceptually:

```text
A -> B
  == Thunk(Pi(A, Comp(row, B)))
```

The row may be empty, inferred, or later written explicitly by an
effect-aware arrow syntax. The exact surface effect-row notation is outside
this plan.

Therefore a higher-order call becomes explicit at the quotation boundary:

```text
sf := \x : A => body;
higher_func &sf;
```

The parameter of `higher_func` has the surface arrow value type. Application
of that parameter can remain ordinary source syntax:

```text
higher_func := \f : A -> B => f value;
```

Because `f` has a thunked Pi classifier, application lowering can insert
`FORCE(f)` unambiguously before `APP`. This is not the prohibited general
computation-to-value coercion: the value classifier itself proves that the
callee is a suspended function computation.

An occurrence of a raw named function in direct callee position may remain a
raw `APP` without creating `THUNK` followed immediately by `FORCE`.

## Remaining Explicit Force Boundary

`&` cannot eliminate every need to open a suspended computation. Automatic
force is unambiguous for a value used as an application callee when its
classifier is `Thunk(Pi(...))`. It is not generally unambiguous for an
arbitrary `Thunk(Comp(E, A))` value.

The first implementation therefore retains a non-keyword escape hatch:

```text
#.force delayed
```

This is an elaborator-recognized CBPV boundary, not a host operation and not
an effect request. A later computation-block design may add a dedicated
binding form for forcing a thunk, but ordinary `x := delayed;` must not
silently choose between aliasing the thunk value and executing its suspended
computation.

Function calls are the only planned implicit force site in this phase.

## Ordering and Dependency Graph

The source block gives a lexical order. The initial lowering must preserve it
for all computation bindings, even when two bindings have no value dependency:

```text
{
	x := print_a;
	y := print_b;
	y
}
```

Reordering these computations can change observable behavior. A data
dependency DAG alone is therefore not the operational semantics of a block.

The OperationGraph may still record data dependencies and share pure value
nodes. A later optimizer may topologically reorder or parallelize nodes only
after an effect/commutativity rule proves the transformation valid. The parser
and initial lowering must not make that optimization decision.

## Implemented Migration

The prototype now has one strict surface policy. The old automatic-coercion
compiler option and CLI flags were removed. The resulting behavior is:

| Current behavior | Target behavior |
| --- | --- |
| raw function used as an argument is automatically thunked | require `&f` |
| arbitrary computation argument is automatically executed through BIND | reject; bind it in `{ ... }` |
| explicit source `thunk M` | replace with `&M` |
| explicit source `return v` | omit; computation context inserts `RETURN` |
| explicit source `force v` | retain only as provisional `#.force v`, except function calls |
| implicit BIND in any value consumer | restrict BIND introduction to block sequencing and recognized core forms |

The old expression forms `return v`, `thunk M`, and `force v` are no longer
reader productions. `return` remains only as the structural return clause in
the provisional handler syntax. `perform` and `handle` remain pending the
separate effect-surface migration.

Application lowering accepts a computation callee, including a curried
intermediate APP, but requires its argument to be a value. A thunked function
value is forced only in callee position. This preserves `(f x) y` while
rejecting `f (g x)` unless the result of `g x` is bound in a block.

## AST and OperationGraph Design

The implementation uses AST tags with source occurrence identity:

```text
PROTOTYPE_AST_QUOTE
PROTOTYPE_AST_COMPUTATION_BLOCK
```

A block is represented recursively as `BLOCK_BIND(value, rest)`, wrapped by
one `COMPUTATION_BLOCK` node. Each bind stores:

```text
local AST binder identity
source symbol id
optional binder type expression id
right-hand-side AST id
rest AST id
source span
```

The recursion ends at the terminal AST node. Block-local names use normal AST
binder identities and are added to reader scope only after their right-hand
side has been parsed. This prevents accidental recursive local definitions in
the initial design and makes reverse structural lowering unnecessary.

Quotation lowers to the existing operation/core pair:

```text
PROTOTYPE_OPERATION_THUNK
PROTOTYPE_TERM_THUNK
```

No new TermDB quotation tag is required.

The block itself does not need a persistent TermDB tag. It is source
sequencing syntax that lowers to existing `BIND`, `LAMBDA`, and `RETURN`
nodes. OperationGraph must nevertheless retain enough source provenance for
the generated nodes to point back to the block entry and terminal occurrence.

Do not attach block binder identity to `BIND`. The current binderless BIND
invariant remains:

```text
BIND(computation, LAMBDA(x, continuation))
```

## Lowering Algorithm

Lower a block from the last entry toward the first while preserving source
order.

1. Lower the terminal term in computation context.
2. For a value terminal, emit `RETURN(value)`.
3. For each preceding entry in reverse order, lower its right-hand side while
   retaining its occurrence polarity and computation kind.
4. For a returning computation `M : Comp(E, A)`, create a Lambda whose binder
   is the entry's AST binder and whose body is the already-lowered remainder;
   then emit `BIND(M, lambda)`.
5. For a value right-hand side, extend the local occurrence environment with
   a value alias. Do not synthesize a runtime BIND.
6. For a raw function computation, extend the local occurrence environment
   with the raw function occurrence. Do not classify it as a returning
   computation.

Step 5 cannot be implemented as textual substitution. It must preserve the
typed OperationGraph occurrence even when its core TermDB node is structurally
shared with another annotated occurrence.

The existing fixed-point solver supplies the result classifier of a returning
computation and the Lambda binder assumption. Block lowering may create the
structural BIND graph before those classifiers are solved. It must generate
constraints rather than demand a final classifier during parsing.

## Reader Work

1. Add `TOKEN_AMPERSAND` to `reader.c` and lex a standalone `&`.
2. Add quote parsing at unary/atom precedence.
3. Admit `TOKEN_LBRACE` as the start of a term atom or primary computation.
4. Parse ordered local assignments and one mandatory terminal term.
5. Allocate an AST binder for each local assignment and extend scope only for
   subsequent entries and the terminal term.
6. Keep `@{ ... }` parsing exclusively in type-formation parsing; bare
   `{ ... }` is a computation block.
7. Produce precise errors for a missing terminal term, duplicate local name
   in one active scope, and unterminated block.

Match case parsing must parse a leading block directly rather than call the
general `parse_term`. Otherwise the following `@case` is consumed as a nested
Match whose scrutinee is the block. This bug was found and fixed during the
migration.

The parser must not decide whether a block assignment is a value alias, raw
function definition, or computation bind. That decision belongs to
polarity-aware lowering and constraint solving.

## Type and Effect Constraints

Quotation has a structural classifier rule:

```text
M : C
-----------------
&M : ThunkType(C)
```

A returning block binding generates the existing BIND equations:

```text
M : Comp(E, A)
K : Pi(A, Comp(F, B))
---------------------------------
BIND(M, K) : Comp(E union F, B)
```

Block lowering must not use an empty effect row to mean "unknown". Unknown
rows remain row variables; a proven pure computation uses the canonical empty
row. This plan depends on the effect-row constraint migration documented in
`2026-07-17T01-33-29-EFFECT-ROW-CONSTRAINT-MIGRATION.md`.

Dependent BIND results and residual runtime verification retain the policy in
`2026-07-16T01-00-00-RESIDUAL-DEPENDENT-CBPV-MIGRATION.md`. A block is surface
sequencing syntax; it does not weaken or bypass dependent-BIND obligations.

## Artifact Impact

No new TermDB tag is required, so a compiled block and `&M` use existing core
serialization for BIND/LAMBDA/RETURN/THUNK.

Artifact interface behavior still changes because exported source definitions
may now retain raw computation polarity where the old lowering inserted a
THUNK automatically. Before declaring the artifact format unchanged, tests
must verify that:

* exported raw function computations preserve their raw Pi classifier;
* exported quoted function values preserve `Thunk(Pi(...))`;
* imports do not reintroduce automatic thunking based only on a shared core
  term;
* OperationGraph/debug provenance for block-generated BINDs is stable;
* artifact append/readback normalization agrees with direct compilation.

If source AST or OperationGraph data is serialized in the active artifact
version, bump the version and reject the old layout. Do not add a compatibility
reader.

## Implementation Phases

### Phase 1: Add syntax without retracting coercions

1. Add `&M` AST/reader support and lower it to existing THUNK nodes.
2. Add block AST storage and reader scope handling.
3. Lower computation block bindings to binderless BIND plus Lambda.
4. Add focused parser, scope, typing, effect-union, and normalization tests.

During this phase the old contextual forms may remain only to keep debugging
the new syntax isolated. They are not a compatibility commitment.

### Phase 2: Make computation-to-value strict

1. Remove automatic raw-function thunking from Lambda and referenced-name
   value lowering.
2. Remove the fallback in `compile_ast_value_then` that executes arbitrary
   computation arguments for a value consumer.
3. Reject `f module` when `f` requires the returned value type rather than a
   thunk type.
4. Preserve automatic force only for application callees proven to have a
   thunked Pi classifier.
5. Require `&f` for higher-order function arguments and `&module` for stored
   computations.

### Phase 3: Retract old surface words

1. Remove reader productions for `return` and `thunk`.
2. Replace general `force` syntax with the provisional `#.force` intrinsic.
3. Move operation request construction away from explicit `perform` as
   described in the dependent CBPV effect migration plan, then remove
   `perform`.
4. Replace special handler syntax with the selected application-shaped or
   block-integrated handler form before removing `handle`.
5. Remove the automatic-coercion compiler option and its dual-semantics tests.

`perform` and `handle` retraction is not a prerequisite for the block and `&`
semantics. Do not couple all syntax removal into one unreviewable patch.

### Phase 4: Artifact and regression closure

1. Regenerate artifact fixtures if the active format changes.
2. Run direct compile, artifact write/readback, append, link, and imported
   higher-order function tests.
3. Verify examples 01-07 and 09.
4. Add permanent tests for quoted functions, quoted effectful computations,
   repeated force, execute-once block sharing, and illegal computation
   arguments.

## Required Tests

### Positive

```text
module := { value };
main := { x := module; x };
```

The block returns the result value and has the unioned computation classifier.

```text
id := \x : A => x;
apply := \f : A -> A => f value;
main := apply &id;
```

The quote creates one THUNK and application of `f` creates one FORCE before
APP.

```text
delayed := &module;
```

The definition is a value and compiling it does not execute or normalize the
effectful body through a host handler.

```text
main := {
	x := module;
	pair x x
};
```

`module` occurs once as the input to BIND; the result value occurs twice.

### Negative

```text
f module
```

Reject when `f` requires `A` and `module : Comp(E,A)`.

```text
&value
```

Reject quotation of an ordinary value.

```text
{
	x := module;
}
```

Reject a missing terminal term in the initial grammar.

```text
higher_func id
```

Reject when the parameter requires the first-class arrow value and `id` is a
raw named function computation. Require `higher_func &id`.

### Behavioral

* Forcing `&module` twice executes two computation instances.
* Binding `module` once and using its result twice executes one computation
  instance.
* Two effectful block entries preserve source order.
* Pure value aliases add no BIND node.
* A raw local Lambda definition adds no BIND node.
* A block terminal Match and a Match branch containing a block preserve the
  synthesized motive and effect-row constraints.
* Shared core identity does not merge quoted and raw OperationGraph
  occurrences.

## Implementation Record

The implementation completed Phases 1 and 2 and the `return`/`thunk`/`force`
parts of Phase 3. `perform` and `handle` deliberately remain separate work.
No TermDB or artifact-format tag was added: blocks lower to existing
`BIND`/`LAMBDA`/`RETURN`, and `&` lowers to existing `THUNK`.

The implementation exposed and corrected four issues beyond the initial
syntax work:

1. A curried APP result was marked with unknown computation kind and then
   rejected in callee position. Computation callees are now accepted without
   requiring the syntactic root to be a Lambda.
2. BIND input propagation originally matched shared core VAR nodes. It now
   uses AST binder occurrence identity when available, preventing unrelated
   typed binders with the same tagless core from receiving each other's
   classifiers.
3. A bare IH was provisionally marked as a function computation. A block
   assignment now treats an IH as a computation to execute, so `n := *rest`
   produces BIND rather than a raw-function alias.
4. A block at the start of a Match branch consumed the following `@case` as a
   nested Match suffix. The case-body parser now stops after the block.

Permanent tests cover typed and inferred block binders, value and raw-function
aliases, blocks in Match branches, IH block sequencing, explicit quotation,
illegal computation arguments, illegal value quotation, a missing terminal,
curried application, repeated force, execute-once binding, and artifact
write/read/link/normalization flows.

Accepted source migration still required:

```text
/* examples/05_bool_to_nat.p */
main := {
	b := negate Bool.false;
	toNat b
};

/* examples/07_add.p */
@succ k => (\m : Nat => {
	result := *k m;
	Nat.succ result
})

/* examples/09_list_induction.p */
@cons x xs => {
	n := *xs;
	Nat.succ n
}
```

These are source migrations, not requests to restore the removed implicit
computation-to-value coercion.

## Non-goals

This migration does not:

* make a thunk an address or mutable reference;
* memoize quoted computations;
* add parallel block evaluation;
* infer that arbitrary effects commute;
* add a new TermDB execution environment;
* turn proof equality into kernel conversion;
* settle the final effect-aware arrow spelling;
* settle the final user-defined handler syntax; or
* add Unit solely to permit empty blocks.

## Completion Criteria

The migration is complete when:

1. Every implicit value-to-computation boundary is a `RETURN` justified by a
   computation context.
2. Every general computation-to-value boundary is written as `&`.
3. Every execution of an intermediate computation whose result is consumed is
   represented by a computation block BIND or another explicit core
   eliminator.
4. Function application inserts FORCE only from a proven thunked-Pi callee
   classifier.
5. A raw computation cannot be accepted as an ordinary APP argument.
6. Top-level raw computations, Lambda bodies, Match bodies, and block tails
   retain their computation polarity.
7. Source order of effectful block entries is preserved.
8. Direct and artifact compilation agree on polarity, classifier, effect row,
   and normalization behavior.
9. The compatibility automatic-coercion mode and the old surface `thunk` and
   `return` forms are removed rather than retained as alternate semantics.
