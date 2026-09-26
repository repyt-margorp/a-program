static int judgement_delta_occurrence_child(
	const struct prototype_judgement_delta* delta,
	uint32_t parent_occurrence,
	int role,
	uint32_t ordinal,
	uint32_t* p_child_occurrence
) {
	if (!delta || !p_child_occurrence || !delta->occurrences ||
		parent_occurrence >= delta->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* parent =
		&delta->occurrences[parent_occurrence];
	if (!delta->occurrence_edges || parent->first_edge >
		delta->occurrence_edge_count || parent->edge_count >
		delta->occurrence_edge_count - parent->first_edge) {
		return -1;
	}
	for (uint32_t i = 0; i < parent->edge_count; ++i) {
		const struct prototype_typed_occurrence_edge* edge =
			&delta->occurrence_edges[parent->first_edge + i];
		if (edge->role == role && edge->ordinal == ordinal &&
			edge->child_occurrence < delta->occurrence_count) {
			*p_child_occurrence = edge->child_occurrence;
			return 0;
		}
	}
	return 1;
}


int prototype_judgement_delta_record_substitution_reindex(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source,
	uint32_t substitution_id,
	struct prototype_judgement_selected_evidence* evidence
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(delta ? delta->substitutions : NULL, substitution_id);
	uint32_t subject;
	uint32_t classifier;
	if (!delta || !terms || !type_declarations || !source || !evidence ||
		!substitution || source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		substitution->target_context != source->context_id) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY) {
		*evidence = *source;
		return 0;
	}
	if (
		prototype_term_reindex(
			terms,
			type_declarations,
			delta->contexts,
			delta->substitutions,
			source->subject,
			substitution_id,
			&subject
		) != 0 || prototype_term_reindex(
			terms,
			type_declarations,
			delta->contexts,
			delta->substitutions,
			source->classifier,
			substitution_id,
			&classifier
		) != 0) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		.authority_id = subject,
		.context_id = substitution->source_context,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX;
	initialize_proof_rule_parameters(
		&proof,
		premises,
		(struct prototype_judgement_proposition[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		]){{0}}
	);
	proof.semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
	proof.semantic_action_id = substitution_id;
	proof.premise_count = 1;
	proof.premises[0].builder_proposition->kind = source->kind;
	proof.premises[0].builder_proposition->authority_kind = source->authority_kind;
	proof.premises[0].builder_proposition->authority_id = source->authority_id;
	proof.premises[0].builder_proposition->context_id = source->context_id;
	proof.premises[0].builder_proposition->occurrence_id = source->occurrence_id;
	proof.premises[0].builder_proposition->subject = source->subject;
	proof.premises[0].builder_proposition->classifier = source->classifier;
	if (add_complete_delta_relation(delta, &relation, &proof) != 0) {
		return -1;
	}
	selected_evidence_from_candidate(&relation, evidence);
	return 0;
}

