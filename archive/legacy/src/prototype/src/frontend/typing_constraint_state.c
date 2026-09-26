#include "a_program/frontend/typing_constraint_state.h"

#include "a_program/kernel/judgement/rules.h"
#include "a_program/support/schema.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void typing_classifier_rhs_hash_mix(uint64_t* hash, uint64_t value) {
	*hash ^= value;
	*hash *= UINT64_C(1099511628211);
}

static uint64_t typing_classifier_rhs_hash(
	int kind,
	uint32_t source_term,
	const struct prototype_typing_equation_operand* operands,
	uint32_t operand_count
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	typing_classifier_rhs_hash_mix(&hash, (uint32_t)kind);
	typing_classifier_rhs_hash_mix(&hash, source_term);
	typing_classifier_rhs_hash_mix(&hash, operand_count);
	for (uint32_t i = 0; i < operand_count; ++i) {
		typing_classifier_rhs_hash_mix(&hash, (uint32_t)operands[i].role);
		typing_classifier_rhs_hash_mix(&hash, (uint32_t)operands[i].target_kind);
		typing_classifier_rhs_hash_mix(&hash, operands[i].target);
		typing_classifier_rhs_hash_mix(&hash, operands[i].qualifier);
	}
	return hash;
}

static int typing_classifier_rhs_equal(
	const struct prototype_typing_constraint_db* db,
	const struct prototype_typing_classifier_rhs* existing,
	int kind,
	uint32_t source_term,
	const struct prototype_typing_equation_operand* operands,
	uint32_t operand_count
) {
	if (!db || !existing || existing->kind != kind ||
		existing->source_term != source_term ||
		existing->operand_count != operand_count) {
		return 0;
	}
	for (uint32_t i = 0; i < operand_count; ++i) {
		const struct prototype_typing_equation_operand* edge =
			prototype_typing_classifier_rhs_operand_get(db, existing, i);
		if (!edge || memcmp(edge, &operands[i], sizeof(*edge)) != 0) {
			return 0;
		}
	}
	return 1;
}

static int typing_constraint_payload_key(
	const struct prototype_typing_constraint* constraint,
	uint32_t values[10],
	uint32_t* p_count
) {
	if (!constraint || !values || !p_count) {
		return -1;
	}
	uint32_t count = 0;
#define ADD_VALUE(value) values[count++] = (uint32_t)(value)
	switch (constraint->domain) {
	case OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER:
		switch (constraint->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE:
		case OPERATION_CONSTRAINT_TERMINATION_WITNESS:
			break;
		case OPERATION_CONSTRAINT_EQUAL:
			ADD_VALUE(constraint->payload.classifier.goal.reference.referenced_operation);
			break;
		case OPERATION_CONSTRAINT_IMPORTED_REFERENCE:
			ADD_VALUE(constraint->payload.classifier.goal.imported_reference.classifier_core_term);
			ADD_VALUE(constraint->payload.classifier.goal.imported_reference.import_kind);
			ADD_VALUE(constraint->payload.classifier.goal.imported_reference.import_id);
			break;
		case OPERATION_CONSTRAINT_INTRINSIC_REFERENCE:
			ADD_VALUE(constraint->payload.classifier.goal.intrinsic_reference.target_kind);
			ADD_VALUE(constraint->payload.classifier.goal.intrinsic_reference.target_id);
			ADD_VALUE(constraint->payload.classifier.goal.intrinsic_reference.type_symbol_id);
			break;
		case OPERATION_CONSTRAINT_CONVERTIBLE:
			ADD_VALUE(constraint->payload.classifier.goal.conversion.body_occurrence);
			ADD_VALUE(constraint->payload.classifier.goal.conversion.expected_classifier);
			break;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			ADD_VALUE(constraint->payload.classifier.goal.pi.role);
			ADD_VALUE(constraint->payload.classifier.goal.pi.body_or_function_operation);
			ADD_VALUE(constraint->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence);
			break;
		case OPERATION_CONSTRAINT_PI_DOMAIN_RELATION:
			ADD_VALUE(constraint->payload.classifier.goal.pi_domain_relation.
				application_typed_projection);
			ADD_VALUE(constraint->payload.classifier.goal.pi_domain_relation.
				function_typed_projection);
			break;
		case OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION:
			ADD_VALUE(constraint->payload.classifier.goal.
				sequence_binder_relation.fold_typed_projection);
			ADD_VALUE(constraint->payload.classifier.goal.
				sequence_binder_relation.producer_typed_projection);
			break;
		case OPERATION_CONSTRAINT_PROJECTED_CLASSIFIER:
			break;
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION: {
			const struct prototype_typing_classifier_motive_case_goal* goal =
				&constraint->payload.classifier.goal.motive_case;
			ADD_VALUE(goal->branch_operation);
			ADD_VALUE(goal->scrutinee_operation);
			ADD_VALUE(goal->branch_typed_projection);
			ADD_VALUE(goal->scrutinee_typed_projection);
			ADD_VALUE(goal->match_case_projection);
			ADD_VALUE(goal->case_index);
			ADD_VALUE(goal->constructor_owner);
			ADD_VALUE(goal->constructor_index);
			ADD_VALUE(goal->ih_scope_id);
			break;
		}
		case OPERATION_CONSTRAINT_IH_EXPECTED:
			ADD_VALUE(constraint->payload.classifier.goal.induction_hypothesis.owner_match_occurrence);
			ADD_VALUE(constraint->payload.classifier.goal.induction_hypothesis.recursive_argument_occurrence);
			ADD_VALUE(constraint->payload.classifier.goal.induction_hypothesis.recursive_argument_binding_id);
			ADD_VALUE(constraint->payload.classifier.goal.induction_hypothesis.ih_scope_id);
			ADD_VALUE(constraint->payload.classifier.goal.induction_hypothesis.case_index);
			ADD_VALUE(constraint->payload.classifier.goal.induction_hypothesis.field_index);
			break;
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
			ADD_VALUE(constraint->payload.classifier.goal.cbpv_boundary.child_operation);
			break;
		case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			ADD_VALUE(constraint->payload.classifier.goal.computation_fold.computation_typed_projection);
			ADD_VALUE(constraint->payload.classifier.goal.computation_fold.return_body_typed_projection);
			ADD_VALUE(constraint->payload.classifier.goal.computation_fold.fold_topology_id);
			break;
		case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
			ADD_VALUE(constraint->payload.classifier.goal.operation_request.operation_typed_projection);
			ADD_VALUE(constraint->payload.classifier.goal.operation_request.argument_typed_projection);
			ADD_VALUE(constraint->payload.classifier.goal.operation_request.continuation_typed_projection);
			break;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			ADD_VALUE(constraint->payload.classifier.goal.constructor_formation.function_operation);
			ADD_VALUE(constraint->payload.classifier.goal.constructor_formation.argument_occurrence);
			break;
		case OPERATION_CONSTRAINT_BINDER_TYPE:
			ADD_VALUE(constraint->payload.classifier.goal.binder_type.binding_context);
			ADD_VALUE(constraint->payload.classifier.goal.binder_type.binding_id);
			break;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_HEAD:
			ADD_VALUE(constraint->payload.classifier.goal.constructor_head.constructor_id);
			ADD_VALUE(constraint->payload.classifier.goal.constructor_head.owner_typed_projection);
			break;
		default:
			return -1;
		}
		break;
	case OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT:
		ADD_VALUE(constraint->payload.branch_refinement.match_occurrence);
		ADD_VALUE(constraint->payload.branch_refinement.scrutinee_occurrence);
		ADD_VALUE(constraint->payload.branch_refinement.case_index);
		ADD_VALUE(constraint->payload.branch_refinement.source_context);
		ADD_VALUE(constraint->payload.branch_refinement.match_typed_projection);
		ADD_VALUE(constraint->payload.branch_refinement.scrutinee_typed_projection);
		ADD_VALUE(constraint->payload.branch_refinement.branch_typed_projection);
		ADD_VALUE(constraint->payload.branch_refinement.match_case_projection);
		break;
	case OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW:
		ADD_VALUE(constraint->payload.effect.result_row);
		ADD_VALUE(constraint->payload.effect.left_row);
		ADD_VALUE(constraint->payload.effect.right_row);
		break;
	case OPERATION_CONSTRAINT_DOMAIN_COMPUTATION:
		ADD_VALUE(constraint->payload.computation.judgement_delta_constraint_id);
		break;
	case PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_USAGE:
		ADD_VALUE(constraint->payload.usage.first_operand);
		ADD_VALUE(constraint->payload.usage.second_operand);
		ADD_VALUE(constraint->payload.usage.binding_id);
		break;
	case PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_TOTALITY:
		ADD_VALUE(constraint->payload.totality.first_operand);
		ADD_VALUE(constraint->payload.totality.second_operand);
		ADD_VALUE(constraint->payload.totality.constant);
		break;
	default:
		return -1;
	}
#undef ADD_VALUE
	*p_count = count;
	return 0;
}

