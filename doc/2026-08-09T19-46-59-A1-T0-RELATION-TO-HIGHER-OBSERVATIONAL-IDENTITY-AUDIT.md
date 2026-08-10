# A1.T0 Relation-to-Higher-Observational-Identity Audit

Date: 2026-08-09

Status: the admitted closed identity-family fragment, object term action, pure
raw-CBPV function APP, type-level Pi/APP/Lambda action, two-dimensional
Bool/nondependent-Box cases, finite Universe correspondence projections,
`B(a)`-selected dependent fibers, and first-dimensional dependent constructor
fields are executable. Genuinely dependent Pi, dependent higher constructor
squares, effectful computation identity, and higher Universe coherence remain
residual. The finite Universe correspondence is scaffolding and is not an
artifact identity root.

Update 2026-08-11: artifact v70 persists 16 roots from that explicitly admitted
fragment.  Every root is an ordinary generated ADT/Pi/Thunk family and optional
ordinary proof Term; compiler-local `RELATION_*`, action, bridge, work, and fuel
state are absent.  The stronger identity-versus-exhaustive-Match Bool proof is
one of the roots.  This establishes a non-DefEq introduction example, not a
decision procedure for contextual equivalence.  The term *observational* means
that identity computes by the observed type former. Adequacy with respect to
all admitted eliminators remains a metatheoretic obligation, and completeness
or full abstraction is not claimed.

Update 2026-08-11: the implementation order was rechecked directly against
*Towards Higher Observational Type Theory*.  The primitive object notion is
not an arbitrary four-place `Obs(A0,A1,x0,x1)`.  The calculus has homogeneous
identity `Id_A(x0,x1)`, telescope identity `Id_Delta(delta0,delta1)`, and a
heterogeneous fiber identity selected by an exact telescope identity
`delta2`.  Every typed term supplies the corresponding action `ap_a(delta2)`,
with homogeneous `refl_A(a)` as the empty-telescope specialization.  Equality
at the Universe computes to a proof-relevant correspondence equipped with the
data that makes it an equivalence; the induced correspondence then computes
the fiber identity.  Consequently `RELATION_*` remains only machinery for
constructing actions.  It cannot be used as an object identity former or as an
identity introduction rule.

This recheck does not invalidate the existing non-DefEq Bool-function test.
Its family is computed by the Pi identity rule and its witness is an ordinary
pointwise Lambda/Match Term, so it is a valid finite-fragment identity
introduction.  It does change the remaining order: dependent Pi must obtain
its result fiber through the codomain family's exact object action; Universe
transport/lifting and higher coherence must then be closed; symmetry and
composition are derived conformance tests after that structure exists.  They
must not be implemented first as operations on an arbitrary relation.

Update 2026-08-10: the general Universe-correspondence projection no longer
requires the correspondence witness itself to be a Context variable. The
kernel checks the projection Match under the generic Context
`Gamma,x0:U,x1:U,p:IdU(x0,x1)`, abstracts it with ordinary Lambdas, and applies
it to the actual endpoints and witness. Replay verifies the exact APP/Lambda/
Match Derivation DAG. Together with ordinary type-level Pi identity and APP
action, this makes `B : Pi(A,Universe), a : A, b : B(a)` executable without a
compiler-local relation fallback. It does not close dependent constructor
fields or the recursive coherence payload of Universe identity.

Update 2026-08-10: the theory review is accepted. The compiler-local
`RELATION_*` action is a logical-relation/parametricity substrate and is not an
object equality merely because it has diagonal, Lambda, APP, constructor, and
Match preservation rules. The object fragment may be called observational
identity only where the identity family is computed by the observed type
former, its inhabitant is an ordinary checked Term, and the required
transport/lifting structure is present. Reflexivity, symmetry, and composition
are necessary derived structure but are not a sufficient definition: an
arbitrary equivalence relation has those operations as well.

The finite Universe identity now has ordinary `R`, `trr`, `trl`, `liftr`, and
`liftl` projections. An attempted dependent-Context use exposed the next exact
blocker: a projection Match constructed in one endpoint/proof Context cannot be
reused in an alpha-isomorphic object bridge Context merely because both
Contexts alpha-intern to the same Core Match. Its accepted `MATCH_ELIM`
Derivation owns branch Context, binder, refinement-substitution, and premise
identity. The implementation must add proof-preserving reindexing of this typed
Operation/Derivation; it must not recover authority by searching for a matching
Core Term or by reconstructing a proof from shape.

Update 2026-08-10: Context bridges now record one of `EMPTY_IDENTITY`,
`PARAMETRIC_RELATION`, or `OBJECT_IDENTITY`. A non-empty homogeneous object
extension is built only from an exact `IDENTITY_TYPE_COMPUTATION` certificate
and an ordinary Context-binding witness Claim. Relation and object bridges may
coexist for the same source Context, and composed validation rejects exchanging
their action certificates. This establishes the representation boundary; it
does not yet implement dependent fiber lifting.

Update 2026-08-10: compiler-local Term tags 32/33 and their proof rules are now
named `RELATION_TYPE` and `RELATION_WITNESS`. Historical references below to
`OBSERVATION_TYPE` and `OBSERVATION_WITNESS` describe the withdrawn pre-audit
interpretation. Their numeric tags remain stable, but neither name nor tag has
object-identity authority.

The earlier closed `Id_(IdBool(false,false))(p,p)` test is classified as a
degenerate square only. General two-dimensional identity acts on the family
`IdBool(x0,x1)` over the object bridge of its two-binder endpoint Context. That
bridge contains six boundary faces. Closed Bool lifts through its first
extension by `ap_const(delta)=refl`; the next action now computes the square
center as an eight-index generated identity datatype. The positive test fills
the all-`false` boundary with its ordinary nullary constructor, while a boundary
with one `true` corner rejects that constructor. This closes only the nullary
datatype square rule. A field-bearing `Box Bool` square is also executable: its
single source field expands to four corner values, four edge identities, and a
field-square witness, and the resulting nine-field constructor spine checks as
an ordinary Term. A field classifier depending on an earlier field remains
residual.

Canonical constructor degeneracy is executable for the admitted ordinary-ADT
fragment. It recursively builds ordinary constructors of the computed identity
datatype and has tests for Bool, a field-bearing wrapper, and recursive Nat.
Neutral Bool, field-bearing Box, and recursive Nat endpoints now build an
ordinary dependent Match with the direct motive `lambda z. IdA(z,z)`, ordinary
branch Contexts, exact constructor-spine Claims, and guarded ordinary IH
Derivations. Pure thunked Return degeneracy and the canonical identity Lambda's
pointwise Pi degeneracy are now executable as ordinary constructor/Lambda/
Return/Thunk Terms. A pure Lambda body `APP(FORCE(f),x)` also acts on the
function and argument witnesses and uses ordinary zero-clause
`COMPUTATION_FOLD` nodes to sequence the three pointwise applications. This is
not yet generic `refl` for all admitted type formers: Match bodies, arbitrary
pure computations, effects, and Universe/lifting remain explicit obligations.

Applies to `hott.c`, `hott.h`, `hott_fragment_v2.schema`, the compiler-local
relation-action forms in `term.c`, their proof rules in `typing.c`,
`hott_goal_check.c`, and
the V2-A1 artifact implementation plan.

This is the theory gate required by A1.T0. It corrects the point at which the
implemented internal-parametricity action was prematurely identified with
object-language equality.

## 1. Audit Result

The current O1 implementation is a useful proof-relevant relational action:

```text
Gamma |- a : A
---------------------------------
Gamma^R |- a^R : A^R(a0, a1)
```

It is not yet a complete Higher Observational Identity type. Logical relations
are the intended mechanism for defining the action on contexts,
substitutions, types, and terms, but an arbitrary logical relation is not an
identity. The missing object structure is the telescope-indexed identity of
the HOTT calculus, its `ap` action, and its transport/lifting and computation
rules.

The following current claims are withdrawn:

```text
Eq A x y = OBSERVATION_TYPE(A,A,x,y)
OBSERVATION_WITNESS(x,y) is a proof-relevant equality witness
acting on an observation type again is sufficient to establish higher identity
```

They may become derivable after the construction below, but they are not
properties of the current implementation.

### 1.0.1 Review adjudication after the nontrivial-equality challenge

The external review correctly identifies the original `OBSERVATION_*` path as
logical-relation/parametricity infrastructure. Its conclusion applies to that
path, not automatically to the later object-identity computation path.

The implementation now has two intentionally separate actions:

```text
RELATION_TYPE / TERM
  computes and preserves a selected parametric relation

IDENTITY_TYPE_COMPUTATION
  computes an ordinary object-language identity family by type former
```

Only the second can supply a persistent identity-family root. Even then, a
witness is accepted only as an ordinary Term checked against that computed
family. A successful relation search, endpoint conversion, or
`OBSERVATION_WITNESS(left,right)` is not an identity introduction rule.

The executable Bool matrix contains two distinct tests. The first is the
weaker constant-function case:

```text
f = lambda b. true
g = lambda b. match b { false -> true; true -> true }
```

An ordinary pointwise Lambda witness for `Id(f,g)` is accepted, while reuse of
that witness for `Id(f,lambda b.false)` is rejected. This establishes that
object identity introduction is no longer restricted to the two reindexings
of one source Term.

The focused kernel fixture also establishes the stronger test

```text
lambda b.b
lambda b. match b { false -> false; true -> true }
```

Its ordinary three-argument pointwise Lambda eliminates the arbitrary input
witness `xR : IdBool(x0,x1)`, retains the branch-local index refinements, and
returns the corresponding generated computation-identity constructor. The
accepted graph is replayed read-only, and the same proof is rejected at the
identity/constant-false classifier.

The stronger proof is now published in the authoritative compilation
`JudgementDB` and `OperationGraph`, and its exact family/witness Claims are
registered as a v70 identity root. The focused test no longer owns an isolated
proof arena. Artifact root validation therefore sees the same ordinary
Term/Derivation graph as the compiler. Composition and transport remain
separate general operations; this example does not by itself implement either
one.

