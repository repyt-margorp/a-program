static int compile_phase_infer_pending_types(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}

	for (;;) {
		size_t before_operation_classifiers = count_classified_operations(ctx);
		uint64_t before_classifier_revision =
			ctx->typing->constraints.semantic_transition_count;
		uint64_t before_context_projection_revision =
			ctx->typing->context_projections.solution_revision;
		uint64_t before_context_classifier_revision =
			ctx->metadata->contexts.semantic_revision;
		size_t before_unresolved = count_unresolved_resolution_items(ctx);
		size_t before_relations = ctx->judgement_delta.proposition_count;
		int match_resolution_status = compile_phase_resolve_pending_match_items(ctx);
		if (match_resolution_status < 0) {
			fprintf(stderr,
				"typing fixed point: pending Match resolution failed\n"
			);
			return -1;
		}
		if (operation_solver_resolve_contexts(ctx, 0) != 0) {
			fprintf(stderr,
				"typing fixed point: pre-inference context resolution failed\n"
			);
			return -1;
		}
			int status = compile_phase_infer_general_classifiers(ctx, 0);
			if (status != 0) {
				if (status < 0) {
					fprintf(stderr,
						"typing fixed point: classifier inference failed status=%d\n",
						status);
				}
				return status;
			}
			if (operation_solver_resolve_contexts(ctx, 0) != 0) {
				fprintf(stderr,
					"typing fixed point: post-inference context resolution failed\n"
				);
				return -1;
			}
		int cbpv_changed = 0;
		if (operation_solver_generate_computation_constraints(ctx) != 0) {
			fprintf(stderr, "typing fixed point: computation constraint generation failed\n");
			return -1;
		}
		if (operation_solver_prepare_computation_constraint_inputs(
				ctx,
				ctx->computation_constraint_inputs,
				ctx->computation_constraint_input_capacity
			) != 0) {
			fprintf(stderr, "typing fixed point: computation operand refresh failed\n");
			return -1;
		}
		int request_changed = 0;
		if (prototype_judgement_delta_solve_recorded_computation_requests(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				ctx->computation_constraint_inputs,
				ctx->computation_constraint_input_capacity,
				ctx->computation_constraint_results,
				ctx->computation_constraint_result_capacity
			) != 0) {
			fprintf(stderr,
				"typing fixed point: recorded operation request solving failed\n");
			return -1;
		}
		if (operation_solver_bind_computation_constraint_solutions(
				ctx, &request_changed, 1
			) != 0) {
			fprintf(stderr,
				"typing fixed point: operation request solution binding failed\n");
			return -1;
		}
		if (operation_solver_prepare_computation_constraint_inputs(
				ctx,
				ctx->computation_constraint_inputs,
				ctx->computation_constraint_input_capacity
			) != 0) {
			fprintf(stderr,
				"typing fixed point: operation request operand refresh failed\n");
			return -1;
		}
		if (request_changed) {
			cbpv_changed = 1;
			continue;
		}
		if (prototype_judgement_delta_solve_recorded_computation_constraints(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				ctx->computation_constraint_inputs,
				ctx->computation_constraint_input_capacity,
				ctx->computation_constraint_results,
				ctx->computation_constraint_result_capacity
			) != 0) {
			fprintf(stderr, "typing fixed point: computation constraint solving failed\n");
			return -1;
		}
		if (operation_solver_bind_computation_constraint_solutions(
				ctx, &cbpv_changed, 1
			) != 0) {
			fprintf(stderr, "typing fixed point: computation solution binding failed\n");
			return -1;
		}
		if (operation_effect_generate_constraints(ctx, 0) != 0) {
			fprintf(stderr, "typing fixed point: effect constraint generation failed\n");
			return -1;
		}
		if (count_classified_operations(ctx) == before_operation_classifiers &&
			ctx->typing->constraints.semantic_transition_count ==
				before_classifier_revision &&
			ctx->typing->context_projections.solution_revision ==
				before_context_projection_revision &&
			ctx->metadata->contexts.semantic_revision ==
				before_context_classifier_revision &&
			count_unresolved_resolution_items(ctx) == before_unresolved &&
			ctx->judgement_delta.proposition_count == before_relations && !cbpv_changed) {
			break;
		}
	}

	int match_resolution_status = compile_phase_resolve_pending_match_items(ctx);
	if (match_resolution_status < 0) {
		fprintf(stderr,
			"typing fixed point: final pending Match resolution failed\n"
		);
		return -1;
	}
	int status = compile_phase_infer_general_classifiers(ctx, 1);
	if (status != 0) {
		if (status < 0) {
			fprintf(stderr, "typing fixed point: final classifier inference failed\n");
		}
		return status;
	}
	if (operation_solver_resolve_contexts(ctx, 0) != 0) {
		fprintf(stderr, "typing fixed point: final context resolution failed\n");
		return -1;
	}
	/* Refresh the Layer T candidate-to-materialized projection before constructing
	 * dependent branch pullbacks. Candidate Context identity remains stable until
	 * classifier inference and motive rebuilding have both closed. */
	if (operation_solver_resolve_contexts(ctx, 0) != 0) {
		fprintf(stderr, "typing fixed point: pre-motive context resolution failed\n");
		return -1;
	}
	/* Indexed pullbacks change the Context in which motive equations live.
	 * Reclose the same classifier fixed point in that final Context before any
	 * classifier is projected into a frozen occurrence or a proof Claim. */
	status = compile_phase_infer_general_classifiers(ctx, 1);
	if (status != 0) {
		if (status < 0) {
			fprintf(stderr,
				"typing fixed point: refined classifier inference failed status=%d "
				"steps=%" PRIu64 " limit=%" PRIu64 " exhausted=%u\n",
				status, ctx->metadata->effort.used,
				ctx->metadata->effort.limit, ctx->metadata->effort.exhausted);
		}
		return status;
	}
	/* Indexed motive refinement can change a Lambda binder classifier. Project the
	 * now-closed Layer T occurrence/proof view exactly once before materializing
	 * accepted evidence; the immutable constraint topology keeps candidate IDs. */
	if (operation_solver_resolve_contexts(ctx, 1) != 0) {
		fprintf(stderr, "typing fixed point: refined context resolution failed\n");
		return -1;
	}
	if (operation_solver_refresh_usage_solutions(ctx) != 0) {
		fprintf(stderr, "typing fixed point: refined branch usage solving failed\n");
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
	size_t remaining_resolution_items = count_unresolved_resolution_items(ctx);
	if (remaining_resolution_items != 0) {
		fprintf(stderr, "typing fixed point: unresolved resolution items=%zu\n",
			remaining_resolution_items);
	}
	return remaining_resolution_items == 0 ? 0 : -1;
}

