# TypeDeclarationDB Memory Leak Remediation Report

Date: 2026-08-12

Status: implemented and verified

Finding: `BH-CMEM-001`

## Summary

LeakSanitizer reported a direct leak of 3072 bytes from
`prototype_type_declaration_db_init`. The initializer allocated the type
representation table with `calloc`, but `prototype_type_declaration_db` had no
destruction contract and its normal owners did not release that allocation.
The leak occurred on successful compilation as well as rejected input paths.

The fix removes the hidden allocation. Representation storage is now supplied
by the database owner, matching every other fixed-capacity array in
`prototype_type_declaration_db`.

## Root Cause

The database previously mixed two ownership models:

- type declarations, constructors, parameters, readback fields, and type
  expressions were borrowed from caller-owned storage;
- type representations were allocated internally and implicitly owned by the
  database.

No destroy API existed to discharge the second ownership mode. Adding cleanup
only to the drivers would have left early-return paths, test fixtures, and future
owners vulnerable to the same leak.

## Implemented Contract

`prototype_type_declaration_db_init` now receives:

```c
struct prototype_type_representation* representations,
size_t representation_capacity
```

The database borrows this storage. The public structure declaration now states
that all storage referenced by the database is borrowed from its owner.

The following owners were migrated:

- file reader and artifact inspection driver;
- REPL driver;
- artifact closure/publication graph;
- kernel and representation checks;
- CBPV, Context, conversion, universe, shared-term, and HOTT fixtures.

The artifact closure graph remains dynamically allocated as a whole. It now
allocates and frees its representation array alongside its other arrays instead
of relying on a hidden allocation inside the database initializer.

## Empty Database Invariant

The migration exposed a pre-existing dependency on the implementation-defined
result of `calloc(0, size)`. An artifact closure with zero type declarations has
no representation storage. Representation rebuilding now treats this as the
canonical empty database:

- `representation_count = 0`;
- `representations_dirty = 0`;
- success without dereferencing representation storage.

A non-empty database still requires non-null storage with capacity at least
`type_count`.

## Verification

The following checks passed after the change:

1. `make -f src/prototype/Makefile all`
2. `make -f src/prototype/Makefile test-integration`
3. ASan and LeakSanitizer on
   `examples/type-infer-and-check/level0/01_function.p`
4. ASan and LeakSanitizer across artifact write and readback
5. ASan and LeakSanitizer on an empty input
6. ASan and LeakSanitizer on rejected recursive-IH input
7. ASan and LeakSanitizer on rejected CBPV computation-block input
8. ASan and LeakSanitizer on REPL initialization followed by `:quit`

The sanitizer runs produced no leak report. Rejected inputs retained their
expected nonzero status and diagnostics.

The complete integration suite also exercises the empty artifact-publication
case and the HOTT fixture ownership boundary.

## Result

`prototype_type_declaration_db` no longer performs heap allocation. Its lifetime
is now independent of individual driver return paths, and its ownership model is
consistent across the prototype kernel and artifact implementation.
