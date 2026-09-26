#ifndef A_PROGRAM_PROTOTYPE_PARAMETRICITY_EXECUTION_H
#define A_PROGRAM_PROTOTYPE_PARAMETRICITY_EXECUTION_H

#include "a_program/identity/types.h"
#include "a_program/parametricity/types.h"

int prototype_parametricity_relation_plan_and_execute(
	struct prototype_parametricity_relation_goal_db* goals,
	struct prototype_parametricity_candidate_db* candidates,
	struct prototype_parametricity_work_db* work,
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
