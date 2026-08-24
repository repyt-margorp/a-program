static int operation_solver_reindex_in_enclosing_indexed_branches(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t term,
	uint32_t* p_reindexed
);

static int operation_solver_solve_branch_refinement_constraints(
	struct compile_context* ctx,
	int* p_graph_changed
);

static int operation_solver_rebuild_refined_match_motives(
	struct compile_context* ctx
);

static int operation_solver_occurrence_waits_for_branch_action(
	const struct compile_context* ctx,
	uint32_t occurrence_id
);

static struct operation_constraint* operation_solver_branch_refinement_constraint(
	struct compile_context* ctx,
	uint32_t operation_case_id
);

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static int operation_constraint_transition(
	struct operation_constraint* constraint,
	int state,
	int reason
) {
	if (!constraint ||
		state < OPERATION_CONSTRAINT_STATE_PENDING ||
		state > OPERATION_CONSTRAINT_STATE_CONTRADICTION ||
		(constraint->state != OPERATION_CONSTRAINT_STATE_PENDING &&
		 constraint->state != state)) {
		return -1;
	}
	constraint->state = state;
	constraint->reason = reason;
	return 0;
}

static int operation_constraint_reopen(
	struct operation_constraint* constraint,
	int reason
) {
	if (!constraint) {
		return -1;
	}
	constraint->state = OPERATION_CONSTRAINT_STATE_PENDING;
	constraint->reason = reason;
	constraint->evidence_id = PROTOTYPE_INVALID_ID;
	return 0;
}

