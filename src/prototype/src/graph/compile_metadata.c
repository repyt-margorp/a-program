#include "a_program/graph/compile_metadata.h"
#include "a_program/graph/typed_occurrence_graph.h"

#include <string.h>

void prototype_compile_metadata_set_function_graph_storage(
	struct prototype_compile_metadata* metadata,
	struct prototype_function_graph_request* requests,
	size_t request_capacity,
	struct prototype_function_graph_association* associations,
	size_t association_capacity
) {
	if (!metadata) {
		return;
	}
	metadata->function_graph_requests = requests;
	metadata->function_graph_request_capacity = requests ? request_capacity : 0;
	metadata->function_graph_request_count = 0;
	metadata->function_graph_associations = associations;
	metadata->function_graph_association_capacity = associations ?
		association_capacity : 0;
	metadata->function_graph_association_count = 0;
}

int prototype_compile_metadata_request_function_graph(
	struct prototype_compile_metadata* metadata,
	int owner_symbol_id,
	uint32_t owner_assignment_id,
	uint32_t owner_source_entry_id,
	uint32_t request_flags,
	uint32_t requesting_ast,
	uint32_t* p_request_id
) {
	if (!metadata || !metadata->function_graph_requests || owner_symbol_id < 0 ||
		owner_assignment_id == PROTOTYPE_INVALID_ID ||
		owner_source_entry_id == PROTOTYPE_INVALID_ID || request_flags == 0 ||
		!p_request_id) {
		return -1;
	}
	for (uint32_t i = 0; i < metadata->function_graph_request_count; ++i) {
		struct prototype_function_graph_request* request =
			&metadata->function_graph_requests[i];
		if (request->owner_assignment_id != owner_assignment_id ||
			request->owner_source_entry_id != owner_source_entry_id) {
			continue;
		}
		request->request_flags |= request_flags;
		*p_request_id = i;
		return 0;
	}
	if (metadata->function_graph_request_count >=
		metadata->function_graph_request_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)metadata->function_graph_request_count++;
	metadata->function_graph_requests[id] =
		(struct prototype_function_graph_request) {
			.owner_symbol_id = owner_symbol_id,
			.owner_assignment_id = owner_assignment_id,
			.owner_source_entry_id = owner_source_entry_id,
			.request_flags = request_flags,
			.first_requesting_ast = requesting_ast,
			.state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_PENDING,
			.reason = PROTOTYPE_FUNCTION_GRAPH_REASON_NONE
		};
	*p_request_id = id;
	return 0;
}

const struct prototype_function_graph_request*
prototype_compile_metadata_function_graph_request(
	const struct prototype_compile_metadata* metadata,
	uint32_t request_id
) {
	return metadata && metadata->function_graph_requests &&
		request_id < metadata->function_graph_request_count ?
		&metadata->function_graph_requests[request_id] : NULL;
}

int prototype_compile_metadata_add_function_graph_association(
	struct prototype_compile_metadata* metadata,
	struct prototype_function_graph_association association,
	uint32_t* p_association_id
) {
	if (!metadata || !metadata->function_graph_associations ||
		association.owner_symbol_id < 0 ||
		(!association.imported &&
			(association.owner_assignment_id == PROTOTYPE_INVALID_ID ||
			 association.owner_source_entry_id == PROTOTYPE_INVALID_ID)) ||
		!p_association_id) {
		return -1;
	}
	for (uint32_t i = 0; i < metadata->function_graph_association_count; ++i) {
		const struct prototype_function_graph_association* existing =
			&metadata->function_graph_associations[i];
		if (existing->owner_symbol_id == association.owner_symbol_id &&
			existing->imported && association.imported &&
			existing->imported_interface_index ==
				association.imported_interface_index &&
			existing->imported_owner_term_export_index ==
				association.imported_owner_term_export_index) {
			*p_association_id = i;
			return 0;
		}
		if ((!existing->imported && !association.imported &&
				(existing->owner_assignment_id == association.owner_assignment_id ||
				 existing->owner_source_entry_id ==
					association.owner_source_entry_id)) ||
			existing->owner_symbol_id == association.owner_symbol_id) {
			return -1;
		}
	}
	if (metadata->function_graph_association_count >=
		metadata->function_graph_association_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)metadata->function_graph_association_count++;
	metadata->function_graph_associations[id] = association;
	*p_association_id = id;
	return 0;
}

const struct prototype_function_graph_association*
prototype_compile_metadata_function_graph_association_for_owner(
	const struct prototype_compile_metadata* metadata,
	int owner_symbol_id
) {
	if (!metadata || !metadata->function_graph_associations || owner_symbol_id < 0) {
		return NULL;
	}
	for (size_t i = 0; i < metadata->function_graph_association_count; ++i) {
		if (metadata->function_graph_associations[i].owner_symbol_id ==
			owner_symbol_id) {
			return &metadata->function_graph_associations[i];
		}
	}
	return NULL;
}

