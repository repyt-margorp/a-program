# Prototype C Backend

## Problem List

1. Produce executable C from `.a` without making C the owner of artifact semantics.
2. State the support and acceptance boundaries, rather than erase unsupported proofs.
3. Keep LinkerScript, native products and the public C ABI downstream of `.a`.

## Subjective (User)

2026-09-29, paraphrase: continue artifact implementation through C transpilation;
push verified increments. Preserve the single-fuel policy and separation of
computation from typing. This supersedes the previous wait-before-C instruction.

## Objective (Code)

`emit.c` reads a borrowed closed `pg_occurrence`, Core DAG and public Oracle
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
- Entry selection uses the first image root as a source module. Exported
  entries must be returning computations or closed values, not unapplied Pi.

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
Every product contains `component.c`, `component.h`, `runtime.c`, `runtime.h`
and `link.json`. Product roles, not `.a` suffixes, distinguish the input artifact
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
