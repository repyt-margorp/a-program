# Unused and Obsolete Code Semantics-Preserving Refactor Plan

Date: 2026-08-28

Status: implementation and equivalence validation complete

Baseline commit: `8cff173b81d63e1df2c67801f43dc7ae1de5c7c2`

## 1. Objective

This plan removes implementation that is no longer reachable, collapses empty
compatibility facades, narrows accidental public linkage, and removes test assets
that no test can execute.

The work is strictly semantics-preserving. It must not change:

- accepted surface syntax;
- elaboration or lowering results;
- Core Term identities or reduction behavior;
- classifier, effect, usage, totality, or equality decisions;
- Context, Substitution, Proposition, Claim, or Derivation meaning;
- artifact schema, serialized bytes, replay, linking, or diagnostics;
- runtime output, evaluation order, or backend behavior;
- test coverage represented by an active test.

This is not a compatibility-preservation project. The prototype has no stable C
ABI. A dead wrapper is removed rather than retained as an alias. Compatibility
wrappers would preserve the obsolete structure that this work is intended to
remove.

## 2. Audit Boundary

The audit covered:

- all C implementation and declarations below `src/prototype/src/` and
  `src/prototype/include/`;
- all `.inc` implementation fragments and their include sites;
- the reader and REPL build manifests in
  `src/prototype/build/sources.mk` and `src/prototype/Makefile`;
- integration scripts, checker tests, and source fixtures below
  `src/prototype/tests/`;
- current artifact schema and historical artifact schema files;
- prior structure and authority refactoring plans in `doc/`;
- global object symbols from the current reader/repl build;
- same-translation-unit calls after `.inc` files are assembled;
- source token references for enum members, macros, and public declarations.

The implementation baseline contains:

| Measure | Baseline |
| --- | ---: |
| C implementation files under `src/prototype/src/` | 51 |
| C files linked by reader/repl | 49 |
| Implementation C/header/include files | 173 |
| Implementation lines | 177,678 |
| Static functions | 1,873 |
| Globally defined functions in current objects | 778 |
| Integration shell scripts | 53 |
| Checker C/include test sources | 38 |
| Source fixtures inspected | 111 |

The two C files not linked by the ordinary reader/repl target are HOTT-specific
test components. They are intentionally used by HOTT tests and are not dead.
Every `.inc` file currently has an include site.

## 3. Classification Rules

Reference count alone is not a deletion rule. Every candidate is assigned one of
the following classes.

| Class | Meaning | Required action |
| --- | --- | --- |
| `CONFIRMED_DEAD` | No caller, no test, no schema role, and no documented public contract | Delete |
| `CASCADE_DEAD` | Reachable only from another confirmed-dead function | Delete in the same package |
| `INTERNAL_ONLY_EXPORT` | Used, but only inside its defining translation unit | Remove public declaration and make `static` |
| `DUPLICATE_TEST_ASSET` | No harness reaches it and an active test covers the same boundary | Delete after recording the successor test |
| `PUBLIC_CONTRACT_GAP` | Coherent public capability with no direct test | Retain and add a focused test |
| `RETAINED_CONTRACT` | Wire value, sentinel, reserved identifier, generated source, or explicit test API | Retain unchanged |
| `TEST_ONLY_COMPONENT` | Not linked by the product target but linked by a test target | Retain unchanged |

Before deletion, the implementation phase must repeat symbol extraction from a
clean build. A symbol that gained a caller after this baseline is reclassified;
it is not deleted mechanically.

## 4. Confirmed Findings

### U-001: Imported type-instance rewrite island is unreachable

File:

- `src/prototype/src/frontend/lowering/graph_construction.inc`

The block beginning with `imported_type_export_by_local_type` and ending before
`imported_constructor_classifier_from_curried_cache` is unreachable. Its main
entry, `rewrite_imported_type_instances_to_external`, is explicitly marked
`__attribute__((unused))`; its only calls are recursive calls from itself. Its two
setup helpers are called only by that entry.

The dead island is approximately 467 lines, currently around lines 2060-2526.
Current import lowering uses the canonical relocation/interface path instead.

Action:

- delete the complete helper island;
- do not move any part into artifact linking;
- do not retain the `unused` attribute as a suppression;
- verify imported generic types, constructor classifiers, and artifact append.

### U-002: Reader options are an empty compatibility facade

