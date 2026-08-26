#ifndef A_PROGRAM_PROTOTYPE_CHECKER_MODULE_H
#define A_PROGRAM_PROTOTYPE_CHECKER_MODULE_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/core/term.h"
#include "a_program/dimension/types.h"
#include "a_program/kernel/structural_reader.h"

struct prototype_frozen_module_snapshot;
struct prototype_intrinsic_environment;
struct prototype_type_semantic_schema_db;
struct prototype_universe_db;
struct symbol_table;

struct prototype_semantic_ih_scope {
	uint32_t match_term;
	uint32_t scrutinee_binding_id;
};

/* These views expose semantic arena prefixes only. Runtime indices, caches,
 * allocation cursors, solver state, and source provenance are unreachable from
 * the checked-Core input type. */
struct prototype_semantic_term_graph_view {
	const struct prototype_term* terms;
	size_t term_count;
	const struct prototype_match_case* cases;
	size_t case_count;
	const struct prototype_case_binder* case_binders;
	size_t case_binder_count;
	const struct prototype_semantic_ih_scope* ih_scopes;
	size_t ih_scope_count;
	const struct prototype_computation_fold_clause* computation_fold_clauses;
	size_t computation_fold_clause_count;
};

struct prototype_semantic_symbol_table_view {
	const char* const* strings;
	size_t count;
};

struct prototype_semantic_intrinsic_environment {
	const struct prototype_pure_primitive_declaration* pure_primitives;
	size_t pure_primitive_count;
	const struct prototype_effect_operation_declaration* effect_operations;
	size_t effect_operation_count;
	int default_integer_host_type;
};

enum prototype_semantic_context_extension_kind {
	PROTOTYPE_SEMANTIC_CONTEXT_EXTENSION_INVALID = 0,
	PROTOTYPE_SEMANTIC_CONTEXT_EXTENSION_VALUE = 1,
	PROTOTYPE_SEMANTIC_CONTEXT_EXTENSION_SEQUENCE_RESULT = 2
};

struct prototype_semantic_context {
	uint32_t parent;
	uint32_t binding_id;
	uint32_t classifier;
	int extension_kind;
	uint32_t producer_computation;
};

struct prototype_semantic_context_graph_view {
	const struct prototype_semantic_context* contexts;
	size_t context_count;
};

enum prototype_semantic_substitution_kind {
	PROTOTYPE_SEMANTIC_SUBSTITUTION_IDENTITY = 1,
	PROTOTYPE_SEMANTIC_SUBSTITUTION_EMPTY = 2,
	PROTOTYPE_SEMANTIC_SUBSTITUTION_PROJECTION = 3,
	PROTOTYPE_SEMANTIC_SUBSTITUTION_EXTEND = 4,
	PROTOTYPE_SEMANTIC_SUBSTITUTION_COMPOSE = 5
};

struct prototype_semantic_substitution {
	int kind;
	uint32_t source_context;
	uint32_t target_context;
	uint32_t first;
	uint32_t second;
	uint32_t term;
	uint32_t term_classifier;
};

struct prototype_semantic_substitution_graph_view {
	const struct prototype_semantic_substitution* substitutions;
	size_t substitution_count;
};

int prototype_semantic_term_structural_reader(
	const struct prototype_semantic_term_graph_view* graph,
	struct prototype_term_structural_reader* p_reader
);

int prototype_semantic_context_structural_reader(
	const struct prototype_semantic_context_graph_view* graph,
	struct prototype_context_structural_reader* p_reader
);

int prototype_semantic_substitution_structural_reader(
	const struct prototype_semantic_substitution_graph_view* graph,
	struct prototype_substitution_structural_reader* p_reader
);

struct prototype_semantic_universe_level {
	uint32_t level_var;
	int value;
};

struct prototype_semantic_universe_view {
	const struct prototype_semantic_universe_level* levels;
	size_t level_count;
};

struct prototype_semantic_type_declaration {
	int name_symbol_id;
	int namespace_symbol_id;
	uint32_t type_index;
	uint32_t representation_id;
	uint32_t formation_classifier;
	uint32_t parameter_context;
	uint32_t parameter_count;
	uint32_t index_context;
	uint32_t index_count;
	uint32_t first_constructor;
	uint32_t constructor_count;
};

