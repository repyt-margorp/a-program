static int operation_solver_reindex_in_enclosing_indexed_branches(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t term,
	uint32_t* p_reindexed
);

static int operation_constraint_db_assert_topology(
	struct prototype_typing_pipeline* typing,
	struct prototype_compile_metadata* metadata
) {
	if (!typing || !metadata || !typing->constraints.topology_sealed) {
		return -1;
	}
	metadata->constraint_topology_validation_count++;
	uint64_t expected = 0;
	uint64_t actual = 0;
	int status = prototype_typing_pipeline_validate_constraint_topology(
		typing, &expected, &actual
	);
	if (status != 0) {
		metadata->constraint_topology_validation_failure_count++;
		fprintf(
			stderr,
			"sealed Layer T constraint topology changed expected=%" PRIu64
			" actual=%" PRIu64 " constraints=%u/%u dependencies=%u/%u\n",
			expected,
			actual,
			typing->constraints.sealed_constraint_count,
			typing->constraints.constraint_count,
			typing->constraints.sealed_dependent_constraint_count,
			typing->constraints.dependent_constraint_count
		);
		return -1;
	}
	return 0;
}

static int operation_constraint_db_seal_topology(
	struct prototype_typing_pipeline* typing,
	struct prototype_compile_metadata* metadata
) {
	if (!typing || !metadata || typing->constraints.topology_sealed) {
		return -1;
	}
	uint64_t digest = 0;
	if (prototype_typing_pipeline_seal_constraint_topology(
		typing, &digest
	) != 0) {
		return -1;
	}
	metadata->constraint_topology_digest = digest;
	metadata->constraint_topology_sealed = 1;
	return 0;
}

static int operation_motive_mark_equations_ready(
	struct compile_context* ctx,
	uint32_t operation
);

static int operation_motive_mark_waiting(
	struct compile_context* ctx,
	uint32_t operation
);

static int operation_motive_mark_guarded_recursive(
	struct compile_context* ctx,
	uint32_t operation
);

static int operation_solver_solve_branch_refinement_constraints(
	struct compile_context* ctx,
	int* p_projection_waiting
);

static int operation_solver_occurrence_waits_for_branch_action(
	struct compile_context* ctx,
	uint32_t occurrence_id
);

static struct prototype_typing_constraint* operation_solver_branch_refinement_constraint(
	struct compile_context* ctx,
	uint32_t operation_case_id
);

static int operation_subtree_contains_operation(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint8_t* visited
);

static int operation_classifier_rhs_graph_validate(
	const struct compile_context* ctx
) {
	if (!ctx || !ctx->typing || !ctx->metadata || !ctx->terms) {
		return -1;
	}
	const struct prototype_typing_constraint_db* db =
		&ctx->typing->constraints;
	if (db->classifier_rhs_count > PROTOTYPE_TYPING_CLASSIFIER_RHS_CAPACITY ||
		db->operand_count > PROTOTYPE_TYPING_EQUATION_OPERAND_CAPACITY) {
		return -1;
	}
	for (uint32_t rhs_id = 0; rhs_id < db->classifier_rhs_count; ++rhs_id) {
		const struct prototype_typing_classifier_rhs* rhs =
			&db->classifier_rhs[rhs_id];
		if (rhs->kind < PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL ||
			rhs->kind > PROTOTYPE_TYPING_CLASSIFIER_RHS_TERMINATES ||
			rhs->source_term >= ctx->terms->term_count ||
			(rhs->operand_count == 0 &&
			 rhs->first_operand != PROTOTYPE_INVALID_ID) ||
			(rhs->operand_count != 0 &&
			 (rhs->first_operand == PROTOTYPE_INVALID_ID ||
			  rhs->first_operand > db->operand_count ||
			  rhs->operand_count > db->operand_count - rhs->first_operand))) {
			fprintf(stderr, "classifier RHS header invalid rhs=%u\n", rhs_id);
			return -1;
		}
		uint32_t expected_count;
		switch (rhs->kind) {
		case PROTOTYPE_TYPING_CLASSIFIER_RHS_LITERAL:
			expected_count = 0;
			break;
		case PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT:
		case PROTOTYPE_TYPING_CLASSIFIER_RHS_THUNK:
		case PROTOTYPE_TYPING_CLASSIFIER_RHS_TERMINATES:
			expected_count = 1;
			break;
		case PROTOTYPE_TYPING_CLASSIFIER_RHS_VALUE_APP:
		case PROTOTYPE_TYPING_CLASSIFIER_RHS_TYPE_APP:
			expected_count = 2;
			break;
		case PROTOTYPE_TYPING_CLASSIFIER_RHS_ACCEPTED_SUBSTITUTION:
			expected_count = rhs->operand_count;
			break;
		default:
			return -1;
		}
		if (rhs->operand_count != expected_count) {
			fprintf(stderr, "classifier RHS arity invalid rhs=%u\n", rhs_id);
			return -1;
		}
		for (uint32_t operand_id = 0;
			operand_id < rhs->operand_count;
			++operand_id) {
			const struct prototype_typing_equation_operand* operand =
				prototype_typing_classifier_rhs_operand_get(
					db, rhs, operand_id
				);
			if (!operand) {
				return -1;
			}
			if (rhs->kind == PROTOTYPE_TYPING_CLASSIFIER_RHS_ENDPOINT) {
				if (operand->role !=
						PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_STATIC_ENDPOINT ||
					operand->target_kind !=
						PROTOTYPE_TYPING_EQUATION_OPERAND_SOURCE_OCCURRENCE ||
					operand->target >=
						ctx->metadata->typed_occurrences.occurrence_count) {
					return -1;
				}
				continue;
			}
			int substitution = rhs->kind ==
				PROTOTYPE_TYPING_CLASSIFIER_RHS_ACCEPTED_SUBSTITUTION;
			if (operand->target_kind !=
					PROTOTYPE_TYPING_EQUATION_OPERAND_CLASSIFIER_RHS ||
				operand->target >= rhs_id ||
				operand->role != (substitution ?
					PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_SUBSTITUTION_TARGET :
					PROTOTYPE_TYPING_EQUATION_OPERAND_ROLE_EXPRESSION_CHILD) ||
				(!substitution && operand->qualifier != operand_id)) {
				fprintf(stderr,
					"classifier RHS edge invalid rhs=%u operand=%u\n",
					rhs_id, operand_id);
				return -1;
			}
		}
	}
	return 0;
}