Files:

- `src/prototype/include/a_program/frontend/reader.h`
- `src/prototype/src/frontend/reader.c`
- `src/prototype/src/driver/repl.c`
- `src/prototype/src/driver/read_file.c`

`struct prototype_read_options` contains only
`forbid_standalone_expectations`. No parser or lowering code reads the field. The
drivers zero-initialize the structure and pass it through
`prototype_read_ast_*_with_options`, but the copied value has no effect.

Action:

- remove `prototype_read_options`;
- remove `prototype_read_ast_file_with_options` and
  `prototype_read_ast_string_with_options`;
- make both drivers call `prototype_read_ast_file` and
  `prototype_read_ast_string` directly;
- preserve parser diagnostics byte-for-byte for the same inputs;
- do not introduce a replacement flag or compatibility wrapper.

### U-003: The old Lambda expansion rule file is wholly unreachable

Files:

- `src/prototype/src/kernel/rules/introduction_lambda.inc`
- its include site in `src/prototype/src/kernel/judgement.c`
- the corresponding declarations in judgement rule headers

The 203-line file contains `prototype_judgement_expand_lambda` and
`prototype_judgement_delta_expand_lambda`. The Delta function is called only by
the dead wrapper, and the wrapper has no repository caller. Current Lambda
typing is performed through occurrence typing, candidate solving, and accepted
replay.

Action:

- remove the entire include fragment;
- remove its include directive and public declarations;
- do not redirect the old wrapper to a new implementation;
- verify Lambda synthesis, nested Lambda, dependent Pi, identity sharing, and
  artifact replay.

### U-004: Obsolete judgement entry points remain public

The following functions have a declaration and definition but no caller. They
are old direct mutation, pre-transaction, abandoned solver, or convenience
entry points. Their active Delta, Claim, candidate, occurrence, and replay
counterparts remain in place.

| Module | Remove |
| --- | --- |
| `kernel/rules/introduction/structural.inc` | `prototype_judgement_add_context_weakened_claim` |
| `kernel/typing/accepted_replay.inc` | `prototype_judgement_add_induction_hypothesis_claim` |
| `kernel/typing/accepted_replay.inc` | `prototype_judgement_add_is_type` |
| `kernel/typing/conversion.inc` | `prototype_judgement_classifier_conversion_with_definitions` |
| `kernel/rules/match/candidate_search.inc` | `prototype_judgement_delta_build_match_motive_from_branch_hints` |
| `kernel/rules/match/candidate_search.inc` | `prototype_judgement_delta_build_match_motive_from_known_branches` |
| `kernel/rules/match/motive_rule_emission.inc` | `prototype_judgement_delta_expand_match_motive` |
| `kernel/rules/cbpv.inc` | `prototype_judgement_delta_infer_computation_constraints` |
| `kernel/rules/formation_recording.inc` | `prototype_judgement_delta_record_int_literal_admissibility` |
| `kernel/typing/classifier_solver.inc` | `prototype_judgement_delta_resolve_induction_hypothesis_request` |
| `kernel/rules/cbpv.inc` | `prototype_judgement_delta_solve_computation_constraints` |
| `kernel/rules/elimination_app.inc` | `prototype_judgement_expand_app` |
| `kernel/rules/formation_host.inc` | `prototype_judgement_expand_int_literal` |
| `kernel/rules/match/expansion_rule_emission.inc` | `prototype_judgement_expand_match` |
| `kernel/rules/match/motive_rule_emission.inc` | `prototype_judgement_expand_match_motive` |
| `kernel/rules/formation_host.inc` | `prototype_judgement_expand_text_literal` |
| `kernel/rules/formation_recording.inc` | `prototype_judgement_record_declaration_fact` |
| `kernel/typing/classifier_solver.inc` | `prototype_judgement_resolve_match_case_request` |
| `kernel/typing/classifier_solver.inc` | `prototype_judgement_synthesize_match_pattern_classifier` |

`prototype_judgement_expand_lambda` is covered by U-003.

This package must not merge APP, Match, IH, or computation-fold proof rules.
Those rules may share plumbing but prove different propositions. This task only
removes unreachable entry points.

After each removal, compile with unused-function warnings promoted to errors.
Private helpers reached only by a removed entry are `CASCADE_DEAD` and are
removed in the same commit. Helpers shared by active rules remain.

