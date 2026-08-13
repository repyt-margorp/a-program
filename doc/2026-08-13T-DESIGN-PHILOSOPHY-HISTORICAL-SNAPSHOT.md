# Design Philosophy: Historical Snapshot and Current Amendments

**Snapshot copied to `doc/`: 2026-08-13**

This file preserves the original 2025 design philosophy and records the
normative amendments established by the implementation work through artifact
v73 and the explicit index-family migration. The original sections below are
historical evidence. They are not authoritative where they conflict with the
current amendments near the end of this document.

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

The first form is historical and is rejected by the current reader. The
declaration name is deliberately absent from its own constructor-schema scope;
recursive self-reference is represented only by `*`.

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

## Current Normative Amendments (2026-08-13)

### 1. The source language has an AST and a semantic lowering pipeline

The old claim that `.p` source maps directly and trivially to one Core term is
obsolete. The current compiler deliberately distinguishes:

1. source AST and source binding occurrences;
2. OperationGraph, which retains typed source operations and sequencing;
3. TermDB, which hash-conses the shared computational graph;
4. ContextDB and SubstitutionDB, which represent scopes and reindexing;
5. classifier constraints and their fixed-point solutions;
6. Judgement propositions, Claims, and replayable Derivation DAGs;
7. artifact publication, relocation, linking, and accepted replay.

These layers are not duplicate evaluators. OperationGraph preserves the source
operation identity needed for typing and diagnostics; TermDB preserves shared
calculation independent of source names. A shared Core node may therefore be
used by distinct typed operations without collapsing their classifiers.

### 2. Classifier synthesis is bottom-up and source-directed

The surface language is intentionally restricted so that every accepted term
construct supplies enough local structure to generate its classifier
constraints. Constructors synthesize from their declaration telescope, APP
synthesizes from its function classifier and argument, Lambda synthesizes from
its explicit domain and body, and Match synthesizes a motive from its
scrutinee, constructor schemas, branch classifiers, and structural IH edges.

The solver may combine equations generated by those constructs. It must not
invent missing typing information from a later assertion. In particular, an
indexed Match must solve equations such as:

```text
M zero                 = branch_zero_classifier
M (succ k)             = branch_succ_classifier
classifier(*rest)      = M k
constructor_domain_3   = Vec B k
```

as one scoped constraint problem. Selecting the first known branch as a
constant motive before the other equations are available is an implementation
bug, not a reason to consult an annotation.

### 3. `::` is permanently a post-synthesis type expectation

This is a compiler invariant:

```text
synthesize(operation) = actual_classifier
check(actual_classifier, expected_classifier_from_::)
```

The following reverse flow is forbidden:

```text
expected_classifier_from_:: -> synthesize(operation)
```

Consequences:

- adding, removing, or changing `name :: T` must not change the principal
  classifier synthesized for `name`;
- `::` must not seed a Match motive, an IH classifier, an APP argument
  classifier, a constructor field classifier, or an effect row;
- `::` may request conversion, compatibility, or a structured checking proof
  only after synthesis has produced the body classifier;
- failure to synthesize without `::` is a synthesis bug or an explicit
  unsupported boundary; accepting the same term only when `::` is present is
  not a valid repair;
- artifact evidence for an assertion must refer to the independently
  synthesized Operation evidence and a replayable checking/conversion step.

Constructor domains and explicitly written Lambda binder domains are not
`::` expectations. They are rule-owned inputs to local synthesis. For example,
the third application in `Vec.cons k value rest` may propagate its telescope
domain `Vec A k` to the classifier constraint for `rest`; this remains
synthetic typing because the information comes from the constructor itself.

### 4. Constraint generation and solving are distinct

Lowering discovers graph structure and emits scoped equations. The solver
combines those equations to a fixed point under a configured effort budget.
TermDB nodes created during normalization or solving are acceptable compiler
workspace data, but only named and proof-reachable roots belong in a published
artifact.

Solver-local approximations are not kernel truths. In particular:

- a first branch does not establish that a Match motive is constant;
- an unresolved effect row is not an empty row;
- an unresolved index equation is not an impossible branch;
- a structural Core shape match is not nominal TypeView equality;
- a successful expectation check is not an independently synthesized
  classifier.

### 5. Indexed families separate parameters from indices

The current source form is:

```text
Vec :=
  \A : @ =>
  @\n : Nat =>
  {
    nil  : * Nat.zero;
    cons : (k : Nat) -> A -> * k -> * (Nat.succ k);
  };
```

Uniform parameters select constructor ownership and specialize constructor
telescopes. Indices occur in the full family and exact constructor results.
Recursive-field recognition therefore requires the same nominal family and
the same parameter spine; indices are allowed to change structurally.
The family name itself is not in scope in the constructor block. Uniform
parameters are captured by `*`; only the explicit index spine follows it.

Branch refinement is local substitution evidence. It must neither union Term
IDs nor extend global definitional equality. Solved, impossible, and residual
index equations remain distinct outcomes.

### 6. CBPV structure and effects do not replace pure reduction

Value and computation polarity are represented by classifier structure and by
the `RETURN`, `THUNK`, `FORCE`, request, and computation-fold boundaries. The
shared graph constructors do not justify collapsing values and computations,
nor does algebraic-effect handling turn beta/iota reduction into an effect
operation. Pure reduction remains deterministic kernel computation under an
explicit normalization profile; requests and folds represent effect syntax
and handling.

### 7. Equality remains separate from definitional conversion

Definitional equality is the result of the selected pure conversion procedure.
Object Identity is represented by ordinary terms, classifiers, Claims, and
Derivations. An object Identity proof must not be inserted into a global
conversion union-find. The current implementation contains a closed
homogeneous one-dimensional fragment and partial higher structure; general
dependent, Universe, coherent higher Identity, transport, and surface equality
remain incomplete.

### 8. Artifacts contain evidence, not search history

Artifacts publish graph slices, named views, accepted Claims, and replayable
Derivation premises. They do not publish work queues, temporary motive
metavariables, solver fuel history, or unproved approximations. A bounded
compiler may close more obligations when configured with more steps, but every
published result must state and replay the evidence actually obtained.

### 9. Current implementation direction

The migration sequence reflected by the dated plans is:

1. separate shared Core calculation from typed Operation occurrences;
2. make Context/Substitution and proof binding identities explicit;
3. split constraint generation, solving, proof publication, and replay;
4. introduce CBPV boundaries and algebraic-effect requests/folds;
5. separate nominal TypeView identity from structural representation;
6. represent object Identity without equality reflection;
7. add explicit indexed-family binders and exact constructor results;
8. generalize Match refinement, motives, and IH before admitting `Acc`;
9. admit strictly-positive Pi-codomain recursion and define well-founded
   recursion as library code rather than a privileged kernel primitive.

Any future implementation plan that violates the post-synthesis role of `::`,
uses readback metadata as semantic authority, or turns solver approximations
into accepted Claims must be revised before implementation.

---

## Related Documents

- `CORE-README.md` — Core term language details (to be created)
- `SYNTAX-SPEC.md` — Formal grammar specification (to be created)
- `CODING_STYLE.md` — Implementation conventions
- `REFACTORING_REVIEW_V3.md` — Current codebase status
- `program-old/DESIGN-PHILOSOPHY.md` — Original design document
