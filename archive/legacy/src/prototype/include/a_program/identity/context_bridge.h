#ifndef __A_PROGRAM_IDENTITY_CONTEXT_BRIDGE_H__
#define __A_PROGRAM_IDENTITY_CONTEXT_BRIDGE_H__

#include "a_program/identity/types.h"

int prototype_hott_bridge_db_init(
	struct prototype_hott_bridge_db* db,
	struct prototype_hott_bridge* bridges,
	size_t bridge_capacity,
	struct prototype_hott_bridge_certificate* certificates,
	size_t certificate_capacity,
	struct prototype_hott_bridge_face_binding* face_bindings,
	size_t face_binding_capacity,
	struct prototype_dimension_operator_db* dimension_operators
);
const struct prototype_hott_bridge* prototype_hott_bridge_db_get(
	const struct prototype_hott_bridge_db* db,
	uint32_t bridge_id
);
const struct prototype_hott_bridge_face_binding*
prototype_hott_bridge_face_binding_get(
	const struct prototype_hott_bridge_db* db,
	uint32_t bridge_id,
	uint32_t face_ordinal
);
const struct prototype_hott_bridge_face_binding*
prototype_hott_bridge_binding_action_get(
	const struct prototype_hott_bridge_db* db,
	uint32_t bridge_id,
	uint32_t source_binding_id,
	uint32_t face_ordinal
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
