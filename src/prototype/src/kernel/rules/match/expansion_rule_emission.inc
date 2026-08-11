int prototype_judgement_expand_match(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
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
		classifier_kernel_whnf_no_definitions(terms, type_declarations, classifier, &normalized_classifier) != 0) {
		return -1;
	}
	if (select_judgement_universe_classifier(
			judgement,
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
	if (match->as.match.case_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t premise_count = match->as.match.case_count + 1;
	premise_subjects[0] = classifier;
	premise_classifiers[0] = motive_result_classifier;
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
		if ((motive_is_case_split && select_judgement_match_branch_classifier_for_motive(
				judgement,
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
	}
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_count
	);
}

static int delta_record_substitution_reindex(
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
		substitution->target_context != source->context_id ||
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
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = subject;
	proof.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&proof, premises);
	proof.semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
	proof.semantic_action_id = substitution_id;
	proof.premise_count = 1;
	proof.premises[0].proposition.kind = source->kind;
	proof.premises[0].proposition.authority_kind = source->authority_kind;
	proof.premises[0].proposition.authority_id = source->authority_id;
	proof.premises[0].proposition.context_id = source->context_id;
	proof.premises[0].proposition.operation_id = source->operation_id;
	proof.premises[0].proposition.subject = source->subject;
	proof.premises[0].proposition.classifier = source->classifier;
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
		if (delta->operations && delta->operation_cases &&
			delta->current_operation_id < delta->operation_count) {
			const struct prototype_operation_node* operation =
				&delta->operations[delta->current_operation_id];
			if (operation->tag == PROTOTYPE_OPERATION_MATCH &&
				operation->core_term == subject && operation->scrutinee <
					delta->operation_count && operation->first_case + i <
					delta->operation_case_count) {
				const struct prototype_operation_node* scrutinee_operation =
					&delta->operations[operation->scrutinee];
				uint32_t indexed_type_id;
				uint32_t indexed_arguments[16];
				uint32_t indexed_argument_count;
				if (prototype_term_type_instance_info(
						terms,
						scrutinee_operation->classifier,
						&indexed_type_id,
						indexed_arguments,
						&indexed_argument_count
					) == 0 && indexed_type_id < type_declarations->type_count) {
					const struct prototype_type_declaration* indexed_type =
						&type_declarations->type_declarations[indexed_type_id];
					if (indexed_type->origin_kind ==
							PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
						indexed_type->parameter_count == 0 &&
						indexed_type->index_count == 2 &&
						indexed_argument_count == 2) {
						const struct prototype_operation_match_case* operation_case =
							&delta->operation_cases[operation->first_case + i];
						const struct prototype_operation_node* body_operation =
							operation_case->body_operation < delta->operation_count ?
							&delta->operations[operation_case->body_operation] : NULL;
						uint32_t refined_context;
						uint32_t refinement_substitution;
						uint32_t constructor_term;
						struct prototype_judgement_selected_evidence source_evidence;
						uint32_t saved_context = delta->current_context_id;
						uint32_t saved_operation = delta->current_operation_id;
						if (!body_operation ||
							operation_case->constructor_id != match_case->constructor_id ||
							prototype_judgement_indexed_branch_refinement(
								delta->contexts,
								delta->substitutions,
								terms,
								type_declarations,
								scrutinee_operation->context_id,
								scrutinee_operation->core_term,
								scrutinee_operation->classifier,
								match_case->constructor_id,
								operation_case->context_id,
								&terms->case_binders[match_case->first_binder],
								match_case->binder_count,
								&refined_context,
								&refinement_substitution,
								&constructor_term
							) != 0 || operation_case->context_id !=
								prototype_substitution_get(
									delta->substitutions, refinement_substitution
								)->target_context || prototype_judgement_delta_select_evidence(
								delta,
								operation_case->body_operation,
								operation_case->context_id,
								body_operation->core_term,
								body_operation->classifier,
								&source_evidence
							) != 0 || delta_record_substitution_reindex(
								delta,
								terms,
								type_declarations,
								&source_evidence,
								refinement_substitution,
								&premise_evidence[i + 1]
							) != 0 || prototype_term_reindex(
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
							prototype_judgement_delta_set_operation(
								delta, PROTOTYPE_INVALID_ID
							);
							if (prototype_judgement_delta_record_expected_type_exposure(
									delta,
									terms,
									type_declarations,
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
							prototype_judgement_delta_set_operation(
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
	int conclusion_authority = delta->current_operation_id <
		delta->operation_count ? PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION :
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	uint32_t conclusion_authority_id = conclusion_authority ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ? delta->current_operation_id :
		subject;
	uint32_t conclusion_operation_id = conclusion_authority ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ? delta->current_operation_id :
		PROTOTYPE_INVALID_ID;
	int status = add_delta_relation_with_authority_and_explicit_premise_actions(
		delta,
		conclusion_authority,
		conclusion_authority_id,
		conclusion_operation_id,
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
	uint32_t field_index
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		!term_exists(terms, classifier) ||
		!term_exists(terms, motive) ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = delta->current_context_id,
		.operation_id = delta->current_operation_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = relation.operation_id;
	proof.conclusion_subject = relation.subject;
	proof.conclusion_classifier = relation.classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.rule_data.induction.match = match_term;
	proof.rule_data.induction.motive = motive;
	proof.rule_data.induction.case_index = case_index;
	proof.rule_data.induction.field_index = field_index;
	return add_complete_delta_relation(delta, &relation, &proof);
}
