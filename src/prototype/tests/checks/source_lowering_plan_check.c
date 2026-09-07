#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "a_program/frontend/ast.h"
#include "a_program/frontend/source_lowering_plan.h"
#include "a_program/frontend/source_schedule.h"

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

static int find_binder_role(
	const struct prototype_source_lowering_plan* plan,
	uint32_t owner_slot,
	int role
) {
	const struct prototype_source_lowering_slot* slot = &plan->slots[owner_slot];
	for (uint32_t i = 0; i < slot->binder_count; ++i) {
		if (plan->binders[slot->first_binder + i].role == role) return 1;
	}
	return 0;
}

static uint32_t find_binder(
	const struct prototype_source_lowering_plan* plan,
	uint32_t owner_slot,
	int role,
	uint32_t ordinal
) {
	const struct prototype_source_lowering_slot* slot = &plan->slots[owner_slot];
	for (uint32_t i = 0; i < slot->binder_count; ++i) {
		uint32_t binder_id = slot->first_binder + i;
		if (plan->binders[binder_id].role == role &&
			plan->binders[binder_id].ordinal == ordinal) return binder_id;
	}
	return UINT32_MAX;
}

static const struct prototype_source_lowering_edge* find_edge(
	const struct prototype_source_lowering_plan* plan,
	uint32_t owner_slot,
	int role,
	uint32_t ordinal
) {
	const struct prototype_source_lowering_slot* slot = &plan->slots[owner_slot];
	for (uint32_t i = 0; i < slot->edge_count; ++i) {
		const struct prototype_source_lowering_edge* edge =
			&plan->edges[slot->first_edge + i];
		if (edge->role == role && edge->ordinal == ordinal) return edge;
	}
	return NULL;
}

static int recipe_is(
	const struct prototype_source_lowering_plan* plan,
	uint32_t recipe_id,
	uint32_t binder_id,
	uint32_t parent_recipe
) {
	return recipe_id < plan->scope_recipe_count &&
		plan->scope_recipes[recipe_id].binder_id == binder_id &&
		plan->scope_recipes[recipe_id].parent_recipe == parent_recipe;
}

