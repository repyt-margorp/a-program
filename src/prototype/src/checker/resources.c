#include "internal.h"

#include <stdlib.h>
#include <string.h>

static int occurrence_child(
	const struct prototype_semantic_occurrence_graph_view* graph,
	uint32_t occurrence_id,
	int role,
	uint32_t ordinal,
	uint32_t* p_child
) {
	if (!graph || occurrence_id >= graph->occurrence_count || !p_child) {
		return -1;
	}
	const struct prototype_semantic_occurrence* occurrence =
		&graph->occurrences[occurrence_id];
	for (uint32_t i = 0; i < occurrence->edge_count; ++i) {
		const struct prototype_semantic_occurrence_edge* edge =
			&graph->edges[occurrence->first_edge + i];
		if (edge->role == role && edge->ordinal == ordinal) {
			*p_child = edge->child_occurrence;
			return 0;
		}
	}
	return -1;
}

static int solution_view(
	const struct prototype_checker_usage_solution* solutions,
	size_t solution_count,
	const struct prototype_usage_entry* entries,
	size_t entry_count,
	uint32_t occurrence_id,
	struct prototype_usage_vector* p_usage
) {
	if (!solutions || !p_usage || occurrence_id >= solution_count) {
		return -1;
	}
	const struct prototype_checker_usage_solution* solution =
		&solutions[occurrence_id];
	if (solution->first_entry > entry_count ||
		solution->entry_count > entry_count - solution->first_entry ||
		(solution->entry_count != 0 && !entries)) {
		return -1;
	}
	p_usage->entries = solution->entry_count == 0 ? NULL :
		(struct prototype_usage_entry*)&entries[solution->first_entry];
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
	const struct prototype_checker_usage_solution* solutions,
	size_t solution_count,
	const struct prototype_usage_entry* entries,
	size_t entry_count,
	struct prototype_usage_vector* target,
	uint32_t child,
	int scalar,
	int join
) {
	struct prototype_usage_vector child_usage;
	return solution_view(
		solutions, solution_count, entries, entry_count, child, &child_usage
	) != 0 || usage_accumulate(
		target, &child_usage, scalar, join
	) != 0 ? -1 : 0;
}

static int context_contains_binding(
	const struct prototype_semantic_context_graph_view* contexts,
	uint32_t context_id,
	uint32_t binding_id
) {
	while (context_id != 0) {
		const struct prototype_semantic_context* context =
			&contexts->contexts[context_id];
		if (context->binding_id == binding_id) {
			return 1;
		}
		context_id = context->parent;
	}
	return 0;
}

static int clear_local_usage(
	const struct prototype_semantic_context_graph_view* contexts,
	uint32_t parent_context,
	uint32_t local_context,
	struct prototype_usage_vector* usage
) {
	uint32_t cursor = local_context;
	while (cursor != 0) {
		const struct prototype_semantic_context* context =
			&contexts->contexts[cursor];
		if (!context_contains_binding(
				contexts, parent_context, context->binding_id
			) && prototype_usage_vector_set(
				usage, context->binding_id, PROTOTYPE_USAGE_ZERO
			) != 0) {
			return -1;
		}
		cursor = context->parent;
	}
	return 0;
}

static int callable_binder_grade(
	const struct prototype_semantic_occurrence_graph_view* graph,
	const struct prototype_checker_usage_solution* solutions,
	uint32_t occurrence_id,
	int* p_grade
) {
	for (size_t depth = 0;
		occurrence_id < graph->occurrence_count && depth < graph->occurrence_count;
		++depth) {
		const struct prototype_semantic_occurrence* occurrence =
			&graph->occurrences[occurrence_id];
		if (occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_LAMBDA) {
			*p_grade = solutions[occurrence_id].binder_usage;
			return 0;
		}
		if (occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_REFERENCE ||
			occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_EXPECTED_TYPE) {
			occurrence_id = occurrence->wrapped_occurrence;
			continue;
		}
		uint32_t value;
		uint32_t computation;
		if (occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_FORCE &&
			occurrence_child(
				graph, occurrence_id, PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0, &value
			) == 0 && graph->occurrences[value].kind ==
				PROTOTYPE_SEMANTIC_OCCURRENCE_THUNK && occurrence_child(
				graph, value, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0,
				&computation
			) == 0) {
			occurrence_id = computation;
			continue;
		}
		*p_grade = PROTOTYPE_USAGE_MANY;
		return 0;
	}
	return -1;
}

