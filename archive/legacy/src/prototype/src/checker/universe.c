#include "internal.h"

#include <limits.h>
#include <stdlib.h>

enum checker_level_kind {
	CHECKER_LEVEL_EXPLICIT = 1,
	CHECKER_LEVEL_DERIVED_TERM = 2
};

struct checker_level {
	int kind;
	uint32_t key;
	int value;
};

struct checker_constraint {
	size_t lower;
	size_t upper;
	int offset;
};

struct checker_universe_graph {
	const struct prototype_elaborated_module_view* module;
	struct checker_level* levels;
	size_t level_count;
	size_t level_capacity;
	struct checker_constraint* constraints;
	size_t constraint_count;
	size_t constraint_capacity;
};

static int reserve_array(void** p_data, size_t* p_capacity, size_t required,
	size_t item_size) {
	if (required <= *p_capacity) {
		return 0;
	}
	size_t capacity = *p_capacity == 0 ? 16 : *p_capacity;
	while (capacity < required) {
		if (capacity > SIZE_MAX / 2) {
			return -1;
		}
		capacity *= 2;
	}
	if (capacity > SIZE_MAX / item_size) {
		return -1;
	}
	void* data = realloc(*p_data, capacity * item_size);
	if (!data) {
		return -1;
	}
	*p_data = data;
	*p_capacity = capacity;
	return 0;
}

static int level_index(
	struct checker_universe_graph* graph,
	int kind,
	uint32_t key,
	size_t* p_index
) {
	for (size_t i = 0; i < graph->level_count; ++i) {
		if (graph->levels[i].kind == kind && graph->levels[i].key == key) {
			*p_index = i;
			return 0;
		}
	}
	if (reserve_array(
			(void**)&graph->levels,
			&graph->level_capacity,
			graph->level_count + 1,
			sizeof(*graph->levels)
		) != 0) {
		return -1;
	}
	*p_index = graph->level_count;
	graph->levels[graph->level_count++] = (struct checker_level) {
		.kind = kind,
		.key = key,
		.value = 0
	};
	return 0;
}

static int add_constraint(
	struct checker_universe_graph* graph,
	size_t lower,
	size_t upper,
	int offset
) {
	if (offset < 0 || lower >= graph->level_count || upper >= graph->level_count) {
		return -1;
	}
	for (size_t i = 0; i < graph->constraint_count; ++i) {
		const struct checker_constraint* constraint = &graph->constraints[i];
		if (constraint->lower == lower && constraint->upper == upper &&
			constraint->offset == offset) {
			return 0;
		}
	}
	if (reserve_array(
			(void**)&graph->constraints,
			&graph->constraint_capacity,
			graph->constraint_count + 1,
			sizeof(*graph->constraints)
		) != 0) {
		return -1;
	}
	graph->constraints[graph->constraint_count++] =
		(struct checker_constraint) { lower, upper, offset };
	return 0;
}

static int explicit_level_for_term(
	struct checker_universe_graph* graph,
	uint32_t term_id,
	size_t* p_level
) {
	if (term_id >= graph->module->terms.term_count ||
		graph->module->terms.terms[term_id].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return -1;
	}
	return level_index(
		graph,
		CHECKER_LEVEL_EXPLICIT,
		graph->module->terms.terms[term_id].as.universe_var.level_var,
		p_level
	);
}

