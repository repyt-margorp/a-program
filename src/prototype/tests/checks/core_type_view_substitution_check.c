#include "a_program/core/term.h"

#include <stdint.h>
#include <stdio.h>

#define TERM_CAPACITY 64
#define CASE_CAPACITY 4
#define CASE_BINDER_CAPACITY 4
#define IH_SCOPE_CAPACITY 4

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

	const struct prototype_qualified_name argument_identity = {
		.namespace_symbol_id = 1,
		.name_symbol_id = 2
	};
	const struct prototype_qualified_name owner_identity = {
		.namespace_symbol_id = 1,
		.name_symbol_id = 3
	};
	uint32_t argument_core;
	uint32_t argument_source;
	uint32_t argument_view;
	uint32_t owner_core;
	uint32_t owner_source;
	uint32_t variable;
	uint32_t open_core;
	uint32_t open_source;
	uint32_t open_view;
	uint32_t expected_core;
	uint32_t expected_source;
	uint32_t expected_view;
	uint32_t substituted;
	uint32_t reindexed;
	uint32_t binding = prototype_term_new_binding(&terms);
	if (binding == PROTOTYPE_INVALID_ID ||
		prototype_term_type_former(&terms, 10, 2, &argument_core) != 0 ||
		prototype_term_type_declaration(
			&terms, argument_identity, &argument_source
		) != 0 || prototype_term_type_view(
			&terms,
			argument_identity,
			argument_core,
			argument_source,
			&argument_view
		) != 0 || prototype_term_type_former(
			&terms, 11, 1, &owner_core
		) != 0 || prototype_term_type_declaration(
			&terms, owner_identity, &owner_source
		) != 0 || prototype_term_var(
			&terms, binding, &variable
		) != 0 || prototype_term_app(
			&terms, owner_core, variable, &open_core
		) != 0 || prototype_term_app(
			&terms, owner_source, variable, &open_source
		) != 0 || prototype_term_type_view(
			&terms, owner_identity, open_core, open_source, &open_view
		) != 0 || prototype_term_app(
			&terms, owner_core, argument_core, &expected_core
		) != 0 || prototype_term_app(
			&terms, owner_source, argument_view, &expected_source
		) != 0 || prototype_term_type_view(
			&terms,
			owner_identity,
			expected_core,
			expected_source,
			&expected_view
		) != 0 || prototype_term_graph_substitute_bound_var(
			&terms, open_view, binding, argument_view, &substituted
		) != 0) {
		fprintf(stderr, "failed to construct TypeView substitution fixture\n");
		prototype_term_db_dispose_runtime_state(&terms);
		return 1;
	}
	if (substituted != expected_view ||
		terms.terms[substituted].tag != PROTOTYPE_TERM_TYPE_VIEW ||
		terms.terms[substituted].as.type_view.core != expected_core ||
		terms.terms[substituted].as.type_view.source != expected_source) {
		fprintf(stderr, "TypeView substitution mixed Core and source projections\n");
		prototype_term_db_dispose_runtime_state(&terms);
		return 1;
	}

	const struct prototype_binding_replacement replacement = {
		.binding_id = binding,
		.replacement = argument_view
	};
	if (prototype_term_graph_reindex_bindings(
			&terms, open_view, &replacement, 1, &reindexed
		) != 0 || reindexed != expected_view) {
		fprintf(stderr, "TypeView simultaneous reindex violated projection law\n");
		prototype_term_db_dispose_runtime_state(&terms);
		return 1;
	}

	prototype_term_db_dispose_runtime_state(&terms);
	printf("Core TypeView substitution checks passed\n");
	return 0;
}
