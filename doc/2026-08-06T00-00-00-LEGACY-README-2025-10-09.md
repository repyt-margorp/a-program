# Core Language Specification

**Date**: 2025-10-09
**Status**: In development (migrating from program-old)

This document defines the Core term language — the **only** intermediate representation in this system. Unlike traditional compilers, there is no separate AST layer; the parser translates `.p` files directly into Core terms.

---

## Table of Contents

1. [Design Goals](#design-goals)
2. [Core Term Constructors](#core-term-constructors)
3. [Data Structures](#data-structures)
4. [Universe Levels](#universe-levels)
5. [Inductive Types](#inductive-types)
6. [Pattern Matching](#pattern-matching)
7. [Equality and Identity Types](#equality-and-identity-types)
8. [Global Environment](#global-environment)
9. [Evaluation Strategy](#evaluation-strategy)
10. [Type Checking](#type-checking)
11. [What is Intentionally Absent](#what-is-intentionally-absent)

---

## Design Goals

### Minimal Yet Sufficient

The Core language is designed to be:
- **Minimal**: Small enough for manual inspection and formal verification
- **Sufficient**: Expressive enough for indexed inductive types and dependent pattern matching
- **Canonical**: Uses de Bruijn indices and spine form for predictable equality
- **Direct**: No elaboration layer; parser output = type checker input

### Not a Research Vehicle

This is **not** an experiment in:
- Type inference algorithms
- Implicit argument synthesis
- Syntactic sugar translation
- Advanced elaboration techniques

Instead, it is a **typed assembly language** for dependent types, where all complexity is explicit.

---

## Core Term Constructors

### Term Tags (`struct term.tag`)

```c
enum {
    T_SORT = 1,      // Universe Type u
    T_VAR,           // De Bruijn indexed variable
    T_PI,            // Dependent function type Π(x:A).B
    T_LAM,           // Lambda abstraction λ(x:A).body
    T_APP,           // Application (spine form)

    T_CONST = 10,    // Global constant reference
    T_INDUCTIVE,     // Inductive type reference
    T_CTOR,          // Constructor
    T_MATCH,         // Pattern match eliminator

    T_META = 14,     // Meta-variable (hole for inference)
    T_ANN,           // Type annotation (term : type)

    T_EQ,            // Propositional equality type
    T_REFL,          // Reflexivity constructor
    T_EQ_ELIM,       // Equality eliminator (J-rule)

    T_IH_CALL,       // Induction hypothesis call (*k)
};
```

### Term Structure

```c
struct term {
    int tag;
    struct span loc;  // Source location for error messages
    union {
        struct { struct univ_level level; } sort;
        struct { uint32_t binder_ref; uint32_t name; } var;
        struct { struct telescope dom; uint32_t cod; } pi;
        struct { struct telescope binders; uint32_t body; } lam;
        struct { uint32_t fn; uint32_t* args; uint32_t argc; } app;
        struct { uint32_t name; } const_;
        struct { uint32_t name; } inductive;
        struct { uint32_t name; uint32_t inductive_id; uint32_t ctor_index; } ctor;
        struct match_core match_;
        struct { uint32_t id; } meta;
        struct { uint32_t term; uint32_t type; } ann;
        struct { uint32_t type; uint32_t lhs; uint32_t rhs; } eq;
        struct { uint32_t type; uint32_t term; } refl;
        struct { uint32_t type; uint32_t motive; uint32_t base;
                 uint32_t lhs; uint32_t rhs; uint32_t proof; } eq_elim;
        struct { uint32_t param_name; uint32_t param_index;
                 uint32_t match_term; } ih_call;
    } as;
};
```

---

## Data Structures

### Term Arena

All terms are stored in a flat arena with hash-consing for sharing:

```c
struct term_arena {
    struct term* data;
    uint32_t len;
    uint32_t cap;
};
```

**Properties**:
- Terms referenced by **ID** (uint32_t index into arena)
- Immutable once created (functional data structure)
- Automatic sharing of identical subterms
- Efficient equality testing (pointer equality when hash-consed)

### Binders and Telescopes

```c
struct binder {
    uint32_t name;       // Symbol ID (for printing)
    uint32_t type;       // Type term ID
    bool implicit;       // Implicit argument flag
    uint8_t relevance;   // REL_RUNTIME or REL_ERASED
};

struct telescope {
    struct binder* params;
    uint32_t len;
};
```

**Telescope**: Sequence of binders for Π-types and λ-abstractions
- Allows multiple binders in one node (efficiency)
- Example: `Π(A:Type)(x:A)(y:A). ...` → single Pi node with 3 binders

### Relevance Annotations

```c
#define REL_RUNTIME 0  // Must be kept at runtime
#define REL_ERASED  1  // Can be erased (proof term)
```

Used for **proof erasure** in future optimization passes.

---

## Universe Levels

### Level Structure

```c
struct univ_level {
    int tag;
    uint32_t a, b, v, k;  // Interpretation depends on tag
};

enum {
    UL_ZERO = 1,  // Type 0
    UL_SUCC,      // Type (u+1)
    UL_MAX,       // Type (max u v)
    UL_VAR,       // Type u (universe variable)
};
```

### Level Expressions

- `UL_ZERO`: Base universe `Type 0`
- `UL_SUCC(u)`: Successor `Type (u+1)`
- `UL_MAX(u,v)`: Maximum `Type (max u v)`
- `UL_VAR(i)`: Universe variable `Type u_i`

**Current implementation**: Only `Type 0` is used. Full universe polymorphism planned for future.

### Universe Checking Rules

```
Γ ⊢ Type u : Type (u+1)        (Universe hierarchy)

Γ, x:A ⊢ B : Type v
─────────────────────────      (Pi formation)
Γ ⊢ Π(x:A).B : Type (max u v)
  where Γ ⊢ A : Type u
```

---

## Inductive Types

### Inductive Definition Structure

```c
struct inductive_def {
    uint32_t name;              // Type name
    struct telescope params;     // Type parameters
    struct telescope indices;    // Indices (for indexed families)
    uint32_t param_count;       // Number of parameters
    uint32_t sort;              // Universe term
    struct ctor_def* ctors;     // Constructors
    uint32_t ctor_count;
};

struct ctor_def {
    uint32_t name;              // Constructor name
    uint32_t type;              // Full type (term ID)
    uint32_t ctor_index;        // Index in inductive's ctor array
};
```

### Parameters vs Indices

**Parameters**: Uniform across all constructors
```scheme
List := \A : @ => @{
  nil  : *;
  cons : A -> * -> *;
};
// A is a parameter (same for nil and cons)
```

**Indices**: Vary per constructor
```scheme
Vec := \A : @ => \n : Nat => @{
  nil  : Vec A Nat.zero;                    // Index: Nat.zero
  cons : A -> Vec A n -> Vec A (Nat.succ n); // Index: Nat.succ n
};
// n is an index (different for nil and cons)
```

### The `*` Self-Reference Marker

**In IR syntax**, `*` represents "the type being defined":

```scheme
Nat := @{
  zero : *;        // * = Nat
  succ : * -> *;   // * -> * = Nat -> Nat
};
```

**Elaboration**: Parser/elaborator replaces `*` with actual type reference
**Guarantee**: Enforces that constructors return the correct type
**Positivity**: Structural constraint prevents non-positive occurrences

---

## Pattern Matching

### Match Structure

```c
struct match_core {
    uint32_t scrutinee;       // Term being matched
    uint32_t inductive_id;    // Which inductive type
    uint32_t* branches;       // Array[ctor_count], indexed by ctor_index
    uint32_t branch_count;    // = ctor_count
    uint32_t motive;          // Return type (dependent on scrutinee)
    struct match_ih_info* ih_refs;  // Induction hypothesis references
    uint32_t ih_ref_count;
};
```

### Indexed Match Semantics

**Key innovation**: Branches are **indexed by constructor index**, not sequential.

```c
// Constructor with ctor_index=2 → branches[2]
// O(1) lookup, no linear search needed
```

**Example**:
```scheme
Bool := @{
  true : *;   // ctor_index = 0
  false : *;  // ctor_index = 1
};

not : Bool -> Bool := \b : Bool =>
  b @true => Bool.false    // branches[0]
    @false => Bool.true;   // branches[1]
```

### Motive (Return Type)

For **dependent pattern matching**, the motive specifies the return type as a function of the scrutinee:

```
motive : (x : Ind params indices) -> Type
```

**Example** (dependent elimination):
```scheme
// Simple (non-dependent): motive = λ(_:Nat). Nat
pred : Nat -> Nat := \n : Nat =>
  n @zero => Nat.zero
    @succ k => k;

// Dependent: motive = λ(n:Nat). Vec A n
// (return type depends on scrutinee value)
```

### Induction Hypothesis (`*k`)

For **recursive constructors**, `*k` invokes the induction hypothesis:

```scheme
add : Nat -> Nat -> Nat := \n : Nat =>
  n @zero => (\m : Nat => m)
    @succ k => (\m : Nat => Nat.succ (*k m));
                                      ↑
                              Induction hypothesis:
                              recursive call with k
```

**Core representation**:
```c
struct {
    uint32_t param_name;   // k
    uint32_t param_index;  // De Bruijn index of k
    uint32_t match_term;   // ID of enclosing match
} ih_call;
```

**Evaluation**: `*k` re-evaluates the match with `k`'s value to get recursive result.

---

## Equality and Identity Types

### Propositional Equality

```c
// T_EQ: Propositional equality type
struct { uint32_t type; uint32_t lhs; uint32_t rhs; } eq;

// Eq A x y  ≡  "x equals y at type A"
```

**Constructor**: Only reflexivity
```c
// T_REFL: Reflexivity proof
struct { uint32_t type; uint32_t term; } refl;

// refl : Π(A:Type)(x:A). Eq A x x
```

### Equality Elimination (J-rule)

```c
// T_EQ_ELIM: Equality eliminator
struct {
    uint32_t type;    // A
    uint32_t motive;  // P : Π(x y:A). Eq A x y -> Type
    uint32_t base;    // base : Π(x:A). P x x refl
    uint32_t lhs;     // x
    uint32_t rhs;     // y
    uint32_t proof;   // p : Eq A x y
} eq_elim;
```

**Reduction rule**:
```
eq_elim A P base x x refl  ⟶  base x
```

**Use case**: Transport along equality, substitution, proving properties

---

## Global Environment

### Structure

```c
struct global_env {
    struct const_def* consts;
    uint32_t const_count;

    struct inductive_def* inds;
    uint32_t ind_count;
};
```

### Constant Definitions

```c
struct const_def {
    uint32_t name;
    uint32_t type;      // Type term
    uint32_t body;      // Definition (0 if axiom/opaque)
    bool opaque;        // If true, block δ-reduction
};
```

**δ-reduction**: Unfold non-opaque constants
```
Γ ⊢ c ↝ body    (if c is defined and not opaque)
```

---

## Evaluation Strategy

### WHNF (Weak Head Normal Form)

Evaluation reduces to WHNF using:

- **β-reduction**: `(λx.body) arg ⟶ body[arg/x]`
- **δ-reduction**: `const ⟶ body` (if non-opaque)
- **ι-reduction**: `match (Ctor args) { ... | Ctor x => rhs | ... } ⟶ rhs[args/x]`

### Closure-Based Evaluation

```c
struct value {
    int tag;  // V_SORT, V_PI, V_LAM, V_NEUTRAL, V_REFL, V_IH_CALL
    // ... tag-specific data
};

struct env {
    struct value** data;  // Runtime environment
    uint32_t len;
};
```

**Closure**: `{ env, binders, body }`
- Captures environment at lambda creation
- Application extends environment without copying

**Neutral values**: Variables and stuck computations
```c
struct {
    int head_tag;      // H_VAR or H_CONST
    uint32_t head_id;
    struct value** args;  // Spine of arguments
    uint32_t argc;
} neutral;
```

### Evaluation Algorithm

```c
struct value* eval_whnf(struct eval_ctx* ctx,
                        struct env* env,
                        uint32_t term_id);
```

**Strategy**:
1. Lookup term in arena
2. Match on term tag:
   - `T_VAR`: Lookup in environment
   - `T_LAM`: Create closure
   - `T_APP`: Evaluate function, then apply
   - `T_CONST`: Unfold if non-opaque, else neutral
   - `T_MATCH`: Evaluate scrutinee, dispatch to branch
   - `T_SORT`, `T_PI`, etc.: Return corresponding value

**Normalization**: Current implementation stops at WHNF. Full normalization via NbE planned for future.

---

## Type Checking

### Bidirectional Type Checking

```c
// Infer mode: compute the type of a term
int type_infer(struct type_ctx* tc,
               struct local_ctx* lctx,
               uint32_t term,
               uint32_t* out_type);

// Check mode: verify term has expected type
int type_check(struct type_ctx* tc,
               struct local_ctx* lctx,
               uint32_t term,
               uint32_t expected_type);
```

### Type Checking Context

```c
struct local_ctx {
    struct local_entry* entries;
    uint32_t len;
};

struct local_entry {
    uint32_t name;         // Symbol (for printing)
    uint32_t type;         // Type term
    uint32_t binder_ref;   // Reference to binding lambda/pi/match
    bool has_value;        // For definitional unfolding
    uint32_t value;        // Term (if has_value)
};
```

### Meta-Variables (Type Inference)

```c
struct type_ctx {
    struct global_env* genv;
    struct term_arena* arena;
    struct sym_table* syms;

    struct type_meta_slot* metas;  // Meta-variable state
    uint32_t meta_count;
    uint32_t meta_next;            // Next meta ID
};

struct type_meta_slot {
    uint32_t id;
    uint32_t term;       // T_META term
    uint32_t solution;   // Solved value (0 if unsolved)
    bool solved;
    struct span loc;     // Error reporting
};
```

**Unification**: Solve meta-variables during type checking
**Constraint solving**: Deferred equality constraints

---

## What is Intentionally Absent

### 1. **No `Let` Binding**

**Desugaring**:
```
let x = v in body  ⟹  (λx. body) v
```

**Rationale**:
- Theoretically redundant (ζ-reduction ⊆ β-reduction)
- Simplifies equality checking (only βδι, no ζ)
- Sharing handled by environment/arena, not syntax

### 2. **No Σ (Dependent Pairs) as Primitive**

Can be encoded as inductive type:
```scheme
Sigma := \A : @ => \B : (A -> @) => @{
  pair : (x : A) -> B x -> *;
};
```

**Future**: May add as primitive for efficiency if heavily used.

### 3. **No Pattern Matching Sugar**

**Not allowed in Core IR**:
```haskell
pred Zero = Zero        -- Multiple equations
pred (Succ k) = k       -- Pattern in function head
```

**Must write**:
```scheme
pred : Nat -> Nat := \n : Nat =>
  n @zero => Nat.zero
    @succ k => k;
```

**Rationale**: Eliminates elaboration complexity, keeps Core minimal.

### 4. **No Implicit Arguments**

All arguments are **explicit** in Core IR:
```scheme
id : (A : @) -> A -> A := \A : @ => \x : A => x;

// Call site: MUST pass A explicitly
id Nat Nat.zero
```

**Future**: High-level language can infer and insert implicit arguments, then emit explicit Core IR.

### 5. **No Type Classes / Coercions**

These are high-level features that compile to explicit dictionary passing in Core IR.

### 6. **No Module System**

Global environment is flat. Namespacing handled by dot notation (`Nat.zero`).

**Future**: Module system in high-level language, compiles to unique names in Core.

---

## De Bruijn Indices

### Representation

Variables are represented by **indices** (distance to binder):

```
λx. λy. x y
  ↓
λ. λ. 1 0
```

- `0` = innermost binder (y)
- `1` = next outer binder (x)

### Binder References

In addition to indices, we track `binder_ref` for better error messages:

```c
struct {
    uint32_t binder_ref;  // Term ID of binding lambda/pi/match
    uint32_t name;        // Symbol ID (for printing)
} var;
```

**Conversion**: `binder_ref` → De Bruijn index via binder stack during evaluation.

---

## Spine Form Applications

### Structure

Instead of nested binary application:
```
((f x) y) z   // Traditional
```

Use **spine form**:
```c
struct {
    uint32_t fn;      // f
    uint32_t* args;   // [x, y, z]
    uint32_t argc;    // 3
} app;
```

**Benefits**:
- Efficient WHNF (single step to head)
- Fast equality testing
- Natural for partial application

---

## Summary

### Core Language Characteristics

| Property | Value |
|----------|-------|
| **Size** | ~20 term constructors |
| **Binding** | De Bruijn indices |
| **Application** | Spine form |
| **Sharing** | Arena + hash-consing |
| **Equality** | WHNF + pointer equality |
| **Universes** | Predicative hierarchy |
| **Inductives** | Indexed families supported |
| **Pattern match** | Indexed branches, explicit motive |
| **Proofs** | Erasable via relevance |

### Design Priorities

1. ✅ **Minimality**: Small trusted kernel
2. ✅ **Explicitness**: No hidden elaboration
3. ✅ **Canonicality**: De Bruijn, spine form
4. ✅ **Efficiency**: Arena allocation, sharing
5. ✅ **Correctness**: Verifiable implementation

---

## Implementation Status

### Completed (program-old)
- ✅ Full Core term structure
- ✅ Arena allocation with hash-consing
- ✅ Parser (`.p` → Core terms)
- ✅ Type checker (infer + check)
- ✅ Evaluator (WHNF with βδι)
- ✅ Indexed inductive types
- ✅ Dependent pattern matching
- ✅ Identity types (Eq, refl, eq_elim)
- ✅ Induction hypothesis (`*k`)

### In Progress (current migration)
- 🔄 Migrating Core structures to new codebase
- 🔄 Simplifying parser (removing AST layer)
- 🔄 Porting type checker
- 🔄 Porting evaluator

### Planned
- ⏳ Universe constraint solving
- ⏳ Positivity checking
- ⏳ Termination checking
- ⏳ NbE for full normalization
- ⏳ Proof erasure pass
- ⏳ Code generation backend

---

## References

### Internal Documents
- `DESIGN-PHILOSOPHY.md` — Overall design philosophy
- `CODING_STYLE.md` — Implementation conventions
- `program-old/CORE-README.md` — Original specification

### External References
- **Coquand & Huet (1988)**: "The Calculus of Constructions" (foundational theory)
- **de Bruijn (1972)**: "Lambda calculus notation with nameless dummies" (indices)
- **Norell (2007)**: "Towards a practical programming language based on dependent type theory" (Agda Core)
- **Brady (2013)**: "Idris, a general-purpose dependently typed programming language" (practical DTT)
- **Univalent Foundations (2013)**: "Homotopy Type Theory" (identity types)

---

**Last updated**: 2025-10-09
**Authors**: Original design by program-old authors, updated for IR-first architecture