Specific active functions that must remain include:

- `prototype_judgement_expand_primitives`;
- `prototype_judgement_delta_build_match_motive`;
- `prototype_judgement_delta_build_constant_match_motive`;
- `prototype_judgement_delta_build_match_motive_from_cases`;
- `prototype_judgement_delta_type_match_from_cases`;
- `prototype_judgement_delta_generate_computation_constraints`;
- the static `prototype_judgement_delta_expand_match_motive_with_premises`.

### U-005: Uncalled public convenience APIs have no contract

The following globally defined functions have only their declaration and
definition in the repository. Except for the two entries in U-007, they should
be removed because there is no stable prototype ABI and no active caller.

| Area | Remove |
| --- | --- |
| Artifact interface | `prototype_artifact_interface_renumber_universe_vars` |
| AST lowering | `prototype_ast_compile_pending` |
| AST | `prototype_ast_lookup_assignment_const` |
| AST | `prototype_ast_pair_type_expectation` |
| AST | `prototype_ast_type_expr_universe` |
| Checked module set | `prototype_checked_module_set_original_index_at` |
| Compile metadata | `prototype_compile_metadata_function_graph_origin_group` |
| Compile metadata | `prototype_compile_metadata_function_graph_request` |
| CwF certificate | `prototype_cwf_certificate_db_validate_substitution_roots` |
| Dimension operators | `prototype_dimension_operator_db_clear` |
| Runtime | `prototype_runtime_evaluate_core` |
| Core Term | `prototype_term_effect_row_forall_parts` |
| Core Term | `prototype_term_host_type_source_name` |
| Core Term | `prototype_term_host_type_term_tag` |
| Core Term diagnostics | `prototype_term_print` |
| Core Term linking | `prototype_term_source_shape_equal_for_link` |
| Core Term | `prototype_term_type_instance_source_make` |
| Type declarations | `prototype_type_declaration_intern_representation` |
| Typed occurrence graph | `prototype_typed_occurrence_graph_selected_classifier` |
| Universe | `prototype_universe_find_type_node` |
| Symbol support | `symbol_map_is_used_at` |

Notes:

- `prototype_type_declaration_intern_representation` no longer interns. It only
  returns an already finalized representation ID, so retaining that name would
  preserve a false ownership model.
- `prototype_term_source_shape_equal_for_link` is only an unused wrapper. The
  active core/view source-shape comparators remain.
- Removing `prototype_runtime_evaluate_core` does not remove runtime evaluation;
  the annotated evaluator is the active implementation.
- CwF root validation remains available through the active certificate and
  substitution validation paths. If the pre-edit API inventory discovers an
  explicit documented bulk-root contract, this one function is reclassified as
  `PUBLIC_CONTRACT_GAP` and tested rather than removed.

### U-006: Active internal functions have accidental external linkage

The following functions are used, but only inside the translation unit that
defines them. They should become `static`, and declarations should move out of
public headers where applicable.

| Area | Internalize |
| --- | --- |
| Checker module | `prototype_checker_calculus_fingerprint` |
| Checker module | `prototype_semantic_intrinsic_fingerprint` |
| Core Term | `prototype_term_primitive_int64` |
| Dimension face | `prototype_dimension_face_ordinal` |
| Dimension operator | `prototype_dimension_operator_db_rebuild_index` |
| Compiler driver | `prototype_link_external_refs` |
| Diagnostics | `prototype_compile_diagnostic_phase_name` |
| Diagnostics | `prototype_compile_diagnostic_reason_name` |
| Function graph | `prototype_accepted_definition_view_open` |
| Universe collection | `prototype_universe_close_program` |
| Universe collection | `prototype_universe_reconstruct_obligations` |
| Verification DB | `prototype_verification_db_discharge_effect_row_equation` |
| Verification DB | `prototype_verification_db_get_mutable` |
| Verification DB | `prototype_verification_db_remove_obligation_dependencies` |
| Verification DB | `prototype_verification_effect_row_equation_holds` |
| Context | `prototype_context_pullback_telescope` |
| Match candidate search | `prototype_judgement_delta_build_match_motive_from_cases` |
| Match motive rules | `prototype_judgement_delta_ensure_type_at_universe` |
| Accepted replay | `prototype_judgement_validate_occurrence_typing` |
| Classifier solver | `prototype_judgement_classifier_is_recursive_family_instance` |
| Classifier solver | `prototype_judgement_constructor_specialize` |
| Early formation | `prototype_judgement_solve_index_pattern` |
| Universe solver | `prototype_universe_solve` |

