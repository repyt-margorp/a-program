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
	constraint->result_term = PROTOTYPE_INVALID_ID;
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
				constraint->payload.classifier.classifier_variable !=
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
	for (uint32_t match_id = 0;
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
			uint32_t source_context = operation_case->context_id;
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
				.result_term = PROTOTYPE_INVALID_ID,
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

static int operation_solver_reset_classifier_state(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	struct operation_classifier_solver* solver = &ctx->classifier_solver;
	uint64_t revision = solver->solution_revision + 1;
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			solver->solutions[i].classifier != PROTOTYPE_INVALID_ID) {
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
	for (uint32_t i = 0; i < 4096; ++i) {
		solver->solutions[i].classifier = PROTOTYPE_INVALID_ID;
		solver->solutions[i].effect_row = PROTOTYPE_INVALID_ID;
		solver->solutions[i].first_usage_entry = PROTOTYPE_INVALID_ID;
		solver->solutions[i].usage_entry_count = 0;
		solver->solutions[i].binder_usage = PROTOTYPE_USAGE_ZERO;
		solver->solutions[i].classifier_state =
			OPERATION_CONSTRAINT_STATE_PENDING;
		solver->solutions[i].effect_state = OPERATION_CONSTRAINT_STATE_PENDING;
		solver->solutions[i].usage_state = OPERATION_CONSTRAINT_STATE_PENDING;
		solver->solutions[i].reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE;
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
	for (uint32_t i = 0; i < 4096; ++i) {
		constraints->first_dependent_constraint[i] = PROTOTYPE_INVALID_ID;
		constraints->computation_constraint_for_occurrence[i] =
			PROTOTYPE_INVALID_ID;
	}
	if (operation_solver_reset_classifier_state(ctx) != 0) {
		return -1;
	}
	for (uint32_t i = 0;
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
			ctx->classifier_hints[i] == PROTOTYPE_INVALID_ID &&
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
			payload.conversion.expected_classifier = ctx->classifier_hints[i];
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
				operation->binder_classifier;
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
	if (operation_solver_reset_classifier_state(ctx) != 0) {
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
		constraint->result_term = PROTOTYPE_INVALID_ID;
		constraint->evidence_id = PROTOTYPE_INVALID_ID;
		memset(&constraint->conversion_goal, 0, sizeof(constraint->conversion_goal));
		switch (constraint->kind) {
			case OPERATION_CONSTRAINT_CONVERTIBLE:
				constraint->payload.classifier.goal.conversion.
					expected_classifier = ctx->classifier_hints[operation_id];
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
						operation->binder_classifier;
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
		constraint->result_term = PROTOTYPE_INVALID_ID;
		constraint->evidence_id = PROTOTYPE_INVALID_ID;
		constraint->payload.branch_refinement.source_context =
			operation_case->context_id;
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
		uint32_t seed = ctx->classifier_hints[i] != PROTOTYPE_INVALID_ID ?
			ctx->classifier_hints[i] :
			operation->classifier;
		/*
		 * ASCRIPTION is solved against its body, and NAME follows its referenced
		 * source operation. Seeding either from the surface classifier creates a
		 * second authoritative path and can retain unsolved elaboration rows.
		 */
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
			operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
			seed != PROTOTYPE_INVALID_ID &&
			operation_solver_bind(ctx, i, seed, p_changed) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static int operation_subtree_contains_external_ref(
	const struct compile_context* ctx,
	uint32_t root_operation
);

static int operation_effect_row_binder_is_owned(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t binding_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 64; ++depth) {
		const struct prototype_typed_occurrence* wrapper =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t inner = PROTOTYPE_INVALID_ID;
		if (wrapper->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			inner = wrapper->wrapped_occurrence;
		} else if (wrapper->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			inner = wrapper->wrapped_occurrence;
		} else {
			break;
		}
		if (inner >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		operation_id = inner;
	}
	for (uint32_t owner = 0; owner < ctx->metadata->typed_occurrences.occurrence_count; ++owner) {
		const struct prototype_typed_occurrence* lambda =
			&ctx->metadata->typed_occurrences.occurrences[owner];
		if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
			continue;
		}
		int declares_binder = 0;
		for (uint32_t row = 0; row < lambda->implicit_effect_row_count; ++row) {
			if (lambda->implicit_effect_row_binders[row] == binding_id) {
				declares_binder = 1;
				break;
			}
		}
		if (!declares_binder) {
			continue;
		}
		uint8_t visited[ctx->metadata->typed_occurrences.occurrence_count];
		memset(visited, 0, sizeof(visited));
		int contains = operation_subtree_contains_operation(
			ctx, owner, operation_id, visited
		);
		if (contains < 0) {
			return -1;
		}
		if (contains) {
			return 1;
		}
		memset(visited, 0, sizeof(visited));
		contains = operation_subtree_contains_operation(
			ctx, operation_id, owner, visited
		);
		if (contains < 0) {
			return -1;
		}
		if (contains) {
			return 1;
		}
	}
	return 0;
}

static int operation_solver_close_unowned_effect_rows(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier,
	uint32_t* p_closed
) {
	if (!ctx || !p_closed || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t closed = classifier;
	const uint32_t* variable_terms;
	size_t variable_count;
	if (prototype_term_effect_row_variable_terms(
			ctx->terms, &variable_terms, &variable_count
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < variable_count; ++i) {
		uint32_t binding_id = ctx->terms->terms[
			variable_terms[i]
		].as.effect_row_var.binding_id;
		if (!prototype_term_contains_free_binding(
				ctx->terms, closed, binding_id
			)) {
			continue;
		}
		int owned = operation_effect_row_binder_is_owned(
			ctx, operation_id, binding_id
		);
		if (owned < 0) {
			return -1;
		}
		if (owned) {
			continue;
		}
		uint32_t empty_row;
		if (prototype_term_effect_row_empty(ctx->terms, &empty_row) != 0 ||
			prototype_term_graph_substitute_bound_var(
				ctx->terms,
				ctx->type_declarations,
				closed,
				binding_id,
				empty_row,
				&closed
			) != 0) {
			return -1;
		}
	}
	*p_closed = closed;
	return 0;
}

static int operation_classifier_contains_unowned_effect_row(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier
) {
	if (!ctx || !ctx->terms || !ctx->metadata ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	const uint32_t* variable_terms;
	size_t variable_count;
	if (prototype_term_effect_row_variable_terms(
			ctx->terms, &variable_terms, &variable_count
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < variable_count; ++i) {
		uint32_t binding_id = ctx->terms->terms[
			variable_terms[i]
		].as.effect_row_var.binding_id;
		if (prototype_term_contains_free_binding(
				ctx->terms,
				classifier,
			binding_id
		)) {
			int owned = operation_effect_row_binder_is_owned(
				ctx, operation_id, binding_id
			);
			if (owned < 0) {
				return -1;
			}
			if (!owned) {
				return 1;
			}
		}
	}
	return 0;
}

static int operation_effect_constraint_add(
	struct compile_context* ctx,
	int kind,
	uint32_t operation,
	uint32_t result_row,
	uint32_t left_row,
	uint32_t right_row,
	uint32_t origin_constraint_id
) {
	if (!ctx || !ctx->metadata ||
		operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		result_row >= ctx->terms->term_count ||
		left_row >= ctx->terms->term_count ||
		(right_row != PROTOTYPE_INVALID_ID &&
			right_row >= ctx->terms->term_count) ||
		(origin_constraint_id != PROTOTYPE_INVALID_ID &&
			(origin_constraint_id >= ctx->constraints.constraint_count ||
			 ctx->constraints.constraints[origin_constraint_id].domain !=
				OPERATION_CONSTRAINT_DOMAIN_COMPUTATION))) {
		return -1;
	}
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		if (id >= ctx->constraints.constraint_count) {
			return -1;
		}
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW) {
			return -1;
		}
		if (constraint->kind == kind &&
			constraint->source_occurrence == operation &&
			constraint->origin_constraint_id == origin_constraint_id &&
			constraint->payload.effect.result_row == result_row &&
			constraint->payload.effect.left_row == left_row &&
			constraint->payload.effect.right_row == right_row) {
			return 0;
		}
	}
	if (ctx->constraints.effect_constraint_count >=
			PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY ||
		ctx->constraints.constraint_count >=
			PROTOTYPE_COMPILER_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t id = ctx->constraints.constraint_count++;
	ctx->constraints.effect_constraint_count++;
	ctx->constraints.constraints[id] = (struct operation_constraint){
		.id = id,
		.domain = OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW,
		.kind = kind,
		.state = PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_PENDING,
		.result_term = PROTOTYPE_INVALID_ID,
		.evidence_id = PROTOTYPE_INVALID_ID,
		.origin_constraint_id = origin_constraint_id,
		.source_occurrence = operation,
		.source_ast = ctx->metadata->typed_occurrences.occurrences[
			operation
		].source_ast,
		.context_id = ctx->metadata->typed_occurrences.occurrences[
			operation
		].context_id,
		.payload.effect = {
			.result_row = result_row,
			.left_row = left_row,
			.right_row = right_row
		}
	};
	return 0;
}

static int operation_classifier_computation_view(
	struct compile_context* ctx,
	uint32_t operation,
	struct prototype_term_classifier_view* p_view
) {
	if (!ctx || !p_view || operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t classifier = operation_available_classifier(ctx, operation);
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			classifier,
			p_view
		) != 0) {
		return -1;
	}
	return p_view->category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		p_view->computation_kind ==
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
		p_view->effect_row != PROTOTYPE_INVALID_ID ? 0 : 1;
}

static int operation_effect_row_meta_find_or_add(
	struct compile_context* ctx,
	uint32_t owner_operation,
	uint32_t placeholder_row,
	struct operation_effect_row_meta** p_meta
) {
	if (!ctx || !p_meta || owner_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[placeholder_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->effect_solver.meta_count; ++i) {
		struct operation_effect_row_meta* meta = &ctx->effect_solver.metas[i];
		if (meta->owner_operation == owner_operation &&
			meta->placeholder_row == placeholder_row) {
			*p_meta = meta;
			return 0;
		}
	}
	if (ctx->effect_solver.meta_count >=
		PROTOTYPE_TYPED_OCCURRENCE_EFFECT_ROW_META_CAPACITY) {
		return -1;
	}
	struct operation_effect_row_meta* meta =
		&ctx->effect_solver.metas[ctx->effect_solver.meta_count++];
	*meta = (struct operation_effect_row_meta){
		.owner_operation = owner_operation,
		.placeholder_row = placeholder_row,
		.first_solution_atom = PROTOTYPE_INVALID_ID,
		.solution_row = PROTOTYPE_INVALID_ID,
		.state = OPERATION_EFFECT_ROW_META_UNSOLVED
	};
	*p_meta = meta;
	return 0;
}

static int operation_effect_normal_add_atom(
	struct prototype_effect_row_normal_form* normal,
	uint32_t atom
) {
	if (!normal) {
		return -1;
	}
	for (uint32_t i = 0; i < normal->atom_count; ++i) {
		if (normal->atoms[i] == atom) {
			return 0;
		}
	}
	if (normal->atom_count >= PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	normal->atoms[normal->atom_count++] = atom;
	return 0;
}

static int operation_effect_normal_union(
	struct prototype_effect_row_normal_form* target,
	const struct prototype_effect_row_normal_form* source
) {
	if (!target || !source) {
		return -1;
	}
	for (uint32_t i = 0; i < source->atom_count; ++i) {
		if (operation_effect_normal_add_atom(target, source->atoms[i]) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_normal_equal(
	const struct prototype_effect_row_normal_form* left,
	const struct prototype_effect_row_normal_form* right
) {
	return left && right &&
		prototype_term_effect_row_normal_form_includes(left, right) &&
		prototype_term_effect_row_normal_form_includes(right, left);
}

static int operation_effect_meta_solution_normal(
	const struct operation_effect_solver* solver,
	const struct operation_effect_row_meta* meta,
	struct prototype_effect_row_normal_form* p_normal
) {
	if (!solver || !meta || !p_normal ||
		meta->state != OPERATION_EFFECT_ROW_META_SOLVED ||
		meta->first_solution_atom > solver->solution_atom_count ||
		meta->solution_atom_count > solver->solution_atom_count -
			meta->first_solution_atom ||
		meta->solution_atom_count >
			PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	memset(p_normal, 0, sizeof(*p_normal));
	p_normal->atom_count = meta->solution_atom_count;
	memcpy(
		p_normal->atoms,
		&solver->solution_atoms[meta->first_solution_atom],
		meta->solution_atom_count * sizeof(*p_normal->atoms)
	);
	return 0;
}

static int operation_effect_meta_store_solution(
	struct operation_effect_solver* solver,
	struct operation_effect_row_meta* meta,
	const struct prototype_effect_row_normal_form* normal
) {
	if (!solver || !meta || !normal ||
		normal->atom_count > PROTOTYPE_TYPED_OCCURRENCE_EFFECT_ROW_SOLUTION_ATOM_CAPACITY -
			solver->solution_atom_count) {
		return -1;
	}
	if (meta->state == OPERATION_EFFECT_ROW_META_SOLVED) {
		struct prototype_effect_row_normal_form existing;
		return operation_effect_meta_solution_normal(
			solver, meta, &existing
		) == 0 && operation_effect_normal_equal(&existing, normal) ? 0 : -1;
	}
	meta->first_solution_atom = solver->solution_atom_count;
	meta->solution_atom_count = normal->atom_count;
	memcpy(
		&solver->solution_atoms[solver->solution_atom_count],
		normal->atoms,
		normal->atom_count * sizeof(*normal->atoms)
	);
	solver->solution_atom_count += normal->atom_count;
	meta->state = OPERATION_EFFECT_ROW_META_SOLVED;
	return 0;
}

static int operation_effect_resolve_row_normal(
	struct compile_context* ctx,
	uint32_t owner_operation,
	uint32_t row,
	struct prototype_effect_row_normal_form* p_normal
) {
	struct prototype_effect_row_normal_form source;
	if (!ctx || !p_normal ||
		prototype_term_effect_row_normal_form(ctx->terms, row, &source) != 0) {
		return -1;
	}
	memset(p_normal, 0, sizeof(*p_normal));
	for (uint32_t i = 0; i < source.atom_count; ++i) {
		uint32_t atom = source.atoms[i];
		struct operation_effect_row_meta* solved_meta = NULL;
		if (atom < ctx->terms->term_count &&
			ctx->terms->terms[atom].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			for (uint32_t j = 0; j < ctx->effect_solver.meta_count; ++j) {
				struct operation_effect_row_meta* candidate =
					&ctx->effect_solver.metas[j];
				if (candidate->owner_operation == owner_operation &&
					candidate->placeholder_row == atom &&
					candidate->state == OPERATION_EFFECT_ROW_META_SOLVED) {
					solved_meta = candidate;
					break;
				}
			}
		}
		if (solved_meta) {
			struct prototype_effect_row_normal_form solution;
			if (operation_effect_meta_solution_normal(
					&ctx->effect_solver, solved_meta, &solution
				) != 0 || operation_effect_normal_union(p_normal, &solution) != 0) {
				return -1;
			}
		} else if (operation_effect_normal_add_atom(p_normal, atom) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_effect_materialize_solution(
	struct compile_context* ctx,
	struct operation_effect_row_meta* meta,
	uint32_t replacement
) {
	if (!ctx || !meta || replacement >= ctx->terms->term_count ||
		meta->owner_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		meta->placeholder_row >= ctx->terms->term_count ||
		ctx->terms->terms[meta->placeholder_row].tag !=
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return -1;
	}
	uint32_t owner = meta->owner_operation;
	uint32_t binding_id = ctx->terms->terms[
		meta->placeholder_row
	].as.effect_row_var.binding_id;
	uint32_t* classifiers[] = {
		&ctx->classifier_hints[owner],
		&ctx->metadata->typed_occurrences.occurrences[owner].binder_classifier,
		&ctx->classifier_solver.solutions[owner].classifier
	};
	for (size_t i = 0; i < sizeof(classifiers) / sizeof(classifiers[0]); ++i) {
		if (*classifiers[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (prototype_term_graph_substitute_bound_var(
				ctx->terms,
				ctx->type_declarations,
				*classifiers[i],
				binding_id,
				replacement,
				classifiers[i]
			) != 0) {
			return -1;
		}
	}
	meta->solution_row = replacement;
	meta->state = OPERATION_EFFECT_ROW_META_SOLVED;
	return 0;
}

static int operation_effect_row_is_ground(
	const struct prototype_term_db* terms,
	uint32_t row,
	uint32_t depth
) {
	if (!terms || row >= terms->term_count || depth > 256) {
		return 0;
	}
	const struct prototype_term* term = &terms->terms[row];
	switch (term->tag) {
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 1;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return operation_effect_row_is_ground(
				terms, term->as.effect_row_operation.latent_row, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return operation_effect_row_is_ground(
					terms, term->as.effect_row_union.left, depth + 1
				) && operation_effect_row_is_ground(
					terms, term->as.effect_row_union.right, depth + 1
				);
		default:
			return 0;
	}
}

static int operation_effect_normal_is_ground(
	const struct prototype_term_db* terms,
	const struct prototype_effect_row_normal_form* normal
) {
	if (!terms || !normal) {
		return 0;
	}
	for (uint32_t i = 0; i < normal->atom_count; ++i) {
		if (!operation_effect_row_is_ground(terms, normal->atoms[i], 0)) {
			return 0;
		}
	}
	return 1;
}

static int operation_effect_solve_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (;;) {
		int changed = 0;
		for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
			uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
			if (id >= ctx->constraints.constraint_count) {
				return -1;
			}
			struct operation_constraint* constraint =
				&ctx->constraints.constraints[id];
			if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW) {
				return -1;
			}
			if ((constraint->kind ==
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT ||
				 constraint->kind ==
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY) &&
				constraint->payload.effect.result_row ==
					constraint->payload.effect.left_row) {
				int unowned = operation_classifier_contains_unowned_effect_row(
					ctx,
					constraint->source_occurrence,
					operation_available_classifier(
						ctx, constraint->source_occurrence
					)
				);
				if (unowned < 0) {
					return -1;
				}
				int has_external_ref = unowned != 0 ?
					operation_subtree_contains_external_ref(
						ctx, constraint->source_occurrence
					) : 0;
				if (has_external_ref < 0) {
					return -1;
				}
				/* The identity equation adds no obligation when every symbolic
				 * row is scoped by this Operation. A free external row remains a
				 * residual link-time obligation even though rho = rho is trivial. */
				int state = unowned != 0 && has_external_ref != 0 ?
					OPERATION_CONSTRAINT_STATE_RESIDUAL :
					OPERATION_CONSTRAINT_STATE_SOLVED;
				if (operation_constraint_transition(
						constraint, state, OPERATION_CLASSIFIER_GOAL_REASON_NONE
					) != 0) {
					return -1;
				}
				continue;
			}
			struct prototype_effect_row_normal_form left;
			struct prototype_effect_row_normal_form right;
			struct prototype_effect_row_normal_form expected;
			struct prototype_effect_row_normal_form actual;
			int deferred = 0;
			if (operation_effect_resolve_row_normal(
					ctx,
					constraint->source_occurrence,
					constraint->payload.effect.left_row,
					&left
				) != 0 ||
				(constraint->payload.effect.right_row != PROTOTYPE_INVALID_ID &&
				 operation_effect_resolve_row_normal(
					ctx,
					constraint->source_occurrence,
					constraint->payload.effect.right_row,
					&right
				 ) != 0) ||
				operation_effect_resolve_row_normal(
					ctx,
					constraint->source_occurrence,
					constraint->payload.effect.result_row,
					&actual
				) != 0) {
				return -1;
			}
			memset(&expected, 0, sizeof(expected));
			switch (constraint->kind) {
				case PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT:
				case PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY:
					expected = left;
					break;
				case PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION:
					expected = left;
					if (operation_effect_normal_union(&expected, &right) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL:
					for (uint32_t handled_index = 0;
						handled_index < right.atom_count;
						++handled_index) {
						uint32_t handled_atom = right.atoms[handled_index];
						if (handled_atom >= ctx->terms->term_count ||
							ctx->terms->terms[handled_atom].tag !=
								PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
							deferred = 1;
							goto residual_complete;
						}
					}
					for (uint32_t atom_index = 0;
						atom_index < left.atom_count;
						++atom_index) {
						uint32_t atom = left.atoms[atom_index];
						int handled = 0;
						if (atom < ctx->terms->term_count &&
							ctx->terms->terms[atom].tag ==
								PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
							int operation_id = ctx->terms->terms[
								atom
							].as.effect_row_operation.operation_id;
							for (uint32_t handled_index = 0;
								handled_index < right.atom_count;
								++handled_index) {
								uint32_t handled_atom = right.atoms[handled_index];
								handled = handled || ctx->terms->terms[
									handled_atom
								].as.effect_row_operation.operation_id == operation_id;
							}
						}
						if (!handled && operation_effect_normal_add_atom(
								&expected, atom
							) != 0) {
							return -1;
						}
					}
				residual_complete:
					if (deferred) {
						continue;
					}
					break;
				default:
					return -1;
			}
			if (operation_effect_normal_equal(&actual, &expected)) {
				if (operation_constraint_transition(
						constraint,
						OPERATION_CONSTRAINT_STATE_SOLVED,
						OPERATION_CLASSIFIER_GOAL_REASON_NONE
					) != 0) {
					return -1;
				}
				continue;
			}
			if (constraint->payload.effect.result_row >= ctx->terms->term_count ||
				ctx->terms->terms[constraint->payload.effect.result_row].tag !=
					PROTOTYPE_TERM_EFFECT_ROW_VAR) {
				if (operation_effect_normal_is_ground(ctx->terms, &actual) &&
					operation_effect_normal_is_ground(ctx->terms, &expected)) {
					return -1;
					}
				if (operation_constraint_transition(
						constraint,
						OPERATION_CONSTRAINT_STATE_PENDING,
						OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
					) != 0) {
					return -1;
				}
				continue;
			}
			uint32_t result_binder = ctx->terms->terms[
				constraint->payload.effect.result_row
			].as.effect_row_var.binding_id;
			int owned = operation_effect_row_binder_is_owned(
				ctx, constraint->source_occurrence, result_binder
			);
			if (owned < 0) {
				return -1;
			}
			if (owned) {
				if (operation_constraint_transition(
						constraint,
						OPERATION_CONSTRAINT_STATE_PENDING,
						OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
					) != 0) {
					return -1;
				}
				continue;
			}
			if (!operation_effect_normal_is_ground(ctx->terms, &expected)) {
				if (operation_constraint_transition(
						constraint,
						OPERATION_CONSTRAINT_STATE_PENDING,
						OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
					) != 0) {
					return -1;
				}
				continue;
			}
			struct operation_effect_row_meta* meta;
			if (operation_effect_row_meta_find_or_add(
					ctx,
					constraint->source_occurrence,
					constraint->payload.effect.result_row,
					&meta
				) != 0) {
				return -1;
			}
			int was_unsolved = meta->state == OPERATION_EFFECT_ROW_META_UNSOLVED;
			if (operation_effect_meta_store_solution(
					&ctx->effect_solver, meta, &expected
				) != 0) {
				return -1;
			}
			if (operation_constraint_transition(
					constraint,
					OPERATION_CONSTRAINT_STATE_SOLVED,
					OPERATION_CLASSIFIER_GOAL_REASON_NONE
				) != 0) {
				return -1;
			}
			if (was_unsolved) {
				changed = 1;
			}
		}
		if (!changed) {
			break;
		}
	}
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		if (id >= ctx->constraints.constraint_count) {
			return -1;
		}
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->state == OPERATION_CONSTRAINT_STATE_PENDING &&
			operation_constraint_transition(
				constraint,
				OPERATION_CONSTRAINT_STATE_RESIDUAL,
				OPERATION_CLASSIFIER_GOAL_REASON_RESIDUAL_OBLIGATION
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->effect_solver.meta_count; ++i) {
		struct operation_effect_row_meta* meta = &ctx->effect_solver.metas[i];
		if (meta->state != OPERATION_EFFECT_ROW_META_SOLVED ||
			meta->solution_row != PROTOTYPE_INVALID_ID) {
			continue;
		}
		struct prototype_effect_row_normal_form solution;
		uint32_t replacement;
		if (operation_effect_meta_solution_normal(
				&ctx->effect_solver, meta, &solution
			) != 0 || prototype_term_effect_row_materialize_normal_form(
				ctx->terms, &solution, &replacement
			) != 0 || operation_effect_materialize_solution(
				ctx, meta, replacement
			) != 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		if (id >= ctx->constraints.constraint_count) {
			return -1;
		}
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->source_occurrence <
			ctx->metadata->typed_occurrences.occurrence_count) {
			struct operation_solver_solution* solution =
				&ctx->classifier_solver.solutions[constraint->source_occurrence];
			solution->effect_state = constraint->state;
			if (constraint->state ==
				PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_SOLVED) {
				solution->effect_row = constraint->payload.effect.result_row;
			}
		}
	}
	if (operation_constraint_db_validate(ctx) != 0) {
		return -1;
	}
	/* Preserve an immutable diagnostic snapshot. The solver does not read this
	 * projection and v78 does not publish it. */
	if (ctx->constraints.effect_constraint_count >
		ctx->metadata->effect_constraint_capacity) {
		return -1;
	}
	ctx->metadata->effect_constraint_count = 0;
	for (size_t i = 0; i < ctx->constraints.effect_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_effect_constraint + (uint32_t)i;
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		ctx->metadata->effect_constraints[
			ctx->metadata->effect_constraint_count++
		] = (struct prototype_occurrence_effect_constraint){
			.id = constraint->id,
			.kind = constraint->kind,
			.state = constraint->state,
			.occurrence = constraint->source_occurrence,
			.result_row = constraint->payload.effect.result_row,
			.left_row = constraint->payload.effect.left_row,
			.right_row = constraint->payload.effect.right_row
		};
	}
	return 0;
}

static int operation_effect_import_judgement_constraint(
	struct compile_context* ctx,
	const struct prototype_judgement_effect_row_constraint* source
) {
	if (!ctx || !source || source->operand_count != 2 ||
		source->kind == PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION ||
		source->subject >= ctx->terms->term_count ||
		source->result_row >= ctx->terms->term_count ||
		source->operands[0] >= ctx->terms->term_count ||
		source->operands[1] >= ctx->terms->term_count) {
		return source && source->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION ? 0 : -1;
	}
	if (source->computation_constraint_id == PROTOTYPE_INVALID_ID) {
		/* Authority-neutral Core helper equations do not own a source occurrence
		 * and therefore cannot become elaboration constraints. */
		return 0;
	}
	if (source->computation_constraint_id >=
		ctx->judgement_delta.computation_constraint_count) {
		return -1;
	}
	const struct prototype_judgement_computation_constraint* computation =
		&ctx->judgement_delta.computation_constraints[
			source->computation_constraint_id
		];
	if (computation->subject != source->subject ||
		computation->occurrence_id == PROTOTYPE_INVALID_ID ||
		computation->occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t lifecycle_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < ctx->constraints.computation_constraint_count; ++i) {
		uint32_t id = ctx->constraints.first_computation_constraint + i;
		if (id >= ctx->constraints.constraint_count) {
			return -1;
		}
		const struct operation_constraint* candidate =
			&ctx->constraints.constraints[id];
		if (candidate->domain == OPERATION_CONSTRAINT_DOMAIN_COMPUTATION &&
			candidate->payload.computation.judgement_delta_constraint_id ==
				source->computation_constraint_id) {
			lifecycle_id = id;
			break;
		}
	}
	if (lifecycle_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_term_classifier_view current_view;
	if (operation_classifier_computation_view(
			ctx, computation->occurrence_id, &current_view
		) != 0) {
		/* The kernel may emit the equation before the occurrence classifier
		 * reaches a computation view. Keep the computation dependency as the
		 * authority and import the row equation on the next fixed-point pass. */
		return 0;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			current_view.effect_row,
			source->result_row
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* The equation belongs to an older fixed-point operand snapshot. Its
		 * owner is known, but it is not a premise of the current projection. */
		return 0;
	}
	int kind = source->kind == PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN ?
		PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION :
		PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_RESIDUAL;
	return operation_effect_constraint_add(
		ctx,
		kind,
		computation->occurrence_id,
		source->result_row,
		source->operands[0],
		source->operands[1],
		lifecycle_id
	);
}

static int operation_effect_generate_constraints_impl(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	/* These constraints are a derived view of the current operation
	 * classifiers. A fixed-point round may replace a symbolic row with a
	 * concrete one, so retaining constraints from an earlier round creates
	 * false residual obligations. */
	uint32_t empty_row;
	if (prototype_term_effect_row_empty(ctx->terms, &empty_row
		) != 0) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		ctx->classifier_solver.initialized &&
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		struct prototype_term_classifier_view result_view;
		if (operation_classifier_computation_view(
				ctx, operation_id, &result_view
			) != 0) {
			continue;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN) {
			if (operation_effect_constraint_add(
					ctx,
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_EXACT,
					operation_id,
					result_view.effect_row,
					empty_row,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_FORCE) {
			uint32_t child = operation->wrapped_occurrence;
			if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_FORCE &&
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0,
					&child
				) != 0) {
				return -1;
			}
			struct prototype_term_classifier_view child_view;
			if (operation_classifier_computation_view(
					ctx, child, &child_view
				) == 0 && operation_effect_constraint_add(
					ctx,
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY,
					operation_id,
					result_view.effect_row,
					child_view.effect_row,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
			operation->case_count != 0) {
			uint32_t accumulated = PROTOTYPE_INVALID_ID;
			for (uint32_t case_index = 0;
				case_index < operation->case_count;
				++case_index) {
				uint32_t branch_operation;
				if (compile_occurrence_child(
						ctx, operation_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
						case_index, &branch_operation
					) != 0) {
					return -1;
				}
				struct prototype_term_classifier_view case_view;
				if (operation_classifier_computation_view(
						ctx, branch_operation, &case_view
					) != 0) {
					accumulated = PROTOTYPE_INVALID_ID;
					break;
				}
				if (accumulated == PROTOTYPE_INVALID_ID) {
					accumulated = case_view.effect_row;
					continue;
				}
				uint32_t union_row;
				if (prototype_term_effect_row_union(
						ctx->terms,
						accumulated,
						case_view.effect_row,
						&union_row
					) != 0 || operation_effect_constraint_add(
						ctx,
						PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_UNION,
						operation_id,
						union_row,
						accumulated,
						case_view.effect_row,
						PROTOTYPE_INVALID_ID
					) != 0) {
					return -1;
				}
				accumulated = union_row;
			}
			if (accumulated != PROTOTYPE_INVALID_ID &&
				operation_effect_constraint_add(
					ctx,
					PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY,
					operation_id,
					result_view.effect_row,
					accumulated,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
		}
		int has_constraint = 0;
		for (size_t constraint_id = 0;
			constraint_id < ctx->constraints.effect_constraint_count;
			++constraint_id) {
			uint32_t id = ctx->constraints.first_effect_constraint +
				(uint32_t)constraint_id;
			if (id >= ctx->constraints.constraint_count) {
				return -1;
			}
			if (ctx->constraints.constraints[id].source_occurrence == operation_id) {
				has_constraint = 1;
				break;
			}
		}
		if (!has_constraint && operation_effect_constraint_add(
				ctx,
				PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_COPY,
				operation_id,
				result_view.effect_row,
				result_view.effect_row,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
	}
	for (size_t constraint_id = 0;
		constraint_id < ctx->judgement_delta.effect_row_constraint_count;
		++constraint_id) {
		const struct prototype_judgement_effect_row_constraint* row_constraint =
			&ctx->judgement_delta.effect_row_constraints[constraint_id];
		if (row_constraint->operand_count != 2 || row_constraint->kind ==
				PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION) {
			continue;
		}
		if (operation_effect_import_judgement_constraint(
				ctx, row_constraint
			) != 0) {
			return -1;
		}
	}
	return operation_effect_solve_constraints(ctx);
}

static int operation_effect_generate_constraints(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	ctx->constraints.constraint_count = ctx->constraints.first_effect_constraint;
	ctx->constraints.effect_constraint_count = 0;
	struct operation_constraint_db_mark mark =
		operation_constraint_db_mark(&ctx->constraints);
	int status = operation_effect_generate_constraints_impl(ctx);
	if (status != 0 && operation_constraint_db_rollback(
			&ctx->constraints, mark
		) != 0) {
		return -1;
	}
	return status;
}

static size_t operation_effect_unresolved_count(
	const struct prototype_compile_metadata* metadata
) {
	if (!metadata) {
		return 0;
	}
	size_t count = 0;
	for (size_t i = 0; i < metadata->effect_constraint_count; ++i) {
		if (metadata->effect_constraints[i].state !=
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_SOLVED) {
			count++;
		}
	}
	return count;
}

static int compile_phase_record_residual_effect_constraints(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata || !ctx->terms) {
		return -1;
	}
	for (size_t i = 0; i < ctx->metadata->effect_constraint_count; ++i) {
		const struct prototype_occurrence_effect_constraint* constraint =
			&ctx->metadata->effect_constraints[i];
		if (constraint->state ==
			PROTOTYPE_TYPED_OCCURRENCE_EFFECT_CONSTRAINT_SOLVED) {
			continue;
		}
		if (constraint->occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count ||
			constraint->result_row >= ctx->terms->term_count ||
			constraint->left_row >= ctx->terms->term_count ||
			(constraint->right_row != PROTOTYPE_INVALID_ID &&
			 constraint->right_row >= ctx->terms->term_count)) {
			return -1;
		}
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[
				constraint->occurrence
			];
		int already_recorded = 0;
		for (uint32_t j = 0;
			j < prototype_verification_db_count(&ctx->metadata->verification);
			++j) {
			const struct prototype_verification_obligation* existing =
				prototype_verification_db_get(&ctx->metadata->verification, j);
			if (existing && existing->kind ==
					PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION &&
				existing->occurrence == constraint->occurrence &&
				existing->effect_row == constraint->result_row &&
				existing->input_classifier == constraint->left_row &&
				existing->classifier_family == constraint->right_row) {
				already_recorded = 1;
				break;
			}
		}
		if (!already_recorded && prototype_verification_db_add(
				&ctx->metadata->verification,
				(struct prototype_verification_obligation) {
					.kind =
						PROTOTYPE_VERIFICATION_OBLIGATION_EFFECT_ROW_EQUATION,
					.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
					.occurrence = constraint->occurrence,
					.core_term = occurrence->core_term,
					.computation_occurrence = PROTOTYPE_INVALID_ID,
					.continuation_occurrence = PROTOTYPE_INVALID_ID,
					.continuation_binder_id = PROTOTYPE_INVALID_ID,
					.input_classifier = constraint->left_row,
					.classifier_family = constraint->right_row,
					.effect_row = constraint->result_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF
				},
				NULL
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_require_complete(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t constraint_id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[constraint_id];
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			constraint->state != OPERATION_CONSTRAINT_STATE_RESIDUAL ||
			constraint->payload.branch_refinement.refinement_status !=
				PROTOTYPE_INDEX_REFINEMENT_RESIDUAL) {
			continue;
		}
		const struct prototype_typed_occurrence* branch =
			constraint->source_occurrence <
				ctx->metadata->typed_occurrences.occurrence_count ?
				&ctx->metadata->typed_occurrences.occurrences[
					constraint->source_occurrence
				] : NULL;
		uint32_t source_ast = branch ? branch->source_ast :
			constraint->source_ast;
		(void)add_compile_diagnostic(
			ctx,
			PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
			PROTOTYPE_COMPILE_DIAGNOSTIC_BRANCH_REFINEMENT_RESIDUAL,
			source_ast,
			constraint->source_occurrence,
			constraint_id,
			constraint->payload.branch_refinement.residual_pattern,
			constraint->payload.branch_refinement.residual_value,
			source_ast < ctx->asts->node_count ?
				ctx->asts->nodes[source_ast].span :
				(struct prototype_source_span) { 0, 0 }
		);
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		switch (operation->tag) {
			case PROTOTYPE_TYPED_OCCURRENCE_APP:
			case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
			case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
				if (ctx->classifier_solver.solutions[i].classifier ==
					PROTOTYPE_INVALID_ID) {
					if (ctx->metadata->compile_diagnostic_count == 0) {
						int reason = PROTOTYPE_COMPILE_DIAGNOSTIC_UNSOLVED_CLASSIFIER;
						uint32_t constraint_id = PROTOTYPE_INVALID_ID;
						uint32_t expected = PROTOTYPE_INVALID_ID;
						uint32_t actual = PROTOTYPE_INVALID_ID;
						for (uint32_t j = 0;
							j < ctx->constraints.classifier_constraint_count;
							++j) {
							const struct operation_constraint* constraint =
								&ctx->constraints.constraints[j];
							if (constraint->source_occurrence != i) {
								continue;
							}
							constraint_id = constraint->id;
							switch (constraint->kind) {
								case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
									reason = PROTOTYPE_COMPILE_DIAGNOSTIC_CONSTRUCTOR_DOMAIN_MISMATCH;
									break;
								case OPERATION_CONSTRAINT_IH_EXPECTED:
									reason = PROTOTYPE_COMPILE_DIAGNOSTIC_IH_OWNERSHIP;
									break;
								case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
									reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MATCH_RESULT_MOTIVE;
									break;
								case OPERATION_CONSTRAINT_CONVERTIBLE:
									reason = PROTOTYPE_COMPILE_DIAGNOSTIC_ASCRIPTION_MISMATCH;
									expected = constraint->payload.classifier.goal.conversion.expected_classifier;
									actual = operation_solver_classifier(
										ctx, constraint->payload.classifier.goal.conversion.body_occurrence
									);
									break;
								default:
									break;
							}
							if (reason != PROTOTYPE_COMPILE_DIAGNOSTIC_UNSOLVED_CLASSIFIER) {
								break;
							}
						}
						(void)add_compile_diagnostic(
							ctx,
							PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
							reason,
							operation->source_ast,
							i,
							constraint_id,
							expected,
							actual,
							operation->source_ast < ctx->asts->node_count ?
								ctx->asts->nodes[operation->source_ast].span :
								(struct prototype_source_span) { 0, 0 }
						);
					}
					return -1;
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

static int compile_phase_record_residual_dependent_folds(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->terms || !ctx->type_declarations) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t computation_occurrence;
		uint32_t continuation_occurrence;
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[operation->core_term].as.computation_fold.clause_count != 0 ||
			compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&computation_occurrence
			) != 0 || compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&continuation_occurrence
			) != 0) {
			continue;
		}
		uint32_t input_classifier = operation_solver_classifier(
			ctx, computation_occurrence
		);
		uint32_t continuation_classifier = operation_solver_classifier(
			ctx, continuation_occurrence
		);
		struct prototype_term_classifier_view input_view;
		uint32_t domain;
		uint32_t classifier_family;
		uint32_t continuation_binder_id;
		uint32_t codomain;
		if (input_classifier == PROTOTYPE_INVALID_ID ||
			continuation_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_judgement_pi_parts(
				ctx->terms, continuation_classifier, &domain, &classifier_family
			) != 0 || !(prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, domain, input_view.result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_pure_family_parts(
				ctx->terms, classifier_family, &continuation_binder_id, &codomain
			) != 0 || !prototype_term_contains_free_binding(
				ctx->terms, codomain, continuation_binder_id
			)) {
			continue;
		}
		struct prototype_term_normalization_result normalized;
		if (prototype_term_normalize_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				ctx->terms->terms[operation->core_term].as.computation_fold.computation,
				ctx->metadata->normalization_step_limit,
				&normalized
			) != 0 || normalized.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			return -1;
		}
		if (UINT64_MAX - ctx->metadata->normalization_steps_used < normalized.steps_used) {
			return -1;
		}
		ctx->metadata->normalization_steps_used += normalized.steps_used;
		if (normalized.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			continue;
		}
		if (normalized.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT &&
			normalized.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED) {
			return -1;
		}
		uint32_t existing_obligation;
		int find_status = prototype_verification_db_find_occurrence(
			&ctx->metadata->verification,
			PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
			operation_id,
			&existing_obligation
		);
		if (find_status < 0) {
			return -1;
		}
		int already_recorded = find_status == 0;
		if (!already_recorded && prototype_verification_db_add(
				&ctx->metadata->verification,
				(struct prototype_verification_obligation){
					.kind = PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
					.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
					.occurrence = operation_id,
					.core_term = operation->core_term,
					.computation_occurrence = computation_occurrence,
					.continuation_occurrence = continuation_occurrence,
					.continuation_binder_id = continuation_binder_id,
					.input_classifier = input_view.result,
					.classifier_family = classifier_family,
					.effect_row = input_view.effect_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
				},
				NULL
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int compile_phase_record_residual_computation_fold_results(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->terms || !ctx->type_declarations) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t computation_occurrence;
		uint32_t return_operation;
		uint32_t return_body_occurrence;
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
			compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&computation_occurrence
			) != 0 || compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&return_operation
			) != 0 || compile_occurrence_child(
				ctx, return_operation, PROTOTYPE_TERM_CHILD_BODY, 0,
				&return_body_occurrence
			) != 0 ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[operation->core_term].as.computation_fold.clause_count == 0) {
			continue;
		}
		uint32_t input_classifier = operation_solver_classifier(
			ctx, computation_occurrence
		);
		uint32_t return_classifier = operation_solver_classifier(
			ctx, return_body_occurrence
		);
		struct prototype_term_classifier_view input_view;
		if (input_classifier == PROTOTYPE_INVALID_ID ||
			return_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			!prototype_term_contains_free_binding(
				ctx->terms, return_classifier, operation->fold_return_binder_id
			)) {
			continue;
		}
		uint32_t classifier_family;
		if (prototype_term_pure_family(
				ctx->terms,
				operation->fold_return_binder_id,
				return_classifier,
				&classifier_family
			) != 0) {
			return -1;
		}
		uint32_t existing_obligation;
		int find_status = prototype_verification_db_find_occurrence(
			&ctx->metadata->verification,
			PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
			operation_id,
			&existing_obligation
		);
		if (find_status < 0) {
			return -1;
		}
		if (find_status > 0 && prototype_verification_db_add(
				&ctx->metadata->verification,
				(struct prototype_verification_obligation){
					.kind = PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
					.state = PROTOTYPE_VERIFICATION_OBLIGATION_PENDING,
					.occurrence = operation_id,
					.core_term = operation->core_term,
					.computation_occurrence = computation_occurrence,
					.continuation_occurrence = return_body_occurrence,
					.continuation_binder_id =
						operation->fold_return_binder_id,
					.input_classifier = input_view.result,
					.classifier_family = classifier_family,
					.effect_row = input_view.effect_row,
					.normalization_profile =
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
				},
				NULL
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_generalize_lambda_effect_rows(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier,
	uint32_t* p_ret
) {
	if (!ctx || !ctx->metadata || !p_ret || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		operation->implicit_effect_row_count > 16) {
		return -1;
	}
	for (uint32_t i = operation->implicit_effect_row_count; i > 0; --i) {
		if (prototype_term_effect_row_forall(
					ctx->terms,
					operation->implicit_effect_row_binders[i - 1],
					classifier,
					&classifier
				) != 0) {
			return -1;
		}
	}
	*p_ret = classifier;
	return 0;
}

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
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		struct operation_solver_solution* solution =
			&ctx->classifier_solver.solutions[i];
		solution->classifier_state = solution->classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CONSTRAINT_STATE_PENDING :
			OPERATION_CONSTRAINT_STATE_SOLVED;
		solution->reason = solution->classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY :
			OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
	}
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
		struct operation_solver_solution* solution =
			&ctx->classifier_solver.solutions[constraint->payload.classifier.classifier_variable];
		if (result->state != OPERATION_CONSTRAINT_STATE_SOLVED) {
			solution->classifier_state = result->state;
			solution->reason = result->reason;
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
		if (constraint->source_occurrence <
			ctx->metadata->typed_occurrences.occurrence_count) {
			ctx->classifier_solver.solutions[
				constraint->source_occurrence
			].effect_state = constraint->state;
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
		uint32_t current = operation_solver_classifier(ctx, operation_id);
		if (!input_is_value && current != PROTOTYPE_INVALID_ID &&
			prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				current, input_value_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			int refine_status = operation_solver_refine_sequenced_ih_result(
				ctx, operation_id, current, p_changed
			);
			if (refine_status < 0) {
				return -1;
			}
			if (refine_status == 0) {
				return 0;
			}
		}
		if (operation_solver_bind_proven_classifier(
				ctx, operation_id, input_value_classifier, p_changed
			) != 0) {
			return -1;
		}
	}
	if (*p_changed) {
		return 0;
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
							if (operation_solver_bind(
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
										operation_solver_refine_sequenced_ih_result(
										ctx,
										constraint->payload.classifier.goal.pi.
											domain_classifier_or_argument_occurrence,
										expected_domain,
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
							}
						}
						if (function_classifier == PROTOTYPE_INVALID_ID ||
							argument_classifier == PROTOTYPE_INVALID_ID) {
							break;
						}
						uint32_t expected_argument_domain;
						uint32_t ignored_argument_family;
						if (prototype_judgement_pi_parts(
								ctx->terms, function_classifier,
								&expected_argument_domain, &ignored_argument_family
							) == 0 && prototype_judgement_classifier_conversion(
								ctx->terms, ctx->type_declarations,
								argument_classifier, expected_argument_domain
							).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
								int refine_status = operation_solver_refine_sequenced_ih_result(
								ctx,
								constraint->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence,
								expected_argument_domain, &pass_changed
							);
								if (refine_status < 0) {
									return -1;
								}
							if (refine_status == 0) {
								argument_classifier = operation_solver_classifier(
									ctx,
									constraint->payload.classifier.goal.pi.
										domain_classifier_or_argument_occurrence
								);
							}
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
					int constructor_status =
						operation_solver_constructor_spine_classifier(
							ctx, target, &classifier, &pass_changed
						);
					if (constructor_status < 0) {
						return -1;
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
						(void)add_compile_diagnostic(
							ctx,
							PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
							PROTOTYPE_COMPILE_DIAGNOSTIC_CONSTRUCTOR_DOMAIN_MISMATCH,
							operation->source_ast,
							target,
							constraint->id,
							PROTOTYPE_INVALID_ID,
							PROTOTYPE_INVALID_ID,
							operation->source_ast < ctx->asts->node_count ?
								ctx->asts->nodes[operation->source_ast].span :
								(struct prototype_source_span) { 0, 0 }
						);
						return -1;
					}
					if (constructor_status == 0 && operation_solver_bind(
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

static int compile_phase_infer_general_classifiers(
	struct compile_context* ctx,
	int require_complete
) {
	if (!ctx) {
		return -1;
	}
	return operation_solver_solve(ctx, require_complete);
}

static size_t count_classified_operations(const struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return 0;
	}
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
	uint32_t* p_effect_row
);

static int operation_solver_reindex_in_enclosing_indexed_branches(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t term,
	uint32_t* p_reindexed
) {
	if (!ctx || !p_reindexed || term >= ctx->terms->term_count ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph =
		prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
	uint64_t dependency_signature = UINT64_C(1469598103934665603);
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
			match_operation->first_case + match_operation->case_count >
				ctx->metadata->typed_occurrences.case_count) {
			continue;
		}
		for (uint32_t case_index = 0;
			case_index < match_operation->case_count; ++case_index) {
			uint32_t branch_operation;
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[
					match_operation->first_case + case_index
				];
			uint32_t ignored_path[512];
			uint32_t ignored_path_count;
			if (compile_occurrence_child(
					ctx, match_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
					case_index, &branch_operation
				) != 0 || prototype_context_extension_path(
					&ctx->metadata->contexts, operation_case->context_id,
					context_id, ignored_path, 512, &ignored_path_count
				) != 0 || prototype_typed_occurrence_graph_reaches(
					graph, ctx->terms, branch_operation, operation_id
				) <= 0) {
				continue;
			}
			dependency_signature ^= match_id;
			dependency_signature *= UINT64_C(1099511628211);
			dependency_signature ^= operation_solver_classifier(
				ctx, scrutinee_operation
			);
			dependency_signature *= UINT64_C(1099511628211);
			struct operation_constraint* pending_refinement =
				operation_solver_branch_refinement_constraint(
					ctx, match_operation->first_case + case_index
				);
			uint32_t refinement_action =
				prototype_typed_occurrence_match_case_is_solved(operation_case) ?
					operation_case->refinement_substitution :
					(pending_refinement && pending_refinement->payload.
						branch_refinement.refinement_status ==
							PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
						pending_refinement->payload.branch_refinement.substitution :
						PROTOTYPE_INVALID_ID);
			dependency_signature ^= refinement_action;
			dependency_signature *= UINT64_C(1099511628211);
			for (uint32_t binder = 0;
				binder < operation_case->binder_count;
				++binder) {
				uint32_t binding_context;
				if (operation_case->binder_ids[binder] == PROTOTYPE_INVALID_ID ||
					prototype_context_find_binding(
						&ctx->metadata->contexts,
						operation_case->context_id,
						operation_case->binder_ids[binder],
						&binding_context
					) != 0) {
					return -1;
				}
				const struct prototype_context* binding = prototype_context_get(
					&ctx->metadata->contexts, binding_context
				);
				if (!binding) {
					return -1;
				}
				dependency_signature ^=
					prototype_context_classifier_term(binding);
				dependency_signature *= UINT64_C(1099511628211);
			}
		}
	}
	uint32_t cache_hash = operation_id * UINT32_C(2654435761) ^
		context_id * UINT32_C(2246822519) ^ term * UINT32_C(3266489917);
	uint32_t cache_slot = cache_hash % OPERATION_REINDEX_CACHE_CAPACITY;
	const struct operation_reindex_cache_entry* cached =
		&ctx->operation_reindex_cache[cache_slot];
	if (cached->occupied && cached->occurrence_id == operation_id &&
		cached->context_id == context_id && cached->term == term &&
		cached->dependency_signature == dependency_signature) {
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
			) == 0 ? prototype_typed_occurrence_graph_reaches(
				graph, ctx->terms, branch_operation, operation_id
			) : -1;
			if (operation_case->constructor_id == PROTOTYPE_INVALID_ID ||
				context_path_status != 0 || operation_path_status == 0) {
				continue;
			}
			if (operation_path_status < 0) {
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
				if (!refinement ||
					refinement->source_context != operation_case->context_id) {
					return -1;
				}
				refinement_substitution = operation_case->refinement_substitution;
				refinement_status = PROTOTYPE_INDEX_REFINEMENT_SOLVED;
			} else {
				struct operation_constraint* refinement =
					operation_solver_branch_refinement_constraint(
						ctx, match_operation->first_case + case_index
					);
				if (refinement) {
					refinement_status = refinement->payload.branch_refinement.
						refinement_status;
					refinement_substitution = refinement->payload.
						branch_refinement.substitution;
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
				return -1;
			}
		}
	}
	*p_reindexed = current;
	/* TermDB nodes are immutable once interned. Cache against the enclosing
	 * indexed branches that can change this action and overwrite collisions. */
	struct operation_reindex_cache_entry* entry =
		&ctx->operation_reindex_cache[cache_slot];
	entry->occurrence_id = operation_id;
	entry->context_id = context_id;
	entry->term = term;
	entry->reindexed = current;
	entry->dependency_signature = dependency_signature;
	entry->occupied = 1;
	return 0;
}

static int operation_solver_indexed_branch_endomorphism(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t* p_substitution
) {
	if (!ctx || !p_substitution || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	uint32_t path[512];
	uint32_t path_count;
	if (prototype_context_extension_path(
			&ctx->metadata->contexts,
			prototype_context_empty(&ctx->metadata->contexts),
			context_id,
			path,
			512,
			&path_count
		) != 0) {
		return -1;
	}
	uint32_t arguments[512];
	for (uint32_t i = 0; i < path_count; ++i) {
		const struct prototype_context* entry = prototype_context_get(
			&ctx->metadata->contexts, path[i]
		);
		uint32_t variable;
		if (!entry || entry->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(ctx->terms, entry->binding_id, &variable) != 0 ||
			operation_solver_reindex_in_enclosing_indexed_branches(
				ctx, operation_id, context_id, variable, &arguments[i]
			) != 0) {
			return -1;
		}
	}
	return prototype_context_substitution_from_terms(
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		ctx->terms,
		ctx->type_declarations,
		context_id,
		context_id,
		arguments,
		path_count,
		p_substitution
	);
}

static int operation_solver_branch_subtree_reaches_at_depth(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
) {
	if (!graph || !terms || !visited || root_operation >= graph->occurrence_count ||
		target_operation >= graph->occurrence_count) {
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
		&graph->occurrences[root_operation];
	/* A reference occurrence belongs to the branch, but its wrapped definition
	 * does not. Pulling the definition body into the branch Context would mutate
	 * a separately named source occurrence. Expected-type wrappers, by contrast,
	 * are local elaboration nodes and remain part of the branch subtree. */
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
		return 0;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		int wrapped = operation_solver_branch_subtree_reaches_at_depth(
			graph, terms, operation->wrapped_occurrence, target_operation, visited
		);
		if (wrapped != 0) {
			return wrapped;
		}
	}
	uint32_t child_count;
	if (prototype_typed_occurrence_core_child_count(
			graph, terms, root_operation, &child_count
		) != 0) {
		return -1;
	}
	for (uint32_t child_index = 0; child_index < child_count; ++child_index) {
		uint32_t child;
		if (prototype_typed_occurrence_core_child(
				graph, terms, root_operation, child_index, &child
			) != 0) {
			return -1;
		}
		int reaches = operation_solver_branch_subtree_reaches_at_depth(
			graph, terms, child, target_operation, visited
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	return 0;
}

static int operation_solver_branch_subtree_reaches(
	const struct prototype_typed_occurrence_graph* graph,
	const struct prototype_term_db* terms,
	uint32_t root_operation,
	uint32_t target_operation
) {
	if (!graph || root_operation >= graph->occurrence_count ||
		target_operation >= graph->occurrence_count) {
		return -1;
	}
	uint8_t visited[graph->occurrence_count];
	memset(visited, 0, sizeof(visited));
	return operation_solver_branch_subtree_reaches_at_depth(
		graph, terms, root_operation, target_operation, visited
	);
}

static int operation_solver_occurrence_is_in_impossible_branch(
	const struct compile_context* ctx,
	uint32_t occurrence_id
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_graph* graph =
		&ctx->metadata->typed_occurrences;
	for (uint32_t match_id = 0; match_id < graph->occurrence_count; ++match_id) {
		const struct prototype_typed_occurrence* match = &graph->occurrences[match_id];
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			match->first_case + match->case_count > graph->case_count) {
			continue;
		}
		for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
			const struct prototype_typed_occurrence_match_case* operation_case =
				&graph->cases[match->first_case + case_index];
			if (operation_case->refinement_status !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
				continue;
			}
			uint32_t branch_operation;
			if (prototype_typed_occurrence_graph_child(
					graph,
					match_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
					case_index,
					&branch_operation
				) != 0) {
				return -1;
			}
			int reaches = operation_solver_branch_subtree_reaches(
				graph, ctx->terms, branch_operation, occurrence_id
			);
			if (reaches != 0) {
				return reaches;
			}
		}
	}
	return 0;
}

static int operation_solver_pullback_branch_operations(
	struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t source_context,
	uint32_t refined_context,
	uint32_t refinement_substitution
) {
	if (!ctx || !ctx->metadata ||
		root_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	struct prototype_typed_occurrence_graph* graph =
		prototype_compile_metadata_typed_occurrences(ctx->metadata);
	uint8_t reachable_operations[graph->occurrence_count];
	uint8_t reachable_cases[graph->case_count == 0 ? 1 : graph->case_count];
	for (uint32_t operation_id = 0;
		operation_id < graph->occurrence_count; ++operation_id) {
		int reaches = operation_solver_branch_subtree_reaches(
			graph, ctx->terms, root_operation, operation_id
		);
		if (reaches < 0) {
			fprintf(stderr,
				"branch pullback reachability invalid root=%u operation=%u\n",
				root_operation, operation_id);
			return -1;
		}
		reachable_operations[operation_id] = reaches > 0;
	}
	for (uint32_t case_id = 0; case_id < graph->case_count; ++case_id) {
		uint32_t owner_match = PROTOTYPE_INVALID_ID;
		for (uint32_t operation_id = 0;
			operation_id < graph->occurrence_count; ++operation_id) {
			const struct prototype_typed_occurrence* candidate =
				&graph->occurrences[operation_id];
			if (candidate->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
				case_id >= candidate->first_case &&
				case_id - candidate->first_case < candidate->case_count) {
				owner_match = operation_id;
				break;
			}
		}
		if (owner_match == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		reachable_cases[case_id] = reachable_operations[owner_match];
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count; ++operation_id) {
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t path[256];
		uint32_t path_count;
		if (!reachable_operations[operation_id] || prototype_context_extension_path(
				&ctx->metadata->contexts, source_context,
				operation->context_id, path, 256, &path_count
			) != 0) {
			continue;
		}
		uint32_t target_context = refined_context;
		uint32_t action = refinement_substitution;
		if (operation->context_id != source_context &&
			prototype_context_pullback_occurrence_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				source_context,
				operation->context_id,
				refinement_substitution,
				&target_context,
				&action
			) != 0) {
			fprintf(stderr,
				"branch pullback telescope failed root=%u operation=%u source=%u operation-context=%u substitution=%u\n",
				root_operation, operation_id, source_context,
				operation->context_id, refinement_substitution);
			return -1;
		}
		uint32_t classifier = operation_available_classifier(ctx, operation_id);
		uint32_t known_classifier = ctx->classifier_hints[operation_id];
		uint32_t binder_classifier = operation->binder_classifier;
		int classifier_is_structural_seed =
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS;
		if (!classifier_is_structural_seed && operation->tag !=
				PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			/* classifier_hints is also used as a cache by the fixed-point solver.
			 * For a derived occurrence it is therefore not independent evidence
			 * that may survive a Context action. */
			known_classifier = PROTOTYPE_INVALID_ID;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			classifier == PROTOTYPE_INVALID_ID &&
			known_classifier == PROTOTYPE_INVALID_ID &&
			operation->core_term < ctx->terms->term_count &&
			ctx->terms->terms[operation->core_term].tag == PROTOTYPE_TERM_VAR) {
			uint32_t binding_context;
			if (prototype_context_find_binding(
					&ctx->metadata->contexts,
					operation->context_id,
					ctx->terms->terms[
						operation->core_term
					].as.var.binding_id,
					&binding_context
				) != 0) {
				return -1;
			}
			const struct prototype_context* binding = prototype_context_get(
				&ctx->metadata->contexts, binding_context
			);
			if (!binding) {
				return -1;
			}
			uint32_t binding_classifier =
				prototype_context_classifier_term(binding);
			if (binding_classifier != PROTOTYPE_INVALID_ID) {
				classifier = binding_classifier;
				known_classifier = classifier;
			}
		}
		uint32_t source_classifier = known_classifier != PROTOTYPE_INVALID_ID ?
			known_classifier : classifier_is_structural_seed ?
				classifier : PROTOTYPE_INVALID_ID;
		int has_source_classifier =
			source_classifier != PROTOTYPE_INVALID_ID;
		uint32_t effect_row =
			ctx->classifier_solver.solutions[operation_id].effect_row;
		uint32_t projected_core = operation->core_term;
#define REINDEX_OPERATION_FIELD(field) \
	do { \
		if ((field) != PROTOTYPE_INVALID_ID && prototype_term_reindex( \
				ctx->terms, ctx->type_declarations, \
				&ctx->metadata->contexts, &ctx->metadata->substitutions, \
				(field), action, &(field) \
			) != 0) { \
			fprintf(stderr, \
				"branch pullback term reindex failed root=%u operation=%u field=%s term=%u action=%u source=%u target=%u\n", \
				root_operation, operation_id, #field, (field), action, \
				source_context, target_context); \
			return -1; \
		} \
	} while (0)
		REINDEX_OPERATION_FIELD(classifier);
		REINDEX_OPERATION_FIELD(known_classifier);
		REINDEX_OPERATION_FIELD(binder_classifier);
		REINDEX_OPERATION_FIELD(effect_row);
		REINDEX_OPERATION_FIELD(projected_core);
#undef REINDEX_OPERATION_FIELD
		for (uint32_t check_id = 0;
			check_id < ctx->pending_ascription_check_count;
			++check_id) {
			struct pending_ascription_check* check =
				&ctx->pending_ascription_checks[check_id];
			if (check->operation != operation_id) {
				continue;
			}
			if (prototype_term_reindex(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					check->subject,
					action,
					&check->subject
				) != 0 || prototype_term_reindex(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					check->expected_classifier,
					action,
					&check->expected_classifier
				) != 0) {
				return -1;
			}
		}
		int rebuilt_in_target_context = 0;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR) {
			int saturated;
			int constructor_status =
				prototype_judgement_constructor_spine_classifier(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					target_context,
					projected_core,
					PROTOTYPE_INVALID_ID,
					NULL,
					0,
					&classifier,
					&saturated
				);
			if (constructor_status !=
					PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID) {
				return -1;
			}
			(void)saturated;
			known_classifier = classifier;
			rebuilt_in_target_context = 1;
		}
		if (rebuilt_in_target_context) {
			operation->source_core_term = PROTOTYPE_INVALID_ID;
			operation->source_classifier = PROTOTYPE_INVALID_ID;
			action = PROTOTYPE_INVALID_ID;
		} else if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
			uint32_t composed;
			if (prototype_substitution_compose(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					operation->context_action_substitution,
					action,
					&composed
				) != 0) {
				fprintf(stderr,
					"branch pullback action composition failed root=%u operation=%u existing=%u action=%u\n",
					root_operation, operation_id,
					operation->context_action_substitution, action);
				return -1;
			}
			action = composed;
		} else if (has_source_classifier) {
			operation->source_core_term = operation->core_term;
			operation->source_classifier = source_classifier;
		} else {
			/* Preserve source topology so the enclosing Core node can validate its
			 * child projection. No source typing derivation existed yet, so the
			 * final classifier is established directly in target_context. */
			operation->source_core_term = operation->core_term;
			operation->source_classifier = PROTOTYPE_INVALID_ID;
		}
		if (!rebuilt_in_target_context && !classifier_is_structural_seed) {
			/* APP/Lambda/Match and CBPV boundary classifiers are consequences of
			 * their child occurrences. Reindexing a provisional parent preserves
			 * stale child choices, so retain only an authoritative hint and let the
			 * ordinary constraints rebuild every other derived classifier. */
			classifier = known_classifier;
			effect_row = PROTOTYPE_INVALID_ID;
		}
		operation->context_id = target_context;
		operation->context_action_substitution = action;
		operation->core_term = projected_core;
		ctx->classifier_hints[operation_id] = known_classifier;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
			operation->binder_classifier != binder_classifier) {
			operation_context_binding_dependency_changed(
				ctx, operation->binding_id
			);
		}
		operation->binder_classifier = binder_classifier;
		ctx->classifier_solver.solutions[operation_id].classifier = classifier;
		ctx->classifier_solver.solutions[operation_id].effect_row = effect_row;
		ctx->classifier_solver.solutions[operation_id].classifier_state =
			classifier == PROTOTYPE_INVALID_ID ?
				OPERATION_CONSTRAINT_STATE_PENDING :
				OPERATION_CONSTRAINT_STATE_SOLVED;
		ctx->classifier_solver.solutions[operation_id].reason =
			classifier == PROTOTYPE_INVALID_ID ?
				OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY :
				OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
	}
	for (uint32_t case_id = 0;
		case_id < ctx->metadata->typed_occurrences.case_count; ++case_id) {
		struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[case_id];
		uint32_t owner_match = PROTOTYPE_INVALID_ID;
		for (uint32_t operation_id = 0;
			operation_id < ctx->metadata->typed_occurrences.occurrence_count; ++operation_id) {
			const struct prototype_typed_occurrence* candidate =
				&ctx->metadata->typed_occurrences.occurrences[operation_id];
			if (candidate->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
				case_id >= candidate->first_case &&
				case_id - candidate->first_case < candidate->case_count) {
				owner_match = operation_id;
				break;
			}
		}
		uint32_t path[256];
		uint32_t path_count;
		if (owner_match == PROTOTYPE_INVALID_ID || !reachable_cases[case_id] ||
			prototype_context_extension_path(
				&ctx->metadata->contexts, source_context,
				operation_case->context_id, path, 256, &path_count
			) != 0) {
			continue;
		}
		uint32_t target_context = refined_context;
		uint32_t action = refinement_substitution;
		if (operation_case->context_id != source_context &&
			prototype_context_pullback_occurrence_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				source_context,
				operation_case->context_id,
				refinement_substitution,
				&target_context,
				&action
			) != 0) {
			return -1;
		}
		uint32_t owner = operation_case->constructor_owner;
		if (owner != PROTOTYPE_INVALID_ID && prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				owner, action, &owner
			) != 0) {
			return -1;
		}
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			uint32_t composed_refinement;
			if (operation_case->refinement_substitution ==
					PROTOTYPE_INVALID_ID || prototype_substitution_compose(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					operation_case->refinement_substitution,
					action,
					&composed_refinement
				) != 0) {
				return -1;
			}
			operation_case->refinement_substitution = composed_refinement;
		}
		operation_case->context_id = target_context;
		operation_case->constructor_owner = owner;
	}
	/* An outer pullback changes a whole nested branch subtree at once. Rebuild
	 * every mutable branch constraint from its occurrence/case authority so no
	 * payload observes a partially transformed graph. */
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
			constraint->source_occurrence >= graph->occurrence_count ||
			match_id >= graph->occurrence_count ||
			constraint->payload.branch_refinement.case_index >=
				graph->occurrences[match_id].case_count) {
			return -1;
		}
		uint32_t case_id = graph->occurrences[match_id].first_case +
			constraint->payload.branch_refinement.case_index;
		if (case_id >= graph->case_count) {
			return -1;
		}
		const struct prototype_typed_occurrence_match_case* operation_case =
			&graph->cases[case_id];
		constraint->context_id =
			graph->occurrences[constraint->source_occurrence].context_id;
		if (!prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			continue;
		}
		const struct prototype_substitution* branch_action =
			prototype_substitution_get(
				&ctx->metadata->substitutions,
				operation_case->refinement_substitution
			);
		if (!branch_action || branch_action->source_context !=
				operation_case->context_id) {
			return -1;
		}
		constraint->payload.branch_refinement.source_context =
			branch_action->target_context;
		constraint->payload.branch_refinement.refined_context =
			branch_action->source_context;
		constraint->payload.branch_refinement.substitution =
			operation_case->refinement_substitution;
		constraint->payload.branch_refinement.refinement_status =
			PROTOTYPE_INDEX_REFINEMENT_SOLVED;
	}
	ctx->classifier_solver.solution_revision++;
	return 0;
}

static int operation_solver_occurrence_waits_for_branch_action(
	const struct compile_context* ctx,
	uint32_t occurrence_id
) {
	if (!ctx || !ctx->metadata || occurrence_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		uint32_t match_id =
			constraint->payload.branch_refinement.match_occurrence;
		uint32_t case_id = match_id <
			ctx->metadata->typed_occurrences.occurrence_count ?
			ctx->metadata->typed_occurrences.occurrences[match_id].first_case +
				constraint->payload.branch_refinement.case_index :
			PROTOTYPE_INVALID_ID;
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			constraint->payload.branch_refinement.refinement_status !=
				PROTOTYPE_INDEX_REFINEMENT_SOLVED ||
			case_id >= ctx->metadata->typed_occurrences.case_count ||
			prototype_typed_occurrence_match_case_is_solved(
				&ctx->metadata->typed_occurrences.cases[case_id]
			)) {
			continue;
		}
		int reaches = operation_solver_branch_subtree_reaches(
			&ctx->metadata->typed_occurrences,
			ctx->terms,
			constraint->source_occurrence,
			occurrence_id
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

static struct operation_constraint* operation_solver_branch_refinement_constraint(
	struct compile_context* ctx,
	uint32_t operation_case_id
) {
	if (!ctx || !ctx->metadata || operation_case_id >=
		ctx->metadata->typed_occurrences.case_count) {
		return NULL;
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
	const struct compile_context* ctx,
	uint32_t match_id
) {
	if (!ctx || !ctx->metadata || match_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		const struct operation_constraint* enclosing =
			&ctx->constraints.constraints[id];
		uint32_t enclosing_match =
			enclosing->payload.branch_refinement.match_occurrence;
		if (enclosing->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			enclosing_match == match_id || enclosing_match >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			continue;
		}
		const struct prototype_typed_occurrence* owner =
			&ctx->metadata->typed_occurrences.occurrences[enclosing_match];
		uint32_t case_id = owner->first_case +
			enclosing->payload.branch_refinement.case_index;
		if (owner->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			case_id >= ctx->metadata->typed_occurrences.case_count) {
			return -1;
		}
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[case_id];
		if (prototype_typed_occurrence_match_case_is_solved(operation_case) ||
			operation_case->refinement_status ==
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
			continue;
		}
		int reaches = operation_solver_branch_subtree_reaches(
			&ctx->metadata->typed_occurrences,
			ctx->terms,
			enclosing->source_occurrence,
			match_id
		);
		if (reaches < 0) {
			return -1;
		}
		if (reaches > 0) {
			/* Refinement substitutions compose from outer Contexts inward.
			 * Solving the nested equation first would leave its case action
			 * rooted in the pre-refinement telescope. */
			return 1;
		}
	}
	return 0;
}

static int operation_solver_solve_branch_refinement_constraints(
	struct compile_context* ctx,
	int* p_graph_changed
) {
	if (!ctx || !ctx->metadata || !p_graph_changed) {
		return -1;
	}
	*p_graph_changed = 0;
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
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
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			continue;
		}
		if (operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int waits_for_enclosing =
			operation_solver_waits_for_enclosing_branch_refinement(ctx, match_id);
		if (waits_for_enclosing < 0) {
			return -1;
		}
		if (waits_for_enclosing > 0) {
			continue;
		}
		uint32_t scrutinee_classifier = operation_solver_classifier(
			ctx, scrutinee_id
		);
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
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
		int status = prototype_judgement_source_match_branch_refinement(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			match->context_id,
			ctx->metadata->typed_occurrences.occurrences[scrutinee_id].core_term,
			scrutinee_classifier,
			operation_case->constructor_id,
			operation_case->context_id,
			occurrence_binders,
			term_case->binder_count,
			&refined_context,
			&substitution,
			&constructor_term,
			&residual_pattern,
			&residual_value
		);
		constraint->payload.branch_refinement.refinement_status = status;
		constraint->payload.branch_refinement.refined_context = refined_context;
		constraint->payload.branch_refinement.substitution = substitution;
		constraint->payload.branch_refinement.constructor_term = constructor_term;
		constraint->payload.branch_refinement.residual_pattern = residual_pattern;
		constraint->payload.branch_refinement.residual_value = residual_value;
		constraint->result_term = constructor_term;
		if (status == PROTOTYPE_INDEX_REFINEMENT_INVALID) {
			fprintf(stderr,
				"branch refinement invalid constraint=%u match=%u case=%u "
				"scrutinee=%u match-context=%u branch-context=%u "
				"scrutinee-classifier=%u constructor=%u\n",
				constraint->id, match_id, case_index, scrutinee_id,
				match->context_id, operation_case->context_id,
				scrutinee_classifier, operation_case->constructor_id);
			(void)operation_constraint_transition(
				constraint,
				OPERATION_CONSTRAINT_STATE_CONTRADICTION,
				OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_INVALID
			);
			return -1;
		}
		if (status == PROTOTYPE_INDEX_REFINEMENT_RESIDUAL) {
			operation_case->refinement_status =
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_RESIDUAL;
			if (operation_constraint_transition(
					constraint,
					OPERATION_CONSTRAINT_STATE_RESIDUAL,
					OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_RESIDUAL
				) != 0) {
				return -1;
			}
			continue;
		}
		if (status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
			operation_case->refinement_status =
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE;
			if (operation_constraint_transition(
					constraint,
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
			return -1;
		}
		uint32_t branch_operation = constraint->source_occurrence;
		uint32_t source_context = operation_case->context_id;
		if (operation_solver_pullback_branch_operations(
				ctx,
				branch_operation,
				source_context,
				refined_context,
				substitution
			) != 0) {
			fprintf(stderr,
				"branch refinement occurrence pullback failed id=%u branch=%u source=%u refined=%u substitution=%u\n",
				id, branch_operation, source_context, refined_context,
				substitution);
			return -1;
		}
		uint32_t refined_constructor_owner = operation_case->constructor_owner;
		if (refined_constructor_owner != PROTOTYPE_INVALID_ID &&
			prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				refined_constructor_owner,
				substitution,
				&refined_constructor_owner
			) != 0) {
			fprintf(stderr,
				"branch refinement owner reindex failed id=%u owner=%u substitution=%u\n",
				id, operation_case->constructor_owner, substitution);
			return -1;
		}
		operation_case->context_id = refined_context;
		operation_case->constructor_owner = refined_constructor_owner;
		operation_case->refinement_status =
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED;
		operation_case->refinement_substitution = substitution;
		constraint->payload.branch_refinement.source_context =
			solved_action->target_context;
		constraint->context_id = refined_context;
		constraint->state = OPERATION_CONSTRAINT_STATE_SOLVED;
		constraint->reason =
			OPERATION_CLASSIFIER_GOAL_REASON_BRANCH_REFINEMENT_VALIDATED;
		*p_graph_changed = 1;
		/* The pullback changes every nested occurrence Context. Regenerate all
		 * constraint origins before considering another branch. */
		return 0;
	}
	return 0;
}

static int operation_solver_join_match_classifiers(
	struct compile_context* ctx,
	uint32_t left,
	uint32_t right,
	uint32_t* p_joined
);

static int build_operation_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count != operation->case_count ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count) {
		return 1;
	}
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t binder_cursor = 0;
	int returns_computation = -1;
	uint32_t joined_effect_row = PROTOTYPE_INVALID_ID;
	uint32_t branch_results[64];
	uint32_t branch_constructor_results[64];
	uint32_t branch_constructor_terms[64];
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, typing->operation, case_index
			);
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[match->as.match.first_case + case_index];
		const struct prototype_typed_occurrence_match_case* source_operation_case =
			&ctx->metadata->typed_occurrences.cases[
				operation->first_case + case_index
			];
		if (!equation || equation->payload.classifier.goal.motive_case.branch_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
			source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256 ||
			source_operation_case->binder_count != source_case->binder_count) {
			return 1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.solutions[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier;
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		int branch_is_refined = prototype_typed_occurrence_match_case_is_solved(source_operation_case);
		if (source_case->binder_count > 64) {
			return -1;
		}
		if (branch_is_refined) {
			reindexed_binder_count = source_operation_case->binder_count;
			motive_case_context = source_operation_case->context_id;
			for (uint32_t i = 0; i < reindexed_binder_count; ++i) {
				reindexed_binders[i] = source_operation_case->binder_ids[i];
			}
			if (prototype_substitution_identity(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					motive_case_context,
					&case_substitution
				) != 0) {
				return -1;
			}
		} else if (prototype_context_reindex_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				operation->context_id,
				ctx->metadata->typed_occurrences.cases[
					operation->first_case + case_index
				].context_id,
				reindexed_binders,
				64,
				&reindexed_binder_count,
				&motive_case_context,
				&case_substitution
			) != 0) {
			return -1;
		}
		if (reindexed_binder_count != source_case->binder_count) {
			return -1;
		}
		(void)motive_case_context;
		for (uint32_t i = 0; i < source_case->binder_count; ++i) {
			motive_binders[binder_cursor + i].binding_id =
				reindexed_binders[i];
			motive_binders[binder_cursor + i].is_recursive =
				ctx->terms->case_binders[
					source_case->first_binder + i
				].is_recursive;
		}
		motive_cases[case_index].case_label_symbol_id =
			ctx->terms->case_label_symbols[match->as.match.first_case + case_index];
		motive_cases[case_index].constructor_owner = source_case->constructor_owner;
		motive_cases[case_index].constructor_id = source_case->constructor_id;
		motive_cases[case_index].binders = &motive_binders[binder_cursor];
		motive_cases[case_index].binder_count = source_case->binder_count;
		uint32_t reindexed_classifier;
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				branch_classifier,
				case_substitution,
				&reindexed_classifier
			) != 0) {
			return -1;
		}
		int branch_returns;
		uint32_t branch_result;
		uint32_t branch_effect_row;
		if (operation_solver_match_classifier_parts(
				ctx,
				reindexed_classifier,
				&branch_returns,
				&branch_result,
				&branch_effect_row
			) != 0 || (returns_computation >= 0 &&
			branch_returns != returns_computation)) {
			return 1;
		}
		returns_computation = branch_returns;
		motive_cases[case_index].body = branch_result;
		branch_results[case_index] = branch_result;
		uint32_t constructor_term;
		uint32_t argument_classifiers[64];
		uint32_t constructor_classifier;
		int constructor_saturated;
		if (prototype_term_constructor(
				ctx->terms, source_case->constructor_owner,
				source_case->constructor_id, &constructor_term
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < source_case->binder_count; ++i) {
			uint32_t field;
			uint32_t binding_context;
			const struct prototype_context* binding_entry;
			if (prototype_term_var(
					ctx->terms,
					source_operation_case->binder_ids[i],
					&field
				) != 0 || prototype_term_app(
					ctx->terms, constructor_term, field, &constructor_term
				) != 0 || prototype_context_find_binding(
					&ctx->metadata->contexts,
					ctx->metadata->typed_occurrences.cases[
						operation->first_case + case_index
					].context_id,
					source_operation_case->binder_ids[i],
					&binding_context
				) != 0 || !(binding_entry = prototype_context_get(
					&ctx->metadata->contexts, binding_context
				))) {
				return -1;
			}
			argument_classifiers[i] = prototype_context_classifier_term(binding_entry);
		}
		if (prototype_judgement_constructor_spine_classifier(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				ctx->metadata->typed_occurrences.cases[
					operation->first_case + case_index
				].context_id,
				constructor_term, source_case->constructor_owner,
				argument_classifiers, source_case->binder_count,
				&constructor_classifier, &constructor_saturated
			) != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID ||
			!constructor_saturated || prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				constructor_term, case_substitution,
				&branch_constructor_terms[case_index]
			) != 0 || prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				constructor_classifier, case_substitution,
				&branch_constructor_results[case_index]
			) != 0) {
			return -1;
		}
		if (branch_returns) {
			if (joined_effect_row == PROTOTYPE_INVALID_ID) {
				joined_effect_row = branch_effect_row;
			} else if (prototype_term_effect_row_union(
					ctx->terms,
					joined_effect_row,
					branch_effect_row,
					&joined_effect_row
				) != 0) {
				return -1;
			}
		}
		binder_cursor += source_case->binder_count;
	}
	/* Infer an ordinary dependent motive by abstracting each branch's saturated
	 * constructor value from its result classifier. This covers motives such as
	 * `\n => LE n n` and `\xs => Result (length xs)` without constraining the
	 * result family to have the scrutinee family's index arity. Reinstantiation
	 * is checked before the inferred motive is accepted. */
	uint32_t motive_binding = prototype_term_new_binding(ctx->terms);
	uint32_t motive_var;
	uint32_t generalized_result = PROTOTYPE_INVALID_ID;
	int generalized_motive = motive_binding != PROTOTYPE_INVALID_ID &&
		prototype_term_var(ctx->terms, motive_binding, &motive_var) == 0;
	for (uint32_t i = 0; generalized_motive && i < operation->case_count; ++i) {
		uint32_t generalized_branch;
		uint32_t reinstantiated_branch;
		if (prototype_term_graph_replace_exact(
				ctx->terms, ctx->type_declarations, branch_results[i],
				branch_constructor_terms[i], motive_var, &generalized_branch
			) != 0 || generalized_branch == branch_results[i] ||
			prototype_term_graph_substitute_bound_var(
				ctx->terms, ctx->type_declarations, generalized_branch,
				motive_binding, branch_constructor_terms[i],
				&reinstantiated_branch
			) != 0 || prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				reinstantiated_branch, branch_results[i]
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			generalized_motive = 0;
			break;
		}
		if (generalized_result == PROTOTYPE_INVALID_ID) {
			generalized_result = generalized_branch;
		} else if (prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				generalized_result, generalized_branch
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			generalized_motive = 0;
		}
	}
	if (generalized_motive && generalized_result != PROTOTYPE_INVALID_ID) {
		uint32_t motive_body = generalized_result;
		uint32_t motive;
		if (returns_computation && prototype_term_computation_type(
				ctx->terms, joined_effect_row, generalized_result, &motive_body
			) != 0) {
			return -1;
		}
		if (prototype_term_lambda(
				ctx->terms, motive_binding, motive_body, &motive
			) != 0 || prototype_term_app(
				ctx->terms, motive, match->as.match.scrutinee, p_classifier
			) != 0) {
			return -1;
		}
		return 0;
	}
	uint32_t scrutinee_operation;
	if (compile_occurrence_child(
			ctx, typing->operation, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) != 0) {
		return -1;
	}
	uint32_t scrutinee_classifier = operation_solver_classifier(
		ctx, scrutinee_operation
	);
	uint32_t scrutinee_type;
	uint32_t scrutinee_arguments[32];
	uint32_t scrutinee_argument_count;
	int has_indexed_scrutinee = scrutinee_classifier != PROTOTYPE_INVALID_ID &&
		prototype_term_type_instance_info(
			ctx->terms, scrutinee_classifier, &scrutinee_type,
			scrutinee_arguments, &scrutinee_argument_count
		) == 0 && scrutinee_type < ctx->type_declarations->type_count;
	uint32_t index_count = has_indexed_scrutinee ?
		ctx->type_declarations->type_declarations[scrutinee_type].index_count : 0;
	uint32_t scrutinee_parameter_count = has_indexed_scrutinee ?
		ctx->type_declarations->type_declarations[scrutinee_type].parameter_count : 0;

	/* A result family may place a scrutinee index in either a declared parameter
	 * or index position. Infer the dependency from branch equations themselves:
	 * each varying result argument must coincide with one scrutinee-constructor
	 * index in every branch. */
	if (has_indexed_scrutinee && index_count > 0) {
		uint32_t result_type = PROTOTYPE_INVALID_ID;
		uint32_t result_argument_count = 0;
		uint32_t branch_result_arguments[64][32];
		uint32_t branch_constructor_arguments[64][32];
		int result_instance_motive = operation->case_count != 0;
		for (uint32_t branch = 0;
			result_instance_motive && branch < operation->case_count; ++branch) {
			uint32_t branch_result_type;
			uint32_t branch_result_count;
			uint32_t constructor_type;
			uint32_t constructor_count;
			if (prototype_term_type_instance_info(
					ctx->terms, branch_results[branch], &branch_result_type,
					branch_result_arguments[branch], &branch_result_count
				) != 0 || prototype_term_type_instance_info(
					ctx->terms, branch_constructor_results[branch], &constructor_type,
					branch_constructor_arguments[branch], &constructor_count
				) != 0 || constructor_type != scrutinee_type ||
				constructor_count != scrutinee_argument_count ||
				branch_result_count > 32 || (branch != 0 &&
				 (branch_result_type != result_type ||
				  branch_result_count != result_argument_count))) {
				result_instance_motive = 0;
				break;
			}
			result_type = branch_result_type;
			result_argument_count = branch_result_count;
		}
		uint32_t index_bindings[32];
		uint32_t index_variables[32];
		for (uint32_t index = 0;
			result_instance_motive && index < index_count; ++index) {
			index_bindings[index] = prototype_term_new_binding(ctx->terms);
			if (index_bindings[index] == PROTOTYPE_INVALID_ID || prototype_term_var(
					ctx->terms, index_bindings[index], &index_variables[index]
				) != 0) {
				return -1;
			}
		}
		uint32_t generic_result_arguments[32];
		for (uint32_t argument = 0;
			result_instance_motive && argument < result_argument_count; ++argument) {
			int constant = 1;
			for (uint32_t branch = 0;
				constant && branch < operation->case_count; ++branch) {
				for (uint32_t binder = 0;
					binder < motive_cases[branch].binder_count; ++binder) {
					if (prototype_term_contains_free_binding(
							ctx->terms,
							branch_result_arguments[branch][argument],
							motive_cases[branch].binders[binder].binding_id
						)) {
						constant = 0;
						break;
					}
				}
			}
			for (uint32_t branch = 1; branch < operation->case_count; ++branch) {
				if (prototype_judgement_classifier_conversion(
						ctx->terms, ctx->type_declarations,
						branch_result_arguments[0][argument],
						branch_result_arguments[branch][argument]
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					constant = 0;
					break;
				}
			}
			if (constant) {
				generic_result_arguments[argument] =
					branch_result_arguments[0][argument];
				continue;
			}
			uint32_t matching_index = PROTOTYPE_INVALID_ID;
			for (uint32_t index = 0; index < index_count; ++index) {
				int matches = 1;
				for (uint32_t branch = 0;
					branch < operation->case_count; ++branch) {
					if (prototype_judgement_classifier_conversion(
							ctx->terms, ctx->type_declarations,
							branch_result_arguments[branch][argument],
							branch_constructor_arguments[branch][
								scrutinee_parameter_count + index
							]
						).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
						matches = 0;
						break;
					}
				}
				if (matches) {
					matching_index = index;
					break;
				}
			}
			if (matching_index == PROTOTYPE_INVALID_ID) {
				result_instance_motive = 0;
				break;
			}
			generic_result_arguments[argument] = index_variables[matching_index];
		}
		if (result_instance_motive) {
			uint32_t generic_result;
			uint32_t motive_body;
			uint32_t value_binding = prototype_term_new_binding(ctx->terms);
			uint32_t motive;
			if (value_binding == PROTOTYPE_INVALID_ID ||
				prototype_term_type_instance_make(
					ctx->terms, ctx->type_declarations, result_type,
					generic_result_arguments, result_argument_count,
					&generic_result
				) != 0) {
				return -1;
			}
			motive_body = generic_result;
			if (returns_computation && prototype_term_computation_type(
					ctx->terms, joined_effect_row, generic_result, &motive_body
				) != 0) {
				return -1;
			}
			if (prototype_term_lambda(
					ctx->terms, value_binding, motive_body, &motive
				) != 0) {
				return -1;
			}
			for (uint32_t index = index_count; index > 0; --index) {
				if (prototype_term_lambda(
						ctx->terms, index_bindings[index - 1], motive, &motive
					) != 0) {
					return -1;
				}
			}
			uint32_t classifier = motive;
			for (uint32_t index = 0; index < index_count; ++index) {
				if (prototype_term_app(
						ctx->terms, classifier,
						scrutinee_arguments[scrutinee_parameter_count + index],
						&classifier
					) != 0) {
					return -1;
				}
			}
			return prototype_term_app(
				ctx->terms, classifier, match->as.match.scrutinee, p_classifier
			);
		}
	}

	/* Indexed motives are ordinary curried lambdas in TermDB:
	 * M : i1 -> ... -> in -> scrutinee -> classifier.  Infer this form when
	 * every branch returns one common indexed family and differs only in its
	 * index spine.  This is synthesized from branch classifiers and the
	 * scrutinee family; no expected `::` type participates. */
	uint32_t result_type = PROTOTYPE_INVALID_ID;
	uint32_t result_arguments[64][32];
	uint32_t result_argument_counts[64];
	int indexed_motive = has_indexed_scrutinee;
	for (uint32_t i = 0; indexed_motive && i < operation->case_count; ++i) {
		uint32_t branch_type;
		uint32_t constructor_type;
		uint32_t constructor_arguments[32];
		uint32_t constructor_argument_count;
		if (prototype_term_type_instance_info(
				ctx->terms, branch_results[i], &branch_type,
				result_arguments[i], &result_argument_counts[i]
			) != 0 || prototype_term_type_instance_info(
				ctx->terms, branch_constructor_results[i], &constructor_type,
				constructor_arguments, &constructor_argument_count
			) != 0 || constructor_type != scrutinee_type ||
			constructor_argument_count != scrutinee_argument_count ||
			branch_type >= ctx->type_declarations->type_count ||
			(i != 0 && (branch_type != result_type ||
			 result_argument_counts[i] != result_argument_counts[0]))) {
			indexed_motive = 0;
			break;
		}
		uint32_t branch_parameter_count =
			ctx->type_declarations->type_declarations[branch_type].parameter_count;
		for (uint32_t index = 0; indexed_motive && index < index_count; ++index) {
			if (prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					result_arguments[i][branch_parameter_count + index],
					constructor_arguments[scrutinee_parameter_count + index]
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				indexed_motive = 0;
			}
		}
		result_type = branch_type;
	}
	uint32_t result_parameter_count = indexed_motive ?
		ctx->type_declarations->type_declarations[result_type].parameter_count : 0;
	if (indexed_motive &&
		(ctx->type_declarations->type_declarations[result_type].index_count !=
			index_count || result_argument_counts[0] !=
				result_parameter_count + index_count || index_count == 0)) {
		indexed_motive = 0;
	}
	for (uint32_t i = 1; indexed_motive && i < operation->case_count; ++i) {
		for (uint32_t p = 0; p < result_parameter_count; ++p) {
			if (prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					result_arguments[0][p], result_arguments[i][p]
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				indexed_motive = 0;
				break;
			}
		}
	}
	/* A result family such as Acc's eliminator returns P(index), not another
	 * indexed ADT instance.  For a single constructor whose result indices are
	 * distinct field variables, abstraction is exact: substitute those field
	 * bindings by fresh motive index binders. More general constructor index
	 * expressions remain with the branch-Match motive below. */
	if (!indexed_motive && scrutinee_classifier != PROTOTYPE_INVALID_ID &&
		operation->case_count == 1 && scrutinee_type <
			ctx->type_declarations->type_count && index_count > 0 &&
		prototype_term_type_instance_info(
			ctx->terms, branch_constructor_results[0], &(uint32_t){0},
			scrutinee_arguments, &scrutinee_argument_count
		) == 0) {
		uint32_t parameter_count = ctx->type_declarations->type_declarations[
			scrutinee_type
		].parameter_count;
		uint32_t generalized = branch_results[0];
		uint32_t index_bindings[32];
		uint32_t index_vars[32];
		int can_abstract = index_count <= 32 &&
			scrutinee_argument_count == parameter_count + index_count;
		for (uint32_t i = 0; can_abstract && i < index_count; ++i) {
			uint32_t source_index = scrutinee_arguments[parameter_count + i];
			index_bindings[i] = prototype_term_new_binding(ctx->terms);
			if (source_index >= ctx->terms->term_count ||
				ctx->terms->terms[source_index].tag != PROTOTYPE_TERM_VAR ||
				index_bindings[i] == PROTOTYPE_INVALID_ID || prototype_term_var(
					ctx->terms, index_bindings[i], &index_vars[i]
				) != 0 || prototype_term_graph_substitute_bound_var(
					ctx->terms, ctx->type_declarations, generalized,
					ctx->terms->terms[source_index].as.var.binding_id,
					index_vars[i], &generalized
				) != 0) {
				can_abstract = 0;
			}
		}
		if (can_abstract) {
			uint32_t value_binding = prototype_term_new_binding(ctx->terms);
			uint32_t motive_body = generalized;
			uint32_t motive;
			if (returns_computation && prototype_term_computation_type(
					ctx->terms, joined_effect_row, generalized, &motive_body
				) != 0) {
				return -1;
			}
			if (value_binding == PROTOTYPE_INVALID_ID || prototype_term_lambda(
					ctx->terms, value_binding, motive_body, &motive
				) != 0) {
				return -1;
			}
			for (uint32_t i = index_count; i > 0; --i) {
				if (prototype_term_lambda(
						ctx->terms, index_bindings[i - 1], motive, &motive
					) != 0) {
					return -1;
				}
			}
			uint32_t classifier = motive;
			uint32_t source_type;
			uint32_t source_arguments[32];
			uint32_t source_argument_count;
			if (prototype_term_type_instance_info(
					ctx->terms, scrutinee_classifier, &source_type,
					source_arguments, &source_argument_count
				) != 0 || source_type != scrutinee_type ||
				source_argument_count != parameter_count + index_count) {
				return -1;
			}
			for (uint32_t i = 0; i < index_count; ++i) {
				if (prototype_term_app(
						ctx->terms, classifier,
						source_arguments[parameter_count + i], &classifier
					) != 0) {
					return -1;
				}
			}
			return prototype_term_app(
				ctx->terms, classifier, match->as.match.scrutinee, p_classifier
			);
		}
	}
	if (indexed_motive) {
		uint32_t index_bindings[32];
		uint32_t generic_arguments[32];
		for (uint32_t p = 0; p < result_parameter_count; ++p) {
			generic_arguments[p] = result_arguments[0][p];
		}
		for (uint32_t i = 0; i < index_count; ++i) {
			index_bindings[i] = prototype_term_new_binding(ctx->terms);
			if (index_bindings[i] == PROTOTYPE_INVALID_ID ||
				prototype_term_var(
					ctx->terms, index_bindings[i],
					&generic_arguments[result_parameter_count + i]
				) != 0) {
				return -1;
			}
		}
		uint32_t generic_result;
		uint32_t motive_body;
		uint32_t value_binding = prototype_term_new_binding(ctx->terms);
		uint32_t motive;
		if (value_binding == PROTOTYPE_INVALID_ID ||
			prototype_term_type_instance_make(
				ctx->terms, ctx->type_declarations, result_type,
				generic_arguments, result_parameter_count + index_count,
				&generic_result
			) != 0) {
			return -1;
		}
		motive_body = generic_result;
		if (returns_computation && prototype_term_computation_type(
				ctx->terms, joined_effect_row, generic_result, &motive_body
			) != 0) {
			return -1;
		}
		if (prototype_term_lambda(
				ctx->terms, value_binding, motive_body, &motive
			) != 0) {
			return -1;
		}
		for (uint32_t i = index_count; i > 0; --i) {
			if (prototype_term_lambda(
					ctx->terms, index_bindings[i - 1], motive, &motive
				) != 0) {
				return -1;
			}
		}
		uint32_t classifier = motive;
		uint32_t scrutinee_parameter_count =
			ctx->type_declarations->type_declarations[scrutinee_type].parameter_count;
		for (uint32_t i = 0; i < index_count; ++i) {
			if (prototype_term_app(
					ctx->terms, classifier,
					scrutinee_arguments[scrutinee_parameter_count + i],
					&classifier
				) != 0) {
				return -1;
			}
		}
		if (prototype_term_app(
				ctx->terms, classifier, match->as.match.scrutinee,
				p_classifier
			) != 0) {
			return -1;
		}
		return 0;
	}
	uint32_t binding_id = prototype_term_new_binding(ctx->terms);
	uint32_t binder_var;
	uint32_t motive_match;
	uint32_t motive_body;
	uint32_t motive;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, binding_id, &binder_var) != 0 ||
		prototype_term_match(ctx->terms, binder_var, motive_cases, operation->case_count, &motive_match) != 0) {
		return -1;
	}
	motive_body = motive_match;
	if (returns_computation && prototype_term_computation_type(
			ctx->terms, joined_effect_row, motive_match, &motive_body
		) != 0) {
		return -1;
	}
	if (prototype_term_lambda(ctx->terms, binding_id, motive_body, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

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
		ctx->operation_usage_solutions,
		ctx->metadata->typed_occurrences.occurrence_count,
		ctx->operation_usage_entries,
		ctx->operation_usage_entry_count,
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


static int operation_solver_refresh_usage_solutions(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->operation_usage_entries ||
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
			ctx->operation_usage_solutions,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY,
			ctx->operation_usage_entries,
			ctx->operation_usage_entry_capacity,
			&entry_count
		) != 0 || entry_count > UINT32_MAX) {
		return -1;
	}
	ctx->operation_usage_entry_count = (uint32_t)entry_count;
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_occurrence_usage_solution* source =
			&ctx->operation_usage_solutions[operation_id];
		struct operation_solver_solution* target =
			&ctx->classifier_solver.solutions[operation_id];
		target->first_usage_entry = source->first_entry;
		target->usage_entry_count = source->entry_count;
		target->binder_usage = source->binder_usage;
		target->usage_state = OPERATION_CONSTRAINT_STATE_SOLVED;
		for (uint32_t usage_index = 0;
			usage_index < source->entry_count;
			++usage_index) {
			const struct prototype_usage_entry* entry =
				&ctx->operation_usage_entries[
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
		uint32_t core_term = ctx->metadata->typed_occurrences.occurrences[target].core_term;
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

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct operation_constraint* equation,
	uint32_t classifier
) {
	if (!ctx || !equation || classifier >= ctx->terms->term_count ||
		equation->payload.classifier.classifier_variable >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t classifier_whnf;
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier_whnf
		) != 0) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[equation->payload.classifier.classifier_variable];
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
	for (uint32_t i = 0; i < core_case->binder_count; ++i) {
		uint32_t canonical_binding = ctx->terms->case_binders[
			core_case->first_binder + i
		].binding_id;
		if (prototype_term_contains_free_binding(
				ctx->terms, classifier_whnf, canonical_binding
			) || prototype_term_contains_free_binding(
				ctx->terms, classifier_whnf, operation_case->binder_ids[i]
			)) {
			return 1;
		}
	}
	for (uint32_t binding_id = 0;
		binding_id < ctx->terms->next_binding_id;
		++binding_id) {
		if (!prototype_term_contains_free_binding(
				ctx->terms, classifier_whnf, binding_id
			) || prototype_context_contains_binding(
				&ctx->metadata->contexts, match->context_id, binding_id
			)) {
			continue;
		}
		int effect_owned = operation_effect_row_binder_is_owned(
			ctx, equation->payload.classifier.classifier_variable, binding_id
		);
		if (effect_owned < 0) {
			return -1;
		}
		if (!effect_owned) {
			return 1;
		}
	}
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
	uint32_t* p_effect_row
) {
	if (!ctx || !p_returns_computation || !p_result || !p_effect_row ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, classifier, &view
		) != 0) {
		return -1;
	}
	if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		*p_returns_computation = 1;
		*p_result = view.result;
		*p_effect_row = view.effect_row;
		return 0;
	}
	*p_returns_computation = 0;
	*p_result = classifier;
	*p_effect_row = PROTOTYPE_INVALID_ID;
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
	if (!ctx || !p_joined || operation_solver_match_classifier_parts(
			ctx, left, &left_returns, &left_result, &left_row
		) != 0 || operation_solver_match_classifier_parts(
			ctx, right, &right_returns, &right_result, &right_row
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
	return prototype_term_effect_row_union(
			ctx->terms, left_row, right_row, &joined_row
		) != 0 ? -1 : prototype_term_computation_type(
			ctx->terms, joined_row, left_result, p_joined
		);
}

/*
 * A uniform motive is represented as a constant lambda. This is not a Match
 * typing shortcut: APP(\_ => T, scrutinee) reduces by beta to T. It is valid
 * only when no branch classifier mentions that branch's pattern binders.
 */
static int build_operation_uniform_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count != operation->case_count) {
		return 1;
	}
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, typing->operation, case_index
			);
		if (!equation || equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.solutions[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier;
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		int capture_status = operation_classifier_captures_case_binder(
			ctx, equation, branch_classifier
		);
		if (capture_status < 0) {
			return -1;
		}
		if (capture_status > 0) {
			return 1;
		}
		if (classifier == PROTOTYPE_INVALID_ID) {
			classifier = branch_classifier;
		} else {
			uint32_t joined;
			int join_status = operation_solver_join_match_classifiers(
				ctx, classifier, branch_classifier, &joined
			);
			if (join_status != 0) {
				return join_status;
			}
			classifier = joined;
		}
	}
	uint32_t binding_id = prototype_term_new_binding(ctx->terms);
	uint32_t motive;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_lambda(ctx->terms, binding_id, classifier, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

static const struct pending_match_typing* lookup_pending_match_typing(
	const struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx) {
		return NULL;
	}
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		if (ctx->pending_match_typings[i].operation == operation) {
			return &ctx->pending_match_typings[i];
		}
	}
	return NULL;
}

/*
 * Solve the guarded equation M(C(..., rest, ...)) = M(rest) without first
 * inventing a TermDB classifier for M(rest). The resulting motive carries its
 * own structurally-recursive match frame, so the recursive equation is a
 * finite graph rather than a cyclic solver substitution.
 */
static int build_operation_guarded_recursive_motive(
	struct compile_context* ctx,
	const struct pending_match_typing* typing,
	uint32_t* p_classifier
) {
	if (!ctx || !typing || !p_classifier ||
		typing->operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		typing->match_term >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[typing->operation];
	const struct prototype_term* match = &ctx->terms->terms[typing->match_term];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		match->tag != PROTOTYPE_TERM_MATCH ||
		operation->case_count == 0 || operation->case_count > 64 ||
		match->as.match.case_count != operation->case_count) {
		return 1;
	}
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t binder_cursor = 0;
	uint32_t motive_frame = prototype_term_new_ih_scope(ctx->terms);
	if (motive_frame == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, typing->operation, case_index
			);
		uint32_t source_case_id = match->as.match.first_case + case_index;
		if (!equation || source_case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* source_case =
			&ctx->terms->cases[source_case_id];
		const struct prototype_typed_occurrence_match_case* source_operation_case =
			&ctx->metadata->typed_occurrences.cases[
				operation->first_case + case_index
			];
		if (source_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			source_case->constructor_id == PROTOTYPE_INVALID_ID ||
			binder_cursor + source_case->binder_count > 256 ||
			source_operation_case->binder_count != source_case->binder_count) {
			return 1;
		}
		uint32_t reindexed_binders[64];
		uint32_t reindexed_binder_count;
		uint32_t motive_case_context;
		uint32_t case_substitution;
		if (source_case->binder_count > 64) {
			return -1;
		}
		if (prototype_typed_occurrence_match_case_is_solved(source_operation_case)) {
			reindexed_binder_count = source_operation_case->binder_count;
			motive_case_context = source_operation_case->context_id;
			for (uint32_t i = 0; i < reindexed_binder_count; ++i) {
				reindexed_binders[i] = source_operation_case->binder_ids[i];
			}
			if (prototype_substitution_identity(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					motive_case_context,
					&case_substitution
				) != 0) {
				return -1;
			}
		} else if (prototype_context_reindex_telescope(
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				ctx->terms,
				ctx->type_declarations,
				operation->context_id,
				ctx->metadata->typed_occurrences.cases[
					operation->first_case + case_index
				].context_id,
				reindexed_binders,
				64,
				&reindexed_binder_count,
				&motive_case_context,
				&case_substitution
			) != 0) {
			return -1;
		}
		if (reindexed_binder_count != source_case->binder_count) {
			return -1;
		}
		(void)motive_case_context;
		for (uint32_t binder_index = 0;
			binder_index < source_case->binder_count;
			++binder_index) {
			const struct prototype_case_binder* source_binder =
				&ctx->terms->case_binders[source_case->first_binder + binder_index];
			motive_binders[binder_cursor + binder_index].binding_id =
				reindexed_binders[binder_index];
			motive_binders[binder_cursor + binder_index].is_recursive =
				source_binder->is_recursive;
		}
		motive_cases[case_index].case_label_symbol_id =
			ctx->terms->case_label_symbols[source_case_id];
		motive_cases[case_index].constructor_owner = source_case->constructor_owner;
		motive_cases[case_index].constructor_id = source_case->constructor_id;
		motive_cases[case_index].binders = &motive_binders[binder_cursor];
		motive_cases[case_index].binder_count = source_case->binder_count;
		uint32_t branch_classifier =
			ctx->classifier_solver.solutions[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier;
		if (branch_classifier != PROTOTYPE_INVALID_ID) {
			if (prototype_term_reindex(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					branch_classifier,
					case_substitution,
					&motive_cases[case_index].body
				) != 0) {
				return -1;
			}
		} else {
			const struct prototype_typed_occurrence* branch =
				&ctx->metadata->typed_occurrences.occurrences[
					equation->payload.classifier.goal.motive_case.branch_operation
				];
			uint32_t branch_argument;
			if (branch->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
				branch->ih_owner_occurrence != typing->operation ||
				compile_occurrence_child(
					ctx, equation->payload.classifier.goal.motive_case.branch_operation,
					PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
					&branch_argument
				) != 0) {
				return 1;
			}
			uint32_t original_argument =
				ctx->metadata->typed_occurrences.occurrences[branch_argument].core_term;
			if (original_argument >= ctx->terms->term_count ||
				ctx->terms->terms[original_argument].tag != PROTOTYPE_TERM_VAR) {
				return 1;
			}
			uint32_t recursive_index = PROTOTYPE_INVALID_ID;
			for (uint32_t binder_index = 0;
				binder_index < source_case->binder_count;
				++binder_index) {
				const struct prototype_case_binder* source_binder =
					&ctx->terms->case_binders[source_case->first_binder + binder_index];
				if (source_binder->is_recursive &&
					source_operation_case->binder_ids[binder_index] ==
						ctx->terms->terms[original_argument].as.var.binding_id) {
					recursive_index = binder_index;
					break;
				}
			}
			if (recursive_index == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			uint32_t recursive_var;
			if (prototype_term_var(
					ctx->terms,
					motive_binders[binder_cursor + recursive_index].binding_id,
					&recursive_var
				) != 0 ||
				prototype_term_induction_hypothesis(
					ctx->terms, motive_frame, recursive_var,
					&motive_cases[case_index].body
				) != 0) {
				return -1;
			}
		}
		binder_cursor += source_case->binder_count;
	}
	uint32_t motive_binder = prototype_term_new_binding(ctx->terms);
	uint32_t motive_var;
	uint32_t motive_match;
	uint32_t motive;
	if (motive_binder == PROTOTYPE_INVALID_ID ||
		prototype_term_var(ctx->terms, motive_binder, &motive_var) != 0 ||
		prototype_term_match_with_ih_scope(
			ctx->terms, motive_var, motive_cases, operation->case_count,
			motive_frame, &motive_match
		) != 0 ||
		prototype_term_set_ih_scope_term(ctx->terms, motive_frame, motive_match) != 0 ||
		prototype_term_lambda(ctx->terms, motive_binder, motive_match, &motive) != 0 ||
		prototype_term_app(ctx->terms, motive, match->as.match.scrutinee, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

/*
 * This is the solver occurs check for the only recursive metavariable form
 * currently admitted by the language: an IH denotes M(rest). The occurrence
 * must point back to its own Match frame and `rest` must be a recursive field
 * of that Match. A raw M(x) self-reference outside that structural edge is
 * rejected instead of becoming a cyclic solver substitution.
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
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[ih_operation];
		uint32_t constraint_id = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < ctx->constraints.classifier_constraint_count; ++i) {
			const struct operation_constraint* constraint =
				&ctx->constraints.constraints[i];
			if (constraint->kind == OPERATION_CONSTRAINT_IH_EXPECTED &&
				constraint->payload.classifier.classifier_variable == ih_operation) {
				constraint_id = constraint->id;
				break;
			}
		}
		(void)add_compile_diagnostic(
			ctx,
			PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
			PROTOTYPE_COMPILE_DIAGNOSTIC_IH_OWNERSHIP,
			operation->source_ast,
			ih_operation,
			constraint_id,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			operation->source_ast < ctx->asts->node_count ?
				ctx->asts->nodes[operation->source_ast].span :
				(struct prototype_source_span) { 0, 0 }
		);
		return -1;
	}
	return 0;
}

static int operation_solver_nonrecursive_seed_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
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
	uint32_t candidate = PROTOTYPE_INVALID_ID;
	uint32_t source_body_occurrence = PROTOTYPE_INVALID_ID;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, operation_id, case_index
			);
		if (!equation || equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		uint32_t branch_classifier =
			ctx->classifier_solver.solutions[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier;
		int classifier_is_provisional = 0;
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
		if (branch_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_term* match =
			&ctx->terms->terms[operation->core_term];
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match->as.match.first_case + case_index
		];
		int has_recursive_binder = 0;
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			if (ctx->terms->case_binders[
					match_case->first_binder + binder_index
				].is_recursive) {
				has_recursive_binder = 1;
				break;
			}
		}
		if (has_recursive_binder && !classifier_is_provisional) {
			continue;
		}
		if (candidate == PROTOTYPE_INVALID_ID) {
			candidate = branch_classifier;
			source_body_occurrence = equation->payload.classifier.goal.motive_case.branch_operation;
		} else {
			uint32_t joined;
			int join_status = operation_solver_join_match_classifiers(
				ctx, candidate, branch_classifier, &joined
			);
			if (join_status != 0) {
				return join_status;
			}
			candidate = joined;
		}
	}
	if (candidate == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = candidate;
	*p_source_body_occurrence = source_body_occurrence;
	return 0;
}

static int operation_solver_match_has_recursive_binder(
	const struct compile_context* ctx,
	const struct prototype_typed_occurrence* operation
) {
	if (!ctx || !operation || operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	const struct prototype_term* match = &ctx->terms->terms[operation->core_term];
	for (uint32_t case_index = 0; case_index < match->as.match.case_count; ++case_index) {
		const struct prototype_match_case* match_case = &ctx->terms->cases[
			match->as.match.first_case + case_index
		];
		for (uint32_t binder_index = 0; binder_index < match_case->binder_count;
			++binder_index) {
			if (ctx->terms->case_binders[
					match_case->first_binder + binder_index
				].is_recursive) {
				return 1;
			}
		}
	}
	return 0;
}

static int operation_solver_has_guarded_recursive_equation(
	const struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	int has_unresolved_ih = 0;
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	for (uint32_t case_index = 0; case_index < match->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, operation_id, case_index
			);
		if (!equation) {
			fprintf(stderr,
				"match solve: missing motive equation match=%u case=%u state=%u constraints=%u\n",
				operation_id, case_index,
				ctx->classifier_solver.motive_solution_states[operation_id],
				ctx->constraints.classifier_constraint_count);
			return -1;
		}
		if (ctx->classifier_solver.solutions[
				equation->payload.classifier.goal.motive_case.branch_operation
			].classifier !=
			PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_typed_occurrence* body =
			&ctx->metadata->typed_occurrences.occurrences[
				equation->payload.classifier.goal.motive_case.branch_operation
			];
		if (body->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
			body->ih_owner_occurrence != operation_id) {
			return 0;
		}
		has_unresolved_ih = 1;
	}
	return has_unresolved_ih;
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
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	const struct pending_match_typing* typing =
		lookup_pending_match_typing(ctx, operation_id);
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || !typing ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count) {
		return -1;
	}
	if (ctx->classifier_solver.solutions[operation_id].classifier !=
			PROTOTYPE_INVALID_ID ||
		ctx->classifier_solver.motive_solution_states[operation_id] ==
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
		return 0;
	}
	int has_unresolved_body = 0;
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct operation_constraint* equation =
			operation_solver_motive_equation(
				&ctx->constraints, operation_id, case_index
			);
		if (!equation) {
			return -1;
		}
		uint32_t body_occurrence = equation->payload.classifier.goal.motive_case.branch_operation;
		if (body_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
			ctx->classifier_solver.solutions[body_occurrence].classifier ==
				PROTOTYPE_INVALID_ID) {
			has_unresolved_body = 1;
		}
	}
	if (!has_unresolved_body) {
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_READY;
		return 0;
	}
	int has_recursive_binder = operation_solver_match_has_recursive_binder(ctx, operation);
	if (has_recursive_binder < 0) {
		fprintf(stderr, "match solve: recursive binder inspection failed match=%u\n",
			operation_id);
		return -1;
	}
	if (!has_recursive_binder) {
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
		ctx, operation_id, operation, &seed_classifier, &seed_source_body_occurrence
	);
	if (seed_status < 0) {
		fprintf(stderr, "match solve: nonrecursive seed failed match=%u\n",
			operation_id);
		return -1;
	}
	if (seed_status == 0) {
		seed_status = operation_solver_seed_motive(
			ctx, operation_id, seed_classifier, seed_source_body_occurrence, p_changed
		);
		if (seed_status < 0) {
			fprintf(stderr,
				"match solve: motive seed failed match=%u classifier=%u source=%u\n",
				operation_id, seed_classifier, seed_source_body_occurrence);
		}
		return seed_status < 0 ? -1 : 0;
	}
	int guarded_status = operation_solver_has_guarded_recursive_equation(ctx, operation_id);
	if (guarded_status < 0) {
		fprintf(stderr, "match solve: guarded equation inspection failed match=%u\n",
			operation_id);
		return -1;
	}
	if (guarded_status > 0) {
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE;
		return 0;
	}
	return 0;
}

static int operation_solver_materialize_match_solution(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	const struct pending_match_typing* typing =
		lookup_pending_match_typing(ctx, operation_id);
	uint8_t state = ctx->classifier_solver.motive_solution_states[operation_id];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || !typing ||
		state == OPERATION_MOTIVE_SOLUTION_UNRESOLVED ||
		state == OPERATION_MOTIVE_SOLUTION_PROVISIONAL ||
		state == OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
		return 0;
	}
	int all_branches_impossible = operation->case_count > 0;
	for (uint32_t case_index = 0;
		case_index < operation->case_count;
		++case_index) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[
				operation->first_case + case_index
			];
		if (operation_case->refinement_status !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
			all_branches_impossible = 0;
			break;
		}
	}
	if (all_branches_impossible &&
		ctx->classifier_solver.solutions[operation_id].classifier !=
			PROTOTYPE_INVALID_ID) {
		uint32_t existing =
			ctx->classifier_solver.solutions[operation_id].classifier;
		if (existing >= ctx->terms->term_count ||
			ctx->terms->terms[existing].tag != PROTOTYPE_TERM_APP) {
			return -1;
		}
		uint32_t motive = ctx->terms->terms[existing].as.app.function;
		if (motive >= ctx->terms->term_count ||
			ctx->terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		/* Empty elimination has no reachable branch equation.  Its result
		 * classifier must therefore come from the surrounding constraint, not
		 * from the syntactic bodies of impossible branches. */
		ctx->classifier_solver.motive_terms[operation_id] = motive;
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
		return operation_solver_enqueue_dependents(ctx, operation_id);
	}
	uint32_t classifier;
	int status;
	if (state == OPERATION_MOTIVE_SOLUTION_GUARDED_RECURSIVE) {
		status = build_operation_guarded_recursive_motive(ctx, typing, &classifier);
	} else {
		status = build_operation_uniform_motive(ctx, typing, &classifier);
		if (status > 0) {
			status = build_operation_motive(ctx, typing, &classifier);
		}
	}
	if (status != 0) {
		return status < 0 ? -1 : 0;
	}
	/* The constant candidate is a bootstrap equation from one non-recursive
	 * branch, not a proposed classifier for the complete Match.  In a dependent
	 * motive the complete classifier need not be convertible to that branch
	 * equation; build_operation_motive has already checked every equation. */
	uint32_t motive = ctx->terms->terms[classifier].as.app.function;
	while (motive < ctx->terms->term_count &&
		ctx->terms->terms[motive].tag == PROTOTYPE_TERM_APP) {
		motive = ctx->terms->terms[motive].as.app.function;
	}
	if (motive >= ctx->terms->term_count ||
		ctx->terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	ctx->classifier_solver.motive_terms[operation_id] = motive;
	ctx->classifier_solver.motive_solution_states[operation_id] =
		OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
	int bind_status = operation_solver_bind(ctx, operation_id, classifier, p_changed);
	if (bind_status != 0) {
		return bind_status;
	}
	/* Motive materialization is solver state that IH constraints consume.  The
	 * Match classifier may already have been provisionally bound to the same
	 * Term, so classifier binding alone is not guaranteed to wake them. */
	return operation_solver_enqueue_dependents(ctx, operation_id);
}

static int operation_solver_materialize_solved_motives(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		if (ctx->metadata->typed_occurrences.occurrences[i].tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
			operation_solver_materialize_match_solution(ctx, i, p_changed) != 0) {
			fprintf(stderr,
				"classifier solver motive materialization failed operation=%u\n",
				i);
			return -1;
		}
	}
	return 0;
}

/* Indexed branch pullback changes the Context in which branch classifiers are
 * represented. A motive materialized before that pullback is derived state,
 * so reconstruct it from the final branch equations instead of retaining a
 * source-Context snapshot or treating a branch-local classifier as expected
 * input for the whole Match. */
static int operation_solver_rebuild_refined_match_motives(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t pending_id = 0;
		pending_id < ctx->pending_match_typing_count;
		++pending_id) {
		uint32_t operation_id =
			ctx->pending_match_typings[pending_id].operation;
		if (operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			operation->first_case + operation->case_count >
				ctx->metadata->typed_occurrences.case_count) {
			continue;
		}
		int has_refined_branch = 0;
		int all_branches_closed = 1;
		for (uint32_t case_index = 0;
			case_index < operation->case_count;
			++case_index) {
			uint32_t operation_case_id = operation->first_case + case_index;
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[operation_case_id];
			if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
				has_refined_branch = 1;
			} else if (operation_case->refinement_status !=
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
				all_branches_closed = 0;
			}
		}
		if (!has_refined_branch || !all_branches_closed) {
			continue;
		}

		struct operation_solver_solution* solution =
			&ctx->classifier_solver.solutions[operation_id];
		struct operation_solver_solution previous_solution = *solution;
		uint32_t previous_constant_candidate =
			ctx->classifier_solver.motive_constant_candidates[operation_id];
		uint32_t previous_motive =
			ctx->classifier_solver.motive_terms[operation_id];
		uint8_t previous_motive_state =
			ctx->classifier_solver.motive_solution_states[operation_id];
		uint32_t previous_hint = ctx->classifier_hints[operation_id];
		if (solution->classifier != PROTOTYPE_INVALID_ID) {
			ctx->classifier_solver.solution_revision++;
		}
		solution->classifier = PROTOTYPE_INVALID_ID;
		solution->effect_row = PROTOTYPE_INVALID_ID;
		solution->classifier_state = OPERATION_CONSTRAINT_STATE_PENDING;
		solution->reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE;
		ctx->classifier_solver.motive_constant_candidates[operation_id] =
			PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_terms[operation_id] = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_READY;
		ctx->classifier_hints[operation_id] = PROTOTYPE_INVALID_ID;

		int changed = 0;
		int rebuild_status = operation_solver_materialize_match_solution(
			ctx, operation_id, &changed
		);
		if (rebuild_status < 0) {
			return -1;
		}
		if (rebuild_status > 0 || solution->classifier == PROTOTYPE_INVALID_ID) {
			/* Some closed ordinary Matches still depend on classifier equations
			 * scheduled later in this pass. Rebuilding is a cache repair, so an
			 * unavailable replacement must leave the existing solution intact. */
			*solution = previous_solution;
			ctx->classifier_solver.motive_constant_candidates[operation_id] =
				previous_constant_candidate;
			ctx->classifier_solver.motive_terms[operation_id] = previous_motive;
			ctx->classifier_solver.motive_solution_states[operation_id] =
				previous_motive_state;
			ctx->classifier_hints[operation_id] = previous_hint;
			continue;
		}
		ctx->classifier_hints[operation_id] = solution->classifier;
	}
	return 0;
}

/* Keep IH propagation entirely in the solver until its parent motive has
 * been materialized from a solved equation. */
static int operation_solver_materialize_induction_hypothesis(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !p_changed || operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	/* Indexed branch materialization already reindexes the solved IH classifier
	 * through its recorded Context action. Re-running motive lifting on that
	 * target-Context occurrence would apply the pullback twice. The recorded
	 * substitution, not a second synthesis pass, is authoritative here. */
	if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
		if (ctx->classifier_solver.solutions[operation_id].classifier !=
				PROTOTYPE_INVALID_ID) {
			return 0;
		}
		uint32_t classifier;
		if (operation->source_classifier != PROTOTYPE_INVALID_ID &&
			prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				operation->source_classifier,
				operation->context_action_substitution,
				&classifier
			) == 0) {
			operation_solver_store_classifier_solution(
				ctx, operation_id, classifier
			);
			*p_changed = 1;
			return operation_solver_enqueue_dependents(ctx, operation_id);
		}
	}
	uint32_t argument_occurrence;
	uint32_t parent_match = operation->ih_owner_occurrence;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
			&argument_occurrence
		) != 0 || parent_match >= ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[parent_match].tag !=
			PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		fprintf(stderr,
			"IH materialization: invalid edge operation=%u parent=%u argument=%u\n",
			operation_id, parent_match, argument_occurrence);
		return -1;
	}
	if (operation_solver_solve_match(ctx, parent_match, p_changed) != 0) {
		fprintf(stderr, "IH materialization: parent match solve failed operation=%u match=%u\n",
			operation_id, parent_match);
		return -1;
	}
	uint32_t motive = ctx->classifier_solver.motive_terms[parent_match];
	if (motive == PROTOTYPE_INVALID_ID) {
		uint32_t parent_classifier = operation_solver_classifier(ctx, parent_match);
		uint32_t match_scrutinee;
		if (parent_classifier != PROTOTYPE_INVALID_ID &&
			parent_classifier < ctx->terms->term_count &&
			ctx->terms->terms[parent_classifier].tag == PROTOTYPE_TERM_APP &&
			compile_occurrence_child(
				ctx, parent_match, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
				&match_scrutinee
			) == 0 &&
			ctx->terms->terms[parent_classifier].as.app.argument ==
				ctx->metadata->typed_occurrences.occurrences[match_scrutinee].core_term) {
			motive = ctx->terms->terms[parent_classifier].as.app.function;
			while (motive < ctx->terms->term_count &&
				ctx->terms->terms[motive].tag == PROTOTYPE_TERM_APP) {
				motive = ctx->terms->terms[motive].as.app.function;
			}
			if (motive >= ctx->terms->term_count ||
				ctx->terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA) {
				fprintf(stderr,
					"IH materialization: recovered motive is not lambda operation=%u "
					"parent=%u motive=%u\n",
					operation_id, parent_match, motive);
				return -1;
			}
			ctx->classifier_solver.motive_terms[parent_match] = motive;
			ctx->classifier_solver.motive_solution_states[parent_match] =
				OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
		}
	}
	if (motive == PROTOTYPE_INVALID_ID) {
		if (operation_solver_validate_guarded_motive_occurrence(
				ctx, operation_id, parent_match, argument_occurrence
			) != 0) {
			fprintf(stderr,
				"IH materialization: guarded occurrence invalid operation=%u parent=%u\n",
				operation_id, parent_match);
			return -1;
		}
		if (operation_solver_set_ih_motive_application(
				ctx, operation_id, parent_match, argument_occurrence, p_changed
			) != 0) {
			fprintf(stderr,
				"IH materialization: motive application invalid operation=%u parent=%u "
				"argument=%u previous=%u count=%u\n",
				operation_id, parent_match, argument_occurrence,
				ctx->classifier_solver.ih_motive_application_ids[operation_id],
				ctx->classifier_solver.motive_application_count);
			return -1;
		}
		/* A constant motive equation M(_) == T is already a concrete solver
		 * solution for this IH occurrence.  Bind it now so CBPV wrappers can
		 * propagate through RETURN/THUNK/COMPUTATION_FOLD; the lambda for M is still
		 * materialized only after every Match equation has been checked. */
		uint32_t candidate =
			ctx->classifier_solver.motive_constant_candidates[parent_match];
		if (candidate != PROTOTYPE_INVALID_ID) {
			uint32_t previous = ctx->classifier_solver.solutions[
				operation_id
			].classifier;
			return previous == PROTOTYPE_INVALID_ID ?
				operation_solver_bind(ctx, operation_id, candidate, p_changed) : 0;
		}
		return 0;
	}
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[parent_match];
	uint32_t match_scrutinee;
	uint32_t scrutinee_classifier = compile_occurrence_child(
		ctx, parent_match, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
		&match_scrutinee
	) == 0 ? operation_solver_classifier(ctx, match_scrutinee) :
		PROTOTYPE_INVALID_ID;
	uint32_t field_classifier = PROTOTYPE_INVALID_ID;
	uint32_t field_context = operation->context_id;
	uint32_t field_action = PROTOTYPE_INVALID_ID;
	if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
		const struct prototype_substitution* action =
			prototype_substitution_get(
				&ctx->metadata->substitutions,
				operation->context_action_substitution
			);
		if (!action || action->source_context != operation->context_id) {
			return -1;
		}
		/* Constructor field schemas are stated in the source branch telescope.
		 * A solved index equation may remove one of those binders from the pulled-
		 * back occurrence Context, so construct the schema before the action and
		 * then transport the resulting classifier through that same action. */
		field_context = action->target_context;
		field_action = operation->context_action_substitution;
	}
	if (match_operation->core_term < ctx->terms->term_count &&
		ctx->terms->terms[match_operation->core_term].tag == PROTOTYPE_TERM_MATCH &&
		operation->ih_case_index <
			ctx->terms->terms[match_operation->core_term].as.match.case_count) {
		uint32_t case_id = ctx->terms->terms[
			match_operation->core_term
		].as.match.first_case + operation->ih_case_index;
		if (case_id < ctx->terms->case_count) {
			const struct prototype_match_case* match_case =
				&ctx->terms->cases[case_id];
			if (operation->ih_field_index < match_case->binder_count &&
				prototype_judgement_constructor_field_classifier(
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					field_context,
					match_case->constructor_owner,
					match_case->constructor_id,
					&ctx->terms->case_binders[match_case->first_binder],
					operation->ih_field_index,
					operation->ih_field_index,
					&field_classifier
				) != 0) {
				fprintf(stderr,
					"IH materialization: field classifier failed operation=%u parent=%u "
					"case=%u field=%u context=%u\n",
					operation_id, parent_match, operation->ih_case_index,
					operation->ih_field_index, operation->context_id);
				return -1;
			}
		}
	}
	if (field_classifier != PROTOTYPE_INVALID_ID &&
		field_action != PROTOTYPE_INVALID_ID && prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			field_classifier,
			field_action,
			&field_classifier
		) != 0) {
		return -1;
	}
	uint32_t classifier;
	int lift_status = field_classifier == PROTOTYPE_INVALID_ID ||
		scrutinee_classifier == PROTOTYPE_INVALID_ID ? -2 :
		prototype_judgement_lift_recursive_field_classifier(
			ctx->terms,
			ctx->type_declarations,
			field_classifier,
			ctx->metadata->typed_occurrences.occurrences[argument_occurrence].core_term,
			scrutinee_classifier,
			motive,
			&classifier
		);
	if (lift_status != 0) {
		fprintf(stderr,
			"IH materialization: recursive field lift failed operation=%u status=%d "
			"field=%u scrutinee=%u motive=%u\n",
			operation_id, lift_status, field_classifier,
			scrutinee_classifier, motive);
		return -1;
	}
	for (uint32_t reduction = 0; reduction < 4; ++reduction) {
		struct prototype_term_normalization_result normalized_classifier;
		if (prototype_term_normalize_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				classifier,
				ctx->metadata->normalization_step_limit,
				&normalized_classifier
			) != 0 || normalized_classifier.status ==
				PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			fprintf(stderr,
				"IH materialization: classifier normalization failed operation=%u classifier=%u\n",
				operation_id, classifier);
			return -1;
		}
		if (normalized_classifier.status !=
				PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			normalized_classifier.term_id == classifier) {
			break;
		}
		classifier = normalized_classifier.term_id;
	}
	if (operation_solver_close_unowned_effect_rows(
			ctx, operation_id, classifier, &classifier
		) != 0) {
		fprintf(stderr,
			"IH materialization: effect closure failed operation=%u classifier=%u\n",
			operation_id, classifier);
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.solutions[operation_id].classifier;
	if (previous < ctx->terms->term_count &&
		ctx->terms->terms[previous].tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			ctx->terms->terms[previous].as.computation_type.result,
			classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* The recursive occurrence already crossed a CBPV boundary. Its motive
		 * computes the result classifier, while the occurrence itself retains the
		 * enclosing computation and effect row. */
		return 0;
	}
	if (previous != PROTOTYPE_INVALID_ID) {
		struct prototype_term_classifier_view previous_view;
		struct prototype_term_classifier_view lifted_view;
		if (prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, previous, &previous_view
			) == 0 && prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL, classifier, &lifted_view
			) == 0 && previous_view.category ==
				PROTOTYPE_TERM_CATEGORY_COMPUTATION && lifted_view.category ==
				PROTOTYPE_TERM_CATEGORY_VALUE && prototype_term_computation_type(
				ctx->terms, previous_view.effect_row, classifier, &classifier
			) != 0) {
			return -1;
		}
	}
	if (previous == PROTOTYPE_INVALID_ID) {
		return operation_solver_bind(ctx, operation_id, classifier, p_changed);
	}
	struct prototype_term_classifier_view previous_view;
	struct prototype_term_classifier_view classifier_view;
	int previous_view_status = prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, previous, &previous_view
		);
	int classifier_view_status = prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, classifier, &classifier_view
		);
	if (previous_view_status == 0 && classifier_view_status == 0 &&
		previous_view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		classifier_view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		previous_view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
		classifier_view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations,
			previous_view.result, classifier_view.result
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* A constructor-domain equation propagated through a zero-clause fold is
		 * source-authoritative. Do not replace it with a provisional Match motive;
		 * the final derivation pass will validate both equations together. */
		for (uint32_t fold_id = 0;
			fold_id < ctx->metadata->typed_occurrences.occurrence_count; ++fold_id) {
			const struct prototype_typed_occurrence* fold =
				&ctx->metadata->typed_occurrences.occurrences[fold_id];
			uint32_t fold_computation;
			uint32_t fold_return_clause;
			if (fold->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
				compile_occurrence_child(
					ctx, fold_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					&fold_computation
				) != 0 || fold_computation != operation_id ||
				compile_occurrence_child(
					ctx, fold_id, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&fold_return_clause
				) != 0 ||
				fold->core_term >= ctx->terms->term_count ||
				ctx->terms->terms[fold->core_term].tag !=
					PROTOTYPE_TERM_COMPUTATION_FOLD ||
				ctx->terms->terms[fold->core_term].as.computation_fold.clause_count != 0) {
				continue;
			}
			const struct prototype_typed_occurrence* continuation =
				&ctx->metadata->typed_occurrences.occurrences[fold_return_clause];
			if (continuation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
				continuation->binder_classifier != PROTOTYPE_INVALID_ID &&
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					continuation->binder_classifier, previous_view.result
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
		}
		/* Without a source-domain equation, the materialized motive remains the
		 * only available classifier for this recursive application. */
		operation_solver_store_classifier_solution(ctx, operation_id, classifier);
		*p_changed = 1;
		return operation_solver_enqueue_dependents(ctx, operation_id);
	}
	int widen_status = operation_solver_widen_computation_binding(
		ctx, operation_id, classifier, p_changed
	);
	if (widen_status != 0) {
		fprintf(stderr,
			"IH materialization: computation widening failed operation=%u previous=%u "
			"classifier=%u status=%d\n",
			operation_id, previous, classifier, widen_status);
	}
	return widen_status;
}

static int operation_solver_select_evidence_in_context(
	struct compile_context* ctx,
	uint32_t expected_operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
);

static int operation_solver_collect_effect_row_assumptions(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* evidence,
	uint32_t capacity,
	uint32_t* p_count
) {
	if (!ctx || !evidence || !p_count || operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count || classifier >= ctx->terms->term_count) {
		return -1;
	}
	*p_count = 0;
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, classifier, &view
		) != 0 || view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	struct prototype_effect_row_normal_form row;
	if (prototype_term_effect_row_normal_form(
			ctx->terms, view.effect_row, &row
		) != 0) {
		return -1;
	}
	for (uint32_t atom_index = 0; atom_index < row.atom_count; ++atom_index) {
		uint32_t atom = row.atoms[atom_index];
		if (atom >= ctx->terms->term_count ||
			ctx->terms->terms[atom].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			continue;
		}
		uint32_t binding_id =
			ctx->terms->terms[atom].as.effect_row_var.binding_id;
		int found_owner = 0;
		for (uint32_t lambda_id = 0;
			lambda_id < ctx->metadata->typed_occurrences.occurrence_count;
			++lambda_id) {
			const struct prototype_typed_occurrence* lambda =
				&ctx->metadata->typed_occurrences.occurrences[lambda_id];
			uint32_t lambda_body;
			if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
				compile_occurrence_child(
					ctx, lambda_id, PROTOTYPE_TERM_CHILD_BODY, 0, &lambda_body
				) != 0) {
				continue;
			}
			int declares = 0;
			for (uint32_t i = 0; i < lambda->implicit_effect_row_count; ++i) {
				declares = declares ||
					lambda->implicit_effect_row_binders[i] == binding_id;
			}
			if (!declares) {
				continue;
			}
			uint8_t visited[ctx->metadata->typed_occurrences.occurrence_count];
			memset(visited, 0, sizeof(visited));
			int contains = operation_subtree_contains_operation(
				ctx, lambda_id, operation_id, visited
			);
			if (contains < 0) {
				return -1;
			}
			if (!contains) {
				continue;
			}
			if (*p_count >= capacity) {
				return -1;
			}
			uint32_t binder_var;
			if (prototype_term_var(
					ctx->terms, lambda->binding_id, &binder_var
				) != 0 || operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					ctx->metadata->typed_occurrences.occurrences[operation_id].context_id,
					binder_var,
					lambda->binder_classifier,
					&evidence[*p_count]
				) != 0) {
				return -1;
			}
			(*p_count)++;
			found_owner = 1;
			break;
		}
		if (!found_owner) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_materialize_induction_hypothesis_judgement(
	struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	uint32_t classifier = operation->classifier;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		classifier == PROTOTYPE_INVALID_ID ||
		operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
		return -1;
	}
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_proposition* relations =
			source == 0 ? ctx->judgement_delta.propositions : ctx->judgement->propositions;
		const struct prototype_judgement_derivation_candidate* derivations =
			source == 0 ? ctx->judgement_delta.derivation_candidates :
				ctx->judgement->derivation_candidates;
		size_t proposition_count =
			source == 0 ? ctx->judgement_delta.proposition_count : ctx->judgement->proposition_count;
		size_t derivation_candidate_count = source == 0 ?
			ctx->judgement_delta.derivation_candidate_count :
			ctx->judgement->derivation_candidate_count;
		for (size_t i = 0; i < proposition_count; ++i) {
			if (relations[i].kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relations[i].occurrence_id == operation_id &&
				relations[i].subject == operation->core_term &&
				prototype_judgement_candidate_find_derivation_kind(
					relations,
					proposition_count,
					derivations,
					derivation_candidate_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
					&(uint32_t){0}
				) == 0 &&
				(prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					relations[i].classifier, classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
				return 0;
			}
		}
	}
	const struct prototype_typed_occurrence_graph* graph;
	struct prototype_typed_occurrence_induction_edge edge;
	graph = prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
	if (prototype_typed_occurrence_graph_induction_edge(
			graph, ctx->terms, operation_id, &edge
		) != 0) {
		fprintf(stderr, "IH materialization edge failed operation=%u\n", operation_id);
		return -1;
	}
	uint32_t match_term = ctx->metadata->typed_occurrences.occurrences[
		edge.owner_match_occurrence
	].core_term;
	uint32_t match_classifier = ctx->metadata->typed_occurrences.occurrences[
		edge.owner_match_occurrence
	].classifier;
	if (match_classifier >= ctx->terms->term_count ||
		ctx->terms->terms[match_classifier].tag != PROTOTYPE_TERM_APP) {
		fprintf(stderr, "IH materialization match classifier failed operation=%u match=%u classifier=%u\n", operation_id, edge.owner_match_occurrence, match_classifier);
		return -1;
	}
	uint32_t motive = ctx->terms->terms[match_classifier].as.app.function;
	while (motive < ctx->terms->term_count &&
		ctx->terms->terms[motive].tag == PROTOTYPE_TERM_APP) {
		motive = ctx->terms->terms[motive].as.app.function;
	}
	if (motive >= ctx->terms->term_count ||
		ctx->terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA) {
		fprintf(stderr, "IH materialization motive failed operation=%u motive=%u\n", operation_id, motive);
		return -1;
	}
	const struct prototype_typed_occurrence* owner_match_occurrence =
		&ctx->metadata->typed_occurrences.occurrences[edge.owner_match_occurrence];
	uint32_t owner_case_id = owner_match_occurrence->first_case + edge.case_index;
	const struct prototype_typed_occurrence_match_case* owner_case =
		owner_case_id < ctx->metadata->typed_occurrences.case_count ?
		&ctx->metadata->typed_occurrences.cases[owner_case_id] : NULL;
	if (owner_case && prototype_typed_occurrence_match_case_is_solved(owner_case) && prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			motive,
			owner_case->refinement_substitution,
			&motive
		) != 0) {
		fprintf(stderr, "IH materialization motive reindex failed operation=%u case=%u\n", operation_id, owner_case_id);
		return -1;
	}
	struct prototype_judgement_selected_evidence effect_row_evidence[16];
	uint32_t effect_row_evidence_count;
	if (operation_solver_collect_effect_row_assumptions(
			ctx,
			operation_id,
			classifier,
			effect_row_evidence,
			16,
			&effect_row_evidence_count
		) != 0) {
		fprintf(stderr, "IH materialization effect evidence failed operation=%u context=%u classifier=%u\n", operation_id, operation->context_id, classifier);
		return -1;
	}
	int expand_status = prototype_judgement_delta_expand_induction_hypothesis(
		&ctx->judgement_delta,
		ctx->terms,
		operation->core_term,
		classifier,
		match_term,
		motive,
		edge.case_index,
		edge.field_index,
		effect_row_evidence,
		effect_row_evidence_count
	);
	if (expand_status != 0) {
		fprintf(
			stderr,
			"IH materialization expansion failed operation=%u context=%u subject=%u classifier=%u match=%u motive=%u case=%u field=%u status=%d\n",
			operation_id,
			operation->context_id,
			operation->core_term,
			classifier,
			match_term,
			motive,
			edge.case_index,
			edge.field_index,
			expand_status
		);
	}
	return expand_status;
}

/*
 * The solver owns classifier synthesis. Once all operation variables have a
 * concrete solution, this phase only reconstructs JudgementDB derivations for
 * those already-fixed conclusions. It is deliberately not a second inference
 * pass: every generated conclusion must be normalization-equal to the solver
 * binding for the same operation.
 */
static int operation_match_cases_are_resolved(
	const struct compile_context* ctx,
	const struct prototype_typed_occurrence* operation
) {
	if (!ctx || !ctx->metadata || !operation ||
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		operation->first_case + operation->case_count >
			ctx->metadata->typed_occurrences.case_count) {
		return 0;
	}
	for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[operation->first_case + case_index];
		if (operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
			return 0;
		}
	}
	return 1;
}

static uint32_t operation_solver_evidence_owner(
	const struct compile_context* ctx,
	uint32_t operation_id
);

static int operation_solver_select_operation_evidence_in_context(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
);

static int operation_solver_materialize_match_judgement(
	struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		!operation_match_cases_are_resolved(ctx, operation) ||
		operation->classifier == PROTOTYPE_INVALID_ID ||
		operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
		operation->case_count > 64) {
		return 1;
	}
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, operation->context_id
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, operation_id
	);
	uint32_t branch_occurrence_ids[64];
	uint32_t branch_substitution_ids[64];
	struct prototype_judgement_selected_evidence branch_evidence[64];
	uint32_t match_first_case =
		ctx->terms->terms[operation->core_term].as.match.first_case;
	for (uint32_t case_index = 0;
		case_index < operation->case_count;
		++case_index) {
		uint32_t branch_operation;
		uint32_t term_case_id = match_first_case + case_index;
		if (compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
				case_index, &branch_operation
			) != 0 ||
			term_case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* body =
			&ctx->metadata->typed_occurrences.occurrences[branch_operation];
		uint32_t operation_case_id = operation->first_case + case_index;
		struct operation_constraint* refinement =
			operation_solver_branch_refinement_constraint(
				ctx, operation_case_id
			);
		int refinement_status = refinement ?
			refinement->payload.branch_refinement.refinement_status :
			PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
		if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
			branch_occurrence_ids[case_index] = PROTOTYPE_INVALID_ID;
			branch_substitution_ids[case_index] = PROTOTYPE_INVALID_ID;
			memset(
				&branch_evidence[case_index],
				0,
				sizeof(branch_evidence[case_index])
			);
			continue;
		}
		if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_INVALID) {
			return -1;
		}
		if (body->classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		branch_occurrence_ids[case_index] = branch_operation;
		branch_substitution_ids[case_index] = refinement_status ==
			PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
			ctx->metadata->typed_occurrences.cases[
				operation_case_id
			].refinement_substitution : PROTOTYPE_INVALID_ID;
		int evidence_status = operation_solver_select_operation_evidence_in_context(
			ctx,
			branch_operation,
			body->context_id,
			body->core_term,
			body->classifier,
			&branch_evidence[case_index]
		);
		if (evidence_status != 0) {
			return evidence_status;
		}
	}
	int record_status = prototype_judgement_delta_record_materialized_match_motive(
		&ctx->judgement_delta,
		ctx->terms,
		operation->core_term,
		operation->classifier,
		branch_occurrence_ids,
		branch_evidence,
		branch_substitution_ids,
		operation->case_count
	);
	return record_status;
}

static int operation_solver_delta_select_evidence(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
);
static int operation_solver_select_evidence_in_context(
	struct compile_context* ctx,
	uint32_t expected_operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
);
static int operation_solver_refresh_delta_resource_usage(
	struct compile_context* ctx
);

static int operation_solver_materialize_refinement_binding_evidence_at_depth(
	struct compile_context* ctx,
	uint32_t substitution_id,
	uint32_t depth
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(
			ctx ? &ctx->metadata->substitutions : NULL, substitution_id
		);
	if (!ctx || !ctx->metadata || !substitution ||
		depth > ctx->metadata->substitutions.substitution_count) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
		return operation_solver_materialize_refinement_binding_evidence_at_depth(
			ctx, substitution->first, depth + 1
		) != 0 ||
			operation_solver_materialize_refinement_binding_evidence_at_depth(
				ctx, substitution->second, depth + 1
			) != 0 ? -1 : 0;
	}
	if (substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND) {
		return 0;
	}
	if (operation_solver_materialize_refinement_binding_evidence_at_depth(
			ctx, substitution->first, depth + 1
		) != 0) {
		return -1;
	}
	if (substitution->term >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_judgement_selected_evidence existing_evidence;
	int existing_status;
	if (ctx->terms->terms[substitution->term].tag == PROTOTYPE_TERM_VAR) {
		if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
			return -1;
		}
		existing_status = prototype_judgement_delta_select_evidence_by_authority(
			&ctx->judgement_delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING,
			substitution->term,
			substitution->source_context,
			substitution->term,
			substitution->term_classifier,
			&existing_evidence
		);
	} else {
		existing_status = operation_solver_select_evidence_in_context(
			ctx,
			PROTOTYPE_INVALID_ID,
			substitution->source_context,
			substitution->term,
			substitution->term_classifier,
			&existing_evidence
		);
	}
	if (existing_status == 0) {
		return 0;
	}
	if (existing_status < 0 || existing_status == 2) {
		return -1;
	}
	if (ctx->terms->terms[substitution->term].tag ==
			PROTOTYPE_TERM_CONSTRUCTOR) {
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta, substitution->source_context
		);
		prototype_judgement_delta_set_occurrence(
			&ctx->judgement_delta, PROTOTYPE_INVALID_ID
		);
		return prototype_judgement_delta_record_constructor_intro(
			&ctx->judgement_delta,
			ctx->terms,
			ctx->type_declarations,
			substitution->term,
			substitution->term_classifier
		);
	}
	if (ctx->terms->terms[substitution->term].tag == PROTOTYPE_TERM_APP) {
		uint32_t head;
		uint32_t owner;
		uint32_t constructor_index;
		uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t argument_count;
		uint32_t argument_occurrences[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		struct prototype_judgement_selected_evidence
			argument_evidence[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		if (prototype_term_constructor_spine_info(
				ctx->terms,
				substitution->term,
				&head,
				&owner,
				&constructor_index,
				arguments,
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
				&argument_count
			) != 0 || argument_count == 0) {
			return 0;
		}
		for (uint32_t i = 0; i < argument_count; ++i) {
			uint32_t binding_context_id;
			if (arguments[i] >= ctx->terms->term_count ||
				ctx->terms->terms[arguments[i]].tag != PROTOTYPE_TERM_VAR ||
				prototype_context_find_binding(
					&ctx->metadata->contexts,
					substitution->source_context,
					ctx->terms->terms[arguments[i]].as.var.binding_id,
					&binding_context_id
				) != 0) {
				return 0;
			}
			const struct prototype_context* binding_context = prototype_context_get(
				&ctx->metadata->contexts, binding_context_id
			);
			uint32_t argument_classifier =
				prototype_context_classifier_term(binding_context);
			if (!binding_context || argument_classifier == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			int argument_status = operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					substitution->source_context,
					arguments[i],
					argument_classifier,
					&argument_evidence[i]
				);
			if (argument_status > 0) {
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, substitution->source_context
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				if (prototype_judgement_delta_record_context_binding_assumption(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->terms->terms[arguments[i]].as.var.binding_id,
						argument_classifier
					) != 0) {
					return 0;
				}
				argument_status = operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					substitution->source_context,
					arguments[i],
					argument_classifier,
					&argument_evidence[i]
				);
			}
			if (argument_status != 0) {
				return 0;
			}
			argument_occurrences[i] = PROTOTYPE_INVALID_ID;
		}
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta, substitution->source_context
		);
		prototype_judgement_delta_set_occurrence(
			&ctx->judgement_delta, PROTOTYPE_INVALID_ID
		);
		(void)head;
		(void)owner;
		(void)constructor_index;
		return prototype_judgement_delta_record_constructor_spine(
			&ctx->judgement_delta,
			ctx->terms,
			ctx->type_declarations,
			substitution->term,
			substitution->term_classifier,
			argument_occurrences,
			argument_evidence,
			argument_count
		);
	}
	if (ctx->terms->terms[substitution->term].tag != PROTOTYPE_TERM_VAR) {
		return 0;
	}
	uint32_t binding_id =
		ctx->terms->terms[substitution->term].as.var.binding_id;
	uint32_t binding_context_id;
	if (prototype_context_find_binding(
			&ctx->metadata->contexts,
			substitution->source_context,
			binding_id,
			&binding_context_id
		) != 0) {
		return -1;
	}
	const struct prototype_context* binding_context = prototype_context_get(
		&ctx->metadata->contexts, binding_context_id
	);
	if (!binding_context ||
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			prototype_context_classifier_term(binding_context),
			substitution->term_classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, substitution->source_context
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, PROTOTYPE_INVALID_ID
	);
	return prototype_judgement_delta_record_context_binding_assumption(
		&ctx->judgement_delta,
		ctx->terms,
		binding_id,
		substitution->term_classifier
	);
}

static int operation_solver_materialize_match_pattern_assumptions(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
			operation->source_ast >= ctx->asts->node_count ||
			ctx->asts->nodes[operation->source_ast].tag != PROTOTYPE_AST_MATCH) {
			continue;
		}
		/* Constructor selection depends on the scrutinee classifier. The solver
		 * may have learned that classifier only in this fixed-point round, so
		 * defer proof materialization until the following round resolves cases. */
		if (!operation_match_cases_are_resolved(ctx, operation)) {
			continue;
		}
		const struct prototype_term* match =
			&ctx->terms->terms[operation->core_term];
		const struct prototype_ast_node* source_match =
			&ctx->asts->nodes[operation->source_ast];
		if (match->as.match.case_count != operation->case_count ||
			source_match->as.match.case_count != operation->case_count ||
			operation->first_case + operation->case_count >
				ctx->metadata->typed_occurrences.case_count) {
			return -1;
		}
		for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
			uint32_t branch_operation;
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[operation->first_case + case_index];
			uint32_t term_case_id = match->as.match.first_case + case_index;
			uint32_t ast_case_id = source_match->as.match.first_case + case_index;
			if (term_case_id >= ctx->terms->case_count || ast_case_id >= ctx->asts->case_count ||
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
					case_index, &branch_operation
				) != 0 ||
				operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
				operation_case->constructor_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation_case->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, PROTOTYPE_INVALID_ID
			);
			const struct prototype_match_case* match_case =
				&ctx->terms->cases[term_case_id];
			const struct prototype_ast_match_case* ast_case =
				&ctx->asts->cases[ast_case_id];
			if (match_case->binder_count != ast_case->binder_count ||
				match_case->binder_count > 16 ||
				match_case->first_binder + match_case->binder_count >
					ctx->terms->case_binder_count ||
				ast_case->first_binder + ast_case->binder_count >
					ctx->asts->case_binder_count) {
				return -1;
			}
			struct prototype_case_binder occurrence_binders[16];
			for (uint32_t binder_index = 0;
				binder_index < match_case->binder_count; ++binder_index) {
				if (!prototype_context_contains_binding(
						&ctx->metadata->contexts,
						operation_case->context_id,
						operation_case->binder_ids[binder_index]
					)) {
					return -1;
				}
				occurrence_binders[binder_index] = ctx->terms->case_binders[
					match_case->first_binder + binder_index
				];
				occurrence_binders[binder_index].binding_id =
					operation_case->binder_ids[binder_index];
			}
			for (uint32_t binder_index = 0;
				binder_index < match_case->binder_count;
				++binder_index) {
				uint32_t binding_context_id;
				uint32_t binder_var;
				if (prototype_context_find_binding(
						&ctx->metadata->contexts,
						operation_case->context_id,
						occurrence_binders[binder_index].binding_id,
						&binding_context_id
					) != 0 ||
					prototype_term_var(
						ctx->terms,
						occurrence_binders[binder_index].binding_id,
						&binder_var
					) != 0) {
					fprintf(stderr,
						"match assumption classifier failed operation=%u case=%u "
						"binder=%u context=%u owner=%u constructor=%u\n",
						operation_id, case_index, binder_index,
						operation_case->context_id,
						operation_case->constructor_owner,
						operation_case->constructor_id);
					return -1;
				}
				const struct prototype_context* binding_context =
					prototype_context_get(
						&ctx->metadata->contexts, binding_context_id
					);
				if (!binding_context) {
					return -1;
				}
				uint32_t classifier =
					prototype_context_classifier_term(binding_context);
				if (classifier == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				int already_materialized = 0;
				for (size_t relation_id = 0;
					relation_id < ctx->judgement_delta.proposition_count;
					++relation_id) {
					const struct prototype_judgement_proposition* relation =
						&ctx->judgement_delta.propositions[relation_id];
					if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						relation->context_id != operation_case->context_id ||
						relation->subject != binder_var ||
						relation->classifier != classifier) {
						continue;
					}
					uint32_t cursor = 0;
					uint32_t derivation_id;
					while (prototype_judgement_candidate_derivation_next(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)relation_id,
						&cursor,
						&derivation_id
					) == 0) {
						const struct prototype_judgement_derivation_candidate* proof =
							&ctx->judgement_delta.derivation_candidates[derivation_id];
						if (proof->proof_kind ==
								PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION &&
							proof->rule_data.constructor.owner_view ==
							operation_case->constructor_owner &&
							proof->rule_data.constructor.constructor_index ==
							operation_case->constructor_id &&
							proof->rule_data.constructor.field_index == binder_index) {
							already_materialized = 1;
							break;
						}
					}
					if (already_materialized) {
						break;
					}
				}
				if (already_materialized) {
					continue;
				}
				if (prototype_judgement_delta_record_match_pattern(
						&ctx->judgement_delta,
						ctx->terms,
						binder_var,
						classifier,
						operation_case->constructor_owner,
						operation_case->constructor_id,
						binder_index,
						prototype_typed_occurrence_match_case_is_solved(operation_case) ?
							operation_case->refinement_substitution :
							PROTOTYPE_INVALID_ID
					) != 0) {
					return -1;
				}
			}
			/* Pattern assumptions are the authoritative evidence for case fields.
			 * Materialize them before traversing the refinement substitution so an
			 * identity-mapped field is not redundantly recorded as a plain binder
			 * assumption, which would erase its constructor-field provenance. */
			if (prototype_typed_occurrence_match_case_is_solved(operation_case) &&
				operation_solver_materialize_refinement_binding_evidence_at_depth(
					ctx, operation_case->refinement_substitution, 0
				) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

static uint32_t operation_solver_evidence_owner(
	const struct compile_context* ctx,
	uint32_t operation_id
) {
	for (uint32_t depth = 0;
		ctx && ctx->metadata && operation_id < ctx->metadata->typed_occurrences.occurrence_count &&
		depth < 64;
		++depth) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		int authority_neutral_atom = operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
			operation->core_term < ctx->terms->term_count &&
			(ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_EXTERNAL_REF ||
			 ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_PURE_PRIMITIVE ||
			 ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_EFFECT_OPERATION ||
			 prototype_term_type_instance_info(
				ctx->terms,
				operation->core_term,
				&(uint32_t){0},
				NULL,
				&(uint32_t){0}
			) == 0);
		if ((authority_neutral_atom &&
			 operation->context_action_substitution == PROTOTYPE_INVALID_ID) ||
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			 operation->context_action_substitution == PROTOTYPE_INVALID_ID) ||
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR &&
			 operation->context_action_substitution == PROTOTYPE_INVALID_ID)) {
			return PROTOTYPE_INVALID_ID;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			operation_id = operation->wrapped_occurrence;
			continue;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
			operation->wrapped_occurrence < ctx->metadata->typed_occurrences.occurrence_count &&
			operation->classifier ==
				ctx->metadata->typed_occurrences.occurrences[
					operation->wrapped_occurrence
				].classifier) {
			operation_id = operation->wrapped_occurrence;
			continue;
		}
		return operation_id;
	}
	return operation_id;
}

