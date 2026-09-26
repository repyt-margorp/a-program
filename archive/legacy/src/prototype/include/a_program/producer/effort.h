#ifndef A_PROGRAM_PROTOTYPE_PRODUCER_EFFORT_H
#define A_PROGRAM_PROTOTYPE_PRODUCER_EFFORT_H

#include <stdint.h>

#define PROTOTYPE_EFFORT_COST_MODEL_VERSION UINT32_C(1)

enum prototype_effort_phase {
	PROTOTYPE_EFFORT_PHASE_GRAPH = 0,
	PROTOTYPE_EFFORT_PHASE_CLASSIFIER = 1,
	PROTOTYPE_EFFORT_PHASE_NORMALIZATION = 2,
	PROTOTYPE_EFFORT_PHASE_MOTIVE = 3,
	PROTOTYPE_EFFORT_PHASE_PROOF = 4,
	PROTOTYPE_EFFORT_PHASE_FUNCTION_GRAPH = 5,
	PROTOTYPE_EFFORT_PHASE_CHECKER = 6,
	PROTOTYPE_EFFORT_PHASE_MERGE = 7,
	PROTOTYPE_EFFORT_PHASE_COUNT = 8
};

/* Compile effort is a meta-level scheduler resource. It is deliberately
 * separate from object computation totality and effect rows. */
struct prototype_effort_account {
	uint32_t cost_model_version;
	uint64_t limit;
	uint64_t used;
	uint64_t phase_used[PROTOTYPE_EFFORT_PHASE_COUNT];
	int exhausted;
	int exhausted_phase;
};

void prototype_effort_account_init(
	struct prototype_effort_account* account,
	uint64_t limit
);

uint64_t prototype_effort_remaining(
	const struct prototype_effort_account* account
);

int prototype_effort_add_credits(
	struct prototype_effort_account* account,
	uint64_t credits
);

/* Returns zero after consuming credits, one when the account must pause, and
 * minus one for malformed input or arithmetic overflow. */
int prototype_effort_consume(
	struct prototype_effort_account* account,
	int phase,
	uint64_t credits
);

/* Consumes one credit vector atomically. No phase is charged unless the whole
 * vector fits in the remaining account. */
int prototype_effort_consume_split(
	struct prototype_effort_account* account,
	const uint64_t phase_credits[PROTOTYPE_EFFORT_PHASE_COUNT]
);

int prototype_effort_account_accumulate_snapshot(
	struct prototype_effort_account* target,
	const struct prototype_effort_account* source
);

#endif