static uint64_t typing_constraint_hash(
	const struct prototype_typing_constraint* constraint,
	const struct prototype_typing_equation_operand* operands,
	uint32_t operand_count,
	const uint32_t* payload,
	uint32_t payload_count
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	typing_classifier_rhs_hash_mix(&hash, (uint32_t)constraint->domain);
	typing_classifier_rhs_hash_mix(&hash, (uint32_t)constraint->kind);
	typing_classifier_rhs_hash_mix(&hash, constraint->owner_typed_projection);
	typing_classifier_rhs_hash_mix(&hash, constraint->source_occurrence);
	typing_classifier_rhs_hash_mix(&hash, constraint->origin_constraint_id);
	typing_classifier_rhs_hash_mix(&hash, constraint->classifier_rhs_root);
	typing_classifier_rhs_hash_mix(&hash, operand_count);
	for (uint32_t i = 0; i < operand_count; ++i) {
		typing_classifier_rhs_hash_mix(&hash, (uint32_t)operands[i].role);
		typing_classifier_rhs_hash_mix(&hash, (uint32_t)operands[i].target_kind);
		typing_classifier_rhs_hash_mix(&hash, operands[i].target);
		typing_classifier_rhs_hash_mix(&hash, operands[i].qualifier);
	}
	for (uint32_t i = 0; i < payload_count; ++i) {
		typing_classifier_rhs_hash_mix(&hash, payload[i]);
	}
	return hash;
}

static int typing_constraint_equal(
	const struct prototype_typing_constraint_db* db,
	const struct prototype_typing_constraint* existing,
	const struct prototype_typing_constraint* candidate,
	const struct prototype_typing_equation_operand* operands,
	uint32_t operand_count,
	const uint32_t* payload,
	uint32_t payload_count
) {
	if (!db || !existing || !candidate || existing->domain != candidate->domain ||
		existing->kind != candidate->kind ||
		existing->owner_typed_projection != candidate->owner_typed_projection ||
		existing->source_occurrence != candidate->source_occurrence ||
		existing->origin_constraint_id != candidate->origin_constraint_id ||
		existing->classifier_rhs_root != candidate->classifier_rhs_root ||
		existing->operand_count != operand_count) {
		return 0;
	}
	for (uint32_t i = 0; i < operand_count; ++i) {
		const struct prototype_typing_equation_operand* edge =
			prototype_typing_constraint_operand_get(db, existing, i);
		if (!edge || memcmp(edge, &operands[i], sizeof(*edge)) != 0) {
			return 0;
		}
	}
	uint32_t existing_payload[10];
	uint32_t existing_count;
	return typing_constraint_payload_key(
			existing, existing_payload, &existing_count
		) == 0 && existing_count == payload_count &&
		memcmp(existing_payload, payload, payload_count * sizeof(*payload)) == 0;
}

static int typing_constraint_rebuild_hash(
	struct prototype_typing_constraint_db* db
) {
	if (!db) {
		return -1;
	}
	for (uint32_t i = 0; i < PROTOTYPE_TYPING_CONSTRAINT_HASH_CAPACITY; ++i) {
		db->constraint_hash_slots[i] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t id = 0; id < db->constraint_count; ++id) {
		struct prototype_typing_constraint* constraint = &db->constraints[id];
		uint32_t payload[10];
		uint32_t payload_count;
		if (typing_constraint_payload_key(
				constraint, payload, &payload_count
			) != 0 || (constraint->operand_count != 0 &&
			 (constraint->first_operand == PROTOTYPE_INVALID_ID ||
			  constraint->first_operand > db->operand_count ||
			  constraint->operand_count >
				db->operand_count - constraint->first_operand))) {
			return -1;
		}
		const struct prototype_typing_equation_operand* operands =
			constraint->operand_count != 0 ?
				&db->operands[constraint->first_operand] : NULL;
		constraint->key_hash = typing_constraint_hash(
			constraint, operands, constraint->operand_count,
			payload, payload_count
		);
		uint32_t slot = (uint32_t)(constraint->key_hash %
			PROTOTYPE_TYPING_CONSTRAINT_HASH_CAPACITY);
		constraint->hash_next = db->constraint_hash_slots[slot];
		db->constraint_hash_slots[slot] = id;
	}
	return 0;
}

