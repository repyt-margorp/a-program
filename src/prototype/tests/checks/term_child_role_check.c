#include "a_program/core/term.h"
#include "a_program/graph/typed_occurrence_graph.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 16
#define CASE_BINDER_CAPACITY 16
#define IH_SCOPE_CAPACITY 16
#define OCCURRENCE_CAPACITY 16

struct test_storage {
	struct prototype_term terms[TERM_CAPACITY];
	struct prototype_match_case cases[CASE_CAPACITY];
	int case_labels[CASE_CAPACITY];
	struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scopes[IH_SCOPE_CAPACITY];
	struct prototype_typed_occurrence occurrences[OCCURRENCE_CAPACITY];
	struct prototype_typed_occurrence_edge occurrence_edges[32];
	struct prototype_typed_occurrence_match_case occurrence_cases[2];
	struct prototype_typed_occurrence_fold_clause occurrence_fold_clauses[2];
};

static int expect_child(
	const struct prototype_term_db* terms,
	uint32_t parent,
	uint32_t index,
	int role,
	uint32_t ordinal,
	uint32_t child_term
) {
	struct prototype_term_child child;
	return prototype_term_child_at(terms, parent, index, &child) == 0 &&
		child.role == role && child.ordinal == ordinal && child.term == child_term ?
		0 : -1;
}

static int expect_count(
	const struct prototype_term_db* terms,
	uint32_t parent,
	uint32_t expected
) {
	uint32_t count;
	return prototype_term_child_count(terms, parent, &count) == 0 &&
		count == expected ? 0 : -1;
}

static int expect_occurrence_child(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t parent,
	uint32_t index,
	uint32_t expected
) {
	uint32_t child;
	return prototype_typed_occurrence_core_child(
		graph, terms, parent, index, &child
	) == 0 && child == expected ? 0 : -1;
}

