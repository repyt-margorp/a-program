#ifndef A_PROGRAM_PROTOTYPE_PARAMETRICITY_ACTION_EXECUTION_H
#define A_PROGRAM_PROTOTYPE_PARAMETRICITY_ACTION_EXECUTION_H

#include "a_program/identity/types.h"

int prototype_parametricity_execute_relation_type_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
);

#endif
