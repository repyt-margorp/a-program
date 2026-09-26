static int compile_phase_infer_general_classifiers(
	struct compile_context* ctx,
	int require_complete
) {
	if (!ctx) {
		return -1;
	}
	if (ctx->typing->constraints.topology_sealed) {
		ctx->metadata->proof_solver_call_by_domain[
			PROTOTYPE_COMPILE_SOLUTION_CLASSIFIER
		]++;
		return -1;
	}
	return operation_solver_solve(ctx, require_complete);
}

static size_t count_classified_operations(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return 0;
	}
	ctx->metadata->full_scan_count[PROTOTYPE_COMPILE_SCAN_OCCURRENCE]++;
	ctx->metadata->full_scan_visit_count[PROTOTYPE_COMPILE_SCAN_OCCURRENCE] +=
		ctx->metadata->typed_occurrences.occurrence_count;
	size_t count = 0;
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		if (operation_available_classifier(ctx, i) != PROTOTYPE_INVALID_ID) {
			count++;
		}
	}
	return count;
}

static int operation_solver_match_classifier_parts(
	struct compile_context* ctx,
	uint32_t classifier,
	int* p_returns_computation,
	uint32_t* p_result,
	uint32_t* p_effect_row,
	int* p_totality
);

static int operation_solver_branch_subtree_reaches(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation
);

static void operation_solver_invalidate_reindex_cache(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->typing) {
		return;
	}
	ctx->typing->reindex.revision++;
	if (ctx->typing->reindex.revision == 0) {
		memset(
			ctx->typing->reindex.entries,
			0,
			sizeof(ctx->typing->reindex.entries)
		);
		ctx->typing->reindex.revision = 1;
	}
}

static int operation_solver_reindex_in_enclosing_indexed_branches(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t term,
	uint32_t* p_reindexed
) {
	if (!ctx || !ctx->typing || !ctx->metadata ||
		!p_reindexed || term >= ctx->terms->term_count ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	uint32_t cache_hash = operation_id * UINT32_C(2654435761) ^
		context_id * UINT32_C(2246822519) ^ term * UINT32_C(3266489917);
	uint32_t cache_slot = cache_hash % PROTOTYPE_TYPING_REINDEX_CACHE_CAPACITY;
	const struct prototype_typing_reindex_cache_entry* cached =
		&ctx->typing->reindex.entries[cache_slot];
	if (cached->occupied && cached->occurrence_id == operation_id &&
		cached->context_id == context_id && cached->term == term &&
		cached->dependency_signature == ctx->typing->reindex.revision) {
		*p_reindexed = cached->reindexed;
		return 0;
	}
	uint32_t current = term;
	for (uint32_t match_cursor = ctx->metadata->typed_occurrences.occurrence_count;
		match_cursor > 0;) {
		uint32_t match_id = --match_cursor;
		const struct prototype_typed_occurrence* match_operation =
			&ctx->metadata->typed_occurrences.occurrences[match_id];
		uint32_t scrutinee_operation;
		if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			compile_occurrence_child(
				ctx, match_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
				&scrutinee_operation
			) != 0 ||
			match_operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[match_operation->core_term].tag !=
				PROTOTYPE_TERM_MATCH) {
			continue;
		}
		for (uint32_t case_index = 0;
			case_index < match_operation->case_count; ++case_index) {
			uint32_t branch_operation;
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[
					match_operation->first_case + case_index
				];
			uint32_t context_path[512];
			uint32_t context_path_count;
			int context_path_status = prototype_context_extension_path(
				&ctx->metadata->contexts, operation_case->context_id,
				context_id, context_path, 512, &context_path_count
			);
			int operation_path_status = compile_occurrence_child(
				ctx, match_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, case_index,
				&branch_operation
			) == 0 ? operation_solver_branch_subtree_reaches(
				ctx, branch_operation, operation_id
			) : -1;
			if (operation_case->constructor_id == PROTOTYPE_INVALID_ID ||
				context_path_status != 0 || operation_path_status == 0) {
				continue;
			}
			if (operation_path_status < 0) {
				fprintf(
					stderr,
					"branch reindex: subtree lookup failed operation=%u match=%u "
					"case=%u branch=%u\n",
					operation_id,
					match_id,
					case_index,
					branch_operation
				);
				return -1;
			}
			uint32_t refinement_substitution = PROTOTYPE_INVALID_ID;
			int refinement_status = PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
				if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
				const struct prototype_substitution* refinement =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						operation_case->refinement_substitution
					);
					uint32_t materialized_case_context =
						operation_case_materialized_context(
							ctx, match_operation->first_case + case_index
						);
					if (!refinement || materialized_case_context ==
							PROTOTYPE_INVALID_ID ||
						refinement->source_context != materialized_case_context) {
					fprintf(
						stderr,
						"branch reindex: invalid solved action operation=%u match=%u "
						"case=%u substitution=%u materialized=%u source=%u\n",
						operation_id,
						match_id,
						case_index,
						operation_case->refinement_substitution,
						materialized_case_context,
						refinement ? refinement->source_context : PROTOTYPE_INVALID_ID
					);
					return -1;
				}
				refinement_substitution = operation_case->refinement_substitution;
				refinement_status = PROTOTYPE_INDEX_REFINEMENT_SOLVED;
			} else {
				struct prototype_typing_constraint* refinement =
					operation_solver_branch_refinement_constraint(
						ctx, match_operation->first_case + case_index
					);
				if (refinement) {
					const struct prototype_typing_constraint_solution* solution =
						&ctx->typing->constraints.solutions[refinement->id];
					refinement_status =
						solution->branch_refinement.refinement_status;
					refinement_substitution =
						solution->branch_refinement.substitution;
				}
			}
			if (refinement_status != PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
				continue;
			}
			if (prototype_term_reindex(
					ctx->terms, ctx->type_declarations,
					&ctx->metadata->contexts, &ctx->metadata->substitutions,
					current, refinement_substitution, &current
				) != 0) {
				fprintf(
					stderr,
					"branch reindex: term action failed operation=%u match=%u "
					"case=%u term=%u substitution=%u\n",
					operation_id,
					match_id,
					case_index,
					current,
					refinement_substitution
				);
				return -1;
			}
		}
	}
	*p_reindexed = current;
	/* TermDB nodes are immutable once interned. Cache against the enclosing
	 * indexed branches that can change this action and overwrite collisions. */
	struct prototype_typing_reindex_cache_entry* entry =
		&ctx->typing->reindex.entries[cache_slot];
	entry->occurrence_id = operation_id;
	entry->context_id = context_id;
	entry->term = term;
	entry->reindexed = current;
	entry->dependency_signature = ctx->typing->reindex.revision;
	entry->occupied = 1;
	return 0;
}

