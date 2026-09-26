#include "a_program/producer/capsule.h"
#include "a_program/producer/effort.h"
#include "a_program/producer/goal.h"
#include "a_program/producer/incremental.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int files_equal(FILE* left, FILE* right) {
	if (fflush(left) != 0 || fflush(right) != 0 ||
		fseek(left, 0, SEEK_END) != 0 || fseek(right, 0, SEEK_END) != 0) return 0;
	long left_size = ftell(left);
	long right_size = ftell(right);
	if (left_size < 0 || left_size != right_size ||
		fseek(left, 0, SEEK_SET) != 0 || fseek(right, 0, SEEK_SET) != 0) return 0;
	for (long i = 0; i < left_size; ++i) {
		if (fgetc(left) != fgetc(right)) return 0;
	}
	return 1;
}

static int check_incremental_supports(void) {
	struct prototype_goal_key goal_a;
	struct prototype_goal_key goal_b;
	struct prototype_goal_key base;
	struct prototype_goal_key support_a1;
	struct prototype_goal_key support_a2;
	struct prototype_goal_key support_b;
	struct prototype_goal_key space;
	if (prototype_goal_key_make(1, "N", "A", NULL, 0, &goal_a) != 0 ||
		prototype_goal_key_make(1, "N", "B", NULL, 0, &goal_b) != 0 ||
		prototype_goal_key_make(1, "N", "Base", NULL, 0, &base) != 0 ||
		prototype_goal_key_make(2, "N", "A", "one", 3, &support_a1) != 0 ||
		prototype_goal_key_make(2, "N", "A", "two", 3, &support_a2) != 0 ||
		prototype_goal_key_make(2, "N", "B", NULL, 0, &support_b) != 0 ||
		prototype_goal_key_make(3, "N", "Space", NULL, 0, &space) != 0) {
		return -1;
	}
	struct prototype_incremental_goal goals[] = {
		{.key = goal_a, .content_fingerprint = {101, 0, 0, 0}},
		{.key = goal_b, .content_fingerprint = {103, 0, 0, 0}}
	};
	struct prototype_incremental_support supports[] = {
		{.goal = goal_a, .support = support_a1},
		{.goal = goal_a, .support = support_a2},
		{.goal = goal_b, .support = support_b}
	};
	struct prototype_incremental_positive_observation positive[] = {
		{.support = support_a1, .dependency = base,
			.observed_fingerprint = {107, 0, 0, 0}},
		{.support = support_a2, .dependency = base,
			.observed_fingerprint = {109, 0, 0, 0}},
		{.support = support_b, .dependency = goal_a,
			.observed_fingerprint = {101, 0, 0, 0}}
	};
	struct prototype_incremental_negative_observation negative[] = {
		{.support = support_a2, .candidate_space = space,
			.observed_sealed_fingerprint = {113, 0, 0, 0}}
	};
	struct prototype_incremental_goal current_goals[] = {
		{.key = base, .content_fingerprint = {109, 0, 0, 0}},
		{.key = goal_a, .content_fingerprint = {101, 0, 0, 0}},
		{.key = goal_b, .content_fingerprint = {103, 0, 0, 0}}
	};
	struct prototype_incremental_candidate_space spaces[] = {
		{.key = space, .sealed_fingerprint = {113, 0, 0, 0}}
	};
	struct prototype_incremental_graph graph = {
		.goals = goals, .goal_count = 2,
		.supports = supports, .support_count = 3,
		.positive = positive, .positive_count = 3,
		.negative = negative, .negative_count = 1
	};
	struct prototype_incremental_snapshot snapshot = {
		.goals = current_goals, .goal_count = 3,
		.candidate_spaces = spaces, .candidate_space_count = 1
	};
	unsigned char invalidated[2];
	if (prototype_incremental_compute_invalidation(
			&graph, &snapshot, invalidated, 2
		) != 0 || invalidated[0] || invalidated[1]) {
		return -1;
	}
	current_goals[0].content_fingerprint[0] = 127;
	if (prototype_incremental_compute_invalidation(
			&graph, &snapshot, invalidated, 2
		) != 0 || !invalidated[0] || !invalidated[1]) {
		return -1;
	}
	return 0;
}

