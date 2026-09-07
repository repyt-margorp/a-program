#include "a_program/core/term.h"

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
	struct prototype_ih_scope ih_scopes[MATCH_FRAME_CAPACITY];
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
		storage->ih_scopes,
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
	uint32_t frame = prototype_term_new_ih_scope(db);
	uint32_t binder = prototype_term_new_binding(db);
	uint32_t variable;
	uint32_t ih;
	struct prototype_case_binder case_binder;
	struct prototype_match_case_input match_case;
	if (frame == PROTOTYPE_INVALID_ID || binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, binder, &variable) != 0 ||
		prototype_term_induction_hypothesis(db, frame, variable, &ih) != 0) {
		return -1;
	}
	case_binder.binding_id = binder;
	case_binder.is_recursive = 1;
	match_case.case_label_symbol_id = 1;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = constructor_id;
	match_case.binders = &case_binder;
	match_case.binder_count = 1;
	match_case.body = ih;
	if (prototype_term_match_with_ih_scope(
			db, scrutinee, &match_case, 1, frame, p_match
		) != 0 || prototype_term_set_ih_scope_term(db, frame, *p_match) != 0) {
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
	uint32_t enclosing_frame = prototype_term_new_ih_scope(db);
	uint32_t foreign_frame = prototype_term_new_ih_scope(db);
	uint32_t binder = prototype_term_new_binding(db);
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
	case_binder.binding_id = binder;
	case_binder.is_recursive = 1;
	match_case.case_label_symbol_id = 1;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = 1;
	match_case.binders = &case_binder;
	match_case.binder_count = 1;
	match_case.body = ih;
	return prototype_term_match_with_ih_scope(
		db, scrutinee, &match_case, 1, enclosing_frame, p_match
	);
}

static int build_match_with_binder_role(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	int is_recursive,
	uint32_t* p_match
) {
	uint32_t binder = prototype_term_new_binding(db);
	uint32_t variable;
	struct prototype_case_binder case_binder;
	struct prototype_match_case_input match_case;
	if (binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, binder, &variable) != 0) {
		return -1;
	}
	case_binder.binding_id = binder;
	case_binder.is_recursive = is_recursive;
	match_case.case_label_symbol_id = 3;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = 3;
	match_case.binders = &case_binder;
	match_case.binder_count = 1;
	match_case.body = variable;
	return prototype_term_match(db, scrutinee, &match_case, 1, p_match);
}

static int build_subject_binding_match(
	struct prototype_term_db* db,
	uint32_t subject_binding,
	uint32_t* p_match
) {
	uint32_t frame = prototype_term_new_ih_scope(db);
	uint32_t subject;
	struct prototype_match_case_input match_case;
	if (frame == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, subject_binding, &subject) != 0) {
		return -1;
	}
	match_case.case_label_symbol_id = 4;
	match_case.constructor_owner = PROTOTYPE_INVALID_ID;
	match_case.constructor_id = 4;
	match_case.binders = NULL;
	match_case.binder_count = 0;
	match_case.body = subject;
	if (prototype_term_match_with_ih_scope(
			db, subject, &match_case, 1, frame, p_match
		) != 0) {
		return -1;
	}
	return prototype_term_set_ih_scope_term(db, frame, *p_match);
}

