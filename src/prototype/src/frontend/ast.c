#include "a_program/frontend/ast.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../internal/ast_common.h"

void prototype_ast_db_init(
	struct prototype_ast_db* db,
	struct prototype_ast_node* nodes,
	size_t node_capacity,
	struct prototype_ast_type_expectation_def* expectations,
	size_t expectation_capacity,
	struct prototype_ast_term_assignment_def* assignments,
	size_t assignment_capacity,
	struct prototype_ast_import_def* imports,
	size_t import_capacity,
	struct prototype_ast_def_open_address_entry* def_index,
	size_t def_index_capacity,
	struct prototype_ast_match_case* cases,
	size_t case_capacity,
	struct prototype_ast_binder* case_binders,
	size_t case_binder_capacity,
	struct prototype_ast_computation_fold_clause* computation_fold_clauses,
	size_t computation_fold_clause_capacity,
	uint32_t* block_items,
	size_t block_item_capacity,
	uint32_t* definition_items,
	size_t definition_item_capacity,
	struct prototype_ast_type_expr* type_exprs,
	size_t type_expr_capacity,
	struct prototype_ast_type_def* type_defs,
	size_t type_def_capacity,
	struct prototype_ast_family_binder* family_binders,
	size_t family_binder_capacity,
	struct prototype_ast_type_constructor* type_constructors,
	size_t type_constructor_capacity,
	uint32_t* type_field_exprs,
	uint32_t* type_field_binder_ids,
	int* type_field_name_symbol_ids,
	size_t type_field_expr_capacity
) {
	memset(db, 0, sizeof(*db));
	db->nodes = nodes;
	db->node_capacity = node_capacity;
	db->expectations = expectations;
	db->expectation_capacity = expectation_capacity;
	db->assignments = assignments;
	db->assignment_capacity = assignment_capacity;
	db->imports = imports;
	db->import_capacity = import_capacity;
	db->def_index = def_index;
	db->def_index_capacity = def_index_capacity;
	db->cases = cases;
	db->case_capacity = case_capacity;
	db->case_binders = case_binders;
	db->case_binder_capacity = case_binder_capacity;
	db->computation_fold_clauses = computation_fold_clauses;
	db->computation_fold_clause_capacity = computation_fold_clause_capacity;
	db->block_items = block_items;
	db->block_item_capacity = block_item_capacity;
	db->definition_items = definition_items;
	db->definition_item_capacity = definition_item_capacity;
	db->root_definition_block = PROTOTYPE_INVALID_ID;
	db->root_definition_select = PROTOTYPE_INVALID_ID;
	db->type_exprs = type_exprs;
	db->type_expr_capacity = type_expr_capacity;
	db->type_defs = type_defs;
	db->type_def_capacity = type_def_capacity;
	db->family_binders = family_binders;
	db->family_binder_capacity = family_binder_capacity;
	db->type_constructors = type_constructors;
	db->type_constructor_capacity = type_constructor_capacity;
	db->type_field_exprs = type_field_exprs;
	db->type_field_binder_ids = type_field_binder_ids;
	db->type_field_name_symbol_ids = type_field_name_symbol_ids;
	db->type_field_expr_capacity = type_field_expr_capacity;
}

