#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_USAGE_STATE_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_TYPING_USAGE_STATE_H

#include <stdint.h>

#include "a_program/frontend/typing_resource_usage.h"

#define PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY 4096
#define PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY 131072

struct prototype_occurrence_usage_solution {
	uint32_t first_entry;
	uint32_t entry_count;
	int binder_usage;
};

/* Rebuildable structural analysis cache. Accepted judgement identity owns a
 * copied usage vector; this workspace can be discarded and recomputed from the
 * TypedOccurrence, Context, and Substitution graphs. */
struct prototype_typing_usage_workspace {
	struct prototype_occurrence_usage_solution solutions[
		PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY
	];
	struct prototype_usage_entry entries[
		PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY
	];
	uint32_t entry_count;
};

#endif
