int prototype_judgement_delta_record_effect_weaken(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t target_classifier
) {
	if (!delta || !terms || !type_declarations || !source_evidence ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		source_evidence->subject >= terms->term_count ||
		source_evidence->classifier >= terms->term_count ||
		target_classifier >= terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view source;
	struct prototype_term_classifier_view target;
	unsigned source_effects;
	unsigned target_effects;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, source_evidence->classifier, &source
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, target_classifier, &target
		) != 0 || source.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		target.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		target.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, source.result, target.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_effect_row_closed_bits(
			terms, source.effect_row, &source_effects
		) != 0 || prototype_term_effect_row_closed_bits(
			terms, target.effect_row, &target_effects
		) != 0 || (source_effects & target_effects) != source_effects) {
		return -1;
	}
	uint32_t premise_subjects[1] = { source_evidence->subject };
	uint32_t premise_classifiers[1] = { source_evidence->classifier };
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		source_evidence->subject,
		target_classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		source_evidence,
		1
	);
}

int prototype_judgement_delta_record_type_formation(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms, subject, &type_id, arguments, &argument_count
		) != 0 || type_id >= type_declarations->type_count ||
		argument_count > 16) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (argument_count == 0 &&
		type->formation_classifier != PROTOTYPE_INVALID_ID) {
		if (type->formation_classifier == classifier) {
			return add_delta_relation(
				delta,
				PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
				subject,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				classifier,
				PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
			);
		}
		if (prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			type->formation_classifier,
			classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL || add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			subject,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			type->formation_classifier,
			PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
		) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence source_evidence = {
			.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			.authority_id = subject,
			.context_id = delta->current_context_id,
			.operation_id = PROTOTYPE_INVALID_ID,
			.subject = subject,
			.classifier = type->formation_classifier
		};
		return prototype_judgement_delta_add_conversion(
			delta, terms, type_declarations, &source_evidence, classifier
		);
	}
	if (!type_instance_has_known_type(terms, type_declarations, subject) ||
		!term_is_universe_var(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
	);
}

int prototype_judgement_delta_record_pure_primitive_type(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_PURE_PRIMITIVE) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	uint32_t inferred_classifier;
	if (prototype_judgement_pure_primitive_classifier(
			terms,
			type_declarations,
			&terms->terms[subject],
			&inferred_classifier
		) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			inferred_classifier,
			classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO
	);
}

int prototype_judgement_delta_record_effect_operation_type(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	(void)type_declarations;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_EFFECT_OPERATION) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	uint32_t inferred_classifier;
	if (prototype_judgement_effect_operation_classifier(
			terms, &terms->terms[subject], &inferred_classifier
		) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, inferred_classifier, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO
	);
}

int prototype_judgement_delta_record_text_literal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t text;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_TEXT_LITERAL) ||
		!term_exists(terms, classifier) ||
		prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0 ||
		text != classifier) {
		return -1;
	}
	return add_delta_relation(
		delta,
		delta->current_operation_id == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER :
			PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		delta->current_operation_id == PROTOTYPE_INVALID_ID ?
			subject : delta->current_operation_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO
	);
}

int prototype_judgement_delta_record_int_literal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t integer;
	uint32_t integer64;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_exists(terms, classifier) ||
		prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT64, &integer64) != 0) {
		return -1;
	}
	if (classifier != integer64) {
		if (terms->terms[subject].as.int_literal.value < INT32_MIN ||
			terms->terms[subject].as.int_literal.value > INT32_MAX ||
			prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT32, &integer) != 0 ||
			classifier != integer) {
			return -1;
		}
	}
	return add_delta_relation(
		delta,
		delta->current_operation_id == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER :
			PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		delta->current_operation_id == PROTOTYPE_INVALID_ID ?
			subject : delta->current_operation_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO
	);
}

