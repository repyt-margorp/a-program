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
	uint32_t generalized_lambda_classifier;
	uint32_t generalized_pi;
	uint32_t generalized_domain;
	uint32_t generalized_codomain;
	if (classifier_kernel_strip_effect_row_foralls(
			terms,
			type_declarations,
			NULL,
			classifier,
			&generalized_lambda_classifier
		) != 0 || classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			generalized_lambda_classifier,
			&generalized_pi,
			&generalized_domain,
			&generalized_codomain
		) != 0) {
		return -1;
	}
	(void)generalized_pi;
	(void)generalized_codomain;
	if (prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			generalized_domain,
			binder_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* Introduction preserves a row quantified only by the codomain. */
		lambda_classifier = generalized_lambda_classifier;
	} else if (prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			classifier,
			binder_classifier,
			&lambda_classifier
		) != 0) {
		return -1;
	}
	if (classifier_kernel_as_pi(
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
	uint32_t callable_function_classifier;
	if (prototype_judgement_expose_callable_classifier(
			terms,
			type_declarations,
			function_classifier,
			&callable_function_classifier
		) != 0 || prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			callable_function_classifier,
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
	uint32_t solved_domain;
	if (prototype_judgement_solve_expected_effect_rows(
			terms,
			type_declarations,
			NULL,
			domain_whnf,
			argument_classifier,
			&solved_domain
		) != 0) {
		return -1;
	}
	(void)solved_domain;
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
		.classifier = source_evidence->classifier,
		.resource_usage_count = source_evidence->resource_usage_count,
		.resource_usage = source_evidence->resource_usage
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
	proof.premises[0].proposition.resource_usage_count =
		source_evidence->resource_usage_count;
	proof.premises[0].proposition.resource_usage = source_evidence->resource_usage;
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
	evidence->resource_usage_count = prototype_judgement_proposition_get(
		judgement, claim->proposition_id
	)->resource_usage_count;
	evidence->resource_usage = prototype_judgement_proposition_get(
		judgement, claim->proposition_id
	)->resource_usage;
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

static int reindex_resource_usage(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* source,
	uint32_t substitution_id,
	struct prototype_usage_vector* target
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!source || !target) {
		return -1;
	}
	struct prototype_usage_entry replacement_entries[PROTOTYPE_CONTEXT_CAPACITY];
	struct prototype_usage_entry scaled_entries[PROTOTYPE_CONTEXT_CAPACITY];
	struct prototype_usage_entry sum_entries[PROTOTYPE_CONTEXT_CAPACITY];
	struct prototype_usage_vector replacement;
	struct prototype_usage_vector scaled;
	struct prototype_usage_vector sum;
	prototype_usage_vector_init(
		&replacement, replacement_entries, PROTOTYPE_CONTEXT_CAPACITY
	);
	prototype_usage_vector_init(
		&scaled, scaled_entries, PROTOTYPE_CONTEXT_CAPACITY
	);
	prototype_usage_vector_init(&sum, sum_entries, PROTOTYPE_CONTEXT_CAPACITY);
	target->count = 0;
	for (uint32_t i = 0; i < source->resource_usage_count; ++i) {
		uint32_t replacement_term;
		if (prototype_substitution_binding_term(
				terms,
				type_declarations,
				contexts,
				substitutions,
				substitution_id,
				source->resource_usage[i].binding_id,
				&replacement_term
			) != 0 || prototype_term_usage_analyze(
				terms, replacement_term, &replacement
			) != 0 || prototype_usage_vector_scale(
				&scaled, source->resource_usage[i].grade, &replacement
			) != 0 || prototype_usage_vector_add(
				&sum, target, &scaled
			) != 0 || prototype_usage_vector_copy(target, &sum) != 0) {
			return -1;
		}
	}
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
	const struct prototype_judgement_proposition* source_proposition = source ?
		prototype_judgement_proposition_get(
			judgement, source->proposition_id
		) : NULL;
	uint32_t subject;
	uint32_t classifier;
	if (!source || !source_proposition || !substitution || !p_claim_id ||
		(source_proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		 source_proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) ||
		substitution->target_context != source_proposition->context_id) {
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
		subject = source_proposition->subject;
		classifier = source_proposition->classifier;
	}
	struct prototype_usage_entry transformed_entries[PROTOTYPE_CONTEXT_CAPACITY];
	struct prototype_usage_vector transformed_usage;
	prototype_usage_vector_init(
		&transformed_usage, transformed_entries, PROTOTYPE_CONTEXT_CAPACITY
	);
	if ((require_projection ? prototype_usage_vector_copy(
			&transformed_usage,
			&(struct prototype_usage_vector) {
				.entries = (struct prototype_usage_entry*)source_proposition->resource_usage,
				.count = source_proposition->resource_usage_count,
				.capacity = source_proposition->resource_usage_count
			}
		) : reindex_resource_usage(
			terms,
			type_declarations,
			contexts,
			substitutions,
			source_proposition,
			substitution_id,
			&transformed_usage
		)) != 0) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = source_proposition->kind,
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
		.classifier = classifier,
		.resource_usage_count = (uint32_t)transformed_usage.count,
		.resource_usage = transformed_usage.entries
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
	proof.premises[0].proposition.resource_usage_count =
		prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->resource_usage_count;
	proof.premises[0].proposition.resource_usage =
		prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->resource_usage;
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