This resolves only the finite closed fragment. It does not establish complete
HOTT. A finite Universe correspondence with ordinary `R`, `trr`, `trl`,
`liftr`, and `liftl` fields and its first variable-family telescope action are
now executable, but recursive higher bisimulation/coherence, general dependent
telescope identity, and effectful identity remain residual. Nor does the test
establish full abstraction. The required metatheoretic direction for now is
only:

```text
p : Id A x y
----------------------------------------------
no admitted well-typed A Program observation distinguishes x and y
```

That adequacy statement remains an explicit proof obligation. Contextual
equivalence is not used as a kernel proof-search rule or as the definition of
`Id`.

The non-trivial inhabitant does not by itself complete even the closed HOTT
introduction interface. The generic `TERM` action still targets the old
compiler-local relation family and its endpoint-only witness former. It has
not yet been connected to the exact ordinary ADT/Pi family returned by
`IDENTITY_TYPE_COMPUTATION`. Therefore family computation and a hand-built
inhabitant are complete, while general degeneracy `refl` and term action `ap`
remain implementation obligations.

### 1.1 Decision after the relation-versus-equality review

The current four-argument form is retained only as the application of a
generated binary relation. It is not renamed to `Eq`, and an accepted artifact
must not advertise it as Higher Observational Identity. In specifications,
tests, and future APIs it is denoted provisionally by:

```text
RelAlong(R, x0, x1)
```

where `R` is the selected relation family carried by the Type action or by a
telescope identity. The current Term spine omits `R`; this is an implementation
defect to remove, not an implicit choice of a canonical relation.

The object-language identity judgement to implement is instead:

```text
delta2 : Id_Delta(delta0, delta1)
-----------------------------------------------------------
Id_(Delta.A)^delta2(a0, a1) type
```

Its finite introduction mechanisms are:

1. degeneracy `refl(a)`, derived as `ap_a` on the diagonal telescope;
2. congruence `ap_a(delta2)` for a typed term and an already selected
   telescope identity;
3. ordinary Lambda/constructor proof Terms inhabiting a computed identity
   family; and
4. at the Universe, `glue(R,b)` where `b` proves the transport/lifting or
   bisimulation contract for the selected correspondence `R`.

Reflexivity, symmetry, and transitivity are required consequences, but are not
the definition of Higher Observational Identity. An arbitrary equivalence
relation can have all three without supplying dependent transport, lifting,
or coherent higher action.

The word *observational* means that `Id` computes by the observed type former:
datatype identity is a datatype, function identity is pointwise, and so on.
It does not mean that the kernel searches every closing program Context. The
initial metatheoretic claim is only soundness with respect to the admitted
eliminators; completeness/full abstraction is not assumed.

### 1.2 Correction after the introduction-rule review

The kernel does not admit the rule:

```text
bounded observation search says that x and y behave alike
----------------------------------------------------------
                     p : Id A x y
```

Higher Observational Type Theory instead supplies a computational identity
family for each admitted type former. Introduction then means constructing an
ordinary object Term that checks against the computed family:

- `refl(a)` is degeneracy and computes structurally;
- `ap_a(delta2)` is the higher action of an already typed term;
- a datatype identity is inhabited by constructors of the computed higher
  datatype;
- a function identity is inhabited by an ordinary Lambda checked against the
  computed pointwise function type; and
- a non-diagonal Universe identity is introduced by `glue(R,b)`, where `b`
  supplies the required bisimulation/lifting structure.

Consequently, symmetry and transitivity are required derived operations, but
checking them as properties of an arbitrary relation does not turn that
relation into identity. The decisive HOTT contract is type-directed identity
computation together with coherent transport/lifting in every dimension.

The compiler may run bounded proof synthesis as an elaboration or optimization
service. Such synthesis is not a kernel introduction rule. On success it must
produce an ordinary proof Term that the kernel can check independently; on
budget exhaustion it produces a residual result, never an identity witness.

### 1.3 Final adjudication: what makes this identity rather than a relation

Reflexivity, symmetry, and transitivity are necessary tests, but they are not
sufficient criteria. An arbitrary equivalence relation has all three. A
relation is admitted as Higher Observational Identity in A Program only when
all of the following authority chain is present:

1. the relation family is computed canonically from the selected type former
   and the exact telescope identity, rather than supplied by relation search;
2. degeneracy and typed term action compute into that exact family;
3. dependent elimination is justified by transport/lifting operations that
   compute coherently at higher identity dimensions; and
4. every public witness is an ordinary Term with an independently replayable
   Claim and Derivation against the computed family.

The current implementation satisfies only a closed part of this chain. For an
ordinary closed ADT it computes an indexed identity datatype. Canonical source
constructors have degeneracy witnesses built from constructors of that
datatype. Pure functions use a computed pointwise family, and the Bool litmus
has an explicit Lambda/Match inhabitant between two non-convertible functions.
These are valid introductions for that finite fragment; they are not evidence
that the old four-place `OBSERVATION_TYPE` is identity.

Implementation naming is frozen accordingly. Until transport/lifting and the
selected Universe correspondence are executable, code and artifacts may claim
only a *closed homogeneous observational-identity computation fragment*. They
must not claim that arbitrary `Obs` inhabitants, relation preservation, or the
whole A Program kernel implement Higher Observational Type Theory. The
parametricity action remains useful compiler infrastructure, but it has no
object-identity authority by itself.

The introduction boundary is therefore:

| Candidate evidence | Kernel identity introduction? | Reason |
| --- | --- | --- |
| generated identity constructor checked at its indexed family | yes, in the admitted ADT fragment | ordinary object inhabitant of type-former-computed identity |
| Lambda checked at a computed pointwise Pi identity | yes, in the admitted pure-Pi fragment | extensional object proof, not pointer or syntax equality |
| generated Match/APP composition of such inhabitants | yes, once its ordinary typing derivation replays | ordinary elimination and congruence inside the object theory |
| old `OBSERVATION_WITNESS(left,right)` | no | endpoint token for the parametricity substrate; it omits the selected identity family |
| success of bounded contextual testing | no | useful synthesis heuristic, but not replayable proof authority |
| reflexive/symmetric/transitive arbitrary relation | no | equivalence structure alone supplies neither type-directed computation nor dependent lifting |

### 1.4 Implementation freeze gate after the identity-introduction review

The word `observation` must not be interpreted as bounded testing of the two
endpoints. The selected calculus computes identity from the observed *type
former*. Its primitive higher action has the shape:

```text
delta2 : Id_Delta(delta0, delta1)
Gamma |- a : A
----------------------------------------------------------
ap_a(delta2) : Id_(Delta.A)^delta2(a[delta0], a[delta1])
```

Homogeneous identity and reflexivity are specializations to the empty
telescope. Thus `refl(a)` is not introduced because the normalizer compared
`a` with itself, and it is not an endpoint-only witness token. It is the
degenerate instance of the same typed action that must also act coherently on
non-empty telescope identities.

Implementation must stop before persistent A1 publication unless all of the
following distinctions remain mechanically enforced:

1. `RELATION_TYPE`/`RELATION_TERM` may implement internal parametricity, but
   cannot authorize an object identity Claim.
2. `IDENTITY_TYPE_COMPUTATION` must be indexed by the exact endpoint
   substitutions and selected telescope identity. The empty-telescope fragment
   may erase this data only because its witness is uniquely fixed by the
   certified diagonal specialization.
3. `refl` and `ap` must produce ordinary Terms checked against the exact family
   returned by identity computation. Conversion or bounded observation search
   cannot manufacture the Claim.
4. Symmetry and composition must be derivable ordinary operations and tested,
   but their existence does not by itself validate a candidate relation as
   identity.
5. Function identity is pointwise identity and datatype identity is computed by
   its constructors. Universe identity must select a proof-relevant
   correspondence satisfying the required equivalence/lifting contract.
6. Proofs remain proof-relevant. Applying identity computation to an identity
   type must not collapse all witnesses to a singleton.

The current implementation is therefore named the **closed homogeneous
observational-identity computation fragment**. The following statements remain
forbidden until the corresponding theory and replay rules exist:

```text
the old four-place OBSERVATION_TYPE is Eq
successful relation search introduces equality
the current fragment implements complete Higher Observational Type Theory
contextual equivalence is the kernel definition of identity
```

The name is provisional at the fragment boundary. It means that the implemented
identity families follow the HOTT type-former equations for the tested closed
forms; it does not mean that the old relation action characterizes contextual
equivalence. Before widening the claim, the implementation must keep both
directions separate:

```text
object identity witness -> indistinguishable by admitted eliminators
    required adequacy/soundness obligation

contextually indistinguishable -> object identity witness
    optional completeness/full-abstraction obligation, not a kernel rule
```

The minimum executable acceptance matrix before extending `refl`/`ap` is:

- a non-convertible pointwise-equal Bool function pair has an explicit Lambda
  witness checked against the computed Pi identity;
- the identity and constantly-false Bool functions do not share that witness;
- generated ADT identity discriminates distinct constructors;
- the same proof Terms replay after all HOTT planner/action state is discarded;
- no accepted object proof contains `OBSERVATION_WITNESS_FORMER`.

This gate does not require completeness with respect to contextual equivalence.
Adequacy is a metatheoretic obligation: accepted identity evidence must be
sound for every admitted A Program eliminator. Full abstraction is a stronger
future result and is not assumed by the kernel.

For example, if `f` is Bool identity and `g` is its non-convertible exhaustive
Match implementation, identity introduction does not consist of observing the
two closed truth-table results and asserting equality. It consists of checking
an ordinary pointwise proof Term:

```text
(b0 : Bool) -> (b1 : Bool) ->
(bR : IdBool(b0,b1)) -> IdBool(f(b0),g(b1))
```

The body eliminates `bR`; its two constructors refine both endpoints to the
same Bool constructor and return the corresponding generated identity
constructor. Conversely, the same construction fails for the constantly-false
function in the `true` fiber. This is the first non-definitional introduction
test. It establishes discrimination for the closed Bool fragment, not a
general contextual-equivalence decision procedure.

