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

static int operation_solver_reify_core_proof(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t core_term,
	uint32_t depth
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
			fprintf(stderr,
				"match materialization invalid refinement operation=%u case=%u\n",
				operation_id, case_index);
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
		int reify_status = operation_solver_reify_core_proof(
			ctx, branch_operation, body->core_term, 0
		);
		if (reify_status != 0) {
			return reify_status;
		}
		int evidence_status = operation_solver_select_evidence_in_context(
			ctx,
			branch_operation,
			body->context_id,
			body->core_term,
			body->classifier,
			&branch_evidence[case_index]
		);
		if (evidence_status != 0) {
			fprintf(stderr,
				"match materialization branch evidence failed operation=%u case=%u branch=%u status=%d context=%u term=%u classifier=%u\n",
				operation_id, case_index, branch_operation, evidence_status,
				body->context_id, body->core_term, body->classifier);
			return evidence_status;
		}
		if (branch_evidence[case_index].occurrence_id != branch_operation) {
			fprintf(stderr,
				"match branch evidence owner differs operation=%u branch=%u branch-tag=%d evidence-occurrence=%u authority=%d:%u\n",
				operation_id, branch_operation, body->tag,
				branch_evidence[case_index].occurrence_id,
				branch_evidence[case_index].authority_kind,
				branch_evidence[case_index].authority_id);
		}
	}
	/* Branch proof reification changes the active Delta authority. The motive
	 * conclusion belongs to the Match occurrence, not to the final branch or IH
	 * visited above. */
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, operation->context_id
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, operation_id
	);
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
	if (record_status != 0) {
		const struct prototype_term* classifier_term =
			operation->classifier < ctx->terms->term_count ?
			&ctx->terms->terms[operation->classifier] : NULL;
		uint32_t classifier_head = operation->classifier;
		while (classifier_head < ctx->terms->term_count &&
			ctx->terms->terms[classifier_head].tag == PROTOTYPE_TERM_APP) {
			classifier_head = ctx->terms->terms[classifier_head].as.app.function;
		}
		fprintf(stderr,
			"match materialization motive record failed operation=%u status=%d classifier=%u tag=%d scrutinee=%u app-argument=%u head=%u head-tag=%d\n",
			operation_id, record_status, operation->classifier,
			classifier_term ? classifier_term->tag : -1,
			ctx->terms->terms[operation->core_term].as.match.scrutinee,
			classifier_term && classifier_term->tag == PROTOTYPE_TERM_APP ?
				classifier_term->as.app.argument : PROTOTYPE_INVALID_ID,
			classifier_head,
			classifier_head < ctx->terms->term_count ?
				ctx->terms->terms[classifier_head].tag : -1);
	}
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
		const struct prototype_context* binding_context = prototype_context_get(
			&ctx->metadata->contexts, binding_context_id
		);
		if (!binding_context || prototype_context_classifier_term(binding_context) ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		classifier = prototype_context_classifier_term(binding_context);
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
			prototype_judgement_constructor_spine_expected_domains(
				ctx->terms, ctx->type_declarations, &ctx->metadata->contexts,
				&ctx->metadata->substitutions, context_id, subject,
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
			fprintf(stderr,
				"refinement evidence compose failed substitution=%u first=%u:%d "
				"second=%u:%d depth=%u first-kind=%d first-source=%u "
				"first-target=%u first-prefix=%u first-term=%u first-classifier=%u\n",
				substitution_id, substitution->first, first_status,
				substitution->second, second_status, depth,
				first ? first->kind : -1,
				first ? first->source_context : PROTOTYPE_INVALID_ID,
				first ? first->target_context : PROTOTYPE_INVALID_ID,
				first ? first->first : PROTOTYPE_INVALID_ID,
				first ? first->term : PROTOTYPE_INVALID_ID,
				first ? first->term_classifier : PROTOTYPE_INVALID_ID);
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
		fprintf(stderr,
			"refinement evidence prefix failed substitution=%u first=%u depth=%u\n",
			substitution_id, substitution->first, depth);
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
	if (existing_status < 0) {
		fprintf(stderr,
			"refinement evidence selection failed substitution=%u term=%u "
			"classifier=%u context=%u status=%d\n",
			substitution_id, substitution->term, substitution->term_classifier,
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
			proposition->classifier == substitution->term_classifier) {
			for (uint32_t derivation_id = 0;
				derivation_id < ctx->judgement->derivation_count;
				++derivation_id) {
				const struct prototype_judgement_derivation* derivation =
					prototype_judgement_derivation_get(
						ctx->judgement, derivation_id
					);
				if (derivation && derivation->conclusion_claim_id == claim_id) {
					return 0;
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
				substitution->term_classifier
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
		if (operation_evidence.classifier != substitution->term_classifier) {
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
					substitution->term_classifier,
					substitution->term
				) != 0) {
				return -1;
			}
		}
		if (require_accepted) {
			ctx->proof_materialization_substitution_pending = 1;
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
			substitution->term_classifier,
			depth + 1,
			&structural_evidence
		);
		if (structural_status == 0 && require_accepted) {
			ctx->proof_materialization_substitution_pending = 1;
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
				substitution->term_classifier,
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
	if (!binding_context ||
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			prototype_context_classifier_term(binding_context),
				substitution->term_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		fprintf(stderr,
			"refinement evidence classifier mismatch substitution=%u binding=%u "
			"binding-context=%u actual=%u expected=%u\n",
			substitution_id, binding_id, binding_context_id,
			binding_context ? prototype_context_classifier_term(binding_context) :
				PROTOTYPE_INVALID_ID,
			substitution->term_classifier);
		return -1;
	}
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, substitution->source_context
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, PROTOTYPE_INVALID_ID
	);
	uint32_t classifier_spine = substitution->term_classifier;
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
			uint32_t argument_classifier =
				prototype_context_classifier_term(argument_binding);
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
		uint32_t head_classifier = prototype_context_classifier_term(head_binding);
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
			prototype_term_print_debug(
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
			substitution->term_classifier
		) != 0) {
		if (getenv("A_PROGRAM_FORMATION_TRACE")) {
			fprintf(stderr, "classifier formation expression: ");
			prototype_term_print_debug(
				stderr,
				ctx->symbols,
				ctx->intrinsic_environment,
				ctx->type_declarations,
				ctx->terms,
				substitution->term_classifier
			);
			fprintf(stderr, "\n");
		}
		return -1;
	}
	return prototype_judgement_delta_record_context_binding_assumption(
		&ctx->judgement_delta,
		ctx->terms,
		binding_id,
		substitution->term_classifier
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
				match_case->binder_count >
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY ||
				match_case->first_binder + match_case->binder_count >
					ctx->terms->case_binder_count ||
				ast_case->first_binder + ast_case->binder_count >
					ctx->asts->case_binder_count) {
				return -1;
			}
			struct prototype_case_binder occurrence_binders[
				PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY
			];
			for (uint32_t binder_index = 0;
				binder_index < match_case->binder_count; ++binder_index) {
				if (!prototype_context_contains_binding(
						&ctx->metadata->contexts,
						operation_case->context_id,
						operation_case->binder_ids[binder_index]
					)) {
					fprintf(stderr,
						"match assumption missing binding operation=%u case=%u binder=%u "
						"binding-id=%u context=%u\n",
						operation_id, case_index, binder_index,
						operation_case->binder_ids[binder_index],
						operation_case->context_id);
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
						classifier, operation_case->context_id);
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
		uint32_t type_id;
		uint32_t type_arguments[16];
		uint32_t type_argument_count;
		int type_instance = operation->core_term < ctx->terms->term_count &&
			prototype_term_type_instance_info(
				ctx->terms,
				operation->core_term,
				&type_id,
				type_arguments,
				&type_argument_count
			) == 0;
		int authority_neutral_atom = operation->tag == PROTOTYPE_TYPED_OCCURRENCE_ATOM &&
			operation->core_term < ctx->terms->term_count &&
			(ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_EXTERNAL_REF ||
			 ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_PURE_PRIMITIVE ||
			 ctx->terms->terms[operation->core_term].tag ==
				PROTOTYPE_TERM_EFFECT_OPERATION);
		if (((authority_neutral_atom || type_instance) &&
			 operation->context_action_substitution == PROTOTYPE_INVALID_ID) ||
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
			 operation->context_action_substitution == PROTOTYPE_INVALID_ID) ||
			(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR &&
			 operation->context_action_substitution == PROTOTYPE_INVALID_ID)) {
			return PROTOTYPE_INVALID_ID;
		}
		if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
			operation->wrapped_occurrence <
				ctx->metadata->typed_occurrences.occurrence_count &&
			operation->classifier == ctx->metadata->typed_occurrences.occurrences[
				operation->wrapped_occurrence
			].classifier) {
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
		prototype_term_type_instance_info(
			ctx->terms, term_id, &type_id, arguments, &argument_count
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
		prototype_term_type_instance_info(
			ctx->terms, term_id, &type_id, arguments, &argument_count
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
	if (prototype_term_type_instance_info(
			ctx->terms, term_id, &type_id, arguments, &argument_count
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
		int authority_kind = operation->context_action_substitution ==
			PROTOTYPE_INVALID_ID ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING :
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE;
		uint32_t authority_id = authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
			operation_id : subject;
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
);

static int operation_solver_reify_core_proof_impl(
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
		if (accepted_status == 0) {
			ctx->metadata->proof_reify_accepted_reuse_count++;
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
			uint32_t app_core;
			uint32_t type_id;
			uint32_t type_arguments[16];
			uint32_t type_argument_count;
			if (prototype_term_type_instance_info(
					ctx->terms,
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
					fprintf(stderr, "constructor reification: invalid spine operation=%u\n",
						operation_id);
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
						if (status < 0) {
							fprintf(stderr,
								"constructor reification: argument proof failed "
								"operation=%u argument=%u position=%u\n",
								operation_id, argument_occurrence, i);
						}
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
								operation->context_id, argument_classifiers[i],
								&projected_argument_classifier
							);
						int expected_projection =
							prototype_judgement_project_pure_sequence_results(
								ctx->terms, ctx->type_declarations,
								&ctx->metadata->contexts,
								&ctx->metadata->typed_occurrences,
								operation->context_id, expected_domains[i],
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
								arguments[i]
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
									operation->context_id,
									arguments[i],
									expected_domains[i],
									&argument_evidence[i]
								) : operation_solver_delta_select_evidence(
									ctx,
									argument_occurrence,
									operation->context_id,
									arguments[i],
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
				if (status < 0) {
					fprintf(stderr,
						"app reification: function failed operation=%u function=%u "
						"core=%u classifier=%u\n",
						operation_id, function_operation, function->core_term,
						function->classifier);
				}
				return status;
			}
			status = operation_solver_reify_core_proof(
					ctx, argument_occurrence, app_term->as.app.argument, depth + 1
			);
			if (status != 0) {
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
				status = operation_solver_select_operation_evidence_in_context(
					ctx,
					function_operation,
					operation->context_id,
					app_term->as.app.function,
				function->classifier,
				&function_evidence
			);
			if (status != 0) {
				if (status < 0) {
					fprintf(stderr,
						"app reification: function evidence failed operation=%u function=%u\n",
						operation_id, function_operation);
				}
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
				if (status < 0) {
					fprintf(stderr,
						"app reification: argument evidence failed operation=%u argument=%u\n",
						operation_id, argument_occurrence);
				}
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
				fprintf(stderr,
					"app reification: branch reindex failed operation=%u direct-status=%d "
					"direct=%u classifier=%u\n",
					operation_id, direct_status,
					direct_status == 0 ? direct_classifier : PROTOTYPE_INVALID_ID,
					classifier);
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
						fprintf(stderr,
							"app reification: branch endomorphism failed operation=%u "
							"premise=%u\n",
							operation_id, premise);
						return -1;
					}
					if (operation_solver_materialize_refinement_binding_evidence_at_depth(
							ctx, branch_endomorphism, 0
						) != 0) {
						fprintf(stderr,
							"app reification: refinement evidence failed operation=%u "
							"premise=%u substitution=%u\n",
							operation_id, premise, branch_endomorphism);
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
						fprintf(stderr,
							"app reification: substitution reindex failed operation=%u "
							"premise=%u substitution=%u source=%u target=%u\n",
							operation_id, premise, branch_endomorphism,
							app_evidence[premise]->classifier,
							reindexed_classifiers[premise]);
						return -1;
					}
					if (refined_evidence.classifier != reindexed_classifiers[premise]) {
					fprintf(stderr,
						"app reification: reindexed classifier mismatch operation=%u "
						"premise=%u actual=%u expected=%u\n",
						operation_id, premise, refined_evidence.classifier,
						reindexed_classifiers[premise]);
					fprintf(stderr, "  actual-classifier: ");
					prototype_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms,
						refined_evidence.classifier
					);
					fprintf(stderr, "\n  expected-classifier: ");
					prototype_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms,
						reindexed_classifiers[premise]
					);
					fprintf(stderr, "\n  conversion=%d\n",
						prototype_judgement_classifier_conversion(
							ctx->terms, ctx->type_declarations,
							refined_evidence.classifier,
							reindexed_classifiers[premise]
						).status);
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
			int app_proof_kind = operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION ?
				PROTOTYPE_JUDGEMENT_PROOF_STATIC_FAMILY_APP_ELIM :
				PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM;
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
					struct prototype_term_normalization_result debug_normalization;
					int debug_normalization_status = prototype_term_normalize_with_profile(
						ctx->terms, ctx->type_declarations, NULL,
						PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
						function_evidence.classifier, UINT64_C(1000000),
						&debug_normalization
					);
					fprintf(stderr,
						"app reification: APP classifier failed operation=%u "
						"function=%u:%u(tag=%d) argument=%u:%u(tag=%d) "
						"solved=%u(tag=%d)\n",
						operation_id, function_evidence.subject,
						function_evidence.classifier,
						ctx->terms->terms[function_evidence.classifier].tag,
						argument_evidence.subject, argument_evidence.classifier,
						ctx->terms->terms[argument_evidence.classifier].tag,
						classifier, ctx->terms->terms[classifier].tag);
					fprintf(stderr,
						"app reification: function normalization call=%d status=%d result=%u "
						"tag=%d steps=%llu\n",
						debug_normalization_status, debug_normalization.status,
						debug_normalization.term_id,
						debug_normalization.term_id < ctx->terms->term_count ?
							ctx->terms->terms[debug_normalization.term_id].tag : -1,
						(unsigned long long)debug_normalization.steps_used);
					if (ctx->terms->terms[function_evidence.classifier].tag ==
						PROTOTYPE_TERM_PI) {
						fprintf(stderr,
							"app reification: function PI domain=%u family=%u\n",
							ctx->terms->terms[function_evidence.classifier].as.pi.domain,
							ctx->terms->terms[function_evidence.classifier].as.pi.codomain_family);
					}
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
						operation_id,
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
						operation->source_ast < ctx->asts->node_count ?
							ctx->asts->nodes[operation->source_ast].tag : -1,
						operation->source_ast < ctx->asts->node_count &&
							ctx->asts->nodes[operation->source_ast].tag == PROTOTYPE_AST_VAR ?
							ctx->asts->nodes[operation->source_ast].as.var.ast_binder_id :
							PROTOTYPE_INVALID_ID,
						operation->source_symbol_id,
						operation->referenced_ast_binder_id,
						operation->binding_id,
						ctx->terms->terms[
							operation->source_core_term
						].as.var.binding_id,
						reindexed_evidence.classifier, classifier,
						source_classifier, operation->context_action_substitution,
						action->target_context, action->source_context);
					fprintf(stderr, "  actual classifier: ");
					prototype_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms,
						reindexed_evidence.classifier
					);
					fprintf(stderr, "\n  expected classifier: ");
					prototype_term_print_debug(
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
			const struct prototype_typed_occurrence* referenced =
				&ctx->metadata->typed_occurrences.occurrences[
					operation->wrapped_occurrence
				];
			if (referenced->classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int child_status = operation_solver_reify_core_proof(
				ctx, operation->wrapped_occurrence, core_term, depth + 1
			);
			if (child_status != 0) {
				return child_status;
			}
			struct prototype_judgement_selected_evidence source_evidence;
			int name_status = operation_solver_select_operation_evidence_in_context(
				ctx,
				operation->wrapped_occurrence,
				operation->context_id,
				core_term,
				referenced->classifier,
				&source_evidence
			);
			if (name_status != 0 || referenced->classifier == classifier) {
				return name_status;
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
			int source_status = operation_solver_select_operation_evidence_in_context(
					ctx,
					operation->wrapped_occurrence,
					body->context_id,
					core_term,
					body->classifier,
					&source_evidence
				);
			if (source_status != 0) {
				return source_status;
			}
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
					operation_id,
					&reindexed_evidence,
					classifier,
					core_term
				);
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
	struct operation_reification_cache_entry* cached = NULL;
	if (ctx && ctx->metadata &&
		operation_id < ctx->metadata->typed_occurrences.occurrence_count) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		cached = &ctx->reification_cache[operation_id];
		if (cached->context_id == operation->context_id &&
			cached->classifier == operation->classifier) {
			if (cached->state == OPERATION_REIFICATION_SUCCEEDED) {
				ctx->metadata->proof_reify_current_pass_reuse_count++;
				return 0;
			}
			if (cached->state == OPERATION_REIFICATION_IN_PROGRESS) {
				ctx->metadata->proof_reify_cycle_count++;
				return 1;
			}
		}
		cached->state = OPERATION_REIFICATION_IN_PROGRESS;
		cached->context_id = operation->context_id;
		cached->classifier = operation->classifier;
	}
	if (ctx && ctx->metadata) {
		if (depth == 0) {
			ctx->metadata->proof_reify_root_count++;
		} else {
			ctx->metadata->proof_reify_recursive_count++;
		}
	}
	int status = operation_solver_reify_core_proof_impl(
		ctx, operation_id, core_term, depth
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
		cached->state = status == 0 ? OPERATION_REIFICATION_SUCCEEDED :
			OPERATION_REIFICATION_UNSEEN;
	}
	return status;
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
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		int match_status = operation_solver_materialize_match_judgement(
			ctx, operation_id
		);
		if (match_status < 0) {
			fprintf(
				stderr,
				"judgement materialization: match motive failed operation=%u ast=%u term=%u classifier=%u cases=%u context=%u\n",
				operation_id,
				operation->source_ast,
				operation->core_term,
				operation->classifier,
				operation->case_count,
				operation->context_id
			);
		}
		return match_status;
	}
	int impossible_branch = operation_solver_occurrence_is_in_impossible_branch(
		ctx, operation_id
	);
	if (impossible_branch < 0) {
		return -1;
	}
	if (impossible_branch > 0) {
		return 0;
	}
	prototype_judgement_delta_set_context(
		&ctx->judgement_delta, operation->context_id
	);
	prototype_judgement_delta_set_occurrence(
		&ctx->judgement_delta, operation_id
	);
	uint32_t classifier = operation->classifier;
	if (classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	int free_effect_row = operation_classifier_contains_unowned_effect_row(
		ctx, operation_id, classifier
	);
	if (free_effect_row < 0) {
		return -1;
	}
	if (free_effect_row != 0) {
		return 0;
	}
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
			prototype_term_print_debug(
				stderr, NULL, NULL, ctx->type_declarations, ctx->terms,
				operation->core_term
			);
			fprintf(stderr, "\njudgement materialization classifier: ");
			prototype_term_print_debug(
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
					operation->source_ast < ctx->asts->node_count ?
						ctx->asts->nodes[operation->source_ast].span :
						(struct prototype_source_span) { 0, 0 }
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
	if (full_type_instance || declaration_head) {
		return prototype_judgement_delta_record_type_formation(
			&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
			operation->core_term, classifier
		);
	}
	const struct prototype_term* term = &ctx->terms->terms[operation->core_term];
	if (term->tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
		return prototype_judgement_delta_record_pure_primitive_type(
			&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
			operation->core_term, classifier
		);
	}
	if (term->tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
		return prototype_judgement_delta_record_effect_operation_type(
			&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
			operation->core_term, classifier
		);
	}
	if (term->tag == PROTOTYPE_TERM_TEXT_LITERAL) {
		return prototype_judgement_delta_record_text_literal(
			&ctx->judgement_delta, ctx->terms, operation->core_term, classifier
		);
	}
	if (term->tag == PROTOTYPE_TERM_INT_LITERAL) {
		return prototype_judgement_delta_record_int_literal(
			&ctx->judgement_delta, ctx->terms, ctx->intrinsic_environment,
			operation->core_term, classifier
		);
	}
	return 0;
}

static int operation_solver_materialize_judgements(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	int initial_pass = !ctx->proof_materialization_initialized;
	ctx->proof_materialization_substitution_pending = 0;
	ctx->proof_materialization_initialized = 1;
	ctx->metadata->proof_materialization_pass_count++;
	if (initial_pass) {
		ctx->metadata->proof_materialization_full_scan_count++;
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
		int constructor_domains_changed = 0;
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
			if (ignored_changed) {
				constructor_domains_changed = 1;
			}
		}
		/* Constructor domains can refine a guarded IH and invalidate the classifier
		 * of an enclosing APP.  Do not freeze that intermediate state: replay the
		 * ordinary classifier equations so every projected classifier remains
		 * derivable by its public kernel rule. */
		if (constructor_domains_changed && operation_solver_solve(ctx, 1) != 0) {
			fprintf(stderr,
				"judgement materialization: constructor refinement did not stabilize\n"
			);
			ctx->materializing_checked_classifiers = 0;
			return -1;
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
		current_count = ctx->proof_materialization_pending_count;
		ctx->metadata->evidence_consumer_retry_count += current_count;
		memcpy(
			current_queue,
			ctx->proof_materialization_pending_ids,
			current_count * sizeof(current_queue[0])
		);
	}
	ctx->proof_materialization_pending_count = 0;
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
			if (status < 0) {
				return -1;
			}
			if (status > 0) {
				next_queue[next_count++] = operation_id;
			}
		}
		/* Computation constraints are solved before this phase. Their replayable
		 * rules are retried only after this round produced new premise evidence. */
		if (operation_solver_refresh_computation_constraint_operands(ctx) != 0 ||
			prototype_judgement_delta_solve_recorded_computation_requests(
				&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
				ctx->computation_constraint_results,
				ctx->computation_constraint_result_capacity
			) != 0 ||
			prototype_judgement_delta_solve_recorded_computation_constraints(
				&ctx->judgement_delta, ctx->terms, ctx->type_declarations,
				ctx->computation_constraint_results,
				ctx->computation_constraint_result_capacity
			) != 0 || operation_solver_validate_solution_projection(ctx) != 0) {
			return -1;
		}
		if (next_count == 0 ||
			ctx->judgement_delta.proposition_count == before_relation_count) {
			ctx->proof_materialization_pending_count = next_count;
			memcpy(
				ctx->proof_materialization_pending_ids,
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
	free(materialized_substitution_roots);
	if (operation_solver_refresh_delta_resource_usage(ctx) != 0) {
		return -1;
	}
	/* Match motive results are a construction-time lookup index. The replayable
	 * derivations above are the persistent proof state. */
	ctx->judgement_delta.match_motive_result_count = 0;
	return 0;
}