int main(void) {
	struct prototype_term_db left_db;
	struct prototype_term_db right_db;
	struct test_term_storage left_storage;
	struct test_term_storage right_storage;
	init_term_db(&left_db, &left_storage);
	init_term_db(&right_db, &right_storage);

	uint32_t left_binder = prototype_term_new_binding(&left_db);
	uint32_t right_binder = prototype_term_new_binding(&left_db);
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

	uint32_t subject_binding = prototype_term_new_binding(&left_db);
	uint32_t subject_match;
	uint32_t concrete;
	uint32_t concrete_match;
	if (subject_binding == PROTOTYPE_INVALID_ID) {
		return 15;
	}
	if (build_subject_binding_match(
			&left_db, subject_binding, &subject_match
		) != 0) {
		return 15;
	}
	if (prototype_term_int_literal(&left_db, 7, &concrete) != 0) {
		return 15;
	}
	if (prototype_term_graph_substitute_bound_var(
			&left_db,
			subject_match,
			subject_binding,
			concrete,
			&concrete_match
		) != 0) {
		return 15;
	}
	if (concrete_match == subject_match) {
		return 15;
	}
	const struct prototype_term* concrete_match_term =
		&left_db.terms[concrete_match];
	if (concrete_match_term->tag != PROTOTYPE_TERM_MATCH) {
		return 16;
	}
	if (concrete_match_term->as.match.scrutinee != concrete) {
		return 16;
	}
	if (concrete_match_term->as.match.ih_scope_id >= left_db.ih_scope_count) {
		return 16;
	}
	const struct prototype_ih_scope* concrete_scope = &left_db.ih_scopes[
		concrete_match_term->as.match.ih_scope_id
	];
	const struct prototype_match_case* concrete_case = &left_db.cases[
		concrete_match_term->as.match.first_case
	];
	if (concrete_scope->scrutinee_binding_id != subject_binding) {
		return 17;
	}
	if (concrete_case->body >= left_db.term_count) {
		return 17;
	}
	if (left_db.terms[concrete_case->body].tag != PROTOTYPE_TERM_VAR) {
		return 17;
	}
	if (left_db.terms[concrete_case->body].as.var.binding_id != subject_binding) {
		return 17;
	}
	if (prototype_term_contains_free_binding(
			&left_db, concrete_match, subject_binding
		)) {
		return 17;
	}
	struct prototype_term_canonical_key concrete_key;
	if (prototype_term_canonical_key(
			&left_db, concrete_match, &concrete_key
		) != 0) {
		return 18;
	}
	if (concrete_key.free_binder_count != 0) {
		return 18;
	}

	uint32_t renamed_binding = prototype_term_new_binding(&left_db);
	uint32_t renamed_var;
	uint32_t renamed_match;
	uint32_t closed_renamed_match;
	struct prototype_term_canonical_key renamed_key;
	if (renamed_binding == PROTOTYPE_INVALID_ID) {
		return 19;
	}
	if (prototype_term_var(&left_db, renamed_binding, &renamed_var) != 0) {
		return 19;
	}
	if (prototype_term_graph_substitute_bound_var(
			&left_db,
			subject_match,
			subject_binding,
			renamed_var,
			&renamed_match
		) != 0) {
		return 19;
	}
	if (left_db.terms[renamed_match].tag != PROTOTYPE_TERM_MATCH) {
		return 19;
	}
	const struct prototype_term* renamed_match_term = &left_db.terms[renamed_match];
	const struct prototype_ih_scope* renamed_scope = &left_db.ih_scopes[
		renamed_match_term->as.match.ih_scope_id
	];
	const struct prototype_match_case* renamed_case = &left_db.cases[
		renamed_match_term->as.match.first_case
	];
	if (renamed_match_term->as.match.scrutinee != renamed_var) {
		return 20;
	}
	if (renamed_scope->scrutinee_binding_id != renamed_binding) {
		return 20;
	}
	if (renamed_case->body != renamed_var) {
		return 20;
	}
	if (prototype_term_lambda(
			&left_db, renamed_binding, renamed_match, &closed_renamed_match
		) != 0) {
		return 20;
	}
	if (prototype_term_canonical_key(
			&left_db, closed_renamed_match, &renamed_key
		) != 0) {
		return 20;
	}
	if (renamed_key.free_binder_count != 0) {
		return 20;
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
	struct prototype_ih_scope_key left_frame_key;
	struct prototype_ih_scope_key second_frame_key;
	if (prototype_term_ih_scope_key(
			&left_db, left_frame, &left_frame_key
		) != 0 || prototype_term_ih_scope_key(
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

	uint32_t wrapper_binder = prototype_term_new_binding(&left_db);
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
			&left_db, left_match,
			&right_db, right_match_id,
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
			&left_db, left_free_ih,
			&right_db, right_free_ih,
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
	right_db.ih_scopes[mismatched_frame].key = left_frame_key;
	if (prototype_term_induction_hypothesis(
			&right_db, mismatched_frame, right_argument, &mismatched_free_ih
		) != 0 || prototype_term_view_shape_equal_for_link(
			&left_db, left_free_ih,
			&right_db, mismatched_free_ih,
			&equal
		) != 0 || equal) {
		return 13;
	}

	uint32_t recursive_role_match;
	uint32_t plain_role_match;
	if (build_match_with_binder_role(
			&left_db, left_scrutinee, 1, &recursive_role_match
		) != 0 || build_match_with_binder_role(
			&right_db, right_scrutinee, 0, &plain_role_match
		) != 0 || prototype_term_view_shape_equal_for_link(
			&left_db, recursive_role_match,
			&right_db, plain_role_match,
			&equal
		) != 0 || equal) {
		return 14;
	}

	return 0;
}