static int operation_solver_refresh_delta_resource_usage(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	for (size_t i = 0; i < ctx->judgement_delta.proposition_count; ++i) {
		struct prototype_judgement_proposition* proposition =
			&ctx->judgement_delta.propositions[i];
		if (proposition->occurrence_id != PROTOTYPE_INVALID_ID) {
			struct prototype_usage_vector usage;
			if (operation_usage_solution_view(
					ctx, proposition->occurrence_id, &usage
				) != 0 || usage.count > UINT32_MAX) {
				return -1;
			}
			proposition->resource_usage_count = (uint32_t)usage.count;
			proposition->resource_usage = usage.entries;
		}
	}
	return 0;
}

static int operation_solver_delta_select_evidence(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!ctx || operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	return prototype_judgement_delta_select_evidence(
		&ctx->judgement_delta,
		operation_id,
		context_id,
		subject,
		classifier,
		selected
	);
}

static int operation_solver_select_evidence_in_context(
	struct compile_context* ctx,
	uint32_t expected_operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	if (!ctx || !ctx->metadata ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		fprintf(stderr,
			"evidence resource refresh failed operation=%u context=%u subject=%u classifier=%u\n",
			expected_operation_id, context_id, subject, classifier);
		return -1;
	}
	uint32_t source_context = context_id;
	for (;;) {
		struct prototype_judgement_selected_evidence source_evidence;
		int selection_status = operation_solver_delta_select_evidence(
			ctx,
			expected_operation_id,
			source_context,
			subject,
			classifier,
			&source_evidence
		);
		if (selection_status < 0 || selection_status == 2) {
			return -1;
		}
		if (selection_status == 0) {
			if (source_context == context_id) {
				if (p_evidence) {
					*p_evidence = source_evidence;
				}
				return 0;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, expected_operation_id
			);
			uint32_t weakening_substitution;
			if (prototype_substitution_projection_path(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					context_id,
					source_context,
					&weakening_substitution
				) != 0) {
				return -1;
			}
			int weaken_status = prototype_judgement_delta_record_context_weaken(
				&ctx->judgement_delta,
				&source_evidence,
				weakening_substitution
			);
			if (weaken_status != 0 || !p_evidence) {
				return weaken_status;
			}
			return operation_solver_delta_select_evidence(
				ctx,
				expected_operation_id,
				context_id,
				subject,
				classifier,
				p_evidence
			);
		}
		if (source_context == 0) {
			break;
		}
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, source_context
		);
		if (!context || context->parent >= source_context) {
			return -1;
		}
		source_context = context->parent;
	}
	return 1;
}

