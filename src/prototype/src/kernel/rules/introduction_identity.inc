int prototype_judgement_delta_record_lambda_intro(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t conclusion_operation_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t binder_subject,
	uint32_t body_subject,
	const struct prototype_judgement_selected_evidence* binder_evidence,
	uint32_t body_operation_id,
	const struct prototype_judgement_selected_evidence* body_evidence
) {
	if (!delta || !terms || !type_declarations ||
		!binder_evidence || !body_evidence ||
		binder_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		body_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		binder_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		body_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_LAMBDA) ||
		!term_has_tag(terms, binder_subject, PROTOTYPE_TERM_VAR) ||
		!term_exists(terms, body_subject) ||
		!term_exists(terms, classifier) ||
		binder_evidence->subject != binder_subject ||
		body_evidence->subject != body_subject ||
		!term_exists(terms, binder_evidence->classifier) ||
		!term_exists(terms, body_evidence->classifier)) {
		return -1;
	}
	uint32_t binder_classifier = binder_evidence->classifier;
	uint32_t body_classifier = body_evidence->classifier;
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t occurrence_lambda;
	int occurrence_matches;
	uint32_t expected_body_classifier;
	uint32_t lambda_classifier;
	uint32_t binder_id = terms->terms[binder_subject].as.var.binding_id;
	if (classifier_kernel_strip_effect_row_foralls(
			terms, type_declarations, NULL, classifier, &lambda_classifier
		) != 0 || classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			lambda_classifier,
			&lambda_pi,
			&domain,
			&codomain_family
		) != 0 ||
		prototype_term_lambda(terms, binder_id, body_subject, &occurrence_lambda) != 0 ||
		prototype_term_core_shape_equal(
			terms, occurrence_lambda, subject, &occurrence_matches
		) != 0 || !occurrence_matches ||
		pi_codomain_at_binder_in_context(
			delta,
			terms,
			type_declarations,
			body_evidence->context_id,
			lambda_pi,
			binder_subject,
			binder_classifier,
			PROTOTYPE_INVALID_ID,
			&expected_body_classifier
		) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, domain, binder_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
		!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			expected_body_classifier,
			body_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	(void)codomain_family;
	uint32_t premise_subjects[2] = {
		binder_subject,
		body_subject
	};
	uint32_t premise_classifiers[2] = {
		binder_classifier,
		body_classifier
	};
	uint32_t premise_context_ids[2] = {
		binder_evidence->context_id,
		body_evidence->context_id
	};
	uint32_t premise_operation_ids[2] = {
		PROTOTYPE_INVALID_ID,
		body_operation_id
	};
	struct prototype_judgement_selected_evidence premise_evidence[2] = {
		*binder_evidence,
		*body_evidence
	};
	int authority_kind = conclusion_operation_id == PROTOTYPE_INVALID_ID ?
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER :
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION;
	uint32_t authority_id = conclusion_operation_id == PROTOTYPE_INVALID_ID ?
		subject : conclusion_operation_id;
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		authority_kind,
		authority_id,
		conclusion_operation_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
		premise_context_ids,
		premise_operation_ids,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		2
	);
}

int prototype_judgement_delta_app_elim_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function_classifier,
	uint32_t argument_subject,
	uint32_t argument_classifier,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		!term_exists(terms, function_classifier) ||
		!term_exists(terms, argument_subject) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	uint32_t specialized_function_classifier;
	if (prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			function_classifier,
			argument_classifier,
			&specialized_function_classifier
		) != 0) {
		return -1;
	}
	uint32_t function_pi;
	uint32_t domain;
	uint32_t codomain_family;
	if (classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			specialized_function_classifier,
			&function_pi,
			&domain,
			&codomain_family
		) != 0) {
		return -1;
	}
	uint32_t domain_whnf;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			domain,
			&domain_whnf
		) != 0 || domain_whnf >= terms->term_count) {
		return -1;
	}
	if (terms->terms[domain_whnf].tag == PROTOTYPE_TERM_RETURN) {
		domain_whnf = terms->terms[domain_whnf].as.return_term.value;
	}
	if (!prototype_judgement_classifier_compatible(
			terms, type_declarations, domain_whnf, argument_classifier
		)) {
		return -1;
	}
	(void)codomain_family;
	return pi_codomain_after_argument_in_context(
		delta->contexts,
		delta->substitutions,
		terms,
		type_declarations,
		delta->current_context_id,
		function_pi,
		argument_subject,
		argument_classifier,
		p_classifier
	);
}

