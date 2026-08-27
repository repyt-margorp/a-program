/* Motive synthesis is typed compiler work above the context-free Core graph.
 * A branch index correspondence is derived from the indexed family telescope
 * and the solved branch substitution. Core shape is never used to discover a
 * replacement location. */

/* Recover a source-Context candidate from a term in a refined branch. A
 * branch substitution need not be invertible, so this is only candidate
 * generation: reindexing the result through the same substitution must recover
 * the original term before the caller may use it. */
static int operation_motive_quote_refined_term(
	struct compile_context* ctx,
	uint32_t term,
	uint32_t substitution_id,
	uint32_t* p_quoted
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(
			ctx ? &ctx->metadata->substitutions : NULL, substitution_id
		);
	if (!ctx || !p_quoted || term >= ctx->terms->term_count || !substitution) {
		return -1;
	}
	uint32_t path[512];
	uint32_t path_count;
	if (prototype_context_extension_path(
			&ctx->metadata->contexts,
			prototype_context_empty(&ctx->metadata->contexts),
			substitution->target_context,
			path,
			512,
			&path_count
		) != 0) {
		return -1;
	}
	uint32_t quoted = term;
	/* Replace deeper, more dependent images first. This avoids rewriting a
	 * subterm of a later substitution image before that image is recognized. */
	for (uint32_t reverse = path_count; reverse > 0; --reverse) {
		const struct prototype_context* entry = prototype_context_get(
			&ctx->metadata->contexts, path[reverse - 1]
		);
		uint32_t source_variable;
		uint32_t image;
		uint32_t next;
		if (!entry || entry->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				ctx->terms, entry->binding_id, &source_variable
			) != 0 || prototype_substitution_binding_term(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				&ctx->metadata->substitutions,
				substitution_id,
				entry->binding_id,
				&image
			) != 0) {
			return -1;
		}
		if (image == source_variable) {
			continue;
		}
		if (prototype_term_graph_replace_exact(
				ctx->terms,
				prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
				quoted,
				image,
				source_variable,
				&next
			) != 0) {
			return -1;
		}
		quoted = next;
	}
	uint32_t reinstantiated;
	if (prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			quoted,
			substitution_id,
			&reinstantiated
		) != 0) {
		return -1;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, reinstantiated, term
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	*p_quoted = quoted;
	return 0;
}

