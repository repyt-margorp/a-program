#include "a_program/frontend/lowering.h"

#include <stdint.h>
#include <string.h>

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
static struct prototype_ih_scope ih_scopes[MATCH_FRAME_CAPACITY];
static struct prototype_type_declaration type_declarations[TYPE_CAPACITY];
static struct prototype_type_constructor_declaration constructor_declarations[CONSTRUCTOR_CAPACITY];
static struct prototype_type_parameter_declaration parameter_declarations[PARAMETER_CAPACITY];
static uint32_t field_types[FIELD_TYPE_CAPACITY];
static struct prototype_type_expr type_exprs[TYPE_EXPR_CAPACITY];
static struct prototype_type_representation type_representations[TYPE_CAPACITY];
static struct prototype_context contexts[8];

static int add_nullary_type(
	struct prototype_term_db* terms_db,
	struct prototype_type_declaration_db* types_db,
	uint32_t name_symbol_id,
	uint32_t constructor_count,
	uint32_t* p_type_id
) {
	uint32_t self_expr;
	uint32_t view;
	uint32_t constructor_id;
	if (!terms_db || !types_db || !p_type_id || prototype_type_declaration_add(
			types_db, name_symbol_id, p_type_id
		) != 0) {
		return -1;
	}
	types_db->type_declarations[*p_type_id].parameter_context = 0;
	types_db->type_declarations[*p_type_id].index_context = 0;
	if (prototype_type_expr_self(types_db, &self_expr) != 0 ||
		prototype_term_type_instance_make(
			terms_db, types_db, *p_type_id, NULL, 0, &view
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < constructor_count; ++i) {
		if (prototype_type_declaration_add_constructor(
				types_db,
				*p_type_id,
				name_symbol_id + i + 1,
				NULL,
				0,
				self_expr,
				0,
				0,
				view,
				view,
				&constructor_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

int main(void) {
	struct prototype_term_db term_db;
	struct prototype_type_declaration_db type_db;
	struct prototype_context_db context_db;
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
		TYPE_EXPR_CAPACITY,
		type_representations,
		TYPE_CAPACITY
	);
	prototype_context_db_init(&context_db, contexts, 8);

	uint32_t bool_type_id;
	uint32_t two_type_id;
	uint32_t self_expr;
	uint32_t bool_view;
	uint32_t two_view;
	uint32_t ignored_constructor;
	if (prototype_type_declaration_add(&type_db, 1, &bool_type_id) != 0) return 10;
	type_db.type_declarations[bool_type_id].parameter_context = 0;
	type_db.type_declarations[bool_type_id].index_context = 0;
	if (prototype_type_expr_self(&type_db, &self_expr) != 0) return 12;
	if (prototype_term_type_instance_make(&term_db, &type_db, bool_type_id, NULL, 0, &bool_view) != 0) return 13;
	if (prototype_type_declaration_add_constructor(
		&type_db, bool_type_id, 11, NULL, 0, self_expr,
		0, 0, bool_view, bool_view, &ignored_constructor
	) != 0) return 15;
	if (prototype_type_declaration_add_constructor(
		&type_db, bool_type_id, 12, NULL, 0, self_expr,
		0, 0, bool_view, bool_view, &ignored_constructor
	) != 0) return 16;
	if (prototype_type_declaration_add(&type_db, 2, &two_type_id) != 0) return 11;
	type_db.type_declarations[two_type_id].parameter_context = 0;
	type_db.type_declarations[two_type_id].index_context = 0;
	if (prototype_term_type_instance_make(&term_db, &type_db, two_type_id, NULL, 0, &two_view) != 0) return 14;
	if (prototype_type_declaration_add_constructor(
		&type_db, two_type_id, 21, NULL, 0, self_expr,
		0, 0, two_view, two_view, &ignored_constructor
	) != 0) return 17;
	if (prototype_type_declaration_add_constructor(
		&type_db, two_type_id, 22, NULL, 0, self_expr,
		0, 0, two_view, two_view, &ignored_constructor
	) != 0) return 18;
	if (prototype_type_declaration_rebuild_representations(
			&term_db, &type_db, &context_db
		) != 0) return 19;
	if (prototype_term_rebind_type_former_anchors(&term_db, &type_db) != 0) return 20;

	int equal = 0;
	struct prototype_term_conversion_result conversion;
	if (prototype_term_core_shape_equal(&term_db, bool_view, two_view, &equal) != 0 || !equal) {
		return 2;
	}
	if (prototype_term_compare_for_conversion(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			bool_view,
			two_view,
			UINT64_MAX,
			&conversion
		) != 0) {
		return 3;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return 4;
	}

	uint32_t bool_classifier;
	uint32_t two_classifier;
	if (prototype_term_pi(&term_db, bool_view, bool_view, &bool_classifier) != 0 ||
		prototype_term_pi(&term_db, two_view, two_view, &two_classifier) != 0 ||
		prototype_term_compare_for_conversion(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			bool_classifier,
			two_classifier,
			UINT64_MAX,
			&conversion
		) != 0 || conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return 5;
	}

	uint32_t bool_zero;
	uint32_t binder = prototype_term_new_binding(&term_db);
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

	struct prototype_term cross_terms_storage[64];
	struct prototype_match_case cross_cases[8];
	int cross_case_labels[8];
	struct prototype_case_binder cross_case_binders[8];
	struct prototype_ih_scope cross_ih_scopes[8];
	struct prototype_type_declaration cross_types[4];
	struct prototype_type_constructor_declaration cross_constructors[8];
	struct prototype_type_parameter_declaration cross_parameters[4];
	uint32_t cross_fields[8];
	struct prototype_type_expr cross_exprs[8];
	struct prototype_type_representation cross_representations[4];
	struct prototype_context cross_contexts[4];
	struct prototype_term_db cross_term_db;
	struct prototype_type_declaration_db cross_type_db;
	struct prototype_context_db cross_context_db;
	prototype_term_db_init(
		&cross_term_db,
		cross_terms_storage,
		64,
		cross_cases,
		cross_case_labels,
		8,
		cross_case_binders,
		8,
		cross_ih_scopes,
		8
	);
	prototype_type_declaration_db_init(
		&cross_type_db,
		cross_types,
		4,
		cross_constructors,
		8,
		cross_parameters,
		4,
		cross_fields,
		8,
		cross_exprs,
		8,
		cross_representations,
		4
	);
	prototype_context_db_init(&cross_context_db, cross_contexts, 4);
	uint32_t cross_two_type;
	uint32_t cross_one_type;
	if (add_nullary_type(
			&cross_term_db, &cross_type_db, 31, 2, &cross_two_type
		) != 0 || add_nullary_type(
			&cross_term_db, &cross_type_db, 41, 1, &cross_one_type
		) != 0 || prototype_type_declaration_rebuild_representations(
			&cross_term_db, &cross_type_db, &cross_context_db
		) != 0) {
		return 21;
	}
	int cross_equal = 0;
	if (prototype_type_declaration_representations_equal(
			&term_db,
			&type_db,
			&context_db,
			bool_type_id,
			&cross_term_db,
			&cross_type_db,
			&cross_context_db,
			cross_two_type,
			&cross_equal
		) != 0 || !cross_equal) {
		return 22;
	}
	uint32_t bool_representation =
		type_db.type_declarations[bool_type_id].representation_id;
	uint32_t one_representation =
		cross_type_db.type_declarations[cross_one_type].representation_id;
	if (bool_representation >= type_db.representation_count ||
		one_representation >= cross_type_db.representation_count) {
		return 23;
	}
	/* A fingerprint is only a prefilter. Simulating a collision must not make
	 * structurally different constructor schemas equal. */
	cross_type_db.representations[one_representation].fingerprint =
		type_db.representations[bool_representation].fingerprint;
	if (prototype_type_declaration_representations_equal(
			&term_db,
			&type_db,
			&context_db,
			bool_type_id,
			&cross_term_db,
			&cross_type_db,
			&cross_context_db,
			cross_one_type,
			&cross_equal
		) != 0 || cross_equal) {
		return 24;
	}

	uint32_t literal_one;
	uint32_t literal_two;
	struct prototype_compile_label colliding_labels[2];
	struct prototype_compile_metadata colliding_metadata;
	struct prototype_canonical_link_entry colliding_entries[2];
	struct prototype_canonical_link_table colliding_table;
	if (prototype_term_int_literal(&term_db, 1, &literal_one) != 0 ||
		prototype_term_int_literal(&term_db, 2, &literal_two) != 0) {
		return 25;
	}
	memset(&colliding_labels[0], 0, sizeof(colliding_labels[0]));
	memset(&colliding_labels[1], 0, sizeof(colliding_labels[1]));
	if (prototype_term_canonical_key(
			&term_db, literal_one, &colliding_labels[0].canonical_key
		) != 0) {
		return 25;
	}
	colliding_labels[0].term = literal_one;
	colliding_labels[1].term = literal_two;
	colliding_labels[1].canonical_key = colliding_labels[0].canonical_key;
	memset(&colliding_metadata, 0, sizeof(colliding_metadata));
	colliding_metadata.labels = colliding_labels;
	colliding_metadata.label_count = 2;
	prototype_canonical_link_table_init(
		&colliding_table, colliding_entries, 2
	);
	if (prototype_canonical_link_table_add_metadata(
			&colliding_table,
			&term_db,
			&type_db,
			&colliding_metadata,
			0,
			0
		) != 0 || colliding_table.entry_count != 2 ||
		colliding_table.entries[0].representative ==
			colliding_table.entries[1].representative) {
		return 26;
	}
	return 0;
}
