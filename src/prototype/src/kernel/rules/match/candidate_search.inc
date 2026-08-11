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
