#include "a_program/checker/module_set.h"

#include "module_set_internal.h"

#include "a_program/checker/container.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct prototype_checked_module_set {
	struct prototype_canonical_checked_image* images;
	size_t count;
};

static size_t checked_module_image_serialization_count;

void prototype_checked_module_image_serialization_count_reset(void) {
	checked_module_image_serialization_count = 0;
}

size_t prototype_checked_module_image_serialization_count(void) {
	return checked_module_image_serialization_count;
}

static void checked_module_image_destroy(
	struct prototype_canonical_checked_image* image
) {
	if (!image) return;
	free(image->bytes);
	memset(image, 0, sizeof(*image));
}

static int checked_module_image_build(
	const struct prototype_checked_module* module,
	size_t input_index,
	struct prototype_canonical_checked_image* image
) {
	if (!image) return -1;
	if (!module || !prototype_checked_module_elaborated_view(module)) return 1;
	checked_module_image_serialization_count += 1;
	FILE* stream = tmpfile();
	if (!stream || prototype_checked_artifact_write(stream, module) != 0 ||
		fseek(stream, 0, SEEK_END) != 0) {
		if (stream) fclose(stream);
		return -1;
	}
	long length = ftell(stream);
	if (length < 0 || (uintmax_t)length > SIZE_MAX ||
		fseek(stream, 0, SEEK_SET) != 0) {
		fclose(stream);
		return -1;
	}
	unsigned char* bytes = length == 0 ? NULL : malloc((size_t)length);
	if ((length != 0 && !bytes) ||
		(length != 0 && fread(bytes, 1, (size_t)length, stream) !=
			(size_t)length)) {
		free(bytes);
		fclose(stream);
		return -1;
	}
	fclose(stream);
	*image = (struct prototype_canonical_checked_image) {
		.module = module,
		.bytes = bytes,
		.count = (size_t)length,
		.input_index = input_index
	};
	return 0;
}

static int checked_module_image_compare(const void* a, const void* b) {
	const struct prototype_canonical_checked_image* left = a;
	const struct prototype_canonical_checked_image* right = b;
	size_t common = left->count < right->count ? left->count : right->count;
	int compared = common == 0 ? 0 : memcmp(left->bytes, right->bytes, common);
	if (compared != 0) return compared;
	return left->count < right->count ? -1 : left->count > right->count;
}

static int checked_module_images_equal(
	const struct prototype_canonical_checked_image* left,
	const struct prototype_canonical_checked_image* right
) {
	return left->count == right->count &&
		(left->count == 0 || memcmp(left->bytes, right->bytes, left->count) == 0);
}

static const char* checked_module_symbol(
	const struct prototype_elaborated_module_view* module,
	int symbol
) {
	return module && symbol >= 0 && (size_t)symbol < module->symbols.count ?
		module->symbols.strings[symbol] : NULL;
}

static int checked_module_symbol_equal(
	const struct prototype_elaborated_module_view* left,
	int left_symbol,
	const struct prototype_elaborated_module_view* right,
	int right_symbol
) {
	if (left_symbol < 0 || right_symbol < 0) return left_symbol == right_symbol;
	const char* left_name = checked_module_symbol(left, left_symbol);
	const char* right_name = checked_module_symbol(right, right_symbol);
	return left_name && right_name && strcmp(left_name, right_name) == 0;
}

static int checked_term_exports_conflict(
	const struct prototype_elaborated_module_view* left,
	const struct prototype_elaborated_module_view* right
) {
	for (size_t i = 0; i < left->interface.term_export_count; ++i) {
		const struct prototype_semantic_term_export* a =
			&left->interface.term_exports[i];
		for (size_t j = 0; j < right->interface.term_export_count; ++j) {
			const struct prototype_semantic_term_export* b =
				&right->interface.term_exports[j];
			if (checked_module_symbol_equal(
					left, a->namespace_symbol_id, right, b->namespace_symbol_id
				) && checked_module_symbol_equal(
					left, a->name_symbol_id, right, b->name_symbol_id
				)) return 1;
		}
	}
	return 0;
}