struct prototype_semantic_type_constructor {
	int name_symbol_id;
	uint32_t owner_type;
	uint32_t constructor_index;
	uint32_t parameter_context;
	uint32_t field_context;
	uint32_t result_classifier;
};

struct prototype_semantic_type_schema_view {
	const struct prototype_semantic_type_declaration* type_declarations;
	size_t type_count;
	const struct prototype_semantic_type_constructor* constructor_declarations;
	size_t constructor_count;
};

struct prototype_semantic_dimension_operator {
	uint32_t source_dimension;
	uint32_t target_dimension;
	size_t image_offset;
	size_t image_count;
};

struct prototype_semantic_dimension_graph_view {
	const struct prototype_semantic_dimension_operator* operators;
	size_t operator_count;
	const struct prototype_dimension_axis_image* images;
	size_t image_count;
};

enum prototype_semantic_classifier_evidence_kind {
	PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT = 1,
	PROTOTYPE_SEMANTIC_CLASSIFIER_CONDITIONAL = 2
};

enum prototype_semantic_occurrence_kind {
	PROTOTYPE_SEMANTIC_OCCURRENCE_ATOM = 1,
	PROTOTYPE_SEMANTIC_OCCURRENCE_VAR = 2,
	PROTOTYPE_SEMANTIC_OCCURRENCE_REFERENCE = 3,
	PROTOTYPE_SEMANTIC_OCCURRENCE_CONSTRUCTOR = 4,
	PROTOTYPE_SEMANTIC_OCCURRENCE_APP = 5,
	PROTOTYPE_SEMANTIC_OCCURRENCE_LAMBDA = 6,
	PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH = 7,
	PROTOTYPE_SEMANTIC_OCCURRENCE_INDUCTION_HYPOTHESIS = 8,
	PROTOTYPE_SEMANTIC_OCCURRENCE_EXPECTED_TYPE = 9,
	PROTOTYPE_SEMANTIC_OCCURRENCE_RETURN = 10,
	PROTOTYPE_SEMANTIC_OCCURRENCE_THUNK = 11,
	PROTOTYPE_SEMANTIC_OCCURRENCE_FORCE = 12,
	PROTOTYPE_SEMANTIC_OCCURRENCE_REQUEST = 13,
	PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD = 14
};

enum prototype_semantic_match_refinement_kind {
	PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_SOLVED = 1,
	PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_IMPOSSIBLE = 2,
	PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_CONSTANT = 3
};

/* One context-indexed use of an erased Core term. Source AST identity and
 * solver lifecycle are deliberately absent. The asserted classifier is an
 * untrusted checker input, never evidence for itself. */
struct prototype_semantic_occurrence {
	int kind;
	int category;
	int computation_kind;
	int application_role;
	int classifier_evidence_kind;
	uint32_t context_id;
	uint32_t context_action_substitution;
	uint32_t origin_core_term;
	uint32_t origin_classifier;
	uint32_t core_term;
	uint32_t asserted_classifier;
	uint32_t conditional_contract;
	uint32_t binding_id;
	uint32_t first_edge;
	uint32_t edge_count;
	uint32_t wrapped_occurrence;
	uint32_t binder_classifier;
	uint32_t match_motive;
	uint32_t ih_owner_occurrence;
	uint32_t ih_scope_id;
	uint32_t ih_case_index;
	uint32_t ih_field_index;
	uint32_t fold_return_binding_id;
	uint32_t implicit_effect_row_binders[16];
	uint32_t implicit_effect_row_count;
	uint32_t first_case;
	uint32_t case_count;
	uint32_t first_fold_clause;
	uint32_t fold_clause_count;
};

struct prototype_semantic_occurrence_edge {
	int role;
	uint32_t ordinal;
	uint32_t child_occurrence;
};

#define PROTOTYPE_SEMANTIC_MATCH_BINDER_CAPACITY 64

struct prototype_semantic_match_case {
	uint32_t context_id;
	int refinement_kind;
	uint32_t refinement_substitution;
	uint32_t constructor_owner;
	uint32_t constructor_id;
	uint32_t binder_count;
	uint32_t binder_ids[PROTOTYPE_SEMANTIC_MATCH_BINDER_CAPACITY];
};

struct prototype_semantic_fold_clause {
	uint32_t context_id;
	uint32_t argument_binding_id;
	uint32_t continuation_binding_id;
};