static int operation_constraint_db_validate(
	const struct compile_context* ctx
) {
	struct prototype_core_inspect_counts core_counts;
	if (!ctx || !ctx->metadata ||
		ctx->typing->constraints.constraint_count >
			PROTOTYPE_TYPING_CONSTRAINT_CAPACITY) {
		fprintf(
			stderr,
			"constraint db size invalid total=%u capacity=%u\n",
			ctx ? ctx->typing->constraints.constraint_count : 0,
			PROTOTYPE_TYPING_CONSTRAINT_CAPACITY
		);
		return -1;
	}
	uint32_t indexed_constraint_count = 0;
	for (int domain = OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER;
		domain < PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT;
		++domain) {
		uint32_t traversed = 0;
		for (uint32_t id = prototype_typing_constraint_first_in_domain(
				&ctx->typing->constraints, domain
			);
			id != PROTOTYPE_INVALID_ID;
			id = prototype_typing_constraint_next_in_domain(
				&ctx->typing->constraints, id
			)) {
			if (!prototype_typing_constraint_has_domain(
					&ctx->typing->constraints, id, domain
				) || ++traversed > ctx->typing->constraints.constraint_count) {
				return -1;
			}
		}
		if (traversed != prototype_typing_constraint_count_in_domain(
				&ctx->typing->constraints, domain
			)) {
			return -1;
		}
		indexed_constraint_count += traversed;
	}
	if (indexed_constraint_count != ctx->typing->constraints.constraint_count) {
		return -1;
	}
	if (typing_core_inspect_counts(
			ctx->core_requests, &core_counts, NULL
		) != 0 || operation_classifier_rhs_graph_validate(ctx) != 0) {
		return -1;
	}
	ctx->metadata->full_scan_count[PROTOTYPE_COMPILE_SCAN_CONSTRAINT]++;
	ctx->metadata->full_scan_visit_count[PROTOTYPE_COMPILE_SCAN_CONSTRAINT] +=
		ctx->typing->constraints.constraint_count;
	for (uint32_t id = 0; id < ctx->typing->constraints.constraint_count; ++id) {
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[id];
		const struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		if (constraint->id != id ||
			solution->state < OPERATION_CONSTRAINT_STATE_PENDING ||
			solution->state > OPERATION_CONSTRAINT_STATE_CONTRADICTION ||
			constraint->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			fprintf(stderr, "constraint db header invalid id=%u domain=%d state=%d occurrence=%u\n",
				id, constraint->domain, solution->state,
				constraint->source_occurrence);
			return -1;
		}
		if (constraint->classifier_rhs_root != PROTOTYPE_INVALID_ID &&
			(constraint->classifier_rhs_root >=
				ctx->typing->constraints.classifier_rhs_count ||
			 constraint->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER ||
			 constraint->kind != OPERATION_CONSTRAINT_PI_EXPECTED ||
			 constraint->payload.classifier.goal.pi.role !=
				OPERATION_CLASSIFIER_PI_GOAL_INTRO)) {
			fprintf(stderr, "constraint classifier RHS root invalid id=%u root=%u\n",
				id, constraint->classifier_rhs_root);
			return -1;
		}
		if (constraint->origin_constraint_id != PROTOTYPE_INVALID_ID &&
			(constraint->origin_constraint_id >= id ||
			 ctx->typing->constraints.constraints[
				constraint->origin_constraint_id
			 ].domain != OPERATION_CONSTRAINT_DOMAIN_COMPUTATION ||
			 ctx->typing->constraints.constraints[
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
		if (occurrence->context_id >= ctx->metadata->contexts.context_count ||
			(ctx->asts && constraint->source_ast != PROTOTYPE_INVALID_ID &&
			 constraint->source_ast >= ctx->asts->node_count)) {
			fprintf(stderr, "constraint db origin invalid id=%u occurrence-context=%u ast=%u\n",
				id, occurrence->context_id,
				constraint->source_ast);
			return -1;
		}
		if (constraint->domain == OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER) {
			if (!prototype_typing_constraint_has_domain(
					&ctx->typing->constraints,
					ctx->typing->constraints.classifier_constraint_for_projection[
						constraint->owner_typed_projection
					],
					OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
				)) {
				fprintf(stderr, "constraint db classifier payload invalid id=%u\n", id);
				return -1;
			}
			if (constraint->kind == OPERATION_CONSTRAINT_IH_EXPECTED &&
				ctx->typing->constraints.ih_constraint_for_projection[
					constraint->owner_typed_projection
				] != id) {
				return -1;
			}
			if (constraint->kind == OPERATION_CONSTRAINT_MOTIVE_EQUATION) {
				uint32_t case_index =
					constraint->payload.classifier.goal.motive_case.case_index;
				uint32_t match_case_projection = constraint->payload.classifier.
					goal.motive_case.match_case_projection;
				if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
					case_index >= occurrence->case_count ||
					occurrence->first_case + case_index >=
						PROTOTYPE_TYPING_CONSTRAINT_CAPACITY ||
					match_case_projection >= ctx->typing->context_projections.
						match_case_projection_count ||
					ctx->typing->constraints.
						motive_constraint_for_match_case_projection[
							match_case_projection
					] != id) {
					return -1;
				}
			}
		} else if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT) {
			const struct prototype_typing_constraint_solution* solution =
				&ctx->typing->constraints.solutions[id];
			const struct prototype_substitution* branch_action =
				solution->branch_refinement.substitution ==
					PROTOTYPE_INVALID_ID ? NULL : prototype_substitution_get(
					&ctx->metadata->substitutions,
					solution->branch_refinement.substitution
				);
			uint32_t match_id =
				constraint->payload.branch_refinement.match_occurrence;
			uint32_t case_index =
				constraint->payload.branch_refinement.case_index;
			uint32_t match_projection_id =
				constraint->payload.branch_refinement.match_typed_projection;
			uint32_t scrutinee_projection_id =
				constraint->payload.branch_refinement.scrutinee_typed_projection;
			uint32_t branch_projection_id =
				constraint->payload.branch_refinement.branch_typed_projection;
			uint32_t case_projection_id =
				constraint->payload.branch_refinement.match_case_projection;
			const struct prototype_typed_occurrence* match = match_id <
				ctx->metadata->typed_occurrences.occurrence_count ?
					&ctx->metadata->typed_occurrences.occurrences[match_id] : NULL;
			const struct prototype_typed_projection* match_projection =
				prototype_typed_projection_get(
					&ctx->typing->context_projections, match_projection_id
				);
			const struct prototype_typed_projection* scrutinee_projection =
				prototype_typed_projection_get(
					&ctx->typing->context_projections, scrutinee_projection_id
				);
			const struct prototype_typed_projection* branch_projection =
				prototype_typed_projection_get(
					&ctx->typing->context_projections, branch_projection_id
				);
			const struct prototype_typed_match_case_projection* case_projection =
				prototype_typed_match_case_projection_get(
					&ctx->typing->context_projections, case_projection_id
				);
			int branch_payload_valid = constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT;
			if (branch_payload_valid) {
				branch_payload_valid = match && match_projection &&
					scrutinee_projection && branch_projection && case_projection;
			}
			if (branch_payload_valid) {
				branch_payload_valid = constraint->payload.branch_refinement.
					scrutinee_occurrence <
					ctx->metadata->typed_occurrences.occurrence_count;
			}
			if (branch_payload_valid) {
				branch_payload_valid = match->tag ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH;
			}
			if (branch_payload_valid) {
				branch_payload_valid = case_index < match->case_count;
			}
			if (branch_payload_valid) {
				branch_payload_valid = match->first_case + case_index <
					ctx->metadata->typed_occurrences.case_count;
			}
			if (branch_payload_valid) {
				branch_payload_valid = case_projection->source_case ==
					match->first_case + case_index;
			}
			if (branch_payload_valid) {
				branch_payload_valid = case_projection->owner_typed_projection ==
					match_projection_id;
			}
			if (branch_payload_valid) {
				branch_payload_valid = match_projection->source_occurrence ==
					match_id;
			}
			if (branch_payload_valid) {
				branch_payload_valid = scrutinee_projection->source_occurrence ==
					constraint->payload.branch_refinement.scrutinee_occurrence;
			}
			if (branch_payload_valid) {
				branch_payload_valid = branch_projection->source_occurrence ==
					constraint->source_occurrence;
			}
			if (branch_payload_valid) {
				branch_payload_valid = constraint->owner_typed_projection ==
					branch_projection_id;
			}
			if (branch_payload_valid) {
				branch_payload_valid = ctx->typing->constraints.
					branch_refinement_constraint_for_match_case_projection[
						case_projection_id
					] == id;
			}
			if (branch_payload_valid) {
				branch_payload_valid = constraint->payload.branch_refinement.
					source_context == occurrence->context_id;
			}
			if (branch_payload_valid &&
				solution->branch_refinement.refinement_status ==
					PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
				branch_payload_valid = branch_action != NULL;
				if (branch_payload_valid) {
					branch_payload_valid = branch_action->source_context ==
						solution->branch_refinement.refined_context;
				}
				if (branch_payload_valid) {
					branch_payload_valid = branch_action->target_context ==
						solution->branch_refinement.materialized_source_context;
				}
			} else if (branch_payload_valid) {
				branch_payload_valid = solution->branch_refinement.substitution ==
					PROTOTYPE_INVALID_ID;
			}
			if (!branch_payload_valid) {
				fprintf(stderr,
					"constraint db branch payload invalid id=%u match=%u case=%u "
					"status=%d constraint-context=%u payload-context=%u "
					"source=%u refined=%u substitution=%u action-source=%u "
					"action-target=%u projections=%u/%u/%u case-projection=%u "
					"projection-sources=%u/%u/%u\n",
					id,
					constraint->payload.branch_refinement.match_occurrence,
					constraint->payload.branch_refinement.case_index,
					solution->branch_refinement.refinement_status,
					occurrence->context_id,
					constraint->payload.branch_refinement.source_context,
					solution->branch_refinement.materialized_source_context,
					solution->branch_refinement.refined_context,
					solution->branch_refinement.substitution,
					branch_action ? branch_action->source_context : PROTOTYPE_INVALID_ID,
					branch_action ? branch_action->target_context : PROTOTYPE_INVALID_ID,
					match_projection_id, scrutinee_projection_id,
					branch_projection_id, case_projection_id,
					match_projection ? match_projection->source_occurrence :
						PROTOTYPE_INVALID_ID,
					scrutinee_projection ? scrutinee_projection->source_occurrence :
						PROTOTYPE_INVALID_ID,
					branch_projection ? branch_projection->source_occurrence :
						PROTOTYPE_INVALID_ID);
				return -1;
			}
		} else if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_COMPUTATION) {
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
		} else if (constraint->domain ==
				OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW &&
			(constraint->payload.effect.result_row >= core_counts.term_count ||
			constraint->payload.effect.left_row >= core_counts.term_count ||
			(constraint->payload.effect.right_row != PROTOTYPE_INVALID_ID &&
			 constraint->payload.effect.right_row >= core_counts.term_count))) {
			fprintf(stderr, "constraint db effect payload invalid id=%u\n", id);
			return -1;
		} else if (constraint->domain < OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER ||
			constraint->domain >= PROTOTYPE_TYPING_CONSTRAINT_DOMAIN_COUNT) {
			return -1;
		}
		if (constraint->domain == OPERATION_CONSTRAINT_DOMAIN_EFFECT_ROW &&
			constraint->kind == PROTOTYPE_TYPING_EFFECT_CONSTRAINT_SOLUTION &&
			(constraint->owner_typed_projection >=
					PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY ||
			 ctx->typing->constraints.
				effect_solution_constraint_for_projection[
					constraint->owner_typed_projection
				] != id ||
			 ctx->terms->terms[constraint->payload.effect.result_row].tag !=
				PROTOTYPE_TERM_EFFECT_ROW_VAR ||
			 (solution->result_term != PROTOTYPE_INVALID_ID &&
				solution->result_term >= core_counts.term_count))) {
			fprintf(stderr,
				"constraint db effect solution invalid id=%u projection=%u row=%u\n",
				id,
				constraint->owner_typed_projection,
				solution->result_term
			);
			return -1;
		}
	}
	for (uint32_t occurrence = 0;
		occurrence < ctx->metadata->typed_occurrences.occurrence_count;
		++occurrence) {
		uint32_t computation_id =
			ctx->typing->constraints.computation_constraint_for_occurrence[occurrence];
		if (computation_id != PROTOTYPE_INVALID_ID &&
			(!prototype_typing_constraint_has_domain(
				&ctx->typing->constraints, computation_id,
				OPERATION_CONSTRAINT_DOMAIN_COMPUTATION
			 ) ||
			 ctx->typing->constraints.constraints[computation_id].source_occurrence !=
				occurrence)) {
			fprintf(stderr,
				"constraint db computation index invalid occurrence=%u id=%u\n",
				occurrence, computation_id);
			return -1;
		}
	}
	for (uint32_t typed_projection = 0;
		typed_projection <
			ctx->typing->context_projections.typed_projection_count;
		++typed_projection) {
		uint32_t traversed = 0;
		for (uint32_t dependency = ctx->typing->constraints.
				first_dependent_constraint[typed_projection];
			dependency != PROTOTYPE_INVALID_ID;
			dependency = ctx->typing->constraints.
				dependent_constraints[dependency].next) {
			if (dependency >= ctx->typing->constraints.dependent_constraint_count ||
				ctx->typing->constraints.dependent_constraints[
					dependency
				].source_kind !=
					PROTOTYPE_TYPING_DEPENDENCY_SOURCE_PROJECTION ||
				ctx->typing->constraints.dependent_constraints[
					dependency
				].source != typed_projection ||
				ctx->typing->constraints.dependent_constraints[
					dependency
				].constraint >= ctx->typing->constraints.constraint_count ||
				++traversed > ctx->typing->constraints.dependent_constraint_count) {
				fprintf(
					stderr,
					"constraint db dependency invalid projection=%u dependency=%u\n",
					typed_projection,
					dependency
				);
				return -1;
			}
		}
	}
	for (uint32_t equation = 0;
		equation < ctx->typing->constraints.constraint_count;
		++equation) {
		uint32_t traversed = 0;
		for (uint32_t dependency = ctx->typing->constraints.
				first_equation_dependent_constraint[equation];
			dependency != PROTOTYPE_INVALID_ID;
			dependency = ctx->typing->constraints.
				dependent_constraints[dependency].next) {
			if (dependency >= ctx->typing->constraints.dependent_constraint_count ||
				ctx->typing->constraints.dependent_constraints[
					dependency
				].source_kind !=
					PROTOTYPE_TYPING_DEPENDENCY_SOURCE_EQUATION ||
				ctx->typing->constraints.dependent_constraints[
					dependency
				].source != equation ||
				ctx->typing->constraints.dependent_constraints[
					dependency
				].constraint >= ctx->typing->constraints.constraint_count ||
				++traversed > ctx->typing->constraints.dependent_constraint_count) {
				fprintf(
					stderr,
					"constraint db equation dependency invalid equation=%u dependency=%u\n",
					equation,
					dependency
				);
				return -1;
			}
		}
	}
	return 0;
}

