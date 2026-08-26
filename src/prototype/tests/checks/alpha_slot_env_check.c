#include "a_program/core/alpha_slot_env.h"

#include <stdio.h>
#include <string.h>

int main(void) {
	struct prototype_alpha_slot_env env;
	memset(&env, 0, sizeof(env));
	uint32_t slot;
	if (prototype_alpha_slot_lookup(&env, 10, &slot) != 0 ||
		prototype_alpha_slot_intern(&env, 10, &slot) != 0 || slot != 0 ||
		prototype_alpha_slot_intern(&env, 10, &slot) != 0 || slot != 0) return 1;
	struct prototype_alpha_slot_checkpoint checkpoint =
		prototype_alpha_slot_checkpoint(&env);
	if (prototype_alpha_slot_push(&env, 10) != 0 ||
		prototype_alpha_slot_lookup(&env, 10, &slot) != 1 || slot != 1 ||
		prototype_alpha_slot_intern(&env, 20, &slot) != 0 || slot != 2 ||
		prototype_alpha_slot_restore(&env, checkpoint) != 0 ||
		prototype_alpha_slot_lookup(&env, 20, &slot) != 0 ||
		prototype_alpha_slot_lookup(&env, 10, &slot) != 1 || slot != 0) return 2;

	memset(&env, 0, sizeof(env));
	for (uint32_t i = 0; i < PROTOTYPE_ALPHA_SLOT_CAPACITY; ++i) {
		if (prototype_alpha_slot_push(&env, i) != 0) return 3;
	}
	if (prototype_alpha_slot_push(&env, UINT32_MAX) == 0 ||
		prototype_alpha_slot_restore(
			&env, (struct prototype_alpha_slot_checkpoint) {
				.count = PROTOTYPE_ALPHA_SLOT_CAPACITY + 1,
				.next_slot = PROTOTYPE_ALPHA_SLOT_CAPACITY + 1
			}
		) == 0) return 4;
	puts("alpha slot environment checks passed");
	return 0;
}
