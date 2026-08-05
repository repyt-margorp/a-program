# Thunk-Encoded Higher-Order Operations and Resource Scopes

Date: 2026-08-05

## 1. Status and Scope

This document records the current design decision for operations which receive
computations as parameters.  It also states what the current prototype can
already represent, what remains only an encoding, and what is still required
before resource-scoped operations such as `with_file` are sound.

This document follows the implemented match-style computation-fold design in
`2026-08-05T00-00-00-MATCH-STYLE-COMPUTATION-FOLD-AND-INTRINSIC-RETURN-PLAN.md`.
Where older documents describe a singular handler clause or say that a separate
higher-order-operation syntax is required, this document supersedes them.

The primary decision is:

> Do not add a separate surface `higher_operation` construct or a separate
> `HIGHER_OPERATION` Core tag.  A computation parameter is quoted with `&` and
> passed as an ordinary operation argument.  The operation declaration and its
> handler contract determine whether that quoted computation is opaque, forced,
> handled recursively, discarded, or invoked more than once.

This decision establishes representability.  It does not by itself establish
effect safety, modular higher-order forwarding, resource lifetime safety, or
linearity.

## 2. Terms That Must Remain Distinct

Several previously discussed claims were inconsistent because they used
"higher-order operation" for four different properties.

### 2.1 Quoted-computation operation

An operation receives one or more thunk values which contain computations:

```text
op : Request(Thunk(Comp(E, A))) -> Comp({op}, B)
```

This is already representable by the existing source and Core syntax.

### 2.2 Higher-order handling

A handler may force, transform, or recursively handle a computation supplied in
an operation request.  This requires a handling policy for the inner
computation.  It is not implied merely by storing `THUNK(M)` in the request.

### 2.3 Scoped operation

A scoped operation distinguishes:

- the inner computation supplied to the operation; and
- the outer continuation after the operation returns.

Examples include bracketing, local state, exception scopes, and resource
scopes.  The current Core can represent both pieces, but it does not yet attach
a scope contract to them.

### 2.4 Resource-safe scoped operation

A resource-safe operation additionally guarantees properties such as:

- acquisition and release are paired;
- release runs on every supported exit path;
- a resource capability does not escape its region;
- an owning capability is not duplicated;
- a captured continuation cannot replay a unique resource unsafely.

These are typing and runtime properties beyond higher-order representability.

## 3. What the Current Prototype Can Already Do

### 3.1 Quote a computation as a value

`src/prototype/reader.c:1266` accepts `&` before a term atom and constructs
`PROTOTYPE_AST_QUOTE`.  Lowering uses `THUNK`, and the type layer uses
`THUNK_TYPE`.

Conceptually:

```text
M : Comp(E, A)
-------------------------
&M : Thunk(Comp(E, A))
```

`FORCE(THUNK(M))` is existing CBPV cut elimination.  No new Core node is
required to suspend or later execute an operation's inner computation.

### 3.2 Carry an arbitrary Term as an operation argument

`OPERATION_REQUEST` contains:

```text
operation
argument
continuation
```

The argument is an ordinary TermDB reference.  It may therefore be a thunk or
an ADT value containing one or more thunks.  The current unary request shape is
not a restriction on the number of logical parameters: a request ADT can
package paths, modes, values, and several quoted computations.

For example, a future request can be represented as:

```text
WithFileRequest.mk(path, mode, &body)
```

and the Core request as:

```text
OPERATION_REQUEST(
	with_file,
	WithFileRequest.mk(path, mode, THUNK(body)),
	THUNK(outer_continuation)
)
```

The request thunk is the inner computation.  The existing request continuation
is the outer computation.  Therefore the present Core graph has enough
structure to distinguish the two sides of a scoped operation.

### 3.3 Handle several nominal operations in one fold

The current surface form is plural:

```text
M
	@#.return value => return_body
	@op_1 request_1 continuation_1 => body_1
	@op_2 request_2 continuation_2 => body_2;
```

The reader stores up to 31 operation clauses.  AST, OperationGraph, TermDB,
typing, runtime dispatch, and artifact v55 all carry clause arrays.  The former
single-clause limitation is no longer present.

### 3.4 Deeply handle the outer continuation

When `COMPUTATION_FOLD` sees a request, `src/prototype/term.c:6086` rebuilds the
request continuation under the same fold.  Unknown requests are forwarded with
that folded continuation.

