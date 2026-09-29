# Prototype C Backend

## Problem List

1. Produce executable C from `.a` without making C the owner of artifact semantics.
2. State the support and acceptance boundaries, rather than erase unsupported proofs.

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
selection through existing Solve, then emission and atomic publication. Imported
typed structure alone grants no acceptance. The adapter reports reconstruction
fuel and refuses pending/rejected entries, including invalid siblings and `::`.

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
generic runtime type arguments. The emitter also realizes the kernel's diagonal
transport equation in either direction when `Act A` has a known inert reference
as `A` (host type, rigid classifier or nominal declaration). It follows the
transported value without modifying the source DAG or evaluating it. This is a
restricted compilation rule, not Nat compression or general proof erasure.

Limitations:
- Other Identity Act/transport/lift and unknown reachable Oracles reject before
  publishing C, even in a syntactically reachable but unused branch. The existing
  Acc QuickSort has an `identity-field` dependency, so it is **not yet compiled**.
  `check-c-sorting-boundary` confirms valid interpreter output `FFTT`, explicit
  backend rejection and no emitted file. This is not a successful C sort test.
- The command adapter currently reconstructs/checks via ordinary Solve. It does
  not implement trusted cache admission, validation sublimits or exact suspended
  frontier restoration. A `--steps 0` import does not emit unchecked code.
- Runtime allocation is invocation-wide; no garbage collection, tail-call
  guarantee, bounded memory or performance claim. C stack/heap resources bound
  execution. The ABI is versioned locally, never written into canonical `.a`.
- Entry selection uses the first image root as a source module. Executable
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
Use a new overlay directory. The adapter accepts `--steps N` and an explicit
fixed `--image-limit N` (default 1,000,000), or explicit `--image-limit none`,
independent of file bytes or fuel. Removing the quota does not confer trust or
skip format/representability checks. It does not reduce the retained graph.
Exit codes: 0 emitted, 1 rejected, 2 input/I/O failure, 3 pending, 4 unsupported.

Tests compare standalone output with the interpreter, require deterministic C
and unchanged input-image digests, and cover zero fuel, input-only images,
generic List, nominal ADTs, host bounds, NUL, repeated thunks, handlers and output
failure. Raw Oracle tests separately check all Int64 operations and two distinct
operation labels; these descriptive test inputs are not acceptance receipts.
They assert emission performs no evaluator/substitution steps or graph mutation.
Diagonal tests include kernel-checked Text transports in both directions, a
lexically captured payload and raw classifier-head correspondence. Lifts,
unknown families and actions of binders/functions still reject with empty output;
an unsupported transported payload cannot be erased by the diagonal rule.
