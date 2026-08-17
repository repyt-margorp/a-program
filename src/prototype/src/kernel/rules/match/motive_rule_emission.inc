int prototype_judgement_expand_match_motive(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_is_universe_var(terms, classifier) ||
		terms->terms[subject].as.match.case_count != 0) {
		return -1;
	}
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		NULL,
		NULL,
		NULL,
		0
	);
}

static int prototype_judgement_delta_add_conversion(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected
) {
	if (!delta || !type_declarations || !source_evidence ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		!term_exists(terms, source_evidence->subject) ||
		!term_exists(terms, expected) ||
		!term_exists(terms, source_evidence->classifier)) {
		return -1;
	}
	if (expected == source_evidence->classifier) {
		return 0;
	}
	if (prototype_judgement_classifier_conversion(
			(struct prototype_term_db*)terms,
			type_declarations,
			source_evidence->classifier,
			expected
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	uint32_t premise_subjects[1] = { source_evidence->subject };
	uint32_t premise_classifiers[1] = { source_evidence->classifier };
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		source_evidence->subject,
		expected,
		PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		source_evidence,
		1
	);
}

static int prototype_judgement_delta_add_universe_cumulativity(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_is_universe_var(terms, subject) ||
		!term_is_universe_var(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY
	);
}

int prototype_judgement_delta_ensure_type_at_universe(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t universe
) {
	if (!delta || !terms || !type_declarations ||
		!term_exists(terms, subject) ||
		!term_is_universe_var(terms, universe)) {
		return -1;
	}
	struct prototype_judgement_selected_evidence existing_evidence;
	if (lookup_delta_prior_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			subject,
			universe,
			&existing_evidence
		) == 0) {
		return 0;
	}
	if (term_is_universe_var(terms, subject)) {
		return prototype_judgement_delta_add_universe_cumulativity(
			delta,
			terms,
			subject,
			universe
		);
	}
	if (type_instance_has_known_type(terms, type_declarations, subject)) {
		return prototype_judgement_delta_record_type_formation(
			delta, terms, type_declarations, subject, universe
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_COMPUTATION_TYPE)) {
		const struct prototype_term* computation_type = &terms->terms[subject];
		struct prototype_effect_row_normal_form effect_row;
		if (prototype_term_effect_row_normal_form(
				terms, computation_type->as.computation_type.label, &effect_row
			) != 0 || prototype_judgement_delta_ensure_type_at_universe(
				delta,
				terms,
				type_declarations,
				computation_type->as.computation_type.result,
				universe
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence result_evidence;
		if (lookup_delta_prior_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				computation_type->as.computation_type.result,
				universe,
				&result_evidence
			) != 0) {
			return -1;
		}
		uint32_t premise_subjects[1] = {
			computation_type->as.computation_type.result
		};
		uint32_t premise_classifiers[1] = { universe };
		return add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			subject,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			universe,
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION,
			NULL,
			NULL,
			premise_subjects,
			premise_classifiers,
			&result_evidence,
			1
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_THUNK_TYPE)) {
		uint32_t computation = terms->terms[subject].as.thunk_type.computation;
		if (prototype_judgement_delta_ensure_type_at_universe(
				delta, terms, type_declarations, computation, universe
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence computation_evidence;
		if (lookup_delta_prior_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				computation,
				universe,
				&computation_evidence
			) != 0) {
			return -1;
		}
		uint32_t premise_subjects[1] = { computation };
		uint32_t premise_classifiers[1] = { universe };
		return add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			subject,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			universe,
			PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION,
			NULL,
			NULL,
			premise_subjects,
			premise_classifiers,
			&computation_evidence,
			1
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_PI)) {
		uint32_t domain;
		uint32_t codomain_family;
		uint32_t codomain_lambda;
		if (pi_parts(terms, subject, &domain, &codomain_family) != 0) {
			return -1;
		}
		if (prototype_term_pure_family_lambda(
				terms,
				codomain_family,
				&codomain_lambda
			) != 0) {
			return -1;
		}
		uint32_t codomain_binder;
		uint32_t codomain;
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[subject].as.pi.codomain_family,
				&codomain_binder,
				&codomain
			) != 0) {
			return -1;
		}
		uint32_t outer_context = delta->current_context_id;
		uint32_t outer_operation = delta->current_occurrence_id;
		if (ensure_lambda_binder_assumption(
				delta, terms, codomain_lambda, domain
			) != 0 || prototype_judgement_delta_ensure_type_at_universe(
				delta,
				terms,
				type_declarations,
				domain,
				universe
			) != 0) {
			return -1;
		}
		uint32_t binder_context;
		if (!delta->contexts || prototype_context_extend(
				delta->contexts,
				outer_context,
				codomain_binder,
				domain,
				PROTOTYPE_INVALID_ID,
				&binder_context
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence outer_codomain_evidence;
		int has_outer_codomain_evidence =
			lookup_delta_prior_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				codomain,
				universe,
				&outer_codomain_evidence
			) == 0;
		uint32_t binder_projection = PROTOTYPE_INVALID_ID;
		if (has_outer_codomain_evidence &&
			(!delta->substitutions || prototype_substitution_projection_path(
				delta->substitutions,
				delta->contexts,
				binder_context,
				outer_context,
				&binder_projection
			) != 0)) {
			return -1;
		}
		prototype_judgement_delta_set_context(delta, binder_context);
		prototype_judgement_delta_set_occurrence(delta, PROTOTYPE_INVALID_ID);
		if ((has_outer_codomain_evidence &&
			 prototype_judgement_delta_record_context_weaken(
				delta, &outer_codomain_evidence, binder_projection
			 ) != 0) || prototype_judgement_delta_ensure_type_at_universe(
				delta,
				terms,
				type_declarations,
				codomain,
				universe
			) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_occurrence(delta, outer_operation);
			return -1;
		}
		uint32_t lambda_universe_family;
		uint32_t lambda_classifier;
		uint32_t returned;
		uint32_t empty_effects;
		uint32_t returned_classifier;
		uint32_t family_classifier;
		if (prototype_term_effect_row_empty(terms, &empty_effects
				) != 0 || prototype_term_computation_type(
					terms, empty_effects, universe, &returned_classifier
				) != 0 || prototype_term_pure_family(
					terms, codomain_binder, returned_classifier, &lambda_universe_family
				) != 0 || prototype_term_pi_family(
					terms, domain, lambda_universe_family, &lambda_classifier
				) != 0 || prototype_term_thunk_type(
					terms, lambda_classifier, &family_classifier
				) != 0 ||
			codomain_family >= terms->term_count ||
			terms->terms[subject].as.pi.codomain_family >= terms->term_count) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_occurrence(delta, outer_operation);
			return -1;
		}
		uint32_t family = terms->terms[subject].as.pi.codomain_family;
		returned = terms->terms[family].as.thunk.computation;
		if (returned >= terms->term_count ||
			terms->terms[returned].tag != PROTOTYPE_TERM_LAMBDA ||
			terms->terms[returned].as.lambda.binding_id != codomain_binder ||
			terms->terms[returned].as.lambda.body >= terms->term_count ||
			terms->terms[terms->terms[returned].as.lambda.body].tag != PROTOTYPE_TERM_RETURN ||
			terms->terms[terms->terms[returned].as.lambda.body].as.return_term.value != codomain) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_occurrence(delta, outer_operation);
			return -1;
		}
		uint32_t returned_body = terms->terms[returned].as.lambda.body;
		uint32_t returned_binder;
		if (prototype_term_var(terms, codomain_binder, &returned_binder) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_occurrence(delta, outer_operation);
			return -1;
		}
		struct prototype_judgement_selected_evidence codomain_evidence;
		if (lookup_delta_prior_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				codomain,
				universe,
				&codomain_evidence
			) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_occurrence(delta, outer_operation);
			return -1;
		}
		uint32_t return_subjects[1] = { codomain };
		uint32_t return_classifiers[1] = { codomain_evidence.classifier };
		if (add_delta_relation_with_authority_and_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
				returned_body,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				returned_body,
				returned_classifier,
				PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
				NULL,
				NULL,
				return_subjects,
				return_classifiers,
				&codomain_evidence,
				1
			) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_occurrence(delta, outer_operation);
			return -1;
		}
		struct prototype_judgement_selected_evidence binder_evidence;
		struct prototype_judgement_selected_evidence body_evidence;
		int binder_evidence_status = lookup_delta_subject_local_evidence_exact(
			delta,
			delta->current_context_id,
			returned_binder,
			domain,
			&binder_evidence
		);
		if (binder_evidence_status == 1) {
			binder_evidence_status = prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				delta->current_context_id,
				returned_binder,
				domain,
				&binder_evidence
			);
		}
		int body_evidence_status = lookup_delta_subject_local_evidence_exact(
			delta,
			delta->current_context_id,
			returned_body,
			returned_classifier,
			&body_evidence
		);
		if (body_evidence_status == 1) {
			body_evidence_status = prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				delta->current_context_id,
				returned_body,
				returned_classifier,
				&body_evidence
			);
		}
		if (binder_evidence_status != 0 || body_evidence_status != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_occurrence(delta, outer_operation);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, outer_context);
		prototype_judgement_delta_set_occurrence(delta, outer_operation);
		if (prototype_judgement_delta_record_lambda_intro(
				delta,
				terms,
				type_declarations,
				PROTOTYPE_INVALID_ID,
				returned,
				lambda_classifier,
				returned_binder,
				returned_body,
				&binder_evidence,
				PROTOTYPE_INVALID_ID,
				&body_evidence
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence lambda_evidence;
		if (prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				delta->current_context_id,
				returned,
				lambda_classifier,
				&lambda_evidence
			) != 0) {
			return -1;
		}
		uint32_t thunk_subjects[1] = { returned };
		uint32_t thunk_classifiers[1] = { lambda_classifier };
		if (add_delta_relation_with_authority_and_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
				family,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				family,
				family_classifier,
				PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
				NULL,
				NULL,
				thunk_subjects,
				thunk_classifiers,
				&lambda_evidence,
				1
			) != 0) {
			return -1;
		}
		uint32_t premise_subjects[3] = {
			domain,
			codomain,
			family
		};
		uint32_t premise_classifiers[3] = {
			universe,
			universe,
			family_classifier
		};
		struct prototype_judgement_selected_evidence formation_evidence[3];
		int domain_evidence_status = lookup_delta_subject_local_evidence_exact(
			delta,
			outer_context,
			domain,
			universe,
			&formation_evidence[0]
		);
		if (domain_evidence_status == 1) {
			domain_evidence_status = prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				outer_context,
				domain,
				universe,
				&formation_evidence[0]
			);
		}
		int codomain_evidence_status = lookup_delta_subject_local_evidence_exact(
			delta,
			binder_context,
			codomain,
			universe,
			&formation_evidence[1]
		);
		if (codomain_evidence_status == 1) {
			codomain_evidence_status = prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				binder_context,
				codomain,
				universe,
				&formation_evidence[1]
			);
		}
		if (domain_evidence_status != 0 || codomain_evidence_status != 0 ||
			prototype_judgement_delta_select_evidence(
				delta, PROTOTYPE_INVALID_ID, delta->current_context_id,
				family, family_classifier, &formation_evidence[2]
			) != 0) {
			return -1;
		}
		return add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			subject,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			universe,
			PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO,
			NULL,
			NULL,
			premise_subjects,
			premise_classifiers,
			formation_evidence,
			3
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_APP)) {
		uint32_t inferred;
		int status = prototype_judgement_delta_expand_app(
			delta,
			terms,
			type_declarations,
			subject,
			&inferred
		);
		if (status < 0) {
			return -1;
		}
			if (status > 0) {
				const struct prototype_term* app = &terms->terms[subject];
				if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
					uint32_t argument_classifiers[32];
					uint32_t argument_classifier_count = 0;
					if (collect_subject_classifiers(
							delta,
							terms,
							type_declarations,
							app->as.app.argument,
							argument_classifiers,
							32,
							&argument_classifier_count
						) != 0) {
						return -1;
					}
					for (uint32_t i = 0; i < argument_classifier_count; ++i) {
						uint32_t argument_classifier = argument_classifiers[i];
						const struct prototype_term* lambda = &terms->terms[app->as.app.function];
						uint32_t binder_var;
						if (prototype_term_var(
								terms,
								lambda->as.lambda.binding_id,
								&binder_var
							) != 0 ||
							ensure_lambda_binder_assumption(
								delta,
								terms,
								app->as.app.function,
								argument_classifier
							) != 0) {
							return -1;
						}
						if (prototype_judgement_delta_ensure_type_at_universe(
								delta,
								terms,
								type_declarations,
								lambda->as.lambda.body,
								universe
							) != 0) {
							return -1;
						}
						uint32_t classifier;
						if (prototype_term_pi(
								terms,
								argument_classifier,
								universe,
								&classifier
							) != 0) {
							return -1;
						}
						uint32_t premise_subjects[2] = {
							binder_var,
							lambda->as.lambda.body
						};
						uint32_t premise_classifiers[2] = {
							argument_classifier,
							universe
						};
						struct prototype_judgement_selected_evidence premise_evidence[2];
					if (lookup_delta_prior_evidence_normalization_equal(
								delta,
								terms,
								type_declarations,
								binder_var,
								argument_classifier,
								&premise_evidence[0]
						) != 0 || lookup_delta_prior_evidence_normalization_equal(
								delta,
								terms,
								type_declarations,
								lambda->as.lambda.body,
								universe,
								&premise_evidence[1]
							) != 0 || add_delta_relation_with_authority_and_explicit_premises(
								delta,
								PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
								app->as.app.function,
								PROTOTYPE_INVALID_ID,
								PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
								app->as.app.function,
								classifier,
								PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
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
				}
			if (prototype_judgement_delta_infer_core_helper_facts(
					delta,
					terms,
					type_declarations
				) != 0) {
				return -1;
			}
			status = prototype_judgement_delta_expand_app(
				delta,
				terms,
				type_declarations,
				subject,
				&inferred
			);
			if (status < 0) {
				return -1;
			}
		}
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH)) {
		const struct prototype_term* match = &terms->terms[subject];
		if (match->as.match.case_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		struct prototype_judgement_selected_evidence premise_evidence[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		];
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			uint32_t case_id = match->as.match.first_case + i;
			if (case_id >= terms->case_count) {
				return -1;
			}
			uint32_t body = terms->cases[case_id].body;
			if (prototype_judgement_delta_ensure_type_at_universe(
					delta,
					terms,
					type_declarations,
					body,
					universe
				) != 0) {
				return -1;
			}
			premise_subjects[i] = body;
			premise_classifiers[i] = universe;
			if (lookup_delta_prior_evidence_normalization_equal(
					delta,
					terms,
					type_declarations,
					body,
					universe,
					&premise_evidence[i]
				) != 0) {
				return -1;
			}
		}
		if (prototype_judgement_delta_expand_match_motive_with_premises(
				delta,
				terms,
				subject,
				universe,
				premise_subjects,
				premise_classifiers,
				premise_evidence,
				match->as.match.case_count
			) != 0) {
			return -1;
		}
	}
	uint32_t actual;
	uint32_t actual_candidates[32];
	uint32_t actual_candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			subject,
			actual_candidates,
			32,
			&actual_candidate_count
		) != 0) {
		return -1;
	}
	actual = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < actual_candidate_count; ++i) {
		if (!prototype_judgement_classifier_compatible(
				terms,
				type_declarations,
				universe,
				actual_candidates[i]
			)) {
			continue;
		}
		if (actual != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				actual,
				actual_candidates[i]
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		actual = actual_candidates[i];
	}
	if (actual == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (actual == universe) {
		return 0;
	}
	struct prototype_judgement_selected_evidence source_evidence;
	if (prototype_judgement_delta_select_evidence(
			delta,
			delta->current_occurrence_id,
			delta->current_context_id,
			subject,
			actual,
			&source_evidence
		) != 0) {
		return -1;
	}
	return prototype_judgement_delta_add_conversion(
		delta,
		terms,
		type_declarations,
		&source_evidence,
		universe
	);
}

static uint32_t structural_type_formation_claim_exact(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe
) {
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	for (size_t i = judgement->claim_count; i > 0; --i) {
		uint32_t claim_id = (uint32_t)i - 1;
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, claim_id);
		if (proposition && proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			proposition->context_id == context_id && proposition->subject == subject &&
			proposition->classifier == universe) {
			return claim_id;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static int materialize_type_formation_from_is_type(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t is_type_claim_id,
	size_t remaining_depth,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_proposition* is_type =
		prototype_judgement_claim_proposition(judgement, is_type_claim_id);
	if (!is_type || !p_claim_id || remaining_depth == 0 ||
		is_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) {
		return -1;
	}
	uint32_t existing = structural_type_formation_claim_exact(
		judgement, is_type->context_id, is_type->subject, is_type->classifier
	);
	if (existing != PROTOTYPE_INVALID_ID) {
		*p_claim_id = existing;
		return 0;
	}
	size_t derivation_count = judgement->derivation_count;
	for (uint32_t i = 0; i < derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			prototype_judgement_derivation_get(judgement, i);
		if (!derivation || derivation->conclusion_claim_id != is_type_claim_id ||
			derivation->premise_count != 1 ||
			derivation->premises[0].claim_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t premise_claim_id = derivation->premises[0].claim_id;
		const struct prototype_judgement_proposition* premise =
			prototype_judgement_claim_proposition(judgement, premise_claim_id);
		if (derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE && premise &&
			premise->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			premise->context_id == is_type->context_id &&
			premise->subject == is_type->subject &&
			premise->classifier == is_type->classifier) {
			*p_claim_id = premise_claim_id;
			return 0;
		}
		if ((derivation->proof_kind !=
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX &&
			 derivation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN) ||
			derivation->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			!premise || premise->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) {
			continue;
		}
		uint32_t substitution_id = derivation->semantic_action_id;
		int proof_kind = derivation->proof_kind;
		uint32_t source_formation_claim_id;
		if (materialize_type_formation_from_is_type(
				judgement,
				terms,
				type_declarations,
				contexts,
				substitutions,
				premise_claim_id,
				remaining_depth - 1,
				&source_formation_claim_id
			) != 0) {
			fprintf(
				stderr,
				"formation materialize source failed is=%u premise=%u rule=%d subst=%u\n",
				is_type_claim_id,
				premise_claim_id,
				proof_kind,
				substitution_id
			);
			continue;
		}
		uint32_t reindexed_claim_id;
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				type_declarations,
				contexts,
				substitutions,
				source_formation_claim_id,
				substitution_id,
				&reindexed_claim_id
			) != 0) {
			fprintf(
				stderr,
				"formation materialize reindex failed is=%u source-has=%u subst=%u\n",
				is_type_claim_id,
				source_formation_claim_id,
				substitution_id
			);
			return -1;
		}
		const struct prototype_judgement_proposition* reindexed =
			prototype_judgement_claim_proposition(judgement, reindexed_claim_id);
		if (!reindexed || reindexed->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			reindexed->context_id != is_type->context_id ||
			reindexed->subject != is_type->subject ||
			reindexed->classifier != is_type->classifier) {
			fprintf(
				stderr,
				"formation materialize mismatch is=%u result=%u context=%u/%u "
				"subject=%u/%u classifier=%u/%u\n",
				is_type_claim_id,
				reindexed_claim_id,
				reindexed ? reindexed->context_id : PROTOTYPE_INVALID_ID,
				is_type->context_id,
				reindexed ? reindexed->subject : PROTOTYPE_INVALID_ID,
				is_type->subject,
				reindexed ? reindexed->classifier : PROTOTYPE_INVALID_ID,
				is_type->classifier
			);
			return -1;
		}
		*p_claim_id = reindexed_claim_id;
		return 0;
	}
	return 1;
}

static int reuse_ancestor_type_formation_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe,
	uint32_t* p_claim_id
) {
	uint32_t existing = structural_type_formation_claim_exact(
		judgement, context_id, subject, universe
	);
	if (existing != PROTOTYPE_INVALID_ID) {
		*p_claim_id = existing;
		return 0;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (!proposition || proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
			proposition->context_id != context_id || proposition->subject != subject ||
			proposition->classifier != universe) {
			continue;
		}
		int materialize_status = materialize_type_formation_from_is_type(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			i,
			judgement->claim_count + 1,
			p_claim_id
		);
		if (materialize_status <= 0) {
			return materialize_status;
		}
	}
	const struct prototype_context* context = prototype_context_get(contexts, context_id);
	uint32_t ancestor = context ? context->parent : PROTOTYPE_INVALID_ID;
	while (ancestor != PROTOTYPE_INVALID_ID) {
		uint32_t source_claim_id = structural_type_formation_claim_exact(
			judgement, ancestor, subject, universe
		);
		if (source_claim_id != PROTOTYPE_INVALID_ID) {
			uint32_t projection;
			uint32_t reindexed_claim_id;
			if (prototype_substitution_projection_path(
					substitutions, contexts, context_id, ancestor, &projection
				) != 0 || prototype_judgement_add_reindexed_claim(
					judgement,
					terms,
					type_declarations,
					contexts,
					substitutions,
					source_claim_id,
					projection,
					&reindexed_claim_id
				) != 0) {
				return -1;
			}
			const struct prototype_judgement_proposition* reindexed =
				prototype_judgement_claim_proposition(judgement, reindexed_claim_id);
			if (!reindexed || reindexed->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				reindexed->context_id != context_id || reindexed->subject != subject ||
				reindexed->classifier != universe) {
				return -1;
			}
			*p_claim_id = reindexed_claim_id;
			return 0;
		}
		context = prototype_context_get(contexts, ancestor);
		ancestor = context ? context->parent : PROTOTYPE_INVALID_ID;
	}
	return 1;
}

int prototype_judgement_add_structural_type_formation_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe,
	uint32_t* p_claim_id
) {
	enum { CAPACITY = 512 };
	if (!judgement || !terms || !type_declarations || !contexts || !substitutions ||
		!p_claim_id) {
		return -1;
	}
	int reuse_status = reuse_ancestor_type_formation_claim(
		judgement,
		terms,
		type_declarations,
		contexts,
		substitutions,
		context_id,
		subject,
		universe,
		p_claim_id
	);
	if (reuse_status <= 0) {
		return reuse_status;
	}
	struct prototype_judgement_proposition* propositions = calloc(
		CAPACITY, sizeof(*propositions)
	);
	struct prototype_judgement_derivation_candidate* derivations = calloc(
		CAPACITY, sizeof(*derivations)
	);
	struct prototype_judgement_candidate_premise* premises = calloc(
		CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES, sizeof(*premises)
	);
	struct prototype_judgement_match_motive_result* motives = calloc(
		CAPACITY, sizeof(*motives)
	);
	struct prototype_judgement_computation_constraint* constraints = calloc(
		CAPACITY, sizeof(*constraints)
	);
	struct prototype_judgement_effect_row_constraint* effect_rows = calloc(
		CAPACITY, sizeof(*effect_rows)
	);
	if (!propositions || !derivations || !premises || !motives || !constraints ||
		!effect_rows) {
		free(propositions);
		free(derivations);
		free(premises);
		free(motives);
		free(constraints);
		free(effect_rows);
		return -1;
	}
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta,
		judgement,
		propositions,
		derivations,
		CAPACITY,
		premises,
		CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motives,
		CAPACITY,
		constraints,
		CAPACITY,
		effect_rows,
		CAPACITY
	);
	prototype_judgement_delta_set_context_store(&delta, contexts, substitutions);
	prototype_judgement_delta_set_context(&delta, context_id);
	int status = prototype_judgement_delta_ensure_type_at_universe(
		&delta, terms, type_declarations, subject, universe
	);
	int formation_stage = 1;
	if (status == 0) {
		formation_stage = 2;
		status = prototype_judgement_delta_publish_complete(&delta);
	}
	if (status == 0) {
		formation_stage = 3;
		status = 1;
		for (size_t i = judgement->claim_count; i > 0; --i) {
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(judgement, (uint32_t)i - 1);
			if (proposition && proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				proposition->context_id == context_id && proposition->subject == subject &&
				proposition->classifier == universe) {
				*p_claim_id = (uint32_t)i - 1;
				status = 0;
				break;
			}
		}
	}
	if (status != 0) {
		fprintf(
			stderr,
			"structural formation failed subject=%u tag=%d context=%u universe=%u "
			"stage=%d status=%d delta-propositions=%zu delta-derivations=%zu\n",
			subject,
			subject < terms->term_count ? terms->terms[subject].tag : -1,
			context_id,
			universe,
			formation_stage,
			status,
			delta.proposition_count,
			delta.derivation_candidate_count
		);
		for (size_t i = 0; i < delta.proposition_count; ++i) {
			fprintf(
				stderr,
				"  delta proposition=%zu kind=%d context=%u subject=%u classifier=%u\n",
				i,
				delta.propositions[i].kind,
				delta.propositions[i].context_id,
				delta.propositions[i].subject,
				delta.propositions[i].classifier
			);
		}
		for (size_t i = 0; i < delta.derivation_candidate_count; ++i) {
			fprintf(
				stderr,
				"  delta derivation=%zu rule=%d conclusion=%u premises=%u\n",
				i,
				delta.derivation_candidates[i].proof_kind,
				delta.derivation_candidates[i].conclusion_proposition_id,
				delta.derivation_candidates[i].premise_count
			);
		}
	}
	free(propositions);
	free(derivations);
	free(premises);
	free(motives);
	free(constraints);
	free(effect_rows);
	return status;
}