static int operation_solver_var_binding_context(
	const struct prototype_typed_occurrence_graph* occurrences,
	const struct prototype_context_db* contexts,
	uint32_t variable_occurrence,
	uint32_t* p_binding_context
) {
	if (!occurrences || !contexts || !p_binding_context ||
		variable_occurrence >= occurrences->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* variable =
		&occurrences->occurrences[variable_occurrence];
	if (variable->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
		variable->binding_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (prototype_context_find_binding(
			contexts,
			variable->context_id,
			variable->binding_id,
			p_binding_context
		) != 0) {
		return 1;
	}
	return 0;
}

static int operation_solver_projection_requires_live_equation(
	const struct compile_context* ctx,
	uint32_t typed_projection
) {
	if (!ctx || !ctx->typing || typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	if (!projection || !projection->topology_expanded) {
		return -1;
	}
	uint32_t existing = ctx->typing->constraints.
		classifier_constraint_for_projection[typed_projection];
	if (prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, existing,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		)) {
		return 1;
	}
	if (projection->source_occurrence >= ctx->accepted_typing.occurrence_count) {
		return 1;
	}
	const struct prototype_typed_publication_projection* accepted = NULL;
	int accepted_status = prototype_accepted_typing_input_lookup(
		&ctx->accepted_typing,
		projection->source_occurrence,
		&accepted,
		NULL
	);
	if (accepted_status < 0) {
		return -1;
	}
	if (accepted_status > 0) {
		/* A generated root can make a formerly unreachable source occurrence live. */
		return 1;
	}
	if (!accepted) {
		return -1;
	}
	/* TypedProjection IDs are local to a compilation image. Compare with the
	 * publication projection selected by the current topology, never with the
	 * numeric ID stored by a preceding image. */
	if (ctx->typing->publication_typed_projection_for_occurrence[
			projection->source_occurrence
		] != typed_projection) {
		return 1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		/* IH equations and their owner motive are one recursive SCC. Publication
		 * stores only the aggregate classifier, so replaying either side requires
		 * the explicit IH equation in the current image. */
		return operation->ih_owner_occurrence <
			ctx->metadata->typed_occurrences.occurrence_count ? 1 : -1;
	}
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		return 0;
	}
	/* An induction hypothesis needs its owner Match's motive equations, which are
	 * not part of the publication classifier seed. Generate that exact Match
	 * equation without reopening unrelated accepted Matches. The IH itself may
	 * belong to the accepted prefix while a generated graph reuses it. */
	for (uint32_t occurrence = 0;
		occurrence < ctx->metadata->typed_occurrences.occurrence_count;
		++occurrence) {
		const struct prototype_typed_occurrence* candidate =
			&ctx->metadata->typed_occurrences.occurrences[occurrence];
		if (candidate->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
			candidate->ih_owner_occurrence == projection->source_occurrence) {
			return 1;
		}
	}
	return 0;
}

static int operation_solver_case_projection_requires_live_equation(
	const struct compile_context* ctx,
	uint32_t match_case_projection
) {
	if (!ctx || !ctx->typing || match_case_projection >=
			ctx->typing->context_projections.match_case_projection_count) {
		return -1;
	}
	const struct prototype_typed_match_case_projection* projection =
		prototype_typed_match_case_projection_get(
			&ctx->typing->context_projections, match_case_projection
		);
	if (!projection) {
		return -1;
	}
	/* A case projection denotes a branch pullback, so its refinement equation is
	 * part of that projection's canonical topology even when the owner Match's
	 * aggregate classifier is supplied by accepted seed. Accepted cases solve the
	 * equation from their immutable publication instead of reopening the Match. */
	return projection->owner_typed_projection <
		ctx->typing->context_projections.typed_projection_count ? 1 : -1;
}

static int operation_solver_add_branch_refinement_constraints(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata || ctx->typing->constraints.topology_sealed) {
		return -1;
	}
	for (uint32_t case_projection_id = 0;
		case_projection_id <
			ctx->typing->context_projections.match_case_projection_count;
		++case_projection_id) {
		const struct prototype_typed_match_case_projection* case_projection =
			prototype_typed_match_case_projection_get(
				&ctx->typing->context_projections, case_projection_id
			);
		const struct prototype_typed_projection* match_projection =
			case_projection ? prototype_typed_projection_get(
				&ctx->typing->context_projections,
				case_projection->owner_typed_projection
			) : NULL;
		int requires_equation =
			operation_solver_case_projection_requires_live_equation(
				ctx, case_projection_id
			);
		if (requires_equation < 0) {
			return -1;
		}
		if (!case_projection || !match_projection || !requires_equation) {
			continue;
		}
		uint32_t match_id = match_projection->source_occurrence;
		if (match_id >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* match =
			&ctx->metadata->typed_occurrences.occurrences[match_id];
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			case_projection->source_case < match->first_case ||
			case_projection->source_case - match->first_case >= match->case_count) {
			return -1;
		}
		uint32_t case_index = case_projection->source_case - match->first_case;
		uint32_t scrutinee_typed_projection;
		uint32_t branch_typed_projection;
		if (operation_solver_projected_child(
				ctx,
				case_projection->owner_typed_projection,
				PROTOTYPE_TERM_CHILD_SCRUTINEE,
				0,
				&scrutinee_typed_projection
			) != 0 || operation_solver_projected_child(
				ctx,
				case_projection->owner_typed_projection,
				PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
				case_index,
				&branch_typed_projection
			) != 0) {
			return -1;
		}
		const struct prototype_typed_projection* scrutinee_projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, scrutinee_typed_projection
			);
		const struct prototype_typed_projection* branch_projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, branch_typed_projection
			);
		if (!scrutinee_projection || !branch_projection ||
			scrutinee_projection->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count ||
			branch_projection->source_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		uint32_t source_context = ctx->metadata->typed_occurrences.occurrences[
			branch_projection->source_occurrence
		].context_id;
		struct prototype_typing_constraint constraint = {
			.domain = OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT,
			.kind = OPERATION_BRANCH_REFINEMENT_CONSTRAINT_MATCH_CASE,
			.origin_constraint_id = PROTOTYPE_INVALID_ID,
			.classifier_rhs_root = PROTOTYPE_INVALID_ID,
			.payload.branch_refinement = {
				.match_occurrence = match_id,
				.scrutinee_occurrence =
					scrutinee_projection->source_occurrence,
				.case_index = case_index,
				.source_context = source_context,
				.match_typed_projection =
					case_projection->owner_typed_projection,
				.scrutinee_typed_projection = scrutinee_typed_projection,
				.branch_typed_projection = branch_typed_projection,
				.match_case_projection = case_projection_id
			}
		};
		uint32_t id;
		if (operation_solver_intern_projected_constraint(
				ctx, branch_typed_projection, &constraint, NULL, 0, &id
			) != 0) {
			return -1;
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
	/* Populate the semantic case key before building edges. An enclosing case
	 * can appear later in storage order; equation identity must not depend on
	 * that order. */
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
		if (constraint->domain !=
				OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT) {
			return -1;
		}
		uint32_t case_projection =
			constraint->payload.branch_refinement.match_case_projection;
		if (case_projection >=
				ctx->typing->context_projections.match_case_projection_count ||
			ctx->typing->constraints.
				branch_refinement_constraint_for_match_case_projection[
					case_projection
				] != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		ctx->typing->constraints.
			branch_refinement_constraint_for_match_case_projection[
				case_projection
			] = id;
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
		uint32_t dependencies[] = {
			constraint->payload.branch_refinement.scrutinee_typed_projection,
			constraint->payload.branch_refinement.match_typed_projection
		};
		for (size_t dependency = 0;
			dependency < sizeof(dependencies) / sizeof(dependencies[0]);
			++dependency) {
			if (prototype_typing_constraint_add_dependency(
					&ctx->typing->constraints,
					(uint32_t)ctx->typing->context_projections.
						typed_projection_count,
					dependencies[dependency],
					id
				) != 0) {
				return -1;
			}
		}
		uint32_t motive_equation = ctx->typing->constraints.
			motive_constraint_for_match_case_projection[
				constraint->payload.branch_refinement.match_case_projection
			];
		uint32_t branch_equation = ctx->typing->constraints.
			classifier_constraint_for_projection[
				constraint->payload.branch_refinement.branch_typed_projection
			];
		if (motive_equation < ctx->typing->constraints.constraint_count) {
			if (prototype_typing_constraint_add_equation_dependency(
					&ctx->typing->constraints, id, motive_equation
				) != 0) {
				return -1;
			}
		} else {
			uint32_t owner_projection = constraint->payload.branch_refinement.
				match_typed_projection;
			int owner_is_live = operation_solver_projection_requires_live_equation(
				ctx, owner_projection
			);
			if (owner_is_live != 0) {
				return -1;
			}
		}
		if (branch_equation < ctx->typing->constraints.constraint_count) {
			if (prototype_typing_constraint_add_equation_dependency(
					&ctx->typing->constraints, id, branch_equation
				) != 0) {
				return -1;
			}
		} else {
			uint32_t branch_projection = constraint->payload.branch_refinement.
				branch_typed_projection;
			int branch_is_live = operation_solver_projection_requires_live_equation(
				ctx, branch_projection
			);
			if (branch_is_live != 0 || operation_solver_classifier_for_projection(
					ctx, branch_projection
				) == PROTOTYPE_INVALID_ID) {
				return -1;
			}
		}
		if (constraint->owner_typed_projection !=
			constraint->payload.branch_refinement.branch_typed_projection) {
			return -1;
		}
		const struct prototype_typed_projection* branch_projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections,
				constraint->payload.branch_refinement.branch_typed_projection
			);
		const struct prototype_context_projection* context_projection =
			branch_projection ? prototype_context_projection_get(
				&ctx->typing->context_projections,
				branch_projection->context_projection
			) : NULL;
		if (!branch_projection || !context_projection) {
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
				break;
			}
			if (action->kind == PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT &&
				action->dependency_id != constraint->payload.branch_refinement.
					match_case_projection) {
				uint32_t case_projection = action->dependency_id;
				if (case_projection >= ctx->typing->context_projections.
						match_case_projection_count) {
					return -1;
				}
				uint32_t enclosing_id = ctx->typing->constraints.
					branch_refinement_constraint_for_match_case_projection[
						case_projection
					];
				if (enclosing_id >= ctx->typing->constraints.constraint_count) {
					return -1;
				}
				const struct prototype_typing_constraint* enclosing =
					&ctx->typing->constraints.constraints[enclosing_id];
				if (enclosing->domain !=
						OPERATION_CONSTRAINT_DOMAIN_BRANCH_REFINEMENT ||
					enclosing->payload.branch_refinement.match_case_projection !=
						case_projection ||
					prototype_typing_constraint_add_dependency(
						&ctx->typing->constraints,
						(uint32_t)ctx->typing->context_projections.
							typed_projection_count,
						enclosing->payload.branch_refinement.
							branch_typed_projection,
						id
					) != 0) {
					return -1;
				}
			}
			if (action->parent_action == PROTOTYPE_INVALID_ID ||
				action->parent_action == action_id) {
				return -1;
			}
			action_id = action->parent_action;
			if (depth == 512) {
				return -1;
			}
		}
	}
	return 0;
}