static int checked_type_exports_conflict(
	const struct prototype_elaborated_module_view* left,
	const struct prototype_elaborated_module_view* right
) {
	for (size_t i = 0; i < left->interface.type_export_count; ++i) {
		const struct prototype_semantic_type_export* a =
			&left->interface.type_exports[i];
		for (size_t j = 0; j < right->interface.type_export_count; ++j) {
			const struct prototype_semantic_type_export* b =
				&right->interface.type_exports[j];
			if (checked_module_symbol_equal(
					left, a->namespace_symbol_id, right, b->namespace_symbol_id
				) && checked_module_symbol_equal(
					left, a->name_symbol_id, right, b->name_symbol_id
				)) return 1;
		}
	}
	return 0;
}

static int checked_constructor_exports_conflict(
	const struct prototype_elaborated_module_view* left,
	const struct prototype_elaborated_module_view* right
) {
	for (size_t i = 0; i < left->interface.constructor_export_count; ++i) {
		const struct prototype_semantic_constructor_export* a =
			&left->interface.constructor_exports[i];
		const struct prototype_semantic_type_export* a_owner =
			&left->interface.type_exports[a->type_export];
		for (size_t j = 0; j < right->interface.constructor_export_count; ++j) {
			const struct prototype_semantic_constructor_export* b =
				&right->interface.constructor_exports[j];
			const struct prototype_semantic_type_export* b_owner =
				&right->interface.type_exports[b->type_export];
			if (checked_module_symbol_equal(
					left, a_owner->namespace_symbol_id,
					right, b_owner->namespace_symbol_id
				) && checked_module_symbol_equal(
					left, a_owner->name_symbol_id, right, b_owner->name_symbol_id
				) && checked_module_symbol_equal(
					left, a->name_symbol_id, right, b->name_symbol_id
				)) return 1;
		}
	}
	return 0;
}

int prototype_checked_modules_conflict(
	const struct prototype_checked_module* left,
	const struct prototype_checked_module* right,
	int* p_export_kind
) {
	const struct prototype_elaborated_module_view* left_view =
		prototype_checked_module_elaborated_view(left);
	const struct prototype_elaborated_module_view* right_view =
		prototype_checked_module_elaborated_view(right);
	if (!left_view || !right_view || !p_export_kind) return -1;
	*p_export_kind = checked_term_exports_conflict(left_view, right_view) ?
		PROTOTYPE_CHECKED_EXPORT_TERM :
		checked_type_exports_conflict(left_view, right_view) ?
			PROTOTYPE_CHECKED_EXPORT_TYPE :
			checked_constructor_exports_conflict(left_view, right_view) ?
				PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR : 0;
	return *p_export_kind == 0 ? 0 : 1;
}

int prototype_checked_module_set_prepare(
	const struct prototype_checked_module* const* modules,
	size_t module_count,
	struct prototype_checked_module_set** p_set
) {
	if (!p_set || (module_count != 0 && !modules)) return -1;
	*p_set = NULL;
	struct prototype_checked_module_set* set = calloc(1, sizeof(*set));
	if (!set) return -1;
	set->images = module_count == 0 ? NULL : calloc(
		module_count, sizeof(*set->images)
	);
	if (module_count != 0 && !set->images) {
		free(set);
		return -1;
	}
	set->count = module_count;
	for (size_t i = 0; i < module_count; ++i) {
		int status = checked_module_image_build(
			modules[i], i, &set->images[i]
		);
		if (status != 0) {
			prototype_checked_module_set_destroy(set);
			return status;
		}
	}
	qsort(set->images, set->count, sizeof(*set->images),
		checked_module_image_compare);
	for (size_t i = 1; i < set->count;) {
		if (!checked_module_images_equal(&set->images[i - 1], &set->images[i])) {
			i += 1;
			continue;
		}
		checked_module_image_destroy(&set->images[i]);
		memmove(&set->images[i], &set->images[i + 1],
			(set->count - i - 1) * sizeof(*set->images));
		set->count -= 1;
	}
	*p_set = set;
	return 0;
}

