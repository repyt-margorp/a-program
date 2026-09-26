#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_CONTEXT_PROJECTION_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_CONTEXT_PROJECTION_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/support/schema.h"

#define PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT 4093

/*
 * These records belong exclusively to Layer T. They describe where a typed
 * source occurrence is checked. They are not executable syntax and must never
 * participate in Core Term interning or reduction.
 */
enum prototype_context_action_expression_kind {
	PROTOTYPE_CONTEXT_ACTION_IDENTITY = 1,
	PROTOTYPE_CONTEXT_ACTION_PROJECTION = 2,
	PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT = 3,
	PROTOTYPE_CONTEXT_ACTION_RESTRICTION = 4
};

/*
 * The fields form one tagged immutable key.
 *
 * IDENTITY:
 *   target_candidate_context is the identity object.
 * PROJECTION:
 *   parent_action is the action for the lexical parent Context.
 * BRANCH_REFINEMENT:
 *   parent_action is the inherited action and dependency_id names the
 *   immutable typed Match-case projection whose refinement is required.
 */
struct prototype_context_action_expression {
	int kind;
	uint32_t target_candidate_context;
	uint32_t parent_action;
	uint32_t dependency_id;
	uint64_t key_hash;
	uint32_t hash_next;
};

struct prototype_context_projection {
	uint32_t candidate_context;
	uint32_t action_expression;
	uint64_t key_hash;
	uint32_t hash_next;
};

enum prototype_context_projection_solution_state {
	PROTOTYPE_CONTEXT_PROJECTION_PENDING = 0,
	PROTOTYPE_CONTEXT_PROJECTION_SOLVED = 1,
	PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION = 2
};

struct prototype_context_projection_solution {
	int state;
	/* Derived memo of evaluating the immutable projection equation against the
	 * current branch-constraint and ContextDB authorities. It may be replaced
	 * when either authority advances; it is never an independent typing fact. */
	uint32_t concrete_context;
	uint32_t substitution;
	/* Digest of the exact parent projection and branch-refinement answers used
	 * to derive this memo. It controls reuse only and is not semantic Authority. */
	uint64_t input_fingerprint;
	/* Exact, per-projection semantic revision. This is the Authority used by
	 * equation answer snapshots. The aggregate DB revision is diagnostic only. */
	uint64_t revision;
};

/* A typing node may have several Context-indexed Layer T projections. */
struct prototype_typed_projection {
	/* Semantic source identity for both term and type-expression judgements. */
	uint32_t source_typing_node;
	/* Optional term-only provenance. It is not part of the semantic key. */
	uint32_t source_occurrence;
	uint32_t context_projection;
	uint64_t key_hash;
	uint32_t hash_next;
	/* Derived Layer T adjacency. It is not part of the semantic key. */
	uint32_t typing_node_next;
	uint32_t context_projection_next;
	int topology_expanded;
};

struct prototype_typed_match_case_projection {
	uint32_t source_case;
	uint32_t owner_typed_projection;
	uint32_t context_projection;
	uint64_t key_hash;
	uint32_t hash_next;
};

enum prototype_typed_projection_edge_kind {
	PROTOTYPE_TYPED_PROJECTION_EDGE_WRAPPED = 1,
	PROTOTYPE_TYPED_PROJECTION_EDGE_CHILD = 2,
	PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE = 3,
	PROTOTYPE_TYPED_PROJECTION_EDGE_SEQUENCE_PRODUCER = 4,
	PROTOTYPE_TYPED_PROJECTION_EDGE_INDUCTION_OWNER = 5
};

/*
 * A projected edge mirrors exactly one immutable Layer T source relation.
 * source_relation is a source occurrence ID for WRAPPED, a source edge ID for
 * CHILD, a source Match-case ID for MATCH_CASE, and a source occurrence ID
 * for SEQUENCE_PRODUCER and INDUCTION_OWNER.
 */
