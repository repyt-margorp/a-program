#include "a_program/checker/container.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define CHECKED_WIRE_MAGIC "APCHK087"
#define CHECKED_WIRE_MAGIC_SIZE 8
#define CHECKED_WIRE_SECTION_SEMANTIC 1
#define CHECKED_WIRE_SECTION_CONTRACTS 2
#define CHECKED_WIRE_SECTION_PRODUCER 3
#define CHECKED_WIRE_SECTION_DEBUG 4
#define CHECKED_WIRE_SECTION_COUNT 2
#define CHECKED_WIRE_MAX_COUNT UINT32_C(16777216)
#define CHECKED_WIRE_MAX_SECTION_SIZE UINT64_C(1073741824)
#define CHECKED_WIRE_FNV_OFFSET UINT64_C(1469598103934665603)
#define CHECKED_WIRE_FNV_PRIME UINT64_C(1099511628211)

struct checked_wire_buffer {
	unsigned char* bytes;
	size_t count;
	size_t capacity;
};

struct checked_wire_reader {
	const unsigned char* bytes;
	size_t count;
	size_t cursor;
};

static int checked_wire_term(
	struct checked_wire_buffer* buffer,
	const struct prototype_term* term
);

static void checked_wire_buffer_destroy(struct checked_wire_buffer* buffer) {
	if (!buffer) {
		return;
	}
	free(buffer->bytes);
	memset(buffer, 0, sizeof(*buffer));
}

static int checked_wire_reserve(
	struct checked_wire_buffer* buffer,
	size_t additional
) {
	if (!buffer || additional > SIZE_MAX - buffer->count) {
		return -1;
	}
	size_t required = buffer->count + additional;
	if (required <= buffer->capacity) {
		return 0;
	}
	size_t capacity = buffer->capacity == 0 ? 4096 : buffer->capacity;
	while (capacity < required) {
		if (capacity > SIZE_MAX / 2) {
			capacity = required;
			break;
		}
		capacity *= 2;
	}
	unsigned char* bytes = realloc(buffer->bytes, capacity);
	if (!bytes) {
		return -1;
	}
	buffer->bytes = bytes;
	buffer->capacity = capacity;
	return 0;
}

static int checked_wire_bytes(
	struct checked_wire_buffer* buffer,
	const void* bytes,
	size_t count
) {
	if (checked_wire_reserve(buffer, count) != 0 || (count != 0 && !bytes)) {
		return -1;
	}
	if (count != 0) {
		memcpy(&buffer->bytes[buffer->count], bytes, count);
	}
	buffer->count += count;
	return 0;
}

static int checked_wire_u32(
	struct checked_wire_buffer* buffer,
	uint32_t value
) {
	unsigned char bytes[4];
	for (uint32_t i = 0; i < 4; ++i) {
		bytes[i] = (unsigned char)(value >> (i * 8));
	}
	return checked_wire_bytes(buffer, bytes, sizeof(bytes));
}

static int checked_wire_i32(struct checked_wire_buffer* buffer, int value) {
	if (value < INT32_MIN || value > INT32_MAX) {
		return -1;
	}
	return checked_wire_u32(buffer, (uint32_t)(int32_t)value);
}

static int checked_wire_u64(
	struct checked_wire_buffer* buffer,
	uint64_t value
) {
	unsigned char bytes[8];
	for (uint32_t i = 0; i < 8; ++i) {
		bytes[i] = (unsigned char)(value >> (i * 8));
	}
	return checked_wire_bytes(buffer, bytes, sizeof(bytes));
}

static int checked_wire_i64(
	struct checked_wire_buffer* buffer,
	int64_t value
) {
	return checked_wire_u64(buffer, (uint64_t)value);
}

static int checked_wire_count(
	struct checked_wire_buffer* buffer,
	size_t count
) {
	return count <= UINT32_MAX ? checked_wire_u32(buffer, (uint32_t)count) : -1;
}

static int checked_wire_string(
	struct checked_wire_buffer* buffer,
	const char* string
) {
	if (!string) {
		return -1;
	}
	size_t length = strlen(string);
	return checked_wire_count(buffer, length) == 0 &&
		checked_wire_bytes(buffer, string, length) == 0 ? 0 : -1;
}

static int checked_read_bytes(
	struct checked_wire_reader* reader,
	void* target,
	size_t count
) {
	if (!reader || reader->cursor > reader->count ||
		count > reader->count - reader->cursor ||
		(count != 0 && !target)) {
		return -1;
	}
	if (count != 0) {
		memcpy(target, &reader->bytes[reader->cursor], count);
	}
	reader->cursor += count;
	return 0;
}

