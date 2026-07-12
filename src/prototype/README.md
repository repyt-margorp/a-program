# Prototype Source Area

This directory is reserved for AI-developed or experimental implementation code.

Code here is not accepted project implementation until the user explicitly asks
to promote, adopt, accept, or merge it into the main source tree.

Accepted implementation code belongs in `src/` outside this directory and in
`include/`.

## Current Prototype Files

- `current/`: prototype-side snapshot of the current accepted compiler source,
  including `main`, parser, lexer, environment, symbols, terms, and headers.
- `symbol.h`, `symbol.c`: prototype-owned symbol table used by the current
  prototype reader and REPL.
- `type_declaration.h`, `type_declaration.c`: source-derived type formation declaration
  indexes used while lowering AST into graph nodes. Concrete types themselves
  live in the term graph as `TYPE_FORMER` applications; classifier facts live in
  JudgementDB.
- `term.h`, `term.c`: prototype data structures for lambda terms and graph
  nodes. Top-level names are published as compile metadata labels that point to
  term graph roots. The terms preserve syntax structure before evaluation; for
  example, `x := id Bool.true;` is stored as an application of the term pointed
  to by `id` to the constructor term `Bool.true`.
- `reader.h`, `reader.c`: prototype reader for a small `.p` subset.
- `read_file.c`: temporary CLI used to compile and inspect the prototype reader.
- `repl.c`: temporary interactive CLI that keeps the loaded program state and
  accepts additional top-level code entries.

The type prototype models each type as a finite set of constructors. Constructor
names are stored together with their owning type and constructor index, so a
compiler pass can later annotate identifier references such as `true` or `false`
with their resolved type-level constructor identity.

## Canonical Term Identity

Term graph nodes keep binder references as local binder pointers, not as source
variable names. This is useful for graph rewriting and substitution, but those
local binder ids are not a stable identity across partial compilation units.
For link-time interning, the prototype exposes `prototype_term_canonical_key`.
It walks a term with a binder environment and maps every bound binder to a
canonical slot before hashing. For example, both of these local graphs produce
the same canonical key:

```
LAMBDA(FileA.Binder0, VAR(FileA.Binder0))
LAMBDA(FileB.Binder17, VAR(FileB.Binder17))
```

The runtime graph representation therefore remains pointer-like, while the
comparison key is alpha-normalized. `add_term` also uses the same alpha-aware
comparison when interning new nodes, so structurally equal lambda or match
graphs do not depend on the absolute binder ids chosen by a lowering path.
Classifier comparison in the prototype typing layer also delegates to this
alpha-aware term comparison, so type checking does not keep a separate
binder-id structural equality path. Export labels store the canonical key for
their current term root, and linking refreshes those keys after resolving
external references.

`prototype_canonical_link_table` is the prototype artifact-linking index. It
registers the export labels from one or more compile metadata blocks and groups
labels with equal canonical keys under the same representative entry. This is
the data structure a later serialized artifact format can use for
`export label -> canonical key -> representative term` linking without treating
file-local term ids or binder ids as global identity. Each entry also keeps the
source term database pointer for the current in-memory unit, so key collisions
are not trusted blindly: when a new label has the same canonical key as an
existing entry, the table verifies the two roots with cross-database
alpha-aware structural comparison before sharing the representative.

Free binders remain keyed by their local binder id; exported closed terms are
the intended input for cross-file canonical linking. Match frame ids are still
local evaluation handles, but each populated frame also stores a linkable frame
key derived from the canonical key of the match term it represents. When that
key is available, `INDUCTION_HYPOTHESIS` canonicalization uses the frame key
instead of the numeric frame id. Terms that still contain unrelocated frame-local
references are flagged so an artifact linker can reject or relocate them
explicitly.

