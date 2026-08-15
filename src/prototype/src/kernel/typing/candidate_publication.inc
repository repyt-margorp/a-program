static int proof_candidates_equal(
	const struct prototype_judgement_derivation_candidate* left,
	const struct prototype_judgement_derivation_candidate* right
) {
	if (!left || !right || left->proof_kind != right->proof_kind ||
		left->conclusion_proposition_id !=
			right->conclusion_proposition_id ||
		left->conclusion_kind != right->conclusion_kind ||
		left->conclusion_context_id != right->conclusion_context_id ||
		left->conclusion_operation_id != right->conclusion_operation_id ||
		left->conclusion_subject != right->conclusion_subject ||
		left->conclusion_classifier != right->conclusion_classifier ||
		memcmp(&left->rule_data, &right->rule_data, sizeof(left->rule_data)) != 0 ||
		left->semantic_action_kind != right->semantic_action_kind ||
		left->semantic_action_id != right->semantic_action_id ||
		left->premise_count != right->premise_count) {
		return 0;
	}
	for (uint32_t i = 0; i < left->premise_count; ++i) {
		if (left->premises[i].proposition.kind != right->premises[i].proposition.kind ||
			left->premises[i].proposition.context_id != right->premises[i].proposition.context_id ||
			left->premises[i].proposition.subject != right->premises[i].proposition.subject ||
			left->premises[i].proposition.classifier != right->premises[i].proposition.classifier ||
			left->premises[i].proposition.authority_kind !=
				right->premises[i].proposition.authority_kind ||
			left->premises[i].proposition.authority_id !=
				right->premises[i].proposition.authority_id ||
			left->premises[i].proposition.operation_id !=
				right->premises[i].proposition.operation_id ||
			left->premises[i].semantic_action_kind !=
				right->premises[i].semantic_action_kind ||
			left->premises[i].semantic_action_id !=
				right->premises[i].semantic_action_id) {
			return 0;
		}
	}
	return 1;
}

int prototype_judgement_candidate_derivation_next(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	uint32_t* p_cursor,
	uint32_t* p_derivation_id
) {
	if (!claims || !derivations || !p_cursor || !p_derivation_id ||
		claim_id >= claim_count || *p_cursor > derivation_count) {
		return -1;
	}
	for (uint32_t i = *p_cursor; i < (uint32_t)derivation_count; ++i) {
		if (derivations[i].proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID ||
			derivations[i].conclusion_proposition_id != claim_id) {
			continue;
		}
		*p_cursor = i + 1;
		*p_derivation_id = i;
		return 0;
	}
	*p_cursor = (uint32_t)derivation_count;
	return 1;
}

int prototype_judgement_candidate_find_derivation_kind(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	int proof_kind,
	uint32_t* p_derivation_id
) {
	if (!p_derivation_id ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
		return -1;
	}
	uint32_t cursor = 0;
	uint32_t derivation_id;
	int status;
	while ((status = prototype_judgement_candidate_derivation_next(
		claims,
		claim_count,
		derivations,
		derivation_count,
		claim_id,
		&cursor,
		&derivation_id
	)) == 0) {
		if (derivations[derivation_id].proof_kind == proof_kind) {
			*p_derivation_id = derivation_id;
			return 0;
		}
	}
	return status < 0 ? -1 : 1;
}

static int candidate_has_derivation_kind(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	int proof_kind
) {
	uint32_t derivation_id;
	int status = prototype_judgement_candidate_find_derivation_kind(
		claims,
		claim_count,
		derivations,
		derivation_count,
		claim_id,
		proof_kind,
		&derivation_id
	);
	return status == 0 ? 1 : (status > 0 ? 0 : -1);
}

static int candidate_has_derivation_other_than(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	int excluded_proof_kind
) {
	uint32_t derivation_id;
	int status = prototype_judgement_candidate_find_derivation_other_than(
		claims,
		claim_count,
		derivations,
		derivation_count,
		claim_id,
		excluded_proof_kind,
		&derivation_id
	);
	return status == 0 ? 1 : (status > 0 ? 0 : -1);
}

int prototype_judgement_candidate_find_derivation_other_than(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	int excluded_proof_kind,
	uint32_t* p_derivation_id
) {
	if (!p_derivation_id) {
		return -1;
	}
	uint32_t cursor = 0;
	uint32_t derivation_id;
	int status;
	while ((status = prototype_judgement_candidate_derivation_next(
		claims,
		claim_count,
		derivations,
		derivation_count,
		claim_id,
		&cursor,
		&derivation_id
	)) == 0) {
		if (derivations[derivation_id].proof_kind != excluded_proof_kind) {
			*p_derivation_id = derivation_id;
			return 0;
		}
	}
	return status < 0 ? -1 : 1;
}

