# Unified Dependent CBPV and Algebraic Effects Migration

Date: 2026-07-14

Status: migration in progress. TermDB supports the shared `RETURN`, `THUNK`,
and `FORCE` boundary nodes, including artifact traversal, relocation, and
normalization. The source reader and lowering accept `return t`, `thunk t`,
and `force t`. Their classifiers are derived through `RETURN_INTRO`,
`THUNK_INTRO`, and `FORCE_ELIM`.

`PI` codomain families now use the validated representation
`THUNK(LAMBDA(..., RETURN(...)))`; a raw lambda is rejected by the `PI` constructor.
The accessor `prototype_term_pure_family_lambda` is the sole way for Pi
consumers to inspect the enclosed lambda. Artifact reconstruction and imported
dependent constructor classifier relocation traverse this wrapper. Source
`perform` and dependent sequencing lower to and type-check as the shared
CBPV nodes. User-defined `handle` graphs now use the same thunked continuation
boundary as ordinary source calls. The Match-motive cutover remains.

Update: source definitions whose right-hand side is a lambda now use raw CBPV
polarity. `id := \\x : A => x` lowers to `LAMBDA(x, RETURN(x))`, not to a
thunk wrapper, and its outer classifier is `Pi(A, Comp({}, A))`. A lambda
used in a value position still lowers to `THUNK(LAMBDA(...))`. This records
the selected policy; historical value-first text below is superseded where it
conflicts with this decision.

## Current Boundary and Deferred Effect Polymorphism

The raw-function rule is now implemented and tested. A definition is not
required to be value-first: a top-level lambda is a computation function, and
a use of that definition where a value is required inserts `THUNK`; a call
inserts `FORCE`. Consequently both of the following are valid source-level
forms without an explicit CBPV keyword:

```text
id := \\x : Nat => x;
apply := \\f : Nat -> Nat => f Nat.zero;
main := apply id;
```

The surface arrow in a lambda binder now introduces one scoped latent effect
row. The classifier for a higher-order forwarding function is:

```text
apply : forall e.
  Thunk(Pi(A, Comp(e, B))) -> Comp(e, B)
```

`EFFECT_ROW_FORALL(row_binder, classifier)` is a classifier-only TermDB node;
it binds `EFFECT_ROW_VAR(row_binder)` structurally and has no runtime
argument. It participates in alpha comparison, canonicalization,
substitution, graph relocation, artifact read/write, and readback. An
application specializes the row from the actual thunked function argument
before applying the ordinary Pi rule. The application proof retains the
original scheme and actual argument as its premises; specialization is a
deterministic kernel check, never a global DefEq extension.

This is intentionally not a free variable: replacing an empty row in `A -> B`
with an unbound `e` would make occurrence selection unstable. The enclosing
lambda is the only generalization site. Current regression coverage verifies
both `apply id` and forwarding a function which performs `Terminal`:

```text
forward := \f : Text -> Text => f "x";
printer := \text : Text => perform (print text);
main := forward printer; // Comp(Terminal, Text)
```

Symbolic `BIND` retains both the unit-law case `union(e, {}) = e` and a
non-empty row composition. In particular, the following compiles without
requiring the bind result to be classified during graph lowering:

```text
both := \f : Text -> Text =>
  let x <- f "x" in perform (print x);
```

Its generalized classifier contains `Comp(union(e, Terminal), Text)`, and an
application to `printer` specializes that row to `Terminal`. Lowering builds
the dependent operation codomain structurally when an operand classifier is
not yet available; the ordinary application constraint later proves the
operand domain. This keeps TermDB construction graph-first rather than
value-first. `EFFECT_ROW_UNION` remains a symbolic TermDB row and is not an
approximation of `e` by the empty row.

The deliberately deferred boundary is now narrower: surface annotations have
no row syntax, and only direct arrow binders introduce an implicit generalized
row. Nested arrow annotations and exported function ascriptions still require
a separate, explicit effect-row surface design before they can express or
quantify arbitrary row schemes.

## Discussion Required Before Surface Effect Syntax

The current implementation establishes the graph-first invariant, but it does
not settle the surface language for effects. These decisions remain explicit:

1. `A -> B` is role-directed today. A direct definition exports a raw
   computation function, while a binder, constructor field, or returned
   function denotes its thunked value. The project must decide whether a
   future explicit `Comp(E, A)` notation replaces this contextual reading or
   merely makes the effect row visible.
2. `EFFECT_ROW_FORALL` is currently introduced only for a direct arrow binder.
   Generalizing nested arrows, let-bound functions, and imported scheme
   interfaces needs value restriction and generalization boundaries specified
   together; it must not be added as an unconditional free row variable.
3. `EFFECT_ROW_UNION` is symbolic. Its current kernel laws cover the empty-row
   unit and call-site substitution. Associativity, idempotence, handler
   subtraction, and any row-variable unification must be specified as a
   separate row-constraint solver, not hidden in normalization equality.
4. Lowering may construct an operation application before its operand has a
   classifier. This is intentional and does not make checking permissive: the
   ordinary APP/Pi constraint remains responsible for proving the operand
   domain before the program is accepted.

## 2026-07-14: Raw Function and Non-Speculative Lowering Boundary

The top-level boundary is now explicitly **not value-first**. A source
definition whose right-hand side is a lambda is lowered as the raw CBPV
function:

```text
id := \x : Int => x

TermDB(id)   = LAMBDA(x, RETURN(x))
TermDB(id #1) = APP(LAMBDA(x, RETURN(x)), #1)
```

No `THUNK`/`FORCE` pair is introduced for that direct call. A reference to the
same definition in a value position still becomes a thunk; this is required
for a higher-order binder such as `\f : A -> B => ...`.

Application lowering now classifies the AST head before it builds any TermDB
node:

- a host-operation head uses operation-spine lowering;
- a constructor head first attempts a value spine;
- every other head uses ordinary CBPV application and sequencing.

Earlier code attempted the first two alternatives speculatively. A rejected
alternative could leave an unreachable `RETURN`, `THUNK`, or `APP` in TermDB.
The global computation-constraint pass could then manufacture a judgement for
that abandoned node. This is invalid because TermDB is allowed to retain
evaluation intermediates, but JudgementDB must only contain derivations for a
selected source occurrence. The new dispatch avoids creating those candidates.

A constructor head is not always a value spine. For example, in
`Nat.succ (*k m)`, the recursive application is a computation. The argument
must be sequenced and only then passed to `Nat.succ`. Therefore a constructor
value-spine attempt that finds a non-value argument falls back to ordinary
CBPV application; it is not a lowering error.

Operation request continuations also now record the alpha-canonical lambda
body returned by TermDB, rather than the provisional binder used while that
lambda is constructed. This keeps the `RETURN_INTRO` premise and its binder
assumption on the same canonical node. Type-former anchor rebinding likewise
preserves a Match frame whose `match_term` is still `INVALID_ID`; allocation
precedes frame commitment during lowering.

Regression scope currently includes examples `01` through `07` and `09`,
`training/double.p`, raw direct functions, higher-order thunking, symbolic
effect-row forwarding, artifact round-trips, and shared-core occurrences.
Examples `10` and later are deliberately outside this CBPV regression script:
they depend on the unfinished general Match-motive/constraint solver and are
not evidence for or against this raw-function boundary.

The next discussion items are:

1. Specify source syntax for latent effect rows and exported effect-polymorphic
   annotations, instead of using an empty-row placeholder.
2. Replace the current direct guarded-IH recognizer with explicit Match-motive
   constraint generation and solving, so examples `10+`, multiple recursive
   calls, dependent motives, Sigma, and IADT share one mechanism.
3. Restrict the global computation-constraint scan to source-reachable
   operations or replace it with occurrence-local registration. Non-speculative
   lowering prevents the current orphan-node failure, but reachability should
   become an explicit invariant before evaluation-generated terms are used
   more broadly.

The core now also contains `BIND(computation, binder, body)` and
`OPERATION_REQUEST(operation, argument, continuation)`. `BIND(RETURN(v), x,
body)` substitutes `v` into `body`; binding a request composes its
continuation without executing it. Both nodes participate in alpha-aware core
shape comparison, canonical keys, substitution, external-reference rewrite,
graph compaction, artifact read/write/relocation, and readback. Artifact
format version 36 records these node meanings and serializes host operation
references by their source key rather than by a host-table enum value.

