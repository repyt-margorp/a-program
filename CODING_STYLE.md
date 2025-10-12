# Coding Style for A Program

- Identifiers use snake_case.
  - Functions: lower_snake (e.g., `term_arena_init`).
  - Variables/fields: lower_snake.
  - Type-like aliases/struct names: lower_snake or suffix `_t` is acceptable; keep consistent per module. Existing CamelCase will be migrated incrementally.
- Macros and compile-time constants use UPPER_SNAKE (e.g., `T_SORT`, `UL_ZERO`).
- Header guards follow `__FILENAME_H__` (double underscore prefix/suffix).
- No typedef in user-authored code.
  - Do not create aliases for scalar or struct/union types.
  - Always use explicit `struct name` and concrete integer types (`uint32_t`, etc.).
  - Exception: generated code (Bison/Flex) may contain typedefs; these are allowed.
- Comments and documentation must be in English.
  - Write block/line comments, docstrings, and Markdown docs in clear English.
  - Prefer concise rationale over verbose prose; include why-not when helpful.
- All code, identifiers, comments, and documentation (including coding guidelines) must be written in English.
- Files:
  - Headers under `include/` only declare interfaces; no non-trivial inline logic.
  - Source under `src/` implements interfaces.
- C standard: C11 (`-std=c11`).
- Warnings: `-Wall -Wextra` clean builds. Treat warnings as errors when stable.
- Memory: Ownership is explicit. Arena types document what they do NOT free (e.g., inner dynamic arrays) and provide dedicated free helpers when needed.
- External dependencies: only Flex/Bison for parsing, standard C library otherwise.
- Grammars (Bison/Flex) should avoid overlapping `%empty` productions.
  - Prefer right-recursive list/branch rules when optional/sequence constructs are ambiguous because of `%empty` (e.g., `rest -> ε | elem rest`).
  - When left recursion is the natural choice (left-associative operators, etc.), keep it, but do not stack multiple `%empty` productions at the same position.
  - Optional constructs (e.g., the motive in eliminators) should allow `%empty` in only one nonterminal at a given position to avoid shift/reduce or reduce/reduce conflicts.
  - Limit `%empty` to places that really need it; ambiguous optional grammar tends to complicate downstream typing logic.
  - Do **not** rely on `%left`, `%right`, `%precedence`, or `%nonassoc` to encode operator associativity. Express associativity purely in the grammar (lists/recursion). Violations should be rejected during review.

Indentation
- Use tabs for indentation only; do not use spaces for leading indentation in source files.
- Alignment within a line (e.g., after commas or for inline comments) may use spaces, but the leading indent at the beginning of a line must be tabs.

Notes
- Legacy names using CamelCase (e.g., `Term`, `SymTable`) remain temporarily. New code should prefer snake_case and `_t` for typedefs. We will phase-in renaming with mechanical refactors.
