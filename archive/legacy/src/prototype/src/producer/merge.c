#include "a_program/producer/merge.h"

#include "a_program/checker/container.h"
#include "a_program/producer/effort.h"

#include "../checker/module_set_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MERGE_PAYLOAD_MAGIC "APMRG001"
#define MERGE_PAYLOAD_MAGIC_SIZE 8

struct merge_module_image {
	const struct prototype_checked_module* checked;
	struct prototype_elaborated_module* owned_module;
};

struct prototype_merge_producer_session {
	struct merge_module_image* images;
	size_t owned_image_count;
	size_t image_count;
	struct prototype_checked_module_set* canonical_images;
	size_t left;
	size_t right;
	size_t completed_pairs;
	struct prototype_effort_account effort;
	struct prototype_work_capsule_compatibility identity;
	struct prototype_checked_module_set* result;
	int outcome;
	int conflict_kind;
};

struct merge_buffer {
	unsigned char* bytes;
	size_t count;
	size_t capacity;
};

struct merge_reader {
	const unsigned char* bytes;
	size_t count;
	size_t cursor;
};

static int merge_buffer_write(
	struct merge_buffer* buffer,
	const void* bytes,
	size_t count
) {
	if (!buffer || count > SIZE_MAX - buffer->count ||
		(count != 0 && !bytes)) return -1;
	size_t required = buffer->count + count;
	if (required > buffer->capacity) {
		size_t capacity = buffer->capacity == 0 ? 1024 : buffer->capacity;
		while (capacity < required) {
			if (capacity > SIZE_MAX / 2) {
				capacity = required;
				break;
			}
			capacity *= 2;
		}
		unsigned char* resized = realloc(buffer->bytes, capacity);
		if (!resized) return -1;
		buffer->bytes = resized;
		buffer->capacity = capacity;
	}
	if (count != 0) memcpy(&buffer->bytes[buffer->count], bytes, count);
	buffer->count += count;
	return 0;
}

static int merge_buffer_u64(struct merge_buffer* buffer, uint64_t value) {
	unsigned char bytes[8];
	for (size_t i = 0; i < 8; ++i) bytes[i] = (unsigned char)(value >> (8 * i));
	return merge_buffer_write(buffer, bytes, sizeof(bytes));
}

static int merge_read(
	struct merge_reader* reader,
	void* target,
	size_t count
) {
	if (!reader || reader->cursor > reader->count ||
		count > reader->count - reader->cursor || (count != 0 && !target)) {
		return -1;
	}
	if (count != 0) memcpy(target, &reader->bytes[reader->cursor], count);
	reader->cursor += count;
	return 0;
}

static int merge_read_u64(struct merge_reader* reader, uint64_t* p_value) {
	unsigned char bytes[8];
	if (!p_value || merge_read(reader, bytes, sizeof(bytes)) != 0) return -1;
	*p_value = 0;
	for (size_t i = 0; i < 8; ++i) *p_value |= (uint64_t)bytes[i] << (8 * i);
	return 0;
}

static void merge_image_destroy(struct merge_module_image* image) {
	if (!image) return;
	prototype_checked_module_destroy(
		image->owned_module ? (struct prototype_checked_module*)image->checked : NULL
	);
	if (image->owned_module) {
		prototype_elaborated_module_destroy(image->owned_module);
		free(image->owned_module);
	}
	memset(image, 0, sizeof(*image));
}

static int merge_session_prepare(struct prototype_merge_producer_session* session) {
	if (!session || !session->canonical_images) return -1;
	session->image_count = prototype_checked_module_set_count(
		session->canonical_images
	);
	session->left = 0;
	session->right = session->image_count > 1 ? 1 : session->image_count;
	return 0;
}

int prototype_merge_producer_create(
	const struct prototype_checked_module* const* modules,
	size_t module_count,
	const struct prototype_work_capsule_compatibility* identity,
	struct prototype_merge_producer_session** p_session
) {
	if (!identity || !p_session || (module_count != 0 && !modules) ||
		identity->producer_kind != PROTOTYPE_WORK_CAPSULE_FRAGMENT_MERGE ||
		identity->producer_version != PROTOTYPE_MERGE_PRODUCER_VERSION ||
		identity->cost_model_version != PROTOTYPE_EFFORT_COST_MODEL_VERSION) {
		return -1;
	}
	*p_session = NULL;
	struct prototype_merge_producer_session* session = calloc(1, sizeof(*session));
	if (!session) return -1;
	session->identity = *identity;
	prototype_effort_account_init(&session->effort, 0);
	if (prototype_checked_module_set_prepare(
			modules, module_count, &session->canonical_images
		) != 0 || merge_session_prepare(session) != 0) {
		prototype_merge_producer_destroy(session);
		return -1;
	}
	*p_session = session;
	return 0;
}

