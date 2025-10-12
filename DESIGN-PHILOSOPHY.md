# Design Philosophy

This document records the design principles for the dependent type system compiler/interpreter.
We maintain a **single dependent IR** that is minimal yet expressive enough for indexed
inductive types (HIT), type checking, and evaluation.

**Date**: 2025-10-09
**Status**: Active development (migrating from program-old)

---

## Core Principles

### 1. **IR-First Design Philosophy**

This system is **not** a high-level language compiler. Instead:

```
┌─────────────────────────────────────────────────┐
│  High-level Language (Future)                   │
│  - Pattern matching sugar                       │
│  - Do notation                                   │
│  - Type class desugaring                        │
│  - Implicit arguments inference                 │
└─────────────────┬───────────────────────────────┘
                  │ Compiler (to be built)
                  ▼
┌─────────────────────────────────────────────────┐
│  .p files (Core IR Language) ← THIS SYSTEM      │
│  - Explicit everything                          │
│  - No syntactic sugar                           │
│  - Direct parser → Core term translation        │
│  - Human-readable but assembly-like             │
└─────────────────┬───────────────────────────────┘
                  │ parser.y (trivial 1:1 mapping)
                  ▼
┌─────────────────────────────────────────────────┐
│  Core Terms (term_arena)                        │
│  - Type checking target                         │
│  - Evaluation target                            │
└─────────────────────────────────────────────────┘
```

**Key insight**: The `.p` file format is analogous to:
- **LLVM IR**: Typed assembly for LLVM
- **Lisp S-expressions**: Direct AST representation
- **WebAssembly text format**: Human-readable binary IR

### 2. **No AST Layer**

Unlike traditional compilers, this system **does not have an AST → Core elaboration layer**.

**Rationale**:
1. `.p` files are already "elaborated" — they contain explicit types, explicit constructors,
   explicit pattern matches
2. Parser performs only **syntactic analysis**, not semantic desugaring
3. All "elaboration" (if needed) happens in the future high-level language compiler
4. Core IR is the **only** intermediate representation

**Comparison**:
```
Traditional compiler:
  Source → AST → Desugaring → Core IR → Type checking → Codegen

This system:
  .p files → Parser → Core terms → Type checking → Evaluation
              ↑                        ↑
         (syntactic)            (semantic)
```

### 3. **Parser is a Pure Translator**

The parser's job is **text → data structure conversion**, nothing more:
- No name resolution (names are already resolved in `.p` files)
- No type inference (types are explicit)
- No desugaring (no sugar exists)
- No implicit argument insertion (everything is explicit)

**Parser complexity**: O(syntax nodes) — linear and predictable

---

## IR Syntax Design Principles

### Principle 1: **The `*` Self-Reference Marker**

#### Problem: Preventing Invalid Inductive Definitions

Without `*`, users could write:
```scheme
;; INVALID: Constructor returns wrong type
Nat := @{
  zero : Nat;           // OK
  succ : Nat -> Nat;    // OK
  evil : List Nat;      // BUG! Constructor returning List Nat instead of Nat
};
```

This breaks:
- **Positivity checking** (needed for termination)
- **Induction principles** (match would be unsound)
- **Type system soundness** (logical consistency)

#### Solution: Restrict Constructor Return Types with `*`

```scheme
;; VALID: * = "this inductive type being defined"
Nat := @{
  zero : *;           // * = Nat
  succ : * -> *;      // * -> * = Nat -> Nat
};
```

**Enforcement**: Parser/elaborator only allows `*` in constructor return positions.

**Guarantee**: All constructors return **exactly** the type being defined.

#### Why Not Just Write `Nat`?

**Problem with explicit type names**:
```scheme
;; Parametric types
List := \A : @ => @{
  nil  : List A;         // Which A? Requires name resolution
  cons : A -> List A -> List A;  // Type gets verbose
};

;; With *
List := \A : @ => @{
  nil  : *;              // * = List A (implicitly)
  cons : A -> * -> *;    // Clean and unambiguous
};
```

**Benefits of `*`**:
1. **No name repetition**: DRY principle
2. **Scope-independent**: No need to resolve the inductive's name
3. **Parametric-friendly**: Works naturally with type parameters
4. **Positivity enforcement**: Structural constraint, not semantic check

---

### Principle 2: **Indexed Inductive Types Need Explicit Indices**

For **indexed families** (Level 3 types), `*` alone is insufficient:

```scheme
;; Length-indexed vectors
Vec := \A : @ => \n : Nat => @{
  nil  : Vec A Nat.zero;              // Index = zero (explicit)
  cons : A -> Vec A n -> Vec A (Nat.succ n);  // Index = succ n (explicit)
};
```