static int operation_constraint_db_validate(
	const struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata ||
		ctx->constraints.constraint_count >
			PROTOTYPE_COMPILER_CONSTRAINT_CAPACITY ||
		ctx->constraints.classifier_constraint_count >
			ctx->constraints.constraint_count ||
		ctx->constraints.first_branch_refinement_constraint !=
			ctx->constraints.classifier_constraint_count ||
		ctx->constraints.first_computation_constraint !=
			ctx->constraints.first_branch_refinement_constraint +
				ctx->constraints.branch_refinement_constraint_count ||
		ctx->constraints.first_effect_constraint !=
			ctx->constraints.first_computation_constraint +
				ctx->constraints.computation_constraint_count ||
		ctx->constraints.constraint_count !=
			ctx->constraints.first_effect_constraint +
				ctx->constraints.effect_constraint_count) {
		fprintf(
			stderr,
			"constraint db layout invalid total=%u classifier=%u first-branch=%u branch=%u first-computation=%u computation=%u first-effect=%u effect=%u\n",
			ctx ? ctx->constraints.constraint_count : 0,
			ctx ? ctx->constraints.classifier_constraint_count : 0,
			ctx ? ctx->constraints.first_branch_refinement_constraint : 0,
			ctx ? ctx->constraints.branch_refinement_constraint_count : 0,
			ctx ? ctx->constraints.first_computation_constraint : 0,
			ctx ? ctx->constraints.computation_constraint_count : 0,
			ctx ? ctx->constraints.first_effect_constraint : 0,
			ctx ? ctx->constraints.effect_constraint_count : 0
		);
		return -1;
	}
	for (uint32_t id = 0; id < ctx->constraints.constraint_count; ++id) {
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->id != id ||
			constraint->state < OPERATION_CONSTRAINT_STATE_PENDING ||
			constraint->state > OPERATION_CONSTRAINT_STATE_CONTRADICTION ||
			constraint->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count ||
			constraint->context_id >= ctx->metadata->contexts.context_count) {
			fprintf(stderr, "constraint db header invalid id=%u domain=%d state=%d occurrence=%u context=%u\n",
				id, constraint->domain, constraint->state,
				constraint->source_occurrence, constraint->context_id);
			return -1;
		}
		if (constraint->origin_constraint_id != PROTOTYPE_INVALID_ID &&
			(constraint->origin_constraint_id >= id ||
			 ctx->constraints.constraints[
				constraint->origin_constraint_id
			 ].domain != OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
			 ctx->constraints.constraints[
				constraint->origin_constraint_id
			 ].source_occurrence != constraint->source_occurrence)) {
			fprintf(stderr, "constraint db parent edge invalid id=%u parent=%u\n",
				id, constraint->origin_constraint_id);
			return -1;
		}
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[
				constraint->source_occurrence
			];
		if (constraint->context_id != occurrence->context_id ||
			(constraint->source_ast != PROTOTYPE_INVALID_ID &&
			 constraint->source_ast >= ctx->asts->node_count)) {
			fprintf(stderr, "constraint db origin invalid id=%u context=%u occurrence-context=%u ast=%u\n",
				id, constraint->context_id, occurrence->context_id,
				constraint->source_ast);
			return -1;
		}
		if (id < ctx->constraints.first_branch_refinement_constraint) {
			if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER ||
				constraint->payload.classifier.classifier_meta_id !=
					constraint->source_occurrence) {
				fprintf(stderr, "constraint db classifier payload invalid id=%u\n", id);
				return -1;
			}
		} else if (id < ctx->constraints.first_computation_constraint) {
			const struct prototype_substitution* branch_action =
				constraint->payload.branch_refinement.substitution ==
					PROTOTYPE_INVALID_ID ? NULL : prototype_substitution_get(
					&ctx->metadata->substitutions,
					constraint->payload.branch_refinement.substitution
				);
			if (constraint->domain !=
					OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
				constraint->payload.branch_refinement.match_occurrence >=
					ctx->metadata->typed_occurrences.occurrence_count ||
				constraint->payload.branch_refinement.scrutinee_occurrence >=
					ctx->metadata->typed_occurrences.occurrence_count ||
				ctx->metadata->typed_occurrences.occurrences[
					constraint->payload.branch_refinement.match_occurrence
				].tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
				constraint->payload.branch_refinement.case_index >=
					ctx->metadata->typed_occurrences.occurrences[
						constraint->payload.branch_refinement.match_occurrence
				].case_count ||
				(constraint->payload.branch_refinement.refinement_status ==
						PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
					(!branch_action || branch_action->source_context !=
						constraint->payload.branch_refinement.refined_context ||
					 branch_action->target_context !=
						constraint->payload.branch_refinement.source_context) :
					(constraint->payload.branch_refinement.substitution !=
						PROTOTYPE_INVALID_ID ||
					 constraint->payload.branch_refinement.source_context !=
						constraint->context_id))) {
				fprintf(stderr,
					"constraint db branch payload invalid id=%u match=%u case=%u "
					"status=%d constraint-context=%u source=%u refined=%u "
					"substitution=%u action-source=%u action-target=%u\n",
					id,
					constraint->payload.branch_refinement.match_occurrence,
					constraint->payload.branch_refinement.case_index,
					constraint->payload.branch_refinement.refinement_status,
					constraint->context_id,
					constraint->payload.branch_refinement.source_context,
					constraint->payload.branch_refinement.refined_context,
					constraint->payload.branch_refinement.substitution,
					branch_action ? branch_action->source_context : PROTOTYPE_INVALID_ID,
					branch_action ? branch_action->target_context : PROTOTYPE_INVALID_ID);
				return -1;
			}
		} else if (id < ctx->constraints.first_effect_constraint) {
			uint32_t delta_id = constraint->payload.computation.
				judgement_delta_constraint_id;
			if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
				delta_id >=
					ctx->judgement_delta.computation_constraint_count ||
				ctx->judgement_delta.computation_constraints[
					delta_id
				].occurrence_id != constraint->source_occurrence) {
				fprintf(stderr, "constraint db computation payload invalid id=%u delta=%u\n",
					id, delta_id);
				return -1;
			}
		} else if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW ||
			constraint->payload.effect.result_row >= ctx->terms->term_count ||
			constraint->payload.effect.left_row >= ctx->terms->term_count ||
			(constraint->payload.effect.right_row != PROTOTYPE_INVALID_ID &&
			 constraint->payload.effect.right_row >= ctx->terms->term_count)) {
			fprintf(stderr, "constraint db effect payload invalid id=%u\n", id);
			return -1;
		}
		if (constraint->domain == OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW &&
			constraint->payload.effect.effect_meta_id != PROTOTYPE_INVALID_ID) {
			uint32_t meta_id = constraint->payload.effect.effect_meta_id;
			if (meta_id >= ctx->effect_solver.meta_count ||
				ctx->effect_solver.metas[meta_id].owner_occurrence !=
					constraint->source_occurrence ||
				ctx->effect_solver.metas[meta_id].placeholder_row !=
					constraint->payload.effect.result_row ||
				(constraint->state == OPERATION_CONSTRAINT_STATE_SOLVED &&
				 ctx->effect_solver.metas[meta_id].state !=
					OPERATION_EFFECT_ROW_META_SOLVED)) {
				fprintf(stderr, "constraint db effect meta edge invalid id=%u meta=%u\n",
					id, meta_id);
				return -1;
			}
		}
	}
	for (uint32_t occurrence = 0;
		occurrence < ctx->metadata->typed_occurrences.occurrence_count;
		++occurrence) {
		uint32_t computation_id =
			ctx->constraints.computation_constraint_for_occurrence[occurrence];
		if (computation_id != PROTOTYPE_INVALID_ID &&
			(computation_id < ctx->constraints.first_computation_constraint ||
			 computation_id >= ctx->constraints.first_effect_constraint ||
			 ctx->constraints.constraints[computation_id].domain !=
				OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
			 ctx->constraints.constraints[computation_id].source_occurrence !=
				occurrence)) {
			fprintf(stderr,
				"constraint db computation index invalid occurrence=%u id=%u\n",
				occurrence, computation_id);
			return -1;
		}
		uint32_t traversed = 0;
		for (uint32_t dependency =
				ctx->constraints.first_dependent_constraint[occurrence];
			dependency != PROTOTYPE_INVALID_ID;
			dependency = ctx->constraints.dependent_constraints[dependency].next) {
			if (dependency >= ctx->constraints.dependent_constraint_count ||
				ctx->constraints.dependent_constraints[
					dependency
				].source_occurrence != occurrence ||
				ctx->constraints.dependent_constraints[
					dependency
				].constraint >= ctx->constraints.constraint_count ||
				++traversed > ctx->constraints.dependent_constraint_count) {
				fprintf(
					stderr,
					"constraint db dependency invalid occurrence=%u dependency=%u\n",
					occurrence,
					dependency
				);
				return -1;
			}
		}
	}
	return 0;
}

