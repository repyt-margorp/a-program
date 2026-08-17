static int dimension_action_info_is_valid(
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t action,
	uint32_t* p_source,
	uint32_t* p_operator_id,
	uint32_t* p_target_dimension
) {
	uint32_t source_dimension;
	if (prototype_term_dimension_action_info(
			terms, action, p_source, p_operator_id
		) != 0 || prototype_dimension_action_term_dimension(
			terms, dimension_operators, *p_source, &source_dimension
		) != 0) {
		return 0;
	}
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(dimension_operators, *p_operator_id);
	if (!operator || operator->source_dimension != source_dimension) {
		return 0;
	}
	*p_target_dimension = operator->target_dimension;
	return 1;
}

static int validate_dimension_action_type_formation_proof(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_proposition* conclusion,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t source;
	uint32_t operator_id;
	uint32_t target_dimension;
	if (!terms || !dimension_operators || !conclusion || !proof ||
		conclusion->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 2 || !dimension_action_info_is_valid(
			terms,
			dimension_operators,
			conclusion->subject,
			&source,
			&operator_id,
			&target_dimension
		)) {
		return -1;
	}
	const struct prototype_judgement_proposition* source_type =
		proof->premises[0].proposition;
	const struct prototype_judgement_proposition* classifier_type =
		proof->premises[1].proposition;
	uint32_t expected_classifier;
	if (!source_type || !classifier_type || source_type->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source_type->context_id != conclusion->context_id ||
		source_type->subject != source || classifier_type->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		classifier_type->context_id != conclusion->context_id ||
		classifier_type->subject != conclusion->classifier ||
		classifier_type->classifier != source_type->classifier ||
		prototype_dimension_action_type_classifier(
			terms,
			dimension_operators,
			source,
			conclusion->subject,
			source_type->classifier,
			target_dimension,
			&expected_classifier
		) != 0 || expected_classifier != conclusion->classifier) {
		return -1;
	}
	(void)operator_id;
	return 0;
}

static int validate_dimension_action_term_proof(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_proposition* conclusion,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t source;
	uint32_t operator_id;
	uint32_t target_dimension;
	if (!terms || !dimension_operators || !conclusion || !proof ||
		conclusion->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 3 || !dimension_action_info_is_valid(
			terms,
			dimension_operators,
			conclusion->subject,
			&source,
			&operator_id,
			&target_dimension
		)) {
		return -1;
	}
	const struct prototype_judgement_proposition* source_term =
		proof->premises[0].proposition;
	const struct prototype_judgement_proposition* source_classifier =
		proof->premises[1].proposition;
	const struct prototype_judgement_proposition* acted_classifier =
		proof->premises[2].proposition;
	uint32_t acted_type;
	uint32_t expected_classifier;
	if (!source_term || !source_classifier || !acted_classifier ||
		source_term->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_term->context_id != conclusion->context_id ||
		source_term->subject != source || source_classifier->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source_classifier->context_id != conclusion->context_id ||
		source_classifier->subject != source_term->classifier ||
		acted_classifier->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		acted_classifier->context_id != conclusion->context_id ||
		acted_classifier->subject != conclusion->classifier ||
		acted_classifier->classifier != source_classifier->classifier ||
		prototype_term_dimension_action(
			terms,
			dimension_operators,
			source_term->classifier,
			operator_id,
			&acted_type
		) != 0 || prototype_dimension_action_term_classifier(
			terms,
			dimension_operators,
			source,
			acted_type,
			target_dimension,
			&expected_classifier
		) != 0 || expected_classifier != conclusion->classifier) {
		return -1;
	}
	return 0;
}

