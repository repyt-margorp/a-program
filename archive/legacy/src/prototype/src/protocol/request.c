#include "a_program/protocol/request.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static size_t payload_header_size(void) {
	return offsetof(struct prototype_core_request_payload, bytes);
}

static uint64_t payload_digest_bytes(
	const struct prototype_core_request_payload* payload
) {
	uint64_t digest = UINT64_C(1469598103934665603);
	for (size_t i = 0; i < sizeof(payload->byte_count); ++i) {
		digest ^= (uint8_t)(payload->byte_count >> (i * CHAR_BIT));
		digest *= UINT64_C(1099511628211);
	}
	for (size_t i = 0; i < payload->byte_count; ++i) {
		digest ^= payload->bytes[i];
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

size_t prototype_core_request_payload_storage_size(size_t byte_capacity) {
	return byte_capacity <= SIZE_MAX - payload_header_size() ?
		payload_header_size() + byte_capacity : 0;
}

int prototype_core_request_payload_init(
	void* storage,
	size_t storage_size,
	struct prototype_core_request_payload** payload
) {
	size_t header_size = payload_header_size();
	if (!storage || !payload || storage_size < header_size) return -1;
	memset(storage, 0, storage_size);
	struct prototype_core_request_payload* result = storage;
	result->byte_capacity = storage_size - header_size;
	*payload = result;
	return 0;
}

int prototype_core_request_payload_append(
	struct prototype_core_request_payload* payload,
	size_t storage_size,
	const void* values,
	size_t element_size,
	size_t element_count,
	struct prototype_core_request_range* range
) {
	size_t header_size = payload_header_size();
	if (!payload || !range || payload->sealed || storage_size < header_size ||
		payload->byte_capacity != storage_size - header_size ||
		element_size == 0 || element_count > UINT32_MAX ||
		(element_count != 0 && !values) ||
		element_count > SIZE_MAX / element_size) {
		return -1;
	}
	size_t alignment = _Alignof(union prototype_core_request_alignment);
	size_t padding = (alignment - payload->byte_count % alignment) % alignment;
	size_t byte_count = element_size * element_count;
	if (padding > payload->byte_capacity ||
		payload->byte_count > payload->byte_capacity - padding ||
		byte_count > payload->byte_capacity - payload->byte_count - padding ||
		payload->byte_count + padding > UINT32_MAX) {
		return -1;
	}
	if (padding != 0) {
		memset(payload->bytes + payload->byte_count, 0, padding);
	}
	range->byte_offset = (uint32_t)(payload->byte_count + padding);
	range->element_count = (uint32_t)element_count;
	if (byte_count != 0) {
		memcpy(payload->bytes + range->byte_offset, values, byte_count);
	}
	payload->byte_count = range->byte_offset + byte_count;
	return 0;
}

int prototype_core_request_payload_seal(
	struct prototype_core_request_payload* payload,
	size_t storage_size
) {
	size_t header_size = payload_header_size();
	if (!payload || payload->sealed || storage_size < header_size ||
		payload->byte_capacity != storage_size - header_size ||
		payload->byte_count > payload->byte_capacity) {
		return -1;
	}
	payload->digest = payload_digest_bytes(payload);
	if (payload->digest == 0) return -1;
	payload->sealed = 1;
	return 0;
}

static int core_request_payload_validate(
	const struct prototype_core_request_payload* payload,
	size_t storage_size,
	uint64_t expected_digest
) {
	size_t header_size = payload_header_size();
	if (!payload || storage_size < header_size || !payload->sealed ||
		payload->byte_capacity != storage_size - header_size ||
		payload->byte_count > payload->byte_capacity || payload->digest == 0 ||
		expected_digest != payload->digest ||
		payload_digest_bytes(payload) != payload->digest) {
		return -1;
	}
	return 0;
}

int prototype_core_formation_request_payload_validate(
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size
) {
	if (!request) return -1;
	int variable_payload = request->kind == PROTOTYPE_CORE_FORM_MATCH ?
		request->as.match.cases.element_count != 0 :
		request->kind == PROTOTYPE_CORE_FORM_COMPUTATION_FOLD ?
			request->as.computation_fold.clauses.element_count != 0 : 0;
	if (variable_payload) {
		return core_request_payload_validate(
			payload, payload_size, request->payload_digest
		);
	}
	return !payload && payload_size == 0 && request->payload_digest == 0 ?
		0 : -1;
}

int prototype_core_rewrite_request_payload_validate(
	const struct prototype_core_rewrite_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size
) {
	if (!request) return -1;
	int variable_payload = request->kind == PROTOTYPE_CORE_REWRITE_BINDINGS ?
		request->as.bindings.element_count != 0 :
		request->kind == PROTOTYPE_CORE_REWRITE_BINDING_SEQUENCE ?
			request->as.binding_sequence.element_count != 0 : 0;
	if (variable_payload) {
		return core_request_payload_validate(
			payload, payload_size, request->payload_digest
		);
	}
	return !payload && payload_size == 0 && request->payload_digest == 0 ?
		0 : -1;
}
