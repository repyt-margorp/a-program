#include "term.h"
#include "type_declaration.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 16
#define CASE_BINDER_CAPACITY 16
#define MATCH_FRAME_CAPACITY 16
#define TYPE_CAPACITY 8
#define CONSTRUCTOR_CAPACITY 8
#define PARAMETER_CAPACITY 8
#define FIELD_TYPE_CAPACITY 8
#define TYPE_EXPR_CAPACITY 8

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

	uint32_t type_id;
	uint32_t owner;
	uint32_t constructor;
	uint32_t argument;
	uint32_t lambda;
	uint32_t application;
	uint32_t branch;
	if (prototype_type_declaration_add(&type_db, 1, &type_id) != 0 ||
		prototype_term_type_instance_make(
			&term_db, &type_db, type_id, NULL, 0, &owner
		) != 0 ||
		prototype_term_constructor(&term_db, owner, 0, &constructor) != 0 ||
		prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ PROTOTYPE_BASE_NAMESPACE_ID, 2 },
			&argument
		) != 0 ||
		prototype_term_lambda(&term_db, prototype_term_fresh_binder(&term_db), constructor, &lambda) != 0 ||
		prototype_term_app(&term_db, lambda, argument, &application) != 0 ||
		prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ PROTOTYPE_BASE_NAMESPACE_ID, 3 },
			&branch
		) != 0) {
		return 1;
	}

	struct prototype_match_case_input match_case = {
		.case_label_symbol_id = -1,
		.constructor_owner = PROTOTYPE_INVALID_ID,
		.constructor_id = 0,
		.binders = NULL,
		.binder_count = 0,
		.body = branch
	};
	uint32_t match_term;
	if (prototype_term_match(&term_db, application, &match_case, 1, &match_term) != 0) {
		return 1;
	}
	uint32_t neutral_scrutinee;
	if (prototype_term_external_ref(
			&term_db,
			(struct prototype_qualified_name){ PROTOTYPE_BASE_NAMESPACE_ID, 4 },
			&neutral_scrutinee
		) != 0) {
		return 1;
	}
	struct prototype_match_case_input uniform_cases[2] = {
		{
			.case_label_symbol_id = -1,
			.constructor_owner = PROTOTYPE_INVALID_ID,
			.constructor_id = 0,
			.binders = NULL,
			.binder_count = 0,
			.body = branch
		},
		{
			.case_label_symbol_id = -1,
			.constructor_owner = PROTOTYPE_INVALID_ID,
			.constructor_id = 1,
			.binders = NULL,
			.binder_count = 0,
			.body = branch
		}
	};
	uint32_t neutral_uniform_match;
	if (prototype_term_match(
			&term_db, neutral_scrutinee, uniform_cases, 2, &neutral_uniform_match
		) != 0) {
		return 1;
	}
	uint32_t kernel_whnf;
	if (prototype_term_whnf_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_KERNEL_CONVERSION_WHNF,
			neutral_uniform_match,
			&kernel_whnf
		) != 0 || kernel_whnf != neutral_uniform_match) {
		return 1;
	}

	prototype_term_normalization_cache_clear(&term_db);
	uint32_t lambda_whnf;
	if (prototype_term_whnf_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_LAMBDA_WHNF,
			match_term,
			&lambda_whnf
		) != 0 ||
		lambda_whnf != match_term) {
		return 1;
	}
	uint32_t cached_lambda_whnf;
	if (prototype_term_whnf_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_LAMBDA_WHNF,
			match_term,
			&cached_lambda_whnf
		) != 0 ||
		cached_lambda_whnf != match_term) {
		return 1;
	}
	struct prototype_term_normalization_cache_stats stats;
	prototype_term_normalization_cache_get_stats(&term_db, &stats);
	if (stats.hit_count == 0) {
		return 1;
	}

	uint32_t unresolved_inductive_whnf;
	if (prototype_term_whnf_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_INDUCTIVE_WHNF,
			match_term,
			&unresolved_inductive_whnf
		) != 0 ||
		unresolved_inductive_whnf != match_term) {
		return 1;
	}

	if (prototype_term_resolve_match_case(&term_db, match_term, 0, owner, 0) != 0) {
		return 1;
	}
	uint32_t inductive_whnf;
	if (prototype_term_whnf_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_INDUCTIVE_WHNF,
			match_term,
			&inductive_whnf
		) != 0 ||
		inductive_whnf != branch) {
		return 1;
	}
	uint32_t cached_inductive_whnf;
	if (prototype_term_whnf_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_INDUCTIVE_WHNF,
			match_term,
			&cached_inductive_whnf
		) != 0 ||
		cached_inductive_whnf != branch) {
		return 1;
	}
	prototype_term_normalization_cache_get_stats(&term_db, &stats);
	if (stats.hit_count < 2) {
		return 1;
	}

	/* Artifact linking mutates graph slots directly; its notification must make
	 * the cached WHNF of this match unusable. */
	term_db.cases[term_db.terms[match_term].as.match.first_case].body = application;
	prototype_term_notify_graph_mutation(&term_db);
	uint32_t mutated_whnf;
	if (prototype_term_whnf_with_profile(
			&term_db,
			&type_db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_INDUCTIVE_WHNF,
			match_term,
			&mutated_whnf
		) != 0 ||
		mutated_whnf != constructor) {
		return 1;
	}
	return 0;
}
