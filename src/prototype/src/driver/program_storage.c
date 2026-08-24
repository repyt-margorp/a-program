#include "a_program/driver/compiler_session.h"

#include <stdlib.h>
#include <string.h>

#define PROGRAM_SYMBOL_MAP_CAPACITY 1024
#define PROGRAM_SYMBOL_STORAGE_CAPACITY 512
#define PROGRAM_TYPE_CAPACITY 64
#define PROGRAM_CONSTRUCTOR_CAPACITY 256
#define PROGRAM_PARAMETER_CAPACITY 128
#define PROGRAM_FIELD_TYPE_CAPACITY 512
#define PROGRAM_TYPE_EXPR_CAPACITY 1024
#define PROGRAM_AST_CAPACITY 4096
#define PROGRAM_AST_DEF_CAPACITY 256
#define PROGRAM_AST_MATCH_CASE_CAPACITY 256
#define PROGRAM_AST_MATCH_BINDER_CAPACITY 512
#define PROGRAM_AST_MATCH_SELECTOR_CAPACITY 512
#define PROGRAM_AST_FOLD_CLAUSE_CAPACITY 256
#define PROGRAM_AST_BLOCK_ITEM_CAPACITY 4096
#define PROGRAM_AST_DEFINITION_ITEM_CAPACITY 4096
#define PROGRAM_AST_TYPE_EXPR_CAPACITY 4096
#define PROGRAM_AST_TYPE_DEF_CAPACITY 64
#define PROGRAM_AST_FAMILY_BINDER_CAPACITY 128
#define PROGRAM_AST_TYPE_CONSTRUCTOR_CAPACITY 256
#define PROGRAM_AST_TYPE_FIELD_EXPR_CAPACITY 512
#define PROGRAM_AST_ACCEPTED_BINDING_SUBSTITUTION_CAPACITY 8192
#define PROGRAM_UNIVERSE_NODE_CAPACITY 256
#define PROGRAM_UNIVERSE_EDGE_CAPACITY 512
#define PROGRAM_UNIVERSE_LEVEL_CAPACITY 1024
#define PROGRAM_UNIVERSE_CONSTRAINT_CAPACITY 4096
#define PROGRAM_TERM_CAPACITY 262144
#define PROGRAM_MATCH_CASE_CAPACITY 262144
#define PROGRAM_MATCH_BINDER_CAPACITY 262144
#define PROGRAM_MATCH_FRAME_CAPACITY 4096
#define PROGRAM_JUDGEMENT_CAPACITY 4096
#define PROGRAM_COMPILE_LABEL_CAPACITY 512
#define PROGRAM_COMPILE_TYPE_EXPORT_CAPACITY 256
#define PROGRAM_COMPILE_CONSTRUCTOR_EXPORT_CAPACITY 512
#define PROGRAM_RESOLVE_ERROR_CAPACITY 512
#define PROGRAM_COMPILE_DIAGNOSTIC_CAPACITY 512
#define PROGRAM_RESOLUTION_ITEM_CAPACITY 2048
#define PROGRAM_RESOLUTION_ITERATION_CAPACITY 128
#define PROGRAM_RESOLUTION_EVENT_CAPACITY 2048
#define PROGRAM_OPERATION_CAPACITY 4096
#define PROGRAM_OCCURRENCE_EDGE_CAPACITY (PROGRAM_OPERATION_CAPACITY * 8)
#define PROGRAM_OPERATION_CASE_CAPACITY 4096
#define PROGRAM_OPERATION_FOLD_CLAUSE_CAPACITY 4096
#define PROGRAM_VERIFICATION_OBLIGATION_CAPACITY 4096
#define PROGRAM_VERIFICATION_DEPENDENCY_CAPACITY 8192
#define PROGRAM_DIMENSION_OPERATOR_CAPACITY 256
#define PROGRAM_DIMENSION_IMAGE_CAPACITY 4096
#define PROGRAM_FUNCTION_GRAPH_REQUEST_CAPACITY 128
#define PROGRAM_FUNCTION_GRAPH_ASSOCIATION_CAPACITY 128
#define PROGRAM_FUNCTION_GRAPH_ORIGIN_GROUP_CAPACITY 2048
#define PROGRAM_ARTIFACT_TERM_EXPORT_CAPACITY 512
#define PROGRAM_ARTIFACT_TYPE_EXPORT_CAPACITY 256
#define PROGRAM_ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY 512
#define PROGRAM_ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY 512
#define PROGRAM_ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY 1024
#define PROGRAM_ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY 2048
#define PROGRAM_ARTIFACT_IDENTITY_ROOT_CAPACITY 512
#define PROGRAM_ARTIFACT_FUNCTION_GRAPH_SELECTOR_GROUP_CAPACITY 2048
#define PROGRAM_ARTIFACT_DEPENDENCY_CAPACITY 512
#define PROGRAM_ARTIFACT_EXTERNAL_TERM_REF_CAPACITY 512
#define PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY 512
#define PROGRAM_ARTIFACT_RESOLVED_CONSTRUCTOR_OWNER_REF_CAPACITY 1024
#define PROGRAM_ARTIFACT_DEBUG_NAME_CAPACITY 1024
#define PROGRAM_ARTIFACT_DEFINITION_CAPACITY 512

