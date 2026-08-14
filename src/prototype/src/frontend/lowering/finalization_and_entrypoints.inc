static int compile_phase_infer_pending_types(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}

	for (;;) {
		size_t before_operation_classifiers = count_classified_operations(ctx);
		size_t before_unresolved = count_unresolved_resolution_items(ctx);
		size_t before_relations = ctx->judgement_delta.proposition_count;
		int match_resolution_status = compile_phase_resolve_pending_match_items(ctx);
		if (match_resolution_status < 0) {
			return -1;
		}
		if (operation_solver_resolve_contexts(ctx, 0) != 0) {
			return -1;
		}
			int status = compile_phase_infer_general_classifiers(ctx, 0);
			if (status != 0) {
				return -1;
			}
			if (operation_solver_resolve_contexts(ctx, 0) != 0) {
				return -1;
			}
		int cbpv_changed = 0;
		if (bind_cbpv_operation_classifiers(ctx, &cbpv_changed) != 0) {
			fprintf(stderr, "typing fixed point: CBPV classifier binding failed\n");
			return -1;
		}
		if (operation_solver_generate_computation_constraints(ctx) != 0) {
			fprintf(stderr, "typing fixed point: computation constraint generation failed\n");
			return -1;
		}
		if (operation_solver_refresh_computation_constraint_operands(ctx) != 0) {
			fprintf(stderr, "typing fixed point: computation operand refresh failed\n");
			return -1;
		}
		int request_changed = 0;
		if (prototype_judgement_delta_solve_recorded_computation_requests(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations
			) != 0 || operation_solver_bind_computation_constraint_solutions(
				ctx, &request_changed
			) != 0 || operation_solver_refresh_computation_constraint_operands(ctx) != 0) {
			fprintf(stderr, "typing fixed point: operation request closure failed\n");
			return -1;
		}
		if (request_changed) {
			cbpv_changed = 1;
			continue;
		}
		if (prototype_judgement_delta_solve_recorded_computation_constraints(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations
			) != 0) {
			fprintf(stderr, "typing fixed point: computation constraint solving failed\n");
			return -1;
		}
		if (operation_solver_bind_computation_constraint_solutions(
				ctx, &cbpv_changed
			) != 0) {
			fprintf(stderr, "typing fixed point: computation solution binding failed\n");
			return -1;
		}
		if (operation_effect_generate_constraints(ctx) != 0) {
			fprintf(stderr, "typing fixed point: effect constraint generation failed\n");
			return -1;
		}
		if (count_classified_operations(ctx) == before_operation_classifiers &&
			count_unresolved_resolution_items(ctx) == before_unresolved &&
			ctx->judgement_delta.proposition_count == before_relations && !cbpv_changed) {
			break;
		}
	}

	int match_resolution_status = compile_phase_resolve_pending_match_items(ctx);
	if (match_resolution_status < 0) {
		return -1;
	}
	int status = compile_phase_infer_general_classifiers(ctx, 1);
	if (status != 0) {
		fprintf(stderr, "typing fixed point: final classifier inference failed\n");
		return -1;
	}
	if (operation_solver_resolve_contexts(ctx, 0) != 0) {
		fprintf(stderr, "typing fixed point: final context resolution failed\n");
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		uint32_t operation = ctx->pending_match_typings[i].operation;
		if (operation >= ctx->metadata->operation_count ||
			ctx->classifier_solver.solutions[operation].classifier ==
				PROTOTYPE_INVALID_ID) {
			fprintf(
				stderr,
				"pending Match classifier missing index=%u operation=%u operation-count=%zu classifier=%u\n",
				i,
				operation,
				ctx->metadata->operation_count,
				operation < ctx->metadata->operation_count ?
					ctx->classifier_solver.solutions[operation].classifier :
					PROTOTYPE_INVALID_ID
			);
			if (operation < ctx->metadata->operation_count) {
				const struct prototype_operation_node* node =
					&ctx->metadata->operations[operation];
				(void)add_compile_diagnostic(
					ctx,
					PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
					PROTOTYPE_COMPILE_DIAGNOSTIC_MATCH_RESULT_MOTIVE,
					node->source_ast,
					operation,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					node->source_ast < ctx->asts->node_count ?
						ctx->asts->nodes[node->source_ast].span :
						(struct prototype_source_span) { 0, 0 }
				);
			}
			return -1;
		}
	}
	if (operation_solver_materialize_judgements(ctx) != 0) {
		fprintf(stderr, "typing fixed point: judgement materialization failed\n");
		return -1;
	}
	/* Reification can introduce binder premises in a symbolic Context after the
	 * last solver pass. Refine those Contexts before publishing exact binding
	 * identities to JudgementDB. */
	if (operation_solver_resolve_contexts(ctx, 1) != 0) {
		fprintf(stderr, "typing fixed point: finalized context resolution failed\n");
		return -1;
	}
	/* Candidate publication is decided by the authority-complete Claim closure.
	 * Do not pre-prune structural derivations with legacy Core tuples here. */
	if (prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0) {
		fprintf(stderr, "typing fixed point: judgement commit failed\n");
		return -1;
	}
	/* The materialized classifier relation has been committed. This temporary
	 * solver lookup index must not survive as pending JudgementDB state. */
	ctx->judgement_delta.match_motive_result_count = 0;
	return count_unresolved_resolution_items(ctx) == 0 ? 0 : -1;
}