static int operation_solver_select_operation_evidence_in_context(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		return operation_solver_select_evidence_in_context(
			ctx,
			operation_solver_evidence_owner(ctx, operation_id),
			context_id,
			subject,
			classifier,
			p_evidence
		);
	}
	int authority_kind = operation->context_action_substitution ==
		PROTOTYPE_INVALID_ID ?
		PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING :
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE;
	uint32_t authority_id = authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ? operation_id : subject;
	uint32_t source_context = context_id;
	for (;;) {
		struct prototype_judgement_selected_evidence source_evidence;
		int selection_status =
			prototype_judgement_delta_select_evidence_by_authority(
				&ctx->judgement_delta,
				authority_kind,
				authority_id,
				source_context,
				subject,
				classifier,
				&source_evidence
			);
		if (selection_status < 0 || selection_status == 2) {
			return -1;
		}
		if (selection_status == 0) {
			if (source_context == context_id) {
				if (p_evidence) {
					*p_evidence = source_evidence;
				}
				return 0;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta,
				authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
					operation_id : PROTOTYPE_INVALID_ID
			);
			uint32_t weakening_substitution;
			if (prototype_substitution_projection_path(
					&ctx->metadata->substitutions,
					&ctx->metadata->contexts,
					context_id,
					source_context,
					&weakening_substitution
				) != 0 || prototype_judgement_delta_record_context_weaken(
					&ctx->judgement_delta,
					&source_evidence,
					weakening_substitution
				) != 0) {
				return -1;
			}
			if (!p_evidence) {
				return 0;
			}
			return prototype_judgement_delta_select_evidence_by_authority(
				&ctx->judgement_delta,
				authority_kind,
				authority_id,
				context_id,
				subject,
				classifier,
				p_evidence
			);
		}
		if (source_context == 0) {
			break;
		}
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, source_context
		);
		if (!context || context->parent >= source_context) {
			return -1;
		}
		source_context = context->parent;
	}
	return 1;
}

