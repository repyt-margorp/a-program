#ifndef A_PROGRAM_PROTOTYPE_PRODUCER_CAPSULE_H
#define A_PROGRAM_PROTOTYPE_PRODUCER_CAPSULE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define PROTOTYPE_WORK_CAPSULE_VERSION UINT32_C(2)

enum prototype_work_capsule_producer_kind {
	PROTOTYPE_WORK_CAPSULE_CLASSIFIER = 1,
	PROTOTYPE_WORK_CAPSULE_NORMALIZER = 2,
	PROTOTYPE_WORK_CAPSULE_MOTIVE = 3,
	PROTOTYPE_WORK_CAPSULE_PROOF = 4,
	PROTOTYPE_WORK_CAPSULE_FUNCTION_GRAPH = 5,
	PROTOTYPE_WORK_CAPSULE_FRAGMENT_MERGE = 6
};

struct prototype_work_capsule_dependency {
	uint64_t goal_key[4];
	uint64_t checked_content_fingerprint[4];
};

struct prototype_work_capsule_negative_observation {
	uint64_t candidate_space_key[4];
	uint64_t sealed_content_fingerprint[4];
};

/* Untrusted scheduling state. A producer-kind codec owns payload semantics;
 * pointers and checked capabilities are never valid wire payloads. */
struct prototype_work_capsule {
	uint32_t capsule_version;
	uint32_t producer_kind;
	uint32_t producer_version;
	uint32_t cost_model_version;
	uint64_t calculus_fingerprint;
	uint64_t intrinsic_fingerprint;
	uint64_t base_revision[4];
	uint64_t goal_key[4];
	struct prototype_work_capsule_dependency* dependencies;
	size_t dependency_count;
	struct prototype_work_capsule_negative_observation* negative_observations;
	size_t negative_observation_count;
	uint32_t payload_format;
	unsigned char* payload;
	size_t payload_size;
};

struct prototype_work_capsule_compatibility {
	uint32_t producer_kind;
	uint32_t producer_version;
	uint32_t cost_model_version;
	uint64_t calculus_fingerprint;
	uint64_t intrinsic_fingerprint;
	uint64_t base_revision[4];
	uint64_t goal_key[4];
};

void prototype_work_capsule_init(struct prototype_work_capsule* capsule);
void prototype_work_capsule_destroy(struct prototype_work_capsule* capsule);
int prototype_work_capsule_write(
	FILE* stream,
	const struct prototype_work_capsule* capsule
);
int prototype_work_capsule_read(
	FILE* stream,
	struct prototype_work_capsule* capsule
);
int prototype_work_capsule_is_compatible(
	const struct prototype_work_capsule* capsule,
	const struct prototype_work_capsule_compatibility* expected
);

#endif
