#include "a_program/frontend/typing_pipeline.h"

#include <string.h>

static void typing_constraint_topology_mix(
	uint64_t* hash,
	uint64_t value
) {
	*hash ^= value;
	*hash *= UINT64_C(1099511628211);
}

static void typing_constraint_classifier_edge_digest(
	uint64_t* hash,
	const struct prototype_typing_constraint* constraint
) {
	const union prototype_typing_classifier_goal_payload* goal =
		&constraint->payload.classifier.goal;
	switch (constraint->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE:
			break;
		case OPERATION_CONSTRAINT_EQUAL:
			typing_constraint_topology_mix(
				hash, goal->reference.referenced_operation
			);
			break;
		case OPERATION_CONSTRAINT_IMPORTED_REFERENCE:
			typing_constraint_topology_mix(
				hash, goal->imported_reference.classifier_core_term
			);
			typing_constraint_topology_mix(
				hash, (uint32_t)goal->imported_reference.import_kind
			);
			typing_constraint_topology_mix(
				hash, goal->imported_reference.import_id
			);
			break;
		case OPERATION_CONSTRAINT_INTRINSIC_REFERENCE:
			typing_constraint_topology_mix(
				hash, (uint32_t)goal->intrinsic_reference.target_kind
			);
			typing_constraint_topology_mix(
				hash, goal->intrinsic_reference.target_id
			);
			typing_constraint_topology_mix(
				hash, (uint32_t)goal->intrinsic_reference.type_symbol_id
			);
			break;
		case OPERATION_CONSTRAINT_CONVERTIBLE:
			typing_constraint_topology_mix(
				hash, goal->conversion.body_occurrence
			);
			typing_constraint_topology_mix(
				hash, goal->conversion.expected_classifier
			);
			break;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			typing_constraint_topology_mix(hash, (uint32_t)goal->pi.role);
			typing_constraint_topology_mix(
				hash, goal->pi.body_or_function_operation
			);
			typing_constraint_topology_mix(
				hash,
				goal->pi.domain_classifier_or_argument_occurrence
			);
			break;
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
			typing_constraint_topology_mix(
				hash, goal->motive_case.branch_operation
			);
			typing_constraint_topology_mix(
				hash, goal->motive_case.scrutinee_operation
			);
			typing_constraint_topology_mix(
				hash, goal->motive_case.branch_typed_projection
			);
			typing_constraint_topology_mix(
				hash, goal->motive_case.scrutinee_typed_projection
			);
			typing_constraint_topology_mix(
				hash, goal->motive_case.match_case_projection
			);
			typing_constraint_topology_mix(
				hash, goal->motive_case.case_index
			);
			typing_constraint_topology_mix(
				hash, goal->motive_case.constructor_owner
			);
			typing_constraint_topology_mix(
				hash, goal->motive_case.constructor_index
			);
			typing_constraint_topology_mix(
				hash, goal->motive_case.ih_scope_id
			);
			break;
		case OPERATION_CONSTRAINT_IH_EXPECTED:
			typing_constraint_topology_mix(
				hash,
				goal->induction_hypothesis.owner_match_occurrence
			);
			typing_constraint_topology_mix(
				hash,
				goal->induction_hypothesis.recursive_argument_occurrence
			);
			typing_constraint_topology_mix(
				hash,
				goal->induction_hypothesis.recursive_argument_binding_id
			);
			typing_constraint_topology_mix(
				hash, goal->induction_hypothesis.ih_scope_id
			);
			typing_constraint_topology_mix(
				hash, goal->induction_hypothesis.case_index
			);
			typing_constraint_topology_mix(
				hash, goal->induction_hypothesis.field_index
			);
			break;
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
			typing_constraint_topology_mix(
				hash, goal->cbpv_boundary.child_operation
			);
			break;
		case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			typing_constraint_topology_mix(
				hash, goal->computation_fold.computation_typed_projection
			);
			typing_constraint_topology_mix(
				hash, goal->computation_fold.return_body_typed_projection
			);
			typing_constraint_topology_mix(
				hash, goal->computation_fold.fold_topology_id
			);
			break;
		case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
			typing_constraint_topology_mix(
				hash, goal->operation_request.operation_typed_projection
			);
			typing_constraint_topology_mix(
				hash, goal->operation_request.argument_typed_projection
			);
			typing_constraint_topology_mix(
				hash, goal->operation_request.continuation_typed_projection
			);
			break;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			typing_constraint_topology_mix(
				hash, goal->constructor_formation.function_operation
			);
			typing_constraint_topology_mix(
				hash, goal->constructor_formation.argument_occurrence
			);
			break;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_HEAD:
			typing_constraint_topology_mix(
				hash, goal->constructor_head.constructor_id
			);
			typing_constraint_topology_mix(
				hash, goal->constructor_head.owner_typed_projection
			);
			break;
		case OPERATION_CONSTRAINT_BINDER_TYPE:
			typing_constraint_topology_mix(
				hash, goal->binder_type.binding_context
			);
			typing_constraint_topology_mix(
				hash, goal->binder_type.binding_id
			);
			break;
		case OPERATION_CONSTRAINT_TERMINATION_WITNESS:
			/* The exact computation endpoint is an ordinary operand edge. */
			break;
		default:
			typing_constraint_topology_mix(hash, UINT64_MAX);
			break;
	}
}