This is ordinary deep handling of the request's outer continuation.

### 3.5 Explicitly interpret a quoted inner computation

An operation clause receives the request value.  It can extract a thunk, force
it, and place another computation fold around the resulting computation.

Schematically:

```text
M
	@#.return x => return_case x
	@scope request k => {
		inner_result := (force request.body)
			@#.return x => inner_return x
			@op request_2 k_2 => inner_clause request_2 k_2;
		k inner_result;
};
```

`force` in this schematic example names the existing Core `FORCE` operation.
It does not decide that `force` should become a new surface keyword.  The
current elaborator may open a thunk when a computation demand requires it; an
explicit surface spelling remains a separate syntax decision.

Consequently, higher-order behavior is constructible without a new operation
node.  The recursive handling policy is explicit in the clause body.

### 3.6 Current capability matrix

| Capability | Current status |
|---|---|
| Parse `&M` | implemented |
| Lower `&M` to `THUNK(M)` | implemented |
| Store a thunk in an operation request argument | Core supports it |
| Package several thunk/value parameters in an ADT request | Core and ADT representation support it |
| Handle several nominal operation identities in one fold | implemented |
| Deeply fold the outer request continuation | implemented |
| Explicitly force and fold an inner thunk | representable; no declared test operation currently exercises it |
| Declare an operation domain as `Thunk(Comp(E, A))` | not supported by host-only declaration metadata |
| Infer a polymorphic latent effect row for such an operation | not implemented |
| Automatically traverse inner computations while forwarding | not implemented and not yet selected as policy |
| Prove one-shot use, region non-escape, and finalization | not implemented |

## 4. What the Encoding Does Not Provide Automatically

### 4.1 A thunk is opaque to an enclosing fold

The current fold recurses through the outer continuation.  It does not descend
into `THUNK(M)` stored in the request argument.  This is intentional for an
ordinary CBPV value: forcing a quoted computation is an observable scheduling
choice.

Therefore these policies remain distinct:

```text
opaque       forward or store &M without inspecting M
execute      force M without applying the current fold to M
deep         force M and recursively apply the current fold to M
shallow      handle selected requests in M without recursively wrapping its continuations
discard      never force M
duplicate    force M more than once
```

No generic handler may silently choose among these policies.

### 4.2 Generic forwarding of an unknown higher-order operation is unresolved

Current forwarding preserves the request argument unchanged and folds only the
outer continuation.  For an unknown request containing `&M`, this means the
handler does not enter `M`.

That is a coherent opaque-forwarding rule, but it is not the only possible
scoped-effect rule.  Some scoped calculi require explicit forwarding clauses
which state how both the inner computation and outer continuation are
transformed.  A Program must not claim general modular scoped handlers until a
forwarding contract is selected and checked.

### 4.3 Current operation declarations cannot state the real type

`src/prototype/term.h:141` limits effect-operation metadata to one host-type
argument and one host-type result.  `src/prototype/typing.c:5162` reconstructs
the operation classifier from that metadata.

This is sufficient for the currently declared `#.print`, but cannot express:

- `Thunk(Comp(E, A))` as a domain;
- an ADT request whose fields contain thunks;
- effect-row polymorphism in `E`;
- a result family depending on a request parameter;
- region quantification;
- linear or affine multiplicities.

This is the first representation barrier.  It is not a parser or TermDB
barrier.

### 4.4 Latent effects are not the operation's immediate effects

If `&M : Thunk(Comp(E, A))`, quoting `M` must not add `E` to the current effect
row.  Those effects are latent until the thunk is forced.

However, a declared operation which promises to execute `M` must account for
`E` in its result computation.  A fixed declaration such as:

```text
scope : Thunk(Comp(E, A)) -> Comp({scope}, A)
```

would under-approximate effects if every implementation executes `M`.  The
sound classifier is closer to:

```text
scope : forall E A.
	Thunk(Comp(E, A)) -> Comp({scope} union E, A)
```

Alternatively, `scope` may remain an uninterpreted operation whose handler
clause explicitly forces `M`; then the clause's inferred effects account for
`E`.  An unhandled host dispatcher must not secretly force the thunk under the
weaker signature.

### 4.5 Usage multiplicity is independent of the effect row

An effect row says which operations may occur.  It does not say whether a thunk
or continuation is forced zero, one, or many times.

For resource operations, the distinction is essential:

```text
discard body       acquisition may never be used
force body once    bracket-style execution
force body twice   duplicates effects and may duplicate resource use
save body          permits execution after the intended scope
```

Neither the current effect-row solver nor `COMPUTATION_FOLD` proves a one-shot
property.

## 5. Resource-Scoped Operations

### 5.1 `with_file` is representable with the selected encoding

A future surface use can remain ordinary operation syntax with a quoted body:

```text
perform (
	#.with_file
		(WithFileRequest.mk
			path
			mode
			&(\file : FileBorrow r => {
				bytes := #.read file;
				bytes;
			}))
)
```

No `higher perform` keyword is required.

The conceptual classifier is scoped and rank-polymorphic:

```text
with_file : forall E A.
	Path ->
	Mode ->
	(forall r. Thunk(Pi(FileBorrow(r), Comp(E, A)))) ->
	Comp(E union {Filesystem}, Result(OpenError, A))
```

The region `r` must not occur in `A`.  That non-escape condition prevents the
borrowed file capability from being returned after the file has been closed.

The exact surface representation of `forall r` is not decided here.  It may be
an elaborator-generated scope binder rather than user syntax.

### 5.2 Resource lifetime is not solved by thunk encoding

The handler or runtime interpretation must still ensure:

1. acquisition is attempted once per dynamic invocation;
2. a finalizer is installed only after successful acquisition;
3. release runs after normal return;
4. release also runs after `!` lambda exit, abortive operation, supported error,
   and cancellation;
5. the inner thunk and continuation cannot replay unique ownership illegally;
6. the borrowed handle cannot escape the region.

These obligations require a resource-scope protocol, not another syntax for
passing the body.

### 5.3 Live resources must not be TermDB nodes

`src/prototype/ast.c:1394` stores runtime bindings as `uint32_t` TermDB IDs, and
runtime substitution materializes those values into TermDB graphs.  That is
valid for ordinary language values but not for an OS file descriptor or
`FILE *`.

The established invariant remains:

> TermDB is the computation graph, not the execution environment or resource
> table.

A live file must therefore use a runtime-only value and resource table, for
example a `(slot, generation)` reference.  Artifacts may serialize operation
requirements and backend capabilities, but never an open resource.

## 6. Decisions

### D1. Preserve one operation-request Core form

Keep `OPERATION_REQUEST(operation, argument, continuation)`.  Do not introduce
separate first-order, higher-order, scoped, or resource request tags merely
because the argument contains thunks.

### D2. Use request ADTs for logical arity

Keep the operation request structurally unary.  Package multiple values and
quoted computations into a typed request ADT.  This avoids adding another
variadic spine beside ordinary APP and constructor formation.

### D3. Make quoted inner computations explicit

Use `&M` as the surface boundary which suppresses execution and produces a
thunk value.  Do not automatically quote a computation solely because an
operation happens to expect one.

### D4. Keep inner and outer handling policies separate

The existing deep fold continues to transform the outer continuation.  It does
not automatically enter thunk arguments.  An operation declaration or handler
clause must explicitly select the inner-computation policy.

### D5. Opaque forwarding is the current default

Until a general scoped-handler contract is implemented, an unrecognized
operation forwards its thunk argument unchanged and recursively folds only its
outer continuation.  This behavior is precise and implementable, but must not
be advertised as general higher-order forwarding.

### D6. Replace host-only operation signatures

The authoritative operation classifier must become a graph-level classifier
family.  Host type IDs, fixed arity, operation labels, and backend capability
flags may remain derived declaration metadata, but they must not define the
language-level type.

Conceptually:

```text
effect operation declaration
	operation identity
	classifier family Term
	language effect label
	required backend capabilities
	inner-computation policy
	resumption multiplicity
```

The final layout is deferred until the constraint representation for effect-row
polymorphism is fixed.

### D7. Do not infer resource safety from effect membership

`Filesystem` in an effect row records permission or possible execution.  File
identity, open/closed state, ownership, region, and release obligations belong
to resource values and usage constraints.

### D8. Introduce resources through scoped borrowing first

The first safe File API should expose `with_file` and a non-escaping borrowed
capability.  A public manual `open`/`close` ownership API should wait until
linear or affine multiplicities, typestate transitions, and exceptional exits
are specified.

### D9. Do not place live resources in TermDB