Definitional equality is a separate layer: the canonical key handles
alpha-normalized structure, not beta or match computation. The prototype now
exposes `prototype_term_whnf` for the current weak-head evaluator and
`prototype_term_normalization_equal` for WHNF-based recursive equality over lambda, app, pi,
match, constructor, and induction-hypothesis nodes. The typing layer
uses this normalization equality path for classifier compatibility where it has access to the
mutable term graph and type declaration database, including lambda expansion,
match motive applications, and the current motive-derived induction hypothesis
resolution path. `*rest` typing no longer uses a seed classifier or a `P_`-style
constant. The match typing phase builds a concrete generated motive and records
the temporary match result classifier in a dedicated match-motive result table.
IH resolution may read only that motive-derived result or a committed
`MATCH_ELIM` fact. Expected/ascribed classifiers live in a separate expected
classifier table and are not allowed to seed IH typing. This is still not a
general dependent recursive motive solver.
Match lowering now records the scrutinee term, case labels, branch bodies, and
match frame without requiring the scrutinee classifier immediately. Constructor
case resolution reads the scrutinee classifier later from synthesized HAS_TYPE
facts, and match result typing is handled by the classifier-inference phase.
This keeps the computation graph construction path from using type expectations
or synthetic motive terms as graph-building inputs.
Lambda binder assumptions are queued during graph construction and seeded into
JudgementDB at the start of classifier inference. The lambda term still carries
the binder pointer and body graph, while the proof fact `VAR(binder) : T` is now
introduced as inference context rather than as an AST-lowering side effect.
Type formation and constructor classifier facts are also synthesized lazily.
Lowering a type declaration installs the declaration and constructor namespace
metadata, but it no longer emits HAS_TYPE facts for every local type former and
constructor up front. When a type formation or constructor term actually
participates in typing, the classifier-inference phase creates the corresponding
type-formation-intro or constructor-intro proof. Imported constructor namespace
resolution follows the same split: AST lowering builds the constructor term and
queues classifier materialization, then the later classifier phase reconstructs
the constructor classifier from the artifact interface.
External declarations remain a boundary fact: when source declares or imports
an unresolved name with a classifier, graph construction queues a declaration
fact and the classifier phase seeds it before APP typing. This is no longer an
AST-lowering write into JudgementDB, but it is still not synthesized from term
structure. It should eventually move behind an explicit declaration/import
environment if JudgementDB is made a pure proof database.
Ascriptions such as `term :: T` are also lowered as graph terms plus a pending
check. `EXPECTS_TYPE` is not a judgement relation in the prototype anymore.
After classifier inference, the checker selects an already synthesized
classifier compatible with `T` and records only the resulting `HAS_TYPE`
conversion proof when the exposed classifier differs from the synthesized one.
Top-level expectations use the same post-inference path, including the case
where several exported names point to the same core lambda graph but expose
different classifiers.
The evaluator is controlled by explicit reduction flags: definition unfolding,
lambda beta reduction, match branch reduction, and induction-hypothesis
expansion can be enabled independently through
`prototype_term_whnf_with_options` and `prototype_term_normalization_equal_with_options`.
This keeps the lambda-core computation layer separate from the eliminator layer:
beta-only checks can run without selecting match branches, and match-only checks
can select constructor-headed branches without reducing beta redexes first.
When match reduction sees a stuck scrutinee but every case body reduces to the
same binder-independent graph under the active reduction flags, the pure
normalizer may reduce the match to that common body. This is the prototype hook
that lets a generated type-level motive match collapse to `Nat`, `Bool`,
`PI(...)`, or another type term after computation, rather than by a parser-side
branch-unification rule. Effect-performing strategies first remove that uniform
rule from the active reduction options. This prevents `perform` from observing,
duplicating, or reordering effects by inspecting branch bodies before the
scrutinee is known.
`TYPE_FORMER` normalization equality compares the declaration identity, not the
declaration's `TypeCodeShapeKey`. The prototype keeps two type-facing layers:
the core shape layer, where structurally equal type codes may share a
representative, and the named type view layer, where fresh `@{ ... }`
declarations remain generative. `TypeCodeShapeKey` is only a structural
fingerprint for the core shape layer. Runtime match reduction compares
constructor owners through normalization equality before using the constructor
ordinal, but that normalization equality no longer equates separately generated
type declarations merely because their fingerprints match.