static struct operation_constraint_db_mark operation_constraint_db_mark(
	const struct operation_constraint_db* db
) {
	return (struct operation_constraint_db_mark){
		.constraint_count = db->constraint_count,
		.classifier_constraint_count = db->classifier_constraint_count,
		.first_branch_refinement_constraint =
			db->first_branch_refinement_constraint,
		.branch_refinement_constraint_count =
			db->branch_refinement_constraint_count,
		.first_computation_constraint = db->first_computation_constraint,
		.computation_constraint_count = db->computation_constraint_count,
		.first_effect_constraint = db->first_effect_constraint,
		.effect_constraint_count = db->effect_constraint_count,
		.dependent_constraint_count = db->dependent_constraint_count,
		.worklist_head = db->worklist_head,
		.worklist_count = db->worklist_count
	};
}

static int operation_constraint_db_rollback(
	struct operation_constraint_db* db,
	struct operation_constraint_db_mark mark
) {
	if (!db || mark.constraint_count > db->constraint_count ||
		mark.dependent_constraint_count > db->dependent_constraint_count) {
		return -1;
	}
	for (uint32_t i = db->constraint_count; i > mark.constraint_count; --i) {
		uint32_t constraint_id = i - 1;
		const struct operation_constraint* constraint =
			&db->constraints[constraint_id];
		if (constraint->domain == OPERATION_CONSTRAINT_DOMAIN_COMPUTATION &&
			constraint->source_occurrence < 4096 &&
			db->computation_constraint_for_occurrence[
				constraint->source_occurrence
			] == constraint_id) {
			db->computation_constraint_for_occurrence[
				constraint->source_occurrence
			] = PROTOTYPE_INVALID_ID;
		}
	}
	for (uint32_t i = db->dependent_constraint_count;
		i > mark.dependent_constraint_count;
		--i) {
		uint32_t dependency_id = i - 1;
		uint32_t source =
			db->dependent_constraints[dependency_id].source_occurrence;
		if (source >= 4096 ||
			db->first_dependent_constraint[source] != dependency_id) {
			return -1;
		}
		db->first_dependent_constraint[source] =
			db->dependent_constraints[dependency_id].next;
	}
	memset(
		&db->constraints[mark.constraint_count],
		0,
		(db->constraint_count - mark.constraint_count) *
			sizeof(*db->constraints)
	);
	db->constraint_count = mark.constraint_count;
	db->classifier_constraint_count = mark.classifier_constraint_count;
	db->first_branch_refinement_constraint =
		mark.first_branch_refinement_constraint;
	db->branch_refinement_constraint_count =
		mark.branch_refinement_constraint_count;
	db->first_computation_constraint = mark.first_computation_constraint;
	db->computation_constraint_count = mark.computation_constraint_count;
	db->first_effect_constraint = mark.first_effect_constraint;
	db->effect_constraint_count = mark.effect_constraint_count;
	db->dependent_constraint_count = mark.dependent_constraint_count;
	db->worklist_head = mark.worklist_head;
	db->worklist_count = mark.worklist_count;
	return 0;
}

