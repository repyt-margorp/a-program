#include "ast.h"

#include <stdio.h>
#include <string.h>

int main(void) {
	struct prototype_term terms[32];
	struct prototype_match_case cases[8];
	int case_labels[8];
	struct prototype_case_binder case_binders[8];
	struct prototype_match_frame frames[8];
	struct prototype_term_db term_db;
	struct prototype_context_db contexts;
	struct prototype_context context_storage[16];
	struct prototype_substitution_db substitutions;
	struct prototype_substitution substitution_storage[32];
	struct prototype_type_declaration_db type_declarations;
	struct prototype_type_declaration type_storage[4];
	struct prototype_type_constructor_declaration constructor_storage[4];
	struct prototype_type_parameter_declaration parameter_storage[4];
	uint32_t field_type_storage[8];
	struct prototype_type_expr type_expr_storage[8];
	uint32_t int_type;
	uint32_t text_type;
	uint32_t int_context;
	uint32_t same_int_context;
	uint32_t text_context;
	uint32_t nested_context;
	uint32_t dependent_context;
	uint32_t unresolved_left;
	uint32_t unresolved_right;
	uint32_t empty_substitution;
	uint32_t section;
	uint32_t dependent_section;
	uint32_t identity;
	uint32_t composed;
	uint32_t projection;
	uint32_t variable;
	uint32_t literal;
	uint32_t constant_family;
	uint32_t dependent_classifier;
	uint32_t reindexed;
	uint32_t composed_reindexed;
	uint32_t dependent_reindexed;
	uint32_t cloned_binders[2];
	uint32_t cloned_binder_count;
	uint32_t cloned_context;
	uint32_t clone_substitution;
	uint32_t cloned_classifier;
	struct prototype_operation_node operation_storage[2];
	struct prototype_operation_match_case operation_case_storage[1];
	struct prototype_operation_graph operation_graph;
	struct prototype_operation_node int_occurrence;
	struct prototype_operation_node text_occurrence;
	struct prototype_operation_node invalid_occurrence;
	uint32_t int_operation;
	uint32_t text_operation;

	prototype_term_db_init(
		&term_db,
		terms,
		32,
		cases,
		case_labels,
		8,
		case_binders,
		8,
		frames,
		8
	);
	prototype_type_declaration_db_init(
		&type_declarations,
		type_storage,
		4,
		constructor_storage,
		4,
		parameter_storage,
		4,
		field_type_storage,
		8,
		type_expr_storage,
		8
	);
	prototype_context_db_init(&contexts, context_storage, 16);
	prototype_substitution_db_init(
		&substitutions, substitution_storage, 32
	);
	if (prototype_term_primitive_int(&term_db, &int_type) != 0 ||
		prototype_term_primitive_text(&term_db, &text_type) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, int_type, PROTOTYPE_INVALID_ID, &int_context
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, int_type, PROTOTYPE_INVALID_ID, &same_int_context
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, text_type, PROTOTYPE_INVALID_ID, &text_context
		) != 0 ||
		prototype_context_extend(
			&contexts, int_context, 1, text_type, PROTOTYPE_INVALID_ID,
			&nested_context
		) != 0 ||
		prototype_term_var(&term_db, 0, &variable) != 0 ||
		prototype_term_lambda(&term_db, 0, int_type, &constant_family) != 0 ||
		prototype_term_app(
			&term_db, constant_family, variable, &dependent_classifier
		) != 0 ||
		prototype_context_extend(
			&contexts,
			int_context,
			1,
			dependent_classifier,
			PROTOTYPE_INVALID_ID,
			&dependent_context
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, PROTOTYPE_INVALID_ID, 17, &unresolved_left
		) != 0 ||
		prototype_context_extend(
			&contexts, 0, 0, PROTOTYPE_INVALID_ID, 18, &unresolved_right
		) != 0) {
		fprintf(stderr, "failed to construct categorical contexts\n");
		return 1;
	}
	if (prototype_context_empty(&contexts) != 0 ||
		int_context != same_int_context ||
		int_context == text_context ||
		unresolved_left == unresolved_right ||
		!prototype_context_contains_binder(&contexts, nested_context, 0) ||
		!prototype_context_contains_binder(&contexts, nested_context, 1) ||
		prototype_context_contains_binder(&contexts, text_context, 1) ||
		prototype_context_get(&contexts, nested_context)->depth != 2 ||
		prototype_context_db_validate(&contexts, &term_db) != 0) {
		fprintf(stderr, "categorical context law failed\n");
		return 1;
	}
	if (prototype_term_int_literal(&term_db, 7, &literal) != 0 ||
		prototype_substitution_empty(
			&substitutions, &contexts, 0, &empty_substitution
		) != 0 ||
		prototype_substitution_extend(
			&substitutions,
			&contexts,
			&term_db,
			&type_declarations,
			empty_substitution,
			int_context,
			literal,
			int_type,
			PROTOTYPE_INVALID_ID,
			&section
		) != 0 ||
		prototype_substitution_extend(
			&substitutions,
			&contexts,
			&term_db,
			&type_declarations,
			section,
			dependent_context,
			literal,
			int_type,
			PROTOTYPE_INVALID_ID,
			&dependent_section
		) != 0 ||
		prototype_substitution_identity(
			&substitutions, &contexts, int_context, &identity
		) != 0 ||
		prototype_substitution_projection(
			&substitutions, &contexts, int_context, &projection
		) != 0 ||
		prototype_substitution_compose(
			&substitutions, &contexts, identity, section, &composed
		) != 0 ||
		prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			section,
			&reindexed
		) != 0 ||
		prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			composed,
			&composed_reindexed
		) != 0 ||
		prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			dependent_classifier,
			section,
			&dependent_reindexed
		) != 0 ||
		prototype_context_fresh_reindex_extension(
			&contexts,
			&substitutions,
			&term_db,
			&type_declarations,
			prototype_context_empty(&contexts),
			dependent_context,
			cloned_binders,
			2,
			&cloned_binder_count,
			&cloned_context,
			&clone_substitution
		) != 0 ||
		prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			dependent_classifier,
			clone_substitution,
			&cloned_classifier
		) != 0 ||
		reindexed != literal ||
		composed_reindexed != literal ||
		cloned_binder_count != 2 ||
		prototype_substitution_get(
			&substitutions, clone_substitution
		)->source_context != cloned_context ||
		prototype_substitution_get(
			&substitutions, clone_substitution
		)->target_context != dependent_context ||
		!prototype_judgement_classifier_normalization_equal(
			&term_db,
			&type_declarations,
			dependent_reindexed,
			int_type
		) ||
		!prototype_judgement_classifier_normalization_equal(
			&term_db,
			&type_declarations,
			cloned_classifier,
			dependent_classifier
		) ||
		prototype_substitution_get(&substitutions, dependent_section) == NULL ||
		prototype_substitution_get(&substitutions, projection) == NULL ||
		prototype_substitution_db_validate(
			&substitutions, &contexts, &term_db
		) != 0) {
		fprintf(stderr, "categorical substitution law failed\n");
		return 1;
	}
	prototype_operation_graph_init(
		&operation_graph,
		operation_storage,
		2,
		operation_case_storage,
		1
	);
	memset(&int_occurrence, 0xff, sizeof(int_occurrence));
	int_occurrence.tag = PROTOTYPE_OPERATION_ATOM;
	int_occurrence.polarity = PROTOTYPE_OPERATION_POLARITY_VALUE;
	int_occurrence.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	int_occurrence.application_role = PROTOTYPE_TERM_APPLICATION_NONE;
	int_occurrence.context_id = int_context;
	int_occurrence.core_term = literal;
	int_occurrence.source_symbol_id = -1;
	int_occurrence.binder_symbol_id = -1;
	int_occurrence.case_count = 0;
	memset(&text_occurrence, 0xff, sizeof(text_occurrence));
	text_occurrence = int_occurrence;
	text_occurrence.context_id = text_context;
	invalid_occurrence = int_occurrence;
	invalid_occurrence.context_id = 99;
	if (prototype_operation_graph_add(
			&operation_graph, &contexts, int_occurrence, &int_operation
		) != 0 ||
		prototype_operation_graph_add(
			&operation_graph, &contexts, text_occurrence, &text_operation
		) != 0 ||
		prototype_operation_graph_add(
			&operation_graph, &contexts, invalid_occurrence, NULL
		) == 0 ||
		int_operation == text_operation ||
		operation_graph.operations[int_operation].core_term !=
			operation_graph.operations[text_operation].core_term ||
		operation_graph.operations[int_operation].context_id ==
			operation_graph.operations[text_operation].context_id ||
		prototype_operation_graph_validate(
			&operation_graph, &term_db, &contexts
		) != 0) {
		fprintf(stderr, "context-indexed operation graph law failed\n");
		return 1;
	}
	printf("context category checks passed\n");
	return 0;
}