The normalizer profiles are now named for the intended semantic boundary:
`CORE_WHNF`, `COMPUTATION_WHNF`, and `PURE_TYPE_WHNF`. Kernel conversion uses
only `PURE_TYPE_WHNF`; none of these profiles dispatches a host effect.
`CORE_WHNF` performs beta only and stops at `FORCE`, `BIND`, and `HANDLE`.
`COMPUTATION_WHNF` additionally performs structural CBPV cut rules, without
host dispatch. `PURE_TYPE_WHNF` admits only CBPV beta justified by a
`RETURN` result; it stops at requests and handlers. This prevents a handler
interpretation from becoming a type-conversion rule. It also has a distinct
normalization-cache identity from computation WHNF.

## Decision

The prototype will use one graph syntax for `LAMBDA`, `APP`, and `MATCH`,
rather than keeping a static graph beside a separate computation graph. This
is required by the project's existing goal that one computation has one core
graph identity.

Whether an occurrence is a value, a computation, or a type-level expression
is derived uniformly from its synthesized classifier. The classifier is the
single source of truth; TermDB tags do not encode a duplicated evaluation
language. Values are introduced into a computation by `RETURN`, and a
computation can be held as a value by `THUNK`.

The representation for Pi codomains is a thunked computation whose effect row
is empty and whose result is a Universe value. This retains one Lambda/App/
Match implementation while preserving the boundary that type conversion never
runs IO, a handler, or an arbitrary operation. At the present stage purity is
enforced by the only constructible family shape, `THUNK(LAMBDA(..., RETURN(...)))`.
Once `perform` exists, Pi formation must additionally validate the
effect-empty computation classifier rather than rely on this syntactic subset.

The target is a restricted dependent CBPV system with dependent sequencing.
The dCBPV+ rule must not be read as allowing a global classifier to contain
the run-time bind result `x`. Its dependency is instead represented through a
value `z : Thunk(Comp(E, A))`, and the result of `BIND(M, x, N)` has the
family result `B[THUNK(M) / z]`. The body may use the local value `x`, but the
published classifier is closed by the thunk of the input computation. Thus a
type can observe the identity of a computation as an opaque value without
executing it. This is the required boundary between dependent sequencing and
kernel purity.

## CBPV Placement: One Core, One Classifier Boundary

CBPV must not be implemented by cloning `APP`, `LAMBDA`, or `MATCH` into a
second value graph and a second computation graph. That would lose the
project's intended invariant: two annotations over the same lambda-calculus
calculation point at one core graph.

The split is therefore exactly this:

```text
TermDB:        one untyped structural graph
JudgementDB:   classifiers and derivations for occurrences of that graph
Runtime:       continuation, handler stack, capability, and external state
```

`APP` and `LAMBDA` remain the sole application and binder payloads. Their
reduction is the shared beta rule over the core graph. CBPV changes which
well-typed *uses* are admitted: `RETURN` introduces a value into a
computation, `BIND` sequences computations, and `THUNK` is the only way to
make a computation available as a value. No `TYPE_APP`, `VALUE_APP`,
`COMPUTATION_APP`, or typed-lambda term tag is allowed.

"Making APP and LAMBDA CBPV" therefore does not mean replacing their TermDB
tags. It means that source elaboration and the judgement solver assign each
occurrence a polarity through its classifier. A raw `APP` is still one core
application node. Crossing from a value to a computation or back is explicit
and uses only `RETURN`, `THUNK`, `FORCE`, and `BIND`. The type layer must prove
that a family is pure through this same boundary; it must not run an alternate
type-level APP/LAMBDA evaluator. This keeps purity a property that can be
checked uniformly, rather than an accidental consequence of using a second
syntax tree.

The type layer is not a second evaluator. A type family is a value whose
classifier proves it is a suspended, empty-effect computation returning a
Universe value. The present concrete representation is
`THUNK(LAMBDA(..., RETURN(...)))`. It is a classifier-level purity boundary around
the same core lambda, not an alternate lambda implementation. This is the
place where purity is checked; the core beta normalizer itself must not choose
different semantics according to `Bool -> Bool` versus `Nat -> Nat` views of
the same lambda.

## Selected Raw Function Boundary

The source definition boundary is a computation boundary. `compile_def`
recognizes a lambda right-hand side and lowers it through the raw lambda path:

```text
id := \\x : A => x

id = LAMBDA(x, RETURN(x))
id : Pi(A, Comp({}, A))
main := id value
main = APP(id, value)
```

This does not make every Pi-shaped term a computation function. Constructors
also have Pi classifiers but produce values. The elaborator identifies a raw
source function from its unthunked core `LAMBDA`, while constructor-headed
applications remain value spines and become `RETURN(APP(...))` only when a
computation is required. This distinction is required for the recursive body
`Nat.succ (*k m)`: the IH result is bound and forced, then passed to the value
constructor before that constructed value is returned.

The generic reading of an arrow remains a function value type:

```text
value(A -> B) = Thunk(Pi(value(A), Comp(E, value(B))))
definition(A -> B) = Pi(value(A), Comp(E, value(B)))
```

This is a role-directed presentation of one negative function type, not a
return to value-first definitions. In particular, a curried source result is
a value boundary: the outer computation must return a thunk containing the
next raw function. Treating every nested textual arrow as an unthunked result
would make `Nat -> Nat -> Nat` attempt to return a computation as a value and
break `append` and `add`. The current parser has no explicit `Comp` or thunk
type notation, so source role is the only available way to distinguish these
two CBPV kinds.

The two readings occur in distinct source roles, not as two meanings for one
operation occurrence. A definition exports a raw computation function. A
binder such as `\\f : A -> B => ...`, a constructor field, or a returned
function needs a value and therefore uses the thunked reading. Effect row `E`
is unspecified when source syntax does not specify it. The current elaborator
uses the empty row only as that annotation placeholder, not as a claim of
purity. Thus `main :: A -> B` constrains the result type but its lambda can
still synthesize `Pi(A, Comp({Terminal}, B))`. The actual row remains on the
classifier and is not replaced by the annotation placeholder. Row variables
remain internal to the effect solver until surface row syntax exists.

The value boundary is now inserted for an ordinary named function reference.
For example:

```text
id := \\x : Nat => x;
apply := \\f : Nat -> Nat => f Nat.zero;
main := apply id;
```

lowers `id` itself to raw `LAMBDA(x, RETURN(x))`, but lowers the argument
occurrence of `id` to `THUNK(LAMBDA(x, RETURN(x)))`. `apply` subsequently
uses `FORCE(f)` before `APP`. No source-level `thunk` keyword is required for
this ordinary higher-order use. Direct function application still observes the
raw computation function path; `FORCE(THUNK(...))` introduced at a named
value boundary is a normal CBPV administrative redex.

For a closed function body, the classifier solver already computes the row by
unioning the rows of its computation children. Thus a function containing
`perform (#.print ...)` exports `Pi(A, Comp({Terminal}, B))`, while a pure
function exports `Pi(A, Comp({}, B))`. This information belongs to the raw
function classifier, not to the core lambda node or to its surface name.

The remaining limitation is genuine higher-order effect polymorphism. The
annotation `f : A -> B` currently means that `f` is a value containing a raw
function, but source syntax does not bind a row variable for its latent effect.
Consequently a function that calls an arbitrary `f` cannot yet export a
universally quantified row such as `Pi(e, Thunk(Pi(A, Comp(e, B))) ->
Comp(e, B))`. Treating the omitted row as an empty row would be unsound;
treating it as a wildcard is only an annotation compatibility rule. This must
be solved by explicit or implicit row quantification, not by silently adding
proof-specific rows to the TermDB.

Open work before higher-order effectful programs:

1. Add source syntax or an internal, scoped implicit binder for closed, open,
   and universally quantified effect rows. Function
   annotations currently leave their row unspecified, so they cannot require
   `Terminal`, assert purity, or state effect polymorphism explicitly. The
   corresponding artifact interface must preserve that binder scope.
2. Specify the dCBPV+ family result of dependent `BIND` without allowing the
   kernel to execute an effectful computation while forming a classifier.
3. Define whether an arbitrary computation, rather than a raw function, may
   ever be suspended and stored as a value. This is intentionally not inferred:
   effectful computations must not become delayed values merely because they
   occur in a value-required position.

### Dependent-Constructor Boundary Regression

The local reproducer is:

```text
Sigma := \A : @ => \B : A -> @ => @{ mk : (a : A) -> B a -> *; };
Nat := @{ zero : *; succ : * -> *; };
ConstNat := \x : Nat => Nat;
main := (Sigma Nat ConstNat).mk Nat.zero Nat.zero;
```

The classifier solver normalizes the second field domain `ConstNat Nat.zero`
through pure CBPV reduction and obtains `Nat`; the APP proof constructor and
validator use the same boundary. The remaining proof-origin issue was that a
source lambda whose binder occurred only in annotations canonicalized to a
core lambda with `PI_UNUSED_BINDER`. Its JudgementDB premise still used the
source binder, leaving an unresolved proof edge.

Binder assumptions are now materialized from the canonical lambda binder. The
source lambda operation remains in OperationGraph metadata, while JudgementDB
premises refer only to the TermDB graph they prove. Artifact append also offsets
the binder ID stored in lambda/bind proof contexts. The reproducer is covered
by a local compile and artifact round-trip regression in
`src/prototype/test_cbpv_surface.sh`.

This must remain a graph-level classifier-family path. Do not replace it with
field metadata: that would again lose the dependency needed by Sigma/IADT
constructors.

## Required Direct-Style Elaboration Cutover

The old `THUNK(RETURN(LAMBDA(...)))` family wrapper was transitional. It
made a raw `LAMBDA` a value-like direct-style result and has been replaced by
the canonical pure family below.

The final representation keeps the same TermDB tags, but assigns their CBPV
polarity by classifier:

```text
value        V ::= variable | constructor | thunk computation | ...
computation  M ::= return value | lambda value-binder . computation |
                  application computation value | match ... | ...
```

At core level, a lambda is `LAMBDA(x, computation_body)` and an application
is `APP(computation_function, value_argument)`. Surface syntax keeps ordinary
functions as values by elaborating `\\x => value_body` once to
`THUNK(LAMBDA(x, RETURN(value_body)))`. Its source arrow is consequently a
value classifier:

```text
A -> B  =  ThunkType(Pi(A, Comp({}, B)))
```

`Pi(A, Comp(E, B))` remains the classifier of a computation-level function;
it is used directly by host operation declarations and by `FORCE(f)`, where
`f : ThunkType(Pi(A, Comp(E, B)))`. A surface call to such a function creates
the one core computation `APP(FORCE(f), value) : Comp(E, B)`. It is not
silently also a value. If a surrounding source construct needs that result as
a value, lowering inserts the one required `BIND`; if the enclosing position
already expects a computation, it uses the application directly. Thus the
compiler has an expected-polarity parameter, but it does not build two ASTs,
two application graphs, or two evaluators.

This is also the rule for source type expressions. It is deliberately
different from the internal operation-signature builder, which constructs the
computation function classifier `Pi(A, Comp(E, B))` directly. A dependent Pi
family is represented by the same pure value
`THUNK(LAMBDA(x, RETURN(type_body)))`, not by a separate type-lambda tag and
not by an alternate type evaluator.

## Source Elaboration Contract

The cutover must replace the existing raw `compile_ast_ref` paths, rather than
adding a parallel CBPV compiler. There is one recursive lowering engine with
an expected polarity parameter:

```text
lower(expr, Value)       -> value term and one operation occurrence
lower(expr, Computation) -> computation term and one operation occurrence
```

The two entry points are wrappers around the same source occurrence graph.
They differ only by explicit CBPV boundary nodes:

```text
lower(v, Computation)        = RETURN(lower(v, Value))
lower(\\x => e, Value)       = THUNK(LAMBDA(x, lower(e, Computation)))
lower(f a, Computation)      = APP(FORCE(lower(f, Value)), lower(a, Value))
lower(let x <- m in n, Computation)
                            = BIND(lower(m, Computation), x,
                                   lower(n, Computation))
```

`lower(expr, Value)` may not invent a second application representation. If
`expr` is computation-producing, its enclosing computation context must
sequence it with `BIND`. This is an ANF/CPS elaboration concern, not a TermDB
concern. Match scrutinees, constructor fields, and function arguments request
a value; their enclosing computation supplies the necessary bind. This is the
single point where direct-style source syntax is translated into CBPV.

The old `compile_ast` plus `compile_ast_ref` split currently violates this
contract: both recursively construct raw `LAMBDA` and raw `APP`, while the
CBPV forms are only recognized for explicit surface keywords. The migration
must remove the raw fallback after the polarity-aware lowering covers every
AST tag. No compatibility path may preserve raw source lambdas or source
applications.

## Surface Syntax Retraction Before dCBPV+

The CBPV core nodes are not a mandate to expose corresponding source keywords.
Before adding a source notation for the dCBPV+ result family `B(z)`, the
surface language must return to an ordinary Lambda/App/Match style wherever
the elaborator can insert the boundary unambiguously. This is a source-reader
and elaboration change only: `RETURN`, `THUNK`, `FORCE`, `BIND`,
`OPERATION_REQUEST`, and `HANDLE` remain core TermDB nodes and continue to
have the same typing and runtime rules.

### Current Reader Keywords

The current reader recognizes these CBPV-oriented words:

```text
let x <- M in N
return V
thunk M
force V
perform op
handle (M) with (op) arg continuation => N ; return value => R
```

`in` is currently a lexer token and is therefore globally unavailable as an
identifier. `let`, `return`, `thunk`, `force`, `perform`, `handle`, and
`with` are parser-contextual words, but they still prevent a bare occurrence
at an expression position from denoting an ordinary name. `BIND` itself is
not a source keyword.

The following code already demonstrates that the core boundary need not be
spelled at the source level:

* `compile_ast_computation_ref` falls back to `RETURN` when a source value is
  required as a computation.
* `compile_ast_lambda_value_ref` lowers an ordinary source lambda directly to
  `THUNK(LAMBDA(...))`.
* `compile_application_argument_continuation` lowers an ordinary non-
  constructor application through `FORCE`, `APP`, and a compiler-created
  `BIND` when its result is needed as a value.
* `compile_ast_value_then` already sequences an occurrence whose classifier
  is a computation before passing its result to a value consumer.

Thus the source spelling and the core CBPV representation can be separated
without introducing a second graph or a second evaluator.

### Retraction Decision Table

| Current spelling | Target surface policy | Required implementation work |
| --- | --- | --- |
| `return V` | Remove. A value in a computation position elaborates to `RETURN(V)`. | Remove only the reader production after regression tests prove all value-to-computation sites use `compile_ast_computation_ref`. Keep the AST and TermDB tags internal. |
| `force V` | Remove from ordinary calls. Function application remains `f x`; lowering inserts `FORCE`. | Already implemented for ordinary source application. Retain a non-keyword escape hatch for forcing an arbitrary suspended computation. |
| `let x <- M in N` | Prefer ordinary application `((\\x : A => N) M)` when `A` is available. | Existing application lowering already sequences a computation argument. Full removal additionally requires omitted lambda-binder annotation to generate a domain constraint from `M`; current lambda syntax requires a binder type whereas `let` does not. Do not remove `let` before that constraint path exists, or accepted programs lose expressiveness. |
| `perform op` | Remove from source. An application classified as an operation computation, when demanded as a computation, elaborates to `OPERATION_REQUEST`. | Move request construction out of `compile_ast_perform_ref` into the shared operation-spine/computation lowering path. Preserve a value-context rule: an operation application used as data must be explicitly suspended rather than accidentally requested. |
| `thunk M` | Do not retain as a keyword. Keep one explicit non-keyword boundary form for passing a computation as data. | Introduce a typed system intrinsic such as `#.thunk M`, recognized by elaboration rather than host dispatch. A boundary cannot be inferred in every value position: without it, a computation cannot be intentionally passed or stored as a value. |
| `handle ... with ...` | Do not retain as keywords. Keep handling explicit as an ordinary intrinsic application. | Define an application-shaped `#.handle M op operation_clause return_clause`; clauses are ordinary source lambdas, so their binders use existing lambda scope. Lower this recognized intrinsic to the existing `HANDLER/HANDLE` core nodes. |

The exact names `#.thunk`, `#.force`, and `#.handle` are provisional. Their
semantic requirement is not provisional: they must be elaborator-recognized
core-boundary intrinsics, not host operations and not effect requests. They
therefore require no runtime handler capability and cannot be dispatched by
the host operation table.