static int checked_wire_semantic(
	struct checked_wire_buffer* buffer,
	const struct prototype_elaborated_module_view* module
) {
	if (!buffer || !module) {
		return -1;
	}
#define WU(value) if (checked_wire_u32(buffer, (uint32_t)(value)) != 0) return -1
#define WI(value) if (checked_wire_i32(buffer, (int)(value)) != 0) return -1
#define WC(value) if (checked_wire_count(buffer, (value)) != 0) return -1
	if (checked_wire_u64(buffer, module->calculus_fingerprint) != 0 ||
		checked_wire_u64(buffer, module->intrinsic_fingerprint) != 0) {
		return -1;
	}
	WC(module->intrinsic_environment.pure_primitive_count);
	for (size_t i = 0;
		i < module->intrinsic_environment.pure_primitive_count; ++i) {
		const struct prototype_pure_primitive_declaration* value =
			&module->intrinsic_environment.pure_primitives[i];
		WI(value->primitive_id);
		WU(value->arity);
		for (size_t j = 0; j < PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY; ++j) {
			WI(value->argument_types[j]);
		}
		WI(value->result_type);
	}
	WC(module->intrinsic_environment.effect_operation_count);
	for (size_t i = 0;
		i < module->intrinsic_environment.effect_operation_count; ++i) {
		const struct prototype_effect_operation_declaration* value =
			&module->intrinsic_environment.effect_operations[i];
		WI(value->operation_id);
		WI(value->classifier_schema);
		WU(value->required_host_effects);
		WU(value->arity);
		WI(value->inner_policy);
		WI(value->resumption_multiplicity);
	}
	WI(module->intrinsic_environment.default_integer_host_type);

	WC(module->symbols.count);
	for (size_t i = 0; i < module->symbols.count; ++i) {
		if (checked_wire_string(buffer, module->symbols.strings[i]) != 0) {
			return -1;
		}
	}
	WC(module->terms.term_count);
	for (size_t i = 0; i < module->terms.term_count; ++i) {
		if (checked_wire_term(buffer, &module->terms.terms[i]) != 0) {
			return -1;
		}
	}
	WC(module->terms.case_count);
	for (size_t i = 0; i < module->terms.case_count; ++i) {
		const struct prototype_match_case* value = &module->terms.cases[i];
		WU(value->constructor_owner); WU(value->constructor_id);
		WU(value->first_binder); WU(value->binder_count); WU(value->body);
	}
	WC(module->terms.case_binder_count);
	for (size_t i = 0; i < module->terms.case_binder_count; ++i) {
		WU(module->terms.case_binders[i].binding_id);
		WI(module->terms.case_binders[i].is_recursive);
	}
	WC(module->terms.ih_scope_count);
	for (size_t i = 0; i < module->terms.ih_scope_count; ++i) {
		WU(module->terms.ih_scopes[i].match_term);
		WU(module->terms.ih_scopes[i].scrutinee_binding_id);
	}
	WC(module->terms.computation_fold_clause_count);
	for (size_t i = 0; i < module->terms.computation_fold_clause_count; ++i) {
		WU(module->terms.computation_fold_clauses[i].operation);
		WU(module->terms.computation_fold_clauses[i].body);
	}

	WC(module->contexts.context_count);
	for (size_t i = 0; i < module->contexts.context_count; ++i) {
		const struct prototype_semantic_context* value =
			&module->contexts.contexts[i];
		WU(value->parent); WU(value->binding_id); WU(value->classifier);
		WI(value->extension_kind); WU(value->producer_computation);
	}
	WC(module->substitutions.substitution_count);
	for (size_t i = 0; i < module->substitutions.substitution_count; ++i) {
		const struct prototype_semantic_substitution* value =
			&module->substitutions.substitutions[i];
		WI(value->kind); WU(value->source_context); WU(value->target_context);
		WU(value->first); WU(value->second); WU(value->term);
		WU(value->term_classifier);
	}
	WC(module->universes.level_count);
	for (size_t i = 0; i < module->universes.level_count; ++i) {
		WU(module->universes.levels[i].level_var);
		WI(module->universes.levels[i].value);
	}
	WC(module->type_schema.type_count);
	for (size_t i = 0; i < module->type_schema.type_count; ++i) {
		const struct prototype_semantic_type_declaration* value =
			&module->type_schema.type_declarations[i];
		WI(value->name_symbol_id); WI(value->namespace_symbol_id);
		WU(value->type_index); WU(value->representation_id);
		WU(value->formation_classifier); WU(value->parameter_context);
		WU(value->parameter_count); WU(value->index_context);
		WU(value->index_count); WU(value->first_constructor);
		WU(value->constructor_count);
	}
	WC(module->type_schema.constructor_count);
	for (size_t i = 0; i < module->type_schema.constructor_count; ++i) {
		const struct prototype_semantic_type_constructor* value =
			&module->type_schema.constructor_declarations[i];
		WI(value->name_symbol_id); WU(value->owner_type);
		WU(value->constructor_index); WU(value->parameter_context);
		WU(value->field_context); WU(value->result_classifier);
	}
	WC(module->dimensions.operator_count);
	for (size_t i = 0; i < module->dimensions.operator_count; ++i) {
		const struct prototype_semantic_dimension_operator* value =
			&module->dimensions.operators[i];
		WU(value->source_dimension); WU(value->target_dimension);
		WC(value->image_offset); WC(value->image_count);
	}
	WC(module->dimensions.image_count);
	for (size_t i = 0; i < module->dimensions.image_count; ++i) {
		WI(module->dimensions.images[i].kind);
		WU(module->dimensions.images[i].target_axis);
	}

	WC(module->occurrences.occurrence_count);
	for (size_t i = 0; i < module->occurrences.occurrence_count; ++i) {
		const struct prototype_semantic_occurrence* value =
			&module->occurrences.occurrences[i];
		WI(value->kind); WI(value->category); WI(value->computation_kind);
		WI(value->application_role); WI(value->classifier_evidence_kind);
		WU(value->context_id); WU(value->context_action_substitution);
		WU(value->origin_core_term); WU(value->origin_classifier);
		WU(value->core_term); WU(value->asserted_classifier);
		WU(value->conditional_contract); WU(value->binding_id);
		WU(value->first_edge); WU(value->edge_count);
		WU(value->wrapped_occurrence); WU(value->binder_classifier);
		WU(value->match_motive); WU(value->ih_owner_occurrence);
		WU(value->ih_scope_id); WU(value->ih_case_index);
		WU(value->ih_field_index); WU(value->fold_return_binding_id);
		for (size_t j = 0; j < 16; ++j) {
			WU(value->implicit_effect_row_binders[j]);
		}
		WU(value->implicit_effect_row_count); WU(value->first_case);
		WU(value->case_count); WU(value->first_fold_clause);
		WU(value->fold_clause_count);
	}
	WC(module->occurrences.edge_count);
	for (size_t i = 0; i < module->occurrences.edge_count; ++i) {
		WI(module->occurrences.edges[i].role);
		WU(module->occurrences.edges[i].ordinal);
		WU(module->occurrences.edges[i].child_occurrence);
	}
	WC(module->occurrences.case_count);
	for (size_t i = 0; i < module->occurrences.case_count; ++i) {
		const struct prototype_semantic_match_case* value =
			&module->occurrences.cases[i];
		WU(value->context_id); WI(value->refinement_kind);
		WU(value->refinement_substitution); WU(value->constructor_owner);
		WU(value->constructor_id); WU(value->binder_count);
		for (size_t j = 0; j < PROTOTYPE_SEMANTIC_MATCH_BINDER_CAPACITY; ++j) {
			WU(value->binder_ids[j]);
		}
	}
	WC(module->occurrences.fold_clause_count);
	for (size_t i = 0; i < module->occurrences.fold_clause_count; ++i) {
		const struct prototype_semantic_fold_clause* value =
			&module->occurrences.fold_clauses[i];
		WU(value->context_id); WU(value->argument_binding_id);
		WU(value->continuation_binding_id);
	}

	WC(module->interface.term_export_count);
	for (size_t i = 0; i < module->interface.term_export_count; ++i) {
		const struct prototype_semantic_term_export* value =
			&module->interface.term_exports[i];
		WI(value->namespace_symbol_id); WI(value->name_symbol_id);
		WU(value->occurrence); WU(value->term); WU(value->classifier);
		WI(value->transparency);
	}
	WC(module->interface.type_export_count);
	for (size_t i = 0; i < module->interface.type_export_count; ++i) {
		const struct prototype_semantic_type_export* value =
			&module->interface.type_exports[i];
		WI(value->namespace_symbol_id); WI(value->name_symbol_id);
		WU(value->type_declaration); WU(value->first_constructor);
		WU(value->constructor_count);
	}
	WC(module->interface.constructor_export_count);
	for (size_t i = 0; i < module->interface.constructor_export_count; ++i) {
		const struct prototype_semantic_constructor_export* value =
			&module->interface.constructor_exports[i];
		WU(value->type_export); WI(value->name_symbol_id); WU(value->ordinal);
		WU(value->constructor_declaration);
	}
	WC(module->interface.dependency_count);
	for (size_t i = 0; i < module->interface.dependency_count; ++i) {
		WI(module->interface.dependencies[i].namespace_symbol_id);
		WI(module->interface.dependencies[i].name_symbol_id);
	}
	WC(module->interface.function_graph_association_count);
	for (size_t i = 0;
		i < module->interface.function_graph_association_count; ++i) {
		const struct prototype_semantic_function_graph_association* value =
			&module->interface.function_graph_associations[i];
		WU(value->owner_term_export); WU(value->graph_type_export);
		WU(value->result_type_export); WU(value->graph_interface_term_export);
		WU(value->certified_adapter_term_export);
		WU(value->certified_runner_term_export);
		WU(value->certified_argument_index); WU(value->first_selector_group);
		WU(value->selector_group_count);
	}
	WC(module->interface.function_graph_selector_group_count);
	for (size_t i = 0;
		i < module->interface.function_graph_selector_group_count; ++i) {
		const struct prototype_semantic_function_graph_selector_group* value =
			&module->interface.function_graph_selector_groups[i];
		WU(value->association); WU(value->constructor_ordinal);
		WI(value->display_symbol_id); WU(value->role_mask);
		WU(value->value_field_ordinal); WU(value->graph_field_ordinal);
		WI(value->recursive);
	}
	WU(module->selected_entry_term);
	WU(module->selected_entry_classifier);
	WU(module->selected_entry_occurrence);
	if (checked_wire_u64(buffer, module->required_runtime_capabilities) != 0) {
		return -1;
	}
#undef WU
#undef WI
#undef WC
	return 0;
}

