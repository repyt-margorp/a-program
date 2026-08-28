#ifndef A_PROGRAM_PROTOTYPE_GRAPH_RUNTIME_H
#define A_PROGRAM_PROTOTYPE_GRAPH_RUNTIME_H

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
	uint32_t failed_occurrence;
	uint32_t frame_count;
	int frame_kinds[64];
	uint32_t frame_occurrences[64];
	uint32_t obligation_instance_count;
	int obligation_states[64];
	uint32_t obligation_occurrences[64];
};

struct prototype_runtime_annotations {
	const struct prototype_typed_occurrence_graph* occurrences;
	const struct prototype_verification_db* verification;
	struct prototype_type_declaration_db* verification_type_declarations;
};


int prototype_runtime_evaluate_core_with_annotations(
	const struct prototype_runtime_annotations* annotations,
	struct prototype_term_db* terms,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t core_term,
	uint32_t trace_occurrence,
	uint32_t* p_ret,
	int* p_verification_state,
	struct prototype_runtime_trace* p_trace
);

#endif
