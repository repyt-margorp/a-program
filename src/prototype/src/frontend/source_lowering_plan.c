#include "a_program/frontend/source_lowering_plan.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_LOWERING_INVALID_ID UINT32_MAX
#define SOURCE_LOWERING_HASH_OFFSET UINT64_C(1469598103934665603)
#define SOURCE_LOWERING_HASH_PRIME UINT64_C(1099511628211)

struct source_lowering_plan_builder {
	const struct prototype_ast_db* asts;
	struct prototype_source_lowering_slot* slots;
	size_t slot_capacity;
	struct prototype_source_lowering_edge* edges;
	size_t edge_capacity;
	struct prototype_source_lowering_binder* binders;
	size_t binder_capacity;
	size_t edge_count;
	size_t binder_count;
	int write;
};

static int source_count_add(size_t* count, size_t amount) {
	if (!count || amount > SIZE_MAX - *count) return -1;
	*count += amount;
	return 0;
}

static int source_slot_count(
	const struct prototype_ast_db* asts,
	size_t* p_count
) {
	if (!asts || !p_count || asts->node_count > UINT32_MAX ||
		asts->type_expr_count > UINT32_MAX ||
		asts->type_expr_count > UINT32_MAX - asts->node_count) {
		return -1;
	}
	*p_count = asts->node_count + asts->type_expr_count;
	return 0;
}

static int term_slot(
	const struct prototype_ast_db* asts,
	uint32_t ast_node_id,
	uint32_t* p_slot
) {
	if (!asts || !p_slot || ast_node_id >= asts->node_count) return -1;
	*p_slot = ast_node_id;
	return 0;
}

static int type_expr_slot(
	const struct prototype_ast_db* asts,
	uint32_t ast_type_expr_id,
	uint32_t* p_slot
) {
	if (!asts || !p_slot || ast_type_expr_id >= asts->type_expr_count ||
		asts->node_count > UINT32_MAX - ast_type_expr_id) {
		return -1;
	}
	*p_slot = (uint32_t)asts->node_count + ast_type_expr_id;
	return 0;
}

static int builder_add_edge(
	struct source_lowering_plan_builder* builder,
	uint32_t owner_slot,
	uint32_t child_slot,
	int role,
	uint32_t ordinal
) {
	if (!builder || owner_slot >= builder->asts->node_count +
		builder->asts->type_expr_count || child_slot >= builder->asts->node_count +
		builder->asts->type_expr_count || builder->edge_count == UINT32_MAX) {
		return -1;
	}
	if (builder->write) {
		if (builder->edge_count >= builder->edge_capacity) return -1;
		builder->edges[builder->edge_count] =
			(struct prototype_source_lowering_edge) {
				.owner_slot = owner_slot,
				.child_slot = child_slot,
				.ordinal = ordinal,
				.role = role,
				.scope_recipe = SOURCE_LOWERING_INVALID_ID
			};
	}
	return source_count_add(&builder->edge_count, 1);
}

static int builder_add_term_edge(
	struct source_lowering_plan_builder* builder,
	uint32_t owner_slot,
	uint32_t ast_node_id,
	int role,
	uint32_t ordinal
) {
	uint32_t child_slot;
	return term_slot(builder->asts, ast_node_id, &child_slot) == 0 ?
		builder_add_edge(builder, owner_slot, child_slot, role, ordinal) : -1;
}

static int builder_add_type_edge(
	struct source_lowering_plan_builder* builder,
	uint32_t owner_slot,
	uint32_t ast_type_expr_id,
	int role,
	uint32_t ordinal
) {
	uint32_t child_slot;
	return type_expr_slot(builder->asts, ast_type_expr_id, &child_slot) == 0 ?
		builder_add_edge(builder, owner_slot, child_slot, role, ordinal) : -1;
}

static int builder_add_binder(
	struct source_lowering_plan_builder* builder,
	uint32_t owner_slot,
	uint32_t source_binder_id,
	int symbol_id,
	uint32_t annotation_slot,
	int role,
	uint32_t ordinal
) {
	int anonymous_constructor_field =
		role == PROTOTYPE_SOURCE_BINDER_CONSTRUCTOR_FIELD &&
		source_binder_id == SOURCE_LOWERING_INVALID_ID && symbol_id < 0;
	int generated_effect_row =
		role == PROTOTYPE_SOURCE_BINDER_COMPUTATION_EFFECT_ROW &&
		source_binder_id == SOURCE_LOWERING_INVALID_ID && symbol_id < 0;
	if (!builder ||
		(source_binder_id == SOURCE_LOWERING_INVALID_ID &&
		 !anonymous_constructor_field && !generated_effect_row) ||
		owner_slot >= builder->asts->node_count + builder->asts->type_expr_count ||
		(annotation_slot != SOURCE_LOWERING_INVALID_ID && annotation_slot >=
			builder->asts->node_count + builder->asts->type_expr_count) ||
		builder->binder_count == UINT32_MAX) {
		return -1;
	}
	if (builder->write) {
		if (builder->binder_count >= builder->binder_capacity) return -1;
		builder->binders[builder->binder_count] =
			(struct prototype_source_lowering_binder) {
				.owner_slot = owner_slot,
				.source_binder_id = source_binder_id,
				.symbol_id = symbol_id,
				.annotation_slot = annotation_slot,
				.ordinal = ordinal,
				.role = role
			};
	}
	return source_count_add(&builder->binder_count, 1);
}

static int builder_add_typed_binder(
	struct source_lowering_plan_builder* builder,
	uint32_t owner_slot,
	uint32_t source_binder_id,
	int symbol_id,
	uint32_t ast_type_expr_id,
	int role,
	uint32_t ordinal
) {
	uint32_t annotation_slot;
	if (type_expr_slot(builder->asts, ast_type_expr_id, &annotation_slot) != 0 ||
		builder_add_type_edge(
			builder, owner_slot, ast_type_expr_id,
			role == PROTOTYPE_SOURCE_BINDER_CONSTRUCTOR_FIELD ?
				PROTOTYPE_SOURCE_EDGE_TYPE_CONSTRUCTOR_FIELD :
				PROTOTYPE_SOURCE_EDGE_BINDER_TYPE,
			ordinal
		) != 0) {
		return -1;
	}
	return builder_add_binder(
		builder, owner_slot, source_binder_id, symbol_id, annotation_slot,
		role, ordinal
	);
}