static int typing_classifier_rhs_rebuild_hash(
	struct prototype_typing_constraint_db* db
) {
	if (!db) {
		return -1;
	}
	for (uint32_t i = 0;
		i < PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY;
		++i) {
		db->classifier_rhs_hash_slots[i] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t rhs_id = 0; rhs_id < db->classifier_rhs_count; ++rhs_id) {
		const struct prototype_typing_classifier_rhs* rhs =
			&db->classifier_rhs[rhs_id];
		if ((rhs->operand_count == 0 &&
			 rhs->first_operand != PROTOTYPE_INVALID_ID) ||
			(rhs->operand_count != 0 &&
			 (rhs->first_operand == PROTOTYPE_INVALID_ID ||
			  rhs->first_operand > db->operand_count ||
			  rhs->operand_count > db->operand_count - rhs->first_operand))) {
			return -1;
		}
		const struct prototype_typing_equation_operand* operands =
			rhs->operand_count != 0 ? &db->operands[rhs->first_operand] : NULL;
		uint32_t slot = (uint32_t)(typing_classifier_rhs_hash(
			rhs->kind, rhs->source_term, operands, rhs->operand_count
		) % PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY);
		for (uint32_t probe = 0;
			probe < PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY;
			++probe) {
			if (db->classifier_rhs_hash_slots[slot] == PROTOTYPE_INVALID_ID) {
				db->classifier_rhs_hash_slots[slot] = rhs_id;
				break;
			}
			slot = (slot + 1) % PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY;
			if (probe + 1 == PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY) {
				return -1;
			}
		}
	}
	return 0;
}

void prototype_typing_constraint_solution_initialize(
	struct prototype_typing_constraint_solution* solution
) {
	if (!solution) return;
	memset(solution, 0, sizeof(*solution));
	solution->state = OPERATION_CONSTRAINT_STATE_PENDING;
	solution->reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE;
	solution->result_term = PROTOTYPE_INVALID_ID;
	solution->source_classifier_candidate = PROTOTYPE_INVALID_ID;
	solution->answer_state = PROTOTYPE_TYPING_EQUATION_ANSWER_UNSOLVED;
	solution->answer_pending_operand_fingerprint = 0;
	solution->projected_binder_classifier = PROTOTYPE_INVALID_ID;
	solution->producer_constraint_id = PROTOTYPE_INVALID_ID;
	solution->evidence_id = PROTOTYPE_INVALID_ID;
	solution->projected_motive = PROTOTYPE_INVALID_ID;
	solution->conversion_goal.id = PROTOTYPE_INVALID_ID;
	solution->conversion_goal.context_id = PROTOTYPE_INVALID_ID;
	solution->conversion_goal.carrier_classifier = PROTOTYPE_INVALID_ID;
	solution->conversion_goal.left_term = PROTOTYPE_INVALID_ID;
	solution->conversion_goal.right_term = PROTOTYPE_INVALID_ID;
	solution->validated_motive = PROTOTYPE_INVALID_ID;
	solution->validated_actual_classifier = PROTOTYPE_INVALID_ID;
	solution->validated_context = PROTOTYPE_INVALID_ID;
	solution->validated_substitution = PROTOTYPE_INVALID_ID;
	solution->contradiction_expected_classifier = PROTOTYPE_INVALID_ID;
	solution->contradiction_actual_classifier = PROTOTYPE_INVALID_ID;
	solution->branch_refinement.materialized_source_context = PROTOTYPE_INVALID_ID;
	solution->branch_refinement.refined_context = PROTOTYPE_INVALID_ID;
	solution->branch_refinement.substitution = PROTOTYPE_INVALID_ID;
	solution->branch_refinement.constructor_term = PROTOTYPE_INVALID_ID;
	solution->branch_refinement.residual_pattern = PROTOTYPE_INVALID_ID;
	solution->branch_refinement.residual_value = PROTOTYPE_INVALID_ID;
	solution->branch_refinement.refinement_status =
		PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
}

int prototype_typing_constraint_db_init(
	struct prototype_typing_constraint_db* db
) {
	if (!db) {
		return -1;
	}
	memset(db, 0, sizeof(*db));
	for (uint32_t i = 0;
		i < PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY;
		++i) {
		db->classifier_rhs_hash_slots[i] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0;
		i < PROTOTYPE_TYPING_CONSTRAINT_HASH_CAPACITY;
		++i) {
		db->constraint_hash_slots[i] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t domain = 0;
		domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT;
		++domain) {
		db->first_constraint_for_domain[domain] = PROTOTYPE_INVALID_ID;
		db->last_constraint_for_domain[domain] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0;
		i < PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY;
		++i) {
		db->first_dependent_constraint[i] = PROTOTYPE_INVALID_ID;
		db->first_classifier_premise_for_projection[i] =
			PROTOTYPE_INVALID_ID;
		db->last_classifier_premise_for_projection[i] =
			PROTOTYPE_INVALID_ID;
		db->classifier_constraint_for_projection[i] =
			PROTOTYPE_INVALID_ID;
		db->conversion_constraint_for_projection[i] =
			PROTOTYPE_INVALID_ID;
		db->motive_constraint_for_match_case_projection[i] =
			PROTOTYPE_INVALID_ID;
		db->ih_constraint_for_projection[i] = PROTOTYPE_INVALID_ID;
		db->pi_intro_constraint_for_projection[i] = PROTOTYPE_INVALID_ID;
		db->branch_refinement_constraint_for_match_case_projection[i] =
			PROTOTYPE_INVALID_ID;
		db->motive_solution_for_projection[i] =
			(struct prototype_typing_motive_solution) {
				.constant_candidate = PROTOTYPE_INVALID_ID,
				.motive = PROTOTYPE_INVALID_ID,
				.expected_equation_classifier = PROTOTYPE_INVALID_ID,
				.expected_equation_constraint_id = PROTOTYPE_INVALID_ID,
				.expected_equation_context_id = PROTOTYPE_INVALID_ID,
				.source_case_index = PROTOTYPE_INVALID_ID,
				.source_classifier = PROTOTYPE_INVALID_ID,
				.status = OPERATION_MOTIVE_STATUS_UNSOLVED,
				.phase = OPERATION_MOTIVE_PHASE_NONE
			};
		db->computation_constraint_for_occurrence[i] = PROTOTYPE_INVALID_ID;
		db->first_effect_constraint_for_occurrence[i] = PROTOTYPE_INVALID_ID;
		db->effect_solution_constraint_for_projection[i] =
			PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < PROTOTYPE_CONTEXT_CAPACITY; ++i) {
		db->sequence_relation_for_context_equation[i] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < PROTOTYPE_TYPING_CONSTRAINT_CAPACITY; ++i) {
		prototype_typing_constraint_solution_initialize(&db->solutions[i]);
		db->next_constraint_in_domain[i] = PROTOTYPE_INVALID_ID;
		db->next_classifier_premise[i] = PROTOTYPE_INVALID_ID;
		db->first_branch_ih_dependency[i] = PROTOTYPE_INVALID_ID;
		db->first_equation_dependent_constraint[i] = PROTOTYPE_INVALID_ID;
		db->next_effect_constraint[i] = PROTOTYPE_INVALID_ID;
		db->scc_for_constraint[i] = PROTOTYPE_INVALID_ID;
		db->first_scc_member[i] = PROTOTYPE_INVALID_ID;
	}
	return 0;
}

