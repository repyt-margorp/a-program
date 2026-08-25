#ifndef A_PROGRAM_PROTOTYPE_CHECKER_PARALLEL_H
#define A_PROGRAM_PROTOTYPE_CHECKER_PARALLEL_H

#include <stddef.h>

#include "a_program/checker/session.h"

struct prototype_parallel_check_task {
	const struct prototype_elaborated_module_view* module;
	struct prototype_checker_options options;
};

/* Each task owns a distinct effort account and an immutable module view. The
 * function never permits workers to share mutable semantic stores. Results
 * retain input order; scheduling order is not semantic output. */
int prototype_checker_check_modules_parallel(
	const struct prototype_parallel_check_task* tasks,
	size_t task_count,
	size_t worker_count,
	struct prototype_checked_module** checked,
	struct prototype_checker_report* reports
);

#endif