Two entries from the mechanical same-translation-unit list are deleted instead
of internalized:

- `prototype_judgement_delta_expand_lambda`, under U-003;
- `prototype_judgement_delta_infer_cbpv_boundaries`, if it becomes unreachable
  after removal of `prototype_judgement_delta_infer_computation_constraints`.

`prototype_judgement_delta_generate_computation_constraints` remains active and
becomes `static`.

`prototype_read_ast_file` must remain externally visible because U-002 changes
the drivers to call it directly. It is therefore not an internalization target.

The objective is ownership clarity, not merely a smaller symbol table. No
function should be moved to another module only to make it `static`.

### U-007: Two coherent public capabilities need tests, not deletion

These functions currently have no caller, but they complete an intentional
read-only API boundary:

- `prototype_checked_constructor_export`;
- `prototype_type_view_constructor_telescope_query`.

The checked module API already exposes term and type exports. Constructor export
access is the corresponding checked capability. The TypeView telescope query is
the narrow authority-preserving alternative to exposing mutable
`TypeDeclarationDB` internals.

Action:

- retain both functions;
- add direct checker tests that consume their returned views;
- verify out-of-range and malformed-view rejection;
- do not add production callers solely to increase reference count.

If either function cannot be given a meaningful behavior-level test without
constructing private state, stop and reclassify it in the review rather than
adding a tautological call.

### U-008: Five fixture files are unreachable and duplicated

All 53 integration scripts are discovered dynamically, and all 38 checker test
sources are referenced. The following five `.p` fixtures have no active harness
reference and are superseded by active tests:

| Unreachable fixture | Active successor |
| --- | --- |
| `tests/fixtures/cbpv/execution_demand_check.p` | `test_cbpv_boundary.sh` and inline `test_cbpv_surface.sh` case |
| `tests/fixtures/cbpv/runtime_strict_dependent_check.p` | inline strict-dependent case in `test_cbpv_surface.sh` |
| `tests/fixtures/typing/multi_app_occurrence_check.p` | inline artifact-flow and P0 occurrence tests |
| `tests/fixtures/typing/function_graph_result_package_probe.p` | `function_graph_certified_length_model.p` coverage |
| `tests/fixtures/typing/computation_indexed_family_check.p` | current indexed-function-graph foundation tests |

These files total 67 lines.

Action:

- record the exact successor assertion before deletion;
- run the successor test independently;
- delete only the unattached fixture;
- do not reduce the assertions in the successor test.

### U-009: No orphan source or include fragment exists

All ordinary product C sources appear in the build manifest. The two additional
HOTT sources are linked by HOTT-specific tests. Every `.inc` file has an include
site. Therefore this audit does not propose broad source-tree deletion based on
the reader/repl target alone.

### U-010: Wire values, sentinels, and test controls are not dead code

Low token counts identified enum members and macros that are intentionally not
ordinary call sites. They must remain unchanged, including:

- computation-kind values represented in the current artifact schema;
- work-capsule producer kinds used as persistent scheduler identifiers;
- Term field-role descriptors used by schema/table iteration;
- compile-time Term interning controls used by tests;
- archived artifact schema files documenting prior wire versions.

No enum number may be removed or renumbered in this refactor. Historical schemas
are documentation and compatibility evidence, not obsolete parser code.

## 5. Implementation Packages

### UR0: Freeze the semantic and API baseline

- [x] Record the exact Git commit and dirty-worktree state.
- [x] Build reader and REPL with the current warning policy.
- [x] Record exported object symbols.
- [x] Record current artifact schema version and artifact fixture hashes.
- [x] Record accepted Claim and Derivation counts for representative programs.
- [x] Record CLI stdout, stderr, and exit status for positive and negative cases.
- [x] Record current full-suite pass/fail status without repairing unrelated
      failures.
- [x] Create a short supported prototype C API inventory from actual drivers,
      tests, and documented checker capabilities.

Exit criterion: every removal candidate is either outside the supported API or
explicitly reclassified before editing.

### UR1: Remove the imported rewrite island

