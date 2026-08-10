#include "a_program/graph/compile_metadata.h"

#include <string.h>

void prototype_compile_metadata_init(
	struct prototype_compile_metadata* metadata,
	struct prototype_compile_label* labels,
	size_t label_capacity,
	struct prototype_compile_type_export* type_exports,
	size_t type_export_capacity,
	struct prototype_compile_constructor_export* constructor_exports,
	size_t constructor_export_capacity,
	struct prototype_resolve_error* resolve_errors,
	size_t resolve_error_capacity,
	struct prototype_resolution_item* resolution_items,
	size_t resolution_item_capacity,
	struct prototype_resolution_iteration* resolution_iterations,
	size_t resolution_iteration_capacity,
	struct prototype_resolution_event* resolution_events,
	size_t resolution_event_capacity,
	struct prototype_context* contexts,
	size_t context_capacity,
	struct prototype_substitution* substitutions,
	size_t substitution_capacity,
	struct prototype_operation_node* operations,
	size_t operation_capacity,
	struct prototype_operation_match_case* operation_cases,
	size_t operation_case_capacity,
	struct prototype_operation_computation_fold_clause* operation_fold_clauses,
	size_t operation_fold_clause_capacity,
	struct prototype_operation_effect_constraint* effect_constraints,
	size_t effect_constraint_capacity,
	struct prototype_verification_obligation* verification_obligations,
	size_t verification_obligation_capacity
) {
	memset(metadata, 0, sizeof(*metadata));
	metadata->compile_policy = PROTOTYPE_COMPILE_POLICY_HYBRID;
	metadata->definition_thunk_policy = PROTOTYPE_DEFINITION_THUNK_IMPLICIT;
	metadata->selected_entry_symbol_id = -1;
	metadata->selected_entry_term = PROTOTYPE_INVALID_ID;
	metadata->selected_entry_classifier = PROTOTYPE_INVALID_ID;
	metadata->selected_entry_operation = PROTOTYPE_INVALID_ID;
	metadata->normalization_step_limit = PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT;
	metadata->solver_step_limit = PROTOTYPE_SOLVER_DEFAULT_STEP_LIMIT;
	prototype_context_db_init(&metadata->contexts, contexts, context_capacity);
	prototype_substitution_db_init(
		&metadata->substitutions,
		substitutions,
		substitution_capacity
	);
	metadata->labels = labels;
	metadata->label_capacity = label_capacity;
	metadata->type_exports = type_exports;
	metadata->type_export_capacity = type_export_capacity;
	metadata->constructor_exports = constructor_exports;
	metadata->constructor_export_capacity = constructor_export_capacity;
	metadata->resolve_errors = resolve_errors;
	metadata->resolve_error_capacity = resolve_error_capacity;
	metadata->resolution_items = resolution_items;
	metadata->resolution_item_capacity = resolution_item_capacity;
	metadata->resolution_iterations = resolution_iterations;
	metadata->resolution_iteration_capacity = resolution_iteration_capacity;
	metadata->resolution_events = resolution_events;
	metadata->resolution_event_capacity = resolution_event_capacity;
	metadata->operations = operations;
	metadata->operation_capacity = operation_capacity;
	metadata->operation_cases = operation_cases;
	metadata->operation_case_capacity = operation_case_capacity;
	metadata->operation_fold_clauses = operation_fold_clauses;
	metadata->operation_fold_clause_capacity = operation_fold_clause_capacity;
	metadata->effect_constraints = effect_constraints;
	metadata->effect_constraint_capacity = effect_constraint_capacity;
	prototype_verification_db_init(
		&metadata->verification,
		verification_obligations,
		verification_obligation_capacity
	);
}
