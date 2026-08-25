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
