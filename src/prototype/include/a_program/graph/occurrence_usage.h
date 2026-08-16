#ifndef A_PROGRAM_PROTOTYPE_GRAPH_OCCURRENCE_USAGE_H
#define A_PROGRAM_PROTOTYPE_GRAPH_OCCURRENCE_USAGE_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/graph/typed_occurrence_model.h"
#include "a_program/kernel/resource_usage.h"

#define PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY 4096
#define PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY 131072

struct prototype_context_db;

struct prototype_occurrence_usage_solution {
	uint32_t first_entry;
	uint32_t entry_count;
	int binder_usage;
};

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
