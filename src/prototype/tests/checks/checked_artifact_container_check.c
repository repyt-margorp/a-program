#include "a_program/checker/container.h"
#include "a_program/driver/compiler_session.h"
#include "a_program/frontend/reader.h"
#include "a_program/graph/compile_metadata.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FNV_OFFSET UINT64_C(1469598103934665603)
#define TEST_FNV_PRIME UINT64_C(1099511628211)

static uint64_t test_hash(const unsigned char* bytes, size_t count) {
	uint64_t hash = TEST_FNV_OFFSET;
	for (size_t i = 0; i < count; ++i) {
		hash ^= bytes[i];
		hash *= TEST_FNV_PRIME;
	}
	return hash;
}

static void test_u32(unsigned char* bytes, uint32_t value) {
	for (uint32_t i = 0; i < 4; ++i) bytes[i] = (unsigned char)(value >> (i * 8));
}

static void test_u64(unsigned char* bytes, uint64_t value) {
	for (uint32_t i = 0; i < 8; ++i) bytes[i] = (unsigned char)(value >> (i * 8));
}

static uint64_t test_read_u64(const unsigned char* bytes) {
	uint64_t value = 0;
	for (uint32_t i = 0; i < 8; ++i) value |= (uint64_t)bytes[i] << (i * 8);
	return value;
}

static int read_file_bytes(FILE* file, unsigned char** p_bytes, size_t* p_count) {
	if (!file || !p_bytes || !p_count || fflush(file) != 0 ||
		fseek(file, 0, SEEK_END) != 0) return -1;
	long end = ftell(file);
	if (end < 0 || fseek(file, 0, SEEK_SET) != 0) return -1;
	size_t count = (size_t)end;
	unsigned char* bytes = count == 0 ? NULL : malloc(count);
	if ((count != 0 && !bytes) ||
		(count != 0 && fread(bytes, 1, count, file) != count)) {
		free(bytes);
		return -1;
	}
	*p_bytes = bytes;
	*p_count = count;
	return 0;
}

static int write_file_bytes(FILE* file, const unsigned char* bytes, size_t count) {
	return file && fwrite(bytes, 1, count, file) == count &&
		fflush(file) == 0 && fseek(file, 0, SEEK_SET) == 0 ? 0 : -1;
}

static int project_fixture(
	struct prototype_program_storage* storage,
	struct prototype_elaborated_module* module
) {
	static const char path[] =
		"src/prototype/tests/fixtures/typing/"
		"function_graph_generated_length_check.p";
	struct prototype_read_error error;
	struct prototype_frozen_module_snapshot snapshot;
	memset(&error, 0, sizeof(error));
	if (prototype_program_storage_init(storage) != 0 ||
		prototype_read_file(path, &storage->program, &error) != 0 ||
		prototype_compile_metadata_frozen_snapshot(
			&storage->metadata, &snapshot
		) != 0 || prototype_elaborated_module_project(
			&storage->symbols,
			&storage->terms,
			&storage->type_declarations.semantic_schema,
			storage->program.intrinsic_environment,
			&storage->universe,
			&snapshot,
			module
		) != 0) {
		fprintf(stderr, "checked artifact fixture projection failed: %s\n", error.message);
		return -1;
	}
	return 0;
}

static int expect_read_rejected(
	const unsigned char* bytes,
	size_t count,
	const struct prototype_checker_options* options
) {
	FILE* file = tmpfile();
	struct prototype_elaborated_module module;
	struct prototype_checked_module* checked = NULL;
	struct prototype_checker_report report;
	if (!file || write_file_bytes(file, bytes, count) != 0) {
		if (file) fclose(file);
		return -1;
	}
	int read_result = prototype_checked_artifact_read(
		file, options, &module, &checked, &report
	);
	prototype_checked_module_destroy(checked);
	if (read_result == 0) prototype_elaborated_module_destroy(&module);
	fclose(file);
	return read_result != 0 ? 0 : -1;
}

static int expect_canonical_rewrite(
	const struct prototype_checked_module* checked,
	const unsigned char* expected,
	size_t expected_count
) {
	FILE* file = tmpfile();
	unsigned char* bytes = NULL;
	size_t count = 0;
	int result = file && prototype_checked_artifact_write(file, checked) == 0 &&
		read_file_bytes(file, &bytes, &count) == 0 && count == expected_count &&
		memcmp(bytes, expected, count) == 0 ? 0 : -1;
	free(bytes);
	if (file) fclose(file);
	return result;
}