### Why `let` Cannot Be Deleted Mechanically

The current source lambda grammar requires `\\x : A => N`, while `let x <- M
in N` obtains the local binder classifier from `M : Comp(E, A)`. The current
`solve_bind_constraint` deliberately creates that local binder assumption
after the input computation classifier is known. Replacing every bind with a
source lambda now would require users to write an annotation that the old
`let` did not require, or would create an unimplemented unannotated-lambda
inference path.

The prerequisite is a normal constraint-generation rule, not dCBPV+:

```text
lower((\\x => N) M)
  creates a fresh domain variable A
  constrains M : Comp(E, A)
  checks N under x : A
```

This rule must remain occurrence-scoped, because the shared core lambda can
be used at different annotated source occurrences. It solves the ordinary
domain constraint only. It must not be conflated with the later dCBPV+
problem of publishing a result family `B(thunk M)`.

### Historical Value-First Alternative (Superseded)

The following alternative describes the earlier value-first elaborator. It is
kept to explain the migration constraints, but it is not the current source
semantics. The selected raw-function policy is specified above.

The old elaborator exported a lambda definition as `THUNK(LAMBDA(...))`.
That policy is no longer current. It is retained only as a migration warning:
changing the definition boundary without also changing function annotations
and application lowering creates an incoherent mix of raw and value functions.
The selected policy is the raw-function boundary above. A higher-order value
uses a thunked function type; a top-level named lambda exports the raw
function computation.

### Required Execution Order

Complete these stages before choosing or implementing a dCBPV+ family
annotation.

1. Add surface regression tests that use no `return`, `force`, `perform`, or
   `let` for ordinary pure functions, constructor values, Match branches, and
   operation calls. Assert their printed core graph still contains the
   appropriate CBPV nodes.
2. Centralize operation-request elaboration in the existing operation-spine
   path. Delete the `perform` reader production only after a computation-
   context operation call produces `OPERATION_REQUEST` and a value-context
   occurrence does not.
3. Add elaborator-only system boundary intrinsics for suspension, forcing, and
   handling. They must lower to the existing core nodes and must not enter the
   host operation declaration table.
4. Introduce unannotated source lambda binders through ordinary domain
   constraints. Then desugar source `let` to that lambda/application form and
   remove `let` and lexer-reserved `in`.
5. Remove the remaining reader productions for `return`, `thunk`, `force`,
   `perform`, and `handle`; retain internal AST constructors only as a
   compiler test API until the reader tests no longer require them.
6. Only then design the dCBPV+ family syntax or a deliberately restricted
   family-synthesis rule. It must build on the single application/binder
   elaborator from the preceding stages.

This ordering is intentional. It prevents a new dCBPV+ syntax from being
designed around temporary CBPV keywords and prevents an implicit operation
call from becoming an accidental host dispatch in a value context.

### Atomicity Requirement for the Source Cutover

This is not safely separable into a "lambda conversion" followed later by an
"application conversion". A concrete failure is the ordinary curried source
term:

```text
id Bool Bool.true
```

After the first source application, the direct-CBPV elaboration has a
computation whose result is the value needed as the function of the second
application:

```text
APP(FORCE(id), Bool) : Comp({}, Thunk(Pi(...)))
```

The second application cannot use that computation directly. It must be
lowered through a fresh, compiler-only binder:

```text
BIND(APP(FORCE(id), Bool), f, APP(FORCE(f), Bool.true))
```

Treating the first `APP` as both a computation and a value would recreate the
very dual semantics that CBPV is intended to remove. Consequently the source
cutover has one atomic implementation unit:

1. Replace `compile_ast_ref` with one continuation/ANF elaborator.
2. Make its public result a computation, `lower_computation(ast)`.
3. Implement `lower_value(ast, continuation)` only as an internal operation;
   it never publishes an unsequenced computation as a value.
4. Lower constructor spines as value introductions, but lower ordinary calls
   through `FORCE`, `APP`, and `BIND`.
5. Lower every Match branch to a computation before constructing the one
   shared `MATCH` node.
6. Only after all of the above, change source lambda and source arrow to
   `THUNK(LAMBDA(..., computation))` and
   `ThunkType(Pi(A, Comp({}, B)))` respectively.

The current compiler deliberately remains on the old lowering until this
unit is present. In particular, changing only the `PI` classifier view or
only the source-lambda representation is prohibited: it makes a source
application consume a computation as a value and breaks ordinary curried
programs before a type error can explain the issue.

The compiler-only binders introduced by ANF lowering are TermDB binders, but
they are not source binder names and must not be published in the name layer.
Their JudgementDB facts are scoped to their `BIND` operation. This is an
elaboration detail, not a new user-visible binder syntax and not a second
operation graph.

Before changing source lambda lowering, the operation classifier solver must
gain occurrence-scoped constraints. A core `LAMBDA(x, RETURN(x))` is shared by
many source occurrences, for example an `Int -> Int` identity and a `Text ->
Text` identity. Their binder and body facts must not be combined as a global
Cartesian product merely because they point at the same core lambda node.
Each source operation occurrence therefore needs an explicit scoped premise
edge to its binder fact and body operation. Only after that relation exists
may lowering change a source lambda from raw `LAMBDA(x, body)` to
`THUNK(LAMBDA(x, computation(body)))`. This is a prerequisite, not a
compatibility fallback: without it the CBPV classifier pass would construct
spurious function types for a shared core term.

The first part of this prerequisite is now implemented in `ast.c`. Solver
input facts no longer retain only `(core_var, classifier)`. A lambda-binder
fact carries `(core_var, classifier, source_lambda_operation,
source_ast_binder)`, and a source `VAR` operation consumes it only when its
`referenced_ast_binder_id` matches. Declaration facts remain unscoped.
This means that source occurrence identity, not canonical core-var identity,
selects a binder premise. The same occurrence discipline applies to lambda
bodies, compiler-generated `BIND` binders, and thunked operation
continuations. The source ANF elaborator is the only lowering path.

### Current Cutover Findings

The direct ANF elaborator is now the active source path for ordinary lambda
and application. It emits `THUNK(LAMBDA(...))` for a source function and
uses `FORCE`, `APP`, and compiler-introduced `BIND` for ordinary calls. A
constructor-headed application remains a value spine. This has exposed two
non-negotiable occurrence-scoping requirements:

1. A shared `RETURN(VAR)` cannot be inferred once from the erased TermDB
   child. `identityInt` and `identityText` share that core node but require
   separate `RETURN_INTRO` premises. The kernel helper now accepts an
   occurrence-selected child classifier; the source operation solver applies
   that one helper for each `RETURN`, `THUNK`, or `FORCE` occurrence.
2. The old direct guarded-IH solver recognizes only a branch whose immediate
   operation is `INDUCTION_HYPOTHESIS`. Under direct CBPV an IH denotes the
   recursive computation `M(rest) : Comp(E, A)`, not its result value. When a
   source occurrence needs that result as a value, ANF elaboration must create
   `BIND(M(rest), x, ...)`; the IH can consequently occur below `RETURN`,
   `THUNK`, `LAMBDA`, `APP`, and `BIND`. The solver must follow this scoped
   operation dependency and then materialize the same finite motive graph.
   Reintroducing a raw direct-style branch, or treating `*rest` as an
   unsequenced value, merely to preserve the old recognizer is prohibited.

The first rule already restores independently typed shared identities. The
second is the remaining blocker for `add`, `append`, and multi-step `*rest`
examples; it is a solver migration, not a new TermDB representation.

The first solver migration step is now active. `RETURN`, `THUNK`, and `FORCE`
are classified by one kernel helper,
`prototype_judgement_cbpv_boundary_classifier`. The operation solver invokes
that helper as a constraint rule; JudgementDB invokes the same helper when it
materializes the corresponding proof. A boundary is therefore no longer typed
only after the operation fixed point has ended.

For a guarded recursive Match, a compatible non-recursive branch can seed the
solver equation `M(_) == T`. This makes an IH occurrence immediately usable as
`M(rest) == T` for propagation through CBPV wrappers, but does not create a
provisional `APP(M, rest)` in TermDB. The actual motive lambda and Match
classifier are materialized only after every branch equation has been checked.
This restores the current `add`, `append`, and `double` examples without
reintroducing a direct-style recursive branch.

