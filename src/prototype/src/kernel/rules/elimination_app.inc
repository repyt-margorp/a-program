int prototype_judgement_expand_app(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!judgement) {
		return -1;
	}
	struct prototype_judgement_proposition relations[64];
	struct prototype_judgement_derivation_candidate proofs[64];
	struct prototype_judgement_candidate_premise premises[
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result motives[1];
	struct prototype_judgement_computation_constraint constraints[1];
	struct prototype_judgement_effect_row_constraint effect_rows[1];
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta, judgement, relations, proofs, 64,
		premises, 64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motives, 1, constraints, 1, effect_rows, 1
	);
	int status = prototype_judgement_delta_expand_app(
		&delta, terms, type_declarations, subject, p_classifier
	);
	return status == 0 ? prototype_judgement_delta_commit(&delta, 0) : status;
}

static int infer_lambda_classifier_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	uint32_t argument_classifier
);

static int ensure_lambda_binder_assumption(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t lambda_term,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !term_has_tag(terms, lambda_term, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	uint32_t binder_var;
	if (prototype_term_var(terms, lambda->as.lambda.binding_id, &binder_var) != 0) {
		return -1;
	}
	uint32_t binder_context;
	if (!delta->contexts || prototype_context_extend(
			delta->contexts,
			delta->current_context_id,
			lambda->as.lambda.binding_id,
			argument_classifier,
			PROTOTYPE_INVALID_ID,
			&binder_context
		) != 0) {
		return -1;
	}
	for (size_t i = delta->proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation = &delta->propositions[i - 1];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate_has_derivation_kind(
				delta->propositions,
				delta->proposition_count,
				delta->derivation_candidates,
				delta->derivation_candidate_count,
				(uint32_t)(i - 1),
				PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
			) != 1 ||
			relation->context_id != binder_context ||
			relation->subject != binder_var ||
			relation->classifier != argument_classifier) {
			continue;
		}
		return 0;
	}
	size_t before = delta->proposition_count;
	uint32_t outer_context = delta->current_context_id;
	uint32_t outer_operation = delta->current_operation_id;
	prototype_judgement_delta_set_context(delta, binder_context);
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	if (prototype_judgement_delta_record_context_binding_assumption(
			delta, terms, lambda->as.lambda.binding_id, argument_classifier
		) != 0 || delta->proposition_count != before + 1) {
		prototype_judgement_delta_set_context(delta, outer_context);
		prototype_judgement_delta_set_operation(delta, outer_operation);
		return -1;
	}
	prototype_judgement_delta_set_context(delta, outer_context);
	prototype_judgement_delta_set_operation(delta, outer_operation);
	return 0;
}