static int operation_solver_projection_reaches_occurrence(
	const struct compile_context* ctx,
	uint32_t typed_projection_id,
	uint32_t target_occurrence,
	uint8_t* visited,
	uint32_t depth
) {
	if (!ctx || !ctx->typing || !ctx->metadata || !visited || depth > 512 ||
		typed_projection_id >=
			ctx->typing->context_projections.typed_projection_count ||
		target_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	if (visited[typed_projection_id]) {
		return 0;
	}
	visited[typed_projection_id] = 1;
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection_id
		);
	if (!projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	if (projection->source_occurrence == target_occurrence) {
		return 1;
	}
	const struct prototype_typed_occurrence* source =
		&ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		];
	for (uint32_t edge_id = prototype_typed_projection_first_edge(
			&ctx->typing->context_projections, typed_projection_id
		);
		edge_id != PROTOTYPE_INVALID_ID;
		edge_id = prototype_typed_projection_next_edge(
			&ctx->typing->context_projections, edge_id
		)) {
		const struct prototype_typed_projection_edge* edge =
			prototype_typed_projection_edge_get(
				&ctx->typing->context_projections, edge_id
			);
		if (!edge || edge->parent_typed_projection != typed_projection_id) {
			return -1;
		}
		if (edge->kind == PROTOTYPE_TYPED_PROJECTION_EDGE_INDUCTION_OWNER) {
			continue;
		}
		/* References retain their own branch occurrence but do not pull a
		 * separately rooted definition into that branch. */
		if (edge->kind == PROTOTYPE_TYPED_PROJECTION_EDGE_WRAPPED &&
			source->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			continue;
		}
		int reaches = operation_solver_projection_reaches_occurrence(
			ctx, edge->child_typed_projection, target_occurrence, visited, depth + 1
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	return 0;
}

static int operation_solver_branch_subtree_reaches(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation
) {
	if (!ctx || !ctx->typing || !ctx->metadata) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph =
		&ctx->metadata->typed_occurrences;
	if (root_operation >= graph->occurrence_count ||
		target_operation >= graph->occurrence_count) {
		return -1;
	}
	size_t projection_count =
		ctx->typing->context_projections.typed_projection_count;
	uint8_t visited[projection_count == 0 ? 1 : projection_count];
	memset(visited, 0, sizeof(visited));
	for (uint32_t projection = prototype_typed_projection_first_for_typing_node(
			&ctx->typing->context_projections, root_operation
		);
		projection != PROTOTYPE_INVALID_ID;
		projection = prototype_typed_projection_next_for_typing_node(
			&ctx->typing->context_projections, projection
		)) {
		int reaches = operation_solver_projection_reaches_occurrence(
			ctx, projection, target_operation, visited, 0
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	return 0;
}

static int operation_solver_occurrence_is_in_impossible_branch(
	const struct compile_context* ctx,
	uint32_t occurrence_id
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	return prototype_typed_occurrence_graph_occurrence_is_unreachable(
		&ctx->metadata->typed_occurrences, occurrence_id
	);
}


static int operation_solver_occurrence_waits_for_branch_action(
	struct compile_context* ctx,
	uint32_t occurrence_id
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		uint32_t case_id = match_id <
			ctx->metadata->typed_occurrences.occurrence_count ?
			ctx->metadata->typed_occurrences.occurrences[match_id].first_case +
				constraint->payload.branch_refinement.case_index :
			PROTOTYPE_INVALID_ID;
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			case_id >= ctx->metadata->typed_occurrences.case_count) {
			continue;
		}
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[case_id];
		if (prototype_typed_occurrence_match_case_is_admitted(operation_case) ||
			operation_case->refinement_status ==
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
			continue;
		}
		int reaches = operation_solver_branch_subtree_reaches(
			ctx,
			constraint->source_occurrence,
			occurrence_id
		);
		if (reaches < 0) {
			return -1;
		}
		if (reaches > 0) {
			return 1;
		}
		/* A consumer can also contain the unresolved Match in one of its
		 * operands. This is the opposite graph direction from an occurrence
		 * located inside the branch, but it is the same semantic dependency:
		 * rejecting the consumer before constructor resolution would turn
		 * worklist order into a typing decision. */
		reaches = operation_solver_branch_subtree_reaches(
			ctx, occurrence_id, match_id
		);
		if (reaches < 0) {
			return -1;
		}
		if (reaches > 0) {
			return 1;
		}
	}
	return 0;
}

static struct prototype_typing_constraint* operation_solver_branch_refinement_constraint(
	struct compile_context* ctx,
	uint32_t operation_case_id
) {
	if (!ctx || !ctx->metadata || operation_case_id >=
		ctx->metadata->typed_occurrences.case_count) {
		return NULL;
	}
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT &&
			match_id < ctx->metadata->typed_occurrences.occurrence_count &&
			ctx->metadata->typed_occurrences.occurrences[match_id].first_case +
				constraint->payload.branch_refinement.case_index ==
				operation_case_id) {
			return constraint;
		}
	}
	return NULL;
}

static int operation_solver_waits_for_enclosing_branch_refinement(
	struct compile_context* ctx,
	uint32_t branch_typed_projection,
	uint32_t current_match_case_projection
) {
	if (!ctx || !ctx->typing || branch_typed_projection >=
			ctx->typing->context_projections.typed_projection_count ||
		current_match_case_projection >=
			ctx->typing->context_projections.match_case_projection_count) {
		return -1;
	}
	const struct prototype_typed_projection* branch =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, branch_typed_projection
		);
	const struct prototype_context_projection* context_projection = branch ?
		prototype_context_projection_get(
			&ctx->typing->context_projections, branch->context_projection
		) : NULL;
	if (!branch || !context_projection) {
		return -1;
	}
	uint32_t action_id = context_projection->action_expression;
	for (uint32_t depth = 0; depth <= 512; ++depth) {
		const struct prototype_context_action_expression* action =
			prototype_context_action_expression_get(
				&ctx->typing->context_projections, action_id
			);
		if (!action) {
			return -1;
		}
		if (action->kind == PROTOTYPE_CONTEXT_ACTION_IDENTITY) {
			return 0;
		}
		if (action->kind == PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT &&
			action->dependency_id != current_match_case_projection) {
			uint32_t case_projection = action->dependency_id;
			if (case_projection >=
					ctx->typing->context_projections.match_case_projection_count ||
				case_projection >=
					PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
				return -1;
			}
			uint32_t equation_id = ctx->typing->constraints.
				branch_refinement_constraint_for_match_case_projection[
					case_projection
				];
			if (equation_id >= ctx->typing->constraints.constraint_count) {
				return -1;
			}
			const struct prototype_typing_constraint* equation =
				&ctx->typing->constraints.constraints[equation_id];
			const struct prototype_typing_constraint_solution* solution =
				&ctx->typing->constraints.solutions[equation_id];
			if (equation->domain !=
					OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
				equation->payload.branch_refinement.match_case_projection !=
					case_projection) {
				return -1;
			}
			if (solution->state == OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
				return -1;
			}
			if (solution->state != OPERATION_CONSTRAINT_STATE_SOLVED) {
				/* Context actions compose outer refinements before inner ones.
				 * The immutable action chain is the dependency path; no graph
				 * rediscovery is required here. */
				if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
					fprintf(
						stderr,
						"branch action wait projection=%u current-case=%u "
						"dependency-case=%u equation=%u state=%d\n",
						branch_typed_projection,
						current_match_case_projection,
						case_projection,
						equation_id,
						solution->state
					);
				}
				return 1;
			}
		}
		if (action->parent_action == PROTOTYPE_INVALID_ID ||
			action->parent_action == action_id) {
			return -1;
		}
		action_id = action->parent_action;
	}
	return -1;
}

static int operation_solver_match_motive_is_constant(
	const struct compile_context* ctx,
	uint32_t match_typed_projection
) {
	if (!ctx || !ctx->metadata || match_typed_projection >=
		ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	const struct prototype_typing_motive_solution* solution =
		&ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	uint32_t motive = solution->motive;
	if (motive == PROTOTYPE_INVALID_ID) {
		/* A constant candidate is the solver's branch equation M(_) = T.
		 * It is enough to show that index refinement is irrelevant; motive
		 * materialization still validates every reachable branch against T. */
		return solution->constant_candidate != PROTOTYPE_INVALID_ID;
	}
	int saw_lambda = 0;
	while (motive < ctx->terms->term_count &&
		ctx->terms->terms[motive].tag == PROTOTYPE_TERM_LAMBDA) {
		const struct prototype_term* lambda = &ctx->terms->terms[motive];
		uint32_t body_whnf;
		if (typing_core_normalize_complete(
				ctx->core_requests,
				PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
				lambda->as.lambda.body,
				&body_whnf
			) != 0 || body_whnf >= ctx->terms->term_count) {
			return -1;
		}
		if (prototype_term_contains_free_binding(
				ctx->terms, body_whnf, lambda->as.lambda.binding_id
			)) {
			return 0;
		}
		saw_lambda = 1;
		motive = body_whnf;
	}
	return saw_lambda && motive < ctx->terms->term_count;
}

static int operation_solver_seed_accepted_branch_refinement(
	struct compile_context* ctx,
	const struct prototype_typing_constraint* constraint,
	const struct prototype_typed_occurrence_match_case* operation_case,
	struct prototype_typing_constraint_solution* solution,
	int* p_seeded
) {
	if (!ctx || !constraint || !operation_case || !solution || !p_seeded) {
		return -1;
	}
	*p_seeded = 0;
	uint32_t match_id = constraint->payload.branch_refinement.match_occurrence;
	uint32_t match_projection =
		constraint->payload.branch_refinement.match_typed_projection;
	if (match_id >= ctx->accepted_typing.occurrence_count ||
		ctx->typing->publication_typed_projection_for_occurrence[match_id] !=
			match_projection) {
		return 0;
	}
	uint32_t source_case = ctx->metadata->typed_occurrences.occurrences[
		match_id
	].first_case + constraint->payload.branch_refinement.case_index;
	const struct prototype_typed_publication_match_case_projection* accepted =
		prototype_accepted_typing_input_match_case(
			&ctx->accepted_typing, source_case
		);
	if (!accepted || accepted->source_case != source_case) {
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
			fprintf(stderr,
				"accepted branch seed missing constraint=%u match=%u projection=%u "
				"source-case=%u accepted-source=%u\n",
				constraint->id, match_id, match_projection, source_case,
				accepted ? accepted->source_case : PROTOTYPE_INVALID_ID);
		}
		return -1;
	}
	int refinement_status;
	int reason;
	switch (operation_case->refinement_status) {
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED:
			refinement_status = PROTOTYPE_INDEX_REFINEMENT_SOLVED;
			reason = OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_VALIDATED;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT:
			refinement_status = PROTOTYPE_INDEX_REFINEMENT_CONSTANT;
			reason = OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_CONSTANT;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE:
			refinement_status = PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
			reason = OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_IMPOSSIBLE;
			break;
		default:
			if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
				fprintf(stderr,
					"accepted branch seed status invalid constraint=%u match=%u "
					"case=%u status=%d\n",
					constraint->id, match_id, source_case,
					operation_case->refinement_status);
			}
			return -1;
	}
	uint32_t substitution = refinement_status == PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
		accepted->refinement_substitution : PROTOTYPE_INVALID_ID;
	uint32_t refined_context = refinement_status == PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
		accepted->concrete_context : PROTOTYPE_INVALID_ID;
	uint32_t materialized_source_context = accepted->concrete_context;
	if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
		const struct prototype_substitution* action = prototype_substitution_get(
			&ctx->metadata->substitutions, substitution
		);
		if (!action || action->source_context != refined_context) {
			if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
				fprintf(stderr,
					"accepted branch seed substitution invalid constraint=%u match=%u "
					"projection=%u case=%u substitution=%u context=%u/%u expected=%u/%u\n",
					constraint->id, match_id, match_projection, source_case,
					substitution,
					action ? action->source_context : PROTOTYPE_INVALID_ID,
					action ? action->target_context : PROTOTYPE_INVALID_ID,
					refined_context, PROTOTYPE_INVALID_ID);
			}
			return -1;
		}
		materialized_source_context = action->target_context;
	} else if (accepted->refinement_substitution != PROTOTYPE_INVALID_ID) {
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
			fprintf(stderr,
				"accepted branch seed unexpected substitution constraint=%u match=%u "
				"projection=%u case=%u status=%d substitution=%u\n",
				constraint->id, match_id, match_projection, source_case,
				refinement_status, accepted->refinement_substitution);
		}
		return -1;
	}
	int changed = solution->branch_refinement.refinement_status !=
			refinement_status ||
		solution->branch_refinement.materialized_source_context !=
			materialized_source_context ||
		solution->branch_refinement.refined_context != refined_context ||
		solution->branch_refinement.substitution != substitution;
	solution->branch_refinement.materialized_source_context =
		materialized_source_context;
	solution->branch_refinement.refinement_status = refinement_status;
	solution->branch_refinement.refined_context = refined_context;
	solution->branch_refinement.substitution = substitution;
	if (prototype_typing_constraint_transition(
			&ctx->typing->constraints,
			constraint->id,
			OPERATION_CONSTRAINT_STATE_SOLVED,
			reason
		) != 0) {
		return -1;
	}
	if (changed && (operation_solver_enqueue_equation_dependents(
			ctx, constraint->id
		) != 0 || operation_motive_reopen_after_branch_refinement(
			ctx, match_projection
		) != 0)) {
		return -1;
	}
	if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
		fprintf(
			stderr,
			"accepted branch seed constraint=%u match=%u projection=%u "
			"case-projection=%u source-case=%u context=%u substitution=%u\n",
			constraint->id,
			match_id,
			match_projection,
			constraint->payload.branch_refinement.match_case_projection,
			source_case,
			refined_context,
			substitution
		);
	}
	*p_seeded = 1;
	return 0;
}

