#ifndef __A_PROGRAM_IDENTITY_OBJECT_TERM_ACTION_H__
#define __A_PROGRAM_IDENTITY_OBJECT_TERM_ACTION_H__

#include "a_program/identity/types.h"

int prototype_hott_construct_object_term_action(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t source_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
);
int prototype_hott_instantiate_object_identity_family(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_identity_family_claim_id
);
int prototype_hott_check_object_identity_witness(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t witness_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_checked_witness_claim_id
);
int prototype_hott_execute_substitution_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_result_id
);
int prototype_hott_execute_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
);

#endif