static int validate_dimension_action_constructor_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_proposition* conclusion,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t source;
	uint32_t operator_id;
	uint32_t target_dimension;
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_ordinal;
	uint32_t argument_count;
	if (!terms || !type_declarations || !contexts || !dimension_operators ||
		!conclusion ||
		!proof || conclusion->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 4 || !dimension_action_info_is_valid(
			terms,
			dimension_operators,
			conclusion->subject,
			&source,
			&operator_id,
			&target_dimension
		) || prototype_term_constructor_spine_info(
			terms,
			source,
			&head,
			&owner,
			&constructor_ordinal,
			NULL,
			0,
			&argument_count
		) != 0 || argument_count != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* source_constructor =
		proof->premises[0].proposition;
	const struct prototype_judgement_proposition* source_owner =
		proof->premises[1].proposition;
	const struct prototype_judgement_proposition* acted_owner =
		proof->premises[2].proposition;
	const struct prototype_judgement_proposition* acted_classifier =
		proof->premises[3].proposition;
	uint32_t acted_owner_source;
	uint32_t acted_owner_operator;
	uint32_t expected_classifier;
	struct prototype_type_schema_view type_view;
	if (!source_constructor || !source_owner || !acted_owner ||
		!acted_classifier ||
		source_constructor->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_constructor->context_id != conclusion->context_id ||
		source_constructor->subject != source || source_owner->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_owner->context_id != conclusion->context_id ||
		source_owner->subject != owner || acted_owner->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		acted_owner->context_id != conclusion->context_id ||
		acted_classifier->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		acted_classifier->context_id != conclusion->context_id ||
		acted_classifier->subject != conclusion->classifier ||
		acted_classifier->classifier != source_owner->classifier ||
		prototype_term_dimension_action_info(
			terms,
			acted_owner->subject,
			&acted_owner_source,
			&acted_owner_operator
		) != 0 || acted_owner_source != owner ||
		acted_owner_operator != operator_id || prototype_type_schema_view_query(
			type_declarations,
			contexts,
			terms,
			dimension_operators,
			acted_owner->subject,
			&type_view
		) != 0) {
		return -1;
	}
	if (prototype_constructor_schema_view_action_classifier(
			type_declarations,
			contexts,
			terms,
			dimension_operators,
			&type_view,
			constructor_ordinal,
			&expected_classifier
		) != 0 || expected_classifier != conclusion->classifier) {
		return -1;
	}
	(void)head;
	return 0;
}

static int publish_dimension_action_rule(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* conclusion,
	int proof_kind,
	const uint32_t* premise_claim_ids,
	uint32_t premise_count,
	uint32_t* p_claim_id
) {
	if (!judgement || !conclusion || !premise_claim_ids || !p_claim_id ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_proposition premise_propositions[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	initialize_proof_rule_parameters(&proof, premises, premise_propositions);
	proof.premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, premise_claim_ids[i]);
		const struct prototype_judgement_proposition* proposition = claim ?
			prototype_judgement_proposition_get(
				judgement, claim->proposition_id
			) : NULL;
		if (!proposition) {
			return -1;
		}
		premises[i].proposition_id = claim->proposition_id;
		premises[i].proposition_store_kind =
			PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB;
		premises[i].proposition =
			(struct prototype_judgement_proposition*)proposition;
	}
	return publish_complete_relation(
		judgement,
		conclusion,
		&proof,
		premise_claim_ids,
		p_claim_id
	);
}