`perform` lowers its host-operation prefix through ordinary shared `APP` nodes
and constructs `OPERATION_REQUEST(prefix, argument, THUNK(LAMBDA(...)))`
directly. Its compiler-generated continuation is represented as an operation
occurrence with an explicit result-binder classifier. `handle` uses the same
thunked-continuation contract and occurrence-scoped premises.

This is an elaboration change, not a second graph. `LAMBDA`, `APP`, and
`MATCH` retain one canonical TermDB implementation, one substitution rule,
and one normalizer. The only duplicated information is the justified
classifier/proof for a source occurrence in JudgementDB. The former
direct-style source lowering and its disabled compatibility implementation have
been deleted; future work must extend this representation rather than add a
second lowering path.

The migration removes direct host operation evaluation. It does not retain a
compatibility semantics where the same operation node can either call C
directly or be handled.

The former `prototype_term_evaluate` convenience entry point was removed. It
combined ordinary reduction with direct pure-intrinsic execution and therefore
provided a second operational interpretation for operation-headed `APP`.
Runtime execution now enters through `prototype_term_perform_with_options` or
the default host handler; conversion and classifier normalization never do.

## Unified Core

There is one graph syntax in TermDB and three classifier-derived categories.

```text
Value:       term : A
Computation: term : Comp(E, A)
Type:        term : Universe(u)
```

The graph constructors are shared:

```text
LAMBDA(binder, body)
APP(function, argument)
MATCH(scrutinee, cases)
```

Their admissible use is checked by the classifier-derived category. For
example, the same `APP` payload may construct a type-family application only
when its classifier is pure, while an application classified as `Comp(E, A)`
is evaluated only by a computation evaluator. This is the CBPV elaboration
boundary, not a second implementation of application.

The new graph forms are:

```text
RETURN(value)
THUNK(computation)
FORCE(value)
OPERATION_REQUEST(operation, argument_spine, continuation)
HANDLER(operation, return_clause, operation_clause)
HANDLE(handler, computation)
HANDLER_TYPE(operation, input_computation, output_computation)
```

The currently accepted source forms are:

```text
let x <- M in N
perform (op argument)
handle (M) with (op) argument continuation => operation_body ; return value => return_body
```

`let` lowers directly to `BIND(M, x, N)`. `perform` requires one explicit
application in parentheses and lowers to `OPERATION_REQUEST(op, argument,
lambda result => return result)`. It does not call a host oracle while
lowering or during kernel conversion.

The current AST operation-metadata solver predates computation constraints.
After it commits the source `APP`/`LAMBDA`/`MATCH` derivations, the shared
`JudgementDelta` computation pass derives `RETURN`/`THUNK`/`FORCE` facts and
solves `BIND` and request constraints to a fixed point. It deliberately does
not re-run global type formation, because that would introduce competing
fresh-Universe derivations for already classified source nodes. Full source
compilation now publishes and validates `BIND_INTRO` and
`OPERATION_REQUEST_INTRO` proofs.

`APP_ELIM` and `LAMBDA_INTRO` are also single-sourced in
`JudgementDelta`. The older JudgementDB-facing expansion entry points now
create a temporary delta, invoke the same shared rule, and commit the result.
They no longer retain a second candidate-selection or lambda-premise
algorithm. The remaining source-specific solver is limited to generating and
solving Match motive, pattern-binder, and guarded-IH constraints. Its current
classifier-binding representation must be replaced by shared constraints
before the migration can claim one fixed-point solver.

The default WHNF and normalization-equality modes never execute host
operations. `PROTOTYPE_TERM_EVALUATE_DEFAULT` evaluates an operation only
after an `OPERATION_REQUEST` reaches runtime dispatch; an empty effect row
does not make a host implementation a beta-reduction rule. Thus type
conversion and ordinary graph normalization cannot accidentally depend on the
host operation table.

The default REPL runtime now dispatches a host operation only from an
`OPERATION_REQUEST`. A direct application such as `#.print #"x"` or
`((#.int_add 1) 2)` remains a graph application and does not execute a host
implementation. The request dispatcher performs the selected host operation
outside TermDB and resumes the request continuation with its result. All host
operations, including deterministic arithmetic, have a result classifier of
the form `Comp(E, A)`; arithmetic presently uses `E = {}`. `HANDLER` and
`HANDLE` are now static TermDB
nodes: a matching request applies the operation clause to its argument and a
deep continuation re-wrapped by the same handler; a `RETURN` applies the
return clause; an unmatched request propagates with that wrapped continuation.
Handler stacks, capabilities, and external state remain runtime data, not
TermDB nodes. The classifier solver now supports a closed one-operation
handler with a closed residual row:

```text
Handler(op, Comp(Eop union Eres, A), Comp(Eout union Eres, B))
```

Its return clause has type `A -> Comp(Eout, B)` and its operation clause has
type `P -> ThunkType(R -> Comp(Eout union Eres, B)) -> Comp(Eout union Eres, B)`,
where `op : P -> Comp(Eop, R)`. `HANDLE` accepts `Comp(Eop union Eres, A)` and has
type `Comp(Eout union Eres, B)`. For closed labels, `Eres` is solved by bitset
subtraction. Multiple operations are handled by nesting. Symbolic residual
rows are represented in TermDB and retained as JudgementDelta equations. The
current solver deliberately leaves them unresolved until it has a unique
substitution; the compiler does not guess them.
The surface handler clauses bind names without surface type annotations. Their
types are supplied by the closed handler constraint: `argument : P`,
`continuation : ThunkType(R -> Comp(Eout union Eres, B))`, and `value : A`. Lowering still creates
only ordinary `LAMBDA` nodes for those binders; it adds no typed lambda tag or
handler-specific evaluator.

### Request Continuation Boundary

`OPERATION_REQUEST` stores its third child as a thunked lambda. Direct-style
source application lowers a function variable through `FORCE`, so a handler
clause that receives its continuation as a normal source variable receives the
same function-value representation as every other function:

```text
OPERATION_REQUEST(op, argument, THUNK(LAMBDA(result, computation)))
```

The corresponding handler binder is
`ThunkType(Pi(result_type, handled_computation))`. This is not a special
handler application rule: source `k x` lowers through the ordinary
`APP(FORCE(k), x)` path.

This was changed as one atomic migration across all of the following:

1. `perform` lowering and the OperationGraph premise for the thunked
   continuation.
2. `OPERATION_REQUEST_INTRO` and handler constraint derivation.
3. request/bind/handle evaluator reductions, including continuation
   composition and deep-handler rewrapping.
4. proof validation and artifact round trips.

There is no raw-continuation compatibility path. `prototype_term_operation_request`
rejects a raw lambda continuation, so an invalid request cannot enter TermDB.
The boundary test verifies both thunked request construction and thunked
continuation composition through `BIND`.

Runtime request dispatch is now an explicit callback in
`prototype_term_reduction_options`. A callback receives an operation, its
argument, and the ambient evaluator state, then either returns a result,
leaves the request unhandled, or reports failure. The current host table is
only the default callback selected by the REPL execution mode. This keeps
handler stacks and capabilities outside TermDB. `HANDLE` uses the same
evaluator before this callback boundary, so it intercepts requests without
another evaluator or another APP form.

The built-in table is split at this boundary. `prototype_operation_declaration`
contains the language-level source key, arity, argument classifiers, result
classifier, and effect row. Typing consults only that declaration. The private
host implementation map contains the C oracle kind and is consulted only by
the default runtime callback. Artifact identity is the source key, not either
the declaration array position or the implementation oracle. A future library
or target backend supplies another implementation map without changing the
operation classifier.

`BIND` is a generic computation node. It is not a second application or
lambda representation:

```text
let x <- computation in body
```

`BIND(computation, x, body)` sequences `RETURN` or `OPERATION_REQUEST` with
its continuation. This is necessary to ensure that an operation executes once
before its result is used. Beta substitution alone would duplicate an
operation in:

```text
(lambda x => add x x) (perform read)
```

The first implemented reduction is:

```text
BIND(RETURN(value), x, body) --> body[value/x]
```

The request-continuation rule is added with `OPERATION_REQUEST`; it is the
same graph rule, not a parallel evaluator.

`THUNK` has a genuine classifier rather than a special evaluator convention:

```text
THUNK_TYPE(Comp(E, A)) = U(Comp(E, A))
thunk M                 : THUNK_TYPE(Comp(E, A))
force (thunk M)         : Comp(E, A)
```

