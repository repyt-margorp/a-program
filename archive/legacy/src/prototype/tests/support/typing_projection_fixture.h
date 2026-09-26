#ifndef A_PROGRAM_PROTOTYPE_TESTS_SUPPORT_TYPING_PROJECTION_FIXTURE_H
#define A_PROGRAM_PROTOTYPE_TESTS_SUPPORT_TYPING_PROJECTION_FIXTURE_H

#include "a_program/frontend/typing_pipeline.h"

static inline int prototype_test_install_term_typing_slots(
	struct prototype_typing_pipeline* typing,
	uint32_t occurrence_count
) {
	if (!typing || typing->typing_occurrence_count != occurrence_count ||
		typing->source_topology_slot_count != 0) return -1;
	for (uint32_t occurrence = 0;
		occurrence < occurrence_count;
		++occurrence) {
		typing->typing_occurrences[occurrence].typing_node_id = occurrence;
		typing->typing_occurrences[occurrence].source_typing_node_id = occurrence;
		typing->source_topology_slots[occurrence] =
			(struct prototype_source_typing_slot_result) {
				.typing_node_id = occurrence,
				.occurrence_id = occurrence,
				.source_core_term_id = typing->typing_occurrences[
					occurrence
				].core_term_id,
				.node_kind = PROTOTYPE_SOURCE_TYPING_TERM_OCCURRENCE
			};
	}
	typing->source_topology_slot_count = occurrence_count;
	return 0;
}

/* Test-only construction of one explicit identity projection per occurrence. */
static inline int prototype_test_seal_identity_typing_projections(
	struct prototype_typing_pipeline* typing,
	uint32_t occurrence_count
) {
	if (!typing || !typing->source_topology_sealed ||
		typing->typing_occurrence_count != occurrence_count ||
		typing->context_projections.typed_projection_count != 0) return -1;
	if (typing->source_topology_slot_count == 0 &&
		prototype_test_install_term_typing_slots(
			typing, occurrence_count
		) != 0) return -1;
	uint32_t action;
	uint32_t context_projection;
	if (prototype_context_action_identity(
			&typing->context_projections, 0, &action
		) != 0 || prototype_context_projection_intern(
			&typing->context_projections, 0, action, &context_projection
		) != 0) return -1;
	for (uint32_t occurrence = 0; occurrence < occurrence_count; ++occurrence) {
		uint32_t typed_projection;
		uint32_t typing_node = typing->typing_occurrences[
			occurrence
		].typing_node_id;
		if (typing->typing_occurrences[occurrence].occurrence_id != occurrence ||
			typing_node == PROTOTYPE_INVALID_ID ||
			prototype_typed_projection_intern(
				&typing->context_projections, typing_node, occurrence,
				context_projection,
				&typed_projection
			) != 0 || typed_projection != occurrence) return -1;
		typing->base_typed_projection_for_occurrence[occurrence] =
			typed_projection;
	}
	return prototype_typing_pipeline_finish_graph_build(typing) != 0 ||
		prototype_typing_pipeline_seal_projection_solutions(typing) != 0 ?
		-1 : 0;
}

#endif
