#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"

#include <stdint.h>
#include <stdio.h>

#define TERM_CAPACITY 256
#define CASE_CAPACITY 16
#define CASE_BINDER_CAPACITY 16
#define IH_SCOPE_CAPACITY 8

int main(void) {
	struct prototype_term term_storage[TERM_CAPACITY];
	struct prototype_match_case case_storage[CASE_CAPACITY];
	int case_label_storage[CASE_CAPACITY];
	struct prototype_case_binder case_binder_storage[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scope_storage[IH_SCOPE_CAPACITY];
	struct prototype_term_db terms;
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
		IH_SCOPE_CAPACITY
	);

	struct prototype_type_declaration type_storage[1];
	struct prototype_type_constructor_declaration constructor_storage[1];
	struct prototype_type_constructor_readback constructor_readbacks[1];
	struct prototype_constructor_classifier_cache_entry constructor_caches[1];
	struct prototype_type_parameter_declaration parameter_storage[1];
	uint32_t field_type_storage[1];
	struct prototype_type_expr type_expr_storage[1];
	struct prototype_type_representation representation_storage[1];
	struct prototype_type_declaration_db types;
	prototype_type_declaration_db_init(
		&types,
		type_storage,
		1,
		constructor_storage,
		1,
		parameter_storage,
		1,
		constructor_readbacks,
		1,
		field_type_storage,
		1,
		type_expr_storage,
		1,
		representation_storage,
		1,
		constructor_caches,
		1
	);

	uint32_t owner;
	uint32_t recursive_value;
	uint32_t branch_value;
	uint32_t branch_result;
	uint32_t neutral_scrutinee;
	uint32_t ih_scope;
	uint32_t frame_match;
	struct prototype_match_case_input match_case;
	match_case.case_label_symbol_id = -1;
	match_case.constructor_owner = 0;
	match_case.constructor_id = 0;
	match_case.binders = NULL;
	match_case.binder_count = 0;
	if (prototype_term_primitive_int(&terms, &owner) != 0 ||
		prototype_term_constructor(&terms, owner, 0, &recursive_value) != 0 ||
		prototype_term_int_literal(&terms, 7, &branch_value) != 0 ||
		prototype_term_return(&terms, branch_value, &branch_result) != 0 ||
		prototype_term_external_ref(
			&terms,
			(struct prototype_qualified_name) { PROTOTYPE_BASE_NAMESPACE_ID, 1 },
			&neutral_scrutinee
		) != 0 ||
		(ih_scope = prototype_term_new_ih_scope(&terms)) == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	match_case.constructor_owner = owner;
	match_case.body = branch_result;
	if (prototype_term_match_with_ih_scope(
			&terms, neutral_scrutinee, &match_case, 1, ih_scope, &frame_match
		) != 0 || prototype_term_set_ih_scope_term(
			&terms, ih_scope, frame_match
		) != 0) {
		return 2;
	}

	uint32_t argument_binding = prototype_term_new_binding(&terms);
	uint32_t down_body;
	uint32_t down_function;
	uint32_t down;
	uint32_t lifted_ih;
	uint32_t lifted_binding = prototype_term_new_binding(&terms);
	uint32_t lifted_var;
	uint32_t forced_lifted;
	uint32_t argument;
	uint32_t applied_lifted;
	uint32_t continuation;
	uint32_t observed;
	if (argument_binding == PROTOTYPE_INVALID_ID ||
		lifted_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_return(&terms, recursive_value, &down_body) != 0 ||
		prototype_term_lambda(
			&terms, argument_binding, down_body, &down_function
		) != 0 || prototype_term_thunk(&terms, down_function, &down) != 0 ||
		prototype_term_induction_hypothesis(
			&terms, ih_scope, down, &lifted_ih
		) != 0 || prototype_term_var(
			&terms, lifted_binding, &lifted_var
		) != 0 || prototype_term_force(
			&terms, lifted_var, &forced_lifted
		) != 0 || prototype_term_int_literal(&terms, 0, &argument) != 0 ||
		prototype_term_app(
			&terms, forced_lifted, argument, &applied_lifted
		) != 0 || prototype_term_lambda(
			&terms, lifted_binding, applied_lifted, &continuation
		) != 0 || prototype_term_computation_fold(
			&terms, lifted_ih, continuation, NULL, 0, &observed
		) != 0) {
		return 3;
	}

	uint32_t normal;
	if (prototype_term_nf_with_options(
			&terms,
			&types,
			NULL,
			(struct prototype_term_reduction_options) {
				.flags = PROTOTYPE_TERM_REDUCE_DEFAULT
			},
			observed,
			&normal
		) != 0 || normal != branch_result) {
		fprintf(stderr, "lifted induction hypothesis did not reduce through Pi\n");
		return 4;
	}
	return 0;
}
