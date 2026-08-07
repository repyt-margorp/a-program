#include "term.h"
#include "type_declaration.h"
#include "judgement.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 8
#define CASE_BINDER_CAPACITY 8
#define MATCH_FRAME_CAPACITY 8
#define TYPE_CAPACITY 4
#define CONSTRUCTOR_CAPACITY 4
#define PARAMETER_CAPACITY 4
#define FIELD_TYPE_CAPACITY 4
#define TYPE_EXPR_CAPACITY 4

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[CASE_CAPACITY];
static int case_label_symbols[CASE_CAPACITY];
static struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
static struct prototype_ih_scope ih_scopes[MATCH_FRAME_CAPACITY];
static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration
	constructor_declarations[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration
	parameter_declarations[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];

static int expect_status(
	struct prototype_term_db* terms_db,
	struct prototype_type_declaration_db* type_db,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	int expected_status,
	int expected_reason
) {
	struct prototype_term_conversion_result result;
	if (prototype_term_compare_for_conversion(
			terms_db,
			type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			left,
			right,
			step_limit,
			&result
		) != 0 || result.status != expected_status) {
		return -1;
	}
	if (expected_reason != PROTOTYPE_TERM_CONVERSION_REASON_NONE &&
		result.reason != expected_reason) {
		return -1;
	}
	if (result.left != left || result.right != right ||
		result.step_limit != step_limit ||
		result.graph_revision != terms_db->normalization_graph_revision) {
		return -1;
	}
	return 0;
}

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	prototype_term_db_init(
		&term_db,
		terms,
		TERM_CAPACITY,
		cases,
		case_label_symbols,
		CASE_CAPACITY,
		case_binders,
		CASE_BINDER_CAPACITY,
		ih_scopes,
		MATCH_FRAME_CAPACITY
	);
	prototype_type_declaration_db_init(
		&type_db,
		type_declarations,
		TYPE_CAPACITY,
		constructor_declarations,
		CONSTRUCTOR_CAPACITY,
		parameter_declarations,
		PARAMETER_CAPACITY,
		field_types,
		FIELD_TYPE_CAPACITY,
		type_exprs,
		TYPE_EXPR_CAPACITY
	);

	uint32_t universe_u;
	uint32_t universe_v;
	if (prototype_term_universe_var(&term_db, 7, &universe_u) != 0 ||
		prototype_term_universe_var(&term_db, 8, &universe_v) != 0) {
		return 1;
	}
	if (expect_status(
			&term_db,
			&type_db,
			universe_u,
			universe_u,
			0,
			PROTOTYPE_TERM_CONVERSION_EQUAL,
			PROTOTYPE_TERM_CONVERSION_REASON_NONE
		) != 0) {
		return 2;
	}
	if (expect_status(
			&term_db,
			&type_db,
			universe_u,
			universe_v,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_NOT_EQUAL,
			PROTOTYPE_TERM_CONVERSION_REASON_NONE
		) != 0) {
		return 3;
	}

	uint32_t binder = prototype_term_new_binding(&term_db);
	uint32_t variable;
	uint32_t identity;
	uint32_t beta_redex;
	if (binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(&term_db, binder, &variable) != 0 ||
		prototype_term_lambda(&term_db, binder, variable, &identity) != 0 ||
		prototype_term_app(&term_db, identity, universe_u, &beta_redex) != 0) {
		return 4;
	}
	if (expect_status(
			&term_db,
			&type_db,
			beta_redex,
			universe_u,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_EQUAL,
			PROTOTYPE_TERM_CONVERSION_REASON_NONE
		) != 0) {
		return 5;
	}

	prototype_term_normalization_cache_clear(&term_db);
	if (expect_status(
			&term_db,
			&type_db,
			beta_redex,
			universe_u,
			0,
			PROTOTYPE_TERM_CONVERSION_EXHAUSTED,
			PROTOTYPE_TERM_CONVERSION_REASON_STEP_LIMIT
		) != 0) {
		return 6;
	}

	uint32_t pi_with_redex;
	uint32_t pi_reduced;
	if (prototype_term_pi(
			&term_db, universe_u, beta_redex, &pi_with_redex
		) != 0 || prototype_term_pi(
			&term_db, universe_u, universe_u, &pi_reduced
		) != 0) {
		return 7;
	}
	prototype_term_normalization_cache_clear(&term_db);
	if (expect_status(
			&term_db,
			&type_db,
			pi_with_redex,
			pi_reduced,
			3,
			PROTOTYPE_TERM_CONVERSION_EXHAUSTED,
			PROTOTYPE_TERM_CONVERSION_REASON_STEP_LIMIT
		) != 0) {
		return 8;
	}

	uint32_t continuation_binder = prototype_term_new_binding(&term_db);
	uint32_t continuation_var;
	uint32_t continuation_lambda;
	uint32_t continuation_thunk;
	uint32_t operation;
	uint32_t request;
	if (continuation_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(
			&term_db, continuation_binder, &continuation_var
		) != 0 || prototype_term_lambda(
			&term_db,
			continuation_binder,
			continuation_var,
			&continuation_lambda
		) != 0 || prototype_term_thunk(
			&term_db, continuation_lambda, &continuation_thunk
		) != 0 || prototype_term_effect_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, &operation
		) != 0 || prototype_term_operation_request(
			&term_db,
			operation,
			universe_u,
			continuation_thunk,
			&request
		) != 0) {
		return 9;
	}
	prototype_term_normalization_cache_clear(&term_db);
	if (expect_status(
			&term_db,
			&type_db,
			request,
			universe_u,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT,
			PROTOTYPE_TERM_CONVERSION_REASON_EFFECT_REQUEST
		) != 0) {
		return 10;
	}

	struct prototype_term_conversion_result invalid;
	if (prototype_term_compare_for_conversion(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			term_db.term_count,
			universe_u,
			UINT64_MAX,
			&invalid
		) != 0 || invalid.status != PROTOTYPE_TERM_CONVERSION_INVALID ||
		invalid.reason != PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH) {
		return 11;
	}

	struct prototype_term_conversion_result universe_result =
		prototype_judgement_classifier_conversion(
			&term_db, &type_db, universe_u, universe_v
		);
	if (universe_result.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return 12;
	}
	return 0;
}
