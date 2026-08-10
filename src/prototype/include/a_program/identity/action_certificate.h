#ifndef __A_PROGRAM_IDENTITY_ACTION_CERTIFICATE_H__
#define __A_PROGRAM_IDENTITY_ACTION_CERTIFICATE_H__

#include "a_program/identity/types.h"

void prototype_hott_action_db_init(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action_request* requests,
	size_t request_capacity,
	struct prototype_hott_action_certificate* certificates,
	size_t certificate_capacity,
	struct prototype_hott_action_result* results,
	size_t result_capacity
);
const struct prototype_hott_action_request* prototype_hott_action_request_get(
	const struct prototype_hott_action_db* db,
	uint32_t request_id
);
const struct prototype_hott_action_result* prototype_hott_action_result_get(
	const struct prototype_hott_action_db* db,
	uint32_t result_id
);
int prototype_hott_action_request_intern(
	struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_hott_action_request request,
	uint32_t* p_request_id
);
int prototype_hott_action_certificate_add(
	struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_hott_action_certificate certificate,
	uint32_t* p_certificate_id
);
int prototype_hott_action_result_publish(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action_result result,
	uint32_t* p_result_id
);
int prototype_hott_action_db_validate(
	const struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges
);
int prototype_hott_register_identity_root(
	struct prototype_artifact_interface* interface,
	const struct prototype_hott_action_db* actions,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t witness_has_type_claim_id,
	uint32_t* p_root_id
);

#endif
