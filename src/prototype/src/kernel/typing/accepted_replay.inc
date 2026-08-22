int prototype_judgement_add_normalization_premise_conversions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_typed_occurrence_graph* operations,
	struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !judgement) {
		return -1;
	}
	size_t derivation_candidate_count = judgement->derivation_candidate_count;
	for (size_t i = 0; i < derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* proof = &judgement->derivation_candidates[i];
		if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			uint32_t expected_operation_id = PROTOTYPE_INVALID_ID;
			int operation_status = judgement_expected_premise_operation(
				operations, proof, j, &expected_operation_id
			);
			if (operation_status < 0) {
				return -1;
			}
			int existing_status = operation_status == 0 ?
				judgement_operation_candidate_claim_has_derivation(
					judgement,
					expected_operation_id,
					proof->premises[j].proposition->context_id,
					proof->premises[j].proposition->kind,
					proof->premises[j].proposition->subject,
					proof->premises[j].proposition->classifier
				) : judgement_candidate_claim_has_derivation(
					judgement,
					proof->premises[j].proposition->context_id,
					proof->premises[j].proposition->kind,
					proof->premises[j].proposition->subject,
					proof->premises[j].proposition->classifier,
					proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION &&
					proof->proof_kind !=
						PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE
				);
			if (existing_status == 0) {
				continue;
			}
			if (proof->premises[j].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
				continue;
			}
			for (size_t k = 0; k < judgement->proposition_count; ++k) {
				const struct prototype_judgement_proposition* candidate =
					&judgement->propositions[k];
				uint32_t candidate_cursor = 0;
				uint32_t candidate_derivation_id;
				int has_direct_derivation = 0;
				while (prototype_judgement_candidate_derivation_next(
					judgement->propositions,
					judgement->proposition_count,
					judgement->derivation_candidates,
					judgement->derivation_candidate_count,
					(uint32_t)k,
					&candidate_cursor,
					&candidate_derivation_id
				) == 0) {
					int candidate_kind = judgement->derivation_candidates[
						candidate_derivation_id
					].proof_kind;
					if (candidate_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION &&
						candidate_kind !=
							PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE) {
						has_direct_derivation = 1;
						break;
					}
				}
				if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					(operation_status == 0 &&
					 candidate->occurrence_id != expected_operation_id) ||
					candidate->context_id != proof->premises[j].proposition->context_id ||
					candidate->subject != proof->premises[j].proposition->subject ||
					!has_direct_derivation ||
					!(prototype_judgement_classifier_conversion(
						terms,
						type_declarations,
						proof->premises[j].proposition->classifier,
						candidate->classifier
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					continue;
				}
				uint32_t premise_subjects[1] = { candidate->subject };
				uint32_t premise_classifiers[1] = { candidate->classifier };
				if (add_relation_with_premises(
						judgement,
						proof->premises[j].proposition->context_id,
						candidate->occurrence_id,
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
						proof->premises[j].proposition->subject,
						proof->premises[j].proposition->classifier,
						PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
						NULL,
						premise_subjects,
						premise_classifiers,
						1
					) != 0) {
					return -1;
				}
				break;
			}
		}
	}
	return 0;
}

static int initialize_candidate_rule_application_view(
	const struct prototype_judgement_proposition* conclusion,
	const struct prototype_judgement_derivation_candidate* candidate,
	struct prototype_judgement_rule_application_view* view
) {
	if (!conclusion || !candidate || !view ||
		candidate->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		(candidate->premise_count != 0 && !candidate->premises)) {
		return -1;
	}
	for (uint32_t i = 0; i < candidate->premise_count; ++i) {
		if (!candidate->premises[i].proposition) {
			return -1;
		}
	}
	memset(view, 0, sizeof(*view));
	view->proof_kind = candidate->proof_kind;
	view->conclusion = conclusion;
	view->rule_data = candidate->rule_data;
	view->semantic_action_kind = candidate->semantic_action_kind;
	view->semantic_action_id = candidate->semantic_action_id;
	view->premise_count = candidate->premise_count;
	for (uint32_t i = 0; i < candidate->premise_count; ++i) {
		view->premises[i] = (struct prototype_judgement_premise_view){
			.proposition = candidate->premises[i].proposition,
			.semantic_action_kind =
				candidate->premises[i].semantic_action_kind,
			.semantic_action_id = candidate->premises[i].semantic_action_id
		};
	}
	return 0;
}

static int judgement_expected_rule_view_premise_operation(
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_rule_application_view* view,
	uint32_t premise_index,
	uint32_t* p_operation_id
) {
	if (!view || !view->conclusion || !p_operation_id) {
		return -1;
	}
	if (view->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO &&
		premise_index == 0) {
		*p_operation_id = PROTOTYPE_INVALID_ID;
		return 0;
	}
	uint32_t child_operation_id;
	int status = judgement_structural_premise_child(
		operations,
		view->proof_kind,
		view->conclusion->occurrence_id,
		view->premise_count,
		premise_index,
		&child_operation_id
	);
	if (status != 0) {
		return status;
	}
	*p_operation_id = child_operation_id == PROTOTYPE_INVALID_ID ?
		PROTOTYPE_INVALID_ID : judgement_operation_evidence_owner(
			operations, child_operation_id, 0
		);
	return 0;
}

static int judgement_premise_operation_matches(
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t expected_operation_id,
	const struct prototype_judgement_proposition* premise
) {
	if (!operations || !premise || premise->authority_kind !=
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ||
		premise->occurrence_id >= operations->occurrence_count) {
		return 0;
	}
	return premise->occurrence_id == expected_operation_id ||
		judgement_operation_evidence_owner(
			operations, premise->occurrence_id, 0
		) == expected_operation_id;
}

static int validate_app_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		proof->premise_count != 2 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	const struct prototype_term* app;
	if (term_core_app(terms, relation->subject, &app) != 0) {
		return -1;
	}
	int function_matches = 0;
	int argument_matches = 0;
	if (prototype_term_core_shape_equal(
			terms, app->as.app.function, proof->premises[0].proposition->subject, &function_matches
		) != 0 ||
		prototype_term_core_shape_equal(
			terms, app->as.app.argument, proof->premises[1].proposition->subject, &argument_matches
		) != 0 ||
		!function_matches || !argument_matches) {
		return -1;
	}
	uint32_t function_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t specialized_function_classifier;
	uint32_t callable_function_classifier;
	if (prototype_judgement_expose_callable_classifier(
			terms,
			type_declarations,
			proof->premises[0].proposition->classifier,
			&callable_function_classifier
		) != 0 || prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			callable_function_classifier,
			proof->premises[1].proposition->classifier,
			&specialized_function_classifier
		) != 0) {
		return -1;
	}
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		specialized_function_classifier,
		&function_pi,
		&domain,
		&codomain_family
	);
	if (status != 0) {
		return -1;
	}
	(void)function_pi;
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
			proof->premises[1].proposition->classifier,
			&solved_domain
		) != 0) {
		return -1;
	}
	(void)solved_domain;
	uint32_t result_classifier;
	uint32_t identity;
	if (prototype_substitution_identity(
			substitutions, contexts, relation->context_id, &identity
		) != 0 ||
		instantiate_pure_family_in_context(
			contexts,
			substitutions,
			terms,
			type_declarations,
			identity,
			proof->premises[1].proposition->classifier,
			codomain_family,
			proof->premises[1].proposition->subject,
			proof->premises[1].proposition->classifier,
			&result_classifier
		) != 0 ||
			!(prototype_judgement_classifier_conversion(
				terms,
			type_declarations,
			result_classifier,
			relation->classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return 0;
}
static int judgement_operation_owns_effect_row_binder(
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	uint32_t binding_id
);

static int classifier_effect_specialization_is_operation_owned(
	struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	uint32_t expected,
	uint32_t actual
) {
	if (!terms || expected >= terms->term_count || actual >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < terms->term_count; ++i) {
		if (terms->terms[i].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			continue;
		}
		uint32_t binding_id = terms->terms[i].as.effect_row_var.binding_id;
		if (!prototype_term_contains_free_binding(terms, actual, binding_id) ||
			prototype_term_contains_free_binding(terms, expected, binding_id)) {
			continue;
		}
		int owned = judgement_operation_owns_effect_row_binder(
			terms, operations, operation_id, binding_id
		);
		if (owned <= 0) {
			return owned < 0 ? -1 : 0;
		}
	}
	return 1;
}

static int validate_lambda_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_LAMBDA ||
		proof->premise_count != 2 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	if (proof->premises[0].proposition->subject >= terms->term_count ||
		proof->premises[1].proposition->subject >= terms->term_count ||
		terms->terms[proof->premises[0].proposition->subject].tag != PROTOTYPE_TERM_VAR ||
		proof->premises[0].proposition->context_id != proof->premises[1].proposition->context_id) {
		return -1;
	}
	uint32_t occurrence_lambda;
	int occurrence_matches;
	if (prototype_term_lambda(
			terms,
			terms->terms[proof->premises[0].proposition->subject].as.var.binding_id,
			proof->premises[1].proposition->subject,
			&occurrence_lambda
		) != 0 || prototype_term_core_shape_equal(
			terms, occurrence_lambda, relation->subject, &occurrence_matches
		) != 0 || !occurrence_matches) {
		return -1;
	}
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t lambda_classifier;
	if (classifier_kernel_strip_effect_row_foralls(
			terms,
			type_declarations,
			NULL,
			relation->classifier,
			&lambda_classifier
		) != 0) {
		return -1;
	}
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		lambda_classifier,
		&lambda_pi,
		&domain,
		&codomain_family
	);
	(void)lambda_pi;
	int domain_matches = status == 0 &&
		prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			domain,
			proof->premises[0].proposition->classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
	if (!domain_matches && status == 0) {
		uint32_t solved_argument;
		int ownership = classifier_effect_specialization_is_operation_owned(
			terms,
			operations,
			relation->occurrence_id,
			domain,
			proof->premises[0].proposition->classifier
		);
		domain_matches = ownership > 0 &&
			prototype_judgement_solve_expected_effect_rows(
				terms,
				type_declarations,
				NULL,
				proof->premises[0].proposition->classifier,
				domain,
				&solved_argument
			) == 0 && prototype_judgement_classifier_compatible(
				terms, type_declarations, solved_argument, domain
			);
	}
	if (status != 0 || !domain_matches) {
		return -1;
	}
	uint32_t expected_body_classifier;
	uint32_t projection;
	if (prototype_substitution_projection(
			substitutions,
			contexts,
			proof->premises[0].proposition->context_id,
			&projection
		) != 0 ||
		instantiate_pure_family_in_context(
			contexts,
			substitutions,
			terms,
			type_declarations,
			projection,
			proof->premises[0].proposition->classifier,
			codomain_family,
			proof->premises[0].proposition->subject,
			proof->premises[0].proposition->classifier,
			&expected_body_classifier
		) != 0) {
		return -1;
	}
	int body_matches = prototype_judgement_classifier_conversion(
		terms,
		type_declarations,
		expected_body_classifier,
		proof->premises[1].proposition->classifier
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
	if (!body_matches) {
		uint32_t solved_body;
		int ownership = classifier_effect_specialization_is_operation_owned(
			terms,
			operations,
			relation->occurrence_id,
			expected_body_classifier,
			proof->premises[1].proposition->classifier
		);
		body_matches = ownership > 0 &&
			prototype_judgement_solve_expected_effect_rows(
				terms,
				type_declarations,
				NULL,
				proof->premises[1].proposition->classifier,
				expected_body_classifier,
				&solved_body
			) == 0 && prototype_judgement_classifier_compatible(
				terms, type_declarations, solved_body,
				expected_body_classifier
			);
	}
	if (!body_matches) {
		return -1;
	}
	(void)codomain_family;
	return 0;
}

static int match_branch_classifier_satisfies_motive(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t branch_classifier,
	uint32_t motive_classifier
);

static int validate_match_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_APP ||
		proof->premise_count !=
			terms->terms[relation->subject].as.match.case_count + 1 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition->subject != relation->classifier ||
		!term_is_universe_var(terms, proof->premises[0].proposition->classifier)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[relation->subject];
	const struct prototype_term* motive_app = &terms->terms[relation->classifier];
	const struct prototype_term* motive_lambda;
	const struct prototype_term* motive_body;
	uint32_t normalized_classifier;
	if (motive_app->as.app.argument != match->as.match.scrutinee ||
		terms->terms[motive_app->as.app.function].tag != PROTOTYPE_TERM_LAMBDA ||
		classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			relation->classifier,
			&normalized_classifier
		) != 0) {
		return -1;
	}
	motive_lambda = &terms->terms[motive_app->as.app.function];
	if (motive_lambda->as.lambda.body >= terms->term_count) {
		return -1;
	}
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
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_is_case_split ?
			motive_body->as.match.first_case + i : PROTOTYPE_INVALID_ID;
		if (case_id >= terms->case_count ||
			(motive_is_case_split && motive_case_id >= terms->case_count) ||
			proof->premises[i + 1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			!term_exists(terms, proof->premises[i + 1].proposition->classifier)) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = motive_is_case_split ?
			&terms->cases[motive_case_id] : NULL;
		const struct prototype_judgement_premise_view* branch_premise =
			&proof->premises[i + 1];
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
		uint32_t expected_branch_body = match_case->body;
		uint32_t expected_motive_body;
		if (motive_is_case_split) {
			expected_motive_body = motive_case->body;
		} else if (match_case_motive_classifier(
				terms,
				match_case,
				motive_app->as.app.function,
				&expected_motive_body
			) != 0) {
			return -1;
		}
		if (branch_premise->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			const struct prototype_substitution* supplied =
				prototype_substitution_get(
					substitutions, branch_premise->semantic_action_id
				);
			uint32_t refined_context;
			uint32_t expected_substitution;
			uint32_t constructor_term;
			if (!supplied || match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
				branch_premise->proposition->context_id !=
					supplied->source_context || prototype_judgement_identity_relation_branch_refinement(
					contexts,
					substitutions,
					terms,
					type_declarations,
					dimension_operators,
					relation->context_id,
					match->as.match.scrutinee,
					match_case->constructor_owner,
					match_case->constructor_id,
					supplied->target_context,
					&terms->case_binders[match_case->first_binder],
					match_case->binder_count,
					&refined_context,
					&expected_substitution,
					&constructor_term
				) != 0 || refined_context != supplied->source_context ||
				expected_substitution != branch_premise->semantic_action_id ||
				prototype_term_reindex(
					terms,
					type_declarations,
					contexts,
					substitutions,
					match_case->body,
					expected_substitution,
					&expected_branch_body
				) != 0 || prototype_term_reindex(
					terms,
					type_declarations,
					contexts,
					substitutions,
					expected_motive_body,
					expected_substitution,
					&expected_motive_body
				) != 0) {
				return -1;
			}
			(void)constructor_term;
		} else if (branch_premise->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID ||
			branch_premise->semantic_action_id != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		if (branch_premise->proposition->subject != expected_branch_body) {
			return -1;
		}
		if (!motive_is_case_split) {
			if (!match_branch_classifier_satisfies_motive(
					terms,
					type_declarations,
					branch_premise->proposition->classifier,
					expected_motive_body
				)) {
				return -1;
			}
			continue;
		}
		uint32_t expected_motive_case_body;
		if (core_match_case_alpha_reindex(
				terms,
				type_declarations,
				&terms->case_binders[match_case->first_binder],
				&terms->case_binders[motive_case->first_binder],
				match_case->binder_count,
				branch_premise->proposition->classifier,
				&expected_motive_case_body
			) != 0 || !match_branch_classifier_satisfies_motive(
				terms,
				type_declarations,
				expected_motive_case_body,
				expected_motive_body
			)) {
			return -1;
		}
	}
	(void)normalized_classifier;
	return 0;
}

static int operation_case_occurrence_binders(
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t match_operation_id,
	uint32_t case_index,
	struct prototype_case_binder* binders,
	uint32_t binder_capacity
) {
	if (!contexts || !substitutions || !terms || !operations || !binders ||
		match_operation_id >= operations->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match_operation =
		&operations->occurrences[match_operation_id];
	uint32_t scrutinee_occurrence;
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		judgement_occurrence_child(
			operations, match_operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee_occurrence
		) != 0 ||
		match_operation->core_term >= terms->term_count ||
		terms->terms[match_operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
		case_index >= match_operation->case_count ||
		match_operation->first_case + case_index >= operations->case_count) {
		return -1;
	}
	const struct prototype_term* core_match =
		&terms->terms[match_operation->core_term];
	uint32_t term_case_id = core_match->as.match.first_case + case_index;
	if (term_case_id >= terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* core_case = &terms->cases[term_case_id];
	const struct prototype_typed_occurrence_match_case* operation_case =
		&operations->cases[match_operation->first_case + case_index];
	if (core_case->binder_count > binder_capacity ||
		core_case->binder_count != operation_case->binder_count) {
		return -1;
	}
	uint32_t occurrence_context = operation_case->context_id;
	if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
		const struct prototype_substitution* refinement =
			prototype_substitution_get(
				substitutions, operation_case->refinement_substitution
			);
		if (!refinement || refinement->source_context != operation_case->context_id) {
			return -1;
		}
	}
	for (uint32_t binder_index = 0;
		binder_index < core_case->binder_count; ++binder_index) {
		if (!prototype_context_contains_binding(
				contexts, occurrence_context,
				operation_case->binder_ids[binder_index]
			)) {
			return -1;
		}
		binders[binder_index].binding_id =
			operation_case->binder_ids[binder_index];
		binders[binder_index].is_recursive = terms->case_binders[
			core_case->first_binder + binder_index
		].is_recursive;
	}
	return 0;
}

/* A zero-clause fold is the CBPV sequencing rule. Its continuation classifier
 * is checked under a fresh runtime-result binder, while a recursive motive is
 * expressed in the source Context. When the input has a structural pure-value
 * projection, replay the admissible zeta equation by replacing that local
 * binder with the projected source value. This is reconstructed from accepted
 * operation edges; no compiler solver cell or evaluation history is trusted. */
static int replay_project_pure_sequence_results(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t context_id,
	uint32_t classifier,
	uint32_t* p_projected
) {
	return prototype_judgement_project_pure_sequence_results(
		terms, type_declarations, contexts, operations, context_id,
		classifier, p_projected
	);
}

static int trace_induction_replay_rejection(
	const struct prototype_judgement_proposition* relation,
	unsigned line
) {
	if (getenv("A_PROGRAM_ACCEPTED_REPLAY_TRACE")) {
		fprintf(
			stderr,
			"IH replay rejected line=%u context=%u occurrence=%u subject=%u classifier=%u\n",
			line,
			relation ? relation->context_id : PROTOTYPE_INVALID_ID,
			relation ? relation->occurrence_id : PROTOTYPE_INVALID_ID,
			relation ? relation->subject : PROTOTYPE_INVALID_ID,
			relation ? relation->classifier : PROTOTYPE_INVALID_ID
		);
	}
	return -1;
}

/* Surface computation bodies may end in a raw value. Lowering interprets that
 * boundary as RETURN(value), but the erased Core node and its value classifier
 * remain unchanged. Replay this one admissible polarity bridge explicitly;
 * it is not global classifier conversion and it never removes an effect. */
static int classifier_is_pure_computation_result(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t computation_classifier,
	uint32_t value_classifier
) {
	struct prototype_term_classifier_view computation;
	struct prototype_term_classifier_view value;
	struct prototype_effect_row_normal_form row;
	return prototype_judgement_classifier_view(
			terms, type_declarations, NULL, computation_classifier, &computation
		) == 0 && computation.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		computation.computation_kind ==
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
		prototype_term_effect_row_normal_form(
			terms, computation.effect_row, &row
		) == 0 && row.atom_count == 0 && prototype_judgement_classifier_view(
			terms, type_declarations, NULL, value_classifier, &value
		) == 0 && value.category == PROTOTYPE_TERM_CATEGORY_VALUE &&
		prototype_judgement_classifier_conversion(
			terms, type_declarations, computation.result, value_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
}

static int validate_induction_hypothesis_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		proof->rule_data.induction.match >= terms->term_count ||
		proof->rule_data.induction.motive >= terms->term_count ||
		terms->terms[proof->rule_data.induction.match].tag != PROTOTYPE_TERM_MATCH ||
		proof->rule_data.induction.case_index >=
			terms->terms[proof->rule_data.induction.match].as.match.case_count) {
		return -1;
	}
	const struct prototype_term* ih = &terms->terms[relation->subject];
	if (ih->as.induction_hypothesis.ih_scope_id >= terms->ih_scope_count ||
		terms->ih_scopes[ih->as.induction_hypothesis.ih_scope_id].match_term !=
			proof->rule_data.induction.match) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[proof->rule_data.induction.match];
	uint32_t case_id = match->as.match.first_case + proof->rule_data.induction.case_index;
	if (case_id >= terms->case_count ||
		proof->rule_data.induction.field_index >= terms->cases[case_id].binder_count) {
		return -1;
	}
	const struct prototype_case_binder* binder =
		&terms->case_binders[
			terms->cases[case_id].first_binder + proof->rule_data.induction.field_index
		];
	if (ih->as.induction_hypothesis.argument >= terms->term_count ||
		terms->terms[ih->as.induction_hypothesis.argument].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t replay_operation_id = PROTOTYPE_INVALID_ID;
	if (relation->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE) {
		replay_operation_id = relation->occurrence_id;
	} else if (operations) {
		for (uint32_t operation_id = 0;
			operation_id < operations->occurrence_count;
			++operation_id) {
			const struct prototype_typed_occurrence* operation =
				&operations->occurrences[operation_id];
			if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS &&
				operation->core_term == relation->subject &&
				operation->context_id == relation->context_id) {
				if (replay_operation_id != PROTOTYPE_INVALID_ID) {
					replay_operation_id = PROTOTYPE_INVALID_ID;
					break;
				}
				replay_operation_id = operation_id;
			}
		}
	}
	if (replay_operation_id != PROTOTYPE_INVALID_ID) {
		struct prototype_typed_occurrence_induction_edge edge;
		if (!operations ||
			prototype_typed_occurrence_graph_induction_edge(
				operations, terms, replay_operation_id, &edge
			) != 0 || edge.case_index != proof->rule_data.induction.case_index ||
			edge.field_index != proof->rule_data.induction.field_index) {
			return -1;
		}
	} else if (terms->terms[
			ih->as.induction_hypothesis.argument
		].as.var.binding_id != binder->binding_id) {
		return -1;
	}
	if (!binder->is_recursive) {
		return -1;
	}
	uint32_t expected_classifier;
	int schema_status = -1;
	if (contexts && substitutions) {
		uint32_t field_classifier;
		uint32_t constructor_owner = terms->cases[case_id].constructor_owner;
		struct prototype_case_binder occurrence_binders[64];
		const struct prototype_case_binder* replay_binders =
			&terms->case_binders[terms->cases[case_id].first_binder];
		if (operations && replay_operation_id < operations->occurrence_count) {
			struct prototype_typed_occurrence_induction_edge edge;
			if (prototype_typed_occurrence_graph_induction_edge(
					operations, terms, replay_operation_id, &edge
				) != 0 || edge.case_index != proof->rule_data.induction.case_index ||
				operation_case_occurrence_binders(
					contexts,
					substitutions,
					terms,
					operations,
					edge.owner_match_occurrence,
					edge.case_index,
					occurrence_binders,
					64
				) != 0) {
				return -1;
			}
			replay_binders = occurrence_binders;
			constructor_owner = operations->cases[
				operations->occurrences[edge.owner_match_occurrence].first_case +
				edge.case_index
			].constructor_owner;
		}
		schema_status = prototype_judgement_constructor_field_classifier(
			terms, type_declarations, contexts, substitutions,
			relation->context_id,
			constructor_owner,
			terms->cases[case_id].constructor_id,
			replay_binders,
			proof->rule_data.induction.field_index,
			proof->rule_data.induction.field_index,
			&field_classifier
		);
		if (schema_status == 0) {
			schema_status = prototype_judgement_lift_recursive_field_classifier(
				terms, type_declarations, field_classifier,
				ih->as.induction_hypothesis.argument,
				constructor_owner,
				proof->rule_data.induction.motive,
				&expected_classifier
			);
		}
	}
	if (schema_status != 0) {
		/* Source operations are replayed from the authoritative constructor
		 * schema. Compiler-generated helper claims, including relation actions,
		 * may intentionally have no matching source Context and retain the
		 * original direct-motive replay rule. */
		if (relation->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE) {
			return trace_induction_replay_rejection(relation, __LINE__);
		}
		if (prototype_term_app(
				terms, proof->rule_data.induction.motive,
				ih->as.induction_hypothesis.argument,
				&expected_classifier
			) != 0) {
			return trace_induction_replay_rejection(relation, __LINE__);
		}
	}
	(void)judgement;
	uint32_t relation_classifier = relation->classifier;
	if (operations && contexts) {
		int projection_status = replay_project_pure_sequence_results(
			terms, type_declarations, contexts, operations,
			relation->context_id, relation->classifier, &relation_classifier
		);
		if (projection_status < 0) {
			return -1;
		}
	}
	int conversion = prototype_judgement_classifier_conversion(
		terms,
		type_declarations,
		expected_classifier,
		relation_classifier
	).status;
	if (conversion == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	uint32_t replay_expected_classifier = expected_classifier;
	/* An IH used below a nested indexed Match is classified in that branch's
	 * refined context. Replay the same source-index equations before comparing
	 * it with the motive application. This remains kernel reconstruction: every
	 * substitution is derived from the stored Match, constructor schema, and
	 * TypedOccurrenceGraph path. */
	if (operations && replay_operation_id < operations->occurrence_count) {
		uint32_t refined_expected = expected_classifier;
		for (uint32_t match_operation_id = 0;
			match_operation_id < operations->occurrence_count;
			++match_operation_id) {
			const struct prototype_typed_occurrence* match_operation =
				&operations->occurrences[match_operation_id];
			uint32_t scrutinee_occurrence;
			if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
				judgement_occurrence_child(
					operations, match_operation_id,
					PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
					&scrutinee_occurrence
				) != 0 ||
				match_operation->core_term >= terms->term_count ||
				terms->terms[match_operation->core_term].tag !=
					PROTOTYPE_TERM_MATCH) {
				continue;
			}
			for (uint32_t case_index = 0;
				case_index < match_operation->case_count;
				++case_index) {
				const struct prototype_typed_occurrence_match_case* operation_case =
					&operations->cases[
						match_operation->first_case + case_index
					];
				uint32_t path[128];
				uint32_t path_count;
				if (prototype_context_extension_path(
						contexts,
						operation_case->context_id,
						relation->context_id,
						path,
						128,
						&path_count
					) != 0) {
					continue;
				}
				if (prototype_typed_occurrence_match_case_is_solved(operation_case) &&
					prototype_term_reindex(
						terms, type_declarations, contexts, substitutions,
						refined_expected,
						operation_case->refinement_substitution,
						&refined_expected
					) != 0) {
					return -1;
				}
			}
		}
		if (prototype_judgement_classifier_conversion(
				terms, type_declarations, refined_expected,
				relation_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 0;
		}
		replay_expected_classifier = refined_expected;
	}
	if (classifier_is_pure_computation_result(
			terms, type_declarations, replay_expected_classifier,
			relation_classifier
		)) {
		return 0;
	}
	/* A higher-order recursive field lifts to a function value.  The IH
	 * operation constructs that value purely, so its operation classifier is
	 * Comp({}, lifted-field), unlike a direct recursive leaf whose motive is
	 * already a computation classifier. */
	struct prototype_term_classifier_view relation_view;
	struct prototype_term_classifier_view expected_view;
	struct prototype_effect_row_normal_form row;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, relation_classifier, &relation_view
		) != 0 || relation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		relation_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		prototype_term_effect_row_normal_form(
			terms, relation_view.effect_row, &row
		) != 0 || row.atom_count != 0) {
		if (getenv("A_PROGRAM_ACCEPTED_REPLAY_TRACE")) {
			fprintf(stderr, "IH replay non-pure-computation relation=%u expected=%u\nrelation: ",
				relation_classifier, replay_expected_classifier);
			prototype_term_print_debug(
				stderr, NULL, NULL, type_declarations, terms, relation_classifier
			);
			fprintf(stderr, "\nexpected: ");
			prototype_term_print_debug(
				stderr, NULL, NULL, type_declarations, terms,
				replay_expected_classifier
			);
			fprintf(stderr, "\n");
		}
		return trace_induction_replay_rejection(relation, __LINE__);
	}
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, replay_expected_classifier,
			&expected_view
		) == 0 && expected_view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		expected_view.computation_kind ==
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		uint32_t solved_expected;
		int solve_status = prototype_judgement_solve_expected_effect_rows(
			terms, type_declarations, NULL, replay_expected_classifier,
			relation_classifier, &solved_expected
		);
		if (solve_status == 0 && prototype_judgement_classifier_compatible(
				terms, type_declarations, solved_expected, relation_classifier
			)) {
			return 0;
		}
		return trace_induction_replay_rejection(relation, __LINE__);
	}
	int inner_conversion = prototype_judgement_classifier_conversion(
		terms, type_declarations, replay_expected_classifier, relation_view.result
	).status;
	if (inner_conversion != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		uint32_t solved_expected;
		int solve_status = prototype_judgement_solve_expected_effect_rows(
				terms, type_declarations, NULL, replay_expected_classifier,
				relation_view.result, &solved_expected
			);
		int compatible = solve_status == 0 && prototype_judgement_classifier_compatible(
				terms, type_declarations, solved_expected, relation_view.result
			);
		if (solve_status != 0 || !compatible) {
			if (getenv("A_PROGRAM_ACCEPTED_REPLAY_TRACE")) {
				fprintf(
					stderr,
					"IH replay classifier mismatch expected=%u result=%u solved=%u "
					"solve_status=%d compatible=%d\nexpected: ",
					replay_expected_classifier,
					relation_view.result,
					solve_status == 0 ? solved_expected : PROTOTYPE_INVALID_ID,
					solve_status,
					compatible
				);
				prototype_term_print_debug(
					stderr, NULL, NULL, type_declarations, terms,
					replay_expected_classifier
				);
				fprintf(stderr, "\nresult: ");
				prototype_term_print_debug(
					stderr, NULL, NULL, type_declarations, terms,
					relation_view.result
				);
				fprintf(stderr, "\n");
			}
			return trace_induction_replay_rejection(relation, __LINE__);
		}
	}
	return 0;
}

