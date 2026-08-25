#include "a_program/producer/incremental.h"

#include <stdlib.h>
#include <string.h>

static size_t find_goal(
	const struct prototype_incremental_goal* goals,
	size_t count,
	const struct prototype_goal_key* key
) {
	for (size_t i = 0; i < count; ++i) {
		if (prototype_goal_key_equal(&goals[i].key, key)) return i;
	}
	return SIZE_MAX;
}

static size_t find_support(
	const struct prototype_incremental_support* supports,
	size_t count,
	const struct prototype_goal_key* key
) {
	for (size_t i = 0; i < count; ++i) {
		if (prototype_goal_key_equal(&supports[i].support, key)) return i;
	}
	return SIZE_MAX;
}

static size_t find_space(
	const struct prototype_incremental_candidate_space* spaces,
	size_t count,
	const struct prototype_goal_key* key
) {
	for (size_t i = 0; i < count; ++i) {
		if (prototype_goal_key_equal(&spaces[i].key, key)) return i;
	}
	return SIZE_MAX;
}

static int fingerprint_equal(const uint64_t left[4], const uint64_t right[4]) {
	return memcmp(left, right, 4 * sizeof(*left)) == 0;
}

int prototype_incremental_compute_invalidation(
	const struct prototype_incremental_graph* graph,
	const struct prototype_incremental_snapshot* current,
	unsigned char* invalidated_goals,
	size_t invalidated_capacity
) {
	if (!graph || !current || invalidated_capacity < graph->goal_count ||
		(graph->goal_count != 0 && (!graph->goals || !invalidated_goals)) ||
		(graph->support_count != 0 && !graph->supports) ||
		(graph->positive_count != 0 && !graph->positive) ||
		(graph->negative_count != 0 && !graph->negative) ||
		(current->goal_count != 0 && !current->goals) ||
		(current->candidate_space_count != 0 && !current->candidate_spaces)) {
		return -1;
	}
	unsigned char* support_valid = graph->support_count == 0 ? NULL : malloc(
		graph->support_count
	);
	if (graph->support_count != 0 && !support_valid) return -1;
	memset(support_valid, 1, graph->support_count);
	memset(invalidated_goals, 0, graph->goal_count);
	for (size_t i = 0; i < graph->goal_count; ++i) {
		if (find_goal(graph->goals, i, &graph->goals[i].key) != SIZE_MAX) {
			free(support_valid);
			return -1;
		}
	}
	for (size_t i = 0; i < graph->support_count; ++i) {
		if (find_goal(graph->goals, graph->goal_count,
				&graph->supports[i].goal) == SIZE_MAX ||
			find_support(graph->supports, i, &graph->supports[i].support) !=
				SIZE_MAX) {
			free(support_valid);
			return -1;
		}
	}
	for (size_t i = 0; i < graph->positive_count; ++i) {
		size_t support = find_support(
			graph->supports, graph->support_count, &graph->positive[i].support
		);
		size_t dependency = find_goal(
			current->goals, current->goal_count, &graph->positive[i].dependency
		);
		if (support == SIZE_MAX) {
			free(support_valid);
			return -1;
		}
		if (dependency == SIZE_MAX || !fingerprint_equal(
				graph->positive[i].observed_fingerprint,
				current->goals[dependency].content_fingerprint
			)) support_valid[support] = 0;
	}
	for (size_t i = 0; i < graph->negative_count; ++i) {
		size_t support = find_support(
			graph->supports, graph->support_count, &graph->negative[i].support
		);
		size_t space = find_space(
			current->candidate_spaces, current->candidate_space_count,
			&graph->negative[i].candidate_space
		);
		if (support == SIZE_MAX) {
			free(support_valid);
			return -1;
		}
		/* An absent or changed candidate space is not a valid negative fact.
		 * Only an exact sealed-space observation can preserve the support. */
		if (space == SIZE_MAX || !fingerprint_equal(
				graph->negative[i].observed_sealed_fingerprint,
				current->candidate_spaces[space].sealed_fingerprint
			)) support_valid[support] = 0;
	}
	int changed;
	do {
		changed = 0;
		for (size_t i = 0; i < graph->goal_count; ++i) {
			int has_support = 0;
			for (size_t j = 0; j < graph->support_count; ++j) {
				if (support_valid[j] && prototype_goal_key_equal(
						&graph->supports[j].goal, &graph->goals[i].key
					)) {
					has_support = 1;
					break;
				}
			}
			if (!has_support && !invalidated_goals[i]) {
				invalidated_goals[i] = 1;
				changed = 1;
			}
		}
		for (size_t i = 0; i < graph->positive_count; ++i) {
			size_t dependency = find_goal(
				graph->goals, graph->goal_count, &graph->positive[i].dependency
			);
			size_t support = find_support(
				graph->supports, graph->support_count, &graph->positive[i].support
			);
			if (dependency != SIZE_MAX && invalidated_goals[dependency] &&
				support != SIZE_MAX && support_valid[support]) {
				support_valid[support] = 0;
				changed = 1;
			}
		}
	} while (changed);
	free(support_valid);
	return 0;
}