Accordingly, A1 remains gated on generic degeneracy and term action for every
persisted root former. It is additionally gated on type-directed identity of
the resulting identity families themselves: preserving proof Terms as endpoint
IDs is not the same as computing their higher identity. The generated ADT path
now admits the exact nullary fiber `IdBool(false,false)` as a source and
computes ordinary identity and degeneracy for its proof Term. This is the first
higher base case, not general indexed lifting. Field-bearing indexed
constructors still require lifting and must therefore precede symmetry and
composition as the next theory step. Transport/lifting and Universe
correspondence remain required before extending the claim beyond the closed
homogeneous fragment.

## 2. Meaning of Higher Observational Identity

For A Program, Higher Observational Identity is an object-language,
type-directed and proof-relevant identity family with these properties:

1. identity is distinct from TermDB sharing and kernel conversion;
2. the identity family computes according to the observed type former;
3. its inhabitants are ordinary object terms and can themselves be endpoints
   of another identity family;
4. dependent terms can be transported/lifted along identity evidence;
5. all admitted eliminators preserve the selected relation;
6. equality of types retains the correspondence explaining how their
   inhabitants are related; and
7. object identity never extends global kernel conversion implicitly.

"Observational" does not mean enumerating every program context during proof
search. The kernel admits type-directed introduction rules whose soundness
obligation is that every observable eliminator preserves the relation.
Contextual equivalence is an adequacy result, not the introduction algorithm.

"Higher" does not mean every type has non-trivial paths at every dimension.
Bool may be a set whose identity types are propositions. Higher structure must
remain available at the Universe, user-defined HITs, and identity types whose
witnesses are not truncated.

### 2.0 Selected calculus, not a paper-name label

The A Program target is the following explicit combination:

1. the telescope-indexed `Id` and `ap` judgements of *Towards Higher
   Observational Type Theory*;
2. definitional computation of identity families, degeneracy, and term action
   by each admitted type/term former;
3. Narya-style `trr`, `trl`, `liftr`, and `liftl` structure, including its
   higher-dimensional instances;
4. Universe introduction through a proof-relevant correspondence plus an
   `isBisim`-strength contract; and
5. A Program's shared TermDB, explicit Context/Substitution graph, and
   budgeted residual execution policy.

Items 1--4 define the intended HOTT fragment. Item 5 is its A Program
realization. The 2022 extended abstract explicitly describes itself as work in
progress, so citing its name is not a substitute for specifying these rules.
Likewise, implementing only the common parametricity substrate used by Narya
does not implement HOTT.

CBPV computations and algebraic effects are not silently covered by the
value-type rules. Until identity computation and lifting are specified for
`Comp(E,A)`, operation requests, and computation folds, those forms are
residual at the HOTT boundary.

### 2.1 The governing judgement is telescope-indexed

The TYPES 2022 calculus does not begin with an arbitrary four-place relation.
Relative to an ambient Context, it has:

```text
delta0, delta1 : Delta
delta2 : Id_Delta(delta0, delta1)
A : Type over Delta
a0 : A[delta0]
a1 : A[delta1]
------------------------------------------------
Id_(Delta.A)^delta2(a0, a1) : Universe
```

Thus the relation between the fibers `A[delta0]` and `A[delta1]` is selected
by `delta2`, the identity between the endpoint substitutions. It is not
reconstructed from `A0`, `A1`, `a0`, and `a1` alone.

Every typed term preserves this identity:

```text
a : A over Delta
delta2 : Id_Delta(delta0, delta1)
------------------------------------------------
ap_a(delta2) : Id_(Delta.A)^delta2(a[delta0], a[delta1])
```

Homogeneous identity and reflexivity are special cases at the empty
telescope:

```text
Id_A(a0, a1) := Id_(empty.A)^unit(a0, a1)
refl_A(a)     := ap_a(unit)
```

This is the target interpretation for A Program. The existing Context bridge
and term action resemble `delta2` and `ap`, but currently do not establish
their identity rules.

### 2.2 Three constructions that must not be conflated

1. `Rel_A(a0,a1)` generated by the current parametricity translation is a
   logical-relation value.
2. `Id_(Delta.A)^delta2(a0,a1)` is the object identity family determined by a
   telescope identity.
3. At the Universe, an inhabitant of `Id_U(A0,A1)` contains a proof-relevant
   correspondence and evidence that it is an equivalence/bisimulation.

The relational action is implementation scaffolding for object identity.
Selected correspondence payload is how two different type-level
correspondences, such as the two Bool/Two bijections, remain distinguishable.
An arbitrary user relation does not become type identity merely by being
supplied to the kernel.

## 3. Three Separate Equalities

### 3.1 Representation equality

Two graph references denote the same interned TermDB node. This is sharing of
calculation syntax and has no independent proof term.

### 3.2 Definitional equality

Two terms convert under an explicit pure normalization profile and budget.
This is a kernel judgement. Exhaustion is residual, not inequality.

### 3.3 Object identity

An ordinary term inhabits a type-directed identity family. It can be named,
passed, eliminated, transported over, and compared at the next dimension.
Producing it never unions its endpoints in DefEq.

## 4. Two Current Object-Form Defects

### 4.1 The endpoint-type relation is missing

The current family is:

```text
OBSERVATION_TYPE(A0, A1, x0, x1)
```

It records endpoint types and values, but not the relation connecting the
types. This fails the established Bool/Two example, which has at least two
equivalences:

```text
true <-> zero, false <-> one
true <-> one,  false <-> zero
```

The same four arguments cannot select a correspondence. The kernel must not
select one from nominal names or shared core shape.

### 4.2 Endpoint-only witness syntax collapses object proofs

The current witness is:

```text
OBSERVATION_WITNESS(x0, x1)
```

TermDB interning gives every construction with the same endpoints the same
Term ID. Multiple Derivations can justify one Claim, but Derivations are
meta-level authority and are not object terms. Two distinct proofs `p` and `q`
of common endpoints therefore cannot become distinct endpoints of a higher
identity. The previous claim that multiple Derivations preserve higher
witnesses is false.

## 5. Selected Representation

### 5.1 Relational actions use the existing dependent pure-family graph

A relation between value types needs no new HOTT-specific TermDB tag. At the
calculus level it has the dependent-function shape:

```text
R : Pi(x0 : A0). Pi(x1 : A1). Universe
```

Internally, A Program already represents a Pi codomain family as:

```text
THUNK(LAMBDA(x, RETURN(body)))
```

The binary relation uses two nested pure families. Its specialization is typed
Context substitution/reindexing, not an untyped direct double APP. A direct
double APP would return an intermediate computation in dCBPV and would require
an implicit BIND to obtain the inner function.

The provisional relational type action for `A` produces `R_A`, exact typing
evidence, and the endpoint substitutions explaining its scope. Context
bridges retain this generated family, not merely two endpoint types. This is
the representation needed to test the `ap` fragment; it does not by itself
promote `R_A` to identity.

This follows the A Program rule that concepts expressible by the shared
APP/LAMBDA/MATCH graph are not duplicated as value/computation syntax tags.

### 5.2 Homogeneous identity is derived from empty-telescope identity

For a well-formed closed `A`, the empty-telescope specialization is intended
to compute to a diagonal relation:

```text
IdRel(A) : Pi(x : A). Pi(y : A). Universe
Id A x y := specialize(IdRel(A), x, y)
```

The generated relation is calculated from the admitted type-former rule. It is
not an arbitrary relation and is not inferred from the name of `A`. This
equation becomes normative only after the finite type-former rules and their
transport laws have been checked.

### 5.3 Type identity retains a correspondence

At the Universe the intended semantic shape is:

```text
TypeId(A0, A1)
  ~= Sigma(R : A0 -> A1 -> Universe). IsBisimulation(R)
```

`IsBisimulation` denotes the exact coherent equivalence/lifting condition
selected by the Universe phase. It must derive transport in both directions
and remain proof-relevant. Sigma here specifies semantics; it need not become
a primitive TermDB tag because A Program intends to define Sigma as a dependent
ADT.

Given `e : TypeId(A0,A1)`, element identity uses its selected relation:

```text
RelAlong(e, x0, x1) : Universe
```

This distinguishes the two Bool/Two correspondences.

### 5.4 Finite identity families use existing graph forms

A Program does not add one Term tag per identity computation rule. The finite
implementation uses the existing graph forms as follows.

For an ordinary ADT carrier `T`, prior computation creates an anonymous,
compiler-generated indexed declaration in `TypeDeclarationDB`:

```text
IdT : Pi(x0:T). Pi(x1:T). Universe
```

The endpoint telescope is an **index telescope**, not the uniform parameter
telescope of `T`. This distinction is semantic. For Bool, the higher
constructors are:

```text
trueR  : IdT true true
falseR : IdT false false
```

They do not quantify over arbitrary `x0 x1` before returning these specialized
results. Therefore `TypeDeclarationDB` must represent uniform parameters and
indices separately before the generated declaration is accepted. Encoding
the two endpoints as ordinary parameters would be a migration shortcut that
breaks the indexed eliminator contract.

For a nullary Bool-like declaration it has ordinary generated constructors:

```text
trueR  : IdT true true
falseR : IdT false false
```

For a constructor with fields, its higher constructor telescope contains the
left fields, right fields, and ordinary identity proofs for corresponding
fields. Its result classifier is `IdT (C left-fields) (C right-fields)`. The
initial implementation admits nullary and non-dependent field telescopes.
Dependent later-field classifiers remain residual until lifting along earlier
field identity has been implemented; silently treating the two fibers as one
classifier is forbidden.

Generated identity declarations are not surface nominal types. They therefore
carry explicit generated-origin metadata and source carrier provenance instead
of fabricated Symbol IDs. If rooted, artifact publication relocates this
provenance and declaration graph exactly. Allocation order or a generated name
must not define identity.