/* Match is the elimination boundary that joins branch effects. Its branch
 * premise may therefore have a smaller row than the solved motive while the
 * result carrier remains definitionally equal. This relation is local to the
 * Match rule; it is deliberately not classifier conversion. */
static int match_branch_classifier_satisfies_motive(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t branch_classifier,
	uint32_t motive_classifier
) {
	if (!terms || !type_declarations ||
		branch_classifier >= terms->term_count ||
		motive_classifier >= terms->term_count) {
		return 0;
	}
	if (prototype_judgement_classifier_conversion(
			terms, type_declarations, branch_classifier, motive_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	if (classifier_is_pure_computation_result(
			terms, type_declarations, motive_classifier, branch_classifier
		)) {
		return 1;
	}
	uint32_t solved_branch_classifier;
	if (prototype_judgement_solve_expected_effect_rows(
			terms,
			type_declarations,
			NULL,
			branch_classifier,
			motive_classifier,
			&solved_branch_classifier
		) == 0 && prototype_judgement_classifier_compatible(
			terms,
			type_declarations,
			solved_branch_classifier,
			motive_classifier
		)) {
		return 1;
	}
	struct prototype_term_classifier_view branch;
	struct prototype_term_classifier_view motive;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, branch_classifier, &branch
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, motive_classifier, &motive
		) != 0 || branch.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		motive.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		branch.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		motive.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		prototype_judgement_classifier_conversion(
			terms, type_declarations, branch.result, motive.result
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	struct prototype_effect_row_normal_form branch_row;
	struct prototype_effect_row_normal_form motive_row;
	return prototype_term_effect_row_normal_form(
			terms, branch.effect_row, &branch_row
		) == 0 && prototype_term_effect_row_normal_form(
			terms, motive.effect_row, &motive_row
		) == 0 && prototype_term_effect_row_normal_form_includes(
			&motive_row, &branch_row
		);
}

static int replay_operation_case_refinement_status(
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t match_operation_id,
	uint32_t case_index
) {
	if (!substitutions || !operations ||
		match_operation_id >= operations->occurrence_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	const struct prototype_typed_occurrence* match_operation =
		&operations->occurrences[match_operation_id];
	if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		case_index >= match_operation->case_count ||
		match_operation->first_case + case_index >= operations->case_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	const struct prototype_typed_occurrence_match_case* operation_case =
		&operations->cases[match_operation->first_case + case_index];
	switch (operation_case->refinement_status) {
	case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_SOLVED:
	{
		const struct prototype_substitution* refinement =
			prototype_substitution_get(
				substitutions, operation_case->refinement_substitution
			);
		return refinement &&
			refinement->source_context == operation_case->context_id ?
			PROTOTYPE_INDEX_REFINEMENT_SOLVED :
			PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE:
		return operation_case->refinement_substitution == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE :
			PROTOTYPE_INDEX_REFINEMENT_INVALID;
	case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_CONSTANT:
		return operation_case->refinement_substitution == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_INDEX_REFINEMENT_CONSTANT :
			PROTOTYPE_INDEX_REFINEMENT_INVALID;
	case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_PENDING:
	case PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_RESIDUAL:
	default:
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
}

static int replay_match_premise_refinement_action(
	const struct prototype_typed_occurrence_match_case* operation_case,
	const struct prototype_judgement_premise_view* premise
) {
	if (!operation_case || !premise) {
		return 0;
	}
	if (prototype_typed_occurrence_match_case_is_solved(operation_case)) {
		return premise->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION &&
			premise->semantic_action_id ==
				operation_case->refinement_substitution;
	}
	return premise->semantic_action_kind ==
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
}

static int replay_term_scratch_init(
	const struct prototype_term_db* source,
	struct prototype_term_db* scratch
) {
	if (!source || !scratch || source->term_capacity < source->term_count ||
		source->case_capacity < source->case_count ||
		source->case_binder_capacity < source->case_binder_count ||
		source->ih_scope_capacity < source->ih_scope_count) {
		return -1;
	}
	*scratch = *source;
	/* Runtime indexes are projections of one physical Term arena. A replay
	 * scratch arena must rebuild its own projection instead of mutating the
	 * source arena through pointers copied with the semantic graph descriptor. */
	scratch->intern_keys = NULL;
	scratch->intern_canonical_ids = NULL;
	scratch->intern_next = NULL;
	scratch->intern_buckets = NULL;
	scratch->intern_exact_hashes = NULL;
	scratch->intern_exact_next = NULL;
	scratch->intern_exact_buckets = NULL;
	scratch->effect_row_variable_terms = NULL;
	scratch->effect_row_variable_count = 0;
	scratch->effect_row_variable_capacity = 0;
	scratch->effect_row_variable_index_dirty = 1;
	scratch->intern_entry_capacity = 0;
	scratch->intern_bucket_count = 0;
	scratch->intern_indexed_count = 0;
	scratch->intern_index_dirty = 1;
	memset(&scratch->intern_stats, 0, sizeof(scratch->intern_stats));
	prototype_term_normalization_cache_clear(scratch);
	/* Replay may instantiate every binder in a deeply nested classifier. Use the
	 * authoritative store's configured capacities rather than a term-count
	 * heuristic, while keeping every generated node isolated in this arena. */
	scratch->term_capacity = source->term_capacity;
	scratch->case_capacity = source->case_capacity;
	scratch->case_binder_capacity = source->case_binder_capacity;
	scratch->ih_scope_capacity = source->ih_scope_capacity;
	scratch->terms = malloc(scratch->term_capacity * sizeof(*scratch->terms));
	scratch->cases = malloc(scratch->case_capacity * sizeof(*scratch->cases));
	scratch->case_label_symbols = malloc(
		scratch->case_capacity * sizeof(*scratch->case_label_symbols)
	);
	scratch->case_binders = malloc(
		scratch->case_binder_capacity * sizeof(*scratch->case_binders)
	);
	scratch->ih_scopes = malloc(
		scratch->ih_scope_capacity * sizeof(*scratch->ih_scopes)
	);
	if (!scratch->terms || !scratch->cases || !scratch->case_label_symbols ||
		!scratch->case_binders || !scratch->ih_scopes) {
		free(scratch->terms);
		free(scratch->cases);
		free(scratch->case_label_symbols);
		free(scratch->case_binders);
		free(scratch->ih_scopes);
		memset(scratch, 0, sizeof(*scratch));
		return -1;
	}
	memcpy(scratch->terms, source->terms,
		source->term_count * sizeof(*source->terms));
	memcpy(scratch->cases, source->cases,
		source->case_count * sizeof(*source->cases));
	memcpy(scratch->case_label_symbols, source->case_label_symbols,
		source->case_count * sizeof(*source->case_label_symbols));
	memcpy(scratch->case_binders, source->case_binders,
		source->case_binder_count * sizeof(*source->case_binders));
	memcpy(scratch->ih_scopes, source->ih_scopes,
		source->ih_scope_count * sizeof(*source->ih_scopes));
	return 0;
}

static void replay_term_scratch_free(struct prototype_term_db* scratch) {
	if (!scratch) {
		return;
	}
	prototype_term_db_dispose_runtime_state(scratch);
	free(scratch->terms);
	free(scratch->cases);
	free(scratch->case_label_symbols);
	free(scratch->case_binders);
	free(scratch->ih_scopes);
	memset(scratch, 0, sizeof(*scratch));
}

static int replay_reindexed_classifier_satisfies(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t source_classifier,
	uint32_t substitution_id,
	uint32_t actual_classifier
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		source_classifier >= terms->term_count ||
		actual_classifier >= terms->term_count ||
		substitutions->substitution_capacity < substitutions->substitution_count) {
		return 0;
	}
	struct prototype_term_db scratch_terms;
	memset(&scratch_terms, 0, sizeof(scratch_terms));
	struct prototype_substitution_db scratch_substitutions = *substitutions;
	scratch_substitutions.substitution_capacity =
		substitutions->substitution_capacity;
	scratch_substitutions.substitutions = malloc(
		scratch_substitutions.substitution_capacity *
			sizeof(*scratch_substitutions.substitutions)
	);
	if (replay_term_scratch_init(terms, &scratch_terms) != 0 ||
		!scratch_substitutions.substitutions) {
		replay_term_scratch_free(&scratch_terms);
		free(scratch_substitutions.substitutions);
		return 0;
	}
	memcpy(scratch_substitutions.substitutions, substitutions->substitutions,
		substitutions->substitution_count * sizeof(*substitutions->substitutions));
	memset(scratch_substitutions.reindex_cache, 0,
		sizeof(scratch_substitutions.reindex_cache));
	uint32_t expected_classifier;
	int result = prototype_term_reindex(
		&scratch_terms, type_declarations, contexts, &scratch_substitutions,
		source_classifier, substitution_id, &expected_classifier
	) == 0 && match_branch_classifier_satisfies_motive(
		&scratch_terms, type_declarations,
		actual_classifier, expected_classifier
	);
	replay_term_scratch_free(&scratch_terms);
	free(scratch_substitutions.substitutions);
	return result;
}

static int replay_match_case_constructor_term(
	struct prototype_term_db* terms,
	const struct prototype_match_case* match_case,
	const struct prototype_case_binder* binders,
	uint32_t* p_constructor
) {
	if (!terms || !match_case || !binders || !p_constructor ||
		prototype_term_constructor(
			terms, match_case->constructor_owner,
			match_case->constructor_id, p_constructor
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		uint32_t field;
		if (prototype_term_var(terms, binders[i].binding_id, &field) != 0 ||
			prototype_term_app(terms, *p_constructor, field, p_constructor) != 0) {
			return -1;
		}
	}
	return 0;
}

static int validate_solved_match_motive_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!match_motive_result_classifier(terms, relation->subject, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[relation->subject];
	if (proof->premise_count > match->as.match.case_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* match_operation = NULL;
	uint32_t match_scrutinee_occurrence = PROTOTYPE_INVALID_ID;
	if (operations && relation->occurrence_id != PROTOTYPE_INVALID_ID) {
		if (relation->occurrence_id >= operations->occurrence_count) {
			return -1;
		}
		match_operation = &operations->occurrences[relation->occurrence_id];
		if (match_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			judgement_occurrence_child(
				operations, relation->occurrence_id,
				PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
				&match_scrutinee_occurrence
			) != 0 ||
			match_operation->core_term != relation->subject ||
			match_operation->classifier != relation->classifier ||
			match_operation->case_count != match->as.match.case_count ||
			match_operation->first_case > operations->case_count ||
			match_operation->case_count >
				operations->case_count - match_operation->first_case) {
			return -1;
		}
	}
	const struct prototype_term* motive_app = &terms->terms[relation->classifier];
	uint32_t motive_id = motive_app->as.app.function;
	while (motive_id < terms->term_count &&
		terms->terms[motive_id].tag == PROTOTYPE_TERM_APP) {
		motive_id = terms->terms[motive_id].as.app.function;
	}
	if (motive_id >= terms->term_count ||
		terms->terms[motive_id].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	const struct prototype_term* motive = &terms->terms[motive_id];
	if (motive->as.lambda.body >= terms->term_count) {
		return -1;
	}
	if (match_operation) {
		uint32_t premise_index = 0;
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			const struct prototype_typed_occurrence_match_case* typed_case =
				&operations->cases[match_operation->first_case + i];
			int refinement_status = replay_operation_case_refinement_status(
				substitutions, operations, relation->occurrence_id, i
			);
			if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
				continue;
			}
			if ((refinement_status != PROTOTYPE_INDEX_REFINEMENT_SOLVED &&
				 refinement_status != PROTOTYPE_INDEX_REFINEMENT_CONSTANT) ||
				premise_index >= proof->premise_count) {
				return -1;
			}
			uint32_t branch_occurrence;
			if (judgement_occurrence_child(
					operations, relation->occurrence_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
					&branch_occurrence
				) != 0) {
				return -1;
			}
			const struct prototype_judgement_premise_view* premise =
				&proof->premises[premise_index++];
			if (premise->proposition->kind !=
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				premise->proposition->occurrence_id != branch_occurrence ||
				premise->proposition->subject !=
					operations->occurrences[branch_occurrence].core_term ||
				!replay_match_premise_refinement_action(typed_case, premise) ||
				(refinement_status == PROTOTYPE_INDEX_REFINEMENT_SOLVED ?
					!replay_reindexed_classifier_satisfies(
						terms, type_declarations, contexts, substitutions,
						relation->classifier, typed_case->refinement_substitution,
						premise->proposition->classifier
					) : !match_branch_classifier_satisfies_motive(
						terms, type_declarations,
						premise->proposition->classifier, relation->classifier
					))) {
				return -1;
			}
		}
		return premise_index == proof->premise_count ? 0 : -1;
	}
	const struct prototype_term* motive_body = &terms->terms[motive->as.lambda.body];
	if (motive_body->tag != PROTOTYPE_TERM_MATCH) {
		/* A direct dependent motive is checked by applying it to the constructor
		 * pattern represented by each branch. Constant motives are the special
		 * case in which this beta-reduces to the same classifier. */
		uint32_t premise_index = 0;
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			uint32_t case_id = match->as.match.first_case + i;
			uint32_t expected_classifier;
			uint32_t constructor;
			uint32_t constructor_classifier;
			uint32_t argument_classifiers[16];
			if (case_id >= terms->case_count) {
				return -1;
			}
			const struct prototype_match_case* match_case = &terms->cases[case_id];
			uint32_t constructor_owner_view = match_case->constructor_owner;
			struct prototype_case_binder occurrence_binders[16];
			const struct prototype_case_binder* branch_binders =
				&terms->case_binders[match_case->first_binder];
			uint32_t owner_type_id;
			uint32_t owner_arguments[16];
			uint32_t owner_argument_count;
			if (prototype_type_declaration_instance_info(
					type_declarations, terms, match_case->constructor_owner,
					&owner_type_id, owner_arguments, 16, &owner_argument_count
				) != 0 || owner_type_id >= type_declarations->semantic_schema.type_count) {
				return -1;
			}
			if (match_operation &&
				type_declarations->semantic_schema.type_declarations[owner_type_id].index_count != 0) {
				uint32_t branch_occurrence;
				if (judgement_occurrence_child(
						operations, relation->occurrence_id,
						PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
						&branch_occurrence
					) != 0 ||
					match_scrutinee_occurrence >= operations->occurrence_count) {
					return -1;
				}
				int refinement_status = replay_operation_case_refinement_status(
					substitutions, operations, relation->occurrence_id, i
				);
				if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
					continue;
				}
				if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_INVALID) {
					return -1;
				}
				if (operation_case_occurrence_binders(
						contexts, substitutions, terms, operations,
						relation->occurrence_id, i, occurrence_binders, 16
					) != 0) {
					return -1;
				}
				branch_binders = occurrence_binders;
			}
			if (premise_index >= proof->premise_count) {
				return -1;
			}
			const struct prototype_judgement_premise_view* premise =
				&proof->premises[premise_index++];
			uint32_t expected_subject = terms->cases[case_id].body;
			if (match_operation) {
				const struct prototype_typed_occurrence_match_case* typed_case =
					&operations->cases[match_operation->first_case + i];
				uint32_t branch_occurrence;
				if (judgement_occurrence_child(
						operations, relation->occurrence_id,
						PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
						&branch_occurrence
					) != 0 ||
					premise->proposition->occurrence_id !=
						branch_occurrence ||
						!replay_match_premise_refinement_action(
							typed_case, premise
							)) {
					return -1;
				}
				expected_subject =
					operations->occurrences[branch_occurrence].core_term;
			}
			if (type_declarations->semantic_schema.type_declarations[owner_type_id].index_count == 0) {
				if (premise->proposition->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					premise->proposition->subject != expected_subject ||
					match_case_motive_classifier(
						terms, match_case, motive_app->as.app.function,
						&expected_classifier
					) != 0 || !match_branch_classifier_satisfies_motive(
						terms, type_declarations,
						premise->proposition->classifier,
						expected_classifier
					)) {
					return -1;
				}
				continue;
			}
			if (match_case->binder_count > 16 ||
				replay_match_case_constructor_term(
					terms, match_case, branch_binders, &constructor
				) != 0) {
				return -1;
			}
			if (match_operation) {
				const struct prototype_typed_occurrence_match_case* typed_case =
					&operations->cases[match_operation->first_case + i];
				constructor_owner_view = typed_case->constructor_owner;
			}
			for (uint32_t j = 0; j < match_case->binder_count; ++j) {
				uint32_t binding_context;
				const struct prototype_context* binding_entry;
				if (prototype_context_find_binding(
						contexts, premise->proposition->context_id,
						branch_binders[j].binding_id,
						&binding_context
					) != 0 || !(binding_entry = prototype_context_get(
						contexts, binding_context
					))) {
					return -1;
				}
				argument_classifiers[j] =
					prototype_context_classifier_term(binding_entry);
			}
			int saturated;
			int constructor_status = prototype_judgement_constructor_spine_classifier(
					terms, type_declarations, contexts, substitutions,
					premise->proposition->context_id,
					constructor, constructor_owner_view,
					argument_classifiers, match_case->binder_count,
					&constructor_classifier, &saturated
				);
			int motive_status = constructor_status ==
				PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID && saturated ?
				prototype_judgement_apply_indexed_motive(
					terms, type_declarations, motive_id, constructor,
					constructor_classifier, 1, &expected_classifier
					) : -1;
			if (constructor_status != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID ||
				!saturated || motive_status != 0 ||
				premise->proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					premise->proposition->subject != expected_subject ||
					premise->proposition->context_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			if (!match_branch_classifier_satisfies_motive(
					terms,
					type_declarations,
					premise->proposition->classifier,
					expected_classifier
				)) {
				const struct prototype_typed_occurrence_match_case* typed_case =
					match_operation ? &operations->cases[
						match_operation->first_case + i
					] : NULL;
				uint32_t stored_refined_expected = PROTOTYPE_INVALID_ID;
				if (typed_case && prototype_typed_occurrence_match_case_is_solved(typed_case) &&
					prototype_term_reindex(
						terms, type_declarations, contexts, substitutions,
						expected_classifier,
						typed_case->refinement_substitution,
						&stored_refined_expected
					) == 0 && match_branch_classifier_satisfies_motive(
						terms, type_declarations,
						premise->proposition->classifier,
						stored_refined_expected
					)) {
					continue;
				}
				return -1;
			}
		}
		return premise_index == proof->premise_count ? 0 : -1;
	}
	if (motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binding_id !=
			motive->as.lambda.binding_id) {
		return -1;
	}
	uint32_t premise_index = 0;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_body->as.match.first_case + i;
		if (case_id >= terms->case_count || motive_case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = &terms->cases[motive_case_id];
		if (match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count ||
			match_case->constructor_owner != motive_case->constructor_owner) {
			return -1;
		}
		uint32_t owner_type_id;
		uint32_t owner_arguments[16];
		uint32_t owner_argument_count;
		int owner_status = prototype_type_declaration_instance_info(
				type_declarations, terms, match_case->constructor_owner,
				&owner_type_id, owner_arguments, 16, &owner_argument_count
			);
		if (owner_status != 0 || owner_type_id >= type_declarations->semantic_schema.type_count) {
			return -1;
		}
		if (match_operation &&
			type_declarations->semantic_schema.type_declarations[owner_type_id].index_count != 0) {
			uint32_t branch_occurrence;
			if (judgement_occurrence_child(
					operations, relation->occurrence_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
					&branch_occurrence
				) != 0 ||
				match_scrutinee_occurrence >= operations->occurrence_count) {
				return -1;
			}
			int refinement_status = replay_operation_case_refinement_status(
				substitutions, operations, relation->occurrence_id, i
			);
			if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
				continue;
			}
			if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_INVALID) {
				return -1;
			}
		}
		if (premise_index >= proof->premise_count) {
			return -1;
		}
		const struct prototype_judgement_premise_view* premise =
			&proof->premises[premise_index++];
		uint32_t expected_subject = match_case->body;
		if (match_operation) {
			const struct prototype_typed_occurrence_match_case* typed_case =
				&operations->cases[match_operation->first_case + i];
			uint32_t branch_occurrence;
			if (judgement_occurrence_child(
					operations, relation->occurrence_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
					&branch_occurrence
				) != 0 ||
				premise->proposition->occurrence_id !=
					branch_occurrence ||
				!replay_match_premise_refinement_action(typed_case, premise)) {
				return -1;
			}
			expected_subject =
				operations->occurrences[branch_occurrence].core_term;
		}
		if (premise->proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			premise->proposition->subject != expected_subject) {
			return -1;
		}
		int found = 0;
		{
			uint32_t expected_motive_case_body = PROTOTYPE_INVALID_ID;
			int prepare_status = core_match_case_alpha_reindex(
					terms, type_declarations,
					&terms->case_binders[match_case->first_binder],
					&terms->case_binders[motive_case->first_binder],
					match_case->binder_count, premise->proposition->classifier,
					&expected_motive_case_body
				);
			if (prepare_status == 0) {
				int equal;
				equal = match_branch_classifier_satisfies_motive(
					terms,
					type_declarations,
					expected_motive_case_body,
					motive_case->body
				);
				if (equal) {
					found = 1;
				}
			}
			/* A structurally recursive motive records M(rest) as an IH node in
			 * its own match frame. This is a guarded equation, not a new WHNF
			 * conversion rule. Verify the frame and the substituted recursive
			 * binder explicitly. */
			if (prepare_status == 0 && motive_case->body < terms->term_count &&
				terms->terms[motive_case->body].tag ==
					PROTOTYPE_TERM_INDUCTION_HYPOTHESIS &&
				expected_motive_case_body < terms->term_count &&
				terms->terms[expected_motive_case_body].tag == PROTOTYPE_TERM_APP) {
				const struct prototype_term* motive_ih =
					&terms->terms[motive_case->body];
				const struct prototype_term* expected_app =
					&terms->terms[expected_motive_case_body];
				if (motive_body->as.match.ih_scope_id == motive_ih->as.induction_hypothesis.ih_scope_id &&
					expected_app->as.app.function == motive_app->as.app.function &&
					expected_app->as.app.argument == motive_ih->as.induction_hypothesis.argument) {
					found = 1;
				}
			}
		}
		if (!found) {
			return -1;
		}
	}
	if (premise_index != proof->premise_count) {
		return -1;
	}
	return 0;
}

static int initialize_exact_claim_rule(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* conclusion,
	int proof_kind,
	const uint32_t* premise_claim_ids,
	uint32_t premise_count,
	struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* proof_premises
) {
	if (!judgement || !conclusion || !proof || !proof_premises ||
		(premise_count != 0 && !premise_claim_ids) ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	memset(proof, 0, sizeof(*proof));
	proof->proof_kind = proof_kind;
	proof->conclusion_proposition_id = PROTOTYPE_INVALID_ID;
	proof->conclusion = conclusion;
	memset(
		proof_premises,
		0,
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES * sizeof(*proof_premises)
	);
	proof->premises = proof_premises;
	memset(&proof->rule_data, 0xff, sizeof(proof->rule_data));
	proof->semantic_action_kind = PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
	proof->semantic_action_id = PROTOTYPE_INVALID_ID;
	proof->premise_count = premise_count;
	for (uint32_t i = 0;
		i < PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES; ++i) {
		proof->premises[i].proposition_store_kind =
			PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_INVALID;
		proof->premises[i].proposition_id = PROTOTYPE_INVALID_ID;
		proof->premises[i].semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		proof->premises[i].semantic_action_id = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < premise_count; ++i) {
		const struct prototype_judgement_proposition* premise =
			prototype_judgement_claim_proposition(
				judgement, premise_claim_ids[i]
			);
		if (!premise) {
			return -1;
		}
		proof->premises[i].proposition_id =
			judgement->claims[premise_claim_ids[i]].proposition_id;
		proof->premises[i].proposition_store_kind =
			PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB;
		proof->premises[i].proposition =
			premise;
		proof->premises[i].builder_proposition = NULL;
	}
	return 0;
}

static int validate_return_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
);

static int validate_thunk_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
);