int prototype_judgement_add_dimension_action_type_formation(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_dimension_operator_db* dimension_operators,
	uint32_t context_id,
	uint32_t acted_type,
	uint32_t acted_classifier,
	uint32_t source_type_claim_id,
	uint32_t acted_classifier_claim_id,
	uint32_t* p_claim_id
) {
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		.authority_id = acted_type,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = acted_type,
		.classifier = acted_classifier
	};
	uint32_t premise_ids[2] = {
		source_type_claim_id, acted_classifier_claim_id
	};
	struct prototype_judgement_rule_application_view proof;
	struct prototype_judgement_candidate_premise premises[2];
	memset(&proof, 0, sizeof(proof));
	proof.conclusion = &conclusion;
	proof.premise_count = 2;
	for (uint32_t i = 0; i < 2; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		premises[i].proposition = claim ?
			(struct prototype_judgement_proposition*)
			prototype_judgement_proposition_get(
				judgement, claim->proposition_id
			) : NULL;
	}
	proof.premises = premises;
	uint32_t target_dimension;
	uint32_t source;
	uint32_t operator_id;
	if (!dimension_action_info_is_valid(
			terms,
			dimension_operators,
			acted_type,
			&source,
			&operator_id,
			&target_dimension
		) || prototype_dimension_action_prepare_face_operators(
			dimension_operators, target_dimension
		) != 0 || validate_dimension_action_type_formation_proof(
			terms, dimension_operators, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_dimension_action_rule(
		judgement,
		&conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TYPE_FORMATION,
		premise_ids,
		2,
		p_claim_id
	);
}

int prototype_judgement_add_dimension_action_term(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_dimension_operator_db* dimension_operators,
	uint32_t context_id,
	uint32_t acted_term,
	uint32_t acted_classifier,
	uint32_t source_term_claim_id,
	uint32_t source_classifier_claim_id,
	uint32_t acted_classifier_claim_id,
	uint32_t* p_claim_id
) {
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		.authority_id = acted_term,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = acted_term,
		.classifier = acted_classifier
	};
	uint32_t premise_ids[3] = {
		source_term_claim_id,
		source_classifier_claim_id,
		acted_classifier_claim_id
	};
	struct prototype_judgement_rule_application_view proof;
	struct prototype_judgement_candidate_premise premises[3];
	memset(&proof, 0, sizeof(proof));
	proof.conclusion = &conclusion;
	proof.premise_count = 3;
	for (uint32_t i = 0; i < 3; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		premises[i].proposition = claim ?
			(struct prototype_judgement_proposition*)
			prototype_judgement_proposition_get(
				judgement, claim->proposition_id
			) : NULL;
	}
	proof.premises = premises;
	uint32_t target_dimension;
	uint32_t source;
	uint32_t operator_id;
	if (!dimension_action_info_is_valid(
			terms,
			dimension_operators,
			acted_term,
			&source,
			&operator_id,
			&target_dimension
		) || prototype_dimension_action_prepare_face_operators(
			dimension_operators, target_dimension
		) != 0 || validate_dimension_action_term_proof(
			terms, dimension_operators, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_dimension_action_rule(
		judgement,
		&conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TERM,
		premise_ids,
		3,
		p_claim_id
	);
}

int prototype_judgement_add_dimension_action_constructor(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_dimension_operator_db* dimension_operators,
	uint32_t context_id,
	uint32_t acted_constructor,
	uint32_t acted_classifier,
	uint32_t source_constructor_claim_id,
	uint32_t source_owner_claim_id,
	uint32_t acted_owner_claim_id,
	uint32_t acted_constructor_classifier_claim_id,
	const uint32_t* field_claim_ids,
	uint32_t field_claim_count,
	uint32_t* p_claim_id
) {
	if (field_claim_count != 0 || field_claim_ids) {
		return -1;
	}
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		.authority_id = acted_constructor,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = acted_constructor,
		.classifier = acted_classifier
	};
	uint32_t premise_ids[4] = {
		source_constructor_claim_id,
		source_owner_claim_id,
		acted_owner_claim_id,
		acted_constructor_classifier_claim_id
	};
	struct prototype_judgement_rule_application_view proof;
	struct prototype_judgement_candidate_premise premises[4];
	memset(&proof, 0, sizeof(proof));
	proof.conclusion = &conclusion;
	proof.premise_count = 4;
	for (uint32_t i = 0; i < 4; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		premises[i].proposition = claim ?
			(struct prototype_judgement_proposition*)
			prototype_judgement_proposition_get(
				judgement, claim->proposition_id
			) : NULL;
	}
	proof.premises = premises;
	if (validate_dimension_action_constructor_proof(
			terms,
			type_declarations,
			contexts,
			dimension_operators,
			&conclusion,
			&proof
		) != 0) {
		return -1;
	}
	return publish_dimension_action_rule(
		judgement,
		&conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_CONSTRUCTOR,
		premise_ids,
		4,
		p_claim_id
	);
}