uint32_t prototype_typing_constraint_first_in_domain(
	const struct prototype_typing_constraint_db* db,
	int domain
) {
	return db && domain >= OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER &&
		domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT ?
		db->first_constraint_for_domain[domain] : PROTOTYPE_INVALID_ID;
}

uint32_t prototype_typing_constraint_next_in_domain(
	const struct prototype_typing_constraint_db* db,
	uint32_t constraint_id
) {
	return db && constraint_id < db->constraint_count ?
		db->next_constraint_in_domain[constraint_id] : PROTOTYPE_INVALID_ID;
}

uint32_t prototype_typing_constraint_count_in_domain(
	const struct prototype_typing_constraint_db* db,
	int domain
) {
	return db && domain >= OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER &&
		domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT ?
		db->constraint_count_by_domain[domain] : 0;
}

int prototype_typing_constraint_has_domain(
	const struct prototype_typing_constraint_db* db,
	uint32_t constraint_id,
	int domain
) {
	return db && constraint_id < db->constraint_count &&
		db->constraints[constraint_id].domain == domain;
}

static int typing_constraint_rebuild_domain_index(
	struct prototype_typing_constraint_db* db
) {
	if (!db) {
		return -1;
	}
	for (uint32_t domain = 0;
		domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT;
		++domain) {
		db->first_constraint_for_domain[domain] = PROTOTYPE_INVALID_ID;
		db->last_constraint_for_domain[domain] = PROTOTYPE_INVALID_ID;
		db->constraint_count_by_domain[domain] = 0;
	}
	for (uint32_t id = 0; id < db->constraint_count; ++id) {
		int domain = db->constraints[id].domain;
		if (domain < OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER ||
			domain >= PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT) {
			return -1;
		}
		db->next_constraint_in_domain[id] = PROTOTYPE_INVALID_ID;
		uint32_t last = db->last_constraint_for_domain[domain];
		if (last == PROTOTYPE_INVALID_ID) {
			db->first_constraint_for_domain[domain] = id;
		} else {
			db->next_constraint_in_domain[last] = id;
		}
		db->last_constraint_for_domain[domain] = id;
		db->constraint_count_by_domain[domain]++;
	}
	return 0;
}

int prototype_typing_constraint_transition(
	struct prototype_typing_constraint_db* db,
	uint32_t constraint_id,
	int state,
	int reason
) {
	if (!db || constraint_id >= db->constraint_count ||
		state < OPERATION_CONSTRAINT_STATE_PENDING ||
		state > OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
		return -1;
	}
	struct prototype_typing_constraint_solution* solution =
		&db->solutions[constraint_id];
	if (solution->state == state && solution->reason == reason) {
		if (state != OPERATION_CONSTRAINT_STATE_PENDING ||
			solution->evidence_id == PROTOTYPE_INVALID_ID) {
			return 0;
		}
	}
	int semantic_terminal =
		state == OPERATION_CONSTRAINT_STATE_SOLVED ||
		state == OPERATION_CONSTRAINT_STATE_RESIDUAL ||
		state == OPERATION_CONSTRAINT_STATE_CONTRADICTION;
	if (semantic_terminal &&
		prototype_typing_constraint_record_semantic_transition(
			db, db->constraints[constraint_id].domain
		) != 0) {
		return -1;
	}
	solution->state = state;
	solution->reason = reason;
	if (state == OPERATION_CONSTRAINT_STATE_PENDING) {
		solution->evidence_id = PROTOTYPE_INVALID_ID;
		solution->contradiction_expected_classifier = PROTOTYPE_INVALID_ID;
		solution->contradiction_actual_classifier = PROTOTYPE_INVALID_ID;
	}
	return 0;
}

int prototype_typing_constraint_record_semantic_transition(
	struct prototype_typing_constraint_db* db,
	int domain
) {
	if (!db || domain <= 0 ||
		domain >= PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT ||
		db->semantic_transition_count == UINT64_MAX ||
		db->semantic_transition_count_by_domain[domain] == UINT64_MAX) {
		return -1;
	}
	db->semantic_transition_count++;
	db->semantic_transition_count_by_domain[domain]++;
	return 0;
}

int prototype_typing_constraint_mark_input_changed(
	struct prototype_typing_constraint_db* db,
	uint32_t constraint_id
) {
	if (!db || constraint_id >= db->constraint_count ||
		db->solutions[constraint_id].input_revision == UINT64_MAX) {
		return -1;
	}
	db->solutions[constraint_id].input_revision++;
	return 0;
}