static int operation_check_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t expected_classifier,
	uint32_t depth
) {
	if (!ctx || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		expected_classifier >= ctx->terms->term_count || depth > 128) {
		fprintf(stderr,
			"classifier check: invalid input operation=%u occurrence-count=%zu expected=%u "
			"term-count=%zu depth=%u\n",
			operation_id,
			ctx && ctx->metadata ? ctx->metadata->typed_occurrences.occurrence_count : 0,
			expected_classifier, ctx && ctx->terms ? ctx->terms->term_count : 0, depth);
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL && operation_id >= 1000) {
		const struct prototype_typing_binder_owner* trace_owner =
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR ?
			operation_binder_owner_find(
				ctx, operation->binding_id, operation->context_id
			) : NULL;
		fprintf(
			stderr,
			"expected-refinement operation=%u tag=%d expected=%u depth=%u owner=%u "
			"case=%u field=%u binder-match=%u/%u/%u\n",
			operation_id, operation->tag, expected_classifier, depth,
			operation->ih_owner_occurrence, operation->ih_case_index,
			operation->ih_field_index,
			trace_owner ? trace_owner->match_occurrence : PROTOTYPE_INVALID_ID,
			trace_owner ? trace_owner->match_case_index : PROTOTYPE_INVALID_ID,
			trace_owner ? trace_owner->match_field_index : PROTOTYPE_INVALID_ID
		);
	}
	/* Nested surface lambdas are represented by a direct TypedOccurrenceGraph edge,
	 * while their enclosing Pi codomain records the implicit
	 * RETURN(THUNK(lambda)) boundary. Expose the raw Pi before checking the
	 * lambda occurrence itself. */
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
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
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
		operation->wrapped_occurrence < ctx->metadata->typed_occurrences.occurrence_count) {
		return operation_check_classifier(
			ctx, operation->wrapped_occurrence, expected_classifier, depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
		operation->wrapped_occurrence < ctx->metadata->typed_occurrences.occurrence_count) {
		return operation_check_classifier(
			ctx, operation->wrapped_occurrence, expected_classifier, depth + 1
		);
	}
	uint32_t actual_classifier = operation_solver_classifier(ctx, operation_id);
	if (actual_classifier == PROTOTYPE_INVALID_ID) {
		actual_classifier = operation_available_classifier(ctx, operation_id);
	}
	if (operation->context_action_substitution != PROTOTYPE_INVALID_ID &&
		operation->source_classifier < ctx->terms->term_count) {
		/* A pulled-back occurrence is typed by reindexing its source evidence.
		 * Solver cells may still contain the source principal while a dependent
		 * branch is being rebuilt, so the explicit Context action is authoritative
		 * for occurrence-level checking. */
		if (prototype_term_reindex(
				ctx->terms, ctx->type_declarations,
				&ctx->metadata->contexts, &ctx->metadata->substitutions,
				operation->source_classifier,
				operation->context_action_substitution,
				&actual_classifier
			) != 0) {
			return -1;
		}
	}
	uint32_t child_occurrence;
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK &&
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0,
			&child_occurrence
		) == 0 &&
		ctx->terms->terms[expected_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		return operation_check_classifier(
			ctx,
			child_occurrence,
			ctx->terms->terms[expected_classifier].as.thunk_type.computation,
			depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0,
			&child_occurrence
		) == 0 &&
		ctx->terms->terms[expected_classifier].tag ==
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return operation_check_classifier(
			ctx,
			child_occurrence,
			ctx->terms->terms[expected_classifier].as.computation_type.result,
			depth + 1
		);
	}
	uint32_t body_occurrence;
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_BODY, 0, &body_occurrence
		) == 0 &&
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
		uint32_t binder_classifier = operation_solver_binder_classifier(
			ctx, operation_id
		);
		int domain_equal = pi_status == 0 && family_status == 0 &&
			binder_classifier < ctx->terms->term_count &&
			(prototype_judgement_classifier_reference_equal(
				ctx->terms, ctx->type_declarations,
				expected_domain, binder_classifier
			) || (prototype_judgement_solve_expected_effect_rows(
					ctx->terms, ctx->type_declarations, NULL,
					expected_domain, binder_classifier,
					&solved_expected_domain
				) == 0 && prototype_judgement_classifier_reference_equal(
					ctx->terms, ctx->type_declarations,
					solved_expected_domain, binder_classifier
				)) || prototype_judgement_classifier_compatible(
					ctx->terms, ctx->type_declarations,
					expected_domain, binder_classifier
				));
		if (pi_status != 0 || family_status != 0 || !domain_equal ||
			operation->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				ctx->terms, operation->binding_id,
				&body_binder_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, expected_body,
				expected_binder, body_binder_var, &specialized_body
			) != 0) {
			return 0;
		}
		return operation_check_classifier(
			ctx, body_occurrence, specialized_body, depth + 1
		);
	}
	uint32_t scrutinee_occurrence;
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_occurrence
		) == 0 &&
		operation->core_term < ctx->terms->term_count &&
		ctx->terms->terms[operation->core_term].tag == PROTOTYPE_TERM_MATCH &&
		operation->first_case + operation->case_count <=
			ctx->metadata->typed_occurrences.case_count) {
		const struct prototype_typed_occurrence* scrutinee =
			&ctx->metadata->typed_occurrences.occurrences[scrutinee_occurrence];
		const struct prototype_term match_value =
			ctx->terms->terms[operation->core_term];
		const struct prototype_term* match = &match_value;
		uint32_t scrutinee_classifier = operation_solver_classifier(
			ctx, scrutinee_occurrence
		);
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
			scrutinee_classifier = operation_available_classifier(
				ctx, scrutinee_occurrence
			);
		}
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID &&
			scrutinee->core_term < ctx->terms->term_count &&
			ctx->terms->terms[scrutinee->core_term].tag == PROTOTYPE_TERM_VAR) {
			uint32_t binding_context;
			if (prototype_context_find_binding(
					&ctx->metadata->contexts, scrutinee->context_id,
					ctx->terms->terms[scrutinee->core_term].as.var.binding_id,
					&binding_context
				) == 0) {
				scrutinee_classifier = operation_context_classifier_term(
					ctx, binding_context
				);
			}
		}
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		for (uint32_t i = 0; i < operation->case_count; ++i) {
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[operation->first_case + i];
			uint32_t branch_occurrence;
			uint32_t term_case_id = match->as.match.first_case + i;
			if (term_case_id >= ctx->terms->case_count ||
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
					&branch_occurrence
				) != 0) {
				fprintf(stderr,
					"classifier check: invalid match case edge match=%u case=%u term-case=%u\n",
					operation_id, i, term_case_id);
				return -1;
			}
			const struct prototype_match_case* term_case =
				&ctx->terms->cases[term_case_id];
			if (term_case->binder_count > 64 || term_case->binder_count !=
					operation_case->binder_count) {
				fprintf(stderr,
					"classifier check: match binder mismatch match=%u case=%u term=%u occurrence=%u\n",
					operation_id, i, term_case->binder_count,
					operation_case->binder_count);
				return -1;
			}
			if (operation_case->refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
				continue;
			}
			int pending_pullback = 0;
			int constant_refinement =
				operation_case->refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT;
			uint32_t refinement_substitution = PROTOTYPE_INVALID_ID;
			if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
				refinement_substitution = operation_case->refinement_substitution;
			} else if (!constant_refinement) {
				struct prototype_typing_constraint* refinement =
					operation_solver_branch_refinement_constraint(
						ctx, operation->first_case + i
					);
				const struct prototype_typing_constraint_solution* solution = refinement ?
					&ctx->typing->constraints.solutions[refinement->id] : NULL;
				int refinement_status = solution ?
					solution->branch_refinement.refinement_status :
					PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
				if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
					continue;
				}
				if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
					refinement_substitution =
						solution->branch_refinement.substitution;
					pending_pullback = 1;
				} else if (refinement_status ==
						PROTOTYPE_INDEX_REFINEMENT_CONSTANT) {
					constant_refinement = 1;
				} else {
					if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
						fprintf(stderr,
							"classifier check: branch refinement unavailable match=%u "
							"case=%u status=%d scrutinee-classifier=%u\n",
							operation_id, i, refinement_status,
							scrutinee_classifier);
					}
					return refinement_status == PROTOTYPE_INDEX_REFINEMENT_INVALID ?
						-1 : 0;
				}
			}
			if (!constant_refinement) {
				const struct prototype_substitution* stored =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						refinement_substitution
					);
				int case_is_solved =
					prototype_typed_occurrence_match_case_is_solved(operation_case);
				uint32_t materialized_case_context = case_is_solved ?
					operation_case_materialized_context(
						ctx, operation->first_case + i
					) : operation_case_input_context(
						ctx, operation->first_case + i
					);
				int action_matches = stored &&
					materialized_case_context != PROTOTYPE_INVALID_ID &&
					(case_is_solved ?
						stored->source_context == materialized_case_context :
						stored->target_context == materialized_case_context);
				if (!action_matches) {
					fprintf(stderr,
						"classifier check: stale branch refinement match=%u case=%u substitution=%u "
						"source=%u target=%u case-context=%u materialized-input=%u pending=%d\n",
						operation_id, i, refinement_substitution,
						stored ? stored->source_context : PROTOTYPE_INVALID_ID,
						stored ? stored->target_context : PROTOTYPE_INVALID_ID,
						operation_case->context_id, materialized_case_context,
						pending_pullback);
					return -1;
				}
			}
			uint32_t refined_expected = expected_classifier;
			uint32_t branch_classifier = operation_solver_classifier(
				ctx, branch_occurrence
			);
			if (branch_classifier == PROTOTYPE_INVALID_ID) {
				branch_classifier = operation_available_classifier(
					ctx, branch_occurrence
				);
			}
			uint32_t refined_actual;
			uint32_t solved_refined_expected;
			if (branch_classifier >= ctx->terms->term_count ||
				(!constant_refinement && prototype_term_reindex(
					ctx->terms, ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					expected_classifier, refinement_substitution,
					&refined_expected
				 ) != 0)) {
				return 0;
			}
			if (pending_pullback &&
				operation_solver_reindex_in_enclosing_indexed_branches(
					ctx, branch_occurrence, operation_case->context_id,
					branch_classifier, &refined_actual
				) != 0) {
				return 0;
			}
			if (!pending_pullback) {
				refined_actual = branch_classifier;
				/* A solved branch has already been pulled into its refined
				 * Context. Validate the source occurrence structure there before
				 * comparing its provisional classifier. In particular, RETURN and
				 * REFERENCE edges carry the evidence that a branch result was
				 * reindexed; their cached outer classifier need not be rebuilt yet. */
				int structural_status = operation_check_classifier(
					ctx, branch_occurrence, refined_expected, depth + 1
				);
				if (structural_status < 0) {
					return -1;
				}
				if (structural_status > 0) {
					continue;
				}
			}
			solved_refined_expected = refined_expected;
			int effect_status = prototype_judgement_solve_expected_effect_rows(
					ctx->terms, ctx->type_declarations, NULL,
					refined_expected, refined_actual,
					&solved_refined_expected
				);
			struct prototype_term_conversion_result branch_conversion =
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					solved_refined_expected, refined_actual
				);
			if (effect_status != 0 ||
				(branch_conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL &&
					!prototype_judgement_classifier_compatible(
						ctx->terms, ctx->type_declarations,
						solved_refined_expected, refined_actual
					))) {
				return 0;
			}
		}
		uint32_t scrutinee_type_id;
		uint32_t scrutinee_arguments[32];
		uint32_t scrutinee_argument_count;
		if (prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, scrutinee_classifier,
				&scrutinee_type_id, scrutinee_arguments,
				&scrutinee_argument_count
			) == 0 && scrutinee_type_id < ctx->type_declarations->semantic_schema.type_count) {
			const struct prototype_type_declaration* declaration =
				&ctx->type_declarations->semantic_schema.type_declarations[scrutinee_type_id];
			for (uint32_t constructor_index = 0;
				constructor_index < declaration->constructor_count;
				++constructor_index) {
				int covered = 0;
				for (uint32_t case_index = 0;
					case_index < operation->case_count;
					++case_index) {
					if (ctx->metadata->typed_occurrences.cases[
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
					&ctx->type_declarations->semantic_schema.constructor_declarations[
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
					fprintf(stderr,
						"classifier check: omitted constructor context path failed match=%u "
						"constructor=%u parameter-context=%u field-context=%u\n",
						operation_id, constructor_index, constructor->parameter_context,
						constructor->field_context);
					return -1;
				}
					struct prototype_case_binder binders[64];
					uint32_t synthetic_context = scrutinee->context_id;
					uint32_t owner;
					struct prototype_context_classifier_view context_view =
						operation_type_context_view(ctx);
				if (prototype_type_projection_instance_make(
						ctx->terms, ctx->type_declarations,
						scrutinee_type_id, scrutinee_arguments,
						declaration->parameter_count, &owner
					) != 0) {
					fprintf(stderr,
						"classifier check: omitted constructor owner failed match=%u "
						"constructor=%u type=%u arguments=%u parameters=%u\n",
						operation_id, constructor_index, scrutinee_type_id,
						scrutinee_argument_count, declaration->parameter_count);
					return -1;
				}
				for (uint32_t field = 0; field < field_count; ++field) {
					binders[field].binding_id = prototype_term_new_binding(ctx->terms);
					binders[field].is_recursive = 0;
					uint32_t field_classifier;
					if (binders[field].binding_id == PROTOTYPE_INVALID_ID ||
						prototype_judgement_constructor_field_classifier_in_view(
							ctx->terms, ctx->type_declarations,
							&context_view,
							&ctx->metadata->substitutions,
							synthetic_context, owner, constructor_index,
							binders, field, field, &field_classifier
						) != 0 || prototype_context_extend(
							&ctx->metadata->contexts, synthetic_context,
							binders[field].binding_id, field_classifier,
							&synthetic_context
						) != 0) {
						fprintf(stderr,
							"classifier check: omitted constructor field failed match=%u "
							"constructor=%u field=%u synthetic-context=%u owner=%u\n",
							operation_id, constructor_index, field, synthetic_context, owner);
						return -1;
					}
				}
				uint32_t ignored_context;
				uint32_t ignored_substitution;
				uint32_t ignored_constructor;
				uint32_t ignored_residual_pattern;
				uint32_t ignored_residual_value;
				int omission_status =
					prototype_judgement_source_match_branch_refinement(
						&ctx->metadata->contexts,
						&ctx->metadata->substitutions,
						ctx->terms, ctx->type_declarations,
						scrutinee->context_id, scrutinee->core_term,
						scrutinee->binding_id,
						scrutinee_classifier,
						constructor_index, synthetic_context, binders,
						field_count, &ignored_context,
						&ignored_substitution, &ignored_constructor,
						&ignored_residual_pattern, &ignored_residual_value
					);
				if (omission_status != PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
					fprintf(stderr,
						"match coverage mismatch operation=%u constructor=%u status=%d\n",
						operation_id, constructor_index, omission_status);
					return omission_status == PROTOTYPE_INDEX_REFINEMENT_INVALID ? -1 : 0;
				}
			}
		}
		return 1;
	}
	return actual_classifier < ctx->terms->term_count &&
		(prototype_judgement_classifier_reference_equal(
			ctx->terms, ctx->type_declarations,
			expected_classifier, actual_classifier
		) || prototype_judgement_classifier_compatible(
			ctx->terms, ctx->type_declarations,
			expected_classifier, actual_classifier
		));
}

static int operation_refine_projected_classifier_from_expected(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t expected_classifier,
	uint32_t origin_constraint_id,
	uint32_t depth,
	int* p_changed
);

static int operation_match_binder_owner_projection(
	const struct compile_context* ctx,
	uint32_t typed_projection,
	const struct prototype_typing_binder_owner* owner,
	uint32_t* p_match_typed_projection
) {
	if (!ctx || !owner || !p_match_typed_projection ||
		typed_projection >=
			ctx->typing->context_projections.typed_projection_count ||
		owner->match_occurrence == PROTOTYPE_INVALID_ID ||
		owner->match_case_index == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	const struct prototype_context_projection* context_projection = projection ?
		prototype_context_projection_get(
			&ctx->typing->context_projections,
			projection->context_projection
		) : NULL;
	if (!context_projection) {
		return -1;
	}
	uint32_t action_id = context_projection->action_expression;
	for (size_t depth = 0;
		action_id != PROTOTYPE_INVALID_ID &&
			depth <= ctx->typing->context_projections.action_count;
		++depth) {
		const struct prototype_context_action_expression* action =
			prototype_context_action_expression_get(
				&ctx->typing->context_projections, action_id
			);
		if (!action) {
			return -1;
		}
		if (action->kind == PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT) {
			const struct prototype_typed_match_case_projection* match_case =
				prototype_typed_match_case_projection_get(
					&ctx->typing->context_projections,
					action->dependency_id
				);
			const struct prototype_typed_projection* match_projection = match_case ?
				prototype_typed_projection_get(
					&ctx->typing->context_projections,
					match_case->owner_typed_projection
				) : NULL;
			const struct prototype_typed_occurrence* match = match_projection &&
				match_projection->source_occurrence <
					ctx->metadata->typed_occurrences.occurrence_count ?
				&ctx->metadata->typed_occurrences.occurrences[
					match_projection->source_occurrence
				] : NULL;
			if (match_case && match &&
				match_projection->source_occurrence == owner->match_occurrence &&
				match->first_case != PROTOTYPE_INVALID_ID &&
				match_case->source_case ==
					match->first_case + owner->match_case_index) {
				*p_match_typed_projection = match_case->owner_typed_projection;
				return 0;
			}
		}
		if (action->kind == PROTOTYPE_CONTEXT_ACTION_IDENTITY) {
			return 1;
		}
		action_id = action->parent_action;
	}
	return action_id == PROTOTYPE_INVALID_ID ? 1 : -1;
}

static int operation_match_case_scrutinee_classifier_from_schema(
	struct compile_context* ctx,
	uint32_t match_projection,
	const struct prototype_typing_binder_owner* owner,
	uint32_t* p_classifier
) {
	if (!ctx || !owner || !p_classifier ||
		owner->match_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match =
		&ctx->metadata->typed_occurrences.occurrences[owner->match_occurrence];
	if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		owner->match_case_index >= match->case_count ||
		match->first_case == PROTOTYPE_INVALID_ID ||
		match->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[match->core_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	uint32_t operation_case_id = match->first_case + owner->match_case_index;
	uint32_t term_case_id = ctx->terms->terms[match->core_term].as.match.first_case +
		owner->match_case_index;
	if (operation_case_id >= ctx->metadata->typed_occurrences.case_count ||
		term_case_id >= ctx->terms->case_count) {
		return -1;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[operation_case_id];
	const struct prototype_match_case* term_case = &ctx->terms->cases[term_case_id];
	if (operation_case->binder_count != term_case->binder_count ||
		operation_case->binder_count > 64 ||
		owner->match_field_index >= operation_case->binder_count) {
		return -1;
	}
	uint32_t match_case_projection_id;
	int case_status = prototype_typed_match_case_projection_find(
		&ctx->typing->context_projections,
		operation_case_id,
		match_projection,
		&match_case_projection_id
	);
	if (case_status != 0) {
		return case_status;
	}
	const struct prototype_typed_match_case_projection* match_case_projection =
		prototype_typed_match_case_projection_get(
			&ctx->typing->context_projections, match_case_projection_id
		);
	if (!match_case_projection) {
		return -1;
	}
	int projection_status = operation_context_projection_solve(
		ctx, match_case_projection->context_projection, 0
	);
	if (projection_status != 0) {
		return projection_status;
	}
	const struct prototype_context_projection_solution* case_solution =
		prototype_context_projection_solution_get(
			&ctx->typing->context_projections,
			match_case_projection->context_projection
		);
	if (!case_solution || case_solution->state !=
			PROTOTYPE_CONTEXT_PROJECTION_SOLVED) {
		return 1;
	}
	uint32_t subject;
	if (prototype_term_constructor(
			ctx->terms,
			term_case->constructor_owner,
			term_case->constructor_id,
			&subject
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < operation_case->binder_count; ++i) {
		uint32_t field;
		if (prototype_term_var(
				ctx->terms, operation_case->binder_ids[i], &field
			) != 0 || prototype_term_app(
				ctx->terms, subject, field, &subject
			) != 0) {
			return -1;
		}
	}
	uint32_t field_classifiers[64];
	uint32_t field_count;
	if (operation_constructor_spine_expected_domains(
			ctx,
			case_solution->concrete_context,
			subject,
			term_case->constructor_owner,
			field_classifiers,
			64,
			&field_count
		) != 0 || field_count != operation_case->binder_count) {
		return -1;
	}
	int saturated = 0;
	if (operation_constructor_spine_result(
			ctx,
			case_solution->concrete_context,
			subject,
			term_case->constructor_owner,
			field_classifiers,
			field_count,
			p_classifier,
			&saturated
		) != 0) {
		return -1;
	}
	return saturated ? 0 : 1;
}

/* A Match field is a projection of its scrutinee constructor telescope.  When
 * a consumer constrains that field, reconstruct the corresponding scrutinee
 * family equation before refining the producer.  This is the inverse of the
 * ordinary field projection and remains indexed by the exact Layer T path. */
static int operation_refine_match_binder_from_expected(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t expected_classifier,
	uint32_t origin_constraint_id,
	uint32_t depth,
	int* p_changed
) {
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	const struct prototype_typed_occurrence* variable = projection &&
		projection->source_occurrence <
			ctx->metadata->typed_occurrences.occurrence_count ?
		&ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		] : NULL;
	if (!variable || variable->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
		variable->binding_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	const struct prototype_typing_binder_owner* owner =
		operation_binder_owner_find(
			ctx, variable->binding_id, variable->context_id
		);
	if (!owner || owner->match_occurrence == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t match_projection;
	int owner_status = operation_match_binder_owner_projection(
		ctx, typed_projection, owner, &match_projection
	);
	if (owner_status != 0) {
		return owner_status;
	}
	uint32_t scrutinee_projection;
	if (operation_solver_projected_child(
			ctx,
			match_projection,
			PROTOTYPE_TERM_CHILD_SCRUTINEE,
			0,
			&scrutinee_projection
		) != 0) {
		return -1;
	}
	const struct prototype_typed_projection* projected_scrutinee =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, scrutinee_projection
		);
	uint32_t scrutinee_occurrence = projected_scrutinee ?
		projected_scrutinee->source_occurrence : PROTOTYPE_INVALID_ID;
	while (scrutinee_occurrence <
		ctx->metadata->typed_occurrences.occurrence_count) {
		const struct prototype_typed_occurrence* wrapper =
			&ctx->metadata->typed_occurrences.occurrences[scrutinee_occurrence];
		if (wrapper->tag != PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
			wrapper->tag != PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			break;
		}
		scrutinee_occurrence = wrapper->wrapped_occurrence;
	}
	const struct prototype_typed_occurrence* scrutinee =
		scrutinee_occurrence <
			ctx->metadata->typed_occurrences.occurrence_count ?
		&ctx->metadata->typed_occurrences.occurrences[scrutinee_occurrence] : NULL;
	const struct prototype_typing_binder_owner* scrutinee_owner = scrutinee &&
		scrutinee->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
		scrutinee->binding_id != PROTOTYPE_INVALID_ID ?
		operation_binder_owner_find(
			ctx, scrutinee->binding_id, scrutinee->context_id
		) : NULL;
	uint32_t guarded_ih = PROTOTYPE_INVALID_ID;
	int guarded_status = scrutinee_owner &&
		scrutinee_owner->bound_computation_occurrence != PROTOTYPE_INVALID_ID ?
		operation_subgraph_unique_induction_hypothesis(
			ctx,
			scrutinee_owner->bound_computation_occurrence,
			0,
			&guarded_ih
		) : 1;
	if (guarded_status < 0) {
		return -1;
	}
	if (guarded_status != 0 || guarded_ih == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t field_classifier = operation_solver_classifier_for_projection(
		ctx, typed_projection
	);
	uint32_t scrutinee_classifier = operation_solver_classifier_for_projection(
		ctx, scrutinee_projection
	);
	if (field_classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
		int schema_status = operation_match_case_scrutinee_classifier_from_schema(
			ctx, match_projection, owner, &scrutinee_classifier
		);
		if (schema_status != 0) {
			return schema_status;
		}
	}
	uint32_t field_type;
	uint32_t expected_type;
	uint32_t field_arguments[64];
	uint32_t expected_arguments[64];
	uint32_t field_argument_count;
	uint32_t expected_argument_count;
	int field_info_status = prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema,
			field_classifier,
			&field_type,
			field_arguments,
			&field_argument_count
		);
	int expected_info_status = prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema,
			expected_classifier,
			&expected_type,
			expected_arguments,
			&expected_argument_count
		);
	if (field_info_status != 0 || expected_info_status != 0 ||
		field_type != expected_type ||
		field_argument_count != expected_argument_count ||
		field_argument_count > 64) {
		return 1;
	}
	uint32_t reconstructed_scrutinee = scrutinee_classifier;
	uint32_t reconstructed_field = field_classifier;
	int changed = 0;
	for (uint32_t i = 0; i < field_argument_count; ++i) {
		if (prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				field_arguments[i],
				expected_arguments[i]
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			continue;
		}
		uint32_t next_scrutinee;
		uint32_t next_field;
		if (prototype_term_graph_replace_exact(
				ctx->terms,
				reconstructed_scrutinee,
				field_arguments[i],
				expected_arguments[i],
				&next_scrutinee
			) != 0 || prototype_term_graph_replace_exact(
				ctx->terms,
				reconstructed_field,
				field_arguments[i],
				expected_arguments[i],
				&next_field
			) != 0) {
			return -1;
		}
		reconstructed_scrutinee = next_scrutinee;
		reconstructed_field = next_field;
		changed = 1;
	}
	if (!changed || prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			reconstructed_field,
			expected_classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL) {
		const struct prototype_typed_projection* traced_scrutinee =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, scrutinee_projection
			);
		const struct prototype_typed_occurrence* traced_operation = traced_scrutinee &&
			traced_scrutinee->source_occurrence <
				ctx->metadata->typed_occurrences.occurrence_count ?
			&ctx->metadata->typed_occurrences.occurrences[
				traced_scrutinee->source_occurrence
			] : NULL;
		fprintf(
			stderr,
			"match-field-inverse variable=%u projection=%u match=%u "
			"match-projection=%u field=%u scrutinee-projection=%u "
			"scrutinee=%u/%d current=%u expected=%u reconstructed=%u guarded-ih=%u\n",
			projection->source_occurrence, typed_projection,
			owner->match_occurrence, match_projection,
			owner->match_field_index, scrutinee_projection,
			traced_scrutinee ? traced_scrutinee->source_occurrence :
				PROTOTYPE_INVALID_ID,
			traced_operation ? traced_operation->tag : -1,
			scrutinee_classifier, expected_classifier,
			reconstructed_scrutinee, guarded_ih
		);
		fprintf(stderr, "  match-field-current: ");
		prototype_type_term_print_debug(
			stderr, ctx->symbols, ctx->intrinsic_environment,
			ctx->type_declarations, ctx->terms, field_classifier
		);
		fprintf(stderr, "\n  match-field-expected: ");
		prototype_type_term_print_debug(
			stderr, ctx->symbols, ctx->intrinsic_environment,
			ctx->type_declarations, ctx->terms, expected_classifier
		);
		fputc('\n', stderr);
	}
	int refine_status = operation_refine_projected_classifier_from_expected(
		ctx,
		scrutinee_projection,
		reconstructed_scrutinee,
		origin_constraint_id,
		depth + 1,
		p_changed
	);
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL) {
		fprintf(
			stderr,
			"match-field-inverse-result variable=%u status=%d changed=%d\n",
			projection->source_occurrence, refine_status, *p_changed
		);
	}
	return refine_status;
}

static int operation_refine_projected_classifier_from_expected(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t expected_classifier,
	uint32_t origin_constraint_id,
	uint32_t depth,
	int* p_changed
) {
	if (!ctx || !p_changed || expected_classifier >= ctx->terms->term_count ||
		!prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, origin_constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		) ||
		depth > 128) {
		return -1;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	if (!projection || projection->source_occurrence >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t operation_id = projection->source_occurrence;
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL) {
		fprintf(
			stderr,
			"projected-expected-refinement operation=%u projection=%u tag=%d "
			"expected=%u depth=%u\n",
			operation_id, typed_projection, operation->tag,
			expected_classifier, depth
		);
	}
	/* A synthesized Match classifier does not prove that its motive equation
	 * has been derived. Consume the expected-result premise before any
	 * classifier-equality shortcut so the proof graph and the projected answer
	 * cannot diverge. */
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		uint32_t motive_application = PROTOTYPE_INVALID_ID;
		int motive_status = operation_motive_offer_expected_equation(
			ctx, operation_id, expected_classifier, origin_constraint_id,
			&motive_application, p_changed
		);
		if (motive_status <= 0) {
			return motive_status;
		}
		uint32_t current = operation_solver_classifier_for_projection(
			ctx, typed_projection
		);
		if (motive_application >= ctx->terms->term_count) {
			return -1;
		}
		if (current >= ctx->terms->term_count ||
			prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				current, motive_application
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			int classifier_changed = 0;
			uint32_t aggregate_equation = ctx->typing->constraints.
				classifier_constraint_for_projection[typed_projection];
			if (!prototype_typing_constraint_has_domain(
					&ctx->typing->constraints, aggregate_equation,
					OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
				) ||
				operation_solver_projection_answer_transition(
					ctx,
					typed_projection,
					OPERATION_SOLVER_ANSWER_PUBLISH,
					motive_application,
					aggregate_equation,
					&classifier_changed
				) != 0) {
				return -1;
			}
			*p_changed |= classifier_changed;
		}
		return 1;
	}
	uint32_t synthesized = operation_solver_classifier_for_projection(
		ctx, typed_projection
	);
	if (synthesized != PROTOTYPE_INVALID_ID) {
		struct prototype_term_conversion_result conversion =
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				synthesized,
				expected_classifier
			);
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL &&
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
			fprintf(
				stderr,
				"projected-variable-conversion projection=%u synthesized=%u "
				"expected=%u status=%d\n",
				typed_projection, synthesized, expected_classifier,
				conversion.status
			);
		}
		if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 0;
		}
		if (prototype_judgement_classifier_compatible(
				ctx->terms,
				ctx->type_declarations,
				synthesized,
				expected_classifier
			)) {
			switch (operation->tag) {
			case PROTOTYPE_TYPED_OCCURRENCE_VAR:
				break;
			default:
				return 0;
			}
		}
		if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
			return -1;
		}
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		return operation_solver_seed_indexed_motive_from_ih_expected(
			ctx,
			operation_id,
			typed_projection,
			expected_classifier,
			p_changed
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		int match_status = operation_refine_match_binder_from_expected(
			ctx,
			typed_projection,
			expected_classifier,
			origin_constraint_id,
			depth,
			p_changed
		);
		if (getenv("A_PROGRAM_MOTIVE_PATH_TRACE") != NULL) {
			fprintf(
				stderr,
				"projected-variable-match-status projection=%u status=%d\n",
				typed_projection, match_status
			);
		}
		if (match_status <= 0) {
			return match_status;
		}
		return operation_solver_bind_projected_variable_expected(
			ctx, typed_projection, expected_classifier, p_changed
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		uint32_t wrapped_projection;
		if (operation_solver_projected_wrapped(
				ctx,
				typed_projection,
				operation->wrapped_occurrence,
				&wrapped_projection
			) != 0) {
			return -1;
		}
		return operation_refine_projected_classifier_from_expected(
			ctx,
			wrapped_projection,
			expected_classifier,
			origin_constraint_id,
			depth + 1,
			p_changed
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN) {
		if (ctx->terms->terms[expected_classifier].tag !=
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return 1;
		}
		uint32_t child_projection;
		if (operation_solver_projected_child(
				ctx,
				typed_projection,
				PROTOTYPE_TERM_CHILD_RETURN_VALUE,
				0,
				&child_projection
			) != 0) {
			return -1;
		}
		return operation_refine_projected_classifier_from_expected(
			ctx,
			child_projection,
			ctx->terms->terms[expected_classifier].as.computation_type.result,
			origin_constraint_id,
			depth + 1,
			p_changed
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK) {
		if (ctx->terms->terms[expected_classifier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE) {
			return 1;
		}
		uint32_t child_projection;
		if (operation_solver_projected_child(
				ctx,
				typed_projection,
				PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION,
				0,
				&child_projection
			) != 0) {
			return -1;
		}
		return operation_refine_projected_classifier_from_expected(
			ctx,
			child_projection,
			ctx->terms->terms[expected_classifier].as.thunk_type.computation,
			origin_constraint_id,
			depth + 1,
			p_changed
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
		uint32_t lambda_expected = expected_classifier;
		if (ctx->terms->terms[lambda_expected].tag ==
				PROTOTYPE_TERM_COMPUTATION_TYPE) {
			uint32_t result = ctx->terms->terms[
				lambda_expected
			].as.computation_type.result;
			if (result < ctx->terms->term_count &&
				ctx->terms->terms[result].tag == PROTOTYPE_TERM_THUNK_TYPE) {
				lambda_expected =
					ctx->terms->terms[result].as.thunk_type.computation;
			}
		}
		uint32_t expected_domain;
		uint32_t expected_family;
		uint32_t expected_binder;
		uint32_t expected_body;
		uint32_t body_projection;
		uint32_t binding_context;
		if (prototype_judgement_pi_parts(
				ctx->terms, lambda_expected,
				&expected_domain, &expected_family
			) != 0 || prototype_term_pure_family_parts(
				ctx->terms, expected_family,
				&expected_binder, &expected_body
			) != 0 || operation_solver_lambda_projection_binding_context(
				ctx, typed_projection, &body_projection, &binding_context
			) != 0) {
			return 1;
		}
		const struct prototype_context* binding = prototype_context_get(
			&ctx->metadata->contexts, binding_context
		);
		uint32_t body_binder;
		uint32_t specialized_body;
		if (!binding || binding->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				ctx->terms, binding->binding_id, &body_binder
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, expected_body, expected_binder,
				body_binder, &specialized_body
			) != 0) {
			return -1;
		}
		uint32_t sequence_relation;
		int sequence_status =
			operation_solver_projected_sequence_binder_relation_id(
				ctx, typed_projection, &sequence_relation
			);
		if (sequence_status < 0) {
			return -1;
		}
		if (sequence_status == 0) {
			if (operation_solver_equation_candidate_transition(
					ctx,
					typed_projection,
					sequence_relation,
					expected_domain,
					p_changed
				) != 0) {
				return -1;
			}
			int materialize_status =
				operation_solver_materialize_sequence_binder_context_view(
					ctx,
					&ctx->typing->constraints.constraints[sequence_relation],
					ctx->typing->constraints.solutions[
						sequence_relation
					].result_term,
					p_changed
				);
			if (materialize_status < 0) {
				return -1;
			}
		} else {
			int bind_status =
				operation_solver_bind_lambda_projection_binder_classifier(
					ctx, typed_projection, expected_domain, 0, p_changed
				);
			if (bind_status != 0) {
				return bind_status;
			}
		}
		return operation_refine_projected_classifier_from_expected(
			ctx, body_projection, specialized_body,
			origin_constraint_id, depth + 1, p_changed
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
		operation->core_term < ctx->terms->term_count &&
		ctx->terms->terms[operation->core_term].tag ==
			PROTOTYPE_TERM_COMPUTATION_FOLD &&
		ctx->terms->terms[operation->core_term].as.computation_fold.clause_count == 0) {
		uint32_t input_projection;
		uint32_t continuation_projection;
		uint32_t continuation_body_projection;
		if (operation_solver_projected_child(
				ctx, typed_projection,
				PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&input_projection
			) != 0 || operation_solver_projected_child(
				ctx, typed_projection,
				PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&continuation_projection
			) != 0 || operation_solver_projected_child(
				ctx, continuation_projection,
				PROTOTYPE_TERM_CHILD_BODY, 0,
				&continuation_body_projection
			) != 0) {
			return -1;
		}
		uint32_t input_classifier = operation_solver_classifier_for_projection(
			ctx, input_projection
		);
		if (input_classifier == PROTOTYPE_INVALID_ID) {
			return operation_refine_projected_classifier_from_expected(
				ctx, continuation_body_projection, expected_classifier,
				origin_constraint_id, depth + 1, p_changed
			);
		}
		struct prototype_classifier_view input_view;
		if (prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL,
				input_classifier, &input_view
			) != 0 || input_view.category !=
				PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
			input_view.computation_kind !=
				PROTOTYPE_COMPUTATION_KIND_RETURNING) {
			return 1;
		}
		uint32_t continuation_expected;
		if (prototype_term_pi(
				ctx->terms, input_view.result, expected_classifier,
				&continuation_expected
			) != 0) {
			return -1;
		}
		return operation_refine_projected_classifier_from_expected(
			ctx, continuation_projection, continuation_expected,
			origin_constraint_id, depth + 1, p_changed
		);
	}
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP) {
		return 1;
	}
	uint32_t function_projection;
	uint32_t argument_projection;
	if (operation_solver_projected_child(
			ctx,
			typed_projection,
			PROTOTYPE_TERM_CHILD_FUNCTION,
			0,
			&function_projection
		) != 0 || operation_solver_projected_child(
			ctx,
			typed_projection,
			PROTOTYPE_TERM_CHILD_ARGUMENT,
			0,
			&argument_projection
		) != 0) {
		return -1;
	}
	uint32_t argument_classifier = operation_solver_classifier_for_projection(
		ctx, argument_projection
	);
	const struct prototype_typed_projection* argument =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, argument_projection
		);
	if (!argument || argument_classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t argument_term;
	if (operation_solver_term_in_projection_context(
			ctx,
			argument_projection,
			ctx->metadata->typed_occurrences.occurrences[
				argument->source_occurrence
			].core_term,
			&argument_term
		) != 0) {
		return -1;
	}
	uint32_t classifier_argument;
	uint32_t codomain_binder = ctx->classifier_workspace.
		expected_app_codomain_binders[operation_id];
	if (codomain_binder == PROTOTYPE_INVALID_ID) {
		codomain_binder = prototype_term_new_binding(ctx->terms);
		ctx->classifier_workspace.
			expected_app_codomain_binders[operation_id] = codomain_binder;
	}
	uint32_t codomain_var;
	uint32_t codomain_body;
	if (codomain_binder == PROTOTYPE_INVALID_ID ||
		prototype_judgement_dependent_classifier_argument(
			ctx->terms,
			ctx->type_declarations,
			argument_classifier,
			argument_term,
			&classifier_argument
		) != 0 || prototype_term_var(
			ctx->terms, codomain_binder, &codomain_var
		) != 0) {
		return -1;
	}
	int abstraction_status;
	if (ctx->terms->terms[classifier_argument].tag == PROTOTYPE_TERM_VAR) {
		abstraction_status = prototype_term_graph_substitute_bound_var(
			ctx->terms,
			expected_classifier,
			ctx->terms->terms[classifier_argument].as.var.binding_id,
			codomain_var,
			&codomain_body
		);
	} else {
		abstraction_status = prototype_term_graph_replace_exact(
			ctx->terms,
			expected_classifier,
			classifier_argument,
			codomain_var,
			&codomain_body
		);
	}
	uint32_t codomain_family;
	uint32_t function_expected;
	if (abstraction_status != 0 || prototype_term_pure_family(
			ctx->terms, codomain_binder, codomain_body, &codomain_family
		) != 0 || prototype_term_pi_family(
			ctx->terms,
			argument_classifier,
			codomain_family,
			&function_expected
		) != 0) {
		return -1;
	}
	const struct prototype_typed_projection* function =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, function_projection
		);
	if (!function) {
		return -1;
	}
	const struct prototype_typed_occurrence* function_operation =
		&ctx->metadata->typed_occurrences.occurrences[
			function->source_occurrence
		];
	if (function_operation->category ==
			PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
		function_operation->computation_kind ==
			PROTOTYPE_COMPUTATION_KIND_RETURNING) {
		uint32_t empty_row;
		if (prototype_term_effect_row_empty(ctx->terms, &empty_row) != 0 ||
			prototype_term_total_computation_type(
				ctx->terms,
				empty_row,
				function_expected,
				&function_expected
			) != 0) {
			return -1;
		}
	}
	return operation_refine_projected_classifier_from_expected(
		ctx,
		function_projection,
		function_expected,
		origin_constraint_id,
		depth + 1,
		p_changed
	);
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
	uint32_t origin_constraint_id,
	uint32_t depth,
	int* p_changed
) {
	if (!ctx || !p_changed || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		expected_classifier >= ctx->terms->term_count ||
		!prototype_typing_constraint_has_domain(
			&ctx->typing->constraints, origin_constraint_id,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		) ||
		depth > 128) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (depth == 0 && !ctx->evidence_workspace.materializing_checked_classifiers &&
		operation_solver_resolve_contexts(ctx, 0) != 0) {
		return -1;
	}
	uint32_t child_occurrence;
	uint32_t body_occurrence;
	uint32_t scrutinee_occurrence;
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
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
	if (check < 0) {
		fprintf(stderr,
			"expected refinement: classifier check failed operation=%u tag=%u "
			"expected=%u depth=%u synthesized=%u\n",
			operation_id, operation->tag, expected_classifier, depth,
			operation_solver_classifier(ctx, operation_id));
		return check;
	}
	/* Expected classifiers solve missing equations; they do not replace a
	 * synthesized, incompatible classifier. Keep IH refinement below as the
	 * explicit recursive-equation exception. */
	uint32_t synthesized_classifier = operation_solver_classifier(
		ctx, operation_id
	);
	uint32_t guarded_ih = PROTOTYPE_INVALID_ID;
	int guarded_ih_status = operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP ?
		operation_subgraph_unique_induction_hypothesis(
			ctx, operation_id, 0, &guarded_ih
		) : 1;
	if (guarded_ih_status < 0) {
		return -1;
	}
	int sequenced_result_var = 0;
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
		operation->binding_id != PROTOTYPE_INVALID_ID) {
		const struct prototype_typing_binder_owner* owner =
			operation_binder_owner_find(
				ctx, operation->binding_id, operation->context_id
			);
		sequenced_result_var = owner &&
			owner->bound_computation_occurrence != PROTOTYPE_INVALID_ID;
	}
	if (check == 0 && synthesized_classifier < ctx->terms->term_count &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_THUNK &&
		!sequenced_result_var &&
		!(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
			guarded_ih_status == 0 && guarded_ih != PROTOTYPE_INVALID_ID)) {
		return 0;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
		operation->ih_owner_occurrence < ctx->metadata->typed_occurrences.occurrence_count &&
		ctx->metadata->typed_occurrences.occurrences[operation->ih_owner_occurrence].tag ==
			PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		int ih_equation_changed = 0;
		uint32_t ih_typed_projection =
			operation_solver_active_projection_for_occurrence(ctx, operation_id);
		if (ih_typed_projection == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		/* Every guarded recursive use contributes an equation for the motive.
		 * The APP-side expectation may expose a suspended function as a callable
		 * Pi, so it is not itself authoritative for the IH occurrence. The owner
		 * Match motive and constructor schema reconstruct the canonical IH
		 * classifier; ordinary APP solving opens its callable boundary afterward. */
		int motive_status = operation_solver_seed_indexed_motive_from_ih_expected(
			ctx, operation_id, ih_typed_projection, expected_classifier,
			&ih_equation_changed
		);
		if (motive_status < 0) {
			return -1;
		}
		if (ih_equation_changed) {
			*p_changed = 1;
		}
		return 1;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		const struct prototype_typing_binder_owner* owner =
			operation->binding_id != PROTOTYPE_INVALID_ID ?
			operation_binder_owner_find(
				ctx, operation->binding_id, operation->context_id
			) : NULL;
		if (owner &&
			owner->bound_computation_occurrence != PROTOTYPE_INVALID_ID) {
			/* Every occurrence of a sequence result reads the lexical binder's
			 * classifier equation. A consumer expectation therefore solves that
			 * equation; the zero-clause fold constraint is the sole rule that projects
			 * it back to the input computation and any guarded IH. Keeping the expected
			 * answer on the binder makes suspension and worklist wake-up explicit. */
			if (owner->lambda_occurrence == PROTOTYPE_INVALID_ID ||
				operation_binder_classifier_meta_solve(
					ctx, owner->lambda_occurrence, expected_classifier, p_changed
				) != 0) {
				return -1;
			}
			return 1;
		}
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
		operation->wrapped_occurrence < ctx->metadata->typed_occurrences.occurrence_count) {
		int status = operation_refine_classifier_from_expected(
			ctx, operation->wrapped_occurrence, expected_classifier,
			origin_constraint_id, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
		operation->wrapped_occurrence < ctx->metadata->typed_occurrences.occurrence_count) {
		int status = operation_refine_classifier_from_expected(
			ctx, operation->wrapped_occurrence, expected_classifier,
			origin_constraint_id, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK &&
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0,
			&child_occurrence
		) == 0 &&
		ctx->terms->terms[expected_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		int status = operation_refine_classifier_from_expected(
			ctx, child_occurrence,
			ctx->terms->terms[expected_classifier].as.thunk_type.computation,
			origin_constraint_id, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0,
			&child_occurrence
		) == 0 &&
		ctx->terms->terms[expected_classifier].tag ==
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		int status = operation_refine_classifier_from_expected(
			ctx, child_occurrence,
			ctx->terms->terms[expected_classifier].as.computation_type.result,
			origin_constraint_id, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP) {
		uint32_t function_occurrence;
		uint32_t argument_occurrence;
		if (compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
				&function_occurrence
			) != 0 || compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
				&argument_occurrence
			) != 0) {
			return -1;
		}
		uint32_t argument_classifier = operation_solver_classifier(
			ctx, argument_occurrence
		);
		uint32_t argument_term = ctx->metadata->typed_occurrences.occurrences[
			argument_occurrence
		].core_term;
		if (argument_classifier >= ctx->terms->term_count ||
			argument_term >= ctx->terms->term_count) {
			return 0;
		}
		uint32_t classifier_argument;
		uint32_t codomain_binder =
			ctx->classifier_workspace.expected_app_codomain_binders[operation_id];
		if (codomain_binder == PROTOTYPE_INVALID_ID) {
			codomain_binder = prototype_term_new_binding(ctx->terms);
			ctx->classifier_workspace.expected_app_codomain_binders[operation_id] =
				codomain_binder;
		}
		uint32_t codomain_var;
		uint32_t codomain_body;
		uint32_t codomain_family;
		uint32_t function_expected;
		if (codomain_binder == PROTOTYPE_INVALID_ID ||
			prototype_judgement_dependent_classifier_argument(
				ctx->terms, ctx->type_declarations,
				argument_classifier, argument_term, &classifier_argument
			) != 0 || prototype_term_var(
				ctx->terms, codomain_binder, &codomain_var
			) != 0) {
			return -1;
		}
		int abstraction_status =
			ctx->terms->terms[classifier_argument].tag == PROTOTYPE_TERM_VAR ?
			prototype_term_graph_substitute_bound_var(
				ctx->terms, expected_classifier,
				ctx->terms->terms[classifier_argument].as.var.binding_id,
				codomain_var, &codomain_body
			) : prototype_term_graph_replace_exact(
				ctx->terms, expected_classifier,
				classifier_argument, codomain_var, &codomain_body
			);
		if (abstraction_status != 0 || prototype_term_pure_family(
				ctx->terms, codomain_binder, codomain_body, &codomain_family
			) != 0 || prototype_term_pi_family(
				ctx->terms, argument_classifier, codomain_family,
				&function_expected
			) != 0) {
			return -1;
		}
		uint32_t reinstantiated;
		if (prototype_term_graph_substitute_bound_var(
				ctx->terms, codomain_body,
				codomain_binder, classifier_argument, &reinstantiated
			) != 0 || prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				reinstantiated, expected_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 0;
		}
		const struct prototype_typed_occurrence* function_operation =
			&ctx->metadata->typed_occurrences.occurrences[function_occurrence];
		if (function_operation->category == PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
			function_operation->computation_kind ==
				PROTOTYPE_COMPUTATION_KIND_RETURNING) {
			uint32_t empty_row;
			if (prototype_term_effect_row_empty(
					ctx->terms, &empty_row
				) != 0 || prototype_term_total_computation_type(
					ctx->terms, empty_row, function_expected,
					&function_expected
				) != 0) {
				return -1;
			}
		}
		int status = operation_refine_classifier_from_expected(
			ctx, function_occurrence, function_expected,
			origin_constraint_id, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_BODY, 0, &body_occurrence
		) == 0 &&
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
			) != 0 || operation->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				ctx->terms, operation->binding_id,
				&body_binder_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				ctx->terms, expected_body,
				expected_binder, body_binder_var, &specialized_body
			) != 0) {
			return -1;
		}
		uint32_t binder_classifier = operation_solver_binder_classifier(
			ctx, operation_id
		);
		if (binder_classifier == PROTOTYPE_INVALID_ID) {
			if (operation_binder_classifier_meta_solve(
					ctx, operation_id, expected_domain, p_changed
				) != 0) {
				return -1;
			}
		} else if (!prototype_judgement_classifier_reference_equal(
				ctx->terms,
				ctx->type_declarations,
				binder_classifier,
				expected_domain
			) && !prototype_judgement_classifier_compatible(
				ctx->terms,
				ctx->type_declarations,
				binder_classifier,
				expected_domain
			)) {
			return -1;
		}
		int status = operation_refine_classifier_from_expected(
			ctx, body_occurrence, specialized_body,
			origin_constraint_id, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
			&child_occurrence
		) == 0 && compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
			&body_occurrence
		) == 0 &&
		operation->core_term < ctx->terms->term_count &&
		ctx->terms->terms[operation->core_term].tag ==
			PROTOTYPE_TERM_COMPUTATION_FOLD &&
		ctx->terms->terms[operation->core_term].as.computation_fold.clause_count == 0) {
		uint32_t input_classifier = operation_solver_classifier(
			ctx, child_occurrence
		);
		uint32_t continuation_body;
		if (compile_occurrence_child(
				ctx, body_occurrence, PROTOTYPE_TERM_CHILD_BODY, 0,
				&continuation_body
			) != 0) {
			return -1;
		}
		if (input_classifier == PROTOTYPE_INVALID_ID) {
			int body_status = operation_refine_classifier_from_expected(
				ctx, continuation_body, expected_classifier,
				origin_constraint_id, depth + 1, p_changed
			);
			if (body_status < 0) {
				fprintf(stderr,
					"fold expected refinement: continuation failed fold=%u continuation=%u "
					"body=%u body-tag=%u expected=%u depth=%u\n",
					operation_id, body_occurrence, continuation_body,
					ctx->metadata->typed_occurrences.occurrences[continuation_body].tag,
					expected_classifier, depth);
				return -1;
			}
			input_classifier = operation_solver_classifier(
				ctx, child_occurrence
			);
		}
		struct prototype_classifier_view input_view;
		uint32_t continuation_classifier;
		if (input_classifier >= ctx->terms->term_count ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL,
				input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
			input_view.computation_kind !=
				PROTOTYPE_COMPUTATION_KIND_RETURNING ||
			prototype_term_pi(
				ctx->terms, input_view.result, expected_classifier,
				&continuation_classifier
			) != 0) {
			return 0;
		}
		int status = operation_refine_classifier_from_expected(
			ctx, body_occurrence, continuation_classifier,
			origin_constraint_id, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		if (compile_occurrence_child(
				ctx, operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
				&scrutinee_occurrence
			) != 0 ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag !=
				PROTOTYPE_TERM_MATCH) {
			return -1;
		}
		const struct prototype_term* match =
			&ctx->terms->terms[operation->core_term];
		uint32_t scrutinee = ctx->metadata->typed_occurrences.occurrences[
			scrutinee_occurrence
		].core_term;
		if (scrutinee >= ctx->terms->term_count) {
			return -1;
		}
		uint32_t scrutinee_classifier = operation_solver_classifier(
			ctx, scrutinee_occurrence
		);
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
			scrutinee_classifier = operation_available_classifier(
				ctx, scrutinee_occurrence
			);
		}
		uint32_t motive_application = PROTOTYPE_INVALID_ID;
		int motive_status = operation_motive_offer_expected_equation(
			ctx, operation_id, expected_classifier, origin_constraint_id,
			&motive_application, p_changed
		);
		if (motive_status <= 0) {
			return motive_status;
		}
		uint32_t current_classifier = operation_solver_classifier(
			ctx, operation_id
		);
		if (current_classifier >= ctx->terms->term_count ||
			prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				current_classifier, motive_application
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			/* The motive is the semantic authority for a Match classifier. The
			 * projection cell is only its checked cache, so replace that projection
			 * through the ordinary projection API and invalidate its dependents. */
			uint32_t match_projection =
				operation_solver_active_projection_for_occurrence(
					ctx, operation_id
				);
			int classifier_changed = 0;
			if (match_projection == PROTOTYPE_INVALID_ID ||
				operation_solver_publish_projection(
					ctx, match_projection, motive_application,
					&classifier_changed
				) != 0) {
				return -1;
			}
			*p_changed |= classifier_changed;
		}
		if (check > 0) {
			/* The structural check above has already validated every branch in its
			 * own pulled-back Context. Only the Match motive is refined here; pushing
			 * the unspecialized expectation through those branches a second time would
			 * confuse source and target Context classifiers. */
			return 1;
		}
		const struct prototype_typed_occurrence* scrutinee_operation =
			&ctx->metadata->typed_occurrences.occurrences[scrutinee_occurrence];
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID &&
			scrutinee_operation->core_term < ctx->terms->term_count &&
			ctx->terms->terms[scrutinee_operation->core_term].tag ==
				PROTOTYPE_TERM_VAR) {
			uint32_t binding_context;
			if (prototype_context_find_binding(
					&ctx->metadata->contexts, scrutinee_operation->context_id,
					ctx->terms->terms[
						scrutinee_operation->core_term
					].as.var.binding_id, &binding_context
				) == 0) {
				scrutinee_classifier = operation_context_classifier_term(
					ctx, binding_context
				);
			}
		}
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		for (uint32_t case_index = 0;
			case_index < operation->case_count; ++case_index) {
			uint32_t branch_occurrence;
			if (compile_occurrence_child(
					ctx, operation_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, case_index,
					&branch_occurrence
				) != 0) {
				return -1;
			}
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[
					operation->first_case + case_index
				];
			const struct prototype_match_case* term_case = &ctx->terms->cases[
				match->as.match.first_case + case_index
			];
			if (term_case->binder_count > 64 || term_case->binder_count !=
					operation_case->binder_count) {
				return -1;
			}
			if (operation_case->refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
				continue;
			}
			int pending_pullback = 0;
			int constant_refinement =
				operation_case->refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT;
			uint32_t refinement_substitution = PROTOTYPE_INVALID_ID;
			if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
				refinement_substitution = operation_case->refinement_substitution;
			} else if (!constant_refinement) {
				struct prototype_typing_constraint* refinement =
					operation_solver_branch_refinement_constraint(
						ctx, operation->first_case + case_index
					);
				const struct prototype_typing_constraint_solution* solution = refinement ?
					&ctx->typing->constraints.solutions[refinement->id] : NULL;
				int refinement_status = solution ?
					solution->branch_refinement.refinement_status :
					PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
				if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
					continue;
				}
				if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_CONSTANT) {
					constant_refinement = 1;
				} else if (refinement_status != PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
					return refinement_status == PROTOTYPE_INDEX_REFINEMENT_INVALID ?
						-1 : 0;
				} else {
					refinement_substitution =
						solution->branch_refinement.substitution;
					pending_pullback = 1;
				}
			}
			if (!constant_refinement) {
				const struct prototype_substitution* stored =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						refinement_substitution
					);
				int case_is_solved =
					prototype_typed_occurrence_match_case_is_solved(operation_case);
				uint32_t materialized_case_context = case_is_solved ?
					operation_case_materialized_context(
						ctx, operation->first_case + case_index
					) : operation_case_input_context(
						ctx, operation->first_case + case_index
					);
				if (!stored || materialized_case_context == PROTOTYPE_INVALID_ID ||
					(case_is_solved ? stored->source_context :
						stored->target_context) != materialized_case_context) {
					return -1;
				}
			}
			uint32_t branch_expected = expected_classifier;
			if (!constant_refinement && prototype_term_reindex(
					ctx->terms, ctx->type_declarations,
					&ctx->metadata->contexts, &ctx->metadata->substitutions,
					expected_classifier, refinement_substitution,
					&branch_expected
				 ) != 0) {
				fprintf(stderr,
					"match expected refinement: branch expectation failed match=%u case=%u "
					"status=%d pending=%d expected=%u substitution=%u\n",
					operation_id, case_index, operation_case->refinement_status,
					pending_pullback,
					expected_classifier,
					refinement_substitution);
				return -1;
			}
			int branch_status = operation_refine_classifier_from_expected(
				ctx, branch_occurrence, branch_expected,
				origin_constraint_id, depth + 1, p_changed
			);
			if (branch_status <= 0) {
				if (branch_status < 0) {
					fprintf(stderr,
						"match expected refinement: branch failed match=%u case=%u branch=%u "
						"tag=%u expected=%u depth=%u\n",
						operation_id, case_index, branch_occurrence,
						ctx->metadata->typed_occurrences.occurrences[branch_occurrence].tag,
						branch_expected, depth);
				}
				return branch_status;
			}
		}
		return 1;
	}
	check = operation_check_classifier(
		ctx, operation_id, expected_classifier, depth
	);
	if (check <= 0) {
		return check;
	}
	if ((ctx->evidence_workspace.materializing_checked_classifiers ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
		synthesized_classifier == PROTOTYPE_INVALID_ID) {
		if (operation_classifier_meta_solve(
				ctx, operation_id, expected_classifier
			) != 0) {
			return -1;
		}
	}
	return 1;
}

