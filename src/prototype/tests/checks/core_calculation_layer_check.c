#include "a_program/core/term.h"
#include "a_program/core/pipeline.h"
#include "a_program/core/read.h"
#include "a_program/core/request.h"
#include "a_program/core/request_provider.h"

#include <stdint.h>
#include <stdio.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 16
#define CASE_BINDER_CAPACITY 16
#define IH_SCOPE_CAPACITY 4

int main(void) {
	struct prototype_term term_storage[TERM_CAPACITY];
	struct prototype_match_case case_storage[CASE_CAPACITY];
	int case_label_storage[CASE_CAPACITY];
	struct prototype_case_binder case_binder_storage[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scope_storage[IH_SCOPE_CAPACITY];
	const struct prototype_core_pipeline_storage core_storage = {
		.terms = term_storage,
		.term_capacity = TERM_CAPACITY,
		.cases = case_storage,
		.case_label_symbols = case_label_storage,
		.case_capacity = CASE_CAPACITY,
		.case_binders = case_binder_storage,
		.case_binder_capacity = CASE_BINDER_CAPACITY,
		.ih_scopes = ih_scope_storage,
		.ih_scope_capacity = IH_SCOPE_CAPACITY
	};
	struct prototype_core_pipeline core;
	if (prototype_core_pipeline_init(&core, &core_storage, NULL) != 0) {
		fprintf(stderr, "Core pipeline initialization failed\n");
		return 1;
	}
	struct prototype_term_db* terms = prototype_core_pipeline_terms(&core);

	uint32_t seven;
	uint32_t nine;
	uint32_t variable;
	uint32_t identity;
	uint32_t application;
	uint32_t normalized;
	uint32_t binding = prototype_term_new_binding(terms);
	if (binding == PROTOTYPE_INVALID_ID ||
		prototype_term_int_literal(terms, 7, &seven) != 0 ||
		prototype_term_int_literal(terms, 9, &nine) != 0 ||
		prototype_term_var(terms, binding, &variable) != 0 ||
		prototype_term_lambda(terms, binding, variable, &identity) != 0 ||
		prototype_term_app(terms, identity, seven, &application) != 0 ||
		prototype_term_normalize_complete_with_profile(
			terms, NULL, PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			application, &normalized
		) != 0 || normalized != seven) {
		fprintf(stderr, "Core-only beta normalization failed\n");
		return 1;
	}
	struct prototype_core_request_service core_requests = { 0 };
	struct prototype_core_normalization_request normalize_request = {
		.term_id = application,
		.profile = PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
		.step_limit = PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT
	};
	struct prototype_core_normalization_response normalize_response = { 0 };
	prototype_core_request_service_init(&core_requests, &core);
	if (prototype_core_request_normalize(
			&core_requests, &normalize_request, &normalize_response
		) != 0 || normalize_response.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
		normalize_response.term_id != seven ||
		normalize_response.steps_used > normalize_response.step_limit) {
		fprintf(stderr, "Core request boundary normalization failed\n");
		return 1;
	}
	struct prototype_core_formation_request form_request = {
		.kind = PROTOTYPE_CORE_FORM_APP,
		.as.app = { .function = identity, .argument = nine }
	};
	struct prototype_core_formation_response form_response = { 0 };
	uint32_t identity_nine;
	if (prototype_core_request_form(
			&core_requests, &form_request, NULL, 0, &form_response
		) != 0 || prototype_term_app(
			terms, identity, nine, &identity_nine
		) != 0 || form_response.term_id != identity_nine ||
		form_response.graph_revision != terms->normalization_graph_revision) {
		fprintf(stderr, "Core formation request boundary failed\n");
		return 1;
	}
	struct prototype_core_formation_request level_request = {
		.kind = PROTOTYPE_CORE_FORM_NEW_UNIVERSE_LEVEL
	};
	struct prototype_core_formation_response first_level = { 0 };
	struct prototype_core_formation_response second_level = { 0 };
	struct prototype_core_formation_response universe_term = { 0 };
	if (prototype_core_request_form(
			&core_requests, &level_request, NULL, 0, &first_level
		) != 0 || prototype_core_request_form(
			&core_requests, &level_request, NULL, 0, &second_level
		) != 0 || first_level.identity_id == PROTOTYPE_INVALID_ID ||
		second_level.identity_id != first_level.identity_id + 1) {
		fprintf(stderr, "Core Universe-level identity allocation failed\n");
		return 1;
	}
	struct prototype_core_formation_request universe_request = {
		.kind = PROTOTYPE_CORE_FORM_UNIVERSE_VAR,
		.as.universe_var = { .level_var = first_level.identity_id }
	};
	if (prototype_core_request_form(
			&core_requests, &universe_request, NULL, 0, &universe_term
		) != 0 || universe_term.term_id == PROTOTYPE_INVALID_ID ||
		terms->terms[universe_term.term_id].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		terms->terms[universe_term.term_id].as.universe_var.level_var !=
			first_level.identity_id) {
		fprintf(stderr, "Core Universe syntax formation failed\n");
		return 1;
	}
	struct prototype_core_rewrite_request rewrite_request = {
		.kind = PROTOTYPE_CORE_REWRITE_REPLACE_EXACT,
		.term_id = application,
		.as.replace_exact = { .exact_term = seven, .replacement = nine }
	};
	struct prototype_core_rewrite_response rewrite_response = { 0 };
	if (prototype_core_request_rewrite(
			&core_requests, &rewrite_request, NULL, 0, &rewrite_response
		) != 0 || rewrite_response.term_id != identity_nine ||
		rewrite_response.graph_revision != terms->normalization_graph_revision) {
		fprintf(stderr, "Core rewrite request boundary failed\n");
		return 1;
	}

	uint32_t owner;
	uint32_t zero;
	uint32_t successor;
	uint32_t successor_seven;
	uint32_t case_binding = prototype_term_new_binding(terms);
	uint32_t case_variable;
	if (case_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_primitive_int(terms, &owner) != 0 ||
		prototype_term_constructor(terms, owner, 0, &zero) != 0 ||
		prototype_term_constructor(terms, owner, 1, &successor) != 0 ||
		prototype_term_app(terms, successor, seven, &successor_seven) != 0 ||
		prototype_term_var(terms, case_binding, &case_variable) != 0) {
		fprintf(stderr, "Core-only Match fixture construction failed\n");
		return 1;
	}
	struct prototype_case_binder successor_binder = {
		.binding_id = case_binding,
		.is_recursive = 0
	};
	struct prototype_match_case_input sealed_cases[2] = {
		{
			.case_label_symbol_id = 10,
			.constructor_owner = owner,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = nine
		},
		{
			.case_label_symbol_id = 11,
			.constructor_owner = owner,
			.constructor_id = 1,
			.binders = &successor_binder,
			.binder_count = 1,
			.body = case_variable
		}
	};
	uint32_t sealed_match;
	if (prototype_term_match(
			terms, successor_seven, sealed_cases, 2, &sealed_match
		) != 0 || prototype_term_normalize_complete_with_profile(
			terms, NULL, PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			sealed_match, &normalized
		) != 0 || normalized != seven) {
		fprintf(stderr, "Core-only iota normalization failed\n");
		return 1;
	}
	struct prototype_core_read_snapshot snapshot;
	struct prototype_term match_view;
	struct prototype_term_child match_child;
	struct prototype_core_match_case_view successor_case_view;
	struct prototype_case_binder successor_binder_view;
	uint32_t match_child_count;
	uint32_t spine_head;
	uint32_t spine_owner;
	uint32_t spine_constructor;
	uint32_t spine_arguments[1];
	uint32_t spine_argument_count;
	if (prototype_core_pipeline_read_snapshot(&core, &snapshot) != 0 ||
		prototype_core_read_term(&snapshot, sealed_match, &match_view) != 0 ||
		match_view.tag != PROTOTYPE_TERM_MATCH ||
		prototype_core_read_child_count(
			&snapshot, sealed_match, &match_child_count
		) != 0 || match_child_count != 3 ||
		prototype_core_read_child_at(
			&snapshot, sealed_match, 0, &match_child
		) != 0 || match_child.term != successor_seven ||
		prototype_core_read_constructor_spine(
			&snapshot,
			successor_seven,
			&spine_head,
			&spine_owner,
			&spine_constructor,
			spine_arguments,
			1,
			&spine_argument_count
		) != 0 || spine_head != successor || spine_owner != owner ||
		spine_constructor != 1 || spine_argument_count != 1 ||
		spine_arguments[0] != seven ||
		prototype_core_read_match_case(
			&snapshot,
			match_view.as.match.first_case + 1,
			&successor_case_view
		) != 0 || successor_case_view.label_symbol_id != 11 ||
		prototype_core_read_case_binder(
			&snapshot,
			successor_case_view.match_case.first_binder,
			&successor_binder_view
		) != 0 || successor_binder_view.binding_id != case_binding) {
		fprintf(stderr, "Core structural read snapshot failed\n");
		return 1;
	}

	struct prototype_match_case_input open_cases[2] = {
		sealed_cases[0], sealed_cases[1]
	};
	open_cases[0].constructor_owner = PROTOTYPE_INVALID_ID;
	open_cases[0].constructor_id = PROTOTYPE_INVALID_ID;
	open_cases[1].constructor_owner = PROTOTYPE_INVALID_ID;
	open_cases[1].constructor_id = PROTOTYPE_INVALID_ID;
	uint32_t open_match;
	if (prototype_term_match(
			terms, successor_seven, open_cases, 2, &open_match
		) != 0 || prototype_term_normalize_complete_with_profile(
			terms, NULL, PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			open_match, &normalized
		) != 0 || normalized != open_match) {
		fprintf(stderr, "open Core Match was repaired without Layer T\n");
		return 1;
	}

	uint32_t empty_row;
	uint32_t print_row;
	uint32_t duplicated_row;
	if (prototype_term_effect_row_empty(terms, &empty_row) != 0 ||
		prototype_term_effect_row_operation(
			terms, PROTOTYPE_EFFECT_OPERATION_PRINT, empty_row, &print_row
		) != 0 || prototype_term_effect_row_union(
			terms, print_row, print_row, &duplicated_row
		) != 0 || prototype_term_normalize_complete_with_profile(
			terms, NULL, PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			duplicated_row, &normalized
		) != 0 || normalized != print_row) {
		fprintf(stderr, "Core-only effect-row syntax normalization failed\n");
		return 1;
	}

	uint32_t returned;
	uint32_t thunked;
	uint32_t forced;
	uint32_t continuation_binding = prototype_term_new_binding(terms);
	uint32_t continuation_variable;
	uint32_t continuation_result;
	uint32_t continuation;
	uint32_t folded;
	if (continuation_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_return(terms, seven, &returned) != 0 ||
		prototype_term_thunk(terms, returned, &thunked) != 0 ||
		prototype_term_force(terms, thunked, &forced) != 0 ||
		prototype_term_normalize_complete_with_profile(
			terms, NULL, PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			forced, &normalized
		) != 0 || normalized != returned ||
		prototype_term_var(
			terms, continuation_binding, &continuation_variable
		) != 0 || prototype_term_return(
			terms, continuation_variable, &continuation_result
		) != 0 || prototype_term_lambda(
			terms, continuation_binding, continuation_result, &continuation
		) != 0 || prototype_term_computation_fold(
			terms, returned, continuation, NULL, 0, &folded
		) != 0 || prototype_term_normalize_complete_with_profile(
			terms, NULL, PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			folded, &normalized
		) != 0 || normalized != returned) {
		fprintf(stderr, "Core-only CBPV cut elimination failed\n");
		return 1;
	}
	struct prototype_core_structural_return_projection_request projection_request = {
		.computation = folded,
		.step_limit = PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT
	};
	struct prototype_core_structural_return_projection_response
		projection_response = { 0 };
	if (prototype_core_request_project_structural_return(
			&core_requests, &projection_request, &projection_response
		) != 0 || projection_response.status !=
			PROTOTYPE_CORE_STRUCTURAL_RETURN_PROJECTION_COMPLETE ||
		projection_response.value != seven || projection_response.graph_revision !=
			terms->normalization_graph_revision) {
		fprintf(stderr, "Core structural-return projection failed\n");
		return 1;
	}

	uint32_t operation;
	uint32_t continuation_thunk;
	uint32_t request;
	struct prototype_term_normalization_result request_result;
	if (prototype_term_effect_operation(
			terms, PROTOTYPE_EFFECT_OPERATION_PRINT, &operation
		) != 0 || prototype_term_thunk(
			terms, continuation, &continuation_thunk
		) != 0 || prototype_term_operation_request(
			terms, operation, seven, continuation_thunk, &request
		) != 0 || prototype_term_normalize_with_profile(
			terms,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			request,
			PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
			&request_result
		) != 0 || request_result.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
		request_result.term_id != request ||
		prototype_term_normalize_with_profile(
			terms,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			request,
			PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
			&request_result
		) != 0 || request_result.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT ||
		request_result.term_id != request) {
		fprintf(stderr, "Core-only operation boundary handling failed\n");
		return 1;
	}
	/* Appending Core syntax does not alter records visible through an older
	 * snapshot, while mutation of an existing record invalidates the pin. */
	if (prototype_core_read_snapshot_validate(&snapshot) != 0 ||
		prototype_core_read_term(
			&snapshot, (uint32_t)snapshot.term_count, &match_view
		) == 0) {
		fprintf(stderr, "Core read snapshot append boundary failed\n");
		return 1;
	}
	prototype_term_notify_graph_mutation(terms);
	if (prototype_core_read_snapshot_validate(&snapshot) == 0) {
		fprintf(stderr, "Core read snapshot mutation pin remained valid\n");
		return 1;
	}

	size_t transaction_term_count = terms->term_count;
	uint32_t transaction_binding_count = terms->next_binding_id;
	uint32_t transaction_universe_level_count = terms->next_universe_level_id;
	struct prototype_core_formation_request transaction_form = {
		.kind = PROTOTYPE_CORE_FORM_INT_LITERAL,
		.as.int_literal = { .value = 1234567 }
	};
	if (prototype_core_pipeline_begin_transaction(&core) != 0 ||
		prototype_core_request_form(
			&core_requests, &transaction_form, NULL, 0, &form_response
		) != 0 || prototype_core_request_form(
			&core_requests, &level_request, NULL, 0, &second_level
		) != 0 || terms->term_count <= transaction_term_count ||
		prototype_core_pipeline_rollback_transaction(&core) != 0 ||
		terms->term_count != transaction_term_count ||
		terms->next_binding_id != transaction_binding_count ||
		terms->next_universe_level_id != transaction_universe_level_count ||
		core.transaction_active) {
		fprintf(stderr, "Core transaction rollback failed\n");
		return 1;
	}

	prototype_core_pipeline_dispose(&core);
	puts("core calculation layer check passed");
	return 0;
}
