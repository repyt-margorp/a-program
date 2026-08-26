#include "internal.h"

#include "a_program/core/term.h"

#include <stddef.h>
#include <string.h>

struct semantic_reference_field {
	size_t offset;
	int target;
	int requirement;
};

#define REFERENCE(type, member, target_value, requirement_value) \
	{ offsetof(type, member), (target_value), (requirement_value) }
#define TERM_REFERENCE(type, member, requirement_value) \
	REFERENCE(type, member, PROTOTYPE_CHECKER_REFERENCE_TERM, requirement_value)

static const struct semantic_reference_field context_fields[] = {
	REFERENCE(struct prototype_semantic_context, parent,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	TERM_REFERENCE(struct prototype_semantic_context, classifier,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	TERM_REFERENCE(struct prototype_semantic_context, producer_computation,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL)
};
static const struct semantic_reference_field substitution_fields[] = {
	REFERENCE(struct prototype_semantic_substitution, source_context,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_substitution, target_context,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_substitution, first,
		PROTOTYPE_CHECKER_REFERENCE_SUBSTITUTION,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	REFERENCE(struct prototype_semantic_substitution, second,
		PROTOTYPE_CHECKER_REFERENCE_SUBSTITUTION,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_substitution, term,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_substitution, term_classifier,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL)
};
static const struct semantic_reference_field type_fields[] = {
	TERM_REFERENCE(struct prototype_semantic_type_declaration,
		formation_classifier, PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_type_declaration, parameter_context,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_type_declaration, index_context,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field constructor_fields[] = {
	REFERENCE(struct prototype_semantic_type_constructor, owner_type,
		PROTOTYPE_CHECKER_REFERENCE_TYPE,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_type_constructor, parameter_context,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_type_constructor, field_context,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	TERM_REFERENCE(struct prototype_semantic_type_constructor,
		result_classifier, PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field occurrence_fields[] = {
	REFERENCE(struct prototype_semantic_occurrence, context_id,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_occurrence, context_action_substitution,
		PROTOTYPE_CHECKER_REFERENCE_SUBSTITUTION,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_occurrence, core_term,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	TERM_REFERENCE(struct prototype_semantic_occurrence, origin_core_term,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_occurrence, origin_classifier,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_occurrence, asserted_classifier,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_occurrence, binder_classifier,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_occurrence, match_motive,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	REFERENCE(struct prototype_semantic_occurrence, conditional_contract,
		PROTOTYPE_CHECKER_REFERENCE_CONTRACT,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	REFERENCE(struct prototype_semantic_occurrence, wrapped_occurrence,
		PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	REFERENCE(struct prototype_semantic_occurrence, ih_owner_occurrence,
		PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	REFERENCE(struct prototype_semantic_occurrence, ih_scope_id,
		PROTOTYPE_CHECKER_REFERENCE_IH_SCOPE,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL)
};
static const struct semantic_reference_field occurrence_edge_fields[] = {
	REFERENCE(struct prototype_semantic_occurrence_edge, child_occurrence,
		PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field match_case_fields[] = {
	REFERENCE(struct prototype_semantic_match_case, context_id,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_match_case, refinement_substitution,
		PROTOTYPE_CHECKER_REFERENCE_SUBSTITUTION,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_match_case, constructor_owner,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field fold_clause_fields[] = {
	REFERENCE(struct prototype_semantic_fold_clause, context_id,
		PROTOTYPE_CHECKER_REFERENCE_CONTEXT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field contract_fields[] = {
	REFERENCE(struct prototype_semantic_contract, occurrence,
		PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_contract, computation_occurrence,
		PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	REFERENCE(struct prototype_semantic_contract, continuation_occurrence,
		PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_contract, core_term,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	TERM_REFERENCE(struct prototype_semantic_contract, input_classifier,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_contract, classifier_family,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	TERM_REFERENCE(struct prototype_semantic_contract, effect_row,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL)
};
static const struct semantic_reference_field contract_dependency_fields[] = {
	REFERENCE(struct prototype_semantic_contract_dependency, occurrence,
		PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_contract_dependency, contract_id,
		PROTOTYPE_CHECKER_REFERENCE_CONTRACT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field term_export_fields[] = {
	REFERENCE(struct prototype_semantic_term_export, occurrence,
		PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	TERM_REFERENCE(struct prototype_semantic_term_export, term,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	TERM_REFERENCE(struct prototype_semantic_term_export, classifier,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_term_export, namespace_symbol_id,
		PROTOTYPE_CHECKER_REFERENCE_SYMBOL,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_term_export, name_symbol_id,
		PROTOTYPE_CHECKER_REFERENCE_SYMBOL,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field type_export_fields[] = {
	REFERENCE(struct prototype_semantic_type_export, type_declaration,
		PROTOTYPE_CHECKER_REFERENCE_TYPE,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_type_export, namespace_symbol_id,
		PROTOTYPE_CHECKER_REFERENCE_SYMBOL,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_type_export, name_symbol_id,
		PROTOTYPE_CHECKER_REFERENCE_SYMBOL,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field constructor_export_fields[] = {
	REFERENCE(struct prototype_semantic_constructor_export, type_export,
		PROTOTYPE_CHECKER_REFERENCE_TYPE_EXPORT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_constructor_export,
		constructor_declaration, PROTOTYPE_CHECKER_REFERENCE_CONSTRUCTOR,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_constructor_export, name_symbol_id,
		PROTOTYPE_CHECKER_REFERENCE_SYMBOL,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field dependency_fields[] = {
	REFERENCE(struct prototype_semantic_dependency, namespace_symbol_id,
		PROTOTYPE_CHECKER_REFERENCE_SYMBOL,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_dependency, name_symbol_id,
		PROTOTYPE_CHECKER_REFERENCE_SYMBOL,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field association_fields[] = {
	REFERENCE(struct prototype_semantic_function_graph_association,
		owner_term_export, PROTOTYPE_CHECKER_REFERENCE_TERM_EXPORT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_function_graph_association,
		graph_type_export, PROTOTYPE_CHECKER_REFERENCE_TYPE_EXPORT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_function_graph_association,
		result_type_export, PROTOTYPE_CHECKER_REFERENCE_TYPE_EXPORT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_function_graph_association,
		graph_interface_term_export, PROTOTYPE_CHECKER_REFERENCE_TERM_EXPORT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_function_graph_association,
		certified_adapter_term_export, PROTOTYPE_CHECKER_REFERENCE_TERM_EXPORT,
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL),
	REFERENCE(struct prototype_semantic_function_graph_association,
		certified_runner_term_export, PROTOTYPE_CHECKER_REFERENCE_TERM_EXPORT,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};
static const struct semantic_reference_field selector_fields[] = {
	REFERENCE(struct prototype_semantic_function_graph_selector_group,
		association, PROTOTYPE_CHECKER_REFERENCE_FUNCTION_GRAPH_ASSOCIATION,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED),
	REFERENCE(struct prototype_semantic_function_graph_selector_group,
		display_symbol_id, PROTOTYPE_CHECKER_REFERENCE_SYMBOL,
		PROTOTYPE_CHECKER_REFERENCE_REQUIRED)
};

static int visit_record_array(
	const void* records,
	size_t record_count,
	size_t record_size,
	const struct semantic_reference_field* fields,
	size_t field_count,
	prototype_checker_semantic_reference_visitor visitor,
	void* state
) {
	if ((record_count != 0 && !records) || !fields || !visitor) return -1;
	for (size_t i = 0; i < record_count; ++i) {
		const unsigned char* record = (const unsigned char*)records + i * record_size;
		for (size_t j = 0; j < field_count; ++j) {
			uint32_t value;
			memcpy(&value, record + fields[j].offset, sizeof(value));
			if (visitor(
					state, fields[j].target, fields[j].requirement, value
				) != 0) return -1;
		}
	}
	return 0;
}

static int relocate_record_array(
	void* records,
	size_t record_count,
	size_t record_size,
	const struct semantic_reference_field* fields,
	size_t field_count,
	const uint32_t* relocation,
	size_t source_count
) {
	if ((record_count != 0 && !records) || !fields || !relocation) return -1;
	for (size_t i = 0; i < record_count; ++i) {
		unsigned char* record = (unsigned char*)records + i * record_size;
		for (size_t j = 0; j < field_count; ++j) {
			if (fields[j].target != PROTOTYPE_CHECKER_REFERENCE_TERM) continue;
			uint32_t value;
			memcpy(&value, record + fields[j].offset, sizeof(value));
			if (value == PROTOTYPE_INVALID_ID && fields[j].requirement ==
				PROTOTYPE_CHECKER_REFERENCE_OPTIONAL) continue;
			if (value >= source_count || relocation[value] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			value = relocation[value];
			memcpy(record + fields[j].offset, &value, sizeof(value));
		}
	}
	return 0;
}

#define VISIT_ARRAY(records, count, fields) \
	visit_record_array((records), (count), sizeof(*(records)), (fields), \
		sizeof(fields) / sizeof((fields)[0]), visitor, state)

int prototype_checker_visit_semantic_references(
	const struct prototype_elaborated_module_view* module,
	prototype_checker_semantic_reference_visitor visitor,
	void* state
) {
	if (!module || !visitor || visitor(
			state, PROTOTYPE_CHECKER_REFERENCE_TERM,
			PROTOTYPE_CHECKER_REFERENCE_OPTIONAL,
			module->selected_entry_term
		) != 0 || visitor(
			state, PROTOTYPE_CHECKER_REFERENCE_TERM,
			PROTOTYPE_CHECKER_REFERENCE_OPTIONAL,
			module->selected_entry_classifier
		) != 0 || visitor(
			state, PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE,
			PROTOTYPE_CHECKER_REFERENCE_OPTIONAL,
			module->selected_entry_occurrence
		) != 0 || (module->contexts.context_count > 1 && VISIT_ARRAY(
			&module->contexts.contexts[1], module->contexts.context_count - 1,
			context_fields
		) != 0) || VISIT_ARRAY(
			module->substitutions.substitutions,
			module->substitutions.substitution_count, substitution_fields
		) != 0 || VISIT_ARRAY(
			module->type_schema.type_declarations,
			module->type_schema.type_count, type_fields
		) != 0 || VISIT_ARRAY(
			module->type_schema.constructor_declarations,
			module->type_schema.constructor_count, constructor_fields
		) != 0 || VISIT_ARRAY(
			module->occurrences.occurrences,
			module->occurrences.occurrence_count, occurrence_fields
		) != 0 || VISIT_ARRAY(
			module->occurrences.edges,
			module->occurrences.edge_count, occurrence_edge_fields
		) != 0 || VISIT_ARRAY(
			module->occurrences.cases,
			module->occurrences.case_count, match_case_fields
		) != 0 || VISIT_ARRAY(
			module->occurrences.fold_clauses,
			module->occurrences.fold_clause_count, fold_clause_fields
		) != 0 || VISIT_ARRAY(
			module->contracts.contracts,
			module->contracts.contract_count, contract_fields
		) != 0 || VISIT_ARRAY(
			module->contracts.dependencies,
			module->contracts.dependency_count, contract_dependency_fields
		) != 0 || VISIT_ARRAY(
			module->interface.term_exports,
			module->interface.term_export_count, term_export_fields
		) != 0 || VISIT_ARRAY(
			module->interface.type_exports,
			module->interface.type_export_count, type_export_fields
		) != 0 || VISIT_ARRAY(
			module->interface.constructor_exports,
			module->interface.constructor_export_count, constructor_export_fields
		) != 0 || VISIT_ARRAY(
			module->interface.dependencies,
			module->interface.dependency_count, dependency_fields
		) != 0 || VISIT_ARRAY(
			module->interface.function_graph_associations,
			module->interface.function_graph_association_count, association_fields
		) != 0 || VISIT_ARRAY(
			module->interface.function_graph_selector_groups,
			module->interface.function_graph_selector_group_count, selector_fields
		) != 0) return -1;
	return 0;
}

struct term_reference_adapter {
	prototype_checker_term_reference_visitor visitor;
	void* state;
};

static int visit_term_reference_only(
	void* state,
	int target,
	int requirement,
	uint32_t reference
) {
	struct term_reference_adapter* adapter = state;
	if (!adapter || !adapter->visitor) return -1;
	return target == PROTOTYPE_CHECKER_REFERENCE_TERM ? adapter->visitor(
		adapter->state, requirement, reference
	) : 0;
}

int prototype_checker_visit_semantic_term_roots(
	const struct prototype_elaborated_module_view* module,
	prototype_checker_term_reference_visitor visitor,
	void* state
) {
	struct term_reference_adapter adapter = {
		.visitor = visitor,
		.state = state
	};
	return prototype_checker_visit_semantic_references(
		module, visit_term_reference_only, &adapter
	);
}

#define RELOCATE_ARRAY(records, count, fields) \
	relocate_record_array((records), (count), sizeof(*(records)), (fields), \
		sizeof(fields) / sizeof((fields)[0]), relocation, source_count)

static int relocate_root(
	uint32_t* p_value,
	int requirement,
	const uint32_t* relocation,
	size_t source_count
) {
	if (!p_value || !relocation) return -1;
	if (*p_value == PROTOTYPE_INVALID_ID && requirement ==
		PROTOTYPE_CHECKER_REFERENCE_OPTIONAL) return 0;
	if (*p_value >= source_count || relocation[*p_value] == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_value = relocation[*p_value];
	return 0;
}

int prototype_checker_relocate_semantic_term_roots(
	struct prototype_elaborated_module* module,
	const uint32_t* relocation,
	size_t source_count
) {
	if (!module || !relocation || relocate_root(
			&module->view.selected_entry_term,
			PROTOTYPE_CHECKER_REFERENCE_OPTIONAL, relocation, source_count
		) != 0 || relocate_root(
			&module->view.selected_entry_classifier,
			PROTOTYPE_CHECKER_REFERENCE_OPTIONAL, relocation, source_count
		) != 0 || (module->view.contexts.context_count > 1 && RELOCATE_ARRAY(
			&module->contexts[1], module->view.contexts.context_count - 1,
			context_fields
		) != 0) || RELOCATE_ARRAY(
			module->substitutions, module->view.substitutions.substitution_count,
			substitution_fields
		) != 0 || RELOCATE_ARRAY(
			module->type_declarations, module->view.type_schema.type_count,
			type_fields
		) != 0 || RELOCATE_ARRAY(
			module->constructor_declarations,
			module->view.type_schema.constructor_count, constructor_fields
		) != 0 || RELOCATE_ARRAY(
			module->occurrences, module->view.occurrences.occurrence_count,
			occurrence_fields
		) != 0 || RELOCATE_ARRAY(
			module->match_cases, module->view.occurrences.case_count,
			match_case_fields
		) != 0 || RELOCATE_ARRAY(
			module->contracts, module->view.contracts.contract_count,
			contract_fields
		) != 0 || RELOCATE_ARRAY(
			module->term_exports, module->view.interface.term_export_count,
			term_export_fields
		) != 0) return -1;
	return 0;
}

#undef RELOCATE_ARRAY
#undef VISIT_ARRAY
#undef TERM_REFERENCE
#undef REFERENCE
