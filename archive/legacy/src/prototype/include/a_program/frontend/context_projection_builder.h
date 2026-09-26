#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_CONTEXT_PROJECTION_BUILDER_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_CONTEXT_PROJECTION_BUILDER_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/frontend/context_projection.h"

struct prototype_typed_occurrence_graph;
struct prototype_context_db;
struct prototype_substitution_db;
struct prototype_typing_source_results;

/*
 * Transitional producer adapter.  It may read the legacy occurrence graph,
 * but it can only append immutable Layer T projection topology.  T1 semantic
 * rules must depend on context_projection.h, never on this capability.
 */
struct prototype_typing_graph_build_context {
	const struct prototype_typing_source_results* source_results;
	const struct prototype_typed_occurrence_graph* occurrences;
	const struct prototype_context_db* contexts;
	const struct prototype_substitution_db* substitutions;
	struct prototype_context_projection_db* projections;
	uint32_t transaction_occurrence_start;
	uint32_t selected_entry_occurrence;
	uint32_t* base_projection_for_occurrence;
	size_t base_projection_capacity;
	uint32_t* publication_projection_for_occurrence;
	size_t publication_projection_capacity;
};

int prototype_context_projection_build_rooted_topology(
	struct prototype_context_projection_db* db,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	const uint32_t* root_occurrences,
	size_t root_count
);

int prototype_typing_graph_build_rooted_projections(
	const struct prototype_typing_graph_build_context* context
);

#endif
