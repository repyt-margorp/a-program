#ifndef A_PROGRAM_PROTOTYPE_CORE_READ_H
#define A_PROGRAM_PROTOTYPE_CORE_READ_H

#include <stddef.h>

#include "a_program/core/graph.h"

/*
 * Immutable, revision-pinned structural access to the Core calculation graph.
 * The provider is opaque outside the Core implementation. Accessors copy
 * records by value so callers cannot retain an interior TermDB pointer.
 */
struct prototype_core_read_snapshot {
	const void* provider;
	uint64_t graph_revision;
	size_t term_count;
	size_t case_count;
	size_t case_binder_count;
	size_t ih_scope_count;
	size_t computation_fold_clause_count;
};

struct prototype_core_match_case_view {
	int label_symbol_id;
	struct prototype_match_case match_case;
};

int prototype_core_read_snapshot_validate(
	const struct prototype_core_read_snapshot* snapshot
);

int prototype_core_read_term(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t term_id,
	struct prototype_term* term
);

int prototype_core_read_child_count(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t term_id,
	uint32_t* child_count
);

int prototype_core_read_child_at(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t term_id,
	uint32_t child_index,
	struct prototype_term_child* child
);

int prototype_core_read_constructor_spine(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t term_id,
	uint32_t* head,
	uint32_t* owner,
	uint32_t* constructor_id,
	uint32_t* arguments,
	uint32_t argument_capacity,
	uint32_t* argument_count
);

int prototype_core_read_match_case(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t case_id,
	struct prototype_core_match_case_view* match_case
);

int prototype_core_read_case_binder(
	const struct prototype_core_read_snapshot* snapshot,
	uint32_t binder_id,
	struct prototype_case_binder* binder
);

#endif