static uint64_t typing_constraint_topology_digest(
	const struct prototype_typing_constraint_db* db
) {
	if (!db) {
		return 0;
	}
	uint64_t hash = UINT64_C(1469598103934665603);
	typing_constraint_topology_mix(&hash, db->constraint_count);
	typing_constraint_topology_mix(&hash, db->operand_count);
	typing_constraint_topology_mix(&hash, db->classifier_rhs_count);
	for (uint32_t domain = OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER;
		domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT;
		++domain) {
		typing_constraint_topology_mix(
			&hash, db->constraint_count_by_domain[domain]
		);
	}
	for (uint32_t domain = OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER;
		domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT;
		++domain) {
		for (uint32_t id = db->first_constraint_for_domain[domain];
			id != PROTOTYPE_INVALID_ID;
			id = db->next_constraint_in_domain[id]) {
			typing_constraint_topology_mix(&hash, id);
		}
	}
	for (uint32_t rhs_id = 0; rhs_id < db->classifier_rhs_count; ++rhs_id) {
		const struct prototype_typing_classifier_rhs* rhs =
			&db->classifier_rhs[rhs_id];
		typing_constraint_topology_mix(&hash, rhs_id);
		typing_constraint_topology_mix(&hash, (uint32_t)rhs->kind);
		typing_constraint_topology_mix(&hash, rhs->source_term);
		typing_constraint_topology_mix(&hash, rhs->operand_count);
		for (uint32_t operand = 0; operand < rhs->operand_count; ++operand) {
			const struct prototype_typing_equation_operand* edge =
				prototype_typing_classifier_rhs_operand_get(db, rhs, operand);
			if (!edge) {
				typing_constraint_topology_mix(&hash, UINT64_MAX);
				continue;
			}
			typing_constraint_topology_mix(&hash, (uint32_t)edge->role);
			typing_constraint_topology_mix(&hash, (uint32_t)edge->target_kind);
			typing_constraint_topology_mix(&hash, edge->target);
			typing_constraint_topology_mix(&hash, edge->qualifier);
		}
	}
	for (uint32_t i = 0; i < db->constraint_count; ++i) {
		const struct prototype_typing_constraint* constraint =
			&db->constraints[i];
		typing_constraint_topology_mix(&hash, constraint->id);
		typing_constraint_topology_mix(
			&hash, (uint32_t)constraint->domain
		);
		typing_constraint_topology_mix(
			&hash, (uint32_t)constraint->kind
		);
		typing_constraint_topology_mix(
			&hash, constraint->owner_typed_projection
		);
		typing_constraint_topology_mix(&hash, constraint->operand_count);
		for (uint32_t operand = 0; operand < constraint->operand_count; ++operand) {
			const struct prototype_typing_equation_operand* edge =
				prototype_typing_constraint_operand_get(db, constraint, operand);
			if (!edge) {
				typing_constraint_topology_mix(&hash, UINT64_MAX);
				continue;
			}
			typing_constraint_topology_mix(
				&hash, (uint32_t)edge->role
			);
			typing_constraint_topology_mix(&hash, (uint32_t)edge->target_kind);
			typing_constraint_topology_mix(&hash, edge->target);
			typing_constraint_topology_mix(&hash, edge->qualifier);
		}
		typing_constraint_topology_mix(&hash, constraint->source_occurrence);
		typing_constraint_topology_mix(&hash, constraint->source_ast);
		typing_constraint_topology_mix(
			&hash, constraint->origin_constraint_id
		);
		typing_constraint_topology_mix(
			&hash, constraint->classifier_rhs_root
		);
		switch (constraint->domain) {
			case OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER:
				typing_constraint_classifier_edge_digest(
					&hash, constraint
				);
				break;
			case OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT:
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.branch_refinement.match_occurrence
				);
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.branch_refinement.scrutinee_occurrence
				);
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.branch_refinement.case_index
				);
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.branch_refinement.source_context
				);
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.branch_refinement.match_typed_projection
				);
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.branch_refinement.scrutinee_typed_projection
				);
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.branch_refinement.branch_typed_projection
				);
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.branch_refinement.match_case_projection
				);
				break;
			case OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW:
				if (constraint->kind ==
						PROTOTYPE_TYPING_EFFECT_CONSTRAINT_NARY_UNION ||
					constraint->kind ==
						PROTOTYPE_TYPING_EFFECT_CONSTRAINT_COMPUTATION_FOLD) {
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.first_operand
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.operand_count
					);
					typing_constraint_topology_mix(
						&hash,
						constraint->payload.projection_fold.first_auxiliary_operand
					);
					typing_constraint_topology_mix(
						&hash,
						constraint->payload.projection_fold.auxiliary_operand_count
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.topology_id
					);
				} else {
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_effect.first_operand
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_effect.second_operand
					);
				}
				break;
			case OPERATION_CONSTRAINT_DOMAIN_COMPUTATION:
				typing_constraint_topology_mix(
					&hash,
					constraint->payload.computation
						.judgement_delta_constraint_id
				);
				break;
			case PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_USAGE:
				if (constraint->kind ==
						PROTOTYPE_TYPING_USAGE_CONSTRAINT_MATCH ||
					constraint->kind ==
						PROTOTYPE_TYPING_USAGE_CONSTRAINT_COMPUTATION_FOLD) {
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.first_operand
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.operand_count
					);
					typing_constraint_topology_mix(
						&hash,
						constraint->payload.projection_fold.first_auxiliary_operand
					);
					typing_constraint_topology_mix(
						&hash,
						constraint->payload.projection_fold.auxiliary_operand_count
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.topology_id
					);
				} else {
					typing_constraint_topology_mix(
						&hash, constraint->payload.usage.first_operand
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.usage.second_operand
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.usage.binding_id
					);
				}
				break;
			case PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_TOTALITY:
				if (constraint->kind ==
						PROTOTYPE_TYPING_TOTALITY_CONSTRAINT_NARY_JOIN ||
					constraint->kind ==
						PROTOTYPE_TYPING_TOTALITY_CONSTRAINT_COMPUTATION_FOLD) {
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.first_operand
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.operand_count
					);
					typing_constraint_topology_mix(
						&hash,
						constraint->payload.projection_fold.first_auxiliary_operand
					);
					typing_constraint_topology_mix(
						&hash,
						constraint->payload.projection_fold.auxiliary_operand_count
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.projection_fold.topology_id
					);
				} else {
					typing_constraint_topology_mix(
						&hash, constraint->payload.totality.first_operand
					);
					typing_constraint_topology_mix(
						&hash, constraint->payload.totality.second_operand
					);
					typing_constraint_topology_mix(
						&hash, (uint32_t)constraint->payload.totality.constant
					);
				}
				break;
			default:
				typing_constraint_topology_mix(&hash, UINT64_MAX);
				break;
		}
	}
	typing_constraint_topology_mix(&hash, db->dependent_constraint_count);
	for (uint32_t i = 0; i < db->dependent_constraint_count; ++i) {
		typing_constraint_topology_mix(
			&hash, (uint32_t)db->dependent_constraints[i].source_kind
		);
		typing_constraint_topology_mix(
			&hash, db->dependent_constraints[i].source
		);
		typing_constraint_topology_mix(
			&hash, db->dependent_constraints[i].constraint
		);
		typing_constraint_topology_mix(
			&hash, db->dependent_constraints[i].next
		);
	}
	typing_constraint_topology_mix(&hash, db->branch_ih_dependency_total);
	for (uint32_t i = 0; i < db->branch_ih_dependency_total; ++i) {
		typing_constraint_topology_mix(
			&hash, db->branch_ih_dependencies[i]
		);
	}
	typing_constraint_topology_mix(&hash, db->scc_count);
	typing_constraint_topology_mix(&hash, db->scc_member_total);
	for (uint32_t i = 0; i < db->constraint_count; ++i) {
		typing_constraint_topology_mix(&hash, db->scc_for_constraint[i]);
	}
	for (uint32_t scc = 0; scc < db->scc_count; ++scc) {
		typing_constraint_topology_mix(&hash, db->first_scc_member[scc]);
		typing_constraint_topology_mix(&hash, db->scc_member_count[scc]);
		typing_constraint_topology_mix(&hash, db->scc_recursive[scc]);
	}
	for (uint32_t i = 0; i < db->scc_member_total; ++i) {
		typing_constraint_topology_mix(&hash, db->scc_members[i]);
	}
	return hash;
}