static const struct prototype_effect_operation_declaration*
effect_declaration(
	const struct prototype_elaborated_module_view* module,
	int operation_id
) {
	for (size_t i = 0;
		i < module->intrinsic_environment.effect_operation_count;
		++i) {
		const struct prototype_effect_operation_declaration* declaration =
			&module->intrinsic_environment.effect_operations[i];
		if (declaration->operation_id == operation_id) {
			return declaration;
		}
	}
	return NULL;
}

static int operation_identity(
	const struct prototype_elaborated_module_view* module,
	uint32_t term_id,
	int* p_operation_id
) {
	const struct prototype_term* term = &module->terms.terms[term_id];
	while (term->tag == PROTOTYPE_TERM_APP) {
		term_id = term->as.app.function;
		term = &module->terms.terms[term_id];
	}
	if (term->tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	*p_operation_id = term->as.effect_operation.operation_id;
	return 0;
}

static int term_usage_visit(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t term_id,
	struct prototype_usage_vector* usage,
	uint32_t depth
);

static int term_usage_visit_scoped(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t term_id,
	const uint32_t* bindings,
	uint32_t binding_count,
	struct prototype_usage_vector* usage,
	uint32_t depth
) {
	struct prototype_usage_entry* entries = usage->capacity == 0 ? NULL : malloc(
		usage->capacity * sizeof(*entries)
	);
	if (usage->capacity != 0 && !entries) {
		return -1;
	}
	struct prototype_usage_vector local;
	prototype_usage_vector_init(&local, entries, usage->capacity);
	int status = term_usage_visit(terms, term_id, &local, depth + 1);
	for (uint32_t i = 0; status == 0 && i < binding_count; ++i) {
		status = prototype_usage_vector_set(
			&local, bindings[i], PROTOTYPE_USAGE_ZERO
		);
	}
	if (status == 0) {
		status = usage_accumulate(
			usage, &local, PROTOTYPE_USAGE_ONE, 0
		);
	}
	free(entries);
	return status;
}

static int term_usage_visit_branches(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t first_case,
	uint32_t case_count,
	struct prototype_usage_vector* usage,
	uint32_t depth
) {
	struct prototype_usage_entry* branch_entries = usage->capacity == 0 ? NULL :
		malloc(usage->capacity * sizeof(*branch_entries));
	struct prototype_usage_entry* case_entries = usage->capacity == 0 ? NULL :
		malloc(usage->capacity * sizeof(*case_entries));
	if (usage->capacity != 0 && (!branch_entries || !case_entries)) {
		free(branch_entries);
		free(case_entries);
		return -1;
	}
	struct prototype_usage_vector branches;
	struct prototype_usage_vector case_usage;
	prototype_usage_vector_init(&branches, branch_entries, usage->capacity);
	prototype_usage_vector_init(&case_usage, case_entries, usage->capacity);
	int status = 0;
	for (uint32_t i = 0; status == 0 && i < case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[first_case + i];
		case_usage.count = 0;
		status = term_usage_visit(
			terms, match_case->body, &case_usage, depth + 1
		);
		for (uint32_t j = 0; status == 0 && j < match_case->binder_count; ++j) {
			status = prototype_usage_vector_set(
				&case_usage,
				terms->case_binders[match_case->first_binder + j].binding_id,
				PROTOTYPE_USAGE_ZERO
			);
		}
		if (status == 0) {
			status = usage_accumulate(
				&branches, &case_usage, PROTOTYPE_USAGE_ONE, 1
			);
		}
	}
	if (status == 0) {
		status = usage_accumulate(
			usage, &branches, PROTOTYPE_USAGE_ONE, 0
		);
	}
	free(branch_entries);
	free(case_entries);
	return status;
}

static int term_usage_visit(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t term_id,
	struct prototype_usage_vector* usage,
	uint32_t depth
) {
	if (!terms || term_id >= terms->term_count || depth > terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR: {
			int previous;
			int combined;
			return prototype_usage_vector_get(
				usage, term->as.var.binding_id, &previous
			) != 0 || prototype_usage_grade_add(
				previous, PROTOTYPE_USAGE_ONE, &combined
			) != 0 ? -1 : prototype_usage_vector_set(
				usage, term->as.var.binding_id, combined
			);
		}
		case PROTOTYPE_TERM_APP:
			return term_usage_visit(
				terms, term->as.app.function, usage, depth + 1
			) != 0 ? -1 : term_usage_visit(
				terms, term->as.app.argument, usage, depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA:
			return term_usage_visit_scoped(
				terms, term->as.lambda.body, &term->as.lambda.binding_id, 1,
				usage, depth
			);
		case PROTOTYPE_TERM_MATCH:
			return term_usage_visit(
				terms, term->as.match.scrutinee, usage, depth + 1
			) != 0 ? -1 : term_usage_visit_branches(
				terms,
				term->as.match.first_case,
				term->as.match.case_count,
				usage,
				depth
			);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return term_usage_visit(
				terms, term->as.induction_hypothesis.argument, usage, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return term_usage_visit(
				terms, term->as.return_term.value, usage, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return term_usage_visit(
				terms, term->as.thunk.computation, usage, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return term_usage_visit(
				terms, term->as.force.value, usage, depth + 1
			);
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return term_usage_visit(
				terms, term->as.operation_request.operation, usage, depth + 1
			) != 0 || term_usage_visit(
				terms, term->as.operation_request.argument, usage, depth + 1
			) != 0 ? -1 : term_usage_visit(
				terms, term->as.operation_request.continuation, usage, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (term_usage_visit(
					terms,
					term->as.computation_fold.computation,
					usage,
					depth + 1
				) != 0 || term_usage_visit(
					terms,
					term->as.computation_fold.return_clause,
					usage,
					depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0;
				i < term->as.computation_fold.clause_count;
				++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[
						term->as.computation_fold.first_clause + i
					];
				if (term_usage_visit(
						terms, clause->operation, usage, depth + 1
					) != 0 || term_usage_visit(
						terms, clause->body, usage, depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		default:
			return 0;
	}
}

static int semantic_term_usage_analyze(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t term_id,
	struct prototype_usage_vector* usage
) {
	usage->count = 0;
	return term_usage_visit(terms, term_id, usage, 0);
}

static int store_solution(
	struct prototype_checker_usage_solution* solutions,
	uint32_t occurrence_id,
	struct prototype_usage_entry* entries,
	size_t entry_capacity,
	size_t* p_entry_count,
	const struct prototype_usage_vector* usage,
	int binder_usage
) {
	if (usage->count > entry_capacity - *p_entry_count) {
		return -1;
	}
	solutions[occurrence_id] = (struct prototype_checker_usage_solution) {
		.first_entry = (uint32_t)*p_entry_count,
		.entry_count = (uint32_t)usage->count,
		.binder_usage = binder_usage
	};
	if (usage->count != 0) {
		memcpy(
			&entries[*p_entry_count],
			usage->entries,
			usage->count * sizeof(*usage->entries)
		);
	}
	*p_entry_count += usage->count;
	return 0;
}

int prototype_checker_reconstruct_usage(
	const struct prototype_elaborated_module_view* module,
	struct prototype_checker_usage_solution** p_solutions,
	struct prototype_usage_entry** p_entries,
	size_t* p_entry_count
) {
	if (!module || !p_solutions || !p_entries || !p_entry_count ||
		module->occurrences.occurrence_count > UINT32_MAX ||
		(module->contexts.context_count != 0 &&
		 module->occurrences.occurrence_count >
			 SIZE_MAX / module->contexts.context_count)) {
		return -1;
	}
	size_t solution_count = module->occurrences.occurrence_count;
	size_t local_capacity = module->contexts.context_count;
	size_t entry_capacity = solution_count * local_capacity;
	struct prototype_checker_usage_solution* solutions = solution_count == 0 ?
		NULL : calloc(solution_count, sizeof(*solutions));
	struct prototype_usage_entry* entries = entry_capacity == 0 ? NULL : malloc(
		entry_capacity * sizeof(*entries)
	);
	struct prototype_usage_entry* local_entries = local_capacity == 0 ? NULL :
		malloc(local_capacity * sizeof(*local_entries));
	struct prototype_usage_entry* branch_entries = local_capacity == 0 ? NULL :
		malloc(local_capacity * sizeof(*branch_entries));
	struct prototype_usage_entry* case_entries = local_capacity == 0 ? NULL :
		malloc(local_capacity * sizeof(*case_entries));
	if ((solution_count != 0 && !solutions) ||
		(entry_capacity != 0 && !entries) ||
		(local_capacity != 0 &&
		 (!local_entries || !branch_entries || !case_entries))) {
		free(solutions);
		free(entries);
		free(local_entries);
		free(branch_entries);
		free(case_entries);
		return -1;
	}
	size_t entry_count = 0;
	const struct prototype_semantic_occurrence_graph_view* graph =
		&module->occurrences;
	for (uint32_t occurrence_id = 0;
		occurrence_id < graph->occurrence_count;
		++occurrence_id) {
		struct prototype_usage_vector usage;
		prototype_usage_vector_init(&usage, local_entries, local_capacity);
		int binder_usage = PROTOTYPE_USAGE_ZERO;
		const struct prototype_semantic_occurrence* occurrence =
			&graph->occurrences[occurrence_id];
#define ACCUMULATE(target, child, scalar, join) \
	accumulate_child(solutions, occurrence_id, entries, entry_count, \
		(target), (child), (scalar), (join))
		switch (occurrence->kind) {
		case PROTOTYPE_SEMANTIC_OCCURRENCE_ATOM:
		case PROTOTYPE_SEMANTIC_OCCURRENCE_CONSTRUCTOR:
			break;
		case PROTOTYPE_SEMANTIC_OCCURRENCE_VAR:
			if (occurrence->context_action_substitution != PROTOTYPE_INVALID_ID) {
				if (semantic_term_usage_analyze(
						&module->terms, occurrence->core_term, &usage
					) != 0) {
					goto fail;
				}
			} else if (prototype_usage_vector_set(
				&usage, occurrence->binding_id, PROTOTYPE_USAGE_ONE
			) != 0) {
				goto fail;
			}
			break;
		case PROTOTYPE_SEMANTIC_OCCURRENCE_REFERENCE:
		case PROTOTYPE_SEMANTIC_OCCURRENCE_EXPECTED_TYPE:
			if (ACCUMULATE(
					&usage, occurrence->wrapped_occurrence,
					PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				goto fail;
			}
			break;
		case PROTOTYPE_SEMANTIC_OCCURRENCE_LAMBDA: {
			uint32_t body;
			if (occurrence_child(
					graph, occurrence_id, PROTOTYPE_TERM_CHILD_BODY, 0, &body
				) != 0 || ACCUMULATE(
					&usage, body, PROTOTYPE_USAGE_ONE, 0
				) != 0 || prototype_usage_vector_get(
					&usage, occurrence->binding_id, &binder_usage
				) != 0 || prototype_usage_vector_set(
					&usage, occurrence->binding_id, PROTOTYPE_USAGE_ZERO
				) != 0) {
				goto fail;
			}
			break;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_APP: {
			uint32_t function;
			uint32_t argument;
			int argument_scale;
			if (occurrence_child(
					graph, occurrence_id, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
					&function
				) != 0 || occurrence_child(
					graph, occurrence_id, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
					&argument
				) != 0 || ACCUMULATE(
					&usage, function, PROTOTYPE_USAGE_ONE, 0
				) != 0 || callable_binder_grade(
					graph, solutions, function, &argument_scale
				) != 0 || ACCUMULATE(
					&usage, argument, argument_scale, 0
				) != 0) {
				goto fail;
			}
			break;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_RETURN:
		case PROTOTYPE_SEMANTIC_OCCURRENCE_THUNK:
		case PROTOTYPE_SEMANTIC_OCCURRENCE_FORCE: {
			int role = occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_RETURN ?
				PROTOTYPE_TERM_CHILD_RETURN_VALUE : occurrence->kind ==
				PROTOTYPE_SEMANTIC_OCCURRENCE_THUNK ?
				PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION :
				PROTOTYPE_TERM_CHILD_FORCE_VALUE;
			uint32_t child;
			if (occurrence_child(
					graph, occurrence_id, role, 0, &child
				) != 0 || ACCUMULATE(
					&usage, child, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				goto fail;
			}
			break;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_REQUEST: {
			uint32_t operation;
			uint32_t argument;
			uint32_t continuation;
			int operation_id;
			if (occurrence_child(
					graph, occurrence_id,
					PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0, &operation
				) != 0 || occurrence_child(
					graph, occurrence_id,
					PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0, &argument
				) != 0 || occurrence_child(
					graph, occurrence_id,
					PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0, &continuation
				) != 0 || ACCUMULATE(
					&usage, operation, PROTOTYPE_USAGE_ONE, 0
				) != 0 || ACCUMULATE(
					&usage, argument, PROTOTYPE_USAGE_ONE, 0
				) != 0 || operation_identity(
					module, graph->occurrences[operation].core_term, &operation_id
				) != 0) {
				goto fail;
			}
			const struct prototype_effect_operation_declaration* declaration =
				effect_declaration(module, operation_id);
			if (!declaration) {
				goto fail;
			}
			int scale = declaration->resumption_multiplicity ==
					PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE ?
					PROTOTYPE_USAGE_ZERO : declaration->resumption_multiplicity ==
					PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT ?
					PROTOTYPE_USAGE_ONE : PROTOTYPE_USAGE_MANY;
			if (ACCUMULATE(&usage, continuation, scale, 0) != 0) {
				goto fail;
			}
			break;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH: {
			uint32_t scrutinee;
			struct prototype_usage_vector branches;
			prototype_usage_vector_init(
				&branches, branch_entries, local_capacity
			);
			if (occurrence_child(
					graph, occurrence_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
					&scrutinee
				) != 0 || ACCUMULATE(
					&usage, scrutinee, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				goto fail;
			}
			for (uint32_t i = 0; i < occurrence->case_count; ++i) {
				struct prototype_usage_vector case_usage;
				prototype_usage_vector_init(
					&case_usage, case_entries, local_capacity
				);
				uint32_t body;
				const struct prototype_semantic_match_case* match_case =
					&graph->cases[occurrence->first_case + i];
				if (occurrence_child(
						graph, occurrence_id,
						PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i, &body
					) != 0 || ACCUMULATE(
						&case_usage, body, PROTOTYPE_USAGE_ONE, 0
					) != 0 || clear_local_usage(
						&module->contexts,
						occurrence->context_id,
						match_case->context_id,
						&case_usage
					) != 0 || usage_accumulate(
						&branches, &case_usage, PROTOTYPE_USAGE_ONE, 1
					) != 0) {
					goto fail;
				}
			}
			if (usage_accumulate(
					&usage, &branches, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				goto fail;
			}
			break;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_COMPUTATION_FOLD: {
			uint32_t computation;
			uint32_t return_clause;
			if (occurrence_child(
					graph, occurrence_id,
					PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0, &computation
				) != 0 || ACCUMULATE(
					&usage, computation, PROTOTYPE_USAGE_ONE, 0
				) != 0 || occurrence_child(
					graph, occurrence_id,
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0, &return_clause
				) != 0) {
				goto fail;
			}
			if (occurrence->fold_clause_count == 0) {
				if (ACCUMULATE(
						&usage, return_clause, PROTOTYPE_USAGE_ONE, 0
					) != 0) {
					goto fail;
				}
				break;
			}
			struct prototype_usage_vector branches;
			prototype_usage_vector_init(
				&branches, branch_entries, local_capacity
			);
			uint32_t return_body;
			if (occurrence_child(
					graph, return_clause, PROTOTYPE_TERM_CHILD_BODY, 0,
					&return_body
				) != 0 || ACCUMULATE(
					&branches, return_body, PROTOTYPE_USAGE_ONE, 1
				) != 0 || prototype_usage_vector_set(
					&branches,
					graph->occurrences[return_clause].binding_id,
					PROTOTYPE_USAGE_ZERO
				) != 0) {
				goto fail;
			}
			for (uint32_t i = 0; i < occurrence->fold_clause_count; ++i) {
				struct prototype_usage_vector clause_usage;
				prototype_usage_vector_init(
					&clause_usage, case_entries, local_capacity
				);
				uint32_t argument_lambda;
				uint32_t continuation_lambda;
				uint32_t body;
				const struct prototype_semantic_fold_clause* clause =
					&graph->fold_clauses[occurrence->first_fold_clause + i];
				if (occurrence_child(
						graph, occurrence_id,
						PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY, i,
						&argument_lambda
					) != 0 || occurrence_child(
						graph, argument_lambda, PROTOTYPE_TERM_CHILD_BODY, 0,
						&continuation_lambda
					) != 0 || occurrence_child(
						graph, continuation_lambda, PROTOTYPE_TERM_CHILD_BODY, 0,
						&body
					) != 0 || ACCUMULATE(
						&clause_usage, body, PROTOTYPE_USAGE_ONE, 0
					) != 0 || prototype_usage_vector_set(
						&clause_usage, clause->argument_binding_id,
						PROTOTYPE_USAGE_ZERO
					) != 0 || prototype_usage_vector_set(
						&clause_usage, clause->continuation_binding_id,
						PROTOTYPE_USAGE_ZERO
					) != 0 || usage_accumulate(
						&branches, &clause_usage, PROTOTYPE_USAGE_ONE, 1
					) != 0) {
					goto fail;
				}
			}
			if (usage_accumulate(
					&usage, &branches, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				goto fail;
			}
			break;
		}
		case PROTOTYPE_SEMANTIC_OCCURRENCE_INDUCTION_HYPOTHESIS: {
			uint32_t argument;
			if (occurrence_child(
					graph, occurrence_id,
					PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0, &argument
				) != 0 || ACCUMULATE(
					&usage, argument, PROTOTYPE_USAGE_ONE, 0
				) != 0) {
				goto fail;
			}
			break;
		}
		default:
			goto fail;
		}
#undef ACCUMULATE
		if (store_solution(
				solutions,
				occurrence_id,
				entries,
				entry_capacity,
				&entry_count,
				&usage,
				binder_usage
			) != 0) {
			goto fail;
		}
	}
	free(local_entries);
	free(branch_entries);
	free(case_entries);
	*p_solutions = solutions;
	*p_entries = entries;
	*p_entry_count = entry_count;
	return 0;

fail:
	free(local_entries);
	free(branch_entries);
	free(case_entries);
	free(solutions);
	free(entries);
	return -1;
}
