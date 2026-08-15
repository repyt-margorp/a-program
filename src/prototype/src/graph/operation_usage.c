#include "a_program/graph/operation_usage.h"

#include "a_program/kernel/context.h"
#include "a_program/kernel/resource_usage.h"

#include <string.h>

int prototype_operation_usage_solution_view(
	const struct prototype_operation_usage_solution* solutions,
	size_t solution_count,
	const struct prototype_usage_entry* entries,
	size_t entry_count,
	uint32_t operation_id,
	struct prototype_usage_vector* p_usage
) {
	if (!solutions || !p_usage || operation_id >= solution_count) {
		return -1;
	}
	const struct prototype_operation_usage_solution* solution =
		&solutions[operation_id];
	if (solution->first_entry > entry_count ||
		solution->entry_count > entry_count - solution->first_entry ||
		(solution->entry_count != 0 && !entries)) {
		return -1;
	}
	p_usage->entries = (struct prototype_usage_entry*)&entries[
		solution->first_entry
	];
	p_usage->count = solution->entry_count;
	p_usage->capacity = solution->entry_count;
	return 0;
}

static int usage_accumulate(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* source,
	int scalar,
	int join
) {
	if (!target || !source) {
		return -1;
	}
	for (size_t i = 0; i < source->count; ++i) {
		int scaled;
		int previous;
		int combined;
		if (prototype_usage_grade_multiply(
				scalar, source->entries[i].grade, &scaled
			) != 0 || prototype_usage_vector_get(
				target, source->entries[i].binding_id, &previous
			) != 0 || (join ? prototype_usage_grade_join(
				previous, scaled, &combined
			) : prototype_usage_grade_add(
				previous, scaled, &combined
			)) != 0 || prototype_usage_vector_set(
				target, source->entries[i].binding_id, combined
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int accumulate_child(
	const struct prototype_operation_usage_solution* solutions,
	size_t solution_count,
	const struct prototype_usage_entry* entries,
	size_t entry_count,
	struct prototype_usage_vector* target,
	uint32_t child_operation,
	int scalar,
	int join
) {
	struct prototype_usage_vector child;
	return prototype_operation_usage_solution_view(
		solutions,
		solution_count,
		entries,
		entry_count,
		child_operation,
		&child
	) != 0 || usage_accumulate(target, &child, scalar, join) != 0 ? -1 : 0;
}

static int clear_case_local_usage(
	const struct prototype_context_db* contexts,
	uint32_t match_context,
	uint32_t case_context,
	struct prototype_usage_vector* usage
) {
	if (!contexts || !usage || !prototype_context_get(
			contexts, match_context
		) || !prototype_context_get(contexts, case_context)) {
		return -1;
	}
	uint32_t cursor = case_context;
	while (cursor != prototype_context_empty(contexts)) {
		const struct prototype_context* entry = prototype_context_get(
			contexts, cursor
		);
		if (!entry) {
			return -1;
		}
		if (!prototype_context_contains_binding(
				contexts, match_context, entry->binding_id
			) && prototype_usage_vector_set(
				usage, entry->binding_id, PROTOTYPE_USAGE_ZERO
			) != 0) {
			return -1;
		}
		cursor = entry->parent;
	}
	return 0;
}

static int callable_binder_grade(
	const struct prototype_operation_graph* operations,
	const struct prototype_operation_usage_solution* solutions,
	uint32_t operation_id,
	int* p_grade
) {
	if (!operations || !solutions || !p_grade) {
		return -1;
	}
	for (size_t depth = 0;
		operation_id < operations->operation_count &&
		depth < operations->operation_count;
		++depth) {
		const struct prototype_operation_node* operation =
			&operations->operations[operation_id];
		if (operation->tag == PROTOTYPE_OPERATION_LAMBDA) {
			*p_grade = solutions[operation_id].binder_usage;
			return 0;
		}
		if (operation->tag == PROTOTYPE_OPERATION_NAME) {
			operation_id = operation->function;
			continue;
		}
		if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
			operation_id = operation->body;
			continue;
		}
		if (operation->tag == PROTOTYPE_OPERATION_FORCE &&
			operation->argument < operations->operation_count &&
			operations->operations[operation->argument].tag ==
				PROTOTYPE_OPERATION_THUNK) {
			operation_id = operations->operations[operation->argument].argument;
			continue;
		}
		/* Quantitative Pi domains are not represented yet. Conservatively permit
		 * an unknown higher-order callee to duplicate its argument. */
		*p_grade = PROTOTYPE_USAGE_MANY;
		return 0;
	}
	return -1;
}

static int store_solution(
	struct prototype_operation_usage_solution* solutions,
	uint32_t operation_id,
	struct prototype_usage_entry* entries,
	size_t entry_capacity,
	size_t* p_entry_count,
	const struct prototype_usage_vector* usage,
	int binder_usage
) {
	if (!solutions || !p_entry_count || !usage ||
		usage->count > entry_capacity - *p_entry_count) {
		return -1;
	}
	solutions[operation_id].first_entry = (uint32_t)*p_entry_count;
	solutions[operation_id].entry_count = (uint32_t)usage->count;
	solutions[operation_id].binder_usage = binder_usage;
	memcpy(
		&entries[*p_entry_count],
		usage->entries,
		usage->count * sizeof(*usage->entries)
	);
	*p_entry_count += usage->count;
	return 0;
}

int prototype_operation_usage_solve(
	const struct prototype_operation_graph* operations,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	struct prototype_operation_usage_solution* solutions,
	size_t solution_capacity,
	struct prototype_usage_entry* entries,
	size_t entry_capacity,
	size_t* p_entry_count
) {
	if (!operations || !terms || !solutions || !p_entry_count ||
		operations->operation_count > solution_capacity ||
		(entry_capacity != 0 && !entries)) {
		return -1;
	}
	*p_entry_count = 0;
	memset(
		solutions,
		0,
		operations->operation_count * sizeof(*solutions)
	);
	for (uint32_t operation_id = 0;
		operation_id < operations->operation_count;
		++operation_id) {
		struct prototype_usage_entry local_entries[PROTOTYPE_CONTEXT_CAPACITY];
		struct prototype_usage_vector usage;
		prototype_usage_vector_init(
			&usage, local_entries, PROTOTYPE_CONTEXT_CAPACITY
		);
		const struct prototype_operation_node* operation =
			&operations->operations[operation_id];
		int binder_usage = PROTOTYPE_USAGE_ZERO;
#define ACCUMULATE_CHILD(target, child, scalar, join) \
	accumulate_child(solutions, operation_id, entries, *p_entry_count, \
		(target), (child), (scalar), (join))
		switch (operation->tag) {
		case PROTOTYPE_OPERATION_ATOM:
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
			break;
		case PROTOTYPE_OPERATION_VAR:
			if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
				if (prototype_term_usage_analyze(
						terms, operation->core_term, &usage
					) != 0) {
					return -1;
				}
				break;
			}
			if (operation->core_term >= terms->term_count ||
				terms->terms[operation->core_term].tag != PROTOTYPE_TERM_VAR ||
				operation->binding_id == PROTOTYPE_INVALID_ID ||
				prototype_usage_vector_set(
					&usage,
					operation->binding_id,
					PROTOTYPE_USAGE_ONE
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_OPERATION_NAME:
			if (ACCUMULATE_CHILD(
					&usage, operation->function, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_OPERATION_ASCRIPTION:
			if (ACCUMULATE_CHILD(
					&usage, operation->body, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_OPERATION_LAMBDA:
			if (ACCUMULATE_CHILD(
					&usage, operation->body, PROTOTYPE_USAGE_ONE, 0
				) != 0 || prototype_usage_vector_get(
					&usage, operation->binding_id, &binder_usage
				) != 0 || prototype_usage_vector_set(
					&usage, operation->binding_id, PROTOTYPE_USAGE_ZERO
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_OPERATION_APP: {
			int argument_scale;
			if (ACCUMULATE_CHILD(
					&usage, operation->function, PROTOTYPE_USAGE_ONE, 0
				) != 0 || callable_binder_grade(
					operations, solutions, operation->function, &argument_scale
				) != 0 || ACCUMULATE_CHILD(
					&usage, operation->argument, argument_scale, 0
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_THUNK:
		case PROTOTYPE_OPERATION_FORCE:
			if (ACCUMULATE_CHILD(
					&usage, operation->argument, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_OPERATION_REQUEST: {
			int operation_identity;
			if (ACCUMULATE_CHILD(
					&usage, operation->function, PROTOTYPE_USAGE_ONE, 0
				) != 0 || ACCUMULATE_CHILD(
					&usage, operation->argument, PROTOTYPE_USAGE_ONE, 0
				) != 0 || prototype_term_effect_operation_identity(
					terms,
					operations->operations[operation->function].core_term,
					&operation_identity
				) != 0) {
				return -1;
			}
			const struct prototype_effect_operation_declaration* declaration =
				prototype_term_effect_operation_declaration(operation_identity);
			if (!declaration) {
				return -1;
			}
			int continuation_scale = declaration->resumption_multiplicity ==
					PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE ?
					PROTOTYPE_USAGE_ZERO : declaration->resumption_multiplicity ==
					PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT ?
					PROTOTYPE_USAGE_ONE : PROTOTYPE_USAGE_MANY;
			if (ACCUMULATE_CHILD(
					&usage, operation->body, continuation_scale, 0
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_OPERATION_MATCH: {
			if (operation->first_case > operations->case_count ||
				operation->case_count > operations->case_count -
					operation->first_case || ACCUMULATE_CHILD(
					&usage, operation->scrutinee, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				return -1;
			}
			struct prototype_usage_entry branch_entries[PROTOTYPE_CONTEXT_CAPACITY];
			struct prototype_usage_vector branches;
			prototype_usage_vector_init(
				&branches, branch_entries, PROTOTYPE_CONTEXT_CAPACITY
			);
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				const struct prototype_operation_match_case* operation_case =
					&operations->cases[operation->first_case + i];
				struct prototype_usage_entry case_entries[PROTOTYPE_CONTEXT_CAPACITY];
				struct prototype_usage_vector case_usage;
				prototype_usage_vector_init(
					&case_usage, case_entries, PROTOTYPE_CONTEXT_CAPACITY
				);
				if (ACCUMULATE_CHILD(
						&case_usage,
						operation_case->body_operation,
						PROTOTYPE_USAGE_ONE,
						0
					) != 0) {
					return -1;
				}
				if (contexts && clear_case_local_usage(
						contexts,
						operation->context_id,
						operation_case->context_id,
						&case_usage
					) != 0) {
					return -1;
				}
				if (usage_accumulate(
						&branches, &case_usage, PROTOTYPE_USAGE_ONE, 1
					) != 0) {
					return -1;
				}
			}
			if (usage_accumulate(
					&usage, &branches, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD: {
			if (ACCUMULATE_CHILD(
					&usage, operation->function, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				return -1;
			}
			if (operation->fold_clause_count == 0) {
				if (ACCUMULATE_CHILD(
						&usage, operation->argument, PROTOTYPE_USAGE_ONE, 0
					) != 0) {
					return -1;
				}
				break;
			}
			if (operation->first_fold_clause > operations->fold_clause_count ||
				operation->fold_clause_count > operations->fold_clause_count -
					operation->first_fold_clause) {
				return -1;
			}
			struct prototype_usage_entry branch_entries[PROTOTYPE_CONTEXT_CAPACITY];
			struct prototype_usage_vector branches;
			prototype_usage_vector_init(
				&branches, branch_entries, PROTOTYPE_CONTEXT_CAPACITY
			);
			if (operation->fold_return_operation >= operations->operation_count ||
				operations->operations[operation->fold_return_operation].tag !=
					PROTOTYPE_OPERATION_LAMBDA || ACCUMULATE_CHILD(
					&branches, operation->scrutinee, PROTOTYPE_USAGE_ONE, 1
				) != 0 || prototype_usage_vector_set(
					&branches,
					operations->operations[
						operation->fold_return_operation
					].binding_id,
					PROTOTYPE_USAGE_ZERO
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->fold_clause_count; ++i) {
				const struct prototype_operation_computation_fold_clause* clause =
					&operations->fold_clauses[operation->first_fold_clause + i];
				struct prototype_usage_entry clause_entries[PROTOTYPE_CONTEXT_CAPACITY];
				struct prototype_usage_vector clause_usage;
				prototype_usage_vector_init(
					&clause_usage, clause_entries, PROTOTYPE_CONTEXT_CAPACITY
				);
				if (ACCUMULATE_CHILD(
						&clause_usage,
						clause->body_operation,
						PROTOTYPE_USAGE_ONE,
						0
					) != 0 || prototype_usage_vector_set(
						&clause_usage,
						clause->argument_binder_id,
						PROTOTYPE_USAGE_ZERO
					) != 0 || prototype_usage_vector_set(
						&clause_usage,
						clause->continuation_binder_id,
						PROTOTYPE_USAGE_ZERO
					) != 0 || usage_accumulate(
						&branches, &clause_usage, PROTOTYPE_USAGE_ONE, 1
					) != 0) {
					return -1;
				}
			}
			if (usage_accumulate(
					&usage, &branches, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			if (ACCUMULATE_CHILD(
					&usage, operation->argument, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				return -1;
			}
			break;
		default:
			return -1;
		}
#undef ACCUMULATE_CHILD
		if (store_solution(
				solutions,
				operation_id,
				entries,
				entry_capacity,
				p_entry_count,
				&usage,
				binder_usage
			) != 0) {
			return -1;
		}
	}
	return 0;
}