int main(void) {
	struct test_storage storage;
	struct prototype_term_db terms;
	prototype_term_db_init(
		&terms,
		storage.terms,
		TERM_CAPACITY,
		storage.cases,
		storage.case_labels,
		CASE_CAPACITY,
		storage.case_binders,
		CASE_BINDER_CAPACITY,
		storage.ih_scopes,
		IH_SCOPE_CAPACITY
	);

	uint32_t zero;
	uint32_t one;
	uint32_t app;
	uint32_t app_view;
	uint32_t binder = prototype_term_new_binding(&terms);
	uint32_t variable;
	uint32_t lambda;
	if (binder == PROTOTYPE_INVALID_ID ||
		prototype_term_int_literal(&terms, 0, &zero) != 0 ||
		prototype_term_int_literal(&terms, 1, &one) != 0 ||
		prototype_term_app(&terms, zero, one, &app) != 0 ||
		prototype_term_var(&terms, binder, &variable) != 0 ||
		prototype_term_lambda(&terms, binder, variable, &lambda) != 0 ||
		expect_count(&terms, app, 2) != 0 ||
		expect_child(&terms, app, 0, PROTOTYPE_TERM_CHILD_FUNCTION, 0, zero) != 0 ||
		expect_child(&terms, app, 1, PROTOTYPE_TERM_CHILD_ARGUMENT, 0, one) != 0 ||
		expect_count(&terms, lambda, 1) != 0 ||
		expect_child(&terms, lambda, 0, PROTOTYPE_TERM_CHILD_BODY, 0, variable) != 0) {
		return 1;
	}
	if (terms.term_count >= TERM_CAPACITY) {
		return 1;
	}
	app_view = (uint32_t)terms.term_count++;
	storage.terms[app_view] = (struct prototype_term) {
		.tag = PROTOTYPE_TERM_TYPE_VIEW,
		.as.type_view = {
			.view_type_id = 0,
			.core = app,
			.source = app
		}
	};

	struct prototype_match_case_input match_cases[2] = {
		{
			.case_label_symbol_id = 1,
			.constructor_owner = zero,
			.constructor_id = 0,
			.body = zero
		},
		{
			.case_label_symbol_id = 2,
			.constructor_owner = zero,
			.constructor_id = 1,
			.body = one
		}
	};
	uint32_t match;
	if (prototype_term_match(&terms, variable, match_cases, 2, &match) != 0 ||
		expect_count(&terms, match, 3) != 0 ||
		expect_child(
			&terms, match, 0, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0, variable
		) != 0 ||
		expect_child(
			&terms, match, 1, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, 0, zero
		) != 0 ||
		expect_child(
			&terms, match, 2, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, 1, one
		) != 0) {
		return 2;
	}

	uint32_t type_former = (uint32_t)terms.term_count++;
	storage.terms[type_former] = (struct prototype_term) {
		.tag = PROTOTYPE_TERM_TYPE_FORMER,
		.as.type_former = {
			.representation_id = 17,
			.constructor_count = 2
		}
	};
	struct prototype_match_case_input constant_cases[2] = {
		{
			.case_label_symbol_id = 3,
			.constructor_owner = type_former,
			.constructor_id = 0,
			.body = one
		},
		{
			.case_label_symbol_id = 4,
			.constructor_owner = type_former,
			.constructor_id = 1,
			.body = one
		}
	};
	uint32_t constant_match;
	uint32_t reduced_constant_match;
	if (prototype_term_match(
			&terms, variable, constant_cases, 2, &constant_match
		) != 0 || prototype_term_perform_with_options(
			&terms,
			NULL,
			NULL,
			(struct prototype_term_reduction_options) {
				.flags = PROTOTYPE_TERM_EVALUATE_DEFAULT
			},
			constant_match,
			&reduced_constant_match
		) != 0 || reduced_constant_match != one) {
		return 7;
	}

	uint32_t return_zero;
	uint32_t thunk_return;
	uint32_t force_thunk;
	uint32_t effect_operation;
	uint32_t second_effect_operation;
	uint32_t continuation;
	uint32_t request;
	if (prototype_term_return(&terms, zero, &return_zero) != 0 ||
		prototype_term_thunk(&terms, return_zero, &thunk_return) != 0 ||
		prototype_term_force(&terms, thunk_return, &force_thunk) != 0 ||
		prototype_term_effect_operation(
			&terms, PROTOTYPE_EFFECT_OPERATION_PRINT, &effect_operation
		) != 0 || prototype_term_effect_operation(
			&terms, PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT,
			&second_effect_operation
		) != 0 || prototype_term_thunk(&terms, lambda, &continuation) != 0 ||
		prototype_term_operation_request(
			&terms, effect_operation, zero, continuation, &request
		) != 0 ||
		expect_child(
			&terms, return_zero, 0, PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0, zero
		) != 0 ||
		expect_child(
			&terms, thunk_return, 0, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0,
			return_zero
		) != 0 ||
		expect_child(
			&terms, force_thunk, 0, PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0,
			thunk_return
		) != 0 ||
		expect_count(&terms, request, 3) != 0 ||
		expect_child(
			&terms, request, 0, PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
			effect_operation
		) != 0 ||
		expect_child(
			&terms, request, 1, PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0, zero
		) != 0 ||
		expect_child(
			&terms, request, 2, PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
			continuation
		) != 0) {
		return 3;
	}

	struct prototype_computation_fold_clause clauses[2] = {
		{ .operation = effect_operation, .body = lambda },
		{ .operation = second_effect_operation, .body = lambda }
	};
	uint32_t fold;
	if (prototype_term_computation_fold(
			&terms, request, lambda, clauses, 2, &fold
		) != 0 || expect_count(&terms, fold, 6) != 0 ||
		expect_child(
			&terms, fold, 0, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0, request
		) != 0 ||
		expect_child(
			&terms, fold, 1, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0, lambda
		) != 0 ||
		expect_child(
			&terms, fold, 2, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION, 0,
			effect_operation
		) != 0 ||
		expect_child(
			&terms, fold, 3, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY, 0, lambda
		) != 0 ||
		expect_child(
			&terms, fold, 4, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION, 1,
			second_effect_operation
		) != 0 ||
		expect_child(
			&terms, fold, 5, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY, 1, lambda
		) != 0 || prototype_term_child_at(&terms, fold, 6, &(struct prototype_term_child){0}) == 0) {
		return 4;
	}

	storage.occurrences[0] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_ATOM,
		.core_term = zero
	};
	storage.occurrences[1] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_ATOM,
		.core_term = one
	};
	storage.occurrences[2] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_VAR,
		.core_term = variable
	};
	storage.occurrences[3] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_APP,
		.core_term = app
	};
	storage.occurrences[4] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_LAMBDA,
		.core_term = lambda
	};
	storage.occurrences[5] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_MATCH,
		.core_term = match,
		.first_case = 0,
		.case_count = 2
	};
	storage.occurrences[6] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_RETURN,
		.core_term = return_zero
	};
	storage.occurrences[7] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_THUNK,
		.core_term = thunk_return
	};
	storage.occurrences[8] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_FORCE,
		.core_term = force_thunk
	};
	storage.occurrences[9] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_ATOM,
		.core_term = effect_operation
	};
	storage.occurrences[10] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_ATOM,
		.core_term = second_effect_operation
	};
	storage.occurrences[11] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_THUNK,
		.core_term = continuation
	};
	storage.occurrences[12] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_REQUEST,
		.core_term = request
	};
	storage.occurrences[13] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD,
		.core_term = fold,
		.first_fold_clause = 0,
		.fold_clause_count = 2
	};
	storage.occurrences[14] = (struct prototype_typed_occurrence) {
		.tag = PROTOTYPE_TYPED_OCCURRENCE_APP,
		.application_role = PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION,
		.core_term = app_view
	};
	struct prototype_typed_occurrence_graph occurrence_graph = {
		.occurrences = storage.occurrences,
		.occurrence_count = 15,
		.occurrence_capacity = OCCURRENCE_CAPACITY,
		.edges = storage.occurrence_edges,
		.edge_capacity = 32,
		.cases = storage.occurrence_cases,
		.case_count = 2,
		.case_capacity = 2,
		.fold_clauses = storage.occurrence_fold_clauses,
		.fold_clause_count = 2,
		.fold_clause_capacity = 2
	};
	for (uint32_t i = 0; i < 15; ++i) {
		storage.occurrences[i].first_edge = PROTOTYPE_INVALID_ID;
		storage.occurrences[i].edge_count = 0;
	}
