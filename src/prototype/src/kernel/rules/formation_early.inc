int prototype_judgement_delta_commit(
	struct prototype_judgement_delta* delta,
	size_t mark
) {
	if (!delta || !delta->db || mark != 0) {
		return -1;
	}
	size_t proof_mark = delta->db->derivation_candidate_count;
	size_t premise_mark = delta->db->candidate_premise_count;
	for (size_t i = 0; i < delta->derivation_candidate_count; ++i) {
		const struct prototype_judgement_derivation_candidate* proof =
			&delta->derivation_candidates[i];
		if (proof->conclusion_proposition_id >=
			delta->proposition_count) {
			goto rollback;
		}
		struct prototype_judgement_proposition relation =
			delta->propositions[proof->conclusion_proposition_id];
		if (add_complete_candidate(delta->db, &relation, proof) != 0) {
			goto rollback;
		}
	}
	delta->proposition_count = 0;
	delta->derivation_candidate_count = 0;
	delta->candidate_premise_count = 0;
	return 0;

rollback:
	delta->db->derivation_candidate_count = proof_mark;
	delta->db->candidate_premise_count = premise_mark;
	return -1;
}

int prototype_judgement_expand_type_def(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!type_instance_has_known_type(terms, type_declarations, subject) ||
		!term_is_universe_var(terms, classifier)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO);
}

int prototype_judgement_add_type_formation_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_claim_id
) {
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (!p_claim_id || !term_exists(terms, subject) ||
		!term_exists(terms, classifier) ||
		prototype_term_type_instance_info(
			terms, subject, &type_id, arguments, &argument_count
		) != 0 || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if ((argument_count == 0 && type->formation_classifier != classifier) ||
		(argument_count != 0 &&
			(!type_instance_has_known_type(terms, type_declarations, subject) ||
			 !term_is_universe_var(terms, classifier)))) {
		return -1;
	}
	struct prototype_judgement_proposition proposition = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate derivation;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO;
	derivation.conclusion_kind = proposition.kind;
	derivation.conclusion_context_id = context_id;
	derivation.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	derivation.conclusion_subject = subject;
	derivation.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&derivation, premises);
	initialize_proof_premises(&derivation, NULL, NULL, NULL, 0);
	return publish_complete_relation(
		judgement,
		&proposition,
		&derivation,
		NULL,
		p_claim_id
	);
}

int prototype_judgement_expand_constructor_def(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_CONSTRUCTOR) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	const struct prototype_term* constructor = &terms->terms[subject];
	if (!constructor_belongs_to_owner(
			terms,
			type_declarations,
			constructor->as.constructor.owner,
			constructor->as.constructor.constructor_id
		) ||
		!classifier_returns_owner(
			terms,
			type_declarations,
			classifier,
			constructor->as.constructor.owner
		)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO);
}

int prototype_judgement_add_constructor_intro_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_claim_id
) {
	if (!p_claim_id || !term_has_tag(
			terms, subject, PROTOTYPE_TERM_CONSTRUCTOR
		) || !term_exists(terms, classifier)) {
		return -1;
	}
	const struct prototype_term* constructor = &terms->terms[subject];
	if (!constructor_belongs_to_owner(
			terms,
			type_declarations,
			constructor->as.constructor.owner,
			constructor->as.constructor.constructor_id
		) || !classifier_returns_owner(
			terms,
			type_declarations,
			classifier,
			constructor->as.constructor.owner
		)) {
		return -1;
	}
	struct prototype_judgement_proposition proposition = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate derivation;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO;
	derivation.conclusion_kind = proposition.kind;
	derivation.conclusion_context_id = context_id;
	derivation.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	derivation.conclusion_subject = subject;
	derivation.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&derivation, premises);
	initialize_proof_premises(&derivation, NULL, NULL, NULL, 0);
	return publish_complete_relation(
		judgement,
		&proposition,
		&derivation,
		NULL,
		p_claim_id
	);
}

