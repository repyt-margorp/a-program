#include <stdio.h>
#include <string.h>

#include "a_program/frontend/ast.h"
#include "a_program/frontend/source_schedule.h"

int main(void) {
	struct prototype_ast_import_def imports[1];
	struct prototype_ast_type_expectation_def expectations[1];
	struct prototype_ast_term_assignment_def assignments[2];
	memset(imports, 0, sizeof(imports));
	memset(expectations, 0, sizeof(expectations));
	memset(assignments, 0, sizeof(assignments));
	imports[0].source_entry_id = 3;
	expectations[0].source_entry_id = 1;
	assignments[0].source_entry_id = 4;
	assignments[1].source_entry_id = 2;

	struct prototype_ast_db asts;
	memset(&asts, 0, sizeof(asts));
	asts.imports = imports;
	asts.import_count = 1;
	asts.expectations = expectations;
	asts.expectation_count = 1;
	asts.assignments = assignments;
	asts.assignment_count = 2;

	struct prototype_source_schedule_item items[4];
	struct prototype_source_schedule schedule;
	if (prototype_source_schedule_required_capacity(&asts) != 4 ||
		prototype_source_schedule_build(
			&asts, items, sizeof(items) / sizeof(items[0]), &schedule
		) != 0 || schedule.item_count != 4 ||
		schedule.items[0].kind != PROTOTYPE_SOURCE_SCHEDULE_EXPECTATION ||
		schedule.items[0].table_id != 0 ||
		schedule.items[1].kind != PROTOTYPE_SOURCE_SCHEDULE_ASSIGNMENT ||
		schedule.items[1].table_id != 1 ||
		schedule.items[2].kind != PROTOTYPE_SOURCE_SCHEDULE_IMPORT ||
		schedule.items[3].kind != PROTOTYPE_SOURCE_SCHEDULE_ASSIGNMENT ||
		schedule.items[3].table_id != 0) {
		fprintf(stderr, "source schedule did not preserve source-entry order\n");
		return 1;
	}

	assignments[0].source_entry_id = 1;
	if (prototype_source_schedule_build(
			&asts, items, sizeof(items) / sizeof(items[0]), &schedule
		) == 0) {
		fprintf(stderr, "source schedule accepted duplicate source identity\n");
		return 1;
	}
	return 0;
}
