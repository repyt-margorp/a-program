hott_bridge_for_source_context_and_semantics(
	const struct prototype_hott_bridge_db* bridges,
	uint32_t source_context,
	int semantics,
	uint32_t* p_bridge_id
) {
	if (!bridges) {
		return NULL;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		if (bridges->bridges[i].source_context_id == source_context &&
			bridges->certificates[i].semantics == semantics) {
			if (p_bridge_id) {
				*p_bridge_id = i;
			}
			return &bridges->bridges[i];
		}
	}
	return NULL;
}

static const struct prototype_judgement_derivation*
hott_constructor_derivation_for_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	if (!judgement) {
		return NULL;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id == claim_id &&
			(derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO ||
			 derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION)) {
			return derivation;
		}
	}
	return NULL;
}

static const struct prototype_judgement_derivation*
hott_derivation_for_claim_and_kind(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	int proof_kind
) {
	if (!judgement) {
		return NULL;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id == claim_id &&
			derivation->proof_kind == proof_kind) {
			return derivation;
		}
	}
	return NULL;
}

static uint32_t hott_is_type_claim_for_subject(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject
) {
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
			prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == context_id && prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == subject) {
			return i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static uint32_t hott_has_type_claim_for_subject(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject
) {
	uint32_t found = PROTOTYPE_INVALID_ID;
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == context_id && prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == subject) {
			if (found != PROTOTYPE_INVALID_ID &&
				prototype_judgement_claim_proposition(
					judgement, found
				)->classifier != prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier) {
				return PROTOTYPE_INVALID_ID;
			}
			found = i;
		}
	}
	return found;
}

static int hott_ensure_is_type_claim_in_context(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t* p_claim_id
);

struct hott_object_action_environment {
	uint32_t left_substitution_id;
	uint32_t right_substitution_id;
	uint32_t target_context_id;
	const uint32_t* source_binding_ids;
	const uint32_t* identity_claim_ids;
	uint32_t binding_count;
};
