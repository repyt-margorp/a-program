# Pointer Core Reimplementation Plan

Date: 2026-09-07
Status: in progress on `rewrite/pointer-core-hott`; N0/N1 incomplete
Predecessor: `2026-08-29T08-31-43-SINGLE-PATH-COMPILER-ARCHITECTURE-IMPLEMENTATION-PLAN.md` (stopped)
Failure record: `2026-09-07-FAILED-SINGLE-PATH-REFACTOR-RECORD.md`
Archived source commit: `5bdecb4` on `archive/2026-09-07-failed-single-path-refactor`
AI implementation location: `src/prototype/pointer/`

Revision: HOTT and dimensional action are foundational from N0/N1, following
the user's 2026-09-07 correction. They are not a feature to bolt on at N6.
Further correction: Core interning uses exact pointer tuples only. Alpha
comparison and normalization are explicit operations, never construction-time
criteria for merging different Lambda or semantic-object references.

## 1. Objective and Source of Decisions

Reimplement A Program around an erased pointer graph with Lambda, Application,
and references to bindings or semantic objects. Preserve the current surface
language and supported behavior; keep CBPV typing, effects, dependencies, and
proof checking above this representation. Typed terms, binders and declaration
objects support dimensional action from their first implementation. This is a fresh implementation, not a
rename of the current TermDB or a pointer facade over its integer IDs.

The user's `src/handmade/` and sibling `a-program-handmade` are read-only design
inputs for AI work. Do not modify, generate code in, or commit to the sibling
repository. Do not promote AI code into accepted directories. Use a separate
prototype build target and executable until replacement acceptance.

Examined handmade inputs:

| Input | Revision / content digest | Observed direction |
| --- | --- | --- |
| `src/handmade/main.c` | SHA256 `faa5a7d0b7128fd60c7417f639e4427de3f76abf6e0c5f16284971b31b97d0e8` | APP child pointers; IADT pointer; separate term/type binding |
| sibling `a-program-handmade/main.c` | repository HEAD `9a78c3db978af82e378aedb06bc8fca0271e85fc`; file SHA256 `87c6fc6be415be7870b5dda678283af1a01d6d0cfa20edfd6fd22281a5815ce2` | generic REFERENCE with object pointer; constructor and Match objects |

