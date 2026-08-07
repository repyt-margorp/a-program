#include "term.h"
#include "type_declaration.h"

#include <stdint.h>
#include <stdio.h>

#define TERM_CAPACITY 512
#define CASE_CAPACITY 64
#define CASE_BINDER_CAPACITY 64
#define MATCH_FRAME_CAPACITY 32
#define TYPE_CAPACITY 4
#define CONSTRUCTOR_CAPACITY 4
#define PARAMETER_CAPACITY 4
#define FIELD_TYPE_CAPACITY 4
#define TYPE_EXPR_CAPACITY 4

struct test_storage {
	struct prototype_term terms[TERM_CAPACITY];
	struct prototype_match_case cases[CASE_CAPACITY];
	int case_label_symbols[CASE_CAPACITY];
	struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scopes[MATCH_FRAME_CAPACITY];
	struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
	struct prototype_type_constructor_declaration constructors[CONSTRUCTOR_CAPACITY];
	struct prototype_type_parameter_declaration parameters[PARAMETER_CAPACITY];
	uint32_t field_types[FIELD_TYPE_CAPACITY];
	struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];
};

static void init_databases(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* types,
	struct test_storage* storage
) {
	prototype_term_db_init(
		terms,
		storage->terms,
		TERM_CAPACITY,
		storage->cases,
		storage->case_label_symbols,
		CASE_CAPACITY,
		storage->case_binders,
		CASE_BINDER_CAPACITY,
		storage->ih_scopes,
		MATCH_FRAME_CAPACITY
	);
	prototype_type_declaration_db_init(
		types,
		storage->type_declarations,
		TYPE_CAPACITY,
		storage->constructors,
		CONSTRUCTOR_CAPACITY,
		storage->parameters,
		PARAMETER_CAPACITY,
		storage->field_types,
		FIELD_TYPE_CAPACITY,
		storage->type_exprs,
		TYPE_EXPR_CAPACITY
	);
}

static int conversion_status(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* types,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	int expected
) {
	struct prototype_term_conversion_result result;
	if (prototype_term_compare_for_conversion(
			terms,
			types,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			left,
			right,
			step_limit,
			&result
		) != 0 || result.status != expected) {
		fprintf(
			stderr,
			"conversion status mismatch: expected %s, got %s (%s)\n",
			prototype_term_conversion_status_name(expected),
			prototype_term_conversion_status_name(result.status),
			prototype_term_conversion_reason_name(result.reason)
		);
		return -1;
	}
	return 0;
}

static int build_recursive_match(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	int beta_wrap_body,
	uint32_t* p_frame,
	uint32_t* p_match
) {
	uint32_t frame = prototype_term_new_ih_scope(db);
	uint32_t case_binder = prototype_term_new_binding(db);
	uint32_t case_variable;
	uint32_t body;
	if (frame == PROTOTYPE_INVALID_ID || case_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, case_binder, &case_variable) != 0 ||
		prototype_term_induction_hypothesis(db, frame, case_variable, &body) != 0) {
		return -1;
	}

	if (beta_wrap_body) {
		uint32_t identity_binder = prototype_term_new_binding(db);
		uint32_t identity_variable;
		uint32_t identity;
		uint32_t application;
		if (identity_binder == PROTOTYPE_INVALID_ID ||
			prototype_term_var(db, identity_binder, &identity_variable) != 0 ||
			prototype_term_lambda(db, identity_binder, identity_variable, &identity) != 0 ||
			prototype_term_app(db, identity, body, &application) != 0) {
			return -1;
		}
		body = application;
	}

	struct prototype_case_binder binder;
	binder.binding_id = case_binder;
	binder.is_recursive = 1;
	struct prototype_match_case_input match_case;
	match_case.case_label_symbol_id = 1;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = 1;
	match_case.binders = &binder;
	match_case.binder_count = 1;
	match_case.body = body;
	if (prototype_term_match_with_ih_scope(
			db, scrutinee, &match_case, 1, frame, p_match
		) != 0 || prototype_term_set_ih_scope_term(db, frame, *p_match) != 0) {
		return -1;
	}
	*p_frame = frame;
	return 0;
}

