#include "a_program/checker/module.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/structural_reader.h"

#include <stdio.h>
#include <string.h>

#define CONTEXT_COUNT 3
#define SUBSTITUTION_COUNT 8
#define TERM_COUNT 10

static int compare_term_readers(
	const struct prototype_term_structural_reader* left,
	const struct prototype_term_structural_reader* right
) {
	if (left->term_count != right->term_count) return -1;
	for (uint32_t i = 0; i < left->term_count; ++i) {
		const struct prototype_term* a;
		const struct prototype_term* b;
		if (prototype_term_structural_read(left, i, &a) != 0 ||
			prototype_term_structural_read(right, i, &b) != 0 ||
			memcmp(a, b, sizeof(*a)) != 0) return -1;
	}
	return 0;
}

static int compare_context_readers(
	const struct prototype_context_structural_reader* left,
	const struct prototype_context_structural_reader* right
) {
	if (left->count != right->count) return -1;
	for (uint32_t i = 0; i < left->count; ++i) {
		struct prototype_context_structural_record a;
		struct prototype_context_structural_record b;
		if (prototype_context_structural_read(left, i, &a) != 0 ||
			prototype_context_structural_read(right, i, &b) != 0 ||
			memcmp(&a, &b, sizeof(a)) != 0) return -1;
	}
	return 0;
}

static int compare_substitution_readers(
	const struct prototype_substitution_structural_reader* left,
	const struct prototype_substitution_structural_reader* right
) {
	if (left->count != right->count) return -1;
	for (uint32_t i = 0; i < left->count; ++i) {
		struct prototype_substitution_structural_record a;
		struct prototype_substitution_structural_record b;
		if (prototype_substitution_structural_read(left, i, &a) != 0 ||
			prototype_substitution_structural_read(right, i, &b) != 0 ||
			memcmp(&a, &b, sizeof(a)) != 0) return -1;
	}
	return 0;
}

