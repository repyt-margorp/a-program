#include "term.h"
#include "type_declaration.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 16
#define CASE_BINDER_CAPACITY 16
#define MATCH_FRAME_CAPACITY 16
#define TYPE_CAPACITY 8
#define CONSTRUCTOR_CAPACITY 16
#define PARAMETER_CAPACITY 8
#define FIELD_TYPE_CAPACITY 16
#define TYPE_EXPR_CAPACITY 16

static struct prototype_term terms[TERM_CAPACITY];
static struct prototype_match_case cases[CASE_CAPACITY];
static int case_label_symbols[CASE_CAPACITY];
static struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
static struct prototype_match_frame match_frames[MATCH_FRAME_CAPACITY];
static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructor_declarations[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameter_declarations[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];

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
		match_frames,
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

	uint32_t bool_type_id;
	uint32_t two_type_id;
	uint32_t self_expr;
	uint32_t bool_view;
	uint32_t two_view;
	uint32_t ignored_constructor;
	if (prototype_type_declaration_add(&type_db, 1, &bool_type_id) != 0) return 10;
	if (prototype_type_expr_self(&type_db, &self_expr) != 0) return 12;
	if (prototype_term_type_instance_make(&term_db, &type_db, bool_type_id, NULL, 0, &bool_view) != 0) return 13;
	if (prototype_type_declaration_add_constructor(
		&type_db, bool_type_id, 11, NULL, 0, self_expr, bool_view, &ignored_constructor
	) != 0) return 15;
	if (prototype_type_declaration_add_constructor(
		&type_db, bool_type_id, 12, NULL, 0, self_expr, bool_view, &ignored_constructor
	) != 0) return 16;
	if (prototype_type_declaration_add(&type_db, 2, &two_type_id) != 0) return 11;
	if (prototype_term_type_instance_make(&term_db, &type_db, two_type_id, NULL, 0, &two_view) != 0) return 14;
	if (prototype_type_declaration_add_constructor(
		&type_db, two_type_id, 21, NULL, 0, self_expr, two_view, &ignored_constructor
	) != 0) return 17;
	if (prototype_type_declaration_add_constructor(
		&type_db, two_type_id, 22, NULL, 0, self_expr, two_view, &ignored_constructor
	) != 0) return 18;
	if (prototype_type_declaration_rebuild_representations(&term_db, &type_db) != 0) return 19;
	if (prototype_term_rebind_type_former_anchors(&term_db, &type_db) != 0) return 20;

	int equal = 0;
	if (prototype_term_core_shape_equal(&term_db, bool_view, two_view, &equal) != 0 || !equal) {
		return 2;
	}
	if (prototype_term_normalization_equal_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		bool_view,
		two_view,
		&equal
		) != 0) {
		return 3;
	}
	if (equal) {
		return 4;
	}

	uint32_t bool_classifier;
	uint32_t two_classifier;
	if (prototype_term_pi(&term_db, bool_view, bool_view, &bool_classifier) != 0 ||
		prototype_term_pi(&term_db, two_view, two_view, &two_classifier) != 0 ||
		prototype_term_normalization_equal_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			bool_classifier,
			two_classifier,
			&equal
		) != 0 || equal) {
		return 5;
	}

	uint32_t bool_zero;
	uint32_t binder = prototype_term_fresh_binder(&term_db);
	uint32_t variable;
	uint32_t identity_core;
	uint32_t application;
	uint32_t whnf;
	if (prototype_term_constructor(&term_db, bool_view, 0, &bool_zero) != 0 ||
		prototype_term_var(&term_db, binder, &variable) != 0 ||
		prototype_term_lambda(&term_db, binder, variable, &identity_core) != 0 ||
		prototype_term_app(&term_db, identity_core, bool_zero, &application) != 0 ||
		prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			application,
			&whnf
		) != 0 || whnf != bool_zero) {
		return 6;
	}

	uint32_t bool_view_after;
	uint32_t two_view_after;
	if (prototype_term_type_instance_make(
			&term_db, &type_db, bool_type_id, NULL, 0, &bool_view_after
		) != 0 || prototype_term_type_instance_make(
			&term_db, &type_db, two_type_id, NULL, 0, &two_view_after
		) != 0 ||
		term_db.terms[bool_view_after].tag != PROTOTYPE_TERM_TYPE_VIEW ||
		term_db.terms[two_view_after].tag != PROTOTYPE_TERM_TYPE_VIEW ||
		term_db.terms[bool_view_after].as.type_view.core !=
			term_db.terms[two_view_after].as.type_view.core) {
		return 7;
	}
	uint32_t shared_core = term_db.terms[bool_view_after].as.type_view.core;
	uint32_t bool_whnf;
	uint32_t two_whnf;
	if (prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			shared_core,
			&bool_whnf
		) != 0 || prototype_term_normalize_complete_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			term_db.terms[two_view_after].as.type_view.core,
			&two_whnf
		) != 0 || bool_whnf != shared_core || two_whnf != shared_core) {
		return 8;
	}
	return 0;
}