int main(void) {
	struct prototype_ast_node nodes[11];
	struct prototype_ast_type_expr type_exprs[3];
	struct prototype_ast_match_case cases[2];
	struct prototype_ast_binder case_binders[1];
	struct prototype_ast_match_selector match_selectors[1];
	struct prototype_ast_computation_fold_clause fold_clauses[1];
	uint32_t block_items[2] = { 6, 7 };
	struct prototype_ast_type_def type_defs[1];
	struct prototype_ast_family_binder family_binders[1];
	struct prototype_ast_type_constructor constructors[1];
	uint32_t field_exprs[1] = { 2 };
	uint32_t field_binder_ids[1] = { 201 };
	int field_symbols[1] = { 21 };
	struct prototype_ast_type_expectation_def expectations[1];
	struct prototype_ast_term_assignment_def assignments[2];
	memset(nodes, 0, sizeof(nodes));
	memset(type_exprs, 0, sizeof(type_exprs));
	memset(cases, 0, sizeof(cases));
	memset(case_binders, 0, sizeof(case_binders));
	memset(match_selectors, 0, sizeof(match_selectors));
	memset(fold_clauses, 0, sizeof(fold_clauses));
	memset(type_defs, 0, sizeof(type_defs));
	memset(family_binders, 0, sizeof(family_binders));
	memset(constructors, 0, sizeof(constructors));
	memset(expectations, 0, sizeof(expectations));
	memset(assignments, 0, sizeof(assignments));

	type_exprs[0].tag = PROTOTYPE_AST_TYPE_EXPR_UNIVERSE;
	type_exprs[1].tag = PROTOTYPE_AST_TYPE_EXPR_PI;
	type_exprs[1].as.pi.ast_binder_id = 100;
	type_exprs[1].as.pi.symbol_id = 10;
	type_exprs[1].as.pi.domain = 0;
	type_exprs[1].as.pi.codomain = 0;
	type_exprs[2].tag = PROTOTYPE_AST_TYPE_EXPR_SELF;

	nodes[0].tag = PROTOTYPE_AST_VAR;
	nodes[0].as.var.ast_binder_id = 100;
	nodes[1].tag = PROTOTYPE_AST_LAMBDA;
	nodes[1].as.lambda.ast_binder_id = 100;
	nodes[1].as.lambda.binder_symbol_id = 10;
	nodes[1].as.lambda.binder_type = 0;
	nodes[1].as.lambda.body = 0;
	nodes[2].tag = PROTOTYPE_AST_INT_LITERAL;
	nodes[2].as.int_literal.value = 7;
	nodes[3].tag = PROTOTYPE_AST_APP;
	nodes[3].as.app.function = 1;
	nodes[3].as.app.argument = 2;
	nodes[4].tag = PROTOTYPE_AST_MATCH;
	nodes[4].as.match.scrutinee = 0;
	nodes[4].as.match.first_case = 0;
	nodes[4].as.match.case_count = 2;
	cases[0].constructor_symbol_id = 30;
	cases[0].first_binder = 0;
	cases[0].binder_count = 1;
	cases[0].body = 2;
	case_binders[0].ast_binder_id = 101;
	case_binders[0].symbol_id = 11;
	cases[1].constructor_symbol_id = 31;
	cases[1].first_selector = 0;
	cases[1].selector_count = 1;
	cases[1].body = 2;
	match_selectors[0].source_symbol_id = 32;
	match_selectors[0].local_symbol_id = 33;
	match_selectors[0].value_ast_binder_id = 106;
	match_selectors[0].graph_ast_binder_id = 107;
	nodes[5].tag = PROTOTYPE_AST_ASCRIPTION;
	nodes[5].as.ascription.term = 3;
	nodes[5].as.ascription.type_expr = 1;
	nodes[6].tag = PROTOTYPE_AST_BLOCK_BINDING;
	nodes[6].as.block_binding.ast_binder_id = 102;
	nodes[6].as.block_binding.binder_symbol_id = 12;
	nodes[6].as.block_binding.binder_type = 0;
	nodes[6].as.block_binding.value = 2;
	nodes[7].tag = PROTOTYPE_AST_BLOCK_EXPRESSION;
	nodes[7].as.block_expression.term = 3;
	nodes[8].tag = PROTOTYPE_AST_COMPUTATION_BLOCK;
	nodes[8].as.block.first_item = 0;
	nodes[8].as.block.item_count = 2;
	nodes[9].tag = PROTOTYPE_AST_COMPUTATION_FOLD;
	nodes[9].as.computation_fold.computation = 8;
	nodes[9].as.computation_fold.first_clause = 0;
	nodes[9].as.computation_fold.clause_count = 1;
	nodes[9].as.computation_fold.return_binder_id = 103;
	nodes[9].as.computation_fold.return_symbol_id = 13;
	nodes[9].as.computation_fold.return_body = 2;
	fold_clauses[0].operation = 2;
	fold_clauses[0].operation_argument_binder_id = 104;
	fold_clauses[0].operation_argument_symbol_id = 14;
	fold_clauses[0].operation_continuation_binder_id = 105;
	fold_clauses[0].operation_continuation_symbol_id = 15;
	fold_clauses[0].body = 3;
	nodes[10].tag = PROTOTYPE_AST_TYPE_FORMATION;
	nodes[10].as.type_formation.ast_type_def_id = 0;

	type_defs[0].first_family_binder = 0;
	type_defs[0].name_symbol_id = 22;
	type_defs[0].parameter_count = 1;
	type_defs[0].first_constructor = 0;
	type_defs[0].constructor_count = 1;
	family_binders[0].ast_binder_id = 200;
	family_binders[0].name_symbol_id = 20;
	family_binders[0].type_expr = 0;
	constructors[0].first_field_type = 0;
	constructors[0].field_count = 1;
	constructors[0].result_type = 2;

	expectations[0].source_entry_id = 1;
	expectations[0].type_expr = 1;
	assignments[0].source_entry_id = 3;
	assignments[0].ast = 5;
	assignments[1].source_entry_id = 2;
	assignments[1].ast = 10;

	struct prototype_ast_db asts;
	memset(&asts, 0, sizeof(asts));
	asts.nodes = nodes;
	asts.node_count = ARRAY_COUNT(nodes);
	asts.type_exprs = type_exprs;
	asts.type_expr_count = ARRAY_COUNT(type_exprs);
	asts.cases = cases;
	asts.case_count = ARRAY_COUNT(cases);
	asts.case_binders = case_binders;
	asts.case_binder_count = ARRAY_COUNT(case_binders);
	asts.match_selectors = match_selectors;
	asts.match_selector_count = ARRAY_COUNT(match_selectors);
	asts.match_selector_capacity = ARRAY_COUNT(match_selectors);
	asts.computation_fold_clauses = fold_clauses;
	asts.computation_fold_clause_count = ARRAY_COUNT(fold_clauses);
	asts.block_items = block_items;
	asts.block_item_count = ARRAY_COUNT(block_items);
	asts.type_defs = type_defs;
	asts.type_def_count = ARRAY_COUNT(type_defs);
	asts.family_binders = family_binders;
	asts.family_binder_count = ARRAY_COUNT(family_binders);
	asts.type_constructors = constructors;
	asts.type_constructor_count = ARRAY_COUNT(constructors);
	asts.type_field_exprs = field_exprs;
	asts.type_field_binder_ids = field_binder_ids;
	asts.type_field_name_symbol_ids = field_symbols;
	asts.type_field_expr_count = ARRAY_COUNT(field_exprs);
	asts.expectations = expectations;
	asts.expectation_count = ARRAY_COUNT(expectations);
	asts.assignments = assignments;
	asts.assignment_count = ARRAY_COUNT(assignments);

	struct prototype_source_schedule_item schedule_items[3];
	struct prototype_source_schedule schedule;
	if (prototype_source_schedule_build(
			&asts, schedule_items, ARRAY_COUNT(schedule_items), &schedule
		) != 0) {
		fprintf(stderr, "source schedule construction failed\n");
		return 1;
	}

	struct prototype_source_lowering_plan_capacity capacity;
	if (prototype_source_lowering_plan_measure(
			&asts, &schedule, &capacity
		) != 0 || capacity.slot_count != 14 || capacity.root_count != 3 ||
		capacity.edge_count == 0 || capacity.binder_count < 10) {
		fprintf(stderr, "source lowering plan measurement failed\n");
		return 1;
	}
	struct prototype_source_lowering_slot slots[14];
	struct prototype_source_lowering_edge edges[64];
	struct prototype_source_lowering_binder binders[16];
	struct prototype_source_lowering_scope_recipe scope_recipes[16];
	struct prototype_source_lowering_root roots[3];
	uint32_t order[14];
	struct prototype_source_lowering_plan plan;
	if (prototype_source_lowering_plan_build(
			&asts, &schedule,
			slots, ARRAY_COUNT(slots),
			edges, ARRAY_COUNT(edges),
			binders, ARRAY_COUNT(binders),
			scope_recipes, ARRAY_COUNT(scope_recipes),
			roots, ARRAY_COUNT(roots),
			order, ARRAY_COUNT(order),
			&plan
		) != 0 || prototype_source_lowering_plan_validate(&plan) != 0 ||
		prototype_source_lowering_plan_term_slot(&plan, 10) != 10 ||
		prototype_source_lowering_plan_type_expr_slot(&plan, 1) != 12 ||
		plan.roots[0].source_item_kind != PROTOTYPE_SOURCE_SCHEDULE_EXPECTATION ||
		plan.roots[0].slot_id != 12 || plan.roots[1].slot_id != 10 ||
		plan.roots[2].slot_id != 5 ||
		plan.slots[13].owner_type_def_id != 0) {
		fprintf(stderr, "source lowering plan construction failed\n");
		return 1;
	}
	if (plan.slots[1].edge_count != 2 || plan.slots[1].binder_count != 1 ||
		plan.binders[plan.slots[1].first_binder].annotation_slot != 11 ||
		plan.slots[9].binder_count != 3 ||
		!find_binder_role(
			&plan, 4, PROTOTYPE_SOURCE_BINDER_MATCH_FIELD
		) || !find_binder_role(
			&plan, 4, PROTOTYPE_SOURCE_BINDER_MATCH_SELECTOR_VALUE
		) || !find_binder_role(
			&plan, 4, PROTOTYPE_SOURCE_BINDER_MATCH_SELECTOR_GRAPH
		) || !find_binder_role(
			&plan, 10, PROTOTYPE_SOURCE_BINDER_FAMILY_PARAMETER
		) || !find_binder_role(
			&plan, 10, PROTOTYPE_SOURCE_BINDER_CONSTRUCTOR_FIELD
		)) {
		fprintf(stderr, "source lowering topology lost an edge or binder\n");
		return 1;
	}
	const struct prototype_source_lowering_edge* lambda_body = find_edge(
		&plan, 1, PROTOTYPE_SOURCE_EDGE_BODY, 0
	);
	const struct prototype_source_lowering_edge* pi_codomain = find_edge(
		&plan, 12, PROTOTYPE_SOURCE_EDGE_TYPE_CODOMAIN, 0
	);
	const struct prototype_source_lowering_edge* block_first = find_edge(
		&plan, 8, PROTOTYPE_SOURCE_EDGE_BLOCK_ITEM, 0
	);
	const struct prototype_source_lowering_edge* block_second = find_edge(
		&plan, 8, PROTOTYPE_SOURCE_EDGE_BLOCK_ITEM, 1
	);
	const struct prototype_source_lowering_edge* family_annotation = find_edge(
		&plan, 10, PROTOTYPE_SOURCE_EDGE_TYPE_FAMILY_BINDER, 0
	);
	const struct prototype_source_lowering_edge* field_annotation = find_edge(
		&plan, 10, PROTOTYPE_SOURCE_EDGE_TYPE_CONSTRUCTOR_FIELD, 0
	);
	const struct prototype_source_lowering_edge* constructor_result = find_edge(
		&plan, 10, PROTOTYPE_SOURCE_EDGE_TYPE_CONSTRUCTOR_RESULT, 0
	);
	uint32_t lambda_binder = find_binder(
		&plan, 1, PROTOTYPE_SOURCE_BINDER_LAMBDA, 0
	);
	uint32_t pi_binder = find_binder(
		&plan, 12, PROTOTYPE_SOURCE_BINDER_PI, 0
	);
	uint32_t block_binder = find_binder(
		&plan, 6, PROTOTYPE_SOURCE_BINDER_BLOCK_RESULT, 0
	);
	uint32_t family_binder = find_binder(
		&plan, 10, PROTOTYPE_SOURCE_BINDER_FAMILY_PARAMETER, 0
	);
	uint32_t field_binder = find_binder(
		&plan, 10, PROTOTYPE_SOURCE_BINDER_CONSTRUCTOR_FIELD, 0
	);
	if (!lambda_body || !pi_codomain || !block_first || !block_second ||
		!family_annotation || !field_annotation || !constructor_result ||
		!recipe_is(
			&plan, lambda_body->scope_recipe, lambda_binder, UINT32_MAX
		) || !recipe_is(
			&plan, pi_codomain->scope_recipe, pi_binder, UINT32_MAX
		) || block_first->scope_recipe != UINT32_MAX || !recipe_is(
			&plan, block_second->scope_recipe, block_binder, UINT32_MAX
		) || family_annotation->scope_recipe != UINT32_MAX || !recipe_is(
			&plan, field_annotation->scope_recipe, family_binder, UINT32_MAX
		) || !recipe_is(
			&plan, constructor_result->scope_recipe, field_binder,
			field_annotation->scope_recipe
		)) {
		fprintf(stderr, "source lexical scope recipes are inconsistent\n");
		return 1;
	}
	if (prototype_source_lowering_plan_build(
			&asts, &schedule,
			slots, ARRAY_COUNT(slots) - 1,
			edges, ARRAY_COUNT(edges),
			binders, ARRAY_COUNT(binders),
			scope_recipes, ARRAY_COUNT(scope_recipes),
			roots, ARRAY_COUNT(roots),
			order, ARRAY_COUNT(order),
			&plan
		) == 0) {
		fprintf(stderr, "source lowering plan accepted undersized storage\n");
		return 1;
	}
	if (prototype_source_lowering_plan_build(
			&asts, &schedule,
			slots, ARRAY_COUNT(slots),
			edges, ARRAY_COUNT(edges),
			binders, ARRAY_COUNT(binders),
			scope_recipes, ARRAY_COUNT(scope_recipes),
			roots, ARRAY_COUNT(roots),
			order, ARRAY_COUNT(order),
			&plan
		) != 0) {
		return 1;
	}
	int64_t saved_literal = nodes[2].as.int_literal.value;
	nodes[2].as.int_literal.value = 8;
	if (prototype_source_lowering_plan_validate(&plan) == 0) {
		fprintf(stderr, "source lowering plan accepted source mutation\n");
		return 1;
	}
	nodes[2].as.int_literal.value = saved_literal;
	if (prototype_source_lowering_plan_validate(&plan) != 0) {
		fprintf(stderr, "source lowering plan did not recover source seal\n");
		return 1;
	}
	uint32_t saved_child = edges[0].child_slot;
	edges[0].child_slot = edges[0].owner_slot;
	if (prototype_source_lowering_plan_validate(&plan) == 0) {
		fprintf(stderr, "source lowering plan accepted mutable topology\n");
		return 1;
	}
	edges[0].child_slot = saved_child;
	if (prototype_source_lowering_plan_validate(&plan) != 0) {
		fprintf(stderr, "source lowering plan did not recover after restoration\n");
		return 1;
	}
	return 0;
}
