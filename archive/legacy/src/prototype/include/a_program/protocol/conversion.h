#ifndef A_PROGRAM_PROTOTYPE_PROTOCOL_CONVERSION_H
#define A_PROGRAM_PROTOTYPE_PROTOCOL_CONVERSION_H

#include <stdint.h>

enum prototype_term_conversion_status {
	PROTOTYPE_TERM_CONVERSION_EQUAL = 1,
	PROTOTYPE_TERM_CONVERSION_NOT_EQUAL,
	PROTOTYPE_TERM_CONVERSION_RESIDUAL,
	PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT,
	PROTOTYPE_TERM_CONVERSION_EXHAUSTED,
	PROTOTYPE_TERM_CONVERSION_INVALID
};

enum prototype_term_conversion_reason {
	PROTOTYPE_TERM_CONVERSION_REASON_NONE = 0,
	PROTOTYPE_TERM_CONVERSION_REASON_NEUTRAL,
	PROTOTYPE_TERM_CONVERSION_REASON_OPAQUE_DEFINITION,
	PROTOTYPE_TERM_CONVERSION_REASON_UNSUPPORTED_RULE,
	PROTOTYPE_TERM_CONVERSION_REASON_EFFECT_REQUEST,
	PROTOTYPE_TERM_CONVERSION_REASON_STEP_LIMIT,
	PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT,
	PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH
};

struct prototype_term_conversion_result {
	int status;
	int reason;
	int profile;
	uint32_t left;
	uint32_t right;
	uint32_t left_observation;
	uint32_t right_observation;
	uint64_t step_limit;
	uint64_t steps_used;
	uint64_t graph_revision;
};

const char* prototype_term_conversion_status_name(int status);
const char* prototype_term_conversion_reason_name(int reason);

#endif
