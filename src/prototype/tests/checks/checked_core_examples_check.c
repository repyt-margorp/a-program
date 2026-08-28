#include "a_program/checker/module.h"
#include "a_program/checker/session.h"
#include "a_program/driver/compiler_session.h"
#include "a_program/frontend/reader.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/graph/occurrence_usage.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t checked_core_time_ns;
static uint64_t accepted_replay_time_ns;
static uint64_t producer_effort_by_phase[PROTOTYPE_EFFORT_PHASE_COUNT];

static uint64_t test_clock_ns(void) {
	struct timespec now;
	if (timespec_get(&now, TIME_UTC) != TIME_UTC) {
		return 0;
	}
	return (uint64_t)now.tv_sec * UINT64_C(1000000000) +
		(uint64_t)now.tv_nsec;
}

static void print_checker_failure(
	const char* path,
	const struct prototype_elaborated_module* module,
	const struct prototype_checker_report* report
) {
	fprintf(
		stderr,
		"%s: checker stopped status=%d reason=%d subject=%u effort=%llu\n",
		path,
		report->status,
		report->stop_reason,
		report->subject,
		(unsigned long long)report->effort_used
	);
	if (report->stop_reason == PROTOTYPE_CHECKER_STOP_CONTEXT) {
		if (report->subject >= module->view.contexts.context_count) {
			fprintf(stderr, "checker subject domain=context invalid-id\n");
			return;
		}
		const struct prototype_semantic_context* context =
			&module->view.contexts.contexts[report->subject];
		fprintf(
			stderr,
			"checker subject domain=context parent=%u binding=%u "
			"classifier=%u extension=%d producer-core=%u\n",
			context->parent,
			context->binding_id,
			context->classifier,
			context->extension_kind,
			context->producer_computation
		);
		return;
	}
	if (report->stop_reason != PROTOTYPE_CHECKER_STOP_OCCURRENCE) {
		fprintf(stderr, "checker subject domain=reason-specific\n");
		return;
	}
	if (report->subject >= module->view.occurrences.occurrence_count) {
		fprintf(stderr, "checker subject domain=occurrence invalid-id\n");
		return;
	}
	const struct prototype_semantic_occurrence* occurrence =
		&module->view.occurrences.occurrences[report->subject];
	const struct prototype_term* core =
		occurrence->core_term < module->view.terms.term_count ?
			&module->view.terms.terms[occurrence->core_term] : NULL;
	const struct prototype_term* classifier =
		occurrence->asserted_classifier < module->view.terms.term_count ?
			&module->view.terms.terms[occurrence->asserted_classifier] : NULL;
	fprintf(
		stderr,
		"checker subject domain=occurrence kind=%d core=%u:%d "
		"classifier=%u:%d origin-core=%u role=%d evidence=%d wrapped=%u\n",
		occurrence->kind,
		occurrence->core_term,
		core ? core->tag : -1,
		occurrence->asserted_classifier,
		classifier ? classifier->tag : -1,
		occurrence->origin_core_term,
		occurrence->application_role,
		occurrence->classifier_evidence_kind,
		occurrence->wrapped_occurrence
	);
}

static int compare_resource_usage(
	const char* path,
	const struct prototype_program_storage* storage,
	const struct prototype_checked_module* checked
) {
	size_t solution_count = storage->metadata.typed_occurrences.occurrence_count;
	struct prototype_occurrence_usage_solution* solutions = solution_count == 0 ?
		NULL : calloc(solution_count, sizeof(*solutions));
	struct prototype_usage_entry* entries = malloc(
		PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY * sizeof(*entries)
	);
	size_t entry_count = 0;
	if ((solution_count != 0 && !solutions) || !entries ||
		prototype_occurrence_usage_solve(
			&storage->metadata.typed_occurrences,
			&storage->terms,
			&storage->metadata.contexts,
			solutions,
			solution_count,
			entries,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY,
			&entry_count
		) != 0) {
		fprintf(stderr, "%s: producer resource solver failed\n", path);
		free(solutions);
		free(entries);
		return -1;
	}
	for (uint32_t occurrence_id = 0;
		occurrence_id < solution_count;
		++occurrence_id) {
		struct prototype_usage_vector producer;
		const struct prototype_usage_entry* checker_entries;
		size_t checker_entry_count;
		int checker_binder_usage;
		if (prototype_occurrence_usage_solution_view(
				solutions,
				solution_count,
				entries,
				entry_count,
				occurrence_id,
				&producer
			) != 0 || prototype_checked_module_occurrence_usage(
				checked,
				occurrence_id,
				&checker_entries,
				&checker_entry_count,
				&checker_binder_usage
			) != 0 || solutions[occurrence_id].binder_usage !=
				checker_binder_usage || producer.count != checker_entry_count ||
			(checker_entry_count != 0 && memcmp(
				producer.entries,
				checker_entries,
				checker_entry_count * sizeof(*checker_entries)
			) != 0)) {
			fprintf(
				stderr,
				"%s: resource usage mismatch at occurrence %u\n",
				path,
				occurrence_id
			);
			free(solutions);
			free(entries);
			return -1;
		}
	}
	free(solutions);
	free(entries);
	return 0;
}