`THUNK_TYPE` is a type former. It does not duplicate `LAMBDA`, `APP`, or
`MATCH`; it only expresses the classifier of a suspended computation.

## Pure Pi Families Without a Second Lambda

A Pi codomain is a value-level thunk of an effect-empty computation function
returning a type code, rather than a raw `LAMBDA` term:

```text
family : U(A -> Comp({}, Universe))
Pi(A, family)
```

Conceptually, a source family:

```text
lambda x : A => TypeBody(x)
```

lowers to:

```text
thunk(lambda x => return TypeBodyCode(x))
```

The constructor validates this exact graph shape and all Pi consumers obtain
the enclosed lambda through one accessor. Pi formation additionally derives
the wrapped `RETURN` and `THUNK` classifiers: the family must have a thunk
type whose computation is `Comp({}, A -> Universe)`. It may symbolically
normalize beta/iota steps under the pure profile, but it may not dispatch an
operation request.

Match motive construction must materialize the same
`THUNK(LAMBDA(..., RETURN(...)))` pure-family wrapper and record its
`RETURN_INTRO` and `THUNK_INTRO` facts.
The existing motive application uses the shared pure-family accessor to obtain
that lambda; this is the same structural bridge used by Pi consumers, not a
second type-level application tag. Full dependent sequencing can later force
and bind the family through the shared CBPV rule. There is no separate
`TYPE_LAMBDA`, `TYPE_APP`, or `TYPE_MATCH` tag.

### Atomic Pi/Motive Cutover

The Pi cutover was not a local `term.c` representation edit. It required the
following consumers to move together:

- Pi inspection now goes through `prototype_term_pure_family_lambda` in the
  typing kernel, AST application classifier, and type representation key.
- Artifact append/import reconstructs dependent constructor classifiers and
  relocates imported instances under `RETURN`, `THUNK`, `FORCE`,
  `COMPUTATION_TYPE`, and `THUNK_TYPE` as well as the pre-existing nodes.
- Type representation equality unwraps the pure Pi family before comparing its
  alpha-bound lambda body.

The shared `RETURN`, `THUNK`, and `FORCE` nodes participate in type-code shape
keys and artifact relocation. The remaining work is motive/sequencing and the
effectful runtime cutover, not a second graph implementation.

This design deliberately moves purity into the type system. A thunked
computation that has type `Comp({Terminal}, Universe)` cannot be a Pi
codomain family. An effect-free family is accepted only after its
effect-empty classifier has been derived.

## Typed Interfaces

Use closed effect rows at first:

```text
return : A -> Comp({}, A)

op : Operation(Eop, (x1 : A1) -> ... -> (xn : An) -> R(x1, ..., xn))
perform op x1 ... xn : Comp(Eop, R(x1, ..., xn))

handle : Handler(Handled, Residual, Input, Output)
      -> Comp(Handled union Residual, Input)
      -> Comp(Residual, Output)
```

For a deep one-shot handler, an operation clause returning `R` receives:

```text
k : R -> Comp(Residual, Output)
```

The existing `EFFECT_LABEL` bit set is sufficient for initial closed rows.
The present `COMPUTATION_TYPE(label, result)` has been renamed from
`EFFECT_TYPE`. It is now uniform for the host operation declarations: both
terminal IO and deterministic arithmetic return `Comp(E, A)`, including
`Comp({}, A)`. A direct application is still only graph structure; `perform`
is required to create the runtime request.

### Effect Row Generalization

`EFFECT_LABEL(unsigned)` is a closed-row literal, not the final effect-row
language. It remains the compact representation of a fully known runtime
capability set. TermDB now also has:

```text
EFFECT_ROW_UNION(left, right)
EFFECT_ROW_VAR(binder)
```

There is still one `COMPUTATION_TYPE(row, result)` constructor. The classifier
view must expose the row *term*, not only an unsigned bit set. A helper may
recover bits only when a row normalizes to a closed `EFFECT_LABEL`; that helper
is for runtime capability checking and never for kernel conversion.
The pure normalizer also reduces a fully closed `EFFECT_ROW_UNION` to its
canonical `EFFECT_LABEL`, so artifact readback cannot make effect equality
depend on whether a closed row was serialized as a union or a literal.

The computation solver derives closed-row instances of these equations while
the row-constraint layer retains the general form separately from term
classifier equations:

```text
BIND(M, x, N):       Eresult = Em union En
OPERATION_REQUEST:   Eresult = Eoperation union Econtinuation
HANDLE(op, M):       Ein = Eop union Eres
                      Eresult = Eout union Eres
```

The current implementation stores these equations in `JudgementDelta`.
It normalizes unions of closed rows and solves a closed handler residual by
bitset subtraction. A symbolic handler residual receives an
`EFFECT_ROW_VAR` and a retained residual equation; it is deliberately not
replaced with a bit set. Until the row solver has a unique substitution rule,
the enclosing handler remains unclassified rather than accepting an invented
effect. No surface row-variable syntax is required for that first step: row
variables are introduced by typed handler and function-family constraints.
This preserves one TermDB representation while making the closed handler a
special case of the general rule.

## Required Judgements

The current relation remains the one authoritative typing relation:

```text
HAS_TYPE(term, classifier)
```

It must not be duplicated into parallel value and computation databases.
Instead, the kernel normalizes `classifier` with the conversion profile and
uses one classifier-view operation:

```text
classifier view = Value(A) | Computation(E, A) | Type(Universe(u))
```

This view is the single boundary for typing, evaluation, and pure-family
validation. The current prototype exposes it as
`prototype_judgement_classifier_view`; the raw TermDB helper only recognizes
the already exposed outer constructor.

Required introduction/elimination evidence:

```text
RETURN_INTRO
THUNK_INTRO
FORCE_ELIM
LAMBDA_INTRO
APP_ELIM
MATCH_ELIM
OPERATION_REQUEST_INTRO
HANDLE_ELIM
```

`APP_ELIM` consults the classifier view rather than a second `APP` tag. The
source elaborator decides whether the shared `APP` is value-level family
application or computation application; the latter may be evaluated only
when its classifier view is `Computation(E, A)`.

Dependent sequencing is the dCBPV+ boundary. The body is checked with a local
result value `x : A`, while the exported classifier is a family over a thunk
value `z : Thunk(Comp(E, A))`:

```text
z : Thunk(Comp(E, A)) |- B(z) computation type
M : Comp(E, A)
x : A |- N : B(thunk (return x))
---------------------------------------------
let x <- M in N : B(thunk M)
```

The constraint solver retains the scoped family `B(z)` symbolically; it never
runs `M` during type synthesis. `B(z)` denotes the whole output computation
classifier, including its effect row, rather than merely the value result
inside `Comp(E, _)`. The solver verifies that this output row obeys the input /
body row rule before it publishes `B(thunk M)`. `x` is a local proof/checking
binder only. It must not escape into a global classifier.

### Dependent Bind Admission Policy

The solver must not manufacture a family by replacing every occurrence of
`x` with `FORCE(z)`. That would make the type family inspect an arbitrary
effectful computation and would turn type conversion into evaluation. A bind
result is admitted only by one of these rules:

1. The body result classifier does not mention `x`. This is the existing
   non-dependent rule.
2. The input has an empty effect row and pure WHNF `RETURN(v)`. The solver may
   substitute `v` for `x` in the body classifier. This is ordinary pure CBPV
   beta reduction, not dependent Kleisli extension.
3. A scoped family `B(z)` is already supplied or solved as a type constraint,
   and the body classifier is convertible to `B(THUNK(RETURN(x)))`. The
   exported classifier is then `B(THUNK(M))`.

Case 3 is the dCBPV+ boundary. It needs an explicit family constraint or a
restricted unification algorithm; it is not justified by a syntactic
substitution heuristic. In particular, it is not enabled by default for
`Terminal`, state, input, or other effects whose execution can refine what is
known. Enabling it for an effect requires an effect-specific refinement or
subtyping policy and a subject-reduction argument.

The current surface grammar has no annotation position that supplies this
family. The implementation must therefore not invent a hidden syntax, infer it
by replacing `x` with `FORCE(z)`, or silently accept an effectful dependent
bind. The next solver API may store an explicitly supplied family, but source
elaboration will connect it only after a reviewed surface form or a restricted
family-synthesis rule has been specified.

### Computation Constraint Boundary