static int checked_wire_contracts(
	struct checked_wire_buffer* buffer,
	const struct prototype_semantic_contract_graph_view* contracts
) {
	if (!buffer || !contracts ||
		checked_wire_count(buffer, contracts->contract_count) != 0) {
		return -1;
	}
#define WU(value) if (checked_wire_u32(buffer, (uint32_t)(value)) != 0) return -1
#define WI(value) if (checked_wire_i32(buffer, (int)(value)) != 0) return -1
	for (size_t i = 0; i < contracts->contract_count; ++i) {
		const struct prototype_semantic_contract* value = &contracts->contracts[i];
		WI(value->kind); WU(value->occurrence); WU(value->core_term);
		WU(value->computation_occurrence); WU(value->continuation_occurrence);
		WU(value->continuation_binding_id); WU(value->input_classifier);
		WU(value->classifier_family); WU(value->effect_row);
		WI(value->effect_constraint_kind); WI(value->normalization_profile);
		WU(value->schema_version);
	}
	if (checked_wire_count(buffer, contracts->dependency_count) != 0) {
		return -1;
	}
	for (size_t i = 0; i < contracts->dependency_count; ++i) {
		WU(contracts->dependencies[i].occurrence);
		WU(contracts->dependencies[i].contract_id);
	}
#undef WU
#undef WI
	return 0;
}

static int checked_read_u32(
	struct checked_wire_reader* reader,
	uint32_t* p_value
) {
	unsigned char bytes[4];
	if (!p_value || checked_read_bytes(reader, bytes, sizeof(bytes)) != 0) {
		return -1;
	}
	*p_value = 0;
	for (uint32_t i = 0; i < 4; ++i) {
		*p_value |= (uint32_t)bytes[i] << (i * 8);
	}
	return 0;
}

static int checked_read_i32(struct checked_wire_reader* reader, int* p_value) {
	uint32_t value;
	if (!p_value || checked_read_u32(reader, &value) != 0) {
		return -1;
	}
	*p_value = (int32_t)value;
	return 0;
}

static int checked_read_u64(
	struct checked_wire_reader* reader,
	uint64_t* p_value
) {
	unsigned char bytes[8];
	if (!p_value || checked_read_bytes(reader, bytes, sizeof(bytes)) != 0) {
		return -1;
	}
	*p_value = 0;
	for (uint32_t i = 0; i < 8; ++i) {
		*p_value |= (uint64_t)bytes[i] << (i * 8);
	}
	return 0;
}

static int checked_read_i64(
	struct checked_wire_reader* reader,
	int64_t* p_value
) {
	uint64_t value;
	if (!p_value || checked_read_u64(reader, &value) != 0) {
		return -1;
	}
	*p_value = (int64_t)value;
	return 0;
}

static int checked_read_count(
	struct checked_wire_reader* reader,
	size_t* p_count
) {
	uint32_t count;
	if (!p_count || checked_read_u32(reader, &count) != 0 ||
		count > CHECKED_WIRE_MAX_COUNT) {
		return -1;
	}
	*p_count = count;
	return 0;
}

static int checked_read_string(
	struct checked_wire_reader* reader,
	char** p_string
) {
	size_t length;
	if (!p_string || checked_read_count(reader, &length) != 0 ||
		length == SIZE_MAX) {
		return -1;
	}
	char* string = malloc(length + 1);
	if (!string || checked_read_bytes(reader, string, length) != 0) {
		free(string);
		return -1;
	}
	string[length] = '\0';
	*p_string = string;
	return 0;
}

static uint64_t checked_wire_hash(const unsigned char* bytes, size_t count) {
	uint64_t hash = CHECKED_WIRE_FNV_OFFSET;
	for (size_t i = 0; i < count; ++i) {
		hash ^= bytes[i];
		hash *= CHECKED_WIRE_FNV_PRIME;
	}
	return hash;
}

static int checked_wire_name(
	struct checked_wire_buffer* buffer,
	struct prototype_qualified_name name
) {
	return checked_wire_i32(buffer, name.namespace_symbol_id) == 0 &&
		checked_wire_i32(buffer, name.name_symbol_id) == 0 ? 0 : -1;
}

static int checked_read_name(
	struct checked_wire_reader* reader,
	struct prototype_qualified_name* name
) {
	return name && checked_read_i32(reader, &name->namespace_symbol_id) == 0 &&
		checked_read_i32(reader, &name->name_symbol_id) == 0 ? 0 : -1;
}