int prototype_typing_pipeline_init(
	struct prototype_typing_pipeline* pipeline
) {
	if (!pipeline) {
		return -1;
	}
	memset(pipeline, 0, sizeof(*pipeline));
	prototype_typing_binding_state_init(&pipeline->bindings);
	prototype_typing_seed_state_init(&pipeline->seed);
	if (prototype_typing_constraint_db_init(
			&pipeline->constraints
		) != 0) {
		return -1;
	}
	prototype_context_projection_db_init(
		&pipeline->context_projections,
		pipeline->context_projection_actions,
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY,
		pipeline->context_projection_nodes,
		pipeline->context_projection_solutions,
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY,
		pipeline->typed_projection_nodes,
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY,
		pipeline->typed_projection_typing_node_heads,
		PROTOTYPE_TYPING_PIPELINE_TYPING_NODE_CAPACITY,
		pipeline->typed_projection_context_heads,
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY,
		pipeline->match_case_projection_nodes,
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY,
		pipeline->typed_projection_edges,
		PROTOTYPE_TYPING_PIPELINE_PROJECTION_EDGE_CAPACITY,
		pipeline->typed_projection_edge_parent_heads,
		PROTOTYPE_TYPING_PIPELINE_CONTEXT_PROJECTION_CAPACITY
	);
	for (size_t i = 0;
		i < PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY;
		++i) {
		pipeline->base_typed_projection_for_occurrence[i] =
			PROTOTYPE_INVALID_ID;
		pipeline->publication_typed_projection_for_occurrence[i] =
			PROTOTYPE_INVALID_ID;
	}
	pipeline->initialized = 1;
	return 0;
}