static int operation_solver_solve_branch_refinement_constraints(
	struct compile_context* ctx,
	int* p_projection_waiting
) {
	if (!ctx || !ctx->metadata || !p_projection_waiting) {
		return -1;
	}
	*p_projection_waiting = 0;
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[id];
		struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		uint32_t scrutinee_id =
			constraint->payload.branch_refinement.scrutinee_occurrence;
		uint32_t case_index =
			constraint->payload.branch_refinement.case_index;
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			match_id >= ctx->metadata->typed_occurrences.occurrence_count ||
			scrutinee_id >= ctx->metadata->typed_occurrences.occurrence_count) {
			fprintf(stderr, "branch refinement constraint header invalid id=%u\n", id);
			return -1;
		}
		struct prototype_typed_occurrence* match =
			&ctx->metadata->typed_occurrences.occurrences[match_id];
		uint32_t operation_case_id = match->first_case + case_index;
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			case_index >= match->case_count || operation_case_id >=
				ctx->metadata->typed_occurrences.case_count ||
			match->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[match->core_term].tag != PROTOTYPE_TERM_MATCH) {
			fprintf(stderr,
				"branch refinement Match header invalid id=%u match=%u case=%u\n",
				id, match_id, case_index);
			return -1;
		}
		struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[operation_case_id];
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
			fprintf(stderr,
				"branch consider constraint=%u match=%u case=%u case-id=%u "
				"constructor=%u admitted=%d refinement=%d solution=%d\n",
				id, match_id, case_index, operation_case_id,
				operation_case->constructor_id,
				prototype_typed_occurrence_match_case_is_admitted(operation_case),
				operation_case->refinement_status,
				solution->state);
		}
		if (operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
			if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
				fprintf(stderr, "branch wait constraint=%u reason=constructor\n", id);
			}
			continue;
		}
		int accepted_seeded;
		if (operation_solver_seed_accepted_branch_refinement(
				ctx, constraint, operation_case, solution, &accepted_seeded
			) != 0) {
			return -1;
		}
		if (accepted_seeded) {
			continue;
		}
		int waits_for_enclosing =
			operation_solver_waits_for_enclosing_branch_refinement(
				ctx,
				constraint->payload.branch_refinement.branch_typed_projection,
				constraint->payload.branch_refinement.match_case_projection
			);
		if (waits_for_enclosing < 0) {
			return -1;
		}
		if (waits_for_enclosing > 0) {
			if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
				fprintf(stderr, "branch wait constraint=%u reason=enclosing\n", id);
			}
			continue;
		}
		uint32_t scrutinee_classifier =
			operation_solver_classifier_for_projection(
				ctx,
				constraint->payload.branch_refinement.scrutinee_typed_projection
			);
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
			if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
				fprintf(stderr, "branch wait constraint=%u reason=scrutinee-classifier\n", id);
			}
			continue;
		}
		uint32_t term_case_id =
			ctx->terms->terms[match->core_term].as.match.first_case + case_index;
		if (term_case_id >= ctx->terms->case_count) {
			fprintf(stderr, "branch refinement term case invalid id=%u term-case=%u\n",
				id, term_case_id);
			return -1;
		}
		const struct prototype_match_case* term_case =
			&ctx->terms->cases[term_case_id];
		struct prototype_case_binder occurrence_binders[64];
		if (term_case->binder_count > 64 || term_case->binder_count !=
			operation_case->binder_count) {
			fprintf(stderr,
				"branch refinement binder count invalid id=%u term=%u occurrence=%u\n",
				id, term_case->binder_count, operation_case->binder_count);
			return -1;
		}
		for (uint32_t binder_index = 0;
			binder_index < term_case->binder_count;
			++binder_index) {
			if (!prototype_context_contains_binding(
					&ctx->metadata->contexts,
					operation_case->context_id,
					operation_case->binder_ids[binder_index]
				)) {
				fprintf(stderr,
					"branch refinement binder scope invalid id=%u binder=%u context=%u\n",
					id, operation_case->binder_ids[binder_index],
					operation_case->context_id);
				return -1;
			}
			occurrence_binders[binder_index] = ctx->terms->case_binders[
				term_case->first_binder + binder_index
			];
			occurrence_binders[binder_index].binding_id =
				operation_case->binder_ids[binder_index];
		}
		uint32_t refined_context = PROTOTYPE_INVALID_ID;
		uint32_t substitution = PROTOTYPE_INVALID_ID;
		uint32_t constructor_term = PROTOTYPE_INVALID_ID;
		uint32_t residual_pattern = PROTOTYPE_INVALID_ID;
		uint32_t residual_value = PROTOTYPE_INVALID_ID;
		uint32_t match_projection_substitution;
		uint32_t match_typed_projection =
			constraint->payload.branch_refinement.match_typed_projection;
		int publishes_operation_case =
			match_id >= ctx->accepted_typing.occurrence_count &&
			ctx->typing->publication_typed_projection_for_occurrence[match_id] ==
				match_typed_projection;
		uint32_t match_case_projection_id =
			constraint->payload.branch_refinement.match_case_projection;
		uint32_t branch_typed_projection =
			constraint->payload.branch_refinement.branch_typed_projection;
		uint32_t materialized_match_context;
		uint32_t materialized_branch_context;
		const struct prototype_typed_match_case_projection* match_case_projection =
			prototype_typed_match_case_projection_get(
				&ctx->typing->context_projections,
				match_case_projection_id
			);
		if (!match_case_projection ||
			match_case_projection->source_case != operation_case_id ||
			match_case_projection->owner_typed_projection !=
				match_typed_projection) {
			fprintf(stderr,
				"branch refinement exact projection missing constraint=%u match=%u case=%u\n",
				id, match_id, operation_case_id);
			return -1;
		}
		int projection_status = operation_typed_projection_materialized_context(
			ctx,
			match_typed_projection,
			&materialized_match_context,
			&match_projection_substitution
		);
		if (projection_status > 0) {
			if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
				fprintf(stderr, "branch wait constraint=%u reason=match-projection\n", id);
			}
			continue;
		}
		if (projection_status < 0) {
			return -1;
		}
		projection_status = operation_context_projection_solve(
			ctx, match_case_projection->context_projection, 0
		);
		if (projection_status > 0) {
			if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
				fprintf(stderr, "branch wait constraint=%u reason=case-projection\n", id);
			}
			continue;
		}
		if (projection_status < 0) {
			return -1;
		}
		const struct prototype_context_projection_solution* case_input_solution =
			prototype_context_projection_solution_get(
				&ctx->typing->context_projections,
				match_case_projection->context_projection
			);
		if (!case_input_solution || case_input_solution->state !=
				PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
			return -1;
		}
		materialized_branch_context = case_input_solution->concrete_context;
		/* The classifier answer belongs to the candidate Match Context. Apply the
		 * materialized Context action before selecting its nominal family; an
		 * enclosing branch may reduce a neutral dependent Match in the classifier. */
		uint32_t materialized_scrutinee_classifier;
		uint32_t scrutinee_schema_classifier;
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				scrutinee_classifier,
				match_projection_substitution,
				&materialized_scrutinee_classifier
			) != 0) {
			return -1;
		}
		int schema_status = prototype_judgement_project_nominal_classifier_instance(
			ctx->terms,
			ctx->type_declarations,
			materialized_scrutinee_classifier,
			&scrutinee_schema_classifier
		);
		if (schema_status > 0) {
			if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
				fprintf(stderr,
					"branch wait constraint=%u reason=scrutinee-family\n", id);
			}
			continue;
		}
		if (schema_status < 0) {
			return -1;
		}
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE")) {
			const struct prototype_substitution* match_projection_action =
				prototype_substitution_get(
					&ctx->metadata->substitutions,
					match_projection_substitution
				);
			fprintf(stderr,
				"branch solve constraint=%u match=%u scrutinee=%u scrutinee-tag=%d "
				"scrutinee-core=%u scrutinee-binding=%u "
				"case=%u projections=%u/%u/%u candidate-match=%u "
				"materialized-match=%u candidate-branch=%u materialized-branch=%u "
				"match-action=%u projection-substitution=%u projection-source=%u "
				"projection-target=%u case-action=%u case-status=%d\n",
				constraint->id, match_id, scrutinee_id,
				ctx->metadata->typed_occurrences.occurrences[scrutinee_id].tag,
				ctx->metadata->typed_occurrences.occurrences[scrutinee_id].core_term,
				ctx->metadata->typed_occurrences.occurrences[scrutinee_id].binding_id,
				case_index, match_typed_projection,
				constraint->payload.branch_refinement.scrutinee_typed_projection,
				branch_typed_projection, match->context_id,
				materialized_match_context, operation_case->context_id,
				materialized_branch_context, match->context_action_substitution,
				match_projection_substitution,
				match_projection_action ? match_projection_action->source_context :
					PROTOTYPE_INVALID_ID,
				match_projection_action ? match_projection_action->target_context :
					PROTOTYPE_INVALID_ID,
				operation_case->refinement_substitution,
				operation_case->refinement_status);
		}
		if (materialized_match_context == PROTOTYPE_INVALID_ID ||
			materialized_branch_context == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		struct prototype_match_constructor_resolution schema_resolution;
		uint32_t schema_type;
		uint32_t owner_type;
		uint32_t ignored_arguments[32];
		uint32_t ignored_argument_count;
		if (prototype_judgement_resolve_match_constructor(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				scrutinee_schema_classifier,
				operation_case->case_label_symbol_id,
				&schema_resolution
			) != 0 ||
			schema_resolution.constructor_id != operation_case->constructor_id ||
			prototype_type_projection_instance_info(
				ctx->terms,
				&ctx->type_declarations->semantic_schema,
				schema_resolution.constructor_owner,
				&schema_type,
				ignored_arguments,
				&ignored_argument_count
			) != 0 ||
			prototype_type_projection_instance_info(
				ctx->terms,
				&ctx->type_declarations->semantic_schema,
				operation_case->constructor_owner,
				&owner_type,
				ignored_arguments,
				&ignored_argument_count
			) != 0 || schema_type != owner_type) {
			return -1;
		}
		const struct prototype_typed_occurrence* scrutinee_occurrence =
			&ctx->metadata->typed_occurrences.occurrences[scrutinee_id];
		uint32_t scrutinee_binding_id = scrutinee_occurrence->binding_id;
		if (scrutinee_binding_id == PROTOTYPE_INVALID_ID &&
			scrutinee_occurrence->core_term < ctx->terms->term_count &&
			ctx->terms->terms[scrutinee_occurrence->core_term].tag ==
				PROTOTYPE_TERM_VAR) {
			scrutinee_binding_id = ctx->terms->terms[
				scrutinee_occurrence->core_term
			].as.var.binding_id;
		}
		struct prototype_context_classifier_view context_view =
			operation_type_context_view(ctx);
		int status = prototype_judgement_source_match_branch_refinement_in_view(
			&context_view,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			materialized_match_context,
			scrutinee_occurrence->core_term,
			scrutinee_binding_id,
			scrutinee_schema_classifier,
			operation_case->constructor_id,
			materialized_branch_context,
			occurrence_binders,
			term_case->binder_count,
			&refined_context,
			&substitution,
			&constructor_term,
			&residual_pattern,
			&residual_value
		);
		int constant_motive = 0;
		int effective_status = status;
		if (status == PROTOTYPE_INDEX_REFINEMENT_RESIDUAL) {
			constant_motive = operation_solver_match_motive_is_constant(
				ctx,
				constraint->payload.branch_refinement.match_typed_projection
			);
			if (constant_motive < 0) {
				return -1;
			}
			if (constant_motive > 0) {
				effective_status = PROTOTYPE_INDEX_REFINEMENT_CONSTANT;
			}
		}
		int refinement_changed =
			solution->branch_refinement.refinement_status != effective_status ||
			solution->branch_refinement.refined_context != refined_context ||
			solution->branch_refinement.substitution != substitution;
		if (refinement_changed) {
			operation_solver_invalidate_reindex_cache(ctx);
		}
		solution->branch_refinement.materialized_source_context =
			materialized_branch_context;
		solution->branch_refinement.refinement_status = effective_status;
		solution->branch_refinement.refined_context = refined_context;
		solution->branch_refinement.substitution = substitution;
		solution->branch_refinement.constructor_term = constructor_term;
		solution->branch_refinement.residual_pattern = residual_pattern;
		solution->branch_refinement.residual_value = residual_value;
		if (refinement_changed && operation_solver_enqueue_equation_dependents(
				ctx, constraint->id
			) != 0) {
			return -1;
		}
		if (status == PROTOTYPE_INDEX_REFINEMENT_INVALID) {
			fprintf(stderr,
				"branch refinement invalid constraint=%u match=%u case=%u "
				"scrutinee=%u match-context=%u branch-context=%u "
				"scrutinee-classifier=%u schema-classifier=%u constructor=%u ast=%u\n",
				constraint->id, match_id, case_index, scrutinee_id,
				match->context_id, operation_case->context_id,
				scrutinee_classifier, scrutinee_schema_classifier,
				operation_case->constructor_id,
				match->source_ast);
			fprintf(stderr, "branch refinement scrutinee classifier: ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				scrutinee_classifier
			);
			fprintf(stderr, "\n");
			if (ctx->asts && match->source_ast < ctx->asts->node_count) {
				const struct prototype_ast_node* source =
					&ctx->asts->nodes[match->source_ast];
				fprintf(stderr,
					"branch refinement source tag=%d scrutinee-ast=%u cases=%u first-case=%u\n",
					source->tag,
					source->tag == PROTOTYPE_AST_MATCH ?
						source->as.match.scrutinee : PROTOTYPE_INVALID_ID,
					source->tag == PROTOTYPE_AST_MATCH ?
						source->as.match.case_count : 0,
					source->tag == PROTOTYPE_AST_MATCH ?
						source->as.match.first_case : PROTOTYPE_INVALID_ID);
				if (source->tag == PROTOTYPE_AST_MATCH &&
					source->as.match.first_case < ctx->asts->case_count) {
					const struct prototype_ast_match_case* source_case =
						&ctx->asts->cases[source->as.match.first_case];
					fprintf(stderr,
						"branch refinement source case constructor-symbol=%d binders=%u body=%u\n",
						source_case->constructor_symbol_id,
						source_case->binder_count, source_case->body);
				}
			}
			(void)prototype_typing_constraint_transition(
				&ctx->typing->constraints,
				constraint->id,
				OPERATION_CONSTRAINT_STATE_CONTRADICTION,
				OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_INVALID
			);
			return -1;
		}
		if (status == PROTOTYPE_INDEX_REFINEMENT_RESIDUAL) {
			if (constant_motive > 0) {
				if (publishes_operation_case) {
					operation_case->refinement_status =
						PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT;
					operation_case->refinement_substitution = PROTOTYPE_INVALID_ID;
				}
				solution->branch_refinement.refinement_status =
					PROTOTYPE_INDEX_REFINEMENT_CONSTANT;
				if (solution->state != OPERATION_CONSTRAINT_STATE_SOLVED ||
					solution->reason !=
						OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_CONSTANT) {
					if (prototype_typing_constraint_transition(
							&ctx->typing->constraints,
							constraint->id,
							OPERATION_CONSTRAINT_STATE_SOLVED,
							OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_CONSTANT
						) != 0) {
						return -1;
					}
				}
					if (refinement_changed &&
						operation_motive_reopen_after_branch_refinement(
							ctx,
							constraint->payload.branch_refinement.
								match_typed_projection
						) != 0) {
						return -1;
					}
					/* A constant motive discharges the refinement equation without
				 * changing an occurrence Context or projection. */
				continue;
			}
			if (publishes_operation_case) {
				operation_case->refinement_status =
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_RESIDUAL;
			}
			if (prototype_typing_constraint_transition(
					&ctx->typing->constraints,
					constraint->id,
					OPERATION_CONSTRAINT_STATE_RESIDUAL,
					OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_RESIDUAL
				) != 0) {
				return -1;
			}
			continue;
		}
		if (status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
			if (publishes_operation_case) {
				operation_case->refinement_status =
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE;
			}
			if (prototype_typing_constraint_transition(
					&ctx->typing->constraints,
					constraint->id,
					OPERATION_CONSTRAINT_STATE_SOLVED,
					OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_IMPOSSIBLE
				) != 0) {
				return -1;
			}
			continue;
		}
		if (status != PROTOTYPE_INDEX_REFINEMENT_SOLVED ||
			refined_context == PROTOTYPE_INVALID_ID ||
			substitution == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		const struct prototype_substitution* solved_action =
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution
			);
		if (!solved_action || solved_action->source_context != refined_context) {
			fprintf(stderr,
				"branch refinement substitution invalid constraint=%u action=%u "
				"source=%u refined=%u target=%u\n",
				constraint->id, substitution,
				solved_action ? solved_action->source_context : PROTOTYPE_INVALID_ID,
				refined_context,
				solved_action ? solved_action->target_context : PROTOTYPE_INVALID_ID);
			return -1;
		}
		if (prototype_typing_constraint_transition(
				&ctx->typing->constraints,
				constraint->id,
				OPERATION_CONSTRAINT_STATE_SOLVED,
				OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_VALIDATED
			) != 0) {
			fprintf(stderr,
				"branch refinement transition invalid constraint=%u state=%d\n",
				constraint->id,
				ctx->typing->constraints.solutions[constraint->id].state);
			return -1;
		}
		uint32_t projected_context;
		uint32_t projected_action;
		int branch_projection_status =
			operation_typed_projection_materialized_context(
				ctx,
				branch_typed_projection,
				&projected_context,
				&projected_action
			);
		if (branch_projection_status > 0) {
			*p_projection_waiting = 1;
			continue;
		}
		if (branch_projection_status < 0 ||
			projected_context != refined_context) {
			fprintf(stderr,
				"branch projection result invalid constraint=%u projection=%u "
				"status=%d projected-context=%u refined-context=%u action=%u\n",
				constraint->id, branch_typed_projection,
				branch_projection_status, projected_context, refined_context,
				projected_action);
			return -1;
		}
		(void)projected_action;
		if (refinement_changed &&
			operation_motive_reopen_after_branch_refinement(
				ctx, match_typed_projection
			) != 0) {
			return -1;
		}
		uint32_t branch_classifier_equation = ctx->typing->constraints.
			classifier_constraint_for_projection[branch_typed_projection];
		if (!prototype_typing_constraint_has_domain(
				&ctx->typing->constraints, branch_classifier_equation,
				OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
			)) {
			return -1;
		}
		/* The refinement pass scans its immutable equation domain after every
		 * classifier worklist drain. A scan is not a semantic event: wake the
		 * branch equation and its consumers only when the canonical refinement
		 * answer (Context action) changes. Unconditional wake-up here makes a
		 * solved recursive Match rediscover the same Pi/computation equations
		 * forever. */
		if (refinement_changed &&
			(operation_solver_enqueue_constraint(
				ctx,
				branch_classifier_equation,
				PROTOTYPE_COMPILE_ENQUEUE_DEPENDENCY
			) != 0 || operation_solver_enqueue_projection_dependents(
				ctx, branch_typed_projection
			) != 0)) {
			return -1;
		}
		if (publishes_operation_case) {
			operation_case->refinement_status =
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED;
			operation_case->refinement_substitution = substitution;
		}
	}
	return 0;
}

