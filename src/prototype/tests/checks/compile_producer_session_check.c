#include "a_program/driver/compiler_session.h"
#include "a_program/frontend/reader.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	static const char source[] =
		"Bool := @{ false : *; true : *; };\n"
		"id := \\b : Bool => b;\n"
		"main := id Bool.true;\n";
	struct prototype_program_storage storage;
	struct prototype_program_storage uninterrupted;
	struct prototype_read_error error;
	struct prototype_compile_producer_session* session = NULL;
	struct prototype_compile_producer_report report;
	int result = 1;
	int uninterrupted_initialized = 0;
	struct prototype_effort_account split_account;
	uint64_t split_credits[PROTOTYPE_EFFORT_PHASE_COUNT] = { 0 };
	split_credits[PROTOTYPE_EFFORT_PHASE_GRAPH] = 1;
	split_credits[PROTOTYPE_EFFORT_PHASE_PROOF] = 1;
	prototype_effort_account_init(&split_account, 1);
	if (prototype_effort_consume_split(&split_account, split_credits) != 1 ||
		split_account.used != 0 || prototype_effort_add_credits(
			&split_account, 1
		) != 0 || prototype_effort_consume_split(
			&split_account, split_credits
		) != 0 || split_account.used != 2) {
		fprintf(stderr, "split effort charge was not atomic\n");
		return 1;
	}
	memset(&error, 0, sizeof(error));
	if (prototype_program_storage_init(&storage) != 0 ||
		prototype_read_ast_string(
			"<producer-session>", source, &storage.program, &error
		) != 0) {
		fprintf(stderr, "producer session setup failed: %s\n", error.message);
		return 1;
	}
	if (prototype_program_storage_init(&uninterrupted) != 0 ||
		prototype_read_ast_string(
			"<producer-uninterrupted>", source, &uninterrupted.program, &error
		) != 0) {
		fprintf(stderr, "uninterrupted producer setup failed: %s\n", error.message);
		prototype_program_storage_destroy(&storage);
		return 1;
	}
	uninterrupted_initialized = 1;
	prototype_effort_account_init(&storage.metadata.effort, 0);
	if (prototype_compile_producer_session_create(
			&storage.asts,
			&storage.terms,
			&storage.type_declarations,
			&storage.judgement,
			&storage.metadata,
			&storage.symbols,
			storage.program.intrinsic_environment,
			storage.program.namespace_symbol_id,
			NULL,
			0,
			&session
		) != 0 || prototype_compile_producer_session_advance(
			session, 0, &report
		) != 0 || report.status != PROTOTYPE_COMPILE_PRODUCER_PAUSED ||
		report.phase != 0 || storage.metadata.typed_occurrences.transaction_active) {
		fprintf(stderr, "producer did not pause before its atomic graph phase\n");
		goto cleanup;
	}

	int saw_classifier_pause = 0;
	for (uint32_t round = 0; round < 10000; ++round) {
		uint64_t credit = report.phase >= 2 ? UINT64_C(1000000) : 1;
		if (prototype_compile_producer_session_advance(
				session, credit, &report
			) != 0) {
			fprintf(stderr, "producer advance API failed at round %u\n", round);
			goto cleanup;
		}
		if (report.status == PROTOTYPE_COMPILE_PRODUCER_REJECTED) {
			fprintf(stderr, "producer rejected a valid program at phase %d\n", report.phase);
			goto cleanup;
		}
		if (report.status == PROTOTYPE_COMPILE_PRODUCER_PAUSED && report.phase == 1) {
			saw_classifier_pause = 1;
		}
		if (report.status == PROTOTYPE_COMPILE_PRODUCER_COMPLETE) break;
	}
	if (report.status != PROTOTYPE_COMPILE_PRODUCER_COMPLETE ||
		!saw_classifier_pause || report.phase != 3 ||
		storage.metadata.typed_occurrences.transaction_active ||
		!storage.metadata.typed_occurrences.frozen) {
		fprintf(stderr, "producer did not preserve and complete its frontier\n");
		goto cleanup;
	}
	uint64_t phase_sum = 0;
	for (int phase = 0; phase < PROTOTYPE_EFFORT_PHASE_COUNT; ++phase) {
		phase_sum += storage.metadata.effort.phase_used[phase];
	}
	if (phase_sum != storage.metadata.effort.used ||
		storage.metadata.effort.phase_used[PROTOTYPE_EFFORT_PHASE_GRAPH] == 0 ||
		storage.metadata.effort.phase_used[PROTOTYPE_EFFORT_PHASE_CLASSIFIER] == 0 ||
		storage.metadata.effort.phase_used[PROTOTYPE_EFFORT_PHASE_PROOF] == 0) {
		fprintf(stderr, "producer effort accounting is incomplete\n");
		goto cleanup;
	}
	prototype_effort_account_init(
		&uninterrupted.metadata.effort, UINT64_C(1000000)
	);
	if (prototype_ast_compile_pending_with_imports(
			&uninterrupted.asts,
			&uninterrupted.terms,
			&uninterrupted.type_declarations,
			&uninterrupted.judgement,
			&uninterrupted.metadata,
			&uninterrupted.symbols,
			uninterrupted.program.intrinsic_environment,
			uninterrupted.program.namespace_symbol_id,
			NULL,
			0
		) != 0 || storage.terms.term_count != uninterrupted.terms.term_count ||
		storage.terms.case_count != uninterrupted.terms.case_count ||
		storage.metadata.contexts.context_count !=
			uninterrupted.metadata.contexts.context_count ||
		storage.metadata.substitutions.substitution_count !=
			uninterrupted.metadata.substitutions.substitution_count ||
		storage.metadata.typed_occurrences.occurrence_count !=
			uninterrupted.metadata.typed_occurrences.occurrence_count ||
		storage.metadata.typed_occurrences.edge_count !=
			uninterrupted.metadata.typed_occurrences.edge_count ||
		storage.metadata.typed_occurrences.case_count !=
			uninterrupted.metadata.typed_occurrences.case_count ||
		memcmp(
			storage.terms.terms,
			uninterrupted.terms.terms,
			storage.terms.term_count * sizeof(*storage.terms.terms)
		) != 0 || memcmp(
			storage.metadata.typed_occurrences.occurrences,
			uninterrupted.metadata.typed_occurrences.occurrences,
			storage.metadata.typed_occurrences.occurrence_count *
				sizeof(*storage.metadata.typed_occurrences.occurrences)
		) != 0 || memcmp(
			storage.metadata.typed_occurrences.edges,
			uninterrupted.metadata.typed_occurrences.edges,
			storage.metadata.typed_occurrences.edge_count *
				sizeof(*storage.metadata.typed_occurrences.edges)
		) != 0) {
		fprintf(stderr, "split and uninterrupted producer content differs\n");
		goto cleanup;
	}
	result = 0;

cleanup:
	prototype_compile_producer_session_destroy(session);
	if (uninterrupted_initialized) {
		prototype_program_storage_destroy(&uninterrupted);
	}
	prototype_program_storage_destroy(&storage);
	if (result == 0) puts("compile producer session check passed");
	return result;
}