- [x] Delete the three-entry dead island and all private cascade-only helpers.
- [x] Remove the `unused` suppression.
- [x] Build with unused warnings as errors.
- [x] Run imported generic type and constructor classifier tests.
- [x] Run artifact append, relocation, link, and replay tests.
- [x] Compare artifact bytes and diagnostics with UR0.

Exit criterion: no behavior or serialized output changes.

### UR2: Collapse the reader options facade

- [x] Remove the option structure and `_with_options` declarations.
- [x] Simplify reader implementation entry points.
- [x] Update REPL and file driver call sites.
- [x] Remove resulting dead fields and copies.
- [x] Run parser, standalone expectation, REPL, and negative diagnostic tests.
- [x] Compare exact parser diagnostics with UR0.

Exit criterion: one reader API per input kind and identical parse behavior.

### UR3: Remove obsolete judgement entry points

- [x] Delete `introduction_lambda.inc` and its include site.
- [x] Remove U-004 declarations and definitions by rule family.
- [x] Remove cascade-only private helpers revealed by `-Werror`.
- [x] Internalize active same-unit rule helpers from U-006.
- [x] Verify that active Delta, candidate, Claim, occurrence, and replay routes
      remain the only authorities.
- [x] Run focused APP, Lambda, Match, IH, CBPV, and accepted replay tests after
      each rule-family edit.

Exit criterion: proof rules and accepted propositions are unchanged; only dead
entry surfaces disappear.

### UR4: Remove orphan convenience APIs

- [x] Process U-005 one module at a time.
- [x] Remove public declarations together with definitions.
- [x] Remove cascade-only helpers only after a warning-clean build identifies
      them.
- [x] Do not replace removed functions with aliases.
- [x] Run the owning module's focused tests after each module.

Exit criterion: no declaration-only API remains without a caller or documented
contract.

### UR5: Narrow accidental external linkage

- [x] Mark every retained U-006 function `static`.
- [x] Remove it from public headers.
- [x] Keep local forward declarations only where source order requires them.
- [x] Re-run object-symbol extraction.
- [x] Confirm no public test or driver depended on the symbol.

Exit criterion: externally visible functions correspond to real cross-module or
test contracts.

### UR6: Remove duplicate fixtures and test retained capabilities

- [x] Add direct tests for the two U-007 APIs.
- [x] Record successor assertions for every U-008 fixture.
- [x] Run each successor independently.
- [x] Delete the five unreachable fixture files.
- [x] Run the complete fixture-reference audit again.

Exit criterion: no active assertion is lost and retained public capabilities are
behavior-tested.

### UR7: Add permanent dead-boundary checks

Add a prototype-only audit script. It should fail on:

- implementation functions suppressed with `__attribute__((unused))`;
- reappearance of specifically retired API names;
- `.inc` files without include sites;
- implementation `.c` files absent from all relevant source manifests;
- deleted fixtures reintroduced without a harness reference;
- public declarations that have neither a cross-translation-unit caller nor an
  explicit test/API allowlist entry.

The allowlist must contain a reason and owner category, such as wire contract,
test entry, checker capability, or reserved schema value. It must not become a
generic suppression list.

The script must understand dynamically discovered integration tests. A basename
grep is insufficient and produced false positives during this audit.

### UR8: Final equivalence and accounting

- [x] Run clean warning-as-error builds for reader and REPL.
- [x] Run all focused suites listed below.
- [x] Run the full integration/checker suite and compare with UR0.
- [x] Compare accepted propositions, claims, derivations, and residual
      obligations for representative programs.
- [x] Compare artifact bytes where deterministic serialization is expected.
- [x] Compare runtime output and normalization output.
- [x] Compare compile-time and peak-memory measurements; neither may regress
      materially from deletion-only work.
- [x] Record per-file added, deleted, and net lines.
- [x] Record total added, deleted, and net implementation and test lines
      separately from documentation.
- [x] Update this checklist and add the implementation commit ID.

## 6. Verification Matrix

At minimum, run:

- ordinary positive examples through the current supported example boundary;
- identity sharing across distinct annotations;
- dependent Pi and indexed family tests;
- nested Lambda and Match motive tests;
- `test_cbpv_boundary.sh`;
- `test_cbpv_surface.sh`;
- artifact flow, append, relocation, link, and checked replay tests;
- checked core session/container tests;
- constraint authority tests;
- Context/Substitution immutability and CwF certificate tests;
- dimension action tests;
- HOTT/identity goal tests;
- certified function-graph execution tests;
- Issue 23 regression tests;
- the complete integration suite.