static int operation_solver_require_evidence_in_context(
	struct compile_context* ctx,
	uint32_t expected_operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier
) {
	return operation_solver_select_evidence_in_context(
		ctx,
		expected_operation_id,
		context_id,
		subject,
		classifier,
		NULL
	);
}

static int operation_solver_lookup_operation_derivation_classifier(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t subject,
	int proof_kind,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier) {
		return -1;
	}
	for (size_t i = ctx->judgement_delta.proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation =
			&ctx->judgement_delta.propositions[i - 1];
		if (relation->occurrence_id == operation_id &&
			relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			prototype_judgement_candidate_find_derivation_kind(
				ctx->judgement_delta.propositions,
				ctx->judgement_delta.proposition_count,
				ctx->judgement_delta.derivation_candidates,
				ctx->judgement_delta.derivation_candidate_count,
				(uint32_t)(i - 1),
				proof_kind,
				&(uint32_t){0}
			) == 0) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	if (ctx->judgement_delta.db) {
		for (size_t i = ctx->judgement_delta.db->proposition_count; i > 0; --i) {
			const struct prototype_judgement_proposition* relation =
				&ctx->judgement_delta.db->propositions[i - 1];
			if (relation->occurrence_id == operation_id &&
				relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->subject == subject &&
				prototype_judgement_candidate_find_derivation_kind(
					ctx->judgement_delta.db->propositions,
					ctx->judgement_delta.db->proposition_count,
					ctx->judgement_delta.db->derivation_candidates,
					ctx->judgement_delta.db->derivation_candidate_count,
					(uint32_t)(i - 1),
					proof_kind,
					&(uint32_t){0}
				) == 0) {
				*p_classifier = relation->classifier;
				return 0;
			}
		}
	}
	return 1;
}