struct prototype_program_storage_backing {
	int symbol_ids[PROGRAM_SYMBOL_MAP_CAPACITY];
	uint32_t symbol_hashes[PROGRAM_SYMBOL_MAP_CAPACITY];
	char* symbol_strings[PROGRAM_SYMBOL_STORAGE_CAPACITY];
	struct prototype_type_declaration type_declarations[PROGRAM_TYPE_CAPACITY];
	struct prototype_type_constructor_declaration constructors[PROGRAM_CONSTRUCTOR_CAPACITY];
	struct prototype_type_readback_entry type_readback_entries[PROGRAM_TYPE_CAPACITY];
	struct prototype_type_constructor_readback constructor_readback[PROGRAM_CONSTRUCTOR_CAPACITY];
	struct prototype_constructor_classifier_cache_entry constructor_classifier_cache[PROGRAM_CONSTRUCTOR_CAPACITY];
	struct prototype_type_parameter_declaration parameters[PROGRAM_PARAMETER_CAPACITY];
	uint32_t field_types[PROGRAM_FIELD_TYPE_CAPACITY];
	struct prototype_type_expr type_exprs[PROGRAM_TYPE_EXPR_CAPACITY];
	struct prototype_type_representation type_representations[PROGRAM_TYPE_CAPACITY];
	struct prototype_ast_node ast_nodes[PROGRAM_AST_CAPACITY];
	struct prototype_ast_type_expectation_def ast_expectations[PROGRAM_AST_DEF_CAPACITY];
	struct prototype_ast_term_assignment_def ast_assignments[PROGRAM_AST_DEF_CAPACITY];
	struct prototype_ast_import_def ast_imports[PROGRAM_AST_DEF_CAPACITY];
	struct prototype_ast_def_open_address_entry ast_def_index[PROGRAM_AST_DEF_CAPACITY];
	struct prototype_ast_match_case ast_match_cases[PROGRAM_AST_MATCH_CASE_CAPACITY];
	struct prototype_ast_binder ast_match_binders[PROGRAM_AST_MATCH_BINDER_CAPACITY];
	struct prototype_ast_match_selector ast_match_selectors[
		PROGRAM_AST_MATCH_SELECTOR_CAPACITY
	];
	struct prototype_ast_computation_fold_clause ast_fold_clauses[PROGRAM_AST_FOLD_CLAUSE_CAPACITY];
	uint32_t ast_block_items[PROGRAM_AST_BLOCK_ITEM_CAPACITY];
	uint32_t ast_definition_items[PROGRAM_AST_DEFINITION_ITEM_CAPACITY];
	struct prototype_ast_type_expr ast_type_exprs[PROGRAM_AST_TYPE_EXPR_CAPACITY];
	struct prototype_ast_type_def ast_type_defs[PROGRAM_AST_TYPE_DEF_CAPACITY];
	struct prototype_ast_family_binder ast_family_binders[PROGRAM_AST_FAMILY_BINDER_CAPACITY];
	struct prototype_ast_type_constructor ast_type_constructors[PROGRAM_AST_TYPE_CONSTRUCTOR_CAPACITY];
	uint32_t ast_type_field_exprs[PROGRAM_AST_TYPE_FIELD_EXPR_CAPACITY];
	uint32_t ast_type_field_binder_ids[PROGRAM_AST_TYPE_FIELD_EXPR_CAPACITY];
	int ast_type_field_name_symbol_ids[PROGRAM_AST_TYPE_FIELD_EXPR_CAPACITY];
	struct prototype_ast_accepted_binding_substitution
		ast_accepted_binding_substitutions[
			PROGRAM_AST_ACCEPTED_BINDING_SUBSTITUTION_CAPACITY
		];
	struct prototype_universe_node universe_nodes[PROGRAM_UNIVERSE_NODE_CAPACITY];
	struct prototype_universe_edge universe_edges[PROGRAM_UNIVERSE_EDGE_CAPACITY];
	struct prototype_universe_level universe_levels[PROGRAM_UNIVERSE_LEVEL_CAPACITY];
	struct prototype_universe_constraint universe_constraints[PROGRAM_UNIVERSE_CONSTRAINT_CAPACITY];
	struct prototype_universe_obligation_span
		universe_obligation_spans[PROTOTYPE_UNIVERSE_OBLIGATION_SPAN_CAPACITY];
	struct prototype_term terms[PROGRAM_TERM_CAPACITY];
	struct prototype_match_case match_cases[PROGRAM_MATCH_CASE_CAPACITY];
	int match_case_label_symbols[PROGRAM_MATCH_CASE_CAPACITY];
	struct prototype_case_binder match_binders[PROGRAM_MATCH_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scopes[PROGRAM_MATCH_FRAME_CAPACITY];
	struct prototype_judgement_proposition propositions[PROGRAM_JUDGEMENT_CAPACITY];
	struct prototype_judgement_derivation_candidate candidates[PROGRAM_JUDGEMENT_CAPACITY];
	struct prototype_judgement_claim claims[PROGRAM_JUDGEMENT_CAPACITY];
	struct prototype_judgement_derivation derivations[PROGRAM_JUDGEMENT_CAPACITY];
	struct prototype_judgement_candidate_premise candidate_premises[
		PROGRAM_JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_premise_edge accepted_premises[
		PROGRAM_JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_usage_entry judgement_resource_usage[PROGRAM_JUDGEMENT_CAPACITY * 32];
	struct prototype_compile_label compile_labels[PROGRAM_COMPILE_LABEL_CAPACITY];
	struct prototype_compile_type_export compile_type_exports[PROGRAM_COMPILE_TYPE_EXPORT_CAPACITY];
	struct prototype_compile_constructor_export compile_constructor_exports[PROGRAM_COMPILE_CONSTRUCTOR_EXPORT_CAPACITY];
	struct prototype_function_graph_request
		function_graph_requests[PROGRAM_FUNCTION_GRAPH_REQUEST_CAPACITY];
	struct prototype_function_graph_association
		function_graph_associations[PROGRAM_FUNCTION_GRAPH_ASSOCIATION_CAPACITY];
	struct prototype_function_graph_origin_group
		function_graph_origin_groups[PROGRAM_FUNCTION_GRAPH_ORIGIN_GROUP_CAPACITY];
	struct prototype_resolve_error resolve_errors[PROGRAM_RESOLVE_ERROR_CAPACITY];
	struct prototype_compile_diagnostic compile_diagnostics[PROGRAM_COMPILE_DIAGNOSTIC_CAPACITY];
	struct prototype_resolution_item resolution_items[PROGRAM_RESOLUTION_ITEM_CAPACITY];
	struct prototype_resolution_iteration resolution_iterations[PROGRAM_RESOLUTION_ITERATION_CAPACITY];
	struct prototype_resolution_event resolution_events[PROGRAM_RESOLUTION_EVENT_CAPACITY];
	struct prototype_context contexts[PROTOTYPE_CONTEXT_CAPACITY];
	struct prototype_substitution substitutions[PROTOTYPE_SUBSTITUTION_CAPACITY];
	uint32_t accepted_substitution_claims[PROTOTYPE_SUBSTITUTION_CAPACITY];
	struct prototype_dimension_operator
		dimension_operators[PROGRAM_DIMENSION_OPERATOR_CAPACITY];
	struct prototype_dimension_axis_image
		dimension_images[PROGRAM_DIMENSION_IMAGE_CAPACITY];
	struct prototype_typed_occurrence occurrences[PROGRAM_OPERATION_CAPACITY];
	struct prototype_typed_occurrence_edge occurrence_edges[PROGRAM_OCCURRENCE_EDGE_CAPACITY];
	struct prototype_typed_occurrence_match_case occurrence_cases[PROGRAM_OPERATION_CASE_CAPACITY];
	struct prototype_typed_occurrence_fold_clause fold_clauses[PROGRAM_OPERATION_FOLD_CLAUSE_CAPACITY];
	struct prototype_verification_obligation verification_obligations[PROGRAM_VERIFICATION_OBLIGATION_CAPACITY];
	struct prototype_verification_dependency verification_dependencies[
		PROGRAM_VERIFICATION_DEPENDENCY_CAPACITY
	];
};

struct prototype_artifact_interface_storage_backing {
	struct prototype_artifact_term_export
		term_exports[PROGRAM_ARTIFACT_TERM_EXPORT_CAPACITY];
	struct prototype_artifact_type_export
		type_exports[PROGRAM_ARTIFACT_TYPE_EXPORT_CAPACITY];
	struct prototype_artifact_type_parameter_export
		type_parameters[PROGRAM_ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY];
	struct prototype_artifact_constructor_export
		constructor_exports[PROGRAM_ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY];
	uint32_t constructor_field_type_exprs[
		PROGRAM_ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY
	];
	struct prototype_type_expr
		type_exprs[PROGRAM_ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY];
	struct prototype_artifact_identity_root
		identity_roots[PROGRAM_ARTIFACT_IDENTITY_ROOT_CAPACITY];
	struct prototype_artifact_function_graph_selector_group
		function_graph_selector_groups[
			PROGRAM_ARTIFACT_FUNCTION_GRAPH_SELECTOR_GROUP_CAPACITY
		];
	struct prototype_artifact_dependency
		dependencies[PROGRAM_ARTIFACT_DEPENDENCY_CAPACITY];
	struct prototype_artifact_external_term_ref
		external_term_refs[PROGRAM_ARTIFACT_EXTERNAL_TERM_REF_CAPACITY];
	struct prototype_artifact_resolved_external_term_ref
		resolved_external_term_refs[PROGRAM_ARTIFACT_EXTERNAL_TERM_REF_CAPACITY];
	struct prototype_artifact_external_type_expr_ref
		external_type_expr_refs[PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY];
	struct prototype_artifact_resolved_external_type_expr_ref
		resolved_external_type_expr_refs[
			PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY
		];
	struct prototype_artifact_external_type_former_ref
		external_type_former_refs[
			PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY
		];
	struct prototype_artifact_resolved_external_type_former_ref
		resolved_external_type_former_refs[
			PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY
		];
	struct prototype_artifact_resolved_constructor_owner_ref
		resolved_constructor_owner_refs[
			PROGRAM_ARTIFACT_RESOLVED_CONSTRUCTOR_OWNER_REF_CAPACITY
		];
	struct prototype_artifact_debug_term_name
		debug_term_names[PROGRAM_ARTIFACT_DEBUG_NAME_CAPACITY];
	struct prototype_artifact_debug_type_name
		debug_type_names[PROGRAM_ARTIFACT_DEBUG_NAME_CAPACITY];
	struct prototype_artifact_debug_constructor_name
		debug_constructor_names[PROGRAM_ARTIFACT_DEBUG_NAME_CAPACITY];
	struct prototype_term_definition
		definitions[PROGRAM_ARTIFACT_DEFINITION_CAPACITY];
};

static void initialize_program_storage_views(
	struct prototype_program_storage* storage
) {
	struct prototype_program_storage_backing* b = storage->backing;
	symbol_table_init(&storage->symbols, b->symbol_ids, b->symbol_hashes,
		PROGRAM_SYMBOL_MAP_CAPACITY, b->symbol_strings,
		PROGRAM_SYMBOL_STORAGE_CAPACITY);
	prototype_type_declaration_db_init(&storage->type_declarations,
		b->type_declarations, PROGRAM_TYPE_CAPACITY, b->constructors,
		PROGRAM_CONSTRUCTOR_CAPACITY, b->type_readback_entries,
		PROGRAM_TYPE_CAPACITY, b->parameters, PROGRAM_PARAMETER_CAPACITY,
		b->constructor_readback, PROGRAM_CONSTRUCTOR_CAPACITY, b->field_types,
		PROGRAM_FIELD_TYPE_CAPACITY, b->type_exprs, PROGRAM_TYPE_EXPR_CAPACITY,
		b->type_representations, PROGRAM_TYPE_CAPACITY,
		b->constructor_classifier_cache, PROGRAM_CONSTRUCTOR_CAPACITY);
	prototype_ast_db_init(&storage->asts, b->ast_nodes, PROGRAM_AST_CAPACITY,
		b->ast_expectations, PROGRAM_AST_DEF_CAPACITY, b->ast_assignments,
		PROGRAM_AST_DEF_CAPACITY, b->ast_imports, PROGRAM_AST_DEF_CAPACITY,
		b->ast_def_index, PROGRAM_AST_DEF_CAPACITY, b->ast_match_cases,
		PROGRAM_AST_MATCH_CASE_CAPACITY, b->ast_match_binders,
		PROGRAM_AST_MATCH_BINDER_CAPACITY, b->ast_fold_clauses,
		PROGRAM_AST_FOLD_CLAUSE_CAPACITY, b->ast_block_items,
		PROGRAM_AST_BLOCK_ITEM_CAPACITY, b->ast_definition_items,
		PROGRAM_AST_DEFINITION_ITEM_CAPACITY, b->ast_type_exprs,
		PROGRAM_AST_TYPE_EXPR_CAPACITY, b->ast_type_defs,
		PROGRAM_AST_TYPE_DEF_CAPACITY, b->ast_family_binders,
		PROGRAM_AST_FAMILY_BINDER_CAPACITY, b->ast_type_constructors,
		PROGRAM_AST_TYPE_CONSTRUCTOR_CAPACITY, b->ast_type_field_exprs,
		b->ast_type_field_binder_ids, b->ast_type_field_name_symbol_ids,
		PROGRAM_AST_TYPE_FIELD_EXPR_CAPACITY);
	prototype_ast_db_set_accepted_substitution_storage(
		&storage->asts,
		b->ast_accepted_binding_substitutions,
		PROGRAM_AST_ACCEPTED_BINDING_SUBSTITUTION_CAPACITY
	);
	prototype_ast_db_set_match_selector_storage(
		&storage->asts,
		b->ast_match_selectors,
		PROGRAM_AST_MATCH_SELECTOR_CAPACITY
	);
	prototype_universe_db_init(&storage->universe, b->universe_nodes,
		PROGRAM_UNIVERSE_NODE_CAPACITY, b->universe_edges,
		PROGRAM_UNIVERSE_EDGE_CAPACITY, b->universe_levels,
		PROGRAM_UNIVERSE_LEVEL_CAPACITY, b->universe_constraints,
		PROGRAM_UNIVERSE_CONSTRAINT_CAPACITY, b->universe_obligation_spans,
		PROTOTYPE_UNIVERSE_OBLIGATION_SPAN_CAPACITY);
	prototype_term_db_init(&storage->terms, b->terms, PROGRAM_TERM_CAPACITY,
		b->match_cases, b->match_case_label_symbols, PROGRAM_MATCH_CASE_CAPACITY,
		b->match_binders, PROGRAM_MATCH_BINDER_CAPACITY, b->ih_scopes,
		PROGRAM_MATCH_FRAME_CAPACITY);
	prototype_compile_metadata_init(&storage->metadata, b->compile_labels,
		PROGRAM_COMPILE_LABEL_CAPACITY, b->compile_type_exports,
		PROGRAM_COMPILE_TYPE_EXPORT_CAPACITY, b->compile_constructor_exports,
		PROGRAM_COMPILE_CONSTRUCTOR_EXPORT_CAPACITY, b->resolve_errors,
		PROGRAM_RESOLVE_ERROR_CAPACITY, b->resolution_items,
		PROGRAM_RESOLUTION_ITEM_CAPACITY, b->resolution_iterations,
		PROGRAM_RESOLUTION_ITERATION_CAPACITY, b->resolution_events,
		PROGRAM_RESOLUTION_EVENT_CAPACITY, b->contexts, PROTOTYPE_CONTEXT_CAPACITY,
		b->substitutions, PROTOTYPE_SUBSTITUTION_CAPACITY, b->occurrences,
		PROGRAM_OPERATION_CAPACITY, b->occurrence_edges,
		PROGRAM_OCCURRENCE_EDGE_CAPACITY, b->occurrence_cases,
		PROGRAM_OPERATION_CASE_CAPACITY, b->fold_clauses,
		PROGRAM_OPERATION_FOLD_CLAUSE_CAPACITY, b->verification_obligations,
			PROGRAM_VERIFICATION_OBLIGATION_CAPACITY, b->verification_dependencies,
			PROGRAM_VERIFICATION_DEPENDENCY_CAPACITY);
	prototype_compile_metadata_set_dimension_storage(
		&storage->metadata,
		b->dimension_operators,
		PROGRAM_DIMENSION_OPERATOR_CAPACITY,
		b->dimension_images,
		PROGRAM_DIMENSION_IMAGE_CAPACITY
	);
	prototype_compile_metadata_set_accepted_substitution_claim_storage(
		&storage->metadata,
		b->accepted_substitution_claims,
		PROTOTYPE_SUBSTITUTION_CAPACITY
	);
	prototype_compile_metadata_set_function_graph_storage(
		&storage->metadata,
		b->function_graph_requests,
		PROGRAM_FUNCTION_GRAPH_REQUEST_CAPACITY,
		b->function_graph_associations,
		PROGRAM_FUNCTION_GRAPH_ASSOCIATION_CAPACITY,
		b->function_graph_origin_groups,
		PROGRAM_FUNCTION_GRAPH_ORIGIN_GROUP_CAPACITY
	);
	prototype_compile_metadata_set_diagnostic_storage(&storage->metadata,
		b->compile_diagnostics, PROGRAM_COMPILE_DIAGNOSTIC_CAPACITY);
	prototype_judgement_db_init(&storage->judgement, b->propositions,
		b->candidates, b->claims, b->derivations, PROGRAM_JUDGEMENT_CAPACITY,
		b->candidate_premises,
		PROGRAM_JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		b->accepted_premises,
		PROGRAM_JUDGEMENT_CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES);
	prototype_judgement_db_set_resource_usage_storage(&storage->judgement,
		b->judgement_resource_usage, PROGRAM_JUDGEMENT_CAPACITY * 32);
	storage->program.intrinsic_environment = prototype_default_intrinsic_environment();
	storage->program.symbols = &storage->symbols;
	storage->program.namespace_symbol_id = -1;
	storage->program.asts = &storage->asts;
	storage->program.type_declarations = &storage->type_declarations;
	storage->program.terms = &storage->terms;
	storage->program.judgement = &storage->judgement;
	storage->program.metadata = &storage->metadata;
	storage->program.universe = &storage->universe;
}

int prototype_program_storage_init(struct prototype_program_storage* storage) {
	if (!storage) {
		return -1;
	}
	memset(storage, 0, sizeof(*storage));
	storage->backing = calloc(1, sizeof(*storage->backing));
	if (!storage->backing) {
		return -1;
	}
	initialize_program_storage_views(storage);
	return 0;
}

int prototype_program_storage_reset(struct prototype_program_storage* storage) {
	if (!storage || !storage->backing) {
		return -1;
	}
	prototype_term_db_dispose_runtime_state(&storage->terms);
	symbol_table_free(&storage->symbols);
	memset(storage->backing, 0, sizeof(*storage->backing));
	memset(&storage->program, 0, sizeof(storage->program));
	memset(&storage->symbols, 0, sizeof(storage->symbols));
	memset(&storage->type_declarations, 0, sizeof(storage->type_declarations));
	memset(&storage->asts, 0, sizeof(storage->asts));
	memset(&storage->terms, 0, sizeof(storage->terms));
	memset(&storage->judgement, 0, sizeof(storage->judgement));
	memset(&storage->metadata, 0, sizeof(storage->metadata));
	memset(&storage->universe, 0, sizeof(storage->universe));
	initialize_program_storage_views(storage);
	return 0;
}

void prototype_program_storage_destroy(struct prototype_program_storage* storage) {
	if (!storage) {
		return;
	}
	prototype_term_db_dispose_runtime_state(&storage->terms);
	symbol_table_free(&storage->symbols);
	free(storage->backing);
	memset(storage, 0, sizeof(*storage));
}

static void initialize_artifact_interface_storage_views(
	struct prototype_artifact_interface_storage* storage
) {
	struct prototype_artifact_interface_storage_backing* b = storage->backing;
	prototype_artifact_interface_init(
		&storage->interface,
		b->term_exports,
		PROGRAM_ARTIFACT_TERM_EXPORT_CAPACITY,
		b->type_exports,
		PROGRAM_ARTIFACT_TYPE_EXPORT_CAPACITY,
		b->type_parameters,
		PROGRAM_ARTIFACT_TYPE_PARAMETER_EXPORT_CAPACITY,
		b->constructor_exports,
		PROGRAM_ARTIFACT_CONSTRUCTOR_EXPORT_CAPACITY,
		b->constructor_field_type_exprs,
		PROGRAM_ARTIFACT_CONSTRUCTOR_FIELD_TYPE_EXPR_CAPACITY,
		b->type_exprs,
		PROGRAM_ARTIFACT_INTERFACE_TYPE_EXPR_CAPACITY,
		b->identity_roots,
		PROGRAM_ARTIFACT_IDENTITY_ROOT_CAPACITY,
		b->function_graph_selector_groups,
		PROGRAM_ARTIFACT_FUNCTION_GRAPH_SELECTOR_GROUP_CAPACITY,
		b->dependencies,
		PROGRAM_ARTIFACT_DEPENDENCY_CAPACITY
	);
	prototype_artifact_relocation_table_init(
		&storage->relocation,
		b->external_term_refs,
		PROGRAM_ARTIFACT_EXTERNAL_TERM_REF_CAPACITY,
		b->resolved_external_term_refs,
		PROGRAM_ARTIFACT_EXTERNAL_TERM_REF_CAPACITY,
		b->external_type_expr_refs,
		PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY,
		b->resolved_external_type_expr_refs,
		PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY,
		b->external_type_former_refs,
		PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY,
		b->resolved_external_type_former_refs,
		PROGRAM_ARTIFACT_EXTERNAL_TYPE_EXPR_REF_CAPACITY,
		b->resolved_constructor_owner_refs,
		PROGRAM_ARTIFACT_RESOLVED_CONSTRUCTOR_OWNER_REF_CAPACITY
	);
	prototype_artifact_debug_table_init(
		&storage->debug,
		b->debug_term_names,
		PROGRAM_ARTIFACT_DEBUG_NAME_CAPACITY,
		b->debug_type_names,
		PROGRAM_ARTIFACT_DEBUG_NAME_CAPACITY,
		b->debug_constructor_names,
		PROGRAM_ARTIFACT_DEBUG_NAME_CAPACITY
	);
}

int prototype_artifact_interface_storage_init(
	struct prototype_artifact_interface_storage* storage
) {
	if (!storage) {
		return -1;
	}
	memset(storage, 0, sizeof(*storage));
	storage->backing = calloc(1, sizeof(*storage->backing));
	if (!storage->backing) {
		return -1;
	}
	initialize_artifact_interface_storage_views(storage);
	return 0;
}

int prototype_artifact_interface_storage_reset(
	struct prototype_artifact_interface_storage* storage
) {
	if (!storage || !storage->backing) {
		return -1;
	}
	memset(storage->backing, 0, sizeof(*storage->backing));
	memset(&storage->interface, 0, sizeof(storage->interface));
	memset(&storage->relocation, 0, sizeof(storage->relocation));
	memset(&storage->debug, 0, sizeof(storage->debug));
	initialize_artifact_interface_storage_views(storage);
	return 0;
}

void prototype_artifact_interface_storage_destroy(
	struct prototype_artifact_interface_storage* storage
) {
	if (!storage) {
		return;
	}
	free(storage->backing);
	memset(storage, 0, sizeof(*storage));
}

struct prototype_term_definition*
prototype_artifact_interface_storage_definitions(
	struct prototype_artifact_interface_storage* storage
) {
	return storage && storage->backing ? storage->backing->definitions : NULL;
}

size_t prototype_artifact_interface_storage_definition_capacity(
	const struct prototype_artifact_interface_storage* storage
) {
	return storage && storage->backing ?
		PROGRAM_ARTIFACT_DEFINITION_CAPACITY : 0;
}