struct prototype_typing_constraint_db_mark
prototype_typing_constraint_db_mark(
	const struct prototype_typing_constraint_db* db
) {
	if (!db) {
		return (struct prototype_typing_constraint_db_mark) { 0 };
	}
	return (struct prototype_typing_constraint_db_mark) {
		.operand_count = db->operand_count,
		.classifier_rhs_count = db->classifier_rhs_count,
		.constraint_count = db->constraint_count,
		.dependent_constraint_count = db->dependent_constraint_count,
		.branch_ih_dependency_total = db->branch_ih_dependency_total,
		.worklist_head = db->worklist_head,
		.worklist_count = db->worklist_count,
		.semantic_transition_count = db->semantic_transition_count,
		.semantic_transition_count_by_domain = {
			db->semantic_transition_count_by_domain[0],
			db->semantic_transition_count_by_domain[1],
			db->semantic_transition_count_by_domain[2],
			db->semantic_transition_count_by_domain[3],
			db->semantic_transition_count_by_domain[4],
			db->semantic_transition_count_by_domain[5],
			db->semantic_transition_count_by_domain[6]
		}
	};
}

int prototype_typing_constraint_db_rollback(
	struct prototype_typing_constraint_db* db,
	struct prototype_typing_constraint_db_mark mark
) {
	if (!db || db->topology_sealed ||
		mark.operand_count > db->operand_count ||
		mark.classifier_rhs_count > db->classifier_rhs_count ||
		mark.constraint_count > db->constraint_count ||
		mark.dependent_constraint_count > db->dependent_constraint_count) {
		return -1;
	}
	for (uint32_t i = db->constraint_count; i > mark.constraint_count; --i) {
		uint32_t constraint_id = i - 1;
		const struct prototype_typing_constraint* constraint =
			&db->constraints[constraint_id];
		if (constraint->domain == OPERATION_CONSTRAINT_DOMAIN_COMPUTATION &&
			constraint->source_occurrence <
				PROTOTYPE_TYPING_CONSTRAINT_OCCURRENCE_CAPACITY &&
			db->computation_constraint_for_occurrence[
				constraint->source_occurrence
			] == constraint_id) {
			db->computation_constraint_for_occurrence[
				constraint->source_occurrence
			] = PROTOTYPE_INVALID_ID;
		}
		if (constraint->domain == OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW &&
			constraint->source_occurrence <
				PROTOTYPE_TYPING_CONSTRAINT_OCCURRENCE_CAPACITY &&
			db->first_effect_constraint_for_occurrence[
				constraint->source_occurrence
			] == constraint_id) {
			db->first_effect_constraint_for_occurrence[
				constraint->source_occurrence
			] = db->next_effect_constraint[constraint_id];
		}
		if (constraint->domain == OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW &&
			constraint->kind == PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION &&
			constraint->owner_typed_projection <
				PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY &&
			db->effect_solution_constraint_for_projection[
				constraint->owner_typed_projection
			] == constraint_id) {
			db->effect_solution_constraint_for_projection[
				constraint->owner_typed_projection
			] = PROTOTYPE_INVALID_ID;
		}
	}
	for (uint32_t i = db->dependent_constraint_count;
		i > mark.dependent_constraint_count;
		--i) {
		uint32_t dependency_id = i - 1;
		const struct prototype_typing_dependent_constraint* dependency =
			&db->dependent_constraints[dependency_id];
		uint32_t* first = NULL;
		if (dependency->source_kind ==
				PROTOTYPE_TYPING_DEPENDENCY_SOURCE_PROJECTION &&
			dependency->source < PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
			first = &db->first_dependent_constraint[dependency->source];
		} else if (dependency->source_kind ==
				PROTOTYPE_TYPING_DEPENDENCY_SOURCE_EQUATION &&
			dependency->source < PROTOTYPE_TYPING_CONSTRAINT_CAPACITY) {
			first = &db->first_equation_dependent_constraint[dependency->source];
		}
		if (!first || *first != dependency_id) {
			return -1;
		}
		*first = dependency->next;
	}
	memset(
		&db->classifier_rhs[mark.classifier_rhs_count],
		0,
		(db->classifier_rhs_count - mark.classifier_rhs_count) *
			sizeof(*db->classifier_rhs)
	);
	db->classifier_rhs_count = mark.classifier_rhs_count;
	memset(
		&db->operands[mark.operand_count],
		0,
		(db->operand_count - mark.operand_count) * sizeof(*db->operands)
	);
	db->operand_count = mark.operand_count;
	memset(
		&db->constraints[mark.constraint_count],
		0,
		(db->constraint_count - mark.constraint_count) *
			sizeof(*db->constraints)
	);
	memset(
		&db->solutions[mark.constraint_count],
		0,
		(db->constraint_count - mark.constraint_count) *
			sizeof(*db->solutions)
	);
	db->constraint_count = mark.constraint_count;
	db->dependent_constraint_count = mark.dependent_constraint_count;
	db->branch_ih_dependency_total = mark.branch_ih_dependency_total;
	db->scc_count = 0;
	db->scc_member_total = 0;
	for (uint32_t i = 0; i < PROTOTYPE_TYPING_CONSTRAINT_CAPACITY; ++i) {
		db->scc_for_constraint[i] = PROTOTYPE_INVALID_ID;
		db->first_scc_member[i] = PROTOTYPE_INVALID_ID;
		db->scc_member_count[i] = 0;
		db->scc_recursive[i] = 0;
	}
	db->worklist_head = mark.worklist_head;
	db->worklist_count = mark.worklist_count;
	db->semantic_transition_count = mark.semantic_transition_count;
	memcpy(
		db->semantic_transition_count_by_domain,
		mark.semantic_transition_count_by_domain,
		sizeof(db->semantic_transition_count_by_domain)
	);
	return typing_constraint_rebuild_domain_index(db) == 0 &&
		typing_classifier_rhs_rebuild_hash(db) == 0 ?
		typing_constraint_rebuild_hash(db) : -1;
}