static int operation_match_cases_are_resolved(
	const struct compile_context* ctx,
	const struct prototype_typed_occurrence* operation
);

static int operation_solver_materialize_match_judgement(
	struct compile_context* ctx,
	uint32_t operation_id
);

/*
 * A source operation is a typed occurrence.  Its TermDB root can be shared
 * with an alpha-equivalent occurrence whose children have different raw
 * binder ids.  JudgementDB is serialized using TermDB representatives, so
 * reify the occurrence derivation against the representative's structural
 * children instead of assuming operation->core_term retains the source
 * child ids.
 */
static int operation_solver_reify_core_proof(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t core_term,
	uint32_t depth
) {
	if (!ctx || !ctx->metadata || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		core_term >= ctx->terms->term_count || depth > 256) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	/* TypedOccurrenceGraph owns the typed occurrence. The proof conclusion must use
	 * that occurrence's own Core projection, not a structurally equal child
	 * recovered from an interned parent Term. */
	core_term = operation->core_term;
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, operation_id
	);
	uint32_t classifier = operation->classifier;
	const struct prototype_term* term = &ctx->terms->terms[core_term];
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	int free_effect_row = operation_classifier_contains_unowned_effect_row(
		ctx, operation_id, classifier
	);
	if (free_effect_row < 0) {
		return -1;
	}
	if (free_effect_row != 0) {
		/* Recursive evidence construction must obey the same closure boundary as
		 * the outer materialization pass. An ascription or parent APP must not
		 * turn a residual external effect-row constraint into a kernel fact. */
		return 1;
	}
	if (ctx->judgement) {
		struct prototype_judgement_selected_evidence accepted_evidence;
		uint32_t accepted_authority =
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE ?
			operation_id : operation_solver_evidence_owner(ctx, operation_id);
		int accepted_status;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
			int authority_kind = operation->context_action_substitution ==
				PROTOTYPE_INVALID_ID ?
				PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING :
				PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE;
			uint32_t authority_id = authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ? operation_id : core_term;
			accepted_status = prototype_judgement_select_evidence_by_authority(
				ctx->judgement,
				authority_kind,
				authority_id,
				operation->context_id,
				core_term,
				classifier,
				&accepted_evidence
			);
		} else {
			accepted_status = prototype_judgement_select_evidence(
				ctx->judgement,
				accepted_authority,
				operation->context_id,
				core_term,
				classifier,
				&accepted_evidence
			);
		}
		if (accepted_status < 0 || accepted_status == 2) {
			return -1;
		}
		if (accepted_status == 0) {
			return 0;
		}
	}
	switch (operation->tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA: {
			uint32_t body_occurrence;
			if (term->tag != PROTOTYPE_TERM_LAMBDA ||
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_BODY, 0,
					&body_occurrence
				) != 0) {
				fprintf(stderr,
					"lambda reification: invalid structure operation=%u core=%u tag=%d\n",
					operation_id, core_term, term->tag);
				return -1;
			}
			const struct prototype_typed_occurrence* body =
				&ctx->metadata->typed_occurrences.occurrences[body_occurrence];
			if (body->classifier == PROTOTYPE_INVALID_ID ||
				operation->binder_classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, body_occurrence, body->core_term, depth + 1
			);
			if (status != 0) {
				if (status < 0) {
					fprintf(stderr,
						"lambda reification: body failed operation=%u body=%u "
						"context=%u core=%u classifier=%u\n",
						operation_id, body_occurrence, body->context_id,
						body->core_term, body->classifier);
				}
				return status;
			}
			const struct prototype_context* binder_context = prototype_context_get(
				&ctx->metadata->contexts, body->context_id
			);
			uint32_t binder_var;
			uint32_t context_binder_classifier =
				prototype_context_classifier_term(binder_context);
			if (!binder_context || binder_context->parent != operation->context_id ||
				binder_context->binding_id == PROTOTYPE_INVALID_ID ||
				context_binder_classifier == PROTOTYPE_INVALID_ID ||
				prototype_term_var(
					ctx->terms, binder_context->binding_id, &binder_var
				) != 0) {
				fprintf(stderr,
					"lambda reification: invalid binder context operation=%u "
					"lambda-context=%u body=%u body-context=%u parent=%u "
					"binding=%u classifier=%u\n",
					operation_id, operation->context_id, body_occurrence,
					body->context_id,
					binder_context ? binder_context->parent : PROTOTYPE_INVALID_ID,
					binder_context ? binder_context->binding_id : PROTOTYPE_INVALID_ID,
					context_binder_classifier);
				return -1;
			}
			int has_binder_assumption = 0;
			for (size_t i = 0;
				i < ctx->judgement_delta.proposition_count;
				++i) {
				const struct prototype_judgement_proposition* relation =
					&ctx->judgement_delta.propositions[i];
				if (relation->context_id == body->context_id &&
					relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
					relation->subject == binder_var &&
					relation->classifier == context_binder_classifier &&
					prototype_judgement_candidate_find_derivation_kind(
						ctx->judgement_delta.propositions,
						ctx->judgement_delta.proposition_count,
						ctx->judgement_delta.derivation_candidates,
						ctx->judgement_delta.derivation_candidate_count,
						(uint32_t)i,
						PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
						&(uint32_t){0}
					) == 0) {
					has_binder_assumption = 1;
					break;
				}
			}
			if (!has_binder_assumption) {
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, body->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				size_t before = ctx->judgement_delta.proposition_count;
				if (prototype_judgement_delta_record_context_binding_assumption(
						&ctx->judgement_delta,
						ctx->terms,
						binder_context->binding_id,
						context_binder_classifier
					) != 0 ||
					ctx->judgement_delta.proposition_count <= before) {
					fprintf(stderr,
						"lambda reification: binder assumption failed operation=%u "
						"context=%u binding=%u classifier=%u\n",
						operation_id, body->context_id,
						binder_context->binding_id, context_binder_classifier);
					return -1;
				}
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			struct prototype_judgement_selected_evidence binder_evidence;
			int binder_status =
				prototype_judgement_delta_select_evidence_by_authority(
				&ctx->judgement_delta,
				PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING,
				binder_var,
				body->context_id,
				binder_var,
				context_binder_classifier,
				&binder_evidence
			);
			if (binder_status != 0) {
				if (binder_status < 0) {
					fprintf(stderr,
						"lambda reification: binder evidence failed operation=%u "
						"context=%u binding-var=%u classifier=%u status=%d\n",
						operation_id, body->context_id, binder_var,
						context_binder_classifier, binder_status);
				}
				return binder_status;
			}
			uint32_t proof_binder_classifier = context_binder_classifier;
			uint32_t classifier_domain;
			uint32_t classifier_family;
			if (prototype_judgement_pi_parts(
					ctx->terms, classifier,
					&classifier_domain, &classifier_family
				) == 0 && prototype_judgement_classifier_value_whnf(
					ctx->terms,
					ctx->type_declarations,
					classifier_domain,
					&classifier_domain
				) == 0 && classifier_domain != proof_binder_classifier) {
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, body->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				int binder_exposure_status =
					prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&binder_evidence,
						classifier_domain,
						binder_var
					);
				int exposed_binder_status = binder_exposure_status == 0 ?
					operation_solver_delta_select_evidence(
						ctx,
						PROTOTYPE_INVALID_ID,
						body->context_id,
						binder_var,
						classifier_domain,
						&binder_evidence
					) : -1;
				if (binder_exposure_status != 0 || exposed_binder_status != 0) {
					fprintf(
						stderr,
						"lambda binder exposure failed operation=%u context=%u "
						"source=%u target=%u record=%d select=%d\n",
						operation_id,
						body->context_id,
						context_binder_classifier,
						classifier_domain,
						binder_exposure_status,
						exposed_binder_status
					);
					return -1;
				}
				proof_binder_classifier = classifier_domain;
			}
			(void)classifier_family;
			uint32_t expected_family;
			uint32_t expected_classifier;
			if (prototype_term_pure_family(
					ctx->terms,
					operation->binding_id,
					body->classifier,
					&expected_family
				) != 0 || prototype_term_pi_family(
					ctx->terms,
					proof_binder_classifier,
					expected_family,
					&expected_classifier
				) != 0 || operation_solver_generalize_lambda_effect_rows(
					ctx, operation_id, expected_classifier, &expected_classifier
				) != 0) {
				fprintf(stderr,
					"lambda reification: classifier construction failed operation=%u "
					"binding=%u domain=%u body-classifier=%u\n",
					operation_id, operation->binding_id,
					proof_binder_classifier, body->classifier);
				return -1;
			}
			int classifier_is_derived = expected_classifier != classifier;
			if (classifier_is_derived && !prototype_judgement_classifier_compatible(
					ctx->terms,
					ctx->type_declarations,
					classifier,
					expected_classifier
				)) {
				return 1;
			}
			struct prototype_judgement_selected_evidence body_evidence;
			int body_status = operation_solver_select_operation_evidence_in_context(
				ctx,
				body_occurrence,
				body->context_id,
				body->core_term,
				body->classifier,
				&body_evidence
			);
			if (body_status != 0) {
				if (body_status < 0) {
					fprintf(stderr,
						"lambda reification: body evidence failed operation=%u body=%u "
						"context=%u core=%u classifier=%u status=%d\n",
						operation_id, body_occurrence, body->context_id,
						body->core_term, body->classifier, body_status);
				}
				return body_status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			uint32_t lambda_intro_operation = classifier_is_derived ?
				PROTOTYPE_INVALID_ID : operation_id;
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, lambda_intro_operation
			);
			if (prototype_judgement_delta_record_lambda_intro(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					lambda_intro_operation,
					core_term,
					expected_classifier,
					binder_var,
					body->core_term,
						&binder_evidence,
						body_occurrence,
					&body_evidence
				) != 0) {
				fprintf(stderr,
					"lambda reification: introduction failed operation=%u "
					"context=%u core=%u expected=%u actual=%u binder=%u:%u "
					"body=%u:%u:%u\n",
					operation_id, operation->context_id, core_term,
					expected_classifier, classifier,
					binder_var, binder_evidence.classifier,
					body_occurrence, body_evidence.subject,
					body_evidence.classifier);
				return -1;
			}
			if (classifier_is_derived) {
				struct prototype_judgement_selected_evidence lambda_evidence;
				if (operation_solver_delta_select_evidence(
						ctx,
						PROTOTYPE_INVALID_ID,
						operation->context_id,
						core_term,
						expected_classifier,
						&lambda_evidence
					) != 0) {
					fprintf(stderr,
						"lambda reification: derived evidence selection failed "
						"operation=%u context=%u core=%u classifier=%u\n",
						operation_id, operation->context_id, core_term,
						expected_classifier);
					return -1;
				}
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, operation_id
				);
				if (prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&lambda_evidence,
						classifier,
						core_term
					) != 0) {
					fprintf(stderr,
						"lambda reification: classifier exposure failed operation=%u "
						"context=%u core=%u source=%u target=%u\n",
						operation_id, operation->context_id, core_term,
						expected_classifier, classifier);
					return -1;
				}
			}
			return 0;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_APP: {
			uint32_t function_operation;
			uint32_t argument_occurrence;
			uint32_t app_core;
			uint32_t type_id;
			uint32_t type_argument_count;
			if (prototype_term_type_instance_info(
					ctx->terms,
					core_term,
					&type_id,
					NULL,
					&type_argument_count
				) == 0) {
				if (type_id >= ctx->type_declarations->type_count) {
					return -1;
				}
				const struct prototype_type_declaration* type =
					&ctx->type_declarations->type_declarations[type_id];
				uint32_t family_arity = type->parameter_count + type->index_count;
				if (type_argument_count > family_arity) {
					return -1;
				}
				if (type_argument_count < family_arity) {
					/* Partial family applications are classifier-bearing source
					 * occurrences, but are not themselves formed types. */
					return 1;
				}
				return prototype_judgement_delta_record_type_formation(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier
				);
			}
			if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION) {
				/* This occurrence is compiler staging evidence for exposing a pure
				 * family lambda. Its solved classifier remains in TypedOccurrence,
				 * while the resulting full type receives TYPE_FORMATION evidence. */
				return 1;
			}
			if (prototype_term_core_projection(
					ctx->terms, core_term, &app_core
				) != 0 || ctx->terms->terms[app_core].tag != PROTOTYPE_TERM_APP ||
				compile_occurrence_child(
						ctx, operation_id, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
						&function_operation
					) != 0 || compile_occurrence_child(
						ctx, operation_id, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
						&argument_occurrence
					) != 0) {
				return -1;
			}
			const struct prototype_term* app_term = &ctx->terms->terms[app_core];
			if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
				uint32_t head;
				uint32_t owner;
				uint32_t constructor_index;
				uint32_t arguments[64];
				uint32_t argument_count;
				uint32_t reverse_argument_occurrences[64];
				uint32_t argument_occurrence_count = 0;
				uint32_t expected_domains[64];
				uint32_t expected_domain_count;
				uint32_t cursor = operation_id;
				if (prototype_term_constructor_spine_info(
						ctx->terms,
						core_term,
						&head,
						&owner,
						&constructor_index,
						arguments,
						64,
						&argument_count
					) != 0) {
					return -1;
				}
					if (prototype_judgement_constructor_spine_expected_domains(
						ctx->terms,
						ctx->type_declarations,
						&ctx->metadata->contexts,
						&ctx->metadata->substitutions,
						operation->context_id,
						core_term,
						PROTOTYPE_INVALID_ID,
						expected_domains,
						64,
						&expected_domain_count
					) != 0) {
					return -1;
				}
				while (cursor < ctx->metadata->typed_occurrences.occurrence_count) {
					const struct prototype_typed_occurrence* cursor_operation =
						&ctx->metadata->typed_occurrences.occurrences[cursor];
					uint32_t cursor_function;
					uint32_t cursor_argument;
					if (cursor_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
						cursor_operation->application_role !=
							PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
						break;
					}
					if (argument_occurrence_count >= 64 ||
						compile_occurrence_child(
							ctx, cursor, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
							&cursor_function
						) != 0 || compile_occurrence_child(
							ctx, cursor, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
							&cursor_argument
						) != 0) {
						return -1;
					}
					reverse_argument_occurrences[argument_occurrence_count++] =
						cursor_argument;
					cursor = cursor_function;
				}
				if (argument_occurrence_count != argument_count) {
					return -1;
				}
				if (expected_domain_count != argument_count) {
					return -1;
				}
					uint32_t argument_occurrences[64];
					uint32_t argument_classifiers[64];
					struct prototype_judgement_selected_evidence argument_evidence[64];
				for (uint32_t i = 0; i < argument_count; ++i) {
					uint32_t argument_occurrence = reverse_argument_occurrences[
						argument_count - i - 1
					];
					argument_occurrences[i] = argument_occurrence;
					const struct prototype_typed_occurrence* argument_occurrence_node =
						&ctx->metadata->typed_occurrences.occurrences[argument_occurrence];
					argument_classifiers[i] = argument_occurrence_node->classifier;
					if (argument_classifiers[i] == PROTOTYPE_INVALID_ID) {
						return 1;
					}
					int status = operation_solver_reify_core_proof(
						ctx,
						argument_occurrence,
						arguments[i],
						depth + 1
					);
					if (status != 0) {
						return status;
					}
						status = operation_solver_select_operation_evidence_in_context(
							ctx,
							argument_occurrence,
						operation->context_id,
						arguments[i],
							argument_classifiers[i],
							&argument_evidence[i]
					);
					if (status != 0) {
						return status;
					}
					if (argument_classifiers[i] != expected_domains[i]) {
						prototype_judgement_delta_set_context(
							&ctx->judgement_delta, operation->context_id
						);
						prototype_judgement_delta_set_occurrence(
							&ctx->judgement_delta, argument_occurrence
						);
						int exposure_status =
							prototype_judgement_delta_record_expected_type_exposure(
								&ctx->judgement_delta,
								ctx->terms,
								ctx->type_declarations,
								&argument_evidence[i],
								expected_domains[i],
								arguments[i]
							);
						int expected_evidence_status = exposure_status == 0 ?
							operation_solver_delta_select_evidence(
								ctx,
								argument_occurrence,
								operation->context_id,
								arguments[i],
								expected_domains[i],
								&argument_evidence[i]
							) : -1;
						if (exposure_status != 0 || expected_evidence_status != 0) {
							return -1;
						}
						argument_classifiers[i] = expected_domains[i];
					}
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, operation_id
				);
				(void)head;
				(void)owner;
				(void)constructor_index;
				return prototype_judgement_delta_record_constructor_spine(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
						classifier,
						argument_occurrences,
						argument_evidence,
					argument_count
				);
			}
			const struct prototype_typed_occurrence* function =
				&ctx->metadata->typed_occurrences.occurrences[function_operation];
			const struct prototype_typed_occurrence* argument =
				&ctx->metadata->typed_occurrences.occurrences[argument_occurrence];
			if (function->classifier == PROTOTYPE_INVALID_ID ||
				argument->classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
					ctx, function_operation, app_term->as.app.function, depth + 1
			);
			if (status != 0) {
				return status;
			}
			status = operation_solver_reify_core_proof(
					ctx, argument_occurrence, app_term->as.app.argument, depth + 1
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence function_evidence;
				status = operation_solver_select_operation_evidence_in_context(
					ctx,
					function_operation,
					operation->context_id,
					app_term->as.app.function,
				function->classifier,
				&function_evidence
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence argument_evidence;
				status = operation_solver_select_operation_evidence_in_context(
					ctx,
					argument_occurrence,
					operation->context_id,
					app_term->as.app.argument,
				argument->classifier,
				&argument_evidence
			);
			if (status != 0) {
				return status;
			}
			uint32_t reindexed_function_classifier;
			uint32_t reindexed_argument_classifier;
			uint32_t direct_classifier;
			int direct_status = prototype_judgement_delta_app_elim_classifier(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				function_evidence.classifier,
				argument_evidence.subject,
				argument_evidence.classifier,
				&direct_classifier
			);
			int direct_matches = direct_status == 0 &&
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					direct_classifier, classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
			reindexed_function_classifier = function_evidence.classifier;
			reindexed_argument_classifier = argument_evidence.classifier;
			if (!direct_matches && (operation_solver_reindex_in_enclosing_indexed_branches(
					ctx,
					operation_id,
					operation->context_id,
					function_evidence.classifier,
					&reindexed_function_classifier
				) != 0 || operation_solver_reindex_in_enclosing_indexed_branches(
					ctx,
					operation_id,
					operation->context_id,
					argument_evidence.classifier,
					&reindexed_argument_classifier
				) != 0)) {
				return -1;
			}
			struct prototype_judgement_selected_evidence* app_evidence[2] = {
				&function_evidence, &argument_evidence
			};
			uint32_t reindexed_classifiers[2] = {
				reindexed_function_classifier, reindexed_argument_classifier
			};
			uint32_t branch_endomorphism = PROTOTYPE_INVALID_ID;
			for (uint32_t premise = 0; premise < 2; ++premise) {
				if (app_evidence[premise]->classifier == reindexed_classifiers[premise]) {
					continue;
				}
				if (branch_endomorphism == PROTOTYPE_INVALID_ID &&
					operation_solver_indexed_branch_endomorphism(
						ctx,
						operation_id,
						operation->context_id,
						&branch_endomorphism
					) != 0) {
					return -1;
				}
				if (operation_solver_materialize_refinement_binding_evidence_at_depth(
						ctx, branch_endomorphism, 0
					) != 0) {
					return -1;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta,
					premise == 0 ? function_operation : argument_occurrence
				);
				struct prototype_judgement_selected_evidence refined_evidence;
				if (prototype_judgement_delta_record_substitution_reindex(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						app_evidence[premise],
						branch_endomorphism,
						&refined_evidence
					) != 0) {
					return -1;
				}
				if (refined_evidence.classifier != reindexed_classifiers[premise]) {
					return -1;
				}
				*app_evidence[premise] = refined_evidence;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			uint32_t proof_classifier;
			if (prototype_judgement_delta_app_elim_classifier(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					function_evidence.classifier,
					argument_evidence.subject,
					argument_evidence.classifier,
					&proof_classifier
				) != 0 || prototype_judgement_delta_record_app_elim(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					proof_classifier,
					&function_evidence,
					&argument_evidence
				) != 0) {
				return -1;
			}
			if (proof_classifier != classifier) {
				struct prototype_judgement_selected_evidence app_result_evidence;
				if (operation_solver_delta_select_evidence(
						ctx,
						operation_id,
						operation->context_id,
						core_term,
						proof_classifier,
						&app_result_evidence
					) != 0 ||
					prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&app_result_evidence,
						classifier,
						core_term
					) != 0) {
					return -1;
				}
			}
			return 0;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE: {
			uint32_t child_term;
			uint32_t child_operation;
			int child_role;
			if (term->tag == PROTOTYPE_TERM_RETURN) {
				child_term = term->as.return_term.value;
				child_role = PROTOTYPE_TERM_CHILD_RETURN_VALUE;
			} else if (term->tag == PROTOTYPE_TERM_THUNK) {
				child_term = term->as.thunk.computation;
				child_role = PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION;
			} else if (term->tag == PROTOTYPE_TERM_FORCE) {
				child_term = term->as.force.value;
				child_role = PROTOTYPE_TERM_CHILD_FORCE_VALUE;
			} else {
				return -1;
			}
			if (compile_occurrence_child(
					ctx, operation_id, child_role, 0, &child_operation
				) != 0) {
				return -1;
			}
			uint32_t child_classifier = ctx->metadata->typed_occurrences.occurrences[
				child_operation
			].classifier;
			if (child_classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, child_operation, child_term, depth + 1
			);
			if (status != 0) {
				return status;
			}
			struct prototype_judgement_selected_evidence child_evidence;
			status = operation_solver_select_operation_evidence_in_context(
				ctx,
				child_operation,
				operation->context_id,
				child_term,
				child_classifier,
				&child_evidence
			);
			if (status != 0) {
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			uint32_t derived_classifier;
			if (prototype_judgement_cbpv_boundary_classifier(
					ctx->terms,
					ctx->type_declarations,
					core_term,
					child_classifier,
					&derived_classifier
				) != 0 || prototype_judgement_delta_record_cbpv_boundary(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					child_operation,
					&child_evidence
				) != 0) {
				return -1;
			}
			if (derived_classifier != classifier) {
				struct prototype_judgement_selected_evidence boundary_evidence;
				if (operation_solver_delta_select_evidence(
						ctx,
						operation_id,
						operation->context_id,
						core_term,
						derived_classifier,
						&boundary_evidence
					) != 0 ||
					prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&boundary_evidence,
						classifier,
						core_term
					) != 0) {
					return -1;
				}
			}
			return 0;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD: {
			uint32_t computation_occurrence;
			uint32_t return_operation_id;
			if (term->tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					&computation_occurrence
				) != 0 || compile_occurrence_child(
					ctx, operation_id,
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&return_operation_id
				) != 0) {
				return -1;
			}
			uint32_t proven_classifier;
			int proven_status = operation_solver_lookup_operation_derivation_classifier(
				ctx,
				operation_id,
				core_term,
				PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
				&proven_classifier
			);
			if (proven_status < 0) {
				return -1;
			}
			if (proven_status == 0) {
				int evidence_status = operation_solver_require_evidence_in_context(
					ctx,
					operation_id,
					operation->context_id,
					core_term,
					proven_classifier
				);
				return evidence_status;
			}
			uint32_t clause_count = term->as.computation_fold.clause_count;
			if (clause_count != 0) {
				/* Clause-bearing folds are kernel solver conclusions. The
				 * TypedOccurrenceGraph classifier is only a fixed-point approximation. */
				return 1;
			}
			if (clause_count > PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
				operation->fold_clause_count != clause_count ||
				(clause_count != 0 &&
				 (operation->first_fold_clause >
					ctx->metadata->typed_occurrences.fold_clause_count ||
				  clause_count > ctx->metadata->typed_occurrences.fold_clause_count -
					operation->first_fold_clause ||
				  term->as.computation_fold.first_clause >
					ctx->terms->computation_fold_clause_count ||
				  clause_count > ctx->terms->computation_fold_clause_count -
					term->as.computation_fold.first_clause))) {
				return -1;
			}
			const struct prototype_typed_occurrence* computation =
				&ctx->metadata->typed_occurrences.occurrences[computation_occurrence];
			const struct prototype_typed_occurrence* return_operation =
				&ctx->metadata->typed_occurrences.occurrences[return_operation_id];
			uint32_t premise_count = 2 + 2 * clause_count;
				uint32_t premise_occurrences[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_terms[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				struct prototype_judgement_selected_evidence premise_evidence[
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
				];
			premise_occurrences[0] = computation_occurrence;
			premise_occurrences[1] = return_operation_id;
			premise_terms[0] = term->as.computation_fold.computation;
			premise_terms[1] = term->as.computation_fold.return_clause;
			premise_classifiers[0] = computation->classifier;
			premise_classifiers[1] = return_operation->classifier;
			if (computation->tag == PROTOTYPE_TYPED_OCCURRENCE_REQUEST) {
				int proven_status = operation_solver_lookup_operation_derivation_classifier(
					ctx,
					computation_occurrence,
					premise_terms[0],
					PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
					&premise_classifiers[0]
				);
				if (proven_status != 0) {
					return proven_status < 0 ? -1 : 1;
				}
			}
			for (uint32_t i = 0; i < clause_count; ++i) {
				uint32_t clause_operation_occurrence;
				uint32_t clause_lambda_occurrence;
				uint32_t clause_body_occurrence;
				const struct prototype_computation_fold_clause* core_clause =
					&ctx->terms->computation_fold_clauses[
						term->as.computation_fold.first_clause + i
					];
				if (compile_fold_clause_occurrences(
						ctx, operation_id, i, &clause_operation_occurrence,
						&clause_lambda_occurrence, &clause_body_occurrence
					) != 0) {
					return -1;
				}
				(void)clause_body_occurrence;
				premise_occurrences[2 + 2 * i] = clause_operation_occurrence;
				premise_occurrences[3 + 2 * i] = clause_lambda_occurrence;
				premise_terms[2 + 2 * i] = core_clause->operation;
				premise_terms[3 + 2 * i] = core_clause->body;
			}
			for (uint32_t i = 2; i < premise_count; ++i) {
				if (premise_occurrences[i] >= ctx->metadata->typed_occurrences.occurrence_count) {
					return -1;
				}
				premise_classifiers[i] = ctx->metadata->typed_occurrences.occurrences[
					premise_occurrences[i]
				].classifier;
			}
			for (uint32_t i = 0; i < premise_count; ++i) {
				if (premise_classifiers[i] == PROTOTYPE_INVALID_ID) {
					return 1;
				}
			}
			for (uint32_t i = 0; i < premise_count; ++i) {
				int status = operation_solver_reify_core_proof(
					ctx,
					premise_occurrences[i],
					premise_terms[i],
					depth + 1
				);
				if (status != 0) {
					return status;
				}
					if (clause_count == 0) {
						status = operation_solver_select_operation_evidence_in_context(
							ctx,
							premise_occurrences[i],
						operation->context_id,
						premise_terms[i],
							premise_classifiers[i],
							&premise_evidence[i]
					);
					if (status != 0) {
						return status;
					}
				}
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			int status = prototype_judgement_delta_record_computation_fold_elim(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
					core_term,
						classifier,
						premise_occurrences,
						premise_evidence,
				premise_count
			);
			return status < 0 ? -1 : 0;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_VAR: {
			if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
				/* One reindexed variable can be reached through both its direct
				 * occurrence and a parent constructor/return. Its derivation is
				 * authoritative for this exact typed occurrence, so materialization
				 * must be idempotent within the current compile delta. */
				struct prototype_judgement_selected_evidence existing_evidence;
				int existing_status = operation_solver_delta_select_evidence(
					ctx,
					operation_id,
					operation->context_id,
					core_term,
					classifier,
					&existing_evidence
				);
				if (existing_status < 0 || existing_status == 2) {
					return -1;
				}
				if (existing_status == 0) {
					return 0;
				}
				const struct prototype_substitution* action =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						operation->context_action_substitution
					);
				uint32_t source_binding_context;
				if (operation->source_core_term >= ctx->terms->term_count ||
					ctx->terms->terms[operation->source_core_term].tag !=
						PROTOTYPE_TERM_VAR || !action ||
					action->source_context != operation->context_id) {
					fprintf(stderr, "reindex VAR invalid action operation=%u\n", operation_id);
					return -1;
				}
				if (prototype_context_find_binding(
						&ctx->metadata->contexts,
						action->target_context,
						ctx->terms->terms[
							operation->source_core_term
						].as.var.binding_id,
						&source_binding_context
					) != 0) {
					fprintf(stderr, "reindex VAR source binding missing operation=%u\n", operation_id);
					return -1;
				}
				const struct prototype_context* source_binding = prototype_context_get(
					&ctx->metadata->contexts, source_binding_context
				);
				uint32_t source_classifier = source_binding ?
					prototype_context_classifier_term(source_binding) :
					PROTOTYPE_INVALID_ID;
				if (source_classifier == PROTOTYPE_INVALID_ID) {
					fprintf(stderr, "reindex VAR source classifier missing operation=%u\n", operation_id);
					return -1;
				}
				int source_status = operation_solver_require_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					action->target_context,
					operation->source_core_term,
					source_classifier
				);
				if (source_status > 0) {
					/* A pattern field may already have constructor-field evidence in
					 * this Context. Only synthesize a plain binder assumption when no
					 * more specific derivation exists. */
					prototype_judgement_delta_set_context(
						&ctx->judgement_delta, action->target_context
					);
					prototype_judgement_delta_set_occurrence(
						&ctx->judgement_delta, PROTOTYPE_INVALID_ID
					);
					if (prototype_judgement_delta_record_context_binding_assumption(
							&ctx->judgement_delta,
							ctx->terms,
							ctx->terms->terms[
								operation->source_core_term
							].as.var.binding_id,
							source_classifier
						) != 0) {
						return -1;
					}
					source_status = operation_solver_require_evidence_in_context(
						ctx,
						PROTOTYPE_INVALID_ID,
						action->target_context,
						operation->source_core_term,
						source_classifier
					);
				}
				if (source_status != 0) {
					return source_status;
				}
				struct prototype_judgement_selected_evidence source_evidence;
				if (operation_solver_select_evidence_in_context(
						ctx,
						PROTOTYPE_INVALID_ID,
						action->target_context,
						operation->source_core_term,
						source_classifier,
						&source_evidence
					) != 0) {
					fprintf(stderr, "reindex VAR source evidence selection failed operation=%u\n",
						operation_id);
					return -1;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				struct prototype_judgement_selected_evidence reindexed_evidence;
				if (prototype_judgement_delta_record_substitution_reindex(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&source_evidence,
						operation->context_action_substitution,
						&reindexed_evidence
					) != 0 || reindexed_evidence.subject != core_term) {
					fprintf(stderr, "reindex VAR derivation failed operation=%u source=%u core=%u\n",
						operation_id, reindexed_evidence.subject, core_term);
					return -1;
				}
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, operation_id
				);
				if (prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&reindexed_evidence,
						classifier,
						core_term
					) != 0) {
					fprintf(stderr,
						"reindex VAR classifier exposure failed operation=%u "
						"actual=%u expected=%u source-classifier=%u action=%u "
						"source-context=%u target-context=%u\n",
						operation_id, reindexed_evidence.classifier, classifier,
						source_classifier, operation->context_action_substitution,
						action->target_context, action->source_context);
					return -1;
				}
				return 0;
			}
			if (term->tag != PROTOTYPE_TERM_VAR) {
				return -1;
			}
			int status = operation_solver_require_evidence_in_context(
				ctx,
				PROTOTYPE_INVALID_ID,
				operation->context_id,
				core_term,
				classifier
			);
			if (status <= 0) {
				return status;
			}
			for (uint32_t lambda_operation_id = 0;
				lambda_operation_id < ctx->metadata->typed_occurrences.occurrence_count;
				++lambda_operation_id) {
				const struct prototype_typed_occurrence* lambda_operation =
					&ctx->metadata->typed_occurrences.occurrences[lambda_operation_id];
				uint32_t lambda_body_occurrence;
				uint8_t visited[4096];
				if (lambda_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
					lambda_operation->referenced_ast_binder_id !=
						operation->referenced_ast_binder_id ||
					lambda_operation->binder_classifier == PROTOTYPE_INVALID_ID ||
					!(prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						lambda_operation->binder_classifier,
						classifier
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					continue;
				}
				if (compile_occurrence_child(
						ctx, lambda_operation_id,
						PROTOTYPE_TERM_CHILD_BODY, 0,
						&lambda_body_occurrence
					) != 0) {
					continue;
				}
				memset(visited, 0, sizeof(visited));
				if (operation_subtree_contains_operation(
						ctx,
						lambda_body_occurrence,
						operation_id,
						visited
					) <= 0) {
					continue;
				}
				uint32_t binder_context =
					ctx->metadata->typed_occurrences.occurrences[lambda_body_occurrence].context_id;
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, binder_context
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				if (prototype_judgement_delta_record_context_binding_assumption(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->terms->terms[core_term].as.var.binding_id,
						classifier
					) != 0) {
					return -1;
				}
				status = operation_solver_require_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					operation->context_id,
					core_term,
					classifier
				);
				return status == 0 ? 0 : status;
			}
			return 1;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE: {
			if (operation->wrapped_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			int child_status = operation_solver_reify_core_proof(
				ctx, operation->wrapped_occurrence, core_term, depth + 1
			);
			if (child_status != 0) {
				return child_status;
			}
			int name_status = operation_solver_select_operation_evidence_in_context(
				ctx,
				operation->wrapped_occurrence,
				operation->context_id,
				core_term,
				classifier,
				NULL
			);
			return name_status;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH: {
			uint32_t scrutinee_occurrence;
			if (term->tag != PROTOTYPE_TERM_MATCH ||
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
					&scrutinee_occurrence
				) != 0 ||
				operation->first_case + operation->case_count >
					ctx->metadata->typed_occurrences.case_count ||
				term->as.match.case_count != operation->case_count) {
				return -1;
			}
			int status = operation_solver_reify_core_proof(
				ctx,
				scrutinee_occurrence,
				term->as.match.scrutinee,
				depth + 1
			);
			if (status != 0) {
				return status;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				uint32_t case_body_occurrence;
				const struct prototype_typed_occurrence_match_case* operation_case =
					&ctx->metadata->typed_occurrences.cases[
						operation->first_case + i
					];
				if (operation_case->refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
					continue;
				}
				if (case_id >= ctx->terms->case_count ||
					compile_occurrence_child(
						ctx, operation_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
						&case_body_occurrence
					) != 0) {
					return -1;
				}
				status = operation_solver_reify_core_proof(
					ctx,
					case_body_occurrence,
					ctx->terms->cases[case_id].body,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
			}
			return operation_solver_require_evidence_in_context(
				ctx, operation_id, operation->context_id, core_term, classifier
			);
		}
		case PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE: {
			if (operation->wrapped_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			const struct prototype_typed_occurrence* body =
				&ctx->metadata->typed_occurrences.occurrences[operation->wrapped_occurrence];
			if (body->classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_core_proof(
				ctx, operation->wrapped_occurrence, body->core_term, depth + 1
			);
			if (status != 0) {
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			struct prototype_judgement_selected_evidence source_evidence;
			if (operation_solver_select_operation_evidence_in_context(
					ctx,
					operation->wrapped_occurrence,
					body->context_id,
					core_term,
					body->classifier,
					&source_evidence
				) != 0) {
				return -1;
			}
			int exposure_status =
				prototype_judgement_delta_record_expected_type_exposure(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				&source_evidence,
				classifier,
					core_term
				);
			return exposure_status;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_ATOM:
		case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR:
			if (operation->context_action_substitution != PROTOTYPE_INVALID_ID &&
				operation->source_classifier != PROTOTYPE_INVALID_ID) {
				const struct prototype_substitution* action =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						operation->context_action_substitution
					);
				if (!action || action->source_context != operation->context_id ||
					operation->source_core_term >= ctx->terms->term_count ||
					operation->source_classifier >= ctx->terms->term_count) {
					return -1;
				}
				struct prototype_judgement_selected_evidence source_evidence;
				int source_status = operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					action->target_context,
					operation->source_core_term,
					operation->source_classifier,
					&source_evidence
				);
				if (source_status != 0) {
					return source_status;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				struct prototype_judgement_selected_evidence reindexed_evidence;
				if (prototype_judgement_delta_record_substitution_reindex(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						&source_evidence,
						operation->context_action_substitution,
						&reindexed_evidence
					) != 0 || reindexed_evidence.subject != core_term) {
					return -1;
				}
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, operation_id
				);
				return prototype_judgement_delta_record_expected_type_exposure(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					&reindexed_evidence,
					classifier,
					core_term
				);
			}
			if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR) {
				int existing_status = operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					operation->context_id,
					core_term,
					classifier,
					NULL
				);
				if (existing_status <= 0) {
					return existing_status;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				return prototype_judgement_delta_record_constructor_intro(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier
				);
			}
			return 0;
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			return 0;
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST: {
			uint32_t request_operation;
			uint32_t request_argument;
			uint32_t request_continuation;
			if (term->tag != PROTOTYPE_TERM_OPERATION_REQUEST ||
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
					&request_operation
				) != 0 || compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0,
					&request_argument
				) != 0 || compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
					&request_continuation
				) != 0) {
				return -1;
			}
			{
				const struct prototype_typed_occurrence* function =
					&ctx->metadata->typed_occurrences.occurrences[request_operation];
				const struct prototype_typed_occurrence* argument =
					&ctx->metadata->typed_occurrences.occurrences[request_argument];
				if (function->classifier == PROTOTYPE_INVALID_ID ||
					argument->classifier == PROTOTYPE_INVALID_ID) {
					return 1;
				}
				int status = operation_solver_reify_core_proof(
					ctx,
					request_operation,
					term->as.operation_request.operation,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
				status = operation_solver_reify_core_proof(
					ctx,
					request_argument,
					term->as.operation_request.argument,
					depth + 1
				);
				if (status != 0) {
					return status;
				}
				uint32_t application;
				if (prototype_term_app(
						ctx->terms,
						term->as.operation_request.operation,
						term->as.operation_request.argument,
						&application
					) != 0) {
					return -1;
				}
				struct prototype_judgement_selected_evidence function_evidence;
				status = operation_solver_select_operation_evidence_in_context(
					ctx,
					request_operation,
					operation->context_id,
					term->as.operation_request.operation,
					function->classifier,
					&function_evidence
				);
				if (status != 0) {
					return status;
				}
				struct prototype_judgement_selected_evidence argument_evidence;
				status = operation_solver_select_operation_evidence_in_context(
					ctx,
					request_argument,
					operation->context_id,
					term->as.operation_request.argument,
					argument->classifier,
					&argument_evidence
				);
				if (status != 0) {
					return status;
				}
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				uint32_t application_classifier;
				if (prototype_judgement_delta_app_elim_classifier(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						function->classifier,
						term->as.operation_request.argument,
						argument->classifier,
						&application_classifier
					) != 0 || prototype_judgement_delta_record_app_elim(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						application,
						application_classifier,
						&function_evidence,
						&argument_evidence
					) != 0) {
					return -1;
				}
			}
			(void)request_continuation;
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			return prototype_judgement_delta_record_computation_constraint(
				&ctx->judgement_delta,
				ctx->terms,
				operation->context_id,
				core_term
			);
		}
		default:
			return -1;
	}
}

static int operation_solver_validate_solution_projection(
	const struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct operation_solver_solution* solution =
			&ctx->classifier_solver.solutions[i];
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (solution->classifier != operation->classifier) {
			return -1;
		}
		if (solution->classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		struct prototype_term_classifier_view view;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				solution->classifier,
				&view
			) != 0) {
			return -1;
		}
		if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
			solution->effect_row != view.effect_row) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_materialize_judgements(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	memset(
		ctx->operation_reindex_cache,
		0,
		sizeof(ctx->operation_reindex_cache)
	);
	memset(
		ctx->metadata->substitutions.reindex_cache,
		0,
		sizeof(ctx->metadata->substitutions.reindex_cache)
	);
	/* Constructor-domain checking may establish a more precise classifier for
	 * a higher-order argument than standalone synthesis can choose (notably an
	 * empty indexed Match). Commit those already-checked occurrence classifiers
	 * once, after fixed-point solving and before constructing the proof DAG. */
	ctx->materializing_checked_classifiers = 1;
	uint32_t mutable_occurrence_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t i = mutable_occurrence_start;
		i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
			operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			continue;
		}
		int ignored_changed = 0;
		if (operation_solver_propagate_constructor_domains(
				ctx, i, &ignored_changed
			) != 0) {
			fprintf(stderr,
				"judgement materialization: constructor domain propagation failed "
				"operation=%u context=%u core=%u classifier=%u\n",
				i, operation->context_id, operation->core_term,
				operation->classifier);
			ctx->materializing_checked_classifiers = 0;
			return -1;
		}
	}
	for (uint32_t i = mutable_occurrence_start;
		i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		uint32_t classifier = operation_solver_classifier(ctx, i);
		if (classifier == PROTOTYPE_INVALID_ID) {
			uint32_t obligation_id = PROTOTYPE_INVALID_ID;
			int obligation_status =
				ctx->metadata->typed_occurrences.occurrences[i].tag ==
					PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ?
				prototype_verification_db_find_occurrence(
					&ctx->metadata->verification,
					PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
					i,
					&obligation_id
				) : 1;
			if (obligation_status == 0 && obligation_id <
					prototype_verification_db_count(&ctx->metadata->verification)) {
				ctx->metadata->typed_occurrences.occurrences[i].classifier =
					PROTOTYPE_INVALID_ID;
				ctx->metadata->typed_occurrences.occurrences[i].classifier_status =
					PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_RESIDUAL_VERIFICATION;
				ctx->metadata->typed_occurrences.occurrences[i].
					classifier_verification_obligation = obligation_id;
				continue;
			}
			ctx->materializing_checked_classifiers = 0;
			fprintf(stderr,
				"judgement materialization: unsolved classifier occurrence=%u tag=%d "
				"hint=%u core=%u core-tag=%u wrapper=%u source_ast=%u context=%u "
				"binding=%u referenced_ast_binder=%u\n",
				i,
				ctx->metadata->typed_occurrences.occurrences[i].tag,
				ctx->classifier_hints[i],
				ctx->metadata->typed_occurrences.occurrences[i].core_term,
				ctx->metadata->typed_occurrences.occurrences[i].core_term <
					ctx->terms->term_count ? ctx->terms->terms[
						ctx->metadata->typed_occurrences.occurrences[i].core_term
					].tag : 0,
				ctx->metadata->typed_occurrences.occurrences[i].wrapped_occurrence,
				ctx->metadata->typed_occurrences.occurrences[i].source_ast,
				ctx->metadata->typed_occurrences.occurrences[i].context_id,
				ctx->metadata->typed_occurrences.occurrences[i].binding_id,
				ctx->metadata->typed_occurrences.occurrences[i].referenced_ast_binder_id
			);
			return -1;
		}
		ctx->classifier_hints[i] = classifier;
		ctx->metadata->typed_occurrences.occurrences[i].classifier = classifier;
		ctx->metadata->typed_occurrences.occurrences[i].classifier_status =
			PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED;
		ctx->metadata->typed_occurrences.occurrences[i].
			classifier_verification_obligation = PROTOTYPE_INVALID_ID;
	}
	ctx->materializing_checked_classifiers = 0;
	if (operation_solver_validate_solution_projection(ctx) != 0) {
		fprintf(stderr, "judgement materialization: occurrence projection failed\n");
		return -1;
	}
	if (materialize_pending_binder_assumptions(ctx) != 0 ||
		materialize_pending_declaration_facts(ctx) != 0) {
		fprintf(stderr, "judgement materialization: pending input facts failed\n");
		return -1;
	}
	if (operation_solver_materialize_match_pattern_assumptions(ctx) != 0) {
		fprintf(stderr, "judgement materialization: match assumptions failed\n");
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		uint32_t action =
			ctx->metadata->typed_occurrences.occurrences[i].context_action_substitution;
		if (action != PROTOTYPE_INVALID_ID &&
			operation_solver_materialize_refinement_binding_evidence_at_depth(
				ctx, action, 0
			) != 0) {
			fprintf(stderr,
				"judgement materialization: operation action evidence failed "
				"operation=%u substitution=%u\n",
				i, action);
			return -1;
		}
	}
	for (;;) {
		size_t before_relation_count = ctx->judgement_delta.proposition_count;
		for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
			if (ctx->metadata->typed_occurrences.occurrences[i].tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
				continue;
			}
			int match_status = operation_solver_materialize_match_judgement(
				ctx, i
			);
			if (match_status < 0) {
				fprintf(stderr,
					"judgement materialization: match motive failed operation=%u\n",
					i);
				return -1;
			}
		}
		for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
			const struct prototype_typed_occurrence* operation =
				&ctx->metadata->typed_occurrences.occurrences[i];
			int impossible_branch =
				operation_solver_occurrence_is_in_impossible_branch(ctx, i);
			if (impossible_branch < 0) {
				return -1;
			}
			if (impossible_branch > 0) {
				continue;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, i
			);
			uint32_t classifier = operation->classifier;
			if (classifier == PROTOTYPE_INVALID_ID) {
				continue;
			}
			int free_effect_row = operation_classifier_contains_unowned_effect_row(
				ctx, i, classifier
			);
			if (free_effect_row < 0) {
				return -1;
			}
			if (free_effect_row != 0) {
				/* The TypedOccurrenceGraph may retain this constraint state, but a
				 * JudgementDB conclusion must be closed or explicitly generalized. */
				continue;
			}
			if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP ||
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE ||
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ||
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ||
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_FORCE ||
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REQUEST) {
				int reify_status = operation_solver_reify_core_proof(
					ctx, i, operation->core_term, 0
				);
				if (reify_status < 0) {
					fprintf(
						stderr,
						"judgement materialization: reify failed operation=%u tag=%d\n",
						i,
						operation->tag
					);
					return -1;
				}
				if (reify_status > 0) {
					continue;
				}
			} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
				if (operation_solver_materialize_induction_hypothesis_judgement(
						ctx, i
					) != 0) {
					fprintf(stderr, "judgement materialization: IH failed operation=%u classifier=%u\n",
						i, classifier);
					return -1;
				}
			} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
				prototype_term_type_instance_info(
					ctx->terms,
					operation->core_term,
					&(uint32_t){0},
					NULL,
					&(uint32_t){0}
				) == 0 &&
				prototype_judgement_delta_record_type_formation(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_PURE_PRIMITIVE &&
				prototype_judgement_delta_record_pure_primitive_type(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_EFFECT_OPERATION &&
				prototype_judgement_delta_record_effect_operation_type(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation->core_term,
					operation->classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_TEXT_LITERAL &&
				prototype_judgement_delta_record_text_literal(
					&ctx->judgement_delta,
					ctx->terms,
					operation->core_term,
					classifier
				) != 0) {
				return -1;
			} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_INT_LITERAL) {
				if (prototype_judgement_delta_record_int_literal(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->intrinsic_environment,
						operation->core_term,
						classifier
					) != 0) {
					return -1;
				}
				}
			}
			/* Computation constraints are generated and solved before this phase, but
			 * their replayable derivations require premise evidence materialized above.
			 * Re-run only the checked rule closure; operation solutions are validated
			 * again and are never selected here. */
			if (operation_solver_refresh_computation_constraint_operands(ctx) != 0 ||
				prototype_judgement_delta_solve_recorded_computation_requests(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					ctx->computation_constraint_results,
					ctx->computation_constraint_result_capacity
				) != 0 ||
				prototype_judgement_delta_solve_recorded_computation_constraints(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					ctx->computation_constraint_results,
					ctx->computation_constraint_result_capacity
				) != 0 || operation_solver_validate_solution_projection(ctx) != 0) {
				return -1;
			}
			if (ctx->judgement_delta.proposition_count == before_relation_count) {
				break;
		}
	}
	if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	/* Match motive results are a construction-time lookup index. The replayable
	 * derivations above are the persistent proof state. */
	ctx->judgement_delta.match_motive_result_count = 0;
	return 0;
}

