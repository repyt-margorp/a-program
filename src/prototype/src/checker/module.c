#include "internal.h"

#include "a_program/core/term_schema.h"
#include "a_program/dimension/operator.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/graph/verification.h"
#include "a_program/kernel/universe.h"
#include "a_program/support/symbol.h"

#include <stdlib.h>
#include <string.h>

#define CHECKER_FNV_OFFSET UINT64_C(1469598103934665603)
#define CHECKER_FNV_PRIME UINT64_C(1099511628211)

static uint64_t fingerprint_u32(uint64_t hash, uint32_t value) {
	for (uint32_t shift = 0; shift < 32; shift += 8) {
		hash ^= (value >> shift) & UINT32_C(0xff);
		hash *= CHECKER_FNV_PRIME;
	}
	return hash;
}

static uint64_t prototype_checker_calculus_fingerprint(void) {
	static const char schema[] =
		"a-program-checked-core:term-v38:occurrence-v15:context-v1:interface-v2";
	uint64_t hash = CHECKER_FNV_OFFSET;
	for (size_t i = 0; i < sizeof(schema) - 1; ++i) {
		hash ^= (unsigned char)schema[i];
		hash *= CHECKER_FNV_PRIME;
	}
	return hash;
}

static uint64_t prototype_semantic_intrinsic_fingerprint(
	const struct prototype_semantic_intrinsic_environment* environment
) {
	if (!environment ||
		(environment->pure_primitive_count != 0 &&
		 !environment->pure_primitives) ||
		(environment->effect_operation_count != 0 &&
		 !environment->effect_operations)) {
		return 0;
	}
	uint64_t hash = CHECKER_FNV_OFFSET;
	hash = fingerprint_u32(hash, (uint32_t)environment->pure_primitive_count);
	for (size_t i = 0; i < environment->pure_primitive_count; ++i) {
		const struct prototype_pure_primitive_declaration* declaration =
			&environment->pure_primitives[i];
		hash = fingerprint_u32(hash, (uint32_t)declaration->primitive_id);
		hash = fingerprint_u32(hash, declaration->arity);
		for (size_t j = 0; j < PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY; ++j) {
			hash = fingerprint_u32(
				hash, (uint32_t)declaration->argument_types[j]
			);
		}
		hash = fingerprint_u32(hash, (uint32_t)declaration->result_type);
	}
	hash = fingerprint_u32(hash, (uint32_t)environment->effect_operation_count);
	for (size_t i = 0; i < environment->effect_operation_count; ++i) {
		const struct prototype_effect_operation_declaration* declaration =
			&environment->effect_operations[i];
		hash = fingerprint_u32(hash, (uint32_t)declaration->operation_id);
		hash = fingerprint_u32(hash, (uint32_t)declaration->classifier_schema);
		hash = fingerprint_u32(hash, declaration->required_host_effects);
		hash = fingerprint_u32(hash, declaration->arity);
		hash = fingerprint_u32(hash, (uint32_t)declaration->inner_policy);
		hash = fingerprint_u32(
			hash, (uint32_t)declaration->resumption_multiplicity
		);
	}
	return fingerprint_u32(
		hash, (uint32_t)environment->default_integer_host_type
	);
}

static int semantic_term_exists(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t term_id
) {
	return terms && term_id < terms->term_count && terms->terms &&
		terms->terms[term_id].tag != 0;
}

static int semantic_optional_term_exists(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t term_id
) {
	return term_id == PROTOTYPE_INVALID_ID || semantic_term_exists(terms, term_id);
}

static int semantic_constructor_owner_arity(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t owner_id,
	uint32_t* p_constructor_count
) {
	const struct prototype_term* owner = semantic_term_exists(terms, owner_id) ?
		&terms->terms[owner_id] : NULL;
	for (;;) {
		while (owner && owner->tag == PROTOTYPE_TERM_APP &&
			semantic_term_exists(terms, owner->as.app.function)) {
			owner = &terms->terms[owner->as.app.function];
		}
		if (!owner || owner->tag != PROTOTYPE_TERM_TYPE_VIEW ||
			!semantic_term_exists(terms, owner->as.type_view.core)) {
			break;
		}
		owner = &terms->terms[owner->as.type_view.core];
	}
	if (!owner || owner->tag != PROTOTYPE_TERM_TYPE_FORMER) {
		return -1;
	}
	*p_constructor_count = owner->as.type_former.constructor_count;
	return 0;
}

static int semantic_context_exists(
	const struct prototype_semantic_context_graph_view* contexts,
	uint32_t context_id
) {
	return contexts && contexts->contexts && context_id < contexts->context_count;
}

static int semantic_optional_substitution_exists(
	const struct prototype_semantic_substitution_graph_view* substitutions,
	uint32_t substitution_id
) {
	return substitution_id == PROTOTYPE_INVALID_ID ||
		(substitutions && substitutions->substitutions &&
		 substitution_id < substitutions->substitution_count);
}

static int semantic_symbol_exists(
	const struct prototype_semantic_symbol_table_view* symbols,
	int symbol_id
) {
	return symbols && symbols->strings && symbol_id >= 0 &&
		(size_t)symbol_id < symbols->count && symbols->strings[symbol_id];
}

static int semantic_universe_level_value(
	const struct prototype_semantic_universe_view* universes,
	uint32_t level_var,
	int* p_value
) {
	if (!universes || !p_value) {
		return -1;
	}
	for (size_t i = 0; i < universes->level_count; ++i) {
		if (universes->levels[i].level_var == level_var) {
			*p_value = universes->levels[i].value;
			return 0;
		}
	}
	return -1;
}

static int project_context_extension_kind(int source_kind) {
	switch (source_kind) {
		case PROTOTYPE_CONTEXT_EXTENSION_VALUE:
			return PROTOTYPE_SEMANTIC_CONTEXT_EXTENSION_VALUE;
		case PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT:
			return PROTOTYPE_SEMANTIC_CONTEXT_EXTENSION_SEQUENCE_RESULT;
		default:
			return PROTOTYPE_SEMANTIC_CONTEXT_EXTENSION_INVALID;
	}
}

static int project_substitution_kind(int source_kind) {
	switch (source_kind) {
		case PROTOTYPE_SUBSTITUTION_IDENTITY:
			return PROTOTYPE_SEMANTIC_SUBSTITUTION_IDENTITY;
		case PROTOTYPE_SUBSTITUTION_EMPTY:
			return PROTOTYPE_SEMANTIC_SUBSTITUTION_EMPTY;
		case PROTOTYPE_SUBSTITUTION_PROJECTION:
			return PROTOTYPE_SEMANTIC_SUBSTITUTION_PROJECTION;
		case PROTOTYPE_SUBSTITUTION_EXTEND:
			return PROTOTYPE_SEMANTIC_SUBSTITUTION_EXTEND;
		case PROTOTYPE_SUBSTITUTION_COMPOSE:
			return PROTOTYPE_SEMANTIC_SUBSTITUTION_COMPOSE;
		default:
			return 0;
	}
}

static int semantic_qualified_name_valid(
	const struct prototype_semantic_symbol_table_view* symbols,
	struct prototype_qualified_name name
) {
	return (name.namespace_symbol_id == PROTOTYPE_BASE_NAMESPACE_ID ||
		semantic_symbol_exists(symbols, name.namespace_symbol_id)) &&
		semantic_symbol_exists(symbols, name.name_symbol_id);
}

static int semantic_representation_constructor_exists(
	const struct prototype_semantic_type_schema_view* schema,
	uint32_t representation_id,
	uint32_t constructor_id,
	uint32_t expected_constructor_count
) {
	if (!schema || !schema->type_declarations) {
		return 0;
	}
	for (size_t i = 0; i < schema->type_count; ++i) {
		const struct prototype_semantic_type_declaration* type =
			&schema->type_declarations[i];
		if (type->representation_id == representation_id &&
			type->constructor_count == expected_constructor_count &&
			(expected_constructor_count == 0 ||
			 constructor_id < type->constructor_count)) {
			return 1;
		}
	}
	return 0;
}

static int semantic_pure_primitive_exists(
	const struct prototype_semantic_intrinsic_environment* intrinsic_environment,
	int primitive_id
) {
	if (!intrinsic_environment || !intrinsic_environment->pure_primitives) {
		return 0;
	}
	for (size_t i = 0; i < intrinsic_environment->pure_primitive_count; ++i) {
		if (intrinsic_environment->pure_primitives[i].primitive_id == primitive_id) {
			return 1;
		}
	}
	return 0;
}

static int semantic_effect_operation_exists(
	const struct prototype_semantic_intrinsic_environment* intrinsic_environment,
	int operation_id
) {
	if (!intrinsic_environment || !intrinsic_environment->effect_operations) {
		return 0;
	}
	for (size_t i = 0; i < intrinsic_environment->effect_operation_count; ++i) {
		if (intrinsic_environment->effect_operations[i].operation_id == operation_id) {
			return 1;
		}
	}
	return 0;
}

static int semantic_term_reader_read_term(
	const void* state,
	uint32_t term_id,
	const struct prototype_term** p_term
) {
	const struct prototype_semantic_term_graph_view* graph = state;
	if (!graph || !p_term || term_id >= graph->term_count) return -1;
	*p_term = &graph->terms[term_id];
	return 0;
}

static int semantic_term_reader_read_case(
	const void* state,
	uint32_t case_id,
	const struct prototype_match_case** p_case
) {
	const struct prototype_semantic_term_graph_view* graph = state;
	if (!graph || !p_case || case_id >= graph->case_count) return -1;
	*p_case = &graph->cases[case_id];
	return 0;
}

static int semantic_term_reader_read_case_binder(
	const void* state,
	uint32_t binder_id,
	const struct prototype_case_binder** p_binder
) {
	const struct prototype_semantic_term_graph_view* graph = state;
	if (!graph || !p_binder || binder_id >= graph->case_binder_count) return -1;
	*p_binder = &graph->case_binders[binder_id];
	return 0;
}

static int semantic_term_reader_read_ih_scope(
	const void* state,
	uint32_t scope_id,
	struct prototype_term_structural_ih_scope* p_scope
) {
	const struct prototype_semantic_term_graph_view* graph = state;
	if (!graph || !p_scope || scope_id >= graph->ih_scope_count) return -1;
	p_scope->match_term = graph->ih_scopes[scope_id].match_term;
	p_scope->scrutinee_binding_id =
		graph->ih_scopes[scope_id].scrutinee_binding_id;
	return 0;
}

static int semantic_term_reader_read_fold_clause(
	const void* state,
	uint32_t clause_id,
	const struct prototype_computation_fold_clause** p_clause
) {
	const struct prototype_semantic_term_graph_view* graph = state;
	if (!graph || !p_clause || clause_id >= graph->computation_fold_clause_count) {
		return -1;
	}
	*p_clause = &graph->computation_fold_clauses[clause_id];
	return 0;
}

int prototype_semantic_term_structural_reader(
	const struct prototype_semantic_term_graph_view* graph,
	struct prototype_term_structural_reader* p_reader
) {
	if (!graph || !p_reader || (graph->term_count != 0 && !graph->terms)) {
		return -1;
	}
	*p_reader = (struct prototype_term_structural_reader) {
		.state = graph,
		.term_count = graph->term_count,
		.case_count = graph->case_count,
		.case_binder_count = graph->case_binder_count,
		.ih_scope_count = graph->ih_scope_count,
		.fold_clause_count = graph->computation_fold_clause_count,
		.read_term = semantic_term_reader_read_term,
		.read_case = semantic_term_reader_read_case,
		.read_case_binder = semantic_term_reader_read_case_binder,
		.read_ih_scope = semantic_term_reader_read_ih_scope,
		.read_fold_clause = semantic_term_reader_read_fold_clause
	};
	return 0;
}

static int semantic_context_reader_read(
	const void* state,
	uint32_t context_id,
	struct prototype_context_structural_record* p_record
) {
	const struct prototype_semantic_context_graph_view* graph = state;
	if (!graph || !p_record || context_id >= graph->context_count) return -1;
	const struct prototype_semantic_context* context = &graph->contexts[context_id];
	*p_record = (struct prototype_context_structural_record) {
		.parent = context->parent,
		.binding_id = context->binding_id,
		.classifier = context->classifier,
		.extension_kind = context->extension_kind,
		.producer_computation = context->producer_computation
	};
	return 0;
}

int prototype_semantic_context_structural_reader(
	const struct prototype_semantic_context_graph_view* graph,
	struct prototype_context_structural_reader* p_reader
) {
	if (!graph || !p_reader || graph->context_count == 0 || !graph->contexts) {
		return -1;
	}
	*p_reader = (struct prototype_context_structural_reader) {
		.state = graph,
		.count = graph->context_count,
		.read = semantic_context_reader_read
	};
	return 0;
}

static int semantic_substitution_reader_read(
	const void* state,
	uint32_t substitution_id,
	struct prototype_substitution_structural_record* p_record
) {
	const struct prototype_semantic_substitution_graph_view* graph = state;
	if (!graph || !p_record || substitution_id >= graph->substitution_count) {
		return -1;
	}
	const struct prototype_semantic_substitution* substitution =
		&graph->substitutions[substitution_id];
	*p_record = (struct prototype_substitution_structural_record) {
		.kind = substitution->kind,
		.source_context = substitution->source_context,
		.target_context = substitution->target_context,
		.first = substitution->first,
		.second = substitution->second,
		.term = substitution->term,
		.term_classifier = substitution->term_classifier
	};
	return 0;
}