static int check_file(const char* path) {
	struct prototype_program_storage storage;
	struct prototype_read_error error;
	struct prototype_frozen_module_snapshot snapshot;
	struct prototype_elaborated_module module;
	struct prototype_checked_module* checked = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	int result = -1;
	int storage_initialized = 0;

	memset(&error, 0, sizeof(error));
	prototype_effort_account_init(&effort, UINT64_C(1000000));
	prototype_elaborated_module_init(&module);
	if (prototype_program_storage_init(&storage) != 0) {
		return -1;
	}
	storage_initialized = 1;
	if (prototype_read_file(path, &storage.program, &error) != 0) {
		fprintf(stderr, "%s: compile failed: %s\n", path, error.message);
		goto cleanup;
	}
	if (prototype_compile_metadata_frozen_snapshot(
			&storage.metadata, &snapshot
		) != 0 || prototype_elaborated_module_project(
			&storage.symbols,
			&storage.terms,
			&storage.type_declarations.semantic_schema,
			storage.program.intrinsic_environment,
			&storage.universe,
			&snapshot,
			&module
		) != 0) {
		fprintf(stderr, "%s: semantic projection failed\n", path);
		goto cleanup;
	}
	if (storage.judgement.accepted_replay_stats.validation_count == 0) {
		fprintf(stderr, "%s: compile skipped the v86 replay oracle\n", path);
		goto cleanup;
	}
	uint64_t producer_effort_sum = 0;
	for (size_t i = 0; i < PROTOTYPE_EFFORT_PHASE_COUNT; ++i) {
		producer_effort_sum += storage.metadata.effort.phase_used[i];
		producer_effort_by_phase[i] += storage.metadata.effort.phase_used[i];
	}
	if (producer_effort_sum != storage.metadata.effort.used ||
		storage.metadata.effort.cost_model_version !=
			PROTOTYPE_EFFORT_COST_MODEL_VERSION) {
		fprintf(stderr, "%s: typed producer effort accounting mismatch\n", path);
		goto cleanup;
	}
	accepted_replay_time_ns += storage.metadata.accepted_replay_time_ns;
	uint64_t checker_start = test_clock_ns();
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
		print_checker_failure(path, &module, &report);
		goto cleanup;
	}
	uint64_t checker_end = test_clock_ns();
	if (checker_end >= checker_start) {
		checked_core_time_ns += checker_end - checker_start;
	}
	if (compare_resource_usage(path, &storage, checked) != 0) {
		goto cleanup;
	}
	prototype_checked_module_destroy(checked);
	checked = NULL;
	prototype_program_storage_destroy(&storage);
	storage_initialized = 0;
	if (prototype_checker_check_module(
			&module.view, &options, &checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
		fprintf(
			stderr,
			"%s: proof-graph-erased recheck failed status=%d reason=%d\n",
			path,
			report.status,
			report.stop_reason
		);
		goto cleanup;
	}
	result = 0;

cleanup:
	prototype_checked_module_destroy(checked);
	prototype_elaborated_module_destroy(&module);
	if (storage_initialized) {
		prototype_program_storage_destroy(&storage);
	}
	return result;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		return 1;
	}
	for (int i = 1; i < argc; ++i) {
		if (check_file(argv[i]) != 0) {
			return 1;
		}
	}
	if (producer_effort_by_phase[PROTOTYPE_EFFORT_PHASE_GRAPH] == 0 ||
		producer_effort_by_phase[PROTOTYPE_EFFORT_PHASE_CLASSIFIER] == 0 ||
		producer_effort_by_phase[PROTOTYPE_EFFORT_PHASE_NORMALIZATION] == 0 ||
		producer_effort_by_phase[PROTOTYPE_EFFORT_PHASE_PROOF] == 0 ||
		producer_effort_by_phase[PROTOTYPE_EFFORT_PHASE_FUNCTION_GRAPH] == 0) {
		fprintf(stderr, "producer effort phases were not independently charged\n");
		return 1;
	}
	fprintf(
		stderr,
		"A_PROGRAM_CHECKED_CORE_TIMING files=%d accepted_replay_ns=%llu "
		"checker_ns=%llu\n",
		argc - 1,
		(unsigned long long)accepted_replay_time_ns,
		(unsigned long long)checked_core_time_ns
	);
	return 0;
}
