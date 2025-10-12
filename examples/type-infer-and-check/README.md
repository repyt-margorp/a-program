# Type Inference and Checking Test Suite

This directory contains test cases for the type system, organized by the type hierarchy levels documented in `2025-10-05-TYPE-INFER-AND-CHECK.md`.

## Directory Structure

```
type-infer-and-check/
├── level0/          # Primitive types
├── level1/          # Finite & non-recursive data types
├── level2/          # Simple inductive types (W-type equivalent)
└── level3/          # Indexed inductive families (future)
```

## Test Status

### Level 0: Primitive Types

| File | Description | Status |
|------|-------------|--------|
| `00_universe.p` | Universe type `@` | ✅ Should pass |
| `01_function.p` | Simple function types | ✅ Should pass |
| `02_dependent.p` | Dependent function types (Π-types) | ✅ Should pass |

### Level 1: Finite & Non-recursive Data Types

| File | Description | Status |
|------|-------------|--------|
| `00_bool.p` | Boolean type and operations | ✅ Should pass |
| `01_unit.p` | Unit type | ✅ Should pass |
| `02_sum.p` | Sum type (Either) | ✅ Should pass |
| `03_product.p` | Product type (Pair) | ✅ Should pass |
| `04_maybe.p` | Maybe/Option type | ✅ Should pass |

### Level 2: Simple Inductive Types

| File | Description | Status |
|------|-------------|--------|
| `00_nat.p` | Natural numbers with addition | ⚠️ IH implementation in progress |
| `01_list.p` | Lists with length, append, map | ⚠️ IH implementation in progress |
| `02_tree.p` | Binary trees with size, height | ⚠️ IH implementation in progress |
| `03_rose.p` | Rose trees (nested recursion) | ⚠️ IH implementation in progress |

**Current Issue**: Induction hypothesis (`*k` notation) evaluation needs fixing.
See ongoing work on T_IH_CALL implementation.

### Level 3: Indexed Inductive Families

| File | Description | Status |
|------|-------------|--------|
| `00_vec.p` | Length-indexed vectors | ❌ Type checker not implemented |
| `01_fin.p` | Finite sets | ❌ Type checker not implemented |
| `02_eq.p` | Propositional equality | ❌ Type checker not implemented |
| `03_matrix.p` | 2D indexed matrices | ❌ Type checker not implemented |

**Note**: These files demonstrate that the **syntax already supports** indexed inductive families through lambda abstraction. However, the type checker doesn't yet handle:
- Index unification
- Dependent pattern matching
- Index refinement in match branches
- Motive inference for indexed types

## Running Tests

### Run individual test:
```bash
./bin/program examples/type-infer-and-check/level1/00_bool.p
```

### Run all tests in a level:
```bash
for f in examples/type-infer-and-check/level1/*.p; do
  echo "Testing $f"
  ./bin/program "$f"
done
```

### Expected Output

**Passing tests** should output:
```
parse ok: N decls
elab ok: consts=X inds=Y
passes ok
eval demo ok: ...
main => <result>
```

**Failing tests** (Level 3) will show type errors during elaboration.

## What Each Level Tests

### Level 0: Foundation
- Universe hierarchy
- Function type formation
- Dependent types (Π-types)

### Level 1: Basic Data Structures
- Non-recursive algebraic data types
- Pattern matching without recursion
- Simple case analysis

### Level 2: Recursion and Induction
- Recursive data structures
- Induction hypothesis generation
- `*k` notation for recursive calls
- Structural recursion

### Level 3: Advanced Type Features
- Index-dependent constructors
- Dependent pattern matching
- Type-level computation
- Index refinement

## Development Workflow

1. **Add new test**: Create `.p` file in appropriate level directory
2. **Update this README**: Add entry to the table
3. **Test manually**: `./bin/program examples/type-infer-and-check/levelN/XX_name.p`
4. **Document behavior**: Note whether it passes, fails, or is expected to fail

## Future Work

### Phase 1: Complete Level 2 Support
- [ ] Fix T_IH_CALL evaluation
- [ ] Ensure all Level 2 tests pass
- [ ] Add more complex inductive types

### Phase 2: Implement Level 3 Support
- [ ] Index unification algorithm
- [ ] Dependent pattern matching
- [ ] Index refinement in branches
- [ ] Motive inference

### Phase 3: Advanced Features
- [ ] Universe polymorphism
- [ ] Proof irrelevance
- [ ] Better error messages for indexed types

## References

See `2025-10-05-TYPE-INFER-AND-CHECK.md` for detailed documentation of the type system design and capabilities.
