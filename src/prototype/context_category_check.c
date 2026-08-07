#include "ast.h"

#include <stdio.h>
#include <string.h>

int main(void) {
	struct prototype_term terms[128];
	struct prototype_match_case cases[8];
	int case_labels[8];
	struct prototype_case_binder case_binders[8];
	struct prototype_ih_scope frames[8];
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
	uint32_t empty_identity;
	uint32_t right_identity_composed;
	uint32_t identity_squared;
	uint32_t associative_left;
	uint32_t associative_right;
	uint32_t law_reindexed;
	struct prototype_operation_node operation_storage[3];
	struct prototype_operation_match_case operation_case_storage[1];
	struct prototype_operation_computation_fold_clause operation_fold_clause_storage[1];
	struct prototype_operation_graph operation_graph;
	struct prototype_operation_node int_occurrence;
	struct prototype_operation_node text_occurrence;
	struct prototype_operation_node invalid_occurrence;
	struct prototype_operation_node saturated_effect_occurrence;
	uint32_t int_operation;
	uint32_t text_operation;
	uint32_t effect_operation;
	uint32_t effect_application;

	prototype_term_db_init(
		&term_db,
		terms,
		128,
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
			&contexts, 0, 99, int_type, PROTOTYPE_INVALID_ID, &same_int_context
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
		prototype_context_get(&contexts, same_int_context)->binding_id != 0 ||
		int_context == text_context ||
		unresolved_left == unresolved_right ||
		!prototype_context_contains_binding(&contexts, nested_context, 0) ||
		!prototype_context_contains_binding(&contexts, nested_context, 1) ||
		prototype_context_contains_binding(&contexts, text_context, 1) ||
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
		!(prototype_judgement_classifier_conversion(
			&term_db,
			&type_declarations,
			dependent_reindexed,
			int_type
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
		!(prototype_judgement_classifier_conversion(
			&term_db,
			&type_declarations,
			cloned_classifier,
			dependent_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
		prototype_substitution_get(&substitutions, dependent_section) == NULL ||
		prototype_substitution_get(&substitutions, projection) == NULL ||
		prototype_substitution_db_validate(
			&substitutions, &contexts, &term_db
		) != 0) {
		fprintf(stderr, "categorical substitution law failed\n");
		return 1;
	}
	if (prototype_substitution_identity(
			&substitutions,
			&contexts,
			prototype_context_empty(&contexts),
			&empty_identity
		) != 0 || prototype_substitution_compose(
			&substitutions,
			&contexts,
			section,
			empty_identity,
			&right_identity_composed
		) != 0 || prototype_substitution_compose(
			&substitutions,
			&contexts,
			identity,
			identity,
			&identity_squared
		) != 0 || prototype_substitution_compose(
			&substitutions,
			&contexts,
			identity_squared,
			section,
			&associative_left
		) != 0 || prototype_substitution_compose(
			&substitutions,
			&contexts,
			identity,
			composed,
			&associative_right
		) != 0 || prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			right_identity_composed,
			&law_reindexed
		) != 0 || law_reindexed != literal || prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			associative_left,
			&law_reindexed
		) != 0 || law_reindexed != literal || prototype_term_reindex(
			&term_db,
			&type_declarations,
			&contexts,
			&substitutions,
			variable,
			associative_right,
			&law_reindexed
		) != 0 || law_reindexed != literal) {
		fprintf(stderr, "substitution identity/associativity law failed\n");
		return 1;
	}
	if (prototype_substitution_compose(
			&substitutions,
			&contexts,
			section,
			identity,
			&law_reindexed
		) == 0) {
		fprintf(stderr, "substitution context mismatch was accepted\n");
		return 1;
	}
	{
		enum { LARGE_CONTEXT_DEPTH = 513 };
		static struct prototype_term large_term_storage[16];
		static struct prototype_match_case large_case_storage[1];
		static int large_case_labels[1];
		static struct prototype_case_binder large_case_binders[1];
		static struct prototype_ih_scope large_frames[1];
		static struct prototype_context
			large_context_storage[LARGE_CONTEXT_DEPTH + 1];
		static struct prototype_substitution
			large_substitution_storage[LARGE_CONTEXT_DEPTH + 1];
		struct prototype_term_db large_terms;
		struct prototype_context_db large_contexts;
		struct prototype_substitution_db large_substitutions;
		uint32_t large_int_type;
		uint32_t large_literal;
		uint32_t large_context = 0;
		uint32_t large_variable;
		uint32_t large_result;

		prototype_term_db_init(
			&large_terms,
			large_term_storage,
			16,
			large_case_storage,
			large_case_labels,
			1,
			large_case_binders,
			1,
			large_frames,
			1
		);
		prototype_context_db_init(
			&large_contexts,
			large_context_storage,
			LARGE_CONTEXT_DEPTH + 1
		);
		prototype_substitution_db_init(
			&large_substitutions,
			large_substitution_storage,
			LARGE_CONTEXT_DEPTH + 1
		);
		if (prototype_term_primitive_int(&large_terms, &large_int_type) != 0 ||
			prototype_term_int_literal(&large_terms, 7, &large_literal) != 0) {
			fprintf(stderr, "failed to initialize large reindex fixture\n");
			return 1;
		}
		large_substitution_storage[0] = (struct prototype_substitution) {
			.kind = PROTOTYPE_SUBSTITUTION_EMPTY,
			.source_context = 0,
			.target_context = 0,
			.first = PROTOTYPE_INVALID_ID,
			.second = PROTOTYPE_INVALID_ID,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID,
			.term_proof_id = PROTOTYPE_INVALID_ID
		};
		large_substitutions.substitution_count = 1;
		for (uint32_t i = 0; i < LARGE_CONTEXT_DEPTH; ++i) {
			uint32_t extended_context;
			if (prototype_context_extend(
					&large_contexts,
					large_context,
					1000 + i,
					large_int_type,
					PROTOTYPE_INVALID_ID,
					&extended_context
				) != 0) {
				fprintf(stderr, "failed to extend large context\n");
				return 1;
			}
			large_substitution_storage[i + 1] =
				(struct prototype_substitution) {
					.kind = PROTOTYPE_SUBSTITUTION_EXTEND,
					.source_context = 0,
					.target_context = extended_context,
					.first = i,
					.second = PROTOTYPE_INVALID_ID,
					.term = large_literal,
					.term_classifier = large_int_type,
					.term_proof_id = PROTOTYPE_INVALID_ID
				};
			large_substitutions.substitution_count++;
			large_context = extended_context;
		}
		uint32_t binding_count_before = large_terms.next_binding_id;
		if (prototype_term_var(
				&large_terms, 1000 + LARGE_CONTEXT_DEPTH - 1, &large_variable
			) != 0 || prototype_context_db_validate(
				&large_contexts, &large_terms
			) != 0 || prototype_substitution_db_validate(
				&large_substitutions, &large_contexts, &large_terms
			) != 0 || prototype_term_reindex(
				&large_terms,
				&type_declarations,
				&large_contexts,
				&large_substitutions,
				large_variable,
				LARGE_CONTEXT_DEPTH,
				&large_result
			) != 0 || large_result != large_literal ||
			large_terms.next_binding_id != binding_count_before) {
			fprintf(stderr, "large direct reindex law failed\n");
			return 1;
		}
	}
	{
		struct prototype_context malformed_context_storage[1];
		struct prototype_context_db malformed_contexts;
		struct prototype_substitution malformed_substitution_storage[1];
		struct prototype_substitution_db malformed_substitutions;

		prototype_context_db_init(
			&malformed_contexts, malformed_context_storage, 1
		);
		malformed_context_storage[0].depth = 1;
		prototype_substitution_db_init(
			&malformed_substitutions, malformed_substitution_storage, 1
		);
		malformed_substitutions.substitution_count = 1;
		malformed_substitution_storage[0] = (struct prototype_substitution) {
			.kind = PROTOTYPE_SUBSTITUTION_COMPOSE,
			.source_context = prototype_context_empty(&contexts),
			.target_context = prototype_context_empty(&contexts),
			.first = 0,
			.second = 0,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID,
			.term_proof_id = PROTOTYPE_INVALID_ID
		};
		if (prototype_context_db_validate(
				&malformed_contexts, &term_db
			) == 0 || prototype_substitution_db_validate(
				&malformed_substitutions, &contexts, &term_db
			) == 0) {
			fprintf(stderr, "malformed categorical storage was accepted\n");
			return 1;
		}
	}
	{
		struct prototype_context source_context_storage[4];
		struct prototype_context target_context_storage[4];
		struct prototype_context_db source_contexts;
		struct prototype_context_db target_contexts;
		struct prototype_substitution source_substitution_storage[4];
		struct prototype_substitution target_substitution_storage[4];
		struct prototype_substitution_db source_substitutions;
		struct prototype_substitution_db target_substitutions;
		uint32_t source_context;
		uint32_t source_empty;
		uint32_t source_section;
		uint32_t context_relocation[4];

		prototype_context_db_init(
			&source_contexts, source_context_storage, 4
		);
		prototype_context_db_init(
			&target_contexts, target_context_storage, 4
		);
		prototype_substitution_db_init(
			&source_substitutions, source_substitution_storage, 4
		);
		prototype_substitution_db_init(
			&target_substitutions, target_substitution_storage, 4
		);
		if (prototype_context_extend(
				&source_contexts,
				prototype_context_empty(&source_contexts),
				41,
				int_type,
				PROTOTYPE_INVALID_ID,
				&source_context
			) != 0 || prototype_substitution_empty(
				&source_substitutions,
				&source_contexts,
				source_context,
				&source_empty
			) != 0 || prototype_substitution_extend(
				&source_substitutions,
				&source_contexts,
				&term_db,
				&type_declarations,
				source_empty,
				source_context,
				literal,
				int_type,
				5,
				&source_section
			) != 0 || prototype_context_db_append_relocated(
				&target_contexts,
				&source_contexts,
				3,
				100,
				context_relocation,
				4
			) != 0 || prototype_substitution_db_append_relocated(
				&target_substitutions,
				&source_substitutions,
				context_relocation,
				source_contexts.context_count,
				3,
				7
			) != 0 || source_section != 1 ||
			target_contexts.context_count != 2 ||
			target_contexts.contexts[context_relocation[source_context]].binding_id !=
				141 ||
			target_contexts.contexts[
				context_relocation[source_context]
			].classifier != int_type + 3 ||
			target_substitutions.substitution_count != 2 ||
			target_substitutions.substitutions[1].source_context !=
				context_relocation[source_context] ||
			target_substitutions.substitutions[1].target_context !=
				context_relocation[source_context] ||
			target_substitutions.substitutions[1].first != 0 ||
			target_substitutions.substitutions[1].term != literal + 3 ||
			target_substitutions.substitutions[1].term_classifier != int_type + 3 ||
			target_substitutions.substitutions[1].term_proof_id != 12) {
			fprintf(stderr, "context/substitution relocation law failed\n");
			return 1;
		}
	}
	prototype_operation_graph_init(
		&operation_graph,
		operation_storage,
		3,
		operation_case_storage,
		1,
		operation_fold_clause_storage,
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
	if (prototype_term_effect_operation(
			&term_db, PROTOTYPE_EFFECT_OPERATION_PRINT, &effect_operation
		) != 0 || prototype_term_app(
			&term_db, effect_operation, literal, &effect_application
		) != 0) {
		fprintf(stderr, "failed to construct saturated effect application\n");
		return 1;
	}
	saturated_effect_occurrence = int_occurrence;
	saturated_effect_occurrence.tag = PROTOTYPE_OPERATION_APP;
	saturated_effect_occurrence.polarity = PROTOTYPE_OPERATION_POLARITY_COMPUTATION;
	saturated_effect_occurrence.computation_kind =
		PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
	saturated_effect_occurrence.application_role =
		PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION;
	saturated_effect_occurrence.core_term = effect_application;
	saturated_effect_occurrence.function = int_operation;
	saturated_effect_occurrence.argument = text_operation;
	if (prototype_operation_graph_add(
			&operation_graph,
			&contexts,
			saturated_effect_occurrence,
			NULL
		) != 0 || prototype_operation_graph_validate(
			&operation_graph, &term_db, &contexts
		) == 0) {
		fprintf(stderr, "saturated effect APP escaped request validation\n");
		return 1;
	}
	printf("context category checks passed\n");
	return 0;
}
