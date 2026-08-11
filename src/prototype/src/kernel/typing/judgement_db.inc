#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/graph/operation_graph.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int reserve_slot(size_t count, size_t capacity) {
	return count < capacity ? 0 : -1;
}

static uint64_t judgement_key_hash_mix(uint64_t hash, uint32_t value) {
	hash ^= value;
	hash *= UINT64_C(1099511628211);
	return hash;
}

static void judgement_index_clear(uint32_t* heads) {
	for (size_t i = 0;
		i < PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
		++i) {
		heads[i] = PROTOTYPE_INVALID_ID;
	}
}

static void candidate_claim_authority(
	const struct prototype_judgement_proposition* relation,
	int* p_authority_kind,
	uint32_t* p_authority_id
);
static void selected_evidence_from_candidate(
	const struct prototype_judgement_proposition* candidate,
	struct prototype_judgement_selected_evidence* evidence
);
static int selected_evidence_equal(
	const struct prototype_judgement_selected_evidence* left,
	const struct prototype_judgement_selected_evidence* right
);
static int candidate_has_derivation_kind(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	int proof_kind
);
static int candidate_has_derivation_other_than(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	int excluded_proof_kind
);
static int judgement_operation_expected_polarity(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	uint32_t operation_id,
	int* p_category
);

void prototype_judgement_db_init(
	struct prototype_judgement_db* db,
	struct prototype_judgement_proposition* relations,
	struct prototype_judgement_derivation_candidate* proofs,
	struct prototype_judgement_claim* claims,
	struct prototype_judgement_derivation* derivations,
	size_t claim_capacity,
	struct prototype_judgement_candidate_premise* candidate_premises,
	size_t candidate_premise_capacity,
	struct prototype_judgement_premise_edge* accepted_premises,
	size_t accepted_premise_capacity
) {
	memset(db, 0, sizeof(*db));
	db->propositions = relations;
	db->derivation_candidates = proofs;
	db->proposition_capacity = claim_capacity;
	db->derivation_candidate_capacity = claim_capacity;
	db->claims = claims;
	db->derivations = derivations;
	db->candidate_premises = candidate_premises;
	db->accepted_premises = accepted_premises;
	db->claim_capacity = claim_capacity;
	db->derivation_capacity = claim_capacity;
	db->candidate_premise_capacity = candidate_premise_capacity;
	db->accepted_premise_capacity = accepted_premise_capacity;
	judgement_index_clear(db->claim_index_heads);
	judgement_index_clear(db->proposition_index_heads);
	judgement_index_clear(db->derivation_index_heads);
}

void prototype_judgement_db_set_resource_usage_storage(
	struct prototype_judgement_db* db,
	struct prototype_usage_entry* entries,
	size_t capacity
) {
	if (!db) {
		return;
	}
	db->proposition_resource_usage = entries;
	db->proposition_resource_usage_capacity = capacity;
	db->proposition_resource_usage_count = 0;
}

static int judgement_proposition_equal(
	const struct prototype_judgement_proposition* left,
	const struct prototype_judgement_proposition* right
) {
	return left && right && left->kind == right->kind &&
		left->authority_kind == right->authority_kind &&
		left->authority_id == right->authority_id &&
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

static int selected_evidence_is_subject_local(
	const struct prototype_judgement_selected_evidence* evidence
) {
	if (!evidence || evidence->authority_id != evidence->subject) {
		return 0;
	}
	return evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING ||
		evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION ||
		evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
}

static int merge_subject_local_evidence_exact(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if ((!candidates && candidate_count != 0) || !selected || !p_found) {
		return -1;
	}
	for (size_t i = 0; i < candidate_count; ++i) {
		const struct prototype_judgement_proposition* candidate =
			&candidates[i];
		if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate->context_id != context_id ||
			candidate->subject != subject ||
			candidate->classifier != classifier) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!selected_evidence_is_subject_local(&current)) {
			continue;
		}
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
		} else if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