static int checked_wire_term(
	struct checked_wire_buffer* buffer,
	const struct prototype_term* term
) {
	if (!term || checked_wire_i32(buffer, term->tag) != 0) {
		return -1;
	}
#define TERM_U32(value) if (checked_wire_u32(buffer, (value)) != 0) return -1
#define TERM_I32(value) if (checked_wire_i32(buffer, (value)) != 0) return -1
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			TERM_U32(term->as.var.binding_id);
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			TERM_U32(term->as.constructor.owner);
			TERM_U32(term->as.constructor.constructor_id);
			break;
		case PROTOTYPE_TERM_APP:
			TERM_U32(term->as.app.function);
			TERM_U32(term->as.app.argument);
			break;
		case PROTOTYPE_TERM_LAMBDA:
			TERM_U32(term->as.lambda.binding_id);
			TERM_U32(term->as.lambda.body);
			break;
		case PROTOTYPE_TERM_PI:
			TERM_U32(term->as.pi.domain);
			TERM_U32(term->as.pi.codomain_family);
			break;
		case PROTOTYPE_TERM_MATCH:
			TERM_U32(term->as.match.scrutinee);
			TERM_U32(term->as.match.first_case);
			TERM_U32(term->as.match.case_count);
			TERM_U32(term->as.match.ih_scope_id);
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			TERM_U32(term->as.type_former.declaration_type_id);
			TERM_U32(term->as.type_former.representation_id);
			TERM_U32(term->as.type_former.constructor_count);
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			TERM_U32(term->as.type_declaration.type_id);
			return checked_wire_name(buffer, term->as.type_declaration.identity);
		case PROTOTYPE_TERM_TYPE_VIEW:
			TERM_U32(term->as.type_view.view_type_id);
			if (checked_wire_name(buffer, term->as.type_view.identity) != 0) return -1;
			TERM_U32(term->as.type_view.core);
			TERM_U32(term->as.type_view.source);
			break;
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			TERM_U32(term->as.induction_hypothesis.ih_scope_id);
			TERM_U32(term->as.induction_hypothesis.argument);
			break;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			TERM_U32(term->as.universe_var.level_var);
			break;
		case PROTOTYPE_TERM_TEXT_LITERAL:
			TERM_I32(term->as.text_literal.text_symbol_id);
			break;
		case PROTOTYPE_TERM_INT_LITERAL:
			return checked_wire_i64(buffer, term->as.int_literal.value);
		case PROTOTYPE_TERM_EXTERNAL_REF:
			return checked_wire_name(buffer, term->as.external_ref.name);
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			TERM_I32(term->as.pure_primitive.primitive_id);
			TERM_I32(term->as.pure_primitive.type_symbol_id);
			break;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			TERM_I32(term->as.effect_operation.operation_id);
			TERM_U32(term->as.effect_operation.classifier);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			TERM_U32(term->as.effect_row_var.binding_id);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			TERM_U32(term->as.effect_row_union.left);
			TERM_U32(term->as.effect_row_union.right);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			TERM_U32(term->as.effect_row_forall.binding_id);
			TERM_U32(term->as.effect_row_forall.body);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			TERM_I32(term->as.effect_row_operation.operation_id);
			TERM_U32(term->as.effect_row_operation.latent_row);
			break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			TERM_U32(term->as.computation_type.label);
			TERM_U32(term->as.computation_type.result);
			TERM_I32(term->as.computation_type.totality);
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			TERM_U32(term->as.thunk_type.computation);
			break;
		case PROTOTYPE_TERM_RETURN:
			TERM_U32(term->as.return_term.value);
			break;
		case PROTOTYPE_TERM_THUNK:
			TERM_U32(term->as.thunk.computation);
			break;
		case PROTOTYPE_TERM_FORCE:
			TERM_U32(term->as.force.value);
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			TERM_U32(term->as.operation_request.operation);
			TERM_U32(term->as.operation_request.argument);
			TERM_U32(term->as.operation_request.continuation);
			break;
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			TERM_U32(term->as.computation_fold.computation);
			TERM_U32(term->as.computation_fold.return_clause);
			TERM_U32(term->as.computation_fold.first_clause);
			TERM_U32(term->as.computation_fold.clause_count);
			break;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			TERM_U32(term->as.dimension_action.source);
			TERM_U32(term->as.dimension_action.operator_id);
			break;
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			break;
		default:
			return -1;
	}
#undef TERM_U32
#undef TERM_I32
	return 0;
}

static int checked_read_term(
	struct checked_wire_reader* reader,
	struct prototype_term* term
) {
	if (!term) {
		return -1;
	}
	memset(term, 0, sizeof(*term));
	if (checked_read_i32(reader, &term->tag) != 0) {
		return -1;
	}
#define READ_TERM_U32(field) if (checked_read_u32(reader, &(field)) != 0) return -1
#define READ_TERM_I32(field) if (checked_read_i32(reader, &(field)) != 0) return -1
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			READ_TERM_U32(term->as.var.binding_id);
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			READ_TERM_U32(term->as.constructor.owner);
			READ_TERM_U32(term->as.constructor.constructor_id);
			break;
		case PROTOTYPE_TERM_APP:
			READ_TERM_U32(term->as.app.function);
			READ_TERM_U32(term->as.app.argument);
			break;
		case PROTOTYPE_TERM_LAMBDA:
			READ_TERM_U32(term->as.lambda.binding_id);
			READ_TERM_U32(term->as.lambda.body);
			break;
		case PROTOTYPE_TERM_PI:
			READ_TERM_U32(term->as.pi.domain);
			READ_TERM_U32(term->as.pi.codomain_family);
			break;
		case PROTOTYPE_TERM_MATCH:
			READ_TERM_U32(term->as.match.scrutinee);
			READ_TERM_U32(term->as.match.first_case);
			READ_TERM_U32(term->as.match.case_count);
			READ_TERM_U32(term->as.match.ih_scope_id);
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			READ_TERM_U32(term->as.type_former.declaration_type_id);
			READ_TERM_U32(term->as.type_former.representation_id);
			READ_TERM_U32(term->as.type_former.constructor_count);
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			READ_TERM_U32(term->as.type_declaration.type_id);
			return checked_read_name(reader, &term->as.type_declaration.identity);
		case PROTOTYPE_TERM_TYPE_VIEW:
			READ_TERM_U32(term->as.type_view.view_type_id);
			if (checked_read_name(reader, &term->as.type_view.identity) != 0) return -1;
			READ_TERM_U32(term->as.type_view.core);
			READ_TERM_U32(term->as.type_view.source);
			break;
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			READ_TERM_U32(term->as.induction_hypothesis.ih_scope_id);
			READ_TERM_U32(term->as.induction_hypothesis.argument);
			break;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			READ_TERM_U32(term->as.universe_var.level_var);
			break;
		case PROTOTYPE_TERM_TEXT_LITERAL:
			READ_TERM_I32(term->as.text_literal.text_symbol_id);
			break;
		case PROTOTYPE_TERM_INT_LITERAL:
			return checked_read_i64(reader, &term->as.int_literal.value);
		case PROTOTYPE_TERM_EXTERNAL_REF:
			return checked_read_name(reader, &term->as.external_ref.name);
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			READ_TERM_I32(term->as.pure_primitive.primitive_id);
			READ_TERM_I32(term->as.pure_primitive.type_symbol_id);
			break;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			READ_TERM_I32(term->as.effect_operation.operation_id);
			READ_TERM_U32(term->as.effect_operation.classifier);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			READ_TERM_U32(term->as.effect_row_var.binding_id);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			READ_TERM_U32(term->as.effect_row_union.left);
			READ_TERM_U32(term->as.effect_row_union.right);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			READ_TERM_U32(term->as.effect_row_forall.binding_id);
			READ_TERM_U32(term->as.effect_row_forall.body);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			READ_TERM_I32(term->as.effect_row_operation.operation_id);
			READ_TERM_U32(term->as.effect_row_operation.latent_row);
			break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			READ_TERM_U32(term->as.computation_type.label);
			READ_TERM_U32(term->as.computation_type.result);
			READ_TERM_I32(term->as.computation_type.totality);
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			READ_TERM_U32(term->as.thunk_type.computation);
			break;
		case PROTOTYPE_TERM_RETURN:
			READ_TERM_U32(term->as.return_term.value);
			break;
		case PROTOTYPE_TERM_THUNK:
			READ_TERM_U32(term->as.thunk.computation);
			break;
		case PROTOTYPE_TERM_FORCE:
			READ_TERM_U32(term->as.force.value);
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			READ_TERM_U32(term->as.operation_request.operation);
			READ_TERM_U32(term->as.operation_request.argument);
			READ_TERM_U32(term->as.operation_request.continuation);
			break;
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			READ_TERM_U32(term->as.computation_fold.computation);
			READ_TERM_U32(term->as.computation_fold.return_clause);
			READ_TERM_U32(term->as.computation_fold.first_clause);
			READ_TERM_U32(term->as.computation_fold.clause_count);
			break;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			READ_TERM_U32(term->as.dimension_action.source);
			READ_TERM_U32(term->as.dimension_action.operator_id);
			break;
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			break;
		default:
			return -1;
	}