static int compile_phase_check_ascriptions(struct compile_context* ctx) {
	if (!ctx || !ctx->typing || !ctx->metadata) {
		return -1;
	}
	for (uint32_t id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
		id != PROTOTYPE_INVALID_ID;
		id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, id
		)) {
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[id];
		const struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[id];
		if (constraint->kind != OPERATION_CONSTRAINT_CONVERTIBLE ||
			solution->state != OPERATION_CONSTRAINT_STATE_CONTRADICTION ||
			solution->reason != OPERATION_CLASSIFIER_GOAL_REASON_CONVERSION_REJECTED) {
			continue;
		}
		uint32_t expected_classifier =
			constraint->payload.classifier.goal.conversion.expected_classifier;
		uint32_t actual_classifier = constraint->operand_count == 1 ?
			operation_solver_classifier_for_projection(
				ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
			) : PROTOTYPE_INVALID_ID;
		uint32_t projected_expected;
			if (expected_classifier < ctx->terms->term_count &&
			operation_solver_term_in_projection_context(
				ctx,
				constraint->owner_typed_projection,
				expected_classifier,
				&projected_expected
			) == 0) {
				expected_classifier = projected_expected;
			}
			if (getenv("A_PROGRAM_CLASSIFIER_TRACE") != NULL) {
				fprintf(stderr, "rejected expected classifier: ");
				prototype_type_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, expected_classifier
				);
				fprintf(stderr, "\nrejected actual classifier: ");
				prototype_type_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, actual_classifier
				);
				fputc('\n', stderr);
			}
		(void)add_resolve_error(
			ctx,
			PROTOTYPE_RESOLVE_ERROR_COMPILE,
			-1,
			-1,
			constraint->source_ast
		);
		(void)add_compile_diagnostic(
			ctx,
			PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_EXPECTATION_CHECK,
			PROTOTYPE_COMPILE_DIAGNOSTIC_ASCRIPTION_MISMATCH,
			constraint->source_ast,
			constraint->source_occurrence,
			constraint->id,
			expected_classifier,
			actual_classifier,
			compile_source_span(ctx, constraint->source_ast)
		);
	}
	return 0;
}