`BIND` must not be inferred by adding a classifier directly to an operation
node. The compiler first records a computation constraint containing the
input computation classifier, the local result binder, and the body operation.
The solver then derives the body under the binder assumption and emits one
`BIND_INTRO` proof only after it can construct:

```text
input : Comp(E, A)
body  : B(thunk (return x))
result: B(thunk M)
```

For the first closed-row implementation, `E union F` is bitwise union of
`EFFECT_LABEL` values. A dependent result remains a graph term with `x` free;
it is not evaluated while solving. This constraint is also the sole entry
point for a future `OPERATION_REQUEST_INTRO` rule. It prevents the old
operation fixed point from becoming a second, effect-specific type checker.

## Codebase Changes

### `src/prototype/term.h` and `src/prototype/term.c`

- Add `RETURN`, `THUNK`, `FORCE`, `OPERATION_REQUEST`, and `HANDLE` tags.
- Keep one generic `LAMBDA`, `APP`, and `MATCH` payload. Do not add
  `TYPE_LAMBDA`, `TYPE_APP`, `TYPE_MATCH`, `VALUE_APP`, or
  `COMPUTATION_APP` tags. Their admissibility follows from the classifier
  view and the operation-level typing derivation.
- Change `PI` to hold a thunked pure family value rather than requiring an
  ordinary `LAMBDA` codomain family.
- Update structural equality, canonical keys, free-binder traversal,
  substitution, clone/rebuild, printing, and normalization-cache invalidation
  for all new nodes.
- Replace the present normalizer profiles with at least:

  ```text
  PURE_TYPE_WHNF
  COMPUTATION_WHNF
  RUNTIME_EVALUATION
  ```

  Only `PURE_TYPE_WHNF` is usable in kernel conversion. It rejects or stops
  at effectful computation; it never calls host callbacks.
- Keep host execution behind the runtime request callback. It must not be
  reached from an `APP` normalizer case.

### `src/prototype/ast.h`, `src/prototype/reader.c`, and `src/prototype/ast.c`

- Add explicit source syntax for `return`, `perform`, `thunk`, `force`,
  `handle`, and `let x <- computation in body`.
- Split lowering into value and computation entry points. A value is accepted
  as a computation only through explicit `return` until diagnostics are
  stable.
- Preserve a source lambda as the one core `LAMBDA` node. When a lambda is
  used as a Pi codomain family, construct the classifier-level pure-family
  wrapper `THUNK(LAMBDA(..., RETURN(...)))`; do not introduce a second source or
  TermDB lambda representation.
- Lower handler operation and return clauses through the same source-operation
  path as ordinary lambda bodies. A handler clause must not fall back to raw
  TermDB lowering, or its nested `RETURN`/`BIND`/`HANDLE` facts disappear from
  the shared classifier fixed point.
- Replace the former host-oracle system-name branch with an operation
  declaration reference. `#.int_add` and `#.print` are values; `perform` creates
  requests.
- Construct Pi families and Match motives as thunks of empty-effect
  computation functions. Existing direct guarded IH motive materialization
  must use this representation, not a raw classifier lambda.
- The operation/source-occurrence graph remains useful, but each operation
  must record whether it produces a value or a computation.

### `src/prototype/judgement.h` and `src/prototype/typing.c`

- Add `RETURN_INTRO`, `THUNK_INTRO`, and `FORCE_ELIM` proof validation while
  retaining one `HAS_TYPE` judgement relation. Replace proof validation that
  assumes all `LAMBDA/APP/MATCH` subjects have one operational category with
  classifier-view checks.
- Rework Pi formation: its codomain family is checked as a thunked pure
  function returning a Universe value.
- Rework Match motive synthesis to produce the same pure-family form.
- Generalize the current classifier fixed point into constraint generation
  plus solving before dependent sequencing and general recursive motives are
  claimed complete.

### Artifact, runtime, and target backends

- Update all `src/prototype/ast.c` artifact mark/read/validate/relocate
  switches for the new tags and bump the artifact format. Old artifacts are
  rejected.
- Add `src/prototype/runtime.c` and `src/prototype/runtime.h`. Runtime owns
  handler stack, continuation frames, capability checks, and IO state; none
  belong to TermDB.
- The REPL installs a default system handler bundle. C and Verilog backends
  lower operation identities to target implementations after handlers are
  selected.
- Operation identity must be stable across artifact boundaries. A host table
  index is not an artifact identity.

## Migration Sequence and Gates

### Phase 0: semantic contract

Fix deep one-shot handlers, closed initial rows, operation identity, and the
pure-family rule. Specify `text_to_nat` failure: its current host oracle is
partial and has a cutoff, so it cannot be a total pure kernel reduction.

### Phase 1: CBPV core without operations

Retain one core `LAMBDA/APP/MATCH` syntax, but replace direct-style source
elaboration with `THUNK(LAMBDA(..., RETURN(...)))` for function values and
`APP(FORCE(...), ...)` for calls. Classify every resulting occurrence through
value/computation judgements; move Pi and motive representation to the same
pure thunk shape. The old `THUNK(RETURN(LAMBDA))` representation is rejected.
`RETURN`, `THUNK`, and `FORCE` are already available, so the gate is their
uniform use, not the presence of extra tags.

Gate: `identityBool` and `identityNat` share the
`THUNK(LAMBDA(..., RETURN(...)))` computation skeleton; `List Nat`, append,
direct guarded IH, artifacts, and all current pure examples pass. Kernel
conversion rejects an effectful family.

### Phase 2: operation requests and sequencing

Move the current host declaration table behind a stable operation-declaration
interface. `OPERATION_REQUEST` and `let x <- perform ... in ...` are already
lowered; kernel WHNF stops at the request. The remaining work is to remove the
host-only declaration table as the type authority so artifacts and target
backends share one declaration interface.

Gate: no operation occurs during kernel conversion; the duplication test
executes one read for one bind, not one read per variable occurrence.

### Phase 3: handlers and runtime

Add `HANDLE`, handler typing, and runtime handler dispatch. Route `#.Int.*`
and `#.Terminal.print` through the same request path. Direct host
normalization is removed; the remaining host oracle kind is runtime callback
metadata and must not be consulted by conversion.

Gate: local handler interception, unhandled-operation diagnostics, default
REPL handler, and unchanged kernel conversion.

### Phase 4: dependent sequencing, solver, artifact cutover

Implement symbolic dependent sequencing constraints, general guarded motive
solving, and artifact serialization for dependent computation families.

Gate: dependent result families after a bind are accepted without evaluating
the operation; artifacts preserve operation identity and handler graphs.

The generic effectful core intentionally stops before this gate. A `BIND` body
classifier may mention its local result binder, so attaching that classifier
directly to `BIND` would leak a free binder into a global judgement. The
solver must first retain a scoped family whose public argument is
`z : Thunk(Comp(E, A))`, prove the body under the corresponding local value
assumption, and then publish the family application to `THUNK(M)`. The
initial closed-row subset may accept only binder-independent body results, but
that restriction must be explicit; it must not be disguised as a general
dependent `BIND` rule.

The implementation order is deliberately narrower than a general dependent
Kleisli extension: first admit the pure `RETURN(v)` rule above, then store a
`B(z)` equation in the computation-constraint layer, and only then expose a
source-level way to provide or solve that family. No new TermDB application,
lambda, or type-family tag is needed at any stage.

The implementation records `BIND` and `OPERATION_REQUEST` constraints in
`JudgementDelta` during classifier inference. Source-operation inference and
CBPV constraint solving run as one alternating fixed point: a `BIND_INTRO`,
`RETURN_INTRO`, `HANDLE_ELIM`, or related proof is first derived over the
shared core term, then its premises are matched back to the source operation
occurrence. This does not create a second evaluator or a typed `APP`/`LAMBDA`
tree.

The closed result subset is now solved and proved: when the synthesized body
result classifier has no free occurrence of the bind-result variable, the
solver derives `BIND_INTRO` with the union of the two closed effect rows.
It also admits the pure beta case: if the input has empty effects and its
pure WHNF is `RETURN(v)`, the solver substitutes `v` into the body result
classifier before publishing the `BIND` classifier. For example,
`let x <- return T in return (lambda y : x => y)` publishes a classifier in
which the local `x` has been replaced by `T`.

