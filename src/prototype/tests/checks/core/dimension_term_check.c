#include "a_program/core/term.h"
#include "a_program/dimension/operator.h"

#include <stdint.h>

#define TERM_CAPACITY 128
#define CASE_CAPACITY 8
#define CASE_BINDER_CAPACITY 8
#define IH_SCOPE_CAPACITY 8
#define OPERATOR_CAPACITY 16
#define IMAGE_CAPACITY 64

struct test_term_storage {
	struct prototype_term terms[TERM_CAPACITY];
	struct prototype_match_case cases[CASE_CAPACITY];
	int case_labels[CASE_CAPACITY];
	struct prototype_case_binder case_binders[CASE_BINDER_CAPACITY];
	struct prototype_ih_scope ih_scopes[IH_SCOPE_CAPACITY];
};

static void init_terms(
	struct prototype_term_db* terms,
	struct test_term_storage* storage
) {
	prototype_term_db_init(
		terms,
		storage->terms,
		TERM_CAPACITY,
		storage->cases,
		storage->case_labels,
		CASE_CAPACITY,
		storage->case_binders,
		CASE_BINDER_CAPACITY,
		storage->ih_scopes,
		IH_SCOPE_CAPACITY
	);
}

static void init_operators(
	struct prototype_dimension_operator_db* operators,
	struct prototype_dimension_operator* operator_storage,
	struct prototype_dimension_axis_image* image_storage
) {
	prototype_dimension_operator_db_init(
		operators,
		operator_storage,
		OPERATOR_CAPACITY,
		image_storage,
		IMAGE_CAPACITY
	);
}

int main(void) {
	struct test_term_storage source_storage;
	struct prototype_term_db source_terms;
	struct prototype_dimension_operator source_operator_storage[OPERATOR_CAPACITY];
	struct prototype_dimension_axis_image source_image_storage[IMAGE_CAPACITY];
	struct prototype_dimension_operator_db source_operators;
	init_terms(&source_terms, &source_storage);
	init_operators(
		&source_operators, source_operator_storage, source_image_storage
	);

	uint32_t extension_0;
	uint32_t identity_0;
	uint32_t binding = prototype_term_new_binding(&source_terms);
	uint32_t variable;
	uint32_t action;
	uint32_t action_again;
	uint32_t identity_action;
	if (prototype_dimension_operator_extension(
			&source_operators, 0, &extension_0
		) != 0 || prototype_dimension_operator_identity(
			&source_operators, 0, &identity_0
		) != 0 || binding == PROTOTYPE_INVALID_ID ||
		prototype_term_var(&source_terms, binding, &variable) != 0 ||
		prototype_term_dimension_action(
			&source_terms, &source_operators, variable, extension_0, &action
		) != 0 || prototype_term_dimension_action(
			&source_terms, &source_operators, variable, extension_0, &action_again
		) != 0 || action_again != action || prototype_term_dimension_action(
			&source_terms, &source_operators, variable, identity_0, &identity_action
		) != 0 || action == identity_action || prototype_term_dimension_action(
			&source_terms,
			&source_operators,
			variable,
			UINT32_MAX,
			&action_again
		) == 0) {
		return 1;
	}

	uint32_t child_count;
	struct prototype_term_child child;
	struct prototype_term_semantics semantics;
	if (prototype_term_child_count(&source_terms, action, &child_count) != 0 ||
		child_count != 1 || prototype_term_child_at(
			&source_terms, action, 0, &child
		) != 0 || child.role != PROTOTYPE_TERM_CHILD_DIMENSION_ACTION_SOURCE ||
		child.term != variable || !prototype_term_contains_free_binding(
			&source_terms, action, binding
		) || prototype_term_semantics(
			&source_terms, action, &semantics
		) != 0 || semantics.layer != PROTOTYPE_TERM_LAYER_DIMENSION_ACTION) {
		return 1;
	}

	uint32_t literal;
	uint32_t substituted;
	uint32_t substituted_source;
	uint32_t substituted_operator;
	if (prototype_term_int_literal(&source_terms, 7, &literal) != 0 ||
		prototype_term_graph_substitute_bound_var(
			&source_terms, NULL, action, binding, literal, &substituted
		) != 0 || prototype_term_dimension_action_info(
			&source_terms,
			substituted,
			&substituted_source,
			&substituted_operator
		) != 0 || substituted_source != literal ||
		substituted_operator != extension_0) {
		return 1;
	}

	int equal;
	struct prototype_term_canonical_key key;
	if (prototype_term_core_shape_equal(
			&source_terms, action, action_again, &equal
		) != 0 || !equal) {
		return 1;
	}
	if (prototype_term_core_shape_equal(
			&source_terms, action, identity_action, &equal
		) != 0 || equal || prototype_term_canonical_key(
			&source_terms, action, &key
		) != 0 || key.node_count != 2) {
		return 1;
	}

	struct test_term_storage target_storage;
	struct prototype_term_db target_terms;
	struct prototype_dimension_operator target_operator_storage[OPERATOR_CAPACITY];
	struct prototype_dimension_axis_image target_image_storage[IMAGE_CAPACITY];
	struct prototype_dimension_operator_db target_operators;
	init_terms(&target_terms, &target_storage);
	init_operators(
		&target_operators, target_operator_storage, target_image_storage
	);
	uint32_t target_extension_1;
	uint32_t target_extension_0;
	uint32_t target_identity_0;
	if (prototype_dimension_operator_extension(
			&target_operators, 1, &target_extension_1
		) != 0 || prototype_dimension_operator_extension(
			&target_operators, 0, &target_extension_0
		) != 0 || prototype_dimension_operator_identity(
			&target_operators, 0, &target_identity_0
		) != 0 || target_extension_0 == extension_0) {
		return 1;
	}
	(void)target_extension_1;

	uint32_t target_binding = prototype_term_new_binding(&target_terms);
	uint32_t type_relocation[1] = { 0 };
	uint32_t binding_relocation[1] = { target_binding };
	uint32_t representation_relocation[1] = { 0 };
	uint32_t operator_relocation[2];
	operator_relocation[extension_0] = target_extension_0;
	operator_relocation[identity_0] = target_identity_0;
	uint32_t source_order[1] = { action };
	uint32_t term_relocation[TERM_CAPACITY];
	if (target_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_db_append_relocated(
			&target_terms,
			&source_terms,
			type_relocation,
			1,
			binding_relocation,
			1,
			0,
			representation_relocation,
			1,
			operator_relocation,
			2,
			source_order,
			1,
			term_relocation,
			TERM_CAPACITY
		) != 0 || term_relocation[action] == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t relocated_source;
	uint32_t relocated_operator;
	if (prototype_term_dimension_action_info(
			&target_terms,
			term_relocation[action],
			&relocated_source,
			&relocated_operator
		) != 0 || relocated_operator != target_extension_0 ||
		relocated_source >= target_terms.term_count ||
		target_terms.terms[relocated_source].tag != PROTOTYPE_TERM_VAR ||
		target_terms.terms[relocated_source].as.var.binding_id != target_binding) {
		return 1;
	}
	return 0;
}