int main(void) {
	if (check_incremental_supports() != 0) {
		fprintf(stderr, "incremental support invalidation failed\n");
		return 1;
	}
	struct prototype_goal_key first_goal;
	struct prototype_goal_key same_goal;
	struct prototype_goal_key other_goal;
	uint32_t path[] = {1, 4, 2};
	if (prototype_goal_key_make(
			PROTOTYPE_WORK_CAPSULE_CLASSIFIER, "Example", "value",
			path, sizeof(path), &first_goal
		) != 0 || prototype_goal_key_make(
			PROTOTYPE_WORK_CAPSULE_CLASSIFIER, "Example", "value",
			path, sizeof(path), &same_goal
		) != 0 || prototype_goal_key_make(
			PROTOTYPE_WORK_CAPSULE_NORMALIZER, "Example", "value",
			path, sizeof(path), &other_goal
		) != 0 || !prototype_goal_key_equal(&first_goal, &same_goal) ||
		prototype_goal_key_equal(&first_goal, &other_goal)) {
		fprintf(stderr, "stable GoalKey boundary failed\n");
		return 1;
	}
	struct prototype_work_capsule capsule;
	struct prototype_work_capsule decoded;
	prototype_work_capsule_init(&capsule);
	prototype_work_capsule_init(&decoded);
	capsule.producer_kind = PROTOTYPE_WORK_CAPSULE_CLASSIFIER;
	capsule.producer_version = 3;
	capsule.cost_model_version = PROTOTYPE_EFFORT_COST_MODEL_VERSION;
	capsule.calculus_fingerprint = UINT64_C(41);
	capsule.intrinsic_fingerprint = UINT64_C(43);
	capsule.base_revision[0] = UINT64_C(47);
	capsule.goal_key[0] = UINT64_C(53);
	capsule.dependency_count = 1;
	capsule.dependencies = calloc(1, sizeof(*capsule.dependencies));
	capsule.negative_observation_count = 1;
	capsule.negative_observations = calloc(
		1, sizeof(*capsule.negative_observations)
	);
	capsule.payload_format = 7;
	capsule.payload_size = 5;
	capsule.payload = malloc(capsule.payload_size);
	if (!capsule.dependencies || !capsule.negative_observations ||
		!capsule.payload) return 1;
	capsule.dependencies[0] = (struct prototype_work_capsule_dependency) {
		.goal_key = {UINT64_C(57), 0, 0, 0},
		.checked_content_fingerprint = {UINT64_C(59), 0, 0, 0}
	};
	capsule.negative_observations[0] =
		(struct prototype_work_capsule_negative_observation) {
			.candidate_space_key = {UINT64_C(61), 0, 0, 0},
			.sealed_content_fingerprint = {UINT64_C(67), 0, 0, 0}
	};
	memcpy(capsule.payload, "front", capsule.payload_size);
	FILE* first = tmpfile();
	FILE* second = tmpfile();
	if (!first || !second || prototype_work_capsule_write(first, &capsule) != 0 ||
		prototype_work_capsule_write(second, &capsule) != 0 ||
		!files_equal(first, second) || fseek(first, 0, SEEK_SET) != 0 ||
		prototype_work_capsule_read(first, &decoded) != 0) {
		fprintf(stderr, "capsule canonical round trip failed\n");
		return 1;
	}
	struct prototype_work_capsule_compatibility expected = {
		.producer_kind = capsule.producer_kind,
		.producer_version = capsule.producer_version,
		.cost_model_version = capsule.cost_model_version,
		.calculus_fingerprint = capsule.calculus_fingerprint,
		.intrinsic_fingerprint = capsule.intrinsic_fingerprint,
		.base_revision = { UINT64_C(47), 0, 0, 0 },
		.goal_key = { UINT64_C(53), 0, 0, 0 }
	};
	if (!prototype_work_capsule_is_compatible(&decoded, &expected) ||
		decoded.dependency_count != 1 ||
		decoded.dependencies[0].goal_key[0] != UINT64_C(57) ||
		decoded.negative_observation_count != 1 ||
		decoded.negative_observations[0].candidate_space_key[0] != UINT64_C(61) ||
		decoded.payload_size != capsule.payload_size ||
		memcmp(decoded.payload, capsule.payload, capsule.payload_size) != 0) {
		fprintf(stderr, "compatible capsule was rejected\n");
		return 1;
	}
	expected.cost_model_version += 1;
	if (prototype_work_capsule_is_compatible(&decoded, &expected)) {
		fprintf(stderr, "stale capsule was accepted\n");
		return 1;
	}
	if (fseek(second, -9, SEEK_END) != 0) return 1;
	int byte = fgetc(second);
	if (byte == EOF || fseek(second, -1, SEEK_CUR) != 0 ||
		fputc(byte ^ 1, second) == EOF || fflush(second) != 0 ||
		fseek(second, 0, SEEK_SET) != 0) return 1;
	struct prototype_work_capsule corrupt;
	prototype_work_capsule_init(&corrupt);
	if (prototype_work_capsule_read(second, &corrupt) == 0) {
		fprintf(stderr, "corrupt capsule was accepted\n");
		return 1;
	}
	prototype_work_capsule_destroy(&corrupt);
	prototype_work_capsule_destroy(&decoded);
	prototype_work_capsule_destroy(&capsule);
	fclose(second);
	fclose(first);
	puts("work capsule check passed");
	return 0;
}