A non-pure result classifier containing the bind-result variable remains an
unresolved constraint. It is not silently generalized, reduced, or treated
as the result of a host effect. The next implementation slice must represent
the explicit/synthesized scoped family `B(z)` described above.

Computation constraints are persistent entries in the compile-local
`JudgementDelta`, keyed by their immutable TermDB subject. Re-running the
source/CBPV fixed point may add facts, but must not recreate and erase
constraint-local state. This is necessary for a future symbolic `B(z)` family
and for symbolic effect-row solutions.

The same solver derives `OPERATION_REQUEST_INTRO` only after the shared
`APP(operation, argument)` is classified as `Comp(Eop, A)`. It checks a
thunked lambda from `A` to a closed computation classifier
and unions both effect rows. An empty-effect operation application such as
`((#.int64_add #1) #2)` is still only a graph application until `perform`
constructs its request; it has classifier `Comp({}, Int64)` rather than a
special direct-evaluation type.

## Invariants

1. There is one Lambda/App/Match computation graph implementation.
2. TermDB contains static syntax only, never handler stacks, environments, or
   external state.
3. A type family is a thunked computation only when its effect row is empty
   and its result is a Universe value.
4. Kernel conversion evaluates only this verified pure fragment.
5. User proofs and user handlers never extend global DefEq.
6. Every system operation has exactly one runtime interpretation path after
   handler selection.

## Feasibility

This is implementable in `src/prototype/`, but Phase 1 is a genuine rewrite of
the current typing boundary. The necessary centralization already exists:
TermDB tag dispatch and normalization live in `term.c`; source lowering and
the classifier fixed point live in `ast.c`; proof forms live in `judgement.h`
and `typing.c`; artifact traversal is centralized in `ast.c`.

The main technical risk is not graph representation. It is proving the
empty-effect purity of Pi codomain and motive thunks without reintroducing
host evaluation into conversion. That is why operations are not introduced
until the pure CBPV core is verified.

## Implementation Correction: One Elaboration, Not Local Tag Rewrites

The direct-style examples now use the CBPV lowering path. The original
migration hazard remains relevant: changing only the `LAMBDA` branch to emit
`THUNK(LAMBDA(...))`, or only the arrow branch to emit
`ThunkType(Pi(...))`. A raw source `APP` would then attempt to apply the
thunk value directly. Nested calls additionally require a `BIND` before their
intermediate computation result can be used as the next function value.

The required replacement is therefore one polarity-aware elaborator, not
separate repairs in the `APP`, `LAMBDA`, `MATCH`, and type-expression paths.
It must lower each source occurrence once with an explicit requested result
polarity and share the operation occurrence/proof edge produced by that
lowering. It is responsible for every inserted `RETURN`, `FORCE`, `THUNK`,
and `BIND`. The TermDB and the normalizer remain shared.

There is also no need for a type-only lambda tag. `PI` already needs an
explicit dependent codomain family. The canonical TermDB encoding of that
family is `THUNK(LAMBDA(binder, RETURN(classifier_body)))`. The nested
`THUNK/LAMBDA/RETURN` here represents a binder family; it is not a second
runtime `Comp({}, Pi(...))` wrapper and it does not introduce a second
evaluator. `prototype_term_pure_family` and its validated accessor remain the
single representation for this purpose.

The next code change must consequently introduce an internal elaboration
result that records `Value` or `Computation` polarity together with the one
source-operation occurrence. Only after scoped operation premises replace the
current global lambda candidate scan may source lambdas and calls be switched
to this elaborator. This order avoids both duplicated `APP` rules and the
current shared-core classifier cross-product.

The first prerequisite is now represented in the compiler: a pending source
lambda binder assumption carries the source lambda operation ID in its proof
context auxiliary field. Generic TermDB-only lambda inference ignores those
occurrence-scoped assumptions; the source operation solver is responsible for
combining that lambda's binder classifier with that lambda occurrence's body
operation. The core lambda node and the binder variable remain shared and
canonical. Only the derivation provenance is occurrence-local.

Classifier choice is likewise solver-owned. Integer literals are represented
by one canonical TermDB node and may have `Int` or `Int64` typing facts. The
Pi-domain application constraint specializes the *source operation occurrence*
when its domain is one of those primitive integer types. This works after an
inner application has been solved, so nested applications do not require a
second lowering-time type pass. It is deliberately not a conversion between
`Int` and `Int64`, and it does not change the shared literal node.

The remaining direct-style failure is intentional evidence for the next
elaboration step, not a solver conversion to add. In
`#.int_add (#.int_add #1 #2) #3`, the inner application synthesizes
`Comp({}, Int)`, while the outer operation requires a value `Int`. The only
correct CBPV lowering is `BIND(inner, x, APP(FORCE(int_add), x))` (with the
second argument sequenced similarly when necessary). Treating `Comp({}, Int)`
as convertible to `Int` would erase the value/computation boundary and make
effectful nested calls unsound. This example is therefore a required
acceptance test for the polarity-aware elaborator, not a reason to restore
the old direct evaluator typing rule.

Pure host intrinsics are also dispatched only from `OPERATION_REQUEST`.
Their empty effect row permits the runtime handler to execute them without an
external capability, but never permits a bare operation-headed `APP` to
reduce. The kernel's three normalization profiles never dispatch host
operations. A bare `APP(print, text)` and a bare `APP(int_add, 1)` therefore
remain graph applications; `OPERATION_REQUEST` is the sole runtime boundary.
This keeps deterministic arithmetic usable without turning the host ABI into a
DefEq rule.

## Elaboration Algorithm

The direct-style source replacement is a single CPS/ANF elaborator, not an
after-the-fact graph rewrite. Its public result is always a computation:

```text
lower_computation(e) = lower_value(e, \v. RETURN(v))
```

`lower_value(e, k)` does not return a separate value graph. It calls `k` with
a TermDB value occurrence; `k` constructs the surrounding computation. For a
computation-producing source form it constructs one `BIND` and continues with
the fresh binder variable.

```text
lower_value(literal, k)       = k(literal)
lower_value(lambda x. e, k)   = lower_computation(e) |> c =>
                                k(THUNK(LAMBDA(x, c)))
lower_value(f a, k)           = lower_value(f, \fv. lower_value(a, \av.
                                BIND(APP(FORCE(fv), av), r, k(VAR(r)))))
lower_value(match s cases, k) = lower_value(s, \sv.
                                BIND(MATCH(sv, lower_computation(cases)), r,
                                     k(VAR(r))))
```

A constructor spine is the only application exception: after resolving its
head as a constructor introduction, it is a value and invokes `k` with the
shared `APP(CONSTRUCTOR(...), field)` spine. This is determined at the source
occurrence; TermDB still has one `APP` tag. Named definitions are published as
computations, so `lower_value(name, k)` sequences their published computation
before calling `k`. This makes top-level values, top-level calls, and nested
calls use the same rule and removes the need for a hidden direct-style value
path.

The compiler implementation must represent the continuation in a small
compile-local arena or callback object. It must not construct a second AST or
a persistent second graph. Each emitted node has exactly one source operation
occurrence and uses the existing scoped premise mechanism.

The compiler now carries this distinction in the compile-local `compile_ref`
and in a compiled definition cache as `UNKNOWN`, `VALUE`, or `COMPUTATION`.
It is not serialized and does not change TermDB identity. Explicit CBPV forms
already set the classification; the direct-style elaborator will replace the
remaining `UNKNOWN` source `APP`/`MATCH` paths rather than guess a classifier
from a shared TermDB node.

The elaborator must also distinguish two uses of the one structural `APP`
node. A constructor spine such as `Nat.succ n` is a value introduction and
lowers to `APP(CONSTRUCTOR(...), n)` without `FORCE`. A source function value
such as `f` has a thunk classifier and a call lowers to
`APP(FORCE(f), argument)` as a computation. This distinction is not a second
APP tag or a second normalizer: it is a source-occurrence classification based
on the resolved constructor/type view. Treating every source application as a
forced function call would make ordinary ADT construction impossible; treating
every application as a value spine would reintroduce the direct-style effect
leak. Constructor-spine recognition therefore belongs in the single source
elaboration phase.

## Reading

- Paul Blain Levy, *Call-By-Push-Value: A Functional/Imperative Synthesis*.
- Matthijs Vakar, *A Framework for Dependent Types and Effects* (2015).
- Gordon Plotkin and Matija Pretnar, *Handling Algebraic Effects* (2009).