int prototype_judgement_delta_expand_app(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!delta || !term_has_tag(terms, subject, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	/* Generic Core inference does not own a typed source occurrence. Operation
	 * reification uses prototype_judgement_delta_record_app_elim() after
	 * selecting the owning Operation explicitly. */
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	const struct prototype_term* app = &terms->terms[subject];
	struct app_classifier_candidate candidates[64];
	uint32_t candidate_count = 0;
	int select_status = collect_delta_app_classifier_candidates(
			delta,
			terms,
			type_declarations,
			app->as.app.function,
			app->as.app.argument,
			candidates,
			64,
			&candidate_count
		);
	if (select_status < 0) {
		return -1;
	}
	if (select_status > 0) {
		uint32_t argument_candidates[32];
		uint32_t argument_candidate_count = 0;
		if (collect_subject_classifiers(
				delta,
				terms,
				type_declarations,
				app->as.app.argument,
				argument_candidates,
				32,
				&argument_candidate_count
			) != 0) {
			return -1;
		}
		if (argument_candidate_count == 0) {
			return 1;
		}
		if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
			for (uint32_t i = 0; i < argument_candidate_count; ++i) {
				(void)infer_lambda_classifier_for_app_argument(
					delta,
					terms,
					type_declarations,
					app->as.app.function,
					argument_candidates[i]
				);
			}
		}
		if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS)) {
			for (uint32_t i = 0; i < argument_candidate_count; ++i) {
				int ih_status =
					prototype_judgement_delta_resolve_induction_hypothesis_for_app_argument(
						delta,
						terms,
						type_declarations,
						app->as.app.function,
						argument_candidates[i]
					);
				if (ih_status < 0) {
					return -1;
				}
				}
			}
			select_status = collect_delta_app_classifier_candidates(
					delta,
					terms,
					type_declarations,
					app->as.app.function,
					app->as.app.argument,
					candidates,
					64,
					&candidate_count
				);
			if (select_status < 0) {
				return -1;
		}
		if (select_status > 0) {
				return 1;
			}
		}
		uint32_t first_classifier = candidates[0].result_classifier;
		if (p_classifier) {
			*p_classifier = first_classifier;
		}
		for (uint32_t i = 0; i < candidate_count; ++i) {
			uint32_t premise_subjects[2] = {
				app->as.app.function,
				app->as.app.argument
			};
		uint32_t premise_classifiers[2] = {
			candidates[i].function_evidence.classifier,
			candidates[i].argument_evidence.classifier
		};
		struct prototype_judgement_selected_evidence premise_evidence[2] = {
			candidates[i].function_evidence,
			candidates[i].argument_evidence
		};
		if (add_delta_relation_with_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				candidates[i].result_classifier,
				PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
				NULL,
				NULL,
				premise_subjects,
				premise_classifiers,
				premise_evidence,
				2
			) != 0) {
				return -1;
			}
		}
		return 0;
	}

static int lambda_body_classifier_matches_binder(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t body,
	uint32_t binding_id,
	uint32_t binder_classifier,
	uint32_t body_classifier
) {
	if (!terms || !type_declarations || body >= terms->term_count) {
		return 0;
	}
	if (terms->terms[body].tag == PROTOTYPE_TERM_VAR &&
		terms->terms[body].as.var.binding_id == binding_id) {
		return (prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			binder_classifier,
			body_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL);
	}
	if (terms->terms[body].tag == PROTOTYPE_TERM_RETURN) {
		uint32_t value = terms->terms[body].as.return_term.value;
		struct prototype_term_classifier_view view;
		if (value >= terms->term_count) {
			return 0;
		}
		if (terms->terms[value].tag != PROTOTYPE_TERM_VAR ||
			terms->terms[value].as.var.binding_id != binding_id) {
			return 1;
		}
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, body_classifier, &view
			) != 0 || view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return 0;
		}
		return (prototype_judgement_classifier_conversion(
			terms, type_declarations, binder_classifier, view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL);
	}
	return 1;
}

static int infer_lambda_classifier_pair(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	const struct prototype_judgement_selected_evidence* binder_evidence,
	const struct prototype_judgement_selected_evidence* body_evidence,
	int* p_changed
) {
	if (!delta || !terms || !type_declarations || !p_changed ||
		!binder_evidence || !body_evidence ||
		binder_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		body_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		binder_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		body_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		lambda_term >= terms->term_count ||
		terms->terms[lambda_term].tag != PROTOTYPE_TERM_LAMBDA ||
		binder_evidence->subject >= terms->term_count ||
		body_evidence->subject >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	uint32_t binder_subject = binder_evidence->subject;
	uint32_t binder_classifier = binder_evidence->classifier;
	uint32_t body_classifier = body_evidence->classifier;
	if (body_evidence->subject != lambda->as.lambda.body ||
		binder_evidence->context_id != body_evidence->context_id) {
		return -1;
	}
	if (!lambda_body_classifier_matches_binder(
			terms,
			type_declarations,
			lambda->as.lambda.body,
			lambda->as.lambda.binding_id,
			binder_classifier,
			body_classifier
		)) {
		return 0;
	}
	uint32_t codomain_family;
	uint32_t classifier;
	/* PI stores a dependent codomain as a binder family.  In the single
	 * TermDB that family is encoded by the canonical pure THUNK/LAMBDA/RETURN
	 * representation, even when its body is a computation classifier.  This
	 * is a binder representation, not an extra Comp({}, PI(...)) wrapper. */
	if (prototype_term_pure_family(
			terms,
			lambda->as.lambda.binding_id,
			body_classifier,
			&codomain_family
		) != 0 ||
		prototype_term_pi_family(
			terms,
			binder_classifier,
			codomain_family,
			&classifier
		) != 0) {
		return -1;
	}
	uint32_t premise_subjects[2] = {
		binder_subject,
		lambda->as.lambda.body
	};
	uint32_t premise_classifiers[2] = {
		binder_classifier,
		body_classifier
	};
	uint32_t premise_context_ids[2] = {
		binder_evidence->context_id,
		body_evidence->context_id
	};
	struct prototype_judgement_selected_evidence premise_evidence[2] = {
		*binder_evidence,
		*body_evidence
	};
	size_t before = delta->proposition_count;
	if (add_delta_relation_with_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			lambda_term,
			classifier,
			PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			premise_context_ids,
			NULL,
			premise_subjects,
			premise_classifiers,
			premise_evidence,
			2
		) != 0) {
		return -1;
	}
	if (delta->proposition_count > before) {
		*p_changed = 1;
	}
	return 0;
}

