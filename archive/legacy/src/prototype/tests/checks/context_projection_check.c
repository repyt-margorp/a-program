#include "a_program/frontend/context_projection.h"
#include "a_program/frontend/context_projection_builder.h"
#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/core/graph.h"
#include "a_program/kernel/context.h"

#include <stdio.h>

#define ACTION_CAPACITY 32
#define PROJECTION_CAPACITY 32
#define TYPED_PROJECTION_CAPACITY 32
#define SOURCE_OCCURRENCE_CAPACITY 16
#define TYPED_PROJECTION_EDGE_CAPACITY 64

static struct prototype_typed_occurrence make_occurrence(
	int tag,
	uint32_t context,
	uint32_t core_term
) {
	return (struct prototype_typed_occurrence) {
		.tag = tag,
		.context_id = context,
		.context_action_substitution = PROTOTYPE_INVALID_ID,
		.source_core_term = core_term,
		.source_classifier = PROTOTYPE_INVALID_ID,
		.core_term = core_term,
		.classifier = PROTOTYPE_INVALID_ID,
		.classifier_status = PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_PENDING,
		.classifier_verification_obligation = PROTOTYPE_INVALID_ID,
		.source_ast = PROTOTYPE_INVALID_ID,
		.source_symbol_id = -1,
		.binder_symbol_id = -1,
		.referenced_ast_binder_id = PROTOTYPE_INVALID_ID,
		.binding_id = PROTOTYPE_INVALID_ID,
		.first_edge = PROTOTYPE_INVALID_ID,
		.wrapped_occurrence = PROTOTYPE_INVALID_ID,
		.binder_classifier = PROTOTYPE_INVALID_ID,
		.match_motive = PROTOTYPE_INVALID_ID,
		.ih_owner_occurrence = PROTOTYPE_INVALID_ID,
		.first_case = PROTOTYPE_INVALID_ID,
		.first_fold_clause = PROTOTYPE_INVALID_ID
	};
}