int prototype_merge_producer_advance(
	struct prototype_merge_producer_session* session,
	uint64_t additional_effort,
	struct prototype_merge_producer_report* p_report
) {
	if (!session || !p_report || session->outcome != 0 ||
		prototype_effort_add_credits(&session->effort, additional_effort) != 0) {
		return -1;
	}
	uint64_t before = session->effort.used;
	while (session->left < session->image_count &&
		session->right < session->image_count) {
		int charged = prototype_effort_consume(
			&session->effort, PROTOTYPE_EFFORT_PHASE_MERGE, 1
		);
		if (charged == 1) {
			*p_report = (struct prototype_merge_producer_report) {
				.status = PROTOTYPE_MERGE_PRODUCER_PAUSED,
				.effort_used = session->effort.used - before,
				.completed_pairs = session->completed_pairs
			};
			return 0;
		}
		if (charged != 0) return -1;
		int kind;
		int conflict = prototype_checked_modules_conflict(
			prototype_checked_module_set_at(session->canonical_images, session->left),
			prototype_checked_module_set_at(session->canonical_images, session->right),
			&kind
		);
		if (conflict != 0) {
			session->outcome = PROTOTYPE_MERGE_PRODUCER_REJECTED;
			session->conflict_kind = conflict > 0 ? kind : 0;
			*p_report = (struct prototype_merge_producer_report) {
				.status = session->outcome,
				.conflict_kind = session->conflict_kind,
				.effort_used = session->effort.used - before,
				.completed_pairs = session->completed_pairs
			};
			return 0;
		}
		session->completed_pairs += 1;
		session->right += 1;
		if (session->right == session->image_count) {
			session->left += 1;
			session->right = session->left + 1;
		}
	}
	session->result = session->canonical_images;
	session->canonical_images = NULL;
	session->outcome = PROTOTYPE_MERGE_PRODUCER_COMPLETE;
	*p_report = (struct prototype_merge_producer_report) {
		.status = session->outcome,
		.effort_used = session->effort.used - before,
		.completed_pairs = session->completed_pairs
	};
	return 0;
}

int prototype_merge_producer_make_capsule(
	const struct prototype_merge_producer_session* session,
	struct prototype_work_capsule* capsule
) {
	if (!session || !capsule || session->outcome != 0) return -1;
	struct merge_buffer payload = {0};
	if (merge_buffer_write(
			&payload, MERGE_PAYLOAD_MAGIC, MERGE_PAYLOAD_MAGIC_SIZE
		) != 0 || merge_buffer_u64(&payload, session->image_count) != 0 ||
		merge_buffer_u64(&payload, session->left) != 0 ||
		merge_buffer_u64(&payload, session->right) != 0 ||
		merge_buffer_u64(&payload, session->completed_pairs) != 0) {
		free(payload.bytes);
		return -1;
	}
	for (size_t i = 0; i < session->image_count; ++i) {
		const unsigned char* bytes;
		size_t count;
		if (prototype_checked_module_set_canonical_image_at(
				session->canonical_images, i, &bytes, &count
			) != 0 || merge_buffer_u64(&payload, count) != 0 ||
			merge_buffer_write(
				&payload, bytes, count
			) != 0) {
			free(payload.bytes);
			return -1;
		}
	}
	prototype_work_capsule_destroy(capsule);
	prototype_work_capsule_init(capsule);
	capsule->producer_kind = PROTOTYPE_WORK_CAPSULE_FRAGMENT_MERGE;
	capsule->producer_version = PROTOTYPE_MERGE_PRODUCER_VERSION;
	capsule->cost_model_version = session->identity.cost_model_version;
	capsule->calculus_fingerprint = session->identity.calculus_fingerprint;
	capsule->intrinsic_fingerprint = session->identity.intrinsic_fingerprint;
	memcpy(capsule->base_revision, session->identity.base_revision,
		sizeof(capsule->base_revision));
	memcpy(capsule->goal_key, session->identity.goal_key,
		sizeof(capsule->goal_key));
	capsule->payload_format = PROTOTYPE_MERGE_PRODUCER_PAYLOAD_FORMAT;
	capsule->payload = payload.bytes;
	capsule->payload_size = payload.count;
	return 0;
}

