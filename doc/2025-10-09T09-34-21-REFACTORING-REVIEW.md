# Refactoring Review - Current Codebase Analysis

**Date**: 2025-10-09
**Scope**: Lambda calculus interpreter (current implementation)
**Reference**: `program-old/CODING_STYLE.md` for project conventions

---

## Executive Summary

This document identifies inconsistencies, unclear expressions, and areas for improvement in the current codebase. The analysis covers:

1. **Coding convention violations** against the established `CODING_STYLE.md`
2. **Naming inconsistencies** across modules
3. **Documentation gaps** and unclear interfaces
4. **Architectural concerns** (global state, error handling)
5. **Dead code and unused definitions**

---

## 1. Coding Convention Violations

### 1.1 Language Mixing (Critical)

**Issue**: Code contains Japanese comments despite the CODING_STYLE.md requirement:
> "All code, identifiers, comments, and documentation (including coding guidelines) must be written in English."

**Locations**:
- `src/main.c:10-14`: Japanese comments for static memory allocation
- `src/main.c:16-25`: Japanese comments for data structures
- `src/main.c:35-85`: Multiple Japanese comments throughout main()
- `src/environment.c:2,11,16,24,33`: Japanese comments
- `src/symbol.c:5,28,41`: Japanese comments
- `src/term.c:8,21`: Japanese comments
- `src/parse.c:4-8,29,33,36`: Japanese comments
- `include/environment.h:4,10,18,27-45`: Mixed Japanese/English documentation
- `include/symbol.h:7`: Japanese comment
- `include/parse.h:21`: Japanese comment

**Recommendation**: Convert ALL comments and documentation to English.

---

### 1.2 Header Guard Naming Inconsistency

**Issue**: Current header guards follow `__FILENAME_H__` but files under `include/` should potentially use a different pattern than files under `include/kernel/`.

**Current State**:
```c
// Current project
#ifndef __AST_H__
#ifndef __ENVIRONMENT_H__
#ifndef __PARSE_H__
#ifndef __SYMBOL_H__
#ifndef __TERM_H__

// program-old uses
#ifndef __KERNEL_TERM_H__
#ifndef __KERNEL_PARSE_H__
```

**Recommendation**:
- Keep current pattern for now (`__FILENAME_H__`)
- When migrating more from `program-old`, consider namespace prefixing (e.g., `__LAMBDA_TERM_H__`)

---

### 1.3 Typo in Macro Name

**Issue**: `ast.h:7` defines `AST_TAG_INTRODUCTOIN` (typo: should be "INTRODUCTION")

```c
#define AST_TAG_INTRODUCTOIN	4  // ❌ Typo
```

**Recommendation**: Fix typo and ensure consistency with `TERM_TAG_*` naming if AST is still relevant.

---

## 2. Naming Inconsistencies

### 2.1 Tag Constant Naming Patterns

**Issue**: Inconsistent prefixing for tag constants.

**Current State**:
```c
// ast.h (unused file)
#define AST_TAG_NAME          1
#define AST_TAG_APPLICATION   2

// term.h (actively used)
#define TERM_TAG_VAR          1
#define TERM_TAG_CONST        2
#define TERM_TAG_APP          3
#define TERM_TAG_LAMBDA       4
```

**program-old uses**:
```c
#define T_SORT       1
#define T_VAR        2
#define T_PI         3
#define T_LAM        4
#define T_APP        5
```

**Observation**:
- Current code uses verbose `TERM_TAG_*` pattern
- `program-old` uses terse `T_*` pattern
- Both are valid per CODING_STYLE.md (UPPER_SNAKE for macros)

**Recommendation**:
- Decide on one pattern project-wide (suggest `T_*` for brevity, matching program-old)
- OR keep `TERM_TAG_*` and document rationale

---

### 2.2 Struct Field Naming: `as` Union

**Issue**: All term-like structures use `as` for the union field, which is a C keyword (though legal here).

```c
struct term {
    int tag;
    union {
        struct term_var      as_var;
        struct term_const    as_const;
        struct term_app      as_app;
        struct term_lambda   as_lambda;
    } as;  // ← "as" is a C++ keyword, legal in C but could confuse C++ interop
};
```