struct prototype_semantic_occurrence_graph_view {
	const struct prototype_semantic_occurrence* occurrences;
	size_t occurrence_count;
	const struct prototype_semantic_occurrence_edge* edges;
	size_t edge_count;
	const struct prototype_semantic_match_case* cases;
	size_t case_count;
	const struct prototype_semantic_fold_clause* fold_clauses;
	size_t fold_clause_count;
};

/* A conditional contract is an admitted static interface boundary. It does not
 * represent solver exhaustion or paused producer work. Only pending semantic
 * obligations are projected into this arena. */
enum prototype_semantic_contract_kind {
	PROTOTYPE_SEMANTIC_CONTRACT_COMPUTATION_FOLD_RESULT = 1,
	PROTOTYPE_SEMANTIC_CONTRACT_EFFECT_ROW_EQUATION = 2
};

enum prototype_semantic_effect_constraint_kind {
	PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_EXACT = 1,
	PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_COPY = 2,
	PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_UNION = 3,
	PROTOTYPE_SEMANTIC_EFFECT_CONSTRAINT_RESIDUAL = 4
};

struct prototype_semantic_contract {
	int kind;
	uint32_t occurrence;
	uint32_t core_term;
	uint32_t computation_occurrence;
	uint32_t continuation_occurrence;
	uint32_t continuation_binding_id;
	uint32_t input_classifier;
	uint32_t classifier_family;
	uint32_t effect_row;
	int effect_constraint_kind;
	int normalization_profile;
	uint32_t schema_version;
};

struct prototype_semantic_contract_dependency {
	uint32_t occurrence;
	uint32_t contract_id;
};

struct prototype_semantic_contract_graph_view {
	const struct prototype_semantic_contract* contracts;
	size_t contract_count;
	const struct prototype_semantic_contract_dependency* dependencies;
	size_t dependency_count;
};

enum prototype_semantic_export_transparency {
	PROTOTYPE_SEMANTIC_EXPORT_OPAQUE = 1,
	PROTOTYPE_SEMANTIC_EXPORT_TRANSPARENT = 2
};

struct prototype_semantic_term_export {
	int namespace_symbol_id;
	int name_symbol_id;
	uint32_t occurrence;
	uint32_t term;
	uint32_t classifier;
	int transparency;
};

struct prototype_semantic_type_export {
	int namespace_symbol_id;
	int name_symbol_id;
	uint32_t type_declaration;
	uint32_t first_constructor;
	uint32_t constructor_count;
};

struct prototype_semantic_constructor_export {
	uint32_t type_export;
	int name_symbol_id;
	uint32_t ordinal;
	uint32_t constructor_declaration;
};

struct prototype_semantic_dependency {
	int namespace_symbol_id;
	int name_symbol_id;
};

struct prototype_semantic_function_graph_association {
	uint32_t owner_term_export;
	uint32_t graph_type_export;
	uint32_t result_type_export;
	uint32_t graph_interface_term_export;
	uint32_t certified_adapter_term_export;
	uint32_t certified_runner_term_export;
	uint32_t certified_argument_index;
	uint32_t first_selector_group;
	uint32_t selector_group_count;
};

enum prototype_semantic_function_graph_origin_role {
	PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_VALUE = 1u << 0,
	PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_GRAPH = 1u << 1,
	PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_IH = 1u << 2
};

struct prototype_semantic_function_graph_selector_group {
	uint32_t association;
	uint32_t constructor_ordinal;
	int display_symbol_id;
	uint32_t role_mask;
	uint32_t value_field_ordinal;
	uint32_t graph_field_ordinal;
	int recursive;
};

struct prototype_semantic_interface_view {
	const struct prototype_semantic_term_export* term_exports;
	size_t term_export_count;
	const struct prototype_semantic_type_export* type_exports;
	size_t type_export_count;
	const struct prototype_semantic_constructor_export* constructor_exports;
	size_t constructor_export_count;
	const struct prototype_semantic_dependency* dependencies;
	size_t dependency_count;
	const struct prototype_semantic_function_graph_association*
		function_graph_associations;
	size_t function_graph_association_count;
	const struct prototype_semantic_function_graph_selector_group*
		function_graph_selector_groups;
	size_t function_graph_selector_group_count;
};

/* Semantic runtime requirements are reconstructed by the independent checker.
 * These values are part of checked-Core, not aliases of producer metadata. */
