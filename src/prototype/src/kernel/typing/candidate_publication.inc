static int propositions_equal(
	const struct prototype_judgement_proposition* left,
	const struct prototype_judgement_proposition* right
);

static int proof_candidates_equal(
	const struct prototype_judgement_derivation_candidate* left,
	const struct prototype_judgement_derivation_candidate* right
) {
	if (!left || !right || left->proof_kind != right->proof_kind ||
		left->conclusion_proposition_id !=
			right->conclusion_proposition_id ||
		memcmp(&left->rule_data, &right->rule_data, sizeof(left->rule_data)) != 0 ||
		left->semantic_action_kind != right->semantic_action_kind ||
		left->semantic_action_id != right->semantic_action_id ||
		left->premise_count != right->premise_count) {
		return 0;
	}
	for (uint32_t i = 0; i < left->premise_count; ++i) {
		if (left->premises[i].proposition_store_kind !=
				right->premises[i].proposition_store_kind ||
			left->premises[i].proposition_id !=
				right->premises[i].proposition_id ||
			left->premises[i].semantic_action_kind !=
				right->premises[i].semantic_action_kind ||
			left->premises[i].semantic_action_id !=
				right->premises[i].semantic_action_id) {
			return 0;
		}
	}
	return 1;
}

static uint64_t proof_candidate_key_hash(
	const struct prototype_judgement_derivation_candidate* proof
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = judgement_key_hash_mix(hash, (uint32_t)proof->proof_kind);
	hash = judgement_key_hash_mix(hash, proof->conclusion_proposition_id);
	for (uint32_t i = 0; i < 4; ++i) {
		hash = judgement_key_hash_mix(hash, proof->rule_data.words[i]);
	}
	hash = judgement_key_hash_mix(hash, (uint32_t)proof->semantic_action_kind);
	hash = judgement_key_hash_mix(hash, proof->semantic_action_id);
	hash = judgement_key_hash_mix(hash, proof->premise_count);
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		hash = judgement_key_hash_mix(
			hash, (uint32_t)proof->premises[i].proposition_store_kind
		);
		hash = judgement_key_hash_mix(
			hash, proof->premises[i].proposition_id
		);
		hash = judgement_key_hash_mix(
			hash, (uint32_t)proof->premises[i].semantic_action_kind
		);
		hash = judgement_key_hash_mix(
			hash, proof->premises[i].semantic_action_id
		);
	}
	return hash;
}

static int find_proposition_id(
	const struct prototype_judgement_proposition* propositions,
	size_t proposition_count,
	const struct prototype_judgement_proposition* target,
	uint32_t* p_proposition_id
) {
	if (!propositions || !target || !p_proposition_id) {
		return -1;
	}
	for (uint32_t i = 0; i < proposition_count; ++i) {
		if (propositions_equal(&propositions[i], target)) {
			*p_proposition_id = i;
			return 0;
		}
	}
	return 1;
}

static int find_premise_authority_proposition_id(
	const struct prototype_judgement_proposition* propositions,
	size_t proposition_count,
	const struct prototype_judgement_proposition* target,
	uint32_t* p_proposition_id
) {
	if (!propositions || !target || !p_proposition_id ||
		target->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
		return 1;
	}
	for (uint32_t i = 0; i < proposition_count; ++i) {
		const struct prototype_judgement_proposition* candidate =
			&propositions[i];
		if (candidate->kind == target->kind &&
			candidate->authority_kind == target->authority_kind &&
			candidate->authority_id == target->authority_id &&
			candidate->context_id == target->context_id &&
			candidate->subject == target->subject &&
			candidate->classifier == target->classifier) {
			*p_proposition_id = i;
			return 0;
		}
	}
	return 1;
}

