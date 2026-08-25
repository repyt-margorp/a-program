#include "a_program/producer/capsule.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define CAPSULE_MAGIC "APCAP002"
#define CAPSULE_MAGIC_SIZE 8
#define CAPSULE_MAX_COUNT UINT32_C(16777216)
#define CAPSULE_MAX_PAYLOAD UINT64_C(1073741824)
#define CAPSULE_FNV_OFFSET UINT64_C(1469598103934665603)
#define CAPSULE_FNV_PRIME UINT64_C(1099511628211)

static uint64_t hash_bytes(uint64_t hash, const void* data, size_t count) {
	const unsigned char* bytes = data;
	for (size_t i = 0; i < count; ++i) {
		hash ^= bytes[i];
		hash *= CAPSULE_FNV_PRIME;
	}
	return hash;
}

static int write_u32(FILE* stream, uint32_t value, uint64_t* hash) {
	unsigned char bytes[4];
	for (uint32_t i = 0; i < 4; ++i) bytes[i] = (unsigned char)(value >> (8 * i));
	if (fwrite(bytes, 1, sizeof(bytes), stream) != sizeof(bytes)) return -1;
	if (hash) *hash = hash_bytes(*hash, bytes, sizeof(bytes));
	return 0;
}

static int write_u64(FILE* stream, uint64_t value, uint64_t* hash) {
	unsigned char bytes[8];
	for (uint32_t i = 0; i < 8; ++i) bytes[i] = (unsigned char)(value >> (8 * i));
	if (fwrite(bytes, 1, sizeof(bytes), stream) != sizeof(bytes)) return -1;
	if (hash) *hash = hash_bytes(*hash, bytes, sizeof(bytes));
	return 0;
}

static int read_u32(FILE* stream, uint32_t* value, uint64_t* hash) {
	unsigned char bytes[4];
	if (!value || fread(bytes, 1, sizeof(bytes), stream) != sizeof(bytes)) return -1;
	*value = 0;
	for (uint32_t i = 0; i < 4; ++i) *value |= (uint32_t)bytes[i] << (8 * i);
	if (hash) *hash = hash_bytes(*hash, bytes, sizeof(bytes));
	return 0;
}

static int read_u64(FILE* stream, uint64_t* value, uint64_t* hash) {
	unsigned char bytes[8];
	if (!value || fread(bytes, 1, sizeof(bytes), stream) != sizeof(bytes)) return -1;
	*value = 0;
	for (uint32_t i = 0; i < 8; ++i) *value |= (uint64_t)bytes[i] << (8 * i);
	if (hash) *hash = hash_bytes(*hash, bytes, sizeof(bytes));
	return 0;
}

void prototype_work_capsule_init(struct prototype_work_capsule* capsule) {
	if (!capsule) return;
	memset(capsule, 0, sizeof(*capsule));
	capsule->capsule_version = PROTOTYPE_WORK_CAPSULE_VERSION;
}

void prototype_work_capsule_destroy(struct prototype_work_capsule* capsule) {
	if (!capsule) return;
	free(capsule->dependencies);
	free(capsule->negative_observations);
	free(capsule->payload);
	memset(capsule, 0, sizeof(*capsule));
}

