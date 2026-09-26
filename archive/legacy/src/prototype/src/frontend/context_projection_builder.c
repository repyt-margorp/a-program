#include "a_program/frontend/context_projection_builder.h"

#include "a_program/core/graph.h"
#include "a_program/frontend/typing_source_results.h"
#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/kernel/context.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int context_projection_restrict_action(
	struct prototype_context_projection_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_candidate_context,
	uint32_t target_candidate_context,
	uint32_t source_action,
	uint32_t* p_target_action
) {
	if (!db || !contexts || !p_target_action ||
		prototype_context_is_ancestor(
			contexts, target_candidate_context, source_candidate_context
		) != 1) {
		fprintf(
			stderr,
			"invalid Context restriction source=%u target=%u action=%u\n",
			source_candidate_context,
			target_candidate_context,
			source_action
		);
		return -1;
	}
	const struct prototype_context_action_expression* source =
		prototype_context_action_expression_get(db, source_action);
	if (!source || source->target_candidate_context != source_candidate_context) {
		fprintf(
			stderr,
			"Context restriction action mismatch source=%u target=%u action=%u "
			"action-target=%u\n",
			source_candidate_context,
			target_candidate_context,
			source_action,
			source ? source->target_candidate_context : PROTOTYPE_INVALID_ID
		);
		return -1;
	}
	if (source_candidate_context == target_candidate_context) {
		*p_target_action = source_action;
		return 0;
	}
	return prototype_context_action_restriction(
		db, source_action, target_candidate_context, p_target_action
	);
}

static int context_projection_lift_action(
	struct prototype_context_projection_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_candidate_context,
	uint32_t target_candidate_context,
	uint32_t source_action,
	uint32_t* p_target_action
) {
	if (!db || !contexts || !p_target_action) {
		return -1;
	}
	const struct prototype_context_action_expression* action =
		prototype_context_action_expression_get(db, source_action);
	if (!action || action->target_candidate_context != source_candidate_context) {
		fprintf(
			stderr,
			"Context action source mismatch source=%u target=%u action=%u "
			"action-target=%u\n",
			source_candidate_context,
			target_candidate_context,
			source_action,
			action ? action->target_candidate_context : PROTOTYPE_INVALID_ID
		);
		return -1;
	}
	if (prototype_context_is_ancestor(
			contexts, target_candidate_context, source_candidate_context
		) == 1) {
		return context_projection_restrict_action(
			db,
			contexts,
			source_candidate_context,
			target_candidate_context,
			source_action,
			p_target_action
		);
	}
	uint32_t path[512];
	uint32_t path_count;
	if (prototype_context_extension_path(
			contexts,
			source_candidate_context,
			target_candidate_context,
			path,
			512,
			&path_count
		) != 0) {
		fprintf(
			stderr,
			"missing Context extension path source=%u target=%u action=%u\n",
			source_candidate_context,
			target_candidate_context,
			source_action
		);
		return -1;
	}
	uint32_t lifted = source_action;
	for (uint32_t i = 0; i < path_count; ++i) {
		if (prototype_context_action_projection(
				db, lifted, path[i], &lifted
			) != 0) {
			return -1;
		}
	}
	*p_target_action = lifted;
	return 0;
}

