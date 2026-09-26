#include "a_program/producer/effort.h"

#include <string.h>

void prototype_effort_account_init(
	struct prototype_effort_account* account,
	uint64_t limit
) {
	if (!account) return;
	memset(account, 0, sizeof(*account));
	account->cost_model_version = PROTOTYPE_EFFORT_COST_MODEL_VERSION;
	account->limit = limit;
	account->exhausted_phase = -1;
}

uint64_t prototype_effort_remaining(
	const struct prototype_effort_account* account
) {
	if (!account || account->cost_model_version !=
		PROTOTYPE_EFFORT_COST_MODEL_VERSION || account->used > account->limit) {
		return 0;
	}
	return account->limit - account->used;
}

int prototype_effort_add_credits(
	struct prototype_effort_account* account,
	uint64_t credits
) {
	if (!account || account->cost_model_version !=
			PROTOTYPE_EFFORT_COST_MODEL_VERSION ||
		UINT64_MAX - account->limit < credits) {
		return -1;
	}
	account->limit += credits;
	account->exhausted = 0;
	account->exhausted_phase = -1;
	return 0;
}

int prototype_effort_consume(
	struct prototype_effort_account* account,
	int phase,
	uint64_t credits
) {
	if (!account || account->cost_model_version !=
			PROTOTYPE_EFFORT_COST_MODEL_VERSION ||
		phase < 0 || phase >= PROTOTYPE_EFFORT_PHASE_COUNT ||
		UINT64_MAX - account->used < credits ||
		UINT64_MAX - account->phase_used[phase] < credits) {
		return -1;
	}
	if (credits > account->limit - account->used) {
		account->exhausted = 1;
		account->exhausted_phase = phase;
		return 1;
	}
	account->used += credits;
	account->phase_used[phase] += credits;
	return 0;
}

int prototype_effort_consume_split(
	struct prototype_effort_account* account,
	const uint64_t phase_credits[PROTOTYPE_EFFORT_PHASE_COUNT]
) {
	if (!account || !phase_credits || account->cost_model_version !=
			PROTOTYPE_EFFORT_COST_MODEL_VERSION) {
		return -1;
	}
	uint64_t total = 0;
	for (int phase = 0; phase < PROTOTYPE_EFFORT_PHASE_COUNT; ++phase) {
		if (UINT64_MAX - total < phase_credits[phase] ||
			UINT64_MAX - account->phase_used[phase] < phase_credits[phase]) {
			return -1;
		}
		total += phase_credits[phase];
	}
	if (UINT64_MAX - account->used < total) return -1;
	if (total > prototype_effort_remaining(account)) {
		account->exhausted = 1;
		account->exhausted_phase = -1;
		for (int phase = 0; phase < PROTOTYPE_EFFORT_PHASE_COUNT; ++phase) {
			if (phase_credits[phase] != 0) {
				account->exhausted_phase = phase;
				break;
			}
		}
		return 1;
	}
	account->used += total;
	for (int phase = 0; phase < PROTOTYPE_EFFORT_PHASE_COUNT; ++phase) {
		account->phase_used[phase] += phase_credits[phase];
	}
	return 0;
}

int prototype_effort_account_accumulate_snapshot(
	struct prototype_effort_account* target,
	const struct prototype_effort_account* source
) {
	if (!target || !source || target->cost_model_version !=
			PROTOTYPE_EFFORT_COST_MODEL_VERSION ||
		source->cost_model_version != PROTOTYPE_EFFORT_COST_MODEL_VERSION ||
		UINT64_MAX - target->limit < source->limit ||
		UINT64_MAX - target->used < source->used) {
		return -1;
	}
	for (size_t i = 0; i < PROTOTYPE_EFFORT_PHASE_COUNT; ++i) {
		if (UINT64_MAX - target->phase_used[i] < source->phase_used[i]) {
			return -1;
		}
	}
	target->limit += source->limit;
	target->used += source->used;
	for (size_t i = 0; i < PROTOTYPE_EFFORT_PHASE_COUNT; ++i) {
		target->phase_used[i] += source->phase_used[i];
	}
	if (source->exhausted) {
		target->exhausted = 1;
		target->exhausted_phase = source->exhausted_phase;
	}
	return 0;
}