For a pure Pi carrier, identity computes directly to the existing dependent
`PI`/pure-family graph. Its inhabitants are ordinary `LAMBDA` Terms. No
generated Pi-specific declaration and no value-side/computation-side duplicate
tag is introduced.

This choice has two consequences:

1. two proofs with common endpoints remain different when their ordinary
   constructor/Lambda payload differs; and
2. existing constructor telescope, substitution, normalization, Claim, and
   Derivation machinery remains authoritative instead of being reimplemented
   in a parallel equality database.

## 6. Introduction Rules

Introduction is type-directed object-term construction, not a Boolean
comparison or unrestricted proof search. There are three distinct sources of
evidence:

- `ap_a(delta2)` introduces identity preservation for a typed term;
- homogeneous `refl(a)` is the empty-telescope instance of `ap`;
- non-reflexive identity proofs are ordinary terms built using the computed
  identity structure, for example pointwise Lambdas for functions or a
  Universe correspondence plus equivalence evidence for type identity.

The kernel must not introduce arbitrary observational equivalence merely by
running a contextual-equivalence search.

There are therefore two deliberately separate APIs:

```text
kernel:     compute Id A x y, then check p : Id A x y
elaborator: optionally search for p, then submit p to the kernel
```

Only the first API defines the calculus. The second is replaceable,
fuel-bounded compiler policy.

### 6.1 Reflexivity

```text
Gamma |- a : A
-----------------------------
Gamma |- refl(a) : Id A a a
```

The diagonal term action is a candidate implementation of `ap_a(unit)`. It
must construct the proof term recursively and must not be represented solely
by a Derivation of an endpoint-only witness token.

### 6.2 Functions

In the homogeneous non-dependent case:

```text
Id (Pi(x:A).B) f g
  ~~> Pi(x:A). Id B (APP(f,x)) (APP(g,x))
```

The displayed shorthand is valid only when the domain and codomain are
constant enough to identify the two boundary arguments. The governing rule
is the higher dependent rule:

```text
Id (Pi(x:A).B(x)) f0 f1
  ~~>
Pi(x0:A). Pi(x1:A). Pi(x2:Id A x0 x1).
  Id_(B)^x2(APP(f0,x0), APP(f1,x1))
```

Here `Id_(B)^x2` is selected by the higher action of the codomain family `B`
on `x2`. It is not reconstructed from the two endpoint classifier Terms. Its
proof is an ordinary Lambda over the two boundaries and their identity
witness.

Thus these non-convertible functions can have an extensional identity proof:

```text
f = lambda b. b
g = lambda b. match b { true -> true; false -> false; }
```

The proof body eliminates the computed Bool identity. A generic endpoint token
or success bit from relation search is not sufficient.

This is the mandatory non-trivial introduction test. Let:

```text
f = lambda b. b
g = lambda b. match b { true -> true; false -> false; }
```

After function identity computes pointwise, an explicitly constructed proof is
an ordinary Lambda that
accepts related Bool endpoints and their Bool-identity witness. Eliminating
that witness leaves only the `true/true` and `false/false` constructor cases;
each result is introduced by the corresponding nullary constructor identity.
The kernel checks this Lambda; it is not required to discover it from the two
function endpoints. No conversion premise between `f` and `g` is permitted in
this test. Conversely,
`identity` and `alwaysFalse` have no such proof because the `true/true` input
case would require an inhabitant of the empty `true/false` Bool identity.

This example separates three facts that the previous implementation mixed:

- a term preserves a logical relation;
- two distinct functions inhabit a pointwise identity family; and
- those functions are contextually indistinguishable by admitted observers.

The first is implemented scaffolding. The second is the object proof to add.
The implication from the second to the third is the adequacy obligation.

The positive object-proof test is now executable in `hott_goal_check.c`.
It establishes all of the following separately:

1. the two endpoint functions are not definitionally equal under the pure
   type-normalization profile;
2. Pi identity computes to the nested pointwise identity classifier;
3. the witness is an ordinary nested Lambda/Match/constructor Term, not an
   endpoint-only observation token; and
4. its accepted Claim and Derivation DAG pass read-only kernel replay.

This is evidence for the function fragment only. It does not close the
Universe identity or dependent transport/lifting obligations, and therefore
does not justify calling the whole implementation a completed HOTT.

### 6.3 Ordinary ADTs

Identity computes by constructor and field structure:

- equal nullary constructors produce an inhabited unit-like relation;
- distinct constructors produce an empty relation;
- equal constructors with fields produce the dependent product of field
  identities, reindexed along earlier field evidence.

Unit/Empty for a set-level ADT does not make the entire theory
proof-irrelevant. It says this particular identity type is a proposition.
Witnesses are ordinary constructors and field witnesses.

For a constructor with dependent fields, the identity classifier of a later
field is selected by the higher action of that field's classifier family on
all earlier endpoint fields and identity fields. Merely comparing or
normalizing the two later endpoint classifier Terms loses this dependency and
is not the HOTT rule.

### 6.4 Match and computations

Match preserves identity when its scrutinee and corresponding branches
preserve selected relations. It is not a rule equating arbitrary Match nodes.

The initial CBPV fragment has one admitted computation action:

- `RETURN(v)` is related through `v`;
- `THUNK(M)` is related through an admitted observation of `M`;
- a pure `APP(FORCE(f),x)` acts on `f` and `x`, then applies the pointwise
  function-identity witness to the two endpoints and their identity witness;
- intermediate results of that three-stage application are sequenced by
  ordinary zero-clause `COMPUTATION_FOLD` Derivations;
- unresolved/effectful computations are residual;
- operation requests and handler/fold observation wait for effect semantics.
  The zero-clause folds above are only pure sequencing, not an effect-handler
  identity rule.

### 6.5 The exact boundary between parametricity and HOTT

The shared observational substrate supplies `Id`-shaped families, degeneracy
(`refl`), and term action (`ap`). This alone can be interpreted as internal
parametricity: a type identity at the Universe is then merely a
correspondence `A0 -> A1 -> Universe`. Such a correspondence need not support
symmetry, composition, or transport and must not be called equality solely
because its notation is `Id`.

HOTT adds a fibrancy contract. For a selected type identity
`e : TypeId(A0,A1)`, the kernel must obtain at least:

```text
trr(e)   : A0 -> A1
liftr(e) : Pi(x0:A0). RelAlong(e, x0, trr(e,x0))
trl(e)   : A1 -> A0
liftl(e) : Pi(x1:A1). RelAlong(e, trl(e,x1), x1)
```

Higher-dimensional instances of the lifting fields provide the coherence
needed to derive groupoid operations and `J`. This is the property that turns
the observational correspondence into identity; reflexivity, symmetry, and
transitivity checked only as isolated operations would still be insufficient.

Accordingly, introduction has three distinct forms in the finite A Program
fragment:

1. `refl(a)` is degeneracy and is calculated by the identity structure of
   `A`;
2. `ap_a(delta2)` maps an already selected telescope identity through a typed
   term;
3. `glue(A0,A1,R,b)` introduces a non-diagonal identity in the Universe only
   when `b : IsBisimulation(A0,A1,R)` supplies transport and lifting.

For element identity, the kernel then computes
`IdAlong(glue(R,b),x0,x1)` to the selected `R x0 x1`. It does not search all
program contexts and it does not promote an arbitrary user relation. For
functions and ADTs, non-reflexive proofs are ordinary Lambda/constructor
terms inhabiting the computed identity family. Failure of bounded synthesis
is residual; only a type computation to an empty observation justifies
rejection.

This separation follows the current primary implementation evidence as well:
Narya exposes a parametricity mode where Universe identity is a
correspondence, and a HOTT mode where transport/lifting and `glue` with an
`isBisim` witness are added. A Program uses the same conceptual split while
retaining its shared TermDB and budgeted prior-computation policy.

## 7. Elimination and Transport

Reflexivity and congruence alone do not define identity. The finite fragment
must provide lifting with this semantic contract:

```text
e : TypeId(A0,A1)    x0 : A0
----------------------------
transport(e,x0) : A1
```

Dependent transport acts on the family relation and computes on diagonal
evidence. The final surface spelling may be `J`, `transport`, `coe`, or a more
primitive lifting operation; the kernel rule and computation law are mandatory
before this relation is called identity.

Symmetry and composition should be object functions derived from the selected
relation/equivalence structure where possible, not C-only graph rewrites.
Their existence is a theory test, but is insufficient as the definition:
the universal relation is reflexive, symmetric, and transitive too.

## 8. Higher Closure

Proof relevance requires:

```text
p : Id A x y
q : Id A x y
Id (Id A x y) p q
```

to retain `p` and `q` as potentially different object Terms. Different
Derivations of one Claim do not meet this requirement.

`OBSERVATION_WITNESS(left,right)` must therefore be retired as the universal
proof representation. Where identity computes to Pi/ADT/Universe structure,
its witnesses are ordinary Lambda/constructor/equivalence terms. If a
temporary primitive witness is necessary, its graph key contains proof
payload, not only endpoints.

## 9. Adequacy Boundary

The initial soundness claim is:

```text
p : Id A x y
```

implies that every admitted, typed eliminator/observer for `A` maps `x` and
`y` to related observations. Bool is observed by Match, functions by APP, and
user ADTs by their declared eliminators. Effectful computations require
handlers and host observations before an adequacy claim.

T0 does not claim the converse, completeness, or full abstraction with
respect to every future A Program context. In particular, failure of the
current bounded action to synthesize an inhabitant means `residual` or
`unsupported`; it is not a proof that the identity type is empty.

## 10. Executable Litmus Matrix

| Case | Expected result |
| --- | --- |
| `Id Bool true true` | inhabitant constructed |
| `Id Bool true false` | no inhabitant constructed by Bool rules |
| `Id (Bool -> Bool) identity identityByMatch` | inhabitant without endpoint conversion |
| `Id (Bool -> Bool) identity alwaysFalse` | no inhabitant; `true` discriminates |
| two proofs with common endpoints | distinct Terms unless proof payloads are DefEq |
| identity between identity proofs | accepted without proof irrelevance |
| Bool/Two with two equivalences | correspondence selected by type-identity witness |
| unsupported effectful endpoints | residual, not uninhabited |