For negative fixtures, compare both exit status and diagnostic text. For
artifacts, compare schema version, root counts, relocation tables, and bytes when
the serializer is deterministic. For solver-facing tests, compare accepted
Claim/Derivation counts and residual-obligation categories, not only process
success.

A pre-existing baseline failure is not repaired in this refactor. It must remain
the same failure unless separately authorized.

## 7. Stop Conditions

Stop the package and investigate if any of the following occurs:

- a Core Term ID or canonical shape changes for the same source;
- a classifier, effect row, usage result, or totality result changes;
- accepted Claim or Derivation structure changes;
- a new residual obligation appears or an old one disappears;
- artifact bytes, schema values, or replay decisions change;
- parser or checker diagnostics change;
- runtime output or evaluation order changes;
- a supposedly dead function is reached by a supported test;
- deletion requires a compatibility adapter to keep the build working.

The last condition indicates that the candidate was not actually isolated and
must be re-audited rather than hidden behind another wrapper.

## 8. Explicit Non-Goals

This plan does not:

- merge distinct APP, Match, IH, or computation-fold proof rules;
- redesign TermDB, ContextDB, SubstitutionDB, JudgementDB, or TypeDeclarationDB;
- change authority ownership between constraints and solutions;
- alter CBPV polarity, computation requests, handlers, or effect rows;
- alter HOTT relation/identity semantics or add missing higher coherence;
- alter indexed-family, Acc, totality, or function-graph semantics;
- renumber enum tags or artifact fields;
- remove historical schemas;
- promote prototype code into accepted `src/` or `include/` code;
- edit generated parser or lexer files;
- combine this cleanup with performance optimization.

## 9. Expected Scale

The currently measured lower bound of directly removable content is 737 lines:

- approximately 467 lines in the imported rewrite island;
- 203 lines in `introduction_lambda.inc`;
- 67 lines in unreachable fixtures.

Reader facade removal, obsolete judgement bodies, orphan convenience APIs, and
cascade-only helpers will increase the deletion count. No target line count is
set: a lower count is acceptable when preserving a real contract requires code
to remain.

The final report must provide a table with:

| File | Added | Deleted | Net | Reason |
| --- | ---: | ---: | ---: | --- |

Documentation line changes must be reported separately so they do not obscure
implementation reduction.

## 10. Review Decisions

The audit rejects the following tempting but invalid deletions:

- HOTT-only source files omitted from the ordinary reader/repl build;
- checker and artifact producer APIs used directly by test executables;
- enum members with artifact or scheduler identity;
- sentinels and table descriptors referenced structurally rather than by name;
- compile-time test-control macros;
- archived artifact schemas;
- active rule-specific eliminators merely because they have similar plumbing.

The audit also rejects adding synthetic production calls solely to make an
uncalled API appear used. A coherent public capability receives a behavior test;
an incoherent or obsolete capability is deleted.

## 11. Progress Summary

- [x] Repository/build-manifest audit complete.
- [x] Global and static symbol audit complete.
- [x] Include-fragment reachability audit complete.
- [x] Fixture reachability audit complete.
- [x] Artifact/wire false-positive review complete.
- [x] Prior refactoring-plan overlap review complete.
- [x] UR0 baseline frozen.
- [x] UR1 imported rewrite island removed.
- [x] UR2 reader facade collapsed.
- [x] UR3 obsolete judgement paths removed.
- [x] UR4 orphan APIs removed.
- [x] UR5 external linkage narrowed.
- [x] UR6 fixture cleanup and retained API tests complete.
- [x] UR7 permanent audit check installed.
- [x] UR8 full equivalence and line accounting complete.

Implementation proceeded package by package with a warning-clean build and the
owning focused tests after each package. The final implementation is recorded as
one deletion-focused commit because no package changed observable semantics.

## 12. Implementation Evidence

Implementation commit: `384c7af938ebd51d288b9487a594fcd261835048`

### 12.1 Semantic equivalence

- Baseline and current `examples/09_list_induction.p` artifacts are byte-identical.
- Both artifacts have SHA-256
  `9f3a1cacdb02556d504824389a1ded29480b4216986e24586bdcdf4fc86f54d2`.