static int normalize_db_candidate_premises(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* normalized
) {
	if (!judgement || !proof ||
		(proof->premise_count != 0 && (!proof->premises || !normalized))) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		const struct prototype_judgement_candidate_premise* source =
			&proof->premises[i];
		uint32_t proposition_id = PROTOTYPE_INVALID_ID;
		if (source->proposition_store_kind ==
				PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB &&
			source->proposition_id < judgement->proposition_count) {
			proposition_id = source->proposition_id;
		} else if (!source->proposition) {
			return -1;
		} else if (find_proposition_id(
				judgement->propositions,
				judgement->proposition_count,
				source->proposition,
				&proposition_id
			) != 0 && find_premise_authority_proposition_id(
				judgement->propositions,
				judgement->proposition_count,
				source->proposition,
				&proposition_id
			) != 0 && prototype_judgement_proposition_intern(
				judgement, source->proposition, &proposition_id
			) != 0) {
			return -1;
		}
		normalized[i] = *source;
		normalized[i].proposition_store_kind =
			PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB;
		normalized[i].proposition_id = proposition_id;
		normalized[i].proposition = &judgement->propositions[proposition_id];
		normalized[i].builder_proposition = NULL;
	}
	return 0;
}

static int normalize_delta_candidate_premises(
	struct prototype_judgement_delta* delta,
	const struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* normalized
) {
	if (!delta || !proof ||
		(proof->premise_count != 0 && (!proof->premises || !normalized))) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		const struct prototype_judgement_candidate_premise* source =
			&proof->premises[i];
		int store_kind = source->proposition_store_kind;
		uint32_t proposition_id = source->proposition_id;
		const struct prototype_judgement_proposition* proposition = NULL;
		if (store_kind == PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DELTA &&
			proposition_id < delta->proposition_count) {
			proposition = &delta->propositions[proposition_id];
		} else if (store_kind == PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB &&
			delta->db && proposition_id < delta->db->proposition_count) {
			proposition = &delta->db->propositions[proposition_id];
		} else if (source->proposition && find_proposition_id(
				delta->propositions,
				delta->proposition_count,
				source->proposition,
				&proposition_id
			) == 0) {
			store_kind = PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DELTA;
			proposition = &delta->propositions[proposition_id];
		} else if (source->proposition && find_premise_authority_proposition_id(
				delta->propositions,
				delta->proposition_count,
				source->proposition,
				&proposition_id
			) == 0) {
			store_kind = PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DELTA;
			proposition = &delta->propositions[proposition_id];
		} else if (source->proposition && delta->db && find_proposition_id(
				delta->db->propositions,
				delta->db->proposition_count,
				source->proposition,
				&proposition_id
			) == 0) {
			store_kind = PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB;
			proposition = &delta->db->propositions[proposition_id];
		} else if (source->proposition && delta->db &&
			find_premise_authority_proposition_id(
				delta->db->propositions,
				delta->db->proposition_count,
				source->proposition,
				&proposition_id
			) == 0) {
			store_kind = PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB;
			proposition = &delta->db->propositions[proposition_id];
		} else if (source->proposition && reserve_slot(
				delta->proposition_count, delta->proposition_capacity
			) == 0) {
			proposition_id = (uint32_t)delta->proposition_count++;
			delta->propositions[proposition_id] = *source->proposition;
			store_kind = PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DELTA;
			proposition = &delta->propositions[proposition_id];
		} else {
			return -1;
		}
		normalized[i] = *source;
		normalized[i].proposition_store_kind = store_kind;
		normalized[i].proposition_id = proposition_id;
		normalized[i].proposition =
			proposition;
		normalized[i].builder_proposition = NULL;
	}
	return 0;
}

struct candidate_proposition_transaction_mark {
	struct prototype_storage_transaction_mark extent;
	uint64_t intern_requests;
	uint64_t intern_hits;
	uint64_t intern_probes;
};

