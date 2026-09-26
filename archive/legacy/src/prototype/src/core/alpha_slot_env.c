#include "a_program/core/alpha_slot_env.h"

int prototype_alpha_slot_lookup(
	const struct prototype_alpha_slot_env* env,
	uint32_t identity,
	uint32_t* p_slot
) {
	if (!env || !p_slot) return 0;
	for (uint32_t i = env->count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->identities[index] == identity) {
			*p_slot = env->slots[index];
			return 1;
		}
	}
	return 0;
}

int prototype_alpha_slot_push(
	struct prototype_alpha_slot_env* env,
	uint32_t identity
) {
	if (!env || env->count >= PROTOTYPE_ALPHA_SLOT_CAPACITY) return -1;
	env->identities[env->count] = identity;
	env->slots[env->count] = env->next_slot++;
	env->count += 1;
	return 0;
}

struct prototype_alpha_slot_checkpoint prototype_alpha_slot_checkpoint(
	const struct prototype_alpha_slot_env* env
) {
	return env ? (struct prototype_alpha_slot_checkpoint) {
		.count = env->count,
		.next_slot = env->next_slot
	} : (struct prototype_alpha_slot_checkpoint) {0};
}

int prototype_alpha_slot_restore(
	struct prototype_alpha_slot_env* env,
	struct prototype_alpha_slot_checkpoint checkpoint
) {
	if (!env || checkpoint.count > env->count ||
		checkpoint.next_slot > env->next_slot) return -1;
	env->count = checkpoint.count;
	env->next_slot = checkpoint.next_slot;
	return 0;
}

int prototype_alpha_slot_intern(
	struct prototype_alpha_slot_env* env,
	uint32_t identity,
	uint32_t* p_slot
) {
	if (!env || !p_slot) return -1;
	if (prototype_alpha_slot_lookup(env, identity, p_slot)) return 0;
	if (prototype_alpha_slot_push(env, identity) != 0) return -1;
	*p_slot = env->slots[env->count - 1];
	return 0;
}