int prototype_checked_module_set_adopt(
	struct prototype_canonical_checked_image* images,
	size_t image_count,
	struct prototype_checked_module_set** p_set
) {
	if (!p_set || (image_count != 0 && !images)) return -1;
	*p_set = NULL;
	for (size_t i = 0; i < image_count; ++i) {
		if (!images[i].module || !prototype_checked_module_elaborated_view(
				images[i].module
			) || (images[i].count != 0 && !images[i].bytes) || (i != 0 &&
			checked_module_image_compare(&images[i - 1], &images[i]) >= 0)) {
			return -1;
		}
	}
	struct prototype_checked_module_set* set = calloc(1, sizeof(*set));
	if (!set) return -1;
	set->images = images;
	set->count = image_count;
	*p_set = set;
	return 0;
}

static int checked_module_set_check_conflicts(
	struct prototype_checked_module_set* set,
	struct prototype_checked_module_set_report* p_report
) {
	for (size_t i = 0; i < set->count; ++i) {
		for (size_t j = i + 1; j < set->count; ++j) {
			int kind;
			int conflict = prototype_checked_modules_conflict(
				set->images[i].module, set->images[j].module, &kind
			);
			if (conflict < 0) {
				return -1;
			}
			if (conflict != 0) {
				p_report->status = PROTOTYPE_CHECKED_MODULE_SET_CONFLICT;
				p_report->export_kind = kind;
				p_report->left_module = set->images[i].input_index;
				p_report->right_module = set->images[j].input_index;
				return 0;
			}
		}
	}
	p_report->status = PROTOTYPE_CHECKED_MODULE_SET_COMPLETE;
	return 0;
}

int prototype_checked_module_set_create(
	const struct prototype_checked_module* const* modules,
	size_t module_count,
	struct prototype_checked_module_set** p_set,
	struct prototype_checked_module_set_report* p_report
) {
	if (!p_set || !p_report || (module_count != 0 && !modules)) return -1;
	*p_set = NULL;
	*p_report = (struct prototype_checked_module_set_report) {
		.status = PROTOTYPE_CHECKED_MODULE_SET_MALFORMED,
		.left_module = SIZE_MAX,
		.right_module = SIZE_MAX
	};
	struct prototype_checked_module_set* set = NULL;
	int prepare_status = prototype_checked_module_set_prepare(
		modules, module_count, &set
	);
	if (prepare_status < 0) return -1;
	if (prepare_status > 0) return 0;
	if (checked_module_set_check_conflicts(set, p_report) != 0 ||
		p_report->status != PROTOTYPE_CHECKED_MODULE_SET_COMPLETE) {
		prototype_checked_module_set_destroy(set);
		return 0;
	}
	*p_set = set;
	return 0;
}

void prototype_checked_module_set_destroy(struct prototype_checked_module_set* set) {
	if (!set) return;
	for (size_t i = 0; i < set->count; ++i) {
		checked_module_image_destroy(&set->images[i]);
	}
	free(set->images);
	free(set);
}

size_t prototype_checked_module_set_count(const struct prototype_checked_module_set* set) {
	return set ? set->count : 0;
}

const struct prototype_checked_module* prototype_checked_module_set_at(
	const struct prototype_checked_module_set* set,
	size_t index
) {
	return set && index < set->count ? set->images[index].module : NULL;
}

int prototype_checked_module_set_canonical_image_at(
	const struct prototype_checked_module_set* set,
	size_t index,
	const unsigned char** p_bytes,
	size_t* p_count
) {
	if (!set || index >= set->count || !p_bytes || !p_count) return -1;
	*p_bytes = set->images[index].bytes;
	*p_count = set->images[index].count;
	return 0;
}
