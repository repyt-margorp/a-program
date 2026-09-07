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
	uint32_t evidence_context,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* evidence,
	uint32_t capacity,
	uint32_t* p_count
) {
	if (!ctx || !evidence || !p_count || operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		evidence_context >= ctx->metadata->contexts.context_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	*p_count = 0;
	struct prototype_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, classifier, &view
		) != 0 || view.category != PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION ||
		view.computation_kind != PROTOTYPE_COMPUTATION_KIND_RETURNING) {
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
				if (lambda->implicit_effect_row_binders[i] == binding_id) {
					declares = 1;
					break;
				}
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
				) != 0) {
				return -1;
			}
			uint32_t binder_classifier = operation_solver_binder_classifier(
				ctx, lambda_id
			);
			if (operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					evidence_context,
					binder_var,
					binder_classifier,
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

static int operation_solver_materialize_induction_hypothesis_projection_judgement(
	struct compile_context* ctx,
	uint32_t typed_projection
) {
	const struct prototype_typed_projection* projection = ctx && ctx->typing ?
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		) : NULL;
	if (!ctx || !ctx->metadata || !projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t operation_id = projection->source_occurrence;
	const struct prototype_typed_occurrence* stored_operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	struct prototype_typed_occurrence proof_operation = *stored_operation;
	uint32_t core_term;
	if (operation_typed_projection_publication_typing_answer(
			ctx, typed_projection,
			&proof_operation.context_id,
			&proof_operation.context_action_substitution,
			&core_term,
			&proof_operation.classifier
		) != 0) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation = &proof_operation;
	uint32_t classifier = operation->classifier;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
		classifier == PROTOTYPE_INVALID_ID ||
		core_term >= ctx->terms->term_count ||
		ctx->terms->terms[core_term].tag !=
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
				relations[i].context_id == operation->context_id &&
				relations[i].subject == core_term &&
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
	/* The IH term retains the source Match scope identity. Only the owner's
	 * classifier is projected into the branch Context to recover its motive. */
	if (prototype_term_reindex(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			match_classifier,
			operation->context_action_substitution,
			&match_classifier
		) != 0) {
		return -1;
	}
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
	struct prototype_judgement_selected_evidence effect_row_evidence[16];
	uint32_t effect_row_evidence_count;
	if (operation_solver_collect_effect_row_assumptions(
			ctx,
			operation_id,
			operation->context_id,
			classifier,
			effect_row_evidence,
			16,
			&effect_row_evidence_count
		) != 0) {
		fprintf(stderr, "IH materialization effect evidence failed operation=%u context=%u classifier=%u\n", operation_id, operation->context_id, classifier);
		return -1;
	}
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, operation->context_id
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, operation_id
	);
	int expand_status = prototype_judgement_delta_expand_induction_hypothesis(
		&ctx->judgement_delta,
		ctx->terms,
		core_term,
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

static int operation_solver_materialize_induction_hypothesis_judgement(
	struct compile_context* ctx,
	uint32_t operation_id
) {
	uint32_t typed_projection = operation_solver_publication_projection(
		ctx, operation_id
	);
	if (typed_projection == PROTOTYPE_INVALID_ID) return -1;
	return operation_solver_materialize_induction_hypothesis_projection_judgement(
		ctx, typed_projection
	);
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

static int operation_solver_var_evidence_authority(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t subject,
	int* p_authority_kind,
	uint32_t* p_authority_id,
	uint32_t* p_context,
	uint32_t* p_substitution
);

static int operation_solver_var_projection_evidence_authority(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t subject,
	int* p_authority_kind,
	uint32_t* p_authority_id,
	uint32_t* p_context,
	uint32_t* p_substitution
);

static uint32_t operation_solver_evidence_owner(
	struct compile_context* ctx,
	uint32_t operation_id
);

static int operation_solver_select_authority_evidence_in_context(
	struct compile_context* ctx,
	uint32_t operation_id,
	int authority_kind,
	uint32_t authority_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	if (!ctx || !ctx->metadata || context_id >=
			ctx->metadata->contexts.context_count) {
		return -1;
	}
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
		if (selection_status == 1 && ctx->judgement) {
			selection_status = prototype_judgement_select_evidence_by_authority(
				ctx->judgement,
				authority_kind,
				authority_id,
				source_context,
				subject,
				classifier,
				&source_evidence
			);
		}
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

static int operation_solver_select_operation_evidence_in_context(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
);

static int operation_solver_reify_core_proof(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t core_term,
	uint32_t depth
);

static int operation_solver_reify_projected_core_proof(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t core_term,
	uint32_t depth
);

/* The proof producer and every accepted-evidence audit use this one boundary.
 * A zero result is not a failed typing judgement: it denotes an unreachable
 * occurrence or a classifier whose external effect row remains intentionally
 * outside the closed kernel proof fragment. */
static int operation_solver_occurrence_has_closed_evidence_boundary(
	const struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata || operation_id >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	int impossible_branch = operation_solver_occurrence_is_in_impossible_branch(
		ctx, operation_id
	);
	if (impossible_branch != 0) {
		return impossible_branch < 0 ? -1 : 0;
	}
	uint32_t classifier =
		ctx->metadata->typed_occurrences.occurrences[operation_id].classifier;
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	int free_effect_row = operation_classifier_contains_unowned_effect_row(
		ctx, operation_id, classifier
	);
	return free_effect_row < 0 ? -1 : free_effect_row == 0;
}

static int operation_solver_accepted_match_motive_claim_exists(
	const struct prototype_judgement_db* judgement,
	uint32_t occurrence_id
) {
	if (!judgement) {
		return 0;
	}
	for (uint32_t derivation_id = 0;
		derivation_id < judgement->derivation_count;
		++derivation_id) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[derivation_id];
		if (derivation->proof_kind !=
				PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE ||
			derivation->conclusion_claim_id >= judgement->claim_count) {
			continue;
		}
		const struct prototype_judgement_claim* claim =
			&judgement->claims[derivation->conclusion_claim_id];
		if (claim->proposition_id >= judgement->proposition_count) {
			return -1;
		}
		const struct prototype_judgement_proposition* proposition =
			&judgement->propositions[claim->proposition_id];
		if (proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->occurrence_id == occurrence_id) {
			return 1;
		}
	}
	return 0;
}


/* A solved Layer T motive is publishable only when the accepted proof DAG has
 * replayable kernel evidence for that exact typed occurrence. This is a
 * read-only cross-check: proof materialization cannot alter the solver cell. */
static int operation_solver_validate_accepted_motive_evidence(
	const struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata || !ctx->judgement) {
		return -1;
	}
	for (uint32_t operation_id = 0; operation_id <
		ctx->metadata->typed_occurrences.occurrence_count; ++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			continue;
		}
		int closed_evidence =
			operation_solver_occurrence_has_closed_evidence_boundary(
			ctx, operation_id
		);
		if (closed_evidence < 0) {
			return -1;
		}
		if (!closed_evidence) {
			continue;
		}
		if (operation->match_motive == PROTOTYPE_INVALID_ID) {
			fprintf(
				stderr,
				"accepted motive evidence: unpublished Match operation=%u\n",
				operation_id
			);
			return -1;
		}
		int found = operation_solver_accepted_match_motive_claim_exists(
			ctx->judgement, operation_id
		);
		if (found < 0) {
			return -1;
		}
		if (!found) {
			if (getenv("A_PROGRAM_FORMATION_TRACE")) {
				for (uint32_t derivation_id = 0;
					derivation_id < ctx->judgement->derivation_count;
					++derivation_id) {
					const struct prototype_judgement_derivation* derivation =
						&ctx->judgement->derivations[derivation_id];
					if (derivation->proof_kind !=
							PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE ||
						derivation->conclusion_claim_id >=
							ctx->judgement->claim_count) {
						continue;
					}
					const struct prototype_judgement_claim* trace_claim =
						&ctx->judgement->claims[derivation->conclusion_claim_id];
					if (trace_claim->proposition_id >=
						ctx->judgement->proposition_count) {
						continue;
					}
					const struct prototype_judgement_proposition* trace_proposition =
						&ctx->judgement->propositions[trace_claim->proposition_id];
					fprintf(stderr,
						"accepted motive candidate derivation=%u claim=%u occurrence=%u context=%u subject=%u classifier=%u authority=%d:%u\n",
						derivation_id,
						derivation->conclusion_claim_id,
						trace_proposition->occurrence_id,
						trace_proposition->context_id,
						trace_proposition->subject,
						trace_proposition->classifier,
						trace_proposition->authority_kind,
						trace_proposition->authority_id);
				}
			}
			fprintf(
				stderr,
				"accepted motive evidence missing operation=%u core=%u "
				"classifier=%u motive=%u\n",
				operation_id, operation->core_term, operation->classifier,
				operation->match_motive
			);
			return -1;
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

static int operation_solver_materialize_structural_term_evidence(
	struct compile_context* ctx,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t depth,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	if (!ctx || !ctx->metadata || !p_evidence ||
		context_id >= ctx->metadata->contexts.context_count ||
		subject >= ctx->terms->term_count ||
		classifier >= ctx->terms->term_count || depth > ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &ctx->terms->terms[subject];
	int authority_kind;
	uint32_t authority_id = subject;
	if (term->tag == PROTOTYPE_TERM_VAR) {
		uint32_t binding_context_id;
		if (prototype_context_find_binding(
				&ctx->metadata->contexts, context_id, term->as.var.binding_id,
				&binding_context_id
			) != 0) {
			return -1;
		}
		classifier = operation_context_classifier_term(ctx, binding_context_id);
		if (classifier == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING;
	} else if (term->tag == PROTOTYPE_TERM_CONSTRUCTOR) {
		authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	} else if (term->tag == PROTOTYPE_TERM_APP) {
		authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION;
	} else {
		return 1;
	}
	if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	int select_status = prototype_judgement_delta_select_evidence_by_authority(
		&ctx->judgement_delta, authority_kind, authority_id, context_id, subject,
		classifier, p_evidence
	);
	if (select_status == 0) {
		return 0;
	}
	if (select_status < 0 || select_status == 2) {
		return -1;
	}
	prototype_judgement_delta_set_context(&ctx->judgement_delta, context_id);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, PROTOTYPE_INVALID_ID
	);
	if (term->tag == PROTOTYPE_TERM_VAR) {
		int formation_status = materialize_classifier_application_formation(
			ctx, context_id, classifier
		);
		int assumption_status = formation_status == 0 ?
			prototype_judgement_delta_record_context_binding_assumption(
				&ctx->judgement_delta, ctx->terms, term->as.var.binding_id,
				classifier
			) : -1;
		if (formation_status != 0 || assumption_status != 0) {
			return 1;
		}
	} else if (term->tag == PROTOTYPE_TERM_CONSTRUCTOR) {
		if (prototype_judgement_delta_record_constructor_intro(
				&ctx->judgement_delta, ctx->terms, ctx->type_declarations, subject,
				classifier
			) != 0) {
			return -1;
		}
	} else {
		uint32_t head;
		uint32_t owner;
		uint32_t constructor_index;
		uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t argument_count;
		uint32_t expected_domains[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t expected_domain_count;
		uint32_t argument_occurrences[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		struct prototype_judgement_selected_evidence
			argument_evidence[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		if (prototype_term_constructor_spine_info(
				ctx->terms, subject, &head, &owner, &constructor_index, arguments,
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES, &argument_count
			) != 0 || argument_count == 0 ||
			operation_constructor_spine_expected_domains(
				ctx, context_id, subject,
				PROTOTYPE_INVALID_ID, expected_domains,
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES, &expected_domain_count
			) != 0 || expected_domain_count != argument_count) {
			return 1;
		}
		for (uint32_t i = 0; i < argument_count; ++i) {
			int argument_status =
				operation_solver_materialize_structural_term_evidence(
					ctx, context_id, arguments[i], expected_domains[i], depth + 1,
					&argument_evidence[i]
				);
			if (argument_status != 0) {
				return 1;
			}
			argument_occurrences[i] = PROTOTYPE_INVALID_ID;
		}
		prototype_judgement_delta_set_context(&ctx->judgement_delta, context_id);
		prototype_judgement_delta_set_occurrence(
			&ctx->judgement_delta, PROTOTYPE_INVALID_ID
		);
		(void)head;
		(void)owner;
		(void)constructor_index;
		if (prototype_judgement_delta_record_constructor_spine(
				&ctx->judgement_delta, ctx->terms, ctx->type_declarations, subject,
				classifier, argument_occurrences, argument_evidence, argument_count
			) != 0) {
			return -1;
		}
	}
	if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	return prototype_judgement_delta_select_evidence_by_authority(
		&ctx->judgement_delta, authority_kind, authority_id, context_id, subject,
		classifier, p_evidence
	);
}

static int operation_solver_substitution_assignment_classifier(
	struct compile_context* ctx,
	uint32_t substitution_id,
	uint32_t* p_classifier
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(
			ctx ? &ctx->metadata->substitutions : NULL, substitution_id
		);
	if (!ctx || !p_classifier || !substitution || substitution->kind !=
			PROTOTYPE_SUBSTITUTION_EXTEND || substitution->term >=
			ctx->terms->term_count) {
		return -1;
	}
	uint32_t target_classifier = PROTOTYPE_INVALID_ID;
	struct prototype_context_classifier_view context_view =
		operation_type_context_view(ctx);
	int target_status = prototype_substitution_term_classifier_in_view(
		&context_view,
		&ctx->metadata->substitutions,
		ctx->terms,
		ctx->type_declarations,
		substitution_id,
		&target_classifier
	);
	if (target_status != 0) {
		return -1;
	}
	/* For an EXTEND substitution, the target binder classifier acted on by the
	 * prefix is the proposition that the assignment must inhabit. A bare VAR's
	 * source-Context classifier can be less refined and is not a second typing
	 * authority; exact source evidence is selected against this result below. */
	*p_classifier = target_classifier;
	return 0;
}

static int operation_solver_materialize_refinement_binding_evidence_with_mode(
	struct compile_context* ctx,
	uint32_t substitution_id,
	uint32_t depth,
	int require_accepted
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
		int first_status =
			operation_solver_materialize_refinement_binding_evidence_with_mode(
				ctx, substitution->first, depth + 1, require_accepted
			);
		int second_status = first_status == 0 ?
			operation_solver_materialize_refinement_binding_evidence_with_mode(
				ctx, substitution->second, depth + 1, require_accepted
			) : 0;
		if (first_status != 0 || second_status != 0) {
			const struct prototype_substitution* first =
				prototype_substitution_get(
					&ctx->metadata->substitutions, substitution->first
				);
			const struct prototype_substitution* second =
				prototype_substitution_get(
					&ctx->metadata->substitutions, substitution->second
				);
			fprintf(stderr,
				"refinement evidence compose failed substitution=%u first=%u:%d "
				"second=%u:%d depth=%u first-kind=%d first-source=%u "
				"first-target=%u first-prefix=%u first-term=%u second-kind=%d "
				"second-source=%u second-target=%u second-prefix=%u second-term=%u\n",
				substitution_id, substitution->first, first_status,
				substitution->second, second_status, depth,
				first ? first->kind : -1,
				first ? first->source_context : PROTOTYPE_INVALID_ID,
				first ? first->target_context : PROTOTYPE_INVALID_ID,
				first ? first->first : PROTOTYPE_INVALID_ID,
				first ? first->term : PROTOTYPE_INVALID_ID,
				second ? second->kind : -1,
				second ? second->source_context : PROTOTYPE_INVALID_ID,
				second ? second->target_context : PROTOTYPE_INVALID_ID,
				second ? second->first : PROTOTYPE_INVALID_ID,
				second ? second->term : PROTOTYPE_INVALID_ID);
			return -1;
		}
		return 0;
	}
	if (substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND) {
		return 0;
	}
	if (operation_solver_materialize_refinement_binding_evidence_with_mode(
			ctx, substitution->first, depth + 1, require_accepted
		) != 0) {
		const struct prototype_substitution* first = prototype_substitution_get(
			&ctx->metadata->substitutions, substitution->first
		);
		fprintf(stderr,
			"refinement evidence prefix failed substitution=%u first=%u depth=%u "
			"first-kind=%d source=%u target=%u term=%u\n",
			substitution_id, substitution->first, depth,
			first ? first->kind : -1,
			first ? first->source_context : PROTOTYPE_INVALID_ID,
			first ? first->target_context : PROTOTYPE_INVALID_ID,
			first ? first->term : PROTOTYPE_INVALID_ID);
		return -1;
	}
	if (substitution->term >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t expected_classifier;
	if (operation_solver_substitution_assignment_classifier(
			ctx, substitution_id, &expected_classifier
		) != 0) {
		return -1;
	}
	/* Target Context plus prefix is the sole classifier authority for EXTEND. */
	uint32_t substitution_classifier = expected_classifier;
	uint32_t accepted_claim =
		prototype_compile_metadata_accepted_substitution_claim(
			ctx->metadata, substitution_id
		);
	if (accepted_claim != PROTOTYPE_INVALID_ID) {
		return prototype_cwf_substitution_claim_matches_assignment(
			&ctx->metadata->substitutions,
			ctx->judgement,
			substitution_id,
			accepted_claim
		) ? 0 : -1;
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
			substitution_classifier,
			&existing_evidence
		);
	} else {
		existing_status = operation_solver_select_evidence_in_context(
			ctx,
			PROTOTYPE_INVALID_ID,
			substitution->source_context,
			substitution->term,
			substitution_classifier,
			&existing_evidence
		);
	}
	if (existing_status < 0) {
		fprintf(stderr,
			"refinement evidence selection failed substitution=%u term=%u "
			"classifier=%u context=%u status=%d\n",
			substitution_id, substitution->term, substitution_classifier,
			substitution->source_context, existing_status);
		return -1;
	}
	if (!require_accepted && (existing_status == 0 || existing_status == 2)) {
		/* Construction within one JudgementDelta may have several derivations for
		 * the same proposition. They are sufficient until the publication fixed
		 * point chooses an accepted Claim. */
		return 0;
	}
	/* Delta selection can report one or several candidates that have not yet
	 * entered the accepted proof DAG. A CwF substitution is publishable only
	 * after an exact accepted HAS_TYPE Claim exists. */
	for (uint32_t claim_id = 0; claim_id < ctx->judgement->claim_count; ++claim_id) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(ctx->judgement, claim_id);
		if (proposition &&
			proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->context_id == substitution->source_context &&
			proposition->subject == substitution->term &&
			proposition->classifier == substitution_classifier) {
			for (uint32_t derivation_id = 0;
				derivation_id < ctx->judgement->derivation_count;
				++derivation_id) {
				const struct prototype_judgement_derivation* derivation =
					prototype_judgement_derivation_get(
						ctx->judgement, derivation_id
					);
				if (derivation && derivation->conclusion_claim_id == claim_id) {
					return prototype_compile_metadata_record_accepted_substitution_claim(
						ctx->metadata, substitution_id, claim_id
					);
				}
			}
		}
	}
	/* Static endpoints embedded in generated type expressions still originate
	 * from TypedOccurrences. A substitution extension needs an exact CwF typing
	 * witness for that endpoint, regardless of its Core tag. Reify the owning
	 * occurrence before falling back to structural binder/constructor rules. */
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->core_term != substitution->term ||
			operation->context_id != substitution->source_context ||
			operation->classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				operation->classifier,
				substitution_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			continue;
		}
		int reify_status = operation_solver_reify_core_proof(
			ctx, operation_id, substitution->term, depth + 1
		);
		if (reify_status != 0) {
			return reify_status;
		}
		struct prototype_judgement_selected_evidence operation_evidence;
		int select_status = operation_solver_select_operation_evidence_in_context(
			ctx,
			operation_id,
			substitution->source_context,
			substitution->term,
			operation->classifier,
			&operation_evidence
		);
		if (select_status != 0) {
			return select_status;
		}
		if (operation_evidence.classifier != substitution_classifier) {
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, substitution->source_context
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			if (prototype_judgement_delta_record_expected_type_exposure(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation_id,
					&operation_evidence,
					substitution_classifier,
					substitution->term
				) != 0) {
				return -1;
			}
		}
		if (require_accepted) {
			ctx->evidence_workspace.materialization_substitution_pending = 1;
		}
		return 0;
	}
	if (ctx->terms->terms[substitution->term].tag == PROTOTYPE_TERM_CONSTRUCTOR ||
		ctx->terms->terms[substitution->term].tag == PROTOTYPE_TERM_APP) {
		struct prototype_judgement_selected_evidence structural_evidence;
		int structural_status = operation_solver_materialize_structural_term_evidence(
			ctx,
			substitution->source_context,
			substitution->term,
			substitution_classifier,
			depth + 1,
			&structural_evidence
		);
		if (structural_status == 0 && require_accepted) {
			ctx->evidence_workspace.materialization_substitution_pending = 1;
		}
		if (structural_status == 1) {
			return require_accepted ? -1 : 0;
		}
		return structural_status;
	}
	if (ctx->terms->terms[substitution->term].tag != PROTOTYPE_TERM_VAR) {
		if (require_accepted) {
			fprintf(
				stderr,
				"accepted substitution evidence has no producer substitution=%u "
				"term=%u tag=%d classifier=%u context=%u\n",
				substitution_id,
				substitution->term,
				ctx->terms->terms[substitution->term].tag,
				substitution_classifier,
				substitution->source_context
			);
			return -1;
		}
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
		fprintf(stderr,
			"refinement evidence binding unavailable substitution=%u binding=%u "
			"context=%u\n",
			substitution_id, binding_id, substitution->source_context);
		return -1;
	}
	const struct prototype_context* binding_context = prototype_context_get(
		&ctx->metadata->contexts, binding_context_id
	);
	uint32_t binding_classifier = operation_context_classifier_term(
		ctx, binding_context_id
	);
	if (!binding_context ||
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			binding_classifier,
				substitution_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		fprintf(stderr,
			"refinement evidence classifier mismatch substitution=%u binding=%u "
			"binding-context=%u actual=%u expected=%u\n",
			substitution_id, binding_id, binding_context_id,
			binding_classifier,
				substitution_classifier);
		return -1;
	}
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, substitution->source_context
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, PROTOTYPE_INVALID_ID
	);
	uint32_t classifier_spine = substitution_classifier;
	while (classifier_spine < ctx->terms->term_count &&
		ctx->terms->terms[classifier_spine].tag == PROTOTYPE_TERM_APP) {
		uint32_t argument = ctx->terms->terms[classifier_spine].as.app.argument;
		if (argument < ctx->terms->term_count &&
			ctx->terms->terms[argument].tag == PROTOTYPE_TERM_VAR) {
			uint32_t argument_binding_context;
			if (prototype_context_find_binding(
					&ctx->metadata->contexts,
					substitution->source_context,
					ctx->terms->terms[argument].as.var.binding_id,
					&argument_binding_context
				) != 0) {
				return -1;
			}
			const struct prototype_context* argument_binding =
				prototype_context_get(
					&ctx->metadata->contexts, argument_binding_context
				);
			uint32_t argument_classifier = operation_context_classifier_term(
				ctx, argument_binding_context
			);
			int argument_status = !argument_binding ||
				argument_classifier == PROTOTYPE_INVALID_ID ? -1 :
				operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					substitution->source_context,
					argument,
					argument_classifier,
					&(struct prototype_judgement_selected_evidence){0}
				);
			if (argument_status != 0) {
				if (getenv("A_PROGRAM_FORMATION_TRACE")) {
					fprintf(
						stderr,
						"classifier argument evidence failed context=%u argument=%u "
						"classifier=%u status=%d\n",
						substitution->source_context,
						argument,
						argument_classifier,
						argument_status
					);
				}
				return -1;
			}
		}
		classifier_spine = ctx->terms->terms[classifier_spine].as.app.function;
	}
	if (classifier_spine < ctx->terms->term_count &&
		ctx->terms->terms[classifier_spine].tag == PROTOTYPE_TERM_VAR) {
		uint32_t head_binding_context;
		if (prototype_context_find_binding(
				&ctx->metadata->contexts,
				substitution->source_context,
				ctx->terms->terms[classifier_spine].as.var.binding_id,
				&head_binding_context
			) != 0) {
			return -1;
		}
		const struct prototype_context* head_binding = prototype_context_get(
			&ctx->metadata->contexts, head_binding_context
		);
		uint32_t head_classifier = operation_context_classifier_term(
			ctx, head_binding_context
		);
		struct prototype_judgement_selected_evidence head_evidence;
		if (!head_binding || head_classifier == PROTOTYPE_INVALID_ID ||
			operation_solver_select_evidence_in_context(
				ctx,
				PROTOTYPE_INVALID_ID,
				substitution->source_context,
				classifier_spine,
				head_classifier,
				&head_evidence
			) != 0) {
			return -1;
		}
		if (getenv("A_PROGRAM_FORMATION_TRACE")) {
			fprintf(stderr, "classifier head evidence: ");
			prototype_type_term_print_debug(
				stderr,
				ctx->symbols,
				ctx->intrinsic_environment,
				ctx->type_declarations,
				ctx->terms,
				head_evidence.classifier
			);
			fprintf(stderr, "\n");
		}
	}
	if (materialize_classifier_application_formation(
			ctx,
			substitution->source_context,
			substitution_classifier
		) != 0) {
		if (getenv("A_PROGRAM_FORMATION_TRACE")) {
			fprintf(stderr, "classifier formation expression: ");
			prototype_type_term_print_debug(
				stderr,
				ctx->symbols,
				ctx->intrinsic_environment,
				ctx->type_declarations,
				ctx->terms,
				substitution_classifier
			);
			fprintf(stderr, "\n");
		}
		return -1;
	}
	return prototype_judgement_delta_record_context_binding_assumption(
		&ctx->judgement_delta,
		ctx->terms,
		binding_id,
		substitution_classifier
	);
}

static int operation_solver_materialize_refinement_binding_evidence_at_depth(
	struct compile_context* ctx,
	uint32_t substitution_id,
	uint32_t depth
) {
	return operation_solver_materialize_refinement_binding_evidence_with_mode(
		ctx, substitution_id, depth, 0
	);
}

static int operation_solver_materialize_publication_substitutions(
	struct compile_context* ctx,
	uint32_t first_occurrence,
	int require_accepted
) {
	if (!ctx || !ctx->typing || !ctx->metadata || first_occurrence >
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t occurrence_id = first_occurrence;
		occurrence_id < ctx->metadata->typed_occurrences.occurrence_count;
		++occurrence_id) {
		uint32_t typed_projection =
			ctx->typing->publication_typed_projection_for_occurrence[occurrence_id];
		uint32_t concrete_context;
		uint32_t substitution;
		int status = operation_typed_projection_materialized_context(
			ctx, typed_projection, &concrete_context, &substitution
		);
		if (status == PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
			continue;
		}
		if (status != 0 || concrete_context == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		if (substitution != PROTOTYPE_INVALID_ID &&
			operation_solver_materialize_refinement_binding_evidence_with_mode(
				ctx, substitution, 0, require_accepted
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_materialize_match_pattern_assumptions(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	uint32_t mutable_occurrence_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t operation_id = mutable_occurrence_start;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			operation->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH) {
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
		if (match->as.match.case_count != operation->case_count ||
			operation->first_case + operation->case_count >
				ctx->metadata->typed_occurrences.case_count) {
			return -1;
		}
		for (uint32_t case_index = 0; case_index < operation->case_count; ++case_index) {
			uint32_t branch_operation;
			uint32_t operation_case_id = operation->first_case + case_index;
			const struct prototype_typed_occurrence_match_case* operation_case =
				&ctx->metadata->typed_occurrences.cases[operation_case_id];
			uint32_t pattern_context = operation_case_materialized_context(
				ctx, operation_case_id
			);
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				const struct prototype_substitution* pattern_action =
					prototype_typed_occurrence_match_case_is_solved(operation_case) ?
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						operation_case->refinement_substitution
					) : NULL;
				fprintf(stderr,
					"A_PROGRAM_PROOF_TRACE pattern-context match=%u case=%u global=%u "
					"candidate=%u materialized=%u action=%u source=%u target=%u\n",
					operation_id, case_index, operation_case_id,
					operation_case->context_id, pattern_context,
					operation_case->refinement_substitution,
					pattern_action ? pattern_action->source_context : PROTOTYPE_INVALID_ID,
					pattern_action ? pattern_action->target_context : PROTOTYPE_INVALID_ID);
			}
			uint32_t term_case_id = match->as.match.first_case + case_index;
			if (term_case_id >= ctx->terms->case_count ||
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
					case_index, &branch_operation
				) != 0 ||
				operation_case->constructor_owner == PROTOTYPE_INVALID_ID ||
				operation_case->constructor_id == PROTOTYPE_INVALID_ID ||
				pattern_context == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, pattern_context
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, PROTOTYPE_INVALID_ID
			);
			const struct prototype_match_case* match_case =
				&ctx->terms->cases[term_case_id];
			if (match_case->binder_count >
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY ||
				match_case->first_binder + match_case->binder_count >
					ctx->terms->case_binder_count) {
				return -1;
			}
			struct prototype_case_binder occurrence_binders[
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY
			];
			for (uint32_t binder_index = 0;
				binder_index < match_case->binder_count; ++binder_index) {
				if (!prototype_context_contains_binding(
						&ctx->metadata->contexts,
						pattern_context,
						operation_case->binder_ids[binder_index]
					)) {
					fprintf(stderr,
						"match assumption missing binding operation=%u case=%u binder=%u "
						"binding-id=%u context=%u\n",
						operation_id, case_index, binder_index,
						operation_case->binder_ids[binder_index],
						pattern_context);
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
						pattern_context,
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
						pattern_context,
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
				uint32_t classifier = operation_context_classifier_term(
					ctx, binding_context_id
				);
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
						relation->context_id != pattern_context ||
						relation->subject != binder_var ||
						relation->classifier != classifier) {
						continue;
					}
					uint32_t cursor = 0;
					uint32_t derivation_id;
					while (prototype_judgement_candidate_derivation_for_conclusion_next(
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
					fprintf(stderr,
						"match assumption recording failed operation=%u case=%u binder=%u "
						"term=%u classifier=%u context=%u\n",
						operation_id, case_index, binder_index, binder_var,
						classifier, pattern_context);
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
					fprintf(stderr,
						"match refinement evidence failed operation=%u ast=%u core=%u context=%u case=%u case-context=%u substitution=%u\n",
						operation_id, operation->source_ast, operation->core_term,
						operation->context_id, case_index,
						operation_case->context_id,
						operation_case->refinement_substitution);
				return -1;
			}
		}
	}
	return 0;
}

static int operation_solver_var_projection_evidence_authority(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t subject,
	int* p_authority_kind,
	uint32_t* p_authority_id,
	uint32_t* p_context,
	uint32_t* p_substitution
) {
	const struct prototype_typed_projection* projection = ctx && ctx->typing ?
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		) : NULL;
	if (!ctx || !ctx->metadata || !projection || !p_authority_kind ||
		!p_authority_id || !p_context || !p_substitution ||
		projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrences[
			projection->source_occurrence
		].tag != PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		return -1;
	}
	uint32_t materialized_subject;
	uint32_t classifier;
	if (operation_typed_projection_publication_typing_answer(
			ctx, typed_projection, p_context, p_substitution,
			&materialized_subject, &classifier
		) != 0 || materialized_subject != subject) {
		return -1;
	}
	(void)classifier;
	const struct prototype_substitution* substitution = prototype_substitution_get(
		&ctx->metadata->substitutions, *p_substitution
	);
	if (!substitution || substitution->source_context != *p_context) {
		return -1;
	}
	if (subject < ctx->terms->term_count &&
		ctx->terms->terms[subject].tag == PROTOTYPE_TERM_VAR) {
		uint32_t binding_context;
		if (prototype_context_find_binding(
				&ctx->metadata->contexts,
				*p_context,
				ctx->terms->terms[subject].as.var.binding_id,
				&binding_context
			) == 0) {
			uint32_t binding_classifier = operation_context_classifier_term(
				ctx, binding_context
			);
			if (binding_classifier != PROTOTYPE_INVALID_ID &&
				prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					binding_classifier,
					classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				*p_authority_kind =
					PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING;
				*p_authority_id = subject;
				return 0;
			}
		}
	}
	uint32_t publication_projection = operation_solver_publication_projection(
		ctx, projection->source_occurrence
	);
	*p_authority_kind = substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY ?
		PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING :
		typed_projection == publication_projection ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE :
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	*p_authority_id = *p_authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
		projection->source_occurrence : subject;
	return 0;
}

static int operation_solver_var_evidence_authority(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t subject,
	int* p_authority_kind,
	uint32_t* p_authority_id,
	uint32_t* p_context,
	uint32_t* p_substitution
) {
	uint32_t typed_projection = operation_solver_publication_projection(
		ctx, operation_id
	);
	if (typed_projection == PROTOTYPE_INVALID_ID) return -1;
	return operation_solver_var_projection_evidence_authority(
		ctx, typed_projection, subject, p_authority_kind, p_authority_id,
		p_context, p_substitution
	);
}

static uint32_t operation_solver_evidence_owner(
	struct compile_context* ctx,
	uint32_t operation_id
) {
	for (uint32_t depth = 0;
		ctx && ctx->metadata && operation_id < ctx->metadata->typed_occurrences.occurrence_count &&
		depth < 64;
		++depth) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		uint32_t type_id;
		uint32_t type_arguments[16];
		uint32_t type_argument_count;
		int type_instance = operation->core_term < ctx->terms->term_count &&
			prototype_type_projection_instance_info(
				ctx->terms,
			&ctx->type_declarations->semantic_schema,
				operation->core_term,
				&type_id,
				type_arguments,
				&type_argument_count
			) == 0;
		if (type_instance && operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP) {
			const struct prototype_type_declaration* declaration =
				type_id < ctx->type_declarations->semantic_schema.type_count ?
				&ctx->type_declarations->semantic_schema.type_declarations[type_id] :
				NULL;
			/* A partial family application is derived by an APP elimination and
			 * therefore keeps occurrence authority. Only a fully formed type
			 * instance is an authority-neutral type-formation fact. */
			type_instance = declaration && type_argument_count ==
				declaration->parameter_count + declaration->index_count;
		}
		const struct prototype_term* core = operation->core_term <
			ctx->terms->term_count ? &ctx->terms->terms[operation->core_term] : NULL;
		int authority_neutral_atom = operation->tag ==
			PROTOTYPE_TYPED_OCCURRENCE_ATOM && core &&
			(core->tag == PROTOTYPE_TERM_EXTERNAL_REF ||
			 core->tag == PROTOTYPE_TERM_PURE_PRIMITIVE ||
			 core->tag == PROTOTYPE_TERM_EFFECT_OPERATION ||
			 core->tag == PROTOTYPE_TERM_TEXT_LITERAL ||
			 core->tag == PROTOTYPE_TERM_INT_LITERAL);
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
			int authority_kind;
			uint32_t authority_id;
			uint32_t context_id;
			uint32_t substitution_id;
			if (operation_solver_var_evidence_authority(
					ctx, operation_id, operation->core_term,
					&authority_kind, &authority_id, &context_id, &substitution_id
				) != 0) {
				return PROTOTYPE_INVALID_ID;
			}
			(void)authority_id;
			(void)context_id;
			(void)substitution_id;
			return authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING ?
				PROTOTYPE_INVALID_ID : operation_id;
		}
		if (((authority_neutral_atom || type_instance) &&
			 operation->context_action_substitution == PROTOTYPE_INVALID_ID) ||
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR &&
			 (operation->context_action_substitution == PROTOTYPE_INVALID_ID ||
			  operation->source_classifier == PROTOTYPE_INVALID_ID))) {
			return PROTOTYPE_INVALID_ID;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
			operation->wrapped_occurrence <
				ctx->metadata->typed_occurrences.occurrence_count &&
			operation->classifier == ctx->metadata->typed_occurrences.occurrences[
				operation->wrapped_occurrence
			].classifier) {
			uint32_t materialized_context;
			uint32_t materialized_substitution;
			const struct prototype_substitution* substitution = NULL;
			if (operation_occurrence_materialized_context_status(
					ctx, operation_id, &materialized_context, &materialized_substitution
				) == 0) {
				substitution = prototype_substitution_get(
					&ctx->metadata->substitutions, materialized_substitution
				);
			}
			(void)materialized_context;
			if (substitution && substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY) {
				operation_id = operation->wrapped_occurrence;
				continue;
			}
		}
		return operation_id;
	}
	return operation_id;
}

static int operation_solver_evidence_has_accepted_claim(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_selected_evidence* evidence
) {
	if (!judgement || !evidence) {
		return 0;
	}
	struct prototype_judgement_proposition proposition = {
		.kind = evidence->kind,
		.authority_kind = evidence->authority_kind,
		.authority_id = evidence->authority_id,
		.context_id = evidence->context_id,
		.occurrence_id = evidence->occurrence_id,
		.subject = evidence->subject,
		.classifier = evidence->classifier,
		.resource_usage_count = evidence->resource_usage_count,
		.resource_usage = evidence->resource_usage
	};
	uint32_t proposition_id;
	if (prototype_judgement_proposition_find_exact(
			judgement, &proposition, &proposition_id
		) != 0) {
		return 0;
	}
	struct prototype_judgement_claim claim = {
		.proposition_id = proposition_id
	};
	uint32_t claim_id;
	if (prototype_judgement_find_exact_claim(
			judgement, &claim, &claim_id
		) != 0) {
		return 0;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(judgement, i);
		if (derivation && derivation->conclusion_claim_id == claim_id) {
			return 1;
		}
	}
	return 0;
}

static int operation_solver_type_instance_is_fully_applied(
	const struct compile_context* ctx,
	uint32_t term_id
) {
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (!ctx || !ctx->terms || !ctx->type_declarations ||
		prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, term_id, &type_id, arguments, &argument_count
		) != 0) {
		return 0;
	}
	if (type_id >= ctx->type_declarations->semantic_schema.type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	uint32_t arity = type->parameter_count + type->index_count;
	if (argument_count > arity) {
		return -1;
	}
	return argument_count == arity;
}

static int operation_solver_type_instance_is_declaration_head(
	const struct compile_context* ctx,
	uint32_t term_id
) {
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (!ctx || !ctx->terms || !ctx->type_declarations ||
		prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, term_id, &type_id, arguments, &argument_count
		) != 0) {
		return 0;
	}
	if (type_id >= ctx->type_declarations->semantic_schema.type_count) {
		return -1;
	}
	return argument_count == 0;
}