**Why not use `*` here?**

`*` would mean "some Vec A k" but we need **index refinement**:
- `nil` returns `Vec A zero` specifically
- `cons` receives `Vec A n` and returns `Vec A (succ n)` specifically

**Type checker validates**:
1. Return type has form `Vec A <index-expr>`
2. `<index-expr>` is well-formed given parameters
3. Indices satisfy positivity

**Tradeoff**:
- Simple inductives: Use `*` for brevity
- Indexed families: Use explicit types for precision

---

### Principle 3: **Top-level Type Annotations are Mandatory**

```scheme
;; REQUIRED
add : Nat -> Nat -> Nat := \n : Nat => \m : Nat => ...

;; NOT ALLOWED (in IR)
add := \n => \m => ...  // Which type? Ambiguous!
```

**Rationale**:
1. **Type inference halting**: Dependent types make inference undecidable
2. **Match disambiguation**: Pattern match needs to know scrutinee type
3. **Contract enforcement**: Types document expected behavior
4. **Error messages**: Better diagnostics when types are known upfront

**High-level language** (future) can infer types, then emit explicit annotations in `.p` IR.

---

### Principle 4: **Dot Notation for Constructor Namespace**

```scheme
Nat.zero      // Constructor 'zero' of type 'Nat'
Nat.succ      // Constructor 'succ' of type 'Nat'
List.nil      // Constructor 'nil' of type 'List'
List.cons     // Constructor 'cons' of type 'List'
```

**Why not just `zero`, `succ`, etc.?**

**Problem**: Name collisions
```scheme
Bool := @{ true : *; false : * };
Nat  := @{ zero : *; succ : * -> * };
List := \A => @{ nil : *; cons : A -> * -> * };

// Without namespacing:
nil    // Bool.nil? List.nil? Ambiguous!
zero   // Nat.zero? Other zero? Unclear!

// With namespacing:
List.nil   // Unambiguous
Nat.zero   // Clear
```

**Benefits**:
1. **No global name pollution**: Each inductive has its own namespace
2. **Explicit type information**: Reader knows which type the constructor belongs to
3. **Machine-friendly**: Name resolution is trivial (no search needed)
4. **IR clarity**: Assembly-like explicitness

**Comparison to languages**:
- **Haskell**: `True` (global), requires qualified imports for disambiguation
- **OCaml**: `Some` (global), module qualification optional
- **Rust**: `Option::Some` (always qualified) ← Similar to our approach

---

### Principle 5: **Pattern Matching is Explicit**

```scheme
;; Pattern match on Nat
pred : Nat -> Nat := \n : Nat =>
  n @zero => Nat.zero
    @succ k => k;
```

**Structure**:
- Scrutinee: `n`
- Branches: `@ctor => rhs`
- **No hidden control flow**: Each branch is explicit
- **Induction hypothesis**: `*k` for recursive calls (when needed)

**Not allowed** (high-level syntax):
```haskell
pred Zero = Zero      -- Pattern in function definition
pred (Succ k) = k     -- Multiple equations
```

These desugar to the explicit form in the high-level compiler.

---

## Core Term Language

### Minimal Term Constructors

Following CORE-README.md, the Core includes:

| Tag | Description | Notes |
|-----|-------------|-------|
| `T_SORT` | Universe `Type u` | Universe levels (0, succ, max, var) |
| `T_VAR` | De Bruijn indexed variable | Canonical representation |
| `T_PI` | Dependent function type `Π(x:A).B` | Telescope for efficiency |
| `T_LAM` | Lambda abstraction | Telescope + body |
| `T_APP` | Spine application | Head + arg array |
| `T_CONST` | Global constant | Delta-reduction controlled by opacity |
| `T_INDUCTIVE` | Inductive type reference | Metadata in global_env |
| `T_CTOR` | Constructor | Index into inductive's ctor list |
| `T_MATCH` | Pattern match eliminator | Explicit motive + branches |
| `T_META` | Meta-variable (hole) | For type inference |
| `T_ANN` | Type annotation `(term : type)` | Bidirectional checking |
| `T_EQ` | Propositional equality | HoTT support |
| `T_REFL` | Reflexivity proof | Identity type |
| `T_EQ_ELIM` | Equality eliminator | J-rule |

### Intentionally Absent

- **No `Let`**: Desugars to `(λx. body) value`
  - Rationale: β-reduction handles sharing via environment/arena
  - Keeps equality simple (βδι only)

- **No Σ (dependent pairs)**: Can be encoded as inductive type
  - If needed frequently, may add later as primitive