static int operation_check_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t expected_classifier,
	uint32_t depth
) {
	if (!ctx || operation_id >= ctx->metadata->operation_count ||
		expected_classifier >= ctx->terms->term_count || depth > 128) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	/* Nested surface lambdas are represented by a direct OperationGraph edge,
	 * while their enclosing Pi codomain records the implicit
	 * RETURN(THUNK(lambda)) boundary. Expose the raw Pi before checking the
	 * lambda occurrence itself. */
	if (operation->tag == PROTOTYPE_OPERATION_LAMBDA &&
		ctx->terms->terms[expected_classifier].tag ==
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		uint32_t result = ctx->terms->terms[
			expected_classifier
		].as.computation_type.result;
		if (result < ctx->terms->term_count &&
			ctx->terms->terms[result].tag == PROTOTYPE_TERM_THUNK_TYPE) {
			expected_classifier =
				ctx->terms->terms[result].as.thunk_type.computation;
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_NAME &&
		operation->function < ctx->metadata->operation_count) {
		return operation_check_classifier(
			ctx, operation->function, expected_classifier, depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION &&
		operation->body < ctx->metadata->operation_count) {
		return operation_check_classifier(
			ctx, operation->body, expected_classifier, depth + 1
		);
	}
	uint32_t actual_classifier = operation_available_classifier(operation);
	if (operation->tag == PROTOTYPE_OPERATION_THUNK &&
		operation->argument < ctx->metadata->operation_count &&
		ctx->terms->terms[expected_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		return operation_check_classifier(
			ctx,
			operation->argument,
			ctx->terms->terms[expected_classifier].as.thunk_type.computation,
			depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_RETURN &&
		operation->argument < ctx->metadata->operation_count &&
		ctx->terms->terms[expected_classifier].tag ==
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return operation_check_classifier(
			ctx,
			operation->argument,
			ctx->terms->terms[expected_classifier].as.computation_type.result,
			depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_LAMBDA &&
		operation->body < ctx->metadata->operation_count &&
		operation->core_term < ctx->terms->term_count &&
		ctx->terms->terms[operation->core_term].tag == PROTOTYPE_TERM_LAMBDA) {
		uint32_t expected_domain;
		uint32_t expected_family;
		uint32_t expected_binder;
		uint32_t expected_body;
		uint32_t body_binder_var;
		uint32_t specialized_body;
		uint32_t solved_expected_domain;
		int pi_status = prototype_judgement_pi_parts(
				ctx->terms, expected_classifier,
				&expected_domain, &expected_family
			);
		int family_status = pi_status == 0 ? prototype_term_pure_family_parts(
				ctx->terms, expected_family,
				&expected_binder, &expected_body
			) : -1;
		int domain_equal = pi_status == 0 && family_status == 0 &&
			operation->binder_classifier < ctx->terms->term_count &&
			(prototype_judgement_classifier_reference_equal(
				ctx->terms, ctx->type_declarations,
				expected_domain, operation->binder_classifier
			) || (prototype_judgement_solve_expected_effect_rows(
					ctx->terms, ctx->type_declarations, NULL,
					expected_domain, operation->binder_classifier,
					&solved_expected_domain
				) == 0 && prototype_judgement_classifier_reference_equal(
					ctx->terms, ctx->type_declarations,
					solved_expected_domain, operation->binder_classifier
				)) || prototype_judgement_classifier_compatible(
					ctx->terms, ctx->type_declarations,
					expected_domain, operation->binder_classifier
				));
		if (pi_status != 0 || family_status != 0 || !domain_equal ||
			prototype_term_var(
				ctx->terms,
				ctx->terms->terms[operation->core_term].as.lambda.binding_id,
				&body_binder_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, ctx->type_declarations, expected_body,
				expected_binder, body_binder_var, &specialized_body
			) != 0) {
			return 0;
		}
		return operation_check_classifier(
			ctx, operation->body, specialized_body, depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_MATCH &&
		operation->scrutinee < ctx->metadata->operation_count &&
		operation->core_term < ctx->terms->term_count &&
		ctx->terms->terms[operation->core_term].tag == PROTOTYPE_TERM_MATCH &&
		operation->first_case + operation->case_count <=
			ctx->metadata->operation_case_count) {
		const struct prototype_operation_node* scrutinee =
			&ctx->metadata->operations[operation->scrutinee];
		const struct prototype_term* match =
			&ctx->terms->terms[operation->core_term];
		for (uint32_t i = 0; i < operation->case_count; ++i) {
			const struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[operation->first_case + i];
			uint32_t term_case_id = match->as.match.first_case + i;
			if (term_case_id >= ctx->terms->case_count ||
				operation_case->body_operation >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_match_case* term_case =
				&ctx->terms->cases[term_case_id];
			uint32_t refined_context;
			uint32_t refinement_substitution;
			uint32_t constructor_term;
			int refinement_status =
				prototype_judgement_source_indexed_branch_refinement(
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					ctx->terms,
					ctx->type_declarations,
					scrutinee->context_id,
					scrutinee->core_term,
					operation_available_classifier(scrutinee),
					operation_case->constructor_id,
					operation_case->context_id,
					&ctx->terms->case_binders[term_case->first_binder],
					term_case->binder_count,
					&refined_context,
					&refinement_substitution,
					&constructor_term
				);
			(void)refined_context;
			(void)constructor_term;
			if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
				continue;
			}
			if (refinement_status != PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
				return refinement_status == PROTOTYPE_INDEX_REFINEMENT_INVALID ? -1 : 0;
			}
			uint32_t refined_expected;
			uint32_t branch_classifier = operation_available_classifier(
				&ctx->metadata->operations[operation_case->body_operation]
			);
			uint32_t refined_actual;
			uint32_t solved_refined_expected;
			if (branch_classifier >= ctx->terms->term_count ||
				prototype_term_reindex(
					ctx->terms, ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					expected_classifier, refinement_substitution,
					&refined_expected
				) != 0 || prototype_term_reindex(
					ctx->terms, ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					branch_classifier, refinement_substitution,
					&refined_actual
				) != 0 || prototype_judgement_solve_expected_effect_rows(
					ctx->terms, ctx->type_declarations, NULL,
					refined_expected, refined_actual,
					&solved_refined_expected
				) != 0 || prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					solved_refined_expected, refined_actual
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
		}
		uint32_t scrutinee_type_id;
		uint32_t scrutinee_arguments[32];
		uint32_t scrutinee_argument_count;
		if (prototype_term_type_instance_info(
				ctx->terms, operation_available_classifier(scrutinee),
				&scrutinee_type_id, scrutinee_arguments,
				&scrutinee_argument_count
			) == 0 && scrutinee_type_id < ctx->type_declarations->type_count) {
			const struct prototype_type_declaration* declaration =
				&ctx->type_declarations->type_declarations[scrutinee_type_id];
			for (uint32_t constructor_index = 0;
				constructor_index < declaration->constructor_count;
				++constructor_index) {
				int covered = 0;
				for (uint32_t case_index = 0;
					case_index < operation->case_count;
					++case_index) {
					if (ctx->metadata->operation_cases[
						operation->first_case + case_index
					].constructor_id == constructor_index) {
						covered = 1;
						break;
					}
				}
				if (covered) {
					continue;
				}
				const struct prototype_type_constructor_declaration* constructor =
					&ctx->type_declarations->constructor_declarations[
						declaration->first_constructor + constructor_index
					];
				uint32_t declaration_fields[64];
				uint32_t field_count;
				if (prototype_context_extension_path(
						&ctx->metadata->contexts,
						constructor->parameter_context,
						constructor->field_context,
						declaration_fields, 64, &field_count
					) != 0) {
					return -1;
				}
				struct prototype_case_binder binders[64];
				uint32_t synthetic_context = scrutinee->context_id;
				uint32_t owner;
				if (prototype_term_type_instance_make(
						ctx->terms, ctx->type_declarations,
						scrutinee_type_id, scrutinee_arguments,
						declaration->parameter_count, &owner
					) != 0) {
					return -1;
				}
				for (uint32_t field = 0; field < field_count; ++field) {
					binders[field].binding_id = prototype_term_new_binding(ctx->terms);
					binders[field].is_recursive = 0;
					uint32_t field_classifier;
					if (binders[field].binding_id == PROTOTYPE_INVALID_ID ||
						prototype_judgement_constructor_field_classifier(
							ctx->terms, ctx->type_declarations,
							&ctx->metadata->contexts,
							&ctx->metadata->substitutions,
							synthetic_context, owner, constructor_index,
							binders, field, field, &field_classifier
						) != 0 || prototype_context_extend(
							&ctx->metadata->contexts, synthetic_context,
							binders[field].binding_id, field_classifier,
							PROTOTYPE_INVALID_ID, &synthetic_context
						) != 0) {
						return -1;
					}
				}
				uint32_t ignored_context;
				uint32_t ignored_substitution;
				uint32_t ignored_constructor;
				int omission_status =
					prototype_judgement_source_indexed_branch_refinement(
						&ctx->metadata->contexts,
						&ctx->metadata->substitutions,
						ctx->terms, ctx->type_declarations,
						scrutinee->context_id, scrutinee->core_term,
						operation_available_classifier(scrutinee),
						constructor_index, synthetic_context, binders,
						field_count, &ignored_context,
						&ignored_substitution, &ignored_constructor
					);
				if (omission_status != PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
					return omission_status == PROTOTYPE_INDEX_REFINEMENT_INVALID ? -1 : 0;
				}
			}
		}
		return 1;
	}
	return actual_classifier < ctx->terms->term_count &&
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations,
			expected_classifier, actual_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
}

/* Propagate an independently established expected classifier through the
 * source-operation structure. This is a later constraint-solving step, not
 * bidirectional surface elaboration: synthesis has already built every node.
 * In particular, an all-impossible indexed Match may acquire its motive from
 * a constructor domain without allowing an unreachable branch body to choose
 * that motive. */
static int operation_refine_classifier_from_expected(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t expected_classifier,
	uint32_t depth,
	int* p_changed
) {
	if (!ctx || !p_changed || operation_id >= ctx->metadata->operation_count ||
		expected_classifier >= ctx->terms->term_count || depth > 128) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag == PROTOTYPE_OPERATION_LAMBDA &&
		ctx->terms->terms[expected_classifier].tag ==
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		uint32_t result = ctx->terms->terms[
			expected_classifier
		].as.computation_type.result;
		if (result < ctx->terms->term_count &&
			ctx->terms->terms[result].tag == PROTOTYPE_TERM_THUNK_TYPE) {
			expected_classifier =
				ctx->terms->terms[result].as.thunk_type.computation;
		}
	}
	int check = operation_check_classifier(
		ctx, operation_id, expected_classifier, depth
	);
	if (check <= 0) {
		return check;
	}
	if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION &&
		operation->body < ctx->metadata->operation_count) {
		int status = operation_refine_classifier_from_expected(
			ctx, operation->body, expected_classifier, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_OPERATION_NAME &&
		operation->function < ctx->metadata->operation_count) {
		int status = operation_refine_classifier_from_expected(
			ctx, operation->function, expected_classifier, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_OPERATION_THUNK &&
		operation->argument < ctx->metadata->operation_count &&
		ctx->terms->terms[expected_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		int status = operation_refine_classifier_from_expected(
			ctx, operation->argument,
			ctx->terms->terms[expected_classifier].as.thunk_type.computation,
			depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_OPERATION_RETURN &&
		operation->argument < ctx->metadata->operation_count &&
		ctx->terms->terms[expected_classifier].tag ==
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		int status = operation_refine_classifier_from_expected(
			ctx, operation->argument,
			ctx->terms->terms[expected_classifier].as.computation_type.result,
			depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_OPERATION_LAMBDA &&
		operation->body < ctx->metadata->operation_count &&
		operation->core_term < ctx->terms->term_count &&
		ctx->terms->terms[operation->core_term].tag == PROTOTYPE_TERM_LAMBDA) {
		uint32_t expected_domain;
		uint32_t expected_family;
		uint32_t expected_binder;
		uint32_t expected_body;
		uint32_t body_binder_var;
		uint32_t specialized_body;
		if (prototype_judgement_pi_parts(
				ctx->terms, expected_classifier,
				&expected_domain, &expected_family
			) != 0 || prototype_term_pure_family_parts(
				ctx->terms, expected_family,
				&expected_binder, &expected_body
			) != 0 || prototype_term_var(
				ctx->terms,
				ctx->terms->terms[operation->core_term].as.lambda.binding_id,
				&body_binder_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, ctx->type_declarations, expected_body,
				expected_binder, body_binder_var, &specialized_body
			) != 0) {
			return -1;
		}
		int status = operation_refine_classifier_from_expected(
			ctx, operation->body, specialized_body, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	}
	if (operation->tag == PROTOTYPE_OPERATION_MATCH) {
		if (operation->scrutinee >= ctx->metadata->operation_count) {
			return -1;
		}
		uint32_t scrutinee = ctx->metadata->operations[
			operation->scrutinee
		].core_term;
		if (scrutinee >= ctx->terms->term_count ||
			ctx->terms->terms[scrutinee].tag != PROTOTYPE_TERM_VAR) {
			return -1;
		}
		uint32_t motive_binder = prototype_term_new_binding(ctx->terms);
		uint32_t motive_var;
		uint32_t motive_body;
		uint32_t motive;
		uint32_t motive_application;
		if (motive_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
				ctx->terms, motive_binder, &motive_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, ctx->type_declarations, expected_classifier,
				ctx->terms->terms[scrutinee].as.var.binding_id,
				motive_var, &motive_body
			) != 0 || prototype_term_lambda(
				ctx->terms, motive_binder, motive_body, &motive
			) != 0 || prototype_term_app(
				ctx->terms, motive, scrutinee, &motive_application
			) != 0 || operation_solver_bind_proven_classifier(
				ctx, operation_id, motive_application, p_changed
			) != 0) {
			return -1;
		}
		ctx->classifier_solver.motive_terms[operation_id] = motive;
		ctx->classifier_solver.motive_constant_candidates[operation_id] =
			PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_solution_states[operation_id] =
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
		if (ctx->materializing_checked_classifiers) {
			ctx->metadata->operations[operation_id].known_classifier =
				motive_application;
			ctx->metadata->operations[operation_id].classifier =
				motive_application;
		}
	}
	if (ctx->materializing_checked_classifiers &&
		operation->tag != PROTOTYPE_OPERATION_MATCH) {
		operation_solver_store_classifier_solution(
			ctx, operation_id, expected_classifier
		);
		ctx->metadata->operations[operation_id].known_classifier =
			expected_classifier;
		ctx->metadata->operations[operation_id].classifier = expected_classifier;
	} else if (operation->tag != PROTOTYPE_OPERATION_NAME &&
		operation_solver_classifier(ctx, operation_id) == PROTOTYPE_INVALID_ID &&
		operation_solver_bind_proven_classifier(
			ctx, operation_id, expected_classifier, p_changed
		) != 0) {
		return -1;
	}
	return 1;
}

static int compile_phase_check_ascriptions(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}

	struct prototype_term_definition imported_definitions[1024];
	struct prototype_term_definition_env imported_definition_env;
	if (build_imported_external_definition_env(
			ctx,
			imported_definitions,
			1024,
			&imported_definition_env
		) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < ctx->pending_ascription_check_count; ++i) {
		const struct pending_ascription_check* check =
			&ctx->pending_ascription_checks[i];
		uint32_t operation_classifier = PROTOTYPE_INVALID_ID;
		uint32_t ascription_operation = PROTOTYPE_INVALID_ID;
		uint32_t solved_expected_classifier = check->expected_classifier;
		if (check->operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[check->operation].classifier !=
				PROTOTYPE_INVALID_ID) {
			operation_classifier = ctx->metadata->operations[check->operation].classifier;
			int solve_status = prototype_judgement_solve_expected_effect_rows(
					ctx->terms,
					ctx->type_declarations,
					&imported_definition_env,
					check->expected_classifier,
					operation_classifier,
					&solved_expected_classifier
				);
			if (solve_status != 0) {
				int structural_status = operation_check_classifier(
					ctx, check->operation, check->expected_classifier, 0
				);
				if (structural_status < 0) {
					return -1;
				}
				if (structural_status > 0) {
					solved_expected_classifier = check->expected_classifier;
					goto ascription_classifier_checked;
				}
				(void)add_resolve_error(
					ctx,
					PROTOTYPE_RESOLVE_ERROR_COMPILE,
					-1,
					-1,
					check->ast
				);
				(void)add_compile_diagnostic(
					ctx,
					PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_EXPECTATION_CHECK,
					PROTOTYPE_COMPILE_DIAGNOSTIC_ASCRIPTION_MISMATCH,
					check->ast,
					check->operation,
					PROTOTYPE_INVALID_ID,
					check->expected_classifier,
					operation_classifier,
					check->ast < ctx->asts->node_count ?
						ctx->asts->nodes[check->ast].span :
						(struct prototype_source_span) { 0, 0 }
				);
				continue;
			}
		}
	ascription_classifier_checked:
		int structurally_checked = operation_check_classifier(
			ctx, check->operation, solved_expected_classifier, 0
		);
		if (structurally_checked < 0) {
			return -1;
		}
		if (operation_classifier == PROTOTYPE_INVALID_ID ||
			(!structurally_checked && !prototype_judgement_classifier_compatible_with_definitions(
				ctx->terms,
				ctx->type_declarations,
				&imported_definition_env,
				solved_expected_classifier,
				operation_classifier
			))) {
			(void)add_resolve_error(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				-1,
				-1,
				check->ast
			);
			(void)add_compile_diagnostic(
				ctx,
				PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_EXPECTATION_CHECK,
				PROTOTYPE_COMPILE_DIAGNOSTIC_ASCRIPTION_MISMATCH,
				check->ast,
				check->operation,
				PROTOTYPE_INVALID_ID,
				solved_expected_classifier,
				operation_classifier,
				check->ast < ctx->asts->node_count ?
					ctx->asts->nodes[check->ast].span :
					(struct prototype_source_span) { 0, 0 }
			);
			continue;
		}
		for (uint32_t operation = 0;
			operation < ctx->metadata->operation_count;
			++operation) {
			if (ctx->metadata->operations[operation].tag ==
					PROTOTYPE_OPERATION_ASCRIPTION &&
				ctx->metadata->operations[operation].body == check->operation &&
				ctx->metadata->operations[operation].source_ast == check->ast) {
				ctx->metadata->operations[operation].classifier =
					solved_expected_classifier;
				ascription_operation = operation;
			}
		}
		if (ascription_operation == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		/* This phase validates and solves the source ascription constraint. Its
		 * evidence is materialized earlier from the exact ASCRIPTION Operation in
		 * JudgementDelta. Writing a second relation here would bypass closure and
		 * solver-evidence atomicity. */
	}
	return 0;
}

static int compile_phase_check_expectations(struct compile_context* ctx) {
	if (!ctx || !ctx->asts) {
		return -1;
	}

	struct prototype_term_definition imported_definitions[1024];
	struct prototype_term_definition_env imported_definition_env;
	if (build_imported_external_definition_env(
			ctx,
			imported_definitions,
			1024,
			&imported_definition_env
		) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)ctx->asts->expectation_count; ++i) {
		struct prototype_ast_type_expectation_def* expectation = &ctx->asts->expectations[i];
		if (expectation->kind != PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION) {
			continue;
		}
		struct prototype_ast_term_assignment_def* def;
		uint32_t expected_classifier;
		uint32_t actual_classifier = PROTOTYPE_INVALID_ID;
		uint32_t compiled_term;
		int actual_from_operation = 0;
		if (resolve_unique_assignment(ctx, expectation->name_symbol_id, PROTOTYPE_INVALID_ID, &def) != 0) {
			continue;
		}
		ctx->binder_count = 0;
		ctx->ih_scope_count = 0;
		if (compile_def(ctx, def, &compiled_term) != 0 ||
			compile_type_expectation_classifier(ctx, expectation, &expected_classifier) != 0) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				expectation->name_symbol_id,
				-1,
				def->ast,
				expectation->name_span
			);
			continue;
		}
		if (def->definition_value_required && def->ast < ctx->asts->node_count &&
			ctx->asts->nodes[def->ast].tag != PROTOTYPE_AST_QUOTE &&
			def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag ==
				PROTOTYPE_OPERATION_THUNK) {
			/*
			 * The ascription checks the explicit definition RHS before the definition
			 * boundary inserts its implicit thunk. The exported definition retains
			 * the synthesized thunk classifier.
			 */
			continue;
		}
		struct compile_ref expected_ref;
		compile_ref_clear(&expected_ref);
		if (def->compiled_operation >= ctx->metadata->operation_count) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				expectation->name_symbol_id,
				-1,
				def->ast,
				expectation->name_span
			);
			continue;
		}
		expected_ref.category =
			ctx->metadata->operations[def->compiled_operation].category;
		expected_ref.computation_kind =
			ctx->metadata->operations[def->compiled_operation].computation_kind;
		if (compile_expected_classifier_for_ref(
				ctx, &expected_ref, expected_classifier, &expected_classifier
			) != 0) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				expectation->name_symbol_id,
				-1,
				def->ast,
				expectation->type_span
			);
			continue;
		}
		if (def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag ==
				PROTOTYPE_OPERATION_ASCRIPTION &&
			ctx->metadata->operations[def->compiled_operation].body <
				ctx->metadata->operation_count &&
			ctx->metadata->operations[
				ctx->metadata->operations[def->compiled_operation].body
			].classifier != PROTOTYPE_INVALID_ID) {
			/* The ascription is an annotation; its body supplies the evidence. */
			actual_classifier = ctx->metadata->operations[
				ctx->metadata->operations[def->compiled_operation].body
			].classifier;
			actual_from_operation = 1;
		} else if (def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag !=
				PROTOTYPE_OPERATION_ASCRIPTION &&
			ctx->metadata->operations[def->compiled_operation].classifier !=
				PROTOTYPE_INVALID_ID) {
			/*
			 * A source definition denotes an operation-layer node. Its classifier
			 * must not be recovered by collecting every classifier ever assigned to
			 * the shared erased core term.
			 */
			actual_classifier =
				ctx->metadata->operations[def->compiled_operation].classifier;
		} else if (def->compiled_operation < ctx->metadata->operation_count &&
			ctx->metadata->operations[def->compiled_operation].tag !=
				PROTOTYPE_OPERATION_ASCRIPTION &&
			def->compiled_classifier != PROTOTYPE_INVALID_ID) {
			actual_classifier = def->compiled_classifier;
		} else {
			actual_classifier = PROTOTYPE_INVALID_ID;
		}
		int structurally_checked = actual_classifier == PROTOTYPE_INVALID_ID ? 0 :
			operation_check_classifier(
				ctx, def->compiled_operation, expected_classifier, 0
			);
		if (structurally_checked < 0) {
			return -1;
		}
		if (actual_classifier == PROTOTYPE_INVALID_ID ||
			(!structurally_checked && prototype_judgement_solve_expected_effect_rows(
				ctx->terms,
				ctx->type_declarations,
				&imported_definition_env,
				expected_classifier,
				actual_classifier,
				&expected_classifier
			) != 0)) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				expectation->name_symbol_id,
				-1,
				def->ast,
				expectation->type_span
			);
			continue;
		}
		int free_effect_row = operation_classifier_contains_unowned_effect_row(
			ctx, def->compiled_operation, actual_classifier
		);
		if (free_effect_row < 0) {
			return -1;
		}
		if (free_effect_row != 0) {
			/* The expectation has been checked, but its external effect-row
			 * equation remains a residual artifact obligation. Do not expose that
			 * open solver state as a closed JudgementDB certificate. */
			def->compiled_classifier = actual_classifier;
			continue;
		}
		struct prototype_term_conversion_result closed_conversion =
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				expected_classifier,
				actual_classifier
			);
		if (closed_conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL &&
			!structurally_checked && !prototype_judgement_classifier_compatible(
				ctx->terms,
				ctx->type_declarations,
				expected_classifier,
				actual_classifier
			)) {
			/* Compatibility may depend on transparent imported definitions. The
			 * source expectation is valid in that environment, but the standalone
			 * kernel certificate has no definition premise with which to replay it. */
			def->compiled_classifier = actual_classifier;
			continue;
			}
		if (structurally_checked &&
			closed_conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			/* `::` validates a source definition; it does not replace the
			 * synthesized dependent motive by a non-convertible annotation.
			 * Keep the operation classifier as the reusable compiler result. */
			def->compiled_classifier = actual_classifier;
			continue;
		}
			if (actual_from_operation) {
				struct prototype_judgement_selected_evidence source_evidence;
				uint32_t conclusion_context = ctx->metadata->operations[
					def->compiled_operation
				].context_id;
				if (prototype_judgement_select_evidence(
						ctx->judgement,
						def->compiled_operation,
						conclusion_context,
						compiled_term,
						expected_classifier,
						&source_evidence
					) == 0) {
					def->compiled_classifier = expected_classifier;
					continue;
				}
				uint32_t source_operation = def->compiled_operation;
				const struct prototype_operation_node* compiled_operation =
					&ctx->metadata->operations[source_operation];
				if (compiled_operation->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
					/* The direct body Operation supplied actual_classifier above. Nested
					 * annotations retain distinct evidence and must not be stripped to an
					 * untyped VAR or CONSTRUCTOR occurrence. */
					source_operation = compiled_operation->body;
				}
				if (source_operation >= ctx->metadata->operation_count) {
					return -1;
				}
				uint32_t source_context =
					ctx->metadata->operations[source_operation].context_id;
				if (prototype_judgement_select_evidence(
						ctx->judgement,
						source_operation,
						source_context,
						compiled_term,
						actual_classifier,
						&source_evidence
					) != 0) {
					return -1;
				}
				if (prototype_judgement_add_expected_type_exposure(
						ctx->judgement,
						ctx->terms,
						ctx->type_declarations,
						source_context,
						def->compiled_operation,
						&source_evidence,
						expected_classifier,
						compiled_term
					) != 0) {
				return -1;
			}
			def->compiled_classifier = expected_classifier;
			continue;
			}
			struct prototype_judgement_selected_evidence source_evidence;
			if (prototype_judgement_select_evidence(
					ctx->judgement,
					PROTOTYPE_INVALID_ID,
					0,
					compiled_term,
					actual_classifier,
					&source_evidence
				) != 0 || prototype_judgement_add_expected_type_exposure(
					ctx->judgement,
					ctx->terms,
					ctx->type_declarations,
					0,
					def->compiled_operation,
					&source_evidence,
					expected_classifier,
					compiled_term
				) != 0) {
				return -1;
			}
			/*
			 * A shared computation graph may have several classifiers. The expectation
		 * does not synthesize a classifier here; it selects the already inferred
		 * classifier that this exported name promises to expose.
		 */
		def->compiled_classifier = actual_classifier;
	}
	return 0;
}

