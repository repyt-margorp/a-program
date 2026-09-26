#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_CONVERSION_GOAL_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_CONVERSION_GOAL_H

#include <stdint.h>

#include "a_program/protocol/conversion.h"

/* Layer T records why a deterministic Core conversion is required. The goal
 * carries only IDs and a value result, never Core or Judgement storage. */
struct prototype_typing_conversion_goal {
	uint32_t id;
	uint32_t context_id;
	uint32_t carrier_classifier;
	uint32_t left_term;
	uint32_t right_term;
	int normalization_profile;
	uint64_t step_limit;
	struct prototype_term_conversion_result result;
};

#endif