static int propositions_equal(
	const struct prototype_judgement_proposition* left,
	const struct prototype_judgement_proposition* right
) {
	if (!left || !right) {
		return 0;
	}
	int left_authority_kind;
	int right_authority_kind;
	uint32_t left_authority_id;
	uint32_t right_authority_id;
	candidate_claim_authority(
		left, &left_authority_kind, &left_authority_id
	);
	candidate_claim_authority(
		right, &right_authority_kind, &right_authority_id
	);
	return left->kind == right->kind &&
		left_authority_kind == right_authority_kind &&
		left_authority_id == right_authority_id &&
		left->context_id == right->context_id &&
		left->operation_id == right->operation_id &&
		left->subject == right->subject &&
		left->classifier == right->classifier &&
		left->resource_usage_count == right->resource_usage_count &&
		prototype_usage_vector_equal(
			&(struct prototype_usage_vector) {
				.entries = (struct prototype_usage_entry*)left->resource_usage,
				.count = left->resource_usage_count,
				.capacity = left->resource_usage_count
			},
			&(struct prototype_usage_vector) {
				.entries = (struct prototype_usage_entry*)right->resource_usage,
				.count = right->resource_usage_count,
				.capacity = right->resource_usage_count
			}
		);
}

static void candidate_claim_authority(
	const struct prototype_judgement_proposition* relation,
	int* p_authority_kind,
	uint32_t* p_authority_id
) {
	if (relation->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
		*p_authority_kind = relation->authority_kind;
		*p_authority_id = relation->authority_id;
		return;
	}
	if (relation->operation_id != PROTOTYPE_INVALID_ID) {
		*p_authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION;
		*p_authority_id = relation->operation_id;
		return;
	}
	*p_authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID;
	*p_authority_id = PROTOTYPE_INVALID_ID;
}

static void initialize_candidate_claim_authority(
	struct prototype_judgement_proposition* candidate,
	int proof_kind
) {
	if (!candidate ||
		candidate->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
		return;
	}
	if (candidate->operation_id != PROTOTYPE_INVALID_ID) {
		candidate->authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION;
		candidate->authority_id = candidate->operation_id;
		return;
	}
	switch (proof_kind) {
	case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
	case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
		candidate->authority_kind =
			PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING;
		break;
	case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION:
	case PROTOTYPE_JUDGEMENT_PROOF_DECLARATION:
		candidate->authority_kind =
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION;
		break;
	case PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO:
		candidate->authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC;
		break;
	case PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION:
	case PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION:
		candidate->authority_kind =
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION;
		break;
	case PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY:
		candidate->authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE;
		break;
	default:
		candidate->authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
		break;
	}
	candidate->authority_id = candidate->subject;
}

static int judgement_intern_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	uint32_t* p_claim_id
) {
	int authority_kind;
	uint32_t authority_id;
	candidate_claim_authority(relation, &authority_kind, &authority_id);
	uint32_t operation_id = authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
		relation->operation_id : PROTOTYPE_INVALID_ID;
	struct prototype_judgement_proposition proposition = *relation;
	proposition.authority_kind = authority_kind;
	proposition.authority_id = authority_id;
	proposition.operation_id = operation_id;
	uint32_t proposition_id;
	if (prototype_judgement_proposition_intern(
			judgement, &proposition, &proposition_id
		) != 0) {
		return -1;
	}
	return prototype_judgement_claim_intern_exact(
		judgement, proposition_id, p_claim_id
	);
}

int prototype_judgement_claim_intern_exact(
	struct prototype_judgement_db* judgement,
	uint32_t proposition_id,
	uint32_t* p_claim_id
) {
	if (!judgement || !p_claim_id ||
		!prototype_judgement_proposition_get(judgement, proposition_id)) {
		return -1;
	}
	struct prototype_judgement_claim identity = {
		.proposition_id = proposition_id,
		.closure_rank = PROTOTYPE_INVALID_ID
	};
	uint64_t key_hash = judgement_claim_key_hash(&identity);
	size_t bucket = key_hash % PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
	judgement->claim_intern_requests++;
	for (uint32_t i = judgement->claim_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = judgement->claims[i].hash_next) {
		judgement->claim_intern_probes++;
		if (i >= judgement->claim_count) {
			return -1;
		}
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (claim->key_hash == key_hash &&
			judgement_claim_identity_equal(claim, &identity)) {
			judgement->claim_intern_hits++;
			*p_claim_id = i;
			return 0;
		}
	}
	if (reserve_slot(judgement->claim_count, judgement->claim_capacity) != 0) {
		return -1;
	}
	uint32_t claim_id = (uint32_t)judgement->claim_count++;
	identity.key_hash = key_hash;
	identity.hash_next = judgement->claim_index_heads[bucket];
	judgement->claims[claim_id] = identity;
	judgement->claim_index_heads[bucket] = claim_id;
	*p_claim_id = claim_id;
	return 0;
}

static int derivations_equal(
	const struct prototype_judgement_derivation* left,
	const struct prototype_judgement_derivation* right
) {
	if (left->proof_kind != right->proof_kind ||
		left->conclusion_claim_id != right->conclusion_claim_id ||
		memcmp(&left->rule_data, &right->rule_data, sizeof(left->rule_data)) != 0 ||
		left->semantic_action_kind != right->semantic_action_kind ||
		left->semantic_action_id != right->semantic_action_id ||
		left->premise_count != right->premise_count) {
		return 0;
	}
	for (uint32_t i = 0; i < left->premise_count; ++i) {
		if (left->premises[i].claim_id != right->premises[i].claim_id ||
			left->premises[i].scoped_proposition_id !=
				right->premises[i].scoped_proposition_id ||
			left->premises[i].semantic_action_kind !=
				right->premises[i].semantic_action_kind ||
			left->premises[i].semantic_action_id !=
				right->premises[i].semantic_action_id) {
			return 0;
		}
	}
	return 1;
}