int prototype_judgement_delta_expand_match(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_has_tag(terms, classifier, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[subject];
	const struct prototype_term* motive_app = &terms->terms[classifier];
	const struct prototype_term* motive_lambda;
	const struct prototype_term* motive_body;
	uint32_t motive_result_classifier;
	uint32_t normalized_classifier;
	if (motive_app->as.app.argument != match->as.match.scrutinee ||
		!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA) ||
		classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			classifier,
			&normalized_classifier
			) != 0) {
		return -1;
	}
	if (select_delta_universe_classifier(
			delta,
			terms,
			type_declarations,
			classifier,
			&motive_result_classifier
		) != 0) {
		return -1;
	}
	(void)normalized_classifier;
	motive_lambda = &terms->terms[motive_app->as.app.function];
	motive_body = &terms->terms[motive_lambda->as.lambda.body];
	int motive_is_case_split = motive_body->tag == PROTOTYPE_TERM_MATCH;
	if (motive_is_case_split &&
		(motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binding_id !=
			motive_lambda->as.lambda.binding_id)) {
		return -1;
	}
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_selected_evidence premise_evidence[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	int premise_semantic_action_kinds[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	uint32_t premise_semantic_action_ids[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (match->as.match.case_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t premise_count = match->as.match.case_count + 1;
	for (uint32_t i = 0; i < premise_count; ++i) {
		premise_semantic_action_kinds[i] =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		premise_semantic_action_ids[i] = PROTOTYPE_INVALID_ID;
	}
	premise_subjects[0] = classifier;
	premise_classifiers[0] = motive_result_classifier;
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			classifier,
			motive_result_classifier,
			&premise_evidence[0]
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_is_case_split ?
			motive_body->as.match.first_case + i : PROTOTYPE_INVALID_ID;
		if (case_id >= terms->case_count ||
			(motive_is_case_split && motive_case_id >= terms->case_count)) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = motive_is_case_split ?
			&terms->cases[motive_case_id] : NULL;
		if (motive_is_case_split &&
			(match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count)) {
			return -1;
		}
		if (motive_is_case_split &&
			(match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			motive_case->constructor_owner == PROTOTYPE_INVALID_ID)) {
			if (match_case->constructor_owner != motive_case->constructor_owner) {
				return -1;
			}
		} else if (motive_is_case_split) {
			int same_owner = 0;
			if (prototype_term_view_shape_equal(
					terms,
					match_case->constructor_owner,
					motive_case->constructor_owner,
					&same_owner
				) != 0 ||
				!same_owner) {
				return -1;
			}
		}
		if ((motive_is_case_split && select_delta_match_branch_classifier_for_motive(
				delta,
				terms,
				type_declarations,
				match_case,
				motive_case,
				&premise_classifiers[i + 1]
			) != 0) || (!motive_is_case_split &&
			match_case_motive_classifier(
				terms,
				match_case,
				motive_app->as.app.function,
				&premise_classifiers[i + 1]
			) != 0)) {
			return -1;
		}
		premise_subjects[i + 1] = terms->cases[case_id].body;
		int refined = 0;
		if (delta->occurrences && delta->occurrence_match_cases &&
			delta->current_occurrence_id < delta->occurrence_count) {
			const struct prototype_typed_occurrence* operation =
				&delta->occurrences[delta->current_occurrence_id];
			uint32_t scrutinee_occurrence;
			if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
				operation->core_term == subject && judgement_delta_occurrence_child(
					delta, delta->current_occurrence_id,
					PROTOTYPE_TERM_CHILD_SCRUTINEE, 0, &scrutinee_occurrence
				) == 0 && operation->first_case + i <
					delta->occurrence_match_case_count) {
				const struct prototype_typed_occurrence* scrutinee_operation =
					&delta->occurrences[scrutinee_occurrence];
				uint32_t indexed_type_id;
				uint32_t indexed_arguments[16];
				uint32_t indexed_argument_count;
				int identity_relation_classifier = 0;
				(void)indexed_type_id;
				(void)indexed_arguments;
				(void)indexed_argument_count;
				{
						uint32_t action;
						uint32_t action_source;
						uint32_t operator_id;
						uint32_t target_dimension;
						uint32_t action_arguments[2];
						size_t action_argument_count;
						identity_relation_classifier = delta->dimension_operators &&
							prototype_dimension_action_family_instance_info(
								terms,
								delta->dimension_operators,
								scrutinee_operation->classifier,
								&action,
								&action_source,
								&operator_id,
								&target_dimension,
								action_arguments,
								2,
								&action_argument_count
							) == 0 && target_dimension == 1 &&
							action_argument_count == 2;
						(void)action;
						(void)action_source;
						(void)operator_id;
					}
				if (identity_relation_classifier) {
						const struct prototype_typed_occurrence_match_case* operation_case =
							&delta->occurrence_match_cases[operation->first_case + i];
						uint32_t body_occurrence_id;
						const struct prototype_typed_occurrence* body_occurrence =
							judgement_delta_occurrence_child(
								delta, delta->current_occurrence_id,
								PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
								&body_occurrence_id
							) == 0 ? &delta->occurrences[body_occurrence_id] : NULL;
						uint32_t refined_context;
						uint32_t refinement_substitution;
						uint32_t constructor_term;
						struct prototype_judgement_selected_evidence source_evidence;
						uint32_t saved_context = delta->current_context_id;
						uint32_t saved_operation = delta->current_occurrence_id;
						int already_refined =
							prototype_typed_occurrence_match_case_is_solved(operation_case);
						const struct prototype_substitution* stored_refinement =
							already_refined ? prototype_substitution_get(
								delta->substitutions,
								operation_case->refinement_substitution
							) : NULL;
						uint32_t source_branch_context = already_refined && stored_refinement ?
							stored_refinement->target_context : operation_case->context_id;
						if (!body_occurrence ||
							operation_case->constructor_id != match_case->constructor_id ||
							(already_refined && (!stored_refinement ||
							 stored_refinement->source_context != operation_case->context_id)) ||
							prototype_judgement_identity_relation_branch_refinement(
								delta->contexts,
								delta->substitutions,
								terms,
								type_declarations,
								delta->dimension_operators,
								scrutinee_operation->context_id,
								scrutinee_operation->core_term,
								scrutinee_operation->classifier,
								match_case->constructor_id,
								source_branch_context,
								&terms->case_binders[match_case->first_binder],
								match_case->binder_count,
								&refined_context,
								&refinement_substitution,
								&constructor_term
							) != 0 || (already_refined &&
							 (refined_context != operation_case->context_id ||
							  refinement_substitution !=
								operation_case->refinement_substitution)) ||
							(!already_refined && source_branch_context !=
								prototype_substitution_get(
									delta->substitutions, refinement_substitution
								)->target_context) || prototype_judgement_delta_select_evidence(
								delta,
								body_occurrence_id,
								body_occurrence->context_id,
								body_occurrence->core_term,
								body_occurrence->classifier,
								&source_evidence
							) != 0 || (already_refined &&
							 body_occurrence->context_id != operation_case->context_id)) {
							return -1;
						}
						if (already_refined) {
							premise_evidence[i + 1] = source_evidence;
						} else if (prototype_judgement_delta_record_substitution_reindex(
								delta,
								terms,
								type_declarations,
								&source_evidence,
								refinement_substitution,
								&premise_evidence[i + 1]
							) != 0) {
							return -1;
						}
						if (prototype_term_reindex(
								terms,
								type_declarations,
								delta->contexts,
								delta->substitutions,
								motive_is_case_split ? motive_case->body :
									motive_lambda->as.lambda.body,
								refinement_substitution,
								&premise_classifiers[i + 1]
							) != 0 || premise_evidence[i + 1].context_id !=
								refined_context) {
							return -1;
						}
						if (premise_evidence[i + 1].classifier !=
							premise_classifiers[i + 1]) {
							prototype_judgement_delta_set_context(
								delta, refined_context
							);
							prototype_judgement_delta_set_occurrence(
								delta, PROTOTYPE_INVALID_ID
							);
							if (prototype_judgement_delta_record_expected_type_exposure(
									delta,
									terms,
									type_declarations,
									PROTOTYPE_INVALID_ID,
									&premise_evidence[i + 1],
									premise_classifiers[i + 1],
									premise_evidence[i + 1].subject
								) != 0 || prototype_judgement_delta_select_evidence(
									delta,
									PROTOTYPE_INVALID_ID,
									refined_context,
									premise_evidence[i + 1].subject,
									premise_classifiers[i + 1],
									&premise_evidence[i + 1]
								) != 0) {
								return -1;
							}
							prototype_judgement_delta_set_context(
								delta, saved_context
							);
							prototype_judgement_delta_set_occurrence(
								delta, saved_operation
							);
						}
						premise_subjects[i + 1] =
							premise_evidence[i + 1].subject;
						premise_semantic_action_kinds[i + 1] =
							PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
						premise_semantic_action_ids[i + 1] =
							refinement_substitution;
						refined = 1;
						(void)constructor_term;
				}
			}
		}
		if (!refined && lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				premise_subjects[i + 1],
				premise_classifiers[i + 1],
				&premise_evidence[i + 1]
			) != 0) {
			return -1;
		}
	}
	int conclusion_authority = delta->current_occurrence_id <
		delta->occurrence_count ? PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE :
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	uint32_t conclusion_authority_id = conclusion_authority ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ? delta->current_occurrence_id :
		subject;
	uint32_t conclusion_occurrence_id = conclusion_authority ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ? delta->current_occurrence_id :
		PROTOTYPE_INVALID_ID;
	int status = add_delta_relation_with_authority_and_explicit_premise_actions(
		delta,
		conclusion_authority,
		conclusion_authority_id,
		conclusion_occurrence_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		premise_semantic_action_kinds,
		premise_semantic_action_ids,
		premise_count
	);
	if (status == 0) {
		remove_match_motive_results_normalization_equal(
			delta,
			terms,
			type_declarations,
			subject,
			classifier
		);
	}
	return status;
}

