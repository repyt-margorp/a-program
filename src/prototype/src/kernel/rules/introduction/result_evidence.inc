static int result_evidence_claim(
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

static void result_evidence_premise(
	struct prototype_judgement_candidate_premise* premise,
	const struct prototype_judgement_proposition* proposition
) {
	*premise->builder_proposition = *proposition;
}

static int result_evidence_publish(
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
		result_evidence_premise(&proof.premises[i], premises[i]);
	}
	return publish_complete_relation(
		judgement, conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_result_computation_endpoints_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	int* p_equal
) {
	if (!terms || !type_declarations || !p_equal ||
		left >= terms->term_count || right >= terms->term_count) {
		return -1;
	}
	*p_equal = 0;
	if (prototype_term_core_shape_equal_for_link(
			terms, type_declarations, left,
			terms, type_declarations, right, p_equal
		) != 0 || *p_equal) {
		return *p_equal ? 0 : -1;
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			terms, type_declarations, left, right
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		*p_equal = 1;
		return 0;
	}
	*p_equal = 0;
	if (terms->terms[left].tag != PROTOTYPE_TERM_THUNK ||
		terms->terms[right].tag != PROTOTYPE_TERM_THUNK) {
		return 0;
	}
	uint32_t left_result;
	uint32_t right_result;
	int left_status = prototype_term_project_pure_computation_value(
		terms, type_declarations, terms->terms[left].as.thunk.computation,
		step_limit, &left_result
	);
	int right_status = prototype_term_project_pure_computation_value(
		terms, type_declarations, terms->terms[right].as.thunk.computation,
		step_limit, &right_result
	);
	if (left_status != 0 || right_status != 0) {
		return 0;
	}
	return prototype_term_core_shape_equal_for_link(
		terms, type_declarations, left_result,
		terms, type_declarations, right_result, p_equal
	);
}

static int returns_claim_shape(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t returns_type,
	const struct prototype_judgement_proposition* computation,
	const struct prototype_judgement_proposition* value
) {
	uint32_t suspended;
	uint32_t endpoint;
	uint32_t computation_classifier;
	uint32_t computation_type;
	int computation_endpoint_equal = 0;
	int value_endpoint_equal = 0;
	struct prototype_term_classifier_view computation_view;
	if (!terms || !type_declarations || !computation || !value ||
		prototype_term_returns_type_info(
			terms, returns_type, &suspended, &endpoint
		) != 0 || computation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		value->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		computation->context_id != context_id || value->context_id != context_id ||
		prototype_judgement_result_computation_endpoints_equal(
			terms, type_declarations, computation->subject, suspended,
			PROTOTYPE_RESULT_EVIDENCE_REPLAY_STEP_LIMIT,
			&computation_endpoint_equal
		) != 0 || !computation_endpoint_equal ||
		prototype_term_core_shape_equal_for_link(
			terms, type_declarations, value->subject,
			terms, type_declarations, endpoint, &value_endpoint_equal
		) != 0 || !value_endpoint_equal ||
		prototype_judgement_classifier_view(
			terms, type_declarations, NULL, computation->classifier,
			&computation_view
		) != 0 || computation_view.category != PROTOTYPE_TERM_CATEGORY_VALUE ||
		prototype_judgement_classifier_value_whnf(
			terms, type_declarations, computation->classifier,
			&computation_classifier
		) != 0 || computation_classifier >= terms->term_count ||
		terms->terms[computation_classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	if (prototype_judgement_classifier_value_whnf(
			terms,
			type_declarations,
			terms->terms[computation_classifier].as.thunk_type.computation,
			&computation_type
		) != 0 || computation_type >= terms->term_count ||
		terms->terms[computation_type].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	return prototype_judgement_classifier_conversion(
		terms,
		type_declarations,
		terms->terms[computation_type].as.computation_type.result,
		value->classifier
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 0 : -1;
}

int prototype_judgement_add_returns_type_formation(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t returns_type,
	uint32_t universe,
	uint32_t computation_claim_id,
	uint32_t value_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_proposition* computation;
	const struct prototype_judgement_proposition* value;
	if (result_evidence_claim(judgement, computation_claim_id, &computation) != 0 ||
		result_evidence_claim(judgement, value_claim_id, &value) != 0 ||
		returns_claim_shape(
			terms, type_declarations, context_id, returns_type, computation, value
		) != 0 || universe >= terms->term_count ||
		terms->terms[universe].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return -1;
	}
	const uint32_t premise_ids[2] = { computation_claim_id, value_claim_id };
	const struct prototype_judgement_proposition* premises[2] = {
		computation, value
	};
	const struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		.authority_id = returns_type,
		.context_id = context_id,
		.occurrence_id = PROTOTYPE_INVALID_ID,
		.subject = returns_type,
		.classifier = universe
	};
	return result_evidence_publish(
		judgement, &conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_RETURNS_TYPE_FORMATION,
		premise_ids, premises, 2, p_claim_id
	);
}

static int returns_evaluation_is_valid(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t suspended,
	uint32_t expected,
	uint64_t step_limit
) {
	uint32_t computation;
	uint32_t actual;
	if (!terms || !type_declarations || suspended >= terms->term_count ||
		terms->terms[suspended].tag != PROTOTYPE_TERM_THUNK) {
		return -1;
	}
	computation = terms->terms[suspended].as.thunk.computation;
	int projection = prototype_term_project_pure_computation_value(
		terms, type_declarations, computation, step_limit, &actual
	);
	return projection == 0 && prototype_judgement_classifier_conversion(
		terms, type_declarations, actual, expected
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 0 : -1;
}

int prototype_judgement_add_returns_evaluation(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t occurrence_id,
	uint32_t witness,
	uint32_t returns_type,
	uint32_t returns_type_claim_id,
	uint32_t computation_claim_id,
	uint32_t value_claim_id,
	uint64_t step_limit,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_proposition* type_formation;
	const struct prototype_judgement_proposition* computation;
	const struct prototype_judgement_proposition* value;
	uint32_t suspended;
	uint32_t expected;
	uint32_t witness_computation;
	uint32_t witness_value;
	int witness_computation_equal = 0;
	int witness_value_equal = 0;
	if (result_evidence_claim(judgement, returns_type_claim_id, &type_formation) != 0 ||
		result_evidence_claim(judgement, computation_claim_id, &computation) != 0 ||
		result_evidence_claim(judgement, value_claim_id, &value) != 0 ||
		type_formation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		type_formation->context_id != context_id ||
		type_formation->subject != returns_type ||
		returns_claim_shape(
			terms, type_declarations, context_id, returns_type, computation, value
		) != 0 || prototype_term_returns_type_info(
			terms, returns_type, &suspended, &expected
		) != 0 || prototype_term_returns_witness_info(
			terms, witness, &witness_computation, &witness_value
		) != 0 || prototype_judgement_result_computation_endpoints_equal(
			terms, type_declarations, witness_computation, suspended,
			step_limit, &witness_computation_equal
		) != 0 || !witness_computation_equal ||
		prototype_term_core_shape_equal_for_link(
			terms, type_declarations, witness_value,
			terms, type_declarations, expected, &witness_value_equal
		) != 0 || !witness_value_equal ||
		returns_evaluation_is_valid(
			terms, type_declarations, suspended, expected, step_limit
		) != 0) {
		return -1;
	}
	const uint32_t premise_ids[3] = {
		returns_type_claim_id, computation_claim_id, value_claim_id
	};
	const struct prototype_judgement_proposition* premises[3] = {
		type_formation, computation, value
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
		.classifier = returns_type
	};
	return result_evidence_publish(
		judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_RETURNS_EVALUATION,
		premise_ids, premises, 3, p_claim_id
	);
}

static int returns_sequence_binding_is_valid(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t suspended,
	uint32_t expected
) {
	uint32_t binding_id;
	uint32_t entry_context_id;
	int computation_equal = 0;
	if (!terms || !type_declarations || !contexts ||
		suspended >= terms->term_count || expected >= terms->term_count ||
		terms->terms[suspended].tag != PROTOTYPE_TERM_THUNK ||
		terms->terms[expected].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	binding_id = terms->terms[expected].as.var.binding_id;
	if (prototype_context_find_binding(
			contexts, context_id, binding_id, &entry_context_id
		) != 0) {
		return -1;
	}
	const struct prototype_context* entry = prototype_context_get(
		contexts, entry_context_id
	);
	if (!entry || entry->extension_kind !=
			PROTOTYPE_CONTEXT_EXTENSION_COMPUTATION_RESULT ||
		entry->source_computation == PROTOTYPE_INVALID_ID ||
		prototype_term_core_shape_equal_for_link(
			terms,
			type_declarations,
			entry->source_computation,
			terms,
			type_declarations,
			terms->terms[suspended].as.thunk.computation,
			&computation_equal
		) != 0) {
		return -1;
	}
	if (!computation_equal) {
		struct prototype_term_conversion_result conversion =
			prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				entry->source_computation,
				terms->terms[suspended].as.thunk.computation
			);
		if (conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
	}
	return 0;
}

int prototype_judgement_add_returns_sequence_binding(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t occurrence_id,
	uint32_t witness,
	uint32_t returns_type,
	uint32_t returns_type_claim_id,
	uint32_t computation_claim_id,
	uint32_t value_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_proposition* type_formation;
	const struct prototype_judgement_proposition* computation;
	const struct prototype_judgement_proposition* value;
	uint32_t suspended;
	uint32_t expected;
	uint32_t witness_computation;
	uint32_t witness_value;
	int witness_computation_equal = 0;
	int witness_value_equal = 0;
	if (result_evidence_claim(judgement, returns_type_claim_id, &type_formation) != 0 ||
		result_evidence_claim(judgement, computation_claim_id, &computation) != 0 ||
		result_evidence_claim(judgement, value_claim_id, &value) != 0 ||
		type_formation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		type_formation->context_id != context_id ||
		type_formation->subject != returns_type ||
		returns_claim_shape(
			terms, type_declarations, context_id, returns_type, computation, value
		) != 0 || prototype_term_returns_type_info(
			terms, returns_type, &suspended, &expected
		) != 0 || prototype_term_returns_witness_info(
			terms, witness, &witness_computation, &witness_value
		) != 0 || prototype_judgement_result_computation_endpoints_equal(
			terms, type_declarations, witness_computation, suspended,
			PROTOTYPE_RESULT_EVIDENCE_REPLAY_STEP_LIMIT,
			&witness_computation_equal
		) != 0 || !witness_computation_equal ||
		prototype_term_core_shape_equal_for_link(
			terms, type_declarations, witness_value,
			terms, type_declarations, expected, &witness_value_equal
		) != 0 || !witness_value_equal ||
		returns_sequence_binding_is_valid(
			terms, type_declarations, contexts, context_id, suspended, expected
		) != 0) {
		return -1;
	}
	const uint32_t premise_ids[3] = {
		returns_type_claim_id, computation_claim_id, value_claim_id
	};
	const struct prototype_judgement_proposition* premises[3] = {
		type_formation, computation, value
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
		.classifier = returns_type
	};
	return result_evidence_publish(
		judgement, &conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_RETURNS_SEQUENCE_BINDING,
		premise_ids, premises, 3, p_claim_id
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
	if (result_evidence_claim(judgement, computation_claim_id, &computation) != 0 ||
		prototype_term_terminates_type_info(
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
	return result_evidence_publish(
		judgement, &conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TYPE_FORMATION,
		premise_ids, premises, 1, p_claim_id
	);
}

int prototype_judgement_add_terminates_from_returns(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t occurrence_id,
	uint32_t witness,
	uint32_t terminates_type,
	uint32_t terminates_type_claim_id,
	uint32_t returns_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_proposition* type_formation;
	const struct prototype_judgement_proposition* returns;
	uint32_t suspended;
	uint32_t witness_computation;
	uint32_t returns_computation;
	uint32_t returns_value;
	if (result_evidence_claim(judgement, terminates_type_claim_id, &type_formation) != 0 ||
		result_evidence_claim(judgement, returns_claim_id, &returns) != 0 ||
		type_formation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		type_formation->context_id != context_id ||
		type_formation->subject != terminates_type ||
		returns->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		returns->context_id != context_id ||
		prototype_term_terminates_type_info(
			terms, terminates_type, &suspended
		) != 0 || prototype_term_terminates_witness_info(
			terms, witness, &witness_computation
		) != 0 || witness_computation != suspended ||
		prototype_term_returns_type_info(
			terms, returns->classifier, &returns_computation, &returns_value
		) != 0 || returns_computation != suspended) {
		return -1;
	}
	(void)returns_value;
	const uint32_t premise_ids[2] = {
		terminates_type_claim_id, returns_claim_id
	};
	const struct prototype_judgement_proposition* premises[2] = {
		type_formation, returns
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
	return result_evidence_publish(
		judgement, &conclusion,
		PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_FROM_RETURNS,
		premise_ids, premises, 2, p_claim_id
	);
}
