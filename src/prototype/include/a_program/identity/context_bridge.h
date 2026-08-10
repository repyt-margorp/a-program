#ifndef __A_PROGRAM_IDENTITY_CONTEXT_BRIDGE_H__
#define __A_PROGRAM_IDENTITY_CONTEXT_BRIDGE_H__

#include "a_program/identity/types.h"

void prototype_hott_bridge_db_init(
	struct prototype_hott_bridge_db* db,
	struct prototype_hott_bridge* bridges,
	size_t bridge_capacity,
	struct prototype_hott_bridge_certificate* certificates,
	size_t certificate_capacity
);
const struct prototype_hott_bridge* prototype_hott_bridge_db_get(
	const struct prototype_hott_bridge_db* db,
	uint32_t bridge_id
);
int prototype_hott_bridge_db_construct(
	struct prototype_hott_bridge_db* db,
	struct prototype_kernel_builder* kernel,
	uint32_t source_context_id,
	uint32_t* p_bridge_id
);
int prototype_hott_bridge_db_validate(
	const struct prototype_hott_bridge_db* db,
	const struct prototype_kernel_view* kernel
);

int prototype_hott_bridge_db_construct_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t relation_type_action_request_id,
	uint32_t* p_bridge_id
);
int prototype_hott_bridge_db_construct_identity_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t identity_type_action_request_id,
	uint32_t* p_bridge_id
);
int prototype_hott_bridge_db_ensure_identity_context(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t* p_bridge_id,
	int* p_residual_reason
);


#endif