The linked judgement database may contain several classifiers for the same core
term. For example, `identityBool := \x : Bool => x;` and
`identityNat := \y : Nat => y;` both publish the same lambda graph node while
their `Bool -> Bool` and `Nat -> Nat` classifiers remain separate judgement
facts. Link validation therefore checks that exported terms are closed after
local label resolution; it does not reject distinct classifiers attached to the
same core term.

## Term Semantics Layers

`prototype_term_semantics` records how each term tag participates in the current
calculus. This is documentation in code form: it does not create a second graph,
but it makes the intended separation observable.

- Lambda core: `VAR`, `APP`, `LAMBDA`
- Eliminator layer: `MATCH`
- Type-former layer: `PI`, `TYPE_FORMER`, `UNIVERSE_VAR`, host primitive types
  such as `PRIMITIVE_TEXT`, `PRIMITIVE_INT`, and `PRIMITIVE_INT64`, and effect
  classifier nodes such as `EFFECT_LABEL` and `EFFECT_TYPE`
- Data layer: `CONSTRUCTOR`, host literal values such as `TEXT_LITERAL` and
  `INT_LITERAL`
- Link layer: `EXTERNAL_REF`
- Host layer: `INTRINSIC`; concrete processing-system oracles are registered
  through the host intrinsic table in `term.c`
- Induction layer: `INDUCTION_HYPOTHESIS`

The key distinction is that alpha/canonical identity is still structural, while
normalization equality is computational. `APP` reduces by beta when its WHNF
function is a lambda. `MATCH` evaluates its scrutinee and reduces when the
scrutinee head is a known constructor. Pure host intrinsics such as integer
arithmetic reduce through the same step engine and are enabled in the default
pure reduction mode. Effectful host intrinsics are typed as effectful
computations, for example `#.print : #.Text -> Effect(Terminal, #.Text)`, and
only run when the evaluator mode explicitly allows the corresponding effect
capability. `INDUCTION_HYPOTHESIS` uses the match frame key for identity but
still uses the local frame handle to build the recursive match during
evaluation. Direct match reduction is part of normalization equality; fully
normalizing recursive definitions that rely on `INDUCTION_HYPOTHESIS` still
needs the recursive match frame to carry the outer substitution environment
explicitly.

`TermDB` is allowed to be the computation graph, but it must not become the
execution machine state. Evaluation may materialize intermediate terms into the
same graph, such as beta results, instantiated matches, constructor
applications, and other ordinary language terms that did not appear literally in
the source. That is still computation graph growth. By contrast, evaluator
environments, captured binder-to-value substitutions, runtime frame stacks, and
continuations are execution state and must stay outside the term graph. In
particular, induction-hypothesis evaluation should not introduce an environment
node. The current evaluator handles substitutions such as `A := Nat` by
materializing an instantiated `MATCH` and a corresponding instantiated
`MatchFrame` in `TermDB`; any additional runtime stacks or continuations needed
by later evaluation strategies should remain evaluator state rather than graph
terms.

For the current substitution-materializing evaluator, instantiated matches also
materialize their match frame when necessary. If substituting through a
`MATCH(..., frame#F)` does not change the scrutinee, constructor owners, branch
bodies, or nested induction references, the original match and frame are reused.
If substitution does change the match, the evaluator creates a fresh frame for
the instantiated match and substitutes branch bodies under a temporary frame
scope, so nested `INDUCTION_HYPOTHESIS(frame#F, x)` nodes are rebuilt against the
fresh frame as part of ordinary term substitution. This keeps `append Nat` as an
actual graph specialization of `append`, while avoiding both needless frame
cloning and the earlier two-pass branch substitution.

### Structural Induction Hypotheses

The current `*name` syntax is intentionally not a general recursive call. It is
lowered only as an induction hypothesis tied to a match frame. The reader first
requires `name` to be a local match-case binder. During graph lowering, each
case binder is associated with the `MatchFrame` for the surrounding match, and
`*name` becomes `INDUCTION_HYPOTHESIS(frame, VAR(name))`.

