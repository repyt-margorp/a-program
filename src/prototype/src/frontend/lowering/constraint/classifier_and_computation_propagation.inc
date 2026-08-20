static int operation_solver_solve_match(
	struct compile_context* ctx,
	uint32_t operation,
	int* p_changed
);

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
);

static int operation_solver_materialize_induction_hypothesis(
	struct compile_context* ctx,
	uint32_t operation,
	int* p_changed
);

static int operation_solver_match_has_recursive_binder(
	const struct compile_context* ctx,
	const struct prototype_typed_occurrence* operation
);

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static int operation_solver_classifier_goal_is_satisfied(
	const struct compile_context* ctx,
	const struct operation_constraint* goal,
	int* p_reason
) {
	if (!ctx || !ctx->metadata || !goal || !p_reason ||
		goal->payload.classifier.classifier_variable >= ctx->metadata->typed_occurrences.occurrence_count ||
		goal->id >= ctx->constraints.classifier_constraint_count) {
		return 0;
	}
	const struct operation_constraint* result =
		&ctx->constraints.constraints[goal->id];
	uint32_t target = operation_solver_classifier(
		ctx, goal->payload.classifier.classifier_variable
	);
	if (target == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	switch (goal->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE:
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_EQUAL: {
			uint32_t referenced = operation_solver_classifier(
				ctx, goal->payload.classifier.goal.reference.referenced_operation
			);
			if (referenced == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, target, referenced
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_REFERENCE_VALIDATED;
			return 1;
		}
		case OPERATION_CONSTRAINT_CONVERTIBLE:
			if (result->conversion_goal.result.status !=
				PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			if (operation_solver_classifier(
					ctx, goal->payload.classifier.goal.pi.body_or_function_operation
				) == PROTOTYPE_INVALID_ID ||
				(goal->payload.classifier.goal.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
				 operation_solver_classifier(
					ctx,
					goal->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence
				 ) == PROTOTYPE_INVALID_ID)) {
				return 0;
			}
			*p_reason = goal->payload.classifier.goal.pi.role ==
				OPERATION_CLASSIFIER_PI_GOAL_INTRO ?
				OPERATION_CLASSIFIER_GOAL_REASON_PI_INTRO_VALIDATED :
				OPERATION_CLASSIFIER_GOAL_REASON_PI_ELIM_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			if (operation_solver_classifier(
					ctx,
					goal->payload.classifier.goal.constructor_formation.function_operation
				) == PROTOTYPE_INVALID_ID || operation_solver_classifier(
					ctx,
					goal->payload.classifier.goal.constructor_formation.argument_occurrence
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_CONSTRUCTOR_FORMATION_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_BINDER_TYPE: {
			const struct prototype_context* binding = prototype_context_get(
				&ctx->metadata->contexts,
				goal->payload.classifier.goal.binder_type.binding_context
			);
			if (!binding) {
				return 0;
			}
			uint32_t binder_classifier =
				prototype_context_classifier_term(binding);
			if (binder_classifier == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, target, binder_classifier
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_BINDER_TYPE_VALIDATED;
			return 1;
		}
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
			/* A Match may already carry a validated expected classifier, in which
			 * case no synthesized motive term is materialized. The semantic case
			 * goal is discharged once both that Match classifier and this branch
			 * classifier exist; JudgementDelta later validates the exact rule. */
			if (operation_solver_classifier(
					ctx, goal->payload.classifier.goal.motive_case.branch_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_MOTIVE_CASE_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_IH_EXPECTED:
		{
			const struct prototype_typed_occurrence_graph* graph;
			struct prototype_typed_occurrence_induction_edge edge;
			graph = prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
			int edge_status = prototype_typed_occurrence_graph_induction_edge(
					graph, ctx->terms, goal->payload.classifier.classifier_variable, &edge
				);
			uint32_t recursive_classifier = operation_solver_classifier(
				ctx,
				goal->payload.classifier.goal.induction_hypothesis.recursive_argument_occurrence
			);
			if (edge_status != 0 ||
				goal->payload.classifier.goal.induction_hypothesis.owner_match_occurrence !=
					edge.owner_match_occurrence ||
				goal->payload.classifier.goal.induction_hypothesis.ih_scope_id != edge.scope_id ||
				goal->payload.classifier.goal.induction_hypothesis.case_index != edge.case_index ||
				goal->payload.classifier.goal.induction_hypothesis.field_index != edge.field_index ||
				recursive_classifier == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_IH_VALIDATED;
			return 1;
		}
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
			if (operation_solver_classifier(
					ctx, goal->payload.classifier.goal.cbpv_boundary.child_operation
				) == PROTOTYPE_INVALID_ID) {
				return 0;
			}
			*p_reason = OPERATION_CLASSIFIER_GOAL_REASON_CBPV_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_COMPUTATION_FOLD_VALIDATED;
			return 1;
		case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
			*p_reason =
				OPERATION_CLASSIFIER_GOAL_REASON_OPERATION_REQUEST_VALIDATED;
			return 1;
		default:
			return 0;
	}
}

static void operation_solver_refresh_constraint_states(
	struct compile_context* ctx,
	int incomplete
) {
	if (!ctx || !ctx->metadata) {
		return;
	}
	ctx->metadata->solver_constraint_count = ctx->constraints.classifier_constraint_count;
	ctx->metadata->solver_solved_count = 0;
	ctx->metadata->solver_residual_count = 0;
	ctx->metadata->solver_incomplete_count = 0;
	for (uint32_t i = 0; i < ctx->constraints.classifier_constraint_count; ++i) {
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[i];
		struct operation_constraint* result =
			&ctx->constraints.constraints[i];
		if (result->state == OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			continue;
		}
		int residual = constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE &&
			(result->conversion_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_RESIDUAL ||
			 result->conversion_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT);
		int goal_incomplete = constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE &&
			result->conversion_goal.result.status ==
				PROTOTYPE_TERM_CONVERSION_EXHAUSTED;
		for (size_t obligation_id = 0;
			obligation_id <
				prototype_verification_db_count(&ctx->metadata->verification);
			++obligation_id) {
			const struct prototype_verification_obligation* obligation =
				prototype_verification_db_get(
					&ctx->metadata->verification, (uint32_t)obligation_id
				);
			if (!obligation) {
				continue;
			}
			if (obligation->occurrence == constraint->payload.classifier.classifier_variable) {
				residual = 1;
				break;
			}
		}
		int solved_reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE;
		int solved = operation_solver_classifier_goal_is_satisfied(
			ctx, constraint, &solved_reason
		);
		int state = residual ? OPERATION_CONSTRAINT_STATE_RESIDUAL :
			(solved ? OPERATION_CONSTRAINT_STATE_SOLVED :
			(incomplete || goal_incomplete ? OPERATION_CONSTRAINT_STATE_INCOMPLETE :
				OPERATION_CONSTRAINT_STATE_PENDING));
		int reason = residual ?
			(constraint->kind == OPERATION_CONSTRAINT_CONVERTIBLE ?
				OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_RESIDUAL :
				OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION) :
			(solved ? solved_reason :
			(incomplete || goal_incomplete ?
				OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET :
				OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY));
		if (operation_constraint_reopen(
				result, OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
			) != 0 || operation_constraint_transition(result, state, reason) != 0) {
			return;
		}
		if (result->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (result->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (result->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
	ctx->metadata->solver_constraint_count +=
		ctx->constraints.branch_refinement_constraint_count;
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		if (id >= ctx->constraints.constraint_count) {
			continue;
		}
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (incomplete &&
			constraint->state == OPERATION_CONSTRAINT_STATE_PENDING) {
			if (operation_constraint_transition(
					constraint,
					OPERATION_CONSTRAINT_STATE_INCOMPLETE,
					OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET
				) != 0) {
				return;
			}
		}
		if (constraint->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
	ctx->metadata->solver_constraint_count +=
		ctx->constraints.computation_constraint_count;
	for (uint32_t i = 0;
		i < ctx->constraints.computation_constraint_count;
		++i) {
		uint32_t id = ctx->constraints.first_computation_constraint + i;
		if (id >= ctx->constraints.constraint_count) {
			continue;
		}
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (incomplete && constraint->state == OPERATION_CONSTRAINT_STATE_PENDING) {
			if (operation_constraint_transition(
					constraint,
					OPERATION_CONSTRAINT_STATE_INCOMPLETE,
					OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET
				) != 0) {
				return;
			}
		}
		if (constraint->state == OPERATION_CONSTRAINT_STATE_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (constraint->state == OPERATION_CONSTRAINT_STATE_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
	ctx->metadata->solver_constraint_count +=
		ctx->constraints.effect_constraint_count;
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		if (id >= ctx->constraints.constraint_count) {
			continue;
		}
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (incomplete &&
			constraint->state == PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_PENDING) {
			if (operation_constraint_transition(
					constraint,
					OPERATION_CONSTRAINT_STATE_INCOMPLETE,
					OPERATION_CLASSIFIER_GOAL_REASON_INCOMPLETE_BUDGET
				) != 0) {
				return;
			}
		}
		if (constraint->state ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_SOLVED) {
			ctx->metadata->solver_solved_count++;
		} else if (constraint->state ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNSOLVED_RESIDUAL) {
			ctx->metadata->solver_residual_count++;
		} else if (constraint->state ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_INCOMPLETE) {
			ctx->metadata->solver_incomplete_count++;
		}
	}
}

static int operation_solver_propagate_zero_clause_computation_fold_input(
	struct compile_context* ctx,
	uint32_t sequence_fold_operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		sequence_fold_operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* sequence_fold_operation =
		&ctx->metadata->typed_occurrences.occurrences[sequence_fold_operation_id];
	uint32_t computation_occurrence;
	uint32_t continuation_occurrence_id;
	if (sequence_fold_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
		compile_occurrence_child(
			ctx, sequence_fold_operation_id,
			PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
			&computation_occurrence
		) != 0 || compile_occurrence_child(
			ctx, sequence_fold_operation_id,
			PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
			&continuation_occurrence_id
		) != 0 ||
		sequence_fold_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[sequence_fold_operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		ctx->terms->terms[sequence_fold_operation->core_term].as.computation_fold.clause_count != 0) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier(
		ctx, computation_occurrence
	);
	if (input_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	struct prototype_term_classifier_view input_view;
	uint32_t input_value_classifier;
	int input_is_value =
		ctx->terms->terms[input_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE;
	if (input_is_value) {
		/* A recursive function field resolves to a suspended value after graph
		 * construction. The provisional fold is its implicit RETURN boundary. */
		input_value_classifier = input_classifier;
	} else {
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				input_classifier,
				&input_view
			) != 0) {
			return -1;
		}
		/* Other value-level synthesis frontiers remain pending until the motive
		 * determines whether they are returning computations. */
		if (input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return 0;
		}
		if (input_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		input_value_classifier = input_view.result;
	}
	const struct prototype_term* sequence_fold_term =
		&ctx->terms->terms[sequence_fold_operation->core_term];
	if (sequence_fold_term->as.computation_fold.return_clause >= ctx->terms->term_count ||
		ctx->terms->terms[sequence_fold_term->as.computation_fold.return_clause].tag !=
			PROTOTYPE_TERM_LAMBDA ||
		ctx->metadata->typed_occurrences.occurrences[continuation_occurrence_id].tag !=
			PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
		return -1;
	}
	struct prototype_typed_occurrence* continuation_occurrence =
		&ctx->metadata->typed_occurrences.occurrences[continuation_occurrence_id];
	if (continuation_occurrence->binder_classifier == PROTOTYPE_INVALID_ID ||
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			continuation_occurrence->binder_classifier,
			input_value_classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		continuation_occurrence->binder_classifier = input_value_classifier;
		operation_context_binding_dependency_changed(
			ctx, continuation_occurrence->binding_id
		);
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(
			ctx, continuation_occurrence_id
		) != 0) {
			return -1;
		}
	}
	uint32_t binder_var;
	if (prototype_term_var(
			ctx->terms,
			ctx->terms->terms[sequence_fold_term->as.computation_fold.return_clause].as.lambda.binding_id,
			&binder_var
		) != 0) {
		return -1;
	}
	uint8_t visited[ctx->metadata->typed_occurrences.occurrence_count];
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		memset(visited, 0, sizeof(visited));
		int binder_matches =
			continuation_occurrence->referenced_ast_binder_id != PROTOTYPE_INVALID_ID ?
				operation->referenced_ast_binder_id ==
					continuation_occurrence->referenced_ast_binder_id :
				operation->core_term == binder_var;
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR || !binder_matches ||
			operation_subtree_contains_operation(
				ctx,
				continuation_occurrence_id,
				operation_id,
				visited
			) <= 0) {
			continue;
		}
		/* Sequencing projects the authoritative computation result into its
		 * continuation binder.  A stale binder is not an equation for rewriting
		 * the input computation; independent expected-type constraints perform
		 * that reverse refinement at their own source. */
		if (operation_solver_bind_proven_classifier(
			ctx, operation_id, input_value_classifier, p_changed
		) != 0) {
			return -1;
		}
	}
	if (*p_changed) {
		/* This constraint may itself have refined the continuation binder or one
		 * of its occurrences. Those mutations are premises of the same fold
		 * equation, so requeue the fold after downstream Lambda/APP constraints.
		 * Without this edge the worklist can stop at a stale dependent codomain. */
		return operation_solver_enqueue_dependents(
			ctx, sequence_fold_operation_id
		);
	}
	uint32_t continuation_classifier = operation_solver_classifier(
		ctx, continuation_occurrence_id
	);
	if (continuation_classifier != PROTOTYPE_INVALID_ID) {
		uint32_t continuation_domain;
		uint32_t continuation_family;
		if (prototype_judgement_pi_parts(
				ctx->terms,
				continuation_classifier,
				&continuation_domain,
				&continuation_family
			) == 0 && prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				continuation_domain,
				input_value_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			/* Refining the sequenced result changes the continuation binder before
			 * its Pi-formation constraint rebuilds the Lambda classifier. Defer this
			 * fold equation to that authoritative constraint instead of rejecting the
			 * stale Pi snapshot as a source typing error. */
			(void)continuation_family;
			return operation_solver_enqueue_dependents(
				ctx, continuation_occurrence_id
			);
		}
		uint32_t sequence_fold_classifier;
		int status = prototype_judgement_computation_fold_result_classifier(
			ctx->terms,
			ctx->type_declarations,
			sequence_fold_term->as.computation_fold.computation,
			input_classifier,
			continuation_classifier,
			&sequence_fold_classifier
		);
		if (status < 0 || (status == 0 && operation_solver_bind_proven_classifier(
				ctx, sequence_fold_operation_id, sequence_fold_classifier, p_changed
			) != 0)) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_propagate_clause_computation_fold_input(
	struct compile_context* ctx,
	uint32_t handle_operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		handle_operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* handle_operation =
		&ctx->metadata->typed_occurrences.occurrences[handle_operation_id];
	uint32_t computation_occurrence;
	uint32_t return_operation;
	uint32_t return_body_occurrence;
	if (handle_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
		compile_occurrence_child(
			ctx, handle_operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION,
			0, &computation_occurrence
		) != 0 || compile_occurrence_child(
			ctx, handle_operation_id,
			PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0, &return_operation
		) != 0 || compile_occurrence_child(
			ctx, return_operation, PROTOTYPE_TERM_CHILD_BODY, 0,
			&return_body_occurrence
		) != 0) {
		return -1;
	}
	uint32_t input_classifier = operation_solver_classifier(
		ctx, computation_occurrence
	);
	if (input_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	struct prototype_term_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			input_classifier,
			&input_view
		) != 0) {
		return -1;
	}
	/* Keep provisional motive applications pending until the IH/motive
	 * equations close them as computations. */
	if (input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return 0;
	}
	if (
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint8_t visited[ctx->metadata->typed_occurrences.occurrence_count];
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
			operation->referenced_ast_binder_id ==
				handle_operation->fold_return_ast_binder_id &&
			operation->binder_classifier == PROTOTYPE_INVALID_ID) {
			operation->binder_classifier = input_view.result;
			operation_context_binding_dependency_changed(ctx, operation->binding_id);
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, operation_id) != 0) {
				return -1;
			}
		}
		memset(visited, 0, sizeof(visited));
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			operation->referenced_ast_binder_id ==
				handle_operation->fold_return_ast_binder_id &&
			operation_subtree_contains_operation(
				ctx,
				return_body_occurrence,
				operation_id,
				visited
			) > 0 && operation_solver_bind(
				ctx, operation_id, input_view.result, p_changed
			) != 0) {
			return -1;
		}
	}
	if (handle_operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[handle_operation->core_term].tag !=
			PROTOTYPE_TERM_COMPUTATION_FOLD) {
		return -1;
	}
	const struct prototype_term* fold_term =
		&ctx->terms->terms[handle_operation->core_term];
	if ((size_t)fold_term->as.computation_fold.first_clause +
			fold_term->as.computation_fold.clause_count >
			ctx->terms->computation_fold_clause_count ||
		handle_operation->fold_clause_count !=
			fold_term->as.computation_fold.clause_count ||
		(size_t)handle_operation->first_fold_clause +
			handle_operation->fold_clause_count >
			ctx->metadata->typed_occurrences.fold_clause_count) {
		return -1;
	}
	uint32_t return_body_classifier = operation_solver_classifier(
		ctx, return_body_occurrence
	);
	struct prototype_term_classifier_view return_body_view;
	if (return_body_classifier == PROTOTYPE_INVALID_ID ||
		prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			return_body_classifier,
			&return_body_view
		) != 0 || return_body_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		return_body_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	uint32_t fold_effect_row = return_body_view.effect_row;
	int clause_bodies_complete = 1;
	uint32_t handled_row;
	if (prototype_term_effect_row_empty(ctx->terms, &handled_row
		) != 0) {
		return -1;
	}
	for (uint32_t clause_index = 0;
		clause_index < fold_term->as.computation_fold.clause_count;
		++clause_index) {
		const struct prototype_computation_fold_clause* core_clause =
			&ctx->terms->computation_fold_clauses[
				fold_term->as.computation_fold.first_clause + clause_index
			];
		const struct prototype_typed_occurrence_fold_clause* occurrence_clause =
			&ctx->metadata->typed_occurrences.fold_clauses[
				handle_operation->first_fold_clause + clause_index
			];
		uint32_t clause_operation_occurrence;
		uint32_t clause_lambda_occurrence;
		uint32_t clause_body_occurrence;
		if (compile_fold_clause_occurrences(
				ctx, handle_operation_id, clause_index,
				&clause_operation_occurrence, &clause_lambda_occurrence,
				&clause_body_occurrence
			) != 0) {
			return -1;
		}
		(void)occurrence_clause;
		(void)clause_operation_occurrence;
		(void)clause_lambda_occurrence;
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				ctx->terms, core_clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		uint32_t empty_row;
		uint32_t operation_atom;
		uint32_t next_handled_row;
		if (prototype_term_effect_row_empty(ctx->terms, &empty_row
			) != 0 || prototype_term_effect_row_operation(
				ctx->terms, operation_identity, empty_row, &operation_atom
			) != 0 || prototype_term_effect_row_union(
				ctx->terms, handled_row, operation_atom, &next_handled_row
			) != 0) {
			return -1;
		}
		handled_row = next_handled_row;
		uint32_t clause_body_classifier = operation_solver_classifier(
			ctx, clause_body_occurrence
		);
		if (clause_body_classifier == PROTOTYPE_INVALID_ID) {
			clause_bodies_complete = 0;
			continue;
		}
		struct prototype_term_classifier_view clause_body_view;
		uint32_t joined_row;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				clause_body_classifier,
				&clause_body_view
			) != 0 || clause_body_view.category !=
				PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			clause_body_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_union(
				ctx->terms,
				fold_effect_row,
				clause_body_view.effect_row,
				&joined_row
			) != 0) {
			return -1;
		}
		fold_effect_row = joined_row;
	}
	uint32_t residual_row;
	uint32_t joined_row;
	int residual_status = prototype_term_effect_row_residual(
		ctx->terms, input_view.effect_row, handled_row, &residual_row
	);
	if (residual_status < 0 || (residual_status == 0 &&
		prototype_term_effect_row_union(
			ctx->terms, fold_effect_row, residual_row, &joined_row
		) != 0)) {
		return -1;
	}
	if (residual_status == 0) {
		fold_effect_row = joined_row;
	}
	uint32_t fold_output_classifier;
	if (prototype_term_computation_type(
			ctx->terms,
			fold_effect_row,
			return_body_view.result,
			&fold_output_classifier
		) != 0) {
		return -1;
	}
	/* A symbolic clause row is precisely the case in which the operation
	 * domain must still be propagated into the clause binders below.  Defer
	 * publishing the fold result, but do not return before that propagation. */
	if (clause_bodies_complete &&
		prototype_term_effect_row_is_closed(ctx->terms, fold_effect_row) == 1) {
		if (operation_solver_widen_computation_binding(
				ctx, handle_operation_id, fold_output_classifier, p_changed
			) != 0) {
			return -1;
		}
	}
	for (uint32_t clause_index = 0;
		clause_index < fold_term->as.computation_fold.clause_count;
		++clause_index) {
		const struct prototype_computation_fold_clause* core_clause =
			&ctx->terms->computation_fold_clauses[
				fold_term->as.computation_fold.first_clause + clause_index
			];
		const struct prototype_typed_occurrence_fold_clause* occurrence_clause =
			&ctx->metadata->typed_occurrences.fold_clauses[
				handle_operation->first_fold_clause + clause_index
			];
		uint32_t clause_operation_occurrence;
		uint32_t clause_lambda_occurrence;
		uint32_t clause_body_occurrence;
		if (compile_fold_clause_occurrences(
				ctx, handle_operation_id, clause_index,
				&clause_operation_occurrence, &clause_lambda_occurrence,
				&clause_body_occurrence
			) != 0) {
			return -1;
		}
		(void)clause_body_occurrence;
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				ctx->terms, core_clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		if (declaration->resumption_multiplicity !=
			PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT) {
			const struct prototype_typed_occurrence* outer_clause =
				&ctx->metadata->typed_occurrences.occurrences[clause_lambda_occurrence];
			uint32_t continuation_clause_occurrence;
			if (outer_clause->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
				compile_occurrence_child(
					ctx, clause_lambda_occurrence, PROTOTYPE_TERM_CHILD_BODY, 0,
					&continuation_clause_occurrence
				) != 0) {
				return -1;
			}
			const struct prototype_typed_occurrence* continuation_clause =
				&ctx->metadata->typed_occurrences.occurrences[continuation_clause_occurrence];
			const struct operation_solver_solution* continuation_solution =
				&ctx->classifier_solver.solutions[continuation_clause_occurrence];
			if (continuation_clause->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
				continuation_clause->binding_id !=
					occurrence_clause->continuation_binder_id ||
				continuation_solution->usage_state !=
					OPERATION_CONSTRAINT_STATE_SOLVED ||
				(declaration->resumption_multiplicity ==
						PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE &&
					continuation_solution->binder_usage !=
						PROTOTYPE_USAGE_ZERO) ||
				(declaration->resumption_multiplicity ==
						PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT &&
					continuation_solution->binder_usage ==
						PROTOTYPE_USAGE_MANY)) {
				return -1;
			}
		}
		uint32_t outer_lambda = core_clause->body;
		if (outer_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[outer_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t inner_lambda = ctx->terms->terms[outer_lambda].as.lambda.body;
		if (inner_lambda >= ctx->terms->term_count ||
			ctx->terms->terms[inner_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t operation_classifier = operation_solver_classifier(
			ctx, clause_operation_occurrence
		);
		uint32_t operation_classifier_whnf;
		uint32_t operation_domain;
		uint32_t operation_codomain_family;
		if (operation_classifier == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		int specialization_status =
			prototype_judgement_specialize_fold_operation_classifier(
				ctx->terms,
				ctx->type_declarations,
				input_view.effect_row,
				operation_identity,
				operation_classifier,
				&operation_classifier
			);
		if (specialization_status < 0) {
			return -1;
		}
		/* A symbolic input row cannot yet specialize a higher-order operation's
		 * delayed-computation domain. Its generic Pi still determines the request
		 * result and therefore the resumption binder. Propagate that classifier
		 * now; a later fold pass may replace the row-polymorphic domain with the
		 * specialized one. */
		if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			operation_classifier,
			&operation_classifier_whnf
			) != 0) {
			return -1;
		}
		for (uint32_t depth = 0;
			depth < 32 && operation_classifier_whnf < ctx->terms->term_count &&
			ctx->terms->terms[operation_classifier_whnf].tag ==
				PROTOTYPE_TERM_EFFECT_ROW_FORALL;
			++depth) {
			operation_classifier_whnf = ctx->terms->terms[
				operation_classifier_whnf
			].as.effect_row_forall.body;
			if (prototype_term_normalize_complete_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				operation_classifier_whnf,
				&operation_classifier_whnf
				) != 0) {
				return -1;
			}
		}
		if (prototype_judgement_pi_parts(
				ctx->terms,
				operation_classifier_whnf,
				&operation_domain,
				&operation_codomain_family
			) != 0) {
			return -1;
		}
		uint32_t operation_result_binder;
		uint32_t operation_result_computation;
		struct prototype_term_classifier_view operation_result_view;
		if (
			prototype_term_pure_family_parts(
				ctx->terms,
				operation_codomain_family,
				&operation_result_binder,
				&operation_result_computation
			) != 0 || prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				operation_result_computation,
				&operation_result_view
			) != 0 || operation_result_view.category !=
				PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			operation_result_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return 0;
		}
		(void)operation_result_binder;
		uint32_t continuation_function_classifier;
		uint32_t continuation_binder_classifier;
		if (prototype_term_pi(
				ctx->terms,
				operation_result_view.result,
				fold_output_classifier,
				&continuation_function_classifier
			) != 0 || prototype_term_thunk_type(
				ctx->terms,
				continuation_function_classifier,
				&continuation_binder_classifier
			) != 0) {
			return -1;
		}
		/* A clause argument has the operation domain even when the source body
	 * discards it. VAR occurrences are usage evidence, not typing authority. */
	for (uint32_t lambda_operation_id = 0;
		lambda_operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++lambda_operation_id) {
		struct prototype_typed_occurrence* lambda_operation =
			&ctx->metadata->typed_occurrences.occurrences[lambda_operation_id];
		uint32_t ast_binder_id;
		if (lambda_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
			lambda_operation->core_term != outer_lambda ||
			operation_solver_lambda_ast_binder(
				ctx, lambda_operation_id, &ast_binder_id
			) != 0 || ast_binder_id != occurrence_clause->argument_ast_binder_id) {
			continue;
		}
		if (lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID ||
			!(prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				lambda_operation->binder_classifier,
				operation_domain
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			lambda_operation->binder_classifier = operation_domain;
			operation_context_binding_dependency_changed(
				ctx, lambda_operation->binding_id
			);
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
				return -1;
			}
		}
	}
	for (uint32_t lambda_operation_id = 0;
		lambda_operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++lambda_operation_id) {
		struct prototype_typed_occurrence* lambda_operation =
			&ctx->metadata->typed_occurrences.occurrences[lambda_operation_id];
		uint32_t ast_binder_id;
		if (lambda_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
			lambda_operation->core_term != inner_lambda ||
			operation_solver_lambda_ast_binder(
				ctx, lambda_operation_id, &ast_binder_id
			) != 0 || ast_binder_id !=
				occurrence_clause->continuation_ast_binder_id) {
			continue;
		}
			if (lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID) {
				lambda_operation->binder_classifier = continuation_binder_classifier;
				operation_context_binding_dependency_changed(
					ctx, lambda_operation->binding_id
				);
				*p_changed = 1;
				if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
					return -1;
				}
			} else if (operation_solver_continuation_effect_widening(
					ctx,
					lambda_operation->binder_classifier,
					continuation_binder_classifier
				)) {
				lambda_operation->binder_classifier = continuation_binder_classifier;
				operation_context_binding_dependency_changed(
					ctx, lambda_operation->binding_id
				);
				*p_changed = 1;
				if (operation_solver_enqueue_dependents(ctx, lambda_operation_id) != 0) {
					return -1;
				}
			} else if (!(prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				lambda_operation->binder_classifier,
				continuation_binder_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t lambda_term;
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR) {
			continue;
		}
		if (operation->referenced_ast_binder_id ==
			occurrence_clause->argument_ast_binder_id) {
			lambda_term = outer_lambda;
		} else if (operation->referenced_ast_binder_id ==
			occurrence_clause->continuation_ast_binder_id) {
			lambda_term = inner_lambda;
		} else {
			continue;
		}
		memset(visited, 0, sizeof(visited));
		if (operation_subtree_contains_operation(
				ctx,
				clause_body_occurrence,
				operation_id,
				visited
			) <= 0) {
			continue;
		}
		uint32_t classifier =
			operation->referenced_ast_binder_id ==
				occurrence_clause->argument_ast_binder_id ?
				operation_domain : PROTOTYPE_INVALID_ID;
		for (uint32_t lambda_operation_id = 0;
			classifier == PROTOTYPE_INVALID_ID &&
				lambda_operation_id < ctx->metadata->typed_occurrences.occurrence_count;
			++lambda_operation_id) {
			const struct prototype_typed_occurrence* lambda_operation =
				&ctx->metadata->typed_occurrences.occurrences[lambda_operation_id];
			uint32_t ast_binder_id;
			if (lambda_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
				lambda_operation->core_term != lambda_term ||
				operation_solver_lambda_ast_binder(
					ctx, lambda_operation_id, &ast_binder_id
				) != 0 ||
				ast_binder_id != operation->referenced_ast_binder_id) {
				continue;
			}
			if (lambda_operation->binder_classifier != PROTOTYPE_INVALID_ID) {
				classifier = lambda_operation->binder_classifier;
				break;
			}
			uint32_t lambda_body_occurrence;
			if (compile_occurrence_child(
					ctx, lambda_operation_id, PROTOTYPE_TERM_CHILD_BODY, 0,
					&lambda_body_occurrence
				) != 0) {
				continue;
			}
			const struct prototype_typed_occurrence* lambda_body =
				&ctx->metadata->typed_occurrences.occurrences[lambda_body_occurrence];
			const struct prototype_context* lambda_binder_context =
				prototype_context_get(
					&ctx->metadata->contexts, lambda_body->context_id
				);
			uint32_t binder_var;
			if (!lambda_binder_context ||
				lambda_binder_context->parent != lambda_operation->context_id ||
				prototype_term_var(
					ctx->terms, lambda_binder_context->binding_id, &binder_var
				) != 0) {
				continue;
			}
			for (size_t relation_id = 0;
				relation_id < ctx->judgement_delta.proposition_count;
				++relation_id) {
				const struct prototype_judgement_proposition* relation =
					&ctx->judgement_delta.propositions[relation_id];
				uint32_t assumption_context_id;
				if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					relation->subject != binder_var ||
					prototype_judgement_candidate_find_derivation_kind(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)relation_id,
						PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
						&(uint32_t){0}
					) != 0 ||
					prototype_context_find_binding(
						&ctx->metadata->contexts,
						relation->context_id,
						ctx->terms->terms[relation->subject].as.var.binding_id,
						&assumption_context_id
					) != 0) {
					continue;
				}
				const struct prototype_context* assumption_context =
					prototype_context_get(
						&ctx->metadata->contexts, assumption_context_id
					);
				const struct prototype_context* argument_context = assumption_context ?
					prototype_context_get(
						&ctx->metadata->contexts, assumption_context->parent
					) : NULL;
				uint32_t argument_classifier =
					prototype_context_classifier_term(argument_context);
				if (!argument_context ||
					argument_classifier == PROTOTYPE_INVALID_ID ||
					!(prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						argument_classifier,
						operation_domain
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					continue;
				}
				classifier = relation->classifier;
				break;
			}
			break;
		}
			if (classifier != PROTOTYPE_INVALID_ID &&
				(operation->referenced_ast_binder_id ==
					occurrence_clause->continuation_ast_binder_id ?
					operation_solver_widen_continuation_binding(
						ctx, operation_id, classifier, p_changed
					) : operation_solver_bind_proven_classifier(
						ctx, operation_id, classifier, p_changed
					)) != 0) {
				return -1;
			}
			if (classifier != PROTOTYPE_INVALID_ID) {
				for (uint32_t lambda_operation_id = 0;
					lambda_operation_id < ctx->metadata->typed_occurrences.occurrence_count;
					++lambda_operation_id) {
					struct prototype_typed_occurrence* lambda_operation =
						&ctx->metadata->typed_occurrences.occurrences[lambda_operation_id];
					uint32_t ast_binder_id;
					if (lambda_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
						lambda_operation->core_term != lambda_term ||
						operation_solver_lambda_ast_binder(
							ctx, lambda_operation_id, &ast_binder_id
						) != 0 ||
						ast_binder_id != operation->referenced_ast_binder_id) {
						continue;
					}
					if (lambda_operation->binder_classifier ==
							PROTOTYPE_INVALID_ID ||
						(operation->referenced_ast_binder_id ==
							occurrence_clause->argument_ast_binder_id &&
						 !(prototype_judgement_classifier_conversion(
							ctx->terms,
							ctx->type_declarations,
							lambda_operation->binder_classifier,
							classifier
						 ).status == PROTOTYPE_TERM_CONVERSION_EQUAL))) {
						lambda_operation->binder_classifier = classifier;
						operation_context_binding_dependency_changed(
							ctx, lambda_operation->binding_id
						);
						*p_changed = 1;
						if (operation_solver_enqueue_dependents(
								ctx, lambda_operation_id
							) != 0) {
							return -1;
						}
					} else if (operation->referenced_ast_binder_id ==
							occurrence_clause->continuation_ast_binder_id &&
						operation_solver_continuation_effect_widening(
							ctx,
							lambda_operation->binder_classifier,
							classifier
						)) {
						lambda_operation->binder_classifier = classifier;
						operation_context_binding_dependency_changed(
							ctx, lambda_operation->binding_id
						);
						*p_changed = 1;
						if (operation_solver_enqueue_dependents(
								ctx, lambda_operation_id
							) != 0) {
							return -1;
						}
					} else if (
						!(prototype_judgement_classifier_conversion(
							ctx->terms,
							ctx->type_declarations,
							lambda_operation->binder_classifier,
							classifier
						).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
						return -1;
					}
				}
			}
		}
	}
	return 0;
}

static int operation_solver_solve(struct compile_context* ctx, int require_complete) {
	if (!ctx) {
		return -1;
	}
	uint32_t branch_restart_count = 0;
	int regenerate_constraints =
		!ctx->classifier_solver.initialized ||
		ctx->classifier_solver.initialized_occurrence_count !=
			ctx->metadata->typed_occurrences.occurrence_count;

restart_after_branch_pullback:
	if (branch_restart_count++ >
		ctx->metadata->typed_occurrences.case_count + 1) {
		fprintf(stderr, "classifier solver: branch pullback did not stabilize\n");
		return -1;
	}
	if (regenerate_constraints && operation_solver_generate_constraints(ctx) != 0) {
		fprintf(stderr, "classifier solver: constraint generation failed\n");
		return -1;
	}
	regenerate_constraints = 0;
	int changed = 0;
	if (operation_solver_seed_known_classifiers(ctx, &changed) != 0) {
		fprintf(stderr, "classifier solver: input seeding failed\n");
		return -1;
	}
	int branch_graph_changed = 0;
	if (operation_solver_solve_branch_refinement_constraints(
			ctx, &branch_graph_changed
		) != 0) {
		fprintf(stderr, "classifier solver: branch refinement failed\n");
		return -1;
	}
	if (branch_graph_changed) {
		if (operation_solver_refresh_classifier_constraints(ctx) != 0) {
			fprintf(stderr,
				"classifier solver: branch constraint refresh failed\n");
			return -1;
		}
		goto restart_after_branch_pullback;
	}
	int refined_motives_rebuilt = 0;
	for (;;) {
		uint32_t i;
		int pop_status;
		while ((pop_status = operation_solver_pop_constraint(ctx, &i)) == 0) {
			int pass_changed = 0;
			const struct operation_constraint* constraint =
				&ctx->constraints.constraints[i];
			struct operation_constraint* constraint_result =
				&ctx->constraints.constraints[i];
			if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER) {
				continue;
			}
			uint32_t target = constraint->payload.classifier.classifier_variable;
			if (ctx->metadata->solver_steps_used >= ctx->metadata->solver_step_limit) {
				ctx->metadata->solver_exhausted = 1;
				operation_solver_refresh_constraint_states(ctx, 1);
				return 1;
			}
			ctx->metadata->solver_steps_used++;
			uint32_t classifier;
			switch (constraint->kind) {
				case OPERATION_CONSTRAINT_EQUAL:
					if (constraint->payload.classifier.goal.reference.referenced_operation >=
							ctx->metadata->typed_occurrences.occurrence_count ||
						(classifier = operation_solver_classifier(
							ctx, constraint->payload.classifier.goal.reference.referenced_operation
						)) ==
							PROTOTYPE_INVALID_ID) {
						break;
					}
					if ((ctx->metadata->typed_occurrences.occurrences[target].tag ==
							PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ?
						operation_solver_bind_proven_classifier(
							ctx, target, classifier, &pass_changed
						) : operation_solver_bind(
							ctx, target, classifier, &pass_changed
						)) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CONVERTIBLE: {
					uint32_t expected_classifier =
						constraint->payload.classifier.goal.conversion.expected_classifier;
						if (expected_classifier == PROTOTYPE_INVALID_ID) {
							expected_classifier = ctx->classifier_hints[target];
						}
					if (constraint->payload.classifier.goal.conversion.body_occurrence >=
							ctx->metadata->typed_occurrences.occurrence_count ||
						expected_classifier >=
							ctx->terms->term_count) {
						break;
					}
						classifier = operation_solver_classifier(
							ctx, constraint->payload.classifier.goal.conversion.body_occurrence
						);
						if (classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						/*
						 * An omitted surface effect row is an elaboration variable.
						 * Resolve it while the TypedOccurrenceGraph fixed point is still
						 * propagating, so downstream APP/COMPUTATION_FOLD proofs never capture the
						 * unsolved annotation. Imported transparent definitions are
						 * checked again by the dedicated ascription phase.
						 */
						uint32_t solved_expected;
						int solve_status =
							prototype_judgement_solve_expected_effect_rows(
								ctx->terms,
								ctx->type_declarations,
								NULL,
								expected_classifier,
								classifier,
								&solved_expected
							);
						if (solve_status < 0) {
							return -1;
						}
						if (solve_status != 0) {
							/* The body classifier is already a valid synthesis frontier.
							 * Preserve it for downstream operations while the dedicated
							 * ascription phase retains responsibility for proving conversion
							 * to the surface expectation. */
							if (operation_solver_bind_proven_classifier(
									ctx,
									target,
									classifier,
									&pass_changed
								) != 0) {
								return -1;
							}
							break;
						}
						constraint_result->conversion_goal.id = constraint->id;
						constraint_result->conversion_goal.context_id =
							constraint->context_id;
						constraint_result->conversion_goal.carrier_classifier =
							PROTOTYPE_INVALID_ID;
						constraint_result->conversion_goal.left_term =
							solved_expected;
						constraint_result->conversion_goal.right_term = classifier;
						constraint_result->conversion_goal.normalization_profile =
							PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF;
						constraint_result->conversion_goal.step_limit =
							ctx->metadata->normalization_step_limit;
						if (prototype_judgement_kernel_conversion_goal_execute(
								&ctx->metadata->contexts,
								ctx->terms,
								ctx->type_declarations,
								NULL,
								&constraint_result->conversion_goal,
								0
							) != 0) {
							return -1;
						}
						if (constraint_result->conversion_goal.result.status !=
							PROTOTYPE_TERM_CONVERSION_EQUAL) {
							if (constraint_result->conversion_goal.result.status ==
								PROTOTYPE_TERM_CONVERSION_INVALID) {
								return -1;
							}
							if (constraint_result->conversion_goal.result.status ==
								PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
								if (operation_constraint_transition(
										constraint_result,
										OPERATION_CONSTRAINT_STATE_CONTRADICTION,
										OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED
									) != 0) {
									return -1;
								}
							}
							if (operation_solver_bind(
									ctx, target, classifier, &pass_changed
								) != 0) {
								return -1;
							}
							break;
						}
						ctx->classifier_hints[target] = solved_expected;
						if (operation_solver_bind(
								ctx,
								target,
								solved_expected,
								&pass_changed
							) != 0) {
							return -1;
							}
							break;
						}
				case OPERATION_CONSTRAINT_PI_EXPECTED:
					if (constraint->payload.classifier.goal.pi.body_or_function_operation >=
							ctx->metadata->typed_occurrences.occurrence_count ||
						(constraint->payload.classifier.goal.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
							constraint->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence >=
								ctx->metadata->typed_occurrences.occurrence_count)) {
						break;
					}
					if (constraint->payload.classifier.goal.pi.role == OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
						uint32_t expected =
							ctx->classifier_solver.solutions[target].classifier;
						uint32_t current_body_classifier = operation_solver_classifier(
							ctx, constraint->payload.classifier.goal.pi.body_or_function_operation
						);
						if (expected != PROTOTYPE_INVALID_ID &&
							current_body_classifier == PROTOTYPE_INVALID_ID) {
							uint32_t domain;
							uint32_t codomain_family;
							uint32_t codomain_binder;
							uint32_t codomain_body;
							uint32_t binder_var;
							uint32_t body_classifier;
							const struct prototype_typed_occurrence* lambda =
								&ctx->metadata->typed_occurrences.occurrences[target];
							if (lambda->core_term >= ctx->terms->term_count ||
								ctx->terms->terms[lambda->core_term].tag !=
									PROTOTYPE_TERM_LAMBDA ||
								prototype_judgement_pi_parts(
									ctx->terms, expected, &domain, &codomain_family
								) != 0 ||
								prototype_term_pure_family_parts(
									ctx->terms,
									codomain_family,
									&codomain_binder,
									&codomain_body
								) != 0 ||
								!(prototype_judgement_classifier_conversion(
									ctx->terms, ctx->type_declarations,
									domain,
									lambda->binder_classifier
								).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
								lambda->binding_id == PROTOTYPE_INVALID_ID ||
								prototype_term_var(
									ctx->terms, lambda->binding_id,
									&binder_var
								) != 0 ||
								prototype_term_graph_substitute_bound_var(
									ctx->terms,
									ctx->type_declarations,
									codomain_body,
									codomain_binder,
									binder_var,
									&body_classifier
								) != 0 ||
								operation_solver_bind(
									ctx,
									constraint->payload.classifier.goal.pi.body_or_function_operation,
									body_classifier,
									&pass_changed
								) != 0) {
								/* The expected Pi may be more reduced than the current
								 * operation frontier. Leave this reverse propagation for a
								 * later pass; the forward Pi constraint remains authoritative. */
								break;
							}
						}
						classifier = operation_solver_classifier(
							ctx, constraint->payload.classifier.goal.pi.body_or_function_operation
						);
						if (classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						uint32_t codomain_family;
						const struct prototype_typed_occurrence* lambda =
							&ctx->metadata->typed_occurrences.occurrences[target];
						uint32_t effective_domain = lambda->binder_classifier;
						if (effective_domain == PROTOTYPE_INVALID_ID) {
							break;
						}
						if (lambda->core_term >= ctx->terms->term_count ||
							ctx->terms->terms[lambda->core_term].tag !=
								PROTOTYPE_TERM_LAMBDA ||
							lambda->binding_id == PROTOTYPE_INVALID_ID ||
							prototype_term_pure_family(
								ctx->terms,
								lambda->binding_id,
								classifier,
								&codomain_family
							) != 0 ||
							prototype_term_pi_family(
								ctx->terms,
								effective_domain,
								codomain_family,
								&classifier
							) != 0 || operation_solver_generalize_lambda_effect_rows(
								ctx, target, classifier, &classifier
							) != 0 || operation_solver_bind_proven_classifier(
								ctx, target, classifier, &pass_changed
							) != 0) {
							return -1;
						}
					} else {
						/* A source APP may materialize a named TypeView before all family
						 * parameters are saturated. Its classifier is obtained by replaying
						 * the declaration telescope, not by treating the intermediate
						 * TypeView classifier as a runtime-callable Pi. */
						int type_instance_status = type_instance_formation_classifier(
							ctx,
							ctx->metadata->typed_occurrences.occurrences[target].core_term,
							&classifier
						);
						if (type_instance_status < 0) {
							return -1;
						}
						if (type_instance_status == 0) {
							if (operation_solver_bind_proven_classifier(
									ctx, target, classifier, &pass_changed
								) != 0) {
								return -1;
							}
							break;
						}
						uint32_t function_classifier =
							operation_solver_classifier(
								ctx, constraint->payload.classifier.goal.pi.body_or_function_operation
							);
						uint32_t argument_classifier =
							operation_solver_classifier(
								ctx,
								constraint->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence
							);
						if (function_classifier != PROTOTYPE_INVALID_ID &&
							argument_classifier == PROTOTYPE_INVALID_ID) {
							uint32_t function_whnf;
							uint32_t expected_domain;
							uint32_t ignored_family;
							if (prototype_term_normalize_complete_with_profile(
									ctx->terms,
									ctx->type_declarations,
									NULL,
									PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
									function_classifier,
									&function_whnf
								) != 0) {
								return -1;
							}
							struct prototype_term_classifier_view function_view;
							int function_view_status = prototype_judgement_classifier_view(
									ctx->terms, ctx->type_declarations, NULL,
									function_whnf, &function_view
								);
							if (function_view_status == 0 && function_view.category ==
									PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
								function_view.result < ctx->terms->term_count) {
								function_whnf = function_view.result;
							}
							if (function_whnf < ctx->terms->term_count &&
								ctx->terms->terms[function_whnf].tag ==
									PROTOTYPE_TERM_THUNK_TYPE) {
								function_whnf = ctx->terms->terms[
									function_whnf
								].as.thunk_type.computation;
							}
							for (uint32_t depth = 0; depth < 32 &&
								function_whnf < ctx->terms->term_count &&
								ctx->terms->terms[function_whnf].tag ==
									PROTOTYPE_TERM_EFFECT_ROW_FORALL; ++depth) {
								const struct prototype_term* quantified =
									&ctx->terms->terms[function_whnf];
								uint32_t empty_row;
								uint32_t instantiated;
								if (prototype_term_effect_row_empty(
										ctx->terms, &empty_row
									) != 0 || prototype_term_graph_substitute_bound_var(
										ctx->terms,
										ctx->type_declarations,
										quantified->as.effect_row_forall.body,
										quantified->as.effect_row_forall.binding_id,
										empty_row,
										&instantiated
									) != 0) {
									return -1;
								}
								function_whnf = instantiated;
							}
							if (prototype_judgement_pi_parts(
									ctx->terms, function_whnf,
									&expected_domain, &ignored_family
								) == 0) {
								int refine_status =
									operation_refine_classifier_from_expected(
										ctx,
										constraint->payload.classifier.goal.pi.
											domain_classifier_or_argument_occurrence,
										expected_domain,
										0,
										&pass_changed
									);
								if (refine_status < 0) {
									return -1;
								}
								argument_classifier = operation_solver_classifier(
									ctx,
									constraint->payload.classifier.goal.pi.
										domain_classifier_or_argument_occurrence
								);
								if (argument_classifier == PROTOTYPE_INVALID_ID &&
									operation_solver_bind(
										ctx,
										constraint->payload.classifier.goal.pi.
											domain_classifier_or_argument_occurrence,
										expected_domain,
										&pass_changed
									) != 0) {
									return -1;
								}
								argument_classifier = operation_solver_classifier(
									ctx,
									constraint->payload.classifier.goal.pi.
										domain_classifier_or_argument_occurrence
								);
							}
						}
						if (function_classifier == PROTOTYPE_INVALID_ID ||
							argument_classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						uint32_t expected_argument_domain;
						uint32_t ignored_argument_family;
						int argument_pi_status = prototype_judgement_pi_parts(
								ctx->terms, function_classifier,
								&expected_argument_domain, &ignored_argument_family
							);
						if (argument_pi_status == 0) {
							uint32_t argument_operation = constraint->payload.classifier.
								goal.pi.domain_classifier_or_argument_occurrence;
							int refine_status = operation_refine_classifier_from_expected(
								ctx, argument_operation, expected_argument_domain, 0,
								&pass_changed
							);
							if (refine_status < 0) {
								return -1;
							}
							argument_classifier = operation_solver_classifier(
								ctx, argument_operation
							);
						}
						int apply_status = operation_apply_typed_occurrence_classifier(
							ctx,
							target,
							function_classifier,
							argument_classifier,
							ctx->metadata->typed_occurrences.occurrences[
								constraint->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence
							].core_term,
							&classifier
						);
						if (apply_status != 0) {
							uint32_t reindexed_function_classifier;
							uint32_t reindexed_argument_classifier;
							if (operation_solver_reindex_in_enclosing_indexed_branches(
									ctx,
									target,
									ctx->metadata->typed_occurrences.occurrences[target].context_id,
									function_classifier,
									&reindexed_function_classifier
								) != 0 ||
								operation_solver_reindex_in_enclosing_indexed_branches(
									ctx,
									target,
									ctx->metadata->typed_occurrences.occurrences[target].context_id,
									argument_classifier,
									&reindexed_argument_classifier
								) != 0) {
								return -1;
							}
							apply_status = operation_apply_typed_occurrence_classifier(
								ctx,
								target,
								reindexed_function_classifier,
								reindexed_argument_classifier,
								ctx->metadata->typed_occurrences.occurrences[
									constraint->payload.classifier.goal.pi.
										domain_classifier_or_argument_occurrence
								].core_term,
								&classifier
							);
						}
						/* A non-Pi or incompatible current candidate may belong to a
						 * shared core node. Leave this operation constraint unresolved
						 * until a source-operation binding selects its classifier. */
						if (apply_status == 0 && operation_solver_bind_proven_classifier(
								ctx, target, classifier, &pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION: {
					if (operation_solver_propagate_constructor_domains(
							ctx, target, &pass_changed
						) != 0) {
						return -1;
					}
					if (pass_changed) {
						if (operation_solver_enqueue_constraint(ctx, i) != 0) {
							return -1;
						}
						break;
					}
					struct operation_solver_constructor_mismatch mismatch;
					int constructor_status =
						operation_solver_constructor_spine_classifier(
							ctx, target, &classifier, &pass_changed, &mismatch
						);
					if (constructor_status < 0) {
						return -1;
					}
					if (constructor_status ==
							OPERATION_SOLVER_CONSTRUCTOR_SPINE_WAITING &&
						pass_changed) {
						if (operation_solver_enqueue_constraint(ctx, i) != 0) {
							return -1;
						}
						changed = 1;
						break;
					}
					if (constructor_status ==
							OPERATION_SOLVER_CONSTRUCTOR_SPINE_CONTRADICTION) {
						if (pass_changed) {
							changed = 1;
							break;
						}
						int waits_for_branch =
							operation_solver_occurrence_waits_for_branch_action(
								ctx, target
							);
						if (waits_for_branch < 0) {
							return -1;
						}
						if (waits_for_branch > 0) {
							break;
						}
						const struct prototype_typed_occurrence* operation =
							&ctx->metadata->typed_occurrences.occurrences[target];
						(void)add_constructor_compile_diagnostic(
							ctx,
							operation->source_ast,
							target,
							constraint->id,
							mismatch.expected_classifier,
							mismatch.actual_classifier,
							mismatch.context_id,
							mismatch.constructor_ordinal,
							mismatch.field_ordinal,
							operation->source_ast < ctx->asts->node_count ?
								ctx->asts->nodes[operation->source_ast].span :
								(struct prototype_source_span) { 0, 0 }
						);
						return -1;
					}
					/* The declaration telescope is the constructor classifier authority.
					 * A prior pass may have projected an under-refined indexed result;
					 * replace that projection after the dependent fields are solved. */
					if (constructor_status == 0 && operation_solver_bind_proven_classifier(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_BINDER_TYPE: {
					if (ctx->classifier_solver.solutions[target].classifier !=
							PROTOTYPE_INVALID_ID) {
						break;
					}
					if (constraint->payload.classifier.goal.binder_type.
							binding_context == PROTOTYPE_INVALID_ID) {
						break;
					}
					const struct prototype_context* binding = prototype_context_get(
						&ctx->metadata->contexts,
						constraint->payload.classifier.goal.binder_type.binding_context
					);
					if (!binding) {
						return -1;
					}
					classifier = prototype_context_classifier_term(binding);
					if (classifier != PROTOTYPE_INVALID_ID &&
						operation_solver_bind_proven_classifier(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_HAS_TYPE:
					if (ctx->classifier_solver.solutions[target].classifier !=
						PROTOTYPE_INVALID_ID) {
						break;
					}
					/* Core facts may seed atoms only, never a typed operation edge. */
					if (ctx->metadata->typed_occurrences.occurrences[target].tag ==
							PROTOTYPE_TYPED_OCCURRENCE_APP ||
						ctx->metadata->typed_occurrences.occurrences[target].tag ==
							PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
						ctx->metadata->typed_occurrences.occurrences[target].tag ==
							PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
						break;
					}
					{
						uint32_t classifiers[32];
						uint32_t classifier_count = 0;
						if (operation_solver_collect_input_classifiers(
								ctx,
								target,
								classifiers,
								32,
								&classifier_count
							) != 0) {
							return -1;
						}
						if (classifier_count == 1 && operation_solver_bind(
								ctx, target, classifiers[0], &pass_changed
							) != 0) {
							return -1;
						}
					}
					break;
				case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
					if (operation_solver_solve_match(
							ctx, target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_IH_EXPECTED:
					if (operation_solver_materialize_induction_hypothesis(
							ctx, target, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				case OPERATION_CONSTRAINT_CBPV_BOUNDARY: {
					if (constraint->payload.classifier.goal.cbpv_boundary.child_operation >=
						ctx->metadata->typed_occurrences.occurrence_count) {
						return -1;
					}
					uint32_t child_classifier = operation_solver_classifier(
						ctx, constraint->payload.classifier.goal.cbpv_boundary.child_operation
					);
					if (child_classifier == PROTOTYPE_INVALID_ID) {
						break;
					}
					int boundary_status = prototype_judgement_cbpv_boundary_classifier(
							ctx->terms,
							ctx->type_declarations,
							ctx->metadata->typed_occurrences.occurrences[target].core_term,
							child_classifier,
							&classifier
						);
					/* A child may still carry its synthesis frontier while a later
					 * constructor-domain or APP constraint supplies the value/computation
					 * boundary. Keep this equation pending instead of turning worklist
					 * order into a typing error. The strict kernel helper is rerun once
					 * that occurrence classifier changes. */
					if (boundary_status != 0) {
						break;
					}
					if (operation_solver_bind_proven_classifier(
							ctx, target, classifier, &pass_changed
						) != 0) {
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT: {
					const struct prototype_typed_occurrence* fold =
						&ctx->metadata->typed_occurrences.occurrences[target];
					if (fold->core_term >= ctx->terms->term_count ||
						ctx->terms->terms[fold->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
						return -1;
					}
					int status = ctx->terms->terms[fold->core_term].as.computation_fold.clause_count == 0 ?
						operation_solver_propagate_zero_clause_computation_fold_input(
							ctx, target, &pass_changed
						) : operation_solver_propagate_clause_computation_fold_input(
							ctx, target, &pass_changed
					);
					if (status != 0) {
						fprintf(stderr,
							"computation fold input propagation failed operation=%u status=%d "
							"clauses=%u\n",
							target, status,
							ctx->terms->terms[fold->core_term].as.computation_fold.
								clause_count);
						return -1;
					}
					break;
				}
				case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
					/* These occurrence constraints are solved by the CBPV computation
					 * constraint pass. Their state is published only after that pass has
					 * either produced closed evidence or a VerificationDB obligation. */
					break;
				default:
					return -1;
			}
			if (pass_changed) {
				changed = 1;
			}
		}
		if (pop_status < 0) {
			return -1;
		}
		if (!refined_motives_rebuilt) {
			if (operation_solver_rebuild_refined_match_motives(ctx) != 0) {
				fprintf(stderr,
					"classifier solver: refined Match motive rebuild failed\n");
				return -1;
			}
			refined_motives_rebuilt = 1;
		}
		int materialized = 0;
		if (operation_solver_materialize_solved_motives(
				ctx, &materialized
			) != 0) {
			fprintf(stderr, "classifier solver: motive materialization pass failed\n");
			return -1;
		}
		branch_graph_changed = 0;
		if (operation_solver_solve_branch_refinement_constraints(
				ctx, &branch_graph_changed
			) != 0) {
			fprintf(stderr, "classifier solver: branch refinement pass failed\n");
			return -1;
		}
		if (branch_graph_changed) {
			if (operation_solver_refresh_classifier_constraints(ctx) != 0) {
				fprintf(stderr,
					"classifier solver: branch constraint refresh failed\n");
				return -1;
			}
			goto restart_after_branch_pullback;
		}
		if (!materialized) {
			if (require_complete && operation_solver_require_complete(ctx) != 0) {
				operation_solver_refresh_constraint_states(ctx, 1);
				return -1;
			}
			operation_solver_refresh_constraint_states(ctx, 0);
			return 0;
		}
		changed = 1;
	}
}