- The current canonical artifact schema remains `artifact_v86.schema`.
- Byte identity covers the serialized Term, proposition, Claim, Derivation,
  relocation, root, and residual-obligation data for this representative input.
- Focused APP, Lambda, Match, IH, CBPV, indexed-family, artifact, Context,
  Substitution, dimension, HOTT, and function-graph tests pass.
- Parser, checker, runtime, WHNF/NF, and negative diagnostic assertions in the
  active suite remain unchanged.

The full-suite comparison is:

| Revision | Tests | Passed | Failed | Wall time |
| --- | ---: | ---: | ---: | ---: |
| Baseline `8cff173b` | 53 | 49 | 4 | 44,673 ms |
| Refactored tree | 54 | 52 | 2 | 38,235 ms |

The two remaining failures are unchanged baseline failures:

- `test_checked_core_examples`: the existing explicit Acc indexed-family check;
- `test_if8_fuel_free_quicksort`: the existing QuickSort classifier failure.

The baseline-only failures in `test_artifact_flow` and
`test_conversion_result` were static audits for APIs intentionally removed by
this refactor. Their behavior tests pass after the obsolete APIs disappear. The
additional 54th test is `test_dead_code_boundaries`.

A 50-run compile sample for `examples/09_list_induction.p` measured 13.639 ms
mean wall time and 39,380 KiB peak RSS at baseline, versus 13.454 ms and 39,416
KiB after the refactor. The RSS difference is 36 KiB and is not material.

### 12.2 Structural result

- Implementation files changed from 177,678 to 174,933 lines.
- Globally defined object functions changed from 778 to 709.
- No implementation declaration uses `__attribute__((unused))`.
- Every implementation `.c` remains in a reader, REPL, or HOTT source manifest.
- Every `.inc` fragment has an include site.
- The retained checked-constructor export and TypeView telescope-query APIs now
  have direct success and rejection tests.

Line accounting is separated by ownership boundary:

| Boundary | Files | Added | Deleted | Net |
| --- | ---: | ---: | ---: | ---: |
| Public prototype headers | 24 | 0 | 426 | -426 |
| Prototype implementation | 43 | 43 | 2,362 | -2,319 |
| Existing tests and fixtures | 6 | 56 | 67 | -11 |
| New permanent audit test | 1 | 156 | 0 | +156 |
| This documentation | 1 | recorded by its documentation commit | 0 | documentation only |

The implementation total is 43 additions, 2,788 deletions, net -2,745 lines.
Tests total 212 additions, 67 deletions, net +145 lines. Exact per-file counts
are retained by the implementation commit and can be reproduced with:

```sh
git show --numstat --format= 384c7af938ebd51d288b9487a594fcd261835048
```

Largest individual deletions are:

| File | Added | Deleted | Net | Reason |
| --- | ---: | ---: | ---: | --- |
| `frontend/lowering/graph_construction.inc` | 0 | 467 | -467 | unreachable imported rewrite island |
| `core/term/evaluation_and_conversion.inc` | 0 | 289 | -289 | orphan public APIs and cascade-only printer helpers |
| `kernel/typing/classifier_solver.inc` | 2 | 271 | -269 | obsolete direct solver entry points and local linkage |
| `kernel/rules/match/candidate_search.inc` | 1 | 253 | -252 | obsolete motive builders and cascade-only selectors |
| `kernel/rules/introduction_lambda.inc` | 0 | 203 | -203 | wholly unreachable old Lambda expansion route |
| `kernel/judgement/rules.h` | 0 | 163 | -163 | declarations for retired judgement routes |
| `kernel/rules/match/expansion_rule_emission.inc` | 0 | 117 | -117 | obsolete Match wrapper route |
| `kernel/rules/cbpv.inc` | 1 | 95 | -94 | obsolete computation inference/solve wrappers |

### 12.3 Permanent checks

`test_dead_code_boundaries.sh` now rejects:

- reintroduction of retired implementation symbols or duplicate fixtures;
- unused-attribute suppression in implementation code;
- unmanifested implementation C files;
- orphan `.inc` fragments;
- externally linked prototype functions without a cross-module or direct-test
  contract.

The allowlist is intentionally empty. A future entry must state a concrete wire,
checker, test, or schema contract rather than merely suppressing the audit.