static int operation_solver_var_binding_context(
	const struct compile_context* ctx,
	uint32_t variable_occurrence,
	uint32_t* p_binding_context
) {
	if (!ctx || !ctx->metadata || !p_binding_context ||
		variable_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* variable =
		&ctx->metadata->typed_occurrences.occurrences[variable_occurrence];
	if (variable->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
		variable->binding_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (prototype_context_find_binding(
			&ctx->metadata->contexts,
			variable->context_id,
			variable->binding_id,
			p_binding_context
		) != 0) {
		return 1;
	}
	return 0;
}

static int operation_solver_add_branch_refinement_constraints(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	ctx->constraints.first_branch_refinement_constraint =
		ctx->constraints.constraint_count;
	ctx->constraints.branch_refinement_constraint_count = 0;
	uint32_t mutable_occurrence_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t match_id = mutable_occurrence_start;
		match_id < ctx->metadata->typed_occurrences.occurrence_count;
		++match_id) {
		const struct prototype_typed_occurrence* match =
			&ctx->metadata->typed_occurrences.occurrences[match_id];
		uint32_t scrutinee;
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			continue;
		}
		if (match->first_case + match->case_count >
				ctx->metadata->typed_occurrences.case_count ||
			compile_occurrence_child(
				ctx, match_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
				&scrutinee
			) != 0) {
			fprintf(stderr,
				"branch constraint generation invalid Match match=%u first=%u count=%u cases=%zu\n",
				match_id, match->first_case, match->case_count,
				ctx->metadata->typed_occurrences.case_count);
			return -1;
		}
		for (uint32_t case_index = 0;
			case_index < match->case_count;
			++case_index) {
			uint32_t branch;
			uint32_t case_id = match->first_case + case_index;
			struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[case_id];
			if (ctx->constraints.constraint_count >=
					PROTOTYPE_COMPILER_CONSTRAINT_CAPACITY ||
				compile_occurrence_child(
					ctx, match_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
					case_index, &branch
				) != 0) {
				fprintf(stderr,
					"branch constraint generation invalid case match=%u case=%u constraints=%u\n",
					match_id, case_index, ctx->constraints.constraint_count);
				return -1;
			}
			const struct prototype_typed_occurrence* branch_occurrence =
				&ctx->metadata->typed_occurrences.occurrences[branch];
			/* Until refinement produces an accepted substitution, the branch
			 * occurrence Context is the only source Context represented by this
			 * constraint. The case Context remains authoritative on
			 * operation_case itself. */
			uint32_t source_context = branch_occurrence->context_id;
			uint32_t refined_context = PROTOTYPE_INVALID_ID;
			uint32_t substitution = PROTOTYPE_INVALID_ID;
			int refinement_status = PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			int state = OPERATION_CONSTRAINT_STATE_PENDING;
			int reason = OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY;
			if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
				const struct prototype_substitution* action =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						operation_case->refinement_substitution
					);
				if (!action || action->source_context !=
						operation_case->context_id) {
					fprintf(stderr,
						"branch constraint generation invalid frozen action match=%u case=%u action=%u action-source=%u case-context=%u\n",
						match_id, case_index,
						operation_case->refinement_substitution,
						action ? action->source_context : PROTOTYPE_INVALID_ID,
						operation_case->context_id);
					return -1;
				}
				source_context = action->target_context;
				refined_context = action->source_context;
				substitution = operation_case->refinement_substitution;
				refinement_status = PROTOTYPE_INDEX_REFINEMENT_SOLVED;
				state = OPERATION_CONSTRAINT_STATE_SOLVED;
				reason =
					OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_VALIDATED;
			} else if (operation_case->refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
				refinement_status = PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
				state = OPERATION_CONSTRAINT_STATE_SOLVED;
				reason =
					OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_IMPOSSIBLE;
			} else if (operation_case->refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT) {
				refinement_status = PROTOTYPE_INDEX_REFINEMENT_CONSTANT;
				state = OPERATION_CONSTRAINT_STATE_SOLVED;
				reason =
					OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_CONSTANT;
			}
			uint32_t id = ctx->constraints.constraint_count++;
			ctx->constraints.branch_refinement_constraint_count++;
			ctx->constraints.constraints[id] = (struct operation_constraint){
				.id = id,
				.domain = OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT,
				.kind = OPERATION_BRANCH_REFINEMENT_CONSTRAINT_MATCH_CASE,
				.source_occurrence = branch,
				.source_ast = branch_occurrence->source_ast,
				.context_id = branch_occurrence->context_id,
				.state = state,
				.reason = reason,
				.evidence_id = PROTOTYPE_INVALID_ID,
				.origin_constraint_id = PROTOTYPE_INVALID_ID,
				.payload.branch_refinement = {
					.match_occurrence = match_id,
					.scrutinee_occurrence = scrutinee,
					.case_index = case_index,
					.source_context = source_context,
					.refined_context = refined_context,
					.substitution = substitution,
					.constructor_term = PROTOTYPE_INVALID_ID,
					.residual_pattern = PROTOTYPE_INVALID_ID,
					.residual_value = PROTOTYPE_INVALID_ID,
					.refinement_status = refinement_status
				}
			};
		}
	}
	return 0;
}

