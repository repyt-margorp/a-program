#include "a_program/core/term.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 16
#define CASE_BINDER_CAPACITY 16
#define IH_SCOPE_CAPACITY 8

int main(void) {
	struct prototype_term terms[TERM_CAPACITY];
	struct prototype_match_case cases[CASE_CAPACITY];
	int case_labels[CASE_CAPACITY];
	struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scopes[IH_SCOPE_CAPACITY];
	struct prototype_term_db db;
	prototype_term_db_init(
		&db,
		terms,
		TERM_CAPACITY,
		cases,
		case_labels,
		CASE_CAPACITY,
		case_binders,
		CASE_BINDER_CAPACITY,
		ih_scopes,
		IH_SCOPE_CAPACITY
	);

	uint32_t left_binder = prototype_term_new_binding(&db);
	uint32_t right_binder = prototype_term_new_binding(&db);
	uint32_t left_var;
	uint32_t right_var;
	uint32_t left_lambda;
	uint32_t right_lambda;
	uint32_t repeated_lambda;
	if (left_binder == PROTOTYPE_INVALID_ID ||
		right_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(&db, left_binder, &left_var) != 0 ||
		prototype_term_var(&db, right_binder, &right_var) != 0 ||
		prototype_term_lambda(&db, left_binder, left_var, &left_lambda) != 0 ||
		prototype_term_lambda(&db, right_binder, right_var, &right_lambda) != 0 ||
		prototype_term_lambda(
			&db, left_binder, left_var, &repeated_lambda
		) != 0 || left_lambda != right_lambda ||
		left_lambda != repeated_lambda) {
		prototype_term_db_dispose_runtime_state(&db);
		return 1;
	}

	uint32_t left_case_binder = prototype_term_new_binding(&db);
	uint32_t right_case_binder = prototype_term_new_binding(&db);
	uint32_t left_case_var;
	uint32_t right_case_var;
	uint32_t scrutinee;
	uint32_t left_match;
	uint32_t right_match;
	struct prototype_case_binder left_pattern = {
		.binding_id = left_case_binder,
		.is_recursive = 0
	};
	struct prototype_case_binder right_pattern = {
		.binding_id = right_case_binder,
		.is_recursive = 0
	};
	struct prototype_match_case_input left_case = {
		.case_label_symbol_id = 17,
		.constructor_owner = PROTOTYPE_INVALID_ID,
		.constructor_id = 3,
		.binders = &left_pattern,
		.binder_count = 1,
		.body = PROTOTYPE_INVALID_ID
	};
	struct prototype_match_case_input right_case = left_case;
	right_case.binders = &right_pattern;
	if (left_case_binder == PROTOTYPE_INVALID_ID ||
		right_case_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(&db, left_case_binder, &left_case_var) != 0 ||
		prototype_term_var(&db, right_case_binder, &right_case_var) != 0 ||
		prototype_term_int_literal(&db, 11, &scrutinee) != 0) {
		prototype_term_db_dispose_runtime_state(&db);
		return 2;
	}
	left_case.body = left_case_var;
	right_case.body = right_case_var;
	if (prototype_term_match(&db, scrutinee, &left_case, 1, &left_match) != 0 ||
		prototype_term_match(&db, scrutinee, &right_case, 1, &right_match) != 0 ||
		left_match != right_match) {
		prototype_term_db_dispose_runtime_state(&db);
		return 3;
	}

	uint32_t argument;
	uint32_t first_app;
	uint32_t repeated_app;
	if (prototype_term_int_literal(&db, 7, &argument) != 0 ||
		prototype_term_app(&db, left_lambda, argument, &first_app) != 0 ||
		prototype_term_app(&db, left_lambda, argument, &repeated_app) != 0 ||
		first_app != repeated_app) {
		prototype_term_db_dispose_runtime_state(&db);
		return 4;
	}

	struct prototype_term_intern_stats before_mutation;
	prototype_term_intern_get_stats(&db, &before_mutation);
	if (before_mutation.formation_request_count != 13 ||
		before_mutation.unique_term_count != db.term_count ||
		before_mutation.exact_probe_count == 0 ||
		before_mutation.alpha_compare_count == 0) {
		prototype_term_db_dispose_runtime_state(&db);
		return 5;
	}

	/* Runtime indexes are projections. A physical graph mutation invalidates
	 * them, and the next formation rebuilds from the authoritative Term graph. */
	db.terms[argument].as.int_literal.value = 8;
	prototype_term_notify_graph_mutation(&db);
	uint32_t rebuilt_literal;
	if (prototype_term_int_literal(&db, 8, &rebuilt_literal) != 0 ||
		rebuilt_literal != argument) {
		prototype_term_db_dispose_runtime_state(&db);
		return 6;
	}
	struct prototype_term_intern_stats after_mutation;
	prototype_term_intern_get_stats(&db, &after_mutation);
	if (after_mutation.index_rebuild_count <=
		before_mutation.index_rebuild_count) {
		prototype_term_db_dispose_runtime_state(&db);
		return 7;
	}

	/* Rebuilds may discover physical duplicates created by an approved bulk
	 * mutation. Lookup must still select the earliest canonical Term ID. */
	uint32_t later_literal;
	if (prototype_term_int_literal(&db, 9, &later_literal) != 0 ||
		later_literal <= argument) {
		prototype_term_db_dispose_runtime_state(&db);
		return 8;
	}
	db.terms[later_literal].as.int_literal.value = 8;
	prototype_term_notify_graph_mutation(&db);
	uint32_t earliest_literal;
	if (prototype_term_int_literal(&db, 8, &earliest_literal) != 0 ||
		earliest_literal != argument) {
		prototype_term_db_dispose_runtime_state(&db);
		return 9;
	}

	prototype_term_db_dispose_runtime_state(&db);
	if (db.intern_keys || db.intern_canonical_ids || db.intern_next ||
		db.intern_buckets ||
		db.intern_exact_hashes || db.intern_exact_next ||
		db.intern_exact_buckets) {
		return 10;
	}
	return 0;
}
