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
		uint32_t outer_operation = delta->current_operation_id;
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
		prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
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
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		uint32_t lambda_universe_family;
		uint32_t lambda_classifier;
		uint32_t returned;
		uint32_t empty_effects;
		uint32_t returned_classifier;
		uint32_t family_classifier;
		if (prototype_term_effect_label(
					terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects
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
			prototype_judgement_delta_set_operation(delta, outer_operation);
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
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		uint32_t returned_body = terms->terms[returned].as.lambda.body;
		uint32_t returned_binder;
		if (prototype_term_var(terms, codomain_binder, &returned_binder) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
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
			prototype_judgement_delta_set_operation(delta, outer_operation);
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
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		struct prototype_judgement_selected_evidence binder_evidence;
		struct prototype_judgement_selected_evidence body_evidence;
		if (prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				delta->current_context_id,
				returned_binder,
				domain,
				&binder_evidence
			) != 0 || prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				delta->current_context_id,
				returned_body,
				returned_classifier,
				&body_evidence
			) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, outer_context);
		prototype_judgement_delta_set_operation(delta, outer_operation);
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
			delta->current_operation_id,
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
	if (status == 0) {
		status = prototype_judgement_delta_publish_complete(&delta);
	}
	if (status == 0) {
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
	uint32_t outer_operation = delta->current_operation_id;
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
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
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
					j
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
	prototype_judgement_delta_set_operation(delta, outer_operation);
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
	uint32_t outer_operation = delta->current_operation_id;
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
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
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
	prototype_judgement_delta_set_operation(delta, outer_operation);
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
static int core_match_case_alpha_reindex(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_case_binder* source_binders,
	const struct prototype_case_binder* motive_binders,
	uint32_t binder_count,
	uint32_t branch_classifier,
	uint32_t* p_motive_body
) {
	if (!terms || !type_declarations || !p_motive_body ||
		branch_classifier >= terms->term_count ||
		(binder_count > 0 && (!source_binders || !motive_binders))) {
		return -1;
	}

	uint32_t current = branch_classifier;
	for (uint32_t i = 0; i < binder_count; ++i) {
		uint32_t motive_binder_var;
		if (prototype_term_var(
			terms,
			motive_binders[i].binding_id,
			&motive_binder_var
		) != 0 ||
			prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				current,
				source_binders[i].binding_id,
				motive_binder_var,
				&current
			) != 0) {
			return -1;
		}
	}
	*p_motive_body = current;
	return 0;
}

static int classifier_list_contains_normalization_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const uint32_t* classifiers,
	uint32_t classifier_count,
	uint32_t candidate
) {
	for (uint32_t i = 0; i < classifier_count; ++i) {
		struct prototype_term_conversion_result conversion =
			prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				classifiers[i],
				candidate
			);
		if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 1;
		}
		if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
			return -1;
		}
	}
	return 0;
}

static int collect_subject_classifiers(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
) {
	if (!delta || !terms || !type_declarations || !classifiers ||
		!p_classifier_count || subject >= terms->term_count) {
		return -1;
	}
	*p_classifier_count = 0;
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_proposition* relations =
			source == 0 ? delta->propositions : delta->db->propositions;
		const struct prototype_judgement_derivation_candidate* derivations =
			source == 0 ? delta->derivation_candidates :
				delta->db->derivation_candidates;
		size_t proposition_count =
			source == 0 ? delta->proposition_count : delta->db->proposition_count;
		size_t derivation_candidate_count = source == 0 ?
			delta->derivation_candidate_count :
			delta->db->derivation_candidate_count;
		for (size_t i = 0; i < proposition_count; ++i) {
			const struct prototype_judgement_proposition* relation = &relations[i];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != subject ||
				candidate_has_derivation_other_than(
					relations,
					proposition_count,
					derivations,
					derivation_candidate_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
				) != 1) {
				continue;
			}
			int contains = classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					classifiers,
					*p_classifier_count,
					relation->classifier
				);
			if (contains < 0) {
				return -1;
			}
			if (contains > 0) {
				continue;
			}
			if (*p_classifier_count >= classifier_capacity) {
				return -1;
			}
			classifiers[(*p_classifier_count)++] = relation->classifier;
		}
	}
	return 0;
}