static int operation_solver_index_branch_refinement_constraints(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			operation_solver_add_constraint_dependency(
				ctx,
				constraint->payload.branch_refinement.scrutinee_occurrence,
				id
			) != 0 || operation_solver_add_constraint_dependency(
				ctx,
				constraint->payload.branch_refinement.match_occurrence,
				id
			) != 0 || operation_solver_add_constraint_dependency(
				ctx, constraint->source_occurrence, id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_reset_classifier_state(
	struct compile_context* ctx,
	int preserve_append_only_prefix
) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	struct operation_classifier_solver* solver = &ctx->classifier_solver;
	uint64_t revision = solver->solution_revision + 1;
	struct operation_classifier_meta_solution preserved[4096];
	uint32_t preserved_count = preserve_append_only_prefix && solver->initialized ?
		solver->initialized_occurrence_count : 0;
	if (preserved_count > ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	if (preserved_count != 0) {
		memcpy(preserved, solver->classifier_metas, sizeof(preserved));
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			solver->classifier_metas[i].classifier != PROTOTYPE_INVALID_ID) {
			operation_context_binding_dependency_changed(
				ctx, occurrence->binding_id
			);
		}
	}
	memset(solver, 0, sizeof(*solver));
	solver->initialized = 1;
	solver->initialized_occurrence_count =
		ctx->metadata->typed_occurrences.occurrence_count;
	solver->solution_revision = revision;
	size_t immutable_prefix =
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t i = 0; i < 4096; ++i) {
		if (i < preserved_count) {
			solver->classifier_metas[i] = preserved[i];
			solver->classifier_metas[i].owner_occurrence = i;
		} else {
			solver->classifier_metas[i].owner_occurrence = i;
			solver->classifier_metas[i].classifier = i < immutable_prefix ?
				ctx->metadata->typed_occurrences.occurrences[i].classifier :
				PROTOTYPE_INVALID_ID;
			solver->classifier_metas[i].stale_classifier = PROTOTYPE_INVALID_ID;
			solver->classifier_metas[i].evidence_constraint_id = PROTOTYPE_INVALID_ID;
			solver->classifier_metas[i].state = i < immutable_prefix ?
				OPERATION_CLASSIFIER_META_FROZEN :
				OPERATION_CLASSIFIER_META_UNSOLVED;
			solver->classifier_metas[i].binder_classifier = i < immutable_prefix ?
				ctx->metadata->typed_occurrences.occurrences[i].binder_classifier :
				PROTOTYPE_INVALID_ID;
			solver->classifier_metas[i].binder_evidence_constraint_id =
				PROTOTYPE_INVALID_ID;
			solver->classifier_metas[i].binder_state = i < immutable_prefix ?
				OPERATION_CLASSIFIER_META_FROZEN :
				OPERATION_CLASSIFIER_META_UNSOLVED;
		}
		solver->motive_constant_candidates[i] = PROTOTYPE_INVALID_ID;
		solver->motive_solution_states[i] =
			OPERATION_MOTIVE_SOLUTION_UNRESOLVED;
		solver->ih_motive_application_ids[i] = PROTOTYPE_INVALID_ID;
		solver->motive_terms[i] = PROTOTYPE_INVALID_ID;
	}
	return operation_solver_refresh_usage_solutions(ctx) != 0 ||
		operation_solver_initialize_input_facts(ctx) != 0 ? -1 : 0;
}