int prototype_work_capsule_write(
	FILE* stream,
	const struct prototype_work_capsule* capsule
) {
	if (!stream || !capsule || capsule->capsule_version !=
			PROTOTYPE_WORK_CAPSULE_VERSION || capsule->producer_kind == 0 ||
		capsule->producer_version == 0 || capsule->cost_model_version == 0 ||
		capsule->dependency_count > CAPSULE_MAX_COUNT ||
		capsule->negative_observation_count > CAPSULE_MAX_COUNT ||
		capsule->payload_size > CAPSULE_MAX_PAYLOAD ||
		(capsule->dependency_count != 0 && !capsule->dependencies) ||
		(capsule->negative_observation_count != 0 &&
		 !capsule->negative_observations) ||
		(capsule->payload_size != 0 && !capsule->payload) ||
		fwrite(CAPSULE_MAGIC, 1, CAPSULE_MAGIC_SIZE, stream) != CAPSULE_MAGIC_SIZE) {
		return -1;
	}
	uint64_t hash = CAPSULE_FNV_OFFSET;
	if (write_u32(stream, capsule->capsule_version, &hash) != 0 ||
		write_u32(stream, capsule->producer_kind, &hash) != 0 ||
		write_u32(stream, capsule->producer_version, &hash) != 0 ||
		write_u32(stream, capsule->cost_model_version, &hash) != 0 ||
		write_u64(stream, capsule->calculus_fingerprint, &hash) != 0 ||
		write_u64(stream, capsule->intrinsic_fingerprint, &hash) != 0) return -1;
	for (size_t i = 0; i < 4; ++i) {
		if (write_u64(stream, capsule->base_revision[i], &hash) != 0 ||
			write_u64(stream, capsule->goal_key[i], &hash) != 0) return -1;
	}
	if (write_u32(stream, (uint32_t)capsule->dependency_count, &hash) != 0) {
		return -1;
	}
	for (size_t i = 0; i < capsule->dependency_count; ++i) {
		const struct prototype_work_capsule_dependency* dependency =
			&capsule->dependencies[i];
		for (size_t lane = 0; lane < 4; ++lane) {
			if (write_u64(stream, dependency->goal_key[lane], &hash) != 0 ||
				write_u64(
					stream, dependency->checked_content_fingerprint[lane], &hash
				) != 0) return -1;
		}
	}
	if (write_u32(
			stream, (uint32_t)capsule->negative_observation_count, &hash
		) != 0) return -1;
	for (size_t i = 0; i < capsule->negative_observation_count; ++i) {
		const struct prototype_work_capsule_negative_observation* observation =
			&capsule->negative_observations[i];
		for (size_t lane = 0; lane < 4; ++lane) {
			if (write_u64(
					stream, observation->candidate_space_key[lane], &hash
				) != 0 || write_u64(
					stream, observation->sealed_content_fingerprint[lane], &hash
				) != 0) return -1;
		}
	}
	if (write_u32(stream, capsule->payload_format, &hash) != 0 ||
		write_u64(stream, capsule->payload_size, &hash) != 0) return -1;
	if (capsule->payload_size != 0) {
		if (fwrite(capsule->payload, 1, capsule->payload_size, stream) !=
				capsule->payload_size) return -1;
		hash = hash_bytes(hash, capsule->payload, capsule->payload_size);
	}
	return write_u64(stream, hash, NULL);
}

