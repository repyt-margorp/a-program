#ifndef A_PROGRAM_PROTOTYPE_PRODUCER_INCREMENTAL_H
#define A_PROGRAM_PROTOTYPE_PRODUCER_INCREMENTAL_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/producer/goal.h"

struct prototype_incremental_goal {
	struct prototype_goal_key key;
	uint64_t content_fingerprint[4];
};

struct prototype_incremental_support {
	struct prototype_goal_key goal;
	struct prototype_goal_key support;
};

struct prototype_incremental_positive_observation {
	struct prototype_goal_key support;
	struct prototype_goal_key dependency;
	uint64_t observed_fingerprint[4];
};

struct prototype_incremental_negative_observation {
	struct prototype_goal_key support;
	struct prototype_goal_key candidate_space;
	uint64_t observed_sealed_fingerprint[4];
};

struct prototype_incremental_candidate_space {
	struct prototype_goal_key key;
	uint64_t sealed_fingerprint[4];
};

struct prototype_incremental_snapshot {
	const struct prototype_incremental_goal* goals;
	size_t goal_count;
	const struct prototype_incremental_candidate_space* candidate_spaces;
	size_t candidate_space_count;
};

struct prototype_incremental_graph {
	const struct prototype_incremental_goal* goals;
	size_t goal_count;
	const struct prototype_incremental_support* supports;
	size_t support_count;
	const struct prototype_incremental_positive_observation* positive;
	size_t positive_count;
	const struct prototype_incremental_negative_observation* negative;
	size_t negative_count;
};

/* Writes one byte per graph goal: one means every known support was
 * invalidated. Arrays are immutable and may be shared by scheduler workers. */
int prototype_incremental_compute_invalidation(
	const struct prototype_incremental_graph* graph,
	const struct prototype_incremental_snapshot* current,
	unsigned char* invalidated_goals,
	size_t invalidated_capacity
);

#endif
