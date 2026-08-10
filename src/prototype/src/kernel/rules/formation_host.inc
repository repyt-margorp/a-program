int prototype_judgement_expand_text_literal(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_TEXT_LITERAL) ||
		!term_has_tag(terms, classifier, PROTOTYPE_TERM_PRIMITIVE_TEXT)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO);
}

int prototype_judgement_expand_int_literal(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_is_primitive_integer(terms, classifier)) {
		return -1;
	}
	if (term_is_primitive_int(terms, classifier) &&
		!int_literal_fits_int32(terms->terms[subject].as.int_literal.value)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO);
}

static int judgement_has_host_type_intro(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject
) {
	if (!judgement) {
		return 0;
	}
	if (!term_is_host_primitive(terms, subject)) {
		return 0;
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			candidate_has_derivation_kind(
				judgement->propositions,
				judgement->proposition_count,
				judgement->derivation_candidates,
				judgement->derivation_candidate_count,
				(uint32_t)i,
				PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO
			) == 1) {
			return 1;
		}
	}
	return 0;
}

static int add_host_type_intro_classifier(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	uint32_t subject
) {
	if (!term_is_host_primitive(terms, subject)) {
		return -1;
	}
	if (!judgement_has_host_type_intro(judgement, terms, subject)) {
		uint32_t universe;
		if (prototype_term_universe_var(
				terms,
				judgement->next_universe_var++,
				&universe
			) != 0 ||
			add_relation(
				judgement,
				0,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				universe,
				PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static void sync_judgement_universe_counter(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms
) {
	if (!judgement || !terms) {
		return;
	}
	uint32_t next_level_var = 0;
	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
			terms->terms[i].as.universe_var.level_var >= next_level_var) {
			next_level_var = terms->terms[i].as.universe_var.level_var + 1;
		}
	}
	if (judgement->next_universe_var < next_level_var) {
		judgement->next_universe_var = next_level_var;
	}
}

int prototype_judgement_expand_primitives(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms
) {
	if (!judgement || !terms) {
		return -1;
	}
	sync_judgement_universe_counter(judgement, terms);
	for (size_t i = 0; i < prototype_term_host_type_count(); ++i) {
		int host_type;
		uint32_t primitive;
		if (prototype_term_host_type_at(i, &host_type) != 0 ||
			prototype_term_make_host_type(terms, host_type, &primitive) != 0) {
			return -1;
		}
		if (add_host_type_intro_classifier(judgement, terms, primitive) != 0) {
			return -1;
		}
	}
	return 0;
}