int prototype_typing_constraint_enqueue(
	struct prototype_typing_constraint_db* db,
	uint32_t constraint_id,
	int* p_enqueued
) {
	if (!db || !p_enqueued || constraint_id >= db->constraint_count) {
		return -1;
	}
	if (db->constraint_queued[constraint_id]) {
		*p_enqueued = 0;
		return 0;
	}
	if (db->worklist_count >= PROTOTYPE_TYPING_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t tail = (
		db->worklist_head + db->worklist_count
	) % PROTOTYPE_TYPING_CONSTRAINT_CAPACITY;
	db->worklist[tail] = constraint_id;
	db->constraint_queued[constraint_id] = 1;
	db->worklist_count++;
	*p_enqueued = 1;
	return 0;
}

int prototype_typing_constraint_pop(
	struct prototype_typing_constraint_db* db,
	uint32_t* p_constraint_id
) {
	if (!db || !p_constraint_id) {
		return -1;
	}
	if (db->worklist_count == 0) {
		return 1;
	}
	uint32_t constraint_id = db->worklist[db->worklist_head];
	if (constraint_id >= db->constraint_count ||
		!db->constraint_queued[constraint_id]) {
		return -1;
	}
	db->worklist_head = (db->worklist_head + 1) %
		PROTOTYPE_TYPING_CONSTRAINT_CAPACITY;
	db->worklist_count--;
	db->constraint_queued[constraint_id] = 0;
	*p_constraint_id = constraint_id;
	return 0;
}

int prototype_typing_constraint_intern(
	struct prototype_typing_constraint_db* db,
	const struct prototype_typing_constraint* constraint,
	const struct prototype_typing_equation_operand* operands,
	uint32_t operand_count,
	uint32_t* constraint_id
) {
	if (!db || !constraint || !constraint_id ||
		(operand_count != 0 && !operands)) {
		return -1;
	}
	for (uint32_t i = 0; i < operand_count; ++i) {
		if (operands[i].target_kind <
				PROTOTYPE_TYPING_EQUATION_OPERAND_TYPED_PROJECTION ||
			operands[i].target_kind >
				PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE ||
			operands[i].target == PROTOTYPE_INVALID_ID) {
			fprintf(stderr, "equation operand is invalid index=%u\n", i);
			return -1;
		}
	}
	if (constraint->domain < OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER ||
		constraint->domain > PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_TOTALITY) {
		fprintf(stderr, "constraint domain invalid domain=%d\n", constraint->domain);
		return -1;
	}
	if (constraint->owner_typed_projection == PROTOTYPE_INVALID_ID) {
		fprintf(stderr, "constraint owner projection is missing\n");
		return -1;
	}
	uint32_t payload[10];
	uint32_t payload_count;
	if (typing_constraint_payload_key(
			constraint, payload, &payload_count
		) != 0) {
		fprintf(stderr, "constraint payload is invalid\n");
		return -1;
	}
	uint64_t key_hash = typing_constraint_hash(
		constraint, operands, operand_count, payload, payload_count
	);
	db->constraint_intern_requests++;
	uint32_t slot = (uint32_t)(key_hash %
		PROTOTYPE_TYPING_CONSTRAINT_HASH_CAPACITY);
	for (uint32_t id = db->constraint_hash_slots[slot];
		id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->constraint_count) {
			return -1;
		}
		const struct prototype_typing_constraint* existing =
			&db->constraints[id];
		db->constraint_intern_probes++;
		if (existing->key_hash == key_hash && typing_constraint_equal(
				db, existing, constraint, operands, operand_count,
				payload, payload_count
			)) {
			db->constraint_intern_hits++;
			*constraint_id = id;
			return 0;
		}
		id = existing->hash_next;
	}
	if (db->topology_sealed) {
		fprintf(stderr, "new constraint rejected after topology seal\n");
		return -1;
	}
	if (db->constraint_count >= PROTOTYPE_TYPING_CONSTRAINT_CAPACITY) {
		fprintf(stderr, "constraint capacity exhausted\n");
		return -1;
	}
	if (operand_count > PROTOTYPE_TYPING_EQUATION_OPERAND_CAPACITY -
			db->operand_count) {
		fprintf(stderr, "equation operand capacity exhausted\n");
		return -1;
	}
	uint32_t id = db->constraint_count++;
	db->constraints[id] = *constraint;
	db->constraints[id].id = id;
	db->constraints[id].key_hash = key_hash;
	db->constraints[id].hash_next = db->constraint_hash_slots[slot];
	db->constraints[id].first_operand = operand_count != 0 ?
		db->operand_count : PROTOTYPE_INVALID_ID;
	db->constraints[id].operand_count = operand_count;
	for (uint32_t i = 0; i < operand_count; ++i) {
		db->operands[db->operand_count++] = operands[i];
	}
	prototype_typing_constraint_solution_initialize(&db->solutions[id]);
	db->constraint_hash_slots[slot] = id;
	db->first_branch_ih_dependency[id] = PROTOTYPE_INVALID_ID;
	db->branch_ih_dependency_count[id] = 0;
	db->scc_count = 0;
	db->scc_member_total = 0;
	db->next_constraint_in_domain[id] = PROTOTYPE_INVALID_ID;
	uint32_t last = db->last_constraint_for_domain[constraint->domain];
	if (last == PROTOTYPE_INVALID_ID) {
		db->first_constraint_for_domain[constraint->domain] = id;
	} else {
		db->next_constraint_in_domain[last] = id;
	}
	db->last_constraint_for_domain[constraint->domain] = id;
	db->constraint_count_by_domain[constraint->domain]++;
	*constraint_id = id;
	return 0;
}

int prototype_typing_classifier_rhs_intern(
	struct prototype_typing_constraint_db* db,
	int kind,
	uint32_t source_term,
	const struct prototype_typing_equation_operand* operands,
	uint32_t operand_count,
	uint32_t* p_rhs
) {
	if (!db || !p_rhs ||
		kind < PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL ||
		kind > PROTOTYPE_TYPING_CLASSIFIER_RHS_TERMINATES ||
		source_term == PROTOTYPE_INVALID_ID ||
		(operand_count != 0 && !operands)) {
		return -1;
	}
	for (uint32_t i = 0; i < operand_count; ++i) {
		if (operands[i].target_kind <
				PROTOTYPE_TYPING_EQUATION_OPERAND_TYPED_PROJECTION ||
			operands[i].target_kind >
				PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE ||
			operands[i].target == PROTOTYPE_INVALID_ID ||
			(operands[i].target_kind ==
				PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS &&
			 operands[i].target >= db->classifier_rhs_count)) {
			return -1;
		}
	}
	uint32_t slot = (uint32_t)(typing_classifier_rhs_hash(
		kind, source_term, operands, operand_count
	) % PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY);
	for (uint32_t probe = 0;
		probe < PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY;
		++probe) {
		uint32_t existing = db->classifier_rhs_hash_slots[slot];
		if (existing == PROTOTYPE_INVALID_ID) {
			if (db->topology_sealed || db->classifier_rhs_count >=
					PROTOTYPE_TYPING_CLASSIFIER_RHS_CAPACITY ||
				operand_count > PROTOTYPE_TYPING_EQUATION_OPERAND_CAPACITY -
					db->operand_count) {
				return -1;
			}
			uint32_t rhs_id = db->classifier_rhs_count++;
			db->classifier_rhs[rhs_id] =
				(struct prototype_typing_classifier_rhs) {
					.kind = kind,
					.source_term = source_term,
					.first_operand = operand_count != 0 ?
						db->operand_count : PROTOTYPE_INVALID_ID,
					.operand_count = operand_count
				};
			for (uint32_t i = 0; i < operand_count; ++i) {
				db->operands[db->operand_count++] = operands[i];
			}
			db->classifier_rhs_hash_slots[slot] = rhs_id;
			*p_rhs = rhs_id;
			return 0;
		}
		if (existing >= db->classifier_rhs_count) {
			return -1;
		}
		if (typing_classifier_rhs_equal(
				db,
				&db->classifier_rhs[existing],
				kind,
				source_term,
				operands,
				operand_count
			)) {
			*p_rhs = existing;
			return 0;
		}
		slot = (slot + 1) % PROTOTYPE_TYPING_CLASSIFIER_RHS_HASH_CAPACITY;
	}
	return -1;
}