int prototype_work_capsule_read(
	FILE* stream,
	struct prototype_work_capsule* capsule
) {
	char magic[CAPSULE_MAGIC_SIZE];
	struct prototype_work_capsule decoded;
	prototype_work_capsule_init(&decoded);
	if (!stream || !capsule ||
		fread(magic, 1, sizeof(magic), stream) != sizeof(magic) ||
		memcmp(magic, CAPSULE_MAGIC, sizeof(magic)) != 0) return -1;
	uint64_t hash = CAPSULE_FNV_OFFSET;
	uint32_t dependency_count;
	uint32_t negative_observation_count;
	uint64_t payload_size;
	if (read_u32(stream, &decoded.capsule_version, &hash) != 0 ||
		read_u32(stream, &decoded.producer_kind, &hash) != 0 ||
		read_u32(stream, &decoded.producer_version, &hash) != 0 ||
		read_u32(stream, &decoded.cost_model_version, &hash) != 0 ||
		read_u64(stream, &decoded.calculus_fingerprint, &hash) != 0 ||
		read_u64(stream, &decoded.intrinsic_fingerprint, &hash) != 0 ||
		decoded.capsule_version != PROTOTYPE_WORK_CAPSULE_VERSION ||
		decoded.producer_kind == 0 || decoded.producer_version == 0 ||
		decoded.cost_model_version == 0) goto fail;
	for (size_t i = 0; i < 4; ++i) {
		if (read_u64(stream, &decoded.base_revision[i], &hash) != 0 ||
			read_u64(stream, &decoded.goal_key[i], &hash) != 0) goto fail;
	}
	if (read_u32(stream, &dependency_count, &hash) != 0 ||
		dependency_count > CAPSULE_MAX_COUNT) goto fail;
	if (dependency_count != 0) {
		decoded.dependencies = calloc(dependency_count, sizeof(*decoded.dependencies));
		if (!decoded.dependencies) goto fail;
	}
	decoded.dependency_count = dependency_count;
	for (size_t i = 0; i < decoded.dependency_count; ++i) {
		for (size_t lane = 0; lane < 4; ++lane) {
			if (read_u64(
					stream, &decoded.dependencies[i].goal_key[lane], &hash
				) != 0 || read_u64(
					stream,
					&decoded.dependencies[i].checked_content_fingerprint[lane],
					&hash
				) != 0) goto fail;
		}
	}
	if (read_u32(stream, &negative_observation_count, &hash) != 0 ||
		negative_observation_count > CAPSULE_MAX_COUNT) goto fail;
	if (negative_observation_count != 0) {
		decoded.negative_observations = calloc(
			negative_observation_count, sizeof(*decoded.negative_observations)
		);
		if (!decoded.negative_observations) goto fail;
	}
	decoded.negative_observation_count = negative_observation_count;
	for (size_t i = 0; i < decoded.negative_observation_count; ++i) {
		for (size_t lane = 0; lane < 4; ++lane) {
			if (read_u64(
					stream,
					&decoded.negative_observations[i].candidate_space_key[lane],
					&hash
				) != 0 || read_u64(
					stream,
					&decoded.negative_observations[i].sealed_content_fingerprint[lane],
					&hash
				) != 0) goto fail;
		}
	}
	if (read_u32(stream, &decoded.payload_format, &hash) != 0 ||
		read_u64(stream, &payload_size, &hash) != 0 ||
		payload_size > CAPSULE_MAX_PAYLOAD || payload_size > SIZE_MAX) goto fail;
	if (payload_size != 0) {
		decoded.payload = malloc((size_t)payload_size);
		if (!decoded.payload || fread(
				decoded.payload, 1, (size_t)payload_size, stream
			) != payload_size) goto fail;
		hash = hash_bytes(hash, decoded.payload, (size_t)payload_size);
	}
	decoded.payload_size = (size_t)payload_size;
	uint64_t expected_hash;
	if (read_u64(stream, &expected_hash, NULL) != 0 ||
		expected_hash != hash || fgetc(stream) != EOF) goto fail;
	prototype_work_capsule_destroy(capsule);
	*capsule = decoded;
	return 0;

fail:
	prototype_work_capsule_destroy(&decoded);
	return -1;
}

int prototype_work_capsule_is_compatible(
	const struct prototype_work_capsule* capsule,
	const struct prototype_work_capsule_compatibility* expected
) {
	return capsule && expected && capsule->capsule_version ==
			PROTOTYPE_WORK_CAPSULE_VERSION &&
		capsule->producer_kind == expected->producer_kind &&
		capsule->producer_version == expected->producer_version &&
		capsule->cost_model_version == expected->cost_model_version &&
		capsule->calculus_fingerprint == expected->calculus_fingerprint &&
		capsule->intrinsic_fingerprint == expected->intrinsic_fingerprint &&
		memcmp(capsule->base_revision, expected->base_revision,
			sizeof(capsule->base_revision)) == 0 &&
		memcmp(capsule->goal_key, expected->goal_key,
			sizeof(capsule->goal_key)) == 0;
}