static int infer_lambda_classifiers_from_body(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	int* p_changed
) {
	if (!delta || !terms || !type_declarations || !p_changed ||
		lambda_term >= terms->term_count ||
		terms->terms[lambda_term].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_proposition* binder_relations =
			source == 0 ? delta->propositions : delta->db->propositions;
		const struct prototype_judgement_derivation_candidate* binder_derivations =
			source == 0 ? delta->derivation_candidates :
				delta->db->derivation_candidates;
		size_t binder_count =
			source == 0 ? delta->proposition_count : delta->db->proposition_count;
		size_t binder_derivation_count = source == 0 ?
			delta->derivation_candidate_count :
			delta->db->derivation_candidate_count;
		for (size_t i = 0; i < binder_count; ++i) {
			const struct prototype_judgement_proposition* binder_relation =
				&binder_relations[i];
			if (binder_relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				candidate_has_derivation_kind(
					binder_relations,
					binder_count,
					binder_derivations,
					binder_derivation_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
				) != 1 ||
				binder_relation->subject >= terms->term_count ||
				terms->terms[binder_relation->subject].tag != PROTOTYPE_TERM_VAR ||
				terms->terms[binder_relation->subject].as.var.binding_id !=
					lambda->as.lambda.binding_id) {
				continue;
			}
			for (int body_source = 0; body_source < 2; ++body_source) {
				const struct prototype_judgement_proposition* body_relations =
					body_source == 0 ? delta->propositions : delta->db->propositions;
				const struct prototype_judgement_derivation_candidate* body_derivations =
					body_source == 0 ? delta->derivation_candidates :
						delta->db->derivation_candidates;
				size_t body_count =
					body_source == 0 ? delta->proposition_count : delta->db->proposition_count;
				size_t body_derivation_count = body_source == 0 ?
					delta->derivation_candidate_count :
					delta->db->derivation_candidate_count;
				for (size_t j = 0; j < body_count; ++j) {
					const struct prototype_judgement_proposition* body_relation =
						&body_relations[j];
					if (body_relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						candidate_has_derivation_other_than(
							body_relations,
							body_count,
							body_derivations,
							body_derivation_count,
							(uint32_t)j,
							PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN
						) != 1 ||
						body_relation->context_id != binder_relation->context_id ||
						body_relation->subject != lambda->as.lambda.body) {
						continue;
					}
					struct prototype_judgement_selected_evidence binder_evidence;
					struct prototype_judgement_selected_evidence body_evidence;
					selected_evidence_from_candidate(
						binder_relation, &binder_evidence
					);
					selected_evidence_from_candidate(body_relation, &body_evidence);
					if (infer_lambda_classifier_pair(
							delta,
							terms,
							type_declarations,
							lambda_term,
							&binder_evidence,
							&body_evidence,
							p_changed
						) != 0) {
						return -1;
					}
				}
			}
		}
	}
	return 0;
}