**program-old uses**: Same pattern (`as` union field)

**Recommendation**: Keep for consistency with `program-old`, but document that C++ interop is not a goal.

---

### 2.3 Function Naming Consistency

**Current Functions**:
- `symbol_table_init` ✅
- `symbol_table_free` ✅
- `symbol_intern` ✅ (no prefix, but clearly in symbol.h)
- `symbol_to_string` ✅
- `symbol_map_is_used_at` ❓ (why no `symbol_table_` prefix?)

**Recommendation**: Rename `symbol_map_is_used_at` → `symbol_table_map_is_used_at` or just `symbol_map_is_used` for clarity.

---

## 3. Dead Code and Unused Definitions

### 3.1 Unused AST Module

**Issue**: `include/ast.h` defines a complete AST structure but is **never included** by any source file.

**Evidence**:
```bash
$ grep -r "ast.h" src/ include/
include/ast.h:1:#ifndef __AST_H__
# No other matches in src/ or active headers
```

**Content**: Defines `AST_TAG_*` constants and `struct ast` with fields for name, application, lambda, introduction, elimination.

**Recommendation**:
- **Remove** `ast.h` if AST is replaced by direct `term` construction
- **OR** document if AST will be used in future migration stages from `program-old`

---

### 3.2 Unused Global Environment Functions

**Issue**: `global_environment_define` and `global_environment_lookup` are **defined but never called**.

**Evidence**:
```bash
$ grep -r "global_environment_define" src/
src/environment.c:10:int global_environment_define(...) {
src/parser.y:55:        // global_environment_define(state->globals, $1, $3);
```

The call in `parser.y:55` is **commented out**.

**Recommendation**:
- Keep functions (needed for future top-level definition support)
- Add TODO comment explaining when they'll be activated
- OR remove if not part of current stage

---

### 3.3 Unused `symbol_map_is_used_at` Function

**Issue**: Defined in `symbol.c:106-111` but never called anywhere.

**Recommendation**: Remove or mark as debug/diagnostic utility if intentionally kept.

---

## 4. Architectural Concerns

### 4.1 Global State in Parser/Lexer (Major)

**Issue**: Lexer relies on a global variable `g_current_parse_state` to access the parse state, violating encapsulation.

**Location**: `src/lexer.l:11`
```c
struct parse_state* g_current_parse_state = NULL;
```

**Used in**:
- `lexer.l:39` (symbol_intern call)
- `lexer.l:44-47` (error reporting)
- `parse.c:6,30,38` (extern declaration and setting/unsetting)

**Problem**:
- Not thread-safe
- Prevents multiple concurrent parsers
- Hard to test in isolation

**Recommendation**:
- Document this limitation as "single-threaded parser only"
- Consider migrating to reentrant lexer when time permits (requires refactoring lexer/parser integration)

---

### 4.2 Inconsistent Error Handling

**Issue**: Error handling patterns vary across modules.

**Patterns Observed**:
1. **Return -1 on error** (most common)
   - `symbol_intern`: returns -1
   - `term_create_*`: returns -1
   - `parse_file`: returns -1

2. **Return NULL on error**
   - `symbol_to_string`: returns NULL
   - `global_environment_lookup`: returns NULL
   - `term_arena_alloc`: returns NULL

3. **Error count in state**
   - `parse_state.error_count` incremented on lexer/parser errors

**Recommendation**:
- Document convention: functions returning `int` use -1 for errors, functions returning pointers use NULL
- Add error code constants if needed (e.g., `#define SYMBOL_ERR_FULL -1`)

---

### 4.3 Memory Management Clarity

**Issue**: Arena allocator never frees memory, but `symbol_table_free` does free strings.

**Current Documentation**: None explicit about ownership.

**Recommendation**: Add ownership documentation to headers:
```c
/**
 * @brief Allocate from arena (never freed; arena is reset as a whole)
 */
void* term_arena_alloc(...);

/**
 * @brief Free all interned strings (call at shutdown)
 * @note Does NOT free the arrays passed to symbol_table_init
 */
void symbol_table_free(struct symbol_table* t);
```

---

## 5. Documentation Gaps

### 5.1 Missing Module-Level Documentation

**Issue**: Header files lack module-level overview comments.

