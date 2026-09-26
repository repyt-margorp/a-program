static int totality_evidence_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	const struct prototype_judgement_proposition** p_proposition
) {
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	if (!claim || !p_proposition) {
		return -1;
	}
	*p_proposition = prototype_judgement_claim_proposition(judgement, claim_id);
	return *p_proposition ? 0 : -1;
}

static int totality_evidence_publish(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* conclusion,
	int proof_kind,
	const uint32_t* premise_ids,
	const struct prototype_judgement_proposition* const* premises,
	uint32_t premise_count,
	uint32_t* p_claim_id
) {
	if (!judgement || !conclusion || !p_claim_id ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		(premise_count != 0 && (!premise_ids || !premises))) {
		return -1;
	}
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_proposition
		premise_storage[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	memset(premise_storage, 0, sizeof(premise_storage));
	proof.proof_kind = proof_kind;
	initialize_proof_rule_parameters(&proof, proof_premises, premise_storage);
	proof.premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		*proof.premises[i].builder_proposition = *premises[i];
	}
	return publish_complete_relation(
		judgement, conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_terminates_type_formation(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t terminates_type,
	uint32_t universe,
	uint32_t computation_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_proposition* computation;
	uint32_t suspended;
	uint32_t computation_classifier;
	if (totality_evidence_claim(
			judgement, computation_claim_id, &computation
		) != 0 || prototype_term_terminates_type_info(
			terms, terminates_type, &suspended
		) != 0 || computation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		computation->context_id != context_id || computation->subject != suspended ||
		prototype_judgement_classifier_value_whnf(
			terms, type_declarations, computation->classifier,
			&computation_classifier
		) != 0 || computation_classifier >= terms->term_count ||
		terms->terms[computation_classifier].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		universe >= terms->term_count ||
		terms->terms[universe].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return -1;
	}
	const uint32_t premise_ids[1] = { computation_claim_id };
	const struct prototype_judgement_proposition* premises[1] = { computation };
	const struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		.authority_id = terminates_type,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = terminates_type,
		.classifier = universe
	};
	return totality_evidence_publish(
		judgement, &conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TYPE_FORMATION,
		premise_ids, premises, 1, p_claim_id
	);
}

int prototype_judgement_add_terminates_total_computation(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t occurrence_id,
	uint32_t witness,
	uint32_t terminates_type,
	uint32_t terminates_type_claim_id,
	uint32_t computation_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_proposition* type_formation;
	const struct prototype_judgement_proposition* computation;
	uint32_t suspended;
	uint32_t witness_computation;
	if (totality_evidence_claim(
			judgement, terminates_type_claim_id, &type_formation
		) != 0 || totality_evidence_claim(
			judgement, computation_claim_id, &computation
		) != 0 || type_formation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		type_formation->context_id != context_id ||
		type_formation->subject != terminates_type ||
		computation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		computation->context_id != context_id ||
		prototype_term_terminates_type_info(
			terms, terminates_type, &suspended
		) != 0 || prototype_term_terminates_witness_info(
			terms, witness, &witness_computation
		) != 0 || witness_computation != suspended ||
		computation->subject != suspended ||
		computation->classifier >= terms->term_count ||
		terms->terms[computation->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t computation_type =
		terms->terms[computation->classifier].as.thunk_type.computation;
	if (computation_type >= terms->term_count ||
		terms->terms[computation_type].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		terms->terms[computation_type].as.computation_type.totality !=
			PROTOTYPE_COMPUTATION_TOTALITY_TOTAL) {
		return -1;
	}
	const uint32_t premise_ids[2] = {
		terminates_type_claim_id, computation_claim_id
	};
	const struct prototype_judgement_proposition* premises[2] = {
		type_formation, computation
	};
	const struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = occurrence_id == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER :
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE,
		.authority_id = occurrence_id == PROTOTYPE_INVALID_ID ? witness : occurrence_id,
		.context_id = context_id,
		.occurrence_id = occurrence_id,
		.subject = witness,
		.classifier = terminates_type
	};
	return totality_evidence_publish(
		judgement, &conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TOTAL_COMPUTATION,
		premise_ids, premises, 2, p_claim_id
	);
}