void prototype_typing_pipeline_dispose(
	struct prototype_typing_pipeline* pipeline
) {
	if (!pipeline) {
		return;
	}
	memset(pipeline, 0, sizeof(*pipeline));
}

int prototype_typing_pipeline_reset_image_progress(
	struct prototype_typing_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized || pipeline->transaction_active) {
		return -1;
	}
	prototype_typing_binding_state_init(&pipeline->bindings);
	prototype_typing_seed_state_init(&pipeline->seed);
	memset(&pipeline->reindex, 0, sizeof(pipeline->reindex));
	if (prototype_typing_constraint_db_init(&pipeline->constraints) != 0 ||
		prototype_context_projection_db_rebuild_indexes(
			&pipeline->context_projections
		) != 0) {
		return -1;
	}
	return 0;
}

int prototype_typing_pipeline_begin_transaction(
	struct prototype_typing_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized || pipeline->transaction_active) {
		return -1;
	}
	pipeline->transaction_action_count =
		pipeline->context_projections.action_count;
	pipeline->transaction_context_projection_count =
		pipeline->context_projections.projection_count;
	pipeline->transaction_typed_projection_count =
		pipeline->context_projections.typed_projection_count;
	pipeline->transaction_match_case_projection_count =
		pipeline->context_projections.match_case_projection_count;
	pipeline->transaction_projection_edge_count =
		pipeline->context_projections.typed_projection_edge_count;
	pipeline->transaction_topology_revision = pipeline->topology_revision;
	pipeline->transaction_active = 1;
	return 0;
}