static int compile_phase_report_def_index_errors(struct compile_context* ctx) {
	if (!ctx || !ctx->asts) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)ctx->asts->def_index_capacity; ++i) {
		const struct prototype_ast_def_open_address_entry* entry = &ctx->asts->def_index[i];
		if (!entry->occupied) {
			continue;
		}
		uint32_t declaration_count = 0;
		uint32_t scan_id = entry->first_expectation;
		while (scan_id != PROTOTYPE_INVALID_ID && scan_id < ctx->asts->expectation_count) {
			const struct prototype_ast_type_expectation_def* type_entry =
				&ctx->asts->expectations[scan_id];
			if (type_entry->kind == PROTOTYPE_AST_TYPE_ENTRY_DECLARATION) {
				declaration_count++;
			}
			scan_id = type_entry->next_for_symbol;
		}
		if (declaration_count + entry->assignment_count > 1) {
			uint32_t expectation_id = entry->first_expectation;
			while (expectation_id != PROTOTYPE_INVALID_ID && expectation_id < ctx->asts->expectation_count) {
				const struct prototype_ast_type_expectation_def* expectation =
					&ctx->asts->expectations[expectation_id];
				if (expectation->kind == PROTOTYPE_AST_TYPE_ENTRY_DECLARATION) {
					(void)add_resolve_error_at_span(
						ctx,
						PROTOTYPE_RESOLVE_ERROR_DUPLICATE_DEFINITION,
						entry->symbol_id,
						-1,
						PROTOTYPE_INVALID_ID,
						expectation->name_span
					);
				}
				expectation_id = expectation->next_for_symbol;
			}
			uint32_t assignment_id = entry->first_assignment;
			while (assignment_id != PROTOTYPE_INVALID_ID && assignment_id < ctx->asts->assignment_count) {
				const struct prototype_ast_term_assignment_def* assignment =
					&ctx->asts->assignments[assignment_id];
				(void)add_resolve_error_at_span(
					ctx,
					PROTOTYPE_RESOLVE_ERROR_DUPLICATE_DEFINITION,
					entry->symbol_id,
					-1,
					assignment->ast,
					assignment->name_span
				);
				assignment_id = assignment->next_for_symbol;
			}
		}
	}
	return 0;
}