int prototype_semantic_substitution_structural_reader(
	const struct prototype_semantic_substitution_graph_view* graph,
	struct prototype_substitution_structural_reader* p_reader
) {
	if (!graph || !p_reader ||
		(graph->substitution_count != 0 && !graph->substitutions)) return -1;
	*p_reader = (struct prototype_substitution_structural_reader) {
		.state = graph,
		.count = graph->substitution_count,
		.read = semantic_substitution_reader_read
	};
	return 0;
}

static int semantic_term_symbol_field_valid(
	const struct prototype_semantic_symbol_table_view* symbols,
	const struct prototype_term_field_value* field
) {
	if (field->as.i32 == PROTOTYPE_BASE_NAMESPACE_ID) {
		return field->role ==
				PROTOTYPE_TERM_FIELD_ROLE_TYPE_DECLARATION_NAMESPACE ||
			field->role == PROTOTYPE_TERM_FIELD_ROLE_TYPE_VIEW_NAMESPACE ||
			field->role == PROTOTYPE_TERM_FIELD_ROLE_EXTERNAL_NAMESPACE ||
			field->role == PROTOTYPE_TERM_FIELD_ROLE_PRIMITIVE_TYPE;
	}
	return semantic_symbol_exists(symbols, field->as.i32);
}

static int semantic_term_field_structurally_valid(
	const struct prototype_elaborated_module_view* module,
	const struct prototype_term* term,
	const struct prototype_term_field_value* field
) {
	const struct prototype_semantic_term_graph_view* graph = &module->terms;
	switch (field->kind) {
		case PROTOTYPE_TERM_FIELD_TERM_REQUIRED:
			return semantic_term_exists(graph, field->as.u32);
		case PROTOTYPE_TERM_FIELD_TERM_OPTIONAL:
			return field->as.u32 == PROTOTYPE_INVALID_ID ||
				semantic_term_exists(graph, field->as.u32);
		case PROTOTYPE_TERM_FIELD_BINDING:
			return field->as.u32 != PROTOTYPE_INVALID_ID;
		case PROTOTYPE_TERM_FIELD_IH_SCOPE:
			return field->as.u32 < graph->ih_scope_count;
		case PROTOTYPE_TERM_FIELD_CASE_SLICE:
			return field->as.u32 <= graph->case_count &&
				term->as.match.case_count <= graph->case_count - field->as.u32;
		case PROTOTYPE_TERM_FIELD_FOLD_CLAUSE_SLICE:
			return field->as.u32 <= graph->computation_fold_clause_count &&
				term->as.computation_fold.clause_count <=
					graph->computation_fold_clause_count - field->as.u32;
		case PROTOTYPE_TERM_FIELD_TYPE_DECLARATION:
			return field->as.u32 < module->type_schema.type_count;
		case PROTOTYPE_TERM_FIELD_SYMBOL:
			return semantic_term_symbol_field_valid(&module->symbols, field);
		case PROTOTYPE_TERM_FIELD_OPERATION:
			return semantic_effect_operation_exists(
				&module->intrinsic_environment, field->as.i32
			);
		case PROTOTYPE_TERM_FIELD_DIMENSION_OPERATOR:
			return field->as.u32 < module->dimensions.operator_count;
		case PROTOTYPE_TERM_FIELD_REPRESENTATION:
		case PROTOTYPE_TERM_FIELD_UNIVERSE_LEVEL:
		case PROTOTYPE_TERM_FIELD_SCALAR_U32:
		case PROTOTYPE_TERM_FIELD_SCALAR_I32:
		case PROTOTYPE_TERM_FIELD_SCALAR_I64:
			return 1;
		default:
			return 0;
	}
}