static int build_binder_role_match(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	int is_recursive,
	uint32_t* p_match
) {
	uint32_t binding_id = prototype_term_new_binding(db);
	uint32_t body;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, binding_id, &body) != 0) {
		return -1;
	}
	struct prototype_case_binder binder;
	binder.binding_id = binding_id;
	binder.is_recursive = is_recursive;
	struct prototype_match_case_input match_case;
	match_case.case_label_symbol_id = 2;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = 2;
	match_case.binders = &binder;
	match_case.binder_count = 1;
	match_case.body = body;
	return prototype_term_match(db, scrutinee, &match_case, 1, p_match);
}

static int build_frame_presence_match(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	uint32_t ih_scope,
	uint32_t body_frame,
	uint32_t* p_match
) {
	uint32_t binding_id = prototype_term_new_binding(db);
	uint32_t variable;
	uint32_t body;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, binding_id, &variable) != 0) {
		return -1;
	}
	body = variable;
	if (body_frame != PROTOTYPE_INVALID_ID &&
		prototype_term_induction_hypothesis(
			db, body_frame, variable, &body
		) != 0) {
		return -1;
	}
	struct prototype_case_binder binder;
	binder.binding_id = binding_id;
	binder.is_recursive = body_frame != PROTOTYPE_INVALID_ID;
	struct prototype_match_case_input match_case;
	match_case.case_label_symbol_id = 3;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = 3;
	match_case.binders = &binder;
	match_case.binder_count = 1;
	match_case.body = body;
	return prototype_term_match_with_ih_scope(
		db, scrutinee, &match_case, 1, ih_scope, p_match
	);
}

static int build_nested_recursive_match(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	int cross_frames,
	int beta_wrap_body,
	uint32_t* p_match
) {
	uint32_t outer_frame = prototype_term_new_ih_scope(db);
	uint32_t inner_frame = prototype_term_new_ih_scope(db);
	uint32_t outer_binder = prototype_term_new_binding(db);
	uint32_t inner_binder = prototype_term_new_binding(db);
	uint32_t outer_variable;
	uint32_t inner_variable;
	uint32_t outer_ih;
	uint32_t inner_ih;
	uint32_t inner_body;
	if (outer_frame == PROTOTYPE_INVALID_ID ||
		inner_frame == PROTOTYPE_INVALID_ID ||
		outer_binder == PROTOTYPE_INVALID_ID ||
		inner_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, outer_binder, &outer_variable) != 0 ||
		prototype_term_var(db, inner_binder, &inner_variable) != 0 ||
		prototype_term_induction_hypothesis(
			db, outer_frame, outer_variable, &outer_ih
		) != 0 || prototype_term_induction_hypothesis(
			db, inner_frame, inner_variable, &inner_ih
		) != 0 || prototype_term_app(
			db,
			cross_frames ? inner_ih : outer_ih,
			cross_frames ? outer_ih : inner_ih,
			&inner_body
		) != 0) {
		return -1;
	}
	if (beta_wrap_body) {
		uint32_t identity_binder = prototype_term_new_binding(db);
		uint32_t identity_variable;
		uint32_t identity;
		if (identity_binder == PROTOTYPE_INVALID_ID ||
			prototype_term_var(db, identity_binder, &identity_variable) != 0 ||
			prototype_term_lambda(
				db, identity_binder, identity_variable, &identity
			) != 0 || prototype_term_app(
				db, identity, inner_body, &inner_body
			) != 0) {
			return -1;
		}
	}
	struct prototype_case_binder inner_case_binder;
	inner_case_binder.binding_id = inner_binder;
	inner_case_binder.is_recursive = 1;
	struct prototype_match_case_input inner_case;
	inner_case.case_label_symbol_id = 4;
	inner_case.constructor_owner = PROTOTYPE_INVALID_ID;
	inner_case.constructor_id = 4;
	inner_case.binders = &inner_case_binder;
	inner_case.binder_count = 1;
	inner_case.body = inner_body;
	uint32_t inner_match;
	if (prototype_term_match_with_ih_scope(
			db, outer_variable, &inner_case, 1, inner_frame, &inner_match
		) != 0 || prototype_term_set_ih_scope_term(
			db, inner_frame, inner_match
		) != 0) {
		return -1;
	}

	struct prototype_case_binder outer_case_binder;
	outer_case_binder.binding_id = outer_binder;
	outer_case_binder.is_recursive = 1;
	struct prototype_match_case_input outer_case;
	outer_case.case_label_symbol_id = 5;
	outer_case.constructor_owner = PROTOTYPE_INVALID_ID;
	outer_case.constructor_id = 5;
	outer_case.binders = &outer_case_binder;
	outer_case.binder_count = 1;
	outer_case.body = inner_match;
	if (prototype_term_match_with_ih_scope(
			db, scrutinee, &outer_case, 1, outer_frame, p_match
		) != 0 || prototype_term_set_ih_scope_term(
			db, outer_frame, *p_match
		) != 0) {
		return -1;
	}
	return 0;
}