int prototype_typing_pipeline_commit_transaction(
	struct prototype_typing_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized || !pipeline->transaction_active) {
		return -1;
	}
	if (pipeline->transaction_revision == UINT64_MAX) {
		return -1;
	}
	pipeline->transaction_active = 0;
	++pipeline->transaction_revision;
	return 0;
}

int prototype_typing_pipeline_rollback_transaction(
	struct prototype_typing_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized || !pipeline->transaction_active) {
		return -1;
	}
	if (pipeline->transaction_revision == UINT64_MAX) {
		return -1;
	}
	struct prototype_context_projection_db* projections =
		&pipeline->context_projections;
	projections->action_count = pipeline->transaction_action_count;
	projections->projection_count =
		pipeline->transaction_context_projection_count;
	projections->typed_projection_count =
		pipeline->transaction_typed_projection_count;
	projections->match_case_projection_count =
		pipeline->transaction_match_case_projection_count;
	projections->typed_projection_edge_count =
		pipeline->transaction_projection_edge_count;
	pipeline->topology_revision = pipeline->transaction_topology_revision;
	for (size_t occurrence = 0;
		occurrence < PROTOTYPE_TYPING_PIPELINE_SOURCE_OCCURRENCE_CAPACITY;
		++occurrence) {
		if (pipeline->base_typed_projection_for_occurrence[occurrence] >=
				projections->typed_projection_count) {
			pipeline->base_typed_projection_for_occurrence[occurrence] =
				PROTOTYPE_INVALID_ID;
		}
		if (pipeline->publication_typed_projection_for_occurrence[occurrence] >=
				projections->typed_projection_count) {
			pipeline->publication_typed_projection_for_occurrence[occurrence] =
				PROTOTYPE_INVALID_ID;
		}
	}
	pipeline->transaction_active = 0;
	++pipeline->transaction_revision;
	if (prototype_typing_pipeline_reset_image_progress(pipeline) != 0) {
		return -1;
	}
	return 0;
}

int prototype_typing_pipeline_validate_constraint_topology(
	const struct prototype_typing_pipeline* pipeline,
	uint64_t* expected_digest,
	uint64_t* actual_digest
) {
	if (!pipeline || !pipeline->initialized) {
		return -1;
	}
	if (!pipeline->constraints.topology_sealed) {
		return -1;
	}
	uint64_t actual = typing_constraint_topology_digest(
		&pipeline->constraints
	);
	if (expected_digest) {
		*expected_digest = pipeline->constraints.topology_digest;
	}
	if (actual_digest) {
		*actual_digest = actual;
	}
	if (pipeline->constraints.constraint_count !=
			pipeline->constraints.sealed_constraint_count) {
		return 1;
	}
	if (pipeline->constraints.operand_count !=
			pipeline->constraints.sealed_operand_count) {
		return 1;
	}
	if (pipeline->constraints.classifier_rhs_count !=
			pipeline->constraints.sealed_classifier_rhs_count) {
		return 1;
	}
	if (pipeline->constraints.dependent_constraint_count !=
			pipeline->constraints.sealed_dependent_constraint_count) {
		return 1;
	}
	if (pipeline->constraints.branch_ih_dependency_total !=
			pipeline->constraints.sealed_branch_ih_dependency_total) {
		return 1;
	}
	if (pipeline->constraints.scc_count !=
			pipeline->constraints.sealed_scc_count ||
		pipeline->constraints.scc_member_total !=
			pipeline->constraints.sealed_scc_member_total) {
		return 1;
	}
	return actual == pipeline->constraints.topology_digest ? 0 : 1;
}