static int lookup_delta_subject_local_evidence_exact(
	const struct prototype_judgement_delta* delta,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_subject_local_evidence_exact(
		delta->propositions,
		delta->proposition_count,
		context_id,
		subject,
		classifier,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	if (delta->db) {
		status = merge_subject_local_evidence_exact(
			delta->db->propositions,
			delta->db->proposition_count,
			context_id,
			subject,
			classifier,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
	}
	return found ? 0 : 1;
}

static uint64_t judgement_proposition_key_hash(
	const struct prototype_judgement_proposition* proposition
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = judgement_key_hash_mix(hash, (uint32_t)proposition->kind);
	hash = judgement_key_hash_mix(hash, (uint32_t)proposition->authority_kind);
	hash = judgement_key_hash_mix(hash, proposition->authority_id);
	hash = judgement_key_hash_mix(hash, proposition->context_id);
	hash = judgement_key_hash_mix(hash, proposition->operation_id);
	hash = judgement_key_hash_mix(hash, proposition->subject);
	hash = judgement_key_hash_mix(hash, proposition->classifier);
	hash = judgement_key_hash_mix(hash, proposition->resource_usage_count);
	for (uint32_t i = 0; i < proposition->resource_usage_count; ++i) {
		hash = judgement_key_hash_mix(
			hash, proposition->resource_usage[i].binding_id
		);
		hash = judgement_key_hash_mix(
			hash, (uint32_t)proposition->resource_usage[i].grade
		);
	}
	return hash;
}

int prototype_judgement_proposition_intern(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* identity,
	uint32_t* p_proposition_id
) {
	if (!judgement || !identity || !p_proposition_id) {
		return -1;
	}
	if ((identity->resource_usage_count != 0 && !identity->resource_usage) ||
		identity->resource_usage_count >
			judgement->proposition_resource_usage_capacity -
			judgement->proposition_resource_usage_count) {
		return -1;
	}
	for (uint32_t i = 0; i < identity->resource_usage_count; ++i) {
		if (identity->resource_usage[i].grade <= PROTOTYPE_USAGE_ZERO ||
			identity->resource_usage[i].grade > PROTOTYPE_USAGE_MANY ||
			(i != 0 && identity->resource_usage[i - 1].binding_id >=
				identity->resource_usage[i].binding_id)) {
			return -1;
		}
	}
	uint64_t key_hash = judgement_proposition_key_hash(identity);
	size_t bucket = key_hash % PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
	judgement->proposition_intern_requests++;
	for (uint32_t i = judgement->proposition_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = judgement->propositions[i].hash_next) {
		judgement->proposition_intern_probes++;
		if (i >= judgement->proposition_count) {
			return -1;
		}
		if (judgement->propositions[i].key_hash == key_hash &&
			judgement_proposition_equal(&judgement->propositions[i], identity)) {
			judgement->proposition_intern_hits++;
			*p_proposition_id = i;
			return 0;
		}
	}
	if (reserve_slot(
			judgement->proposition_count, judgement->proposition_capacity
		) != 0) {
		return -1;
	}
	uint32_t proposition_id = (uint32_t)judgement->proposition_count++;
	judgement->propositions[proposition_id] = *identity;
	struct prototype_usage_entry* resource_usage =
		identity->resource_usage_count == 0 ? NULL :
		&judgement->proposition_resource_usage[
			judgement->proposition_resource_usage_count
		];
	if (identity->resource_usage_count != 0) {
		memcpy(
			resource_usage,
			identity->resource_usage,
			identity->resource_usage_count * sizeof(*resource_usage)
		);
	}
	judgement->proposition_resource_usage_count +=
		identity->resource_usage_count;
	judgement->propositions[proposition_id].resource_usage = resource_usage;
	judgement->propositions[proposition_id].key_hash = key_hash;
	judgement->propositions[proposition_id].hash_next =
		judgement->proposition_index_heads[bucket];
	judgement->proposition_index_heads[bucket] = proposition_id;
	*p_proposition_id = proposition_id;
	return 0;
}

int prototype_judgement_proposition_find_exact(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* identity,
	uint32_t* p_proposition_id
) {
	if (!judgement || !identity || !p_proposition_id) {
		return -1;
	}
	uint64_t key_hash = judgement_proposition_key_hash(identity);
	size_t bucket = key_hash % PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
	for (uint32_t i = judgement->proposition_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = judgement->propositions[i].hash_next) {
		if (i >= judgement->proposition_count) {
			return -1;
		}
		if (judgement->propositions[i].key_hash == key_hash &&
			judgement_proposition_equal(&judgement->propositions[i], identity)) {
			*p_proposition_id = i;
			return 0;
		}
	}
	return 1;
}

const struct prototype_judgement_claim* prototype_judgement_claim_get(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	const struct prototype_judgement_claim* claim = judgement &&
		claim_id < judgement->claim_count ? &judgement->claims[claim_id] : NULL;
	return claim && prototype_judgement_claim_proposition(judgement, claim_id) ?
		claim : NULL;
}

const struct prototype_judgement_proposition* prototype_judgement_proposition_get(
	const struct prototype_judgement_db* judgement,
	uint32_t proposition_id
) {
	if (!judgement || proposition_id >= judgement->proposition_count ||
		judgement->propositions[proposition_id].kind ==
			PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
		return NULL;
	}
	return &judgement->propositions[proposition_id];
}

const struct prototype_judgement_proposition* prototype_judgement_claim_proposition(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	if (!judgement || claim_id >= judgement->claim_count) {
		return NULL;
	}
	return prototype_judgement_proposition_get(
		judgement, judgement->claims[claim_id].proposition_id
	);
}

const struct prototype_judgement_proposition* prototype_judgement_premise_proposition(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_premise_edge* premise
) {
	if (!judgement || !premise ||
		(premise->claim_id == PROTOTYPE_INVALID_ID) ==
			(premise->scoped_proposition_id == PROTOTYPE_INVALID_ID)) {
		return NULL;
	}
	if (premise->claim_id != PROTOTYPE_INVALID_ID) {
		return prototype_judgement_claim_proposition(
			judgement, premise->claim_id
		);
	}
	return prototype_judgement_proposition_get(
		judgement, premise->scoped_proposition_id
	);
}

const struct prototype_judgement_derivation* prototype_judgement_derivation_get(
	const struct prototype_judgement_db* judgement,
	uint32_t derivation_id
) {
	return judgement && derivation_id < judgement->derivation_count ?
		&judgement->derivations[derivation_id] : NULL;
}

static int judgement_claim_identity_equal(
	const struct prototype_judgement_claim* left,
	const struct prototype_judgement_claim* right
) {
	return left && right && left->proposition_id == right->proposition_id;
}

static uint64_t judgement_claim_key_hash(
	const struct prototype_judgement_claim* claim
) {
	return judgement_key_hash_mix(
		UINT64_C(1469598103934665603), claim->proposition_id
	);
}

static uint64_t judgement_derivation_key_hash(
	const struct prototype_judgement_derivation* derivation
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = judgement_key_hash_mix(hash, (uint32_t)derivation->proof_kind);
	hash = judgement_key_hash_mix(hash, derivation->conclusion_claim_id);
	for (uint32_t i = 0; i < 4; ++i) {
		hash = judgement_key_hash_mix(hash, derivation->rule_data.words[i]);
	}
	hash = judgement_key_hash_mix(hash, (uint32_t)derivation->semantic_action_kind);
	hash = judgement_key_hash_mix(hash, derivation->semantic_action_id);
	hash = judgement_key_hash_mix(hash, derivation->premise_count);
	for (uint32_t i = 0; i < derivation->premise_count; ++i) {
		hash = judgement_key_hash_mix(hash, derivation->premises[i].claim_id);
		hash = judgement_key_hash_mix(
			hash, derivation->premises[i].scoped_proposition_id
		);
		hash = judgement_key_hash_mix(
			hash, (uint32_t)derivation->premises[i].semantic_action_kind
		);
		hash = judgement_key_hash_mix(
			hash, derivation->premises[i].semantic_action_id
		);
	}
	return hash;
}

int prototype_judgement_find_exact_claim(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_claim* identity,
	uint32_t* p_claim_id
) {
	if (!judgement || !identity || !p_claim_id) {
		return -1;
	}
	uint64_t key_hash = judgement_claim_key_hash(identity);
	size_t bucket = key_hash % PROTOTYPE_JUDGEMENT_GRAPH_INDEX_BUCKET_COUNT;
	for (uint32_t i = judgement->claim_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = judgement->claims[i].hash_next) {
		if (i >= judgement->claim_count) {
			return -1;
		}
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (claim->key_hash == key_hash &&
			judgement_claim_identity_equal(claim, identity)) {
			*p_claim_id = i;
			return 0;
		}
	}
	return 1;
}

int prototype_judgement_claim_derivations(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	uint32_t* derivation_ids,
	size_t derivation_capacity,
	size_t* p_derivation_count
) {
	if (!judgement || claim_id >= judgement->claim_count ||
		(!derivation_ids && derivation_capacity != 0) || !p_derivation_count) {
		return -1;
	}
	size_t count = 0;
	for (uint32_t i = 0; i < (uint32_t)judgement->derivation_count; ++i) {
		if (judgement->derivations[i].conclusion_claim_id != claim_id) {
			continue;
		}
		if (count < derivation_capacity) {
			derivation_ids[count] = i;
		}
		count++;
	}
	*p_derivation_count = count;
	return count <= derivation_capacity ? 0 : 1;
}

void prototype_judgement_delta_init(
	struct prototype_judgement_delta* delta,
	struct prototype_judgement_db* db,
	struct prototype_judgement_proposition* relations,
	struct prototype_judgement_derivation_candidate* proofs,
	size_t proposition_capacity,
	struct prototype_judgement_candidate_premise* candidate_premises,
	size_t candidate_premise_capacity,
	struct prototype_judgement_match_motive_result* match_motive_results,
	size_t match_motive_result_capacity,
	struct prototype_judgement_computation_constraint* computation_constraints,
	size_t computation_constraint_capacity,
	struct prototype_judgement_effect_row_constraint* effect_row_constraints,
	size_t effect_row_constraint_capacity
) {
	memset(delta, 0, sizeof(*delta));
	delta->db = db;
	delta->propositions = relations;
	delta->derivation_candidates = proofs;
	delta->candidate_premises = candidate_premises;
	delta->match_motive_results = match_motive_results;
	delta->computation_constraints = computation_constraints;
	delta->effect_row_constraints = effect_row_constraints;
	delta->proposition_capacity = proposition_capacity;
	delta->derivation_candidate_capacity = proposition_capacity;
	delta->candidate_premise_capacity = candidate_premise_capacity;
	delta->match_motive_result_capacity = match_motive_result_capacity;
	delta->computation_constraint_capacity = computation_constraint_capacity;
	delta->effect_row_constraint_capacity = effect_row_constraint_capacity;
	delta->current_operation_id = PROTOTYPE_INVALID_ID;
}

void prototype_judgement_delta_set_intrinsic_environment(
	struct prototype_judgement_delta* delta,
	const struct prototype_intrinsic_environment* intrinsic_environment
) {
	if (!delta) {
		return;
	}
	delta->intrinsic_environment = intrinsic_environment;
}

void prototype_judgement_delta_set_solver_budget(
	struct prototype_judgement_delta* delta,
	uint64_t step_limit,
	uint64_t* steps_used,
	int* exhausted
) {
	if (!delta) {
		return;
	}
	delta->solver_step_limit = step_limit;
	delta->solver_steps_used = steps_used;
	delta->solver_exhausted = exhausted;
}

void prototype_judgement_delta_set_context(
	struct prototype_judgement_delta* delta,
	uint32_t context_id
) {
	if (!delta) {
		return;
	}
	delta->current_context_id = context_id;
}

void prototype_judgement_delta_set_operation(
	struct prototype_judgement_delta* delta,
	uint32_t operation_id
) {
	if (!delta) {
		return;
	}
	delta->current_operation_id = operation_id;
}

void prototype_judgement_delta_set_operation_usage_provider(
	struct prototype_judgement_delta* delta,
	void* context,
	int (*provider)(
		void* context,
		uint32_t operation_id,
		const struct prototype_usage_entry** p_entries,
		uint32_t* p_count
	)
) {
	if (!delta) {
		return;
	}
	delta->operation_usage_provider_context = context;
	delta->operation_usage_provider = provider;
}

void prototype_judgement_delta_set_context_store(
	struct prototype_judgement_delta* delta,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions
) {
	if (!delta) {
		return;
	}
	delta->contexts = contexts;
	delta->substitutions = substitutions;
}

void prototype_judgement_delta_set_operation_store(
	struct prototype_judgement_delta* delta,
	const struct prototype_operation_node* operations,
	size_t operation_count,
	const struct prototype_operation_match_case* operation_cases,
	size_t operation_case_count
) {
	if (!delta) {
		return;
	}
	delta->operations = operations;
	delta->operation_count = operation_count;
	delta->operation_cases = operation_cases;
	delta->operation_case_count = operation_case_count;
}