The positive function test first asserts that its endpoints are not equal
under the selected DefEq profile. Otherwise it tests conversion only.

## 11. Rule Correspondence Audit

| HOTT calculus object | Current A Program representation | Assessment |
| --- | --- | --- |
| ambient Context `Gamma` | `ContextDB` | retained |
| telescope `Delta` over `Gamma` | an ordered chain of `ContextDB` extensions | retained |
| endpoints `delta0`, `delta1` | `prototype_hott_bridge.left_substitution_id` and `right_substitution_id` | retained |
| telescope identity `delta2` | relation bindings in `bridge_context_id`, justified recursively by bridge certificates | structural candidate; not yet object identity |
| fiber identity `Id_(Delta.A)^delta2(a0,a1)` | `OBSERVATION_TYPE(A0,A1,a0,a1)` | insufficient: the Term omits `delta2`/selected correspondence |
| equality preservation `ap_a(delta2)` | Type/Term action requests and certificates | useful compiler action; result proof syntax is insufficient |
| homogeneous `Id_A(a0,a1)` | previously identified with four-place `OBSERVATION_TYPE` | withdrawn until empty-telescope specialization is implemented |
| `refl_A(a)` | diagonal Term action | valid construction strategy, but endpoint-only witness payload must be replaced |
| equality in `Universe` | unsupported/residual | must retain correspondence plus equivalence/bisimulation evidence |
| transport/lifting | absent | mandatory before calling the family identity |
| higher identity of proofs | reapplying current observation action | not established because endpoint-only witness Terms collapse proof payload |

The bridge record need not contain a single monolithic `delta2` Term. A
telescope identity is itself composed extension by extension. Nor should it
duplicate a `relation_binding_id`: `ContextDB` already gives the exact binding
identity of its final extension. The persistent contract is the exact tail
Context extension plus the bridge certificate linking it to the preceding
bridge and Type-action certificate. Reader replay must validate that chain;
shape-only recovery is insufficient.

This representation is sufficient only for the generated diagonal/term-action
fragment if every extension's relation family and relation-witness binding is
exactly retained. It is not sufficient for arbitrary non-diagonal type
identity while the selected Universe identity Term is absent. Such an identity
must eventually select the relation family through its object-level
correspondence payload, rather than through a compiler certificate alone.

## 12. Refactor Sequence

### 12.0 Planner/action split found by the T0 code audit

The code currently contains two only partially connected mechanisms:

1. `prototype_hott_observation_plan()` accepts distinct left and right Claims
   and can select ADT, Pi-pointwise, Return, Thunk, and conversion candidates;
2. `prototype_hott_observation_execute()` materializes an object Term action
   only when the selected candidate is `OBS_DIAGONAL`.

Consequently a non-convertible but extensionally related pair can be planned,
but its selected candidate cannot become an object proof Term. This is the
concrete implementation form of the parametricity/equality gap. It must not be
worked around by pretending that the diagonal action's endpoint token proves
an arbitrary selected candidate.

The authoritative kernel pipeline is:

```text
typed carrier and endpoints
  -> type-directed identity-family computation
  -> explicit object proof Term
  -> exact HAS_TYPE Claim and Derivation DAG
```

An optional compiler service may place a fuel-bounded candidate-DAG search
before the explicit proof Term, but its result has no authority until the
kernel checks that Term. The existing diagonal Term action remains a candidate
implementation of degeneracy for one source Term. Materialization between
distinct endpoints is an elaboration operation, not an unrestricted identity
introduction rule. Both use the same TermDB, ContextDB, SubstitutionDB, and
JudgementDB; no parallel identity database is introduced.

### T0.R1: Stop the false public interpretation

- [x] Describe current forms as relational actions in the manifest.
- [x] Keep artifact capability deferred.
- [x] Mark the old endpoint-only normative model superseded.

### T0.R2: Make the selected relation explicit

- [x] Give every immutable goal separate exact left and right carrier Claims;
      dependent fibers are no longer forced through one common classifier.
- [x] Add the relation family to Type-action results/certificates.
- [x] Preserve its selection through the Type-action certificate referenced by
      each Context-bridge extension.
- [x] Represent it through the existing nested dependent pure-family graph and
      specialize its body through typed Context substitution.
- [ ] Stop reconstructing relations from endpoint type IDs where multiple
      correspondences are possible.
- [ ] Index every generated fiber relation by the exact Context/telescope
      identity or bridge evidence that selected it.
- [ ] Keep arbitrary logical relations separate from `Id_U` correspondences
      carrying equivalence/bisimulation evidence.

### T0.R3: Replace endpoint-only proof tokens

- [x] Distinguish candidate results that construct a witness, compute an empty
      family, or defer object semantics. Relation-family planning success no
      longer implies witness synthesis success.
- [x] Generate finite same-constructor ADT field child goals with exact left
      and right argument and carrier Claims.
- [ ] Add candidate-specific object-term materialization for arbitrary
      left/right identity goals; do not route non-diagonal candidates through
      the one-source Term action.
- [ ] Recursively solve and materialize every selected child-goal edge before
      its parent candidate.
- [ ] Generate ordinary proof Terms from type-directed action.
- [ ] Retire `prototype_term_observation_witness(left,right)` from accepted
      proof construction.
- [ ] Keep Derivations as validators, not hidden object-proof identity.
- [ ] Verify common endpoints can retain distinct proof payloads.

The current non-diagonal conversion and same-constructor ADT execution paths
still construct `OBSERVATION_WITNESS(left,right)`. They are therefore
relational-scaffolding experiments only. Their `HAS_TYPE` Claims must not be
published as object-identity roots, and they do not complete any unchecked item
above. They must be replaced by ordinary candidate-specific proof Terms.

### T0.R4: Implement finite identity computations

- [x] Select ordinary generated indexed TypeDeclarations for ADT identity and
      ordinary Pi/Lambda graph forms for function identity.
- [x] Separate uniform parameter and index telescopes in TypeDeclarationDB;
      generated identity endpoints are indices, never synthetic parameters.
- [x] Add generated-origin/source-carrier provenance to TypeDeclarationDB.
- [x] Add a distinct identity-type-computation action kind; do not reuse or
      rename the parametric relation Type action as identity.
- [x] Bool/nullary ADT identity. The indexed family and diagonal higher
      constructors compute, the family receives an exact
      `TYPE_FORMATION_INTRO` certificate, an ordinary constructor Term inhabits
      `IdBool false false`, and the same constructor is rejected at
      `IdBool false true` by exact result-index validation.
- [x] nondependent constructor-field identity, including recursive fields.
      Each source field generates left, right, and identity-witness telescope
      entries. Dependent later fields remain blocked on lifting.
- [x] pure Pi pointwise identity. A Program Pi has a computation codomain. Its
      identity computes through `EqC(C,M0,M1) :=
      IdV(ThunkType(C),Thunk(M0),Thunk(M1))`; the executable Bool-function
      proof checks the resulting nested pointwise Pi family.
- [x] type-former-independent variable action and the first non-empty pure-Pi
      Lambda action. A Context variable uses the exact identity Claim stored by
      its object bridge even when its classifier has no generated ADT identity
      declaration. A typed Lambda over a constant pure Pi family composes the
      outer endpoint substitutions, reindexes inherited identity Claims below
      `x0/x1/xR`, and constructs an ordinary pointwise witness from its accepted
      THUNK/LAMBDA/RETURN Derivation.
- [x] immutable object term action authority. `OBJECT_TERM` is keyed by the
      exact source HAS_TYPE Claim, `OBJECT_IDENTITY` bridge, and prior
      `IDENTITY_TYPE_COMPUTATION` request. Its replayable certificate retains
      both reindexed endpoint Claims, the instantiated identity-family Claim,
      and the ordinary witness Term/HAS_TYPE Claim. It is distinct from the
      compiler-local parametric `TERM` action. The Universe-variable test uses
      this certificate before projecting the selected correspondence `R`.
- [x] pure raw-CBPV APP action inside a pointwise Lambda. For
      `THUNK(LAMBDA(x,APP(FORCE(f),x)))`, the function and argument actions are
      computed from accepted `FORCE_ELIM` and `APP_ELIM` premises. The
      pointwise function witness is applied to `x0`, `x1`, and `xR`; generated
      intermediate types receive structural formation DAGs, and zero-clause
      `COMPUTATION_FOLD_ELIM` sequences intermediate computations. Specialized
      convertible families are connected by an explicit `CONVERSION` Claim.
      General dependent codomain lifting, Match bodies, arbitrary computation
      bodies, and effects remain residual.
- [x] non-empty pure `THUNK(RETURN(v))` action. The action retains the CBPV
      boundary, recursively acts on the accepted Return premise under the outer
      object bridge, and constructs the ordinary generated thunk-identity
      constructor from the two values and their identity witness. Constant
      family reuse is accepted only for a generated declaration whose recorded
      source carrier is that exact pure Thunk type.
- [x] pure Return and Thunk identity. `ThunkType(Comp(empty,A))` computes to an
      ordinary generated indexed identity declaration whose higher
      constructor carries `v0`, `v1`, and `IdV(A,v0,v1)`.
- [x] Match preservation for the nullary indexed fragment required by the
      non-conversion Bool-function proof. General dependent-field and IADT
      refinement remains residual.
- [x] first higher action on a resulting nullary identity fiber. The ordinary
      proof `p : IdBool(false,false)` is accepted as an endpoint of
      `Id_(IdBool(false,false))(p,p)`, whose ordinary diagonal constructor is
      checked and persisted. Source constructor selection is recovered from
      exact endpoint spines rather than a second provenance authority.
- [x] higher action on identity declarations with nondependent source fields.
      Every source field expands to four corners, four edge identities, and a
      field-square witness. A `Box Bool` constructor-spine test exercises the
      resulting nine-field telescope. Dependent source fields still require
      lifting through preceding endpoint and identity fields; reusing
      `RELATION_*` or endpoint conversion remains an invalid shortcut.

