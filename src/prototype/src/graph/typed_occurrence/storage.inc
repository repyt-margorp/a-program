#include "a_program/graph/typed_occurrence_graph.h"

#include "a_program/graph/runtime.h"
#include "a_program/graph/verification.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/graph/compile_metadata.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../internal/ast_common.h"

void prototype_typed_occurrence_graph_init(
	struct prototype_typed_occurrence_graph* graph,
	struct prototype_typed_occurrence* occurrences,
	size_t occurrence_capacity,
	struct prototype_typed_occurrence_edge* edges,
	size_t edge_capacity,
	struct prototype_typed_occurrence_match_case* cases,
	size_t case_capacity,
	struct prototype_typed_occurrence_fold_clause* fold_clauses,
	size_t fold_clause_capacity
) {
	if (!graph) {
		return;
	}
	memset(graph, 0, sizeof(*graph));
	graph->occurrences = occurrences;
	graph->occurrence_capacity = occurrence_capacity;
	graph->edges = edges;
	graph->edge_capacity = edge_capacity;
	graph->cases = cases;
	graph->case_capacity = case_capacity;
	graph->fold_clauses = fold_clauses;
	graph->fold_clause_capacity = fold_clause_capacity;
}

size_t prototype_typed_occurrence_graph_count(const struct prototype_typed_occurrence_graph* graph) {
	return graph ? graph->occurrence_count : 0;
}

size_t prototype_typed_occurrence_graph_case_count(const struct prototype_typed_occurrence_graph* graph) {
	return graph ? graph->case_count : 0;
}

const struct prototype_typed_occurrence* prototype_typed_occurrence_graph_get(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t operation_id
) {
	return graph && operation_id < graph->occurrence_count ?
		&graph->occurrences[operation_id] : NULL;
}

const struct prototype_typed_occurrence_edge*
prototype_typed_occurrence_graph_get_edge(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t edge_id
) {
	return graph && edge_id < graph->edge_count ? &graph->edges[edge_id] : NULL;
}

