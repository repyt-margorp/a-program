#include "a_program/producer/goal.h"

#include <string.h>

static void goal_mix(struct prototype_goal_key* key, const void* data, size_t count) {
	static const uint64_t primes[4] = {
		UINT64_C(1099511628211), UINT64_C(14029467366897019727),
		UINT64_C(1609587929392839161), UINT64_C(9650029242287828579)
	};
	const unsigned char* bytes = data;
	for (size_t i = 0; i < count; ++i) {
		for (size_t lane = 0; lane < 4; ++lane) {
			key->words[lane] ^= (uint64_t)bytes[i] + (lane << 8);
			key->words[lane] *= primes[lane];
			key->words[lane] ^= key->words[lane] >> (13 + lane);
		}
	}
}

static void goal_mix_string(struct prototype_goal_key* key, const char* value) {
	uint64_t length = value ? strlen(value) : UINT64_MAX;
	goal_mix(key, &length, sizeof(length));
	if (value) goal_mix(key, value, (size_t)length);
}

int prototype_goal_key_make(
	uint32_t producer_kind,
	const char* namespace_name,
	const char* declaration_name,
	const void* semantic_path,
	size_t semantic_path_size,
	struct prototype_goal_key* p_key
) {
	if (!declaration_name || !p_key ||
		(semantic_path_size != 0 && !semantic_path)) return -1;
	*p_key = (struct prototype_goal_key) {{
		UINT64_C(1469598103934665603), UINT64_C(7809847782465536322),
		UINT64_C(9650029242287828579), UINT64_C(2870177450012600261)
	}};
	goal_mix(p_key, &producer_kind, sizeof(producer_kind));
	goal_mix_string(p_key, namespace_name);
	goal_mix_string(p_key, declaration_name);
	goal_mix(p_key, &semantic_path_size, sizeof(semantic_path_size));
	goal_mix(p_key, semantic_path, semantic_path_size);
	return 0;
}

int prototype_goal_key_equal(
	const struct prototype_goal_key* left,
	const struct prototype_goal_key* right
) {
	return left && right && memcmp(left, right, sizeof(*left)) == 0;
}

int prototype_goal_key_compare(
	const struct prototype_goal_key* left,
	const struct prototype_goal_key* right
) {
	if (!left || !right) return left ? 1 : right ? -1 : 0;
	return memcmp(left->words, right->words, sizeof(left->words));
}