static int prototype_judgement_delta_expand_match_motive_with_premises(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
) {
	if (!delta || !premise_subjects || !premise_classifiers ||
		!premise_evidence) {
		return -1;
	}
	if (!term_exists(terms, subject) ||
		!term_is_universe_var(terms, classifier) ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		premise_count != terms->terms[subject].as.match.case_count) {
		return -1;
	}
	for (uint32_t i = 0; i < premise_count; ++i) {
		uint32_t case_id = terms->terms[subject].as.match.first_case + i;
		if (case_id >= terms->case_count ||
			premise_subjects[i] != terms->cases[case_id].body ||
			premise_classifiers[i] != classifier ||
			premise_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			premise_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
			premise_evidence[i].subject != premise_subjects[i] ||
			premise_evidence[i].classifier != premise_classifiers[i]) {
			return -1;
		}
	}
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		subject,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		premise_count
	);
}

int prototype_judgement_delta_expand_match_motive(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_is_universe_var(terms, classifier) ||
		terms->terms[subject].as.match.case_count != 0) {
		return -1;
	}
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		subject,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		0
	);
}

int prototype_judgement_delta_build_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	const struct prototype_match_case_input* motive_cases,
	uint32_t case_count,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !motive_cases || !p_motive_result ||
		scrutinee >= terms->term_count ||
		scrutinee_classifier >= terms->term_count ||
		case_count > 64) {
		return -1;
	}
	(void)type_declarations;

	uint32_t motive_binder_id = prototype_term_new_binding(terms);
	uint32_t motive_binder_var;
	uint32_t motive_body;
	uint32_t motive_lambda;
	uint32_t motive_pi;
	uint32_t motive_universe;
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_selected_evidence premise_evidence[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (motive_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (prototype_term_var(
		terms,
		motive_binder_id,
		&motive_binder_var
	) != 0) {
		return -1;
	}
	if (prototype_term_match(
		terms,
		motive_binder_var,
		motive_cases,
		case_count,
		&motive_body
	) != 0) {
		return -1;
	}
	if (prototype_term_lambda(
			terms,
			motive_binder_id,
			motive_body,
			&motive_lambda
		) != 0 || motive_lambda >= terms->term_count ||
		terms->terms[motive_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	motive_binder_id = terms->terms[motive_lambda].as.lambda.binding_id;
	motive_body = terms->terms[motive_lambda].as.lambda.body;
	if (prototype_term_var(
			terms,
			motive_binder_id,
			&motive_binder_var
		) != 0) {
		return -1;
	}
	uint32_t outer_context = delta->current_context_id;
	uint32_t outer_operation = delta->current_occurrence_id;
	uint32_t motive_context;
	if (!delta->contexts || prototype_context_extend(
			delta->contexts,
			outer_context,
			motive_binder_id,
			scrutinee_classifier,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_context(delta, motive_context);
	prototype_judgement_delta_set_occurrence(delta, PROTOTYPE_INVALID_ID);
	if (prototype_judgement_delta_record_context_binding_assumption(
			delta,
			terms,
			motive_binder_id,
			scrutinee_classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t case_id = terms->terms[motive_body].as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* motive_case = &terms->cases[case_id];
		for (uint32_t j = 0; j < motive_case->binder_count; ++j) {
			const struct prototype_case_binder* binder =
				&terms->case_binders[motive_case->first_binder + j];
			uint32_t binder_var;
			uint32_t binder_classifier;
			if (prototype_term_var(
					terms,
					binder->binding_id,
					&binder_var
				) != 0 ||
				constructor_field_classifier_from_spine(
					delta,
					terms,
					type_declarations,
					scrutinee_classifier,
					motive_case->constructor_id,
					&terms->case_binders[motive_case->first_binder],
					j,
					j,
					&binder_classifier
				) != 0 ||
				prototype_judgement_delta_record_match_pattern(
					delta,
					terms,
					binder_var,
					binder_classifier,
					scrutinee_classifier,
					motive_case->constructor_id,
					j,
					PROTOTYPE_INVALID_ID
			) != 0) {
				return -1;
			}
		}
	}
	if (prototype_term_universe_var(
		terms,
		universe_level_var,
		&motive_universe
	) != 0) {
		return -1;
	}
	if (case_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t case_id = terms->terms[motive_body].as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		uint32_t motive_case_body = terms->cases[case_id].body;
			if (prototype_judgement_delta_ensure_type_at_universe(
					delta,
					terms,
					type_declarations,
					motive_case_body,
					motive_universe
				) != 0) {
				return -1;
			}
		premise_subjects[i] = motive_case_body;
		premise_classifiers[i] = motive_universe;
		if (lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				motive_case_body,
				motive_universe,
				&premise_evidence[i]
			) != 0) {
			return -1;
		}
	}
		if (prototype_judgement_delta_expand_match_motive_with_premises(
			delta,
			terms,
		motive_body,
		motive_universe,
			premise_subjects,
			premise_classifiers,
			premise_evidence,
			case_count
		) != 0) {
			return -1;
		}
	if (prototype_term_pi(
		terms,
		scrutinee_classifier,
		motive_universe,
		&motive_pi
	) != 0) {
		return -1;
	}
	uint32_t lambda_premise_subjects[2] = {
		motive_binder_var,
		motive_body
	};
	uint32_t lambda_premise_classifiers[2] = {
		scrutinee_classifier,
		motive_universe
	};
	struct prototype_judgement_selected_evidence lambda_premise_evidence[2];
	uint32_t lambda_premise_context_ids[2] = {
		motive_context,
		motive_context
	};
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			motive_binder_var,
			scrutinee_classifier,
			&lambda_premise_evidence[0]
		) != 0 || lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			motive_body,
			motive_universe,
			&lambda_premise_evidence[1]
		) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_context(delta, outer_context);
	prototype_judgement_delta_set_occurrence(delta, outer_operation);
	if (add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			motive_lambda,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			motive_lambda,
			motive_pi,
			PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			lambda_premise_context_ids,
			NULL,
			lambda_premise_subjects,
			lambda_premise_classifiers,
			lambda_premise_evidence,
			2
		) != 0) {
		return -1;
	}
	struct prototype_judgement_selected_evidence lambda_evidence;
	if (prototype_judgement_delta_select_evidence(
			delta,
			PROTOTYPE_INVALID_ID,
			delta->current_context_id,
			motive_lambda,
			motive_pi,
			&lambda_evidence
		) != 0) {
		return -1;
	}
	if (prototype_term_app(terms, motive_lambda, scrutinee, p_motive_result) != 0) {
		return -1;
	}
	uint32_t app_premise_subjects[2] = {
		motive_lambda,
		scrutinee
	};
	uint32_t app_premise_classifiers[2] = {
		motive_pi,
		scrutinee_classifier
	};
	struct prototype_judgement_selected_evidence app_premise_evidence[2];
	app_premise_evidence[0] = lambda_evidence;
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			scrutinee,
			scrutinee_classifier,
			&app_premise_evidence[1]
		) != 0) {
		return -1;
	}
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		*p_motive_result,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		*p_motive_result,
		motive_universe,
		PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
		NULL,
		NULL,
		app_premise_subjects,
		app_premise_classifiers,
		app_premise_evidence,
		2
	);
}