#define ADD_EDGE(parent, child_role, child_ordinal, child) \
	do { \
		if (prototype_typed_occurrence_graph_add_edge( \
				&occurrence_graph, (parent), \
				(struct prototype_typed_occurrence_edge) { \
					.role = (child_role), \
					.ordinal = (child_ordinal), \
					.child_occurrence = (child) \
				} \
			) != 0) { \
			return 5; \
		} \
	} while (0)
	ADD_EDGE(3, PROTOTYPE_TERM_CHILD_FUNCTION, 0, 0);
	ADD_EDGE(3, PROTOTYPE_TERM_CHILD_ARGUMENT, 0, 1);
	ADD_EDGE(4, PROTOTYPE_TERM_CHILD_BODY, 0, 2);
	ADD_EDGE(5, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0, 2);
	ADD_EDGE(5, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, 0, 0);
	ADD_EDGE(5, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, 1, 1);
	ADD_EDGE(6, PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0, 0);
	ADD_EDGE(7, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0, 6);
	ADD_EDGE(8, PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0, 7);
	ADD_EDGE(11, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0, 4);
	ADD_EDGE(12, PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0, 9);
	ADD_EDGE(12, PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0, 0);
	ADD_EDGE(12, PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0, 11);
	ADD_EDGE(13, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0, 12);
	ADD_EDGE(13, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0, 4);
	ADD_EDGE(13, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION, 0, 9);
	ADD_EDGE(13, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY, 0, 4);
	ADD_EDGE(13, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION, 1, 10);
	ADD_EDGE(13, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY, 1, 4);
	ADD_EDGE(14, PROTOTYPE_TERM_CHILD_FUNCTION, 0, 0);
	ADD_EDGE(14, PROTOTYPE_TERM_CHILD_ARGUMENT, 0, 1);
#undef ADD_EDGE
	if (expect_occurrence_child(&occurrence_graph, &terms, 3, 0, 0) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 3, 1, 1) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 4, 0, 2) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 5, 0, 2) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 5, 1, 0) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 5, 2, 1) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 12, 2, 11) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 13, 0, 12) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 13, 1, 4) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 13, 2, 9) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 13, 3, 4) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 13, 4, 10) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 13, 5, 4) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 14, 0, 0) != 0 ||
		expect_occurrence_child(&occurrence_graph, &terms, 14, 1, 1) != 0 ||
		prototype_typed_occurrence_graph_reaches(
			&occurrence_graph, &terms, 13, 2
		) != 1) {
		return 5;
	}
	storage.occurrence_edges[1].child_occurrence = 0;
	if (prototype_typed_occurrence_core_child(
			&occurrence_graph, &terms, 3, 1, &(uint32_t){0}
		) == 0) {
		return 6;
	}

	return 0;
}
