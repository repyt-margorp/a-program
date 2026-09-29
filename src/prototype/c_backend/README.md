# Prototype C Backend

## Problem List

1. Produce executable C from `.a` without making C the owner of artifact semantics.
2. State the support and acceptance boundaries, rather than erase unsupported proofs.
3. Keep LinkerScript, native products and the public C ABI downstream of `.a`.

## Subjective (User)

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

Native products use POSIX `execvp` with explicit argument vectors, no shell.
`--cc TOOL` and `--ar TOOL` select GCC/Clang-compatible host C11 tools (defaults
`cc`, `ar`); compilation uses `-std=c11 -O2 -c`. Executables may specify
`native_script "layout.ld"`; this is actually passed to the GNU/ELF-LLD-compatible
linker through the compiler driver, not merely listed in the receipt. No generic
cross-target profile or generated shared-library export map is claimed.

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
foreign imports and shared-library visibility remain AP5.6 work.

The initial packaging issue #46 is closed; it does not imply a general native
C API. #49 and [AP6](../../../doc/2026-09-28-ARTIFACT-SEMANTIC-PERSISTENCE-REFACTOR-PLAN.md#ap6-target-native-public-modules)
now track typed C arguments/results, direct calls and representation lowering.
PR #50's hand-derived Bool sorter is research, not an available lowering profile.
A native-only request must not silently become this status-only structural ABI.

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
