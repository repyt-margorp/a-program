#ifndef A_PROGRAM_PROTOTYPE_CORE_ALPHA_SLOT_ENV_H
#define A_PROGRAM_PROTOTYPE_CORE_ALPHA_SLOT_ENV_H

#include <stdint.h>

#define PROTOTYPE_ALPHA_SLOT_CAPACITY 512

struct prototype_alpha_slot_env {
	uint32_t identities[PROTOTYPE_ALPHA_SLOT_CAPACITY];
	uint32_t slots[PROTOTYPE_ALPHA_SLOT_CAPACITY];
	uint32_t count;
	uint32_t next_slot;
};

struct prototype_alpha_slot_checkpoint {
	uint32_t count;
	uint32_t next_slot;
};

int prototype_alpha_slot_lookup(
	const struct prototype_alpha_slot_env* env,
	uint32_t identity,
	uint32_t* p_slot
);

int prototype_alpha_slot_push(
	struct prototype_alpha_slot_env* env,
	uint32_t identity
);

struct prototype_alpha_slot_checkpoint prototype_alpha_slot_checkpoint(
	const struct prototype_alpha_slot_env* env
);

int prototype_alpha_slot_restore(
	struct prototype_alpha_slot_env* env,
	struct prototype_alpha_slot_checkpoint checkpoint
);

int prototype_alpha_slot_intern(
	struct prototype_alpha_slot_env* env,
	uint32_t identity,
	uint32_t* p_slot
);

#endif
