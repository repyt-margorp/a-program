static int term_is_saturated_effect_application(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	uint32_t argument_count = 0;
	uint32_t head = term_id;
	if (!terms || term_id >= terms->term_count) {
		return 0;
	}
	while (head < terms->term_count &&
		terms->terms[head].tag == PROTOTYPE_TERM_APP) {
		head = terms->terms[head].as.app.function;
		argument_count++;
	}
	if (head >= terms->term_count ||
		terms->terms[head].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return 0;
	}
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(
			terms->terms[head].as.effect_operation.operation_id
		);
	return declaration && argument_count >= declaration->arity;
}

int prototype_operation_graph_induction_edge(
	const struct prototype_operation_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t operation_id,
	struct prototype_operation_induction_edge* p_edge
) {
	if (!graph || !terms || !p_edge || operation_id >= graph->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* induction =
		prototype_operation_graph_get(graph, operation_id);
	if (!induction || induction->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
		induction->argument >= graph->operation_count ||
		induction->scrutinee >= graph->operation_count ||
		induction->referenced_ast_binder_id == PROTOTYPE_INVALID_ID ||
		induction->ih_scope_id >= terms->ih_scope_count ||
		induction->ih_case_index == PROTOTYPE_INVALID_ID ||
		induction->ih_field_index == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	const struct prototype_operation_node* owner =
		prototype_operation_graph_get(graph, induction->scrutinee);
	const struct prototype_operation_node* argument =
		prototype_operation_graph_get(graph, induction->argument);
	if (!owner || !argument || owner->tag != PROTOTYPE_OPERATION_MATCH ||
		argument->tag != PROTOTYPE_OPERATION_VAR ||
		argument->referenced_ast_binder_id !=
			induction->referenced_ast_binder_id ||
		owner->core_term >= terms->term_count ||
		induction->core_term >= terms->term_count ||
		argument->core_term >= terms->term_count ||
		terms->terms[owner->core_term].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[induction->core_term].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		terms->terms[argument->core_term].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	const struct prototype_term* core_match = &terms->terms[owner->core_term];
	const struct prototype_term* core_induction =
		&terms->terms[induction->core_term];
	if (induction->ih_case_index >= owner->case_count ||
		induction->ih_case_index >= core_match->as.match.case_count ||
		owner->first_case > graph->case_count ||
		owner->case_count > graph->case_count - owner->first_case ||
		core_match->as.match.first_case > terms->case_count ||
		core_match->as.match.case_count >
			terms->case_count - core_match->as.match.first_case ||
		core_induction->as.induction_hypothesis.ih_scope_id !=
			induction->ih_scope_id ||
		terms->ih_scopes[induction->ih_scope_id].match_term != owner->core_term ||
		core_induction->as.induction_hypothesis.argument != argument->core_term) {
		return -1;
	}
	const struct prototype_operation_match_case* typed_case =
		&graph->cases[owner->first_case + induction->ih_case_index];
	const struct prototype_match_case* core_case =
		&terms->cases[
			core_match->as.match.first_case + induction->ih_case_index
		];
	if (induction->ih_field_index >= typed_case->binder_count ||
		induction->ih_field_index >= core_case->binder_count ||
		typed_case->ast_binder_ids[induction->ih_field_index] !=
			induction->referenced_ast_binder_id ||
		core_case->first_binder > terms->case_binder_count ||
		core_case->binder_count >
			terms->case_binder_count - core_case->first_binder) {
		return -1;
	}
	const struct prototype_case_binder* core_binder =
		&terms->case_binders[core_case->first_binder + induction->ih_field_index];
	if (!core_binder->is_recursive ||
		core_binder->binding_id !=
			terms->terms[argument->core_term].as.var.binding_id) {
		return -1;
	}
	*p_edge = (struct prototype_operation_induction_edge){
		.induction_operation = operation_id,
		.owner_match_operation = induction->scrutinee,
		.scope_id = induction->ih_scope_id,
		.case_index = induction->ih_case_index,
		.field_index = induction->ih_field_index,
		.source_ast_binder_id = induction->referenced_ast_binder_id,
		.argument_operation = induction->argument
	};
	return 0;
}

int prototype_operation_graph_validate(
	const struct prototype_operation_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
) {
	if (!graph || !terms || !contexts ||
		(graph->operation_count != 0 && !graph->operations) ||
		(graph->case_count != 0 && !graph->cases) ||
		(graph->fold_clause_count != 0 && !graph->fold_clauses)) {
		return -1;
	}
	for (uint32_t i = 0; i < graph->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			prototype_operation_graph_get(graph, i);
		if (!operation ||
			operation->tag < PROTOTYPE_OPERATION_ATOM ||
			operation->tag > PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			!prototype_context_get(contexts, operation->context_id)) {
			return -1;
		}
		if (operation->application_role < PROTOTYPE_TERM_APPLICATION_NONE ||
			operation->application_role >
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION ||
			(operation->tag != PROTOTYPE_OPERATION_APP &&
			 operation->application_role != PROTOTYPE_TERM_APPLICATION_NONE) ||
			(operation->tag == PROTOTYPE_OPERATION_APP &&
			 operation->application_role == PROTOTYPE_TERM_APPLICATION_NONE)) {
			return -1;
		}
		uint32_t term_references[] = {
			operation->core_term,
			operation->known_classifier,
			operation->classifier,
			operation->binder_classifier
		};
		for (size_t j = 0; j < sizeof(term_references) / sizeof(term_references[0]); ++j) {
			if (term_references[j] != PROTOTYPE_INVALID_ID &&
				(term_references[j] >= terms->term_count ||
				 terms->terms[term_references[j]].tag == 0)) {
				return -1;
			}
		}
		if (operation->tag == PROTOTYPE_OPERATION_APP) {
			struct prototype_term_semantics semantics;
			if (prototype_term_semantics(
					terms, operation->core_term, &semantics
				) != 0 ||
				semantics.application_role != operation->application_role ||
				term_is_saturated_effect_application(terms, operation->core_term) ||
				(operation->application_role ==
						PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION &&
				 operation->category != PROTOTYPE_TERM_CATEGORY_VALUE)) {
				return -1;
			}
		}
		if (operation->tag == PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS) {
			struct prototype_operation_induction_edge edge;
			if (prototype_operation_graph_induction_edge(
					graph, terms, i, &edge
				) != 0) {
				return -1;
			}
		}
		uint32_t operation_references[] = {
			operation->function,
			operation->argument,
			operation->body,
			operation->scrutinee,
			operation->fold_return_operation
		};
		for (size_t j = 0;
			j < sizeof(operation_references) / sizeof(operation_references[0]);
			++j) {
			if (operation_references[j] != PROTOTYPE_INVALID_ID &&
				operation_references[j] >= graph->operation_count) {
				return -1;
			}
		}
		if (operation->tag == PROTOTYPE_OPERATION_MATCH &&
			operation->first_case != PROTOTYPE_INVALID_ID &&
			(operation->first_case > graph->case_count ||
			 operation->case_count > graph->case_count - operation->first_case)) {
			return -1;
		}
		if (operation->first_fold_clause != PROTOTYPE_INVALID_ID &&
			(operation->first_fold_clause > graph->fold_clause_count ||
			 operation->fold_clause_count >
				graph->fold_clause_count - operation->first_fold_clause)) {
			return -1;
		}
		if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD) {
			if (terms->terms[operation->core_term].tag !=
					PROTOTYPE_TERM_COMPUTATION_FOLD ||
				operation->function >= graph->operation_count) {
				return -1;
			}
			const struct prototype_term* fold =
				&terms->terms[operation->core_term];
			uint32_t return_operation_id =
				fold->as.computation_fold.clause_count == 0 ?
					operation->argument : operation->fold_return_operation;
			if (return_operation_id >= graph->operation_count) {
				return -1;
			}
			const struct prototype_operation_node* input_operation =
				prototype_operation_graph_get(graph, operation->function);
			const struct prototype_operation_node* return_operation =
				prototype_operation_graph_get(graph, return_operation_id);
			if (!input_operation || !return_operation ||
				input_operation->core_term != fold->as.computation_fold.computation ||
				return_operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
				return_operation->core_term != fold->as.computation_fold.return_clause ||
				operation->fold_clause_count != fold->as.computation_fold.clause_count) {
				return -1;
			}
			if (fold->as.computation_fold.clause_count != 0 &&
				(operation->first_fold_clause == PROTOTYPE_INVALID_ID ||
				operation->first_fold_clause > graph->fold_clause_count ||
				operation->fold_clause_count >
					graph->fold_clause_count - operation->first_fold_clause ||
				fold->as.computation_fold.first_clause >
					terms->computation_fold_clause_count ||
				fold->as.computation_fold.clause_count >
					terms->computation_fold_clause_count -
						fold->as.computation_fold.first_clause)) {
				return -1;
			}
			for (uint32_t clause_index = 0;
				clause_index < fold->as.computation_fold.clause_count;
				++clause_index) {
				const struct prototype_operation_computation_fold_clause* occurrence =
					prototype_operation_graph_get_fold_clause(
						graph, operation->first_fold_clause + clause_index
					);
				const struct prototype_computation_fold_clause* core_clause =
					&terms->computation_fold_clauses[
						fold->as.computation_fold.first_clause + clause_index
					];
				const struct prototype_operation_node* operation_occurrence =
					occurrence ? prototype_operation_graph_get(
						graph, occurrence->operation_operation
					) : NULL;
				const struct prototype_operation_node* clause_occurrence =
					occurrence ? prototype_operation_graph_get(
						graph, occurrence->clause_operation
					) : NULL;
				if (!occurrence || !operation_occurrence || !clause_occurrence ||
					operation_occurrence->core_term != core_clause->operation ||
					clause_occurrence->tag != PROTOTYPE_OPERATION_LAMBDA ||
					clause_occurrence->core_term != core_clause->body) {
					return -1;
				}
			}
		}
	}
	for (uint32_t i = 0; i < graph->case_count; ++i) {
		const struct prototype_operation_match_case* operation_case =
			prototype_operation_graph_get_case(graph, i);
		if (!operation_case ||
			operation_case->binder_count > 16 ||
			operation_case->body_operation >= graph->operation_count ||
			!prototype_context_get(contexts, operation_case->context_id) ||
			operation_case->constructor_owner >= terms->term_count ||
			terms->terms[operation_case->constructor_owner].tag == 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < graph->fold_clause_count; ++i) {
		const struct prototype_operation_computation_fold_clause* clause =
			prototype_operation_graph_get_fold_clause(graph, i);
		if (!clause || clause->operation_operation >= graph->operation_count ||
			clause->body_operation >= graph->operation_count ||
			clause->clause_operation >= graph->operation_count ||
			!prototype_context_get(contexts, clause->context_id)) {
			return -1;
		}
	}
	return 0;
}

void prototype_compile_metadata_operation_graph(
	struct prototype_compile_metadata* metadata,
	struct prototype_operation_graph* graph
) {
	if (!graph) {
		return;
	}
	memset(graph, 0, sizeof(*graph));
	if (!metadata) {
		return;
	}
	graph->operations = metadata->operations;
	graph->operation_count = metadata->operation_count;
	graph->operation_capacity = metadata->operation_capacity;
	graph->cases = metadata->operation_cases;
	graph->case_count = metadata->operation_case_count;
	graph->case_capacity = metadata->operation_case_capacity;
	graph->fold_clauses = metadata->operation_fold_clauses;
	graph->fold_clause_count = metadata->operation_fold_clause_count;
	graph->fold_clause_capacity = metadata->operation_fold_clause_capacity;
}

void prototype_compile_metadata_operation_graph_const(
	const struct prototype_compile_metadata* metadata,
	struct prototype_operation_graph* graph
) {
	if (!graph) {
		return;
	}
	memset(graph, 0, sizeof(*graph));
	if (!metadata) {
		return;
	}
	graph->operations = metadata->operations;
	graph->operation_count = metadata->operation_count;
	graph->operation_capacity = metadata->operation_capacity;
	graph->cases = metadata->operation_cases;
	graph->case_count = metadata->operation_case_count;
	graph->case_capacity = metadata->operation_case_capacity;
	graph->fold_clauses = metadata->operation_fold_clauses;
	graph->fold_clause_count = metadata->operation_fold_clause_count;
	graph->fold_clause_capacity = metadata->operation_fold_clause_capacity;
}

void prototype_compile_metadata_commit_operation_graph(
	struct prototype_compile_metadata* metadata,
	const struct prototype_operation_graph* graph
) {
	if (!metadata || !graph) {
		return;
	}
	metadata->operation_count = graph->operation_count;
	metadata->operation_case_count = graph->case_count;
	metadata->operation_fold_clause_count = graph->fold_clause_count;
}