#undef READ_TERM_U32
#undef READ_TERM_I32
	return 0;
}

static int checked_read_allocate(
	struct checked_wire_reader* reader,
	size_t item_size,
	size_t* p_count,
	void** p_items
) {
	size_t count;
	if (!p_count || !p_items || item_size == 0 ||
		checked_read_count(reader, &count) != 0 ||
		count > SIZE_MAX / item_size) {
		return -1;
	}
	void* items = count == 0 ? NULL : calloc(count, item_size);
	if (count != 0 && !items) {
		return -1;
	}
	*p_count = count;
	*p_items = items;
	return 0;
}

static void checked_module_connect_views(
	struct prototype_elaborated_module* module
) {
	module->view.symbols.strings = (const char* const*)module->symbols;
	module->view.terms.terms = module->terms;
	module->view.terms.cases = module->term_cases;
	module->view.terms.case_binders = module->case_binders;
	module->view.terms.ih_scopes = module->ih_scopes;
	module->view.terms.computation_fold_clauses =
		module->computation_fold_clauses;
	module->view.intrinsic_environment.pure_primitives = module->pure_primitives;
	module->view.intrinsic_environment.effect_operations =
		module->effect_operations;
	module->view.contexts.contexts = module->contexts;
	module->view.substitutions.substitutions = module->substitutions;
	module->view.universes.levels = module->universe_levels;
	module->view.type_schema.type_declarations = module->type_declarations;
	module->view.type_schema.constructor_declarations =
		module->constructor_declarations;
	module->view.dimensions.operators = module->dimension_operators;
	module->view.dimensions.images = module->dimension_images;
	module->view.occurrences.occurrences = module->occurrences;
	module->view.occurrences.edges = module->occurrence_edges;
	module->view.occurrences.cases = module->match_cases;
	module->view.occurrences.fold_clauses = module->fold_clauses;
	module->view.contracts.contracts = module->contracts;
	module->view.contracts.dependencies = module->contract_dependencies;
	module->view.interface.term_exports = module->term_exports;
	module->view.interface.type_exports = module->type_exports;
	module->view.interface.constructor_exports = module->constructor_exports;
	module->view.interface.dependencies = module->dependencies;
	module->view.interface.function_graph_associations =
		module->function_graph_associations;
	module->view.interface.function_graph_selector_groups =
		module->function_graph_selector_groups;
}