const struct prototype_typing_classifier_rhs*
prototype_typing_classifier_rhs_get(
	const struct prototype_typing_constraint_db* db,
	uint32_t rhs
) {
	return db && rhs < db->classifier_rhs_count ?
		&db->classifier_rhs[rhs] : NULL;
}

const struct prototype_typing_equation_operand*
prototype_typing_classifier_rhs_operand_get(
	const struct prototype_typing_constraint_db* db,
	const struct prototype_typing_classifier_rhs* rhs,
	uint32_t index
) {
	if (!db || !rhs || index >= rhs->operand_count ||
		rhs->first_operand == PROTOTYPE_INVALID_ID ||
		rhs->first_operand > db->operand_count ||
		index >= db->operand_count - rhs->first_operand) {
		return NULL;
	}
	return &db->operands[rhs->first_operand + index];
}

const struct prototype_typing_equation_operand*
prototype_typing_constraint_operand_get(
	const struct prototype_typing_constraint_db* db,
	const struct prototype_typing_constraint* constraint,
	uint32_t index
) {
	if (!db || !constraint || index >= constraint->operand_count ||
		constraint->first_operand == PROTOTYPE_INVALID_ID ||
		constraint->first_operand > db->operand_count ||
		index >= db->operand_count - constraint->first_operand) {
		return NULL;
	}
	return &db->operands[constraint->first_operand + index];
}

uint32_t prototype_typing_constraint_typed_projection_operand(
	const struct prototype_typing_constraint_db* db,
	const struct prototype_typing_constraint* constraint,
	uint32_t index
) {
	const struct prototype_typing_equation_operand* operand =
		prototype_typing_constraint_operand_get(db, constraint, index);
	return operand && operand->target_kind ==
		PROTOTYPE_TYPING_EQUATION_OPERAND_TYPED_PROJECTION ?
		operand->target : PROTOTYPE_INVALID_ID;
}