The typing/resolution phase then checks that the referenced case binder is a
direct recursive self field of the constructor selected by that case. A pending
induction hypothesis that cannot be resolved under those constraints is rejected
before compilation succeeds. In the current supported fragment, this gives a
syntax-directed termination discipline: recursive use can only follow the
structurally smaller field exposed by the active match case.

This is deliberately conservative. Recursive fields are recognized as direct
`SELF` fields in the type declaration. Aliases, indexed families, mutual
recursion, nested inductive occurrences, and more general normalization equality-based
recognition of recursive fields are not part of the current guarantee. Those
extensions require a more explicit structural-order checker; they should not be
introduced by weakening the `*name` lowering rule.

## Artifact Shape

The intended compiled artifact is one physical file with several logical
sections, not separate API and object files. Importers should be able to read
only the interface section, while the linker or evaluator can read the graph
section when body terms are needed.

Files are physical input chunks; namespaces are logical name-resolution and
export-identity units. By default, the prototype derives a namespace from the
output artifact basename, or from the first source file basename when no output
artifact path is supplied, with the file extension stripped. Passing
`--namespace Name` uses `Name` exactly as the logical namespace and makes every
source file in that compile invocation publish its exports there. This keeps the
language assign-based and order-insensitive at source level while allowing the
build driver to choose whether one file, several files, or a generated bundle
form one namespace.

An artifact may contain more than one namespace. Aggregate artifacts therefore
do not mean that all exports with the same textual name are the same export:
`Nat` from namespace `PreludeNat` and `Nat` from namespace `UserNat` can coexist
in one physical artifact. Export identity is the provider namespace plus the
exported name. `TypeCodeShapeKey` remains a structural fingerprint and relocation
aid, not the identity of a generative type declaration. This is the boundary
that prevents separately declared `Bool`/`Two` or `List`/`ListB` declarations
from collapsing merely because their constructor structure is the same.

Dependencies can still be emitted as unresolved unqualified names. In the text
artifact these are written with namespace `-`, meaning the current source syntax
or search path did not specify a provider namespace. A later linker may resolve
that name against a selected provider namespace or against an aggregate
interface. This keeps file paths out of the semantic identity while still
supporting the current simple `import Nat;` prototype flow.

```
artifact
  interface section
    exported type formers
    interface-local type expressions
    type parameter binders
    constructor ordinal tables
    constructor field type closures
    exported term labels and classifiers
    transparency flags
    canonical keys
    dependency/import records

  graph section
    term nodes
    type declaration tables
    match cases and match frames
    judgement relations

  relocation/import section
    external term label references
    external type former references
    constructor owner references

  judgement/universe section
    universe variables and constraints
    checked judgement summaries

  debug/name annotation section
    exported term display names
    exported term source spans
    exported type display names
    exported type source spans
    constructor display names and ordinals
    constructor source spans
```

`TYPE_VIEW(view_type_id, core, source)` is the bridge between the two layers.
`core` points at an interned representation spine, while `view_type_id` and
`source` preserve the named type view used for constructor label lookup and type
checking. Constructor ordinals remain integers; the evaluator uses an erased
owner representation plus the ordinal, while source lookup chooses that ordinal
through a named `TYPE_VIEW`.

The prototype now keeps two explicit equality modes for type views. View-shape
equality preserves the `TYPE_VIEW` wrapper and compares `view_type_id`, `core`,
and `source`; this is the mode used by typed conversion. Core-shape equality
unwraps `TYPE_VIEW.core` before comparing and
is only structural evidence that two views share the same computational shape.
Core-shape equality must not be used as a typed conversion until a later
transport/equality proof explicitly changes the view.