static int checked_read_semantic(
	struct checked_wire_reader* reader,
	struct prototype_elaborated_module* module
) {
	if (!reader || !module ||
		checked_read_u64(reader, &module->view.calculus_fingerprint) != 0 ||
		checked_read_u64(reader, &module->view.intrinsic_fingerprint) != 0) {
		return -1;
	}
#define RU(field) if (checked_read_u32(reader, &(field)) != 0) return -1
#define RI(field) if (checked_read_i32(reader, &(field)) != 0) return -1
#define RA(field, count_field, type) \
	if (checked_read_allocate( \
		reader, sizeof(type), &(count_field), (void**)&(field) \
	) != 0) return -1
	RA(
		module->pure_primitives,
		module->view.intrinsic_environment.pure_primitive_count,
		struct prototype_pure_primitive_declaration
	);
	for (size_t i = 0;
		i < module->view.intrinsic_environment.pure_primitive_count; ++i) {
		struct prototype_pure_primitive_declaration* value =
			&module->pure_primitives[i];
		RI(value->primitive_id); RU(value->arity);
		for (size_t j = 0; j < PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY; ++j) {
			RI(value->argument_types[j]);
		}
		RI(value->result_type);
	}
	RA(
		module->effect_operations,
		module->view.intrinsic_environment.effect_operation_count,
		struct prototype_effect_operation_declaration
	);
	for (size_t i = 0;
		i < module->view.intrinsic_environment.effect_operation_count; ++i) {
		struct prototype_effect_operation_declaration* value =
			&module->effect_operations[i];
		RI(value->operation_id); RI(value->classifier_schema);
		RU(value->required_host_effects); RU(value->arity);
		RI(value->inner_policy); RI(value->resumption_multiplicity);
	}
	RI(module->view.intrinsic_environment.default_integer_host_type);

	RA(module->symbols, module->view.symbols.count, char*);
	for (size_t i = 0; i < module->view.symbols.count; ++i) {
		if (checked_read_string(reader, &module->symbols[i]) != 0) return -1;
	}
	RA(module->terms, module->view.terms.term_count, struct prototype_term);
	for (size_t i = 0; i < module->view.terms.term_count; ++i) {
		if (checked_read_term(reader, &module->terms[i]) != 0) return -1;
	}
	RA(
		module->term_cases, module->view.terms.case_count,
		struct prototype_match_case
	);
	for (size_t i = 0; i < module->view.terms.case_count; ++i) {
		struct prototype_match_case* value = &module->term_cases[i];
		RU(value->constructor_owner); RU(value->constructor_id);
		RU(value->first_binder); RU(value->binder_count); RU(value->body);
	}
	RA(
		module->case_binders, module->view.terms.case_binder_count,
		struct prototype_case_binder
	);
	for (size_t i = 0; i < module->view.terms.case_binder_count; ++i) {
		RU(module->case_binders[i].binding_id);
		RI(module->case_binders[i].is_recursive);
	}
	RA(
		module->ih_scopes, module->view.terms.ih_scope_count,
		struct prototype_semantic_ih_scope
	);
	for (size_t i = 0; i < module->view.terms.ih_scope_count; ++i) {
		RU(module->ih_scopes[i].match_term);
		RU(module->ih_scopes[i].scrutinee_binding_id);
	}
	RA(
		module->computation_fold_clauses,
		module->view.terms.computation_fold_clause_count,
		struct prototype_computation_fold_clause
	);
	for (size_t i = 0;
		i < module->view.terms.computation_fold_clause_count; ++i) {
		RU(module->computation_fold_clauses[i].operation);
		RU(module->computation_fold_clauses[i].body);
	}

	RA(
		module->contexts, module->view.contexts.context_count,
		struct prototype_semantic_context
	);
	for (size_t i = 0; i < module->view.contexts.context_count; ++i) {
		struct prototype_semantic_context* value = &module->contexts[i];
		RU(value->parent); RU(value->binding_id); RU(value->classifier);
		RI(value->extension_kind); RU(value->producer_computation);
	}
	RA(
		module->substitutions, module->view.substitutions.substitution_count,
		struct prototype_semantic_substitution
	);
	for (size_t i = 0; i < module->view.substitutions.substitution_count; ++i) {
		struct prototype_semantic_substitution* value = &module->substitutions[i];
		RI(value->kind); RU(value->source_context); RU(value->target_context);
		RU(value->first); RU(value->second); RU(value->term);
		RU(value->term_classifier);
	}
	RA(
		module->universe_levels, module->view.universes.level_count,
		struct prototype_semantic_universe_level
	);
	for (size_t i = 0; i < module->view.universes.level_count; ++i) {
		RU(module->universe_levels[i].level_var);
		RI(module->universe_levels[i].value);
	}
	RA(
		module->type_declarations, module->view.type_schema.type_count,
		struct prototype_semantic_type_declaration
	);
	for (size_t i = 0; i < module->view.type_schema.type_count; ++i) {
		struct prototype_semantic_type_declaration* value =
			&module->type_declarations[i];
		RI(value->name_symbol_id); RI(value->namespace_symbol_id);
		RU(value->type_index); RU(value->representation_id);
		RU(value->formation_classifier); RU(value->parameter_context);
		RU(value->parameter_count); RU(value->index_context);
		RU(value->index_count); RU(value->first_constructor);
		RU(value->constructor_count);
	}
	RA(
		module->constructor_declarations,
		module->view.type_schema.constructor_count,
		struct prototype_semantic_type_constructor
	);
	for (size_t i = 0; i < module->view.type_schema.constructor_count; ++i) {
		struct prototype_semantic_type_constructor* value =
			&module->constructor_declarations[i];
		RI(value->name_symbol_id); RU(value->owner_type);
		RU(value->constructor_index); RU(value->parameter_context);
		RU(value->field_context); RU(value->result_classifier);
	}
	RA(
		module->dimension_operators, module->view.dimensions.operator_count,
		struct prototype_semantic_dimension_operator
	);
	for (size_t i = 0; i < module->view.dimensions.operator_count; ++i) {
		struct prototype_semantic_dimension_operator* value =
			&module->dimension_operators[i];
		RU(value->source_dimension); RU(value->target_dimension);
		if (checked_read_count(reader, &value->image_offset) != 0 ||
			checked_read_count(reader, &value->image_count) != 0) return -1;
	}
	RA(
		module->dimension_images, module->view.dimensions.image_count,
		struct prototype_dimension_axis_image
	);
	for (size_t i = 0; i < module->view.dimensions.image_count; ++i) {
		RI(module->dimension_images[i].kind);
		RU(module->dimension_images[i].target_axis);
	}

	RA(
		module->occurrences, module->view.occurrences.occurrence_count,
		struct prototype_semantic_occurrence
	);
	for (size_t i = 0; i < module->view.occurrences.occurrence_count; ++i) {
		struct prototype_semantic_occurrence* value = &module->occurrences[i];
		RI(value->kind); RI(value->category); RI(value->computation_kind);
		RI(value->application_role); RI(value->classifier_evidence_kind);
		RU(value->context_id); RU(value->context_action_substitution);
		RU(value->origin_core_term); RU(value->origin_classifier);
		RU(value->core_term); RU(value->asserted_classifier);
		RU(value->conditional_contract); RU(value->binding_id);
		RU(value->first_edge); RU(value->edge_count);
		RU(value->wrapped_occurrence); RU(value->binder_classifier);
		RU(value->match_motive); RU(value->ih_owner_occurrence);
		RU(value->ih_scope_id); RU(value->ih_case_index);
		RU(value->ih_field_index); RU(value->fold_return_binding_id);
		for (size_t j = 0; j < 16; ++j) {
			RU(value->implicit_effect_row_binders[j]);
		}
		RU(value->implicit_effect_row_count); RU(value->first_case);
		RU(value->case_count); RU(value->first_fold_clause);
		RU(value->fold_clause_count);
	}
	RA(
		module->occurrence_edges, module->view.occurrences.edge_count,
		struct prototype_semantic_occurrence_edge
	);
	for (size_t i = 0; i < module->view.occurrences.edge_count; ++i) {
		RI(module->occurrence_edges[i].role);
		RU(module->occurrence_edges[i].ordinal);
		RU(module->occurrence_edges[i].child_occurrence);
	}
	RA(
		module->match_cases, module->view.occurrences.case_count,
		struct prototype_semantic_match_case
	);
	for (size_t i = 0; i < module->view.occurrences.case_count; ++i) {
		struct prototype_semantic_match_case* value = &module->match_cases[i];
		RU(value->context_id); RI(value->refinement_kind);
		RU(value->refinement_substitution); RU(value->constructor_owner);
		RU(value->constructor_id); RU(value->binder_count);
		for (size_t j = 0; j < PROTOTYPE_SEMANTIC_MATCH_BINDER_CAPACITY; ++j) {
			RU(value->binder_ids[j]);
		}
	}
	RA(
		module->fold_clauses, module->view.occurrences.fold_clause_count,
		struct prototype_semantic_fold_clause
	);
	for (size_t i = 0; i < module->view.occurrences.fold_clause_count; ++i) {
		RU(module->fold_clauses[i].context_id);
		RU(module->fold_clauses[i].argument_binding_id);
		RU(module->fold_clauses[i].continuation_binding_id);
	}

	RA(
		module->term_exports, module->view.interface.term_export_count,
		struct prototype_semantic_term_export
	);
	for (size_t i = 0; i < module->view.interface.term_export_count; ++i) {
		struct prototype_semantic_term_export* value = &module->term_exports[i];
		RI(value->namespace_symbol_id); RI(value->name_symbol_id);
		RU(value->occurrence); RU(value->term); RU(value->classifier);
		RI(value->transparency);
	}
	RA(
		module->type_exports, module->view.interface.type_export_count,
		struct prototype_semantic_type_export
	);
	for (size_t i = 0; i < module->view.interface.type_export_count; ++i) {
		struct prototype_semantic_type_export* value = &module->type_exports[i];
		RI(value->namespace_symbol_id); RI(value->name_symbol_id);
		RU(value->type_declaration); RU(value->first_constructor);
		RU(value->constructor_count);
	}
	RA(
		module->constructor_exports,
		module->view.interface.constructor_export_count,
		struct prototype_semantic_constructor_export
	);
	for (size_t i = 0; i < module->view.interface.constructor_export_count; ++i) {
		struct prototype_semantic_constructor_export* value =
			&module->constructor_exports[i];
		RU(value->type_export); RI(value->name_symbol_id); RU(value->ordinal);
		RU(value->constructor_declaration);
	}
	RA(
		module->dependencies, module->view.interface.dependency_count,
		struct prototype_semantic_dependency
	);
	for (size_t i = 0; i < module->view.interface.dependency_count; ++i) {
		RI(module->dependencies[i].namespace_symbol_id);
		RI(module->dependencies[i].name_symbol_id);
	}
	RA(
		module->function_graph_associations,
		module->view.interface.function_graph_association_count,
		struct prototype_semantic_function_graph_association
	);
	for (size_t i = 0;
		i < module->view.interface.function_graph_association_count; ++i) {
		struct prototype_semantic_function_graph_association* value =
			&module->function_graph_associations[i];
		RU(value->owner_term_export); RU(value->graph_type_export);
		RU(value->result_type_export); RU(value->graph_interface_term_export);
		RU(value->certified_adapter_term_export);
		RU(value->certified_runner_term_export);
		RU(value->certified_argument_index); RU(value->first_selector_group);
		RU(value->selector_group_count);
	}
	RA(
		module->function_graph_selector_groups,
		module->view.interface.function_graph_selector_group_count,
		struct prototype_semantic_function_graph_selector_group
	);
	for (size_t i = 0;
		i < module->view.interface.function_graph_selector_group_count; ++i) {
		struct prototype_semantic_function_graph_selector_group* value =
			&module->function_graph_selector_groups[i];
		RU(value->association); RU(value->constructor_ordinal);
		RI(value->display_symbol_id); RU(value->role_mask);
		RU(value->value_field_ordinal); RU(value->graph_field_ordinal);
		RI(value->recursive);
	}
	RU(module->view.selected_entry_term);
	RU(module->view.selected_entry_classifier);
	RU(module->view.selected_entry_occurrence);
	if (checked_read_u64(
			reader, &module->view.required_runtime_capabilities
		) != 0) {
		return -1;
	}
#undef RU
#undef RI
#undef RA
	checked_module_connect_views(module);
	return 0;
}