#### Post-nullary implementation audit

The first executable identity computation now covers closed ADTs with nullary
or nondependent constructor fields. It deliberately does not reuse the
relation-family request. For a source `T`, it generates an ordinary indexed
declaration `IdT : T -> T -> Universe`; the endpoint telescope is represented
by `TypeDeclarationDB.index_context/index_count`. Each source constructor
`C(a)` gives an ordinary constructor whose field telescope contains
`a0`, `a1`, and `aR : IdA(a0,a1)`, and whose result is
`IdT(C(a0),C(a1))`. Recursive fields select the identity declaration currently
being generated. `Box Bool` and recursive `Nat.succ` exercise the same
algorithm. The resulting `boxR false false falseR` is checked by the ordinary
constructor-spine classifier, not a HOTT-specific witness checker.

Generated constructor result classifiers retain their generated TypeView.
Reducing them to source-only graph terms made the schema disagree with typed
use sites and incorrectly delegated the distinction to conversion.

The code audit also fixes the next boundary:

1. `PROTOTYPE_HOTT_RULE_OBS_PI_POINTWISE` remains only a relational planner
   candidate. It does not compute the identity of A Program's computation Pi
   or construct an ordinary Lambda inhabitant.
2. `pi_codomain_after_argument_in_context` performs ordinary substitution of
   one concrete argument into one codomain family. It cannot implement
   `Id_(B)^xR(B[x0],B[x1])`, because that family is selected by the identity
   witness `xR`, not by either endpoint alone.
3. The Context bridge already stores two endpoints and one relation binding,
   but that binding is currently certified only as the provisional
   parametricity relation. It cannot be reused as telescope identity authority
   until its classifier is the computed domain identity and its transport and
   lifting operations are available.
4. The current pure `RETURN`/`THUNK` relation rules preserve their child
   relation. They do not yet define identity computation or lifting for the
   CBPV `F/U` boundary. Effectful computations remain residual.

The implementation must not promote the existing Pi relation candidate. It
must first define identity for the computation codomain, then construct the
exact domain identity, extend the proof Context by `x0`, `x1`, and an ordinary
`xR` inhabitant, and compute the codomain computation identity selected by
`xR`. Genuinely dependent codomains additionally require lifting.

#### Indexed elimination is a prerequisite, not a later IADT feature

The mandatory `identity` versus `identityByMatch` proof exposes an additional
dependency. Its function-identity proof receives:

```text
x0 : Bool
x1 : Bool
xR : IdBool x0 x1
```

Eliminating `xR` through `falseR` and `trueR` must refine the branch Contexts
to the corresponding constructor results:

```text
falseR branch: x0 = false, x1 = false
trueR branch:  x0 = true,  x1 = true
```

The current Match kernel selects a constructor by declaration/ordinal and
extends field binders, but it does not solve and retain constructor-result
index constraints in each branch. Therefore the generated indexed family can
currently be introduced but not yet eliminated with its indices refined.

This is not optional general IADT convenience. It is required by the first
non-trivial HOTT function proof. The corrected implementation order is:

1. indexed identity-family formation and exact constructor introduction;
2. constructor-result index unification as explicit branch constraints;
3. branch Context refinement and a replayable Match Derivation containing the
   selected constraint solution;
4. elimination of `IdBool x0 x1`;
5. pure Pi pointwise identity and its ordinary Lambda inhabitant.

No branch may rewrite endpoint Variables merely by binder renaming or global
DefEq. The index solution is local proof authority owned by the Match branch.
Failure to solve a branch index is residual while the constraint procedure is
incomplete; a provably disjoint constructor result may remove that branch.

#### Indexed nullary Match implementation status

The generated-identity fragment now implements steps 2-4 for nullary higher
constructors. `prototype_judgement_delta_expand_match()` obtains the exact
Operation-owned Match occurrence, computes a branch-local CwF substitution,
and reindexes both the source branch Claim and motive branch through that
substitution. The accepted `MATCH_ELIM` premise edge carries the substitution
action. Replay recomputes the expected substitution from the selected
constructor result and rejects a branch carrying another constructor's
solution.

Publication preserves two distinct authorities on such a premise. The source
Operation identifies the syntactic Match branch, while the substitution edge
identifies the refined Context in which its Claim holds. Structural premise
reconstruction must not replace the latter Context with the source Operation's
Context. Doing so previously left a valid `MATCH_ELIM` candidate ungrounded;
the publication resolver now treats an explicit substitution action as the
authoritative reindexing and leaves its detailed validation to accepted replay.

Identity computation certificates now also have one uniform output boundary:
an endpoint Context `Gamma,x0:A,x1:A`, the exact endpoint binding identities,
the computed identity type in that Context, and its formation Claim. For the
ADT implementation the certificate separately retains the generated backing
type former and its formation Claim. This prevents the ADT implementation
detail, an unsaturated `IdT`, from becoming the semantic result contract and
allows Pi identity to return a computed pointwise type through the same
endpoint boundary.

The old `expand_match_with_branch_hints` proof path was removed because it
could bypass this authority by accepting caller-supplied branch classifiers.
The executable HOTT fixture checks both distinct `falseR`/`trueR` branch
substitutions and rejection of a forged swapped edge.

This closes indexed elimination only for the nullary generated-identity
fragment. It does not mark general Match preservation complete: constructors
with fields, recursive indexed families, and the ordinary Lambda inhabitant of
the Pi litmus remain pending.

### T0.R5: Add lifting and the Universe contract

- [ ] Implement diagonal lifting for the finite fragment.
- [ ] Define the type-identity/equivalence interface needed by Bool/Two.
- [ ] Keep general Universe, UA, HIT, and effect support residual.

### T0.R5a: Correct the CBPV identity boundary before Pi

The shared A Program `PI` is a computation-function classifier, not a second
value-side Pi:

```text
Gamma |-v A type       Gamma.A |-c C ctype
------------------------------------------------
Gamma |-c Pi(A,C) ctype
```

Consequently a fixture `Pi(Bool,Bool)` cannot stand in for the executable
function type whose codomain is `Comp(empty,Bool)`. An attempted implementation
did exactly this and was removed after the classifier-category audit. The
ordinary Pi-formation Context fix discovered by that attempt remains valid,
but no Pi identity capability was published.

The selected A Program direction keeps one object identity on values and uses
the existing CBPV adjunction to define computation equivalence:

```text
IdV(A, v0, v1) := value identity type
EqC(C, M0, M1) := IdV(ThunkType(C), Thunk(M0), Thunk(M1))
```

Thus computation equivalence is object-level and proof-relevant without a new
`EqC` Term tag or Judgement kind. Its endpoints are values, so ordinary type
dependency never receives a computation Term directly. The graph remains
shared; no value/computation copies of `APP`, `LAMBDA`, or `PI` are introduced.

For the first pure fragment, the intended equations are:

```text
EqC(Pi(A,C), f0, f1)
  computes pointwise over x0:A, x1:A, xR:IdV(A,x0,x1)
  to EqC(C, APP(f0,x0), APP(f1,x1))

EqC(Comp(empty,A), RETURN(v0), RETURN(v1))
  computes to IdV(A,v0,v1)
```

The `Comp(empty,A)` rule is represented as an indexed ordinary identity for
`ThunkType(Comp(empty,A))`: its higher constructor has fields `v0`, `v1`, and
`vR:IdV(A,v0,v1)`, with result indices `Thunk(Return(v0))` and
`Thunk(Return(v1))`. Closed pure endpoints may be exposed to that constructor
by bounded prior reduction. A neutral or exhausted endpoint remains residual.
Effectful rows require a selected handler/observation semantics and remain
unsupported.

This follows the standard CBPV separation rather than inventing a value Pi.
Forster et al. define value relation `V[U C]` through computation relation
`E[C]`, computation relation at `F A` through returned related values, and
computation relation at `A -> C` pointwise. Vakar's dCBPV likewise restricts
ordinary dependency to values and adds dependent Kleisli extension only in the
stronger dCBPV+ system. These sources justify the sorted boundary, but the
proof-relevant higher identity and budgeted residual realization above are A
Program design obligations, not claims imported from either paper.

Implementation gate:

- [x] Reuse the existing computation observation category as compiler work;
      object evidence is always `IdV(ThunkType(C),Thunk(M0),Thunk(M1))`.
- [x] Generate and replay the indexed `RETURN` higher constructor for
      `ThunkType(Comp(empty,A))`.
- [x] Complete pointwise `EqC` for pure Pi and the weaker constant-function
      litmus. The
      direct pointwise identity family for nondependent
      `ThunkType(Pi(A,Comp(empty,B)))` is generated and replayed without
      allocating during replay. An explicit ordinary nested-Lambda witness is
      accepted between the constant-true Bool function and its non-DefEq
      exhaustive-Match implementation. Reuse at `alwaysFalse` is rejected.
- [x] Complete the focused identity Bool function versus eta-expanded Match
      litmus by eliminating the arbitrary input `IdBool` witness. Its local
      accepted graph is read-only replayable.
- [x] Move the same proof into the authoritative compilation Judgement and
      persist it as an exact A1 identity root. The fixture no longer allocates
      an isolated proof arena.
- [x] Route pure-Lambda degeneracy through the accepted typed body Claim and a
      recursive value action. Both the identity body and a constructor-valued
      constant body now build ordinary pointwise identity witnesses; the Core
      Lambda's alpha-interned binder/body pair is not used as typing authority.
- [ ] Keep non-empty and unresolved effect rows residual until handler-indexed
      observation is selected.

The completed `THUNK_RETURN` step is deliberately narrower than contextual
equivalence. It establishes the object introduction rule

```text
vR : IdV(A,v0,v1)
---------------------------------------------
returnR(v0,v1,vR) :
  IdV(ThunkType(Comp(empty,A)),
      Thunk(Return(v0)), Thunk(Return(v1)))
```

