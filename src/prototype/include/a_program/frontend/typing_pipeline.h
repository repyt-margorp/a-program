#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_PIPELINE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/frontend/context_projection.h"
#include "a_program/frontend/typing_binding_state.h"
#include "a_program/frontend/typing_constraint_state.h"
#include "a_program/frontend/typing_reindex_state.h"
#include "a_program/frontend/typing_seed_state.h"

#define PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY 4096
#define PROTOTYPE_TYPING_PIPELINE_TYPING_NODE_CAPACITY \
	(PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY * 2)
#define PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY 16384
#define PROTOTYPE_TYPING_PIPELINE_PROJECTION_EDGE_CAPACITY 32768

/*
 * Layer T owns typing facts about opaque Layer C terms. Source elaboration and
 * artifact publication are clients of this state, not additional authorities.
 */
struct prototype_typing_pipeline {
	struct prototype_typing_binding_state bindings;
	struct prototype_typing_constraint_db constraints;
	struct prototype_typing_reindex_state reindex;
	struct prototype_typing_seed_state seed;
	struct prototype_context_projection_db context_projections;
	struct prototype_context_action_expression context_projection_actions[
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY
	];
	struct prototype_context_projection context_projection_nodes[
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY
	];
	struct prototype_context_projection_solution context_projection_solutions[
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY
	];
	struct prototype_typed_projection typed_projection_nodes[
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY
	];
	uint32_t typed_projection_typing_node_heads[
		PROTOTYPE_TYPING_PIPELINE_TYPING_NODE_CAPACITY
	];
	uint32_t typed_projection_context_heads[
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY
	];
	struct prototype_typed_match_case_projection match_case_projection_nodes[
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY
	];
	struct prototype_typed_projection_edge typed_projection_edges[
		PROTOTYPE_TYPING_PIPELINE_PROJECTION_EDGE_CAPACITY
	];
	uint32_t typed_projection_edge_parent_heads[
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY
	];
	uint32_t base_typed_projection_for_occurrence[
		PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY
	];
	uint32_t publication_typed_projection_for_occurrence[
		PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY
	];
	uint64_t topology_revision;
	uint64_t transaction_revision;
	size_t transaction_action_count;
	size_t transaction_context_projection_count;
	size_t transaction_typed_projection_count;
	size_t transaction_match_case_projection_count;
	size_t transaction_projection_edge_count;
	uint64_t transaction_topology_revision;
	int transaction_active;
	int initialized;
};

int prototype_typing_pipeline_init(
	struct prototype_typing_pipeline* pipeline
);

void prototype_typing_pipeline_dispose(
	struct prototype_typing_pipeline* pipeline
);

/* Begin a new compilation image while retaining the accepted immutable Layer T
 * projection topology. Mutable answers, constraints, queues, and caches are
 * discarded; they are reconstructed by solve(image, effort). */
int prototype_typing_pipeline_reset_image_progress(
	struct prototype_typing_pipeline* pipeline
);

int prototype_typing_pipeline_begin_transaction(
	struct prototype_typing_pipeline* pipeline
);

int prototype_typing_pipeline_commit_transaction(
	struct prototype_typing_pipeline* pipeline
);

int prototype_typing_pipeline_rollback_transaction(
	struct prototype_typing_pipeline* pipeline
);

int prototype_typing_pipeline_seal_constraint_topology(
	struct prototype_typing_pipeline* pipeline,
	uint64_t* topology_digest
);

int prototype_typing_pipeline_validate_constraint_topology(
	const struct prototype_typing_pipeline* pipeline,
	uint64_t* expected_digest,
	uint64_t* actual_digest
);

int prototype_typing_pipeline_finish_graph_build(
	struct prototype_typing_pipeline* pipeline
);

#endif
