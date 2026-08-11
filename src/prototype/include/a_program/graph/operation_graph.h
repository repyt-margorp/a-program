#ifndef A_PROGRAM_PROTOTYPE_GRAPH_OPERATION_GRAPH_H
#define A_PROGRAM_PROTOTYPE_GRAPH_OPERATION_GRAPH_H

#include "a_program/graph/operation_model.h"

struct prototype_compile_metadata;
struct prototype_context_db;

int prototype_operation_graph_reaches(
	const struct prototype_operation_graph* graph,
	uint32_t root_operation,
	uint32_t target_operation
);

void prototype_operation_graph_init(
	struct prototype_operation_graph* graph,
	struct prototype_operation_node* operations,
	size_t operation_capacity,
	struct prototype_operation_match_case* cases,
	size_t case_capacity,
	struct prototype_operation_computation_fold_clause* fold_clauses,
	size_t fold_clause_capacity
);
size_t prototype_operation_graph_count(const struct prototype_operation_graph* graph);
size_t prototype_operation_graph_case_count(const struct prototype_operation_graph* graph);
const struct prototype_operation_node* prototype_operation_graph_get(
	const struct prototype_operation_graph* graph,
	uint32_t operation_id
);
int prototype_operation_graph_selected_classifier(
	const struct prototype_operation_graph* graph,
	uint32_t operation_id,
	uint32_t* p_classifier
);
const struct prototype_operation_match_case* prototype_operation_graph_get_case(
	const struct prototype_operation_graph* graph,
	uint32_t case_id
);
const struct prototype_operation_computation_fold_clause*
prototype_operation_graph_get_fold_clause(
	const struct prototype_operation_graph* graph,
	uint32_t clause_id
);
int prototype_operation_graph_induction_edge(
	const struct prototype_operation_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t operation_id,
	struct prototype_operation_induction_edge* p_edge
);
int prototype_operation_graph_add(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_node operation,
	uint32_t* p_operation_id
);
int prototype_operation_graph_add_case(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_match_case operation_case,
	uint32_t* p_case_id
);
int prototype_operation_graph_add_fold_clause(
	struct prototype_operation_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_operation_computation_fold_clause clause,
	uint32_t* p_clause_id
);
int prototype_operation_graph_validate(
	const struct prototype_operation_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
);
void prototype_compile_metadata_operation_graph(
	struct prototype_compile_metadata* metadata,
	struct prototype_operation_graph* graph
);
void prototype_compile_metadata_operation_graph_const(
	const struct prototype_compile_metadata* metadata,
	struct prototype_operation_graph* graph
);
void prototype_compile_metadata_commit_operation_graph(
	struct prototype_compile_metadata* metadata,
	const struct prototype_operation_graph* graph
);
#endif