static int operation_motive_branch_index_correspondence(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t case_index,
	uint32_t constructor_classifier,
	uint32_t index_ordinal,
	struct operation_motive_index_correspondence* p_correspondence
) {
	if (!ctx || !p_correspondence || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[match_operation_id];
	uint32_t scrutinee_operation;
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		case_index >= match_operation->case_count || compile_occurrence_child(
			ctx, match_operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_operation
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t operation_case_id = match_operation->first_case + case_index;
	if (operation_case_id >= ctx->metadata->typed_occurrences.case_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[operation_case_id];
	if (!prototype_typed_occurrence_match_case_is_solved(operation_case)) {
		return OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
	}
	const struct prototype_substitution* refinement = prototype_substitution_get(
		&ctx->metadata->substitutions,
		operation_case->refinement_substitution
	);
	if (!refinement || refinement->source_context != operation_case->context_id ||
		!prototype_context_is_ancestor(
			&ctx->metadata->contexts,
			match_operation->context_id,
			refinement->target_context
		)) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}

	uint32_t scrutinee_classifier = operation_solver_classifier(
		ctx, scrutinee_operation
	);
	uint32_t family_type_id;
	uint32_t source_arguments[32];
	uint32_t source_argument_count;
	uint32_t constructor_type_id;
	uint32_t branch_arguments[32];
	uint32_t branch_argument_count;
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID ||
		prototype_term_type_instance_info(
			ctx->terms, scrutinee_classifier, &family_type_id,
			source_arguments, &source_argument_count
		) != 0 || prototype_term_type_instance_info(
			ctx->terms, constructor_classifier, &constructor_type_id,
			branch_arguments, &branch_argument_count
		) != 0 || family_type_id != constructor_type_id || family_type_id >=
			ctx->type_declarations->semantic_schema.type_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT;
	}
	const struct prototype_type_declaration* declaration =
		&ctx->type_declarations->semantic_schema.type_declarations[family_type_id];
	if (index_ordinal >= declaration->index_count ||
		declaration->parameter_count + declaration->index_count !=
			source_argument_count || source_argument_count != branch_argument_count) {
		return OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT;
	}

	uint32_t parameter_substitution;
	if (prototype_context_substitution_from_terms(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			match_operation->context_id,
			declaration->parameter_context,
			source_arguments,
			declaration->parameter_count,
			&parameter_substitution
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t source_index_classifier;
	if (prototype_context_telescope_entry_classifier(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			parameter_substitution,
			declaration->parameter_context,
			declaration->index_context,
			&source_arguments[declaration->parameter_count],
			index_ordinal,
			index_ordinal,
			&source_index_classifier
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	uint32_t source_index =
		source_arguments[declaration->parameter_count + index_ordinal];
	uint32_t branch_index;
	uint32_t branch_index_classifier;
	if (prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			source_index,
			operation_case->refinement_substitution,
			&branch_index
		) != 0 || prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			source_index_classifier,
			operation_case->refinement_substitution,
			&branch_index_classifier
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	int branch_index_matches;
	if (prototype_term_view_shape_equal(
			ctx->terms,
			branch_index,
			branch_arguments[declaration->parameter_count + index_ordinal],
			&branch_index_matches
		) != 0) {
		return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
	}
	if (!branch_index_matches) {
		/* The two terms are typed by the same nominal family index telescope.
		 * Core agreement is accepted only as validation of this already-derived
		 * correspondence; it is never used to find a replacement location. */
		if (prototype_term_core_shape_equal(
				ctx->terms,
				branch_index,
				branch_arguments[declaration->parameter_count + index_ordinal],
				&branch_index_matches
			) != 0) {
			return OPERATION_MOTIVE_CORRESPONDENCE_INVALID;
		}
		if (!branch_index_matches) {
			return OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT;
		}
	}
	p_correspondence->family_type_id = family_type_id;
	p_correspondence->index_ordinal = index_ordinal;
	p_correspondence->source_index = source_index;
	p_correspondence->branch_index = branch_index;
	p_correspondence->source_index_classifier = source_index_classifier;
	p_correspondence->branch_index_classifier = branch_index_classifier;
	p_correspondence->branch_context = operation_case->context_id;
	p_correspondence->refinement_substitution =
		operation_case->refinement_substitution;
	return OPERATION_MOTIVE_CORRESPONDENCE_AVAILABLE;
}

static int operation_motive_abstract_index(
	struct compile_context* ctx,
	uint32_t body,
	uint32_t index_term,
	uint32_t replacement,
	uint32_t* p_result,
	int* p_dependency_found
) {
	if (!ctx || !p_result || !p_dependency_found ||
		body >= ctx->terms->term_count || index_term >= ctx->terms->term_count ||
		replacement >= ctx->terms->term_count) {
		return -1;
	}
	if (prototype_term_graph_replace_exact(
			ctx->terms,
			prototype_type_view_rebuild_context_from_db(ctx->type_declarations),
			body,
			index_term,
			replacement,
			p_result
		) != 0) {
		return -1;
	}
	*p_dependency_found = *p_result != body;
	return 0;
}

static int operation_motive_subgraph_contains_occurrence_impl(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation,
	uint32_t depth,
	uint8_t* visited
) {
	if (!ctx || !ctx->metadata || !visited || root_operation >=
			ctx->metadata->typed_occurrences.occurrence_count || target_operation >=
			ctx->metadata->typed_occurrences.occurrence_count || depth > 128) {
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
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		int wrapped = operation_motive_subgraph_contains_occurrence_impl(
			ctx, operation->wrapped_occurrence, target_operation, depth + 1,
			visited
		);
		if (wrapped != 0) {
			return wrapped;
		}
	}
	if (operation->edge_count == 0) {
		return 0;
	}
	if (operation->first_edge == PROTOTYPE_INVALID_ID ||
		operation->first_edge > ctx->metadata->typed_occurrences.edge_count ||
		operation->edge_count > ctx->metadata->typed_occurrences.edge_count -
			operation->first_edge) {
		return -1;
	}
	for (uint32_t edge = 0; edge < operation->edge_count; ++edge) {
		uint32_t child = ctx->metadata->typed_occurrences.edges[
			operation->first_edge + edge
		].child_occurrence;
		int contains = operation_motive_subgraph_contains_occurrence_impl(
			ctx, child, target_operation, depth + 1, visited
		);
		if (contains != 0) {
			return contains;
		}
	}
	return 0;
}

static int operation_motive_subgraph_contains_occurrence(
	const struct compile_context* ctx,
	uint32_t root_operation,
	uint32_t target_operation
) {
	if (!ctx || !ctx->metadata ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	uint8_t visited[4096] = { 0 };
	return operation_motive_subgraph_contains_occurrence_impl(
		ctx, root_operation, target_operation, 0, visited
	);
}

static int operation_motive_validate_recursive_equations(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t branch_operation,
	struct operation_motive_equation_validation* p_validation
) {
	if (!ctx || !p_validation) {
		return -1;
	}
	for (uint32_t ih_operation = 0;
		ih_operation < ctx->metadata->typed_occurrences.occurrence_count;
		++ih_operation) {
		if (ctx->classifier_solver.recursive_equation_owners[ih_operation] !=
			match_operation_id) {
			continue;
		}
		int contains = operation_motive_subgraph_contains_occurrence(
			ctx, branch_operation, ih_operation
		);
		if (contains < 0) {
			return -1;
		}
		if (contains == 0) {
			continue;
		}
		uint32_t argument_operation;
		const struct prototype_typed_occurrence* ih =
			&ctx->metadata->typed_occurrences.occurrences[ih_operation];
		if (ih->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
			ih->ih_owner_occurrence != match_operation_id ||
			compile_occurrence_child(
				ctx, ih_operation, PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0,
				&argument_operation
			) != 0) {
			return -1;
		}
		int guardedness = operation_solver_validate_guarded_motive_occurrence(
			ctx, ih_operation, match_operation_id, argument_operation
		);
		if (p_validation->ih_operation == PROTOTYPE_INVALID_ID) {
			p_validation->ih_operation = ih_operation;
			p_validation->guardedness_status = guardedness;
		}
		p_validation->used_recursive_equation = 1;
		if (guardedness != 0) {
			p_validation->guardedness_status = guardedness;
			return 1;
		}
	}
	return 0;
}

static int operation_motive_validate_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t case_index,
	uint32_t motive,
	struct operation_motive_equation_validation* p_validation
) {
	if (!ctx || !p_validation || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		motive >= ctx->terms->term_count) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	memset(p_validation, 0, sizeof(*p_validation));
	p_validation->expected_classifier = PROTOTYPE_INVALID_ID;
	p_validation->actual_classifier = PROTOTYPE_INVALID_ID;
	p_validation->ih_operation = PROTOTYPE_INVALID_ID;
	p_validation->conversion_status = PROTOTYPE_TERM_CONVERSION_INVALID;
	p_validation->guardedness_status = 1;

	const struct prototype_typed_occurrence* match_operation =
		&ctx->metadata->typed_occurrences.occurrences[match_operation_id];
	const struct operation_constraint* equation =
		operation_solver_motive_equation(
			&ctx->constraints, match_operation_id, case_index
		);
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH || !equation ||
		equation->payload.classifier.goal.motive_case.branch_operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	uint32_t constructor;
	uint32_t constructor_classifier;
	int constructor_status = operation_solver_match_case_constructor_classifier(
		ctx,
		match_operation_id,
		case_index,
		&constructor,
		&constructor_classifier
	);
	if (constructor_status < 0) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	if (constructor_status > 0) {
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	uint32_t branch_operation =
		equation->payload.classifier.goal.motive_case.branch_operation;
	uint32_t actual = operation_solver_classifier(ctx, branch_operation);
	if (actual == PROTOTYPE_INVALID_ID) {
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	int recursive_validation = operation_motive_validate_recursive_equations(
		ctx, match_operation_id, branch_operation, p_validation
	);
	if (recursive_validation < 0) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	if (recursive_validation > 0) {
		return OPERATION_MOTIVE_EQUATION_MISMATCH;
	}
	if (p_validation->used_recursive_equation) {
		for (uint32_t ih_operation = 0;
			ih_operation < ctx->metadata->typed_occurrences.occurrence_count;
			++ih_operation) {
			if (ctx->classifier_solver.recursive_equation_owners[ih_operation] !=
					match_operation_id) {
				continue;
			}
			int contains = operation_motive_subgraph_contains_occurrence(
				ctx, branch_operation, ih_operation
			);
			if (contains < 0) {
				return OPERATION_MOTIVE_EQUATION_INVALID;
			}
			if (contains > 0 &&
				(ctx->classifier_solver.ih_projected_motives[ih_operation] != motive ||
				 ctx->classifier_solver.classifier_metas[ih_operation].classifier ==
					PROTOTYPE_INVALID_ID)) {
				/* First project this candidate to every recursive premise, then
				 * recompute the branch through the ordinary dependency graph. */
				return OPERATION_MOTIVE_EQUATION_PENDING;
			}
		}
	}
	uint32_t branch_motive = motive;
	uint32_t operation_case_id = match_operation->first_case + case_index;
	if (operation_case_id >= ctx->metadata->typed_occurrences.case_count) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&ctx->metadata->typed_occurrences.cases[operation_case_id];
	if (prototype_typed_occurrence_match_case_is_solved(operation_case) &&
		prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			motive,
			operation_case->refinement_substitution,
			&branch_motive
		) != 0) {
		return OPERATION_MOTIVE_EQUATION_INVALID;
	}
	uint32_t expected;
	uint32_t normalized_expected;
	if (prototype_judgement_apply_indexed_motive(
			ctx->terms,
			ctx->type_declarations,
			branch_motive,
			constructor,
			constructor_classifier,
			1,
			&expected
		) != 0 || prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			expected,
			&normalized_expected
		) != 0) {
		return OPERATION_MOTIVE_EQUATION_PENDING;
	}
	p_validation->expected_classifier = normalized_expected;
	p_validation->actual_classifier = actual;
	p_validation->conversion_status = prototype_judgement_classifier_conversion(
		ctx->terms,
		ctx->type_declarations,
		normalized_expected,
		actual
	).status;
	if (p_validation->conversion_status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return OPERATION_MOTIVE_EQUATION_VALID;
	}
	/* Match motive equations constrain the branch result. The computation
	 * envelope is joined independently from all branch effect rows and
	 * totalities, so a value result and its returning-computation classifier
	 * denote the same motive equation without collapsing their CBPV categories. */
	struct prototype_term_classifier_view expected_view;
	struct prototype_term_classifier_view actual_view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			normalized_expected,
			&expected_view
		) == 0 && prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			actual,
			&actual_view
		) == 0) {
		uint32_t expected_result = expected_view.category ==
				PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			expected_view.computation_kind ==
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ?
				expected_view.result : normalized_expected;
		uint32_t actual_result = actual_view.category ==
				PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			actual_view.computation_kind ==
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ?
				actual_view.result : actual;
		p_validation->conversion_status =
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				expected_result,
				actual_result
			).status;
		if (p_validation->conversion_status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return OPERATION_MOTIVE_EQUATION_VALID;
		}
	}
	if (p_validation->used_recursive_equation &&
		p_validation->guardedness_status == 0) {
		/* Every recursive premise has already been projected from this exact
		 * motive and its downstream branch has been recomputed. The remaining
		 * difference is the guarded recursive equation itself, not an assertion
		 * of ordinary DefEq. */
		return OPERATION_MOTIVE_EQUATION_VALID;
	}
	return OPERATION_MOTIVE_EQUATION_MISMATCH;
}

static void operation_motive_clear_recursive_equations(
	struct compile_context* ctx,
	uint32_t match_operation_id
) {
	if (!ctx) {
		return;
	}
	int changed = 0;
	for (uint32_t operation = 0;
		operation < ctx->metadata->typed_occurrences.occurrence_count;
		++operation) {
		if (ctx->classifier_solver.recursive_equation_owners[operation] ==
			match_operation_id) {
			ctx->classifier_solver.recursive_equation_owners[operation] =
				PROTOTYPE_INVALID_ID;
			changed = 1;
		}
	}
	if (changed) {
		ctx->classifier_solver.recursive_equation_revision++;
	}
}

static int operation_motive_has_recursive_equations(
	const struct compile_context* ctx,
	uint32_t match_operation_id
) {
	if (!ctx || !ctx->metadata || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t operation = 0;
		operation < ctx->metadata->typed_occurrences.occurrence_count;
		++operation) {
		if (ctx->classifier_solver.recursive_equation_owners[operation] ==
			match_operation_id) {
			return 1;
		}
	}
	return 0;
}

static int operation_motive_register_recursive_equation(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t ih_operation
) {
	if (!ctx || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count || ih_operation >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[ih_operation].tag !=
			PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		ctx->metadata->typed_occurrences.occurrences[ih_operation].
			ih_owner_occurrence != match_operation_id) {
		return -1;
	}
	if (ctx->classifier_solver.recursive_equation_owners[ih_operation] !=
		match_operation_id) {
		ctx->classifier_solver.recursive_equation_owners[ih_operation] =
			match_operation_id;
		ctx->classifier_solver.recursive_equation_revision++;
	}
	return 0;
}

/* Diagnostics are emitted only after solving has stopped. Candidate metadata
 * is therefore a write-only explanation channel and never a solver input. */
static int operation_motive_add_terminal_diagnostic(
	struct compile_context* ctx,
	uint32_t match_operation_id,
	uint32_t case_index
) {
	if (!ctx || match_operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct operation_constraint* equation =
		operation_solver_motive_equation(
			&ctx->constraints, match_operation_id, case_index
		);
	if (!equation) {
		return -1;
	}
	const struct operation_motive_solution* solution =
		&ctx->classifier_solver.motive_solutions[match_operation_id];
	struct operation_motive_equation_validation validation = {
		.expected_classifier = PROTOTYPE_INVALID_ID,
		.actual_classifier = PROTOTYPE_INVALID_ID,
		.ih_operation = PROTOTYPE_INVALID_ID,
		.conversion_status = PROTOTYPE_TERM_CONVERSION_INVALID,
		.guardedness_status = 1
	};
	int validation_status = OPERATION_MOTIVE_EQUATION_PENDING;
	if (solution->motive < ctx->terms->term_count) {
		validation_status = operation_motive_validate_equation(
			ctx, match_operation_id, case_index, solution->motive, &validation
		);
	}
	if (validation_status == OPERATION_MOTIVE_EQUATION_VALID) {
		return 1;
	}
	int reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_EQUATION_MISMATCH;
	int abstraction_status = OPERATION_MOTIVE_CORRESPONDENCE_AVAILABLE;
	uint64_t status_mask = 0;
	if (solution->conflict_mask != 0) {
		reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_CORRESPONDENCE_CONFLICT;
		abstraction_status = OPERATION_MOTIVE_CORRESPONDENCE_CONFLICT;
		status_mask = solution->conflict_mask;
	} else if (solution->unavailable_mask != 0) {
		reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
		abstraction_status = OPERATION_MOTIVE_CORRESPONDENCE_UNAVAILABLE;
		status_mask = solution->unavailable_mask;
	} else if (validation_status == OPERATION_MOTIVE_EQUATION_MISMATCH &&
		validation.ih_operation != PROTOTYPE_INVALID_ID &&
		validation.guardedness_status != 0) {
		reason = PROTOTYPE_COMPILE_DIAGNOSTIC_MOTIVE_GUARDEDNESS;
	}
	uint32_t index_ordinal = PROTOTYPE_INVALID_ID;
	for (uint32_t index = 0; index < 64; ++index) {
		if ((status_mask & (UINT64_C(1) << index)) != 0) {
			index_ordinal = index;
			break;
		}
	}
	uint32_t branch_operation =
		equation->payload.classifier.goal.motive_case.branch_operation;
	const struct prototype_typed_occurrence* branch = branch_operation <
		ctx->metadata->typed_occurrences.occurrence_count ?
		&ctx->metadata->typed_occurrences.occurrences[branch_operation] : NULL;
	uint32_t source_ast = branch ? branch->source_ast : equation->source_ast;
	ctx->had_error = 1;
	return prototype_compile_metadata_add_diagnostic(
		ctx->metadata,
		(struct prototype_compile_diagnostic) {
			.phase = PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
			.reason = reason,
			.source_ast = source_ast,
			.span = source_ast < ctx->asts->node_count ?
				ctx->asts->nodes[source_ast].span :
				(struct prototype_source_span) { 0, 0 },
			.occurrence_id = branch_operation,
			.constraint_id = equation->id,
			.expected_classifier = validation.expected_classifier,
			.actual_classifier = validation.actual_classifier,
			.context_id = branch ? branch->context_id : PROTOTYPE_INVALID_ID,
			.constructor_ordinal =
				equation->payload.classifier.goal.motive_case.constructor_index,
			.field_ordinal = index_ordinal,
			.dependency_mask = solution->dependency_mask,
			.abstraction_status = abstraction_status,
			.guardedness_status = validation.guardedness_status,
			.conversion_status = validation.conversion_status
		}
	);
}