int prototype_typed_occurrence_graph_child(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t parent_occurrence,
	int role,
	uint32_t ordinal,
	uint32_t* p_child_occurrence
) {
	if (!graph || !p_child_occurrence ||
		parent_occurrence >= graph->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* parent =
		&graph->occurrences[parent_occurrence];
	if (parent->first_edge == PROTOTYPE_INVALID_ID ||
		parent->first_edge > graph->edge_count ||
		parent->edge_count > graph->edge_count - parent->first_edge) {
		return -1;
	}
	for (uint32_t i = 0; i < parent->edge_count; ++i) {
		const struct prototype_typed_occurrence_edge* edge =
			&graph->edges[parent->first_edge + i];
		if (edge->role == role && edge->ordinal == ordinal &&
			edge->child_occurrence < graph->occurrence_count) {
			*p_child_occurrence = edge->child_occurrence;
			return 0;
		}
	}
	return 1;
}

int prototype_typed_occurrence_graph_add_edge(
	struct prototype_typed_occurrence_graph* graph,
	uint32_t parent_occurrence,
	struct prototype_typed_occurrence_edge edge
) {
	if (!graph || graph->frozen || graph->sealed || !graph->edges ||
		parent_occurrence >= graph->occurrence_count ||
		(graph->transaction_active &&
		 parent_occurrence < graph->transaction_occurrence_start) ||
		edge.child_occurrence >= graph->occurrence_count ||
		reserve_slot(graph->edge_count, graph->edge_capacity) != 0) {
		return -1;
	}
	struct prototype_typed_occurrence* parent =
		&graph->occurrences[parent_occurrence];
	if (parent->edge_count == 0) {
		parent->first_edge = (uint32_t)graph->edge_count;
	} else if (parent->first_edge > graph->edge_count ||
		parent->edge_count != graph->edge_count - parent->first_edge) {
		return -1;
	}
	graph->edges[graph->edge_count++] = edge;
	parent->edge_count++;
	return 0;
}

int prototype_typed_occurrence_graph_selected_classifier(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t operation_id,
	uint32_t* p_classifier
) {
	const struct prototype_typed_occurrence* operation =
		prototype_typed_occurrence_graph_get(graph, operation_id);
	if (!operation || !p_classifier ||
		operation->classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_classifier = operation->classifier;
	return 0;
}

const struct prototype_typed_occurrence_match_case* prototype_typed_occurrence_graph_get_case(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t case_id
) {
	return graph && case_id < graph->case_count ?
		&graph->cases[case_id] : NULL;
}

const struct prototype_typed_occurrence_fold_clause*
prototype_typed_occurrence_graph_get_fold_clause(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t clause_id
) {
	return graph && clause_id < graph->fold_clause_count ?
		&graph->fold_clauses[clause_id] : NULL;
}

int prototype_typed_occurrence_graph_add(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_typed_occurrence operation,
	uint32_t* p_operation_id
) {
	if (!graph || graph->frozen || graph->sealed ||
		!contexts || !graph->occurrences ||
		!prototype_context_get(contexts, operation.context_id) ||
		reserve_slot(graph->occurrence_count, graph->occurrence_capacity) != 0) {
		return -1;
	}
	if (p_operation_id) {
		*p_operation_id = (uint32_t)graph->occurrence_count;
	}
	if (operation.tag != PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
		operation.tag != PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		operation.wrapped_occurrence = PROTOTYPE_INVALID_ID;
	}
	if (operation.tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		operation.ih_owner_occurrence = PROTOTYPE_INVALID_ID;
	}
	operation.match_motive = PROTOTYPE_INVALID_ID;
	if (operation.classifier_status ==
			PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_PENDING) {
		operation.classifier_verification_obligation = PROTOTYPE_INVALID_ID;
	}
	graph->occurrences[graph->occurrence_count++] = operation;
	return 0;
}

int prototype_typed_occurrence_graph_add_case(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_typed_occurrence_match_case operation_case,
	uint32_t* p_case_id
) {
	if (!graph || graph->frozen || graph->sealed ||
		!contexts || !graph->cases ||
		!prototype_context_get(contexts, operation_case.context_id) ||
		operation_case.binder_count >
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY ||
		reserve_slot(graph->case_count, graph->case_capacity) != 0) {
		return -1;
	}
	for (uint32_t binder_index = 0;
		binder_index < operation_case.binder_count; ++binder_index) {
		if (!prototype_context_contains_binding(
				contexts, operation_case.context_id,
				operation_case.binder_ids[binder_index]
			)) {
			return -1;
		}
	}
	if (operation_case.refinement_status !=
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED) {
		operation_case.refinement_substitution = PROTOTYPE_INVALID_ID;
	}
	if (p_case_id) {
		*p_case_id = (uint32_t)graph->case_count;
	}
	graph->cases[graph->case_count++] = operation_case;
	return 0;
}

int prototype_typed_occurrence_graph_add_fold_clause(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_typed_occurrence_fold_clause clause,
	uint32_t* p_clause_id
) {
	if (!graph || graph->frozen || graph->sealed ||
		!contexts || !graph->fold_clauses ||
		!prototype_context_get(contexts, clause.context_id) ||
		reserve_slot(graph->fold_clause_count, graph->fold_clause_capacity) != 0) {
		return -1;
	}
	if (p_clause_id) {
		*p_clause_id = (uint32_t)graph->fold_clause_count;
	}
	graph->fold_clauses[graph->fold_clause_count++] = clause;
	return 0;
}

static int typed_occurrence_expected_core_tag(
	const struct prototype_typed_occurrence* occurrence,
	int* p_core_tag
) {
	if (!occurrence || !p_core_tag) {
		return 0;
	}
	switch (occurrence->tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
			*p_core_tag = PROTOTYPE_TERM_APP;
			return 1;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			*p_core_tag = PROTOTYPE_TERM_LAMBDA;
			return 1;
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
			*p_core_tag = PROTOTYPE_TERM_MATCH;
			return 1;
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			*p_core_tag = PROTOTYPE_TERM_INDUCTION_HYPOTHESIS;
			return 1;
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
			*p_core_tag = PROTOTYPE_TERM_RETURN;
			return 1;
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
			*p_core_tag = PROTOTYPE_TERM_THUNK;
			return 1;
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
			*p_core_tag = PROTOTYPE_TERM_FORCE;
			return 1;
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
			*p_core_tag = PROTOTYPE_TERM_OPERATION_REQUEST;
			return 1;
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
			*p_core_tag = PROTOTYPE_TERM_COMPUTATION_FOLD;
			return 1;
		default:
			return 0;
	}
}

static int typed_occurrence_structural_core(
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence* occurrence,
	uint32_t term_id,
	uint32_t* p_structural_core
) {
	int expected_core_tag;
	uint32_t structural_core;
	if (!terms || !occurrence || !p_structural_core ||
		!typed_occurrence_expected_core_tag(occurrence, &expected_core_tag) ||
		prototype_term_core_projection(terms, term_id, &structural_core) != 0 ||
		terms->terms[structural_core].tag != expected_core_tag) {
		return -1;
	}
	*p_structural_core = structural_core;
	return 0;
}

int prototype_typed_occurrence_core_child_count(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t occurrence_id,
	uint32_t* p_count
) {
	const struct prototype_typed_occurrence* occurrence =
		prototype_typed_occurrence_graph_get(graph, occurrence_id);
	int expected_core_tag;
	uint32_t structural_core;
	if (!occurrence || !terms || !p_count ||
		occurrence->core_term >= terms->term_count) {
		return -1;
	}
	if (!typed_occurrence_expected_core_tag(occurrence, &expected_core_tag)) {
		*p_count = 0;
		return 0;
	}
	if (typed_occurrence_structural_core(
			terms, occurrence, occurrence->core_term, &structural_core
		) != 0) {
		return -1;
	}
	return prototype_term_child_count(terms, structural_core, p_count);
}

static int typed_occurrence_child_core_shape_equal(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t parent_occurrence_id,
	uint32_t parent_core,
	const struct prototype_term_child* core_child,
	uint32_t child_core,
	int* p_equal
) {
	const struct prototype_typed_occurrence* parent =
		prototype_typed_occurrence_graph_get(graph, parent_occurrence_id);
	if (!parent || !terms || !core_child || !p_equal ||
		parent_core >= terms->term_count || child_core >= terms->term_count) {
		return -1;
	}
	if (core_child->term == child_core) {
		*p_equal = 1;
		return 0;
	}
	if (core_child->role == PROTOTYPE_TERM_CHILD_BODY &&
		terms->terms[parent_core].tag == PROTOTYPE_TERM_LAMBDA) {
		return prototype_term_core_shape_equal_under_binder(
			terms,
			terms->terms[parent_core].as.lambda.binding_id,
			core_child->term,
			parent->binding_id,
			child_core,
			p_equal
		);
	}
	if (core_child->role == PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY &&
		terms->terms[parent_core].tag == PROTOTYPE_TERM_MATCH &&
		core_child->ordinal < terms->terms[parent_core].as.match.case_count &&
		parent->first_case != PROTOTYPE_INVALID_ID &&
		core_child->ordinal < parent->case_count &&
		parent->first_case + core_child->ordinal < graph->case_count) {
		const struct prototype_match_case* core_case = &terms->cases[
			terms->terms[parent_core].as.match.first_case + core_child->ordinal
		];
		const struct prototype_typed_occurrence_match_case* occurrence_case =
			&graph->cases[parent->first_case + core_child->ordinal];
		if (core_case->binder_count != occurrence_case->binder_count) {
			return -1;
		}
		uint32_t core_binders[core_case->binder_count == 0 ? 1 : core_case->binder_count];
		for (uint32_t i = 0; i < core_case->binder_count; ++i) {
			core_binders[i] = terms->case_binders[
				core_case->first_binder + i
			].binding_id;
		}
		uint32_t left_ih_scope = terms->terms[parent_core].as.match.ih_scope_id;
		uint32_t right_ih_scope = left_ih_scope;
		int found_occurrence_scope = 0;
		/* Alpha sharing may reuse a Core Match whose IH scope differs from the
		 * scope owned by this typed occurrence. The explicit IH owner relation
		 * supplies that frame correspondence. */
		for (uint32_t i = 0; i < graph->occurrence_count; ++i) {
			const struct prototype_typed_occurrence* candidate =
				&graph->occurrences[i];
			if (candidate->tag !=
					PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
				candidate->ih_owner_occurrence != parent_occurrence_id) {
				continue;
			}
			if (found_occurrence_scope && right_ih_scope != candidate->ih_scope_id) {
				return -1;
			}
			right_ih_scope = candidate->ih_scope_id;
			found_occurrence_scope = 1;
		}
		return prototype_term_core_shape_equal_under_binders_and_ih_scope(
			terms,
			core_binders,
			occurrence_case->binder_ids,
			core_case->binder_count,
			left_ih_scope,
			right_ih_scope,
			core_child->term,
			child_core,
			p_equal
		);
	}
	return prototype_term_core_shape_equal(
		terms, child_core, core_child->term, p_equal
	);
}

int prototype_typed_occurrence_core_child(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t occurrence_id,
	uint32_t child_index,
	uint32_t* p_child_occurrence
) {
	const struct prototype_typed_occurrence* occurrence =
		prototype_typed_occurrence_graph_get(graph, occurrence_id);
	struct prototype_term_child core_child;
	int expected_core_tag;
	uint32_t structural_core;
	uint32_t child = PROTOTYPE_INVALID_ID;
	if (!occurrence || !terms || !p_child_occurrence ||
		!typed_occurrence_expected_core_tag(occurrence, &expected_core_tag) ||
		typed_occurrence_structural_core(
			terms, occurrence, occurrence->core_term, &structural_core
		) != 0 ||
		prototype_term_child_at(
			terms, structural_core, child_index, &core_child
		) != 0) {
		return -1;
	}
	if (occurrence->first_edge > graph->edge_count ||
		occurrence->edge_count > graph->edge_count - occurrence->first_edge ||
		child_index >= occurrence->edge_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_edge* edge =
		&graph->edges[occurrence->first_edge + child_index];
	if (edge->role != core_child.role || edge->ordinal != core_child.ordinal) {
		return -1;
	}
	child = edge->child_occurrence;
	int same_core_shape = 0;
	const struct prototype_typed_occurrence* child_occurrence =
		child < graph->occurrence_count ? &graph->occurrences[child] : NULL;
	if (child_occurrence) {
		(void)typed_occurrence_child_core_shape_equal(
			graph, terms, occurrence_id, structural_core, &core_child,
			child_occurrence->core_term,
			&same_core_shape
		);
	}
	if (!same_core_shape && child_occurrence &&
		occurrence->context_action_substitution != PROTOTYPE_INVALID_ID &&
		occurrence->source_core_term != PROTOTYPE_INVALID_ID &&
		child_occurrence->context_action_substitution != PROTOTYPE_INVALID_ID &&
		child_occurrence->source_core_term != PROTOTYPE_INVALID_ID) {
		struct prototype_term_child source_core_child;
		uint32_t structural_source_core;
		if (typed_occurrence_structural_core(
				terms, occurrence, occurrence->source_core_term,
				&structural_source_core
			) == 0 && prototype_term_child_at(
				terms, structural_source_core, child_index,
				&source_core_child
			) == 0 && source_core_child.role == core_child.role &&
			source_core_child.ordinal == core_child.ordinal) {
			(void)typed_occurrence_child_core_shape_equal(
				graph, terms, occurrence_id, structural_source_core,
				&source_core_child, child_occurrence->source_core_term,
				&same_core_shape
			);
		}
	}
	if (!same_core_shape && child_occurrence &&
		occurrence->context_action_substitution == PROTOTYPE_INVALID_ID &&
		child_occurrence->context_action_substitution != PROTOTYPE_INVALID_ID &&
		child_occurrence->source_core_term != PROTOTYPE_INVALID_ID) {
		(void)typed_occurrence_child_core_shape_equal(
			graph, terms, occurrence_id, structural_core, &core_child,
			child_occurrence->source_core_term, &same_core_shape
		);
	}
	if (child >= graph->occurrence_count || !same_core_shape) {
		return -1;
	}
	*p_child_occurrence = child;
	return 0;
}

static int typed_occurrence_graph_reaches_at_depth(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t root_operation,
	uint32_t target_operation,
	unsigned char* visited
) {
	if (!graph || !terms || !visited || root_operation >= graph->occurrence_count ||
		target_operation >= graph->occurrence_count) {
		return -1;
	}
	if (root_operation == target_operation) {
		return 1;
	}
	if (visited[root_operation]) {
		return 0;
	}
	visited[root_operation] = 1;
	const struct prototype_typed_occurrence* operation =
		&graph->occurrences[root_operation];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		uint32_t child = operation->wrapped_occurrence;
		int reaches = typed_occurrence_graph_reaches_at_depth(
			graph, terms, child, target_operation, visited
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	uint32_t child_count;
	if (prototype_typed_occurrence_core_child_count(
			graph, terms, root_operation, &child_count
		) != 0) {
		return -1;
	}
	for (uint32_t child_index = 0; child_index < child_count; ++child_index) {
		uint32_t child;
		if (prototype_typed_occurrence_core_child(
				graph, terms, root_operation, child_index, &child
			) != 0) {
			return -1;
		}
		int reaches = typed_occurrence_graph_reaches_at_depth(
			graph, terms, child, target_operation, visited
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	return 0;
}

int prototype_typed_occurrence_graph_reaches(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t root_operation,
	uint32_t target_operation
) {
	if (!graph || !terms || root_operation >= graph->occurrence_count ||
		target_operation >= graph->occurrence_count) {
		return -1;
	}
	unsigned char visited[graph->occurrence_count];
	memset(visited, 0, sizeof(visited));
	return typed_occurrence_graph_reaches_at_depth(
		graph, terms, root_operation, target_operation, visited
	);
}
