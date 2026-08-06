#include "term.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 32
#define CASE_BINDER_CAPACITY 32
#define MATCH_FRAME_CAPACITY 16

struct test_term_storage {
	struct prototype_term terms[TERM_CAPACITY];
	struct prototype_match_case cases[CASE_CAPACITY];
	int case_label_symbols[CASE_CAPACITY];
	struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
	struct prototype_match_frame match_frames[MATCH_FRAME_CAPACITY];
};

static void init_term_db(
	struct prototype_term_db* db,
	struct test_term_storage* storage
) {
	prototype_term_db_init(
		db,
		storage->terms,
		TERM_CAPACITY,
		storage->cases,
		storage->case_label_symbols,
		CASE_CAPACITY,
		storage->case_binders,
		CASE_BINDER_CAPACITY,
		storage->match_frames,
		MATCH_FRAME_CAPACITY
	);
}

static int build_recursive_match_with_constructor(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	uint32_t constructor_id,
	uint32_t* p_frame,
	uint32_t* p_match
) {
	uint32_t frame = prototype_term_new_match_frame(db);
	uint32_t binder = prototype_term_fresh_binder(db);
	uint32_t variable;
	uint32_t ih;
	struct prototype_case_binder case_binder;
	struct prototype_match_case_input match_case;
	if (frame == PROTOTYPE_INVALID_ID || binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, binder, &variable) != 0 ||
		prototype_term_induction_hypothesis(db, frame, variable, &ih) != 0) {
		return -1;
	}
	case_binder.binder_id = binder;
	case_binder.is_recursive = 1;
	match_case.case_label_symbol_id = 1;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = constructor_id;
	match_case.binders = &case_binder;
	match_case.binder_count = 1;
	match_case.body = ih;
	if (prototype_term_match_with_frame(
			db, scrutinee, &match_case, 1, frame, p_match
		) != 0 || prototype_term_set_match_frame_term(db, frame, *p_match) != 0) {
		return -1;
	}
	*p_frame = frame;
	return 0;
}

static int build_recursive_match(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	uint32_t* p_frame,
	uint32_t* p_match
) {
	return build_recursive_match_with_constructor(
		db, scrutinee, 1, p_frame, p_match
	);
}

static int build_match_with_foreign_ih(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	uint32_t* p_match
) {
	uint32_t enclosing_frame = prototype_term_new_match_frame(db);
	uint32_t foreign_frame = prototype_term_new_match_frame(db);
	uint32_t binder = prototype_term_fresh_binder(db);
	uint32_t variable;
	uint32_t ih;
	struct prototype_case_binder case_binder;
	struct prototype_match_case_input match_case;
	if (enclosing_frame == PROTOTYPE_INVALID_ID ||
		foreign_frame == PROTOTYPE_INVALID_ID || binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, binder, &variable) != 0 ||
		prototype_term_induction_hypothesis(db, foreign_frame, variable, &ih) != 0) {
		return -1;
	}
	case_binder.binder_id = binder;
	case_binder.is_recursive = 1;
	match_case.case_label_symbol_id = 1;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = 1;
	match_case.binders = &case_binder;
	match_case.binder_count = 1;
	match_case.body = ih;
	return prototype_term_match_with_frame(
		db, scrutinee, &match_case, 1, enclosing_frame, p_match
	);
}