### Evaluation Strategy

- **WHNF (Weak Head Normal Form)**: β, δ, ι reductions
- **Closure-based**: Environments capture bindings
- **Spine-guided comparison**: Fast definitional equality
- **Future**: NbE (Normalization by Evaluation) for full normalization

---

## Global Environment

### Inductive Definitions
```c
struct inductive_def {
    uint32_t name;              // Symbol ID
    struct telescope params;     // Type parameters
    struct telescope indices;    // Indices (for indexed families)
    uint32_t sort;              // Universe term
    struct ctor_def* ctors;     // Constructor array
    uint32_t ctor_count;
};
```

**Metadata stored**:
- Parameters: Uniform across all constructors
- Indices: Refined per constructor
- Sort: Universe level
- Constructors: Full types after parameterization

### Constant Definitions
```c
struct const_def {
    uint32_t name;
    uint32_t type;      // Term ID
    uint32_t body;      // Term ID (0 if axiom)
    bool opaque;        // Controls δ-reduction
};
```

---

## Compilation Pipeline

### Current System (MVP)
```
.p file → Parser → Core terms → (Future: Type checker) → Evaluation
```

### Full System (Target)
```
.p file → Parser → Core terms → Type checker → Passes → Evaluation/Codegen
                                     ↓
                              Meta resolution
                              Constraint solving
```

**Passes** (all HLIR → HLIR transformations):
1. Proof erasure (remove `Prop`-level terms)
2. Monomorphization (specialize polymorphic definitions)
3. Match strategy selection (if-chain vs jump table)
4. Closure conversion
5. ADT lowering (tagged unions, etc.)

**Each pass**:
- Input: Core term
- Output: Core term + Eq proof (transformation preserves semantics)

---

## Design Rationale Summary

### Why This Design?

1. **Simplicity**: No AST layer, no desugaring, no complex elaboration
2. **Predictability**: Parser is O(n), type checking is predictable
3. **Debuggability**: IR is human-readable, easy to inspect
4. **Correctness**: Small trusted kernel, formal verification feasible
5. **Extensibility**: High-level languages compile to this IR
6. **Performance**: Direct Core representation, efficient evaluation

### What This Is **Not**

- ❌ Not a "user-facing" programming language
- ❌ Not trying to infer types automatically
- ❌ Not hiding complexity behind syntax sugar
- ❌ Not a research vehicle for type inference algorithms

### What This **Is**

- ✅ A typed assembly language for dependent types
- ✅ A compilation target for future high-level languages
- ✅ A minimal, verifiable kernel for type checking
- ✅ An interpreter for indexed inductive types
- ✅ A foundation for HoTT and proof-carrying optimization

---

## Influences and Comparisons

### Similar Systems

| System | Similarity | Difference |
|--------|-----------|------------|
| **LLVM IR** | Typed assembly, explicit everything | Not dependent types |
| **Cedille Core** | Minimal dependent core | Different term language |
| **Agda Core** | Dependent types, DeBruijn | Has implicit arguments |
| **Coq's Gallina** | Inductive types, Π | Higher-level (user-facing) |
| **Lean 4 IR** | Compilation target | Not human-readable |
| **WebAssembly** | Explicit, stack-based | No dependent types |

### Philosophy Alignment

Closest to:
- **LLVM's philosophy**: "Libraries, not frameworks; mechanisms, not policies"
- **Unix philosophy**: "Do one thing well" (type-check dependent terms)
- **Lisp tradition**: "Code as data" (terms are values)

---

## Future Directions

### Near-term (Current Migration)
1. Complete Core term structure (from program-old)
2. Implement type checker (infer + check)
3. Implement evaluator (WHNF)
4. Test with Level 0-3 examples

### Medium-term
1. Universe constraints
2. Positivity checking
3. Termination checking
4. Better error messages

### Long-term
1. NbE for definitional equality
2. Proof erasure pass
3. High-level language compiler (to .p IR)
4. Backend code generation

---

## Document History

- 2025-10-09: Created from program-old/DESIGN-PHILOSOPHY.md, added IR syntax rationale
- Incorporated insights from program-old/CORE-README.md and ELAB-README.md
- Added explicit design principles for `*`, dot notation, and type annotations

---

## Related Documents

- `CORE-README.md` — Core term language details (to be created)
- `SYNTAX-SPEC.md` — Formal grammar specification (to be created)
- `CODING_STYLE.md` — Implementation conventions
- `REFACTORING_REVIEW_V3.md` — Current codebase status
- `program-old/DESIGN-PHILOSOPHY.md` — Original design document