static int operation_solver_join_match_classifiers(
	struct compile_context* ctx,
	uint32_t left,
	uint32_t right,
	uint32_t* p_joined
);

static int operation_solver_validate_guarded_motive_occurrence(
	struct compile_context* ctx,
	uint32_t ih_operation,
	uint32_t motive_operation,
	uint32_t argument_occurrence
);

/* Solve a dependent Match equation from the branch Context morphisms. This
 * is the graph-level counterpart of anti-unifying branch classifiers: every
 * branch must quote to one source term, and the resulting motive must replay
 * to every independently synthesized branch classifier. */
static int operation_usage_solution_view(
	const struct compile_context* ctx,
	uint32_t operation_id,
	struct prototype_usage_vector* p_vector
) {
	if (!ctx || !ctx->metadata || !p_vector ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	return prototype_occurrence_usage_solution_view(
		ctx->usage_workspace.solutions,
		ctx->metadata->typed_occurrences.occurrence_count,
		ctx->usage_workspace.entries,
		ctx->usage_workspace.entry_count,
		operation_id,
		p_vector
	);
}

static int operation_solver_proposition_usage(
	void* context,
	uint32_t operation_id,
	const struct prototype_usage_entry** p_entries,
	uint32_t* p_count
) {
	struct compile_context* ctx = context;
	struct prototype_usage_vector usage;
	if (!ctx || !p_entries || !p_count ||
		operation_usage_solution_view(ctx, operation_id, &usage) != 0 ||
		usage.count > UINT32_MAX) {
		return -1;
	}
	*p_entries = usage.entries;
	*p_count = (uint32_t)usage.count;
	return 0;
}

static int operation_solver_project_usage_solution(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t substitution_id,
	uint32_t materialized_context
) {
	const struct prototype_substitution* substitution = prototype_substitution_get(
		&ctx->metadata->substitutions, substitution_id
	);
	if (!substitution) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY) {
		return 0;
	}
	struct prototype_occurrence_usage_solution* solution =
		&ctx->usage_workspace.solutions[operation_id];
	if (solution->first_entry > ctx->usage_workspace.entry_count ||
		solution->entry_count > ctx->usage_workspace.entry_count -
			solution->first_entry) {
		return -1;
	}
	struct prototype_usage_entry projected_entries[PROTOTYPE_CONTEXT_CAPACITY];
	struct prototype_usage_vector projected;
	prototype_usage_vector_init(
		&projected, projected_entries, PROTOTYPE_CONTEXT_CAPACITY
	);
	struct prototype_usage_vector source = {
		.entries = &ctx->usage_workspace.entries[solution->first_entry],
		.count = solution->entry_count,
		.capacity = solution->entry_count
	};
	if (prototype_usage_vector_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			&source,
			substitution_id,
			&projected
		) != 0) {
		return -1;
	}
	if (projected.count > PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY -
			ctx->usage_workspace.entry_count) {
		return -1;
	}
	uint32_t first_entry = ctx->usage_workspace.entry_count;
	for (size_t i = 0; i < projected.count; ++i) {
		if (!prototype_context_contains_binding(
				&ctx->metadata->contexts,
				materialized_context,
				projected.entries[i].binding_id
			)) {
			return -1;
		}
		ctx->usage_workspace.entries[
			ctx->usage_workspace.entry_count++
		] = projected.entries[i];
	}
	solution->first_entry = first_entry;
	solution->entry_count = (uint32_t)projected.count;
	return 0;
}