int main(void) {
	struct prototype_term_db left_db;
	struct prototype_term_db right_db;
	struct test_term_storage left_storage;
	struct test_term_storage right_storage;
	init_term_db(&left_db, &left_storage);
	init_term_db(&right_db, &right_storage);

	uint32_t left_binder = prototype_term_fresh_binder(&left_db);
	uint32_t right_binder = prototype_term_fresh_binder(&left_db);
	uint32_t left_var;
	uint32_t right_var;
	uint32_t left_lambda;
	uint32_t right_lambda;
	if (prototype_term_var(&left_db, left_binder, &left_var) != 0 ||
		prototype_term_var(&left_db, right_binder, &right_var) != 0 ||
		left_var == right_var ||
		prototype_term_lambda(&left_db, left_binder, left_var, &left_lambda) != 0 ||
		prototype_term_lambda(&left_db, right_binder, right_var, &right_lambda) != 0 ||
		left_lambda != right_lambda) {
		return 1;
	}

	uint32_t left_scrutinee;
	uint32_t left_frame;
	uint32_t second_left_frame;
	uint32_t left_match;
	uint32_t second_left_match;
	if (prototype_term_primitive_int(&left_db, &left_scrutinee) != 0 ||
		build_recursive_match(
			&left_db, left_scrutinee, &left_frame, &left_match
		) != 0 ||
		build_recursive_match(
			&left_db, left_scrutinee, &second_left_frame, &second_left_match
		) != 0 ||
		left_match != second_left_match) {
		return 2;
	}
	struct prototype_match_frame_key left_frame_key;
	struct prototype_match_frame_key second_frame_key;
	if (prototype_term_match_frame_key(
			&left_db, left_frame, &left_frame_key
		) != 0 || prototype_term_match_frame_key(
			&left_db, second_left_frame, &second_frame_key
		) != 0 || left_frame_key.match_key.hash != second_frame_key.match_key.hash ||
		left_frame_key.match_key.node_count != second_frame_key.match_key.node_count ||
		left_frame_key.case_count != second_frame_key.case_count) {
		return 10;
	}

	uint32_t shared_argument;
	uint32_t left_free_ih;
	uint32_t second_free_ih;
	if (prototype_term_int_literal(&left_db, 0, &shared_argument) != 0 ||
		prototype_term_induction_hypothesis(
			&left_db, left_frame, shared_argument, &left_free_ih
		) != 0 ||
		prototype_term_induction_hypothesis(
			&left_db, second_left_frame, shared_argument, &second_free_ih
		) != 0 || left_free_ih == second_free_ih) {
		return 3;
	}
	int equal = 0;
	if (prototype_term_view_shape_equal(
			&left_db, left_free_ih, second_free_ih, &equal
		) != 0 || equal) {
		return 9;
	}

	uint32_t wrapper_binder = prototype_term_fresh_binder(&left_db);
	uint32_t left_wrapper;
	uint32_t right_wrapper;
	if (prototype_term_lambda(
			&left_db, wrapper_binder, left_free_ih, &left_wrapper
		) != 0 || prototype_term_lambda(
			&left_db, wrapper_binder, second_free_ih, &right_wrapper
		) != 0 || left_wrapper == right_wrapper) {
		return 4;
	}
	uint32_t left_app_wrapper;
	uint32_t right_app_wrapper;
	uint32_t left_thunk_wrapper;
	uint32_t right_thunk_wrapper;
	if (prototype_term_app(
			&left_db, left_free_ih, shared_argument, &left_app_wrapper
		) != 0 || prototype_term_app(
			&left_db, second_free_ih, shared_argument, &right_app_wrapper
		) != 0 || left_app_wrapper == right_app_wrapper ||
		prototype_term_thunk(
			&left_db, left_free_ih, &left_thunk_wrapper
		) != 0 || prototype_term_thunk(
			&left_db, second_free_ih, &right_thunk_wrapper
		) != 0 || left_thunk_wrapper == right_thunk_wrapper) {
		return 11;
	}

	uint32_t foreign_match;
	if (build_match_with_foreign_ih(
			&left_db, left_scrutinee, &foreign_match
		) != 0 || foreign_match == left_match) {
		return 5;
	}

	uint32_t right_scrutinee;
	uint32_t right_frame_id;
	uint32_t right_match_id;
	if (prototype_term_primitive_int(&right_db, &right_scrutinee) != 0 ||
		build_recursive_match(
			&right_db, right_scrutinee, &right_frame_id, &right_match_id
		) != 0) {
		return 6;
	}
	if (prototype_term_view_shape_equal_for_link(
			&left_db, NULL, left_match,
			&right_db, NULL, right_match_id,
			&equal
		) != 0 || !equal) {
		return 7;
	}

	uint32_t right_argument;
	uint32_t right_free_ih;
	if (prototype_term_int_literal(&right_db, 0, &right_argument) != 0 ||
		prototype_term_induction_hypothesis(
			&right_db, right_frame_id, right_argument, &right_free_ih
		) != 0 || prototype_term_view_shape_equal_for_link(
			&left_db, NULL, left_free_ih,
			&right_db, NULL, right_free_ih,
			&equal
		) != 0 || !equal) {
		return 8;
	}

	uint32_t mismatched_frame;
	uint32_t mismatched_match;
	uint32_t mismatched_free_ih;
	if (build_recursive_match_with_constructor(
			&right_db,
			right_scrutinee,
			2,
			&mismatched_frame,
			&mismatched_match
		) != 0 || mismatched_match == right_match_id) {
		return 12;
	}
	/* Simulate a canonical-key collision. Link equality must still inspect the
	 * referenced Match graph and reject the different constructor case. */
	right_db.match_frames[mismatched_frame].key = left_frame_key;
	if (prototype_term_induction_hypothesis(
			&right_db, mismatched_frame, right_argument, &mismatched_free_ih
		) != 0 || prototype_term_view_shape_equal_for_link(
			&left_db, NULL, left_free_ih,
			&right_db, NULL, mismatched_free_ih,
			&equal
		) != 0 || equal) {
		return 13;
	}

	return 0;
}
