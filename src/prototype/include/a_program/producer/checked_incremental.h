#ifndef A_PROGRAM_PROTOTYPE_PRODUCER_CHECKED_INCREMENTAL_H
#define A_PROGRAM_PROTOTYPE_PRODUCER_CHECKED_INCREMENTAL_H

#include <stddef.h>

#include "a_program/checker/module_set.h"
#include "a_program/producer/incremental.h"

struct prototype_checked_incremental_snapshot;

/* A checked fragment/SCC is the atomic edit unit. Its exports receive stable
 * GoalKeys and share the fragment's canonical checked-content fingerprint. */
int prototype_checked_incremental_snapshot_create(
	const struct prototype_checked_module_set* set,
	struct prototype_checked_incremental_snapshot** p_snapshot
);

void prototype_checked_incremental_snapshot_destroy(
	struct prototype_checked_incremental_snapshot* snapshot
);

size_t prototype_checked_incremental_snapshot_goal_count(
	const struct prototype_checked_incremental_snapshot* snapshot
);

const struct prototype_incremental_goal*
prototype_checked_incremental_snapshot_goal_at(
	const struct prototype_checked_incremental_snapshot* snapshot,
	size_t index
);

/* Writes one byte per previous goal. Removed or changed fragments invalidate
 * their exports and the reverse closure of checked interface dependencies. */
int prototype_checked_incremental_compare(
	const struct prototype_checked_incremental_snapshot* previous,
	const struct prototype_checked_incremental_snapshot* current,
	unsigned char* invalidated_goals,
	size_t invalidated_capacity
);

#endif
