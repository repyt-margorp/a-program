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
	judgement_index_clear(delta->candidate_index_heads);
	return 0;

rollback:
	delta->db->derivation_candidate_count = proof_mark;
	delta->db->candidate_premise_count = premise_mark;
	(void)prototype_judgement_db_rebuild_index(delta->db);
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
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate derivation;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO;
	initialize_proof_rule_parameters(
		&derivation,
		premises,
		(struct prototype_judgement_proposition[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		]){{0}}
	);
	initialize_proof_premises(&derivation, context_id, NULL, NULL, NULL, 0);
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
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate derivation;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO;
	initialize_proof_rule_parameters(
		&derivation,
		premises,
		(struct prototype_judgement_proposition[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		]){{0}}
	);
	initialize_proof_premises(&derivation, context_id, NULL, NULL, NULL, 0);
	return publish_complete_relation(
		judgement,
		&proposition,
		&derivation,
		NULL,
		p_claim_id
	);
}

int prototype_judgement_delta_record_constructor_intro(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !term_has_tag(terms, subject, PROTOTYPE_TERM_CONSTRUCTOR) ||
		!term_exists(terms, classifier)) {
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
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO
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
		) != 0 || !saturated ||
		(!(prototype_judgement_classifier_conversion(
			terms, type_declarations, expected_classifier, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) &&
		 !prototype_judgement_classifier_reference_equal(
			terms, type_declarations, expected_classifier, classifier
		))) {
		return -1;
	}
	struct prototype_judgement_proposition proposition = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate derivation;
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION;
	initialize_proof_rule_parameters(
		&derivation,
		premises,
		(struct prototype_judgement_proposition[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		]){{0}}
	);
	derivation.rule_data.constructor.owner_view = owner;
	derivation.premise_count = argument_count;
	for (uint32_t i = 0; i < argument_count; ++i) {
		premises[i].proposition_store_kind =
			PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB;
		premises[i].proposition_id =
			judgement->claims[argument_claim_ids[i]].proposition_id;
		premises[i].proposition =
			(struct prototype_judgement_proposition*)prototype_judgement_claim_proposition(
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

int prototype_judgement_solve_index_pattern(
	const struct prototype_term_db* terms,
	uint32_t binding_id,
	uint32_t pattern,
	uint32_t value,
	uint32_t* p_solution
) {
	if (!terms || !p_solution || pattern >= terms->term_count ||
		value >= terms->term_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	const struct prototype_term* left = &terms->terms[pattern];
	const struct prototype_term* right = &terms->terms[value];
	if (left->tag == PROTOTYPE_TERM_VAR &&
		left->as.var.binding_id == binding_id) {
		if (prototype_term_contains_free_binding(terms, value, binding_id)) {
			return value == pattern ? PROTOTYPE_INDEX_REFINEMENT_RESIDUAL :
				PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
		}
		*p_solution = value;
		return PROTOTYPE_INDEX_REFINEMENT_SOLVED;
	}
	uint32_t left_head = pattern;
	uint32_t right_head = value;
	while (left_head < terms->term_count &&
		terms->terms[left_head].tag == PROTOTYPE_TERM_APP) {
		left_head = terms->terms[left_head].as.app.function;
	}
	while (right_head < terms->term_count &&
		terms->terms[right_head].tag == PROTOTYPE_TERM_APP) {
		right_head = terms->terms[right_head].as.app.function;
	}
	if (left_head < terms->term_count && right_head < terms->term_count &&
		terms->terms[left_head].tag == PROTOTYPE_TERM_CONSTRUCTOR &&
		terms->terms[right_head].tag == PROTOTYPE_TERM_CONSTRUCTOR &&
		terms->terms[left_head].as.constructor.owner ==
			terms->terms[right_head].as.constructor.owner &&
		terms->terms[left_head].as.constructor.constructor_id !=
			terms->terms[right_head].as.constructor.constructor_id) {
		return PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
	}
	if (left->tag != PROTOTYPE_TERM_APP || right->tag != PROTOTYPE_TERM_APP) {
		if (left->tag == PROTOTYPE_TERM_CONSTRUCTOR &&
			right->tag == PROTOTYPE_TERM_CONSTRUCTOR &&
			left->as.constructor.owner == right->as.constructor.owner &&
			left->as.constructor.constructor_id !=
				right->as.constructor.constructor_id) {
			return PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
		}
		return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
	}
	int same_function = 0;
	if (prototype_term_core_shape_equal(
			terms, left->as.app.function, right->as.app.function,
			&same_function
		) != 0) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	if (!same_function) {
		const struct prototype_term* left_function =
			&terms->terms[left->as.app.function];
		const struct prototype_term* right_function =
			&terms->terms[right->as.app.function];
		if (left_function->tag == PROTOTYPE_TERM_CONSTRUCTOR &&
			right_function->tag == PROTOTYPE_TERM_CONSTRUCTOR &&
			left_function->as.constructor.owner ==
				right_function->as.constructor.owner &&
			left_function->as.constructor.constructor_id !=
				right_function->as.constructor.constructor_id) {
			return PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
		}
		return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
	}
	return prototype_judgement_solve_index_pattern(
		terms, binding_id, left->as.app.argument, right->as.app.argument,
		p_solution
	);
}

/* Return 1 only for a constructor-disjoint equation, 0 when the terms may
 * still unify, and -1 for malformed input. This deliberately proves less than
 * full index unification: it is the context-layout-independent contradiction
 * check needed before source index binders are available. */
static int source_index_terms_are_constructor_disjoint(
	const struct prototype_term_db* terms,
	uint32_t left,
	uint32_t right,
	uint32_t depth
) {
	if (!terms || left >= terms->term_count || right >= terms->term_count ||
		depth > 64) {
		return -1;
	}
	while (left < terms->term_count &&
		terms->terms[left].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		left = terms->terms[left].as.type_view.core;
	}
	while (right < terms->term_count &&
		terms->terms[right].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		right = terms->terms[right].as.type_view.core;
	}
	if (left >= terms->term_count || right >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* left_term = &terms->terms[left];
	const struct prototype_term* right_term = &terms->terms[right];
	uint32_t left_head = left;
	uint32_t right_head = right;
	while (terms->terms[left_head].tag == PROTOTYPE_TERM_APP) {
		left_head = terms->terms[left_head].as.app.function;
	}
	while (terms->terms[right_head].tag == PROTOTYPE_TERM_APP) {
		right_head = terms->terms[right_head].as.app.function;
	}
	const struct prototype_term* left_head_term = &terms->terms[left_head];
	const struct prototype_term* right_head_term = &terms->terms[right_head];
	if (left_head_term->tag == PROTOTYPE_TERM_CONSTRUCTOR &&
		right_head_term->tag == PROTOTYPE_TERM_CONSTRUCTOR &&
		left_head_term->as.constructor.owner ==
			right_head_term->as.constructor.owner &&
		left_head_term->as.constructor.constructor_id !=
			right_head_term->as.constructor.constructor_id) {
		return 1;
	}
	if (left_term->tag == PROTOTYPE_TERM_APP &&
		right_term->tag == PROTOTYPE_TERM_APP) {
		int head = source_index_terms_are_constructor_disjoint(
			terms, left_term->as.app.function,
			right_term->as.app.function, depth + 1
		);
		if (head != 0) {
			return head;
		}
		return source_index_terms_are_constructor_disjoint(
			terms, left_term->as.app.argument,
			right_term->as.app.argument, depth + 1
		);
	}
	return 0;
}

static int source_index_unique_free_context_binding(
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t term,
	uint32_t* p_binding_context
) {
	if (!contexts || !terms || !p_binding_context ||
		!prototype_context_get(contexts, context_id) || term >= terms->term_count) {
		return -1;
	}
	uint32_t found = PROTOTYPE_INVALID_ID;
	uint32_t cursor = context_id;
	while (cursor != prototype_context_empty(contexts)) {
		const struct prototype_context* entry = prototype_context_get(
			contexts, cursor
		);
		if (!entry) {
			return -1;
		}
		if (prototype_term_contains_free_binding(
				terms, term, entry->binding_id
			)) {
			if (found != PROTOTYPE_INVALID_ID) {
				return 1;
			}
			found = cursor;
		}
		cursor = entry->parent;
	}
	*p_binding_context = found;
	return 0;
}

int prototype_judgement_source_match_branch_refinement(
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
	uint32_t* p_constructor_term,
	uint32_t* p_residual_pattern,
	uint32_t* p_residual_value
) {
	uint32_t type_id;
	uint32_t scrutinee_arguments[32];
	uint32_t scrutinee_argument_count;
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_refined_context_id || !p_refinement_substitution_id ||
		!p_constructor_term || !p_residual_pattern || !p_residual_value ||
		branch_binder_count > 64 ||
		(branch_binder_count != 0 && !branch_binders) ||
		scrutinee_term >= terms->term_count ||
		prototype_term_type_instance_info(
			terms, scrutinee_classifier, &type_id, scrutinee_arguments,
			&scrutinee_argument_count
		) != 0 || type_id >= type_declarations->type_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	*p_residual_pattern = PROTOTYPE_INVALID_ID;
	*p_residual_value = PROTOTYPE_INVALID_ID;
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count + type->index_count !=
			scrutinee_argument_count || constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			type_declarations->constructor_count) {
		return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	if (terms->terms[scrutinee_term].tag != PROTOTYPE_TERM_VAR) {
		uint32_t concrete_head;
		uint32_t concrete_owner;
		uint32_t concrete_constructor;
		uint32_t concrete_arguments[64];
		uint32_t concrete_argument_count;
		int concrete_status = prototype_term_constructor_spine_info(
			terms,
			scrutinee_term,
			&concrete_head,
			&concrete_owner,
			&concrete_constructor,
			concrete_arguments,
			64,
			&concrete_argument_count
		);
		if (concrete_status < 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
		if (concrete_status > 0) {
			*p_residual_pattern = scrutinee_term;
			return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
		}
		uint32_t concrete_type_id;
		uint32_t concrete_owner_arguments[32];
		uint32_t concrete_owner_argument_count;
		if (prototype_term_type_instance_info(
				terms,
				concrete_owner,
				&concrete_type_id,
				concrete_owner_arguments,
				&concrete_owner_argument_count
			) != 0 || concrete_type_id != type_id) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
		(void)concrete_head;
		(void)concrete_owner_arguments;
		(void)concrete_owner_argument_count;
		if (concrete_constructor != constructor_index) {
			return PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
		}
		if (concrete_argument_count != branch_binder_count ||
			!prototype_context_get(contexts, branch_context_id) ||
			prototype_substitution_identity(
				substitutions,
				contexts,
				branch_context_id,
				p_refinement_substitution_id
			) != 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
		*p_refined_context_id = branch_context_id;
		*p_constructor_term = scrutinee_term;
		return PROTOTYPE_INDEX_REFINEMENT_SOLVED;
	}
	uint32_t raw_result_type_id;
	uint32_t raw_result_arguments[32];
	uint32_t raw_result_argument_count;
	if (prototype_term_type_instance_info(
			terms, constructor->result_classifier, &raw_result_type_id,
			raw_result_arguments, &raw_result_argument_count
		) != 0 || raw_result_type_id != type_id || raw_result_argument_count !=
			type->parameter_count + type->index_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	for (uint32_t i = 0; i < type->index_count; ++i) {
		int disjoint = source_index_terms_are_constructor_disjoint(
			terms,
			scrutinee_arguments[type->parameter_count + i],
			raw_result_arguments[type->parameter_count + i],
			0
		);
		if (disjoint < 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
		if (disjoint > 0) {
			return PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
		}
	}
	uint32_t index_context_path[32];
	uint32_t index_context_count;
	if (type->index_count > 32 || prototype_context_extension_path(
			contexts, type->parameter_context, type->index_context,
			index_context_path, 32, &index_context_count
		) != 0 || index_context_count != type->index_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	uint32_t scrutinee_binding_context;
	const struct prototype_context* scrutinee_context;
	if (prototype_context_find_binding(
			contexts, scrutinee_context_id,
			terms->terms[scrutinee_term].as.var.binding_id,
			&scrutinee_binding_context
		) != 0 || !(scrutinee_context = prototype_context_get(
			contexts, scrutinee_binding_context
		))) {
		return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
	}
	uint32_t source_index_contexts[32];
	uint32_t base_context = scrutinee_context->parent;
	uint32_t earliest_depth = UINT32_MAX;
	for (uint32_t i = 0; i < type->index_count; ++i) {
		int binding_status = source_index_unique_free_context_binding(
			contexts, terms, scrutinee_context->parent,
			scrutinee_arguments[type->parameter_count + i],
			&source_index_contexts[i]
		);
		if (binding_status < 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
		if (binding_status > 0) {
			*p_residual_pattern =
				scrutinee_arguments[type->parameter_count + i];
			*p_residual_value =
				raw_result_arguments[type->parameter_count + i];
			return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
		}
		if (source_index_contexts[i] != PROTOTYPE_INVALID_ID) {
			const struct prototype_context* source_index = prototype_context_get(
				contexts, source_index_contexts[i]
			);
			if (!source_index) {
				return PROTOTYPE_INDEX_REFINEMENT_INVALID;
			}
			if (source_index->depth < earliest_depth) {
				earliest_depth = source_index->depth;
				base_context = source_index->parent;
			}
		}
	}
	uint32_t branch_path[64];
	uint32_t branch_path_count;
	if (constructor->constructor_index != constructor_index ||
		prototype_context_extension_path(
			contexts, scrutinee_context_id, branch_context_id,
			branch_path, 64, &branch_path_count
		) != 0 || branch_path_count != branch_binder_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	uint32_t owner_arguments[32];
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		owner_arguments[i] = scrutinee_arguments[i];
	}
	uint32_t owner;
	if (prototype_term_type_instance_make(
			terms, type_declarations, type_id, owner_arguments,
			type->parameter_count, &owner
		) != 0) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	uint32_t refined_context = base_context;
	uint32_t refined_path[64];
	for (uint32_t i = 0; i < branch_binder_count; ++i) {
		const struct prototype_context* target = prototype_context_get(
			contexts, branch_path[i]
		);
		uint32_t field_classifier;
		if (!target || target->binding_id != branch_binders[i].binding_id ||
			prototype_judgement_constructor_field_classifier(
				terms, type_declarations, contexts, substitutions,
				refined_context, owner, constructor_index,
				branch_binders, i, i, &field_classifier
			) != 0 || prototype_context_extend(
				contexts, refined_context, branch_binders[i].binding_id,
				field_classifier, PROTOTYPE_INVALID_ID, &refined_context
			) != 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
		refined_path[i] = refined_context;
	}
	uint32_t declaration_substitution;
	if (prototype_context_substitution_from_terms(
			contexts, substitutions, terms, type_declarations,
			refined_context, constructor->parameter_context,
			scrutinee_arguments, type->parameter_count,
			&declaration_substitution
		) != 0) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	uint32_t declaration_path[64];
	uint32_t declaration_count;
	if (prototype_context_extension_path(
			contexts, constructor->parameter_context, constructor->field_context,
			declaration_path, 64, &declaration_count
		) != 0 || declaration_count != branch_binder_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	for (uint32_t i = 0; i < branch_binder_count; ++i) {
		const struct prototype_context* field_context = prototype_context_get(
			contexts, refined_path[i]
		);
		uint32_t field;
		if (!field_context || prototype_term_var(
				terms, branch_binders[i].binding_id, &field
			) != 0 || prototype_substitution_extend(
				substitutions, contexts, terms, type_declarations,
				declaration_substitution, declaration_path[i], field,
				prototype_context_classifier_term(field_context),
				&declaration_substitution
			) != 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
	}
	uint32_t result_type_id;
	uint32_t result_arguments[32];
	uint32_t result_argument_count;
	if (prototype_term_type_instance_info(
			terms, constructor->result_classifier, &result_type_id,
			result_arguments, &result_argument_count
		) != 0 || result_type_id != type_id || result_argument_count !=
			type->parameter_count + type->index_count) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	uint32_t specialized_arguments[32];
	for (uint32_t i = 0; i < result_argument_count; ++i) {
		if (prototype_term_reindex(
				terms, type_declarations, contexts, substitutions,
				result_arguments[i], declaration_substitution,
				&specialized_arguments[i]
			) != 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
	}
	uint32_t index_solutions[32];
	for (uint32_t i = 0; i < type->index_count; ++i) {
		index_solutions[i] = PROTOTYPE_INVALID_ID;
		if (source_index_contexts[i] == PROTOTYPE_INVALID_ID) {
			if (prototype_judgement_classifier_conversion(
					terms, type_declarations,
					scrutinee_arguments[type->parameter_count + i],
					specialized_arguments[type->parameter_count + i]
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				int disjoint = source_index_terms_are_constructor_disjoint(
					terms,
					scrutinee_arguments[type->parameter_count + i],
					specialized_arguments[type->parameter_count + i], 0
				);
				if (disjoint < 0) {
					return PROTOTYPE_INDEX_REFINEMENT_INVALID;
				}
				if (disjoint > 0) {
					return PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE;
				}
			}
			continue;
		}
		const struct prototype_context* source_index = prototype_context_get(
			contexts, source_index_contexts[i]
		);
		if (!source_index) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
		int solve_status = prototype_judgement_solve_index_pattern(
			terms, source_index->binding_id,
			scrutinee_arguments[type->parameter_count + i],
			specialized_arguments[type->parameter_count + i],
			&index_solutions[i]
		);
		if (solve_status != PROTOTYPE_INDEX_REFINEMENT_SOLVED) {
			if (solve_status == PROTOTYPE_INDEX_REFINEMENT_RESIDUAL) {
				*p_residual_pattern =
					scrutinee_arguments[type->parameter_count + i];
				*p_residual_value =
					specialized_arguments[type->parameter_count + i];
			}
			return solve_status;
		}
	}
	uint32_t constructor_result;
	uint32_t constructor_term;
	if (prototype_term_type_instance_make(
			terms, type_declarations, type_id, specialized_arguments,
			result_argument_count, &constructor_result
		) != 0 || prototype_term_constructor(
			terms, owner, constructor_index, &constructor_term
		) != 0) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	for (uint32_t i = 0; i < branch_binder_count; ++i) {
		uint32_t field;
		if (prototype_term_var(
				terms, branch_binders[i].binding_id, &field
			) != 0 || prototype_term_app(
				terms, constructor_term, field, &constructor_term
			) != 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
	}
	uint32_t substitution;
	if (prototype_substitution_projection_path(
			substitutions, contexts, refined_context, base_context,
			&substitution
		) != 0) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	uint32_t source_path[512];
	uint32_t source_path_count;
	if (prototype_context_extension_path(
			contexts, base_context, branch_context_id,
			source_path, 512, &source_path_count
		) != 0) {
		return PROTOTYPE_INDEX_REFINEMENT_INVALID;
	}
	for (uint32_t path_index = 0; path_index < source_path_count; ++path_index) {
		uint32_t source_extension = source_path[path_index];
		const struct prototype_context* source_entry = prototype_context_get(
			contexts, source_extension
		);
		if (!source_entry) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
		uint32_t image = PROTOTYPE_INVALID_ID;
		uint32_t image_classifier = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < type->index_count; ++i) {
			if (source_index_contexts[i] != source_extension) {
				continue;
			}
			if (image != PROTOTYPE_INVALID_ID &&
				prototype_judgement_classifier_conversion(
					terms, type_declarations, image, index_solutions[i]
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			}
			image = index_solutions[i];
		}
		if (source_extension == scrutinee_binding_context) {
			image = constructor_term;
			image_classifier = constructor_result;
		}
		for (uint32_t i = 0; i < branch_binder_count; ++i) {
			if (branch_path[i] != source_extension) {
				continue;
			}
			const struct prototype_context* field_context = prototype_context_get(
				contexts, refined_path[i]
			);
			if (!field_context || prototype_term_var(
					terms, branch_binders[i].binding_id, &image
				) != 0) {
				return PROTOTYPE_INDEX_REFINEMENT_INVALID;
			}
			image_classifier = prototype_context_classifier_term(field_context);
		}
		if (image == PROTOTYPE_INVALID_ID) {
			uint32_t source_classifier = prototype_context_classifier_term(
				source_entry
			);
			uint32_t classifier;
			uint32_t target_extension;
			uint32_t projection;
			uint32_t weakened_substitution;
			uint32_t variable;
			if (source_classifier == PROTOTYPE_INVALID_ID) {
				return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			}
			if (prototype_term_reindex(
					terms, type_declarations, contexts, substitutions,
					source_classifier, substitution, &classifier
				) != 0) {
				return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			}
			if (prototype_context_extend(
					contexts, refined_context, source_entry->binding_id,
					classifier, PROTOTYPE_INVALID_ID, &target_extension
				) != 0) {
				return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			}
			if (prototype_substitution_projection(
					substitutions, contexts, target_extension, &projection
				) != 0) {
				return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			}
			if (prototype_substitution_compose(
					substitutions, contexts, substitution, projection,
					&weakened_substitution
				) != 0) {
				return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			}
			if (prototype_term_var(
					terms, source_entry->binding_id, &variable
				) != 0) {
				return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			}
			if (prototype_substitution_extend(
					substitutions, contexts, terms, type_declarations,
					weakened_substitution, source_extension, variable,
					classifier, &substitution
				) != 0) {
				return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
			}
			refined_context = target_extension;
			continue;
		}
		if (image_classifier == PROTOTYPE_INVALID_ID &&
			(prototype_term_reindex(
				terms, type_declarations, contexts, substitutions,
				prototype_context_classifier_term(source_entry), substitution,
				&image_classifier
			) != 0 || prototype_judgement_classifier_value_whnf(
				terms, type_declarations, image_classifier, &image_classifier
			) != 0)) {
			return PROTOTYPE_INDEX_REFINEMENT_RESIDUAL;
		}
		if (prototype_substitution_extend(
				substitutions, contexts, terms, type_declarations,
				substitution, source_extension, image,
				image_classifier, &substitution
			) != 0) {
			return PROTOTYPE_INDEX_REFINEMENT_INVALID;
		}
	}
	*p_refined_context_id = refined_context;
	*p_refinement_substitution_id = substitution;
	*p_constructor_term = constructor_term;
	return PROTOTYPE_INDEX_REFINEMENT_SOLVED;
}

static int dimension_action_nullary_branch_refinement(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_dimension_operator_db* dimension_operators,
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
	(void)branch_binders;
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_refined_context_id ||
		!p_refinement_substitution_id || !p_constructor_term ||
		branch_binder_count != 0 ||
		scrutinee_term >= terms->term_count ||
		terms->terms[scrutinee_term].tag != PROTOTYPE_TERM_VAR ||
		scrutinee_classifier >= terms->term_count) {
		return 1;
	}
	uint32_t right_app = scrutinee_classifier;
	if (terms->terms[right_app].tag != PROTOTYPE_TERM_APP) {
		return 1;
	}
	uint32_t right_endpoint = terms->terms[right_app].as.app.argument;
	uint32_t left_app = terms->terms[right_app].as.app.function;
	if (left_app >= terms->term_count ||
		terms->terms[left_app].tag != PROTOTYPE_TERM_APP) {
		return 1;
	}
	uint32_t left_endpoint = terms->terms[left_app].as.app.argument;
	uint32_t acted_type = terms->terms[left_app].as.app.function;
	uint32_t source_type;
	uint32_t operator_id;
	if (prototype_term_dimension_action_info(
			terms, acted_type, &source_type, &operator_id
		) != 0 || !dimension_operators) {
		return 1;
	}
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(dimension_operators, operator_id);
	struct prototype_type_schema_view type_view;
	struct prototype_constructor_schema_view constructor_view;
	uint32_t acted_classifier;
	if (!operator || operator->source_dimension != 0 ||
		operator->target_dimension != 1) {
		fprintf(stderr, "dimension branch operator failed operator=%u\n", operator_id);
		return -1;
	}
	if (prototype_type_schema_view_query(
			type_declarations,
			contexts,
			terms,
			dimension_operators,
			acted_type,
			&type_view
		) != 0 || constructor_index >=
			type_view.source_declaration->constructor_count) {
		fprintf(stderr, "dimension branch type view failed acted=%u ctor=%u\n",
			acted_type, constructor_index);
		return -1;
	}
	if (prototype_constructor_schema_view_query(
			type_declarations,
			contexts,
			terms,
			dimension_operators,
			&type_view,
			constructor_index,
			&constructor_view
		) != 0) {
		fprintf(stderr, "dimension branch constructor view failed acted=%u ctor=%u\n",
			acted_type, constructor_index);
		return -1;
	}
	if (prototype_constructor_schema_view_action_classifier(
			type_declarations,
			contexts,
			terms,
			dimension_operators,
			&type_view,
			constructor_index,
			&acted_classifier
		) != 0) {
		fprintf(stderr, "dimension branch classifier query failed acted=%u ctor=%u\n",
			acted_type, constructor_index);
		return -1;
	}
	if (constructor_view.source_classifier < terms->term_count &&
		terms->terms[constructor_view.source_classifier].tag == PROTOTYPE_TERM_PI) {
		return 1;
	}
	if (left_endpoint >= terms->term_count || right_endpoint >= terms->term_count ||
		terms->terms[left_endpoint].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[right_endpoint].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t left_context_id;
	uint32_t right_context_id;
	uint32_t proof_context_id;
	if (prototype_context_find_binding(
			contexts,
			scrutinee_context_id,
			terms->terms[left_endpoint].as.var.binding_id,
			&left_context_id
		) != 0 || prototype_context_find_binding(
			contexts,
			scrutinee_context_id,
			terms->terms[right_endpoint].as.var.binding_id,
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
		branch_context_id != scrutinee_context_id ||
		prototype_context_classifier_term(proof_context) != scrutinee_classifier) {
		return -1;
	}
	uint32_t base_context_id = left_context->parent;
	uint32_t substitution;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			base_context_id,
			base_context_id,
			&substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			substitution,
			left_context_id,
			constructor_view.source_constructor_term,
			prototype_context_classifier_term(left_context),
			&substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			substitution,
			right_context_id,
			constructor_view.source_constructor_term,
			prototype_context_classifier_term(right_context),
			&substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			substitution,
			proof_context_id,
			constructor_view.acted_constructor_term,
			acted_classifier,
			&substitution
		) != 0) {
		return -1;
	}
	*p_refined_context_id = base_context_id;
	*p_refinement_substitution_id = substitution;
	*p_constructor_term = constructor_view.acted_constructor_term;
	return 0;
}

int prototype_judgement_identity_relation_branch_refinement(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_dimension_operator_db* dimension_operators,
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
	int action_status = dimension_action_nullary_branch_refinement(
		contexts,
		substitutions,
		terms,
		type_declarations,
		dimension_operators,
		scrutinee_context_id,
		scrutinee_term,
		scrutinee_classifier,
		constructor_index,
		branch_context_id,
		branch_binders,
		branch_binder_count,
		p_refined_context_id,
		p_refinement_substitution_id,
		p_constructor_term
	);
	if (action_status <= 0) {
		return action_status;
	}
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
	uint32_t constructor_field_index,
	uint32_t refinement_substitution_id
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
		.occurrence_id = delta->current_occurrence_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION;
	initialize_proof_rule_parameters(
		&proof,
		proof_premises,
		(struct prototype_judgement_proposition[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		]){{0}}
	);
	proof.rule_data.constructor.owner_view = constructor_owner_view;
	proof.rule_data.constructor.constructor_index = constructor_index;
	proof.rule_data.constructor.field_index = constructor_field_index;
	if (refinement_substitution_id != PROTOTYPE_INVALID_ID) {
		proof.semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
		proof.semantic_action_id = refinement_substitution_id;
	}
	return add_complete_delta_relation(delta, &relation, &proof);
}