static int semantic_term_graph_validate(
	const struct prototype_elaborated_module_view* module
) {
	const struct prototype_semantic_term_graph_view* graph = &module->terms;
	for (uint32_t i = 0; i < graph->case_binder_count; ++i) {
		if (graph->case_binders[i].binding_id == PROTOTYPE_INVALID_ID ||
			(graph->case_binders[i].is_recursive != 0 &&
			 graph->case_binders[i].is_recursive != 1)) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < graph->case_count; ++i) {
		const struct prototype_match_case* match_case = &graph->cases[i];
		uint32_t constructor_count = 0;
		if (match_case->first_binder > graph->case_binder_count ||
			match_case->binder_count > graph->case_binder_count -
				match_case->first_binder ||
			!semantic_term_exists(graph, match_case->body) ||
			((match_case->constructor_owner == PROTOTYPE_INVALID_ID) !=
			 (match_case->constructor_id == PROTOTYPE_INVALID_ID)) ||
			(match_case->constructor_owner != PROTOTYPE_INVALID_ID &&
			 (semantic_constructor_owner_arity(
				graph,
				match_case->constructor_owner,
				&constructor_count
			  ) != 0 || match_case->constructor_id >= constructor_count))) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < graph->ih_scope_count; ++i) {
		if (!semantic_term_exists(graph, graph->ih_scopes[i].match_term) ||
			graph->terms[graph->ih_scopes[i].match_term].tag !=
				PROTOTYPE_TERM_MATCH) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < graph->computation_fold_clause_count; ++i) {
		if (!semantic_term_exists(
				graph, graph->computation_fold_clauses[i].operation
			) || !semantic_term_exists(
				graph, graph->computation_fold_clauses[i].body
			)) {
			return -1;
		}
	}

	for (uint32_t i = 0; i < graph->term_count; ++i) {
		const struct prototype_term* term = &graph->terms[i];
		size_t field_count;
		if (prototype_term_schema_field_count(
				term->tag, &field_count
			) != 0) {
			return -1;
		}
		for (size_t j = 0; j < field_count; ++j) {
			struct prototype_term_field_value field;
			if (prototype_term_schema_field_at(term, j, &field) != 0 ||
				!semantic_term_field_structurally_valid(module, term, &field)) {
				return -1;
			}
		}
		uint32_t constructor_count = 0;
		switch (term->tag) {
			case PROTOTYPE_TERM_CONSTRUCTOR:
				if (semantic_constructor_owner_arity(
						graph,
						term->as.constructor.owner,
						&constructor_count
					) != 0 || term->as.constructor.constructor_id >=
						constructor_count) {
					return -1;
				}
				break;
			case PROTOTYPE_TERM_TYPE_FORMER:
				if (!semantic_representation_constructor_exists(
						&module->type_schema,
						term->as.type_former.representation_id,
						term->as.type_former.constructor_count == 0 ? 0 :
							term->as.type_former.constructor_count - 1,
						term->as.type_former.constructor_count
					)) {
					return -1;
				}
				break;
			case PROTOTYPE_TERM_PURE_PRIMITIVE:
				if (!semantic_pure_primitive_exists(
						&module->intrinsic_environment,
						term->as.pure_primitive.primitive_id
					)) {
					return -1;
				}
				break;
			case PROTOTYPE_TERM_COMPUTATION_TYPE:
				if (term->as.computation_type.totality !=
						PROTOTYPE_COMPUTATION_TOTALITY_TOTAL &&
					 term->as.computation_type.totality !=
						PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE) {
					return -1;
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

void prototype_elaborated_module_init(
	struct prototype_elaborated_module* module
) {
	if (module) {
		memset(module, 0, sizeof(*module));
	}
}

void prototype_elaborated_module_destroy(
	struct prototype_elaborated_module* module
) {
	if (!module) {
		return;
	}
	for (size_t i = 0; i < module->view.symbols.count; ++i) {
		free(module->symbols[i]);
	}
	free(module->symbols);
	free(module->terms);
	free(module->term_cases);
	free(module->case_binders);
	free(module->ih_scopes);
	free(module->computation_fold_clauses);
	free(module->pure_primitives);
	free(module->effect_operations);
	free(module->contexts);
	free(module->substitutions);
	free(module->universe_levels);
	free(module->type_declarations);
	free(module->constructor_declarations);
	free(module->dimension_operators);
	free(module->dimension_images);
	free(module->occurrences);
	free(module->occurrence_edges);
	free(module->match_cases);
	free(module->fold_clauses);
	free(module->contracts);
	free(module->contract_dependencies);
	free(module->term_exports);
	free(module->type_exports);
	free(module->constructor_exports);
	free(module->dependencies);
	free(module->function_graph_associations);
	free(module->function_graph_selector_groups);
	memset(module, 0, sizeof(*module));
}

static int semantic_occurrence_ranges_valid(
	const struct prototype_semantic_occurrence_graph_view* graph,
	const struct prototype_semantic_occurrence* occurrence
) {
	if (occurrence->first_edge == PROTOTYPE_INVALID_ID) {
		if (occurrence->edge_count != 0) {
			return 0;
		}
	} else if (occurrence->first_edge > graph->edge_count ||
		occurrence->edge_count > graph->edge_count - occurrence->first_edge) {
		return 0;
	}
	if (occurrence->first_case == PROTOTYPE_INVALID_ID) {
		if (occurrence->case_count != 0) {
			return 0;
		}
	} else if (occurrence->first_case > graph->case_count ||
		occurrence->case_count > graph->case_count - occurrence->first_case) {
		return 0;
	}
	if (occurrence->first_fold_clause == PROTOTYPE_INVALID_ID) {
		return occurrence->fold_clause_count == 0;
	}
	return occurrence->first_fold_clause <= graph->fold_clause_count &&
		occurrence->fold_clause_count <=
			graph->fold_clause_count - occurrence->first_fold_clause;
}

struct semantic_term_root_validation_state {
	const struct prototype_elaborated_module_view* module;
};

static int semantic_reference_valid(
	void* state,
	int target,
	int requirement,
	uint32_t reference
) {
	const struct semantic_term_root_validation_state* validation = state;
	if (!validation || !validation->module ||
		(requirement != PROTOTYPE_CHECKER_REFERENCE_REQUIRED &&
		 requirement != PROTOTYPE_CHECKER_REFERENCE_OPTIONAL)) return -1;
	if (requirement == PROTOTYPE_CHECKER_REFERENCE_OPTIONAL &&
		reference == PROTOTYPE_INVALID_ID) return 0;
	const struct prototype_elaborated_module_view* module = validation->module;
	switch (target) {
		case PROTOTYPE_CHECKER_REFERENCE_TERM:
			return semantic_term_exists(&module->terms, reference) ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_CONTEXT:
			return reference < module->contexts.context_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_SUBSTITUTION:
			return reference < module->substitutions.substitution_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_IH_SCOPE:
			return reference < module->terms.ih_scope_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE:
			return reference < module->occurrences.occurrence_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_CONTRACT:
			return reference < module->contracts.contract_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_TYPE:
			return reference < module->type_schema.type_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_CONSTRUCTOR:
			return reference < module->type_schema.constructor_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_TERM_EXPORT:
			return reference < module->interface.term_export_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_TYPE_EXPORT:
			return reference < module->interface.type_export_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_CONSTRUCTOR_EXPORT:
			return reference < module->interface.constructor_export_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_FUNCTION_GRAPH_ASSOCIATION:
			return reference <
				module->interface.function_graph_association_count ? 0 : -1;
		case PROTOTYPE_CHECKER_REFERENCE_SYMBOL:
			return (int32_t)reference == PROTOTYPE_BASE_NAMESPACE_ID ||
				semantic_symbol_exists(&module->symbols, (int32_t)reference) ? 0 : -1;
		default:
			return -1;
	}
}

int prototype_elaborated_module_validate_structure(
	const struct prototype_elaborated_module_view* module
) {
	if (!module || module->calculus_fingerprint !=
		prototype_checker_calculus_fingerprint() ||
		module->intrinsic_fingerprint !=
		prototype_semantic_intrinsic_fingerprint(
			&module->intrinsic_environment
		) ||
		(module->intrinsic_environment.pure_primitive_count != 0 &&
		 !module->intrinsic_environment.pure_primitives) ||
		(module->intrinsic_environment.effect_operation_count != 0 &&
		 !module->intrinsic_environment.effect_operations) ||
		(module->symbols.count != 0 && !module->symbols.strings) ||
		(module->terms.term_count != 0 && !module->terms.terms) ||
		(module->contexts.context_count != 0 && !module->contexts.contexts) ||
		(module->substitutions.substitution_count != 0 &&
		 !module->substitutions.substitutions) ||
		(module->universes.level_count != 0 && !module->universes.levels) ||
		(module->type_schema.type_count != 0 &&
		 !module->type_schema.type_declarations) ||
		(module->type_schema.constructor_count != 0 &&
		 !module->type_schema.constructor_declarations) ||
		(module->dimensions.operator_count != 0 && !module->dimensions.operators) ||
		(module->dimensions.image_count != 0 && !module->dimensions.images) ||
		(module->occurrences.occurrence_count != 0 &&
		 !module->occurrences.occurrences) ||
		(module->occurrences.edge_count != 0 && !module->occurrences.edges) ||
		(module->occurrences.case_count != 0 && !module->occurrences.cases) ||
		(module->occurrences.fold_clause_count != 0 &&
		 !module->occurrences.fold_clauses) ||
		(module->contracts.contract_count != 0 && !module->contracts.contracts) ||
		(module->contracts.dependency_count != 0 &&
		 !module->contracts.dependencies) ||
		(module->interface.term_export_count != 0 &&
		 !module->interface.term_exports) ||
		(module->interface.type_export_count != 0 &&
		 !module->interface.type_exports) ||
		(module->interface.constructor_export_count != 0 &&
		 !module->interface.constructor_exports) ||
		(module->interface.dependency_count != 0 &&
		 !module->interface.dependencies) ||
		(module->interface.function_graph_association_count != 0 &&
		 !module->interface.function_graph_associations) ||
		(module->interface.function_graph_selector_group_count != 0 &&
		 !module->interface.function_graph_selector_groups)) {
		return -1;
	}
	for (size_t i = 0; i < module->symbols.count; ++i) {
		if (!module->symbols.strings[i]) {
			return -1;
		}
		for (size_t j = 0; j < i; ++j) {
			if (strcmp(module->symbols.strings[i], module->symbols.strings[j]) == 0) {
				return -1;
			}
		}
	}
	if (semantic_term_graph_validate(module) != 0) {
		return -1;
	}
	struct semantic_term_root_validation_state root_validation = {
		.module = module
	};
	if (prototype_checker_visit_semantic_references(
			module, semantic_reference_valid, &root_validation
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < module->universes.level_count; ++i) {
		if (module->universes.levels[i].value < 0) {
			return -1;
		}
		for (size_t j = 0; j < i; ++j) {
			if (module->universes.levels[i].level_var ==
				module->universes.levels[j].level_var) {
				return -1;
			}
		}
	}
	for (size_t i = 0; i < module->terms.term_count; ++i) {
		int ignored_value;
		if (module->terms.terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
			semantic_universe_level_value(
				&module->universes,
				module->terms.terms[i].as.universe_var.level_var,
				&ignored_value
			) != 0) {
			return -1;
		}
	}

	struct prototype_context_structural_reader context_reader;
	struct prototype_substitution_structural_reader substitution_reader;
	if (prototype_semantic_context_structural_reader(
			&module->contexts, &context_reader
		) != 0 || prototype_semantic_substitution_structural_reader(
			&module->substitutions, &substitution_reader
		) != 0 || prototype_context_structural_validate(
			&context_reader, module->terms.term_count
		) != 0 || prototype_substitution_structural_validate(
			&substitution_reader, &context_reader, module->terms.term_count
		) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < module->type_schema.type_count; ++i) {
		const struct prototype_semantic_type_declaration* type =
			&module->type_schema.type_declarations[i];
		if (type->type_index != i ||
			!semantic_context_exists(&module->contexts, type->parameter_context) ||
			!semantic_context_exists(&module->contexts, type->index_context) ||
			!semantic_term_exists(&module->terms, type->formation_classifier) ||
			type->first_constructor > module->type_schema.constructor_count ||
			type->constructor_count > module->type_schema.constructor_count -
				type->first_constructor) {
			return -1;
		}
		for (uint32_t j = 0; j < type->constructor_count; ++j) {
			const struct prototype_semantic_type_constructor* constructor =
				&module->type_schema.constructor_declarations[
					type->first_constructor + j
				];
			if (constructor->owner_type != i ||
				constructor->constructor_index != j) {
				return -1;
			}
		}
	}
	for (uint32_t i = 0; i < module->type_schema.constructor_count; ++i) {
		const struct prototype_semantic_type_constructor* constructor =
			&module->type_schema.constructor_declarations[i];
		if (constructor->owner_type >= module->type_schema.type_count ||
			!semantic_context_exists(
				&module->contexts, constructor->parameter_context
			) || !semantic_context_exists(
				&module->contexts, constructor->field_context
			) || !semantic_term_exists(
				&module->terms, constructor->result_classifier
			)) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < module->dimensions.operator_count; ++i) {
		const struct prototype_semantic_dimension_operator* operator =
			&module->dimensions.operators[i];
		if (operator->image_offset > module->dimensions.image_count ||
			operator->image_count > module->dimensions.image_count -
				operator->image_offset || prototype_dimension_operator_validate(
				operator->source_dimension,
				operator->target_dimension,
				operator->image_count == 0 ? NULL :
					&module->dimensions.images[operator->image_offset],
				operator->image_count
			) != 0) {
			return -1;
		}
	}

	for (uint32_t i = 0; i < module->occurrences.occurrence_count; ++i) {
		const struct prototype_semantic_occurrence* occurrence =
			&module->occurrences.occurrences[i];
		if (occurrence->kind < PROTOTYPE_SEMANTIC_OCCURRENCE_ATOM ||
			occurrence->kind > PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD ||
			!semantic_context_exists(&module->contexts, occurrence->context_id) ||
			!semantic_optional_substitution_exists(
				&module->substitutions, occurrence->context_action_substitution
			) || !semantic_term_exists(&module->terms, occurrence->core_term) ||
			!semantic_optional_term_exists(
				&module->terms, occurrence->origin_core_term
			) || !semantic_optional_term_exists(
				&module->terms, occurrence->origin_classifier
			) || !semantic_optional_term_exists(
				&module->terms, occurrence->asserted_classifier
			) || !semantic_optional_term_exists(
				&module->terms, occurrence->binder_classifier
			) || !semantic_optional_term_exists(
				&module->terms, occurrence->match_motive
			) || occurrence->implicit_effect_row_count > 16 ||
			!semantic_occurrence_ranges_valid(&module->occurrences, occurrence)) {
			return -1;
		}
		if (occurrence->classifier_evidence_kind ==
				PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT) {
			if (occurrence->asserted_classifier == PROTOTYPE_INVALID_ID ||
				occurrence->conditional_contract != PROTOTYPE_INVALID_ID) {
				return -1;
			}
		} else if (occurrence->classifier_evidence_kind ==
				PROTOTYPE_SEMANTIC_CLASSIFIER_CONDITIONAL) {
			if (occurrence->conditional_contract >= module->contracts.contract_count) {
				return -1;
			}
		} else {
			return -1;
		}
		if (occurrence->wrapped_occurrence != PROTOTYPE_INVALID_ID &&
			occurrence->wrapped_occurrence >= module->occurrences.occurrence_count) {
			return -1;
		}
		if (occurrence->ih_owner_occurrence != PROTOTYPE_INVALID_ID &&
			occurrence->ih_owner_occurrence >= module->occurrences.occurrence_count) {
			return -1;
		}
		if (occurrence->ih_scope_id != PROTOTYPE_INVALID_ID &&
			occurrence->ih_scope_id >= module->terms.ih_scope_count) {
			return -1;
		}
		if ((occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH) !=
			(occurrence->match_motive != PROTOTYPE_INVALID_ID)) {
			return -1;
		}
	}

	for (uint32_t i = 0; i < module->occurrences.edge_count; ++i) {
		if (module->occurrences.edges[i].role <= PROTOTYPE_TERM_CHILD_INVALID ||
			module->occurrences.edges[i].child_occurrence >=
				module->occurrences.occurrence_count) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < module->occurrences.case_count; ++i) {
		const struct prototype_semantic_match_case* match_case =
			&module->occurrences.cases[i];
		uint32_t constructor_count = 0;
		if (!semantic_context_exists(&module->contexts, match_case->context_id) ||
			match_case->binder_count > PROTOTYPE_SEMANTIC_MATCH_BINDER_CAPACITY ||
			match_case->refinement_kind <
				PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_SOLVED ||
			match_case->refinement_kind >
				PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_CONSTANT ||
			!semantic_optional_substitution_exists(
				&module->substitutions, match_case->refinement_substitution
			) || semantic_constructor_owner_arity(
				&module->terms,
				match_case->constructor_owner,
				&constructor_count
			) != 0 || match_case->constructor_id >= constructor_count) {
			return -1;
		}
		if ((match_case->refinement_kind ==
				 PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_SOLVED) !=
			(match_case->refinement_substitution != PROTOTYPE_INVALID_ID)) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < module->occurrences.fold_clause_count; ++i) {
		if (!semantic_context_exists(
				&module->contexts, module->occurrences.fold_clauses[i].context_id
			)) {
			return -1;
		}
	}

	for (uint32_t i = 0; i < module->contracts.contract_count; ++i) {
		const struct prototype_semantic_contract* contract =
			&module->contracts.contracts[i];
		if (contract->kind <
				PROTOTYPE_SEMANTIC_CONTRACT_COMPUTATION_FOLD_RESULT ||
			contract->kind > PROTOTYPE_SEMANTIC_CONTRACT_EFFECT_ROW_EQUATION ||
			contract->occurrence >= module->occurrences.occurrence_count ||
			!semantic_term_exists(&module->terms, contract->core_term) ||
			!semantic_optional_term_exists(
				&module->terms, contract->input_classifier
			) || !semantic_optional_term_exists(
				&module->terms, contract->classifier_family
			) || !semantic_optional_term_exists(
				&module->terms, contract->effect_row
			)) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < module->contracts.dependency_count; ++i) {
		const struct prototype_semantic_contract_dependency* dependency =
			&module->contracts.dependencies[i];
		if (dependency->occurrence >= module->occurrences.occurrence_count ||
			dependency->contract_id >= module->contracts.contract_count) {
			return -1;
		}
	}
	for (size_t i = 0; i < module->interface.term_export_count; ++i) {
		const struct prototype_semantic_term_export* export =
			&module->interface.term_exports[i];
		if (!semantic_qualified_name_valid(
				&module->symbols,
				(struct prototype_qualified_name) {
					.namespace_symbol_id = export->namespace_symbol_id,
					.name_symbol_id = export->name_symbol_id
				}
			) || export->occurrence >= module->occurrences.occurrence_count ||
			!semantic_term_exists(&module->terms, export->term) ||
			!semantic_term_exists(&module->terms, export->classifier) ||
			(export->transparency != PROTOTYPE_SEMANTIC_EXPORT_OPAQUE &&
			 export->transparency != PROTOTYPE_SEMANTIC_EXPORT_TRANSPARENT)) {
			return -1;
		}
	}
	for (size_t i = 0; i < module->interface.type_export_count; ++i) {
		const struct prototype_semantic_type_export* export =
			&module->interface.type_exports[i];
		if (!semantic_qualified_name_valid(
				&module->symbols,
				(struct prototype_qualified_name) {
					.namespace_symbol_id = export->namespace_symbol_id,
					.name_symbol_id = export->name_symbol_id
				}
			) || export->type_declaration >= module->type_schema.type_count ||
			export->first_constructor > module->interface.constructor_export_count ||
			export->constructor_count >
				module->interface.constructor_export_count -
					export->first_constructor) {
			return -1;
		}
	}
	for (size_t i = 0; i < module->interface.constructor_export_count; ++i) {
		const struct prototype_semantic_constructor_export* export =
			&module->interface.constructor_exports[i];
		if (export->type_export >= module->interface.type_export_count ||
			!semantic_symbol_exists(&module->symbols, export->name_symbol_id) ||
			export->constructor_declaration >=
				module->type_schema.constructor_count) {
			return -1;
		}
	}
	for (size_t i = 0; i < module->interface.dependency_count; ++i) {
		const struct prototype_semantic_dependency* dependency =
			&module->interface.dependencies[i];
		if (!semantic_qualified_name_valid(
				&module->symbols,
				(struct prototype_qualified_name) {
					.namespace_symbol_id = dependency->namespace_symbol_id,
					.name_symbol_id = dependency->name_symbol_id
				}
			)) {
			return -1;
		}
		for (size_t j = 0; j < i; ++j) {
			if (module->interface.dependencies[j].namespace_symbol_id ==
					dependency->namespace_symbol_id &&
				module->interface.dependencies[j].name_symbol_id ==
					dependency->name_symbol_id) {
				return -1;
			}
		}
	}
	for (size_t i = 0;
		i < module->interface.function_graph_association_count;
		++i) {
		const struct prototype_semantic_function_graph_association* association =
			&module->interface.function_graph_associations[i];
		if (association->owner_term_export >=
				module->interface.term_export_count ||
			association->graph_type_export >=
				module->interface.type_export_count ||
			association->result_type_export >=
				module->interface.type_export_count ||
			association->graph_interface_term_export >=
				module->interface.term_export_count ||
			association->certified_runner_term_export >=
				module->interface.term_export_count ||
			(association->certified_adapter_term_export != PROTOTYPE_INVALID_ID &&
			 association->certified_adapter_term_export >=
				module->interface.term_export_count) ||
			association->first_selector_group >
				module->interface.function_graph_selector_group_count ||
			association->selector_group_count >
				module->interface.function_graph_selector_group_count -
					association->first_selector_group) {
			return -1;
		}
	}
	for (size_t i = 0;
		i < module->interface.function_graph_selector_group_count;
		++i) {
		const struct prototype_semantic_function_graph_selector_group* group =
			&module->interface.function_graph_selector_groups[i];
		if (group->association >=
				module->interface.function_graph_association_count ||
			!semantic_symbol_exists(&module->symbols, group->display_symbol_id) ||
			group->role_mask == 0 ||
			(group->role_mask &
				PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_VALUE) == 0 ||
			(group->role_mask &
				~(PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_VALUE |
				  PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_GRAPH |
				  PROTOTYPE_SEMANTIC_FUNCTION_GRAPH_ORIGIN_IH)) != 0 ||
			group->recursive < 0 || group->recursive > 1) {
			return -1;
		}
		const struct prototype_semantic_function_graph_association* association =
			&module->interface.function_graph_associations[group->association];
		if (i < association->first_selector_group ||
			i >= association->first_selector_group +
				association->selector_group_count) {
			return -1;
		}
	}

	if (!semantic_optional_term_exists(&module->terms, module->selected_entry_term) ||
		!semantic_optional_term_exists(
			&module->terms, module->selected_entry_classifier
		) || (module->selected_entry_occurrence != PROTOTYPE_INVALID_ID &&
		module->selected_entry_occurrence >= module->occurrences.occurrence_count)) {
		return -1;
	}
	return 0;
}

static int mark_context_path(
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	unsigned char* marked
) {
	while (context_id != PROTOTYPE_INVALID_ID) {
		if (!contexts || !contexts->contexts || context_id >= contexts->context_count) {
			return -1;
		}
		if (marked[context_id]) {
			return 0;
		}
		marked[context_id] = 1;
		if (context_id == 0) {
			return contexts->contexts[0].parent == PROTOTYPE_INVALID_ID ? 0 : -1;
		}
		if (contexts->contexts[context_id].parent >= context_id) {
			return -1;
		}
		context_id = contexts->contexts[context_id].parent;
	}
	return -1;
}

static int mark_optional_substitution(
	const struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	unsigned char* marked
) {
	if (substitution_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (!substitutions || !substitutions->substitutions ||
		substitution_id >= substitutions->substitution_count) {
		return -1;
	}
	marked[substitution_id] = 1;
	return 0;
}

static int project_structural_graphs(
	const struct prototype_type_semantic_schema_db* type_schema,
	const struct prototype_frozen_module_snapshot* snapshot,
	struct prototype_elaborated_module* module,
	uint32_t** p_context_relocation,
	uint32_t** p_substitution_relocation
) {
	unsigned char* context_marked = NULL;
	unsigned char* substitution_marked = NULL;
	uint32_t* substitution_queue = NULL;
	size_t substitution_queue_count = 0;
	uint32_t* context_relocation = NULL;
	uint32_t* substitution_relocation = NULL;
	if (!type_schema || !snapshot || !module || !p_context_relocation ||
		!p_substitution_relocation || snapshot->contexts.context_count == 0) {
		return -1;
	}
	context_marked = calloc(snapshot->contexts.context_count, 1);
	context_relocation = malloc(
		snapshot->contexts.context_count * sizeof(*context_relocation)
	);
	if (snapshot->substitutions.substitution_count != 0) {
		substitution_marked = calloc(
			snapshot->substitutions.substitution_count, 1
		);
		substitution_relocation = malloc(
			snapshot->substitutions.substitution_count *
				sizeof(*substitution_relocation)
		);
		substitution_queue = malloc(
			snapshot->substitutions.substitution_count *
				sizeof(*substitution_queue)
		);
	}
	if (!context_marked || !context_relocation ||
		(snapshot->substitutions.substitution_count != 0 &&
		 (!substitution_marked || !substitution_relocation ||
		  !substitution_queue))) {
		goto fail;
	}
	for (size_t i = 0; i < snapshot->contexts.context_count; ++i) {
		context_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < snapshot->substitutions.substitution_count; ++i) {
		substitution_relocation[i] = PROTOTYPE_INVALID_ID;
	}

	for (size_t i = 0; i < type_schema->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_schema->type_declarations[i];
		if (mark_context_path(
				&snapshot->contexts, type->parameter_context, context_marked
			) != 0 || mark_context_path(
				&snapshot->contexts, type->index_context, context_marked
			) != 0) {
			goto fail;
		}
	}
	for (size_t i = 0; i < type_schema->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_schema->constructor_declarations[i];
		if (mark_context_path(
				&snapshot->contexts, constructor->parameter_context, context_marked
			) != 0 || mark_context_path(
				&snapshot->contexts, constructor->field_context, context_marked
			) != 0) {
			goto fail;
		}
	}
	for (size_t i = 0; i < snapshot->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* occurrence =
			&snapshot->typed_occurrences.occurrences[i];
		if (mark_context_path(
				&snapshot->contexts, occurrence->context_id, context_marked
			) != 0 || mark_optional_substitution(
				&snapshot->substitutions,
				occurrence->context_action_substitution,
				substitution_marked
			) != 0) {
			goto fail;
		}
	}
	for (size_t i = 0; i < snapshot->typed_occurrences.case_count; ++i) {
		const struct prototype_typed_occurrence_match_case* match_case =
			&snapshot->typed_occurrences.cases[i];
		if (mark_context_path(
				&snapshot->contexts, match_case->context_id, context_marked
			) != 0 || mark_optional_substitution(
				&snapshot->substitutions,
				match_case->refinement_substitution,
				substitution_marked
			) != 0) {
			goto fail;
		}
	}
	for (size_t i = 0; i < snapshot->typed_occurrences.fold_clause_count; ++i) {
		if (mark_context_path(
				&snapshot->contexts,
				snapshot->typed_occurrences.fold_clauses[i].context_id,
				context_marked
			) != 0) {
			goto fail;
		}
	}

	for (uint32_t i = 0; i < snapshot->substitutions.substitution_count; ++i) {
		if (substitution_marked[i]) {
			substitution_queue[substitution_queue_count++] = i;
		}
	}
	for (size_t cursor = 0; cursor < substitution_queue_count; ++cursor) {
		uint32_t i = substitution_queue[cursor];
		const struct prototype_substitution* substitution =
			&snapshot->substitutions.substitutions[i];
			if (mark_context_path(
					&snapshot->contexts,
					substitution->source_context,
					context_marked
				) != 0 || mark_context_path(
					&snapshot->contexts,
					substitution->target_context,
					context_marked
				) != 0) {
				goto fail;
			}
			uint32_t dependencies[2] = {
				substitution->first, substitution->second
			};
			for (size_t j = 0; j < 2; ++j) {
				uint32_t dependency = dependencies[j];
				if (dependency == PROTOTYPE_INVALID_ID) {
					continue;
				}
				if (dependency >= snapshot->substitutions.substitution_count ||
					dependency >= i) {
					goto fail;
				}
				if (!substitution_marked[dependency]) {
					substitution_marked[dependency] = 1;
					substitution_queue[substitution_queue_count++] = dependency;
				}
			}
	}

	for (size_t i = 0; i < snapshot->contexts.context_count; ++i) {
		if (context_marked[i]) {
			context_relocation[i] = (uint32_t)module->view.contexts.context_count++;
		}
	}
	for (size_t i = 0; i < snapshot->substitutions.substitution_count; ++i) {
		if (substitution_marked[i]) {
			substitution_relocation[i] =
				(uint32_t)module->view.substitutions.substitution_count++;
		}
	}
	module->contexts = calloc(
		module->view.contexts.context_count, sizeof(*module->contexts)
	);
	if (module->view.substitutions.substitution_count != 0) {
		module->substitutions = calloc(
			module->view.substitutions.substitution_count,
			sizeof(*module->substitutions)
		);
	}
	module->type_declarations = calloc(
		type_schema->type_count, sizeof(*module->type_declarations)
	);
	module->constructor_declarations = calloc(
		type_schema->constructor_count, sizeof(*module->constructor_declarations)
	);
	if (!module->contexts ||
		(module->view.substitutions.substitution_count != 0 &&
		 !module->substitutions) ||
		(type_schema->type_count != 0 && !module->type_declarations) ||
		(type_schema->constructor_count != 0 &&
		 !module->constructor_declarations)) {
		goto fail;
	}
	for (uint32_t i = 0; i < snapshot->contexts.context_count; ++i) {
		if (!context_marked[i]) {
			continue;
		}
		const struct prototype_context* source = &snapshot->contexts.contexts[i];
		uint32_t classifier = source->classifier_ref.term_id;
		if ((i == 0 && source->classifier_ref.kind !=
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_INVALID) ||
			(i != 0 && source->classifier_ref.kind !=
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM &&
			 source->classifier_ref.kind !=
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL) ||
			(i != 0 && classifier == PROTOTYPE_INVALID_ID)) {
			goto fail;
		}
		module->contexts[context_relocation[i]] =
			(struct prototype_semantic_context) {
				.parent = i == 0 ? PROTOTYPE_INVALID_ID :
					context_relocation[source->parent],
				.binding_id = source->binding_id,
				.classifier = classifier,
				.extension_kind = project_context_extension_kind(
					source->extension_kind
				),
				.producer_computation = source->producer_computation
			};
	}
	for (uint32_t i = 0; i < snapshot->substitutions.substitution_count; ++i) {
		if (!substitution_marked[i]) {
			continue;
		}
		const struct prototype_substitution* source =
			&snapshot->substitutions.substitutions[i];
		module->substitutions[substitution_relocation[i]] =
			(struct prototype_semantic_substitution) {
				.kind = project_substitution_kind(source->kind),
				.source_context = context_relocation[source->source_context],
				.target_context = context_relocation[source->target_context],
				.first = source->first == PROTOTYPE_INVALID_ID ?
					PROTOTYPE_INVALID_ID : substitution_relocation[source->first],
				.second = source->second == PROTOTYPE_INVALID_ID ?
					PROTOTYPE_INVALID_ID : substitution_relocation[source->second],
				.term = source->term,
				.term_classifier = source->term_classifier
			};
	}
	for (size_t i = 0; i < type_schema->type_count; ++i) {
		const struct prototype_type_declaration* source =
			&type_schema->type_declarations[i];
		module->type_declarations[i] =
			(struct prototype_semantic_type_declaration) {
				.name_symbol_id = source->name_symbol_id,
				.namespace_symbol_id = source->namespace_symbol_id,
				.type_index = source->type_index,
				.representation_id = source->representation_id,
				.formation_classifier = source->formation_classifier,
				.parameter_context = context_relocation[source->parameter_context],
				.parameter_count = source->parameter_count,
				.index_context = context_relocation[source->index_context],
				.index_count = source->index_count,
				.first_constructor = source->first_constructor,
				.constructor_count = source->constructor_count
			};
	}
	for (size_t i = 0; i < type_schema->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* source =
			&type_schema->constructor_declarations[i];
		module->constructor_declarations[i] =
			(struct prototype_semantic_type_constructor) {
				.name_symbol_id = source->name_symbol_id,
				.owner_type = source->owner_type,
				.constructor_index = source->constructor_index,
				.parameter_context =
					context_relocation[source->parameter_context],
				.field_context = context_relocation[source->field_context],
				.result_classifier = source->result_classifier
			};
	}
	if (snapshot->dimension_operators.operator_count != 0) {
		module->dimension_operators = calloc(
			snapshot->dimension_operators.operator_count,
			sizeof(*module->dimension_operators)
		);
		if (!module->dimension_operators) {
			goto fail;
		}
	}
	if (snapshot->dimension_operators.image_count != 0) {
		if (!snapshot->dimension_operators.images) {
			goto fail;
		}
		module->dimension_images = malloc(
			snapshot->dimension_operators.image_count *
				sizeof(*module->dimension_images)
		);
		if (!module->dimension_images) {
			goto fail;
		}
		memcpy(
			module->dimension_images,
			snapshot->dimension_operators.images,
			snapshot->dimension_operators.image_count *
				sizeof(*module->dimension_images)
		);
	}
	for (size_t i = 0; i < snapshot->dimension_operators.operator_count; ++i) {
		const struct prototype_dimension_operator* source =
			&snapshot->dimension_operators.operators[i];
		module->dimension_operators[i] =
			(struct prototype_semantic_dimension_operator) {
				.source_dimension = source->source_dimension,
				.target_dimension = source->target_dimension,
				.image_offset = source->image_offset,
				.image_count = source->image_count
			};
	}
	module->view.contexts.contexts = module->contexts;
	module->view.substitutions.substitutions = module->substitutions;
	module->view.type_schema = (struct prototype_semantic_type_schema_view) {
		.type_declarations = module->type_declarations,
		.type_count = type_schema->type_count,
		.constructor_declarations = module->constructor_declarations,
		.constructor_count = type_schema->constructor_count
	};
	module->view.dimensions = (struct prototype_semantic_dimension_graph_view) {
		.operators = module->dimension_operators,
		.operator_count = snapshot->dimension_operators.operator_count,
		.images = module->dimension_images,
		.image_count = snapshot->dimension_operators.image_count
	};
	free(context_marked);
	free(substitution_marked);
	free(substitution_queue);
	*p_context_relocation = context_relocation;
	*p_substitution_relocation = substitution_relocation;
	return 0;

fail:
	free(context_marked);
	free(substitution_marked);
	free(substitution_queue);
	free(context_relocation);
	free(substitution_relocation);
	return -1;
}

static int project_term_graph(
	const struct prototype_term_db* source,
	const struct prototype_type_semantic_schema_db* type_schema,
	struct prototype_elaborated_module* module
) {
	if (!source || !type_schema || !module ||
		(source->term_count != 0 && !source->terms) ||
		(source->case_count != 0 && !source->cases) ||
		(source->case_binder_count != 0 && !source->case_binders) ||
		(source->ih_scope_count != 0 && !source->ih_scopes)) {
		return -1;
	}
	if (source->term_count != 0) {
		module->terms = malloc(source->term_count * sizeof(*module->terms));
		if (!module->terms) {
			return -1;
		}
		memcpy(
			module->terms,
			source->terms,
			source->term_count * sizeof(*module->terms)
		);
	}
	if (source->ih_scope_count != 0) {
		module->ih_scopes = calloc(
			source->ih_scope_count, sizeof(*module->ih_scopes)
		);
		if (!module->ih_scopes) {
			return -1;
		}
	}
	if (source->case_count != 0) {
		module->term_cases = malloc(
			source->case_count * sizeof(*module->term_cases)
		);
	}
	if (source->case_binder_count != 0) {
		module->case_binders = malloc(
			source->case_binder_count * sizeof(*module->case_binders)
		);
	}
	if (source->computation_fold_clause_count != 0) {
		module->computation_fold_clauses = malloc(
			source->computation_fold_clause_count *
				sizeof(*module->computation_fold_clauses)
		);
	}
	if ((source->case_count != 0 && !module->term_cases) ||
		(source->case_binder_count != 0 && !module->case_binders) ||
		(source->computation_fold_clause_count != 0 &&
		 !module->computation_fold_clauses)) {
		return -1;
	}
	if (source->case_count != 0) {
		memcpy(
			module->term_cases,
			source->cases,
			source->case_count * sizeof(*module->term_cases)
		);
	}
	if (source->case_binder_count != 0) {
		memcpy(
			module->case_binders,
			source->case_binders,
			source->case_binder_count * sizeof(*module->case_binders)
		);
	}
	if (source->computation_fold_clause_count != 0) {
		memcpy(
			module->computation_fold_clauses,
			source->computation_fold_clauses,
			source->computation_fold_clause_count *
				sizeof(*module->computation_fold_clauses)
		);
	}
	for (size_t i = 0; i < source->term_count; ++i) {
		if (module->terms[i].tag != PROTOTYPE_TERM_TYPE_FORMER) {
			continue;
		}
		uint32_t representation_id =
			module->terms[i].as.type_former.representation_id;
		uint32_t canonical_declaration = PROTOTYPE_INVALID_ID;
		for (uint32_t j = 0; j < type_schema->type_count; ++j) {
			if (type_schema->type_declarations[j].representation_id ==
				representation_id) {
				canonical_declaration = j;
				break;
			}
		}
		if (canonical_declaration == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		module->terms[i].as.type_former.declaration_type_id =
			canonical_declaration;
	}
	for (size_t i = 0; i < source->ih_scope_count; ++i) {
		module->ih_scopes[i] = (struct prototype_semantic_ih_scope) {
			.match_term = source->ih_scopes[i].match_term,
			.scrutinee_binding_id = source->ih_scopes[i].scrutinee_binding_id
		};
	}
	module->view.terms = (struct prototype_semantic_term_graph_view) {
		.terms = module->terms,
		.term_count = source->term_count,
		.cases = module->term_cases,
		.case_count = source->case_count,
		.case_binders = module->case_binders,
		.case_binder_count = source->case_binder_count,
		.ih_scopes = module->ih_scopes,
		.ih_scope_count = source->ih_scope_count,
		.computation_fold_clauses = module->computation_fold_clauses,
		.computation_fold_clause_count = source->computation_fold_clause_count
	};
	return 0;
}

static int project_intrinsic_environment(
	const struct prototype_intrinsic_environment* source,
	struct prototype_elaborated_module* module
) {
	if (!source || !module ||
		(source->pure_primitive_count != 0 && !source->pure_primitives) ||
		(source->effect_operation_count != 0 && !source->effect_operations)) {
		return -1;
	}
	if (source->pure_primitive_count != 0) {
		module->pure_primitives = malloc(
			source->pure_primitive_count * sizeof(*module->pure_primitives)
		);
	}
	if (source->effect_operation_count != 0) {
		module->effect_operations = malloc(
			source->effect_operation_count * sizeof(*module->effect_operations)
		);
	}
	if ((source->pure_primitive_count != 0 && !module->pure_primitives) ||
		(source->effect_operation_count != 0 && !module->effect_operations)) {
		return -1;
	}
	if (source->pure_primitive_count != 0) {
		memcpy(
			module->pure_primitives,
			source->pure_primitives,
			source->pure_primitive_count * sizeof(*module->pure_primitives)
		);
	}
	if (source->effect_operation_count != 0) {
		memcpy(
			module->effect_operations,
			source->effect_operations,
			source->effect_operation_count * sizeof(*module->effect_operations)
		);
	}
	module->view.intrinsic_environment =
		(struct prototype_semantic_intrinsic_environment) {
			.pure_primitives = module->pure_primitives,
			.pure_primitive_count = source->pure_primitive_count,
			.effect_operations = module->effect_operations,
			.effect_operation_count = source->effect_operation_count,
			.default_integer_host_type = source->default_integer_host_type
		};
	module->view.calculus_fingerprint =
		prototype_checker_calculus_fingerprint();
	module->view.intrinsic_fingerprint =
		prototype_semantic_intrinsic_fingerprint(
			&module->view.intrinsic_environment
		);
	return 0;
}

static int mark_semantic_symbol(
	unsigned char* marked,
	size_t symbol_count,
	int symbol_id
) {
	if (symbol_id == PROTOTYPE_BASE_NAMESPACE_ID) {
		return 0;
	}
	if (symbol_id < 0 || (size_t)symbol_id >= symbol_count) {
		return -1;
	}
	marked[symbol_id] = 1;
	return 0;
}

static int relocate_semantic_symbol(
	const uint32_t* relocation,
	size_t symbol_count,
	int* p_symbol_id
) {
	if (*p_symbol_id == PROTOTYPE_BASE_NAMESPACE_ID) {
		return 0;
	}
	if (*p_symbol_id < 0 || (size_t)*p_symbol_id >= symbol_count ||
		relocation[*p_symbol_id] == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_symbol_id = (int)relocation[*p_symbol_id];
	return 0;
}

static int project_semantic_exports(
	const struct prototype_frozen_module_snapshot* snapshot,
	struct prototype_elaborated_module* module
) {
	if ((snapshot->label_count != 0 && !snapshot->labels) ||
		(snapshot->type_export_count != 0 && !snapshot->type_exports) ||
		(snapshot->constructor_export_count != 0 &&
		 !snapshot->constructor_exports) ||
		(snapshot->function_graph_association_count != 0 &&
		 !snapshot->function_graph_associations) ||
		(snapshot->function_graph_origin_group_count != 0 &&
		 !snapshot->function_graph_origin_groups)) {
		return -1;
	}
	if (snapshot->label_count != 0) {
		module->term_exports = calloc(
			snapshot->label_count, sizeof(*module->term_exports)
		);
	}
	if (snapshot->type_export_count != 0) {
		module->type_exports = calloc(
			snapshot->type_export_count, sizeof(*module->type_exports)
		);
	}
	if (snapshot->constructor_export_count != 0) {
		module->constructor_exports = calloc(
			snapshot->constructor_export_count,
			sizeof(*module->constructor_exports)
		);
	}
	if ((snapshot->label_count != 0 && !module->term_exports) ||
		(snapshot->type_export_count != 0 && !module->type_exports) ||
		(snapshot->constructor_export_count != 0 &&
		 !module->constructor_exports)) {
		return -1;
	}
	for (size_t i = 0; i < snapshot->label_count; ++i) {
		const struct prototype_compile_label* label = &snapshot->labels[i];
		if (label->exposed_occurrence >=
			module->view.occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_semantic_occurrence* occurrence =
			&module->occurrences[label->exposed_occurrence];
		if (occurrence->classifier_evidence_kind !=
			PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT) {
			return -1;
		}
		module->term_exports[i] = (struct prototype_semantic_term_export) {
			.namespace_symbol_id = PROTOTYPE_BASE_NAMESPACE_ID,
			.name_symbol_id = label->name_symbol_id,
			.occurrence = label->exposed_occurrence,
			.term = occurrence->core_term,
			.classifier = occurrence->asserted_classifier,
			.transparency = PROTOTYPE_SEMANTIC_EXPORT_TRANSPARENT
		};
	}
	for (size_t i = 0; i < snapshot->type_export_count; ++i) {
		const struct prototype_compile_type_export* source =
			&snapshot->type_exports[i];
		uint32_t declaration_id = PROTOTYPE_INVALID_ID;
		for (uint32_t j = 0; j < module->view.type_schema.type_count; ++j) {
			if (module->type_declarations[j].type_index == source->type_id) {
				declaration_id = j;
				break;
			}
		}
		if (declaration_id == PROTOTYPE_INVALID_ID ||
			source->first_constructor_export > snapshot->constructor_export_count ||
			source->constructor_count > snapshot->constructor_export_count -
				source->first_constructor_export) {
			return -1;
		}
		const struct prototype_semantic_type_declaration* declaration =
			&module->type_declarations[declaration_id];
		module->type_exports[i] = (struct prototype_semantic_type_export) {
			.namespace_symbol_id = declaration->namespace_symbol_id,
			.name_symbol_id = declaration->name_symbol_id,
			.type_declaration = declaration_id,
			.first_constructor = source->first_constructor_export,
			.constructor_count = source->constructor_count
		};
	}
	for (size_t i = 0; i < snapshot->constructor_export_count; ++i) {
		const struct prototype_compile_constructor_export* source =
			&snapshot->constructor_exports[i];
		if (source->type_export_index >= snapshot->type_export_count) {
			return -1;
		}
		const struct prototype_semantic_type_export* type_export =
			&module->type_exports[source->type_export_index];
		const struct prototype_semantic_type_declaration* declaration =
			&module->type_declarations[type_export->type_declaration];
		if (source->ordinal >= declaration->constructor_count) {
			return -1;
		}
		uint32_t constructor_id = declaration->first_constructor + source->ordinal;
		if (constructor_id >= module->view.type_schema.constructor_count ||
			module->constructor_declarations[constructor_id].name_symbol_id !=
				source->name_symbol_id) {
			return -1;
		}
		module->constructor_exports[i] =
			(struct prototype_semantic_constructor_export) {
				.type_export = source->type_export_index,
				.name_symbol_id = source->name_symbol_id,
				.ordinal = source->ordinal,
				.constructor_declaration = constructor_id
			};
	}
	size_t association_count = 0;
	size_t selector_count = 0;
	for (size_t i = 0; i < snapshot->function_graph_association_count; ++i) {
		const struct prototype_function_graph_association* association =
			&snapshot->function_graph_associations[i];
		if (association->imported) {
			continue;
		}
		if (!association->origin_groups_staged ||
			!association->origin_groups_frozen ||
			association->first_origin_group >
				snapshot->function_graph_origin_group_count ||
			association->origin_group_count >
				snapshot->function_graph_origin_group_count -
					association->first_origin_group) {
			return -1;
		}
		association_count += 1;
		selector_count += association->origin_group_count;
	}
	if (association_count != 0) {
		module->function_graph_associations = calloc(
			association_count, sizeof(*module->function_graph_associations)
		);
	}
	if (selector_count != 0) {
		module->function_graph_selector_groups = calloc(
			selector_count, sizeof(*module->function_graph_selector_groups)
		);
	}
	if ((association_count != 0 && !module->function_graph_associations) ||
		(selector_count != 0 && !module->function_graph_selector_groups)) {
		return -1;
	}
	size_t association_cursor = 0;
	size_t selector_cursor = 0;
	for (size_t i = 0; i < snapshot->function_graph_association_count; ++i) {
		const struct prototype_function_graph_association* source =
			&snapshot->function_graph_associations[i];
		if (source->imported) {
			continue;
		}
		uint32_t owner = PROTOTYPE_INVALID_ID;
		uint32_t graph_interface = PROTOTYPE_INVALID_ID;
		uint32_t adapter = PROTOTYPE_INVALID_ID;
		uint32_t runner = PROTOTYPE_INVALID_ID;
		uint32_t graph = PROTOTYPE_INVALID_ID;
		uint32_t result = PROTOTYPE_INVALID_ID;
		for (uint32_t j = 0; j < snapshot->label_count; ++j) {
			int symbol = module->term_exports[j].name_symbol_id;
			if (symbol == source->owner_symbol_id) {
				owner = j;
			} else if (symbol == source->graph_interface_symbol_id) {
				graph_interface = j;
			} else if (source->certified_adapter_symbol_id >= 0 &&
				symbol == source->certified_adapter_symbol_id) {
				adapter = j;
			} else if (symbol == source->certified_runner_symbol_id) {
				runner = j;
			}
		}
		for (uint32_t j = 0; j < snapshot->type_export_count; ++j) {
			int symbol = module->type_exports[j].name_symbol_id;
			if (symbol == source->graph_symbol_id) {
				graph = j;
			} else if (symbol == source->result_symbol_id) {
				result = j;
			}
		}
		if (owner == PROTOTYPE_INVALID_ID || graph == PROTOTYPE_INVALID_ID ||
			result == PROTOTYPE_INVALID_ID ||
			graph_interface == PROTOTYPE_INVALID_ID ||
			runner == PROTOTYPE_INVALID_ID ||
			(source->certified_adapter_symbol_id >= 0 &&
			 adapter == PROTOTYPE_INVALID_ID)) {
			return -1;
		}
		struct prototype_semantic_function_graph_association* target =
			&module->function_graph_associations[association_cursor];
		*target = (struct prototype_semantic_function_graph_association) {
			.owner_term_export = owner,
			.graph_type_export = graph,
			.result_type_export = result,
			.graph_interface_term_export = graph_interface,
			.certified_adapter_term_export = adapter,
			.certified_runner_term_export = runner,
			.certified_argument_index = source->certified_argument_index,
			.first_selector_group = (uint32_t)selector_cursor,
			.selector_group_count = source->origin_group_count
		};
		for (uint32_t j = 0; j < source->origin_group_count; ++j) {
			const struct prototype_function_graph_origin_group* origin =
				&snapshot->function_graph_origin_groups[
					source->first_origin_group + j
				];
			if (origin->association_id != i) {
				return -1;
			}
			module->function_graph_selector_groups[selector_cursor++] =
				(struct prototype_semantic_function_graph_selector_group) {
					.association = (uint32_t)association_cursor,
					.constructor_ordinal = origin->constructor_ordinal,
					.display_symbol_id = origin->display_symbol_id,
					.role_mask = origin->role_mask,
					.value_field_ordinal = origin->value_field_ordinal,
					.graph_field_ordinal = origin->graph_field_ordinal,
					.recursive = origin->recursive
				};
		}
		association_cursor += 1;
	}
	module->view.interface = (struct prototype_semantic_interface_view) {
		.term_exports = module->term_exports,
		.term_export_count = snapshot->label_count,
		.type_exports = module->type_exports,
		.type_export_count = snapshot->type_export_count,
		.constructor_exports = module->constructor_exports,
		.constructor_export_count = snapshot->constructor_export_count,
		.function_graph_associations = module->function_graph_associations,
		.function_graph_association_count = association_count,
		.function_graph_selector_groups = module->function_graph_selector_groups,
		.function_graph_selector_group_count = selector_count
	};
	return 0;
}

static int project_semantic_symbols(
	const struct symbol_table* source,
	struct prototype_elaborated_module* module
) {
	if (!source || !module) {
		return -1;
	}
	unsigned char* marked = source->storage.count == 0 ? NULL : calloc(
		source->storage.count, 1
	);
	uint32_t* relocation = source->storage.count == 0 ? NULL : malloc(
		source->storage.count * sizeof(*relocation)
	);
	if (source->storage.count != 0 && (!marked || !relocation)) {
		free(marked);
		free(relocation);
		return -1;
	}
	for (size_t i = 0; i < source->storage.count; ++i) {
		relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < module->view.terms.term_count; ++i) {
		const struct prototype_term* term = &module->terms[i];
		size_t field_count;
		if (prototype_term_schema_field_count(
				term->tag, &field_count
			) != 0) {
			goto fail;
		}
		for (size_t j = 0; j < field_count; ++j) {
			struct prototype_term_field_value field;
			if (prototype_term_schema_field_at(term, j, &field) != 0) {
				goto fail;
			}
			if (field.kind == PROTOTYPE_TERM_FIELD_SYMBOL &&
				mark_semantic_symbol(
					marked, source->storage.count, field.as.i32
				) != 0) {
				goto fail;
			}
		}
	}
	for (size_t i = 0; i < module->view.type_schema.type_count; ++i) {
		if (mark_semantic_symbol(
				marked,
				source->storage.count,
				module->type_declarations[i].namespace_symbol_id
			) != 0 || mark_semantic_symbol(
				marked,
				source->storage.count,
				module->type_declarations[i].name_symbol_id
			) != 0) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	for (size_t i = 0; i < module->view.type_schema.constructor_count; ++i) {
		if (mark_semantic_symbol(
				marked,
				source->storage.count,
				module->constructor_declarations[i].name_symbol_id
			) != 0) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	for (size_t i = 0; i < module->view.interface.term_export_count; ++i) {
		if (mark_semantic_symbol(
				marked,
				source->storage.count,
				module->term_exports[i].namespace_symbol_id
			) != 0 || mark_semantic_symbol(
				marked,
				source->storage.count,
				module->term_exports[i].name_symbol_id
			) != 0) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	for (size_t i = 0;
		i < module->view.interface.function_graph_selector_group_count;
		++i) {
		if (mark_semantic_symbol(
				marked,
				source->storage.count,
				module->function_graph_selector_groups[i].display_symbol_id
			) != 0) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	size_t target_count = 0;
	for (size_t i = 0; i < source->storage.count; ++i) {
		if (marked[i]) {
			relocation[i] = (uint32_t)target_count++;
		}
	}
	if (target_count != 0) {
		module->symbols = calloc(target_count, sizeof(*module->symbols));
		if (!module->symbols) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	for (size_t i = 0; i < source->storage.count; ++i) {
		if (!marked[i] || !source->storage.strings[i]) {
			continue;
		}
		size_t length = strlen(source->storage.strings[i]);
		module->symbols[relocation[i]] = malloc(length + 1);
		if (!module->symbols[relocation[i]]) {
			free(marked);
			free(relocation);
			return -1;
		}
		memcpy(
			module->symbols[relocation[i]],
			source->storage.strings[i],
			length + 1
		);
	}
	for (size_t i = 0; i < module->view.terms.term_count; ++i) {
		struct prototype_term* term = &module->terms[i];
		size_t field_count;
		if (prototype_term_schema_field_count(
				term->tag, &field_count
			) != 0) {
			goto fail;
		}
		for (size_t j = 0; j < field_count; ++j) {
			struct prototype_term_field_value field;
			if (prototype_term_schema_field_at(term, j, &field) != 0) {
				goto fail;
			}
			if (field.kind != PROTOTYPE_TERM_FIELD_SYMBOL) {
				continue;
			}
			if (relocate_semantic_symbol(
					relocation, source->storage.count, &field.as.i32
				) != 0 || prototype_term_schema_field_write(
					term, j, &field
				) != 0) {
				goto fail;
			}
		}
	}
	for (size_t i = 0; i < module->view.type_schema.type_count; ++i) {
		if (relocate_semantic_symbol(
				relocation,
				source->storage.count,
				&module->type_declarations[i].namespace_symbol_id
			) != 0 || relocate_semantic_symbol(
				relocation,
				source->storage.count,
				&module->type_declarations[i].name_symbol_id
			) != 0) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	for (size_t i = 0; i < module->view.type_schema.constructor_count; ++i) {
		if (relocate_semantic_symbol(
				relocation,
				source->storage.count,
				&module->constructor_declarations[i].name_symbol_id
			) != 0) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	for (size_t i = 0; i < module->view.interface.term_export_count; ++i) {
		if (relocate_semantic_symbol(
				relocation,
				source->storage.count,
				&module->term_exports[i].namespace_symbol_id
			) != 0 || relocate_semantic_symbol(
				relocation,
				source->storage.count,
				&module->term_exports[i].name_symbol_id
			) != 0) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	for (size_t i = 0; i < module->view.interface.type_export_count; ++i) {
		if (module->type_exports[i].type_declaration >=
			module->view.type_schema.type_count) {
			free(marked);
			free(relocation);
			return -1;
		}
		const struct prototype_semantic_type_declaration* declaration =
			&module->type_declarations[module->type_exports[i].type_declaration];
		module->type_exports[i].namespace_symbol_id =
			declaration->namespace_symbol_id;
		module->type_exports[i].name_symbol_id = declaration->name_symbol_id;
	}
	for (size_t i = 0;
		i < module->view.interface.constructor_export_count;
		++i) {
		if (module->constructor_exports[i].constructor_declaration >=
			module->view.type_schema.constructor_count) {
			free(marked);
			free(relocation);
			return -1;
		}
		module->constructor_exports[i].name_symbol_id =
			module->constructor_declarations[
				module->constructor_exports[i].constructor_declaration
			].name_symbol_id;
	}
	for (size_t i = 0;
		i < module->view.interface.function_graph_selector_group_count;
		++i) {
		if (relocate_semantic_symbol(
				relocation,
				source->storage.count,
				&module->function_graph_selector_groups[i].display_symbol_id
			) != 0) {
			free(marked);
			free(relocation);
			return -1;
		}
	}
	module->view.symbols = (struct prototype_semantic_symbol_table_view) {
		.strings = (const char* const*)module->symbols,
		.count = target_count
	};
	free(marked);
	free(relocation);
	return 0;

fail:
	free(marked);
	free(relocation);
	return -1;
}

static int project_semantic_dependencies(
	struct prototype_elaborated_module* module
) {
	if (module->view.terms.term_count != 0) {
		module->dependencies = calloc(
			module->view.terms.term_count, sizeof(*module->dependencies)
		);
		if (!module->dependencies) {
			return -1;
		}
	}
	size_t count = 0;
	for (size_t i = 0; i < module->view.terms.term_count; ++i) {
		const struct prototype_term* term = &module->terms[i];
		if (term->tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		int duplicate = 0;
		for (size_t j = 0; j < count; ++j) {
			if (module->dependencies[j].namespace_symbol_id ==
					term->as.external_ref.name.namespace_symbol_id &&
				module->dependencies[j].name_symbol_id ==
					term->as.external_ref.name.name_symbol_id) {
				duplicate = 1;
				break;
			}
		}
		if (!duplicate) {
			module->dependencies[count++] =
				(struct prototype_semantic_dependency) {
					.namespace_symbol_id =
						term->as.external_ref.name.namespace_symbol_id,
					.name_symbol_id = term->as.external_ref.name.name_symbol_id
				};
		}
	}
	module->view.interface.dependencies = module->dependencies;
	module->view.interface.dependency_count = count;
	return 0;
}

struct semantic_term_marks {
	unsigned char* terms;
	unsigned char* cases;
	unsigned char* ih_scopes;
	unsigned char* fold_clauses;
	uint32_t* queue;
	size_t queue_count;
};

static int mark_semantic_term_id(
	const struct prototype_elaborated_module* module,
	struct semantic_term_marks* marks,
	uint32_t term_id
) {
	if (term_id >= module->view.terms.term_count) {
		return -1;
	}
	if (!marks->terms[term_id]) {
		marks->terms[term_id] = 1;
		marks->queue[marks->queue_count++] = term_id;
	}
	return 0;
}

static int mark_optional_semantic_term_id(
	const struct prototype_elaborated_module* module,
	struct semantic_term_marks* marks,
	uint32_t term_id
) {
	return term_id == PROTOTYPE_INVALID_ID ? 0 :
		mark_semantic_term_id(module, marks, term_id);
}

static int mark_semantic_ih_scope(
	const struct prototype_elaborated_module* module,
	struct semantic_term_marks* marks,
	uint32_t ih_scope_id
) {
	if (ih_scope_id >= module->view.terms.ih_scope_count) {
		return -1;
	}
	if (!marks->ih_scopes[ih_scope_id]) {
		marks->ih_scopes[ih_scope_id] = 1;
		return mark_semantic_term_id(
			module, marks, module->ih_scopes[ih_scope_id].match_term
		);
	}
	return 0;
}

static int mark_semantic_term_children(
	const struct prototype_elaborated_module* module,
	struct semantic_term_marks* marks,
	uint32_t term_id
) {
	const struct prototype_term* term = &module->terms[term_id];
	size_t field_count;
	if (prototype_term_schema_field_count(term->tag, &field_count) != 0) {
		return -1;
	}
	for (size_t i = 0; i < field_count; ++i) {
		struct prototype_term_field_value field;
		if (prototype_term_schema_field_at(term, i, &field) != 0) return -1;
		if (field.kind == PROTOTYPE_TERM_FIELD_TERM_REQUIRED) {
			if (mark_semantic_term_id(module, marks, field.as.u32) != 0) return -1;
		} else if (field.kind == PROTOTYPE_TERM_FIELD_TERM_OPTIONAL) {
			if (mark_optional_semantic_term_id(
					module, marks, field.as.u32
				) != 0) return -1;
		}
	}
	if (term->tag == PROTOTYPE_TERM_MATCH) {
		if (term->as.match.first_case > module->view.terms.case_count ||
			term->as.match.case_count > module->view.terms.case_count -
				term->as.match.first_case ||
			mark_semantic_ih_scope(
				module, marks, term->as.match.ih_scope_id
			) != 0) return -1;
		for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
			uint32_t case_id = term->as.match.first_case + i;
			marks->cases[case_id] = 1;
			const struct prototype_match_case* match_case =
				&module->term_cases[case_id];
			if (mark_semantic_term_id(
					module, marks, match_case->body
				) != 0 || mark_optional_semantic_term_id(
					module, marks, match_case->constructor_owner
				) != 0) return -1;
		}
	} else if (term->tag == PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
		if (mark_semantic_ih_scope(
				module, marks, term->as.induction_hypothesis.ih_scope_id
			) != 0) return -1;
	} else if (term->tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
		if (term->as.computation_fold.first_clause >
				module->view.terms.computation_fold_clause_count ||
			term->as.computation_fold.clause_count >
				module->view.terms.computation_fold_clause_count -
					term->as.computation_fold.first_clause) return -1;
		for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
			uint32_t clause_id = term->as.computation_fold.first_clause + i;
			marks->fold_clauses[clause_id] = 1;
			if (mark_semantic_term_id(
					module, marks,
					module->computation_fold_clauses[clause_id].operation
				) != 0 || mark_semantic_term_id(
					module, marks,
					module->computation_fold_clauses[clause_id].body
				) != 0) return -1;
		}
	}
	return 0;
}

static int relocate_required_term_id(
	const uint32_t* relocation,
	size_t source_count,
	uint32_t* p_term_id
) {
	if (*p_term_id >= source_count ||
		relocation[*p_term_id] == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_term_id = relocation[*p_term_id];
	return 0;
}

static int relocate_optional_term_id(
	const uint32_t* relocation,
	size_t source_count,
	uint32_t* p_term_id
) {
	return *p_term_id == PROTOTYPE_INVALID_ID ? 0 :
		relocate_required_term_id(relocation, source_count, p_term_id);
}

struct mark_semantic_term_roots_state {
	const struct prototype_elaborated_module* module;
	struct semantic_term_marks* marks;
};

static int mark_semantic_term_root(
	void* state,
	int requirement,
	uint32_t term_id
) {
	struct mark_semantic_term_roots_state* roots = state;
	if (!roots || !roots->module || !roots->marks) return -1;
	if (requirement == PROTOTYPE_CHECKER_REFERENCE_REQUIRED) {
		return mark_semantic_term_id(roots->module, roots->marks, term_id);
	}
	if (requirement == PROTOTYPE_CHECKER_REFERENCE_OPTIONAL) {
		return mark_optional_semantic_term_id(
			roots->module, roots->marks, term_id
		);
	}
	return -1;
}

static int relocate_term_record(
	struct prototype_term* term,
	const uint32_t* terms,
	size_t term_count,
	const uint32_t* cases,
	size_t case_count,
	const uint32_t* ih_scopes,
	size_t ih_scope_count,
	const uint32_t* fold_clauses,
	size_t fold_clause_count
) {
	size_t field_count;
	if (!term || prototype_term_schema_field_count(
			term->tag, &field_count
		) != 0) return -1;
	for (size_t i = 0; i < field_count; ++i) {
		struct prototype_term_field_value field;
		if (prototype_term_schema_field_at(term, i, &field) != 0) return -1;
		if (field.kind == PROTOTYPE_TERM_FIELD_TERM_REQUIRED) {
			if (relocate_required_term_id(
					terms, term_count, &field.as.u32
				) != 0) return -1;
		} else if (field.kind == PROTOTYPE_TERM_FIELD_TERM_OPTIONAL) {
			if (relocate_optional_term_id(
					terms, term_count, &field.as.u32
				) != 0) return -1;
		} else if (field.kind == PROTOTYPE_TERM_FIELD_CASE_SLICE) {
			if (term->as.match.case_count == 0) {
				field.as.u32 = 0;
			} else if (field.as.u32 >= case_count || !cases ||
				cases[field.as.u32] == PROTOTYPE_INVALID_ID) {
				return -1;
			} else {
				field.as.u32 = cases[field.as.u32];
			}
		} else if (field.kind == PROTOTYPE_TERM_FIELD_IH_SCOPE) {
			if (field.as.u32 >= ih_scope_count || !ih_scopes ||
				ih_scopes[field.as.u32] == PROTOTYPE_INVALID_ID) return -1;
			field.as.u32 = ih_scopes[field.as.u32];
		} else if (field.kind == PROTOTYPE_TERM_FIELD_FOLD_CLAUSE_SLICE) {
			if (term->as.computation_fold.clause_count == 0) {
				field.as.u32 = 0;
			} else if (field.as.u32 >= fold_clause_count || !fold_clauses ||
				fold_clauses[field.as.u32] == PROTOTYPE_INVALID_ID) {
				return -1;
			} else {
				field.as.u32 = fold_clauses[field.as.u32];
			}
		} else {
			continue;
		}
		if (prototype_term_schema_field_write(term, i, &field) != 0) return -1;
	}
	return 0;
}

static int compact_semantic_terms(struct prototype_elaborated_module* module) {
	if (!module || module->view.terms.term_count == 0) {
		return -1;
	}
	size_t source_term_count = module->view.terms.term_count;
	struct semantic_term_marks marks = {
		.terms = calloc(source_term_count, 1),
		.cases = module->view.terms.case_count == 0 ? NULL : calloc(
			module->view.terms.case_count, 1
		),
		.ih_scopes = module->view.terms.ih_scope_count == 0 ? NULL : calloc(
			module->view.terms.ih_scope_count, 1
		),
		.fold_clauses =
			module->view.terms.computation_fold_clause_count == 0 ? NULL : calloc(
				module->view.terms.computation_fold_clause_count, 1
			),
		.queue = malloc(source_term_count * sizeof(*marks.queue)),
		.queue_count = 0
	};
	if (!marks.terms || !marks.queue ||
		(module->view.terms.case_count != 0 && !marks.cases) ||
		(module->view.terms.ih_scope_count != 0 && !marks.ih_scopes) ||
		(module->view.terms.computation_fold_clause_count != 0 &&
		 !marks.fold_clauses)) {
		goto fail;
	}
	struct mark_semantic_term_roots_state mark_roots = {
		.module = module,
		.marks = &marks
	};
	if (prototype_checker_visit_semantic_term_roots(
			&module->view, mark_semantic_term_root, &mark_roots
		) != 0) goto fail;
	for (size_t cursor = 0; cursor < marks.queue_count; ++cursor) {
		if (mark_semantic_term_children(
				module, &marks, marks.queue[cursor]
			) != 0) {
			goto fail;
		}
	}

	uint32_t* term_relocation = malloc(
		source_term_count * sizeof(*term_relocation)
	);
	uint32_t* case_relocation = module->view.terms.case_count == 0 ? NULL : malloc(
		module->view.terms.case_count * sizeof(*case_relocation)
	);
	uint32_t* ih_relocation = module->view.terms.ih_scope_count == 0 ? NULL : malloc(
		module->view.terms.ih_scope_count * sizeof(*ih_relocation)
	);
	uint32_t* fold_relocation =
		module->view.terms.computation_fold_clause_count == 0 ? NULL : malloc(
			module->view.terms.computation_fold_clause_count *
				sizeof(*fold_relocation)
		);
	if (!term_relocation ||
		(module->view.terms.case_count != 0 && !case_relocation) ||
		(module->view.terms.ih_scope_count != 0 && !ih_relocation) ||
		(module->view.terms.computation_fold_clause_count != 0 &&
		 !fold_relocation)) {
		free(term_relocation);
		free(case_relocation);
		free(ih_relocation);
		free(fold_relocation);
		goto fail;
	}
	size_t term_count = 0;
	for (size_t i = 0; i < source_term_count; ++i) {
		term_relocation[i] = marks.terms[i] ?
			(uint32_t)term_count++ : PROTOTYPE_INVALID_ID;
	}
	size_t case_count = 0;
	size_t binder_count = 0;
	for (size_t i = 0; i < module->view.terms.case_count; ++i) {
		case_relocation[i] = marks.cases[i] ?
			(uint32_t)case_count++ : PROTOTYPE_INVALID_ID;
		if (marks.cases[i]) {
			binder_count += module->term_cases[i].binder_count;
		}
	}
	size_t ih_count = 0;
	for (size_t i = 0; i < module->view.terms.ih_scope_count; ++i) {
		ih_relocation[i] = marks.ih_scopes[i] ?
			(uint32_t)ih_count++ : PROTOTYPE_INVALID_ID;
	}
	size_t fold_count = 0;
	for (size_t i = 0;
		i < module->view.terms.computation_fold_clause_count;
		++i) {
		fold_relocation[i] = marks.fold_clauses[i] ?
			(uint32_t)fold_count++ : PROTOTYPE_INVALID_ID;
	}
	struct prototype_term* compact_terms = calloc(
		term_count, sizeof(*compact_terms)
	);
	struct prototype_match_case* compact_cases = case_count == 0 ? NULL : calloc(
		case_count, sizeof(*compact_cases)
	);
	struct prototype_case_binder* compact_binders =
		binder_count == 0 ? NULL : calloc(binder_count, sizeof(*compact_binders));
	struct prototype_semantic_ih_scope* compact_ih = ih_count == 0 ? NULL : calloc(
		ih_count, sizeof(*compact_ih)
	);
	struct prototype_computation_fold_clause* compact_fold =
		fold_count == 0 ? NULL : calloc(fold_count, sizeof(*compact_fold));
	if (!compact_terms || (case_count != 0 && !compact_cases) ||
		(binder_count != 0 && !compact_binders) ||
		(ih_count != 0 && !compact_ih) ||
		(fold_count != 0 && !compact_fold)) {
		free(compact_terms);
		free(compact_cases);
		free(compact_binders);
		free(compact_ih);
		free(compact_fold);
		free(term_relocation);
		free(case_relocation);
		free(ih_relocation);
		free(fold_relocation);
		goto fail;
	}
	for (size_t i = 0; i < source_term_count; ++i) {
		if (!marks.terms[i]) {
			continue;
		}
		compact_terms[term_relocation[i]] = module->terms[i];
		if (relocate_term_record(
				&compact_terms[term_relocation[i]],
				term_relocation,
				source_term_count,
				case_relocation,
				module->view.terms.case_count,
				ih_relocation,
				module->view.terms.ih_scope_count,
				fold_relocation,
				module->view.terms.computation_fold_clause_count
			) != 0) {
			goto compact_fail;
		}
	}
	size_t binder_cursor = 0;
	for (size_t i = 0; i < module->view.terms.case_count; ++i) {
		if (!marks.cases[i]) {
			continue;
		}
		struct prototype_match_case target = module->term_cases[i];
		target.first_binder = (uint32_t)binder_cursor;
		if (relocate_required_term_id(
				term_relocation, source_term_count, &target.body
			) != 0 || relocate_optional_term_id(
				term_relocation, source_term_count, &target.constructor_owner
			) != 0) {
			goto compact_fail;
		}
		compact_cases[case_relocation[i]] = target;
		for (uint32_t j = 0; j < target.binder_count; ++j) {
			compact_binders[binder_cursor++] = module->case_binders[
				module->term_cases[i].first_binder + j
			];
		}
	}
	for (size_t i = 0; i < module->view.terms.ih_scope_count; ++i) {
		if (!marks.ih_scopes[i]) {
			continue;
		}
		compact_ih[ih_relocation[i]] = module->ih_scopes[i];
		if (relocate_required_term_id(
				term_relocation,
				source_term_count,
				&compact_ih[ih_relocation[i]].match_term
			) != 0) {
			goto compact_fail;
		}
	}
	for (size_t i = 0;
		i < module->view.terms.computation_fold_clause_count;
		++i) {
		if (!marks.fold_clauses[i]) {
			continue;
		}
		compact_fold[fold_relocation[i]] = module->computation_fold_clauses[i];
		if (relocate_required_term_id(
				term_relocation,
				source_term_count,
				&compact_fold[fold_relocation[i]].operation
			) != 0 || relocate_required_term_id(
				term_relocation,
				source_term_count,
				&compact_fold[fold_relocation[i]].body
			) != 0) {
			goto compact_fail;
		}
	}
	if (prototype_checker_relocate_semantic_term_roots(
			module, term_relocation, source_term_count
		) != 0) goto compact_fail;
	for (size_t i = 0; i < module->view.occurrences.occurrence_count; ++i) {
		struct prototype_semantic_occurrence* occurrence = &module->occurrences[i];
		if (occurrence->ih_scope_id != PROTOTYPE_INVALID_ID) {
			if (occurrence->ih_scope_id >= module->view.terms.ih_scope_count ||
				ih_relocation[occurrence->ih_scope_id] == PROTOTYPE_INVALID_ID) {
				goto compact_fail;
			}
			occurrence->ih_scope_id = ih_relocation[occurrence->ih_scope_id];
		}
	}
	free(module->terms);
	free(module->term_cases);
	free(module->case_binders);
	free(module->ih_scopes);
	free(module->computation_fold_clauses);
	module->terms = compact_terms;
	module->term_cases = compact_cases;
	module->case_binders = compact_binders;
	module->ih_scopes = compact_ih;
	module->computation_fold_clauses = compact_fold;
	module->view.terms = (struct prototype_semantic_term_graph_view) {
		.terms = module->terms,
		.term_count = term_count,
		.cases = module->term_cases,
		.case_count = case_count,
		.case_binders = module->case_binders,
		.case_binder_count = binder_count,
		.ih_scopes = module->ih_scopes,
		.ih_scope_count = ih_count,
		.computation_fold_clauses = module->computation_fold_clauses,
		.computation_fold_clause_count = fold_count
	};
	free(term_relocation);
	free(case_relocation);
	free(ih_relocation);
	free(fold_relocation);
	free(marks.terms);
	free(marks.cases);
	free(marks.ih_scopes);
	free(marks.fold_clauses);
	free(marks.queue);
	return 0;

compact_fail:
	free(compact_terms);
	free(compact_cases);
	free(compact_binders);
	free(compact_ih);
	free(compact_fold);
	free(term_relocation);
	free(case_relocation);
	free(ih_relocation);
	free(fold_relocation);
fail:
	free(marks.terms);
	free(marks.cases);
	free(marks.ih_scopes);
	free(marks.fold_clauses);
	free(marks.queue);
	return -1;
}

static int project_semantic_universes(
	const struct prototype_universe_db* source,
	struct prototype_elaborated_module* module
) {
	if (!source || !module || source->certificate.state !=
		PROTOTYPE_UNIVERSE_CERTIFICATE_CLOSED) {
		return -1;
	}
	for (size_t i = 0; i < module->view.terms.term_count; ++i) {
		const struct prototype_term* term = &module->terms[i];
		if (term->tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
			continue;
		}
		int already_present = 0;
		for (size_t j = 0; j < module->view.universes.level_count; ++j) {
			if (module->universe_levels[j].level_var ==
				term->as.universe_var.level_var) {
				already_present = 1;
				break;
			}
		}
		if (already_present) {
			continue;
		}
		const struct prototype_universe_level* source_level = NULL;
		for (size_t j = 0; j < source->level_count; ++j) {
			if (source->levels[j].level_var == term->as.universe_var.level_var) {
				source_level = &source->levels[j];
				break;
			}
		}
		if (!source_level) {
			return -1;
		}
		struct prototype_semantic_universe_level* levels = realloc(
			module->universe_levels,
			(module->view.universes.level_count + 1) * sizeof(*levels)
		);
		if (!levels) {
			return -1;
		}
		module->universe_levels = levels;
		module->universe_levels[module->view.universes.level_count++] =
			(struct prototype_semantic_universe_level) {
				.level_var = source_level->level_var,
				.value = source_level->value
			};
	}
	module->view.universes.levels = module->universe_levels;
	return 0;
}

static int project_contracts(
	const struct prototype_verification_db* source,
	struct prototype_elaborated_module* module,
	uint32_t** p_relocation
) {
	uint32_t* relocation = NULL;
	if (source->obligation_count != 0) {
		relocation = malloc(source->obligation_count * sizeof(*relocation));
		if (!relocation) {
			return -1;
		}
		for (size_t i = 0; i < source->obligation_count; ++i) {
			relocation[i] = PROTOTYPE_INVALID_ID;
		}
	}
	for (size_t i = 0; i < source->obligation_count; ++i) {
		const struct prototype_verification_obligation* obligation =
			&source->obligations[i];
		if (obligation->state == PROTOTYPE_VERIFICATION_OBLIGATION_FAILED) {
			free(relocation);
			return -1;
		}
		if (obligation->state == PROTOTYPE_VERIFICATION_OBLIGATION_PENDING) {
			relocation[i] = (uint32_t)module->view.contracts.contract_count++;
		}
	}
	if (module->view.contracts.contract_count != 0) {
		module->contracts = calloc(
			module->view.contracts.contract_count, sizeof(*module->contracts)
		);
		if (!module->contracts) {
			free(relocation);
			return -1;
		}
	}
	for (size_t i = 0; i < source->obligation_count; ++i) {
		if (relocation[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_verification_obligation* source_contract =
			&source->obligations[i];
		module->contracts[relocation[i]] = (struct prototype_semantic_contract) {
			.kind = source_contract->kind,
			.occurrence = source_contract->occurrence,
			.core_term = source_contract->core_term,
			.computation_occurrence = source_contract->computation_occurrence,
			.continuation_occurrence = source_contract->continuation_occurrence,
			.continuation_binding_id = source_contract->continuation_binder_id,
			.input_classifier = source_contract->input_classifier,
			.classifier_family = source_contract->classifier_family,
			.effect_row = source_contract->effect_row,
			.effect_constraint_kind = source_contract->effect_constraint_kind,
			.normalization_profile = source_contract->normalization_profile,
			.schema_version = source_contract->schema_version
		};
	}
	for (size_t i = 0; i < source->dependency_count; ++i) {
		uint32_t obligation_id = source->dependencies[i].obligation_id;
		if (obligation_id >= source->obligation_count) {
			free(relocation);
			return -1;
		}
		if (relocation[obligation_id] != PROTOTYPE_INVALID_ID) {
			module->view.contracts.dependency_count++;
		}
	}
	if (module->view.contracts.dependency_count != 0) {
		module->contract_dependencies = calloc(
			module->view.contracts.dependency_count,
			sizeof(*module->contract_dependencies)
		);
		if (!module->contract_dependencies) {
			free(relocation);
			return -1;
		}
	}
	size_t target_dependency = 0;
	for (size_t i = 0; i < source->dependency_count; ++i) {
		const struct prototype_verification_dependency* source_dependency =
			&source->dependencies[i];
		if (relocation[source_dependency->obligation_id] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		module->contract_dependencies[target_dependency++] =
			(struct prototype_semantic_contract_dependency) {
				.occurrence = source_dependency->occurrence,
				.contract_id = relocation[source_dependency->obligation_id]
			};
	}
	module->view.contracts.contracts = module->contracts;
	module->view.contracts.dependencies = module->contract_dependencies;
	*p_relocation = relocation;
	return 0;
}

static int project_occurrence_kind(int source_kind, int* p_target_kind) {
	if (!p_target_kind) {
		return -1;
	}
	switch (source_kind) {
		case PROTOTYPE_TYPED_OCCURRENCE_ATOM:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_ATOM;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_VAR:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_VAR;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_REFERENCE;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_CONSTRUCTOR;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_APP;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_LAMBDA;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_INDUCTION_HYPOTHESIS;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_EXPECTED_TYPE;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_RETURN;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_THUNK;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_FORCE;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_REQUEST;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
			*p_target_kind = PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD;
			break;
		default:
			return -1;
	}
	return 0;
}

static int project_occurrences(
	const struct prototype_typed_occurrence_graph* source,
	const uint32_t* contract_relocation,
	size_t contract_relocation_count,
	const uint32_t* context_relocation,
	size_t context_relocation_count,
	const uint32_t* substitution_relocation,
	size_t substitution_relocation_count,
	struct prototype_elaborated_module* module
) {
	if (source->occurrence_count != 0) {
		module->occurrences = calloc(
			source->occurrence_count, sizeof(*module->occurrences)
		);
	}
	if (source->edge_count != 0) {
		module->occurrence_edges = calloc(
			source->edge_count, sizeof(*module->occurrence_edges)
		);
	}
	if (source->case_count != 0) {
		module->match_cases = calloc(
			source->case_count, sizeof(*module->match_cases)
		);
	}
	if (source->fold_clause_count != 0) {
		module->fold_clauses = calloc(
			source->fold_clause_count, sizeof(*module->fold_clauses)
		);
	}
	if ((source->occurrence_count != 0 && !module->occurrences) ||
		(source->edge_count != 0 && !module->occurrence_edges) ||
		(source->case_count != 0 && !module->match_cases) ||
		(source->fold_clause_count != 0 && !module->fold_clauses)) {
		return -1;
	}

	for (size_t i = 0; i < source->occurrence_count; ++i) {
		const struct prototype_typed_occurrence* occurrence = &source->occurrences[i];
		struct prototype_semantic_occurrence* target = &module->occurrences[i];
		if (occurrence->context_id >= context_relocation_count ||
			context_relocation[occurrence->context_id] == PROTOTYPE_INVALID_ID ||
			(occurrence->context_action_substitution != PROTOTYPE_INVALID_ID &&
			 (occurrence->context_action_substitution >=
				substitution_relocation_count || !substitution_relocation ||
			  substitution_relocation[
				occurrence->context_action_substitution
			  ] == PROTOTYPE_INVALID_ID))) {
			return -1;
		}
		if (project_occurrence_kind(occurrence->tag, &target->kind) != 0) {
			return -1;
		}
		target->category = occurrence->category;
		target->computation_kind = occurrence->computation_kind;
		target->application_role = occurrence->application_role;
		target->context_id = context_relocation[occurrence->context_id];
		target->context_action_substitution =
			occurrence->context_action_substitution == PROTOTYPE_INVALID_ID ?
				PROTOTYPE_INVALID_ID : substitution_relocation[
					occurrence->context_action_substitution
				];
		target->origin_core_term = occurrence->source_core_term;
		target->origin_classifier = occurrence->source_classifier;
		target->core_term = occurrence->core_term;
		target->asserted_classifier = occurrence->classifier;
		target->conditional_contract = PROTOTYPE_INVALID_ID;
		if (occurrence->classifier_status ==
			PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED) {
			target->classifier_evidence_kind =
				PROTOTYPE_SEMANTIC_CLASSIFIER_EXACT;
		} else if (occurrence->classifier_status ==
			PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_RESIDUAL_VERIFICATION &&
			occurrence->classifier_verification_obligation <
				contract_relocation_count && contract_relocation &&
			contract_relocation[occurrence->classifier_verification_obligation] !=
				PROTOTYPE_INVALID_ID) {
			target->classifier_evidence_kind =
				PROTOTYPE_SEMANTIC_CLASSIFIER_CONDITIONAL;
			target->conditional_contract =
				contract_relocation[occurrence->classifier_verification_obligation];
		} else {
			return -1;
		}
		target->binding_id = occurrence->binding_id;
		target->first_edge = occurrence->first_edge;
		target->edge_count = occurrence->edge_count;
		target->wrapped_occurrence = occurrence->wrapped_occurrence;
		target->binder_classifier = occurrence->binder_classifier;
		target->match_motive = occurrence->match_motive;
		target->ih_owner_occurrence = occurrence->ih_owner_occurrence;
		target->ih_scope_id = occurrence->ih_scope_id;
		target->ih_case_index = occurrence->ih_case_index;
		target->ih_field_index = occurrence->ih_field_index;
		target->fold_return_binding_id = occurrence->fold_return_binder_id;
		target->implicit_effect_row_count = occurrence->implicit_effect_row_count;
		memcpy(
			target->implicit_effect_row_binders,
			occurrence->implicit_effect_row_binders,
			sizeof(target->implicit_effect_row_binders)
		);
		target->first_case = target->kind ==
			PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH ? occurrence->first_case :
			PROTOTYPE_INVALID_ID;
		target->case_count = target->kind ==
			PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH ? occurrence->case_count : 0;
		target->first_fold_clause = target->kind ==
			PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD ?
				occurrence->first_fold_clause : PROTOTYPE_INVALID_ID;
		target->fold_clause_count = target->kind ==
			PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD ?
				occurrence->fold_clause_count : 0;
	}
	for (size_t i = 0; i < source->edge_count; ++i) {
		module->occurrence_edges[i] = (struct prototype_semantic_occurrence_edge) {
			.role = source->edges[i].role,
			.ordinal = source->edges[i].ordinal,
			.child_occurrence = source->edges[i].child_occurrence
		};
	}
	for (size_t i = 0; i < source->case_count; ++i) {
		const struct prototype_typed_occurrence_match_case* source_case =
			&source->cases[i];
		struct prototype_semantic_match_case* target = &module->match_cases[i];
		if (source_case->context_id >= context_relocation_count ||
			context_relocation[source_case->context_id] == PROTOTYPE_INVALID_ID ||
			(source_case->refinement_substitution != PROTOTYPE_INVALID_ID &&
			 (source_case->refinement_substitution >=
				substitution_relocation_count || !substitution_relocation ||
			  substitution_relocation[
				source_case->refinement_substitution
			  ] == PROTOTYPE_INVALID_ID))) {
			return -1;
		}
		target->context_id = context_relocation[source_case->context_id];
		target->refinement_substitution =
			source_case->refinement_substitution == PROTOTYPE_INVALID_ID ?
				PROTOTYPE_INVALID_ID : substitution_relocation[
					source_case->refinement_substitution
				];
		target->constructor_owner = source_case->constructor_owner;
		target->constructor_id = source_case->constructor_id;
		target->binder_count = source_case->binder_count;
		memcpy(target->binder_ids, source_case->binder_ids, sizeof(target->binder_ids));
		switch (source_case->refinement_status) {
			case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED:
				target->refinement_kind =
					PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_SOLVED;
				break;
			case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE:
				target->refinement_kind =
					PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_IMPOSSIBLE;
				break;
			case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT:
				target->refinement_kind =
					PROTOTYPE_SEMANTIC_MATCH_REFINEMENT_CONSTANT;
				break;
			default:
				return -1;
		}
	}
	for (size_t i = 0; i < source->fold_clause_count; ++i) {
		if (source->fold_clauses[i].context_id >= context_relocation_count ||
			context_relocation[source->fold_clauses[i].context_id] ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		module->fold_clauses[i] = (struct prototype_semantic_fold_clause) {
			.context_id = context_relocation[source->fold_clauses[i].context_id],
			.argument_binding_id = source->fold_clauses[i].argument_binder_id,
			.continuation_binding_id = source->fold_clauses[i].continuation_binder_id
		};
	}
	module->view.occurrences = (struct prototype_semantic_occurrence_graph_view) {
		.occurrences = module->occurrences,
		.occurrence_count = source->occurrence_count,
		.edges = module->occurrence_edges,
		.edge_count = source->edge_count,
		.cases = module->match_cases,
		.case_count = source->case_count,
		.fold_clauses = module->fold_clauses,
		.fold_clause_count = source->fold_clause_count
	};
	return 0;
}

int prototype_elaborated_module_project(
	const struct symbol_table* symbols,
	const struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* type_schema,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_universe_db* universes,
	const struct prototype_frozen_module_snapshot* snapshot,
	struct prototype_elaborated_module* module
) {
	if (!symbols || !terms || !type_schema || !intrinsic_environment || !universes ||
		!snapshot || !module ||
		!snapshot->typed_occurrences.sealed || !snapshot->typed_occurrences.frozen ||
		snapshot->typed_occurrences.transaction_active) {
		return -1;
	}
	prototype_elaborated_module_destroy(module);
	module->view.selected_entry_term = snapshot->selected_entry_term;
	module->view.selected_entry_classifier = snapshot->selected_entry_classifier;
	module->view.selected_entry_occurrence = snapshot->selected_entry_occurrence;
	module->view.required_runtime_capabilities =
		snapshot->required_runtime_capabilities;

	uint32_t* contract_relocation = NULL;
	uint32_t* context_relocation = NULL;
	uint32_t* substitution_relocation = NULL;
	if (project_intrinsic_environment(intrinsic_environment, module) != 0) {
		goto project_fail;
	}
	if (project_term_graph(terms, type_schema, module) != 0) {
		goto project_fail;
	}
	if (project_structural_graphs(
			type_schema,
			snapshot,
			module,
			&context_relocation,
			&substitution_relocation
		) != 0) {
		goto project_fail;
	}
	if (project_contracts(
			&snapshot->verification, module, &contract_relocation
		) != 0) {
		goto project_fail;
	}
	if (project_occurrences(
			&snapshot->typed_occurrences,
			contract_relocation,
			snapshot->verification.obligation_count,
			context_relocation,
			snapshot->contexts.context_count,
			substitution_relocation,
			snapshot->substitutions.substitution_count,
			module
		) != 0) {
		goto project_fail;
	}
	if (compact_semantic_terms(module) != 0) {
		goto project_fail;
	}
	if (project_semantic_exports(snapshot, module) != 0) {
		goto project_fail;
	}
	if (project_semantic_universes(universes, module) != 0) {
		goto project_fail;
	}
	if (project_semantic_symbols(symbols, module) != 0) {
		goto project_fail;
	}
	if (project_semantic_dependencies(module) != 0) {
		goto project_fail;
	}
	goto project_done;

project_fail:
		free(contract_relocation);
		free(context_relocation);
		free(substitution_relocation);
		prototype_elaborated_module_destroy(module);
		return -1;
project_done:
	free(contract_relocation);
	free(context_relocation);
	free(substitution_relocation);
	if (prototype_elaborated_module_validate_structure(&module->view) != 0) {
		prototype_elaborated_module_destroy(module);
		return -1;
	}
	return 0;
}