static struct candidate_proposition_transaction_mark
candidate_proposition_transaction_mark(
	const struct prototype_judgement_db* judgement
) {
	return (struct candidate_proposition_transaction_mark) {
		.extent = prototype_storage_transaction_mark(
			judgement->proposition_count,
			judgement->proposition_resource_usage_count
		),
		.intern_requests = judgement->proposition_intern_requests,
		.intern_hits = judgement->proposition_intern_hits,
		.intern_probes = judgement->proposition_intern_probes
	};
}

static int candidate_proposition_transaction_rollback(
	struct prototype_judgement_db* judgement,
	struct candidate_proposition_transaction_mark mark
) {
	if (!judgement || prototype_storage_transaction_rollback(
			mark.extent,
			&judgement->proposition_count,
			&judgement->proposition_resource_usage_count
		) != 0) {
		return -1;
	}
	judgement->proposition_intern_requests = mark.intern_requests;
	judgement->proposition_intern_hits = mark.intern_hits;
	judgement->proposition_intern_probes = mark.intern_probes;
	return prototype_judgement_db_rebuild_index(judgement);
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
		left->occurrence_id == right->occurrence_id &&
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
	if (relation->occurrence_id != PROTOTYPE_INVALID_ID) {
		*p_authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE;
		*p_authority_id = relation->occurrence_id;
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
	if (candidate->occurrence_id != PROTOTYPE_INVALID_ID) {
		candidate->authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE;
		candidate->authority_id = candidate->occurrence_id;
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
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
		relation->occurrence_id : PROTOTYPE_INVALID_ID;
	struct prototype_judgement_proposition proposition = *relation;
	proposition.authority_kind = authority_kind;
	proposition.authority_id = authority_id;
	proposition.occurrence_id = operation_id;
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
	size_t bucket = judgement_index_bucket(key_hash);
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

/* Accepted Claim dependencies are a finite proof DAG. A scoped Proposition is
 * a rule parameter, not a Claim edge. This traversal is used only when adding
 * another Derivation for an already accepted Claim, where allocation order no
 * longer proves that the new edge is acyclic. */
static int judgement_claim_dependency_reaches(
	const struct prototype_judgement_db* judgement,
	uint32_t source_claim_id,
	uint32_t target_claim_id
) {
	if (!judgement || source_claim_id >= judgement->claim_count ||
		target_claim_id >= judgement->claim_count) {
		return -1;
	}
	unsigned char* visited = calloc(
		judgement->claim_count ? judgement->claim_count : 1,
		sizeof(*visited)
	);
	uint32_t* stack = malloc(
		(judgement->claim_count ? judgement->claim_count : 1) * sizeof(*stack)
	);
	if (!visited || !stack) {
		free(visited);
		free(stack);
		return -1;
	}
	size_t stack_count = 0;
	visited[source_claim_id] = 1;
	stack[stack_count++] = source_claim_id;
	int result = 0;
	while (stack_count != 0) {
		uint32_t current = stack[--stack_count];
		if (current == target_claim_id) {
			result = 1;
			break;
		}
		for (uint32_t i = 0;
			i < (uint32_t)judgement->derivation_count; ++i) {
			const struct prototype_judgement_derivation* derivation =
				&judgement->derivations[i];
			if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID ||
				derivation->conclusion_claim_id != current) {
				continue;
			}
			if (derivation->premise_count >
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
				(derivation->premise_count != 0 && !derivation->premises)) {
				result = -1;
				break;
			}
			for (uint32_t j = 0; j < derivation->premise_count; ++j) {
				uint32_t premise_claim_id = derivation->premises[j].claim_id;
				if (premise_claim_id == PROTOTYPE_INVALID_ID) {
					continue;
				}
				if (premise_claim_id >= judgement->claim_count) {
					result = -1;
					break;
				}
				if (!visited[premise_claim_id]) {
					visited[premise_claim_id] = 1;
					stack[stack_count++] = premise_claim_id;
				}
			}
			if (result < 0) {
				break;
			}
		}
		if (result < 0) {
			break;
		}
	}
	free(visited);
	free(stack);
	return result;
}

static int judgement_derivation_would_create_cycle(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation* derivation
) {
	if (!judgement || !derivation ||
		derivation->conclusion_claim_id >= judgement->claim_count ||
		(derivation->premise_count != 0 && !derivation->premises)) {
		return -1;
	}
	int conclusion_is_existing_dependency = 0;
	for (uint32_t i = 0; i < derivation->premise_count; ++i) {
		uint32_t premise_claim_id = derivation->premises[i].claim_id;
		if (premise_claim_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (premise_claim_id >= judgement->claim_count) {
			return -1;
		}
		if (premise_claim_id == derivation->conclusion_claim_id) {
			return 1;
		}
	}
	/* A newly interned Claim cannot already be the target of an accepted
	 * dependency edge. Avoid a full reachability traversal for that dominant
	 * publication case. */
	for (uint32_t i = 0;
		i < (uint32_t)judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* accepted =
			&judgement->derivations[i];
		if (accepted->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		if (accepted->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
			(accepted->premise_count != 0 && !accepted->premises)) {
			return -1;
		}
		for (uint32_t j = 0; j < accepted->premise_count; ++j) {
			if (accepted->premises[j].claim_id ==
					derivation->conclusion_claim_id) {
				conclusion_is_existing_dependency = 1;
				break;
			}
		}
		if (conclusion_is_existing_dependency) {
			break;
		}
	}
	if (!conclusion_is_existing_dependency) {
		return 0;
	}
	for (uint32_t i = 0; i < derivation->premise_count; ++i) {
		uint32_t premise_claim_id = derivation->premises[i].claim_id;
		if (premise_claim_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int reaches = judgement_claim_dependency_reaches(
			judgement,
			premise_claim_id,
			derivation->conclusion_claim_id
		);
		if (reaches != 0) {
			return reaches;
		}
	}
	return 0;
}

static int judgement_claim_dependency_dag_is_valid(
	const struct prototype_judgement_db* judgement
) {
	if (!judgement || (judgement->claim_count != 0 && !judgement->claims)) {
		return -1;
	}
	size_t count = judgement->claim_count ? judgement->claim_count : 1;
	uint32_t* incoming = calloc(count, sizeof(*incoming));
	uint32_t* queue = malloc(count * sizeof(*queue));
	if (!incoming || !queue) {
		free(incoming);
		free(queue);
		return -1;
	}
	for (uint32_t i = 0;
		i < (uint32_t)judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			uint32_t premise_claim_id = derivation->premises[j].claim_id;
			if (premise_claim_id != PROTOTYPE_INVALID_ID) {
				if (incoming[premise_claim_id] == UINT32_MAX) {
					free(incoming);
					free(queue);
					return -1;
				}
				incoming[premise_claim_id]++;
			}
		}
	}
	size_t queue_first = 0;
	size_t queue_count = 0;
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		if (incoming[i] == 0) {
			queue[queue_count++] = i;
		}
	}
	size_t visited_count = 0;
	while (queue_first < queue_count) {
		uint32_t current = queue[queue_first++];
		visited_count++;
		for (uint32_t i = 0;
			i < (uint32_t)judgement->derivation_count; ++i) {
			const struct prototype_judgement_derivation* derivation =
				&judgement->derivations[i];
			if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID ||
				derivation->conclusion_claim_id != current) {
				continue;
			}
			for (uint32_t j = 0; j < derivation->premise_count; ++j) {
				uint32_t premise_claim_id = derivation->premises[j].claim_id;
				if (premise_claim_id != PROTOTYPE_INVALID_ID &&
					--incoming[premise_claim_id] == 0) {
					queue[queue_count++] = premise_claim_id;
				}
			}
		}
	}
	free(incoming);
	free(queue);
	return visited_count == judgement->claim_count ? 0 : -1;
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
	judgement_index_clear(judgement->candidate_index_heads);
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
		struct prototype_judgement_derivation_candidate* candidate =
			&judgement->derivation_candidates[i];
		if (candidate->proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		const struct prototype_judgement_proposition* conclusion =
			prototype_judgement_proposition_get(
				judgement, candidate->conclusion_proposition_id
			);
		if (!conclusion || candidate->premise_count >
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
			(candidate->premise_count != 0 && !candidate->premises)) {
			return -1;
		}
		candidate->conclusion = conclusion;
		for (uint32_t j = 0; j < candidate->premise_count; ++j) {
			struct prototype_judgement_candidate_premise* premise =
				&candidate->premises[j];
			if (premise->proposition_store_kind !=
					PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_DB ||
				!prototype_judgement_proposition_get(
					judgement, premise->proposition_id
				)) {
				return -1;
			}
			premise->proposition = &judgement->propositions[
				premise->proposition_id
			];
			premise->builder_proposition = NULL;
		}
		candidate->key_hash = proof_candidate_key_hash(candidate);
		size_t bucket = judgement_index_bucket(candidate->key_hash);
		for (uint32_t existing = judgement->candidate_index_heads[bucket];
			existing != PROTOTYPE_INVALID_ID;
			existing = judgement->derivation_candidates[existing].hash_next) {
			if (judgement->derivation_candidates[existing].key_hash ==
					candidate->key_hash && proof_candidates_equal(
					&judgement->derivation_candidates[existing], candidate
				)) {
				return -1;
			}
		}
		candidate->hash_next = judgement->candidate_index_heads[bucket];
		judgement->candidate_index_heads[bucket] = i;
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
	return judgement_claim_dependency_dag_is_valid(judgement);
}

int prototype_judgement_delta_rebuild_candidate_index(
	struct prototype_judgement_delta* delta
) {
	if (!delta ||
		delta->derivation_candidate_count >
			delta->derivation_candidate_capacity) {
		return -1;
	}
	judgement_index_clear(delta->candidate_index_heads);
	for (uint32_t i = 0;
		i < (uint32_t)delta->derivation_candidate_count;
		++i) {
		struct prototype_judgement_derivation_candidate* candidate =
			&delta->derivation_candidates[i];
		if (candidate->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			candidate->key_hash = 0;
			candidate->hash_next = PROTOTYPE_INVALID_ID;
			continue;
		}
		if (candidate->conclusion_proposition_id >=
				delta->proposition_count ||
			candidate->premise_count >
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
			(candidate->premise_count != 0 && !candidate->premises)) {
			return -1;
		}
		candidate->conclusion = &delta->propositions[
			candidate->conclusion_proposition_id
		];
		candidate->key_hash = proof_candidate_key_hash(candidate);
		size_t bucket = judgement_index_bucket(candidate->key_hash);
		for (uint32_t existing = delta->candidate_index_heads[bucket];
			existing != PROTOTYPE_INVALID_ID;
			existing = delta->derivation_candidates[existing].hash_next) {
			if (delta->derivation_candidates[existing].key_hash ==
					candidate->key_hash && proof_candidates_equal(
					&delta->derivation_candidates[existing], candidate
				)) {
				return -1;
			}
		}
		candidate->hash_next = delta->candidate_index_heads[bucket];
		delta->candidate_index_heads[bucket] = i;
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
	size_t bucket = judgement_index_bucket(key_hash);
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
	size_t bucket = judgement_index_bucket(key_hash);
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
	if (judgement_derivation_would_create_cycle(judgement, identity) != 0) {
		return -1;
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
		candidate_proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	if (reserve_slot(
			judgement->derivation_candidate_count,
			judgement->derivation_candidate_capacity
		) != 0 || candidate_proof->premise_count >
			judgement->candidate_premise_capacity -
			judgement->candidate_premise_count) {
		return -1;
	}
	struct candidate_proposition_transaction_mark transaction =
		candidate_proposition_transaction_mark(judgement);
	struct prototype_judgement_proposition normalized_relation =
		*candidate_relation;
	initialize_candidate_claim_authority(
		&normalized_relation, candidate_proof->proof_kind
	);
	uint32_t relation_id;
	if (prototype_judgement_proposition_intern(
			judgement, &normalized_relation, &relation_id
		) != 0) {
		candidate_proposition_transaction_rollback(judgement, transaction);
		return -1;
	}
	struct prototype_judgement_derivation_candidate proof = *candidate_proof;
	struct prototype_judgement_candidate_premise
		normalized_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	proof.conclusion_proposition_id = relation_id;
	proof.conclusion = &judgement->propositions[relation_id];
	if (normalize_db_candidate_premises(
			judgement, candidate_proof, normalized_premises
		) != 0) {
		candidate_proposition_transaction_rollback(judgement, transaction);
		return -1;
	}
	proof.premises = proof.premise_count == 0 ? NULL : normalized_premises;
	proof.key_hash = proof_candidate_key_hash(&proof);
	size_t bucket = judgement_index_bucket(proof.key_hash);
	for (uint32_t i = judgement->candidate_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = judgement->derivation_candidates[i].hash_next) {
		if (judgement->derivation_candidates[i].key_hash == proof.key_hash &&
			proof_candidates_equal(&judgement->derivation_candidates[i], &proof)) {
			return 0;
		}
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
	judgement->derivation_candidates[proof_id].hash_next =
		judgement->candidate_index_heads[bucket];
	judgement->candidate_index_heads[bucket] = proof_id;
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
				candidate_proof->premises[i].proposition
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
		(void)prototype_judgement_db_rebuild_index(judgement);
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
	int cycle_status = judgement_derivation_would_create_cycle(
		judgement, &derivation
	);
	if (cycle_status != 0) {
		judgement->derivation_candidate_count = derivation_candidate_mark;
		judgement->candidate_premise_count = candidate_premise_mark;
		if (judgement->claim_count == claim_mark + 1 && claim_id == claim_mark) {
			size_t bucket = judgement->claims[claim_id].key_hash %
				PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
			judgement->claim_index_heads[bucket] =
				judgement->claims[claim_id].hash_next;
			judgement->claim_count = claim_mark;
		}
		if (cycle_status > 0 && claim_id < claim_mark) {
			/* The conclusion already has finite accepted evidence. Reindexing it
			 * back through a Claim that depends on it adds only circular
			 * certificate provenance, so normalize the operation to reuse the
			 * existing Claim. */
			*p_claim_id = claim_id;
			(void)prototype_judgement_db_rebuild_index(judgement);
			return 0;
		}
		(void)prototype_judgement_db_rebuild_index(judgement);
		return -1;
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
		(void)prototype_judgement_db_rebuild_index(judgement);
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
					derivation->premises[j].proposition,
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
							derivation->premises[j].proposition,
							&ignored_claim_id
						) > 0) {
						fprintf(
							stderr,
							"complete derivation blocked rule=%d subject=%u "
							"classifier=%u premise=%u:%u context=%u authority=%d:%u "
							"occurrence=%u\n",
							derivation->proof_kind,
							derivation->conclusion->subject,
							derivation->conclusion->classifier,
							derivation->premises[j].proposition->subject,
							derivation->premises[j].proposition->classifier,
							derivation->premises[j].proposition->context_id,
							derivation->premises[j].proposition->authority_kind,
							derivation->premises[j].proposition->authority_id,
							derivation->premises[j].proposition->occurrence_id
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
	judgement_index_clear(delta->candidate_index_heads);

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
		.occurrence_id = operation_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_proposition
		premise_propositions[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	initialize_proof_rule_parameters(
		&proof, proof_premises, premise_propositions
	);
	initialize_proof_premises(
		&proof,
		context_id,
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
	size_t proposition_mark = delta ? delta->proposition_count : 0;
	if (!delta || !candidate_relation || !candidate_proof ||
		(candidate_proof->premise_count != 0 && !candidate_proof->premises) ||
		candidate_proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
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
			 candidate_proof->premises[0].proposition->context_id !=
				candidate_relation->context_id ||
			 candidate_proof->premises[0].proposition->subject !=
				candidate_relation->subject ||
			 candidate_proof->premises[0].proposition->classifier !=
				candidate_relation->classifier))) {
			return -1;
		}
	}
	struct prototype_judgement_proposition normalized_relation =
		*candidate_relation;
	if (normalized_relation.occurrence_id != PROTOTYPE_INVALID_ID &&
		delta->occurrence_usage_provider && delta->occurrence_usage_provider(
			delta->occurrence_usage_provider_context,
			normalized_relation.occurrence_id,
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
	struct prototype_judgement_candidate_premise
		normalized_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	proof.conclusion_proposition_id = relation_id;
	proof.conclusion = &delta->propositions[relation_id];
	if (normalize_delta_candidate_premises(
			delta, candidate_proof, normalized_premises
		) != 0) {
		delta->proposition_count = proposition_mark;
		return -1;
	}
	proof.premises = proof.premise_count == 0 ? NULL : normalized_premises;
	proof.key_hash = proof_candidate_key_hash(&proof);
	size_t bucket = judgement_index_bucket(proof.key_hash);
	for (uint32_t i = delta->candidate_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = delta->derivation_candidates[i].hash_next) {
		if (delta->derivation_candidates[i].key_hash == proof.key_hash &&
			proof_candidates_equal(&delta->derivation_candidates[i], &proof)) {
			return 0;
		}
	}
	if (reserve_slot(
			delta->derivation_candidate_count,
			delta->derivation_candidate_capacity
		) != 0 || candidate_proof->premise_count >
			delta->candidate_premise_capacity -
			delta->candidate_premise_count) {
		delta->proposition_count = proposition_mark;
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
	delta->derivation_candidates[proof_id].hash_next =
		delta->candidate_index_heads[bucket];
	delta->candidate_index_heads[bucket] = proof_id;
	delta->derivation_candidate_count++;
	return 0;
}

static int add_delta_relation_with_authority_and_explicit_premise_actions(
	struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	uint32_t conclusion_occurrence_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_occurrence_ids,
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
		.occurrence_id = conclusion_occurrence_id,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_proposition
		premise_propositions[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	initialize_proof_rule_parameters(
		&proof, proof_premises, premise_propositions
	);
	proof.premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		proof.premises[i].builder_proposition->kind = premise_evidence ?
			premise_evidence[i].kind : PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof.premises[i].builder_proposition->context_id = premise_context_ids ?
			premise_context_ids[i] : (premise_evidence ?
				premise_evidence[i].context_id : delta->current_context_id);
		proof.premises[i].builder_proposition->occurrence_id =
			premise_evidence && premise_evidence[i].kind != 0 ?
			premise_evidence[i].occurrence_id : (premise_occurrence_ids ?
				premise_occurrence_ids[i] : PROTOTYPE_INVALID_ID);
		if (premise_evidence && premise_evidence[i].kind != 0) {
			if (premise_evidence[i].subject != premise_subjects[i] ||
				premise_evidence[i].classifier != premise_classifiers[i] ||
				(premise_context_ids && premise_context_ids[i] !=
					premise_evidence[i].context_id)) {
				return -1;
			}
			proof.premises[i].builder_proposition->authority_kind =
				premise_evidence[i].authority_kind;
			proof.premises[i].builder_proposition->authority_id =
				premise_evidence[i].authority_id;
			proof.premises[i].builder_proposition->resource_usage_count =
				premise_evidence[i].resource_usage_count;
			proof.premises[i].builder_proposition->resource_usage =
				premise_evidence[i].resource_usage;
		}
		proof.premises[i].builder_proposition->subject = premise_subjects[i];
		proof.premises[i].builder_proposition->classifier = premise_classifiers[i];
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
	uint32_t conclusion_occurrence_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_occurrence_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
) {
	return add_delta_relation_with_authority_and_explicit_premise_actions(
		delta,
		authority_kind,
		authority_id,
		conclusion_occurrence_id,
		kind,
		subject,
		classifier,
		proof_kind,
		premise_context_ids,
		premise_occurrence_ids,
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
	const uint32_t* premise_occurrence_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
) {
	int authority_kind = delta && delta->current_occurrence_id !=
		PROTOTYPE_INVALID_ID ?
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE :
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	uint32_t authority_id = authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
		delta->current_occurrence_id : subject;
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		authority_kind,
		authority_id,
		delta ? delta->current_occurrence_id : PROTOTYPE_INVALID_ID,
		kind,
		subject,
		classifier,
		proof_kind,
		premise_context_ids,
		premise_occurrence_ids,
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
	uint32_t conclusion_occurrence_id = authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
		authority_id : PROTOTYPE_INVALID_ID;
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		authority_kind,
		authority_id,
		conclusion_occurrence_id,
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
	const uint32_t* branch_occurrence_ids,
	const struct prototype_judgement_selected_evidence* branch_evidence,
	const uint32_t* branch_substitution_ids,
	uint32_t branch_count
) {
	if (!delta || !terms || !branch_occurrence_ids || !branch_evidence ||
		match_term >= terms->term_count ||
		classifier >= terms->term_count ||
		!match_motive_result_classifier(terms, match_term, classifier) ||
		branch_count != terms->terms[match_term].as.match.case_count ||
		branch_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		fprintf(stderr,
			"materialized match motive invalid input match=%u classifier=%u branch-count=%u term-count=%zu case-count=%u motive=%d\n",
			match_term, classifier, branch_count, terms ? terms->term_count : 0,
			terms && match_term < terms->term_count &&
				terms->terms[match_term].tag == PROTOTYPE_TERM_MATCH ?
				terms->terms[match_term].as.match.case_count : PROTOTYPE_INVALID_ID,
			terms ? match_motive_result_classifier(terms, match_term, classifier) : 0);
		return -1;
	}
	if (add_match_motive_result(delta, match_term, classifier) != 0) {
		return -1;
	}
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_context_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_occurrence_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_selected_evidence
		premise_evidence[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	int premise_semantic_action_kinds[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_semantic_action_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_count = 0;
	for (uint32_t i = 0; i < branch_count; ++i) {
		uint32_t case_id = terms->terms[match_term].as.match.first_case + i;
		if (branch_occurrence_ids[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (case_id >= terms->case_count ||
			branch_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			branch_evidence[i].occurrence_id != branch_occurrence_ids[i] ||
			branch_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
			branch_evidence[i].subject >= terms->term_count ||
			branch_evidence[i].classifier >= terms->term_count) {
			fprintf(stderr,
				"materialized match motive invalid branch case=%u case-id=%u/%zu expected-occurrence=%u evidence-kind=%d evidence-occurrence=%u authority=%d subject=%u classifier=%u\n",
				i, case_id, terms->case_count, branch_occurrence_ids[i],
				branch_evidence[i].kind, branch_evidence[i].occurrence_id,
				branch_evidence[i].authority_kind, branch_evidence[i].subject,
				branch_evidence[i].classifier);
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
		premise_occurrence_ids[premise_count] = branch_occurrence_ids[i];
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
	int authority_kind = delta->current_occurrence_id != PROTOTYPE_INVALID_ID ?
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE :
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	uint32_t authority_id = authority_kind ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
		delta->current_occurrence_id : match_term;
	return add_delta_relation_with_authority_and_explicit_premise_actions(
		delta,
		authority_kind,
		authority_id,
		delta->current_occurrence_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		match_term,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE,
		premise_context_ids,
		premise_occurrence_ids,
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
