#include "a_program/core/pipeline.h"
#include "a_program/core/read.h"
#include "a_program/core/request_provider.h"
#include "a_program/core/term.h"
#include "a_program/frontend/core_lowering.h"

#include <stdio.h>
#include <string.h>

#define TERM_CAPACITY 64
#define CASE_CAPACITY 4
#define CASE_BINDER_CAPACITY 4
#define IH_SCOPE_CAPACITY 4

#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

int main(void) {
	struct prototype_term terms_storage[TERM_CAPACITY];
	struct prototype_match_case cases[CASE_CAPACITY];
	int case_labels[CASE_CAPACITY];
	struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scopes[IH_SCOPE_CAPACITY];
	struct prototype_core_pipeline core;
	const struct prototype_core_pipeline_storage core_storage = {
		.terms = terms_storage,
		.term_capacity = TERM_CAPACITY,
		.cases = cases,
		.case_label_symbols = case_labels,
		.case_capacity = CASE_CAPACITY,
		.case_binders = case_binders,
		.case_binder_capacity = CASE_BINDER_CAPACITY,
		.ih_scopes = ih_scopes,
		.ih_scope_capacity = IH_SCOPE_CAPACITY
	};
	struct prototype_core_request_service service = { 0 };
	struct prototype_c0_lowering_stage stage;
	struct prototype_c0_open_term formed;
	uint32_t direct;
	if (prototype_core_pipeline_init(&core, &core_storage, NULL) != 0) {
		return 1;
	}
	struct prototype_term_db* terms = prototype_core_pipeline_terms(&core);
	prototype_core_request_service_init(&service, &core);
	if (prototype_c0_lowering_stage_init(&stage, &service) != 0 ||
		prototype_c0_lowering_stage_text_literal(&stage, 17, &formed) != 0 ||
		prototype_term_text_literal(terms, 17, &direct) != 0 ||
		formed.term_id != direct ||
		formed.graph_revision != terms->normalization_graph_revision) {
		fprintf(stderr, "C0 text-literal formation stage failed\n");
		return 1;
	}
	if (prototype_c0_lowering_stage_int_literal(&stage, 42, &formed) != 0 ||
		prototype_term_int_literal(terms, 42, &direct) != 0 ||
		formed.term_id != direct) {
		fprintf(stderr, "C0 int-literal formation stage failed\n");
		return 1;
	}
	if (prototype_c0_lowering_stage_host_type(
			&stage, PROTOTYPE_HOST_TYPE_TEXT, &formed
		) != 0 || prototype_term_make_host_type(
			terms, PROTOTYPE_HOST_TYPE_TEXT, &direct
		) != 0 || formed.term_id != direct) {
		fprintf(stderr, "C0 host-type formation stage failed\n");
		return 1;
	}
	if (prototype_c0_lowering_stage_pure_primitive(
			&stage, PROTOTYPE_PURE_PRIMITIVE_INT_ADD, -1, &formed
		) != 0 || prototype_term_pure_primitive(
			terms, PROTOTYPE_PURE_PRIMITIVE_INT_ADD, -1, &direct
		) != 0 || formed.term_id != direct) {
		fprintf(stderr, "C0 pure-primitive formation stage failed\n");
		return 1;
	}
	if (prototype_c0_lowering_stage_effect_operation(
			&stage, PROTOTYPE_EFFECT_OPERATION_PRINT, &formed
		) != 0 || prototype_term_effect_operation(
			terms, PROTOTYPE_EFFECT_OPERATION_PRINT, &direct
		) != 0 || formed.term_id != direct) {
		fprintf(stderr, "C0 effect-operation formation stage failed\n");
		return 1;
	}
	uint32_t value;
	if (prototype_term_int_literal(terms, 7, &value) != 0 ||
		prototype_c0_lowering_stage_return(&stage, value, &formed) != 0 ||
		prototype_term_return(terms, value, &direct) != 0 ||
		formed.term_id != direct) {
		fprintf(stderr, "C0 return formation stage failed\n");
		return 1;
	}
	uint32_t returned = formed.term_id;
	if (prototype_c0_lowering_stage_thunk(&stage, returned, &formed) != 0 ||
		prototype_term_thunk(terms, returned, &direct) != 0 ||
		formed.term_id != direct) {
		fprintf(stderr, "C0 thunk formation stage failed\n");
		return 1;
	}
	uint32_t thunked = formed.term_id;
	if (prototype_c0_lowering_stage_force(&stage, thunked, &formed) != 0 ||
		prototype_term_force(terms, thunked, &direct) != 0 ||
		formed.term_id != direct) {
		fprintf(stderr, "C0 force formation stage failed\n");
		return 1;
	}

	struct prototype_ast_node source_nodes[3];
	struct prototype_ast_type_expr source_types[1];
	struct prototype_ast_term_assignment_def source_assignments[2];
	struct prototype_ast_db source;
	memset(source_nodes, 0, sizeof(source_nodes));
	memset(source_types, 0, sizeof(source_types));
	memset(source_assignments, 0, sizeof(source_assignments));
	memset(&source, 0, sizeof(source));
	source_nodes[0].tag = PROTOTYPE_AST_VAR;
	source_nodes[0].as.var.ast_binder_id = 1;
	source_nodes[1].tag = PROTOTYPE_AST_LAMBDA;
	source_nodes[1].as.lambda.ast_binder_id = 1;
	source_nodes[1].as.lambda.binder_symbol_id = 2;
	source_nodes[1].as.lambda.binder_type = 0;
	source_nodes[1].as.lambda.body = 0;
	source_nodes[2].tag = PROTOTYPE_AST_LAMBDA;
	source_nodes[2].as.lambda.ast_binder_id = 1;
	source_nodes[2].as.lambda.binder_symbol_id = 2;
	source_nodes[2].as.lambda.binder_type = 0;
	source_nodes[2].as.lambda.body = 0;
	source_types[0].tag = PROTOTYPE_AST_TYPE_EXPR_UNIVERSE;
	source_assignments[0].source_entry_id = 1;
	source_assignments[0].name_symbol_id = 3;
	source_assignments[0].ast = 1;
	source_assignments[1].source_entry_id = 2;
	source_assignments[1].name_symbol_id = 4;
	source_assignments[1].ast = 2;
	source.nodes = source_nodes;
	source.node_count = ARRAY_COUNT(source_nodes);
	source.type_exprs = source_types;
	source.type_expr_count = ARRAY_COUNT(source_types);
	source.assignments = source_assignments;
	source.assignment_count = ARRAY_COUNT(source_assignments);
	struct prototype_source_schedule_item schedule_items[2];
	struct prototype_source_schedule schedule;
	struct prototype_source_lowering_plan_capacity capacity;
	struct prototype_source_lowering_slot plan_slots[4];
	struct prototype_source_lowering_edge plan_edges[6];
	struct prototype_source_lowering_binder plan_binders[2];
	struct prototype_source_lowering_scope_recipe plan_scope_recipes[2];
	struct prototype_source_lowering_root plan_roots[2];
	uint32_t plan_order[4];
	struct prototype_source_lowering_plan plan;
	if (prototype_source_schedule_build(
			&source, schedule_items, ARRAY_COUNT(schedule_items), &schedule
		) != 0 || prototype_source_lowering_plan_measure(
			&source, &schedule, &capacity
		) != 0 || capacity.slot_count != 4 || capacity.binder_count != 2 ||
		prototype_source_lowering_plan_build(
			&source, &schedule,
			plan_slots, ARRAY_COUNT(plan_slots),
			plan_edges, ARRAY_COUNT(plan_edges),
			plan_binders, ARRAY_COUNT(plan_binders),
			plan_scope_recipes, ARRAY_COUNT(plan_scope_recipes),
			plan_roots, ARRAY_COUNT(plan_roots),
			plan_order, ARRAY_COUNT(plan_order), &plan
		) != 0) {
		fprintf(stderr, "C0 binding fixture construction failed\n");
		return 1;
	}
	struct prototype_source_core_slot_result handoff_slots[4];
	struct prototype_source_core_binder_result handoff_binders[2];
	struct prototype_source_core_handoff_builder handoff;
	struct prototype_source_core_handoff_view handoff_view;
	size_t deferred_count = SIZE_MAX;
	if (prototype_source_core_handoff_builder_init(
			&handoff,
			handoff_slots, ARRAY_COUNT(handoff_slots),
			handoff_binders, ARRAY_COUNT(handoff_binders),
			plan.source_fingerprint
		) != 0 || prototype_c0_lowering_stage_form_open_plan(
			&stage, &plan, &handoff, &deferred_count
		) != 0 || deferred_count != 0 ||
		handoff_binders[0].state != PROTOTYPE_SOURCE_CORE_FORMED ||
		handoff_binders[0].binding_id == PROTOTYPE_INVALID_ID ||
		handoff_binders[1].state != PROTOTYPE_SOURCE_CORE_FORMED ||
		handoff_binders[1].binding_id != handoff_binders[0].binding_id) {
		fprintf(stderr, "C0 open-plan formation failed\n");
		return 1;
	}
	if (handoff_slots[1].state != PROTOTYPE_SOURCE_CORE_FORMED ||
		handoff_slots[2].state != PROTOTYPE_SOURCE_CORE_FORMED ||
		handoff_slots[1].term_id != handoff_slots[2].term_id ||
		prototype_c0_lowering_stage_preallocate_bindings(
			&stage, &plan, &handoff
		) != 0 || prototype_source_core_handoff_builder_seal_complete(
			&handoff, terms->normalization_graph_revision, &handoff_view
		) != 0 || prototype_c0_lowering_stage_preallocate_bindings(
			&stage, &plan, &handoff
		) == 0) {
		fprintf(stderr, "C0 source-slot sealing failed\n");
		return 1;
	}

	struct prototype_ast_node match_nodes[4];
	struct prototype_ast_type_expr match_types[1];
	struct prototype_ast_match_case match_cases[1];
	struct prototype_ast_binder match_case_binders[1];
	struct prototype_ast_term_assignment_def match_assignments[1];
	struct prototype_ast_db match_source;
	memset(match_nodes, 0, sizeof(match_nodes));
	memset(match_types, 0, sizeof(match_types));
	memset(match_cases, 0, sizeof(match_cases));
	memset(match_case_binders, 0, sizeof(match_case_binders));
	memset(match_assignments, 0, sizeof(match_assignments));
	memset(&match_source, 0, sizeof(match_source));
	match_nodes[0].tag = PROTOTYPE_AST_VAR;
	match_nodes[0].as.var.ast_binder_id = 100;
	match_nodes[1].tag = PROTOTYPE_AST_INDUCTION_HYPOTHESIS;
	match_nodes[1].as.induction_hypothesis.ast_binder_id = 101;
	match_nodes[2].tag = PROTOTYPE_AST_MATCH;
	match_nodes[2].as.match.scrutinee = 0;
	match_nodes[2].as.match.first_case = 0;
	match_nodes[2].as.match.case_count = 1;
	match_nodes[3].tag = PROTOTYPE_AST_LAMBDA;
	match_nodes[3].as.lambda.ast_binder_id = 100;
	match_nodes[3].as.lambda.binder_symbol_id = 10;
	match_nodes[3].as.lambda.binder_type = 0;
	match_nodes[3].as.lambda.body = 2;
	match_types[0].tag = PROTOTYPE_AST_TYPE_EXPR_UNIVERSE;
	match_cases[0].constructor_symbol_id = 11;
	match_cases[0].first_binder = 0;
	match_cases[0].binder_count = 1;
	match_cases[0].body = 1;
	match_case_binders[0].ast_binder_id = 101;
	match_case_binders[0].symbol_id = 12;
	match_assignments[0].source_entry_id = 1;
	match_assignments[0].name_symbol_id = 13;
	match_assignments[0].ast = 3;
	match_source.nodes = match_nodes;
	match_source.node_count = ARRAY_COUNT(match_nodes);
	match_source.type_exprs = match_types;
	match_source.type_expr_count = ARRAY_COUNT(match_types);
	match_source.cases = match_cases;
	match_source.case_count = ARRAY_COUNT(match_cases);
	match_source.case_binders = match_case_binders;
	match_source.case_binder_count = ARRAY_COUNT(match_case_binders);
	match_source.assignments = match_assignments;
	match_source.assignment_count = ARRAY_COUNT(match_assignments);
	struct prototype_source_schedule_item match_schedule_items[1];
	struct prototype_source_schedule match_schedule;
	struct prototype_source_lowering_plan_capacity match_capacity;
	struct prototype_source_lowering_slot match_plan_slots[5];
	struct prototype_source_lowering_edge match_plan_edges[8];
	struct prototype_source_lowering_binder match_plan_binders[3];
	struct prototype_source_lowering_scope_recipe match_plan_scope_recipes[3];
	struct prototype_source_lowering_root match_plan_roots[1];
	uint32_t match_plan_order[5];
	struct prototype_source_lowering_plan match_plan;
	if (prototype_source_schedule_build(
			&match_source, match_schedule_items, ARRAY_COUNT(match_schedule_items),
			&match_schedule
		) != 0 || prototype_source_lowering_plan_measure(
			&match_source, &match_schedule, &match_capacity
		) != 0 || match_capacity.slot_count > ARRAY_COUNT(match_plan_slots) ||
		match_capacity.edge_count > ARRAY_COUNT(match_plan_edges) ||
		match_capacity.binder_count > ARRAY_COUNT(match_plan_binders) ||
		prototype_source_lowering_plan_build(
			&match_source, &match_schedule,
			match_plan_slots, ARRAY_COUNT(match_plan_slots),
			match_plan_edges, ARRAY_COUNT(match_plan_edges),
			match_plan_binders, ARRAY_COUNT(match_plan_binders),
			match_plan_scope_recipes, ARRAY_COUNT(match_plan_scope_recipes),
			match_plan_roots, ARRAY_COUNT(match_plan_roots),
			match_plan_order, ARRAY_COUNT(match_plan_order), &match_plan
		) != 0) {
		fprintf(stderr, "C0 Match fixture construction failed\n");
		return 1;
	}
	struct prototype_source_core_slot_result match_handoff_slots[5];
	struct prototype_source_core_binder_result match_handoff_binders[3];
	struct prototype_source_core_handoff_builder match_handoff;
	struct prototype_source_core_handoff_view match_handoff_view;
	deferred_count = SIZE_MAX;
	if (prototype_source_core_handoff_builder_init(
			&match_handoff,
			match_handoff_slots, match_plan.slot_count,
			match_handoff_binders, match_plan.binder_count,
			match_plan.source_fingerprint
		) != 0 || prototype_c0_lowering_stage_form_open_plan(
			&stage, &match_plan, &match_handoff, &deferred_count
		) != 0 || deferred_count != 0 ||
		prototype_source_core_handoff_builder_seal_complete(
			&match_handoff, terms->normalization_graph_revision,
			&match_handoff_view
		) != 0) {
		fprintf(stderr, "C0 open Match formation failed\n");
		return 1;
	}
	const struct prototype_source_core_slot_result* match_result =
		prototype_source_core_handoff_view_slot_get(&match_handoff_view, 2);
	struct prototype_core_read_snapshot snapshot;
	struct prototype_term match_term;
	struct prototype_core_match_case_view match_case;
	struct prototype_case_binder match_binder;
	if (!match_result || match_result->ih_scope_id == PROTOTYPE_INVALID_ID ||
		match_result->result_kind != PROTOTYPE_SOURCE_CORE_SLOT_TERM ||
		prototype_core_pipeline_read_snapshot(&core, &snapshot) != 0 ||
		prototype_core_read_term(
			&snapshot, match_result->term_id, &match_term
		) != 0 || match_term.tag != PROTOTYPE_TERM_MATCH ||
		match_term.as.match.ih_scope_id != match_result->ih_scope_id ||
		prototype_core_read_match_case(
			&snapshot, match_term.as.match.first_case, &match_case
		) != 0 || match_case.match_case.constructor_owner != PROTOTYPE_INVALID_ID ||
		match_case.match_case.constructor_id != PROTOTYPE_INVALID_ID ||
		prototype_core_read_case_binder(
			&snapshot, match_case.match_case.first_binder, &match_binder
		) != 0 || match_binder.is_recursive) {
		fprintf(stderr, "C0 decided typed Match evidence\n");
		return 1;
	}
	prototype_core_pipeline_dispose(&core);
	printf("C0 lowering stage check passed\n");
	return 0;
}