int prototype_judgement_add_lambda_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t binder_claim_id,
	uint32_t body_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[2] = { binder_claim_id, body_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			premise_ids, 2, &proof, premises
		) != 0 || initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0 || validate_lambda_intro_proof(
			terms, type_declarations, contexts, substitutions, NULL,
			&conclusion, &view
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_app_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t function_claim_id,
	uint32_t argument_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[2] = { function_claim_id, argument_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
			premise_ids, 2, &proof, premises
		) != 0 || initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0 || validate_app_elim_proof(
			terms, type_declarations, contexts, substitutions, &conclusion, &view
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

static int judgement_add_unary_intro_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t premise_claim_id,
	int proof_kind,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[1] = { premise_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement, &conclusion, proof_kind, premise_ids, 1, &proof, premises
		) != 0) {
		return -1;
	}
	if (initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0) {
		return -1;
	}
	if (proof_kind == PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO) {
		if (validate_return_intro_proof(
				terms, type_declarations, &conclusion, &view
			) != 0) {
			return -1;
		}
	} else if (proof_kind == PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO) {
		if (validate_thunk_intro_proof(
				terms, type_declarations, &conclusion, &view
			) != 0) {
			return -1;
		}
	} else {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_return_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t value_claim_id,
	uint32_t* p_claim_id
) {
	return judgement_add_unary_intro_claim(
		judgement,
		terms,
		type_declarations,
		context_id,
		subject,
		classifier,
		value_claim_id,
		PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
		p_claim_id
	);
}

int prototype_judgement_add_thunk_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t computation_claim_id,
	uint32_t* p_claim_id
) {
	return judgement_add_unary_intro_claim(
		judgement,
		terms,
		type_declarations,
		context_id,
		subject,
		classifier,
		computation_claim_id,
		PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
		p_claim_id
	);
}

int prototype_judgement_add_indexed_match_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t classifier_claim_id,
	const uint32_t* branch_claim_ids,
	const uint32_t* branch_substitution_ids,
	uint32_t branch_claim_count,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || (branch_claim_count != 0 && !branch_claim_ids) ||
		branch_claim_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	(void)operations;
	premise_ids[0] = classifier_claim_id;
	for (uint32_t i = 0; i < branch_claim_count; ++i) {
		premise_ids[i + 1] = branch_claim_ids[i];
	}
	if (initialize_exact_claim_rule(
			judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
			premise_ids, branch_claim_count + 1, &proof, premises
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < branch_claim_count; ++i) {
		if (!branch_substitution_ids) {
			continue;
		}
		if (!prototype_substitution_get(
				substitutions, branch_substitution_ids[i]
			)) {
			return -1;
		}
		premises[i + 1].semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
		premises[i + 1].semantic_action_id = branch_substitution_ids[i];
	}
	if (initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0 || validate_match_elim_proof(
			terms, type_declarations, contexts, substitutions,
			dimension_operators,
			judgement, &conclusion, &view
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_match_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t classifier_claim_id,
	const uint32_t* branch_claim_ids,
	uint32_t branch_claim_count,
	uint32_t* p_claim_id
) {
	return prototype_judgement_add_indexed_match_claim(
		judgement,
		terms,
		type_declarations,
		contexts,
		substitutions,
		dimension_operators,
		operations,
		context_id,
		subject,
		classifier,
		classifier_claim_id,
		branch_claim_ids,
		NULL,
		branch_claim_count,
		p_claim_id
	);
}

int prototype_judgement_add_induction_hypothesis_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t match,
	uint32_t motive,
	uint32_t case_index,
	uint32_t field_index,
	uint32_t* p_claim_id
) {
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement,
			&conclusion,
			PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
			NULL,
			0,
			&proof,
			premises
		) != 0) {
		return -1;
	}
	proof.rule_data.induction.match = match;
	proof.rule_data.induction.motive = motive;
	proof.rule_data.induction.case_index = case_index;
	proof.rule_data.induction.field_index = field_index;
	if (initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0 || validate_induction_hypothesis_elim_proof(
			terms, type_declarations, NULL, NULL, NULL, judgement, &conclusion, &view
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, NULL, p_claim_id
	);
}

static int validate_type_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms, relation->subject, &type_id, arguments, &argument_count
		) != 0 || type_id >= type_declarations->semantic_schema.type_count ||
		argument_count > 16) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->semantic_schema.type_declarations[type_id];
	if (argument_count == 0 &&
		type->formation_classifier != PROTOTYPE_INVALID_ID) {
		return type->formation_classifier == relation->classifier ? 0 : -1;
	}
	return type_instance_has_known_type(terms, type_declarations, relation->subject) &&
		term_is_universe_var(terms, relation->classifier) ? 0 : -1;
}

static int validate_computation_type_formation_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_COMPUTATION_TYPE) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* computation_type = &terms->terms[relation->subject];
	struct prototype_effect_row_normal_form effect_row;
	return prototype_term_effect_row_normal_form(
			terms, computation_type->as.computation_type.label, &effect_row
		) == 0 && proof->premises[0].proposition->kind ==
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[0].proposition->subject ==
			computation_type->as.computation_type.result &&
		proof->premises[0].proposition->classifier == relation->classifier ? 0 : -1;
}

static int validate_thunk_type_formation_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_THUNK_TYPE) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return proof->premises[0].proposition->kind ==
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[0].proposition->subject ==
			terms->terms[relation->subject].as.thunk_type.computation &&
		proof->premises[0].proposition->classifier == relation->classifier ? 0 : -1;
}

static int validate_constructor_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_CONSTRUCTOR) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* constructor = &terms->terms[relation->subject];
	if (!constructor_belongs_to_owner(
			terms,
			type_declarations,
			constructor->as.constructor.owner,
			constructor->as.constructor.constructor_id
		) ||
			!classifier_returns_owner(
				terms,
				type_declarations,
				relation->classifier,
				constructor->as.constructor.owner
			)) {
		return -1;
	}
	return 0;
}

static int validate_constructor_spine_formation_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!operations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count;
	if (prototype_term_constructor_spine_info(
			terms,
			relation->subject,
			&head,
			&owner,
			&constructor_index,
			arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&argument_count
		) != 0 || argument_count != proof->premise_count) {
		return -1;
	}
	uint32_t argument_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (proof->premises[i].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition->context_id != relation->context_id ||
			proof->premises[i].proposition->subject != arguments[i]) {
			return -1;
		}
		argument_classifiers[i] = proof->premises[i].proposition->classifier;
	}
	uint32_t expected_classifier;
	int saturated;
	int classifier_status = prototype_judgement_constructor_spine_classifier(
			terms,
			type_declarations,
			contexts,
			substitutions,
			relation->context_id,
			relation->subject,
			proof->rule_data.constructor.owner_view,
			argument_classifiers,
			argument_count,
			&expected_classifier,
			&saturated
		);
	int classifier_matches = classifier_status == 0 &&
		(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			expected_classifier,
			relation->classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL ||
		prototype_judgement_classifier_reference_equal(
			terms, type_declarations, expected_classifier, relation->classifier
		));
	if (!classifier_matches) {
		uint32_t projected_subject;
		uint32_t projected_classifier;
		uint32_t projected_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t projected_argument_classifiers[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		];
		int subject_projection_status =
			prototype_judgement_project_pure_sequence_results(
				terms,
			type_declarations,
			contexts,
			operations,
			relation->context_id,
			relation->subject,
				&projected_subject
			);
		int classifier_projection_status =
			prototype_judgement_project_pure_sequence_results(
				terms,
				type_declarations,
				contexts,
				operations,
				relation->context_id,
				relation->classifier,
				&projected_classifier
			);
		if (subject_projection_status != 0 || classifier_projection_status != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < argument_count; ++i) {
			if (prototype_judgement_project_pure_sequence_results(
					terms,
					type_declarations,
					contexts,
					operations,
					relation->context_id,
					arguments[i],
					&projected_arguments[i]
				) != 0 ||
				prototype_judgement_project_pure_sequence_results(
					terms,
					type_declarations,
					contexts,
					operations,
					relation->context_id,
					argument_classifiers[i],
					&projected_argument_classifiers[i]
				) != 0) {
				return -1;
			}
		}
		uint32_t projected_head;
		uint32_t projected_owner;
		uint32_t projected_constructor_index;
		uint32_t decomposed_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t projected_argument_count;
		if (prototype_term_constructor_spine_info(
				terms,
				projected_subject,
				&projected_head,
				&projected_owner,
				&projected_constructor_index,
				decomposed_arguments,
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
				&projected_argument_count
			) != 0 ||
			projected_argument_count != argument_count) {
			return -1;
		}
		for (uint32_t i = 0; i < argument_count; ++i) {
			if (decomposed_arguments[i] != projected_arguments[i]) {
				return -1;
			}
		}
		int projected_constructor_status =
			prototype_judgement_constructor_spine_classifier(
				terms,
				type_declarations,
				contexts,
				substitutions,
				relation->context_id,
				projected_subject,
				proof->rule_data.constructor.owner_view,
				projected_argument_classifiers,
				argument_count,
				&expected_classifier,
				&saturated
			);
		struct prototype_term_conversion_result projected_conversion =
			projected_constructor_status == 0 ?
			prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				expected_classifier,
				projected_classifier
			) : (struct prototype_term_conversion_result){0};
		if (projected_constructor_status != 0 ||
			(projected_conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL &&
			 !prototype_judgement_classifier_reference_equal(
				terms,
				type_declarations,
				expected_classifier,
				projected_classifier
			))) {
			return -1;
		}
		(void)projected_head;
		(void)projected_owner;
		(void)projected_constructor_index;
	}
	(void)head;
	(void)owner;
	(void)constructor_index;
	(void)saturated;
	return 0;
}

static int validate_match_type_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_exists(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier) ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_MATCH) ||
		proof->premise_count != terms->terms[relation->subject].as.match.case_count) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		uint32_t case_id = terms->terms[relation->subject].as.match.first_case + i;
		if (case_id >= terms->case_count ||
			terms->cases[case_id].body >= terms->term_count ||
			terms->cases[case_id].first_binder > terms->case_binder_count ||
			terms->cases[case_id].binder_count >
				terms->case_binder_count - terms->cases[case_id].first_binder ||
			!match_case_has_valid_constructor(
				terms,
				type_declarations,
				&terms->cases[case_id]
			) ||
			proof->premises[i].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition->subject != terms->cases[case_id].body ||
			proof->premises[i].proposition->classifier != relation->classifier) {
			return -1;
		}
	}
	return 0;
}

static int validate_universe_cumulativity_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_universe_var(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_conversion_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	struct prototype_term_conversion_result conversion;
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition->subject != relation->subject ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier) ||
		!term_exists(terms, proof->premises[0].proposition->classifier)) {
		return -1;
	}
	conversion = classifier_kernel_conversion(
			terms,
			type_declarations,
			NULL,
			relation->classifier,
			proof->premises[0].proposition->classifier
		);
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	return 0;
}