**Example**: `term.h` defines arena, tags, and functions but doesn't explain:
- What is the arena's lifecycle?
- What is the "locally nameless" representation?
- Why are there two reduce functions (term_reduce vs term_evaluate)?

**Recommendation**: Add file-level comments at top of each header:
```c
/**
 * @file term.h
 * @brief Core term representation using locally nameless encoding
 *
 * Terms represent lambda calculus expressions with:
 * - VAR: variable bound to a lambda (points to binder)
 * - CONST: free variable or global constant (symbol ID)
 * - APP: function application
 * - LAMBDA: lambda abstraction
 *
 * Memory is allocated from a term_arena (bump allocator).
 */
```

---

### 5.2 Undocumented Bison/Flex Integration

**Issue**: `parse.c` has partially outdated comments:

```c
// --- Bison/Flex Re-entrant API Forward Declarations ---
//
// これらは、Bison/Flexが自動生成する関数の宣言です。
// 次のステップで、`parser.y` と `lexer.l` を修正し、
// これらの関数が `p_` というプレフィックスで生成されるように設定します。
```

The comment mentions "`p_` prefix" but the actual code uses standard `yy` prefix.

**Recommendation**: Update or remove stale comments.

---

### 5.3 Unclear `binding_context` Definition

**Issue**: `struct binding_context` is defined **inside** `parser.y` but forward-declared in `parse.h`.

**Location**:
- `parse.h:9`: Forward declaration
- `parser.y:21-25`: Actual definition

**Problem**: C requires complete type for `parse_state.local_binder_stack` member, so forward declaration is insufficient if other code needs to manipulate it.

**Recommendation**: Move `struct binding_context` definition to `parse.h` or make it opaque.

---

## 6. Code Quality Issues

### 6.1 Magic Numbers

**Issue**: Several hardcoded constants without explanation.

**Examples**:
- `main.c:11-14`: Capacities like 8192, 4096, 256, 1MB
- `term.c:37`: Depth limit 20
- `term.c:146`: Step limit 10000

**Recommendation**:
- Move to named constants with comments explaining rationale
- Example:
```c
// Maximum recursion depth for pretty-printing (prevents stack overflow on cycles)
#define TERM_PRINT_MAX_DEPTH 20

// Maximum beta-reduction steps (prevents infinite loops)
#define TERM_EVAL_MAX_STEPS 10000
```

---

### 6.2 Inconsistent Commenting Style

**Issue**: Mix of line comments (`//`) and block comments (`/* */`), both English and Japanese.

**Examples**:
```c
// --- Arena Allocator ---                 (English, section header)
// メモリアドレスを、指定された...         (Japanese, inline)
/* skip whitespace */                      (English, lexer rule)
// FNV-1a ハッシュ関数                      (Japanese, function header)
```

**Recommendation**: Standardize on:
- `//` for single-line comments
- `/* ... */` for multi-line block comments
- Section headers: `// ---` style (as used in term.c)
- **All English**

---

### 6.3 Inconsistent Null Checks

**Issue**: Some functions check for null, others don't.

**Examples**:
```c
// Checks null:
void* term_arena_alloc(struct term_arena* arena, ...) {
    if (!arena || !arena->memory) return NULL;
}

// Does NOT check null:
int term_create_var(struct term_arena* arena, ...) {
    struct term* nt = term_arena_alloc(arena, ...);  // No check if arena is null
    if (!nt) return -1;
}
```

**Recommendation**: Document preconditions clearly:
- "Caller must ensure `arena` is non-null"
- OR add defensive checks everywhere

---

## 7. Structural Issues

### 7.1 Parser/Lexer Not Truly Reentrant

**Issue**: Despite using Bison's pure API (`%define api.pure full`), the lexer uses global state.

**Evidence**:
- `lexer.l:1`: `%option noyywrap` but NOT `%option reentrant`
- `lexer.l:11`: `struct parse_state* g_current_parse_state = NULL;`

**Recommendation**: Document limitation or migrate to fully reentrant lexer.

---

### 7.2 Unused `lex-param` in Parser

**Issue**: `parser.y:6` declares `%lex-param { struct parse_state* state }` but the lexer doesn't actually use this parameter (it uses global state instead).