static int operation_solver_context_classifier_from_lift(
	struct compile_context* ctx,
	uint32_t context_id,
	const uint32_t* relocation,
	uint32_t resolved_context_count,
	const uint32_t* first_lift_by_source,
	const uint32_t* next_lift,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !relocation || !first_lift_by_source ||
		!next_lift || !p_classifier ||
		context_id >= ctx->metadata->contexts.context_count) {
		return -1;
	}
	const struct prototype_context* context = prototype_context_get(
		&ctx->metadata->contexts, context_id
	);
	if (!context) {
		return -1;
	}
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	for (uint32_t substitution_id = first_lift_by_source[context_id];
		substitution_id != PROTOTYPE_INVALID_ID;
		substitution_id = next_lift[substitution_id]) {
		if (substitution_id >=
			ctx->metadata->substitutions.substitution_count) {
			return -1;
		}
		const struct prototype_substitution* substitution =
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution_id
			);
		if (!substitution ||
			substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND ||
			substitution->source_context != context_id ||
			substitution->target_context == context_id ||
			substitution->term >= ctx->terms->term_count ||
			ctx->terms->terms[substitution->term].tag != PROTOTYPE_TERM_VAR ||
			ctx->terms->terms[substitution->term].as.var.binding_id !=
				context->binding_id) {
			continue;
		}
		const struct prototype_context* target = prototype_context_get(
			&ctx->metadata->contexts, substitution->target_context
		);
		const struct prototype_substitution* prefix =
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution->first
			);
		if (!target || !prefix || target->binding_id != context->binding_id ||
			target->parent != prefix->target_context ||
			prefix->source_context != context_id) {
			continue;
		}
		uint32_t target_classifier = prototype_context_classifier_term(target);
		if (target_classifier == PROTOTYPE_INVALID_ID &&
			substitution->target_context < resolved_context_count) {
			const struct prototype_context* resolved_target = prototype_context_get(
				&ctx->metadata->contexts,
				relocation[substitution->target_context]
			);
			target_classifier = resolved_target ?
				prototype_context_classifier_term(resolved_target) :
				PROTOTYPE_INVALID_ID;
		}
		if (target_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t candidate;
		if (prototype_term_reindex(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				target_classifier,
				substitution->first,
				&candidate
			) != 0) {
			return -1;
		}
		if (classifier != PROTOTYPE_INVALID_ID &&
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				classifier,
				candidate
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		classifier = candidate;
	}
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = classifier;
	return 0;
}