int prototype_judgement_add_conversion_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t expected_classifier,
	uint32_t source_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[1] = { source_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = expected_classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
			premise_ids, 1, &proof, premises
		) != 0 || initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0 || validate_conversion_proof(
			terms, type_declarations, &conclusion, &view
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

static int persisted_classifier_pair_satisfies(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
) {
	if (!terms || !type_declarations || expected >= terms->term_count ||
		actual >= terms->term_count) {
		return 0;
	}
	if (prototype_judgement_classifier_reference_equal(
			terms, type_declarations, expected, actual
		)) {
		return 1;
	}
	uint32_t solved_expected;
	return prototype_judgement_solve_expected_effect_rows(
			terms, type_declarations, NULL, expected, actual, &solved_expected
		) == 0 && prototype_judgement_classifier_compatible(
			terms, type_declarations, solved_expected, actual
		);
}

static int occurrence_classifier_satisfies_at_depth(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	uint32_t expected_classifier,
	uint32_t depth
) {
	if (!terms || !type_declarations || !contexts || !substitutions || !operations ||
		operation_id >= operations->occurrence_count ||
		expected_classifier >= terms->term_count || depth > 128) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[operation_id];
	if (operation->core_term >= terms->term_count ||
		operation->classifier >= terms->term_count) {
		return -1;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		if (operation->wrapped_occurrence >= operations->occurrence_count) {
			return -1;
		}
		return occurrence_classifier_satisfies_at_depth(
			terms, type_declarations, contexts, substitutions, operations,
			operation->wrapped_occurrence, expected_classifier, depth + 1
		);
	}
	if (persisted_classifier_pair_satisfies(
			terms, type_declarations, expected_classifier, operation->classifier
		)) {
		return 1;
	}
	uint32_t child_occurrence;
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK &&
		terms->terms[expected_classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		if (judgement_occurrence_child(
				operations, operation_id, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION,
				0, &child_occurrence
			) != 0) {
			return -1;
		}
		return occurrence_classifier_satisfies_at_depth(
			terms, type_declarations, contexts, substitutions, operations,
			child_occurrence,
			terms->terms[expected_classifier].as.thunk_type.computation,
			depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
		terms->terms[expected_classifier].tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		struct prototype_term_classifier_view actual_view;
		struct prototype_effect_row_normal_form expected_row;
		struct prototype_effect_row_normal_form actual_row;
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, operation->classifier, &actual_view
			) != 0 || actual_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			actual_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_normal_form(
				terms, terms->terms[expected_classifier].as.computation_type.label,
				&expected_row
			) != 0 || prototype_term_effect_row_normal_form(
				terms, actual_view.effect_row, &actual_row
			) != 0 || !prototype_term_effect_row_normal_form_includes(
				&expected_row, &actual_row
			) || judgement_occurrence_child(
				operations, operation_id, PROTOTYPE_TERM_CHILD_RETURN_VALUE,
				0, &child_occurrence
			) != 0) {
			return 0;
		}
		return occurrence_classifier_satisfies_at_depth(
			terms, type_declarations, contexts, substitutions, operations,
			child_occurrence,
			terms->terms[expected_classifier].as.computation_type.result,
			depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
		if (terms->terms[expected_classifier].tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
			uint32_t result = terms->terms[
				expected_classifier
			].as.computation_type.result;
			if (result < terms->term_count &&
				terms->terms[result].tag == PROTOTYPE_TERM_THUNK_TYPE) {
				expected_classifier = terms->terms[result].as.thunk_type.computation;
			}
		}
		uint32_t expected_domain;
		uint32_t expected_family;
		uint32_t expected_binder;
		uint32_t expected_body;
		uint32_t body_binder_var;
		uint32_t specialized_body;
		if (prototype_judgement_pi_parts(
				terms, expected_classifier, &expected_domain, &expected_family
			) != 0 || prototype_term_pure_family_parts(
				terms, expected_family, &expected_binder, &expected_body
			) != 0 || operation->binder_classifier >= terms->term_count ||
			!persisted_classifier_pair_satisfies(
				terms, type_declarations, expected_domain,
				operation->binder_classifier
			) || operation->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				terms, operation->binding_id, &body_binder_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				terms, type_declarations, expected_body, expected_binder,
				body_binder_var, &specialized_body
			) != 0 || judgement_occurrence_child(
				operations, operation_id, PROTOTYPE_TERM_CHILD_BODY, 0,
				&child_occurrence
			) != 0) {
			return 0;
		}
		return occurrence_classifier_satisfies_at_depth(
			terms, type_declarations, contexts, substitutions, operations,
			child_occurrence, specialized_body, depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
		if (operation->first_case > operations->case_count ||
			operation->case_count > operations->case_count - operation->first_case) {
			return -1;
		}
		for (uint32_t i = 0; i < operation->case_count; ++i) {
			const struct prototype_typed_occurrence_match_case* operation_case =
				&operations->cases[operation->first_case + i];
			int refinement_status = replay_operation_case_refinement_status(
				substitutions, operations, operation_id, i
			);
			if (refinement_status == PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
				continue;
			}
			if ((refinement_status != PROTOTYPE_INDEX_REFINEMENT_SOLVED &&
				 refinement_status != PROTOTYPE_INDEX_REFINEMENT_CONSTANT) ||
				judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
					&child_occurrence
				) != 0 || child_occurrence >= operations->occurrence_count) {
				return -1;
			}
			uint32_t refined_expected = expected_classifier;
			if ((refinement_status == PROTOTYPE_INDEX_REFINEMENT_SOLVED &&
				 prototype_term_reindex(
					terms, type_declarations, contexts, substitutions,
					expected_classifier, operation_case->refinement_substitution,
					&refined_expected
				 ) != 0) || !persisted_classifier_pair_satisfies(
					terms, type_declarations, refined_expected,
					operations->occurrences[child_occurrence].classifier
				)) {
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

int prototype_judgement_occurrence_classifier_satisfies(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	uint32_t expected_classifier
) {
	return occurrence_classifier_satisfies_at_depth(
		terms, type_declarations, contexts, substitutions, operations,
		operation_id, expected_classifier, 0
	);
}

static int validate_expected_type_exposure_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition->subject != relation->subject ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier) ||
		!term_exists(terms, proof->premises[0].proposition->classifier)) {
		return -1;
	}
	struct prototype_term_db scratch_terms;
	memset(&scratch_terms, 0, sizeof(scratch_terms));
	if (replay_term_scratch_init(terms, &scratch_terms) != 0) {
		return -1;
	}
	uint32_t solved_expected;
	int solve_status = prototype_judgement_solve_expected_effect_rows(
			&scratch_terms,
			type_declarations,
			NULL,
			relation->classifier,
			proof->premises[0].proposition->classifier,
			&solved_expected
		);
	if (solve_status == 0) {
		replay_term_scratch_free(&scratch_terms);
		return 0;
	}
	int compatible = classifier_kernel_compatible_at_depth(
		&scratch_terms,
		type_declarations,
		NULL,
		relation->classifier,
		proof->premises[0].proposition->classifier,
		0
	);
	if (!compatible && operations &&
		relation->occurrence_id != PROTOTYPE_INVALID_ID &&
		relation->occurrence_id < operations->occurrence_count) {
		const struct prototype_typed_occurrence* occurrence =
			&operations->occurrences[relation->occurrence_id];
		if (occurrence->core_term == relation->subject) {
			int occurrence_status =
				prototype_judgement_occurrence_classifier_satisfies(
					&scratch_terms, type_declarations, contexts, substitutions,
					operations, relation->occurrence_id, relation->classifier
				);
			compatible = occurrence_status == 1;
		}
	}
	if (!compatible) {
		fprintf(stderr,
			"expected-type exposure replay failed expected=%u actual=%u "
			"effect-status=%d\n",
			relation->classifier,
			proof->premises[0].proposition->classifier,
			solve_status);
	}
	replay_term_scratch_free(&scratch_terms);
	return compatible ? 0 : -1;
}

static int validate_effect_weaken_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition->subject != relation->subject ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier) ||
		!term_exists(terms, proof->premises[0].proposition->classifier)) {
		return -1;
	}
	struct prototype_term_classifier_view source;
	struct prototype_term_classifier_view target;
	struct prototype_effect_row_normal_form source_effects;
	struct prototype_effect_row_normal_form target_effects;
	memset(&source, 0, sizeof(source));
	memset(&target, 0, sizeof(target));
	memset(&source_effects, 0, sizeof(source_effects));
	memset(&target_effects, 0, sizeof(target_effects));
	int source_view_status = prototype_judgement_classifier_view(
			terms, type_declarations, NULL,
			proof->premises[0].proposition->classifier, &source
		);
	int target_view_status = prototype_judgement_classifier_view(
			terms, type_declarations, NULL, relation->classifier, &target
		);
	struct prototype_term_conversion_result result_conversion =
		(source_view_status == 0 && target_view_status == 0) ?
		prototype_judgement_classifier_conversion(
			terms, type_declarations, source.result, target.result
		) : (struct prototype_term_conversion_result) {
			.status = PROTOTYPE_TERM_CONVERSION_INVALID
		};
	int source_effect_status = source_view_status == 0 ?
		prototype_term_effect_row_normal_form(
			terms, source.effect_row, &source_effects
		) : -1;
	int target_effect_status = target_view_status == 0 ?
		prototype_term_effect_row_normal_form(
			terms, target.effect_row, &target_effects
		) : -1;
	int effect_inclusion = source_effect_status == 0 && target_effect_status == 0 ?
		prototype_term_effect_row_normal_form_includes(
			&target_effects, &source_effects
		) : 0;
	if (source_view_status != 0 || target_view_status != 0 ||
		source.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		target.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		target.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		result_conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
		source_effect_status != 0 || target_effect_status != 0 ||
		!effect_inclusion) {
		fprintf(
			stderr,
			"effect-weaken replay failed source=%u target=%u "
			"views=%d:%d categories=%d:%d computation-kinds=%d:%d "
			"results=%u:%u conversion=%d rows=%u:%u row-status=%d:%d "
			"includes=%d\n",
			proof->premises[0].proposition->classifier,
			relation->classifier,
			source_view_status,
			target_view_status,
			source.category,
			target.category,
			source.computation_kind,
			target.computation_kind,
			source.result,
			target.result,
			result_conversion.status,
			source.effect_row,
			target.effect_row,
			source_effect_status,
			target_effect_status,
			effect_inclusion
		);
		return -1;
	}
	return 0;
}

static int validate_declaration_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	if (term_has_tag(terms, relation->subject, PROTOTYPE_TERM_EXTERNAL_REF)) {
		return proof->premise_count == 0 &&
			term_is_structural_type(terms, type_declarations, relation->classifier) ? 0 : -1;
	}
	if (term_has_tag(terms, relation->subject, PROTOTYPE_TERM_CONSTRUCTOR)) {
		if (proof->premise_count != 0) {
			return -1;
		}
		const struct prototype_term* constructor = &terms->terms[relation->subject];
		uint32_t type_id;
		uint32_t args[16];
		uint32_t arg_count;
		int has_local_owner =
			prototype_term_type_instance_info(
				terms,
				constructor->as.constructor.owner,
				&type_id,
				args,
				&arg_count
			) == 0 &&
			type_id < type_declarations->semantic_schema.type_count;
		(void)args;
		(void)arg_count;
		if (has_local_owner &&
			!constructor_belongs_to_owner(
				terms,
				type_declarations,
				constructor->as.constructor.owner,
				constructor->as.constructor.constructor_id
			)) {
			return -1;
		}
			return classifier_returns_owner(
				terms,
				type_declarations,
				relation->classifier,
				constructor->as.constructor.owner
			) ? 0 : -1;
	}
	if (proof->premise_count != 1 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition->subject != relation->subject ||
		!term_exists(terms, proof->premises[0].proposition->classifier)) {
		return -1;
	}
	if (!prototype_judgement_classifier_compatible(
			terms,
			type_declarations,
			relation->classifier,
			proof->premises[0].proposition->classifier
		)) {
		return -1;
	}
	(void)judgement;
	return 0;
}

static int collect_accepted_subject_classifiers(
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
	for (uint32_t claim_id = 0; claim_id < judgement->claim_count; ++claim_id) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, claim_id);
		if (!proposition || proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proposition->subject != subject) {
			continue;
		}
		int has_non_conversion_derivation = 0;
		for (size_t derivation_id = 0;
			derivation_id < judgement->derivation_count;
			++derivation_id) {
			const struct prototype_judgement_derivation* derivation =
				&judgement->derivations[derivation_id];
			if (derivation->conclusion_claim_id == claim_id &&
				derivation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID &&
				derivation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
				has_non_conversion_derivation = 1;
				break;
			}
		}
		if (!has_non_conversion_derivation) {
			continue;
		}
		int contains = classifier_list_contains_normalization_equal(
			terms,
			type_declarations,
			classifiers,
			*p_classifier_count,
			proposition->classifier
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
		classifiers[(*p_classifier_count)++] = proposition->classifier;
	}
	return 0;
}

static int validate_assumption_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_VAR) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
		uint32_t classifier_classifier;
		uint32_t binding_id = terms->terms[relation->subject].as.var.binding_id;
		uint32_t assumption_context_id;
		if (prototype_context_find_binding(
				contexts,
				relation->context_id,
				binding_id,
				&assumption_context_id
			) != 0) {
			fprintf(
				stderr,
				"binder-assumption replay missing binding context=%u binding=%u "
				"subject=%u classifier=%u\n",
				relation->context_id,
				binding_id,
				relation->subject,
				relation->classifier
			);
			return -1;
		}
		const struct prototype_context* entry =
			prototype_context_get(contexts, assumption_context_id);
		uint32_t entry_classifier = prototype_context_classifier_term(entry);
		struct prototype_term_conversion_result entry_conversion =
			entry && entry_classifier != PROTOTYPE_INVALID_ID ?
			prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				entry_classifier,
				relation->classifier
			) : (struct prototype_term_conversion_result) {
				.status = PROTOTYPE_TERM_CONVERSION_INVALID
			};
		if (!entry || entry_classifier == PROTOTYPE_INVALID_ID ||
			entry_conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			fprintf(
				stderr,
				"binder-assumption replay classifier mismatch context=%u "
				"binding-context=%u binding=%u entry=%u(tag=%d) "
				"relation=%u(tag=%d) conversion=%d\n",
				relation->context_id,
				assumption_context_id,
				binding_id,
				entry_classifier,
				entry_classifier < terms->term_count ?
					terms->terms[entry_classifier].tag : PROTOTYPE_TERM_VAR - 1,
				relation->classifier,
				relation->classifier < terms->term_count ?
					terms->terms[relation->classifier].tag : PROTOTYPE_TERM_VAR - 1,
				entry_conversion.status
			);
			return -1;
		}
		if (term_is_structural_type(terms, type_declarations, relation->classifier)) {
			return 0;
		}
		uint32_t classifier_classifiers[32];
		uint32_t classifier_classifier_count = 0;
		if (collect_accepted_subject_classifiers(
				judgement,
				terms,
				type_declarations,
				relation->classifier,
				classifier_classifiers,
				32,
				&classifier_classifier_count
			) != 0) {
			fprintf(
				stderr,
				"binder-assumption replay classifier evidence collection failed "
				"classifier=%u\n",
				relation->classifier
			);
			return -1;
		}
		for (uint32_t i = 0; i < classifier_classifier_count; ++i) {
			classifier_classifier = classifier_classifiers[i];
			if (term_is_universe_var(terms, classifier_classifier)) {
				return 0;
			}
		}
		fprintf(
			stderr,
			"binder-assumption replay classifier lacks accepted Universe evidence "
			"context=%u binding=%u classifier=%u evidence-count=%u\n",
			relation->context_id,
			binding_id,
			relation->classifier,
			classifier_classifier_count
		);
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
		uint32_t binding_id = terms->terms[relation->subject].as.var.binding_id;
		uint32_t type_id;
		uint32_t arguments[64];
		uint32_t argument_count;
		if (proof->rule_data.constructor.owner_view == PROTOTYPE_INVALID_ID ||
			proof->rule_data.constructor.owner_view >= terms->term_count ||
			proof->rule_data.constructor.constructor_index == PROTOTYPE_INVALID_ID ||
			proof->rule_data.constructor.field_index >= 64 ||
			prototype_type_declaration_instance_info(
				type_declarations,
				terms,
				proof->rule_data.constructor.owner_view,
				&type_id,
				arguments,
				64,
				&argument_count
			) != 0 ||
			type_id >= type_declarations->semantic_schema.type_count ||
			relation->context_id >= contexts->context_count) {
			return -1;
		}
		const struct prototype_type_declaration* type =
			&type_declarations->semantic_schema.type_declarations[type_id];
		if (proof->rule_data.constructor.constructor_index >= type->constructor_count ||
			type->first_constructor + proof->rule_data.constructor.constructor_index >=
				type_declarations->semantic_schema.constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->semantic_schema.constructor_declarations[
				type->first_constructor + proof->rule_data.constructor.constructor_index
			];
		const struct prototype_context* parameter_context =
			prototype_context_get(contexts, constructor->parameter_context);
		const struct prototype_context* field_context =
			prototype_context_get(contexts, constructor->field_context);
		if (!parameter_context || !field_context ||
			field_context->depth < parameter_context->depth ||
			field_context->depth - parameter_context->depth > 16 ||
			proof->rule_data.constructor.field_index >=
				field_context->depth - parameter_context->depth) {
			return -1;
		}
		uint32_t field_count = field_context->depth - parameter_context->depth;
		const struct prototype_typed_occurrence_match_case* typed_case = NULL;
		if (operations) {
			for (uint32_t operation_id = 0;
				operation_id < operations->occurrence_count && !typed_case;
				++operation_id) {
				const struct prototype_typed_occurrence* operation =
					&operations->occurrences[operation_id];
				if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
					operation->first_case > operations->case_count ||
					operation->case_count >
						operations->case_count - operation->first_case) {
					continue;
				}
				for (uint32_t case_index = 0;
					case_index < operation->case_count; ++case_index) {
					const struct prototype_typed_occurrence_match_case* candidate =
						&operations->cases[operation->first_case + case_index];
					if (candidate->context_id == relation->context_id &&
						candidate->constructor_id ==
							proof->rule_data.constructor.constructor_index &&
						candidate->binder_count == field_count &&
						candidate->binder_ids[
							proof->rule_data.constructor.field_index
						] == binding_id) {
						typed_case = candidate;
						break;
					}
				}
			}
		}
		if (!typed_case) {
			return -1;
		}
		for (uint32_t i = 0; i < field_count; ++i) {
			if (!prototype_context_contains_binding(
					contexts, relation->context_id,
					typed_case->binder_ids[i]
				)) {
				return -1;
			}
		}
		if (typed_case->binder_ids[
				proof->rule_data.constructor.field_index
			] != binding_id) {
			return -1;
		}
		if ((prototype_typed_occurrence_match_case_is_solved(typed_case) &&
			 (proof->semantic_action_kind !=
					PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			  proof->semantic_action_id != typed_case->refinement_substitution)) ||
			(!prototype_typed_occurrence_match_case_is_solved(typed_case) &&
			 proof->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID)) {
			return -1;
		}
		uint32_t binding_context;
		if (prototype_context_find_binding(
				contexts, relation->context_id, binding_id, &binding_context
			) != 0) {
			return -1;
		}
		const struct prototype_context* binding_entry =
			prototype_context_get(contexts, binding_context);
		uint32_t expected_classifier =
			prototype_context_classifier_term(binding_entry);
		if (!binding_entry || expected_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_classifier_conversion(
				terms, type_declarations, expected_classifier,
				relation->classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return -1;
			}
		return 0;
	}
	return 0;
}

static int validate_text_literal_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_TEXT_LITERAL) ||
		!term_is_primitive_text(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_int_literal_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	int classifier_host_type;
	if (!terms || !intrinsic_environment || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_INT_LITERAL) ||
		relation->classifier >= terms->term_count ||
		prototype_term_host_type_from_term_tag(
			terms->terms[relation->classifier].tag, &classifier_host_type
		) != 0 || classifier_host_type !=
			intrinsic_environment->default_integer_host_type) {
		return -1;
	}
	int bit_width = prototype_term_host_type_bit_width(classifier_host_type);
	int64_t value = terms->terms[relation->subject].as.int_literal.value;
	if ((bit_width == 32 && (value < INT32_MIN || value > INT32_MAX)) ||
		(bit_width != 32 && bit_width != 64)) {
		return -1;
	}
	return 0;
}

static int validate_int_literal_admissibility_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof || proof->premise_count != 1 ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_is_primitive_int(terms, relation->classifier) ||
		!int_literal_fits_int32(
			terms->terms[relation->subject].as.int_literal.value
		) || proof->premises[0].proposition->subject != relation->subject ||
		!term_is_primitive_integer(terms, proof->premises[0].proposition->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_return_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_RETURN) ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	const struct prototype_term* classifier = &terms->terms[relation->classifier];
	return proof->premises[0].proposition->subject == terms->terms[relation->subject].as.return_term.value &&
		prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			proof->premises[0].proposition->classifier,
			classifier->as.computation_type.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL &&
		prototype_term_effect_row_purity(
			terms, classifier->as.computation_type.label
		) == PROTOTYPE_EFFECT_ROW_PURITY_PURE ? 0 : -1;
}

static int validate_thunk_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_THUNK) ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	return proof->premises[0].proposition->subject == terms->terms[relation->subject].as.thunk.computation &&
		prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			proof->premises[0].proposition->classifier,
			terms->terms[relation->classifier].as.thunk_type.computation
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 0 : -1;
}

static int validate_force_elim_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_FORCE) ||
		proof->premises[0].proposition->classifier >= terms->term_count ||
		terms->terms[proof->premises[0].proposition->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	return proof->premises[0].proposition->subject == terms->terms[relation->subject].as.force.value &&
		relation->classifier ==
			terms->terms[proof->premises[0].proposition->classifier].as.thunk_type.computation ? 0 : -1;
}

static int computation_fold_carrier_compatible(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t left,
	uint32_t right
) {
	struct prototype_term_classifier_view left_view;
	struct prototype_term_classifier_view right_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, left, &left_view
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, right, &right_view
		) != 0 || left_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		right_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		left_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		right_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, left_view.result, right_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 0;
	}
	struct prototype_effect_row_normal_form left_effects;
	struct prototype_effect_row_normal_form right_effects;
	if (prototype_term_effect_row_normal_form(
			terms, left_view.effect_row, &left_effects
		) != 0 || prototype_term_effect_row_normal_form(
			terms, right_view.effect_row, &right_effects
		) != 0) {
		/* Partial-policy artifacts retain this equality as a residual obligation.
		 * Strict policy rejects the artifact until both rows are materialized. */
		return 1;
	}
	/* A return clause, operation clause, or resumption may use fewer effects
	 * than the fold carrier. The validator checks the exact union of all local
	 * rows against the result row after validating these inclusions. */
	return prototype_term_effect_row_normal_form_includes(
		&right_effects, &left_effects
	);
}