int prototype_typing_pipeline_seal_constraint_topology(
	struct prototype_typing_pipeline* pipeline,
	uint64_t* topology_digest
) {
	if (!pipeline || !pipeline->initialized) {
		return -1;
	}
	if (pipeline->constraints.topology_sealed) {
		return -1;
	}
	pipeline->constraints.topology_digest = typing_constraint_topology_digest(
		&pipeline->constraints
	);
	pipeline->constraints.sealed_constraint_count =
		pipeline->constraints.constraint_count;
	pipeline->constraints.sealed_operand_count =
		pipeline->constraints.operand_count;
	pipeline->constraints.sealed_classifier_rhs_count =
		pipeline->constraints.classifier_rhs_count;
	pipeline->constraints.sealed_dependent_constraint_count =
		pipeline->constraints.dependent_constraint_count;
	pipeline->constraints.sealed_branch_ih_dependency_total =
		pipeline->constraints.branch_ih_dependency_total;
	pipeline->constraints.sealed_scc_count =
		pipeline->constraints.scc_count;
	pipeline->constraints.sealed_scc_member_total =
		pipeline->constraints.scc_member_total;
	pipeline->constraints.topology_sealed = 1;
	if (topology_digest) {
		*topology_digest = pipeline->constraints.topology_digest;
	}
	return prototype_typing_pipeline_validate_constraint_topology(
		pipeline, NULL, NULL
	);
}

int prototype_typing_pipeline_finish_graph_build(
	struct prototype_typing_pipeline* pipeline
) {
	if (!pipeline || !pipeline->initialized) {
		return -1;
	}
	if (pipeline->topology_revision == UINT64_MAX) {
		return -1;
	}
	++pipeline->topology_revision;
	return 0;
}
void prototype_typing_seed_state_init(
	struct prototype_typing_seed_state* state
) {
	if (!state) {
		return;
	}
	memset(state, 0, sizeof(*state));
	for (uint32_t i = 0;
		i < PROTOTYPE_TYPING_CLASSIFIER_OCCURRENCE_CAPACITY;
		++i) {
		state->classifier_seeds[i] =
			(struct prototype_typing_classifier_seed) {
				.owner_occurrence = i,
				.classifier = PROTOTYPE_INVALID_ID,
				.binder_classifier = PROTOTYPE_INVALID_ID,
				.reason = PROTOTYPE_TYPING_CLASSIFIER_SEED_NONE
			};
	}
}

int prototype_typing_seed_state_mark(
	const struct prototype_typing_seed_state* state,
	struct prototype_typing_seed_mark* mark
) {
	if (!state || !mark) {
		return -1;
	}
	*mark = (struct prototype_typing_seed_mark) {
		.match_resolution_goal_count = state->match_resolution_goal_count,
		.match_goal_count = state->match_goal_count,
		.imported_constructor_goal_count = state->imported_constructor_goal_count,
		.binder_input_count = state->binder_input_count,
		.declaration_input_count = state->declaration_input_count,
		.publication_input_count = state->publication_input_count,
		.termination_witness_input_count =
			state->termination_witness_input_count
	};
	return 0;
}

int prototype_typing_seed_state_truncate(
	struct prototype_typing_seed_state* state,
	const struct prototype_typing_seed_mark* mark
) {
	if (!state || !mark ||
		mark->match_resolution_goal_count > state->match_resolution_goal_count ||
		mark->match_goal_count > state->match_goal_count ||
		mark->imported_constructor_goal_count >
			state->imported_constructor_goal_count ||
		mark->binder_input_count > state->binder_input_count ||
		mark->declaration_input_count > state->declaration_input_count ||
		mark->publication_input_count > state->publication_input_count ||
		mark->termination_witness_input_count >
			state->termination_witness_input_count) {
		return -1;
	}
	state->match_resolution_goal_count = mark->match_resolution_goal_count;
	state->match_goal_count = mark->match_goal_count;
	state->imported_constructor_goal_count =
		mark->imported_constructor_goal_count;
	state->binder_input_count = mark->binder_input_count;
	state->declaration_input_count = mark->declaration_input_count;
	state->publication_input_count = mark->publication_input_count;
	state->termination_witness_input_count =
		mark->termination_witness_input_count;
	return 0;
}

