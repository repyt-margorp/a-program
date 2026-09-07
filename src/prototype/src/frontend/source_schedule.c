#include "a_program/frontend/source_schedule.h"

#include <stdlib.h>

#include "a_program/frontend/ast.h"

static int source_schedule_item_compare(const void* left, const void* right) {
	const struct prototype_source_schedule_item* a = left;
	const struct prototype_source_schedule_item* b = right;
	if (a->source_entry_id < b->source_entry_id) return -1;
	if (a->source_entry_id > b->source_entry_id) return 1;
	if (a->kind < b->kind) return -1;
	if (a->kind > b->kind) return 1;
	if (a->table_id < b->table_id) return -1;
	if (a->table_id > b->table_id) return 1;
	return 0;
}

size_t prototype_source_schedule_required_capacity(
	const struct prototype_ast_db* asts
) {
	if (!asts || asts->expectation_count > SIZE_MAX - asts->assignment_count ||
		asts->expectation_count + asts->assignment_count >
			SIZE_MAX - asts->import_count) {
		return SIZE_MAX;
	}
	return asts->expectation_count + asts->assignment_count + asts->import_count;
}

int prototype_source_schedule_build(
	const struct prototype_ast_db* asts,
	struct prototype_source_schedule_item* items,
	size_t item_capacity,
	struct prototype_source_schedule* schedule
) {
	if (!asts || !schedule) return -1;
	size_t required = prototype_source_schedule_required_capacity(asts);
	if (required == SIZE_MAX || required > item_capacity ||
		(required > 0 && !items)) return -1;

	size_t count = 0;
	for (uint32_t i = 0; i < asts->import_count; ++i) {
		items[count++] = (struct prototype_source_schedule_item) {
			.source_entry_id = asts->imports[i].source_entry_id,
			.table_id = i,
			.kind = PROTOTYPE_SOURCE_SCHEDULE_IMPORT
		};
	}
	for (uint32_t i = 0; i < asts->expectation_count; ++i) {
		items[count++] = (struct prototype_source_schedule_item) {
			.source_entry_id = asts->expectations[i].source_entry_id,
			.table_id = i,
			.kind = PROTOTYPE_SOURCE_SCHEDULE_EXPECTATION
		};
	}
	for (uint32_t i = 0; i < asts->assignment_count; ++i) {
		items[count++] = (struct prototype_source_schedule_item) {
			.source_entry_id = asts->assignments[i].source_entry_id,
			.table_id = i,
			.kind = PROTOTYPE_SOURCE_SCHEDULE_ASSIGNMENT
		};
	}
	qsort(items, count, sizeof(*items), source_schedule_item_compare);
	for (size_t i = 1; i < count; ++i) {
		if (items[i - 1].source_entry_id == items[i].source_entry_id) {
			return -1;
		}
	}
	*schedule = (struct prototype_source_schedule) {
		.items = items,
		.item_count = count
	};
	return 0;
}
