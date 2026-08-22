#include "a_program/frontend/function_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUNCTION_GRAPH_MAX_BINDINGS 32
#define FUNCTION_GRAPH_MAX_RECURSIVE_CALLS 16
#define FUNCTION_GRAPH_MAX_ARGUMENTS 16

struct function_graph_binding_map {
	uint32_t source_ast_binder;
	uint32_t source_binding;
	uint32_t target_ast_binder;
	int symbol_id;
};

struct function_graph_recursive_call {
	uint32_t source_ih_ast;
	uint32_t source_argument_ast_binder;
	int argument_symbol_id;
	uint32_t output_ast_binder;
	uint32_t graph_ast_binder;
	int output_symbol_id;
	int graph_symbol_id;
};

struct function_graph_generation {
	struct prototype_ast_db* asts;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_compile_metadata* metadata;
	struct symbol_table* symbols;
	struct prototype_accepted_definition_view view;
	struct prototype_source_span span;
	int owner_symbol;
	int graph_symbol;
	int result_symbol;
	int returned_symbol;
	int runner_symbol;
	int executable_symbol;
	int executed_symbol;
	uint32_t graph_type_def;
	uint32_t result_type_def;
	uint32_t graph_assignment;
	uint32_t result_assignment;
	uint32_t runner_assignment;
	uint32_t executable_assignment;
	uint32_t owner_source_lambda;
	uint32_t owner_source_match;
	uint32_t owner_source_input_binder;
	uint32_t owner_source_input_binding;
	uint32_t owner_input_classifier;
	uint32_t runner_input_binder;
	int runner_input_symbol;
	uint32_t source_body;
	uint32_t argument_count;
	uint32_t source_argument_ast_binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t source_argument_bindings[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t source_argument_classifiers[FUNCTION_GRAPH_MAX_ARGUMENTS];
	int source_argument_symbols[FUNCTION_GRAPH_MAX_ARGUMENTS];
	int branch_precise;
};

static const struct prototype_typed_occurrence* function_graph_occurrence(
	const struct prototype_compile_metadata* metadata,
	uint32_t source_ast,
	int tag
) {
	if (!metadata) {
		return NULL;
	}
	for (size_t i = 0; i < metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* occurrence =
			&metadata->typed_occurrences.occurrences[i];
		if (occurrence->source_ast == source_ast && occurrence->tag == tag) {
			return occurrence;
		}
	}
	return NULL;
}

static int function_graph_internal_symbol(
	struct symbol_table* symbols,
	const char* prefix,
	const char* owner,
	int* p_symbol
) {
	char buffer[256];
	if (!symbols || !prefix || !owner || !p_symbol ||
		snprintf(buffer, sizeof(buffer), "$%s.%s", prefix, owner) < 0) {
		return -1;
	}
	int symbol = symbol_intern(symbols, buffer, strlen(buffer));
	if (symbol < 0) {
		return -1;
	}
	*p_symbol = symbol;
	return 0;
}

static int function_graph_projection_type(
	struct function_graph_generation* generation,
	uint32_t term,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t* p_type
) {
	if (!generation || !p_type || binding_count > FUNCTION_GRAPH_MAX_BINDINGS) {
		return -1;
	}
	struct prototype_ast_accepted_binding_projection projections[
		FUNCTION_GRAPH_MAX_BINDINGS
	];
	for (uint32_t i = 0; i < binding_count; ++i) {
		projections[i].source_binding_id = bindings[i].source_binding;
		projections[i].target_ast_binder_id = bindings[i].target_ast_binder;
	}
	return prototype_ast_type_expr_accepted_projection(
		generation->asts,
		term,
		projections,
		binding_count,
		generation->span,
		p_type
	);
}

static int function_graph_type_name_app(
	struct function_graph_generation* generation,
	int type_symbol,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_type
) {
	uint32_t type;
	if (!generation || !p_type || prototype_ast_type_expr_name(
			generation->asts, type_symbol, generation->span, &type
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (prototype_ast_type_expr_app(
				generation->asts, type, arguments[i], generation->span, &type
			) != 0) {
			return -1;
		}
	}
	*p_type = type;
	return 0;
}

static int function_graph_self_app(
	struct function_graph_generation* generation,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_type
) {
	uint32_t type;
	if (!generation || !p_type || prototype_ast_type_expr_self(
			generation->asts, generation->span, &type
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (prototype_ast_type_expr_app(
				generation->asts, type, arguments[i], generation->span, &type
			) != 0) {
			return -1;
		}
	}
	*p_type = type;
	return 0;
}

static const struct function_graph_binding_map* function_graph_find_binding(
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t source_ast_binder
) {
	for (uint32_t i = 0; i < binding_count; ++i) {
		if (bindings[i].source_ast_binder == source_ast_binder) {
			return &bindings[i];
		}
	}
	return NULL;
}

static const struct function_graph_recursive_call* function_graph_find_recursive(
	const struct function_graph_recursive_call* recursive,
	uint32_t recursive_count,
	uint32_t source_ih_ast
) {
	for (uint32_t i = 0; i < recursive_count; ++i) {
		if (recursive[i].source_ih_ast == source_ih_ast) {
			return &recursive[i];
		}
	}
	return NULL;
}

static int function_graph_clone_value(
	struct function_graph_generation* generation,
	uint32_t source_ast,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	const struct function_graph_recursive_call* recursive,
	uint32_t recursive_count,
	uint32_t* p_ast
) {
	if (!generation || !p_ast || source_ast >= generation->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node source = generation->asts->nodes[source_ast];
	switch (source.tag) {
		case PROTOTYPE_AST_VAR: {
			const struct function_graph_binding_map* binding =
				function_graph_find_binding(
					bindings, binding_count, source.as.var.ast_binder_id
				);
			return !binding ? -1 : prototype_ast_var(
				generation->asts,
				binding->target_ast_binder,
				binding->symbol_id,
				generation->span,
				p_ast
			);
		}
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS: {
			const struct function_graph_recursive_call* call =
				function_graph_find_recursive(recursive, recursive_count, source_ast);
			return !call ? -1 : prototype_ast_var(
				generation->asts,
				call->output_ast_binder,
				call->output_symbol_id,
				generation->span,
				p_ast
			);
		}
		case PROTOTYPE_AST_NAME:
			return prototype_ast_name(
				generation->asts, source.as.name.symbol_id,
				generation->span, p_ast
			);
		case PROTOTYPE_AST_NAME_IN_NAMESPACE:
			return prototype_ast_name_in_namespace(
				generation->asts,
				source.as.name_in_namespace.namespace_symbol_id,
				source.as.name_in_namespace.symbol_id,
				generation->span,
				p_ast
			);
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE: {
			uint32_t owner;
			if (function_graph_clone_value(
					generation,
					source.as.name_in_ast_namespace.namespace_ast,
					bindings,
					binding_count,
					recursive,
					recursive_count,
					&owner
				) != 0) {
				return -1;
			}
			return prototype_ast_name_in_ast_namespace(
				generation->asts,
				owner,
				source.as.name_in_ast_namespace.symbol_id,
				generation->span,
				p_ast
			);
		}
		case PROTOTYPE_AST_APP: {
			uint32_t function;
			uint32_t argument;
			if (function_graph_clone_value(
					generation, source.as.app.function, bindings, binding_count,
					recursive, recursive_count, &function
				) != 0 || function_graph_clone_value(
					generation, source.as.app.argument, bindings, binding_count,
					recursive, recursive_count, &argument
				) != 0) {
				return -1;
			}
			return prototype_ast_app(
				generation->asts, function, argument, generation->span, p_ast
			);
		}
		case PROTOTYPE_AST_TEXT_LITERAL:
			return prototype_ast_text_literal(
				generation->asts, source.as.text_literal.text_symbol_id,
				generation->span, p_ast
			);
		case PROTOTYPE_AST_INT_LITERAL:
			return prototype_ast_int_literal(
				generation->asts, source.as.int_literal.value,
				generation->span, p_ast
			);
		default:
			return -1;
	}
}

static int function_graph_value_type_expr(
	struct function_graph_generation* generation,
	uint32_t value_ast,
	uint32_t* p_type
) {
	if (!generation || !p_type || value_ast >= generation->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node value = generation->asts->nodes[value_ast];
	switch (value.tag) {
		case PROTOTYPE_AST_VAR:
			return prototype_ast_type_expr_var(
				generation->asts,
				value.as.var.ast_binder_id,
				value.as.var.symbol_id,
				generation->span,
				p_type
			);
		case PROTOTYPE_AST_NAME:
			return prototype_ast_type_expr_name(
				generation->asts, value.as.name.symbol_id,
				generation->span, p_type
			);
		case PROTOTYPE_AST_NAME_IN_NAMESPACE:
			return prototype_ast_type_expr_name_in_namespace(
				generation->asts,
				value.as.name_in_namespace.namespace_symbol_id,
				value.as.name_in_namespace.symbol_id,
				generation->span,
				p_type
			);
		case PROTOTYPE_AST_APP: {
			uint32_t function;
			uint32_t argument;
			if (function_graph_value_type_expr(
					generation, value.as.app.function, &function
				) != 0 || function_graph_value_type_expr(
					generation, value.as.app.argument, &argument
				) != 0) {
				return -1;
			}
			return prototype_ast_type_expr_app(
				generation->asts, function, argument, generation->span, p_type
			);
		}
		default:
			return -1;
	}
}

static int function_graph_collect_ih(
	const struct prototype_ast_db* asts,
	uint32_t ast,
	uint32_t* ih_asts,
	uint32_t capacity,
	uint32_t* p_count
) {
	if (!asts || !ih_asts || !p_count || ast >= asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &asts->nodes[ast];
	if (node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		for (uint32_t i = 0; i < *p_count; ++i) {
			if (ih_asts[i] == ast) {
				return 0;
			}
		}
		if (*p_count >= capacity) {
			return -1;
		}
		ih_asts[(*p_count)++] = ast;
		return 0;
	}
	if (node->tag == PROTOTYPE_AST_APP) {
		return function_graph_collect_ih(
			asts, node->as.app.function, ih_asts, capacity, p_count
		) != 0 ? -1 : function_graph_collect_ih(
			asts, node->as.app.argument, ih_asts, capacity, p_count
		);
	}
	return node->tag == PROTOTYPE_AST_VAR || node->tag == PROTOTYPE_AST_NAME ||
		node->tag == PROTOTYPE_AST_NAME_IN_NAMESPACE ||
		node->tag == PROTOTYPE_AST_NAME_IN_AST_NAMESPACE ||
		node->tag == PROTOTYPE_AST_TEXT_LITERAL ||
		node->tag == PROTOTYPE_AST_INT_LITERAL ? 0 : -1;
}

static int function_graph_constructor_value(
	struct function_graph_generation* generation,
	const struct prototype_typed_occurrence_match_case* operation_case,
	const uint32_t* field_values,
	uint32_t field_count,
	uint32_t* p_value
) {
	uint32_t type_id;
	uint32_t owner_arguments[16];
	uint32_t owner_argument_count;
	if (!generation || !operation_case || !p_value ||
		field_count != operation_case->binder_count ||
		prototype_term_type_instance_info(
			generation->terms,
			operation_case->constructor_owner,
			&type_id,
			owner_arguments,
			&owner_argument_count
		) != 0 || owner_argument_count != 0 ||
		type_id >= generation->type_declarations->semantic_schema.type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&generation->type_declarations->semantic_schema.type_declarations[type_id];
	if (operation_case->constructor_id >= type->constructor_count ||
		type->first_constructor + operation_case->constructor_id >=
			generation->type_declarations->semantic_schema.constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&generation->type_declarations->semantic_schema.constructor_declarations[
			type->first_constructor + operation_case->constructor_id
		];
	uint32_t value;
	if (prototype_ast_name_in_namespace(
			generation->asts,
			type->name_symbol_id,
			constructor->name_symbol_id,
			generation->span,
			&value
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		if (prototype_ast_app(
				generation->asts, value, field_values[i], generation->span, &value
			) != 0) {
			return -1;
		}
	}
	*p_value = value;
	return 0;
}

static int function_graph_add_type_assignment(
	struct function_graph_generation* generation,
	int symbol,
	uint32_t type_def,
	uint32_t* p_assignment
) {
	uint32_t formation;
	uint32_t source_entry;
	if (!generation || !p_assignment ||
		prototype_ast_type_formation(
			generation->asts, type_def, generation->span, &formation
		) != 0 ||
		(source_entry = prototype_ast_new_source_entry(generation->asts)) ==
			PROTOTYPE_INVALID_ID) {
		return -1;
	}
	return prototype_ast_add_term_assignment(
		generation->asts,
		symbol,
		formation,
		source_entry,
		generation->span,
		generation->span,
		p_assignment
	);
}

static int function_graph_add_term_assignment(
	struct function_graph_generation* generation,
	int symbol,
	uint32_t term,
	uint32_t* p_assignment
) {
	uint32_t source_entry;
	if (!generation || !p_assignment ||
		(source_entry = prototype_ast_new_source_entry(generation->asts)) ==
			PROTOTYPE_INVALID_ID) {
		return -1;
	}
	return prototype_ast_add_term_assignment(
		generation->asts,
		symbol,
		term,
		source_entry,
		generation->span,
		generation->span,
		p_assignment
	);
}

static int function_graph_prepare(
	struct function_graph_generation* generation,
	uint32_t assignment_id
) {
	if (!generation || assignment_id >= generation->asts->assignment_count) {
		return -1;
	}
	if (prototype_accepted_definition_view_open(
			generation->asts,
			generation->terms,
			generation->type_declarations,
			generation->judgement,
			generation->metadata,
			assignment_id,
			&generation->view
		) != 0) {
		const struct prototype_ast_term_assignment_def* failed =
			&generation->asts->assignments[assignment_id];
		fprintf(stderr,
			"accepted definition view unavailable compiled=%d published=%d operation=%u classifier=%u frozen=%d sealed=%d\n",
			failed->compiled, failed->published, failed->compiled_operation,
			failed->compiled_classifier,
			generation->metadata->typed_occurrences.frozen,
			generation->metadata->typed_occurrences.sealed);
		return -1;
	}
	if (generation->view.final_totality !=
			PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ||
		generation->view.final_effect_row >= generation->terms->term_count ||
		generation->terms->terms[generation->view.final_effect_row].tag !=
			PROTOTYPE_TERM_EFFECT_ROW_EMPTY) {
		fprintf(stderr,
			"accepted definition graph fragment rejected totality=%d effect=%u tag=%d\n",
			generation->view.final_totality,
			generation->view.final_effect_row,
			generation->view.final_effect_row < generation->terms->term_count ?
				generation->terms->terms[generation->view.final_effect_row].tag : -1);
		return -1;
	}
	const struct prototype_ast_term_assignment_def* owner =
		&generation->asts->assignments[assignment_id];
	generation->owner_symbol = owner->name_symbol_id;
	generation->span = owner->body_span;
	const char* owner_name = symbol_to_string(
		generation->symbols, generation->owner_symbol
	);
	if (!owner_name || function_graph_internal_symbol(
			generation->symbols, "graph", owner_name, &generation->graph_symbol
		) != 0 || function_graph_internal_symbol(
			generation->symbols, "result", owner_name, &generation->result_symbol
		) != 0 || function_graph_internal_symbol(
			generation->symbols, "certified", owner_name, &generation->runner_symbol
		) != 0 || function_graph_internal_symbol(
			generation->symbols, "executable", owner_name,
			&generation->executable_symbol
		) != 0 || (generation->returned_symbol = symbol_intern(
			generation->symbols, "returned", 8
		)) < 0 || (generation->executed_symbol = symbol_intern(
			generation->symbols, "executed", 8
		)) < 0) {
		return -1;
	}
	generation->argument_count = 0;
	uint32_t current = owner->ast;
	while (current < generation->asts->node_count &&
		generation->asts->nodes[current].tag == PROTOTYPE_AST_LAMBDA) {
		if (generation->argument_count >= FUNCTION_GRAPH_MAX_ARGUMENTS) {
			return 1;
		}
		const struct prototype_ast_node* argument = &generation->asts->nodes[current];
		const struct prototype_typed_occurrence* occurrence = function_graph_occurrence(
			generation->metadata, current, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA
		);
		if (!occurrence || occurrence->binding_id == PROTOTYPE_INVALID_ID ||
			occurrence->binder_classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		uint32_t index = generation->argument_count++;
		generation->source_argument_ast_binders[index] =
			argument->as.lambda.ast_binder_id;
		generation->source_argument_bindings[index] = occurrence->binding_id;
		generation->source_argument_classifiers[index] =
			occurrence->binder_classifier;
		generation->source_argument_symbols[index] =
			argument->as.lambda.binder_symbol_id;
		current = argument->as.lambda.body;
	}
	if (generation->argument_count == 0) {
		return 1;
	}
	generation->source_body = current;
	generation->owner_source_lambda = owner->ast;
	generation->owner_source_input_binder =
		generation->source_argument_ast_binders[0];
	generation->owner_source_input_binding = generation->source_argument_bindings[0];
	generation->owner_input_classifier = generation->source_argument_classifiers[0];
	generation->runner_input_symbol = generation->source_argument_symbols[0];
	generation->branch_precise = 0;
	if (generation->argument_count == 1 && current < generation->asts->node_count &&
		generation->asts->nodes[current].tag == PROTOTYPE_AST_MATCH) {
		const struct prototype_ast_node* match = &generation->asts->nodes[current];
		if (match->as.match.scrutinee < generation->asts->node_count &&
			generation->asts->nodes[match->as.match.scrutinee].tag ==
				PROTOTYPE_AST_VAR &&
			generation->asts->nodes[match->as.match.scrutinee].as.var.ast_binder_id ==
				generation->owner_source_input_binder) {
			generation->branch_precise = 1;
			generation->owner_source_match = current;
		}
	}
	return 0;
}

static int function_graph_generate_graph_type(
	struct function_graph_generation* generation
) {
	uint32_t input_type;
	uint32_t output_type;
	uint32_t graph_input_binder = prototype_ast_new_binder(generation->asts);
	uint32_t graph_output_binder = prototype_ast_new_binder(generation->asts);
	if (graph_input_binder == PROTOTYPE_INVALID_ID ||
		graph_output_binder == PROTOTYPE_INVALID_ID ||
		function_graph_projection_type(
			generation, generation->owner_input_classifier, NULL, 0, &input_type
		) != 0) {
		return -1;
	}
	struct function_graph_binding_map input_map = {
		.source_ast_binder = generation->owner_source_input_binder,
		.source_binding = generation->owner_source_input_binding,
		.target_ast_binder = graph_input_binder,
		.symbol_id = generation->runner_input_symbol
	};
	if (function_graph_projection_type(
			generation, generation->view.final_result_type,
			&input_map, 1, &output_type
		) != 0 || prototype_ast_type_add(
			generation->asts,
			generation->graph_symbol,
			generation->span,
			generation->span,
			&generation->graph_type_def
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts,
			generation->graph_type_def,
			graph_input_binder,
			generation->runner_input_symbol,
			input_type,
			PROTOTYPE_AST_FAMILY_BINDER_INDEX,
			generation->span
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts,
			generation->graph_type_def,
			graph_output_binder,
			symbol_intern(generation->symbols, "output", 6),
			output_type,
			PROTOTYPE_AST_FAMILY_BINDER_INDEX,
			generation->span
		) != 0) {
		return -1;
	}
	const struct prototype_ast_node* source_match =
		&generation->asts->nodes[generation->owner_source_match];
	const struct prototype_typed_occurrence* match_occurrence =
		function_graph_occurrence(
			generation->metadata,
			generation->owner_source_match,
			PROTOTYPE_TYPED_OCCURRENCE_MATCH
		);
	if (!match_occurrence || match_occurrence->case_count !=
			source_match->as.match.case_count) {
		return -1;
	}
	for (uint32_t case_index = 0; case_index < source_match->as.match.case_count;
		++case_index) {
		const struct prototype_ast_match_case* source_case =
			&generation->asts->cases[source_match->as.match.first_case + case_index];
		const struct prototype_typed_occurrence_match_case* operation_case =
			&generation->metadata->typed_occurrences.cases[
				match_occurrence->first_case + case_index
			];
		if (source_case->binder_count != operation_case->binder_count ||
			operation_case->refinement_status !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED) {
			return -1;
		}
		uint32_t field_types[FUNCTION_GRAPH_MAX_BINDINGS];
		uint32_t field_binders[FUNCTION_GRAPH_MAX_BINDINGS];
		int field_symbols[FUNCTION_GRAPH_MAX_BINDINGS];
		uint32_t field_values[FUNCTION_GRAPH_MAX_BINDINGS];
		struct function_graph_binding_map bindings[FUNCTION_GRAPH_MAX_BINDINGS];
		uint32_t binding_count = 0;
		uint32_t field_count = 0;
		for (uint32_t field = 0; field < source_case->binder_count; ++field) {
			const struct prototype_ast_binder* source_binder =
				&generation->asts->case_binders[source_case->first_binder + field];
			uint32_t target_binder = prototype_ast_new_binder(generation->asts);
			const struct prototype_context* field_context = prototype_context_get(
				&generation->metadata->contexts,
				operation_case->context_id
			);
			uint32_t field_binding_context;
			if (target_binder == PROTOTYPE_INVALID_ID || !field_context ||
				prototype_context_find_binding(
					&generation->metadata->contexts,
					operation_case->context_id,
					operation_case->binder_ids[field],
					&field_binding_context
				) != 0) {
				return -1;
			}
			const struct prototype_context* binding_context = prototype_context_get(
				&generation->metadata->contexts, field_binding_context
			);
			if (!binding_context || function_graph_projection_type(
					generation,
					prototype_context_classifier_term(binding_context),
					bindings,
					binding_count,
					&field_types[field_count]
				) != 0 || prototype_ast_var(
					generation->asts,
					target_binder,
					source_binder->symbol_id,
					generation->span,
					&field_values[field_count]
				) != 0) {
				return -1;
			}
			field_binders[field_count] = target_binder;
			field_symbols[field_count] = source_binder->symbol_id;
			bindings[binding_count++] = (struct function_graph_binding_map) {
				.source_ast_binder = source_binder->ast_binder_id,
				.source_binding = operation_case->binder_ids[field],
				.target_ast_binder = target_binder,
				.symbol_id = source_binder->symbol_id
			};
			field_count++;
		}
		uint32_t ih_asts[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
		uint32_t ih_count = 0;
		if (function_graph_collect_ih(
				generation->asts, source_case->body, ih_asts,
				FUNCTION_GRAPH_MAX_RECURSIVE_CALLS, &ih_count
			) != 0) {
			return -1;
		}
		struct function_graph_recursive_call recursive[
			FUNCTION_GRAPH_MAX_RECURSIVE_CALLS
		];
		for (uint32_t i = 0; i < ih_count; ++i) {
			const struct prototype_ast_node* ih = &generation->asts->nodes[ih_asts[i]];
			const struct function_graph_binding_map* argument =
				function_graph_find_binding(
					bindings, binding_count, ih->as.induction_hypothesis.ast_binder_id
				);
			if (!argument || field_count + 2 > FUNCTION_GRAPH_MAX_BINDINGS) {
				return -1;
			}
			uint32_t output_binder = prototype_ast_new_binder(generation->asts);
			uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
			int output_symbol = symbol_intern(generation->symbols, "recursiveOutput", 15);
			int graph_symbol = symbol_intern(generation->symbols, "recursiveGraph", 14);
			struct function_graph_binding_map recursive_input = {
				.source_ast_binder = generation->owner_source_input_binder,
				.source_binding = generation->owner_source_input_binding,
				.target_ast_binder = argument->target_ast_binder,
				.symbol_id = argument->symbol_id
			};
			if (output_binder == PROTOTYPE_INVALID_ID ||
				graph_binder == PROTOTYPE_INVALID_ID || output_symbol < 0 || graph_symbol < 0 ||
				function_graph_projection_type(
					generation, generation->view.final_result_type,
					&recursive_input, 1, &field_types[field_count]
				) != 0) {
				return -1;
			}
			field_binders[field_count] = output_binder;
			field_symbols[field_count] = output_symbol;
			uint32_t recursive_output_type_var;
			uint32_t recursive_argument_type_var;
			if (prototype_ast_type_expr_var(
					generation->asts,
					argument->target_ast_binder,
					argument->symbol_id,
					generation->span,
					&recursive_argument_type_var
				) != 0 || prototype_ast_type_expr_var(
					generation->asts,
					output_binder,
					output_symbol,
					generation->span,
					&recursive_output_type_var
				) != 0) {
				return -1;
			}
			uint32_t graph_arguments[2] = {
				recursive_argument_type_var, recursive_output_type_var
			};
			if (function_graph_self_app(
					generation, graph_arguments, 2,
					&field_types[field_count + 1]
				) != 0) {
				return -1;
			}
			field_binders[field_count + 1] = graph_binder;
			field_symbols[field_count + 1] = graph_symbol;
			recursive[i] = (struct function_graph_recursive_call) {
				.source_ih_ast = ih_asts[i],
				.source_argument_ast_binder = argument->source_ast_binder,
				.argument_symbol_id = argument->symbol_id,
				.output_ast_binder = output_binder,
				.graph_ast_binder = graph_binder,
				.output_symbol_id = output_symbol,
				.graph_symbol_id = graph_symbol
			};
			field_count += 2;
		}
		uint32_t output_value;
		uint32_t input_value;
		if (function_graph_clone_value(
				generation, source_case->body, bindings, binding_count,
				recursive, ih_count, &output_value
			) != 0 || function_graph_constructor_value(
				generation, operation_case, field_values,
				source_case->binder_count, &input_value
			) != 0) {
			return -1;
		}
		uint32_t input_index;
		uint32_t output_index;
		uint32_t result_arguments[2];
		if (function_graph_value_type_expr(generation, input_value, &input_index) != 0 ||
			function_graph_value_type_expr(generation, output_value, &output_index) != 0) {
			return -1;
		}
		result_arguments[0] = input_index;
		result_arguments[1] = output_index;
		uint32_t result_type;
		if (function_graph_self_app(
				generation, result_arguments, 2, &result_type
			) != 0 || prototype_ast_type_add_constructor(
				generation->asts,
				generation->graph_type_def,
				source_case->constructor_symbol_id,
				generation->span,
				field_types,
				field_binders,
				field_symbols,
				field_count,
				result_type
			) != 0) {
			return -1;
		}
	}
	return function_graph_add_type_assignment(
		generation,
		generation->graph_symbol,
		generation->graph_type_def,
		&generation->graph_assignment
	);
}

static int function_graph_generate_result_type(
	struct function_graph_generation* generation
) {
	uint32_t family_input_binder = prototype_ast_new_binder(generation->asts);
	uint32_t input_type;
	if (family_input_binder == PROTOTYPE_INVALID_ID ||
		function_graph_projection_type(
			generation, generation->owner_input_classifier, NULL, 0, &input_type
		) != 0 || prototype_ast_type_add(
			generation->asts,
			generation->result_symbol,
			generation->span,
			generation->span,
			&generation->result_type_def
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts,
			generation->result_type_def,
			family_input_binder,
			generation->runner_input_symbol,
			input_type,
			PROTOTYPE_AST_FAMILY_BINDER_PARAMETER,
			generation->span
		) != 0) {
		return -1;
	}
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	int graph_symbol = symbol_intern(generation->symbols, "graph", 5);
	if (output_binder == PROTOTYPE_INVALID_ID || graph_binder == PROTOTYPE_INVALID_ID ||
		output_symbol < 0 || graph_symbol < 0) {
		return -1;
	}
	uint32_t field_types[2];
	uint32_t field_binders[2] = {
		output_binder, graph_binder
	};
	int field_symbols[2] = {
		output_symbol, graph_symbol
	};
	struct function_graph_binding_map input_map = {
		.source_ast_binder = generation->owner_source_input_binder,
		.source_binding = generation->owner_source_input_binding,
		.target_ast_binder = family_input_binder,
		.symbol_id = generation->runner_input_symbol
	};
	if (function_graph_projection_type(
			generation, generation->view.final_result_type, &input_map, 1, &field_types[0]
		) != 0) {
		return -1;
	}
	uint32_t input_var;
	uint32_t output_var;
	if (prototype_ast_type_expr_var(
			generation->asts, family_input_binder, generation->runner_input_symbol,
			generation->span, &input_var
		) != 0 || prototype_ast_type_expr_var(
			generation->asts, output_binder, output_symbol,
			generation->span, &output_var
		) != 0) {
		return -1;
	}
	uint32_t graph_arguments[2] = { input_var, output_var };
	if (function_graph_type_name_app(
			generation, generation->graph_symbol, graph_arguments, 2,
			&field_types[1]
		) != 0) {
		return -1;
	}
	uint32_t result_type;
	if (function_graph_self_app(
			generation, NULL, 0, &result_type
		) != 0 || prototype_ast_type_add_constructor(
			generation->asts,
			generation->result_type_def,
			generation->returned_symbol,
			generation->span,
			field_types,
			field_binders,
			field_symbols,
			2,
			result_type
		) != 0) {
		return -1;
	}
	return function_graph_add_type_assignment(
		generation,
		generation->result_symbol,
		generation->result_type_def,
		&generation->result_assignment
	);
}

static int function_graph_result_constructor(
	struct function_graph_generation* generation,
	uint32_t input,
	uint32_t output,
	uint32_t graph,
	uint32_t* p_result
) {
	uint32_t owner;
	uint32_t result;
	if (!generation || !p_result || prototype_ast_name(
			generation->asts,
			generation->result_symbol,
			generation->span,
			&owner
		) != 0 || prototype_ast_app(
			generation->asts, owner, input, generation->span, &owner
		) != 0 || prototype_ast_name_in_ast_namespace(
			generation->asts,
			owner,
			generation->returned_symbol,
			generation->span,
			&result
		) != 0 || prototype_ast_app(
			generation->asts, result, output, generation->span, &result
		) != 0 || prototype_ast_app(
			generation->asts, result, graph, generation->span, &result
		) != 0) {
		return -1;
	}
	*p_result = result;
	return 0;
}

static int function_graph_graph_constructor(
	struct function_graph_generation* generation,
	int constructor_symbol,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_graph
) {
	uint32_t graph;
	if (!generation || !p_graph || prototype_ast_name_in_namespace(
			generation->asts,
			generation->graph_symbol,
			constructor_symbol,
			generation->span,
			&graph
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (prototype_ast_app(
				generation->asts, graph, arguments[i], generation->span, &graph
			) != 0) {
			return -1;
		}
	}
	*p_graph = graph;
	return 0;
}

static int function_graph_graph_ascription(
	struct function_graph_generation* generation,
	uint32_t graph,
	uint32_t input,
	uint32_t output,
	uint32_t* p_ascribed
) {
	uint32_t input_type;
	uint32_t output_type;
	if (function_graph_value_type_expr(generation, input, &input_type) != 0 ||
		function_graph_value_type_expr(generation, output, &output_type) != 0) {
		return -1;
	}
	uint32_t graph_arguments[2] = { input_type, output_type };
	uint32_t graph_type;
	if (function_graph_type_name_app(
			generation, generation->graph_symbol, graph_arguments, 2, &graph_type
		) != 0) {
		return -1;
	}
	return prototype_ast_ascription(
		generation->asts, graph, graph_type, generation->span, p_ascribed
	);
}

static int function_graph_generate_runner_branch(
	struct function_graph_generation* generation,
	const struct prototype_ast_match_case* source_case,
	const struct prototype_typed_occurrence_match_case* operation_case,
	struct prototype_ast_binder* generated_case_binders,
	struct prototype_ast_match_case_input* p_case
) {
	if (!generation || !source_case || !operation_case ||
		!generated_case_binders || !p_case ||
		source_case->binder_count != operation_case->binder_count ||
		source_case->binder_count > FUNCTION_GRAPH_MAX_BINDINGS) {
		return -1;
	}
	struct function_graph_binding_map bindings[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t field_values[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t binding_count = 0;
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		const struct prototype_ast_binder* source_binder =
			&generation->asts->case_binders[source_case->first_binder + i];
		uint32_t target_binder = prototype_ast_new_binder(generation->asts);
		if (target_binder == PROTOTYPE_INVALID_ID || prototype_ast_var(
				generation->asts,
				target_binder,
				source_binder->symbol_id,
				generation->span,
				&field_values[i]
			) != 0) {
			return -1;
		}
		generated_case_binders[i] = (struct prototype_ast_binder) {
			.ast_binder_id = target_binder,
			.symbol_id = source_binder->symbol_id
		};
		bindings[binding_count++] = (struct function_graph_binding_map) {
			.source_ast_binder = source_binder->ast_binder_id,
			.source_binding = operation_case->binder_ids[i],
			.target_ast_binder = target_binder,
			.symbol_id = source_binder->symbol_id
		};
	}
	uint32_t ih_asts[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t ih_count = 0;
	if (function_graph_collect_ih(
			generation->asts,
			source_case->body,
			ih_asts,
			FUNCTION_GRAPH_MAX_RECURSIVE_CALLS,
			&ih_count
		) != 0) {
		return -1;
	}
	struct function_graph_recursive_call recursive[
		FUNCTION_GRAPH_MAX_RECURSIVE_CALLS
	];
	uint32_t recursive_input_values[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t recursive_graph_values[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t recursive_output_values[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t recursive_packet_binders[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	int recursive_packet_symbols[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	for (uint32_t i = 0; i < ih_count; ++i) {
		const struct prototype_ast_node* ih = &generation->asts->nodes[ih_asts[i]];
		const struct function_graph_binding_map* argument =
			function_graph_find_binding(
				bindings, binding_count, ih->as.induction_hypothesis.ast_binder_id
			);
		if (!argument || prototype_ast_var(
				generation->asts,
				argument->target_ast_binder,
				argument->symbol_id,
				generation->span,
				&recursive_input_values[i]
			) != 0) {
			return -1;
		}
		recursive[i].source_ih_ast = ih_asts[i];
		recursive[i].source_argument_ast_binder = argument->target_ast_binder;
		recursive[i].argument_symbol_id = argument->symbol_id;
		recursive[i].output_ast_binder = prototype_ast_new_binder(generation->asts);
		recursive[i].graph_ast_binder = prototype_ast_new_binder(generation->asts);
		recursive[i].output_symbol_id = symbol_intern(
			generation->symbols, "recursiveOutput", 15
		);
		recursive[i].graph_symbol_id = symbol_intern(
			generation->symbols, "recursiveGraph", 14
		);
		recursive_packet_binders[i] = prototype_ast_new_binder(generation->asts);
		recursive_packet_symbols[i] = symbol_intern(
			generation->symbols, "recursivePacket", 15
		);
		if (recursive[i].output_ast_binder == PROTOTYPE_INVALID_ID ||
			recursive[i].graph_ast_binder == PROTOTYPE_INVALID_ID ||
			recursive_packet_binders[i] == PROTOTYPE_INVALID_ID ||
			recursive[i].output_symbol_id < 0 || recursive[i].graph_symbol_id < 0 ||
			recursive_packet_symbols[i] < 0 || prototype_ast_var(
				generation->asts,
				recursive[i].output_ast_binder,
				recursive[i].output_symbol_id,
				generation->span,
				&recursive_output_values[i]
			) != 0 || prototype_ast_var(
				generation->asts,
				recursive[i].graph_ast_binder,
				recursive[i].graph_symbol_id,
				generation->span,
				&recursive_graph_values[i]
			) != 0) {
			return -1;
		}
	}
	uint32_t output;
	uint32_t input;
	if (function_graph_clone_value(
			generation,
			source_case->body,
			bindings,
			binding_count,
			recursive,
			ih_count,
			&output
		) != 0 || function_graph_constructor_value(
			generation,
			operation_case,
			field_values,
			source_case->binder_count,
			&input
		) != 0) {
		return -1;
	}
	uint32_t graph_arguments[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t graph_argument_count = 0;
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		graph_arguments[graph_argument_count++] = field_values[i];
	}
	for (uint32_t i = 0; i < ih_count; ++i) {
		uint32_t ascribed_graph;
		if (function_graph_graph_ascription(
				generation,
				recursive_graph_values[i],
				recursive_input_values[i],
				recursive_output_values[i],
				&ascribed_graph
			) != 0 || graph_argument_count + 2 > FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		graph_arguments[graph_argument_count++] = recursive_output_values[i];
		graph_arguments[graph_argument_count++] = ascribed_graph;
	}
	uint32_t graph;
	uint32_t result;
	if (function_graph_graph_constructor(
			generation,
			source_case->constructor_symbol_id,
			graph_arguments,
			graph_argument_count,
			&graph
		) != 0 || function_graph_result_constructor(
			generation, input, output, graph, &result
		) != 0) {
		return -1;
	}
	uint32_t body = result;
	for (uint32_t reverse = ih_count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		uint32_t packet_var;
		uint32_t call;
		if (prototype_ast_var(
				generation->asts,
				recursive_packet_binders[i],
				recursive_packet_symbols[i],
				generation->span,
				&packet_var
			) != 0 || prototype_ast_induction_hypothesis(
				generation->asts,
				recursive[i].source_argument_ast_binder,
				recursive[i].argument_symbol_id,
				generation->span,
				&call
			) != 0) {
			return -1;
		}
		struct prototype_ast_binder returned_binders[2] = {
			{
				.ast_binder_id = recursive[i].output_ast_binder,
				.symbol_id = recursive[i].output_symbol_id
			},
			{
				.ast_binder_id = recursive[i].graph_ast_binder,
				.symbol_id = recursive[i].graph_symbol_id
			}
		};
		struct prototype_ast_match_case_input returned_case = {
			.constructor_symbol_id = generation->returned_symbol,
			.binders = returned_binders,
			.binder_count = 2,
			.body = body,
			.span = generation->span
		};
		uint32_t packet_match;
		uint32_t binding_item;
		uint32_t expression_item;
		if (prototype_ast_match(
				generation->asts,
				packet_var,
				&returned_case,
				1,
				generation->span,
				&packet_match
			) != 0 || prototype_ast_block_binding(
				generation->asts,
				recursive_packet_binders[i],
				recursive_packet_symbols[i],
				PROTOTYPE_INVALID_ID,
				call,
				generation->span,
				&binding_item
			) != 0 || prototype_ast_block_expression(
				generation->asts,
				packet_match,
				generation->span,
				&expression_item
			) != 0) {
			return -1;
		}
		uint32_t items[2] = { binding_item, expression_item };
		if (prototype_ast_computation_block(
				generation->asts,
				items,
				2,
				1,
				PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
				generation->span,
				&body
			) != 0) {
			return -1;
		}
	}
	*p_case = (struct prototype_ast_match_case_input) {
		.constructor_symbol_id = source_case->constructor_symbol_id,
		.binders = generated_case_binders,
		.binder_count = source_case->binder_count,
		.body = body,
		.span = generation->span
	};
	return 0;
}

static int function_graph_generate_runner(
	struct function_graph_generation* generation
) {
	const struct prototype_ast_node* source_match =
		&generation->asts->nodes[generation->owner_source_match];
	const struct prototype_typed_occurrence* match_occurrence =
		function_graph_occurrence(
			generation->metadata,
			generation->owner_source_match,
			PROTOTYPE_TYPED_OCCURRENCE_MATCH
		);
	if (!match_occurrence || source_match->as.match.case_count > 32 ||
		match_occurrence->case_count != source_match->as.match.case_count) {
		return -1;
	}
	generation->runner_input_binder = prototype_ast_new_binder(generation->asts);
	uint32_t input_type;
	uint32_t input_var;
	if (generation->runner_input_binder == PROTOTYPE_INVALID_ID ||
		function_graph_projection_type(
			generation, generation->owner_input_classifier, NULL, 0, &input_type
		) != 0 || prototype_ast_var(
			generation->asts,
			generation->runner_input_binder,
			generation->runner_input_symbol,
			generation->span,
			&input_var
		) != 0) {
		return -1;
	}
	struct prototype_ast_match_case_input generated_cases[32];
	struct prototype_ast_binder generated_binders[
		32 * FUNCTION_GRAPH_MAX_BINDINGS
	];
	for (uint32_t i = 0; i < source_match->as.match.case_count; ++i) {
		const struct prototype_ast_match_case* source_case =
			&generation->asts->cases[source_match->as.match.first_case + i];
		const struct prototype_typed_occurrence_match_case* operation_case =
			&generation->metadata->typed_occurrences.cases[
				match_occurrence->first_case + i
			];
		if (function_graph_generate_runner_branch(
				generation,
				source_case,
				operation_case,
				&generated_binders[i * FUNCTION_GRAPH_MAX_BINDINGS],
				&generated_cases[i]
			) != 0) {
			return -1;
		}
	}
	uint32_t match;
	uint32_t runner;
	if (prototype_ast_match(
			generation->asts,
			input_var,
			generated_cases,
			source_match->as.match.case_count,
			generation->span,
			&match
		) != 0 || prototype_ast_lambda(
			generation->asts,
			generation->runner_input_binder,
			generation->runner_input_symbol,
			input_type,
			match,
			generation->span,
			&runner
		) != 0 || function_graph_add_term_assignment(
			generation,
			generation->runner_symbol,
			runner,
			&generation->runner_assignment
		) != 0) {
		return -1;
	}
	return 0;
}

static int function_graph_generate_projection(
	struct function_graph_generation* generation
) {
	uint32_t input_binder = prototype_ast_new_binder(generation->asts);
	uint32_t input_type;
	uint32_t input_var;
	uint32_t runner;
	uint32_t call;
	uint32_t packet_binder = prototype_ast_new_binder(generation->asts);
	int packet_symbol = symbol_intern(generation->symbols, "certified", 9);
	uint32_t packet_var;
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	int graph_symbol = symbol_intern(generation->symbols, "graph", 5);
	uint32_t output_var;
	if (input_binder == PROTOTYPE_INVALID_ID || packet_binder == PROTOTYPE_INVALID_ID ||
		output_binder == PROTOTYPE_INVALID_ID || graph_binder == PROTOTYPE_INVALID_ID ||
		packet_symbol < 0 || output_symbol < 0 ||
		graph_symbol < 0 || function_graph_projection_type(
			generation, generation->owner_input_classifier, NULL, 0, &input_type
		) != 0 || prototype_ast_var(
			generation->asts, input_binder, generation->runner_input_symbol,
			generation->span, &input_var
		) != 0 || prototype_ast_name(
			generation->asts, generation->runner_symbol,
			generation->span, &runner
		) != 0 || prototype_ast_app(
			generation->asts, runner, input_var, generation->span, &call
		) != 0 || prototype_ast_var(
			generation->asts, packet_binder, packet_symbol,
			generation->span, &packet_var
		) != 0 || prototype_ast_var(
			generation->asts, output_binder, output_symbol,
			generation->span, &output_var
		) != 0) {
		return -1;
	}
	struct prototype_ast_binder returned_binders[2] = {
		{ .ast_binder_id = output_binder, .symbol_id = output_symbol },
		{ .ast_binder_id = graph_binder, .symbol_id = graph_symbol }
	};
	struct prototype_ast_match_case_input returned_case = {
		.constructor_symbol_id = generation->returned_symbol,
		.binders = returned_binders,
		.binder_count = 2,
		.body = output_var,
		.span = generation->span
	};
	uint32_t packet_match;
	uint32_t binding_item;
	uint32_t expression_item;
	uint32_t block;
	uint32_t projection;
	if (prototype_ast_match(
			generation->asts, packet_var, &returned_case, 1,
			generation->span, &packet_match
		) != 0 || prototype_ast_block_binding(
			generation->asts,
			packet_binder,
			packet_symbol,
			PROTOTYPE_INVALID_ID,
			call,
			generation->span,
			&binding_item
		) != 0 || prototype_ast_block_expression(
			generation->asts,
			packet_match,
			generation->span,
			&expression_item
		) != 0) {
		return -1;
	}
	uint32_t items[2] = { binding_item, expression_item };
	if (prototype_ast_computation_block(
			generation->asts,
			items,
			2,
			1,
			PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
			generation->span,
			&block
		) != 0 || prototype_ast_lambda(
			generation->asts,
			input_binder,
			generation->runner_input_symbol,
			input_type,
			block,
			generation->span,
			&projection
		) != 0) {
		return -1;
	}
	return function_graph_add_term_assignment(
		generation,
		generation->executable_symbol,
		projection,
		&generation->executable_assignment
	);
}

static void function_graph_unpublish_owner(
	struct prototype_compile_metadata* metadata,
	int owner_symbol
) {
	if (!metadata) {
		return;
	}
	size_t write = 0;
	for (size_t read = 0; read < metadata->label_count; ++read) {
		if (metadata->labels[read].name_symbol_id == owner_symbol) {
			continue;
		}
		if (write != read) {
			metadata->labels[write] = metadata->labels[read];
		}
		write++;
	}
	metadata->label_count = write;
}

static int function_graph_make_argument_context(
	struct function_graph_generation* generation,
	uint32_t* binders,
	uint32_t* types,
	uint32_t* values,
	struct function_graph_binding_map* maps
) {
	if (!generation || !binders || !types || !values || !maps) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		binders[i] = prototype_ast_new_binder(generation->asts);
		if (binders[i] == PROTOTYPE_INVALID_ID || function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				maps,
				i,
				&types[i]
			) != 0 || prototype_ast_var(
				generation->asts,
				binders[i],
				generation->source_argument_symbols[i],
				generation->span,
				&values[i]
			) != 0) {
			return -1;
		}
		maps[i] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = binders[i],
			.symbol_id = generation->source_argument_symbols[i]
		};
	}
	return 0;
}

static int function_graph_generate_coarse_graph_type(
	struct function_graph_generation* generation
) {
	uint32_t binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t types[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	if (prototype_ast_type_add(
			generation->asts,
			generation->graph_symbol,
			generation->span,
			generation->span,
			&generation->graph_type_def
		) != 0 || function_graph_make_argument_context(
			generation, binders, types, values, maps
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		if (prototype_ast_type_add_family_binder(
				generation->asts,
				generation->graph_type_def,
				binders[i],
				generation->source_argument_symbols[i],
				types[i],
				PROTOTYPE_AST_FAMILY_BINDER_INDEX,
				generation->span
			) != 0) {
			return -1;
		}
	}
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	uint32_t output_type;
	uint32_t output_value;
	if (output_binder == PROTOTYPE_INVALID_ID || output_symbol < 0 ||
		function_graph_projection_type(
			generation,
			generation->view.final_result_type,
			maps,
			generation->argument_count,
			&output_type
		) != 0 || prototype_ast_var(
			generation->asts,
			output_binder,
			output_symbol,
			generation->span,
			&output_value
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts,
			generation->graph_type_def,
			output_binder,
			output_symbol,
			output_type,
			PROTOTYPE_AST_FAMILY_BINDER_INDEX,
			generation->span
		) != 0) {
		return -1;
	}

	uint32_t field_binders[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
	uint32_t field_types[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
	int field_symbols[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
	uint32_t field_values[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
	struct function_graph_binding_map field_maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		field_binders[i] = prototype_ast_new_binder(generation->asts);
		field_symbols[i] = generation->source_argument_symbols[i];
		if (field_binders[i] == PROTOTYPE_INVALID_ID || function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				field_maps,
				i,
				&field_types[i]
			) != 0 || prototype_ast_type_expr_var(
				generation->asts,
				field_binders[i],
				field_symbols[i],
				generation->span,
				&field_values[i]
			) != 0) {
			return -1;
		}
		field_maps[i] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = field_binders[i],
			.symbol_id = field_symbols[i]
		};
	}
	uint32_t output_index = generation->argument_count;
	field_binders[output_index] = prototype_ast_new_binder(generation->asts);
	field_symbols[output_index] = output_symbol;
	if (field_binders[output_index] == PROTOTYPE_INVALID_ID ||
		function_graph_projection_type(
			generation,
			generation->view.final_result_type,
			field_maps,
			generation->argument_count,
			&field_types[output_index]
		) != 0 || prototype_ast_type_expr_var(
			generation->asts,
			field_binders[output_index],
			output_symbol,
			generation->span,
			&field_values[output_index]
		) != 0) {
		return -1;
	}
	uint32_t result_type;
	if (function_graph_self_app(
			generation,
			field_values,
			generation->argument_count + 1,
			&result_type
		) != 0 || prototype_ast_type_add_constructor(
			generation->asts,
			generation->graph_type_def,
			generation->executed_symbol,
			generation->span,
			field_types,
			field_binders,
			field_symbols,
			generation->argument_count + 1,
			result_type
		) != 0) {
		return -1;
	}
	return function_graph_add_type_assignment(
		generation,
		generation->graph_symbol,
		generation->graph_type_def,
		&generation->graph_assignment
	);
}

static int function_graph_generate_coarse_result_type(
	struct function_graph_generation* generation
) {
	uint32_t binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t types[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	if (prototype_ast_type_add(
			generation->asts,
			generation->result_symbol,
			generation->span,
			generation->span,
			&generation->result_type_def
		) != 0 || function_graph_make_argument_context(
			generation, binders, types, values, maps
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		if (prototype_ast_type_add_family_binder(
				generation->asts,
				generation->result_type_def,
				binders[i],
				generation->source_argument_symbols[i],
				types[i],
				PROTOTYPE_AST_FAMILY_BINDER_PARAMETER,
				generation->span
			) != 0) {
			return -1;
		}
	}
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	int graph_symbol = symbol_intern(generation->symbols, "graph", 5);
	uint32_t field_types[2];
	uint32_t field_binders[2] = { output_binder, graph_binder };
	int field_symbols[2] = { output_symbol, graph_symbol };
	if (output_binder == PROTOTYPE_INVALID_ID || graph_binder == PROTOTYPE_INVALID_ID ||
		output_symbol < 0 || graph_symbol < 0 || function_graph_projection_type(
			generation,
			generation->view.final_result_type,
			maps,
			generation->argument_count,
			&field_types[0]
		) != 0) {
		return -1;
	}
	uint32_t graph_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		if (prototype_ast_type_expr_var(
				generation->asts,
				binders[i],
				generation->source_argument_symbols[i],
				generation->span,
				&graph_arguments[i]
			) != 0) {
			return -1;
		}
	}
	if (prototype_ast_type_expr_var(
			generation->asts,
			output_binder,
			output_symbol,
			generation->span,
			&graph_arguments[generation->argument_count]
		) != 0 || function_graph_type_name_app(
			generation,
			generation->graph_symbol,
			graph_arguments,
			generation->argument_count + 1,
			&field_types[1]
		) != 0) {
		return -1;
	}
	uint32_t result_type;
	if (function_graph_self_app(generation, NULL, 0, &result_type) != 0 ||
		prototype_ast_type_add_constructor(
			generation->asts,
			generation->result_type_def,
			generation->returned_symbol,
			generation->span,
			field_types,
			field_binders,
			field_symbols,
			2,
			result_type
		) != 0) {
		return -1;
	}
	return function_graph_add_type_assignment(
		generation,
		generation->result_symbol,
		generation->result_type_def,
		&generation->result_assignment
	);
}

static int function_graph_coarse_result_constructor(
	struct function_graph_generation* generation,
	const uint32_t* inputs,
	uint32_t output,
	uint32_t graph,
	uint32_t* p_result
) {
	uint32_t owner;
	uint32_t result;
	if (!generation || !inputs || !p_result || prototype_ast_name(
			generation->asts, generation->result_symbol, generation->span, &owner
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		if (prototype_ast_app(
				generation->asts, owner, inputs[i], generation->span, &owner
			) != 0) {
			return -1;
		}
	}
	if (prototype_ast_name_in_ast_namespace(
			generation->asts,
			owner,
			generation->returned_symbol,
			generation->span,
			&result
		) != 0 || prototype_ast_app(
			generation->asts, result, output, generation->span, &result
		) != 0 || prototype_ast_app(
			generation->asts, result, graph, generation->span, &result
		) != 0) {
		return -1;
	}
	*p_result = result;
	return 0;
}

static int function_graph_wrap_lambdas(
	struct function_graph_generation* generation,
	const uint32_t* binders,
	const uint32_t* types,
	uint32_t body,
	uint32_t* p_lambda
) {
	if (!generation || !binders || !types || !p_lambda) {
		return -1;
	}
	for (uint32_t reverse = generation->argument_count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		if (prototype_ast_lambda(
				generation->asts,
				binders[i],
				generation->source_argument_symbols[i],
				types[i],
				body,
				generation->span,
				&body
			) != 0) {
			return -1;
		}
	}
	*p_lambda = body;
	return 0;
}

static int function_graph_generate_coarse_runner(
	struct function_graph_generation* generation
) {
	uint32_t binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t types[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t body_function;
	if (function_graph_make_argument_context(
			generation, binders, types, values, maps
		) != 0 || prototype_ast_name(
			generation->asts,
			generation->owner_symbol,
			generation->span,
			&body_function
		) != 0) {
		return -1;
	}
	uint32_t call = body_function;
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		if (prototype_ast_app(
				generation->asts, call, values[i], generation->span, &call
			) != 0) {
			return -1;
		}
	}
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	uint32_t output;
	if (output_binder == PROTOTYPE_INVALID_ID || output_symbol < 0 ||
		prototype_ast_var(
			generation->asts,
			output_binder,
			output_symbol,
			generation->span,
			&output
		) != 0) {
		return -1;
	}
	uint32_t graph_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		graph_arguments[i] = values[i];
	}
	graph_arguments[generation->argument_count] = output;
	uint32_t graph;
	uint32_t result;
	if (function_graph_graph_constructor(
			generation,
			generation->executed_symbol,
			graph_arguments,
			generation->argument_count + 1,
			&graph
		) != 0 || function_graph_coarse_result_constructor(
			generation, values, output, graph, &result
		) != 0) {
		return -1;
	}
	uint32_t binding_item;
	uint32_t result_item;
	uint32_t block;
	if (prototype_ast_block_binding(
			generation->asts,
			output_binder,
			output_symbol,
			PROTOTYPE_INVALID_ID,
			call,
			generation->span,
			&binding_item
		) != 0 || prototype_ast_block_expression(
			generation->asts, result, generation->span, &result_item
		) != 0) {
		return -1;
	}
	uint32_t items[2] = { binding_item, result_item };
	if (prototype_ast_computation_block(
			generation->asts,
			items,
			2,
			1,
			PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
			generation->span,
			&block
		) != 0) {
		return -1;
	}
	uint32_t runner;
	if (function_graph_wrap_lambdas(
			generation, binders, types, block, &runner
		) != 0 || function_graph_add_term_assignment(
			generation,
			generation->runner_symbol,
			runner,
			&generation->runner_assignment
		) != 0) {
		return -1;
	}
	return 0;
}

static int function_graph_generate_coarse_projection(
	struct function_graph_generation* generation
) {
	uint32_t binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t types[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t runner;
	if (function_graph_make_argument_context(
			generation, binders, types, values, maps
		) != 0 || prototype_ast_name(
			generation->asts,
			generation->runner_symbol,
			generation->span,
			&runner
		) != 0) {
		return -1;
	}
	uint32_t call = runner;
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		if (prototype_ast_app(
				generation->asts, call, values[i], generation->span, &call
			) != 0) {
			return -1;
		}
	}
	uint32_t packet_binder = prototype_ast_new_binder(generation->asts);
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
	int packet_symbol = symbol_intern(generation->symbols, "certified", 9);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	int graph_symbol = symbol_intern(generation->symbols, "graph", 5);
	uint32_t packet;
	uint32_t output;
	if (packet_binder == PROTOTYPE_INVALID_ID || output_binder == PROTOTYPE_INVALID_ID ||
		graph_binder == PROTOTYPE_INVALID_ID || packet_symbol < 0 || output_symbol < 0 ||
		graph_symbol < 0 || prototype_ast_var(
			generation->asts, packet_binder, packet_symbol, generation->span, &packet
		) != 0 || prototype_ast_var(
			generation->asts, output_binder, output_symbol, generation->span, &output
		) != 0) {
		return -1;
	}
	struct prototype_ast_binder returned_binders[2] = {
		{ .ast_binder_id = output_binder, .symbol_id = output_symbol },
		{ .ast_binder_id = graph_binder, .symbol_id = graph_symbol }
	};
	struct prototype_ast_match_case_input returned_case = {
		.constructor_symbol_id = generation->returned_symbol,
		.binders = returned_binders,
		.binder_count = 2,
		.body = output,
		.span = generation->span
	};
	uint32_t packet_match;
	uint32_t binding_item;
	uint32_t result_item;
	uint32_t block;
	if (prototype_ast_match(
			generation->asts,
			packet,
			&returned_case,
			1,
			generation->span,
			&packet_match
		) != 0 || prototype_ast_block_binding(
			generation->asts,
			packet_binder,
			packet_symbol,
			PROTOTYPE_INVALID_ID,
			call,
			generation->span,
			&binding_item
		) != 0 || prototype_ast_block_expression(
			generation->asts, packet_match, generation->span, &result_item
		) != 0) {
		return -1;
	}
	uint32_t items[2] = { binding_item, result_item };
	if (prototype_ast_computation_block(
			generation->asts,
			items,
			2,
			1,
			PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
			generation->span,
			&block
		) != 0) {
		return -1;
	}
	uint32_t projection;
	if (function_graph_wrap_lambdas(
			generation, binders, types, block, &projection
		) != 0) {
		return -1;
	}
	return function_graph_add_term_assignment(
		generation,
		generation->executable_symbol,
		projection,
		&generation->executable_assignment
	);
}

static int function_graph_generate_one(
	struct function_graph_generation* generation,
	uint32_t request_id
) {
	struct prototype_function_graph_request* request =
		&generation->metadata->function_graph_requests[request_id];
	int prepare = function_graph_prepare(generation, request->owner_assignment_id);
	if (prepare != 0) {
		fprintf(stderr,
			"function graph prepare failed owner=%d assignment=%u status=%d\n",
			request->owner_symbol_id, request->owner_assignment_id, prepare);
		request->state = prepare > 0 ? PROTOTYPE_FUNCTION_GRAPH_REQUEST_RESIDUAL :
			PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = prepare > 0 ?
			PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE :
			(generation->view.final_totality ==
				PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE ?
				PROTOTYPE_FUNCTION_GRAPH_REASON_NONTOTAL_OWNER :
				PROTOTYPE_FUNCTION_GRAPH_REASON_EFFECTFUL_OWNER);
		return -1;
	}
	int graph_status = generation->branch_precise ?
		function_graph_generate_graph_type(generation) :
		function_graph_generate_coarse_graph_type(generation);
	if (graph_status != 0) {
		fprintf(stderr, "function graph family generation failed owner=%d\n",
			request->owner_symbol_id);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	int result_status = generation->branch_precise ?
		function_graph_generate_result_type(generation) :
		function_graph_generate_coarse_result_type(generation);
	if (result_status != 0) {
		fprintf(stderr, "function graph result generation failed owner=%d\n",
			request->owner_symbol_id);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	int runner_status = generation->branch_precise ?
		function_graph_generate_runner(generation) :
		function_graph_generate_coarse_runner(generation);
	if (runner_status != 0) {
		fprintf(stderr, "function graph runner generation failed owner=%d\n",
			request->owner_symbol_id);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	int projection_status = generation->branch_precise ?
		function_graph_generate_projection(generation) :
		function_graph_generate_coarse_projection(generation);
	if (projection_status != 0) {
		fprintf(stderr, "function graph projection generation failed owner=%d\n",
			request->owner_symbol_id);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	struct prototype_function_graph_association association = {
		.owner_symbol_id = request->owner_symbol_id,
		.owner_assignment_id = request->owner_assignment_id,
		.owner_source_entry_id = request->owner_source_entry_id,
		.graph_symbol_id = generation->graph_symbol,
		.result_symbol_id = generation->result_symbol,
		.returned_constructor_symbol_id = generation->returned_symbol,
		.certified_runner_symbol_id = generation->runner_symbol,
		.graph_type_id = PROTOTYPE_INVALID_ID,
		.result_type_id = PROTOTYPE_INVALID_ID,
		.graph_type_assignment_id = generation->graph_assignment,
		.result_type_assignment_id = generation->result_assignment,
		.certified_runner_assignment_id = generation->runner_assignment,
		.executable_assignment_id = generation->executable_assignment
	};
	uint32_t association_id;
	if (prototype_compile_metadata_add_function_graph_association(
			generation->metadata, association, &association_id
		) != 0) {
		return -1;
	}
	(void)association_id;
	function_graph_unpublish_owner(
		generation->metadata, generation->executable_symbol
	);
	request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_GENERATED;
	request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_NONE;
	return 0;
}

static int accepted_definition_final_computation(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_result,
	uint32_t* p_effect_row,
	int* p_totality
) {
	if (!terms || !type_declarations || !p_result || !p_effect_row || !p_totality) {
		return -1;
	}
	uint32_t current = classifier;
	for (uint32_t depth = 0; depth < 64 && current < terms->term_count; ++depth) {
		struct prototype_term_normalization_result normalized;
		if (prototype_term_normalize_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				current,
				10000,
				&normalized
			) != 0 || normalized.status !=
				PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			return -1;
		}
		current = normalized.term_id;
		const struct prototype_term* term = &terms->terms[current];
		if (term->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
			current = term->as.effect_row_forall.body;
			continue;
		}
		if (term->tag == PROTOTYPE_TERM_PI) {
			uint32_t ignored_binder;
			if (prototype_term_pure_family_parts(
					terms, term->as.pi.codomain_family,
					&ignored_binder, &current
				) != 0) {
				return -1;
			}
			continue;
		}
		if (term->tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return -1;
		}
		*p_result = term->as.computation_type.result;
		*p_effect_row = term->as.computation_type.label;
		*p_totality = term->as.computation_type.totality;
		return 0;
	}
	return -1;
}

static int definition_root_has_accepted_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t occurrence_id,
	const struct prototype_typed_occurrence* occurrence
) {
	if (!judgement || !occurrence) {
		return 0;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (proposition && proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->occurrence_id == occurrence_id &&
			proposition->context_id == occurrence->context_id &&
			proposition->subject == occurrence->core_term &&
			proposition->classifier == occurrence->classifier) {
			return 1;
		}
	}
	return 0;
}

int prototype_accepted_definition_view_open(
	const struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	uint32_t assignment_id,
	struct prototype_accepted_definition_view* p_view
) {
	if (!asts || !terms || !type_declarations || !judgement || !metadata ||
		!p_view || assignment_id >= asts->assignment_count ||
		!metadata->typed_occurrences.frozen ||
		!metadata->typed_occurrences.sealed ||
		metadata->typed_occurrences.transaction_active) {
		return -1;
	}
	const struct prototype_ast_term_assignment_def* assignment =
		&asts->assignments[assignment_id];
	if (!assignment->compiled || !assignment->published ||
		assignment->compiled_operation >=
			metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* occurrence =
		&metadata->typed_occurrences.occurrences[assignment->compiled_operation];
	if (occurrence->classifier == PROTOTYPE_INVALID_ID ||
		assignment->compiled_classifier != occurrence->classifier ||
		!definition_root_has_accepted_claim(
			judgement,
			assignment->compiled_operation,
			occurrence
		)) {
		return -1;
	}
	struct prototype_accepted_definition_view view;
	memset(&view, 0, sizeof(view));
	view.asts = asts;
	view.terms = terms;
	view.type_declarations = type_declarations;
	view.judgement = judgement;
	view.metadata = metadata;
	view.assignment_id = assignment_id;
	view.source_entry_id = assignment->source_entry_id;
	view.root_ast = assignment->ast;
	view.root_occurrence = assignment->compiled_operation;
	view.classifier = assignment->compiled_classifier;
	if (accepted_definition_final_computation(
			terms,
			type_declarations,
			view.classifier,
			&view.final_result_type,
			&view.final_effect_row,
			&view.final_totality
		) != 0) {
		return -1;
	}
	*p_view = view;
	return 0;
}

int prototype_function_graph_generate_requested(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols
) {
	if (!asts || !terms || !type_declarations || !judgement || !metadata ||
		!symbols || !metadata->typed_occurrences.frozen ||
		metadata->function_graph_association_count != 0) {
		return -1;
	}
	uint32_t request_count = (uint32_t)metadata->function_graph_request_count;
	for (uint32_t request_id = 0; request_id < request_count; ++request_id) {
		struct function_graph_generation generation;
		memset(&generation, 0, sizeof(generation));
		generation.asts = asts;
		generation.terms = terms;
		generation.type_declarations = type_declarations;
		generation.judgement = judgement;
		generation.metadata = metadata;
		generation.symbols = symbols;
		if (function_graph_generate_one(&generation, request_id) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_function_graph_finalize_associations(
	struct prototype_ast_db* asts,
	struct prototype_compile_metadata* metadata
) {
	if (!asts || !metadata) {
		return -1;
	}
	for (size_t association_id = 0;
		association_id < metadata->function_graph_association_count;
		++association_id) {
		struct prototype_function_graph_association* association =
			&metadata->function_graph_associations[association_id];
		association->graph_type_id = PROTOTYPE_INVALID_ID;
		association->result_type_id = PROTOTYPE_INVALID_ID;
		for (size_t type_id = 0; type_id < asts->type_def_count; ++type_id) {
			const struct prototype_ast_type_def* type = &asts->type_defs[type_id];
			if (!type->compiled || type->compiled_type == PROTOTYPE_INVALID_ID) {
				continue;
			}
			if (type->name_symbol_id == association->graph_symbol_id) {
				association->graph_type_id = type->compiled_type;
			} else if (type->name_symbol_id == association->result_symbol_id) {
				association->result_type_id = type->compiled_type;
			}
		}
		if (association->graph_type_id == PROTOTYPE_INVALID_ID ||
			association->result_type_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		if (association->owner_assignment_id >= asts->assignment_count ||
			association->executable_assignment_id >= asts->assignment_count) {
			return -1;
		}
		struct prototype_ast_term_assignment_def* owner =
			&asts->assignments[association->owner_assignment_id];
		const struct prototype_ast_term_assignment_def* executable =
			&asts->assignments[association->executable_assignment_id];
		if (!executable->compiled || !executable->published) {
			return -1;
		}
		struct prototype_compile_label executable_label;
		int found_executable_label = 0;
		int found_owner_label = 0;
		for (size_t label_id = 0; label_id < metadata->label_count; ++label_id) {
			if (metadata->labels[label_id].name_symbol_id ==
					executable->name_symbol_id) {
				if (found_executable_label) {
					return -1;
				}
				executable_label = metadata->labels[label_id];
				found_executable_label = 1;
			}
		}
		if (!found_executable_label) {
			return -1;
		}
		for (size_t label_id = 0; label_id < metadata->label_count; ++label_id) {
			if (metadata->labels[label_id].name_symbol_id !=
					association->owner_symbol_id) {
				continue;
			}
			if (found_owner_label) {
				return -1;
			}
			metadata->labels[label_id] = executable_label;
			metadata->labels[label_id].name_symbol_id = association->owner_symbol_id;
			found_owner_label = 1;
		}
		if (!found_owner_label) {
			return -1;
		}
		owner->compiled_term = executable->compiled_term;
		owner->compiled_classifier = executable->compiled_classifier;
		owner->compiled_operation = executable->compiled_operation;
		owner->compiling = 0;
		owner->compiled = 1;
		owner->published = 1;
		owner->definition_value_required = executable->definition_value_required;
		function_graph_unpublish_owner(metadata, executable->name_symbol_id);
	}
	return 0;
}
