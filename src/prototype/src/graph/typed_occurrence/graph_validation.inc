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

int prototype_typed_occurrence_graph_induction_edge(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t operation_id,
	struct prototype_typed_occurrence_induction_edge* p_edge
) {
	if (!graph || !terms || !p_edge || operation_id >= graph->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* induction =
		prototype_typed_occurrence_graph_get(graph, operation_id);
	uint32_t argument_occurrence;
	if (!induction || induction->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		prototype_typed_occurrence_graph_child(
			graph, operation_id, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
			&argument_occurrence
		) != 0 ||
		induction->ih_owner_occurrence >= graph->occurrence_count ||
		induction->binding_id == PROTOTYPE_INVALID_ID ||
		induction->ih_scope_id >= terms->ih_scope_count ||
		induction->ih_case_index == PROTOTYPE_INVALID_ID ||
		induction->ih_field_index == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	const struct prototype_typed_occurrence* owner =
		prototype_typed_occurrence_graph_get(graph, induction->ih_owner_occurrence);
	const struct prototype_typed_occurrence* argument =
		prototype_typed_occurrence_graph_get(graph, argument_occurrence);
	if (!owner || !argument || owner->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		argument->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
		argument->binding_id != induction->binding_id ||
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
	const struct prototype_typed_occurrence_match_case* typed_case =
		&graph->cases[owner->first_case + induction->ih_case_index];
	const struct prototype_match_case* core_case =
		&terms->cases[
			core_match->as.match.first_case + induction->ih_case_index
		];
	if (induction->ih_field_index >= typed_case->binder_count ||
		induction->ih_field_index >= core_case->binder_count ||
		typed_case->binder_ids[induction->ih_field_index] !=
			induction->binding_id ||
		core_case->first_binder > terms->case_binder_count ||
		core_case->binder_count >
			terms->case_binder_count - core_case->first_binder) {
		return -1;
	}
	const struct prototype_case_binder* core_binder =
		&terms->case_binders[core_case->first_binder + induction->ih_field_index];
	/* Core Match binders are alpha-interned. Persistent occurrence Binding IDs
	 * provide the correspondence without making source AST numbering semantic. */
	if (!core_binder->is_recursive) {
		return -1;
	}
	*p_edge = (struct prototype_typed_occurrence_induction_edge){
		.induction_occurrence = operation_id,
		.owner_match_occurrence = induction->ih_owner_occurrence,
		.scope_id = induction->ih_scope_id,
		.case_index = induction->ih_case_index,
		.field_index = induction->ih_field_index,
		.binding_id = induction->binding_id,
		.argument_occurrence = argument_occurrence
	};
	return 0;
}

int prototype_typed_occurrence_graph_validate(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
) {
	if (!graph || !terms || !contexts ||
		(graph->occurrence_count != 0 && !graph->occurrences) ||
		(graph->case_count != 0 && !graph->cases) ||
		(graph->fold_clause_count != 0 && !graph->fold_clauses)) {
		return -1;
	}
	for (uint32_t i = 0; i < graph->occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			prototype_typed_occurrence_graph_get(graph, i);
		if (!operation ||
			operation->tag < PROTOTYPE_TYPED_OCCURRENCE_ATOM ||
			operation->tag > PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
			!prototype_context_get(contexts, operation->context_id)) {
			fprintf(stderr,
				"typed occurrence header invalid occurrence=%u tag=%d context=%u\n",
				i, operation ? operation->tag : 0,
				operation ? operation->context_id : PROTOTYPE_INVALID_ID);
			return -1;
		}
		if (operation->application_role < PROTOTYPE_TERM_APPLICATION_NONE ||
			operation->application_role >
				PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION ||
			(operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP &&
			 operation->application_role != PROTOTYPE_TERM_APPLICATION_NONE) ||
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
			 operation->application_role == PROTOTYPE_TERM_APPLICATION_NONE)) {
			fprintf(stderr,
				"typed occurrence application role invalid occurrence=%u tag=%d role=%d\n",
				i, operation->tag, operation->application_role);
			return -1;
		}
		uint32_t term_references[] = {
			operation->core_term,
			operation->classifier,
			operation->binder_classifier,
			operation->source_core_term,
			operation->source_classifier
		};
		for (size_t j = 0; j < sizeof(term_references) / sizeof(term_references[0]); ++j) {
			if (term_references[j] != PROTOTYPE_INVALID_ID &&
				(term_references[j] >= terms->term_count ||
				 terms->terms[term_references[j]].tag == 0)) {
				fprintf(stderr,
					"typed occurrence term reference invalid occurrence=%u slot=%zu term=%u\n",
					i, j, term_references[j]);
				return -1;
			}
		}
		uint32_t structural_child_count;
		if (prototype_typed_occurrence_core_child_count(
				graph, terms, i, &structural_child_count
			) != 0) {
			fprintf(stderr, "typed occurrence core shape invalid occurrence=%u tag=%d core=%u\n",
				i, operation->tag, operation->core_term);
			return -1;
		}
		for (uint32_t child_index = 0;
			child_index < structural_child_count;
			++child_index) {
			uint32_t child_occurrence;
			if (prototype_typed_occurrence_core_child(
					graph, terms, i, child_index, &child_occurrence
				) != 0) {
				struct prototype_term_child core_child;
				const struct prototype_typed_occurrence_edge* edge =
					operation->first_edge != PROTOTYPE_INVALID_ID &&
					operation->first_edge + child_index < graph->edge_count ?
						&graph->edges[operation->first_edge + child_index] : NULL;
				int has_core_child = prototype_term_child_at(
					terms, operation->core_term, child_index, &core_child
				) == 0;
				fprintf(stderr,
					"typed occurrence child mismatch occurrence=%u tag=%d core=%u "
					"child=%u core_role=%d core_ordinal=%u core_child=%u "
					"edge_role=%d edge_ordinal=%u edge_child=%u edge_core=%u "
					"edge_source_core=%u edge_action=%u parent_source_core=%u "
					"parent_action=%u core_ih_scope=%u occurrence_ih_scope=%u\n",
					i, operation->tag, operation->core_term, child_index,
					has_core_child ? core_child.role : PROTOTYPE_TERM_CHILD_INVALID,
					has_core_child ? core_child.ordinal : PROTOTYPE_INVALID_ID,
					has_core_child ? core_child.term : PROTOTYPE_INVALID_ID,
					edge ? edge->role : PROTOTYPE_TERM_CHILD_INVALID,
					edge ? edge->ordinal : PROTOTYPE_INVALID_ID,
					edge ? edge->child_occurrence : PROTOTYPE_INVALID_ID,
					edge && edge->child_occurrence < graph->occurrence_count ?
						graph->occurrences[edge->child_occurrence].core_term :
						PROTOTYPE_INVALID_ID,
					edge && edge->child_occurrence < graph->occurrence_count ?
						graph->occurrences[edge->child_occurrence].source_core_term :
						PROTOTYPE_INVALID_ID,
					edge && edge->child_occurrence < graph->occurrence_count ?
						graph->occurrences[edge->child_occurrence].
							context_action_substitution : PROTOTYPE_INVALID_ID,
					operation->source_core_term,
					operation->context_action_substitution,
					operation->core_term < terms->term_count &&
						terms->terms[operation->core_term].tag == PROTOTYPE_TERM_MATCH ?
						terms->terms[operation->core_term].as.match.ih_scope_id :
						PROTOTYPE_INVALID_ID,
					operation->ih_scope_id);
				return -1;
			}
		}
		if ((operation->context_action_substitution == PROTOTYPE_INVALID_ID &&
			 (operation->source_core_term != PROTOTYPE_INVALID_ID ||
			  operation->source_classifier != PROTOTYPE_INVALID_ID)) ||
			(operation->context_action_substitution != PROTOTYPE_INVALID_ID &&
			 operation->source_core_term == PROTOTYPE_INVALID_ID)) {
			fprintf(stderr,
				"typed occurrence source action tuple invalid occurrence=%u tag=%d context=%u action=%u source-core=%u source-classifier=%u classifier=%u core=%u\n",
				i, operation->tag, operation->context_id,
				operation->context_action_substitution,
				operation->source_core_term, operation->source_classifier,
				operation->classifier, operation->core_term);
			return -1;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP) {
			struct prototype_term_semantics semantics;
			uint32_t structural_core;
			if (prototype_term_core_projection(
					terms, operation->core_term, &structural_core
				) != 0 || prototype_term_semantics(
					terms, structural_core, &semantics
				) != 0 ||
				(semantics.application_role != operation->application_role &&
				 !(semantics.application_role ==
						PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION &&
				   operation->application_role ==
						PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION)) ||
				term_is_saturated_effect_application(terms, structural_core) ||
				(operation->application_role ==
						PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION &&
				 operation->category != PROTOTYPE_TERM_CATEGORY_VALUE)) {
				fprintf(stderr,
					"typed occurrence APP semantics invalid occurrence=%u core=%u role=%d category=%d\n",
					i, operation->core_term, operation->application_role,
					operation->category);
				return -1;
			}
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			struct prototype_typed_occurrence_induction_edge edge;
			if (prototype_typed_occurrence_graph_induction_edge(
					graph, terms, i, &edge
				) != 0) {
				fprintf(stderr,
					"typed occurrence induction edge invalid occurrence=%u core=%u owner=%u scope=%u case=%u field=%u binding=%u\n",
					i, operation->core_term, operation->ih_owner_occurrence,
					operation->ih_scope_id, operation->ih_case_index,
					operation->ih_field_index, operation->binding_id);
				return -1;
			}
		}
		if ((operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
			 operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) &&
			operation->wrapped_occurrence >= graph->occurrence_count) {
			return -1;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
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
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD) {
			uint32_t input_occurrence;
			uint32_t return_occurrence;
			if (terms->terms[operation->core_term].tag !=
					PROTOTYPE_TERM_COMPUTATION_FOLD ||
				prototype_typed_occurrence_graph_child(
					graph, i, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					&input_occurrence
				) != 0 || prototype_typed_occurrence_graph_child(
					graph, i, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&return_occurrence
				) != 0) {
				return -1;
			}
			const struct prototype_term* fold =
				&terms->terms[operation->core_term];
			const struct prototype_typed_occurrence* input_operation =
				prototype_typed_occurrence_graph_get(graph, input_occurrence);
			const struct prototype_typed_occurrence* return_operation =
				prototype_typed_occurrence_graph_get(graph, return_occurrence);
			if (!input_operation || !return_operation ||
				input_operation->core_term != fold->as.computation_fold.computation ||
				return_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
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
				const struct prototype_typed_occurrence_fold_clause* occurrence =
					prototype_typed_occurrence_graph_get_fold_clause(
						graph, operation->first_fold_clause + clause_index
					);
				const struct prototype_computation_fold_clause* core_clause =
					&terms->computation_fold_clauses[
						fold->as.computation_fold.first_clause + clause_index
					];
				uint32_t operation_occurrence_id;
				uint32_t clause_occurrence_id;
				const struct prototype_typed_occurrence* operation_occurrence =
					prototype_typed_occurrence_graph_child(
						graph, i,
						PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION,
						clause_index, &operation_occurrence_id
					) == 0 ? prototype_typed_occurrence_graph_get(
						graph, operation_occurrence_id
					) : NULL;
				const struct prototype_typed_occurrence* clause_occurrence =
					prototype_typed_occurrence_graph_child(
						graph, i, PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY,
						clause_index, &clause_occurrence_id
					) == 0 ? prototype_typed_occurrence_graph_get(
						graph, clause_occurrence_id
					) : NULL;
				if (!occurrence || !operation_occurrence || !clause_occurrence ||
					operation_occurrence->core_term != core_clause->operation ||
					clause_occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
					clause_occurrence->core_term != core_clause->body) {
					return -1;
				}
			}
		}
	}
	for (uint32_t i = 0; i < graph->case_count; ++i) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			prototype_typed_occurrence_graph_get_case(graph, i);
		if (!operation_case ||
			operation_case->binder_count > 16 ||
			!prototype_context_get(contexts, operation_case->context_id) ||
			operation_case->constructor_owner >= terms->term_count ||
			terms->terms[operation_case->constructor_owner].tag == 0 ||
			operation_case->refinement_status <
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_PENDING ||
			operation_case->refinement_status >
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT ||
			(prototype_typed_occurrence_match_case_is_solved(operation_case) !=
				(operation_case->refinement_substitution != PROTOTYPE_INVALID_ID)) ||
			(graph->frozen && operation_case->refinement_status !=
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED &&
				operation_case->refinement_status !=
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE &&
				operation_case->refinement_status !=
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT)) {
			fprintf(stderr,
				"typed occurrence case invalid case=%u context=%u owner=%u binders=%u refined=%u action=%u\n",
				i,
				operation_case ? operation_case->context_id : PROTOTYPE_INVALID_ID,
				operation_case ? operation_case->constructor_owner : PROTOTYPE_INVALID_ID,
				operation_case ? operation_case->binder_count : PROTOTYPE_INVALID_ID,
				operation_case ? operation_case->refinement_status : -1,
				operation_case ? operation_case->refinement_substitution :
					PROTOTYPE_INVALID_ID);
			return -1;
		}
		for (uint32_t binder_index = 0;
			binder_index < operation_case->binder_count; ++binder_index) {
			if (!prototype_context_contains_binding(
					contexts, operation_case->context_id,
					operation_case->binder_ids[binder_index]
				)) {
				fprintf(stderr,
					"typed occurrence case binder missing case=%u binder-index=%u binding=%u context=%u\n",
					i, binder_index,
					operation_case->binder_ids[binder_index],
					operation_case->context_id);
				return -1;
			}
		}
	}
	for (uint32_t i = 0; i < graph->fold_clause_count; ++i) {
		const struct prototype_typed_occurrence_fold_clause* clause =
			prototype_typed_occurrence_graph_get_fold_clause(graph, i);
		if (!clause || !prototype_context_get(contexts, clause->context_id)) {
			return -1;
		}
	}
	return 0;
}

int prototype_typed_occurrence_graph_seal(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
) {
	if (!graph || graph->frozen || graph->sealed ||
		prototype_typed_occurrence_graph_validate(graph, terms, contexts) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < graph->occurrence_count; ++i) {
		struct prototype_typed_occurrence* occurrence = &graph->occurrences[i];
		if (occurrence->classifier_status ==
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_PENDING &&
			occurrence->classifier != PROTOTYPE_INVALID_ID) {
			occurrence->classifier_status =
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED;
		}
		if ((occurrence->classifier_status ==
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED &&
			 occurrence->classifier == PROTOTYPE_INVALID_ID) ||
			(occurrence->classifier_status ==
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_RESIDUAL_VERIFICATION &&
			 (occurrence->classifier != PROTOTYPE_INVALID_ID ||
			  occurrence->classifier_verification_obligation ==
				PROTOTYPE_INVALID_ID)) ||
			occurrence->classifier_status ==
				PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_PENDING) {
			fprintf(stderr,
				"typed occurrence classifier cannot seal occurrence=%u tag=%d "
				"core=%u classifier=%u status=%d obligation=%u\n",
				i, occurrence->tag, occurrence->core_term,
				occurrence->classifier, occurrence->classifier_status,
				occurrence->classifier_verification_obligation);
			return -1;
		}
	}
	graph->sealed = 1;
	return 0;
}

int prototype_typed_occurrence_graph_freeze(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
) {
	if (!graph || graph->frozen ||
		(!graph->sealed && prototype_typed_occurrence_graph_seal(
			graph, terms, contexts
		) != 0)) {
		return -1;
	}
	graph->frozen = 1;
	graph->transaction_active = 0;
	graph->transaction_base_frozen = 0;
	graph->transaction_occurrence_start = graph->occurrence_count;
	graph->transaction_edge_start = graph->edge_count;
	graph->transaction_case_start = graph->case_count;
	graph->transaction_fold_clause_start = graph->fold_clause_count;
	return 0;
}

int prototype_typed_occurrence_graph_begin_transaction(
	struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts
) {
	if (!graph || graph->transaction_active || graph->sealed != graph->frozen) {
		return -1;
	}
	if (graph->frozen &&
		prototype_typed_occurrence_graph_validate(graph, terms, contexts) != 0) {
		return -1;
	}
	graph->transaction_active = 1;
	graph->transaction_base_frozen = graph->frozen;
	graph->frozen = 0;
	graph->sealed = 0;
	graph->transaction_occurrence_start = graph->occurrence_count;
	graph->transaction_edge_start = graph->edge_count;
	graph->transaction_case_start = graph->case_count;
	graph->transaction_fold_clause_start = graph->fold_clause_count;
	return 0;
}

int prototype_typed_occurrence_graph_rollback_transaction(
	struct prototype_typed_occurrence_graph* graph
) {
	if (!graph || !graph->transaction_active ||
		graph->transaction_occurrence_start > graph->occurrence_count ||
		graph->transaction_edge_start > graph->edge_count ||
		graph->transaction_case_start > graph->case_count ||
		graph->transaction_fold_clause_start > graph->fold_clause_count) {
		return -1;
	}
	graph->occurrence_count = graph->transaction_occurrence_start;
	graph->edge_count = graph->transaction_edge_start;
	graph->case_count = graph->transaction_case_start;
	graph->fold_clause_count = graph->transaction_fold_clause_start;
	graph->frozen = graph->transaction_base_frozen;
	graph->sealed = graph->transaction_base_frozen;
	graph->transaction_active = 0;
	graph->transaction_base_frozen = 0;
	return 0;
}

size_t prototype_typed_occurrence_graph_transaction_start(
	const struct prototype_typed_occurrence_graph* graph
) {
	if (!graph) {
		return 0;
	}
	return graph->transaction_active ? graph->transaction_occurrence_start : 0;
}

struct prototype_typed_occurrence_graph* prototype_compile_metadata_typed_occurrences(
	struct prototype_compile_metadata* metadata
) {
	return metadata ? &metadata->typed_occurrences : NULL;
}

const struct prototype_typed_occurrence_graph*
prototype_compile_metadata_typed_occurrences_const(
	const struct prototype_compile_metadata* metadata
) {
	return metadata ? &metadata->typed_occurrences : NULL;
}