static int compile_phase_publish_labels(struct compile_context* ctx) {
	if (!ctx || !ctx->asts || !ctx->terms) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)ctx->asts->assignment_count; ++i) {
		struct prototype_ast_term_assignment_def* def = &ctx->asts->assignments[i];
		if (!def->compiled || def->published) {
			continue;
		}
		if (def->compiled_classifier == PROTOTYPE_INVALID_ID &&
			def->compiled_operation < ctx->metadata->operation_count) {
			def->compiled_classifier = ctx->metadata->operations[
				def->compiled_operation
			].classifier;
		}
		if (add_compile_label(
				ctx,
				def->name_symbol_id,
				def->compiled_term,
				def->compiled_classifier,
				def->compiled_operation
			) != 0) {
			return -1;
		}
		def->published = 1;
	}
	if (ctx->asts->root_definition_select != PROTOTYPE_INVALID_ID) {
		const struct prototype_ast_node* select;
		struct prototype_ast_term_assignment_def* selected;
		if (compile_selected_definition(ctx, &select, &selected) != 0) {
			return -1;
		}
		(void)select;
		if (!selected || !selected->compiled ||
			selected->compiled_operation >= ctx->metadata->operation_count ||
			ctx->metadata->operations[selected->compiled_operation].category !=
				PROTOTYPE_TERM_CATEGORY_VALUE) {
			return -1;
		}
		if (ctx->metadata->selected_entry_symbol_id != selected->name_symbol_id ||
			ctx->metadata->selected_entry_term >= ctx->terms->term_count ||
			ctx->metadata->selected_entry_operation >=
				ctx->metadata->operation_count) {
			return -1;
		}
		ctx->metadata->selected_entry_classifier = ctx->metadata->operations[
			ctx->metadata->selected_entry_operation
		].classifier;
		if (ctx->metadata->selected_entry_classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	return 0;
}

static void compile_metadata_refresh_runtime_capabilities(
	struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms
) {
	if (!metadata || !terms) {
		return;
	}
	uint64_t capabilities = 0;
	struct prototype_verification_coverage coverage;
	if (prototype_verification_db_coverage(
			&metadata->verification, &coverage
		) == 0) {
		capabilities |= coverage.required_runtime_capabilities;
	}
	for (size_t i = 0; i < metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation = &metadata->operations[i];
		if (operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD &&
			operation->core_term < terms->term_count &&
			terms->terms[operation->core_term].tag == PROTOTYPE_TERM_COMPUTATION_FOLD &&
			terms->terms[operation->core_term].as.computation_fold.clause_count != 0) {
			capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_HANDLER;
		}
		if (operation->tag != PROTOTYPE_OPERATION_REQUEST ||
			operation->core_term >= terms->term_count ||
			terms->terms[operation->core_term].tag != PROTOTYPE_TERM_OPERATION_REQUEST) {
			continue;
		}
		capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_OPERATION_DISPATCH;
		uint32_t head = terms->terms[operation->core_term].as.operation_request.operation;
		while (head < terms->term_count && terms->terms[head].tag == PROTOTYPE_TERM_APP) {
			head = terms->terms[head].as.app.function;
		}
		if (head < terms->term_count &&
			terms->terms[head].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
			const struct prototype_effect_operation_declaration* declaration =
				prototype_term_effect_operation_declaration(
					terms->terms[head].as.effect_operation.operation_id
				);
			if (declaration &&
				(declaration->required_host_effects &
					PROTOTYPE_HOST_EFFECT_TERMINAL) != 0) {
				capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_TERMINAL;
			}
		}
	}
	metadata->required_runtime_capabilities = capabilities;
}

static uint32_t max_existing_universe_level_var(const struct prototype_term_db* terms) {
	uint32_t max_level_var = 0;
	if (!terms) {
		return 0;
	}
	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
			terms->terms[i].as.universe_var.level_var >= max_level_var) {
			max_level_var = terms->terms[i].as.universe_var.level_var + 1;
		}
	}
	return max_level_var;
}