static int typing_constraint_add_dependency(
	struct prototype_typing_constraint_db* db,
	int source_kind,
	uint32_t source_count,
	uint32_t source,
	uint32_t constraint_id
) {
	if (!db) {
		return -1;
	}
	if (source == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t* first;
	if (source_kind == PROTOTYPE_TYPING_DEPENDENCY_SOURCE_PROJECTION) {
		if (source_count > PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY ||
			source >= source_count) {
			return -1;
		}
		first = &db->first_dependent_constraint[source];
	} else if (source_kind == PROTOTYPE_TYPING_DEPENDENCY_SOURCE_EQUATION) {
		if (source_count > db->constraint_count || source >= source_count) {
			return -1;
		}
		first = &db->first_equation_dependent_constraint[source];
	} else {
		return -1;
	}
	if (db->topology_sealed ||
		constraint_id >= db->constraint_count ||
		db->dependent_constraint_count >=
			PROTOTYPE_TYPING_DEPENDENT_CONSTRAINT_CAPACITY) {
		return -1;
	}
	for (uint32_t dependency = *first;
		dependency != PROTOTYPE_INVALID_ID;
		dependency = db->dependent_constraints[dependency].next) {
		if (dependency >= db->dependent_constraint_count) {
			return -1;
		}
		if (db->dependent_constraints[dependency].source_kind == source_kind &&
			db->dependent_constraints[dependency].source == source &&
			db->dependent_constraints[dependency].constraint == constraint_id) {
			return 0;
		}
	}
	uint32_t dependency = db->dependent_constraint_count++;
	db->dependent_constraints[dependency].source_kind = source_kind;
	db->dependent_constraints[dependency].source = source;
	db->dependent_constraints[dependency].constraint = constraint_id;
	db->dependent_constraints[dependency].next = *first;
	*first = dependency;
	db->scc_count = 0;
	db->scc_member_total = 0;
	return 0;
}

int prototype_typing_constraint_add_dependency(
	struct prototype_typing_constraint_db* db,
	uint32_t projection_count,
	uint32_t source_typed_projection,
	uint32_t constraint_id
) {
	return typing_constraint_add_dependency(
		db,
		PROTOTYPE_TYPING_DEPENDENCY_SOURCE_PROJECTION,
		projection_count,
		source_typed_projection,
		constraint_id
	);
}

int prototype_typing_constraint_add_equation_dependency(
	struct prototype_typing_constraint_db* db,
	uint32_t source_equation,
	uint32_t constraint_id
) {
	return typing_constraint_add_dependency(
		db,
		PROTOTYPE_TYPING_DEPENDENCY_SOURCE_EQUATION,
		db ? db->constraint_count : 0,
		source_equation,
		constraint_id
	);
}

static uint32_t typing_constraint_dependency_source_equation(
	const struct prototype_typing_constraint_db* db,
	uint32_t projection_count,
	const struct prototype_typing_dependent_constraint* dependency
) {
	if (!db || !dependency) {
		return PROTOTYPE_INVALID_ID;
	}
	if (dependency->source_kind ==
			PROTOTYPE_TYPING_DEPENDENCY_SOURCE_EQUATION) {
		return dependency->source < db->constraint_count ? dependency->source :
			PROTOTYPE_INVALID_ID;
	}
	if (dependency->source_kind !=
			PROTOTYPE_TYPING_DEPENDENCY_SOURCE_PROJECTION ||
		dependency->source >= projection_count || dependency->source >=
			PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t equation =
		db->classifier_constraint_for_projection[dependency->source];
	return equation < db->constraint_count ? equation : PROTOTYPE_INVALID_ID;
}

int prototype_typing_constraint_derive_sccs(
	struct prototype_typing_constraint_db* db,
	uint32_t projection_count
) {
	if (!db || db->topology_sealed || projection_count >
			PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY ||
		db->constraint_count > PROTOTYPE_TYPING_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t node_count = db->constraint_count;
	uint32_t edge_capacity = db->dependent_constraint_count == 0 ? 1 :
		db->dependent_constraint_count;
	uint32_t* head_out = malloc((node_count == 0 ? 1 : node_count) *
		sizeof(*head_out));
	uint32_t* head_in = malloc((node_count == 0 ? 1 : node_count) *
		sizeof(*head_in));
	uint32_t* edge_source = malloc(edge_capacity * sizeof(*edge_source));
	uint32_t* edge_target = malloc(edge_capacity * sizeof(*edge_target));
	uint32_t* next_out = malloc(edge_capacity * sizeof(*next_out));
	uint32_t* next_in = malloc(edge_capacity * sizeof(*next_in));
	uint8_t* visited = calloc(node_count == 0 ? 1 : node_count,
		sizeof(*visited));
	uint32_t* order = malloc((node_count == 0 ? 1 : node_count) *
		sizeof(*order));
	uint32_t* stack_node = malloc((node_count == 0 ? 1 : node_count) *
		sizeof(*stack_node));
	uint32_t* stack_edge = malloc((node_count == 0 ? 1 : node_count) *
		sizeof(*stack_edge));
	if (!head_out || !head_in || !edge_source || !edge_target || !next_out ||
		!next_in || !visited || !order || !stack_node || !stack_edge) {
		free(head_out); free(head_in); free(edge_source); free(edge_target);
		free(next_out); free(next_in); free(visited); free(order);
		free(stack_node); free(stack_edge);
		return -1;
	}
	for (uint32_t i = 0; i < node_count; ++i) {
		head_out[i] = PROTOTYPE_INVALID_ID;
		head_in[i] = PROTOTYPE_INVALID_ID;
		db->scc_for_constraint[i] = PROTOTYPE_INVALID_ID;
		db->first_scc_member[i] = PROTOTYPE_INVALID_ID;
		db->scc_member_count[i] = 0;
		db->scc_recursive[i] = 0;
	}
	uint32_t edge_count = 0;
	for (uint32_t i = 0; i < db->dependent_constraint_count; ++i) {
		const struct prototype_typing_dependent_constraint* dependency =
			&db->dependent_constraints[i];
		uint32_t source = typing_constraint_dependency_source_equation(
			db, projection_count, dependency
		);
		if (dependency->constraint >= node_count) {
			free(head_out); free(head_in); free(edge_source); free(edge_target);
			free(next_out); free(next_in); free(visited); free(order);
			free(stack_node); free(stack_edge);
			return -1;
		}
		/* Projection leaves without an owning equation are immutable external
		 * inputs. They wake dependents but do not belong to an equation SCC. */
		if (source == PROTOTYPE_INVALID_ID) {
			continue;
		}
		edge_source[edge_count] = source;
		edge_target[edge_count] = dependency->constraint;
		next_out[edge_count] = head_out[source];
		head_out[source] = edge_count;
		next_in[edge_count] = head_in[dependency->constraint];
		head_in[dependency->constraint] = edge_count;
		edge_count++;
	}
	uint32_t order_count = 0;
	for (uint32_t root = 0; root < node_count; ++root) {
		if (visited[root]) continue;
		uint32_t depth = 1;
		stack_node[0] = root;
		stack_edge[0] = head_out[root];
		visited[root] = 1;
		while (depth != 0) {
			uint32_t top = depth - 1;
			uint32_t edge = stack_edge[top];
			if (edge == PROTOTYPE_INVALID_ID) {
				order[order_count++] = stack_node[top];
				depth--;
				continue;
			}
			stack_edge[top] = next_out[edge];
			uint32_t target = edge_target[edge];
			if (visited[target]) continue;
			visited[target] = 1;
			stack_node[depth] = target;
			stack_edge[depth] = head_out[target];
			depth++;
		}
	}
	memset(visited, 0, (node_count == 0 ? 1 : node_count) *
		sizeof(*visited));
	uint32_t scc_count = 0;
	for (uint32_t cursor = order_count; cursor != 0; --cursor) {
		uint32_t root = order[cursor - 1];
		if (visited[root]) continue;
		uint32_t depth = 1;
		stack_node[0] = root;
		visited[root] = 1;
		while (depth != 0) {
			uint32_t node = stack_node[--depth];
			db->scc_for_constraint[node] = scc_count;
			db->scc_member_count[scc_count]++;
			for (uint32_t edge = head_in[node]; edge != PROTOTYPE_INVALID_ID;
				edge = next_in[edge]) {
				uint32_t source = edge_source[edge];
				if (!visited[source]) {
					visited[source] = 1;
					stack_node[depth++] = source;
				}
			}
		}
		scc_count++;
	}
	uint32_t member_total = 0;
	for (uint32_t scc = 0; scc < scc_count; ++scc) {
		db->first_scc_member[scc] = member_total;
		member_total += db->scc_member_count[scc];
		db->scc_member_count[scc] = 0;
	}
	for (uint32_t node = 0; node < node_count; ++node) {
		uint32_t scc = db->scc_for_constraint[node];
		uint32_t slot = db->first_scc_member[scc] +
			db->scc_member_count[scc]++;
		db->scc_members[slot] = node;
	}
	for (uint32_t scc = 0; scc < scc_count; ++scc) {
		if (db->scc_member_count[scc] > 1) db->scc_recursive[scc] = 1;
	}
	for (uint32_t edge = 0; edge < edge_count; ++edge) {
		if (edge_source[edge] == edge_target[edge]) {
			db->scc_recursive[db->scc_for_constraint[edge_source[edge]]] = 1;
		}
	}
	db->scc_count = scc_count;
	db->scc_member_total = member_total;
	free(head_out); free(head_in); free(edge_source); free(edge_target);
	free(next_out); free(next_in); free(visited); free(order);
	free(stack_node); free(stack_edge);
	return member_total == node_count ? 0 : -1;
}
