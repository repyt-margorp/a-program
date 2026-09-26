#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_SOURCE_RESULTS_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_SOURCE_RESULTS_H

#include <stddef.h>
#include <stdint.h>

enum prototype_typing_source_state {
	PROTOTYPE_TYPING_SOURCE_UNCHECKED = 0,
	PROTOTYPE_TYPING_SOURCE_CHECKING = 1,
	PROTOTYPE_TYPING_SOURCE_CHECKED = 2
};

/* Layer T result for one assignment occurrence. Core syntax is referenced by
 * the separate C0 handoff and is never copied into this record. */
struct prototype_typing_source_assignment_result {
	uint32_t classifier;
	uint32_t occurrence;
	int state;
	int published;
};

struct prototype_typing_source_expectation_result {
	uint32_t classifier;
	uint32_t checked_occurrence;
	uint32_t synthesized_classifier;
	int state;
	int validation_state;
};

struct prototype_typing_source_type_result {
	uint32_t type_declaration_id;
	int state;
};

struct prototype_typing_source_results {
	struct prototype_typing_source_assignment_result* assignments;
	size_t assignment_count;
	struct prototype_typing_source_expectation_result* expectations;
	size_t expectation_count;
	struct prototype_typing_source_type_result* types;
	size_t type_count;
};

int prototype_typing_source_results_init(
	struct prototype_typing_source_results* results,
	struct prototype_typing_source_assignment_result* assignments,
	size_t assignment_count,
	struct prototype_typing_source_expectation_result* expectations,
	size_t expectation_count,
	struct prototype_typing_source_type_result* types,
	size_t type_count
);

struct prototype_typing_source_assignment_result*
prototype_typing_source_assignment_result_mut(
	struct prototype_typing_source_results* results,
	uint32_t assignment_id
);

const struct prototype_typing_source_assignment_result*
prototype_typing_source_assignment_result_get(
	const struct prototype_typing_source_results* results,
	uint32_t assignment_id
);

struct prototype_typing_source_expectation_result*
prototype_typing_source_expectation_result_mut(
	struct prototype_typing_source_results* results,
	uint32_t expectation_id
);

const struct prototype_typing_source_expectation_result*
prototype_typing_source_expectation_result_get(
	const struct prototype_typing_source_results* results,
	uint32_t expectation_id
);

struct prototype_typing_source_type_result*
prototype_typing_source_type_result_mut(
	struct prototype_typing_source_results* results,
	uint32_t type_id
);

void prototype_typing_source_assignment_result_clear(
	struct prototype_typing_source_assignment_result* result
);

#endif