static int operation_solver_initialize_classifier_state(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	struct prototype_typing_classifier_state* solver = &ctx->classifier_workspace;
	if (!solver->initialized) {
		solver->initialized = 1;
		for (uint32_t i = 0; i < 4096; ++i) {
			solver->expected_app_codomain_binders[i] = PROTOTYPE_INVALID_ID;
		}
	}
	return 0;
}

static int operation_solver_generate_constraints(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	if (!ctx->metadata) {
		fprintf(stderr, "constraint generation: compile metadata is missing\n");
		return -1;
	}
	if (ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		fprintf(
			stderr,
			"constraint generation: occurrence capacity exceeded count=%zu capacity=4096\n",
			ctx->metadata->typed_occurrences.occurrence_count
		);
		return -1;
	}
	ctx->metadata->constraint_generation_pass_count++;
	struct prototype_typing_constraint_db* constraints = &ctx->typing->constraints;
	if (operation_solver_initialize_classifier_state(ctx) != 0) {
		fprintf(stderr, "constraint generation: classifier state initialization failed\n");
		return -1;
	}
	for (uint32_t typed_projection = 0;
		typed_projection <
			ctx->typing->context_projections.typed_projection_count;
		++typed_projection) {
		const struct prototype_typed_projection* projected =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, typed_projection
			);
		int requires_equation = operation_solver_projection_requires_live_equation(
			ctx, typed_projection
		);
		if (requires_equation < 0) {
			const struct prototype_typed_projection* failed_projection =
				prototype_typed_projection_get(
					&ctx->typing->context_projections, typed_projection
				);
			fprintf(
				stderr,
				"constraint generation: live-equation decision failed projection=%u "
				"projection-count=%zu source=%u expanded=%d accepted-count=%zu\n",
				typed_projection,
				ctx->typing->context_projections.typed_projection_count,
				failed_projection ? failed_projection->source_occurrence :
					PROTOTYPE_INVALID_ID,
				failed_projection ? failed_projection->topology_expanded : -1,
				ctx->accepted_typing.occurrence_count
			);
			return -1;
		}
		if (!projected || !requires_equation) {
			continue;
		}
		uint32_t i = projected->source_occurrence;
		if (i >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* operation = &ctx->metadata->typed_occurrences.occurrences[i];
		if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
			fprintf(
				stderr,
				"constraint-generation projection=%u occurrence=%u tag=%d core=%u "
				"context=%u\n",
				typed_projection,
				i,
				operation->tag,
				operation->core_term,
				operation->context_id
			);
		}
		int base_constraint_kind = OPERATION_CONSTRAINT_HAS_TYPE;
		union prototype_typing_classifier_goal_payload base_payload = { 0 };
		uint32_t termination_computation;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
			prototype_term_terminates_witness_info(
				ctx->terms, operation->core_term, &termination_computation
			) == 0) {
			base_constraint_kind = OPERATION_CONSTRAINT_TERMINATION_WITNESS;
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
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
		/* Structural rules below are themselves the sole answer equation for
		 * their TypedProjection. Adding a generic HAS_TYPE equation first would
		 * create a second mutable solution cell whose only purpose is to receive
		 * the answer produced by the structural rule. Match is different: its
		 * case equations are premises of one aggregate Match answer. */
		int structural_rule_owns_answer =
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
			 (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION ||
			  operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_TYPE_FAMILY_CALCULATION));
		if (!structural_rule_owns_answer && operation_solver_add_classifier_goal(
				ctx,
				base_constraint_kind,
				typed_projection,
				base_payload
			) != 0) {
			fprintf(
				stderr,
				"constraint generation: base goal failed occurrence=%u tag=%d kind=%d\n",
				i,
				operation->tag,
				base_constraint_kind
			);
			return -1;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
			operation->core_term < ctx->terms->term_count &&
			ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_COMPUTATION_FOLD &&
			ctx->terms->terms[operation->core_term].as.computation_fold.
				clause_count == 0) {
			uint32_t producer_projection;
			uint32_t continuation_projection;
			if (operation_solver_projected_child(
					ctx, typed_projection,
					PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					&producer_projection
				) != 0 || operation_solver_projected_child(
					ctx, typed_projection,
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&continuation_projection
				) != 0) {
				return -1;
			}
			union prototype_typing_classifier_goal_payload relation = { 0 };
			relation.sequence_binder_relation.fold_typed_projection =
				typed_projection;
			relation.sequence_binder_relation.producer_typed_projection =
				producer_projection;
			if (operation_solver_add_classifier_goal(
					ctx,
					OPERATION_CONSTRAINT_SEQUENCE_BINDER_RELATION,
					continuation_projection,
					relation
				) != 0) {
				return -1;
			}
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
			uint32_t binding_context;
			int binder_status = operation_solver_var_binding_context(
				&ctx->metadata->typed_occurrences,
				&ctx->metadata->contexts, i, &binding_context
			);
			if (binder_status < 0) {
				fprintf(
					stderr,
					"constraint generation: binder lookup failed occurrence=%u\n",
					i
				);
				return -1;
			}
			if (binder_status == 0) {
				union prototype_typing_classifier_goal_payload payload = { 0 };
				payload.binder_type.binding_context = binding_context;
				payload.binder_type.binding_id = operation->binding_id;
				if (operation_solver_add_classifier_goal(
						ctx, OPERATION_CONSTRAINT_BINDER_TYPE,
						typed_projection, payload
					) != 0) {
					fprintf(
						stderr,
						"constraint generation: binder goal failed projection=%u "
						"occurrence=%u context=%u binding-context=%u binding=%u\n",
						typed_projection,
						i,
						operation->context_id,
						binding_context,
						operation->binding_id
					);
					return -1;
				}
			} else if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_HAS_TYPE,
					typed_projection,
					(union prototype_typing_classifier_goal_payload) { 0 }
				) != 0) {
				return -1;
			}
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			union prototype_typing_classifier_goal_payload payload = { 0 };
			payload.reference.referenced_operation = operation->wrapped_occurrence;
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_EQUAL,
					typed_projection, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			union prototype_typing_classifier_goal_payload payload = { 0 };
			payload.conversion.body_occurrence = operation->wrapped_occurrence;
			payload.conversion.expected_classifier =
				ctx->typing->seed.classifier_seeds[i].classifier;
			if (operation->wrapped_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
				operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_CONVERTIBLE,
					typed_projection, payload
				) != 0) {
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
			union prototype_typing_classifier_goal_payload payload = { 0 };
			payload.pi.role = OPERATION_CLASSIFIER_PI_GOAL_INTRO;
			if (compile_occurrence_child(
					ctx, i, PROTOTYPE_TERM_CHILD_BODY, 0,
					&payload.pi.body_or_function_operation
				) != 0) {
				fprintf(stderr,
					"constraint generation: Lambda body lookup failed projection=%u occurrence=%u\n",
					typed_projection, i);
				return -1;
			}
			payload.pi.domain_classifier_or_argument_occurrence =
				operation_solver_binder_classifier(ctx, i);
			if (operation_solver_add_classifier_goal(
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED,
					typed_projection, payload
				) != 0) {
				fprintf(stderr,
					"constraint generation: Pi intro goal failed projection=%u occurrence=%u\n",
					typed_projection, i);
				return -1;
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
			(operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION ||
			 operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_TYPE_FAMILY_CALCULATION)) {
			union prototype_typing_classifier_goal_payload payload = { 0 };
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
					ctx, OPERATION_CONSTRAINT_PI_EXPECTED,
					typed_projection, payload
				) != 0) {
				return -1;
			}
			uint32_t function_projection;
			uint32_t argument_projection;
			if (operation_solver_projected_child(
					ctx, typed_projection, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
					&function_projection
				) != 0 || operation_solver_projected_child(
					ctx, typed_projection, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
					&argument_projection
				) != 0) {
				return -1;
			}
			union prototype_typing_classifier_goal_payload relation = { 0 };
			relation.pi_domain_relation.application_typed_projection =
				typed_projection;
			relation.pi_domain_relation.function_typed_projection =
				function_projection;
			if (operation_solver_add_classifier_goal(
					ctx,
					OPERATION_CONSTRAINT_PI_DOMAIN_RELATION,
					argument_projection,
					relation
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
					) != 0) {
					return -1;
				}
				if (operation_solver_add_motive_case_goal(
						ctx, typed_projection, i, j,
						branch_operation, scrutinee_operation
					) != 0) {
					fprintf(
						stderr,
						"constraint generation: motive goal failed match=%u case=%u\n",
						i,
						j
					);
					return -1;
				}
			}
		} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			union prototype_typing_classifier_goal_payload payload = { 0 };
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
					ctx, OPERATION_CONSTRAINT_IH_EXPECTED,
					typed_projection, payload
				) != 0) {
				return -1;
			}
		}
	}
	/* Each structural or relational equation above owns one candidate. The
	 * projection aggregate is keyed only by its owner; its premise set is the
	 * partition of non-aggregate classifier equations with that owner. This lets
	 * later Solver expansion append premises without creating another answer
	 * owner or mutating an immutable operand snapshot. */
	uint32_t projection_count = (uint32_t)
		ctx->typing->context_projections.typed_projection_count;
	uint8_t* has_premise = projection_count == 0 ? NULL :
		calloc(projection_count, sizeof(*has_premise));
	if (projection_count != 0 && !has_premise) {
		return -1;
	}
	for (uint32_t equation = prototype_typing_constraint_first_in_domain(
			constraints, OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
		equation != PROTOTYPE_INVALID_ID;
		equation = prototype_typing_constraint_next_in_domain(
			constraints, equation
		)) {
		if (constraints->constraints[equation].kind ==
				OPERATION_CONSTRAINT_PROJECTED_CLASSIFIER) {
			continue;
		}
		uint32_t owner = constraints->constraints[equation].owner_typed_projection;
		if (owner >= projection_count) {
			free(has_premise);
			return -1;
		}
		has_premise[owner] = 1;
	}
	for (uint32_t projection = 0; projection < projection_count; ++projection) {
		if (has_premise[projection] &&
			operation_solver_add_projected_classifier_aggregate(
				ctx, projection
			) != 0) {
			free(has_premise);
			return -1;
		}
	}
	free(has_premise);
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
	if (prototype_typing_constraint_derive_sccs(
			&ctx->typing->constraints,
			(uint32_t)ctx->typing->context_projections.typed_projection_count
		) != 0) {
		fprintf(stderr, "constraint generation: equation SCC derivation failed\n");
		return -1;
	}
	if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
		for (uint32_t scc = 0; scc < ctx->typing->constraints.scc_count;
			++scc) {
			if (!ctx->typing->constraints.scc_recursive[scc]) continue;
			fprintf(stderr, "constraint-scc id=%u members=", scc);
			uint32_t first = ctx->typing->constraints.first_scc_member[scc];
			uint32_t count = ctx->typing->constraints.scc_member_count[scc];
			for (uint32_t member = 0; member < count; ++member) {
				uint32_t equation =
					ctx->typing->constraints.scc_members[first + member];
				fprintf(
					stderr, "%s%u:%d", member == 0 ? "" : ",",
					equation,
					ctx->typing->constraints.constraints[equation].kind
				);
			}
			fputc('\n', stderr);
		}
	}
	if (operation_constraint_db_validate(ctx) != 0) {
		fprintf(stderr, "constraint generation: final validation failed\n");
		return -1;
	}
	ctx->classifier_workspace.initialized_occurrence_count =
		ctx->metadata->typed_occurrences.occurrence_count;
	return 0;
}