static int context_projection_visit_occurrence(
	struct prototype_context_projection_db* db,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	uint32_t* publication_projections,
	size_t publication_projection_capacity,
	int record_publication,
	uint32_t source_occurrence,
	uint32_t action,
	uint32_t* match_projection_stack,
	uint32_t match_projection_count,
	uint32_t depth,
	uint32_t* p_typed_projection
) {
	if (!db || !occurrences || !contexts || !match_projection_stack ||
		!p_typed_projection ||
		depth > 512 || source_occurrence >= occurrences->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* occurrence =
		&occurrences->occurrences[source_occurrence];
	const struct prototype_context_action_expression* action_node =
		prototype_context_action_expression_get(db, action);
	uint32_t context_projection;
	uint32_t typed_projection;
	if (!action_node ||
		action_node->target_candidate_context != occurrence->context_id) {
		return -1;
	}
	if (prototype_context_projection_intern(
			db, occurrence->context_id, action, &context_projection
		) != 0) {
		return -1;
	}
	if (prototype_typed_projection_intern(
			db, source_occurrence, source_occurrence, context_projection,
			&typed_projection
		) != 0) {
		return -1;
	}
	if (record_publication && publication_projections) {
		if (source_occurrence >= publication_projection_capacity) {
			return -1;
		}
		if (publication_projections[source_occurrence] == PROTOTYPE_INVALID_ID) {
			publication_projections[source_occurrence] = typed_projection;
		}
	}
	*p_typed_projection = typed_projection;
	struct prototype_typed_projection* projected =
		&db->typed_projections[typed_projection];
	if (projected->topology_expanded) {
		return 0;
	}
	projected->topology_expanded = 1;

	/* REFERENCE and EXPECTED_TYPE are both Layer T wrappers around one accepted
	 * source occurrence. Restricting the Context action to the wrapped source
	 * naturally reuses a separately rooted definition and also preserves a
	 * lexical computation-result occurrence. */
	if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
		occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		if (occurrence->wrapped_occurrence >= occurrences->occurrence_count) {
			return -1;
		}
		uint32_t wrapped_action;
		uint32_t wrapped_projection;
		uint32_t projected_edge;
		if (context_projection_lift_action(
				db,
				contexts,
				occurrence->context_id,
				occurrences->occurrences[
					occurrence->wrapped_occurrence
				].context_id,
				action,
				&wrapped_action
			) != 0) {
			return -1;
		}
		if (context_projection_visit_occurrence(
				db,
				occurrences,
				contexts,
				publication_projections,
				publication_projection_capacity,
				occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE ?
					record_publication : 0,
				occurrence->wrapped_occurrence,
				wrapped_action,
				match_projection_stack,
				match_projection_count,
				depth + 1,
				&wrapped_projection
			) != 0) {
			return -1;
		}
		if (prototype_typed_projection_edge_intern(
				db,
				PROTOTYPE_TYPED_PROJECTION_EDGE_WRAPPED,
				typed_projection,
				occurrence->wrapped_occurrence,
				wrapped_projection,
				&projected_edge
			) != 0) {
			return -1;
		}
	}

	if (occurrence->first_edge != PROTOTYPE_INVALID_ID &&
		(occurrence->first_edge > occurrences->edge_count ||
		 occurrence->edge_count > occurrences->edge_count -
			occurrence->first_edge)) {
		return -1;
	}
	for (uint32_t i = 0; i < occurrence->edge_count; ++i) {
		uint32_t source_edge = occurrence->first_edge + i;
		const struct prototype_typed_occurrence_edge* edge =
			&occurrences->edges[source_edge];
		if (edge->child_occurrence >= occurrences->occurrence_count) {
			return -1;
		}
		if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
			edge->role == PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY) {
			continue;
		}
		uint32_t child_action;
		uint32_t child_projection;
		uint32_t projected_edge;
		if (context_projection_lift_action(
				db,
				contexts,
				occurrence->context_id,
				occurrences->occurrences[edge->child_occurrence].context_id,
				action,
				&child_action
			) != 0) {
			return -1;
		}
		if (context_projection_visit_occurrence(
				db,
				occurrences,
				contexts,
				publication_projections,
				publication_projection_capacity,
				record_publication,
				edge->child_occurrence,
				child_action,
				match_projection_stack,
				match_projection_count,
				depth + 1,
				&child_projection
			) != 0) {
			return -1;
		}
		if (prototype_typed_projection_edge_intern(
				db,
				PROTOTYPE_TYPED_PROJECTION_EDGE_CHILD,
				typed_projection,
				source_edge,
				child_projection,
				&projected_edge
			) != 0) {
			return -1;
		}
	}

	if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		uint32_t binding_context;
		int binding_status = prototype_context_find_binding(
			contexts,
			occurrence->context_id,
			occurrence->binding_id,
			&binding_context
		);
		if (binding_status < 0) {
			return -1;
		}
		if (binding_status == 0) {
			const struct prototype_context* binding = prototype_context_get(
				contexts, binding_context
			);
			if (!binding) {
				return -1;
			}
			if (binding->extension_kind ==
					PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT) {
				if (binding->producer_occurrence >= occurrences->occurrence_count) {
					return -1;
				}
				uint32_t producer_action;
				uint32_t producer_projection;
				uint32_t projected_edge;
				if (context_projection_lift_action(
						db,
						contexts,
						occurrence->context_id,
						occurrences->occurrences[
							binding->producer_occurrence
						].context_id,
						action,
						&producer_action
					) != 0) {
					return -1;
				}
				if (context_projection_visit_occurrence(
						db,
						occurrences,
						contexts,
						publication_projections,
						publication_projection_capacity,
						0,
					binding->producer_occurrence,
					producer_action,
					match_projection_stack,
					match_projection_count,
					depth + 1,
						&producer_projection
					) != 0) {
					return -1;
				}
				if (prototype_typed_projection_edge_intern(
						db,
						PROTOTYPE_TYPED_PROJECTION_EDGE_SEQUENCE_PRODUCER,
						typed_projection,
						binding->producer_occurrence,
						producer_projection,
						&projected_edge
					) != 0) {
					return -1;
				}
			}
		}
	}
	if (occurrence->tag ==
			PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		uint32_t owner_projection = PROTOTYPE_INVALID_ID;
		for (uint32_t i = match_projection_count; i > 0; --i) {
			const struct prototype_typed_projection* candidate =
				prototype_typed_projection_get(db, match_projection_stack[i - 1]);
			if (candidate && candidate->source_occurrence ==
					occurrence->ih_owner_occurrence) {
				owner_projection = match_projection_stack[i - 1];
				break;
			}
		}
		if (owner_projection == PROTOTYPE_INVALID_ID) {
			fprintf(
				stderr,
				"IH projection owner is not an ancestor occurrence=%u owner=%u\n",
				source_occurrence,
				occurrence->ih_owner_occurrence
			);
			return -1;
		}
		uint32_t projected_edge;
		if (prototype_typed_projection_edge_intern(
				db,
				PROTOTYPE_TYPED_PROJECTION_EDGE_INDUCTION_OWNER,
				typed_projection,
				occurrence->ih_owner_occurrence,
				owner_projection,
				&projected_edge
			) != 0) {
			return -1;
		}
	}

	if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		return 0;
	}
	if (occurrence->first_case == PROTOTYPE_INVALID_ID ||
		occurrence->first_case > occurrences->case_count ||
		occurrence->case_count > occurrences->case_count -
			occurrence->first_case) {
		return -1;
	}
	if (match_projection_count >= 513) {
		return -1;
	}
	match_projection_stack[match_projection_count] = typed_projection;
	for (uint32_t case_index = 0;
		case_index < occurrence->case_count;
		++case_index) {
		uint32_t source_case = occurrence->first_case + case_index;
		const struct prototype_typed_occurrence_match_case* operation_case =
			&occurrences->cases[source_case];
		uint32_t branch_occurrence;
		uint32_t input_action;
		uint32_t input_context_projection;
		uint32_t match_case_projection;
		uint32_t branch_action;
		uint32_t body_action;
		uint32_t body_projection;
		uint32_t projected_edge;
		if (prototype_typed_occurrence_graph_child(
				occurrences,
				source_occurrence,
				PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
				case_index,
				&branch_occurrence
			) != 0 || branch_occurrence >= occurrences->occurrence_count) {
			return -1;
		}
		if (context_projection_lift_action(
				db,
				contexts,
				occurrence->context_id,
				operation_case->context_id,
				action,
				&input_action
			) != 0) {
			return -1;
		}
		if (prototype_context_projection_intern(
				db,
				operation_case->context_id,
				input_action,
				&input_context_projection
			) != 0) {
			return -1;
		}
		if (prototype_typed_match_case_projection_intern(
				db,
				source_case,
				typed_projection,
				input_context_projection,
				&match_case_projection
			) != 0) {
			return -1;
		}
		if (prototype_context_action_branch_refinement(
				db,
				input_action,
				operation_case->context_id,
				match_case_projection,
				&branch_action
			) != 0) {
			return -1;
		}
		if (context_projection_lift_action(
				db,
				contexts,
				operation_case->context_id,
				occurrences->occurrences[branch_occurrence].context_id,
				branch_action,
				&body_action
			) != 0) {
			return -1;
		}
		if (context_projection_visit_occurrence(
				db,
				occurrences,
				contexts,
				publication_projections,
				publication_projection_capacity,
				record_publication,
				branch_occurrence,
				body_action,
				match_projection_stack,
				match_projection_count + 1,
				depth + 1,
				&body_projection
			) != 0) {
			return -1;
		}
		if (prototype_typed_projection_edge_intern(
				db,
				PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE,
				typed_projection,
				source_case,
				body_projection,
				&projected_edge
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int context_projection_build_rooted_topology_internal(
	struct prototype_context_projection_db* db,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	uint32_t* publication_projections,
	size_t publication_projection_capacity,
	const uint32_t* root_occurrences,
	size_t root_count
) {
	if (!db || !occurrences || !contexts ||
		(root_count > 0 && !root_occurrences) ||
		occurrences->occurrence_count >
			db->typed_projection_typing_node_capacity) {
		return -1;
	}
	for (size_t i = 0; i < root_count; ++i) {
		uint32_t root = root_occurrences[i];
		if (root >= occurrences->occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* occurrence =
			&occurrences->occurrences[root];
		uint32_t identity;
		uint32_t typed_projection;
		uint32_t match_projection_stack[513];
		if (prototype_context_action_identity(
				db, occurrence->context_id, &identity
			) != 0) {
			return -1;
		}
		if (context_projection_visit_occurrence(
				db,
				occurrences,
				contexts,
				publication_projections,
				publication_projection_capacity,
				1,
				root,
				identity,
				match_projection_stack,
				0,
				0,
				&typed_projection
			) != 0) {
			fprintf(
				stderr,
				"Layer T root expansion failed root=%u tag=%d context=%u "
				"wrapped=%u ih-owner=%u\n",
				root,
				occurrence->tag,
				occurrence->context_id,
				occurrence->wrapped_occurrence,
				occurrence->ih_owner_occurrence
			);
			return -1;
		}
	}
	return 0;
}

int prototype_context_projection_build_rooted_topology(
	struct prototype_context_projection_db* db,
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	const uint32_t* root_occurrences,
	size_t root_count
) {
	return context_projection_build_rooted_topology_internal(
		db, occurrences, contexts, NULL, 0, root_occurrences, root_count
	);
}

int prototype_typing_graph_build_rooted_projections(
	const struct prototype_typing_graph_build_context* context
) {
	if (!context || !context->source_results || !context->occurrences ||
		!context->contexts || !context->substitutions || !context->projections ||
		!context->base_projection_for_occurrence ||
		!context->publication_projection_for_occurrence ||
		context->occurrences->occurrence_count > UINT32_MAX ||
		context->occurrences->occurrence_count >
			context->base_projection_capacity ||
		context->occurrences->occurrence_count >
			context->publication_projection_capacity ||
		context->transaction_occurrence_start >
			context->occurrences->occurrence_count ||
		context->source_results->assignment_count >
			SIZE_MAX - context->occurrences->occurrence_count - 1 ||
		context->source_results->assignment_count +
			context->occurrences->occurrence_count + 1 >
			SIZE_MAX / sizeof(uint32_t)) {
		return -1;
	}
	if (getenv("A_PROGRAM_PROJECTION_TRACE") != NULL) {
		for (uint32_t occurrence = context->transaction_occurrence_start;
			occurrence < context->occurrences->occurrence_count;
			++occurrence) {
			const struct prototype_typed_occurrence* source =
				&context->occurrences->occurrences[occurrence];
			fprintf(
				stderr,
				"projection-source occurrence=%u tag=%d context=%u ast=%u "
				"wrapped=%u ih-owner=%u first-edge=%u edge-count=%u\n",
				occurrence,
				source->tag,
				source->context_id,
				source->source_ast,
				source->wrapped_occurrence,
				source->ih_owner_occurrence,
				source->first_edge,
				source->edge_count
			);
			for (uint32_t edge_offset = 0;
				edge_offset < source->edge_count;
				++edge_offset) {
				const struct prototype_typed_occurrence_edge* edge =
					&context->occurrences->edges[
						source->first_edge + edge_offset
					];
				fprintf(
					stderr,
					"projection-source-edge parent=%u role=%d ordinal=%u child=%u\n",
					occurrence,
					edge->role,
					edge->ordinal,
					edge->child_occurrence
				);
			}
		}
	}
	size_t root_capacity = context->source_results->assignment_count +
		context->occurrences->occurrence_count + 1;
	uint32_t* roots = malloc(root_capacity * sizeof(*roots));
	uint8_t* seen = context->occurrences->occurrence_count == 0 ? NULL :
		calloc(context->occurrences->occurrence_count, sizeof(*seen));
	uint8_t* incoming = context->occurrences->occurrence_count == 0 ? NULL :
		calloc(context->occurrences->occurrence_count, sizeof(*incoming));
	if (!roots || (context->occurrences->occurrence_count != 0 &&
		(!seen || !incoming))) {
		free(roots);
		free(seen);
		free(incoming);
		return -1;
	}
	size_t root_count = 0;
	for (uint32_t i = 0; i < context->source_results->assignment_count; ++i) {
		const struct prototype_typing_source_assignment_result* assignment =
			prototype_typing_source_assignment_result_get(
				context->source_results, i
			);
		uint32_t root = assignment ? assignment->occurrence : PROTOTYPE_INVALID_ID;
		if (!assignment || assignment->state != PROTOTYPE_TYPING_SOURCE_CHECKED ||
			root < context->transaction_occurrence_start ||
			root >= context->occurrences->occurrence_count || seen[root]) {
			continue;
		}
		seen[root] = 1;
		roots[root_count++] = root;
	}
	uint32_t selected = context->selected_entry_occurrence;
	if (selected >= context->transaction_occurrence_start &&
		selected < context->occurrences->occurrence_count && !seen[selected]) {
		seen[selected] = 1;
		roots[root_count++] = selected;
	}
	if (root_count > 0 && context_projection_build_rooted_topology_internal(
			context->projections,
			context->occurrences,
			context->contexts,
			context->publication_projection_for_occurrence,
			context->publication_projection_capacity,
			roots,
			root_count
		) != 0) {
		fprintf(stderr, "failed to build declared Layer T projection roots\n");
		free(roots);
		free(seen);
		free(incoming);
		return -1;
	}
	/* Every still-unprojected zero-indegree component is a Layer T root. Source
	 * and generated AST identities are diagnostic provenance, not topology. */
	memset(
		incoming,
		0,
		context->occurrences->occurrence_count * sizeof(*incoming)
	);
	for (uint32_t parent = context->transaction_occurrence_start;
		parent < context->occurrences->occurrence_count;
		++parent) {
		if (prototype_typed_projection_first_for_typing_node(
				context->projections, parent
			) != PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_typed_occurrence* source =
			&context->occurrences->occurrences[parent];
		if (source->edge_count != 0 &&
			(source->first_edge >= context->occurrences->edge_count ||
			 source->edge_count > context->occurrences->edge_count -
				source->first_edge)) {
			free(roots);
			free(seen);
			free(incoming);
			return -1;
		}
		for (uint32_t i = 0; i < source->edge_count; ++i) {
			uint32_t child = context->occurrences->edges[
				source->first_edge + i
			].child_occurrence;
			if (child >= context->occurrences->occurrence_count) {
				free(roots);
				free(seen);
				free(incoming);
				return -1;
			}
			if (prototype_typed_projection_first_for_typing_node(
					context->projections, child
				) == PROTOTYPE_INVALID_ID) {
				incoming[child] = 1;
			}
		}
		uint32_t wrapped = source->wrapped_occurrence;
		if (wrapped != PROTOTYPE_INVALID_ID) {
			if (wrapped >= context->occurrences->occurrence_count) {
				free(roots);
				free(seen);
				free(incoming);
				return -1;
			}
			if (prototype_typed_projection_first_for_typing_node(
					context->projections, wrapped
				) == PROTOTYPE_INVALID_ID) {
				incoming[wrapped] = 1;
			}
		}
	}
	size_t generated_root_start = root_count;
	for (uint32_t occurrence = context->transaction_occurrence_start;
		occurrence < context->occurrences->occurrence_count;
		++occurrence) {
		if (prototype_typed_projection_first_for_typing_node(
				context->projections, occurrence
			) != PROTOTYPE_INVALID_ID || incoming[occurrence]) {
			continue;
		}
		seen[occurrence] = 1;
		roots[root_count++] = occurrence;
	}
	if (root_count > generated_root_start &&
		context_projection_build_rooted_topology_internal(
			context->projections,
			context->occurrences,
			context->contexts,
			context->publication_projection_for_occurrence,
			context->publication_projection_capacity,
			&roots[generated_root_start],
			root_count - generated_root_start
		) != 0) {
		fprintf(stderr, "failed to build generated Layer T projection roots\n");
		free(roots);
		free(seen);
		free(incoming);
		return -1;
	}
	if (root_count == 0 && context->transaction_occurrence_start <
			context->occurrences->occurrence_count) {
		free(roots);
		free(seen);
		free(incoming);
		return -1;
	}
	for (uint32_t occurrence = context->transaction_occurrence_start;
		occurrence < context->occurrences->occurrence_count;
		++occurrence) {
		uint32_t first_projection =
			prototype_typed_projection_first_for_typing_node(
				context->projections, occurrence
			);
		if (first_projection != PROTOTYPE_INVALID_ID) {
			/* A sequence producer is reached through the consuming Context and is
			 * deliberately not allowed to replace a separately rooted publication.
			 * When it has no such root, fix its first rooted exact projection here.
			 * Later phases may read this key but must never search for a representative. */
			if (context->publication_projection_for_occurrence[occurrence] ==
					PROTOTYPE_INVALID_ID) {
				context->publication_projection_for_occurrence[occurrence] =
					first_projection;
			}
			continue;
		}
		const struct prototype_typed_occurrence* source =
			&context->occurrences->occurrences[occurrence];
		fprintf(
			stderr,
			"unreachable Layer T source occurrence=%u tag=%d context=%u "
			"core=%u ast=%u wrapped=%u\n",
			occurrence,
			source->tag,
			source->context_id,
			source->core_term,
			source->source_ast,
			source->wrapped_occurrence
		);
		free(roots);
		free(seen);
		free(incoming);
		return -1;
	}
	/* Fix one canonical source projection before solving. A leaf has no ancestor
	 * dependency, so its identity projection is a valid source judgement. A
	 * structural occurrence (notably Match/IH) keeps the rooted projection that
	 * carries its exact ancestor path. */
	for (uint32_t source_occurrence = context->transaction_occurrence_start;
		source_occurrence < context->occurrences->occurrence_count;
		++source_occurrence) {
		if (context->base_projection_for_occurrence[source_occurrence] !=
				PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_typed_occurrence* occurrence =
			&context->occurrences->occurrences[source_occurrence];
		uint32_t typed_projection =
			prototype_typed_projection_first_for_typing_node(
				context->projections, source_occurrence
			);
		int independent_leaf = occurrence->edge_count == 0 &&
			occurrence->wrapped_occurrence == PROTOTYPE_INVALID_ID &&
			(occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM ||
			 occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR);
		if (independent_leaf) {
			uint32_t action;
			uint32_t match_projection_stack[513];
			if (prototype_context_action_identity(
					context->projections, occurrence->context_id, &action
				) != 0 || context_projection_visit_occurrence(
					context->projections,
					context->occurrences,
					context->contexts,
					context->publication_projection_for_occurrence,
					context->publication_projection_capacity,
					0,
					source_occurrence,
					action,
					match_projection_stack,
					0,
					0,
					&typed_projection
				) != 0) {
				free(roots);
				free(seen);
				free(incoming);
				return -1;
			}
			if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM) {
				/* An atom's source judgement is its publication authority. Parent
				 * projections remain local Layer T evidence. Constructors retain the
				 * rooted view selected by their formation proof. */
				context->publication_projection_for_occurrence[source_occurrence] =
					typed_projection;
			}
		}
		const struct prototype_typed_projection* projection =
			prototype_typed_projection_get(
				context->projections, typed_projection
			);
		if (!projection || !projection->topology_expanded) {
			free(roots);
			free(seen);
			free(incoming);
			return -1;
		}
		context->base_projection_for_occurrence[source_occurrence] =
			typed_projection;
	}
	int status = prototype_context_projection_db_validate(
		context->projections,
		context->contexts->context_count,
		context->occurrences->occurrence_count,
		context->occurrences->occurrence_count,
		context->occurrences->edge_count,
		context->occurrences->case_count,
		context->contexts->context_count,
		context->substitutions->substitution_count
	);
	if (status != 0) {
		fprintf(stderr, "invalid Layer T projection database\n");
	}
	free(roots);
	free(seen);
	free(incoming);
	return status;
}
