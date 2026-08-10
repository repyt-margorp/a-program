#ifndef __A_PROGRAM_IDENTITY_ACTION_EXECUTION_H__
#define __A_PROGRAM_IDENTITY_ACTION_EXECUTION_H__

#include "a_program/identity/types.h"

int prototype_hott_relation_plan_and_execute(
	struct prototype_hott_relation_goal_db* goals,
	struct prototype_hott_candidate_db* candidates,
	struct prototype_hott_work_db* work,
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	struct prototype_hott_relation_execution* p_execution
);


#endif