struct prototype_typed_projection_edge {
	int kind;
	uint32_t parent_typed_projection;
	uint32_t source_relation;
	uint32_t child_typed_projection;
	uint64_t key_hash;
	uint32_t hash_next;
	uint32_t parent_next;
};

struct prototype_context_projection_db {
	struct prototype_context_action_expression* actions;
	size_t action_count;
	size_t action_capacity;
	uint32_t action_index_heads[
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT
	];
	struct prototype_context_projection* projections;
	struct prototype_context_projection_solution* projection_solutions;
	size_t projection_count;
	size_t projection_capacity;
	uint32_t projection_index_heads[
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT
	];
	struct prototype_typed_projection* typed_projections;
	size_t typed_projection_count;
	size_t typed_projection_capacity;
	uint32_t* typed_projection_typing_node_heads;
	size_t typed_projection_typing_node_capacity;
	uint32_t* typed_projection_context_heads;
	size_t typed_projection_context_capacity;
	uint32_t typed_projection_index_heads[
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT
	];
	struct prototype_typed_match_case_projection* match_case_projections;
	size_t match_case_projection_count;
	size_t match_case_projection_capacity;
	uint32_t match_case_projection_index_heads[
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT
	];
	struct prototype_typed_projection_edge* typed_projection_edges;
	size_t typed_projection_edge_count;
	size_t typed_projection_edge_capacity;
	uint32_t* typed_projection_edge_parent_heads;
	size_t typed_projection_edge_parent_capacity;
	uint32_t typed_projection_edge_index_heads[
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT
	];
	uint64_t action_intern_requests;
	uint64_t action_intern_hits;
	uint64_t projection_intern_requests;
	uint64_t projection_intern_hits;
	uint64_t typed_projection_intern_requests;
	uint64_t typed_projection_intern_hits;
	uint64_t match_case_projection_intern_requests;
	uint64_t match_case_projection_intern_hits;
	uint64_t typed_projection_edge_intern_requests;
	uint64_t typed_projection_edge_intern_hits;
	uint64_t solution_revision;
};

void prototype_context_projection_db_init(
	struct prototype_context_projection_db* db,
	struct prototype_context_action_expression* actions,
	size_t action_capacity,
	struct prototype_context_projection* projections,
	struct prototype_context_projection_solution* projection_solutions,
	size_t projection_capacity,
	struct prototype_typed_projection* typed_projections,
	size_t typed_projection_capacity,
	uint32_t* typed_projection_typing_node_heads,
	size_t typed_projection_typing_node_capacity,
	uint32_t* typed_projection_context_heads,
	size_t typed_projection_context_capacity,
	struct prototype_typed_match_case_projection* match_case_projections,
	size_t match_case_projection_capacity,
	struct prototype_typed_projection_edge* typed_projection_edges,
	size_t typed_projection_edge_capacity,
	uint32_t* typed_projection_edge_parent_heads,
	size_t typed_projection_edge_parent_capacity
);

const struct prototype_context_action_expression*
prototype_context_action_expression_get(
	const struct prototype_context_projection_db* db,
	uint32_t action_id
);

int prototype_context_action_identity(
	struct prototype_context_projection_db* db,
	uint32_t candidate_context,
	uint32_t* p_action_id
);

int prototype_context_action_projection(
	struct prototype_context_projection_db* db,
	uint32_t parent_action,
	uint32_t extension_candidate_context,
	uint32_t* p_action_id
);
int prototype_context_action_restriction(
	struct prototype_context_projection_db* db,
	uint32_t parent_action,
	uint32_t ancestor_candidate_context,
	uint32_t* p_action_id
);

int prototype_context_action_branch_refinement(
	struct prototype_context_projection_db* db,
	uint32_t parent_action,
	uint32_t branch_candidate_context,
	uint32_t match_case_projection_id,
	uint32_t* p_action_id
);

const struct prototype_context_projection* prototype_context_projection_get(
	const struct prototype_context_projection_db* db,
	uint32_t projection_id
);