int prototype_merge_producer_restore(
	const struct prototype_work_capsule* capsule,
	const struct prototype_work_capsule_compatibility* expected,
	const struct prototype_checker_options* checker_options,
	struct prototype_merge_producer_session** p_session
) {
	if (!capsule || !expected || !checker_options || !p_session ||
		!prototype_work_capsule_is_compatible(capsule, expected) ||
		capsule->producer_kind != PROTOTYPE_WORK_CAPSULE_FRAGMENT_MERGE ||
		capsule->producer_version != PROTOTYPE_MERGE_PRODUCER_VERSION ||
		capsule->payload_format != PROTOTYPE_MERGE_PRODUCER_PAYLOAD_FORMAT) {
		return -1;
	}
	*p_session = NULL;
	struct merge_reader reader = {
		.bytes = capsule->payload,
		.count = capsule->payload_size
	};
	char magic[MERGE_PAYLOAD_MAGIC_SIZE];
	uint64_t image_count;
	uint64_t left;
	uint64_t right;
	uint64_t completed_pairs;
	if (merge_read(&reader, magic, sizeof(magic)) != 0 ||
		memcmp(magic, MERGE_PAYLOAD_MAGIC, sizeof(magic)) != 0 ||
		merge_read_u64(&reader, &image_count) != 0 || image_count > SIZE_MAX ||
		merge_read_u64(&reader, &left) != 0 || left > image_count ||
		merge_read_u64(&reader, &right) != 0 || right > image_count + 1 ||
		merge_read_u64(&reader, &completed_pairs) != 0 ||
		completed_pairs > SIZE_MAX) return -1;
	struct prototype_merge_producer_session* session = calloc(1, sizeof(*session));
	if (!session) return -1;
	session->image_count = (size_t)image_count;
	session->owned_image_count = (size_t)image_count;
	session->images = image_count == 0 ? NULL : calloc(
		(size_t)image_count, sizeof(*session->images)
	);
	if (image_count != 0 && !session->images) {
		free(session);
		return -1;
	}
	session->identity = *expected;
	session->left = (size_t)left;
	session->right = (size_t)right;
	session->completed_pairs = (size_t)completed_pairs;
	prototype_effort_account_init(&session->effort, 0);
	struct prototype_canonical_checked_image* canonical =
		image_count == 0 ? NULL : calloc((size_t)image_count, sizeof(*canonical));
	if (image_count != 0 && !canonical) goto fail;
	for (size_t i = 0; i < session->image_count; ++i) {
		uint64_t size;
		if (merge_read_u64(&reader, &size) != 0 || size > SIZE_MAX ||
			size > reader.count - reader.cursor) goto fail;
		FILE* stream = tmpfile();
		struct prototype_elaborated_module* module = malloc(sizeof(*module));
		struct prototype_checked_module* checked = NULL;
		struct prototype_checker_report report;
		if (!stream || !module || fwrite(
				&reader.bytes[reader.cursor], 1, (size_t)size, stream
			) != (size_t)size || fflush(stream) != 0 ||
			fseek(stream, 0, SEEK_SET) != 0 || prototype_checked_artifact_read(
				stream, checker_options, module, &checked, &report
			) != 0 || report.status != PROTOTYPE_CHECKER_COMPLETE || !checked) {
			if (stream) fclose(stream);
			free(module);
			prototype_checked_module_destroy(checked);
			goto fail;
		}
		fclose(stream);
		session->images[i] = (struct merge_module_image) {
			.checked = checked,
			.owned_module = module
		};
		canonical[i] = (struct prototype_canonical_checked_image) {
			.module = checked,
			.bytes = size == 0 ? NULL : malloc((size_t)size),
			.count = (size_t)size,
			.input_index = i
		};
		if (size != 0 && !canonical[i].bytes) goto fail;
		if (size != 0) memcpy(
			canonical[i].bytes, &reader.bytes[reader.cursor], (size_t)size
		);
		reader.cursor += (size_t)size;
	}
	if (reader.cursor != reader.count || prototype_checked_module_set_adopt(
			canonical, (size_t)image_count, &session->canonical_images
		) != 0) goto fail;
	canonical = NULL;
	*p_session = session;
	return 0;

fail:
	if (canonical) {
		for (size_t i = 0; i < (size_t)image_count; ++i) free(canonical[i].bytes);
		free(canonical);
	}
	prototype_merge_producer_destroy(session);
	return -1;
}

const struct prototype_checked_module_set* prototype_merge_producer_result(
	const struct prototype_merge_producer_session* session
) {
	return session && session->outcome == PROTOTYPE_MERGE_PRODUCER_COMPLETE ?
		session->result : NULL;
}

void prototype_merge_producer_destroy(
	struct prototype_merge_producer_session* session
) {
	if (!session) return;
	prototype_checked_module_set_destroy(session->result);
	prototype_checked_module_set_destroy(session->canonical_images);
	for (size_t i = 0; i < session->owned_image_count; ++i) {
		merge_image_destroy(&session->images[i]);
	}
	free(session->images);
	free(session);
}