static int collect_judgement_subject_classifiers(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
) {
	if (!judgement || !terms || !type_declarations || !classifiers ||
		!p_classifier_count || subject >= terms->term_count) {
		return -1;
	}
	*p_classifier_count = 0;
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			relation->subject != subject ||
			candidate_has_derivation_other_than(
				judgement->propositions,
				judgement->proposition_count,
				judgement->derivation_candidates,
				judgement->derivation_candidate_count,
				(uint32_t)i,
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			) != 1) {
			continue;
		}
		int contains = classifier_list_contains_normalization_equal(
				terms,
				type_declarations,
				classifiers,
				*p_classifier_count,
				relation->classifier
			);
		if (contains < 0) {
			return -1;
		}
		if (contains > 0) {
			continue;
		}
		if (*p_classifier_count >= classifier_capacity) {
			return -1;
		}
		classifiers[(*p_classifier_count)++] = relation->classifier;
	}
	return 0;
}

static int select_match_branch_classifier(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t branch_index,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	if (branch_index >= match->as.match.case_count) {
		return -1;
	}
	uint32_t case_id = match->as.match.first_case + branch_index;
	if (case_id >= terms->case_count) {
		return -1;
	}
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			terms->cases[case_id].body,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	if (candidate_count == 0) {
		return 1;
	}
	if (candidate_count == 1) {
		*p_classifier = candidates[0];
		return 0;
	}
	for (uint32_t i = 0; i < candidate_count; ++i) {
		for (uint32_t j = 0; j < match->as.match.case_count; ++j) {
			if (j == branch_index) {
				continue;
			}
			uint32_t other_case_id = match->as.match.first_case + j;
			if (other_case_id >= terms->case_count) {
				return -1;
			}
			uint32_t other_candidates[32];
			uint32_t other_candidate_count = 0;
			if (collect_subject_classifiers(
					delta,
					terms,
					type_declarations,
					terms->cases[other_case_id].body,
					other_candidates,
					32,
					&other_candidate_count
				) != 0) {
				return -1;
			}
			int contains = classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					other_candidates,
					other_candidate_count,
					candidates[i]
				);
			if (contains < 0) {
				return -1;
			}
			if (contains > 0) {
				*p_classifier = candidates[i];
				return 0;
			}
		}
	}
	return 1;
}