static int operation_solver_resolve_contexts(
	struct compile_context* ctx,
	int finalize
) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->contexts.context_count > PROTOTYPE_CONTEXT_CAPACITY ||
		ctx->metadata->substitutions.substitution_count >
			PROTOTYPE_SUBSTITUTION_CAPACITY) {
		return -1;
	}
	ctx->metadata->context_resolution_request_count++;
	if (ctx->context_resolution_valid &&
		ctx->context_resolution_dependency_revision ==
			ctx->context_dependency_revision &&
		ctx->context_resolution_context_count ==
			ctx->metadata->contexts.context_count &&
		(!finalize || ctx->context_resolution_finalized)) {
		ctx->metadata->context_resolution_skip_count++;
		return 0;
	}
	ctx->metadata->context_resolution_pass_count++;
	if (operation_binder_owners_ensure(ctx) != 0) {
		return -1;
	}
	/* Existing substitutions are immutable.  Rebased morphisms are interned as
	 * new records, so reindex results keyed by old IDs remain valid. */
	uint32_t relocation[PROTOTYPE_CONTEXT_CAPACITY];
	uint8_t context_dirty[PROTOTYPE_CONTEXT_CAPACITY];
	uint8_t context_semantics_changed[PROTOTYPE_CONTEXT_CAPACITY];
	uint8_t substitution_live[PROTOTYPE_SUBSTITUTION_CAPACITY];
	uint8_t substitution_rebased[PROTOTYPE_SUBSTITUTION_CAPACITY];
	uint32_t substitution_relocation[PROTOTYPE_SUBSTITUTION_CAPACITY];
	uint32_t first_lift_by_source[PROTOTYPE_CONTEXT_CAPACITY];
	uint32_t next_lift[PROTOTYPE_SUBSTITUTION_CAPACITY];
	size_t source_count = ctx->metadata->contexts.context_count;
	size_t substitution_count =
		ctx->metadata->substitutions.substitution_count;
	int all_contexts_dirty = ctx->context_dependencies_all_dirty ||
		ctx->context_resolution_context_count == 0 ||
		(finalize && !ctx->context_resolution_finalized);
	int trace_context_resolution =
		getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE") != NULL;
	if (trace_context_resolution) {
		fprintf(
			stderr,
			"A_PROGRAM_CONTEXT_RESOLUTION_TRACE 1 revision=%" PRIu64
			" source_contexts=%zu all_dirty=%d finalize=%d\n",
			ctx->context_dependency_revision, source_count,
			all_contexts_dirty, finalize
		);
	}
	memset(substitution_live, 0, sizeof(substitution_live));
#define MARK_LIVE_SUBSTITUTION(substitution_id) \
	do { \
		uint32_t live_id = (substitution_id); \
		if (live_id != PROTOTYPE_INVALID_ID) { \
			if (live_id >= substitution_count) { \
				return -1; \
			} \
			substitution_live[live_id] = 1; \
		} \
	} while (0)
	/* SubstitutionDB is a monotone candidate arena. Context resolution must
	 * update only actions reachable from the current constraint/proof graph;
	 * rejected alternatives are not semantic roots and may contain deliberately
	 * incomplete candidate payloads. */
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		MARK_LIVE_SUBSTITUTION(
			ctx->metadata->typed_occurrences.occurrences[i].
				context_action_substitution
		);
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.case_count;
		++i) {
		const struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[i];
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			MARK_LIVE_SUBSTITUTION(operation_case->refinement_substitution);
		}
	}
	for (uint32_t i = 0; i < ctx->constraints.constraint_count; ++i) {
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[i];
		if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT &&
			constraint->payload.branch_refinement.refinement_status ==
				PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
			MARK_LIVE_SUBSTITUTION(
				constraint->payload.branch_refinement.substitution
			);
		}
	}
	/* Accepted Derivations retain the immutable Context morphisms under which
	 * they were checked. Only mutable candidate roots participate in rebasing. */
	for (size_t i = 0;
		i < ctx->judgement_delta.derivation_candidate_count;
		++i) {
		const struct prototype_judgement_derivation_candidate* derivation =
			&ctx->judgement_delta.derivation_candidates[i];
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			MARK_LIVE_SUBSTITUTION(derivation->semantic_action_id);
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
				MARK_LIVE_SUBSTITUTION(
					derivation->premises[j].semantic_action_id
				);
			}
		}
	}
	for (size_t reverse = substitution_count; reverse > 0; --reverse) {
		uint32_t substitution_id = (uint32_t)(reverse - 1);
		if (!substitution_live[substitution_id]) {
			continue;
		}
		const struct prototype_substitution* substitution =
			&ctx->metadata->substitutions.substitutions[substitution_id];
		if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
			substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
			MARK_LIVE_SUBSTITUTION(substitution->first);
		}
		if (substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) {
			MARK_LIVE_SUBSTITUTION(substitution->second);
		}
	}