static int operation_solver_refresh_usage_solutions(struct compile_context* ctx) {
	if (!ctx || !ctx->typing || !ctx->metadata ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* operations =
		prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
	size_t entry_count;
	if (prototype_occurrence_usage_solve(
			operations,
			ctx->terms,
			&ctx->metadata->contexts,
			ctx->usage_workspace.solutions,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY,
			ctx->usage_workspace.entries,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY,
			&entry_count
		) != 0 || entry_count > UINT32_MAX) {
		return -1;
	}
	ctx->usage_workspace.entry_count = (uint32_t)entry_count;
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		uint32_t materialized_context;
		uint32_t substitution;
		int projection_status = operation_occurrence_materialized_context_status(
			ctx, operation_id, &materialized_context, &substitution
		);
		if (projection_status > 0) {
			continue;
		}
		if (projection_status < 0 || substitution == PROTOTYPE_INVALID_ID ||
			operation_solver_project_usage_solution(
				ctx, operation_id, substitution, materialized_context
			) != 0) {
			return -1;
		}
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_occurrence_usage_solution* source =
			&ctx->usage_workspace.solutions[operation_id];
		for (uint32_t usage_index = 0;
			usage_index < source->entry_count;
			++usage_index) {
			const struct prototype_usage_entry* entry =
				&ctx->usage_workspace.entries[
					source->first_entry + usage_index
				];
			if (!prototype_context_contains_binding(
					&ctx->metadata->contexts,
					ctx->metadata->typed_occurrences.occurrences[operation_id].context_id,
					entry->binding_id
				)) {
				const struct prototype_typed_occurrence* operation =
					&ctx->metadata->typed_occurrences.occurrences[operation_id];
				fprintf(stderr, "operation usage escapes context operation=%u tag=%d context=%u binding=%u core=%u first-edge=%u edge-count=%u wrapper=%u ih-owner=%u\n",
					operation_id, operation->tag, operation->context_id,
					entry->binding_id, operation->core_term,
					operation->first_edge, operation->edge_count,
					operation->wrapped_occurrence,
					operation->ih_owner_occurrence);
				if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
					uint32_t debug_context = operation->context_id;
					while (debug_context != prototype_context_empty(
						&ctx->metadata->contexts
					)) {
						const struct prototype_context* debug_entry =
							prototype_context_get(
								&ctx->metadata->contexts, debug_context
							);
						if (!debug_entry) {
							break;
						}
						fprintf(stderr, "operation usage context entry=%u binding=%u parent=%u\n",
							debug_context, debug_entry->binding_id,
							debug_entry->parent);
						debug_context = debug_entry->parent;
					}
					for (uint32_t case_index = 0;
						case_index < operation->case_count;
						++case_index) {
						const struct prototype_typed_occurrence_match_case* debug_case =
							&ctx->metadata->typed_occurrences.cases[
								operation->first_case + case_index
							];
						uint32_t debug_body = PROTOTYPE_INVALID_ID;
						(void)compile_occurrence_child(
							ctx, operation_id,
							PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, case_index,
							&debug_body
						);
						fprintf(stderr, "operation usage case=%u body=%u context=%u binders=%u ast0=%u\n",
							case_index, debug_body,
							debug_case->context_id, debug_case->binder_count,
							debug_case->binder_count ? debug_case->ast_binder_ids[0] :
							PROTOTYPE_INVALID_ID);
					}
					for (uint32_t target_id = 0;
						target_id < ctx->metadata->typed_occurrences.occurrence_count;
						++target_id) {
						const struct prototype_typed_occurrence* target_operation =
							&ctx->metadata->typed_occurrences.occurrences[target_id];
						if (target_operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
							target_operation->binding_id == entry->binding_id) {
							fprintf(stderr, "operation usage binding occurrence operation=%u ast=%u context=%u core=%u\n",
								target_id,
								target_operation->referenced_ast_binder_id,
								target_operation->context_id,
								target_operation->core_term);
						}
					}
				}
				return -1;
			}
		}
	}
	return 0;
}

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
) {
	if (!ctx || !ctx->metadata || !visited ||
		root_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		target_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	if (root_operation == target_operation) {
		return 1;
	}
	if (visited[root_operation]) {
		return 0;
	}
	visited[root_operation] = 1;
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[root_operation];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		if (operation->wrapped_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		int status = operation_subtree_contains_operation(
			ctx, operation->wrapped_occurrence, target_operation, visited
		);
		if (status != 0) {
			return status;
		}
	}
	if (operation->first_edge == PROTOTYPE_INVALID_ID) {
		return operation->edge_count == 0 ? 0 : -1;
	}
	if (operation->first_edge > ctx->metadata->typed_occurrences.edge_count ||
		operation->edge_count > ctx->metadata->typed_occurrences.edge_count -
			operation->first_edge) {
		return -1;
	}
	for (uint32_t i = 0; i < operation->edge_count; ++i) {
		const struct prototype_typed_occurrence_edge* edge =
			&ctx->metadata->typed_occurrences.edges[operation->first_edge + i];
		if (edge->child_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		int status = operation_subtree_contains_operation(
			ctx, edge->child_occurrence, target_operation, visited
		);
		if (status != 0) {
			return status;
		}
	}
	return 0;
}

static int operation_subtree_contains_external_ref(
	const struct compile_context* ctx,
	uint32_t root_operation
) {
	if (!ctx || !ctx->metadata || !ctx->terms ||
		root_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t target = 0;
		target < ctx->metadata->typed_occurrences.occurrence_count;
		++target) {
		uint32_t core_term =
			ctx->metadata->typed_occurrences.occurrences[target].core_term;
		if (core_term >= ctx->terms->term_count ||
			ctx->terms->terms[core_term].tag != PROTOTYPE_TERM_EXTERNAL_REF) {
			continue;
		}
		uint8_t visited[ctx->metadata->typed_occurrences.occurrence_count];
		memset(visited, 0, sizeof(visited));
		int contains = operation_subtree_contains_operation(
			ctx, root_operation, target, visited
		);
		if (contains != 0) {
			return contains;
		}
	}
	return 0;
}

struct operation_classifier_capture_query {
	const struct compile_context* ctx;
	const struct prototype_typed_occurrence* match;
	uint32_t match_occurrence;
	const struct prototype_typed_occurrence_match_case* operation_case;
	const struct prototype_match_case* core_case;
};

static int operation_classifier_binding_is_captured(
	void* context,
	uint32_t binding_id
) {
	const struct operation_classifier_capture_query* query = context;
	if (!query || !query->ctx || !query->match || !query->operation_case ||
		!query->core_case) {
		return -1;
	}
	for (uint32_t i = 0; i < query->core_case->binder_count; ++i) {
		uint32_t canonical_binding = query->ctx->terms->case_binders[
			query->core_case->first_binder + i
		].binding_id;
		if (binding_id == canonical_binding ||
			binding_id == query->operation_case->binder_ids[i]) {
			return 1;
		}
	}
	if (prototype_context_contains_binding(
			&query->ctx->metadata->contexts,
			query->match->context_id,
			binding_id
		)) {
		return 0;
	}
	int effect_owned = operation_effect_row_binder_is_owned(
		query->ctx, query->match_occurrence, binding_id
	);
	return effect_owned < 0 ? -1 : !effect_owned;
}

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct prototype_typing_constraint* equation,
	uint32_t classifier,
	uint32_t* p_classifier_whnf
) {
	if (!ctx || !equation || classifier >= ctx->terms->term_count ||
		!p_classifier_whnf ||
		equation->source_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t classifier_whnf;
	if (typing_core_normalize_complete(
			ctx->core_requests,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			classifier,
			&classifier_whnf
		) != 0) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[equation->source_occurrence];
	if (match->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match->core_term].tag != PROTOTYPE_TERM_MATCH ||
		equation->payload.classifier.goal.motive_case.case_index >= match->case_count ||
		match->first_case + equation->payload.classifier.goal.motive_case.case_index >=
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	const struct prototype_term* core_match = &ctx->terms->terms[match->core_term];
	uint32_t core_case_id = core_match->as.match.first_case +
		equation->payload.classifier.goal.motive_case.case_index;
	if (core_case_id >= ctx->terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* core_case =
		&ctx->terms->cases[core_case_id];
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[
			match->first_case + equation->payload.classifier.goal.motive_case.case_index
		];
	if (core_case->binder_count != operation_case->binder_count) {
		return -1;
	}
	struct operation_classifier_capture_query query = {
		.ctx = ctx,
		.match = match,
		.match_occurrence = equation->source_occurrence,
		.operation_case = operation_case,
		.core_case = core_case
	};
	int captures;
	if (prototype_term_any_free_binding(
			ctx->terms,
			classifier_whnf,
			operation_classifier_binding_is_captured,
			&query,
			&captures
		) != 0) {
		return -1;
	}
	if (captures) {
		return 1;
	}
	*p_classifier_whnf = classifier_whnf;
	return 0;
}

/* Match result equations and effect equations are distinct. A returning
 * computation contributes its result carrier to the motive and its row to the
 * Match join. Other classifiers contribute themselves to the motive. */
static int operation_solver_match_classifier_parts(
	struct compile_context* ctx,
	uint32_t classifier,
	int* p_returns_computation,
	uint32_t* p_result,
	uint32_t* p_effect_row,
	int* p_totality
) {
	if (!ctx || !p_returns_computation || !p_result || !p_effect_row ||
		!p_totality ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, classifier, &view
		) != 0) {
		return -1;
	}
	if (view.category == PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
		view.computation_kind == PROTOTYPE_COMPUTATION_KIND_RETURNING) {
		*p_returns_computation = 1;
		*p_result = view.result;
		*p_effect_row = view.effect_row;
		*p_totality = view.totality;
		return 0;
	}
	*p_returns_computation = 0;
	*p_result = classifier;
	*p_effect_row = PROTOTYPE_INVALID_ID;
	*p_totality = PROTOTYPE_COMPUTATION_TOTALITY_TOTAL;
	return 0;
}

static int operation_solver_join_match_classifiers(
	struct compile_context* ctx,
	uint32_t left,
	uint32_t right,
	uint32_t* p_joined
) {
	int left_returns;
	int right_returns;
	uint32_t left_result;
	uint32_t right_result;
	uint32_t left_row;
	uint32_t right_row;
	int left_totality;
	int right_totality;
	if (!ctx || !p_joined || operation_solver_match_classifier_parts(
			ctx, left, &left_returns, &left_result, &left_row, &left_totality
		) != 0 || operation_solver_match_classifier_parts(
			ctx, right, &right_returns, &right_result, &right_row, &right_totality
		) != 0 || left_returns != right_returns ||
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, left_result, right_result
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	if (!left_returns) {
		*p_joined = left;
		return 0;
	}
	uint32_t joined_row;
	int joined_totality = prototype_classifier_totality_join(
		left_totality, right_totality
	);
	if (joined_totality == PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN) {
		return 1;
	}
	return prototype_term_effect_row_union(
			ctx->terms, left_row, right_row, &joined_row
		) != 0 ? -1 : prototype_term_computation_type(
			ctx->terms, joined_row, left_result, joined_totality, p_joined
		);
}

/*
 * A uniform motive is represented as a constant lambda. This is not a Match
 * typing shortcut: APP(\_ => T, scrutinee) reduces by beta to T. It is valid
 * only when no branch classifier mentions that branch's pattern binders.
 */
static int operation_match_typing_view(
	const struct compile_context* ctx,
	uint32_t operation,
	struct prototype_typing_match_goal* p_typing
) {
	if (!ctx || !ctx->metadata || !p_typing || operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* occurrence =
		&ctx->metadata->typed_occurrences.occurrences[operation];
	if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		occurrence->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[occurrence->core_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->typing->seed.match_goal_count; ++i) {
		if (ctx->typing->seed.match_goals[i].operation == operation) {
			*p_typing = ctx->typing->seed.match_goals[i];
			/* Constructor resolution may derive a rebuilt Core Match. The seed
			 * retains the unresolved source endpoint; this read-only view selects
			 * the current derived endpoint without mutating replay input. */
			p_typing->match_term = occurrence->core_term;
			return 0;
		}
	}
	*p_typing = (struct prototype_typing_match_goal) {
		.match_term = occurrence->core_term,
		.operation = operation,
		.universe_level_var = PROTOTYPE_INVALID_ID
	};
	return 0;
}

/*
 * Solve the guarded equation M(C(..., rest, ...)) = M(rest) without first
 * inventing a TermDB classifier for M(rest). The resulting motive carries its
 * own structurally-recursive match frame, so the recursive equation is a
 * finite graph rather than a cyclic solver substitution.
 */
static int operation_solver_validate_guarded_motive_occurrence(
	struct compile_context* ctx,
	uint32_t ih_operation,
	uint32_t motive_operation,
	uint32_t argument_occurrence
) {
	if (!ctx || !ctx->metadata ||
		ih_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		motive_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		argument_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph;
	struct prototype_typed_occurrence_induction_edge edge;
	graph = prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
	int edge_status = prototype_typed_occurrence_graph_induction_edge(
			graph, ctx->terms, ih_operation, &edge
		);
	if (edge_status != 0 || edge.owner_match_occurrence != motive_operation ||
		edge.argument_occurrence != argument_occurrence) {
		fprintf(stderr,
			"IH ownership mismatch operation=%u edge-status=%d "
			"edge-owner=%u expected-owner=%u edge-argument=%u expected-argument=%u\n",
			ih_operation, edge_status,
			edge_status == 0 ? edge.owner_match_occurrence : PROTOTYPE_INVALID_ID,
			motive_operation,
			edge_status == 0 ? edge.argument_occurrence : PROTOTYPE_INVALID_ID,
			argument_occurrence);
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[ih_operation];
		uint32_t constraint_id =
			operation_solver_active_ih_equation_for_occurrence(
				ctx, ih_operation
			);
		(void)add_compile_diagnostic(
			ctx,
			PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
			PROTOTYPE_COMPILE_DIAGNOSTIC_IH_OWNERSHIP,
			operation->source_ast,
			ih_operation,
			constraint_id,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			compile_source_span(ctx, operation->source_ast)
		);
		return -1;
	}
	return 0;
}

static int operation_motive_case_is_impossible(
	const struct compile_context* ctx,
	const struct prototype_typed_occurrence* operation,
	uint32_t case_index
) {
	if (!ctx || !ctx->metadata || !operation || case_index >=
			operation->case_count || operation->first_case + case_index >=
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	return ctx->metadata->typed_occurrences.cases[
		operation->first_case + case_index
	].refinement_status ==
		PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE;
}

/* Read a classifier consequence from an exact Layer T projection without
 * publishing it as the projection's accepted answer. This breaks the guarded
 * recursion dependency only through already-recorded motive equations: final
 * classifier publication still waits for every branch and IH premise. */
static int operation_solver_provisional_seed_classifier(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t depth,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier || depth > 64 || typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	uint32_t classifier = operation_solver_classifier_for_projection(
		ctx, typed_projection
	);
	if (classifier < ctx->terms->term_count) {
		*p_classifier = classifier;
		return 0;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	if (!projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		const struct prototype_typing_motive_solution* motive_solution =
			&ctx->typing->constraints.motive_solution_for_projection[
				typed_projection
			];
		uint32_t scrutinee_projection;
		uint32_t scrutinee_classifier;
		if (motive_solution->motive >= ctx->terms->term_count ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
			operation_solver_projected_child(
				ctx, typed_projection, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
				&scrutinee_projection
			) != 0 || (scrutinee_classifier =
				operation_solver_classifier_for_projection(
					ctx, scrutinee_projection
				)) >= ctx->terms->term_count) {
			return 1;
		}
		return prototype_judgement_apply_indexed_motive(
			ctx->terms, ctx->type_declarations, motive_solution->motive,
			ctx->terms->terms[operation->core_term].as.match.scrutinee,
			scrutinee_classifier, 0, p_classifier
		) == 0 ? 0 : 1;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
		uint32_t body_projection;
		uint32_t body_classifier;
		uint32_t domain;
		uint32_t codomain_family;
		if (operation_solver_projected_child(
				ctx, typed_projection, PROTOTYPE_TERM_CHILD_BODY, 0,
				&body_projection
			) != 0) {
			return -1;
		}
		int body_status = operation_solver_provisional_seed_classifier(
			ctx, body_projection, depth + 1, &body_classifier
		);
		int domain_status = operation_solver_lambda_projection_binder_classifier(
			ctx, typed_projection, &domain
		);
		if (body_status < 0 || domain_status < 0) {
			return -1;
		}
		if (body_status > 0 || domain_status > 0) {
			return 1;
		}
		if (prototype_term_pure_family(
				ctx->terms, operation->binding_id, body_classifier,
				&codomain_family
			) != 0 || prototype_term_pi_family(
				ctx->terms, domain, codomain_family, p_classifier
			) != 0) {
			return -1;
		}
		return 0;
	}
	return 1;
}

static int operation_solver_nonrecursive_seed_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t match_typed_projection,
	const struct prototype_typed_occurrence* operation,
	uint32_t* p_classifier,
	uint32_t* p_source_body_occurrence
) {
	if (!ctx || !operation || !p_classifier || !p_source_body_occurrence ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count || operation->case_count == 0 ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	uint32_t candidates[2] = {
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID
	};
	uint32_t source_body_occurrences[2] = {
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID
	};
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		int impossible = operation_motive_case_is_impossible(
			ctx, operation, case_index
		);
		if (impossible < 0) {
			return -1;
		}
		if (impossible > 0) {
			continue;
		}
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
			);
		if (!equation || equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		/* Keep acyclic answers and cyclic structural fallbacks in separate
		 * buckets. A real non-cyclic premise always wins; combining it with a
		 * provisional answer from the cycle would make the bootstrap depend on
		 * worklist order. */
		uint32_t bucket = ctx->typing->constraints.branch_ih_dependency_count[
			equation->id
		] == 0 ? 0 : 1;
		uint32_t branch_classifier = bucket == 0 && equation->operand_count == 2 ?
			operation_solver_classifier_for_projection(
				ctx, operation_solver_constraint_operand_projection(ctx, equation, 0)
			) : PROTOTYPE_INVALID_ID;
		int classifier_is_provisional = 0;
		if (branch_classifier == PROTOTYPE_INVALID_ID &&
			equation->operand_count == 2) {
			int provisional_status = operation_solver_provisional_seed_classifier(
				ctx, operation_solver_constraint_operand_projection(ctx, equation, 0), 0,
				&branch_classifier
			);
			if (provisional_status < 0) {
				return -1;
			}
			classifier_is_provisional = provisional_status == 0;
		}
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			int provisional_status =
				operation_solver_constructor_provisional_classifier(
					ctx, equation->payload.classifier.goal.motive_case.branch_operation,
					&branch_classifier
				);
			if (provisional_status < 0) {
				return -1;
			}
			classifier_is_provisional = provisional_status == 0;
		}
		if (branch_classifier == PROTOTYPE_INVALID_ID ||
			(bucket == 1 && !classifier_is_provisional)) {
			continue;
		}
		if (candidates[bucket] == PROTOTYPE_INVALID_ID) {
			candidates[bucket] = branch_classifier;
			source_body_occurrences[bucket] = equation->payload.classifier.goal.
				motive_case.branch_operation;
		} else {
			uint32_t joined;
			int join_status = operation_solver_join_match_classifiers(
				ctx, candidates[bucket], branch_classifier, &joined
			);
			if (join_status != 0) {
				return join_status;
			}
			candidates[bucket] = joined;
		}
	}
	uint32_t selected_bucket = candidates[0] != PROTOTYPE_INVALID_ID ? 0 : 1;
	if (candidates[selected_bucket] == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = candidates[selected_bucket];
	*p_source_body_occurrence = source_body_occurrences[selected_bucket];
	return 0;
}

static int operation_solver_has_guarded_recursive_equation(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t match_typed_projection
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
		int impossible = operation_motive_case_is_impossible(
			ctx, match, case_index
		);
		if (impossible < 0) {
			return -1;
		}
		if (impossible > 0) {
			continue;
		}
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
			);
		if (!equation) {
			fprintf(stderr,
				"match solve: missing motive equation match=%u case=%u state=%u constraints=%u\n",
				operation_id, case_index,
				ctx->typing->constraints.motive_solution_for_projection[
					match_typed_projection
				].phase,
				prototype_typing_constraint_count_in_domain(
					&ctx->typing->constraints,
					OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
				));
			return -1;
		}
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE")) {
			fprintf(
				stderr,
				"guarded-equation match=%u projection=%u case=%u equation=%u "
				"ih-dependencies=%u\n",
				operation_id, match_typed_projection, case_index, equation->id,
				ctx->typing->constraints.branch_ih_dependency_count[equation->id]
			);
		}
		/* Recursive use is a property of the complete projected branch, not of
		 * its root syntax.  In particular, Acc places the IH below Lambda and
		 * COMPUTATION_FOLD.  The immutable branch dependency slice records the
		 * owner-checked back-edge and is also the source used by SCC formation. */
		if (ctx->typing->constraints.branch_ih_dependency_count[
				equation->id
			] != 0) {
			return 1;
		}
	}
	return 0;
}

/*
 * Match solving records only an equation solution state. It deliberately does
 * not build a motive or an IH classifier in TermDB: an unresolved ?M is a
 * solver object, not a graph node. Materialization happens after propagation
 * reaches a fixed point.
 */
static int operation_solver_solve_match(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t match_typed_projection,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		match_typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	struct prototype_typing_match_goal typing;
	if (operation_match_typing_view(ctx, operation_id, &typing) != 0 ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	struct prototype_typing_motive_solution* motive_solution =
		&ctx->typing->constraints.motive_solution_for_projection[
			match_typed_projection
		];
	if (motive_solution->phase ==
			OPERATION_MOTIVE_PHASE_MATERIALIZED) {
		return 0;
	}
	if (operation_motive_mark_waiting(ctx, match_typed_projection) != 0) {
		fprintf(stderr, "match solve: waiting transition failed match=%u\n",
			operation_id);
		return -1;
	}
	int has_unresolved_body = 0;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		int impossible = operation_motive_case_is_impossible(
			ctx, operation, case_index
		);
		if (impossible < 0) {
			return -1;
		}
		if (impossible > 0) {
			continue;
		}
		const struct prototype_typing_constraint* equation =
			operation_solver_motive_equation_for_projection(
				ctx, match_typed_projection, case_index
			);
		if (!equation) {
			fprintf(stderr,
				"match solve: exact motive equation missing match=%u case=%u\n",
				operation_id, case_index);
			return -1;
		}
		uint32_t body_occurrence = equation->payload.classifier.goal.motive_case.branch_operation;
		if (body_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
			equation->operand_count != 2 ||
			operation_solver_classifier_for_projection(
				ctx, operation_solver_constraint_operand_projection(ctx, equation, 0)
			) ==
				PROTOTYPE_INVALID_ID) {
			has_unresolved_body = 1;
		}
	}
	if (!has_unresolved_body) {
		int ready_status = operation_motive_mark_equations_ready(
			ctx, match_typed_projection
		);
		if (ready_status != 0) {
			fprintf(stderr,
				"match solve: equation-ready transition failed match=%u\n",
				operation_id);
		}
		return ready_status;
	}
	int guarded_status = operation_solver_has_guarded_recursive_equation(
		ctx, operation_id, match_typed_projection
	);
	if (guarded_status < 0) {
		fprintf(stderr, "match solve: guarded equation inspection failed match=%u\n",
			operation_id);
		return -1;
	}
	if (guarded_status == 0) {
		return 0;
	}
	/* A guarded recursive-use equation is stronger than the one-branch bootstrap:
	 * it determines the indexed abstraction itself. Do not replace that READY
	 * solution with a PROVISIONAL constant candidate while the fixed point makes
	 * another pass over the same Match. */
	if (motive_solution->phase ==
			OPERATION_MOTIVE_PHASE_EQUATION_SET &&
		motive_solution->motive < ctx->terms->term_count) {
		return 0;
	}
	/*
	 * A non-recursive branch can constrain a constant motive before the
	 * recursive branch is classified.  This is the normal constraint-solving
	 * path for CBPV lowering: the recursive occurrence may be below RETURN,
	 * THUNK, or COMPUTATION_FOLD, so it must receive M(rest) through the solver before its
	 * enclosing computation can be classified.  Do this before considering the
	 * old direct-IH fallback, which only understands an IH as the immediate
	 * branch operation.
	 */
	uint32_t seed_classifier;
	uint32_t seed_source_body_occurrence;
	int seed_status = operation_solver_nonrecursive_seed_classifier(
		ctx, operation_id, match_typed_projection, operation,
		&seed_classifier, &seed_source_body_occurrence
	);
	if (getenv("A_PROGRAM_CONSTRUCTOR_PROVISIONAL_TRACE")) {
		fprintf(stderr,
			"match-provisional-seed operation=%u projection=%u status=%d classifier=%u source=%u\n",
			operation_id, match_typed_projection, seed_status,
			seed_status == 0 ? seed_classifier : PROTOTYPE_INVALID_ID,
			seed_status == 0 ? seed_source_body_occurrence : PROTOTYPE_INVALID_ID);
	}
	if (seed_status < 0) {
		fprintf(stderr, "match solve: nonrecursive seed failed match=%u\n",
			operation_id);
		return -1;
	}
	if (seed_status == 0) {
		seed_status = operation_solver_seed_motive(
			ctx, operation_id, match_typed_projection, seed_classifier,
			seed_source_body_occurrence, p_changed
		);
		if (getenv("A_PROGRAM_CONSTRUCTOR_PROVISIONAL_TRACE")) {
			fprintf(stderr,
				"match-provisional-motive operation=%u projection=%u status=%d phase=%u motive=%u\n",
				operation_id, match_typed_projection, seed_status,
				ctx->typing->constraints.motive_solution_for_projection[
					match_typed_projection
				].phase,
				ctx->typing->constraints.motive_solution_for_projection[
					match_typed_projection
				].motive);
		}
		if (seed_status < 0) {
			fprintf(stderr,
				"match solve: motive seed failed match=%u classifier=%u source=%u\n",
				operation_id, seed_classifier, seed_source_body_occurrence);
		}
		if (seed_status < 0) {
			return -1;
		}
		if (seed_status == 0) {
			return 0;
		}
		/* A branch seed can be temporarily unavailable while constructor
		 * refinement is still pending. That is not evidence of a recursive
		 * motive equation. Only an active IH back-edge below may move this
		 * solution into the guarded-recursive phase. */
	}
	return operation_motive_mark_guarded_recursive(
		ctx, match_typed_projection
	);
}