static int checked_read_contracts(
	struct checked_wire_reader* reader,
	struct prototype_elaborated_module* module
) {
	if (!reader || !module) return -1;
#define RU(field) if (checked_read_u32(reader, &(field)) != 0) return -1
#define RI(field) if (checked_read_i32(reader, &(field)) != 0) return -1
#define RA(field, count_field, type) \
	if (checked_read_allocate( \
		reader, sizeof(type), &(count_field), (void**)&(field) \
	) != 0) return -1
	RA(
		module->contracts, module->view.contracts.contract_count,
		struct prototype_semantic_contract
	);
	for (size_t i = 0; i < module->view.contracts.contract_count; ++i) {
		struct prototype_semantic_contract* value = &module->contracts[i];
		RI(value->kind); RU(value->occurrence); RU(value->core_term);
		RU(value->computation_occurrence); RU(value->continuation_occurrence);
		RU(value->continuation_binding_id); RU(value->input_classifier);
		RU(value->classifier_family); RU(value->effect_row);
		RI(value->effect_constraint_kind); RI(value->normalization_profile);
		RU(value->schema_version);
	}
	RA(
		module->contract_dependencies,
		module->view.contracts.dependency_count,
		struct prototype_semantic_contract_dependency
	);
	for (size_t i = 0; i < module->view.contracts.dependency_count; ++i) {
		RU(module->contract_dependencies[i].occurrence);
		RU(module->contract_dependencies[i].contract_id);
	}
#undef RU
#undef RI
#undef RA
	checked_module_connect_views(module);
	return 0;
}

static int checked_wire_section(
	struct checked_wire_buffer* file,
	uint32_t kind,
	const struct checked_wire_buffer* payload
) {
	if (!file || !payload ||
		checked_wire_u32(file, kind) != 0 ||
		checked_wire_u64(file, payload->count) != 0 ||
		checked_wire_u64(
			file, checked_wire_hash(payload->bytes, payload->count)
		) != 0 ||
		checked_wire_bytes(file, payload->bytes, payload->count) != 0) {
		return -1;
	}
	return 0;
}

int prototype_checked_artifact_write_with_capsule(
	FILE* output,
	const struct prototype_checked_module* checked,
	const struct prototype_work_capsule* capsule
) {
	const struct prototype_elaborated_module_view* module =
		prototype_checked_module_elaborated_view(checked);
	if (!output || !module ||
		prototype_elaborated_module_validate_structure(module) != 0) {
		return -1;
	}
	struct checked_wire_buffer semantic = {0};
	struct checked_wire_buffer contracts = {0};
	struct checked_wire_buffer producer = {0};
	struct checked_wire_buffer file = {0};
	if (capsule) {
		FILE* capsule_stream = tmpfile();
		if (!capsule_stream || prototype_work_capsule_write(
				capsule_stream, capsule
			) != 0 || fflush(capsule_stream) != 0 ||
			fseek(capsule_stream, 0, SEEK_END) != 0) {
			if (capsule_stream) fclose(capsule_stream);
			return -1;
		}
		long capsule_size = ftell(capsule_stream);
		if (capsule_size < 0 || (uint64_t)capsule_size > SIZE_MAX ||
			fseek(capsule_stream, 0, SEEK_SET) != 0 ||
			checked_wire_reserve(&producer, (size_t)capsule_size) != 0 ||
			fread(
				producer.bytes, 1, (size_t)capsule_size, capsule_stream
			) != (size_t)capsule_size) {
			fclose(capsule_stream);
			checked_wire_buffer_destroy(&producer);
			return -1;
		}
		producer.count = (size_t)capsule_size;
		fclose(capsule_stream);
	}
	int result = checked_wire_semantic(&semantic, module) == 0 &&
		checked_wire_contracts(&contracts, &module->contracts) == 0 &&
		checked_wire_bytes(
			&file, CHECKED_WIRE_MAGIC, CHECKED_WIRE_MAGIC_SIZE
		) == 0 &&
		checked_wire_u32(&file, PROTOTYPE_CHECKED_ARTIFACT_VERSION) == 0 &&
		checked_wire_u32(
			&file, CHECKED_WIRE_SECTION_COUNT + (capsule ? 1 : 0)
		) == 0 &&
		checked_wire_section(
			&file, CHECKED_WIRE_SECTION_SEMANTIC, &semantic
		) == 0 &&
		checked_wire_section(
			&file, CHECKED_WIRE_SECTION_CONTRACTS, &contracts
		) == 0 && (!capsule || checked_wire_section(
			&file, CHECKED_WIRE_SECTION_PRODUCER, &producer
		) == 0) &&
		fwrite(file.bytes, 1, file.count, output) == file.count &&
		fflush(output) == 0 ? 0 : -1;
	checked_wire_buffer_destroy(&file);
	checked_wire_buffer_destroy(&producer);
	checked_wire_buffer_destroy(&contracts);
	checked_wire_buffer_destroy(&semantic);
	return result;
}

int prototype_checked_artifact_write(
	FILE* output,
	const struct prototype_checked_module* checked
) {
	return prototype_checked_artifact_write_with_capsule(output, checked, NULL);
}

struct checked_wire_section_data {
	unsigned char* bytes;
	size_t count;
};

static void checked_wire_section_data_destroy(
	struct checked_wire_section_data* section
) {
	if (!section) return;
	free(section->bytes);
	section->bytes = NULL;
	section->count = 0;
}

static int checked_file_bytes(FILE* input, void* bytes, size_t count) {
	return input && (count == 0 || (bytes && fread(bytes, 1, count, input) == count)) ?
		0 : -1;
}

static int checked_file_u32(FILE* input, uint32_t* p_value) {
	unsigned char bytes[4];
	if (!p_value || checked_file_bytes(input, bytes, sizeof(bytes)) != 0) {
		return -1;
	}
	*p_value = 0;
	for (uint32_t i = 0; i < 4; ++i) {
		*p_value |= (uint32_t)bytes[i] << (i * 8);
	}
	return 0;
}

static int checked_file_u64(FILE* input, uint64_t* p_value) {
	unsigned char bytes[8];
	if (!p_value || checked_file_bytes(input, bytes, sizeof(bytes)) != 0) {
		return -1;
	}
	*p_value = 0;
	for (uint32_t i = 0; i < 8; ++i) {
		*p_value |= (uint64_t)bytes[i] << (i * 8);
	}
	return 0;
}

