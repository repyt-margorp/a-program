#ifndef A_PROGRAM_PROTOTYPE_PRODUCER_MERGE_H
#define A_PROGRAM_PROTOTYPE_PRODUCER_MERGE_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/checker/module_set.h"
#include "a_program/producer/capsule.h"

#define PROTOTYPE_MERGE_PRODUCER_VERSION UINT32_C(1)
#define PROTOTYPE_MERGE_PRODUCER_PAYLOAD_FORMAT UINT32_C(1)

enum prototype_merge_producer_status {
	PROTOTYPE_MERGE_PRODUCER_COMPLETE = 1,
	PROTOTYPE_MERGE_PRODUCER_PAUSED = 2,
	PROTOTYPE_MERGE_PRODUCER_REJECTED = 3
};

struct prototype_merge_producer_session;

struct prototype_merge_producer_report {
	int status;
	int conflict_kind;
	uint64_t effort_used;
	size_t completed_pairs;
};

int prototype_merge_producer_create(
	const struct prototype_checked_module* const* modules,
	size_t module_count,
	const struct prototype_work_capsule_compatibility* identity,
	struct prototype_merge_producer_session** p_session
);

int prototype_merge_producer_restore(
	const struct prototype_work_capsule* capsule,
	const struct prototype_work_capsule_compatibility* expected,
	const struct prototype_checker_options* checker_options,
	struct prototype_merge_producer_session** p_session
);

int prototype_merge_producer_advance(
	struct prototype_merge_producer_session* session,
	uint64_t additional_effort,
	struct prototype_merge_producer_report* p_report
);

int prototype_merge_producer_make_capsule(
	const struct prototype_merge_producer_session* session,
	struct prototype_work_capsule* capsule
);

const struct prototype_checked_module_set* prototype_merge_producer_result(
	const struct prototype_merge_producer_session* session
);

void prototype_merge_producer_destroy(
	struct prototype_merge_producer_session* session
);

#endif