int prototype_judgement_db_rebuild_index(
	struct prototype_judgement_db* judgement
) {
	if (!judgement ||
		(judgement->claim_count != 0 && !judgement->claims) ||
		(judgement->derivation_count != 0 && !judgement->derivations) ||
		judgement->proposition_count > judgement->proposition_capacity ||
		judgement->derivation_candidate_count >
			judgement->derivation_candidate_capacity ||
		judgement->claim_count > judgement->claim_capacity ||
		judgement->derivation_count > judgement->derivation_capacity) {
		return -1;
	}
	judgement_index_clear(judgement->claim_index_heads);
	judgement_index_clear(judgement->proposition_index_heads);
	judgement_index_clear(judgement->derivation_index_heads);
	for (uint32_t i = 0;
		i < (uint32_t)judgement->proposition_count; ++i) {
		struct prototype_judgement_proposition* proposition =
			&judgement->propositions[i];
		if (proposition->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			proposition->key_hash = 0;
			proposition->hash_next = PROTOTYPE_INVALID_ID;
			continue;
		}
		proposition->key_hash = judgement_proposition_key_hash(proposition);
		size_t bucket = proposition->key_hash %
			PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
		for (uint32_t existing = judgement->proposition_index_heads[bucket];
			existing != PROTOTYPE_INVALID_ID;
			existing = judgement->propositions[existing].hash_next) {
			if (judgement->propositions[existing].key_hash ==
					proposition->key_hash && judgement_proposition_equal(
					&judgement->propositions[existing], proposition
				)) {
				return -1;
			}
		}
		proposition->hash_next = judgement->proposition_index_heads[bucket];
		judgement->proposition_index_heads[bucket] = i;
	}
	for (uint32_t i = 0;
		i < (uint32_t)judgement->derivation_candidate_count; ++i) {
		if (judgement->derivation_candidates[i].proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		if (!prototype_judgement_proposition_get(
				judgement,
				judgement->derivation_candidates[i].conclusion_proposition_id
			)) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (!prototype_judgement_proposition_get(
				judgement, claim->proposition_id
			)) {
			claim->key_hash = 0;
			claim->hash_next = PROTOTYPE_INVALID_ID;
			continue;
		}
		claim->hash_next = PROTOTYPE_INVALID_ID;
		claim->key_hash = judgement_claim_key_hash(claim);
		size_t bucket = claim->key_hash %
			PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
		for (uint32_t existing = judgement->claim_index_heads[bucket];
			existing != PROTOTYPE_INVALID_ID;
			existing = judgement->claims[existing].hash_next) {
			if (judgement->claims[existing].key_hash == claim->key_hash &&
				judgement_claim_identity_equal(
					&judgement->claims[existing], claim
				)) {
				return -1;
			}
		}
		claim->hash_next = judgement->claim_index_heads[bucket];
		judgement->claim_index_heads[bucket] = i;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->derivation_count; ++i) {
		struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		derivation->hash_next = PROTOTYPE_INVALID_ID;
		if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			derivation->key_hash = 0;
			continue;
		}
		if (!prototype_judgement_claim_get(
				judgement, derivation->conclusion_claim_id
			) || derivation->premise_count >
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
			(derivation->premise_count != 0 && !derivation->premises)) {
			return -1;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			const struct prototype_judgement_premise_edge* premise =
				&derivation->premises[j];
			if ((premise->claim_id == PROTOTYPE_INVALID_ID) ==
					(premise->scoped_proposition_id == PROTOTYPE_INVALID_ID) ||
				(premise->claim_id != PROTOTYPE_INVALID_ID &&
				 !prototype_judgement_claim_get(
					 judgement, premise->claim_id
				 )) ||
				(premise->scoped_proposition_id != PROTOTYPE_INVALID_ID &&
				 !prototype_judgement_proposition_get(
					 judgement, premise->scoped_proposition_id
				 ))) {
				return -1;
			}
		}
		derivation->key_hash = judgement_derivation_key_hash(derivation);
		size_t bucket = derivation->key_hash %
			PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
		for (uint32_t existing = judgement->derivation_index_heads[bucket];
			existing != PROTOTYPE_INVALID_ID;
			existing = judgement->derivations[existing].hash_next) {
			if (judgement->derivations[existing].key_hash ==
					derivation->key_hash && derivations_equal(
					&judgement->derivations[existing], derivation
				)) {
				return -1;
			}
		}
		derivation->hash_next = judgement->derivation_index_heads[bucket];
		judgement->derivation_index_heads[bucket] = i;
	}
	return 0;
}

