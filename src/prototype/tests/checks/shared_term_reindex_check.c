#include "a_program/kernel/context.h"
#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"

#include <stdint.h>
#include <stdio.h>

#define TERM_CAPACITY 512
#define CASE_CAPACITY 32
#define CASE_BINDER_CAPACITY 32
#define MATCH_FRAME_CAPACITY 16
#define CONTEXT_CAPACITY 32
#define SUBSTITUTION_CAPACITY 64

static int expect_reindex(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* types,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source,
	uint32_t substitution,
	uint32_t expected,
	const char* law
) {
	uint32_t result;
	if (prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source,
			substitution,
			&result
		) != 0 || result != expected) {
		fprintf(stderr, "%s failed\n", law);
		return -1;
	}
	return 0;
}

int main(void) {
	struct prototype_term term_storage[TERM_CAPACITY];
	struct prototype_match_case case_storage[CASE_CAPACITY];
	int case_label_storage[CASE_CAPACITY];
	struct prototype_case_binder case_binder_storage[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scope_storage[MATCH_FRAME_CAPACITY];
	struct prototype_term_db terms;
	struct prototype_type_declaration type_storage[1];
	struct prototype_type_constructor_declaration constructor_storage[1];
	struct prototype_type_parameter_declaration parameter_storage[1];
	uint32_t field_type_storage[1];
	struct prototype_type_expr type_expr_storage[1];
	struct prototype_type_declaration_db types;
	struct prototype_context context_storage[CONTEXT_CAPACITY];
	struct prototype_context_db contexts;
	struct prototype_substitution substitution_storage[SUBSTITUTION_CAPACITY];
	struct prototype_substitution_db substitutions;

	prototype_term_db_init(
		&terms,
		term_storage,
		TERM_CAPACITY,
		case_storage,
		case_label_storage,
		CASE_CAPACITY,
		case_binder_storage,
		CASE_BINDER_CAPACITY,
		ih_scope_storage,
		MATCH_FRAME_CAPACITY
	);
	prototype_type_declaration_db_init(
		&types,
		type_storage,
		1,
		constructor_storage,
		1,
		parameter_storage,
		1,
		field_type_storage,
		1,
		type_expr_storage,
		1
	);
	prototype_context_db_init(&contexts, context_storage, CONTEXT_CAPACITY);
	prototype_substitution_db_init(
		&substitutions, substitution_storage, SUBSTITUTION_CAPACITY
	);

	uint32_t int_type;
	uint32_t literal_seven;
	uint32_t literal_nine;
	uint32_t empty_row;
	uint32_t terminal_row;
	uint32_t symbolic_row;
	uint32_t symbolic_union;
	uint32_t outer_context;
	uint32_t section;
	uint32_t identity;
	uint32_t composed;
	uint32_t outer_variable;
	if (prototype_term_primitive_int(&terms, &int_type) != 0 ||
		prototype_term_int_literal(&terms, 7, &literal_seven) != 0 ||
		prototype_term_int_literal(&terms, 9, &literal_nine) != 0 ||
		prototype_term_effect_row_empty(&terms, &empty_row
		) != 0 || prototype_term_effect_row_operation(
			&terms, PROTOTYPE_EFFECT_OPERATION_PRINT, empty_row, &terminal_row
		) != 0 || prototype_term_effect_row_var(
			&terms, 400, &symbolic_row
		) != 0 || prototype_term_effect_row_union(
			&terms, symbolic_row, terminal_row, &symbolic_union
		) != 0 || prototype_context_extend(
			&contexts,
			prototype_context_empty(&contexts),
			100,
			int_type,
			PROTOTYPE_INVALID_ID,
			&outer_context
		) != 0 || prototype_term_var(
			&terms,
			prototype_context_get(&contexts, outer_context)->binding_id,
			&outer_variable
		) != 0 || prototype_substitution_empty(
			&substitutions,
			&contexts,
			prototype_context_empty(&contexts),
			&section
		) != 0 || prototype_substitution_extend(
			&substitutions,
			&contexts,
			&terms,
			&types,
			section,
			outer_context,
			literal_seven,
			int_type,
			&section
		) != 0 || prototype_substitution_identity(
			&substitutions, &contexts, outer_context, &identity
		) != 0 || prototype_substitution_compose(
			&substitutions, &contexts, identity, section, &composed
		) != 0) {
		fprintf(stderr, "failed to construct shared-term reindex fixture\n");
		return 1;
	}

	if (prototype_term_effect_row_purity(&terms, empty_row) !=
			PROTOTYPE_EFFECT_ROW_PURITY_PURE ||
		prototype_term_effect_row_purity(&terms, terminal_row) !=
			PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL ||
		prototype_term_effect_row_purity(&terms, symbolic_row) !=
			PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED ||
		prototype_term_effect_row_purity(&terms, symbolic_union) !=
			PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED ||
		prototype_term_effect_row_purity(&terms, PROTOTYPE_INVALID_ID) !=
			PROTOTYPE_EFFECT_ROW_PURITY_INVALID) {
		fprintf(stderr, "effect-row purity trichotomy failed\n");
		return 1;
	}

	uint32_t direct_left;
	uint32_t direct_right;
	uint32_t direct_pair;
	uint32_t direct_expected;
	uint32_t direct_result;
	struct prototype_binding_replacement direct_bindings[] = {
		{ .binding_id = 410 },
		{ .binding_id = 411 }
	};
	uint32_t binding_count_before_direct = terms.next_binding_id;
	if (prototype_term_var(&terms, 410, &direct_left) != 0 ||
		prototype_term_var(&terms, 411, &direct_right) != 0 ||
		prototype_term_app(&terms, direct_left, direct_right, &direct_pair) != 0) {
		fprintf(stderr, "failed to construct direct reindex fixture\n");
		return 1;
	}
	direct_bindings[0].replacement = direct_right;
	direct_bindings[1].replacement = literal_seven;
	if (prototype_term_app(
			&terms, direct_right, literal_seven, &direct_expected
		) != 0 || prototype_term_graph_reindex_bindings(
			&terms,
			&types,
			direct_pair,
			direct_bindings,
			2,
			&direct_result
		) != 0 || direct_result != direct_expected ||
		terms.next_binding_id != binding_count_before_direct) {
		fprintf(stderr, "simultaneous non-cascading binding reindex failed\n");
		return 1;
	}
	direct_bindings[0].replacement = direct_right;
	direct_bindings[1].replacement = direct_left;
	if (prototype_term_app(
			&terms, direct_right, direct_left, &direct_expected
		) != 0 || prototype_term_graph_reindex_bindings(
			&terms,
			&types,
			direct_pair,
			direct_bindings,
			2,
			&direct_result
		) != 0 || direct_result != direct_expected) {
		fprintf(stderr, "simultaneous binding permutation failed\n");
		return 1;
	}

	uint32_t alpha_left_variable;
	uint32_t alpha_right_variable;
	uint32_t alpha_left_lambda;
	uint32_t alpha_right_lambda;
	if (prototype_term_var(&terms, 420, &alpha_left_variable) != 0 ||
		prototype_term_var(&terms, 421, &alpha_right_variable) != 0 ||
		alpha_left_variable == alpha_right_variable ||
		prototype_term_lambda(
			&terms, 420, alpha_left_variable, &alpha_left_lambda
		) != 0 || prototype_term_lambda(
			&terms, 421, alpha_right_variable, &alpha_right_lambda
		) != 0 || alpha_left_lambda != alpha_right_lambda) {
		fprintf(stderr, "bound alpha/free binding identity boundary failed\n");
		return 1;
	}

	uint32_t app;
	uint32_t expected_app;
	uint32_t returned;
	uint32_t expected_returned;
	uint32_t lambda;
	uint32_t expected_lambda;
	uint32_t shadow_lambda;
	uint32_t computation_type;
	uint32_t expected_computation_type;
	uint32_t thunk_type;
	uint32_t expected_thunk_type;
	uint32_t thunk;
	uint32_t expected_thunk;
	uint32_t force;
	uint32_t expected_force;
	if (prototype_term_app(
			&terms, outer_variable, literal_nine, &app
		) != 0 || prototype_term_app(
			&terms, literal_seven, literal_nine, &expected_app
		) != 0 || prototype_term_return(
			&terms, outer_variable, &returned
		) != 0 || prototype_term_return(
			&terms, literal_seven, &expected_returned
		) != 0 || prototype_term_lambda(
			&terms, 200, returned, &lambda
		) != 0 || prototype_term_lambda(
			&terms, 200, expected_returned, &expected_lambda
		) != 0 || prototype_term_lambda(
			&terms,
			prototype_context_get(&contexts, outer_context)->binding_id,
			outer_variable,
			&shadow_lambda
		) != 0 || prototype_term_computation_type(
			&terms, empty_row, outer_variable, &computation_type
		) != 0 || prototype_term_computation_type(
			&terms, empty_row, literal_seven, &expected_computation_type
		) != 0 || prototype_term_thunk_type(
			&terms, computation_type, &thunk_type
		) != 0 || prototype_term_thunk_type(
			&terms, expected_computation_type, &expected_thunk_type
		) != 0 || prototype_term_thunk(
			&terms, returned, &thunk
		) != 0 || prototype_term_thunk(
			&terms, expected_returned, &expected_thunk
		) != 0 || prototype_term_force(
			&terms, thunk, &force
		) != 0 || prototype_term_force(
			&terms, expected_thunk, &expected_force
		) != 0) {
		fprintf(stderr, "failed to construct shared APP/F-U terms\n");
		return 1;
	}

	uint32_t codomain_family;
	uint32_t expected_codomain_family;
	uint32_t pi;
	uint32_t expected_pi;
	if (prototype_term_pure_family(
			&terms, 201, computation_type, &codomain_family
		) != 0 || prototype_term_pure_family(
			&terms, 201, expected_computation_type, &expected_codomain_family
		) != 0 || prototype_term_pi_family(
			&terms, int_type, codomain_family, &pi
		) != 0 || prototype_term_pi_family(
			&terms, int_type, expected_codomain_family, &expected_pi
		) != 0) {
		fprintf(stderr, "failed to construct shared Pi terms\n");
		return 1;
	}

	struct prototype_match_case_input free_case = {
		.case_label_symbol_id = 1,
		.constructor_owner = int_type,
		.constructor_id = 0,
		.binders = NULL,
		.binder_count = 0,
		.body = returned
	};
	struct prototype_match_case_input expected_free_case = free_case;
	expected_free_case.body = expected_returned;
	uint32_t match;
	uint32_t expected_match;
	uint32_t match_motive;
	uint32_t expected_match_motive;
	if (prototype_term_match(
			&terms, outer_variable, &free_case, 1, &match
		) != 0 || prototype_term_match(
			&terms, literal_seven, &expected_free_case, 1, &expected_match
		) != 0 || prototype_term_pure_family(
			&terms, 203, match, &match_motive
		) != 0 || prototype_term_pure_family(
			&terms, 203, expected_match, &expected_match_motive
		) != 0) {
		fprintf(stderr, "failed to construct reindexed Match terms\n");
		return 1;
	}

	struct prototype_case_binder shadow_binder = {
		.binding_id = prototype_context_get(&contexts, outer_context)->binding_id,
		.is_recursive = 0
	};
	struct prototype_match_case_input shadow_case = {
		.case_label_symbol_id = 2,
		.constructor_owner = int_type,
		.constructor_id = 1,
		.binders = &shadow_binder,
		.binder_count = 1,
		.body = outer_variable
	};
	uint32_t shadow_match;
	if (prototype_term_match(
			&terms, literal_nine, &shadow_case, 1, &shadow_match
		) != 0) {
		fprintf(stderr, "failed to construct shadowing Match\n");
		return 1;
	}

	uint32_t ih_frame = prototype_term_new_ih_scope(&terms);
	uint32_t ih_term;
	struct prototype_case_binder recursive_binder = {
		.binding_id = 202,
		.is_recursive = 1
	};
	struct prototype_match_case_input ih_case = {
		.case_label_symbol_id = 3,
		.constructor_owner = int_type,
		.constructor_id = 2,
		.binders = &recursive_binder,
		.binder_count = 1,
		.body = PROTOTYPE_INVALID_ID
	};
	uint32_t ih_match;
	if (ih_frame == PROTOTYPE_INVALID_ID ||
		prototype_term_induction_hypothesis(
			&terms, ih_frame, outer_variable, &ih_term
		) != 0) {
		fprintf(stderr, "failed to construct scoped IH\n");
		return 1;
	}
	ih_case.body = ih_term;
	if (prototype_term_match_with_ih_scope(
			&terms, outer_variable, &ih_case, 1, ih_frame, &ih_match
		) != 0 || prototype_term_set_ih_scope_term(
			&terms, ih_frame, ih_match
		) != 0) {
		fprintf(stderr, "failed to construct scoped IH Match\n");
		return 1;
	}

	struct {
		uint32_t source;
		uint32_t expected;
		const char* law;
	} laws[] = {
		{ app, expected_app, "APP reindex" },
		{ returned, expected_returned, "RETURN reindex" },
		{ lambda, expected_lambda, "LAMBDA reindex" },
		{ computation_type, expected_computation_type, "Comp reindex" },
		{ thunk_type, expected_thunk_type, "ThunkType reindex" },
		{ thunk, expected_thunk, "THUNK reindex" },
		{ force, expected_force, "FORCE reindex" },
		{ pi, expected_pi, "PI reindex" },
		{ match, expected_match, "MATCH reindex" },
		{ match_motive, expected_match_motive, "MATCH motive reindex" },
		{ shadow_lambda, shadow_lambda, "LAMBDA shadowing" },
		{ shadow_match, shadow_match, "MATCH binder shadowing" }
	};
	uint32_t binding_count_before_reindex = terms.next_binding_id;
	for (size_t i = 0; i < sizeof(laws) / sizeof(laws[0]); ++i) {
		if (expect_reindex(
				&terms,
				&types,
				&contexts,
				&substitutions,
				laws[i].source,
				section,
				laws[i].expected,
				laws[i].law
			) != 0 || expect_reindex(
				&terms,
				&types,
				&contexts,
				&substitutions,
				laws[i].source,
				composed,
				laws[i].expected,
				"composition reindex"
			) != 0 || expect_reindex(
				&terms,
				&types,
				&contexts,
				&substitutions,
				laws[i].source,
				identity,
				laws[i].source,
				"identity reindex"
			) != 0) {
			return 1;
		}
	}
	if (terms.next_binding_id != binding_count_before_reindex) {
		fprintf(stderr, "ordinary reindex consumed transient bindings\n");
		return 1;
	}

	uint32_t reindexed_ih_match;
	if (prototype_term_reindex(
			&terms,
			&types,
			&contexts,
			&substitutions,
			ih_match,
			section,
			&reindexed_ih_match
		) != 0 || reindexed_ih_match >= terms.term_count ||
		terms.terms[reindexed_ih_match].tag != PROTOTYPE_TERM_MATCH ||
		terms.terms[reindexed_ih_match].as.match.ih_scope_id == ih_frame ||
		terms.terms[reindexed_ih_match].as.match.first_case >= terms.case_count) {
		fprintf(stderr, "scoped IH Match reindex failed\n");
		return 1;
	}
	const struct prototype_match_case* reindexed_ih_case =
		&terms.cases[terms.terms[reindexed_ih_match].as.match.first_case];
	if (reindexed_ih_case->body >= terms.term_count ||
		terms.terms[reindexed_ih_case->body].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		terms.terms[reindexed_ih_case->body].as.induction_hypothesis.ih_scope_id !=
			terms.terms[reindexed_ih_match].as.match.ih_scope_id ||
		terms.terms[reindexed_ih_case->body].as.induction_hypothesis.argument !=
			literal_seven) {
		fprintf(stderr, "scoped IH frame remap law failed\n");
		return 1;
	}

	uint32_t constant_family;
	uint32_t dependent_classifier;
	uint32_t dependent_context;
	uint32_t telescope_classifier;
	if (prototype_term_lambda(
			&terms, 300, int_type, &constant_family
		) != 0 || prototype_term_app(
			&terms, constant_family, outer_variable, &dependent_classifier
		) != 0 || prototype_context_extend(
			&contexts,
			outer_context,
			101,
			dependent_classifier,
			PROTOTYPE_INVALID_ID,
			&dependent_context
		) != 0 || prototype_context_telescope_entry_classifier(
			&contexts,
			&substitutions,
			&terms,
			&types,
			section,
			outer_context,
			dependent_context,
			NULL,
			0,
			0,
			&telescope_classifier
		) != 0 || telescope_classifier != int_type) {
		fprintf(stderr, "dependent telescope reindex failed\n");
		return 1;
	}

	if (prototype_context_db_validate(&contexts, &terms) != 0 ||
		prototype_substitution_db_validate_typed(
			&substitutions, &contexts, &terms, &types
		) != 0) {
		fprintf(stderr, "shared-term categorical stores are invalid\n");
		return 1;
	}

	printf("shared-term HOTT substrate checks passed\n");
	return 0;
}
