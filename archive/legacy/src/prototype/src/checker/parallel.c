#include "a_program/checker/parallel.h"

#include <pthread.h>
#include <stdlib.h>

struct parallel_check_worker {
	const struct prototype_parallel_check_task* tasks;
	size_t task_count;
	size_t worker_index;
	size_t worker_count;
	struct prototype_checked_module** checked;
	struct prototype_checker_report* reports;
	int failed;
};

static void* parallel_check_worker_run(void* argument) {
	struct parallel_check_worker* worker = argument;
	for (size_t i = worker->worker_index; i < worker->task_count;
		i += worker->worker_count) {
		if (prototype_checker_check_module(
				worker->tasks[i].module,
				&worker->tasks[i].options,
				&worker->checked[i],
				&worker->reports[i]
			) != 0) {
			worker->failed = 1;
			break;
		}
	}
	return NULL;
}

int prototype_checker_check_modules_parallel(
	const struct prototype_parallel_check_task* tasks,
	size_t task_count,
	size_t worker_count,
	struct prototype_checked_module** checked,
	struct prototype_checker_report* reports
) {
	if ((task_count != 0 && (!tasks || !checked || !reports)) ||
		worker_count == 0) {
		return -1;
	}
	if (task_count == 0) return 0;
	for (size_t i = 0; i < task_count; ++i) {
		for (size_t j = i + 1; tasks[i].options.effort && j < task_count; ++j) {
			if (tasks[i].options.effort == tasks[j].options.effort) {
				return -1;
			}
		}
	}
	if (worker_count > task_count) worker_count = task_count;
	pthread_t* threads = calloc(worker_count, sizeof(*threads));
	struct parallel_check_worker* workers = calloc(
		worker_count, sizeof(*workers)
	);
	if (!threads || !workers) {
		free(threads);
		free(workers);
		return -1;
	}
	for (size_t i = 0; i < task_count; ++i) checked[i] = NULL;
	size_t started = 0;
	for (size_t i = 0; i < worker_count; ++i) {
		workers[i] = (struct parallel_check_worker) {
			.tasks = tasks,
			.task_count = task_count,
			.worker_index = i,
			.worker_count = worker_count,
			.checked = checked,
			.reports = reports
		};
		if (pthread_create(
				&threads[i], NULL, parallel_check_worker_run, &workers[i]
			) != 0) {
			break;
		}
		started += 1;
	}
	int failed = started != worker_count;
	for (size_t i = 0; i < started; ++i) {
		if (pthread_join(threads[i], NULL) != 0 || workers[i].failed) {
			failed = 1;
		}
	}
	free(workers);
	free(threads);
	if (failed) {
		for (size_t i = 0; i < task_count; ++i) {
			prototype_checked_module_destroy(checked[i]);
			checked[i] = NULL;
		}
		return -1;
	}
	return 0;
}
