#ifndef A_PROGRAM_PROTOTYPE_GRAPH_OCCURRENCE_USAGE_H
#define A_PROGRAM_PROTOTYPE_GRAPH_OCCURRENCE_USAGE_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/frontend/typing_usage_state.h"
#include "a_program/graph/typed_occurrence_model.h"

struct prototype_context_db;
struct prototype_term_db;

int prototype_occurrence_usage_solve(
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	struct prototype_occurrence_usage_solution* solutions,
	size_t solution_capacity,
	struct prototype_usage_entry* entries,
	size_t entry_capacity,
	size_t* p_entry_count
);

int prototype_occurrence_usage_solution_view(
	const struct prototype_occurrence_usage_solution* solutions,
	size_t solution_count,
	const struct prototype_usage_entry* entries,
	size_t entry_count,
	uint32_t occurrence_id,
	struct prototype_usage_vector* p_usage
);

#endif