static int add_node(struct prototype_ast_db* db, struct prototype_ast_node node, uint32_t* p_ret) {
	if (!db || !p_ret || reserve_slot(db->node_count, db->node_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)db->node_count;
	db->nodes[id] = node;
	db->node_count++;
	*p_ret = id;
	return 0;
}

uint32_t prototype_ast_new_binder(struct prototype_ast_db* db) {
	if (!db) {
		return PROTOTYPE_INVALID_ID;
	}
	return db->next_ast_binder_id++;
}

static int add_type_expr(
	struct prototype_ast_db* db,
	struct prototype_ast_type_expr expr,
	uint32_t* p_ret
) {
	if (!db || !p_ret || reserve_slot(db->type_expr_count, db->type_expr_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)db->type_expr_count;
	db->type_exprs[id] = expr;
	db->type_expr_count++;
	*p_ret = id;
	return 0;
}

int prototype_ast_type_expr_universe(
	struct prototype_ast_db* db,
	uint32_t level,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_UNIVERSE;
	expr.span = span;
	expr.as.universe.level = level;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_fresh_universe(
	struct prototype_ast_db* db,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db) {
		return -1;
	}

	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR;
	expr.span = span;
	expr.as.universe_var.level_var = db->next_ast_level_var++;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_self(
	struct prototype_ast_db* db,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_SELF;
	expr.span = span;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_var(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_VAR;
	expr.span = span;
	expr.as.var.ast_binder_id = ast_binder_id;
	expr.as.var.symbol_id = symbol_id;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_name(
	struct prototype_ast_db* db,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_NAME;
	expr.span = span;
	expr.as.name.symbol_id = symbol_id;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_name_in_namespace(
	struct prototype_ast_db* db,
	int namespace_symbol_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_type_expr expr;
	if (!db || !p_ret || namespace_symbol_id < 0 || symbol_id < 0) {
		return -1;
	}
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_NAME_IN_NAMESPACE;
	expr.span = span;
	expr.as.name_in_namespace.namespace_symbol_id = namespace_symbol_id;
	expr.as.name_in_namespace.symbol_id = symbol_id;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_host_type(
	struct prototype_ast_db* db,
	int host_type_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!prototype_term_host_type_debug_name(host_type_id)) {
		return -1;
	}
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE;
	expr.span = span;
	expr.as.host_type.host_type_id = host_type_id;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_app(
	struct prototype_ast_db* db,
	uint32_t function,
	uint32_t argument,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || function >= db->type_expr_count || argument >= db->type_expr_count) {
		return -1;
	}

	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_APP;
	expr.span = span;
	expr.as.app.function = function;
	expr.as.app.argument = argument;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_arrow(
	struct prototype_ast_db* db,
	uint32_t domain,
	uint32_t codomain,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || domain >= db->type_expr_count || codomain >= db->type_expr_count) {
		return -1;
	}

	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_ARROW;
	expr.span = span;
	expr.as.arrow.domain = domain;
	expr.as.arrow.codomain = codomain;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_pi(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	uint32_t domain,
	uint32_t codomain,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || ast_binder_id == PROTOTYPE_INVALID_ID || symbol_id < 0 ||
		domain >= db->type_expr_count || codomain >= db->type_expr_count) {
		return -1;
	}

	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_PI;
	expr.span = span;
	expr.as.pi.ast_binder_id = ast_binder_id;
	expr.as.pi.symbol_id = symbol_id;
	expr.as.pi.domain = domain;
	expr.as.pi.codomain = codomain;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_computation_reference(
	struct prototype_ast_db* db,
	uint32_t result,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || result >= db->type_expr_count) {
		return -1;
	}

	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE;
	expr.span = span;
	expr.as.computation_reference.result = result;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_returns(
	struct prototype_ast_db* db,
	uint32_t computation,
	uint32_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || computation >= db->node_count || value >= db->node_count) {
		return -1;
	}
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_RETURNS;
	expr.span = span;
	expr.as.returns.computation = computation;
	expr.as.returns.value = value;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_expr_terminates(
	struct prototype_ast_db* db,
	uint32_t computation,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || computation >= db->node_count) {
		return -1;
	}
	struct prototype_ast_type_expr expr;
	memset(&expr, 0, sizeof(expr));
	expr.tag = PROTOTYPE_AST_TYPE_EXPR_TERMINATES;
	expr.span = span;
	expr.as.terminates.computation = computation;
	return add_type_expr(db, expr, p_ret);
}

int prototype_ast_type_add(
	struct prototype_ast_db* db,
	int name_symbol_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_type_def_id
) {
	if (!db || !p_type_def_id || reserve_slot(db->type_def_count, db->type_def_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)db->type_def_count;
	struct prototype_ast_type_def* type = &db->type_defs[id];
	memset(type, 0, sizeof(*type));
	type->name_symbol_id = name_symbol_id;
	type->name_span = name_span;
	type->body_span = body_span;
	type->first_family_binder = (uint32_t)db->family_binder_count;
	type->first_constructor = (uint32_t)db->type_constructor_count;
	type->compiled_type = PROTOTYPE_INVALID_ID;
	db->type_def_count++;
	*p_type_def_id = id;
	return 0;
}

int prototype_ast_type_add_family_binder(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	uint32_t ast_binder_id,
	int name_symbol_id,
	uint32_t type_expr,
	int role,
	struct prototype_source_span span
) {
	if (!db || ast_type_def_id >= db->type_def_count ||
		type_expr >= db->type_expr_count ||
		(role != PROTOTYPE_AST_FAMILY_BINDER_PARAMETER &&
		 role != PROTOTYPE_AST_FAMILY_BINDER_INDEX)) {
		return -1;
	}
	if (reserve_slot(db->family_binder_count, db->family_binder_capacity) != 0) {
		return -1;
	}

	struct prototype_ast_type_def* type = &db->type_defs[ast_type_def_id];
	if ((uint32_t)db->family_binder_count != type->first_family_binder +
			type->parameter_count + type->index_count ||
		(role == PROTOTYPE_AST_FAMILY_BINDER_PARAMETER && type->index_count != 0)) {
		return -1;
	}

	uint32_t id = (uint32_t)db->family_binder_count;
	db->family_binders[id].ast_binder_id = ast_binder_id;
	db->family_binders[id].name_symbol_id = name_symbol_id;
	db->family_binders[id].type_expr = type_expr;
	db->family_binders[id].role = role;
	db->family_binders[id].span = span;
	db->family_binder_count++;
	if (role == PROTOTYPE_AST_FAMILY_BINDER_PARAMETER) {
		type->parameter_count++;
	} else {
		type->index_count++;
	}
	return 0;
}

int prototype_ast_type_add_constructor(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	int name_symbol_id,
	struct prototype_source_span name_span,
	const uint32_t* field_type_exprs,
	const uint32_t* field_binder_ids,
	const int* field_name_symbol_ids,
	uint32_t field_count,
	uint32_t result_type_expr
) {
	if (!db || ast_type_def_id >= db->type_def_count || result_type_expr >= db->type_expr_count) {
		return -1;
	}
	if (field_count > 0 && !field_type_exprs) {
		return -1;
	}
	if (reserve_slot(db->type_constructor_count, db->type_constructor_capacity) != 0) {
		return -1;
	}
	if (db->type_field_expr_count + field_count > db->type_field_expr_capacity) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		if (field_type_exprs[i] >= db->type_expr_count) {
			return -1;
		}
	}

	struct prototype_ast_type_def* type = &db->type_defs[ast_type_def_id];
	if ((uint32_t)db->type_constructor_count != type->first_constructor + type->constructor_count) {
		return -1;
	}

	uint32_t id = (uint32_t)db->type_constructor_count;
	struct prototype_ast_type_constructor* constructor = &db->type_constructors[id];
	memset(constructor, 0, sizeof(*constructor));
	constructor->name_symbol_id = name_symbol_id;
	constructor->name_span = name_span;
	constructor->first_field_type = (uint32_t)db->type_field_expr_count;
	constructor->field_count = field_count;
	constructor->result_type = result_type_expr;
	for (uint32_t i = 0; i < field_count; ++i) {
		uint32_t field_id = (uint32_t)db->type_field_expr_count++;
		db->type_field_exprs[field_id] = field_type_exprs[i];
		if (db->type_field_binder_ids) {
			db->type_field_binder_ids[field_id] =
				field_binder_ids ? field_binder_ids[i] : PROTOTYPE_INVALID_ID;
		}
		if (db->type_field_name_symbol_ids) {
			db->type_field_name_symbol_ids[field_id] =
				field_name_symbol_ids ? field_name_symbol_ids[i] : -1;
		}
	}
	db->type_constructor_count++;
	type->constructor_count++;
	return 0;
}

int prototype_ast_var(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_VAR;
	node.span = span;
	node.as.var.ast_binder_id = ast_binder_id;
	node.as.var.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_name(
	struct prototype_ast_db* db,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_NAME;
	node.span = span;
	node.as.name.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_name_in_namespace(
	struct prototype_ast_db* db,
	int namespace_symbol_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_NAME_IN_NAMESPACE;
	node.span = span;
	node.as.name_in_namespace.namespace_symbol_id = namespace_symbol_id;
	node.as.name_in_namespace.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_name_in_ast_namespace(
	struct prototype_ast_db* db,
	uint32_t namespace_ast,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || namespace_ast >= db->node_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_NAME_IN_AST_NAMESPACE;
	node.span = span;
	node.as.name_in_ast_namespace.namespace_ast = namespace_ast;
	node.as.name_in_ast_namespace.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_app(
	struct prototype_ast_db* db,
	uint32_t function,
	uint32_t argument,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || function >= db->node_count || argument >= db->node_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_APP;
	node.span = span;
	node.as.app.function = function;
	node.as.app.argument = argument;
	return add_node(db, node, p_ret);
}

int prototype_ast_lambda(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int binder_symbol_id,
	uint32_t binder_type,
	uint32_t body,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || body >= db->node_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_LAMBDA;
	node.span = span;
	node.as.lambda.ast_binder_id = ast_binder_id;
	node.as.lambda.binder_symbol_id = binder_symbol_id;
	node.as.lambda.binder_type = binder_type;
	node.as.lambda.body = body;
	return add_node(db, node, p_ret);
}

int prototype_ast_match(
	struct prototype_ast_db* db,
	uint32_t scrutinee,
	const struct prototype_ast_match_case_input* cases,
	uint32_t case_count,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || !cases || !p_ret || scrutinee >= db->node_count) {
		return -1;
	}
	if (db->case_count + case_count > db->case_capacity) {
		return -1;
	}

	size_t needed_binders = 0;
	for (uint32_t i = 0; i < case_count; ++i) {
		if (cases[i].body >= db->node_count) {
			return -1;
		}
		if (cases[i].binder_count > 0 && !cases[i].binders) {
			return -1;
		}
		needed_binders += cases[i].binder_count;
	}
	if (db->case_binder_count + needed_binders > db->case_binder_capacity) {
		return -1;
	}

	uint32_t first_case = (uint32_t)db->case_count;
	for (uint32_t i = 0; i < case_count; ++i) {
		struct prototype_ast_match_case* stored_case = &db->cases[db->case_count++];
		stored_case->constructor_symbol_id = cases[i].constructor_symbol_id;
		stored_case->first_binder = (uint32_t)db->case_binder_count;
		stored_case->binder_count = cases[i].binder_count;
		stored_case->body = cases[i].body;
		stored_case->span = cases[i].span;
		for (uint32_t j = 0; j < cases[i].binder_count; ++j) {
			db->case_binders[db->case_binder_count++] = cases[i].binders[j];
		}
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_MATCH;
	node.span = span;
	node.as.match.scrutinee = scrutinee;
	node.as.match.first_case = first_case;
	node.as.match.case_count = case_count;
	return add_node(db, node, p_ret);
}

int prototype_ast_type_literal(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || ast_type_def_id >= db->type_def_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_TYPE_LITERAL;
	node.span = span;
	node.as.type_literal.ast_type_def_id = ast_type_def_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_type_formation(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || !p_ret || ast_type_def_id >= db->type_def_count) {
		return -1;
	}

	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_TYPE_FORMATION;
	node.span = span;
	node.as.type_formation.ast_type_def_id = ast_type_def_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_induction_hypothesis(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_INDUCTION_HYPOTHESIS;
	node.span = span;
	node.as.induction_hypothesis.ast_binder_id = ast_binder_id;
	node.as.induction_hypothesis.symbol_id = symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_text_literal(
	struct prototype_ast_db* db,
	int text_symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_TEXT_LITERAL;
	node.span = span;
	node.as.text_literal.text_symbol_id = text_symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_int_literal(
	struct prototype_ast_db* db,
	int64_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_INT_LITERAL;
	node.span = span;
	node.as.int_literal.value = value;
	return add_node(db, node, p_ret);
}

int prototype_ast_system_name(
	struct prototype_ast_db* db,
	int namespace_symbol_id,
	int symbol_id,
	int type_symbol_id,
	int kind,
	int host_type_id,
	int pure_primitive_id,
	int effect_operation_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_SYSTEM_NAME;
	node.span = span;
	node.as.system_name.namespace_symbol_id = namespace_symbol_id;
	node.as.system_name.symbol_id = symbol_id;
	node.as.system_name.type_symbol_id = type_symbol_id;
	node.as.system_name.kind = kind;
	node.as.system_name.host_type_id = host_type_id;
	node.as.system_name.pure_primitive_id = pure_primitive_id;
	node.as.system_name.effect_operation_id = effect_operation_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_ascription(
	struct prototype_ast_db* db,
	uint32_t term,
	uint32_t type_expr,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || term >= db->node_count || type_expr >= db->type_expr_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_ASCRIPTION;
	node.span = span;
	node.as.ascription.term = term;
	node.as.ascription.type_expr = type_expr;
	return add_node(db, node, p_ret);
}

static int prototype_ast_unary(
	struct prototype_ast_db* db,
	int tag,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || term >= db->node_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = tag;
	node.span = span;
	node.as.unary.term = term;
	return add_node(db, node, p_ret);
}

int prototype_ast_quote(
	struct prototype_ast_db* db,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	return prototype_ast_unary(db, PROTOTYPE_AST_QUOTE, term, span, p_ret);
}

int prototype_ast_returns_witness(
	struct prototype_ast_db* db,
	uint32_t computation,
	uint32_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || computation >= db->node_count || value >= db->node_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_RETURNS_WITNESS;
	node.span = span;
	node.as.returns_witness.computation = computation;
	node.as.returns_witness.value = value;
	return add_node(db, node, p_ret);
}

int prototype_ast_terminates_witness(
	struct prototype_ast_db* db,
	uint32_t computation,
	uint32_t returns_witness,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || computation >= db->node_count || returns_witness >= db->node_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_TERMINATES_WITNESS;
	node.span = span;
	node.as.terminates_witness.computation = computation;
	node.as.terminates_witness.returns_witness = returns_witness;
	return add_node(db, node, p_ret);
}

int prototype_ast_definition_block(
	struct prototype_ast_db* db,
	const uint32_t* assignments,
	uint32_t assignment_count,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || !assignments || assignment_count == 0 ||
		db->definition_item_count + assignment_count >
			db->definition_item_capacity) {
		return -1;
	}
	uint32_t first_assignment = (uint32_t)db->definition_item_count;
	for (uint32_t i = 0; i < assignment_count; ++i) {
		if (assignments[i] >= db->assignment_count) {
			db->definition_item_count = first_assignment;
			return -1;
		}
		db->definition_items[db->definition_item_count++] = assignments[i];
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_DEFINITION_BLOCK;
	node.span = span;
	node.as.definition_block.first_assignment = first_assignment;
	node.as.definition_block.assignment_count = assignment_count;
	if (add_node(db, node, p_ret) != 0) {
		db->definition_item_count = first_assignment;
		return -1;
	}
	return 0;
}

int prototype_ast_definition_select(
	struct prototype_ast_db* db,
	uint32_t definition_block,
	int name_symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || definition_block >= db->node_count || name_symbol_id < 0 ||
		db->nodes[definition_block].tag != PROTOTYPE_AST_DEFINITION_BLOCK) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_DEFINITION_SELECT;
	node.span = span;
	node.as.definition_select.definition_block = definition_block;
	node.as.definition_select.name_symbol_id = name_symbol_id;
	return add_node(db, node, p_ret);
}

int prototype_ast_computation_block(
	struct prototype_ast_db* db,
	const uint32_t* items,
	uint32_t item_count,
	uint32_t result_item_index,
	int result_mode,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || !items || item_count == 0 || result_item_index >= item_count ||
		(result_mode != PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM &&
		 result_mode != PROTOTYPE_AST_BLOCK_RESULT_SELECTED_BINDING) ||
		db->block_item_count + item_count > db->block_item_capacity) {
		return -1;
	}
	for (uint32_t i = 0; i < item_count; ++i) {
		if (items[i] >= db->node_count) {
			return -1;
		}
	}
	uint32_t first_item = (uint32_t)db->block_item_count;
	for (uint32_t i = 0; i < item_count; ++i) {
		db->block_items[db->block_item_count++] = items[i];
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_COMPUTATION_BLOCK;
	node.span = span;
	node.as.block.first_item = first_item;
	node.as.block.item_count = item_count;
	node.as.block.result_item_index = result_item_index;
	node.as.block.result_mode = result_mode;
	if (add_node(db, node, p_ret) != 0) {
		db->block_item_count = first_item;
		return -1;
	}
	return 0;
}

int prototype_ast_block_binding(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int binder_symbol_id,
	uint32_t binder_type,
	uint32_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || value >= db->node_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_BLOCK_BINDING;
	node.span = span;
	node.as.block_binding.ast_binder_id = ast_binder_id;
	node.as.block_binding.binder_symbol_id = binder_symbol_id;
	node.as.block_binding.binder_type = binder_type;
	node.as.block_binding.value = value;
	return add_node(db, node, p_ret);
}

int prototype_ast_block_expression(
	struct prototype_ast_db* db,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || term >= db->node_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_BLOCK_EXPRESSION;
	node.span = span;
	node.as.block_expression.term = term;
	return add_node(db, node, p_ret);
}

int prototype_ast_block_lambda_exit(
	struct prototype_ast_db* db,
	uint32_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || value >= db->node_count) {
		return -1;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_BLOCK_LAMBDA_EXIT;
	node.span = span;
	node.as.block_lambda_exit.value = value;
	return add_node(db, node, p_ret);
}

int prototype_ast_computation_fold(
	struct prototype_ast_db* db,
	uint32_t computation,
	const struct prototype_ast_computation_fold_clause_input* clauses,
	uint32_t clause_count,
	uint32_t return_binder_id,
	int return_symbol_id,
	uint32_t return_body,
	struct prototype_source_span span,
	uint32_t* p_ret
) {
	if (!db || computation >= db->node_count || return_body >= db->node_count ||
		(clause_count != 0 && !clauses) ||
		db->computation_fold_clause_count + clause_count >
			db->computation_fold_clause_capacity) {
		return -1;
	}
	for (uint32_t i = 0; i < clause_count; ++i) {
		if (clauses[i].operation >= db->node_count ||
			clauses[i].body >= db->node_count) {
			return -1;
		}
	}
	uint32_t first_clause = (uint32_t)db->computation_fold_clause_count;
	for (uint32_t i = 0; i < clause_count; ++i) {
		struct prototype_ast_computation_fold_clause* stored =
			&db->computation_fold_clauses[db->computation_fold_clause_count++];
		stored->operation = clauses[i].operation;
		stored->operation_argument_binder_id =
			clauses[i].operation_argument_binder_id;
		stored->operation_argument_symbol_id =
			clauses[i].operation_argument_symbol_id;
		stored->operation_continuation_binder_id =
			clauses[i].operation_continuation_binder_id;
		stored->operation_continuation_symbol_id =
			clauses[i].operation_continuation_symbol_id;
		stored->body = clauses[i].body;
		stored->span = clauses[i].span;
	}
	struct prototype_ast_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_AST_COMPUTATION_FOLD;
	node.span = span;
	node.as.computation_fold.computation = computation;
	node.as.computation_fold.first_clause = first_clause;
	node.as.computation_fold.clause_count = clause_count;
	node.as.computation_fold.return_binder_id = return_binder_id;
	node.as.computation_fold.return_symbol_id = return_symbol_id;
	node.as.computation_fold.return_body = return_body;
	if (add_node(db, node, p_ret) != 0) {
		db->computation_fold_clause_count = first_clause;
		return -1;
	}
	return 0;
}

uint32_t prototype_ast_new_source_entry(struct prototype_ast_db* db) {
	if (!db) {
		return PROTOTYPE_INVALID_ID;
	}
	return db->next_source_entry_id++;
}

static size_t def_index_hash(int symbol_id) {
	uint32_t x = (uint32_t)symbol_id;
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return (size_t)x;
}

static struct prototype_ast_def_open_address_entry* find_def_index_entry(
	struct prototype_ast_db* db,
	int symbol_id
) {
	if (!db || !db->def_index || db->def_index_capacity == 0) {
		return NULL;
	}
	size_t start = def_index_hash(symbol_id) % db->def_index_capacity;
	for (size_t probe = 0; probe < db->def_index_capacity; ++probe) {
		size_t index = (start + probe) % db->def_index_capacity;
		struct prototype_ast_def_open_address_entry* entry = &db->def_index[index];
		if (!entry->occupied) {
			return NULL;
		}
		if (entry->symbol_id == symbol_id) {
			return entry;
		}
	}
	return NULL;
}

static struct prototype_ast_def_open_address_entry* find_or_add_def_index_entry(
	struct prototype_ast_db* db,
	int symbol_id
) {
	struct prototype_ast_def_open_address_entry* existing = find_def_index_entry(db, symbol_id);
	if (existing) {
		return existing;
	}
	if (!db || !db->def_index || reserve_slot(db->def_index_count, db->def_index_capacity) != 0) {
		return NULL;
	}
	size_t start = def_index_hash(symbol_id) % db->def_index_capacity;
	for (size_t probe = 0; probe < db->def_index_capacity; ++probe) {
		size_t index = (start + probe) % db->def_index_capacity;
		struct prototype_ast_def_open_address_entry* entry = &db->def_index[index];
		if (entry->occupied) {
			continue;
		}
		memset(entry, 0, sizeof(*entry));
		entry->occupied = 1;
		entry->symbol_id = symbol_id;
		entry->first_expectation = PROTOTYPE_INVALID_ID;
		entry->first_assignment = PROTOTYPE_INVALID_ID;
		db->def_index_count++;
		return entry;
	}
	return NULL;
}

int prototype_ast_add_type_expectation(
	struct prototype_ast_db* db,
	int kind,
	int name_symbol_id,
	uint32_t type_expr,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span type_span,
	uint32_t paired_assignment_id,
	uint32_t* p_ret
) {
	if (!db || !p_ret || type_expr >= db->type_expr_count ||
		reserve_slot(db->expectation_count, db->expectation_capacity) != 0) {
		return -1;
	}
	if (kind != PROTOTYPE_AST_TYPE_ENTRY_DECLARATION &&
		kind != PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION) {
		return -1;
	}
	struct prototype_ast_def_open_address_entry* symbol = find_or_add_def_index_entry(db, name_symbol_id);
	if (!symbol) {
		return -1;
	}

	uint32_t id = (uint32_t)db->expectation_count;
	memset(&db->expectations[id], 0, sizeof(db->expectations[id]));
	db->expectations[id].kind = kind;
	db->expectations[id].name_symbol_id = name_symbol_id;
	db->expectations[id].type_expr = type_expr;
	db->expectations[id].source_entry_id = source_entry_id;
	db->expectations[id].name_span = name_span;
	db->expectations[id].type_span = type_span;
	db->expectations[id].paired_assignment_id = paired_assignment_id;
	db->expectations[id].next_for_symbol = symbol->first_expectation;
	symbol->first_expectation = id;
	symbol->expectation_count++;
	db->expectation_count++;
	*p_ret = id;
	return 0;
}

int prototype_ast_add_term_assignment(
	struct prototype_ast_db* db,
	int name_symbol_id,
	uint32_t ast,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_ret
) {
	if (!db || !p_ret || ast >= db->node_count ||
		reserve_slot(db->assignment_count, db->assignment_capacity) != 0) {
		return -1;
	}
	struct prototype_ast_def_open_address_entry* symbol = find_or_add_def_index_entry(db, name_symbol_id);
	if (!symbol) {
		return -1;
	}

	uint32_t id = (uint32_t)db->assignment_count;
	memset(&db->assignments[id], 0, sizeof(db->assignments[id]));
	db->assignments[id].name_symbol_id = name_symbol_id;
	db->assignments[id].ast = ast;
	db->assignments[id].source_entry_id = source_entry_id;
	db->assignments[id].name_span = name_span;
	db->assignments[id].body_span = body_span;
	db->assignments[id].next_for_symbol = symbol->first_assignment;
	db->assignments[id].compiled_term = PROTOTYPE_INVALID_ID;
	db->assignments[id].compiled_classifier = PROTOTYPE_INVALID_ID;
	symbol->first_assignment = id;
	symbol->assignment_count++;
	db->assignment_count++;
	*p_ret = id;
	return 0;
}

int prototype_ast_add_import(
	struct prototype_ast_db* db,
	int name_symbol_id,
	uint32_t source_entry_id,
	struct prototype_source_span name_span
) {
	if (!db || name_symbol_id < 0 ||
		reserve_slot(db->import_count, db->import_capacity) != 0) {
		return -1;
	}
	for (size_t i = 0; i < db->import_count; ++i) {
		if (db->imports[i].name_symbol_id == name_symbol_id) {
			return 0;
		}
	}
	uint32_t id = (uint32_t)db->import_count;
	memset(&db->imports[id], 0, sizeof(db->imports[id]));
	db->imports[id].name_symbol_id = name_symbol_id;
	db->imports[id].source_entry_id = source_entry_id;
	db->imports[id].name_span = name_span;
	db->import_count++;
	return 0;
}

int prototype_ast_pair_type_expectation(
	struct prototype_ast_db* db,
	uint32_t expectation_id,
	uint32_t assignment_id
) {
	if (!db || expectation_id >= db->expectation_count || assignment_id >= db->assignment_count) {
		return -1;
	}

	struct prototype_ast_type_expectation_def* expectation = &db->expectations[expectation_id];
	struct prototype_ast_term_assignment_def* assignment = &db->assignments[assignment_id];
	if (expectation->name_symbol_id != assignment->name_symbol_id ||
		expectation->source_entry_id != assignment->source_entry_id) {
		return -1;
	}
	expectation->paired_assignment_id = assignment_id;
	return 0;
}

const struct prototype_ast_term_assignment_def* prototype_ast_lookup_assignment_const(
	const struct prototype_ast_db* db,
	int name_symbol_id
) {
	if (!db) {
		return NULL;
	}

	for (size_t i = 0; i < db->def_index_capacity; ++i) {
		const struct prototype_ast_def_open_address_entry* entry = &db->def_index[i];
		if (!entry->occupied || entry->symbol_id != name_symbol_id) {
			continue;
		}
		if (entry->assignment_count != 1 || entry->first_assignment >= db->assignment_count) {
			return NULL;
		}
		return &db->assignments[entry->first_assignment];
	}
	return NULL;
}