static int operation_solver_generate_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	ctx->metadata->constraint_generation_pass_count++;
	struct operation_constraint_db* constraints = &ctx->constraints;
	memset(constraints, 0, sizeof(*constraints));
	memset(&ctx->effect_solver, 0, sizeof(ctx->effect_solver));
	for (uint32_t i = 0; i < 4096; ++i) {
		constraints->first_dependent_constraint[i] = PROTOTYPE_INVALID_ID;
		constraints->computation_constraint_for_occurrence[i] =
			PROTOTYPE_INVALID_ID;
		ctx->effect_solver.meta_for_occurrence[i] = PROTOTYPE_INVALID_ID;
	}
	if (operation_solver_reset_classifier_state(
			ctx, ctx->classifier_solver.initialized
		) != 0) {
		return -1;
	}
	uint32_t mutable_occurrence_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t i = mutable_occurrence_start;
		i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation = &ctx->metadata->typed_occurrences.occurrences[i];
		int base_constraint_kind = OPERATION_CONSTRAINT_HAS_TYPE;
		union operation_classifier_goal_payload base_payload = { 0 };
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
			operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			base_constraint_kind = OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
					&base_payload.constructor_formation.function_operation
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
					&base_payload.constructor_formation.argument_occurrence
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_FORCE) {
			int child_role = operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ?
				PROTOTYPE_TERM_CHILD_RETURN_VALUE :
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ?
					PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION :
					PROTOTYPE_TERM_CHILD_FORCE_VALUE;
			base_constraint_kind = OPERATION_CONSTRAINT_CBPV_BOUNDARY;
			if (compile_occurrence_child(
					ctx, i, child_role, 0,
					&base_payload.cbpv_boundary.child_operation
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD) {
			base_constraint_kind = OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					&base_payload.computation.first_operation
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&base_payload.computation.second_operation
				) != 0 || compile_occurrence_child(
					ctx, base_payload.computation.second_operation,
					PROTOTYPE_TERM_CHILD_BODY, 0,
					&base_payload.computation.third_operation
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REQUEST) {
			base_constraint_kind = OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
					&base_payload.computation.first_operation
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0,
					&base_payload.computation.second_operation
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
					&base_payload.computation.third_operation
				) != 0) {
				return -1;
			}
		}
		if (operation_solver_add_classifier_goal(
				ctx,
				base_constraint_kind,
				i,
				base_payload
			) != 0) {
			return -1;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			ctx->classifier_seeds[i].classifier == PROTOTYPE_INVALID_ID &&
			operation->classifier == PROTOTYPE_INVALID_ID) {
			uint32_t binding_context;
			int binder_status = operation_solver_var_binding_context(
				ctx, i, &binding_context
			);
			if (binder_status < 0) {
				return -1;
			}
			if (binder_status == 0) {
				union operation_classifier_goal_payload payload = { 0 };
				payload.binder_type.binding_context = binding_context;
				if (operation_solver_add_classifier_goal(
						ctx, OPERATION_CONSTRAINT_BINDER_TYPE, i, payload
					) != 0) {
					return -1;
				}
			}
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.reference.referenced_operation = operation->wrapped_occurrence;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_EQUAL, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.conversion.body_occurrence = operation->wrapped_occurrence;
			payload.conversion.expected_classifier = ctx->classifier_seeds[i].classifier;
			if (operation->wrapped_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
				operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_CONVERTIBLE, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.pi.role = OPERATION_CLASSIFIER_PI_GOAL_INTRO;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_BODY, 0,
					&payload.pi.body_or_function_operation
				) != 0) {
				return -1;
			}
			payload.pi.domain_classifier_or_argument_occurrence =
				operation_solver_binder_classifier(ctx, i);
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
			(operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION ||
			 operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION)) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.pi.role = OPERATION_CLASSIFIER_PI_GOAL_ELIM;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
					&payload.pi.body_or_function_operation
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
					&payload.pi.domain_classifier_or_argument_occurrence
				) != 0) {
				return -1;
			}
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			uint32_t scrutinee_operation;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
					&scrutinee_operation
				) != 0) {
				return -1;
			}
			for (uint32_t j = 0; j < operation->case_count; ++j) {
				uint32_t branch_operation;
				if (compile_occurrence_child(
						ctx, i, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, j,
						&branch_operation
					) != 0 ||
					operation_solver_add_motive_case_goal(
						ctx, i, j, branch_operation, scrutinee_operation
					) != 0) {
					return -1;
				}
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			union operation_classifier_goal_payload payload = { 0 };
			payload.induction_hypothesis.owner_match_occurrence =
				operation->ih_owner_occurrence;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
					&payload.induction_hypothesis.recursive_argument_occurrence
				) != 0) {
				return -1;
			}
			payload.induction_hypothesis.ih_scope_id = operation->ih_scope_id;
			payload.induction_hypothesis.case_index = operation->ih_case_index;
			payload.induction_hypothesis.field_index = operation->ih_field_index;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_IH_EXPECTED, i, payload
				) != 0) {
				return -1;
			}
		}
	}
	if (operation_solver_add_branch_refinement_constraints(ctx) != 0) {
		fprintf(stderr, "constraint generation: branch constraints failed\n");
		return -1;
	}
	if (operation_solver_index_constraints(ctx) != 0) {
		fprintf(stderr, "constraint generation: classifier dependency index failed\n");
		return -1;
	}
	if (operation_solver_index_branch_refinement_constraints(ctx) != 0) {
		fprintf(stderr, "constraint generation: branch dependency index failed\n");
		return -1;
	}
	ctx->constraints.first_computation_constraint =
		ctx->constraints.constraint_count;
	ctx->constraints.computation_constraint_count = 0;
	ctx->constraints.first_effect_constraint = ctx->constraints.constraint_count;
	ctx->constraints.effect_constraint_count = 0;
	return operation_constraint_db_validate(ctx);
}