const struct prototype_context_projection_solution*
prototype_context_projection_solution_get(
	const struct prototype_context_projection_db* db,
	uint32_t projection_id
);

int prototype_context_projection_find(
	const struct prototype_context_projection_db* db,
	uint32_t candidate_context,
	uint32_t action_expression,
	uint32_t* p_projection_id
);

int prototype_context_projection_intern(
	struct prototype_context_projection_db* db,
	uint32_t candidate_context,
	uint32_t action_expression,
	uint32_t* p_projection_id
);

int prototype_context_projection_publish(
	struct prototype_context_projection_db* db,
	uint32_t projection_id,
	uint32_t concrete_context,
	uint32_t substitution,
	uint64_t input_fingerprint
);

int prototype_context_projection_reject(
	struct prototype_context_projection_db* db,
	uint32_t projection_id
);

const struct prototype_typed_projection* prototype_typed_projection_get(
	const struct prototype_context_projection_db* db,
	uint32_t typed_projection_id
);

int prototype_typed_projection_intern(
	struct prototype_context_projection_db* db,
	uint32_t source_typing_node,
	uint32_t source_occurrence,
	uint32_t context_projection,
	uint32_t* p_typed_projection_id
);

uint32_t prototype_typed_projection_first_for_typing_node(
	const struct prototype_context_projection_db* db,
	uint32_t source_typing_node
);

uint32_t prototype_typed_projection_next_for_typing_node(
	const struct prototype_context_projection_db* db,
	uint32_t typed_projection_id
);

uint32_t prototype_typed_projection_first_for_context_projection(
	const struct prototype_context_projection_db* db,
	uint32_t context_projection_id
);

uint32_t prototype_typed_projection_next_for_context_projection(
	const struct prototype_context_projection_db* db,
	uint32_t typed_projection_id
);

const struct prototype_typed_match_case_projection*
prototype_typed_match_case_projection_get(
	const struct prototype_context_projection_db* db,
	uint32_t match_case_projection_id
);

int prototype_typed_match_case_projection_intern(
	struct prototype_context_projection_db* db,
	uint32_t source_case,
	uint32_t owner_typed_projection,
	uint32_t context_projection,
	uint32_t* p_match_case_projection_id
);

int prototype_typed_match_case_projection_find(
	const struct prototype_context_projection_db* db,
	uint32_t source_case,
	uint32_t owner_typed_projection,
	uint32_t* p_match_case_projection_id
);

const struct prototype_typed_projection_edge* prototype_typed_projection_edge_get(
	const struct prototype_context_projection_db* db,
	uint32_t edge_id
);

uint32_t prototype_typed_projection_first_edge(
	const struct prototype_context_projection_db* db,
	uint32_t parent_typed_projection
);

uint32_t prototype_typed_projection_next_edge(
	const struct prototype_context_projection_db* db,
	uint32_t edge_id
);

int prototype_typed_projection_edge_intern(
	struct prototype_context_projection_db* db,
	int kind,
	uint32_t parent_typed_projection,
	uint32_t source_relation,
	uint32_t child_typed_projection,
	uint32_t* p_edge_id
);

int prototype_typed_projection_edge_find(
	const struct prototype_context_projection_db* db,
	int kind,
	uint32_t parent_typed_projection,
	uint32_t source_relation,
	uint32_t* p_child_typed_projection
);

int prototype_context_projection_db_validate(
	const struct prototype_context_projection_db* db,
	size_t candidate_context_count,
	size_t typing_node_count,
	size_t occurrence_count,
	size_t occurrence_edge_count,
	size_t match_case_count,
	size_t concrete_context_count,
	size_t substitution_count
);

/* Rebuild every derived hash/adjacency index after retaining an immutable
 * prefix of the projection topology. Counts are Authority; all links and
 * solution memos are reconstructed from that prefix. */
int prototype_context_projection_db_rebuild_indexes(
	struct prototype_context_projection_db* db
);

#endif