enum prototype_semantic_runtime_capability {
	PROTOTYPE_SEMANTIC_RUNTIME_CAPABILITY_COMPUTATION_FOLD_RESULT_VERIFIER =
		1u << 0,
	PROTOTYPE_SEMANTIC_RUNTIME_CAPABILITY_OPERATION_DISPATCH = 1u << 1,
	PROTOTYPE_SEMANTIC_RUNTIME_CAPABILITY_HANDLER = 1u << 2,
	PROTOTYPE_SEMANTIC_RUNTIME_CAPABILITY_TERMINAL = 1u << 3
};

/* Untrusted, fully elaborated input to independent checking. The view contains
 * assertions and exact semantic structure, but no checked capability. */
struct prototype_elaborated_module_view {
	uint64_t calculus_fingerprint;
	uint64_t intrinsic_fingerprint;
	struct prototype_semantic_intrinsic_environment intrinsic_environment;
	struct prototype_semantic_symbol_table_view symbols;
	struct prototype_semantic_term_graph_view terms;
	struct prototype_semantic_context_graph_view contexts;
	struct prototype_semantic_substitution_graph_view substitutions;
	struct prototype_semantic_universe_view universes;
	struct prototype_semantic_type_schema_view type_schema;
	struct prototype_semantic_dimension_graph_view dimensions;
	struct prototype_semantic_occurrence_graph_view occurrences;
	struct prototype_semantic_contract_graph_view contracts;
	struct prototype_semantic_interface_view interface;
	uint32_t selected_entry_term;
	uint32_t selected_entry_classifier;
	uint32_t selected_entry_occurrence;
	uint64_t required_runtime_capabilities;
};

/* Owns every array reachable through view. The projected module therefore
 * remains valid independently of producer arenas and solver work state. */
struct prototype_elaborated_module {
	struct prototype_elaborated_module_view view;
	char** symbols;
	struct prototype_term* terms;
	struct prototype_match_case* term_cases;
	struct prototype_case_binder* case_binders;
	struct prototype_semantic_ih_scope* ih_scopes;
	struct prototype_computation_fold_clause* computation_fold_clauses;
	struct prototype_pure_primitive_declaration* pure_primitives;
	struct prototype_effect_operation_declaration* effect_operations;
	struct prototype_semantic_context* contexts;
	struct prototype_semantic_substitution* substitutions;
	struct prototype_semantic_universe_level* universe_levels;
	struct prototype_semantic_type_declaration* type_declarations;
	struct prototype_semantic_type_constructor* constructor_declarations;
	struct prototype_semantic_dimension_operator* dimension_operators;
	struct prototype_dimension_axis_image* dimension_images;
	struct prototype_semantic_occurrence* occurrences;
	struct prototype_semantic_occurrence_edge* occurrence_edges;
	struct prototype_semantic_match_case* match_cases;
	struct prototype_semantic_fold_clause* fold_clauses;
	struct prototype_semantic_contract* contracts;
	struct prototype_semantic_contract_dependency* contract_dependencies;
	struct prototype_semantic_term_export* term_exports;
	struct prototype_semantic_type_export* type_exports;
	struct prototype_semantic_constructor_export* constructor_exports;
	struct prototype_semantic_dependency* dependencies;
	struct prototype_semantic_function_graph_association*
		function_graph_associations;
	struct prototype_semantic_function_graph_selector_group*
		function_graph_selector_groups;
};

/* Checked capabilities are intentionally opaque. Numeric IDs from an input
 * artifact cannot be cast into one of these capabilities. */
struct prototype_checked_context_ref;
struct prototype_checked_substitution_ref;
struct prototype_checked_occurrence_ref;
struct prototype_checked_export_ref;

void prototype_elaborated_module_init(
	struct prototype_elaborated_module* module
);

void prototype_elaborated_module_destroy(
	struct prototype_elaborated_module* module
);

int prototype_elaborated_module_project(
	const struct symbol_table* symbols,
	const struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* type_schema,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_universe_db* universes,
	const struct prototype_frozen_module_snapshot* snapshot,
	struct prototype_elaborated_module* module
);

int prototype_elaborated_module_validate_structure(
	const struct prototype_elaborated_module_view* module
);

uint64_t prototype_checker_calculus_fingerprint(void);

uint64_t prototype_semantic_intrinsic_fingerprint(
	const struct prototype_semantic_intrinsic_environment* environment
);

#endif