int prototype_compile_metadata_frozen_snapshot(
	const struct prototype_compile_metadata* metadata,
	struct prototype_frozen_module_snapshot* p_snapshot
) {
	if (!metadata || !p_snapshot || !metadata->typed_occurrences.frozen ||
		!metadata->typed_occurrences.sealed ||
		metadata->typed_occurrences.transaction_active) {
		return -1;
	}
	*p_snapshot = (struct prototype_frozen_module_snapshot) {
		.reduction_environment = metadata->reduction_environment,
		.contexts = metadata->contexts,
		.substitutions = metadata->substitutions,
		.dimension_operators = metadata->dimension_operators,
		.typed_occurrences = metadata->typed_occurrences,
		.verification = metadata->verification,
		.selected_entry_term = metadata->selected_entry_term,
		.selected_entry_classifier = metadata->selected_entry_classifier,
		.selected_entry_occurrence = metadata->selected_entry_occurrence,
		.required_runtime_capabilities = metadata->required_runtime_capabilities
	};
	return 0;
}

void prototype_compile_metadata_set_accepted_substitution_claim_storage(
	struct prototype_compile_metadata* metadata,
	uint32_t* claim_ids,
	size_t claim_id_capacity
) {
	if (!metadata) {
		return;
	}
	metadata->accepted_substitution_claims = claim_ids;
	metadata->accepted_substitution_claim_capacity = claim_id_capacity;
	if (!claim_ids) {
		metadata->accepted_substitution_claim_capacity = 0;
		return;
	}
	for (size_t i = 0; i < claim_id_capacity; ++i) {
		claim_ids[i] = PROTOTYPE_INVALID_ID;
	}
}