int prototype_judgement_delta_record_app_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const struct prototype_judgement_selected_evidence* function_evidence,
	const struct prototype_judgement_selected_evidence* argument_evidence
) {
	if (!delta || !terms || !type_declarations ||
		!function_evidence || !argument_evidence ||
		function_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		argument_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		function_evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		argument_evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		subject >= terms->term_count ||
		function_evidence->subject >= terms->term_count ||
		argument_evidence->subject >= terms->term_count ||
		!term_exists(terms, classifier) ||
		!term_exists(terms, function_evidence->classifier) ||
		!term_exists(terms, argument_evidence->classifier)) {
		return -1;
	}
	const struct prototype_term* app;
	if (term_core_app(terms, subject, &app) != 0) {
		return -1;
	}
	int function_matches = 0;
	int argument_matches = 0;
	if (prototype_term_core_shape_equal(
			terms,
			app->as.app.function,
			function_evidence->subject,
			&function_matches
		) != 0 ||
		prototype_term_core_shape_equal(
			terms,
			app->as.app.argument,
			argument_evidence->subject,
			&argument_matches
		) != 0 ||
		!function_matches || !argument_matches) {
		return -1;
	}
	uint32_t result_classifier;
	int result_status = prototype_judgement_delta_app_elim_classifier(
		delta,
		terms,
		type_declarations,
		function_evidence->classifier,
		argument_evidence->subject,
		argument_evidence->classifier,
		&result_classifier
	);
	int result_equal = result_status == 0 &&
		(prototype_judgement_classifier_conversion(
			terms, type_declarations, result_classifier, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL);
	if (result_status != 0 || !result_equal) {
		return -1;
	}
	uint32_t premise_subjects[2] = {
		function_evidence->subject,
		argument_evidence->subject
	};
	uint32_t premise_classifiers[2] = {
		function_evidence->classifier,
		argument_evidence->classifier
	};
	struct prototype_judgement_selected_evidence premise_evidence[2] = {
		*function_evidence,
		*argument_evidence
	};
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		2
	);
}

int prototype_judgement_delta_record_context_weaken(
	struct prototype_judgement_delta* delta,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t substitution_id
) {
	if (!delta || !source_evidence ||
		(source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		 source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		source_evidence->context_id == delta->current_context_id ||
		!prototype_substitution_is_projection_path(
			delta->substitutions,
			delta->contexts,
			substitution_id,
			delta->current_context_id,
			source_evidence->context_id
		)) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = source_evidence->kind,
		.authority_kind = source_evidence->authority_kind,
		.authority_id = source_evidence->authority_id,
		.context_id = delta->current_context_id,
		.operation_id = source_evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
			source_evidence->operation_id : PROTOTYPE_INVALID_ID,
		.subject = source_evidence->subject,
		.classifier = source_evidence->classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = relation.operation_id;
	proof.conclusion_subject = relation.subject;
	proof.conclusion_classifier = relation.classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
	proof.semantic_action_id = substitution_id;
	proof.premise_count = 1;
	proof.premises[0].proposition.kind = source_evidence->kind;
	proof.premises[0].proposition.context_id = source_evidence->context_id;
	proof.premises[0].proposition.subject = source_evidence->subject;
	proof.premises[0].proposition.classifier = source_evidence->classifier;
	proof.premises[0].proposition.authority_kind = source_evidence->authority_kind;
	proof.premises[0].proposition.authority_id = source_evidence->authority_id;
	proof.premises[0].proposition.operation_id = source_evidence->operation_id;
	return add_complete_delta_relation(delta, &relation, &proof);
}

static void selected_evidence_from_accepted_claim(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_claim* claim,
	struct prototype_judgement_selected_evidence* evidence
) {
	memset(evidence, 0, sizeof(*evidence));
	evidence->kind = prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind;
	evidence->authority_kind = prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind;
	evidence->authority_id = prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_id;
	evidence->context_id = prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id;
	evidence->operation_id = prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id;
	evidence->subject = prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject;
	evidence->classifier = prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier;
}

int prototype_judgement_selected_evidence_from_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	if (!claim || !p_evidence ||
		!prototype_judgement_proposition_get(judgement, claim->proposition_id)) {
		return -1;
	}
	selected_evidence_from_accepted_claim(judgement, claim, p_evidence);
	return 0;
}