static int checked_read_section_payload(
	FILE* input,
	uint64_t length,
	uint64_t expected_hash,
	struct checked_wire_section_data* section
) {
	if (!section || length > CHECKED_WIRE_MAX_SECTION_SIZE ||
		length > SIZE_MAX) {
		return -1;
	}
	section->count = (size_t)length;
	section->bytes = section->count == 0 ? NULL : malloc(section->count);
	if ((section->count != 0 && !section->bytes) ||
		checked_file_bytes(input, section->bytes, section->count) != 0 ||
		checked_wire_hash(section->bytes, section->count) != expected_hash) {
		checked_wire_section_data_destroy(section);
		return -1;
	}
	return 0;
}

static int checked_parse_section(
	const struct checked_wire_section_data* section,
	int (*parse)(
		struct checked_wire_reader*, struct prototype_elaborated_module*
	),
	struct prototype_elaborated_module* module
) {
	struct checked_wire_reader reader = {
		.bytes = section ? section->bytes : NULL,
		.count = section ? section->count : 0,
		.cursor = 0
	};
	return section && parse && parse(&reader, module) == 0 &&
		reader.cursor == reader.count ? 0 : -1;
}

int prototype_checked_artifact_extract_capsule(
	FILE* input,
	struct prototype_work_capsule* capsule
) {
	if (!input || !capsule) return -1;
	unsigned char magic[CHECKED_WIRE_MAGIC_SIZE];
	uint32_t version;
	uint32_t section_count;
	if (checked_file_bytes(input, magic, sizeof(magic)) != 0 ||
		memcmp(magic, CHECKED_WIRE_MAGIC, sizeof(magic)) != 0 ||
		checked_file_u32(input, &version) != 0 ||
		version != PROTOTYPE_CHECKED_ARTIFACT_VERSION ||
		checked_file_u32(input, &section_count) != 0 || section_count < 2 ||
		section_count > 4) return -1;
	unsigned seen = 0;
	struct checked_wire_section_data producer = {0};
	int result = 0;
	for (uint32_t i = 0; result == 0 && i < section_count; ++i) {
		uint32_t kind;
		uint64_t length;
		uint64_t hash;
		struct checked_wire_section_data current = {0};
		if (checked_file_u32(input, &kind) != 0 ||
			checked_file_u64(input, &length) != 0 ||
			checked_file_u64(input, &hash) != 0 ||
			kind < CHECKED_WIRE_SECTION_SEMANTIC ||
			kind > CHECKED_WIRE_SECTION_DEBUG ||
			(seen & (1u << kind)) != 0 ||
			checked_read_section_payload(input, length, hash, &current) != 0) {
			result = -1;
			break;
		}
		seen |= 1u << kind;
		if (kind == CHECKED_WIRE_SECTION_PRODUCER) producer = current;
		else checked_wire_section_data_destroy(&current);
	}
	if (result == 0 &&
		((seen & (1u << CHECKED_WIRE_SECTION_SEMANTIC)) == 0 ||
		 (seen & (1u << CHECKED_WIRE_SECTION_CONTRACTS)) == 0 ||
		 fgetc(input) != EOF)) result = -1;
	if (result == 0 &&
		(seen & (1u << CHECKED_WIRE_SECTION_PRODUCER)) == 0) result = 1;
	if (result == 0) {
		FILE* capsule_stream = tmpfile();
		if (!capsule_stream || fwrite(
				producer.bytes, 1, producer.count, capsule_stream
			) != producer.count || fflush(capsule_stream) != 0 ||
			fseek(capsule_stream, 0, SEEK_SET) != 0 ||
			prototype_work_capsule_read(capsule_stream, capsule) != 0) {
			result = -1;
		}
		if (capsule_stream) fclose(capsule_stream);
	}
	checked_wire_section_data_destroy(&producer);
	return result;
}

int prototype_checked_artifact_read(
	FILE* input,
	const struct prototype_checker_options* options,
	struct prototype_elaborated_module* module,
	struct prototype_checked_module** p_checked,
	struct prototype_checker_report* p_report
) {
	if (!input || !options || !module || !p_checked || !p_report) {
		return -1;
	}
	prototype_elaborated_module_init(module);
	*p_checked = NULL;
	*p_report = (struct prototype_checker_report) {
		.status = PROTOTYPE_CHECKER_REJECTED,
		.stop_reason = PROTOTYPE_CHECKER_STOP_MALFORMED,
		.effort_used = 0,
		.subject = PROTOTYPE_INVALID_ID
	};
	unsigned char magic[CHECKED_WIRE_MAGIC_SIZE];
	uint32_t version;
	uint32_t section_count;
	if (checked_file_bytes(input, magic, sizeof(magic)) != 0 ||
		memcmp(magic, CHECKED_WIRE_MAGIC, sizeof(magic)) != 0 ||
		checked_file_u32(input, &version) != 0 ||
		version != PROTOTYPE_CHECKED_ARTIFACT_VERSION ||
		checked_file_u32(input, &section_count) != 0 || section_count < 2 ||
		section_count > 4) {
		return -1;
	}
	struct checked_wire_section_data semantic = {0};
	struct checked_wire_section_data contracts = {0};
	unsigned seen = 0;
	int result = 0;
	for (uint32_t i = 0; result == 0 && i < section_count; ++i) {
		uint32_t kind;
		uint64_t length;
		uint64_t hash;
		struct checked_wire_section_data current = {0};
		if (checked_file_u32(input, &kind) != 0 ||
			checked_file_u64(input, &length) != 0 ||
			checked_file_u64(input, &hash) != 0 ||
			kind < CHECKED_WIRE_SECTION_SEMANTIC ||
			kind > CHECKED_WIRE_SECTION_DEBUG ||
			(seen & (1u << kind)) != 0 ||
			checked_read_section_payload(input, length, hash, &current) != 0) {
			result = -1;
			break;
		}
		seen |= 1u << kind;
		if (kind == CHECKED_WIRE_SECTION_SEMANTIC) {
			semantic = current;
		} else if (kind == CHECKED_WIRE_SECTION_CONTRACTS) {
			contracts = current;
		} else {
			checked_wire_section_data_destroy(&current);
		}
	}
	if (result == 0 &&
		((seen & (1u << CHECKED_WIRE_SECTION_SEMANTIC)) == 0 ||
		 (seen & (1u << CHECKED_WIRE_SECTION_CONTRACTS)) == 0 ||
		 fgetc(input) != EOF)) {
		result = -1;
	}
	if (result == 0 &&
		(checked_parse_section(&semantic, checked_read_semantic, module) != 0 ||
		 checked_parse_section(&contracts, checked_read_contracts, module) != 0 ||
		 prototype_elaborated_module_validate_structure(&module->view) != 0)) {
		result = -1;
	}
	checked_wire_section_data_destroy(&contracts);
	checked_wire_section_data_destroy(&semantic);
	if (result == 0) {
		result = prototype_checker_check_module(
			&module->view, options, p_checked, p_report
		);
	}
	if (result != 0) {
		prototype_checked_module_destroy(*p_checked);
		*p_checked = NULL;
		prototype_elaborated_module_destroy(module);
	}
	return result;
}
