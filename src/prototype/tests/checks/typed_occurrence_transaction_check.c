#include "a_program/core/term.h"
#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/kernel/context.h"

#include <string.h>

int main(void) {
	struct prototype_term term_storage[8];
	struct prototype_match_case case_storage[1];
	int case_label_storage[1];
	struct prototype_case_binder case_binder_storage[1];
	struct prototype_ih_scope ih_scope_storage[1];
	struct prototype_term_db terms;
	prototype_term_db_init(
		&terms,
		term_storage,
		8,
		case_storage,
		case_label_storage,
		1,
		case_binder_storage,
		1,
		ih_scope_storage,
		1
	);
	uint32_t text;
	if (prototype_term_primitive_text(&terms, &text) != 0) {
		return 1;
	}

	struct prototype_context context_storage[2];
	struct prototype_context_db contexts;
	prototype_context_db_init(&contexts, context_storage, 2);
	uint32_t empty = prototype_context_empty(&contexts);
	if (empty == PROTOTYPE_INVALID_ID) {
		return 2;
	}

	struct prototype_typed_occurrence occurrence_storage[4];
	struct prototype_typed_occurrence_edge edge_storage[2];
	struct prototype_typed_occurrence_match_case match_case_storage[1];
	struct prototype_typed_occurrence_fold_clause fold_clause_storage[1];
	struct prototype_typed_occurrence_graph graph;
	prototype_typed_occurrence_graph_init(
		&graph,
		occurrence_storage,
		4,
		edge_storage,
		2,
		match_case_storage,
		1,
		fold_clause_storage,
		1
	);
	struct prototype_typed_occurrence occurrence;
	memset(&occurrence, 0, sizeof(occurrence));
	occurrence.tag = PROTOTYPE_TYPED_OCCURRENCE_ATOM;
	occurrence.category = PROTOTYPE_TERM_CATEGORY_VALUE;
	occurrence.context_id = empty;
	occurrence.context_action_substitution = PROTOTYPE_INVALID_ID;
	occurrence.source_core_term = PROTOTYPE_INVALID_ID;
	occurrence.source_classifier = PROTOTYPE_INVALID_ID;
	occurrence.core_term = text;
	occurrence.classifier = text;
	occurrence.classifier_status = PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED;
	occurrence.classifier_verification_obligation = PROTOTYPE_INVALID_ID;
	occurrence.source_ast = PROTOTYPE_INVALID_ID;
	occurrence.referenced_ast_binder_id = PROTOTYPE_INVALID_ID;
	occurrence.binding_id = PROTOTYPE_INVALID_ID;
	occurrence.first_edge = PROTOTYPE_INVALID_ID;
	occurrence.wrapped_occurrence = PROTOTYPE_INVALID_ID;
	occurrence.binder_classifier = PROTOTYPE_INVALID_ID;
	occurrence.ih_owner_occurrence = PROTOTYPE_INVALID_ID;
	occurrence.ih_scope_id = PROTOTYPE_INVALID_ID;
	occurrence.ih_case_index = PROTOTYPE_INVALID_ID;
	occurrence.ih_field_index = PROTOTYPE_INVALID_ID;
	occurrence.first_case = PROTOTYPE_INVALID_ID;
	occurrence.first_fold_clause = PROTOTYPE_INVALID_ID;
	uint32_t first;
	if (prototype_typed_occurrence_graph_add(
			&graph, &contexts, occurrence, &first
		) != 0 || first != 0 || prototype_typed_occurrence_graph_freeze(
			&graph, &terms, &contexts
		) != 0) {
		return 3;
	}
	struct prototype_typed_occurrence_graph snapshot = graph;
	if (prototype_typed_occurrence_graph_begin_transaction(
			&graph, &terms, &contexts
		) != 0 || graph.frozen || graph.sealed || !graph.transaction_active ||
		!snapshot.frozen || !snapshot.sealed) {
		return 4;
	}
	uint32_t second;
	if (prototype_typed_occurrence_graph_add(
			&graph, &contexts, occurrence, &second
		) != 0 || second != 1 || snapshot.occurrence_count != 1 ||
		prototype_typed_occurrence_graph_add_edge(
			&graph,
			first,
			(struct prototype_typed_occurrence_edge) {
				.role = PROTOTYPE_TERM_CHILD_FUNCTION,
				.ordinal = 0,
				.child_occurrence = second
			}
		) == 0) {
		return 5;
	}
	if (prototype_typed_occurrence_graph_rollback_transaction(&graph) != 0 ||
		graph.occurrence_count != 1 || !graph.frozen || !graph.sealed ||
		graph.transaction_active) {
		return 6;
	}
	if (prototype_typed_occurrence_graph_begin_transaction(
			&graph, &terms, &contexts
		) != 0 || prototype_typed_occurrence_graph_add(
			&graph, &contexts, occurrence, &second
		) != 0 || prototype_typed_occurrence_graph_freeze(
			&graph, &terms, &contexts
		) != 0 || graph.occurrence_count != 2 || !graph.frozen ||
		graph.transaction_active) {
		return 7;
	}
	return 0;
}