static int add_claim_reindexed_by_substitution(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_claim_id,
	uint32_t substitution_id,
	int proof_kind,
	int require_projection,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* source =
		prototype_judgement_claim_get(judgement, source_claim_id);
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	uint32_t subject;
	uint32_t classifier;
	if (!source || !substitution || !p_claim_id ||
		(prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		 prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) ||
		substitution->target_context != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY) {
		*p_claim_id = source_claim_id;
		return 0;
	}
	if ((require_projection && !prototype_substitution_is_projection_path(
			substitutions,
			contexts,
			substitution_id,
			substitution->source_context,
			prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id
		)) || (!require_projection &&
			(prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				prototype_judgement_proposition_get(judgement, source->proposition_id)->subject,
				substitution_id,
				&subject
			) != 0 || prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				prototype_judgement_proposition_get(judgement, source->proposition_id)->classifier,
				substitution_id,
				&classifier
			) != 0))) {
		return -1;
	}
	if (require_projection) {
		subject = prototype_judgement_proposition_get(judgement, source->proposition_id)->subject;
		classifier = prototype_judgement_proposition_get(judgement, source->proposition_id)->classifier;
	}
	struct prototype_judgement_proposition relation = {
		.kind = prototype_judgement_proposition_get(judgement, source->proposition_id)->kind,
		.authority_kind = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID :
			prototype_judgement_proposition_get(
				judgement, source->proposition_id
			)->authority_kind,
		.authority_id = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
			PROTOTYPE_INVALID_ID : prototype_judgement_proposition_get(
				judgement, source->proposition_id
			)->authority_id,
		.context_id = substitution->source_context,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = relation.subject;
	proof.conclusion_classifier = relation.classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
	proof.semantic_action_id = substitution_id;
	proof.premise_count = 1;
	proof.premises[0].proposition.kind = prototype_judgement_proposition_get(judgement, source->proposition_id)->kind;
	proof.premises[0].proposition.context_id = prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id;
	proof.premises[0].proposition.subject = prototype_judgement_proposition_get(judgement, source->proposition_id)->subject;
	proof.premises[0].proposition.classifier = prototype_judgement_proposition_get(judgement, source->proposition_id)->classifier;
	proof.premises[0].proposition.authority_kind = prototype_judgement_proposition_get(judgement, source->proposition_id)->authority_kind;
	proof.premises[0].proposition.authority_id = prototype_judgement_proposition_get(judgement, source->proposition_id)->authority_id;
	proof.premises[0].proposition.operation_id = prototype_judgement_proposition_get(judgement, source->proposition_id)->operation_id;
	uint32_t premise_claim_ids[1] = { source_claim_id };
	return publish_complete_relation(
		judgement, &relation, &proof, premise_claim_ids, p_claim_id
	);
}

int prototype_judgement_add_context_weakened_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t source_claim_id,
	uint32_t target_context_id,
	uint32_t projection_substitution_id,
	uint32_t* p_claim_id
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, projection_substitution_id);
	if (!substitution || substitution->source_context != target_context_id) {
		return -1;
	}
	return add_claim_reindexed_by_substitution(
		judgement,
		NULL,
		NULL,
		contexts,
		(struct prototype_substitution_db*)substitutions,
		source_claim_id,
		projection_substitution_id,
		PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN,
		1,
		p_claim_id
	);
}

int prototype_judgement_add_reindexed_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_claim_id,
	uint32_t substitution_id,
	uint32_t* p_claim_id
) {
	return add_claim_reindexed_by_substitution(
		judgement,
		terms,
		type_declarations,
		contexts,
		substitutions,
		source_claim_id,
		substitution_id,
		PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX,
		0,
		p_claim_id
	);
}

int prototype_judgement_add_context_binding_assumption(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t* p_claim_id
) {
	uint32_t subject;
	uint32_t entry_context_id;
	if (!judgement || !terms || !contexts || !p_claim_id ||
		classifier >= terms->term_count ||
		prototype_context_find_binding(
			contexts, context_id, binding_id, &entry_context_id
		) != 0 ||
		prototype_context_classifier_term(
			prototype_context_get(contexts, entry_context_id)
		) != classifier ||
		prototype_term_var(terms, binding_id, &subject) != 0) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = subject;
	proof.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	return publish_complete_relation(
		judgement, &relation, &proof, NULL, p_claim_id
	);
}

