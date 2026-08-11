#ifndef A_PROGRAM_PROTOTYPE_GRAPH_OPERATION_RUNTIME_H
#define A_PROGRAM_PROTOTYPE_GRAPH_OPERATION_RUNTIME_H

#include "a_program/graph/verification.h"

enum prototype_runtime_failure_kind {
	PROTOTYPE_RUNTIME_FAILURE_NONE = 0,
	PROTOTYPE_RUNTIME_FAILURE_INVALID_OPERATION,
	PROTOTYPE_RUNTIME_FAILURE_STACK_CAPACITY,
	PROTOTYPE_RUNTIME_FAILURE_UNHANDLED_OPERATION,
	PROTOTYPE_RUNTIME_FAILURE_VERIFICATION
};

struct prototype_runtime_trace {
	int failure_kind;
	uint32_t failed_operation;
	uint32_t frame_count;
	int frame_kinds[64];
	uint32_t frame_operations[64];
	uint32_t obligation_instance_count;
	int obligation_states[64];
	uint32_t obligation_operations[64];
};

int prototype_operation_evaluate_with_trace(
	struct prototype_compile_metadata* metadata,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t operation_id,
	uint32_t* p_ret,
	int* p_verification_state,
	struct prototype_runtime_trace* p_trace
);

#endif
