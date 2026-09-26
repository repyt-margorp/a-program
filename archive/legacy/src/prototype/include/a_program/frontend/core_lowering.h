#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_CORE_LOWERING_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_CORE_LOWERING_H

#include <stdint.h>

#include "a_program/protocol/request.h"
#include "a_program/frontend/source_core_handoff.h"
#include "a_program/frontend/source_lowering_plan.h"

struct prototype_core_request_service;

/*
 * C0 source-formation capability. It can submit Core-domain formation
 * requests, but it cannot inspect TermDB or construct a Layer T object.
 */
struct prototype_c0_lowering_stage {
	const struct prototype_core_request_service* core;
};

/* One call-local C0 result. The driver records it in the neutral handoff. */
struct prototype_c0_open_term {
	uint32_t term_id;
	uint64_t graph_revision;
};

enum prototype_c0_source_slot_status {
	PROTOTYPE_C0_SOURCE_SLOT_FORMED = 0,
	PROTOTYPE_C0_SOURCE_SLOT_DEFERRED = 1
};

int prototype_c0_lowering_stage_init(
	struct prototype_c0_lowering_stage* stage,
	const struct prototype_core_request_service* core
);

int prototype_c0_lowering_stage_preallocate_bindings(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_source_lowering_plan* plan,
	struct prototype_source_core_handoff_builder* handoff
);

int prototype_c0_lowering_stage_form_open_plan(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_source_lowering_plan* plan,
	struct prototype_source_core_handoff_builder* handoff,
	size_t* p_deferred_count
);

int prototype_c0_lowering_stage_text_literal(
	const struct prototype_c0_lowering_stage* stage,
	int text_symbol_id,
	struct prototype_c0_open_term* p_result
);

int prototype_c0_lowering_stage_int_literal(
	const struct prototype_c0_lowering_stage* stage,
	int64_t value,
	struct prototype_c0_open_term* p_result
);

int prototype_c0_lowering_stage_host_type(
	const struct prototype_c0_lowering_stage* stage,
	int host_type_id,
	struct prototype_c0_open_term* p_result
);

int prototype_c0_lowering_stage_pure_primitive(
	const struct prototype_c0_lowering_stage* stage,
	int primitive_id,
	int type_symbol_id,
	struct prototype_c0_open_term* p_result
);

int prototype_c0_lowering_stage_effect_operation(
	const struct prototype_c0_lowering_stage* stage,
	int operation_id,
	struct prototype_c0_open_term* p_result
);

int prototype_c0_lowering_stage_return(
	const struct prototype_c0_lowering_stage* stage,
	uint32_t value,
	struct prototype_c0_open_term* p_result
);

int prototype_c0_lowering_stage_thunk(
	const struct prototype_c0_lowering_stage* stage,
	uint32_t computation,
	struct prototype_c0_open_term* p_result
);

int prototype_c0_lowering_stage_force(
	const struct prototype_c0_lowering_stage* stage,
	uint32_t value,
	struct prototype_c0_open_term* p_result
);

#endif
