#include "a_program/frontend/function_graph.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FUNCTION_GRAPH_MAX_BINDINGS 32
#define FUNCTION_GRAPH_MAX_RECURSIVE_CALLS 16
#define FUNCTION_GRAPH_MAX_ARGUMENTS 16
#define FUNCTION_GRAPH_MAX_ORIGIN_GROUPS 256
#define FUNCTION_GRAPH_MAX_TERMINAL_BINDINGS 16

struct function_graph_binding_map {
	uint32_t source_ast_binder;
	uint32_t source_binding;
	uint32_t target_ast_binder;
	uint32_t target_value;
	int symbol_id;
};

static int prototype_accepted_definition_view_open(
	const struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_compile_metadata* metadata,
	uint32_t assignment_id,
	struct prototype_accepted_definition_view* p_view
);

struct function_graph_recursive_call {
	uint32_t source_call_ast;
	uint32_t source_ih_ast;
	uint32_t source_argument_ast_binder;
	int argument_symbol_id;
	uint32_t output_ast_binder;
	uint32_t graph_ast_binder;
	int output_symbol_id;
	int graph_symbol_id;
};

struct function_graph_recursive_site {
	uint32_t call_ast;
	uint32_t ih_ast;
	uint32_t explicit_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t explicit_argument_count;
};

struct function_graph_named_call_site {
	uint32_t call_ast;
	int owner_symbol_id;
	uint32_t owner_assignment_id;
	uint32_t arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t argument_count;
};