int prototype_judgement_delta_expand_induction_hypothesis(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	uint32_t match_term,
	uint32_t motive,
	uint32_t case_index,
	uint32_t field_index,
	const struct prototype_judgement_selected_evidence* context_evidence,
	uint32_t context_evidence_count
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		!term_exists(terms, classifier) ||
		!term_exists(terms, motive) ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		context_evidence_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		(!context_evidence && context_evidence_count != 0)) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = delta->current_context_id,
		.occurrence_id = delta->current_occurrence_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM;
	initialize_proof_rule_parameters(
		&proof,
		proof_premises,
		(struct prototype_judgement_proposition[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		]){{0}}
	);
	proof.rule_data.induction.match = match_term;
	proof.rule_data.induction.motive = motive;
	proof.rule_data.induction.case_index = case_index;
	proof.rule_data.induction.field_index = field_index;
	proof.premise_count = context_evidence_count;
	for (uint32_t i = 0; i < context_evidence_count; ++i) {
		proof.premises[i].builder_proposition->kind = context_evidence[i].kind;
		proof.premises[i].builder_proposition->authority_kind =
			context_evidence[i].authority_kind;
		proof.premises[i].builder_proposition->authority_id =
			context_evidence[i].authority_id;
		proof.premises[i].builder_proposition->context_id = context_evidence[i].context_id;
		proof.premises[i].builder_proposition->occurrence_id =
			context_evidence[i].occurrence_id;
		proof.premises[i].builder_proposition->subject = context_evidence[i].subject;
		proof.premises[i].builder_proposition->classifier = context_evidence[i].classifier;
	}
	return add_complete_delta_relation(delta, &relation, &proof);
}