int prototype_judgement_find_exact_derivation(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation* identity,
	uint32_t* p_derivation_id
) {
	if (!judgement || !identity || !p_derivation_id) {
		return -1;
	}
	uint64_t key_hash = judgement_derivation_key_hash(identity);
	size_t bucket = key_hash % PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
	for (uint32_t i = judgement->derivation_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = judgement->derivations[i].hash_next) {
		if (i >= judgement->derivation_count) {
			return -1;
		}
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->key_hash == key_hash &&
			derivations_equal(derivation, identity)) {
			*p_derivation_id = i;
			return 0;
		}
	}
	return 1;
}

int prototype_judgement_derivation_intern_exact(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation* identity,
	uint32_t* p_derivation_id
) {
	if (!judgement || !identity || !p_derivation_id ||
		(identity->premise_count != 0 && !identity->premises)) {
		return -1;
	}
	for (uint32_t i = 0; i < identity->premise_count; ++i) {
		const struct prototype_judgement_premise_edge* premise =
			&identity->premises[i];
		if ((premise->claim_id == PROTOTYPE_INVALID_ID) ==
				(premise->scoped_proposition_id == PROTOTYPE_INVALID_ID) ||
			(premise->claim_id != PROTOTYPE_INVALID_ID &&
				premise->claim_id >= judgement->claim_count) ||
			(premise->scoped_proposition_id != PROTOTYPE_INVALID_ID &&
				!prototype_judgement_proposition_get(
					judgement, premise->scoped_proposition_id
				))) {
			return -1;
		}
	}
	judgement->derivation_intern_requests++;
	uint64_t key_hash = judgement_derivation_key_hash(identity);
	size_t bucket = key_hash % PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
	for (uint32_t i = judgement->derivation_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = judgement->derivations[i].hash_next) {
		judgement->derivation_intern_probes++;
		if (i >= judgement->derivation_count) {
			return -1;
		}
		if (judgement->derivations[i].key_hash == key_hash &&
			derivations_equal(&judgement->derivations[i], identity)) {
			judgement->derivation_intern_hits++;
			judgement->accepted_premise_reuses += identity->premise_count;
			*p_derivation_id = i;
			return 0;
		}
	}
	if (reserve_slot(
			judgement->derivation_count, judgement->derivation_capacity
		) != 0 || identity->premise_count >
			judgement->accepted_premise_capacity -
			judgement->accepted_premise_count) {
		return -1;
	}
	struct prototype_judgement_premise_edge* premises =
		identity->premise_count == 0 ? NULL :
		&judgement->accepted_premises[judgement->accepted_premise_count];
	if (identity->premise_count != 0) {
		memcpy(
			premises,
			identity->premises,
			identity->premise_count * sizeof(*premises)
		);
	}
	judgement->accepted_premise_count += identity->premise_count;
	judgement->accepted_premise_allocations += identity->premise_count;
	uint32_t id = (uint32_t)judgement->derivation_count++;
	struct prototype_judgement_derivation derivation = *identity;
	derivation.key_hash = key_hash;
	derivation.hash_next = judgement->derivation_index_heads[bucket];
	judgement->derivations[id] = derivation;
	judgement->derivations[id].premises = premises;
	judgement->derivation_index_heads[bucket] = id;
	*p_derivation_id = id;
	return 0;
}

static int add_complete_candidate(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* candidate_relation,
	const struct prototype_judgement_derivation_candidate* candidate_proof
) {
	if (!judgement || !candidate_relation || !candidate_proof ||
		(candidate_proof->premise_count != 0 && !candidate_proof->premises) ||
		candidate_proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		candidate_proof->conclusion_kind != candidate_relation->kind ||
		candidate_proof->conclusion_context_id != candidate_relation->context_id ||
		candidate_proof->conclusion_operation_id != candidate_relation->operation_id ||
		candidate_proof->conclusion_subject != candidate_relation->subject ||
		candidate_proof->conclusion_classifier != candidate_relation->classifier) {
		return -1;
	}
	struct prototype_judgement_proposition normalized_relation =
		*candidate_relation;
	initialize_candidate_claim_authority(
		&normalized_relation, candidate_proof->proof_kind
	);
	uint32_t relation_id;
	if (prototype_judgement_proposition_intern(
			judgement, &normalized_relation, &relation_id
		) != 0) {
		return -1;
	}
	struct prototype_judgement_derivation_candidate proof = *candidate_proof;
	proof.conclusion_proposition_id = relation_id;
	for (size_t i = 0; i < judgement->derivation_candidate_count; ++i) {
		if (proof_candidates_equal(&judgement->derivation_candidates[i], &proof)) {
			return 0;
		}
	}
	if (reserve_slot(
			judgement->derivation_candidate_count,
			judgement->derivation_candidate_capacity
		) != 0 || candidate_proof->premise_count >
			judgement->candidate_premise_capacity -
			judgement->candidate_premise_count) {
		return -1;
	}
	uint32_t proof_id = (uint32_t)judgement->derivation_candidate_count;
	struct prototype_judgement_candidate_premise* premises =
		proof.premise_count == 0 ? NULL :
		&judgement->candidate_premises[judgement->candidate_premise_count];
	if (proof.premise_count != 0) {
		memcpy(
			premises,
			proof.premises,
			proof.premise_count * sizeof(*premises)
		);
	}
	judgement->candidate_premise_count += proof.premise_count;
	judgement->candidate_premise_allocations += proof.premise_count;
	judgement->derivation_candidates[proof_id] = proof;
	judgement->derivation_candidates[proof_id].premises = premises;
	judgement->derivation_candidate_count++;
	return 0;
}