int prototype_judgement_add_constructor_spine_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* argument_claim_ids,
	uint32_t argument_claim_count,
	uint32_t* p_claim_id
) {
	if (!judgement || !terms || !type_declarations || !contexts ||
		!substitutions || !p_claim_id || argument_claim_count == 0 ||
		argument_claim_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		!argument_claim_ids) {
		return -1;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count;
	uint32_t argument_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_candidate_premise
		premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	if (prototype_term_constructor_spine_info(
			terms,
			subject,
			&head,
			&owner,
			&constructor_index,
			arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&argument_count
		) != 0 || argument_count != argument_claim_count) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		const struct prototype_judgement_proposition* premise =
			prototype_judgement_claim_proposition(
				judgement, argument_claim_ids[i]
			);
		if (!premise || premise->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			premise->context_id != context_id || premise->subject != arguments[i]) {
			return -1;
		}
		argument_classifiers[i] = premise->classifier;
	}
	uint32_t expected_classifier;
	int saturated;
	if (prototype_judgement_constructor_spine_classifier(
			terms,
			type_declarations,
			contexts,
			substitutions,
			context_id,
			subject,
			owner,
			argument_classifiers,
			argument_count,
			&expected_classifier,
			&saturated
		) != 0 || !saturated || prototype_judgement_classifier_conversion(
			terms, type_declarations, expected_classifier, classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	struct prototype_judgement_proposition proposition = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate derivation;
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION;
	derivation.conclusion_kind = proposition.kind;
	derivation.conclusion_context_id = context_id;
	derivation.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	derivation.conclusion_subject = subject;
	derivation.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&derivation, premises);
	derivation.rule_data.constructor.owner_view = owner;
	derivation.premise_count = argument_count;
	for (uint32_t i = 0; i < argument_count; ++i) {
		premises[i].proposition = *prototype_judgement_claim_proposition(
			judgement, argument_claim_ids[i]
		);
	}
	(void)head;
	(void)constructor_index;
	return publish_complete_relation(
		judgement,
		&proposition,
		&derivation,
		argument_claim_ids,
		p_claim_id
	);
}

int prototype_judgement_indexed_branch_refinement(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee_context_id,
	uint32_t scrutinee_term,
	uint32_t scrutinee_classifier,
	uint32_t constructor_index,
	uint32_t branch_context_id,
	const struct prototype_case_binder* branch_binders,
	uint32_t branch_binder_count,
	uint32_t* p_refined_context_id,
	uint32_t* p_refinement_substitution_id,
	uint32_t* p_constructor_term
) {
	uint32_t type_id;
	uint32_t index_args[16];
	uint32_t index_count;
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_refined_context_id || !p_refinement_substitution_id ||
		!p_constructor_term || branch_binder_count > 64 ||
		(branch_binder_count != 0 && !branch_binders) ||
		scrutinee_term >= terms->term_count ||
		terms->terms[scrutinee_term].tag != PROTOTYPE_TERM_VAR ||
		prototype_term_type_instance_info(
			terms,
			scrutinee_classifier,
			&type_id,
			index_args,
			&index_count
		) != 0 || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count != 0 || type->index_count != 2 ||
		index_count != 2 || constructor_index >= type->constructor_count ||
		index_args[0] >= terms->term_count || index_args[1] >= terms->term_count ||
		terms->terms[index_args[0]].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[index_args[1]].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t left_context_id;
	uint32_t right_context_id;
	uint32_t proof_context_id;
	if (prototype_context_find_binding(
			contexts,
			scrutinee_context_id,
			terms->terms[index_args[0]].as.var.binding_id,
			&left_context_id
		) != 0 || prototype_context_find_binding(
			contexts,
			scrutinee_context_id,
			terms->terms[index_args[1]].as.var.binding_id,
			&right_context_id
		) != 0 || prototype_context_find_binding(
			contexts,
			scrutinee_context_id,
			terms->terms[scrutinee_term].as.var.binding_id,
			&proof_context_id
		) != 0) {
		return -1;
	}
	const struct prototype_context* left_context =
		prototype_context_get(contexts, left_context_id);
	const struct prototype_context* right_context =
		prototype_context_get(contexts, right_context_id);
	const struct prototype_context* proof_context =
		prototype_context_get(contexts, proof_context_id);
	if (!left_context || !right_context || !proof_context ||
		right_context->parent != left_context_id ||
		proof_context->parent != right_context_id ||
		proof_context_id != scrutinee_context_id ||
		prototype_context_classifier_term(proof_context) !=
			scrutinee_classifier) {
		return -1;
	}
	uint32_t constructor_id = type->first_constructor + constructor_index;
	if (constructor_id >= type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[constructor_id];
	uint32_t result_type_id;
	uint32_t result_args[16];
	uint32_t result_arg_count;
	const struct prototype_context* parameter_context = prototype_context_get(
		contexts, constructor->parameter_context
	);
	const struct prototype_context* field_context = prototype_context_get(
		contexts, constructor->field_context
	);
	if (constructor->constructor_index != constructor_index ||
		!parameter_context || !field_context || field_context->depth <
			parameter_context->depth || field_context->depth -
			parameter_context->depth != branch_binder_count ||
		prototype_term_type_instance_info(
			terms,
			constructor->result_classifier,
			&result_type_id,
			result_args,
			&result_arg_count
		) != 0 || result_type_id != type_id || result_arg_count != 2) {
		return -1;
	}
	uint32_t base_context_id = left_context->parent;
	uint32_t target_branch_path[64];
	uint32_t target_branch_count;
	if (prototype_context_extension_path(
			contexts,
			scrutinee_context_id,
			branch_context_id,
			target_branch_path,
			64,
			&target_branch_count
		) != 0 || target_branch_count != branch_binder_count) {
		return -1;
	}

	uint32_t identity_former;
	uint32_t refined_context_id = base_context_id;
	uint32_t refined_path[64];
	if (prototype_term_type_instance_make(
			terms, type_declarations, type_id, NULL, 0, &identity_former
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < branch_binder_count; ++i) {
		const struct prototype_context* target_field = prototype_context_get(
			contexts, target_branch_path[i]
		);
		uint32_t field_classifier;
		uint32_t next_context;
		if (!target_field || target_field->binding_id !=
				branch_binders[i].binding_id ||
			prototype_judgement_constructor_field_classifier(
				terms,
				type_declarations,
				contexts,
				substitutions,
				refined_context_id,
				identity_former,
				constructor_index,
				branch_binders,
				i,
				i,
				&field_classifier
			) != 0 || prototype_context_extend(
				contexts,
				refined_context_id,
				branch_binders[i].binding_id,
				field_classifier,
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0) {
			return -1;
		}
		refined_context_id = next_context;
		refined_path[i] = next_context;
	}

	uint32_t declaration_path[64];
	uint32_t declaration_count;
	uint32_t declaration_substitution;
	if (prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			declaration_path,
			64,
			&declaration_count
		) != 0 || declaration_count != branch_binder_count ||
		prototype_context_substitution_from_terms(
			contexts,
			substitutions,
			terms,
			type_declarations,
			refined_context_id,
			constructor->parameter_context,
			NULL,
			0,
			&declaration_substitution
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < branch_binder_count; ++i) {
		const struct prototype_context* refined_field = prototype_context_get(
			contexts, refined_path[i]
		);
		uint32_t field;
		if (!refined_field || prototype_term_var(
				terms, branch_binders[i].binding_id, &field
			) != 0 || prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				type_declarations,
				declaration_substitution,
				declaration_path[i],
				field,
				prototype_context_classifier_term(refined_field),
				&declaration_substitution
			) != 0) {
			return -1;
		}
	}
	uint32_t refined_result_args[16];
	for (uint32_t i = 0; i < result_arg_count; ++i) {
		if (prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				result_args[i],
				declaration_substitution,
				&refined_result_args[i]
			) != 0) {
			return -1;
		}
	}

	uint32_t substitution;
	uint32_t owner;
	uint32_t constructor_term;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			refined_context_id,
			base_context_id,
			&substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			substitution,
			left_context_id,
			refined_result_args[0],
			prototype_context_classifier_term(left_context),
			&substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			substitution,
			right_context_id,
			refined_result_args[1],
			prototype_context_classifier_term(right_context),
			&substitution
		) != 0 || prototype_term_type_instance_make(
			terms,
			type_declarations,
			type_id,
			refined_result_args,
			2,
			&owner
		) != 0 || prototype_term_constructor(
			terms, owner, constructor_index, &constructor_term
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < branch_binder_count; ++i) {
		uint32_t field;
		if (prototype_term_var(
				terms, branch_binders[i].binding_id, &field
			) != 0 || prototype_term_app(
				terms, constructor_term, field, &constructor_term
			) != 0) {
			return -1;
		}
	}
	if (prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			substitution,
			proof_context_id,
			constructor_term,
			owner,
			&substitution
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < branch_binder_count; ++i) {
		const struct prototype_context* refined_field = prototype_context_get(
			contexts, refined_path[i]
		);
		uint32_t field;
		if (!refined_field || prototype_term_var(
				terms, branch_binders[i].binding_id, &field
			) != 0 || prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				type_declarations,
				substitution,
				target_branch_path[i],
				field,
				prototype_context_classifier_term(refined_field),
				&substitution
			) != 0) {
			return -1;
		}
	}
	*p_refined_context_id = refined_context_id;
	*p_refinement_substitution_id = substitution;
	*p_constructor_term = constructor_term;
	return 0;
}

int prototype_judgement_delta_record_context_binding_assumption(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t binding_id,
	uint32_t classifier
) {
	uint32_t subject;
	if (!delta || !delta->contexts ||
		classifier >= terms->term_count ||
		!prototype_context_contains_binding(
			delta->contexts,
			delta->current_context_id,
			binding_id
		) || prototype_term_var(terms, binding_id, &subject) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
	);
}

int prototype_judgement_delta_record_match_pattern(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	uint32_t constructor_owner_view,
	uint32_t constructor_index,
	uint32_t constructor_field_index
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_VAR) ||
		classifier >= terms->term_count ||
		constructor_owner_view == PROTOTYPE_INVALID_ID ||
		constructor_index == PROTOTYPE_INVALID_ID ||
		constructor_field_index == PROTOTYPE_INVALID_ID) {
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
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = relation.operation_id;
	proof.conclusion_subject = relation.subject;
	proof.conclusion_classifier = relation.classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.rule_data.constructor.owner_view = constructor_owner_view;
	proof.rule_data.constructor.constructor_index = constructor_index;
	proof.rule_data.constructor.field_index = constructor_field_index;
	return add_complete_delta_relation(delta, &relation, &proof);
}