static void sync_universe_level_counters(struct compile_context* ctx) {
	if (!ctx || !ctx->terms) {
		return;
	}
	uint32_t next_level_var = max_existing_universe_level_var(ctx->terms);
	if (ctx->asts && ctx->asts->next_ast_level_var < next_level_var) {
		ctx->asts->next_ast_level_var = next_level_var;
	}
	if (ctx->judgement && ctx->judgement->next_universe_var < next_level_var) {
		ctx->judgement->next_universe_var = next_level_var;
	}
	if (ctx->type_declarations && ctx->type_declarations->next_level_var < next_level_var) {
		ctx->type_declarations->next_level_var = next_level_var;
	}
}

static int compile_pending_with_workspace(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct compile_judgement_workspace* workspace
) {
	if (!asts || !terms || !type_declarations || !judgement ||
		!intrinsic_environment || !workspace) {
		return -1;
	}
	struct compile_context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.intrinsic_environment = intrinsic_environment;
	ctx.asts = asts;
	ctx.terms = terms;
	ctx.type_declarations = type_declarations;
	ctx.judgement = judgement;
	ctx.operation_usage_entries = workspace->operation_usage_entries;
	ctx.operation_usage_entry_capacity = PROTOTYPE_OPERATION_USAGE_ENTRY_CAPACITY;
	prototype_judgement_delta_init(
		&ctx.judgement_delta,
		judgement,
		workspace->propositions,
		workspace->derivation_candidates,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
		workspace->candidate_premises,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY *
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		workspace->match_motive_results,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
		workspace->computation_constraints,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY,
		workspace->effect_row_constraints,
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY
	);
	prototype_judgement_delta_set_intrinsic_environment(
		&ctx.judgement_delta, intrinsic_environment
	);
	ctx.metadata = metadata;
	if (!metadata || prototype_context_empty(&metadata->contexts) == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	prototype_judgement_delta_set_context_store(
		&ctx.judgement_delta,
		&metadata->contexts,
		&metadata->substitutions
	);
	ctx.context_ids[0] = prototype_context_empty(&metadata->contexts);
	if (metadata) {
		prototype_judgement_delta_set_solver_budget(
			&ctx.judgement_delta,
			metadata->solver_step_limit,
			&metadata->solver_steps_used,
			&metadata->solver_exhausted
		);
	}
	ctx.namespace_symbol_id = namespace_symbol_id;
	ctx.imported_interfaces = imported_interfaces;
	ctx.imported_interface_count = imported_interface_count;
	sync_universe_level_counters(&ctx);

	if (compile_phase_report_def_index_errors(&ctx) != 0) {
		return -1;
	}
	if (compile_phase_build_graph(&ctx) != 0) {
		fprintf(stderr, "compile pending failed: build graph\n");
		return -1;
	}
	prototype_judgement_delta_set_operation_store(
		&ctx.judgement_delta,
		metadata->operations,
		metadata->operation_count,
		metadata->operation_cases,
		metadata->operation_case_count
	);
	prototype_judgement_delta_set_operation_usage_provider(
		&ctx.judgement_delta, &ctx, operation_solver_proposition_usage
	);
	/* Match resolution and imported-classifier inference may emit candidates
	 * before the main constraint-generation pass. Resource usage is part of
	 * Proposition identity, so its graph solution must exist before either
	 * phase can publish a candidate. */
	if (operation_solver_refresh_usage_solutions(&ctx) != 0) {
		return -1;
	}
	if (validate_constructor_occurrence_saturation(&ctx) != 0) {
		return -1;
	}
	if (begin_resolution_iteration(&ctx, 0, count_unresolved_resolution_items(&ctx)) != 0) {
		return -1;
	}
	if (compile_phase_infer_imported_constructor_classifiers(&ctx) != 0) {
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 ||
		prototype_internal_canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (compile_phase_resolve_pending_match_items(&ctx) < 0) {
		return -1;
	}
	if (finish_resolution_iteration(&ctx, count_unresolved_resolution_items(&ctx)) != 0) {
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 ||
		prototype_internal_canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (compile_phase_infer_pending_types(&ctx) != 0) {
		fprintf(stderr, "compile pending failed: infer pending types\n");
		return -1;
	}
	/* The fixed point above has already solved and committed source-operation
	 * computation constraints in their contexts. Rebuilding constraints from
	 * TermDB here would erase occurrence contexts and create false closed
	 * derivations for shared core terms. */
	if (operation_effect_generate_constraints(&ctx) != 0 ||
			prototype_judgement_delta_commit(&ctx.judgement_delta, 0) != 0 ||
			compile_phase_record_residual_dependent_folds(&ctx) != 0 ||
			compile_phase_record_residual_computation_fold_results(&ctx) != 0) {
		fprintf(stderr, "compile pending failed: effect/commit/residual\n");
		return -1;
	}
	operation_solver_refresh_constraint_states(&ctx, ctx.metadata->solver_exhausted);
	compile_metadata_refresh_runtime_capabilities(metadata, terms);
	if (metadata && metadata->compile_policy == PROTOTYPE_COMPILE_POLICY_STRICT &&
		(prototype_verification_db_count(&metadata->verification) != 0 ||
			operation_effect_unresolved_count(metadata) != 0)) {
		return -1;
	}
	if (compile_phase_check_ascriptions(&ctx) != 0) {
		return -1;
	}
	if (compile_phase_check_expectations(&ctx) != 0) {
		return -1;
	}
	if (prototype_judgement_delta_has_pending_classifier_state(&ctx.judgement_delta) != 0) {
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 ||
		prototype_internal_canonicalize_constructor_owner_refs(terms, type_declarations, 0) != 0) {
		return -1;
	}
	if (prototype_judgement_expand_primitives(judgement, terms) != 0) {
		return -1;
	}
	struct prototype_operation_graph operation_graph;
	prototype_compile_metadata_operation_graph(metadata, &operation_graph);
	if (prototype_judgement_add_normalization_premise_conversions(
			terms,
			type_declarations,
			&operation_graph,
			judgement
		) != 0) {
		return -1;
	}
	if (prototype_judgement_publish_candidates(
			&operation_graph,
			judgement
		) != 0) {
		fprintf(stderr, "P0 proof candidate publication failed\n");
		return -1;
	}
	if (prototype_judgement_validate_accepted_graph(
			terms,
			type_declarations,
			intrinsic_environment,
			&metadata->contexts,
			&metadata->substitutions,
			&operation_graph,
			judgement
		) != 0) {
		fprintf(stderr, "P0 accepted proof graph validation failed\n");
		return -1;
	}
	if (prototype_operation_graph_validate(
			&operation_graph, terms, &metadata->contexts
		) != 0 ||
		prototype_constructor_curried_caches_validate(
			type_declarations, &metadata->contexts, terms
		) != 0) {
		return -1;
	}
	if (prototype_term_erase_constructor_view_owners(terms) != 0) {
		return -1;
	}
	if (compile_phase_publish_labels(&ctx) != 0) {
		return -1;
	}
	if (ctx.had_error) {
		return -1;
	}
	return 0;
}

int prototype_ast_compile_pending_with_imports(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
) {
	struct compile_judgement_workspace* workspace = calloc(1, sizeof(*workspace));
	if (!workspace) {
		return -1;
	}
	int status = compile_pending_with_workspace(
		asts,
		terms,
		type_declarations,
		judgement,
		metadata,
		intrinsic_environment,
		namespace_symbol_id,
		imported_interfaces,
		imported_interface_count,
		workspace
	);
	free(workspace);
	return status;
}

int prototype_ast_compile_pending(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata
) {
	return prototype_ast_compile_pending_with_imports(
		asts,
		terms,
		type_declarations,
		judgement,
		metadata,
		prototype_default_intrinsic_environment(),
		-1,
		NULL,
		0
	);
}