static int add_complete_relation(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* candidate_relation,
	const struct prototype_judgement_derivation_candidate* candidate_proof
) {
	return add_complete_candidate(
		judgement, candidate_relation, candidate_proof
	);
}

static int publish_complete_relation(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* candidate_relation,
	const struct prototype_judgement_derivation_candidate* candidate_proof,
	const uint32_t* premise_claim_ids,
	uint32_t* p_claim_id
) {
	if (!judgement || !candidate_relation || !candidate_proof || !p_claim_id ||
		(candidate_proof->premise_count != 0 && !premise_claim_ids) ||
		judgement->claim_count >= judgement->claim_capacity ||
		judgement->derivation_count >= judgement->derivation_capacity ||
		judgement->derivation_candidate_count >=
			judgement->derivation_candidate_capacity ||
		candidate_proof->premise_count >
			judgement->candidate_premise_capacity -
			judgement->candidate_premise_count ||
		candidate_proof->premise_count >
			judgement->accepted_premise_capacity -
			judgement->accepted_premise_count) {
		return -1;
	}
	for (uint32_t i = 0; i < candidate_proof->premise_count; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_claim_ids[i]);
		if (!premise || !propositions_equal(
				prototype_judgement_proposition_get(
					judgement, premise->proposition_id
				),
				&candidate_proof->premises[i].proposition
			)) {
			return -1;
		}
	}
	size_t derivation_candidate_mark = judgement->derivation_candidate_count;
	size_t candidate_premise_mark = judgement->candidate_premise_count;
	if (add_complete_candidate(
			judgement, candidate_relation, candidate_proof
		) != 0) {
		return -1;
	}
	struct prototype_judgement_proposition relation = *candidate_relation;
	initialize_candidate_claim_authority(&relation, candidate_proof->proof_kind);
	size_t claim_mark = judgement->claim_count;
	uint32_t claim_id;
	if (judgement_intern_claim(judgement, &relation, &claim_id) != 0) {
		judgement->derivation_candidate_count = derivation_candidate_mark;
		judgement->candidate_premise_count = candidate_premise_mark;
		return -1;
	}
	struct prototype_judgement_derivation derivation;
	struct prototype_judgement_premise_edge
		premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&derivation, 0, sizeof(derivation));
	memset(premises, 0, sizeof(premises));
	derivation.premises = premises;
	derivation.proof_kind = candidate_proof->proof_kind;
	derivation.conclusion_claim_id = claim_id;
	derivation.closure_rank = PROTOTYPE_INVALID_ID;
	derivation.rule_data = candidate_proof->rule_data;
	derivation.semantic_action_kind = candidate_proof->semantic_action_kind;
	derivation.semantic_action_id = candidate_proof->semantic_action_id;
	derivation.premise_count = candidate_proof->premise_count;
	for (uint32_t i = 0; i < candidate_proof->premise_count; ++i) {
		derivation.premises[i].claim_id = premise_claim_ids[i];
		derivation.premises[i].scoped_proposition_id = PROTOTYPE_INVALID_ID;
		derivation.premises[i].semantic_action_kind =
			candidate_proof->premises[i].semantic_action_kind;
		derivation.premises[i].semantic_action_id =
			candidate_proof->premises[i].semantic_action_id;
	}
	uint32_t derivation_id;
	if (prototype_judgement_derivation_intern_exact(
			judgement, &derivation, &derivation_id
		) != 0) {
		judgement->derivation_candidate_count = derivation_candidate_mark;
		judgement->candidate_premise_count = candidate_premise_mark;
		if (judgement->claim_count == claim_mark + 1 && claim_id == claim_mark) {
			size_t bucket = judgement->claims[claim_id].key_hash %
				PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
			judgement->claim_index_heads[bucket] =
				judgement->claims[claim_id].hash_next;
			judgement->claim_count = claim_mark;
		}
		return -1;
	}
	*p_claim_id = claim_id;
	return 0;
}

static int accepted_claim_for_candidate_proposition(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* candidate,
	uint32_t* p_claim_id
) {
	if (!judgement || !candidate || !p_claim_id) {
		return -1;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* accepted =
			prototype_judgement_claim_proposition(judgement, i);
		if (accepted && propositions_equal(accepted, candidate)) {
			*p_claim_id = i;
			return 0;
		}
	}
	return 1;
}