The current prototype has a text artifact format beginning with
`A_PROGRAM_ARTIFACT 27`. The reader accepts that format only; old artifact
versions are intentionally rejected instead of being kept as compatibility paths.
It writes an `interface` section with term exports, type exports,
interface-local type expressions, type parameter binder records, constructor
ordinal/field-count exports, constructor field type closures, dependency
records, classifiers, transparency flags, term canonical keys, and classifier
canonical keys. Term and type exports carry their provider namespace; dependency
records carry either a provider namespace or `-` for unresolved unqualified
dependencies. Interface-local type expressions, type parameter records, and
constructor field type references are serialized as sparse slot tables, so
unreachable holes keep their ids without being emitted as live entries. It also
writes a `graph` section with local term nodes, type declarations, type
expressions, match cases, match frames, judgement relations, and proof objects.
The graph section uses the same slot/present-count shape for the type side:
unreached type declarations, parameters, constructors, field type references,
and type expressions remain holes rather than silently becoming exported
metadata.
Each proof object records its introduction/elimination reason, conclusion
judgement kind, conclusion subject/classifier, and premises as
`(judgement kind, subject, classifier, proof id)` edges, so artifact validation
checks both the premise judgement and the proof node that derives it.
Proof validation also rejects cyclic premise edges; proof objects form a DAG
over the judgement graph, not a circular justification table. Every proof object
must be referenced by exactly one judgement relation, so serialized artifacts
cannot hide orphan proof nodes.
A `universe`
section stores universe nodes, edges, solved levels, and constraints. Universe
nodes and edges are also sparse: only nodes reachable through exported type
declarations and parameters, and only edges whose endpoints are both present, are
written. A `debug` section stores exported term display-name annotations plus source
entry/name/body spans, exported type display-name annotations plus name/body
spans, and constructor display names plus constructor-name spans separate from
the graph ids. A
`relocation` section records unresolved `EXTERNAL_REF` term nodes, resolved
neutral `EXTERNAL_REF` nodes satisfied by the artifact interface, unresolved
type-expression names, and resolved imported type expressions carrying their
`TypeCodeShapeKey`. Unresolved type-expression names that denote type formers are
also mirrored as explicit external type-former relocation records, and graph
readback preserves those records as `external type former type_expr#... -> Name`
lines. Resolved imported type formers are likewise preserved as
`resolved external type former type_expr#... -> type_export#...Name code_shape_key=...`
lines. It also records resolved constructor owner references for
constructor terms and match cases; each record stores the owner graph key plus
the constructor ordinal. The prototype regression suite now includes a graph
readback check for a Match-bearing artifact so the serialized match case table,
case-binder table, match-frame table, and match-case constructor owner
relocations stay covered. The interface section can be read back without reading
the graph, universe, debug, or relocation sections. The graph and universe
sections can also be deserialized as artifact-local tables. The debug and
relocation sections can be read into small tables; the temporary CLI prints
those counts with `--read-graph`. Linked artifacts that are produced from
already-compiled provider graphs preserve display names but use unknown source
spans for re-exported provider names because the source AST is not present at
that link stage.

Constructor exports include field counts, diagnostic field type closures, and the
graph-level `classifier_family`. Interface-only import can therefore type both
zero-field constructors such as `(List Nat).nil` and ordinary constructor
applications such as `(List Nat).cons Nat.zero tail` without loading the provider
graph. The authoritative constructor classifier is the exported
`classifier_family`; field type expressions are retained for readback and
diagnostics. Imported constructor namespace resolution only builds the
constructor term during AST lowering; it queues classifier materialization for the
later classifier-inference phase, where the interface classifier family is
instantiated against the owner type graph.

Universe variables inside the interface type-expression pool are artifact-local
handles. When an interface is imported for source compilation, the prototype
renumbers those variables into a reserved range and advances the current unit's
fresh-universe counters before lowering the graph. Re-exporting provider
interfaces into an aggregate artifact also renumbers appended interface
universe variables so two providers that both used `?u0` or `?u1` do not expose
colliding interface-level handles.

Aggregate artifacts are the current prototype replacement for a separate
library file format. They keep one file format with an interface section and a
graph section, while allowing a provider artifact such as `Prelude.apo` to
re-export the interfaces and graphs of `Nat.apo`, `List.apo`, and similar
units. Appending providers renumbers artifact-local ids and preserves generated
type identities. Separately compiled declarations with the same shape are not
collapsed by `TypeCodeShapeKey`; only core terms whose canonical keys do not depend
on local type identity, such as the unannotated lambda graph for
`\x : Nat => x`, may share a graph root.