static int match_case_constructor_term(
	struct prototype_term_db* terms,
	const struct prototype_match_case* match_case,
	uint32_t* p_constructor
) {
	if (!terms || !match_case || !p_constructor ||
		match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		match_case->constructor_id == PROTOTYPE_INVALID_ID ||
		match_case->first_binder > terms->case_binder_count ||
		match_case->binder_count > terms->case_binder_count -
			match_case->first_binder || prototype_term_constructor(
			terms,
			match_case->constructor_owner,
			match_case->constructor_id,
			p_constructor
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		uint32_t field;
		if (prototype_term_var(
				terms,
				terms->case_binders[
					match_case->first_binder + i
				].binding_id,
				&field
			) != 0 || prototype_term_app(
				terms, *p_constructor, field, p_constructor
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int match_case_motive_classifier(
	struct prototype_term_db* terms,
	const struct prototype_match_case* match_case,
	uint32_t motive,
	uint32_t* p_classifier
) {
	uint32_t constructor;
	if (!terms || !match_case || !p_classifier ||
		motive >= terms->term_count ||
		terms->terms[motive].tag != PROTOTYPE_TERM_LAMBDA ||
		match_case_constructor_term(terms, match_case, &constructor) != 0 ||
		prototype_term_app(terms, motive, constructor, p_classifier) != 0) {
		return -1;
	}
	return 0;
}

static int match_motive_result_classifier(
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t classifier
) {
	if (!terms ||
		match_term >= terms->term_count ||
		classifier >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[classifier].tag != PROTOTYPE_TERM_APP) {
		return 0;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	const struct prototype_term* app = &terms->terms[classifier];
	return app->as.app.argument == match->as.match.scrutinee &&
		app->as.app.function < terms->term_count &&
		terms->terms[app->as.app.function].tag == PROTOTYPE_TERM_LAMBDA;
}

static int select_existing_match_motive_result(
	const struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	for (size_t i = delta->proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation = &delta->propositions[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == match_term &&
			match_motive_result_classifier(terms, match_term, relation->classifier)) {
			*p_motive_result = relation->classifier;
			return 0;
		}
	}
	for (size_t i = delta->match_motive_result_count; i > 0; --i) {
		const struct prototype_judgement_match_motive_result* result =
			&delta->match_motive_results[i - 1];
		if (result->match_term == match_term &&
			match_motive_result_classifier(terms, match_term, result->classifier)) {
			*p_motive_result = result->classifier;
			return 0;
		}
	}
	if (delta->db) {
		for (size_t i = delta->db->proposition_count; i > 0; --i) {
			const struct prototype_judgement_proposition* relation =
				&delta->db->propositions[i - 1];
			if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->subject == match_term &&
				match_motive_result_classifier(terms, match_term, relation->classifier)) {
				*p_motive_result = relation->classifier;
				return 0;
			}
		}
	}
	return 1;
}

static int match_scrutinee_classifier_for_motive(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t* p_scrutinee_classifier
) {
	if (!delta || !terms || !type_declarations || !p_scrutinee_classifier ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t scrutinee_classifier = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			scrutinee_classifier = PROTOTYPE_INVALID_ID;
			break;
		}
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
			scrutinee_classifier = match_case->constructor_owner;
			continue;
		}
		int same_owner = 0;
		if (prototype_term_view_shape_equal(
				terms,
				scrutinee_classifier,
				match_case->constructor_owner,
				&same_owner
			) != 0) {
			return -1;
		}
		if (!same_owner) {
			return -1;
		}
	}
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
		uint32_t candidates[32];
		uint32_t candidate_count = 0;
		if (collect_subject_classifiers(
				delta,
				terms,
				type_declarations,
				match->as.match.scrutinee,
				candidates,
				32,
				&candidate_count
			) != 0) {
			return -1;
		}
		if (candidate_count != 1) {
			return 1;
		}
		scrutinee_classifier = candidates[0];
	}
	*p_scrutinee_classifier = scrutinee_classifier;
	return 0;
}

static int select_universe_classifier_from_candidates(
	const struct prototype_term_db* terms,
	const uint32_t* candidates,
	uint32_t candidate_count,
	uint32_t* p_classifier
) {
	if (!terms || !candidates || !p_classifier) {
		return -1;
	}
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < candidate_count; ++i) {
		if (!term_is_universe_var(terms, candidates[i])) {
			continue;
		}
		if (selected != PROTOTYPE_INVALID_ID && selected != candidates[i]) {
			return -1;
		}
		selected = candidates[i];
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = selected;
	return 0;
}

static int select_delta_universe_classifier(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			subject,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	return select_universe_classifier_from_candidates(
		terms,
		candidates,
		candidate_count,
		p_classifier
	);
}

static int select_judgement_universe_classifier(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_judgement_subject_classifiers(
			judgement,
			terms,
			type_declarations,
			subject,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	return select_universe_classifier_from_candidates(
		terms,
		candidates,
		candidate_count,
		p_classifier
	);
}

static int select_match_branch_classifier_for_motive_from_candidates(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	const struct prototype_match_case* motive_case,
	const uint32_t* candidates,
	uint32_t candidate_count,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !match_case || !motive_case ||
		!candidates || !p_classifier) {
		return -1;
	}
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < candidate_count; ++i) {
		uint32_t expected_motive_case_body;
		if (core_match_case_alpha_reindex(
				terms,
				type_declarations,
				&terms->case_binders[match_case->first_binder],
				&terms->case_binders[motive_case->first_binder],
				match_case->binder_count,
				candidates[i],
				&expected_motive_case_body
			) != 0) {
			return -1;
		}
		int motive_equal;
		if (classifier_conversion_decision(
				prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					expected_motive_case_body,
					motive_case->body
				),
				&motive_equal
			) != 0) {
			return -1;
		}
		if (!motive_equal) {
			continue;
		}
		if (selected != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				selected,
				candidates[i]
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		selected = candidates[i];
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = selected;
	return 0;
}

static int select_delta_match_branch_classifier_for_motive(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	const struct prototype_match_case* motive_case,
	uint32_t* p_classifier
) {
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			match_case->body,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	return select_match_branch_classifier_for_motive_from_candidates(
		terms,
		type_declarations,
		match_case,
		motive_case,
		candidates,
		candidate_count,
		p_classifier
	);
}

static int select_judgement_match_branch_classifier_for_motive(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	const struct prototype_match_case* motive_case,
	uint32_t* p_classifier
) {
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_judgement_subject_classifiers(
			judgement,
			terms,
			type_declarations,
			match_case->body,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	return select_match_branch_classifier_for_motive_from_candidates(
		terms,
		type_declarations,
		match_case,
		motive_case,
		candidates,
		candidate_count,
		p_classifier
	);
}

static int build_match_motive_from_branch_classifiers(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	const uint32_t* branch_classifiers,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !branch_classifiers || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[match_term].as.match.case_count > 64) {
		return -1;
	}

	int existing_status = select_existing_match_motive_result(
		delta,
		terms,
		match_term,
		p_motive_result
	);
	if (existing_status < 0) {
		return -1;
	}
	if (existing_status == 0) {
		return 0;
	}

	uint32_t scrutinee_classifier;
	int scrutinee_status = match_scrutinee_classifier_for_motive(
		delta,
		terms,
		type_declarations,
		match_term,
		&scrutinee_classifier
	);
	if (scrutinee_status != 0) {
		return scrutinee_status;
	}

	const struct prototype_term* match = &terms->terms[match_term];
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t motive_binder_cursor = 0;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		if (branch_classifiers[i] >= terms->term_count) {
			return -1;
		}
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			match_case->constructor_id == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		if (motive_binder_cursor + match_case->binder_count > 256) {
			return -1;
		}
		motive_cases[i].case_label_symbol_id = terms->case_label_symbols[case_id];
		motive_cases[i].constructor_owner = match_case->constructor_owner;
		motive_cases[i].constructor_id = match_case->constructor_id;
		motive_cases[i].binders = &motive_binders[motive_binder_cursor];
		motive_cases[i].binder_count = match_case->binder_count;
		for (uint32_t j = 0; j < match_case->binder_count; ++j) {
			uint32_t motive_binder_id = prototype_term_new_binding(terms);
			if (motive_binder_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			motive_binders[motive_binder_cursor + j].binding_id = motive_binder_id;
		}
		if (core_match_case_alpha_reindex(
				terms,
				type_declarations,
					&terms->case_binders[match_case->first_binder],
					&motive_binders[motive_binder_cursor],
					match_case->binder_count,
					branch_classifiers[i],
					&motive_cases[i].body
				) != 0) {
				return -1;
			}
		motive_binder_cursor += match_case->binder_count;
	}

	uint32_t motive_result;
	int status = prototype_judgement_delta_build_match_motive(
		delta,
		terms,
		type_declarations,
		match->as.match.scrutinee,
		scrutinee_classifier,
		motive_cases,
		match->as.match.case_count,
		universe_level_var,
		&motive_result
	);
	if (status != 0) {
		return status;
	}
	*p_motive_result = motive_result;
	return add_match_motive_result(delta, match_term, motive_result);
}

int prototype_judgement_delta_build_match_motive_from_known_branches(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	int existing_status = select_existing_match_motive_result(
		delta,
		terms,
		match_term,
		p_motive_result
	);
	if (existing_status < 0) {
		return -1;
	}
	if (existing_status == 0) {
		return 0;
	}

	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t common[32];
	uint32_t common_count = 0;
	int found_known_branch = 0;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		uint32_t branch_classifiers[32];
		uint32_t branch_classifier_count = 0;
		if (collect_subject_classifiers(
				delta,
				terms,
				type_declarations,
				terms->cases[case_id].body,
				branch_classifiers,
				32,
				&branch_classifier_count
			) != 0) {
			return -1;
		}
		if (branch_classifier_count == 0) {
			continue;
		}
		if (!found_known_branch) {
			for (uint32_t j = 0; j < branch_classifier_count; ++j) {
				common[common_count++] = branch_classifiers[j];
			}
			found_known_branch = 1;
			continue;
		}
		uint32_t write = 0;
		for (uint32_t j = 0; j < common_count; ++j) {
			int contains = classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					branch_classifiers,
					branch_classifier_count,
					common[j]
				);
			if (contains < 0) {
				return -1;
			}
			if (contains > 0) {
				common[write++] = common[j];
			}
		}
		common_count = write;
		if (common_count == 0) {
			return 1;
		}
	}
	if (!found_known_branch || common_count != 1) {
		return 1;
	}
	uint32_t branch_classifiers[64];
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		branch_classifiers[i] = common[0];
	}
	return build_match_motive_from_branch_classifiers(
		delta,
		terms,
		type_declarations,
		match_term,
		branch_classifiers,
		universe_level_var,
		p_motive_result
	);
}