struct function_graph_generation {
	struct prototype_ast_db* asts;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_compile_metadata* metadata;
	struct symbol_table* symbols;
	uint32_t accepted_source_node_count;
	struct prototype_accepted_definition_view view;
	struct prototype_source_span span;
	int owner_symbol;
	int graph_symbol;
	int result_symbol;
	int returned_symbol;
	int interface_symbol;
	int adapter_symbol;
	int runner_symbol;
	int executable_symbol;
	uint32_t graph_type_def;
	uint32_t result_type_def;
	uint32_t graph_assignment;
	uint32_t result_assignment;
	uint32_t interface_assignment;
	uint32_t adapter_assignment;
	uint32_t runner_assignment;
	uint32_t executable_assignment;
	uint32_t owner_source_lambda;
	uint32_t owner_source_match;
	uint32_t owner_recursive_match;
	uint32_t owner_source_input_binder;
	uint32_t owner_source_input_binding;
	uint32_t owner_input_classifier;
	uint32_t runner_input_binder;
	int runner_input_symbol;
	uint32_t source_body;
	uint32_t argument_count;
	uint32_t root_argument_count;
	uint32_t joined_argument_count;
	uint32_t match_argument_index;
	uint32_t graph_parameter_count;
	int root_helper_match;
	struct function_graph_named_call_site root_helper;
	uint32_t runner_root_helper_graph_value;
	uint32_t certified_argument_index;
	uint32_t certified_domain_classifier;
	int certified_package_symbol;
	uint32_t certified_package_assignment;
	uint32_t graph_interface_ast_binder;
	int graph_interface_symbol;
	uint32_t graph_interface_value;
	uint32_t runner_interface_value;
	uint32_t runner_callback_value;
	uint32_t source_argument_ast_binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t source_argument_bindings[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t source_argument_classifiers[FUNCTION_GRAPH_MAX_ARGUMENTS];
	int source_argument_symbols[FUNCTION_GRAPH_MAX_ARGUMENTS];
	int branch_precise;
	int nested_recursive;
	struct function_graph_recursive_site recursive_sites[
		FUNCTION_GRAPH_MAX_RECURSIVE_CALLS
	];
	uint32_t recursive_site_count;
	struct prototype_function_graph_origin_group origin_groups[
		FUNCTION_GRAPH_MAX_ORIGIN_GROUPS
	];
	uint32_t origin_group_count;
	int failure_reason;
};

struct function_graph_binary_classifier {
	uint32_t domain;
	uint32_t first_binder;
	uint32_t second_domain;
	uint32_t second_binder;
	uint32_t result;
};

struct function_graph_certified_block {
	uint32_t source_block_binder;
	int source_block_symbol;
	uint32_t callback_arguments[2];
	struct function_graph_named_call_site helper;
};

struct function_graph_terminal_binding {
	uint32_t item_ast;
	uint32_t source_ast_binder;
	int source_symbol_id;
	uint32_t value_ast;
};

struct function_graph_terminal_plan {
	uint32_t body_ast;
	struct function_graph_terminal_binding bindings[
		FUNCTION_GRAPH_MAX_TERMINAL_BINDINGS
	];
	uint32_t binding_count;
};

static const struct function_graph_recursive_site*
function_graph_recursive_site_for_ih(
	const struct function_graph_generation* generation,
	uint32_t ih_ast
);

static int function_graph_value_type_expr(
	struct function_graph_generation* generation,
	uint32_t value_ast,
	uint32_t* p_type
);

static int function_graph_occurrence_is_computation(
	struct function_graph_generation* generation,
	uint32_t source_ast
);

static int accepted_definition_final_computation(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_result,
	uint32_t* p_effect_row,
	int* p_totality
);

static int function_graph_generate_one(
	struct function_graph_generation* generation,
	uint32_t request_id
);

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

static const struct prototype_typed_occurrence* function_graph_any_occurrence(
	const struct prototype_compile_metadata* metadata,
	uint32_t source_ast
) {
	if (!metadata) {
		return NULL;
	}
	for (size_t i = 0; i < metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* occurrence =
			&metadata->typed_occurrences.occurrences[i];
		if (occurrence->source_ast == source_ast) {
			return occurrence;
		}
	}
	return NULL;
}

static int function_graph_named_call_site_open(
	struct function_graph_generation* generation,
	uint32_t call_ast,
	struct function_graph_named_call_site* p_site
) {
	if (!generation || !p_site || call_ast >= generation->asts->node_count) {
		return -1;
	}
	uint32_t reversed[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t count = 0;
	uint32_t current = call_ast;
	while (current < generation->asts->node_count &&
		generation->asts->nodes[current].tag == PROTOTYPE_AST_APP) {
		if (count >= FUNCTION_GRAPH_MAX_ARGUMENTS) {
			return -1;
		}
		reversed[count++] = generation->asts->nodes[current].as.app.argument;
		current = generation->asts->nodes[current].as.app.function;
	}
	if (current >= generation->asts->node_count ||
		generation->asts->nodes[current].tag != PROTOTYPE_AST_NAME) {
		return 0;
	}
	int owner_symbol = generation->asts->nodes[current].as.name.symbol_id;
	uint32_t assignment_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < generation->asts->assignment_count; ++i) {
		const struct prototype_ast_term_assignment_def* assignment =
			&generation->asts->assignments[i];
		if (assignment->name_symbol_id == owner_symbol && assignment->compiled &&
			assignment->published) {
			assignment_id = i;
			break;
		}
	}
	if (assignment_id == PROTOTYPE_INVALID_ID || owner_symbol ==
		generation->owner_symbol || !function_graph_occurrence_is_computation(
			generation, call_ast
		)) {
		return 0;
	}
	*p_site = (struct function_graph_named_call_site) {
		.call_ast = call_ast,
		.owner_symbol_id = owner_symbol,
		.owner_assignment_id = assignment_id,
		.argument_count = count
	};
	for (uint32_t i = 0; i < count; ++i) {
		p_site->arguments[i] = reversed[count - i - 1];
	}
	return 1;
}

static int function_graph_request_named_call(
	struct function_graph_generation* generation,
	const struct function_graph_named_call_site* site,
	uint32_t* p_request_id
) {
	if (!generation || !site || site->owner_assignment_id >=
		generation->asts->assignment_count) {
		return -1;
	}
	const struct prototype_ast_term_assignment_def* assignment =
		&generation->asts->assignments[site->owner_assignment_id];
	uint32_t request_id;
	uint32_t* result_id = p_request_id ? p_request_id : &request_id;
	return prototype_compile_metadata_request_function_graph(
		generation->metadata,
		site->owner_symbol_id,
		site->owner_assignment_id,
		assignment->source_entry_id,
		PROTOTYPE_FUNCTION_GRAPH_REQUEST_FAMILY |
			PROTOTYPE_FUNCTION_GRAPH_REQUEST_CERTIFIED_EXECUTION,
		site->call_ast,
		result_id
	);
}

static int function_graph_quoted_owner_symbol(
	const struct prototype_ast_db* asts,
	uint32_t ast,
	int* p_symbol
) {
	if (!asts || !p_symbol || ast >= asts->node_count ||
		asts->nodes[ast].tag != PROTOTYPE_AST_QUOTE) {
		return 0;
	}
	uint32_t inner = asts->nodes[ast].as.unary.term;
	if (inner >= asts->node_count || asts->nodes[inner].tag != PROTOTYPE_AST_NAME) {
		return 0;
	}
	*p_symbol = asts->nodes[inner].as.name.symbol_id;
	return 1;
}

/* A source use `*f ... &g ...` selects one exact proof-side interface for the
 * higher-order argument. Request that interface before the generated closure
 * is compiled. Anonymous computations remain residual rather than being paired
 * with an unrelated graph family. */
static int function_graph_request_certified_source_dependencies(
	struct function_graph_generation* generation
) {
	if (!generation || generation->certified_argument_index == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	for (uint32_t ast = 0; ast < generation->accepted_source_node_count; ++ast) {
		if (generation->asts->nodes[ast].tag != PROTOTYPE_AST_APP) {
			continue;
		}
		uint32_t arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
		uint32_t argument_count = 0;
		uint32_t head = ast;
		while (head < generation->asts->node_count &&
			generation->asts->nodes[head].tag == PROTOTYPE_AST_APP) {
			if (argument_count >= FUNCTION_GRAPH_MAX_ARGUMENTS) {
				fprintf(stderr,
					"function graph certified dependency spine exceeds %u arguments owner=%d ast=%u\n",
					FUNCTION_GRAPH_MAX_ARGUMENTS, generation->owner_symbol, ast);
				return -1;
			}
			arguments[argument_count++] =
				generation->asts->nodes[head].as.app.argument;
			head = generation->asts->nodes[head].as.app.function;
		}
		if (head >= generation->asts->node_count ||
			generation->asts->nodes[head].tag !=
				PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE ||
			generation->asts->nodes[head].as.certified_function_reference.
				owner_symbol_id != generation->owner_symbol ||
			argument_count <= generation->certified_argument_index) {
			continue;
		}
		uint32_t reversed_index = argument_count -
			generation->certified_argument_index - 1;
		int dependency_symbol;
		if (!function_graph_quoted_owner_symbol(
				generation->asts, arguments[reversed_index], &dependency_symbol
			)) {
			continue;
		}
		if (prototype_compile_metadata_function_graph_association_for_owner(
				generation->metadata, dependency_symbol
			)) {
			continue;
		}
		const struct prototype_ast_term_assignment_def* dependency = NULL;
		for (uint32_t i = 0; i < generation->asts->assignment_count; ++i) {
			if (generation->asts->assignments[i].name_symbol_id != dependency_symbol ||
				!generation->asts->assignments[i].published) {
				continue;
			}
			if (dependency) {
				fprintf(stderr,
					"function graph certified dependency is ambiguous owner=%d dependency=%d\n",
					generation->owner_symbol, dependency_symbol);
				return -1;
			}
			dependency = &generation->asts->assignments[i];
		}
		if (!dependency) {
			fprintf(stderr,
				"function graph certified dependency is unavailable owner=%d dependency=%d\n",
				generation->owner_symbol, dependency_symbol);
			return -1;
		}
		uint32_t request_id;
		if (prototype_compile_metadata_request_function_graph(
				generation->metadata,
				dependency_symbol,
				(uint32_t)(dependency - generation->asts->assignments),
				dependency->source_entry_id,
				PROTOTYPE_FUNCTION_GRAPH_REQUEST_FAMILY |
					PROTOTYPE_FUNCTION_GRAPH_REQUEST_CERTIFIED_EXECUTION,
				arguments[reversed_index],
				&request_id
			) != 0) {
			fprintf(stderr,
				"function graph certified dependency request failed owner=%d dependency=%d assignment=%u\n",
				generation->owner_symbol, dependency_symbol,
				(uint32_t)(dependency - generation->asts->assignments));
			return -1;
		}
	}
	return 0;
}

static int function_graph_certified_block_open(
	struct function_graph_generation* generation,
	uint32_t ast,
	struct function_graph_certified_block* p_block
) {
	if (!generation || !p_block || ast >= generation->asts->node_count ||
		generation->certified_argument_index == PROTOTYPE_INVALID_ID ||
		generation->asts->nodes[ast].tag != PROTOTYPE_AST_COMPUTATION_BLOCK) {
		return 0;
	}
	const struct prototype_ast_node* block = &generation->asts->nodes[ast];
	if (block->as.block.item_count != 2 || block->as.block.result_item_index != 1) {
		return 0;
	}
	uint32_t binding_ast = generation->asts->block_items[block->as.block.first_item];
	uint32_t expression_ast = generation->asts->block_items[
		block->as.block.first_item + 1
	];
	if (binding_ast >= generation->asts->node_count ||
		expression_ast >= generation->asts->node_count ||
		generation->asts->nodes[binding_ast].tag != PROTOTYPE_AST_BLOCK_BINDING ||
		generation->asts->nodes[expression_ast].tag !=
			PROTOTYPE_AST_BLOCK_EXPRESSION) {
		return 0;
	}
	const struct prototype_ast_node* binding = &generation->asts->nodes[binding_ast];
	uint32_t current = binding->as.block_binding.value;
	uint32_t reversed[2];
	uint32_t count = 0;
	while (current < generation->asts->node_count &&
		generation->asts->nodes[current].tag == PROTOTYPE_AST_APP && count < 2) {
		reversed[count++] = generation->asts->nodes[current].as.app.argument;
		current = generation->asts->nodes[current].as.app.function;
	}
	if (count != 2 || current >= generation->asts->node_count ||
		generation->asts->nodes[current].tag != PROTOTYPE_AST_VAR ||
		generation->asts->nodes[current].as.var.ast_binder_id !=
			generation->source_argument_ast_binders[
				generation->certified_argument_index
			]) {
		return 0;
	}
	struct function_graph_named_call_site helper;
	int helper_status = function_graph_named_call_site_open(
		generation,
		generation->asts->nodes[expression_ast].as.block_expression.term,
		&helper
	);
	if (helper_status <= 0) {
		return helper_status;
	}
	*p_block = (struct function_graph_certified_block) {
		.source_block_binder = binding->as.block_binding.ast_binder_id,
		.source_block_symbol = binding->as.block_binding.binder_symbol_id,
		.callback_arguments = { reversed[1], reversed[0] },
		.helper = helper
	};
	return 1;
}

static int function_graph_occurrence_is_computation(
	struct function_graph_generation* generation,
	uint32_t source_ast
) {
	const struct prototype_typed_occurrence* occurrence =
		function_graph_any_occurrence(generation->metadata, source_ast);
	if (!occurrence || occurrence->classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t current = occurrence->classifier;
	for (uint32_t depth = 0; depth < 8; ++depth) {
		struct prototype_term_normalization_result normalized;
		if (prototype_term_normalize_with_profile(
				generation->terms,
				generation->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				current,
				10000,
				&normalized
			) != 0 || normalized.status !=
				PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			normalized.term_id >= generation->terms->term_count) {
			return 0;
		}
		current = normalized.term_id;
		const struct prototype_term* classifier =
			&generation->terms->terms[current];
		if (classifier->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
			current = classifier->as.effect_row_forall.body;
			continue;
		}
		return classifier->tag == PROTOTYPE_TERM_COMPUTATION_TYPE;
	}
	return 0;
}

static int function_graph_ih_spine(
	const struct prototype_ast_db* asts,
	uint32_t ast,
	uint32_t* p_ih_ast,
	uint32_t* arguments,
	uint32_t* p_argument_count
) {
	if (!asts || !p_ih_ast || !arguments || !p_argument_count) {
		return -1;
	}
	uint32_t reversed[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t count = 0;
	uint32_t current = ast;
	while (current < asts->node_count &&
		asts->nodes[current].tag == PROTOTYPE_AST_APP) {
		if (count >= FUNCTION_GRAPH_MAX_ARGUMENTS) {
			return -1;
		}
		reversed[count++] = asts->nodes[current].as.app.argument;
		current = asts->nodes[current].as.app.function;
	}
	if (current >= asts->node_count ||
		asts->nodes[current].tag != PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		return 0;
	}
	for (uint32_t i = 0; i < count; ++i) {
		arguments[i] = reversed[count - i - 1];
	}
	*p_ih_ast = current;
	*p_argument_count = count;
	return 1;
}

static int function_graph_collect_recursive_sites(
	struct function_graph_generation* generation,
	uint32_t ast
) {
	if (!generation || ast >= generation->asts->node_count) {
		return -1;
	}
	uint32_t ih_ast;
	uint32_t arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t argument_count = 0;
	int spine = function_graph_ih_spine(
		generation->asts, ast, &ih_ast, arguments, &argument_count
	);
	if (spine < 0) {
		return -1;
	}
	if (spine > 0 && function_graph_occurrence_is_computation(generation, ast)) {
		if (generation->recursive_site_count >=
				FUNCTION_GRAPH_MAX_RECURSIVE_CALLS) {
			return -1;
		}
		struct function_graph_recursive_site* site =
			&generation->recursive_sites[generation->recursive_site_count++];
		site->call_ast = ast;
		site->ih_ast = ih_ast;
		site->explicit_argument_count = argument_count;
		for (uint32_t i = 0; i < argument_count; ++i) {
			site->explicit_arguments[i] = arguments[i];
		}
		return 0;
	}
	const struct prototype_ast_node* node = &generation->asts->nodes[ast];
	switch (node->tag) {
		case PROTOTYPE_AST_APP:
			if (function_graph_collect_recursive_sites(
					generation, node->as.app.function
				) != 0) {
				return -1;
			}
			return function_graph_collect_recursive_sites(
				generation, node->as.app.argument
			);
		case PROTOTYPE_AST_LAMBDA:
			return function_graph_collect_recursive_sites(
				generation, node->as.lambda.body
			);
		case PROTOTYPE_AST_MATCH:
			if (function_graph_collect_recursive_sites(
					generation, node->as.match.scrutinee
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				const struct prototype_ast_match_case* source_case =
					&generation->asts->cases[node->as.match.first_case + i];
				if (function_graph_collect_recursive_sites(
						generation, source_case->body
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_AST_COMPUTATION_BLOCK:
			for (uint32_t i = 0; i < node->as.block.item_count; ++i) {
				uint32_t item = generation->asts->block_items[
					node->as.block.first_item + i
				];
				if (function_graph_collect_recursive_sites(
						generation, item
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_AST_BLOCK_BINDING:
			return function_graph_collect_recursive_sites(
				generation, node->as.block_binding.value
			);
		case PROTOTYPE_AST_BLOCK_EXPRESSION:
			return function_graph_collect_recursive_sites(
				generation, node->as.block_expression.term
			);
		case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
			return function_graph_collect_recursive_sites(
				generation, node->as.block_lambda_exit.value
			);
		case PROTOTYPE_AST_COMPUTATION_FOLD:
			if (function_graph_collect_recursive_sites(
					generation, node->as.computation_fold.computation
				) != 0 || function_graph_collect_recursive_sites(
					generation, node->as.computation_fold.return_body
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < node->as.computation_fold.clause_count; ++i) {
				const struct prototype_ast_computation_fold_clause* clause =
					&generation->asts->computation_fold_clauses[
						node->as.computation_fold.first_clause + i
					];
				if (function_graph_collect_recursive_sites(
						generation, clause->operation
					) != 0 || function_graph_collect_recursive_sites(
						generation, clause->body
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_AST_ASCRIPTION:
			return function_graph_collect_recursive_sites(
				generation, node->as.ascription.term
			);
		case PROTOTYPE_AST_QUOTE:
			return function_graph_collect_recursive_sites(
				generation, node->as.unary.term
			);
		case PROTOTYPE_AST_TERMINATES_WITNESS:
			return function_graph_collect_recursive_sites(
				generation, node->as.terminates_witness.computation
			);
		case PROTOTYPE_AST_VAR:
		case PROTOTYPE_AST_NAME:
		case PROTOTYPE_AST_NAME_IN_NAMESPACE:
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE:
		case PROTOTYPE_AST_TEXT_LITERAL:
		case PROTOTYPE_AST_INT_LITERAL:
		case PROTOTYPE_AST_SYSTEM_NAME:
		case PROTOTYPE_AST_TYPE_LITERAL:
		case PROTOTYPE_AST_TYPE_FORMATION:
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
		case PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE:
			return 0;
		default:
			return -1;
	}
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

static int function_graph_substitution_type(
	struct function_graph_generation* generation,
	uint32_t term,
	const uint32_t* source_bindings,
	const uint32_t* target_values,
	uint32_t binding_count,
	uint32_t* p_type
) {
	if (!generation || !p_type ||
		(binding_count != 0 && (!source_bindings || !target_values)) ||
		binding_count > FUNCTION_GRAPH_MAX_BINDINGS) {
		return -1;
	}
	/* The certified graph fragment accepts only total computations with an empty
	 * final effect row. Source binder classifiers can still retain the latent-row
	 * metavariable introduced by an unannotated arrow. Close that elaboration
	 * variable with the accepted empty row before embedding the classifier in a
	 * generated declaration. Quantifying here would change
	 * `forall e. (f : F e) -> ...` into `(f : forall e. F e) -> ...`. */
	const uint32_t* effect_row_variables;
	size_t effect_row_variable_count;
	uint32_t empty_effect_row;
	if (prototype_term_effect_row_variable_terms(
			generation->terms,
			&effect_row_variables,
			&effect_row_variable_count
		) != 0 || prototype_term_effect_row_empty(
			generation->terms, &empty_effect_row
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < effect_row_variable_count; ++i) {
		uint32_t variable = effect_row_variables[i];
		if (variable >= generation->terms->term_count ||
			generation->terms->terms[variable].tag !=
				PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			return -1;
		}
		uint32_t binding_id = generation->terms->terms[
			variable
		].as.effect_row_var.binding_id;
		if (!prototype_term_contains_free_binding(
				generation->terms, term, binding_id
			)) {
			continue;
		}
		if (prototype_term_graph_substitute_bound_var(
				generation->terms, prototype_type_view_rebuild_context_from_db(generation->type_declarations),
				term,
				binding_id,
				empty_effect_row,
				&term
			) != 0) {
			return -1;
		}
	}
	struct prototype_ast_accepted_binding_substitution substitutions[
		FUNCTION_GRAPH_MAX_BINDINGS
	];
	uint32_t substitution_count = 0;
	for (uint32_t i = 0; i < binding_count; ++i) {
		if (!prototype_term_contains_free_binding(
				generation->terms, term, source_bindings[i]
			)) {
			continue;
		}
		substitutions[substitution_count++] =
			(struct prototype_ast_accepted_binding_substitution) {
				.source_binding_id = source_bindings[i],
				.target_value = target_values[i]
			};
	}
	return prototype_ast_type_expr_accepted_substitution(
		generation->asts,
		term,
		substitutions,
		substitution_count,
		generation->span,
		p_type
	);
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
	uint32_t source_bindings[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t target_values[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t substitution_count = 0;
	for (uint32_t i = 0; i < binding_count; ++i) {
		if (!prototype_term_contains_free_binding(
				generation->terms, term, bindings[i].source_binding
			)) {
			continue;
		}
		source_bindings[substitution_count] = bindings[i].source_binding;
		if (bindings[i].target_value != PROTOTYPE_INVALID_ID) {
			target_values[substitution_count] = bindings[i].target_value;
		} else if (prototype_ast_var(
				generation->asts,
				bindings[i].target_ast_binder,
				bindings[i].symbol_id,
				generation->span,
				&target_values[substitution_count]
			) != 0) {
			return -1;
		}
		substitution_count++;
	}
	return function_graph_substitution_type(
		generation, term, source_bindings, target_values,
		substitution_count, p_type
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

static int function_graph_type_graph_reference_app(
	struct function_graph_generation* generation,
	int owner_symbol,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_type
) {
	uint32_t type;
	if (!generation || !p_type || prototype_ast_type_expr_function_graph_reference(
			generation->asts, owner_symbol, generation->span, &type
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

static int function_graph_named_result_type_for_values(
	struct function_graph_generation* generation,
	int owner_symbol,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_type
) {
	uint32_t type_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
	const char* owner_name;
	int result_symbol;
	if (!generation || !arguments || !p_type ||
		argument_count > FUNCTION_GRAPH_MAX_ARGUMENTS) {
		return -1;
	}
	owner_name = symbol_to_string(generation->symbols, owner_symbol);
	if (!owner_name || function_graph_internal_symbol(
			generation->symbols, "result", owner_name, &result_symbol
		) != 0) {
		return -1;
	}
	const struct prototype_function_graph_association* association =
		prototype_compile_metadata_function_graph_association_for_owner(
			generation->metadata, owner_symbol
		);
	uint32_t type_argument_count = 0;
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (function_graph_value_type_expr(
				generation, arguments[i], &type_arguments[type_argument_count++]
			) != 0) {
			return -1;
		}
		if (association && i == association->certified_argument_index) {
			if (generation->runner_interface_value == PROTOTYPE_INVALID_ID ||
				function_graph_value_type_expr(
					generation, generation->runner_interface_value,
					&type_arguments[type_argument_count++]
				) != 0) {
				return -1;
			}
		}
	}
	return function_graph_type_name_app(
		generation, result_symbol, type_arguments, type_argument_count, p_type
	);
}

static int function_graph_named_graph_type_for_values(
	struct function_graph_generation* generation,
	int owner_symbol,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t output,
	uint32_t* p_type
) {
	uint32_t graph_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	if (!generation || !arguments || !p_type ||
		argument_count > FUNCTION_GRAPH_MAX_ARGUMENTS) {
		return -1;
	}
	const struct prototype_function_graph_association* association =
		prototype_compile_metadata_function_graph_association_for_owner(
			generation->metadata, owner_symbol
		);
	uint32_t graph_argument_count = 0;
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (function_graph_value_type_expr(
				generation, arguments[i], &graph_arguments[graph_argument_count++]
			) != 0) {
			return -1;
		}
		if (association && i == association->certified_argument_index) {
			if (generation->runner_interface_value == PROTOTYPE_INVALID_ID ||
				function_graph_value_type_expr(
					generation, generation->runner_interface_value,
					&graph_arguments[graph_argument_count++]
				) != 0) {
				return -1;
			}
		}
	}
	if (function_graph_value_type_expr(
			generation, output, &graph_arguments[graph_argument_count++]
		) != 0) {
		return -1;
	}
	return function_graph_type_graph_reference_app(
		generation, owner_symbol, graph_arguments, graph_argument_count, p_type
	);
}

static int function_graph_named_graph_ascription(
	struct function_graph_generation* generation,
	int owner_symbol,
	uint32_t graph,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t output,
	uint32_t* p_ascribed
) {
	uint32_t graph_type;
	if (!generation || !p_ascribed || function_graph_named_graph_type_for_values(
			generation, owner_symbol, arguments, argument_count, output, &graph_type
		) != 0) {
		return -1;
	}
	return prototype_ast_ascription(
		generation->asts, graph, graph_type, generation->span, p_ascribed
	);
}

static int function_graph_named_certified_call(
	struct function_graph_generation* generation,
	const struct function_graph_named_call_site* site,
	const uint32_t* arguments,
	uint32_t* p_call
) {
	if (!generation || !site || !arguments || !p_call ||
		prototype_ast_certified_function_reference(
			generation->asts, site->owner_symbol_id, generation->span, p_call
		) != 0) {
		return -1;
	}
	const struct prototype_function_graph_association* association =
		prototype_compile_metadata_function_graph_association_for_owner(
			generation->metadata, site->owner_symbol_id
		);
	if (association && association->certified_argument_index != PROTOTYPE_INVALID_ID) {
		uint32_t index = association->certified_argument_index;
		if (generation->certified_argument_index == PROTOTYPE_INVALID_ID ||
			index >= site->argument_count || site->arguments[index] >=
				generation->asts->node_count ||
			generation->asts->nodes[site->arguments[index]].tag != PROTOTYPE_AST_VAR ||
			generation->asts->nodes[site->arguments[index]].as.var.ast_binder_id !=
				generation->source_argument_ast_binders[
					generation->certified_argument_index
				] || generation->runner_interface_value == PROTOTYPE_INVALID_ID ||
			generation->runner_callback_value == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < site->argument_count; ++i) {
		if (prototype_ast_app(
				generation->asts, *p_call, arguments[i], generation->span, p_call
			) != 0) {
			return -1;
		}
		if (association && i == association->certified_argument_index &&
			(prototype_ast_app(
				generation->asts, *p_call, generation->runner_interface_value,
				generation->span, p_call
			) != 0 || prototype_ast_app(
				generation->asts, *p_call, generation->runner_callback_value,
				generation->span, p_call
			) != 0)) {
			return -1;
		}
	}
	return 0;
}

static int function_graph_result_type_for_values(
	struct function_graph_generation* generation,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_type
) {
	uint32_t type_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	if (!generation || !arguments || !p_type ||
		argument_count > FUNCTION_GRAPH_MAX_ARGUMENTS) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (function_graph_value_type_expr(
				generation, arguments[i], &type_arguments[i]
			) != 0) {
			return -1;
		}
	}
	return function_graph_type_name_app(
		generation,
		generation->result_symbol,
		type_arguments,
		argument_count,
		p_type
	);
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

static int function_graph_record_origin_group(
	struct function_graph_generation* generation,
	uint32_t constructor_ordinal,
	uint32_t source_ast_binder_id,
	int display_symbol_id,
	uint32_t role_mask,
	uint32_t value_field_ordinal,
	uint32_t graph_field_ordinal,
	int recursive
) {
	if (!generation || display_symbol_id < 0 || role_mask == 0 ||
		value_field_ordinal == PROTOTYPE_INVALID_ID ||
		generation->origin_group_count >= FUNCTION_GRAPH_MAX_ORIGIN_GROUPS) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->origin_group_count; ++i) {
		const struct prototype_function_graph_origin_group* existing =
			&generation->origin_groups[i];
		if (existing->constructor_ordinal == constructor_ordinal &&
			existing->display_symbol_id == display_symbol_id) {
			return -1;
		}
	}
	generation->origin_groups[generation->origin_group_count++] =
		(struct prototype_function_graph_origin_group) {
			.association_id = PROTOTYPE_INVALID_ID,
			.constructor_ordinal = constructor_ordinal,
			.source_ast_binder_id = source_ast_binder_id,
			.display_symbol_id = display_symbol_id,
			.role_mask = role_mask,
			.value_field_ordinal = value_field_ordinal,
			.graph_field_ordinal = graph_field_ordinal,
			.recursive = recursive
		};
	return 0;
}

static int function_graph_record_case_field_origins(
	struct function_graph_generation* generation,
	uint32_t constructor_ordinal,
	const struct prototype_ast_match_case* source_case,
	uint32_t first_field_ordinal
) {
	if (!generation || !source_case || source_case->first_binder +
		source_case->binder_count > generation->asts->case_binder_count) {
		return -1;
	}
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		const struct prototype_ast_binder* binder = &generation->asts->case_binders[
			source_case->first_binder + i
		];
		if (function_graph_record_origin_group(
				generation,
				constructor_ordinal,
				binder->ast_binder_id,
				binder->symbol_id,
				PROTOTYPE_FUNCTION_GRAPH_ORIGIN_VALUE,
				first_field_ordinal + i,
				PROTOTYPE_INVALID_ID,
				0
			) != 0) {
			return -1;
		}
	}
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

static struct function_graph_binding_map* function_graph_find_core_binding(
	struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t source_binding
) {
	for (uint32_t i = 0; i < binding_count; ++i) {
		if (bindings[i].source_binding == source_binding) {
			return &bindings[i];
		}
	}
	return NULL;
}

static int function_graph_binding_value(
	struct function_graph_generation* generation,
	const struct function_graph_binding_map* binding,
	uint32_t* p_value
) {
	if (!generation || !binding || !p_value) {
		return -1;
	}
	if (binding->target_value != PROTOTYPE_INVALID_ID) {
		*p_value = binding->target_value;
		return 0;
	}
	return prototype_ast_var(
		generation->asts,
		binding->target_ast_binder,
		binding->symbol_id,
		generation->span,
		p_value
	);
}

static const struct function_graph_recursive_call* function_graph_find_recursive(
	const struct function_graph_recursive_call* recursive,
	uint32_t recursive_count,
	uint32_t source_ast
) {
	for (uint32_t i = 0; i < recursive_count; ++i) {
		if (recursive[i].source_call_ast == source_ast ||
			recursive[i].source_ih_ast == source_ast) {
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
	const struct function_graph_recursive_call* exact_recursive =
		function_graph_find_recursive(recursive, recursive_count, source_ast);
	if (exact_recursive && exact_recursive->source_call_ast == source_ast) {
		return prototype_ast_var(
			generation->asts,
			exact_recursive->output_ast_binder,
			exact_recursive->output_symbol_id,
			generation->span,
			p_ast
		);
	}
	const struct prototype_ast_node source = generation->asts->nodes[source_ast];
	switch (source.tag) {
		case PROTOTYPE_AST_VAR: {
			const struct function_graph_binding_map* binding =
				function_graph_find_binding(
					bindings, binding_count, source.as.var.ast_binder_id
				);
			return function_graph_binding_value(generation, binding, p_ast);
		}
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS: {
			const struct function_graph_recursive_call* call =
				function_graph_find_recursive(recursive, recursive_count, source_ast);
			if (call && call->source_call_ast == source_ast) {
				return prototype_ast_var(
					generation->asts,
					call->output_ast_binder,
					call->output_symbol_id,
					generation->span,
					p_ast
				);
			}
			const struct function_graph_binding_map* binding =
				function_graph_find_binding(
					bindings, binding_count,
					source.as.induction_hypothesis.ast_binder_id
				);
			return function_graph_binding_value(generation, binding, p_ast);
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
		case PROTOTYPE_AST_MATCH: {
			if (source.as.match.case_count > 32 || binding_count >
				FUNCTION_GRAPH_MAX_BINDINGS) {
				return -1;
			}
			uint32_t scrutinee;
			if (function_graph_clone_value(
					generation, source.as.match.scrutinee, bindings, binding_count,
					recursive, recursive_count, &scrutinee
				) != 0) {
				return -1;
			}
			struct prototype_ast_match_case_input cases[32];
			struct prototype_ast_binder case_binders[FUNCTION_GRAPH_MAX_BINDINGS];
			uint32_t binder_cursor = 0;
			for (uint32_t i = 0; i < source.as.match.case_count; ++i) {
				uint32_t case_id = source.as.match.first_case + i;
				if (case_id >= generation->asts->case_count) {
					return -1;
				}
				const struct prototype_ast_match_case* source_case =
					&generation->asts->cases[case_id];
				if (source_case->first_binder + source_case->binder_count >
					generation->asts->case_binder_count || binder_cursor +
					source_case->binder_count > FUNCTION_GRAPH_MAX_BINDINGS ||
					binding_count + source_case->binder_count >
						FUNCTION_GRAPH_MAX_BINDINGS) {
					return -1;
				}
				struct function_graph_binding_map local_bindings[
					FUNCTION_GRAPH_MAX_BINDINGS
				];
				for (uint32_t j = 0; j < binding_count; ++j) {
					local_bindings[j] = bindings[j];
				}
				uint32_t local_count = binding_count;
				for (uint32_t j = 0; j < source_case->binder_count; ++j) {
					const struct prototype_ast_binder* source_binder =
						&generation->asts->case_binders[
							source_case->first_binder + j
						];
					uint32_t target_binder = prototype_ast_new_binder(generation->asts);
					if (target_binder == PROTOTYPE_INVALID_ID) {
						return -1;
					}
					case_binders[binder_cursor + j] =
						(struct prototype_ast_binder) {
							.ast_binder_id = target_binder,
							.symbol_id = source_binder->symbol_id
						};
					local_bindings[local_count++] =
						(struct function_graph_binding_map) {
							.source_ast_binder = source_binder->ast_binder_id,
							.source_binding = PROTOTYPE_INVALID_ID,
							.target_ast_binder = target_binder,
							.target_value = PROTOTYPE_INVALID_ID,
							.symbol_id = source_binder->symbol_id
						};
				}
				uint32_t body;
				if (function_graph_clone_value(
						generation, source_case->body, local_bindings, local_count,
						recursive, recursive_count, &body
					) != 0) {
					return -1;
				}
				cases[i] = (struct prototype_ast_match_case_input) {
					.constructor_symbol_id = source_case->constructor_symbol_id,
					.binders = &case_binders[binder_cursor],
					.binder_count = source_case->binder_count,
					.body = body,
					.span = generation->span
				};
				binder_cursor += source_case->binder_count;
			}
			return prototype_ast_match(
				generation->asts, scrutinee, cases, source.as.match.case_count,
				generation->span, p_ast
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
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE:
			return prototype_ast_type_expr_value_reference(
				generation->asts, value_ast, generation->span, p_type
			);
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
	if (node->tag == PROTOTYPE_AST_MATCH) {
		if (function_graph_collect_ih(
				asts, node->as.match.scrutinee, ih_asts, capacity, p_count
			) != 0 || node->as.match.first_case + node->as.match.case_count >
				asts->case_count) {
			return -1;
		}
		for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
			if (function_graph_collect_ih(
					asts, asts->cases[node->as.match.first_case + i].body,
					ih_asts, capacity, p_count
				) != 0) {
				return -1;
			}
		}
		return 0;
	}
	return node->tag == PROTOTYPE_AST_VAR || node->tag == PROTOTYPE_AST_NAME ||
		node->tag == PROTOTYPE_AST_NAME_IN_NAMESPACE ||
		node->tag == PROTOTYPE_AST_NAME_IN_AST_NAMESPACE ||
		node->tag == PROTOTYPE_AST_TEXT_LITERAL ||
		node->tag == PROTOTYPE_AST_INT_LITERAL ? 0 : -1;
}

/* A computation block is source sequencing, not a second proof language. For
 * graph generation, retain every binding origin and expose the selected final
 * expression. Recursive computations are still represented by ordinary graph
 * output/evidence fields; the source Binder only determines their local role
 * names and subsequent substitution. */
static int function_graph_terminal_plan_open(
	struct function_graph_generation* generation,
	uint32_t ast,
	struct function_graph_terminal_plan* p_plan
) {
	const struct prototype_ast_db* asts = generation ? generation->asts : NULL;
	if (!asts || !p_plan || ast >= asts->node_count) {
		return -1;
	}
	memset(p_plan, 0, sizeof(*p_plan));
	p_plan->body_ast = ast;
	const struct prototype_ast_node* node = &asts->nodes[ast];
	if (node->tag != PROTOTYPE_AST_COMPUTATION_BLOCK) {
		return 0;
	}
	if (node->as.block.item_count == 0 || node->as.block.result_item_index >=
		node->as.block.item_count || node->as.block.first_item +
		node->as.block.item_count > asts->block_item_count) {
		return -1;
	}
	uint32_t cutoff = node->as.block.result_item_index;
	for (uint32_t i = 0; i <= cutoff; ++i) {
		uint32_t item_ast = asts->block_items[node->as.block.first_item + i];
		if (item_ast >= asts->node_count) {
			return -1;
		}
		const struct prototype_ast_node* item = &asts->nodes[item_ast];
		if (item->tag == PROTOTYPE_AST_BLOCK_BINDING) {
			if (p_plan->binding_count >= FUNCTION_GRAPH_MAX_TERMINAL_BINDINGS) {
				return -1;
			}
			p_plan->bindings[p_plan->binding_count++] =
				(struct function_graph_terminal_binding) {
					.item_ast = item_ast,
					.source_ast_binder = item->as.block_binding.ast_binder_id,
					.source_symbol_id = item->as.block_binding.binder_symbol_id,
					.value_ast = item->as.block_binding.value
				};
			if (i == cutoff && prototype_ast_var(
					generation->asts,
					item->as.block_binding.ast_binder_id,
					item->as.block_binding.binder_symbol_id,
					item->span,
					&p_plan->body_ast
				) != 0) {
				return -1;
			}
			continue;
		}
		if (item->tag != PROTOTYPE_AST_BLOCK_EXPRESSION || i != cutoff) {
			return -1;
		}
		p_plan->body_ast = item->as.block_expression.term;
	}
	return p_plan->body_ast == ast ? -1 : 0;
}

static const struct function_graph_terminal_binding*
function_graph_terminal_binding_for_ih(
	const struct prototype_ast_db* asts,
	const struct function_graph_terminal_plan* plan,
	uint32_t ih_ast
) {
	if (!asts || !plan) {
		return NULL;
	}
	for (uint32_t i = 0; i < plan->binding_count; ++i) {
		uint32_t found[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
		uint32_t found_count = 0;
		if (function_graph_collect_ih(
				asts, plan->bindings[i].value_ast, found,
				FUNCTION_GRAPH_MAX_RECURSIVE_CALLS, &found_count
			) != 0) {
			continue;
		}
		for (uint32_t j = 0; j < found_count; ++j) {
			if (found[j] == ih_ast) {
				return &plan->bindings[i];
			}
		}
	}
	return NULL;
}

static int function_graph_collect_terminal_ih(
	const struct prototype_ast_db* asts,
	const struct function_graph_terminal_plan* plan,
	uint32_t* ih_asts,
	uint32_t capacity,
	uint32_t* p_count
) {
	if (!asts || !plan || !ih_asts || !p_count) {
		return -1;
	}
	for (uint32_t i = 0; i < plan->binding_count; ++i) {
		if (function_graph_collect_ih(
				asts, plan->bindings[i].value_ast, ih_asts, capacity, p_count
			) != 0) {
			return -1;
		}
	}
	return function_graph_collect_ih(
		asts, plan->body_ast, ih_asts, capacity, p_count
	);
}

static int function_graph_terminal_binding_core_id(
	const struct function_graph_generation* generation,
	const struct function_graph_terminal_binding* binding,
	uint32_t* p_binding_id
) {
	if (!generation || !binding || !p_binding_id) {
		return -1;
	}
	const struct prototype_typed_occurrence* occurrence = function_graph_occurrence(
		generation->metadata,
		binding->item_ast,
		PROTOTYPE_TYPED_OCCURRENCE_VAR
	);
	if (!occurrence || occurrence->binding_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_binding_id = occurrence->binding_id;
	return 0;
}

static int function_graph_term_value(
	struct function_graph_generation* generation,
	uint32_t term,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t* p_value
) {
	if (!generation || !p_value || term >= generation->terms->term_count) {
		return -1;
	}
	const struct prototype_term* node = &generation->terms->terms[term];
	if (node->tag == PROTOTYPE_TERM_VAR) {
		for (uint32_t i = 0; i < binding_count; ++i) {
			if (bindings[i].source_binding == node->as.var.binding_id) {
				return function_graph_binding_value(
					generation, &bindings[i], p_value
				);
			}
		}
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t argument_count;
	if (node->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		prototype_term_type_instance_info(
			generation->terms,
			term,
			&type_id,
			arguments,
			&argument_count
		) == 0 &&
		type_id < generation->type_declarations->semantic_schema.type_count) {
		const struct prototype_type_declaration* declaration =
			&generation->type_declarations->semantic_schema.type_declarations[type_id];
		uint32_t value;
		if (prototype_ast_name(
				generation->asts,
				declaration->name_symbol_id,
				generation->span,
				&value
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < argument_count; ++i) {
			uint32_t argument;
			if (function_graph_term_value(
					generation,
					arguments[i],
					bindings,
					binding_count,
					&argument
				) != 0 || prototype_ast_app(
					generation->asts,
					value,
					argument,
					generation->span,
					&value
				) != 0) {
				return -1;
			}
		}
		*p_value = value;
		return 0;
	}
	if (node->tag == PROTOTYPE_TERM_TYPE_DECLARATION &&
		node->as.type_declaration.type_id <
			generation->type_declarations->semantic_schema.type_count) {
		return prototype_ast_name(
			generation->asts,
			generation->type_declarations->semantic_schema.type_declarations[
				node->as.type_declaration.type_id
			].name_symbol_id,
			generation->span,
			p_value
		);
	}
	if (node->tag == PROTOTYPE_TERM_CONSTRUCTOR) {
		uint32_t owner_type_id;
		uint32_t owner_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
		uint32_t owner_argument_count;
		if (prototype_type_declaration_instance_info(
				generation->type_declarations,
				generation->terms,
				node->as.constructor.owner,
				&owner_type_id,
				owner_arguments,
				FUNCTION_GRAPH_MAX_ARGUMENTS,
				&owner_argument_count
			) != 0 || owner_type_id >=
				generation->type_declarations->semantic_schema.type_count) {
			return -1;
		}
		const struct prototype_type_declaration* owner_type =
			&generation->type_declarations->semantic_schema.type_declarations[
				owner_type_id
			];
		if (node->as.constructor.constructor_id >= owner_type->constructor_count ||
			owner_type->first_constructor + node->as.constructor.constructor_id >=
				generation->type_declarations->semantic_schema.constructor_count) {
			return -1;
		}
		uint32_t owner;
		if (prototype_ast_name(
				generation->asts,
				owner_type->name_symbol_id,
				generation->span,
				&owner
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < owner_argument_count; ++i) {
			uint32_t argument;
			if (function_graph_term_value(
					generation,
					owner_arguments[i],
					bindings,
					binding_count,
					&argument
				) != 0 || prototype_ast_app(
					generation->asts,
					owner,
					argument,
					generation->span,
					&owner
				) != 0) {
				return -1;
			}
		}
		const struct prototype_type_constructor_declaration* constructor =
			&generation->type_declarations->semantic_schema.constructor_declarations[
				owner_type->first_constructor + node->as.constructor.constructor_id
			];
		return prototype_ast_name_in_ast_namespace(
			generation->asts,
			owner,
			constructor->name_symbol_id,
			generation->span,
			p_value
		);
	}
	if (node->tag == PROTOTYPE_TERM_APP) {
		uint32_t function;
		uint32_t argument;
		if (function_graph_term_value(
				generation, node->as.app.function, bindings, binding_count, &function
			) != 0 || function_graph_term_value(
				generation, node->as.app.argument, bindings, binding_count, &argument
			) != 0) {
			return -1;
		}
		return prototype_ast_app(
			generation->asts, function, argument, generation->span, p_value
		);
	}
	return -1;
}

static uint32_t function_graph_binding_classifier(
	const struct function_graph_generation* generation,
	uint32_t binding_id
) {
	if (!generation) {
		return PROTOTYPE_INVALID_ID;
	}
	for (uint32_t occurrence_id = 0;
		occurrence_id < generation->metadata->typed_occurrences.occurrence_count;
		++occurrence_id) {
		const struct prototype_typed_occurrence* occurrence =
			&generation->metadata->typed_occurrences.occurrences[occurrence_id];
		if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			occurrence->binding_id == binding_id &&
			occurrence->classifier_status ==
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED &&
			occurrence->classifier < generation->terms->term_count) {
			return occurrence->source_classifier < generation->terms->term_count ?
				occurrence->source_classifier : occurrence->classifier;
		}
	}
	for (uint32_t context_id = 0;
		context_id < generation->metadata->contexts.context_count;
		++context_id) {
		const struct prototype_context* context = prototype_context_get(
			&generation->metadata->contexts, context_id
		);
		if (context && context->binding_id == binding_id &&
			prototype_context_classifier_term(context) != PROTOTYPE_INVALID_ID) {
			return prototype_context_classifier_term(context);
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static int function_graph_term_value_with_owner_view(
	struct function_graph_generation* generation,
	uint32_t term,
	uint32_t owner_view,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t* p_value
) {
	uint32_t head;
	uint32_t erased_owner;
	uint32_t constructor_index;
	uint32_t arguments[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t argument_count;
	uint32_t owner_type_id;
	uint32_t owner_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t owner_argument_count;
	if (!generation || !p_value || prototype_term_constructor_spine_info(
			generation->terms,
			term,
			&head,
			&erased_owner,
			&constructor_index,
			arguments,
			FUNCTION_GRAPH_MAX_BINDINGS,
			&argument_count
		) != 0 || prototype_type_declaration_instance_info(
			generation->type_declarations,
			generation->terms,
			owner_view,
			&owner_type_id,
			owner_arguments,
			FUNCTION_GRAPH_MAX_ARGUMENTS,
			&owner_argument_count
		) != 0 || owner_type_id >=
			generation->type_declarations->semantic_schema.type_count) {
		return function_graph_term_value(
			generation, term, bindings, binding_count, p_value
		);
	}
	const struct prototype_type_declaration* owner_type =
		&generation->type_declarations->semantic_schema.type_declarations[
			owner_type_id
		];
	if (constructor_index >= owner_type->constructor_count ||
		owner_type->first_constructor + constructor_index >=
			generation->type_declarations->semantic_schema.constructor_count) {
		return -1;
	}
	uint32_t owner;
	if (prototype_ast_name(
			generation->asts,
			owner_type->name_symbol_id,
			generation->span,
			&owner
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < owner_argument_count; ++i) {
		uint32_t argument;
		if (function_graph_term_value(
				generation,
				owner_arguments[i],
				bindings,
				binding_count,
				&argument
			) != 0 || prototype_ast_app(
				generation->asts,
				owner,
				argument,
				generation->span,
				&owner
			) != 0) {
			return -1;
		}
	}
	const struct prototype_type_constructor_declaration* constructor =
		&generation->type_declarations->semantic_schema.constructor_declarations[
			owner_type->first_constructor + constructor_index
		];
	uint32_t value;
	if (prototype_ast_name_in_ast_namespace(
			generation->asts,
			owner,
			constructor->name_symbol_id,
			generation->span,
			&value
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		uint32_t argument;
		if (function_graph_term_value(
				generation,
				arguments[i],
				bindings,
				binding_count,
				&argument
			) != 0 || prototype_ast_app(
				generation->asts,
				value,
				argument,
				generation->span,
				&value
			) != 0) {
			return -1;
		}
	}
	(void)head;
	(void)erased_owner;
	*p_value = value;
	return 0;
}

static uint32_t function_graph_case_field_classifier(
	const struct function_graph_generation* generation,
	const struct prototype_typed_occurrence_match_case* operation_case,
	uint32_t field
) {
	if (!generation || !operation_case || field >= operation_case->binder_count) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t binding_context = PROTOTYPE_INVALID_ID;
	uint32_t classifier = prototype_context_find_binding(
		&generation->metadata->contexts,
		operation_case->context_id,
		operation_case->binder_ids[field],
		&binding_context
	) == 0 ? prototype_context_classifier_term(prototype_context_get(
		&generation->metadata->contexts, binding_context
	)) : PROTOTYPE_INVALID_ID;
	for (uint32_t occurrence_id = 0;
		occurrence_id < generation->metadata->typed_occurrences.occurrence_count;
		++occurrence_id) {
		const struct prototype_typed_occurrence* occurrence =
			&generation->metadata->typed_occurrences.occurrences[occurrence_id];
		if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			occurrence->binding_id == operation_case->binder_ids[field] &&
			occurrence->classifier_status ==
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED &&
			occurrence->classifier < generation->terms->term_count) {
			return field > 0 && occurrence->source_classifier <
				generation->terms->term_count ? occurrence->source_classifier :
				occurrence->classifier;
		}
	}
	return classifier;
}

static int function_graph_case_result_classifier(
	struct function_graph_generation* generation,
	const struct prototype_typed_occurrence_match_case* operation_case,
	uint32_t* p_classifier
) {
	uint32_t subject;
	uint32_t argument_classifiers[FUNCTION_GRAPH_MAX_BINDINGS];
	if (!generation || !operation_case || !p_classifier ||
		operation_case->binder_count > FUNCTION_GRAPH_MAX_BINDINGS ||
		prototype_term_constructor(
			generation->terms,
			operation_case->constructor_owner,
			operation_case->constructor_id,
			&subject
		) != 0) {
		return -1;
	}
	uint32_t owner_type_id;
	uint32_t owner_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t owner_argument_count;
	if (prototype_type_declaration_instance_info(
			generation->type_declarations,
			generation->terms,
			operation_case->constructor_owner,
			&owner_type_id,
			owner_arguments,
			FUNCTION_GRAPH_MAX_ARGUMENTS,
			&owner_argument_count
		) == 0 && owner_type_id <
			generation->type_declarations->semantic_schema.type_count &&
		generation->type_declarations->semantic_schema.type_declarations[
			owner_type_id
		].index_count == 0) {
		/* Constructor fields cannot refine a non-indexed family. The accepted
		 * owner instance is therefore the exact branch result even when an unused
		 * source field has no standalone VAR occurrence in the operation graph. */
		*p_classifier = operation_case->constructor_owner;
		return 0;
	}
	for (uint32_t i = 0; i < operation_case->binder_count; ++i) {
		uint32_t field;
		argument_classifiers[i] = function_graph_binding_classifier(
			generation, operation_case->binder_ids[i]
		);
		if (argument_classifiers[i] == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				generation->terms, operation_case->binder_ids[i], &field
			) != 0 || prototype_term_app(
				generation->terms, subject, field, &subject
			) != 0) {
			fprintf(stderr,
				"function graph constructor field unavailable field=%u binding=%u classifier=%u\n",
				i, operation_case->binder_ids[i], argument_classifiers[i]);
			return -1;
		}
	}
	int saturated = 0;
	int status = prototype_judgement_constructor_spine_classifier(
		generation->terms,
		generation->type_declarations,
		&generation->metadata->contexts,
		&generation->metadata->substitutions,
		operation_case->context_id,
		subject,
		PROTOTYPE_INVALID_ID,
		argument_classifiers,
		operation_case->binder_count,
		p_classifier,
		&saturated
	);
	if (status != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID || !saturated) {
		fprintf(stderr,
			"function graph constructor classifier rejected owner=%u constructor=%u fields=%u context=%u status=%d saturated=%d result=%u\n",
			operation_case->constructor_owner, operation_case->constructor_id,
			operation_case->binder_count, operation_case->context_id, status, saturated,
			*p_classifier);
	}
	return status == PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID && saturated ?
		0 : -1;
}

static int function_graph_refine_family_arguments(
	struct function_graph_generation* generation,
	uint32_t refined_classifier,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t* arguments
) {
	uint32_t source_type_id;
	uint32_t source_owner_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t source_owner_argument_count;
	uint32_t recursive_type_id;
	uint32_t recursive_owner_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t recursive_owner_argument_count;
	if (!generation || !bindings || !arguments ||
		refined_classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	int source_status = prototype_type_declaration_instance_info(
			generation->type_declarations,
			generation->terms,
			generation->owner_input_classifier,
			&source_type_id,
			source_owner_arguments,
			FUNCTION_GRAPH_MAX_ARGUMENTS,
			&source_owner_argument_count
		);
	int recursive_status = prototype_type_declaration_instance_info(
			generation->type_declarations,
			generation->terms,
			refined_classifier,
			&recursive_type_id,
			recursive_owner_arguments,
			FUNCTION_GRAPH_MAX_ARGUMENTS,
			&recursive_owner_argument_count
		);
	if (source_status != 0 || recursive_status != 0 ||
		source_type_id != recursive_type_id ||
		source_owner_argument_count != recursive_owner_argument_count) {
		fprintf(stderr,
			"function graph family instance mismatch source=%u status=%d count=%u refined=%u type=%u status=%d count=%u\n",
			generation->owner_input_classifier, source_status,
			source_status == 0 ? source_owner_argument_count : PROTOTYPE_INVALID_ID,
			refined_classifier,
			recursive_status == 0 ? recursive_type_id : PROTOTYPE_INVALID_ID,
			recursive_status,
			recursive_status == 0 ? recursive_owner_argument_count : PROTOTYPE_INVALID_ID);
		return -1;
	}
	for (uint32_t owner_argument = 0;
		owner_argument < source_owner_argument_count;
		++owner_argument) {
		uint32_t source_term;
		if (prototype_term_core_projection(
				generation->terms,
				source_owner_arguments[owner_argument],
				&source_term
			) != 0 || source_term >= generation->terms->term_count ||
			generation->terms->terms[source_term].tag != PROTOTYPE_TERM_VAR) {
			continue;
		}
		uint32_t source_binding =
			generation->terms->terms[source_term].as.var.binding_id;
		for (uint32_t function_argument = 0;
			function_argument < generation->argument_count;
			++function_argument) {
			if (generation->source_argument_bindings[function_argument] !=
				source_binding) {
				continue;
			}
			if (function_graph_term_value(
					generation,
					recursive_owner_arguments[owner_argument],
					bindings,
					binding_count,
					&arguments[function_argument]
				) != 0) {
				return -1;
			}
			break;
		}
	}
	return 0;
}

/* A nested dependent Match can refine a binder introduced by an enclosing
 * Match. Preserve that refinement in generated constructor types and result
 * indices instead of retaining the pre-refinement variable as an independent
 * index. */
static int function_graph_refine_binding_values(
	struct function_graph_generation* generation,
	uint32_t refined_classifier,
	struct function_graph_binding_map* bindings,
	uint32_t binding_count
) {
	uint32_t source_type_id;
	uint32_t source_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t source_argument_count;
	uint32_t refined_type_id;
	uint32_t refined_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t refined_argument_count;
	if (!generation || !bindings || prototype_type_declaration_instance_info(
			generation->type_declarations,
			generation->terms,
			generation->owner_input_classifier,
			&source_type_id,
			source_arguments,
			FUNCTION_GRAPH_MAX_ARGUMENTS,
			&source_argument_count
		) != 0 || prototype_type_declaration_instance_info(
			generation->type_declarations,
			generation->terms,
			refined_classifier,
			&refined_type_id,
			refined_arguments,
			FUNCTION_GRAPH_MAX_ARGUMENTS,
			&refined_argument_count
		) != 0 || source_type_id != refined_type_id ||
		source_argument_count != refined_argument_count) {
		return -1;
	}
	for (uint32_t i = 0; i < source_argument_count; ++i) {
		uint32_t source;
		if (prototype_term_core_projection(
				generation->terms, source_arguments[i], &source
			) != 0 || source >= generation->terms->term_count ||
			generation->terms->terms[source].tag != PROTOTYPE_TERM_VAR) {
			continue;
		}
		struct function_graph_binding_map* binding =
			function_graph_find_core_binding(
				bindings,
				binding_count,
				generation->terms->terms[source].as.var.binding_id
			);
		uint32_t source_classifier = function_graph_binding_classifier(
			generation, generation->terms->terms[source].as.var.binding_id
		);
		uint32_t value;
		if (!binding || source_classifier == PROTOTYPE_INVALID_ID ||
			function_graph_term_value_with_owner_view(
				generation,
				refined_arguments[i],
				source_classifier,
				bindings,
				binding_count,
				&value
			) != 0) {
			return -1;
		}
		binding->target_value = value;
	}
	return 0;
}

static int function_graph_reproject_case_field_types(
	struct function_graph_generation* generation,
	const struct prototype_typed_occurrence_match_case* operation_case,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t first_field,
	uint32_t* field_types
) {
	if (!generation || !operation_case || !bindings || !field_types) {
		return -1;
	}
	for (uint32_t i = 0; i < operation_case->binder_count; ++i) {
		uint32_t classifier = function_graph_case_field_classifier(
			generation, operation_case, i
		);
		if (classifier == PROTOTYPE_INVALID_ID || function_graph_projection_type(
				generation,
				classifier,
				bindings,
				binding_count,
				&field_types[first_field + i]
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int function_graph_refine_recursive_family_arguments(
	struct function_graph_generation* generation,
	uint32_t recursive_binding,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t* arguments
) {
	return function_graph_refine_family_arguments(
		generation,
		function_graph_binding_classifier(generation, recursive_binding),
		bindings,
		binding_count,
		arguments
	);
}

static int function_graph_constructor_value(
	struct function_graph_generation* generation,
	const struct prototype_typed_occurrence_match_case* operation_case,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
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
		) != 0 ||
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
	uint32_t owner;
	if (prototype_ast_name(
			generation->asts, type->name_symbol_id, generation->span, &owner
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < owner_argument_count; ++i) {
		uint32_t argument;
		if (function_graph_term_value(
				generation,
				owner_arguments[i],
				bindings,
				binding_count,
				&argument
			) != 0 || prototype_ast_app(
				generation->asts, owner, argument, generation->span, &owner
			) != 0) {
			return -1;
		}
	}
	uint32_t value;
	if (prototype_ast_name_in_ast_namespace(
			generation->asts,
			owner,
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

static int function_graph_constructor_type_expr(
	struct function_graph_generation* generation,
	const struct prototype_typed_occurrence_match_case* operation_case,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t* p_type
) {
	uint32_t field_values[FUNCTION_GRAPH_MAX_BINDINGS];
	if (!generation || !operation_case || !p_type ||
		operation_case->binder_count > FUNCTION_GRAPH_MAX_BINDINGS) {
		return -1;
	}
	for (uint32_t i = 0; i < operation_case->binder_count; ++i) {
		const struct function_graph_binding_map* projected = NULL;
		for (uint32_t j = 0; j < binding_count; ++j) {
			if (bindings[j].source_binding == operation_case->binder_ids[i]) {
				projected = &bindings[j];
				break;
			}
		}
		if (function_graph_binding_value(
				generation, projected, &field_values[i]
			) != 0) {
			return -1;
		}
	}
	uint32_t constructor;
	if (function_graph_constructor_value(
			generation,
			operation_case,
			bindings,
			binding_count,
			field_values,
			operation_case->binder_count,
			&constructor
		) != 0) {
		return -1;
	}
	return prototype_ast_type_expr_value_reference(
		generation->asts, constructor, generation->span, p_type
	);
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

static int function_graph_add_runner_expectation(
	struct function_graph_generation* generation,
	uint32_t runner,
	uint32_t assignment
) {
	if (!generation || assignment >= generation->asts->assignment_count) {
		return -1;
	}
	uint32_t lambda_binders[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	int lambda_symbols[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	uint32_t lambda_types[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	uint32_t lambda_values[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	uint32_t lambda_count = 0;
	uint32_t current = runner;
	while (current < generation->asts->node_count &&
		generation->asts->nodes[current].tag == PROTOTYPE_AST_LAMBDA) {
		if (lambda_count >= FUNCTION_GRAPH_MAX_ARGUMENTS + 2) {
			return -1;
		}
		const struct prototype_ast_node* lambda = &generation->asts->nodes[current];
		lambda_binders[lambda_count] = lambda->as.lambda.ast_binder_id;
		lambda_symbols[lambda_count] = lambda->as.lambda.binder_symbol_id;
		lambda_types[lambda_count] = lambda->as.lambda.binder_type;
		if (prototype_ast_var(
				generation->asts,
				lambda_binders[lambda_count],
				lambda_symbols[lambda_count],
				generation->span,
				&lambda_values[lambda_count]
			) != 0) {
			return -1;
		}
		lambda_count++;
		current = lambda->as.lambda.body;
	}
	uint32_t hidden_count = generation->certified_argument_index ==
		PROTOTYPE_INVALID_ID ? 0 : 2;
	int nested_final_argument = generation->nested_recursive &&
		lambda_count + 1 == generation->argument_count + hidden_count;
	if (!nested_final_argument &&
		lambda_count != generation->argument_count + hidden_count) {
		return -1;
	}
	uint32_t result_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
	uint32_t result_argument_count = 0;
	uint32_t lambda_index = 0;
	struct function_graph_binding_map argument_maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t argument_map_count = 0;
	uint32_t nested_binder = PROTOTYPE_INVALID_ID;
	uint32_t nested_symbol = PROTOTYPE_INVALID_ID;
	uint32_t nested_type = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		uint32_t argument_value;
		if (nested_final_argument && i + 1 == generation->argument_count) {
			nested_binder = prototype_ast_new_binder(generation->asts);
			nested_symbol = generation->source_argument_symbols[i];
			if (nested_binder == PROTOTYPE_INVALID_ID ||
				function_graph_projection_type(
					generation,
					generation->source_argument_classifiers[i],
					argument_maps,
					argument_map_count,
					&nested_type
				) != 0 || prototype_ast_var(
					generation->asts, nested_binder, nested_symbol,
					generation->span, &argument_value
				) != 0) {
				return -1;
			}
		} else {
			if (lambda_index >= lambda_count) {
				return -1;
			}
			argument_value = lambda_values[lambda_index];
			argument_maps[argument_map_count++] =
				(struct function_graph_binding_map) {
					.source_ast_binder =
						generation->source_argument_ast_binders[i],
					.source_binding = generation->source_argument_bindings[i],
					.target_ast_binder = lambda_binders[lambda_index],
					.target_value = PROTOTYPE_INVALID_ID,
					.symbol_id = generation->source_argument_symbols[i]
				};
			lambda_index++;
		}
		if (function_graph_value_type_expr(
				generation,
				argument_value,
				&result_arguments[result_argument_count++]
			) != 0) {
			return -1;
		}
		if (i == generation->certified_argument_index) {
			if (function_graph_value_type_expr(
					generation,
					lambda_values[lambda_index++],
					&result_arguments[result_argument_count++]
				) != 0) {
				return -1;
			}
			lambda_index++;
		}
	}
	uint32_t expected_type;
	if (lambda_index != lambda_count || function_graph_type_name_app(
			generation,
			generation->result_symbol,
			result_arguments,
			result_argument_count,
			&expected_type
		) != 0) {
		return -1;
	}
	if (nested_final_argument && prototype_ast_type_expr_pi(
			generation->asts,
			nested_binder,
			nested_symbol,
			nested_type,
			expected_type,
			generation->span,
			&expected_type
		) != 0) {
		return -1;
	}
	for (uint32_t reverse = lambda_count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		if (prototype_ast_type_expr_pi(
				generation->asts,
				lambda_binders[i],
				lambda_symbols[i],
				lambda_types[i],
				expected_type,
				generation->span,
				&expected_type
			) != 0) {
			return -1;
		}
	}
	const struct prototype_ast_term_assignment_def* def =
		&generation->asts->assignments[assignment];
	uint32_t expectation;
	return prototype_ast_add_type_expectation(
		generation->asts,
		PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION,
		def->name_symbol_id,
		expected_type,
		def->source_entry_id,
		generation->span,
		generation->span,
		assignment,
		&expectation
	);
}

static int function_graph_binary_graph_family_type(
	struct function_graph_generation* generation,
	uint32_t domain_type,
	uint32_t* p_type
) {
	uint32_t left_binder = prototype_ast_new_binder(generation->asts);
	uint32_t right_binder = prototype_ast_new_binder(generation->asts);
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	int left_symbol = symbol_intern(generation->symbols, "left", 4);
	int right_symbol = symbol_intern(generation->symbols, "right", 5);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	int bool_symbol = symbol_intern(generation->symbols, "Bool", 4);
	uint32_t bool_type;
	uint32_t universe;
	uint32_t type;
	if (!generation || !p_type || left_binder == PROTOTYPE_INVALID_ID ||
		right_binder == PROTOTYPE_INVALID_ID || output_binder == PROTOTYPE_INVALID_ID ||
		left_symbol < 0 || right_symbol < 0 || output_symbol < 0 || bool_symbol < 0 ||
		prototype_ast_type_expr_name(
			generation->asts, bool_symbol, generation->span, &bool_type
		) != 0 || prototype_ast_type_expr_fresh_universe(
			generation->asts, generation->span, &universe
		) != 0 || prototype_ast_type_expr_pi(
			generation->asts, output_binder, output_symbol, bool_type, universe,
			generation->span, &type
		) != 0 || prototype_ast_type_expr_pi(
			generation->asts, right_binder, right_symbol, domain_type, type,
			generation->span, &type
		) != 0 || prototype_ast_type_expr_pi(
			generation->asts, left_binder, left_symbol, domain_type, type,
			generation->span, &type
		) != 0) {
		return -1;
	}
	*p_type = type;
	return 0;
}

static int function_graph_generate_binary_package(
	struct function_graph_generation* generation
) {
	const char* package_name = "$certified.binary-bool";
	int package_symbol;
	if (!generation ||
		(package_symbol = symbol_intern(
			generation->symbols, package_name, strlen(package_name)
		)) < 0) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->asts->assignment_count; ++i) {
		if (generation->asts->assignments[i].name_symbol_id == package_symbol) {
			generation->certified_package_symbol = package_symbol;
			generation->certified_package_assignment = i;
			return 0;
		}
	}
	uint32_t type_def;
	uint32_t a_binder = prototype_ast_new_binder(generation->asts);
	uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
	uint32_t left_binder = prototype_ast_new_binder(generation->asts);
	uint32_t right_binder = prototype_ast_new_binder(generation->asts);
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	uint32_t witness_binder = prototype_ast_new_binder(generation->asts);
	int a_symbol = symbol_intern(generation->symbols, "A", 1);
	int graph_symbol = symbol_intern(generation->symbols, "Graph", 5);
	int left_symbol = symbol_intern(generation->symbols, "left", 4);
	int right_symbol = symbol_intern(generation->symbols, "right", 5);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	int witness_symbol = symbol_intern(generation->symbols, "graph", 5);
	int bool_symbol = symbol_intern(generation->symbols, "Bool", 4);
	int returned_symbol = symbol_intern(generation->symbols, "returned", 8);
	uint32_t universe;
	uint32_t a_type;
	uint32_t graph_type;
	uint32_t bool_type;
	uint32_t graph_var;
	uint32_t left_var;
	uint32_t right_var;
	uint32_t output_var;
	if (a_binder == PROTOTYPE_INVALID_ID || graph_binder == PROTOTYPE_INVALID_ID ||
		left_binder == PROTOTYPE_INVALID_ID || right_binder == PROTOTYPE_INVALID_ID ||
		output_binder == PROTOTYPE_INVALID_ID || witness_binder == PROTOTYPE_INVALID_ID ||
		a_symbol < 0 || graph_symbol < 0 || left_symbol < 0 || right_symbol < 0 ||
		output_symbol < 0 || witness_symbol < 0 || bool_symbol < 0 ||
		returned_symbol < 0 || prototype_ast_type_add(
			generation->asts, package_symbol, generation->span, generation->span,
			&type_def
		) != 0 || prototype_ast_type_expr_fresh_universe(
			generation->asts, generation->span, &universe
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts, type_def, a_binder, a_symbol, universe,
			PROTOTYPE_AST_FAMILY_BINDER_PARAMETER, generation->span
		) != 0 || prototype_ast_type_expr_var(
			generation->asts, a_binder, a_symbol, generation->span, &a_type
		) != 0 || function_graph_binary_graph_family_type(
			generation, a_type, &graph_type
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts, type_def, graph_binder, graph_symbol, graph_type,
			PROTOTYPE_AST_FAMILY_BINDER_PARAMETER, generation->span
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts, type_def, left_binder, left_symbol, a_type,
			PROTOTYPE_AST_FAMILY_BINDER_PARAMETER, generation->span
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts, type_def, right_binder, right_symbol, a_type,
			PROTOTYPE_AST_FAMILY_BINDER_PARAMETER, generation->span
		) != 0 || prototype_ast_type_expr_name(
			generation->asts, bool_symbol, generation->span, &bool_type
		) != 0 || prototype_ast_type_expr_var(
			generation->asts, graph_binder, graph_symbol, generation->span, &graph_var
		) != 0 || prototype_ast_type_expr_var(
			generation->asts, left_binder, left_symbol, generation->span, &left_var
		) != 0 || prototype_ast_type_expr_var(
			generation->asts, right_binder, right_symbol, generation->span, &right_var
		) != 0 || prototype_ast_type_expr_var(
			generation->asts, output_binder, output_symbol, generation->span,
			&output_var
		) != 0) {
		return -1;
	}
	uint32_t witness_type = graph_var;
	if (prototype_ast_type_expr_app(
			generation->asts, witness_type, left_var, generation->span, &witness_type
		) != 0 || prototype_ast_type_expr_app(
			generation->asts, witness_type, right_var, generation->span, &witness_type
		) != 0 || prototype_ast_type_expr_app(
			generation->asts, witness_type, output_var, generation->span, &witness_type
		) != 0) {
		return -1;
	}
	uint32_t result_type;
	uint32_t field_types[2] = { bool_type, witness_type };
	uint32_t field_binders[2] = {
		output_binder, witness_binder
	};
	int field_symbols[2] = {
		output_symbol, witness_symbol
	};
	if (function_graph_self_app(
			generation, NULL, 0, &result_type
		) != 0 || prototype_ast_type_add_constructor(
			generation->asts, type_def, returned_symbol, generation->span,
			field_types, field_binders, field_symbols, 2, result_type
		) != 0 || function_graph_add_type_assignment(
			generation, package_symbol, type_def,
			&generation->certified_package_assignment
		) != 0) {
		return -1;
	}
	generation->certified_package_symbol = package_symbol;
	return 0;
}

static int function_graph_binary_package_type(
	struct function_graph_generation* generation,
	uint32_t domain_type,
	uint32_t graph_type,
	uint32_t left_type,
	uint32_t right_type,
	uint32_t* p_type
) {
	uint32_t type;
	if (!generation || !p_type || generation->certified_package_symbol < 0 ||
		prototype_ast_type_expr_name(
			generation->asts, generation->certified_package_symbol,
			generation->span, &type
		) != 0 || prototype_ast_type_expr_app(
			generation->asts, type, domain_type, generation->span, &type
		) != 0 || prototype_ast_type_expr_app(
			generation->asts, type, graph_type, generation->span, &type
		) != 0 || prototype_ast_type_expr_app(
			generation->asts, type, left_type, generation->span, &type
		) != 0 || prototype_ast_type_expr_app(
			generation->asts, type, right_type, generation->span, &type
		) != 0) {
		return -1;
	}
	*p_type = type;
	return 0;
}

static int function_graph_binary_callback_type(
	struct function_graph_generation* generation,
	uint32_t domain_type,
	uint32_t graph_type,
	uint32_t* p_type
) {
	uint32_t left_binder = prototype_ast_new_binder(generation->asts);
	uint32_t right_binder = prototype_ast_new_binder(generation->asts);
	int left_symbol = symbol_intern(generation->symbols, "left", 4);
	int right_symbol = symbol_intern(generation->symbols, "right", 5);
	uint32_t left_type;
	uint32_t right_type;
	uint32_t result;
	if (!generation || !p_type || left_binder == PROTOTYPE_INVALID_ID ||
		right_binder == PROTOTYPE_INVALID_ID || left_symbol < 0 || right_symbol < 0 ||
		prototype_ast_type_expr_var(
			generation->asts, left_binder, left_symbol, generation->span, &left_type
		) != 0 || prototype_ast_type_expr_var(
			generation->asts, right_binder, right_symbol, generation->span, &right_type
		) != 0 || function_graph_binary_package_type(
			generation, domain_type, graph_type, left_type, right_type, &result
		) != 0 || prototype_ast_type_expr_pi(
			generation->asts, right_binder, right_symbol, domain_type, result,
			generation->span, &result
		) != 0 || prototype_ast_type_expr_pi(
			generation->asts, left_binder, left_symbol, domain_type, result,
			generation->span, &result
		) != 0) {
		return -1;
	}
	*p_type = result;
	return 0;
}

static int function_graph_append_case_fields(
	struct function_graph_generation* generation,
	const struct prototype_ast_match_case* source_case,
	const struct prototype_typed_occurrence_match_case* operation_case,
	struct function_graph_binding_map* bindings,
	uint32_t* p_binding_count,
	uint32_t* field_types,
	uint32_t* field_binders,
	int* field_symbols,
	uint32_t* field_values,
	uint32_t* p_field_count
) {
	if (!generation || !source_case || !operation_case || !bindings ||
		!p_binding_count || !field_types || !field_binders || !field_symbols ||
		!field_values || !p_field_count ||
		source_case->binder_count != operation_case->binder_count) {
		return -1;
	}
	for (uint32_t field = 0; field < source_case->binder_count; ++field) {
		if (*p_binding_count >= FUNCTION_GRAPH_MAX_BINDINGS ||
			*p_field_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		const struct prototype_ast_binder* source_binder =
			&generation->asts->case_binders[source_case->first_binder + field];
		uint32_t target_binder = prototype_ast_new_binder(generation->asts);
		uint32_t field_binding_context;
		if (target_binder == PROTOTYPE_INVALID_ID ||
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
		/* Context classifiers intentionally describe the erased dependent core.
		 * Generated declarations live at the operation layer, so preserve the
		 * accepted TypeView carried by an occurrence of this exact Binding. */
		uint32_t field_classifier = function_graph_case_field_classifier(
			generation, operation_case, field
		);
		uint32_t field_index = *p_field_count;
		if (!binding_context || field_classifier == PROTOTYPE_INVALID_ID ||
			function_graph_projection_type(
				generation,
				field_classifier,
				bindings,
				*p_binding_count,
				&field_types[field_index]
			) != 0 || prototype_ast_var(
				generation->asts,
				target_binder,
				source_binder->symbol_id,
				generation->span,
				&field_values[field_index]
			) != 0) {
			fprintf(stderr,
				"function graph case field projection failed field=%u binding=%u "
				"context=%u classifier=%u target-binder=%u bindings=%u\n",
				field, operation_case->binder_ids[field], field_binding_context,
				field_classifier, target_binder, *p_binding_count);
			return -1;
		}
		field_binders[field_index] = target_binder;
		field_symbols[field_index] = source_binder->symbol_id;
		bindings[(*p_binding_count)++] = (struct function_graph_binding_map) {
			.source_ast_binder = source_binder->ast_binder_id,
			.source_binding = operation_case->binder_ids[field],
			.target_ast_binder = target_binder,
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = source_binder->symbol_id
		};
		(*p_field_count)++;
	}
	return 0;
}

static int function_graph_append_root_suffix_fields(
	struct function_graph_generation* generation,
	struct function_graph_binding_map* bindings,
	uint32_t* p_binding_count,
	uint32_t* field_types,
	uint32_t* field_binders,
	int* field_symbols,
	uint32_t* field_values,
	uint32_t* p_field_count
) {
	if (!generation || !bindings || !p_binding_count || !field_types ||
		!field_binders || !field_symbols || !field_values || !p_field_count) {
		return -1;
	}
	for (uint32_t argument = generation->match_argument_index + 1;
		argument < generation->root_argument_count; ++argument) {
		if (*p_binding_count >= FUNCTION_GRAPH_MAX_BINDINGS ||
			*p_field_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		uint32_t field = (*p_field_count)++;
		uint32_t binder = prototype_ast_new_binder(generation->asts);
		if (binder == PROTOTYPE_INVALID_ID || function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[argument],
				bindings,
				*p_binding_count,
				&field_types[field]
			) != 0 || prototype_ast_var(
				generation->asts,
				binder,
				generation->source_argument_symbols[argument],
				generation->span,
				&field_values[field]
			) != 0) {
			return -1;
		}
		field_binders[field] = binder;
		field_symbols[field] = generation->source_argument_symbols[argument];
		bindings[(*p_binding_count)++] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[argument],
			.source_binding = generation->source_argument_bindings[argument],
			.target_ast_binder = binder,
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generation->source_argument_symbols[argument]
		};
	}
	return 0;
}

static int function_graph_make_runtime_case_bindings(
	struct function_graph_generation* generation,
	const struct prototype_ast_match_case* source_case,
	const struct prototype_typed_occurrence_match_case* operation_case,
	struct function_graph_binding_map* bindings,
	uint32_t* p_binding_count,
	struct prototype_ast_binder* generated_binders,
	uint32_t* values
) {
	if (!generation || !source_case || !operation_case || !bindings ||
		!p_binding_count || !generated_binders || !values ||
		source_case->binder_count != operation_case->binder_count) {
		return -1;
	}
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		if (*p_binding_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		const struct prototype_ast_binder* source_binder =
			&generation->asts->case_binders[source_case->first_binder + i];
		uint32_t target_binder = prototype_ast_new_binder(generation->asts);
		if (target_binder == PROTOTYPE_INVALID_ID || prototype_ast_var(
				generation->asts,
				target_binder,
				source_binder->symbol_id,
				generation->span,
				&values[i]
			) != 0) {
			return -1;
		}
		generated_binders[i] = (struct prototype_ast_binder) {
			.ast_binder_id = target_binder,
			.symbol_id = source_binder->symbol_id
		};
		bindings[(*p_binding_count)++] = (struct function_graph_binding_map) {
			.source_ast_binder = source_binder->ast_binder_id,
			.source_binding = operation_case->binder_ids[i],
			.target_ast_binder = target_binder,
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = source_binder->symbol_id
		};
	}
	return 0;
}

static int function_graph_append_argument(
	struct function_graph_generation* generation,
	uint32_t lambda_ast
) {
	if (!generation || lambda_ast >= generation->asts->node_count ||
		generation->asts->nodes[lambda_ast].tag != PROTOTYPE_AST_LAMBDA ||
		generation->argument_count >= FUNCTION_GRAPH_MAX_ARGUMENTS) {
		return -1;
	}
	const struct prototype_ast_node* lambda = &generation->asts->nodes[lambda_ast];
	const struct prototype_typed_occurrence* occurrence = function_graph_occurrence(
		generation->metadata, lambda_ast, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA
	);
	if (!occurrence || occurrence->binding_id == PROTOTYPE_INVALID_ID ||
		occurrence->binder_classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t index = generation->argument_count++;
	generation->source_argument_ast_binders[index] =
		lambda->as.lambda.ast_binder_id;
	generation->source_argument_bindings[index] = occurrence->binding_id;
	generation->source_argument_classifiers[index] =
		occurrence->binder_classifier;
	generation->source_argument_symbols[index] =
		lambda->as.lambda.binder_symbol_id;
	return 0;
}

static int function_graph_common_branch_lambda_count(
	struct function_graph_generation* generation,
	uint32_t match_ast,
	uint32_t* p_count
) {
	if (!generation || !p_count || match_ast >= generation->asts->node_count ||
		generation->asts->nodes[match_ast].tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	const struct prototype_ast_node* match = &generation->asts->nodes[match_ast];
	uint32_t common = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t body = generation->asts->cases[
			match->as.match.first_case + i
		].body;
		uint32_t count = 0;
		while (body < generation->asts->node_count &&
			generation->asts->nodes[body].tag == PROTOTYPE_AST_LAMBDA) {
			count++;
			body = generation->asts->nodes[body].as.lambda.body;
		}
		if (common == PROTOTYPE_INVALID_ID || count < common) {
			common = count;
		}
	}
	*p_count = common == PROTOTYPE_INVALID_ID ? 0 : common;
	return 0;
}

static int function_graph_append_joined_arguments(
	struct function_graph_generation* generation,
	uint32_t match_ast,
	uint32_t count
) {
	if (!generation || match_ast >= generation->asts->node_count ||
		generation->asts->nodes[match_ast].tag != PROTOTYPE_AST_MATCH ||
		generation->asts->nodes[match_ast].as.match.case_count == 0) {
		return -1;
	}
	uint32_t body = generation->asts->cases[
		generation->asts->nodes[match_ast].as.match.first_case
	].body;
	for (uint32_t i = 0; i < count; ++i) {
		if (function_graph_append_argument(generation, body) != 0) {
			return -1;
		}
		body = generation->asts->nodes[body].as.lambda.body;
	}
	return 0;
}

static int function_graph_add_joined_branch_bindings(
	struct function_graph_generation* generation,
	uint32_t body,
	const struct function_graph_binding_map* argument_maps,
	struct function_graph_binding_map* bindings,
	uint32_t* p_binding_count,
	uint32_t* p_terminal_body
) {
	if (!generation || !argument_maps || !bindings || !p_binding_count ||
		!p_terminal_body) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->joined_argument_count; ++i) {
		if (body >= generation->asts->node_count ||
			generation->asts->nodes[body].tag != PROTOTYPE_AST_LAMBDA ||
			*p_binding_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		const struct prototype_ast_node* lambda = &generation->asts->nodes[body];
		const struct prototype_typed_occurrence* occurrence =
			function_graph_occurrence(
				generation->metadata, body, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA
			);
		uint32_t argument_index = generation->root_argument_count + i;
		if (!occurrence || argument_index >= generation->argument_count) {
			return -1;
		}
		bindings[(*p_binding_count)++] = (struct function_graph_binding_map) {
			.source_ast_binder = lambda->as.lambda.ast_binder_id,
			.source_binding = occurrence->binding_id,
			.target_ast_binder = argument_maps[argument_index].target_ast_binder,
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = argument_maps[argument_index].symbol_id
		};
		body = lambda->as.lambda.body;
	}
	*p_terminal_body = body;
	return 0;
}

static int function_graph_append_joined_branch_fields(
	struct function_graph_generation* generation,
	uint32_t body,
	struct function_graph_binding_map* bindings,
	uint32_t* p_binding_count,
	uint32_t* field_types,
	uint32_t* field_binders,
	int* field_symbols,
	uint32_t* field_values,
	uint32_t* p_field_count,
	uint32_t* joined_values,
	uint32_t* p_terminal_body
) {
	if (!generation || !bindings || !p_binding_count || !field_types ||
		!field_binders || !field_symbols || !field_values || !p_field_count ||
		!joined_values || !p_terminal_body) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->joined_argument_count; ++i) {
		if (body >= generation->asts->node_count ||
			generation->asts->nodes[body].tag != PROTOTYPE_AST_LAMBDA ||
			*p_binding_count >= FUNCTION_GRAPH_MAX_BINDINGS ||
			*p_field_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		const struct prototype_ast_node* lambda = &generation->asts->nodes[body];
		const struct prototype_typed_occurrence* occurrence =
			function_graph_occurrence(
				generation->metadata, body, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA
			);
		uint32_t field = (*p_field_count)++;
		if (!occurrence || function_graph_projection_type(
				generation,
				occurrence->binder_classifier,
				bindings,
				*p_binding_count,
				&field_types[field]
			) != 0 || (field_binders[field] =
				prototype_ast_new_binder(generation->asts)) == PROTOTYPE_INVALID_ID ||
			prototype_ast_var(
				generation->asts,
				field_binders[field],
				lambda->as.lambda.binder_symbol_id,
				generation->span,
				&field_values[field]
			) != 0) {
			return -1;
		}
		field_symbols[field] = lambda->as.lambda.binder_symbol_id;
		joined_values[i] = field_values[field];
		bindings[(*p_binding_count)++] = (struct function_graph_binding_map) {
			.source_ast_binder = lambda->as.lambda.ast_binder_id,
			.source_binding = occurrence->binding_id,
			.target_ast_binder = field_binders[field],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = field_symbols[field]
		};
		body = lambda->as.lambda.body;
	}
	*p_terminal_body = body;
	return 0;
}

/* Follow the accepted Pi argument path. A Match whose branches all return
 * Lambdas contributes their common prefix as ordinary later arguments; this
 * is the eta-expanded view of definitions such as append. */
static int function_graph_collect_argument_path(
	struct function_graph_generation* generation,
	uint32_t root,
	uint32_t* p_body
) {
	if (!generation || !p_body) {
		return -1;
	}
	uint32_t current = root;
	generation->root_argument_count = 0;
	generation->joined_argument_count = 0;
	for (;;) {
		if (current >= generation->asts->node_count) {
			return -1;
		}
		const struct prototype_ast_node* node = &generation->asts->nodes[current];
		if (node->tag == PROTOTYPE_AST_LAMBDA) {
			if (function_graph_append_argument(generation, current) != 0) {
				return -1;
			}
			current = node->as.lambda.body;
			continue;
		}
		generation->root_argument_count = generation->argument_count;
		if (node->tag == PROTOTYPE_AST_MATCH) {
			uint32_t joined_count;
			if (function_graph_common_branch_lambda_count(
					generation, current, &joined_count
				) != 0 || function_graph_append_joined_arguments(
					generation, current, joined_count
				) != 0) {
				return -1;
			}
			generation->joined_argument_count = joined_count;
		}
		if (node->tag == PROTOTYPE_AST_MATCH && node->as.match.case_count == 1 &&
			generation->joined_argument_count > 0) {
			const struct prototype_ast_match_case* source_case =
				&generation->asts->cases[node->as.match.first_case];
			generation->owner_recursive_match = current;
			generation->nested_recursive = 1;
			current = source_case->body;
			for (uint32_t i = 0; i < generation->joined_argument_count; ++i) {
				current = generation->asts->nodes[current].as.lambda.body;
			}
			*p_body = current;
			return 0;
		}
		*p_body = current;
		return 0;
	}
}

static int function_graph_project_final_computation(
	struct function_graph_generation* generation
) {
	if (!generation || generation->argument_count > FUNCTION_GRAPH_MAX_ARGUMENTS) {
		return -1;
	}
	struct prototype_binding_replacement replacements[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t current = generation->view.classifier;
	for (uint32_t argument = 0; argument < generation->argument_count; ++argument) {
		for (;;) {
			struct prototype_term_normalization_result normalized;
			if (current >= generation->terms->term_count ||
				prototype_term_normalize_with_profile(
					generation->terms,
					generation->type_declarations,
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
			const struct prototype_term* classifier =
				&generation->terms->terms[current];
			if (classifier->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
				current = classifier->as.effect_row_forall.body;
				continue;
			}
			if (classifier->tag != PROTOTYPE_TERM_PI) {
				return -1;
			}
			uint32_t classifier_binder;
			uint32_t body;
			uint32_t operation_var;
			if (prototype_term_pure_family_parts(
					generation->terms,
					classifier->as.pi.codomain_family,
					&classifier_binder,
					&body
				) != 0 || prototype_term_var(
					generation->terms,
					generation->source_argument_bindings[argument],
					&operation_var
				) != 0) {
				return -1;
			}
			replacements[argument] = (struct prototype_binding_replacement) {
				.binding_id = classifier_binder,
				.replacement = operation_var
			};
			current = body;
			break;
		}
	}
	uint32_t projected;
	if (prototype_term_graph_reindex_bindings(
			generation->terms, prototype_type_view_rebuild_context_from_db(generation->type_declarations),
			current,
			replacements,
			generation->argument_count,
			&projected
		) != 0) {
		return -1;
	}
	return accepted_definition_final_computation(
		generation->terms,
		generation->type_declarations,
		projected,
		&generation->view.final_result_type,
		&generation->view.final_effect_row,
		&generation->view.final_totality
	);
}

static int function_graph_pure_whnf(
	struct function_graph_generation* generation,
	uint32_t term,
	uint32_t* p_whnf
) {
	struct prototype_term_normalization_result normalized;
	if (!generation || !p_whnf || term >= generation->terms->term_count ||
		prototype_term_normalize_with_profile(
			generation->terms,
			generation->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			term,
			10000,
			&normalized
		) != 0 || normalized.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
		return -1;
	}
	*p_whnf = normalized.term_id;
	return 0;
}

static int function_graph_binary_bool_classifier(
	struct function_graph_generation* generation,
	uint32_t classifier,
	struct function_graph_binary_classifier* p_shape
) {
	struct function_graph_binary_classifier shape;
	uint32_t current;
	uint32_t body;
	if (!generation || !p_shape || function_graph_pure_whnf(
			generation, classifier, &current
		) != 0) {
		return 0;
	}
	if (generation->terms->terms[current].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		current = generation->terms->terms[current].as.thunk_type.computation;
		if (function_graph_pure_whnf(generation, current, &current) != 0) {
			return 0;
		}
	}
	if (generation->terms->terms[current].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			generation->terms,
			generation->terms->terms[current].as.pi.codomain_family,
			&shape.first_binder,
			&body
		) != 0) {
		return 0;
	}
	shape.domain = generation->terms->terms[current].as.pi.domain;
	if (function_graph_pure_whnf(generation, body, &current) != 0 ||
		generation->terms->terms[current].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return 0;
	}
	current = generation->terms->terms[current].as.computation_type.result;
	if (function_graph_pure_whnf(generation, current, &current) != 0) {
		return 0;
	}
	if (generation->terms->terms[current].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		current = generation->terms->terms[current].as.thunk_type.computation;
		if (function_graph_pure_whnf(generation, current, &current) != 0) {
			return 0;
		}
	}
	if (generation->terms->terms[current].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			generation->terms,
			generation->terms->terms[current].as.pi.codomain_family,
			&shape.second_binder,
			&body
		) != 0) {
		return 0;
	}
	shape.second_domain = generation->terms->terms[current].as.pi.domain;
	if (function_graph_pure_whnf(generation, body, &current) != 0 ||
		generation->terms->terms[current].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return 0;
	}
	shape.result = generation->terms->terms[current].as.computation_type.result;
	if (prototype_judgement_classifier_conversion(
			generation->terms,
			generation->type_declarations,
			shape.domain,
			shape.second_domain
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	uint32_t type_id;
	uint32_t arguments[4];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			generation->terms,
			shape.result,
			&type_id,
			arguments,
			&argument_count
		) != 0 || argument_count != 0 || type_id >=
			generation->type_declarations->semantic_schema.type_count) {
		return 0;
	}
	const struct prototype_type_declaration* result_type =
		&generation->type_declarations->semantic_schema.type_declarations[type_id];
	const char* result_name = symbol_to_string(
		generation->symbols, result_type->name_symbol_id
	);
	if (!result_name || strcmp(result_name, "Bool") != 0) {
		return 0;
	}
	*p_shape = shape;
	return 1;
}

static int function_graph_applied_source_argument(
	struct function_graph_generation* generation,
	uint32_t ast,
	uint32_t* p_argument_index
) {
	if (!generation || !p_argument_index || ast >= generation->asts->node_count) {
		return -1;
	}
	uint32_t current = ast;
	uint32_t argument_count = 0;
	while (current < generation->asts->node_count &&
		generation->asts->nodes[current].tag == PROTOTYPE_AST_APP) {
		argument_count++;
		current = generation->asts->nodes[current].as.app.function;
	}
	if (argument_count != 2 || current >= generation->asts->node_count ||
		generation->asts->nodes[current].tag != PROTOTYPE_AST_VAR) {
		return 0;
	}
	uint32_t binder = generation->asts->nodes[current].as.var.ast_binder_id;
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		if (generation->source_argument_ast_binders[i] == binder) {
			*p_argument_index = i;
			return 1;
		}
	}
	return 0;
}

static int function_graph_find_binary_callback(
	struct function_graph_generation* generation,
	uint32_t ast,
	uint32_t* p_argument_index
) {
	uint32_t index;
	int applied = function_graph_applied_source_argument(generation, ast, &index);
	if (applied != 0) {
		return applied < 0 ? -1 : (*p_argument_index = index, 1);
	}
	if (ast >= generation->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &generation->asts->nodes[ast];
	switch (node->tag) {
		case PROTOTYPE_AST_APP: {
			int status = function_graph_find_binary_callback(
				generation, node->as.app.function, p_argument_index
			);
			return status != 0 ? status : function_graph_find_binary_callback(
				generation, node->as.app.argument, p_argument_index
			);
		}
		case PROTOTYPE_AST_LAMBDA:
			return function_graph_find_binary_callback(
				generation, node->as.lambda.body, p_argument_index
			);
		case PROTOTYPE_AST_MATCH:
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				int status = function_graph_find_binary_callback(
					generation,
					generation->asts->cases[node->as.match.first_case + i].body,
					p_argument_index
				);
				if (status != 0) {
					return status;
				}
			}
			return function_graph_find_binary_callback(
				generation, node->as.match.scrutinee, p_argument_index
			);
		case PROTOTYPE_AST_COMPUTATION_BLOCK:
			for (uint32_t i = 0; i < node->as.block.item_count; ++i) {
				int status = function_graph_find_binary_callback(
					generation,
					generation->asts->block_items[node->as.block.first_item + i],
					p_argument_index
				);
				if (status != 0) {
					return status;
				}
			}
			return 0;
		case PROTOTYPE_AST_BLOCK_BINDING:
			return function_graph_find_binary_callback(
				generation, node->as.block_binding.value, p_argument_index
			);
		case PROTOTYPE_AST_BLOCK_EXPRESSION:
			return function_graph_find_binary_callback(
				generation, node->as.block_expression.term, p_argument_index
			);
		case PROTOTYPE_AST_ASCRIPTION:
			return function_graph_find_binary_callback(
				generation, node->as.ascription.term, p_argument_index
			);
		default:
			return 0;
	}
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
		uint32_t ignored_result;
		uint32_t ignored_effect;
		int ignored_totality;
		if (failed->compiled_classifier == PROTOTYPE_INVALID_ID ||
			accepted_definition_final_computation(
				generation->terms,
				generation->type_declarations,
				failed->compiled_classifier,
				&ignored_result,
				&ignored_effect,
				&ignored_totality
			) != 0) {
			generation->failure_reason =
				PROTOTYPE_FUNCTION_GRAPH_REASON_NONFUNCTION_OWNER;
		}
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
		generation->failure_reason =
			generation->view.final_totality ==
				PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE ?
				PROTOTYPE_FUNCTION_GRAPH_REASON_NONTOTAL_OWNER :
				PROTOTYPE_FUNCTION_GRAPH_REASON_EFFECTFUL_OWNER;
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
		generation->symbols, "interface", owner_name, &generation->interface_symbol
		) != 0 || function_graph_internal_symbol(
		generation->symbols, "adapter", owner_name, &generation->adapter_symbol
		) != 0 || function_graph_internal_symbol(
			generation->symbols, "certified", owner_name, &generation->runner_symbol
		) != 0 || function_graph_internal_symbol(
			generation->symbols, "executable", owner_name,
			&generation->executable_symbol
		) != 0 || (generation->returned_symbol = symbol_intern(
			generation->symbols, "returned", 8
		)) < 0) {
		return -1;
	}
	generation->argument_count = 0;
	generation->nested_recursive = 0;
	generation->root_helper_match = 0;
	generation->runner_root_helper_graph_value = PROTOTYPE_INVALID_ID;
	generation->certified_argument_index = PROTOTYPE_INVALID_ID;
	generation->certified_domain_classifier = PROTOTYPE_INVALID_ID;
	generation->certified_package_symbol = -1;
	generation->certified_package_assignment = PROTOTYPE_INVALID_ID;
	generation->graph_interface_ast_binder = PROTOTYPE_INVALID_ID;
	generation->graph_interface_symbol = -1;
	generation->graph_interface_value = PROTOTYPE_INVALID_ID;
	generation->interface_assignment = PROTOTYPE_INVALID_ID;
	generation->adapter_assignment = PROTOTYPE_INVALID_ID;
	generation->runner_interface_value = PROTOTYPE_INVALID_ID;
	generation->runner_callback_value = PROTOTYPE_INVALID_ID;
	generation->owner_recursive_match = PROTOTYPE_INVALID_ID;
	uint32_t current;
	if (function_graph_collect_argument_path(
			generation, owner->ast, &current
		) != 0) {
		return 1;
	}
	generation->recursive_site_count = 0;
	if (function_graph_collect_recursive_sites(
			generation, owner->ast
		) != 0) {
		return 1;
	}
	if (generation->argument_count == 0) {
		return 1;
	}
	uint32_t certified_argument;
	int certified_status = function_graph_find_binary_callback(
		generation, owner->ast, &certified_argument
	);
	if (certified_status < 0) {
		return -1;
	}
	if (certified_status > 0) {
		struct function_graph_binary_classifier classifier;
		if (certified_argument >= generation->argument_count ||
			function_graph_binary_bool_classifier(
				generation,
				generation->source_argument_classifiers[certified_argument],
				&classifier
			) <= 0) {
			return 1;
		}
		generation->certified_argument_index = certified_argument;
		generation->certified_domain_classifier = classifier.domain;
	}
	generation->match_argument_index = PROTOTYPE_INVALID_ID;
	if (current < generation->asts->node_count &&
		generation->asts->nodes[current].tag == PROTOTYPE_AST_MATCH) {
		uint32_t scrutinee = generation->asts->nodes[current].as.match.scrutinee;
		if (scrutinee < generation->asts->node_count &&
			generation->asts->nodes[scrutinee].tag == PROTOTYPE_AST_VAR) {
			uint32_t binder = generation->asts->nodes[scrutinee].as.var.ast_binder_id;
			for (uint32_t i = 0; i < generation->argument_count; ++i) {
				if (generation->source_argument_ast_binders[i] == binder) {
					generation->match_argument_index = i;
					break;
				}
			}
		} else {
			int helper_status = function_graph_named_call_site_open(
				generation, scrutinee, &generation->root_helper
			);
			if (helper_status < 0) {
				return -1;
			}
			if (helper_status > 0) {
				generation->root_helper_match = 1;
				generation->match_argument_index = generation->argument_count;
			}
		}
	}
	if (generation->match_argument_index == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	generation->graph_parameter_count = generation->root_helper_match ?
		generation->argument_count : generation->match_argument_index;
	uint32_t input_type_id;
	uint32_t input_owner_arguments[16];
	uint32_t input_owner_argument_count;
	const struct prototype_typed_occurrence* root_helper_occurrence =
		generation->root_helper_match ? function_graph_any_occurrence(
			generation->metadata,
			generation->asts->nodes[current].as.match.scrutinee
		) : NULL;
	if (generation->root_helper_match && !root_helper_occurrence) {
		return 1;
	}
	uint32_t match_classifier = generation->root_helper_match ?
		PROTOTYPE_INVALID_ID :
		generation->source_argument_classifiers[generation->match_argument_index];
	if (generation->root_helper_match) {
		uint32_t helper_effect_row;
		int helper_totality;
		if (accepted_definition_final_computation(
				generation->terms,
				generation->type_declarations,
				root_helper_occurrence->classifier,
				&match_classifier,
				&helper_effect_row,
				&helper_totality
			) != 0) {
			return 1;
		}
	}
	if (prototype_term_type_instance_info(
			generation->terms,
			match_classifier,
			&input_type_id,
			input_owner_arguments,
			&input_owner_argument_count
		) != 0 || input_type_id >=
			generation->type_declarations->semantic_schema.type_count) {
		return 1;
	}
	const struct prototype_type_declaration* input_type =
		&generation->type_declarations->semantic_schema.type_declarations[
			input_type_id
		];
	for (uint32_t i = input_type->parameter_count;
		i < input_owner_argument_count;
		++i) {
		for (uint32_t source = 0;
			source < generation->match_argument_index;
			++source) {
			if (prototype_term_contains_free_binding(
					generation->terms,
					input_owner_arguments[i],
					generation->source_argument_bindings[source]
				) && source < generation->graph_parameter_count) {
				generation->graph_parameter_count = source;
			}
		}
	}
	if (generation->nested_recursive) {
		const struct prototype_typed_occurrence* recursive_match_occurrence =
			function_graph_occurrence(
				generation->metadata,
				generation->owner_recursive_match,
				PROTOTYPE_TYPED_OCCURRENCE_MATCH
			);
		if (!recursive_match_occurrence || recursive_match_occurrence->case_count != 1) {
			return 1;
		}
		const struct prototype_ast_node* recursive_match_ast =
			&generation->asts->nodes[generation->owner_recursive_match];
		const struct prototype_typed_occurrence* recursive_scrutinee_occurrence =
			function_graph_any_occurrence(
				generation->metadata, recursive_match_ast->as.match.scrutinee
			);
		uint32_t type_id;
		uint32_t owner_arguments[16];
		uint32_t owner_argument_count;
		if (!recursive_scrutinee_occurrence ||
			prototype_term_type_instance_info(
				generation->terms,
				recursive_scrutinee_occurrence->classifier,
				&type_id,
				owner_arguments,
				&owner_argument_count
			) != 0 || type_id >=
				generation->type_declarations->semantic_schema.type_count) {
			return 1;
		}
		const struct prototype_type_declaration* owner_type =
			&generation->type_declarations->semantic_schema.type_declarations[type_id];
		for (uint32_t i = owner_type->parameter_count;
			i < owner_argument_count; ++i) {
			for (uint32_t source = 0; source < generation->argument_count; ++source) {
				if (prototype_term_contains_free_binding(
						generation->terms,
						owner_arguments[i],
						generation->source_argument_bindings[source]
					)) {
					if (source < generation->graph_parameter_count) {
						generation->graph_parameter_count = source;
					}
				}
			}
		}
	}
	generation->source_body = current;
	generation->owner_source_lambda = owner->ast;
	uint32_t input_argument = generation->match_argument_index;
	generation->owner_source_input_binder = generation->root_helper_match ?
		PROTOTYPE_INVALID_ID : generation->source_argument_ast_binders[input_argument];
	generation->owner_source_input_binding = generation->root_helper_match ?
		PROTOTYPE_INVALID_ID : generation->source_argument_bindings[input_argument];
	generation->owner_input_classifier = match_classifier;
	generation->runner_input_symbol = generation->root_helper_match ? -1 :
		generation->source_argument_symbols[input_argument];
	generation->branch_precise = 0;
	if (current < generation->asts->node_count &&
		generation->asts->nodes[current].tag == PROTOTYPE_AST_MATCH) {
		const struct prototype_ast_node* match = &generation->asts->nodes[current];
		if (generation->root_helper_match ||
			(match->as.match.scrutinee < generation->asts->node_count &&
			generation->asts->nodes[match->as.match.scrutinee].tag ==
				PROTOTYPE_AST_VAR &&
			generation->asts->nodes[match->as.match.scrutinee].as.var.ast_binder_id ==
				generation->owner_source_input_binder)) {
			generation->branch_precise = 1;
			generation->owner_source_match = current;
		}
	}
	if (function_graph_project_final_computation(generation) != 0) {
		return 1;
	}
	return 0;
}

static int function_graph_generate_graph_type(
	struct function_graph_generation* generation
) {
	uint32_t output_type;
	uint32_t graph_argument_binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t graph_argument_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map argument_maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t graph_output_binder = prototype_ast_new_binder(generation->asts);
	if (graph_output_binder == PROTOTYPE_INVALID_ID || prototype_ast_type_add(
			generation->asts,
			generation->graph_symbol,
			generation->span,
			generation->span,
			&generation->graph_type_def
		) != 0) {
		fprintf(stderr, "function graph family header failed owner=%d\n",
			generation->owner_symbol);
		return -1;
	}
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		uint32_t argument_type;
		graph_argument_binders[i] = prototype_ast_new_binder(generation->asts);
		if (graph_argument_binders[i] == PROTOTYPE_INVALID_ID ||
			function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				argument_maps,
				i,
				&argument_type
			) != 0 || prototype_ast_type_add_family_binder(
				generation->asts,
				generation->graph_type_def,
				graph_argument_binders[i],
				generation->source_argument_symbols[i],
				argument_type,
				i < generation->graph_parameter_count ?
					PROTOTYPE_AST_FAMILY_BINDER_PARAMETER :
					PROTOTYPE_AST_FAMILY_BINDER_INDEX,
				generation->span
			) != 0 || prototype_ast_type_expr_var(
				generation->asts,
				graph_argument_binders[i],
				generation->source_argument_symbols[i],
				generation->span,
				&graph_argument_values[i]
			) != 0) {
			fprintf(stderr,
				"function graph family argument failed owner=%d argument=%u parameters=%u\n",
				generation->owner_symbol, i, generation->graph_parameter_count);
			return -1;
		}
		argument_maps[i] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = graph_argument_binders[i],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generation->source_argument_symbols[i]
		};
		if (i == generation->certified_argument_index) {
			uint32_t domain_type;
			uint32_t interface_type;
			generation->graph_interface_ast_binder =
				prototype_ast_new_binder(generation->asts);
			generation->graph_interface_symbol = symbol_intern(
				generation->symbols, "ComparatorGraph", 15
			);
			if (i >= generation->graph_parameter_count ||
				generation->graph_interface_ast_binder == PROTOTYPE_INVALID_ID ||
				generation->graph_interface_symbol < 0 || function_graph_projection_type(
					generation,
					generation->certified_domain_classifier,
					argument_maps,
					i + 1,
					&domain_type
				) != 0 || function_graph_binary_graph_family_type(
					generation, domain_type, &interface_type
				) != 0 || prototype_ast_type_add_family_binder(
					generation->asts,
					generation->graph_type_def,
					generation->graph_interface_ast_binder,
					generation->graph_interface_symbol,
					interface_type,
					PROTOTYPE_AST_FAMILY_BINDER_PARAMETER,
					generation->span
				) != 0 || prototype_ast_type_expr_var(
					generation->asts,
					generation->graph_interface_ast_binder,
					generation->graph_interface_symbol,
					generation->span,
					&generation->graph_interface_value
				) != 0) {
				return -1;
			}
		}
	}
	if (function_graph_projection_type(
			generation, generation->view.final_result_type,
			argument_maps, generation->argument_count, &output_type
		) != 0 || prototype_ast_type_add_family_binder(
			generation->asts,
			generation->graph_type_def,
			graph_output_binder,
			symbol_intern(generation->symbols, "output", 6),
			output_type,
			PROTOTYPE_AST_FAMILY_BINDER_INDEX,
			generation->span
		) != 0) {
		fprintf(stderr, "function graph family output failed owner=%d arguments=%u\n",
			generation->owner_symbol, generation->argument_count);
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
		fprintf(stderr, "function graph source match metadata failed owner=%d match=%u\n",
			generation->owner_symbol, generation->owner_source_match);
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
			fprintf(stderr,
				"function graph input refinement rejected case=%u source-fields=%u operation-fields=%u status=%d\n",
				case_index, source_case->binder_count,
				operation_case->binder_count,
				operation_case->refinement_status);
			return -1;
		}
		uint32_t field_types[FUNCTION_GRAPH_MAX_BINDINGS];
		uint32_t field_binders[FUNCTION_GRAPH_MAX_BINDINGS];
		int field_symbols[FUNCTION_GRAPH_MAX_BINDINGS];
		uint32_t field_values[FUNCTION_GRAPH_MAX_BINDINGS];
		struct function_graph_binding_map bindings[FUNCTION_GRAPH_MAX_BINDINGS];
		uint32_t binding_count = generation->graph_parameter_count;
		uint32_t field_count = 0;
		if (binding_count > FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		for (uint32_t i = 0; i < binding_count; ++i) {
			bindings[i] = argument_maps[i];
		}
		const struct prototype_ast_match_case* recursive_source_case = NULL;
		const struct prototype_typed_occurrence_match_case* recursive_operation_case =
			NULL;
		uint32_t nested_outer_first_field = 0;
		if (generation->nested_recursive) {
			const struct prototype_ast_node* recursive_match =
				&generation->asts->nodes[generation->owner_recursive_match];
			const struct prototype_typed_occurrence* recursive_occurrence =
				function_graph_occurrence(
					generation->metadata,
					generation->owner_recursive_match,
					PROTOTYPE_TYPED_OCCURRENCE_MATCH
				);
			if (!recursive_occurrence || recursive_match->as.match.case_count != 1 ||
				recursive_occurrence->case_count != 1) {
				fprintf(stderr,
					"function graph nested outer match metadata failed match=%u source-cases=%u operation-cases=%u\n",
					generation->owner_recursive_match,
					recursive_match->as.match.case_count,
					recursive_occurrence ? recursive_occurrence->case_count : 0);
				return -1;
			}
			recursive_source_case = &generation->asts->cases[
				recursive_match->as.match.first_case
			];
			recursive_operation_case =
				&generation->metadata->typed_occurrences.cases[
					recursive_occurrence->first_case
				];
		}
		uint32_t source_first_field = field_count;
		if (function_graph_append_case_fields(
				generation,
				source_case,
				operation_case,
				bindings,
				&binding_count,
				field_types,
				field_binders,
				field_symbols,
				field_values,
				&field_count
			) != 0 || function_graph_record_case_field_origins(
				generation, case_index, source_case, source_first_field
			) != 0) {
			fprintf(stderr,
				"function graph input fields failed case=%u bindings=%u fields=%u\n",
				case_index, binding_count, field_count);
			return -1;
		}
		if (generation->nested_recursive) {
			uint32_t refined_classifier;
			nested_outer_first_field = field_count;
			if (!recursive_operation_case || function_graph_append_case_fields(
					generation,
					recursive_source_case,
					recursive_operation_case,
					bindings,
					&binding_count,
					field_types,
					field_binders,
					field_symbols,
					field_values,
					&field_count
					) != 0 || function_graph_record_case_field_origins(
						generation,
						case_index,
						recursive_source_case,
						nested_outer_first_field
					) != 0 || function_graph_case_result_classifier(
					generation, operation_case, &refined_classifier
				) != 0 || function_graph_refine_binding_values(
					generation, refined_classifier, bindings, binding_count
				) != 0 || function_graph_reproject_case_field_types(
					generation,
					recursive_operation_case,
					bindings,
					binding_count,
					nested_outer_first_field,
					field_types
				) != 0) {
				fprintf(stderr,
					"function graph nested index projection failed case=%u\n",
					case_index);
				return -1;
			}
		}
		if (!generation->nested_recursive && function_graph_append_root_suffix_fields(
				generation,
				bindings,
				&binding_count,
				field_types,
				field_binders,
				field_symbols,
				field_values,
				&field_count
			) != 0) {
			fprintf(stderr,
				"function graph root suffix fields failed case=%u fields=%u bindings=%u\n",
				case_index, field_count, binding_count);
			return -1;
		}
		uint32_t terminal_body = source_case->body;
		uint32_t joined_field_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
		if (!generation->nested_recursive &&
			function_graph_append_joined_branch_fields(
				generation,
				terminal_body,
				bindings,
				&binding_count,
				field_types,
				field_binders,
				field_symbols,
				field_values,
				&field_count,
				joined_field_values,
				&terminal_body
			) != 0) {
			fprintf(stderr,
				"function graph joined branch fields failed case=%u body=%u fields=%u bindings=%u\n",
				case_index, terminal_body, field_count, binding_count);
			return -1;
		}
		const struct prototype_ast_match_case* terminal_source_case = NULL;
		const struct prototype_typed_occurrence_match_case* terminal_operation_case =
			NULL;
		if (generation->nested_recursive && terminal_body <
				generation->asts->node_count &&
			generation->asts->nodes[terminal_body].tag ==
				PROTOTYPE_AST_COMPUTATION_BLOCK) {
			const struct prototype_ast_node* block =
				&generation->asts->nodes[terminal_body];
			uint32_t nested_match_ast = PROTOTYPE_INVALID_ID;
			for (uint32_t item_index = 0; item_index < block->as.block.item_count;
				++item_index) {
				uint32_t item = generation->asts->block_items[
					block->as.block.first_item + item_index
				];
				if (item < generation->asts->node_count &&
					generation->asts->nodes[item].tag ==
						PROTOTYPE_AST_BLOCK_EXPRESSION) {
					uint32_t expression =
						generation->asts->nodes[item].as.block_expression.term;
					if (expression < generation->asts->node_count &&
						generation->asts->nodes[expression].tag ==
							PROTOTYPE_AST_MATCH) {
						nested_match_ast = expression;
					}
				}
			}
			const struct prototype_typed_occurrence* terminal_occurrence =
				function_graph_occurrence(
					generation->metadata,
					nested_match_ast,
					PROTOTYPE_TYPED_OCCURRENCE_MATCH
				);
			if (nested_match_ast == PROTOTYPE_INVALID_ID || !terminal_occurrence ||
				generation->asts->nodes[nested_match_ast].as.match.case_count != 1 ||
				terminal_occurrence->case_count != 1) {
				fprintf(stderr,
					"function graph nested terminal match metadata failed body=%u match=%u operation=%p\n",
					terminal_body, nested_match_ast, (void*)terminal_occurrence);
				return -1;
			}
			terminal_source_case = &generation->asts->cases[
				generation->asts->nodes[nested_match_ast].as.match.first_case
			];
			terminal_operation_case =
				&generation->metadata->typed_occurrences.cases[
					terminal_occurrence->first_case
				];
				uint32_t terminal_first_field = field_count;
				if (function_graph_append_case_fields(
						generation,
						terminal_source_case,
					terminal_operation_case,
					bindings,
					&binding_count,
					field_types,
					field_binders,
					field_symbols,
					field_values,
						&field_count
					) != 0 || function_graph_record_case_field_origins(
						generation,
						case_index,
						terminal_source_case,
						terminal_first_field
					) != 0) {
				fprintf(stderr,
					"function graph nested terminal fields failed case=%u bindings=%u fields=%u\n",
					case_index, binding_count, field_count);
				return -1;
			}
				terminal_body = terminal_source_case->body;
			}
			const struct prototype_ast_match_case* recursive_result_source_case = NULL;
			const struct prototype_typed_occurrence_match_case*
				recursive_result_operation_case = NULL;
			uint32_t recursive_result_ih = PROTOTYPE_INVALID_ID;
			if (!generation->nested_recursive && terminal_body <
					generation->asts->node_count &&
				generation->asts->nodes[terminal_body].tag == PROTOTYPE_AST_MATCH) {
				const struct prototype_ast_node* recursive_result_match =
					&generation->asts->nodes[terminal_body];
				const struct prototype_typed_occurrence* recursive_result_occurrence =
					function_graph_occurrence(
						generation->metadata,
						terminal_body,
						PROTOTYPE_TYPED_OCCURRENCE_MATCH
					);
				uint32_t scrutinee = recursive_result_match->as.match.scrutinee;
				if (recursive_result_match->as.match.case_count == 1 &&
					recursive_result_occurrence &&
					recursive_result_occurrence->case_count == 1 &&
					scrutinee < generation->asts->node_count &&
					generation->asts->nodes[scrutinee].tag ==
						PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
					recursive_result_source_case = &generation->asts->cases[
						recursive_result_match->as.match.first_case
					];
					recursive_result_operation_case =
						&generation->metadata->typed_occurrences.cases[
							recursive_result_occurrence->first_case
						];
					recursive_result_ih = scrutinee;
					if (function_graph_append_case_fields(
							generation,
							recursive_result_source_case,
							recursive_result_operation_case,
							bindings,
							&binding_count,
							field_types,
							field_binders,
							field_symbols,
							field_values,
							&field_count
						) != 0) {
						return -1;
					}
					terminal_body = recursive_result_source_case->body;
				}
			}
				struct function_graph_certified_block certified_block;
			struct function_graph_terminal_plan terminal_plan = {
				.body_ast = terminal_body
			};
		int certified_block_status = function_graph_certified_block_open(
			generation, terminal_body, &certified_block
		);
		if (certified_block_status < 0) {
			return -1;
		}
		if (certified_block_status > 0) {
			if (field_count + 2 > FUNCTION_GRAPH_MAX_BINDINGS ||
				binding_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
				return -1;
			}
			uint32_t decision_field = field_count;
			uint32_t witness_field = field_count + 1;
			int bool_symbol = symbol_intern(generation->symbols, "Bool", 4);
			int witness_symbol = symbol_intern(
				generation->symbols, "callbackGraph", 13
			);
			field_binders[decision_field] = prototype_ast_new_binder(generation->asts);
			field_binders[witness_field] = prototype_ast_new_binder(generation->asts);
			field_symbols[decision_field] = certified_block.source_block_symbol;
			field_symbols[witness_field] = witness_symbol;
			uint32_t decision_value;
			uint32_t witness_value;
			uint32_t left_value;
			uint32_t right_value;
			if (bool_symbol < 0 || witness_symbol < 0 ||
				field_binders[decision_field] == PROTOTYPE_INVALID_ID ||
				field_binders[witness_field] == PROTOTYPE_INVALID_ID ||
				prototype_ast_type_expr_name(
					generation->asts, bool_symbol, generation->span,
					&field_types[decision_field]
				) != 0 || prototype_ast_var(
					generation->asts, field_binders[decision_field],
					field_symbols[decision_field], generation->span, &decision_value
				) != 0 || prototype_ast_var(
					generation->asts, field_binders[witness_field],
					field_symbols[witness_field], generation->span, &witness_value
				) != 0 || function_graph_clone_value(
					generation, certified_block.callback_arguments[0], bindings,
					binding_count, NULL, 0, &left_value
				) != 0 || function_graph_clone_value(
					generation, certified_block.callback_arguments[1], bindings,
					binding_count, NULL, 0, &right_value
				) != 0) {
				return -1;
			}
			uint32_t left_type;
			uint32_t right_type;
			uint32_t decision_type;
			if (function_graph_value_type_expr(
					generation, left_value, &left_type
				) != 0 || function_graph_value_type_expr(
					generation, right_value, &right_type
				) != 0 || function_graph_value_type_expr(
					generation, decision_value, &decision_type
				) != 0) {
				return -1;
			}
			uint32_t witness_type = generation->graph_interface_value;
			if (prototype_ast_type_expr_app(
					generation->asts, witness_type, left_type, generation->span,
					&witness_type
				) != 0 || prototype_ast_type_expr_app(
					generation->asts, witness_type, right_type, generation->span,
					&witness_type
				) != 0 || prototype_ast_type_expr_app(
					generation->asts, witness_type, decision_type, generation->span,
					&witness_type
				) != 0) {
				return -1;
			}
			field_types[witness_field] = witness_type;
			field_values[decision_field] = decision_value;
			field_values[witness_field] = witness_value;
			const struct prototype_typed_occurrence* decision_occurrence = NULL;
			for (uint32_t i = 0; i < certified_block.helper.argument_count; ++i) {
				uint32_t argument = certified_block.helper.arguments[i];
				if (argument < generation->asts->node_count &&
					generation->asts->nodes[argument].tag == PROTOTYPE_AST_VAR &&
					generation->asts->nodes[argument].as.var.ast_binder_id ==
						certified_block.source_block_binder) {
					decision_occurrence = function_graph_any_occurrence(
						generation->metadata, argument
					);
					break;
				}
			}
			if (!decision_occurrence ||
				decision_occurrence->binding_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			bindings[binding_count++] = (struct function_graph_binding_map) {
				.source_ast_binder = certified_block.source_block_binder,
				.source_binding = decision_occurrence->binding_id,
				.target_ast_binder = field_binders[decision_field],
				.target_value = PROTOTYPE_INVALID_ID,
				.symbol_id = field_symbols[decision_field]
			};
				field_count += 2;
				terminal_body = certified_block.helper.call_ast;
				terminal_plan.body_ast = terminal_body;
			}
			if (certified_block_status == 0 &&
				function_graph_terminal_plan_open(
					generation, terminal_body, &terminal_plan
				) != 0) {
				fprintf(stderr,
					"function graph terminal block projection failed case=%u body=%u\n",
					case_index, terminal_body);
				return -1;
			}
			terminal_body = terminal_plan.body_ast;
				uint32_t ih_asts[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
			uint32_t ih_count = 0;
			if (recursive_result_ih != PROTOTYPE_INVALID_ID) {
				ih_asts[ih_count++] = recursive_result_ih;
			}
			if (function_graph_collect_terminal_ih(
					generation->asts, &terminal_plan, ih_asts,
				FUNCTION_GRAPH_MAX_RECURSIVE_CALLS, &ih_count
			) != 0) {
			fprintf(stderr, "function graph terminal IH collection failed body=%u tag=%d\n",
				terminal_body,
				terminal_body < generation->asts->node_count ?
					generation->asts->nodes[terminal_body].tag : -1);
			return -1;
		}
			struct function_graph_recursive_call recursive[
				FUNCTION_GRAPH_MAX_RECURSIVE_CALLS + 1
			];
		uint32_t recursive_argument_values[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS]
			[FUNCTION_GRAPH_MAX_ARGUMENTS];
				for (uint32_t i = 0; i < ih_count; ++i) {
					const struct prototype_ast_node* ih = &generation->asts->nodes[ih_asts[i]];
					const struct function_graph_terminal_binding* terminal_origin =
						function_graph_terminal_binding_for_ih(
							generation->asts, &terminal_plan, ih_asts[i]
						);
					int destructured_result = ih_asts[i] == recursive_result_ih;
				const struct function_graph_binding_map* argument =
				function_graph_find_binding(
					bindings, binding_count, ih->as.induction_hypothesis.ast_binder_id
				);
				if (!argument || field_count + (destructured_result ? 1 : 2) >
						FUNCTION_GRAPH_MAX_BINDINGS) {
				if (generation->nested_recursive) {
					fprintf(stderr,
						"function graph nested recursive binding failed ih=%u binder=%u found=%d fields=%u\n",
						ih_asts[i], ih->as.induction_hypothesis.ast_binder_id,
						argument != NULL, field_count);
				}
				return -1;
			}
				uint32_t output_binder = destructured_result ? PROTOTYPE_INVALID_ID :
					prototype_ast_new_binder(generation->asts);
				uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
					int output_symbol = destructured_result ? -1 : terminal_origin ?
						terminal_origin->source_symbol_id :
						symbol_intern(generation->symbols, "recursiveOutput", 15);
			int graph_symbol = symbol_intern(generation->symbols, "recursiveGraph", 14);
			const struct function_graph_recursive_site* site =
				function_graph_recursive_site_for_ih(generation, ih_asts[i]);
			if (generation->nested_recursive) {
				if (!site || site->explicit_argument_count < 2 ||
					generation->argument_count - generation->graph_parameter_count != 3) {
					fprintf(stderr,
						"function graph nested recursive site failed ih=%u site=%p explicit=%u arguments=%u parameters=%u\n",
						ih_asts[i], (void*)site,
						site ? site->explicit_argument_count : 0,
						generation->argument_count,
						generation->graph_parameter_count);
					return -1;
				}
				for (uint32_t parameter = 0;
					parameter < generation->graph_parameter_count; ++parameter) {
					if (prototype_ast_var(
							generation->asts,
							argument_maps[parameter].target_ast_binder,
							argument_maps[parameter].symbol_id,
							generation->span,
							&recursive_argument_values[i][parameter]
						) != 0) {
						return -1;
					}
				}
				if (function_graph_clone_value(
						generation,
						site->explicit_arguments[0],
						bindings,
						binding_count,
						NULL,
						0,
						&recursive_argument_values[i][generation->graph_parameter_count]
					) != 0) {
					return -1;
				}
				uint32_t access;
				if (function_graph_clone_value(
						generation, site->ih_ast, bindings, binding_count,
						NULL, 0, &access
					) != 0) {
					return -1;
				}
				for (uint32_t explicit_index = 0;
					explicit_index + 1 < site->explicit_argument_count;
					++explicit_index) {
					uint32_t explicit_argument;
					if (function_graph_clone_value(
							generation,
							site->explicit_arguments[explicit_index],
							bindings,
							binding_count,
							NULL,
							0,
							&explicit_argument
						) != 0 || prototype_ast_app(
							generation->asts,
							access,
							explicit_argument,
							generation->span,
							&access
						) != 0) {
						return -1;
					}
				}
				recursive_argument_values[i][generation->graph_parameter_count + 1] =
					access;
				if (function_graph_clone_value(
						generation,
						site->explicit_arguments[site->explicit_argument_count - 1],
						bindings,
						binding_count,
						NULL,
						0,
						&recursive_argument_values[i][generation->graph_parameter_count + 2]
					) != 0) {
					return -1;
				}
			} else {
				for (uint32_t argument_index = 0;
					argument_index < generation->argument_count; ++argument_index) {
					uint32_t target_binder =
						argument_maps[argument_index].target_ast_binder;
					int target_symbol = argument_maps[argument_index].symbol_id;
					if (argument_index == generation->match_argument_index) {
						target_binder = argument->target_ast_binder;
						target_symbol = argument->symbol_id;
					} else if (argument_index >= generation->root_argument_count &&
						argument_index - generation->root_argument_count <
							generation->joined_argument_count) {
						uint32_t joined = argument_index -
							generation->root_argument_count;
						const struct prototype_ast_node* joined_value =
							&generation->asts->nodes[joined_field_values[joined]];
						if (joined_value->tag != PROTOTYPE_AST_VAR) {
							return -1;
						}
						target_binder = joined_value->as.var.ast_binder_id;
						target_symbol = joined_value->as.var.symbol_id;
					}
					if (prototype_ast_var(
							generation->asts,
							target_binder,
							target_symbol,
							generation->span,
							&recursive_argument_values[i][argument_index]
						) != 0) {
						return -1;
					}
				}
				if (site && generation->match_argument_index + 1 +
					site->explicit_argument_count > generation->argument_count) {
					return -1;
				}
				for (uint32_t explicit_index = 0;
					site && explicit_index < site->explicit_argument_count;
					++explicit_index) {
					if (function_graph_clone_value(
							generation,
							site->explicit_arguments[explicit_index],
							bindings,
							binding_count,
							NULL,
							0,
							&recursive_argument_values[i][
								generation->match_argument_index + 1 + explicit_index
							]
						) != 0) {
						return -1;
					}
				}
			}
			if (!generation->nested_recursive &&
				function_graph_refine_recursive_family_arguments(
					generation,
					argument->source_binding,
					bindings,
					binding_count,
					recursive_argument_values[i]
				) != 0) {
				fprintf(stderr,
					"function graph recursive family refinement failed case=%u ih=%u binding=%u\n",
					case_index, i, argument->source_binding);
				return -1;
			}
				uint32_t recursive_output_type_var;
				uint32_t graph_field = field_count;
				if (destructured_result) {
					if (!recursive_result_operation_case ||
						function_graph_constructor_type_expr(
							generation,
							recursive_result_operation_case,
							bindings,
							binding_count,
							&recursive_output_type_var
						) != 0) {
						return -1;
					}
				} else {
					uint32_t result_source_bindings[FUNCTION_GRAPH_MAX_ARGUMENTS];
					for (uint32_t argument_index = 0;
						argument_index < generation->argument_count;
						++argument_index) {
						result_source_bindings[argument_index] =
							generation->source_argument_bindings[argument_index];
					}
					if (output_binder == PROTOTYPE_INVALID_ID || output_symbol < 0 ||
						function_graph_substitution_type(
							generation,
							generation->view.final_result_type,
							result_source_bindings,
							recursive_argument_values[i],
							generation->argument_count,
							&field_types[field_count]
						) != 0 || prototype_ast_type_expr_var(
							generation->asts,
							output_binder,
							output_symbol,
							generation->span,
							&recursive_output_type_var
						) != 0) {
						return -1;
					}
					field_binders[field_count] = output_binder;
					field_symbols[field_count] = output_symbol;
					graph_field++;
				}
				if (graph_binder == PROTOTYPE_INVALID_ID || graph_symbol < 0) {
					return -1;
				}
			uint32_t graph_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
			uint32_t graph_argument_count = 0;
			for (uint32_t argument_index = generation->graph_parameter_count;
				argument_index < generation->argument_count; ++argument_index) {
				if (function_graph_value_type_expr(
						generation,
						recursive_argument_values[i][argument_index],
						&graph_arguments[graph_argument_count++]
					) != 0) {
					return -1;
				}
			}
			graph_arguments[graph_argument_count++] = recursive_output_type_var;
				if (function_graph_self_app(
						generation, graph_arguments, graph_argument_count,
						&field_types[graph_field]
				) != 0) {
				fprintf(stderr,
					"function graph recursive graph type failed case=%u ih=%u args=%u\n",
					case_index, i, graph_argument_count);
				return -1;
			}
				field_binders[graph_field] = graph_binder;
				field_symbols[graph_field] = graph_symbol;
				recursive[i] = (struct function_graph_recursive_call) {
					.source_call_ast = site ? site->call_ast : ih_asts[i],
				.source_ih_ast = ih_asts[i],
				.source_argument_ast_binder = argument->source_ast_binder,
				.argument_symbol_id = argument->symbol_id,
				.output_ast_binder = output_binder,
				.graph_ast_binder = graph_binder,
					.output_symbol_id = output_symbol,
					.graph_symbol_id = graph_symbol
				};
					if (terminal_origin) {
						uint32_t source_binding;
						if (destructured_result || binding_count >=
								FUNCTION_GRAPH_MAX_BINDINGS ||
							function_graph_terminal_binding_core_id(
								generation, terminal_origin, &source_binding
							) != 0 || function_graph_record_origin_group(
								generation,
								case_index,
								terminal_origin->source_ast_binder,
								terminal_origin->source_symbol_id,
								PROTOTYPE_FUNCTION_GRAPH_ORIGIN_VALUE |
									PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH |
									PROTOTYPE_FUNCTION_GRAPH_ORIGIN_IH,
								field_count,
								graph_field,
								1
							) != 0) {
							return -1;
						}
						bindings[binding_count++] = (struct function_graph_binding_map) {
							.source_ast_binder = terminal_origin->source_ast_binder,
							.source_binding = source_binding,
							.target_ast_binder = output_binder,
							.target_value = PROTOTYPE_INVALID_ID,
							.symbol_id = output_symbol
						};
					}
						field_count += destructured_result ? 1 : 2;
			}
			struct function_graph_named_call_site helper_site;
			uint32_t helper_argument_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
			uint32_t replacement_count = ih_count;
			int helper_status = function_graph_named_call_site_open(
				generation, terminal_body, &helper_site
			);
			if (helper_status < 0) {
				fprintf(stderr,
					"function graph helper call inspection failed owner=%d body=%u\n",
					generation->owner_symbol, terminal_body);
				return -1;
			}
			if (helper_status > 0) {
				if (field_count + 2 > FUNCTION_GRAPH_MAX_BINDINGS ||
					function_graph_request_named_call(generation, &helper_site, NULL) != 0) {
					fprintf(stderr,
						"function graph helper request failed owner=%d helper=%d fields=%u\n",
						generation->owner_symbol, helper_site.owner_symbol_id,
						field_count);
					return -1;
				}
				for (uint32_t i = 0; i < helper_site.argument_count; ++i) {
					if (function_graph_clone_value(
							generation,
							helper_site.arguments[i],
							bindings,
							binding_count,
							recursive,
							ih_count,
							&helper_argument_values[i]
						) != 0) {
						fprintf(stderr,
							"function graph helper argument projection failed owner=%d helper=%d argument=%u\n",
							generation->owner_symbol, helper_site.owner_symbol_id, i);
						return -1;
					}
				}
			const struct prototype_typed_occurrence* helper_occurrence =
				function_graph_any_occurrence(generation->metadata, terminal_body);
				uint32_t helper_result_term;
				uint32_t helper_effect_row;
				int helper_totality;
				uint32_t helper_output_binder = prototype_ast_new_binder(
					generation->asts
				);
				uint32_t helper_graph_binder = prototype_ast_new_binder(
					generation->asts
				);
				int helper_output_symbol = symbol_intern(
					generation->symbols, "helperOutput", 12
				);
				int helper_graph_symbol = symbol_intern(
					generation->symbols, "helperGraph", 11
				);
				if (!helper_occurrence || helper_output_binder == PROTOTYPE_INVALID_ID ||
					helper_graph_binder == PROTOTYPE_INVALID_ID || helper_output_symbol < 0 ||
					helper_graph_symbol < 0 || accepted_definition_final_computation(
						generation->terms,
						generation->type_declarations,
						helper_occurrence->classifier,
						&helper_result_term,
						&helper_effect_row,
						&helper_totality
					) != 0 || function_graph_projection_type(
						generation,
						helper_result_term,
						bindings,
						binding_count,
						&field_types[field_count]
					) != 0) {
					fprintf(stderr,
						"function graph helper result field failed owner=%d helper=%d occurrence=%p classifier=%u\n",
						generation->owner_symbol, helper_site.owner_symbol_id,
						(void*)helper_occurrence,
						helper_occurrence ? helper_occurrence->classifier : PROTOTYPE_INVALID_ID);
					return -1;
				}
				field_binders[field_count] = helper_output_binder;
				field_symbols[field_count] = helper_output_symbol;
				uint32_t helper_output_value;
				if (prototype_ast_var(
						generation->asts,
						helper_output_binder,
						helper_output_symbol,
						generation->span,
						&helper_output_value
					) != 0) {
					return -1;
				}
				if (function_graph_named_graph_type_for_values(
						generation, helper_site.owner_symbol_id,
						helper_argument_values, helper_site.argument_count,
						helper_output_value,
						&field_types[field_count + 1]
					) != 0) {
					fprintf(stderr,
						"function graph helper witness field failed owner=%d helper=%d arguments=%u\n",
						generation->owner_symbol, helper_site.owner_symbol_id,
						helper_site.argument_count);
					return -1;
				}
				field_binders[field_count + 1] = helper_graph_binder;
				field_symbols[field_count + 1] = helper_graph_symbol;
				recursive[replacement_count++] =
					(struct function_graph_recursive_call) {
						.source_call_ast = helper_site.call_ast,
						.output_ast_binder = helper_output_binder,
						.graph_ast_binder = helper_graph_binder,
						.output_symbol_id = helper_output_symbol,
						.graph_symbol_id = helper_graph_symbol
					};
				field_count += 2;
			}
			uint32_t output_value;
			uint32_t input_index;
			if (function_graph_clone_value(
					generation, terminal_body, bindings, binding_count,
					recursive, replacement_count, &output_value
			) != 0 || function_graph_constructor_type_expr(
				generation, operation_case, bindings, binding_count, &input_index
			) != 0) {
			fprintf(stderr,
				"function graph endpoint projection failed case=%u body=%u tag=%d ih=%u fields=%u bindings=%u\n",
				case_index, terminal_body,
				terminal_body < generation->asts->node_count ?
					generation->asts->nodes[terminal_body].tag : -1,
				ih_count, field_count, binding_count);
				return -1;
			}
			if (generation->root_helper_match) {
				uint32_t root_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
				uint32_t graph_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
				uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
				int graph_symbol = symbol_intern(
					generation->symbols, "rootHelperGraph", 15
				);
				if (field_count >= FUNCTION_GRAPH_MAX_BINDINGS ||
					graph_binder == PROTOTYPE_INVALID_ID || graph_symbol < 0) {
					return -1;
				}
				for (uint32_t i = 0; i < generation->root_helper.argument_count; ++i) {
					if (function_graph_clone_value(
							generation,
							generation->root_helper.arguments[i],
							bindings,
							binding_count,
							recursive,
							replacement_count,
							&root_arguments[i]
						) != 0 || function_graph_value_type_expr(
							generation,
							root_arguments[i],
							&graph_arguments[i]
						) != 0) {
						return -1;
					}
				}
				graph_arguments[generation->root_helper.argument_count] = input_index;
				if (function_graph_type_graph_reference_app(
						generation,
						generation->root_helper.owner_symbol_id,
						graph_arguments,
						generation->root_helper.argument_count + 1,
						&field_types[field_count]
					) != 0) {
					return -1;
				}
				field_binders[field_count] = graph_binder;
				field_symbols[field_count] = graph_symbol;
				field_count++;
			}
			uint32_t output_index;
		uint32_t result_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 1];
		uint32_t result_argument_count = 0;
		if (function_graph_value_type_expr(
				generation, output_value, &output_index
			) != 0) {
			fprintf(stderr,
				"function graph output index failed case=%u value=%u tag=%d\n",
				case_index, output_value,
				output_value < generation->asts->node_count ?
					generation->asts->nodes[output_value].tag : -1);
			return -1;
		}
		if (generation->nested_recursive) {
			uint32_t refined_outer_value;
			const struct function_graph_binding_map* refined_outer =
				function_graph_find_binding(
					bindings,
					binding_count,
					generation->asts->case_binders[
						recursive_source_case->first_binder
					].ast_binder_id
				);
			if (!recursive_operation_case || recursive_source_case->binder_count == 0 ||
				function_graph_binding_value(
					generation, refined_outer, &refined_outer_value
				) != 0 || function_graph_value_type_expr(
					generation,
					refined_outer_value,
					&result_arguments[result_argument_count++]
				) != 0 || function_graph_constructor_type_expr(
					generation,
					recursive_operation_case,
					bindings,
					binding_count,
					&result_arguments[result_argument_count++]
				) != 0) {
				return -1;
			}
		}
		if (!generation->nested_recursive) {
			uint32_t branch_argument_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
			uint32_t branch_input_classifier;
			for (uint32_t argument_index = 0;
				argument_index < generation->argument_count;
				++argument_index) {
				if (prototype_ast_var(
						generation->asts,
						argument_maps[argument_index].target_ast_binder,
						argument_maps[argument_index].symbol_id,
						generation->span,
						&branch_argument_values[argument_index]
					) != 0) {
					fprintf(stderr,
						"function graph branch argument variable failed case=%u argument=%u binder=%u\n",
						case_index, argument_index,
						argument_maps[argument_index].target_ast_binder);
					return -1;
				}
			}
			int case_classifier_status = function_graph_case_result_classifier(
				generation, operation_case, &branch_input_classifier
			);
			int family_refinement_status = case_classifier_status == 0 ?
				function_graph_refine_family_arguments(
					generation,
					branch_input_classifier,
					bindings,
					binding_count,
					branch_argument_values
				) : -1;
			if (case_classifier_status != 0 || family_refinement_status != 0) {
				fprintf(stderr,
					"function graph branch family refinement failed case=%u classifier=%u bindings=%u constructor-status=%d refinement-status=%d\n",
					case_index, branch_input_classifier, binding_count,
					case_classifier_status, family_refinement_status);
				return -1;
			}
			for (uint32_t argument_index = generation->graph_parameter_count;
				argument_index < generation->argument_count; ++argument_index) {
				if (argument_index == generation->match_argument_index) {
					result_arguments[result_argument_count++] = input_index;
				} else if (argument_index < generation->match_argument_index) {
					if (function_graph_value_type_expr(
							generation,
							branch_argument_values[argument_index],
							&result_arguments[result_argument_count++]
						) != 0) {
						return -1;
					}
				} else if (argument_index >= generation->root_argument_count &&
					argument_index - generation->root_argument_count <
						generation->joined_argument_count &&
					function_graph_value_type_expr(
						generation,
						joined_field_values[
							argument_index - generation->root_argument_count
						],
						&result_arguments[result_argument_count++]
					) != 0) {
					fprintf(stderr,
						"function graph joined result index failed case=%u argument=%u joined=%u value=%u\n",
						case_index, argument_index,
						argument_index - generation->root_argument_count,
						joined_field_values[
							argument_index - generation->root_argument_count
						]);
					return -1;
				} else if (argument_index < generation->root_argument_count) {
					uint32_t suffix_field = source_case->binder_count +
						argument_index - generation->match_argument_index - 1;
					if (suffix_field >= field_count || function_graph_value_type_expr(
							generation,
							field_values[suffix_field],
							&result_arguments[result_argument_count++]
						) != 0) {
						return -1;
					}
				}
			}
		} else {
			result_arguments[result_argument_count++] = input_index;
		}
		result_arguments[result_argument_count++] = output_index;
		uint32_t result_type;
		if (function_graph_self_app(
				generation, result_arguments, result_argument_count, &result_type
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
			fprintf(stderr,
				"function graph constructor assembly failed case=%u fields=%u indices=%u\n",
				case_index, field_count, result_argument_count);
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

/* A generated IADT family is a TYPE_VIEW with a direct Pi classifier.  A
 * higher-order source parameter, however, receives a suspended CBPV function.
 * Publish the ordinary type-level eta expansion used at that boundary instead
 * of assigning the TYPE_VIEW a forged Thunk(Pi) classifier. */
static int function_graph_generate_interface(
	struct function_graph_generation* generation
) {
	uint32_t binders[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	int symbols[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	uint32_t types[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	uint32_t values[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	struct function_graph_binding_map maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t count = 0;
	if (!generation) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		binders[count] = prototype_ast_new_binder(generation->asts);
		symbols[count] = generation->source_argument_symbols[i];
		if (binders[count] == PROTOTYPE_INVALID_ID || function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				maps,
				i,
				&types[count]
			) != 0 || prototype_ast_var(
				generation->asts, binders[count], symbols[count], generation->span,
				&values[count]
			) != 0) {
			return -1;
		}
		maps[i] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = binders[count],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = symbols[count]
		};
		count++;
		if (i == generation->certified_argument_index) {
			uint32_t domain_type;
			binders[count] = prototype_ast_new_binder(generation->asts);
			symbols[count] = generation->graph_interface_symbol;
			if (binders[count] == PROTOTYPE_INVALID_ID || symbols[count] < 0 ||
				function_graph_projection_type(
					generation, generation->certified_domain_classifier,
					maps, i + 1, &domain_type
				) != 0 || function_graph_binary_graph_family_type(
					generation, domain_type, &types[count]
				) != 0 || prototype_ast_var(
					generation->asts, binders[count], symbols[count], generation->span,
					&values[count]
				) != 0) {
				return -1;
			}
			count++;
		}
	}
	binders[count] = prototype_ast_new_binder(generation->asts);
	symbols[count] = symbol_intern(generation->symbols, "output", 6);
	if (binders[count] == PROTOTYPE_INVALID_ID || symbols[count] < 0 ||
		function_graph_projection_type(
			generation, generation->view.final_result_type,
			maps, generation->argument_count, &types[count]
		) != 0 || prototype_ast_var(
			generation->asts, binders[count], symbols[count], generation->span,
			&values[count]
		) != 0) {
		return -1;
	}
	count++;

	uint32_t body;
	if (prototype_ast_name(
			generation->asts, generation->graph_symbol, generation->span, &body
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < count; ++i) {
		if (prototype_ast_app(
				generation->asts, body, values[i], generation->span, &body
			) != 0) {
			return -1;
		}
	}
	for (uint32_t reverse = count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		if (prototype_ast_lambda(
				generation->asts, binders[i], symbols[i], types[i], body,
				generation->span, &body
			) != 0) {
			return -1;
		}
	}
	uint32_t suspended_interface;
	if (prototype_ast_quote(
			generation->asts, body, generation->span, &suspended_interface
		) != 0) {
		return -1;
	}
	return function_graph_add_term_assignment(
		generation, generation->interface_symbol, suspended_interface,
		&generation->interface_assignment
	);
}

static int function_graph_generate_result_type(
	struct function_graph_generation* generation
) {
	uint32_t family_binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t family_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map argument_maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t interface_family_value = PROTOTYPE_INVALID_ID;
	if (prototype_ast_type_add(
			generation->asts,
			generation->result_symbol,
			generation->span,
			generation->span,
			&generation->result_type_def
		) != 0) {
		fprintf(stderr, "function graph result header failed owner=%d\n",
			generation->owner_symbol);
		return -1;
	}
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		uint32_t argument_type;
		family_binders[i] = prototype_ast_new_binder(generation->asts);
		if (family_binders[i] == PROTOTYPE_INVALID_ID ||
			function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				argument_maps,
				i,
				&argument_type
			) != 0 || prototype_ast_type_add_family_binder(
				generation->asts,
				generation->result_type_def,
				family_binders[i],
				generation->source_argument_symbols[i],
				argument_type,
				PROTOTYPE_AST_FAMILY_BINDER_PARAMETER,
				generation->span
			) != 0 || prototype_ast_type_expr_var(
				generation->asts,
				family_binders[i],
				generation->source_argument_symbols[i],
				generation->span,
				&family_values[i]
			) != 0) {
			fprintf(stderr, "function graph result argument failed owner=%d argument=%u\n",
				generation->owner_symbol, i);
			return -1;
		}
		argument_maps[i] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = family_binders[i],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generation->source_argument_symbols[i]
		};
		if (i == generation->certified_argument_index) {
			uint32_t domain_type;
			uint32_t interface_type;
			uint32_t interface_binder = prototype_ast_new_binder(generation->asts);
			int interface_symbol = symbol_intern(
				generation->symbols, "ComparatorGraph", 15
			);
			if (interface_binder == PROTOTYPE_INVALID_ID || interface_symbol < 0 ||
				function_graph_projection_type(
					generation, generation->certified_domain_classifier,
					argument_maps, i + 1, &domain_type
				) != 0 || function_graph_binary_graph_family_type(
					generation, domain_type, &interface_type
				) != 0 || prototype_ast_type_add_family_binder(
					generation->asts, generation->result_type_def,
					interface_binder, interface_symbol, interface_type,
					PROTOTYPE_AST_FAMILY_BINDER_PARAMETER, generation->span
				) != 0 || prototype_ast_type_expr_var(
					generation->asts, interface_binder, interface_symbol,
					generation->span, &interface_family_value
				) != 0) {
				return -1;
			}
		}
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
	if (function_graph_projection_type(
			generation, generation->view.final_result_type,
			argument_maps, generation->argument_count, &field_types[0]
		) != 0) {
		fprintf(stderr, "function graph result output field failed owner=%d\n",
			generation->owner_symbol);
		return -1;
	}
	uint32_t output_var;
	if (prototype_ast_type_expr_var(
			generation->asts, output_binder, output_symbol,
			generation->span, &output_var
		) != 0) {
		return -1;
	}
	uint32_t graph_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	uint32_t graph_argument_count = 0;
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		graph_arguments[graph_argument_count++] = family_values[i];
		if (i == generation->certified_argument_index) {
			graph_arguments[graph_argument_count++] = interface_family_value;
		}
	}
	graph_arguments[graph_argument_count++] = output_var;
	if (function_graph_type_name_app(
			generation, generation->graph_symbol, graph_arguments,
			graph_argument_count,
			&field_types[1]
		) != 0) {
		fprintf(stderr, "function graph result graph field failed owner=%d args=%u\n",
			generation->owner_symbol, graph_argument_count);
		fprintf(stderr, "function graph result type storage=%zu/%zu\n",
			generation->asts->type_expr_count,
			generation->asts->type_expr_capacity);
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
		fprintf(stderr, "function graph result constructor failed owner=%d\n",
			generation->owner_symbol);
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
	const uint32_t* arguments,
	uint32_t argument_count,
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
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (prototype_ast_app(
				generation->asts, owner, arguments[i], generation->span, &owner
			) != 0) {
			return -1;
		}
		if (i == generation->certified_argument_index &&
			(generation->runner_interface_value == PROTOTYPE_INVALID_ID ||
			 prototype_ast_app(
				generation->asts, owner, generation->runner_interface_value,
				generation->span, &owner
			 ) != 0)) {
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

static int function_graph_graph_constructor(
	struct function_graph_generation* generation,
	int constructor_symbol,
	const uint32_t* owner_arguments,
	uint32_t owner_argument_count,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_graph
) {
	uint32_t graph;
	uint32_t owner;
	if (!generation || !p_graph || prototype_ast_name(
			generation->asts, generation->graph_symbol, generation->span, &owner
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < owner_argument_count; ++i) {
		if (prototype_ast_app(
				generation->asts, owner, owner_arguments[i], generation->span, &owner
			) != 0) {
			return -1;
		}
		if (i == generation->certified_argument_index &&
			(generation->runner_interface_value == PROTOTYPE_INVALID_ID ||
			 prototype_ast_app(
				generation->asts, owner, generation->runner_interface_value,
				generation->span, &owner
			 ) != 0)) {
			return -1;
		}
	}
	if (prototype_ast_name_in_ast_namespace(
			generation->asts, owner, constructor_symbol, generation->span, &graph
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
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t output,
	uint32_t* p_ascribed
) {
	uint32_t output_type;
	if (argument_count > FUNCTION_GRAPH_MAX_ARGUMENTS ||
		function_graph_value_type_expr(generation, output, &output_type) != 0) {
		return -1;
	}
	uint32_t graph_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS + 2];
	uint32_t graph_argument_count = 0;
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (function_graph_value_type_expr(
				generation, arguments[i], &graph_arguments[graph_argument_count++]
			) != 0) {
			return -1;
		}
		if (i == generation->certified_argument_index) {
			if (generation->runner_interface_value == PROTOTYPE_INVALID_ID ||
				function_graph_value_type_expr(
					generation, generation->runner_interface_value,
					&graph_arguments[graph_argument_count++]
				) != 0) {
				return -1;
			}
		}
	}
	graph_arguments[graph_argument_count++] = output_type;
	uint32_t graph_type;
	if (function_graph_type_name_app(
			generation, generation->graph_symbol, graph_arguments,
			graph_argument_count, &graph_type
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
	const struct function_graph_binding_map* argument_bindings,
	const uint32_t* argument_values,
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
	uint32_t binding_count = generation->argument_count;
	if (binding_count > FUNCTION_GRAPH_MAX_BINDINGS) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		bindings[i] = argument_bindings[i];
	}
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
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = source_binder->symbol_id
		};
	}
	uint32_t terminal_body;
	if (function_graph_add_joined_branch_bindings(
			generation,
			source_case->body,
			argument_bindings,
			bindings,
			&binding_count,
			&terminal_body
		) != 0) {
		return -1;
	}
	const struct prototype_ast_match_case* recursive_result_source_case = NULL;
	const struct prototype_typed_occurrence_match_case*
		recursive_result_operation_case = NULL;
	struct prototype_ast_binder recursive_result_binders[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t recursive_result_values[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t recursive_result_ih = PROTOTYPE_INVALID_ID;
	if (terminal_body < generation->asts->node_count &&
		generation->asts->nodes[terminal_body].tag == PROTOTYPE_AST_MATCH) {
		const struct prototype_ast_node* recursive_result_match =
			&generation->asts->nodes[terminal_body];
		const struct prototype_typed_occurrence* recursive_result_occurrence =
			function_graph_occurrence(
				generation->metadata,
				terminal_body,
				PROTOTYPE_TYPED_OCCURRENCE_MATCH
			);
		uint32_t scrutinee = recursive_result_match->as.match.scrutinee;
		if (recursive_result_match->as.match.case_count == 1 &&
			recursive_result_occurrence && recursive_result_occurrence->case_count == 1 &&
			scrutinee < generation->asts->node_count &&
			generation->asts->nodes[scrutinee].tag ==
				PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
			recursive_result_source_case = &generation->asts->cases[
				recursive_result_match->as.match.first_case
			];
			recursive_result_operation_case =
				&generation->metadata->typed_occurrences.cases[
					recursive_result_occurrence->first_case
				];
			recursive_result_ih = scrutinee;
			if (function_graph_make_runtime_case_bindings(
					generation,
					recursive_result_source_case,
					recursive_result_operation_case,
					bindings,
					&binding_count,
					recursive_result_binders,
					recursive_result_values
				) != 0) {
				return -1;
			}
			terminal_body = recursive_result_source_case->body;
		}
	}
	struct function_graph_certified_block certified_block;
	struct function_graph_terminal_plan terminal_plan = {
		.body_ast = terminal_body
	};
	int certified_block_status = function_graph_certified_block_open(
		generation, terminal_body, &certified_block
	);
	uint32_t callback_left = PROTOTYPE_INVALID_ID;
	uint32_t callback_right = PROTOTYPE_INVALID_ID;
	uint32_t callback_output = PROTOTYPE_INVALID_ID;
	uint32_t callback_graph = PROTOTYPE_INVALID_ID;
	uint32_t callback_output_binder = PROTOTYPE_INVALID_ID;
	uint32_t callback_graph_binder = PROTOTYPE_INVALID_ID;
	uint32_t callback_packet_binder = PROTOTYPE_INVALID_ID;
	int callback_output_symbol = -1;
	int callback_graph_symbol = -1;
	int callback_packet_symbol = -1;
	if (certified_block_status < 0) {
		return -1;
	}
	if (certified_block_status > 0) {
		callback_output_binder = prototype_ast_new_binder(generation->asts);
		callback_graph_binder = prototype_ast_new_binder(generation->asts);
		callback_packet_binder = prototype_ast_new_binder(generation->asts);
		callback_output_symbol = certified_block.source_block_symbol;
		callback_graph_symbol = symbol_intern(
			generation->symbols, "callbackGraph", 13
		);
		callback_packet_symbol = symbol_intern(
			generation->symbols, "callbackPacket", 14
		);
		if (callback_output_binder == PROTOTYPE_INVALID_ID ||
			callback_graph_binder == PROTOTYPE_INVALID_ID ||
			callback_packet_binder == PROTOTYPE_INVALID_ID ||
			callback_output_symbol < 0 || callback_graph_symbol < 0 ||
			callback_packet_symbol < 0 || function_graph_clone_value(
				generation, certified_block.callback_arguments[0], bindings,
				binding_count, NULL, 0, &callback_left
			) != 0 || function_graph_clone_value(
				generation, certified_block.callback_arguments[1], bindings,
				binding_count, NULL, 0, &callback_right
			) != 0 || prototype_ast_var(
				generation->asts, callback_output_binder, callback_output_symbol,
				generation->span, &callback_output
			) != 0 || prototype_ast_var(
				generation->asts, callback_graph_binder, callback_graph_symbol,
				generation->span, &callback_graph
			) != 0 || binding_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		const struct prototype_typed_occurrence* decision_occurrence = NULL;
		for (uint32_t i = 0; i < certified_block.helper.argument_count; ++i) {
			uint32_t argument = certified_block.helper.arguments[i];
			if (argument < generation->asts->node_count &&
				generation->asts->nodes[argument].tag == PROTOTYPE_AST_VAR &&
				generation->asts->nodes[argument].as.var.ast_binder_id ==
					certified_block.source_block_binder) {
				decision_occurrence = function_graph_any_occurrence(
					generation->metadata, argument
				);
				break;
			}
		}
		if (!decision_occurrence ||
			decision_occurrence->binding_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		bindings[binding_count++] = (struct function_graph_binding_map) {
			.source_ast_binder = certified_block.source_block_binder,
			.source_binding = decision_occurrence->binding_id,
			.target_ast_binder = callback_output_binder,
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = callback_output_symbol
		};
		terminal_body = certified_block.helper.call_ast;
		terminal_plan.body_ast = terminal_body;
	}
	if (certified_block_status == 0 && function_graph_terminal_plan_open(
			generation, terminal_body, &terminal_plan
		) != 0) {
		return -1;
	}
	terminal_body = terminal_plan.body_ast;
	uint32_t ih_asts[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t ih_count = 0;
	if (recursive_result_ih != PROTOTYPE_INVALID_ID) {
		ih_asts[ih_count++] = recursive_result_ih;
	}
	if (function_graph_collect_terminal_ih(
			generation->asts,
			&terminal_plan,
			ih_asts,
			FUNCTION_GRAPH_MAX_RECURSIVE_CALLS,
			&ih_count
		) != 0) {
		return -1;
	}
	struct function_graph_recursive_call recursive[
		FUNCTION_GRAPH_MAX_RECURSIVE_CALLS + 1
	];
	uint32_t recursive_input_values[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t recursive_argument_values[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS]
		[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t recursive_graph_values[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t recursive_output_values[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t recursive_packet_binders[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	int recursive_packet_symbols[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	for (uint32_t i = 0; i < ih_count; ++i) {
		const struct prototype_ast_node* ih = &generation->asts->nodes[ih_asts[i]];
		const struct function_graph_terminal_binding* terminal_origin =
			function_graph_terminal_binding_for_ih(
				generation->asts, &terminal_plan, ih_asts[i]
			);
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
		for (uint32_t argument_index = 0;
			argument_index < generation->argument_count; ++argument_index) {
			recursive_argument_values[i][argument_index] =
				argument_index == generation->match_argument_index ?
					recursive_input_values[i] : argument_values[argument_index];
		}
		const struct function_graph_recursive_site* site =
			function_graph_recursive_site_for_ih(generation, ih_asts[i]);
		if (site && generation->match_argument_index + 1 +
			site->explicit_argument_count > generation->argument_count) {
			return -1;
		}
		for (uint32_t explicit_index = 0;
			site && explicit_index < site->explicit_argument_count;
			++explicit_index) {
			if (function_graph_clone_value(
					generation,
					site->explicit_arguments[explicit_index],
					bindings,
					binding_count,
					NULL,
					0,
					&recursive_argument_values[i][
						generation->match_argument_index + 1 + explicit_index
					]
				) != 0) {
				return -1;
			}
		}
		if (function_graph_refine_recursive_family_arguments(
				generation,
				argument->source_binding,
				bindings,
				binding_count,
				recursive_argument_values[i]
			) != 0) {
			return -1;
		}
		recursive[i].source_ih_ast = ih_asts[i];
		recursive[i].source_call_ast = site ? site->call_ast : ih_asts[i];
		recursive[i].source_argument_ast_binder = argument->target_ast_binder;
		recursive[i].argument_symbol_id = argument->symbol_id;
		recursive[i].output_ast_binder = prototype_ast_new_binder(generation->asts);
		recursive[i].graph_ast_binder = prototype_ast_new_binder(generation->asts);
		recursive[i].output_symbol_id = terminal_origin ?
			terminal_origin->source_symbol_id : symbol_intern(
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
		if (terminal_origin) {
			uint32_t source_binding;
			if (binding_count >= FUNCTION_GRAPH_MAX_BINDINGS ||
				function_graph_terminal_binding_core_id(
					generation, terminal_origin, &source_binding
				) != 0) {
				return -1;
			}
			bindings[binding_count++] = (struct function_graph_binding_map) {
				.source_ast_binder = terminal_origin->source_ast_binder,
				.source_binding = source_binding,
				.target_ast_binder = recursive[i].output_ast_binder,
				.target_value = PROTOTYPE_INVALID_ID,
				.symbol_id = recursive[i].output_symbol_id
			};
		}
	}
	struct function_graph_named_call_site helper_site;
	uint32_t helper_argument_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t helper_output = PROTOTYPE_INVALID_ID;
	uint32_t helper_graph = PROTOTYPE_INVALID_ID;
	uint32_t helper_output_binder = PROTOTYPE_INVALID_ID;
	uint32_t helper_graph_binder = PROTOTYPE_INVALID_ID;
	uint32_t helper_packet_binder = PROTOTYPE_INVALID_ID;
	int helper_output_symbol = -1;
	int helper_graph_symbol = -1;
	int helper_packet_symbol = -1;
	uint32_t replacement_count = ih_count;
	int helper_status = function_graph_named_call_site_open(
		generation, terminal_body, &helper_site
	);
	if (helper_status < 0) {
		return -1;
	}
	if (helper_status > 0) {
		if (function_graph_request_named_call(generation, &helper_site, NULL) != 0) {
			fprintf(stderr, "function graph helper request in runner failed\n");
			return -1;
		}
		for (uint32_t i = 0; i < helper_site.argument_count; ++i) {
			if (function_graph_clone_value(
					generation,
					helper_site.arguments[i],
					bindings,
					binding_count,
					recursive,
					ih_count,
					&helper_argument_values[i]
				) != 0) {
				return -1;
			}
		}
		helper_output_binder = prototype_ast_new_binder(generation->asts);
		helper_graph_binder = prototype_ast_new_binder(generation->asts);
		helper_packet_binder = prototype_ast_new_binder(generation->asts);
		helper_output_symbol = symbol_intern(
			generation->symbols, "helperOutput", 12
		);
		helper_graph_symbol = symbol_intern(
			generation->symbols, "helperGraph", 11
		);
		helper_packet_symbol = symbol_intern(
			generation->symbols, "helperPacket", 12
		);
		if (helper_output_binder == PROTOTYPE_INVALID_ID ||
			helper_graph_binder == PROTOTYPE_INVALID_ID ||
			helper_packet_binder == PROTOTYPE_INVALID_ID || helper_output_symbol < 0 ||
			helper_graph_symbol < 0 || helper_packet_symbol < 0 || prototype_ast_var(
				generation->asts,
				helper_output_binder,
				helper_output_symbol,
				generation->span,
				&helper_output
			) != 0 || prototype_ast_var(
				generation->asts,
				helper_graph_binder,
				helper_graph_symbol,
				generation->span,
				&helper_graph
			) != 0) {
			return -1;
		}
		recursive[replacement_count++] =
			(struct function_graph_recursive_call) {
				.source_call_ast = helper_site.call_ast,
				.output_ast_binder = helper_output_binder,
				.graph_ast_binder = helper_graph_binder,
				.output_symbol_id = helper_output_symbol,
				.graph_symbol_id = helper_graph_symbol
			};
	}
	uint32_t output;
	uint32_t input;
	if (function_graph_clone_value(
			generation,
			terminal_body,
			bindings,
			binding_count,
			recursive,
			replacement_count,
			&output
			) != 0 || function_graph_constructor_value(
				generation,
				operation_case,
				bindings,
				binding_count,
				field_values,
			source_case->binder_count,
			&input
		) != 0) {
		fprintf(stderr, "function graph runner endpoint clone failed certified=%d body=%u\n",
			certified_block_status, terminal_body);
		return -1;
	}
	uint32_t graph_arguments[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t graph_argument_count = 0;
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		graph_arguments[graph_argument_count++] = field_values[i];
	}
	for (uint32_t argument = generation->match_argument_index + 1;
		argument < generation->root_argument_count; ++argument) {
		if (graph_argument_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		graph_arguments[graph_argument_count++] = argument_values[argument];
	}
	for (uint32_t i = 0; i < generation->joined_argument_count; ++i) {
		uint32_t argument_index = generation->root_argument_count + i;
		if (argument_index >= generation->argument_count ||
			graph_argument_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		graph_arguments[graph_argument_count++] = argument_values[argument_index];
	}
	if (recursive_result_source_case) {
		for (uint32_t i = 0; i < recursive_result_source_case->binder_count; ++i) {
			if (graph_argument_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
				return -1;
			}
			graph_arguments[graph_argument_count++] = recursive_result_values[i];
		}
	}
	if (certified_block_status > 0) {
		uint32_t left_type;
		uint32_t right_type;
		uint32_t output_type;
		uint32_t graph_type;
		uint32_t ascribed_callback_graph;
		if (function_graph_value_type_expr(
				generation, callback_left, &left_type
			) != 0 || function_graph_value_type_expr(
				generation, callback_right, &right_type
			) != 0 || function_graph_value_type_expr(
				generation, callback_output, &output_type
		) != 0) {
		fprintf(stderr, "function graph runner graph assembly failed certified=%d\n",
			certified_block_status);
		return -1;
		}
		if (function_graph_value_type_expr(
				generation, generation->runner_interface_value, &graph_type
			) != 0 || prototype_ast_type_expr_app(
				generation->asts, graph_type, left_type, generation->span,
				&graph_type
			) != 0 || prototype_ast_type_expr_app(
				generation->asts, graph_type, right_type, generation->span,
				&graph_type
			) != 0 || prototype_ast_type_expr_app(
				generation->asts, graph_type, output_type, generation->span,
				&graph_type
			) != 0 || prototype_ast_ascription(
				generation->asts, callback_graph, graph_type, generation->span,
				&ascribed_callback_graph
			) != 0 || graph_argument_count + 2 > FUNCTION_GRAPH_MAX_BINDINGS) {
			fprintf(stderr, "function graph callback witness ascription failed\n");
			return -1;
		}
		graph_arguments[graph_argument_count++] = callback_output;
		graph_arguments[graph_argument_count++] = ascribed_callback_graph;
	}
	for (uint32_t i = 0; i < ih_count; ++i) {
		uint32_t ascribed_graph;
		uint32_t recursive_endpoint = recursive_output_values[i];
		int destructured_result = ih_asts[i] == recursive_result_ih;
		if (destructured_result && (!recursive_result_operation_case ||
			function_graph_constructor_value(
				generation,
				recursive_result_operation_case,
				bindings,
				binding_count,
				recursive_result_values,
				recursive_result_source_case->binder_count,
				&recursive_endpoint
			) != 0)) {
			return -1;
		}
		if (function_graph_graph_ascription(
				generation,
				recursive_graph_values[i],
				recursive_argument_values[i],
				generation->argument_count,
				recursive_endpoint,
				&ascribed_graph
			) != 0 || graph_argument_count + (destructured_result ? 1 : 2) >
				FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		if (!destructured_result) {
			graph_arguments[graph_argument_count++] = recursive_output_values[i];
		}
		graph_arguments[graph_argument_count++] = ascribed_graph;
	}
	if (helper_status > 0) {
		uint32_t ascribed_helper_graph;
		if (function_graph_named_graph_ascription(
				generation,
				helper_site.owner_symbol_id,
				helper_graph,
				helper_argument_values,
				helper_site.argument_count,
				helper_output,
				&ascribed_helper_graph
			) != 0 || graph_argument_count + 2 > FUNCTION_GRAPH_MAX_BINDINGS) {
			fprintf(stderr, "function graph helper witness ascription failed\n");
			return -1;
		}
		graph_arguments[graph_argument_count++] = helper_output;
		graph_arguments[graph_argument_count++] = ascribed_helper_graph;
	}
	if (generation->root_helper_match) {
		uint32_t root_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
		uint32_t ascribed_root_graph;
		if (generation->runner_root_helper_graph_value == PROTOTYPE_INVALID_ID ||
			graph_argument_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		for (uint32_t i = 0; i < generation->root_helper.argument_count; ++i) {
			if (function_graph_clone_value(
					generation,
					generation->root_helper.arguments[i],
					bindings,
					binding_count,
					recursive,
					replacement_count,
					&root_arguments[i]
				) != 0) {
				return -1;
			}
		}
		if (function_graph_named_graph_ascription(
				generation,
				generation->root_helper.owner_symbol_id,
				generation->runner_root_helper_graph_value,
				root_arguments,
				generation->root_helper.argument_count,
				input,
				&ascribed_root_graph
			) != 0) {
			return -1;
		}
		graph_arguments[graph_argument_count++] = ascribed_root_graph;
	}
	uint32_t graph;
	uint32_t ascribed_graph;
	uint32_t result;
	uint32_t graph_index_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t result_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		graph_index_arguments[i] = i == generation->match_argument_index ?
			input : argument_values[i];
		result_arguments[i] = argument_values[i];
	}
	if (function_graph_graph_constructor(
			generation,
			source_case->constructor_symbol_id,
			graph_index_arguments,
			generation->graph_parameter_count,
			graph_arguments,
			graph_argument_count,
			&graph
	) != 0 || function_graph_graph_ascription(
			generation,
			graph,
			graph_index_arguments,
			generation->argument_count,
			output,
			&ascribed_graph
		) != 0 || function_graph_result_constructor(
				generation, result_arguments, generation->argument_count,
					output, ascribed_graph, &result
			) != 0) {
			fprintf(stderr, "function graph runner result assembly failed\n");
			return -1;
		}
	uint32_t body = result;
	if (helper_status > 0) {
		uint32_t packet_var;
		uint32_t call;
		if (prototype_ast_var(
				generation->asts,
				helper_packet_binder,
				helper_packet_symbol,
				generation->span,
				&packet_var
			) != 0 || function_graph_named_certified_call(
				generation, &helper_site, helper_argument_values, &call
			) != 0) {
			return -1;
		}
		struct prototype_ast_binder returned_binders[2] = {
			{ helper_output_binder, helper_output_symbol },
			{ helper_graph_binder, helper_graph_symbol }
		};
		struct prototype_ast_match_case_input returned_case = {
			.constructor_symbol_id = generation->returned_symbol,
			.binders = returned_binders,
			.binder_count = 2,
			.body = body,
			.span = generation->span
		};
		uint32_t packet_match;
		uint32_t packet_type;
		uint32_t binding_item;
		uint32_t expression_item;
		if (prototype_ast_match(
				generation->asts,
				packet_var,
				&returned_case,
				1,
				generation->span,
				&packet_match
			) != 0 || function_graph_named_result_type_for_values(
				generation,
				helper_site.owner_symbol_id,
				helper_argument_values,
				helper_site.argument_count,
				&packet_type
			) != 0 || prototype_ast_block_binding(
				generation->asts,
				helper_packet_binder,
				helper_packet_symbol,
				packet_type,
				call,
				generation->span,
				&binding_item
			) != 0 || prototype_ast_block_expression(
				generation->asts,
				packet_match,
				generation->span,
				&expression_item
			) != 0) {
			fprintf(stderr, "function graph helper package block failed\n");
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
	if (recursive_result_source_case) {
		uint32_t recursive_result_index = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < ih_count; ++i) {
			if (ih_asts[i] == recursive_result_ih) {
				recursive_result_index = i;
				break;
			}
		}
		struct prototype_ast_match_case_input recursive_result_case = {
			.constructor_symbol_id = recursive_result_source_case->constructor_symbol_id,
			.binders = recursive_result_binders,
			.binder_count = recursive_result_source_case->binder_count,
			.body = body,
			.span = generation->span
		};
		if (recursive_result_index == PROTOTYPE_INVALID_ID || prototype_ast_match(
				generation->asts,
				recursive_output_values[recursive_result_index],
				&recursive_result_case,
				1,
				generation->span,
				&body
			) != 0) {
			return -1;
		}
	}
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
	if (certified_block_status > 0) {
		uint32_t packet_var;
		uint32_t call = generation->runner_callback_value;
		uint32_t domain_type;
		uint32_t interface_type;
		uint32_t left_type;
		uint32_t right_type;
		uint32_t packet_type;
		if (call == PROTOTYPE_INVALID_ID || prototype_ast_app(
				generation->asts, call, callback_left, generation->span, &call
			) != 0 || prototype_ast_app(
				generation->asts, call, callback_right, generation->span, &call
			) != 0 || prototype_ast_var(
				generation->asts, callback_packet_binder, callback_packet_symbol,
				generation->span, &packet_var
			) != 0) {
			fprintf(stderr, "function graph callback call assembly failed\n");
			return -1;
		}
		/* The callback package domain is the classifier of either operand. */
		if (function_graph_projection_type(
				generation, generation->certified_domain_classifier,
				bindings, binding_count, &domain_type
			) != 0 || function_graph_value_type_expr(
				generation, generation->runner_interface_value, &interface_type
			) != 0 || function_graph_value_type_expr(
				generation, callback_left, &left_type
			) != 0 || function_graph_value_type_expr(
				generation, callback_right, &right_type
			) != 0 || function_graph_binary_package_type(
				generation, domain_type, interface_type, left_type, right_type,
				&packet_type
			) != 0) {
			fprintf(stderr, "function graph callback package type failed\n");
			return -1;
		}
		struct prototype_ast_binder returned_binders[2] = {
			{ callback_output_binder, callback_output_symbol },
			{ callback_graph_binder, callback_graph_symbol }
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
				generation->asts, packet_var, &returned_case, 1,
				generation->span, &packet_match
			) != 0 || prototype_ast_block_binding(
				generation->asts, callback_packet_binder, callback_packet_symbol,
				packet_type, call, generation->span, &binding_item
			) != 0 || prototype_ast_block_expression(
				generation->asts, packet_match, generation->span, &expression_item
			) != 0) {
			fprintf(stderr, "function graph callback package block failed\n");
			return -1;
		}
		uint32_t items[2] = { binding_item, expression_item };
		if (prototype_ast_computation_block(
				generation->asts, items, 2, 1,
				PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
				generation->span, &body
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

static const struct function_graph_recursive_site*
function_graph_recursive_site_for_ih(
	const struct function_graph_generation* generation,
	uint32_t ih_ast
) {
	if (!generation) {
		return NULL;
	}
	for (uint32_t i = 0; i < generation->recursive_site_count; ++i) {
		if (generation->recursive_sites[i].ih_ast == ih_ast) {
			return &generation->recursive_sites[i];
		}
	}
	return NULL;
}

static int function_graph_nested_recursive_arguments(
	struct function_graph_generation* generation,
	const struct function_graph_recursive_site* site,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	const uint32_t* parameter_values,
	uint32_t* arguments
) {
	if (!generation || !site || !bindings || !parameter_values || !arguments ||
		site->explicit_argument_count < 2 ||
		generation->argument_count - generation->graph_parameter_count != 3) {
		return -1;
	}
	for (uint32_t i = 0; i < generation->graph_parameter_count; ++i) {
		arguments[i] = parameter_values[i];
	}
	if (function_graph_clone_value(
			generation,
			site->explicit_arguments[0],
			bindings,
			binding_count,
			NULL,
			0,
			&arguments[generation->graph_parameter_count]
		) != 0) {
		return -1;
	}
	uint32_t access;
	if (function_graph_clone_value(
			generation, site->ih_ast, bindings, binding_count, NULL, 0, &access
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i + 1 < site->explicit_argument_count; ++i) {
		uint32_t argument;
		if (function_graph_clone_value(
				generation,
				site->explicit_arguments[i],
				bindings,
				binding_count,
				NULL,
				0,
				&argument
			) != 0 || prototype_ast_app(
				generation->asts, access, argument, generation->span, &access
			) != 0) {
			return -1;
		}
	}
	arguments[generation->graph_parameter_count + 1] = access;
	return function_graph_clone_value(
		generation,
		site->explicit_arguments[site->explicit_argument_count - 1],
		bindings,
		binding_count,
		NULL,
		0,
		&arguments[generation->graph_parameter_count + 2]
	);
}

static int function_graph_nested_ih_call(
	struct function_graph_generation* generation,
	const struct function_graph_recursive_site* site,
	const struct function_graph_binding_map* bindings,
	uint32_t binding_count,
	uint32_t* p_call
) {
	if (!generation || !site || !p_call) {
		return -1;
	}
	const struct prototype_ast_node* ih = &generation->asts->nodes[site->ih_ast];
	const struct function_graph_binding_map* target = function_graph_find_binding(
		bindings, binding_count, ih->as.induction_hypothesis.ast_binder_id
	);
	uint32_t call;
	if (!target || prototype_ast_induction_hypothesis(
			generation->asts,
			target->target_ast_binder,
			target->symbol_id,
			generation->span,
			&call
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < site->explicit_argument_count; ++i) {
		uint32_t argument;
		if (function_graph_clone_value(
				generation,
				site->explicit_arguments[i],
				bindings,
				binding_count,
				NULL,
				0,
				&argument
			) != 0 || prototype_ast_app(
				generation->asts, call, argument, generation->span, &call
			) != 0) {
			return -1;
		}
	}
	*p_call = call;
	return 0;
}

static int function_graph_generate_nested_inner_branch(
	struct function_graph_generation* generation,
	const struct prototype_ast_match_case* source_case,
	const struct prototype_typed_occurrence_match_case* operation_case,
	const struct function_graph_binding_map* outer_bindings,
	uint32_t outer_binding_count,
	const uint32_t* root_argument_values,
	const uint32_t* outer_field_values,
	uint32_t outer_field_count,
	struct prototype_ast_binder* generated_case_binders,
	struct prototype_ast_match_case_input* p_case
) {
	struct function_graph_binding_map bindings[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t binding_count = outer_binding_count;
	uint32_t inner_values[FUNCTION_GRAPH_MAX_BINDINGS];
	if (!generation || !source_case || !operation_case || !outer_bindings ||
		!root_argument_values || !outer_field_values || !generated_case_binders ||
		!p_case || binding_count > FUNCTION_GRAPH_MAX_BINDINGS) {
		return -1;
	}
	for (uint32_t i = 0; i < binding_count; ++i) {
		bindings[i] = outer_bindings[i];
	}
	if (function_graph_make_runtime_case_bindings(
			generation,
			source_case,
			operation_case,
			bindings,
			&binding_count,
			generated_case_binders,
			inner_values
		) != 0) {
		fprintf(stderr, "function graph nested inner case bindings failed\n");
		return -1;
	}
	uint32_t terminal_body = source_case->body;
	const struct prototype_ast_match_case* terminal_source_case = NULL;
	const struct prototype_typed_occurrence_match_case* terminal_operation_case =
		NULL;
	uint32_t source_block_binding = PROTOTYPE_INVALID_ID;
	uint32_t source_block_value = PROTOTYPE_INVALID_ID;
	uint32_t source_terminal_match = PROTOTYPE_INVALID_ID;
	if (terminal_body < generation->asts->node_count &&
		generation->asts->nodes[terminal_body].tag ==
			PROTOTYPE_AST_COMPUTATION_BLOCK) {
		const struct prototype_ast_node* block = &generation->asts->nodes[terminal_body];
		for (uint32_t i = 0; i < block->as.block.item_count; ++i) {
			uint32_t item = generation->asts->block_items[block->as.block.first_item + i];
			if (item >= generation->asts->node_count) {
				return -1;
			}
			const struct prototype_ast_node* block_item = &generation->asts->nodes[item];
			if (block_item->tag == PROTOTYPE_AST_BLOCK_BINDING) {
				if (source_block_binding != PROTOTYPE_INVALID_ID) {
					return -1;
				}
				source_block_binding = item;
				source_block_value = block_item->as.block_binding.value;
			} else if (block_item->tag == PROTOTYPE_AST_BLOCK_EXPRESSION &&
				block_item->as.block_expression.term < generation->asts->node_count &&
				generation->asts->nodes[block_item->as.block_expression.term].tag ==
					PROTOTYPE_AST_MATCH) {
				source_terminal_match = block_item->as.block_expression.term;
			}
		}
		const struct prototype_typed_occurrence* terminal_occurrence =
			function_graph_occurrence(
				generation->metadata,
				source_terminal_match,
				PROTOTYPE_TYPED_OCCURRENCE_MATCH
			);
		if (source_block_binding == PROTOTYPE_INVALID_ID ||
			source_terminal_match == PROTOTYPE_INVALID_ID || !terminal_occurrence ||
			generation->asts->nodes[source_terminal_match].as.match.case_count != 1 ||
			terminal_occurrence->case_count != 1) {
			fprintf(stderr,
				"function graph nested terminal shape failed binding=%u match=%u occurrence=%p\n",
				source_block_binding, source_terminal_match,
				(void*)terminal_occurrence);
			return -1;
		}
		terminal_source_case = &generation->asts->cases[
			generation->asts->nodes[source_terminal_match].as.match.first_case
		];
		terminal_operation_case = &generation->metadata->typed_occurrences.cases[
			terminal_occurrence->first_case
		];
		terminal_body = terminal_source_case->body;
	}
	struct prototype_ast_binder terminal_binders[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t terminal_values[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t terminal_field_count = 0;
	uint32_t generated_block_binder = PROTOTYPE_INVALID_ID;
	int generated_block_symbol = -1;
	uint32_t generated_block_value = PROTOTYPE_INVALID_ID;
	if (terminal_source_case) {
		const struct prototype_ast_node* block_binding =
			&generation->asts->nodes[source_block_binding];
		const struct prototype_ast_node* terminal_match =
			&generation->asts->nodes[source_terminal_match];
		const struct prototype_ast_node* terminal_scrutinee =
			&generation->asts->nodes[terminal_match->as.match.scrutinee];
		const struct prototype_typed_occurrence* scrutinee_occurrence =
			function_graph_occurrence(
				generation->metadata,
				terminal_match->as.match.scrutinee,
				PROTOTYPE_TYPED_OCCURRENCE_VAR
			);
		generated_block_binder = prototype_ast_new_binder(generation->asts);
		generated_block_symbol = block_binding->as.block_binding.binder_symbol_id;
		if (terminal_scrutinee->tag != PROTOTYPE_AST_VAR || !scrutinee_occurrence ||
			generated_block_binder == PROTOTYPE_INVALID_ID ||
			prototype_ast_var(
				generation->asts,
				generated_block_binder,
				generated_block_symbol,
				generation->span,
				&generated_block_value
			) != 0 || binding_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
			fprintf(stderr,
				"function graph nested terminal binding failed tag=%d occurrence=%p bindings=%u\n",
				terminal_scrutinee->tag, (void*)scrutinee_occurrence, binding_count);
			return -1;
		}
		bindings[binding_count++] = (struct function_graph_binding_map) {
			.source_ast_binder = terminal_scrutinee->as.var.ast_binder_id,
			.source_binding = scrutinee_occurrence->binding_id,
			.target_ast_binder = generated_block_binder,
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generated_block_symbol
		};
		if (function_graph_make_runtime_case_bindings(
				generation,
				terminal_source_case,
				terminal_operation_case,
				bindings,
				&binding_count,
				terminal_binders,
				terminal_values
			) != 0) {
			fprintf(stderr, "function graph nested terminal case bindings failed\n");
			return -1;
		}
		terminal_field_count = terminal_source_case->binder_count;
	}
	struct function_graph_terminal_plan terminal_plan = {
		.body_ast = terminal_body
	};
	if (function_graph_terminal_plan_open(
			generation, terminal_body, &terminal_plan
		) != 0) {
		fprintf(stderr,
			"function graph nested terminal plan failed body=%u\n",
			terminal_body);
		return -1;
	}
	terminal_body = terminal_plan.body_ast;
	uint32_t ih_asts[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t ih_count = 0;
	if (function_graph_collect_terminal_ih(
			generation->asts,
			&terminal_plan,
			ih_asts,
			FUNCTION_GRAPH_MAX_RECURSIVE_CALLS,
			&ih_count
		) != 0) {
		fprintf(stderr, "function graph nested IH collection failed body=%u\n",
			terminal_body);
		return -1;
	}
	struct function_graph_recursive_call recursive[
		FUNCTION_GRAPH_MAX_RECURSIVE_CALLS + 1
	];
	uint32_t recursive_arguments[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS]
		[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t recursive_outputs[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t recursive_graphs[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	uint32_t packet_binders[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	int packet_symbols[FUNCTION_GRAPH_MAX_RECURSIVE_CALLS];
	for (uint32_t i = 0; i < ih_count; ++i) {
		const struct function_graph_terminal_binding* terminal_origin =
			function_graph_terminal_binding_for_ih(
				generation->asts, &terminal_plan, ih_asts[i]
			);
		const struct function_graph_recursive_site* site =
			function_graph_recursive_site_for_ih(generation, ih_asts[i]);
		if (!site || function_graph_nested_recursive_arguments(
				generation,
				site,
				bindings,
				binding_count,
				root_argument_values,
				recursive_arguments[i]
			) != 0) {
			fprintf(stderr,
				"function graph nested recursive arguments failed ih=%u site=%p\n",
				ih_asts[i], (void*)site);
			return -1;
		}
		recursive[i] = (struct function_graph_recursive_call) {
			.source_call_ast = site->call_ast,
			.source_ih_ast = site->ih_ast,
			.output_ast_binder = prototype_ast_new_binder(generation->asts),
			.graph_ast_binder = prototype_ast_new_binder(generation->asts),
			.output_symbol_id = terminal_origin ? terminal_origin->source_symbol_id :
				symbol_intern(generation->symbols, "recursiveOutput", 15),
			.graph_symbol_id = symbol_intern(
				generation->symbols, "recursiveGraph", 14
			)
		};
		packet_binders[i] = prototype_ast_new_binder(generation->asts);
		packet_symbols[i] = symbol_intern(
			generation->symbols, "recursivePacket", 15
		);
		if (recursive[i].output_ast_binder == PROTOTYPE_INVALID_ID ||
			recursive[i].graph_ast_binder == PROTOTYPE_INVALID_ID ||
			packet_binders[i] == PROTOTYPE_INVALID_ID ||
			recursive[i].output_symbol_id < 0 || recursive[i].graph_symbol_id < 0 ||
			packet_symbols[i] < 0 || prototype_ast_var(
				generation->asts,
				recursive[i].output_ast_binder,
				recursive[i].output_symbol_id,
				generation->span,
				&recursive_outputs[i]
			) != 0 || prototype_ast_var(
				generation->asts,
				recursive[i].graph_ast_binder,
				recursive[i].graph_symbol_id,
				generation->span,
				&recursive_graphs[i]
			) != 0) {
			return -1;
		}
		if (terminal_origin) {
			uint32_t source_binding;
			if (binding_count >= FUNCTION_GRAPH_MAX_BINDINGS ||
				function_graph_terminal_binding_core_id(
					generation, terminal_origin, &source_binding
				) != 0) {
				return -1;
			}
			bindings[binding_count++] = (struct function_graph_binding_map) {
				.source_ast_binder = terminal_origin->source_ast_binder,
				.source_binding = source_binding,
				.target_ast_binder = recursive[i].output_ast_binder,
				.target_value = PROTOTYPE_INVALID_ID,
				.symbol_id = recursive[i].output_symbol_id
			};
		}
	}
	struct function_graph_named_call_site helper_site;
	uint32_t helper_argument_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t helper_output = PROTOTYPE_INVALID_ID;
	uint32_t helper_graph = PROTOTYPE_INVALID_ID;
	uint32_t helper_output_binder = PROTOTYPE_INVALID_ID;
	uint32_t helper_graph_binder = PROTOTYPE_INVALID_ID;
	uint32_t helper_packet_binder = PROTOTYPE_INVALID_ID;
	int helper_output_symbol = -1;
	int helper_graph_symbol = -1;
	int helper_packet_symbol = -1;
	uint32_t replacement_count = ih_count;
	int helper_status = function_graph_named_call_site_open(
		generation, terminal_body, &helper_site
	);
	if (helper_status < 0) {
		return -1;
	}
	if (helper_status > 0) {
		if (function_graph_request_named_call(generation, &helper_site, NULL) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < helper_site.argument_count; ++i) {
			if (function_graph_clone_value(
					generation, helper_site.arguments[i], bindings, binding_count,
					recursive, ih_count, &helper_argument_values[i]
				) != 0) {
				return -1;
			}
		}
		helper_output_binder = prototype_ast_new_binder(generation->asts);
		helper_graph_binder = prototype_ast_new_binder(generation->asts);
		helper_packet_binder = prototype_ast_new_binder(generation->asts);
		helper_output_symbol = symbol_intern(
			generation->symbols, "helperOutput", 12
		);
		helper_graph_symbol = symbol_intern(
			generation->symbols, "helperGraph", 11
		);
		helper_packet_symbol = symbol_intern(
			generation->symbols, "helperPacket", 12
		);
		if (helper_output_binder == PROTOTYPE_INVALID_ID ||
			helper_graph_binder == PROTOTYPE_INVALID_ID ||
			helper_packet_binder == PROTOTYPE_INVALID_ID || helper_output_symbol < 0 ||
			helper_graph_symbol < 0 || helper_packet_symbol < 0 || prototype_ast_var(
				generation->asts, helper_output_binder, helper_output_symbol,
				generation->span, &helper_output
			) != 0 || prototype_ast_var(
				generation->asts, helper_graph_binder, helper_graph_symbol,
				generation->span, &helper_graph
			) != 0) {
			return -1;
		}
		recursive[replacement_count++] =
			(struct function_graph_recursive_call) {
				.source_call_ast = helper_site.call_ast,
				.output_ast_binder = helper_output_binder,
				.graph_ast_binder = helper_graph_binder,
				.output_symbol_id = helper_output_symbol,
				.graph_symbol_id = helper_graph_symbol
			};
	}
	uint32_t output;
	if (function_graph_clone_value(
			generation,
			terminal_body,
			bindings,
			binding_count,
			recursive,
			replacement_count,
			&output
		) != 0) {
		fprintf(stderr,
			"function graph nested output clone failed body=%u ih=%u bindings=%u\n",
			terminal_body, ih_count, binding_count);
		return -1;
	}
	uint32_t graph_fields[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t graph_field_count = 0;
	for (uint32_t i = 0; i < source_case->binder_count; ++i) {
		graph_fields[graph_field_count++] = inner_values[i];
	}
	for (uint32_t i = 0; i < outer_field_count; ++i) {
		graph_fields[graph_field_count++] = outer_field_values[i];
	}
	for (uint32_t i = 0; i < terminal_field_count; ++i) {
		graph_fields[graph_field_count++] = terminal_values[i];
	}
	for (uint32_t i = 0; i < ih_count; ++i) {
		uint32_t graph;
		if (function_graph_graph_ascription(
				generation,
				recursive_graphs[i],
				recursive_arguments[i],
				generation->argument_count,
				recursive_outputs[i],
				&graph
			) != 0 || graph_field_count + 2 > FUNCTION_GRAPH_MAX_BINDINGS) {
			fprintf(stderr,
				"function graph nested recursive graph ascription failed ih=%u fields=%u\n",
				i, graph_field_count);
			return -1;
		}
		graph_fields[graph_field_count++] = recursive_outputs[i];
		graph_fields[graph_field_count++] = graph;
	}
	if (helper_status > 0) {
		uint32_t graph;
		if (function_graph_named_graph_ascription(
				generation,
				helper_site.owner_symbol_id,
				helper_graph,
				helper_argument_values,
				helper_site.argument_count,
				helper_output,
				&graph
			) != 0 || graph_field_count + 2 > FUNCTION_GRAPH_MAX_BINDINGS) {
			return -1;
		}
		graph_fields[graph_field_count++] = helper_output;
		graph_fields[graph_field_count++] = graph;
	}
	uint32_t graph;
	uint32_t ascribed_graph;
	uint32_t result;
	uint32_t exact_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		exact_arguments[i] = root_argument_values[i];
	}
	exact_arguments[generation->graph_parameter_count] = outer_field_values[0];
	if (function_graph_graph_constructor(
			generation,
			source_case->constructor_symbol_id,
			root_argument_values,
			generation->graph_parameter_count,
			graph_fields,
			graph_field_count,
			&graph
		) != 0 || function_graph_graph_ascription(
			generation,
			graph,
			exact_arguments,
			generation->argument_count,
			output,
			&ascribed_graph
		) != 0 || function_graph_result_constructor(
			generation,
				exact_arguments,
				generation->argument_count,
				output,
				ascribed_graph,
				&result
			) != 0) {
			fprintf(stderr,
				"function graph nested result assembly failed fields=%u output=%u\n",
				graph_field_count, output);
			return -1;
		}
	uint32_t body = result;
	if (helper_status > 0) {
		uint32_t packet_var;
		uint32_t call;
		if (prototype_ast_var(
				generation->asts, helper_packet_binder, helper_packet_symbol,
				generation->span, &packet_var
			) != 0 || function_graph_named_certified_call(
				generation, &helper_site, helper_argument_values, &call
			) != 0) {
			return -1;
		}
		struct prototype_ast_binder returned_binders[2] = {
			{ helper_output_binder, helper_output_symbol },
			{ helper_graph_binder, helper_graph_symbol }
		};
		struct prototype_ast_match_case_input returned_case = {
			.constructor_symbol_id = generation->returned_symbol,
			.binders = returned_binders,
			.binder_count = 2,
			.body = body,
			.span = generation->span
		};
		uint32_t packet_match;
		uint32_t packet_type;
		uint32_t binding_item;
		uint32_t expression_item;
		if (prototype_ast_match(
				generation->asts, packet_var, &returned_case, 1,
				generation->span, &packet_match
			) != 0 || function_graph_named_result_type_for_values(
				generation, helper_site.owner_symbol_id, helper_argument_values,
				helper_site.argument_count, &packet_type
			) != 0 || prototype_ast_block_binding(
				generation->asts, helper_packet_binder, helper_packet_symbol,
				packet_type, call, generation->span, &binding_item
			) != 0 || prototype_ast_block_expression(
				generation->asts, packet_match, generation->span, &expression_item
			) != 0) {
			return -1;
		}
		uint32_t items[2] = { binding_item, expression_item };
		if (prototype_ast_computation_block(
				generation->asts, items, 2, 1,
				PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
				generation->span, &body
			) != 0) {
			return -1;
		}
	}
	for (uint32_t reverse = ih_count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		const struct function_graph_recursive_site* site =
			function_graph_recursive_site_for_ih(generation, ih_asts[i]);
		uint32_t packet_var;
		uint32_t call;
		if (!site || prototype_ast_var(
				generation->asts,
				packet_binders[i],
				packet_symbols[i],
				generation->span,
				&packet_var
			) != 0 || function_graph_nested_ih_call(
				generation, site, bindings, binding_count, &call
			) != 0) {
			return -1;
		}
		struct prototype_ast_binder returned_binders[2] = {
			{ recursive[i].output_ast_binder, recursive[i].output_symbol_id },
			{ recursive[i].graph_ast_binder, recursive[i].graph_symbol_id }
		};
		struct prototype_ast_match_case_input returned_case = {
			.constructor_symbol_id = generation->returned_symbol,
			.binders = returned_binders,
			.binder_count = 2,
			.body = body,
			.span = generation->span
		};
		uint32_t packet_match;
		uint32_t packet_type;
		uint32_t binding_item;
		uint32_t expression_item;
		if (prototype_ast_match(
				generation->asts, packet_var, &returned_case, 1,
				generation->span, &packet_match
			) != 0 || function_graph_result_type_for_values(
				generation,
				recursive_arguments[i],
				generation->argument_count,
				&packet_type
			) != 0 || prototype_ast_block_binding(
				generation->asts,
				packet_binders[i],
				packet_symbols[i],
				packet_type,
				call,
				generation->span,
				&binding_item
			) != 0 || prototype_ast_block_expression(
				generation->asts, packet_match, generation->span, &expression_item
			) != 0) {
			return -1;
		}
		uint32_t items[2] = { binding_item, expression_item };
		if (prototype_ast_computation_block(
				generation->asts, items, 2, 1,
				PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
				generation->span, &body
			) != 0) {
			return -1;
		}
	}
	if (terminal_source_case) {
		struct prototype_ast_match_case_input terminal_case = {
			.constructor_symbol_id = terminal_source_case->constructor_symbol_id,
			.binders = terminal_binders,
			.binder_count = terminal_source_case->binder_count,
			.body = body,
			.span = generation->span
		};
		uint32_t terminal_match;
		uint32_t binding_value;
		uint32_t binding_item;
		uint32_t expression_item;
		if (function_graph_clone_value(
				generation,
				source_block_value,
				bindings,
				binding_count,
				NULL,
				0,
				&binding_value
			) != 0 || prototype_ast_match(
				generation->asts,
				generated_block_value,
				&terminal_case,
				1,
				generation->span,
				&terminal_match
			) != 0 || prototype_ast_block_binding(
				generation->asts,
				generated_block_binder,
				generated_block_symbol,
				PROTOTYPE_INVALID_ID,
				binding_value,
				generation->span,
				&binding_item
			) != 0 || prototype_ast_block_expression(
				generation->asts,
				terminal_match,
				generation->span,
				&expression_item
			) != 0) {
			return -1;
		}
		uint32_t items[2] = { binding_item, expression_item };
		if (prototype_ast_computation_block(
				generation->asts, items, 2, 1,
				PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
				generation->span, &body
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

static int function_graph_generate_nested_runner(
	struct function_graph_generation* generation
) {
	uint32_t root_count = generation->argument_count - 1;
	uint32_t root_binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t root_types[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t root_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map root_maps[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t root_binding_count = 0;
	uint32_t interface_binder = PROTOTYPE_INVALID_ID;
	uint32_t interface_type = PROTOTYPE_INVALID_ID;
	uint32_t interface_value_type = PROTOTYPE_INVALID_ID;
	uint32_t callback_binder = PROTOTYPE_INVALID_ID;
	uint32_t callback_type = PROTOTYPE_INVALID_ID;
	int interface_symbol = -1;
	int callback_symbol = -1;
	for (uint32_t i = 0; i < root_count; ++i) {
		root_binders[i] = prototype_ast_new_binder(generation->asts);
		if (root_binders[i] == PROTOTYPE_INVALID_ID ||
			function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				root_maps,
				root_binding_count,
				&root_types[i]
			) != 0 || prototype_ast_var(
				generation->asts,
				root_binders[i],
				generation->source_argument_symbols[i],
				generation->span,
				&root_values[i]
			) != 0) {
			fprintf(stderr,
				"function graph nested root argument failed owner=%d argument=%u classifier=%u\n",
				generation->owner_symbol, i,
				generation->source_argument_classifiers[i]);
			return -1;
		}
		root_maps[root_binding_count] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = root_binders[i],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generation->source_argument_symbols[i]
		};
		if (i == generation->certified_argument_index) {
			uint32_t domain_type;
			interface_binder = prototype_ast_new_binder(generation->asts);
			callback_binder = prototype_ast_new_binder(generation->asts);
			interface_symbol = symbol_intern(
				generation->symbols, "ComparatorGraph", 15
			);
			callback_symbol = symbol_intern(
				generation->symbols, "certifiedComparator", 19
			);
			if (interface_binder == PROTOTYPE_INVALID_ID ||
				callback_binder == PROTOTYPE_INVALID_ID || interface_symbol < 0 ||
				callback_symbol < 0 || function_graph_projection_type(
					generation,
					generation->certified_domain_classifier,
					root_maps,
					root_binding_count + 1,
					&domain_type
				) != 0 || function_graph_binary_graph_family_type(
					generation, domain_type, &interface_type
				) != 0 || prototype_ast_var(
					generation->asts,
					interface_binder,
					interface_symbol,
					generation->span,
					&generation->runner_interface_value
				) != 0 || function_graph_value_type_expr(
					generation,
					generation->runner_interface_value,
					&interface_value_type
				) != 0 || function_graph_binary_callback_type(
					generation, domain_type, interface_value_type, &callback_type
				) != 0 || prototype_ast_var(
					generation->asts,
					callback_binder,
					callback_symbol,
					generation->span,
					&generation->runner_callback_value
				) != 0) {
				return -1;
			}
		}
		root_binding_count++;
	}
	const struct prototype_ast_node* outer_match =
		&generation->asts->nodes[generation->owner_recursive_match];
	const struct prototype_typed_occurrence* outer_occurrence =
		function_graph_occurrence(
			generation->metadata,
			generation->owner_recursive_match,
			PROTOTYPE_TYPED_OCCURRENCE_MATCH
		);
	if (!outer_occurrence || outer_match->as.match.case_count != 1 ||
		outer_occurrence->case_count != 1) {
		fprintf(stderr, "function graph nested outer match failed\n");
		return -1;
	}
	const struct prototype_ast_match_case* outer_source_case =
		&generation->asts->cases[outer_match->as.match.first_case];
	const struct prototype_typed_occurrence_match_case* outer_operation_case =
		&generation->metadata->typed_occurrences.cases[outer_occurrence->first_case];
	struct prototype_ast_binder outer_binders[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t outer_values[FUNCTION_GRAPH_MAX_BINDINGS];
	if (outer_source_case->binder_count == 0 ||
		generation->argument_count - generation->graph_parameter_count != 3 ||
		root_binding_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
		return -1;
	}
	if (function_graph_make_runtime_case_bindings(
			generation,
			outer_source_case,
			outer_operation_case,
			root_maps,
			&root_binding_count,
			outer_binders,
			outer_values
		) != 0) {
		fprintf(stderr, "function graph nested outer bindings failed\n");
		return -1;
	}
	uint32_t input_binder = prototype_ast_new_binder(generation->asts);
	uint32_t input_type;
	uint32_t input_value;
	if (input_binder == PROTOTYPE_INVALID_ID || function_graph_projection_type(
			generation,
			generation->source_argument_classifiers[generation->argument_count - 1],
			root_maps,
			root_binding_count,
			&input_type
		) != 0 || prototype_ast_var(
			generation->asts,
			input_binder,
			generation->source_argument_symbols[generation->argument_count - 1],
			generation->span,
			&input_value
		) != 0) {
		fprintf(stderr, "function graph nested input argument failed\n");
		return -1;
	}
	if (root_binding_count >= FUNCTION_GRAPH_MAX_BINDINGS) {
		return -1;
	}
	root_maps[root_binding_count++] = (struct function_graph_binding_map) {
		.source_ast_binder =
			generation->source_argument_ast_binders[generation->argument_count - 1],
		.source_binding =
			generation->source_argument_bindings[generation->argument_count - 1],
		.target_ast_binder = input_binder,
		.target_value = PROTOTYPE_INVALID_ID,
		.symbol_id = generation->source_argument_symbols[generation->argument_count - 1]
	};
	uint32_t all_argument_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	for (uint32_t i = 0; i < root_count; ++i) {
		all_argument_values[i] = root_values[i];
	}
	all_argument_values[generation->argument_count - 1] = input_value;
	const struct prototype_ast_node* input_match =
		&generation->asts->nodes[generation->owner_source_match];
	const struct prototype_typed_occurrence* input_occurrence =
		function_graph_occurrence(
			generation->metadata,
			generation->owner_source_match,
			PROTOTYPE_TYPED_OCCURRENCE_MATCH
		);
	if (!input_occurrence || input_match->as.match.case_count > 32 ||
		input_occurrence->case_count != input_match->as.match.case_count) {
		fprintf(stderr, "function graph nested input match failed\n");
		return -1;
	}
	struct prototype_ast_match_case_input input_cases[32];
	struct prototype_ast_binder input_binders[32 * FUNCTION_GRAPH_MAX_BINDINGS];
	for (uint32_t i = 0; i < input_match->as.match.case_count; ++i) {
		if (function_graph_generate_nested_inner_branch(
				generation,
				&generation->asts->cases[input_match->as.match.first_case + i],
				&generation->metadata->typed_occurrences.cases[
					input_occurrence->first_case + i
				],
				root_maps,
				root_binding_count,
				all_argument_values,
				outer_values,
				outer_source_case->binder_count,
				&input_binders[i * FUNCTION_GRAPH_MAX_BINDINGS],
				&input_cases[i]
			) != 0) {
			fprintf(stderr,
				"function graph nested runner branch failed owner=%d case=%u\n",
				generation->owner_symbol, i);
			return -1;
		}
	}
	uint32_t input_match_body;
	if (prototype_ast_match(
			generation->asts,
			input_value,
			input_cases,
			input_match->as.match.case_count,
			generation->span,
			&input_match_body
		) != 0) {
		fprintf(stderr, "function graph nested input match assembly failed\n");
		return -1;
	}
	uint32_t outer_body;
	uint32_t outer_body_type;
	uint32_t outer_result_type;
	if (prototype_ast_lambda(
			generation->asts,
			input_binder,
			generation->source_argument_symbols[generation->argument_count - 1],
			input_type,
			input_match_body,
			generation->span,
			&outer_body
		) != 0 || function_graph_result_type_for_values(
			generation,
			all_argument_values,
			generation->argument_count,
			&outer_result_type
		) != 0 || prototype_ast_type_expr_pi(
			generation->asts,
			input_binder,
			generation->source_argument_symbols[generation->argument_count - 1],
			input_type,
			outer_result_type,
			generation->span,
			&outer_body_type
		) != 0 || prototype_ast_ascription(
			generation->asts,
			outer_body,
			outer_body_type,
			generation->span,
			&outer_body
		) != 0) {
		return -1;
	}
	struct prototype_ast_match_case_input outer_case = {
		.constructor_symbol_id = outer_source_case->constructor_symbol_id,
		.binders = outer_binders,
		.binder_count = outer_source_case->binder_count,
		.body = outer_body,
		.span = generation->span
	};
	uint32_t runner;
	if (prototype_ast_match(
			generation->asts,
			root_values[root_count - 1],
			&outer_case,
			1,
			generation->span,
			&runner
		) != 0) {
		fprintf(stderr, "function graph nested outer match assembly failed\n");
		return -1;
	}
	for (uint32_t reverse = root_count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		if (i == generation->certified_argument_index &&
			(prototype_ast_lambda(
				generation->asts,
				callback_binder,
				callback_symbol,
				callback_type,
				runner,
				generation->span,
				&runner
			) != 0 || prototype_ast_lambda(
				generation->asts,
				interface_binder,
				interface_symbol,
				interface_type,
				runner,
				generation->span,
				&runner
			) != 0)) {
			return -1;
		}
		if (prototype_ast_lambda(
				generation->asts,
				root_binders[i],
				generation->source_argument_symbols[i],
				root_types[i],
				runner,
				generation->span,
				&runner
			) != 0) {
			fprintf(stderr,
				"function graph nested root lambda failed argument=%u\n", i);
			return -1;
		}
	}
	if (function_graph_add_term_assignment(
			generation,
			generation->runner_symbol,
			runner,
			&generation->runner_assignment
		) != 0) {
		fprintf(stderr, "function graph nested runner assignment failed\n");
		return -1;
	}
	/* The final input classifier depends on fields introduced by the outer
	 * Match.  Its branch-local ascription above is the exact post-synthesis
	 * contract.  Flattening that contract into a top-level Pi expectation would
	 * leak the source branch Binding instead of preserving the Match motive. */
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
	uint32_t argument_binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t argument_types[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t argument_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map argument_maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t interface_binder = PROTOTYPE_INVALID_ID;
	uint32_t interface_type = PROTOTYPE_INVALID_ID;
	uint32_t interface_value_type = PROTOTYPE_INVALID_ID;
	uint32_t callback_binder = PROTOTYPE_INVALID_ID;
	uint32_t callback_type = PROTOTYPE_INVALID_ID;
	int interface_symbol = -1;
	int callback_symbol = -1;
	uint32_t root_helper_arguments[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t root_helper_output = PROTOTYPE_INVALID_ID;
	uint32_t root_helper_output_binder = PROTOTYPE_INVALID_ID;
	uint32_t root_helper_graph_binder = PROTOTYPE_INVALID_ID;
	uint32_t root_helper_packet_binder = PROTOTYPE_INVALID_ID;
	int root_helper_output_symbol = -1;
	int root_helper_graph_symbol = -1;
	int root_helper_packet_symbol = -1;
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		argument_binders[i] = prototype_ast_new_binder(generation->asts);
		if (argument_binders[i] == PROTOTYPE_INVALID_ID ||
			function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				argument_maps,
				i,
				&argument_types[i]
			) != 0 || prototype_ast_var(
				generation->asts,
				argument_binders[i],
				generation->source_argument_symbols[i],
				generation->span,
				&argument_values[i]
			) != 0) {
			return -1;
		}
		argument_maps[i] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = argument_binders[i],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generation->source_argument_symbols[i]
		};
		if (i == generation->certified_argument_index) {
			uint32_t domain_type;
			interface_binder = prototype_ast_new_binder(generation->asts);
			callback_binder = prototype_ast_new_binder(generation->asts);
			interface_symbol = symbol_intern(
				generation->symbols, "ComparatorGraph", 15
			);
			callback_symbol = symbol_intern(
				generation->symbols, "certifiedComparator", 19
			);
			if (interface_binder == PROTOTYPE_INVALID_ID ||
				callback_binder == PROTOTYPE_INVALID_ID || interface_symbol < 0 ||
				callback_symbol < 0 || function_graph_projection_type(
					generation, generation->certified_domain_classifier,
					argument_maps, i + 1, &domain_type
				) != 0 || function_graph_binary_graph_family_type(
					generation, domain_type, &interface_type
				) != 0 || prototype_ast_var(
					generation->asts, interface_binder, interface_symbol,
					generation->span, &generation->runner_interface_value
				) != 0 || function_graph_value_type_expr(
					generation, generation->runner_interface_value, &interface_value_type
				) != 0 || function_graph_binary_callback_type(
					generation, domain_type, interface_value_type, &callback_type
				) != 0 || prototype_ast_var(
					generation->asts, callback_binder, callback_symbol,
					generation->span, &generation->runner_callback_value
				) != 0) {
				return -1;
			}
		}
	}
	if (generation->root_helper_match) {
		root_helper_output_binder = prototype_ast_new_binder(generation->asts);
		root_helper_graph_binder = prototype_ast_new_binder(generation->asts);
		root_helper_packet_binder = prototype_ast_new_binder(generation->asts);
		root_helper_output_symbol = symbol_intern(
			generation->symbols, "rootHelperOutput", 16
		);
		root_helper_graph_symbol = symbol_intern(
			generation->symbols, "rootHelperGraph", 15
		);
		root_helper_packet_symbol = symbol_intern(
			generation->symbols, "rootHelperPacket", 16
		);
		if (root_helper_output_binder == PROTOTYPE_INVALID_ID ||
			root_helper_graph_binder == PROTOTYPE_INVALID_ID ||
			root_helper_packet_binder == PROTOTYPE_INVALID_ID ||
			root_helper_output_symbol < 0 || root_helper_graph_symbol < 0 ||
			root_helper_packet_symbol < 0 || prototype_ast_var(
				generation->asts,
				root_helper_output_binder,
				root_helper_output_symbol,
				generation->span,
				&root_helper_output
			) != 0 || prototype_ast_var(
				generation->asts,
				root_helper_graph_binder,
				root_helper_graph_symbol,
				generation->span,
				&generation->runner_root_helper_graph_value
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < generation->root_helper.argument_count; ++i) {
			if (function_graph_clone_value(
					generation,
					generation->root_helper.arguments[i],
					argument_maps,
					generation->argument_count,
					NULL,
					0,
					&root_helper_arguments[i]
				) != 0) {
				return -1;
			}
		}
		generation->runner_input_binder = root_helper_output_binder;
	} else {
		generation->runner_input_binder =
			argument_binders[generation->match_argument_index];
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
				argument_maps,
				argument_values,
				&generated_binders[i * FUNCTION_GRAPH_MAX_BINDINGS],
				&generated_cases[i]
			) != 0) {
			fprintf(stderr,
				"function graph runner branch failed owner=%d case=%u certified=%u\n",
				generation->owner_symbol, i, generation->certified_argument_index);
			return -1;
		}
	}
	uint32_t match;
	uint32_t runner;
	if (prototype_ast_match(
			generation->asts,
			generation->root_helper_match ? root_helper_output :
				argument_values[generation->match_argument_index],
			generated_cases,
			source_match->as.match.case_count,
			generation->span,
			&match
		) != 0) {
		return -1;
	}
	runner = match;
	if (generation->root_helper_match) {
		uint32_t packet_var;
		uint32_t call;
		uint32_t packet_type;
		uint32_t packet_match;
		uint32_t binding_item;
		uint32_t expression_item;
		if (prototype_ast_var(
				generation->asts,
				root_helper_packet_binder,
				root_helper_packet_symbol,
				generation->span,
				&packet_var
			) != 0 || function_graph_named_certified_call(
				generation,
				&generation->root_helper,
				root_helper_arguments,
				&call
			) != 0 || function_graph_named_result_type_for_values(
				generation,
				generation->root_helper.owner_symbol_id,
				root_helper_arguments,
				generation->root_helper.argument_count,
				&packet_type
			) != 0) {
			return -1;
		}
		struct prototype_ast_binder returned_binders[2] = {
			{ root_helper_output_binder, root_helper_output_symbol },
			{ root_helper_graph_binder, root_helper_graph_symbol }
		};
		struct prototype_ast_match_case_input returned_case = {
			.constructor_symbol_id = generation->returned_symbol,
			.binders = returned_binders,
			.binder_count = 2,
			.body = runner,
			.span = generation->span
		};
		if (prototype_ast_match(
				generation->asts,
				packet_var,
				&returned_case,
				1,
				generation->span,
				&packet_match
			) != 0 || prototype_ast_block_binding(
				generation->asts,
				root_helper_packet_binder,
				root_helper_packet_symbol,
				packet_type,
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
				&runner
			) != 0) {
			return -1;
		}
	}
	for (uint32_t reverse = generation->argument_count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		if (i == generation->certified_argument_index &&
			(prototype_ast_lambda(
				generation->asts, callback_binder, callback_symbol, callback_type,
				runner, generation->span, &runner
			) != 0 || prototype_ast_lambda(
				generation->asts, interface_binder, interface_symbol, interface_type,
				runner, generation->span, &runner
			) != 0)) {
			return -1;
		}
		if (prototype_ast_lambda(
				generation->asts,
				argument_binders[i],
				generation->source_argument_symbols[i],
				argument_types[i],
				runner,
				generation->span,
				&runner
			) != 0) {
			return -1;
		}
	}
	if (function_graph_add_term_assignment(
			generation,
			generation->runner_symbol,
			runner,
			&generation->runner_assignment
		) != 0) {
		return -1;
	}
	return function_graph_add_runner_expectation(
		generation, runner, generation->runner_assignment
	);
}

/* Convert the owner-specific `$result.f` package to the common binary callback
 * package.  The output and graph witness are eliminated from the former and
 * introduced into the latter; no observation is reconstructed from the raw
 * Bool result. */
static int function_graph_generate_binary_adapter(
	struct function_graph_generation* generation
) {
	struct function_graph_binary_classifier shape;
	if (!generation) {
		return -1;
	}
	if (generation->argument_count != 2 ||
		prototype_judgement_classifier_conversion(
			generation->terms,
			generation->type_declarations,
			generation->source_argument_classifiers[0],
			generation->source_argument_classifiers[1]
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	uint32_t result_whnf;
	uint32_t result_type_id;
	uint32_t result_argument_count;
	if (function_graph_pure_whnf(
			generation, generation->view.final_result_type, &result_whnf
		) != 0 || prototype_term_type_instance_info(
			generation->terms, result_whnf, &result_type_id, NULL,
			&result_argument_count
		) != 0 || result_argument_count != 0 || result_type_id >=
			generation->type_declarations->semantic_schema.type_count) {
		return 0;
	}
	const struct prototype_type_declaration* result_type =
		&generation->type_declarations->semantic_schema.type_declarations[
			result_type_id
		];
	const char* result_name = symbol_to_string(
		generation->symbols, result_type->name_symbol_id
	);
	if (!result_name || strcmp(result_name, "Bool") != 0) {
		return 0;
	}
	shape.domain = generation->source_argument_classifiers[0];
	const char* package_name = "$certified.binary-bool";
	int package_symbol = symbol_intern(
		generation->symbols, package_name, strlen(package_name)
	);
	if (package_symbol < 0) {
		return -1;
	}
	uint32_t package_assignment = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < generation->asts->assignment_count; ++i) {
		if (generation->asts->assignments[i].name_symbol_id == package_symbol) {
			package_assignment = i;
			break;
		}
	}
	/* A binary graph may be generated without ever being selected as a
	 * higher-order callback.  Its adapter is then unnecessary. */
	if (package_assignment == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	generation->certified_package_symbol = package_symbol;
	generation->certified_package_assignment = package_assignment;

	uint32_t binders[2];
	uint32_t types[2];
	uint32_t values[2];
	struct function_graph_binding_map maps[2];
	for (uint32_t i = 0; i < 2; ++i) {
		binders[i] = prototype_ast_new_binder(generation->asts);
		if (binders[i] == PROTOTYPE_INVALID_ID || function_graph_projection_type(
				generation, generation->source_argument_classifiers[i], maps, i,
				&types[i]
			) != 0 || prototype_ast_var(
				generation->asts, binders[i], generation->source_argument_symbols[i],
				generation->span, &values[i]
			) != 0) {
			return -1;
		}
		maps[i] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = binders[i],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generation->source_argument_symbols[i]
		};
	}
	uint32_t domain_value;
	if (function_graph_term_value(
			generation, shape.domain, NULL, 0, &domain_value
		) != 0) {
		return -1;
	}
	uint32_t interface_value;
	uint32_t runner;
	if (prototype_ast_name(
			generation->asts, generation->interface_symbol, generation->span,
			&interface_value
		) != 0 || prototype_ast_name(
			generation->asts, generation->runner_symbol, generation->span, &runner
		) != 0 || prototype_ast_app(
			generation->asts, runner, values[0], generation->span, &runner
		) != 0 || prototype_ast_app(
			generation->asts, runner, values[1], generation->span, &runner
		) != 0) {
		return -1;
	}

	uint32_t packet_binder = prototype_ast_new_binder(generation->asts);
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
	int packet_symbol = symbol_intern(generation->symbols, "certified", 9);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	int graph_symbol = symbol_intern(generation->symbols, "graph", 5);
	uint32_t packet_var;
	uint32_t output_var;
	uint32_t graph_var;
	if (packet_binder == PROTOTYPE_INVALID_ID ||
		output_binder == PROTOTYPE_INVALID_ID || graph_binder == PROTOTYPE_INVALID_ID ||
		packet_symbol < 0 || output_symbol < 0 || graph_symbol < 0 ||
		prototype_ast_var(
			generation->asts, packet_binder, packet_symbol, generation->span,
			&packet_var
		) != 0 || prototype_ast_var(
			generation->asts, output_binder, output_symbol, generation->span,
			&output_var
		) != 0 || prototype_ast_var(
			generation->asts, graph_binder, graph_symbol, generation->span,
			&graph_var
		) != 0) {
		return -1;
	}
	uint32_t package = PROTOTYPE_INVALID_ID;
	if (prototype_ast_name(
			generation->asts, package_symbol, generation->span, &package
		) != 0) {
		return -1;
	}
	uint32_t package_arguments[4] = {
		domain_value, interface_value, values[0], values[1]
	};
	for (uint32_t i = 0; i < 4; ++i) {
		if (prototype_ast_app(
				generation->asts, package, package_arguments[i], generation->span,
				&package
			) != 0) {
			return -1;
		}
	}
	uint32_t result;
	if (prototype_ast_name_in_ast_namespace(
			generation->asts, package, generation->returned_symbol,
			generation->span, &result
		) != 0) {
		return -1;
	}
	uint32_t result_fields[2] = {
		output_var, graph_var
	};
	for (uint32_t i = 0; i < 2; ++i) {
		if (prototype_ast_app(
				generation->asts, result, result_fields[i], generation->span, &result
			) != 0) {
			return -1;
		}
	}
	struct prototype_ast_binder returned_binders[2] = {
		{ .ast_binder_id = output_binder, .symbol_id = output_symbol },
		{ .ast_binder_id = graph_binder, .symbol_id = graph_symbol }
	};
	struct prototype_ast_match_case_input returned_case = {
		.constructor_symbol_id = generation->returned_symbol,
		.binders = returned_binders,
		.binder_count = 2,
		.body = result,
		.span = generation->span
	};
	uint32_t packet_match;
	uint32_t packet_type;
	uint32_t binding_item;
	uint32_t expression_item;
	if (prototype_ast_match(
			generation->asts, packet_var, &returned_case, 1, generation->span,
			&packet_match
		) != 0 || function_graph_result_type_for_values(
			generation, values, 2, &packet_type
		) != 0 || prototype_ast_block_binding(
			generation->asts, packet_binder, packet_symbol, packet_type, runner,
			generation->span, &binding_item
		) != 0 || prototype_ast_block_expression(
			generation->asts, packet_match, generation->span, &expression_item
		) != 0) {
		return -1;
	}
	uint32_t items[2] = { binding_item, expression_item };
	uint32_t body;
	if (prototype_ast_computation_block(
			generation->asts, items, 2, 1, PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
			generation->span, &body
		) != 0) {
		return -1;
	}
	for (uint32_t reverse = 2; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		if (prototype_ast_lambda(
				generation->asts, binders[i], generation->source_argument_symbols[i],
				types[i], body, generation->span, &body
			) != 0) {
			return -1;
		}
	}
	return function_graph_add_term_assignment(
		generation, generation->adapter_symbol, body, &generation->adapter_assignment
	);
}

static int function_graph_generate_projection(
	struct function_graph_generation* generation
) {
	uint32_t argument_binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t argument_types[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t argument_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map argument_maps[FUNCTION_GRAPH_MAX_ARGUMENTS];
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
	if (packet_binder == PROTOTYPE_INVALID_ID ||
		output_binder == PROTOTYPE_INVALID_ID || graph_binder == PROTOTYPE_INVALID_ID ||
		packet_symbol < 0 || output_symbol < 0 ||
		graph_symbol < 0 || prototype_ast_name(
			generation->asts, generation->runner_symbol,
			generation->span, &runner
		) != 0 || prototype_ast_var(
			generation->asts, packet_binder, packet_symbol,
			generation->span, &packet_var
		) != 0 || prototype_ast_var(
			generation->asts, output_binder, output_symbol,
			generation->span, &output_var
		) != 0) {
		return -1;
	}
	call = runner;
	for (uint32_t i = 0; i < generation->argument_count; ++i) {
		argument_binders[i] = prototype_ast_new_binder(generation->asts);
		if (argument_binders[i] == PROTOTYPE_INVALID_ID ||
			function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				argument_maps,
				i,
				&argument_types[i]
			) != 0 || prototype_ast_var(
				generation->asts,
				argument_binders[i],
				generation->source_argument_symbols[i],
				generation->span,
				&argument_values[i]
			) != 0 || prototype_ast_app(
				generation->asts, call, argument_values[i], generation->span, &call
			) != 0) {
			return -1;
		}
		argument_maps[i] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = argument_binders[i],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generation->source_argument_symbols[i]
		};
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
		) != 0) {
		return -1;
	}
	projection = block;
	for (uint32_t reverse = generation->argument_count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		if (prototype_ast_lambda(
				generation->asts,
				argument_binders[i],
				generation->source_argument_symbols[i],
				argument_types[i],
				projection,
				generation->span,
				&projection
			) != 0) {
			return -1;
		}
	}
	return function_graph_add_term_assignment(
		generation,
		generation->executable_symbol,
		projection,
		&generation->executable_assignment
	);
}

static int function_graph_generate_nested_projection(
	struct function_graph_generation* generation
) {
	uint32_t root_count = generation->argument_count - 1;
	uint32_t root_binders[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t root_types[FUNCTION_GRAPH_MAX_ARGUMENTS];
	uint32_t root_values[FUNCTION_GRAPH_MAX_ARGUMENTS];
	struct function_graph_binding_map bindings[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t binding_count = 0;
	for (uint32_t i = 0; i < root_count; ++i) {
		root_binders[i] = prototype_ast_new_binder(generation->asts);
		if (root_binders[i] == PROTOTYPE_INVALID_ID ||
			function_graph_projection_type(
				generation,
				generation->source_argument_classifiers[i],
				bindings,
				binding_count,
				&root_types[i]
			) != 0 || prototype_ast_var(
				generation->asts,
				root_binders[i],
				generation->source_argument_symbols[i],
				generation->span,
				&root_values[i]
			) != 0) {
			return -1;
		}
		bindings[binding_count++] = (struct function_graph_binding_map) {
			.source_ast_binder = generation->source_argument_ast_binders[i],
			.source_binding = generation->source_argument_bindings[i],
			.target_ast_binder = root_binders[i],
			.target_value = PROTOTYPE_INVALID_ID,
			.symbol_id = generation->source_argument_symbols[i]
		};
	}
	const struct prototype_ast_node* outer_match =
		&generation->asts->nodes[generation->owner_recursive_match];
	const struct prototype_typed_occurrence* outer_occurrence =
		function_graph_occurrence(
			generation->metadata,
			generation->owner_recursive_match,
			PROTOTYPE_TYPED_OCCURRENCE_MATCH
		);
	if (!outer_occurrence || outer_match->as.match.case_count != 1 ||
		outer_occurrence->case_count != 1) {
		return -1;
	}
	const struct prototype_ast_match_case* source_case = &generation->asts->cases[
		outer_match->as.match.first_case
	];
	const struct prototype_typed_occurrence_match_case* operation_case =
		&generation->metadata->typed_occurrences.cases[outer_occurrence->first_case];
	struct prototype_ast_binder case_binders[FUNCTION_GRAPH_MAX_BINDINGS];
	uint32_t case_values[FUNCTION_GRAPH_MAX_BINDINGS];
	if (function_graph_make_runtime_case_bindings(
			generation,
			source_case,
			operation_case,
			bindings,
			&binding_count,
			case_binders,
			case_values
		) != 0) {
		return -1;
	}
	uint32_t input_binder = prototype_ast_new_binder(generation->asts);
	uint32_t input_type;
	uint32_t input_value;
	if (input_binder == PROTOTYPE_INVALID_ID || function_graph_projection_type(
			generation,
			generation->source_argument_classifiers[generation->argument_count - 1],
			bindings,
			binding_count,
			&input_type
		) != 0 || prototype_ast_var(
			generation->asts,
			input_binder,
			generation->source_argument_symbols[generation->argument_count - 1],
			generation->span,
			&input_value
		) != 0) {
		return -1;
	}
	uint32_t runner;
	uint32_t call;
	if (prototype_ast_name(
			generation->asts, generation->runner_symbol, generation->span, &runner
		) != 0) {
		return -1;
	}
	call = runner;
	for (uint32_t i = 0; i < root_count; ++i) {
		if (prototype_ast_app(
				generation->asts, call, root_values[i], generation->span, &call
			) != 0) {
			return -1;
		}
	}
	if (prototype_ast_app(
			generation->asts, call, input_value, generation->span, &call
		) != 0) {
		return -1;
	}
	uint32_t packet_binder = prototype_ast_new_binder(generation->asts);
	uint32_t output_binder = prototype_ast_new_binder(generation->asts);
	uint32_t graph_binder = prototype_ast_new_binder(generation->asts);
	int packet_symbol = symbol_intern(generation->symbols, "certified", 9);
	int output_symbol = symbol_intern(generation->symbols, "output", 6);
	int graph_symbol = symbol_intern(generation->symbols, "graph", 5);
	uint32_t packet_value;
	uint32_t output_value;
	if (packet_binder == PROTOTYPE_INVALID_ID ||
		output_binder == PROTOTYPE_INVALID_ID || graph_binder == PROTOTYPE_INVALID_ID ||
		packet_symbol < 0 || output_symbol < 0 || graph_symbol < 0 ||
		prototype_ast_var(
			generation->asts, packet_binder, packet_symbol,
			generation->span, &packet_value
		) != 0 || prototype_ast_var(
			generation->asts, output_binder, output_symbol,
			generation->span, &output_value
		) != 0) {
		return -1;
	}
	struct prototype_ast_binder returned_binders[2] = {
		{ output_binder, output_symbol },
		{ graph_binder, graph_symbol }
	};
	struct prototype_ast_match_case_input returned_case = {
		.constructor_symbol_id = generation->returned_symbol,
		.binders = returned_binders,
		.binder_count = 2,
		.body = output_value,
		.span = generation->span
	};
	uint32_t packet_match;
	uint32_t binding_item;
	uint32_t expression_item;
	uint32_t body;
	if (prototype_ast_match(
			generation->asts, packet_value, &returned_case, 1,
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
			generation->asts, packet_match, generation->span, &expression_item
		) != 0) {
		return -1;
	}
	uint32_t items[2] = { binding_item, expression_item };
	if (prototype_ast_computation_block(
			generation->asts, items, 2, 1,
			PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM,
			generation->span, &body
		) != 0 || prototype_ast_lambda(
			generation->asts,
			input_binder,
			generation->source_argument_symbols[generation->argument_count - 1],
			input_type,
			body,
			generation->span,
			&body
		) != 0) {
		return -1;
	}
	struct prototype_ast_match_case_input outer_case = {
		.constructor_symbol_id = source_case->constructor_symbol_id,
		.binders = case_binders,
		.binder_count = source_case->binder_count,
		.body = body,
		.span = generation->span
	};
	if (prototype_ast_match(
			generation->asts,
			root_values[root_count - 1],
			&outer_case,
			1,
			generation->span,
			&body
		) != 0) {
		return -1;
	}
	for (uint32_t reverse = root_count; reverse > 0; --reverse) {
		uint32_t i = reverse - 1;
		if (prototype_ast_lambda(
				generation->asts,
				root_binders[i],
				generation->source_argument_symbols[i],
				root_types[i],
				body,
				generation->span,
				&body
			) != 0) {
			return -1;
		}
	}
	return function_graph_add_term_assignment(
		generation,
		generation->executable_symbol,
		body,
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

static int function_graph_case_terminal_body(
	struct function_graph_generation* generation,
	const struct prototype_ast_match_case* source_case,
	uint32_t* p_body
) {
	if (!generation || !source_case || !p_body) {
		return -1;
	}
	uint32_t body = source_case->body;
	if (!generation->nested_recursive) {
		for (uint32_t i = 0; i < generation->joined_argument_count; ++i) {
			if (body >= generation->asts->node_count ||
				generation->asts->nodes[body].tag != PROTOTYPE_AST_LAMBDA) {
				return -1;
			}
			body = generation->asts->nodes[body].as.lambda.body;
		}
		*p_body = body;
		return 0;
	}
	if (body >= generation->asts->node_count ||
		generation->asts->nodes[body].tag != PROTOTYPE_AST_COMPUTATION_BLOCK) {
		*p_body = body;
		return 0;
	}
	const struct prototype_ast_node* block = &generation->asts->nodes[body];
	uint32_t nested_match = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < block->as.block.item_count; ++i) {
		uint32_t item = generation->asts->block_items[block->as.block.first_item + i];
		if (item < generation->asts->node_count &&
			generation->asts->nodes[item].tag == PROTOTYPE_AST_BLOCK_EXPRESSION) {
			uint32_t expression =
				generation->asts->nodes[item].as.block_expression.term;
			if (expression < generation->asts->node_count &&
				generation->asts->nodes[expression].tag == PROTOTYPE_AST_MATCH) {
				nested_match = expression;
			}
		}
	}
	if (nested_match == PROTOTYPE_INVALID_ID ||
		generation->asts->nodes[nested_match].as.match.case_count != 1) {
		return -1;
	}
	*p_body = generation->asts->cases[
		generation->asts->nodes[nested_match].as.match.first_case
	].body;
	return 0;
}

static int function_graph_generate_named_dependency(
	struct function_graph_generation* generation,
	const struct function_graph_named_call_site* site
) {
	uint32_t dependency_id;
	if (!generation || !site || function_graph_request_named_call(
			generation, site, &dependency_id
		) != 0 || dependency_id >=
			generation->metadata->function_graph_request_count) {
		return -1;
	}
	struct prototype_function_graph_request* dependency =
		&generation->metadata->function_graph_requests[dependency_id];
	if (dependency->state == PROTOTYPE_FUNCTION_GRAPH_REQUEST_GENERATED) {
		return 0;
	}
	if (dependency->state != PROTOTYPE_FUNCTION_GRAPH_REQUEST_PENDING ||
		dependency->owner_assignment_id == generation->view.assignment_id) {
		return -1;
	}
	struct function_graph_generation child;
	memset(&child, 0, sizeof(child));
	child.asts = generation->asts;
	child.terms = generation->terms;
	child.type_declarations = generation->type_declarations;
	child.judgement = generation->judgement;
	child.metadata = generation->metadata;
	child.symbols = generation->symbols;
	return function_graph_generate_one(&child, dependency_id);
}

static int function_graph_generate_terminal_dependencies(
	struct function_graph_generation* generation
) {
	if (!generation || generation->owner_source_match >=
		generation->asts->node_count) {
		return -1;
	}
	if (generation->root_helper_match && function_graph_generate_named_dependency(
			generation, &generation->root_helper
		) != 0) {
		return -1;
	}
	const struct prototype_ast_node* match =
		&generation->asts->nodes[generation->owner_source_match];
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		const struct prototype_ast_match_case* source_case =
			&generation->asts->cases[match->as.match.first_case + i];
		uint32_t terminal_body;
		if (function_graph_case_terminal_body(
				generation, source_case, &terminal_body
			) != 0) {
			return -1;
		}
		struct function_graph_named_call_site site;
		struct function_graph_certified_block certified_block;
		int certified_status = function_graph_certified_block_open(
			generation, terminal_body, &certified_block
		);
		if (certified_status < 0) {
			return -1;
		}
		int status;
		if (certified_status > 0) {
			site = certified_block.helper;
			status = 1;
		} else {
			struct function_graph_terminal_plan terminal_plan;
			if (function_graph_terminal_plan_open(
					generation, terminal_body, &terminal_plan
				) != 0) {
				return -1;
			}
			status = function_graph_named_call_site_open(
				generation, terminal_plan.body_ast, &site
			);
			if (status < 0) {
				return -1;
			}
		}
		if (status == 0) {
			continue;
		}
		if (function_graph_generate_named_dependency(generation, &site) != 0) {
			return -1;
		}
	}
	return 0;
}

static int function_graph_generate_one(
	struct function_graph_generation* generation,
	uint32_t request_id
) {
	if (!generation || request_id >=
		generation->metadata->function_graph_request_count) {
		return -1;
	}
	struct prototype_function_graph_request* request =
		&generation->metadata->function_graph_requests[request_id];
	if (request->state == PROTOTYPE_FUNCTION_GRAPH_REQUEST_GENERATED) {
		return 0;
	}
	int prepare = function_graph_prepare(generation, request->owner_assignment_id);
	if (prepare != 0) {
		if (prepare > 0) {
			fprintf(stderr,
				"function graph source shape is not structure-preserving "
				"owner=%d assignment=%u\n",
				request->owner_symbol_id, request->owner_assignment_id);
		} else {
			fprintf(stderr,
				"function graph prepare failed owner=%d assignment=%u status=%d\n",
				request->owner_symbol_id, request->owner_assignment_id, prepare);
		}
		request->state = prepare > 0 ? PROTOTYPE_FUNCTION_GRAPH_REQUEST_RESIDUAL :
			PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = prepare > 0 ?
			PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE :
			(generation->failure_reason != PROTOTYPE_FUNCTION_GRAPH_REASON_NONE ?
				generation->failure_reason :
			(generation->view.final_totality ==
				PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE ?
				PROTOTYPE_FUNCTION_GRAPH_REASON_NONTOTAL_OWNER :
				PROTOTYPE_FUNCTION_GRAPH_REASON_EFFECTFUL_OWNER));
		return -1;
	}
	if (!generation->branch_precise) {
		fprintf(stderr,
			"function graph source shape is not structure-preserving "
			"owner=%d arguments=%u recursive-sites=%u nested=%d\n",
			request->owner_symbol_id,
			generation->argument_count,
			generation->recursive_site_count,
			generation->nested_recursive
		);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_RESIDUAL;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	if (function_graph_request_certified_source_dependencies(generation) != 0) {
		fprintf(stderr,
			"function graph certified dependency collection failed owner=%d name=%s\n",
			request->owner_symbol_id,
			symbol_to_string(generation->symbols, request->owner_symbol_id));
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	if (generation->certified_argument_index != PROTOTYPE_INVALID_ID &&
		function_graph_generate_binary_package(generation) != 0) {
		fprintf(stderr,
			"function graph certified callback package failed owner=%d argument=%u\n",
			request->owner_symbol_id, generation->certified_argument_index);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	if (function_graph_generate_terminal_dependencies(generation) != 0) {
		fprintf(stderr,
			"function graph dependency closure failed owner=%d name=%s\n",
			request->owner_symbol_id,
			symbol_to_string(generation->symbols, request->owner_symbol_id));
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	int graph_status = function_graph_generate_graph_type(generation);
	if (graph_status != 0) {
		fprintf(stderr, "function graph family generation failed owner=%d name=%s nested=%d arguments=%u match=%u recursive-match=%u\n",
			request->owner_symbol_id,
			symbol_to_string(generation->symbols, request->owner_symbol_id),
			generation->nested_recursive, generation->argument_count,
			generation->owner_source_match, generation->owner_recursive_match);
		fprintf(stderr,
			"function graph AST storage nodes=%zu/%zu types=%zu/%zu families=%zu/%zu constructors=%zu/%zu fields=%zu/%zu bindings=%zu/%zu substitutions=%zu/%zu\n",
			generation->asts->node_count, generation->asts->node_capacity,
			generation->asts->type_expr_count, generation->asts->type_expr_capacity,
			generation->asts->type_def_count, generation->asts->type_def_capacity,
			generation->asts->type_constructor_count,
			generation->asts->type_constructor_capacity,
			generation->asts->type_field_expr_count,
			generation->asts->type_field_expr_capacity,
			generation->asts->family_binder_count,
			generation->asts->family_binder_capacity,
			generation->asts->accepted_binding_substitution_count,
			generation->asts->accepted_binding_substitution_capacity);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	if (function_graph_generate_interface(generation) != 0) {
		fprintf(stderr, "function graph interface generation failed owner=%d\n",
			request->owner_symbol_id);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	int result_status = function_graph_generate_result_type(generation);
	if (result_status != 0) {
		fprintf(stderr, "function graph result generation failed owner=%d name=%s\n",
			request->owner_symbol_id,
			symbol_to_string(generation->symbols, request->owner_symbol_id));
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	int runner_status = generation->nested_recursive ?
		function_graph_generate_nested_runner(generation) :
		function_graph_generate_runner(generation);
	if (runner_status != 0) {
		fprintf(stderr, "function graph runner generation failed owner=%d\n",
			request->owner_symbol_id);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	if (function_graph_generate_binary_adapter(generation) != 0) {
		fprintf(stderr, "function graph callback adapter generation failed owner=%d\n",
			request->owner_symbol_id);
		request->state = PROTOTYPE_FUNCTION_GRAPH_REQUEST_ERROR;
		request->reason = PROTOTYPE_FUNCTION_GRAPH_REASON_UNSUPPORTED_SOURCE;
		return -1;
	}
	/* A raw higher-order argument does not determine its graph interface.  Keep
	 * the accepted operational definition as the executable authority in that
	 * case; only the certified runner receives the explicit interface.  Closed
	 * first-order definitions can still be projected from certified execution. */
	int projection_status = 0;
	if (generation->certified_argument_index == PROTOTYPE_INVALID_ID) {
		projection_status = generation->nested_recursive ?
			function_graph_generate_nested_projection(generation) :
			function_graph_generate_projection(generation);
	} else {
		generation->executable_assignment = request->owner_assignment_id;
	}
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
		.imported = 0,
		.imported_interface_index = PROTOTYPE_INVALID_ID,
		.imported_owner_term_export_index = PROTOTYPE_INVALID_ID,
		.imported_graph_type_export_index = PROTOTYPE_INVALID_ID,
		.imported_result_type_export_index = PROTOTYPE_INVALID_ID,
		.imported_graph_interface_term_export_index = PROTOTYPE_INVALID_ID,
		.imported_certified_adapter_term_export_index = PROTOTYPE_INVALID_ID,
		.imported_certified_runner_term_export_index = PROTOTYPE_INVALID_ID,
		.graph_symbol_id = generation->graph_symbol,
		.result_symbol_id = generation->result_symbol,
		.returned_constructor_symbol_id = generation->returned_symbol,
		.graph_interface_symbol_id = generation->interface_symbol,
		.certified_adapter_symbol_id = generation->adapter_assignment ==
			PROTOTYPE_INVALID_ID ? -1 : generation->adapter_symbol,
		.certified_runner_symbol_id = generation->runner_symbol,
		.graph_type_id = PROTOTYPE_INVALID_ID,
		.result_type_id = PROTOTYPE_INVALID_ID,
		.graph_type_assignment_id = generation->graph_assignment,
		.result_type_assignment_id = generation->result_assignment,
		.graph_interface_assignment_id = generation->interface_assignment,
		.certified_adapter_assignment_id = generation->adapter_assignment,
		.certified_runner_assignment_id = generation->runner_assignment,
		.executable_assignment_id = generation->executable_assignment,
		.certified_argument_index = generation->certified_argument_index,
		.first_origin_group = PROTOTYPE_INVALID_ID,
		.origin_group_count = 0,
		.origin_groups_staged = 0,
		.origin_groups_frozen = 0
	};
	uint32_t association_id;
	if (prototype_compile_metadata_add_function_graph_association(
			generation->metadata, association, &association_id
		) != 0 || prototype_compile_metadata_stage_function_graph_origin_groups(
			generation->metadata,
			association_id,
			generation->origin_groups,
			generation->origin_group_count
		) != 0) {
		return -1;
	}
	if (generation->executable_assignment != request->owner_assignment_id) {
		function_graph_unpublish_owner(
			generation->metadata, generation->executable_symbol
		);
	}
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

static int prototype_accepted_definition_view_open(
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
		metadata->typed_occurrences.transaction_active) {
		return -1;
	}
	for (size_t i = 0; i < metadata->function_graph_association_count; ++i) {
		if (!metadata->function_graph_associations[i].imported) {
			return -1;
		}
	}
	uint32_t accepted_source_node_count = (uint32_t)asts->node_count;
	for (uint32_t request_id = 0;
		request_id < metadata->function_graph_request_count;
		++request_id) {
		struct function_graph_generation generation;
		memset(&generation, 0, sizeof(generation));
		generation.asts = asts;
		generation.terms = terms;
		generation.type_declarations = type_declarations;
		generation.judgement = judgement;
		generation.metadata = metadata;
		generation.symbols = symbols;
		generation.accepted_source_node_count = accepted_source_node_count;
		if (function_graph_generate_one(&generation, request_id) != 0) {
			return -1;
		}
	}
	return 0;
}

static int function_graph_validate_origin_groups(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_compile_metadata* metadata,
	uint32_t association_id
) {
	if (!terms || !type_declarations || !metadata ||
		association_id >= metadata->function_graph_association_count) {
		return -1;
	}
	const struct prototype_function_graph_association* association =
		&metadata->function_graph_associations[association_id];
	if (!association->origin_groups_staged || association->origin_groups_frozen ||
		association->graph_type_id >=
			type_declarations->semantic_schema.type_count ||
		association->first_origin_group + association->origin_group_count >
			metadata->function_graph_origin_group_count) {
		fprintf(stderr, "function graph origin interface state invalid association=%u\n",
			association_id);
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->semantic_schema.type_declarations[
			association->graph_type_id
		];
	for (uint32_t i = 0; i < association->origin_group_count; ++i) {
		const struct prototype_function_graph_origin_group* group =
			&metadata->function_graph_origin_groups[
				association->first_origin_group + i
			];
		if (group->association_id != association_id ||
			group->constructor_ordinal >= type->constructor_count ||
			type->first_constructor + group->constructor_ordinal >=
				type_declarations->semantic_schema.constructor_count) {
			fprintf(stderr,
				"function graph origin constructor invalid association=%u group=%u constructor=%u\n",
				association_id, i, group->constructor_ordinal);
			return -1;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->semantic_schema.constructor_declarations[
				type->first_constructor + group->constructor_ordinal
			];
		uint32_t field_contexts[128];
		uint32_t field_count;
		if (prototype_context_extension_path(
				&metadata->contexts,
				constructor->parameter_context,
				constructor->field_context,
				field_contexts,
				128,
				&field_count
			) != 0 || group->value_field_ordinal >= field_count ||
			((group->role_mask & PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH) != 0 &&
			 group->graph_field_ordinal >= field_count)) {
			fprintf(stderr,
				"function graph origin field invalid association=%u group=%u value=%u graph=%u fields=%u\n",
				association_id, i, group->value_field_ordinal,
				group->graph_field_ordinal, field_count);
			return -1;
		}
		int recursive = 0;
		if ((group->role_mask & PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH) != 0) {
			const struct prototype_context* graph_field = prototype_context_get(
				&metadata->contexts, field_contexts[group->graph_field_ordinal]
			);
			uint32_t graph_classifier = prototype_context_classifier_term(graph_field);
			if (!graph_field || graph_classifier == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_is_strictly_positive_recursive_field(
					terms,
					type_declarations,
					graph_classifier,
					constructor->result_classifier,
					&recursive
				) != 0) {
				fprintf(stderr,
					"function graph origin recursive classifier invalid association=%u group=%u field=%u kind=%d\n",
					association_id, i, group->graph_field_ordinal,
					graph_field ? graph_field->classifier_ref.kind : -1);
				return -1;
			}
		}
		if ((group->recursive != 0) != (recursive != 0) ||
			(((group->role_mask & PROTOTYPE_FUNCTION_GRAPH_ORIGIN_IH) != 0) !=
			 (recursive != 0))) {
			fprintf(stderr,
				"function graph origin recursive role mismatch association=%u group=%u stored=%d semantic=%d roles=%u\n",
				association_id, i, group->recursive, recursive, group->role_mask);
			return -1;
		}
	}
	return prototype_compile_metadata_freeze_function_graph_origin_groups(
		metadata, association_id
	);
}

int prototype_function_graph_finalize_associations(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_compile_metadata* metadata
) {
	if (!asts || !terms || !type_declarations || !metadata) {
		return -1;
	}
	for (size_t association_id = 0;
		association_id < metadata->function_graph_association_count;
		++association_id) {
		struct prototype_function_graph_association* association =
			&metadata->function_graph_associations[association_id];
		if (association->imported) {
			continue;
		}
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
			association->result_type_id == PROTOTYPE_INVALID_ID ||
			function_graph_validate_origin_groups(
				terms, type_declarations, metadata, (uint32_t)association_id
			) != 0) {
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
		if (association->executable_assignment_id !=
			association->owner_assignment_id) {
			function_graph_unpublish_owner(metadata, executable->name_symbol_id);
		}
	}
	return 0;
}

const char* prototype_function_graph_inspection_state_name(
	enum prototype_function_graph_inspection_state state
) {
	switch (state) {
	case PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AVAILABLE:
		return "available";
	case PROTOTYPE_FUNCTION_GRAPH_INSPECTION_ABSENT:
		return "absent";
	case PROTOTYPE_FUNCTION_GRAPH_INSPECTION_RESIDUAL:
		return "residual";
	case PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AMBIGUOUS:
		return "ambiguous";
	case PROTOTYPE_FUNCTION_GRAPH_INSPECTION_UNEXPORTED:
		return "unexported";
	case PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID:
	default:
		return "invalid";
	}
}

enum prototype_function_graph_inspection_state
prototype_function_graph_request_inspection(
	const struct prototype_ast_db* asts,
	struct prototype_compile_metadata* metadata,
	int owner_symbol_id
) {
	if (!asts || !metadata || owner_symbol_id < 0) {
		return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
	}
	uint32_t assignment_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < asts->assignment_count; ++i) {
		if (asts->assignments[i].name_symbol_id != owner_symbol_id) {
			continue;
		}
		if (assignment_id != PROTOTYPE_INVALID_ID) {
			return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AMBIGUOUS;
		}
		assignment_id = i;
	}
	if (assignment_id == PROTOTYPE_INVALID_ID) {
		return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_ABSENT;
	}
	const struct prototype_ast_term_assignment_def* assignment =
		&asts->assignments[assignment_id];
	uint32_t request_id;
	if (prototype_compile_metadata_request_function_graph(
			metadata,
			owner_symbol_id,
			assignment_id,
			assignment->source_entry_id,
			PROTOTYPE_FUNCTION_GRAPH_REQUEST_FAMILY |
				PROTOTYPE_FUNCTION_GRAPH_REQUEST_CERTIFIED_EXECUTION |
				PROTOTYPE_FUNCTION_GRAPH_REQUEST_INSPECTION,
			assignment->ast,
			&request_id
		) != 0) {
		return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
	}
	return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AVAILABLE;
}

static const char* function_graph_role_text(uint32_t roles) {
	switch (roles) {
	case PROTOTYPE_FUNCTION_GRAPH_ORIGIN_VALUE:
		return "value";
	case PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH:
		return "graph";
	case PROTOTYPE_FUNCTION_GRAPH_ORIGIN_VALUE |
		PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH:
		return "value,graph";
	case PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH |
		PROTOTYPE_FUNCTION_GRAPH_ORIGIN_IH:
		return "graph,ih";
	case PROTOTYPE_FUNCTION_GRAPH_ORIGIN_VALUE |
		PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH |
		PROTOTYPE_FUNCTION_GRAPH_ORIGIN_IH:
		return "value,graph,ih";
	default:
		return "invalid";
	}
}

static void function_graph_print_classifier(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const char* label,
	uint32_t classifier
) {
	fprintf(output, "  %s=", label);
	prototype_term_print_debug(
		output, symbols, intrinsic_environment, type_declarations, terms, classifier
	);
	fputc('\n', output);
}

enum prototype_function_graph_inspection_state prototype_function_graph_inspect(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_compile_metadata* metadata,
	int owner_symbol_id
) {
	if (!output || !symbols || !intrinsic_environment || !terms ||
		!type_declarations || !metadata || owner_symbol_id < 0) {
		return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
	}
	uint32_t association_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < metadata->function_graph_association_count; ++i) {
		if (metadata->function_graph_associations[i].owner_symbol_id !=
			owner_symbol_id) {
			continue;
		}
		if (association_id != PROTOTYPE_INVALID_ID) {
			return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AMBIGUOUS;
		}
		association_id = i;
	}
	if (association_id == PROTOTYPE_INVALID_ID) {
		for (uint32_t i = 0; i < metadata->function_graph_request_count; ++i) {
			const struct prototype_function_graph_request* request =
				&metadata->function_graph_requests[i];
			if (request->owner_symbol_id == owner_symbol_id &&
				request->state == PROTOTYPE_FUNCTION_GRAPH_REQUEST_RESIDUAL) {
				return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_RESIDUAL;
			}
		}
		return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_ABSENT;
	}
	const struct prototype_function_graph_association* association =
		&metadata->function_graph_associations[association_id];
	if (!association->origin_groups_frozen) {
		return association->imported ?
			PROTOTYPE_FUNCTION_GRAPH_INSPECTION_UNEXPORTED :
			PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
	}
	if (association->graph_type_id >=
		type_declarations->semantic_schema.type_count ||
		association->first_origin_group + association->origin_group_count >
			metadata->function_graph_origin_group_count) {
		return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->semantic_schema.type_declarations[
			association->graph_type_id
		];
	const char* owner_name = symbol_to_string(symbols, owner_symbol_id);
	fprintf(
		output,
		"function-graph owner=%s association=%u source=%s graph-type=%u result-type=%u "
		"certified-argument=",
		owner_name ? owner_name : "<unknown>", association_id,
		association->imported ? "imported" : "local",
		association->graph_type_id, association->result_type_id
	);
	if (association->certified_argument_index == PROTOTYPE_INVALID_ID) {
		fprintf(output, "none\n");
	} else {
		fprintf(output, "%u\n", association->certified_argument_index);
	}
	function_graph_print_classifier(
		output, symbols, intrinsic_environment, type_declarations, terms,
		"graph-formation-classifier", type->formation_classifier
	);
	for (uint32_t constructor_ordinal = 0;
		constructor_ordinal < type->constructor_count;
		++constructor_ordinal) {
		uint32_t constructor_id = type->first_constructor + constructor_ordinal;
		if (constructor_id >=
			type_declarations->semantic_schema.constructor_count) {
			return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->semantic_schema.constructor_declarations[
				constructor_id
			];
		uint32_t field_contexts[128];
		uint32_t field_count;
		if (prototype_context_extension_path(
				&metadata->contexts,
				constructor->parameter_context,
				constructor->field_context,
				field_contexts,
				128,
				&field_count
			) != 0) {
			return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
		}
		const char* constructor_name = symbol_to_string(
			symbols, constructor->name_symbol_id
		);
		fprintf(
			output,
			"constructor ordinal=%u name=%s fields=%u\n",
			constructor_ordinal,
			constructor_name ? constructor_name : "<unknown>",
			field_count
		);
		function_graph_print_classifier(
			output, symbols, intrinsic_environment, type_declarations, terms,
			"result-classifier", constructor->result_classifier
		);
		for (uint32_t i = 0; i < association->origin_group_count; ++i) {
			const struct prototype_function_graph_origin_group* group =
				&metadata->function_graph_origin_groups[
					association->first_origin_group + i
				];
			if (group->association_id != association_id ||
				group->constructor_ordinal != constructor_ordinal ||
				group->value_field_ordinal >= field_count ||
				((group->role_mask & PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH) != 0 &&
				 group->graph_field_ordinal >= field_count)) {
				continue;
			}
			const char* display_name = symbol_to_string(
				symbols, group->display_symbol_id
			);
			fprintf(
				output,
				"origin name=%s roles=%s value-field=%u graph-field=",
				display_name ? display_name : "<unknown>",
				function_graph_role_text(group->role_mask),
				group->value_field_ordinal
			);
			if ((group->role_mask & PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH) == 0) {
				fprintf(output, "none recursive=no\n");
			} else {
				fprintf(
					output, "%u recursive=%s\n", group->graph_field_ordinal,
					group->recursive ? "yes" : "no"
				);
			}
			const struct prototype_context* value_field = prototype_context_get(
				&metadata->contexts, field_contexts[group->value_field_ordinal]
			);
			uint32_t value_classifier =
				prototype_context_classifier_term(value_field);
			if (!value_field || value_classifier == PROTOTYPE_INVALID_ID) {
				return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
			}
			function_graph_print_classifier(
				output, symbols, intrinsic_environment, type_declarations, terms,
				"value-classifier", value_classifier
			);
			if ((group->role_mask & PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH) != 0) {
				const struct prototype_context* graph_field = prototype_context_get(
					&metadata->contexts, field_contexts[group->graph_field_ordinal]
				);
				uint32_t graph_classifier =
					prototype_context_classifier_term(graph_field);
				if (!graph_field || graph_classifier == PROTOTYPE_INVALID_ID) {
					return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_INVALID;
				}
				function_graph_print_classifier(
					output, symbols, intrinsic_environment, type_declarations, terms,
					"graph-classifier", graph_classifier
				);
			}
		}
	}
	return PROTOTYPE_FUNCTION_GRAPH_INSPECTION_AVAILABLE;
}
