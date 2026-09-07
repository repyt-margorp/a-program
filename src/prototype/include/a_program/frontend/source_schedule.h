#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_SOURCE_SCHEDULE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_SOURCE_SCHEDULE_H

#include <stddef.h>
#include <stdint.h>

struct prototype_ast_db;

enum prototype_source_schedule_item_kind {
	PROTOTYPE_SOURCE_SCHEDULE_IMPORT = 1,
	PROTOTYPE_SOURCE_SCHEDULE_EXPECTATION,
	PROTOTYPE_SOURCE_SCHEDULE_ASSIGNMENT
};

/*
 * Immutable source-order coordination record. It belongs to neither Layer C
 * nor Layer T and deliberately contains no semantic graph identifier.
 */
struct prototype_source_schedule_item {
	uint32_t source_entry_id;
	uint32_t table_id;
	int kind;
};

struct prototype_source_schedule {
	const struct prototype_source_schedule_item* items;
	size_t item_count;
};

size_t prototype_source_schedule_required_capacity(
	const struct prototype_ast_db* asts
);

int prototype_source_schedule_build(
	const struct prototype_ast_db* asts,
	struct prototype_source_schedule_item* items,
	size_t item_capacity,
	struct prototype_source_schedule* schedule
);

#endif