static int pure_family_body(
	const struct prototype_semantic_term_graph_view* terms,
	uint32_t family_id,
	uint32_t* p_body
) {
	if (family_id >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* thunk = &terms->terms[family_id];
	if (thunk->tag != PROTOTYPE_TERM_THUNK ||
		thunk->as.thunk.computation >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* lambda =
		&terms->terms[thunk->as.thunk.computation];
	if (lambda->tag != PROTOTYPE_TERM_LAMBDA ||
		lambda->as.lambda.body >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* returned = &terms->terms[lambda->as.lambda.body];
	if (returned->tag != PROTOTYPE_TERM_RETURN ||
		returned->as.return_term.value >= terms->term_count) {
		return -1;
	}
	*p_body = returned->as.return_term.value;
	return 0;
}

static int collect_type_level(
	struct checker_universe_graph* graph,
	uint32_t term_id,
	size_t* p_level,
	uint32_t depth
) {
	if (term_id >= graph->module->terms.term_count ||
		depth > graph->module->terms.term_count) {
		return -1;
	}
	const struct prototype_term* term = &graph->module->terms.terms[term_id];
	if (term->tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		size_t source;
		if (explicit_level_for_term(graph, term_id, &source) != 0 ||
			level_index(
				graph, CHECKER_LEVEL_DERIVED_TERM, term_id, p_level
			) != 0 || add_constraint(graph, source, *p_level, 1) != 0) {
			return -1;
		}
		return 0;
	}
	if (term->tag != PROTOTYPE_TERM_PI) {
		return -1;
	}
	if (level_index(
			graph, CHECKER_LEVEL_DERIVED_TERM, term_id, p_level
		) != 0) {
		return -1;
	}
	uint32_t body;
	if (pure_family_body(
			&graph->module->terms, term->as.pi.codomain_family, &body
		) != 0) {
		return -1;
	}
	size_t child_level;
	if (collect_type_level(
			graph, term->as.pi.domain, &child_level, depth + 1
		) == 0 && add_constraint(graph, child_level, *p_level, 0) != 0) {
		return -1;
	}
	if (collect_type_level(graph, body, &child_level, depth + 1) == 0 &&
		add_constraint(graph, child_level, *p_level, 0) != 0) {
		return -1;
	}
	return 0;
}

static int occurrence_child(
	const struct prototype_semantic_occurrence_graph_view* occurrences,
	uint32_t occurrence_id,
	int role,
	uint32_t ordinal,
	uint32_t* p_child
) {
	if (occurrence_id >= occurrences->occurrence_count) {
		return -1;
	}
	const struct prototype_semantic_occurrence* occurrence =
		&occurrences->occurrences[occurrence_id];
	for (uint32_t i = 0; i < occurrence->edge_count; ++i) {
		const struct prototype_semantic_occurrence_edge* edge =
			&occurrences->edges[occurrence->first_edge + i];
		if (edge->role == role && edge->ordinal == ordinal) {
			*p_child = edge->child_occurrence;
			return 0;
		}
	}
	return -1;
}

static int collect_cumulativity(
	struct checker_universe_graph* graph,
	uint32_t expected,
	uint32_t actual,
	uint32_t depth
) {
	if (expected >= graph->module->terms.term_count ||
		actual >= graph->module->terms.term_count || depth > 64) {
		return -1;
	}
	size_t lower;
	size_t upper;
	if (explicit_level_for_term(graph, actual, &lower) == 0 &&
		explicit_level_for_term(graph, expected, &upper) == 0) {
		return add_constraint(graph, lower, upper, 0);
	}
	const struct prototype_term* expected_term =
		&graph->module->terms.terms[expected];
	const struct prototype_term* actual_term =
		&graph->module->terms.terms[actual];
	if (expected_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		actual_term->tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		uint32_t row = expected_term->as.computation_type.label;
		uint32_t result = expected_term->as.computation_type.result;
		if (row < graph->module->terms.term_count && result <
			graph->module->terms.term_count &&
			graph->module->terms.terms[row].tag ==
				PROTOTYPE_TERM_EFFECT_ROW_EMPTY &&
			graph->module->terms.terms[result].tag ==
				PROTOTYPE_TERM_THUNK_TYPE) {
			return collect_cumulativity(
				graph,
				graph->module->terms.terms[result].as.thunk_type.computation,
				actual,
				depth + 1
			);
		}
	}
	if (expected_term->tag != actual_term->tag) {
		return 0;
	}
#define COLLECT_PAIR(left, right) \
	do { \
		if (collect_cumulativity(graph, (left), (right), depth + 1) != 0) { \
			return -1; \
		} \
	} while (0)
	switch (expected_term->tag) {
		case PROTOTYPE_TERM_APP:
			COLLECT_PAIR(expected_term->as.app.function,
				actual_term->as.app.function);
			COLLECT_PAIR(expected_term->as.app.argument,
				actual_term->as.app.argument);
			break;
		case PROTOTYPE_TERM_LAMBDA:
			COLLECT_PAIR(expected_term->as.lambda.body,
				actual_term->as.lambda.body);
			break;
		case PROTOTYPE_TERM_PI:
			COLLECT_PAIR(expected_term->as.pi.domain,
				actual_term->as.pi.domain);
			COLLECT_PAIR(expected_term->as.pi.codomain_family,
				actual_term->as.pi.codomain_family);
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			COLLECT_PAIR(expected_term->as.type_view.core,
				actual_term->as.type_view.core);
			COLLECT_PAIR(expected_term->as.type_view.source,
				actual_term->as.type_view.source);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			COLLECT_PAIR(expected_term->as.effect_row_union.left,
				actual_term->as.effect_row_union.left);
			COLLECT_PAIR(expected_term->as.effect_row_union.right,
				actual_term->as.effect_row_union.right);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			COLLECT_PAIR(expected_term->as.effect_row_forall.body,
				actual_term->as.effect_row_forall.body);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			COLLECT_PAIR(expected_term->as.effect_row_operation.latent_row,
				actual_term->as.effect_row_operation.latent_row);
			break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			COLLECT_PAIR(expected_term->as.computation_type.label,
				actual_term->as.computation_type.label);
			COLLECT_PAIR(expected_term->as.computation_type.result,
				actual_term->as.computation_type.result);
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			COLLECT_PAIR(expected_term->as.thunk_type.computation,
				actual_term->as.thunk_type.computation);
			break;
		default:
			break;
	}
#undef COLLECT_PAIR
	return 0;
}

static int collect_occurrence_constraints(
	struct checker_universe_graph* graph,
	uint32_t occurrence_id
) {
	const struct prototype_semantic_occurrence* occurrence =
		&graph->module->occurrences.occurrences[occurrence_id];
	size_t ignored;
	(void)collect_type_level(
		graph, occurrence->asserted_classifier, &ignored, 0
	);
	size_t subject_level;
	size_t classifier_level;
	if (explicit_level_for_term(
			graph, occurrence->core_term, &subject_level
		) == 0 && explicit_level_for_term(
			graph, occurrence->asserted_classifier, &classifier_level
		) == 0 && add_constraint(
			graph, subject_level, classifier_level, 1
		) != 0) {
		return -1;
	}
	const struct prototype_term* core =
		&graph->module->terms.terms[occurrence->core_term];
	if (core->tag == PROTOTYPE_TERM_PI && explicit_level_for_term(
			graph, occurrence->asserted_classifier, &classifier_level
		) == 0) {
		uint32_t body;
		if (pure_family_body(
				&graph->module->terms, core->as.pi.codomain_family, &body
			) != 0) {
			return -1;
		}
		size_t child_level;
		if (collect_type_level(
				graph, core->as.pi.domain, &child_level, 0
			) == 0 && add_constraint(
				graph, child_level, classifier_level, 0
			) != 0) {
			return -1;
		}
		if (collect_type_level(graph, body, &child_level, 0) == 0 &&
			add_constraint(graph, child_level, classifier_level, 0) != 0) {
			return -1;
		}
	}
	if (occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_APP) {
		uint32_t function;
		uint32_t argument;
		if (occurrence_child(
				&graph->module->occurrences, occurrence_id,
				PROTOTYPE_TERM_CHILD_FUNCTION, 0, &function
			) != 0 || occurrence_child(
				&graph->module->occurrences, occurrence_id,
				PROTOTYPE_TERM_CHILD_ARGUMENT, 0, &argument
			) != 0) {
			return -1;
		}
		const struct prototype_term* function_classifier =
			&graph->module->terms.terms[graph->module->occurrences.occurrences[
				function
			].asserted_classifier];
		if (function_classifier->tag == PROTOTYPE_TERM_PI && collect_cumulativity(
				graph,
				function_classifier->as.pi.domain,
				graph->module->occurrences.occurrences[argument].asserted_classifier,
				0
			) != 0) {
			return -1;
		}
	}
	if (occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_EXPECTED_TYPE) {
		return collect_cumulativity(
			graph,
			occurrence->asserted_classifier,
			graph->module->occurrences.occurrences[
				occurrence->wrapped_occurrence
			].asserted_classifier,
			0
		);
	}
	if (occurrence->kind == PROTOTYPE_SEMANTIC_OCCURRENCE_MATCH &&
		explicit_level_for_term(
			graph, occurrence->asserted_classifier, &classifier_level
		) == 0) {
		for (uint32_t i = 0; i < occurrence->case_count; ++i) {
			uint32_t branch;
			if (occurrence_child(
					&graph->module->occurrences, occurrence_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i, &branch
				) != 0) {
				return -1;
			}
			if (explicit_level_for_term(
					graph,
					graph->module->occurrences.occurrences[branch].asserted_classifier,
					&subject_level
				) == 0 && add_constraint(
					graph, subject_level, classifier_level, 0
				) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

static int solve_graph(struct checker_universe_graph* graph) {
	for (size_t pass = 0; pass <= graph->level_count; ++pass) {
		int changed = 0;
		for (size_t i = 0; i < graph->constraint_count; ++i) {
			const struct checker_constraint* constraint = &graph->constraints[i];
			int lower_value = graph->levels[constraint->lower].value;
			if (lower_value > INT_MAX - constraint->offset) {
				return -1;
			}
			int required = lower_value + constraint->offset;
			if (graph->levels[constraint->upper].value < required) {
				if (pass == graph->level_count) {
					return -1;
				}
				graph->levels[constraint->upper].value = required;
				changed = 1;
			}
		}
		if (!changed) {
			return 0;
		}
	}
	return -1;
}

int prototype_checker_reconstruct_universes(
	const struct prototype_elaborated_module_view* module,
	struct prototype_checker_universe_solution** p_solutions,
	size_t* p_solution_count
) {
	if (!module || !p_solutions || !p_solution_count) {
		return -1;
	}
	struct checker_universe_graph graph = { .module = module };
	for (uint32_t i = 0; i < module->terms.term_count; ++i) {
		if (module->terms.terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
			size_t ignored;
			if (explicit_level_for_term(&graph, i, &ignored) != 0) {
				goto fail;
			}
		}
	}
	for (uint32_t i = 0; i < module->occurrences.occurrence_count; ++i) {
		if (collect_occurrence_constraints(&graph, i) != 0) {
			goto fail;
		}
	}
	for (uint32_t i = 1; i < module->contexts.context_count; ++i) {
		size_t ignored;
		(void)collect_type_level(
			&graph, module->contexts.contexts[i].classifier, &ignored, 0
		);
	}
	if (solve_graph(&graph) != 0) {
		goto fail;
	}
	size_t count = 0;
	for (size_t i = 0; i < graph.level_count; ++i) {
		if (graph.levels[i].kind == CHECKER_LEVEL_EXPLICIT) {
			++count;
		}
	}
	struct prototype_checker_universe_solution* solutions = count == 0 ? NULL :
		malloc(count * sizeof(*solutions));
	if (count != 0 && !solutions) {
		goto fail;
	}
	size_t cursor = 0;
	for (size_t i = 0; i < graph.level_count; ++i) {
		if (graph.levels[i].kind == CHECKER_LEVEL_EXPLICIT) {
			solutions[cursor++] = (struct prototype_checker_universe_solution) {
				.level_var = graph.levels[i].key,
				.value = graph.levels[i].value
			};
		}
	}
	free(graph.levels);
	free(graph.constraints);
	*p_solutions = solutions;
	*p_solution_count = count;
	return 0;

fail:
	free(graph.levels);
	free(graph.constraints);
	return -1;
}