int main(void) {
	struct prototype_term terms[5] = {
		{ .tag = PROTOTYPE_TERM_PRIMITIVE_INT },
		{ .tag = PROTOTYPE_TERM_RETURN, .as.return_term = { .value = 0 } },
		{ .tag = PROTOTYPE_TERM_LAMBDA,
			.as.lambda = { .binding_id = 99, .body = 1 } },
		{ .tag = PROTOTYPE_TERM_THUNK, .as.thunk = { .computation = 2 } },
		{ .tag = PROTOTYPE_TERM_PI,
			.as.pi = { .domain = 0, .codomain_family = 3 } }
	};
	struct prototype_term_db term_db = {
		.terms = terms,
		.term_count = 5,
		.term_capacity = 5
	};
	struct prototype_semantic_term_graph_view semantic_term_graph = {
		.terms = terms,
		.term_count = 5
	};
	struct prototype_term_structural_reader producer_term_reader;
	struct prototype_term_structural_reader semantic_term_reader;
	uint32_t domain;
	uint32_t binding;
	uint32_t body;
	if (prototype_term_structural_reader_from_db(
			&term_db, &producer_term_reader
		) != 0 || prototype_semantic_term_structural_reader(
			&semantic_term_graph, &semantic_term_reader
		) != 0 || compare_term_readers(
			&producer_term_reader, &semantic_term_reader
		) != 0 || prototype_term_structural_pure_family_parts(
			&semantic_term_reader, 3, &binding, &body
		) != 0 || binding != 99 || body != 0 ||
		prototype_term_structural_pi_parts(
			&semantic_term_reader, 4, &domain, &binding, &body
		) != 0 || domain != 0 || binding != 99 || body != 0 ||
		prototype_term_structural_pi_parts(
			&semantic_term_reader, 3, &domain, &binding, &body
		) == 0) return 1;

	struct prototype_context producer_contexts[CONTEXT_COUNT];
	struct prototype_context_db context_db;
	prototype_context_db_init(
		&context_db, producer_contexts, CONTEXT_COUNT
	);
	uint32_t first_context;
	uint32_t second_context;
	if (prototype_context_extend(
			&context_db, 0, 10, 2, PROTOTYPE_INVALID_ID, &first_context
		) != 0 || prototype_context_extend_sequence_result(
			&context_db, first_context, 20, 3, PROTOTYPE_INVALID_ID, 4,
			&second_context
		) != 0 || first_context != 1 || second_context != 2) return 2;

	struct prototype_semantic_context semantic_contexts[CONTEXT_COUNT];
	for (uint32_t i = 0; i < CONTEXT_COUNT; ++i) {
		semantic_contexts[i] = (struct prototype_semantic_context) {
			.parent = producer_contexts[i].parent,
			.binding_id = producer_contexts[i].binding_id,
			.classifier = i == 0 ? PROTOTYPE_INVALID_ID :
				producer_contexts[i].classifier_ref.term_id,
			.extension_kind = producer_contexts[i].extension_kind,
			.producer_computation = producer_contexts[i].producer_computation
		};
	}

	struct prototype_substitution producer_substitutions[SUBSTITUTION_COUNT] = {
		{ .kind = PROTOTYPE_SUBSTITUTION_IDENTITY,
			.source_context = 0, .target_context = 0,
			.first = PROTOTYPE_INVALID_ID, .second = PROTOTYPE_INVALID_ID,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID },
		{ .kind = PROTOTYPE_SUBSTITUTION_IDENTITY,
			.source_context = 1, .target_context = 1,
			.first = PROTOTYPE_INVALID_ID, .second = PROTOTYPE_INVALID_ID,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID },
		{ .kind = PROTOTYPE_SUBSTITUTION_IDENTITY,
			.source_context = 2, .target_context = 2,
			.first = PROTOTYPE_INVALID_ID, .second = PROTOTYPE_INVALID_ID,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID },
		{ .kind = PROTOTYPE_SUBSTITUTION_EMPTY,
			.source_context = 1, .target_context = 0,
			.first = PROTOTYPE_INVALID_ID, .second = PROTOTYPE_INVALID_ID,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID },
		{ .kind = PROTOTYPE_SUBSTITUTION_PROJECTION,
			.source_context = 1, .target_context = 0,
			.first = PROTOTYPE_INVALID_ID, .second = PROTOTYPE_INVALID_ID,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID },
		{ .kind = PROTOTYPE_SUBSTITUTION_EXTEND,
			.source_context = 1, .target_context = 1,
			.first = 3, .second = PROTOTYPE_INVALID_ID,
			.term = 5, .term_classifier = 6 },
		{ .kind = PROTOTYPE_SUBSTITUTION_PROJECTION,
			.source_context = 2, .target_context = 1,
			.first = PROTOTYPE_INVALID_ID, .second = PROTOTYPE_INVALID_ID,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID },
		{ .kind = PROTOTYPE_SUBSTITUTION_COMPOSE,
			.source_context = 2, .target_context = 1,
			.first = 5, .second = 6,
			.term = PROTOTYPE_INVALID_ID,
			.term_classifier = PROTOTYPE_INVALID_ID }
	};
	struct prototype_substitution_db substitution_db = {
		.substitutions = producer_substitutions,
		.substitution_count = SUBSTITUTION_COUNT,
		.substitution_capacity = SUBSTITUTION_COUNT
	};
	struct prototype_semantic_substitution
		semantic_substitutions[SUBSTITUTION_COUNT];
	for (uint32_t i = 0; i < SUBSTITUTION_COUNT; ++i) {
		semantic_substitutions[i] = (struct prototype_semantic_substitution) {
			.kind = producer_substitutions[i].kind,
			.source_context = producer_substitutions[i].source_context,
			.target_context = producer_substitutions[i].target_context,
			.first = producer_substitutions[i].first,
			.second = producer_substitutions[i].second,
			.term = producer_substitutions[i].term,
			.term_classifier = producer_substitutions[i].term_classifier
		};
	}

	struct prototype_context_structural_reader producer_context_reader;
	struct prototype_context_structural_reader semantic_context_reader;
	struct prototype_substitution_structural_reader producer_substitution_reader;
	struct prototype_substitution_structural_reader semantic_substitution_reader;
	struct prototype_semantic_context_graph_view semantic_context_graph = {
		.contexts = semantic_contexts,
		.context_count = CONTEXT_COUNT
	};
	struct prototype_semantic_substitution_graph_view semantic_substitution_graph = {
		.substitutions = semantic_substitutions,
		.substitution_count = SUBSTITUTION_COUNT
	};
	if (prototype_context_structural_reader_from_db(
			&context_db, &producer_context_reader
		) != 0 || prototype_semantic_context_structural_reader(
			&semantic_context_graph, &semantic_context_reader
		) != 0 || prototype_substitution_structural_reader_from_db(
			&substitution_db, &producer_substitution_reader
		) != 0 || prototype_semantic_substitution_structural_reader(
			&semantic_substitution_graph, &semantic_substitution_reader
		) != 0 || compare_context_readers(
			&producer_context_reader, &semantic_context_reader
		) != 0 || compare_substitution_readers(
			&producer_substitution_reader, &semantic_substitution_reader
		) != 0) return 3;

	if (prototype_context_structural_validate(
			&producer_context_reader, TERM_COUNT
		) != 0 || prototype_context_structural_validate(
			&semantic_context_reader, TERM_COUNT
		) != 0 || prototype_substitution_structural_validate(
			&producer_substitution_reader, &producer_context_reader, TERM_COUNT
		) != 0 || prototype_substitution_structural_validate(
			&semantic_substitution_reader, &semantic_context_reader, TERM_COUNT
		) != 0) return 4;

	uint32_t path[2];
	size_t path_count;
	uint32_t entry;
	uint32_t classifier;
	if (prototype_context_structural_is_ancestor(
			&semantic_context_reader, 0, 2
		) != 1 || prototype_context_structural_is_ancestor(
			&semantic_context_reader, 2, 1
		) != 0 || prototype_context_structural_path(
			&semantic_context_reader, 0, 2, path, 2, &path_count
		) != 0 || path_count != 2 || path[0] != 1 || path[1] != 2 ||
		prototype_context_structural_find_binding(
			&semantic_context_reader, 2, 10, &entry, &classifier
		) != 0 || entry != 1 || classifier != 2) return 5;

	struct prototype_substitution_structural_image image;
	if (prototype_substitution_structural_binding_image(
			&semantic_substitution_reader, &semantic_context_reader, 4, 10, &image
		) != 0 || image.kind !=
			PROTOTYPE_SUBSTITUTION_STRUCTURAL_IMAGE_VARIABLE ||
		image.binding_id != 10 || prototype_substitution_structural_binding_image(
			&semantic_substitution_reader, &semantic_context_reader, 5, 10, &image
		) != 0 || image.kind != PROTOTYPE_SUBSTITUTION_STRUCTURAL_IMAGE_TERM ||
		image.term != 5 || prototype_substitution_structural_binding_image(
			&semantic_substitution_reader, &semantic_context_reader, 7, 10, &image
		) != 1) return 6;

	semantic_contexts[2].parent = 2;
	if (prototype_context_structural_validate(
			&semantic_context_reader, TERM_COUNT
		) == 0 || prototype_context_structural_is_ancestor(
			&semantic_context_reader, 0, 2
		) != -1) return 7;
	semantic_contexts[2].parent = 1;

	producer_contexts[1].classifier_ref.kind =
		PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL;
	if (prototype_context_structural_validate(
		&producer_context_reader, TERM_COUNT
		) == 0) return 8;

	puts("immutable structural reader checks passed");
	return 0;
}