static int process_type_definition(
	struct source_lowering_plan_builder* builder,
	uint32_t owner_slot,
	uint32_t ast_type_def_id
) {
	const struct prototype_ast_db* asts = builder->asts;
	if (ast_type_def_id >= asts->type_def_count) return -1;
	const struct prototype_ast_type_def* type = &asts->type_defs[ast_type_def_id];
	size_t family_count = (size_t)type->parameter_count + type->index_count;
	if (family_count > UINT32_MAX || type->first_family_binder >
		asts->family_binder_count || family_count >
		asts->family_binder_count - type->first_family_binder ||
		type->first_constructor > asts->type_constructor_count ||
		type->constructor_count > asts->type_constructor_count -
			type->first_constructor) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)family_count; ++i) {
		const struct prototype_ast_family_binder* binder =
			&asts->family_binders[type->first_family_binder + i];
		int role = i < type->parameter_count ?
			PROTOTYPE_SOURCE_BINDER_FAMILY_PARAMETER :
			PROTOTYPE_SOURCE_BINDER_FAMILY_INDEX;
		uint32_t annotation_slot;
		if (type_expr_slot(asts, binder->type_expr, &annotation_slot) != 0 ||
			builder_add_type_edge(
				builder, owner_slot, binder->type_expr,
				PROTOTYPE_SOURCE_EDGE_TYPE_FAMILY_BINDER, i
			) != 0 || builder_add_binder(
				builder, owner_slot, binder->ast_binder_id,
				binder->name_symbol_id, annotation_slot, role, i
			) != 0) {
			return -1;
		}
	}
	uint32_t field_ordinal = 0;
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_ast_type_constructor* constructor =
			&asts->type_constructors[type->first_constructor + i];
		if (constructor->first_field_type > asts->type_field_expr_count ||
			constructor->field_count > asts->type_field_expr_count -
				constructor->first_field_type) {
			return -1;
		}
		for (uint32_t j = 0; j < constructor->field_count; ++j) {
			uint32_t field_index = constructor->first_field_type + j;
			uint32_t field_expr = asts->type_field_exprs[field_index];
			if (builder_add_typed_binder(
					builder, owner_slot, asts->type_field_binder_ids[field_index],
					asts->type_field_name_symbol_ids[field_index], field_expr,
					PROTOTYPE_SOURCE_BINDER_CONSTRUCTOR_FIELD, field_ordinal
				) != 0) {
				return -1;
			}
			field_ordinal++;
		}
		if (builder_add_type_edge(
				builder, owner_slot, constructor->result_type,
				PROTOTYPE_SOURCE_EDGE_TYPE_CONSTRUCTOR_RESULT, i
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int type_expr_reaches(
	const struct prototype_ast_db* asts,
	uint32_t current,
	uint32_t target,
	unsigned char* visited
) {
	if (!asts || !visited || current >= asts->type_expr_count ||
		target >= asts->type_expr_count) {
		return -1;
	}
	if (current == target) return 1;
	if (visited[current]) return 0;
	visited[current] = 1;
	const struct prototype_ast_type_expr* expr = &asts->type_exprs[current];
	uint32_t first = SOURCE_LOWERING_INVALID_ID;
	uint32_t second = SOURCE_LOWERING_INVALID_ID;
	switch (expr->tag) {
		case PROTOTYPE_AST_TYPE_EXPR_APP:
			first = expr->as.app.function;
			second = expr->as.app.argument;
			break;
		case PROTOTYPE_AST_TYPE_EXPR_ARROW:
			first = expr->as.arrow.domain;
			second = expr->as.arrow.codomain;
			break;
		case PROTOTYPE_AST_TYPE_EXPR_PI:
			first = expr->as.pi.domain;
			second = expr->as.pi.codomain;
			break;
		case PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE:
			first = expr->as.computation_reference.result;
			break;
		default:
			return 0;
	}
	int found = type_expr_reaches(asts, first, target, visited);
	if (found != 0 || second == SOURCE_LOWERING_INVALID_ID) return found;
	return type_expr_reaches(asts, second, target, visited);
}

static int type_definition_contains_type_expr(
	const struct prototype_ast_db* asts,
	uint32_t type_def_id,
	uint32_t target,
	unsigned char* visited
) {
	if (!asts || !visited || type_def_id >= asts->type_def_count) return -1;
	const struct prototype_ast_type_def* type = &asts->type_defs[type_def_id];
	size_t family_count = (size_t)type->parameter_count + type->index_count;
	if (type->first_family_binder > asts->family_binder_count ||
		family_count > asts->family_binder_count - type->first_family_binder ||
		type->first_constructor > asts->type_constructor_count ||
		type->constructor_count > asts->type_constructor_count -
			type->first_constructor) {
		return -1;
	}
	for (uint32_t i = 0; i < family_count; ++i) {
		memset(visited, 0, asts->type_expr_count);
		int found = type_expr_reaches(
			asts, asts->family_binders[type->first_family_binder + i].type_expr,
			target, visited
		);
		if (found != 0) return found;
	}
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_ast_type_constructor* constructor =
			&asts->type_constructors[type->first_constructor + i];
		if (constructor->first_field_type > asts->type_field_expr_count ||
			constructor->field_count > asts->type_field_expr_count -
				constructor->first_field_type) {
			return -1;
		}
		for (uint32_t j = 0; j < constructor->field_count; ++j) {
			memset(visited, 0, asts->type_expr_count);
			int found = type_expr_reaches(
				asts,
				asts->type_field_exprs[constructor->first_field_type + j],
				target, visited
			);
			if (found != 0) return found;
		}
		memset(visited, 0, asts->type_expr_count);
		int found = type_expr_reaches(
			asts, constructor->result_type, target, visited
		);
		if (found != 0) return found;
	}
	return 0;
}

static int self_type_expr_owner(
	const struct prototype_ast_db* asts,
	uint32_t type_expr_id,
	uint32_t* p_owner
) {
	if (!asts || !p_owner || type_expr_id >= asts->type_expr_count ||
		asts->type_exprs[type_expr_id].tag != PROTOTYPE_AST_TYPE_EXPR_SELF) {
		return -1;
	}
	unsigned char* visited = asts->type_expr_count == 0 ? NULL :
		malloc(asts->type_expr_count);
	if (!visited) return -1;
	uint32_t owner = SOURCE_LOWERING_INVALID_ID;
	for (uint32_t i = 0; i < asts->type_def_count; ++i) {
		int found = type_definition_contains_type_expr(
			asts, i, type_expr_id, visited
		);
		if (found < 0 || (found > 0 && owner != SOURCE_LOWERING_INVALID_ID)) {
			free(visited);
			return -1;
		}
		if (found > 0) owner = i;
	}
	free(visited);
	if (owner == SOURCE_LOWERING_INVALID_ID) return -1;
	*p_owner = owner;
	return 0;
}

static int process_term_slot(
	struct source_lowering_plan_builder* builder,
	uint32_t ast_node_id
) {
	const struct prototype_ast_db* asts = builder->asts;
	const struct prototype_ast_node* node = &asts->nodes[ast_node_id];
	uint32_t owner_slot = ast_node_id;
	size_t first_edge = builder->edge_count;
	size_t first_binder = builder->binder_count;
	int status = 0;
	switch (node->tag) {
		case PROTOTYPE_AST_VAR:
		case PROTOTYPE_AST_NAME:
		case PROTOTYPE_AST_TEXT_LITERAL:
		case PROTOTYPE_AST_INT_LITERAL:
		case PROTOTYPE_AST_SYSTEM_NAME:
		case PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE:
		case PROTOTYPE_AST_FUNCTION_GRAPH_ROLE_REFERENCE:
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
			break;
		case PROTOTYPE_AST_NAME_IN_NAMESPACE:
			break;
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.name_in_ast_namespace.namespace_ast,
				PROTOTYPE_SOURCE_EDGE_DEFINITION_BLOCK, 0
			);
			break;
		case PROTOTYPE_AST_APP:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.app.function,
				PROTOTYPE_SOURCE_EDGE_FUNCTION, 0
			) != 0 || builder_add_term_edge(
				builder, owner_slot, node->as.app.argument,
				PROTOTYPE_SOURCE_EDGE_ARGUMENT, 0
			) != 0 ? -1 : 0;
			break;
		case PROTOTYPE_AST_LAMBDA: {
			uint32_t annotation_slot;
			status = type_expr_slot(
				asts, node->as.lambda.binder_type, &annotation_slot
			) != 0 || builder_add_type_edge(
				builder, owner_slot, node->as.lambda.binder_type,
				PROTOTYPE_SOURCE_EDGE_BINDER_TYPE, 0
			) != 0 || builder_add_term_edge(
				builder, owner_slot, node->as.lambda.body,
				PROTOTYPE_SOURCE_EDGE_BODY, 0
			) != 0 || builder_add_binder(
				builder, owner_slot, node->as.lambda.ast_binder_id,
				node->as.lambda.binder_symbol_id, annotation_slot,
				PROTOTYPE_SOURCE_BINDER_LAMBDA, 0
			) != 0 ? -1 : 0;
			break;
		}
		case PROTOTYPE_AST_MATCH:
			if (node->as.match.first_case > asts->case_count ||
				node->as.match.case_count > asts->case_count -
					node->as.match.first_case || builder_add_term_edge(
					builder, owner_slot, node->as.match.scrutinee,
					PROTOTYPE_SOURCE_EDGE_SCRUTINEE, 0
				) != 0) {
				status = -1;
				break;
			}
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				const struct prototype_ast_match_case* match_case =
					&asts->cases[node->as.match.first_case + i];
				if (match_case->first_binder > asts->case_binder_count ||
					match_case->binder_count > asts->case_binder_count -
						match_case->first_binder ||
					match_case->first_selector > asts->match_selector_count ||
					match_case->selector_count > asts->match_selector_count -
						match_case->first_selector || builder_add_term_edge(
						builder, owner_slot, match_case->body,
						PROTOTYPE_SOURCE_EDGE_MATCH_CASE_BODY, i
					) != 0) {
					status = -1;
					break;
				}
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					const struct prototype_ast_binder* binder =
						&asts->case_binders[match_case->first_binder + j];
					if (builder_add_binder(
							builder, owner_slot, binder->ast_binder_id,
							binder->symbol_id, SOURCE_LOWERING_INVALID_ID,
							PROTOTYPE_SOURCE_BINDER_MATCH_FIELD,
							(uint32_t)builder->binder_count - (uint32_t)first_binder
						) != 0) {
						status = -1;
						break;
					}
				}
				if (status != 0) break;
				for (uint32_t j = 0; j < match_case->selector_count; ++j) {
					const struct prototype_ast_match_selector* selector =
						&asts->match_selectors[match_case->first_selector + j];
					uint32_t value_ordinal =
						(uint32_t)builder->binder_count - (uint32_t)first_binder;
					if (builder_add_binder(
							builder, owner_slot, selector->value_ast_binder_id,
							selector->local_symbol_id, SOURCE_LOWERING_INVALID_ID,
							PROTOTYPE_SOURCE_BINDER_MATCH_SELECTOR_VALUE,
							value_ordinal
						) != 0 || builder_add_binder(
							builder, owner_slot, selector->graph_ast_binder_id,
							selector->local_symbol_id, SOURCE_LOWERING_INVALID_ID,
							PROTOTYPE_SOURCE_BINDER_MATCH_SELECTOR_GRAPH,
							value_ordinal + 1
						) != 0) {
						status = -1;
						break;
					}
				}
				if (status != 0) break;
			}
			break;
		case PROTOTYPE_AST_TYPE_LITERAL:
		case PROTOTYPE_AST_TYPE_FORMATION:
			status = process_type_definition(
				builder, owner_slot, node->as.type_formation.ast_type_def_id
			);
			break;
		case PROTOTYPE_AST_ASCRIPTION:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.ascription.term,
				PROTOTYPE_SOURCE_EDGE_ASCRIBED_TERM, 0
			) != 0 || builder_add_type_edge(
				builder, owner_slot, node->as.ascription.type_expr,
				PROTOTYPE_SOURCE_EDGE_ASCRIBED_TYPE, 0
			) != 0 ? -1 : 0;
			break;
		case PROTOTYPE_AST_QUOTE:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.unary.term,
				PROTOTYPE_SOURCE_EDGE_UNARY_TERM, 0
			);
			break;
		case PROTOTYPE_AST_DEFINITION_BLOCK:
			if (node->as.definition_block.first_assignment >
					asts->definition_item_count ||
				node->as.definition_block.assignment_count >
					asts->definition_item_count -
						node->as.definition_block.first_assignment) {
				status = -1;
				break;
			}
			for (uint32_t i = 0;
				i < node->as.definition_block.assignment_count; ++i) {
				uint32_t assignment_id = asts->definition_items[
					node->as.definition_block.first_assignment + i
				];
				if (assignment_id >= asts->assignment_count ||
					builder_add_term_edge(
						builder, owner_slot, asts->assignments[assignment_id].ast,
						PROTOTYPE_SOURCE_EDGE_DEFINITION_ITEM, i
					) != 0) {
					status = -1;
					break;
				}
			}
			break;
		case PROTOTYPE_AST_DEFINITION_SELECT:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.definition_select.definition_block,
				PROTOTYPE_SOURCE_EDGE_DEFINITION_BLOCK, 0
			);
			break;
		case PROTOTYPE_AST_COMPUTATION_BLOCK:
			if (node->as.block.first_item > asts->block_item_count ||
				node->as.block.item_count > asts->block_item_count -
					node->as.block.first_item) {
				status = -1;
				break;
			}
			for (uint32_t i = 0; i < node->as.block.item_count; ++i) {
				if (builder_add_term_edge(
						builder, owner_slot,
						asts->block_items[node->as.block.first_item + i],
						PROTOTYPE_SOURCE_EDGE_BLOCK_ITEM, i
					) != 0) {
					status = -1;
					break;
				}
			}
			break;
			case PROTOTYPE_AST_BLOCK_BINDING: {
				uint32_t annotation_slot = SOURCE_LOWERING_INVALID_ID;
				if (node->as.block_binding.binder_type !=
						SOURCE_LOWERING_INVALID_ID && (
						type_expr_slot(
							asts, node->as.block_binding.binder_type,
							&annotation_slot
						) != 0 || builder_add_type_edge(
							builder, owner_slot,
							node->as.block_binding.binder_type,
							PROTOTYPE_SOURCE_EDGE_BINDER_TYPE, 0
						) != 0
					)) {
					status = -1;
					break;
				}
				status = builder_add_term_edge(
					builder, owner_slot, node->as.block_binding.value,
					PROTOTYPE_SOURCE_EDGE_BLOCK_VALUE, 0
				) != 0 || builder_add_binder(
				builder, owner_slot, node->as.block_binding.ast_binder_id,
				node->as.block_binding.binder_symbol_id, annotation_slot,
				PROTOTYPE_SOURCE_BINDER_BLOCK_RESULT, 0
			) != 0 ? -1 : 0;
			break;
		}
		case PROTOTYPE_AST_BLOCK_EXPRESSION:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.block_expression.term,
				PROTOTYPE_SOURCE_EDGE_BLOCK_VALUE, 0
			);
			break;
		case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.block_lambda_exit.value,
				PROTOTYPE_SOURCE_EDGE_BLOCK_VALUE, 0
			);
			break;
		case PROTOTYPE_AST_COMPUTATION_FOLD:
			if (node->as.computation_fold.first_clause >
					asts->computation_fold_clause_count ||
				node->as.computation_fold.clause_count >
					asts->computation_fold_clause_count -
						node->as.computation_fold.first_clause ||
				builder_add_term_edge(
					builder, owner_slot, node->as.computation_fold.computation,
					PROTOTYPE_SOURCE_EDGE_FOLD_COMPUTATION, 0
				) != 0 || builder_add_term_edge(
					builder, owner_slot, node->as.computation_fold.return_body,
					PROTOTYPE_SOURCE_EDGE_FOLD_RETURN_BODY, 0
				) != 0 || builder_add_binder(
					builder, owner_slot, node->as.computation_fold.return_binder_id,
					node->as.computation_fold.return_symbol_id,
					SOURCE_LOWERING_INVALID_ID,
					PROTOTYPE_SOURCE_BINDER_FOLD_RETURN, 0
				) != 0) {
				status = -1;
				break;
			}
			for (uint32_t i = 0;
				i < node->as.computation_fold.clause_count; ++i) {
				const struct prototype_ast_computation_fold_clause* clause =
					&asts->computation_fold_clauses[
						node->as.computation_fold.first_clause + i
					];
				if (builder_add_term_edge(
						builder, owner_slot, clause->operation,
						PROTOTYPE_SOURCE_EDGE_FOLD_OPERATION, i
					) != 0 || builder_add_term_edge(
						builder, owner_slot, clause->body,
						PROTOTYPE_SOURCE_EDGE_FOLD_CLAUSE_BODY, i
					) != 0 || builder_add_binder(
						builder, owner_slot, clause->operation_argument_binder_id,
						clause->operation_argument_symbol_id,
						SOURCE_LOWERING_INVALID_ID,
						PROTOTYPE_SOURCE_BINDER_FOLD_ARGUMENT, i
					) != 0 || builder_add_binder(
						builder, owner_slot,
						clause->operation_continuation_binder_id,
						clause->operation_continuation_symbol_id,
						SOURCE_LOWERING_INVALID_ID,
						PROTOTYPE_SOURCE_BINDER_FOLD_CONTINUATION, i
					) != 0) {
					status = -1;
					break;
				}
			}
			break;
		case PROTOTYPE_AST_TERMINATES_WITNESS:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.terminates_witness.computation,
				PROTOTYPE_SOURCE_EDGE_CERTIFIED_COMPUTATION, 0
			);
			break;
		case PROTOTYPE_AST_CERTIFIED_ELIMINATION:
			status = builder_add_term_edge(
				builder, owner_slot, node->as.certified_elimination.computation,
				PROTOTYPE_SOURCE_EDGE_CERTIFIED_COMPUTATION, 0
			) != 0 || builder_add_term_edge(
				builder, owner_slot, node->as.certified_elimination.body,
				PROTOTYPE_SOURCE_EDGE_CERTIFIED_BODY, 0
			) != 0 || builder_add_binder(
				builder, owner_slot,
				node->as.certified_elimination.result_ast_binder_id,
				node->as.certified_elimination.result_symbol_id,
				SOURCE_LOWERING_INVALID_ID,
				PROTOTYPE_SOURCE_BINDER_CERTIFIED_RESULT, 0
			) != 0 || builder_add_binder(
				builder, owner_slot,
				node->as.certified_elimination.graph_ast_binder_id,
				-1,
				SOURCE_LOWERING_INVALID_ID,
				PROTOTYPE_SOURCE_BINDER_CERTIFIED_GRAPH, 0
			) != 0 ? -1 : 0;
			break;
		default:
			status = -1;
			break;
	}
	if (status != 0 || builder->edge_count - first_edge > UINT32_MAX ||
		builder->binder_count - first_binder > UINT32_MAX) {
		return -1;
	}
	if (builder->write) {
		builder->slots[owner_slot] = (struct prototype_source_lowering_slot) {
			.kind = PROTOTYPE_SOURCE_LOWERING_TERM,
			.source_id = ast_node_id,
			.source_tag = node->tag,
			.owner_type_def_id = SOURCE_LOWERING_INVALID_ID,
			.span = node->span,
			.first_edge = (uint32_t)first_edge,
			.edge_count = (uint32_t)(builder->edge_count - first_edge),
			.first_binder = (uint32_t)first_binder,
			.binder_count = (uint32_t)(builder->binder_count - first_binder)
		};
	}
	return 0;
}