as an ordinary indexed constructor and ordinary constructor-spine derivation.
Certificate replay checks the exact three-field telescope and both result
indices. It does not infer identity between arbitrary computations, and it
does not make the compiler-local parametricity witness into object evidence.

### T0.R6: Execute the finite-fragment litmus matrix

- [x] Add positive and negative kernel tests for generated Bool and pure-Pi
      identity families.
- [x] Preserve proof payload as ordinary Terms rather than endpoint-only
      witness records.
- [x] Assert that the positive function endpoints are not DefEq under the
      selected profile before accepting their identity inhabitant.
- [x] Run the full prototype suite before reopening A1 wire work.

This does not prove completeness or full abstraction for contextual
equivalence. It establishes an executable, type-directed identity fragment and
its sound introduction rules. Effectful observation, arbitrary Universe
correspondence, transport, symmetry, composition, and their higher coherence
remain explicit later obligations.

### T0.R7: Revalidation of identity introduction

The 2026-08-10 review asked whether reflexivity, symmetry, transitivity, and
left/right relation preservation would already be enough to call the current
family observational equality. They are not. Those operations are mandatory
derived tests, but any proof-relevant equivalence relation may have them.

The selected HOTT introduction contract is instead computational and
type-directed:

```text
A type, x0:A, x1:A
-------------------
Id A x0 x1 type

x:A
-------------------
refl x : Id A x x

Gamma |- t : A
delta : IdContext(Gamma0,Gamma1)
---------------------------------
ap_t(delta) : Id A(delta) t0 t1
```

`Id A x0 x1` computes by the type former of `A`. For an ordinary datatype it
is the generated indexed identity datatype; for a pure function it is the
pointwise identity family. Therefore the non-convertible Bool identity
implementations are related only when an explicit ordinary Lambda/Match Term
inhabits that pointwise family. A successful endpoint comparison or the old
compiler-local `OBSERVATION_WITNESS` is not an introduction rule.

At the Universe, a higher object `R : Id Type A B` must instantiate to a
correspondence `R a b`. In HOTT, unlike unrestricted internal parametricity,
that correspondence must additionally support the transport and path-lifting
operations that make it a bisimulation. This is the decisive missing piece in
the current A Program fragment. It is also exactly what is needed to select
the identity of a dependent constructor field such as `b : B(a)` from the
preceding field identity `aR`, rather than comparing `B(a0)` and `B(a1)` as
endpoint classifier Terms.

Accordingly:

- the generated ADT and pure-Pi families are retained as a closed homogeneous
  object-identity fragment;
- symmetry and composition must be constructed and tested inside that
  fragment, but do not by themselves complete HOTT;
- the next theory implementation is Universe correspondence plus
  transport/lifting, followed by dependent telescope action;
- contextual equivalence is a metatheoretic adequacy target, not a kernel
  proof-search definition of `Id`; and
- artifact v70 remains a prototype fixture and must not be promoted as a
  complete HOTT identity ABI.

Implementation update 2026-08-10: the first finite Universe case is now an
ordinary generated indexed ADT. Its constructor carries a relation, two
transport maps, and two selected fiber witnesses. A diagonal instance for
`Bool` uses the computed `Id_Bool` family and ordinary generated identity
witnesses; both the generic family and the concrete witness survive canonical
v70 write/read/append. This closes representation and persistence only. It
does not yet close generic elimination of those fields, dependent telescope
lifting, or higher coherence, so the conclusion above remains unchanged.
Generic elimination exposed a concrete prerequisite: accepted Match replay can
currently refine indexed generated identities only for nullary constructors.
Projecting `trr`, `trl`, `liftr`, or `liftl` from the seven-field `glue`
constructor requires one substitution from the branch field telescope to the
outer endpoint/proof Context. The implementation must generalize that indexed
Match substitution and reuse it for dependent telescope action; it must not
introduce a privileged C-side field projection for Universe identity.

This matches Narya's explicit separation between parametricity mode and HOTT
mode: both have higher-dimensional families, `refl`, and term action, while
HOTT additionally equips type correspondences with transport and lifting.
Narya's documentation also states that its broader multi-directional theory is
not yet a single completed formal specification. A Program therefore uses the
published rules as constraints but remains responsible for specifying the
CBPV/effect interaction it adds.

### T0.R8: What may introduce observational identity

The word "observational" does not license the kernel to enumerate observations
and turn a successful finite test into an identity proof. Three notions must
remain separate:

1. A compiler-local logical relation may be inhabited because a Term preserves
   a selected relation. This is the role of the compiler-local `RELATION_*`
   substrate, formerly named `OBSERVATION_*`.
2. Contextual equivalence states metatheoretically that no admitted program
   context distinguishes two Terms. This is an adequacy target and is not an
   object-language introduction rule.
3. Object identity is introduced only by checking an ordinary proof Term
   against the exact family computed by `IDENTITY_TYPE_COMPUTATION`.

Consequently, the Bool identity function and its explicit Bool Match
implementation are accepted because an ordinary pointwise Lambda/Match proof
inhabits the computed Pi identity family. The focused fixture constructs this
exact witness; it is not inferred by endpoint sampling. The independent
constant-function test additionally shows that its positive proof cannot be
reused at a constant-false classifier and that no identity is introduced by
endpoint comparison. These tests do not prove that every possible Term fails
to inhabit a negative family. Such uninhabitance requires a
canonicity/normalization theorem for the admitted fragment.

This gives a precise but deliberately limited status:

- the current generated-ADT and pure-Pi implementation is a closed,
  type-directed observational-identity computation fragment;
- it is stronger than merely recording compiler-local parametricity evidence;
- it is not complete HOTT until Universe correspondence supplies transport and
  path lifting, dependent telescopes use that lifting, and the construction is
  closed coherently under higher identity; and
- symmetry and composition are required object operations and tests, but their
  existence alone cannot close this gap.

The implementation must therefore grow from type-former computation outward.
It must not add a generic `Obs` inhabitance oracle and later reinterpret its
answers as equality.

### T0.R9: Universe fiber computation is the next theory gate

The first dependent telescope step must have the following form. For a source
Context

```text
Gamma, A : Universe, b : A
```

an object bridge for `Gamma, A` contains an ordinary witness
`pA : IdU(A0,A1)`. Eliminating `pA` projects its relation field
`R : A0 -> A1 -> Universe`. Identity computation for the following fiber does
not run the compiler-local relation action on `b` and does not compare `A0`
with `A1`. It computes

```text
Id_A-over-pA(b0,b1) = R(b0,b1).
```

The executable fragment now contains
`PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER`, without a new Term tag.
Pi formation in a weakened non-empty Context reindexes the exact codomain type
Claim into its binder Context through an explicit projection premise. General
substitution reindex preserves non-Operation authority, while an Operation
occurrence remains an exact premise rather than falsely owning the reindexed
subject. When a target-local binding/type-formation proof and a reindexed proof
establish the same exact scoped typing proposition, evidence selection uses the
target-local proof as its premise and retains both derivations in the DAG.

The permanent test constructs both `A : Universe, b : A` and
`A : Universe, b : A, c : A`. It builds the object bridge for `A`, projects its
`R`, checks that each computed fiber is exactly `R(b0,b1)` or `R(c0,c1)`, and
asks the recursive object-telescope driver to extend the bridge at both depths.
Generation and certificate replay now share the object-language construction:
apply the immutable `OBJECT_TERM` action to the type expression `A : Universe`
in the current object bridge, obtain its exact Universe-identity witness, and
project `R` from that witness. Ordinary variable action follows `BindingId`;
Universe-fiber computation has no separate ancestor traversal, De Bruijn
reindex, or surface-name lookup. The driver stops with residual evidence when
the type expression action is not yet supported. This closes repeated
variable-family traversal over the currently admitted fiber rules. General
`B(a)` uses the same rule in principle, but remains residual until object action
supports the relevant APP/Match/type-former expression. This does not authorize
a generic `Obs` inhabitance test or a standalone contextful artifact root.

The required distinction from a generic logical relation is that the relation
used for a dependent fiber is selected by the preceding object identity
witness. Reflexivity, symmetry, and transitivity remain required operations,
but none of them can replace this type-former-directed computation law.

### T0.R10: finite Universe correspondence is not yet higher bisimulation

The 2022 calculus computes telescope equality by type equality and defines
Universe equality from a proof-relevant correspondence satisfying an
equivalence/bisimulation condition. The current Narya presentation makes the
missing higher obligation explicit: `isBisim` has the four ordinary operations
`trr`, `trl`, `liftr`, and `liftl`, plus a higher-recursive `id` field showing
that the induced correspondence between identity fibers is again a
bisimulation.

A Program currently generates a seven-field correspondence constructor:

```text
A0, A1, R, trr, trl, liftr, liftl
```

The endpoint types account for two of those fields; the remaining five expose
the finite one-dimensional operations. There is no recursive higher-coherence
payload. The implemented square-center tests preserve proof relevance and are
a real two-dimensional fragment, but they do not manufacture this missing
coherence for every Universe witness.

This is not merely a missing groupoid convenience. With only these fields one
can choose `R : Bool -> Bool -> Universe` to be constantly inhabited, choose
both transports to return `false`, and inhabit both lift fields. Such data
relates `true` and `false`, even though Bool elimination distinguishes them.
The recursive higher field rules out this first-order total-correspondence
counterexample by requiring the induced correspondence on every identity fiber
to satisfy the same bisimulation contract. Therefore the seven-field value is
not accepted as a public `Id Universe A0 A1` witness.

Therefore the accepted terminology is:

- `RELATION_*`: compiler-local logical relation/parametricity action;
- `IDENTITY_TYPE_COMPUTATION`: object identity computation for the explicitly
  admitted finite fragment;
- `UNIVERSE_CORRESPONDENCE`: finite executable correspondence carrying
  transport/lifting data;
- complete Higher Observational Type Theory: not yet implemented.