static int validate_computation_fold_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_COMPUTATION_FOLD) ||
		proof->premises[0].proposition->classifier >= terms->term_count ||
		proof->premises[1].proposition->classifier >= terms->term_count ||
		relation->classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* fold = &terms->terms[relation->subject];
	if (fold->as.computation_fold.clause_count >
			PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
		proof->premise_count != 2 + 2 * fold->as.computation_fold.clause_count ||
		proof->premises[0].proposition->subject != fold->as.computation_fold.computation ||
		proof->premises[1].proposition->subject != fold->as.computation_fold.return_clause ||
		fold->as.computation_fold.first_clause + fold->as.computation_fold.clause_count >
			terms->computation_fold_clause_count) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		if (proof->premises[i].proposition->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition->context_id != relation->context_id) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[fold->as.computation_fold.first_clause + i];
		if (proof->premises[2 + 2 * i].proposition->subject != clause->operation ||
			proof->premises[3 + 2 * i].proposition->subject != clause->body) {
			return -1;
		}
	}
	if (fold->as.computation_fold.clause_count != 0) {
		struct prototype_term_classifier_view input;
		struct prototype_term_classifier_view result;
		if (prototype_judgement_classifier_view(
				terms,
				type_declarations,
				NULL,
				proof->premises[0].proposition->classifier,
				&input
			) != 0 || prototype_judgement_classifier_view(
				terms,
				type_declarations,
				NULL,
				relation->classifier,
				&result
			) != 0 || input.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			result.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		uint32_t return_domain;
		uint32_t return_family;
		uint32_t return_binder;
		uint32_t return_body;
		struct prototype_term_classifier_view return_result;
		if (pi_parts(
				terms,
				proof->premises[1].proposition->classifier,
				&return_domain,
				&return_family
			) != 0 || prototype_term_pure_family_parts(
				terms, return_family, &return_binder, &return_body
			) != 0 || prototype_judgement_classifier_view(
				terms, type_declarations, NULL, return_body, &return_result
			) != 0 || return_result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			return_result.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			!(prototype_judgement_classifier_conversion(
				terms, type_declarations, return_domain, input.result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || !(prototype_judgement_classifier_conversion(
				terms, type_declarations, return_result.result, result.result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		(void)return_binder;
		uint32_t handled_row;
		if (prototype_term_effect_row_empty(terms, &handled_row
			) != 0) {
			return -1;
		}
		uint32_t clause_body_rows[
			PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
		];
		for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
			const struct prototype_computation_fold_clause* clause =
				&terms->computation_fold_clauses[
					fold->as.computation_fold.first_clause + i
				];
			int operation_identity;
			if (prototype_term_effect_operation_identity(
					terms, clause->operation, &operation_identity
				) != 0) {
				return -1;
			}
			const struct prototype_effect_operation_declaration* declaration =
				prototype_term_effect_operation_declaration(operation_identity);
			if (!declaration) {
				return -1;
			}
			uint32_t empty_row;
			uint32_t operation_atom;
			uint32_t next_handled_row;
			if (prototype_term_effect_row_empty(terms, &empty_row
				) != 0 || prototype_term_effect_row_operation(
					terms, operation_identity, empty_row, &operation_atom
				) != 0 || prototype_term_effect_row_union(
					terms, handled_row, operation_atom, &next_handled_row
				) != 0) {
				return -1;
			}
			handled_row = next_handled_row;

			uint32_t operation_classifier = proof->premises[2 + 2 * i].proposition->classifier;
			int specialization_status = prototype_judgement_specialize_fold_operation_classifier(
				terms,
				type_declarations,
				input.effect_row,
				operation_identity,
				operation_classifier,
				&operation_classifier
			);
			if (specialization_status != 0) {
				return -1;
			}
			for (uint32_t depth = 0;
				depth < 32 && operation_classifier < terms->term_count &&
				terms->terms[operation_classifier].tag ==
					PROTOTYPE_TERM_EFFECT_ROW_FORALL;
				++depth) {
				operation_classifier =
					terms->terms[operation_classifier].as.effect_row_forall.body;
			}
			uint32_t operation_domain;
			uint32_t operation_family;
			uint32_t operation_binder;
			uint32_t operation_body;
			struct prototype_term_classifier_view operation_result;
			if (pi_parts(
					terms,
					operation_classifier,
					&operation_domain,
					&operation_family
				) != 0 || prototype_term_pure_family_parts(
					terms, operation_family, &operation_binder, &operation_body
				) != 0 || prototype_judgement_classifier_view(
					terms, type_declarations, NULL, operation_body, &operation_result
				) != 0 || operation_result.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				operation_result.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				return -1;
			}
			(void)operation_binder;

			uint32_t outer_domain;
			uint32_t outer_family;
			uint32_t outer_binder;
			uint32_t inner_classifier;
			uint32_t inner_domain;
			uint32_t inner_family;
			uint32_t inner_binder;
			uint32_t inner_body;
			if (pi_parts(
					terms,
					proof->premises[3 + 2 * i].proposition->classifier,
					&outer_domain,
					&outer_family
				) != 0 || prototype_term_pure_family_parts(
					terms, outer_family, &outer_binder, &inner_classifier
				) != 0 || pi_parts(
					terms,
					inner_classifier,
					&inner_domain,
					&inner_family
				) != 0 || prototype_term_pure_family_parts(
					terms, inner_family, &inner_binder, &inner_body
				) != 0 || inner_domain >= terms->term_count ||
				terms->terms[inner_domain].tag != PROTOTYPE_TERM_THUNK_TYPE) {
				return -1;
			}
			uint32_t instantiated_outer_domain;
			uint32_t instantiated_inner_body;
			if (instantiate_fold_clause_classifier(
					terms,
					type_declarations,
					outer_domain,
					operation_domain,
					outer_domain,
					&instantiated_outer_domain
				) != 0 || instantiate_fold_clause_classifier(
					terms,
					type_declarations,
					outer_domain,
					operation_domain,
					inner_body,
					&instantiated_inner_body
				) != 0 || !(prototype_judgement_classifier_conversion(
					terms, type_declarations, instantiated_outer_domain, operation_domain
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || !computation_fold_carrier_compatible(
					terms,
					type_declarations,
					instantiated_inner_body,
					relation->classifier
				)) {
				return -1;
			}
			inner_body = instantiated_inner_body;
			struct prototype_term_classifier_view clause_body_result;
			if (prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					inner_body,
					&clause_body_result
				) != 0 || clause_body_result.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				clause_body_result.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				return -1;
			}
			clause_body_rows[i] = clause_body_result.effect_row;
			uint32_t continuation_function =
				terms->terms[inner_domain].as.thunk_type.computation;
			uint32_t continuation_domain;
			uint32_t continuation_family;
			uint32_t continuation_binder;
			uint32_t continuation_body;
			if (pi_parts(
					terms,
					continuation_function,
					&continuation_domain,
					&continuation_family
				) != 0 || prototype_term_pure_family_parts(
					terms,
					continuation_family,
					&continuation_binder,
					&continuation_body
				) != 0 || !(prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					continuation_domain,
					operation_result.result
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || !computation_fold_carrier_compatible(
					terms,
					type_declarations,
					continuation_body,
					relation->classifier
				)) {
				return -1;
			}
			(void)outer_binder;
			(void)inner_binder;
			(void)continuation_binder;
		}
		uint32_t residual_row;
		int residual_status = prototype_term_effect_row_residual(
			terms, input.effect_row, handled_row, &residual_row
		);
		if (residual_status != 0) {
			return -1;
		}
		uint32_t expected_row;
		if (prototype_term_effect_row_union(
				terms, return_result.effect_row, residual_row, &expected_row
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
			uint32_t joined_row;
			if (prototype_term_effect_row_union(
					terms, expected_row, clause_body_rows[i], &joined_row
				) != 0) {
				return -1;
			}
			expected_row = joined_row;
		}
		int rows_equal = 0;
		return prototype_term_view_shape_equal(
			terms, expected_row, result.effect_row, &rows_equal
		) == 0 && rows_equal ? 0 : -1;
	}
	uint32_t expected_result;
	if (prototype_judgement_computation_fold_result_classifier(
			terms,
			type_declarations,
			fold->as.computation_fold.computation,
			proof->premises[0].proposition->classifier,
			proof->premises[1].proposition->classifier,
			&expected_result
		) != 0 || !(prototype_judgement_classifier_conversion(
			terms, type_declarations, relation->classifier, expected_result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return 0;
}

int prototype_judgement_add_force_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t value_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[1] = { value_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement,
			&conclusion,
			PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM,
			premise_ids,
			1,
			&proof,
			premises
		) != 0 || initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0 || validate_force_elim_proof(
			terms, &conclusion, &view
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_computation_fold_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_claim_ids,
	uint32_t premise_count,
	uint32_t* p_claim_id
) {
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!premise_claim_ids || !p_claim_id || premise_count == 0 ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		initialize_exact_claim_rule(
			judgement,
			&conclusion,
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
			premise_claim_ids,
			premise_count,
			&proof,
			premises
		) != 0 || initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0 || validate_computation_fold_elim_proof(
			terms, type_declarations, &conclusion, &view
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement,
		&conclusion,
		&proof,
		premise_claim_ids,
		p_claim_id
	);
}

static int validate_operation_request_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || proof->premise_count != 3 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_OPERATION_REQUEST) ||
		proof->premises[1].proposition->subject !=
			terms->terms[relation->subject].as.operation_request.argument ||
		proof->premises[2].proposition->subject !=
			terms->terms[relation->subject].as.operation_request.continuation ||
		proof->premises[0].proposition->classifier >= terms->term_count ||
		proof->premises[1].proposition->classifier >= terms->term_count ||
		proof->premises[2].proposition->classifier >= terms->term_count ||
		relation->classifier >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		if (proof->premises[i].proposition->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition->context_id != relation->context_id) {
			return -1;
		}
	}
	const struct prototype_term* request = &terms->terms[relation->subject];
	if (!term_has_tag(terms, proof->premises[0].proposition->subject, PROTOTYPE_TERM_APP) ||
		terms->terms[proof->premises[0].proposition->subject].as.app.function !=
			request->as.operation_request.operation ||
		terms->terms[proof->premises[0].proposition->subject].as.app.argument !=
			request->as.operation_request.argument) {
		return -1;
	}
	uint32_t operation_head =
		terms->terms[relation->subject].as.operation_request.operation;
	while (operation_head < terms->term_count &&
		terms->terms[operation_head].tag == PROTOTYPE_TERM_APP) {
		operation_head = terms->terms[operation_head].as.app.function;
	}
	if (operation_head >= terms->term_count ||
		terms->terms[operation_head].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	int operation_id = terms->terms[operation_head].as.effect_operation.operation_id;
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	if (!declaration) {
		return -1;
	}
	struct prototype_term_classifier_view operation;
	struct prototype_term_classifier_view result;
	if (prototype_term_classifier_view(terms, proof->premises[0].proposition->classifier, &operation) != 0 ||
		prototype_term_classifier_view(terms, relation->classifier, &result) != 0 ||
		operation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		result.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t continuation_thunk =
		terms->terms[relation->subject].as.operation_request.continuation;
	if (continuation_thunk >= terms->term_count ||
		terms->terms[continuation_thunk].tag != PROTOTYPE_TERM_THUNK ||
		terms->terms[continuation_thunk].as.thunk.computation >= terms->term_count) {
		return -1;
	}
	uint32_t continuation_lambda = terms->terms[continuation_thunk].as.thunk.computation;
	if (terms->terms[continuation_lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[proof->premises[2].proposition->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t continuation_function_classifier =
		terms->terms[proof->premises[2].proposition->classifier].as.thunk_type.computation;
	uint32_t domain;
	uint32_t family;
	if (pi_parts(terms, continuation_function_classifier, &domain, &family) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, domain, operation.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	uint32_t binder_var;
	uint32_t continuation_result;
	uint32_t continuation_context;
	uint32_t projection;
	if (prototype_term_var(
			terms, terms->terms[continuation_lambda].as.lambda.binding_id, &binder_var
		) != 0 ||
		prototype_context_extend(
			contexts,
			relation->context_id,
			terms->terms[continuation_lambda].as.lambda.binding_id,
			operation.result,
			PROTOTYPE_INVALID_ID,
			&continuation_context
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, continuation_context, &projection
		) != 0 ||
		instantiate_pure_family_in_context(
			contexts,
			substitutions,
			terms,
			type_declarations,
			projection,
			operation.result,
			family,
			binder_var,
			operation.result,
			&continuation_result
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view continuation;
	struct prototype_term_classifier_view request_operation = operation;
	int has_suspended_argument;
	if (prototype_term_effect_operation_classifier_has_suspended_argument(
			terms,
			terms->terms[operation_head].as.effect_operation.classifier,
			&has_suspended_argument
		) != 0) {
		return -1;
	}
	if (has_suspended_argument) {
		if (terms->terms[proof->premises[1].proposition->classifier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE) {
			return -1;
		}
		struct prototype_term_classifier_view argument;
		if (prototype_term_classifier_view(
				terms,
				terms->terms[proof->premises[1].proposition->classifier].as.thunk_type.computation,
				&argument
			) != 0 || argument.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			argument.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_operation(
				terms, operation_id, argument.effect_row, &request_operation.effect_row
			) != 0) {
			return -1;
		}
	}
	if (prototype_term_classifier_view(terms, continuation_result, &continuation) != 0 ||
		continuation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!computation_effect_row_is_union(
			terms, type_declarations, &result, &request_operation, &continuation
		) ||
		result.result != continuation.result || prototype_term_contains_free_binding(
			terms, continuation.result, terms->terms[continuation_lambda].as.lambda.binding_id
		)) {
		return -1;
	}
	(void)family;
	return 0;
}


static int validate_text_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_primitive_text(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_int_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_primitive_integer(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_host_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_host_primitive(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int term_is_host_type_id(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	int host_type
) {
	if (!term_exists(terms, term_id)) {
		return 0;
	}
	int actual_host_type;
	return prototype_term_host_type_from_term_tag(
			terms->terms[term_id].tag,
			&actual_host_type
		) == 0 &&
		actual_host_type == host_type;
}

static int validate_host_signature_classifier(
	const struct prototype_term_db* terms,
	uint32_t classifier,
	uint32_t arity,
	const int* argument_types,
	int result_type
) {
	if (!terms || !argument_types || result_type == PROTOTYPE_HOST_TYPE_INVALID) {
		return -1;
	}
	uint32_t current = classifier;
	for (uint32_t i = 0; i < arity; ++i) {
		uint32_t domain;
		uint32_t codomain_family;
		uint32_t binding_id;
		uint32_t codomain;
		if (argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID ||
			pi_parts(terms, current, &domain, &codomain_family) != 0 ||
			prototype_term_pure_family_parts(
				terms, codomain_family, &binding_id, &codomain
			) != 0 ||
			!term_is_host_type_id(terms, domain, argument_types[i])) {
			return -1;
		}
		(void)binding_id;
		current = codomain;
	}
	if (!term_exists(terms, current) ||
		terms->terms[current].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	uint32_t label = terms->terms[current].as.computation_type.label;
	uint32_t result = terms->terms[current].as.computation_type.result;
	if (!term_exists(terms, label) ||
		prototype_term_effect_row_purity(terms, label) !=
			PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return -1;
	}
	return term_is_host_type_id(terms, result, result_type) ? 0 : -1;
}

static int validate_pure_primitive_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !type_declarations || !contexts || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_PURE_PRIMITIVE) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}

	const struct prototype_term* primitive = &terms->terms[relation->subject];
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t codomain_binder;
	uint32_t codomain;
	if (pi_parts(terms, relation->classifier, &domain, &codomain_family) != 0 ||
		prototype_term_pure_family_parts(
			terms, codomain_family, &codomain_binder, &codomain
		) != 0) {
		return -1;
	}
	const struct prototype_pure_primitive_declaration* signature =
		prototype_term_pure_primitive_declaration(primitive->as.pure_primitive.primitive_id);
	if (signature &&
		signature->result_type != PROTOTYPE_HOST_TYPE_INVALID) {
		int host_only = 1;
		for (uint32_t i = 0; i < signature->arity; ++i) {
			if (signature->argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID) {
				host_only = 0;
			}
		}
		if (host_only) {
			return validate_host_signature_classifier(
				terms,
				relation->classifier,
				signature->arity,
				signature->argument_types,
				signature->result_type
			);
		}
	}
	(void)codomain_binder;
	if (codomain >= terms->term_count ||
		terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}

	uint32_t effect_row = terms->terms[codomain].as.computation_type.label;
	uint32_t result = terms->terms[codomain].as.computation_type.result;
	if (effect_row >= terms->term_count || result >= terms->term_count ||
		prototype_term_effect_row_purity(terms, effect_row) !=
			PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return -1;
	}
	if (primitive->as.pure_primitive.primitive_id == PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT) {
		return term_is_primitive_text(terms, domain) &&
			type_formation_is_nat_shape(
				terms, type_declarations, contexts, result
			) &&
			type_formation_has_name_symbol(
				terms,
				type_declarations,
				result,
				primitive->as.pure_primitive.type_symbol_id
			) ? 0 : -1;
	}
	if (primitive->as.pure_primitive.primitive_id == PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT) {
		return type_formation_is_nat_shape(
				terms, type_declarations, contexts, domain
			) &&
			type_formation_has_name_symbol(
				terms,
				type_declarations,
				domain,
				primitive->as.pure_primitive.type_symbol_id
			) &&
			term_is_primitive_text(terms, result) ? 0 : -1;
	}
	return -1;
}

static int validate_reindex_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof,
	int require_projection
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(
			substitutions, proof ? proof->semantic_action_id : PROTOTYPE_INVALID_ID
		);
	uint32_t subject;
	uint32_t classifier;
	if (!contexts || !substitutions || !relation || !proof || !substitution ||
		(relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		 relation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition->kind != relation->kind ||
		proof->semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		substitution->source_context != relation->context_id ||
		substitution->target_context != proof->premises[0].proposition->context_id) {
		fprintf(
			stderr,
			"P0 reindex header failed action=%u relation-context=%u "
			"premise-context=%u substitution=%u->%u kind=%d action-kind=%d\n",
			proof ? proof->semantic_action_id : PROTOTYPE_INVALID_ID,
			relation ? relation->context_id : PROTOTYPE_INVALID_ID,
			proof && proof->premise_count != 0 ?
				proof->premises[0].proposition->context_id : PROTOTYPE_INVALID_ID,
			substitution ? substitution->source_context : PROTOTYPE_INVALID_ID,
			substitution ? substitution->target_context : PROTOTYPE_INVALID_ID,
			proof ? proof->premise_count : 0,
			proof ? proof->semantic_action_kind : -1
		);
		return -1;
	}
	if (require_projection) {
		return proof->premises[0].proposition->context_id != relation->context_id &&
			proof->premises[0].proposition->subject == relation->subject &&
			proof->premises[0].proposition->classifier == relation->classifier &&
			prototype_substitution_is_projection_path(
				substitutions,
				contexts,
				proof->semantic_action_id,
				relation->context_id,
				proof->premises[0].proposition->context_id
			) ? 0 : -1;
	}
	if (!terms || !type_declarations || prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			proof->premises[0].proposition->subject,
			proof->semantic_action_id,
			&subject
		) != 0 || prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			proof->premises[0].proposition->classifier,
			proof->semantic_action_id,
			&classifier
		) != 0) {
		fprintf(stderr, "P0 reindex evaluation failed action=%u\n", proof->semantic_action_id);
		return -1;
	}
	if (subject != relation->subject ||
		prototype_judgement_classifier_conversion(
			terms, type_declarations, classifier, relation->classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	return 0;
}

static int validate_context_weaken_proof(
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	return validate_reindex_proof(
		NULL,
		NULL,
		contexts,
		(struct prototype_substitution_db*)substitutions,
		relation,
		proof,
		1
	);
}

static int validate_substitution_reindex_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	return validate_reindex_proof(
		terms,
		type_declarations,
		contexts,
		substitutions,
		relation,
		proof,
		0
	);
}

static int validate_relation_type_formation_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t left_classifier;
	uint32_t right_classifier;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premise_count != 4 ||
		prototype_term_relation_type_info(
			terms,
			relation->subject,
			&left_classifier,
			&right_classifier,
			&left_endpoint,
			&right_endpoint
		) != 0) {
		return -1;
	}
	return proof->premises[0].proposition->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
		proof->premises[0].proposition->context_id == relation->context_id &&
		proof->premises[0].proposition->subject == left_classifier &&
		proof->premises[0].proposition->classifier == relation->classifier &&
		proof->premises[1].proposition->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
		proof->premises[1].proposition->context_id == relation->context_id &&
		proof->premises[1].proposition->subject == right_classifier &&
		proof->premises[1].proposition->classifier == relation->classifier &&
		proof->premises[2].proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[2].proposition->context_id == relation->context_id &&
		proof->premises[2].proposition->subject == left_endpoint &&
		proof->premises[2].proposition->classifier == left_classifier &&
		proof->premises[3].proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[3].proposition->context_id == relation->context_id &&
		proof->premises[3].proposition->subject == right_endpoint &&
		proof->premises[3].proposition->classifier == right_classifier ? 0 : -1;
}

static int validate_relation_witness_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	struct prototype_term_conversion_result comparison;
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 3 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left ||
		witness_right != relation_right ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition->context_id != relation->context_id ||
		proof->premises[0].proposition->subject != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->context_id != relation->context_id ||
		proof->premises[1].proposition->subject != relation_left ||
		proof->premises[1].proposition->classifier != left_type ||
		proof->premises[2].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition->context_id != relation->context_id ||
		proof->premises[2].proposition->subject != relation_right ||
		proof->premises[2].proposition->classifier != right_type ||
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
	return 0;
}

static int validate_terminates_type_formation_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t suspended;
	return terms && relation && proof &&
		relation->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
		relation->classifier < terms->term_count &&
		terms->terms[relation->classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
		proof->premise_count == 1 && prototype_term_terminates_type_info(
			terms, relation->subject, &suspended
		) == 0 && proof->premises[0].proposition->kind ==
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[0].proposition->context_id == relation->context_id &&
		proof->premises[0].proposition->subject == suspended &&
		proof->premises[0].proposition->classifier < terms->term_count &&
		terms->terms[proof->premises[0].proposition->classifier].tag ==
			PROTOTYPE_TERM_THUNK_TYPE ? 0 : -1;
}

static int validate_terminates_total_computation_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t suspended;
	uint32_t witness_computation;
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 2 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition->context_id != relation->context_id ||
		proof->premises[0].proposition->subject != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->context_id != relation->context_id ||
		prototype_term_terminates_type_info(
			terms, relation->classifier, &suspended
		) != 0 || prototype_term_terminates_witness_info(
			terms, relation->subject, &witness_computation
		) != 0 || witness_computation != suspended ||
		proof->premises[1].proposition->subject != suspended ||
		proof->premises[1].proposition->classifier >= terms->term_count ||
		terms->terms[proof->premises[1].proposition->classifier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t computation_type = terms->terms[
		proof->premises[1].proposition->classifier
	].as.thunk_type.computation;
	return computation_type < terms->term_count &&
		terms->terms[computation_type].tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		terms->terms[computation_type].as.computation_type.totality ==
			PROTOTYPE_COMPUTATION_TOTALITY_TOTAL ? 0 : -1;
}

static int validate_relation_constructor_witness_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count < 3) {
		return -1;
	}
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
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
	uint32_t field_count = proof->premise_count - 3;
	if (prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition->context_id != relation->context_id ||
		proof->premises[0].proposition->subject != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->context_id != relation->context_id ||
		proof->premises[1].proposition->subject != relation_left ||
		proof->premises[1].proposition->classifier != left_type ||
		proof->premises[2].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition->context_id != relation->context_id ||
		proof->premises[2].proposition->subject != relation_right ||
		proof->premises[2].proposition->classifier != right_type ||
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
		left_constructor != right_constructor || left_argument_count != field_count ||
		right_argument_count != field_count) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		uint32_t field_left_type;
		uint32_t field_right_type;
		uint32_t field_left;
		uint32_t field_right;
		uint32_t premise = i + 3;
		if (proof->premises[premise].proposition->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[premise].proposition->context_id != relation->context_id ||
			prototype_term_relation_type_info(
				terms,
				proof->premises[premise].proposition->classifier,
				&field_left_type,
				&field_right_type,
				&field_left,
				&field_right
			) != 0 || field_left != left_arguments[i] ||
			field_right != right_arguments[i]) {
			return -1;
		}
	}
	(void)left_head;
	(void)right_head;
	return 0;
}

static int validate_relation_unary_witness_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof,
	int endpoint_tag
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t child_left_type;
	uint32_t child_right_type;
	uint32_t child_left;
	uint32_t child_right;
	uint32_t left_payload;
	uint32_t right_payload;
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 4 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != endpoint_tag ||
		terms->terms[relation_right].tag != endpoint_tag ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition->context_id != relation->context_id ||
		proof->premises[0].proposition->subject != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->context_id != relation->context_id ||
		proof->premises[1].proposition->subject != relation_left ||
		proof->premises[1].proposition->classifier != left_type ||
		proof->premises[2].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition->context_id != relation->context_id ||
		proof->premises[2].proposition->subject != relation_right ||
		proof->premises[2].proposition->classifier != right_type ||
		proof->premises[3].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[3].proposition->context_id != relation->context_id ||
		prototype_term_relation_type_info(
			terms,
			proof->premises[3].proposition->classifier,
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
	return child_left == left_payload && child_right == right_payload ? 0 : -1;
}

static int validate_relation_app_witness_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 5 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_APP ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_APP ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition->context_id != relation->context_id ||
		proof->premises[0].proposition->subject != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->context_id != relation->context_id ||
		proof->premises[1].proposition->subject != relation_left ||
		proof->premises[1].proposition->classifier != left_type ||
		proof->premises[2].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition->context_id != relation->context_id ||
		proof->premises[2].proposition->subject != relation_right ||
		proof->premises[2].proposition->classifier != right_type) {
		return -1;
	}
	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t child_left_type;
		uint32_t child_right_type;
		uint32_t child_left;
		uint32_t child_right;
		uint32_t premise = i + 3;
		uint32_t expected_left = i == 0 ?
			terms->terms[relation_left].as.app.function :
			terms->terms[relation_left].as.app.argument;
		uint32_t expected_right = i == 0 ?
			terms->terms[relation_right].as.app.function :
			terms->terms[relation_right].as.app.argument;
		if (proof->premises[premise].proposition->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[premise].proposition->context_id != relation->context_id ||
			prototype_term_relation_type_info(
				terms,
				proof->premises[premise].proposition->classifier,
				&child_left_type,
				&child_right_type,
				&child_left,
				&child_right
			) != 0 || child_left != expected_left ||
			child_right != expected_right) {
			return -1;
		}
	}
	return 0;
}

static int validate_relation_lambda_witness_proof(
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	if (!terms || !contexts || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 4 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition->context_id != relation->context_id ||
		proof->premises[0].proposition->subject != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->context_id != relation->context_id ||
		proof->premises[1].proposition->subject != relation_left ||
		proof->premises[1].proposition->classifier != left_type ||
		proof->premises[2].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition->context_id != relation->context_id ||
		proof->premises[2].proposition->subject != relation_right ||
		proof->premises[2].proposition->classifier != right_type ||
		proof->premises[3].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!relation_lambda_witness_shape_valid(
			terms,
			contexts,
			relation->context_id,
			relation->subject,
			relation->classifier,
			proof->premises[3].proposition->context_id,
			proof->premises[3].proposition->classifier
		)) {
		return -1;
	}
	return 0;
}

static int validate_relation_match_witness_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	if (!terms || !type_declarations || !contexts || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count < 5 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation_left].as.match.case_count != proof->premise_count - 5 ||
		terms->terms[relation_right].as.match.case_count != proof->premise_count - 5 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition->context_id != relation->context_id ||
		proof->premises[0].proposition->subject != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->context_id != relation->context_id ||
		proof->premises[1].proposition->subject != relation_left ||
		proof->premises[1].proposition->classifier != left_type ||
		proof->premises[2].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition->context_id != relation->context_id ||
		proof->premises[2].proposition->subject != relation_right ||
		proof->premises[2].proposition->classifier != right_type ||
		proof->premises[3].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[3].proposition->subject >= terms->term_count ||
		terms->terms[proof->premises[3].proposition->subject].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[proof->premises[3].proposition->subject].as.match.case_count !=
			proof->premise_count - 5) {
		return -1;
	}
	for (uint32_t i = 4; i < proof->premise_count; ++i) {
		uint32_t child_left_type;
		uint32_t child_right_type;
		uint32_t child_left;
		uint32_t child_right;
		uint32_t expected_left;
		uint32_t expected_right;
		if (i == 4) {
			expected_left = terms->terms[relation_left].as.match.scrutinee;
			expected_right = terms->terms[relation_right].as.match.scrutinee;
		} else {
			uint32_t case_index = i - 5;
			uint32_t source_case =
				terms->terms[proof->premises[3].proposition->subject].as.match.first_case +
				case_index;
			uint32_t left_case =
				terms->terms[relation_left].as.match.first_case + case_index;
			uint32_t right_case =
				terms->terms[relation_right].as.match.first_case + case_index;
			struct prototype_judgement_proposition case_proposition =
				*proof->premises[i].proposition;
			if (source_case >= terms->case_count || left_case >= terms->case_count ||
				right_case >= terms->case_count ||
				!relation_match_case_witness_shape_valid(
					terms,
					type_declarations,
					contexts,
					relation->context_id,
					&terms->cases[source_case],
					&terms->cases[left_case],
					&terms->cases[right_case],
					&case_proposition
				)) {
				return -1;
			}
			continue;
		}
		if (proof->premises[i].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition->context_id != relation->context_id ||
			prototype_term_relation_type_info(
				terms,
				proof->premises[i].proposition->classifier,
				&child_left_type,
				&child_right_type,
				&child_left,
				&child_right
			) != 0 || child_left != expected_left || child_right != expected_right) {
			return -1;
		}
	}
	return 0;
}

static int validate_relation_induction_hypothesis_witness_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
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
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 5 || prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition->context_id != relation->context_id ||
		proof->premises[0].proposition->subject != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->context_id != relation->context_id ||
		proof->premises[1].proposition->subject != relation_left ||
		proof->premises[1].proposition->classifier != left_type ||
		proof->premises[2].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition->context_id != relation->context_id ||
		proof->premises[2].proposition->subject != relation_right ||
		proof->premises[2].proposition->classifier != right_type ||
		proof->premises[3].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[3].proposition->subject >= terms->term_count ||
		terms->terms[proof->premises[3].proposition->subject].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		proof->premises[4].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[4].proposition->context_id != relation->context_id ||
		prototype_term_relation_type_info(
			terms,
			proof->premises[4].proposition->classifier,
			&argument_left_type,
			&argument_right_type,
			&argument_left,
			&argument_right
		) != 0 || argument_left != terms->terms[relation_left].
			as.induction_hypothesis.argument || argument_right !=
			terms->terms[relation_right].as.induction_hypothesis.argument) {
		return -1;
	}
	return 0;
}

static int validate_is_type_from_has_type_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition->subject != relation->subject ||
		proof->premises[0].proposition->classifier != relation->classifier ||
		!term_exists(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_pi_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 3 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_PI) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	uint32_t domain;
	uint32_t codomain_family;
	if (pi_parts(terms, relation->subject, &domain, &codomain_family) != 0) {
		return -1;
	}
	uint32_t family = terms->terms[relation->subject].as.pi.codomain_family;
	uint32_t binding_id;
	uint32_t codomain;
	if (prototype_term_pure_family_parts(
			terms, family, &binding_id, &codomain
		) != 0 || family >= terms->term_count ||
		terms->terms[family].tag != PROTOTYPE_TERM_THUNK) {
		return -1;
	}
	uint32_t lambda = terms->terms[family].as.thunk.computation;
	if (lambda >= terms->term_count ||
		terms->terms[lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[lambda].as.lambda.binding_id != binding_id ||
		terms->terms[lambda].as.lambda.body >= terms->term_count ||
		terms->terms[terms->terms[lambda].as.lambda.body].tag != PROTOTYPE_TERM_RETURN ||
		terms->terms[terms->terms[lambda].as.lambda.body].as.return_term.value != codomain ||
		proof->premises[2].proposition->classifier >= terms->term_count ||
		terms->terms[proof->premises[2].proposition->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t lambda_classifier =
		terms->terms[proof->premises[2].proposition->classifier].as.thunk_type.computation;
	if (lambda_classifier >= terms->term_count ||
		terms->terms[lambda_classifier].tag != PROTOTYPE_TERM_PI ||
		terms->terms[lambda_classifier].as.pi.domain != domain) {
		return -1;
	}
	uint32_t lambda_result_binder;
	uint32_t lambda_result_body;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[lambda_classifier].as.pi.codomain_family,
			&lambda_result_binder,
			&lambda_result_body
		) != 0) {
		return -1;
	}
	uint32_t binder_var;
	uint32_t returned_classifier;
	if (prototype_term_var(
			(struct prototype_term_db*)terms, binding_id, &binder_var
		) != 0 || prototype_term_graph_substitute_bound_var(
			(struct prototype_term_db*)terms,
			NULL,
			lambda_result_body,
			lambda_result_binder,
			binder_var,
			&returned_classifier
		) != 0 || returned_classifier >= terms->term_count ||
		terms->terms[returned_classifier].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	uint32_t effect_row = terms->terms[returned_classifier].as.computation_type.label;
	if (prototype_term_effect_row_purity(terms, effect_row) !=
			PROTOTYPE_EFFECT_ROW_PURITY_PURE ||
		terms->terms[returned_classifier].as.computation_type.result != relation->classifier) {
		return -1;
	}
	if (proof->premises[0].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition->subject != domain ||
		proof->premises[0].proposition->classifier != relation->classifier ||
		proof->premises[1].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition->subject != codomain ||
		proof->premises[1].proposition->classifier != relation->classifier ||
		proof->premises[2].proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition->subject != family ||
		proof->premises[2].proposition->classifier >= terms->term_count ||
		terms->terms[proof->premises[2].proposition->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		terms->terms[proof->premises[2].proposition->classifier].as.thunk_type.computation != lambda_classifier) {
		return -1;
	}
	(void)codomain_family;
	return 0;
}

int prototype_judgement_add_pi_formation_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe,
	uint32_t domain_claim_id,
	uint32_t codomain_claim_id,
	uint32_t family_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[3] = {
		domain_claim_id,
		codomain_claim_id,
		family_claim_id
	};
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = universe
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_rule_application_view view;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement,
			&conclusion,
			PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO,
			premise_ids,
			3,
			&proof,
			premises
		) != 0 || initialize_candidate_rule_application_view(
			&conclusion, &proof, &view
		) != 0 || validate_pi_formation_intro_proof(
			terms, &conclusion, &view
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

static int validate_effect_operation_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_EFFECT_OPERATION) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* operation = &terms->terms[relation->subject];
	if (!prototype_term_effect_operation_declaration(
			operation->as.effect_operation.operation_id
		) || operation->as.effect_operation.classifier >= terms->term_count) {
		return -1;
	}
	return relation->classifier == operation->as.effect_operation.classifier ? 0 : -1;
}

static int accepted_derivation_rule_application_view(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation* derivation,
	struct prototype_judgement_rule_application_view* view
) {
	if (!judgement || !derivation || !view ||
		derivation->conclusion_claim_id >= judgement->claim_count ||
		derivation->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	const struct prototype_judgement_claim* conclusion =
		&judgement->claims[derivation->conclusion_claim_id];
	const struct prototype_judgement_proposition* conclusion_proposition =
		prototype_judgement_proposition_get(
			judgement, conclusion->proposition_id
		);
	if (!conclusion_proposition) {
		return -1;
	}
	memset(view, 0, sizeof(*view));
	view->proof_kind = derivation->proof_kind;
	view->conclusion = conclusion_proposition;
	view->rule_data = derivation->rule_data;
	view->semantic_action_kind = derivation->semantic_action_kind;
	view->semantic_action_id = derivation->semantic_action_id;
	view->premise_count = derivation->premise_count;
	for (uint32_t i = 0; i < derivation->premise_count; ++i) {
		view->premises[i].semantic_action_kind =
			derivation->premises[i].semantic_action_kind;
		view->premises[i].semantic_action_id =
			derivation->premises[i].semantic_action_id;
		uint32_t claim_id = derivation->premises[i].claim_id;
		if (claim_id == PROTOTYPE_INVALID_ID) {
			const struct prototype_judgement_proposition* scoped =
				prototype_judgement_premise_proposition(
					judgement, &derivation->premises[i]
				);
			if (!scoped) {
				fprintf(stderr, "P0 missing scoped premise index=%u\n", i);
				return -1;
			}
			view->premises[i].proposition = scoped;
			continue;
		}
		if (claim_id >= judgement->claim_count) {
			fprintf(
				stderr,
				"P0 invalid accepted premise index=%u claim=%u count=%zu\n",
				i,
				claim_id,
				judgement->claim_count
			);
			return -1;
		}
		const struct prototype_judgement_claim* premise =
			&judgement->claims[claim_id];
		view->premises[i].proposition =
			prototype_judgement_proposition_get(
				judgement, premise->proposition_id
			);
		if (!view->premises[i].proposition) {
			return -1;
		}
	}
	return 0;
}

static int validate_proof_relation_coverage(
	const struct prototype_judgement_db* judgement
) {
	if (!judgement) {
		return -1;
	}
	for (uint32_t claim_id = 0;
		claim_id < (uint32_t)judgement->claim_count; ++claim_id) {
		if (judgement->claims[claim_id].closure_rank == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (!prototype_judgement_claim_proposition(judgement, claim_id)) {
			continue;
		}
		int found = 0;
		for (uint32_t derivation_id = 0;
			derivation_id < (uint32_t)judgement->derivation_count;
			++derivation_id) {
			const struct prototype_judgement_derivation* derivation =
				&judgement->derivations[derivation_id];
			if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
				continue;
			}
			if (derivation->conclusion_claim_id != claim_id) {
				continue;
			}
			if (derivation->closure_rank == PROTOTYPE_INVALID_ID) {
				continue;
			}
			if (derivation->closure_rank < judgement->claims[claim_id].closure_rank) {
				return -1;
			}
			for (uint32_t premise = 0;
				premise < derivation->premise_count; ++premise) {
				uint32_t source = derivation->premises[premise].claim_id;
				if (source != PROTOTYPE_INVALID_ID &&
					(source >= judgement->claim_count ||
					 judgement->claims[source].closure_rank >=
						derivation->closure_rank)) {
					return -1;
				}
			}
			found = 1;
		}
		if (!found) {
			fprintf(
				stderr,
				"P0 grounded claim lacks derivation claim=%u rank=%u proposition=%u\n",
				claim_id,
				judgement->claims[claim_id].closure_rank,
				judgement->claims[claim_id].proposition_id
			);
			return -1;
		}
	}
	return 0;
}

static int validate_proof_rule_parameters(
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!proof) {
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN ||
		proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX ||
		(proof->proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION &&
		 proof->semantic_action_kind ==
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION)) {
		if (proof->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			proof->semantic_action_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	} else if (proof->semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID ||
		proof->semantic_action_id != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
		return proof->rule_data.constructor.owner_view != PROTOTYPE_INVALID_ID &&
			proof->rule_data.constructor.constructor_index != PROTOTYPE_INVALID_ID &&
			proof->rule_data.constructor.field_index != PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
		return proof->rule_data.induction.match != PROTOTYPE_INVALID_ID &&
			proof->rule_data.induction.motive != PROTOTYPE_INVALID_ID &&
			proof->rule_data.induction.case_index != PROTOTYPE_INVALID_ID &&
			proof->rule_data.induction.field_index != PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->proof_kind ==
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
		return proof->rule_data.constructor.owner_view != PROTOTYPE_INVALID_ID &&
			proof->rule_data.constructor.constructor_index == PROTOTYPE_INVALID_ID &&
			proof->rule_data.constructor.field_index == PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->rule_data.induction.match != PROTOTYPE_INVALID_ID ||
		proof->rule_data.induction.motive != PROTOTYPE_INVALID_ID ||
		proof->rule_data.induction.case_index != PROTOTYPE_INVALID_ID ||
		proof->rule_data.induction.field_index != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	return 0;
}

static int judgement_classifier_free_result_effect_row_binder(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_binder_id
) {
	if (!terms || !type_declarations || !p_binder_id ||
		classifier >= terms->term_count) {
		return -1;
	}
	*p_binder_id = PROTOTYPE_INVALID_ID;
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, classifier, &view
		) != 0 ||
		view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	if (view.effect_row >= terms->term_count ||
		terms->terms[view.effect_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return 0;
	}
	uint32_t binding_id = terms->terms[view.effect_row].as.effect_row_var.binding_id;
	if (!prototype_term_contains_free_binding(
		terms,
		classifier,
		binding_id
	)) {
		return 0;
	}
	*p_binder_id = binding_id;
	return 1;
}

static int judgement_claim_has_effect_row_assumption(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t claim_id,
	uint32_t binding_id,
	size_t depth
) {
	if (!judgement || !terms || claim_id >= judgement->claim_count ||
		depth > judgement->claim_count) {
		return -1;
	}
	const struct prototype_judgement_claim* claim = &judgement->claims[claim_id];
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id != claim_id) {
			continue;
		}
		if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
			if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier < terms->term_count &&
				prototype_term_contains_free_binding(
					terms, prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier, binding_id
				)) {
				return 1;
			}
			continue;
		}
		for (uint32_t source = 0;
			source < derivation->premise_count; ++source) {
			uint32_t source_claim_id = derivation->premises[source].claim_id;
			if (source_claim_id == PROTOTYPE_INVALID_ID) {
				continue;
			}
			int found = judgement_claim_has_effect_row_assumption(
				judgement,
				terms,
				source_claim_id,
				binding_id,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
		}
	}
	return 0;
}

static int judgement_derivation_has_effect_row_assumption(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_derivation* derivation,
	uint32_t binding_id
) {
	if (!judgement || !terms || !derivation) {
		return -1;
	}
	for (uint32_t source = 0;
		source < derivation->premise_count; ++source) {
		uint32_t source_claim_id = derivation->premises[source].claim_id;
		if (source_claim_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int found = judgement_claim_has_effect_row_assumption(
			judgement,
			terms,
			source_claim_id,
			binding_id,
			0
		);
		if (found != 0) {
			return found;
		}
	}
	return 0;
}

static int judgement_operation_owns_effect_row_binder(
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	uint32_t binding_id
) {
	if (!terms || !operations || operation_id >= operations->occurrence_count) {
		return 0;
	}
	for (uint32_t owner = 0; owner < operations->occurrence_count; ++owner) {
		const struct prototype_typed_occurrence* lambda =
			&operations->occurrences[owner];
		if (lambda->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
			continue;
		}
		int declares_binder = 0;
		for (uint32_t row = 0; row < lambda->implicit_effect_row_count; ++row) {
			if (lambda->implicit_effect_row_binders[row] == binding_id) {
				declares_binder = 1;
				break;
			}
		}
		if (!declares_binder) {
			continue;
		}
		int contains = prototype_typed_occurrence_graph_reaches(
			operations, terms, owner, operation_id
		);
		if (contains < 0) {
			return -1;
		}
		if (contains) {
			return 1;
		}
		/* NAME and ASCRIPTION wrappers can own the Lambda occurrence while the
		 * accepted relation is attached to the wrapper itself. */
		contains = prototype_typed_occurrence_graph_reaches(
			operations, terms, operation_id, owner
		);
		if (contains < 0) {
			return -1;
		}
		if (contains) {
			return 1;
		}
	}
	return 0;
}

static int validate_operation_owned_relation(
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!relation || !proof) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		const struct prototype_judgement_premise_view* premise =
			&proof->premises[i];
		if (premise->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID) {
			if (premise->semantic_action_id != PROTOTYPE_INVALID_ID) {
				return -1;
			}
			continue;
		}
		const struct prototype_substitution* action =
			premise->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ?
			prototype_substitution_get(
				substitutions, premise->semantic_action_id
			) : NULL;
		if (!action || action->source_context !=
				premise->proposition->context_id) {
			return -1;
		}
	}
	if (relation->occurrence_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (!operations || relation->occurrence_id >= operations->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[relation->occurrence_id];
	int context_matches = operation->context_id == relation->context_id;
	int weakening_action_matches = 0;
	if (!context_matches &&
		proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN &&
		proof->premise_count == 1 &&
		proof->premises[0].proposition->occurrence_id == relation->occurrence_id) {
		weakening_action_matches = prototype_substitution_is_projection_path(
			substitutions,
			contexts,
			proof->semantic_action_id,
			relation->context_id,
			proof->premises[0].proposition->context_id
		);
		context_matches = weakening_action_matches;
	}
	if (operation->core_term != relation->subject ||
		operation->classifier == PROTOTYPE_INVALID_ID ||
		!context_matches) {
		const struct prototype_substitution* semantic_substitution =
			proof->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ?
			prototype_substitution_get(
				substitutions, proof->semantic_action_id
			) : NULL;
		fprintf(
			stderr,
			"P0 occurrence owner mismatch occurrence=%u tag=%d core=%u subject=%u "
			"classifier=%u relation_classifier=%u context=%u relation_context=%u "
			"rule=%d premise_context=%u premise_occurrence=%u action=%d:%u valid=%d "
			"action_context=%u->%u action_tag=%d\n",
			relation->occurrence_id,
			operation->tag,
			operation->core_term,
			relation->subject,
			operation->classifier,
			relation->classifier,
			operation->context_id,
			relation->context_id,
			proof->proof_kind,
			proof->premise_count != 0 ? proof->premises[0].proposition->context_id :
				PROTOTYPE_INVALID_ID,
			proof->premise_count != 0 ? proof->premises[0].proposition->occurrence_id :
				PROTOTYPE_INVALID_ID,
			proof->semantic_action_kind,
			proof->semantic_action_id,
			weakening_action_matches,
			semantic_substitution ? semantic_substitution->source_context :
				PROTOTYPE_INVALID_ID,
			semantic_substitution ? semantic_substitution->target_context :
				PROTOTYPE_INVALID_ID,
			semantic_substitution ? semantic_substitution->kind : 0
		);
		return -1;
	}
	/* Literal overload and the explicitly derived rules can expose a classifier
	 * other than the selected solver result without changing that result. */
	if (judgement_rule_derives_alternate_operation_classifier(proof->proof_kind)) {
		return 0;
	}
	if (operation->classifier != relation->classifier) {
		fprintf(
			stderr,
			"P0 occurrence classifier mismatch occurrence=%u classifier=%u "
			"relation_classifier=%u rule=%d\n",
			relation->occurrence_id,
			operation->classifier,
			relation->classifier,
			proof->proof_kind
		);
		return -1;
	}
	return 0;
}

static int operation_core_projection_equal(
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	uint32_t projected_core
) {
	if (!terms || !operations || operation_id >= operations->occurrence_count ||
		projected_core >= terms->term_count) {
		return 0;
	}
	uint32_t operation_core = operations->occurrences[operation_id].core_term;
	if (operation_core == projected_core) {
		return 1;
	}
	int equal = 0;
	return operation_core < terms->term_count &&
		prototype_term_core_shape_equal(
			terms, operation_core, projected_core, &equal
		) == 0 && equal;
}

static int term_core_projection_equal(
	const struct prototype_term_db* terms,
	uint32_t left,
	uint32_t right
) {
	if (!terms || left >= terms->term_count || right >= terms->term_count) {
		return 0;
	}
	if (left == right) {
		return 1;
	}
	int equal = 0;
	return prototype_term_core_shape_equal(
		terms, left, right, &equal
	) == 0 && equal;
}

static int operation_match_case_body_projection_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t match_operation_id,
	uint32_t case_index,
	const struct prototype_typed_occurrence_match_case* typed_case,
	const struct prototype_match_case* erased_case,
	uint32_t erased_ih_scope
) {
	uint32_t body_occurrence;
	if (!terms || !contexts || !operations || !typed_case || !erased_case ||
		match_operation_id >= operations->occurrence_count ||
		typed_case->binder_count != erased_case->binder_count ||
		typed_case->binder_count > 16 ||
		judgement_occurrence_child(
			operations, match_operation_id,
			PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, case_index,
			&body_occurrence
		) != 0 ||
		erased_case->first_binder > terms->case_binder_count ||
		erased_case->binder_count > terms->case_binder_count -
			erased_case->first_binder) {
		return 0;
	}
	uint32_t typed_binders[16];
	uint32_t erased_binders[16];
	uint32_t context_id = typed_case->context_id;
	for (uint32_t reverse = typed_case->binder_count; reverse > 0; --reverse) {
		const struct prototype_context* context =
			prototype_context_get(contexts, context_id);
		if (!context || context->binding_id == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		typed_binders[reverse - 1] = context->binding_id;
		erased_binders[reverse - 1] = terms->case_binders[
			erased_case->first_binder + reverse - 1
		].binding_id;
		context_id = context->parent;
	}
	uint32_t typed_body = operations->occurrences[body_occurrence].core_term;
	uint32_t expected_body = erased_case->body;
	if (prototype_typed_occurrence_match_case_is_solved(typed_case)) {
		const struct prototype_substitution* refinement =
			prototype_substitution_get(
				substitutions, typed_case->refinement_substitution
			);
		if (!refinement || refinement->source_context != typed_case->context_id ||
			prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				erased_case->body,
				typed_case->refinement_substitution,
				&expected_body
			) != 0) {
			fprintf(stderr,
				"occurrence Match refinement replay failed occurrence=%u case=%u "
				"context=%u substitution=%u source=%u target=%u\n",
				match_operation_id, case_index, typed_case->context_id,
				typed_case->refinement_substitution,
				refinement ? refinement->source_context : PROTOTYPE_INVALID_ID,
				refinement ? refinement->target_context : PROTOTYPE_INVALID_ID);
			return 0;
		}
	}
	uint32_t typed_ih_scope = PROTOTYPE_INVALID_ID;
	for (uint32_t candidate = 0; candidate < operations->occurrence_count;
		++candidate) {
		const struct prototype_typed_occurrence* induction =
			&operations->occurrences[candidate];
		if (induction->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
			induction->ih_owner_occurrence != match_operation_id ||
			induction->ih_case_index != case_index) {
			continue;
		}
		if (typed_ih_scope != PROTOTYPE_INVALID_ID &&
			typed_ih_scope != induction->ih_scope_id) {
			return 0;
		}
		typed_ih_scope = induction->ih_scope_id;
	}
	if (typed_body == expected_body) {
		return 1;
	}
	int equal = 0;
	int compare_status = typed_body < terms->term_count ?
		prototype_term_core_shape_equal_under_binders_and_ih_scope(
			terms,
			typed_binders,
			erased_binders,
			typed_case->binder_count,
			typed_ih_scope,
			typed_ih_scope == PROTOTYPE_INVALID_ID ?
				PROTOTYPE_INVALID_ID : erased_ih_scope,
			typed_body,
			expected_body,
			&equal
		) : -1;
	return compare_status == 0 && equal;
}

static int judgement_operation_expected_polarity(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	int* p_category
) {
	if (!terms || !type_declarations || !operations || !p_category ||
		operation_id >= operations->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[operation_id];
	if (operation->classifier >= terms->term_count ||
		operation->core_term >= terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view classifier_view;
	if (prototype_judgement_classifier_view(
			terms,
			type_declarations,
			NULL,
			operation->classifier,
			&classifier_view
		) != 0) {
		return -1;
	}
	int category = classifier_view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION ?
		PROTOTYPE_TERM_CATEGORY_COMPUTATION :
		PROTOTYPE_TERM_CATEGORY_VALUE;
	int core_tag = terms->terms[operation->core_term].tag;
	switch (operation->tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR:
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
			category = PROTOTYPE_TERM_CATEGORY_VALUE;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
			category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_ATOM:
			if (core_tag == PROTOTYPE_TERM_TYPE_VIEW ||
				core_tag == PROTOTYPE_TERM_TYPE_FORMER ||
				core_tag == PROTOTYPE_TERM_TYPE_DECLARATION) {
				category = PROTOTYPE_TERM_CATEGORY_VALUE;
			}
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
			if (core_tag == PROTOTYPE_TERM_APP) {
				uint32_t head = operation->core_term;
				while (head < terms->term_count &&
					terms->terms[head].tag == PROTOTYPE_TERM_APP) {
					head = terms->terms[head].as.app.function;
				}
				if (head < terms->term_count &&
					(terms->terms[head].tag == PROTOTYPE_TERM_PURE_PRIMITIVE ||
					 terms->terms[head].tag == PROTOTYPE_TERM_EFFECT_OPERATION)) {
					category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
					break;
				}
			}
			if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
				category = PROTOTYPE_TERM_CATEGORY_VALUE;
			} else if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION) {
				category = PROTOTYPE_TERM_CATEGORY_VALUE;
			} else if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION) {
				/* Pi elimination is shared by computation functions and pure
				 * type-family values. A partial type-family application has a Pi
				 * classifier but remains a value, so its function occurrence owns
				 * the polarity of this APP edge. */
				uint32_t function_occurrence;
				if (judgement_occurrence_child(
						operations, operation_id,
						PROTOTYPE_TERM_CHILD_FUNCTION, 0,
						&function_occurrence
					) != 0) {
					return -1;
				}
				category = operations->occurrences[function_occurrence].category ==
					PROTOTYPE_TERM_CATEGORY_VALUE ?
					PROTOTYPE_TERM_CATEGORY_VALUE :
					PROTOTYPE_TERM_CATEGORY_COMPUTATION;
			} else {
				return -1;
			}
			break;
		case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE:
		case PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE: {
			uint32_t source_operation = operation->wrapped_occurrence;
			if (source_operation >= operations->occurrence_count ||
				judgement_operation_expected_polarity(
					terms,
					type_declarations,
					operations,
					source_operation,
					&category
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_VAR:
			break;
		default:
			return -1;
	}
	*p_category = category;
	return 0;
}

int prototype_judgement_validate_occurrence_typing(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id
) {
	if (!terms || !type_declarations || !contexts || !substitutions || !operations ||
		operation_id >= operations->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[operation_id];
	if (operation->context_id >= contexts->context_count ||
		operation->core_term >= terms->term_count ||
		(operation->classifier != PROTOTYPE_INVALID_ID &&
		operation->classifier >= terms->term_count)) {
		return -1;
	}
	if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
		const struct prototype_substitution* action = prototype_substitution_get(
			substitutions, operation->context_action_substitution
		);
		uint32_t projected_core;
		uint32_t projected_classifier = PROTOTYPE_INVALID_ID;
		if (!action || action->source_context != operation->context_id ||
			operation->source_core_term >= terms->term_count ||
			prototype_term_reindex(
				terms, type_declarations, contexts, substitutions,
				operation->source_core_term,
				operation->context_action_substitution,
				&projected_core
			) != 0 || projected_core != operation->core_term ||
			(operation->source_classifier != PROTOTYPE_INVALID_ID &&
			 (operation->source_classifier >= terms->term_count ||
			  prototype_term_reindex(
				terms, type_declarations, contexts, substitutions,
				operation->source_classifier,
				operation->context_action_substitution,
				&projected_classifier
			  ) != 0))) {
			return -1;
		}
		(void)projected_classifier;
	} else if (operation->source_core_term != PROTOTYPE_INVALID_ID ||
		operation->source_classifier != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	const struct prototype_term* core = &terms->terms[operation->core_term];
	switch (operation->tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_ATOM:
			return 0;
	case PROTOTYPE_TYPED_OCCURRENCE_VAR: {
			if (operation->context_action_substitution != PROTOTYPE_INVALID_ID) {
				const struct prototype_substitution* action =
					prototype_substitution_get(
						substitutions,
						operation->context_action_substitution
					);
				uint32_t projected_core;
				if (!action || action->source_context != operation->context_id ||
					operation->source_core_term >= terms->term_count ||
					terms->terms[operation->source_core_term].tag !=
						PROTOTYPE_TERM_VAR || prototype_term_reindex(
						terms, type_declarations, contexts, substitutions,
						operation->source_core_term,
						operation->context_action_substitution,
						&projected_core
					) != 0 || projected_core != operation->core_term) {
					return -1;
				}
				return 0;
			}
			if (core->tag != PROTOTYPE_TERM_VAR ||
				operation->binding_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			uint32_t binding_context;
			if (prototype_context_find_binding(
					contexts,
					operation->context_id,
					operation->binding_id,
					&binding_context
				) != 0) {
				return -1;
			}
			const struct prototype_context* binding =
				prototype_context_get(contexts, binding_context);
			uint32_t binding_classifier =
				prototype_context_classifier_term(binding);
			if (!binding || binding_classifier == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			int valid = operation->classifier == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					binding_classifier,
					operation->classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
			return valid ? 0 : -1;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE:
			return operation->wrapped_occurrence < operations->occurrence_count &&
				operation_core_projection_equal(
					terms, operations, operation->wrapped_occurrence,
					operation->core_term
				) ? 0 : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR:
		{
			int belongs = core->tag == PROTOTYPE_TERM_CONSTRUCTOR &&
				constructor_belongs_to_owner(
					terms,
					type_declarations,
					core->as.constructor.owner,
					core->as.constructor.constructor_id
				);
			int returns_owner = belongs && classifier_returns_owner(
					terms,
					type_declarations,
					operation->classifier,
					core->as.constructor.owner
				);
			if (!belongs || !returns_owner) {
				uint32_t debug_type_id = PROTOTYPE_INVALID_ID;
				uint32_t debug_args[16];
				uint32_t debug_arg_count = 0;
				int debug_type_status = prototype_term_type_instance_info(
					terms,
					core->as.constructor.owner,
					&debug_type_id,
					debug_args,
					&debug_arg_count
				);
				fprintf(stderr,
					"occurrence constructor validation failed occurrence=%u core-tag=%d owner=%u constructor=%u classifier=%u belongs=%d returns-owner=%d type-status=%d type=%u arguments=%u types=%zu constructors=%zu\n",
					operation_id, core->tag,
					core->tag == PROTOTYPE_TERM_CONSTRUCTOR ?
						core->as.constructor.owner : PROTOTYPE_INVALID_ID,
					core->tag == PROTOTYPE_TERM_CONSTRUCTOR ?
						core->as.constructor.constructor_id : PROTOTYPE_INVALID_ID,
					operation->classifier, belongs, returns_owner,
					debug_type_status, debug_type_id, debug_arg_count,
					type_declarations->semantic_schema.type_count,
					type_declarations->semantic_schema.constructor_count);
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
		{
			uint32_t function_occurrence;
			uint32_t argument_occurrence;
			uint32_t app_core;
			if (prototype_term_core_projection(
					terms, operation->core_term, &app_core
				) != 0 || terms->terms[app_core].tag != PROTOTYPE_TERM_APP) {
				return -1;
			}
			const struct prototype_term* app = &terms->terms[app_core];
			return
				judgement_occurrence_child(
					operations, operation_id, PROTOTYPE_TERM_CHILD_FUNCTION,
					0, &function_occurrence
				) == 0 && judgement_occurrence_child(
					operations, operation_id, PROTOTYPE_TERM_CHILD_ARGUMENT,
					0, &argument_occurrence
				) == 0 &&
				operation_core_projection_equal(
					terms, operations, function_occurrence, app->as.app.function
				) && operation_core_projection_equal(
					terms, operations, argument_occurrence, app->as.app.argument
				) ? 0 : -1;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
		{
			uint32_t body_occurrence;
			if (core->tag != PROTOTYPE_TERM_LAMBDA ||
				judgement_occurrence_child(
					operations, operation_id, PROTOTYPE_TERM_CHILD_BODY, 0,
					&body_occurrence
				) != 0 ||
				operation->binding_id == PROTOTYPE_INVALID_ID ||
				operation->binder_classifier == PROTOTYPE_INVALID_ID ||
				!prototype_context_contains_binding(
					contexts,
					operations->occurrences[body_occurrence].context_id,
					operation->binding_id
				)) {
				return -1;
			}
			int body_equal = 0;
			return prototype_term_core_shape_equal_under_binder(
					terms,
					core->as.lambda.binding_id,
					core->as.lambda.body,
					operation->binding_id,
					operations->occurrences[body_occurrence].core_term,
					&body_equal
				) == 0 && body_equal ? 0 : -1;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
		{
			uint32_t scrutinee_occurrence;
			if (core->tag != PROTOTYPE_TERM_MATCH ||
				judgement_occurrence_child(
					operations, operation_id, PROTOTYPE_TERM_CHILD_SCRUTINEE,
					0, &scrutinee_occurrence
				) != 0 ||
				!operation_core_projection_equal(
					terms, operations, scrutinee_occurrence,
					core->as.match.scrutinee
				) ||
				operation->case_count != core->as.match.case_count ||
				operation->first_case > operations->case_count ||
				operation->case_count > operations->case_count -
					operation->first_case ||
				core->as.match.first_case > terms->case_count ||
				core->as.match.case_count > terms->case_count -
					core->as.match.first_case) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				const struct prototype_typed_occurrence_match_case* typed_case =
					&operations->cases[operation->first_case + i];
				const struct prototype_match_case* erased_case =
					&terms->cases[core->as.match.first_case + i];
				uint32_t body_occurrence;
				int body_projection_equal = judgement_occurrence_child(
						operations, operation_id,
						PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i,
						&body_occurrence
					) == 0 &&
					operation_match_case_body_projection_equal(
						terms,
						type_declarations,
						contexts,
						substitutions,
						operations,
						operation_id,
						i,
						typed_case,
						erased_case,
						core->as.match.ih_scope_id
					);
				if (body_occurrence >= operations->occurrence_count ||
					typed_case->context_id >= contexts->context_count ||
					!body_projection_equal ||
					!term_core_projection_equal(
						terms, typed_case->constructor_owner,
						erased_case->constructor_owner
					) ||
					typed_case->constructor_id != erased_case->constructor_id ||
					typed_case->binder_count != erased_case->binder_count) {
					fprintf(stderr,
						"occurrence Match case validation failed occurrence=%u case=%u "
						"body=%u context=%u projection=%d refinement=%d substitution=%u "
						"constructor=%u/%u binders=%u/%u\n",
						operation_id, i, body_occurrence,
						typed_case->context_id, body_projection_equal,
						prototype_typed_occurrence_match_case_is_solved(typed_case),
						typed_case->refinement_substitution,
						typed_case->constructor_id, erased_case->constructor_id,
						typed_case->binder_count, erased_case->binder_count);
					return -1;
				}
			}
			return 0;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
		{
			struct prototype_typed_occurrence_induction_edge edge;
			return prototype_typed_occurrence_graph_induction_edge(
				operations, terms, operation_id, &edge
			);
		}
		case PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE:
			return operation->wrapped_occurrence < operations->occurrence_count &&
				operation_core_projection_equal(
					terms, operations, operation->wrapped_occurrence,
					operation->core_term
				) ? 0 : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		{
			uint32_t value_occurrence;
			return core->tag == PROTOTYPE_TERM_RETURN &&
				judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0,
					&value_occurrence
				) == 0 &&
				operation_core_projection_equal(
					terms, operations, value_occurrence,
					core->as.return_term.value
				) ? 0 : -1;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
		{
			uint32_t computation_occurrence;
			return core->tag == PROTOTYPE_TERM_THUNK &&
				judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0,
					&computation_occurrence
				) == 0 &&
				operation_core_projection_equal(
					terms, operations, computation_occurrence,
					core->as.thunk.computation
				) ? 0 : -1;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
		{
			uint32_t value_occurrence;
			return core->tag == PROTOTYPE_TERM_FORCE &&
				judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0,
					&value_occurrence
				) == 0 &&
				operation_core_projection_equal(
					terms, operations, value_occurrence, core->as.force.value
				) ? 0 : -1;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
		{
			uint32_t request_operation;
			uint32_t request_argument;
			uint32_t request_continuation;
			return core->tag == PROTOTYPE_TERM_OPERATION_REQUEST &&
				judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
					&request_operation
				) == 0 && judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0,
					&request_argument
				) == 0 && judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
					&request_continuation
				) == 0 &&
				operation_core_projection_equal(
					terms, operations, request_operation,
					core->as.operation_request.operation
				) && operation_core_projection_equal(
					terms, operations, request_argument,
					core->as.operation_request.argument
				) && operation_core_projection_equal(
					terms, operations, request_continuation,
					core->as.operation_request.continuation
				) ? 0 : -1;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
		{
			uint32_t computation_occurrence;
			uint32_t return_occurrence;
			if (core->tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
				judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					&computation_occurrence
				) != 0 ||
				!operation_core_projection_equal(
					terms, operations, computation_occurrence,
					core->as.computation_fold.computation
				) ||
				operation->fold_clause_count !=
					core->as.computation_fold.clause_count ||
				(operation->fold_clause_count != 0 &&
				 (operation->first_fold_clause > operations->fold_clause_count ||
				  operation->fold_clause_count > operations->fold_clause_count -
					operation->first_fold_clause))) {
				return -1;
			}
			if (judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&return_occurrence
				) != 0 ||
				!operation_core_projection_equal(
					terms, operations, return_occurrence,
					core->as.computation_fold.return_clause
				)) {
				return -1;
			}
			return 0;
		}
		default:
			return -1;
	}
}

int prototype_judgement_project_principal_occurrence_proposition(
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	struct prototype_judgement_proposition* p_proposition
) {
	if (!operations || !p_proposition ||
		!operations->sealed ||
		operation_id >= operations->occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[operation_id];
	if (operation->classifier_status !=
			PROTOTYPE_TYPED_OCCURRENCE_CLASSIFIER_SOLVED ||
		operation->context_id == PROTOTYPE_INVALID_ID ||
		operation->core_term == PROTOTYPE_INVALID_ID ||
		operation->classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_proposition = (struct prototype_judgement_proposition){
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE,
		.authority_id = operation_id,
		.context_id = operation->context_id,
		.occurrence_id = operation_id,
		.subject = operation->core_term,
		.classifier = operation->classifier,
		.hash_next = PROTOTYPE_INVALID_ID
	};
	return 0;
}

static int judgement_principal_rule_matches_operation(
	const struct prototype_typed_occurrence* operation,
	int proof_kind
) {
	if (!operation) {
		return 0;
	}
	switch (operation->tag) {
	case PROTOTYPE_TYPED_OCCURRENCE_ATOM:
		return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION;
	case PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR:
		return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO;
	case PROTOTYPE_TYPED_OCCURRENCE_APP:
		if (operation->application_role ==
			PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION) {
			return 0;
		}
		return operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION ?
			proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION :
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM;
	case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
		return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO;
	case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
		return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM ||
			proof_kind == PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE;
	case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
		return proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM;
	case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO;
	case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
		return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO;
	case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
		return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM;
	case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
		return proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO;
	case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
		return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM;
	case PROTOTYPE_TYPED_OCCURRENCE_VAR:
	case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE:
	case PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE:
	default:
		return 0;
	}
}

static int judgement_principal_payload_matches_operation(
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	const struct prototype_judgement_rule_application_view* proof
) {
	if (!terms || !operations || !proof || operation_id >= operations->occurrence_count) {
		return 0;
	}
	if (proof->proof_kind !=
		PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
		return 1;
	}
	struct prototype_typed_occurrence_induction_edge edge;
	if (prototype_typed_occurrence_graph_induction_edge(
			operations, terms, operation_id, &edge
		) != 0) {
		return 0;
	}
	return proof->rule_data.induction.match ==
			operations->occurrences[edge.owner_match_occurrence].core_term &&
		proof->rule_data.induction.case_index == edge.case_index &&
		proof->rule_data.induction.field_index == edge.field_index;
}

int prototype_judgement_audit_principal_occurrence_claims(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement,
	struct prototype_judgement_principal_occurrence_audit* p_audit
) {
	if (!terms || !operations || !judgement || !p_audit ||
		operations->occurrence_count >
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY) {
		return -1;
	}
	struct prototype_occurrence_usage_solution usage_solutions[
		PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY
	];
	struct prototype_usage_entry usage_entries[
		PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY
	];
	size_t usage_entry_count;
	if (prototype_occurrence_usage_solve(
			operations,
			terms,
			contexts,
			usage_solutions,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_SOLUTION_CAPACITY,
			usage_entries,
			PROTOTYPE_TYPED_OCCURRENCE_USAGE_ENTRY_CAPACITY,
			&usage_entry_count
		) != 0) {
		fprintf(stderr, "principal occurrence audit: usage solve failed\n");
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < operations->occurrence_count;
		++operation_id) {
		const struct prototype_typed_occurrence* fold =
			&operations->occurrences[operation_id];
		if (fold->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD) {
			continue;
		}
		if (fold->fold_clause_count == 0) {
			continue;
		}
		if (fold->first_fold_clause > operations->fold_clause_count ||
			fold->fold_clause_count > operations->fold_clause_count -
				fold->first_fold_clause) {
			return -1;
		}
		for (uint32_t clause_index = 0;
			clause_index < fold->fold_clause_count;
			++clause_index) {
			const struct prototype_typed_occurrence_fold_clause* clause =
				&operations->fold_clauses[
					fold->first_fold_clause + clause_index
				];
			uint32_t operation_occurrence;
			uint32_t clause_occurrence;
			uint32_t continuation_occurrence;
			if (judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION, clause_index,
					&operation_occurrence
				) != 0 || judgement_occurrence_child(
					operations, operation_id,
					PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY, clause_index,
					&clause_occurrence
				) != 0 || judgement_occurrence_child(
					operations, clause_occurrence, PROTOTYPE_TERM_CHILD_BODY, 0,
					&continuation_occurrence
				) != 0) {
				return -1;
			}
			int operation_identity;
			if (prototype_term_effect_operation_identity(
					terms,
					operations->occurrences[operation_occurrence].core_term,
					&operation_identity
				) != 0) {
				return -1;
			}
			const struct prototype_effect_operation_declaration* declaration =
				prototype_term_effect_operation_declaration(operation_identity);
			const struct prototype_typed_occurrence* outer =
				&operations->occurrences[clause_occurrence];
			if (!declaration || outer->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
				return -1;
			}
			const struct prototype_typed_occurrence* continuation =
				&operations->occurrences[continuation_occurrence];
			if (continuation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
				continuation->binding_id != clause->continuation_binder_id ||
				(declaration->resumption_multiplicity ==
					PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE &&
				 usage_solutions[continuation_occurrence].binder_usage !=
					PROTOTYPE_USAGE_ZERO) ||
				(declaration->resumption_multiplicity ==
					PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT &&
				 usage_solutions[continuation_occurrence].binder_usage ==
					PROTOTYPE_USAGE_MANY)) {
				return -1;
			}
		}
	}
	memset(p_audit, 0, sizeof(*p_audit));
	p_audit->total_proposition_count = judgement->proposition_count;
	p_audit->total_accepted_claim_count = judgement->claim_count;
	p_audit->claim_wrapper_bytes =
		judgement->claim_count * sizeof(*judgement->claims);
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->closure_rank == PROTOTYPE_INVALID_ID ||
			derivation->conclusion_claim_id >= judgement->claim_count) {
			continue;
		}
		const struct prototype_judgement_proposition* conclusion =
			prototype_judgement_claim_proposition(
				judgement, derivation->conclusion_claim_id
			);
		int role = prototype_judgement_proof_reconstruction_role(
			derivation->proof_kind, conclusion
		);
		if (role <= PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_INVALID ||
			role > PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_NON_OPERATION) {
			return -1;
		}
		p_audit->proof_role_counts[role]++;
	}
	for (uint32_t claim_id = 0;
		claim_id < (uint32_t)judgement->claim_count;
		++claim_id) {
		const struct prototype_judgement_claim* accepted =
			prototype_judgement_claim_get(judgement, claim_id);
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, claim_id);
		if (!accepted || !proposition ||
			accepted->closure_rank == PROTOTYPE_INVALID_ID ||
			proposition->authority_kind !=
				PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE) {
			continue;
		}
		uint32_t operation_id = proposition->occurrence_id;
		struct prototype_judgement_proposition projected;
		int projection_status =
			prototype_judgement_project_principal_occurrence_proposition(
				operations, operation_id, &projected
			);
		if (projection_status > 0) {
			continue;
		}
		if (projection_status < 0) {
			fprintf(stderr,
				"principal occurrence audit: projection failed claim=%u occurrence=%u\n",
				claim_id, operation_id);
			return -1;
		}
		struct prototype_usage_vector projected_usage;
		if (prototype_occurrence_usage_solution_view(
				usage_solutions,
				operations->occurrence_count,
				usage_entries,
				usage_entry_count,
				operation_id,
				&projected_usage
			) != 0) {
			fprintf(stderr,
				"principal occurrence audit: usage view failed claim=%u occurrence=%u\n",
				claim_id, operation_id);
			return -1;
		}
		projected.resource_usage_count = (uint32_t)projected_usage.count;
		projected.resource_usage = projected_usage.entries;
		size_t derivation_count = 0;
		for (size_t i = 0; i < judgement->derivation_count; ++i) {
			const struct prototype_judgement_derivation* derivation =
				&judgement->derivations[i];
			if (derivation->conclusion_claim_id != claim_id ||
				derivation->closure_rank == PROTOTYPE_INVALID_ID) {
				continue;
			}
			int reconstruction_role =
				prototype_judgement_proof_reconstruction_role(
					derivation->proof_kind, proposition
				);
			if (reconstruction_role ==
				PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_INVALID) {
				return -1;
			}
			if (reconstruction_role ==
					PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_PRINCIPAL) {
				struct prototype_judgement_rule_application_view view;
				if (!judgement_principal_rule_matches_operation(
						&operations->occurrences[operation_id],
						derivation->proof_kind
					) || accepted_derivation_rule_application_view(
						judgement,
						derivation,
						&view
					) != 0 || !judgement_principal_payload_matches_operation(
						terms, operations, operation_id, &view
					) || !judgement_proposition_equal(
						view.conclusion, &projected
					)) {
					fprintf(
						stderr,
						"principal occurrence audit: replay mismatch claim=%u "
						"occurrence=%u rule=%d stored_usage=%u projected_usage=%u\n",
						claim_id,
						operation_id,
						derivation->proof_kind,
						proposition->resource_usage_count,
						projected.resource_usage_count
					);
					fprintf(
						stderr,
						"principal occurrence audit: operation tag=%d context=%u "
						"core=%u classifier=%u first-edge=%u edge-count=%u\n",
						operations->occurrences[operation_id].tag,
						operations->occurrences[operation_id].context_id,
						operations->occurrences[operation_id].core_term,
						operations->occurrences[operation_id].classifier,
						operations->occurrences[operation_id].first_edge,
						operations->occurrences[operation_id].edge_count
					);
					return -1;
				}
				for (uint32_t premise_index = 0;
					premise_index < view.premise_count;
					++premise_index) {
					uint32_t expected_operation_id = PROTOTYPE_INVALID_ID;
					int expected_status = judgement_expected_rule_view_premise_operation(
						operations,
						&view,
						premise_index,
						&expected_operation_id
					);
					if (expected_status < 0 ||
						(expected_status == 0 &&
						 view.premises[premise_index].proposition->authority_kind ==
							PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
						 !judgement_premise_operation_matches(
							operations, expected_operation_id,
							view.premises[premise_index].proposition
						))) {
						fprintf(stderr,
							"principal occurrence audit: premise operation mismatch "
							"claim=%u occurrence=%u rule=%d premise=%u status=%d "
							"expected=%u authority=%d:%u actual=%u\n",
							claim_id, operation_id, derivation->proof_kind,
							premise_index, expected_status, expected_operation_id,
							view.premises[premise_index].proposition->authority_kind,
							view.premises[premise_index].proposition->authority_id,
							view.premises[premise_index].proposition->occurrence_id);
						return -1;
					}
				}
				derivation_count++;
			}
		}
		if (derivation_count == 0) {
			continue;
		}
		p_audit->principal_occurrence_count++;
		uint32_t proposition_id;
		if (prototype_judgement_proposition_find_exact(
				judgement, &projected, &proposition_id
			) != 0) {
			fprintf(
				stderr,
				"principal occurrence audit: projected proposition missing "
				"claim=%u occurrence=%u stored_usage=%u projected_usage=%u\n",
				claim_id,
				operation_id,
				proposition->resource_usage_count,
				projected.resource_usage_count
			);
			return -1;
		}
		p_audit->proposition_count++;
		struct prototype_judgement_claim claim_identity = {
			.proposition_id = proposition_id
		};
		uint32_t projected_claim_id;
		if (prototype_judgement_find_exact_claim(
				judgement, &claim_identity, &projected_claim_id
			) != 0 || projected_claim_id != claim_id) {
			return -1;
		}
		p_audit->accepted_claim_count++;
		p_audit->derivation_count += derivation_count;
	}
	return 0;
}

int prototype_judgement_validate_accepted_graph(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_typed_occurrence_graph* operations,
	struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !intrinsic_environment || !contexts ||
		!substitutions || !dimension_operators ||
		!judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			&judgement->propositions[i];
		if (!prototype_context_get(contexts, proposition->context_id) ||
			proposition->subject >= terms->term_count ||
			proposition->classifier >= terms->term_count ||
			(proposition->occurrence_id != PROTOTYPE_INVALID_ID &&
			 (!operations || proposition->occurrence_id >= operations->occurrence_count))) {
			fprintf(
				stderr,
				"P0 proposition reference failed proposition=%u context=%u "
				"occurrence=%u subject=%u classifier=%u\n",
				i,
				proposition->context_id,
				proposition->occurrence_id,
				proposition->subject,
				proposition->classifier
			);
			return -1;
		}
		if (proposition->resource_usage_count != 0 &&
			!proposition->resource_usage) {
			return -1;
		}
		for (uint32_t usage_index = 0;
			usage_index < proposition->resource_usage_count;
			++usage_index) {
			const struct prototype_usage_entry* entry =
				&proposition->resource_usage[usage_index];
			if (entry->grade <= PROTOTYPE_USAGE_ZERO ||
				entry->grade > PROTOTYPE_USAGE_MANY ||
				(usage_index != 0 &&
				 proposition->resource_usage[usage_index - 1].binding_id >=
					entry->binding_id) ||
				!prototype_context_contains_binding(
					contexts, proposition->context_id, entry->binding_id
				)) {
				fprintf(
					stderr,
					"P0 proposition resource usage failed proposition=%u "
					"context=%u occurrence=%u subject=%u classifier=%u "
					"binding=%u grade=%d\n",
					i,
					proposition->context_id,
					proposition->occurrence_id,
					proposition->subject,
					proposition->classifier,
					entry->binding_id,
					entry->grade
				);
				if (operations && proposition->occurrence_id <
					operations->occurrence_count) {
					const struct prototype_typed_occurrence* failed_occurrence =
						&operations->occurrences[proposition->occurrence_id];
					fprintf(
						stderr,
						"P0 resource occurrence tag=%d core=%u edges=%u+%u "
						"clauses=%u\n",
						failed_occurrence->tag,
						failed_occurrence->core_term,
						failed_occurrence->first_edge,
						failed_occurrence->edge_count,
						failed_occurrence->fold_clause_count
					);
				}
				return -1;
			}
		}
	}
	if (prototype_judgement_recompute_closure_ranks(operations, judgement) != 0) {
		fprintf(stderr, "P0 accepted rank computation failed\n");
		return -1;
	}
	if (prototype_judgement_db_rebuild_index(judgement) != 0) {
		fprintf(stderr, "P0 accepted index rebuild failed\n");
		return -1;
	}
	if (validate_proof_relation_coverage(judgement) != 0) {
		fprintf(stderr, "P0 coverage failed\n");
		return -1;
	}
	if (operations) {
		for (uint32_t claim_id = 0;
			claim_id < (uint32_t)judgement->claim_count; ++claim_id) {
			if (judgement->claims[claim_id].closure_rank == PROTOTYPE_INVALID_ID) {
				continue;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(judgement, claim_id);
			if (!proposition ||
				proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				proposition->authority_kind !=
					PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE) {
				continue;
			}
			if (proposition->occurrence_id >= operations->occurrence_count ||
				proposition->authority_id != proposition->occurrence_id) {
				fprintf(
					stderr,
					"P0 occurrence authority failed claim=%u occurrence=%u "
					"authority=%u occurrence_count=%zu\n",
					claim_id,
					proposition->occurrence_id,
					proposition->authority_id,
					operations->occurrence_count
				);
				return -1;
			}
			int expected_polarity;
			int polarity_status = judgement_operation_expected_polarity(
					terms,
					type_declarations,
					operations,
					proposition->occurrence_id,
					&expected_polarity
				);
			if (polarity_status != 0 ||
				operations->occurrences[proposition->occurrence_id].category !=
					expected_polarity) {
				const struct prototype_typed_occurrence* failed =
					&operations->occurrences[proposition->occurrence_id];
				fprintf(
					stderr,
					"P1-R0 accepted occurrence category failed claim=%u occurrence=%u "
					"tag=%d context=%u core=%u core-tag=%d classifier=%u "
					"classifier-tag=%d actual=%d expected=%d status=%d\n",
					claim_id,
					proposition->occurrence_id,
					failed->tag,
					failed->context_id,
					failed->core_term,
					failed->core_term < terms->term_count ?
						terms->terms[failed->core_term].tag : -1,
					failed->classifier,
					failed->classifier < terms->term_count ?
						terms->terms[failed->classifier].tag : -1,
					failed->category,
					polarity_status == 0 ? expected_polarity : -1,
					polarity_status
				);
				return -1;
			}
		}
		for (uint32_t operation_id = 0;
			operation_id < operations->occurrence_count;
			++operation_id) {
			if (prototype_judgement_validate_occurrence_typing(
					terms,
					type_declarations,
					contexts,
					substitutions,
					operations,
					operation_id
				) != 0) {
				const struct prototype_typed_occurrence* failed =
					&operations->occurrences[operation_id];
				fprintf(
					stderr,
					"P0 occurrence validation failed occurrence=%u tag=%d context=%u "
					"core=%u classifier=%u binding=%u edges=%u+%u action=%u "
					"source-core=%u source-classifier=%u\n",
					operation_id,
					failed->tag,
					failed->context_id,
					failed->core_term,
					failed->classifier,
					failed->binding_id,
					failed->first_edge,
					failed->edge_count,
					failed->context_action_substitution,
					failed->source_core_term,
					failed->source_classifier
				);
				return -1;
			}
		}
		struct prototype_judgement_principal_occurrence_audit principal_audit;
		if (prototype_judgement_audit_principal_occurrence_claims(
				terms, contexts, operations, judgement, &principal_audit
			) != 0) {
			fprintf(stderr, "P0 principal occurrence reconstruction audit failed\n");
			return -1;
		}
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID ||
			derivation->closure_rank == PROTOTYPE_INVALID_ID) {
			continue;
		}
		struct prototype_judgement_rule_application_view view;
		if (accepted_derivation_rule_application_view(
				judgement,
				derivation,
				&view
			) != 0) {
			fprintf(
				stderr,
				"P0 accepted derivation replay failed derivation=%zu rule=%d "
				"claim=%u premises=%u\n",
				i,
				derivation->proof_kind,
				derivation->conclusion_claim_id,
				derivation->premise_count
			);
			return -1;
		}
		const struct prototype_judgement_proposition* relation =
			view.conclusion;
		const struct prototype_judgement_rule_application_view* proof =
			&view;
			int rule_parameter_status = validate_proof_rule_parameters(proof);
			int operation_owner_status = validate_operation_owned_relation(
				operations, contexts, substitutions, relation, proof
			);
			if (proof->conclusion != relation ||
				proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
				rule_parameter_status != 0 || operation_owner_status != 0) {
				fprintf(
					stderr,
					"P0 accepted header failed derivation=%zu claim=%u occurrence=%u "
					"proof_occurrence=%u context=%u proof_context=%u "
					"rule_params=%d operation_owner=%d classifier=%u\n",
					i,
					derivation->conclusion_claim_id,
					relation->occurrence_id,
					relation->occurrence_id,
					relation->context_id,
					relation->context_id,
					rule_parameter_status,
					operation_owner_status,
					relation->classifier
				);
				return -1;
			}
			if (operations) {
				for (uint32_t premise_index = 0;
					premise_index < proof->premise_count;
					++premise_index) {
					uint32_t expected_operation_id = PROTOTYPE_INVALID_ID;
					int expected_status = judgement_expected_rule_view_premise_operation(
						operations,
						proof,
						premise_index,
						&expected_operation_id
					);
					if (expected_status < 0 ||
						(expected_status == 0 &&
						 proof->premises[premise_index].proposition->authority_kind ==
							PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
						 !judgement_premise_operation_matches(
							operations, expected_operation_id,
							proof->premises[premise_index].proposition
						))) {
						fprintf(
							stderr,
							"P0 accepted premise owner failed derivation=%zu "
							"rule=%d premise=%u expected=%u actual=%u\n",
							i,
							proof->proof_kind,
							premise_index,
							expected_operation_id,
							proof->premises[premise_index].proposition->occurrence_id
						);
						return -1;
					}
				}
			}
			uint32_t free_effect_row_binder;
			int has_free_effect_row =
				judgement_classifier_free_result_effect_row_binder(
					terms,
					type_declarations,
					relation->classifier,
					&free_effect_row_binder
				);
			int has_effect_row_assumption = 0;
			if (has_free_effect_row > 0) {
				has_effect_row_assumption =
					proof->proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION ?
						1 : judgement_derivation_has_effect_row_assumption(
							judgement,
							terms,
							derivation,
							free_effect_row_binder
						);
				if (has_effect_row_assumption == 0) {
					has_effect_row_assumption =
						judgement_operation_owns_effect_row_binder(
							terms,
							operations,
							relation->occurrence_id,
							free_effect_row_binder
						);
				}
			}
			if (has_effect_row_assumption < 0 ||
				has_free_effect_row < 0 ||
				(has_free_effect_row != 0 && has_effect_row_assumption == 0)) {
				fprintf(
					stderr,
					"P0 effect-row assumption failed derivation=%zu rule=%d free=%d "
					"assumption=%d binder=%u occurrence=%u subject=%u classifier=%u "
					"premises=%u\n",
					i,
					proof->proof_kind,
					has_free_effect_row,
					has_effect_row_assumption,
					free_effect_row_binder,
					relation->occurrence_id,
					relation->subject,
					relation->classifier,
					proof->premise_count
				);
				return -1;
			}
			if (getenv("A_PROGRAM_ACCEPTED_REPLAY_TRACE")) {
				fprintf(stderr,
					"accepted replay derivation=%zu proof=%d claim=%u occurrence=%u "
					"subject=%u classifier=%u premises=%u\n",
					i, proof->proof_kind, derivation->conclusion_claim_id,
					relation->occurrence_id, relation->subject,
					relation->classifier, proof->premise_count);
			}
			switch (proof->proof_kind) {
			case PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO:
				if (validate_type_formation_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
				) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO:
				if (validate_constructor_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION:
				if (validate_constructor_spine_formation_proof(
						terms,
						type_declarations,
						contexts,
						substitutions,
						operations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
					if (validate_assumption_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							operations,
							judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
				if (proof->premise_count != 2) {
					return -1;
				}
					if (validate_lambda_intro_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							operations,
							relation,
							proof
						) != 0) {
						return -1;
					}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
				if (proof->premise_count != 2) {
					return -1;
				}
					if (validate_app_elim_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
					if (validate_return_intro_proof(
							terms, type_declarations, relation, proof
						) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
				if (validate_thunk_intro_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
				if (validate_force_elim_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
				if (validate_computation_fold_elim_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
					if (validate_operation_request_intro_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							relation,
							proof
						) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
				if (validate_match_elim_proof(
						terms,
						type_declarations,
						contexts,
						substitutions,
						dimension_operators,
						judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
					if (validate_solved_match_motive_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							operations,
							judgement,
							relation,
							proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM:
					if (validate_induction_hypothesis_elim_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							operations,
							judgement,
						relation,
						proof
				) != 0) {
					fprintf(stderr,
						"P0 IH replay failed derivation=%zu claim=%u occurrence=%u "
						"subject=%u classifier=%u premises=%u\n",
						i, derivation->conclusion_claim_id,
						relation->occurrence_id, relation->subject,
						relation->classifier, proof->premise_count);
					return -1;
				}
				if (operations && relation->occurrence_id != PROTOTYPE_INVALID_ID &&
					!judgement_principal_payload_matches_operation(
						terms, operations, relation->occurrence_id, proof
					)) {
					fprintf(stderr,
						"P0 IH principal payload failed derivation=%zu claim=%u "
						"occurrence=%u\n",
						i, derivation->conclusion_claim_id,
						relation->occurrence_id);
					return -1;
				}
				break;
				case PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO:
					if (validate_text_literal_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
		case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO:
					if (validate_int_literal_intro_proof(
							terms, intrinsic_environment, relation, proof
						) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY:
					if (validate_int_literal_admissibility_proof(
							terms, relation, proof
						) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO:
					if (validate_pure_primitive_type_intro_proof(
						terms,
						type_declarations,
						contexts,
						relation,
						proof
					) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO:
					if (validate_effect_operation_type_intro_proof(
						terms, relation, proof
					) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO:
					if (validate_text_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO:
					if (validate_int_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO:
					if (validate_host_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO:
				if (validate_match_type_formation_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
				if (validate_conversion_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE:
				if (validate_expected_type_exposure_proof(
						terms,
						type_declarations,
						contexts,
						substitutions,
						operations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_DECLARATION:
				if (validate_declaration_proof(
						terms,
						type_declarations,
						judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE:
				if (validate_is_type_from_has_type_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY:
				if (validate_universe_cumulativity_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO:
				if (validate_pi_formation_intro_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION:
				if (validate_computation_type_formation_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION:
				if (validate_thunk_type_formation_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TYPE_FORMATION:
				if (validate_dimension_action_type_formation_proof(
						terms,
						type_declarations,
						contexts,
						dimension_operators,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TERM:
				if (validate_dimension_action_term_proof(
						terms, dimension_operators, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_CONSTRUCTOR:
				if (validate_dimension_action_constructor_proof(
						terms,
						type_declarations,
						contexts,
						dimension_operators,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_THUNK_RETURN_WITNESS:
				if (validate_dimension_action_thunk_return_witness_proof(
						terms,
						type_declarations,
						dimension_operators,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN:
				if (validate_context_weaken_proof(
						contexts, substitutions, relation, proof
					) != 0) {
					fprintf(
						stderr,
						"P0 context-weaken replay failed derivation=%zu context=%u "
						"classifier=%u premise_context=%u premise_classifier=%u "
						"action=%d:%u\n",
						i,
						relation->context_id,
						relation->classifier,
						proof->premise_count ?
							proof->premises[0].proposition->context_id : PROTOTYPE_INVALID_ID,
						proof->premise_count ?
							proof->premises[0].proposition->classifier : PROTOTYPE_INVALID_ID,
						proof->semantic_action_kind,
						proof->semantic_action_id
					);
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX:
				if (validate_substitution_reindex_proof(
						terms,
						type_declarations,
						contexts,
						substitutions,
						relation,
						proof
					) != 0) {
					fprintf(
						stderr,
						"P0 substitution-reindex replay failed derivation=%zu claim=%u\n",
						i,
						derivation->conclusion_claim_id
					);
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
				if (validate_effect_weaken_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION:
				if (validate_relation_type_formation_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_WITNESS_INTRO:
				if (validate_relation_witness_intro_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TYPE_FORMATION:
				if (validate_terminates_type_formation_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TOTAL_COMPUTATION:
				if (validate_terminates_total_computation_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS:
				if (validate_relation_constructor_witness_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS:
				if (validate_relation_unary_witness_proof(
						terms, relation, proof, PROTOTYPE_TERM_RETURN
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS:
				if (validate_relation_unary_witness_proof(
						terms, relation, proof, PROTOTYPE_TERM_THUNK
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS:
				if (validate_relation_app_witness_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS:
				if (validate_relation_lambda_witness_proof(
						terms, contexts, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS:
				if (validate_relation_match_witness_proof(
						terms, type_declarations, contexts, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS:
				if (validate_relation_induction_hypothesis_witness_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_INVALID:
				return -1;
			default:
				return -1;
		}
	}
	return 0;
}

int prototype_judgement_add_is_type(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t universe
) {
	if (!term_exists(terms, subject) || !term_is_universe_var(terms, universe)) {
		return -1;
	}
	uint32_t premise_subject = subject;
	uint32_t premise_classifier = universe;
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		subject,
		universe,
		PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE,
		NULL,
		&premise_subject,
		&premise_classifier,
		1
	);
}

int prototype_judgement_add_is_type_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe,
	uint32_t has_type_claim_id,
	uint32_t* p_claim_id
) {
	if (!judgement || !p_claim_id || !term_exists(terms, subject) ||
		!term_is_universe_var(terms, universe)) {
		return -1;
	}
	const struct prototype_judgement_proposition* premise =
		prototype_judgement_claim_proposition(judgement, has_type_claim_id);
	if (!premise ||
		premise->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		premise->context_id != context_id || premise->subject != subject ||
		premise->classifier != universe) {
		return -1;
	}
	struct prototype_judgement_proposition proposition = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = universe
	};
	struct prototype_judgement_derivation_candidate derivation;
	struct prototype_judgement_candidate_premise
		premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_proposition
		premise_propositions[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE;
	initialize_proof_rule_parameters(
		&derivation, premises, premise_propositions
	);
	derivation.premise_count = 1;
	premises[0].proposition_id =
		judgement->claims[has_type_claim_id].proposition_id;
	premises[0].proposition_store_kind =
		PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB;
	premises[0].proposition =
		premise;
	premises[0].builder_proposition = NULL;
	return publish_complete_relation(
		judgement,
		&proposition,
		&derivation,
		&has_type_claim_id,
		p_claim_id
	);
}

int prototype_judgement_lookup_authority_neutral_core_classifier(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_classifier
) {
	return lookup_classifier(judgement, NULL, subject, p_classifier);
}

static const char* judgement_kind_name(int kind) {
	switch (kind) {
		case PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE:
			return "has-type";
		case PROTOTYPE_JUDGEMENT_KIND_IS_TYPE:
			return "is-type";
		default:
			return "unknown";
	}
}

static const char* proof_kind_name(int proof_kind) {
	switch (proof_kind) {
		case PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO:
			return "type-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO:
			return "constructor-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION:
			return "constructor-spine-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
			return "binder-assumption";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
			return "match-pattern-assumption";
		case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
			return "lambda-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
			return "app-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
			return "return-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
			return "thunk-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
			return "force-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
			return "computation-fold-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
			return "operation-request-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO:
			return "match-type-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
			return "match-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
			return "solved-match-motive";
		case PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM:
			return "ih-elim";
			case PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO:
				return "text-literal-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO:
			return "int-literal-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY:
			return "int-literal-admissibility";
			case PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO:
				return "pure-primitive-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO:
				return "effect-operation-type-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
			return "conversion";
		case PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE:
			return "expected-type-exposure";
			case PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO:
				return "text-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO:
				return "int-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO:
				return "host-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE:
				return "is-type-from-has-type";
		case PROTOTYPE_JUDGEMENT_PROOF_DECLARATION:
			return "declaration";
		case PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY:
			return "universe-cumulativity";
		case PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO:
			return "pi-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION:
			return "computation-type-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION:
			return "thunk-type-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN:
			return "context-weaken";
		case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
			return "effect-weaken";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION:
			return "relation-type-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_WITNESS_INTRO:
			return "relation-witness-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX:
			return "substitution-reindex";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS:
			return "relation-constructor-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS:
			return "relation-return-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS:
			return "relation-thunk-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS:
			return "relation-app-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS:
			return "relation-lambda-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS:
			return "relation-match-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS:
			return "relation-induction-hypothesis-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TYPE_FORMATION:
			return "dimension-action-type-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TERM:
			return "dimension-action-term";
		case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_CONSTRUCTOR:
			return "dimension-action-constructor";
		case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_THUNK_RETURN_WITNESS:
			return "dimension-action-thunk-return-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TYPE_FORMATION:
			return "terminates-type-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TOTAL_COMPUTATION:
			return "terminates-total-computation";
		default:
			return "invalid";
	}
}

void prototype_judgement_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement
) {
	if (!output || !symbols || !intrinsic_environment || !type_declarations ||
		!terms || !judgement) {
		return;
	}

	fprintf(output, "judgements=%zu\n", judgement->proposition_count);
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		fprintf(output, "%s ", judgement_kind_name(relation->kind));
		prototype_term_print_debug(
			output, symbols, intrinsic_environment, type_declarations,
			terms, relation->subject
		);
		fprintf(output, " ");
		prototype_term_print_debug(
			output, symbols, intrinsic_environment, type_declarations,
			terms, relation->classifier
		);
		uint32_t cursor = 0;
		uint32_t derivation_id;
		while (prototype_judgement_candidate_derivation_next(
			judgement->propositions,
			judgement->proposition_count,
			judgement->derivation_candidates,
			judgement->derivation_candidate_count,
			(uint32_t)i,
			&cursor,
			&derivation_id
		) == 0) {
			const struct prototype_judgement_derivation_candidate* proof =
				&judgement->derivation_candidates[derivation_id];
			fprintf(
				output,
				" [%s proof#%u premises=%u]",
				proof_kind_name(proof->proof_kind),
				derivation_id,
				proof->premise_count
			);
		}
		fprintf(output, "\n");
	}
}