static int process_type_expr_slot(
	struct source_lowering_plan_builder* builder,
	uint32_t ast_type_expr_id
) {
	const struct prototype_ast_db* asts = builder->asts;
	const struct prototype_ast_type_expr* expr =
		&asts->type_exprs[ast_type_expr_id];
	uint32_t owner_slot;
	if (type_expr_slot(asts, ast_type_expr_id, &owner_slot) != 0) return -1;
	size_t first_edge = builder->edge_count;
	size_t first_binder = builder->binder_count;
	uint32_t owner_type_def_id = SOURCE_LOWERING_INVALID_ID;
	int status = 0;
	switch (expr->tag) {
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE:
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR:
		case PROTOTYPE_AST_TYPE_EXPR_VAR:
		case PROTOTYPE_AST_TYPE_EXPR_NAME:
		case PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE:
		case PROTOTYPE_AST_TYPE_EXPR_NAME_IN_NAMESPACE:
		case PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE:
		case PROTOTYPE_AST_TYPE_EXPR_ACCEPTED_SUBSTITUTION:
			break;
		case PROTOTYPE_AST_TYPE_EXPR_SELF:
			status = self_type_expr_owner(
				asts, ast_type_expr_id, &owner_type_def_id
			);
			break;
		case PROTOTYPE_AST_TYPE_EXPR_APP:
			status = builder_add_type_edge(
				builder, owner_slot, expr->as.app.function,
				PROTOTYPE_SOURCE_EDGE_FUNCTION, 0
			) != 0 || builder_add_type_edge(
				builder, owner_slot, expr->as.app.argument,
				PROTOTYPE_SOURCE_EDGE_ARGUMENT, 0
			) != 0 ? -1 : 0;
			break;
		case PROTOTYPE_AST_TYPE_EXPR_ARROW:
			status = builder_add_type_edge(
				builder, owner_slot, expr->as.arrow.domain,
				PROTOTYPE_SOURCE_EDGE_TYPE_DOMAIN, 0
			) != 0 || builder_add_type_edge(
				builder, owner_slot, expr->as.arrow.codomain,
				PROTOTYPE_SOURCE_EDGE_TYPE_CODOMAIN, 0
			) != 0 ? -1 : 0;
			break;
		case PROTOTYPE_AST_TYPE_EXPR_PI: {
			uint32_t annotation_slot;
			status = type_expr_slot(
				asts, expr->as.pi.domain, &annotation_slot
			) != 0 || builder_add_type_edge(
				builder, owner_slot, expr->as.pi.domain,
				PROTOTYPE_SOURCE_EDGE_TYPE_DOMAIN, 0
			) != 0 || builder_add_type_edge(
				builder, owner_slot, expr->as.pi.codomain,
				PROTOTYPE_SOURCE_EDGE_TYPE_CODOMAIN, 0
			) != 0 || builder_add_binder(
				builder, owner_slot, expr->as.pi.ast_binder_id,
				expr->as.pi.symbol_id, annotation_slot,
				PROTOTYPE_SOURCE_BINDER_PI, 0
			) != 0 ? -1 : 0;
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE:
			status = builder_add_type_edge(
				builder, owner_slot, expr->as.computation_reference.result,
				PROTOTYPE_SOURCE_EDGE_TYPE_RESULT, 0
			) != 0 || builder_add_binder(
				builder, owner_slot, SOURCE_LOWERING_INVALID_ID, -1,
				SOURCE_LOWERING_INVALID_ID,
				PROTOTYPE_SOURCE_BINDER_COMPUTATION_EFFECT_ROW, 0
			) != 0 ? -1 : 0;
			break;
		case PROTOTYPE_AST_TYPE_EXPR_VALUE_REFERENCE:
			status = builder_add_term_edge(
				builder, owner_slot, expr->as.value_reference.value,
				PROTOTYPE_SOURCE_EDGE_TYPE_VALUE, 0
			);
			break;
		case PROTOTYPE_AST_TYPE_EXPR_TERMINATES:
			status = builder_add_term_edge(
				builder, owner_slot, expr->as.terminates.computation,
				PROTOTYPE_SOURCE_EDGE_CERTIFIED_COMPUTATION, 0
			);
			break;
		default:
			status = -1;
			break;
	}
	if (status != 0 || builder->edge_count - first_edge > UINT32_MAX ||
		builder->binder_count - first_binder > UINT32_MAX) {
		return -1;
	}
	if (builder->write) {
		builder->slots[owner_slot] = (struct prototype_source_lowering_slot) {
			.kind = PROTOTYPE_SOURCE_LOWERING_TYPE_EXPR,
			.source_id = ast_type_expr_id,
			.source_tag = expr->tag,
			.owner_type_def_id = owner_type_def_id,
			.span = expr->span,
			.first_edge = (uint32_t)first_edge,
			.edge_count = (uint32_t)(builder->edge_count - first_edge),
			.first_binder = (uint32_t)first_binder,
			.binder_count = (uint32_t)(builder->binder_count - first_binder)
		};
	}
	return 0;
}

static int process_all_slots(struct source_lowering_plan_builder* builder) {
	if (!builder || !builder->asts) return -1;
	for (uint32_t i = 0; i < builder->asts->node_count; ++i) {
		if (process_term_slot(builder, i) != 0) return -1;
	}
	for (uint32_t i = 0; i < builder->asts->type_expr_count; ++i) {
		if (process_type_expr_slot(builder, i) != 0) return -1;
	}
	return 0;
}

static int build_roots(
	const struct prototype_ast_db* asts,
	const struct prototype_source_schedule* schedule,
	struct prototype_source_lowering_root* roots,
	size_t root_capacity
) {
	if (!asts || !schedule || schedule->item_count > root_capacity ||
		(schedule->item_count > 0 && !roots)) {
		return -1;
	}
	for (size_t i = 0; i < schedule->item_count; ++i) {
		const struct prototype_source_schedule_item* item = &schedule->items[i];
		uint32_t slot = SOURCE_LOWERING_INVALID_ID;
		switch (item->kind) {
			case PROTOTYPE_SOURCE_SCHEDULE_IMPORT:
				if (item->table_id >= asts->import_count) return -1;
				break;
			case PROTOTYPE_SOURCE_SCHEDULE_EXPECTATION:
				if (item->table_id >= asts->expectation_count || type_expr_slot(
						asts, asts->expectations[item->table_id].type_expr, &slot
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_SOURCE_SCHEDULE_ASSIGNMENT:
				if (item->table_id >= asts->assignment_count || term_slot(
						asts, asts->assignments[item->table_id].ast, &slot
					) != 0) {
					return -1;
				}
				break;
			default:
				return -1;
		}
		roots[i] = (struct prototype_source_lowering_root) {
			.source_entry_id = item->source_entry_id,
			.table_id = item->table_id,
			.slot_id = slot,
			.source_item_kind = item->kind
		};
	}
	return 0;
}

static int build_order(
	const struct prototype_source_lowering_slot* slots,
	size_t slot_count_value,
	const struct prototype_source_lowering_edge* edges,
	size_t edge_count,
	uint32_t* order,
	size_t order_capacity
) {
	if (slot_count_value > order_capacity || (slot_count_value > 0 &&
		(!slots || !order)) || (edge_count > 0 && !edges)) {
		return -1;
	}
	uint32_t* remaining = slot_count_value == 0 ? NULL :
		calloc(slot_count_value, sizeof(*remaining));
	uint32_t* heads = slot_count_value == 0 ? NULL :
		malloc(slot_count_value * sizeof(*heads));
	uint32_t* next = edge_count == 0 ? NULL :
		malloc(edge_count * sizeof(*next));
	uint32_t* queue = slot_count_value == 0 ? NULL :
		malloc(slot_count_value * sizeof(*queue));
	if ((slot_count_value > 0 && (!remaining || !heads || !queue)) ||
		(edge_count > 0 && !next)) {
		free(queue);
		free(next);
		free(heads);
		free(remaining);
		return -1;
	}
	for (size_t i = 0; i < slot_count_value; ++i) {
		heads[i] = SOURCE_LOWERING_INVALID_ID;
		remaining[i] = slots[i].edge_count;
	}
	for (uint32_t i = 0; i < edge_count; ++i) {
		if (edges[i].owner_slot >= slot_count_value ||
			edges[i].child_slot >= slot_count_value) {
			free(queue);
			free(next);
			free(heads);
			free(remaining);
			return -1;
		}
		next[i] = heads[edges[i].child_slot];
		heads[edges[i].child_slot] = i;
	}
	size_t queue_head = 0;
	size_t queue_tail = 0;
	for (uint32_t i = 0; i < slot_count_value; ++i) {
		if (remaining[i] == 0) queue[queue_tail++] = i;
	}
	size_t order_count = 0;
	while (queue_head < queue_tail) {
		uint32_t child = queue[queue_head++];
		order[order_count++] = child;
		for (uint32_t edge_id = heads[child];
			edge_id != SOURCE_LOWERING_INVALID_ID;
			edge_id = next[edge_id]) {
			uint32_t owner = edges[edge_id].owner_slot;
			if (remaining[owner] == 0) {
				order_count = 0;
				break;
			}
			remaining[owner]--;
			if (remaining[owner] == 0) queue[queue_tail++] = owner;
		}
		if (order_count == 0) break;
	}
	free(queue);
	free(next);
	free(heads);
	free(remaining);
	return order_count == slot_count_value ? 0 : -1;
}

static int source_scope_edge_id(
	const struct prototype_source_lowering_slot* slots,
	size_t slot_count,
	const struct prototype_source_lowering_edge* edges,
	size_t edge_count,
	uint32_t owner_slot,
	int role,
	uint32_t ordinal,
	uint32_t* p_edge_id
) {
	if (!slots || !edges || !p_edge_id || owner_slot >= slot_count) return -1;
	const struct prototype_source_lowering_slot* slot = &slots[owner_slot];
	if (slot->first_edge > edge_count ||
		slot->edge_count > edge_count - slot->first_edge) return -1;
	for (uint32_t i = 0; i < slot->edge_count; ++i) {
		uint32_t edge_id = slot->first_edge + i;
		if (edges[edge_id].owner_slot == owner_slot &&
			edges[edge_id].role == role && edges[edge_id].ordinal == ordinal) {
			*p_edge_id = edge_id;
			return 0;
		}
	}
	return -1;
}

static int source_scope_binder_id(
	const struct prototype_source_lowering_slot* slots,
	size_t slot_count,
	const struct prototype_source_lowering_binder* binders,
	size_t binder_count,
	uint32_t owner_slot,
	int role,
	uint32_t ordinal,
	uint32_t* p_binder_id
) {
	if (!slots || !binders || !p_binder_id || owner_slot >= slot_count) return -1;
	const struct prototype_source_lowering_slot* slot = &slots[owner_slot];
	if (slot->first_binder > binder_count ||
		slot->binder_count > binder_count - slot->first_binder) return -1;
	for (uint32_t i = 0; i < slot->binder_count; ++i) {
		uint32_t binder_id = slot->first_binder + i;
		if (binders[binder_id].owner_slot == owner_slot &&
			binders[binder_id].role == role &&
			binders[binder_id].ordinal == ordinal) {
			*p_binder_id = binder_id;
			return 0;
		}
	}
	return -1;
}

static int source_scope_extend(
	struct prototype_source_lowering_scope_recipe* recipes,
	size_t recipe_capacity,
	size_t* p_recipe_count,
	uint32_t* recipe_for_binder,
	size_t binder_count,
	uint32_t parent_recipe,
	uint32_t binder_id,
	uint32_t* p_recipe_id
) {
	if (!recipes || !p_recipe_count || !recipe_for_binder || !p_recipe_id ||
		binder_id >= binder_count ||
		(parent_recipe != SOURCE_LOWERING_INVALID_ID &&
		 parent_recipe >= *p_recipe_count)) return -1;
	uint32_t existing = recipe_for_binder[binder_id];
	if (existing != SOURCE_LOWERING_INVALID_ID) {
		if (existing >= *p_recipe_count ||
			recipes[existing].parent_recipe != parent_recipe) return -1;
		*p_recipe_id = existing;
		return 0;
	}
	if (*p_recipe_count >= recipe_capacity || *p_recipe_count > UINT32_MAX) {
		return -1;
	}
	uint32_t recipe_id = (uint32_t)(*p_recipe_count);
	(*p_recipe_count)++;
	recipes[recipe_id] = (struct prototype_source_lowering_scope_recipe) {
		.parent_recipe = parent_recipe,
		.binder_id = binder_id
	};
	recipe_for_binder[binder_id] = recipe_id;
	*p_recipe_id = recipe_id;
	return 0;
}

static int source_scope_assign_edge(
	const struct prototype_source_lowering_slot* slots,
	size_t slot_count,
	struct prototype_source_lowering_edge* edges,
	size_t edge_count,
	uint32_t owner_slot,
	int role,
	uint32_t ordinal,
	uint32_t recipe_id
) {
	uint32_t edge_id;
	if (source_scope_edge_id(
			slots, slot_count, edges, edge_count, owner_slot, role, ordinal,
			&edge_id
		) != 0 || edges[edge_id].scope_recipe != SOURCE_LOWERING_INVALID_ID) {
		return -1;
	}
	edges[edge_id].scope_recipe = recipe_id;
	return 0;
}

static int build_type_definition_scope_recipes(
	const struct prototype_ast_db* asts,
	const struct prototype_source_lowering_slot* slots,
	size_t slot_count,
	struct prototype_source_lowering_edge* edges,
	size_t edge_count,
	const struct prototype_source_lowering_binder* binders,
	size_t binder_count,
	struct prototype_source_lowering_scope_recipe* recipes,
	size_t recipe_capacity,
	size_t* p_recipe_count,
	uint32_t* recipe_for_binder,
	uint32_t owner_slot,
	uint32_t type_def_id
) {
	if (!asts || type_def_id >= asts->type_def_count) return -1;
	const struct prototype_ast_type_def* type = &asts->type_defs[type_def_id];
	size_t family_count = (size_t)type->parameter_count + type->index_count;
	if (family_count > UINT32_MAX ||
		type->first_constructor > asts->type_constructor_count ||
		type->constructor_count > asts->type_constructor_count -
			type->first_constructor) return -1;
	uint32_t family_recipe = SOURCE_LOWERING_INVALID_ID;
	for (uint32_t i = 0; i < (uint32_t)family_count; ++i) {
		uint32_t binder_id;
		if (source_scope_assign_edge(
				slots, slot_count, edges, edge_count, owner_slot,
				PROTOTYPE_SOURCE_EDGE_TYPE_FAMILY_BINDER, i, family_recipe
			) != 0 || source_scope_binder_id(
				slots, slot_count, binders, binder_count, owner_slot,
				i < type->parameter_count ?
					PROTOTYPE_SOURCE_BINDER_FAMILY_PARAMETER :
					PROTOTYPE_SOURCE_BINDER_FAMILY_INDEX,
				i, &binder_id
			) != 0 || source_scope_extend(
				recipes, recipe_capacity, p_recipe_count, recipe_for_binder,
				binder_count, family_recipe, binder_id, &family_recipe
			) != 0) return -1;
	}
	uint32_t field_ordinal = 0;
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_ast_type_constructor* constructor =
			&asts->type_constructors[type->first_constructor + i];
		if (constructor->first_field_type > asts->type_field_expr_count ||
			constructor->field_count > asts->type_field_expr_count -
				constructor->first_field_type) return -1;
		uint32_t constructor_recipe = family_recipe;
		for (uint32_t j = 0; j < constructor->field_count; ++j) {
			uint32_t binder_id;
			if (source_scope_assign_edge(
					slots, slot_count, edges, edge_count, owner_slot,
					PROTOTYPE_SOURCE_EDGE_TYPE_CONSTRUCTOR_FIELD,
					field_ordinal, constructor_recipe
				) != 0 || source_scope_binder_id(
					slots, slot_count, binders, binder_count, owner_slot,
					PROTOTYPE_SOURCE_BINDER_CONSTRUCTOR_FIELD,
					field_ordinal, &binder_id
				) != 0 || source_scope_extend(
					recipes, recipe_capacity, p_recipe_count, recipe_for_binder,
					binder_count, constructor_recipe, binder_id,
					&constructor_recipe
				) != 0) return -1;
			field_ordinal++;
		}
		if (source_scope_assign_edge(
				slots, slot_count, edges, edge_count, owner_slot,
				PROTOTYPE_SOURCE_EDGE_TYPE_CONSTRUCTOR_RESULT, i,
				constructor_recipe
			) != 0) return -1;
	}
	return 0;
}

static int build_scope_recipes(
	const struct prototype_ast_db* asts,
	const struct prototype_source_lowering_slot* slots,
	size_t slot_count,
	struct prototype_source_lowering_edge* edges,
	size_t edge_count,
	const struct prototype_source_lowering_binder* binders,
	size_t binder_count,
	struct prototype_source_lowering_scope_recipe* recipes,
	size_t recipe_capacity,
	size_t* p_recipe_count
) {
	if (!asts || (slot_count != 0 && !slots) || (edge_count != 0 && !edges) ||
		(binder_count != 0 && (!binders || !recipes)) || !p_recipe_count) {
		return -1;
	}
	uint32_t* recipe_for_binder = binder_count == 0 ? NULL :
		malloc(binder_count * sizeof(*recipe_for_binder));
	if (binder_count != 0 && !recipe_for_binder) return -1;
	for (size_t i = 0; i < binder_count; ++i) {
		recipe_for_binder[i] = SOURCE_LOWERING_INVALID_ID;
	}
	*p_recipe_count = 0;
	for (uint32_t owner_slot = 0; owner_slot < slot_count; ++owner_slot) {
		const struct prototype_source_lowering_slot* slot = &slots[owner_slot];
		if (slot->kind == PROTOTYPE_SOURCE_LOWERING_TERM) {
			const struct prototype_ast_node* node = &asts->nodes[slot->source_id];
			if (node->tag == PROTOTYPE_AST_LAMBDA) {
				uint32_t binder_id;
				uint32_t recipe;
				if (source_scope_binder_id(
						slots, slot_count, binders, binder_count, owner_slot,
						PROTOTYPE_SOURCE_BINDER_LAMBDA, 0, &binder_id
					) != 0 || source_scope_extend(
						recipes, recipe_capacity, p_recipe_count,
						recipe_for_binder, binder_count,
						SOURCE_LOWERING_INVALID_ID, binder_id, &recipe
					) != 0 || source_scope_assign_edge(
						slots, slot_count, edges, edge_count, owner_slot,
						PROTOTYPE_SOURCE_EDGE_BODY, 0, recipe
					) != 0) goto fail;
			} else if (node->tag == PROTOTYPE_AST_MATCH) {
				uint32_t binder_cursor = slot->first_binder;
				for (uint32_t ordinal = 0;
					ordinal < node->as.match.case_count; ++ordinal) {
					const struct prototype_ast_match_case* match_case =
						&asts->cases[node->as.match.first_case + ordinal];
					uint32_t recipe = SOURCE_LOWERING_INVALID_ID;
					uint32_t case_binders = match_case->binder_count +
						match_case->selector_count * 2;
					if (binder_cursor > binder_count ||
						case_binders > binder_count - binder_cursor) goto fail;
					for (uint32_t i = 0; i < case_binders; ++i) {
						if (source_scope_extend(
								recipes, recipe_capacity, p_recipe_count,
								recipe_for_binder, binder_count, recipe,
								binder_cursor + i, &recipe
							) != 0) goto fail;
					}
					if (source_scope_assign_edge(
							slots, slot_count, edges, edge_count, owner_slot,
							PROTOTYPE_SOURCE_EDGE_MATCH_CASE_BODY, ordinal, recipe
						) != 0) goto fail;
					binder_cursor += case_binders;
				}
				if (binder_cursor != slot->first_binder + slot->binder_count) {
					goto fail;
				}
			} else if (node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK) {
				uint32_t recipe = SOURCE_LOWERING_INVALID_ID;
				for (uint32_t i = 0; i < node->as.block.item_count; ++i) {
					uint32_t edge_id;
					if (source_scope_edge_id(
							slots, slot_count, edges, edge_count, owner_slot,
							PROTOTYPE_SOURCE_EDGE_BLOCK_ITEM, i, &edge_id
						) != 0 || edges[edge_id].scope_recipe !=
							SOURCE_LOWERING_INVALID_ID) goto fail;
					edges[edge_id].scope_recipe = recipe;
					uint32_t child_slot = edges[edge_id].child_slot;
					if (child_slot >= slot_count) goto fail;
					const struct prototype_source_lowering_slot* child =
						&slots[child_slot];
					if (child->kind == PROTOTYPE_SOURCE_LOWERING_TERM &&
						child->source_tag == PROTOTYPE_AST_BLOCK_BINDING) {
						uint32_t binder_id;
						if (source_scope_binder_id(
								slots, slot_count, binders, binder_count, child_slot,
								PROTOTYPE_SOURCE_BINDER_BLOCK_RESULT, 0, &binder_id
							) != 0 || source_scope_extend(
								recipes, recipe_capacity, p_recipe_count,
								recipe_for_binder, binder_count, recipe, binder_id,
								&recipe
							) != 0) goto fail;
					}
				}
			} else if (node->tag == PROTOTYPE_AST_COMPUTATION_FOLD) {
				uint32_t return_recipe;
				if (source_scope_extend(
						recipes, recipe_capacity, p_recipe_count,
						recipe_for_binder, binder_count,
						SOURCE_LOWERING_INVALID_ID, slot->first_binder,
						&return_recipe
					) != 0 || source_scope_assign_edge(
						slots, slot_count, edges, edge_count, owner_slot,
						PROTOTYPE_SOURCE_EDGE_FOLD_RETURN_BODY, 0, return_recipe
					) != 0) goto fail;
				for (uint32_t i = 0;
					i < node->as.computation_fold.clause_count; ++i) {
					uint32_t recipe = SOURCE_LOWERING_INVALID_ID;
					uint32_t argument = slot->first_binder + 1 + i * 2;
					if (source_scope_extend(
							recipes, recipe_capacity, p_recipe_count,
							recipe_for_binder, binder_count, recipe, argument,
							&recipe
						) != 0 || source_scope_extend(
							recipes, recipe_capacity, p_recipe_count,
							recipe_for_binder, binder_count, recipe, argument + 1,
							&recipe
						) != 0 || source_scope_assign_edge(
							slots, slot_count, edges, edge_count, owner_slot,
							PROTOTYPE_SOURCE_EDGE_FOLD_CLAUSE_BODY, i, recipe
						) != 0) goto fail;
				}
			} else if (node->tag == PROTOTYPE_AST_CERTIFIED_ELIMINATION) {
				uint32_t recipe = SOURCE_LOWERING_INVALID_ID;
				for (uint32_t i = 0; i < slot->binder_count; ++i) {
					if (source_scope_extend(
							recipes, recipe_capacity, p_recipe_count,
							recipe_for_binder, binder_count, recipe,
							slot->first_binder + i, &recipe
						) != 0) goto fail;
				}
				if (source_scope_assign_edge(
						slots, slot_count, edges, edge_count, owner_slot,
						PROTOTYPE_SOURCE_EDGE_CERTIFIED_BODY, 0, recipe
					) != 0) goto fail;
			} else if (node->tag == PROTOTYPE_AST_TYPE_LITERAL ||
				node->tag == PROTOTYPE_AST_TYPE_FORMATION) {
				if (build_type_definition_scope_recipes(
						asts, slots, slot_count, edges, edge_count, binders,
						binder_count, recipes, recipe_capacity, p_recipe_count,
						recipe_for_binder, owner_slot,
						node->as.type_formation.ast_type_def_id
					) != 0) goto fail;
			}
		} else if (slot->kind == PROTOTYPE_SOURCE_LOWERING_TYPE_EXPR &&
			slot->source_tag == PROTOTYPE_AST_TYPE_EXPR_PI) {
			uint32_t binder_id;
			uint32_t recipe;
			if (source_scope_binder_id(
					slots, slot_count, binders, binder_count, owner_slot,
					PROTOTYPE_SOURCE_BINDER_PI, 0, &binder_id
				) != 0 || source_scope_extend(
					recipes, recipe_capacity, p_recipe_count, recipe_for_binder,
					binder_count, SOURCE_LOWERING_INVALID_ID, binder_id, &recipe
				) != 0 || source_scope_assign_edge(
					slots, slot_count, edges, edge_count, owner_slot,
					PROTOTYPE_SOURCE_EDGE_TYPE_CODOMAIN, 0, recipe
				) != 0) goto fail;
		}
	}
	free(recipe_for_binder);
	return 0;

fail:
	free(recipe_for_binder);
	return -1;
}

static void digest_mix_u32(uint64_t* digest, uint32_t value) {
	for (unsigned i = 0; i < 4; ++i) {
		*digest ^= (uint8_t)(value >> (i * 8));
		*digest *= SOURCE_LOWERING_HASH_PRIME;
	}
}

static void digest_mix_size(uint64_t* digest, size_t value) {
	for (unsigned i = 0; i < sizeof(value); ++i) {
		*digest ^= (uint8_t)(value >> (i * 8));
		*digest *= SOURCE_LOWERING_HASH_PRIME;
	}
}

static void digest_mix_bytes(
	uint64_t* digest,
	const void* data,
	size_t byte_count
) {
	const unsigned char* bytes = data;
	for (size_t i = 0; i < byte_count; ++i) {
		*digest ^= bytes[i];
		*digest *= SOURCE_LOWERING_HASH_PRIME;
	}
}

#define SOURCE_FINGERPRINT_ARRAY(digest, db, member, count_member) \
	do { \
		digest_mix_size((digest), (db)->count_member); \
		if ((db)->count_member > 0) { \
			digest_mix_bytes( \
				(digest), (db)->member, \
				(db)->count_member * sizeof(*(db)->member) \
			); \
		} \
	} while (0)

static uint64_t source_fingerprint(const struct prototype_ast_db* source) {
	if (!source) return 0;
	uint64_t digest = SOURCE_LOWERING_HASH_OFFSET;
	SOURCE_FINGERPRINT_ARRAY(&digest, source, nodes, node_count);
	SOURCE_FINGERPRINT_ARRAY(&digest, source, expectations, expectation_count);
	SOURCE_FINGERPRINT_ARRAY(&digest, source, assignments, assignment_count);
	SOURCE_FINGERPRINT_ARRAY(&digest, source, imports, import_count);
	digest_mix_size(&digest, source->def_index_capacity);
	if (source->def_index_capacity > 0) {
		digest_mix_bytes(
			&digest, source->def_index,
			source->def_index_capacity * sizeof(*source->def_index)
		);
	}
	SOURCE_FINGERPRINT_ARRAY(&digest, source, cases, case_count);
	SOURCE_FINGERPRINT_ARRAY(&digest, source, case_binders, case_binder_count);
	SOURCE_FINGERPRINT_ARRAY(
		&digest, source, match_selectors, match_selector_count
	);
	SOURCE_FINGERPRINT_ARRAY(
		&digest, source, computation_fold_clauses,
		computation_fold_clause_count
	);
	SOURCE_FINGERPRINT_ARRAY(&digest, source, block_items, block_item_count);
	SOURCE_FINGERPRINT_ARRAY(
		&digest, source, definition_items, definition_item_count
	);
	SOURCE_FINGERPRINT_ARRAY(&digest, source, type_exprs, type_expr_count);
	SOURCE_FINGERPRINT_ARRAY(&digest, source, type_defs, type_def_count);
	SOURCE_FINGERPRINT_ARRAY(
		&digest, source, family_binders, family_binder_count
	);
	SOURCE_FINGERPRINT_ARRAY(
		&digest, source, type_constructors, type_constructor_count
	);
	SOURCE_FINGERPRINT_ARRAY(
		&digest, source, type_field_exprs, type_field_expr_count
	);
	digest_mix_size(&digest, source->type_field_expr_count);
	if (source->type_field_expr_count > 0 && source->type_field_binder_ids) {
		digest_mix_bytes(
			&digest, source->type_field_binder_ids,
			source->type_field_expr_count * sizeof(*source->type_field_binder_ids)
		);
	}
	digest_mix_size(&digest, source->type_field_expr_count);
	if (source->type_field_expr_count > 0 && source->type_field_name_symbol_ids) {
		digest_mix_bytes(
			&digest, source->type_field_name_symbol_ids,
			source->type_field_expr_count *
				sizeof(*source->type_field_name_symbol_ids)
		);
	}
	SOURCE_FINGERPRINT_ARRAY(
		&digest, source, accepted_binding_substitutions,
		accepted_binding_substitution_count
	);
	digest_mix_u32(&digest, source->root_definition_block);
	digest_mix_u32(&digest, source->root_definition_select);
	digest_mix_u32(&digest, source->next_ast_binder_id);
	digest_mix_u32(&digest, source->next_ast_level_var);
	digest_mix_u32(&digest, source->next_source_entry_id);
	return digest;
}

int prototype_source_epoch_fingerprint(
	const struct prototype_ast_db* source,
	uint64_t* p_fingerprint
) {
	if (!source || !p_fingerprint) return -1;
	*p_fingerprint = source_fingerprint(source);
	return 0;
}

#undef SOURCE_FINGERPRINT_ARRAY

static uint64_t plan_digest(
	const struct prototype_source_lowering_plan* plan
) {
	uint64_t digest = SOURCE_LOWERING_HASH_OFFSET;
	digest_mix_u32(&digest, (uint32_t)plan->slot_count);
	digest_mix_u32(&digest, (uint32_t)plan->edge_count);
	digest_mix_u32(&digest, (uint32_t)plan->binder_count);
	digest_mix_u32(&digest, (uint32_t)plan->scope_recipe_count);
	digest_mix_u32(&digest, (uint32_t)plan->root_count);
	for (size_t i = 0; i < plan->slot_count; ++i) {
		const struct prototype_source_lowering_slot* slot = &plan->slots[i];
		digest_mix_u32(&digest, (uint32_t)slot->kind);
		digest_mix_u32(&digest, slot->source_id);
		digest_mix_u32(&digest, (uint32_t)slot->source_tag);
		digest_mix_u32(&digest, slot->owner_type_def_id);
		digest_mix_u32(&digest, slot->span.line);
		digest_mix_u32(&digest, slot->span.column);
		digest_mix_u32(&digest, slot->first_edge);
		digest_mix_u32(&digest, slot->edge_count);
		digest_mix_u32(&digest, slot->first_binder);
		digest_mix_u32(&digest, slot->binder_count);
	}
	for (size_t i = 0; i < plan->edge_count; ++i) {
		const struct prototype_source_lowering_edge* edge = &plan->edges[i];
		digest_mix_u32(&digest, edge->owner_slot);
		digest_mix_u32(&digest, edge->child_slot);
		digest_mix_u32(&digest, edge->ordinal);
		digest_mix_u32(&digest, (uint32_t)edge->role);
		digest_mix_u32(&digest, edge->scope_recipe);
	}
	for (size_t i = 0; i < plan->binder_count; ++i) {
		const struct prototype_source_lowering_binder* binder = &plan->binders[i];
		digest_mix_u32(&digest, binder->owner_slot);
		digest_mix_u32(&digest, binder->source_binder_id);
		digest_mix_u32(&digest, (uint32_t)binder->symbol_id);
		digest_mix_u32(&digest, binder->annotation_slot);
		digest_mix_u32(&digest, binder->ordinal);
		digest_mix_u32(&digest, (uint32_t)binder->role);
	}
	for (size_t i = 0; i < plan->scope_recipe_count; ++i) {
		const struct prototype_source_lowering_scope_recipe* recipe =
			&plan->scope_recipes[i];
		digest_mix_u32(&digest, recipe->parent_recipe);
		digest_mix_u32(&digest, recipe->binder_id);
	}
	for (size_t i = 0; i < plan->root_count; ++i) {
		const struct prototype_source_lowering_root* root = &plan->roots[i];
		digest_mix_u32(&digest, root->source_entry_id);
		digest_mix_u32(&digest, root->table_id);
		digest_mix_u32(&digest, root->slot_id);
		digest_mix_u32(&digest, (uint32_t)root->source_item_kind);
	}
	for (size_t i = 0; i < plan->order_count; ++i) {
		digest_mix_u32(&digest, plan->order[i]);
	}
	return digest;
}

int prototype_source_lowering_plan_measure(
	const struct prototype_ast_db* asts,
	const struct prototype_source_schedule* schedule,
	struct prototype_source_lowering_plan_capacity* p_capacity
) {
	if (!asts || !schedule || !p_capacity || schedule->item_count > UINT32_MAX) {
		return -1;
	}
	size_t slots;
	if (source_slot_count(asts, &slots) != 0) return -1;
	struct source_lowering_plan_builder builder;
	memset(&builder, 0, sizeof(builder));
	builder.asts = asts;
	if (process_all_slots(&builder) != 0) return -1;
	for (size_t i = 0; i < schedule->item_count; ++i) {
		const struct prototype_source_schedule_item* item = &schedule->items[i];
		switch (item->kind) {
			case PROTOTYPE_SOURCE_SCHEDULE_IMPORT:
				if (item->table_id >= asts->import_count) return -1;
				break;
			case PROTOTYPE_SOURCE_SCHEDULE_EXPECTATION:
				if (item->table_id >= asts->expectation_count) return -1;
				if (asts->expectations[item->table_id].type_expr >=
					asts->type_expr_count) return -1;
				break;
			case PROTOTYPE_SOURCE_SCHEDULE_ASSIGNMENT:
				if (item->table_id >= asts->assignment_count) return -1;
				if (asts->assignments[item->table_id].ast >= asts->node_count) {
					return -1;
				}
				break;
			default:
				return -1;
		}
	}
	*p_capacity = (struct prototype_source_lowering_plan_capacity) {
		.slot_count = slots,
		.edge_count = builder.edge_count,
		.binder_count = builder.binder_count,
		.scope_recipe_count = builder.binder_count,
		.root_count = schedule->item_count
	};
	return 0;
}

int prototype_source_lowering_plan_build(
	const struct prototype_ast_db* asts,
	const struct prototype_source_schedule* schedule,
	struct prototype_source_lowering_slot* slots,
	size_t slot_capacity,
	struct prototype_source_lowering_edge* edges,
	size_t edge_capacity,
	struct prototype_source_lowering_binder* binders,
	size_t binder_capacity,
	struct prototype_source_lowering_scope_recipe* scope_recipes,
	size_t scope_recipe_capacity,
	struct prototype_source_lowering_root* roots,
	size_t root_capacity,
	uint32_t* order,
	size_t order_capacity,
	struct prototype_source_lowering_plan* p_plan
) {
	struct prototype_source_lowering_plan_capacity required;
	if (!p_plan) return -1;
	if (prototype_source_lowering_plan_measure(
			asts, schedule, &required
		) != 0) return -1;
	if (required.slot_count > slot_capacity ||
		required.slot_count > order_capacity) return -1;
	if (required.edge_count > edge_capacity) return -1;
	if (required.binder_count > binder_capacity) return -1;
	if (required.scope_recipe_count > scope_recipe_capacity) return -1;
	if (required.root_count > root_capacity) return -1;
	if (required.slot_count > 0 && (!slots || !order)) return -1;
	if (required.edge_count > 0 && !edges) return -1;
	if (required.binder_count > 0 && !binders) return -1;
	if (required.scope_recipe_count > 0 && !scope_recipes) return -1;
	if (required.root_count > 0 && !roots) return -1;
	memset(p_plan, 0, sizeof(*p_plan));
	struct source_lowering_plan_builder builder;
	memset(&builder, 0, sizeof(builder));
	builder.asts = asts;
	builder.slots = slots;
	builder.slot_capacity = slot_capacity;
	builder.edges = edges;
	builder.edge_capacity = edge_capacity;
	builder.binders = binders;
	builder.binder_capacity = binder_capacity;
	builder.write = 1;
	size_t scope_recipe_count = 0;
	if (process_all_slots(&builder) != 0) return -1;
	if (builder.edge_count != required.edge_count ||
		builder.binder_count != required.binder_count) return -1;
	if (build_scope_recipes(
			asts, slots, required.slot_count, edges, required.edge_count,
			binders, required.binder_count, scope_recipes,
			scope_recipe_capacity, &scope_recipe_count
		) != 0) return -1;
	if (scope_recipe_count > required.scope_recipe_count) return -1;
	if (build_roots(asts, schedule, roots, root_capacity) != 0) return -1;
	if (build_order(
			slots, required.slot_count, edges, required.edge_count,
			order, order_capacity
		) != 0) return -1;
	*p_plan = (struct prototype_source_lowering_plan) {
		.source = asts,
		.slots = slots,
		.slot_count = required.slot_count,
		.edges = edges,
		.edge_count = required.edge_count,
		.binders = binders,
		.binder_count = required.binder_count,
		.scope_recipes = scope_recipes,
		.scope_recipe_count = scope_recipe_count,
		.roots = roots,
		.root_count = required.root_count,
		.order = order,
		.order_count = required.slot_count,
		.first_type_expr_slot = (uint32_t)asts->node_count,
		.source_fingerprint = source_fingerprint(asts),
		.sealed = 1
	};
	p_plan->topology_digest = plan_digest(p_plan);
	return prototype_source_lowering_plan_validate(p_plan);
}

int prototype_source_lowering_plan_validate(
	const struct prototype_source_lowering_plan* plan
) {
	if (!plan || !plan->sealed || !plan->source) return -1;
	if (plan->source_fingerprint != source_fingerprint(plan->source)) return -1;
	if (plan->slot_count != plan->source->node_count +
		plan->source->type_expr_count) return -1;
	if (plan->first_type_expr_slot != plan->source->node_count) return -1;
	if (plan->order_count != plan->slot_count) return -1;
	if (plan->slot_count > 0 && (!plan->slots || !plan->order)) return -1;
	if (plan->edge_count > 0 && !plan->edges) return -1;
	if (plan->binder_count > 0 && !plan->binders) return -1;
	if (plan->scope_recipe_count > 0 && !plan->scope_recipes) return -1;
	if (plan->root_count > 0 && !plan->roots) return -1;
	uint32_t* positions = plan->slot_count == 0 ? NULL :
		malloc(plan->slot_count * sizeof(*positions));
	if (plan->slot_count > 0 && !positions) return -1;
	for (size_t i = 0; i < plan->slot_count; ++i) {
		positions[i] = SOURCE_LOWERING_INVALID_ID;
		const struct prototype_source_lowering_slot* slot = &plan->slots[i];
		if (slot->source_id >= (slot->kind == PROTOTYPE_SOURCE_LOWERING_TERM ?
				plan->source->node_count : plan->source->type_expr_count) ||
			(slot->kind != PROTOTYPE_SOURCE_LOWERING_TERM &&
			 slot->kind != PROTOTYPE_SOURCE_LOWERING_TYPE_EXPR) ||
			slot->first_edge > plan->edge_count || slot->edge_count >
				plan->edge_count - slot->first_edge ||
			slot->first_binder > plan->binder_count || slot->binder_count >
				plan->binder_count - slot->first_binder) {
			free(positions);
			return -1;
		}
		if ((slot->kind == PROTOTYPE_SOURCE_LOWERING_TYPE_EXPR &&
				slot->source_tag == PROTOTYPE_AST_TYPE_EXPR_SELF) !=
			(slot->owner_type_def_id != SOURCE_LOWERING_INVALID_ID) ||
			(slot->owner_type_def_id != SOURCE_LOWERING_INVALID_ID &&
				slot->owner_type_def_id >= plan->source->type_def_count)) {
			free(positions);
			return -1;
		}
	}
	for (uint32_t i = 0; i < plan->order_count; ++i) {
		uint32_t slot = plan->order[i];
		if (slot >= plan->slot_count || positions[slot] !=
				SOURCE_LOWERING_INVALID_ID) {
			free(positions);
			return -1;
		}
		positions[slot] = i;
	}
	for (size_t i = 0; i < plan->edge_count; ++i) {
		const struct prototype_source_lowering_edge* edge = &plan->edges[i];
		if (edge->owner_slot >= plan->slot_count ||
			edge->child_slot >= plan->slot_count ||
			(edge->scope_recipe != SOURCE_LOWERING_INVALID_ID &&
			 edge->scope_recipe >= plan->scope_recipe_count) ||
			positions[edge->child_slot] >= positions[edge->owner_slot]) {
			free(positions);
			return -1;
		}
	}
	unsigned char* scoped_binders = plan->binder_count == 0 ? NULL :
		calloc(plan->binder_count, 1);
	if (plan->binder_count > 0 && !scoped_binders) {
		free(positions);
		return -1;
	}
	for (uint32_t i = 0; i < plan->scope_recipe_count; ++i) {
		const struct prototype_source_lowering_scope_recipe* recipe =
			&plan->scope_recipes[i];
		if (recipe->binder_id >= plan->binder_count ||
			scoped_binders[recipe->binder_id] ||
			(recipe->parent_recipe != SOURCE_LOWERING_INVALID_ID &&
			 recipe->parent_recipe >= i)) {
			free(scoped_binders);
			free(positions);
			return -1;
		}
		scoped_binders[recipe->binder_id] = 1;
	}
	free(scoped_binders);
	for (uint32_t owner = 0; owner < plan->slot_count; ++owner) {
		const struct prototype_source_lowering_slot* slot = &plan->slots[owner];
		for (uint32_t i = 0; i < slot->edge_count; ++i) {
			if (plan->edges[slot->first_edge + i].owner_slot != owner) {
				free(positions);
				return -1;
			}
		}
		for (uint32_t i = 0; i < slot->binder_count; ++i) {
			const struct prototype_source_lowering_binder* binder =
				&plan->binders[slot->first_binder + i];
			if (binder->owner_slot != owner ||
				(binder->annotation_slot != SOURCE_LOWERING_INVALID_ID &&
				 binder->annotation_slot >= plan->slot_count)) {
				free(positions);
				return -1;
			}
		}
	}
	for (size_t i = 0; i < plan->root_count; ++i) {
		const struct prototype_source_lowering_root* root = &plan->roots[i];
		if ((root->source_item_kind == PROTOTYPE_SOURCE_SCHEDULE_IMPORT &&
				root->slot_id != SOURCE_LOWERING_INVALID_ID) ||
			(root->source_item_kind != PROTOTYPE_SOURCE_SCHEDULE_IMPORT &&
				root->slot_id >= plan->slot_count)) {
			free(positions);
			return -1;
		}
	}
	free(positions);
	return plan->topology_digest == plan_digest(plan) ? 0 : -1;
}

uint32_t prototype_source_lowering_plan_term_slot(
	const struct prototype_source_lowering_plan* plan,
	uint32_t ast_node_id
) {
	return plan && plan->sealed && ast_node_id < plan->first_type_expr_slot ?
		ast_node_id : SOURCE_LOWERING_INVALID_ID;
}

uint32_t prototype_source_lowering_plan_type_expr_slot(
	const struct prototype_source_lowering_plan* plan,
	uint32_t ast_type_expr_id
) {
	return plan && plan->sealed && ast_type_expr_id <
		plan->slot_count - plan->first_type_expr_slot ?
		plan->first_type_expr_slot + ast_type_expr_id :
		SOURCE_LOWERING_INVALID_ID;
}