#undef MARK_LIVE_SUBSTITUTION
	for (uint32_t context_id = 0; context_id < source_count; ++context_id) {
		first_lift_by_source[context_id] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t substitution_id = 0;
		substitution_id < ctx->metadata->substitutions.substitution_count;
		++substitution_id) {
		next_lift[substitution_id] = PROTOTYPE_INVALID_ID;
		const struct prototype_substitution* substitution =
			prototype_substitution_get(
				&ctx->metadata->substitutions, substitution_id
			);
		if (!substitution ||
			!substitution_live[substitution_id] ||
			substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND ||
			substitution->source_context >= source_count) {
			continue;
		}
		next_lift[substitution_id] =
			first_lift_by_source[substitution->source_context];
		first_lift_by_source[substitution->source_context] = substitution_id;
	}
	relocation[0] = 0;
	context_dirty[0] = all_contexts_dirty;
	context_semantics_changed[0] = 0;
	for (uint32_t context_id = 1; context_id < source_count; ++context_id) {
		const struct prototype_context* context = prototype_context_get(
			&ctx->metadata->contexts, context_id
		);
		if (!context || context->parent >= context_id) {
			fprintf(stderr, "context resolution invalid context=%u parent=%u source-count=%zu\n",
				context_id, context ? context->parent : PROTOTYPE_INVALID_ID,
				source_count);
			return -1;
		}
		int binding_dirty = operation_context_binding_is_dirty(
			ctx, context->binding_id
		);
		context_dirty[context_id] = all_contexts_dirty ||
			context_id >= ctx->context_resolution_context_count ||
			context_dirty[context->parent] ||
			binding_dirty;
		if (!context_dirty[context_id]) {
			relocation[context_id] = context_id;
			context_semantics_changed[context_id] = 0;
			continue;
		}
		ctx->metadata->context_resolution_context_visit_count++;
		if (trace_context_resolution) {
			fprintf(
				stderr,
				"A_PROGRAM_CONTEXT_RESOLUTION_TRACE 1 revision=%" PRIu64
				" context=%u parent=%u binding=%u cause=%s\n",
				ctx->context_dependency_revision, context_id, context->parent,
				context->binding_id,
				all_contexts_dirty ? "global" :
					(context_id >= ctx->context_resolution_context_count ? "new" :
						(binding_dirty ? "binding" : "parent"))
			);
		}
		uint32_t classifier = prototype_context_classifier_term(context);
		uint32_t classifier_variable =
			prototype_context_classifier_variable(context);
		/* A concrete Context classifier is already expressed in its parent
		 * Context.  In particular, comprehension has reindexed dependent binder
		 * types through the branch action.  BindingId identifies the binder
		 * object, but it does not make those context-indexed classifier terms
		 * interchangeable.  Consult the owner only for an unresolved extension. */
		int classifier_variable_resolved = 0;
		uint32_t lifted_classifier;
		int lift_status = operation_solver_context_classifier_from_lift(
			ctx,
			context_id,
			relocation,
			context_id,
			first_lift_by_source,
			next_lift,
			&lifted_classifier
		);
		if (lift_status < 0) {
			return -1;
		}
		if (lift_status == 0) {
			classifier = lifted_classifier;
			classifier_variable_resolved = 1;
		} else {
			/* The direct Lambda body Context is a projection of the current solved
			 * binder occurrence. It is first created from the surface annotation, but
			 * classifier/effect constraints may refine that occurrence in a later
			 * fixed-point round. Reindexed branch Contexts may carry a genuinely
			 * different dependent classifier and are deliberately excluded by the
			 * parent check. */
			struct operation_binder_owner_entry* owner =
				operation_binder_owner_find(ctx, context->binding_id);
			if (owner && owner->lambda_occurrence != PROTOTYPE_INVALID_ID) {
				if (owner->lambda_occurrence >=
						ctx->metadata->typed_occurrences.occurrence_count) {
					return -1;
				}
				const struct prototype_typed_occurrence* lambda =
					&ctx->metadata->typed_occurrences.occurrences[
						owner->lambda_occurrence
					];
				if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
					lambda->binding_id != context->binding_id) {
					return -1;
				}
				if (context->parent == lambda->context_id &&
					lambda->binder_classifier != PROTOTYPE_INVALID_ID) {
					classifier = lambda->binder_classifier;
					classifier_variable_resolved = 1;
				}
			}
			if (!classifier_variable_resolved &&
				classifier != PROTOTYPE_INVALID_ID) {
				classifier_variable_resolved = 1;
			}
		}
		if (!classifier_variable_resolved) {
			uint32_t owner_classifier;
			int owner_status = operation_binder_owner_classifier(
				ctx, context->binding_id, &owner_classifier
			);
			if (owner_status < 0) {
				return -1;
			}
			if (owner_status == 0) {
				classifier = owner_classifier;
				classifier_variable_resolved = 1;
			}
		}
		if (classifier_variable != PROTOTYPE_INVALID_ID) {
			int occurrence_classifier_resolved = 0;
			struct operation_binder_owner_entry* evidence_owner =
				operation_binder_owner_find(ctx, context->binding_id);
			for (uint32_t evidence_id = evidence_owner ?
					evidence_owner->first_classifier_evidence :
					PROTOTYPE_INVALID_ID;
				evidence_id != PROTOTYPE_INVALID_ID;
				evidence_id =
					ctx->binder_classifier_evidence[evidence_id].next) {
				if (evidence_id >= ctx->binder_classifier_evidence_count) {
					return -1;
				}
				uint32_t operation_id =
					ctx->binder_classifier_evidence[evidence_id].occurrence_id;
				if (operation_id >=
					ctx->metadata->typed_occurrences.occurrence_count) {
					return -1;
				}
				const struct prototype_typed_occurrence* operation =
					&ctx->metadata->typed_occurrences.occurrences[operation_id];
				uint32_t operation_classifier =
					operation_available_classifier(ctx, operation_id);
				uint32_t operation_binding_context;
				if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
					operation->referenced_ast_binder_id != classifier_variable ||
					operation->binding_id != context->binding_id ||
					operation_classifier == PROTOTYPE_INVALID_ID ||
					prototype_context_find_binding(
						&ctx->metadata->contexts,
						operation->context_id,
						operation->binding_id,
						&operation_binding_context
					) != 0 || operation_binding_context != context_id) {
					continue;
				}
				if (occurrence_classifier_resolved &&
					prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						classifier,
						operation_classifier
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					fprintf(stderr,
						"context resolution occurrence classifier conflict "
						"context=%u operation=%u\n",
						context_id, operation_id);
					return -1;
				}
				classifier = operation_classifier;
				occurrence_classifier_resolved = 1;
				classifier_variable_resolved = 1;
			}
			for (uint32_t pending_id = 0;
				!classifier_variable_resolved &&
				pending_id < ctx->pending_binder_assumption_count;
				++pending_id) {
				const struct pending_binder_assumption* pending =
					&ctx->pending_binder_assumptions[pending_id];
				uint32_t assumption_context_id;
				if (pending->binder_var >= ctx->terms->term_count ||
					ctx->terms->terms[pending->binder_var].tag !=
						PROTOTYPE_TERM_VAR ||
					prototype_context_find_binding(
						&ctx->metadata->contexts,
						pending->context_id,
						ctx->terms->terms[pending->binder_var].as.var.binding_id,
						&assumption_context_id
					) != 0 || assumption_context_id != context_id) {
					continue;
				}
				if (classifier_variable_resolved &&
					prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						classifier,
						pending->classifier
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					fprintf(stderr,
						"context resolution assumption classifier conflict context=%u "
						"pending=%u binding=%u classifier=%u pending-classifier=%u "
						"source-operation=%u\n",
						context_id, pending_id, context->binding_id, classifier,
						pending->classifier, pending->source_operation);
					return -1;
				}
				classifier = pending->classifier;
				classifier_variable_resolved = 1;
			}
			/* A provisional Context may already carry the source-scope classifier.
			 * Refined occurrences use distinct Context extensions and must not
			 * overwrite that source authority merely because they retain the same
			 * surface binder identity. */
			if (!classifier_variable_resolved &&
				classifier != PROTOTYPE_INVALID_ID) {
				classifier_variable_resolved = 1;
			}
			for (int source = 0;
				classifier == PROTOTYPE_INVALID_ID && source < 2;
				++source) {
				const struct prototype_judgement_proposition* relations = source == 0 ?
					ctx->judgement_delta.propositions : ctx->judgement->propositions;
				const struct prototype_judgement_derivation_candidate* derivations = source == 0 ?
					ctx->judgement_delta.derivation_candidates :
					ctx->judgement->derivation_candidates;
				size_t proposition_count = source == 0 ?
					ctx->judgement_delta.proposition_count : ctx->judgement->proposition_count;
				size_t derivation_candidate_count = source == 0 ?
					ctx->judgement_delta.derivation_candidate_count :
					ctx->judgement->derivation_candidate_count;
				for (size_t relation_id = 0; relation_id < proposition_count; ++relation_id) {
					const struct prototype_judgement_proposition* relation =
						&relations[relation_id];
					if (relation->context_id != context_id ||
						relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						prototype_judgement_candidate_find_derivation_kind(
							relations,
							proposition_count,
							derivations,
							derivation_candidate_count,
							(uint32_t)relation_id,
							PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION,
							&(uint32_t){0}
						) != 0 ||
						relation->subject >= ctx->terms->term_count ||
						ctx->terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
						ctx->terms->terms[relation->subject].as.var.binding_id !=
							context->binding_id) {
						continue;
					}
					classifier = relation->classifier;
					classifier_variable_resolved = 1;
					break;
				}
			}
		}
		size_t context_count_before = ctx->metadata->contexts.context_count;
		if (prototype_context_extend(
				&ctx->metadata->contexts,
				relocation[context->parent],
				context->binding_id,
				classifier,
				finalize && classifier_variable_resolved ?
					PROTOTYPE_INVALID_ID : classifier_variable,
				&relocation[context_id]
			) != 0) {
			fprintf(stderr, "context resolution extension failed context=%u parent=%u classifier=%u variable=%u\n",
				context_id, context->parent, classifier, classifier_variable);
			return -1;
		}
		if (ctx->metadata->contexts.context_count != context_count_before) {
			ctx->metadata->context_resolution_context_insert_count++;
		}
		if (relocation[context_id] != context_id) {
			ctx->metadata->context_resolution_context_change_count++;
		}
		context_semantics_changed[context_id] =
			context_semantics_changed[context->parent] ||
			(prototype_context_classifier_term(context) != PROTOTYPE_INVALID_ID &&
			 prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				prototype_context_classifier_term(context),
				classifier
			 ).status != PROTOTYPE_TERM_CONVERSION_EQUAL);
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		uint32_t context_id = ctx->metadata->typed_occurrences.occurrences[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->metadata->typed_occurrences.occurrences[i].context_id =
				relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (uint32_t i = 0; i < ctx->constraints.constraint_count; ++i) {
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[i];
		uint32_t context_id = constraint->context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			constraint->context_id = relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
		if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT) {
			uint32_t source_context =
				constraint->payload.branch_refinement.source_context;
			uint32_t refined_context =
				constraint->payload.branch_refinement.refined_context;
			if (source_context >= source_count ||
				(refined_context != PROTOTYPE_INVALID_ID &&
				 refined_context >= source_count)) {
				return -1;
			}
			if (relocation[source_context] != source_context) {
				constraint->payload.branch_refinement.source_context =
					relocation[source_context];
				ctx->metadata->context_resolution_root_projection_count++;
			}
			if (refined_context != PROTOTYPE_INVALID_ID) {
				if (relocation[refined_context] != refined_context) {
					constraint->payload.branch_refinement.refined_context =
						relocation[refined_context];
					ctx->metadata->context_resolution_root_projection_count++;
				}
			}
		}
	}
	/* Context canonicalization may turn a previously unresolved comprehension
	 * classifier into a concrete binder type. The occurrence topology and all
	 * unrelated classifier solutions remain valid; refresh only the persistent
	 * binder-lookup constraints and wake their reverse dependents. */
	for (uint32_t id = 0;
		id < ctx->constraints.classifier_constraint_count;
		++id) {
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER ||
			constraint->kind != OPERATION_CONSTRAINT_BINDER_TYPE) {
			continue;
		}
		uint32_t operation_id = constraint->source_occurrence;
		uint32_t binding_context;
		int binding_status = operation_solver_var_binding_context(
			ctx, operation_id, &binding_context
		);
		if (binding_status < 0) {
			fprintf(stderr,
				"context binder projection lookup failed operation=%u context=%u\n",
				operation_id,
				ctx->metadata->typed_occurrences.occurrences[operation_id].context_id);
			return -1;
		}
		constraint->payload.classifier.goal.binder_type.binding_context =
			binding_status == 0 ? binding_context : PROTOTYPE_INVALID_ID;
	}
	for (uint32_t operation_id = 0;
		ctx->classifier_solver.initialized &&
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
			ctx->classifier_solver.solutions[operation_id].classifier !=
				PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t binding_context;
		int binding_status = operation_solver_var_binding_context(
			ctx, operation_id, &binding_context
		);
		if (binding_status < 0) {
			return -1;
		}
		if (binding_status != 0) {
			continue;
		}
		const struct prototype_context* binding = prototype_context_get(
			&ctx->metadata->contexts, binding_context
		);
		if (!binding) {
			fprintf(stderr,
				"context binder projection missing Context operation=%u binding-context=%u\n",
				operation_id, binding_context);
			return -1;
		}
		uint32_t classifier = prototype_context_classifier_term(binding);
		if (classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		operation_solver_store_classifier_solution(ctx, operation_id, classifier);
		if (operation_solver_enqueue_dependents(ctx, operation_id) != 0) {
			fprintf(stderr,
				"context binder projection enqueue failed operation=%u\n",
				operation_id);
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.case_count; ++i) {
		uint32_t context_id = ctx->metadata->typed_occurrences.cases[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->metadata->typed_occurrences.cases[i].context_id =
				relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.fold_clause_count; ++i) {
		uint32_t context_id = ctx->metadata->typed_occurrences.fold_clauses[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->metadata->typed_occurrences.fold_clauses[i].context_id =
				relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		uint32_t context_id = ctx->pending_binder_assumptions[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->pending_binder_assumptions[i].context_id = relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		uint32_t context_id = ctx->pending_declaration_facts[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->pending_declaration_facts[i].context_id = relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	for (size_t i = 0; i < ctx->judgement_delta.proposition_count; ++i) {
		struct prototype_judgement_proposition* proposition =
			&ctx->judgement_delta.propositions[i];
		uint32_t context_id = proposition->context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			proposition->context_id = relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	/* Rebase live morphisms into new immutable records. The original payload and
	 * ID remain valid for already accepted proof edges. Components are ordered
	 * before users by the SubstitutionDB invariant, so one forward pass suffices. */
	for (uint32_t i = 0; i < substitution_count; ++i) {
		substitution_relocation[i] = i;
		substitution_rebased[i] = 0;
		const struct prototype_substitution* substitution =
			prototype_substitution_get(&ctx->metadata->substitutions, i);
		if (!substitution || !substitution_live[i]) {
			continue;
		}
		if (substitution->source_context >= source_count ||
			substitution->target_context >= source_count ||
			((substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
			  substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) &&
			 substitution->first >= i) ||
			(substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE &&
			 substitution->second >= i)) {
			return -1;
		}
		uint32_t source_context = relocation[substitution->source_context];
		uint32_t target_context = relocation[substitution->target_context];
		uint32_t first = substitution->first == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_INVALID_ID : substitution_relocation[substitution->first];
		uint32_t second = substitution->second == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_INVALID_ID : substitution_relocation[substitution->second];
		if (context_semantics_changed[substitution->source_context] ||
			context_semantics_changed[substitution->target_context] ||
			((substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND ||
			  substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE) &&
			 !substitution_rebased[substitution->first]) ||
			(substitution->kind == PROTOTYPE_SUBSTITUTION_COMPOSE &&
			 !substitution_rebased[substitution->second])) {
			continue;
		}
		uint32_t term_classifier = substitution->term_classifier;
		if (substitution->kind == PROTOTYPE_SUBSTITUTION_EXTEND &&
			substitution->term < ctx->terms->term_count &&
			ctx->terms->terms[substitution->term].tag == PROTOTYPE_TERM_VAR) {
			uint32_t binding_context_id;
			if (prototype_context_find_binding(
					&ctx->metadata->contexts,
					source_context,
					ctx->terms->terms[substitution->term].as.var.binding_id,
					&binding_context_id
				) != 0) {
				fprintf(stderr,
					"context resolution found out-of-scope substitution term "
					"substitution=%u source=%u target=%u first=%u term=%u "
					"binding=%u\n",
					i, source_context, target_context,
					substitution->first, substitution->term,
					ctx->terms->terms[substitution->term].as.var.binding_id);
				return -1;
			}
			const struct prototype_context* binding_context = prototype_context_get(
				&ctx->metadata->contexts, binding_context_id
			);
			if (!binding_context) {
				return -1;
			}
			uint32_t binding_classifier =
				prototype_context_classifier_term(binding_context);
			if (binding_classifier == PROTOTYPE_INVALID_ID) {
				/* Context resolution participates in the classifier fixed point.  A
				 * branch substitution may therefore mention a binder whose classifier
				 * is still pending in a non-final pass.  Preserve that pending payload;
				 * the final pass below remains the publication boundary that rejects it. */
				if (finalize) {
					return -1;
				}
					term_classifier = PROTOTYPE_INVALID_ID;
				} else {
				term_classifier = binding_classifier;
				}
			}
		if (source_context == substitution->source_context &&
			target_context == substitution->target_context &&
			first == substitution->first && second == substitution->second &&
			term_classifier == substitution->term_classifier) {
			substitution_rebased[i] = 1;
			continue;
		}
		ctx->metadata->context_resolution_substitution_visit_count++;
		size_t substitution_count_before =
			ctx->metadata->substitutions.substitution_count;
		if (prototype_substitution_rebase(
				&ctx->metadata->substitutions,
				i,
				source_context,
				target_context,
				first,
				second,
				term_classifier,
				&substitution_relocation[i]
			) != 0) {
			return -1;
		}
		substitution_rebased[i] = 1;
		if (ctx->metadata->substitutions.substitution_count !=
			substitution_count_before) {
			ctx->metadata->context_resolution_substitution_insert_count++;
		}
		if (substitution_relocation[i] != i) {
			ctx->metadata->context_resolution_substitution_rebase_count++;
		}
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
			if (operation->context_action_substitution >= substitution_count ||
				!substitution_live[operation->context_action_substitution]) {
				return -1;
			}
			uint32_t action = operation->context_action_substitution;
			operation->context_action_substitution = substitution_relocation[action];
			if (operation->context_action_substitution != action) {
				ctx->metadata->context_resolution_root_projection_count++;
			}
		}
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.case_count;
		++i) {
		struct prototype_typed_occurrence_match_case* operation_case =
			&ctx->metadata->typed_occurrences.cases[i];
		if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
			if (operation_case->refinement_substitution >= substitution_count ||
				!substitution_live[operation_case->refinement_substitution]) {
				return -1;
			}
			uint32_t action = operation_case->refinement_substitution;
			operation_case->refinement_substitution = substitution_relocation[action];
			if (operation_case->refinement_substitution != action) {
				ctx->metadata->context_resolution_root_projection_count++;
			}
		}
	}
	int candidate_actions_changed = 0;
	for (size_t i = 0;
		i < ctx->judgement_delta.derivation_candidate_count;
		++i) {
		struct prototype_judgement_derivation_candidate* derivation =
			&ctx->judgement_delta.derivation_candidates[i];
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			if (derivation->semantic_action_id >= substitution_count ||
				!substitution_live[derivation->semantic_action_id]) {
				return -1;
			}
			uint32_t action = derivation->semantic_action_id;
			derivation->semantic_action_id = substitution_relocation[action];
			if (derivation->semantic_action_id != action) {
				candidate_actions_changed = 1;
				ctx->metadata->context_resolution_root_projection_count++;
			}
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			if (derivation->premises[j].semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
				uint32_t action = derivation->premises[j].semantic_action_id;
				if (action >= substitution_count || !substitution_live[action]) {
					return -1;
				}
				derivation->premises[j].semantic_action_id =
					substitution_relocation[action];
				if (derivation->premises[j].semantic_action_id != action) {
					candidate_actions_changed = 1;
					ctx->metadata->context_resolution_root_projection_count++;
				}
			}
		}
	}
	if (candidate_actions_changed &&
		prototype_judgement_delta_rebuild_candidate_index(
			&ctx->judgement_delta
		) != 0) {
		return -1;
	}
	/* Mutable branch constraints follow the rebased action. Accepted proof edges
	 * continue to denote their original immutable morphisms. */
	for (uint32_t offset = 0;
		offset < ctx->constraints.branch_refinement_constraint_count;
		++offset) {
		uint32_t id =
			ctx->constraints.first_branch_refinement_constraint + offset;
		struct operation_constraint* constraint =
			&ctx->constraints.constraints[id];
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
			constraint->payload.branch_refinement.refinement_status !=
				PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
			continue;
		}
		uint32_t old_action =
			constraint->payload.branch_refinement.substitution;
		if (old_action >= substitution_count || !substitution_live[old_action]) {
			return -1;
		}
		constraint->payload.branch_refinement.substitution =
			substitution_relocation[old_action];
		if (constraint->payload.branch_refinement.substitution != old_action) {
			ctx->metadata->context_resolution_root_projection_count++;
		}
		const struct prototype_substitution* action = prototype_substitution_get(
			&ctx->metadata->substitutions,
			constraint->payload.branch_refinement.substitution
		);
		if (!action) {
			return -1;
		}
		constraint->payload.branch_refinement.source_context =
			action->target_context;
		constraint->payload.branch_refinement.refined_context =
			action->source_context;
	}
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (operation->context_action_substitution == PROTOTYPE_INVALID_ID) {
			continue;
		}
		const struct prototype_substitution* action = prototype_substitution_get(
			&ctx->metadata->substitutions,
			operation->context_action_substitution
		);
		if (!action) {
			return -1;
		}
		if (action->source_context != operation->context_id) {
			/* A changed binder classifier invalidates this mutable projection. The
			 * original immutable action remains available to accepted evidence, while
			 * ordinary constraints rebuild the occurrence in its final Context. */
			operation->context_action_substitution = PROTOTYPE_INVALID_ID;
			operation->source_core_term = PROTOTYPE_INVALID_ID;
			operation->source_classifier = PROTOTYPE_INVALID_ID;
		}
	}
	for (size_t i = 0; i < ctx->judgement_delta.derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* proof =
			&ctx->judgement_delta.derivation_candidates[i];
		if (proof->conclusion_proposition_id >=
			ctx->judgement_delta.proposition_count) {
			return -1;
		}
		proof->conclusion = &ctx->judgement_delta.propositions[
			proof->conclusion_proposition_id
		];
		if (proof->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			((proof->proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ?
				proof->premise_count != 0 : proof->premise_count != 1) ||
			 proof->semantic_action_id >=
				ctx->metadata->substitutions.substitution_count)) {
			return -1;
		}
	}
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		uint32_t context_id =
			ctx->judgement_delta.computation_constraints[i].context_id;
		if (context_id >= source_count) {
			return -1;
		}
		if (relocation[context_id] != context_id) {
			ctx->judgement_delta.computation_constraints[i].context_id =
				relocation[context_id];
			ctx->metadata->context_resolution_root_projection_count++;
		}
	}
	ctx->context_resolution_dependency_revision =
		ctx->context_dependency_revision;
	ctx->context_resolution_context_count =
		ctx->metadata->contexts.context_count;
	ctx->context_dirty_binding_count = 0;
	ctx->context_dependencies_all_dirty = 0;
	ctx->context_resolution_valid = 1;
	ctx->context_resolution_finalized = finalize;
	return 0;
}

static int operation_solver_generate_computation_constraints_impl(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	/* Branch actions may reindex an operation after an earlier computation
	 * payload was recorded. JudgementDelta keeps stable record slots, but only
	 * the payload matching the current typed occurrence is an active mutable
	 * constraint. Retire stale slots before rebuilding ConstraintDB links. */
	for (size_t constraint_id = 0;
		constraint_id < ctx->judgement_delta.computation_constraint_count;
		++constraint_id) {
		struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[constraint_id];
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (constraint->occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[
				constraint->occurrence_id
			];
		if (operation->core_term != constraint->subject ||
			operation->context_id != constraint->context_id ||
			(operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
			 operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REQUEST)) {
			constraint->occurrence_id = PROTOTYPE_INVALID_ID;
			constraint->premise_occurrence_count = 0;
			constraint->return_body_occurrence_id = PROTOTYPE_INVALID_ID;
		}
	}
	if (prototype_judgement_delta_rebuild_computation_constraint_index(
			&ctx->judgement_delta
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
			operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REQUEST) {
			continue;
		}
		prototype_judgement_delta_set_occurrence(
			&ctx->judgement_delta, i
		);
		int status = prototype_judgement_delta_record_computation_constraint(
			&ctx->judgement_delta,
			ctx->terms,
			operation->context_id,
			operation->core_term
		);
		if (status != 0) {
			return -1;
		}
		uint32_t delta_constraint_id;
		if (prototype_judgement_delta_computation_constraint_for_occurrence(
				&ctx->judgement_delta, i, &delta_constraint_id
			) != 0) {
			return -1;
		}
		struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[delta_constraint_id];
		uint32_t lifecycle_id =
			ctx->constraints.computation_constraint_for_occurrence[i];
		if (lifecycle_id == PROTOTYPE_INVALID_ID) {
			if (ctx->constraints.constraint_count >=
				PROTOTYPE_COMPILER_CONSTRAINT_CAPACITY) {
				return -1;
			}
			uint32_t id = ctx->constraints.constraint_count++;
			ctx->constraints.computation_constraint_count++;
			lifecycle_id = id;
			ctx->constraints.constraints[id] = (struct operation_constraint){
				.id = id,
				.domain = OPERATION_CONSTRAINT_DOMAIN_COMPUTATION,
				.kind = constraint->kind,
				.source_occurrence = i,
				.source_ast = operation->source_ast,
				.context_id = operation->context_id,
				.state = OPERATION_CONSTRAINT_STATE_PENDING,
				.reason = OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY,
				.result_term = PROTOTYPE_INVALID_ID,
				.evidence_id = PROTOTYPE_INVALID_ID,
				.origin_constraint_id = PROTOTYPE_INVALID_ID,
				.payload.computation = {
					.judgement_delta_constraint_id = delta_constraint_id
				}
			};
			ctx->constraints.computation_constraint_for_occurrence[i] = id;
		} else {
			if (lifecycle_id >= ctx->constraints.constraint_count ||
				ctx->constraints.constraints[lifecycle_id].domain !=
					OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
				ctx->constraints.constraints[lifecycle_id].source_occurrence != i) {
				return -1;
			}
			struct operation_constraint* lifecycle =
				&ctx->constraints.constraints[lifecycle_id];
			lifecycle->kind = constraint->kind;
			lifecycle->source_ast = operation->source_ast;
			lifecycle->context_id = operation->context_id;
			lifecycle->payload.computation.judgement_delta_constraint_id =
				delta_constraint_id;
			if (operation_constraint_reopen(
					lifecycle,
					OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY
				) != 0) {
				return -1;
			}
		}
		if (lifecycle_id == PROTOTYPE_INVALID_ID ||
			operation_solver_add_constraint_dependency(
				ctx, i, lifecycle_id
			) != 0) {
			return -1;
		}
		constraint->premise_occurrence_count = 0;
		constraint->return_body_occurrence_id = PROTOTYPE_INVALID_ID;
		for (uint32_t premise = 0;
			premise < PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES;
			++premise) {
			constraint->premise_contexts[premise] = PROTOTYPE_INVALID_ID;
			constraint->premise_states[premise] =
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REQUEST) {
			uint32_t request_operation;
			uint32_t request_argument;
			uint32_t request_continuation;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
					&request_operation
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0,
					&request_argument
				) != 0 || compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
					&request_continuation
				) != 0) {
				return -1;
			}
			constraint->premise_occurrences[0] = request_operation;
			constraint->premise_occurrences[1] = request_argument;
			constraint->premise_occurrences[2] = request_continuation;
			constraint->premise_occurrence_count = 3;
			for (uint32_t premise = 0;
				premise < constraint->premise_occurrence_count;
				++premise) {
				if (operation_solver_add_constraint_dependency(
						ctx,
						constraint->premise_occurrences[premise],
						lifecycle_id
					) != 0) {
					return -1;
				}
			}
			continue;
		}
		uint32_t computation_occurrence;
		uint32_t return_operation;
		uint32_t premise_count = 2 + 2 * operation->fold_clause_count;
		if (compile_occurrence_child(
				ctx, i, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&computation_occurrence
			) != 0 || compile_occurrence_child(
				ctx, i, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&return_operation
			) != 0 || premise_count > 64 ||
			(operation->fold_clause_count != 0 &&
			 ((size_t)operation->first_fold_clause + operation->fold_clause_count >
				ctx->metadata->typed_occurrences.fold_clause_count))) {
			return -1;
		}
		constraint->premise_occurrences[0] = computation_occurrence;
		constraint->premise_occurrences[1] = return_operation;
		if (ctx->metadata->typed_occurrences.occurrences[return_operation].tag ==
				PROTOTYPE_TYPED_OCCURRENCE_LAMBDA && compile_occurrence_child(
				ctx, return_operation, PROTOTYPE_TERM_CHILD_BODY, 0,
				&constraint->return_body_occurrence_id
			) != 0) {
			return -1;
		}
		for (uint32_t clause = 0; clause < operation->fold_clause_count; ++clause) {
			uint32_t clause_operation;
			uint32_t clause_lambda;
			uint32_t clause_body;
			if (compile_fold_clause_occurrences(
					ctx, i, clause, &clause_operation, &clause_lambda,
					&clause_body
				) != 0) {
				return -1;
			}
			constraint->premise_occurrences[2 + 2 * clause] =
				clause_operation;
			constraint->premise_occurrences[3 + 2 * clause] =
				clause_lambda;
			(void)clause_body;
		}
		constraint->premise_occurrence_count = premise_count;
		for (uint32_t premise = 0; premise < premise_count; ++premise) {
			if (operation_solver_add_constraint_dependency(
					ctx,
					constraint->premise_occurrences[premise],
					lifecycle_id
				) != 0) {
				return -1;
			}
		}
	}
	ctx->constraints.first_effect_constraint = ctx->constraints.constraint_count;
	return operation_constraint_db_validate(ctx);
}

static int operation_solver_generate_computation_constraints(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	ctx->metadata->computation_constraint_generation_pass_count++;
	/* Effect equations are regenerated after computation solving. Remove the
	 * previous effect suffix before adding any newly discovered computation
	 * obligations; classifier and existing computation identities remain
	 * stable across fixed-point rounds. */
	ctx->constraints.constraint_count = ctx->constraints.first_effect_constraint;
	ctx->constraints.effect_constraint_count = 0;
	struct operation_constraint_db_mark mark =
		operation_constraint_db_mark(&ctx->constraints);
	int status = operation_solver_generate_computation_constraints_impl(ctx);
	if (status != 0 && operation_constraint_db_rollback(
			&ctx->constraints, mark
		) != 0) {
		return -1;
	}
	return status;
}

static int operation_solver_bind_computation_constraint_solutions(
	struct compile_context* ctx,
	int* p_changed,
	int publish_occurrences
) {
	if (!ctx || !p_changed) {
		return -1;
	}
	/* Requests refine the computations consumed by enclosing folds. Apply their
	 * solutions first so the fold snapshot check below observes those
	 * refinements independently of source/storage order. */
	for (int pass = 0; pass < 2; ++pass) {
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		const struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[i];
		if (i >= ctx->computation_constraint_result_capacity) {
			fprintf(stderr, "computation binding result capacity id=%zu capacity=%zu\n",
				i, ctx->computation_constraint_result_capacity);
			return -1;
		}
		const struct prototype_judgement_computation_constraint_result* result =
			&ctx->computation_constraint_results[i];
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t lifecycle_id = constraint->occurrence_id < 4096 ?
			ctx->constraints.computation_constraint_for_occurrence[
				constraint->occurrence_id
			] : PROTOTYPE_INVALID_ID;
		if (lifecycle_id == PROTOTYPE_INVALID_ID ||
			lifecycle_id >= ctx->constraints.constraint_count) {
			fprintf(stderr,
				"computation binding lifecycle missing delta=%zu total-delta=%zu first=%u count=%u total=%u\n",
				i, ctx->judgement_delta.computation_constraint_count,
				ctx->constraints.first_computation_constraint,
				ctx->constraints.computation_constraint_count,
				ctx->constraints.constraint_count);
			return -1;
		}
		struct operation_constraint* lifecycle =
			&ctx->constraints.constraints[lifecycle_id];
		if (lifecycle->domain != OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
			lifecycle->payload.computation.judgement_delta_constraint_id != i) {
			return -1;
		}
		lifecycle->result_term = result->solved_classifier;
		int state = result->solved_classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CONSTRAINT_STATE_PENDING :
			OPERATION_CONSTRAINT_STATE_SOLVED;
		int reason = result->solved_classifier == PROTOTYPE_INVALID_ID ?
			OPERATION_CLASSIFIER_GOAL_REASON_WAITING_DEPENDENCY :
			OPERATION_CLASSIFIER_GOAL_REASON_CBPV_VALIDATED;
		if (operation_constraint_transition(lifecycle, state, reason) != 0) {
			fprintf(stderr,
				"computation binding lifecycle transition delta=%zu current=%d target=%d\n",
				i, lifecycle->state, state);
			return -1;
		}
		if (!publish_occurrences) {
			continue;
		}
		int is_request = constraint->kind ==
			PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST;
		if ((pass == 0) != is_request) {
			continue;
		}
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID ||
			lifecycle->result_term == PROTOTYPE_INVALID_ID) {
			continue;
		}
		/* Solutions are derived from the premise-classifier snapshot populated
		 * before this binding pass. Earlier solutions in this same pass can
		 * refine an operation used by a later constraint. Do not publish the
		 * dependent stale solution; the next fixed-point round will refresh and
		 * solve that constraint from the refined premise. */
		int stale = 0;
		for (uint32_t premise = 0;
			premise < constraint->premise_occurrence_count;
			++premise) {
			uint32_t operation_id = constraint->premise_occurrences[premise];
			if (operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
				fprintf(stderr,
					"computation binding premise range delta=%zu premise=%u occurrence=%u count=%zu\n",
					i, premise, operation_id,
					ctx->metadata->typed_occurrences.occurrence_count);
				return -1;
			}
			uint32_t current = operation_solver_classifier(ctx, operation_id);
			uint32_t snapshot = constraint->premise_classifiers[premise];
			if (current == snapshot) {
				continue;
			}
			if (current == PROTOTYPE_INVALID_ID ||
				snapshot == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, current, snapshot
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				stale = 1;
				break;
			}
		}
		if (stale) {
			continue;
		}
		if (constraint->occurrence_id >= ctx->metadata->typed_occurrences.occurrence_count ||
			lifecycle->result_term >= ctx->terms->term_count ||
			operation_solver_bind_proven_classifier(
				ctx,
				constraint->occurrence_id,
				lifecycle->result_term,
				p_changed
			) != 0) {
			fprintf(stderr,
				"computation binding publication failed delta=%zu occurrence=%u result=%u occurrence-count=%zu term-count=%zu\n",
				i, constraint->occurrence_id, lifecycle->result_term,
				ctx->metadata->typed_occurrences.occurrence_count,
				ctx->terms->term_count);
			return -1;
		}
	}
	}
	return 0;
}

static int operation_solver_refresh_computation_constraint_operands(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (size_t i = 0;
		i < ctx->judgement_delta.computation_constraint_count;
		++i) {
		struct prototype_judgement_computation_constraint* constraint =
			&ctx->judgement_delta.computation_constraints[i];
		if (constraint->occurrence_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (constraint->occurrence_id != PROTOTYPE_INVALID_ID) {
			if (constraint->occurrence_id >= ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			if (i >= ctx->computation_constraint_result_capacity) {
				return -1;
			}
			ctx->computation_constraint_results[i].projected_classifier =
				operation_solver_classifier(
				ctx, constraint->occurrence_id
			);
		}
		if (constraint->premise_occurrence_count > 64) {
			return -1;
		}
		for (uint32_t premise = 0;
			premise < constraint->premise_occurrence_count;
			++premise) {
			uint32_t operation_id = constraint->premise_occurrences[premise];
			if (operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			uint32_t classifier = operation_solver_classifier(ctx, operation_id);
			constraint->premise_classifiers[premise] = classifier;
			memset(
				&constraint->premise_evidence[premise],
				0,
				sizeof(constraint->premise_evidence[premise])
			);
			constraint->premise_contexts[premise] =
				ctx->metadata->typed_occurrences.occurrences[operation_id].context_id;
			if (classifier == PROTOTYPE_INVALID_ID) {
				constraint->premise_states[premise] =
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
				continue;
			}
			int local = operation_classifier_contains_unowned_effect_row(
				ctx, operation_id, classifier
			);
			if (local < 0) {
				return -1;
			}
			if (local) {
				constraint->premise_states[premise] =
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_LOCAL;
				continue;
			}
			int evidence_status = operation_solver_select_operation_evidence_in_context(
				ctx,
				operation_id,
				constraint->context_id,
				ctx->metadata->typed_occurrences.occurrences[operation_id].core_term,
				classifier,
				&constraint->premise_evidence[premise]
			);
			if (evidence_status < 0) {
				return -1;
			}
			constraint->premise_states[premise] = evidence_status == 0 ?
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED :
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED;
		}
	}
	return 0;
}

static int bind_cbpv_operation_classifiers(
	struct compile_context* ctx,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	/* RETURN/THUNK/FORCE are one kernel rule parameterized by the child
	 * classifier of this source occurrence.  A shared erased boundary node may
	 * legitimately have several such derivations, so do not recover one child
	 * classifier globally from TermDB. */
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t child_occurrence;
		int child_role = operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ?
			PROTOTYPE_TERM_CHILD_RETURN_VALUE :
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ?
				PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION :
				PROTOTYPE_TERM_CHILD_FORCE_VALUE);
		if ((operation->tag != PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
			 operation->tag != PROTOTYPE_TYPED_OCCURRENCE_THUNK &&
			 operation->tag != PROTOTYPE_TYPED_OCCURRENCE_FORCE) ||
			compile_occurrence_child(
				ctx, operation_id, child_role, 0, &child_occurrence
			) != 0) {
			continue;
		}
		uint32_t child_classifier = operation_solver_classifier(
			ctx, child_occurrence
		);
		if (child_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t classifier;
		int boundary_status = prototype_judgement_cbpv_boundary_classifier(
				ctx->terms,
				ctx->type_declarations,
				operation->core_term,
				child_classifier,
				&classifier
			);
		if (boundary_status != 0) {
			continue;
		}
		if (operation_solver_bind_proven_classifier(
				ctx, operation_id, classifier, p_changed
			) != 0) {
			return -1;
		}
	}
	return 0;
}
