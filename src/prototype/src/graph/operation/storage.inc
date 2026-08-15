#include "a_program/graph/operation_graph.h"

#include "a_program/graph/operation_runtime.h"
#include "a_program/graph/verification.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/graph/compile_metadata.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../internal/ast_common.h"

void prototype_operation_graph_init(
	struct prototype_operation_graph* graph,
	struct prototype_operation_node* operations,
	size_t operation_capacity,
	struct prototype_operation_match_case* cases,
	size_t case_capacity,
	struct prototype_operation_computation_fold_clause* fold_clauses,
	size_t fold_clause_capacity
) {
	if (!graph) {
		return;
	}
	memset(graph, 0, sizeof(*graph));
	graph->operations = operations;
	graph->operation_capacity = operation_capacity;
	graph->cases = cases;
	graph->case_capacity = case_capacity;
	graph->fold_clauses = fold_clauses;
	graph->fold_clause_capacity = fold_clause_capacity;
}

size_t prototype_operation_graph_count(const struct prototype_operation_graph* graph) {
	return graph ? graph->operation_count : 0;
}

size_t prototype_operation_graph_case_count(const struct prototype_operation_graph* graph) {
	return graph ? graph->case_count : 0;
}

const struct prototype_operation_node* prototype_operation_graph_get(
	const struct prototype_operation_graph* graph,
	uint32_t operation_id
) {
	return graph && operation_id < graph->operation_count ?
		&graph->operations[operation_id] : NULL;
}

int prototype_operation_graph_selected_classifier(
	const struct prototype_operation_graph* graph,
	uint32_t operation_id,
	uint32_t* p_classifier
) {
	const struct prototype_operation_node* operation =
		prototype_operation_graph_get(graph, operation_id);
	if (!operation || !p_classifier ||
		operation->classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_classifier = operation->classifier;
	return 0;
}

const struct prototype_operation_match_case* prototype_operation_graph_get_case(
	const struct prototype_operation_graph* graph,
	uint32_t case_id
) {
	return graph && case_id < graph->case_count ?
		&graph->cases[case_id] : NULL;
}

const struct prototype_operation_computation_fold_clause*
prototype_operation_graph_get_fold_clause(
	const struct prototype_operation_graph* graph,
	uint32_t clause_id
) {
	return graph && clause_id < graph->fold_clause_count ?
		&graph->fold_clauses[clause_id] : NULL;
}

int prototype_operation_graph_add(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_node operation,
	uint32_t* p_operation_id
) {
	if (!graph || !contexts || !graph->operations ||
		!prototype_context_get(contexts, operation.context_id) ||
		reserve_slot(graph->operation_count, graph->operation_capacity) != 0) {
		return -1;
	}
	if (p_operation_id) {
		*p_operation_id = (uint32_t)graph->operation_count;
	}
	graph->operations[graph->operation_count++] = operation;
	return 0;
}

int prototype_operation_graph_add_case(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_match_case operation_case,
	uint32_t* p_case_id
) {
	if (!graph || !contexts || !graph->cases ||
		!prototype_context_get(contexts, operation_case.context_id) ||
		operation_case.binder_count > 16 ||
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
	if (!operation_case.has_refinement) {
		operation_case.refinement_substitution = PROTOTYPE_INVALID_ID;
	}
	if (p_case_id) {
		*p_case_id = (uint32_t)graph->case_count;
	}
	graph->cases[graph->case_count++] = operation_case;
	return 0;
}

int prototype_operation_graph_add_fold_clause(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_computation_fold_clause clause,
	uint32_t* p_clause_id
) {
	if (!graph || !contexts || !graph->fold_clauses ||
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

static int operation_graph_reaches_at_depth(
	const struct prototype_operation_graph* graph,
	uint32_t root_operation,
	uint32_t target_operation,
	unsigned char* visited
) {
	if (!graph || !visited || root_operation >= graph->operation_count ||
		target_operation >= graph->operation_count) {
		return -1;
	}
	if (root_operation == target_operation) {
		return 1;
	}
	if (visited[root_operation]) {
		return 0;
	}
	visited[root_operation] = 1;
	const struct prototype_operation_node* operation =
		&graph->operations[root_operation];
	uint32_t children[5] = {
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID
	};
	size_t child_count = 0;
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_NAME:
			children[child_count++] = operation->function;
			break;
		case PROTOTYPE_OPERATION_ASCRIPTION:
		case PROTOTYPE_OPERATION_LAMBDA:
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_APP:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			children[child_count++] = operation->argument;
			break;
		case PROTOTYPE_OPERATION_REQUEST:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			children[child_count++] = operation->body;
			break;
		case PROTOTYPE_OPERATION_MATCH:
			children[child_count++] = operation->scrutinee;
			break;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			children[child_count++] = operation->function;
			children[child_count++] = operation->argument;
			children[child_count++] = operation->scrutinee;
			children[child_count++] = operation->fold_return_operation;
			break;
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_VAR:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
			break;
		default:
			return -1;
	}
	for (size_t i = 0; i < child_count; ++i) {
		if (children[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int reaches = operation_graph_reaches_at_depth(
			graph, children[i], target_operation, visited
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_MATCH) {
		if (operation->case_count != 0 &&
			(operation->first_case == PROTOTYPE_INVALID_ID ||
			 operation->first_case > graph->case_count ||
			 operation->case_count > graph->case_count - operation->first_case)) {
			return -1;
		}
		for (uint32_t i = 0; i < operation->case_count; ++i) {
			int reaches = operation_graph_reaches_at_depth(
				graph,
				graph->cases[operation->first_case + i].body_operation,
				target_operation,
				visited
			);
			if (reaches != 0) {
				return reaches;
			}
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD) {
		if (operation->fold_clause_count != 0 &&
			(operation->first_fold_clause == PROTOTYPE_INVALID_ID ||
			 operation->first_fold_clause > graph->fold_clause_count ||
			operation->fold_clause_count >
				graph->fold_clause_count - operation->first_fold_clause)) {
			return -1;
		}
		for (uint32_t i = 0; i < operation->fold_clause_count; ++i) {
			const struct prototype_operation_computation_fold_clause* clause =
				&graph->fold_clauses[operation->first_fold_clause + i];
			uint32_t clause_children[] = {
				clause->operation_operation,
				clause->body_operation,
				clause->clause_operation
			};
			for (size_t j = 0;
				j < sizeof(clause_children) / sizeof(clause_children[0]);
				++j) {
				int reaches = operation_graph_reaches_at_depth(
					graph, clause_children[j], target_operation, visited
				);
				if (reaches != 0) {
					return reaches;
				}
			}
		}
	}
	return 0;
}

int prototype_operation_graph_reaches(
	const struct prototype_operation_graph* graph,
	uint32_t root_operation,
	uint32_t target_operation
) {
	if (!graph || root_operation >= graph->operation_count ||
		target_operation >= graph->operation_count) {
		return -1;
	}
	unsigned char visited[graph->operation_count];
	memset(visited, 0, sizeof(visited));
	return operation_graph_reaches_at_depth(
		graph, root_operation, target_operation, visited
	);
}
