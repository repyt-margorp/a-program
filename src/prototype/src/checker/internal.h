#ifndef A_PROGRAM_PROTOTYPE_CHECKER_INTERNAL_H
#define A_PROGRAM_PROTOTYPE_CHECKER_INTERNAL_H

#include "a_program/checker/module.h"
#include "a_program/kernel/resource_usage.h"

struct prototype_checker_usage_solution {
	uint32_t first_entry;
	uint32_t entry_count;
	int binder_usage;
};

struct prototype_checker_universe_solution {
	uint32_t level_var;
	int value;
};

enum prototype_checker_reference_requirement {
	PROTOTYPE_CHECKER_REFERENCE_REQUIRED = 1,
	PROTOTYPE_CHECKER_REFERENCE_OPTIONAL = 2
};

enum prototype_checker_reference_target {
	PROTOTYPE_CHECKER_REFERENCE_TERM = 1,
	PROTOTYPE_CHECKER_REFERENCE_CONTEXT = 2,
	PROTOTYPE_CHECKER_REFERENCE_SUBSTITUTION = 3,
	PROTOTYPE_CHECKER_REFERENCE_IH_SCOPE = 4,
	PROTOTYPE_CHECKER_REFERENCE_OCCURRENCE = 5,
	PROTOTYPE_CHECKER_REFERENCE_CONTRACT = 6,
	PROTOTYPE_CHECKER_REFERENCE_TYPE = 7,
	PROTOTYPE_CHECKER_REFERENCE_CONSTRUCTOR = 8,
	PROTOTYPE_CHECKER_REFERENCE_TERM_EXPORT = 9,
	PROTOTYPE_CHECKER_REFERENCE_TYPE_EXPORT = 10,
	PROTOTYPE_CHECKER_REFERENCE_CONSTRUCTOR_EXPORT = 11,
	PROTOTYPE_CHECKER_REFERENCE_FUNCTION_GRAPH_ASSOCIATION = 12,
	PROTOTYPE_CHECKER_REFERENCE_SYMBOL = 13
};

typedef int (*prototype_checker_semantic_reference_visitor)(
	void* state,
	int target,
	int requirement,
	uint32_t reference
);

typedef int (*prototype_checker_term_reference_visitor)(
	void* state,
	int requirement,
	uint32_t term_id
);

int prototype_checker_visit_semantic_references(
	const struct prototype_elaborated_module_view* module,
	prototype_checker_semantic_reference_visitor visitor,
	void* state
);

/* Visit Term roots stored outside the Term graph. Term-to-Term edges are owned
 * by the Core Term schema and deliberately excluded from this inventory. */
int prototype_checker_visit_semantic_term_roots(
	const struct prototype_elaborated_module_view* module,
	prototype_checker_term_reference_visitor visitor,
	void* state
);

int prototype_checker_relocate_semantic_term_roots(
	struct prototype_elaborated_module* module,
	const uint32_t* relocation,
	size_t source_count
);

int prototype_checker_reconstruct_usage(
	const struct prototype_elaborated_module_view* module,
	struct prototype_checker_usage_solution** p_solutions,
	struct prototype_usage_entry** p_entries,
	size_t* p_entry_count
);

int prototype_checker_reconstruct_universes(
	const struct prototype_elaborated_module_view* module,
	struct prototype_checker_universe_solution** p_solutions,
	size_t* p_solution_count
);

#endif