int prototype_judgement_delta_publish_complete(
	struct prototype_judgement_delta* delta
) {
	if (!delta || !delta->db ||
		delta->derivation_candidate_count > delta->db->claim_capacity -
			delta->db->claim_count ||
		delta->derivation_candidate_count > delta->db->derivation_capacity -
			delta->db->derivation_count ||
		delta->derivation_candidate_count >
			delta->db->derivation_candidate_capacity -
			delta->db->derivation_candidate_count ||
		delta->candidate_premise_count > delta->db->candidate_premise_capacity -
			delta->db->candidate_premise_count ||
		delta->candidate_premise_count > delta->db->accepted_premise_capacity -
			delta->db->accepted_premise_count) {
		return -1;
	}
	unsigned char* published = calloc(
		delta->derivation_candidate_count == 0 ? 1 :
			delta->derivation_candidate_count,
		sizeof(*published)
	);
	if (!published) {
		return -1;
	}
	size_t published_count = 0;
	int status = 0;
	while (published_count < delta->derivation_candidate_count) {
		int changed = 0;
		for (uint32_t i = 0; i < delta->derivation_candidate_count; ++i) {
			if (published[i]) {
				continue;
			}
			const struct prototype_judgement_derivation_candidate* derivation =
				&delta->derivation_candidates[i];
			if (derivation->conclusion_proposition_id >= delta->proposition_count) {
				status = -1;
				goto done;
			}
			uint32_t premise_claim_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			int ready = 1;
			for (uint32_t j = 0; j < derivation->premise_count; ++j) {
				int found = accepted_claim_for_candidate_proposition(
					delta->db,
					&derivation->premises[j].proposition,
					&premise_claim_ids[j]
				);
				if (found < 0) {
					status = -1;
					goto done;
				}
				if (found > 0) {
					ready = 0;
					break;
				}
			}
			if (!ready) {
				continue;
			}
			uint32_t claim_id;
			if (publish_complete_relation(
					delta->db,
					&delta->propositions[derivation->conclusion_proposition_id],
					derivation,
					premise_claim_ids,
					&claim_id
				) != 0) {
				status = -1;
				goto done;
			}
			published[i] = 1;
			published_count++;
			changed = 1;
		}
		if (!changed) {
			for (uint32_t i = 0; i < delta->derivation_candidate_count; ++i) {
				if (published[i]) {
					continue;
				}
				const struct prototype_judgement_derivation_candidate* derivation =
					&delta->derivation_candidates[i];
				for (uint32_t j = 0; j < derivation->premise_count; ++j) {
					uint32_t ignored_claim_id;
					if (accepted_claim_for_candidate_proposition(
							delta->db,
							&derivation->premises[j].proposition,
							&ignored_claim_id
						) > 0) {
						fprintf(
							stderr,
							"complete derivation blocked rule=%d subject=%u "
							"classifier=%u premise=%u:%u context=%u authority=%d:%u "
							"operation=%u\n",
							derivation->proof_kind,
							derivation->conclusion_subject,
							derivation->conclusion_classifier,
							derivation->premises[j].proposition.subject,
							derivation->premises[j].proposition.classifier,
							derivation->premises[j].proposition.context_id,
							derivation->premises[j].proposition.authority_kind,
							derivation->premises[j].proposition.authority_id,
							derivation->premises[j].proposition.operation_id
						);
						break;
					}
				}
				break;
			}
			status = -1;
			goto done;
		}
	}
	delta->proposition_count = 0;
	delta->derivation_candidate_count = 0;
	delta->candidate_premise_count = 0;

done:
	free(published);
	return status;
}