GitHub was also checked directly: [repyt-margorp/a-program-handmade at
9a78c3d](https://github.com/repyt-margorp/a-program-handmade/blob/9a78c3db978af82e378aedb06bc8fca0271e85fc/main.c).
Its default branch is `master`. The sibling has an uncommitted addition of the
Match-eliminator sort; distinguish that local extension from the published
source. Both are read-only references, not locations for this implementation.

The boundary to adopt is concrete: APP knows only the two terms it connects;
REFERENCE directs operation to the object that owns its semantics;
`data_type_object` refers to the IADT owner, while `binding` holds term and type
separately. Preserve this ownership in the replacement. Core must not include
IADT field layouts or query the typing solver. IADT owns its constructors and
elimination rules; typing owns admissibility and evidence. This is a direct
in-process pointer boundary, not a message protocol or a duplicated graph.

These are sketches, not a finished evaluator specification. In particular:

- `lambda.index` currently selects an environment position; it contains neither
  an abstraction body nor a binder pointer. Implement actual abstraction and
  lexical reference separately within the proposed representation.
- Application currently extends the environment and evaluates its function;
  lexical closure capture and general beta substitution still need definition.
- The sibling sketch retains stale `TERM_IADT` / `as.type_system` references,
  and gives element constructor and Match eliminator the same sort value.
- Recursive child hashing, fixed capacities, and pointer formatting are sketch
  details, not requirements to reproduce.

No claim is made that three outer node tags alone prove semantic equivalence to
CBPV. The translation and its operational obligations are specified below and
must be tested. Oracle objects still carry real semantics; moving them behind a
pointer does not eliminate those rules.

## 2. Nonnegotiable Design Rules

1. Core evaluation does not consult a classifier, proof context, or TypeView to
   choose the meaning of the same executable node. A reference's descriptor
   already determines its operational meaning.
2. Typed occurrences carry Core references, annotations, context and evidence.
   Two differently typed identities may share Core without sharing their typing
   judgement. Never recover a source classifier by searching all types of Core.
3. Synthesis derives a classifier from source structure and binding declarations.
   `::` checks the result afterward; it never supplies a missing motive/domain.
4. CBPV value/computation distinctions remain typing rules, not duplicate
   Lambda/APP node kinds. Erasure must still preserve sequencing and suspension.
5. Pointers identify live immutable objects. They do not certify typing, prove
   arbitrary equality, or provide portable artifact identities.
6. A budget exhaustion result is pending, not false or accepted. Static
   evaluation uses only the explicitly admitted pure semantics. Host effects
   execute only at an execution boundary.
7. Hash lookup followed by exact pointer-key comparison and allocation is the common
   construction path. Rule dispatch, genuine scope distinctions and errors
   remain explicit. Deleting all conditional operators is not an acceptance test.

## 3. Small Physical Model

Proposed initial modules (headers are declarative; avoid a service layer per DB):

| Module under `src/prototype/pointer/` | Owned information |
| --- | --- |
| `graph.c`, `graph.h` | stable allocation, immutable nodes, interning, binder/object references |
| `eval.c`, `eval.h` | environments, application spines, resumable reduction, pure memoization |
| `reader.c`, `reader.h` | tokens, source spans, AST, lexical names; no solving |
| `typing.c`, `typing.h` | typed occurrences, persistent contexts, constraints, derivation checking |
| `iadt.c`, `iadt.h` | declaration/constructor/Match descriptors, recursive-field schemas |
| `effect.c`, `effect.h` | request, return and fold descriptors; handler execution |
| `dimension.c`, `dimension.h` | dimension maps, boundary diagrams and action, present from N1 |
| `identity.c`, `identity.h` | Identity computation, transport/lifting and evidence, present from N2 |
| `image.c`, `image.h` | program roots and pointer relocation for `.a` |
| `driver.c`, prototype-local build/test files | CLI, REPL, test runner |

Add a module only when it owns a distinct responsibility. Share storage helpers;
do not build a generic database framework before these modules need one.

Core nodes have three forms:

```text
Lambda(binder*, body*)
Application(function*, argument*)
Reference(object*)
```

A lexical variable is a reference to a binder object. Semantic references point
to immutable descriptors for constructors, eliminators, literals or operations.
Use a declared object header/discriminant and checked access, not unchecked
`void *` casts scattered through the evaluator. A descriptor may contain Core
children; graph traversal must enumerate them for substitution and persistence.

Allocate stable chunks: growing an arena cannot move referenced objects. Separate
mutable evaluation frames from immutable nodes. A closure captures a body and
its environment; sharing a term does not share one mutable runtime invocation.
Start with whole-program arena ownership and reclaim temporary frames per run.
Measure before adding fine-grained GC or environment indexes.

Binder objects are allocated before their bodies and are frozen with the binding
scope. Intern Lambda by `(binder*, body*)`, APP by `(function*, argument*)`, and
REFERENCE by `object*`, with the node kind in every key. There is no recursive
inspection of children during interning. Different binder pointers remain
different structural nodes even when the lambdas are alpha-equivalent.

When alpha comparison is explicitly required, use a binder correspondence;
never use its result to merge the input nodes. Do not adopt De Bruijn storage.
Likewise, evaluating `APP(identity, value)` may return `value`, but the original
APP remains a distinct interned node. WHNF/NF equality belongs to explicit
conversion, not allocation. Match/IADT references follow the same rule: distinct
semantic objects are not merged because their computations agree. Reindexing and
capture avoidance still exist with pointers; share unchanged subgraphs and
memoize substitution by term plus immutable environment/mapping.

IADT declarations have generative object identity. Constructors point to their
owner declaration and their field telescope, and Match clauses point to actual
constructor objects. Structurally identical Bool and Two stay distinct. Array
positions/counts and serialized reference numbers are permitted; they are not
the in-memory semantic identity. Nominal creation is not structural interning.

## 4. Reduction and CBPV Correspondence

Lambda beta reduction is shared by all callers. An applied semantic reference
collects its arguments and delegates to its fixed reducer. Reducers return a
reduced graph, a blocked neutral, an effect request, or an error with consumed
steps. Core contains no type-directed fallback path.

| Typed construct | Proposed erased representation / action |
| --- | --- |
| Lambda / APP | the two ordinary Core forms |
| constructor application | constructor reference applied to field values; saturated data is inert |
| Match / IH | eliminator/frame references applied to scrutinee and clause closures; general IADT reducer |
| return / thunk / force | references to fixed structural operations and APP spines |
| computation fold | immutable clause descriptor plus applied computation and return clause |
| pure primitive | fixed reference semantics admitted under a documented conversion policy |
| effect operation | operation reference producing a request; handler or host supplies its result |

The graph has a uniform physical vocabulary but retains the distinctions needed
for execution. Thunk must stop traversal, force must release it, and deep handler
resumption must reinstall the handler. Unhandled operations retain the transformed
continuation. A single reference protocol does not make arbitrary host callbacks
safe for conversion or serializable.

Use the existing CBPV surface elaboration policy as an explicit translation:
insert return in a computation body containing a value; sequence returning
computations where the current language does so; preserve explicit `&`; preserve
callee-force behavior. Choose evaluation order explicitly, including function
position, arguments, constructor fields, and blocks. Do not rely on unrestricted
beta reduction to establish the order of effects.

Before broadening the implementation, show for the initial pure fragment that a
typed reduction and its erased execution agree on the result, and demonstrate
effect order with traces. Record the admitted equations rather than claiming a
general CBPV equivalence theorem from tests alone.

`pg_computation_eval_init` now installs a fixed pure CBPV dispatcher on the
same lexical beta machine. The generic demand protocol suspends an applied
reference, evaluates one selected argument, materializes that WHNF, restores
the caller and resumes its operation. FORCE demands its argument and releases
the body of THUNK. RETURN and THUNK remain inert heads. Unknown arguments leave
FORCE neutral with the inspected argument retained; no repeated demand loop is
needed. Demand frames are included in pending readback. Each frame resumption
is a budgeted machine step, although materialization itself is not yet separately
budgeted. Captured environments are still handled by the shared readback path.

- [x] Pure FORCE/THUNK execution: test inert quoted divergence, released
  divergence, neutral arguments, nested demands, pending readback, external
  application arguments, captured environments and split-budget equivalence.
- [ ] Extend fixed reference semantics to constructor/Match, folds and requests;
  verify effect order and typed action compatibility. The dispatcher is not yet
  a general effect runtime, nor connected to conversion or source execution.

The zero-operation-clause `computation-fold` reference now takes M and a raw
return continuation K. It demands M through the shared evaluator protocol;
`FOLD(RETURN v,K)` applies K to v while preserving lexical closures and any
arguments outside the fold. No separate BIND Core node is added. The first
checked rule requires `M:F A`, `K:Pi(x:A,C)` with C structurally independent of x.
C may be a raw computation Pi, not only a returning computation; the left
operand must still be F A. This is pure CBPV sequencing, not executing raw Pi
as a source of a bound result. Dependence on x is checked by fresh substitution
and explicit alpha comparison, never by modifying interned node identity.

- [x] Check/execute zero-clause pure FOLD and recover its result formation;
  test neutral/divergent input, pending readback and non-value/quoted-continuation
  rejection. Reject a result classifier depending on the bound result in this
  nondependent rule; do not fabricate an effectful type-level result.
- [x] Lower a returning source argument through a fresh typed continuation and
  FOLD. Source fixtures execute `f (g x)`, including a continuation returning
  raw Pi followed by another application. Both computations are synthesized
  before sequencing; no expected type determines their synthesis.
  The curried fixture exposed a hygiene mismatch: separate substitutions
  freshened bound classifier pointers differently. Lambda introduction now
  checks structural alpha equality of its body classifier rather than exact
  pointer identity. No nodes are merged; nonstructural conversion still needs
  explicit evidence. This is a binder-renaming rule, not WHNF interning.
- [x] Sequenced arguments use the same resumable classifier comparison as
  ordinary value arguments. A block returning a quoted identity function can
  be passed to a higher-order function even when its Pi binder pointers differ;
  the continuation's APP retains explicit conversion evidence. A genuinely
  different result type is rejected. Block and argument sequencing share
  continuation opening/closing through the existing checked proof rules.
- [x] Sequence a callee returning a quoted function before its argument.
  Both operands use the same continuation-opening operation; their frames close
  in reverse order to form `FOLD(callee, \f. FOLD(argument, \x. FORCE(f) x))`.
  Tests execute returned/quoted callees, two computed operands and higher-order
  returned functions, inspect this nesting in the proof DAG, and reject
  nonfunction results and mismatched arguments. This checks the pure translation;
  observable effect-order tests still require requests and handlers.
- [ ] Add operation clauses, effect rows/forwarding, dependent sequencing and
  computed classifier normalization. These remain unsupported by this initial path.

Memoization keys include the semantic reference policy and captured environment
when relevant. Never memoize a dispatched effect as if repeated force were pure.
Keep WHNF and NF distinct. Type conversion and object Identity remain distinct.

Implemented `pg_beta_work` shares resumable beta-WHNF requests by input pointer.
Its policy is fixed: every unbound REFERENCE is neutral, including semantic
objects; no oracle dispatch occurs. Each request owns its closure machine and
one materialized answer. Repeated requests resume that machine or return the
existing answer without reduction or readback. Captured environments are local
to the request, never indexed by the current body pointer alone. Completed jobs
release closure storage; their answers live in the output graph. The input and
answer nodes remain distinct. This is request-level memoization, not yet reuse
of arbitrary intermediate closure evaluations.

- [x] Test shared pending requests, split budgets, stable completed answers,
  different captured arguments, nontermination remaining pending and index
  growth. Ordinary and ASan/UBSan checks pass.
- [ ] Connect beta work to typed conversion without interpreting beta-WHNF as
  full semantic WHNF. Add explicit owner reduction policies before semantic
  dispatch; do not silently expand this cache to runtime effects.
- [ ] Make readback itself budgetable before claiming a bound on all work:
  the current budget counts machine transitions, not output graph traversal.

`pointer/conversion.c` adds a separate resumable beta-conversion traversal using
the shared beta jobs. It decomposes normalized Lambda/APP/reference pairs,
tracks the binder correspondence, and memoizes visited pairs with that scope.
The pending stack is work, not an equality certificate. The result can be equal,
different, pending, or allocation error. A same-pointer shortcut is allowed only
outside a binder correspondence. No result is fed into structural interning.

- [x] Explicit beta conversion under lambdas; reject bound/free confusion; test
  alpha-renamed shared DAGs and divergence remaining pending under split fuel.
  Ordinary and ASan/UBSan checks pass.
- [x] Connect beta comparison to typed premises and record conversion evidence.
  Comparison state is private; only a completed equal comparison issues an
  immutable certificate in the program graph arena. A conversion derivation
  requires an existing term derivation, target-type formation in the same
  context and sort, and exact certificate endpoints matching the old and new
  classifiers. It preserves the original occurrence and derivation. Tests
  convert a thunked function between distinct alpha-equivalent Pi classifiers
  and apply it, reject mismatched scopes/sorts/endpoints, and retain the
  certificate after comparison/work-store destruction.
- [ ] Extend admitted comparison semantics beyond beta; attach typed HOTT rules
  and source synthesis. A certificate currently records the verified endpoints
  and fixed beta policy, not a serialized reduction trace. Image loading must
  recompute that check or validate a future retained trace using the same rules;
  it must not trust a certificate reconstructed from endpoint pointers alone.
  `DIFFERENT` currently means different under beta-only neutral-reference
  semantics, not inequality in a future owner-reduction policy or object
  Identity. There are no eta, iota, transport or effect equations in this
  comparator yet. A successful untyped comparison does not establish that
  either input is a well-formed classifier.

## 5. Foundational HOTT Action

Narya sources consulted on 2026-09-07:

- [Observational higher dimensions](https://narya.readthedocs.io/en/latest/observational.html):
  `Id`, `refl`, and `ap` share the higher-dimensional construction; dependent
  identification includes a correspondence along the base identification.
- [HOTT](https://narya.readthedocs.io/en/latest/hott.html): transport and lifting
  supply structure beyond parametricity. The documentation explicitly identifies
  limitations of current computation rules and special difficulties for indexed
  inductive types in its fibrancy construction.
- [Implementation remarks](https://narya.readthedocs.io/en/latest/remarks.html):
  Narya uses De Bruijn indices/levels and intrinsically scoped OCaml structures.
  Adopt its semantic discipline, not those representations.
- [Dimension theory](https://github.com/gwaithimirdain/narya/tree/master/lib/dim),
  [op.ml](https://github.com/gwaithimirdain/narya/blob/master/lib/dim/op.ml),
  [hott.ml](https://github.com/gwaithimirdain/narya/blob/master/lib/dim/hott.ml):
  operators have domain/codomain, act contravariantly, and compose; faces and
  degeneracies are distinct. These moving upstream references must be pinned
  to a commit when implementing the corresponding rules.

The following is the A Program adaptation, not a claim that Narya already
provides a pointer-based CBPV implementation.

**Representation.** From N1, use immutable dimension/operator objects and shared
boundary diagrams whose cells hold term/binder pointers. Start with binary
endpoints and one direction, but no fixed maximum dimension or separate 1D/2D
term tags. Materialize demanded faces instead of eagerly copying every cube.
Dimensions and array sizes may use integers; bound identities remain pointers.

**Two operations to define precisely.** Restriction by a dimensional map and
forming the higher action of a typed term are related but not interchangeable.
For `rho : m -> n`, restriction sends an n-dimensional object to dimension m.
It obeys `restrict(id,t)=t` and
`restrict(sigma,restrict(rho,t))=restrict(rho compose sigma,t)`.
Producing a higher term additionally acts on its context, classifier, and
boundary bindings; raising a dimension counter alone is not a proof.

An internal `Act` operation takes an explicit typed occurrence, dimension map
and boundary environment and produces another typed occurrence plus ordinary
Core. It cannot select an annotation from a naked erased Core pointer. Its
generated executable references contain the chosen immutable descriptors, so
subsequent Core reduction has fixed semantics. A suspended action can itself be
represented through REFERENCE and APP, with no mandatory fourth Core tag.

**Scope.** Acting on a binder produces its boundary binders and center binder in
one persistent scope diagram. Act on Lambda, APP, classifiers and substitutions
together. Check compatibility of iterated restrictions on shared faces and
commutation with substitution/beta. Share the graph traversal machinery, but do
not equate ordinary variable substitution with dimension restriction.

`pg_term_restrict_bindings` connects strict-face restriction of explicitly
listed free boundary bindings to the common term-substitution traversal.
It leaves unlisted references and opaque semantic data untouched. Identity
restriction preserves the original pointer; composed restrictions can produce
distinct alpha-equivalent lambda nodes because capture avoidance freshens
binders. No normalization or alpha interning is used to merge them.

- [x] Term-level fixture checks for identity restriction, a square-to-corner
  composite, beta/restriction commutation and protection of bound variables.
  Ordinary and ASan/UBSan checks pass; these are examples, not a general proof.
- [x] All strict-face entry points now use one validated, interned map path.
  Previously the untyped binding/Term path only counted axes, while the typed
  context path checked coordinate validity. Repeated axes could therefore pass
  the former's face test, and a copied identity map could create a distinct
  boundary binding. Tests require copied maps to reuse the canonical face and
  reject duplicate axes in binding, Term, context and composition entry points.
  Composition validates its maps before substituting coordinates. Existing
  structurally equal maps are retrieved before allocating validation scratch
  space; coordinate interning does not identify terms by reduction or alpha.
- [x] `action.c` builds a checked restriction substitution for an explicitly
  supplied typed boundary telescope. Starting from the empty context, it lifts
  the same substitution through each source declaration using the corresponding
  restricted pointer binder. Consequently later dependent declaration types
  use the earlier restricted variables. NULL binding entries preserve ordinary
  binders. No independent context-rewriting or classifier solver is added.
  Tests compare square-to-edge-to-vertex with direct restriction, including
  dependent classifiers, identity maps and invalid binder/dimension mappings.
- [x] Extend that fixture to checked Lambda/APP derivations over the boundary
  telescope. Restricting an APP agrees up to explicit alpha comparison with
  applying its separately restricted function and argument; their classifiers
  agree and regularity is recovered in the target context. Twice-restricted
  Lambdas agree with direct restriction. Executing the restricted identity
  application yields the independently derived, restricted beta result at the
  same classifier. These tests reuse context substitution/reindex and the
  existing evaluator, without a second action-specific substitution engine.
  They do not construct object Identity witnesses or establish general naturality.
- [ ] Identity-family/context action and restriction inside semantic owners
  remain unimplemented. The helper rejects degeneracies: forming a higher witness
  cannot be replaced by generating another free boundary variable.
  The supplied fixture types are ordinary verified telescopes used to check
  substitution preservation. A cube-associated variable is not thereby a path
  or Identity witness. Generating the actual HOTT boundary types, center types,
  witnesses and transport remains a separate required N2 gate.

**Identity.** Generate identity families and witnesses from this action rather
than adding an unrelated `Obs(left_type,right_type,left,right)` authority.
For heterogeneous identification retain the chosen family/correspondence and
base witness: identical endpoints can admit different identifications. Do not
erase those choices or collapse all evidence into an endpoint pair. Pointer
sharing and kernel conversion do not imply equality reflection.

### N2 implementation contract: families are not returning computations

#### Next HOTT milestone: polarized family formation, not more evaluator prerequisites

Re-audit at `a7c268f` found no acted-family formation rule. The symbolic rules
below now begin that implementation; type-former computation and transport are
still missing. The preceding fuel work alone did not implement Identity.

The next implementation must define the following together. These equations
are an A Program design proposal to validate, not established CBPV-HOTT theorems
or rules already accepted by the kernel. Here `Id_A`/`Id_C` denote the action of
a specified type family, not a global endpoint-only relation.

- [ ] Value-family action forms a value type. Computation-family action forms
  a computation type. This distinction is a judgement, not separate Core
  Lambda/APP constructors. Both retain the chosen family, its typed boundary
  substitutions and the dimension operator.
- [ ] Computational endpoints are retained as checked computational occurrences.
  Whenever an endpoint must enter a value-only context telescope, use its
  explicit thunk and U type. Do not insert raw computation into ContextDB, or
  treat a formation under a telescope as a function returning `F Universe`.
- [ ] Specify and implement the canonical pure rules:

  ```text
  Id_(U C) (THUNK M0) (THUNK M1)  computes to U (Id_C M0 M1)
  Id_(F A) (RETURN v0) (RETURN v1) computes to F (Id_A v0 v1)
  ```

  With heterogeneous boundaries, `C` and `A` above are selected acted families,
  including their lower-dimensional base evidence. They are not inferred from
  two endpoint classifiers. The equations describe classifier computation;
  they do not claim that arbitrary endpoint pairs have witnesses.
- [ ] For neutral `M0,M1 : F A`, retain a neutral computation-family instance
  with those endpoints. Forming it must neither run M0/M1 nor invent returned
  values. Its introduction/elimination and transport laws remain obligations;
  successful formation alone cannot discharge equality or termination.
- [ ] Pi action consumes a value boundary telescope `x0, x1, x01` and returns
  the acted computation codomain. Lambda and APP action must construct/consume
  this same telescope. The constant-family homogeneous equations above are
  not a substitute for dependent codomain instantiation through `x01`.
- [ ] Universe action needs an inhabitant contract containing transport and
  lifting plus their higher action, not only an arbitrary binary relation or
  four unrelated functions. Validate this before introducing a general
  universe-Identity witness constructor. Preserve distinct choices even when
  endpoint types coincide.
- [ ] Add these semantic-family computation rules to a fixed pure conversion
  policy when implemented. The current conversion wrapper admits beta only;
  its generic pair walker is not permission to certify an arbitrary callback's
  answers. Runtime handlers/oracles must not change this policy.

Acceptance examples must cover both polarities, a neutral F endpoint that stays
neutral, a returned endpoint that computes, dependent Pi action, and a second
action retaining the first action's chosen family. Transport/lifting boundary
tests are required before claiming this fragment is HOTT rather than a logical
relation. Indexed-family fibrancy remains a separate unresolved obligation.

Primary references checked again for this milestone:
[Narya observational primitives and heterogeneous Identity](https://narya.readthedocs.io/en/latest/observational.html),
[HOTT transport, lifting, glue and higher bisimulation](https://narya.readthedocs.io/en/latest/hott.html).
The latter explicitly distinguishes HOTT from parametricity and records limits
of implemented transport. Neither page specifies our CBPV F/U adaptation.
These are moving documentation pages, not the pinned Narya source revision.

Rechecked the primary [observational documentation](https://narya.readthedocs.io/en/latest/observational.html#heterogeneous-identity-types)
and [HOTT documentation](https://narya.readthedocs.io/en/latest/hott.html#transport-and-lifting).
In Narya a heterogeneous identification retains the base identification through
the acted family. A universe identification has an underlying correspondence,
but instantiating that correspondence is not ordinary function application.
HOTT adds transport/lifting and higher compatibility; an arbitrary reflexive
correspondence is not thereby an equality. Its indexed-type fibrancy caveat
must not be mistaken for an already implemented generic solution.

The following is our CBPV adaptation, not a Narya theorem or implemented rule:

- A family is first an accepted formation under a boundary context. Given
  `Delta |- A type` and a checked substitution `sigma : Gamma -> Delta`, its
  instance is `Gamma |- A[sigma] type`. Use `pg_prove_substitution` and
  `pg_prove_reindex`; do not represent this operation as obtaining a runtime
  value from `F Universe`. Do not add ValueSidePi to encode this meta-level
  binding. Raw computation Pi retains its existing meaning.
- A higher family request must identify the original formation/occurrence,
  dimension operator, acted context and chosen lower-dimensional family/base
  evidence. The two endpoint types alone are not a sufficient key. A neutral
  semantic reference may eventually retain these immutable inputs, but only
  after its formation rule exists; allocating such a reference is not evidence.
- Materialize the boundary context in dependency order. Each declaration's
  classifier is an instance of an already checked lower face family. The center
  formation must have this entire context as its source. Supplying an arbitrary
  center type of the right universe does not establish that it is the action
  of the original type.
- Keep VALUE_TYPE/COMPUTATION_TYPE judgements distinct without adding Core tags.
  A raw Pi action has computation polarity. Passing its center as an ordinary
  context argument requires the existing U/thunk discipline; an implementation
  must not insert raw computation proofs into a value context. The Id rules for
  F/U remain obligations, not an assumption that all computations have values.
- Derive the classifier of an acted term by instantiating the selected acted
  family with its checked boundary terms. Lambda action must use an acted binder
  telescope; APP action must consume that same telescope, including the center.
  Checking only that corresponding subterms have some relation is insufficient.

Code audit: `pointer/action.c` currently only constructs restriction substitutions.
`pointer/evidence.c` already supplies checked family instantiation by reindexing,
and now the symbolic Identity rules below. Legacy
`src/prototype/src/dimension/action.c` has boundary-applied family/classifier
builders; they return Term IDs rather than the fresh kernel's accepted formation
evidence. Reuse their equations only after verifying the premises, not their
successful return codes as new-kernel certificates.

#### Symbolic Identity formation and selected instantiation

- [x] One immutable `identity-action` reference represents `refl t`; applying
  `refl A` to two endpoints represents homogeneous `Id A x y`. Iteration uses
  that same Core reference, not dimension-specific tags. All source/endpoint
  terms remain ordinary APP operands, visible to substitution and comparison.
- [x] `PG_IDENTITY_FORM` checks an accepted base formation and both endpoint
  derivations in its context. Its output keeps the base polarity and universe
  bound. `PG_REFLEXIVITY` supplies only the diagonal witness with that formation
  as a premise; it cannot choose unrelated endpoints or register DefEq unions.
- [x] `PG_IDENTITY_INSTANCE` checks `R : Id Universe_i A B`, `x : A`, `y : B`
  and forms `R x y : Universe_i`. It keeps R explicitly, rather than replacing
  it with the endpoint pair. Ordinary functions and arbitrary relations are
  not accepted as R. This is not raw Pi application or a new value-side Pi.
- [x] Tests cover distinct choices R/S with identical endpoints, wrong endpoint
  types/scopes, universe bounds, three iterated symbolic diagonal witnesses,
  computation versus value polarity, ordinary reindex, and shared erased
  functions whose differently typed actions retain different classifiers.
- [x] Pure RETURN/THUNK and canonical F/U equations now share evaluator demand
  frames with beta, FORCE and zero-clause FOLD. The reducer inspects reference
  descriptors and Core operands, not classifiers or TypeViews:

  ```text
  refl (RETURN x)                     -> RETURN (refl x)
  refl (THUNK M)                      -> THUNK (refl M)
  Id (F A) (RETURN x) (RETURN y)       -> F (Id A x y)
  Id (U C) (THUNK M) (THUNK N)         -> U (Id C M N)
  ```

  Formation does not run these computations; explicit normalization may expose
  the source and endpoint heads using only the fixed pure policy. Unknown
  references remain neutral. Thunk contents are not demanded for these WHNF
  rules. Arbitrary selected R and noncanonical endpoints remain neutral.
- [x] `pg_whnf_work` replaces the beta-only memo API. Keys include the input
  and immutable policy pointer. Beta and pure results coexist without aliasing;
  conversion explicitly requests the fixed `pg_pure_policy`, never an arbitrary
  caller's dispatcher. Tests cover split budgets, capture, classifier conversion
  before/after action, suspension, neutral endpoints and policy isolation.
  Synchronous readback inside semantic-demand resumption remains a fuel caveat.
- [ ] Compute Pi/Lambda/APP action and general F/U rules on selected
  heterogeneous/higher families. The equations above are not full observational
  Identity. Checked typed reduction jobs still need introduction inversion for
  action proofs; the current tests establish Core reduction and admissible
  classifier conversion, not complete source-level action execution.
- [ ] Generalize selected instantiation to acted boundary telescopes: the
  present instance rule handles a value-universe identification, not arbitrary
  higher-dimensional families. Connect the existing dimension-map operators.
- [ ] Implement transport, lifting, their computations and higher compatibility.
  No arbitrary relation-to-universe-Identity constructor has been added.

The value rules follow Narya's
[Identity of the universe and selected instantiation](https://narya.readthedocs.io/en/latest/observational.html#id-of-the-universe).
Keeping a computation base's Identity/reflexivity on the computation side is
our CBPV extension, not a theorem supplied by that document. The initial symbolic
checkpoint has now been extended by the four explicit equations above. They do not
establish termination, contextual equivalence, general higher coherence, or
the HOTT transport/lifting structure described in the
[HOTT documentation](https://narya.readthedocs.io/en/latest/hott.html).

- [x] Checked telescope pairing is shared with substitution lifting:
  `pg_prove_substitution_pair(sigma, Gamma.x, a)` constructs `(sigma,a)` using
  the existing substitution checker and canonical record. It does not add a
  Core tag, a second substitution authority, or a value-side function type.
  Reindexing with the result instantiates both value and computation families.
  Tests reject skipped dependencies, wrong classifiers, raw computations and
  out-of-context images; repeated pairing reuses the flat substitution result.
- [x] Family-instance regression: two selected formations may instantiate to
  the same Core type without losing their distinct formation premises. A
  subsequent reindex retains that choice and agrees with composed substitution.
  These are ordinary family formations, **not** Identity formation or a
  transport/lifting certificate. `PG_PI_CODOMAIN` still uses its valid direct
  substitution rule; no claim is made that Pi inversion has been migrated.

- [ ] Implement a checked acted-context/center-formation contract using these
  existing proof inputs; test different chosen correspondences with identical
  endpoints remain distinct.
- [ ] Implement the Pi/Lambda/APP center rules together, including F/U polarity
  and exact boundary instantiation; test identity and constant actions followed
  by a second action. A generic relation-preservation test cannot replace this.
- [ ] Supply transport/lifting computations and their boundary tests before
  advertising this as HOTT Identity. Pure type evaluation, generated IADT
  families and their fibrancy obligations remain part of the full goal.

- [x] Initial typed beta reduction derives a reduct from a checked APP whose
  function premise is a Lambda introduction (possibly weakened). It builds
  the ordinary checked substitution from the body context into the application
  context, then reindexes the existing body proof. It preserves the original
  APP and checks the result classifier up to explicit alpha equality. No new
  equality axiom, Core node, or unchecked classifier assignment is introduced.
  Tests cover lexical weakening, invalid contexts/non-APP inputs, an applied
  polymorphic Lambda whose result is another Lambda, and 100 repeated requests
  without Term/evidence growth. Temporary image arrays are still rebuilt.
- [x] `pg_reduce_computation` dispatches checked beta, FORCE/THUNK and
  zero-clause FOLD/RETURN steps. Introduction inversion follows existing
  projection/reindex premises and applies the same context action to their
  contents. FORCE returns the checked computation content; FOLD constructs an
  ordinary checked APP of the continuation to the returned value. Classifiers
  are checked up to alpha equality, without merging Core nodes or manufacturing
  an Identity/normalization certificate. Tests cover direct steps, weakened
  introductions, introductions beneath beta-generated reindex evidence, wrong
  contexts, unsupported converted introductions, and stable Term/evidence
  counts over 100 repeated requests. This does not execute effect operations.
- [x] Typed beta now follows projection/reindex chains on its Lambda premise.
  It maps the original free variables through those existing context actions,
  adds the application argument as the original bound variable's image, and
  reindexes the original body with that checked simultaneous substitution.
  No runtime closure store or regenerated Lambda introduction is needed.
  The typed step also commutes through projection/reindex around an elimination
  proof. Source-synthesized polymorphic identity tests perform two beta steps,
  include interleaved projection and multiple reindex actions, compare with
  independent Core evaluation, and repeat requests without Term/proof growth.
- [ ] General typed reduction must handle converted function proofs,
  converted introductions and admitted semantic owners, plus budgeted reduction chains and
  general pure type-result exposure. `pg_reduce_beta` is only the Lambda-head
  step above; its NULL result is not a normalization or untypability verdict.
  Its reduct is ordinary reindex evidence, not an object Identity witness or a
  persistent conversion certificate for the original APP.
  Introduction inversion currently traverses its immutable premise chain;
  budgeting that traversal and composing arbitrary chains remain unfinished.
- [x] Initial source-level pure type-result exposure uses existing accepted
  derivations: `pg_prove_return_value` inverts RETURN through context actions,
  and the existing value-universe rule checks whether that returned value is a
  type. Lambda/Pi domains, Pi codomains and the right side of `::` share one
  synthesis helper. It retains its current proof between scheduler steps;
  each successful reduction yields before continuing. APP can advance its
  callee and FOLD its input by rebuilding the same checked elimination rules.
  Tests cover computed domains/codomains, nested computed arguments in a
  post-check, a computed callee, rejection of a returned non-type and missing
  subject, and an unsupported neutral type-family application. `::` still
  supplies no information to synthesis of its left-hand side.
- [ ] Neutral/open pure type-family computations require their own checked
  formation contract; reaching `F Universe` alone is not sufficient to invent
  a returned type. Current inability to expose `f A` for an unknown `f` is
  reported unsupported, not proof that dependent families are invalid. The
  checked family/substitution action contract above is still required.
  Demand/context traversal and primitive substitution costs are not yet charged
  individually to fuel.
- [x] Pure returned-value requests now use the existing synthesis job index,
  ready queue, completion status and waiters. The immutable key is the job role
  plus context evidence and input computation evidence; source expression and
  definition requests use the same index with their own input pair. There is
  no second evaluation scheduler or copied authoritative classifier answer.
  Type-position consumers subscribe to the returned-value producer rather
  than retaining independent reduction progress. Zero-budget requests do no
  reduction; a partial request resumes its existing proof; completed requests
  perform no further scheduler steps. Tests cover those cases, two source
  consumers sharing a computed annotation, same-Core inputs in distinct
  contexts, differing input derivations, and rejection of foreign-owner proofs.
- [x] Demanded reduction steps use the same job table and waiters as whole
  returned-value requests. Preparing a step returns either a checked direct
  reduct or a context/input pair for its demanded operand. APP callee, FOLD
  input and projection/reindex demands become shared reduction-job dependencies.
  Resumption rebuilds the original rule with the checked operand derivation.
  This reconstruction is a typing operation, not an equality axiom accepting
  arbitrary replacement evidence. The synchronous `pg_reduce_computation`
  helper delegates to this same prepare/rebuild contract; synthesis no longer
  calls its recursive traversal. Tests put two applications simultaneously on
  the same pending callee job, verify notification and cached step reuse, and
  check the whole application against independent Core execution.
- [ ] Budget traversal inside each primitive step. Introduction inversion,
  context-prefix lookup, beta image construction and term substitution can
  still perform unbudgeted work. Different input derivations are deliberately
  distinct keys even when their Core happens to be shared. Effectful execution
  must not reuse a prior runtime result through this pure-work mechanism.
- [x] Returned-value jobs preserve explicit computation conversion. For an
  accepted `M : F B` obtained by conversion from `F A`, the job obtains the
  checked value of the original computation, recovers `B` formation from the
  target `F B`, and uses the existing resumable conversion checker to derive
  the returned value at `B`. It neither discards the target classifier nor
  treats the certificate for `F A = F B` as a certificate for `A = B`.
  Projection/reindex wrappers resume the same returned-value requests in their
  source contexts and apply the existing context rules to their results.
  Tests cover source `::` inside computed annotations, conversion under beta
  reindexing, distinct alpha-equivalent thunk-Pi classifiers, preservation of
  original proofs, context actions and completed-job reuse. This is not a
  blanket rule erasing conversion evidence from every computation.
- [x] RETURN and THUNK content requests share one scheduler state machine for
  introduction inversion, context actions and checked conversion. A THUNK
  request exposes its stored computation without executing it; FORCE reduction
  subscribes to that request. Conversion of `U C` to `U D` requires a separately
  checked conversion of the extracted code from `C` to `D`. Context-action
  decomposition is shared with reduction preparation for either polarity,
  without new Core tags or a second context algorithm. Tests cover distinct
  thunk-Pi classifiers, converted FORCE, projected/reindexed contents, original
  evidence retention, wrong input contexts/polarities and completed-job reuse.
- [x] A direct reindexed variable (including a weakened original variable)
  exposes its existing checked image premise from the substitution. The
  returned-value/content scheduler follows that image rather than descending
  to the original neutral variable. Reindexing APP/FORCE/FOLD can also be
  distributed to their operand proofs using the same accepted rules; the
  content scheduler uses that exposed derivation before demanding a result.
  Original reindex evidence and its occurrence are unchanged. Tests substitute
  a THUNK for a variable, expose its code, evaluate reindexed FORCE to a checked
  value, preserve the original premises, and reuse the completed job. When the
  exposed content already has the exact target classifier, no redundant
  conversion proof is added; otherwise the existing comparison checks it.
- [x] Reduction jobs distribute a direct reindexed APP/FORCE/FOLD before
  demanding its next execution step. The exposed derivation is a shared child
  request, not itself reported as a beta step. The result retains the original
  classifier up to checked alpha equality, using the existing resumable
  conversion checker when substituted binders differ. Formation reconstruction
  can itself freshen Pi binders, so exact classifier-pointer preservation is
  not claimed; neither result is alpha-interned. Tests cover a substituted FORCE demanded inside FOLD, its
  final value, a reindexed polymorphic application returning raw Pi code, and
  completed-step reuse. Original source/reindex proofs remain unchanged.
- [x] Shared reduction jobs handle direct converted computations and application
  of converted raw-Pi code. Argument types are checked against the original
  function domain; reducts are checked against the original request's classifier.
  Sequential argument/result comparisons have separate comparison state.
- [x] Push reindex through nested reindex by checked substitution composition,
  and through conversion by exposing the substituted original premise before
  restoring the target classifier. Do not evaluate the unsubstituted source
  variable first: an outer substitution can supply its executable code.
- [x] APP and FOLD accept structural alpha equality of the domain, consistently
  with Lambda and checked substitution. Reindexing can freshen internal Pi
  binders independently in function and argument classifiers. Pointer equality
  was incorrectly rejecting these valid applications. This is explicit alpha
  comparison, not alpha interning or implicit beta conversion. Structural
  mismatches remain rejected and original term/classifier pointers unchanged.
- [x] Compare Core evaluation with checked returned-value synthesis for five
  application fixtures and twelve source block fixtures, including higher-order
  arguments, nested sequencing and post-synthesis annotations. Each returns the
  existing value `x : A`; single-step fuel tests retain suspended job behavior.
  Normal and ASan/UBSan pointer suites pass, including domain mismatch rejection
  and the unchanged 158-case syntax inventory (not semantic example parity).
- [x] Push substitution through prefix projection: restrict its checked image
  array to the original proof context, then apply the existing substitution
  and reindex constructors. This preserves the supplied values instead of
  trying to evaluate a neutral source variable before substitution. A regression
  failed before the change and now returns `x : A` from a weakened FORCE whose
  thunk variable is instantiated outside the weakening. Exposure reuses the
  previously constructed substitution/reindex proof, allocates no extra Core
  or evidence records on repeated requests, and keeps the original premise DAG.
  Normal and ASan/UBSan pointer suites pass this regression; no higher family
  formation or Identity rule is implied by this context-action law.
- [ ] Generalize substituted-redex exposure across remaining intermediate rule
  wrappers as additional former support is admitted. The synchronous one-step
  reducer retains its original contract; administrative proof construction is
  not itself a beta/FORCE/FOLD execution step. Primitive substitution and alpha
  traversal still need budget accounting. These tests establish neither general
  higher-order completeness nor HOTT/ADT/effect support.
- [x] Replace recursive common readback/substitution traversal with explicit
  heap work frames, indexed by the same `(term, lexical environment)` key.
  Children complete before their parent is constructed; repeated DAG children
  reuse one result. Lambda capture avoidance and simultaneous substitution
  retain their existing semantics, without alpha or reduction interning.
  A depth-50,040 shared APP graph substitutes successfully, including with a
  512 KiB process stack. Existing closure, conversion, restriction and typed
  evaluation tests exercise this same implementation, not a separate fast path.
- [x] Expose resumable substitution using these same work frames. Initialization
  snapshots the binding array without walking the input term; `advance` counts
  traversal transitions and each environment lookup link. Pending jobs expose
  no result. The synchronous substitution API drives this same engine to
  completion; no second substitution semantics is introduced. Tests compare
  split versus whole budgets and total steps on the deep shared graph, Lambda
  capture avoidance, long environment lookup, binding snapshot isolation,
  zero fuel, completion reuse, and destruction of suspended work.
- [x] Beta-WHNF jobs now retain readback work and materialize neutral application
  spines under the same requested budget. Public job status stays pending until
  the complete materialized answer exists; conversion consumes this interface
  unchanged. The shared readback/substitution traversal supplies the work, not
  a second term-reconstruction algorithm. Tests cover short execution with deep
  captured readback, split/whole step equivalence, capture avoidance, argument
  order, completed/pending request reuse and destruction during readback.
- [x] Beta execution now consumes one environment link per variable-lookup
  transition. For a reference, dropping an unrelated environment prefix changes
  neither its denotation nor captured argument closures; the shared environment
  list remains immutable. This removes the synchronous lookup loop without a
  second cursor/tag in the evaluator. A 64-level environment regression checks
  suspension at each link, residual readback, and split/whole step equivalence.
- [x] Reindex evidence construction now advances the common substitution engine
  for Core, classifier and optional annotation before accepting any conclusion.
  The synchronous API uses the same preparation/advance/accept path with local
  work storage; cached synchronous requests do not allocate a new heap job.
  Public work owns no accepted proof: completion returns the immutable premise-
  keyed evidence, including when another request has accepted it in the meantime.
  Tests cover no pending evidence, split/whole execution, cancellation, invalid
  contexts, Lambda domain annotations, and concurrent identical conclusions.
- [x] Content extraction routes reindexed RETURN/THUNK results through the
  existing synthesis queue, keyed by substitution and original proof. Context
  actions are scheduled before synchronous introduction extraction so they do
  not bypass this path. Other jobs wait on the same dependency mechanism; there
  is no second scheduler or new acceptance rule.
- [x] Preparing a typed beta step now returns its checked context substitution
  and original body rather than substituting the body synchronously. Both the
  synchronous reducer and the synthesis reducer consume this same preparation;
  synthesis waits on its shared reindex job. Completion keeps the beta rule's
  structural-alpha classifier check, without manufacturing a conversion proof.
  A 120-level RETURN/THUNK body remains pending during substitution, then returns
  the same accepted evidence as synchronous beta. Existing projection/reindex,
  higher-order, converted-function and shared-demand fixtures remain covered.
- [ ] Thread resumable substitution through remaining typed evidence/synthesis work and
  budget dispatched evaluation readback. Those call sites still use synchronous wrappers.
  Initialization validates/copies bindings synchronously; allocation and hash
  maintenance are not wall-clock bounded. Other recursive traversals are not
  covered by the stack test. This does not complete end-to-end compiler fuel.
  In particular beta preparation still assembles and validates context images
  synchronously; most primitive evidence rules still call synchronous alpha.
- [x] Structural alpha and beta conversion now share one resumable scoped-pair
  walker in graph.c. Structural comparison supplies no normalization; conversion
  supplies the existing fixed beta-job policy. Pair memoization includes the
  binder correspondence, and reference lookup advances one scope link at a time.
  Pointer identity only shortcuts comparison outside a binding correspondence.
  Conversion alone issues its opaque certificate after successful comparison;
  an arbitrary comparison callback cannot produce such a certificate.
- [x] The synchronous alpha API drives that same walker; synthesis uses its
  resumable structural mode for final beta-classifier checking. No Core tag or
  alpha/WHNF interning was added. Tests compare 20,000-deep shared APP structures
  with split/whole budgets and linear task counts, reject bound/free confusion,
  and distinguish structural inequality from beta convertibility. Existing
  typed evaluation and conversion tests continue through the common algorithm.

**HOTT rather than only relation preservation.** N2 includes checked contracts
for transport/lifting and their dimensional boundaries, with actual computation
rules for the initial supported type formers. Unknown families may remain
neutral with checked types. Unsupported rules must be marked unsupported, never
fabricated as axioms or reported as complete HOTT. Derive and check symmetry and
composition in the supported fragment. Do not defer their design until after
the artifact format is frozen.

**IADT and effects.** N3 must implement action on declaration telescopes,
constructors, dependent Match and IH at the same time as ordinary IADT support.
Generate higher families with generic indexed declarations and pointer owners;
do not add a primitive per datatype or per dimension. Write the fibrancy and
transport obligations for indexed families explicitly; a generated higher IADT
alone does not discharge them. N4 must specify the action laws for CBPV
return/thunk/force and fold. Effectful observational equality needs a stated
observation model and cannot follow from equal returned values. An arbitrary
host oracle receives no automatic Identity proof.

**Early gates.** N1 tests map identity/composition, faces, degeneracies and binder
scope at dimensions 0, 1, 2 and a dimension-3 smoke test. N2 must compute action
on identity, a constant function and APP, then act again on the result and check
its boundaries and classifier. Include a non-DefEq function equality witness
once the necessary observational rules exist; same-term reflexivity alone is
insufficient. In N3, distinct Bool constructors must reject an attempted witness,
and Identity-generated datatypes must themselves support Match/action. No claim
of all coherence laws follows merely from running the dimension-3 example.

Current `src/prototype/include/a_program/dimension/action.h` already exposes
acted-family and context-aware classifier operations. Audit it and
`src/prototype/src/dimension/` for reusable equations and fixtures; do not port
their integer stores or the previous multi-stage publication machinery blindly.

## 6. One Typed Work Graph

`pointer/classifier.c` introduces structural classifier constructors/views,
not a checker. Universe objects retain distinct concrete `uint64_t` levels,
interned within one classifier owner. Pi is
`APP(APP(pi-former, domain), LAMBDA(binder, codomain))`, reusing ordinary Core
nodes and simultaneous substitution. Semantic object headers carry an owner
descriptor pointer; its name is diagnostic, not semantic identity. Core
interning/evaluation do not inspect that descriptor to select a type view.
These in-process descriptors will need explicit image relocation identities.

- [x] Test universe level distinction and index growth; Pi exact-pointer reuse,
  separate alpha/beta comparison, and codomain substitution. Sanitizers pass.
- [ ] Add checked universe/Pi formation and source elaboration. The structural
  constructors do not establish value/computation sorts, universe constraints
  or domain/codomain well-formedness. Concrete levels do not replace the planned
  level metavariables/constraints; `UINT64_MAX` has no representable successor
  and must not silently wrap when formation is implemented.

Contexts are persistent extensions containing binder pointers and classifiers;
extending a context shares its prefix. Typed occurrences preserve scope and
source evidence even when Core is shared. Context substitution is a mapping of
these bindings, not a copied parallel term tree.

`pointer/evidence.c` now accepts immutable primitive derivations for empty
context, context extension, concrete Universe formation and variables. Context
extension requires a derivation of its declared type in exactly the parent
context and a fresh binder. Universe formation produces `U(level+1)` and rejects
overflow. Variable evidence references the verified context, whose premise DAG
contains the declaration formation. Term judgments are attached to occurrences,
not only erased Core. Proofs are opaque externally and interned by rule,
conclusion and all premise pointers; no accepted record is overwritten.

- [x] Verify primitive context/Universe/variable premises, rejection of free
  variables, wrong scopes, duplicate binders and non-type declarations; retain
  distinct occurrence evidence over shared Core. Ordinary/sanitizer tests pass.
- [x] Add pure F/U/Pi formation using existing Reference/Application/Lambda
  spines. Distinguish value/computation type formation from value/computation
  inhabitation in evidence conclusions, never by duplicated Core tags.
  A value inhabiting a value universe determines a value type; computation-type
  formation does not make that type a value inhabiting the same universe.
  Formation classifiers record concrete universe bounds. Pi requires a value
  domain, its verified context extension, and a computation codomain in that
  extension; its bound is the maximum of the two premise bounds. Ordinary tests
  cover wrong scopes, wrong sorts, retained premises, interning and bounds.
- [x] Add checked RETURN/THUNK/FORCE and Lambda/APP derivations. Fixed semantic
  operation references build ordinary APP spines. Lambda uses the verified Pi
  context/codomain; APP substitutes its value argument into the codomain with
  the existing simultaneous substitution implementation. Typed operands and
  all proof premises remain attached to occurrences. Tests cover invalid
  value/computation sorts, wrong scope/domain/codomain, explicit force before
  applying a thunked function, proof reuse, and beta reduction of a typed
  identity application to its RETURN spine.
- [ ] Connect these rules to synthesis and semantic execution. The current APP
  rule requires exact classifier pointers; an explicit conversion derivation
  remains necessary for non-identical convertible classifiers. The beta-only
  evaluator intentionally keeps semantic references neutral. F currently
  describes the pure fragment, not unspecified effect rows. Effect rows and
  dependent sequencing remain pending.
- [x] Connect value-type formation to term-level universe inhabitation by an
  explicit checked derivation. This is a Russell-style value universe: F/Pi
  computation formation cannot use this rule; U of a computation type can.
  Test an assumed `f : U(Pi(A:U1, F A))`: applying `force f` to the value U0
  synthesizes `F U0`, while applying it to an open `B:U1` synthesizes `F B`.
  Neither requires executing `f` or deciding B's eventual value. Formation
  evidence is not silently accepted as argument inhabitation evidence.
- [x] Prefix-context projection (weakening) retains the original proof and Core
  and creates only a conclusion occurrence and derivation in the extended
  context. Its operands remain shared under the explicit projection evidence;
  no recursive proof copying or binder renaming occurs. Reject shrinking,
  sibling scopes and context formation used as a term premise.
- [x] Recover classifier formation from existing variable, RETURN, THUNK,
  FORCE, Lambda, type-as-value and conversion derivations. This regularity
  operation consumes an already synthesized judgement, not an expected type.
  Tests obtain the formation of a RETURN body and construct its enclosing Pi
  from that result. No `::` information enters this path.
- [x] Add checked finite context substitutions as derivations, without another
  mutable solution database. Images are value derivations in the destination
  context, ordered by source declarations. Validate each dependent declaration
  after substituting preceding images. An immutable pointer mapping is stored
  inline with the accepted derivation as a projection of those premises.
  Reindex Core/classifier/annotation through the existing capture-avoiding
  substitution traversal and retain the original proof DAG as a premise.
  Tests cover dependent renaming, empty source, invalid arity, image types,
  computation images, foreign scopes and recovery of reindexed classifiers.
- [x] Compose checked substitutions by reindexing their value premises and
  validating the resulting common substitution representation. Lift under a
  binder by reindexing its declared type, extending the destination context,
  projecting old images and adding the new variable image. No composition or
  lifting Core tag is introduced. Test the composite against successive
  reindexing, new/old variable images and rejection of incompatible contexts
  and non-fresh destination binders.
- [x] APP/FORCE regularity now uses formation inversion rather than retracing
  the provenance of a Pi/U type. From a checked `Pi(x:A,B)` and `v:A`, obtain
  formation of `B[v/x]`; from checked `U B`, obtain formation of B. These rules
  apply to generally reindexed formations too. Their derivations retain the
  type formation and argument premises, not an unchecked syntactic view alone.
  Both rules retain the parent's universe *upper bound*. In particular, Pi
  formation bounds its codomain by max(domain,codomain), so codomain inversion
  need not recover a minimal universe. This is not equality between levels.
  This removes APP's repeated reconstruction of the entire source context's
  image array solely to recover result formation.
- [ ] Connect these context actions to dimensional action. The current
  substitution checker admits structural alpha equality;
  nonstructural conversion must be made explicit before supplying an image.
  It does not decide arbitrary effectful equality.
- [x] Reindex results are retrieved from the existing derivation index by their
  immutable substitution/proof inputs before traversing or freshening binders.
  The result's fresh pointers are outputs, not the key identifying this work.
  Context-substitution requests similarly retrieve an accepted derivation before
  recomputing dependent declaration substitutions. The same index lookup code
  serves acceptance and reuse; there is no parallel result authority. Test 100
  repeated binder-containing reindex/lift requests with identical proof results
  and zero growth in Term and derivation counts. Different inputs are not merged
  by alpha or WHNF equality. This is not memoization of runtime effects.
  APP elimination and Pi-codomain formation now use this same input-keyed
  derivation lookup before substitution. A regression returning a dependent
  Pi exposed fresh classifier binders and duplicate proofs on repeated APP;
  100 identical APP/regularity requests now return the original proofs without
  increasing Term or derivation counts. Accepted premises remain part of the
  key, so distinct typed occurrences are not merged through a shared Core.
  FOLD elimination and constant-codomain formation also consult this index
  before repeating their independence check. A repeated FOLD previously
  returned the same evidence but allocated fresh test references on every call;
  the 100-request regression now checks both proof identity and zero Term growth.
  This reuses accepted typing work only, never the result of executing FOLD.
  Regularity for unsupported rules returns NULL, not a refutation. Recovery
  currently traverses the relevant proof premises; scheduling/memoization and
  source synthesis still need integration rather than a separate type authority.
- [x] Initial source-synthesis jobs are interned by syntax and lexical scope.
  Requests do not synthesize; a budgeted ready queue advances dependencies.
  A waiting parent subscribes once to its child and wakes on completion rather
  than scanning all pending work. Results point to accepted derivations, with
  no parallel mutable classifier answer. Completed jobs do not run again.
  Source scopes map names to pointer binders and verified contexts; they are
  not execution environments and do not change Core interning.
- [x] Connect value universes, scoped variables, annotated Lambda, dependent Pi,
  APP, quotation and inline post-synthesis `::`. Lambda bodies retain raw
  computation polarity, while value bodies acquire RETURN. Function-typed
  value domains use U(Pi); callable thunk values are explicitly forced during
  elaboration. Inline `::` first finishes its left job, then synthesizes its
  target and performs a resumable comparison; the expectation never flows back
  into the left job. Tests compile a raw polymorphic nested identity, execute
  an application, check function expectations and reject unresolved names.
- [ ] Complete source lowering/synthesis: imports and full definition diagnostics,
  literals, ADT/IADT, computation blocks/folds, implicit sequencing
  of remaining returning argument/callee cases, computed type
  annotations, structured error reasons and comprehensive surface compatibility.
  Unsupported syntax and unsupported computation-argument cases report UNSUPPORTED,
  not a theorem of untypability. Kernel APIs still conflate some allocation and
  premise failures; error classification must be completed. Budget currently
  counts scheduling/comparison transitions, not all work within a kernel rule.
  This initial expression-job store is not a serialized `.a` image yet.
- [x] Whole-source parsing collects flat definitions into the same definition
  array shape as explicit `{{...}}.name` roots, preserving explicit selection
  rather than inventing a `main` binding for libraries. Assignments, standalone
  post-checks and imports retain their entry kinds and source order. No Core
  terms, name resolution, implicit quotation or effects are produced by this
  step. Tests cover forward names, unchanged computation RHS, empty sources,
  malformed suffixes and rejection after partially consuming a source.
  This is an unresolved syntax input, not yet the N5 program image or a solved
  module. Legacy definition fixtures require implicit/explicit thunk policy;
  that policy belongs to synthesis, not this parser normalization.
- [x] Definition roots now register all local producer jobs before advancing
  bodies. Name references subscribe to those jobs and project their accepted
  evidence into the use context. Aliases share the same evidence; no parallel
  name-to-classifier answer store is introduced. Definition adaptation is a
  distinct job role keyed with syntax/scope, so expression synthesis remains
  raw and independent of the definition's implicit/explicit thunk policy.
  Standalone `::` creates an ordinary post-check job and never supplies an
  expected classifier to the producer. The root waits for all entries, not
  merely a selected prefix as a sequential block does. Unselected libraries
  expose producer jobs rather than inventing an expression or a `main` value.
  Tests cover forward references, type aliases, shared function aliases,
  post-checks before definitions, both quotation policies, duplicate/missing
  names, failure in an unselected definition, and execution after explicit force.
- [x] Definition indexing and activation advance one entry per scheduling
  transition. Producer jobs remain dormant until all names are registered;
  partial budgets cannot expose an incomplete name table to a body. Shared
  producer jobs are activated once even when multiple entries share syntax.
  Tests inspect a one-step prefix (one indexed name, no generated Core) and
  resume it, as well as a shared-syntax definition DAG.
- [ ] Definition SCC diagnostics and imports remain incomplete. A circular
  alias pair exhausts its ready work and stays Pending with no accepted proof;
  it is not a supported recursive definition. Array allocation and hash-index
  growth are still unbudgeted storage operations. Image persistence, complete
  budgeting, imported names and full definition compatibility remain required.
- [x] Active dependency inspection uses the existing subscription edge in both
  directions: the child wakes its waiters and clears their active edge, while
  a consumer exposes the child it is waiting for. There is no copied pending
  status or second dependency graph. An on-demand constant-space cycle query
  follows active edges without scanning all jobs or advancing/rejecting work.
  Tests verify completed jobs have no active dependency, a circular alias pair
  reports a closed waiting path with no accepted result, and additional fuel
  does not reschedule the stalled cycle. This is diagnostic evidence, not an
  SCC acceptance rule or a proof that every pending job can make progress.
- [x] Ordinary APP and inline `::` share one resumable comparison path. Cache
  the already synthesized input and target derivations while comparison is
  pending; do not rerun their synthesis or reconstruct adaptations on each
  comparison step. Pi domain formation is recovered by checked inversion with
  the parent's universe upper bound. A mismatch in argument classifier pointers
  invokes explicit conversion, never alpha interning or expected-type inference.
  A source fixture applies a higher-order function to a quoted Lambda with a
  distinct alpha-equivalent Pi classifier, retains the conversion certificate
  in its argument premise, and evaluates to RETURN of the original argument.
  A function with a different result type is rejected by the same comparison.
- [ ] Semantic conversion extensions, full synthesis scheduling, image checking and
  typed HOTT action still need implementation. These primitive rules do
  not constitute a complete checker; NULL currently combines invalid-premise
  and allocation failures and is not a solver-level logical rejection result.

### Image checking is not a separate Replay semantics

The September 7 clarification applies to every replay gate below. A `.p` file
creates an unresolved program image; solving advances that image, and loading
resumes the same solver. Do not introduce a loader-specific compilation or
proof-search pipeline. Previously solved work must not be searched again merely
because it crossed a file boundary.

Loading still checks structural references and the validity of retained evidence.
Use the same checked derivation rules used to accept new solver results. Check a
shared premise DAG once per load rather than recursively duplicating its work.
Checking evidence is distinct from rediscovering its proof: conversion premises
may nevertheless require reduction or validation of retained reduction evidence.
No unchecked serialized "accepted" bit constitutes a proof.

- [ ] N5: restore pending work and resume the existing solver without a parallel
  Replay engine; distinguish discarded work from retained validated results.
- [ ] N5: route reconstructed derivations through the existing acceptance rules;
  reject invalid evidence and test shared-DAG checking and split-budget resume.
- [ ] N5: document what seed/checkpoint retention saves and which calculations
  must be repeated. Do not promise zero recomputation for a compact image.

`pg_term_substitute` now exposes the evaluator's existing capture-avoiding
readback traversal for simultaneous binder-pointer substitution. Images are
inserted without resubstitution or reduction; later mappings shadow earlier
ones for the same pointer. This is an untyped term operation, not a certified
context morphism or a dimensional action. Typed substitution must still verify
the domain/codomain declarations. The output shares input/image nodes, so their
arenas must outlive it. The current traversal freshens traversed lambda binders;
alpha-equivalent outputs need not have identical pointers and are not interned
by alpha comparison. Empty substitution returns the original pointer.

- [x] Reuse one readback traversal for evaluator closures and explicit term
  substitution; test simultaneous swaps, capture avoidance, shadowing, a
  40-level shared DAG and preservation of unreduced APP. Ordinary and
  ASan/UBSan checks pass.
- [ ] Certify substitution against typed contexts and connect it to dimensional
  restrictions; term substitution alone does not discharge these obligations.

Intern a constraint by its complete semantic inputs: rule, context, typed
operands, policy and relevant declaration identities. Attach diagnostic source
sites separately. Allocate the goal before registering dependencies so cycles
and forward producers do not depend on AST traversal order.

Each goal owns one current answer and its reverse dependencies. The queue stores
work, not another answer. A pending consumer subscribes to actual dependencies;
publication wakes affected consumers only. Provenance needed to justify an
answer is part of the answer's change detection. SCC handling must express the
recursive rule's obligations, not accept a cycle as its own proof.

Keep immutable derivations with explicit premises. Multiple derivations can
conclude one proposition; do not overwrite accepted proof records. A checked
occurrence can reference its accepted evidence directly. Introduce another Claim
store only if a concrete consumer needs a distinct acceptance identity.
Expected-type checks, diagnostics and artifact roots read these results rather
than reproducing their lifecycle.

Implemented input boundary (`pointer/typing.c`): immutable declared contexts
are interned by `(parent*, binder*, declared_type*)`. An occurrence is interned
by `(context*, core*, explicit_annotation*, typed_operands[])`. Typed operands
retain the declarations erased from shared Core children; they do not duplicate
Lambda/APP computation fields. Neither allocation constitutes accepted typing
evidence. Explicit annotations are immutable syntax inputs, not solver answers;
`::` must remain a subsequent check. Diagnostics are not part of these keys.

- [x] Persistent declared contexts and exact-pointer occurrence interning.
- [x] Test distinct lambda occurrences with identical erased Core but different
  body declarations, exact-key reuse after index growth, and invalid storage
  inputs; ordinary and ASan/UBSan builds pass.
- [ ] Validate occurrence scope/operand contracts in elaboration and implement
  synthesis with explicit derivations. Storage currently accepts unchecked
  inputs and must not be presented as a kernel checker.
- [ ] Connect typed occurrences to dimensional action and its classifier rules.

The new input store uses the existing arena and hash index. Context lookup is
currently a parent walk; no claim of constant-time binding lookup is made.
N0/N1 and N2 remain incomplete; these checks do not establish HOTT or surface
compatibility.

## 7. Compatibility Inventory

Inventory execution checkpoint: the new `tests/parse_files.c` runner read all
158 listed current-worktree programs. Initial coverage was 121 parsed / 37
syntax errors. Comparing failures with the existing reader identified missing
repeated `@\\` index markers and bare annotated lambdas (`x:A=>body`); after
implementing these, the result is 133 parsed / 25 syntax errors. Remaining
errors include graph-companion and import syntax plus old drafts. This is not
a conformance percentage: negative semantic fixtures may parse, and accepted
syntax trees still need semantic/precedence comparison. Existing examples
01--09 (eight files; no 08 file) are now permanent parser smoke tests and pass
with sanitizers, without modifying the examples.

`pointer/reader.c` now provides an allocation-free bounded lexer, separated
from name resolution and type decisions. Its tokens cover the current reader's
punctuation, identifier, Int64, plain/raw-delimited Text and C-comment forms.
Words including `return`, `perform` and `import` remain identifiers at this
layer. Text payloads borrow the input; no escaping or encoding conversion is
introduced. Explicit buffer length replaces the old NUL-terminated scan.

- [x] Lexical fixtures for indexed declarations, blocks, handler punctuation,
  contextual words, integer boundaries, delimited text, comments and truncated
  input. Ordinary and ASan/UBSan checks pass.
- [ ] Build the grammar and lowering on these tokens; compare the complete
  frozen fixture inventory. Lexer tests alone do not establish compatibility.

`pointer/syntax.c` now parses the initial annotated Lambda/application/Pi
fragment, qualified names, literals, quotation, and separate `::` checks into
source-owned syntax nodes. It does not resolve names, infer types, insert CBPV
coercions or evaluate expressions. Tests check left-associated application,
dependent binder retention and separation of expected-type checks from
definitions. Ordinary and ASan/UBSan checks pass. Grammar coverage is tracked
below; successful parsing is not a compatibility claim for elaboration.

Declaration syntax now accepts `@{...}` and `@\\index:A => {...}`. Outer
lambdas remain parameters; index lambdas sit inside the declaration marker.
Constructor classifiers are retained whole, in a source-order array, with `*`
and its index applications preserved rather than rewritten to a named recursive
reference. List/Vec/Acc, empty declarations, array growth and malformed syntax
fixtures pass ordinary and sanitizer checks. Name resolution must still reject
using the defining name as a recursive reference. Positivity, index typing,
declaration lowering and HOTT declaration action are not implemented yet.

Computation-block syntax now retains an ordered array of named assignments
(with optional declared annotations), unnamed expressions and lambda-exit
items. Postfix result selection remains explicit syntax; the parser does not
drop the suffix after a selected binding or resolve an exit target. Root-only
`{{...}}.name` uses a distinct definition-block node, including separate `::`
entries. Constructor and block arrays share one storage builder. Tests cover
ordering, nesting, quotation, selected results, exit preservation, root
restrictions and malformed delimiters; ordinary and sanitizer checks pass.
The pure block path is now connected to synthesis. Each active statement is a
shared synthesis job; a continuation frame retains its accepted input, domain
and extended context. Frames discharge through the same Lambda/APP/FOLD rules.
Already available syntactic values use `FOLD(RETURN v,K) = APP(K,v)` directly;
unknown returning computations retain FOLD. This permits dependent uses of
known values without inventing a type-level result of an unknown computation.

- [x] Named/unnamed statements, nested blocks, declared annotations as post-
  synthesis checks, lexical shadowing of outer names, quoted functions and
  selected-result cutoff. Selectors stop at a direct binding; later statements
  are parsed but not resolved or executed. A block-local name index rejects
  duplicate active bindings without repeatedly scanning previous statements.
  Tests reject unknown selectors and wrong annotations and execute eight
  representative block forms to the same expected RETURN value; additional
  application fixtures cover blocks used in function and argument positions.
  A dependent result after `B := A` substitutes the known type value into its
  classifier.
- [x] The corresponding `B := (\T : @ => T) A` case now requests the shared
  checked returned-value job when nondependent FOLD cannot close its
  continuation. Only after obtaining actual value evidence does ordinary APP
  instantiate the continuation's dependent classifier. The same close operation
  serves computation blocks and computed application operands. Formation is
  retained across suspension; multiple application frames close one scheduler
  step at a time instead of an unbounded loop. Tests cover one/two dependent
  block bindings, a computed dependent function argument, classifier agreement
  with the known-value case, and independent Core evaluation. An unknown
  function returning a type remains UNSUPPORTED; no type-level future result is
  fabricated. Nondependent computations still use FOLD without forced eager
  evaluation. The public result accessor exposes only completed jobs, not a
  partially closed application while its value dependency is pending.
- [ ] Lambda-exit target/barrier semantics, remaining computed type/argument
  cases, effect requests and handlers, and complete dependency/resource checks.
  EXIT remains UNSUPPORTED. This does not establish full block compatibility or
  effect-order correctness merely from the passing pure fixtures.

Elimination syntax now retains multiple clauses with unresolved label syntax,
positional binders or named selector aliases, and bodies. `#.return` is retained
as a qualified name, not dispatched in the parser. The same clause/list storage
serves ADT and effect syntax without claiming their typing rules are identical.
Tests cover unparenthesized lambda branch bodies, explicitly grouped nested
matches, operation aliases and return labels; ordinary and sanitizer checks
pass. Name/arity validation, head classification, scope checks and iota/fold
lowering remain unimplemented. The full existing nesting/precedence fixture
inventory still needs comparison before claiming grammar compatibility.

Import and graph-companion syntax is now retained without parser-side type
construction. `\\@f:G=>...` and `\\*f=>...` retain their markers; the latter has
no fabricated domain. `@name` in argument position is distinguished from a
clause head by following syntax. Top-level/root-block imports record only the
requested name. Tests and a sanitizer inventory run now parse 154/158 files.
The four remaining syntax failures are `12_append_assoc_draft.p` and the three
`stage*.p` files. Review against `reader.c` confirms the former uses untokenized
`==` syntax and the latter unannotated lambdas (stage1 also lacks a top-level
definition). `syntax_exclusions.tsv` records their exact diagnostic outcomes;
`syntax_inventory.sh` now verifies all 158 outcomes in `make check`, including
sanitizer runs. This does not claim equivalence of resulting syntax trees. No
negative fixture has been certified merely because it parsed. Companion
origin checking, import resolution, typed lowering and full precedence/scope
conformance remain outstanding.

This table is a starting inventory from the current reader, AST and test tree,
not a claim that the failed snapshot passes every row. N0 must enumerate exact
fixtures/options and distinguish old bugs from intended behavior.

| Area | Current evidence to read/reuse | Replacement acceptance |
| --- | --- | --- |
| names, Lambda, APP, annotations | `src/prototype/src/frontend/reader.c`, `include/a_program/frontend/ast.h`, examples 01-09 | accepted/rejected syntax and synthesized types; `::` cannot steer synthesis |
| ADT, parameters, indexed `@\\i:T => {...}` and `* i` | `test_explicit_index_family_surface.sh`, Vec/Acc fixtures | generic telescopes, nominal owners, correct rejection of recursive self-name syntax |
| dependent Pi, Match, recursive `*field` | `test_dependent_pi.sh`, `test_dependent_match_refinement.sh`, List induction fixture | dependent motive/refinement, single and multiple recursive fields |
| blocks, definitions, `&`, `!` | `test_computation_block_sequence.sh`, `test_definition_block.sh`, `test_cbpv_surface.sh` | exact binding/selection/exit/force policy, not just same final pure value |
| literals, intrinsic names, requests and multi-clause folds | reader `parse_elimination_head`, CBPV fixtures | ordinary operation application, aliases, `@#.return`, output carrier and effect checking |
| imports, exported nominal declarations, CLI/REPL | driver sources, `test_artifact_flow.sh`, reader session tests | cross-process identity/relocation; WHNF/NF commands and diagnostics |
| Acc, totality, generated function graphs | `test_if8_fuel_free_quicksort.sh`, `test_totality_evidence.sh`, function-graph tests | general indexed elimination; no Acc or QuickSort special primitive |
| Identity and higher supported fragments | `src/prototype/src/identity/`, HOTT/Identity tests | port supported rules with explicit premises; no inferred full HOTT claim |
| universes, resources, effect constraints | universe/resource tests and kernel sources | distinguish pending, rejected and proved; no empty-row/unknown conflation |
| checked artifacts and resumption | checker/container and compilation-image tests | valid evidence replay, rejection of invalid references, resumable `.a` contract |

Paths abbreviated as `test_*.sh` refer to `src/prototype/tests/integration/`.
Legacy tests that assert enum numbers, old Core pretty-print tags or internal DB
layout are not language semantics. Replace those assertions with results, types,
effect traces, rejection reasons or evidence replay. Record each disposition;
do not delete a failing semantic test to make the new implementation pass.

## 8. Program Image and Persistence

One in-memory program owns graph roots, typed occurrences, declarations and work
results. Parsing/lowering creates its initial unresolved state; bounded solving
advances it. Execution is an explicit request using the same computation engine
with a host environment. A parsed graph is not permission to run unchecked code.

`.a` uses section-local wire references reconstructed into pointers on load.
Never serialize addresses or function pointers. Resolve semantic descriptors by
versioned builtin names or declaration references and validate their payloads.
Support recursive declaration graphs through allocate-then-link-then-validate.

Writer retention options: RECOMPUTE stores sufficient immutable input; CHECKPOINT
adds validated progress and evidence. They use the same loader and solver. Omit
work queues and historical snapshots; rebuild acceleration indexes. Do not copy
the failed seed/current/handoff structs into the new implementation. A small
round-trip prototype must demonstrate which roots suffice before freezing wire
format. Legacy `.apo/.ao` import, if retained, belongs in a conversion tool, not
the core evaluator. Exact old wire compatibility is not promised by this plan.

## 9. Implementation and Progress

Complete one vertical slice at a time. Update this table with commit, commands,
results, timing and net source/test LOC. An unchecked row is not implemented.

| State | Step | Concrete work | Required completion evidence |
| --- | --- | --- | --- |
| [x] | Archive | preserve failed source and stop predecessor | local archive commit and failure record |
| [ ] | N0 Baseline and HOTT rules | inventory syntax/tests; pin Narya source and define action, boundary, transport/lifting rules for the initial fragment | compatibility manifest plus concise rule/representation correspondence, including CBPV obligations |
| [ ] | N1 Dimensional pointer Core | stable arena, pointer-key interning, explicit alpha comparison, evaluator; dimension maps and shared boundary diagrams | beta/capture tests, no conversion-based interning, dimension 0-3 action laws; no depth-specific Core variants |
| [ ] | N2 Typed HOTT vertical slice | reader, synthesis, contexts; typed Act, Identity and initial transport/lifting computation | shared Core with distinct typing; post-check `::`; iterated action on Lambda/APP, checked boundaries and supported equality operations |
| [ ] | N3 Dimensional ADT/IADT | parameters/indices, self family, telescope action, nominal declarations, Match/IH and their higher rules | Nat/List/Vec/Acc; higher constructors and Match; indexed transport obligations recorded; producer-order-independent goals |
| [ ] | N4 CBPV and effects | remaining block/quote/exit forms; structural references; request/fold reducers; effect rows and continuation rules | 01-09, single versus repeated execution, nested exit boundary, operation alias, multi-clause deep handler and unhandled forwarding |
| [ ] | N5 Image/CLI | `.a` relocation, seed/checkpoint retention, imports, CLI and REPL parity | fresh-process round trips, nominal identity, split-budget solve equivalence, WHNF/NF; replay rejects malformed evidence |
| [ ] | N6 Proof feature parity | supported totality, generated graph IADTs and resources over the foundational HOTT system | IF8, supported function properties and expanded Identity fixtures; negative cases reject; remaining theory limits recorded |
| [ ] | N7 Acceptance | finish manifest, benchmark and review deletion/transfer plan | all agreed supported cases pass; per-module old/new LOC and timings; no hidden use of old solver or runtime |

Dependencies: N0 -> N1 -> N2 -> N3 -> N4 -> N5 -> N6 -> N7. Small experiments for
persistence or recursive binding may happen earlier; they do not bypass the
preceding acceptance gates. N2 must run from source without the old solver before
porting advanced features. N0/N1 already include HOTT's dimensional foundation;
N2 must demonstrate typed higher computation before broader feature migration.

For each step append only a short progress entry:

```text
Step / date / commit:
Behavior implemented:
Checks and elapsed time:
Source + / - / net; tests + / - / net; docs separately:
Unresolved issue and next action:
```

Track graph allocations, intern hits, reducer steps, goal evaluations/wakeups,
and peak memory alongside elapsed time for small examples, List/append and IF8.
Test source-to-result and artifact-to-result independently. Compare the same
input/options on consecutive implementations; do not infer speed from LOC.

## 10. Review Gates and Explicit Limits

- Pointer-based Core is the chosen direction; measure interning collisions and
  explicit alpha-comparison costs separately. Do not add binder canonicalization
  as an implicit construction pass.
- Oracle/IADT descriptors may contain complex rules. Count their code as Core
  functionality when reporting size; hiding it behind pointers is not reduction.
- Do not copy the old solution/projection/transaction infrastructure wholesale.
  Retain mathematical rule distinctions such as APP elimination and Match
  elimination even when they share allocation and scheduling utilities.
- HOTT is an initial architectural requirement, not a later feature. Completing
  every dependent/higher/Universe rule remains a separate mathematical claim;
  record coverage and missing equations without weakening the initial design.
- A complete parser does not mean a complete type checker. Maintain separate
  parsing, typing, evaluation and replay status for each compatibility case.
- Main replacement requires the N7 evidence and explicit acceptance. The failed
  branch remains available. No deletion of the old implementation is needed to
  start this independent prototype.

## 11. Progress: 2026-09-07 Initial Core

Historical checkpoint `6ef8a74`; its alpha-interning decision is superseded by
the exact-pointer-key correction below.

Implemented independently in `src/prototype/pointer/`:

- Stable chunk allocation and a shared hash-index implementation used by terms
  and dimensional maps. Core retains exactly Lambda, Application and Reference.
- Pointer-bound alpha comparison/interning; APP construction hashes its child
  pointers directly rather than recursively rescanning them. Lambda hashing
  still traverses its scope; collision/performance work remains to be measured.
- A resumable lexical closure machine for pure Lambda WHNF and capture-avoiding
  readback. Budgets count machine transitions, including administrative steps.
  Readback performs substitution, not normalization. Semantic references are
  currently neutral: oracle dispatch, NF and evaluation memoization remain open.
- Interned binary semicartesian dimension maps and composition. A map `m -> n`
  stores n coordinates drawn from m distinct source axes or the two endpoints;
  repeated source axes are rejected. The convention follows the plan, not the
  legacy operator API's source/target naming. Boundary diagrams and typed Act
  are not yet implemented.

Narya reference revision obtained from GitHub:
`c7c92b4ec01ae2f528b97207256549242bd21334`.
[Pinned dimension operator source](https://github.com/gwaithimirdain/narya/blob/c7c92b4ec01ae2f528b97207256549242bd21334/lib/dim/op.ml).
This pins the comparison target; it is not evidence that all corresponding
rules have been implemented or that HOTT is complete.

Verification:

```sh
make -f src/prototype/pointer/Makefile check
make -f src/prototype/pointer/Makefile check BUILD=/tmp/a-program-pointer-sanitized CFLAGS='-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'
```

Both passed: lexical capture and independent environments; split-budget
equivalence and bounded divergence; 38 maps in dimensions 0-2 with 10,422
composable triples; dimension-3 face/permutation laws; stable references across
arena/index growth. Observed build-and-test wall time was about 0.05s normally
and 0.26s with sanitizers in this session, not a compiler-performance claim.
New C/header code: +589/-0 lines. Test C: +188/-0 lines. Build/docs excluded.

Neither N0 nor N1 is marked complete. Next: complete the compatibility inventory,
implement shared boundary diagrams and scoped action using this pointer graph,
then integrate typed Act/Identity with source synthesis. Do not resume the old
single-path refactoring or push this partial implementation to main.

## 12. Progress: Pointer Identity and Boundary Bindings

The user's clarification supersedes automatic alpha interning in `6ef8a74`.
Core constructors now compare exact pointer tuples only, with no recursive
hashing, alpha comparison, WHNF evaluation or normalization in that path.
Alpha comparison remains an explicit operation and memoizes term pairs together
with their binder correspondence. It never merges them. Readback separately
memoizes term/environment pairs to preserve DAG sharing during substitution.

Lazy cube boundary binders are interned by `(cube*, face_map*)`. Restriction
composes maps before lookup, so two paths to one corner recover the same binder.
Distinct cube owners remain distinct. A degeneracy cannot allocate an independent
boundary variable; its term action still needs the typed action implementation.
These objects specify scopes, not inhabitants of Identity by themselves.

`src/prototype/pointer/tests/compatibility.tsv` now records the frozen source
inventory; its companion README explains the remaining manual review. The
parser's generated binary-Bool companion classifier was identified as semantic
work that must move to typed elaboration when preserving that syntax.

Normal and ASan/UBSan builds pass the expanded Core suite. Regressions include
40-level shared DAGs, capture-preserving readback, distinct alpha-equivalent
Lambda nodes, unchanged interning after beta reduction, and equal composite
boundary restrictions. The typed classifier for a cube, higher term action,
transport/lifting and source parsing remain incomplete. N0/N1 stay open.

The design is informed by the two handmade files and the current implementation
paths above. It is an A Program engineering proposal, not a claimed direct
implementation of an external paper or a claim that pointers establish a new
type-theoretic result.