static int accepted_claim_for_exact_proposition(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_claim_id
);

static int compile_phase_check_expectations(struct compile_context* ctx) {
	if (!ctx || !ctx->typing || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0;
		i < ctx->typing->seed.publication_input_count;
		++i) {
		const struct prototype_typing_publication_input* input =
			&ctx->typing->seed.publication_inputs[i];
		if (input->expectation_occurrence == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (input->expectation_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			if (ctx->metadata->function_graph_preflight) {
				continue;
			}
			fprintf(stderr,
				"expectation checking: exact occurrence unavailable expectation=%u name=%d\n",
				i, input->name_symbol_id);
			return -1;
		}
		uint32_t checked_occurrence = input->expectation_occurrence;
		uint32_t checked_projection = operation_solver_publication_projection(
			ctx, checked_occurrence
		);
		if (checked_projection >= PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
			return -1;
		}
		uint32_t constraint_id = ctx->typing->constraints.
			conversion_constraint_for_projection[checked_projection];
		const struct prototype_typing_constraint* constraint =
			prototype_typing_constraint_has_domain(
				&ctx->typing->constraints, constraint_id,
				OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
			) ?
				&ctx->typing->constraints.constraints[constraint_id] : NULL;
		const struct prototype_typing_constraint_solution* solution = constraint ?
			&ctx->typing->constraints.solutions[constraint_id] : NULL;
		if (!constraint || !solution ||
			constraint->kind != OPERATION_CONSTRAINT_CONVERTIBLE ||
			constraint->owner_typed_projection != checked_projection ||
			constraint->source_occurrence != checked_occurrence ||
			constraint->payload.classifier.goal.conversion.expected_classifier !=
				input->expectation_classifier ||
			constraint->operand_count != 1 ||
			solution->state != OPERATION_CONSTRAINT_STATE_SOLVED) {
			fprintf(stderr,
				"expectation checking: exact conversion is not solved expectation=%u "
				"occurrence=%u projection=%u constraint=%u state=%d\n",
				i, checked_occurrence, checked_projection, constraint_id,
				solution ? solution->state : -1);
			return -1;
		}
		uint32_t actual_classifier = operation_solver_classifier_for_projection(
			ctx, operation_solver_constraint_operand_projection(ctx, constraint, 0)
		);
		if (actual_classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
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
	if (!ctx || !ctx->typing || !ctx->metadata || !ctx->terms) {
		return -1;
	}

	for (uint32_t i = 0;
		i < ctx->typing->seed.publication_input_count;
		++i) {
		const struct prototype_typing_publication_input* input =
			&ctx->typing->seed.publication_inputs[i];
		if (input->exposed_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[
				input->exposed_occurrence
			];
		if (occurrence->classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		int already_published = 0;
		for (size_t label_id = 0;
			label_id < ctx->metadata->label_count;
			++label_id) {
			const struct prototype_compile_label* label =
				&ctx->metadata->labels[label_id];
			if (label->name_symbol_id != input->name_symbol_id) {
				continue;
			}
			if (label->term != occurrence->core_term ||
				label->exposed_occurrence != input->exposed_occurrence ||
				label->exposed_classifier != occurrence->classifier) {
				return -1;
			}
			already_published = 1;
			break;
		}
		if (already_published) {
			continue;
		}
		if (add_compile_label(
				ctx,
				input->name_symbol_id,
				occurrence->core_term,
				occurrence->classifier,
				input->exposed_occurrence,
				input->expectation_occurrence,
				input->expectation_classifier
			) != 0) {
			fprintf(stderr,
				"label publication entry failed assignment=%u term=%u classifier=%u operation=%u\n",
				i, occurrence->core_term, occurrence->classifier,
				input->exposed_occurrence);
			return -1;
		}
	}
	if (ctx->metadata->selected_entry_occurrence != PROTOTYPE_INVALID_ID) {
		if (ctx->metadata->selected_entry_symbol_id < 0 ||
			ctx->metadata->selected_entry_term >= ctx->terms->term_count ||
			ctx->metadata->selected_entry_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count ||
			ctx->metadata->typed_occurrences.occurrences[
				ctx->metadata->selected_entry_occurrence
			].category != PROTOTYPE_CLASSIFIER_CATEGORY_VALUE) {
			return -1;
		}
		ctx->metadata->selected_entry_classifier = ctx->metadata->typed_occurrences.occurrences[
			ctx->metadata->selected_entry_occurrence
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
	for (size_t i = 0; i < metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation = &metadata->typed_occurrences.occurrences[i];
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
			operation->core_term < terms->term_count &&
			terms->terms[operation->core_term].tag == PROTOTYPE_TERM_COMPUTATION_FOLD &&
			terms->terms[operation->core_term].as.computation_fold.clause_count != 0) {
			capabilities |= PROTOTYPE_RUNTIME_CAPABILITY_HANDLER;
		}
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REQUEST ||
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
			const struct prototype_core_effect_operation_declaration* declaration =
				prototype_core_effect_operation_declaration(
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
	if (ctx->judgement && ctx->judgement->next_universe_var < next_level_var) {
		ctx->judgement->next_universe_var = next_level_var;
	}
	if (ctx->type_declarations && ctx->type_declarations->readback.next_level_var < next_level_var) {
		ctx->type_declarations->readback.next_level_var = next_level_var;
	}
}

static int accepted_claim_for_exact_proposition(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_claim_id
) {
	if (!judgement || !p_claim_id) {
		return -1;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (proposition && proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->context_id == context_id && proposition->subject == subject &&
			proposition->classifier == classifier) {
			*p_claim_id = i;
			return 0;
		}
	}
	return 1;
}

static int accepted_type_formation_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject,
	uint32_t* p_classifier,
	uint32_t* p_claim_id
) {
	if (!judgement || !p_classifier || !p_claim_id) {
		return -1;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (proposition && proposition->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
			proposition->context_id == context_id && proposition->subject == subject) {
			*p_classifier = proposition->classifier;
			*p_claim_id = i;
			return 0;
		}
	}
	return 1;
}

/* Termination evidence is public only for computations whose synthesized
 * classifier is explicitly TOTAL. */
static int compile_phase_publish_termination_evidence(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata || !ctx->judgement) {
		return -1;
	}
	for (uint32_t input_id = 0;
		input_id < ctx->typing->seed.termination_witness_input_count;
		++input_id) {
		uint32_t occurrence_id =
			ctx->typing->seed.termination_witness_inputs[input_id];
		if (occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		uint32_t witness_term_computation;
		const struct prototype_typed_occurrence* witness_occurrence =
			&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
		if (witness_occurrence->core_term >= ctx->terms->term_count ||
			prototype_term_terminates_witness_info(
				ctx->terms, witness_occurrence->core_term,
				&witness_term_computation
			) != 0) {
			return -1;
		}
		uint32_t source_computation_occurrence;
		if (compile_occurrence_child(
				ctx, occurrence_id,
				PROTOTYPE_TERM_CHILD_TERMINATION_EVIDENCE_COMPUTATION, 0,
				&source_computation_occurrence
			) != 0) {
			return -1;
		}
		if (source_computation_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		uint32_t witness_projection = operation_solver_publication_projection(
			ctx, occurrence_id
		);
		uint32_t computation_projection;
		uint32_t witness_context;
		uint32_t witness_substitution;
		uint32_t witness_subject;
		uint32_t witness_classifier;
		uint32_t computation_context;
		uint32_t computation_substitution;
		uint32_t computation_subject;
		uint32_t computation_classifier;
		if (witness_projection == PROTOTYPE_INVALID_ID ||
			operation_solver_projected_child(
				ctx,
				witness_projection,
				PROTOTYPE_TERM_CHILD_TERMINATION_EVIDENCE_COMPUTATION,
				0,
				&computation_projection
			) != 0 || operation_typed_projection_publication_typing_answer(
				ctx,
				witness_projection,
				&witness_context,
				&witness_substitution,
				&witness_subject,
				&witness_classifier
			) != 0 || operation_typed_projection_publication_typing_answer(
				ctx,
				computation_projection,
				&computation_context,
				&computation_substitution,
				&computation_subject,
				&computation_classifier
			) != 0) {
			fprintf(
				stderr,
				"termination evidence projection unresolved occurrence=%u "
				"witness-projection=%u source-computation=%u\n",
				occurrence_id,
				witness_projection,
				source_computation_occurrence
			);
			return -1;
		}
		(void)witness_substitution;
		(void)computation_substitution;
		uint32_t suspended = PROTOTYPE_INVALID_ID;
		uint32_t witness_computation = PROTOTYPE_INVALID_ID;
		if (witness_classifier == PROTOTYPE_INVALID_ID ||
			computation_classifier == PROTOTYPE_INVALID_ID) {
			fprintf(
				stderr,
				"termination evidence has no classifier occurrence=%u "
				"computation-projection=%u\n",
				occurrence_id,
				computation_projection
			);
			return -1;
		}
		if (witness_context != computation_context) {
			fprintf(
				stderr,
				"termination evidence context mismatch occurrence=%u context=%u "
				"computation-projection=%u context=%u\n",
				occurrence_id,
				witness_context,
				computation_projection,
				computation_context
			);
			return -1;
		}
		if (prototype_term_terminates_type_info(
				ctx->terms, witness_classifier, &suspended
			) != 0) {
			fprintf(
				stderr,
				"termination evidence classifier is not Terminates occurrence=%u "
				"classifier=%u\n",
				occurrence_id,
				witness_classifier
			);
			return -1;
		}
		if (prototype_term_terminates_witness_info(
				ctx->terms, witness_subject, &witness_computation
			) != 0) {
			fprintf(
				stderr,
				"termination evidence term is not a witness occurrence=%u term=%u\n",
				occurrence_id,
				witness_subject
			);
			return -1;
		}
		if (suspended != witness_computation ||
			computation_subject != suspended) {
			fprintf(
				stderr,
				"termination evidence endpoint mismatch occurrence=%u suspended=%u "
				"witness=%u computation-term=%u\n",
				occurrence_id,
				suspended,
				witness_computation,
				computation_subject
			);
			return -1;
		}
		uint32_t computation_claim;
		uint32_t terminates_type_claim;
		uint32_t terminates_claim;
		uint32_t universe;
		size_t claim_count_before = ctx->judgement->claim_count;
		if (accepted_claim_for_exact_proposition(
				ctx->judgement, computation_context, computation_subject,
				computation_classifier, &computation_claim
			) != 0) {
			fprintf(stderr,
				"termination computation Claim missing witness-occurrence=%u "
				"source-computation=%u projection=%u context=%u subject=%u "
				"classifier=%u\n",
				occurrence_id,
				source_computation_occurrence,
				computation_projection,
				computation_context,
				computation_subject,
				computation_classifier);
			return -1;
		}
		int formation_status = accepted_type_formation_claim(
			ctx->judgement, witness_context, witness_classifier,
			&universe, &terminates_type_claim
		);
		if (formation_status < 0) {
			return -1;
		}
		if (formation_status > 0 && (
				prototype_term_universe_var(
					ctx->terms, ctx->judgement->next_universe_var++, &universe
				) != 0 || prototype_judgement_add_terminates_type_formation(
					ctx->judgement, ctx->terms, ctx->type_declarations,
					witness_context, witness_classifier, universe,
					computation_claim, &terminates_type_claim
				) != 0
		)) {
			return -1;
		}
		uint32_t computation_classifier_whnf = computation_classifier;
		if (prototype_judgement_classifier_value_whnf(
				ctx->terms, ctx->type_declarations, computation_classifier_whnf,
				&computation_classifier_whnf
			) != 0 || computation_classifier_whnf >= ctx->terms->term_count ||
			ctx->terms->terms[computation_classifier_whnf].tag !=
				PROTOTYPE_TERM_THUNK_TYPE) {
			return -1;
		}
		uint32_t computation_type = ctx->terms->terms[
			computation_classifier_whnf
		].as.thunk_type.computation;
		if (computation_type >= ctx->terms->term_count ||
			ctx->terms->terms[computation_type].tag !=
				PROTOTYPE_TERM_COMPUTATION_TYPE ||
				ctx->terms->terms[computation_type].as.computation_type.totality !=
					PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ||
				prototype_judgement_add_terminates_total_computation(
					ctx->judgement, ctx->terms, witness_context, occurrence_id,
					witness_subject, witness_classifier,
					terminates_type_claim, computation_claim, &terminates_claim
				) != 0) {
			return -1;
		}
		ctx->metadata->termination_evidence_claim_count +=
			ctx->judgement->claim_count - claim_count_before;
		(void)terminates_claim;
	}
	return 0;
}

static int operation_solver_lookup_accepted_projection_classifier(
	void* authority,
	uint32_t occurrence_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t* p_classifier
) {
	struct compile_context* ctx = authority;
	if (!ctx || !ctx->typing || !ctx->metadata || !p_classifier ||
		occurrence_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	int found = 0;
	for (uint32_t typed_projection =
			prototype_typed_projection_first_for_typing_node(
				&ctx->typing->context_projections, occurrence_id
			);
		typed_projection != PROTOTYPE_INVALID_ID;
		typed_projection = prototype_typed_projection_next_for_typing_node(
			&ctx->typing->context_projections, typed_projection
		)) {
		const struct prototype_typed_projection* projection =
			prototype_typed_projection_get(
				&ctx->typing->context_projections, typed_projection
			);
		uint32_t equation_id;
		if (!projection || projection->source_occurrence != occurrence_id ||
			operation_solver_projected_classifier_equation_id(
				ctx, typed_projection, &equation_id
			) != 0 || equation_id >=
				ctx->typing->constraints.constraint_count) {
			return -1;
		}
		const struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[equation_id];
		if (solution->state != OPERATION_CONSTRAINT_STATE_SOLVED ||
			solution->result_term == PROTOTYPE_INVALID_ID ||
			solution->answer_state ==
				PROTOTYPE_TYPING_EQUATION_ANSWER_CONTRADICTION) {
			continue;
		}
		uint32_t projected_context;
		uint32_t substitution;
		uint32_t projected_subject;
		uint32_t projected_classifier;
		int answer_status = operation_typed_projection_publication_typing_answer(
			ctx,
			typed_projection,
			&projected_context,
			&substitution,
			&projected_subject,
			&projected_classifier
		);
		(void)substitution;
		if (answer_status < 0) {
			return -1;
		}
		if (answer_status != 0 || projected_context != context_id ||
			projected_subject != subject) {
			continue;
		}
		if (found && *p_classifier != projected_classifier) {
			return -1;
		}
		*p_classifier = projected_classifier;
		found = 1;
	}
	return found ? 0 : 1;
}

struct compile_driver_state {
	int producer_phase;
	int producer_inference_precharged;
};

static int compile_context_initialize_for_image(
	struct compile_context* ctx,
	struct compile_judgement_workspace* workspace,
	struct prototype_core_pipeline* core,
	struct prototype_typing_pipeline* typing,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	const struct prototype_core_request_service* core_requests,
	size_t accepted_occurrence_prefix,
	size_t accepted_case_prefix,
	size_t accepted_fold_clause_prefix
) {
	if (!ctx || !workspace || !core || !core->initialized || !typing ||
		!type_declarations || !judgement || !metadata || !intrinsic_environment ||
		!core_requests || accepted_occurrence_prefix >
			metadata->typed_occurrences.occurrence_count ||
		accepted_case_prefix > metadata->typed_occurrences.case_count ||
		accepted_fold_clause_prefix >
			metadata->typed_occurrences.fold_clause_count) {
		return -1;
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->active_classifier_constraint_id = PROTOTYPE_INVALID_ID;
	ctx->typing = typing;
	ctx->effort_atomic = 1;
	struct prototype_term_db* terms = prototype_core_pipeline_terms(core);
	if (prototype_accepted_typing_input_view_init(
			&ctx->accepted_typing,
			&metadata->typed_occurrences,
			&metadata->contexts,
			&metadata->substitutions,
			terms,
			&metadata->typed_publication,
			accepted_occurrence_prefix,
			accepted_case_prefix,
			accepted_fold_clause_prefix
		) != 0) {
		return -1;
	}
	ctx->intrinsic_environment = intrinsic_environment;
	ctx->symbols = symbols;
	ctx->terms = terms;
	ctx->core_requests = core_requests;
	ctx->type_declarations = type_declarations;
	ctx->judgement = judgement;
	ctx->computation_constraint_results = workspace->computation_constraint_results;
	ctx->computation_constraint_result_capacity = PROTOTYPE_JUDGEMENT_DELTA_CAPACITY;
	ctx->computation_constraint_inputs = workspace->computation_constraint_inputs;
	ctx->computation_constraint_input_capacity = PROTOTYPE_JUDGEMENT_DELTA_CAPACITY;
	ctx->proof_computation_constraint_inputs =
		workspace->proof_computation_constraint_inputs;
	ctx->proof_computation_constraint_input_capacity =
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY;
	ctx->proof_computation_constraint_results =
		workspace->proof_computation_constraint_results;
	ctx->proof_computation_constraint_result_capacity =
		PROTOTYPE_JUDGEMENT_DELTA_CAPACITY;
	prototype_judgement_delta_init(
		&ctx->judgement_delta,
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
		&ctx->judgement_delta, intrinsic_environment
	);
	ctx->metadata = metadata;
	prototype_judgement_delta_set_context_store(
		&ctx->judgement_delta,
		&metadata->contexts,
		&metadata->substitutions
	);
	struct prototype_context_classifier_view context_classifier_view =
		operation_type_context_view(ctx);
	prototype_judgement_delta_set_context_classifier_view(
		&ctx->judgement_delta, &context_classifier_view
	);
	ctx->judgement_delta.dimension_operators = &metadata->dimension_operators;
	ctx->context_ids[0] = prototype_context_empty(&metadata->contexts);
	ctx->namespace_symbol_id = namespace_symbol_id;
	ctx->imported_interfaces = imported_interfaces;
	ctx->imported_interface_count = imported_interface_count;
	return ctx->context_ids[0] == PROTOTYPE_INVALID_ID ? -1 : 0;
}

static int compile_pending_with_workspace(
	struct prototype_ast_db* asts,
	struct prototype_core_pipeline* core,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct compile_judgement_workspace* workspace,
	struct compile_driver_state* driver,
	const struct prototype_core_request_service* core_requests,
	const struct prototype_source_schedule* source_schedule,
	const struct prototype_source_lowering_plan* source_plan,
	struct prototype_source_core_handoff_builder* core_source_results,
	struct prototype_source_core_handoff_view* core_source_view,
	struct prototype_typing_source_results* typing_source_results,
	struct prototype_typing_pipeline* typing,
	struct compile_context* ctx,
	int stop_after_open_image
) {
	if (!core || !core->initialized || !type_declarations || !judgement || !metadata ||
		!intrinsic_environment || !workspace || !driver || !core_requests ||
		!typing || !ctx) {
		return -1;
	}
	if (driver->producer_phase == 0 &&
		(!asts || !source_schedule || !source_plan || !core_source_results ||
		 !core_source_view || !typing_source_results)) {
		return -1;
	}
	struct prototype_term_db* terms = prototype_core_pipeline_terms(core);
	uint64_t phase_started = 0;
	if (driver->producer_phase == 0) {
	size_t accepted_occurrence_prefix = metadata->typed_occurrences.frozen ?
		metadata->typed_occurrences.occurrence_count : 0;
	size_t accepted_case_prefix = metadata->typed_occurrences.frozen ?
		metadata->typed_occurrences.case_count : 0;
	size_t accepted_fold_clause_prefix = metadata->typed_occurrences.frozen ?
		metadata->typed_occurrences.fold_clause_count : 0;
	if (compile_context_initialize_for_image(
			ctx,
			workspace,
			core,
			typing,
			type_declarations,
			judgement,
			metadata,
			symbols,
			intrinsic_environment,
			namespace_symbol_id,
			imported_interfaces,
			imported_interface_count,
			core_requests,
			accepted_occurrence_prefix,
			accepted_case_prefix,
			accepted_fold_clause_prefix
		) != 0) {
		fprintf(stderr, "compile pending failed: accepted typing input view\n");
		return -1;
	}
	ctx->asts = asts;
	ctx->source_schedule = source_schedule;
	ctx->source_plan = source_plan;
	ctx->core_source_results = core_source_results;
	ctx->core_source_view = core_source_view;
	ctx->typing_source_results = typing_source_results;
	metadata->constraint_topology_sealed = 0;
	metadata->constraint_topology_digest = 0;
	if (prototype_typed_occurrence_graph_begin_transaction(
			&metadata->typed_occurrences, terms, &metadata->contexts
		) != 0) {
		fprintf(stderr, "compile pending failed: typed occurrence transaction\n");
		return -1;
	}
	if (operation_binder_owner_index_seed_transaction(ctx) != 0) {
		fprintf(stderr, "compile pending failed: binder owner index seeding\n");
		return -1;
	}
	sync_universe_level_counters(ctx);
	if (compile_phase_collect_function_graph_requests(ctx) != 0) {
		fprintf(stderr, "compile pending failed: function-graph request collection\n");
		return -1;
	}

	if (compile_phase_report_def_index_errors(ctx) != 0) {
		return -1;
	}
	phase_started = compile_performance_clock_ns();
	if (compile_phase_build_graph(ctx) != 0) {
		fprintf(stderr, "compile pending failed: build graph\n");
		return -1;
	}
	if (compile_phase_materialize_classifier_rhs_seeds(ctx) != 0) {
		fprintf(stderr, "compile pending failed: classifier RHS seed materialization\n");
		return -1;
	}
	struct prototype_core_inspect_counts c0_counts;
	uint64_t c0_graph_revision;
	if (typing_core_inspect_counts(
			ctx->core_requests, &c0_counts, &c0_graph_revision
		) != 0 || prototype_source_core_handoff_builder_seal(
			ctx->core_source_results,
			c0_graph_revision,
			core_source_view
		) != 0) {
		fprintf(stderr, "compile pending failed: C0 source handoff sealing\n");
		return -1;
	}
	struct prototype_typing_graph_build_context typing_graph_build;
	if (prototype_typing_pipeline_graph_build_context(
			ctx->typing,
			ctx->typing_source_results,
			&ctx->metadata->typed_occurrences,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			(uint32_t)prototype_typed_occurrence_graph_transaction_start(
				&ctx->metadata->typed_occurrences
			),
			ctx->metadata->selected_entry_occurrence,
			&typing_graph_build
		) != 0 || prototype_typing_graph_build_rooted_projections(
			&typing_graph_build
		) != 0 || prototype_typing_pipeline_finish_graph_build(
			ctx->typing
		) != 0) {
		fprintf(stderr, "compile pending failed: Layer T projection topology\n");
		return -1;
	}
	metadata->graph_build_time_ns += compile_performance_elapsed_ns(phase_started);
	prototype_judgement_delta_set_occurrence_store(
		&ctx->judgement_delta,
		metadata->typed_occurrences.occurrences,
		metadata->typed_occurrences.occurrence_count,
		metadata->typed_occurrences.edges,
		metadata->typed_occurrences.edge_count,
		metadata->typed_occurrences.cases,
		metadata->typed_occurrences.case_count
	);
	prototype_judgement_delta_set_occurrence_usage_provider(
		&ctx->judgement_delta, ctx, operation_solver_proposition_usage
	);
	/* Source lowering ends by expanding the replayable root equation topology.
	 * This is still zero semantic progress: equation identity, operands,
	 * dependencies and SCCs belong to the open image, while input seeding and
	 * every answer transition belong to solve(image, effort). */
	if (operation_solver_generate_constraints(ctx) != 0) {
		fprintf(stderr, "compile pending failed: open-image equation topology\n");
		return -1;
	}
	if (ctx->typing->constraints.semantic_transition_count != 0) {
		fprintf(stderr,
			"open compilation image contains semantic transitions=%" PRIu64 "\n",
			ctx->typing->constraints.semantic_transition_count
		);
		return -1;
	}
	for (uint32_t equation = 0;
		equation < ctx->typing->constraints.constraint_count;
		++equation) {
		const struct prototype_typing_constraint_solution* solution =
			&ctx->typing->constraints.solutions[equation];
		if (solution->state != OPERATION_CONSTRAINT_STATE_PENDING ||
			solution->reason != OPERATION_CLASSIFIER_GOAL_REASON_NONE ||
			solution->answer_state != PROTOTYPE_TYPING_EQUATION_ANSWER_UNSOLVED ||
			solution->result_term != PROTOTYPE_INVALID_ID ||
			solution->evidence_id != PROTOTYPE_INVALID_ID) {
			fprintf(stderr,
				"open compilation image contains solved equation state id=%u\n",
				equation
			);
			return -1;
		}
	}
	driver->producer_phase = 1;
	if (stop_after_open_image) {
		return 2;
	}
	}
	if (driver->producer_phase == 1) {
	/* Match resolution and imported-classifier inference may emit candidates
	 * before the main constraint-generation pass. Resource usage is part of
	 * Proposition identity, so its graph solution must exist before either
	 * phase can publish a candidate. */
	if (operation_solver_refresh_usage_solutions(ctx) != 0) {
		fprintf(stderr, "compile pending failed: initial usage solutions\n");
		return -1;
	}
	if (validate_constructor_occurrence_saturation(ctx) != 0) {
		fprintf(stderr, "compile pending failed: constructor saturation\n");
		return -1;
	}
	if (begin_resolution_iteration(ctx, 0, count_unresolved_resolution_items(ctx)) != 0) {
		fprintf(stderr, "compile pending failed: initial resolution iteration\n");
		return -1;
	}
	if (compile_phase_infer_imported_constructor_classifiers(ctx) != 0) {
		fprintf(stderr, "compile pending failed: imported constructor classifiers\n");
		return -1;
	}
	struct prototype_context_classifier_view type_context =
		operation_type_context_view(ctx);
	if (type_declarations->representation_db.cache_dirty &&
		prototype_type_declaration_rebuild_representations_in_context(
			terms, type_declarations, &type_context
		) != 0) {
		fprintf(stderr, "compile pending failed: type representation finalization\n");
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 || prototype_internal_canonicalize_constructor_owner_refs(
			terms, type_declarations, 0
		) != 0) {
		fprintf(stderr, "compile pending failed: pre-Match type-view canonicalization\n");
		return -1;
	}
	if (compile_phase_resolve_pending_match_items(ctx) < 0) {
		fprintf(stderr, "compile pending failed: initial Match resolution\n");
		return -1;
	}
	if (finish_resolution_iteration(ctx, count_unresolved_resolution_items(ctx)) != 0) {
		fprintf(stderr, "compile pending failed: initial resolution accounting\n");
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 || prototype_internal_canonicalize_constructor_owner_refs(
			terms, type_declarations, 0
		) != 0) {
		fprintf(stderr, "compile pending failed: post-Match type-view canonicalization\n");
		return -1;
	}
	driver->producer_phase = 2;
	}
	if (driver->producer_phase == 2) {
	if (!driver->producer_inference_precharged) {
		uint64_t phase_credits[PROTOTYPE_EFFORT_PHASE_COUNT] = { 0 };
		phase_credits[PROTOTYPE_EFFORT_PHASE_NORMALIZATION] =
			metadata->typed_occurrences.occurrence_count + 1;
		phase_credits[PROTOTYPE_EFFORT_PHASE_MOTIVE] =
			metadata->typed_occurrences.case_count + 1;
		ctx->effort_atomic = 0;
		int phase_effort = prototype_effort_consume_split(
			&metadata->effort, phase_credits
		);
		if (phase_effort != 0) return phase_effort;
		driver->producer_inference_precharged = 1;
	}
	ctx->effort_atomic = 1;
	phase_started = compile_performance_clock_ns();
	int inference_status = compile_phase_infer_pending_types(ctx);
	if (inference_status != 0) {
		if (inference_status < 0) {
			fprintf(stderr, "compile pending failed: infer pending types\n");
		}
		return inference_status;
	}
	metadata->fixed_point_time_ns += compile_performance_elapsed_ns(phase_started);
	ctx->effort_atomic = 0;
	driver->producer_phase = 3;
	}
	if (driver->producer_phase == 3) {
	uint64_t finalization_credit =
		metadata->typed_occurrences.occurrence_count * UINT64_C(64) + 1;
	int finalization_effort = compile_effort_charge(
		ctx->metadata, ctx->effort_atomic, PROTOTYPE_EFFORT_PHASE_PROOF, finalization_credit
	);
	if (finalization_effort != 0) return finalization_effort;
	ctx->effort_atomic = 1;
	/* Surface `::` is a post-synthesis expectation check. Diagnose an
	 * incompatible wrapper before checked-classifier completion requires every
	 * successful occurrence to have a classifier. */
	if (compile_phase_check_ascriptions(ctx) != 0) {
		fprintf(stderr, "post-synthesis ascription checking failed\n");
		return -1;
	}
	if (ctx->had_error) {
		return -1;
	}
	if (operation_solver_complete_checked_classifiers(ctx) != 0) {
		fprintf(stderr, "compile pending failed: checked classifier completion\n");
		return -1;
	}
	if (operation_solver_close_match_motive_projections(ctx) != 0) {
		fprintf(stderr, "compile pending failed: Match motive closure\n");
		return -1;
	}
	/* The fixed point retains the occurrence-owned computation suffix across
	 * classifier passes. Refresh its solved lifecycle and effect equations before
	 * freezing residual state; no authority reconstruction is required here. */
	int final_computation_changed = 0;
	if (operation_solver_bind_computation_constraint_solutions(
			ctx, &final_computation_changed, 0
		) != 0 || operation_effect_generate_constraints(ctx, 1) != 0 ||
			prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0 ||
			compile_phase_record_residual_effect_constraints(ctx) != 0 ||
			compile_phase_record_residual_dependent_folds(ctx) != 0 ||
			compile_phase_record_residual_computation_fold_results(ctx) != 0) {
		fprintf(stderr, "compile pending failed: effect/commit/residual\n");
		return -1;
	}
	(void)final_computation_changed;
	operation_solver_refresh_constraint_states(ctx, ctx->metadata->effort.exhausted);
	if (prototype_typing_constraint_derive_sccs(
			&ctx->typing->constraints,
			(uint32_t)ctx->typing->context_projections.typed_projection_count
		) != 0 || operation_constraint_db_validate(ctx) != 0 ||
		operation_constraint_db_seal_topology(ctx->typing, ctx->metadata) != 0) {
		fprintf(stderr, "Layer T constraint topology sealing failed\n");
		return -1;
	}
	for (uint32_t constraint_id = prototype_typing_constraint_first_in_domain(
			&ctx->typing->constraints,
			OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER
		);
		constraint_id != PROTOTYPE_INVALID_ID;
		constraint_id = prototype_typing_constraint_next_in_domain(
			&ctx->typing->constraints, constraint_id
		)) {
		const struct prototype_typing_constraint* constraint =
			&ctx->typing->constraints.constraints[constraint_id];
		if (constraint->kind != OPERATION_CONSTRAINT_MOTIVE_EQUATION ||
			ctx->typing->constraints.solutions[constraint_id].state !=
				OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			continue;
		}
		int diagnostic_status = operation_motive_add_terminal_diagnostic(
			ctx,
			constraint->source_occurrence,
			constraint->owner_typed_projection,
			constraint->payload.classifier.goal.motive_case.case_index
		);
		if (diagnostic_status < 0) {
			return -1;
		}
		if (diagnostic_status == 0) {
			return -1;
		}
	}
	compile_metadata_refresh_runtime_capabilities(metadata, terms);
	if (metadata && metadata->compile_policy == PROTOTYPE_COMPILE_POLICY_STRICT &&
		(prototype_verification_db_count(&metadata->verification) != 0 ||
			operation_effect_unresolved_count(&ctx->typing->constraints) != 0)) {
		fprintf(stderr, "strict compilation retained unresolved verification state\n");
		return -1;
	}
	if (operation_constraint_db_assert_topology(ctx->typing, ctx->metadata) != 0) {
		return -1;
	}
	struct prototype_typed_occurrence_graph* occurrence_graph =
		prototype_compile_metadata_typed_occurrences(metadata);
	/* Accepted projection is the sole input to proof materialization. Building it
	 * after accepting source-solution Claims leaves stale classifier terms in the
	 * derivation DAG, even if a later publication pass computes the right tuple. */
	if (prototype_typing_pipeline_build_publication_view(
			ctx->typing,
			occurrence_graph,
			&metadata->contexts,
			&metadata->substitutions,
			terms,
			type_declarations,
			&metadata->typed_publication
		) != 0) {
		fprintf(stderr, "Layer T publication view construction failed\n");
		return -1;
	}
	if (compile_materialize_accepted_subject_equations(ctx) != 0) {
		fprintf(stderr, "Layer T publication subject materialization failed\n");
		return -1;
	}
	if (compile_materialize_accepted_classifier_equations(ctx) != 0) {
		fprintf(stderr, "Layer T publication classifier materialization failed\n");
		return -1;
	}
	if (prototype_typed_publication_view_seal(
			&metadata->typed_publication,
			occurrence_graph,
			ctx->typing->topology_revision,
			ctx->typing->context_projections.solution_revision
		) != 0) {
		fprintf(stderr, "Layer T accepted Context projection sealing failed\n");
		return -1;
	}
	struct prototype_judgement_occurrence_context_view accepted_contexts = {
		.publication = &metadata->typed_publication,
		.projection_authority = ctx,
		.lookup_projection_classifier =
			operation_solver_lookup_accepted_projection_classifier
	};
	ctx->proof_projection_view = &metadata->typed_publication;
	phase_started = compile_performance_clock_ns();
	if (operation_solver_materialize_judgements(ctx) != 0) {
		fprintf(stderr, "post-expectation judgement materialization failed\n");
		return -1;
	}
	metadata->proof_materialization_time_ns +=
		compile_performance_elapsed_ns(phase_started);
	if (prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0) {
		fprintf(stderr, "post-expectation judgement commit failed\n");
		return -1;
	}
	if (operation_constraint_db_assert_topology(ctx->typing, ctx->metadata) != 0) {
		return -1;
	}
	/* Totality evidence consumes only already accepted synthesis results. */
	if (prototype_judgement_publish_candidates(
			occurrence_graph, &accepted_contexts, judgement
		) != 0) {
		fprintf(stderr, "pre-expectation candidate publication failed\n");
		return -1;
	}
	phase_started = compile_performance_clock_ns();
	if (compile_phase_publish_termination_evidence(ctx) != 0) {
		fprintf(stderr, "pre-expectation termination evidence publication failed\n");
		return -1;
	}
	metadata->termination_evidence_time_ns +=
		compile_performance_elapsed_ns(phase_started);
	/* Close only the accepted Judgement DAG again; classifier and effect
	 * solutions remain frozen and cannot receive feedback from evidence. A
	 * publication round can expose a newly accepted substitution premise, so
	 * iterate until both Claims and Derivations are stable. */
	phase_started = compile_performance_clock_ns();
	int evidence_closed = 0;
	for (uint32_t round = 0; round < 16; ++round) {
		size_t before_claims = judgement->claim_count;
		size_t before_derivations = judgement->derivation_count;
		if (operation_solver_materialize_judgements(ctx) != 0 ||
			prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0 ||
			prototype_judgement_publish_candidates(
				occurrence_graph, &accepted_contexts, judgement
			) != 0) {
			fprintf(stderr, "post-evidence judgement closure failed round=%u\n", round);
			return -1;
		}
		if (judgement->claim_count == before_claims &&
			judgement->derivation_count == before_derivations &&
			!ctx->evidence_workspace.materialization_substitution_pending) {
			evidence_closed = 1;
			break;
		}
	}
	if (!evidence_closed) {
		fprintf(stderr, "post-evidence judgement closure did not stabilize\n");
		return -1;
	}
	metadata->evidence_closure_time_ns +=
		compile_performance_elapsed_ns(phase_started);
	if (compile_phase_check_expectations(ctx) != 0) {
		fprintf(stderr, "post-synthesis expectation checking failed\n");
		return -1;
	}
	if (prototype_judgement_delta_has_pending_classifier_state(&ctx->judgement_delta) != 0) {
		fprintf(stderr, "judgement delta retained pending classifier state\n");
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 || prototype_internal_canonicalize_constructor_owner_refs(
			terms, type_declarations, 0
		) != 0) {
		fprintf(stderr, "post-expectation type-view canonicalization failed\n");
		return -1;
	}
	if (prototype_judgement_expand_primitives(judgement, terms) != 0) {
		fprintf(stderr, "primitive judgement expansion failed\n");
		return -1;
	}
	if (prototype_term_erase_constructor_view_owners(terms) != 0) {
		fprintf(stderr, "constructor owner erasure failed\n");
		return -1;
	}
	if (prototype_judgement_publish_candidates(
			occurrence_graph,
			&accepted_contexts,
			judgement
		) != 0) {
		fprintf(stderr, "P0 proof candidate publication failed\n");
		return -1;
	}
	/* Normalization conversion expansion adds scoped substitution premises after
	 * the ordinary evidence closure above. Close those newly accepted edges
	 * before sealing and replaying the graph. */
	int normalization_evidence_closed = 0;
	for (uint32_t round = 0; round < 16; ++round) {
		size_t before_claims = judgement->claim_count;
		size_t before_derivations = judgement->derivation_count;
		if (operation_solver_materialize_judgements(ctx) != 0 ||
			prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0 ||
			prototype_judgement_publish_candidates(
				occurrence_graph, &accepted_contexts, judgement
			) != 0) {
			fprintf(
				stderr,
				"normalization evidence closure failed round=%u\n",
				round
			);
			return -1;
		}
		if (judgement->claim_count == before_claims &&
			judgement->derivation_count == before_derivations &&
			!ctx->evidence_workspace.materialization_substitution_pending) {
			normalization_evidence_closed = 1;
			break;
		}
	}
	if (!normalization_evidence_closed) {
		fprintf(stderr, "normalization evidence closure did not stabilize\n");
		return -1;
	}
	if (operation_constraint_db_assert_topology(ctx->typing, ctx->metadata) != 0) {
		return -1;
	}
	if (prototype_typed_occurrence_graph_seal(
			occurrence_graph, terms, &metadata->contexts
		) != 0) {
		fprintf(stderr, "typed-occurrence graph sealing failed before accepted replay\n");
		return -1;
	}
	int publication_evidence_closed = 0;
	for (uint32_t round = 0; round < 16; ++round) {
		size_t before_claims = judgement->claim_count;
		size_t before_derivations = judgement->derivation_count;
		if (operation_solver_materialize_publication_judgements(ctx) != 0 ||
			prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0 ||
			prototype_judgement_publish_candidates(
				occurrence_graph, &accepted_contexts, judgement
			) != 0) {
			fprintf(stderr,
				"accepted publication evidence closure failed round=%u\n",
				round
			);
			return -1;
		}
		if (judgement->claim_count == before_claims &&
			judgement->derivation_count == before_derivations) {
			publication_evidence_closed = 1;
			break;
		}
	}
	if (!publication_evidence_closed) {
		fprintf(stderr, "accepted publication evidence did not stabilize\n");
		return -1;
	}
	uint32_t mutable_occurrence_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(occurrence_graph);
	for (uint32_t occurrence_id = mutable_occurrence_start;
		occurrence_id < occurrence_graph->occurrence_count;
		++occurrence_id) {
		uint32_t publication_projection =
			ctx->typing->publication_typed_projection_for_occurrence[occurrence_id];
		uint32_t concrete_context;
		uint32_t substitution;
		int projection_status = operation_typed_projection_materialized_context(
			ctx, publication_projection, &concrete_context, &substitution
		);
		if (projection_status == PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
			/* The impossible refinement remains the selected semantic projection.
			 * Publication derives its addressable source Context from the solved
			 * parent action without inventing a second identity typing equation. */
			continue;
		} else if (projection_status != 0) {
			const struct prototype_typed_occurrence* occurrence =
				&occurrence_graph->occurrences[occurrence_id];
			fprintf(stderr,
				"Layer T source Context projection unresolved occurrence=%u "
				"projection=%u accepted-prefix=%zu transaction-prefix=%zu "
				"tag=%d ast=%u\n",
				occurrence_id, publication_projection,
				ctx->accepted_typing.occurrence_count,
				prototype_typed_occurrence_graph_transaction_start(
					occurrence_graph
				),
				occurrence->tag, occurrence->source_ast);
			return -1;
		}
		(void)concrete_context;
		(void)substitution;
	}
	if (operation_constraint_db_assert_topology(ctx->typing, ctx->metadata) != 0) {
		return -1;
	}
	phase_started = compile_performance_clock_ns();
	struct prototype_context_classifier_view accepted_classifier_view =
		operation_type_context_view(ctx);
	if (prototype_judgement_validate_accepted_graph(
			terms,
			type_declarations,
			intrinsic_environment,
			&metadata->contexts,
			&accepted_classifier_view,
			&metadata->substitutions,
			&metadata->dimension_operators,
			occurrence_graph,
			&accepted_contexts,
			judgement
		) != 0) {
		fprintf(stderr, "P0 accepted proof graph validation failed\n");
		return -1;
	}
	if (operation_solver_validate_accepted_motive_evidence(ctx) != 0) {
		fprintf(stderr, "accepted Match motive evidence validation failed\n");
		return -1;
	}
	if (operation_constraint_db_assert_topology(ctx->typing, ctx->metadata) != 0) {
		return -1;
	}
	metadata->accepted_replay_time_ns +=
		compile_performance_elapsed_ns(phase_started);
	if (prototype_typed_occurrence_graph_validate(
			occurrence_graph, terms, &metadata->contexts
		) != 0) {
		fprintf(stderr, "typed-occurrence graph validation failed after accepted replay\n");
		return -1;
	}
	if (prototype_constructor_curried_caches_validate(
			&type_declarations->semantic_schema,
			&type_declarations->constructor_classifier_cache,
			&accepted_classifier_view,
			terms
		) != 0) {
		fprintf(stderr, "constructor cache validation failed after accepted replay\n");
		return -1;
	}
	size_t discharged_compile_effect_equations = 0;
	if (prototype_verification_db_try_discharge_phase(
			&metadata->verification,
			terms,
			PROTOTYPE_VERIFICATION_PHASE_COMPILE,
			&discharged_compile_effect_equations
		) != 0) {
		fprintf(stderr, "compile-time effect-equation discharge failed\n");
		return -1;
	}
	(void)discharged_compile_effect_equations;
	if (compile_phase_publish_labels(ctx) != 0) {
		fprintf(stderr, "label publication failed\n");
		return -1;
	}
	if (ctx->had_error) {
		fprintf(stderr, "compilation retained an earlier lowering error\n");
		return -1;
	}
	if (prototype_typed_occurrence_graph_freeze(
			&metadata->typed_occurrences,
			terms,
			&metadata->contexts
		) != 0) {
		fprintf(stderr, "typed occurrence transaction commit failed\n");
		return -1;
	}
	if (operation_constraint_db_assert_topology(ctx->typing, ctx->metadata) != 0) {
		return -1;
	}
	ctx->effort_atomic = 0;
	driver->producer_phase = 4;
	}
	return 0;
}

struct compilation_image_root_set {
	struct prototype_compilation_image_root* roots;
	size_t count;
	size_t capacity;
	uint64_t revision;
};

struct prototype_compilation_image {
	struct prototype_ast_db* asts;
	struct prototype_core_pipeline* core;
	struct prototype_core_request_service core_requests;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_universe_db* universe;
	struct prototype_compile_metadata* metadata;
	struct symbol_table* symbols;
	const struct prototype_intrinsic_typing_environment* intrinsic_environment;
	int namespace_symbol_id;
	const struct prototype_artifact_interface* const* imported_interfaces;
	size_t imported_interface_count;
	struct compile_judgement_workspace* workspace;
	struct prototype_source_schedule_item* source_schedule_items;
	struct prototype_source_schedule source_schedule;
	struct prototype_source_lowering_slot* source_lowering_slots;
	struct prototype_source_lowering_edge* source_lowering_edges;
	struct prototype_source_lowering_binder* source_lowering_binders;
	struct prototype_source_lowering_scope_recipe* source_lowering_scope_recipes;
	struct prototype_source_lowering_root* source_lowering_roots;
	uint32_t* source_lowering_order;
	struct prototype_source_lowering_plan source_lowering_plan;
	struct prototype_source_core_slot_result* core_slot_results;
	struct prototype_source_core_binder_result* core_binder_results;
	struct prototype_source_core_handoff_builder core_source_results;
	struct prototype_source_core_handoff_view core_source_view;
	struct prototype_typing_source_assignment_result* typing_assignment_results;
	struct prototype_typing_source_expectation_result* typing_expectation_results;
	struct prototype_typing_source_type_result* typing_type_results;
	struct prototype_typing_source_results typing_source_results;
	struct prototype_typing_pipeline* typing;
	struct compile_driver_state driver;
	struct compile_context* context;
	struct compilation_image_root_set root_sets[
		PROTOTYPE_COMPILATION_IMAGE_ROOT_ROLE_COUNT
	];
	uint64_t replay_seed_digest;
	int outcome;
};

static void compilation_image_root_set_clear(
	struct compilation_image_root_set* set
) {
	if (!set) return;
	set->count = 0;
}

static int compilation_image_root_set_add(
	struct compilation_image_root_set* set,
	int kind,
	uint32_t id
) {
	if (!set || kind < PROTOTYPE_COMPILATION_IMAGE_ROOT_EQUATION ||
		kind > PROTOTYPE_COMPILATION_IMAGE_ROOT_CLAIM ||
		id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (set->count == set->capacity) {
		size_t capacity = set->capacity == 0 ? 16 : set->capacity * 2;
		if (capacity < set->capacity || capacity > SIZE_MAX / sizeof(*set->roots)) {
			return -1;
		}
		struct prototype_compilation_image_root* roots = realloc(
			set->roots, capacity * sizeof(*roots)
		);
		if (!roots) return -1;
		set->roots = roots;
		set->capacity = capacity;
	}
	set->roots[set->count++] = (struct prototype_compilation_image_root) {
		.kind = kind,
		.id = id
	};
	return 0;
}

static int compilation_image_root_compare(const void* left, const void* right) {
	const struct prototype_compilation_image_root* left_root = left;
	const struct prototype_compilation_image_root* right_root = right;
	if (left_root->kind != right_root->kind) {
		return left_root->kind < right_root->kind ? -1 : 1;
	}
	if (left_root->id == right_root->id) return 0;
	return left_root->id < right_root->id ? -1 : 1;
}

static int compilation_image_root_set_finish(
	struct compilation_image_root_set* set
) {
	if (!set || set->revision == UINT64_MAX) return -1;
	if (set->count > 1) {
		qsort(
			set->roots,
			set->count,
			sizeof(*set->roots),
			compilation_image_root_compare
		);
		size_t retained = 1;
		for (size_t i = 1; i < set->count; ++i) {
			if (compilation_image_root_compare(
					&set->roots[retained - 1], &set->roots[i]
				) != 0) {
				set->roots[retained++] = set->roots[i];
			}
		}
		set->count = retained;
	}
	set->revision++;
	return 0;
}

static uint64_t compilation_image_root_set_digest(
	const struct compilation_image_root_set* set
) {
	uint64_t digest = UINT64_C(14695981039346656037);
	if (!set) return 0;
	for (size_t i = 0; i < set->count; ++i) {
		uint32_t words[2] = {
			(uint32_t)set->roots[i].kind,
			set->roots[i].id
		};
		const unsigned char* bytes = (const unsigned char*)words;
		for (size_t j = 0; j < sizeof(words); ++j) {
			digest ^= bytes[j];
			digest *= UINT64_C(1099511628211);
		}
	}
	return digest;
}

static uint64_t compilation_image_digest_u64(uint64_t digest, uint64_t value) {
	for (size_t i = 0; i < sizeof(value); ++i) {
		digest ^= (unsigned char)(value >> (i * 8));
		digest *= UINT64_C(1099511628211);
	}
	return digest;
}

static uint64_t compilation_image_digest_proposition(
	uint64_t digest,
	const struct prototype_judgement_proposition* proposition
) {
	digest = compilation_image_digest_u64(digest, (uint32_t)proposition->kind);
	digest = compilation_image_digest_u64(
		digest, (uint32_t)proposition->authority_kind
	);
	digest = compilation_image_digest_u64(digest, proposition->authority_id);
	digest = compilation_image_digest_u64(digest, proposition->context_id);
	digest = compilation_image_digest_u64(digest, proposition->occurrence_id);
	digest = compilation_image_digest_u64(digest, proposition->subject);
	digest = compilation_image_digest_u64(digest, proposition->classifier);
	digest = compilation_image_digest_u64(
		digest, proposition->resource_usage_count
	);
	for (uint32_t i = 0; i < proposition->resource_usage_count; ++i) {
		digest = compilation_image_digest_u64(
			digest, proposition->resource_usage[i].binding_id
		);
		digest = compilation_image_digest_u64(
			digest, (uint32_t)proposition->resource_usage[i].grade
		);
	}
	return digest;
}

static int compilation_image_accepted_evidence_digest(
	const struct prototype_compilation_image* image,
	uint64_t* p_digest
) {
	if (!image || !p_digest || !image->judgement) return -1;
	const struct compilation_image_root_set* accepted = &image->root_sets[
		PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION
	];
	size_t claim_count = image->judgement->claim_count;
	unsigned char* reachable = claim_count == 0 ? NULL : calloc(claim_count, 1);
	uint32_t* stack = claim_count == 0 ? NULL : malloc(
		claim_count * sizeof(*stack)
	);
	if (claim_count != 0 && (!reachable || !stack)) {
		free(stack);
		free(reachable);
		return -1;
	}
	size_t stack_count = 0;
	for (size_t i = 0; i < accepted->count; ++i) {
		if (accepted->roots[i].kind != PROTOTYPE_COMPILATION_IMAGE_ROOT_CLAIM) {
			continue;
		}
		uint32_t claim_id = accepted->roots[i].id;
		if (claim_id >= claim_count) {
			free(stack);
			free(reachable);
			return -1;
		}
		if (!reachable[claim_id]) {
			reachable[claim_id] = 1;
			stack[stack_count++] = claim_id;
		}
	}
	while (stack_count != 0) {
		uint32_t claim_id = stack[--stack_count];
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(image->judgement, claim_id);
		if (!claim || claim->closure_rank == PROTOTYPE_INVALID_ID) {
			free(stack);
			free(reachable);
			return -1;
		}
		for (uint32_t derivation_id = claim->first_derivation;
			derivation_id != PROTOTYPE_INVALID_ID;
			derivation_id = image->judgement->derivations[
				derivation_id
			].next_same_conclusion) {
			const struct prototype_judgement_derivation* derivation =
				prototype_judgement_derivation_get(
					image->judgement, derivation_id
				);
			if (!derivation || derivation->conclusion_claim_id != claim_id) {
				free(stack);
				free(reachable);
				return -1;
			}
			if (derivation->closure_rank == PROTOTYPE_INVALID_ID) continue;
			for (uint32_t premise_id = 0;
				premise_id < derivation->premise_count;
				++premise_id) {
				uint32_t premise_claim = derivation->premises[premise_id].claim_id;
				if (premise_claim == PROTOTYPE_INVALID_ID) continue;
				if (premise_claim >= claim_count) {
					free(stack);
					free(reachable);
					return -1;
				}
				if (!reachable[premise_claim]) {
					reachable[premise_claim] = 1;
					stack[stack_count++] = premise_claim;
				}
			}
		}
	}
	uint64_t digest = UINT64_C(14695981039346656037);
	for (uint32_t claim_id = 0; claim_id < claim_count; ++claim_id) {
		if (!reachable[claim_id]) continue;
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(image->judgement, claim_id);
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(image->judgement, claim_id);
		if (!claim || !proposition) {
			free(stack);
			free(reachable);
			return -1;
		}
		digest = compilation_image_digest_u64(digest, claim_id);
		digest = compilation_image_digest_u64(digest, claim->closure_rank);
		digest = compilation_image_digest_proposition(digest, proposition);
		for (uint32_t derivation_id = claim->first_derivation;
			derivation_id != PROTOTYPE_INVALID_ID;
			derivation_id = image->judgement->derivations[
				derivation_id
			].next_same_conclusion) {
			const struct prototype_judgement_derivation* derivation =
				prototype_judgement_derivation_get(
					image->judgement, derivation_id
				);
			if (!derivation || derivation->conclusion_claim_id != claim_id) {
				free(stack);
				free(reachable);
				return -1;
			}
			if (derivation->closure_rank == PROTOTYPE_INVALID_ID) continue;
			digest = compilation_image_digest_u64(digest, derivation_id);
			digest = compilation_image_digest_u64(
				digest, (uint32_t)derivation->proof_kind
			);
			digest = compilation_image_digest_u64(
				digest, derivation->closure_rank
			);
			for (uint32_t word = 0; word < 4; ++word) {
				digest = compilation_image_digest_u64(
					digest, derivation->rule_data.words[word]
				);
			}
			digest = compilation_image_digest_u64(
				digest, (uint32_t)derivation->semantic_action_kind
			);
			digest = compilation_image_digest_u64(
				digest, derivation->semantic_action_id
			);
			digest = compilation_image_digest_u64(
				digest, derivation->premise_count
			);
			for (uint32_t premise_id = 0;
				premise_id < derivation->premise_count;
				++premise_id) {
				const struct prototype_judgement_premise_edge* premise =
					&derivation->premises[premise_id];
				digest = compilation_image_digest_u64(digest, premise->claim_id);
				digest = compilation_image_digest_u64(
					digest, premise->scoped_proposition_id
				);
				digest = compilation_image_digest_u64(
					digest, (uint32_t)premise->semantic_action_kind
				);
				digest = compilation_image_digest_u64(
					digest, premise->semantic_action_id
				);
				if (premise->claim_id == PROTOTYPE_INVALID_ID) {
					const struct prototype_judgement_proposition* scoped =
						prototype_judgement_proposition_get(
							image->judgement, premise->scoped_proposition_id
						);
					if (!scoped) {
						free(stack);
						free(reachable);
						return -1;
					}
					digest = compilation_image_digest_proposition(digest, scoped);
				}
			}
		}
	}
	free(stack);
	free(reachable);
	*p_digest = digest;
	return 0;
}

static void compilation_image_free_allocations(
	struct prototype_compilation_image* image
) {
	if (!image) return;
	for (int role = 1;
		role < PROTOTYPE_COMPILATION_IMAGE_ROOT_ROLE_COUNT;
		++role) {
		free(image->root_sets[role].roots);
	}
	free(image->source_lowering_order);
	free(image->source_lowering_roots);
	free(image->source_lowering_scope_recipes);
	free(image->source_lowering_binders);
	free(image->source_lowering_edges);
	free(image->source_lowering_slots);
	free(image->typing_type_results);
	free(image->typing_expectation_results);
	free(image->typing_assignment_results);
	free(image->core_binder_results);
	free(image->core_slot_results);
	free(image->source_schedule_items);
	free(image->context);
	free(image->workspace);
}

static int compilation_image_capture_lowered_roots(
	struct prototype_compilation_image* image
) {
	if (!image || !image->typing || !image->metadata) return -1;
	struct compilation_image_root_set* seed = &image->root_sets[
		PROTOTYPE_COMPILATION_IMAGE_ROOT_REPLAY_SEED
	];
	struct compilation_image_root_set* provenance = &image->root_sets[
		PROTOTYPE_COMPILATION_IMAGE_ROOT_SOURCE_PROVENANCE
	];
	compilation_image_root_set_clear(seed);
	compilation_image_root_set_clear(provenance);
	/* Replay roots name only immutable inputs. Equations are deterministic
	 * expansion of this topology and therefore belong to checkpoint progress,
	 * never to the RECOMPUTE seed. */
	for (uint32_t projection = 0;
		projection < image->typing->context_projections.typed_projection_count;
		++projection) {
		if (compilation_image_root_set_add(
				seed,
				PROTOTYPE_COMPILATION_IMAGE_ROOT_TYPED_PROJECTION,
				projection
			) != 0) {
			return -1;
		}
	}
	for (uint32_t type_id = 0;
		type_id < image->type_declarations->semantic_schema.type_count;
		++type_id) {
		if (compilation_image_root_set_add(
				seed,
				PROTOTYPE_COMPILATION_IMAGE_ROOT_TYPE_DECLARATION,
				type_id
			) != 0) {
			return -1;
		}
	}
	/* Every source occurrence is immutable topology at this boundary. Keeping
	 * it as optional provenance never makes it an executable publication root. */
	for (uint32_t occurrence = 0;
		occurrence < image->metadata->typed_occurrences.occurrence_count;
		++occurrence) {
		if (compilation_image_root_set_add(
				provenance,
				PROTOTYPE_COMPILATION_IMAGE_ROOT_TYPED_OCCURRENCE,
				occurrence
			) != 0) {
			return -1;
		}
	}
	return compilation_image_root_set_finish(seed) != 0 ? -1 :
		compilation_image_root_set_finish(provenance);
}

static int compilation_image_validate_match_resolution_topology(
	const struct prototype_compilation_image* image
) {
	if (!image || !image->typing || !image->metadata) return -1;
	const struct prototype_typed_occurrence_graph* graph =
		&image->metadata->typed_occurrences;
	uint8_t* covered = graph->case_count == 0 ? NULL :
		calloc(graph->case_count, sizeof(*covered));
	if (graph->case_count != 0 && !covered) return -1;
	for (uint32_t goal_id = 0;
		goal_id < image->typing->seed.match_resolution_goal_count;
		++goal_id) {
		const struct prototype_typing_match_resolution_goal* goal =
			&image->typing->seed.match_resolution_goals[goal_id];
		if (goal->match_operation >= graph->occurrence_count ||
			goal->scrutinee_operation >= graph->occurrence_count) {
			free(covered);
			return -1;
		}
		const struct prototype_typed_occurrence* match =
			&graph->occurrences[goal->match_operation];
		uint32_t scrutinee_occurrence = PROTOTYPE_INVALID_ID;
		if (match->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			prototype_typed_occurrence_graph_child(
				graph,
				goal->match_operation,
				PROTOTYPE_TERM_CHILD_SCRUTINEE,
				0,
				&scrutinee_occurrence
			) != 0 || scrutinee_occurrence != goal->scrutinee_operation ||
			goal->case_index >= match->case_count ||
			match->first_case >= graph->case_count ||
			goal->case_index >= graph->case_count - match->first_case) {
			free(covered);
			return -1;
		}
		uint32_t case_id = match->first_case + goal->case_index;
		const struct prototype_typed_occurrence_match_case* operation_case =
			&graph->cases[case_id];
		if (covered[case_id] || operation_case->case_label_symbol_id !=
				goal->constructor_symbol_id) {
			free(covered);
			return -1;
		}
		covered[case_id] = 1;
	}
	for (uint32_t case_id = 0; case_id < graph->case_count; ++case_id) {
		if (!covered[case_id]) {
			free(covered);
			return -1;
		}
	}
	free(covered);
	return 0;
}

static int compilation_image_capture_checkpoint_roots(
	struct prototype_compilation_image* image
) {
	if (!image || !image->typing || !image->judgement) return -1;
	struct compilation_image_root_set* checkpoint = &image->root_sets[
		PROTOTYPE_COMPILATION_IMAGE_ROOT_CHECKPOINT_PROGRESS
	];
	compilation_image_root_set_clear(checkpoint);
	for (uint32_t equation = 0;
		equation < image->typing->constraints.constraint_count;
		++equation) {
		if (compilation_image_root_set_add(
				checkpoint, PROTOTYPE_COMPILATION_IMAGE_ROOT_EQUATION, equation
			) != 0) {
			return -1;
		}
	}
	for (uint32_t claim = 0; claim < image->judgement->claim_count; ++claim) {
		if (compilation_image_root_set_add(
				checkpoint, PROTOTYPE_COMPILATION_IMAGE_ROOT_CLAIM, claim
			) != 0) {
			return -1;
		}
	}
	return compilation_image_root_set_finish(checkpoint);
}

static int compilation_image_export_evidence_occurrence(
	const struct prototype_compile_metadata* metadata,
	uint32_t occurrence,
	uint32_t* p_evidence_occurrence
) {
	if (!metadata || !p_evidence_occurrence) return -1;
	for (size_t depth = 0;
		depth <= metadata->typed_occurrences.occurrence_count;
		++depth) {
		if (occurrence >= metadata->typed_occurrences.occurrence_count) return -1;
		const struct prototype_typed_occurrence* node =
			&metadata->typed_occurrences.occurrences[occurrence];
		if (node->tag != PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			*p_evidence_occurrence = occurrence;
			return 0;
		}
		occurrence = node->wrapped_occurrence;
	}
	return -1;
}

static int compilation_image_publication_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t occurrence,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_claim_id
) {
	if (!judgement || !p_claim_id || occurrence == PROTOTYPE_INVALID_ID ||
		classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t fallback = PROTOTYPE_INVALID_ID;
	for (uint32_t claim_id = 0; claim_id < judgement->claim_count; ++claim_id) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, claim_id);
		const struct prototype_judgement_proposition* proposition = claim ?
			prototype_judgement_claim_proposition(judgement, claim_id) : NULL;
		if (!claim || !proposition || claim->closure_rank == PROTOTYPE_INVALID_ID ||
			proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proposition->context_id != context_id ||
			proposition->subject != subject ||
			proposition->classifier != classifier) {
			continue;
		}
		if (proposition->authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
			proposition->authority_id == occurrence &&
			proposition->occurrence_id == occurrence) {
			*p_claim_id = claim_id;
			return 0;
		}
		if (fallback == PROTOTYPE_INVALID_ID) fallback = claim_id;
	}
	if (fallback == PROTOTYPE_INVALID_ID) return 1;
	*p_claim_id = fallback;
	return 0;
}

static int compilation_image_capture_accepted_roots(
	struct prototype_compilation_image* image
) {
	if (!image || !image->typing || !image->metadata || !image->judgement ||
		!image->metadata->typed_publication.sealed) {
		return -1;
	}
	struct compilation_image_root_set* accepted = &image->root_sets[
		PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION
	];
	compilation_image_root_set_clear(accepted);
	/* Kind-major insertion preserves one deterministic ordering without a
	 * separate sorting/canonicalization authority. */
	for (uint32_t i = 0; i < image->typing->seed.publication_input_count; ++i) {
		uint32_t occurrence = image->typing->seed.publication_inputs[i].exposed_occurrence;
		uint32_t evidence_occurrence;
		if (compilation_image_export_evidence_occurrence(
				image->metadata, occurrence, &evidence_occurrence
			) != 0) {
			return -1;
		}
		uint32_t projection = image->typing->
			publication_typed_projection_for_occurrence[evidence_occurrence];
		if (projection == PROTOTYPE_INVALID_ID || projection >=
			image->typing->context_projections.typed_projection_count ||
			compilation_image_root_set_add(
				accepted,
				PROTOTYPE_COMPILATION_IMAGE_ROOT_TYPED_PROJECTION,
				projection
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < image->metadata->type_export_count; ++i) {
		uint32_t type_id = image->metadata->type_exports[i].type_id;
		if (type_id >= image->type_declarations->semantic_schema.type_count ||
			compilation_image_root_set_add(
				accepted,
				PROTOTYPE_COMPILATION_IMAGE_ROOT_TYPE_DECLARATION,
				type_id
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < image->typing->seed.publication_input_count; ++i) {
		uint32_t occurrence = image->typing->seed.publication_inputs[i].exposed_occurrence;
		uint32_t evidence_occurrence;
		uint32_t concrete_context;
		if (compilation_image_export_evidence_occurrence(
				image->metadata, occurrence, &evidence_occurrence
			) != 0) {
			return -1;
		}
		const struct prototype_typed_publication_projection* publication =
			prototype_typed_publication_view_get(
				&image->metadata->typed_publication,
				evidence_occurrence,
				&concrete_context
			);
		uint32_t claim;
		if (!publication || compilation_image_publication_claim(
				image->judgement,
				evidence_occurrence,
				concrete_context,
				publication->subject,
				publication->classifier,
				&claim
			) != 0 || compilation_image_root_set_add(
				accepted, PROTOTYPE_COMPILATION_IMAGE_ROOT_CLAIM, claim
			) != 0) {
			return -1;
		}
	}
	return compilation_image_root_set_finish(accepted);
}

static int compile_source_results_seed_accepted(
	const struct prototype_ast_db* asts,
	const struct prototype_source_lowering_plan* source_plan,
	const struct prototype_core_pipeline* core,
	const struct prototype_compile_metadata* metadata,
	struct prototype_source_core_handoff_builder* core_results,
	struct prototype_typing_source_results* typing_results
) {
	const struct prototype_term_db* terms =
		prototype_core_pipeline_terms_const(core);
	if (!asts || !source_plan || !terms || !metadata || !core_results ||
		!typing_results) {
		return -1;
	}
	for (uint32_t assignment_id = 0;
		assignment_id < (uint32_t)asts->assignment_count;
		++assignment_id) {
		const struct prototype_ast_term_assignment_def* assignment =
			&asts->assignments[assignment_id];
		const struct prototype_compile_label* found = NULL;
		for (size_t label_id = 0; label_id < metadata->label_count; ++label_id) {
			const struct prototype_compile_label* label = &metadata->labels[label_id];
			if (label->name_symbol_id != assignment->name_symbol_id) continue;
			if (found) return -1;
			found = label;
		}
		if (!found) continue;
		if (found->term >= terms->term_count || found->exposed_occurrence >=
			metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		struct prototype_typing_source_assignment_result* typing_result =
			prototype_typing_source_assignment_result_mut(typing_results, assignment_id);
		uint32_t slot_id = prototype_source_lowering_plan_term_slot(
			source_plan, assignment->ast
		);
		if (!typing_result || prototype_source_core_slot_record_formed(
				core_results,
				slot_id,
				found->term,
				PROTOTYPE_INVALID_ID,
				terms->normalization_graph_revision
			) != 0) {
			return -1;
		}
		typing_result->classifier = found->exposed_classifier;
		typing_result->occurrence = found->exposed_occurrence;
		typing_result->state = PROTOTYPE_TYPING_SOURCE_CHECKED;
		typing_result->published = 1;
	}
	for (uint32_t expectation_id = 0;
		expectation_id < (uint32_t)asts->expectation_count;
		++expectation_id) {
		const struct prototype_ast_type_expectation_def* expectation =
			&asts->expectations[expectation_id];
		struct prototype_typing_source_expectation_result* result =
			prototype_typing_source_expectation_result_mut(
				typing_results, expectation_id
			);
		if (!result) return -1;
		for (size_t label_id = 0; label_id < metadata->label_count; ++label_id) {
			const struct prototype_compile_label* label = &metadata->labels[label_id];
			if (label->name_symbol_id != expectation->name_symbol_id) continue;
			uint32_t classifier = label->expectation_classifier != PROTOTYPE_INVALID_ID ?
				label->expectation_classifier : label->exposed_classifier;
			if (result->state == PROTOTYPE_TYPING_SOURCE_CHECKED &&
				result->classifier != classifier) {
				return -1;
			}
			result->classifier = classifier;
			result->state = PROTOTYPE_TYPING_SOURCE_CHECKED;
		}
	}
	for (uint32_t ast_type_id = 0;
		ast_type_id < (uint32_t)asts->type_def_count;
		++ast_type_id) {
		const struct prototype_ast_type_def* ast_type = &asts->type_defs[ast_type_id];
		struct prototype_typing_source_type_result* result =
			prototype_typing_source_type_result_mut(typing_results, ast_type_id);
		if (!result) return -1;
		for (size_t export_id = 0; export_id < metadata->type_export_count; ++export_id) {
			const struct prototype_compile_type_export* export =
				&metadata->type_exports[export_id];
			if (export->name_symbol_id != ast_type->name_symbol_id) continue;
			if (result->state == PROTOTYPE_TYPING_SOURCE_CHECKED &&
				result->type_declaration_id != export->type_id) {
				return -1;
			}
			result->type_declaration_id = export->type_id;
			result->state = PROTOTYPE_TYPING_SOURCE_CHECKED;
		}
	}
	return 0;
}

int prototype_compilation_image_create(
	struct prototype_ast_db* asts,
	struct prototype_core_pipeline* core,
	struct prototype_typing_pipeline* typing,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_universe_db* universe,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct prototype_compilation_image** p_image
) {
	if (!asts || !core || !core->initialized || !typing ||
		!type_declarations || !judgement || !universe || !metadata ||
		!intrinsic_environment || !p_image ||
		metadata->typed_occurrences.transaction_active) {
		return -1;
	}
	*p_image = NULL;
	struct prototype_compilation_image* image =
		calloc(1, sizeof(*image));
	if (!image) return -1;
	image->workspace = calloc(1, sizeof(*image->workspace));
	image->context = calloc(1, sizeof(*image->context));
	image->typing_assignment_results = asts->assignment_count == 0 ? NULL :
		calloc(asts->assignment_count, sizeof(*image->typing_assignment_results));
	image->typing_expectation_results = asts->expectation_count == 0 ? NULL :
		calloc(asts->expectation_count, sizeof(*image->typing_expectation_results));
	image->typing_type_results = asts->type_def_count == 0 ? NULL :
		calloc(asts->type_def_count, sizeof(*image->typing_type_results));
	size_t source_schedule_capacity =
		prototype_source_schedule_required_capacity(asts);
	if (source_schedule_capacity == SIZE_MAX) {
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	if (source_schedule_capacity > 0) {
		image->source_schedule_items = calloc(
			source_schedule_capacity, sizeof(*image->source_schedule_items)
		);
	}
	image->typing = typing;
	if (!image->workspace || !image->context ||
		(asts->assignment_count > 0 && !image->typing_assignment_results) ||
		(asts->expectation_count > 0 && !image->typing_expectation_results) ||
		(asts->type_def_count > 0 && !image->typing_type_results) ||
		(source_schedule_capacity > 0 && !image->source_schedule_items)) {
		fprintf(stderr, "compile producer image allocation failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	if (prototype_source_schedule_build(
			asts,
			image->source_schedule_items,
			source_schedule_capacity,
			&image->source_schedule
		) != 0) {
		fprintf(stderr, "compile producer source schedule construction failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	struct prototype_source_lowering_plan_capacity source_plan_capacity;
	if (prototype_source_lowering_plan_measure(
			asts, &image->source_schedule, &source_plan_capacity
		) != 0) {
		fprintf(stderr, "compile producer source lowering plan measurement failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	image->source_lowering_slots = source_plan_capacity.slot_count == 0 ? NULL :
		calloc(source_plan_capacity.slot_count, sizeof(*image->source_lowering_slots));
	image->source_lowering_edges = source_plan_capacity.edge_count == 0 ? NULL :
		calloc(source_plan_capacity.edge_count, sizeof(*image->source_lowering_edges));
	image->source_lowering_binders = source_plan_capacity.binder_count == 0 ? NULL :
		calloc(source_plan_capacity.binder_count, sizeof(*image->source_lowering_binders));
	image->source_lowering_scope_recipes =
		source_plan_capacity.scope_recipe_count == 0 ? NULL : calloc(
			source_plan_capacity.scope_recipe_count,
			sizeof(*image->source_lowering_scope_recipes)
		);
	image->source_lowering_roots = source_plan_capacity.root_count == 0 ? NULL :
		calloc(source_plan_capacity.root_count, sizeof(*image->source_lowering_roots));
	image->source_lowering_order = source_plan_capacity.slot_count == 0 ? NULL :
		calloc(source_plan_capacity.slot_count, sizeof(*image->source_lowering_order));
	image->core_slot_results = source_plan_capacity.slot_count == 0 ? NULL :
		calloc(source_plan_capacity.slot_count, sizeof(*image->core_slot_results));
	image->core_binder_results = source_plan_capacity.binder_count == 0 ? NULL :
		calloc(source_plan_capacity.binder_count, sizeof(*image->core_binder_results));
	int source_plan_allocation_failed = 0;
	if (source_plan_capacity.slot_count > 0) {
		source_plan_allocation_failed = !image->source_lowering_slots ||
			!image->source_lowering_order || !image->core_slot_results;
	}
	if (!source_plan_allocation_failed && source_plan_capacity.edge_count > 0) {
		source_plan_allocation_failed = !image->source_lowering_edges;
	}
	if (!source_plan_allocation_failed && source_plan_capacity.binder_count > 0) {
		source_plan_allocation_failed = !image->source_lowering_binders ||
			!image->core_binder_results;
	}
	if (!source_plan_allocation_failed &&
		source_plan_capacity.scope_recipe_count > 0) {
		source_plan_allocation_failed = !image->source_lowering_scope_recipes;
	}
	if (!source_plan_allocation_failed && source_plan_capacity.root_count > 0) {
		source_plan_allocation_failed = !image->source_lowering_roots;
	}
	if (source_plan_allocation_failed) {
		fprintf(stderr, "compile producer source lowering allocation failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	if (prototype_source_lowering_plan_build(
			asts,
			&image->source_schedule,
			image->source_lowering_slots,
			source_plan_capacity.slot_count,
			image->source_lowering_edges,
			source_plan_capacity.edge_count,
			image->source_lowering_binders,
			source_plan_capacity.binder_count,
			image->source_lowering_scope_recipes,
			source_plan_capacity.scope_recipe_count,
			image->source_lowering_roots,
			source_plan_capacity.root_count,
			image->source_lowering_order,
			source_plan_capacity.slot_count,
			&image->source_lowering_plan
		) != 0) {
		fprintf(stderr, "compile producer source lowering plan construction failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	if (prototype_source_core_handoff_builder_init(
			&image->core_source_results,
			image->core_slot_results,
			source_plan_capacity.slot_count,
			image->core_binder_results,
			source_plan_capacity.binder_count,
			image->source_lowering_plan.source_fingerprint
		) != 0) {
		fprintf(stderr, "compile producer Layer C source-result initialization failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	if (prototype_typing_source_results_init(
			&image->typing_source_results,
			image->typing_assignment_results,
			asts->assignment_count,
			image->typing_expectation_results,
			asts->expectation_count,
			image->typing_type_results,
			asts->type_def_count
		) != 0) {
		fprintf(stderr, "compile producer Layer T source-result initialization failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	if (compile_source_results_seed_accepted(
			asts,
			&image->source_lowering_plan,
			core,
			metadata,
			&image->core_source_results,
			&image->typing_source_results
		) != 0) {
		fprintf(stderr, "compile producer accepted source-result seeding failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	/* Structural lowering already built this image's Layer T topology. Preserve
	 * it while removing every semantic transition before the first solve call. */
	if (prototype_typing_pipeline_reset_image_progress(image->typing) != 0) {
		fprintf(stderr, "compile producer image progress reset failed\n");
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	image->asts = asts;
	image->core = core;
	prototype_core_request_service_init(&image->core_requests, image->core);
	image->type_declarations = type_declarations;
	image->judgement = judgement;
	image->universe = universe;
	image->metadata = metadata;
	image->symbols = symbols;
	image->intrinsic_environment = intrinsic_environment;
	image->namespace_symbol_id = namespace_symbol_id;
	image->imported_interfaces = imported_interfaces;
	image->imported_interface_count = imported_interface_count;
	if (prototype_typing_pipeline_begin_transaction(image->typing) != 0 ||
		prototype_core_pipeline_begin_transaction(image->core) != 0) {
		if (image->typing->transaction_active) {
			(void)prototype_typing_pipeline_rollback_transaction(image->typing);
		}
		compilation_image_free_allocations(image);
		free(image);
		return -1;
	}
	*p_image = image;
	return 0;
}

static void compilation_image_rollback(
	struct prototype_compilation_image* image
) {
	if (!image) return;
	if (image->metadata &&
		image->metadata->typed_occurrences.transaction_active) {
		(void)prototype_typed_occurrence_graph_rollback_transaction(
			&image->metadata->typed_occurrences
		);
	}
	if (image->core && image->core->transaction_active) {
		(void)prototype_core_pipeline_rollback_transaction(image->core);
	}
	if (image->typing && image->typing->transaction_active) {
		(void)prototype_typing_pipeline_rollback_transaction(image->typing);
	}
}

static int compilation_image_run(
	struct prototype_compilation_image* image,
	uint64_t additional_effort,
	int stop_after_open_image,
	struct prototype_compilation_image_report* p_report
) {
	int source_phase = image && image->driver.producer_phase == 0;
	if (!image || !p_report || image->outcome != 0 ||
		(source_phase &&
		 prototype_source_lowering_plan_validate(
			&image->source_lowering_plan
		 ) != 0) ||
		(!stop_after_open_image && prototype_effort_add_credits(
			&image->metadata->effort, additional_effort
		) != 0)) {
		return -1;
	}
	uint64_t effort_before = image->metadata->effort.used;
	int status = compile_pending_with_workspace(
		image->asts,
		image->core,
		image->type_declarations,
		image->judgement,
		image->metadata,
		image->symbols,
		image->intrinsic_environment,
		image->namespace_symbol_id,
		image->imported_interfaces,
		image->imported_interface_count,
		image->workspace,
		&image->driver,
		&image->core_requests,
			&image->source_schedule,
			&image->source_lowering_plan,
			&image->core_source_results,
			&image->core_source_view,
			&image->typing_source_results,
		image->typing,
		image->context,
		stop_after_open_image
	);
	if (source_phase &&
		prototype_source_lowering_plan_validate(
			&image->source_lowering_plan
		) != 0) {
		fprintf(stderr, "compile producer mutated sealed source epoch\n");
		status = -1;
	}
	p_report->phase = image->driver.producer_phase;
	p_report->effort_used = image->metadata->effort.used - effort_before;
	if (status == 2 && stop_after_open_image &&
		image->driver.producer_phase == 1) {
		if (compilation_image_validate_match_resolution_topology(image) != 0 ||
			compilation_image_capture_lowered_roots(image) != 0) {
			compilation_image_rollback(image);
			image->outcome = PROTOTYPE_COMPILATION_IMAGE_REJECTED;
			p_report->status = image->outcome;
			return 0;
		}
		image->replay_seed_digest = prototype_typing_seed_state_digest(
			&image->typing->seed
		);
		if (image->replay_seed_digest == 0) {
			compilation_image_rollback(image);
			image->outcome = PROTOTYPE_COMPILATION_IMAGE_REJECTED;
			p_report->status = image->outcome;
			return 0;
		}
		/* Source syntax has been fully consumed. Keeping these pointers reachable
		 * from the Solver context would make an in-memory source compile stronger
		 * than a resumed .a image and conceal replay-seed omissions. */
		image->context->asts = NULL;
		image->context->source_schedule = NULL;
		image->context->source_plan = NULL;
		image->context->core_source_results = NULL;
		image->context->core_source_view = NULL;
		image->context->typing_source_results = NULL;
		p_report->status = PROTOTYPE_COMPILATION_IMAGE_OPEN;
		return 0;
	}
	if (!source_phase && image->replay_seed_digest !=
			prototype_typing_seed_state_digest(&image->typing->seed)) {
		fprintf(stderr, "solve mutated immutable compilation-image seed\n");
		compilation_image_rollback(image);
		image->outcome = PROTOTYPE_COMPILATION_IMAGE_REJECTED;
		p_report->status = image->outcome;
		return 0;
	}
	if (!source_phase &&
		compilation_image_validate_match_resolution_topology(image) != 0) {
		fprintf(stderr, "solve invalidated Match resolution seed topology\n");
		compilation_image_rollback(image);
		image->outcome = PROTOTYPE_COMPILATION_IMAGE_REJECTED;
		p_report->status = image->outcome;
		return 0;
	}
	if (status == 0) {
		if (prototype_universe_build_closed(
				image->universe,
				image->type_declarations,
				prototype_core_pipeline_terms_const(image->core),
				&image->metadata->typed_occurrences,
				image->judgement
			) != 0) {
			compilation_image_rollback(image);
			image->outcome = PROTOTYPE_COMPILATION_IMAGE_REJECTED;
			p_report->status = image->outcome;
			return 0;
		}
		if (!image->typing->transaction_active ||
			image->typing->transaction_revision == UINT64_MAX ||
			!image->core->transaction_active ||
			image->core->transaction_revision == UINT64_MAX ||
			compilation_image_capture_checkpoint_roots(image) != 0 ||
			compilation_image_capture_accepted_roots(image) != 0 ||
			prototype_typing_pipeline_commit_transaction(image->typing) != 0 ||
			prototype_core_pipeline_commit_transaction(image->core) != 0) {
			return -1;
		}
		image->outcome = PROTOTYPE_COMPILATION_IMAGE_SOLVED;
		p_report->status = image->outcome;
		return 0;
	}
	if (status == 1 && image->metadata->effort.exhausted) {
		if (compilation_image_capture_checkpoint_roots(image) != 0) return -1;
		p_report->status = PROTOTYPE_COMPILATION_IMAGE_PAUSED;
		return 0;
	}
	compilation_image_rollback(image);
	image->outcome = PROTOTYPE_COMPILATION_IMAGE_REJECTED;
	p_report->status = image->outcome;
	return 0;
}

int prototype_compilation_image_lower(
	struct prototype_compilation_image* image,
	struct prototype_compilation_image_report* p_report
) {
	if (!image || !p_report || image->outcome != 0 ||
		image->driver.producer_phase > 1) {
		return -1;
	}
	if (image->driver.producer_phase == 1) {
		*p_report = (struct prototype_compilation_image_report) {
			.status = PROTOTYPE_COMPILATION_IMAGE_OPEN,
			.phase = 1,
			.effort_used = 0
		};
		return 0;
	}
	return compilation_image_run(
		image, 0, 1, p_report
	);
}

int prototype_compilation_image_solve(
	struct prototype_compilation_image* image,
	uint64_t additional_effort,
	struct prototype_compilation_image_report* p_report
) {
	if (!image || !p_report || image->outcome != 0 ||
		image->driver.producer_phase < 1 || image->context->asts ||
		image->context->source_schedule || image->context->source_plan ||
		image->context->core_source_results || image->context->core_source_view ||
		image->context->typing_source_results) {
		return -1;
	}
	return compilation_image_run(
		image, additional_effort, 0, p_report
	);
}

int prototype_compilation_image_seal(
	const struct prototype_compilation_image* image,
	struct prototype_compilation_image_seal* p_seal
) {
	if (!image || !p_seal ||
		image->outcome != PROTOTYPE_COMPILATION_IMAGE_SOLVED ||
		image->driver.producer_phase != 4 || !image->core || !image->typing ||
		!image->judgement || !image->universe || !image->metadata ||
		image->core->transaction_active ||
		image->typing->transaction_active ||
		image->metadata->typed_occurrences.transaction_active ||
		!image->metadata->typed_occurrences.sealed ||
		!image->metadata->typed_occurrences.frozen ||
		image->root_sets[
			PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION
		].revision == 0) {
		return -1;
	}
	uint64_t expected_digest = 0;
	uint64_t actual_digest = 0;
	if (prototype_typing_pipeline_validate_constraint_topology(
			image->typing, &expected_digest, &actual_digest
		) != 0 || expected_digest != actual_digest ||
		!image->metadata->constraint_topology_sealed ||
		image->metadata->constraint_topology_digest != expected_digest ||
		prototype_typed_occurrence_graph_validate(
			&image->metadata->typed_occurrences,
			prototype_core_pipeline_terms_const(image->core),
			&image->metadata->contexts
		) != 0 || compilation_image_validate_match_resolution_topology(image) != 0 ||
		prototype_universe_validate_replay(
			image->universe,
			image->type_declarations,
			prototype_core_pipeline_terms_const(image->core),
			&image->metadata->typed_occurrences,
			image->judgement
		) != 0) {
		return -1;
	}
	uint64_t accepted_evidence_digest;
	if (compilation_image_accepted_evidence_digest(
			image, &accepted_evidence_digest
		) != 0) {
		return -1;
	}
	*p_seal = (struct prototype_compilation_image_seal) {
		.constraint_topology_digest = expected_digest,
		.semantic_transition_count =
			image->typing->constraints.semantic_transition_count,
		.core_transaction_revision = image->core->transaction_revision,
		.typing_transaction_revision = image->typing->transaction_revision,
		.typing_topology_revision = image->typing->topology_revision,
		.judgement_semantic_revision = image->judgement->semantic_revision,
		.term_count = image->core->terms.term_count,
		.typed_occurrence_count =
			image->metadata->typed_occurrences.occurrence_count,
		.claim_count = image->judgement->claim_count,
		.derivation_count = image->judgement->derivation_count,
		.accepted_root_revision = image->root_sets[
			PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION
		].revision,
		.accepted_root_count = image->root_sets[
			PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION
		].count,
		.accepted_root_digest = compilation_image_root_set_digest(
			&image->root_sets[
				PROTOTYPE_COMPILATION_IMAGE_ROOT_ACCEPTED_PUBLICATION
			]
		),
		.accepted_evidence_digest = accepted_evidence_digest
	};
	return 0;
}

int prototype_compilation_image_roots(
	const struct prototype_compilation_image* image,
	int role,
	struct prototype_compilation_image_root_view* p_view
) {
	if (!image || !p_view || role <= 0 ||
		role >= PROTOTYPE_COMPILATION_IMAGE_ROOT_ROLE_COUNT) {
		return -1;
	}
	const struct compilation_image_root_set* set = &image->root_sets[role];
	*p_view = (struct prototype_compilation_image_root_view) {
		.roots = set->roots,
		.count = set->count,
		.revision = set->revision
	};
	return 0;
}

void prototype_compilation_image_destroy(
	struct prototype_compilation_image* image
) {
	if (!image) return;
	if (image->outcome == 0) compilation_image_rollback(image);
	compilation_image_free_allocations(image);
	free(image);
}

int prototype_ast_compile_pending_with_imports(
	struct prototype_ast_db* asts,
	struct prototype_core_pipeline* core,
	struct prototype_typing_pipeline* typing,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_universe_db* universe,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_typing_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
) {
	struct prototype_compilation_image* image = NULL;
	if (prototype_compilation_image_create(
		asts,
		core,
		typing,
			type_declarations,
			judgement,
			universe,
			metadata,
		symbols,
		intrinsic_environment,
		namespace_symbol_id,
		imported_interfaces,
		imported_interface_count,
		&image
	) != 0) {
		return -1;
	}
	struct prototype_compilation_image_report report;
	int lower_status = prototype_compilation_image_lower(image, &report);
	if (lower_status == 0 &&
		report.status == PROTOTYPE_COMPILATION_IMAGE_OPEN) {
		lower_status = prototype_compilation_image_solve(
			image, 0, &report
		);
	}
	struct prototype_compilation_image_seal seal;
	if (lower_status == 0 &&
		report.status == PROTOTYPE_COMPILATION_IMAGE_SOLVED &&
		prototype_compilation_image_seal(image, &seal) != 0) {
		lower_status = -1;
	}
	prototype_compilation_image_destroy(image);
	if (lower_status != 0) return -1;
	if (report.status == PROTOTYPE_COMPILATION_IMAGE_REJECTED) {
		fprintf(stderr, "compilation image rejected phase=%d\n", report.phase);
	}
	return report.status == PROTOTYPE_COMPILATION_IMAGE_SOLVED ? 0 :
		(report.status == PROTOTYPE_COMPILATION_IMAGE_PAUSED ? 1 : -1);
}