static int check_rooted_topology(void) {
	struct prototype_context contexts[8];
	struct prototype_context_db context_db;
	prototype_context_db_init(&context_db, contexts, 8);
	uint32_t case_context;
	if (prototype_context_extend(
			&context_db,
			prototype_context_empty(&context_db),
			100,
			200,
			&case_context
		) != 0) {
		return -1;
	}
	struct prototype_typed_occurrence occurrences[8];
	struct prototype_typed_occurrence_edge occurrence_edges[8];
	struct prototype_typed_occurrence_match_case occurrence_cases[2];
	struct prototype_typed_occurrence_fold_clause fold_clauses[1];
	struct prototype_typed_occurrence_graph graph;
	prototype_typed_occurrence_graph_init(
		&graph,
		occurrences,
		8,
		occurrence_edges,
		8,
		occurrence_cases,
		2,
		fold_clauses,
		1
	);

	struct prototype_typed_occurrence_match_case match_case = {
		.context_id = case_context,
		.declared_constructor_owner = 300,
		.declared_constructor_id = 0,
		.refinement_status =
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_PENDING,
		.refinement_substitution = PROTOTYPE_INVALID_ID,
		.constructor_owner = 300,
		.constructor_id = 0,
		.case_label_symbol_id = -1,
		.binder_count = 1,
		.binder_ids = {100},
		.ast_binder_ids = {400}
	};
	uint32_t first_case;
	uint32_t second_case;
	if (prototype_typed_occurrence_graph_add_case(
			&graph, &context_db, match_case, &first_case
		) != 0 || prototype_typed_occurrence_graph_add_case(
			&graph, &context_db, match_case, &second_case
		) != 0) {
		return -1;
	}

	uint32_t scrutinee;
	uint32_t shared_body;
	uint32_t unreachable;
	uint32_t first_match;
	uint32_t second_match;
	struct prototype_typed_occurrence occurrence = make_occurrence(
		PROTOTYPE_TYPED_OCCURRENCE_ATOM, 0, 10
	);
	if (prototype_typed_occurrence_graph_add(
			&graph, &context_db, occurrence, &scrutinee
		) != 0) {
		return -1;
	}
	occurrence = make_occurrence(
		PROTOTYPE_TYPED_OCCURRENCE_RETURN, case_context, 11
	);
	if (prototype_typed_occurrence_graph_add(
			&graph, &context_db, occurrence, &shared_body
		) != 0) {
		return -1;
	}
	occurrence = make_occurrence(PROTOTYPE_TYPED_OCCURRENCE_ATOM, 0, 12);
	if (prototype_typed_occurrence_graph_add(
			&graph, &context_db, occurrence, &unreachable
		) != 0) {
		return -1;
	}
	occurrence = make_occurrence(PROTOTYPE_TYPED_OCCURRENCE_MATCH, 0, 20);
	occurrence.first_case = first_case;
	occurrence.case_count = 1;
	if (prototype_typed_occurrence_graph_add(
			&graph, &context_db, occurrence, &first_match
		) != 0) {
		return -1;
	}
	occurrence = make_occurrence(PROTOTYPE_TYPED_OCCURRENCE_MATCH, 0, 21);
	occurrence.first_case = second_case;
	occurrence.case_count = 1;
	if (prototype_typed_occurrence_graph_add(
			&graph, &context_db, occurrence, &second_match
		) != 0) {
		return -1;
	}

	if (prototype_typed_occurrence_graph_add_edge(
			&graph,
			shared_body,
			(struct prototype_typed_occurrence_edge) {
				.role = PROTOTYPE_TERM_CHILD_RETURN_VALUE,
				.ordinal = 0,
				.child_occurrence = scrutinee
			}
		) != 0) {
		return -1;
	}
	uint32_t first_scrutinee_edge = (uint32_t)graph.edge_count;
	if (prototype_typed_occurrence_graph_add_edge(
			&graph,
			first_match,
			(struct prototype_typed_occurrence_edge) {
				.role = PROTOTYPE_TERM_CHILD_SCRUTINEE,
				.ordinal = 0,
				.child_occurrence = scrutinee
			}
		) != 0 || prototype_typed_occurrence_graph_add_edge(
			&graph,
			first_match,
			(struct prototype_typed_occurrence_edge) {
				.role = PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
				.ordinal = 0,
				.child_occurrence = shared_body
			}
		) != 0 || prototype_typed_occurrence_graph_add_edge(
			&graph,
			second_match,
			(struct prototype_typed_occurrence_edge) {
				.role = PROTOTYPE_TERM_CHILD_SCRUTINEE,
				.ordinal = 0,
				.child_occurrence = scrutinee
			}
		) != 0 || prototype_typed_occurrence_graph_add_edge(
			&graph,
			second_match,
			(struct prototype_typed_occurrence_edge) {
				.role = PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
				.ordinal = 0,
				.child_occurrence = shared_body
			}
		) != 0) {
		return -1;
	}

	struct prototype_context_action_expression actions[ACTION_CAPACITY];
	struct prototype_context_projection projections[PROJECTION_CAPACITY];
	struct prototype_context_projection_solution solutions[PROJECTION_CAPACITY];
	struct prototype_typed_projection typed_projections[
		TYPED_PROJECTION_CAPACITY
	];
	uint32_t source_heads[SOURCE_OCCURRENCE_CAPACITY];
	uint32_t context_projection_heads[PROJECTION_CAPACITY];
	struct prototype_typed_match_case_projection case_projections[
		TYPED_PROJECTION_CAPACITY
	];
	struct prototype_typed_projection_edge projection_edges[
		TYPED_PROJECTION_EDGE_CAPACITY
	];
	uint32_t projection_edge_parent_heads[TYPED_PROJECTION_CAPACITY];
	struct prototype_context_projection_db db;
	prototype_context_projection_db_init(
		&db,
		actions,
		ACTION_CAPACITY,
		projections,
		solutions,
		PROJECTION_CAPACITY,
		typed_projections,
		TYPED_PROJECTION_CAPACITY,
		source_heads,
		SOURCE_OCCURRENCE_CAPACITY,
		context_projection_heads,
		PROJECTION_CAPACITY,
		case_projections,
		TYPED_PROJECTION_CAPACITY,
		projection_edges,
		TYPED_PROJECTION_EDGE_CAPACITY,
		projection_edge_parent_heads,
		TYPED_PROJECTION_CAPACITY
	);
	uint32_t roots[] = {first_match, second_match};
	if (prototype_context_projection_build_rooted_topology(
			&db, &graph, &context_db, roots, 2
		) != 0) {
		return -1;
	}

	uint32_t first_match_projection =
		prototype_typed_projection_first_for_typing_node(&db, first_match);
	uint32_t second_match_projection =
		prototype_typed_projection_first_for_typing_node(&db, second_match);
	uint32_t body_projection_0 =
		prototype_typed_projection_first_for_typing_node(&db, shared_body);
	uint32_t body_projection_1 = prototype_typed_projection_next_for_typing_node(
		&db, body_projection_0
	);
	uint32_t projected_child;
	if (first_match_projection == PROTOTYPE_INVALID_ID ||
		second_match_projection == PROTOTYPE_INVALID_ID ||
		body_projection_0 == PROTOTYPE_INVALID_ID ||
		body_projection_1 == PROTOTYPE_INVALID_ID ||
		body_projection_0 == body_projection_1 ||
		prototype_typed_projection_next_for_typing_node(&db, body_projection_1) !=
			PROTOTYPE_INVALID_ID ||
		prototype_typed_projection_first_for_typing_node(&db, unreachable) !=
			PROTOTYPE_INVALID_ID ||
		prototype_typed_projection_edge_find(
			&db,
			PROTOTYPE_TYPED_PROJECTION_EDGE_CHILD,
			first_match_projection,
			first_scrutinee_edge,
			&projected_child
		) != 0 || db.typed_projections[projected_child].source_occurrence !=
			scrutinee || prototype_typed_projection_edge_find(
			&db,
			PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE,
			first_match_projection,
			first_case,
			&projected_child
		) != 0 || db.typed_projections[projected_child].source_occurrence !=
			shared_body || db.match_case_projection_count != 2) {
		return -1;
	}

	size_t action_count = db.action_count;
	size_t projection_count = db.projection_count;
	size_t typed_projection_count = db.typed_projection_count;
	size_t match_case_projection_count = db.match_case_projection_count;
	size_t projection_edge_count = db.typed_projection_edge_count;
	if (prototype_context_projection_build_rooted_topology(
			&db, &graph, &context_db, roots, 2
		) != 0 || db.action_count != action_count ||
		db.projection_count != projection_count ||
		db.typed_projection_count != typed_projection_count ||
		db.match_case_projection_count != match_case_projection_count ||
		db.typed_projection_edge_count != projection_edge_count ||
		prototype_context_projection_db_validate(
			&db,
			context_db.context_count,
			graph.occurrence_count,
			graph.occurrence_count,
			graph.edge_count,
			graph.case_count,
			context_db.context_count,
			0
		) != 0) {
		return -1;
	}
	return 0;
}

