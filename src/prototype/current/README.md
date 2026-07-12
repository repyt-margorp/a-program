# Current Compiler Snapshot

This directory contains a prototype-side copy of the current accepted compiler
implementation.

It exists so parser, lexer, main, environment, symbol, and term changes can be
developed under `src/prototype/` before any user-approved promotion back into
the accepted source tree.

Layout:

- `src/`: copied implementation files from the accepted `src/`
- `include/`: copied public headers from the accepted `include/`
- `Makefile`: local build for this snapshot

Build this snapshot from this directory:

```
make clean && make
./a.out ../../../examples/stage1.p
```

Do not treat edits here as accepted implementation changes.
