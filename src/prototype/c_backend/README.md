# Prototype C Backend

## Problem List

1. Produce executable C from `.a` without making C the owner of artifact semantics.
2. State the support and acceptance boundaries, rather than erase unsupported proofs.
3. Keep LinkerScript, native products and the public C ABI downstream of `.a`.

## Subjective (User)

2026-10-03, English paraphrase of the direct user clarification: prioritize
readily usable downstream C modules through `.a` and LinkerScript; avoid excessive
deep or scope-expanding investigation. The backend connects A Program to external
clients and eventual assembler lowering without authority over source semantics.

2026-09-29, paraphrase: continue artifact implementation through C transpilation;
push verified increments. Preserve the single-fuel policy and separation of
computation from typing. This supersedes the previous wait-before-C instruction.
Further clarification that day: readable target code is a cultural convention
(the user's **Tradition**), not source theory. Refactoring `.a` is permitted,
but do not extend its semantics or fields for backend/Linker responsibilities.
Target representations, ABI choices and lowering analysis remain downstream.

Agent implementation criterion: an artifact format change requires an A Program consumer independent of this
backend. Missing lowering information is first derived from existing semantic
nodes; temporary target analysis belongs to the backend arena. An unsupported
profile reports failure, not a request to rewrite or enrich its input image.
Successful and rejected link requests must leave the input `.a` unchanged.

## Objective (Code)

The structural profile's `emit.c` reads a borrowed closed `pg_occurrence`, Core DAG and public Oracle
views. It emits one C function per shared Core node. It never calls parsing,
Solve, conversion or normalization. Binder/layout/label identities become local
target ordinals without alpha interning or changes to the input graph.

`main.c` is a separate adapter: inert image loading, whole-module-checked name
selection through existing Solve, then emission and atomic publication. Multiple
exports share that Program and one total validation budget. Imported
typed structure alone grants no acceptance. The adapter reports reconstruction
fuel and refuses pending/rejected entries, including invalid siblings and `::`.
With explicit `--trust-image`, the artifact adapter instead borrows a saved local
definition only when the whole module and every entry have saved completion.
This is a user-trusted, unauthenticated assertion, not new Kernel evidence.
It performs no Solve, even with `--steps 0`; it cannot finish partial work.

`runtime.c` supplies target-local closures, lazy pure operands and Oracle
realizations. Generated executables link only this runtime and the C library,
not the parser, kernel, Solver or artifact reader. Runtime values are not another
serialized Core or type graph. Each invocation owns its allocations and frees
them at exit, including reported I/O failures. Host effects execute only in the
outer runner; memoizing a pure Request does not memoize its eventual print.

## Assessment

Supported: Lambda/Application, lexical captures, partial application,
Return/Thunk/Force, total-result projection, zero/multiple-clause deep Fold,
operation forwarding and repeated resumptions, exact Text bytes, print,
Int32/Int64 wrapping add/subtract/multiply/negate and signed ASCII formatting,
structural constructors/Match and the existing Lambda encoding of recursion.
Rigid classifier/family objects remain distinct neutral tokens, including
generic runtime type arguments. ABI 2 adds a one-direction action of generated
closures: binders carry left/right/chosen-center operands, with captured ambient
values fixed at the closure boundary. Lambda/Application and constructor Match
actions retain the chosen center. Runtime diagonal reduction requires an actual
reflexive path and matching endpoints; it is not arbitrary proof erasure.
Scoped U(F) and U(Pi) transport map suspended computations/functions, including
contravariant input transport and lifting. Return payloads and call arguments
remain lazy. No source graph normalization occurs during emission.

Limitations:
- Identity support is not complete. General dependent thunk lifting, iterated
  higher actions and arbitrary U/Pi identity observations are not implemented.
  Unsupported demanded Identity fields stop the executable with status 4;
  arbitrary chosen loops and nonmatching endpoints are never erased. Neutral
  functions can also remain unsupported where the kernel's richer alpha
  comparison succeeds. Unknown reachable Oracle owners still reject emission
  before publication, including syntactically reachable unused branches.
  `check-c-sorting-boundary` now checks actual standalone Acc QuickSort output
  `FFTT` in both checked and trusted modes, not just an unsupported boundary.
- Without `--trust-image`, the command adapter reconstructs/checks via ordinary
  Solve; zero steps cannot emit. Trust mode requires a materialized completed
  module, not an inputs-only image or an isolated completed definition beside
  pending/rejected obligations. No fallback from failed checking to trust.
  `--revalidate-limit R` bounds reconstruction/validation within `--steps B`:
  spent fuel is at most `min(B,R)`, with unused fuel reported, not added to B.
  Hitting R returns pending without publishing C or silently trusting the image.
  This currently bounds the existing whole-module/entry check, not a restored
  checkpoint. Persisted validation history, compiler-wide resume integration,
  authenticated hash reuse and exact suspended frontier restoration remain
  unimplemented. This backend option does not install
  imported evidence in the compiler or provide compiler-wide trusted reuse.
- Runtime allocation is invocation-wide; no garbage collection, tail-call
  guarantee, bounded memory or performance claim. C stack/heap resources bound
  execution. The ABI is versioned locally, never written into canonical `.a`.
- Entry selection uses the first image root as a source module. In the
  structural profile exports must be returning computations or closed values,
  not unapplied Pi. The separate scalar profile below admits a limited Pi subset.

The ABI requires C11, 8-bit bytes and exact `uint32_t`/`uint64_t`. Arithmetic
uses unsigned intermediates and masks, avoiding C signed-overflow undefined
behavior. Text is counted bytes, including NUL; no locale/encoding conversion.
Unknown host operations remain unhandled unless intercepted by a user Fold.
Correspondence is checked by differential tests; this is not a machine-checked
compiler-correctness theorem.

## Plan

Progress is tracked in [AP4](../../../doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md#ap4-first-c-transpiler).
This directory is an unpromoted prototype, not a change to the accepted build.

```sh
bash src/prototype/artifact_persistence/candidate.sh /tmp/a-program-c
make -f src/prototype/c_backend/build.mk OVERLAY=/tmp/a-program-c BUILD=/tmp/a-program-c/build check-c-backend check-c-sorting-boundary
/tmp/a-program-c/build/pointer-check --save /tmp/example.a src/prototype/c_backend/fixtures/arithmetic.p
/tmp/a-program-c/build/a-to-c /tmp/example.a main /tmp/example.c
cc -std=c11 -O2 -Isrc/prototype/c_backend /tmp/example.c src/prototype/c_backend/runtime.c -o /tmp/example
/tmp/example
```

The final command prints `-2147483648:-2147483648:1:2147483647`.
Use a new overlay directory. The adapter accepts `--steps N`, optional
`--revalidate-limit N` (default unlimited within `--steps`), and an explicit
fixed `--image-limit N` (default 1,000,000), or explicit `--image-limit none`,
independent of file bytes or fuel. Removing the quota does not confer trust or
skip format/representability checks. It does not reduce the retained graph.
`--trust-image --steps 0` opts out of revalidation, relying on the image author's
completion statements. Use only when that external trust is justified; a digest
stored inside the same file does not authenticate it. This option is local to
artifact admission, not the C emitter, runtime or Kernel. APGSRC68 is required;
rebuild older images from source. Once any ordinary Solve has begun, the API
refuses trusted export; a save then reports only locally completed producers.
Exit codes: 0 emitted, 1 rejected, 2 input/I/O failure, 3 pending, 4 unsupported.

Tests compare standalone output with the interpreter, require deterministic C
and unchanged input-image digests, and cover zero fuel, input-only images,
generic List, nominal ADTs, host bounds, NUL, repeated thunks, handlers and output
failure. Raw Oracle tests separately check all Int64 operations and two distinct
operation labels; these descriptive test inputs are not acceptance receipts.
They assert emission performs no evaluator/substitution steps or graph mutation.
Identity tests include kernel-checked Text transports, captured and shadowed
binders, partial actions, chosen Match centers, constant families with unused
divergent arguments, scoped U/F/Pi maps in both directions, diagonal lifting and
discarded transported payloads. Raw Oracle fixtures deliberately isolate reduction
rules; they do not claim well-typedness of arbitrary boundary triples. Negative
cases remain neutral in the reference evaluator and refuse execution in C.
An unknown transported payload cannot be erased during emission.
Both trusted and checked exports are compared to interpreter execution, including
effect order and exactly-once entry forcing. Repeated emission is byte-identical
within each mode. Across modes a saved thunk and a checked `force` use can emit
different, behaviorally equivalent C; this does not relax artifact byte checks.

## LinkerScript

The agent-selected initial script/ABI contract is versioned separately from
A Program syntax. One directive per line; blank lines and `#` comments are
allowed. Tokens may be bare or double-quoted; quoted escapes are `\"` and `\\`
only. Paths are relative to the script. All five singleton directives below
are required; `export` is ordered and repeatable. Unknown/duplicate singleton
directives, unsupported ABI/target versions and duplicate aliases reject.

```text
aplink 1
artifact "program.a"
abi isolated_v1
target host-c11
product archive
export "first" first
export "second" second
```

```sh
/tmp/a-program-c/build/a-to-c --link exports.aplink /tmp/component
cc -std=c11 -I/tmp/component client.c /tmp/component/library.a -o /tmp/client
```

The output directory must not exist. It is staged beside the destination and
published after successful emission and native tools; failures publish nothing.
Like other CLI outputs, the path requires exclusive ownership during execution.
Every structural product contains `component.c`, `component.h`, `runtime.c`,
`runtime.h` and `link.json`. Product roles, not `.a` suffixes, distinguish the input artifact
from the native archive:

| Product | Additional files / entry rule |
| --- | --- |
| `source` | Optional `entry ALIAS` adds main |
| `object` | `component.o`, `runtime.o`; no entry; link runtime once across components |
| `archive` | Objects plus `library.a`; no entry |
| `executable` | Objects plus `program`; `entry ALIAS` required |
| `shared` | PIC objects, `library.so`, `symbols.map`; no entry |

Native products use POSIX `execvp` with explicit argument vectors, no shell.
`--cc TOOL` and `--ar TOOL` select GCC/Clang-compatible host C11 tools (defaults
`cc`, `ar`); compilation uses `-std=c11 -O2 -c`. Executables may specify
`native_script "layout.ld"`; this is actually passed to the GNU/ELF-LLD-compatible
linker through the compiler driver, not merely listed in the receipt. No generic
cross-target profile is claimed. `product shared` compiles with `-fPIC` and links
with `-shared` and an anonymous ELF version script. It requires a host compiler/
linker supporting those options; tool failure returns I/O status 2 and removes
the staging directory. `native_script` remains executable-only.

Shared libraries expose the selected `ap_export_ALIAS` functions and existing
native arena/finite-List helpers only. `symbols.map` derives names from the
selected target ABI; other definitions, including the structural runtime, are
local. The receipt identifies `library.so`, `selected-public-api` visibility and
the `elf-version-script` toolchain requirement. Ordinary clients may link the
explicit library path or use a POSIX loader with the same public signatures.
Keep a loaded library alive until calls and its arena destructors finish; input
and result lifetimes still follow the generated header. This adds no portable
loader, install naming/version policy or shared source nominal identity.

Aliases are `[A-Za-z][A-Za-z0-9_]*`; the C symbol is always `ap_export_ALIAS`,
avoiding keywords, main, runtime functions and private tN names. The public ABI
is `int ap_export_ALIAS(void)`: run one closed export, discard its value, return
0 on success, 2 on runtime/allocation/I/O failure, or 4 on an unsupported demanded
operation. Every call allocates/destroys its own runtime and jump target.
Repeated calls repeat effects; failure does not retain that invocation's state.
No callbacks, boxed values, persistent runtimes or handles cross this ABI, so
emission-local nominal IDs never participate in cross-component comparisons.
This is **not** a general Pi/foreign-function interface. Concurrent calls must
externally coordinate effects such as stdout; signal-handler entry is unsupported.
`AP_C_ISOLATED_ABI` 1 is checked by public headers, independently of runtime ABI 2.

The script cannot request trust or relax checking. Existing CLI admission flags
apply to all exports with a single B/R budget; the receipt records its selected
policy and spent validation, not a proof/authentication of the input file.
Selected roots share one emitted DAG. The old single-entry command uses this
same emitter with one export and a main wrapper. No linker metadata is stored
in `.a`; no kernel, source-language parser or artifact codec was changed.

`check-c-link` tests all four products, shared roots, a multi-export C client,
two separately emitted libraries (including a nominal ADT), failed-call recovery,
real native-script section placement, exact/insufficient aggregate budgets,
invalid scripts, failed native tools, unchanged inputs and header ABI mismatch.
O2 and ASan/UBSan pass against clean `e716232` plus the artifact overlays at
`b6bbb0b`. Existing C differentials, Oracle/Identity checks and checked/trusted
Acc QuickSort also pass. This does not fix the separately failing public
artifact split-fuel gate. Callable Pi/boxed/flat ABIs, shared semantic identity,
foreign imports remain AP5.6 work. `check-c-shared` adds ordinary linked/dlopen
clients for scalar arithmetic, finite record Lists, applied Nat Lists and isolated
exports. It checks exact dynamic definitions, repeated/trusted behavior, active
tags, capacity/depth rollback, arena/library lifetime, map stream/close errors,
tool failures, prior products and unchanged images. Its client sanitizer mode
instruments clients; backend, emitted objects/shared-library bodies stay O2.

The initial packaging issue #46 is closed; it does not imply a general native
C API. #49 and [AP6](../../../doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md#ap6-target-native-public-modules)
now track typed C arguments/results, direct calls and representation lowering.
PR #50's hand-derived Bool sorter is research, not an available lowering profile.
A native-only request must not silently become this status-only structural ABI.
The human-authorized audit disposition now routes open source-semantic and
native-target residuals to #61, superseding historical #44/#49 without changing
representation or erasure authority.

## Native Scalar Profile

Agent implementation decision for AP6.1/AP6.2: `lower/scalar.c` borrows admitted
typed exports and translates the supported scalar subset into temporary C-local
functions/DAGs using the existing iterative `pg_dag` and pointer/environment index. It does
not evaluate source terms or change their graph, classifiers, proofs or artifact.
Source Pi binders become actual C parameters; pure total fixed-width results
become output values. Aliases of an identical export share its private C function.
Known callees are shared across call sites by source Lambda, supplied target
representations and free-binder/capture representations, never by runtime argument values or export
spelling. Only required scalar captures become extra private C parameters.
Membership uses the existing immutable source support trie; it does not copy a
source Context or rescan the function body for every capture. Intermediate C
names are local to actual emitted definitions, not administrative source nodes.

```text
aplink 1
artifact integers.a
abi c_scalar_v1
target host-c11
product archive
lowering scalar_direct_v1
fallback reject
export add add
```

For `add := \x : #Int32 => \y : #Int32 => #int_add x y;`, the generated header
declares `int ap_export_add(int32_t a1, int32_t a2, int32_t *out)`.
Call it from an ordinary C translation unit and link `library.a`:

```c
#include "component.h"
int main(void)
{
	int32_t answer;
	return ap_export_add(20, 22, &answer) || answer != 42;
}
```

Status 0 writes the result; status 1 means a null output pointer. Otherwise the
pointer must name writable storage of the declared type. Int32/Int64 wrapping
add/subtract/multiply/negate use unsigned arithmetic and bounded signed decoding.
The native products contain no structural runtime files, closures, `ap_value`,
allocator or compiler dependencies. `component.h` checks `AP_C_SCALAR_ABI` 1.
An executable requires a zero-argument selected export and discards its result.
The receipt records profile/ABI/fallback and implemented transformations;
`runtime_abi: null` means this profile needs no structural runtime.

Supported: scalar constants/parameters, Return, total-result, syntactic
Force/Thunk, zero-clause Fold and known scalar Lambda calls. Fold may expose a
function to subsequent application. Pending operands retain their original
lexical environment, including through shadowing and nested captures.
Unapplied/partially applied exports with a first-order scalar Pi classifier
receive the remaining arguments from C. Saturated private calls are reused,
not textually inlined at every application.
The result must have an empty effect row and established totality. Unsupported
ADT, Text, Identity, effects and higher-order/dynamic operands reject before
publication. `mul (add x y) (sub x y)` and the explicit block counterpart are
both supported. Recursively cyclic native specializations reject; no new
general recursion semantics is introduced. Effectful arguments cannot be erased
just because a known callee ignores them. Higher-order public/closure data
representations remain outside this scalar profile, not restrictions on the
source language. General native representations remain AP6.3-AP6.5 work.

Native scripts must explicitly request `fallback reject`. There is no automatic
boxed fallback. Existing scripts select `structural_v1` by default, or explicitly
with `lowering structural_v1`; that profile requires `abi isolated_v1`.
Checking/trust and the input `.a` format are unchanged by the profile choice.

## Borrowed Unary Scalar Callbacks

The separate opt-in `callback_direct_v1` / `c_callback_v1` profile accepts
borrowed function inputs whose admitted classifier is a thunk of a unary Pi
from Int32 to a pure TOTAL Int32 result, or Int64 to a pure TOTAL Int64 result.
The codomain must be independent of the argument. Other parameters/results
remain scalars. This does not widen `scalar_direct_v1` or `native_direct_v1`;
their public callback refusals remain status 4.

```text
aplink 1
artifact callbacks.a
abi c_callback_v1
target host-c11
product archive
lowering callback_direct_v1
fallback reject
export once32 once32
```

For `once32 := \f : #Int32 -> #Int32 => \x : #Int32 => f x;`, the public header
checks `AP_C_CALLBACK_ABI` 1 and declares:

```c
struct ap_c_callback_i32 { void *context; int32_t (*call)(void *, int32_t); };
struct ap_c_callback_i64 { void *context; int64_t (*call)(void *, int64_t); };
int ap_export_once32(struct ap_c_callback_i32 a1, int32_t a2, int32_t *out);
```

The caller supplies code/context that implement the admitted pure-total source
function with the declared width. Arbitrary C code is not checked by the backend;
this interpretation, valid context and lifetime are external preconditions.
Code/context must remain alive for the synchronous call, including any loaded
library containing the callback code. Context may be null if the function accepts
it. Generated code neither stores these borrowed values beyond the call nor
returns foreign closures. Callback calls use the existing signed/unsigned width
conversions; successful scalar results are written only after the computation.
Status 1 means null output; status 2 means null callback code and preserves the
output. All callback arguments require valid code, including unused arguments.

Source/object/archive/shared products contain ordinary C code and no structural
runtime. The link receipt states the callback shape, ownership and foreign source
interpretation precondition. Mixed-width, multiple-argument, returned/boxed,
effectful, partial or dependent callback contracts are unsupported. Nominal
representation selections cannot be combined with this profile; callable Acc
fields and native QuickSort remain unsupported.

`check-c-callbacks` compares each product with 400 existing Core-evaluator cases
and 20 admitted same-module source observations using explicit thunk arguments.
Historical imported callback probes rejected on worker E8/Root E9+E10 because
their consumer fixtures omitted explicit imports. Root's later E12 explicit-import
controls preserve original output/fuel and pass; no frontend policy bug or new
source authority follows. Original failed inputs/results remain historical
evidence. Core test interpretations do not grant Surface admission or new
formation/equality evidence. Inert lowering, repeated/trusted output,
null code/output, header version and old profile refusals are separate controls.
In sanitizer mode source component/client bodies are instrumented; emitted
object/archive/shared bodies, backend, producer and Core checker remain O2.

`check-c-callback-modules` adds a generated scalar provider and two independently
generated callback consumers with distinct aliases. Immutable borrowed offset
contexts adapt valid scalar calls into the existing callback signatures. It
checks sixteen product pairs, both header orders, Core/source agreement and
provider lifetime across explicit loading/unloading. Source/object/whole-archive
duplicate symbols remain a link refusal; arbitrary shared-library symbol
interposition is not claimed to reject. The adapter requires valid scalar calls
to return success; this adds no foreign failure propagation or escaping ownership.

## Native Nullary ADTs

`native_direct_v1` / `c_native_v1` extends the same scalar lowering, not another
source evaluator. Select each closed, unindexed, nullary nominal type explicitly:

```text
aplink 1
artifact choices.a
abi c_native_v1
target host-c11
product archive
lowering native_direct_v1
fallback reject
enum32 Bool Bool
export negate negate
```

For `Bool := @{false : *; true : *;};` and ordinary `negate`, this emits:

```c
struct ap_enum_Bool { uint32_t tag; };
#define AP_ENUM_Bool_C0 UINT32_C(0)
#define AP_ENUM_Bool_C1 UINT32_C(1)
int ap_export_negate(struct ap_enum_Bool a1, struct ap_enum_Bool *out);
```

Tags are constructor positions in the selected declaration, not source pointer
IDs or inferred equivalences. `Bool` has no special backend meaning. Separate
selected families produce distinct C struct types even with the same arity.
All enum inputs are checked before calling generated code: status 2 leaves the
output untouched on an invalid position. Status 1 means null output; status 0
writes the result. As for scalar APIs, nonnull output must be valid storage.
Headers check `AP_C_NATIVE_ABI` 1 and use the supported GNU/Clang toolchain's
`#pragma once`. Match becomes a C switch calling only the selected branch's
shared helper; captures and subsequent first-order arguments use the existing
native call lowering. An impossible internal tag aborts rather than fabricating
a source result. These products need no structural runtime or allocator.

Representation selections are admitted by the same adapter as function exports.
The lowerer reads existing declarations; their ambient prefix is allowed, but
index/field extensions are not. Unsaturated type families, fieldful/indexed
constructors, higher-order/effectful signatures and unknown representations
reject. Two selected nominal families deliberately sharing one erased layout
also reject: that layout alone cannot choose their C representation. The profile
does not establish shared nominal identity across independently generated
headers. Aliases must be coordinated by the C client; equal names/counts are not
an A Program type-equivalence proof.

`enum32` and its representation index exist only in this backend. No new `.a`
field, source syntax, Kernel rule or persistent target analysis is introduced.
The receipt records selections in `link.json`, not in the input image. This is
not yet List/slice conversion, Acc erasure or native QuickSort lowering.

`check-c-enum` covers all four products, every input constructor in two/three-case
types, same-shaped distinct families, Match returning a function, Fold/captures,
invalid input positions, repeated headers, unsupported selections, interpreter
differentials, deterministic checked/trusted C and unchanged artifact digests.
The raw Oracle fixture additionally rejects ambiguous reused layouts and checks
native enum emission performs no source evaluation or graph/evidence mutation.

### Known local functions

Known local functions work through the existing direct-call machinery in both
native profiles. For example, an admitted block
`{ f := \y : #Int32 => #int_add y offset; f x; }` retains the lexical `offset`
capture and exposes the enclosing first-order API as ordinary C parameters.
Syntactic function thunks remain target-local known values; no runtime closure,
public callback parameter or compiler dependency enters the product. Closed,
captured, shadowed, repeated, curried and unused definitions are tested, including
Int64 parameters and native Nat/List captures. An unused known function's effectful
body is not executed. A demanded effectful body still rejects the native profile.

Known lexical closures retain their transitive native dependencies through a
finite binding queue. Three/four/eight-function captured-offset chains, shadowing,
repeated demand, signed extrema and Int64 parameters are covered; the exact
former `nested_three` refusal now has positive coverage. Binder identity and
stable lexical order determine captures. This introduces no source evaluation,
runtime closure or public callback ABI. Dynamic callbacks, function fields and
unsupported nested recursive captures remain refusals. Native Acc/QuickSort is
still unsupported.

`check-c-transitive-functions` adds 300 raw evaluator comparisons with inert
emission, plus 600 Int32/seven Int64 cases per source/object/archive product and
six source observations. It retains demanded-effect/callback refusals and
deterministic checked/trusted products. The existing static-function gate keeps
its original raw controls and adds the formerly refused three-closure chain.

`check-c-static-functions` verifies ordinary source/object/archive clients,
scalar extrema and source differentials, Nat32 overflow and resource rollback,
inert raw emission and deterministic checked/trusted products. Two native
components with distinct aliases coexist in one C client and may share the
guarded caller-arena ABI. Explicit array copy-out/copy-in exchanges their List
contents; equal layouts do not establish shared nominal types. Either generated
destructor can release that arena's complete allocation chain. Every result
using the arena must be retired first, including results from the other component.

### Fieldful value data

The same native profile accepts `data SOURCE ALIAS` for an admitted, closed,
unindexed, nonrecursive declaration whose fields are Int32, Int64, selected
enum32/nat32 types or other selected nonrecursive value data. Select each value
data child before its parent; missing or later children refuse. The generated
`struct ap_data_ALIAS` contains a `tag`
and `fields` union. Constructor positions use `AP_DATA_ALIAS_C0`, etc.; a
constructor at position 1 with two fields uses `fields.c1.f0` and `fields.c1.f1`.
Selections and these names belong only to LinkerScript/C, never to `.a`.

For example, the fixture's Packet small constructor has an Int32 and Bool field:

```text
enum32 Bool Bool
data Packet Packet
export small small
export number number
```

```c
struct ap_data_Packet packet;
int32_t result;
ap_export_small(42, (struct ap_enum_Bool){AP_ENUM_Bool_C1}, &packet);
ap_export_number(packet, &result);
```

Closed value records can contain these structs directly. For
`Envelope := @{none : *; pair : Packet -> #Int32 -> *;};`, select Packet first:

```text
enum32 Bool Bool
data Packet Packet
data Envelope Envelope
export wrap wrap
export measure measure
```

The ordinary C client passes and receives whole values without allocation:

```c
struct ap_data_Envelope envelope;
ap_export_wrap(packet, 3, &envelope);
ap_export_measure(envelope, &result);
```

Inputs and results are copied by value with no allocation, handles or structural
runtime. Private calls carry tagged structs directly, including captures and
Match results; only the selected case receives its constructor fields. Scalar
fields preserve wrapping arithmetic through unsigned intermediates and explicit
signed conversion. Enum fields preserve their selected nominal representation.
Public inputs must initialize the tag and all active constructor fields, including
active nested fields; output must be writable. Return 1 denotes null output,
2 an invalid constructor/enum
tag, and 0 success. Validation precedes computation and output storage; invalid
inputs leave output unchanged. A by-value input may also be its output destination.
Inactive union fields and padding have no source observation.

The source/target relation maps an exact selected constructor to its position
and each scalar/enum/value field to its existing representation. Constructor emission
builds those fields; Match passes exactly that case's fields to its private
callee, retaining lexical captures and subsequent first-order operands.
Differentials and raw Oracle checks support this correspondence; no new Kernel
refinement theorem is claimed. Unknown field classifiers, indices,
dependent/function fields, nested recursive pointers and recursive aggregate
payloads reject before publication. Finite value-record payloads have the bounded
single-tail List contract below. Private C input validators inspect
only active constructor fields; they do not perform source checking.
Known block-local function thunks have positive coverage; open callback
parameters, function fields and unselected recursive exports remain negative
controls. Selected
single-tail recursive data has the separate native profile described below.
Distinct selected families remain distinct C struct types; shared nominal types
across independently generated modules still require a future explicit contract.

`check-c-data` covers all products, constructors, Match, captures, partial calls,
returned data, wrapping fields, invalid active tags, output aliasing, duplicate
selectors, nominal mixing, checked/trusted determinism and unchanged input bytes.
`check-c-value-records` verifies 105 cases per source/object/archive product,
16 source observations, nested active-tag validation, whole-value extraction,
capture/return, output aliasing and explicit field/selection-order refusals.
The receipt borrows emitter-selected nested-field metadata. The scalar Oracle
gate additionally tests fieldful and nested value emission without evaluator
calls or source graph/proof mutation, compares 32/64-bit fields against the
evaluator, and refuses ambiguous reused layouts before writing either stream.
General recursive containers and native Acc/QuickSort remain AP6.4/AP6.5 work;
the supported finite List/array conversion is described below.

### Single-tail recursive data

The same `data SOURCE ALIAS` directive now also selects closed unindexed data
with at most one direct Self field per constructor; other fields remain Int32,
Int64, selected enum32 or selected nat32. A two-constructor List with one nullary
terminal and one payload/Self cell may also contain previously selected complete
nonrecursive value data. Record payloads in other recursive shapes still reject.
This covers List/Nat shapes. A recursive selection uses
`const struct ap_data_ALIAS *` for native parameters/results and Self fields.
An explicit terminal constructor is a node; NULL is an invalid source value.
General trees with two Self fields, recursive function/thunk fields, indexed and
dependent containers still reject. The selection does not add artifact fields,
source recursion rules or producer changes.

Components with a recursive selection add `struct ap_c_arena *arena` as the
first argument of every public export. Zero initialize the arena, then release
its allocations with `ap_arena_ALIAS_destroy(&arena)`, where ALIAS is the first
nat32 selection, or the first recursive data selection if there is no nat32.
Inputs are borrowed immutable finite chains; the caller
keeps every reachable node readable until all results sharing it are retired.
Constructor nodes belong to the supplied arena. Identity may return its input;
append copies the left chain and shares its right input. Destroying the arena
frees only its allocations, including nodes made by earlier successful calls.
Keep the arena live and avoid copying it. Result storage must be writable and
separate from input nodes and arena metadata. Independent modules sharing a
nominal family or recursive alias still need a future explicit contract.

```c
struct ap_c_arena arena = {0};
struct ap_data_List nil = {.tag = AP_DATA_List_C0};
const struct ap_data_List *result;
int32_t count;
ap_export_cons(&arena, 42, (struct ap_enum_Bool){AP_ENUM_Bool_C1}, &nil, &result);
ap_export_length(&arena, result, &count);
ap_arena_List_destroy(&arena);
```

Public validation checks active enum/tag fields, non-null tails and cycles with
an iterative two-pointer walk before computation or allocation. Every pointer
must refer to a readable, properly initialized C node; this ABI cannot validate
arbitrary addresses. Return statuses are 0 success, 1 null output/arena, 2 invalid
chain/tag, 3 allocation failure and 4 recursive depth limit. All failures preserve
the output and roll back allocations made by that call; earlier successful
results stay live. An arena `capacity` of zero leaves allocation unrestricted;
otherwise it bounds the total live node count. `depth_limit` zero selects 256;
values 1 through 256 bound simultaneously active recursive Match calls. Larger
limits return 4. These are target resource failures, not source typing rules or
termination evidence. Generated C still uses its native stack, with the limit
checked before each recursive entry.

The emitter recognizes the existing erased recursive-Match fixed-point
template and lowers it to a direct recursive C function. It reads existing
admitted induction result classifiers so a composed List-producing call is not
mistaken for its caller's scalar result. Direct IH thunks remain statically known
lexical closures, forced at their source use. Branch functions retain the thunk's
recursive target, field and native captures; they do not eagerly evaluate unused
branches or pass a dynamic callback across the ABI. Unsupported nested recursive
closure captures and other thunk/function shapes reject explicitly. No evaluator,
substitution, source graph mutation or new checking authority is used by emission.

`check-c-list` covers length, sum, append, identity, construction, composition and
stable selection by a stored Bool flag over 511 length/flag cases. It checks all
native products, source evaluator agreement, determinism, immutable input images,
allocation rollback with actual malloc failure, bounded recursion, malformed
tails/tags/cycles and retained tree/thunk/callback/effect refusals. The raw Oracle
gate separately compares recursive sum/append with the existing evaluator while
forbidding evaluation/substitution during emission and checking graph/store counts.
This gate covers Bool-field selection; the numeric partition and copy-out gate
below extends this boundary. Acc/QuickSort, dynamic callbacks and demanded
Identity remain incomplete.

### Selected applied families and source slices

`data_of VALUE ALIAS` selects the retained closed value-type classifier of an
admitted closed value. For example, `empty := (List Nat).nil` permits:

```text
nat32 Nat Nat
data_of empty Numbers
export identity identity
export length length
```

This does not evaluate `List Nat` as a type factory or synthesize from an expected
type. Native signature domains/results must match the exact selected type.
Unindexed families with explicit reference arguments can use existing scalar or
earlier selected enum/natural/value representations, in source parameter order.
Direct supported fields and single Self tails retain the existing value/node ABI.
One instance per erased constructor/Match layout is allowed in a component;
two instances of the same family in one component refuse rather than invent
source nominal equality. Missing/later argument representations, nonreference
arguments, indexed/dependent/function fields and dynamic callbacks still refuse.
The receipt distinguishes a value-classifier selection from `data`'s value-type
selection; no selector or C convention enters `.a`.

`check-c-applied-families` verifies Nat/enum Lists, reversed field order and a
two-parameter value family, plus existing source take/drop/slice recurrences.
The slice fixture lowers its Match/known induction thunks and composed calls
generically; drop reconstructs its suffix. It is not a replacement target routine
or native Acc/QuickSort completion. Source/object/archive clients compare complete
payloads with source observations and preserve output/prior allocations on resource
failures. The gate also combines separately selected Numbers/Flags products in
both header orders, with explicit C payload-array conversion, shared arena
rollback/lifetime and compile-time incompatible node-pointer rejection. This
uses the versioned C arena ABI without proving source families equal. Guarded
emission checks unchanged source graph/evidence and forbids source-work advances.

### Numeric predicates and finite List copy-out

`nat32 SOURCE ALIAS` explicitly maps a selected closed unindexed declaration
with exactly one nullary constructor and one unary direct-Self constructor to
`uint32_t`. Constructor positions come from the declaration, including reversed
order; their names have no meaning to this selection. The relation is
`Rep(zero, 0)` and `Rep(succ n, k + 1)` when `Rep(n, k)` and `k < UINT32_MAX`.
Every uint32 magnitude denotes that many source successors. This is a finite
foreign domain, not a replacement for all source naturals. Status 5 reports
successor overflow, with unchanged output. A shared arena also holds recursion
depth/status for nat32-only components; these functions need no node allocation.

The existing source `nat_less_or_equal` lowers through Match, predecessor fields,
extra first-order recursive arguments and known IH calls. It is not replaced by
a C comparison primitive. Its result drives stable `lower` (at most pivot) and
`upper` (greater than pivot) List partitions. The source equations and conditional
execution are preserved. The depth limit applies to nested List and Nat matches;
large equal inputs can return 4 even though a source evaluation would terminate.
Neither the width nor depth bound supplies source evidence.

A selected `data` shape with two constructors, one nullary and one containing
exactly a single Self tail plus one Int32/Int64/nat32/enum32 or selected finite
value-record payload, additionally emits
`ap_copy_ALIAS(input, buffer, capacity, written)`. Either constructor order and
either field order are supported. If `Rep(xs, [v0, ..., vn-1])`, a successful call
writes those payloads in order to `buffer[0..n-1]` and sets `*written = n`.
The caller supplies writable capacity in elements; inputs, buffer and `written`
must be separate and remain live for the call. The function validates the entire
finite input and counts its length before writing. Status 6 reports insufficient
capacity or length overflow; statuses 1/2 report null outputs or malformed chains.
All failures preserve the buffer and `written`. An empty List accepts a null
buffer with zero capacity. Input nodes and unused buffer elements remain intact.
The helper allocates no storage and does not depend on the recursive depth limit.
The same shape also emits
`ap_from_ALIAS(arena, array, count, out)`. This copies a finite array slice in
order into arena-owned nodes, including a terminal node; it allocates `count + 1`
nodes. The input array may be released or changed after success. An empty slice
accepts a null array. The caller supplies readable input elements and writable
output separate from the input and arena metadata. Status 1 denotes a null
required pointer, 2 an invalid active element tag, 3 allocation/capacity failure,
4 an active arena invocation,
and 6 a count of SIZE_MAX (the terminal would overflow the node count). Failures
preserve output and prior arena allocations; only new allocations roll back.
Both conversion helpers are iterative and independent of recursive execution
depth limits. The List representation inside generated functions remains a
node chain; this is explicit copying at the C boundary, not slice-based lowering.

Enum payloads use arrays of the selected `struct ap_enum_ALIAS`, preserving their
constructor positions and nominal C type. Copy-in validates every tag before
allocating or changing the arena/output; invalid elements leave prior arena state
and results intact. Copy-out reuses finite-node and active-field validation.
`check-c-enum-list` verifies 875 cases per source/object/archive product and
24 source observations, reversed constructor/field order, all allocation failure
positions, copied-input independence and 300-node conversion. Distinct enum arrays
remain incompatible C types. Multi-payload nodes have no array helper; recursive
aggregate payloads, trees and native Acc/QuickSort remain unsupported.

Finite record payload arrays use the selected `struct ap_data_ALIAS` by value.
All active nested fields are validated before copy-in allocation or copy-out
storage; inactive unions do not require child tags. Copying retains whole scalar,
Int64 and nested enum fields without sharing the input array. The ordinary source
recursive Match still lowers through existing typed views, not a special sorter
or producer change. `check-c-record-list` verifies 259 cases per C product,
13 source observations, reversed fields, nested invalid tags, cycles, resource
rollback and 300-node finite copies. The old Recursive/Aggregates refusal shapes
are explicitly positive; missing/later selections, recursive aggregates, multiple
tails, non-List record payloads, callable/indexed fields remain status-4 controls.

```c
struct ap_c_arena arena = {0};
uint32_t values[] = {3, 1, 4, 1};
const struct ap_data_Numbers *input, *selected;
uint32_t buffer[4];
size_t written;
int status = ap_from_Numbers(&arena, values, 4, &input);
if (!status) status = ap_export_lower(&arena, input, 2, &selected);
if (!status) status = ap_copy_Numbers(selected, buffer, 4, &written);
/* Success yields {1, 1}. Release arena only after all node results are retired. */
ap_arena_Nat_destroy(&arena);
```

The names above are selections from the numeric fixture's LinkerScript. The same
client compiles against source, object or archive products without a source
compiler or structural runtime. Native slice-input sorting and Acc lowering
remain unsupported.

`check-c-numeric-list` checks 1,089 comparator pairs and 27,305 stable partition
cases with ordinary C inputs, source/C output agreement, all native products,
checked/trusted determinism, unchanged `.a`, magnitude/depth/allocation failures,
malformed/cyclic input, finite capacity and reversed Int64 List fields. The raw
array client adds 511 finite slices, 5,110 native partition calls, copied-input
independence, empty slices, 300-node conversion and every allocation failure
position, while preserving earlier results. The raw
Oracle also checks 17 Nat observations and Int32 List conversions while forbidding
evaluation/substitution during emission and checking source graph/store counts.
Invalid Nat shapes, unselected nested fields, dynamic callbacks and demanded
effects retain explicit refusal gates. The old data/List/numeric block controls
now have positive native-product coverage. Native indexed/dependent data, trees,
unsupported recursive closure captures, callbacks/effects and higher Identity
remain unsupported. General cross-module nominal exchange remains open.

Receipts use copied emitter-selected contract flags after lowering. They describe
Nat overflow and finite copy-in/copy-out without rebuilding target representations.
`check-c-sorting-boundary` also admits an open Nat-list QuickSort and requires an
explicit native representation refusal with no published product in checked and
trusted modes. Unsupported applied arguments, indexed SizedList and callable Acc fields
are outside the bounded native contract. Structural QuickSort output remains a
separate gate and is not native completion. No source schema change is requested.

`check-c-scalar` tests source/object/archive/executable products, standalone and
two-module C clients, capture/sequencing, alias sharing, null outputs, ABI/profile
refusal, deterministic C, unchanged `.a` and admission boundaries. Its raw Oracle
fixture compares 400 integer cases with the existing evaluator and asserts no
evaluation/substitution steps or source allocation during emission; a 4,096-level
shared DAG checks iterative traversal and bounded output growth. A 96-level
double-call family emits 97 distinct callees plus its entry, under 100 KB rather
than exponentially inlining them. Source-level differentials exercise nested
calls, partial application, function-returning Fold, captures and shadowing.
Both admission modes produce identical C for the current scalar fixtures. These raw
fixtures test correspondence, not Kernel admission of arbitrary descriptions.