Artifact v70 consequently rejects
`PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE` roots. The
internal constructor and its projections remain available to exercise
type-level action and the first `B(a)` fiber, but neither a family-only root nor
a concrete witness root crosses the artifact authority boundary. Re-admission
requires an ordinary, proof-relevant representation of the recursive `id`
coherence and read-only kernel replay of that evidence.

Before the last claim becomes valid, the object theory must represent and
kernel-check the recursive higher bisimulation/coherence, provide dimensional
symmetry, and use symmetry plus box-filling operations to derive the expected
higher groupoid structure (including composition/J at the stated computation
strength). Symmetry/reflexivity/transitivity metadata on a relation is not a
substitute.

### T0.R11: implementation order for the higher closure

The remaining work has a strict dependency order:

1. Object action on a type expression `B(a) : Universe` must construct an
   ordinary witness `pB : Id_U(B0(a0),B1(a1))` from the object identities of
   `B`, `a`, and their Context. A compiler-local relation result is not usable.
2. Dependent Pi identity must use the relation projected from `pB` for its
   result fiber. The existing nondependent pointwise Pi rule is only the
   constant-family case.
3. Dependent constructor-field lifting must replay this operation along every
   preceding field/index identity. This removes the current nondependent-field
   restriction in indexed higher identity generation.
4. Identity computation on the Universe correspondence must then produce the
   next-dimensional coherence of `R`, transports, and lifts as ordinary object
   evidence. This is the executable counterpart of the recursive bisimulation
   obligation; it is not an extra Boolean property on the first-dimensional
   record.
5. Only after box filling is available at those computed higher families should
   dimensional symmetry be used to derive path inverse, composition, and `J`.

Implementing `sym` separately for Bool, Nat, Pi, and each generated identity
declaration before steps 1-4 would duplicate the type-former computation and
would not establish the Universe coherence needed for Higher Observational Type
Theory.

### T0.R12: selected representation of higher closure

A Program uses two representations at this boundary; they must not be
conflated.

For canonical type formers, higher identity is generated dimension by
dimension. A request for the identity of an already computed identity family
constructs the next finite boundary and center as ordinary Context,
Substitution, TypeDeclaration, Term, Claim, and Derivation data. This matches
the project's prior-computation model: no artifact contains an infinite tower,
but every admitted finite request can be continued after readback from its
object evidence. The existing nullary square and `Box Bool` fixtures are the
first instances of this representation.

For arbitrary non-diagonal Universe introduction, finite generation is not
enough. The intended rule is conceptually:

```text
R  : A0 -> A1 -> Universe
b  : IsBisim(A0,A1,R)
--------------------------------
glue(A0,A1,R,b) : Id Universe A0 A1
```

`IsBisim` has ordinary observations `trr`, `trl`, `liftr`, and `liftl`, and a
higher recursive observation `id` whose result is `IsBisim` for the induced
identity-fiber correspondence. This is a coinductive/negative interface, not a
strictly-positive constructor telescope. Merely appending an eighth recursive
field to the current ordinary generated ADT would require a finite constructor
value to contain its entire infinite coherence tower and is therefore the
wrong representation.

The selected implementation order is consequently:

1. finish dimension-on-demand action for canonical dependent fields and Pi;
2. specify a shared negative/coinductive observation interface in the same
   TermDB, without value/computation duplicate APP/LAMBDA tags;
3. represent `IsBisim` through that interface, with guarded or on-demand
   production of its recursive `id` observation;
4. introduce `glue` only when the exact `IsBisim` object Claim is present;
5. define projection computation for `trr`/`trl`/lifting and recursive `id`;
6. re-admit the Universe computation rule to artifact roots only after
   read-only replay verifies this entire object evidence.

Until step 6, the finite seven-field declaration is explicitly scaffolding.
It may support compiler-internal tests of projection and dependent-family
selection, but it has neither object-language introduction authority nor
artifact identity authority.

### T0.R13: dependent constructor boundary after the first object action

The first-dimensional generated identity of an ordinary constructor may now
consume a genuinely dependent field telescope. For a source constructor

```text
box : (a : Bool) -> B(a) -> Box
```

the generated constructor Context contains, for each source field, the left
endpoint, right endpoint, and exact object-identity fiber selected by the
preceding bridge. The second field classifiers are reindexed along the left
and right endpoint substitutions; they are not required to retain the source
Term ID. The generated-identity validator checks those classifiers modulo the
explicit prior BindingId correspondence and checks the five-argument object
action spine

```text
B^R B0 B1 pB b0 b1
```

instead of searching for a context-free two-index generated identity
declaration for `B(a)`.

This closes first-dimensional dependent constructor introduction for the
currently executable object action. It does not close its higher identity.
The existing eight-face `INDEXED_HIGHER_LIFT` constructs products of
independent field squares only. Its capability query now rejects a source
identity whose original constructor contains a later field depending on an
earlier field. Such a request remains residual until dependent square filling
and the recursive Universe coherence of T0.R10-T0.R12 exist.

Object action on a beta-reducible type expression such as
`APP(LAMBDA(a,Bool),x)` normalizes under `PURE_TYPE_WHNF`, acts on the normal
form, and retargets the resulting family to the original endpoint expression.
This is a sound conversion case. Unapplied raw type-level Lambda now uses the
same pointwise object-action constructor as a CBPV function. The shared
constructor branches only at the wrapper boundary: raw type Lambda produces a
raw `LAMBDA` witness, while a CBPV function produces the required
`RETURN`/`THUNK` wrappers. No value-side/computation-side Lambda tag was added.
Genuinely dependent Pi codomains and neutral type-level Match remain explicit
open cases.

### T0.R14: identity introduction is not same-source relation preservation

The latest theory audit found a remaining semantic gap that must not be hidden
by the successful raw-Lambda action test.

The common observational machinery supplies a type-indexed family, degeneracy
(`refl`), and action (`ap`). Internal parametricity and HOTT share this
machinery. They diverge at the Universe and at the structure required of these
families:

- in parametricity mode, a bridge in the Universe may be an arbitrary
  correspondence `A0 -> A1 -> Universe`;
- in HOTT, a Universe identity must carry transport and lifting in both
  directions, recursively coherent in all higher dimensions;
- the identity of each canonical type former computes by observing that type
  former, and every term has an action on those identities.

This meaning of "observational" is type-former-directed computation. It does
not, by definition alone, assert a full-abstraction theorem equating
inhabitance with contextual equivalence under every program context. An
adequacy theorem for A Program's effectful operational semantics is still
desirable, but it is a separate theorem and must not be silently assumed by
the kernel implementation.

Conversely, a same-source fundamental action

```text
a : A
-------------------------------
a^R : Id_A(a[delta0], a[delta1])
```

is not by itself a general introduction rule for identity between distinct
object terms. The current degeneracy path proves diagonal cases and the action
builder proves preservation by constructors, Lambda, APP, and Match. A1 must
also demonstrate an ordinary proof term whose endpoints are not DefEq.

The mandatory positive test is:

```text
f = lambda b. b
g = lambda b. match b { true -> true; false -> false }

p : Id_(Bool -> Bool)(f, g)
```

The proof is pointwise and its body is obtained by Bool elimination. It must
not be accepted merely because `f` and `g` normalize to a common graph, and it
must not be manufactured by applying the diagonal action to one source term.
The mandatory negative test replaces `g` by `lambda b. false`; the kernel must
not construct an identity witness because the `true` observation separates
the endpoints.

Therefore the raw type-level Lambda change closes only the shared pointwise
action mechanism. A1.T0 remains open until the kernel can check the positive
non-DefEq witness, reject the separating case, and preserve the resulting
proof at the next identity dimension. Symmetry, composition, transport, and
their higher coherence remain part of the same gate; field names alone are not
accepted evidence.

The first arbitrary-endpoint gate is now executable. The public object API
instantiates the exact identity family from independent left and right
`HAS_TYPE` Claims, then checks an ordinary witness Claim against that family.
It performs no endpoint proof search and adds no equality to conversion.

The initial non-DefEq function matrix uses:

```text
f = lambda b. return true
g = lambda b. match b { true -> return true; false -> return true }
h = lambda b. return false
```

The test first verifies that `f` and `g` are not kernel-convertible. The
pointwise Pi identity nevertheless admits the ordinary nested Lambda witness
for `Id(f,g)`, because every result observation is the same `true`
constructor. Reusing that witness for `Id(f,h)` is rejected. Thus object
identity introduction is no longer restricted to the two reindexings of one
source term.

This does not discharge the stronger `lambda b.b` versus eta-expanded Bool
Match test above. That proof consumes an arbitrary input identity and combines
it with branchwise identity evidence. It is retained as the gate for identity
composition/transport and higher preservation rather than being weakened to
the constant-function example.

## 13. Effect on A1

A1.R0's accepted-graph replay cleanup remains independently valid. A1.R1 and
later wire work wait. Tags 32/33 and proof rules 33-42 remain compiler-local
relation-action scaffolding and must not become v70 identity authority.
Persistent identity roots instead name the exact Claim of an ordinary generated
ADT/Pi identity family and the optional exact Claim of an ordinary
constructor/Lambda/APP/Match inhabitant.

## 14. Theory Anchors and A Program Departures

- *Towards Higher Observational Type Theory*:
  <https://types22.inria.fr/files/2022/06/TYPES_2022_paper_37.pdf>
- Narya HOTT documentation:
  <https://narya.readthedocs.io/en/latest/hott.html>
- Narya observational higher-dimension documentation:
  <https://narya.readthedocs.io/en/latest/observational.html>
- Narya parametricity documentation:
  <https://narya.readthedocs.io/en/latest/parametricity.html>
- *Internal Parametricity, without an Interval*:
  <https://arxiv.org/abs/2307.06448>
- *Observational Equality, Now*:
  <https://doi.org/10.1145/1292597.1292608>

A Program deliberately uses one graph-consed APP/LAMBDA/MATCH TermDB across
CBPV polarities, treats compilation as budgeted prior execution with residual
evidence, and expresses relations/proof payloads with ordinary object Terms
where possible. These departures require explicit rules and executable kernel
tests, not implementation convenience.
