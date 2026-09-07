#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_CLASSIFIER_STATE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_CLASSIFIER_STATE_H

#include <stdint.h>

#include "a_program/frontend/typing_seed_state.h"

struct prototype_typing_motive_solution {
	uint32_t constant_candidate;
	uint32_t motive;
	uint32_t expected_equation_classifier;
	uint32_t expected_equation_constraint_id;
	uint32_t expected_equation_context_id;
	uint64_t dependency_mask;
	uint64_t unavailable_mask;
	uint64_t conflict_mask;
	uint32_t source_case_index;
	uint32_t source_classifier;
	uint64_t recursive_equation_revision;
	uint8_t status;
	uint8_t phase;
};

/* Mutable classifier and motive answers for Layer T. Core Term IDs occur only
 * as immutable operands/results and never as an ownership capability. */
struct prototype_typing_classifier_state {
	int initialized;
	uint32_t initialized_occurrence_count;
	uint32_t expected_app_codomain_binders[
		PROTOTYPE_TYPING_CLASSIFIER_OCCURRENCE_CAPACITY
	];
};

#endif