int main(void) {
	struct prototype_program_storage storage;
	struct prototype_elaborated_module source;
	struct prototype_elaborated_module decoded;
	struct prototype_checked_module* source_checked = NULL;
	struct prototype_checked_module* decoded_checked = NULL;
	struct prototype_effort_account effort;
	struct prototype_checker_options options = {
		.effort = &effort
	};
	struct prototype_checker_report report;
	FILE* first = NULL;
	FILE* second = NULL;
	FILE* with_capsule = NULL;
	unsigned char* first_bytes = NULL;
	unsigned char* second_bytes = NULL;
	size_t first_count = 0;
	size_t second_count = 0;
	int storage_initialized = 0;
	int result = 1;
	struct prototype_work_capsule capsule;
	prototype_work_capsule_init(&capsule);

	prototype_effort_account_init(&effort, UINT64_C(1000000));
	prototype_elaborated_module_init(&source);
	if (project_fixture(&storage, &source) != 0) goto cleanup;
	storage_initialized = 1;
	if (prototype_checker_check_module(
			&source.view, &options, &source_checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !source_checked) {
		fprintf(stderr, "fixture did not mint checked authority\n");
		goto cleanup;
	}
	first = tmpfile();
	second = tmpfile();
	if (!first || !second || prototype_checked_artifact_write(first, NULL) == 0 ||
		prototype_checked_artifact_write(first, source_checked) != 0 ||
		prototype_checked_artifact_write(second, source_checked) != 0 ||
		read_file_bytes(first, &first_bytes, &first_count) != 0 ||
		read_file_bytes(second, &second_bytes, &second_count) != 0 ||
		first_count != second_count ||
		memcmp(first_bytes, second_bytes, first_count) != 0) {
		fprintf(stderr, "checked artifact output is not canonical\n");
		goto cleanup;
	}
	if (getenv("A_PROGRAM_ARTIFACT_BASELINE")) {
		printf(
			"A_PROGRAM_CHECKED_ARTIFACT_BASELINE 1 bytes=%zu fnv1a64=%llu\n",
			first_count,
			(unsigned long long)test_hash(first_bytes, first_count)
		);
	}
	capsule.producer_kind = PROTOTYPE_WORK_CAPSULE_CLASSIFIER;
	capsule.producer_version = 1;
	capsule.cost_model_version = PROTOTYPE_EFFORT_COST_MODEL_VERSION;
	capsule.calculus_fingerprint = source.view.calculus_fingerprint;
	capsule.intrinsic_fingerprint = source.view.intrinsic_fingerprint;
	capsule.payload_format = 1;
	capsule.payload_size = 1;
	capsule.payload = malloc(1);
	with_capsule = tmpfile();
	if (!capsule.payload || !with_capsule) goto cleanup;
	capsule.payload[0] = 42;
	if (prototype_checked_artifact_write_with_capsule(
			with_capsule, source_checked, &capsule
		) != 0 || fseek(with_capsule, 0, SEEK_SET) != 0 ||
		prototype_checked_artifact_read(
			with_capsule, &options, &decoded, &decoded_checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE ||
		!decoded_checked) {
		fprintf(stderr, "producer capsule changed checked import authority\n");
		prototype_checked_module_destroy(decoded_checked);
		prototype_elaborated_module_destroy(&decoded);
		decoded_checked = NULL;
		goto cleanup;
	}
	if (expect_canonical_rewrite(
			decoded_checked, first_bytes, first_count
		) != 0) {
		fprintf(stderr, "producer erasure changed canonical checked bytes\n");
		prototype_checked_module_destroy(decoded_checked);
		prototype_elaborated_module_destroy(&decoded);
		decoded_checked = NULL;
		goto cleanup;
	}
	prototype_checked_module_destroy(decoded_checked);
	prototype_elaborated_module_destroy(&decoded);
	decoded_checked = NULL;
	struct prototype_work_capsule extracted;
	prototype_work_capsule_init(&extracted);
	if (fseek(with_capsule, 0, SEEK_SET) != 0 ||
		prototype_checked_artifact_extract_capsule(
			with_capsule, &extracted
		) != 0 || extracted.payload_size != 1 || extracted.payload[0] != 42 ||
		fseek(first, 0, SEEK_SET) != 0 ||
		prototype_checked_artifact_extract_capsule(first, &extracted) != 1) {
		fprintf(stderr, "producer capsule extraction boundary failed\n");
		prototype_work_capsule_destroy(&extracted);
		goto cleanup;
	}
	prototype_work_capsule_destroy(&extracted);

	prototype_checked_module_destroy(source_checked);
	source_checked = NULL;
	prototype_elaborated_module_destroy(&source);
	prototype_program_storage_destroy(&storage);
	storage_initialized = 0;
	if (fseek(first, 0, SEEK_SET) != 0) goto cleanup;
	if (prototype_checked_artifact_read(
			first, &options, &decoded, &decoded_checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE ||
		!decoded_checked ||
		prototype_checked_module_elaborated_view(decoded_checked) != &decoded.view ||
		decoded.view.interface.function_graph_association_count == 0) {
		fprintf(stderr, "checked artifact did not survive fresh checking\n");
		prototype_checked_module_destroy(decoded_checked);
		prototype_elaborated_module_destroy(&decoded);
		goto cleanup;
	}
	prototype_checked_module_destroy(decoded_checked);
	prototype_elaborated_module_destroy(&decoded);

	if (first_count <= 36) goto cleanup;
	if (expect_read_rejected(first_bytes, 15, &options) != 0) {
		fprintf(stderr, "truncated checked artifact header was accepted\n");
		goto cleanup;
	}
	if (expect_read_rejected(first_bytes, first_count - 1, &options) != 0) {
		fprintf(stderr, "truncated checked artifact payload was accepted\n");
		goto cleanup;
	}
	test_u32(&first_bytes[12], 1);
	if (expect_read_rejected(first_bytes, first_count, &options) != 0) {
		fprintf(stderr, "missing required section was accepted\n");
		goto cleanup;
	}
	test_u32(&first_bytes[12], 2);
	test_u32(&first_bytes[16], 99);
	if (expect_read_rejected(first_bytes, first_count, &options) != 0) {
		fprintf(stderr, "unknown checked artifact section was accepted\n");
		goto cleanup;
	}
	test_u32(&first_bytes[16], 1);
	uint64_t semantic_count = test_read_u64(&first_bytes[20]);
	if (semantic_count > SIZE_MAX - 56 || semantic_count + 56 > first_count) {
		goto cleanup;
	}
	size_t second_kind_offset = (size_t)semantic_count + 36;
	test_u32(&first_bytes[second_kind_offset], 1);
	if (expect_read_rejected(first_bytes, first_count, &options) != 0) {
		fprintf(stderr, "duplicate checked artifact section was accepted\n");
		goto cleanup;
	}
	test_u32(&first_bytes[second_kind_offset], 2);
	test_u64(&first_bytes[20], UINT64_C(1073741825));
	if (expect_read_rejected(first_bytes, first_count, &options) != 0) {
		fprintf(stderr, "oversized checked artifact section was accepted\n");
		goto cleanup;
	}
	test_u64(&first_bytes[20], semantic_count);
	first_bytes[36] ^= 1;
	if (expect_read_rejected(first_bytes, first_count, &options) != 0) {
		fprintf(stderr, "semantic payload corruption was accepted\n");
		goto cleanup;
	}
	first_bytes[36] ^= 1;

	static const unsigned char debug_payload[] = {'d', 'b', 'g'};
	size_t extended_count = first_count + 20 + sizeof(debug_payload);
	unsigned char* extended = malloc(extended_count);
	if (!extended) goto cleanup;
	memcpy(extended, first_bytes, first_count);
	test_u32(&extended[12], 3);
	test_u32(&extended[first_count], 4);
	test_u64(&extended[first_count + 4], sizeof(debug_payload));
	test_u64(
		&extended[first_count + 12],
		test_hash(debug_payload, sizeof(debug_payload))
	);
	memcpy(&extended[first_count + 20], debug_payload, sizeof(debug_payload));
	FILE* extended_file = tmpfile();
	if (!extended_file ||
		write_file_bytes(extended_file, extended, extended_count) != 0) {
		free(extended);
		if (extended_file) fclose(extended_file);
		goto cleanup;
	}
	free(extended);
	if (prototype_checked_artifact_read(
			extended_file, &options, &decoded, &decoded_checked, &report
		) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !decoded_checked) {
		fprintf(stderr, "non-authoritative debug section changed acceptance\n");
		fclose(extended_file);
		goto cleanup;
	}
	if (expect_canonical_rewrite(
			decoded_checked, first_bytes, first_count
		) != 0) {
		fprintf(stderr, "debug erasure changed canonical checked bytes\n");
		prototype_checked_module_destroy(decoded_checked);
		prototype_elaborated_module_destroy(&decoded);
		fclose(extended_file);
		goto cleanup;
	}
	prototype_checked_module_destroy(decoded_checked);
	prototype_elaborated_module_destroy(&decoded);
	fclose(extended_file);
	result = 0;

cleanup:
	prototype_work_capsule_destroy(&capsule);
	if (with_capsule) fclose(with_capsule);
	free(second_bytes);
	free(first_bytes);
	if (second) fclose(second);
	if (first) fclose(first);
	prototype_checked_module_destroy(source_checked);
	prototype_elaborated_module_destroy(&source);
	if (storage_initialized) prototype_program_storage_destroy(&storage);
	if (result == 0) puts("checked artifact container tests passed");
	return result;
}