Type expressions inside type declarations normalize transparent aliases by
name. A field type written through a transparent local alias such as `B := Nat`
is stored for readback as a reference to the underlying `Nat` name, so the alias
does not create a new displayed field type dependency. The constructor portion of
`TypeCodeShapeKey` is computed from `classifier_family`, not from those field
metadata records, so dependent constructor fields such as
`mk : (a : A) -> B a -> *` canonicalize through the graph binder structure. A new
declaration such as `BoxB := @{ box : B -> *; }` is still generative and remains
distinct from `BoxNat := @{ box : Nat -> *; }`. Interface-only checking no longer
treats two separate exported declarations as the same type merely because their
`TypeCodeShapeKey`s match.

Symbol identifiers are artifact-local implementation handles, so the graph
section does not serialize raw `symbol_id` values for names. Type declaration
names, constructor names, type-expression name refs, text literals, external
refs, intrinsics, and match case labels are written as symbol text and interned
into the current symbol table on readback. This keeps provider graph names from
being accidentally reinterpreted as unrelated target symbols during link.

The prototype can apply term-label relocations after the provider body already
exists in the same merged term graph: `EXTERNAL_REF(name)` is replaced with the
provider interface's exported local term. Cross-artifact graph copying and id
renumbering are implemented as a minimal append operation: provider term, type,
match, binder, universe-level, and judgement ids are shifted into the target
graph's local id space. Artifact graph readback restores the next universe level
counter from both type expressions and term-level `UNIVERSE_VAR` nodes before
append uses that counter as the provider offset. After append, exported terms
may point back to an existing target graph identity only when their canonical
key is linkable across artifacts. Canonical keys that depend on artifact-local
type identity are deliberately not used for cross-artifact merging. Constructor
owner references in appended constructor terms and match cases therefore
preserve generated type identities instead of being redirected by structural
type-former fingerprints.

Type-expression names can be relocated through a provider interface before the
provider graph is loaded. A field type such as `List Nat` can first become an
`IMPORTED_TYPE(ListKey)` reference and, after the provider graph is appended,
resolve to the provider's appended `TYPE_FORMER(List)` graph node. The key
remains a relocation aid and fingerprint; it does not make an independently
declared local `List` definition the same type.

The temporary `read_file` CLI exposes the minimal artifact flow:

```
/tmp/a-program-read-file --write-artifact user.ao User.p
/tmp/a-program-read-file --write-artifact shared.ao --namespace Shared A.p B.p
/tmp/a-program-read-file --write-artifact provider.ao --opaque-export hiddenBody Provider.p
/tmp/a-program-read-file --write-artifact user.ao --import-interface list.ao --import-interface nat.ao User.p
/tmp/a-program-read-file --write-artifact user.ao --import-search-dir artifacts User.p
/tmp/a-program-read-file --read-interface user.ao
/tmp/a-program-read-file --read-graph user.ao
/tmp/a-program-read-file --check-export-normalization-equal provider.ao idNat
/tmp/a-program-read-file --check-export-classifiers-compatible linked.ao idNat idAlias
/tmp/a-program-read-file --link-artifacts user.ao prelude.ao --link-output linked.ao
/tmp/a-program-read-file --link-artifacts user.ao nat.ao --link-provider list.ao --link-output linked.ao
/tmp/a-program-read-file --link-artifacts box.ao --link-search-dir artifacts --link-reexport-providers --link-output linked.ao
/tmp/a-program-read-file --link-artifacts box.ao nat.ao --link-provider list.ao --link-reexport-providers --link-output box-prelude.ao
/tmp/a-program-read-file --aggregate-artifact prelude.apo nat.apo list.apo
```