static int infer_lambda_classifier_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, lambda_term, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	uint32_t saved_operation_id = delta->current_operation_id;
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	if (ensure_lambda_binder_assumption(
			delta, terms, lambda_term, argument_classifier
		) != 0) {
		prototype_judgement_delta_set_operation(delta, saved_operation_id);
		return -1;
	}
	int changed = 0;
	int status = infer_lambda_classifiers_from_body(
		delta,
		terms,
		type_declarations,
		lambda_term,
		&changed
	);
	prototype_judgement_delta_set_operation(delta, saved_operation_id);
	return status;
}

/* A source lambda can have several provisional classifier candidates while the
 * operation graph fixed point is being solved. Consumers with a known input
 * domain must select the compatible Pi candidate, rather than depend on
 * relation insertion order. */
static int select_delta_pi_classifier_for_domain(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected_domain,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !terms || !type_declarations || !selected ||
		subject >= terms->term_count || expected_domain >= terms->term_count) {
		return -1;
	}
	struct prototype_judgement_selected_evidence candidates[32];
	uint32_t candidate_count;
	if (collect_subject_evidence(
			delta,
			subject,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	int found = 0;
	for (uint32_t i = 0; i < candidate_count; ++i) {
		uint32_t pi;
		uint32_t domain;
		uint32_t family;
		if (classifier_kernel_as_pi(
				terms,
				type_declarations,
				NULL,
				candidates[i].classifier,
				&pi,
				&domain,
				&family
			) != 0) {
			continue;
		}
		int domain_equal;
		if (classifier_conversion_decision(
				prototype_judgement_classifier_conversion(
					terms, type_declarations, domain, expected_domain
				),
				&domain_equal
			) != 0) {
			return -1;
		}
		if (!domain_equal) {
			continue;
		}
		if (!found) {
			*selected = candidates[i];
			found = 1;
		} else {
			int classifier_equal;
			if (classifier_conversion_decision(
					prototype_judgement_classifier_conversion(
						terms, type_declarations, selected->classifier, pi
					),
					&classifier_equal
				) != 0) {
				return -1;
			}
			if (classifier_equal) {
				if (!selected_evidence_equal(selected, &candidates[i])) {
					return 2;
				}
				continue;
			}
			uint32_t selected_domain;
			uint32_t selected_family;
			uint32_t selected_family_binder;
			uint32_t selected_body;
			uint32_t candidate_family_binder;
			uint32_t candidate_body;
			struct prototype_term_classifier_view selected_view;
			struct prototype_term_classifier_view candidate_view;
			unsigned selected_effects;
			unsigned candidate_effects;
			if (classifier_kernel_as_pi(
					terms,
					type_declarations,
					NULL,
					selected->classifier,
					NULL,
					&selected_domain,
					&selected_family
				) != 0 || prototype_term_pure_family_parts(
					terms,
					selected_family,
					&selected_family_binder,
					&selected_body
				) != 0 || prototype_term_pure_family_parts(
					terms,
					family,
					&candidate_family_binder,
					&candidate_body
				) != 0 || prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					selected_body,
					&selected_view
				) != 0 || prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					candidate_body,
					&candidate_view
				) != 0 || selected_view.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				candidate_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				selected_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
				candidate_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
				!(prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					selected_view.result,
					candidate_view.result
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_effect_row_closed_bits(
					terms, selected_view.effect_row, &selected_effects
				) != 0 || prototype_term_effect_row_closed_bits(
					terms, candidate_view.effect_row, &candidate_effects
				) != 0) {
				return -1;
			}
			if ((candidate_effects & selected_effects) == candidate_effects) {
				*selected = candidates[i];
			} else if ((selected_effects & candidate_effects) != selected_effects) {
				return -1;
			}
			(void)selected_domain;
			(void)selected_family_binder;
			(void)candidate_family_binder;
		}
	}
	return found ? 0 : 1;
}