Introduce a tagged runtime-value domain and runtime resource table before a
host File operation returns a handle.  An opaque integer encoded as an ordinary
Term is not an acceptable substitute.

## 7. Required Implementation Work

### Stage 1: Graph-level operation classifiers

- Replace `argument_types[]` and `result_type` as the authoritative operation
  signature.
- Allow domains containing user ADTs, `THUNK_TYPE`, dependent Pi families, and
  effect-row variables.
- Keep nominal operation identity separate from required host capabilities.
- Serialize the resolved classifier family in the artifact interface.
- Add tests for an operation whose request ADT contains `&M`.

### Stage 2: Explicit higher-order handling tests

- Define a non-resource test operation which receives a thunk.
- Verify a handler can discard the thunk.
- Verify a handler can force it once.
- Verify a handler can explicitly fold its inner computation.
- Verify an unknown handler forwards the thunk unchanged.
- Verify latent effects do not enter the ambient row before force.

This stage establishes the encoding independently of File lifetime concerns.

### Stage 3: Handling contracts

- Add declaration-level inner-computation policy only if generic handling needs
  automatic behavior.
- Distinguish opaque, scoped-deep, and implementation-defined policies.
- Add resumption multiplicity: at least one-shot, multi-shot, and abortive.
- Reject a resource-scoped declaration whose handler may duplicate ownership.
- Specify how unknown scoped operations are forwarded.

### Stage 4: Runtime values and resource scopes

- Replace term-only runtime bindings with tagged runtime values.
- Add a resource table with kind, slot, generation, state, and finalizer.
- Add resource-scope/finalizer frames to the runtime machine.
- Unwind finalizers through normal return, lambda exit, handled abort, and
  runtime failure.
- Ensure normalization and type conversion always block File operations.

### Stage 5: Region and multiplicity checking

- Add occurrence-level usage constraints to ContextDB/JudgementDB rather than
  TermDB canonical keys.
- Prevent unrestricted thunks and ADTs from capturing an owning capability.
- Check linear resources consistently across Match branches.
- Ensure handler resumptions respect their declared multiplicity.
- Add a generated region binder and a non-escape check for `with_file`.

## 8. Validation Gates

The design is not complete merely because `perform (#.op &M)` parses.  The
following gates distinguish levels of completion:

| Gate | Required observation |
|---|---|
| Representation | `&M` is retained as a thunk in the request argument |
| Typing | the operation domain proves the thunk's full computation classifier |
| Latency | effects of `M` are absent before force and present after force |
| Outer deep handling | forwarded continuations remain under the current fold |
| Inner handling | the declared or explicit policy for `M` is observed |
| Forwarding | unknown higher-order requests preserve a specified meaning |
| Multiplicity | one-shot contracts reject duplicate resumption or force |
| Scope | region-bound resource values cannot escape |
| Finalization | every supported exit path releases each acquired resource once |
| Artifact | no live runtime resource is serialized |

## 9. Theoretical Basis

The following primary sources motivate the distinctions used here:

- Birthe van den Berg and Tom Schrijvers, *A Framework for Higher-Order Effects
  & Handlers*: higher-order signatures generalize ordinary algebraic signatures
  to effects which contain internal computations, including scoped, latent, and
  bracketing effects. <https://arxiv.org/abs/2302.01415>
- Roger Bosman, Birthe van den Berg, Wenhao Tang, and Tom Schrijvers,
  *A Calculus for Scoped Effects & Handlers*: scoped operations distinguish
  computations inside and outside a scope and require an explicit account of
  forwarding unknown scoped operations. <https://arxiv.org/abs/2304.09697>
- Cas van der Rest, Jaro Reinders, and Casper Bach Poulsen, *Handling
  Higher-Order Effects*: operations with computations as parameters require a
  handling account beyond ordinary first-order operation clauses.
  <https://arxiv.org/abs/2203.03288>
- Gordon Plotkin and Matija Pretnar, *Handling Algebraic Effects*: ordinary
  algebraic handlers are induced homomorphisms from free models; this is the
  baseline which higher-order and scoped handlers extend.
  <https://arxiv.org/abs/1312.1399>

The A Program decision is not that thunk encoding makes every higher-order
effect algebraic.  The decision is narrower:

> The existing CBPV quotation boundary is sufficient to represent internal
> computations in operation requests.  Additional type, handling, forwarding,
> multiplicity, and lifetime rules are layered on that representation instead
> of introducing duplicate operation syntax.