int prototype_judgement_delta_build_constant_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	uint32_t result_type,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !p_motive_result ||
		scrutinee >= terms->term_count ||
		scrutinee_classifier >= terms->term_count ||
		result_type >= terms->term_count || !delta->contexts) {
		return -1;
	}
	uint32_t motive_binding = prototype_term_new_binding(terms);
	uint32_t motive_var;
	uint32_t motive_lambda;
	uint32_t motive_universe;
	uint32_t motive_pi;
	uint32_t motive_context;
	uint32_t outer_context = delta->current_context_id;
	uint32_t outer_operation = delta->current_occurrence_id;
	if (motive_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, motive_binding, &motive_var
		) != 0 || prototype_term_lambda(
			terms, motive_binding, result_type, &motive_lambda
		) != 0 || prototype_term_universe_var(
			terms, universe_level_var, &motive_universe
		) != 0 || prototype_context_extend(
			delta->contexts,
			outer_context,
			motive_binding,
			scrutinee_classifier,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_context(delta, motive_context);
	prototype_judgement_delta_set_occurrence(delta, PROTOTYPE_INVALID_ID);
	if (prototype_judgement_delta_record_context_binding_assumption(
			delta, terms, motive_binding, scrutinee_classifier
		) != 0 || prototype_judgement_delta_ensure_type_at_universe(
			delta, terms, type_declarations, result_type, motive_universe
		) != 0 || prototype_term_pi(
			terms, scrutinee_classifier, motive_universe, &motive_pi
		) != 0) {
		return -1;
	}
	struct prototype_judgement_selected_evidence lambda_evidence[2];
	if (prototype_judgement_delta_select_evidence(
			delta,
			PROTOTYPE_INVALID_ID,
			motive_context,
			motive_var,
			scrutinee_classifier,
			&lambda_evidence[0]
		) != 0 || lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			result_type,
			motive_universe,
			&lambda_evidence[1]
		) != 0) {
		return -1;
	}
	uint32_t lambda_subjects[2] = { motive_var, result_type };
	uint32_t lambda_classifiers[2] = {
		scrutinee_classifier, motive_universe
	};
	uint32_t lambda_contexts[2] = { motive_context, motive_context };
	prototype_judgement_delta_set_context(delta, outer_context);
	prototype_judgement_delta_set_occurrence(delta, outer_operation);
	if (add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			motive_lambda,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			motive_lambda,
			motive_pi,
			PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			lambda_contexts,
			NULL,
			lambda_subjects,
			lambda_classifiers,
			lambda_evidence,
			2
		) != 0 || prototype_term_app(
			terms, motive_lambda, scrutinee, p_motive_result
		) != 0) {
		return -1;
	}
	struct prototype_judgement_selected_evidence app_evidence[2];
	if (prototype_judgement_delta_select_evidence(
			delta,
			PROTOTYPE_INVALID_ID,
			outer_context,
			motive_lambda,
			motive_pi,
			&app_evidence[0]
		) != 0 || lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			scrutinee,
			scrutinee_classifier,
			&app_evidence[1]
		) != 0) {
		return -1;
	}
	uint32_t app_subjects[2] = { motive_lambda, scrutinee };
	uint32_t app_classifiers[2] = { motive_pi, scrutinee_classifier };
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		*p_motive_result,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		*p_motive_result,
		motive_universe,
		PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
		NULL,
		NULL,
		app_subjects,
		app_classifiers,
		app_evidence,
		2
	);
}

/*
 * Compare already-erased Match cases whose locally bound core IDs differ.
 * Source elaboration must use ContextDB/SubstitutionDB instead; this private
 * helper is only an alpha transport for legacy TermDB proof validation.
 */