`--link-artifacts` reads both interface and graph sections, applies
type-expression relocation, appends the provider graph, applies term-label
relocation, recomputes dependencies, rebuilds the linked universe graph, and can
write the linked graph back as one artifact. Linked artifact relocation output is based on reachable
interface/judgement roots, so dead `EXTERNAL_REF` nodes left behind by rewriting
do not appear as unresolved relocation records.
Additional provider artifacts can be supplied with repeated `--link-provider`
options. `--link-search-dir` scans `.ao` and `.apo` files in a directory,
reads only their interface sections, and adds providers that export unresolved
dependency names from the target interface or from already discovered provider
interfaces. This closes simple transitive dependencies such as `User -> Box ->
Nat` without source recompilation. During link, newly appended provider graph
refs are also relocated against the interface that has already been linked, so
`Box` can resolve its `Nat` field after `Nat` has been appended. After provider
discovery, the prototype refreshes linked interface keys so re-exported
type code shape keys remain stable after all graph canonicalization and universe
renumbering.
By default, a linked artifact keeps only the target artifact's interface
exports. Passing `--link-reexport-providers` also publishes appended provider
term/type/constructor exports through the linked artifact interface; this is the
temporary way to build a composite artifact such as `Prelude = Nat + List`.
`--aggregate-artifact` is the same composite operation with re-export enabled
and an output artifact required. It is intended for `Prelude.apo = Nat.apo +
List.apo` style bundles without introducing a separate `.lib` artifact kind.

`--import-interface` is the current source-compile side of import. It reads only
the provider artifact's interface section before graph lowering. This lets a
source unit compile external constructor namespace references such as
`(List Nat).nil` without recompiling `List.p`: the owner remains an external
term graph during compile, and link later resolves `List` and `Nat` through the
provider graph sections.

The prototype also accepts top-level source imports such as:

```
import Nat;
import List;
```

These imports name unresolved interface symbols, not source files. During source
compile, repeated `--import-search-dir` options scan `.ao` and `.apo` files,
read only their interface sections, and load the first artifact that exports the
requested symbol. The output artifact still records the imported names as
dependencies; the linker resolves those dependencies from provider graph
sections or an aggregate artifact such as `Prelude.apo = Nat.apo + List.apo`.
This means a source file can write `import Nat; import List;` while the build
command points only at a directory containing `Prelude.apo`; the source file does
not need to name `Nat.p`, `List.p`, `Nat.apo`, or `List.apo`.

The artifact/link invariants above are covered by the prototype regression
script:

```
sh src/prototype/test_artifact_flow.sh
```

It checks that `identityBool := \x : Bool => x;` and
`identityNat := \y : Nat => y;` publish the same core lambda term, that artifact
v8 debug/name records are readable, that term exports keep distinct classifier
keys even when they share a core term, that a split `Nat.apo` + `List.apo`
compile can build `(List Nat).nil` through explicit interface imports and
through source-level `import Nat; import List;` plus `--import-search-dir`, and
through the same source imports backed only by an aggregate `Prelude.apo`, and
that the alias case `B := Nat; (List B).nil` links to the same constructor-owner
graph key. It also checks that a valid imported `cons` application is typed from
the interface field closure, that a bad imported `cons` application is rejected,
that separately generated but structurally identical declarations such as
`Bool`/`Two`, `BoxNat`/`BoxB`, and `List`/`ListB` do not collapse to the same
type identity,
that field type references such as `box : Nat -> *` record unresolved external
type expressions during source compile and resolve them to imported
`TypeCodeShapeKey`s during link, and that aggregate artifacts renumber
interface-local universe variables from different providers. It also checks the
transparency boundary directly:
`EXTERNAL_REF(idNat)` is definitionally equal to the exported body when `idNat`
is transparent, and is not definitionally equal when the same export is marked
opaque. Classifier compatibility follows the same boundary: after linking a
transparent `NatAlias := Nat`, `idAlias :: NatAlias -> NatAlias` is compatible
with `idNat :: Nat -> Nat`; after linking the same alias as opaque, it remains a
neutral external classifier and is not compatible with `Nat -> Nat`. Constructor
owner canonicalization follows the same rule: `(List NatAlias).nil` and
`(List Nat).nil` share the same linked term and canonical key when `NatAlias` is
transparent, while the opaque alias keeps a distinct owner graph.