int main(void) {
	struct prototype_term_db terms;
	struct prototype_type_declaration_db types;
	struct test_storage storage;
	init_databases(&terms, &types, &storage);

	uint32_t scrutinee;
	uint32_t left_frame;
	uint32_t right_frame;
	uint32_t left_match;
	uint32_t right_match;
	if (prototype_term_primitive_int(&terms, &scrutinee) != 0 ||
		build_recursive_match(
			&terms, scrutinee, 0, &left_frame, &left_match
		) != 0 || build_recursive_match(
			&terms, scrutinee, 1, &right_frame, &right_match
		) != 0 || left_match == right_match) {
		fprintf(stderr, "failed to build distinct recursive Match terms\n");
		return 1;
	}
	if (conversion_status(
			&terms,
			&types,
			left_match,
			right_match,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_EQUAL
		) != 0) {
		fprintf(stderr, "recursive Match conversion did not respect frame scope\n");
		return 2;
	}

	uint32_t same_shape_frame;
	uint32_t same_shape_match;
	if (build_recursive_match(
			&terms, scrutinee, 0, &same_shape_frame, &same_shape_match
		) != 0 || same_shape_match != left_match) {
		fprintf(stderr, "failed to build alpha-interned recursive Match\n");
		return 3;
	}
	uint32_t argument;
	uint32_t left_free_ih;
	uint32_t right_free_ih;
	if (prototype_term_int_literal(&terms, 0, &argument) != 0 ||
		prototype_term_induction_hypothesis(
			&terms, left_frame, argument, &left_free_ih
		) != 0 || prototype_term_induction_hypothesis(
			&terms, same_shape_frame, argument, &right_free_ih
		) != 0 || left_free_ih == right_free_ih) {
		fprintf(stderr, "failed to build isolated IH terms\n");
		return 4;
	}
	if (conversion_status(
			&terms,
			&types,
			left_free_ih,
			right_free_ih,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_NOT_EQUAL
		) != 0) {
		fprintf(stderr, "unmapped IH frames were equated through canonical keys\n");
		return 5;
	}

	uint32_t left_row_binder = prototype_term_new_binding(&terms);
	uint32_t right_row_binder = prototype_term_new_binding(&terms);
	uint32_t identity_binder = prototype_term_new_binding(&terms);
	uint32_t left_row;
	uint32_t right_row;
	uint32_t identity_variable;
	uint32_t identity;
	uint32_t beta_row;
	uint32_t left_forall;
	uint32_t right_forall;
	if (left_row_binder == PROTOTYPE_INVALID_ID ||
		right_row_binder == PROTOTYPE_INVALID_ID ||
		identity_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_effect_row_var(&terms, left_row_binder, &left_row) != 0 ||
		prototype_term_effect_row_var(&terms, right_row_binder, &right_row) != 0 ||
		prototype_term_var(&terms, identity_binder, &identity_variable) != 0 ||
		prototype_term_lambda(&terms, identity_binder, identity_variable, &identity) != 0 ||
		prototype_term_app(&terms, identity, left_row, &beta_row) != 0 ||
		prototype_term_effect_row_forall(
			&terms, left_row_binder, beta_row, &left_forall
		) != 0 || prototype_term_effect_row_forall(
			&terms, right_row_binder, right_row, &right_forall
		) != 0 || left_forall == right_forall) {
		fprintf(stderr, "failed to build distinct convertible forall rows\n");
		return 6;
	}
	if (conversion_status(
			&terms,
			&types,
			left_forall,
			right_forall,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_EQUAL
		) != 0) {
		fprintf(stderr, "forall conversion did not reduce under its binder\n");
		return 7;
	}
	if (conversion_status(
			&terms,
			&types,
			left_row,
			right_row,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_NOT_EQUAL
		) != 0) {
		fprintf(stderr, "free effect-row variables became convertible\n");
		return 8;
	}
	prototype_term_normalization_cache_clear(&terms);
	if (conversion_status(
			&terms,
			&types,
			left_forall,
			right_forall,
			0,
			PROTOTYPE_TERM_CONVERSION_EXHAUSTED
		) != 0) {
		fprintf(stderr, "forall body exhaustion was not propagated\n");
		return 13;
	}

	uint32_t left_outer = prototype_term_new_binding(&terms);
	uint32_t left_inner = prototype_term_new_binding(&terms);
	uint32_t right_outer = prototype_term_new_binding(&terms);
	uint32_t right_inner = prototype_term_new_binding(&terms);
	uint32_t left_outer_row;
	uint32_t right_outer_row;
	uint32_t right_inner_row;
	uint32_t left_nested_inner;
	uint32_t right_nested_inner;
	uint32_t crossed_nested_inner;
	uint32_t left_nested;
	uint32_t right_nested;
	uint32_t crossed_nested;
	if (left_outer == PROTOTYPE_INVALID_ID || left_inner == PROTOTYPE_INVALID_ID ||
		right_outer == PROTOTYPE_INVALID_ID || right_inner == PROTOTYPE_INVALID_ID ||
		prototype_term_effect_row_var(&terms, left_outer, &left_outer_row) != 0 ||
		prototype_term_effect_row_var(&terms, right_outer, &right_outer_row) != 0 ||
		prototype_term_effect_row_var(&terms, right_inner, &right_inner_row) != 0 ||
		prototype_term_effect_row_forall(
			&terms, left_inner, left_outer_row, &left_nested_inner
		) != 0 || prototype_term_effect_row_forall(
			&terms, right_inner, right_outer_row, &right_nested_inner
		) != 0 || prototype_term_effect_row_forall(
			&terms, right_inner, right_inner_row, &crossed_nested_inner
		) != 0 || prototype_term_effect_row_forall(
			&terms, left_outer, left_nested_inner, &left_nested
		) != 0 || prototype_term_effect_row_forall(
			&terms, right_outer, right_nested_inner, &right_nested
		) != 0 || prototype_term_effect_row_forall(
			&terms, right_outer, crossed_nested_inner, &crossed_nested
		) != 0 || conversion_status(
			&terms,
			&types,
			left_nested,
			right_nested,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_EQUAL
		) != 0 || conversion_status(
			&terms,
			&types,
			left_nested,
			crossed_nested,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_NOT_EQUAL
		) != 0) {
		fprintf(stderr, "nested forall scopes crossed binder mappings\n");
		return 14;
	}

	uint32_t continuation_binder = prototype_term_new_binding(&terms);
	uint32_t continuation_variable;
	uint32_t continuation_lambda;
	uint32_t continuation_thunk;
	uint32_t operation;
	uint32_t request;
	uint32_t request_forall;
	uint32_t plain_forall;
	if (continuation_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(
			&terms, continuation_binder, &continuation_variable
		) != 0 || prototype_term_lambda(
			&terms,
			continuation_binder,
			continuation_variable,
			&continuation_lambda
		) != 0 || prototype_term_thunk(
			&terms, continuation_lambda, &continuation_thunk
		) != 0 || prototype_term_effect_operation(
			&terms, PROTOTYPE_EFFECT_OPERATION_PRINT, &operation
		) != 0 || prototype_term_operation_request(
			&terms,
			operation,
			scrutinee,
			continuation_thunk,
			&request
		) != 0 || prototype_term_effect_row_forall(
			&terms, left_outer, request, &request_forall
		) != 0 || prototype_term_effect_row_forall(
			&terms, right_outer, right_outer_row, &plain_forall
		) != 0 || conversion_status(
			&terms,
			&types,
			request_forall,
			plain_forall,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT
		) != 0) {
		fprintf(stderr, "forall body blocked-effect status was not propagated\n");
		return 15;
	}

	uint32_t recursive_binder_match;
	uint32_t plain_binder_match;
	int shape_equal = 1;
	if (build_binder_role_match(
			&terms, scrutinee, 1, &recursive_binder_match
		) != 0 || build_binder_role_match(
			&terms, scrutinee, 0, &plain_binder_match
		) != 0 || recursive_binder_match == plain_binder_match ||
		prototype_term_view_shape_equal(
			&terms, recursive_binder_match, plain_binder_match, &shape_equal
		) != 0 || shape_equal) {
		fprintf(stderr, "Match binder roles were erased by local interning\n");
		return 16;
	}
	if (conversion_status(
			&terms,
			&types,
			recursive_binder_match,
			plain_binder_match,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_NOT_EQUAL
		) != 0) {
		fprintf(stderr, "Match binder roles were erased by conversion\n");
		return 17;
	}

	uint32_t vacuous_frame = prototype_term_new_ih_scope(&terms);
	uint32_t framed_match;
	uint32_t unframed_match;
	if (vacuous_frame == PROTOTYPE_INVALID_ID || build_frame_presence_match(
			&terms,
			scrutinee,
			vacuous_frame,
			PROTOTYPE_INVALID_ID,
			&framed_match
		) != 0 || build_frame_presence_match(
			&terms,
			scrutinee,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			&unframed_match
		) != 0 || conversion_status(
			&terms,
			&types,
			framed_match,
			unframed_match,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_EQUAL
		) != 0) {
		fprintf(stderr, "a vacuous internal Match frame changed conversion\n");
		return 18;
	}

	uint32_t used_frame = prototype_term_new_ih_scope(&terms);
	uint32_t owning_match;
	uint32_t foreign_match;
	if (used_frame == PROTOTYPE_INVALID_ID || build_frame_presence_match(
			&terms, scrutinee, used_frame, used_frame, &owning_match
		) != 0 || prototype_term_set_ih_scope_term(
			&terms, used_frame, owning_match
		) != 0 || build_frame_presence_match(
			&terms,
			scrutinee,
			PROTOTYPE_INVALID_ID,
			used_frame,
			&foreign_match
		) != 0 || conversion_status(
			&terms,
			&types,
			owning_match,
			foreign_match,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_NOT_EQUAL
		) != 0) {
		fprintf(stderr, "an enclosing Match frame was treated as a free frame\n");
		return 19;
	}

	uint32_t left_nested_match;
	uint32_t right_nested_match;
	uint32_t crossed_nested_match;
	if (build_nested_recursive_match(
			&terms, scrutinee, 0, 0, &left_nested_match
		) != 0 || build_nested_recursive_match(
			&terms, scrutinee, 0, 1, &right_nested_match
		) != 0 || build_nested_recursive_match(
			&terms, scrutinee, 1, 1, &crossed_nested_match
		) != 0 || conversion_status(
			&terms,
			&types,
			left_nested_match,
			right_nested_match,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_EQUAL
		) != 0 || conversion_status(
			&terms,
			&types,
			left_nested_match,
			crossed_nested_match,
			UINT64_MAX,
			PROTOTYPE_TERM_CONVERSION_NOT_EQUAL
		) != 0) {
		fprintf(stderr, "nested Match frames crossed lexical pairings\n");
		return 20;
	}

	return 0;
}