static int add_relation_with_premises(
	struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t operation_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
) {
	if (!judgement ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = kind,
		.context_id = context_id,
		.operation_id = operation_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	proof.conclusion_kind = kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = operation_id;
	proof.conclusion_subject = subject;
	proof.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	initialize_proof_premises(
		&proof,
		premise_context_ids,
		premise_subjects,
		premise_classifiers,
		premise_count
	);
	return add_complete_relation(judgement, &relation, &proof);
}

static int add_relation(
	struct prototype_judgement_db* judgement,
	uint32_t context_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind
) {
	return add_relation_with_premises(
		judgement,
		context_id,
		PROTOTYPE_INVALID_ID,
		kind,
		subject,
		classifier,
		proof_kind,
		NULL,
		NULL,
		NULL,
		0
	);
}

static int add_complete_delta_relation(
	struct prototype_judgement_delta* delta,
	const struct prototype_judgement_proposition* candidate_relation,
	const struct prototype_judgement_derivation_candidate* candidate_proof
) {
	if (!delta || !candidate_relation || !candidate_proof ||
		(candidate_proof->premise_count != 0 && !candidate_proof->premises) ||
		candidate_proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		candidate_proof->conclusion_kind != candidate_relation->kind ||
		candidate_proof->conclusion_context_id != candidate_relation->context_id ||
		candidate_proof->conclusion_operation_id != candidate_relation->operation_id ||
		candidate_proof->conclusion_subject != candidate_relation->subject ||
		candidate_proof->conclusion_classifier != candidate_relation->classifier) {
		return -1;
	}
	if (candidate_proof->proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX &&
		candidate_proof->semantic_action_kind ==
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
		const struct prototype_substitution* action = prototype_substitution_get(
			delta->substitutions, candidate_proof->semantic_action_id
		);
		if (!action || (action->kind == PROTOTYPE_SUBSTITUTION_IDENTITY &&
			(candidate_proof->premise_count != 1 ||
			 candidate_proof->premises[0].proposition.context_id !=
				candidate_relation->context_id ||
			 candidate_proof->premises[0].proposition.subject !=
				candidate_relation->subject ||
			 candidate_proof->premises[0].proposition.classifier !=
				candidate_relation->classifier))) {
			return -1;
		}
	}
	struct prototype_judgement_proposition normalized_relation =
		*candidate_relation;
	if (normalized_relation.operation_id != PROTOTYPE_INVALID_ID &&
		delta->operation_usage_provider && delta->operation_usage_provider(
			delta->operation_usage_provider_context,
			normalized_relation.operation_id,
			&normalized_relation.resource_usage,
			&normalized_relation.resource_usage_count
		) != 0) {
		return -1;
	}
	initialize_candidate_claim_authority(
		&normalized_relation, candidate_proof->proof_kind
	);
	uint32_t relation_id = PROTOTYPE_INVALID_ID;
	for (size_t i = 0; i < delta->proposition_count; ++i) {
		if (propositions_equal(
			&delta->propositions[i], &normalized_relation
		)) {
			relation_id = (uint32_t)i;
			break;
		}
	}
	int new_relation = relation_id == PROTOTYPE_INVALID_ID;
	if (new_relation && reserve_slot(
			delta->proposition_count, delta->proposition_capacity
		) != 0) {
		return -1;
	}
	if (new_relation) {
		relation_id = (uint32_t)delta->proposition_count++;
		delta->propositions[relation_id] = normalized_relation;
	}
	struct prototype_judgement_derivation_candidate proof = *candidate_proof;
	proof.conclusion_proposition_id = relation_id;
	for (size_t i = 0; i < delta->derivation_candidate_count; ++i) {
		if (proof_candidates_equal(&delta->derivation_candidates[i], &proof)) {
			return 0;
		}
	}
	if (reserve_slot(
			delta->derivation_candidate_count,
			delta->derivation_candidate_capacity
		) != 0 || candidate_proof->premise_count >
			delta->candidate_premise_capacity -
			delta->candidate_premise_count) {
		if (new_relation) {
			delta->proposition_count--;
		}
		return -1;
	}
	uint32_t proof_id = (uint32_t)delta->derivation_candidate_count;
	struct prototype_judgement_candidate_premise* premises =
		proof.premise_count == 0 ? NULL :
		&delta->candidate_premises[delta->candidate_premise_count];
	if (proof.premise_count != 0) {
		memcpy(
			premises,
			proof.premises,
			proof.premise_count * sizeof(*premises)
		);
	}
	delta->candidate_premise_count += proof.premise_count;
	delta->derivation_candidates[proof_id] = proof;
	delta->derivation_candidates[proof_id].premises = premises;
	delta->derivation_candidate_count++;
	return 0;
}

static int add_delta_relation_with_authority_and_explicit_premise_actions(
	struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	uint32_t conclusion_operation_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_operation_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	const int* premise_semantic_action_kinds,
	const uint32_t* premise_semantic_action_ids,
	uint32_t premise_count
) {
	if (!delta ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = kind,
		.authority_kind = authority_kind,
		.authority_id = authority_id,
		.context_id = delta->current_context_id,
		.operation_id = conclusion_operation_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	proof.conclusion_kind = kind;
	proof.conclusion_context_id = delta->current_context_id;
	proof.conclusion_operation_id = conclusion_operation_id;
	proof.conclusion_subject = subject;
	proof.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		proof.premises[i].proposition.kind = premise_evidence ?
			premise_evidence[i].kind : PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof.premises[i].proposition.context_id = premise_context_ids ?
			premise_context_ids[i] : (premise_evidence ?
				premise_evidence[i].context_id : delta->current_context_id);
		proof.premises[i].proposition.operation_id = premise_operation_ids ?
			premise_operation_ids[i] : (premise_evidence ?
				premise_evidence[i].operation_id : PROTOTYPE_INVALID_ID);
		if (premise_evidence && premise_evidence[i].kind != 0) {
			if (premise_evidence[i].subject != premise_subjects[i] ||
				premise_evidence[i].classifier != premise_classifiers[i] ||
				(premise_context_ids && premise_context_ids[i] !=
					premise_evidence[i].context_id)) {
				return -1;
			}
			proof.premises[i].proposition.authority_kind =
				premise_evidence[i].authority_kind;
			proof.premises[i].proposition.authority_id =
				premise_evidence[i].authority_id;
			proof.premises[i].proposition.resource_usage_count =
				premise_evidence[i].resource_usage_count;
			proof.premises[i].proposition.resource_usage =
				premise_evidence[i].resource_usage;
		}
		proof.premises[i].proposition.subject = premise_subjects[i];
		proof.premises[i].proposition.classifier = premise_classifiers[i];
		if (premise_semantic_action_kinds) {
			proof.premises[i].semantic_action_kind =
				premise_semantic_action_kinds[i];
			proof.premises[i].semantic_action_id =
				premise_semantic_action_ids[i];
		}
	}
	return add_complete_delta_relation(delta, &relation, &proof);
}

static int add_delta_relation_with_authority_and_explicit_premises(
	struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	uint32_t conclusion_operation_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_operation_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
) {
	return add_delta_relation_with_authority_and_explicit_premise_actions(
		delta,
		authority_kind,
		authority_id,
		conclusion_operation_id,
		kind,
		subject,
		classifier,
		proof_kind,
		premise_context_ids,
		premise_operation_ids,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		NULL,
		NULL,
		premise_count
	);
}

static int add_delta_relation_with_explicit_premises(
	struct prototype_judgement_delta* delta,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_operation_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
) {
	int authority_kind = delta && delta->current_operation_id !=
		PROTOTYPE_INVALID_ID ?
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION :
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	uint32_t authority_id = authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
		delta->current_operation_id : subject;
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		authority_kind,
		authority_id,
		delta ? delta->current_operation_id : PROTOTYPE_INVALID_ID,
		kind,
		subject,
		classifier,
		proof_kind,
		premise_context_ids,
		premise_operation_ids,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		premise_count
	);
}