static int operation_solver_named_atom_can_share_evidence(
	const struct compile_context* ctx,
	const struct prototype_typed_occurrence* operation,
	const struct prototype_term* term,
	uint32_t term_id
) {
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (!ctx || !operation || !term || operation->source_symbol_id < 0) {
		return 0;
	}
	if (term->tag == PROTOTYPE_TERM_EXTERNAL_REF ||
		term->tag == PROTOTYPE_TERM_PURE_PRIMITIVE ||
		term->tag == PROTOTYPE_TERM_EFFECT_OPERATION ||
		term->tag == PROTOTYPE_TERM_TEXT_LITERAL ||
		term->tag == PROTOTYPE_TERM_INT_LITERAL) {
		return 0;
	}
	/* Type instances and their partial application spines have declaration
	 * authority. They must not inherit ordinary occurrence evidence. */
	if (prototype_type_projection_instance_info(
			ctx->terms,
			&ctx->type_declarations->semantic_schema, term_id, &type_id, arguments, &argument_count
		) == 0) {
		return 0;
	}
	return 1;
}

static int operation_solver_refresh_delta_resource_usage(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	size_t first = ctx->evidence_workspace.
		resource_usage_synced_proposition_count;
	if (first > ctx->judgement_delta.proposition_count) {
		first = 0;
	}
	/* Solver usage answers are frozen before proof materialization and Delta
	 * propositions are append-only between commits. Project usage once when a
	 * proposition enters the Delta instead of rescanning every prior proof edge
	 * for each evidence lookup. */
	for (size_t i = first; i < ctx->judgement_delta.proposition_count; ++i) {
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
	ctx->evidence_workspace.resource_usage_synced_proposition_count =
		ctx->judgement_delta.proposition_count;
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
	int status = prototype_judgement_delta_select_evidence(
		&ctx->judgement_delta,
		operation_id,
		context_id,
		subject,
		classifier,
		selected
	);
	if (status != 1 || !ctx->judgement || !ctx->metadata ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return status;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		int authority_kind;
		uint32_t authority_id;
		uint32_t materialized_context;
		uint32_t materialized_substitution;
		if (operation_solver_var_evidence_authority(
				ctx, operation_id, subject, &authority_kind, &authority_id,
				&materialized_context, &materialized_substitution
			) != 0) {
			return -1;
		}
		(void)materialized_context;
		(void)materialized_substitution;
		return prototype_judgement_select_evidence_by_authority(
			ctx->judgement, authority_kind, authority_id, context_id,
			subject, classifier, selected
		);
	}
	return prototype_judgement_select_evidence(
		ctx->judgement,
		operation_solver_evidence_owner(ctx, operation_id),
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
		if (selection_status < 0) {
			return -1;
		}
		if (selection_status == 2) {
			return 2;
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
		uint32_t evidence_owner = operation_solver_evidence_owner(ctx, operation_id);
		return operation_solver_select_evidence_in_context(
			ctx,
			evidence_owner,
			context_id,
			subject,
			classifier,
			p_evidence
		);
	}
	int authority_kind;
	uint32_t authority_id;
	uint32_t materialized_context;
	uint32_t materialized_substitution;
	if (operation_solver_var_evidence_authority(
			ctx, operation_id, subject, &authority_kind, &authority_id,
			&materialized_context, &materialized_substitution
		) != 0) {
		return -1;
	}
	(void)materialized_context;
	(void)materialized_substitution;
	return operation_solver_select_authority_evidence_in_context(
		ctx, operation_id, authority_kind, authority_id, context_id, subject,
		classifier, p_evidence
	);
}

static int operation_solver_select_projected_evidence_in_context(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	const struct prototype_typed_projection* projection = ctx && ctx->typing ?
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		) : NULL;
	if (!projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t operation_id = projection->source_occurrence;
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
		uint32_t wrapped_projection;
		if (operation_solver_projected_wrapped(
				ctx, typed_projection, operation->wrapped_occurrence,
				&wrapped_projection
			) != 0) {
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(stderr,
					"A_PROGRAM_PROOF_TRACE projected-reference=%u projection=%u "
					"wrapped=%u stage=edge-failed\n",
					operation_id, typed_projection, operation->wrapped_occurrence);
			}
			return -1;
		}
		uint32_t wrapped_context;
		uint32_t wrapped_substitution;
		uint32_t wrapped_subject;
		uint32_t wrapped_classifier;
		int wrapped_status = operation_typed_projection_publication_typing_answer(
				ctx, wrapped_projection, &wrapped_context, &wrapped_substitution,
				&wrapped_subject, &wrapped_classifier
			);
		if (wrapped_status != 0) {
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(stderr,
					"A_PROGRAM_PROOF_TRACE projected-reference=%u projection=%u "
					"wrapped=%u wrapped-projection=%u stage=typing-failed status=%d\n",
					operation_id, typed_projection, operation->wrapped_occurrence,
					wrapped_projection, wrapped_status);
			}
			return -1;
		}
		(void)wrapped_substitution;
		if (getenv("A_PROGRAM_PROOF_TRACE")) {
			fprintf(stderr,
				"A_PROGRAM_PROOF_TRACE projected-reference=%u projection=%u "
				"wrapped=%u wrapped-projection=%u context=%u/%u subject=%u/%u "
				"classifier=%u/%u\n",
				operation_id, typed_projection, operation->wrapped_occurrence,
				wrapped_projection, wrapped_context, context_id, wrapped_subject,
				subject, wrapped_classifier, classifier);
		}
		if (wrapped_context == context_id) {
			if (wrapped_subject == subject) {
				struct prototype_term_conversion_result conversion =
					prototype_judgement_classifier_conversion(
						ctx->terms, ctx->type_declarations, wrapped_classifier,
						classifier
					);
				if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
					return operation_solver_select_projected_evidence_in_context(
						ctx, wrapped_projection, context_id, subject,
						wrapped_classifier, p_evidence
					);
				}
			}
		}
	}
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		uint32_t evidence_owner = operation_solver_evidence_owner(
			ctx, operation_id
		);
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
			evidence_owner == PROTOTYPE_INVALID_ID) {
			/* Authority-neutral atoms include type formation, intrinsics and
			 * Core helpers. Discover the accepted authority once, then use the
			 * ordinary authority-preserving weakening path. */
			uint32_t source_context = context_id;
			for (;;) {
				struct prototype_judgement_selected_evidence accepted;
				int status = prototype_judgement_delta_select_evidence(
					&ctx->judgement_delta,
					PROTOTYPE_INVALID_ID,
					source_context,
					subject,
					classifier,
					&accepted
				);
				if (status < 0 || status == 2) {
					return -1;
				}
				if (status == 0) {
					return operation_solver_select_authority_evidence_in_context(
						ctx, operation_id, accepted.authority_kind,
						accepted.authority_id, context_id, subject, classifier,
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
		return operation_solver_select_evidence_in_context(
			ctx, evidence_owner, context_id, subject, classifier, p_evidence
		);
	}
	int authority_kind;
	uint32_t authority_id;
	uint32_t materialized_context;
	uint32_t materialized_substitution;
	if (operation_solver_var_projection_evidence_authority(
			ctx, typed_projection, subject, &authority_kind, &authority_id,
			&materialized_context, &materialized_substitution
		) != 0 || materialized_context != context_id) {
		return -1;
	}
	(void)materialized_substitution;
	return operation_solver_select_authority_evidence_in_context(
		ctx, operation_id, authority_kind, authority_id, context_id, subject,
		classifier, p_evidence
	);
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
);

static int operation_solver_project_expected_evidence(
	struct compile_context* ctx,
	uint32_t owner_projection,
	uint32_t operation_id,
	uint32_t wrapped_operation_id,
	uint32_t target_subject,
	uint32_t depth,
	struct prototype_judgement_selected_evidence* p_evidence,
	int* p_projection_is_identity
) {
	if (!ctx || !ctx->metadata || !p_evidence || !p_projection_is_identity ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		wrapped_operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* wrapped =
		&ctx->metadata->typed_occurrences.occurrences[wrapped_operation_id];
	if (wrapped->classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t target_context = PROTOTYPE_INVALID_ID;
	uint32_t target_substitution = PROTOTYPE_INVALID_ID;
	uint32_t materialized_target_subject = PROTOTYPE_INVALID_ID;
	uint32_t target_classifier = PROTOTYPE_INVALID_ID;
	uint32_t wrapped_projection = PROTOTYPE_INVALID_ID;
	int target_status = operation_typed_projection_publication_typing_answer(
		ctx, owner_projection, &target_context, &target_substitution,
		&materialized_target_subject, &target_classifier
	);
	if (target_status != 0 || materialized_target_subject != target_subject) {
		return -1;
	}
	(void)target_substitution;
	if (operation_solver_projected_wrapped(
			ctx, owner_projection, wrapped_operation_id, &wrapped_projection
		) != 0) {
		return -1;
	}
	if (target_classifier != PROTOTYPE_INVALID_ID) {
		int existing_status = operation_solver_select_projected_evidence_in_context(
			ctx, owner_projection, target_context, target_subject,
			target_classifier, p_evidence
		);
		if (existing_status <= 0) {
			return existing_status;
		}
	}
	uint32_t source_context;
	uint32_t source_substitution;
	uint32_t source_subject;
	uint32_t source_classifier;
	if (operation_typed_projection_publication_typing_answer(
			ctx, wrapped_projection, &source_context, &source_substitution,
			&source_subject, &source_classifier
		) != 0 || source_context != target_context) {
		return -1;
	}
	(void)source_substitution;
	int status = operation_solver_reify_projected_core_proof(
		ctx, wrapped_projection, source_subject, depth + 1
	);
	if (status != 0) {
		if (getenv("A_PROGRAM_PROOF_TRACE")) {
			fprintf(stderr,
				"A_PROGRAM_PROOF_TRACE projected-wrapper=%u owner=%u child=%u "
				"wrapped=%u reify-status=%d target-context=%u source-context=%u\n",
				operation_id, owner_projection, wrapped_projection,
				wrapped_operation_id, status, target_context, source_context);
		}
		return status;
	}
	status = operation_solver_select_projected_evidence_in_context(
		ctx, wrapped_projection, source_context, source_subject,
		source_classifier, p_evidence
	);
	if (status != 0) {
		if (getenv("A_PROGRAM_PROOF_TRACE")) {
			fprintf(stderr,
				"A_PROGRAM_PROOF_TRACE projected-wrapper=%u owner=%u child=%u "
				"wrapped=%u evidence-status=%d context=%u subject=%u classifier=%u\n",
				operation_id, owner_projection, wrapped_projection,
				wrapped_operation_id, status, source_context, source_subject,
				source_classifier);
			for (size_t i = 0; i < ctx->judgement_delta.proposition_count; ++i) {
				const struct prototype_judgement_proposition* candidate =
					&ctx->judgement_delta.propositions[i];
				if (candidate->subject == source_subject) {
					fprintf(stderr,
						"A_PROGRAM_PROOF_TRACE projected-wrapper-candidate=%zu "
						"occurrence=%u authority=%d:%u context=%u classifier=%u\n",
						i, candidate->occurrence_id, candidate->authority_kind,
						candidate->authority_id, candidate->context_id,
						candidate->classifier);
				}
			}
		}
		return status;
	}
	*p_projection_is_identity = source_subject == target_subject;
	return *p_projection_is_identity ? 0 : -1;
}

static int operation_solver_reify_core_proof_impl(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t core_term,
	uint32_t depth
) {
	const struct prototype_typed_projection* projection = ctx && ctx->typing ?
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		) : NULL;
	if (!ctx || !ctx->metadata || !projection || projection->source_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		core_term >= ctx->terms->term_count || depth > 256) {
		return -1;
	}
	uint32_t operation_id = projection->source_occurrence;
	const struct prototype_typed_occurrence* stored_operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	struct prototype_typed_occurrence proof_operation = *stored_operation;
	int projection_status = operation_typed_projection_publication_typing_answer(
			ctx,
			typed_projection,
			&proof_operation.context_id,
			&proof_operation.context_action_substitution,
			&core_term,
			&proof_operation.classifier
		);
	if (projection_status != 0) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation = &proof_operation;
	/* Layer C remains the erased occurrence graph. Layer T derives its local
	 * proposition from the occurrence's canonical Context projection. */
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
			int authority_kind;
			uint32_t authority_id;
			uint32_t materialized_context;
			uint32_t materialized_substitution;
			if (operation_solver_var_projection_evidence_authority(
					ctx, typed_projection, core_term, &authority_kind, &authority_id,
					&materialized_context, &materialized_substitution
				) != 0 || materialized_context != operation->context_id) {
				return -1;
			}
			(void)materialized_substitution;
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
			fprintf(stderr,
				"reification: accepted evidence selection failed operation=%u "
				"status=%d context=%u core=%u classifier=%u authority=%u\n",
				operation_id, accepted_status, operation->context_id, core_term,
				classifier, accepted_authority);
			return -1;
		}
		if (accepted_status == 0 &&
			!operation_solver_evidence_has_accepted_claim(
				ctx->judgement, &accepted_evidence
			)) {
			accepted_status = 1;
		}
		if (accepted_status == 0 &&
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			int motive_claim = operation_solver_accepted_match_motive_claim_exists(
				ctx->judgement, operation_id
			);
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(stderr,
					"A_PROGRAM_PROOF_TRACE accepted-match operation=%u motive-claim=%d\n",
					operation_id, motive_claim);
			}
			if (motive_claim < 0) {
				return -1;
			}
			if (!motive_claim) {
				accepted_status = 1;
			}
		}
		if (accepted_status == 0) {
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(stderr,
					"A_PROGRAM_PROOF_TRACE accepted-reuse operation=%u projection=%u "
					"context=%u subject=%u classifier=%u authority=%d:%u\n",
					operation_id, typed_projection, operation->context_id, core_term,
					classifier, accepted_evidence.authority_kind,
					accepted_evidence.authority_id);
			}
			ctx->metadata->proof_reify_accepted_reuse_count++;
			return 0;
		}
	}
	switch (operation->tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA: {
			uint32_t body_occurrence;
			uint32_t body_projection;
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
			if (operation_solver_projected_child(
					ctx, typed_projection, PROTOTYPE_TERM_CHILD_BODY, 0,
					&body_projection
				) != 0) {
				return -1;
			}
			const struct prototype_typed_occurrence* stored_body =
				&ctx->metadata->typed_occurrences.occurrences[body_occurrence];
			struct prototype_typed_occurrence proof_body = *stored_body;
			uint32_t body_subject;
			if (operation_typed_projection_publication_typing_answer(
					ctx, body_projection,
					&proof_body.context_id,
					&proof_body.context_action_substitution,
					&body_subject,
					&proof_body.classifier
				) != 0) {
				return -1;
			}
			const struct prototype_typed_occurrence* body = &proof_body;
			if (body->classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int status = operation_solver_reify_projected_core_proof(
				ctx, body_projection, body_subject, depth + 1
			);
			if (status != 0) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(stderr,
						"A_PROGRAM_PROOF_TRACE lambda=%u waits=body:%u status=%d\n",
						operation_id, body_occurrence, status);
				}
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
				operation_context_classifier_term(ctx, body->context_id);
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
				if (materialize_classifier_application_formation(
						ctx, body->context_id, context_binder_classifier
					) != 0) {
					return -1;
				}
				int has_binder_assumption = 0;
			for (size_t i = 0;
				i < ctx->judgement_delta.proposition_count;
				++i) {
				const struct prototype_judgement_proposition* relation =
					&ctx->judgement_delta.propositions[i];
					uint32_t derivation_id;
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
							&derivation_id
						) == 0) {
						const struct prototype_judgement_derivation_candidate* derivation =
							&ctx->judgement_delta.derivation_candidates[derivation_id];
						has_binder_assumption =
							ctx->terms->terms[context_binder_classifier].tag !=
								PROTOTYPE_TERM_APP || derivation->premise_count == 1;
						if (!has_binder_assumption) {
							continue;
						}
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
				) == 0 && prototype_type_projection_classifier_graph(
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
						PROTOTYPE_INVALID_ID,
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
					if (getenv("A_PROGRAM_CONTEXT_RESOLUTION_TRACE")) {
						const struct prototype_substitution* trace_action =
							prototype_substitution_get(
								&ctx->metadata->substitutions,
								operation->context_action_substitution
							);
						fprintf(
							stderr,
							"lambda binder occurrence source-classifier=%u "
							"context-action=%u lambda-context=%u body-context=%u "
							"solved-binder=%u body-classifier=%u\n",
							operation->source_classifier,
							operation->context_action_substitution,
							operation->context_id,
							body->context_id,
							operation_solver_binder_classifier(ctx, operation_id),
							body->classifier
						);
						fprintf(
							stderr,
							"lambda binder action source=%u target=%u\n",
							trace_action ? trace_action->source_context :
								PROTOTYPE_INVALID_ID,
							trace_action ? trace_action->target_context :
								PROTOTYPE_INVALID_ID
						);
						fprintf(stderr, "lambda binder source classifier: ");
						prototype_type_term_print_debug(
							stderr, ctx->symbols, ctx->intrinsic_environment,
							ctx->type_declarations, ctx->terms,
							context_binder_classifier
						);
						fputc('\n', stderr);
						fprintf(stderr, "lambda binder target classifier: ");
						prototype_type_term_print_debug(
							stderr, ctx->symbols, ctx->intrinsic_environment,
							ctx->type_declarations, ctx->terms,
							classifier_domain
						);
						fputc('\n', stderr);
					}
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
			int body_status = operation_solver_select_projected_evidence_in_context(
				ctx,
				body_projection,
				body->context_id,
				body_subject,
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
					body_subject,
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
						operation_id,
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
			uint32_t function_projection;
			uint32_t argument_projection;
			uint32_t app_core;
			uint32_t type_id;
			uint32_t type_arguments[16];
			uint32_t type_argument_count;
			if (prototype_type_projection_instance_info(
					ctx->terms,
			&ctx->type_declarations->semantic_schema,
					core_term,
					&type_id,
					type_arguments,
					&type_argument_count
				) == 0) {
				if (type_id >= ctx->type_declarations->semantic_schema.type_count) {
					return -1;
				}
				const struct prototype_type_declaration* type =
					&ctx->type_declarations->semantic_schema.type_declarations[type_id];
				uint32_t family_arity = type->parameter_count + type->index_count;
				if (type_argument_count > family_arity) {
					return -1;
				}
				if (type_argument_count == family_arity) {
					return prototype_judgement_delta_record_type_formation(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						core_term,
						classifier
					);
				}
				/* A partial family application is a value with the residual Pi
				 * classifier. Reify its APP spine through STATIC_FAMILY_APP_ELIM. */
			}
			app_core = core_term;
			if (ctx->terms->terms[app_core].tag != PROTOTYPE_TERM_APP &&
				(prototype_term_core_projection(
						ctx->terms, core_term, &app_core
					) != 0 || ctx->terms->terms[app_core].tag !=
						PROTOTYPE_TERM_APP)) {
				return -1;
			}
			if (compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
					&function_operation
				) != 0) {
				return -1;
			}
			if (compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
					&argument_occurrence
				) != 0) {
				return -1;
			}
			if (operation_solver_projected_child(
					ctx, typed_projection, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
					&function_projection
				) != 0) {
				return -1;
			}
			if (operation_solver_projected_child(
					ctx, typed_projection, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
					&argument_projection
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
				uint32_t reverse_argument_projections[64];
				uint32_t argument_occurrence_count = 0;
				uint32_t expected_domains[64];
				uint32_t expected_domain_count;
				uint32_t cursor = operation_id;
				uint32_t cursor_projection = typed_projection;
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
					fprintf(stderr, "constructor reification: invalid spine operation=%u\n",
						operation_id);
					return -1;
				}
					if (operation_constructor_spine_expected_domains(
						ctx,
						operation->context_id,
						core_term,
						PROTOTYPE_INVALID_ID,
						expected_domains,
						64,
						&expected_domain_count
					) != 0) {
					fprintf(stderr,
						"constructor reification: expected domains failed operation=%u "
						"context=%u\n",
						operation_id, operation->context_id);
					return -1;
				}
				while (cursor < ctx->metadata->typed_occurrences.occurrence_count) {
					const struct prototype_typed_occurrence* cursor_operation =
						&ctx->metadata->typed_occurrences.occurrences[cursor];
					uint32_t cursor_function;
					uint32_t cursor_argument;
					uint32_t cursor_function_projection;
					uint32_t cursor_argument_projection;
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
						) != 0 || operation_solver_projected_child(
							ctx, cursor_projection, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
							&cursor_function_projection
						) != 0 || operation_solver_projected_child(
							ctx, cursor_projection, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
							&cursor_argument_projection
						) != 0) {
						return -1;
					}
					reverse_argument_occurrences[argument_occurrence_count++] =
						cursor_argument;
					reverse_argument_projections[argument_occurrence_count - 1] =
						cursor_argument_projection;
					cursor = cursor_function;
					cursor_projection = cursor_function_projection;
				}
				if (argument_occurrence_count != argument_count) {
					return -1;
				}
				if (expected_domain_count != argument_count) {
					return -1;
				}
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(stderr,
						"A_PROGRAM_PROOF_TRACE constructor=%u arguments=%u context=%u "
						"classifier=%u\n",
						operation_id, argument_count, operation->context_id, classifier);
					for (uint32_t i = 0; i < argument_count; ++i) {
						uint32_t argument_occurrence = reverse_argument_occurrences[
							argument_count - i - 1
						];
						fprintf(stderr,
							"A_PROGRAM_PROOF_TRACE constructor=%u argument-slot=%u "
							"occurrence=%u classifier=%u expected=%u\n",
							operation_id, i, argument_occurrence,
							ctx->metadata->typed_occurrences.occurrences[
								argument_occurrence
							].classifier,
							expected_domains[i]);
					}
				}
					uint32_t argument_occurrences[64];
					uint32_t argument_classifiers[64];
					struct prototype_judgement_selected_evidence argument_evidence[64];
				for (uint32_t i = 0; i < argument_count; ++i) {
					uint32_t argument_occurrence = reverse_argument_occurrences[
						argument_count - i - 1
					];
					uint32_t argument_projection = reverse_argument_projections[
						argument_count - i - 1
					];
					argument_occurrences[i] = argument_occurrence;
					uint32_t argument_context;
					uint32_t argument_substitution;
					uint32_t argument_subject;
					if (operation_typed_projection_publication_typing_answer(
							ctx,
							argument_projection,
							&argument_context,
							&argument_substitution,
							&argument_subject,
							&argument_classifiers[i]
						) != 0) {
						return -1;
					}
					(void)argument_substitution;
					if (argument_context != operation->context_id ||
						argument_subject != arguments[i]) {
						return -1;
					}
					int status = operation_solver_reify_projected_core_proof(
						ctx, argument_projection, argument_subject, depth + 1
					);
					if (getenv("A_PROGRAM_PROOF_TRACE")) {
						fprintf(stderr,
							"A_PROGRAM_PROOF_TRACE constructor=%u argument=%u position=%u "
							"subject=%u classifier=%u expected=%u reify=%d\n",
							operation_id, argument_occurrence, i, arguments[i],
							argument_classifiers[i], expected_domains[i], status);
					}
					if (status != 0) {
						if (status < 0) {
							fprintf(stderr,
								"constructor reification: argument proof failed "
								"operation=%u argument=%u position=%u\n",
								operation_id, argument_occurrence, i);
						}
						return status;
					}
					status = operation_solver_select_projected_evidence_in_context(
						ctx,
						argument_projection,
						argument_context,
						argument_subject,
						argument_classifiers[i],
						&argument_evidence[i]
					);
					if (getenv("A_PROGRAM_PROOF_TRACE")) {
						fprintf(stderr,
							"A_PROGRAM_PROOF_TRACE constructor=%u argument=%u position=%u "
							"evidence=%d context=%u\n",
							operation_id, argument_occurrence, i, status,
							operation->context_id);
					}
					if (status != 0) {
						if (status < 0) {
							fprintf(stderr,
								"constructor reification: argument evidence failed "
								"operation=%u argument=%u position=%u classifier=%u\n",
								operation_id, argument_occurrence, i,
								argument_classifiers[i]);
						}
						return status;
					}
					int expected_matches = argument_classifiers[i] == expected_domains[i];
					if (!expected_matches) {
						uint32_t projected_argument_classifier;
						uint32_t projected_expected_domain;
						int argument_projection =
							prototype_judgement_project_pure_sequence_results(
								ctx->terms, ctx->type_declarations,
								&ctx->metadata->contexts,
								&ctx->metadata->typed_occurrences,
								argument_context, argument_classifiers[i],
								&projected_argument_classifier
							);
						int expected_projection =
							prototype_judgement_project_pure_sequence_results(
								ctx->terms, ctx->type_declarations,
								&ctx->metadata->contexts,
								&ctx->metadata->typed_occurrences,
								argument_context, expected_domains[i],
								&projected_expected_domain
							);
						if (argument_projection < 0 || expected_projection < 0) {
							return -1;
						}
						expected_matches = argument_projection == 0 &&
							expected_projection == 0 &&
							prototype_judgement_classifier_conversion(
								ctx->terms, ctx->type_declarations,
								projected_argument_classifier,
								projected_expected_domain
							).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
					}
					if (!expected_matches) {
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
								argument_evidence[i].occurrence_id ==
									PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID :
									argument_occurrence,
								&argument_evidence[i],
								expected_domains[i],
								argument_subject
							);
						int expected_evidence_status = -1;
						if (exposure_status == 0 &&
							operation_solver_refresh_delta_resource_usage(ctx) == 0) {
							expected_evidence_status =
								argument_evidence[i].occurrence_id ==
									PROTOTYPE_INVALID_ID ?
								prototype_judgement_delta_select_evidence_by_authority(
									&ctx->judgement_delta,
									argument_evidence[i].authority_kind,
									argument_evidence[i].authority_id,
									argument_context,
									argument_subject,
									expected_domains[i],
									&argument_evidence[i]
								) : operation_solver_delta_select_evidence(
									ctx,
									argument_occurrence,
									argument_context,
									argument_subject,
									expected_domains[i],
									&argument_evidence[i]
								);
						}
						if (exposure_status != 0 || expected_evidence_status != 0) {
							fprintf(stderr,
								"constructor reification: expected exposure failed "
								"operation=%u argument=%u position=%u source=%u target=%u "
								"exposure=%d evidence=%d\n",
								operation_id, argument_occurrence, i,
								argument_classifiers[i], expected_domains[i],
								exposure_status, expected_evidence_status);
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
				int constructor_status =
					prototype_judgement_delta_record_constructor_spine(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier,
					argument_occurrences,
					argument_evidence,
					argument_count
				);
				if (constructor_status != 0) {
					fprintf(stderr,
						"constructor reification: spine proof failed operation=%u "
						"classifier=%u status=%d\n",
						operation_id, classifier, constructor_status);
				}
				return constructor_status;
			}
			struct prototype_typed_occurrence proof_function =
				ctx->metadata->typed_occurrences.occurrences[function_operation];
			struct prototype_typed_occurrence proof_argument =
				ctx->metadata->typed_occurrences.occurrences[argument_occurrence];
			uint32_t function_subject;
			uint32_t argument_subject;
			int function_status =
				operation_typed_projection_publication_typing_answer(
					ctx, function_projection, &proof_function.context_id,
					&proof_function.context_action_substitution, &function_subject,
					&proof_function.classifier
				);
			if (function_status != 0) {
				return function_status;
			}
			int argument_status =
				operation_typed_projection_publication_typing_answer(
					ctx, argument_projection, &proof_argument.context_id,
					&proof_argument.context_action_substitution, &argument_subject,
					&proof_argument.classifier
				);
			if (argument_status != 0) {
				return argument_status;
			}
			int function_matches = 0;
			int argument_matches = 0;
			if (prototype_term_core_shape_equal(
					ctx->terms, function_subject, app_term->as.app.function,
					&function_matches
				) != 0) {
				return -1;
			}
			if (prototype_term_core_shape_equal(
					ctx->terms, argument_subject, app_term->as.app.argument,
					&argument_matches
				) != 0) {
				return -1;
			}
			if (!function_matches || !argument_matches) {
				return -1;
			}
			if (proof_function.context_id != operation->context_id) {
				return -1;
			}
			if (proof_argument.context_id != operation->context_id) {
				return -1;
			}
			const struct prototype_typed_occurrence* function = &proof_function;
			const struct prototype_typed_occurrence* argument = &proof_argument;
			if (function->classifier == PROTOTYPE_INVALID_ID ||
				argument->classifier == PROTOTYPE_INVALID_ID) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(
						stderr,
						"A_PROGRAM_PROOF_TRACE app=%u missing=classifier "
						"function=%u:%u:%u argument=%u:%u:%u\n",
						operation_id, function_operation, function_projection,
						function->classifier, argument_occurrence,
						argument_projection, argument->classifier
					);
				}
				return 1;
			}
			int status = operation_solver_reify_projected_core_proof(
					ctx, function_projection, app_term->as.app.function, depth + 1
			);
			if (status != 0) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(stderr,
						"A_PROGRAM_PROOF_TRACE app=%u waits=function:%u status=%d\n",
						operation_id, function_operation, status);
				}
				if (status < 0) {
					fprintf(stderr,
						"app reification: function failed operation=%u function=%u "
						"core=%u classifier=%u\n",
						operation_id, function_operation, function->core_term,
						function->classifier);
				}
				return status;
			}
			status = operation_solver_reify_projected_core_proof(
					ctx, argument_projection, app_term->as.app.argument, depth + 1
			);
			if (status != 0) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(stderr,
						"A_PROGRAM_PROOF_TRACE app=%u waits=argument:%u status=%d\n",
						operation_id, argument_occurrence, status);
				}
				if (status < 0) {
					fprintf(stderr,
						"app reification: argument failed operation=%u argument=%u "
						"core=%u classifier=%u\n",
						operation_id, argument_occurrence, argument->core_term,
						argument->classifier);
				}
				return status;
			}
			struct prototype_judgement_selected_evidence function_evidence;
				status = operation_solver_select_projected_evidence_in_context(
					ctx,
					function_projection,
					operation->context_id,
					function_subject,
				function->classifier,
				&function_evidence
			);
			if (status != 0) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(stderr,
						"A_PROGRAM_PROOF_TRACE app=%u waits=function-evidence:%u status=%d "
						"context=%u requested=%u operation-core=%u classifier=%u owner=%u "
						"action=%u source-classifier=%u requested-tag=%d operation-tag=%d\n",
						operation_id, function_operation, status,
						operation->context_id, app_term->as.app.function,
						function->core_term, function->classifier,
						operation_solver_evidence_owner(ctx, function_operation),
						function->context_action_substitution,
						function->source_classifier,
						ctx->terms->terms[app_term->as.app.function].tag,
						ctx->terms->terms[function->core_term].tag);
					for (size_t candidate_id = 0;
						candidate_id < ctx->judgement_delta.proposition_count;
						++candidate_id) {
						const struct prototype_judgement_proposition* candidate =
							&ctx->judgement_delta.propositions[candidate_id];
						if (candidate->subject == app_term->as.app.function) {
							fprintf(stderr,
								"A_PROGRAM_PROOF_TRACE function-candidate=%zu occurrence=%u "
								"authority=%d:%u context=%u classifier=%u kind=%d\n",
								candidate_id, candidate->occurrence_id,
								candidate->authority_kind, candidate->authority_id,
								candidate->context_id, candidate->classifier,
								candidate->kind);
						}
					}
				}
				if (status < 0) {
					fprintf(stderr,
						"app reification: function evidence failed operation=%u function=%u\n",
						operation_id, function_operation);
				}
				return status;
			}
			struct prototype_judgement_selected_evidence argument_evidence;
			status = operation_solver_select_projected_evidence_in_context(
				ctx,
				argument_projection,
					operation->context_id,
					argument_subject,
				argument->classifier,
				&argument_evidence
			);
			if (status != 0) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(stderr,
						"A_PROGRAM_PROOF_TRACE app=%u waits=argument-evidence:%u status=%d\n",
						operation_id, argument_occurrence, status);
				}
				if (status < 0) {
					fprintf(stderr,
						"app reification: argument evidence failed operation=%u argument=%u\n",
						operation_id, argument_occurrence);
				}
				return status;
			}
			uint32_t direct_classifier;
			int app_proof_kind = operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_TYPE_FAMILY_CALCULATION ?
				PROTOTYPE_JUDGEMENT_PROOF_STATIC_FAMILY_APP_ELIM :
				PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM;
			int direct_status = app_proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_STATIC_FAMILY_APP_ELIM ?
				prototype_judgement_static_family_app_classifier(
					ctx->judgement_delta.contexts,
					ctx->judgement_delta.substitutions,
					ctx->terms,
					ctx->type_declarations,
					operation->context_id,
					function_evidence.classifier,
					argument_evidence.subject,
					argument_evidence.classifier,
					&direct_classifier
				) : prototype_judgement_delta_app_elim_classifier(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					function_evidence.classifier,
					argument_subject,
					argument_evidence.classifier,
					&direct_classifier
				);
			if (direct_status != 0 ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					direct_classifier, classifier
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				fprintf(stderr,
					"app reification: projected classifier mismatch operation=%u status=%d "
					"direct=%u classifier=%u\n",
					operation_id, direct_status,
					direct_status == 0 ? direct_classifier : PROTOTYPE_INVALID_ID,
					classifier);
				if (getenv("A_PROGRAM_FORMATION_TRACE")) {
					fprintf(stderr, "app reification direct classifier: ");
					prototype_type_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms, direct_classifier
					);
					fprintf(stderr, "\napp reification projected classifier: ");
					prototype_type_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms, classifier
					);
					fputc('\n', stderr);
					uint32_t trace_ast = operation->source_ast;
					for (uint32_t trace_depth = 0;
						ctx->asts && trace_depth < 16 &&
							trace_ast < ctx->asts->node_count;
						++trace_depth) {
						const struct prototype_ast_node* trace_node =
							&ctx->asts->nodes[trace_ast];
						fprintf(stderr,
							"app reification ast depth=%u ast=%u tag=%d",
							trace_depth, trace_ast, trace_node->tag);
						if (trace_node->tag == PROTOTYPE_AST_APP) {
							fprintf(stderr, " function=%u argument=%u\n",
								trace_node->as.app.function,
								trace_node->as.app.argument);
							trace_ast = trace_node->as.app.function;
							continue;
						}
						if (trace_node->tag == PROTOTYPE_AST_VAR) {
							fprintf(stderr, " binder=%u symbol=%d\n",
								trace_node->as.var.ast_binder_id,
								trace_node->as.var.symbol_id);
						} else {
							fprintf(stderr, "\n");
						}
						break;
					}
					fprintf(
						stderr,
						"app reification context operation=%u context=%u function=%u:%u "
						"argument=%u:%u argument-context=%u action=%u binding=%u\n",
						operation_id,
						operation->context_id,
						function_operation,
						function_projection,
						argument_occurrence,
						argument_projection,
						argument->context_id,
						argument->context_action_substitution,
						argument->binding_id
					);
					if (argument->context_action_substitution != PROTOTYPE_INVALID_ID) {
						const struct prototype_substitution* action =
							prototype_substitution_get(
								&ctx->metadata->substitutions,
								argument->context_action_substitution
							);
						if (action) {
							fprintf(
								stderr,
								"app reification argument action kind=%d source=%u "
								"target=%u first=%u second=%u term=%u\n",
								action->kind,
								action->source_context,
								action->target_context,
								action->first,
								action->second,
								action->term
							);
						}
					}
					uint32_t trace_context = operation->context_id;
					for (uint32_t trace_depth = 0;
						trace_depth < 16 && trace_context != PROTOTYPE_INVALID_ID;
						++trace_depth) {
						const struct prototype_context* context = prototype_context_get(
							&ctx->metadata->contexts, trace_context
						);
						if (!context) {
							break;
						}
						const struct prototype_context_classifier_equation* equation =
							prototype_context_classifier_equation_get(
								&ctx->metadata->contexts,
								context->classifier_equation
							);
						fprintf(
							stderr,
							"app reification context-chain id=%u parent=%u binding=%u "
							"equation=%u answer=%u origin=%u\n",
							trace_context,
							context->parent,
							context->binding_id,
							context->classifier_equation,
							equation ? equation->answer : PROTOTYPE_INVALID_ID,
							equation ? equation->source_ast_binder_id : PROTOTYPE_INVALID_ID
						);
						trace_context = context->parent;
					}
					for (size_t candidate_id = 0;
						candidate_id < ctx->judgement_delta.proposition_count;
						++candidate_id) {
						const struct prototype_judgement_proposition* candidate =
							&ctx->judgement_delta.propositions[candidate_id];
						if (candidate->subject == argument_subject) {
							fprintf(
								stderr,
								"app reification delta argument candidate=%zu context=%u "
								"classifier=%u authority=%d:%u proof=%u\n",
								candidate_id,
								candidate->context_id,
								candidate->classifier,
								candidate->authority_kind,
								candidate->authority_id,
								candidate->first_candidate_derivation
							);
						}
					}
					if (ctx->judgement) {
						for (size_t candidate_id = 0;
							candidate_id < ctx->judgement->proposition_count;
							++candidate_id) {
							const struct prototype_judgement_proposition* candidate =
								&ctx->judgement->propositions[candidate_id];
							if (candidate->subject == argument_subject) {
								fprintf(
									stderr,
									"app reification accepted argument candidate=%zu context=%u "
									"classifier=%u authority=%d:%u proof=%u\n",
									candidate_id,
									candidate->context_id,
									candidate->classifier,
									candidate->authority_kind,
									candidate->authority_id,
									candidate->first_candidate_derivation
								);
							}
						}
					}
					for (uint32_t alternative =
							prototype_typed_projection_first_for_typing_node(
								&ctx->typing->context_projections, argument_occurrence
							);
						alternative != PROTOTYPE_INVALID_ID;
						alternative = prototype_typed_projection_next_for_typing_node(
							&ctx->typing->context_projections, alternative
						)) {
						const struct prototype_typed_projection* projected =
							prototype_typed_projection_get(
								&ctx->typing->context_projections, alternative
							);
						uint32_t projected_context = PROTOTYPE_INVALID_ID;
						uint32_t projected_substitution = PROTOTYPE_INVALID_ID;
						uint32_t projected_subject = PROTOTYPE_INVALID_ID;
						uint32_t projected_classifier = PROTOTYPE_INVALID_ID;
						int projected_status =
							operation_typed_projection_materialized_typing_answer(
								ctx,
								alternative,
								&projected_context,
								&projected_substitution,
								&projected_subject,
								&projected_classifier
							);
						fprintf(
							stderr,
							"app reification argument projection=%u source=%u context-projection=%u "
							"expanded=%d status=%d context=%u substitution=%u subject=%u "
							"classifier=%u\n",
							alternative,
							projected ? projected->source_occurrence : PROTOTYPE_INVALID_ID,
							projected ? projected->context_projection : PROTOTYPE_INVALID_ID,
							projected ? projected->topology_expanded : 0,
							projected_status,
							projected_context,
							projected_substitution,
							projected_subject,
							projected_classifier
						);
					}
					fprintf(stderr, "app reification function classifier: ");
					prototype_type_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms,
						function_evidence.classifier
					);
					fprintf(stderr, "\napp reification argument classifier: ");
					prototype_type_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms,
						argument_evidence.classifier
					);
					fputc('\n', stderr);
				}
				return -1;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			uint32_t proof_classifier;
			int app_classifier_status;
			if (app_proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_STATIC_FAMILY_APP_ELIM) {
				app_classifier_status =
					prototype_judgement_static_family_app_classifier(
						ctx->judgement_delta.contexts,
						ctx->judgement_delta.substitutions,
						ctx->terms,
						ctx->type_declarations,
						operation->context_id,
						function_evidence.classifier,
						argument_evidence.subject,
						argument_evidence.classifier,
						&proof_classifier
					);
			} else {
				app_classifier_status =
					prototype_judgement_delta_app_elim_classifier(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						function_evidence.classifier,
						argument_evidence.subject,
						argument_evidence.classifier,
						&proof_classifier
					);
			}
			if (app_classifier_status != 0) {
				fprintf(
					stderr,
					"app reification: classifier derivation failed operation=%u\n",
					operation_id
				);
				return -1;
			}
			if (prototype_judgement_delta_record_app_elim(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier,
					app_proof_kind,
					&function_evidence,
					&argument_evidence
				) != 0) {
					fprintf(stderr,
						"app reification: APP_ELIM record failed operation=%u "
						"proof=%u solved=%u core=%u\n",
						operation_id, proof_classifier, classifier, core_term);
					return -1;
			}
			return 0;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE: {
			uint32_t child_term;
			uint32_t child_operation;
			uint32_t child_projection;
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
				) != 0 || operation_solver_projected_child(
					ctx, typed_projection, child_role, 0, &child_projection
				) != 0) {
				return -1;
			}
			uint32_t child_context;
			uint32_t child_substitution;
			uint32_t child_classifier;
			if (operation_typed_projection_publication_typing_answer(
					ctx,
					child_projection,
					&child_context,
					&child_substitution,
					&child_term,
					&child_classifier
				) != 0) {
				return -1;
			}
			(void)child_substitution;
			int status = operation_solver_reify_projected_core_proof(
				ctx, child_projection, child_term, depth + 1
			);
			if (status != 0) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(
						stderr,
						"A_PROGRAM_PROOF_TRACE boundary=%u waits=child:%u "
						"projection=%u status=%d\n",
						operation_id, child_operation, child_projection, status
					);
				}
				return status;
			}
			struct prototype_judgement_selected_evidence child_evidence;
			status = operation_solver_select_projected_evidence_in_context(
				ctx,
				child_projection,
				child_context,
				child_term,
				child_classifier,
				&child_evidence
			);
			if (status != 0) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(
						stderr,
						"A_PROGRAM_PROOF_TRACE boundary=%u missing=child:%u "
						"projection=%u context=%u subject=%u classifier=%u status=%d\n",
						operation_id, child_operation, child_projection,
						child_context, child_term, child_classifier, status
					);
				}
				return status;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			uint32_t derived_classifier;
			if (prototype_judgement_cbpv_boundary_classifier(
					ctx->terms,
					ctx->type_declarations,
					core_term,
					child_classifier,
					&derived_classifier
				) != 0) {
				return -1;
			}
			int classifier_is_derived = derived_classifier != classifier;
			uint32_t boundary_operation = classifier_is_derived ?
				PROTOTYPE_INVALID_ID : operation_id;
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, boundary_operation
			);
			if (prototype_judgement_delta_record_cbpv_boundary(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					child_operation,
					&child_evidence
				) != 0) {
				return -1;
			}
			if (classifier_is_derived) {
				struct prototype_judgement_selected_evidence boundary_evidence;
				if (operation_solver_delta_select_evidence(
						ctx,
						PROTOTYPE_INVALID_ID,
						operation->context_id,
						core_term,
						derived_classifier,
						&boundary_evidence
					) != 0) {
					return -1;
				}
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, operation_id
				);
				if (
					prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						operation_id,
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
			uint32_t computation_projection;
			uint32_t return_projection;
			if (term->tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
				return -1;
			}
			if (compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					&computation_occurrence
				) != 0) {
				return -1;
			}
			if (compile_occurrence_child(
					ctx, operation_id,
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&return_operation_id
				) != 0) {
				return -1;
			}
			if (operation_solver_projected_child(
					ctx, typed_projection, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION,
					0, &computation_projection
				) != 0) {
				return -1;
			}
			if (operation_solver_projected_child(
					ctx, typed_projection,
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&return_projection
				) != 0) {
				return -1;
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
			struct prototype_typed_occurrence proof_computation =
				ctx->metadata->typed_occurrences.occurrences[computation_occurrence];
			struct prototype_typed_occurrence proof_return =
				ctx->metadata->typed_occurrences.occurrences[return_operation_id];
			uint32_t computation_subject;
			uint32_t return_subject;
			if (operation_typed_projection_publication_typing_answer(
					ctx, computation_projection, &proof_computation.context_id,
					&proof_computation.context_action_substitution,
					&computation_subject, &proof_computation.classifier
				) != 0) {
				return -1;
			}
			if (operation_typed_projection_publication_typing_answer(
					ctx, return_projection, &proof_return.context_id,
					&proof_return.context_action_substitution,
					&return_subject, &proof_return.classifier
				) != 0) {
				return -1;
			}
			const struct prototype_typed_occurrence* computation = &proof_computation;
			const struct prototype_typed_occurrence* return_operation = &proof_return;
			uint32_t premise_count = 2 + 2 * clause_count;
				uint32_t premise_occurrences[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_projections[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_contexts[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_terms[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
				struct prototype_judgement_selected_evidence premise_evidence[
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
				];
			premise_occurrences[0] = computation_occurrence;
			premise_occurrences[1] = return_operation_id;
			premise_projections[0] = computation_projection;
			premise_projections[1] = return_projection;
			premise_contexts[0] = computation->context_id;
			premise_contexts[1] = return_operation->context_id;
			premise_terms[0] = computation_subject;
			premise_terms[1] = return_subject;
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
				int status = operation_solver_reify_projected_core_proof(
					ctx,
					premise_projections[i],
					premise_terms[i],
					depth + 1
				);
				if (status != 0) {
					if (getenv("A_PROGRAM_PROOF_TRACE")) {
						fprintf(
							stderr,
							"A_PROGRAM_PROOF_TRACE fold=%u waits=premise:%u:%u "
							"projection=%u status=%d\n",
							operation_id, i, premise_occurrences[i],
							premise_projections[i], status
						);
					}
					return status;
				}
				if (clause_count == 0) {
					status = operation_solver_select_projected_evidence_in_context(
						ctx,
						premise_projections[i],
						premise_contexts[i],
						premise_terms[i],
						premise_classifiers[i],
						&premise_evidence[i]
					);
					if (status != 0) {
						if (getenv("A_PROGRAM_PROOF_TRACE")) {
							fprintf(
								stderr,
								"A_PROGRAM_PROOF_TRACE fold=%u missing=premise:%u:%u "
								"projection=%u context=%u status=%d\n",
								operation_id, i, premise_occurrences[i],
								premise_projections[i], premise_contexts[i], status
							);
						}
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
			int authority_kind;
			uint32_t authority_id;
			uint32_t materialized_context;
			uint32_t materialized_substitution;
			if (operation_solver_var_projection_evidence_authority(
					ctx, typed_projection, core_term, &authority_kind, &authority_id,
					&materialized_context, &materialized_substitution
				) != 0 || materialized_context != operation->context_id) {
				return -1;
			}
			(void)authority_id;
			const struct prototype_substitution* projection_action =
				prototype_substitution_get(
					&ctx->metadata->substitutions,
					materialized_substitution
				);
			if (!projection_action) {
				return -1;
			}
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(stderr,
					"A_PROGRAM_PROOF_TRACE var=%u context=%u action=%u core=%u "
					"binding=%u classifier=%u\n",
					operation_id, operation->context_id,
					operation->context_action_substitution, core_term,
					term->tag == PROTOTYPE_TERM_VAR ? term->as.var.binding_id :
						PROTOTYPE_INVALID_ID,
					classifier);
			}
			if (authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING) {
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
				const struct prototype_substitution* action = projection_action;
				uint32_t source_term = operation->source_core_term <
					ctx->terms->term_count ? operation->source_core_term :
					stored_operation->core_term;
				uint32_t source_binding_context;
				if (source_term >= ctx->terms->term_count ||
					ctx->terms->terms[source_term].tag !=
						PROTOTYPE_TERM_VAR || !action ||
					action->source_context != operation->context_id) {
					fprintf(stderr,
						"reindex VAR invalid action operation=%u candidate=%u context=%u "
						"action=%u source=%u target=%u source-core=%u\n",
						operation_id, stored_operation->context_id, operation->context_id,
						materialized_substitution,
						action ? action->source_context : PROTOTYPE_INVALID_ID,
						action ? action->target_context : PROTOTYPE_INVALID_ID,
						source_term);
					return -1;
				}
				if (prototype_context_find_binding(
						&ctx->metadata->contexts,
						action->target_context,
						ctx->terms->terms[source_term].as.var.binding_id,
						&source_binding_context
					) != 0) {
					fprintf(stderr, "reindex VAR source binding missing operation=%u\n", operation_id);
					return -1;
				}
				const struct prototype_context* source_binding = prototype_context_get(
					&ctx->metadata->contexts, source_binding_context
				);
				uint32_t source_classifier = source_binding ?
					operation_context_classifier_term(
						ctx, source_binding_context
					) : PROTOTYPE_INVALID_ID;
				if (source_classifier == PROTOTYPE_INVALID_ID) {
					fprintf(stderr, "reindex VAR source classifier missing operation=%u\n", operation_id);
					return -1;
				}
				int source_status = operation_solver_require_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					action->target_context,
					source_term,
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
							ctx->terms->terms[source_term].as.var.binding_id,
							source_classifier
						) != 0) {
						return -1;
					}
					source_status = operation_solver_require_evidence_in_context(
						ctx,
						PROTOTYPE_INVALID_ID,
						action->target_context,
						source_term,
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
						source_term,
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
						materialized_substitution,
						&reindexed_evidence
					) != 0 || reindexed_evidence.subject != core_term) {
					fprintf(stderr, "reindex VAR derivation failed operation=%u source=%u core=%u\n",
						operation_id, reindexed_evidence.subject, core_term);
					return -1;
				}
				uint32_t conclusion_occurrence = typed_projection ==
					operation_solver_publication_projection(ctx, operation_id) ?
					operation_id : PROTOTYPE_INVALID_ID;
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, conclusion_occurrence
				);
				if (prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						conclusion_occurrence,
						&reindexed_evidence,
						classifier,
						core_term
					) != 0) {
					fprintf(stderr,
						"reindex VAR classifier exposure failed operation=%u "
						"ast=%u ast-tag=%d ast-binder=%u symbol=%d binding=%u "
						"occurrence-binding=%u source-binding=%u actual=%u expected=%u "
						"source-classifier=%u action=%u "
						"source-context=%u target-context=%u\n",
						operation_id, operation->source_ast,
						ctx->asts && operation->source_ast < ctx->asts->node_count ?
							ctx->asts->nodes[operation->source_ast].tag : -1,
						ctx->asts && operation->source_ast < ctx->asts->node_count &&
							ctx->asts->nodes[operation->source_ast].tag == PROTOTYPE_AST_VAR ?
							ctx->asts->nodes[operation->source_ast].as.var.ast_binder_id :
							PROTOTYPE_INVALID_ID,
						operation->source_symbol_id,
						operation->referenced_ast_binder_id,
						operation->binding_id,
						ctx->terms->terms[source_term].as.var.binding_id,
						reindexed_evidence.classifier, classifier,
						source_classifier, materialized_substitution,
						action->target_context, action->source_context);
					fprintf(stderr, "  actual classifier: ");
					prototype_type_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms,
						reindexed_evidence.classifier
					);
					fprintf(stderr, "\n  expected classifier: ");
					prototype_type_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms, classifier
					);
					fprintf(stderr, "\n  source symbol name: %s\n",
						symbol_to_string(ctx->symbols, operation->source_symbol_id));
					return -1;
				}
				return 0;
			}
			if (term->tag != PROTOTYPE_TERM_VAR) {
				return -1;
			}
			int status = operation_solver_select_authority_evidence_in_context(
				ctx, operation_id, authority_kind, authority_id,
				operation->context_id, core_term, classifier, NULL
			);
			if (status <= 0) {
				return status;
			}
			if (!prototype_context_contains_binding(
					&ctx->metadata->contexts,
					operation->context_id,
					ctx->terms->terms[core_term].as.var.binding_id
				)) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(stderr,
						"A_PROGRAM_PROOF_TRACE var=%u missing-binding=%u context=%u\n",
						operation_id, ctx->terms->terms[core_term].as.var.binding_id,
						operation->context_id);
				}
				return 1;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
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
			return operation_solver_select_authority_evidence_in_context(
				ctx, operation_id, authority_kind, authority_id,
				operation->context_id, core_term, classifier, NULL
			);
		}
		case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE: {
			if (operation->wrapped_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			struct prototype_judgement_selected_evidence source_evidence;
			int projection_is_identity;
			int source_status = operation_solver_project_expected_evidence(
				ctx,
				typed_projection,
				operation_id,
				operation->wrapped_occurrence,
				core_term,
				depth,
				&source_evidence,
				&projection_is_identity
			);
			if (source_status != 0) {
				return source_status;
			}
			if (projection_is_identity &&
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					source_evidence.classifier, classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			return prototype_judgement_delta_record_expected_type_exposure(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				operation_id,
				&source_evidence,
				classifier,
				core_term
			);
		}
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH: {
			uint32_t scrutinee_occurrence;
			uint32_t scrutinee_projection;
			uint32_t branch_occurrences[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			uint32_t branch_substitutions[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			struct prototype_judgement_selected_evidence
				branch_evidence[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			if (term->tag != PROTOTYPE_TERM_MATCH ||
				compile_occurrence_child(
					ctx, operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
					&scrutinee_occurrence
				) != 0 || operation_solver_projected_child(
					ctx, typed_projection, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
					&scrutinee_projection
				) != 0 ||
				operation->first_case + operation->case_count >
					ctx->metadata->typed_occurrences.case_count ||
				term->as.match.case_count != operation->case_count ||
				operation->case_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
				return -1;
			}
			uint32_t scrutinee_context;
			uint32_t scrutinee_substitution;
			uint32_t scrutinee_subject;
			uint32_t scrutinee_classifier;
			if (operation_typed_projection_publication_typing_answer(
					ctx,
					scrutinee_projection,
					&scrutinee_context,
					&scrutinee_substitution,
					&scrutinee_subject,
					&scrutinee_classifier
				) != 0) {
				return -1;
			}
			(void)scrutinee_occurrence;
			(void)scrutinee_context;
			(void)scrutinee_substitution;
			(void)scrutinee_classifier;
			int status = operation_solver_reify_projected_core_proof(
				ctx, scrutinee_projection, scrutinee_subject, depth + 1
			);
			if (status != 0) {
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(
						stderr,
						"A_PROGRAM_PROOF_TRACE match=%u waits=scrutinee:%u "
						"projection=%u status=%d\n",
						operation_id, scrutinee_occurrence,
						scrutinee_projection, status
					);
				}
				return status;
			}
		for (uint32_t i = 0; i < operation->case_count; ++i) {
			uint32_t case_id = term->as.match.first_case + i;
			uint32_t case_body_occurrence;
			uint32_t case_body_projection;
			uint32_t source_case = operation->first_case + i;
			uint32_t match_case_projection;
			if (prototype_typed_match_case_projection_find(
					&ctx->typing->context_projections,
					source_case,
					typed_projection,
					&match_case_projection
				) != 0 || match_case_projection >=
					PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
				return -1;
			}
			uint32_t refinement_constraint = ctx->typing->constraints.
				branch_refinement_constraint_for_match_case_projection[
					match_case_projection
				];
			const struct prototype_typing_constraint_solution* refinement =
				refinement_constraint < ctx->typing->constraints.constraint_count ?
					&ctx->typing->constraints.solutions[refinement_constraint] : NULL;
			if (!refinement) {
				return -1;
			}
			if (refinement->branch_refinement.refinement_status ==
				PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
				branch_occurrences[i] = PROTOTYPE_INVALID_ID;
				branch_substitutions[i] = PROTOTYPE_INVALID_ID;
				memset(&branch_evidence[i], 0, sizeof(branch_evidence[i]));
				continue;
			}
			if (refinement->branch_refinement.refinement_status !=
					PROTOTYPE_INDEX_REFINEMENT_SOLVED &&
				refinement->branch_refinement.refinement_status !=
					PROTOTYPE_INDEX_REFINEMENT_CONSTANT) {
				return -1;
			}
			if (case_id >= ctx->terms->case_count ||
					compile_occurrence_child(
						ctx, operation_id, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
						&case_body_occurrence
					) != 0 || operation_solver_projected_child(
						ctx, typed_projection, PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
						&case_body_projection
					) != 0) {
					return -1;
				}
				uint32_t case_context;
				uint32_t case_substitution;
				uint32_t case_subject;
				uint32_t case_classifier;
				if (operation_typed_projection_publication_typing_answer(
						ctx,
						case_body_projection,
						&case_context,
						&case_substitution,
						&case_subject,
						&case_classifier
					) != 0) {
					return -1;
			}
			branch_occurrences[i] = case_body_occurrence;
			branch_substitutions[i] =
				refinement->branch_refinement.refinement_status ==
					PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
					refinement->branch_refinement.substitution :
					PROTOTYPE_INVALID_ID;
				status = operation_solver_reify_projected_core_proof(
					ctx, case_body_projection, case_subject, depth + 1
				);
				if (status != 0) {
					if (getenv("A_PROGRAM_PROOF_TRACE")) {
						fprintf(
							stderr,
							"A_PROGRAM_PROOF_TRACE match=%u waits=case:%u:%u "
							"projection=%u status=%d\n",
							operation_id, i, case_body_occurrence,
							case_body_projection, status
						);
					}
					return status;
				}
				status = operation_solver_select_projected_evidence_in_context(
					ctx, case_body_projection, case_context, case_subject,
					case_classifier, &branch_evidence[i]
				);
				if (status != 0) {
					return status;
				}
			}
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			status = prototype_judgement_delta_record_materialized_match_motive(
				&ctx->judgement_delta,
				ctx->terms,
				core_term,
				classifier,
				branch_occurrences,
				branch_evidence,
				branch_substitutions,
				operation->case_count
			);
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(stderr,
					"A_PROGRAM_PROOF_TRACE recorded-match operation=%u status=%d delta-propositions=%zu delta-derivations=%zu\n",
					operation_id, status,
					ctx->judgement_delta.proposition_count,
					ctx->judgement_delta.derivation_candidate_count);
			}
			if (status != 0 || operation_solver_refresh_delta_resource_usage(ctx) != 0) {
				return status != 0 ? status : -1;
			}
			struct prototype_judgement_selected_evidence match_evidence;
			return operation_solver_select_projected_evidence_in_context(
				ctx, typed_projection, operation->context_id, core_term, classifier,
				&match_evidence
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
			struct prototype_judgement_selected_evidence source_evidence;
			int projection_is_identity;
			int source_status = operation_solver_project_expected_evidence(
				ctx,
				typed_projection,
				operation_id,
				operation->wrapped_occurrence,
				core_term,
				depth,
				&source_evidence,
				&projection_is_identity
			);
			if (source_status != 0) {
				return source_status;
			}
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(stderr,
					"A_PROGRAM_PROOF_TRACE expected=%u projection=%u context=%u "
					"wrapped=%u source-occurrence=%u source-authority=%d:%u "
					"source-context=%u subject=%u classifier=%u expected-classifier=%u\n",
					operation_id, typed_projection, operation->context_id,
					operation->wrapped_occurrence, source_evidence.occurrence_id,
					source_evidence.authority_kind, source_evidence.authority_id,
					source_evidence.context_id, source_evidence.subject,
					source_evidence.classifier, classifier);
			}
			(void)projection_is_identity;
			prototype_judgement_delta_set_context(
				&ctx->judgement_delta, operation->context_id
			);
			prototype_judgement_delta_set_occurrence(
				&ctx->judgement_delta, operation_id
			);
			int exposure_status =
				prototype_judgement_delta_record_expected_type_exposure(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					operation_id,
					&source_evidence,
					classifier,
					core_term
				);
			return exposure_status;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_ATOM:
		case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR:
			if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
				const struct prototype_substitution* action =
					prototype_substitution_get(
						&ctx->metadata->substitutions,
						operation->context_action_substitution
					);
				if (!action || action->source_context != operation->context_id) {
					return -1;
				}
				if (action->kind != PROTOTYPE_SUBSTITUTION_IDENTITY) {
					uint32_t source_projection;
					if (operation_solver_canonical_typed_projection(
							ctx, operation_id, &source_projection
						) != 0) {
						return -1;
					}
					if (source_projection == PROTOTYPE_INVALID_ID ||
						source_projection == typed_projection) {
						return -1;
					}
					uint32_t source_context;
					uint32_t source_substitution;
					uint32_t source_core_term;
					uint32_t source_classifier;
					if (operation_typed_projection_publication_typing_answer(
							ctx, source_projection, &source_context,
							&source_substitution, &source_core_term,
							&source_classifier
						) != 0 || source_context != action->target_context) {
						return -1;
					}
					if (getenv("A_PROGRAM_PROOF_TRACE")) {
						fprintf(
							stderr,
							"A_PROGRAM_PROOF_TRACE projected-leaf=%u tag=%d projection=%u "
							"context=%u classifier=%u target-projection=%u target-context=%u "
							"target-classifier=%u\n",
							operation_id, operation->tag, source_projection,
							source_context, source_classifier, typed_projection,
							operation->context_id, classifier
						);
					}
					(void)source_substitution;
					int source_status = operation_solver_reify_projected_core_proof(
						ctx, source_projection, source_core_term, depth + 1
					);
					if (source_status != 0) {
						return source_status;
					}
					struct prototype_judgement_selected_evidence source_evidence;
					source_status = operation_solver_select_projected_evidence_in_context(
						ctx,
						source_projection,
						action->target_context,
						source_core_term,
						source_classifier,
						&source_evidence
					);
					if (source_status != 0) {
						if (getenv("A_PROGRAM_PROOF_TRACE")) {
							fprintf(stderr,
								"A_PROGRAM_PROOF_TRACE projected-atom=%u source-status=%d "
								"context=%u source=%u classifier=%u target-context=%u\n",
								operation_id, source_status, operation->context_id,
								source_core_term, source_classifier,
								action->target_context);
							if (ctx->judgement) {
								for (size_t candidate_id = 0;
									candidate_id < ctx->judgement->proposition_count;
									++candidate_id) {
									const struct prototype_judgement_proposition* candidate =
										&ctx->judgement->propositions[candidate_id];
									if (candidate->subject == source_core_term) {
										fprintf(stderr,
											"A_PROGRAM_PROOF_TRACE projected-atom-candidate=%zu "
											"occurrence=%u authority=%d:%u context=%u classifier=%u\n",
											candidate_id, candidate->occurrence_id,
											candidate->authority_kind, candidate->authority_id,
											candidate->context_id, candidate->classifier);
									}
								}
							}
						}
						return source_status;
					}
					prototype_judgement_delta_set_context(
						&ctx->judgement_delta, operation->context_id
					);
					prototype_judgement_delta_set_occurrence(
						&ctx->judgement_delta, PROTOTYPE_INVALID_ID
					);
					struct prototype_judgement_selected_evidence reindexed_evidence;
					int reindex_status =
						prototype_judgement_delta_record_substitution_reindex(
							&ctx->judgement_delta,
							ctx->terms,
							ctx->type_declarations,
							&source_evidence,
							operation->context_action_substitution,
							&reindexed_evidence
						);
					if (reindex_status != 0 || reindexed_evidence.subject != core_term) {
						if (getenv("A_PROGRAM_PROOF_TRACE")) {
							fprintf(
								stderr,
								"A_PROGRAM_PROOF_TRACE projected-leaf-reindex=%u status=%d "
								"source-subject=%u source-classifier=%u target-subject=%u "
								"target-classifier=%u expected-subject=%u expected-classifier=%u\n",
								operation_id, reindex_status, source_evidence.subject,
								source_evidence.classifier,
								reindex_status == 0 ? reindexed_evidence.subject :
									PROTOTYPE_INVALID_ID,
								reindex_status == 0 ? reindexed_evidence.classifier :
									PROTOTYPE_INVALID_ID,
								core_term, classifier
							);
						}
						return -1;
					}
					prototype_judgement_delta_set_occurrence(
						&ctx->judgement_delta, operation_id
					);
					int exposure_status =
						prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						operation_id,
						&reindexed_evidence,
						classifier,
						core_term
					);
					if (getenv("A_PROGRAM_PROOF_TRACE")) {
						fprintf(
							stderr,
							"A_PROGRAM_PROOF_TRACE projected-leaf-exposure=%u status=%d "
							"subject=%u classifier=%u expected-subject=%u "
							"expected-classifier=%u\n",
							operation_id, exposure_status, reindexed_evidence.subject,
							reindexed_evidence.classifier, core_term, classifier
						);
					}
					return exposure_status;
				}
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
				if (getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(
						stderr,
						"A_PROGRAM_PROOF_TRACE constructor-existing=%u context=%u "
						"subject=%u classifier=%u status=%d\n",
						operation_id, operation->context_id, core_term, classifier,
						existing_status
					);
				}
				if (existing_status <= 0) return existing_status;
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				int constructor_status =
					prototype_judgement_delta_record_constructor_intro(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier
				);
				if (constructor_status != 0 && getenv("A_PROGRAM_PROOF_TRACE")) {
					fprintf(
						stderr,
						"A_PROGRAM_PROOF_TRACE constructor-intro=%u context=%u "
						"subject=%u classifier=%u status=%d\n",
						operation_id, operation->context_id, core_term, classifier,
						constructor_status
					);
				}
				return constructor_status;
			}
			int full_type_instance = operation_solver_type_instance_is_fully_applied(
				ctx, core_term
			);
			int declaration_head = operation_solver_type_instance_is_declaration_head(
				ctx, core_term
			);
			if (full_type_instance < 0 || declaration_head < 0) {
				return -1;
			}
			if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
				term->tag == PROTOTYPE_TERM_TYPE_VIEW &&
				(full_type_instance || declaration_head) &&
				prototype_context_get(
					&ctx->metadata->contexts, operation->context_id
				) && prototype_context_get(
					&ctx->metadata->contexts, operation->context_id
				)->parent == PROTOTYPE_INVALID_ID) {
				prototype_judgement_delta_set_context(
					&ctx->judgement_delta, operation->context_id
				);
				prototype_judgement_delta_set_occurrence(
					&ctx->judgement_delta, PROTOTYPE_INVALID_ID
				);
				return prototype_judgement_delta_record_type_formation(
					&ctx->judgement_delta,
					ctx->terms,
					ctx->type_declarations,
					core_term,
					classifier
				);
			}
			/* A named occurrence may share its Core term with the declaration or
			 * generated helper that established the evidence. Preserve occurrence
			 * provenance by deriving this occurrence from that accepted fact instead
			 * of requiring the original occurrence id to be reused. */
			if (operation_solver_named_atom_can_share_evidence(
					ctx, operation, term, core_term
				)) {
				struct prototype_judgement_selected_evidence source_evidence;
				int source_status = operation_solver_select_evidence_in_context(
					ctx,
					PROTOTYPE_INVALID_ID,
					operation->context_id,
					core_term,
					classifier,
					&source_evidence
				);
				if (source_status < 0) {
					return -1;
				}
				if (source_status == 0) {
					prototype_judgement_delta_set_context(
						&ctx->judgement_delta, operation->context_id
					);
					prototype_judgement_delta_set_occurrence(
						&ctx->judgement_delta, operation_id
					);
					return prototype_judgement_delta_record_expected_type_exposure(
						&ctx->judgement_delta,
						ctx->terms,
						ctx->type_declarations,
						operation_id,
						&source_evidence,
						classifier,
						core_term
					);
				}
			}
			return 0;
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			return operation_solver_materialize_induction_hypothesis_projection_judgement(
				ctx, typed_projection
			);
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
						PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
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

static int operation_solver_reify_core_proof(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t core_term,
	uint32_t depth
) {
	uint32_t typed_projection = operation_solver_publication_projection(
		ctx, operation_id
	);
	if (typed_projection == PROTOTYPE_INVALID_ID) return 1;
	struct prototype_typing_reification_cache_entry* cached = NULL;
	if (ctx && ctx->metadata &&
		operation_id < ctx->metadata->typed_occurrences.occurrence_count) {
		uint32_t operation_context;
		uint32_t operation_substitution;
		uint32_t operation_classifier;
		if (operation_occurrence_materialized_typing_status(
				ctx, operation_id, &operation_context, &operation_substitution,
				&core_term, &operation_classifier
			) != 0) {
			return -1;
		}
		(void)operation_substitution;
		/* The caller may have reached this occurrence through a structurally
		 * shared parent Core term. Evidence belongs to the typed occurrence's
		 * checked projection, never to the representative child recovered from
		 * that parent. Keep the cache validation on the same Layer T-owned
		 * projection used by the reification body. */
		cached = &ctx->evidence_workspace.reification_cache[operation_id];
		if (cached->context_id == operation_context &&
			cached->classifier == operation_classifier) {
			if (cached->state == PROTOTYPE_TYPING_REIFICATION_SUCCEEDED) {
				struct prototype_judgement_selected_evidence evidence;
				int evidence_status =
					operation_solver_select_operation_evidence_in_context(
						ctx, operation_id, operation_context, core_term,
						operation_classifier, &evidence
					);
				if (evidence_status == 0) {
					ctx->metadata->proof_reify_current_pass_reuse_count++;
					return 0;
				}
				if (evidence_status < 0 || evidence_status == 2) {
					return -1;
				}
				cached->state = PROTOTYPE_TYPING_REIFICATION_UNSEEN;
			}
			if (cached->state == PROTOTYPE_TYPING_REIFICATION_IN_PROGRESS) {
				ctx->metadata->proof_reify_cycle_count++;
				return 1;
			}
		}
		cached->state = PROTOTYPE_TYPING_REIFICATION_IN_PROGRESS;
		cached->context_id = operation_context;
		cached->classifier = operation_classifier;
	}
	if (ctx && ctx->metadata) {
		if (depth == 0) {
			ctx->metadata->proof_reify_root_count++;
		} else {
			ctx->metadata->proof_reify_recursive_count++;
		}
	}
	int status = operation_solver_reify_core_proof_impl(
		ctx, typed_projection, core_term, depth
	);
	if (ctx && ctx->metadata) {
		if (status == 0) {
			ctx->metadata->proof_reify_success_count++;
		} else if (status > 0) {
			ctx->metadata->proof_reify_residual_count++;
		} else {
			ctx->metadata->proof_reify_failure_count++;
		}
	}
	if (cached) {
		cached->state = status == 0 ? PROTOTYPE_TYPING_REIFICATION_SUCCEEDED :
			PROTOTYPE_TYPING_REIFICATION_UNSEEN;
	}
	return status;
}

static int operation_solver_reify_projected_core_proof(
	struct compile_context* ctx,
	uint32_t typed_projection,
	uint32_t core_term,
	uint32_t depth
) {
	if (ctx && ctx->metadata) {
		ctx->metadata->proof_reify_recursive_count++;
	}
	int status = operation_solver_reify_core_proof_impl(
		ctx, typed_projection, core_term, depth
	);
	if (ctx && ctx->metadata) {
		if (status == 0) {
			ctx->metadata->proof_reify_success_count++;
		} else if (status > 0) {
			ctx->metadata->proof_reify_residual_count++;
		} else {
			ctx->metadata->proof_reify_failure_count++;
		}
	}
	return status;
}

struct operation_publication_subject_evaluator {
	uint8_t state[PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY];
	uint8_t changed[PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY];
	uint32_t subject[PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY];
};

struct operation_publication_classifier_evaluator {
	struct compile_context* ctx;
	uint8_t state[PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY];
	uint8_t changed[PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY];
	uint32_t classifier[PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY];
	struct operation_classifier_rhs_evaluator rhs;
	struct operation_publication_subject_evaluator subjects;
};

static int operation_publication_projection_tuple(
	const struct compile_context* ctx,
	uint32_t typed_projection,
	struct prototype_typed_publication_projection** p_publication
) {
	if (!ctx || !ctx->typing || !ctx->metadata || !p_publication ||
		typed_projection >=
			ctx->typing->context_projections.typed_projection_count) {
		return -1;
	}
	const struct prototype_typed_projection* projection =
		prototype_typed_projection_get(
			&ctx->typing->context_projections, typed_projection
		);
	if (!projection || projection->source_occurrence >=
		ctx->metadata->typed_publication.count) {
		return -1;
	}
	struct prototype_typed_publication_projection* publication =
		&ctx->metadata->typed_publication.projections[
			projection->source_occurrence
		];
	if (publication->source_occurrence != projection->source_occurrence ||
		publication->typed_projection != typed_projection ||
		publication->subject == PROTOTYPE_INVALID_ID ||
		publication->classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_publication = publication;
	return 0;
}

/* Every equation operand is an exact TypedProjection, while the publication
 * view stores only the selected projection for each occurrence. Evaluate the
 * non-selected projections from the same solved Layer T answer, but mutate only
 * selected publication tuples. */
static int operation_publication_projection_answer(
	struct compile_context* ctx,
	uint32_t typed_projection,
	struct prototype_typed_publication_projection** p_publication,
	uint32_t* p_source_occurrence,
	uint32_t* p_subject,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->typing || !p_publication || !p_source_occurrence ||
		!p_subject || !p_classifier) {
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
	*p_publication = NULL;
	*p_source_occurrence = projection->source_occurrence;
	int publication_status = operation_publication_projection_tuple(
		ctx, typed_projection, p_publication
	);
	if (publication_status < 0) {
		return -1;
	}
	if (publication_status == 0) {
		*p_subject = (*p_publication)->subject;
		*p_classifier = (*p_publication)->classifier;
		return 0;
	}
	uint32_t context;
	uint32_t substitution;
	int answer_status = operation_typed_projection_materialized_typing_answer(
		ctx,
		typed_projection,
		&context,
		&substitution,
		p_subject,
		p_classifier
	);
	(void)context;
	(void)substitution;
	return answer_status;
}

static int operation_publication_projection_classifier_answer(
	struct compile_context* ctx,
	uint32_t typed_projection,
	struct prototype_typed_publication_projection** p_publication,
	uint32_t* p_source_occurrence,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->typing || !p_publication || !p_source_occurrence ||
		!p_classifier) {
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
	*p_publication = NULL;
	*p_source_occurrence = projection->source_occurrence;
	int publication_status = operation_publication_projection_tuple(
		ctx, typed_projection, p_publication
	);
	if (publication_status < 0) {
		return -1;
	}
	if (publication_status == 0) {
		*p_classifier = (*p_publication)->classifier;
		return 0;
	}
	uint32_t equation_id;
	if (operation_solver_projected_classifier_equation_id(
			ctx, typed_projection, &equation_id
		) != 0 || equation_id >= ctx->typing->constraints.constraint_count) {
		return 1;
	}
	const struct prototype_typing_constraint_solution* solution =
		&ctx->typing->constraints.solutions[equation_id];
	if (solution->state != OPERATION_CONSTRAINT_STATE_SOLVED ||
		solution->result_term == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = solution->result_term;
	return 0;
}

static int operation_evaluate_publication_subject(
	struct compile_context* ctx,
	uint32_t typed_projection,
	struct operation_publication_subject_evaluator* evaluator,
	uint32_t* p_subject
) {
	if (!ctx || !evaluator || !p_subject || typed_projection >=
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
		return -1;
	}
	if (evaluator->state[typed_projection] == 2) {
		*p_subject = evaluator->subject[typed_projection];
		return 0;
	}
	if (evaluator->state[typed_projection] == 1) {
		*p_subject = evaluator->subject[typed_projection];
		return 0;
	}
	struct prototype_typed_publication_projection* publication;
	uint32_t source_occurrence;
	uint32_t original_subject;
	uint32_t ignored_classifier;
	int answer_status = operation_publication_projection_answer(
		ctx,
		typed_projection,
		&publication,
		&source_occurrence,
		&original_subject,
		&ignored_classifier
	);
	if (answer_status != 0) {
		return answer_status;
	}
	if (source_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* occurrence =
		&ctx->metadata->typed_occurrences.occurrences[
			source_occurrence
		];
	/* Accepted input is the immutable boundary of this source epoch. Derived
	 * projections may reindex it, but its selected publication is not rebuilt
	 * from the current epoch's equations. */
	if (publication && source_occurrence < ctx->accepted_typing.occurrence_count) {
		evaluator->subject[typed_projection] = original_subject;
		evaluator->state[typed_projection] = 2;
		*p_subject = original_subject;
		return 0;
	}
	if (getenv("A_PROGRAM_PUBLICATION_SUBJECT_TRACE")) {
		fprintf(
			stderr,
			"publication-subject-enter projection=%u occurrence=%u tag=%d "
			"subject=%u selected=%d\n",
			typed_projection,
			source_occurrence,
			occurrence->tag,
			original_subject,
			publication != NULL
		);
	}
	evaluator->subject[typed_projection] = original_subject;
	evaluator->state[typed_projection] = 1;
	uint32_t subject = original_subject;
	uint32_t first_projection;
	uint32_t second_projection;
	uint32_t first;
	uint32_t second;
	int first_status;
	int second_status;
	switch (occurrence->tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_ATOM: {
			uint32_t ignored_computation;
			if (prototype_term_terminates_witness_info(
					ctx->terms, original_subject, &ignored_computation
				) != 0) {
				break;
			}
			first_status = operation_solver_projected_child(
				ctx,
				typed_projection,
				PROTOTYPE_TERM_CHILD_TERMINATION_EVIDENCE_COMPUTATION,
				0,
				&first_projection
			);
			if (first_status < 0) {
				return -1;
			}
			if (first_status > 0) {
				break;
			}
			first_status = operation_evaluate_publication_subject(
				ctx, first_projection, evaluator, &first
			);
			if (first_status < 0) {
				return -1;
			}
			if (first_status == 0 && prototype_term_terminates_witness(
					ctx->terms, first, &subject
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE:
		case PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE:
			first_status = operation_solver_projected_wrapped(
				ctx,
				typed_projection,
				occurrence->wrapped_occurrence,
				&first_projection
			);
			if (first_status != 0) {
				break;
			}
			if (getenv("A_PROGRAM_PUBLICATION_SUBJECT_TRACE")) {
				fprintf(
					stderr,
					"publication-subject-wrapped projection=%u child=%u\n",
					typed_projection,
					first_projection
				);
			}
			first_status = operation_evaluate_publication_subject(
				ctx, first_projection, evaluator, &first
			);
			if (first_status == 0) {
				subject = first;
			}
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
			first_status = operation_solver_projected_child(
				ctx, typed_projection, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
				&first_projection
			);
			second_status = operation_solver_projected_child(
				ctx, typed_projection, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
				&second_projection
			);
			if (first_status < 0 || second_status < 0) {
				return -1;
			}
			if (first_status > 0 || second_status > 0) {
				break;
			}
			first_status = operation_evaluate_publication_subject(
				ctx, first_projection, evaluator, &first
			);
			second_status = operation_evaluate_publication_subject(
				ctx, second_projection, evaluator, &second
			);
			if (first_status < 0 || second_status < 0) {
				return -1;
			}
			if (first_status == 0 && second_status == 0 &&
				prototype_term_app(ctx->terms, first, second, &subject) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			first_status = operation_solver_projected_child(
				ctx, typed_projection, PROTOTYPE_TERM_CHILD_BODY, 0,
				&first_projection
			);
			if (first_status < 0) {
				return -1;
			}
			if (first_status > 0) {
				break;
			}
			first_status = operation_evaluate_publication_subject(
				ctx, first_projection, evaluator, &first
			);
			if (first_status < 0) {
				return -1;
			}
			if (first_status == 0 && prototype_term_lambda(
					ctx->terms, occurrence->binding_id, first, &subject
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			first_status = operation_solver_projected_child(
				ctx,
				typed_projection,
				PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT,
				0,
				&first_projection
			);
			if (first_status < 0) {
				return -1;
			}
			if (first_status > 0) {
				break;
			}
			first_status = operation_evaluate_publication_subject(
				ctx, first_projection, evaluator, &first
			);
			if (first_status < 0) {
				return -1;
			}
			if (first_status == 0 && prototype_term_induction_hypothesis(
					ctx->terms, occurrence->ih_scope_id, first, &subject
				) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE: {
			int role = occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ?
				PROTOTYPE_TERM_CHILD_RETURN_VALUE :
				occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ?
					PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION :
					PROTOTYPE_TERM_CHILD_FORCE_VALUE;
			first_status = operation_solver_projected_child(
				ctx, typed_projection, role, 0, &first_projection
			);
			if (first_status < 0) {
				return -1;
			}
			if (first_status > 0) {
				break;
			}
			first_status = operation_evaluate_publication_subject(
				ctx, first_projection, evaluator, &first
			);
			if (first_status < 0) {
				return -1;
			}
			if (first_status > 0) {
				break;
			}
			int build_status = occurrence->tag ==
					PROTOTYPE_TYPED_OCCURRENCE_RETURN ?
				prototype_term_return(ctx->terms, first, &subject) :
				occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ?
					prototype_term_thunk(ctx->terms, first, &subject) :
					prototype_term_force(ctx->terms, first, &subject);
			if (build_status != 0) {
				return -1;
			}
			break;
		}
		default:
			break;
	}
	if (publication && subject != original_subject &&
		prototype_typed_publication_view_project_subject(
			&ctx->metadata->typed_publication,
			source_occurrence,
			typed_projection,
			original_subject,
			subject
		) != 0) {
		return -1;
	}
	if (getenv("A_PROGRAM_PUBLICATION_SUBJECT_TRACE") &&
		(subject != original_subject ||
		 occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE)) {
		fprintf(
			stderr,
			"publication-subject projection=%u occurrence=%u tag=%d old=%u "
			"new=%u changed=%d\n",
			typed_projection,
			source_occurrence,
			occurrence->tag,
			original_subject,
			subject,
			subject != original_subject
		);
	}
	evaluator->subject[typed_projection] = subject;
	evaluator->changed[typed_projection] = subject != original_subject ? 1 : 0;
	evaluator->state[typed_projection] = 2;
	*p_subject = subject;
	return 0;
}

static int compile_materialize_accepted_subject_equations(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->typing || !ctx->metadata ||
		ctx->metadata->typed_publication.sealed) {
		return -1;
	}
	struct operation_publication_subject_evaluator evaluator = { 0 };
	for (uint32_t occurrence_id = (uint32_t)ctx->accepted_typing.occurrence_count;
		occurrence_id < ctx->metadata->typed_publication.count;
		++occurrence_id) {
		struct prototype_typed_publication_projection* publication =
			&ctx->metadata->typed_publication.projections[occurrence_id];
		if (publication->source_occurrence != occurrence_id ||
			publication->typed_projection == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t subject;
		int evaluation_status = operation_evaluate_publication_subject(
			ctx, publication->typed_projection, &evaluator, &subject
		);
		if (evaluation_status != 0) {
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(
					stderr,
					"publication subject evaluation failed occurrence=%u "
					"projection=%u status=%d\n",
					occurrence_id,
					publication->typed_projection,
					evaluation_status
				);
			}
			return -1;
		}
		(void)subject;
	}
	return 0;
}

static int operation_resolve_publication_classifier_rhs_endpoint(
	void* context,
	uint32_t source_occurrence,
	uint32_t* p_term
) {
	struct operation_publication_classifier_evaluator* evaluator = context;
	struct compile_context* ctx = evaluator ? evaluator->ctx : NULL;
	if (!ctx || !ctx->metadata || !p_term || source_occurrence >=
		ctx->metadata->typed_publication.count) {
		return -1;
	}
	const struct prototype_typed_publication_projection* endpoint =
		&ctx->metadata->typed_publication.projections[source_occurrence];
	if (endpoint->source_occurrence != source_occurrence ||
		endpoint->typed_projection == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	return operation_evaluate_publication_subject(
		ctx, endpoint->typed_projection, &evaluator->subjects, p_term
	);
}

static int operation_evaluate_publication_classifier(
	struct compile_context* ctx,
	uint32_t typed_projection,
	struct operation_publication_classifier_evaluator* evaluator,
	uint32_t* p_classifier
) {
	if (!ctx || !evaluator || !p_classifier || typed_projection >=
		PROTOTYPE_TYPING_CONSTRAINT_PROJECTION_CAPACITY) {
		return -1;
	}
	if (evaluator->state[typed_projection] == 2) {
		*p_classifier = evaluator->classifier[typed_projection];
		return 0;
	}
	if (evaluator->state[typed_projection] == 1) {
		return -1;
	}
	struct prototype_typed_publication_projection* publication;
	uint32_t source_occurrence;
	uint32_t original_classifier;
	int answer_status = operation_publication_projection_classifier_answer(
		ctx,
		typed_projection,
		&publication,
		&source_occurrence,
		&original_classifier
	);
	if (answer_status != 0) {
		return answer_status;
	}
	if (publication && source_occurrence < ctx->accepted_typing.occurrence_count) {
		evaluator->classifier[typed_projection] = original_classifier;
		evaluator->state[typed_projection] = 2;
		*p_classifier = original_classifier;
		return 0;
	}
	evaluator->state[typed_projection] = 1;
	uint32_t classifier = original_classifier;
	int classifier_changed = 0;
	uint32_t equation_id;
	if (operation_solver_projected_classifier_equation_id(
			ctx, typed_projection, &equation_id
		) == 0) {
		const struct prototype_typing_constraint* equation =
			&ctx->typing->constraints.constraints[equation_id];
		const struct prototype_typing_constraint_solution* source_solution =
			&ctx->typing->constraints.solutions[equation_id];
		classifier_changed = source_solution->result_term != PROTOTYPE_INVALID_ID &&
			original_classifier != source_solution->result_term;
		if (getenv("A_PROGRAM_PUBLICATION_CLASSIFIER_TRACE")) {
			fprintf(
				stderr,
				"publication-classifier-eval projection=%u occurrence=%u "
				"equation=%u kind=%d operands=%u source-classifier=%u\n",
				typed_projection,
				source_occurrence,
				equation_id,
				equation->kind,
				equation->operand_count,
				original_classifier
			);
			for (uint32_t operand = 0; operand < equation->operand_count; ++operand) {
				const struct prototype_typed_projection* operand_projection =
					prototype_typed_projection_get(
						&ctx->typing->context_projections,
						operation_solver_constraint_operand_projection(ctx, equation, operand)
					);
				fprintf(
					stderr,
					" publication-classifier-operand index=%u projection=%u "
					"occurrence=%u\n",
					operand,
					operation_solver_constraint_operand_projection(ctx, equation, operand),
					operand_projection ? operand_projection->source_occurrence :
						PROTOTYPE_INVALID_ID
				);
			}
		}
		switch (equation->kind) {
			case OPERATION_CONSTRAINT_EQUAL:
			case OPERATION_CONSTRAINT_CONVERTIBLE: {
				uint32_t operand_classifier;
				if (equation->operand_count != 1) {
					return -1;
				}
				int operand_status = operation_evaluate_publication_classifier(
						ctx,
						operation_solver_constraint_operand_projection(ctx, equation, 0),
						evaluator,
						&operand_classifier
					);
				if (operand_status < 0) {
					return -1;
				}
				if (operand_status == 0) {
					classifier = operand_classifier;
				}
				break;
			}
				case OPERATION_CONSTRAINT_PI_EXPECTED:
					if (equation->payload.classifier.goal.pi.role ==
							OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
						uint32_t body_classifier;
						uint32_t domain;
					if (equation->operand_count != 1) {
						return -1;
					}
					int body_status = operation_evaluate_publication_classifier(
							ctx,
							operation_solver_constraint_operand_projection(ctx, equation, 0),
							evaluator,
							&body_classifier
						);
					if (body_status < 0) {
						return -1;
					}
					if (body_status > 0) {
						break;
					}
						uint32_t original_domain;
						int original_domain_status =
							operation_solver_lambda_projection_binder_classifier(
								ctx, typed_projection, &original_domain
							);
						if (original_domain_status < 0) {
							return -1;
						}
						if (getenv("A_PROGRAM_CLASSIFIER_RHS_TRACE")) {
								fprintf(
									stderr,
									"classifier-rhs-eval projection=%u occurrence=%u "
									"root=%u original-domain=%u\n",
									typed_projection,
									source_occurrence,
									equation->classifier_rhs_root,
									original_domain
								);
						}
						int domain_status;
						if (equation->classifier_rhs_root != PROTOTYPE_INVALID_ID) {
							uint32_t source_domain;
							domain_status = operation_evaluate_classifier_rhs(
								ctx,
								equation->classifier_rhs_root,
								&evaluator->rhs,
								&source_domain
							);
							if (domain_status == 0) {
								domain_status = operation_solver_term_in_projection_context(
									ctx, typed_projection, source_domain, &domain
								);
							}
						} else {
							domain_status = original_domain_status;
							domain = original_domain;
						}
						if (domain_status < 0) {
							return -1;
						}
						if (domain_status > 0) {
							break;
						}
					if (operation_classifier_lambda_intro_result(
							ctx,
							equation->source_occurrence,
							domain,
							body_classifier,
							&classifier
						) != 0) {
						return -1;
					}
				} else if (equation->payload.classifier.goal.pi.role ==
						OPERATION_CLASSIFIER_PI_GOAL_ELIM) {
					uint32_t function_classifier;
					uint32_t argument_subject;
					uint32_t argument_classifier;
					uint32_t function_projection;
					uint32_t argument_projection;
					if (equation->operand_count != 2) {
						return -1;
					}
					function_projection =
						operation_solver_constraint_operand_projection(
							ctx, equation, 0
						);
					argument_projection =
						operation_solver_constraint_operand_projection(
							ctx, equation, 1
						);
					int function_status = operation_evaluate_publication_classifier(
							ctx,
							function_projection,
							evaluator,
							&function_classifier
						);
					int argument_subject_status =
						operation_evaluate_publication_subject(
							ctx,
							argument_projection,
							&evaluator->subjects,
							&argument_subject
						);
					int argument_classifier_status =
						operation_evaluate_publication_classifier(
							ctx,
							argument_projection,
							evaluator,
							&argument_classifier
						);
					if (function_status < 0 || argument_subject_status < 0 ||
						argument_classifier_status < 0) {
						return -1;
					}
					if (function_status > 0 || argument_subject_status > 0 ||
						argument_classifier_status > 0) {
						break;
					}
					/* The equation answer is authoritative while its operands retain
					 * their solved identities. In particular, implicit surface coercions
					 * need not be recoverable from the erased argument subject. */
					if (!evaluator->changed[function_projection] &&
						!evaluator->changed[argument_projection] &&
						!evaluator->subjects.changed[argument_projection]) {
						break;
					}
					uint32_t application_context;
					uint32_t application_substitution;
					int context_status =
						operation_typed_projection_materialized_context(
							ctx,
							typed_projection,
							&application_context,
							&application_substitution
						);
					(void)application_substitution;
					if (context_status < 0) {
						return -1;
					}
					if (context_status > 0) {
						break;
					}
					/* Publication is a checked projection of the equation answer. Use
					 * the same context-aware APP rule as the Solver and kernel replay;
					 * unchecked graph substitution would create a second classifier
					 * authority for dependent domains and solved effect rows. */
					if (prototype_judgement_app_elim_classifier_in_context(
							&ctx->metadata->contexts,
							&ctx->metadata->substitutions,
							ctx->terms,
							ctx->type_declarations,
							application_context,
							function_classifier,
							argument_subject,
							argument_classifier,
							&classifier
						) != 0) {
						if (getenv("A_PROGRAM_PUBLICATION_CLASSIFIER_TRACE")) {
							fprintf(stderr, "publication PI elimination reconstruction failed function: ");
							prototype_type_term_print_debug(
								stderr, ctx->symbols, ctx->intrinsic_environment,
								ctx->type_declarations, ctx->terms, function_classifier
							);
							fprintf(stderr, "\npublication PI elimination argument: ");
							prototype_type_term_print_debug(
								stderr, ctx->symbols, ctx->intrinsic_environment,
								ctx->type_declarations, ctx->terms, argument_subject
							);
							fputc('\n', stderr);
						}
						return -1;
					}
				} else {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CBPV_BOUNDARY: {
				uint32_t child_classifier;
				uint32_t publication_subject;
				if (equation->operand_count != 1) {
					return -1;
				}
				int child_status = operation_evaluate_publication_classifier(
						ctx,
						operation_solver_constraint_operand_projection(ctx, equation, 0),
						evaluator,
						&child_classifier
					);
				if (child_status < 0) {
					return -1;
				}
				if (child_status > 0) {
					break;
				}
				int subject_status = operation_evaluate_publication_subject(
					ctx,
					typed_projection,
					&evaluator->subjects,
					&publication_subject
				);
				if (subject_status != 0) {
					break;
				}
				if (prototype_judgement_cbpv_boundary_classifier(
						ctx->terms,
						ctx->type_declarations,
						publication_subject,
						child_classifier,
						&classifier
					) != 0) {
					return -1;
				}
				break;
			}
			case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT: {
				if ((equation->operand_count != 3 && equation->operand_count != 4) ||
					source_occurrence >=
						ctx->metadata->typed_occurrences.occurrence_count) {
					return -1;
				}
				const struct prototype_typed_occurrence* fold_occurrence =
					&ctx->metadata->typed_occurrences.occurrences[
						source_occurrence
					];
				uint32_t input_classifier;
				uint32_t continuation_classifier;
				int input_status = operation_evaluate_publication_classifier(
					ctx,
					operation_solver_constraint_operand_projection(ctx, equation, 0),
					evaluator,
					&input_classifier
				);
				int continuation_status =
					operation_evaluate_publication_classifier(
						ctx,
						operation_solver_constraint_operand_projection(ctx, equation, 1),
						evaluator,
						&continuation_classifier
					);
				if (input_status < 0 || continuation_status < 0) {
					return -1;
				}
				if (input_status > 0 || continuation_status > 0 ||
					fold_occurrence->fold_clause_count != 0) {
					break;
				}
				uint32_t fold_subject;
				int subject_status = operation_evaluate_publication_subject(
					ctx,
					typed_projection,
					&evaluator->subjects,
					&fold_subject
				);
				if (subject_status != 0 || fold_subject >= ctx->terms->term_count ||
					ctx->terms->terms[fold_subject].tag !=
						PROTOTYPE_TERM_COMPUTATION_FOLD ||
					ctx->terms->terms[fold_subject].as.computation_fold.clause_count !=
						0) {
					break;
				}
				if (prototype_judgement_computation_fold_result_classifier(
						ctx->terms,
						ctx->type_declarations,
						ctx->terms->terms[fold_subject].as.computation_fold.computation,
						input_classifier,
						continuation_classifier,
						&classifier
					) != 0) {
					return -1;
				}
				break;
			}
			case OPERATION_CONSTRAINT_TERMINATION_WITNESS: {
				uint32_t computation_subject;
				if (equation->operand_count != 1) {
					return -1;
				}
				int computation_status =
					operation_evaluate_publication_subject(
						ctx,
						operation_solver_constraint_operand_projection(ctx, equation, 0),
						&evaluator->subjects,
						&computation_subject
					);
				if (computation_status != 0 || prototype_term_terminates_type(
						ctx->terms, computation_subject, &classifier
					) != 0) {
					return -1;
				}
				uint32_t source_computation;
				if (prototype_term_terminates_type_info(
						ctx->terms, original_classifier, &source_computation
					) == 0 && source_computation == computation_subject) {
					classifier = original_classifier;
				}
				break;
			}
			default:
				break;
		}
	}
	if (publication && classifier != original_classifier &&
		prototype_typed_publication_view_project_classifier(
			&ctx->metadata->typed_publication,
			source_occurrence,
			typed_projection,
			original_classifier,
			classifier
		) != 0) {
		return -1;
	}
	classifier_changed = classifier_changed || classifier != original_classifier;
	evaluator->classifier[typed_projection] = classifier;
	evaluator->changed[typed_projection] = classifier_changed ? 1 : 0;
	evaluator->state[typed_projection] = 2;
	if (getenv("A_PROGRAM_PUBLICATION_CLASSIFIER_TRACE")) {
		fprintf(
			stderr,
			"publication-classifier-result projection=%u occurrence=%u "
			"classifier=%u changed=%d\n",
			typed_projection,
			source_occurrence,
			classifier,
			classifier_changed
		);
	}
	*p_classifier = classifier;
	return 0;
}

static int compile_materialize_accepted_classifier_equations(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->typing || !ctx->metadata ||
		ctx->metadata->typed_publication.sealed) {
		return -1;
	}
	struct operation_publication_classifier_evaluator evaluator = { 0 };
	evaluator.ctx = ctx;
	evaluator.rhs.endpoint_context = &evaluator;
	evaluator.rhs.resolve_endpoint =
		operation_resolve_publication_classifier_rhs_endpoint;
	for (uint32_t occurrence_id = (uint32_t)ctx->accepted_typing.occurrence_count;
		occurrence_id < ctx->metadata->typed_publication.count;
		++occurrence_id) {
		struct prototype_typed_publication_projection* publication =
			&ctx->metadata->typed_publication.projections[occurrence_id];
		if (publication->source_occurrence != occurrence_id ||
			publication->typed_projection == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t classifier;
		int evaluation_status = operation_evaluate_publication_classifier(
				ctx, publication->typed_projection, &evaluator, &classifier
			);
		if (evaluation_status != 0) {
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(
					stderr,
					"publication classifier evaluation failed occurrence=%u "
					"projection=%u status=%d\n",
					occurrence_id,
					publication->typed_projection,
					evaluation_status
				);
			}
			return -1;
		}
		(void)classifier;
	}
	return 0;
}

static int operation_solver_materialize_publication_judgements(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata || !ctx->typing ||
		!ctx->metadata->typed_publication.sealed) {
		return -1;
	}
	const struct prototype_typed_publication_view* saved_view =
		ctx->proof_projection_view;
	ctx->proof_projection_view = &ctx->metadata->typed_publication;
	int result = 0;
	for (uint32_t occurrence_id = (uint32_t)ctx->accepted_typing.occurrence_count;
		occurrence_id < ctx->metadata->typed_publication.count;
		++occurrence_id) {
		const struct prototype_typed_publication_projection* publication =
			&ctx->metadata->typed_publication.projections[occurrence_id];
		if (publication->source_occurrence != occurrence_id ||
			publication->typed_projection == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t source_context;
		uint32_t source_substitution;
		uint32_t source_subject;
		uint32_t source_classifier;
		int source_status = operation_typed_projection_materialized_typing_answer(
			ctx,
			publication->typed_projection,
			&source_context,
			&source_substitution,
			&source_subject,
			&source_classifier
		);
		if (source_status != 0) {
			result = source_status < 0 ? -1 : 1;
			break;
		}
		if (source_subject == publication->subject &&
			source_classifier == publication->classifier) {
			continue;
		}
		int status = operation_solver_reify_projected_core_proof(
			ctx,
			publication->typed_projection,
			publication->subject,
			0
		);
		if (status != 0) {
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				fprintf(stderr,
					"publication proof failed occurrence=%u projection=%u "
					"status=%d source-status=%d subject=%u/%u classifier=%u/%u\n",
					occurrence_id,
					publication->typed_projection,
					status,
					source_status,
					source_subject,
					publication->subject,
					source_classifier,
					publication->classifier
				);
			}
			result = status < 0 ? -1 : 1;
			break;
		}
	}
	ctx->proof_projection_view = saved_view;
	return result;
}

static int operation_solver_validate_solution_projection(
	const struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	uint32_t mutable_occurrence_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	for (uint32_t i = mutable_occurrence_start;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		int impossible_branch = operation_solver_occurrence_is_in_impossible_branch(
			ctx, i
		);
		if (impossible_branch < 0) {
			return -1;
		}
		if (impossible_branch > 0) {
			continue;
		}
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		uint32_t typed_projection = operation_solver_publication_projection(ctx, i);
		uint32_t classifier = operation_solver_classifier_for_projection(
			ctx, typed_projection
		);
		if (classifier != operation->classifier) {
			fprintf(stderr,
				"occurrence freeze projection mismatch occurrence=%u answer=%u "
				"graph=%u\n",
				i, classifier, operation->classifier
			);
			return -1;
		}
		uint32_t binder_classifier = operation_solver_binder_classifier(ctx, i);
		if (binder_classifier != operation->binder_classifier) {
			fprintf(stderr,
				"occurrence freeze binder mismatch occurrence=%u equation=%u graph=%u\n",
				i, binder_classifier, operation->binder_classifier
			);
			return -1;
		}
		if (classifier == PROTOTYPE_INVALID_ID) {
			uint32_t obligation_id = operation->classifier_verification_obligation;
			if (operation->classifier_status !=
					PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_RESIDUAL_VERIFICATION ||
				obligation_id >= prototype_verification_db_count(
					&ctx->metadata->verification
				)) {
				fprintf(stderr,
					"occurrence freeze residual mismatch occurrence=%u status=%d "
					"obligation=%u\n",
					i, operation->classifier_status, obligation_id
				);
				return -1;
			}
			continue;
		}
		struct prototype_classifier_view view;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				classifier,
				&view
			) != 0) {
			fprintf(stderr,
				"occurrence freeze classifier view failed occurrence=%u classifier=%u\n",
				i, classifier
			);
			return -1;
		}
		if (view.category == PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_COMPUTATION_KIND_RETURNING) {
			uint32_t answer_id;
			int answer_status = operation_effect_solution_equation_for_occurrence(
				ctx, i, &answer_id
			);
			const struct prototype_typing_constraint* answer =
				answer_status == 0 ?
					&ctx->typing->constraints.constraints[answer_id] : NULL;
			const struct prototype_typing_constraint_solution* effect_solution =
				answer ? &ctx->typing->constraints.solutions[answer_id] : NULL;
			if (answer_status < 0 || (answer &&
				effect_solution->result_term != PROTOTYPE_INVALID_ID &&
				effect_solution->result_term != view.effect_row)) {
				fprintf(stderr,
					"occurrence freeze effect mismatch occurrence=%u tag=%d projection=%u "
					"answer=%u placeholder=%u materialized=%u classifier-row=%u\n",
					i, operation->tag, typed_projection,
					answer ? answer_id : PROTOTYPE_INVALID_ID,
					answer ? answer->payload.effect.result_row :
						PROTOTYPE_INVALID_ID,
					effect_solution ? effect_solution->result_term :
						PROTOTYPE_INVALID_ID,
					view.effect_row
				);
				return -1;
			}
		}
	}
	return 0;
}

/* Materialize one occurrence from already frozen classifier authority. A
 * positive result is a missing-premise residual and is the only state that may
 * be queued again. */
static int operation_solver_materialize_occurrence_judgement(
	struct compile_context* ctx,
	uint32_t operation_id
) {
	if (!ctx || !ctx->metadata ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	int closed_evidence =
		operation_solver_occurrence_has_closed_evidence_boundary(
		ctx, operation_id
	);
	if (closed_evidence < 0) {
		return -1;
	}
	if (!closed_evidence) {
		return 0;
	}
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, operation->context_id
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, operation_id
	);
	uint32_t classifier = operation->classifier;
	int full_type_instance = operation_solver_type_instance_is_fully_applied(
		ctx, operation->core_term
	);
	int declaration_head = operation_solver_type_instance_is_declaration_head(
		ctx, operation->core_term
	);
	if (full_type_instance < 0 || declaration_head < 0) {
		return -1;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_FORCE ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REQUEST ||
		(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
		 operation->core_term < ctx->terms->term_count &&
		 ctx->terms->terms[operation->core_term].tag == PROTOTYPE_TERM_TYPE_VIEW &&
			 (full_type_instance || declaration_head) &&
		 prototype_context_get(
			&ctx->metadata->contexts, operation->context_id
		 ) && prototype_context_get(
			&ctx->metadata->contexts, operation->context_id
		 )->parent == PROTOTYPE_INVALID_ID)) {
		int reify_status = operation_solver_reify_core_proof(
			ctx, operation_id, operation->core_term, 0
		);
		if (reify_status < 0) {
			fprintf(
				stderr,
				"judgement materialization: reify failed operation=%u tag=%d\n",
				operation_id, operation->tag
			);
			fprintf(stderr, "judgement materialization term: ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				operation->core_term
			);
			fprintf(stderr, "\njudgement materialization classifier: ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms, classifier
			);
			fprintf(stderr, "\n");
			if (ctx->metadata->compile_diagnostic_count == 0) {
				(void)add_compile_diagnostic(
					ctx,
					PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
					PROTOTYPE_COMPILE_DIAGNOSTIC_UNSOLVED_CLASSIFIER,
					operation->source_ast,
					operation_id,
					PROTOTYPE_INVALID_ID,
					classifier,
					PROTOTYPE_INVALID_ID,
					compile_source_span(ctx, operation->source_ast)
				);
			}
		}
		return reify_status;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		if (operation_solver_materialize_induction_hypothesis_judgement(
				ctx, operation_id
			) != 0) {
			fprintf(
				stderr,
				"judgement materialization: IH failed operation=%u classifier=%u\n",
				operation_id, classifier
			);
			return -1;
		}
		return 0;
	}
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_ATOM ||
		operation->core_term >= ctx->terms->term_count) {
		return 0;
	}
	uint32_t atom_context = operation_occurrence_materialized_context(
		ctx, operation_id
	);
	if (atom_context == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, atom_context
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, PROTOTYPE_INVALID_ID
	);
	if (full_type_instance || declaration_head) {
		return prototype_judgement_delta_record_type_formation(
			&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
			operation->core_term, classifier
		);
	}
	const struct prototype_term* term = &ctx->terms->terms[operation->core_term];
	if (term->tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta,
			prototype_context_empty(&ctx->metadata->contexts)
		);
		return prototype_judgement_delta_record_pure_primitive_type(
			&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
			operation->core_term, classifier
		);
	}
	if (term->tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta,
			prototype_context_empty(&ctx->metadata->contexts)
		);
		return prototype_judgement_delta_record_effect_operation_type(
			&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
			operation->core_term, classifier
		);
	}
	if (term->tag == PROTOTYPE_TERM_TEXT_LITERAL) {
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta,
			prototype_context_empty(&ctx->metadata->contexts)
		);
		return prototype_judgement_delta_record_text_literal(
			&ctx->judgement_delta, ctx->terms, operation->core_term, classifier
		);
	}
	if (term->tag == PROTOTYPE_TERM_INT_LITERAL) {
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta,
			prototype_context_empty(&ctx->metadata->contexts)
		);
		return prototype_judgement_delta_record_int_literal(
			&ctx->judgement_delta, ctx->terms, ctx->intrinsic_environment,
			operation->core_term, classifier
		);
	}
	return 0;
}

static int operation_solver_freeze_occurrences(
	struct compile_context* ctx,
	uint32_t first_occurrence
) {
	if (!ctx || !ctx->metadata || first_occurrence >
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	for (uint32_t i = first_occurrence;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		int impossible_branch = operation_solver_occurrence_is_in_impossible_branch(
			ctx, i
		);
		if (impossible_branch < 0) {
			return -1;
		}
		if (impossible_branch > 0) {
			continue;
		}
		uint32_t typed_projection = operation_solver_publication_projection(ctx, i);
		uint32_t classifier;
		int classifier_status = operation_solver_freeze_projection_answer(
			ctx, typed_projection, &classifier
		);
		struct prototype_typed_occurrence* frozen =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (classifier_status < 0) {
			return -1;
		}
		if (classifier_status > 0 || classifier == PROTOTYPE_INVALID_ID) {
			uint32_t obligation_id = PROTOTYPE_INVALID_ID;
			int obligation_status = frozen->tag ==
					PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ?
				prototype_verification_db_find_occurrence(
					&ctx->metadata->verification,
					PROTOTYPE_VERIFICATION_OBLIGATION_COMPUTATION_FOLD_RESULT,
					i,
					&obligation_id
				) : 1;
			if (obligation_status == 0 && obligation_id <
					prototype_verification_db_count(&ctx->metadata->verification)) {
				frozen->classifier = PROTOTYPE_INVALID_ID;
				frozen->classifier_status =
					PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_RESIDUAL_VERIFICATION;
				frozen->classifier_verification_obligation = obligation_id;
				continue;
			}
			fprintf(stderr,
				"judgement materialization: unsolved classifier occurrence=%u tag=%d "
				"seed=%u core=%u core-tag=%u wrapper=%u source_ast=%u context=%u "
				"binding=%u referenced_ast_binder=%u\n",
				i,
				frozen->tag,
				ctx->typing->seed.classifier_seeds[i].classifier,
				frozen->core_term,
				frozen->core_term < ctx->terms->term_count ?
					ctx->terms->terms[frozen->core_term].tag : 0,
				frozen->wrapped_occurrence,
				frozen->source_ast,
				frozen->context_id,
				frozen->binding_id,
				frozen->referenced_ast_binder_id
			);
			return -1;
		}
		if (prototype_type_projection_classifier_graph(
				ctx->terms,
				ctx->type_declarations,
				classifier,
				&classifier
			) != 0) {
			return -1;
		}
		frozen->classifier = classifier;
		frozen->binder_classifier = operation_solver_binder_classifier(ctx, i);
		if (frozen->binder_classifier != PROTOTYPE_INVALID_ID &&
			prototype_type_projection_classifier_graph(
				ctx->terms,
				ctx->type_declarations,
				frozen->binder_classifier,
				&frozen->binder_classifier
			) != 0) {
			return -1;
		}
		if (frozen->source_classifier != PROTOTYPE_INVALID_ID &&
			prototype_type_projection_classifier_graph(
				ctx->terms,
				ctx->type_declarations,
				frozen->source_classifier,
				&frozen->source_classifier
			) != 0) {
			return -1;
		}
		if (frozen->match_motive != PROTOTYPE_INVALID_ID &&
			prototype_type_projection_classifier_graph(
				ctx->terms,
				ctx->type_declarations,
				frozen->match_motive,
				&frozen->match_motive
			) != 0) {
			return -1;
		}
		uint32_t effect_answer_id;
		int effect_answer_status = operation_effect_solution_equation_for_occurrence(
			ctx, i, &effect_answer_id
		);
		if (effect_answer_status < 0) {
			return -1;
		}
		if (effect_answer_status == 0 &&
			ctx->typing->constraints.solutions[effect_answer_id].result_term !=
				PROTOTYPE_INVALID_ID) {
			const struct prototype_typing_constraint* effect_answer =
				&ctx->typing->constraints.constraints[effect_answer_id];
			const struct prototype_typing_constraint_solution* effect_solution =
				&ctx->typing->constraints.solutions[effect_answer_id];
			if (frozen->binder_classifier != PROTOTYPE_INVALID_ID) {
				uint32_t binding_id = ctx->terms->terms[
					effect_answer->payload.effect.result_row
				].as.effect_row_var.binding_id;
				if (prototype_term_graph_substitute_bound_var(
						ctx->terms, frozen->binder_classifier,
						binding_id,
						effect_solution->result_term,
						&frozen->binder_classifier
					) != 0) {
					return -1;
				}
			}
		}
		frozen->classifier_status =
			PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED;
		frozen->classifier_verification_obligation = PROTOTYPE_INVALID_ID;
	}
	return operation_solver_validate_solution_projection(ctx);
}

static int operation_solver_complete_checked_classifiers(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata || ctx->typing->constraints.topology_sealed) {
		return -1;
	}
	if (operation_solver_materialize_sequence_binder_context_views(ctx) != 0) {
		fprintf(stderr,
			"semantic completion: sequence binder Context view failed\n");
		return -1;
	}
	memset(
		ctx->typing->reindex.entries,
		0,
		sizeof(ctx->typing->reindex.entries)
	);
	/* Classifier constraints, including constructor domains, are closed before
	 * this phase. Freeze only projects those answers into the readback graph. */
	ctx->evidence_workspace.materializing_checked_classifiers = 1;
	uint32_t mutable_occurrence_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	if (operation_solver_freeze_occurrences(
			ctx, mutable_occurrence_start
		) != 0) {
		ctx->evidence_workspace.materializing_checked_classifiers = 0;
		fprintf(stderr, "semantic completion: occurrence freeze failed\n");
		return -1;
	}
	ctx->evidence_workspace.materializing_checked_classifiers = 0;
	return 0;
}

static int operation_solver_materialize_judgements(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	uint64_t proof_credit = ctx->metadata->typed_occurrences.occurrence_count;
	int proof_effort = compile_effort_charge(
		ctx->metadata,
		ctx->effort_atomic,
		PROTOTYPE_EFFORT_PHASE_PROOF,
		proof_credit == 0 ? 1 : proof_credit
	);
	if (proof_effort != 0) return proof_effort;
	int initial_pass = !ctx->evidence_workspace.materialization_initialized;
	ctx->evidence_workspace.materialization_substitution_pending = 0;
	ctx->evidence_workspace.materialization_initialized = 1;
	ctx->metadata->proof_materialization_pass_count++;
	if (initial_pass) {
		ctx->metadata->proof_materialization_full_scan_count++;
		ctx->metadata->full_scan_count[PROTOTYPE_COMPILE_SCAN_OCCURRENCE]++;
		ctx->metadata->full_scan_visit_count[PROTOTYPE_COMPILE_SCAN_OCCURRENCE] +=
			ctx->metadata->typed_occurrences.occurrence_count;
		memset(
			ctx->typing->reindex.entries,
			0,
			sizeof(ctx->typing->reindex.entries)
		);
		memset(
			ctx->metadata->substitutions.reindex_cache,
			0,
			sizeof(ctx->metadata->substitutions.reindex_cache)
		);
		uint32_t mutable_occurrence_start = (uint32_t)
			prototype_typed_occurrence_graph_transaction_start(
				&ctx->metadata->typed_occurrences
			);
		if (materialize_binder_inputs(ctx) != 0 ||
			materialize_declaration_inputs(ctx) != 0) {
			fprintf(stderr, "judgement materialization: pending input facts failed\n");
			return -1;
		}
		if (operation_solver_materialize_match_pattern_assumptions(ctx) != 0) {
			fprintf(stderr, "judgement materialization: match assumptions failed\n");
			return -1;
		}
		for (uint32_t i = mutable_occurrence_start;
			i < ctx->metadata->typed_occurrences.occurrence_count;
			++i) {
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
		if (operation_solver_materialize_publication_substitutions(
				ctx, mutable_occurrence_start, 0
			) != 0) {
			fprintf(
				stderr,
				"judgement materialization: publication substitution evidence failed\n"
			);
			return -1;
		}
	}
	uint32_t current_queue[4096];
	uint32_t next_queue[4096];
	uint32_t current_count = 0;
	if (initial_pass) {
		uint32_t mutable_occurrence_start = (uint32_t)
			prototype_typed_occurrence_graph_transaction_start(
				&ctx->metadata->typed_occurrences
			);
		/* Function-graph generation can commit declarations and named helper roots
		 * in one graph transaction, then reference or publish them in the next.
		 * Revisit those stable roots so their derivation DAG is materialized before
		 * the later transaction is frozen; accepted evidence makes this idempotent. */
		for (uint32_t i = 0; i < mutable_occurrence_start; ++i) {
			const struct prototype_typed_occurrence* operation =
				&ctx->metadata->typed_occurrences.occurrences[i];
			const struct prototype_context* context = prototype_context_get(
				&ctx->metadata->contexts, operation->context_id
			);
			int stable_type_root = operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
				operation->core_term < ctx->terms->term_count &&
				ctx->terms->terms[operation->core_term].tag ==
					PROTOTYPE_TERM_TYPE_VIEW &&
				(operation_solver_type_instance_is_fully_applied(
					ctx, operation->core_term
				) == 1 || operation_solver_type_instance_is_declaration_head(
					ctx, operation->core_term
				) == 1);
			if (context && context->parent == PROTOTYPE_INVALID_ID &&
				(stable_type_root || operation->source_symbol_id >= 0)) {
				if (current_count >= 4096) {
					return -1;
				}
				current_queue[current_count++] = i;
			}
		}
		for (uint32_t i = mutable_occurrence_start;
			i < ctx->metadata->typed_occurrences.occurrence_count;
			++i) {
			if (current_count >= 4096) {
				return -1;
			}
			current_queue[current_count++] = i;
		}
	} else {
		current_count = ctx->evidence_workspace.materialization_pending_count;
		ctx->metadata->evidence_consumer_retry_count += current_count;
		memcpy(
			current_queue,
			ctx->evidence_workspace.materialization_pending_ids,
			current_count * sizeof(current_queue[0])
		);
	}
	ctx->evidence_workspace.materialization_pending_count = 0;
	while (current_count != 0) {
		ctx->metadata->proof_materialization_round_count++;
		size_t before_relation_count = ctx->judgement_delta.proposition_count;
		uint32_t next_count = 0;
		for (uint32_t queue_index = 0;
			queue_index < current_count;
			++queue_index) {
			uint32_t operation_id = current_queue[queue_index];
			ctx->metadata->proof_materialization_occurrence_visit_count++;
			int status = operation_solver_materialize_occurrence_judgement(
				ctx, operation_id
			);
			if (getenv("A_PROGRAM_PROOF_TRACE")) {
				const struct prototype_typed_occurrence* traced =
					&ctx->metadata->typed_occurrences.occurrences[operation_id];
				fprintf(
					stderr,
					"A_PROGRAM_PROOF_TRACE operation=%u tag=%d status=%d context=%u "
					"core=%u classifier=%u\n",
					operation_id, traced->tag, status, traced->context_id,
					traced->core_term, traced->classifier
				);
			}
			if (status < 0) {
				return -1;
			}
			if (status > 0) {
				next_queue[next_count++] = operation_id;
			}
		}
		/* Computation constraints are solved before this phase. Their replayable
		 * rules are retried only after this round produced new premise evidence. */
		if (operation_solver_prepare_computation_constraint_inputs(
				ctx,
				ctx->proof_computation_constraint_inputs,
				ctx->proof_computation_constraint_input_capacity
			) != 0) {
			return -1;
		}
		if (
			prototype_judgement_delta_solve_recorded_computation_requests(
				&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
				ctx->proof_computation_constraint_inputs,
				ctx->proof_computation_constraint_input_capacity,
				ctx->proof_computation_constraint_results,
				ctx->proof_computation_constraint_result_capacity
			) != 0 ||
			prototype_judgement_delta_solve_recorded_computation_constraints(
				&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
				ctx->proof_computation_constraint_inputs,
				ctx->proof_computation_constraint_input_capacity,
				ctx->proof_computation_constraint_results,
				ctx->proof_computation_constraint_result_capacity
			) != 0 || operation_solver_validate_solution_projection(ctx) != 0) {
			return -1;
		}
		if (next_count == 0 ||
			ctx->judgement_delta.proposition_count == before_relation_count) {
			ctx->evidence_workspace.materialization_pending_count = next_count;
			memcpy(
				ctx->evidence_workspace.materialization_pending_ids,
				next_queue,
				next_count * sizeof(next_queue[0])
			);
			break;
		}
		memcpy(current_queue, next_queue, next_count * sizeof(next_queue[0]));
		current_count = next_count;
	}
	/* A rule may create a telescope substitution that is not attached to a
	 * source occurrence as its context action. Its accepted derivation still
	 * publishes that substitution, so materialize the extension evidence before
	 * committing the proof DAG. */
	for (uint32_t i = 0;
		i < ctx->judgement_delta.derivation_candidate_count;
		++i) {
		const struct prototype_judgement_derivation_candidate* candidate =
			&ctx->judgement_delta.derivation_candidates[i];
		if (candidate->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			candidate->semantic_action_id != PROTOTYPE_INVALID_ID &&
			operation_solver_materialize_refinement_binding_evidence_at_depth(
				ctx, candidate->semantic_action_id, 0
			) != 0) {
			fprintf(
				stderr,
				"judgement materialization: substitution action evidence failed "
				"candidate=%u substitution=%u\n",
				i,
				candidate->semantic_action_id
			);
			return -1;
		}
		for (uint32_t premise = 0; premise < candidate->premise_count; ++premise) {
			const struct prototype_judgement_candidate_premise* edge =
				&candidate->premises[premise];
			if (edge->semantic_action_kind !=
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
				edge->semantic_action_id == PROTOTYPE_INVALID_ID) {
				continue;
			}
			if (operation_solver_materialize_refinement_binding_evidence_at_depth(
					ctx, edge->semantic_action_id, 0
				) != 0) {
				fprintf(
					stderr,
					"judgement materialization: premise substitution evidence failed "
					"candidate=%u premise=%u substitution=%u\n",
					i,
					premise,
					edge->semantic_action_id
				);
				return -1;
			}
		}
	}
	unsigned char* materialized_substitution_roots = calloc(
		ctx->metadata->substitutions.substitution_count == 0 ? 1 :
			ctx->metadata->substitutions.substitution_count,
		sizeof(*materialized_substitution_roots)
	);
	if (!materialized_substitution_roots) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(ctx->judgement, i);
		if (!derivation) {
			free(materialized_substitution_roots);
			return -1;
		}
		uint32_t roots[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES + 1];
		uint32_t root_count = 0;
		if (derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			derivation->semantic_action_id != PROTOTYPE_INVALID_ID) {
			roots[root_count++] = derivation->semantic_action_id;
		}
		for (uint32_t premise = 0; premise < derivation->premise_count; ++premise) {
			const struct prototype_judgement_premise_edge* edge =
				&derivation->premises[premise];
			if (edge->semantic_action_kind ==
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
				edge->semantic_action_id != PROTOTYPE_INVALID_ID) {
				roots[root_count++] = edge->semantic_action_id;
			}
		}
		for (uint32_t root_index = 0; root_index < root_count; ++root_index) {
			uint32_t root = roots[root_index];
			if (root >= ctx->metadata->substitutions.substitution_count) {
				free(materialized_substitution_roots);
				return -1;
			}
			if (materialized_substitution_roots[root]) {
				continue;
			}
			materialized_substitution_roots[root] = 1;
			if (operation_solver_materialize_refinement_binding_evidence_with_mode(
					ctx, root, 0, 1
				) != 0) {
				fprintf(
					stderr,
					"judgement materialization: accepted substitution evidence failed "
					"derivation=%u substitution=%u\n",
					i,
					root
				);
				free(materialized_substitution_roots);
				return -1;
			}
		}
	}
	uint32_t mutable_occurrence_start = (uint32_t)
		prototype_typed_occurrence_graph_transaction_start(
			&ctx->metadata->typed_occurrences
		);
	if (operation_solver_materialize_publication_substitutions(
			ctx, mutable_occurrence_start, 1
		) != 0) {
		free(materialized_substitution_roots);
		fprintf(
			stderr,
			"judgement materialization: accepted publication substitution "
			"evidence failed\n"
		);
		return -1;
	}
	free(materialized_substitution_roots);
	if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	/* Match motive results are a construction-time lookup index. The replayable
	 * derivations above are the persistent proof state. */
	ctx->judgement_delta.match_motive_result_count = 0;
	return 0;
}