int prototype_compile_metadata_record_accepted_substitution_claim(
	struct prototype_compile_metadata* metadata,
	uint32_t substitution_id,
	uint32_t claim_id
) {
	if (!metadata || !metadata->accepted_substitution_claims ||
		substitution_id >= metadata->accepted_substitution_claim_capacity ||
		claim_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t existing = metadata->accepted_substitution_claims[substitution_id];
	if (existing != PROTOTYPE_INVALID_ID && existing != claim_id) {
		return -1;
	}
	metadata->accepted_substitution_claims[substitution_id] = claim_id;
	return 0;
}

uint32_t prototype_compile_metadata_accepted_substitution_claim(
	const struct prototype_compile_metadata* metadata,
	uint32_t substitution_id
) {
	return metadata && metadata->accepted_substitution_claims &&
		substitution_id < metadata->accepted_substitution_claim_capacity ?
		metadata->accepted_substitution_claims[substitution_id] :
		PROTOTYPE_INVALID_ID;
}

int prototype_compile_metadata_append_accepted_substitution_claims(
	struct prototype_compile_metadata* target,
	const struct prototype_compile_metadata* source,
	const uint32_t* substitution_relocation,
	size_t substitution_relocation_count,
	const uint32_t* claim_relocation,
	size_t claim_relocation_count
) {
	if (!target || !source || !target->accepted_substitution_claims ||
		!source->accepted_substitution_claims || !substitution_relocation ||
		!claim_relocation || substitution_relocation_count <
			source->substitutions.substitution_count) {
		return -1;
	}
	for (size_t i = 0; i < source->substitutions.substitution_count; ++i) {
		uint32_t source_claim =
			prototype_compile_metadata_accepted_substitution_claim(source, (uint32_t)i);
		if (source_claim == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (source_claim >= claim_relocation_count ||
			substitution_relocation[i] == PROTOTYPE_INVALID_ID ||
			claim_relocation[source_claim] == PROTOTYPE_INVALID_ID ||
			substitution_relocation[i] >=
				target->accepted_substitution_claim_capacity) {
			return -1;
		}
		uint32_t target_substitution = substitution_relocation[i];
		uint32_t target_claim = claim_relocation[source_claim];
		uint32_t existing = target->accepted_substitution_claims[
			target_substitution
		];
		/* Linking constructs a new canonical artifact. If structurally equal
		 * substitutions merge, choose deterministically among their exact,
		 * already accepted evidence Claims. */
		if (existing == PROTOTYPE_INVALID_ID || target_claim < existing) {
			target->accepted_substitution_claims[target_substitution] = target_claim;
		}
	}
	return 0;
}

enum prototype_type_inspection_state prototype_compile_metadata_inspect_type(
	const struct prototype_compile_metadata* metadata,
	int name_symbol_id,
	struct prototype_type_inspection* p_inspection
) {
	if (!metadata || !p_inspection) {
		return PROTOTYPE_TYPE_INSPECTION_INVALID;
	}
	for (size_t i = metadata->label_count; i > 0; --i) {
		const struct prototype_compile_label* label = &metadata->labels[i - 1];
		if (label->name_symbol_id != name_symbol_id) {
			continue;
		}
		if (label->body_occurrence >= metadata->typed_occurrences.occurrence_count ||
			label->exposed_occurrence >= metadata->typed_occurrences.occurrence_count ||
			label->body_classifier == PROTOTYPE_INVALID_ID ||
			label->exposed_classifier == PROTOTYPE_INVALID_ID ||
			metadata->typed_occurrences.occurrences[label->body_occurrence].classifier !=
				label->body_classifier ||
			metadata->typed_occurrences.occurrences[label->exposed_occurrence].classifier !=
				label->exposed_classifier) {
			return PROTOTYPE_TYPE_INSPECTION_AMBIGUOUS;
		}
		*p_inspection = (struct prototype_type_inspection) {
			.body_occurrence = label->body_occurrence,
			.body_classifier = label->body_classifier,
			.exposed_occurrence = label->exposed_occurrence,
			.exposed_classifier = label->exposed_classifier,
			.expectation_classifier = label->expectation_classifier,
			.expectation_claim_id = label->expectation_claim_id
		};
		return PROTOTYPE_TYPE_INSPECTION_AVAILABLE;
	}
	return PROTOTYPE_TYPE_INSPECTION_UNAVAILABLE;
}

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
	struct prototype_typed_occurrence* occurrences,
	size_t occurrence_capacity,
	struct prototype_typed_occurrence_edge* occurrence_edges,
	size_t occurrence_edge_capacity,
	struct prototype_typed_occurrence_match_case* occurrence_match_cases,
	size_t occurrence_match_case_capacity,
	struct prototype_typed_occurrence_fold_clause* occurrence_fold_clauses,
	size_t occurrence_fold_clause_capacity,
	struct prototype_occurrence_effect_constraint* effect_constraints,
	size_t effect_constraint_capacity,
	struct prototype_verification_obligation* verification_obligations,
	size_t verification_obligation_capacity,
	struct prototype_verification_dependency* verification_dependencies,
	size_t verification_dependency_capacity
) {
	memset(metadata, 0, sizeof(*metadata));
	metadata->compile_policy = PROTOTYPE_COMPILE_POLICY_HYBRID;
	metadata->definition_thunk_policy = PROTOTYPE_DEFINITION_THUNK_IMPLICIT;
	metadata->selected_entry_symbol_id = -1;
	metadata->selected_entry_term = PROTOTYPE_INVALID_ID;
	metadata->selected_entry_classifier = PROTOTYPE_INVALID_ID;
	metadata->selected_entry_occurrence = PROTOTYPE_INVALID_ID;
	metadata->reduction_environment.system_nat_owner = PROTOTYPE_INVALID_ID;
	metadata->reduction_environment.system_nat_zero_constructor =
		PROTOTYPE_INVALID_ID;
	metadata->reduction_environment.system_nat_succ_constructor =
		PROTOTYPE_INVALID_ID;
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
	prototype_typed_occurrence_graph_init(
		&metadata->typed_occurrences,
		occurrences,
		occurrence_capacity,
		occurrence_edges,
		occurrence_edge_capacity,
		occurrence_match_cases,
		occurrence_match_case_capacity,
		occurrence_fold_clauses,
		occurrence_fold_clause_capacity
	);
	metadata->effect_constraints = effect_constraints;
	metadata->effect_constraint_capacity = effect_constraint_capacity;
	prototype_verification_db_init(
		&metadata->verification,
		verification_obligations,
		verification_obligation_capacity,
		verification_dependencies,
		verification_dependency_capacity
	);
}

void prototype_compile_metadata_set_dimension_storage(
	struct prototype_compile_metadata* metadata,
	struct prototype_dimension_operator* operators,
	size_t operator_capacity,
	struct prototype_dimension_axis_image* images,
	size_t image_capacity
) {
	if (!metadata) {
		return;
	}
	prototype_dimension_operator_db_init(
		&metadata->dimension_operators,
		operators,
		operator_capacity,
		images,
		image_capacity
	);
}

void prototype_compile_metadata_set_diagnostic_storage(
	struct prototype_compile_metadata* metadata,
	struct prototype_compile_diagnostic* diagnostics,
	size_t diagnostic_capacity
) {
	if (!metadata) {
		return;
	}
	metadata->compile_diagnostics = diagnostics;
	metadata->compile_diagnostic_capacity = diagnostic_capacity;
	metadata->compile_diagnostic_count = 0;
}

int prototype_compile_metadata_add_diagnostic(
	struct prototype_compile_metadata* metadata,
	struct prototype_compile_diagnostic diagnostic
) {
	if (!metadata || !metadata->compile_diagnostics ||
		metadata->compile_diagnostic_count >= metadata->compile_diagnostic_capacity ||
		diagnostic.phase <= 0 || diagnostic.reason <= 0) {
		return -1;
	}
	if (metadata->compile_diagnostic_count > 0) {
		return 0;
	}
	metadata->compile_diagnostics[metadata->compile_diagnostic_count++] = diagnostic;
	return 0;
}