uint64_t prototype_typing_seed_state_digest(
	const struct prototype_typing_seed_state* state
) {
	if (!state ||
		state->match_resolution_goal_count >
			PROTOTYPE_TYPING_MATCH_RESOLUTION_GOAL_CAPACITY ||
		state->match_goal_count > PROTOTYPE_TYPING_MATCH_GOAL_CAPACITY ||
		state->imported_constructor_goal_count >
			PROTOTYPE_TYPING_IMPORTED_CONSTRUCTOR_GOAL_CAPACITY ||
		state->binder_input_count > PROTOTYPE_TYPING_BINDER_INPUT_CAPACITY ||
		state->declaration_input_count >
			PROTOTYPE_TYPING_DECLARATION_INPUT_CAPACITY ||
		state->publication_input_count >
			PROTOTYPE_TYPING_PUBLICATION_INPUT_CAPACITY ||
		state->termination_witness_input_count >
			PROTOTYPE_TYPING_TERMINATION_WITNESS_INPUT_CAPACITY) {
		return 0;
	}
	uint64_t hash = UINT64_C(1469598103934665603);
#define SEED_MIX(value) typing_constraint_topology_mix(&hash, (uint64_t)(value))
	for (uint32_t i = 0;
		i < PROTOTYPE_TYPING_CLASSIFIER_OCCURRENCE_CAPACITY;
		++i) {
		SEED_MIX(state->classifier_seeds[i].owner_occurrence);
		SEED_MIX(state->classifier_seeds[i].classifier);
		SEED_MIX(state->classifier_seeds[i].binder_classifier);
		SEED_MIX((uint32_t)state->classifier_seeds[i].reason);
	}
	SEED_MIX(state->match_resolution_goal_count);
	for (uint32_t i = 0; i < state->match_resolution_goal_count; ++i) {
		const struct prototype_typing_match_resolution_goal* goal =
			&state->match_resolution_goals[i];
		SEED_MIX(goal->match_term);
		SEED_MIX(goal->match_operation);
		SEED_MIX(goal->scrutinee_operation);
		SEED_MIX(goal->case_index);
		SEED_MIX(goal->scrutinee_term);
		SEED_MIX((uint32_t)goal->constructor_symbol_id);
	}
	SEED_MIX(state->match_goal_count);
	for (uint32_t i = 0; i < state->match_goal_count; ++i) {
		SEED_MIX(state->match_goals[i].match_term);
		SEED_MIX(state->match_goals[i].operation);
		SEED_MIX(state->match_goals[i].universe_level_var);
	}
	SEED_MIX(state->imported_constructor_goal_count);
	for (uint32_t i = 0; i < state->imported_constructor_goal_count; ++i) {
		const struct prototype_typing_imported_constructor_goal* goal =
			&state->imported_constructor_goals[i];
		SEED_MIX(goal->constructor_term);
		SEED_MIX(goal->owner);
		SEED_MIX(goal->imported_interface_index);
		SEED_MIX(goal->type_export_id);
		SEED_MIX(goal->constructor_export_id);
	}
	SEED_MIX(state->binder_input_count);
	for (uint32_t i = 0; i < state->binder_input_count; ++i) {
		const struct prototype_typing_binder_input* input =
			&state->binder_inputs[i];
		SEED_MIX(input->context_id);
		SEED_MIX(input->binder_var);
		SEED_MIX(input->classifier);
		SEED_MIX(input->source_operation);
		SEED_MIX(input->classifier_rhs_root);
	}
	SEED_MIX(state->declaration_input_count);
	for (uint32_t i = 0; i < state->declaration_input_count; ++i) {
		SEED_MIX(state->declaration_inputs[i].context_id);
		SEED_MIX(state->declaration_inputs[i].subject);
		SEED_MIX(state->declaration_inputs[i].classifier);
	}
	SEED_MIX(state->publication_input_count);
	for (uint32_t i = 0; i < state->publication_input_count; ++i) {
		SEED_MIX((uint32_t)state->publication_inputs[i].name_symbol_id);
		SEED_MIX(state->publication_inputs[i].exposed_occurrence);
		SEED_MIX(state->publication_inputs[i].expectation_occurrence);
		SEED_MIX(state->publication_inputs[i].expectation_classifier);
	}
	SEED_MIX(state->termination_witness_input_count);
	for (uint32_t i = 0; i < state->termination_witness_input_count; ++i) {
		SEED_MIX(state->termination_witness_inputs[i]);
	}
#undef SEED_MIX
	return hash;
}