static int add_delta_relation(
	struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind
) {
	uint32_t conclusion_operation_id = authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
		authority_id : PROTOTYPE_INVALID_ID;
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		authority_kind,
		authority_id,
		conclusion_operation_id,
		kind,
		subject,
		classifier,
		proof_kind,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		0
	);
}

static int add_match_motive_result(
	struct prototype_judgement_delta* delta,
	uint32_t match_term,
	uint32_t classifier
) {
	if (!delta ||
		!delta->match_motive_results ||
		reserve_slot(
			delta->match_motive_result_count,
			delta->match_motive_result_capacity
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < delta->match_motive_result_count; ++i) {
		if (delta->match_motive_results[i].match_term == match_term &&
			delta->match_motive_results[i].classifier == classifier) {
			return 0;
		}
	}
	uint32_t id = (uint32_t)delta->match_motive_result_count;
	delta->match_motive_results[id].match_term = match_term;
	delta->match_motive_results[id].classifier = classifier;
	delta->match_motive_result_count++;
	return 0;
}

int prototype_judgement_delta_record_materialized_match_motive(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t classifier,
	const uint32_t* branch_operation_ids,
	const struct prototype_judgement_selected_evidence* branch_evidence,
	const uint32_t* branch_substitution_ids,
	uint32_t branch_count
) {
	if (!delta || !terms || !branch_operation_ids || !branch_evidence ||
		match_term >= terms->term_count ||
		classifier >= terms->term_count ||
		!match_motive_result_classifier(terms, match_term, classifier) ||
		branch_count != terms->terms[match_term].as.match.case_count ||
		branch_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	if (add_match_motive_result(delta, match_term, classifier) != 0) {
		return -1;
	}
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_context_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_operation_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_selected_evidence
		premise_evidence[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	int premise_semantic_action_kinds[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_semantic_action_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_count = 0;
	for (uint32_t i = 0; i < branch_count; ++i) {
		uint32_t case_id = terms->terms[match_term].as.match.first_case + i;
		if (branch_operation_ids[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (case_id >= terms->case_count ||
			branch_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			branch_evidence[i].operation_id != branch_operation_ids[i] ||
			branch_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
			branch_evidence[i].subject >= terms->term_count ||
			branch_evidence[i].classifier >= terms->term_count) {
			return -1;
		}
		/* A typed Match occurrence can share an alpha-interned Core Match with
		 * another occurrence while retaining occurrence-local branch Operations.
		 * Publish those actual branch subjects; accepted replay checks them against
		 * the principal Match Operation rather than a canonical representative's
		 * case slots. */
		premise_subjects[premise_count] = branch_evidence[i].subject;
		premise_context_ids[premise_count] = branch_evidence[i].context_id;
		premise_classifiers[premise_count] = branch_evidence[i].classifier;
		premise_operation_ids[premise_count] = branch_operation_ids[i];
		premise_evidence[premise_count] = branch_evidence[i];
		premise_semantic_action_kinds[premise_count] =
			branch_substitution_ids && branch_substitution_ids[i] !=
				PROTOTYPE_INVALID_ID ?
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION :
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		premise_semantic_action_ids[premise_count] = branch_substitution_ids ?
			branch_substitution_ids[i] : PROTOTYPE_INVALID_ID;
		premise_count++;
	}
	int authority_kind = delta->current_operation_id != PROTOTYPE_INVALID_ID ?
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION :
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	uint32_t authority_id = authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
		delta->current_operation_id : match_term;
	return add_delta_relation_with_authority_and_explicit_premise_actions(
		delta,
		authority_kind,
		authority_id,
		delta->current_operation_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		match_term,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE,
		premise_context_ids,
		premise_operation_ids,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		premise_semantic_action_kinds,
		premise_semantic_action_ids,
		premise_count
	);
}

static void remove_match_motive_results_normalization_equal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t classifier
) {
	if (!delta || !delta->match_motive_results || !terms || !type_declarations) {
		return;
	}
	size_t write = 0;
	for (size_t read = 0; read < delta->match_motive_result_count; ++read) {
		if (delta->match_motive_results[read].match_term == match_term &&
			(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				classifier,
				delta->match_motive_results[read].classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			continue;
		}
		if (write != read) {
			delta->match_motive_results[write] =
				delta->match_motive_results[read];
		}
		write++;
	}
	delta->match_motive_result_count = write;
}