int prototype_judgement_delta_build_match_motive_from_branch_hints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	const uint32_t* branch_classifiers,
	uint32_t branch_count,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !branch_classifiers || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		branch_count != terms->terms[match_term].as.match.case_count || branch_count > 64) {
		return -1;
	}
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < branch_count; ++i) {
		uint32_t classifier = branch_classifiers[i];
		if (classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (classifier >= terms->term_count) {
			return -1;
		}
		if (selected != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				terms, type_declarations, selected, classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 1;
		}
		selected = classifier;
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t uniform_classifiers[64];
	for (uint32_t i = 0; i < branch_count; ++i) {
		uniform_classifiers[i] = selected;
	}
	return build_match_motive_from_branch_classifiers(
		delta,
		terms,
		type_declarations,
		match_term,
		uniform_classifiers,
		universe_level_var,
		p_motive_result
	);
}

int prototype_judgement_delta_build_match_motive_from_cases(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[match_term].as.match.case_count > 64) {
		return -1;
	}

	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t branch_classifiers[64];
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		uint32_t branch_classifier;
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			match_case->constructor_id == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		if (select_match_branch_classifier(
				delta,
				terms,
				type_declarations,
				match_term,
				i,
				&branch_classifier
			) != 0) {
			int infer_status = prototype_judgement_delta_infer_core_helper_facts(
				delta,
				terms,
				type_declarations
			);
			if (infer_status != 0) {
				return -1;
			}
			if (select_match_branch_classifier(
					delta,
					terms,
					type_declarations,
					match_term,
					i,
					&branch_classifier
				) != 0) {
				return 1;
			}
		}
		branch_classifiers[i] = branch_classifier;
	}
	return build_match_motive_from_branch_classifiers(
		delta,
		terms,
		type_declarations,
		match_term,
		branch_classifiers,
		universe_level_var,
		p_motive_result
	);
}

int prototype_judgement_delta_type_match_from_cases(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	uint32_t motive_result;
	int status = prototype_judgement_delta_build_match_motive_from_cases(
		delta,
		terms,
		type_declarations,
		match_term,
		universe_level_var,
		&motive_result
	);
	if (status != 0) {
		return status;
	}
	status = prototype_judgement_delta_expand_match(
		delta,
		terms,
		type_declarations,
		match_term,
		motive_result
	);
	if (status != 0) {
		return status;
	}
	*p_motive_result = motive_result;
	return 0;
}

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