**Recommendation**: Remove `%lex-param` or migrate to reentrant lexer.

---

## 8. Potential Bugs

### 8.1 Term Evaluation May Loop Forever

**Issue**: `term_evaluate` has a hardcoded step limit (10000) but doesn't report that it hit the limit.

**Location**: `term.c:142-153`
```c
for (int step = 0; step < 10000; step++) {
    if (term_reduce(arena, &next, current) != 0) return 1;
    if (next == current) { *p_ret = current; return 0; }
    current = next;
}
*p_ret = current;
return 0;  // ← Returns success even if limit was hit!
```

**Recommendation**: Return error code or log warning when step limit is reached.

---

### 8.2 Print Function Recursion Depth Check

**Issue**: `term_print_recursive` checks depth > 20 but doesn't print a warning.

**Location**: `term.c:37`
```c
if (depth > 20) { printf("..."); return; }
```

**Recommendation**: Add stderr warning for debugging:
```c
if (depth > 20) {
    fprintf(stderr, "[warning: print depth limit reached]\n");
    printf("...");
    return;
}
```

---

## 9. Summary of Recommendations

### High Priority (Breaking or Critical)

1. **Convert all Japanese comments to English** (CODING_STYLE.md violation)
2. **Fix typo**: `AST_TAG_INTRODUCTOIN` → `AST_TAG_INTRODUCTION`
3. **Remove or document unused `ast.h`** (dead code)
4. **Document global state limitation** in parser/lexer (non-reentrant)

### Medium Priority (Quality/Consistency)

5. **Standardize tag constant naming** (`TERM_TAG_*` vs `T_*`)
6. **Move `binding_context` definition** to `parse.h` or make opaque
7. **Add module-level documentation** to all headers
8. **Replace magic numbers** with named constants
9. **Fix outdated comments** in `parse.c` about `p_` prefix

### Low Priority (Nice-to-Have)

10. **Add ownership documentation** to memory management functions
11. **Standardize error handling** documentation
12. **Remove unused functions** or mark as utilities
13. **Add warning for evaluation step limit** reached
14. **Improve null-check consistency**

---

## 10. Migration Checklist (from program-old)

When migrating features from `program-old`, ensure:

- [ ] All comments are in English
- [ ] Tag constants follow chosen pattern (recommend `T_*` for consistency)
- [ ] Functions use `lower_snake` naming
- [ ] Macros use `UPPER_SNAKE`
- [ ] No `typedef` for user-defined types
- [ ] Headers use `__FILENAME_H__` guards
- [ ] Tab indentation only (no leading spaces)
- [ ] `-Wall -Wextra` clean compilation

---

## Appendix: File-by-File Issues

### `include/ast.h`
- **UNUSED FILE** - remove or document purpose
- Typo: `AST_TAG_INTRODUCTOIN`

### `include/environment.h`
- Japanese comments (lines 4, 10, 18, 27-45)
- Missing module-level documentation

### `include/parse.h`
- Japanese comment (line 21)
- `binding_context` only forward-declared (definition in parser.y)

### `include/symbol.h`
- Japanese comment (line 7)
- Missing module-level documentation

### `include/term.h`
- Missing module-level documentation (what is locally nameless?)
- `term_reduce` exported but marked static in forward declaration (inconsistency resolved)

### `src/main.c`
- Extensive Japanese comments (lines 10-85)
- Magic numbers (capacities)
- Debug print statements (`--- System initialized ---`)

### `src/environment.c`
- Japanese comments (lines 2, 11, 16, 24, 33)

### `src/symbol.c`
- Japanese comments (lines 5, 28, 41)
- Unused function: `symbol_map_is_used_at`

### `src/term.c`
- Japanese comments (lines 8, 21)
- Magic numbers (depth 20, steps 10000)
- No warning when limits reached

### `src/parse.c`
- Japanese comments (lines 4-8, 29, 33, 36)
- Outdated comment about `p_` prefix
- Global state usage documented but not clearly warned

### `src/parser.y`
- Commented-out code (line 55: global_environment_define)
- `%lex-param` declared but not actually used (lexer uses global state)

### `src/lexer.l`
- Global state (`g_current_parse_state`)
- Not truly reentrant despite Bison pure API

---

**End of Review**