This is still not a complete linker. It has prototype provider search over
artifact directories and source-level symbol imports, but does not yet implement
archive/library indexes, graph compaction, or complete cross-artifact universe
constraint relocation.
Linked output currently rebuilds the universe graph from merged judgements
instead of preserving provider universe sections as independently relocated
proof objects.

Term normalization equality can now be supplied with a definition environment derived from an
artifact interface. `OPAQUE` exports are never unfolded by normalization equality. `TRANSPARENT`
exports can unfold `EXTERNAL_REF(name)` to the exported body term and then
continue ordinary WHNF reduction, including beta reduction. Loading that body
from another artifact's graph section is still a linker responsibility. Term
relocation follows the same rule: transparent exports may rewrite
`EXTERNAL_REF(name)` to the body graph, while opaque exports keep the external
reference as a neutral name. If that opaque name is present in the linked
artifact interface, dependency and relocation output treat it as resolved
rather than unresolved. Such references are emitted as
`resolved_external_term_ref` records so the graph still states which interface
export satisfies the neutral name.

The temporary CLI exposes this through `--opaque-export name` when writing an
artifact. Exports are transparent by default. This is intentionally an artifact
interface property, not a change to the core term graph: the same term body can
be present in the graph section while the interface tells normalization equality and relocation
whether external users may unfold it. `--check-export-normalization-equal artifact.ao name` is
a prototype inspection hook for this boundary: it reads the artifact interface
and graph, builds the definition environment from export transparency, then
checks whether `EXTERNAL_REF(name)` is definitionally equal to the exported body.
`--check-exports-normalization-equal artifact.ao left right --reduction-mode mode` compares
two exported terms using `mode` equal to `default`, `beta`, `match`, or `none`.
`--check-export-classifiers-compatible artifact.ao expected actual` performs the
same definition-environment check for the classifiers of two exported terms.

Parameterized definitions such as `List := \A : @ => @{ ... };` are represented
as namespace factories, not as concrete types. `List` is a type-level lambda-like
object that produces an applied constructor namespace such as `List Bool`.
Constructors are then referenced through that applied namespace, for example
`(List Bool).nil`.

The temporary reader/compiler currently accepts the prototype syntax covered by
the regression examples:

- anonymous finite type declarations in the form `Identifier := @{ ... };`
- constructor clauses in the form `identifier : *;`
- constructor clauses with fields in the form `identifier : * -> *;`, where
  the final `*` is the type being defined and each preceding `*` field is also
  read as the type being defined
- lambda term definitions in the form `identifier := \x : @ => body;`
- namespaced constant references in the form `Namespace.member`
- applied namespace constant references in the form `(Namespace Arg).member`;
  for example, `List` is treated as both a type-level function and a namespace,
  so `(List Bool).nil` is the `nil` member of the concrete `List Bool` namespace

For lambda binders, `@` is stored as `PROTOTYPE_INVALID_ID`, meaning the binder
has no concrete type annotation yet.

The interactive CLI can be built manually while the prototype is not part of the
accepted build:

```
cc -std=c11 -Wall -Wextra -I src/prototype \
	src/prototype/repl.c src/prototype/ast.c src/prototype/ast_inspect.c \
	src/prototype/reader.c src/prototype/term.c src/prototype/type_declaration.c \
	src/prototype/typing.c src/prototype/universe.c src/prototype/symbol.c \
	-o /tmp/a-program-prototype-repl
```

Run it with optional initial files:

```
/tmp/a-program-prototype-repl examples/01_bool.p
```

Inside the prompt, `:state` prints the current loaded state and `:quit` exits.
Entering a bare top-level term name queries the stored syntax and the current
evaluated value:

```
prototype> x
term x := APP(LAMBDA(...), CONSTRUCTOR(Bool.true))
value x := CONSTRUCTOR(Bool.true)
```