int main(void) {
	struct prototype_context_action_expression actions[ACTION_CAPACITY];
	struct prototype_context_projection projections[PROJECTION_CAPACITY];
	struct prototype_context_projection_solution solutions[PROJECTION_CAPACITY];
	struct prototype_typed_projection typed_projections[
		TYPED_PROJECTION_CAPACITY
	];
	uint32_t typed_projection_typing_node_heads[SOURCE_OCCURRENCE_CAPACITY];
	uint32_t typed_projection_context_heads[PROJECTION_CAPACITY];
	struct prototype_typed_match_case_projection match_case_projections[
		TYPED_PROJECTION_CAPACITY
	];
	struct prototype_typed_projection_edge typed_projection_edges[
		TYPED_PROJECTION_EDGE_CAPACITY
	];
	uint32_t typed_projection_edge_parent_heads[TYPED_PROJECTION_CAPACITY];
	struct prototype_context_projection_db db;
	prototype_context_projection_db_init(
		&db,
		actions,
		ACTION_CAPACITY,
		projections,
		solutions,
		PROJECTION_CAPACITY,
		typed_projections,
		TYPED_PROJECTION_CAPACITY,
		typed_projection_typing_node_heads,
		SOURCE_OCCURRENCE_CAPACITY,
		typed_projection_context_heads,
		PROJECTION_CAPACITY,
		match_case_projections,
		TYPED_PROJECTION_CAPACITY,
		typed_projection_edges,
		TYPED_PROJECTION_EDGE_CAPACITY,
		typed_projection_edge_parent_heads,
		TYPED_PROJECTION_CAPACITY
	);

	uint32_t identity_0;
	uint32_t identity_0_again;
	uint32_t identity_1;
	uint32_t lifted_1;
	if (prototype_context_action_identity(&db, 0, &identity_0) != 0 ||
		prototype_context_action_identity(&db, 0, &identity_0_again) != 0 ||
		identity_0 != identity_0_again ||
		prototype_context_action_identity(&db, 1, &identity_1) != 0 ||
		identity_0 == identity_1 ||
		prototype_context_action_projection(&db, identity_0, 1, &lifted_1) != 0) {
		fprintf(stderr, "context action interning failed\n");
		return 1;
	}
	uint32_t projection_0;
	uint32_t projection_0_again;
	uint32_t owner_projection_1;
	uint32_t owner_projection_lifted_1;
	if (prototype_context_projection_intern(
			&db, 0, identity_0, &projection_0
		) != 0 || prototype_context_projection_intern(
			&db, 0, identity_0_again, &projection_0_again
		) != 0 || projection_0 != projection_0_again ||
		prototype_context_projection_intern(
			&db, 1, identity_1, &owner_projection_1
		) != 0 || prototype_context_projection_intern(
			&db, 1, lifted_1, &owner_projection_lifted_1
		) != 0 || owner_projection_1 == owner_projection_lifted_1) {
		fprintf(stderr, "context projection interning failed\n");
		return 1;
	}

	uint32_t owner_typed_1;
	uint32_t owner_typed_lifted_1;
	if (prototype_typed_projection_intern(
			&db, 7, 7, owner_projection_1, &owner_typed_1
		) != 0 || prototype_typed_projection_intern(
			&db, 7, 7, owner_projection_lifted_1, &owner_typed_lifted_1
		) != 0 || owner_typed_1 == owner_typed_lifted_1) {
		fprintf(stderr, "typed projection interning failed\n");
		return 1;
	}
	uint32_t case_1;
	uint32_t case_1_again;
	uint32_t case_lifted_1;
	if (prototype_typed_match_case_projection_intern(
			&db, 2, owner_typed_1, owner_projection_1, &case_1
		) != 0 || prototype_typed_match_case_projection_intern(
			&db, 2, owner_typed_1, owner_projection_1, &case_1_again
		) != 0 || case_1 != case_1_again ||
		prototype_typed_match_case_projection_intern(
			&db, 2, owner_typed_lifted_1, owner_projection_lifted_1,
			&case_lifted_1
		) != 0 || case_1 == case_lifted_1) {
		fprintf(stderr, "Match-case projection interning failed\n");
		return 1;
	}

	uint32_t branch_action_1;
	uint32_t branch_action_lifted_1;
	if (prototype_context_action_branch_refinement(
			&db, identity_1, 2, case_1, &branch_action_1
		) != 0 || prototype_context_action_branch_refinement(
			&db, lifted_1, 2, case_lifted_1, &branch_action_lifted_1
		) != 0 || branch_action_1 == branch_action_lifted_1) {
		fprintf(stderr, "branch action interning failed\n");
		return 1;
	}

	uint32_t branch_projection_1;
	uint32_t branch_projection_lifted_1;
	if (prototype_context_projection_intern(
			&db, 2, branch_action_1, &branch_projection_1
		) != 0 || prototype_context_projection_intern(
			&db, 2, branch_action_lifted_1, &branch_projection_lifted_1
		) != 0 || branch_projection_1 == branch_projection_lifted_1) {
		fprintf(stderr, "branch projection interning failed\n");
		return 1;
	}

	uint32_t typed_1;
	uint32_t typed_1_again;
	uint32_t typed_lifted_1;
	if (prototype_typed_projection_intern(
			&db, 8, 8, branch_projection_1, &typed_1
		) != 0 || prototype_typed_projection_intern(
			&db, 8, 8, branch_projection_1, &typed_1_again
		) != 0 || typed_1 != typed_1_again ||
		prototype_typed_projection_intern(
			&db, 8, 8, branch_projection_lifted_1, &typed_lifted_1
		) != 0 || typed_1 == typed_lifted_1) {
		fprintf(stderr, "branch typed projection interning failed\n");
		return 1;
	}
	uint32_t projected_edge;
	uint32_t projected_edge_again;
	uint32_t projected_child;
	if (prototype_typed_projection_edge_intern(
			&db,
			PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE,
			owner_typed_1,
			2,
			typed_1,
			&projected_edge
		) != 0 || prototype_typed_projection_edge_intern(
			&db,
			PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE,
			owner_typed_1,
			2,
			typed_1,
			&projected_edge_again
		) != 0 || projected_edge != projected_edge_again ||
		prototype_typed_projection_edge_intern(
			&db,
			PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE,
			owner_typed_1,
			2,
			typed_lifted_1,
			&projected_edge_again
		) == 0 || prototype_typed_projection_edge_find(
			&db,
			PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE,
			owner_typed_1,
			2,
			&projected_child
		) != 0 || projected_child != typed_1 ||
		prototype_typed_projection_first_for_typing_node(&db, 8) ==
			PROTOTYPE_INVALID_ID) {
		fprintf(stderr, "typed projection edge interning failed\n");
		return 1;
	}

	uint64_t solution_revision = db.solution_revision;
	if (solutions[branch_projection_1].revision != 0 ||
		prototype_context_projection_publish(
			&db, branch_projection_1, 8, 9, 11
		) != 0 || solutions[branch_projection_1].revision != 1 ||
		db.solution_revision != solution_revision + 1 ||
		prototype_context_projection_publish(
			&db, branch_projection_1, 8, 9, 11
		) != 0 || solutions[branch_projection_1].revision != 1 ||
		db.solution_revision != solution_revision + 1 ||
		prototype_context_projection_publish(
			&db, branch_projection_1, 10, 9, 12
		) != 0 || solutions[branch_projection_1].revision != 2 ||
		db.solution_revision != solution_revision + 2 ||
		prototype_context_projection_reject(
			&db, branch_projection_1
		) == 0 || prototype_context_projection_reject(
			&db, branch_projection_lifted_1
		) != 0 || solutions[branch_projection_lifted_1].revision != 1 ||
		db.solution_revision != solution_revision + 3 ||
		prototype_context_projection_publish(
			&db, branch_projection_lifted_1, 8, 9, 13
		) == 0) {
		fprintf(stderr, "projection solution memo update failed\n");
		return 1;
	}

	if (prototype_context_projection_db_validate(
			&db,
			16,
			16,
			16,
			16,
			16,
			16,
			16
		) != 0 || db.action_intern_hits == 0 ||
		db.projection_intern_hits == 0 || db.typed_projection_intern_hits == 0 ||
		db.match_case_projection_intern_hits == 0 ||
		db.typed_projection_edge_intern_hits == 0) {
		fprintf(stderr, "context projection validation failed\n");
		return 1;
	}
	if (check_rooted_topology() != 0) {
		fprintf(stderr, "rooted typed projection topology failed\n");
		return 1;
	}

	printf("context projection check passed\n");
	return 0;
}
