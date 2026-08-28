static int compile_phase_infer_pending_types(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}

	for (;;) {
		size_t before_operation_classifiers = count_classified_operations(ctx);
		uint64_t before_classifier_revision =
			ctx->classifier_solver.solution_revision;
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
				ctx->type_declarations,
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
		if (operation_solver_refresh_computation_constraint_operands(ctx) != 0) {
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
		if (operation_effect_generate_constraints(ctx) != 0) {
			fprintf(stderr, "typing fixed point: effect constraint generation failed\n");
			return -1;
		}
		if (count_classified_operations(ctx) == before_operation_classifiers &&
			ctx->classifier_solver.solution_revision ==
				before_classifier_revision &&
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
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		uint32_t operation = ctx->pending_match_typings[i].operation;
		if (operation >= ctx->metadata->typed_occurrences.occurrence_count ||
			ctx->classifier_solver.classifier_metas[operation].classifier ==
				PROTOTYPE_INVALID_ID) {
			fprintf(
				stderr,
				"pending Match classifier missing index=%u operation=%u operation-count=%zu classifier=%u\n",
				i,
				operation,
				ctx->metadata->typed_occurrences.occurrence_count,
				operation < ctx->metadata->typed_occurrences.occurrence_count ?
					ctx->classifier_solver.classifier_metas[operation].classifier :
					PROTOTYPE_INVALID_ID
			);
			if (operation < ctx->metadata->typed_occurrences.occurrence_count) {
				const struct prototype_typed_occurrence* node =
					&ctx->metadata->typed_occurrences.occurrences[operation];
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
	/* Freeze source Context classifiers before constructing dependent branch
	 * pullbacks. The resulting Context morphisms and later certificates then
	 * describe the same final Context graph. */
	if (operation_solver_resolve_contexts(ctx, 1) != 0) {
		fprintf(stderr, "typing fixed point: finalized context resolution failed\n");
		return -1;
	}
	if (operation_solver_rebuild_refined_match_motives(ctx) != 0) {
		fprintf(stderr, "typing fixed point: refined Match motive rebuilding failed\n");
		return -1;
	}
	/* Indexed pullbacks change the Context in which motive equations live.
	 * Reclose the classifier fixed point in that final Context before any
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
	/* Indexed motive refinement can change a Lambda binder classifier after the
	 * first immutable Context canonicalization. Re-intern the affected Context
	 * extensions and relocate their occurrences before proof materialization. */
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
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), expected_body,
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
				const struct prototype_context* binding = prototype_context_get(
					&ctx->metadata->contexts, binding_context
				);
				if (binding) {
					scrutinee_classifier =
						prototype_context_classifier_term(binding);
				}
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
				struct operation_constraint* refinement =
					operation_solver_branch_refinement_constraint(
						ctx, operation->first_case + i
					);
				int refinement_status = refinement ? refinement->payload.
					branch_refinement.refinement_status :
					PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
				if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
					continue;
				}
				if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
					refinement_substitution = refinement->payload.
						branch_refinement.substitution;
					pending_pullback = 1;
				} else if (refinement_status ==
						PROTOTYPE_INDEX_REFINEMENT_CONSTANT) {
					constant_refinement = 1;
				} else {
				fprintf(stderr,
					"classifier check: branch refinement unavailable match=%u "
					"case=%u status=%d scrutinee-classifier=%u\n",
					operation_id, i, refinement_status,
					scrutinee_classifier);
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
				int action_matches = stored && (pending_pullback ?
					stored->target_context == operation_case->context_id :
					stored->source_context == operation_case->context_id);
				if (!action_matches) {
					fprintf(stderr,
						"classifier check: stale branch refinement match=%u case=%u substitution=%u "
						"source=%u target=%u case-context=%u pending=%d\n",
						operation_id, i, refinement_substitution,
						stored ? stored->source_context : PROTOTYPE_INVALID_ID,
						stored ? stored->target_context : PROTOTYPE_INVALID_ID,
						operation_case->context_id, pending_pullback);
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
		if (prototype_term_type_instance_info(
			ctx->terms, scrutinee_classifier,
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
				if (prototype_term_type_instance_make(
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
	if (!ctx || !p_changed || operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		expected_classifier >= ctx->terms->term_count || depth > 128) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (depth == 0 && !ctx->materializing_checked_classifiers &&
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
		const struct operation_binder_owner_entry* owner =
			operation_binder_owner_find(ctx, operation->binding_id);
		sequenced_result_var = owner &&
			owner->bound_computation_occurrence != PROTOTYPE_INVALID_ID;
	}
	if (check == 0 && synthesized_classifier < ctx->terms->term_count &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
		operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
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
		/* Every guarded recursive use contributes an equation for the motive.
		 * The APP-side expectation may expose a suspended function as a callable
		 * Pi, so it is not itself authoritative for the IH occurrence. The owner
		 * Match motive and constructor schema reconstruct the canonical IH
		 * classifier; ordinary APP solving opens its callable boundary afterward. */
		int motive_status = operation_solver_seed_indexed_motive_from_ih_expected(
			ctx, operation_id, expected_classifier, &ih_equation_changed
		);
		if (motive_status < 0) {
			return -1;
		}
		if (ih_equation_changed) {
			*p_changed = 1;
		}
		return 1;
	}
	if (depth == 0 && operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		/* A variable bound by a zero-clause computation fold denotes the result of
		 * the fold input. An expected classifier for that result is therefore an
		 * equation for the source computation and its guarded IH, not a mutable
		 * classifier hint for the VAR node. */
		int sequence_status = operation_solver_refine_sequenced_ih_result(
			ctx, operation_id, expected_classifier, p_changed
		);
		if (sequence_status < 0) {
			return -1;
		}
		if (sequence_status == 0) {
			return 1;
		}
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
		operation->wrapped_occurrence < ctx->metadata->typed_occurrences.occurrence_count) {
		int status = operation_refine_classifier_from_expected(
			ctx, operation->wrapped_occurrence, expected_classifier, depth + 1,
			p_changed
		);
		if (status <= 0) {
			return status;
		}
	} else if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
		operation->wrapped_occurrence < ctx->metadata->typed_occurrences.occurrence_count) {
		int status = operation_refine_classifier_from_expected(
			ctx, operation->wrapped_occurrence, expected_classifier, depth + 1,
			p_changed
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
			depth + 1, p_changed
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
			depth + 1, p_changed
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
		uint32_t codomain_binder = prototype_term_new_binding(ctx->terms);
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
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), expected_classifier,
				ctx->terms->terms[classifier_argument].as.var.binding_id,
				codomain_var, &codomain_body
			) : prototype_term_graph_replace_exact(
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), expected_classifier,
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
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), codomain_body,
				codomain_binder, classifier_argument, &reinstantiated
			) != 0 || prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				reinstantiated, expected_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 0;
		}
		const struct prototype_typed_occurrence* function_operation =
			&ctx->metadata->typed_occurrences.occurrences[function_occurrence];
		if (function_operation->category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			function_operation->computation_kind ==
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
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
			ctx, function_occurrence, function_expected, depth + 1, p_changed
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
				ctx->terms, prototype_type_view_rebuild_context_from_db(ctx->type_declarations), expected_body,
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
			ctx, body_occurrence, specialized_body, depth + 1, p_changed
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
				ctx, continuation_body, expected_classifier, depth + 1, p_changed
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
		struct prototype_term_classifier_view input_view;
		uint32_t continuation_classifier;
		if (input_classifier >= ctx->terms->term_count ||
			prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL,
				input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_pi(
				ctx->terms, input_view.result, expected_classifier,
				&continuation_classifier
			) != 0) {
			return 0;
		}
		int status = operation_refine_classifier_from_expected(
			ctx, body_occurrence, continuation_classifier, depth + 1, p_changed
		);
		if (status <= 0) {
			return status;
		}
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		uint64_t motive_revision =
			ctx->classifier_solver.recursive_equation_revision;
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
		uint32_t motive = ctx->classifier_solver.motive_solutions[operation_id].motive;
		uint32_t motive_application = PROTOTYPE_INVALID_ID;
		int reuse_motive = motive < ctx->terms->term_count &&
			scrutinee_classifier < ctx->terms->term_count &&
			prototype_judgement_apply_indexed_motive(
				ctx->terms, ctx->type_declarations, motive, scrutinee,
				scrutinee_classifier, 0, &motive_application
			) == 0 && prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				motive_application, expected_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
		if (!reuse_motive) {
			/* Expected classifiers validate or locally refine a Match; they do not
			 * become an authority for a new indexed family. Only branch/IH equations
			 * may synthesize an indexed motive. This fallback therefore abstracts the
			 * exact scrutinee into the traditional unary motive. */
			uint32_t motive_binder = prototype_term_new_binding(ctx->terms);
			uint32_t motive_var;
			uint32_t motive_body;
			int fallback_status;
			if (motive_binder == PROTOTYPE_INVALID_ID || prototype_term_var(
					ctx->terms, motive_binder, &motive_var
				) != 0) {
				return -1;
			}
			if (ctx->terms->terms[scrutinee].tag == PROTOTYPE_TERM_VAR) {
				fallback_status = prototype_term_graph_substitute_bound_var(
					ctx->terms,
					prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
					expected_classifier,
					ctx->terms->terms[scrutinee].as.var.binding_id,
					motive_var, &motive_body
				);
			} else {
				fallback_status = prototype_term_graph_replace_exact(
					ctx->terms,
					prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
					expected_classifier, scrutinee, motive_var, &motive_body
				);
			}
			if (fallback_status != 0 || prototype_term_lambda(
					ctx->terms, motive_binder, motive_body, &motive
				) != 0) {
				return -1;
			}
			if (scrutinee_classifier >= ctx->terms->term_count) {
				/* Branch refinement and motive application both require the scrutinee
				 * classifier. Its absence is a pending constraint, not malformed Core. */
				return 0;
			}
			if (prototype_judgement_apply_indexed_motive(
					ctx->terms, ctx->type_declarations, motive, scrutinee,
					scrutinee_classifier, 0, &motive_application
				) != 0) {
				return -1;
			}
		}
		uint32_t selected_motive =
			ctx->classifier_solver.motive_solutions[operation_id].motive;
		if ((ctx->classifier_solver.recursive_equation_revision !=
				motive_revision || operation_motive_has_recursive_equations(
					ctx, operation_id
				) > 0) && selected_motive < ctx->terms->term_count &&
			selected_motive != motive) {
			/* A nested expected check added a stronger recursive equation while
			 * this Match expectation was being elaborated. Discard the classifier
			 * derived from the stale local motive; the queued graph edge will rerun
			 * this refinement with the selected motive. */
			return 1;
		}
		uint32_t current_classifier = operation_solver_classifier(
			ctx, operation_id
		);
		if (current_classifier >= ctx->terms->term_count ||
			prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				current_classifier, motive_application
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			if (operation_classifier_meta_solve(
				ctx, operation_id, motive_application
			) != 0) {
				return -1;
			}
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, operation_id) != 0) {
				return -1;
			}
		}
		ctx->classifier_solver.motive_solutions[operation_id].motive = motive;
		ctx->classifier_solver.motive_solutions[operation_id].constant_candidate =
			PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.motive_solutions[operation_id].state =
			OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
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
				const struct prototype_context* binding = prototype_context_get(
					&ctx->metadata->contexts, binding_context
				);
				if (binding) {
					scrutinee_classifier =
						prototype_context_classifier_term(binding);
				}
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
				struct operation_constraint* refinement =
					operation_solver_branch_refinement_constraint(
						ctx, operation->first_case + case_index
					);
				int refinement_status = refinement ? refinement->payload.
					branch_refinement.refinement_status :
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
					refinement_substitution = refinement->payload.
						branch_refinement.substitution;
					pending_pullback = 1;
				}
			}
			if (!constant_refinement) {
				const struct prototype_substitution* stored =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						refinement_substitution
					);
				if (!stored || (pending_pullback ?
					stored->target_context != operation_case->context_id :
					stored->source_context != operation_case->context_id)) {
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
				depth + 1, p_changed
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
	if (ctx->materializing_checked_classifiers &&
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
		if (check->operation < ctx->metadata->typed_occurrences.occurrence_count &&
			(operation_classifier = operation_available_classifier(
				ctx, check->operation
			)) != PROTOTYPE_INVALID_ID) {
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
		;
		int structurally_checked = operation_check_classifier(
			ctx, check->operation, solved_expected_classifier, 0
		);
		if (structurally_checked < 0) {
			fprintf(stderr, "expectation %u: structural classifier check failed\n", i);
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
			operation < ctx->metadata->typed_occurrences.occurrence_count;
			++operation) {
			if (ctx->metadata->typed_occurrences.occurrences[operation].tag ==
					PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
				ctx->metadata->typed_occurrences.occurrences[operation].wrapped_occurrence ==
					check->operation &&
				ctx->metadata->typed_occurrences.occurrences[operation].source_ast == check->ast) {
				if (operation_classifier_meta_solve(
					ctx, operation, solved_expected_classifier
				) != 0) {
					return -1;
				}
				ascription_operation = operation;
			}
		}
		if (ascription_operation == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		/* Evidence is materialized after both inline ascriptions and top-level
		 * expectations have closed their effect rows. */
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
		if (ctx->metadata->function_graph_preflight && !def->compiled) {
			continue;
		}
		ctx->binder_count = 0;
		ctx->ih_scope_count = 0;
		int expectation_def_status = compile_def(ctx, def, &compiled_term);
		int expectation_classifier_status = expectation_def_status == 0 ?
			compile_type_expectation_classifier(
				ctx, expectation, &expected_classifier
			) : -1;
		if (expectation_def_status != 0 || expectation_classifier_status != 0) {
			fprintf(stderr,
				"expectation lowering failed name=%d def=%d classifier=%d type-expr=%u\n",
				expectation->name_symbol_id, expectation_def_status,
				expectation_classifier_status, expectation->type_expr);
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
			def->compiled_operation < ctx->metadata->typed_occurrences.occurrence_count &&
			ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].tag ==
				PROTOTYPE_TYPED_OCCURRENCE_THUNK) {
			/*
			 * The ascription checks the explicit definition RHS before the definition
			 * boundary inserts its implicit thunk. The exported definition retains
			 * the synthesized thunk classifier.
			 */
			continue;
		}
		struct compile_ref expected_ref;
		compile_ref_clear(&expected_ref);
		if (def->compiled_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
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
			ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].category;
		expected_ref.computation_kind =
			ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].computation_kind;
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
		if (def->compiled_operation < ctx->metadata->typed_occurrences.occurrence_count &&
			ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].tag ==
				PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
			ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].wrapped_occurrence <
				ctx->metadata->typed_occurrences.occurrence_count &&
			operation_available_classifier(
				ctx,
				ctx->metadata->typed_occurrences.occurrences[
					def->compiled_operation
				].wrapped_occurrence
			) != PROTOTYPE_INVALID_ID) {
			/* The ascription is an annotation; its body supplies the evidence. */
			actual_classifier = operation_available_classifier(
				ctx,
				ctx->metadata->typed_occurrences.occurrences[
					def->compiled_operation
				].wrapped_occurrence
			);
			actual_from_operation = 1;
		} else if (def->compiled_operation < ctx->metadata->typed_occurrences.occurrence_count &&
			ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].tag !=
				PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
			operation_available_classifier(ctx, def->compiled_operation) !=
				PROTOTYPE_INVALID_ID) {
			/*
			 * A source definition denotes an operation-layer node. Its classifier
			 * must not be recovered by collecting every classifier ever assigned to
			 * the shared erased core term.
			 */
			actual_classifier = operation_available_classifier(
				ctx, def->compiled_operation
			);
			actual_from_operation = 1;
		} else if (def->compiled_operation < ctx->metadata->typed_occurrences.occurrence_count &&
			ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].tag !=
				PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
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
		int expected_effect_status = actual_classifier == PROTOTYPE_INVALID_ID ? -1 :
			prototype_judgement_solve_expected_effect_rows(
				ctx->terms,
				ctx->type_declarations,
				&imported_definition_env,
				expected_classifier,
				actual_classifier,
				&expected_classifier
			);
		int classifier_compatible = actual_classifier != PROTOTYPE_INVALID_ID &&
			prototype_judgement_classifier_compatible_with_definitions(
				ctx->terms,
				ctx->type_declarations,
				&imported_definition_env,
				expected_classifier,
				actual_classifier
			);
		if (actual_classifier == PROTOTYPE_INVALID_ID ||
			(!structurally_checked && expected_effect_status != 0 &&
				!classifier_compatible)) {
			fprintf(stderr,
				"expectation mismatch name=%d operation=%u expected=%u actual=%u structural=%d effect=%d compatible=%d\n",
				expectation->name_symbol_id, def->compiled_operation,
				expected_classifier, actual_classifier, structurally_checked,
				expected_effect_status, classifier_compatible);
			fprintf(stderr, "expectation expected: ");
			prototype_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				expected_classifier
			);
			fprintf(stderr, "\nexpectation actual: ");
			prototype_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				actual_classifier
			);
			fprintf(stderr, "\n");
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
		/* Keep the checked, effect-row-resolved expectation for read-only label
		 * publication. This remains a result of post-synthesis checking; it is
		 * never read by the classifier solver. */
		expectation->compiled_classifier = expected_classifier;
		int free_effect_row = operation_classifier_contains_unowned_effect_row(
			ctx, def->compiled_operation, actual_classifier
		);
		if (free_effect_row < 0) {
			fprintf(stderr, "expectation %u: effect-row ownership check failed\n", i);
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
		(void)closed_conversion;
			if (actual_from_operation) {
				struct prototype_judgement_selected_evidence source_evidence;
				uint32_t source_operation = def->compiled_operation;
				const struct prototype_typed_occurrence* compiled_operation =
					&ctx->metadata->typed_occurrences.occurrences[source_operation];
				if (compiled_operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
					/* The direct body Operation supplied actual_classifier above. Nested
					 * annotations retain distinct evidence and must not be stripped to an
					 * untyped VAR or CONSTRUCTOR occurrence. */
					source_operation = compiled_operation->wrapped_occurrence;
				}
				if (source_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
					fprintf(stderr, "expectation %u: invalid source operation\n", i);
					return -1;
				}
				uint32_t source_context =
					ctx->metadata->typed_occurrences.occurrences[source_operation].context_id;
				uint32_t source_authority = operation_solver_evidence_owner(
					ctx, source_operation
				);
				int source_status = prototype_judgement_select_evidence(
						ctx->judgement,
						source_authority,
						source_context,
						compiled_term,
						actual_classifier,
						&source_evidence
				);
				if (source_status != 0) {
					fprintf(stderr,
						"expectation source evidence missing id=%u operation=%u authority=%u "
						"context=%u term=%u classifier=%u status=%d\n",
						i, source_operation, source_authority, source_context,
						compiled_term, actual_classifier, source_status);
					return -1;
				}
				if (prototype_judgement_add_expected_type_exposure(
						ctx->judgement,
						ctx->terms,
						ctx->type_declarations,
						&ctx->metadata->contexts,
						&ctx->metadata->substitutions,
						&ctx->metadata->typed_occurrences,
						source_context,
						def->compiled_operation,
						&source_evidence,
						expected_classifier,
						compiled_term
					) != 0) {
					fprintf(stderr,
						"expectation %u: expected-type exposure failed expected=%u actual=%u\n",
						i, expected_classifier, source_evidence.classifier
					);
					return -1;
			}
			def->compiled_classifier = actual_classifier;
			continue;
			}
			struct prototype_judgement_selected_evidence source_evidence;
			int source_status = prototype_judgement_select_evidence(
					ctx->judgement,
					PROTOTYPE_INVALID_ID,
					0,
					compiled_term,
					actual_classifier,
					&source_evidence
				);
			if (source_status != 0) {
				fprintf(stderr,
					"expectation %u: closed source evidence status=%d\n",
					i, source_status);
				return -1;
			}
			if (prototype_judgement_add_expected_type_exposure(
					ctx->judgement,
					ctx->terms,
					ctx->type_declarations,
					&ctx->metadata->contexts,
					&ctx->metadata->substitutions,
					&ctx->metadata->typed_occurrences,
					0,
					def->compiled_operation,
					&source_evidence,
					expected_classifier,
					compiled_term
				) != 0) {
				fprintf(stderr, "expectation %u: closed expected-type exposure failed\n", i);
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
			def->compiled_operation < ctx->metadata->typed_occurrences.occurrence_count) {
			def->compiled_classifier = ctx->metadata->typed_occurrences.occurrences[
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
			fprintf(stderr,
				"label publication entry failed assignment=%u term=%u classifier=%u operation=%u\n",
				i, def->compiled_term, def->compiled_classifier,
				def->compiled_operation);
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
			selected->compiled_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
			ctx->metadata->typed_occurrences.occurrences[selected->compiled_operation].category !=
				PROTOTYPE_TERM_CATEGORY_VALUE) {
			return -1;
		}
		if (ctx->metadata->selected_entry_symbol_id != selected->name_symbol_id ||
			ctx->metadata->selected_entry_term >= ctx->terms->term_count ||
			ctx->metadata->selected_entry_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
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
	for (uint32_t occurrence_id = 0;
		occurrence_id < ctx->metadata->typed_occurrences.occurrence_count;
		++occurrence_id) {
		uint32_t computation_occurrence =
			ctx->termination_evidence_computation_occurrences[occurrence_id];
		if (computation_occurrence == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (computation_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		const struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
		const struct prototype_typed_occurrence* computation =
			&ctx->metadata->typed_occurrences.occurrences[computation_occurrence];
		uint32_t suspended = PROTOTYPE_INVALID_ID;
		uint32_t witness_computation = PROTOTYPE_INVALID_ID;
		if (occurrence->classifier == PROTOTYPE_INVALID_ID ||
			computation->classifier == PROTOTYPE_INVALID_ID ||
			occurrence->context_id != computation->context_id ||
			prototype_term_terminates_type_info(
				ctx->terms, occurrence->classifier, &suspended
			) != 0 || prototype_term_terminates_witness_info(
				ctx->terms, occurrence->core_term, &witness_computation
			) != 0 || suspended != witness_computation ||
			computation->core_term != suspended) {
			return -1;
		}
		uint32_t computation_claim;
		uint32_t terminates_type_claim;
		uint32_t terminates_claim;
		uint32_t universe;
		size_t claim_count_before = ctx->judgement->claim_count;
		if (accepted_claim_for_exact_proposition(
				ctx->judgement, computation->context_id, computation->core_term,
				computation->classifier, &computation_claim
			) != 0) {
			return -1;
		}
		int formation_status = accepted_type_formation_claim(
			ctx->judgement, occurrence->context_id, occurrence->classifier,
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
					occurrence->context_id, occurrence->classifier, universe,
					computation_claim, &terminates_type_claim
				) != 0
		)) {
			return -1;
		}
		uint32_t computation_classifier = computation->classifier;
		if (prototype_judgement_classifier_value_whnf(
				ctx->terms, ctx->type_declarations, computation_classifier,
				&computation_classifier
			) != 0 || computation_classifier >= ctx->terms->term_count ||
			ctx->terms->terms[computation_classifier].tag !=
				PROTOTYPE_TERM_THUNK_TYPE) {
			return -1;
		}
		uint32_t computation_type = ctx->terms->terms[
			computation_classifier
		].as.thunk_type.computation;
		if (computation_type >= ctx->terms->term_count ||
			ctx->terms->terms[computation_type].tag !=
				PROTOTYPE_TERM_COMPUTATION_TYPE ||
			ctx->terms->terms[computation_type].as.computation_type.totality !=
				PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ||
			prototype_judgement_add_terminates_total_computation(
				ctx->judgement, ctx->terms, occurrence->context_id, occurrence_id,
				occurrence->core_term, occurrence->classifier,
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

static int compile_pending_with_workspace(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct compile_judgement_workspace* workspace,
	struct compile_context* ctx
) {
	if (!asts || !terms || !type_declarations || !judgement || !metadata ||
		!intrinsic_environment || !workspace || !ctx) {
		return -1;
	}
	uint64_t phase_started = 0;
	if (ctx->producer_phase == 0) {
	uint64_t phase_credits[PROTOTYPE_EFFORT_PHASE_COUNT] = { 0 };
	phase_credits[PROTOTYPE_EFFORT_PHASE_FUNCTION_GRAPH] =
		asts->type_expr_count + asts->node_count;
	phase_credits[PROTOTYPE_EFFORT_PHASE_GRAPH] =
		asts->assignment_count + asts->node_count;
	if (phase_credits[PROTOTYPE_EFFORT_PHASE_FUNCTION_GRAPH] == 0) {
		phase_credits[PROTOTYPE_EFFORT_PHASE_FUNCTION_GRAPH] = 1;
	}
	if (phase_credits[PROTOTYPE_EFFORT_PHASE_GRAPH] == 0) {
		phase_credits[PROTOTYPE_EFFORT_PHASE_GRAPH] = 1;
	}
	int phase_effort = prototype_effort_consume_split(
		&metadata->effort, phase_credits
	);
	if (phase_effort != 0) return phase_effort;
	memset(ctx, 0, sizeof(*ctx));
	ctx->effort_atomic = 1;
	for (size_t i = 0;
		i < sizeof(ctx->classifier_seeds) / sizeof(ctx->classifier_seeds[0]);
		++i) {
		ctx->classifier_seeds[i].owner_occurrence = (uint32_t)i;
		ctx->classifier_seeds[i].classifier = PROTOTYPE_INVALID_ID;
		ctx->classifier_seeds[i].binder_classifier = PROTOTYPE_INVALID_ID;
		ctx->classifier_seeds[i].reason = OPERATION_CLASSIFIER_SEED_NONE;
		ctx->classifier_solver.classifier_metas[i].owner_occurrence = (uint32_t)i;
		ctx->classifier_solver.classifier_metas[i].classifier = PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.classifier_metas[i].stale_classifier =
			PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.classifier_metas[i].evidence_constraint_id =
			PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.classifier_metas[i].state =
			OPERATION_CLASSIFIER_META_UNSOLVED;
		ctx->classifier_solver.classifier_metas[i].binder_classifier =
			PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.classifier_metas[i].binder_evidence_constraint_id =
			PROTOTYPE_INVALID_ID;
		ctx->classifier_solver.classifier_metas[i].binder_state =
			OPERATION_CLASSIFIER_META_UNSOLVED;
		ctx->effect_solver.meta_for_occurrence[i] = PROTOTYPE_INVALID_ID;
		ctx->termination_evidence_computation_occurrences[i] = PROTOTYPE_INVALID_ID;
	}
	ctx->intrinsic_environment = intrinsic_environment;
	ctx->symbols = symbols;
	ctx->asts = asts;
	ctx->terms = terms;
	ctx->type_declarations = type_declarations;
	ctx->judgement = judgement;
	ctx->operation_usage_entries = workspace->operation_usage_entries;
	ctx->branch_reachability_bits = workspace->branch_reachability_bits;
	ctx->branch_reachability_valid = workspace->branch_reachability_valid;
	ctx->branch_reachability_occurrence_count =
		&workspace->branch_reachability_occurrence_count;
	ctx->computation_constraint_results = workspace->computation_constraint_results;
	ctx->computation_constraint_result_capacity = PROTOTYPE_JUDGEMENT_DELTA_CAPACITY;
	ctx->active_classifier_constraint_id = PROTOTYPE_INVALID_ID;
	ctx->operation_usage_entry_capacity = PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY;
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
	if (!metadata || prototype_context_empty(&metadata->contexts) == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (prototype_typed_occurrence_graph_begin_transaction(
			&metadata->typed_occurrences, terms, &metadata->contexts
		) != 0) {
		return -1;
	}
	if (operation_binder_owner_index_seed_transaction(ctx) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_context_store(
		&ctx->judgement_delta,
		&metadata->contexts,
		&metadata->substitutions
	);
	ctx->judgement_delta.dimension_operators = &metadata->dimension_operators;
	ctx->context_ids[0] = prototype_context_empty(&metadata->contexts);
	ctx->namespace_symbol_id = namespace_symbol_id;
	ctx->imported_interfaces = imported_interfaces;
	ctx->imported_interface_count = imported_interface_count;
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
		return -1;
	}
	if (compile_phase_infer_imported_constructor_classifiers(ctx) != 0) {
		fprintf(stderr, "compile pending failed: imported constructor classifiers\n");
		return -1;
	}
	if (type_declarations->representation_db.cache_dirty &&
		(prototype_type_declaration_rebuild_representations(
			terms, type_declarations, &metadata->contexts
		) != 0 || prototype_term_rebind_type_former_anchors(
			terms, type_declarations
		) != 0)) {
		fprintf(stderr, "compile pending failed: type representation finalization\n");
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 || prototype_internal_canonicalize_constructor_owner_refs(
			terms, type_declarations, 0
		) != 0) {
		return -1;
	}
	if (compile_phase_resolve_pending_match_items(ctx) < 0) {
		fprintf(stderr, "compile pending failed: initial Match resolution\n");
		return -1;
	}
	if (finish_resolution_iteration(ctx, count_unresolved_resolution_items(ctx)) != 0) {
		return -1;
	}
	if (prototype_internal_canonicalize_type_view_core_refs(
			terms, type_declarations, &metadata->contexts
		) != 0 || prototype_internal_canonicalize_constructor_owner_refs(
			terms, type_declarations, 0
		) != 0) {
		return -1;
	}
	ctx->producer_phase = 1;
	}
	if (ctx->producer_phase == 1) {
	if (!ctx->producer_inference_precharged) {
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
		ctx->producer_inference_precharged = 1;
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
	ctx->producer_phase = 2;
	}
	if (ctx->producer_phase == 2) {
	uint64_t finalization_credit =
		metadata->typed_occurrences.occurrence_count * UINT64_C(64) + 1;
	int finalization_effort = compile_effort_charge(
		ctx, PROTOTYPE_EFFORT_PHASE_PROOF, finalization_credit
	);
	if (finalization_effort != 0) return finalization_effort;
	ctx->effort_atomic = 1;
	/* The fixed point retains the occurrence-owned computation suffix across
	 * classifier passes. Refresh its solved lifecycle and effect equations before
	 * freezing residual state; no authority reconstruction is required here. */
	int final_computation_changed = 0;
	if (operation_solver_bind_computation_constraint_solutions(
			ctx, &final_computation_changed, 0
		) != 0 || operation_effect_generate_constraints(ctx) != 0 ||
			prototype_judgement_delta_commit(&ctx->judgement_delta, 0) != 0 ||
			compile_phase_record_residual_effect_constraints(ctx) != 0 ||
			compile_phase_record_residual_dependent_folds(ctx) != 0 ||
			compile_phase_record_residual_computation_fold_results(ctx) != 0) {
		fprintf(stderr, "compile pending failed: effect/commit/residual\n");
		return -1;
	}
	(void)final_computation_changed;
	operation_solver_refresh_constraint_states(ctx, ctx->metadata->effort.exhausted);
	for (uint32_t constraint_id = 0;
		constraint_id < ctx->constraints.classifier_constraint_count;
		++constraint_id) {
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[constraint_id];
		if (constraint->kind != OPERATION_CONSTRAINT_MOTIVE_EQUATION ||
			constraint->state != OPERATION_CONSTRAINT_STATE_CONTRADICTION) {
			continue;
		}
		int diagnostic_status = operation_motive_add_terminal_diagnostic(
			ctx,
			constraint->payload.classifier.classifier_meta_id,
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
			operation_effect_unresolved_count(&ctx->constraints) != 0)) {
		fprintf(stderr, "strict compilation retained unresolved verification state\n");
		return -1;
	}
	if (compile_phase_check_ascriptions(ctx) != 0) {
		fprintf(stderr, "post-synthesis ascription checking failed\n");
		return -1;
	}
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
	struct prototype_typed_occurrence_graph* occurrence_graph =
		prototype_compile_metadata_typed_occurrences(metadata);
	/* Totality evidence consumes only already accepted synthesis results. */
	if (prototype_judgement_publish_candidates(
			occurrence_graph, judgement
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
				occurrence_graph, judgement
			) != 0) {
			fprintf(stderr, "post-evidence judgement closure failed round=%u\n", round);
			return -1;
		}
		if (judgement->claim_count == before_claims &&
			judgement->derivation_count == before_derivations &&
			!ctx->proof_materialization_substitution_pending) {
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
	if (prototype_judgement_add_normalization_premise_conversions(
			terms,
			type_declarations,
			occurrence_graph,
			judgement
		) != 0) {
		fprintf(stderr, "normalization premise conversion expansion failed\n");
		return -1;
	}
	if (prototype_judgement_publish_candidates(
			occurrence_graph,
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
				occurrence_graph, judgement
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
			!ctx->proof_materialization_substitution_pending) {
			normalization_evidence_closed = 1;
			break;
		}
	}
	if (!normalization_evidence_closed) {
		fprintf(stderr, "normalization evidence closure did not stabilize\n");
		return -1;
	}
	for (uint32_t i = 0; i < occurrence_graph->occurrence_count; ++i) {
		struct prototype_typed_occurrence* occurrence =
			&occurrence_graph->occurrences[i];
		if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			continue;
		}
		uint32_t motive = ctx->classifier_solver.motive_solutions[i].motive;
		int motive_state = ctx->classifier_solver.motive_solutions[i].state;
		if (motive >= terms->term_count ||
			(motive_state != OPERATION_MOTIVE_SOLUTION_EQUATIONS_READY &&
			 motive_state != OPERATION_MOTIVE_SOLUTION_MATERIALIZED)) {
			uint32_t classifier = operation_solver_classifier(ctx, i);
			const struct prototype_term* match = occurrence->core_term <
				terms->term_count ? &terms->terms[occurrence->core_term] : NULL;
			const struct prototype_term* classifier_term = classifier <
				terms->term_count ? &terms->terms[classifier] : NULL;
			const struct prototype_term* classifier_function = classifier_term &&
				classifier_term->tag == PROTOTYPE_TERM_APP &&
				classifier_term->as.app.function < terms->term_count ?
					&terms->terms[classifier_term->as.app.function] : NULL;
			if (match && match->tag == PROTOTYPE_TERM_MATCH && classifier_term &&
				classifier_term->tag == PROTOTYPE_TERM_APP &&
				classifier_term->as.app.argument == match->as.match.scrutinee &&
				classifier_function &&
				classifier_function->tag == PROTOTYPE_TERM_LAMBDA) {
				motive = classifier_term->as.app.function;
				ctx->classifier_solver.motive_solutions[i].motive = motive;
				ctx->classifier_solver.motive_solutions[i].state =
					OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
				occurrence->match_motive = motive;
				continue;
			}
			uint32_t motive_binding = prototype_term_new_binding(terms);
			uint32_t motive_variable;
			uint32_t motive_body;
			int abstract_status;
			if (!match || match->tag != PROTOTYPE_TERM_MATCH ||
				match->as.match.scrutinee >= terms->term_count ||
				classifier >= terms->term_count ||
				motive_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
					terms, motive_binding, &motive_variable
				) != 0) {
				return -1;
			}
			const struct prototype_term* scrutinee =
				&terms->terms[match->as.match.scrutinee];
			if (scrutinee->tag == PROTOTYPE_TERM_VAR) {
				abstract_status = prototype_term_graph_substitute_bound_var(
					terms, prototype_type_view_rebuild_context_from_db(type_declarations),
					classifier,
					scrutinee->as.var.binding_id,
					motive_variable,
					&motive_body
				);
			} else {
				abstract_status = prototype_term_graph_replace_exact(
					terms, prototype_type_view_rebuild_context_from_db(type_declarations),
					classifier,
					match->as.match.scrutinee,
					motive_variable,
					&motive_body
				);
			}
			if (abstract_status != 0 || prototype_term_lambda(
					terms, motive_binding, motive_body, &motive
				) != 0) {
				return -1;
			}
			ctx->classifier_solver.motive_solutions[i].motive = motive;
			ctx->classifier_solver.motive_solutions[i].state =
				OPERATION_MOTIVE_SOLUTION_MATERIALIZED;
		}
		occurrence->match_motive = motive;
	}
	if (prototype_typed_occurrence_graph_seal(
			occurrence_graph, terms, &metadata->contexts
		) != 0) {
		fprintf(stderr, "typed-occurrence graph sealing failed before accepted replay\n");
		return -1;
	}
	phase_started = compile_performance_clock_ns();
	if (prototype_judgement_validate_accepted_graph(
			terms,
			type_declarations,
			intrinsic_environment,
			&metadata->contexts,
			&metadata->substitutions,
			&metadata->dimension_operators,
			occurrence_graph,
			judgement
		) != 0) {
		fprintf(stderr, "P0 accepted proof graph validation failed\n");
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
			&type_declarations->constructor_classifier_cache, &metadata->contexts, terms
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
	ctx->effort_atomic = 0;
	ctx->producer_phase = 3;
	}
	return 0;
}

struct prototype_compile_producer_session {
	struct prototype_ast_db* asts;
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	struct prototype_judgement_db* judgement;
	struct prototype_compile_metadata* metadata;
	struct symbol_table* symbols;
	const struct prototype_intrinsic_environment* intrinsic_environment;
	int namespace_symbol_id;
	const struct prototype_artifact_interface* const* imported_interfaces;
	size_t imported_interface_count;
	struct compile_judgement_workspace* workspace;
	struct compile_context* context;
	int outcome;
};

int prototype_compile_producer_session_create(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count,
	struct prototype_compile_producer_session** p_session
) {
	if (!asts || !terms || !type_declarations || !judgement || !metadata ||
		!intrinsic_environment || !p_session ||
		metadata->typed_occurrences.transaction_active) {
		return -1;
	}
	*p_session = NULL;
	struct prototype_compile_producer_session* session =
		calloc(1, sizeof(*session));
	if (!session) return -1;
	session->workspace = calloc(1, sizeof(*session->workspace));
	session->context = calloc(1, sizeof(*session->context));
	if (!session->workspace || !session->context) {
		free(session->context);
		free(session->workspace);
		free(session);
		return -1;
	}
	session->asts = asts;
	session->terms = terms;
	session->type_declarations = type_declarations;
	session->judgement = judgement;
	session->metadata = metadata;
	session->symbols = symbols;
	session->intrinsic_environment = intrinsic_environment;
	session->namespace_symbol_id = namespace_symbol_id;
	session->imported_interfaces = imported_interfaces;
	session->imported_interface_count = imported_interface_count;
	*p_session = session;
	return 0;
}

int prototype_compile_producer_session_advance(
	struct prototype_compile_producer_session* session,
	uint64_t additional_effort,
	struct prototype_compile_producer_report* p_report
) {
	if (!session || !p_report || session->outcome != 0 ||
		prototype_effort_add_credits(
			&session->metadata->effort, additional_effort
		) != 0) {
		return -1;
	}
	uint64_t effort_before = session->metadata->effort.used;
	int status = compile_pending_with_workspace(
		session->asts,
		session->terms,
		session->type_declarations,
		session->judgement,
		session->metadata,
		session->symbols,
		session->intrinsic_environment,
		session->namespace_symbol_id,
		session->imported_interfaces,
		session->imported_interface_count,
		session->workspace,
		session->context
	);
	p_report->phase = session->context->producer_phase;
	p_report->effort_used = session->metadata->effort.used - effort_before;
	if (status == 0) {
		session->outcome = PROTOTYPE_COMPILE_PRODUCER_COMPLETE;
		p_report->status = session->outcome;
		return 0;
	}
	if (status == 1 && session->metadata->effort.exhausted) {
		p_report->status = PROTOTYPE_COMPILE_PRODUCER_PAUSED;
		return 0;
	}
	if (session->metadata->typed_occurrences.transaction_active) {
		(void)prototype_typed_occurrence_graph_rollback_transaction(
			&session->metadata->typed_occurrences
		);
	}
	session->outcome = PROTOTYPE_COMPILE_PRODUCER_REJECTED;
	p_report->status = session->outcome;
	return 0;
}

void prototype_compile_producer_session_destroy(
	struct prototype_compile_producer_session* session
) {
	if (!session) return;
	if (session->outcome == 0 && session->metadata &&
		session->metadata->typed_occurrences.transaction_active) {
		(void)prototype_typed_occurrence_graph_rollback_transaction(
			&session->metadata->typed_occurrences
		);
	}
	free(session->context);
	free(session->workspace);
	free(session);
}

int prototype_ast_compile_pending_with_imports(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
) {
	struct prototype_compile_producer_session* session = NULL;
	if (prototype_compile_producer_session_create(
		asts,
		terms,
		type_declarations,
		judgement,
		metadata,
		symbols,
		intrinsic_environment,
		namespace_symbol_id,
		imported_interfaces,
		imported_interface_count,
		&session
	) != 0) {
		return -1;
	}
	struct prototype_compile_producer_report report;
	int advance_status = prototype_compile_producer_session_advance(
		session, 0, &report
	);
	prototype_compile_producer_session_destroy(session);
	if (advance_status != 0) return -1;
	return report.status == PROTOTYPE_COMPILE_PRODUCER_COMPLETE ? 0 :
		(report.status == PROTOTYPE_COMPILE_PRODUCER_PAUSED ? 1 : -1);
}
