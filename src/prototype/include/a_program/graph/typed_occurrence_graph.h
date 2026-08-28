#ifndef A_PROGRAM_PROTOTYPE_GRAPH_TYPED_OCCURRENCE_GRAPH_H
#define A_PROGRAM_PROTOTYPE_GRAPH_TYPED_OCCURRENCE_GRAPH_H

#include "a_program/graph/typed_occurrence_model.h"

struct prototype_compile_metadata;
struct prototype_context_db;

int prototype_typed_occurrence_graph_reaches(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t root_occurrence,
	uint32_t target_occurrence
);

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
);
size_t prototype_typed_occurrence_graph_count(const struct prototype_typed_occurrence_graph* graph);
size_t prototype_typed_occurrence_graph_case_count(const struct prototype_typed_occurrence_graph* graph);
const struct prototype_typed_occurrence* prototype_typed_occurrence_graph_get(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t occurrence_id
);
const struct prototype_typed_occurrence_edge*
prototype_typed_occurrence_graph_get_edge(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t edge_id
);
int prototype_typed_occurrence_graph_child(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t parent_occurrence,
	int role,
	uint32_t ordinal,
	uint32_t* p_child_occurrence
);
int prototype_typed_occurrence_graph_add_edge(
	struct prototype_typed_occurrence_graph* graph,
	uint32_t parent_occurrence,
	struct prototype_typed_occurrence_edge edge
);
const struct prototype_typed_occurrence_match_case* prototype_typed_occurrence_graph_get_case(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t case_id
);
const struct prototype_typed_occurrence_fold_clause*
prototype_typed_occurrence_graph_get_fold_clause(
	const struct prototype_typed_occurrence_graph* graph,
	uint32_t clause_id
);
int prototype_typed_occurrence_core_child_count(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t occurrence_id,
	uint32_t* p_count
);
int prototype_typed_occurrence_core_child(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t occurrence_id,
	uint32_t child_index,
	uint32_t* p_child_occurrence
);
int prototype_typed_occurrence_graph_induction_edge(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t occurrence_id,
	struct prototype_typed_occurrence_induction_edge* p_edge
);
int prototype_typed_occurrence_graph_add(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_typed_occurrence occurrence,
	uint32_t* p_occurrence_id
);
int prototype_typed_occurrence_graph_add_case(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_typed_occurrence_match_case occurrence_case,
	uint32_t* p_case_id
);
int prototype_typed_occurrence_graph_add_fold_clause(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_context_db* contexts,
	struct prototype_typed_occurrence_fold_clause clause,
	uint32_t* p_clause_id
);
int prototype_typed_occurrence_graph_freeze(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
);
int prototype_typed_occurrence_graph_seal(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
);
int prototype_typed_occurrence_graph_begin_transaction(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
);
int prototype_typed_occurrence_graph_rollback_transaction(
	struct prototype_typed_occurrence_graph* graph
);
size_t prototype_typed_occurrence_graph_transaction_start(
	const struct prototype_typed_occurrence_graph* graph
);
int prototype_typed_occurrence_graph_validate(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
);
struct prototype_typed_occurrence_graph* prototype_compile_metadata_typed_occurrences(
	struct prototype_compile_metadata* metadata
);
const struct prototype_typed_occurrence_graph*
prototype_compile_metadata_typed_occurrences_const(
	const struct prototype_compile_metadata* metadata
);
#endif