int prototype_judgement_delta_record_int_literal_admissibility(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t admissible_classifier
) {
	uint32_t integer;
	uint32_t integer64;
	if (!source_evidence) {
		return -1;
	}
	if (source_evidence->classifier == admissible_classifier) {
		return 0;
	}
	if (!delta || !terms ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		!term_has_tag(terms, source_evidence->subject, PROTOTYPE_TERM_INT_LITERAL) ||
		source_evidence->classifier >= terms->term_count ||
		prototype_term_make_host_type(
			terms, PROTOTYPE_HOST_TYPE_INT64, &integer64
		) != 0 || prototype_term_make_host_type(
			terms, PROTOTYPE_HOST_TYPE_INT32, &integer
		) != 0 || (source_evidence->classifier != integer64 &&
			source_evidence->classifier != integer) ||
		admissible_classifier != integer || !int_literal_fits_int32(
			terms->terms[source_evidence->subject].as.int_literal.value
		)) {
		return -1;
	}
	uint32_t premise_subject = source_evidence->subject;
	uint32_t premise_classifier = source_evidence->classifier;
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		source_evidence->subject,
		admissible_classifier,
		PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY,
		NULL,
		NULL,
		&premise_subject,
		&premise_classifier,
		source_evidence,
		1
	);
}

int prototype_judgement_record_declaration_fact(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_exists(terms, subject) || !term_exists(terms, classifier)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_DECLARATION);
}

int prototype_judgement_add_expected_type_exposure(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t operation_id,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected,
	uint32_t subject
) {
	if (!judgement || !type_declarations || !source_evidence ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		source_evidence->subject != subject || !term_exists(terms, subject) ||
		!term_exists(terms, expected) ||
		!term_exists(terms, source_evidence->classifier)) {
		return -1;
	}
	if (operation_id == source_evidence->operation_id &&
		context_id == source_evidence->context_id &&
		expected == source_evidence->classifier) {
		return 0;
	}
	struct prototype_term_conversion_result conversion =
		classifier_kernel_conversion(
			terms, type_declarations, NULL, expected, source_evidence->classifier
		);
	int proof_kind;
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONVERSION;
	} else if (conversion.status == PROTOTYPE_TERM_CONVERSION_NOT_EQUAL &&
		classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			NULL,
			expected,
			source_evidence->classifier,
			0
		)) {
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE;
	} else {
		return -1;
	}
	uint32_t premise_subjects[1] = { subject };
	uint32_t premise_classifiers[1] = { source_evidence->classifier };
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = operation_id,
		.subject = subject,
		.classifier = expected
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = operation_id;
	proof.conclusion_subject = subject;
	proof.conclusion_classifier = expected;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 1;
	proof.premises[0].proposition.kind = source_evidence->kind;
	proof.premises[0].proposition.context_id = source_evidence->context_id;
	proof.premises[0].proposition.subject = premise_subjects[0];
	proof.premises[0].proposition.classifier = premise_classifiers[0];
	proof.premises[0].proposition.authority_kind = source_evidence->authority_kind;
	proof.premises[0].proposition.authority_id = source_evidence->authority_id;
	proof.premises[0].proposition.operation_id = source_evidence->operation_id;
	return add_complete_relation(judgement, &relation, &proof);
}

int prototype_judgement_delta_record_expected_type_exposure(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected,
	uint32_t subject
) {
	if (!delta || !type_declarations || !source_evidence ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		source_evidence->subject != subject || !term_exists(terms, subject) ||
		!term_exists(terms, expected) ||
		!term_exists(terms, source_evidence->classifier)) {
		return -1;
	}
	if (delta->current_operation_id == source_evidence->operation_id &&
		expected == source_evidence->classifier) {
		return 0;
	}
	struct prototype_term_conversion_result conversion =
		classifier_kernel_conversion(
			terms, type_declarations, NULL, expected, source_evidence->classifier
		);
	int proof_kind;
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONVERSION;
	} else if (conversion.status == PROTOTYPE_TERM_CONVERSION_NOT_EQUAL &&
		classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			NULL,
			expected,
			source_evidence->classifier,
			0
		)) {
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE;
	} else {
		return -1;
	}
	uint32_t premise_subjects[1] = { subject };
	uint32_t premise_classifiers[1] = { source_evidence->classifier };
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		expected,
		proof_kind,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		source_evidence,
		1
	);
}

int prototype_judgement_delta_record_declaration_fact(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_exists(terms, subject) || !term_exists(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_DECLARATION
	);
}