int prototype_judgement_add_relation_type_formation(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t relation_type,
	uint32_t universe,
	uint32_t left_type_claim_id,
	uint32_t right_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_claim_id
) {
	if (!judgement || !terms || !p_claim_id) {
		return -1;
	}
	uint32_t left_classifier;
	uint32_t right_classifier;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_classifier,
			&right_classifier,
			&left_endpoint,
			&right_endpoint
		) != 0) {
		return -1;
	}
	uint32_t premise_ids[4] = {
		left_type_claim_id,
		right_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id
	};
	const struct prototype_judgement_claim* premise_claims[4];
	for (uint32_t i = 0; i < 4; ++i) {
		premise_claims[i] = prototype_judgement_claim_get(
			judgement, premise_ids[i]
		);
		if (!premise_claims[i] ||
			prototype_judgement_proposition_get(judgement, premise_claims[i]->proposition_id)->context_id != context_id) {
			return -1;
		}
	}
	if (prototype_judgement_proposition_get(judgement, premise_claims[0]->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, premise_claims[0]->proposition_id)->subject != left_classifier ||
		prototype_judgement_proposition_get(judgement, premise_claims[0]->proposition_id)->classifier != universe ||
		prototype_judgement_proposition_get(judgement, premise_claims[1]->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, premise_claims[1]->proposition_id)->subject != right_classifier ||
		prototype_judgement_proposition_get(judgement, premise_claims[1]->proposition_id)->classifier != universe ||
		prototype_judgement_proposition_get(judgement, premise_claims[2]->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, premise_claims[2]->proposition_id)->subject != left_endpoint ||
		prototype_judgement_proposition_get(judgement, premise_claims[2]->proposition_id)->classifier != left_classifier ||
		prototype_judgement_proposition_get(judgement, premise_claims[3]->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, premise_claims[3]->proposition_id)->subject != right_endpoint ||
		prototype_judgement_proposition_get(judgement, premise_claims[3]->proposition_id)->classifier != right_classifier) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = relation_type,
		.classifier = universe
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = relation_type;
	proof.conclusion_classifier = universe;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 4;
	for (uint32_t i = 0; i < 4; ++i) {
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise_claims[i], &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &relation, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_witness_intro(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	struct prototype_term_conversion_result comparison;
	if (!judgement || !terms || !type_declarations || !p_claim_id ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left ||
		witness_right != relation_right || !relation || !left || !right ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != right_type ||
		prototype_term_compare_for_conversion(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			relation_left,
			relation_right,
			64,
			&comparison
		) != 0 || comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RELATION_WITNESS_INTRO;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	uint32_t premise_ids[3] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id
	};
	const struct prototype_judgement_claim* premises[3] = {
		relation,
		left,
		right
	};
	proof.premise_count = 3;
	for (uint32_t i = 0; i < 3; ++i) {
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premises[i], &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_constructor_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	const uint32_t* field_witness_claim_ids,
	uint32_t field_witness_count,
	uint32_t* p_claim_id
) {
	if (!judgement || !terms || !p_claim_id ||
		field_witness_count + 3 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		(field_witness_count != 0 && !field_witness_claim_ids)) {
		return -1;
	}
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t relation_left_type;
	uint32_t relation_right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t left_head;
	uint32_t left_owner;
	uint32_t left_constructor;
	uint32_t left_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t left_argument_count;
	uint32_t right_head;
	uint32_t right_owner;
	uint32_t right_constructor;
	uint32_t right_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t right_argument_count;
	if (!relation || !left || !right ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&relation_left_type,
			&relation_right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != relation_left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != relation_right_type ||
		prototype_term_constructor_spine_info(
			terms,
			relation_left,
			&left_head,
			&left_owner,
			&left_constructor,
			left_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&left_argument_count
		) != 0 || prototype_term_constructor_spine_info(
			terms,
			relation_right,
			&right_head,
			&right_owner,
			&right_constructor,
			right_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&right_argument_count
		) != 0 || left_owner != right_owner ||
		left_constructor != right_constructor ||
		left_argument_count != field_witness_count ||
		right_argument_count != field_witness_count) {
		return -1;
	}
	uint32_t premise_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	premise_ids[0] = relation_type_claim_id;
	premise_ids[1] = left_endpoint_claim_id;
	premise_ids[2] = right_endpoint_claim_id;
	for (uint32_t i = 0; i < field_witness_count; ++i) {
		const struct prototype_judgement_claim* field =
			prototype_judgement_claim_get(
				judgement, field_witness_claim_ids[i]
			);
		uint32_t field_left_type;
		uint32_t field_right_type;
		uint32_t field_left;
		uint32_t field_right;
		if (!field || prototype_judgement_proposition_get(judgement, field->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			prototype_judgement_proposition_get(judgement, field->proposition_id)->context_id != context_id ||
			prototype_term_relation_type_info(
				terms,
				prototype_judgement_proposition_get(judgement, field->proposition_id)->classifier,
				&field_left_type,
				&field_right_type,
				&field_left,
				&field_right
			) != 0 || field_left != left_arguments[i] ||
			field_right != right_arguments[i]) {
			return -1;
		}
		premise_ids[i + 3] = field_witness_claim_ids[i];
	}
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = field_witness_count + 3;
	for (uint32_t i = 0; i < proof.premise_count; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	(void)left_head;
	(void)right_head;
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_unary_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t child_witness_claim_id,
	int proof_kind,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* child =
		prototype_judgement_claim_get(judgement, child_witness_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t relation_left_type;
	uint32_t relation_right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t child_left_type;
	uint32_t child_right_type;
	uint32_t child_left;
	uint32_t child_right;
	uint32_t left_payload;
	uint32_t right_payload;
	int endpoint_tag;
	if (proof_kind == PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS) {
		endpoint_tag = PROTOTYPE_TERM_RETURN;
	} else if (proof_kind ==
		PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS) {
		endpoint_tag = PROTOTYPE_TERM_THUNK;
	} else {
		return -1;
	}
	if (!judgement || !terms || !p_claim_id || !relation || !left || !right ||
		!child ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&relation_left_type,
			&relation_right_type,
			&relation_left,
			&relation_right
		) != 0 || relation_left >= terms->term_count ||
		relation_right >= terms->term_count || witness_left != relation_left ||
		witness_right != relation_right || terms->terms[relation_left].tag !=
			endpoint_tag || terms->terms[relation_right].tag != endpoint_tag ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != relation_left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != relation_right_type ||
		prototype_judgement_proposition_get(judgement, child->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, child->proposition_id)->context_id != context_id || prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, child->proposition_id)->classifier,
			&child_left_type,
			&child_right_type,
			&child_left,
			&child_right
		) != 0) {
		return -1;
	}
	left_payload = endpoint_tag == PROTOTYPE_TERM_RETURN ?
		terms->terms[relation_left].as.return_term.value :
		terms->terms[relation_left].as.thunk.computation;
	right_payload = endpoint_tag == PROTOTYPE_TERM_RETURN ?
		terms->terms[relation_right].as.return_term.value :
		terms->terms[relation_right].as.thunk.computation;
	if (child_left != left_payload || child_right != right_payload) {
		return -1;
	}
	uint32_t premise_ids[4] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		child_witness_claim_id
	};
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 4;
	for (uint32_t i = 0; i < 4; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_app_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t function_witness_claim_id,
	uint32_t argument_witness_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* function_witness =
		prototype_judgement_claim_get(judgement, function_witness_claim_id);
	const struct prototype_judgement_claim* argument_witness =
		prototype_judgement_claim_get(judgement, argument_witness_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t function_left_type;
	uint32_t function_right_type;
	uint32_t function_left;
	uint32_t function_right;
	uint32_t argument_left_type;
	uint32_t argument_right_type;
	uint32_t argument_left;
	uint32_t argument_right;
	if (!judgement || !terms || !p_claim_id || !relation || !left || !right ||
		!function_witness || !argument_witness ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_APP ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_APP ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != right_type ||
		prototype_judgement_proposition_get(judgement, function_witness->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, function_witness->proposition_id)->context_id != context_id ||
		prototype_judgement_proposition_get(judgement, argument_witness->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, argument_witness->proposition_id)->context_id != context_id ||
		prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, function_witness->proposition_id)->classifier,
			&function_left_type,
			&function_right_type,
			&function_left,
			&function_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, argument_witness->proposition_id)->classifier,
			&argument_left_type,
			&argument_right_type,
			&argument_left,
			&argument_right
		) != 0 ||
		function_left != terms->terms[relation_left].as.app.function ||
		function_right != terms->terms[relation_right].as.app.function ||
		argument_left != terms->terms[relation_left].as.app.argument ||
		argument_right != terms->terms[relation_right].as.app.argument) {
		return -1;
	}
	uint32_t premise_ids[5] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		function_witness_claim_id,
		argument_witness_claim_id
	};
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 5;
	for (uint32_t i = 0; i < 5; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

static int relation_lambda_witness_shape_valid(
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t body_context_id,
	uint32_t body_witness_classifier
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t body_left_type;
	uint32_t body_right_type;
	uint32_t body_left;
	uint32_t body_right;
	if (!terms || !contexts ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		left_type >= terms->term_count || right_type >= terms->term_count ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[left_type].tag != PROTOTYPE_TERM_PI ||
		terms->terms[right_type].tag != PROTOTYPE_TERM_PI ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_LAMBDA ||
		prototype_term_relation_type_info(
			terms,
			body_witness_classifier,
			&body_left_type,
			&body_right_type,
			&body_left,
			&body_right
		) != 0) {
		return 0;
	}
	const struct prototype_context* relation_context =
		prototype_context_get(contexts, body_context_id);
	const struct prototype_context* right_context = relation_context ?
		prototype_context_get(contexts, relation_context->parent) : NULL;
	const struct prototype_context* left_context = right_context ?
		prototype_context_get(contexts, right_context->parent) : NULL;
	if (!relation_context || !right_context || !left_context ||
		left_context->parent != context_id ||
		prototype_context_classifier_term(left_context) !=
			terms->terms[left_type].as.pi.domain ||
		prototype_context_classifier_term(right_context) !=
			terms->terms[right_type].as.pi.domain) {
		return 0;
	}
	uint32_t left_argument;
	uint32_t right_argument;
	uint32_t expected_input_relation;
	uint32_t left_lambda;
	uint32_t right_lambda;
	int left_matches;
	int right_matches;
	if (prototype_term_var(
			terms, left_context->binding_id, &left_argument
		) != 0 || prototype_term_var(
			terms, right_context->binding_id, &right_argument
		) != 0 || prototype_term_relation_type(
			terms,
			terms->terms[left_type].as.pi.domain,
			terms->terms[right_type].as.pi.domain,
			left_argument,
			right_argument,
			&expected_input_relation
		) != 0 || prototype_context_classifier_term(relation_context) !=
			expected_input_relation || prototype_term_lambda(
			terms, left_context->binding_id, body_left, &left_lambda
		) != 0 || prototype_term_lambda(
			terms, right_context->binding_id, body_right, &right_lambda
		) != 0 || prototype_term_core_shape_equal(
			terms, left_lambda, relation_left, &left_matches
		) != 0 || prototype_term_core_shape_equal(
			terms, right_lambda, relation_right, &right_matches
		) != 0) {
		return 0;
	}
	(void)body_left_type;
	(void)body_right_type;
	return left_matches && right_matches;
}

int prototype_judgement_add_relation_lambda_witness(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t body_witness_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* body =
		prototype_judgement_claim_get(judgement, body_witness_claim_id);
	uint32_t relation_left_type;
	uint32_t relation_right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	if (!judgement || !terms || !contexts || !p_claim_id || !relation || !left ||
		!right || !body || prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_term_relation_type_info(
			terms,
			relation_type,
			&relation_left_type,
			&relation_right_type,
			&relation_left,
			&relation_right
		) != 0 || prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != relation_left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != relation_right_type ||
		prototype_judgement_proposition_get(judgement, body->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!relation_lambda_witness_shape_valid(
			terms,
			contexts,
			context_id,
			witness,
			relation_type,
			prototype_judgement_proposition_get(judgement, body->proposition_id)->context_id,
			prototype_judgement_proposition_get(judgement, body->proposition_id)->classifier
		)) {
		return -1;
	}
	uint32_t premise_ids[4] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		body_witness_claim_id
	};
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 4;
	for (uint32_t i = 0; i < 4; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

static int relation_match_case_witness_shape_valid(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t outer_context_id,
	const struct prototype_match_case* source_case,
	const struct prototype_match_case* left_case,
	const struct prototype_match_case* right_case,
	const struct prototype_judgement_proposition* case_witness
) {
	if (!terms || !type_declarations || !contexts || !source_case || !left_case ||
		!right_case || !case_witness ||
		source_case->binder_count != left_case->binder_count ||
		left_case->binder_count != right_case->binder_count ||
		source_case->constructor_id != left_case->constructor_id ||
		left_case->constructor_id != right_case->constructor_id) {
		return 0;
	}
	uint32_t binder_count = left_case->binder_count;
	if (binder_count > 64) {
		return 0;
	}
	struct prototype_case_binder left_targets[64];
	struct prototype_case_binder right_targets[64];
	uint32_t current_context = case_witness->context_id;
	for (uint32_t i = binder_count; i > 0; --i) {
		const struct prototype_case_binder* source_binder =
			&terms->case_binders[source_case->first_binder + i - 1];
		const struct prototype_case_binder* left_binder =
			&terms->case_binders[left_case->first_binder + i - 1];
		const struct prototype_case_binder* right_binder =
			&terms->case_binders[right_case->first_binder + i - 1];
		const struct prototype_context* relation_context =
			prototype_context_get(contexts, current_context);
		const struct prototype_context* right_context = relation_context ?
			prototype_context_get(contexts, relation_context->parent) : NULL;
		const struct prototype_context* left_context = right_context ?
			prototype_context_get(contexts, right_context->parent) : NULL;
		if (!relation_context || !right_context || !left_context ||
			source_binder->is_recursive != left_binder->is_recursive ||
			left_binder->is_recursive != right_binder->is_recursive) {
			return 0;
		}
		uint32_t left_variable;
		uint32_t right_variable;
		uint32_t relation_type;
		if (prototype_term_var(
				terms, left_context->binding_id, &left_variable
			) != 0 || prototype_term_var(
				terms, right_context->binding_id, &right_variable
			) != 0 || prototype_term_relation_type(
				terms,
				prototype_context_classifier_term(left_context),
				prototype_context_classifier_term(right_context),
				left_variable,
				right_variable,
				&relation_type
			) != 0 || prototype_context_classifier_term(relation_context) !=
				relation_type) {
			return 0;
		}
		left_targets[i - 1] = (struct prototype_case_binder){
			.binding_id = left_context->binding_id,
			.is_recursive = left_binder->is_recursive
		};
		right_targets[i - 1] = (struct prototype_case_binder){
			.binding_id = right_context->binding_id,
			.is_recursive = right_binder->is_recursive
		};
		current_context = left_context->parent;
	}
	if (current_context != outer_context_id) {
		return 0;
	}
	uint32_t expected_left;
	uint32_t expected_right;
	if (core_match_case_alpha_reindex(
			terms,
			type_declarations,
			&terms->case_binders[source_case->first_binder],
			left_targets,
			binder_count,
			source_case->body,
			&expected_left
		) != 0 || core_match_case_alpha_reindex(
			terms,
			type_declarations,
			&terms->case_binders[source_case->first_binder],
			right_targets,
			binder_count,
			source_case->body,
			&expected_right
		) != 0) {
		return 0;
	}
	uint32_t child_left_type;
	uint32_t child_right_type;
	uint32_t child_left;
	uint32_t child_right;
	int left_equal;
	int right_equal;
	return case_witness->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		prototype_term_relation_type_info(
			terms,
			case_witness->classifier,
			&child_left_type,
			&child_right_type,
			&child_left,
			&child_right
		) == 0 && prototype_term_core_shape_equal(
			terms, child_left, expected_left, &left_equal
		) == 0 && left_equal && prototype_term_core_shape_equal(
			terms, child_right, expected_right, &right_equal
		) == 0 && right_equal;
}

int prototype_judgement_add_relation_match_witness(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t source_match_claim_id,
	uint32_t scrutinee_witness_claim_id,
	const uint32_t* case_witness_claim_ids,
	uint32_t case_witness_count,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* source_match =
		prototype_judgement_claim_get(judgement, source_match_claim_id);
	const struct prototype_judgement_claim* scrutinee =
		prototype_judgement_claim_get(judgement, scrutinee_witness_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t scrutinee_left_type;
	uint32_t scrutinee_right_type;
	uint32_t scrutinee_left;
	uint32_t scrutinee_right;
	if (!judgement || !terms || !type_declarations || !contexts ||
		!case_witness_claim_ids || !p_claim_id ||
		!relation || !left || !right || !source_match || !scrutinee ||
		case_witness_count + 5 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation_left].as.match.case_count != case_witness_count ||
		terms->terms[relation_right].as.match.case_count != case_witness_count ||
		prototype_judgement_proposition_get(judgement, source_match->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, source_match->proposition_id)->subject >= terms->term_count ||
		terms->terms[prototype_judgement_proposition_get(judgement, source_match->proposition_id)->subject].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[prototype_judgement_proposition_get(judgement, source_match->proposition_id)->subject].as.match.case_count != case_witness_count ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != right_type ||
		prototype_judgement_proposition_get(judgement, scrutinee->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, scrutinee->proposition_id)->context_id != context_id ||
		prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, scrutinee->proposition_id)->classifier,
			&scrutinee_left_type,
			&scrutinee_right_type,
			&scrutinee_left,
			&scrutinee_right
		) != 0 || scrutinee_left != terms->terms[relation_left].as.match.scrutinee ||
		scrutinee_right != terms->terms[relation_right].as.match.scrutinee) {
		return -1;
	}
	uint32_t premise_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	premise_ids[0] = relation_type_claim_id;
	premise_ids[1] = left_endpoint_claim_id;
	premise_ids[2] = right_endpoint_claim_id;
	premise_ids[3] = source_match_claim_id;
	premise_ids[4] = scrutinee_witness_claim_id;
	for (uint32_t i = 0; i < case_witness_count; ++i) {
		uint32_t source_case_id =
			terms->terms[prototype_judgement_proposition_get(judgement, source_match->proposition_id)->subject].as.match.first_case + i;
		uint32_t left_case_id = terms->terms[relation_left].as.match.first_case + i;
		uint32_t right_case_id = terms->terms[relation_right].as.match.first_case + i;
		const struct prototype_judgement_claim* case_witness =
			prototype_judgement_claim_get(judgement, case_witness_claim_ids[i]);
		const struct prototype_judgement_proposition* case_proposition =
			prototype_judgement_claim_proposition(
				judgement, case_witness_claim_ids[i]
			);
		uint32_t case_left_type;
		uint32_t case_right_type;
		uint32_t case_left;
		uint32_t case_right;
		if (source_case_id >= terms->case_count || left_case_id >= terms->case_count ||
			right_case_id >= terms->case_count ||
			!case_witness || !case_proposition ||
			!relation_match_case_witness_shape_valid(
				terms,
				type_declarations,
				contexts,
				context_id,
				&terms->cases[source_case_id],
				&terms->cases[left_case_id],
				&terms->cases[right_case_id],
				case_proposition
			)) {
			return -1;
		}
		(void)case_left_type;
		(void)case_right_type;
		(void)case_left;
		(void)case_right;
		premise_ids[i + 5] = case_witness_claim_ids[i];
	}
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = case_witness_count + 5;
	for (uint32_t i = 0; i < proof.premise_count; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_induction_hypothesis_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t source_induction_claim_id,
	uint32_t argument_witness_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* source =
		prototype_judgement_claim_get(judgement, source_induction_claim_id);
	const struct prototype_judgement_claim* argument =
		prototype_judgement_claim_get(judgement, argument_witness_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t argument_left_type;
	uint32_t argument_right_type;
	uint32_t argument_left;
	uint32_t argument_right;
	int source_has_induction_derivation = 0;
	if (judgement) {
		for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
			if (judgement->derivations[i].conclusion_claim_id ==
					source_induction_claim_id && judgement->derivations[i].proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
				source_has_induction_derivation = 1;
				break;
			}
		}
	}
	if (!judgement || !terms || !p_claim_id || !relation || !left || !right ||
		!source || !argument || !source_has_induction_derivation ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != right_type ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->subject >= terms->term_count || terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		prototype_judgement_proposition_get(judgement, argument->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, argument->proposition_id)->context_id != context_id || prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, argument->proposition_id)->classifier,
			&argument_left_type,
			&argument_right_type,
			&argument_left,
			&argument_right
		) != 0 || argument_left != terms->terms[relation_left].
			as.induction_hypothesis.argument || argument_right !=
			terms->terms[relation_right].as.induction_hypothesis.argument) {
		return -1;
	}
	uint32_t premise_ids[5] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		source_induction_claim_id,
		argument_witness_claim_id
	};
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 5;
	for (uint32_t i = 0; i < 5; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

