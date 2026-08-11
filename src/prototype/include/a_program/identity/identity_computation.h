#ifndef __A_PROGRAM_IDENTITY_IDENTITY_COMPUTATION_H__
#define __A_PROGRAM_IDENTITY_IDENTITY_COMPUTATION_H__

#include "a_program/identity/types.h"

int prototype_hott_execute_identity_type_computation(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
);
int prototype_hott_execute_object_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
);
int prototype_hott_construct_degeneracy(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t source_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
);

int prototype_hott_construct_universe_correspondence_projection(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t universe_identity_result_id,
	uint32_t correspondence_claim_id,
	int projection,
	uint32_t* p_projection_term_id,
	uint32_t* p_projection_claim_id
);


#endif
