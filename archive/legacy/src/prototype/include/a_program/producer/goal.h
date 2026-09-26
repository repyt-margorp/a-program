#ifndef A_PROGRAM_PROTOTYPE_PRODUCER_GOAL_H
#define A_PROGRAM_PROTOTYPE_PRODUCER_GOAL_H

#include <stddef.h>
#include <stdint.h>

struct prototype_goal_key {
	uint64_t words[4];
};

/* Goal identity is stable declaration identity plus a producer-local semantic
 * path. It is deliberately unrelated to generative Context Binding identity. */
int prototype_goal_key_make(
	uint32_t producer_kind,
	const char* namespace_name,
	const char* declaration_name,
	const void* semantic_path,
	size_t semantic_path_size,
	struct prototype_goal_key* p_key
);

int prototype_goal_key_equal(
	const struct prototype_goal_key* left,
	const struct prototype_goal_key* right
);

int prototype_goal_key_compare(
	const struct prototype_goal_key* left,
	const struct prototype_goal_key* right
);

#endif
