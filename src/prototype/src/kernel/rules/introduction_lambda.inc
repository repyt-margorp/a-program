int prototype_judgement_expand_lambda(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement) {
		return -1;
	}
	struct prototype_judgement_proposition relations[4];
	struct prototype_judgement_derivation_candidate proofs[4];
	struct prototype_judgement_candidate_premise premises[
		4 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result motives[1];
	struct prototype_judgement_computation_constraint constraints[1];
	struct prototype_judgement_effect_row_constraint effect_rows[1];
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta, judgement, relations, proofs, 4,
		premises, 4 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motives, 1, constraints, 1, effect_rows, 1
	);
	int status = prototype_judgement_delta_expand_lambda(
		&delta, terms, type_declarations, subject, classifier
	);
	return status == 0 ? prototype_judgement_delta_commit(&delta, 0) : status;
}

int prototype_judgement_delta_expand_lambda(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[subject];
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t binder_classifier;
	uint32_t body_classifier;
	struct prototype_judgement_selected_evidence body_evidence;
	uint32_t expected_body_classifier;
	uint32_t binder_var;
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		classifier,
		&lambda_pi,
		&domain,
		&codomain_family
	);
	(void)codomain_family;
	if (status != 0) {
		return -1;
	}
	if (prototype_term_var(terms, lambda->as.lambda.binding_id, &binder_var) != 0) {
		return -1;
	}
	if (pi_codomain_at_fresh_binder(
			delta,
			terms,
			type_declarations,
			delta->current_context_id,
			lambda_pi,
			binder_var,
			domain,
			PROTOTYPE_INVALID_ID,
			&expected_body_classifier
		) != 0) {
		return -1;
	}
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			lambda->as.lambda.body,
			expected_body_classifier,
			&body_evidence
		) != 0) {
		return -1;
	}
	body_classifier = body_evidence.classifier;
	for (size_t i = delta->proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation = &delta->propositions[i - 1];
		if (candidate_has_derivation_kind(
			delta->propositions,
			delta->proposition_count,
			delta->derivation_candidates,
			delta->derivation_candidate_count,
			(uint32_t)(i - 1),
			PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
		) != 1 ||
			relation->subject >= terms->term_count ||
			terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
			terms->terms[relation->subject].as.var.binding_id != lambda->as.lambda.binding_id) {
			continue;
		}
		binder_classifier = relation->classifier;
		int equal;
		if (classifier_conversion_decision(
				prototype_judgement_classifier_conversion(
					terms, type_declarations, domain, binder_classifier
				),
				&equal
			) != 0) {
			return -1;
		}
			if (!equal) {
				continue;
			}
		uint32_t premise_subjects[2] = {
			relation->subject,
			lambda->as.lambda.body
		};
		uint32_t premise_classifiers[2] = {
			binder_classifier,
			body_classifier
		};
		struct prototype_judgement_selected_evidence premise_evidence[2];
		selected_evidence_from_candidate(relation, &premise_evidence[0]);
		premise_evidence[1] = body_evidence;
			int relation_status = add_delta_relation_with_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				classifier,
				PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
				NULL,
				NULL,
				premise_subjects,
				premise_classifiers,
				premise_evidence,
				2
			);
			return relation_status;
		}
	if (delta->db) {
		for (size_t i = delta->db->proposition_count; i > 0; --i) {
			const struct prototype_judgement_proposition* relation = &delta->db->propositions[i - 1];
			if (candidate_has_derivation_kind(
				delta->db->propositions,
				delta->db->proposition_count,
				delta->db->derivation_candidates,
				delta->db->derivation_candidate_count,
				(uint32_t)(i - 1),
				PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
			) != 1 ||
				relation->subject >= terms->term_count ||
				terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
				terms->terms[relation->subject].as.var.binding_id != lambda->as.lambda.binding_id) {
				continue;
			}
			binder_classifier = relation->classifier;
			int equal;
			if (classifier_conversion_decision(
					prototype_judgement_classifier_conversion(
						terms, type_declarations, domain, binder_classifier
					),
					&equal
				) != 0) {
				return -1;
			}
			if (!equal) {
				continue;
			}
			uint32_t premise_subjects[2] = {
				relation->subject,
				lambda->as.lambda.body
			};
			uint32_t premise_classifiers[2] = {
				binder_classifier,
				body_classifier
			};
			struct prototype_judgement_selected_evidence premise_evidence[2];
			selected_evidence_from_candidate(relation, &premise_evidence[0]);
			premise_evidence[1] = body_evidence;
			int relation_status = add_delta_relation_with_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				classifier,
				PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
				NULL,
				NULL,
				premise_subjects,
				premise_classifiers,
				premise_evidence,
				2
			);
			return relation_status;
		}
	}
	return -1;
}