/* Branch pullback changes the projection of existing occurrences into a new
 * Context, but it does not change occurrence topology. Keep stable constraint
 * IDs and dependency edges; refresh only payload fields derived from the
 * mutable occurrence projection, then rerun the same constraint graph. */
static int operation_solver_refresh_classifier_constraints(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		fprintf(stderr, "branch refresh invalid state\n");
		return -1;
	}
	if (operation_solver_reset_classifier_state(ctx, 0) != 0) {
		fprintf(stderr, "branch refresh solver reset failed\n");
		return -1;
	}
	ctx->constraints.worklist_head = 0;
	ctx->constraints.worklist_count = 0;
	memset(
		ctx->constraints.constraint_queued,
		0,
		sizeof(ctx->constraints.constraint_queued)
	);
	for (uint32_t id = 0;
		id < ctx->constraints.classifier_constraint_count;
		++id) {
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		uint32_t operation_id = constraint->source_occurrence;
		if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER ||
			operation_id >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			fprintf(stderr, "branch refresh classifier header id=%u\n", id);
			return -1;
		}
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		constraint->context_id = operation->context_id;
		constraint->state = OPERATION_CONSTRAINT_STATE_PENDING;
		constraint->reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE;
		constraint->evidence_id = PROTOTYPE_INVALID_ID;
		memset(&constraint->conversion_goal, 0, sizeof(constraint->conversion_goal));
		switch (constraint->kind) {
			case OPERATION_CONSTRAINT_CONVERTIBLE:
				constraint->payload.classifier.goal.conversion.
					expected_classifier = ctx->classifier_seeds[operation_id].classifier;
				break;
			case OPERATION_CONSTRAINT_BINDER_TYPE: {
				uint32_t binding_context;
				int binding_status = operation_solver_var_binding_context(
					ctx, operation_id, &binding_context
				);
				if (binding_status < 0) {
					fprintf(stderr, "branch refresh binder id=%u operation=%u\n", id, operation_id);
					return -1;
				}
				constraint->payload.classifier.goal.binder_type.binding_context =
					binding_status == 0 ? binding_context : PROTOTYPE_INVALID_ID;
				break;
			}
			case OPERATION_CONSTRAINT_PI_EXPECTED:
				if (constraint->payload.classifier.goal.pi.role ==
						OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
					constraint->payload.classifier.goal.pi.
						domain_classifier_or_argument_occurrence =
						operation_solver_binder_classifier(ctx, operation_id);
				}
				break;
			case OPERATION_CONSTRAINT_MOTIVE_EQUATION: {
				uint32_t case_index = constraint->payload.classifier.goal.
					motive_case.case_index;
				if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
					operation->core_term >= ctx->terms->term_count ||
					ctx->terms->terms[operation->core_term].tag !=
						PROTOTYPE_TERM_MATCH ||
					case_index >= operation->case_count) {
					fprintf(stderr, "branch refresh motive header id=%u operation=%u case=%u\n", id, operation_id, case_index);
					return -1;
				}
				uint32_t term_case_id = ctx->terms->terms[
					operation->core_term
				].as.match.first_case + case_index;
				if (term_case_id >= ctx->terms->case_count) {
					fprintf(stderr, "branch refresh motive term case id=%u term-case=%u\n", id, term_case_id);
					return -1;
				}
				constraint->payload.classifier.goal.motive_case.
					constructor_owner =
					ctx->terms->cases[term_case_id].constructor_owner;
				constraint->payload.classifier.goal.motive_case.ih_scope_id =
					ctx->terms->terms[operation->core_term].as.match.ih_scope_id;
				break;
			}
			default:
				break;
		}
		if (operation_solver_enqueue_constraint(ctx, id) != 0) {
			fprintf(stderr, "branch refresh enqueue id=%u\n", id);
			return -1;
		}
	}
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			match_id >= ctx->metadata->typed_occurrences.occurrence_count) {
			fprintf(stderr, "branch refresh branch header id=%u\n", id);
			return -1;
		}
		const struct prototype_typed_occurrence* match =
			&ctx->metadata->typed_occurrences.occurrences[match_id];
		uint32_t case_index =
			constraint->payload.branch_refinement.case_index;
		uint32_t case_id = match->first_case + case_index;
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			case_index >= match->case_count ||
			case_id >= ctx->metadata->typed_occurrences.case_count ||
			constraint->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			fprintf(stderr, "branch refresh branch shape id=%u match=%u case=%u\n", id, match_id, case_index);
			return -1;
		}
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[case_id];
		constraint->context_id = ctx->metadata->typed_occurrences.occurrences[
			constraint->source_occurrence
		].context_id;
		constraint->evidence_id = PROTOTYPE_INVALID_ID;
		constraint->payload.branch_refinement.source_context =
			constraint->context_id;
		constraint->payload.branch_refinement.refined_context =
			PROTOTYPE_INVALID_ID;
		constraint->payload.branch_refinement.substitution = PROTOTYPE_INVALID_ID;
		constraint->payload.branch_refinement.constructor_term =
			PROTOTYPE_INVALID_ID;
		constraint->payload.branch_refinement.residual_pattern =
			PROTOTYPE_INVALID_ID;
		constraint->payload.branch_refinement.residual_value =
			PROTOTYPE_INVALID_ID;
		constraint->payload.branch_refinement.refinement_status =
			PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
		constraint->state = OPERATION_CONSTRAINT_STATE_PENDING;
		constraint->reason =
			OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY;
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			const struct prototype_substitution* action =
				prototype_substitution_get(
					&ctx->metadata->substitutions,
					operation_case->refinement_substitution
				);
			if (!action || action->source_context != operation_case->context_id) {
				fprintf(stderr, "branch refresh solved action id=%u action=%u context=%u\n", id, operation_case->refinement_substitution, operation_case->context_id);
				return -1;
			}
			constraint->payload.branch_refinement.source_context =
				action->target_context;
			constraint->payload.branch_refinement.refined_context =
				action->source_context;
			constraint->payload.branch_refinement.substitution =
				operation_case->refinement_substitution;
			constraint->payload.branch_refinement.refinement_status =
				PROTOTYPE_INDEX_REFINEMENT_SOLVED;
			constraint->state = OPERATION_CONSTRAINT_STATE_SOLVED;
			constraint->reason =
				OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_VALIDATED;
		} else if (operation_case->refinement_status ==
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
			constraint->payload.branch_refinement.refinement_status =
				PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
			constraint->state = OPERATION_CONSTRAINT_STATE_SOLVED;
			constraint->reason =
				OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_IMPOSSIBLE;
		} else if (operation_case->refinement_status ==
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT) {
			constraint->payload.branch_refinement.refinement_status =
				PROTOTYPE_INDEX_REFINEMENT_CONSTANT;
			constraint->state = OPERATION_CONSTRAINT_STATE_SOLVED;
			constraint->reason =
				OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_CONSTANT;
		}
	}
	for (uint32_t id = ctx->constraints.first_computation_constraint;
		id < ctx->constraints.constraint_count;
		++id) {
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		constraint->context_id = ctx->metadata->typed_occurrences.occurrences[
			constraint->source_occurrence
		].context_id;
	}
	/* Constraint records follow occurrence Context roots. Updating that derived
	 * reference does not change a Context object's parent or binder classifier. */
	return operation_constraint_db_validate(ctx);
}

static int operation_solver_seed_known_classifiers(struct compile_context* ctx, int* p_changed) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation = &ctx->metadata->typed_occurrences.occurrences[i];
		uint32_t seed = ctx->classifier_seeds[i].classifier;
		/*
		 * ASCRIPTION is solved against its body, and NAME follows its referenced
		 * source operation. Seeding either from the surface classifier creates a
		 * second authoritative path and can retain unsolved elaboration rows.
		 */
		if (ctx->classifier_solver.classifier_metas[i].classifier ==
				PROTOTYPE_INVALID_ID &&
			operation->tag != PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
			operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
			seed != PROTOTYPE_INVALID_ID &&
			operation_solver_bind(ctx, i, seed, p_changed) != 0) {
			return -1;
		}
		uint32_t binder_seed = ctx->classifier_seeds[i].binder_classifier;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
			ctx->classifier_solver.classifier_metas[i].binder_classifier ==
				PROTOTYPE_INVALID_ID &&
			binder_seed != PROTOTYPE_INVALID_ID &&
			operation_binder_classifier_meta_solve(
				ctx, i, binder_seed, p_changed
			) != 0) {
			return -1;
		}
	}
	return 0;
}
