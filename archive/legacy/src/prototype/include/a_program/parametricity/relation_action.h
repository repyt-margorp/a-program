#ifndef A_PROGRAM_PROTOTYPE_PARAMETRICITY_RELATION_ACTION_H
#define A_PROGRAM_PROTOTYPE_PARAMETRICITY_RELATION_ACTION_H

#include "a_program/parametricity/types.h"

void prototype_parametricity_relation_goal_db_init(
	struct prototype_parametricity_relation_goal_db* db,
	struct prototype_parametricity_relation_goal* goals,
	size_t goal_capacity
);
const struct prototype_parametricity_relation_goal*
prototype_parametricity_relation_goal_db_get(
	const struct prototype_parametricity_relation_goal_db* db,
	uint32_t goal_id
);
int prototype_parametricity_relation_goal_db_intern(
	struct prototype_parametricity_relation_goal_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	int category,
	uint32_t left_carrier_claim_id,
	uint32_t right_carrier_claim_id,
	uint32_t left_claim_id,
	uint32_t right_claim_id,
	uint32_t bridge_id,
	uint32_t* p_goal_id
);
int prototype_parametricity_relation_goal_db_validate(
	const struct prototype_parametricity_relation_goal_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges
);

void prototype_parametricity_candidate_db_init(
	struct prototype_parametricity_candidate_db* db,
	struct prototype_parametricity_candidate* candidates,
	size_t candidate_capacity,
	struct prototype_parametricity_claim_premise* claim_premises,
	size_t claim_premise_capacity,
	struct prototype_parametricity_child_edge* child_edges,
	size_t child_edge_capacity,
	struct prototype_parametricity_conversion_premise* conversion_premises,
	size_t conversion_premise_capacity,
	struct prototype_parametricity_context_certificate_premise* context_certificate_premises,
	size_t context_certificate_premise_capacity,
	struct prototype_parametricity_substitution_certificate_premise* substitution_certificate_premises,
	size_t substitution_certificate_premise_capacity
);
const struct prototype_parametricity_candidate* prototype_parametricity_candidate_db_get(
	const struct prototype_parametricity_candidate_db* db,
	uint32_t candidate_id
);
int prototype_parametricity_candidate_db_validate(
	const struct prototype_parametricity_candidate_db* db,
	const struct prototype_parametricity_relation_goal_db* goals,
	const struct prototype_kernel_view* kernel
);

void prototype_parametricity_work_db_init(
	struct prototype_parametricity_work_db* db,
	struct prototype_parametricity_work_item* items,
	size_t item_capacity
);
const struct prototype_parametricity_work_item* prototype_parametricity_work_db_get(
	const struct prototype_parametricity_work_db* db,
	uint32_t work_item_id
);
int prototype_parametricity_work_db_validate(
	const struct prototype_parametricity_work_db* db,
	const struct prototype_parametricity_relation_goal_db* goals,
	const struct prototype_parametricity_candidate_db* candidates
);
int prototype_parametricity_relation_plan(
	struct prototype_parametricity_relation_goal_db* goals,
	struct prototype_parametricity_candidate_db* candidates,
	struct prototype_parametricity_work_db* work,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_work_item_id
);

void prototype_parametricity_residual_db_init(
	struct prototype_parametricity_residual_db* db,
	struct prototype_parametricity_residual_obligation* obligations,
	size_t obligation_capacity
);
int prototype_parametricity_residual_db_add_from_work(
	struct prototype_parametricity_residual_db* db,
	const struct prototype_parametricity_work_db* work,
	uint32_t work_item_id,
	uint32_t* p_obligation_id
);
int prototype_parametricity_residual_db_validate(
	const struct prototype_parametricity_residual_db* db,
	const struct prototype_parametricity_work_db* work
);
int prototype_parametricity_residual_db_require_artifact_empty(
	const struct prototype_parametricity_residual_db* db
);


#endif