static int operation_solver_seed_known_classifiers(struct compile_context* ctx, int* p_changed) {
	if (!ctx || !ctx->metadata || !p_changed) {
		return -1;
	}
	for (uint32_t typed_projection = 0;
		typed_projection <
			ctx->typing->context_projections.typed_projection_count;
		++typed_projection) {
		const struct prototype_typed_projection* projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, typed_projection
			);
		int requires_equation = operation_solver_projection_requires_live_equation(
			ctx, typed_projection
		);
		if (requires_equation < 0) {
			return -1;
		}
		if (!projection || !requires_equation) {
			continue;
		}
		uint32_t i = projection->source_occurrence;
		if (i >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		uint32_t seed = ctx->typing->seed.classifier_seeds[i].classifier;
		if (seed == PROTOTYPE_INVALID_ID &&
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR) {
			seed = operation->declared_classifier;
		}
		if (i < ctx->accepted_typing.occurrence_count) {
			const struct prototype_typed_publication_projection* accepted = NULL;
			int accepted_status = prototype_accepted_typing_input_lookup(
				&ctx->accepted_typing, i, &accepted, NULL
			);
			if (accepted_status < 0) {
				return -1;
			}
			if (accepted_status == 0) {
				if (!accepted) {
					return -1;
				}
				seed = accepted->classifier;
			}
		}
		/*
		 * ASCRIPTION is solved against its body, and NAME follows its referenced
		 * source operation. Seeding either from the surface classifier creates a
		 * second authoritative path and can retain unsolved elaboration rows.
		 */
		if (operation_solver_classifier_for_projection(
				ctx, typed_projection
			) ==
				PROTOTYPE_INVALID_ID &&
			operation->tag != PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
			operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
				seed != PROTOTYPE_INVALID_ID) {
				uint32_t concrete_context;
				uint32_t substitution;
				uint32_t contextual_seed;
					int context_status = operation_typed_projection_materialized_context(
						ctx,
						typed_projection,
						&concrete_context,
						&substitution
					);
					if (context_status > 0) {
						continue;
					}
				const struct prototype_substitution* action = context_status == 0 ?
					prototype_substitution_get(
						&ctx->metadata->substitutions, substitution
					) : NULL;
				if (context_status < 0 || !action ||
					action->source_context != concrete_context ||
					action->target_context != operation->context_id ||
					prototype_term_reindex(
						ctx->terms,
						ctx->type_declarations,
						&ctx->metadata->contexts,
						&ctx->metadata->substitutions,
						seed,
						substitution,
						&contextual_seed
					) != 0 || operation_solver_publish_projection(
						ctx, typed_projection, contextual_seed, p_changed
					) != 0) {
					return -1;
				}
		}
	}
	uint32_t transaction_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t i = transaction_start;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		uint32_t binder_seed =
			ctx->typing->seed.classifier_seeds[i].binder_classifier;
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
			operation_solver_binder_classifier(ctx, i) == PROTOTYPE_INVALID_ID &&
				binder_seed != PROTOTYPE_INVALID_ID) {
				uint32_t contextual_binder_seed;
				int context_status = operation_solver_term_in_occurrence_context(
					ctx, i, binder_seed, &contextual_binder_seed
				);
				if (context_status > 0) {
					continue;
				}
				if (context_status < 0 || operation_binder_classifier_meta_solve(
						ctx, i, contextual_binder_seed, p_changed
					) != 0) {
				return -1;
			}
		}
	}
	return 0;
}
