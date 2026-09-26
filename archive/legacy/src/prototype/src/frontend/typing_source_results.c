#include "a_program/frontend/typing_source_results.h"

#include <string.h>

#include "a_program/support/schema.h"

void prototype_typing_source_assignment_result_clear(
	struct prototype_typing_source_assignment_result* result
) {
	if (!result) {
		return;
	}
	memset(result, 0, sizeof(*result));
	result->classifier = PROTOTYPE_INVALID_ID;
	result->occurrence = PROTOTYPE_INVALID_ID;
}

static void typing_source_expectation_result_clear(
	struct prototype_typing_source_expectation_result* result
) {
	memset(result, 0, sizeof(*result));
	result->classifier = PROTOTYPE_INVALID_ID;
	result->checked_occurrence = PROTOTYPE_INVALID_ID;
	result->synthesized_classifier = PROTOTYPE_INVALID_ID;
}

static void typing_source_type_result_clear(
	struct prototype_typing_source_type_result* result
) {
	memset(result, 0, sizeof(*result));
	result->type_declaration_id = PROTOTYPE_INVALID_ID;
}

int prototype_typing_source_results_init(
	struct prototype_typing_source_results* results,
	struct prototype_typing_source_assignment_result* assignments,
	size_t assignment_count,
	struct prototype_typing_source_expectation_result* expectations,
	size_t expectation_count,
	struct prototype_typing_source_type_result* types,
	size_t type_count
) {
	if (!results || (assignment_count > 0 && !assignments) ||
		(expectation_count > 0 && !expectations) || (type_count > 0 && !types)) {
		return -1;
	}
	*results = (struct prototype_typing_source_results) {
		.assignments = assignments,
		.assignment_count = assignment_count,
		.expectations = expectations,
		.expectation_count = expectation_count,
		.types = types,
		.type_count = type_count
	};
	for (size_t i = 0; i < assignment_count; ++i) {
		prototype_typing_source_assignment_result_clear(&assignments[i]);
	}
	for (size_t i = 0; i < expectation_count; ++i) {
		typing_source_expectation_result_clear(&expectations[i]);
	}
	for (size_t i = 0; i < type_count; ++i) {
		typing_source_type_result_clear(&types[i]);
	}
	return 0;
}

struct prototype_typing_source_assignment_result*
prototype_typing_source_assignment_result_mut(
	struct prototype_typing_source_results* results,
	uint32_t assignment_id
) {
	return results && assignment_id < results->assignment_count ?
		&results->assignments[assignment_id] : NULL;
}

const struct prototype_typing_source_assignment_result*
prototype_typing_source_assignment_result_get(
	const struct prototype_typing_source_results* results,
	uint32_t assignment_id
) {
	return results && assignment_id < results->assignment_count ?
		&results->assignments[assignment_id] : NULL;
}

struct prototype_typing_source_expectation_result*
prototype_typing_source_expectation_result_mut(
	struct prototype_typing_source_results* results,
	uint32_t expectation_id
) {
	return results && expectation_id < results->expectation_count ?
		&results->expectations[expectation_id] : NULL;
}

const struct prototype_typing_source_expectation_result*
prototype_typing_source_expectation_result_get(
	const struct prototype_typing_source_results* results,
	uint32_t expectation_id
) {
	return results && expectation_id < results->expectation_count ?
		&results->expectations[expectation_id] : NULL;
}

struct prototype_typing_source_type_result*
prototype_typing_source_type_result_mut(
	struct prototype_typing_source_results* results,
	uint32_t type_id
) {
	return results && type_id < results->type_count ?
		&results->types[type_id] : NULL;
}
