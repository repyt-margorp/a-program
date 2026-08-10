#include "judgement.h"
#include "ast.h"

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
	int* p_polarity
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
		left->classifier == right->classifier;
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
	judgement->propositions[proposition_id].key_hash = key_hash;
	judgement->propositions[proposition_id].hash_next =
		judgement->proposition_index_heads[bucket];
	judgement->proposition_index_heads[bucket] = proposition_id;
	*p_proposition_id = proposition_id;
	return 0;
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

static int consume_solver_step(struct prototype_judgement_delta* delta) {
	if (!delta) {
		return -1;
	}
	if (!delta->solver_steps_used) {
		return 0;
	}
	if (*delta->solver_steps_used >= delta->solver_step_limit) {
		if (delta->solver_exhausted) {
			*delta->solver_exhausted = 1;
		}
		return 1;
	}
	(*delta->solver_steps_used)++;
	return 0;
}

static int term_has_tag(const struct prototype_term_db* terms, uint32_t term_id, int tag) {
	return terms && term_id < terms->term_count && terms->terms[term_id].tag == tag;
}

static int computation_effect_row_union(
	struct prototype_term_db* terms,
	const struct prototype_term_classifier_view* left,
	const struct prototype_term_classifier_view* right,
	uint32_t* p_row
) {
	if (!terms || !left || !right || !p_row ||
		left->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		right->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		left->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		right->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	return prototype_term_effect_row_union(terms, left->effect_row, right->effect_row, p_row);
}

static int computation_effect_row_is_union(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_classifier_view* result,
	const struct prototype_term_classifier_view* left,
	const struct prototype_term_classifier_view* right
) {
	if (!terms || !result || !left || !right ||
		result->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		left->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		right->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		result->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		left->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		right->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	unsigned result_effects;
	unsigned left_effects;
	unsigned right_effects;
	if (prototype_term_effect_row_closed_bits(terms, result->effect_row, &result_effects) == 0 &&
		prototype_term_effect_row_closed_bits(terms, left->effect_row, &left_effects) == 0 &&
		prototype_term_effect_row_closed_bits(terms, right->effect_row, &right_effects) == 0) {
		return result_effects == (left_effects | right_effects);
	}
	/* EFFECT_ROW_UNION normalizes the empty row away even when the other row is
	 * symbolic. The proof validator must use the same unit law as TermDB. */
	if (prototype_term_effect_row_closed_bits(terms, left->effect_row, &left_effects) == 0 &&
		left_effects == PROTOTYPE_EFFECT_OPERATION_LABEL_NONE) {
		int equal = 0;
		return prototype_term_view_shape_equal(
			terms, result->effect_row, right->effect_row, &equal
		) == 0 && equal;
	}
	if (prototype_term_effect_row_closed_bits(terms, right->effect_row, &right_effects) == 0 &&
		right_effects == PROTOTYPE_EFFECT_OPERATION_LABEL_NONE) {
		int equal = 0;
		return prototype_term_view_shape_equal(
			terms, result->effect_row, left->effect_row, &equal
		) == 0 && equal;
	}
	if (result->effect_row >= terms->term_count ||
		terms->terms[result->effect_row].tag != PROTOTYPE_TERM_EFFECT_ROW_UNION) {
		return 0;
	}
	const struct prototype_term* row = &terms->terms[result->effect_row];
	int left_equal = 0;
	int right_equal = 0;
	if (prototype_term_view_shape_equal(terms, row->as.effect_row_union.left,
			left->effect_row, &left_equal) != 0 ||
		prototype_term_view_shape_equal(terms, row->as.effect_row_union.right,
			right->effect_row, &right_equal) != 0) {
		return 0;
	}
	(void)type_declarations;
	return left_equal && right_equal;
}

static int effect_row_operation_latent_union(
	struct prototype_term_db* terms,
	uint32_t row,
	int operation_id,
	uint32_t* p_latent,
	int* p_found
) {
	if (!terms || !p_latent || !p_found || row >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[row];
	switch (term->tag) {
		case PROTOTYPE_TERM_EFFECT_LABEL:
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			if (term->as.effect_row_operation.operation_id != operation_id) {
				return 0;
			}
			if (!*p_found) {
				*p_latent = term->as.effect_row_operation.latent_row;
				*p_found = 1;
				return 0;
			}
			return prototype_term_effect_row_union(
				terms,
				*p_latent,
				term->as.effect_row_operation.latent_row,
				p_latent
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			int left_status = effect_row_operation_latent_union(
				terms,
				term->as.effect_row_union.left,
				operation_id,
				p_latent,
				p_found
			);
			return left_status != 0 ? left_status :
				effect_row_operation_latent_union(
					terms,
					term->as.effect_row_union.right,
					operation_id,
					p_latent,
					p_found
				);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return 1;
		default:
			return -1;
	}
}

int prototype_judgement_specialize_fold_operation_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t input_row,
	int operation_id,
	uint32_t classifier,
	uint32_t* p_specialized
) {
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	if (!terms || !type_declarations || !declaration || !p_specialized) {
		return -1;
	}
	*p_specialized = classifier;
	if (declaration->classifier_schema !=
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT) {
		return 0;
	}
	uint32_t latent_row = PROTOTYPE_INVALID_ID;
	int found_latent = 0;
	int latent_status = effect_row_operation_latent_union(
		terms, input_row, operation_id, &latent_row, &found_latent
	);
	if (latent_status != 0) {
		return latent_status;
	}
	if (!found_latent) {
		return 1;
	}
	uint32_t text;
	uint32_t latent_computation;
	uint32_t latent_argument;
	if (prototype_term_make_host_type(
			terms, PROTOTYPE_HOST_TYPE_TEXT, &text
		) != 0 || prototype_term_computation_type(
			terms, latent_row, text, &latent_computation
		) != 0 || prototype_term_thunk_type(
			terms, latent_computation, &latent_argument
		) != 0 || prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			classifier,
			latent_argument,
			p_specialized
		) != 0) {
		return -1;
	}
	return 0;
}

static int instantiate_fold_clause_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t generic_domain,
	uint32_t concrete_domain,
	uint32_t classifier,
	uint32_t* p_instantiated
) {
	if (!terms || !type_declarations || !p_instantiated ||
		generic_domain >= terms->term_count || concrete_domain >= terms->term_count ||
		classifier >= terms->term_count) {
		return -1;
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			terms, type_declarations, generic_domain, concrete_domain
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		*p_instantiated = classifier;
		return 0;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return -1;
	}
	if (terms->terms[generic_domain].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		terms->terms[concrete_domain].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	struct prototype_term_classifier_view generic;
	struct prototype_term_classifier_view concrete;
	if (prototype_judgement_classifier_view(
			terms,
			type_declarations,
			NULL,
			terms->terms[generic_domain].as.thunk_type.computation,
			&generic
		) != 0 || prototype_judgement_classifier_view(
			terms,
			type_declarations,
			NULL,
			terms->terms[concrete_domain].as.thunk_type.computation,
			&concrete
		) != 0 || generic.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		concrete.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		generic.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		concrete.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		generic.effect_row >= terms->term_count ||
		terms->terms[generic.effect_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, generic.result, concrete.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return prototype_term_graph_substitute_bound_var(
		terms,
		type_declarations,
		classifier,
		terms->terms[generic.effect_row].as.effect_row_var.binding_id,
		concrete.effect_row,
		p_instantiated
	);
}

/* Residual subtraction is structural. Higher-order operation atoms are
 * removable by their outer operation identity while retaining opaque latent
 * rows on unhandled atoms. Open rows remain solver obligations. */
static int computation_fold_residual_row(
	struct prototype_term_db* terms,
	const struct prototype_term_classifier_view* input,
	const struct prototype_term_classifier_view* operation,
	uint32_t* p_residual
) {
	unsigned operation_effects;
	if (!terms || !input || !operation || !p_residual ||
		input->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		operation->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		prototype_term_effect_row_closed_bits(
			terms, operation->effect_row, &operation_effects
		) != 0) {
		return 1;
	}
	return prototype_term_effect_row_residual(
		terms, input->effect_row, operation_effects, p_residual
	);
}

static int term_exists(const struct prototype_term_db* terms, uint32_t term_id) {
	return terms && term_id < terms->term_count;
}

static int add_effect_row_constraint(
	struct prototype_judgement_delta* delta,
	int kind,
	uint32_t subject,
	uint32_t result_row,
	const uint32_t* operands,
	uint32_t operand_count
) {
	if (!delta || !delta->effect_row_constraints || !operands ||
		result_row == PROTOTYPE_INVALID_ID || operand_count == 0 ||
		operand_count > PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_MAX_OPERANDS) {
		return -1;
	}
	for (uint32_t i = 0; i < operand_count; ++i) {
		if (operands[i] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	for (size_t i = 0; i < delta->effect_row_constraint_count; ++i) {
		const struct prototype_judgement_effect_row_constraint* constraint =
			&delta->effect_row_constraints[i];
		if (constraint->kind != kind || constraint->subject != subject ||
			constraint->result_row != result_row ||
			constraint->operand_count != operand_count) {
			continue;
		}
		int equal = 1;
		for (uint32_t j = 0; j < operand_count; ++j) {
			if (constraint->operands[j] != operands[j]) {
				equal = 0;
				break;
			}
		}
		if (equal) {
			return 0;
		}
	}
	if (reserve_slot(
			delta->effect_row_constraint_count, delta->effect_row_constraint_capacity
		) != 0) {
		return -1;
	}
	struct prototype_judgement_effect_row_constraint* constraint =
		&delta->effect_row_constraints[delta->effect_row_constraint_count++];
	*constraint = (struct prototype_judgement_effect_row_constraint){
			.kind = kind,
			.subject = subject,
			.result_row = result_row,
			.operand_count = operand_count,
			.solved = 0
		};
	memcpy(constraint->operands, operands, operand_count * sizeof(*operands));
	return 0;
}

/* Solve only equations whose rows are fully known. A symbolic equation stays
 * recorded instead of being approximated by an empty capability set. */
static int solve_effect_row_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms
) {
	if (!delta || !terms) {
		return -1;
	}
	for (size_t i = 0; i < delta->effect_row_constraint_count; ++i) {
		struct prototype_judgement_effect_row_constraint* constraint =
			&delta->effect_row_constraints[i];
		unsigned result;
		if (prototype_term_effect_row_closed_bits(
				terms, constraint->result_row, &result
			) != 0) {
			constraint->solved = 0;
			continue;
		}
		unsigned operand_bits[
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_MAX_OPERANDS
		];
		int closed = 1;
		for (uint32_t j = 0; j < constraint->operand_count; ++j) {
			if (prototype_term_effect_row_closed_bits(
					terms, constraint->operands[j], &operand_bits[j]
				) != 0) {
				closed = 0;
				break;
			}
		}
		if (!closed) {
			constraint->solved = 0;
			continue;
		}
		if (constraint->kind == PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN) {
			unsigned joined = 0;
			for (uint32_t j = 0; j < constraint->operand_count; ++j) {
				joined |= operand_bits[j];
			}
			if (result != joined) {
				return -1;
			}
		} else if (constraint->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_RESIDUAL) {
			if (constraint->operand_count != 2 ||
				(operand_bits[0] & operand_bits[1]) != operand_bits[1] ||
				result != (operand_bits[0] & ~operand_bits[1])) {
				return -1;
			}
		} else if (constraint->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION) {
			if (constraint->operand_count != 2 ||
				(operand_bits[0] & operand_bits[1]) != operand_bits[0] ||
				result != operand_bits[1]) {
				return -1;
			}
		} else {
			return -1;
		}
		constraint->solved = 1;
	}
	return 0;
}

static int term_is_universe_var(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_has_tag(terms, term_id, PROTOTYPE_TERM_UNIVERSE_VAR);
}

static int term_is_primitive_text(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_has_tag(terms, term_id, PROTOTYPE_TERM_PRIMITIVE_TEXT);
}

static int term_is_primitive_int(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_has_tag(terms, term_id, PROTOTYPE_TERM_PRIMITIVE_INT);
}

static int term_is_primitive_int64(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_has_tag(terms, term_id, PROTOTYPE_TERM_PRIMITIVE_INT64);
}

static int term_is_primitive_integer(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_is_primitive_int(terms, term_id) ||
		term_is_primitive_int64(terms, term_id);
}

static int term_is_host_primitive(const struct prototype_term_db* terms, uint32_t term_id) {
	if (!term_exists(terms, term_id)) {
		return 0;
	}
	int host_type;
	return prototype_term_host_type_from_term_tag(terms->terms[term_id].tag, &host_type) == 0;
}

static int int_literal_fits_int32(int64_t value) {
	return value >= INT32_MIN && value <= INT32_MAX;
}

static int add_delta_relation(
	struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind
);
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
);
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
);
static int add_complete_delta_relation(
	struct prototype_judgement_delta* delta,
	const struct prototype_judgement_proposition* candidate_relation,
	const struct prototype_judgement_derivation_candidate* candidate_proof
);
static void initialize_proof_premises(
	struct prototype_judgement_derivation_candidate* proof,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
);
static void initialize_proof_rule_parameters(
	struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* premises
);
static int add_match_motive_result(
	struct prototype_judgement_delta* delta,
	uint32_t match_term,
	uint32_t classifier
);
static void remove_match_motive_results_normalization_equal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t classifier
);
static int prototype_judgement_delta_add_conversion(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected
);
static int prototype_judgement_delta_expand_match_motive_with_premises(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
);
static int collect_subject_classifiers(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
);
static int collect_subject_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t subject,
	struct prototype_judgement_selected_evidence* evidence,
	uint32_t evidence_capacity,
	uint32_t* p_evidence_count
);
static int classifier_returns_owner(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t owner
);
static int classifier_list_contains_normalization_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const uint32_t* classifiers,
	uint32_t classifier_count,
	uint32_t candidate
);
static int match_motive_result_classifier(
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t classifier
);

/* Operation judgements retain TYPE_VIEW nodes. The application rule validates
 * their shared computation against the enclosed core APP. */
static int term_core_app(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	const struct prototype_term** p_app
) {
	if (!terms || !p_app || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t current = term_id;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		current = terms->terms[current].as.type_view.core;
	}
	if (current >= terms->term_count ||
		terms->terms[current].tag != PROTOTYPE_TERM_APP) {
		return -1;
	}
	*p_app = &terms->terms[current];
	return 0;
}

static int collect_subject_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t subject,
	struct prototype_judgement_selected_evidence* evidence,
	uint32_t evidence_capacity,
	uint32_t* p_evidence_count
) {
	if (!delta || !evidence || !p_evidence_count) {
		return -1;
	}
	*p_evidence_count = 0;
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_proposition* candidates =
			source == 0 ? delta->propositions :
			(delta->db ? delta->db->propositions : NULL);
		const struct prototype_judgement_derivation_candidate* derivations =
			source == 0 ? delta->derivation_candidates :
			(delta->db ? delta->db->derivation_candidates : NULL);
		size_t candidate_count = source == 0 ? delta->proposition_count :
			(delta->db ? delta->db->proposition_count : 0);
		size_t derivation_count = source == 0 ?
			delta->derivation_candidate_count :
			(delta->db ? delta->db->derivation_candidate_count : 0);
		for (size_t i = 0; i < candidate_count; ++i) {
			const struct prototype_judgement_proposition* candidate =
				&candidates[i];
			if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				candidate->subject != subject ||
				candidate_has_derivation_other_than(
					candidates,
					candidate_count,
					derivations,
					derivation_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
				) != 1) {
				continue;
			}
			struct prototype_judgement_selected_evidence current;
			selected_evidence_from_candidate(candidate, &current);
			int duplicate = 0;
			for (uint32_t j = 0; j < *p_evidence_count; ++j) {
				if (selected_evidence_equal(&evidence[j], &current)) {
					duplicate = 1;
					break;
				}
			}
			if (duplicate) {
				continue;
			}
			if (*p_evidence_count >= evidence_capacity) {
				return -1;
			}
			evidence[(*p_evidence_count)++] = current;
		}
	}
	return 0;
}

static struct prototype_term_conversion_result classifier_kernel_conversion(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
) {
	struct prototype_term_conversion_result result;
	memset(&result, 0, sizeof(result));
	result.status = PROTOTYPE_TERM_CONVERSION_INVALID;
	result.reason = PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH;
	result.profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF;
	result.left = expected;
	result.right = actual;
	result.left_observation = PROTOTYPE_INVALID_ID;
	result.right_observation = PROTOTYPE_INVALID_ID;
	result.step_limit = UINT64_MAX;
	result.graph_revision = terms ? terms->normalization_graph_revision : 0;
	if (!term_exists(terms, expected) || !term_exists(terms, actual)) {
		return result;
	}
	/* Judgemental equality is reflexive even when WHNF expansion would enter a
	 * guarded recursive motive. Normalization is evidence for distinct nodes,
	 * not a prerequisite for a node being equal to itself. */
	if (expected == actual) {
		result.status = PROTOTYPE_TERM_CONVERSION_EQUAL;
		result.reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
		result.left_observation = expected;
		result.right_observation = actual;
		return result;
	}
	if (term_is_universe_var(terms, expected) && term_is_universe_var(terms, actual)) {
		result.status = terms->terms[expected].as.universe_var.level_var ==
			terms->terms[actual].as.universe_var.level_var ?
			PROTOTYPE_TERM_CONVERSION_EQUAL : PROTOTYPE_TERM_CONVERSION_NOT_EQUAL;
		result.reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
		result.left_observation = expected;
		result.right_observation = actual;
		return result;
	}
	if (prototype_term_compare_for_conversion(
			terms,
			type_declarations,
			definitions,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			expected,
			actual,
			UINT64_MAX,
			&result
		) != 0) {
		result.status = PROTOTYPE_TERM_CONVERSION_INVALID;
		result.reason = PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH;
	}
	return result;
}

struct prototype_term_conversion_result prototype_judgement_classifier_conversion(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_conversion(
		terms,
		type_declarations,
		NULL,
		expected,
		actual
	);
}

struct prototype_term_conversion_result
prototype_judgement_classifier_conversion_with_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_conversion(
		terms,
		type_declarations,
		definitions,
		expected,
		actual
	);
}

static int kernel_conversion_profile_is_admitted(int profile) {
	return profile == PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF ||
		profile == PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF ||
		profile == PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF;
}

int prototype_judgement_kernel_conversion_goal_validate(
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	const struct prototype_kernel_conversion_goal* goal,
	int require_carrier
) {
	if (!contexts || !terms || !goal ||
		goal->context_id >= contexts->context_count ||
		goal->left_term >= terms->term_count ||
		goal->right_term >= terms->term_count ||
		!kernel_conversion_profile_is_admitted(goal->normalization_profile) ||
		(require_carrier && goal->carrier_classifier == PROTOTYPE_INVALID_ID) ||
		(goal->carrier_classifier != PROTOTYPE_INVALID_ID &&
		 goal->carrier_classifier >= terms->term_count)) {
		return -1;
	}
	return 0;
}

int prototype_judgement_kernel_conversion_goal_execute(
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_kernel_conversion_goal* goal,
	int require_carrier
) {
	if (!type_declarations ||
		prototype_judgement_kernel_conversion_goal_validate(
			contexts, terms, goal, require_carrier
		) != 0) {
		return -1;
	}
	return prototype_term_compare_for_conversion(
		terms,
		type_declarations,
		definitions,
		goal->normalization_profile,
		goal->left_term,
		goal->right_term,
		goal->step_limit,
		&goal->result
	);
}


static int classifier_conversion_decision(
	struct prototype_term_conversion_result conversion,
	int* p_equal
) {
	if (!p_equal) {
		return -1;
	}
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		*p_equal = 1;
		return 0;
	}
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		*p_equal = 0;
		return 0;
	}
	return -1;
}

int prototype_judgement_classifier_value_whnf(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_value_classifier
) {
	if (!terms || !type_declarations || !p_value_classifier ||
		classifier >= terms->term_count ||
		prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier
		) != 0 || classifier >= terms->term_count) {
		return -1;
	}
	if (terms->terms[classifier].tag == PROTOTYPE_TERM_RETURN) {
		classifier = terms->terms[classifier].as.return_term.value;
	}
	*p_value_classifier = classifier;
	return 0;
}

int prototype_judgement_classifier_reference_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
) {
	if (prototype_judgement_classifier_value_whnf(
			terms, type_declarations, expected, &expected
		) != 0 || prototype_judgement_classifier_value_whnf(
			terms, type_declarations, actual, &actual
		) != 0) {
		return 0;
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			terms, type_declarations, expected, actual
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return 0;
	}
	const struct prototype_term* left = &terms->terms[expected];
	const struct prototype_term* right = &terms->terms[actual];
	if (left->tag == PROTOTYPE_TERM_EXTERNAL_REF &&
		right->tag == PROTOTYPE_TERM_TYPE_VIEW) {
		return left->as.external_ref.name.namespace_symbol_id ==
				right->as.type_view.identity.namespace_symbol_id &&
			left->as.external_ref.name.name_symbol_id ==
				right->as.type_view.identity.name_symbol_id;
	}
	if (left->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		right->tag == PROTOTYPE_TERM_EXTERNAL_REF) {
		return left->as.type_view.identity.namespace_symbol_id ==
				right->as.external_ref.name.namespace_symbol_id &&
			left->as.type_view.identity.name_symbol_id ==
				right->as.external_ref.name.name_symbol_id;
	}
	return 0;
}

static int classifier_kernel_whnf(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t term_id,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret ||
		term_id >= terms->term_count) {
		return -1;
	}

	uint32_t evaluated;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			definitions,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			term_id,
			&evaluated
		) != 0 ||
		evaluated >= terms->term_count) {
		return -1;
	}

	*p_ret = evaluated;
	return 0;
}

static int classifier_view_with_depth(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	struct prototype_term_classifier_view* p_ret,
	uint32_t depth
) {
	uint32_t whnf;
	if (!p_ret || depth == 0 || classifier_kernel_whnf(
				terms,
			type_declarations,
			definitions,
			classifier,
				&whnf
			) != 0) {
		return -1;
	}
	/* Row schemes classify the same runtime computation as their body. The
	 * binder is only used when an application specializes a latent row. */
	for (uint32_t depth = 0;
		depth < 32 && whnf < terms->term_count &&
		terms->terms[whnf].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL;
		++depth) {
		if (classifier_kernel_whnf(
				terms,
				type_declarations,
				definitions,
				terms->terms[whnf].as.effect_row_forall.body,
				&whnf
			) != 0) {
			return -1;
		}
	}
	if (whnf >= terms->term_count ||
		terms->terms[whnf].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		return -1;
	}
	if (terms->terms[whnf].tag == PROTOTYPE_TERM_MATCH) {
		const struct prototype_term* match = &terms->terms[whnf];
		if (match->as.match.case_count == 0 || match->as.match.case_count > 64 ||
			match->as.match.first_case + match->as.match.case_count > terms->case_count) {
			return -1;
		}
		struct prototype_match_case_input result_cases[64];
		uint32_t effect_row = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			const struct prototype_match_case* match_case =
				&terms->cases[match->as.match.first_case + i];
			struct prototype_term_classifier_view branch_view;
			if (match_case->first_binder + match_case->binder_count >
					terms->case_binder_count || classifier_view_with_depth(
					terms,
					type_declarations,
					definitions,
					match_case->body,
					&branch_view,
					depth - 1
				) != 0 || branch_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				branch_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				return prototype_term_classifier_view(terms, whnf, p_ret);
			}
			result_cases[i].case_label_symbol_id =
				terms->case_label_symbols[match->as.match.first_case + i];
			result_cases[i].constructor_owner = match_case->constructor_owner;
			result_cases[i].constructor_id = match_case->constructor_id;
			result_cases[i].binders = &terms->case_binders[match_case->first_binder];
			result_cases[i].binder_count = match_case->binder_count;
			result_cases[i].body = branch_view.result;
			if (effect_row == PROTOTYPE_INVALID_ID) {
				effect_row = branch_view.effect_row;
			} else if (prototype_term_effect_row_union(
					terms, effect_row, branch_view.effect_row, &effect_row
				) != 0) {
				return -1;
			}
		}
		uint32_t result;
		if (prototype_term_match(
				terms,
				match->as.match.scrutinee,
				result_cases,
				match->as.match.case_count,
				&result
			) != 0) {
			return -1;
		}
		memset(p_ret, 0, sizeof(*p_ret));
		p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
		p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
		p_ret->effect_row = effect_row;
		p_ret->result = result;
		if (prototype_term_effect_row_closed_bits(
				terms, effect_row, &p_ret->effects
			) != 0) {
			p_ret->effects = PROTOTYPE_EFFECT_OPERATION_LABEL_NONE;
		}
		return 0;
	}
	return prototype_term_classifier_view(terms, whnf, p_ret);
}

int prototype_judgement_classifier_view(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	struct prototype_term_classifier_view* p_ret
) {
	return classifier_view_with_depth(
		terms, type_declarations, definitions, classifier, p_ret, 256
	);
}

int prototype_judgement_claim_category(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_operation_graph* operations,
	uint32_t claim_id,
	int* p_category
) {
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	if (!claim || !terms || !type_declarations || !p_category) {
		return -1;
	}
	if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) {
		*p_category = PROTOTYPE_JUDGEMENT_CATEGORY_TYPE;
		return 0;
	}
	if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION) {
		int polarity;
		if (!operations || prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_id != prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id ||
			judgement_operation_expected_polarity(
				terms,
				type_declarations,
				operations,
				prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id,
				&polarity
			) != 0) {
			return -1;
		}
		*p_category = polarity == PROTOTYPE_OPERATION_POLARITY_COMPUTATION ?
			PROTOTYPE_JUDGEMENT_CATEGORY_COMPUTATION :
			PROTOTYPE_JUDGEMENT_CATEGORY_VALUE;
		return 0;
	}
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, definitions, prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier, &view
		) != 0) {
		return -1;
	}
	*p_category = view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION ?
		PROTOTYPE_JUDGEMENT_CATEGORY_COMPUTATION :
		PROTOTYPE_JUDGEMENT_CATEGORY_VALUE;
	return 0;
}

static int classifier_kernel_whnf_no_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t* p_ret
) {
	return classifier_kernel_whnf(terms, type_declarations, NULL, term_id, p_ret);
}

static int lookup_relation_classifier(
	const struct prototype_judgement_proposition* relations,
	size_t proposition_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_candidate_count,
	uint32_t subject,
	uint32_t* p_classifier,
	int include_conversion
) {
	if (!relations || !p_classifier) {
		return -1;
	}
	for (size_t i = proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation = &relations[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			(include_conversion || candidate_has_derivation_other_than(
				relations,
				proposition_count,
				derivations,
				derivation_candidate_count,
				(uint32_t)(i - 1),
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			) == 1)) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	return -1;
}

static void selected_evidence_from_candidate(
	const struct prototype_judgement_proposition* candidate,
	struct prototype_judgement_selected_evidence* evidence
) {
	int authority_kind;
	uint32_t authority_id;
	candidate_claim_authority(candidate, &authority_kind, &authority_id);
	*evidence = (struct prototype_judgement_selected_evidence){
		.kind = candidate->kind,
		.authority_kind = authority_kind,
		.authority_id = authority_id,
		.context_id = candidate->context_id,
		.operation_id = authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
			candidate->operation_id : PROTOTYPE_INVALID_ID,
		.subject = candidate->subject,
		.classifier = candidate->classifier
	};
}

static int selected_evidence_equal(
	const struct prototype_judgement_selected_evidence* left,
	const struct prototype_judgement_selected_evidence* right
) {
	return left && right && left->kind == right->kind &&
		left->authority_kind == right->authority_kind &&
		left->authority_id == right->authority_id &&
		left->context_id == right->context_id &&
		left->operation_id == right->operation_id &&
		left->subject == right->subject &&
		left->classifier == right->classifier;
}

static int merge_exact_selected_evidence(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	uint32_t operation_id,
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
			candidate->operation_id != operation_id ||
			candidate->context_id != context_id ||
			candidate->subject != subject ||
			candidate->classifier != classifier) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
		} else if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

int prototype_judgement_delta_select_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_exact_selected_evidence(
		delta->propositions,
		delta->proposition_count,
		operation_id,
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
		status = merge_exact_selected_evidence(
			delta->db->propositions,
			delta->db->proposition_count,
			operation_id,
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

int prototype_judgement_select_evidence(
	const struct prototype_judgement_db* judgement,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!judgement || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_exact_selected_evidence(
		judgement->propositions,
		judgement->proposition_count,
		operation_id,
		context_id,
		subject,
		classifier,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	return found ? 0 : 1;
}

static int merge_selected_evidence_from_candidates(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t subject,
	int include_conversion,
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
			candidate->subject != subject ||
			(!include_conversion && candidate_has_derivation_other_than(
				candidates,
				candidate_count,
				derivations,
				derivation_count,
				(uint32_t)i,
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			) != 1)) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
			continue;
		}
		if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

/* Returns 0 for one semantic Claim, 1 for no Claim, 2 for ambiguous Claims,
 * and -1 for malformed input. Multiple Derivations of the same Claim do not
 * make selection ambiguous. */
static int lookup_delta_selected_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t subject,
	int include_conversion,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_selected_evidence_from_candidates(
		delta->propositions,
		delta->proposition_count,
		delta->derivation_candidates,
		delta->derivation_candidate_count,
		subject,
		include_conversion,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	if (delta->db) {
		status = merge_selected_evidence_from_candidates(
			delta->db->propositions,
			delta->db->proposition_count,
			delta->db->derivation_candidates,
			delta->db->derivation_candidate_count,
			subject,
			include_conversion,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
	}
	return found ? 0 : 1;
}

static int merge_selected_evidence_with_proof_kind(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t subject,
	int proof_kind,
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
			candidate->subject != subject || candidate_has_derivation_kind(
				candidates,
				candidate_count,
				derivations,
				derivation_count,
				(uint32_t)i,
				proof_kind
			) != 1) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
		} else if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

static int lookup_delta_selected_evidence_with_proof_kind(
	const struct prototype_judgement_delta* delta,
	uint32_t subject,
	int proof_kind,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_selected_evidence_with_proof_kind(
		delta->propositions,
		delta->proposition_count,
		delta->derivation_candidates,
		delta->derivation_candidate_count,
		subject,
		proof_kind,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	if (delta->db) {
		status = merge_selected_evidence_with_proof_kind(
			delta->db->propositions,
			delta->db->proposition_count,
			delta->db->derivation_candidates,
			delta->db->derivation_candidate_count,
			subject,
			proof_kind,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
	}
	return found ? 0 : 1;
}

static int merge_selected_evidence_normalization_equal(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t expected,
	int exact_only,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if ((!candidates && candidate_count != 0) || !terms || !type_declarations ||
		!selected || !p_found) {
		return -1;
	}
	for (size_t i = 0; i < candidate_count; ++i) {
		const struct prototype_judgement_proposition* candidate =
			&candidates[i];
		if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate->context_id != context_id ||
			candidate->subject != subject) {
			continue;
		}
		int matches = candidate->classifier == expected;
		if (!matches && !exact_only) {
			int equal;
			if (classifier_conversion_decision(
					prototype_judgement_classifier_conversion(
						terms,
						type_declarations,
						expected,
						candidate->classifier
					),
					&equal
				) != 0) {
				return -1;
			}
			matches = equal;
		}
		if (!matches) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
		} else if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

static int lookup_delta_selected_evidence_normalization_equal(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !terms || !type_declarations || !selected ||
		!term_exists(terms, subject) || !term_exists(terms, expected)) {
		return -1;
	}
	for (int exact_only = 1; exact_only >= 0; --exact_only) {
		int found = 0;
		int status = merge_selected_evidence_normalization_equal(
			delta->propositions,
			delta->proposition_count,
			terms,
			type_declarations,
			delta->current_context_id,
			subject,
			expected,
			exact_only,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
		if (delta->db) {
			status = merge_selected_evidence_normalization_equal(
				delta->db->propositions,
				delta->db->proposition_count,
				terms,
				type_declarations,
				delta->current_context_id,
				subject,
				expected,
				exact_only,
				selected,
				&found
			);
			if (status != 0) {
				return status;
			}
		}
		if (found) {
			return 0;
		}
	}
	return 1;
}

static int lookup_delta_prior_evidence_normalization_equal(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !terms || !type_declarations || !selected ||
		!term_exists(terms, subject) || !term_exists(terms, expected)) {
		return -1;
	}
	for (int exact_only = 1; exact_only >= 0; --exact_only) {
		int found = 0;
		int status = merge_selected_evidence_normalization_equal(
			delta->propositions,
			delta->proposition_count,
			terms,
			type_declarations,
			delta->current_context_id,
			subject,
			expected,
			exact_only,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
		if (found) {
			return 0;
		}
	}
	if (!delta->db) {
		return 1;
	}
	for (int exact_only = 1; exact_only >= 0; --exact_only) {
		int found = 0;
		int status = merge_selected_evidence_normalization_equal(
			delta->db->propositions,
			delta->db->proposition_count,
			terms,
			type_declarations,
			delta->current_context_id,
			subject,
			expected,
			exact_only,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
		if (found) {
			return 0;
		}
	}
	return 1;
}

static int lookup_classifier(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!judgement) {
		return -1;
	}
	(void)terms;
	return lookup_relation_classifier(
		judgement->propositions,
		judgement->proposition_count,
		judgement->derivation_candidates,
		judgement->derivation_candidate_count,
		subject,
		p_classifier,
		1
	);
}

static int lookup_delta_proven_classifier(
	const struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!delta || !p_classifier) {
		return -1;
	}
	(void)terms;
	if (lookup_relation_classifier(
		delta->propositions,
		delta->proposition_count,
		delta->derivation_candidates,
		delta->derivation_candidate_count,
		subject,
		p_classifier,
		1
	) == 0) {
		return 0;
	}
	return lookup_classifier(delta->db, terms, subject, p_classifier);
}

static int merge_app_elim_argument_evidence(
	const struct prototype_judgement_proposition* relations,
	size_t proposition_count,
	const struct prototype_judgement_derivation_candidate* proofs,
	size_t derivation_candidate_count,
	uint32_t application,
	uint32_t application_classifier,
	uint32_t argument,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if (!relations || !proofs || !selected || !p_found) {
		return -1;
	}
	for (size_t i = 0; i < proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &relations[i];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			relation->subject != application ||
			relation->classifier != application_classifier) {
			continue;
		}
		uint32_t cursor = 0;
		uint32_t derivation_id;
		while (prototype_judgement_candidate_derivation_next(
			relations,
			proposition_count,
			proofs,
			derivation_candidate_count,
			(uint32_t)i,
			&cursor,
			&derivation_id
		) == 0) {
			const struct prototype_judgement_derivation_candidate* proof =
				&proofs[derivation_id];
			if (proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM ||
				proof->premise_count != 2 ||
				proof->premises[1].proposition.subject != argument) {
				continue;
			}
			struct prototype_judgement_selected_evidence current = {
				.kind = proof->premises[1].proposition.kind,
				.authority_kind = proof->premises[1].proposition.authority_kind,
				.authority_id = proof->premises[1].proposition.authority_id,
				.context_id = proof->premises[1].proposition.context_id,
				.operation_id = proof->premises[1].proposition.authority_kind ==
					PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
					proof->premises[1].proposition.operation_id : PROTOTYPE_INVALID_ID,
				.subject = proof->premises[1].proposition.subject,
				.classifier = proof->premises[1].proposition.classifier
			};
			if (!*p_found) {
				*selected = current;
				*p_found = 1;
			} else if (!selected_evidence_equal(selected, &current)) {
				return 2;
			}
		}
	}
	return 0;
}

static int lookup_delta_app_elim_argument_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t application,
	uint32_t application_classifier,
	uint32_t argument,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_app_elim_argument_evidence(
		delta->propositions,
		delta->proposition_count,
		delta->derivation_candidates,
		delta->derivation_candidate_count,
		application,
		application_classifier,
		argument,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	if (delta->db) {
		status = merge_app_elim_argument_evidence(
			delta->db->propositions,
			delta->db->proposition_count,
			delta->db->derivation_candidates,
			delta->db->derivation_candidate_count,
			application,
			application_classifier,
			argument,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
	}
	return found ? 0 : 1;
}

static int pi_parts(
	const struct prototype_term_db* terms,
	uint32_t pi_term,
	uint32_t* p_domain,
	uint32_t* p_codomain_family
) {
	if (!terms || !p_domain || !p_codomain_family ||
		pi_term >= terms->term_count ||
		terms->terms[pi_term].tag != PROTOTYPE_TERM_PI ||
		terms->terms[pi_term].as.pi.codomain_family >= terms->term_count ||
		prototype_term_pure_family_lambda(
			terms, terms->terms[pi_term].as.pi.codomain_family, NULL
		) != 0) {
		return -1;
	}
	*p_domain = terms->terms[pi_term].as.pi.domain;
	*p_codomain_family = terms->terms[pi_term].as.pi.codomain_family;
	return 0;
}

int prototype_judgement_pi_parts(
	const struct prototype_term_db* terms,
	uint32_t pi_term,
	uint32_t* p_domain,
	uint32_t* p_codomain_family
) {
	return pi_parts(terms, pi_term, p_domain, p_codomain_family);
}

static int classifier_kernel_as_pi(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	uint32_t* p_pi,
	uint32_t* p_domain,
	uint32_t* p_codomain_family
) {
	if (!terms || !type_declarations || !p_domain || !p_codomain_family) {
		return -1;
	}
	uint32_t normalized_classifier;
	if (classifier_kernel_whnf(
			terms,
			type_declarations,
			definitions,
			classifier,
			&normalized_classifier
		) != 0) {
		return -1;
	}
	for (uint32_t depth = 0;
		depth < 32 && normalized_classifier < terms->term_count &&
		terms->terms[normalized_classifier].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL;
		++depth) {
		normalized_classifier =
			terms->terms[normalized_classifier].as.effect_row_forall.body;
		if (classifier_kernel_whnf(
				terms, type_declarations, definitions,
				normalized_classifier, &normalized_classifier
			) != 0) {
			return -1;
		}
	}
	if (pi_parts(terms, normalized_classifier, p_domain, p_codomain_family) != 0) {
		return 1;
	}
	if (p_pi) {
		*p_pi = normalized_classifier;
	}
	return 0;
}

/* Effect-row quantification is classifier-only. Lambda introduction proves the
 * underlying Pi; the surrounding implicit row binders are checked by the
 * elaborator and erased from the runtime term. */
static int classifier_kernel_strip_effect_row_foralls(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 32; ++depth) {
		if (classifier_kernel_whnf(
				terms, type_declarations, definitions, classifier, &classifier
			) != 0 || classifier >= terms->term_count) {
			return -1;
		}
		if (terms->terms[classifier].tag != PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
			*p_ret = classifier;
			return 0;
		}
		classifier = terms->terms[classifier].as.effect_row_forall.body;
	}
	return -1;
}

static int find_effect_row_argument(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual,
	uint32_t row_binder,
	uint32_t depth,
	uint32_t* p_row
) {
	if (!terms || !type_declarations || !p_row || depth > 64 ||
		expected >= terms->term_count || actual >= terms->term_count) {
		return -1;
	}
	uint32_t expected_whnf;
	uint32_t actual_whnf;
	if (classifier_kernel_whnf(
			terms, type_declarations, NULL, expected, &expected_whnf
		) != 0 || classifier_kernel_whnf(
			terms, type_declarations, NULL, actual, &actual_whnf
		) != 0 || expected_whnf >= terms->term_count ||
		actual_whnf >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* left = &terms->terms[expected_whnf];
	const struct prototype_term* right = &terms->terms[actual_whnf];
	if (left->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR &&
		left->as.effect_row_var.binding_id == row_binder) {
		*p_row = actual_whnf;
		return 1;
	}
	if (left->tag != right->tag) {
		return 0;
	}
	int status;
	switch (left->tag) {
		case PROTOTYPE_TERM_THUNK_TYPE:
			return find_effect_row_argument(
				terms, type_declarations, left->as.thunk_type.computation,
				right->as.thunk_type.computation, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			status = find_effect_row_argument(
				terms, type_declarations, left->as.computation_type.label,
				right->as.computation_type.label, row_binder, depth + 1, p_row
			);
			return status != 0 ? status : find_effect_row_argument(
				terms, type_declarations, left->as.computation_type.result,
				right->as.computation_type.result, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_PI:
			status = find_effect_row_argument(
				terms, type_declarations, left->as.pi.domain, right->as.pi.domain,
				row_binder, depth + 1, p_row
			);
			return status != 0 ? status : find_effect_row_argument(
				terms, type_declarations, left->as.pi.codomain_family,
				right->as.pi.codomain_family, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_LAMBDA:
			return find_effect_row_argument(
				terms, type_declarations, left->as.lambda.body, right->as.lambda.body,
				row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_RETURN:
			return find_effect_row_argument(
				terms, type_declarations, left->as.return_term.value,
				right->as.return_term.value, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			status = find_effect_row_argument(
				terms, type_declarations, left->as.effect_row_union.left,
				right->as.effect_row_union.left, row_binder, depth + 1, p_row
			);
			return status != 0 ? status : find_effect_row_argument(
				terms, type_declarations, left->as.effect_row_union.right,
				right->as.effect_row_union.right, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			if (left->as.effect_row_forall.binding_id == row_binder) {
				return 0;
			}
			return find_effect_row_argument(
				terms, type_declarations, left->as.effect_row_forall.body,
				right->as.effect_row_forall.body, row_binder, depth + 1, p_row
			);
		default:
			return 0;
	}
}

int prototype_judgement_specialize_effect_rows_for_argument(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret ||
		function_classifier >= terms->term_count || argument_classifier >= terms->term_count) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 16; ++depth) {
		uint32_t normalized_function;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, function_classifier, &normalized_function
			) != 0 || normalized_function >= terms->term_count) {
			return -1;
		}
		if (terms->terms[normalized_function].tag != PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
			*p_ret = function_classifier;
			return 0;
		}
		const struct prototype_term* quantified = &terms->terms[normalized_function];
		uint32_t unquantified_body;
		uint32_t pi;
		uint32_t domain;
		uint32_t family;
		if (classifier_kernel_strip_effect_row_foralls(
				terms,
				type_declarations,
				NULL,
				quantified->as.effect_row_forall.body,
				&unquantified_body
			) != 0 || classifier_kernel_as_pi(
				terms, type_declarations, NULL, unquantified_body,
				&pi, &domain, &family
			) != 0) {
			return 1;
		}
		uint32_t expected_thunk;
		uint32_t actual_thunk;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, domain, &expected_thunk
			) != 0 || classifier_kernel_whnf(
				terms, type_declarations, NULL, argument_classifier, &actual_thunk
			) != 0 || expected_thunk >= terms->term_count ||
			actual_thunk >= terms->term_count ||
			terms->terms[expected_thunk].tag != PROTOTYPE_TERM_THUNK_TYPE ||
			terms->terms[actual_thunk].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			return 1;
		}
		uint32_t expected_computation =
			terms->terms[expected_thunk].as.thunk_type.computation;
		uint32_t actual_computation =
			terms->terms[actual_thunk].as.thunk_type.computation;
		uint32_t expected_computation_whnf;
		uint32_t actual_computation_whnf;
		if (classifier_kernel_whnf(
				terms,
				type_declarations,
				NULL,
				expected_computation,
				&expected_computation_whnf
			) != 0 ||
			classifier_kernel_whnf(
				terms,
				type_declarations,
				NULL,
				actual_computation,
				&actual_computation_whnf
			) != 0 ||
			expected_computation_whnf >= terms->term_count ||
			actual_computation_whnf >= terms->term_count) {
			return -1;
		}
		if (terms->terms[expected_computation_whnf].tag ==
				PROTOTYPE_TERM_COMPUTATION_TYPE &&
			terms->terms[actual_computation_whnf].tag ==
				PROTOTYPE_TERM_COMPUTATION_TYPE) {
			uint32_t row;
			int row_status = find_effect_row_argument(
				terms,
				type_declarations,
				expected_thunk,
				actual_thunk,
				quantified->as.effect_row_forall.binding_id,
				0,
				&row
			);
			if (row_status <= 0 || prototype_term_graph_substitute_bound_var(
					terms,
					type_declarations,
					quantified->as.effect_row_forall.body,
					quantified->as.effect_row_forall.binding_id,
					row,
					&function_classifier
				) != 0) {
				return -1;
			}
			continue;
		}
		uint32_t expected_pi;
		uint32_t actual_pi;
		uint32_t expected_domain;
		uint32_t expected_family;
		uint32_t actual_domain;
		uint32_t actual_family;
		if (classifier_kernel_as_pi(
				terms,
				type_declarations,
				NULL,
				expected_computation,
				&expected_pi,
				&expected_domain,
				&expected_family
			) != 0 || classifier_kernel_as_pi(
				terms,
				type_declarations,
				NULL,
				actual_computation,
				&actual_pi,
				&actual_domain,
				&actual_family
			) != 0 || !prototype_judgement_classifier_compatible(
				terms, type_declarations, expected_domain, actual_domain
			)) {
			return -1;
		}
		uint32_t expected_binder;
		uint32_t expected_result;
		uint32_t actual_binder;
		uint32_t actual_result;
		if (prototype_term_pure_family_parts(
				terms, expected_family, &expected_binder, &expected_result
			) != 0 || prototype_term_pure_family_parts(
				terms, actual_family, &actual_binder, &actual_result
			) != 0 || expected_result >= terms->term_count ||
			actual_result >= terms->term_count ||
			terms->terms[expected_result].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
			terms->terms[actual_result].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return 1;
		}
		uint32_t row = terms->terms[actual_result].as.computation_type.label;
		if (row >= terms->term_count ||
			prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				quantified->as.effect_row_forall.body,
				quantified->as.effect_row_forall.binding_id,
				row,
				&function_classifier
			) != 0) {
			return -1;
		}
		(void)pi;
		(void)family;
		(void)expected_pi;
		(void)actual_pi;
		(void)expected_binder;
		(void)actual_binder;
	}
	return -1;
}

static int instantiate_pure_family_in_context(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t domain,
	uint32_t family,
	uint32_t argument,
	uint32_t argument_classifier,
	uint32_t* p_result
) {
	const struct prototype_substitution* prefix =
		prototype_substitution_get(substitutions, prefix_substitution);
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_result || !prefix ||
		domain >= terms->term_count || family >= terms->term_count ||
		argument >= terms->term_count ||
		argument_classifier >= terms->term_count) {
		return -1;
	}
	uint32_t binding_id;
	uint32_t body;
	if (prototype_term_pure_family_parts(
			terms,
			family,
			&binding_id,
			&body
		) != 0) {
		return -1;
	}
	uint32_t family_context;
	if (prototype_context_extend(
			contexts,
			prefix->target_context,
			binding_id,
			domain,
			PROTOTYPE_INVALID_ID,
			&family_context
		) != 0) {
		return -1;
	}
	const struct prototype_context* context =
		prototype_context_get(contexts, family_context);
	if (!context) {
		return -1;
	}
	if (context->binding_id != binding_id &&
		prototype_term_contains_free_binding(terms, body, binding_id)) {
		uint32_t canonical_var;
		if (prototype_term_var(
				terms, context->binding_id, &canonical_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				body,
				binding_id,
				canonical_var,
				&body
			) != 0) {
			return -1;
		}
	}
	uint32_t section;
	if (prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			prefix_substitution,
			family_context,
			argument,
			argument_classifier,
			&section
		) != 0) {
		return -1;
	}
	return prototype_term_reindex(
		terms,
		type_declarations,
		contexts,
		substitutions,
		body,
		section,
		p_result
	);
}

static int pi_codomain_after_argument_in_context(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t pi_term,
	uint32_t argument,
	uint32_t argument_classifier,
	uint32_t* p_result
) {
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t identity;
	if (!contexts || !substitutions ||
		!terms || !type_declarations || !p_result ||
		argument >= terms->term_count ||
		pi_parts(terms, pi_term, &domain, &codomain_family) != 0) {
		return -1;
	}
	(void)domain;
	if (prototype_substitution_identity(
			substitutions,
			contexts,
			context_id,
			&identity
		) != 0) {
		return -1;
	}
	(void)domain;
	return instantiate_pure_family_in_context(
		contexts,
		substitutions,
		terms,
		type_declarations,
		identity,
		argument_classifier,
		codomain_family,
		argument,
		argument_classifier,
		p_result
	);
}

static int pi_codomain_at_binder_in_context(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t premise_context_id,
	uint32_t pi_term,
	uint32_t binder_var,
	uint32_t binder_classifier,
	uint32_t binder_proof_id,
	uint32_t* p_result
) {
	(void)binder_proof_id;
	uint32_t domain;
	uint32_t family;
	uint32_t projection;
	if (!delta || !delta->contexts || !delta->substitutions ||
		!prototype_context_get(delta->contexts, premise_context_id) ||
		pi_parts(terms, pi_term, &domain, &family) != 0 ||
		prototype_substitution_projection(
			delta->substitutions,
			delta->contexts,
			premise_context_id,
			&projection
		) != 0) {
		return -1;
	}
	return instantiate_pure_family_in_context(
		delta->contexts,
		delta->substitutions,
		terms,
		type_declarations,
		projection,
		binder_classifier,
		family,
		binder_var,
		binder_classifier,
		p_result
	);
}

static int pi_codomain_at_fresh_binder(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t base_context_id,
	uint32_t pi_term,
	uint32_t binder_var,
	uint32_t binder_classifier,
	uint32_t binder_proof_id,
	uint32_t* p_result
) {
	(void)binder_proof_id;
	if (!delta || !delta->contexts || binder_var >= terms->term_count ||
		terms->terms[binder_var].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t premise_context;
	if (prototype_context_extend(
			delta->contexts,
			base_context_id,
			terms->terms[binder_var].as.var.binding_id,
			binder_classifier,
			PROTOTYPE_INVALID_ID,
			&premise_context
		) != 0) {
		return -1;
	}
	return pi_codomain_at_binder_in_context(
		delta,
		terms,
		type_declarations,
		premise_context,
		pi_term,
		binder_var,
		binder_classifier,
		binder_proof_id,
		p_result
	);
}

static int classifier_kernel_compatible_at_depth(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual,
	uint32_t depth
) {
	if (!terms || !type_declarations ||
		!term_exists(terms, expected) ||
		!term_exists(terms, actual) ||
		depth > 32) {
		return 0;
	}
	uint32_t normalized_expected = expected;
	uint32_t normalized_actual = actual;
	if (classifier_kernel_whnf(
			terms,
			type_declarations,
			definitions,
			expected,
			&normalized_expected
		) != 0 ||
		classifier_kernel_whnf(
			terms,
			type_declarations,
			definitions,
			actual,
			&normalized_actual
		) != 0) {
		return 0;
	}
	expected = normalized_expected;
	actual = normalized_actual;
	struct prototype_term_conversion_result conversion = classifier_kernel_conversion(
			terms,
			type_declarations,
			definitions,
			expected,
			actual
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return 0;
	}
	/* Artifact lowering may expose a qualified type name before relocation and
	 * the corresponding TYPE_VIEW after relocation. This is name resolution for
	 * one declaration identity, not structural equality between declarations. */
	const struct prototype_term* expected_term = &terms->terms[expected];
	const struct prototype_term* actual_term = &terms->terms[actual];
	if (expected_term->tag == PROTOTYPE_TERM_EXTERNAL_REF &&
		actual_term->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		expected_term->as.external_ref.name.namespace_symbol_id ==
			actual_term->as.type_view.identity.namespace_symbol_id &&
		expected_term->as.external_ref.name.name_symbol_id ==
			actual_term->as.type_view.identity.name_symbol_id) {
		return 1;
	}
	if (actual_term->tag == PROTOTYPE_TERM_EXTERNAL_REF &&
		expected_term->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		actual_term->as.external_ref.name.namespace_symbol_id ==
			expected_term->as.type_view.identity.namespace_symbol_id &&
		actual_term->as.external_ref.name.name_symbol_id ==
			expected_term->as.type_view.identity.name_symbol_id) {
		return 1;
	}
	/* Universe variables are not judgementally equal unless their level ids
	 * match. They are nevertheless conversion-compatible here: the compiler
	 * records the corresponding level inequality and UniverseDB checks it after
	 * linking the complete graph. */
	if (term_is_universe_var(terms, expected) &&
		term_is_universe_var(terms, actual)) {
		return 1;
	}
	if (terms->terms[expected].tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		terms->terms[actual].tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		const struct prototype_term* expected_computation = &terms->terms[expected];
		const struct prototype_term* actual_computation = &terms->terms[actual];
		if (expected_computation->as.computation_type.label >= terms->term_count ||
			actual_computation->as.computation_type.label >= terms->term_count) {
			return 0;
		}
		if (!(classifier_kernel_conversion(
				terms,
				type_declarations,
				definitions,
				expected_computation->as.computation_type.label,
				actual_computation->as.computation_type.label
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 0;
		}
		return classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			definitions,
			expected_computation->as.computation_type.result,
			actual_computation->as.computation_type.result,
			depth + 1
		);
	}
	if (terms->terms[expected].tag == PROTOTYPE_TERM_THUNK_TYPE &&
		terms->terms[actual].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		return classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			definitions,
			terms->terms[expected].as.thunk_type.computation,
			terms->terms[actual].as.thunk_type.computation,
			depth + 1
		);
	}

	uint32_t expected_domain;
	uint32_t expected_family;
	uint32_t actual_domain;
	uint32_t actual_family;
	if (pi_parts(terms, expected, &expected_domain, &expected_family) != 0 ||
		pi_parts(terms, actual, &actual_domain, &actual_family) != 0) {
		return 0;
	}
	if (!classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		definitions,
		expected_domain,
		actual_domain,
		depth + 1
	)) {
		return 0;
	}

	uint32_t expected_binder;
	uint32_t expected_family_body;
	uint32_t actual_binder;
	uint32_t actual_family_body;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[expected].as.pi.codomain_family,
			&expected_binder,
			&expected_family_body
		) != 0 || prototype_term_pure_family_parts(
			terms,
			terms->terms[actual].as.pi.codomain_family,
			&actual_binder,
			&actual_family_body
		) != 0) {
		return 0;
	}
	uint32_t comparison_binder = prototype_term_new_binding(terms);
	uint32_t comparison_var;
	uint32_t expected_body;
	uint32_t actual_body;
	if (comparison_binder == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (prototype_term_var(
		terms,
		comparison_binder,
		&comparison_var
	) != 0) {
		return 0;
	}
	if (prototype_term_graph_substitute_bound_var(
		terms,
		type_declarations,
		expected_family_body,
		expected_binder,
		comparison_var,
		&expected_body
	) != 0) {
		return 0;
	}
	if (prototype_term_graph_substitute_bound_var(
		terms,
		type_declarations,
		actual_family_body,
		actual_binder,
		comparison_var,
		&actual_body
	) != 0) {
		return 0;
	}
	return classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		definitions,
		expected_body,
		actual_body,
		depth + 1
	);
}

static int classifier_kernel_compatible_no_definitions_at_depth(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual,
	uint32_t depth
) {
	return classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		NULL,
		expected,
		actual,
		depth
	);
}

struct expected_effect_row_solution {
	uint32_t binding_id;
	uint32_t row;
};

struct expected_effect_row_solver {
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_term_definition_env* definitions;
	struct expected_effect_row_solution solutions[64];
	uint32_t solution_count;
};

static int expected_effect_row_bind(
	struct expected_effect_row_solver* solver,
	uint32_t binding_id,
	uint32_t row
) {
	if (!solver || row >= solver->terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < solver->solution_count; ++i) {
		if (solver->solutions[i].binding_id != binding_id) {
			continue;
		}
		return (classifier_kernel_conversion(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			solver->solutions[i].row,
			row
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ? 0 : 1;
	}
	if (solver->solution_count >= 64) {
		return -1;
	}
	solver->solutions[solver->solution_count].binding_id = binding_id;
	solver->solutions[solver->solution_count].row = row;
	solver->solution_count += 1;
	return 0;
}

static int expected_effect_row_solve_row(
	struct expected_effect_row_solver* solver,
	uint32_t expected,
	uint32_t actual
) {
	if (!solver || expected >= solver->terms->term_count ||
		actual >= solver->terms->term_count) {
		return -1;
	}
	const struct prototype_term* expected_term = &solver->terms->terms[expected];
	if (expected_term->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return expected_effect_row_bind(
			solver, expected_term->as.effect_row_var.binding_id, actual
		);
	}
	return (classifier_kernel_conversion(
		solver->terms,
		solver->type_declarations,
		solver->definitions,
		expected,
		actual
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ? 0 : 1;
}

static int expected_effect_row_solve_classifier(
	struct expected_effect_row_solver* solver,
	uint32_t expected,
	uint32_t actual,
	uint32_t depth
) {
	if (!solver || expected >= solver->terms->term_count ||
		actual >= solver->terms->term_count || depth > 32) {
		return -1;
	}
	uint32_t normalized_expected;
	uint32_t normalized_actual;
	if (classifier_kernel_whnf(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			expected,
			&normalized_expected
		) != 0 || classifier_kernel_whnf(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			actual,
			&normalized_actual
		) != 0) {
		return -1;
	}
	expected = normalized_expected;
	actual = normalized_actual;
	const struct prototype_term* expected_term = &solver->terms->terms[expected];
	const struct prototype_term* actual_term = &solver->terms->terms[actual];
	if (expected_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		actual_term->tag == PROTOTYPE_TERM_MATCH) {
		struct prototype_term expected_computation = *expected_term;
		struct prototype_term actual_match = *actual_term;
		uint32_t expected_result;
		if (classifier_kernel_whnf(
				solver->terms,
				solver->type_declarations,
				solver->definitions,
				expected_computation.as.computation_type.result,
				&expected_result
			) != 0 || expected_result >= solver->terms->term_count ||
			solver->terms->terms[expected_result].tag != PROTOTYPE_TERM_MATCH) {
			return 1;
		}
		struct prototype_term expected_match = solver->terms->terms[expected_result];
		if (expected_match.as.match.case_count != actual_match.as.match.case_count ||
			!(classifier_kernel_conversion(
				solver->terms,
				solver->type_declarations,
				solver->definitions,
				expected_match.as.match.scrutinee,
				actual_match.as.match.scrutinee
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 1;
		}
		for (uint32_t i = 0; i < expected_match.as.match.case_count; ++i) {
			uint32_t expected_case_id = expected_match.as.match.first_case + i;
			uint32_t actual_case_id = actual_match.as.match.first_case + i;
			if (expected_case_id >= solver->terms->case_count ||
				actual_case_id >= solver->terms->case_count) {
				return -1;
			}
			struct prototype_match_case expected_case =
				solver->terms->cases[expected_case_id];
			struct prototype_match_case actual_case =
				solver->terms->cases[actual_case_id];
			int use_labels =
				expected_case.constructor_owner == PROTOTYPE_INVALID_ID ||
				actual_case.constructor_owner == PROTOTYPE_INVALID_ID ||
				expected_case.constructor_id == PROTOTYPE_INVALID_ID ||
				actual_case.constructor_id == PROTOTYPE_INVALID_ID;
			int labels_equal = use_labels &&
				solver->terms->case_label_symbols[expected_case_id] >= 0 &&
				solver->terms->case_label_symbols[expected_case_id] ==
					solver->terms->case_label_symbols[actual_case_id];
			if ((use_labels && !labels_equal) ||
				(!use_labels &&
					expected_case.constructor_id != actual_case.constructor_id) ||
				expected_case.binder_count != actual_case.binder_count) {
				return 1;
			}
			if (!use_labels &&
				(expected_case.constructor_owner != PROTOTYPE_INVALID_ID ||
				actual_case.constructor_owner != PROTOTYPE_INVALID_ID)) {
				if (expected_case.constructor_owner == PROTOTYPE_INVALID_ID ||
					actual_case.constructor_owner == PROTOTYPE_INVALID_ID ||
					!(classifier_kernel_conversion(
						solver->terms,
						solver->type_declarations,
						solver->definitions,
						expected_case.constructor_owner,
						actual_case.constructor_owner
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					return 1;
				}
			}

			uint32_t expected_body = expected_case.body;
			uint32_t actual_body = actual_case.body;
			for (uint32_t j = 0; j < expected_case.binder_count; ++j) {
				uint32_t expected_binder_index = expected_case.first_binder + j;
				uint32_t actual_binder_index = actual_case.first_binder + j;
				uint32_t comparison_binder = prototype_term_new_binding(solver->terms);
				uint32_t comparison_var;
				if (expected_binder_index >= solver->terms->case_binder_count ||
					actual_binder_index >= solver->terms->case_binder_count ||
					comparison_binder == PROTOTYPE_INVALID_ID ||
					prototype_term_var(
						solver->terms, comparison_binder, &comparison_var
					) != 0 || prototype_term_graph_substitute_bound_var(
						solver->terms,
						solver->type_declarations,
						expected_body,
						solver->terms->case_binders[expected_binder_index].binding_id,
						comparison_var,
						&expected_body
					) != 0 || prototype_term_graph_substitute_bound_var(
						solver->terms,
						solver->type_declarations,
						actual_body,
						solver->terms->case_binders[actual_binder_index].binding_id,
						comparison_var,
						&actual_body
					) != 0) {
					return -1;
				}
			}

			uint32_t actual_branch;
			if (classifier_kernel_whnf(
					solver->terms,
					solver->type_declarations,
					solver->definitions,
					actual_body,
					&actual_branch
				) != 0 || actual_branch >= solver->terms->term_count ||
				solver->terms->terms[actual_branch].tag !=
					PROTOTYPE_TERM_COMPUTATION_TYPE) {
				return 1;
			}
			struct prototype_term branch = solver->terms->terms[actual_branch];
			int status = expected_effect_row_solve_row(
				solver,
				expected_computation.as.computation_type.label,
				branch.as.computation_type.label
			);
			if (status != 0) {
				return status;
			}
			status = expected_effect_row_solve_classifier(
				solver,
				expected_body,
				branch.as.computation_type.result,
				depth + 1
			);
			if (status != 0) {
				return status;
			}
		}
		return 0;
	}
	if (expected_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		actual_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		int row_status = expected_effect_row_solve_row(
			solver,
			expected_term->as.computation_type.label,
			actual_term->as.computation_type.label
		);
		if (row_status != 0) {
			return row_status;
		}
		return expected_effect_row_solve_classifier(
			solver,
			expected_term->as.computation_type.result,
			actual_term->as.computation_type.result,
			depth + 1
		);
	}
	if (expected_term->tag == PROTOTYPE_TERM_THUNK_TYPE &&
		actual_term->tag == PROTOTYPE_TERM_THUNK_TYPE) {
		return expected_effect_row_solve_classifier(
			solver,
			expected_term->as.thunk_type.computation,
			actual_term->as.thunk_type.computation,
			depth + 1
		);
	}

	uint32_t expected_domain;
	uint32_t expected_family;
	uint32_t actual_domain;
	uint32_t actual_family;
	if (pi_parts(
			solver->terms, expected, &expected_domain, &expected_family
		) == 0 && pi_parts(
			solver->terms, actual, &actual_domain, &actual_family
		) == 0) {
		int domain_status = expected_effect_row_solve_classifier(
			solver, expected_domain, actual_domain, depth + 1
		);
		if (domain_status != 0) {
			return domain_status;
		}
		uint32_t expected_binder;
		uint32_t expected_body;
		uint32_t actual_binder;
		uint32_t actual_body;
		if (prototype_term_pure_family_parts(
				solver->terms,
				expected_family,
				&expected_binder,
				&expected_body
			) != 0 || prototype_term_pure_family_parts(
				solver->terms,
				actual_family,
				&actual_binder,
				&actual_body
			) != 0) {
			return -1;
		}
		uint32_t comparison_binder = prototype_term_new_binding(solver->terms);
		uint32_t comparison_var;
		uint32_t instantiated_expected;
		uint32_t instantiated_actual;
		if (comparison_binder == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				solver->terms, comparison_binder, &comparison_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				solver->terms,
				solver->type_declarations,
				expected_body,
				expected_binder,
				comparison_var,
				&instantiated_expected
			) != 0 || prototype_term_graph_substitute_bound_var(
				solver->terms,
				solver->type_declarations,
				actual_body,
				actual_binder,
				comparison_var,
				&instantiated_actual
			) != 0) {
			return -1;
		}
		return expected_effect_row_solve_classifier(
			solver,
			instantiated_expected,
			instantiated_actual,
			depth + 1
		);
	}

	return classifier_kernel_compatible_at_depth(
		solver->terms,
		solver->type_declarations,
		solver->definitions,
		expected,
		actual,
		depth
	) ? 0 : 1;
}

int prototype_judgement_solve_expected_effect_rows(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual,
	uint32_t* p_solved_expected
) {
	if (!terms || !type_declarations || !p_solved_expected ||
		expected >= terms->term_count || actual >= terms->term_count) {
		return -1;
	}
	struct expected_effect_row_solver solver;
	memset(&solver, 0, sizeof(solver));
	solver.terms = terms;
	solver.type_declarations = type_declarations;
	solver.definitions = definitions;
	int status = expected_effect_row_solve_classifier(
		&solver, expected, actual, 0
	);
	if (status != 0) {
		return status;
	}
	uint32_t solved = expected;
	for (uint32_t i = 0; i < solver.solution_count; ++i) {
		if (prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				solved,
				solver.solutions[i].binding_id,
				solver.solutions[i].row,
				&solved
			) != 0) {
			return -1;
		}
	}
	if (!classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			definitions,
			solved,
			actual,
			0
		)) {
		return 1;
	}
	*p_solved_expected = solved;
	return 0;
}

int prototype_judgement_classifier_compatible(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_compatible_no_definitions_at_depth(terms, type_declarations, expected, actual, 0);
}

int prototype_judgement_classifier_compatible_with_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		definitions,
		expected,
		actual,
		0
	);
}

static int owner_parameter_argument(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t binding_id,
	uint32_t* p_argument
) {
	if (!terms || !type_declarations || !p_argument || owner >= terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(
			terms, owner, &type_id, args, &arg_count
		) != 0 || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		uint32_t parameter_id = type->first_parameter + i;
		if (parameter_id >= type_declarations->parameter_count ||
			i >= arg_count) {
			return -1;
		}
		if (type_declarations->parameter_declarations[parameter_id].binding_id ==
			binding_id) {
			*p_argument = args[i];
			return 0;
		}
	}
	return -1;
}

static int resolver_type_expr_term_with_self(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret || type_expr >= type_declarations->expr_count) {
		return -1;
	}

	const struct prototype_type_expr expr = type_declarations->exprs[type_expr];
	switch (expr.tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			return prototype_term_universe_var(terms, expr.as.universe.level, p_ret);
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			return prototype_term_universe_var(terms, expr.as.universe_var.level_var, p_ret);
		case PROTOTYPE_TYPE_EXPR_SELF:
			if (self_type == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			*p_ret = self_type;
			return 0;
		case PROTOTYPE_TYPE_EXPR_VAR:
			if (self_type != PROTOTYPE_INVALID_ID &&
				owner_parameter_argument(
					terms,
					type_declarations,
					self_type,
					expr.as.var.binding_id,
					p_ret
				) == 0) {
				return 0;
			}
			return prototype_term_var(terms, expr.as.var.binding_id, p_ret);
		case PROTOTYPE_TYPE_EXPR_NAME: {
			const struct prototype_type_declaration* type =
				prototype_type_declaration_lookup(type_declarations, expr.as.name.symbol_id);
			if (!type) {
				return -1;
			}
			return prototype_term_type_instance_make(
				terms,
				type_declarations,
				type->type_index,
				NULL,
				0,
				p_ret
			);
		}
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE: {
				return prototype_term_external_ref(
					terms,
					expr.as.imported_type.name,
					p_ret
				);
			}
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
				return prototype_term_external_ref(terms, expr.as.external_term.name, p_ret);
		case PROTOTYPE_TYPE_EXPR_APP: {
			uint32_t function;
			uint32_t argument;
			if (resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.app.function,
					self_type,
					&function
				) != 0 ||
				resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.app.argument,
					self_type,
					&argument
				) != 0) {
				return -1;
			}
			uint32_t type_id;
				uint32_t args[16];
				uint32_t arg_count;
				if (prototype_term_type_instance_info(terms, function, &type_id, args, &arg_count) == 0) {
					if (type_id >= type_declarations->type_count) {
						return -1;
					}
					const struct prototype_type_declaration* type =
						&type_declarations->type_declarations[type_id];
					if (arg_count < type->parameter_count) {
						return prototype_term_type_instance_extend(
							terms,
							type_declarations,
							function,
							argument,
							p_ret
						);
					}
				}
			return prototype_term_app(terms, function, argument, p_ret);
		}
		case PROTOTYPE_TYPE_EXPR_ARROW: {
			uint32_t domain;
			uint32_t codomain;
			if (resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.arrow.domain,
					self_type,
					&domain
				) != 0 ||
				resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.arrow.codomain,
					self_type,
					&codomain
				) != 0) {
				return -1;
			}
			return prototype_term_pi(terms, domain, codomain, p_ret);
		}
		case PROTOTYPE_TYPE_EXPR_PI: {
			uint32_t domain;
			uint32_t codomain;
			uint32_t family;
			if (resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.pi.domain,
					self_type,
					&domain
				) != 0 || resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.pi.codomain,
					self_type,
					&codomain
				) != 0 || prototype_term_pure_family(
					terms, expr.as.pi.binding_id, codomain, &family
				) != 0) {
				return -1;
			}
			return prototype_term_pi_family(terms, domain, family, p_ret);
		}
		default:
			return -1;
	}
}

int prototype_judgement_type_expr_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
) {
	return resolver_type_expr_term_with_self(
		terms,
		type_declarations,
		type_expr,
		self_type,
		p_ret
	);
}

int prototype_judgement_resolve_match_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t scrutinee_classifier,
	int constructor_symbol_id,
	struct prototype_match_constructor_resolution* p_resolution
) {
	if (!terms || !type_declarations || !contexts || !p_resolution ||
		scrutinee_classifier >= terms->term_count) {
		return -1;
	}

	uint32_t type_id;
	uint32_t ignored_args[16];
	uint32_t ignored_arg_count;
	uint32_t normalized_classifier;
	if (classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			scrutinee_classifier,
			&normalized_classifier
		) != 0) {
		return -1;
	}
	if (prototype_term_type_instance_info(
		terms,
		normalized_classifier,
		&type_id,
		ignored_args,
		&ignored_arg_count
	) != 0) {
		return -1;
	}

	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(
			type_declarations,
			type_id,
			constructor_symbol_id
		);
	if (!constructor) {
		return -1;
	}

	uint32_t field_contexts[64];
	uint32_t field_count;
	if (prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			field_contexts,
			64,
			&field_count
		) != 0) {
		return -1;
	}

	p_resolution->constructor_owner = normalized_classifier;
	p_resolution->constructor_id = constructor->constructor_index;
	p_resolution->field_count = field_count;
	return 0;
}

static const struct prototype_type_constructor_declaration* lookup_constructor_for_owner_index(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations ||
		prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			args,
			16,
			&arg_count
		) != 0 ||
		type_id >= type_declarations->type_count) {
		return NULL;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == constructor_index) {
			return constructor;
		}
	}
	return NULL;
}

static int constructor_classifier_from_curried_cache(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	const struct prototype_type_constructor_declaration* constructor,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !constructor || !p_classifier ||
		constructor->curried_classifier_cache == PROTOTYPE_INVALID_ID ||
		constructor->curried_classifier_cache >= terms->term_count ||
		owner >= terms->term_count) {
		return 1;
	}

	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			args,
			16,
			&arg_count
		) != 0 ||
		type_id >= type_declarations->type_count ||
		type_id != constructor->owner_type) {
		return 1;
	}

	uint32_t classifier = constructor->curried_classifier_cache;
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t app;
		if (prototype_term_app(terms, classifier, args[i], &app) != 0) {
			return -1;
		}
		classifier = app;
	}

	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			(int)classifier,
			&classifier
		) != 0) {
		return -1;
	}
	if (!classifier_returns_owner(terms, type_declarations, classifier, owner)) {
		return 1;
	}
	*p_classifier = classifier;
	return 0;
}

static int materialize_constructor_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index,
	uint32_t* p_constructor_term,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_constructor_term || !p_classifier ||
		owner >= terms->term_count) {
		return -1;
	}

	uint32_t constructor_term;
	if (prototype_term_constructor(
			terms,
			owner,
			constructor_index,
			&constructor_term
		) != 0) {
		return -1;
	}

	uint32_t existing_classifiers[32];
	uint32_t existing_classifier_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			constructor_term,
			existing_classifiers,
			32,
			&existing_classifier_count
		) != 0) {
		return -1;
	}
	uint32_t selected_existing = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < existing_classifier_count; ++i) {
		if (!classifier_returns_owner(terms, type_declarations, existing_classifiers[i], owner)) {
			continue;
		}
		if (selected_existing != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				selected_existing,
				existing_classifiers[i]
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		selected_existing = existing_classifiers[i];
	}
	if (selected_existing != PROTOTYPE_INVALID_ID) {
		*p_constructor_term = constructor_term;
		*p_classifier = selected_existing;
		return 0;
	}

	uint32_t schema_owner;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			(int)owner,
			&schema_owner
		) != 0) {
		return -1;
	}
	uint32_t schema_type_id;
	uint32_t schema_args[16];
	uint32_t schema_arg_count;
	if (prototype_term_type_instance_info(
			terms,
			schema_owner,
			&schema_type_id,
			schema_args,
			&schema_arg_count
		) != 0) {
		if (prototype_type_declaration_instance_info(
				type_declarations,
				terms,
				schema_owner,
				&schema_type_id,
				schema_args,
				16,
				&schema_arg_count
			) != 0 ||
			prototype_term_type_instance_make(
				terms,
				type_declarations,
				schema_type_id,
				schema_args,
				schema_arg_count,
				&schema_owner
			) != 0) {
			return -1;
		}
	}
	const struct prototype_type_constructor_declaration* constructor =
		lookup_constructor_for_owner_index(
			terms,
			type_declarations,
			schema_owner,
			constructor_index
		);
	if (!constructor) {
		return -1;
	}

	uint32_t cached_classifier;
	int cache_status = constructor_classifier_from_curried_cache(
		terms,
		type_declarations,
		owner,
		constructor,
		&cached_classifier
	);
	if (cache_status < 0) {
		return -1;
	}
	if (cache_status > 0) {
		return -1;
	}

	if (add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION,
			constructor_term,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			constructor_term,
			cached_classifier,
			PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO
		) != 0) {
		return -1;
	}
	*p_constructor_term = constructor_term;
	*p_classifier = cached_classifier;
	return 0;
}

int prototype_judgement_constructor_field_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_context,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!p_classifier ||
		(previous_binder_count > 0 && !previous_binders)) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			arguments,
			64,
			&argument_count
		) != 0) {
		return -1;
	}
	if (type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (argument_count != type->parameter_count ||
		constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	if (previous_binder_count < field_index) {
		return -1;
	}
	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			contexts,
			substitutions,
			terms,
			type_declarations,
			source_context,
			constructor->parameter_context,
			arguments,
			argument_count,
			&substitution
		) != 0) {
		return -1;
	}
	uint32_t previous_terms[64];
	for (uint32_t i = 0; i < field_index; ++i) {
		if (prototype_term_var(
				terms, previous_binders[i].binding_id, &previous_terms[i]
			) != 0) {
			return -1;
		}
	}
	return prototype_context_telescope_entry_classifier(
		contexts,
		substitutions,
		terms,
		type_declarations,
		substitution,
		constructor->parameter_context,
		constructor->field_context,
		previous_terms,
		field_index,
		field_index,
		p_classifier
	);
}

static int constructor_field_classifier_from_spine(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!delta) {
		return -1;
	}
	return prototype_judgement_constructor_field_classifier(
		terms,
		type_declarations,
		delta->contexts,
		delta->substitutions,
		delta->current_context_id,
		owner,
		constructor_index,
		previous_binders,
		previous_binder_count,
		field_index,
		p_classifier
	);
}

int prototype_judgement_constructor_spine_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_context,
	uint32_t subject,
	uint32_t constructor_owner_view,
	const uint32_t* argument_classifiers,
	uint32_t argument_classifier_count,
	uint32_t* p_classifier,
	int* p_saturated
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!p_classifier || !p_saturated ||
		(argument_classifier_count > 0 && !argument_classifiers)) {
		return -1;
	}
	uint32_t constructor_head;
	uint32_t structural_owner;
	uint32_t constructor_index;
	uint32_t field_terms[64];
	uint32_t field_count;
	if (prototype_term_constructor_spine_info(
			terms,
			subject,
			&constructor_head,
			&structural_owner,
			&constructor_index,
			field_terms,
			64,
			&field_count
		) != 0 || field_count != argument_classifier_count) {
		return -1;
	}
	uint32_t owner = structural_owner;
	if (constructor_owner_view != PROTOTYPE_INVALID_ID) {
		int same_core;
		if (constructor_owner_view >= terms->term_count ||
			prototype_term_core_shape_equal(
				terms, structural_owner, constructor_owner_view, &same_core
			) != 0 || !same_core) {
			return -1;
		}
		owner = constructor_owner_view;
	}
	uint32_t type_id;
	uint32_t owner_arguments[64];
	uint32_t owner_argument_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			owner_arguments,
			64,
			&owner_argument_count
		) != 0) {
		return -1;
	}
	if (type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			type_declarations->constructor_count ||
		owner_argument_count != type->parameter_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	uint32_t field_context_path[64];
	uint32_t declared_field_count;
	if (prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			field_context_path,
			64,
			&declared_field_count
		) != 0 || field_count > declared_field_count) {
		return -1;
	}
	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			contexts,
			substitutions,
			terms,
			type_declarations,
			source_context,
			constructor->parameter_context,
			owner_arguments,
			owner_argument_count,
			&substitution
		) != 0) {
		return -1;
	}

	uint32_t residual_classifier = constructor->curried_classifier_cache;
	for (uint32_t i = 0; i < owner_argument_count; ++i) {
		if (prototype_term_app(
				terms, residual_classifier, owner_arguments[i], &residual_classifier
			) != 0) {
			return -1;
		}
	}
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			residual_classifier,
			&residual_classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		const struct prototype_context* field_context =
			prototype_context_get(contexts, field_context_path[i]);
		uint32_t expected_classifier;
		uint32_t normalized_expected;
		uint32_t next_residual;
		if (!field_context) {
			return -1;
		}
		uint32_t field_classifier =
			prototype_context_classifier_term(field_context);
		if (prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				field_classifier,
				substitution,
				&expected_classifier
			) != 0) {
			return -1;
		}
		if (prototype_judgement_classifier_value_whnf(
				terms, type_declarations, expected_classifier, &normalized_expected
			) != 0) {
			return -1;
		}
		if (!prototype_judgement_classifier_reference_equal(
				terms,
				type_declarations,
				normalized_expected,
				argument_classifiers[i]
			)) {
			return -1;
		}
		if (pi_codomain_after_argument_in_context(
				contexts,
				substitutions,
				terms,
				type_declarations,
				source_context,
				residual_classifier,
				field_terms[i],
				argument_classifiers[i],
				&next_residual
			) != 0) {
			return -1;
		}
		if (prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				type_declarations,
				substitution,
				field_context_path[i],
				field_terms[i],
				argument_classifiers[i],
				&substitution
			) != 0) {
			return -1;
		}
		residual_classifier = next_residual;
	}

	uint32_t classifier = residual_classifier;
	*p_saturated = field_count == declared_field_count;
	if (*p_saturated && prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			constructor->result_classifier,
			substitution,
			&classifier
		) != 0) {
		return -1;
	}
	if (prototype_judgement_classifier_value_whnf(
			terms, type_declarations, classifier, &classifier
		) != 0) {
		return -1;
	}
	(void)constructor_head;
	*p_classifier = classifier;
	return 0;
}

int prototype_judgement_delta_record_constructor_spine(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* argument_operation_ids,
	const struct prototype_judgement_selected_evidence* argument_evidence,
	uint32_t argument_evidence_count
) {
	if (!delta || !terms || !type_declarations || !delta->contexts ||
		!delta->substitutions || argument_evidence_count == 0 ||
		argument_evidence_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		!argument_operation_ids || !argument_evidence) {
		return -1;
	}
	struct prototype_judgement_selected_evidence
		current_argument_evidence[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < argument_evidence_count; ++i) {
		if (argument_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			argument_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
			return -1;
		}
		current_argument_evidence[i] = argument_evidence[i];
		if (argument_evidence[i].context_id != delta->current_context_id) {
			uint32_t projection;
			if (prototype_substitution_projection_path(
					delta->substitutions,
					delta->contexts,
					delta->current_context_id,
					argument_evidence[i].context_id,
					&projection
				) != 0 || prototype_judgement_delta_record_context_weaken(
					delta, &argument_evidence[i], projection
				) != 0 || prototype_judgement_delta_select_evidence(
					delta,
					argument_evidence[i].authority_kind ==
						PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
						argument_evidence[i].operation_id : PROTOTYPE_INVALID_ID,
					delta->current_context_id,
					argument_evidence[i].subject,
					argument_evidence[i].classifier,
					&current_argument_evidence[i]
				) != 0) {
				return -1;
			}
		}
		argument_classifiers[i] = current_argument_evidence[i].classifier;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count;
	uint32_t expected_classifier;
	int saturated;
	if (prototype_term_constructor_spine_info(
			terms,
			subject,
			&head,
			&owner,
			&constructor_index,
			arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&argument_count
		) != 0 || argument_count != argument_evidence_count ||
		prototype_judgement_constructor_spine_classifier(
			terms,
			type_declarations,
			delta->contexts,
			delta->substitutions,
			delta->current_context_id,
			subject,
			owner,
			argument_classifiers,
			argument_evidence_count,
			&expected_classifier,
			&saturated
		) != 0 || !(prototype_judgement_classifier_conversion(
			terms, type_declarations, expected_classifier, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	(void)head;
	(void)constructor_index;
	(void)saturated;
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
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = relation.operation_id;
	proof.conclusion_subject = relation.subject;
	proof.conclusion_classifier = relation.classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.rule_data.constructor.owner_view = owner;
	proof.premise_count = argument_count;
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (current_argument_evidence[i].subject != arguments[i]) {
			return -1;
		}
		proof.premises[i].proposition.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof.premises[i].proposition.context_id =
			current_argument_evidence[i].context_id;
		proof.premises[i].proposition.operation_id = argument_operation_ids[i];
		proof.premises[i].proposition.subject = arguments[i];
		proof.premises[i].proposition.classifier =
			current_argument_evidence[i].classifier;
		proof.premises[i].proposition.authority_kind =
			current_argument_evidence[i].authority_kind;
		proof.premises[i].proposition.authority_id =
			current_argument_evidence[i].authority_id;
	}
	return add_complete_delta_relation(delta, &relation, &proof);
}

int prototype_judgement_synthesize_match_pattern_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	int constructor_symbol_id,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		scrutinee >= terms->term_count ||
		scrutinee_classifier >= terms->term_count) {
		return -1;
	}

	struct prototype_judgement_selected_evidence scrutinee_evidence;
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			scrutinee,
			scrutinee_classifier,
			&scrutinee_evidence
		) != 0) {
		return -1;
	}

	uint32_t type_id;
	uint32_t ignored_args[16];
	uint32_t ignored_arg_count;
	if (prototype_term_type_instance_info(
		terms,
		scrutinee_classifier,
		&type_id,
		ignored_args,
		&ignored_arg_count
	) != 0) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(type_declarations, type_id, constructor_symbol_id);
	if (!constructor) {
		return -1;
	}
	return constructor_field_classifier_from_spine(
		delta,
		terms,
		type_declarations,
		scrutinee_classifier,
		constructor->constructor_index,
		NULL,
		0,
		field_index,
		p_classifier
	);
}

int prototype_judgement_resolve_match_case_request(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_resolution_request* request,
	struct prototype_match_constructor_resolution* p_resolution
) {
	if (!delta || !terms || !type_declarations || !request || !p_resolution ||
		request->match_term >= terms->term_count ||
		terms->terms[request->match_term].tag != PROTOTYPE_TERM_MATCH ||
		request->case_index >= terms->terms[request->match_term].as.match.case_count) {
		return -1;
	}

	struct prototype_match_constructor_resolution resolution;
	if (prototype_judgement_resolve_match_constructor(
		terms,
		type_declarations,
		delta->contexts,
		request->scrutinee_classifier,
		request->constructor_symbol_id,
		&resolution
	) != 0) {
		return -1;
	}

	uint32_t case_id = terms->terms[request->match_term].as.match.first_case + request->case_index;
	if (case_id >= terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* match_case = &terms->cases[case_id];
	if (match_case->binder_count != resolution.field_count) {
		return -1;
	}
	if (prototype_term_resolve_match_case(
		terms,
		request->match_term,
		request->case_index,
		resolution.constructor_owner,
		resolution.constructor_id
	) != 0) {
		return -1;
	}

		for (uint32_t j = 0; j < match_case->binder_count; ++j) {
		struct prototype_case_binder* binder =
			&terms->case_binders[match_case->first_binder + j];
			uint32_t binder_var;
			uint32_t binder_classifier;
			if (prototype_term_var(
					terms,
					binder->binding_id,
					&binder_var
				) != 0 ||
				constructor_field_classifier_from_spine(
					delta,
					terms,
					type_declarations,
					request->scrutinee_classifier,
					resolution.constructor_id,
					&terms->case_binders[match_case->first_binder],
					j,
					j,
					&binder_classifier
				) != 0 ||
				prototype_judgement_delta_record_match_pattern(
					delta,
					terms,
					binder_var,
					binder_classifier,
					request->scrutinee_classifier,
					resolution.constructor_id,
					j
				) != 0) {
				return -1;
			}
			if (classifier_conversion_decision(
					prototype_judgement_classifier_conversion(
						terms,
						type_declarations,
						binder_classifier,
						request->scrutinee_classifier
					),
					&binder->is_recursive
				) != 0) {
				return -1;
			}
		}

	*p_resolution = resolution;
	return 0;
}

static int match_case_binder_is_recursive_self_field(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	uint32_t binding_id
) {
	if (!delta || !terms || !type_declarations || !match_case ||
		match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		match_case->constructor_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}

	uint32_t field_index = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		const struct prototype_case_binder* binder =
			&terms->case_binders[match_case->first_binder + i];
		if (binder->binding_id == binding_id) {
			field_index = i;
			break;
		}
	}
	if (field_index == PROTOTYPE_INVALID_ID) {
		return 0;
	}

	uint32_t field_classifier;
	if (constructor_field_classifier_from_spine(
			delta,
			terms,
			type_declarations,
			match_case->constructor_owner,
			match_case->constructor_id,
			&terms->case_binders[match_case->first_binder],
			field_index,
			field_index,
			&field_classifier
		) != 0) {
		return -1;
	}
	return (prototype_judgement_classifier_conversion(
		terms,
		type_declarations,
		field_classifier,
		match_case->constructor_owner
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ? 0 : -1;
}

static int induction_hypothesis_classifier_from_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t argument,
	uint32_t* p_classifier,
	uint32_t* p_motive
) {
	if (!delta || !terms || !type_declarations || !p_classifier || !p_motive ||
		!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH) ||
			argument >= terms->term_count) {
		return -1;
	}
	uint32_t match_classifiers[32];
	uint32_t match_classifier_count = 0;
	for (int source = 0; source < 2; ++source) {
		if (source == 1 && !delta->db) {
			continue;
		}
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
				relation->subject != match_term ||
				candidate_has_derivation_other_than(
					relations,
					proposition_count,
					derivations,
					derivation_candidate_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
				) != 1 ||
				!match_motive_result_classifier(terms, match_term, relation->classifier)) {
				continue;
			}
			int contains = classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					match_classifiers,
					match_classifier_count,
					relation->classifier
				);
			if (contains < 0) {
				return -1;
			}
			if (contains > 0) {
				continue;
			}
			if (match_classifier_count >= 32) {
				return -1;
			}
			match_classifiers[match_classifier_count++] = relation->classifier;
		}
	}
	for (size_t i = 0; i < delta->match_motive_result_count; ++i) {
		const struct prototype_judgement_match_motive_result* result =
			&delta->match_motive_results[i];
		if (result->match_term != match_term ||
			!match_motive_result_classifier(terms, match_term, result->classifier)) {
			continue;
		}
		int contains = classifier_list_contains_normalization_equal(
				terms,
				type_declarations,
				match_classifiers,
				match_classifier_count,
				result->classifier
			);
		if (contains < 0) {
			return -1;
		}
		if (contains > 0) {
			continue;
		}
		if (match_classifier_count >= 32) {
			return -1;
		}
		match_classifiers[match_classifier_count++] = result->classifier;
	}
	if (match_classifier_count == 0) {
		return 1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t selected = PROTOTYPE_INVALID_ID;
	uint32_t selected_motive = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match_classifier_count; ++i) {
		uint32_t match_classifier = match_classifiers[i];
		if (!term_has_tag(terms, match_classifier, PROTOTYPE_TERM_APP)) {
			continue;
		}
		const struct prototype_term* motive_app = &terms->terms[match_classifier];
		if (motive_app->as.app.argument != match->as.match.scrutinee ||
			!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
			continue;
		}
		uint32_t candidate;
		if (prototype_term_app(
				terms,
				motive_app->as.app.function,
				argument,
				&candidate
			) != 0) {
			return -1;
		}
		if (selected != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				selected,
				candidate
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		if (selected_motive != PROTOTYPE_INVALID_ID &&
			selected_motive != motive_app->as.app.function) {
			return -1;
		}
		selected = candidate;
		selected_motive = motive_app->as.app.function;
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = selected;
	*p_motive = selected_motive;
	(void)delta;
	return 0;
}

static int induction_hypothesis_context_from_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t argument,
	uint32_t* p_context_case_index,
	uint32_t* p_context_field_index
) {
	if (!delta || !terms || !type_declarations || !p_context_case_index ||
		!p_context_field_index ||
		!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH) ||
		argument >= terms->term_count ||
		terms->terms[argument].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t binding_id = terms->terms[argument].as.var.binding_id;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		int recursive = match_case_binder_is_recursive_self_field(
			delta,
			terms,
			type_declarations,
			&terms->cases[case_id],
			binding_id
		);
		if (recursive < 0) {
			return -1;
		}
		if (recursive > 0) {
			continue;
		}
		for (uint32_t j = 0; j < terms->cases[case_id].binder_count; ++j) {
			if (terms->case_binders[terms->cases[case_id].first_binder + j].binding_id ==
				binding_id) {
				*p_context_case_index = i;
				*p_context_field_index = j;
				return 0;
			}
		}
	}
	return 1;
}

static int collect_judgement_subject_classifiers(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
);
struct app_classifier_candidate {
	struct prototype_judgement_selected_evidence function_evidence;
	struct prototype_judgement_selected_evidence argument_evidence;
	uint32_t function_pi;
	uint32_t result_classifier;
};

static int collect_app_classifier_candidates_from_candidates(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t argument,
	const struct prototype_judgement_selected_evidence* function_candidates,
	uint32_t function_candidate_count,
	const struct prototype_judgement_selected_evidence* argument_candidates,
	uint32_t argument_candidate_count,
	struct app_classifier_candidate* candidates,
	uint32_t candidate_capacity,
	uint32_t* p_candidate_count
) {
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!function_candidates || !argument_candidates ||
		!candidates || !p_candidate_count ||
		argument >= terms->term_count) {
		return -1;
	}
	*p_candidate_count = 0;
	for (uint32_t i = 0; i < function_candidate_count; ++i) {
		uint32_t function_pi;
		uint32_t domain;
		uint32_t codomain_family;
		int status = classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			function_candidates[i].classifier,
			&function_pi,
			&domain,
			&codomain_family
		);
		(void)codomain_family;
		if (status < 0) {
			return -1;
		}
		if (status > 0) {
			continue;
		}
		for (uint32_t j = 0; j < argument_candidate_count; ++j) {
			uint32_t result_classifier;
			if (!prototype_judgement_classifier_compatible(
					terms,
					type_declarations,
					domain,
					argument_candidates[j].classifier
				)) {
				continue;
			}
			if (pi_codomain_after_argument_in_context(
					contexts,
					substitutions,
					terms,
					type_declarations,
					context_id,
					function_pi,
					argument,
					argument_candidates[j].classifier,
					&result_classifier
				) != 0) {
				return -1;
			}
			int duplicate = 0;
			for (uint32_t k = 0; k < *p_candidate_count; ++k) {
				int result_equal;
				if (classifier_conversion_decision(
						prototype_judgement_classifier_conversion(
							terms,
							type_declarations,
							candidates[k].result_classifier,
							result_classifier
						),
						&result_equal
					) != 0) {
					return -1;
				}
				if (selected_evidence_equal(
						&candidates[k].function_evidence,
						&function_candidates[i]
					) && selected_evidence_equal(
						&candidates[k].argument_evidence,
						&argument_candidates[j]
					) && result_equal) {
					duplicate = 1;
					break;
				}
			}
			if (duplicate) {
				continue;
			}
			if (*p_candidate_count >= candidate_capacity) {
				return -1;
			}
			candidates[*p_candidate_count].function_evidence = function_candidates[i];
			candidates[*p_candidate_count].argument_evidence = argument_candidates[j];
			candidates[*p_candidate_count].function_pi = function_pi;
			candidates[*p_candidate_count].result_classifier = result_classifier;
			(*p_candidate_count)++;
		}
	}
	return 0;
}

static int collect_delta_app_classifier_candidates(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function,
	uint32_t argument,
	struct app_classifier_candidate* candidates,
	uint32_t candidate_capacity,
	uint32_t* p_candidate_count
) {
	if (!delta || !terms || !type_declarations ||
		function >= terms->term_count ||
		argument >= terms->term_count) {
		return -1;
	}
	struct prototype_judgement_selected_evidence function_candidates[32];
	struct prototype_judgement_selected_evidence argument_candidates[32];
	uint32_t function_candidate_count = 0;
	uint32_t argument_candidate_count = 0;
	if (collect_subject_evidence(
			delta,
			function,
			function_candidates,
			32,
			&function_candidate_count
		) != 0 ||
		collect_subject_evidence(
			delta,
			argument,
			argument_candidates,
			32,
			&argument_candidate_count
		) != 0) {
		return -1;
	}
	if (function_candidate_count == 0 || argument_candidate_count == 0) {
		return 1;
	}
	if (collect_app_classifier_candidates_from_candidates(
			delta->contexts,
			delta->substitutions,
			terms,
			type_declarations,
			delta->current_context_id,
			argument,
		function_candidates,
			function_candidate_count,
			argument_candidates,
			argument_candidate_count,
			candidates,
			candidate_capacity,
			p_candidate_count
		) != 0) {
		return -1;
	}
	return *p_candidate_count == 0 ? 1 : 0;
}

static int prototype_judgement_delta_resolve_induction_hypothesis_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		argument_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* ih = &terms->terms[subject];
	if (ih->as.induction_hypothesis.ih_scope_id >= terms->ih_scope_count ||
		ih->as.induction_hypothesis.argument >= terms->term_count) {
		return -1;
	}
	uint32_t match_term =
		terms->ih_scopes[ih->as.induction_hypothesis.ih_scope_id].match_term;
	uint32_t context_case_index = PROTOTYPE_INVALID_ID;
	uint32_t context_field_index = PROTOTYPE_INVALID_ID;
	int context_status = induction_hypothesis_context_from_argument(
		delta,
		terms,
		type_declarations,
		match_term,
		ih->as.induction_hypothesis.argument,
		&context_case_index,
		&context_field_index
	);
	if (context_status != 0) {
		return context_status < 0 ? -1 : 1;
	}
	uint32_t classifier;
	uint32_t motive;
	int status = induction_hypothesis_classifier_from_match_motive(
		delta,
		terms,
		type_declarations,
		match_term,
		ih->as.induction_hypothesis.argument,
		&classifier,
		&motive
	);
	if (status < 0) {
		return -1;
	}
	if (status > 0) {
		(void)argument_classifier;
		return 1;
	}
	return prototype_judgement_delta_expand_induction_hypothesis(
		delta,
		terms,
		subject,
		classifier,
		match_term,
		motive,
		context_case_index,
		context_field_index
	);
}

int prototype_judgement_delta_resolve_induction_hypothesis_request(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_induction_hypothesis_resolution_request* request
) {
	if (!delta || !terms || !type_declarations || !request ||
		!term_has_tag(terms, request->subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		request->argument >= terms->term_count ||
		terms->terms[request->argument].tag != PROTOTYPE_TERM_VAR ||
		request->ih_scope_id >= terms->ih_scope_count) {
		return -1;
	}
	const struct prototype_term* subject = &terms->terms[request->subject];
	if (subject->as.induction_hypothesis.ih_scope_id != request->ih_scope_id ||
		subject->as.induction_hypothesis.argument != request->argument) {
		return -1;
	}
	uint32_t ignored_existing_classifier;
	int has_existing_classifier =
		lookup_delta_proven_classifier(
			delta,
			terms,
			request->subject,
			&ignored_existing_classifier
		) == 0;

	uint32_t match_term = terms->ih_scopes[request->ih_scope_id].match_term;
	if (!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH)) {
		return 1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t binding_id = terms->terms[request->argument].as.var.binding_id;
	int found = 0;
	uint32_t context_case_index = PROTOTYPE_INVALID_ID;
	uint32_t context_field_index = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		int recursive = match_case_binder_is_recursive_self_field(
			delta,
			terms,
			type_declarations,
			&terms->cases[case_id],
			binding_id
		);
		if (recursive < 0) {
			return -1;
		}
		if (recursive == 1) {
			return 1;
		}
		if (recursive == 0) {
			for (uint32_t j = 0; j < terms->cases[case_id].binder_count; ++j) {
				if (terms->case_binders[terms->cases[case_id].first_binder + j].binding_id ==
					binding_id) {
					found = 1;
					context_case_index = i;
					context_field_index = j;
					break;
				}
			}
		}
	}
	if (!found) {
		return -1;
	}

	uint32_t classifier;
	uint32_t motive;
	int status = induction_hypothesis_classifier_from_match_motive(
		delta,
		terms,
		type_declarations,
		match_term,
		request->argument,
		&classifier,
		&motive
	);
	if (status < 0) {
		return -1;
	}
	if (status > 0) {
		if (has_existing_classifier) {
			return 0;
		}
		return 1;
	}
	if (status != 0) {
		return status;
	}
		struct prototype_judgement_selected_evidence existing_evidence;
		if (has_existing_classifier &&
			lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				request->subject,
				classifier,
				&existing_evidence
			) == 0) {
			return 0;
		}
		(void)has_existing_classifier;
		return prototype_judgement_delta_expand_induction_hypothesis(
		delta,
		terms,
		request->subject,
		classifier,
		match_term,
		motive,
		context_case_index,
		context_field_index
	);
}

static int type_instance_has_known_type(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	return prototype_term_type_instance_is_saturated(terms, type_declarations, term_id);
}

static int term_is_structural_type(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!term_exists(terms, term_id)) {
		return 0;
	}
		return term_is_universe_var(terms, term_id) ||
			term_is_primitive_text(terms, term_id) ||
			term_is_primitive_int(terms, term_id) ||
			term_is_primitive_int64(terms, term_id) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_EXTERNAL_REF) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_PI) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_COMPUTATION_TYPE) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_THUNK_TYPE) ||
		type_instance_has_known_type(terms, type_declarations, term_id);
}

static int infer_type_formation_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !delta->db || !terms || !type_declarations ||
		!type_instance_has_known_type(terms, type_declarations, term_id)) {
		return -1;
	}
	uint32_t existing_classifier;
	if (lookup_delta_proven_classifier(
			delta, terms, term_id, &existing_classifier
		) == 0 && term_is_universe_var(terms, existing_classifier)) {
		return 0;
	}
	uint32_t classifier;
	if (prototype_term_universe_var(
			terms,
			delta->db->next_universe_var++,
			&classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
	);
}

static int classifier_returns_owner(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t owner
) {
	(void)type_declarations;
	uint32_t current = classifier;
	while (current < terms->term_count) {
		int same_owner = 0;
		if (current == owner) {
			return 1;
		}
		if (prototype_term_core_shape_equal(
				terms, current, owner, &same_owner
			) != 0) {
			return 0;
		}
		if (same_owner) {
			return 1;
		}
		if (prototype_term_source_shape_equal(
				terms, current, owner, &same_owner
			) != 0) {
			return 0;
		}
		if (same_owner) {
			return 1;
		}
		uint32_t domain;
		uint32_t codomain_lambda;
		if (pi_parts(terms, current, &domain, &codomain_lambda) != 0) {
			return 0;
		}
		(void)domain;
		(void)codomain_lambda;
		uint32_t ignored_binder;
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[current].as.pi.codomain_family,
				&ignored_binder,
				&current
			) != 0) {
			return 0;
		}
	}
	return 0;
}

static int constructor_belongs_to_owner(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index
) {
	if (!terms || !type_declarations || owner >= terms->term_count) {
		return 0;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(
			terms, owner, &type_id, args, &arg_count
		) != 0) {
		uint32_t reversed[16];
		uint32_t count = 0;
		uint32_t current = owner;
		while (current < terms->term_count && terms->terms[current].tag == PROTOTYPE_TERM_APP) {
			if (count >= 16) {
				return 0;
			}
			reversed[count++] = terms->terms[current].as.app.argument;
			current = terms->terms[current].as.app.function;
		}
		if (current >= terms->term_count ||
			terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER ||
			prototype_type_declaration_representation_type_id(
				type_declarations,
				terms->terms[current].as.type_former.representation_id,
				&type_id
			) != 0) {
			return 0;
		}
		for (uint32_t i = 0; i < count; ++i) {
			args[i] = reversed[count - i - 1];
		}
		arg_count = count;
	}
	if (type_id >= type_declarations->type_count) {
		return 0;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count > UINT32_MAX - type->index_count ||
		arg_count != type->parameter_count + type->index_count) {
		return 0;
	}
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == constructor_index) {
			if (type->index_count == 0) {
				return 1;
			}
			/* Indexed constructors must specialize the exact owner indices. The
			 * first fragment admits only nullary generated identity constructors;
			 * field and parameter substitution is added with general IADT support. */
			int same_result = 0;
			return type->parameter_count == 0 &&
				constructor->field_context == constructor->parameter_context &&
				prototype_term_source_shape_equal(
					terms,
					constructor->result_classifier,
					owner,
					&same_result
				) == 0 && same_result;
		}
	}
	return 0;
}

static int match_case_has_valid_constructor(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case
) {
	if (!terms || !type_declarations || !match_case ||
		match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		match_case->constructor_id == PROTOTYPE_INVALID_ID ||
		!term_exists(terms, match_case->constructor_owner)) {
		return 0;
	}
	if (constructor_belongs_to_owner(
			terms,
			type_declarations,
			match_case->constructor_owner,
			match_case->constructor_id
		)) {
		return 1;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(
			terms,
			match_case->constructor_owner,
			&type_id,
			args,
			&arg_count
		) == 0 &&
		type_id < type_declarations->type_count) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[type_id];
		for (uint32_t i = 0; i < type->constructor_count; ++i) {
			const struct prototype_type_constructor_declaration* constructor =
				&type_declarations->constructor_declarations[
					type->first_constructor + i
				];
			if (constructor->owner_type == type_id &&
				constructor->constructor_index == match_case->constructor_id) {
				return 1;
			}
		}
		return 0;
	}
	(void)type_id;
	(void)args;
	(void)arg_count;
	return 1;
}

static int type_formation_is_nat_shape(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t term_id
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations || !contexts ||
		prototype_term_type_instance_info(terms, term_id, &type_id, args, &arg_count) != 0 ||
		type_id >= type_declarations->type_count ||
		arg_count != 0) {
		return 0;
	}
	(void)args;
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count != 0 || type->constructor_count != 2) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* zero = NULL;
	const struct prototype_type_constructor_declaration* succ = NULL;
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == 0) {
			zero = constructor;
		} else if (constructor->constructor_index == 1) {
			succ = constructor;
		}
	}
	if (!zero || !succ ||
		zero->owner_type != type_id ||
		succ->owner_type != type_id) {
		return 0;
	}

	uint32_t zero_fields[1];
	uint32_t zero_field_count;
	uint32_t succ_fields[1];
	uint32_t succ_field_count;
	if (prototype_context_extension_path(
			contexts,
			zero->parameter_context,
			zero->field_context,
			zero_fields,
			1,
			&zero_field_count
		) != 0 ||
		prototype_context_extension_path(
			contexts,
			succ->parameter_context,
			succ->field_context,
			succ_fields,
			1,
			&succ_field_count
		) != 0) {
		return 0;
	}
	if (zero_field_count != 0 || succ_field_count != 1) {
		return 0;
	}
	const struct prototype_context* succ_field =
		prototype_context_get(contexts, succ_fields[0]);
	uint32_t succ_classifier = prototype_context_classifier_term(succ_field);
	if (!succ_field || succ_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (!(prototype_judgement_classifier_conversion(
			(struct prototype_term_db*)terms,
			(struct prototype_type_declaration_db*)type_declarations,
			succ_classifier,
			term_id
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 0;
	}
	return 1;
}

static int type_formation_has_name_symbol(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	int name_symbol_id
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations || name_symbol_id < 0 ||
		prototype_term_type_instance_info(terms, term_id, &type_id, args, &arg_count) != 0 ||
		type_id >= type_declarations->type_count ||
		arg_count != 0) {
		return 0;
	}
	(void)args;
	return type_declarations->type_declarations[type_id].name_symbol_id == name_symbol_id;
}

static void candidate_claim_authority(
	const struct prototype_judgement_proposition* relation,
	int* p_authority_kind,
	uint32_t* p_authority_id
);

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
		left->classifier == right->classifier;
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
		if (!premise || prototype_judgement_proposition_get(judgement, premise->proposition_id)->kind != candidate_proof->premises[i].proposition.kind ||
			prototype_judgement_proposition_get(judgement, premise->proposition_id)->context_id != candidate_proof->premises[i].proposition.context_id ||
			prototype_judgement_proposition_get(judgement, premise->proposition_id)->subject != candidate_proof->premises[i].proposition.subject ||
			prototype_judgement_proposition_get(judgement, premise->proposition_id)->classifier != candidate_proof->premises[i].proposition.classifier ||
			prototype_judgement_proposition_get(judgement, premise->proposition_id)->authority_kind !=
				candidate_proof->premises[i].proposition.authority_kind ||
			prototype_judgement_proposition_get(judgement, premise->proposition_id)->authority_id != candidate_proof->premises[i].proposition.authority_id ||
			prototype_judgement_proposition_get(judgement, premise->proposition_id)->operation_id != candidate_proof->premises[i].proposition.operation_id) {
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
	struct prototype_judgement_proposition normalized_relation =
		*candidate_relation;
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
	for (uint32_t i = 0; i < branch_count; ++i) {
		uint32_t case_id = terms->terms[match_term].as.match.first_case + i;
		if (case_id >= terms->case_count ||
			branch_operation_ids[i] == PROTOTYPE_INVALID_ID ||
			branch_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			branch_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
			branch_evidence[i].classifier >= terms->term_count) {
			return -1;
		}
		premise_subjects[i] = terms->cases[case_id].body;
		if (branch_evidence[i].subject != premise_subjects[i]) {
			return -1;
		}
		premise_context_ids[i] = branch_evidence[i].context_id;
		premise_classifiers[i] = branch_evidence[i].classifier;
	}
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		match_term,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE,
		premise_context_ids,
		branch_operation_ids,
		premise_subjects,
		premise_classifiers,
		branch_evidence,
		branch_count
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

int prototype_judgement_expand_lambda(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement) {
		return -1;
	}
	struct prototype_judgement_proposition relations[4];
	struct prototype_judgement_derivation_candidate proofs[4];
	struct prototype_judgement_candidate_premise premises[
		4 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result motives[1];
	struct prototype_judgement_computation_constraint constraints[1];
	struct prototype_judgement_effect_row_constraint effect_rows[1];
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta, judgement, relations, proofs, 4,
		premises, 4 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motives, 1, constraints, 1, effect_rows, 1
	);
	int status = prototype_judgement_delta_expand_lambda(
		&delta, terms, type_declarations, subject, classifier
	);
	return status == 0 ? prototype_judgement_delta_commit(&delta, 0) : status;
}

int prototype_judgement_delta_expand_lambda(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[subject];
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t binder_classifier;
	uint32_t body_classifier;
	struct prototype_judgement_selected_evidence body_evidence;
	uint32_t expected_body_classifier;
	uint32_t binder_var;
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		classifier,
		&lambda_pi,
		&domain,
		&codomain_family
	);
	(void)codomain_family;
	if (status != 0) {
		return -1;
	}
	if (prototype_term_var(terms, lambda->as.lambda.binding_id, &binder_var) != 0) {
		return -1;
	}
	if (pi_codomain_at_fresh_binder(
			delta,
			terms,
			type_declarations,
			delta->current_context_id,
			lambda_pi,
			binder_var,
			domain,
			PROTOTYPE_INVALID_ID,
			&expected_body_classifier
		) != 0) {
		return -1;
	}
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			lambda->as.lambda.body,
			expected_body_classifier,
			&body_evidence
		) != 0) {
		return -1;
	}
	body_classifier = body_evidence.classifier;
	for (size_t i = delta->proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation = &delta->propositions[i - 1];
		if (candidate_has_derivation_kind(
			delta->propositions,
			delta->proposition_count,
			delta->derivation_candidates,
			delta->derivation_candidate_count,
			(uint32_t)(i - 1),
			PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
		) != 1 ||
			relation->subject >= terms->term_count ||
			terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
			terms->terms[relation->subject].as.var.binding_id != lambda->as.lambda.binding_id) {
			continue;
		}
		binder_classifier = relation->classifier;
		int equal;
		if (classifier_conversion_decision(
				prototype_judgement_classifier_conversion(
					terms, type_declarations, domain, binder_classifier
				),
				&equal
			) != 0) {
			return -1;
		}
			if (!equal) {
				continue;
			}
		uint32_t premise_subjects[2] = {
			relation->subject,
			lambda->as.lambda.body
		};
		uint32_t premise_classifiers[2] = {
			binder_classifier,
			body_classifier
		};
		struct prototype_judgement_selected_evidence premise_evidence[2];
		selected_evidence_from_candidate(relation, &premise_evidence[0]);
		premise_evidence[1] = body_evidence;
			int relation_status = add_delta_relation_with_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				classifier,
				PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
				NULL,
				NULL,
				premise_subjects,
				premise_classifiers,
				premise_evidence,
				2
			);
			return relation_status;
		}
	if (delta->db) {
		for (size_t i = delta->db->proposition_count; i > 0; --i) {
			const struct prototype_judgement_proposition* relation = &delta->db->propositions[i - 1];
			if (candidate_has_derivation_kind(
				delta->db->propositions,
				delta->db->proposition_count,
				delta->db->derivation_candidates,
				delta->db->derivation_candidate_count,
				(uint32_t)(i - 1),
				PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
			) != 1 ||
				relation->subject >= terms->term_count ||
				terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
				terms->terms[relation->subject].as.var.binding_id != lambda->as.lambda.binding_id) {
				continue;
			}
			binder_classifier = relation->classifier;
			int equal;
			if (classifier_conversion_decision(
					prototype_judgement_classifier_conversion(
						terms, type_declarations, domain, binder_classifier
					),
					&equal
				) != 0) {
				return -1;
			}
			if (!equal) {
				continue;
			}
			uint32_t premise_subjects[2] = {
				relation->subject,
				lambda->as.lambda.body
			};
			uint32_t premise_classifiers[2] = {
				binder_classifier,
				body_classifier
			};
			struct prototype_judgement_selected_evidence premise_evidence[2];
			selected_evidence_from_candidate(relation, &premise_evidence[0]);
			premise_evidence[1] = body_evidence;
			int relation_status = add_delta_relation_with_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				classifier,
				PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
				NULL,
				NULL,
				premise_subjects,
				premise_classifiers,
				premise_evidence,
				2
			);
			return relation_status;
		}
	}
	return -1;
}

int prototype_judgement_expand_app(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!judgement) {
		return -1;
	}
	struct prototype_judgement_proposition relations[64];
	struct prototype_judgement_derivation_candidate proofs[64];
	struct prototype_judgement_candidate_premise premises[
		64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	struct prototype_judgement_match_motive_result motives[1];
	struct prototype_judgement_computation_constraint constraints[1];
	struct prototype_judgement_effect_row_constraint effect_rows[1];
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta, judgement, relations, proofs, 64,
		premises, 64 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motives, 1, constraints, 1, effect_rows, 1
	);
	int status = prototype_judgement_delta_expand_app(
		&delta, terms, type_declarations, subject, p_classifier
	);
	return status == 0 ? prototype_judgement_delta_commit(&delta, 0) : status;
}

static int infer_lambda_classifier_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	uint32_t argument_classifier
);

static int ensure_lambda_binder_assumption(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t lambda_term,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !term_has_tag(terms, lambda_term, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	uint32_t binder_var;
	if (prototype_term_var(terms, lambda->as.lambda.binding_id, &binder_var) != 0) {
		return -1;
	}
	uint32_t binder_context;
	if (!delta->contexts || prototype_context_extend(
			delta->contexts,
			delta->current_context_id,
			lambda->as.lambda.binding_id,
			argument_classifier,
			PROTOTYPE_INVALID_ID,
			&binder_context
		) != 0) {
		return -1;
	}
	for (size_t i = delta->proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation = &delta->propositions[i - 1];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate_has_derivation_kind(
				delta->propositions,
				delta->proposition_count,
				delta->derivation_candidates,
				delta->derivation_candidate_count,
				(uint32_t)(i - 1),
				PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
			) != 1 ||
			relation->context_id != binder_context ||
			relation->subject != binder_var ||
			relation->classifier != argument_classifier) {
			continue;
		}
		return 0;
	}
	size_t before = delta->proposition_count;
	uint32_t outer_context = delta->current_context_id;
	uint32_t outer_operation = delta->current_operation_id;
	prototype_judgement_delta_set_context(delta, binder_context);
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	if (prototype_judgement_delta_record_context_binding_assumption(
			delta, terms, lambda->as.lambda.binding_id, argument_classifier
		) != 0 || delta->proposition_count != before + 1) {
		prototype_judgement_delta_set_context(delta, outer_context);
		prototype_judgement_delta_set_operation(delta, outer_operation);
		return -1;
	}
	prototype_judgement_delta_set_context(delta, outer_context);
	prototype_judgement_delta_set_operation(delta, outer_operation);
	return 0;
}

int prototype_judgement_delta_expand_app(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!delta || !term_has_tag(terms, subject, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	/* Generic Core inference does not own a typed source occurrence. Operation
	 * reification uses prototype_judgement_delta_record_app_elim() after
	 * selecting the owning Operation explicitly. */
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	const struct prototype_term* app = &terms->terms[subject];
	struct app_classifier_candidate candidates[64];
	uint32_t candidate_count = 0;
	int select_status = collect_delta_app_classifier_candidates(
			delta,
			terms,
			type_declarations,
			app->as.app.function,
			app->as.app.argument,
			candidates,
			64,
			&candidate_count
		);
	if (select_status < 0) {
		return -1;
	}
	if (select_status > 0) {
		uint32_t argument_candidates[32];
		uint32_t argument_candidate_count = 0;
		if (collect_subject_classifiers(
				delta,
				terms,
				type_declarations,
				app->as.app.argument,
				argument_candidates,
				32,
				&argument_candidate_count
			) != 0) {
			return -1;
		}
		if (argument_candidate_count == 0) {
			return 1;
		}
		if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
			for (uint32_t i = 0; i < argument_candidate_count; ++i) {
				(void)infer_lambda_classifier_for_app_argument(
					delta,
					terms,
					type_declarations,
					app->as.app.function,
					argument_candidates[i]
				);
			}
		}
		if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS)) {
			for (uint32_t i = 0; i < argument_candidate_count; ++i) {
				int ih_status =
					prototype_judgement_delta_resolve_induction_hypothesis_for_app_argument(
						delta,
						terms,
						type_declarations,
						app->as.app.function,
						argument_candidates[i]
					);
				if (ih_status < 0) {
					return -1;
				}
				}
			}
			select_status = collect_delta_app_classifier_candidates(
					delta,
					terms,
					type_declarations,
					app->as.app.function,
					app->as.app.argument,
					candidates,
					64,
					&candidate_count
				);
			if (select_status < 0) {
				return -1;
		}
		if (select_status > 0) {
				return 1;
			}
		}
		uint32_t first_classifier = candidates[0].result_classifier;
		if (p_classifier) {
			*p_classifier = first_classifier;
		}
		for (uint32_t i = 0; i < candidate_count; ++i) {
			uint32_t premise_subjects[2] = {
				app->as.app.function,
				app->as.app.argument
			};
		uint32_t premise_classifiers[2] = {
			candidates[i].function_evidence.classifier,
			candidates[i].argument_evidence.classifier
		};
		struct prototype_judgement_selected_evidence premise_evidence[2] = {
			candidates[i].function_evidence,
			candidates[i].argument_evidence
		};
		if (add_delta_relation_with_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				candidates[i].result_classifier,
				PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
				NULL,
				NULL,
				premise_subjects,
				premise_classifiers,
				premise_evidence,
				2
			) != 0) {
				return -1;
			}
		}
		return 0;
	}

static int lambda_body_classifier_matches_binder(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t body,
	uint32_t binding_id,
	uint32_t binder_classifier,
	uint32_t body_classifier
) {
	if (!terms || !type_declarations || body >= terms->term_count) {
		return 0;
	}
	if (terms->terms[body].tag == PROTOTYPE_TERM_VAR &&
		terms->terms[body].as.var.binding_id == binding_id) {
		return (prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			binder_classifier,
			body_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL);
	}
	if (terms->terms[body].tag == PROTOTYPE_TERM_RETURN) {
		uint32_t value = terms->terms[body].as.return_term.value;
		struct prototype_term_classifier_view view;
		if (value >= terms->term_count) {
			return 0;
		}
		if (terms->terms[value].tag != PROTOTYPE_TERM_VAR ||
			terms->terms[value].as.var.binding_id != binding_id) {
			return 1;
		}
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, body_classifier, &view
			) != 0 || view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return 0;
		}
		return (prototype_judgement_classifier_conversion(
			terms, type_declarations, binder_classifier, view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL);
	}
	return 1;
}

static int infer_lambda_classifier_pair(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	const struct prototype_judgement_selected_evidence* binder_evidence,
	const struct prototype_judgement_selected_evidence* body_evidence,
	int* p_changed
) {
	if (!delta || !terms || !type_declarations || !p_changed ||
		!binder_evidence || !body_evidence ||
		binder_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		body_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		binder_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		body_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		lambda_term >= terms->term_count ||
		terms->terms[lambda_term].tag != PROTOTYPE_TERM_LAMBDA ||
		binder_evidence->subject >= terms->term_count ||
		body_evidence->subject >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	uint32_t binder_subject = binder_evidence->subject;
	uint32_t binder_classifier = binder_evidence->classifier;
	uint32_t body_classifier = body_evidence->classifier;
	if (body_evidence->subject != lambda->as.lambda.body ||
		binder_evidence->context_id != body_evidence->context_id) {
		return -1;
	}
	if (!lambda_body_classifier_matches_binder(
			terms,
			type_declarations,
			lambda->as.lambda.body,
			lambda->as.lambda.binding_id,
			binder_classifier,
			body_classifier
		)) {
		return 0;
	}
	uint32_t codomain_family;
	uint32_t classifier;
	/* PI stores a dependent codomain as a binder family.  In the single
	 * TermDB that family is encoded by the canonical pure THUNK/LAMBDA/RETURN
	 * representation, even when its body is a computation classifier.  This
	 * is a binder representation, not an extra Comp({}, PI(...)) wrapper. */
	if (prototype_term_pure_family(
			terms,
			lambda->as.lambda.binding_id,
			body_classifier,
			&codomain_family
		) != 0 ||
		prototype_term_pi_family(
			terms,
			binder_classifier,
			codomain_family,
			&classifier
		) != 0) {
		return -1;
	}
	uint32_t premise_subjects[2] = {
		binder_subject,
		lambda->as.lambda.body
	};
	uint32_t premise_classifiers[2] = {
		binder_classifier,
		body_classifier
	};
	uint32_t premise_context_ids[2] = {
		binder_evidence->context_id,
		body_evidence->context_id
	};
	struct prototype_judgement_selected_evidence premise_evidence[2] = {
		*binder_evidence,
		*body_evidence
	};
	size_t before = delta->proposition_count;
	if (add_delta_relation_with_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			lambda_term,
			classifier,
			PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			premise_context_ids,
			NULL,
			premise_subjects,
			premise_classifiers,
			premise_evidence,
			2
		) != 0) {
		return -1;
	}
	if (delta->proposition_count > before) {
		*p_changed = 1;
	}
	return 0;
}

static int infer_lambda_classifiers_from_body(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	int* p_changed
) {
	if (!delta || !terms || !type_declarations || !p_changed ||
		lambda_term >= terms->term_count ||
		terms->terms[lambda_term].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_proposition* binder_relations =
			source == 0 ? delta->propositions : delta->db->propositions;
		const struct prototype_judgement_derivation_candidate* binder_derivations =
			source == 0 ? delta->derivation_candidates :
				delta->db->derivation_candidates;
		size_t binder_count =
			source == 0 ? delta->proposition_count : delta->db->proposition_count;
		size_t binder_derivation_count = source == 0 ?
			delta->derivation_candidate_count :
			delta->db->derivation_candidate_count;
		for (size_t i = 0; i < binder_count; ++i) {
			const struct prototype_judgement_proposition* binder_relation =
				&binder_relations[i];
			if (binder_relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				candidate_has_derivation_kind(
					binder_relations,
					binder_count,
					binder_derivations,
					binder_derivation_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION
				) != 1 ||
				binder_relation->subject >= terms->term_count ||
				terms->terms[binder_relation->subject].tag != PROTOTYPE_TERM_VAR ||
				terms->terms[binder_relation->subject].as.var.binding_id !=
					lambda->as.lambda.binding_id) {
				continue;
			}
			for (int body_source = 0; body_source < 2; ++body_source) {
				const struct prototype_judgement_proposition* body_relations =
					body_source == 0 ? delta->propositions : delta->db->propositions;
				const struct prototype_judgement_derivation_candidate* body_derivations =
					body_source == 0 ? delta->derivation_candidates :
						delta->db->derivation_candidates;
				size_t body_count =
					body_source == 0 ? delta->proposition_count : delta->db->proposition_count;
				size_t body_derivation_count = body_source == 0 ?
					delta->derivation_candidate_count :
					delta->db->derivation_candidate_count;
				for (size_t j = 0; j < body_count; ++j) {
					const struct prototype_judgement_proposition* body_relation =
						&body_relations[j];
					if (body_relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						candidate_has_derivation_other_than(
							body_relations,
							body_count,
							body_derivations,
							body_derivation_count,
							(uint32_t)j,
							PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN
						) != 1 ||
						body_relation->context_id != binder_relation->context_id ||
						body_relation->subject != lambda->as.lambda.body) {
						continue;
					}
					struct prototype_judgement_selected_evidence binder_evidence;
					struct prototype_judgement_selected_evidence body_evidence;
					selected_evidence_from_candidate(
						binder_relation, &binder_evidence
					);
					selected_evidence_from_candidate(body_relation, &body_evidence);
					if (infer_lambda_classifier_pair(
							delta,
							terms,
							type_declarations,
							lambda_term,
							&binder_evidence,
							&body_evidence,
							p_changed
						) != 0) {
						return -1;
					}
				}
			}
		}
	}
	return 0;
}

static int infer_lambda_classifier_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, lambda_term, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	uint32_t saved_operation_id = delta->current_operation_id;
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	if (ensure_lambda_binder_assumption(
			delta, terms, lambda_term, argument_classifier
		) != 0) {
		prototype_judgement_delta_set_operation(delta, saved_operation_id);
		return -1;
	}
	int changed = 0;
	int status = infer_lambda_classifiers_from_body(
		delta,
		terms,
		type_declarations,
		lambda_term,
		&changed
	);
	prototype_judgement_delta_set_operation(delta, saved_operation_id);
	return status;
}

/* A source lambda can have several provisional classifier candidates while the
 * operation graph fixed point is being solved. Consumers with a known input
 * domain must select the compatible Pi candidate, rather than depend on
 * relation insertion order. */
static int select_delta_pi_classifier_for_domain(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected_domain,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !terms || !type_declarations || !selected ||
		subject >= terms->term_count || expected_domain >= terms->term_count) {
		return -1;
	}
	struct prototype_judgement_selected_evidence candidates[32];
	uint32_t candidate_count;
	if (collect_subject_evidence(
			delta,
			subject,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	int found = 0;
	for (uint32_t i = 0; i < candidate_count; ++i) {
		uint32_t pi;
		uint32_t domain;
		uint32_t family;
		if (classifier_kernel_as_pi(
				terms,
				type_declarations,
				NULL,
				candidates[i].classifier,
				&pi,
				&domain,
				&family
			) != 0) {
			continue;
		}
		int domain_equal;
		if (classifier_conversion_decision(
				prototype_judgement_classifier_conversion(
					terms, type_declarations, domain, expected_domain
				),
				&domain_equal
			) != 0) {
			return -1;
		}
		if (!domain_equal) {
			continue;
		}
		if (!found) {
			*selected = candidates[i];
			found = 1;
		} else {
			int classifier_equal;
			if (classifier_conversion_decision(
					prototype_judgement_classifier_conversion(
						terms, type_declarations, selected->classifier, pi
					),
					&classifier_equal
				) != 0) {
				return -1;
			}
			if (classifier_equal) {
				if (!selected_evidence_equal(selected, &candidates[i])) {
					return 2;
				}
				continue;
			}
			uint32_t selected_domain;
			uint32_t selected_family;
			uint32_t selected_family_binder;
			uint32_t selected_body;
			uint32_t candidate_family_binder;
			uint32_t candidate_body;
			struct prototype_term_classifier_view selected_view;
			struct prototype_term_classifier_view candidate_view;
			unsigned selected_effects;
			unsigned candidate_effects;
			if (classifier_kernel_as_pi(
					terms,
					type_declarations,
					NULL,
					selected->classifier,
					NULL,
					&selected_domain,
					&selected_family
				) != 0 || prototype_term_pure_family_parts(
					terms,
					selected_family,
					&selected_family_binder,
					&selected_body
				) != 0 || prototype_term_pure_family_parts(
					terms,
					family,
					&candidate_family_binder,
					&candidate_body
				) != 0 || prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					selected_body,
					&selected_view
				) != 0 || prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					candidate_body,
					&candidate_view
				) != 0 || selected_view.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				candidate_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				selected_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
				candidate_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
				!(prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					selected_view.result,
					candidate_view.result
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_effect_row_closed_bits(
					terms, selected_view.effect_row, &selected_effects
				) != 0 || prototype_term_effect_row_closed_bits(
					terms, candidate_view.effect_row, &candidate_effects
				) != 0) {
				return -1;
			}
			if ((candidate_effects & selected_effects) == candidate_effects) {
				*selected = candidates[i];
			} else if ((selected_effects & candidate_effects) != selected_effects) {
				return -1;
			}
			(void)selected_domain;
			(void)selected_family_binder;
			(void)candidate_family_binder;
		}
	}
	return found ? 0 : 1;
}

static int operation_named_nat_type_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	int nat_symbol_id,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret || nat_symbol_id < 0) {
		return -1;
	}
	const struct prototype_type_declaration* nat_type =
		prototype_type_declaration_lookup(type_declarations, nat_symbol_id);
	if (!nat_type) {
		return -1;
	}
	return prototype_term_type_instance_make(
		terms,
		type_declarations,
		nat_type->type_index,
		NULL,
		0,
		p_ret
	);
}

static int host_signature_classifier(
	struct prototype_term_db* terms,
	uint32_t arity,
	const int* argument_types,
	int result_type,
	unsigned effects,
	uint32_t* p_ret
) {
	if (!terms || !argument_types || !p_ret ||
		result_type == PROTOTYPE_HOST_TYPE_INVALID) {
		return 1;
	}
	for (uint32_t i = 0; i < arity; ++i) {
		if (argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID) {
			return 1;
		}
	}
	uint32_t current;
	if (prototype_term_make_host_type(terms, result_type, &current) != 0) {
		return -1;
	}
	uint32_t effect_label;
	if (prototype_term_effect_label(terms, effects, &effect_label) != 0 ||
		prototype_term_computation_type(terms, effect_label, current, &current) != 0) {
		return -1;
	}
	for (uint32_t i = arity; i > 0; --i) {
		int argument_type = argument_types[i - 1];
		uint32_t domain;
		if (prototype_term_make_host_type(terms, argument_type, &domain) != 0 ||
			prototype_term_pi(terms, domain, current, &current) != 0) {
			return -1;
		}
	}
	*p_ret = current;
	return 0;
}

int prototype_judgement_pure_primitive_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term* operation,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !operation || !p_ret ||
		operation->tag != PROTOTYPE_TERM_PURE_PRIMITIVE) {
		return -1;
	}
	const struct prototype_pure_primitive_declaration* signature =
		prototype_term_pure_primitive_declaration(operation->as.pure_primitive.primitive_id);
	if (signature) {
		int status = host_signature_classifier(
			terms,
			signature->arity,
			signature->argument_types,
			signature->result_type,
			PROTOTYPE_EFFECT_OPERATION_LABEL_NONE,
			p_ret
		);
		if (status <= 0) {
			return status;
		}
	}

	uint32_t text;
	uint32_t nat;
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0) {
		return -1;
	}
	switch (operation->as.pure_primitive.primitive_id) {
		case PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT: {
			if (operation_named_nat_type_classifier(
					terms,
					type_declarations,
					operation->as.pure_primitive.type_symbol_id,
					&nat
				) != 0) {
				return -1;
			}
			uint32_t effects;
			uint32_t result;
			if (prototype_term_effect_label(terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &effects) != 0 ||
				prototype_term_computation_type(terms, effects, nat, &result) != 0) {
				return -1;
			}
			return prototype_term_pi(terms, text, result, p_ret);
		}
		case PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT: {
			if (operation_named_nat_type_classifier(
					terms,
					type_declarations,
					operation->as.pure_primitive.type_symbol_id,
					&nat
				) != 0) {
				return -1;
			}
			uint32_t effects;
			uint32_t result;
			if (prototype_term_effect_label(terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &effects) != 0 ||
				prototype_term_computation_type(terms, effects, text, &result) != 0) {
				return -1;
			}
			return prototype_term_pi(terms, nat, result, p_ret);
		}
		default:
			return -1;
	}
	}

int prototype_judgement_effect_operation_classifier(
	struct prototype_term_db* terms,
	const struct prototype_term* operation,
	uint32_t* p_ret
) {
	if (!terms || !operation || !p_ret ||
		operation->tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	if (!prototype_term_effect_operation_declaration(
			operation->as.effect_operation.operation_id
		) || operation->as.effect_operation.classifier >= terms->term_count) {
		return -1;
	}
	*p_ret = operation->as.effect_operation.classifier;
	return 0;
}

static int infer_text_literal_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !terms || !term_has_tag(terms, term_id, PROTOTYPE_TERM_TEXT_LITERAL)) {
		return -1;
	}
	uint32_t text;
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		text,
		PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO
	);
}

static int infer_int_literal_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !terms || !term_has_tag(terms, term_id, PROTOTYPE_TERM_INT_LITERAL)) {
		return -1;
	}
	uint32_t integer;
	uint32_t integer64;
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT64, &integer64) != 0 ||
		add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			term_id,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			term_id,
			integer64,
			PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO
		) != 0) {
		return -1;
	}
	if (!int_literal_fits_int32(terms->terms[term_id].as.int_literal.value)) {
		return 0;
	}
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT32, &integer) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		integer,
			PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO
		);
}

static int infer_pure_primitive_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_PURE_PRIMITIVE)) {
		return -1;
	}
	uint32_t classifier;
	if (prototype_judgement_pure_primitive_classifier(
			terms,
			type_declarations,
			&terms->terms[term_id],
			&classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO
	);
}

static int infer_effect_operation_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !terms ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_EFFECT_OPERATION)) {
		return -1;
	}
	uint32_t classifier;
	if (prototype_judgement_effect_operation_classifier(
			terms, &terms->terms[term_id], &classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
		term_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO
	);
}

static int infer_constructor_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_CONSTRUCTOR)) {
		return -1;
	}
	uint32_t constructor_term;
	uint32_t classifier;
	const struct prototype_term* term = &terms->terms[term_id];
	return materialize_constructor_classifier(
		delta,
		terms,
		type_declarations,
		term->as.constructor.owner,
		term->as.constructor.constructor_id,
		&constructor_term,
		&classifier
	);
}

int prototype_judgement_cbpv_boundary_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t child_classifier,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !p_classifier || term_id >= terms->term_count ||
		child_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	uint32_t classifier;
	if (term->tag == PROTOTYPE_TERM_RETURN) {
		uint32_t empty_effects;
		if (prototype_term_effect_label(terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects) != 0 ||
			prototype_term_computation_type(terms, empty_effects, child_classifier, &classifier) != 0) {
			return -1;
		}
	} else if (term->tag == PROTOTYPE_TERM_THUNK) {
		struct prototype_term_classifier_view view;
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, child_classifier, &view
			) != 0 ||
			view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			prototype_term_thunk_type(terms, child_classifier, &classifier) != 0) {
			return -1;
		}
	} else if (term->tag == PROTOTYPE_TERM_FORCE) {
		uint32_t value_whnf;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, child_classifier, &value_whnf
			) != 0 || value_whnf >= terms->term_count ||
			terms->terms[value_whnf].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			return -1;
		}
		classifier = terms->terms[value_whnf].as.thunk_type.computation;
	} else {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}

static int record_cbpv_boundary_with_evidence(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t child_operation_id,
	uint32_t child_classifier,
	const struct prototype_judgement_selected_evidence* child_evidence
) {
	if (!delta || !terms || !type_declarations || term_id >= terms->term_count ||
		child_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	uint32_t child;
	int proof_kind;
	uint32_t classifier;
	if (prototype_judgement_cbpv_boundary_classifier(
			terms, type_declarations, term_id, child_classifier, &classifier
		) != 0) {
		return -1;
	}
	if (term->tag == PROTOTYPE_TERM_RETURN) {
		child = term->as.return_term.value;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO;
	} else if (term->tag == PROTOTYPE_TERM_THUNK) {
		child = term->as.thunk.computation;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO;
	} else if (term->tag == PROTOTYPE_TERM_FORCE) {
		child = term->as.force.value;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM;
	} else {
		return -1;
	}
	uint32_t premise_subjects[1] = { child };
	uint32_t premise_classifiers[1] = { child_classifier };
	uint32_t premise_operation_ids[1] = { child_operation_id };
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		proof_kind,
		NULL,
		premise_operation_ids,
		premise_subjects,
		premise_classifiers,
		child_evidence,
		1
	);
}

int prototype_judgement_delta_record_cbpv_boundary(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t child_operation_id,
	const struct prototype_judgement_selected_evidence* child_evidence
) {
	if (!child_evidence ||
		child_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		child_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
		return -1;
	}
	return record_cbpv_boundary_with_evidence(
		delta,
		terms,
		type_declarations,
		term_id,
		child_operation_id,
		child_evidence->classifier,
		child_evidence
	);
}

static int infer_cbpv_boundary_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t child;
	switch (terms->terms[term_id].tag) {
		case PROTOTYPE_TERM_RETURN:
			child = terms->terms[term_id].as.return_term.value;
			break;
		case PROTOTYPE_TERM_THUNK:
			child = terms->terms[term_id].as.thunk.computation;
			break;
		case PROTOTYPE_TERM_FORCE:
			child = terms->terms[term_id].as.force.value;
			break;
		default:
			return -1;
	}
	struct prototype_judgement_selected_evidence child_evidence;
	int selection_status = lookup_delta_selected_evidence(
		delta, child, 1, &child_evidence
	);
	if (selection_status != 0) {
		return 1;
	}
	return record_cbpv_boundary_with_evidence(
		delta,
		terms,
		type_declarations,
		term_id,
		child_evidence.operation_id,
		child_evidence.classifier,
		&child_evidence
	);
}

int prototype_judgement_delta_infer_cbpv_boundaries(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	for (;;) {
		int changed = 0;
		for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
			int tag = terms->terms[i].tag;
			if (tag != PROTOTYPE_TERM_RETURN &&
				tag != PROTOTYPE_TERM_THUNK &&
				tag != PROTOTYPE_TERM_FORCE) {
				continue;
			}
			uint32_t ignored;
			if (lookup_delta_proven_classifier(delta, terms, i, &ignored) == 0) {
				continue;
			}
			size_t before = delta->proposition_count;
			int status = infer_cbpv_boundary_classifier(delta, terms, type_declarations, i);
			if (status < 0) {
				return -1;
			}
			if (delta->proposition_count > before) {
				changed = 1;
			}
		}
		if (!changed) {
			break;
		}
	}
	return 0;
}

int prototype_judgement_delta_record_computation_constraint(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject
) {
	if (!delta || !terms ||
		(delta->computation_constraint_capacity != 0 &&
			!delta->computation_constraints) ||
		subject >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[subject];
	struct prototype_judgement_computation_constraint constraint;
	memset(&constraint, 0, sizeof(constraint));
	constraint.context_id = context_id;
	constraint.operation_id = delta->current_operation_id;
	constraint.subject = subject;
	constraint.effect_residual_pending = 0;
	constraint.effect_residual_row = PROTOTYPE_INVALID_ID;
	constraint.effect_output_row = PROTOTYPE_INVALID_ID;
	constraint.solved_classifier = PROTOTYPE_INVALID_ID;
	if (term->tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
		constraint.kind = PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD;
		constraint.computation = term->as.computation_fold.computation;
		constraint.continuation = term->as.computation_fold.return_clause;
		constraint.argument = PROTOTYPE_INVALID_ID;
		constraint.application = PROTOTYPE_INVALID_ID;
	} else if (term->tag == PROTOTYPE_TERM_OPERATION_REQUEST) {
		constraint.kind =
			PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST;
		constraint.computation = term->as.operation_request.operation;
		constraint.continuation = term->as.operation_request.continuation;
		constraint.argument = term->as.operation_request.argument;
		constraint.application = PROTOTYPE_INVALID_ID;
	} else {
		return 1;
	}
	for (size_t i = 0; i < delta->computation_constraint_count; ++i) {
		const struct prototype_judgement_computation_constraint* existing =
			&delta->computation_constraints[i];
		if (existing->kind == constraint.kind &&
			existing->context_id == context_id &&
			existing->operation_id == constraint.operation_id &&
			existing->subject == subject) {
			return 0;
		}
	}
	if (reserve_slot(
			delta->computation_constraint_count,
			delta->computation_constraint_capacity
		) != 0) {
		return -1;
	}
	delta->computation_constraints[delta->computation_constraint_count++] =
		constraint;
	return 0;
}

int prototype_judgement_delta_generate_computation_constraints(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms
) {
	if (!delta || !terms) {
		return -1;
	}
	uint32_t saved_operation_id = delta->current_operation_id;
	delta->current_operation_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		int status = prototype_judgement_delta_record_computation_constraint(
			delta, terms, delta->current_context_id, i
		);
		if (status < 0) {
			delta->current_operation_id = saved_operation_id;
			return -1;
		}
	}
	delta->current_operation_id = saved_operation_id;
	return 0;
}

/* A zero-clause computation fold sequences a returning computation through its
 * return algebra. The result remains a returning computation. */
int prototype_judgement_computation_fold_result_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t input_computation,
	uint32_t input_classifier,
	uint32_t continuation_classifier,
	uint32_t* p_ret
) {
	struct prototype_term_classifier_view input_view;
	struct prototype_term_classifier_view continuation_view;
	uint32_t continuation_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t codomain_binder;
	uint32_t codomain;
	uint32_t normalized_input;
	struct prototype_term_normalization_result input_normalization;
	uint32_t codomain_lambda;
	uint32_t applied_codomain;
	uint32_t normalized_codomain;
	if (!terms || !type_declarations || !p_ret ||
		input_computation >= terms->term_count || input_classifier >= terms->term_count ||
		continuation_classifier >= terms->term_count ||
		prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		) != 0 ||
		input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			continuation_classifier,
			&continuation_pi,
			&domain,
			&codomain_family
		) != 0 || prototype_term_pure_family_parts(
			terms, codomain_family, &codomain_binder, &codomain
		) != 0 || !(prototype_judgement_classifier_conversion(
			terms, type_declarations, domain, input_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	if (!prototype_term_contains_free_binding(
			terms, codomain, codomain_binder
		)) {
		applied_codomain = codomain;
	} else {
		if (prototype_term_normalize_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				input_computation,
				PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
				&input_normalization
			) != 0 || input_normalization.status ==
				PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			return -1;
		}
		if (input_normalization.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			return 1;
		}
		normalized_input = input_normalization.term_id;
		if (normalized_input >= terms->term_count ||
			terms->terms[normalized_input].tag != PROTOTYPE_TERM_RETURN ||
			prototype_term_pure_family_lambda(
				terms,
				codomain_family,
				&codomain_lambda
			) != 0 || prototype_term_app(
				terms,
				codomain_lambda,
				terms->terms[normalized_input].as.return_term.value,
				&applied_codomain
			) != 0) {
			return -1;
		}
	}
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			applied_codomain,
			&normalized_codomain
		) != 0 || normalized_codomain >= terms->term_count) {
		return -1;
	}
	if (terms->terms[normalized_codomain].tag == PROTOTYPE_TERM_RETURN) {
		applied_codomain = terms->terms[normalized_codomain].as.return_term.value;
	} else {
		applied_codomain = normalized_codomain;
	}
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, applied_codomain, &continuation_view
		) != 0 || continuation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t effect_row;
	if (computation_effect_row_union(
			terms, &input_view, &continuation_view, &effect_row
		) != 0 || prototype_term_computation_type(
			terms, effect_row, continuation_view.result, p_ret
		) != 0) {
		return -1;
	}
	(void)continuation_pi;
	(void)codomain_binder;
	(void)codomain;
	return 0;
}

int prototype_judgement_delta_record_computation_fold_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_operation_ids,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
) {
	if (!delta || !terms || !type_declarations ||
		subject >= terms->term_count || classifier >= terms->term_count ||
		terms->terms[subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		!premise_operation_ids || !premise_evidence) {
		return -1;
	}
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < premise_count; ++i) {
		if (premise_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			premise_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
			return -1;
		}
		premise_classifiers[i] = premise_evidence[i].classifier;
	}
	const struct prototype_term* fold = &terms->terms[subject];
	uint32_t clause_count = fold->as.computation_fold.clause_count;
	uint32_t expected_premise_count = 2 + 2 * clause_count;
	if (clause_count > PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
		premise_count != expected_premise_count ||
		fold->as.computation_fold.first_clause > terms->computation_fold_clause_count ||
		clause_count > terms->computation_fold_clause_count -
			fold->as.computation_fold.first_clause) {
		return -1;
	}
	for (uint32_t i = 0; i < premise_count; ++i) {
		if (premise_classifiers[i] >= terms->term_count) {
			return -1;
		}
	}
	if (clause_count == 0) {
		uint32_t derived_classifier;
		int status = prototype_judgement_computation_fold_result_classifier(
			terms,
			type_declarations,
			fold->as.computation_fold.computation,
			premise_classifiers[0],
			premise_classifiers[1],
			&derived_classifier
		);
		if (status != 0) {
			return status;
		}
		if (!(prototype_judgement_classifier_conversion(
				terms, type_declarations, classifier, derived_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			/* The Operation classifier may still be a fixed-point approximation.
			 * A mismatch is a deferred equation; final publication separately
			 * requires an exact grounded Claim or residual obligation. */
			return 1;
		}
	} else {
		struct prototype_term_classifier_view result;
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, classifier, &result
			) != 0 || result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return -1;
		}
	}
	uint32_t subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	subjects[0] = fold->as.computation_fold.computation;
	subjects[1] = fold->as.computation_fold.return_clause;
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		subjects[2 + 2 * i] = clause->operation;
		subjects[3 + 2 * i] = clause->body;
	}
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
		NULL,
		premise_operation_ids,
		subjects,
		premise_classifiers,
		premise_evidence,
		premise_count
	);
}

static int select_closed_constraint_premises(
	const struct prototype_judgement_computation_constraint* constraint,
	const uint32_t* subjects,
	const uint32_t* classifiers,
	uint32_t premise_count,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!constraint || !subjects || !classifiers || !selected ||
		constraint->operation_id == PROTOTYPE_INVALID_ID ||
		premise_count > constraint->premise_operation_count) {
		return -1;
	}
	memset(
		selected,
		0,
		premise_count * sizeof(struct prototype_judgement_selected_evidence)
	);
	for (uint32_t i = 0; i < premise_count; ++i) {
		if (constraint->premise_states[i] ==
			PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_LOCAL) {
			selected[i] = (struct prototype_judgement_selected_evidence){
				.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
				.authority_id = subjects[i],
				.context_id = constraint->premise_contexts[i],
				.operation_id = PROTOTYPE_INVALID_ID,
				.subject = subjects[i],
				.classifier = classifiers[i]
			};
			continue;
		}
		if (constraint->premise_states[i] ==
			PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED) {
			return 1;
		}
		if (constraint->premise_states[i] !=
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED ||
			constraint->premise_operations[i] == PROTOTYPE_INVALID_ID ||
			classifiers[i] == PROTOTYPE_INVALID_ID ||
			constraint->premise_evidence[i].kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			constraint->premise_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
			constraint->premise_evidence[i].subject != subjects[i] ||
			constraint->premise_evidence[i].classifier != classifiers[i]) {
			return -1;
		}
		selected[i] = constraint->premise_evidence[i];
	}
	return 0;
}

static int solve_zero_clause_computation_fold_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint* constraint
) {
	uint32_t input_classifier;
	struct prototype_judgement_selected_evidence input_evidence;
	struct prototype_term_classifier_view input_view;
	int input_status;
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		if (constraint->premise_operation_count < 2 ||
			constraint->premise_classifiers[0] == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		input_classifier = constraint->premise_classifiers[0];
		input_status = 0;
	} else {
		if (term_has_tag(
				terms,
				constraint->computation,
				PROTOTYPE_TERM_OPERATION_REQUEST
			)) {
			input_status = lookup_delta_selected_evidence_with_proof_kind(
				delta,
				constraint->computation,
				PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
				&input_evidence
			);
		} else if (term_has_tag(
				terms,
				constraint->computation,
				PROTOTYPE_TERM_COMPUTATION_FOLD
			)) {
			input_status = lookup_delta_selected_evidence_with_proof_kind(
				delta,
				constraint->computation,
				PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
				&input_evidence
			);
		} else {
			input_status = lookup_delta_selected_evidence(
				delta, constraint->computation, 1, &input_evidence
			);
		}
		if (input_status == 0) {
			input_classifier = input_evidence.classifier;
		}
	}
	if (input_status != 0) {
		return 1;
	}
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		/* TermDB identity may be shared by differently typed source occurrences.
		 * A global lookup that finds a non-returning candidate cannot refute this
		 * occurrence-local zero-clause computation fold; defer until the OperationGraph records its selected
		 * operand classifiers. */
		return 1;
	}
	if (constraint->operation_id == PROTOTYPE_INVALID_ID &&
		term_has_tag(terms, constraint->continuation, PROTOTYPE_TERM_LAMBDA) &&
		infer_lambda_classifier_for_app_argument(
			delta,
			terms,
			type_declarations,
			constraint->continuation,
			input_view.result
		) != 0) {
		return 1;
	}
	uint32_t continuation_classifier;
	struct prototype_judgement_selected_evidence continuation_evidence;
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		continuation_classifier = constraint->premise_classifiers[1];
		if (continuation_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
	} else {
		if (lookup_delta_selected_evidence(
				delta, constraint->continuation, 1, &continuation_evidence
			) != 0) {
			return 1;
		}
		continuation_classifier = continuation_evidence.classifier;
	}
	uint32_t classifier;
	int result_status = prototype_judgement_computation_fold_result_classifier(
		terms,
		type_declarations,
		constraint->computation,
		input_classifier,
		continuation_classifier,
		&classifier
	);
	if (result_status != 0) {
		/* This legacy constraint stores only TermDB subjects.  A shared core
		 * term may have several occurrence-local classifiers, so failure for the
		 * candidates selected above cannot refute the source computation fold.  Derive a
		 * proof here only when the candidates work; otherwise let the
		 * OperationGraph validate its explicitly selected child classifiers. */
		return 1;
	}
	uint32_t subjects[2] = { constraint->computation, constraint->continuation };
	uint32_t classifiers[2] = { input_classifier, continuation_classifier };
	uint32_t premise_operation_ids[2];
	const struct prototype_judgement_selected_evidence* premise_evidence = NULL;
	struct prototype_judgement_selected_evidence selected_premises[2];
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		premise_operation_ids[0] = constraint->premise_operations[0];
		premise_operation_ids[1] = constraint->premise_operations[1];
		int premise_status = select_closed_constraint_premises(
				constraint,
				subjects,
				classifiers,
				2,
				selected_premises
			);
		if (premise_status != 0) {
			return premise_status < 0 ? -1 : 1;
		}
		premise_evidence = selected_premises;
	} else {
		selected_premises[0] = input_evidence;
		selected_premises[1] = continuation_evidence;
		premise_operation_ids[0] = input_evidence.operation_id;
		premise_operation_ids[1] = continuation_evidence.operation_id;
		premise_evidence = selected_premises;
	}
	constraint->solved_classifier = classifier;
	return add_delta_relation_with_explicit_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject, classifier,
		PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
		NULL, premise_operation_ids, subjects, classifiers, premise_evidence, 2
	);
}

static int solve_operation_request_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint* constraint
) {
	if (constraint->application == PROTOTYPE_INVALID_ID) {
		for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
			if (terms->terms[i].tag == PROTOTYPE_TERM_APP &&
				terms->terms[i].as.app.function == constraint->computation &&
				terms->terms[i].as.app.argument == constraint->argument) {
				constraint->application = i;
				break;
			}
		}
		if (constraint->application == PROTOTYPE_INVALID_ID &&
			prototype_term_app(
				terms, constraint->computation, constraint->argument, &constraint->application
			) != 0) {
			return -1;
		}
	}
	uint32_t application_classifier;
	uint32_t argument_classifier;
	struct prototype_judgement_selected_evidence application_evidence;
	struct prototype_judgement_selected_evidence argument_evidence;
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		if (constraint->premise_operation_count < 3 ||
			constraint->premise_classifiers[0] == PROTOTYPE_INVALID_ID ||
			constraint->premise_classifiers[1] == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		argument_classifier = constraint->premise_classifiers[1];
		if (prototype_judgement_delta_app_elim_classifier(
				delta,
				terms,
				type_declarations,
				constraint->premise_classifiers[0],
				constraint->argument,
				argument_classifier,
				&application_classifier
			) != 0) {
			return 1;
		}
	} else {
		if (lookup_delta_selected_evidence_with_proof_kind(
				delta,
				constraint->application,
				PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
				&application_evidence
			) != 0) {
			int status = prototype_judgement_delta_expand_app(
				delta, terms, type_declarations, constraint->application, NULL
			);
			return status < 0 ? -1 : 1;
		}
		application_classifier = application_evidence.classifier;
		int argument_status = lookup_delta_app_elim_argument_evidence(
			delta,
			constraint->application,
			application_classifier,
			constraint->argument,
			&argument_evidence
		);
		if (argument_status != 0) {
			return argument_status < 0 ? -1 : 1;
		}
		argument_classifier = argument_evidence.classifier;
	}
	struct prototype_term_classifier_view operation_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, application_classifier, &operation_view
		) != 0) {
		return -1;
	}
	/* A request is only meaningful after the operation application is Comp. */
	if (operation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 1;
	}
	if (!term_has_tag(terms, constraint->continuation, PROTOTYPE_TERM_THUNK)) {
		return -1;
	}
	uint32_t continuation_lambda = terms->terms[constraint->continuation].as.thunk.computation;
	if (!term_has_tag(terms, continuation_lambda, PROTOTYPE_TERM_LAMBDA)) {
		return -1;
	}
	delta->current_context_id = constraint->context_id;
	delta->current_operation_id = constraint->operation_id;
	uint32_t continuation_function_classifier;
	uint32_t continuation_classifier;
	struct prototype_judgement_selected_evidence continuation_evidence;
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		continuation_classifier = constraint->premise_classifiers[2];
		if (continuation_classifier == PROTOTYPE_INVALID_ID ||
			!term_has_tag(terms, continuation_classifier, PROTOTYPE_TERM_THUNK_TYPE)) {
			return 1;
		}
		continuation_function_classifier =
			terms->terms[continuation_classifier].as.thunk_type.computation;
	} else {
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, continuation_lambda, operation_view.result
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence continuation_function_evidence;
		if (lookup_delta_selected_evidence(
				delta, continuation_lambda, 1, &continuation_function_evidence
			) != 0) {
			return 1;
		}
		continuation_function_classifier = continuation_function_evidence.classifier;
		if (prototype_term_thunk_type(
				terms, continuation_function_classifier, &continuation_classifier
			) != 0 || record_cbpv_boundary_with_evidence(
				delta,
				terms,
				type_declarations,
				constraint->continuation,
				continuation_function_evidence.operation_id,
				continuation_function_classifier,
				&continuation_function_evidence
			) != 0) {
			return -1;
		}
		if (lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				constraint->continuation,
				continuation_classifier,
				&continuation_evidence
			) != 0) {
			return 1;
		}
	}
	uint32_t domain;
	uint32_t codomain_family;
	if (pi_parts(terms, continuation_function_classifier, &domain, &codomain_family) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, domain, operation_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	uint32_t continuation_binder = terms->terms[continuation_lambda].as.lambda.binding_id;
	uint32_t continuation_var;
	uint32_t continuation_result;
	if (prototype_term_var(terms, continuation_binder, &continuation_var) != 0 ||
		pi_codomain_at_fresh_binder(
			delta, terms, type_declarations, delta->current_context_id,
			continuation_function_classifier, continuation_var, domain,
			PROTOTYPE_INVALID_ID,
			&continuation_result
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view continuation_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, continuation_result, &continuation_view
		) != 0 || continuation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 1;
	}
	if (prototype_term_contains_free_binding(
			terms, continuation_view.result, continuation_binder
		)) {
		return 1;
	}
	uint32_t operation_effect_row = operation_view.effect_row;
	int operation_id;
	if (prototype_term_effect_operation_identity(
			terms, constraint->computation, &operation_id
		) != 0) {
		return -1;
	}
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	if (!declaration) {
		return -1;
	}
	if (declaration->classifier_schema ==
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT) {
		if (!term_has_tag(terms, argument_classifier, PROTOTYPE_TERM_THUNK_TYPE)) {
			return -1;
		}
		struct prototype_term_classifier_view argument_view;
		if (prototype_judgement_classifier_view(
				terms,
				type_declarations,
				NULL,
				terms->terms[argument_classifier].as.thunk_type.computation,
				&argument_view
			) != 0 || argument_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			argument_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_operation(
				terms, operation_id, argument_view.effect_row, &operation_effect_row
			) != 0) {
			return -1;
		}
	}
	struct prototype_term_classifier_view request_operation_view = operation_view;
	request_operation_view.effect_row = operation_effect_row;
	uint32_t effect_row;
	uint32_t classifier;
	if (computation_effect_row_union(
			terms, &request_operation_view, &continuation_view, &effect_row
		) != 0 || prototype_term_computation_type(
			terms, effect_row, continuation_view.result, &classifier
		) != 0) {
		return -1;
	}
	uint32_t request_rows[2] = {
		operation_effect_row,
		continuation_view.effect_row
	};
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN,
			constraint->subject, effect_row, request_rows, 2
		) != 0) {
		return -1;
	}
	uint32_t subjects[3] = {
		constraint->application,
		constraint->argument,
		constraint->continuation
	};
	uint32_t classifiers[3] = {
		application_classifier,
		argument_classifier,
		continuation_classifier
	};
	delta->current_context_id = constraint->context_id;
	delta->current_operation_id = constraint->operation_id;
	constraint->solved_classifier = classifier;
	uint32_t premise_operation_ids[3];
	const struct prototype_judgement_selected_evidence* premise_evidence = NULL;
	struct prototype_judgement_selected_evidence selected_premises[3];
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		premise_operation_ids[0] = PROTOTYPE_INVALID_ID;
		premise_operation_ids[1] = constraint->premise_operations[1];
		premise_operation_ids[2] = constraint->premise_operations[2];
		memset(selected_premises, 0, sizeof(selected_premises));
		selected_premises[0] = (struct prototype_judgement_selected_evidence){
			.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			.authority_id = constraint->application,
			.context_id = constraint->context_id,
			.operation_id = PROTOTYPE_INVALID_ID,
			.subject = constraint->application,
			.classifier = application_classifier
		};
		for (uint32_t i = 1; i < 3; ++i) {
			if (constraint->premise_states[i] ==
				PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED) {
				return 1;
			}
			if (constraint->premise_states[i] !=
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_CLOSED ||
				constraint->premise_evidence[i].kind !=
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				constraint->premise_evidence[i].authority_kind ==
					PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
				constraint->premise_evidence[i].subject != subjects[i] ||
				constraint->premise_evidence[i].classifier != classifiers[i]) {
				return -1;
			}
			selected_premises[i] = constraint->premise_evidence[i];
		}
		premise_evidence = selected_premises;
	} else {
		selected_premises[0] = application_evidence;
		selected_premises[1] = argument_evidence;
		selected_premises[2] = continuation_evidence;
		for (uint32_t i = 0; i < 3; ++i) {
			premise_operation_ids[i] = selected_premises[i].operation_id;
		}
		premise_evidence = selected_premises;
	}
	return add_delta_relation_with_explicit_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject, classifier,
		PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
		NULL, premise_operation_ids, subjects, classifiers, premise_evidence, 3
	);
}

static int record_computation_lambda_body_effect_weaken(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t body_operation_id,
	uint32_t body_context_id,
	uint32_t body_subject,
	uint32_t lambda,
	uint32_t source_classifier,
	uint32_t target_effect_row,
	uint32_t* p_target_classifier
) {
	if (!delta || !terms || !type_declarations || !p_target_classifier ||
		lambda >= terms->term_count ||
		terms->terms[lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		source_classifier >= terms->term_count || target_effect_row >= terms->term_count ||
		!delta->contexts || body_context_id >= delta->contexts->context_count ||
		body_subject >= terms->term_count) {
		return -1;
	}
	uint32_t source_pi;
	uint32_t domain;
	uint32_t source_family;
	uint32_t binder = terms->terms[lambda].as.lambda.binding_id;
	uint32_t binder_var;
	uint32_t source_body_classifier;
	uint32_t source_family_binder;
	uint32_t source_family_body;
	if (classifier_kernel_as_pi(
			terms, type_declarations, NULL, source_classifier,
			&source_pi, &domain, &source_family
		) != 0 || prototype_term_var(terms, binder, &binder_var) != 0 ||
		prototype_term_pure_family_parts(
			terms,
			source_family,
			&source_family_binder,
			&source_family_body
		) != 0) {
		return -1;
	}
	if (!prototype_term_contains_free_binding(
			terms, source_family_body, source_family_binder
		)) {
		source_body_classifier = source_family_body;
	} else if (pi_codomain_at_binder_in_context(
			delta,
			terms,
			type_declarations,
			delta->current_context_id,
			source_pi,
			binder_var,
			domain,
			PROTOTYPE_INVALID_ID,
			&source_body_classifier
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view source_body;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, source_body_classifier, &source_body
		) != 0 || source_body.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source_body.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t target_body_classifier;
	if (prototype_term_computation_type(
			terms, target_effect_row, source_body.result, &target_body_classifier
		) != 0) {
		return -1;
	}
	uint32_t saved_context = delta->current_context_id;
	uint32_t saved_operation = delta->current_operation_id;
	prototype_judgement_delta_set_context(delta, body_context_id);
	delta->current_operation_id = body_operation_id;
	struct prototype_judgement_selected_evidence source_evidence;
	int source_status = prototype_judgement_delta_select_evidence(
		delta,
		body_operation_id,
		body_context_id,
		body_subject,
		source_body_classifier,
		&source_evidence
	);
	int weaken_status = source_status == 0 ?
		prototype_judgement_delta_record_effect_weaken(
			delta,
			terms,
			type_declarations,
			&source_evidence,
			target_body_classifier
		) : source_status;
	prototype_judgement_delta_set_context(delta, saved_context);
	delta->current_operation_id = saved_operation;
	if (weaken_status != 0) {
		return -1;
	}
	(void)source_family;
	/* Weakening belongs to the computation returned by the lambda. The lambda's
	 * source Pi remains the return-clause derivation used by the fold rule. */
	*p_target_classifier = source_classifier;
	return 0;
}

static int fold_clause_closed_body_row(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t operation_domain,
	uint32_t carrier_result,
	uint32_t* p_effect_row
) {
	uint32_t outer_domain;
	uint32_t outer_family;
	uint32_t outer_binder;
	uint32_t inner_classifier;
	uint32_t inner_domain;
	uint32_t inner_family;
	uint32_t inner_binder;
	uint32_t body_classifier;
	uint32_t instantiated_domain;
	uint32_t instantiated_body;
	struct prototype_term_classifier_view body_view;
	if (!terms || !type_declarations || !p_effect_row ||
		pi_parts(terms, classifier, &outer_domain, &outer_family) != 0 ||
		prototype_term_pure_family_parts(
			terms, outer_family, &outer_binder, &inner_classifier
		) != 0 || pi_parts(
			terms, inner_classifier, &inner_domain, &inner_family
		) != 0 || prototype_term_pure_family_parts(
			terms, inner_family, &inner_binder, &body_classifier
		) != 0 || instantiate_fold_clause_classifier(
			terms,
			type_declarations,
			outer_domain,
			operation_domain,
			outer_domain,
			&instantiated_domain
		) != 0 || instantiate_fold_clause_classifier(
			terms,
			type_declarations,
			outer_domain,
			operation_domain,
			body_classifier,
			&instantiated_body
		) != 0) {
		return -1;
	}
	int domain_equal;
	int result_equal;
	if (classifier_conversion_decision(
			prototype_judgement_classifier_conversion(
				terms, type_declarations, instantiated_domain, operation_domain
			),
			&domain_equal
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, instantiated_body, &body_view
		) != 0 || body_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		body_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		classifier_conversion_decision(
			prototype_judgement_classifier_conversion(
				terms, type_declarations, body_view.result, carrier_result
			),
			&result_equal
		) != 0) {
		return -1;
	}
	(void)outer_binder;
	(void)inner_domain;
	(void)inner_binder;
	if (!domain_equal || !result_equal ||
		(body_view.effect_row < terms->term_count &&
		 terms->terms[body_view.effect_row].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR)) {
		return 1;
	}
	*p_effect_row = body_view.effect_row;
	return 0;
}

static int solve_clause_computation_fold_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint* constraint
) {
	if (constraint->subject >= terms->term_count ||
		terms->terms[constraint->subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
		return -1;
	}
	const struct prototype_term* fold = &terms->terms[constraint->subject];
	uint32_t clause_count = fold->as.computation_fold.clause_count;
	if (clause_count == 0 ||
		clause_count > PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
		fold->as.computation_fold.first_clause > terms->computation_fold_clause_count ||
		clause_count > terms->computation_fold_clause_count -
			fold->as.computation_fold.first_clause) {
		return -1;
	}
	uint32_t input_classifier = PROTOTYPE_INVALID_ID;
	struct prototype_judgement_selected_evidence input_evidence;
	memset(&input_evidence, 0, sizeof(input_evidence));
	int input_status = -1;
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		if (constraint->premise_operation_count != 2 + 2 * clause_count ||
			constraint->premise_classifiers[0] == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		input_classifier = constraint->premise_classifiers[0];
		input_status = 0;
	} else if (term_has_tag(
			terms,
			fold->as.computation_fold.computation,
			PROTOTYPE_TERM_OPERATION_REQUEST
		)) {
		input_status = lookup_delta_selected_evidence_with_proof_kind(
			delta,
			fold->as.computation_fold.computation,
			PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO,
			&input_evidence
		);
	} else if (term_has_tag(
			terms,
			fold->as.computation_fold.computation,
			PROTOTYPE_TERM_COMPUTATION_FOLD
		)) {
		input_status = lookup_delta_selected_evidence_with_proof_kind(
			delta,
			fold->as.computation_fold.computation,
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
			&input_evidence
		);
	} else {
		input_status = lookup_delta_selected_evidence(
			delta,
			fold->as.computation_fold.computation,
			1,
			&input_evidence
		);
	}
	if (input_status != 0) {
		return 1;
	}
	if (constraint->operation_id == PROTOTYPE_INVALID_ID) {
		input_classifier = input_evidence.classifier;
	}
	struct prototype_term_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}

	uint32_t operation_classifiers[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t operation_declaration_classifiers[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t operation_domains[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	struct prototype_term_classifier_view operation_views[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	struct prototype_judgement_selected_evidence operation_evidence[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t handled_operation_rows[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	uint32_t handled_operations_row;
	if (prototype_term_effect_label(
			terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &handled_operations_row
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		uint32_t operation_pi;
		uint32_t operation_family;
		uint32_t operation_binder;
		uint32_t operation_var;
		uint32_t operation_result;
		uint32_t ignored_operation_body;
		if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
			operation_classifiers[i] = constraint->premise_classifiers[2 + 2 * i];
			if (operation_classifiers[i] == PROTOTYPE_INVALID_ID) {
				return 1;
			}
		} else {
			if (lookup_delta_selected_evidence(
					delta, clause->operation, 1, &operation_evidence[i]
				) != 0) {
				return 1;
			}
			operation_classifiers[i] = operation_evidence[i].classifier;
		}
		operation_declaration_classifiers[i] = operation_classifiers[i];
		int operation_identity;
		if (prototype_term_effect_operation_identity(
				terms, clause->operation, &operation_identity
			) != 0) {
			return -1;
		}
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(operation_identity);
		if (!declaration) {
			return -1;
		}
		int specialization_status = prototype_judgement_specialize_fold_operation_classifier(
			terms,
			type_declarations,
			input_view.effect_row,
			operation_identity,
			operation_classifiers[i],
			&operation_classifiers[i]
		);
		if (specialization_status != 0) {
			return specialization_status < 0 ? -1 : 1;
		}
		if (classifier_kernel_as_pi(
				terms, type_declarations, NULL, operation_classifiers[i],
				&operation_pi, &operation_domains[i], &operation_family
			) != 0 || prototype_term_pure_family_parts(
				terms, operation_family, &operation_binder, &ignored_operation_body
			) != 0 || prototype_term_var(terms, operation_binder, &operation_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				operation_pi, operation_var, operation_domains[i],
				PROTOTYPE_INVALID_ID, &operation_result
			) != 0 || prototype_judgement_classifier_view(
				terms, type_declarations, NULL, operation_result, &operation_views[i]
			) != 0 || operation_views[i].category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			operation_views[i].computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		uint32_t union_row;
		if (prototype_term_effect_row_union(
				terms, handled_operations_row, operation_views[i].effect_row, &union_row
			) != 0) {
			return -1;
		}
		handled_operation_rows[i] = operation_views[i].effect_row;
		handled_operations_row = union_row;
	}
	if (add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN,
			constraint->subject,
			handled_operations_row,
			handled_operation_rows,
			clause_count
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view handled_operations_view = {
		.category = PROTOTYPE_TERM_CATEGORY_COMPUTATION,
		.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING,
		.effect_row = handled_operations_row,
		.result = input_view.result
	};
	uint32_t residual_effect_row;
	int residual_status = computation_fold_residual_row(
		terms, &input_view, &handled_operations_view, &residual_effect_row
	);
	if (residual_status < 0) {
		return -1;
	}
	if (residual_status > 0) {
		constraint->effect_residual_pending = 1;
		constraint->effect_residual_row = PROTOTYPE_INVALID_ID;
		return 1;
	}
	constraint->effect_residual_pending = 0;
	constraint->effect_residual_row = residual_effect_row;
	uint32_t residual_rows[2] = {
		input_view.effect_row,
		handled_operations_row
	};
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_RESIDUAL,
			constraint->subject, residual_effect_row, residual_rows, 2
		) != 0) {
		return -1;
	}

	if (infer_lambda_classifier_for_app_argument(
			delta, terms, type_declarations, fold->as.computation_fold.return_clause, input_view.result
		) != 0) {
		return -1;
	}
	uint32_t return_classifier;
	struct prototype_judgement_selected_evidence return_evidence;
	int return_selection;
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		return_classifier = constraint->premise_classifiers[1];
		return_selection = return_classifier == PROTOTYPE_INVALID_ID ? 1 : 0;
	} else {
		return_selection = select_delta_pi_classifier_for_domain(
			delta,
			terms,
			type_declarations,
			fold->as.computation_fold.return_clause,
			input_view.result,
			&return_evidence
		);
		if (return_selection == 0) {
			return_classifier = return_evidence.classifier;
		}
	}
	if (return_selection < 0) {
		return -1;
	}
	if (return_selection > 0) {
		return 1;
	}
	uint32_t return_pi;
	uint32_t return_domain;
	uint32_t return_family;
	if (classifier_kernel_as_pi(
			terms, type_declarations, NULL, return_classifier,
			&return_pi, &return_domain, &return_family
		) != 0) {
		return -1;
	}
	if (fold->as.computation_fold.return_clause >= terms->term_count ||
		terms->terms[fold->as.computation_fold.return_clause].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	uint32_t return_binder =
		terms->terms[fold->as.computation_fold.return_clause].as.lambda.binding_id;
	uint32_t return_var;
	uint32_t output_classifier;
	if (prototype_term_var(terms, return_binder, &return_var) != 0 ||
		pi_codomain_at_fresh_binder(
			delta, terms, type_declarations, delta->current_context_id,
			return_pi, return_var, return_domain,
			PROTOTYPE_INVALID_ID, &output_classifier
		) != 0) {
		return -1;
	}
	int dependent_output = prototype_term_contains_free_binding(
		terms, output_classifier, return_binder
	);
	struct prototype_term_classifier_view output_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, output_classifier, &output_view
		) != 0 || output_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		output_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t carrier_rows[33];
	uint32_t carrier_row_count = 0;
	carrier_rows[carrier_row_count++] = output_view.effect_row;
	carrier_rows[carrier_row_count++] = residual_effect_row;
	uint32_t handled_effect_row;
	uint32_t handled_output_classifier;
	if (prototype_term_effect_row_union(
			terms, output_view.effect_row, residual_effect_row, &handled_effect_row
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
			uint32_t clause_row;
			uint32_t premise = 3 + 2 * i;
			if (premise >= constraint->premise_operation_count ||
				constraint->premise_classifiers[premise] == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			int row_status = fold_clause_closed_body_row(
				terms,
				type_declarations,
				constraint->premise_classifiers[premise],
				operation_domains[i],
				output_view.result,
				&clause_row
			);
			if (row_status < 0) {
				return -1;
			}
			if (row_status == 0) {
				uint32_t joined_row;
				if (prototype_term_effect_row_union(
						terms, handled_effect_row, clause_row, &joined_row
					) != 0) {
					return -1;
				}
				handled_effect_row = joined_row;
				carrier_rows[carrier_row_count++] = clause_row;
			}
			continue;
		}
		for (size_t relation_id = 0;
			relation_id < delta->proposition_count;
			++relation_id) {
			const struct prototype_judgement_proposition* relation =
				&delta->propositions[relation_id];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != clause->body) {
				continue;
			}
			uint32_t clause_row;
			int row_status = fold_clause_closed_body_row(
				terms,
				type_declarations,
				relation->classifier,
				operation_domains[i],
				output_view.result,
				&clause_row
			);
			if (row_status != 0) {
				continue;
			}
			uint32_t joined_row;
			if (prototype_term_effect_row_union(
					terms,
					handled_effect_row,
					clause_row,
					&joined_row
				) != 0) {
				return -1;
			}
			handled_effect_row = joined_row;
			carrier_rows[carrier_row_count++] = clause_row;
			break;
		}
	}
	constraint->effect_output_row = handled_effect_row;
	if (prototype_term_computation_type(
			terms, handled_effect_row, output_view.result, &handled_output_classifier
		) != 0) {
		return -1;
	}
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN,
			constraint->subject,
			handled_effect_row,
			carrier_rows,
			carrier_row_count
		) != 0) {
		return -1;
	}
	uint32_t return_inclusion[2] = {
		output_view.effect_row,
		handled_effect_row
	};
	uint32_t residual_inclusion[2] = {
		residual_effect_row,
		handled_effect_row
	};
	if (add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION,
			constraint->subject,
			handled_effect_row,
			return_inclusion,
			2
		) != 0 || add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION,
			constraint->subject,
			handled_effect_row,
			residual_inclusion,
			2
		) != 0) {
		return -1;
	}
	if (!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			output_view.effect_row,
			handled_effect_row
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		unsigned source_effects;
		unsigned target_effects;
		if (prototype_term_effect_row_closed_bits(
				terms, output_view.effect_row, &source_effects
			) != 0 || prototype_term_effect_row_closed_bits(
				terms, handled_effect_row, &target_effects
			) != 0) {
			/* The partial-policy solver retains this inclusion obligation. It must
			 * not fabricate a closed EFFECT_WEAKEN proof for a symbolic row. */
			return 1;
		}
		(void)source_effects;
		(void)target_effects;
		uint32_t widened_return_classifier;
		if (record_computation_lambda_body_effect_weaken(
				delta,
				terms,
				type_declarations,
				constraint->return_body_operation_id,
				constraint->return_body_context_id,
				constraint->return_body_subject,
				fold->as.computation_fold.return_clause,
				return_classifier,
				handled_effect_row,
				&widened_return_classifier
			) != 0) {
			return -1;
		}
		return_classifier = widened_return_classifier;
	}

	uint32_t outer_classifiers[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	struct prototype_judgement_selected_evidence outer_evidence[
		PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
	];
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		uint32_t operation_domain = operation_domains[i];
		struct prototype_term_classifier_view operation_view = operation_views[i];
		if (clause->body >= terms->term_count ||
			terms->terms[clause->body].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, clause->body, operation_domain
			) != 0) {
			return -1;
		}
		uint32_t continuation_lambda =
			terms->terms[clause->body].as.lambda.body;
		if (continuation_lambda >= terms->term_count ||
			terms->terms[continuation_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t continuation_function_classifier;
		uint32_t continuation_expected;
		if (prototype_term_pi(
				terms,
				operation_view.result,
				handled_output_classifier,
				&continuation_function_classifier
			) != 0 || prototype_term_thunk_type(
				terms, continuation_function_classifier, &continuation_expected
			) != 0) {
			return -1;
		}
		uint32_t saved_context = delta->current_context_id;
		uint32_t operation_argument_context;
		uint32_t continuation_context;
		if (ensure_lambda_binder_assumption(
				delta,
				terms,
				clause->body,
				operation_domain
			) != 0 || prototype_context_extend(
				delta->contexts,
				saved_context,
				terms->terms[clause->body].as.lambda.binding_id,
				operation_domain,
				PROTOTYPE_INVALID_ID,
				&operation_argument_context
			) != 0) {
			return -1;
		}
		prototype_judgement_delta_set_context(delta, operation_argument_context);
		if (ensure_lambda_binder_assumption(
				delta,
				terms,
				continuation_lambda,
				continuation_expected
			) != 0 || prototype_context_extend(
				delta->contexts,
				operation_argument_context,
				terms->terms[continuation_lambda].as.lambda.binding_id,
				continuation_expected,
				PROTOTYPE_INVALID_ID,
				&continuation_context
			) != 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, continuation_context);
		uint32_t continuation_body = terms->terms[continuation_lambda].as.lambda.body;
		if (continuation_body < terms->term_count &&
			terms->terms[continuation_body].tag == PROTOTYPE_TERM_APP &&
			prototype_judgement_delta_expand_app(
				delta, terms, type_declarations, continuation_body, NULL
			) < 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, operation_argument_context);
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, continuation_lambda, continuation_expected
			) != 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, saved_context);
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, clause->body, operation_domain
			) != 0) {
			return -1;
		}
		uint32_t expected_continuation_classifier;
		uint32_t expected_outer_classifier;
		if (prototype_term_pi(
				terms,
				continuation_expected,
				handled_output_classifier,
				&expected_continuation_classifier
			) != 0 || prototype_term_pi(
				terms,
				operation_domain,
				expected_continuation_classifier,
				&expected_outer_classifier
			) != 0) {
			return -1;
		}
		uint32_t outer_classifier;
		if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
			uint32_t outer_premise = 3 + 2 * i;
			outer_classifier = constraint->premise_classifiers[outer_premise];
			if (outer_classifier == PROTOTYPE_INVALID_ID ||
				constraint->premise_states[outer_premise] ==
					PROTOTYPE_JUDGEMENT_CONSTRAINT_OPERAND_UNRESOLVED) {
				return 1;
			}
		} else {
			int outer_selection = lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				clause->body,
				expected_outer_classifier,
				&outer_evidence[i]
			);
			if (outer_selection < 0) {
				return -1;
			}
			if (outer_selection > 0) {
				return 1;
			}
			outer_classifier = outer_evidence[i].classifier;
		}
		uint32_t outer_pi;
		uint32_t outer_domain;
		uint32_t outer_family;
		if (classifier_kernel_as_pi(
				terms, type_declarations, NULL, outer_classifier,
				&outer_pi, &outer_domain, &outer_family
			) != 0) {
			return -1;
		}
		uint32_t outer_binder;
		uint32_t outer_var;
		uint32_t continuation_classifier;
		uint32_t ignored_outer_body;
		if (prototype_term_pure_family_parts(
				terms, outer_family, &outer_binder, &ignored_outer_body
			) != 0 ||
			prototype_term_var(terms, outer_binder, &outer_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				outer_pi, outer_var, outer_domain,
				PROTOTYPE_INVALID_ID, &continuation_classifier
			) != 0 || continuation_classifier >= terms->term_count ||
			terms->terms[clause->body].as.lambda.body >= terms->term_count) {
			return -1;
		}
		uint32_t continuation_pi;
		uint32_t continuation_domain;
		uint32_t continuation_family;
		struct prototype_judgement_selected_evidence continuation_evidence;
		if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
			continuation_classifier = expected_continuation_classifier;
		} else {
			int continuation_selection =
				lookup_delta_selected_evidence_normalization_equal(
					delta,
					terms,
					type_declarations,
					continuation_lambda,
					expected_continuation_classifier,
					&continuation_evidence
				);
			if (continuation_selection != 0) {
				return -1;
			}
			continuation_classifier = continuation_evidence.classifier;
		}
		if (classifier_kernel_as_pi(
				terms, type_declarations, NULL, continuation_classifier,
				&continuation_pi, &continuation_domain, &continuation_family
			) != 0) {
			return -1;
		}
		uint32_t continuation_binder;
		uint32_t continuation_var;
		uint32_t continuation_output;
		uint32_t ignored_continuation_body;
		if (prototype_term_pure_family_parts(
				terms,
				continuation_family,
				&continuation_binder,
				&ignored_continuation_body
			) != 0 ||
			prototype_term_var(terms, continuation_binder, &continuation_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				continuation_pi, continuation_var, continuation_domain,
				PROTOTYPE_INVALID_ID, &continuation_output
			) != 0 || !(prototype_judgement_classifier_conversion(
				terms, type_declarations, continuation_output, handled_output_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		outer_classifiers[i] = outer_classifier;
	}

	if (dependent_output) {
		/* The return value needed to instantiate the computation-fold result family is
		 * produced by the handled computation.  The clause assumptions above
		 * are sufficient to classify the occurrence-local operation body, but
		 * no closed computation-fold classifier exists until runtime supplies that value.
		 * The source compiler records a computation-fold result obligation instead of
		 * publishing a closed fold-elimination proof. */
		return 1;
	}
	uint32_t subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t classifiers[64];
	subjects[0] = fold->as.computation_fold.computation;
	subjects[1] = fold->as.computation_fold.return_clause;
	classifiers[0] = input_classifier;
	classifiers[1] = return_classifier;
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		subjects[2 + 2 * i] = clause->operation;
		subjects[3 + 2 * i] = clause->body;
		classifiers[2 + 2 * i] = operation_declaration_classifiers[i];
		classifiers[3 + 2 * i] = outer_classifiers[i];
	}
	constraint->solved_classifier = handled_output_classifier;
	uint32_t premise_operation_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	const struct prototype_judgement_selected_evidence* premise_evidence = NULL;
	struct prototype_judgement_selected_evidence selected_premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	uint32_t premise_count = 2 + 2 * clause_count;
	if (constraint->operation_id != PROTOTYPE_INVALID_ID) {
		for (uint32_t i = 0; i < premise_count; ++i) {
			premise_operation_ids[i] = constraint->premise_operations[i];
		}
		int premise_status = select_closed_constraint_premises(
				constraint,
				subjects,
				classifiers,
				premise_count,
				selected_premises
			);
		if (premise_status != 0) {
			return premise_status < 0 ? -1 : 1;
		}
		premise_evidence = selected_premises;
	} else {
		selected_premises[0] = input_evidence;
		selected_premises[1] = return_evidence;
		for (uint32_t i = 0; i < clause_count; ++i) {
			selected_premises[2 + 2 * i] = operation_evidence[i];
			selected_premises[3 + 2 * i] = outer_evidence[i];
		}
		for (uint32_t i = 0; i < premise_count; ++i) {
			premise_operation_ids[i] = selected_premises[i].operation_id;
		}
		premise_evidence = selected_premises;
	}
	/* Helper inference above is authority-neutral and may temporarily clear the
	 * active Operation. The solved constraint, not the last helper call, owns the
	 * fold conclusion. */
	delta->current_context_id = constraint->context_id;
	delta->current_operation_id = constraint->operation_id;
	return add_delta_relation_with_explicit_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject,
		handled_output_classifier, PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
		NULL, premise_operation_ids, subjects, classifiers, premise_evidence,
		premise_count
	);
}

static int solve_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	/* Request classifiers are premises of enclosing folds. Derive every request
	 * which is ready before allowing a fold to consume child classifiers. */
	for (int pass = 0; pass < 2; ++pass) {
		for (size_t i = 0; i < delta->computation_constraint_count; ++i) {
			struct prototype_judgement_computation_constraint* constraint =
				&delta->computation_constraints[i];
			int is_request = constraint->kind ==
				PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST;
			if ((pass == 0) != is_request) {
				continue;
			}
			int budget_status = consume_solver_step(delta);
			if (budget_status != 0) {
				return budget_status;
			}
			delta->current_context_id = constraint->context_id;
			delta->current_operation_id = constraint->operation_id;
			if (!is_request) {
				if (constraint->kind != PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD) {
					return -1;
				}
			if (constraint->subject >= terms->term_count ||
				terms->terms[constraint->subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
				return -1;
			}
			const struct prototype_term* fold = &terms->terms[constraint->subject];
			int status = fold->as.computation_fold.clause_count == 0 ?
				solve_zero_clause_computation_fold_constraint(
					delta, terms, type_declarations, constraint
				) : solve_clause_computation_fold_constraint(
					delta, terms, type_declarations, constraint
				);
			if (status < 0) {
				fprintf(
					stderr,
					"fold constraint failed operation=%u subject=%u\n",
					constraint->operation_id,
					constraint->subject
				);
				return -1;
			}
			} else {
			int status = solve_operation_request_constraint(
				delta, terms, type_declarations, constraint
			);
			if (status < 0) {
				fprintf(
					stderr,
					"operation request constraint failed operation=%u subject=%u\n",
					constraint->operation_id,
					constraint->subject
				);
				return -1;
			}
			}
		}
	}
	return solve_effect_row_constraints(delta, terms);
}

int prototype_judgement_delta_infer_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations ||
		prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}

	for (;;) {
		size_t before = delta->proposition_count;
		if (prototype_judgement_delta_infer_cbpv_boundaries(
				delta, terms, type_declarations
			) != 0) {
			return -1;
		}
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->proposition_count == before) {
			return 0;
		}
	}
}

int prototype_judgement_delta_solve_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations ||
		prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}
	for (;;) {
		size_t before = delta->proposition_count;
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->proposition_count == before) {
			return 0;
		}
	}
}

int prototype_judgement_delta_solve_recorded_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	/* Constraint solving may synthesize Core-level helper derivations. The
	 * owning request/fold Operation is reified separately from OperationGraph. */
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	for (;;) {
		prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
		size_t before = delta->proposition_count;
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->proposition_count == before) {
			return 0;
		}
	}
}

int prototype_judgement_delta_infer_core_helper_facts(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	/* This is deliberately an erased-Core closure pass. Its output may support
	 * motive/type synthesis, but cannot acquire the Operation authority that was
	 * active at the call site. Operation-owned evidence is rebuilt separately
	 * from exact OperationGraph edges. */
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	size_t first_relation = delta->proposition_count;
	if (prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}

	int changed = 1;
	while (changed) {
		changed = 0;
			uint32_t term_count = (uint32_t)terms->term_count;
			for (uint32_t i = 0; i < term_count; ++i) {
				uint32_t existing;
				if (terms->terms[i].tag != PROTOTYPE_TERM_LAMBDA &&
					terms->terms[i].tag != PROTOTYPE_TERM_APP &&
					lookup_delta_proven_classifier(delta, terms, i, &existing) == 0) {
					continue;
				}
				if (terms->terms[i].tag == PROTOTYPE_TERM_APP) {
					size_t before = delta->proposition_count;
					int status = prototype_judgement_delta_expand_app(
						delta,
						terms,
					type_declarations,
					i,
						NULL
					);
					if (status < 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
			} else if (terms->terms[i].tag == PROTOTYPE_TERM_LAMBDA) {
				if (infer_lambda_classifiers_from_body(
						delta,
						terms,
							type_declarations,
							i,
							&changed
						) != 0) {
						return -1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_RETURN ||
					terms->terms[i].tag == PROTOTYPE_TERM_THUNK ||
					terms->terms[i].tag == PROTOTYPE_TERM_FORCE) {
					size_t before = delta->proposition_count;
					int status = infer_cbpv_boundary_classifier(delta, terms, type_declarations, i);
					if (status < 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_TEXT_LITERAL) {
					size_t before = delta->proposition_count;
					if (infer_text_literal_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_INT_LITERAL) {
					size_t before = delta->proposition_count;
					if (infer_int_literal_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
					size_t before = delta->proposition_count;
					if (infer_pure_primitive_classifier(
							delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
					size_t before = delta->proposition_count;
					if (infer_effect_operation_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_CONSTRUCTOR) {
					size_t before = delta->proposition_count;
				if (infer_constructor_classifier(
						delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
					if (delta->proposition_count > before) {
						changed = 1;
					}
				} else if (type_instance_has_known_type(terms, type_declarations, i)) {
					size_t before = delta->proposition_count;
				if (infer_type_formation_classifier(
						delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
				if (delta->proposition_count > before) {
					changed = 1;
				}
			}
		}
		size_t before = delta->proposition_count;
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->proposition_count > before) {
			changed = 1;
		}
	}
	for (size_t i = first_relation; i < delta->proposition_count; ++i) {
		if (delta->propositions[i].operation_id != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		uint32_t cursor = 0;
		uint32_t derivation_id;
		int found = 0;
		while (prototype_judgement_candidate_derivation_next(
			delta->propositions,
			delta->proposition_count,
			delta->derivation_candidates,
			delta->derivation_candidate_count,
			(uint32_t)i,
			&cursor,
			&derivation_id
		) == 0) {
			found = 1;
			if (delta->derivation_candidates[derivation_id].conclusion_operation_id !=
				PROTOTYPE_INVALID_ID) {
				return -1;
			}
		}
		if (!found) {
			return -1;
		}
	}
	return 0;
}

int prototype_judgement_expand_match_motive(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_is_universe_var(terms, classifier) ||
		terms->terms[subject].as.match.case_count != 0) {
		return -1;
	}
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		NULL,
		NULL,
		NULL,
		0
	);
}

static int prototype_judgement_delta_add_conversion(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected
) {
	if (!delta || !type_declarations || !source_evidence ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		!term_exists(terms, source_evidence->subject) ||
		!term_exists(terms, expected) ||
		!term_exists(terms, source_evidence->classifier)) {
		return -1;
	}
	if (expected == source_evidence->classifier) {
		return 0;
	}
	if (prototype_judgement_classifier_conversion(
			(struct prototype_term_db*)terms,
			type_declarations,
			source_evidence->classifier,
			expected
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	uint32_t premise_subjects[1] = { source_evidence->subject };
	uint32_t premise_classifiers[1] = { source_evidence->classifier };
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		source_evidence->subject,
		expected,
		PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		source_evidence,
		1
	);
}

static int prototype_judgement_delta_add_universe_cumulativity(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_is_universe_var(terms, subject) ||
		!term_is_universe_var(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_UNIVERSE,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY
	);
}

int prototype_judgement_delta_ensure_type_at_universe(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t universe
) {
	if (!delta || !terms || !type_declarations ||
		!term_exists(terms, subject) ||
		!term_is_universe_var(terms, universe)) {
		return -1;
	}
	struct prototype_judgement_selected_evidence existing_evidence;
	if (lookup_delta_prior_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			subject,
			universe,
			&existing_evidence
		) == 0) {
		return 0;
	}
	if (term_is_universe_var(terms, subject)) {
		return prototype_judgement_delta_add_universe_cumulativity(
			delta,
			terms,
			subject,
			universe
		);
	}
	if (type_instance_has_known_type(terms, type_declarations, subject)) {
		return prototype_judgement_delta_record_type_formation(
			delta, terms, type_declarations, subject, universe
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_COMPUTATION_TYPE)) {
		const struct prototype_term* computation_type = &terms->terms[subject];
		struct prototype_effect_row_normal_form effect_row;
		if (prototype_term_effect_row_normal_form(
				terms, computation_type->as.computation_type.label, &effect_row
			) != 0 || prototype_judgement_delta_ensure_type_at_universe(
				delta,
				terms,
				type_declarations,
				computation_type->as.computation_type.result,
				universe
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence result_evidence;
		if (lookup_delta_prior_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				computation_type->as.computation_type.result,
				universe,
				&result_evidence
			) != 0) {
			return -1;
		}
		uint32_t premise_subjects[1] = {
			computation_type->as.computation_type.result
		};
		uint32_t premise_classifiers[1] = { universe };
		return add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			subject,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			universe,
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION,
			NULL,
			NULL,
			premise_subjects,
			premise_classifiers,
			&result_evidence,
			1
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_THUNK_TYPE)) {
		uint32_t computation = terms->terms[subject].as.thunk_type.computation;
		if (prototype_judgement_delta_ensure_type_at_universe(
				delta, terms, type_declarations, computation, universe
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence computation_evidence;
		if (lookup_delta_prior_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				computation,
				universe,
				&computation_evidence
			) != 0) {
			return -1;
		}
		uint32_t premise_subjects[1] = { computation };
		uint32_t premise_classifiers[1] = { universe };
		return add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			subject,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			universe,
			PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION,
			NULL,
			NULL,
			premise_subjects,
			premise_classifiers,
			&computation_evidence,
			1
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_PI)) {
		uint32_t domain;
		uint32_t codomain_family;
		uint32_t codomain_lambda;
		if (pi_parts(terms, subject, &domain, &codomain_family) != 0) {
			return -1;
		}
		if (prototype_term_pure_family_lambda(
				terms,
				codomain_family,
				&codomain_lambda
			) != 0) {
			return -1;
		}
		uint32_t codomain_binder;
		uint32_t codomain;
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[subject].as.pi.codomain_family,
				&codomain_binder,
				&codomain
			) != 0) {
			return -1;
		}
		uint32_t outer_context = delta->current_context_id;
		uint32_t outer_operation = delta->current_operation_id;
		if (ensure_lambda_binder_assumption(
				delta, terms, codomain_lambda, domain
			) != 0 || prototype_judgement_delta_ensure_type_at_universe(
				delta,
				terms,
				type_declarations,
				domain,
				universe
			) != 0) {
			return -1;
		}
		uint32_t binder_context;
		if (!delta->contexts || prototype_context_extend(
				delta->contexts,
				outer_context,
				codomain_binder,
				domain,
				PROTOTYPE_INVALID_ID,
				&binder_context
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence outer_codomain_evidence;
		int has_outer_codomain_evidence =
			lookup_delta_prior_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				codomain,
				universe,
				&outer_codomain_evidence
			) == 0;
		uint32_t binder_projection = PROTOTYPE_INVALID_ID;
		if (has_outer_codomain_evidence &&
			(!delta->substitutions || prototype_substitution_projection_path(
				delta->substitutions,
				delta->contexts,
				binder_context,
				outer_context,
				&binder_projection
			) != 0)) {
			return -1;
		}
		prototype_judgement_delta_set_context(delta, binder_context);
		prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
		if ((has_outer_codomain_evidence &&
			 prototype_judgement_delta_record_context_weaken(
				delta, &outer_codomain_evidence, binder_projection
			 ) != 0) || prototype_judgement_delta_ensure_type_at_universe(
				delta,
				terms,
				type_declarations,
				codomain,
				universe
			) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		uint32_t lambda_universe_family;
		uint32_t lambda_classifier;
		uint32_t returned;
		uint32_t empty_effects;
		uint32_t returned_classifier;
		uint32_t family_classifier;
		if (prototype_term_effect_label(
					terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects
				) != 0 || prototype_term_computation_type(
					terms, empty_effects, universe, &returned_classifier
				) != 0 || prototype_term_pure_family(
					terms, codomain_binder, returned_classifier, &lambda_universe_family
				) != 0 || prototype_term_pi_family(
					terms, domain, lambda_universe_family, &lambda_classifier
				) != 0 || prototype_term_thunk_type(
					terms, lambda_classifier, &family_classifier
				) != 0 ||
			codomain_family >= terms->term_count ||
			terms->terms[subject].as.pi.codomain_family >= terms->term_count) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		uint32_t family = terms->terms[subject].as.pi.codomain_family;
		returned = terms->terms[family].as.thunk.computation;
		if (returned >= terms->term_count ||
			terms->terms[returned].tag != PROTOTYPE_TERM_LAMBDA ||
			terms->terms[returned].as.lambda.binding_id != codomain_binder ||
			terms->terms[returned].as.lambda.body >= terms->term_count ||
			terms->terms[terms->terms[returned].as.lambda.body].tag != PROTOTYPE_TERM_RETURN ||
			terms->terms[terms->terms[returned].as.lambda.body].as.return_term.value != codomain) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		uint32_t returned_body = terms->terms[returned].as.lambda.body;
		uint32_t returned_binder;
		if (prototype_term_var(terms, codomain_binder, &returned_binder) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		struct prototype_judgement_selected_evidence codomain_evidence;
		if (lookup_delta_prior_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				codomain,
				universe,
				&codomain_evidence
			) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		uint32_t return_subjects[1] = { codomain };
		uint32_t return_classifiers[1] = { codomain_evidence.classifier };
		if (add_delta_relation_with_authority_and_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
				returned_body,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				returned_body,
				returned_classifier,
				PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
				NULL,
				NULL,
				return_subjects,
				return_classifiers,
				&codomain_evidence,
				1
			) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		struct prototype_judgement_selected_evidence binder_evidence;
		struct prototype_judgement_selected_evidence body_evidence;
		if (prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				delta->current_context_id,
				returned_binder,
				domain,
				&binder_evidence
			) != 0 || prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				delta->current_context_id,
				returned_body,
				returned_classifier,
				&body_evidence
			) != 0) {
			prototype_judgement_delta_set_context(delta, outer_context);
			prototype_judgement_delta_set_operation(delta, outer_operation);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, outer_context);
		prototype_judgement_delta_set_operation(delta, outer_operation);
		if (prototype_judgement_delta_record_lambda_intro(
				delta,
				terms,
				type_declarations,
				PROTOTYPE_INVALID_ID,
				returned,
				lambda_classifier,
				returned_binder,
				returned_body,
				&binder_evidence,
				PROTOTYPE_INVALID_ID,
				&body_evidence
			) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence lambda_evidence;
		if (prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				delta->current_context_id,
				returned,
				lambda_classifier,
				&lambda_evidence
			) != 0) {
			return -1;
		}
		uint32_t thunk_subjects[1] = { returned };
		uint32_t thunk_classifiers[1] = { lambda_classifier };
		if (add_delta_relation_with_authority_and_explicit_premises(
				delta,
				PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
				family,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				family,
				family_classifier,
				PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
				NULL,
				NULL,
				thunk_subjects,
				thunk_classifiers,
				&lambda_evidence,
				1
			) != 0) {
			return -1;
		}
		uint32_t premise_subjects[3] = {
			domain,
			codomain,
			family
		};
		uint32_t premise_classifiers[3] = {
			universe,
			universe,
			family_classifier
		};
		struct prototype_judgement_selected_evidence formation_evidence[3];
		int domain_evidence_status = lookup_delta_subject_local_evidence_exact(
			delta,
			outer_context,
			domain,
			universe,
			&formation_evidence[0]
		);
		if (domain_evidence_status == 1) {
			domain_evidence_status = prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				outer_context,
				domain,
				universe,
				&formation_evidence[0]
			);
		}
		int codomain_evidence_status = lookup_delta_subject_local_evidence_exact(
			delta,
			binder_context,
			codomain,
			universe,
			&formation_evidence[1]
		);
		if (codomain_evidence_status == 1) {
			codomain_evidence_status = prototype_judgement_delta_select_evidence(
				delta,
				PROTOTYPE_INVALID_ID,
				binder_context,
				codomain,
				universe,
				&formation_evidence[1]
			);
		}
		if (domain_evidence_status != 0 || codomain_evidence_status != 0 ||
			prototype_judgement_delta_select_evidence(
				delta, PROTOTYPE_INVALID_ID, delta->current_context_id,
				family, family_classifier, &formation_evidence[2]
			) != 0) {
			return -1;
		}
		return add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			subject,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			universe,
			PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO,
			NULL,
			NULL,
			premise_subjects,
			premise_classifiers,
			formation_evidence,
			3
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_APP)) {
		uint32_t inferred;
		int status = prototype_judgement_delta_expand_app(
			delta,
			terms,
			type_declarations,
			subject,
			&inferred
		);
		if (status < 0) {
			return -1;
		}
			if (status > 0) {
				const struct prototype_term* app = &terms->terms[subject];
				if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
					uint32_t argument_classifiers[32];
					uint32_t argument_classifier_count = 0;
					if (collect_subject_classifiers(
							delta,
							terms,
							type_declarations,
							app->as.app.argument,
							argument_classifiers,
							32,
							&argument_classifier_count
						) != 0) {
						return -1;
					}
					for (uint32_t i = 0; i < argument_classifier_count; ++i) {
						uint32_t argument_classifier = argument_classifiers[i];
						const struct prototype_term* lambda = &terms->terms[app->as.app.function];
						uint32_t binder_var;
						if (prototype_term_var(
								terms,
								lambda->as.lambda.binding_id,
								&binder_var
							) != 0 ||
							ensure_lambda_binder_assumption(
								delta,
								terms,
								app->as.app.function,
								argument_classifier
							) != 0) {
							return -1;
						}
						if (prototype_judgement_delta_ensure_type_at_universe(
								delta,
								terms,
								type_declarations,
								lambda->as.lambda.body,
								universe
							) != 0) {
							return -1;
						}
						uint32_t classifier;
						if (prototype_term_pi(
								terms,
								argument_classifier,
								universe,
								&classifier
							) != 0) {
							return -1;
						}
						uint32_t premise_subjects[2] = {
							binder_var,
							lambda->as.lambda.body
						};
						uint32_t premise_classifiers[2] = {
							argument_classifier,
							universe
						};
						struct prototype_judgement_selected_evidence premise_evidence[2];
					if (lookup_delta_prior_evidence_normalization_equal(
								delta,
								terms,
								type_declarations,
								binder_var,
								argument_classifier,
								&premise_evidence[0]
						) != 0 || lookup_delta_prior_evidence_normalization_equal(
								delta,
								terms,
								type_declarations,
								lambda->as.lambda.body,
								universe,
								&premise_evidence[1]
							) != 0 || add_delta_relation_with_authority_and_explicit_premises(
								delta,
								PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
								app->as.app.function,
								PROTOTYPE_INVALID_ID,
								PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
								app->as.app.function,
								classifier,
								PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
								NULL,
								NULL,
								premise_subjects,
								premise_classifiers,
								premise_evidence,
								2
							) != 0) {
							return -1;
						}
					}
				}
			if (prototype_judgement_delta_infer_core_helper_facts(
					delta,
					terms,
					type_declarations
				) != 0) {
				return -1;
			}
			status = prototype_judgement_delta_expand_app(
				delta,
				terms,
				type_declarations,
				subject,
				&inferred
			);
			if (status < 0) {
				return -1;
			}
		}
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH)) {
		const struct prototype_term* match = &terms->terms[subject];
		if (match->as.match.case_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		struct prototype_judgement_selected_evidence premise_evidence[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		];
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			uint32_t case_id = match->as.match.first_case + i;
			if (case_id >= terms->case_count) {
				return -1;
			}
			uint32_t body = terms->cases[case_id].body;
			if (prototype_judgement_delta_ensure_type_at_universe(
					delta,
					terms,
					type_declarations,
					body,
					universe
				) != 0) {
				return -1;
			}
			premise_subjects[i] = body;
			premise_classifiers[i] = universe;
			if (lookup_delta_prior_evidence_normalization_equal(
					delta,
					terms,
					type_declarations,
					body,
					universe,
					&premise_evidence[i]
				) != 0) {
				return -1;
			}
		}
		if (prototype_judgement_delta_expand_match_motive_with_premises(
				delta,
				terms,
				subject,
				universe,
				premise_subjects,
				premise_classifiers,
				premise_evidence,
				match->as.match.case_count
			) != 0) {
			return -1;
		}
	}
	uint32_t actual;
	uint32_t actual_candidates[32];
	uint32_t actual_candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			subject,
			actual_candidates,
			32,
			&actual_candidate_count
		) != 0) {
		return -1;
	}
	actual = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < actual_candidate_count; ++i) {
		if (!prototype_judgement_classifier_compatible(
				terms,
				type_declarations,
				universe,
				actual_candidates[i]
			)) {
			continue;
		}
		if (actual != PROTOTYPE_INVALID_ID &&
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				actual,
				actual_candidates[i]
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		actual = actual_candidates[i];
	}
	if (actual == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (actual == universe) {
		return 0;
	}
	struct prototype_judgement_selected_evidence source_evidence;
	if (prototype_judgement_delta_select_evidence(
			delta,
			delta->current_operation_id,
			delta->current_context_id,
			subject,
			actual,
			&source_evidence
		) != 0) {
		return -1;
	}
	return prototype_judgement_delta_add_conversion(
		delta,
		terms,
		type_declarations,
		&source_evidence,
		universe
	);
}

int prototype_judgement_add_structural_type_formation_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe,
	uint32_t* p_claim_id
) {
	enum { CAPACITY = 512 };
	if (!judgement || !terms || !type_declarations || !contexts || !substitutions ||
		!p_claim_id) {
		return -1;
	}
	struct prototype_judgement_proposition* propositions = calloc(
		CAPACITY, sizeof(*propositions)
	);
	struct prototype_judgement_derivation_candidate* derivations = calloc(
		CAPACITY, sizeof(*derivations)
	);
	struct prototype_judgement_candidate_premise* premises = calloc(
		CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES, sizeof(*premises)
	);
	struct prototype_judgement_match_motive_result* motives = calloc(
		CAPACITY, sizeof(*motives)
	);
	struct prototype_judgement_computation_constraint* constraints = calloc(
		CAPACITY, sizeof(*constraints)
	);
	struct prototype_judgement_effect_row_constraint* effect_rows = calloc(
		CAPACITY, sizeof(*effect_rows)
	);
	if (!propositions || !derivations || !premises || !motives || !constraints ||
		!effect_rows) {
		free(propositions);
		free(derivations);
		free(premises);
		free(motives);
		free(constraints);
		free(effect_rows);
		return -1;
	}
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta,
		judgement,
		propositions,
		derivations,
		CAPACITY,
		premises,
		CAPACITY * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
		motives,
		CAPACITY,
		constraints,
		CAPACITY,
		effect_rows,
		CAPACITY
	);
	prototype_judgement_delta_set_context_store(&delta, contexts, substitutions);
	prototype_judgement_delta_set_context(&delta, context_id);
	int status = prototype_judgement_delta_ensure_type_at_universe(
		&delta, terms, type_declarations, subject, universe
	);
	if (status == 0) {
		status = prototype_judgement_delta_publish_complete(&delta);
	}
	if (status == 0) {
		status = 1;
		for (size_t i = judgement->claim_count; i > 0; --i) {
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(judgement, (uint32_t)i - 1);
			if (proposition && proposition->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				proposition->context_id == context_id && proposition->subject == subject &&
				proposition->classifier == universe) {
				*p_claim_id = (uint32_t)i - 1;
				status = 0;
				break;
			}
		}
	}
	free(propositions);
	free(derivations);
	free(premises);
	free(motives);
	free(constraints);
	free(effect_rows);
	return status;
}

static int prototype_judgement_delta_expand_match_motive_with_premises(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
) {
	if (!delta || !premise_subjects || !premise_classifiers ||
		!premise_evidence) {
		return -1;
	}
	if (!term_exists(terms, subject) ||
		!term_is_universe_var(terms, classifier) ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		premise_count != terms->terms[subject].as.match.case_count) {
		return -1;
	}
	for (uint32_t i = 0; i < premise_count; ++i) {
		uint32_t case_id = terms->terms[subject].as.match.first_case + i;
		if (case_id >= terms->case_count ||
			premise_subjects[i] != terms->cases[case_id].body ||
			premise_classifiers[i] != classifier ||
			premise_evidence[i].kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			premise_evidence[i].authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
			premise_evidence[i].subject != premise_subjects[i] ||
			premise_evidence[i].classifier != premise_classifiers[i]) {
			return -1;
		}
	}
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		subject,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		premise_count
	);
}

int prototype_judgement_delta_expand_match_motive(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_is_universe_var(terms, classifier) ||
		terms->terms[subject].as.match.case_count != 0) {
		return -1;
	}
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		subject,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		0
	);
}

int prototype_judgement_delta_build_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	const struct prototype_match_case_input* motive_cases,
	uint32_t case_count,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !motive_cases || !p_motive_result ||
		scrutinee >= terms->term_count ||
		scrutinee_classifier >= terms->term_count ||
		case_count > 64) {
		return -1;
	}
	(void)type_declarations;

	uint32_t motive_binder_id = prototype_term_new_binding(terms);
	uint32_t motive_binder_var;
	uint32_t motive_body;
	uint32_t motive_lambda;
	uint32_t motive_pi;
	uint32_t motive_universe;
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_selected_evidence premise_evidence[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (motive_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (prototype_term_var(
		terms,
		motive_binder_id,
		&motive_binder_var
	) != 0) {
		return -1;
	}
	if (prototype_term_match(
		terms,
		motive_binder_var,
		motive_cases,
		case_count,
		&motive_body
	) != 0) {
		return -1;
	}
	if (prototype_term_lambda(
			terms,
			motive_binder_id,
			motive_body,
			&motive_lambda
		) != 0 || motive_lambda >= terms->term_count ||
		terms->terms[motive_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	motive_binder_id = terms->terms[motive_lambda].as.lambda.binding_id;
	motive_body = terms->terms[motive_lambda].as.lambda.body;
	if (prototype_term_var(
			terms,
			motive_binder_id,
			&motive_binder_var
		) != 0) {
		return -1;
	}
	uint32_t outer_context = delta->current_context_id;
	uint32_t outer_operation = delta->current_operation_id;
	uint32_t motive_context;
	if (!delta->contexts || prototype_context_extend(
			delta->contexts,
			outer_context,
			motive_binder_id,
			scrutinee_classifier,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_context(delta, motive_context);
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	if (prototype_judgement_delta_record_context_binding_assumption(
			delta,
			terms,
			motive_binder_id,
			scrutinee_classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t case_id = terms->terms[motive_body].as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* motive_case = &terms->cases[case_id];
		for (uint32_t j = 0; j < motive_case->binder_count; ++j) {
			const struct prototype_case_binder* binder =
				&terms->case_binders[motive_case->first_binder + j];
			uint32_t binder_var;
			uint32_t binder_classifier;
			if (prototype_term_var(
					terms,
					binder->binding_id,
					&binder_var
				) != 0 ||
				constructor_field_classifier_from_spine(
					delta,
					terms,
					type_declarations,
					scrutinee_classifier,
					motive_case->constructor_id,
					&terms->case_binders[motive_case->first_binder],
					j,
					j,
					&binder_classifier
				) != 0 ||
				prototype_judgement_delta_record_match_pattern(
					delta,
					terms,
					binder_var,
					binder_classifier,
					scrutinee_classifier,
					motive_case->constructor_id,
					j
			) != 0) {
				return -1;
			}
		}
	}
	if (prototype_term_universe_var(
		terms,
		universe_level_var,
		&motive_universe
	) != 0) {
		return -1;
	}
	if (case_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t case_id = terms->terms[motive_body].as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		uint32_t motive_case_body = terms->cases[case_id].body;
			if (prototype_judgement_delta_ensure_type_at_universe(
					delta,
					terms,
					type_declarations,
					motive_case_body,
					motive_universe
				) != 0) {
				return -1;
			}
		premise_subjects[i] = motive_case_body;
		premise_classifiers[i] = motive_universe;
		if (lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				motive_case_body,
				motive_universe,
				&premise_evidence[i]
			) != 0) {
			return -1;
		}
	}
		if (prototype_judgement_delta_expand_match_motive_with_premises(
			delta,
			terms,
		motive_body,
		motive_universe,
			premise_subjects,
			premise_classifiers,
			premise_evidence,
			case_count
		) != 0) {
			return -1;
		}
	if (prototype_term_pi(
		terms,
		scrutinee_classifier,
		motive_universe,
		&motive_pi
	) != 0) {
		return -1;
	}
	uint32_t lambda_premise_subjects[2] = {
		motive_binder_var,
		motive_body
	};
	uint32_t lambda_premise_classifiers[2] = {
		scrutinee_classifier,
		motive_universe
	};
	struct prototype_judgement_selected_evidence lambda_premise_evidence[2];
	uint32_t lambda_premise_context_ids[2] = {
		motive_context,
		motive_context
	};
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			motive_binder_var,
			scrutinee_classifier,
			&lambda_premise_evidence[0]
		) != 0 || lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			motive_body,
			motive_universe,
			&lambda_premise_evidence[1]
		) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_context(delta, outer_context);
	prototype_judgement_delta_set_operation(delta, outer_operation);
	if (add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			motive_lambda,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			motive_lambda,
			motive_pi,
			PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			lambda_premise_context_ids,
			NULL,
			lambda_premise_subjects,
			lambda_premise_classifiers,
			lambda_premise_evidence,
			2
		) != 0) {
		return -1;
	}
	struct prototype_judgement_selected_evidence lambda_evidence;
	if (prototype_judgement_delta_select_evidence(
			delta,
			PROTOTYPE_INVALID_ID,
			delta->current_context_id,
			motive_lambda,
			motive_pi,
			&lambda_evidence
		) != 0) {
		return -1;
	}
	if (prototype_term_app(terms, motive_lambda, scrutinee, p_motive_result) != 0) {
		return -1;
	}
	uint32_t app_premise_subjects[2] = {
		motive_lambda,
		scrutinee
	};
	uint32_t app_premise_classifiers[2] = {
		motive_pi,
		scrutinee_classifier
	};
	struct prototype_judgement_selected_evidence app_premise_evidence[2];
	app_premise_evidence[0] = lambda_evidence;
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			scrutinee,
			scrutinee_classifier,
			&app_premise_evidence[1]
		) != 0) {
		return -1;
	}
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		*p_motive_result,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		*p_motive_result,
		motive_universe,
		PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
		NULL,
		NULL,
		app_premise_subjects,
		app_premise_classifiers,
		app_premise_evidence,
		2
	);
}

int prototype_judgement_delta_build_constant_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	uint32_t result_type,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !p_motive_result ||
		scrutinee >= terms->term_count ||
		scrutinee_classifier >= terms->term_count ||
		result_type >= terms->term_count || !delta->contexts) {
		return -1;
	}
	uint32_t motive_binding = prototype_term_new_binding(terms);
	uint32_t motive_var;
	uint32_t motive_lambda;
	uint32_t motive_universe;
	uint32_t motive_pi;
	uint32_t motive_context;
	uint32_t outer_context = delta->current_context_id;
	uint32_t outer_operation = delta->current_operation_id;
	if (motive_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, motive_binding, &motive_var
		) != 0 || prototype_term_lambda(
			terms, motive_binding, result_type, &motive_lambda
		) != 0 || prototype_term_universe_var(
			terms, universe_level_var, &motive_universe
		) != 0 || prototype_context_extend(
			delta->contexts,
			outer_context,
			motive_binding,
			scrutinee_classifier,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_context(delta, motive_context);
	prototype_judgement_delta_set_operation(delta, PROTOTYPE_INVALID_ID);
	if (prototype_judgement_delta_record_context_binding_assumption(
			delta, terms, motive_binding, scrutinee_classifier
		) != 0 || prototype_judgement_delta_ensure_type_at_universe(
			delta, terms, type_declarations, result_type, motive_universe
		) != 0 || prototype_term_pi(
			terms, scrutinee_classifier, motive_universe, &motive_pi
		) != 0) {
		return -1;
	}
	struct prototype_judgement_selected_evidence lambda_evidence[2];
	if (prototype_judgement_delta_select_evidence(
			delta,
			PROTOTYPE_INVALID_ID,
			motive_context,
			motive_var,
			scrutinee_classifier,
			&lambda_evidence[0]
		) != 0 || lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			result_type,
			motive_universe,
			&lambda_evidence[1]
		) != 0) {
		return -1;
	}
	uint32_t lambda_subjects[2] = { motive_var, result_type };
	uint32_t lambda_classifiers[2] = {
		scrutinee_classifier, motive_universe
	};
	uint32_t lambda_contexts[2] = { motive_context, motive_context };
	prototype_judgement_delta_set_context(delta, outer_context);
	prototype_judgement_delta_set_operation(delta, outer_operation);
	if (add_delta_relation_with_authority_and_explicit_premises(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
			motive_lambda,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			motive_lambda,
			motive_pi,
			PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			lambda_contexts,
			NULL,
			lambda_subjects,
			lambda_classifiers,
			lambda_evidence,
			2
		) != 0 || prototype_term_app(
			terms, motive_lambda, scrutinee, p_motive_result
		) != 0) {
		return -1;
	}
	struct prototype_judgement_selected_evidence app_evidence[2];
	if (prototype_judgement_delta_select_evidence(
			delta,
			PROTOTYPE_INVALID_ID,
			outer_context,
			motive_lambda,
			motive_pi,
			&app_evidence[0]
		) != 0 || lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			scrutinee,
			scrutinee_classifier,
			&app_evidence[1]
		) != 0) {
		return -1;
	}
	uint32_t app_subjects[2] = { motive_lambda, scrutinee };
	uint32_t app_classifiers[2] = { motive_pi, scrutinee_classifier };
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		*p_motive_result,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		*p_motive_result,
		motive_universe,
		PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
		NULL,
		NULL,
		app_subjects,
		app_classifiers,
		app_evidence,
		2
	);
}

/*
 * Compare already-erased Match cases whose locally bound core IDs differ.
 * Source elaboration must use ContextDB/SubstitutionDB instead; this private
 * helper is only an alpha transport for legacy TermDB proof validation.
 */
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

int prototype_judgement_expand_match(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_has_tag(terms, classifier, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[subject];
	const struct prototype_term* motive_app = &terms->terms[classifier];
	const struct prototype_term* motive_lambda;
	const struct prototype_term* motive_body;
	uint32_t motive_result_classifier;
	uint32_t normalized_classifier;
	if (motive_app->as.app.argument != match->as.match.scrutinee ||
		!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA) ||
		classifier_kernel_whnf_no_definitions(terms, type_declarations, classifier, &normalized_classifier) != 0) {
		return -1;
	}
	if (select_judgement_universe_classifier(
			judgement,
			terms,
			type_declarations,
			classifier,
			&motive_result_classifier
		) != 0) {
		return -1;
	}
	(void)normalized_classifier;
	motive_lambda = &terms->terms[motive_app->as.app.function];
	motive_body = &terms->terms[motive_lambda->as.lambda.body];
	int motive_is_case_split = motive_body->tag == PROTOTYPE_TERM_MATCH;
	if (motive_is_case_split &&
		(motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binding_id !=
			motive_lambda->as.lambda.binding_id)) {
		return -1;
	}
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	if (match->as.match.case_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t premise_count = match->as.match.case_count + 1;
	premise_subjects[0] = classifier;
	premise_classifiers[0] = motive_result_classifier;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_is_case_split ?
			motive_body->as.match.first_case + i : PROTOTYPE_INVALID_ID;
		if (case_id >= terms->case_count ||
			(motive_is_case_split && motive_case_id >= terms->case_count)) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = motive_is_case_split ?
			&terms->cases[motive_case_id] : NULL;
		if (motive_is_case_split &&
			(match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count)) {
			return -1;
		}
		if (motive_is_case_split &&
			(match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			motive_case->constructor_owner == PROTOTYPE_INVALID_ID)) {
			if (match_case->constructor_owner != motive_case->constructor_owner) {
				return -1;
			}
		} else if (motive_is_case_split) {
			int same_owner = 0;
			if (prototype_term_view_shape_equal(
				terms,
				match_case->constructor_owner,
				motive_case->constructor_owner,
				&same_owner
			) != 0 ||
			!same_owner) {
			return -1;
			}
		}
		if ((motive_is_case_split && select_judgement_match_branch_classifier_for_motive(
				judgement,
				terms,
				type_declarations,
				match_case,
				motive_case,
				&premise_classifiers[i + 1]
			) != 0) || (!motive_is_case_split &&
			match_case_motive_classifier(
				terms,
				match_case,
				motive_app->as.app.function,
				&premise_classifiers[i + 1]
			) != 0)) {
			return -1;
		}
		premise_subjects[i + 1] = terms->cases[case_id].body;
	}
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_count
	);
}

static int delta_record_substitution_reindex(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source,
	uint32_t substitution_id,
	struct prototype_judgement_selected_evidence* evidence
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(delta ? delta->substitutions : NULL, substitution_id);
	uint32_t subject;
	uint32_t classifier;
	if (!delta || !terms || !type_declarations || !source || !evidence ||
		!substitution || source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		substitution->target_context != source->context_id ||
		prototype_term_reindex(
			terms,
			type_declarations,
			delta->contexts,
			delta->substitutions,
			source->subject,
			substitution_id,
			&subject
		) != 0 || prototype_term_reindex(
			terms,
			type_declarations,
			delta->contexts,
			delta->substitutions,
			source->classifier,
			substitution_id,
			&classifier
		) != 0) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		.authority_id = subject,
		.context_id = substitution->source_context,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = subject;
	proof.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&proof, premises);
	proof.semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
	proof.semantic_action_id = substitution_id;
	proof.premise_count = 1;
	proof.premises[0].proposition.kind = source->kind;
	proof.premises[0].proposition.authority_kind = source->authority_kind;
	proof.premises[0].proposition.authority_id = source->authority_id;
	proof.premises[0].proposition.context_id = source->context_id;
	proof.premises[0].proposition.operation_id = source->operation_id;
	proof.premises[0].proposition.subject = source->subject;
	proof.premises[0].proposition.classifier = source->classifier;
	if (add_complete_delta_relation(delta, &relation, &proof) != 0) {
		return -1;
	}
	selected_evidence_from_candidate(&relation, evidence);
	return 0;
}

int prototype_judgement_delta_expand_match(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_has_tag(terms, classifier, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[subject];
	const struct prototype_term* motive_app = &terms->terms[classifier];
	const struct prototype_term* motive_lambda;
	const struct prototype_term* motive_body;
	uint32_t motive_result_classifier;
	uint32_t normalized_classifier;
	if (motive_app->as.app.argument != match->as.match.scrutinee ||
		!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA) ||
		classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			classifier,
			&normalized_classifier
			) != 0) {
		return -1;
	}
	if (select_delta_universe_classifier(
			delta,
			terms,
			type_declarations,
			classifier,
			&motive_result_classifier
		) != 0) {
		return -1;
	}
	(void)normalized_classifier;
	motive_lambda = &terms->terms[motive_app->as.app.function];
	motive_body = &terms->terms[motive_lambda->as.lambda.body];
	int motive_is_case_split = motive_body->tag == PROTOTYPE_TERM_MATCH;
	if (motive_is_case_split &&
		(motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binding_id !=
			motive_lambda->as.lambda.binding_id)) {
		return -1;
	}
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_selected_evidence premise_evidence[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	int premise_semantic_action_kinds[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	uint32_t premise_semantic_action_ids[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (match->as.match.case_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t premise_count = match->as.match.case_count + 1;
	for (uint32_t i = 0; i < premise_count; ++i) {
		premise_semantic_action_kinds[i] =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		premise_semantic_action_ids[i] = PROTOTYPE_INVALID_ID;
	}
	premise_subjects[0] = classifier;
	premise_classifiers[0] = motive_result_classifier;
	if (lookup_delta_selected_evidence_normalization_equal(
			delta,
			terms,
			type_declarations,
			classifier,
			motive_result_classifier,
			&premise_evidence[0]
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_is_case_split ?
			motive_body->as.match.first_case + i : PROTOTYPE_INVALID_ID;
		if (case_id >= terms->case_count ||
			(motive_is_case_split && motive_case_id >= terms->case_count)) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = motive_is_case_split ?
			&terms->cases[motive_case_id] : NULL;
		if (motive_is_case_split &&
			(match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count)) {
			return -1;
		}
		if (motive_is_case_split &&
			(match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			motive_case->constructor_owner == PROTOTYPE_INVALID_ID)) {
			if (match_case->constructor_owner != motive_case->constructor_owner) {
				return -1;
			}
		} else if (motive_is_case_split) {
			int same_owner = 0;
			if (prototype_term_view_shape_equal(
					terms,
					match_case->constructor_owner,
					motive_case->constructor_owner,
					&same_owner
				) != 0 ||
				!same_owner) {
				return -1;
			}
		}
		if ((motive_is_case_split && select_delta_match_branch_classifier_for_motive(
				delta,
				terms,
				type_declarations,
				match_case,
				motive_case,
				&premise_classifiers[i + 1]
			) != 0) || (!motive_is_case_split &&
			match_case_motive_classifier(
				terms,
				match_case,
				motive_app->as.app.function,
				&premise_classifiers[i + 1]
			) != 0)) {
			return -1;
		}
		premise_subjects[i + 1] = terms->cases[case_id].body;
		int refined = 0;
		if (delta->operations && delta->operation_cases &&
			delta->current_operation_id < delta->operation_count) {
			const struct prototype_operation_node* operation =
				&delta->operations[delta->current_operation_id];
			if (operation->tag == PROTOTYPE_OPERATION_MATCH &&
				operation->core_term == subject && operation->scrutinee <
					delta->operation_count && operation->first_case + i <
					delta->operation_case_count) {
				const struct prototype_operation_node* scrutinee_operation =
					&delta->operations[operation->scrutinee];
				uint32_t indexed_type_id;
				uint32_t indexed_arguments[16];
				uint32_t indexed_argument_count;
				if (prototype_term_type_instance_info(
						terms,
						scrutinee_operation->classifier,
						&indexed_type_id,
						indexed_arguments,
						&indexed_argument_count
					) == 0 && indexed_type_id < type_declarations->type_count) {
					const struct prototype_type_declaration* indexed_type =
						&type_declarations->type_declarations[indexed_type_id];
					if (indexed_type->origin_kind ==
							PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
						indexed_type->parameter_count == 0 &&
						indexed_type->index_count == 2 &&
						indexed_argument_count == 2) {
						const struct prototype_operation_match_case* operation_case =
							&delta->operation_cases[operation->first_case + i];
						const struct prototype_operation_node* body_operation =
							operation_case->body_operation < delta->operation_count ?
							&delta->operations[operation_case->body_operation] : NULL;
						uint32_t refined_context;
						uint32_t refinement_substitution;
						uint32_t constructor_term;
						struct prototype_judgement_selected_evidence source_evidence;
						uint32_t saved_context = delta->current_context_id;
						uint32_t saved_operation = delta->current_operation_id;
						if (!body_operation ||
							operation_case->constructor_id != match_case->constructor_id ||
							prototype_judgement_indexed_branch_refinement(
								delta->contexts,
								delta->substitutions,
								terms,
								type_declarations,
								scrutinee_operation->context_id,
								scrutinee_operation->core_term,
								scrutinee_operation->classifier,
								match_case->constructor_id,
								operation_case->context_id,
								&terms->case_binders[match_case->first_binder],
								match_case->binder_count,
								&refined_context,
								&refinement_substitution,
								&constructor_term
							) != 0 || operation_case->context_id !=
								prototype_substitution_get(
									delta->substitutions, refinement_substitution
								)->target_context || prototype_judgement_delta_select_evidence(
								delta,
								operation_case->body_operation,
								operation_case->context_id,
								body_operation->core_term,
								body_operation->classifier,
								&source_evidence
							) != 0 || delta_record_substitution_reindex(
								delta,
								terms,
								type_declarations,
								&source_evidence,
								refinement_substitution,
								&premise_evidence[i + 1]
							) != 0 || prototype_term_reindex(
								terms,
								type_declarations,
								delta->contexts,
								delta->substitutions,
								motive_is_case_split ? motive_case->body :
									motive_lambda->as.lambda.body,
								refinement_substitution,
								&premise_classifiers[i + 1]
							) != 0 || premise_evidence[i + 1].context_id !=
								refined_context) {
							return -1;
						}
						if (premise_evidence[i + 1].classifier !=
							premise_classifiers[i + 1]) {
							prototype_judgement_delta_set_context(
								delta, refined_context
							);
							prototype_judgement_delta_set_operation(
								delta, PROTOTYPE_INVALID_ID
							);
							if (prototype_judgement_delta_record_expected_type_exposure(
									delta,
									terms,
									type_declarations,
									&premise_evidence[i + 1],
									premise_classifiers[i + 1],
									premise_evidence[i + 1].subject
								) != 0 || prototype_judgement_delta_select_evidence(
									delta,
									PROTOTYPE_INVALID_ID,
									refined_context,
									premise_evidence[i + 1].subject,
									premise_classifiers[i + 1],
									&premise_evidence[i + 1]
								) != 0) {
								return -1;
							}
							prototype_judgement_delta_set_context(
								delta, saved_context
							);
							prototype_judgement_delta_set_operation(
								delta, saved_operation
							);
						}
						premise_subjects[i + 1] =
							premise_evidence[i + 1].subject;
						premise_semantic_action_kinds[i + 1] =
							PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
						premise_semantic_action_ids[i + 1] =
							refinement_substitution;
						refined = 1;
						(void)constructor_term;
					}
				}
			}
		}
		if (!refined && lookup_delta_selected_evidence_normalization_equal(
				delta,
				terms,
				type_declarations,
				premise_subjects[i + 1],
				premise_classifiers[i + 1],
				&premise_evidence[i + 1]
			) != 0) {
			return -1;
		}
	}
	int conclusion_authority = delta->current_operation_id <
		delta->operation_count ? PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION :
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
	uint32_t conclusion_authority_id = conclusion_authority ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ? delta->current_operation_id :
		subject;
	uint32_t conclusion_operation_id = conclusion_authority ==
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ? delta->current_operation_id :
		PROTOTYPE_INVALID_ID;
	int status = add_delta_relation_with_authority_and_explicit_premise_actions(
		delta,
		conclusion_authority,
		conclusion_authority_id,
		conclusion_operation_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		premise_semantic_action_kinds,
		premise_semantic_action_ids,
		premise_count
	);
	if (status == 0) {
		remove_match_motive_results_normalization_equal(
			delta,
			terms,
			type_declarations,
			subject,
			classifier
		);
	}
	return status;
}

int prototype_judgement_delta_expand_induction_hypothesis(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	uint32_t match_term,
	uint32_t motive,
	uint32_t case_index,
	uint32_t field_index
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		!term_exists(terms, classifier) ||
		!term_exists(terms, motive) ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
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
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = relation.operation_id;
	proof.conclusion_subject = relation.subject;
	proof.conclusion_classifier = relation.classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.rule_data.induction.match = match_term;
	proof.rule_data.induction.motive = motive;
	proof.rule_data.induction.case_index = case_index;
	proof.rule_data.induction.field_index = field_index;
	return add_complete_delta_relation(delta, &relation, &proof);
}

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

int prototype_judgement_delta_record_lambda_intro(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t conclusion_operation_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t binder_subject,
	uint32_t body_subject,
	const struct prototype_judgement_selected_evidence* binder_evidence,
	uint32_t body_operation_id,
	const struct prototype_judgement_selected_evidence* body_evidence
) {
	if (!delta || !terms || !type_declarations ||
		!binder_evidence || !body_evidence ||
		binder_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		body_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		binder_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		body_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_LAMBDA) ||
		!term_has_tag(terms, binder_subject, PROTOTYPE_TERM_VAR) ||
		!term_exists(terms, body_subject) ||
		!term_exists(terms, classifier) ||
		binder_evidence->subject != binder_subject ||
		body_evidence->subject != body_subject ||
		!term_exists(terms, binder_evidence->classifier) ||
		!term_exists(terms, body_evidence->classifier)) {
		return -1;
	}
	uint32_t binder_classifier = binder_evidence->classifier;
	uint32_t body_classifier = body_evidence->classifier;
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t occurrence_lambda;
	int occurrence_matches;
	uint32_t expected_body_classifier;
	uint32_t lambda_classifier;
	uint32_t binder_id = terms->terms[binder_subject].as.var.binding_id;
	if (classifier_kernel_strip_effect_row_foralls(
			terms, type_declarations, NULL, classifier, &lambda_classifier
		) != 0 || classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			lambda_classifier,
			&lambda_pi,
			&domain,
			&codomain_family
		) != 0 ||
		prototype_term_lambda(terms, binder_id, body_subject, &occurrence_lambda) != 0 ||
		prototype_term_core_shape_equal(
			terms, occurrence_lambda, subject, &occurrence_matches
		) != 0 || !occurrence_matches ||
		pi_codomain_at_binder_in_context(
			delta,
			terms,
			type_declarations,
			body_evidence->context_id,
			lambda_pi,
			binder_subject,
			binder_classifier,
			PROTOTYPE_INVALID_ID,
			&expected_body_classifier
		) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, domain, binder_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ||
		!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			expected_body_classifier,
			body_classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	(void)codomain_family;
	uint32_t premise_subjects[2] = {
		binder_subject,
		body_subject
	};
	uint32_t premise_classifiers[2] = {
		binder_classifier,
		body_classifier
	};
	uint32_t premise_context_ids[2] = {
		binder_evidence->context_id,
		body_evidence->context_id
	};
	uint32_t premise_operation_ids[2] = {
		PROTOTYPE_INVALID_ID,
		body_operation_id
	};
	struct prototype_judgement_selected_evidence premise_evidence[2] = {
		*binder_evidence,
		*body_evidence
	};
	int authority_kind = conclusion_operation_id == PROTOTYPE_INVALID_ID ?
		PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER :
		PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION;
	uint32_t authority_id = conclusion_operation_id == PROTOTYPE_INVALID_ID ?
		subject : conclusion_operation_id;
	return add_delta_relation_with_authority_and_explicit_premises(
		delta,
		authority_kind,
		authority_id,
		conclusion_operation_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
		premise_context_ids,
		premise_operation_ids,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		2
	);
}

int prototype_judgement_delta_app_elim_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function_classifier,
	uint32_t argument_subject,
	uint32_t argument_classifier,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		!term_exists(terms, function_classifier) ||
		!term_exists(terms, argument_subject) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	uint32_t specialized_function_classifier;
	if (prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			function_classifier,
			argument_classifier,
			&specialized_function_classifier
		) != 0) {
		return -1;
	}
	uint32_t function_pi;
	uint32_t domain;
	uint32_t codomain_family;
	if (classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			specialized_function_classifier,
			&function_pi,
			&domain,
			&codomain_family
		) != 0) {
		return -1;
	}
	uint32_t domain_whnf;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			domain,
			&domain_whnf
		) != 0 || domain_whnf >= terms->term_count) {
		return -1;
	}
	if (terms->terms[domain_whnf].tag == PROTOTYPE_TERM_RETURN) {
		domain_whnf = terms->terms[domain_whnf].as.return_term.value;
	}
	if (!prototype_judgement_classifier_compatible(
			terms, type_declarations, domain_whnf, argument_classifier
		)) {
		return -1;
	}
	(void)codomain_family;
	return pi_codomain_after_argument_in_context(
		delta->contexts,
		delta->substitutions,
		terms,
		type_declarations,
		delta->current_context_id,
		function_pi,
		argument_subject,
		argument_classifier,
		p_classifier
	);
}

int prototype_judgement_delta_record_app_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const struct prototype_judgement_selected_evidence* function_evidence,
	const struct prototype_judgement_selected_evidence* argument_evidence
) {
	if (!delta || !terms || !type_declarations ||
		!function_evidence || !argument_evidence ||
		function_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		argument_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		function_evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		argument_evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		subject >= terms->term_count ||
		function_evidence->subject >= terms->term_count ||
		argument_evidence->subject >= terms->term_count ||
		!term_exists(terms, classifier) ||
		!term_exists(terms, function_evidence->classifier) ||
		!term_exists(terms, argument_evidence->classifier)) {
		return -1;
	}
	const struct prototype_term* app;
	if (term_core_app(terms, subject, &app) != 0) {
		return -1;
	}
	int function_matches = 0;
	int argument_matches = 0;
	if (prototype_term_core_shape_equal(
			terms,
			app->as.app.function,
			function_evidence->subject,
			&function_matches
		) != 0 ||
		prototype_term_core_shape_equal(
			terms,
			app->as.app.argument,
			argument_evidence->subject,
			&argument_matches
		) != 0 ||
		!function_matches || !argument_matches) {
		return -1;
	}
	uint32_t result_classifier;
	int result_status = prototype_judgement_delta_app_elim_classifier(
		delta,
		terms,
		type_declarations,
		function_evidence->classifier,
		argument_evidence->subject,
		argument_evidence->classifier,
		&result_classifier
	);
	int result_equal = result_status == 0 &&
		(prototype_judgement_classifier_conversion(
			terms, type_declarations, result_classifier, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL);
	if (result_status != 0 || !result_equal) {
		return -1;
	}
	uint32_t premise_subjects[2] = {
		function_evidence->subject,
		argument_evidence->subject
	};
	uint32_t premise_classifiers[2] = {
		function_evidence->classifier,
		argument_evidence->classifier
	};
	struct prototype_judgement_selected_evidence premise_evidence[2] = {
		*function_evidence,
		*argument_evidence
	};
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_evidence,
		2
	);
}

int prototype_judgement_delta_record_context_weaken(
	struct prototype_judgement_delta* delta,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t substitution_id
) {
	if (!delta || !source_evidence ||
		(source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		 source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		source_evidence->context_id == delta->current_context_id ||
		!prototype_substitution_is_projection_path(
			delta->substitutions,
			delta->contexts,
			substitution_id,
			delta->current_context_id,
			source_evidence->context_id
		)) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = source_evidence->kind,
		.authority_kind = source_evidence->authority_kind,
		.authority_id = source_evidence->authority_id,
		.context_id = delta->current_context_id,
		.operation_id = source_evidence->authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
			source_evidence->operation_id : PROTOTYPE_INVALID_ID,
		.subject = source_evidence->subject,
		.classifier = source_evidence->classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = relation.operation_id;
	proof.conclusion_subject = relation.subject;
	proof.conclusion_classifier = relation.classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
	proof.semantic_action_id = substitution_id;
	proof.premise_count = 1;
	proof.premises[0].proposition.kind = source_evidence->kind;
	proof.premises[0].proposition.context_id = source_evidence->context_id;
	proof.premises[0].proposition.subject = source_evidence->subject;
	proof.premises[0].proposition.classifier = source_evidence->classifier;
	proof.premises[0].proposition.authority_kind = source_evidence->authority_kind;
	proof.premises[0].proposition.authority_id = source_evidence->authority_id;
	proof.premises[0].proposition.operation_id = source_evidence->operation_id;
	return add_complete_delta_relation(delta, &relation, &proof);
}

static void selected_evidence_from_accepted_claim(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_claim* claim,
	struct prototype_judgement_selected_evidence* evidence
) {
	memset(evidence, 0, sizeof(*evidence));
	evidence->kind = prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind;
	evidence->authority_kind = prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind;
	evidence->authority_id = prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_id;
	evidence->context_id = prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id;
	evidence->operation_id = prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id;
	evidence->subject = prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject;
	evidence->classifier = prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier;
}

int prototype_judgement_selected_evidence_from_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	struct prototype_judgement_selected_evidence* p_evidence
) {
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	if (!claim || !p_evidence ||
		!prototype_judgement_proposition_get(judgement, claim->proposition_id)) {
		return -1;
	}
	selected_evidence_from_accepted_claim(judgement, claim, p_evidence);
	return 0;
}

static int add_claim_reindexed_by_substitution(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_claim_id,
	uint32_t substitution_id,
	int proof_kind,
	int require_projection,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* source =
		prototype_judgement_claim_get(judgement, source_claim_id);
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	uint32_t subject;
	uint32_t classifier;
	if (!source || !substitution || !p_claim_id ||
		(prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		 prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) ||
		substitution->target_context != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id) {
		return -1;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY) {
		*p_claim_id = source_claim_id;
		return 0;
	}
	if ((require_projection && !prototype_substitution_is_projection_path(
			substitutions,
			contexts,
			substitution_id,
			substitution->source_context,
			prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id
		)) || (!require_projection &&
			(prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				prototype_judgement_proposition_get(judgement, source->proposition_id)->subject,
				substitution_id,
				&subject
			) != 0 || prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				prototype_judgement_proposition_get(judgement, source->proposition_id)->classifier,
				substitution_id,
				&classifier
			) != 0))) {
		return -1;
	}
	if (require_projection) {
		subject = prototype_judgement_proposition_get(judgement, source->proposition_id)->subject;
		classifier = prototype_judgement_proposition_get(judgement, source->proposition_id)->classifier;
	}
	struct prototype_judgement_proposition relation = {
		.kind = prototype_judgement_proposition_get(judgement, source->proposition_id)->kind,
		.authority_kind = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID :
			prototype_judgement_proposition_get(
				judgement, source->proposition_id
			)->authority_kind,
		.authority_id = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
			PROTOTYPE_INVALID_ID : prototype_judgement_proposition_get(
				judgement, source->proposition_id
			)->authority_id,
		.context_id = substitution->source_context,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = relation.context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = relation.subject;
	proof.conclusion_classifier = relation.classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.semantic_action_kind =
		PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
	proof.semantic_action_id = substitution_id;
	proof.premise_count = 1;
	proof.premises[0].proposition.kind = prototype_judgement_proposition_get(judgement, source->proposition_id)->kind;
	proof.premises[0].proposition.context_id = prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id;
	proof.premises[0].proposition.subject = prototype_judgement_proposition_get(judgement, source->proposition_id)->subject;
	proof.premises[0].proposition.classifier = prototype_judgement_proposition_get(judgement, source->proposition_id)->classifier;
	proof.premises[0].proposition.authority_kind = prototype_judgement_proposition_get(judgement, source->proposition_id)->authority_kind;
	proof.premises[0].proposition.authority_id = prototype_judgement_proposition_get(judgement, source->proposition_id)->authority_id;
	proof.premises[0].proposition.operation_id = prototype_judgement_proposition_get(judgement, source->proposition_id)->operation_id;
	uint32_t premise_claim_ids[1] = { source_claim_id };
	return publish_complete_relation(
		judgement, &relation, &proof, premise_claim_ids, p_claim_id
	);
}

int prototype_judgement_add_context_weakened_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t source_claim_id,
	uint32_t target_context_id,
	uint32_t projection_substitution_id,
	uint32_t* p_claim_id
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, projection_substitution_id);
	if (!substitution || substitution->source_context != target_context_id) {
		return -1;
	}
	return add_claim_reindexed_by_substitution(
		judgement,
		NULL,
		NULL,
		contexts,
		(struct prototype_substitution_db*)substitutions,
		source_claim_id,
		projection_substitution_id,
		PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN,
		1,
		p_claim_id
	);
}

int prototype_judgement_add_reindexed_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_claim_id,
	uint32_t substitution_id,
	uint32_t* p_claim_id
) {
	return add_claim_reindexed_by_substitution(
		judgement,
		terms,
		type_declarations,
		contexts,
		substitutions,
		source_claim_id,
		substitution_id,
		PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX,
		0,
		p_claim_id
	);
}

int prototype_judgement_add_context_binding_assumption(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t* p_claim_id
) {
	uint32_t subject;
	uint32_t entry_context_id;
	if (!judgement || !terms || !contexts || !p_claim_id ||
		classifier >= terms->term_count ||
		prototype_context_find_binding(
			contexts, context_id, binding_id, &entry_context_id
		) != 0 ||
		prototype_context_classifier_term(
			prototype_context_get(contexts, entry_context_id)
		) != classifier ||
		prototype_term_var(terms, binding_id, &subject) != 0) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = subject;
	proof.conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&proof, proof_premises);
	return publish_complete_relation(
		judgement, &relation, &proof, NULL, p_claim_id
	);
}

int prototype_judgement_add_relation_type_formation(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t relation_type,
	uint32_t universe,
	uint32_t left_type_claim_id,
	uint32_t right_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_claim_id
) {
	if (!judgement || !terms || !p_claim_id) {
		return -1;
	}
	uint32_t left_classifier;
	uint32_t right_classifier;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_classifier,
			&right_classifier,
			&left_endpoint,
			&right_endpoint
		) != 0) {
		return -1;
	}
	uint32_t premise_ids[4] = {
		left_type_claim_id,
		right_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id
	};
	const struct prototype_judgement_claim* premise_claims[4];
	for (uint32_t i = 0; i < 4; ++i) {
		premise_claims[i] = prototype_judgement_claim_get(
			judgement, premise_ids[i]
		);
		if (!premise_claims[i] ||
			prototype_judgement_proposition_get(judgement, premise_claims[i]->proposition_id)->context_id != context_id) {
			return -1;
		}
	}
	if (prototype_judgement_proposition_get(judgement, premise_claims[0]->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, premise_claims[0]->proposition_id)->subject != left_classifier ||
		prototype_judgement_proposition_get(judgement, premise_claims[0]->proposition_id)->classifier != universe ||
		prototype_judgement_proposition_get(judgement, premise_claims[1]->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, premise_claims[1]->proposition_id)->subject != right_classifier ||
		prototype_judgement_proposition_get(judgement, premise_claims[1]->proposition_id)->classifier != universe ||
		prototype_judgement_proposition_get(judgement, premise_claims[2]->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, premise_claims[2]->proposition_id)->subject != left_endpoint ||
		prototype_judgement_proposition_get(judgement, premise_claims[2]->proposition_id)->classifier != left_classifier ||
		prototype_judgement_proposition_get(judgement, premise_claims[3]->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, premise_claims[3]->proposition_id)->subject != right_endpoint ||
		prototype_judgement_proposition_get(judgement, premise_claims[3]->proposition_id)->classifier != right_classifier) {
		return -1;
	}
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = relation_type,
		.classifier = universe
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = relation_type;
	proof.conclusion_classifier = universe;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 4;
	for (uint32_t i = 0; i < 4; ++i) {
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise_claims[i], &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &relation, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_witness_intro(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	struct prototype_term_conversion_result comparison;
	if (!judgement || !terms || !type_declarations || !p_claim_id ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left ||
		witness_right != relation_right || !relation || !left || !right ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != right_type ||
		prototype_term_compare_for_conversion(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			relation_left,
			relation_right,
			64,
			&comparison
		) != 0 || comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RELATION_WITNESS_INTRO;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	uint32_t premise_ids[3] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id
	};
	const struct prototype_judgement_claim* premises[3] = {
		relation,
		left,
		right
	};
	proof.premise_count = 3;
	for (uint32_t i = 0; i < 3; ++i) {
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premises[i], &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_constructor_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	const uint32_t* field_witness_claim_ids,
	uint32_t field_witness_count,
	uint32_t* p_claim_id
) {
	if (!judgement || !terms || !p_claim_id ||
		field_witness_count + 3 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		(field_witness_count != 0 && !field_witness_claim_ids)) {
		return -1;
	}
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t relation_left_type;
	uint32_t relation_right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t left_head;
	uint32_t left_owner;
	uint32_t left_constructor;
	uint32_t left_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t left_argument_count;
	uint32_t right_head;
	uint32_t right_owner;
	uint32_t right_constructor;
	uint32_t right_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t right_argument_count;
	if (!relation || !left || !right ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&relation_left_type,
			&relation_right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != relation_left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != relation_right_type ||
		prototype_term_constructor_spine_info(
			terms,
			relation_left,
			&left_head,
			&left_owner,
			&left_constructor,
			left_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&left_argument_count
		) != 0 || prototype_term_constructor_spine_info(
			terms,
			relation_right,
			&right_head,
			&right_owner,
			&right_constructor,
			right_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&right_argument_count
		) != 0 || left_owner != right_owner ||
		left_constructor != right_constructor ||
		left_argument_count != field_witness_count ||
		right_argument_count != field_witness_count) {
		return -1;
	}
	uint32_t premise_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	premise_ids[0] = relation_type_claim_id;
	premise_ids[1] = left_endpoint_claim_id;
	premise_ids[2] = right_endpoint_claim_id;
	for (uint32_t i = 0; i < field_witness_count; ++i) {
		const struct prototype_judgement_claim* field =
			prototype_judgement_claim_get(
				judgement, field_witness_claim_ids[i]
			);
		uint32_t field_left_type;
		uint32_t field_right_type;
		uint32_t field_left;
		uint32_t field_right;
		if (!field || prototype_judgement_proposition_get(judgement, field->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			prototype_judgement_proposition_get(judgement, field->proposition_id)->context_id != context_id ||
			prototype_term_relation_type_info(
				terms,
				prototype_judgement_proposition_get(judgement, field->proposition_id)->classifier,
				&field_left_type,
				&field_right_type,
				&field_left,
				&field_right
			) != 0 || field_left != left_arguments[i] ||
			field_right != right_arguments[i]) {
			return -1;
		}
		premise_ids[i + 3] = field_witness_claim_ids[i];
	}
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = field_witness_count + 3;
	for (uint32_t i = 0; i < proof.premise_count; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	(void)left_head;
	(void)right_head;
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_unary_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t child_witness_claim_id,
	int proof_kind,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* child =
		prototype_judgement_claim_get(judgement, child_witness_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t relation_left_type;
	uint32_t relation_right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t child_left_type;
	uint32_t child_right_type;
	uint32_t child_left;
	uint32_t child_right;
	uint32_t left_payload;
	uint32_t right_payload;
	int endpoint_tag;
	if (proof_kind == PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS) {
		endpoint_tag = PROTOTYPE_TERM_RETURN;
	} else if (proof_kind ==
		PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS) {
		endpoint_tag = PROTOTYPE_TERM_THUNK;
	} else {
		return -1;
	}
	if (!judgement || !terms || !p_claim_id || !relation || !left || !right ||
		!child ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&relation_left_type,
			&relation_right_type,
			&relation_left,
			&relation_right
		) != 0 || relation_left >= terms->term_count ||
		relation_right >= terms->term_count || witness_left != relation_left ||
		witness_right != relation_right || terms->terms[relation_left].tag !=
			endpoint_tag || terms->terms[relation_right].tag != endpoint_tag ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != relation_left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != relation_right_type ||
		prototype_judgement_proposition_get(judgement, child->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, child->proposition_id)->context_id != context_id || prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, child->proposition_id)->classifier,
			&child_left_type,
			&child_right_type,
			&child_left,
			&child_right
		) != 0) {
		return -1;
	}
	left_payload = endpoint_tag == PROTOTYPE_TERM_RETURN ?
		terms->terms[relation_left].as.return_term.value :
		terms->terms[relation_left].as.thunk.computation;
	right_payload = endpoint_tag == PROTOTYPE_TERM_RETURN ?
		terms->terms[relation_right].as.return_term.value :
		terms->terms[relation_right].as.thunk.computation;
	if (child_left != left_payload || child_right != right_payload) {
		return -1;
	}
	uint32_t premise_ids[4] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		child_witness_claim_id
	};
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 4;
	for (uint32_t i = 0; i < 4; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_app_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t function_witness_claim_id,
	uint32_t argument_witness_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* function_witness =
		prototype_judgement_claim_get(judgement, function_witness_claim_id);
	const struct prototype_judgement_claim* argument_witness =
		prototype_judgement_claim_get(judgement, argument_witness_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t function_left_type;
	uint32_t function_right_type;
	uint32_t function_left;
	uint32_t function_right;
	uint32_t argument_left_type;
	uint32_t argument_right_type;
	uint32_t argument_left;
	uint32_t argument_right;
	if (!judgement || !terms || !p_claim_id || !relation || !left || !right ||
		!function_witness || !argument_witness ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_APP ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_APP ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != right_type ||
		prototype_judgement_proposition_get(judgement, function_witness->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, function_witness->proposition_id)->context_id != context_id ||
		prototype_judgement_proposition_get(judgement, argument_witness->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, argument_witness->proposition_id)->context_id != context_id ||
		prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, function_witness->proposition_id)->classifier,
			&function_left_type,
			&function_right_type,
			&function_left,
			&function_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, argument_witness->proposition_id)->classifier,
			&argument_left_type,
			&argument_right_type,
			&argument_left,
			&argument_right
		) != 0 ||
		function_left != terms->terms[relation_left].as.app.function ||
		function_right != terms->terms[relation_right].as.app.function ||
		argument_left != terms->terms[relation_left].as.app.argument ||
		argument_right != terms->terms[relation_right].as.app.argument) {
		return -1;
	}
	uint32_t premise_ids[5] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		function_witness_claim_id,
		argument_witness_claim_id
	};
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 5;
	for (uint32_t i = 0; i < 5; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

static int relation_lambda_witness_shape_valid(
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t body_context_id,
	uint32_t body_witness_classifier
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t body_left_type;
	uint32_t body_right_type;
	uint32_t body_left;
	uint32_t body_right;
	if (!terms || !contexts ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		left_type >= terms->term_count || right_type >= terms->term_count ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[left_type].tag != PROTOTYPE_TERM_PI ||
		terms->terms[right_type].tag != PROTOTYPE_TERM_PI ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_LAMBDA ||
		prototype_term_relation_type_info(
			terms,
			body_witness_classifier,
			&body_left_type,
			&body_right_type,
			&body_left,
			&body_right
		) != 0) {
		return 0;
	}
	const struct prototype_context* relation_context =
		prototype_context_get(contexts, body_context_id);
	const struct prototype_context* right_context = relation_context ?
		prototype_context_get(contexts, relation_context->parent) : NULL;
	const struct prototype_context* left_context = right_context ?
		prototype_context_get(contexts, right_context->parent) : NULL;
	if (!relation_context || !right_context || !left_context ||
		left_context->parent != context_id ||
		prototype_context_classifier_term(left_context) !=
			terms->terms[left_type].as.pi.domain ||
		prototype_context_classifier_term(right_context) !=
			terms->terms[right_type].as.pi.domain) {
		return 0;
	}
	uint32_t left_argument;
	uint32_t right_argument;
	uint32_t expected_input_relation;
	uint32_t left_lambda;
	uint32_t right_lambda;
	int left_matches;
	int right_matches;
	if (prototype_term_var(
			terms, left_context->binding_id, &left_argument
		) != 0 || prototype_term_var(
			terms, right_context->binding_id, &right_argument
		) != 0 || prototype_term_relation_type(
			terms,
			terms->terms[left_type].as.pi.domain,
			terms->terms[right_type].as.pi.domain,
			left_argument,
			right_argument,
			&expected_input_relation
		) != 0 || prototype_context_classifier_term(relation_context) !=
			expected_input_relation || prototype_term_lambda(
			terms, left_context->binding_id, body_left, &left_lambda
		) != 0 || prototype_term_lambda(
			terms, right_context->binding_id, body_right, &right_lambda
		) != 0 || prototype_term_core_shape_equal(
			terms, left_lambda, relation_left, &left_matches
		) != 0 || prototype_term_core_shape_equal(
			terms, right_lambda, relation_right, &right_matches
		) != 0) {
		return 0;
	}
	(void)body_left_type;
	(void)body_right_type;
	return left_matches && right_matches;
}

int prototype_judgement_add_relation_lambda_witness(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t body_witness_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* body =
		prototype_judgement_claim_get(judgement, body_witness_claim_id);
	uint32_t relation_left_type;
	uint32_t relation_right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	if (!judgement || !terms || !contexts || !p_claim_id || !relation || !left ||
		!right || !body || prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_term_relation_type_info(
			terms,
			relation_type,
			&relation_left_type,
			&relation_right_type,
			&relation_left,
			&relation_right
		) != 0 || prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != relation_left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != relation_right_type ||
		prototype_judgement_proposition_get(judgement, body->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!relation_lambda_witness_shape_valid(
			terms,
			contexts,
			context_id,
			witness,
			relation_type,
			prototype_judgement_proposition_get(judgement, body->proposition_id)->context_id,
			prototype_judgement_proposition_get(judgement, body->proposition_id)->classifier
		)) {
		return -1;
	}
	uint32_t premise_ids[4] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		body_witness_claim_id
	};
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 4;
	for (uint32_t i = 0; i < 4; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

static int relation_match_case_witness_shape_valid(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t outer_context_id,
	const struct prototype_match_case* source_case,
	const struct prototype_match_case* left_case,
	const struct prototype_match_case* right_case,
	const struct prototype_judgement_proposition* case_witness
) {
	if (!terms || !type_declarations || !contexts || !source_case || !left_case ||
		!right_case || !case_witness ||
		source_case->binder_count != left_case->binder_count ||
		left_case->binder_count != right_case->binder_count ||
		source_case->constructor_id != left_case->constructor_id ||
		left_case->constructor_id != right_case->constructor_id) {
		return 0;
	}
	uint32_t binder_count = left_case->binder_count;
	if (binder_count > 64) {
		return 0;
	}
	struct prototype_case_binder left_targets[64];
	struct prototype_case_binder right_targets[64];
	uint32_t current_context = case_witness->context_id;
	for (uint32_t i = binder_count; i > 0; --i) {
		const struct prototype_case_binder* source_binder =
			&terms->case_binders[source_case->first_binder + i - 1];
		const struct prototype_case_binder* left_binder =
			&terms->case_binders[left_case->first_binder + i - 1];
		const struct prototype_case_binder* right_binder =
			&terms->case_binders[right_case->first_binder + i - 1];
		const struct prototype_context* relation_context =
			prototype_context_get(contexts, current_context);
		const struct prototype_context* right_context = relation_context ?
			prototype_context_get(contexts, relation_context->parent) : NULL;
		const struct prototype_context* left_context = right_context ?
			prototype_context_get(contexts, right_context->parent) : NULL;
		if (!relation_context || !right_context || !left_context ||
			source_binder->is_recursive != left_binder->is_recursive ||
			left_binder->is_recursive != right_binder->is_recursive) {
			return 0;
		}
		uint32_t left_variable;
		uint32_t right_variable;
		uint32_t relation_type;
		if (prototype_term_var(
				terms, left_context->binding_id, &left_variable
			) != 0 || prototype_term_var(
				terms, right_context->binding_id, &right_variable
			) != 0 || prototype_term_relation_type(
				terms,
				prototype_context_classifier_term(left_context),
				prototype_context_classifier_term(right_context),
				left_variable,
				right_variable,
				&relation_type
			) != 0 || prototype_context_classifier_term(relation_context) !=
				relation_type) {
			return 0;
		}
		left_targets[i - 1] = (struct prototype_case_binder){
			.binding_id = left_context->binding_id,
			.is_recursive = left_binder->is_recursive
		};
		right_targets[i - 1] = (struct prototype_case_binder){
			.binding_id = right_context->binding_id,
			.is_recursive = right_binder->is_recursive
		};
		current_context = left_context->parent;
	}
	if (current_context != outer_context_id) {
		return 0;
	}
	uint32_t expected_left;
	uint32_t expected_right;
	if (core_match_case_alpha_reindex(
			terms,
			type_declarations,
			&terms->case_binders[source_case->first_binder],
			left_targets,
			binder_count,
			source_case->body,
			&expected_left
		) != 0 || core_match_case_alpha_reindex(
			terms,
			type_declarations,
			&terms->case_binders[source_case->first_binder],
			right_targets,
			binder_count,
			source_case->body,
			&expected_right
		) != 0) {
		return 0;
	}
	uint32_t child_left_type;
	uint32_t child_right_type;
	uint32_t child_left;
	uint32_t child_right;
	int left_equal;
	int right_equal;
	return case_witness->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		prototype_term_relation_type_info(
			terms,
			case_witness->classifier,
			&child_left_type,
			&child_right_type,
			&child_left,
			&child_right
		) == 0 && prototype_term_core_shape_equal(
			terms, child_left, expected_left, &left_equal
		) == 0 && left_equal && prototype_term_core_shape_equal(
			terms, child_right, expected_right, &right_equal
		) == 0 && right_equal;
}

int prototype_judgement_add_relation_match_witness(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t source_match_claim_id,
	uint32_t scrutinee_witness_claim_id,
	const uint32_t* case_witness_claim_ids,
	uint32_t case_witness_count,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* source_match =
		prototype_judgement_claim_get(judgement, source_match_claim_id);
	const struct prototype_judgement_claim* scrutinee =
		prototype_judgement_claim_get(judgement, scrutinee_witness_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t scrutinee_left_type;
	uint32_t scrutinee_right_type;
	uint32_t scrutinee_left;
	uint32_t scrutinee_right;
	if (!judgement || !terms || !type_declarations || !contexts ||
		!case_witness_claim_ids || !p_claim_id ||
		!relation || !left || !right || !source_match || !scrutinee ||
		case_witness_count + 5 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation_left].as.match.case_count != case_witness_count ||
		terms->terms[relation_right].as.match.case_count != case_witness_count ||
		prototype_judgement_proposition_get(judgement, source_match->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, source_match->proposition_id)->subject >= terms->term_count ||
		terms->terms[prototype_judgement_proposition_get(judgement, source_match->proposition_id)->subject].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[prototype_judgement_proposition_get(judgement, source_match->proposition_id)->subject].as.match.case_count != case_witness_count ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != right_type ||
		prototype_judgement_proposition_get(judgement, scrutinee->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, scrutinee->proposition_id)->context_id != context_id ||
		prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, scrutinee->proposition_id)->classifier,
			&scrutinee_left_type,
			&scrutinee_right_type,
			&scrutinee_left,
			&scrutinee_right
		) != 0 || scrutinee_left != terms->terms[relation_left].as.match.scrutinee ||
		scrutinee_right != terms->terms[relation_right].as.match.scrutinee) {
		return -1;
	}
	uint32_t premise_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	premise_ids[0] = relation_type_claim_id;
	premise_ids[1] = left_endpoint_claim_id;
	premise_ids[2] = right_endpoint_claim_id;
	premise_ids[3] = source_match_claim_id;
	premise_ids[4] = scrutinee_witness_claim_id;
	for (uint32_t i = 0; i < case_witness_count; ++i) {
		uint32_t source_case_id =
			terms->terms[prototype_judgement_proposition_get(judgement, source_match->proposition_id)->subject].as.match.first_case + i;
		uint32_t left_case_id = terms->terms[relation_left].as.match.first_case + i;
		uint32_t right_case_id = terms->terms[relation_right].as.match.first_case + i;
		const struct prototype_judgement_claim* case_witness =
			prototype_judgement_claim_get(judgement, case_witness_claim_ids[i]);
		const struct prototype_judgement_proposition* case_proposition =
			prototype_judgement_claim_proposition(
				judgement, case_witness_claim_ids[i]
			);
		uint32_t case_left_type;
		uint32_t case_right_type;
		uint32_t case_left;
		uint32_t case_right;
		if (source_case_id >= terms->case_count || left_case_id >= terms->case_count ||
			right_case_id >= terms->case_count ||
			!case_witness || !case_proposition ||
			!relation_match_case_witness_shape_valid(
				terms,
				type_declarations,
				contexts,
				context_id,
				&terms->cases[source_case_id],
				&terms->cases[left_case_id],
				&terms->cases[right_case_id],
				case_proposition
			)) {
			return -1;
		}
		(void)case_left_type;
		(void)case_right_type;
		(void)case_left;
		(void)case_right;
		premise_ids[i + 5] = case_witness_claim_ids[i];
	}
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = case_witness_count + 5;
	for (uint32_t i = 0; i < proof.premise_count; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_relation_induction_hypothesis_witness(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t witness,
	uint32_t relation_type,
	uint32_t relation_type_claim_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t source_induction_claim_id,
	uint32_t argument_witness_claim_id,
	uint32_t* p_claim_id
) {
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, relation_type_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, left_endpoint_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, right_endpoint_claim_id);
	const struct prototype_judgement_claim* source =
		prototype_judgement_claim_get(judgement, source_induction_claim_id);
	const struct prototype_judgement_claim* argument =
		prototype_judgement_claim_get(judgement, argument_witness_claim_id);
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t argument_left_type;
	uint32_t argument_right_type;
	uint32_t argument_left;
	uint32_t argument_right;
	int source_has_induction_derivation = 0;
	if (judgement) {
		for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
			if (judgement->derivations[i].conclusion_claim_id ==
					source_induction_claim_id && judgement->derivations[i].proof_kind ==
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
				source_has_induction_derivation = 1;
				break;
			}
		}
	}
	if (!judgement || !terms || !p_claim_id || !relation || !left || !right ||
		!source || !argument || !source_has_induction_derivation ||
		prototype_term_relation_witness_info(
			terms, witness, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation_type,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject != relation_type ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, left->proposition_id)->subject != relation_left ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier != left_type ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, right->proposition_id)->subject != relation_right ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier != right_type ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->subject >= terms->term_count || terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		prototype_judgement_proposition_get(judgement, argument->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, argument->proposition_id)->context_id != context_id || prototype_term_relation_type_info(
			terms,
			prototype_judgement_proposition_get(judgement, argument->proposition_id)->classifier,
			&argument_left_type,
			&argument_right_type,
			&argument_left,
			&argument_right
		) != 0 || argument_left != terms->terms[relation_left].
			as.induction_hypothesis.argument || argument_right !=
			terms->terms[relation_right].as.induction_hypothesis.argument) {
		return -1;
	}
	uint32_t premise_ids[5] = {
		relation_type_claim_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		source_induction_claim_id,
		argument_witness_claim_id
	};
	struct prototype_judgement_proposition result = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = witness,
		.classifier = relation_type
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind =
		PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS;
	proof.conclusion_kind = result.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	proof.conclusion_subject = witness;
	proof.conclusion_classifier = relation_type;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 5;
	for (uint32_t i = 0; i < 5; ++i) {
		const struct prototype_judgement_claim* premise =
			prototype_judgement_claim_get(judgement, premise_ids[i]);
		struct prototype_judgement_selected_evidence evidence;
		selected_evidence_from_accepted_claim(judgement, premise, &evidence);
		proof.premises[i].proposition.kind = evidence.kind;
		proof.premises[i].proposition.context_id = evidence.context_id;
		proof.premises[i].proposition.subject = evidence.subject;
		proof.premises[i].proposition.classifier = evidence.classifier;
		proof.premises[i].proposition.authority_kind = evidence.authority_kind;
		proof.premises[i].proposition.authority_id = evidence.authority_id;
		proof.premises[i].proposition.operation_id = evidence.operation_id;
	}
	return publish_complete_relation(
		judgement, &result, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_delta_record_effect_weaken(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t target_classifier
) {
	if (!delta || !terms || !type_declarations || !source_evidence ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		source_evidence->subject >= terms->term_count ||
		source_evidence->classifier >= terms->term_count ||
		target_classifier >= terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view source;
	struct prototype_term_classifier_view target;
	unsigned source_effects;
	unsigned target_effects;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, source_evidence->classifier, &source
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, target_classifier, &target
		) != 0 || source.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		target.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		target.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, source.result, target.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_effect_row_closed_bits(
			terms, source.effect_row, &source_effects
		) != 0 || prototype_term_effect_row_closed_bits(
			terms, target.effect_row, &target_effects
		) != 0 || (source_effects & target_effects) != source_effects) {
		return -1;
	}
	uint32_t premise_subjects[1] = { source_evidence->subject };
	uint32_t premise_classifiers[1] = { source_evidence->classifier };
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		source_evidence->subject,
		target_classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		source_evidence,
		1
	);
}

int prototype_judgement_delta_record_type_formation(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms, subject, &type_id, arguments, &argument_count
		) != 0 || type_id >= type_declarations->type_count ||
		argument_count > 16) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (argument_count == 0 &&
		type->formation_classifier != PROTOTYPE_INVALID_ID) {
		if (type->formation_classifier == classifier) {
			return add_delta_relation(
				delta,
				PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
				subject,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				classifier,
				PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
			);
		}
		if (prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			type->formation_classifier,
			classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL || add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			subject,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			type->formation_classifier,
			PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
		) != 0) {
			return -1;
		}
		struct prototype_judgement_selected_evidence source_evidence = {
			.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
			.authority_id = subject,
			.context_id = delta->current_context_id,
			.operation_id = PROTOTYPE_INVALID_ID,
			.subject = subject,
			.classifier = type->formation_classifier
		};
		return prototype_judgement_delta_add_conversion(
			delta, terms, type_declarations, &source_evidence, classifier
		);
	}
	if (!type_instance_has_known_type(terms, type_declarations, subject) ||
		!term_is_universe_var(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_FORMATION,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
	);
}

int prototype_judgement_delta_record_pure_primitive_type(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_PURE_PRIMITIVE) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	uint32_t inferred_classifier;
	if (prototype_judgement_pure_primitive_classifier(
			terms,
			type_declarations,
			&terms->terms[subject],
			&inferred_classifier
		) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			inferred_classifier,
			classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO
	);
}

int prototype_judgement_delta_record_effect_operation_type(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	(void)type_declarations;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_EFFECT_OPERATION) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	uint32_t inferred_classifier;
	if (prototype_judgement_effect_operation_classifier(
			terms, &terms->terms[subject], &inferred_classifier
		) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, inferred_classifier, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_INTRINSIC,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO
	);
}

int prototype_judgement_delta_record_text_literal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t text;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_TEXT_LITERAL) ||
		!term_exists(terms, classifier) ||
		prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0 ||
		text != classifier) {
		return -1;
	}
	return add_delta_relation(
		delta,
		delta->current_operation_id == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER :
			PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		delta->current_operation_id == PROTOTYPE_INVALID_ID ?
			subject : delta->current_operation_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO
	);
}

int prototype_judgement_delta_record_int_literal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t integer;
	uint32_t integer64;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_exists(terms, classifier) ||
		prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT64, &integer64) != 0) {
		return -1;
	}
	if (classifier != integer64) {
		if (terms->terms[subject].as.int_literal.value < INT32_MIN ||
			terms->terms[subject].as.int_literal.value > INT32_MAX ||
			prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT32, &integer) != 0 ||
			classifier != integer) {
			return -1;
		}
	}
	return add_delta_relation(
		delta,
		delta->current_operation_id == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER :
			PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION,
		delta->current_operation_id == PROTOTYPE_INVALID_ID ?
			subject : delta->current_operation_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO
	);
}

int prototype_judgement_delta_record_int_literal_admissibility(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t admissible_classifier
) {
	uint32_t integer;
	uint32_t integer64;
	if (!source_evidence) {
		return -1;
	}
	if (source_evidence->classifier == admissible_classifier) {
		return 0;
	}
	if (!delta || !terms ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		!term_has_tag(terms, source_evidence->subject, PROTOTYPE_TERM_INT_LITERAL) ||
		source_evidence->classifier >= terms->term_count ||
		prototype_term_make_host_type(
			terms, PROTOTYPE_HOST_TYPE_INT64, &integer64
		) != 0 || prototype_term_make_host_type(
			terms, PROTOTYPE_HOST_TYPE_INT32, &integer
		) != 0 || (source_evidence->classifier != integer64 &&
			source_evidence->classifier != integer) ||
		admissible_classifier != integer || !int_literal_fits_int32(
			terms->terms[source_evidence->subject].as.int_literal.value
		)) {
		return -1;
	}
	uint32_t premise_subject = source_evidence->subject;
	uint32_t premise_classifier = source_evidence->classifier;
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		source_evidence->subject,
		admissible_classifier,
		PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY,
		NULL,
		NULL,
		&premise_subject,
		&premise_classifier,
		source_evidence,
		1
	);
}

int prototype_judgement_record_declaration_fact(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_exists(terms, subject) || !term_exists(terms, classifier)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_DECLARATION);
}

int prototype_judgement_add_expected_type_exposure(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t operation_id,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected,
	uint32_t subject
) {
	if (!judgement || !type_declarations || !source_evidence ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		source_evidence->subject != subject || !term_exists(terms, subject) ||
		!term_exists(terms, expected) ||
		!term_exists(terms, source_evidence->classifier)) {
		return -1;
	}
	if (operation_id == source_evidence->operation_id &&
		context_id == source_evidence->context_id &&
		expected == source_evidence->classifier) {
		return 0;
	}
	struct prototype_term_conversion_result conversion =
		classifier_kernel_conversion(
			terms, type_declarations, NULL, expected, source_evidence->classifier
		);
	int proof_kind;
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONVERSION;
	} else if (conversion.status == PROTOTYPE_TERM_CONVERSION_NOT_EQUAL &&
		classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			NULL,
			expected,
			source_evidence->classifier,
			0
		)) {
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE;
	} else {
		return -1;
	}
	uint32_t premise_subjects[1] = { subject };
	uint32_t premise_classifiers[1] = { source_evidence->classifier };
	struct prototype_judgement_proposition relation = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = operation_id,
		.subject = subject,
		.classifier = expected
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise
		proof_premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&proof, 0, sizeof(proof));
	proof.proof_kind = proof_kind;
	proof.conclusion_kind = relation.kind;
	proof.conclusion_context_id = context_id;
	proof.conclusion_operation_id = operation_id;
	proof.conclusion_subject = subject;
	proof.conclusion_classifier = expected;
	initialize_proof_rule_parameters(&proof, proof_premises);
	proof.premise_count = 1;
	proof.premises[0].proposition.kind = source_evidence->kind;
	proof.premises[0].proposition.context_id = source_evidence->context_id;
	proof.premises[0].proposition.subject = premise_subjects[0];
	proof.premises[0].proposition.classifier = premise_classifiers[0];
	proof.premises[0].proposition.authority_kind = source_evidence->authority_kind;
	proof.premises[0].proposition.authority_id = source_evidence->authority_id;
	proof.premises[0].proposition.operation_id = source_evidence->operation_id;
	return add_complete_relation(judgement, &relation, &proof);
}

int prototype_judgement_delta_record_expected_type_exposure(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected,
	uint32_t subject
) {
	if (!delta || !type_declarations || !source_evidence ||
		source_evidence->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_evidence->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		source_evidence->subject != subject || !term_exists(terms, subject) ||
		!term_exists(terms, expected) ||
		!term_exists(terms, source_evidence->classifier)) {
		return -1;
	}
	if (delta->current_operation_id == source_evidence->operation_id &&
		expected == source_evidence->classifier) {
		return 0;
	}
	struct prototype_term_conversion_result conversion =
		classifier_kernel_conversion(
			terms, type_declarations, NULL, expected, source_evidence->classifier
		);
	int proof_kind;
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_CONVERSION;
	} else if (conversion.status == PROTOTYPE_TERM_CONVERSION_NOT_EQUAL &&
		classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			NULL,
			expected,
			source_evidence->classifier,
			0
		)) {
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE;
	} else {
		return -1;
	}
	uint32_t premise_subjects[1] = { subject };
	uint32_t premise_classifiers[1] = { source_evidence->classifier };
	return add_delta_relation_with_explicit_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		expected,
		proof_kind,
		NULL,
		NULL,
		premise_subjects,
		premise_classifiers,
		source_evidence,
		1
	);
}

int prototype_judgement_delta_record_declaration_fact(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_exists(terms, subject) || !term_exists(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION,
		subject,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_DECLARATION
	);
}

int prototype_judgement_delta_has_pending_classifier_state(
	const struct prototype_judgement_delta* delta
) {
	if (!delta) {
		return -1;
	}
	return delta->match_motive_result_count > 0 ? 1 : 0;
}

static int judgement_candidate_claim_has_derivation(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int include_conversion
) {
	if (!judgement) {
		return -1;
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		if (relation->kind == kind &&
			relation->context_id == context_id &&
			relation->subject == subject &&
			relation->classifier == classifier) {
			uint32_t cursor = 0;
			uint32_t derivation_id;
			while (prototype_judgement_candidate_derivation_next(
				judgement->propositions,
				judgement->proposition_count,
				judgement->derivation_candidates,
				judgement->derivation_candidate_count,
				(uint32_t)i,
				&cursor,
				&derivation_id
			) == 0) {
				if (!include_conversion &&
					judgement->derivation_candidates[derivation_id].proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
					continue;
				}
				return 0;
			}
		}
	}
	return -1;
}

static void initialize_proof_premises(
	struct prototype_judgement_derivation_candidate* proof,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
) {
	proof->premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		proof->premises[i].proposition.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof->premises[i].proposition.context_id = premise_context_ids ?
			premise_context_ids[i] : proof->conclusion_context_id;
		proof->premises[i].proposition.subject = premise_subjects[i];
		proof->premises[i].proposition.classifier = premise_classifiers[i];
		proof->premises[i].proposition.authority_kind =
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID;
		proof->premises[i].proposition.authority_id = PROTOTYPE_INVALID_ID;
		proof->premises[i].proposition.operation_id = PROTOTYPE_INVALID_ID;
	}
}

static void initialize_proof_rule_parameters(
	struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* premises
) {
	if (!proof || !premises) {
		return;
	}
	memset(
		premises,
		0,
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES * sizeof(*premises)
	);
	proof->premises = premises;
	memset(&proof->rule_data, 0xff, sizeof(proof->rule_data));
	proof->semantic_action_kind = PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
	proof->semantic_action_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES; ++i) {
		proof->premises[i].proposition.authority_kind =
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID;
		proof->premises[i].proposition.authority_id = PROTOTYPE_INVALID_ID;
		proof->premises[i].proposition.operation_id = PROTOTYPE_INVALID_ID;
		proof->premises[i].semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		proof->premises[i].semantic_action_id = PROTOTYPE_INVALID_ID;
	}
}

static int judgement_operation_candidate_claim_has_derivation(
	const struct prototype_judgement_db* judgement,
	uint32_t operation_id,
	uint32_t context_id,
	int kind,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement) {
		return -1;
	}
	for (size_t i = judgement->proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation =
			&judgement->propositions[i - 1];
		if (relation->operation_id == operation_id &&
			relation->context_id == context_id && relation->kind == kind &&
			relation->subject == subject && relation->classifier == classifier) {
			uint32_t cursor = 0;
			uint32_t derivation_id;
			if (prototype_judgement_candidate_derivation_next(
				judgement->propositions,
				judgement->proposition_count,
				judgement->derivation_candidates,
				judgement->derivation_candidate_count,
				(uint32_t)(i - 1),
				&cursor,
				&derivation_id
			) == 0) {
				return 0;
			}
		}
	}
	return 1;
}

/* Returns zero when this structural premise has an authoritative Operation
 * owner, one when the rule still uses non-structural evidence, and -1 for an
 * Operation/rule shape mismatch. INVALID is a deliberate owner for binder or
 * compiler-generated Core facts. */
static uint32_t judgement_operation_evidence_owner(
	const struct prototype_operation_graph* operations,
	uint32_t operation_id,
	uint32_t depth
) {
	if (!operations || operation_id >= operations->operation_count || depth > 64) {
		return operation_id;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[operation_id];
	if (operation->tag == PROTOTYPE_OPERATION_VAR ||
		operation->tag == PROTOTYPE_OPERATION_CONSTRUCTOR) {
		return PROTOTYPE_INVALID_ID;
	}
	if (operation->tag == PROTOTYPE_OPERATION_NAME) {
		return judgement_operation_evidence_owner(
			operations, operation->function, depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION &&
		operation->body < operations->operation_count &&
		operation->classifier == operations->operations[operation->body].classifier) {
		return judgement_operation_evidence_owner(
			operations, operation->body, depth + 1
		);
	}
	return operation_id;
}

static uint32_t judgement_operation_evidence_source(
	const struct prototype_operation_graph* operations,
	uint32_t operation_id,
	uint32_t depth
) {
	if (!operations || operation_id >= operations->operation_count || depth > 64) {
		return operation_id;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[operation_id];
	if (operation->tag == PROTOTYPE_OPERATION_NAME) {
		return judgement_operation_evidence_source(
			operations, operation->function, depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_OPERATION_ASCRIPTION &&
		operation->body < operations->operation_count &&
		operation->classifier == operations->operations[operation->body].classifier) {
		return judgement_operation_evidence_source(
			operations, operation->body, depth + 1
		);
	}
	return operation_id;
}

static int judgement_structural_premise_child(
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_derivation_candidate* proof,
	uint32_t premise_index,
	uint32_t* p_child_operation_id
) {
	if (!operations || !proof || !p_child_operation_id ||
		proof->conclusion_operation_id == PROTOTYPE_INVALID_ID ||
		proof->conclusion_operation_id >= operations->operation_count ||
		premise_index >= proof->premise_count) {
		return 1;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[proof->conclusion_operation_id];
	switch (proof->proof_kind) {
	case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
		if (operation->tag != PROTOTYPE_OPERATION_LAMBDA ||
			proof->premise_count != 2) {
			return -1;
		}
		*p_child_operation_id = premise_index == 0 ?
			operation->body : operation->body;
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
		if (operation->tag != PROTOTYPE_OPERATION_APP ||
			proof->premise_count != 2) {
			return -1;
		}
		*p_child_operation_id = premise_index == 0 ?
			operation->function : operation->argument;
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION: {
		if (operation->tag != PROTOTYPE_OPERATION_APP ||
			operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			return -1;
		}
		uint32_t reverse_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t argument_count = 0;
		uint32_t cursor = proof->conclusion_operation_id;
		while (cursor < operations->operation_count) {
			const struct prototype_operation_node* cursor_operation =
				&operations->operations[cursor];
			if (cursor_operation->tag != PROTOTYPE_OPERATION_APP ||
				cursor_operation->application_role !=
					PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
				break;
			}
			if (argument_count >= PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
				cursor_operation->argument >= operations->operation_count) {
				return -1;
			}
			reverse_arguments[argument_count++] = cursor_operation->argument;
			cursor = cursor_operation->function;
		}
		if (argument_count != proof->premise_count) {
			return -1;
		}
		*p_child_operation_id =
			reverse_arguments[argument_count - premise_index - 1];
		return 0;
	}
	case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
		if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
			operation->first_case + operation->case_count > operations->case_count ||
			proof->premise_count != operation->case_count) {
			return -1;
		}
		*p_child_operation_id = operations->cases[
			operation->first_case + premise_index
		].body_operation;
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
		if ((operation->tag != PROTOTYPE_OPERATION_RETURN &&
			 operation->tag != PROTOTYPE_OPERATION_THUNK &&
			 operation->tag != PROTOTYPE_OPERATION_FORCE) ||
			proof->premise_count != 1) {
			return -1;
		}
		*p_child_operation_id = operation->argument;
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
		if (operation->tag != PROTOTYPE_OPERATION_REQUEST ||
			proof->premise_count != 3) {
			return -1;
		}
		*p_child_operation_id = premise_index == 0 ? PROTOTYPE_INVALID_ID :
			(premise_index == 1 ? operation->argument : operation->body);
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
		if (operation->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			proof->premise_count != 2 + 2 * operation->fold_clause_count) {
			return -1;
		}
		if (premise_index < 2) {
			uint32_t child = premise_index == 0 ? operation->function :
				(operation->fold_clause_count == 0 ? operation->argument :
				 operation->fold_return_operation);
			if (child >= operations->operation_count) {
				return -1;
			}
			*p_child_operation_id = child;
			return 0;
		}
		if (operation->first_fold_clause > operations->fold_clause_count ||
			operation->fold_clause_count > operations->fold_clause_count -
				operation->first_fold_clause) {
			return -1;
		}
		{
			uint32_t clause_index = (premise_index - 2) / 2;
			const struct prototype_operation_computation_fold_clause* clause =
				&operations->fold_clauses[
					operation->first_fold_clause + clause_index
				];
			uint32_t child = (premise_index - 2) % 2 == 0 ?
				clause->operation_operation : clause->clause_operation;
			if (child >= operations->operation_count) {
				return -1;
			}
			*p_child_operation_id = child;
		}
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
		if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
			proof->premise_count != operation->case_count + 1 ||
			operation->first_case > operations->case_count ||
			operation->case_count > operations->case_count - operation->first_case) {
			return -1;
		}
		if (premise_index == 0) {
			/* The motive classifier is generated by the kernel. It has no
			 * source Operation occurrence. */
			*p_child_operation_id = PROTOTYPE_INVALID_ID;
			return 0;
		}
		{
			uint32_t child = operations->cases[
				operation->first_case + premise_index - 1
			].body_operation;
			if (child >= operations->operation_count) {
				return -1;
			}
			*p_child_operation_id = child;
		}
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
	case PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE:
	case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY:
	case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN:
	case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
		if (proof->premise_count != 1) {
			return -1;
		}
		/* These are derived boundaries, not syntax-directed eliminators. Their
		 * source may be a nested annotation, a Context fact, or a generated helper
		 * and is carried by the exact SelectedEvidence premise. Reconstructing it
		 * from the conclusion Operation erases that distinction. */
		return 1;
	default:
		return 1;
	}
}

static int judgement_expected_premise_operation(
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_derivation_candidate* proof,
	uint32_t premise_index,
	uint32_t* p_operation_id
) {
	if (proof && proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO &&
		premise_index == 0) {
		*p_operation_id = PROTOTYPE_INVALID_ID;
		return 0;
	}
	uint32_t child_operation_id;
	int status = judgement_structural_premise_child(
		operations, proof, premise_index, &child_operation_id
	);
	if (status != 0) {
		return status;
	}
	*p_operation_id = child_operation_id == PROTOTYPE_INVALID_ID ?
		PROTOTYPE_INVALID_ID : judgement_operation_evidence_owner(
			operations, child_operation_id, 0
		);
	return 0;
}

static int judgement_candidate_is_publishable(
	const struct prototype_judgement_db* judgement,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_proposition* candidate,
	const struct prototype_judgement_derivation_candidate* proof
);
static int judgement_candidate_has_publishable_derivation(
	const struct prototype_judgement_db* judgement,
	const struct prototype_operation_graph* operations,
	uint32_t candidate_id
);

static int judgement_rule_derives_alternate_operation_classifier(int proof_kind) {
	return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE;
}

static int judgement_premise_is_scoped_rule_parameter(
	const struct prototype_judgement_derivation_candidate* derivation,
	uint32_t premise_index
) {
	if (!derivation || premise_index >= derivation->premise_count) {
		return 0;
	}
	/* An operation handler clause is checked under the enclosing fold's
	 * specialized operation signature and carrier. Its Lambda classifier may
	 * contain a row variable scoped by that fold, so it is a replayable fold
	 * rule parameter rather than an independently publishable Claim. The fold
	 * validator replays the specialization and carrier checks. */
	if (derivation->proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM) {
		return premise_index >= 3 && (premise_index & 1u) != 0;
	}
	/* The APP(operation, argument) used by an operation request is generated
	 * inside the request rule and has no independent Operation occurrence. The
	 * request validator reconstructs and checks that application. */
	return derivation->proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO &&
		premise_index == 0;
}

static int judgement_candidate_source_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_derivation_candidate* derivation,
	uint32_t premise_index,
	uint32_t* p_claim_id
) {
	if (!judgement || !derivation || !p_claim_id ||
		premise_index >= derivation->premise_count) {
		return -1;
	}
	uint32_t expected_operation_id =
		derivation->premises[premise_index].proposition.operation_id;
	int expected_authority_kind =
		derivation->premises[premise_index].proposition.authority_kind;
	uint32_t expected_authority_id =
		derivation->premises[premise_index].proposition.authority_id;
	uint32_t expected_context_id =
		derivation->premises[premise_index].proposition.context_id;
	uint32_t structural_child_operation_id;
	/* A substitution edge is the explicit proof that moves a structural child
	 * into its branch-local Context. Reconstructing the premise from the source
	 * Operation would discard that reindexing and make indexed Match evidence
	 * impossible to publish. Accepted replay validates the substitution and its
	 * connection to the structural branch. */
	int structural_status =
		derivation->premises[premise_index].semantic_action_kind ==
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ?
		1 : judgement_structural_premise_child(
			operations, derivation, premise_index, &structural_child_operation_id
		);
	if (structural_status < 0) {
		fprintf(
			stderr,
			"P0 structural premise shape failed rule=%d conclusion_operation=%u "
			"index=%u count=%u\n",
			derivation->proof_kind,
			derivation->conclusion_operation_id,
			premise_index,
			derivation->premise_count
		);
		return -1;
	}
	if (structural_status == 0) {
		if (structural_child_operation_id == PROTOTYPE_INVALID_ID) {
			expected_operation_id = PROTOTYPE_INVALID_ID;
			if (expected_authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
				expected_authority_kind =
					PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
				expected_authority_id =
					derivation->premises[premise_index].proposition.subject;
			}
		} else {
			const struct prototype_operation_node* child =
				&operations->operations[structural_child_operation_id];
			/* Explicit SelectedEvidence may name a derived weakening Claim in
			 * the conclusion Context. Its own Derivation carries the Context
			 * substitution, so replacing that Context with the raw child
			 * occurrence would bypass the proof edge. */
			if (expected_authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
				expected_context_id = child->context_id;
			}
			uint32_t structural_operation_id =
				judgement_operation_evidence_owner(
					operations, structural_child_operation_id, 0
				);
			if (derivation->premises[premise_index].proposition.operation_id !=
					PROTOTYPE_INVALID_ID &&
				derivation->premises[premise_index].proposition.operation_id !=
					structural_child_operation_id &&
				derivation->premises[premise_index].proposition.operation_id !=
					structural_operation_id) {
				fprintf(
					stderr,
					"P0 structural operation mismatch rule=%d index=%u "
					"stored=%u expected=%u\n",
					derivation->proof_kind,
					premise_index,
					derivation->premises[premise_index].proposition.operation_id,
					structural_child_operation_id
				);
				return -1;
			}
			/* The structural child selects the occurrence and Context. If the
			 * producer retained explicit SelectedEvidence, its authority may be a
			 * derived Context-weakening Claim rather than the child's default
			 * ContextBinding/Operation owner. Preserve it. Only derive a default
			 * authority when the producer supplied none. */
			if (expected_authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
				expected_operation_id = structural_operation_id;
				if (derivation->proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO &&
					premise_index == 0) {
					expected_authority_kind =
						PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING;
					expected_authority_id =
						derivation->premises[premise_index].proposition.subject;
					expected_operation_id = PROTOTYPE_INVALID_ID;
				} else if (structural_operation_id != PROTOTYPE_INVALID_ID) {
					expected_authority_kind =
						PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION;
					expected_authority_id = structural_operation_id;
				} else {
					uint32_t authority_source_operation_id =
						judgement_operation_evidence_source(
							operations, structural_child_operation_id, 0
						);
					if (authority_source_operation_id >=
						operations->operation_count) {
						return -1;
					}
					const struct prototype_operation_node* authority_source =
						&operations->operations[authority_source_operation_id];
					if (authority_source->tag == PROTOTYPE_OPERATION_VAR) {
						expected_authority_kind =
							PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING;
						expected_authority_id =
							derivation->premises[premise_index].proposition.subject;
					} else if (authority_source->tag ==
						PROTOTYPE_OPERATION_CONSTRUCTOR) {
						expected_authority_kind =
							PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION;
						expected_authority_id =
							derivation->premises[premise_index].proposition.subject;
					} else {
						fprintf(
							stderr,
							"P0 unsupported authority-neutral structural child "
							"rule=%d index=%u child=%u tag=%d source=%u "
							"source_tag=%d\n",
							derivation->proof_kind,
							premise_index,
							structural_child_operation_id,
							child->tag,
							authority_source_operation_id,
							authority_source->tag
						);
						return -1;
					}
				}
			} else if (expected_authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION) {
				/* premise_operation_ids keeps the direct structural child. The
				 * selected Claim may belong to the child's transparent NAME or
				 * ASCRIPTION source, so Claim lookup must use that evidence owner. */
				expected_operation_id = expected_authority_id;
			}
		}
	}
	uint32_t selected_claim_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < (uint32_t)judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* candidate =
			&judgement->propositions[i];
		if (!judgement_candidate_has_publishable_derivation(
				judgement, operations, i
			) ||
			candidate->kind != derivation->premises[premise_index].proposition.kind ||
			candidate->context_id != expected_context_id ||
			candidate->subject != derivation->premises[premise_index].proposition.subject ||
			candidate->classifier !=
				derivation->premises[premise_index].proposition.classifier) {
			continue;
		}
		int candidate_authority_kind;
		uint32_t candidate_authority_id;
		candidate_claim_authority(
			candidate, &candidate_authority_kind, &candidate_authority_id
		);
		if (expected_authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID &&
			(candidate_authority_kind != expected_authority_kind ||
			 candidate_authority_id != expected_authority_id)) {
			continue;
		}
		if (expected_authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION &&
			candidate->operation_id != expected_operation_id) {
			continue;
		}
		uint32_t candidate_claim_id;
		if (judgement_intern_claim(
				judgement, candidate, &candidate_claim_id
			) != 0) {
			fprintf(stderr, "P0 premise claim intern failed candidate=%u\n", i);
			return -1;
		}
		if (selected_claim_id != PROTOTYPE_INVALID_ID &&
			selected_claim_id != candidate_claim_id) {
			/* An authority-neutral Core helper may match several typed
			 * occurrences. It cannot choose one of them; leave only this
			 * candidate Derivation unpublished. */
			return 1;
		}
		selected_claim_id = candidate_claim_id;
	}
	if (selected_claim_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_claim_id = selected_claim_id;
	return 0;
}

static int judgement_candidate_is_publishable(
	const struct prototype_judgement_db* judgement,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_proposition* candidate,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!judgement || !candidate || !proof ||
		candidate->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN ||
		proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
		return 0;
	}
	if (proof->conclusion_kind != candidate->kind ||
		proof->conclusion_context_id != candidate->context_id ||
		proof->conclusion_operation_id != candidate->operation_id ||
		proof->conclusion_subject != candidate->subject ||
		proof->conclusion_classifier != candidate->classifier) {
		return 0;
	}
	if (candidate->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION) {
		return 1;
	}
	if (!operations || candidate->operation_id >= operations->operation_count) {
		return 0;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[candidate->operation_id];
	if (operation->classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	/* Fixed-point candidates are solver history. Only the classifier selected by
	 * the final typed occurrence is a source Claim. Explicit derived rules may
	 * expose another convertible/admissible classifier over that occurrence. */
	return operation->classifier == candidate->classifier ||
		judgement_rule_derives_alternate_operation_classifier(
			proof->proof_kind
		);
}

static int judgement_candidate_has_publishable_derivation(
	const struct prototype_judgement_db* judgement,
	const struct prototype_operation_graph* operations,
	uint32_t candidate_id
) {
	if (!judgement || candidate_id >= judgement->proposition_count) {
		return 0;
	}
	for (uint32_t i = 0;
		i < (uint32_t)judgement->derivation_candidate_count; ++i) {
		const struct prototype_judgement_derivation_candidate* proof =
			&judgement->derivation_candidates[i];
		if (proof->conclusion_proposition_id == candidate_id &&
			judgement_candidate_is_publishable(
				judgement,
				operations,
				&judgement->propositions[candidate_id],
				proof
			)) {
			return 1;
		}
	}
	return 0;
}

static int judgement_rebuild_claim_derivations(
	struct prototype_judgement_db* judgement,
	const struct prototype_operation_graph* operations
) {
	if (!judgement) {
		return -1;
	}
	/* Accepted evidence is monotone. Imported certificates and earlier solver
	 * publications remain authoritative; publication only interns additional
	 * candidate conclusions and derivations. */
	if (prototype_judgement_db_rebuild_index(judgement) != 0) {
		return -1;
	}
	for (uint32_t i = 0;
		i < (uint32_t)judgement->derivation_candidate_count; ++i) {
		const struct prototype_judgement_derivation_candidate* proof =
			&judgement->derivation_candidates[i];
		if (proof->conclusion_proposition_id >=
			judgement->proposition_count) {
			continue;
		}
		const struct prototype_judgement_proposition* relation =
			&judgement->propositions[proof->conclusion_proposition_id];
		uint32_t claim_id;
		if (!judgement_candidate_is_publishable(
				judgement, operations, relation, proof
			)) {
			continue;
		}
		if (judgement_intern_claim(
				judgement, relation, &claim_id
			) != 0) {
			fprintf(stderr, "P0 claim intern failed candidate=%u\n", i);
			return -1;
		}
	}
	for (uint32_t i = 0;
		i < (uint32_t)judgement->derivation_candidate_count; ++i) {
		const struct prototype_judgement_derivation_candidate* proof =
			&judgement->derivation_candidates[i];
		if (proof->conclusion_proposition_id >=
			judgement->proposition_count) {
			continue;
		}
		const struct prototype_judgement_proposition* relation =
			&judgement->propositions[proof->conclusion_proposition_id];
		if (!judgement_candidate_is_publishable(
				judgement, operations, relation, proof
			)) {
			continue;
		}
		uint32_t conclusion_claim_id;
		if (judgement_intern_claim(
				judgement, relation, &conclusion_claim_id
			) != 0) {
			fprintf(stderr, "P0 conclusion intern failed candidate=%u\n", i);
			return -1;
		}
		struct prototype_judgement_derivation derivation;
		struct prototype_judgement_premise_edge
			premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		memset(&derivation, 0, sizeof(derivation));
		memset(premises, 0, sizeof(premises));
		derivation.premises = premises;
		derivation.proof_kind = proof->proof_kind;
		derivation.conclusion_claim_id = conclusion_claim_id;
		derivation.closure_rank = PROTOTYPE_INVALID_ID;
		derivation.rule_data = proof->rule_data;
		derivation.semantic_action_kind = proof->semantic_action_kind;
		derivation.semantic_action_id = proof->semantic_action_id;
		derivation.premise_count = proof->premise_count;
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			derivation.premises[j].claim_id = PROTOTYPE_INVALID_ID;
			derivation.premises[j].scoped_proposition_id = PROTOTYPE_INVALID_ID;
			derivation.premises[j].semantic_action_kind =
				proof->premises[j].semantic_action_kind;
			derivation.premises[j].semantic_action_id =
				proof->premises[j].semantic_action_id;
		}
		int sources_resolved = 1;
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (judgement_premise_is_scoped_rule_parameter(proof, j)) {
				if (proof->premises[j].proposition.kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
					fprintf(
						stderr,
						"P0 producer emitted unknown scoped premise rule=%d index=%u "
						"subject=%u classifier=%u\n",
						proof->proof_kind,
						j,
						proof->premises[j].proposition.subject,
						proof->premises[j].proposition.classifier
					);
					return -1;
				}
				struct prototype_judgement_proposition scoped =
					proof->premises[j].proposition;
				scoped.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID;
				scoped.authority_id = PROTOTYPE_INVALID_ID;
				scoped.operation_id = PROTOTYPE_INVALID_ID;
				if (prototype_judgement_proposition_intern(
						judgement,
						&scoped,
						&derivation.premises[j].scoped_proposition_id
					) != 0) {
					return -1;
				}
				continue;
			}
			uint32_t source_claim_id;
			int source_status = judgement_candidate_source_claim(
					judgement, operations, proof, j, &source_claim_id
				);
			if (source_status < 0) {
				return -1;
			}
			if (source_status > 0) {
				sources_resolved = 0;
				break;
			}
			derivation.premises[j].claim_id = source_claim_id;
		}
		if (!sources_resolved) {
			continue;
		}
		for (uint32_t j = 0; j < derivation.premise_count; ++j) {
			if (derivation.premises[j].claim_id == conclusion_claim_id) {
				sources_resolved = 0;
				break;
			}
		}
		if (!sources_resolved) {
			continue;
		}
		uint32_t derivation_id;
		if (prototype_judgement_derivation_intern_exact(
				judgement, &derivation, &derivation_id
			) != 0) {
			fprintf(
				stderr,
				"P0 derivation capacity failed count=%zu capacity=%zu candidate=%u\n",
				judgement->derivation_count,
				judgement->derivation_capacity,
				i
			);
			return -1;
		}
	}
	return 0;
}

int prototype_judgement_recompute_closure_ranks(
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
) {
	if (!judgement || (judgement->claim_count != 0 && !judgement->claims) ||
		(judgement->derivation_count != 0 && !judgement->derivations)) {
		fprintf(stderr, "P0 invalid grounding storage\n");
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (!proposition) {
			judgement->claims[i].closure_rank = PROTOTYPE_INVALID_ID;
			continue;
		}
		if (proposition->operation_id != PROTOTYPE_INVALID_ID &&
			!operations) {
			fprintf(stderr, "P0 missing operation graph for claim=%u\n", i);
			return -1;
		}
		judgement->claims[i].closure_rank = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->derivation_count; ++i) {
		judgement->derivations[i].closure_rank = PROTOTYPE_INVALID_ID;
	}
	int changed;
	do {
		changed = 0;
		for (uint32_t i = 0; i < (uint32_t)judgement->derivation_count; ++i) {
			struct prototype_judgement_derivation* derivation =
				&judgement->derivations[i];
			if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID ||
				derivation->conclusion_claim_id >= judgement->claim_count) {
				continue;
			}
			uint32_t max_rank = 0;
			int ready = 1;
			uint32_t source_count = 0;
			for (uint32_t j = 0; j < derivation->premise_count; ++j) {
				uint32_t source = derivation->premises[j].claim_id;
				if (source == PROTOTYPE_INVALID_ID) {
					continue;
				}
				source_count++;
				if (source == derivation->conclusion_claim_id ||
					source >= judgement->claim_count ||
					judgement->claims[source].closure_rank == PROTOTYPE_INVALID_ID) {
					ready = 0;
					break;
				}
				if (judgement->claims[source].closure_rank > max_rank) {
					max_rank = judgement->claims[source].closure_rank;
				}
			}
			if (!ready) {
				continue;
			}
			if (source_count != 0 && max_rank == PROTOTYPE_INVALID_ID - 1) {
				fprintf(stderr, "P0 closure rank overflow derivation=%u\n", i);
				return -1;
			}
			uint32_t rank = source_count == 0 ? 0 : max_rank + 1;
			if (derivation->closure_rank == PROTOTYPE_INVALID_ID ||
				rank < derivation->closure_rank) {
				derivation->closure_rank = rank;
				changed = 1;
			}
			struct prototype_judgement_claim* conclusion =
				&judgement->claims[derivation->conclusion_claim_id];
			if (conclusion->closure_rank == PROTOTYPE_INVALID_ID ||
				rank < conclusion->closure_rank) {
				conclusion->closure_rank = rank;
				changed = 1;
			}
		}
	} while (changed);
	return 0;
}

int prototype_judgement_publish_candidates(
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
) {
	if (!judgement || (judgement->claim_count != 0 && !judgement->claims) ||
		(judgement->derivation_count != 0 && !judgement->derivations)) {
		fprintf(stderr, "P0 invalid publication storage\n");
		return -1;
	}
	if (judgement_rebuild_claim_derivations(judgement, operations) != 0 ||
		prototype_judgement_recompute_closure_ranks(operations, judgement) != 0) {
		return -1;
	}
	/* Claim and Derivation IDs are append-only arena handles. Artifact
	 * publication performs its own dense relocation, so compacting this arena
	 * would invalidate HOTT actions, CwF certificates, and registered roots.
	 * Ungrounded solver slots remain distinguishable by an invalid closure rank. */
	return prototype_judgement_db_rebuild_index(judgement);
}

int prototype_judgement_add_normalization_premise_conversions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !judgement) {
		return -1;
	}
	size_t derivation_candidate_count = judgement->derivation_candidate_count;
	for (size_t i = 0; i < derivation_candidate_count; ++i) {
		struct prototype_judgement_derivation_candidate* proof = &judgement->derivation_candidates[i];
		if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			uint32_t expected_operation_id = PROTOTYPE_INVALID_ID;
			int operation_status = judgement_expected_premise_operation(
				operations, proof, j, &expected_operation_id
			);
			if (operation_status < 0) {
				return -1;
			}
			int existing_status = operation_status == 0 ?
				judgement_operation_candidate_claim_has_derivation(
					judgement,
					expected_operation_id,
					proof->premises[j].proposition.context_id,
					proof->premises[j].proposition.kind,
					proof->premises[j].proposition.subject,
					proof->premises[j].proposition.classifier
				) : judgement_candidate_claim_has_derivation(
					judgement,
					proof->premises[j].proposition.context_id,
					proof->premises[j].proposition.kind,
					proof->premises[j].proposition.subject,
					proof->premises[j].proposition.classifier,
					proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION &&
					proof->proof_kind !=
						PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE
				);
			if (existing_status == 0) {
				continue;
			}
			if (proof->premises[j].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
				continue;
			}
			for (size_t k = 0; k < judgement->proposition_count; ++k) {
				const struct prototype_judgement_proposition* candidate =
					&judgement->propositions[k];
				uint32_t candidate_cursor = 0;
				uint32_t candidate_derivation_id;
				int has_direct_derivation = 0;
				while (prototype_judgement_candidate_derivation_next(
					judgement->propositions,
					judgement->proposition_count,
					judgement->derivation_candidates,
					judgement->derivation_candidate_count,
					(uint32_t)k,
					&candidate_cursor,
					&candidate_derivation_id
				) == 0) {
					int candidate_kind = judgement->derivation_candidates[
						candidate_derivation_id
					].proof_kind;
					if (candidate_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION &&
						candidate_kind !=
							PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE) {
						has_direct_derivation = 1;
						break;
					}
				}
				if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					(operation_status == 0 &&
					 candidate->operation_id != expected_operation_id) ||
					candidate->context_id != proof->premises[j].proposition.context_id ||
					candidate->subject != proof->premises[j].proposition.subject ||
					!has_direct_derivation ||
					!(prototype_judgement_classifier_conversion(
						terms,
						type_declarations,
						proof->premises[j].proposition.classifier,
						candidate->classifier
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					continue;
				}
				uint32_t premise_subjects[1] = { candidate->subject };
				uint32_t premise_classifiers[1] = { candidate->classifier };
				if (add_relation_with_premises(
						judgement,
						proof->premises[j].proposition.context_id,
						candidate->operation_id,
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
						proof->premises[j].proposition.subject,
						proof->premises[j].proposition.classifier,
						PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
						NULL,
						premise_subjects,
						premise_classifiers,
						1
					) != 0) {
					return -1;
				}
				break;
			}
		}
	}
	return 0;
}

static int validate_app_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		proof->premise_count != 2 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	const struct prototype_term* app;
	if (term_core_app(terms, relation->subject, &app) != 0) {
		return -1;
	}
	int function_matches = 0;
	int argument_matches = 0;
	if (prototype_term_core_shape_equal(
			terms, app->as.app.function, proof->premises[0].proposition.subject, &function_matches
		) != 0 ||
		prototype_term_core_shape_equal(
			terms, app->as.app.argument, proof->premises[1].proposition.subject, &argument_matches
		) != 0 ||
		!function_matches || !argument_matches) {
		return -1;
	}
	uint32_t function_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t specialized_function_classifier;
	if (prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			proof->premises[0].proposition.classifier,
			proof->premises[1].proposition.classifier,
			&specialized_function_classifier
		) != 0) {
		return -1;
	}
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		specialized_function_classifier,
		&function_pi,
		&domain,
		&codomain_family
	);
	if (status != 0) {
		return -1;
	}
	(void)function_pi;
	uint32_t domain_whnf;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			domain,
			&domain_whnf
		) != 0 || domain_whnf >= terms->term_count) {
		return -1;
	}
	if (terms->terms[domain_whnf].tag == PROTOTYPE_TERM_RETURN) {
		domain_whnf = terms->terms[domain_whnf].as.return_term.value;
	}
	if (!prototype_judgement_classifier_compatible(
			terms,
			type_declarations,
			domain_whnf,
			proof->premises[1].proposition.classifier
		)) {
		return -1;
	}
	uint32_t result_classifier;
	uint32_t identity;
	if (prototype_substitution_identity(
			substitutions, contexts, relation->context_id, &identity
		) != 0 ||
		instantiate_pure_family_in_context(
			contexts,
			substitutions,
			terms,
			type_declarations,
			identity,
			proof->premises[1].proposition.classifier,
			codomain_family,
			proof->premises[1].proposition.subject,
			proof->premises[1].proposition.classifier,
			&result_classifier
		) != 0 ||
			!(prototype_judgement_classifier_conversion(
				terms,
			type_declarations,
			result_classifier,
			relation->classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return 0;
}

static int validate_lambda_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_LAMBDA ||
		proof->premise_count != 2 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	if (proof->premises[0].proposition.subject >= terms->term_count ||
		proof->premises[1].proposition.subject >= terms->term_count ||
		terms->terms[proof->premises[0].proposition.subject].tag != PROTOTYPE_TERM_VAR ||
		proof->premises[0].proposition.context_id != proof->premises[1].proposition.context_id) {
		return -1;
	}
	uint32_t occurrence_lambda;
	int occurrence_matches;
	if (prototype_term_lambda(
			terms,
			terms->terms[proof->premises[0].proposition.subject].as.var.binding_id,
			proof->premises[1].proposition.subject,
			&occurrence_lambda
		) != 0 || prototype_term_core_shape_equal(
			terms, occurrence_lambda, relation->subject, &occurrence_matches
		) != 0 || !occurrence_matches) {
		return -1;
	}
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t lambda_classifier;
	if (classifier_kernel_strip_effect_row_foralls(
			terms, type_declarations, NULL, relation->classifier, &lambda_classifier
		) != 0) {
		return -1;
	}
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		lambda_classifier,
		&lambda_pi,
		&domain,
		&codomain_family
	);
	(void)lambda_pi;
	if (status != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			domain,
			proof->premises[0].proposition.classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	uint32_t expected_body_classifier;
	uint32_t projection;
	if (prototype_substitution_projection(
			substitutions,
			contexts,
			proof->premises[0].proposition.context_id,
			&projection
		) != 0 ||
		instantiate_pure_family_in_context(
			contexts,
			substitutions,
			terms,
			type_declarations,
			projection,
			proof->premises[0].proposition.classifier,
			codomain_family,
			proof->premises[0].proposition.subject,
			proof->premises[0].proposition.classifier,
			&expected_body_classifier
		) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			expected_body_classifier,
			proof->premises[1].proposition.classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	(void)codomain_family;
	return 0;
}

static int validate_match_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_APP ||
		proof->premise_count !=
			terms->terms[relation->subject].as.match.case_count + 1 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition.subject != relation->classifier ||
		!term_is_universe_var(terms, proof->premises[0].proposition.classifier)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[relation->subject];
	const struct prototype_term* motive_app = &terms->terms[relation->classifier];
	const struct prototype_term* motive_lambda;
	const struct prototype_term* motive_body;
	uint32_t normalized_classifier;
	if (motive_app->as.app.argument != match->as.match.scrutinee ||
		terms->terms[motive_app->as.app.function].tag != PROTOTYPE_TERM_LAMBDA ||
		classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			relation->classifier,
			&normalized_classifier
		) != 0) {
		return -1;
	}
	motive_lambda = &terms->terms[motive_app->as.app.function];
	if (motive_lambda->as.lambda.body >= terms->term_count) {
		return -1;
	}
	motive_body = &terms->terms[motive_lambda->as.lambda.body];
	int motive_is_case_split = motive_body->tag == PROTOTYPE_TERM_MATCH;
	if (motive_is_case_split &&
		(motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binding_id !=
			motive_lambda->as.lambda.binding_id)) {
		return -1;
	}
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_is_case_split ?
			motive_body->as.match.first_case + i : PROTOTYPE_INVALID_ID;
		if (case_id >= terms->case_count ||
			(motive_is_case_split && motive_case_id >= terms->case_count) ||
			proof->premises[i + 1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			!term_exists(terms, proof->premises[i + 1].proposition.classifier)) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = motive_is_case_split ?
			&terms->cases[motive_case_id] : NULL;
		const struct prototype_judgement_candidate_premise* branch_premise =
			&proof->premises[i + 1];
		if (motive_is_case_split &&
			(match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count)) {
			return -1;
		}
		if (motive_is_case_split &&
			(match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			motive_case->constructor_owner == PROTOTYPE_INVALID_ID)) {
			if (match_case->constructor_owner != motive_case->constructor_owner) {
				return -1;
			}
		} else if (motive_is_case_split) {
			int same_owner = 0;
			if (prototype_term_view_shape_equal(
					terms,
					match_case->constructor_owner,
					motive_case->constructor_owner,
					&same_owner
				) != 0 ||
				!same_owner) {
				return -1;
			}
		}
		uint32_t expected_branch_body = match_case->body;
		uint32_t expected_motive_body;
		if (motive_is_case_split) {
			expected_motive_body = motive_case->body;
		} else if (match_case_motive_classifier(
				terms,
				match_case,
				motive_app->as.app.function,
				&expected_motive_body
			) != 0) {
			return -1;
		}
		if (branch_premise->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			const struct prototype_substitution* supplied =
				prototype_substitution_get(
					substitutions, branch_premise->semantic_action_id
				);
			uint32_t refined_context;
			uint32_t expected_substitution;
			uint32_t constructor_term;
			if (!supplied || match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
				branch_premise->proposition.context_id !=
					supplied->source_context || prototype_judgement_indexed_branch_refinement(
					contexts,
					substitutions,
					terms,
					type_declarations,
					relation->context_id,
					match->as.match.scrutinee,
					match_case->constructor_owner,
					match_case->constructor_id,
					supplied->target_context,
					&terms->case_binders[match_case->first_binder],
					match_case->binder_count,
					&refined_context,
					&expected_substitution,
					&constructor_term
				) != 0 || refined_context != supplied->source_context ||
				expected_substitution != branch_premise->semantic_action_id ||
				prototype_term_reindex(
					terms,
					type_declarations,
					contexts,
					substitutions,
					match_case->body,
					expected_substitution,
					&expected_branch_body
				) != 0 || prototype_term_reindex(
					terms,
					type_declarations,
					contexts,
					substitutions,
					expected_motive_body,
					expected_substitution,
					&expected_motive_body
				) != 0) {
				return -1;
			}
			(void)constructor_term;
		} else if (branch_premise->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID ||
			branch_premise->semantic_action_id != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		if (branch_premise->proposition.subject != expected_branch_body) {
			return -1;
		}
		if (!motive_is_case_split) {
			if (!(prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					branch_premise->proposition.classifier,
					expected_motive_body
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
				return -1;
			}
			continue;
		}
		uint32_t expected_motive_case_body;
		if (core_match_case_alpha_reindex(
				terms,
				type_declarations,
				&terms->case_binders[match_case->first_binder],
				&terms->case_binders[motive_case->first_binder],
				match_case->binder_count,
				branch_premise->proposition.classifier,
				&expected_motive_case_body
			) != 0 ||
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				expected_motive_case_body,
				expected_motive_body
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
	}
	(void)normalized_classifier;
	return 0;
}

static int validate_induction_hypothesis_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		proof->premise_count != 0 ||
		proof->rule_data.induction.match >= terms->term_count ||
		proof->rule_data.induction.motive >= terms->term_count ||
		terms->terms[proof->rule_data.induction.match].tag != PROTOTYPE_TERM_MATCH ||
		proof->rule_data.induction.case_index >=
			terms->terms[proof->rule_data.induction.match].as.match.case_count) {
		return -1;
	}
	const struct prototype_term* ih = &terms->terms[relation->subject];
	if (ih->as.induction_hypothesis.ih_scope_id >= terms->ih_scope_count ||
		terms->ih_scopes[ih->as.induction_hypothesis.ih_scope_id].match_term !=
			proof->rule_data.induction.match) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[proof->rule_data.induction.match];
	uint32_t case_id = match->as.match.first_case + proof->rule_data.induction.case_index;
	if (case_id >= terms->case_count ||
		proof->rule_data.induction.field_index >= terms->cases[case_id].binder_count) {
		return -1;
	}
	const struct prototype_case_binder* binder =
		&terms->case_binders[
			terms->cases[case_id].first_binder + proof->rule_data.induction.field_index
		];
	if (ih->as.induction_hypothesis.argument >= terms->term_count ||
		terms->terms[ih->as.induction_hypothesis.argument].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[ih->as.induction_hypothesis.argument].as.var.binding_id !=
			binder->binding_id) {
		return -1;
	}
	if (!binder->is_recursive) {
		return -1;
	}
	uint32_t match_classifier;
	uint32_t expected_classifier;
	if (prototype_term_app(
			terms,
			proof->rule_data.induction.motive,
			match->as.match.scrutinee,
			&match_classifier
		) != 0 || !match_motive_result_classifier(
			terms, proof->rule_data.induction.match, match_classifier
		) || prototype_term_app(
			terms,
			proof->rule_data.induction.motive,
			ih->as.induction_hypothesis.argument,
			&expected_classifier
		) != 0) {
		return -1;
	}
	(void)judgement;
	return prototype_judgement_classifier_conversion(
		terms,
		type_declarations,
		expected_classifier,
		relation->classifier
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 0 : -1;
}

static int validate_solved_match_motive_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!match_motive_result_classifier(terms, relation->subject, relation->classifier)) {
		fprintf(stderr, "P0 motive validator header failed\n");
		return -1;
	}
	const struct prototype_term* match = &terms->terms[relation->subject];
	if (proof->premise_count != match->as.match.case_count) {
		return -1;
	}
	const struct prototype_term* motive_app = &terms->terms[relation->classifier];
	const struct prototype_term* motive =
		&terms->terms[motive_app->as.app.function];
	if (motive->as.lambda.body >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* motive_body = &terms->terms[motive->as.lambda.body];
	if (motive_body->tag != PROTOTYPE_TERM_MATCH) {
		/* A direct dependent motive is checked by applying it to the constructor
		 * pattern represented by each branch. Constant motives are the special
		 * case in which this beta-reduces to the same classifier. */
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			uint32_t case_id = match->as.match.first_case + i;
			uint32_t expected_classifier;
			if (case_id >= terms->case_count ||
				proof->premises[i].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				proof->premises[i].proposition.subject != terms->cases[case_id].body ||
				match_case_motive_classifier(
					terms,
					&terms->cases[case_id],
					motive_app->as.app.function,
					&expected_classifier
				) != 0) {
				return -1;
			}
			if (!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				proof->premises[i].proposition.classifier,
				expected_classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
				return -1;
			}
		}
		return 0;
	}
	if (motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binding_id !=
			motive->as.lambda.binding_id) {
		fprintf(stderr, "P0 motive validator structure failed\n");
		return -1;
	}
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_body->as.match.first_case + i;
		if (case_id >= terms->case_count || motive_case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = &terms->cases[motive_case_id];
		if (match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count ||
			match_case->constructor_owner != motive_case->constructor_owner) {
			fprintf(stderr, "P0 motive validator case shape failed branch=%u\n", i);
			return -1;
		}
		if (proof->premises[i].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition.subject != match_case->body) {
			return -1;
		}
		int found = 0;
		{
			uint32_t expected_motive_case_body = PROTOTYPE_INVALID_ID;
			int prepare_status = core_match_case_alpha_reindex(
					terms, type_declarations,
					&terms->case_binders[match_case->first_binder],
					&terms->case_binders[motive_case->first_binder],
					match_case->binder_count, proof->premises[i].proposition.classifier,
					&expected_motive_case_body
				);
			if (prepare_status == 0) {
				int equal;
				if (classifier_conversion_decision(
						prototype_judgement_classifier_conversion(
							terms, type_declarations, expected_motive_case_body,
							motive_case->body
						),
						&equal
					) != 0) {
					return -1;
				}
				if (equal) {
					found = 1;
					break;
				}
			}
			/* A structurally recursive motive records M(rest) as an IH node in
			 * its own match frame. This is a guarded equation, not a new WHNF
			 * conversion rule. Verify the frame and the substituted recursive
			 * binder explicitly. */
			if (prepare_status == 0 && motive_case->body < terms->term_count &&
				terms->terms[motive_case->body].tag ==
					PROTOTYPE_TERM_INDUCTION_HYPOTHESIS &&
				expected_motive_case_body < terms->term_count &&
				terms->terms[expected_motive_case_body].tag == PROTOTYPE_TERM_APP) {
				const struct prototype_term* motive_ih =
					&terms->terms[motive_case->body];
				const struct prototype_term* expected_app =
					&terms->terms[expected_motive_case_body];
				if (motive_body->as.match.ih_scope_id == motive_ih->as.induction_hypothesis.ih_scope_id &&
					expected_app->as.app.function == motive_app->as.app.function &&
					expected_app->as.app.argument == motive_ih->as.induction_hypothesis.argument) {
					found = 1;
					break;
				}
			}
		}
		if (!found) {
			fprintf(
				stderr,
				"P0 motive validator branch failed branch=%u "
				"body=%u motive_body=%u\n",
				i,
				match_case->body,
				motive_case->body
			);
			return -1;
		}
	}
	return 0;
}

static int initialize_exact_claim_rule(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* conclusion,
	int proof_kind,
	const uint32_t* premise_claim_ids,
	uint32_t premise_count,
	struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* proof_premises
) {
	if (!judgement || !conclusion || !proof || !proof_premises ||
		(premise_count != 0 && !premise_claim_ids) ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	memset(proof, 0, sizeof(*proof));
	proof->proof_kind = proof_kind;
	proof->conclusion_kind = conclusion->kind;
	proof->conclusion_context_id = conclusion->context_id;
	proof->conclusion_operation_id = conclusion->operation_id;
	proof->conclusion_subject = conclusion->subject;
	proof->conclusion_classifier = conclusion->classifier;
	initialize_proof_rule_parameters(proof, proof_premises);
	proof->premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		const struct prototype_judgement_proposition* premise =
			prototype_judgement_claim_proposition(
				judgement, premise_claim_ids[i]
			);
		if (!premise) {
			return -1;
		}
		proof->premises[i].proposition = *premise;
	}
	return 0;
}

static int validate_return_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
);

static int validate_thunk_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
);

int prototype_judgement_add_lambda_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t binder_claim_id,
	uint32_t body_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[2] = { binder_claim_id, body_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			premise_ids, 2, &proof, premises
		) != 0 || validate_lambda_intro_proof(
			terms, type_declarations, contexts, substitutions, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_app_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t function_claim_id,
	uint32_t argument_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[2] = { function_claim_id, argument_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
			premise_ids, 2, &proof, premises
		) != 0 || validate_app_elim_proof(
			terms, type_declarations, contexts, substitutions, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

static int judgement_add_unary_intro_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t premise_claim_id,
	int proof_kind,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[1] = { premise_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement, &conclusion, proof_kind, premise_ids, 1, &proof, premises
		) != 0) {
		return -1;
	}
	if (proof_kind == PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO) {
		if (validate_return_intro_proof(
				terms, type_declarations, &conclusion, &proof
			) != 0) {
			return -1;
		}
	} else if (proof_kind == PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO) {
		if (validate_thunk_intro_proof(
				terms, type_declarations, &conclusion, &proof
			) != 0) {
			return -1;
		}
	} else {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_return_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t value_claim_id,
	uint32_t* p_claim_id
) {
	return judgement_add_unary_intro_claim(
		judgement,
		terms,
		type_declarations,
		context_id,
		subject,
		classifier,
		value_claim_id,
		PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
		p_claim_id
	);
}

int prototype_judgement_add_thunk_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t computation_claim_id,
	uint32_t* p_claim_id
) {
	return judgement_add_unary_intro_claim(
		judgement,
		terms,
		type_declarations,
		context_id,
		subject,
		classifier,
		computation_claim_id,
		PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
		p_claim_id
	);
}

int prototype_judgement_add_indexed_match_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_operation_graph* operations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t classifier_claim_id,
	const uint32_t* branch_claim_ids,
	const uint32_t* branch_substitution_ids,
	uint32_t branch_claim_count,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || (branch_claim_count != 0 && !branch_claim_ids) ||
		branch_claim_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	(void)operations;
	premise_ids[0] = classifier_claim_id;
	for (uint32_t i = 0; i < branch_claim_count; ++i) {
		premise_ids[i + 1] = branch_claim_ids[i];
	}
	if (initialize_exact_claim_rule(
			judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
			premise_ids, branch_claim_count + 1, &proof, premises
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < branch_claim_count; ++i) {
		if (!branch_substitution_ids) {
			continue;
		}
		if (!prototype_substitution_get(
				substitutions, branch_substitution_ids[i]
			)) {
			return -1;
		}
		premises[i + 1].semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION;
		premises[i + 1].semantic_action_id = branch_substitution_ids[i];
	}
	if (validate_match_elim_proof(
			terms, type_declarations, contexts, substitutions,
			judgement, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_match_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_operation_graph* operations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t classifier_claim_id,
	const uint32_t* branch_claim_ids,
	uint32_t branch_claim_count,
	uint32_t* p_claim_id
) {
	return prototype_judgement_add_indexed_match_claim(
		judgement,
		terms,
		type_declarations,
		contexts,
		substitutions,
		operations,
		context_id,
		subject,
		classifier,
		classifier_claim_id,
		branch_claim_ids,
		NULL,
		branch_claim_count,
		p_claim_id
	);
}

int prototype_judgement_add_induction_hypothesis_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t match,
	uint32_t motive,
	uint32_t case_index,
	uint32_t field_index,
	uint32_t* p_claim_id
) {
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement,
			&conclusion,
			PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM,
			NULL,
			0,
			&proof,
			premises
		) != 0) {
		return -1;
	}
	proof.rule_data.induction.match = match;
	proof.rule_data.induction.motive = motive;
	proof.rule_data.induction.case_index = case_index;
	proof.rule_data.induction.field_index = field_index;
	if (validate_induction_hypothesis_elim_proof(
			terms, type_declarations, judgement, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, NULL, p_claim_id
	);
}

static int validate_type_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms, relation->subject, &type_id, arguments, &argument_count
		) != 0 || type_id >= type_declarations->type_count ||
		argument_count > 16) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (argument_count == 0 &&
		type->formation_classifier != PROTOTYPE_INVALID_ID) {
		return type->formation_classifier == relation->classifier ? 0 : -1;
	}
	return type_instance_has_known_type(terms, type_declarations, relation->subject) &&
		term_is_universe_var(terms, relation->classifier) ? 0 : -1;
}

static int validate_computation_type_formation_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_COMPUTATION_TYPE) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* computation_type = &terms->terms[relation->subject];
	struct prototype_effect_row_normal_form effect_row;
	return prototype_term_effect_row_normal_form(
			terms, computation_type->as.computation_type.label, &effect_row
		) == 0 && proof->premises[0].proposition.kind ==
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[0].proposition.subject ==
			computation_type->as.computation_type.result &&
		proof->premises[0].proposition.classifier == relation->classifier ? 0 : -1;
}

static int validate_thunk_type_formation_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_THUNK_TYPE) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return proof->premises[0].proposition.kind ==
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[0].proposition.subject ==
			terms->terms[relation->subject].as.thunk_type.computation &&
		proof->premises[0].proposition.classifier == relation->classifier ? 0 : -1;
}

static int validate_constructor_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_CONSTRUCTOR) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* constructor = &terms->terms[relation->subject];
	if (!constructor_belongs_to_owner(
			terms,
			type_declarations,
			constructor->as.constructor.owner,
			constructor->as.constructor.constructor_id
		) ||
			!classifier_returns_owner(
				terms,
				type_declarations,
				relation->classifier,
				constructor->as.constructor.owner
			)) {
		return -1;
	}
	return 0;
}

static int validate_constructor_spine_formation_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count == 0 ||
		proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count;
	if (prototype_term_constructor_spine_info(
			terms,
			relation->subject,
			&head,
			&owner,
			&constructor_index,
			arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&argument_count
		) != 0 || argument_count != proof->premise_count) {
		return -1;
	}
	uint32_t argument_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (proof->premises[i].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition.context_id != relation->context_id ||
			proof->premises[i].proposition.subject != arguments[i]) {
			return -1;
		}
		argument_classifiers[i] = proof->premises[i].proposition.classifier;
	}
	uint32_t expected_classifier;
	int saturated;
	if (prototype_judgement_constructor_spine_classifier(
			terms,
			type_declarations,
			contexts,
				substitutions,
				relation->context_id,
				relation->subject,
				proof->rule_data.constructor.owner_view,
				argument_classifiers,
			argument_count,
			&expected_classifier,
			&saturated
		) != 0 || !(prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			expected_classifier,
			relation->classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	(void)head;
	(void)owner;
	(void)constructor_index;
	(void)saturated;
	return 0;
}

static int validate_match_type_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_exists(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier) ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_MATCH) ||
		proof->premise_count != terms->terms[relation->subject].as.match.case_count) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		uint32_t case_id = terms->terms[relation->subject].as.match.first_case + i;
		if (case_id >= terms->case_count ||
			terms->cases[case_id].body >= terms->term_count ||
			terms->cases[case_id].first_binder > terms->case_binder_count ||
			terms->cases[case_id].binder_count >
				terms->case_binder_count - terms->cases[case_id].first_binder ||
			!match_case_has_valid_constructor(
				terms,
				type_declarations,
				&terms->cases[case_id]
			) ||
			proof->premises[i].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition.subject != terms->cases[case_id].body ||
			proof->premises[i].proposition.classifier != relation->classifier) {
			return -1;
		}
	}
	return 0;
}

static int validate_universe_cumulativity_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_universe_var(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_conversion_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	struct prototype_term_conversion_result conversion;
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition.subject != relation->subject ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier) ||
		!term_exists(terms, proof->premises[0].proposition.classifier)) {
		return -1;
	}
	conversion = classifier_kernel_conversion(
			terms,
			type_declarations,
			NULL,
			relation->classifier,
			proof->premises[0].proposition.classifier
		);
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	return 0;
}

int prototype_judgement_add_conversion_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t expected_classifier,
	uint32_t source_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[1] = { source_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = expected_classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement, &conclusion, PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
			premise_ids, 1, &proof, premises
		) != 0 || validate_conversion_proof(
			terms, type_declarations, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

static int validate_expected_type_exposure_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition.subject != relation->subject ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier) ||
		!term_exists(terms, proof->premises[0].proposition.classifier)) {
		return -1;
	}
	return classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		NULL,
		relation->classifier,
		proof->premises[0].proposition.classifier,
		0
	) ? 0 : -1;
}

static int validate_effect_weaken_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition.subject != relation->subject ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier) ||
		!term_exists(terms, proof->premises[0].proposition.classifier)) {
		return -1;
	}
	struct prototype_term_classifier_view source;
	struct prototype_term_classifier_view target;
	unsigned source_effects;
	unsigned target_effects;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL,
			proof->premises[0].proposition.classifier, &source
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, relation->classifier, &target
		) != 0 || source.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		target.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		target.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, source.result, target.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || prototype_term_effect_row_closed_bits(
			terms, source.effect_row, &source_effects
		) != 0 || prototype_term_effect_row_closed_bits(
			terms, target.effect_row, &target_effects
		) != 0 || (source_effects & target_effects) != source_effects) {
		return -1;
	}
	return 0;
}

static int validate_declaration_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	if (term_has_tag(terms, relation->subject, PROTOTYPE_TERM_EXTERNAL_REF)) {
		return proof->premise_count == 0 &&
			term_is_structural_type(terms, type_declarations, relation->classifier) ? 0 : -1;
	}
	if (term_has_tag(terms, relation->subject, PROTOTYPE_TERM_CONSTRUCTOR)) {
		if (proof->premise_count != 0) {
			return -1;
		}
		const struct prototype_term* constructor = &terms->terms[relation->subject];
		uint32_t type_id;
		uint32_t args[16];
		uint32_t arg_count;
		int has_local_owner =
			prototype_term_type_instance_info(
				terms,
				constructor->as.constructor.owner,
				&type_id,
				args,
				&arg_count
			) == 0 &&
			type_id < type_declarations->type_count;
		(void)args;
		(void)arg_count;
		if (has_local_owner &&
			!constructor_belongs_to_owner(
				terms,
				type_declarations,
				constructor->as.constructor.owner,
				constructor->as.constructor.constructor_id
			)) {
			return -1;
		}
			return classifier_returns_owner(
				terms,
				type_declarations,
				relation->classifier,
				constructor->as.constructor.owner
			) ? 0 : -1;
	}
	if (proof->premise_count != 1 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition.subject != relation->subject ||
		!term_exists(terms, proof->premises[0].proposition.classifier)) {
		return -1;
	}
	if (!prototype_judgement_classifier_compatible(
			terms,
			type_declarations,
			relation->classifier,
			proof->premises[0].proposition.classifier
		)) {
		return -1;
	}
	(void)judgement;
	return 0;
}

static int validate_assumption_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_VAR) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
		uint32_t classifier_classifier;
		uint32_t binding_id = terms->terms[relation->subject].as.var.binding_id;
		uint32_t assumption_context_id;
		if (prototype_context_find_binding(
				contexts,
				proof->conclusion_context_id,
				binding_id,
				&assumption_context_id
			) != 0) {
			return -1;
		}
		const struct prototype_context* entry =
			prototype_context_get(contexts, assumption_context_id);
		uint32_t entry_classifier = prototype_context_classifier_term(entry);
		if (!entry || entry_classifier == PROTOTYPE_INVALID_ID ||
			!(prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				entry_classifier,
				relation->classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		if (term_is_structural_type(terms, type_declarations, relation->classifier)) {
			return 0;
		}
		uint32_t classifier_classifiers[32];
		uint32_t classifier_classifier_count = 0;
		if (collect_judgement_subject_classifiers(
				judgement,
				terms,
				type_declarations,
				relation->classifier,
				classifier_classifiers,
				32,
				&classifier_classifier_count
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < classifier_classifier_count; ++i) {
			classifier_classifier = classifier_classifiers[i];
			if (term_is_universe_var(terms, classifier_classifier)) {
				return 0;
			}
		}
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
		uint32_t binding_id = terms->terms[relation->subject].as.var.binding_id;
		struct prototype_case_binder previous_binders[64];
		struct prototype_judgement_proposition delta_relations[16];
		struct prototype_judgement_derivation_candidate delta_proofs[16];
		struct prototype_judgement_candidate_premise delta_premises[
			16 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		];
		struct prototype_judgement_match_motive_result match_motive_results[16];
		struct prototype_judgement_computation_constraint computation_constraints[16];
		struct prototype_judgement_effect_row_constraint effect_row_constraints[16];
		struct prototype_judgement_db judgement_view = *judgement;
		uint32_t type_id;
		uint32_t arguments[64];
		uint32_t argument_count;
		if (proof->rule_data.constructor.owner_view == PROTOTYPE_INVALID_ID ||
			proof->rule_data.constructor.owner_view >= terms->term_count ||
			proof->rule_data.constructor.constructor_index == PROTOTYPE_INVALID_ID ||
			proof->rule_data.constructor.field_index >= 64 ||
			prototype_type_declaration_instance_info(
				type_declarations,
				terms,
				proof->rule_data.constructor.owner_view,
				&type_id,
				arguments,
				64,
				&argument_count
			) != 0 ||
			type_id >= type_declarations->type_count ||
			relation->context_id >= contexts->context_count) {
			return -1;
		}
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[type_id];
		if (proof->rule_data.constructor.constructor_index >= type->constructor_count ||
			type->first_constructor + proof->rule_data.constructor.constructor_index >=
				type_declarations->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[
				type->first_constructor + proof->rule_data.constructor.constructor_index
			];
		const struct prototype_context* parameter_context =
			prototype_context_get(contexts, constructor->parameter_context);
		const struct prototype_context* field_context =
			prototype_context_get(contexts, constructor->field_context);
		if (!parameter_context || !field_context ||
			field_context->depth < parameter_context->depth ||
			field_context->depth - parameter_context->depth > 64 ||
			proof->rule_data.constructor.field_index >=
				field_context->depth - parameter_context->depth) {
			return -1;
		}
		uint32_t field_count = field_context->depth - parameter_context->depth;
		uint32_t context_id = relation->context_id;
		for (uint32_t i = field_count; i > 0; --i) {
			const struct prototype_context* entry =
				prototype_context_get(contexts, context_id);
			if (!entry || context_id == 0) {
				return -1;
			}
			memset(&previous_binders[i - 1], 0, sizeof(previous_binders[i - 1]));
			previous_binders[i - 1].binding_id = entry->binding_id;
			context_id = entry->parent;
		}
		if (previous_binders[proof->rule_data.constructor.field_index].binding_id != binding_id) {
			return -1;
		}
		struct prototype_judgement_delta delta;
		prototype_judgement_delta_init(
			&delta,
			&judgement_view,
				delta_relations,
					delta_proofs,
					16,
					delta_premises,
					16 * PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
					match_motive_results,
					16,
					computation_constraints,
					16,
					effect_row_constraints,
					16
					);
			prototype_judgement_delta_set_context_store(
				&delta, contexts, substitutions
			);
			prototype_judgement_delta_set_context(
				&delta, relation->context_id
			);
			uint32_t expected_classifier;
			if (constructor_field_classifier_from_spine(
					&delta,
					terms,
					type_declarations,
					proof->rule_data.constructor.owner_view,
					proof->rule_data.constructor.constructor_index,
					previous_binders,
					proof->rule_data.constructor.field_index,
					proof->rule_data.constructor.field_index,
					&expected_classifier
				) != 0 ||
				!(prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					expected_classifier,
					relation->classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
				return -1;
			}
		return 0;
	}
	return 0;
}

static int validate_text_literal_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_TEXT_LITERAL) ||
		!term_is_primitive_text(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_int_literal_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_is_primitive_integer(terms, relation->classifier)) {
		return -1;
	}
	if (term_is_primitive_int(terms, relation->classifier) &&
		!int_literal_fits_int32(terms->terms[relation->subject].as.int_literal.value)) {
		return -1;
	}
	return 0;
}

static int validate_int_literal_admissibility_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof || proof->premise_count != 1 ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_is_primitive_int(terms, relation->classifier) ||
		!int_literal_fits_int32(
			terms->terms[relation->subject].as.int_literal.value
		) || proof->premises[0].proposition.subject != relation->subject ||
		!term_is_primitive_integer(terms, proof->premises[0].proposition.classifier)) {
		return -1;
	}
	return 0;
}

static int validate_return_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_RETURN) ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	const struct prototype_term* classifier = &terms->terms[relation->classifier];
	unsigned effects;
	return proof->premises[0].proposition.subject == terms->terms[relation->subject].as.return_term.value &&
		prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			proof->premises[0].proposition.classifier,
			classifier->as.computation_type.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL &&
		prototype_term_effect_row_closed_bits(
			terms, classifier->as.computation_type.label, &effects
		) == 0 && effects == PROTOTYPE_EFFECT_OPERATION_LABEL_NONE ? 0 : -1;
}

static int validate_thunk_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_THUNK) ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	return proof->premises[0].proposition.subject == terms->terms[relation->subject].as.thunk.computation &&
		prototype_judgement_classifier_conversion(
			terms,
			type_declarations,
			proof->premises[0].proposition.classifier,
			terms->terms[relation->classifier].as.thunk_type.computation
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 0 : -1;
}

static int validate_force_elim_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_FORCE) ||
		proof->premises[0].proposition.classifier >= terms->term_count ||
		terms->terms[proof->premises[0].proposition.classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	return proof->premises[0].proposition.subject == terms->terms[relation->subject].as.force.value &&
		relation->classifier ==
			terms->terms[proof->premises[0].proposition.classifier].as.thunk_type.computation ? 0 : -1;
}

static int computation_fold_carrier_compatible(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t left,
	uint32_t right
) {
	struct prototype_term_classifier_view left_view;
	struct prototype_term_classifier_view right_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, left, &left_view
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, right, &right_view
		) != 0 || left_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		right_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		left_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		right_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, left_view.result, right_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 0;
	}
	unsigned left_effects;
	unsigned right_effects;
	if (prototype_term_effect_row_closed_bits(
			terms, left_view.effect_row, &left_effects
		) != 0 || prototype_term_effect_row_closed_bits(
			terms, right_view.effect_row, &right_effects
		) != 0) {
		/* Partial-policy artifacts retain this equality as a residual obligation.
		 * Strict policy rejects the artifact until both rows are materialized. */
		return 1;
	}
	/* A return clause, operation clause, or resumption may use fewer effects
	 * than the fold carrier. The validator checks the exact union of all local
	 * rows against the result row after validating these inclusions. */
	return (left_effects & right_effects) == left_effects;
}

static int validate_computation_fold_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_COMPUTATION_FOLD) ||
		proof->premises[0].proposition.classifier >= terms->term_count ||
		proof->premises[1].proposition.classifier >= terms->term_count ||
		relation->classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* fold = &terms->terms[relation->subject];
	if (fold->as.computation_fold.clause_count >
			PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES ||
		proof->premise_count != 2 + 2 * fold->as.computation_fold.clause_count ||
		proof->premises[0].proposition.subject != fold->as.computation_fold.computation ||
		proof->premises[1].proposition.subject != fold->as.computation_fold.return_clause ||
		fold->as.computation_fold.first_clause + fold->as.computation_fold.clause_count >
			terms->computation_fold_clause_count) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		if (proof->premises[i].proposition.kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition.context_id != relation->context_id) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[fold->as.computation_fold.first_clause + i];
		if (proof->premises[2 + 2 * i].proposition.subject != clause->operation ||
			proof->premises[3 + 2 * i].proposition.subject != clause->body) {
			return -1;
		}
	}
	if (fold->as.computation_fold.clause_count != 0) {
		struct prototype_term_classifier_view input;
		struct prototype_term_classifier_view result;
		if (prototype_judgement_classifier_view(
				terms,
				type_declarations,
				NULL,
				proof->premises[0].proposition.classifier,
				&input
			) != 0 || prototype_judgement_classifier_view(
				terms,
				type_declarations,
				NULL,
				relation->classifier,
				&result
			) != 0 || input.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			result.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		uint32_t return_domain;
		uint32_t return_family;
		uint32_t return_binder;
		uint32_t return_body;
		struct prototype_term_classifier_view return_result;
		if (pi_parts(
				terms,
				proof->premises[1].proposition.classifier,
				&return_domain,
				&return_family
			) != 0 || prototype_term_pure_family_parts(
				terms, return_family, &return_binder, &return_body
			) != 0 || prototype_judgement_classifier_view(
				terms, type_declarations, NULL, return_body, &return_result
			) != 0 || return_result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			return_result.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			!(prototype_judgement_classifier_conversion(
				terms, type_declarations, return_domain, input.result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || !(prototype_judgement_classifier_conversion(
				terms, type_declarations, return_result.result, result.result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return -1;
		}
		(void)return_binder;
		unsigned handled_effects = 0;
		uint32_t clause_body_rows[
			PROTOTYPE_COMPUTATION_FOLD_MAX_OPERATION_CLAUSES
		];
		for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
			const struct prototype_computation_fold_clause* clause =
				&terms->computation_fold_clauses[
					fold->as.computation_fold.first_clause + i
				];
			int operation_identity;
			if (prototype_term_effect_operation_identity(
					terms, clause->operation, &operation_identity
				) != 0) {
				return -1;
			}
			const struct prototype_effect_operation_declaration* declaration =
				prototype_term_effect_operation_declaration(operation_identity);
			if (!declaration) {
				return -1;
			}
			handled_effects |= declaration->operation_labels;

			uint32_t operation_classifier = proof->premises[2 + 2 * i].proposition.classifier;
			int specialization_status = prototype_judgement_specialize_fold_operation_classifier(
				terms,
				type_declarations,
				input.effect_row,
				operation_identity,
				operation_classifier,
				&operation_classifier
			);
			if (specialization_status != 0) {
				return -1;
			}
			for (uint32_t depth = 0;
				depth < 32 && operation_classifier < terms->term_count &&
				terms->terms[operation_classifier].tag ==
					PROTOTYPE_TERM_EFFECT_ROW_FORALL;
				++depth) {
				operation_classifier =
					terms->terms[operation_classifier].as.effect_row_forall.body;
			}
			uint32_t operation_domain;
			uint32_t operation_family;
			uint32_t operation_binder;
			uint32_t operation_body;
			struct prototype_term_classifier_view operation_result;
			if (pi_parts(
					terms,
					operation_classifier,
					&operation_domain,
					&operation_family
				) != 0 || prototype_term_pure_family_parts(
					terms, operation_family, &operation_binder, &operation_body
				) != 0 || prototype_judgement_classifier_view(
					terms, type_declarations, NULL, operation_body, &operation_result
				) != 0 || operation_result.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				operation_result.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				return -1;
			}
			(void)operation_binder;

			uint32_t outer_domain;
			uint32_t outer_family;
			uint32_t outer_binder;
			uint32_t inner_classifier;
			uint32_t inner_domain;
			uint32_t inner_family;
			uint32_t inner_binder;
			uint32_t inner_body;
			if (pi_parts(
					terms,
					proof->premises[3 + 2 * i].proposition.classifier,
					&outer_domain,
					&outer_family
				) != 0 || prototype_term_pure_family_parts(
					terms, outer_family, &outer_binder, &inner_classifier
				) != 0 || pi_parts(
					terms,
					inner_classifier,
					&inner_domain,
					&inner_family
				) != 0 || prototype_term_pure_family_parts(
					terms, inner_family, &inner_binder, &inner_body
				) != 0 || inner_domain >= terms->term_count ||
				terms->terms[inner_domain].tag != PROTOTYPE_TERM_THUNK_TYPE) {
				return -1;
			}
			uint32_t instantiated_outer_domain;
			uint32_t instantiated_inner_body;
			if (instantiate_fold_clause_classifier(
					terms,
					type_declarations,
					outer_domain,
					operation_domain,
					outer_domain,
					&instantiated_outer_domain
				) != 0 || instantiate_fold_clause_classifier(
					terms,
					type_declarations,
					outer_domain,
					operation_domain,
					inner_body,
					&instantiated_inner_body
				) != 0 || !(prototype_judgement_classifier_conversion(
					terms, type_declarations, instantiated_outer_domain, operation_domain
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || !computation_fold_carrier_compatible(
					terms,
					type_declarations,
					instantiated_inner_body,
					relation->classifier
				)) {
				return -1;
			}
			inner_body = instantiated_inner_body;
			struct prototype_term_classifier_view clause_body_result;
			if (prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					inner_body,
					&clause_body_result
				) != 0 || clause_body_result.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				clause_body_result.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				return -1;
			}
			clause_body_rows[i] = clause_body_result.effect_row;
			uint32_t continuation_function =
				terms->terms[inner_domain].as.thunk_type.computation;
			uint32_t continuation_domain;
			uint32_t continuation_family;
			uint32_t continuation_binder;
			uint32_t continuation_body;
			if (pi_parts(
					terms,
					continuation_function,
					&continuation_domain,
					&continuation_family
				) != 0 || prototype_term_pure_family_parts(
					terms,
					continuation_family,
					&continuation_binder,
					&continuation_body
				) != 0 || !(prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					continuation_domain,
					operation_result.result
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) || !computation_fold_carrier_compatible(
					terms,
					type_declarations,
					continuation_body,
					relation->classifier
				)) {
				return -1;
			}
			(void)outer_binder;
			(void)inner_binder;
			(void)continuation_binder;
		}
		uint32_t residual_row;
		int residual_status = prototype_term_effect_row_residual(
			terms, input.effect_row, handled_effects, &residual_row
		);
		if (residual_status != 0) {
			return -1;
		}
		uint32_t expected_row;
		if (prototype_term_effect_row_union(
				terms, return_result.effect_row, residual_row, &expected_row
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
			uint32_t joined_row;
			if (prototype_term_effect_row_union(
					terms, expected_row, clause_body_rows[i], &joined_row
				) != 0) {
				return -1;
			}
			expected_row = joined_row;
		}
		int rows_equal = 0;
		return prototype_term_view_shape_equal(
			terms, expected_row, result.effect_row, &rows_equal
		) == 0 && rows_equal ? 0 : -1;
	}
	uint32_t expected_result;
	if (prototype_judgement_computation_fold_result_classifier(
			terms,
			type_declarations,
			fold->as.computation_fold.computation,
			proof->premises[0].proposition.classifier,
			proof->premises[1].proposition.classifier,
			&expected_result
		) != 0 || !(prototype_judgement_classifier_conversion(
			terms, type_declarations, relation->classifier, expected_result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return 0;
}

int prototype_judgement_add_force_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t value_claim_id,
	uint32_t* p_claim_id
) {
	uint32_t premise_ids[1] = { value_claim_id };
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!p_claim_id || initialize_exact_claim_rule(
			judgement,
			&conclusion,
			PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM,
			premise_ids,
			1,
			&proof,
			premises
		) != 0 || validate_force_elim_proof(
			terms, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement, &conclusion, &proof, premise_ids, p_claim_id
	);
}

int prototype_judgement_add_computation_fold_claim(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_claim_ids,
	uint32_t premise_count,
	uint32_t* p_claim_id
) {
	struct prototype_judgement_proposition conclusion = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = classifier
	};
	struct prototype_judgement_derivation_candidate proof;
	struct prototype_judgement_candidate_premise premises[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	if (!premise_claim_ids || !p_claim_id || premise_count == 0 ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		initialize_exact_claim_rule(
			judgement,
			&conclusion,
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
			premise_claim_ids,
			premise_count,
			&proof,
			premises
		) != 0 || validate_computation_fold_elim_proof(
			terms, type_declarations, &conclusion, &proof
		) != 0) {
		return -1;
	}
	return publish_complete_relation(
		judgement,
		&conclusion,
		&proof,
		premise_claim_ids,
		p_claim_id
	);
}

static int validate_operation_request_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || proof->premise_count != 3 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_OPERATION_REQUEST) ||
		proof->premises[1].proposition.subject !=
			terms->terms[relation->subject].as.operation_request.argument ||
		proof->premises[2].proposition.subject !=
			terms->terms[relation->subject].as.operation_request.continuation ||
		proof->premises[0].proposition.classifier >= terms->term_count ||
		proof->premises[1].proposition.classifier >= terms->term_count ||
		proof->premises[2].proposition.classifier >= terms->term_count ||
		relation->classifier >= terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		if (proof->premises[i].proposition.kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition.context_id != relation->context_id) {
			return -1;
		}
	}
	const struct prototype_term* request = &terms->terms[relation->subject];
	if (!term_has_tag(terms, proof->premises[0].proposition.subject, PROTOTYPE_TERM_APP) ||
		terms->terms[proof->premises[0].proposition.subject].as.app.function !=
			request->as.operation_request.operation ||
		terms->terms[proof->premises[0].proposition.subject].as.app.argument !=
			request->as.operation_request.argument) {
		return -1;
	}
	uint32_t operation_head =
		terms->terms[relation->subject].as.operation_request.operation;
	while (operation_head < terms->term_count &&
		terms->terms[operation_head].tag == PROTOTYPE_TERM_APP) {
		operation_head = terms->terms[operation_head].as.app.function;
	}
	if (operation_head >= terms->term_count ||
		terms->terms[operation_head].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	int operation_id = terms->terms[operation_head].as.effect_operation.operation_id;
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	if (!declaration) {
		return -1;
	}
	struct prototype_term_classifier_view operation;
	struct prototype_term_classifier_view result;
	if (prototype_term_classifier_view(terms, proof->premises[0].proposition.classifier, &operation) != 0 ||
		prototype_term_classifier_view(terms, relation->classifier, &result) != 0 ||
		operation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		result.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t continuation_thunk =
		terms->terms[relation->subject].as.operation_request.continuation;
	if (continuation_thunk >= terms->term_count ||
		terms->terms[continuation_thunk].tag != PROTOTYPE_TERM_THUNK ||
		terms->terms[continuation_thunk].as.thunk.computation >= terms->term_count) {
		return -1;
	}
	uint32_t continuation_lambda = terms->terms[continuation_thunk].as.thunk.computation;
	if (terms->terms[continuation_lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[proof->premises[2].proposition.classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t continuation_function_classifier =
		terms->terms[proof->premises[2].proposition.classifier].as.thunk_type.computation;
	uint32_t domain;
	uint32_t family;
	if (pi_parts(terms, continuation_function_classifier, &domain, &family) != 0 ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, domain, operation.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	uint32_t binder_var;
	uint32_t continuation_result;
	uint32_t continuation_context;
	uint32_t projection;
	if (prototype_term_var(
			terms, terms->terms[continuation_lambda].as.lambda.binding_id, &binder_var
		) != 0 ||
		prototype_context_extend(
			contexts,
			relation->context_id,
			terms->terms[continuation_lambda].as.lambda.binding_id,
			operation.result,
			PROTOTYPE_INVALID_ID,
			&continuation_context
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, continuation_context, &projection
		) != 0 ||
		instantiate_pure_family_in_context(
			contexts,
			substitutions,
			terms,
			type_declarations,
			projection,
			operation.result,
			family,
			binder_var,
			operation.result,
			&continuation_result
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view continuation;
	struct prototype_term_classifier_view request_operation = operation;
	if (declaration->classifier_schema ==
		PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT) {
		if (terms->terms[proof->premises[1].proposition.classifier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE) {
			return -1;
		}
		struct prototype_term_classifier_view argument;
		if (prototype_term_classifier_view(
				terms,
				terms->terms[proof->premises[1].proposition.classifier].as.thunk_type.computation,
				&argument
			) != 0 || argument.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			argument.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			prototype_term_effect_row_operation(
				terms, operation_id, argument.effect_row, &request_operation.effect_row
			) != 0) {
			return -1;
		}
	}
	if (prototype_term_classifier_view(terms, continuation_result, &continuation) != 0 ||
		continuation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!computation_effect_row_is_union(
			terms, type_declarations, &result, &request_operation, &continuation
		) ||
		result.result != continuation.result || prototype_term_contains_free_binding(
			terms, continuation.result, terms->terms[continuation_lambda].as.lambda.binding_id
		)) {
		return -1;
	}
	(void)family;
	return 0;
}


static int validate_text_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_primitive_text(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_int_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_primitive_integer(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_host_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_host_primitive(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int term_is_host_type_id(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	int host_type
) {
	if (!term_exists(terms, term_id)) {
		return 0;
	}
	int actual_host_type;
	return prototype_term_host_type_from_term_tag(
			terms->terms[term_id].tag,
			&actual_host_type
		) == 0 &&
		actual_host_type == host_type;
}

static int validate_host_signature_classifier(
	const struct prototype_term_db* terms,
	uint32_t classifier,
	uint32_t arity,
	const int* argument_types,
	int result_type,
	unsigned expected_effects
) {
	if (!terms || !argument_types || result_type == PROTOTYPE_HOST_TYPE_INVALID) {
		return -1;
	}
	uint32_t current = classifier;
	for (uint32_t i = 0; i < arity; ++i) {
		uint32_t domain;
		uint32_t codomain_family;
		uint32_t binding_id;
		uint32_t codomain;
		if (argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID ||
			pi_parts(terms, current, &domain, &codomain_family) != 0 ||
			prototype_term_pure_family_parts(
				terms, codomain_family, &binding_id, &codomain
			) != 0 ||
			!term_is_host_type_id(terms, domain, argument_types[i])) {
			return -1;
		}
		(void)binding_id;
		current = codomain;
	}
	if (!term_exists(terms, current) ||
		terms->terms[current].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	uint32_t label = terms->terms[current].as.computation_type.label;
	uint32_t result = terms->terms[current].as.computation_type.result;
	unsigned effects;
	if (!term_exists(terms, label) ||
		prototype_term_effect_row_closed_bits(terms, label, &effects) != 0 ||
		effects != expected_effects) {
		return -1;
	}
	return term_is_host_type_id(terms, result, result_type) ? 0 : -1;
}

static int validate_pure_primitive_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !type_declarations || !contexts || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_PURE_PRIMITIVE) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}

	const struct prototype_term* primitive = &terms->terms[relation->subject];
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t codomain_binder;
	uint32_t codomain;
	if (pi_parts(terms, relation->classifier, &domain, &codomain_family) != 0 ||
		prototype_term_pure_family_parts(
			terms, codomain_family, &codomain_binder, &codomain
		) != 0) {
		return -1;
	}
	const struct prototype_pure_primitive_declaration* signature =
		prototype_term_pure_primitive_declaration(primitive->as.pure_primitive.primitive_id);
	if (signature &&
		signature->result_type != PROTOTYPE_HOST_TYPE_INVALID) {
		int host_only = 1;
		for (uint32_t i = 0; i < signature->arity; ++i) {
			if (signature->argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID) {
				host_only = 0;
			}
		}
		if (host_only) {
			return validate_host_signature_classifier(
				terms,
				relation->classifier,
				signature->arity,
				signature->argument_types,
				signature->result_type,
				PROTOTYPE_EFFECT_OPERATION_LABEL_NONE
			);
		}
	}
	(void)codomain_binder;
	if (codomain >= terms->term_count ||
		terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}

	uint32_t effect_label = terms->terms[codomain].as.computation_type.label;
	uint32_t result = terms->terms[codomain].as.computation_type.result;
	unsigned effects;
	if (effect_label >= terms->term_count || result >= terms->term_count ||
		prototype_term_effect_row_closed_bits(terms, effect_label, &effects) != 0 ||
		effects != PROTOTYPE_EFFECT_OPERATION_LABEL_NONE) {
		return -1;
	}
	if (primitive->as.pure_primitive.primitive_id == PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT) {
		return term_is_primitive_text(terms, domain) &&
			type_formation_is_nat_shape(
				terms, type_declarations, contexts, result
			) &&
			type_formation_has_name_symbol(
				terms,
				type_declarations,
				result,
				primitive->as.pure_primitive.type_symbol_id
			) ? 0 : -1;
	}
	if (primitive->as.pure_primitive.primitive_id == PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT) {
		return type_formation_is_nat_shape(
				terms, type_declarations, contexts, domain
			) &&
			type_formation_has_name_symbol(
				terms,
				type_declarations,
				domain,
				primitive->as.pure_primitive.type_symbol_id
			) &&
			term_is_primitive_text(terms, result) ? 0 : -1;
	}
	return -1;
}

static int validate_reindex_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof,
	int require_projection
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(
			substitutions, proof ? proof->semantic_action_id : PROTOTYPE_INVALID_ID
		);
	uint32_t subject;
	uint32_t classifier;
	if (!contexts || !substitutions || !relation || !proof || !substitution ||
		(relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		 relation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition.kind != relation->kind ||
		proof->semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		substitution->source_context != relation->context_id ||
		substitution->target_context != proof->premises[0].proposition.context_id) {
		fprintf(
			stderr,
			"P0 reindex header failed action=%u relation-context=%u "
			"premise-context=%u substitution=%u->%u kind=%d action-kind=%d\n",
			proof ? proof->semantic_action_id : PROTOTYPE_INVALID_ID,
			relation ? relation->context_id : PROTOTYPE_INVALID_ID,
			proof && proof->premise_count != 0 ?
				proof->premises[0].proposition.context_id : PROTOTYPE_INVALID_ID,
			substitution ? substitution->source_context : PROTOTYPE_INVALID_ID,
			substitution ? substitution->target_context : PROTOTYPE_INVALID_ID,
			proof ? proof->premise_count : 0,
			proof ? proof->semantic_action_kind : -1
		);
		return -1;
	}
	if (require_projection) {
		return proof->premises[0].proposition.context_id != relation->context_id &&
			proof->premises[0].proposition.subject == relation->subject &&
			proof->premises[0].proposition.classifier == relation->classifier &&
			prototype_substitution_is_projection_path(
				substitutions,
				contexts,
				proof->semantic_action_id,
				relation->context_id,
				proof->premises[0].proposition.context_id
			) ? 0 : -1;
	}
	if (!terms || !type_declarations || prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			proof->premises[0].proposition.subject,
			proof->semantic_action_id,
			&subject
		) != 0 || prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			proof->premises[0].proposition.classifier,
			proof->semantic_action_id,
			&classifier
		) != 0) {
		fprintf(stderr, "P0 reindex evaluation failed action=%u\n", proof->semantic_action_id);
		return -1;
	}
	if (subject != relation->subject || classifier != relation->classifier) {
		fprintf(
			stderr,
			"P0 reindex result failed action=%u subject=%u/%u classifier=%u/%u\n",
			proof->semantic_action_id,
			subject,
			relation->subject,
			classifier,
			relation->classifier
		);
		return -1;
	}
	return 0;
}

static int validate_context_weaken_proof(
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	return validate_reindex_proof(
		NULL,
		NULL,
		contexts,
		(struct prototype_substitution_db*)substitutions,
		relation,
		proof,
		1
	);
}

static int validate_substitution_reindex_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	return validate_reindex_proof(
		terms,
		type_declarations,
		contexts,
		substitutions,
		relation,
		proof,
		0
	);
}

static int validate_relation_type_formation_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	uint32_t left_classifier;
	uint32_t right_classifier;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premise_count != 4 ||
		prototype_term_relation_type_info(
			terms,
			relation->subject,
			&left_classifier,
			&right_classifier,
			&left_endpoint,
			&right_endpoint
		) != 0) {
		return -1;
	}
	return proof->premises[0].proposition.kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
		proof->premises[0].proposition.context_id == relation->context_id &&
		proof->premises[0].proposition.subject == left_classifier &&
		proof->premises[0].proposition.classifier == relation->classifier &&
		proof->premises[1].proposition.kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
		proof->premises[1].proposition.context_id == relation->context_id &&
		proof->premises[1].proposition.subject == right_classifier &&
		proof->premises[1].proposition.classifier == relation->classifier &&
		proof->premises[2].proposition.kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[2].proposition.context_id == relation->context_id &&
		proof->premises[2].proposition.subject == left_endpoint &&
		proof->premises[2].proposition.classifier == left_classifier &&
		proof->premises[3].proposition.kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
		proof->premises[3].proposition.context_id == relation->context_id &&
		proof->premises[3].proposition.subject == right_endpoint &&
		proof->premises[3].proposition.classifier == right_classifier ? 0 : -1;
}

static int validate_relation_witness_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	struct prototype_term_conversion_result comparison;
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 3 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left ||
		witness_right != relation_right ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition.context_id != relation->context_id ||
		proof->premises[0].proposition.subject != relation->classifier ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.context_id != relation->context_id ||
		proof->premises[1].proposition.subject != relation_left ||
		proof->premises[1].proposition.classifier != left_type ||
		proof->premises[2].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition.context_id != relation->context_id ||
		proof->premises[2].proposition.subject != relation_right ||
		proof->premises[2].proposition.classifier != right_type ||
		prototype_term_compare_for_conversion(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			relation_left,
			relation_right,
			64,
			&comparison
		) != 0 || comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	return 0;
}

static int validate_relation_constructor_witness_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count < 3) {
		return -1;
	}
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t left_head;
	uint32_t left_owner;
	uint32_t left_constructor;
	uint32_t left_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t left_argument_count;
	uint32_t right_head;
	uint32_t right_owner;
	uint32_t right_constructor;
	uint32_t right_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t right_argument_count;
	uint32_t field_count = proof->premise_count - 3;
	if (prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition.context_id != relation->context_id ||
		proof->premises[0].proposition.subject != relation->classifier ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.context_id != relation->context_id ||
		proof->premises[1].proposition.subject != relation_left ||
		proof->premises[1].proposition.classifier != left_type ||
		proof->premises[2].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition.context_id != relation->context_id ||
		proof->premises[2].proposition.subject != relation_right ||
		proof->premises[2].proposition.classifier != right_type ||
		prototype_term_constructor_spine_info(
			terms,
			relation_left,
			&left_head,
			&left_owner,
			&left_constructor,
			left_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&left_argument_count
		) != 0 || prototype_term_constructor_spine_info(
			terms,
			relation_right,
			&right_head,
			&right_owner,
			&right_constructor,
			right_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&right_argument_count
		) != 0 || left_owner != right_owner ||
		left_constructor != right_constructor || left_argument_count != field_count ||
		right_argument_count != field_count) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		uint32_t field_left_type;
		uint32_t field_right_type;
		uint32_t field_left;
		uint32_t field_right;
		uint32_t premise = i + 3;
		if (proof->premises[premise].proposition.kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[premise].proposition.context_id != relation->context_id ||
			prototype_term_relation_type_info(
				terms,
				proof->premises[premise].proposition.classifier,
				&field_left_type,
				&field_right_type,
				&field_left,
				&field_right
			) != 0 || field_left != left_arguments[i] ||
			field_right != right_arguments[i]) {
			return -1;
		}
	}
	(void)left_head;
	(void)right_head;
	return 0;
}

static int validate_relation_unary_witness_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof,
	int endpoint_tag
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t child_left_type;
	uint32_t child_right_type;
	uint32_t child_left;
	uint32_t child_right;
	uint32_t left_payload;
	uint32_t right_payload;
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 4 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != endpoint_tag ||
		terms->terms[relation_right].tag != endpoint_tag ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition.context_id != relation->context_id ||
		proof->premises[0].proposition.subject != relation->classifier ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.context_id != relation->context_id ||
		proof->premises[1].proposition.subject != relation_left ||
		proof->premises[1].proposition.classifier != left_type ||
		proof->premises[2].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition.context_id != relation->context_id ||
		proof->premises[2].proposition.subject != relation_right ||
		proof->premises[2].proposition.classifier != right_type ||
		proof->premises[3].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[3].proposition.context_id != relation->context_id ||
		prototype_term_relation_type_info(
			terms,
			proof->premises[3].proposition.classifier,
			&child_left_type,
			&child_right_type,
			&child_left,
			&child_right
		) != 0) {
		return -1;
	}
	left_payload = endpoint_tag == PROTOTYPE_TERM_RETURN ?
		terms->terms[relation_left].as.return_term.value :
		terms->terms[relation_left].as.thunk.computation;
	right_payload = endpoint_tag == PROTOTYPE_TERM_RETURN ?
		terms->terms[relation_right].as.return_term.value :
		terms->terms[relation_right].as.thunk.computation;
	return child_left == left_payload && child_right == right_payload ? 0 : -1;
}

static int validate_relation_app_witness_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 5 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_APP ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_APP ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition.context_id != relation->context_id ||
		proof->premises[0].proposition.subject != relation->classifier ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.context_id != relation->context_id ||
		proof->premises[1].proposition.subject != relation_left ||
		proof->premises[1].proposition.classifier != left_type ||
		proof->premises[2].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition.context_id != relation->context_id ||
		proof->premises[2].proposition.subject != relation_right ||
		proof->premises[2].proposition.classifier != right_type) {
		return -1;
	}
	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t child_left_type;
		uint32_t child_right_type;
		uint32_t child_left;
		uint32_t child_right;
		uint32_t premise = i + 3;
		uint32_t expected_left = i == 0 ?
			terms->terms[relation_left].as.app.function :
			terms->terms[relation_left].as.app.argument;
		uint32_t expected_right = i == 0 ?
			terms->terms[relation_right].as.app.function :
			terms->terms[relation_right].as.app.argument;
		if (proof->premises[premise].proposition.kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[premise].proposition.context_id != relation->context_id ||
			prototype_term_relation_type_info(
				terms,
				proof->premises[premise].proposition.classifier,
				&child_left_type,
				&child_right_type,
				&child_left,
				&child_right
			) != 0 || child_left != expected_left ||
			child_right != expected_right) {
			return -1;
		}
	}
	return 0;
}

static int validate_relation_lambda_witness_proof(
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	if (!terms || !contexts || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 4 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition.context_id != relation->context_id ||
		proof->premises[0].proposition.subject != relation->classifier ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.context_id != relation->context_id ||
		proof->premises[1].proposition.subject != relation_left ||
		proof->premises[1].proposition.classifier != left_type ||
		proof->premises[2].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition.context_id != relation->context_id ||
		proof->premises[2].proposition.subject != relation_right ||
		proof->premises[2].proposition.classifier != right_type ||
		proof->premises[3].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!relation_lambda_witness_shape_valid(
			terms,
			contexts,
			relation->context_id,
			relation->subject,
			relation->classifier,
			proof->premises[3].proposition.context_id,
			proof->premises[3].proposition.classifier
		)) {
		return -1;
	}
	return 0;
}

static int validate_relation_match_witness_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	if (!terms || !type_declarations || !contexts || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count < 5 ||
		prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation_left].as.match.case_count != proof->premise_count - 5 ||
		terms->terms[relation_right].as.match.case_count != proof->premise_count - 5 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition.context_id != relation->context_id ||
		proof->premises[0].proposition.subject != relation->classifier ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.context_id != relation->context_id ||
		proof->premises[1].proposition.subject != relation_left ||
		proof->premises[1].proposition.classifier != left_type ||
		proof->premises[2].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition.context_id != relation->context_id ||
		proof->premises[2].proposition.subject != relation_right ||
		proof->premises[2].proposition.classifier != right_type ||
		proof->premises[3].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[3].proposition.subject >= terms->term_count ||
		terms->terms[proof->premises[3].proposition.subject].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[proof->premises[3].proposition.subject].as.match.case_count !=
			proof->premise_count - 5) {
		return -1;
	}
	for (uint32_t i = 4; i < proof->premise_count; ++i) {
		uint32_t child_left_type;
		uint32_t child_right_type;
		uint32_t child_left;
		uint32_t child_right;
		uint32_t expected_left;
		uint32_t expected_right;
		if (i == 4) {
			expected_left = terms->terms[relation_left].as.match.scrutinee;
			expected_right = terms->terms[relation_right].as.match.scrutinee;
		} else {
			uint32_t case_index = i - 5;
			uint32_t source_case =
				terms->terms[proof->premises[3].proposition.subject].as.match.first_case +
				case_index;
			uint32_t left_case =
				terms->terms[relation_left].as.match.first_case + case_index;
			uint32_t right_case =
				terms->terms[relation_right].as.match.first_case + case_index;
			struct prototype_judgement_proposition case_proposition =
				proof->premises[i].proposition;
			if (source_case >= terms->case_count || left_case >= terms->case_count ||
				right_case >= terms->case_count ||
				!relation_match_case_witness_shape_valid(
					terms,
					type_declarations,
					contexts,
					relation->context_id,
					&terms->cases[source_case],
					&terms->cases[left_case],
					&terms->cases[right_case],
					&case_proposition
				)) {
				return -1;
			}
			continue;
		}
		if (proof->premises[i].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premises[i].proposition.context_id != relation->context_id ||
			prototype_term_relation_type_info(
				terms,
				proof->premises[i].proposition.classifier,
				&child_left_type,
				&child_right_type,
				&child_left,
				&child_right
			) != 0 || child_left != expected_left || child_right != expected_right) {
			return -1;
		}
	}
	return 0;
}

static int validate_relation_induction_hypothesis_witness_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	uint32_t witness_left;
	uint32_t witness_right;
	uint32_t left_type;
	uint32_t right_type;
	uint32_t relation_left;
	uint32_t relation_right;
	uint32_t argument_left_type;
	uint32_t argument_right_type;
	uint32_t argument_left;
	uint32_t argument_right;
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 5 || prototype_term_relation_witness_info(
			terms, relation->subject, &witness_left, &witness_right
		) != 0 || prototype_term_relation_type_info(
			terms,
			relation->classifier,
			&left_type,
			&right_type,
			&relation_left,
			&relation_right
		) != 0 || witness_left != relation_left || witness_right != relation_right ||
		relation_left >= terms->term_count || relation_right >= terms->term_count ||
		terms->terms[relation_left].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		terms->terms[relation_right].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premises[0].proposition.context_id != relation->context_id ||
		proof->premises[0].proposition.subject != relation->classifier ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.context_id != relation->context_id ||
		proof->premises[1].proposition.subject != relation_left ||
		proof->premises[1].proposition.classifier != left_type ||
		proof->premises[2].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition.context_id != relation->context_id ||
		proof->premises[2].proposition.subject != relation_right ||
		proof->premises[2].proposition.classifier != right_type ||
		proof->premises[3].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[3].proposition.subject >= terms->term_count ||
		terms->terms[proof->premises[3].proposition.subject].tag !=
			PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		proof->premises[4].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[4].proposition.context_id != relation->context_id ||
		prototype_term_relation_type_info(
			terms,
			proof->premises[4].proposition.classifier,
			&argument_left_type,
			&argument_right_type,
			&argument_left,
			&argument_right
		) != 0 || argument_left != terms->terms[relation_left].
			as.induction_hypothesis.argument || argument_right !=
			terms->terms[relation_right].as.induction_hypothesis.argument) {
		return -1;
	}
	return 0;
}

static int validate_is_type_from_has_type_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premise_count != 1 ||
		proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition.subject != relation->subject ||
		proof->premises[0].proposition.classifier != relation->classifier ||
		!term_exists(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_pi_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 3 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_PI) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	uint32_t domain;
	uint32_t codomain_family;
	if (pi_parts(terms, relation->subject, &domain, &codomain_family) != 0) {
		return -1;
	}
	uint32_t family = terms->terms[relation->subject].as.pi.codomain_family;
	uint32_t binding_id;
	uint32_t codomain;
	if (prototype_term_pure_family_parts(
			terms, family, &binding_id, &codomain
		) != 0 || family >= terms->term_count ||
		terms->terms[family].tag != PROTOTYPE_TERM_THUNK) {
		return -1;
	}
	uint32_t lambda = terms->terms[family].as.thunk.computation;
	if (lambda >= terms->term_count ||
		terms->terms[lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[lambda].as.lambda.binding_id != binding_id ||
		terms->terms[lambda].as.lambda.body >= terms->term_count ||
		terms->terms[terms->terms[lambda].as.lambda.body].tag != PROTOTYPE_TERM_RETURN ||
		terms->terms[terms->terms[lambda].as.lambda.body].as.return_term.value != codomain ||
		proof->premises[2].proposition.classifier >= terms->term_count ||
		terms->terms[proof->premises[2].proposition.classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t lambda_classifier =
		terms->terms[proof->premises[2].proposition.classifier].as.thunk_type.computation;
	if (lambda_classifier >= terms->term_count ||
		terms->terms[lambda_classifier].tag != PROTOTYPE_TERM_PI ||
		terms->terms[lambda_classifier].as.pi.domain != domain) {
		return -1;
	}
	uint32_t lambda_result_binder;
	uint32_t lambda_result_body;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[lambda_classifier].as.pi.codomain_family,
			&lambda_result_binder,
			&lambda_result_body
		) != 0) {
		return -1;
	}
	uint32_t binder_var;
	uint32_t returned_classifier;
	if (prototype_term_var(
			(struct prototype_term_db*)terms, binding_id, &binder_var
		) != 0 || prototype_term_graph_substitute_bound_var(
			(struct prototype_term_db*)terms,
			NULL,
			lambda_result_body,
			lambda_result_binder,
			binder_var,
			&returned_classifier
		) != 0 || returned_classifier >= terms->term_count ||
		terms->terms[returned_classifier].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	unsigned effects;
	uint32_t effect_row = terms->terms[returned_classifier].as.computation_type.label;
	if (prototype_term_effect_row_closed_bits(terms, effect_row, &effects) != 0 ||
		effects != PROTOTYPE_EFFECT_OPERATION_LABEL_NONE ||
		terms->terms[returned_classifier].as.computation_type.result != relation->classifier) {
		return -1;
	}
	if (proof->premises[0].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[0].proposition.subject != domain ||
		proof->premises[0].proposition.classifier != relation->classifier ||
		proof->premises[1].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[1].proposition.subject != codomain ||
		proof->premises[1].proposition.classifier != relation->classifier ||
		proof->premises[2].proposition.kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premises[2].proposition.subject != family ||
		proof->premises[2].proposition.classifier >= terms->term_count ||
		terms->terms[proof->premises[2].proposition.classifier].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		terms->terms[proof->premises[2].proposition.classifier].as.thunk_type.computation != lambda_classifier) {
		return -1;
	}
	(void)codomain_family;
	return 0;
}

static int validate_effect_operation_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_EFFECT_OPERATION) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* operation = &terms->terms[relation->subject];
	if (!prototype_term_effect_operation_declaration(
			operation->as.effect_operation.operation_id
		) || operation->as.effect_operation.classifier >= terms->term_count) {
		return -1;
	}
	return relation->classifier == operation->as.effect_operation.classifier ? 0 : -1;
}

static int accepted_derivation_replay_view(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation* derivation,
	struct prototype_judgement_proposition* relation,
	struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* premises
) {
	if (!judgement || !derivation || !relation || !proof || !premises ||
		derivation->conclusion_claim_id >= judgement->claim_count ||
		derivation->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	const struct prototype_judgement_claim* conclusion =
		&judgement->claims[derivation->conclusion_claim_id];
	memset(relation, 0, sizeof(*relation));
	*relation = *prototype_judgement_proposition_get(judgement, conclusion->proposition_id);

	memset(proof, 0, sizeof(*proof));
	memset(
		premises,
		0,
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES * sizeof(*premises)
	);
	proof->premises = premises;
	proof->proof_kind = derivation->proof_kind;
	proof->conclusion_kind = prototype_judgement_proposition_get(judgement, conclusion->proposition_id)->kind;
	proof->conclusion_context_id = prototype_judgement_proposition_get(judgement, conclusion->proposition_id)->context_id;
	proof->conclusion_operation_id = prototype_judgement_proposition_get(judgement, conclusion->proposition_id)->operation_id;
	proof->conclusion_subject = prototype_judgement_proposition_get(judgement, conclusion->proposition_id)->subject;
	proof->conclusion_classifier = prototype_judgement_proposition_get(judgement, conclusion->proposition_id)->classifier;
	proof->rule_data = derivation->rule_data;
	proof->semantic_action_kind = derivation->semantic_action_kind;
	proof->semantic_action_id = derivation->semantic_action_id;
	proof->premise_count = derivation->premise_count;
	for (uint32_t i = 0; i < derivation->premise_count; ++i) {
		proof->premises[i].semantic_action_kind =
			derivation->premises[i].semantic_action_kind;
		proof->premises[i].semantic_action_id =
			derivation->premises[i].semantic_action_id;
		uint32_t claim_id = derivation->premises[i].claim_id;
		if (claim_id == PROTOTYPE_INVALID_ID) {
			const struct prototype_judgement_proposition* scoped =
				prototype_judgement_premise_proposition(
					judgement, &derivation->premises[i]
				);
			if (!scoped) {
				fprintf(stderr, "P0 missing scoped premise index=%u\n", i);
				return -1;
			}
			proof->premises[i].proposition = *scoped;
			proof->premises[i].proposition.authority_kind =
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID;
			proof->premises[i].proposition.authority_id = PROTOTYPE_INVALID_ID;
			proof->premises[i].proposition.operation_id = PROTOTYPE_INVALID_ID;
			continue;
		}
		if (claim_id >= judgement->claim_count) {
			fprintf(
				stderr,
				"P0 invalid accepted premise index=%u claim=%u count=%zu\n",
				i,
				claim_id,
				judgement->claim_count
			);
			return -1;
		}
		const struct prototype_judgement_claim* premise =
			&judgement->claims[claim_id];
		proof->premises[i].proposition.kind = prototype_judgement_proposition_get(judgement, premise->proposition_id)->kind;
		proof->premises[i].proposition.context_id = prototype_judgement_proposition_get(judgement, premise->proposition_id)->context_id;
		proof->premises[i].proposition.subject = prototype_judgement_proposition_get(judgement, premise->proposition_id)->subject;
		proof->premises[i].proposition.classifier = prototype_judgement_proposition_get(judgement, premise->proposition_id)->classifier;
		proof->premises[i].proposition.authority_kind = prototype_judgement_proposition_get(judgement, premise->proposition_id)->authority_kind;
		proof->premises[i].proposition.authority_id = prototype_judgement_proposition_get(judgement, premise->proposition_id)->authority_id;
		proof->premises[i].proposition.operation_id = prototype_judgement_proposition_get(judgement, premise->proposition_id)->operation_id;
	}
	return 0;
}

static int validate_proof_relation_coverage(
	const struct prototype_judgement_db* judgement
) {
	if (!judgement) {
		return -1;
	}
	for (uint32_t claim_id = 0;
		claim_id < (uint32_t)judgement->claim_count; ++claim_id) {
		if (judgement->claims[claim_id].closure_rank == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (!prototype_judgement_claim_proposition(judgement, claim_id)) {
			continue;
		}
		int found = 0;
		for (uint32_t derivation_id = 0;
			derivation_id < (uint32_t)judgement->derivation_count;
			++derivation_id) {
			const struct prototype_judgement_derivation* derivation =
				&judgement->derivations[derivation_id];
			if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
				continue;
			}
			if (derivation->conclusion_claim_id != claim_id) {
				continue;
			}
			if (derivation->closure_rank == PROTOTYPE_INVALID_ID) {
				continue;
			}
			if (derivation->closure_rank < judgement->claims[claim_id].closure_rank) {
				return -1;
			}
			for (uint32_t premise = 0;
				premise < derivation->premise_count; ++premise) {
				uint32_t source = derivation->premises[premise].claim_id;
				if (source != PROTOTYPE_INVALID_ID &&
					(source >= judgement->claim_count ||
					 judgement->claims[source].closure_rank >=
						derivation->closure_rank)) {
					return -1;
				}
			}
			found = 1;
		}
		if (!found) {
			fprintf(
				stderr,
				"P0 grounded claim lacks derivation claim=%u rank=%u proposition=%u\n",
				claim_id,
				judgement->claims[claim_id].closure_rank,
				judgement->claims[claim_id].proposition_id
			);
			return -1;
		}
	}
	return 0;
}

static int validate_proof_rule_parameters(
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!proof) {
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN ||
		proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX) {
		if (proof->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			proof->semantic_action_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	} else if (proof->semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID ||
		proof->semantic_action_id != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
		return proof->rule_data.constructor.owner_view != PROTOTYPE_INVALID_ID &&
			proof->rule_data.constructor.constructor_index != PROTOTYPE_INVALID_ID &&
			proof->rule_data.constructor.field_index != PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
		return proof->rule_data.induction.match != PROTOTYPE_INVALID_ID &&
			proof->rule_data.induction.motive != PROTOTYPE_INVALID_ID &&
			proof->rule_data.induction.case_index != PROTOTYPE_INVALID_ID &&
			proof->rule_data.induction.field_index != PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->proof_kind ==
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
		return proof->rule_data.constructor.owner_view != PROTOTYPE_INVALID_ID &&
			proof->rule_data.constructor.constructor_index == PROTOTYPE_INVALID_ID &&
			proof->rule_data.constructor.field_index == PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->rule_data.induction.match != PROTOTYPE_INVALID_ID ||
		proof->rule_data.induction.motive != PROTOTYPE_INVALID_ID ||
		proof->rule_data.induction.case_index != PROTOTYPE_INVALID_ID ||
		proof->rule_data.induction.field_index != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	return 0;
}

static int judgement_classifier_free_result_effect_row_binder(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_binder_id
) {
	if (!terms || !type_declarations || !p_binder_id ||
		classifier >= terms->term_count) {
		return -1;
	}
	*p_binder_id = PROTOTYPE_INVALID_ID;
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, classifier, &view
		) != 0 ||
		view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	if (view.effect_row >= terms->term_count ||
		terms->terms[view.effect_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return 0;
	}
	uint32_t binding_id = terms->terms[view.effect_row].as.effect_row_var.binding_id;
	if (!prototype_term_contains_free_binding(
		terms,
		classifier,
		binding_id
	)) {
		return 0;
	}
	*p_binder_id = binding_id;
	return 1;
}

static int judgement_claim_has_effect_row_assumption(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t claim_id,
	uint32_t binding_id,
	size_t depth
) {
	if (!judgement || !terms || claim_id >= judgement->claim_count ||
		depth > judgement->claim_count) {
		return -1;
	}
	const struct prototype_judgement_claim* claim = &judgement->claims[claim_id];
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id != claim_id) {
			continue;
		}
		if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
			if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier < terms->term_count &&
				prototype_term_contains_free_binding(
					terms, prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier, binding_id
				)) {
				return 1;
			}
			continue;
		}
		for (uint32_t source = 0;
			source < derivation->premise_count; ++source) {
			uint32_t source_claim_id = derivation->premises[source].claim_id;
			if (source_claim_id == PROTOTYPE_INVALID_ID) {
				continue;
			}
			int found = judgement_claim_has_effect_row_assumption(
				judgement,
				terms,
				source_claim_id,
				binding_id,
				depth + 1
			);
			if (found != 0) {
				return found;
			}
		}
	}
	return 0;
}

static int judgement_derivation_has_effect_row_assumption(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_derivation* derivation,
	uint32_t binding_id
) {
	if (!judgement || !terms || !derivation) {
		return -1;
	}
	for (uint32_t source = 0;
		source < derivation->premise_count; ++source) {
		uint32_t source_claim_id = derivation->premises[source].claim_id;
		if (source_claim_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int found = judgement_claim_has_effect_row_assumption(
			judgement,
			terms,
			source_claim_id,
			binding_id,
			0
		);
		if (found != 0) {
			return found;
		}
	}
	return 0;
}

static int validate_operation_owned_relation(
	const struct prototype_operation_graph* operations,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_proposition* relation,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!relation || !proof) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		const struct prototype_judgement_candidate_premise* premise =
			&proof->premises[i];
		if (premise->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID) {
			if (premise->semantic_action_id != PROTOTYPE_INVALID_ID) {
				return -1;
			}
			continue;
		}
		const struct prototype_substitution* action =
			premise->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ?
			prototype_substitution_get(
				substitutions, premise->semantic_action_id
			) : NULL;
		if (!action || action->source_context !=
				premise->proposition.context_id) {
			return -1;
		}
	}
	if (relation->operation_id == PROTOTYPE_INVALID_ID) {
		return proof->conclusion_operation_id == PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (!operations || relation->operation_id >= operations->operation_count ||
		proof->conclusion_operation_id != relation->operation_id) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[relation->operation_id];
	int context_matches = operation->context_id == relation->context_id;
	int weakening_action_matches = 0;
	if (!context_matches &&
		proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN &&
		proof->premise_count == 1 &&
		proof->premises[0].proposition.operation_id == relation->operation_id) {
		weakening_action_matches = prototype_substitution_is_projection_path(
			substitutions,
			contexts,
			proof->semantic_action_id,
			relation->context_id,
			proof->premises[0].proposition.context_id
		);
		context_matches = weakening_action_matches;
	}
	if (operation->core_term != relation->subject ||
		operation->classifier == PROTOTYPE_INVALID_ID ||
		!context_matches) {
		const struct prototype_substitution* semantic_substitution =
			proof->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ?
			prototype_substitution_get(
				substitutions, proof->semantic_action_id
			) : NULL;
		fprintf(
			stderr,
			"P0 operation owner mismatch operation=%u tag=%d core=%u subject=%u "
			"classifier=%u relation_classifier=%u context=%u relation_context=%u "
			"rule=%d premise_context=%u premise_operation=%u action=%d:%u valid=%d "
			"action_context=%u->%u action_tag=%d\n",
			relation->operation_id,
			operation->tag,
			operation->core_term,
			relation->subject,
			operation->classifier,
			relation->classifier,
			operation->context_id,
			relation->context_id,
			proof->proof_kind,
			proof->premise_count != 0 ? proof->premises[0].proposition.context_id :
				PROTOTYPE_INVALID_ID,
			proof->premise_count != 0 ? proof->premises[0].proposition.operation_id :
				PROTOTYPE_INVALID_ID,
			proof->semantic_action_kind,
			proof->semantic_action_id,
			weakening_action_matches,
			semantic_substitution ? semantic_substitution->source_context :
				PROTOTYPE_INVALID_ID,
			semantic_substitution ? semantic_substitution->target_context :
				PROTOTYPE_INVALID_ID,
			semantic_substitution ? semantic_substitution->kind : 0
		);
		return -1;
	}
	/* Literal overload and the explicitly derived rules can expose a classifier
	 * other than the selected solver result without changing that result. */
	if (judgement_rule_derives_alternate_operation_classifier(proof->proof_kind)) {
		return 0;
	}
	if (operation->classifier != relation->classifier) {
		fprintf(
			stderr,
			"P0 operation classifier mismatch operation=%u classifier=%u "
			"relation_classifier=%u rule=%d\n",
			relation->operation_id,
			operation->classifier,
			relation->classifier,
			proof->proof_kind
		);
		return -1;
	}
	return 0;
}

static int operation_core_projection_equal(
	const struct prototype_term_db* terms,
	const struct prototype_operation_graph* operations,
	uint32_t operation_id,
	uint32_t projected_core
) {
	if (!terms || !operations || operation_id >= operations->operation_count ||
		projected_core >= terms->term_count) {
		return 0;
	}
	uint32_t operation_core = operations->operations[operation_id].core_term;
	if (operation_core == projected_core) {
		return 1;
	}
	int equal = 0;
	return operation_core < terms->term_count &&
		prototype_term_core_shape_equal(
			terms, operation_core, projected_core, &equal
		) == 0 && equal;
}

static int term_core_projection_equal(
	const struct prototype_term_db* terms,
	uint32_t left,
	uint32_t right
) {
	if (!terms || left >= terms->term_count || right >= terms->term_count) {
		return 0;
	}
	if (left == right) {
		return 1;
	}
	int equal = 0;
	return prototype_term_core_shape_equal(
		terms, left, right, &equal
	) == 0 && equal;
}

static int judgement_operation_expected_polarity(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	uint32_t operation_id,
	int* p_polarity
) {
	if (!terms || !type_declarations || !operations || !p_polarity ||
		operation_id >= operations->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[operation_id];
	if (operation->classifier >= terms->term_count ||
		operation->core_term >= terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view classifier_view;
	if (prototype_judgement_classifier_view(
			terms,
			type_declarations,
			NULL,
			operation->classifier,
			&classifier_view
		) != 0) {
		return -1;
	}
	int polarity = classifier_view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION ?
		PROTOTYPE_OPERATION_POLARITY_COMPUTATION :
		PROTOTYPE_OPERATION_POLARITY_VALUE;
	int core_tag = terms->terms[operation->core_term].tag;
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
		case PROTOTYPE_OPERATION_THUNK:
			polarity = PROTOTYPE_OPERATION_POLARITY_VALUE;
			break;
		case PROTOTYPE_OPERATION_LAMBDA:
		case PROTOTYPE_OPERATION_MATCH:
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_FORCE:
		case PROTOTYPE_OPERATION_REQUEST:
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			polarity = PROTOTYPE_OPERATION_POLARITY_COMPUTATION;
			break;
		case PROTOTYPE_OPERATION_ATOM:
			if (core_tag == PROTOTYPE_TERM_TYPE_VIEW ||
				core_tag == PROTOTYPE_TERM_TYPE_FORMER ||
				core_tag == PROTOTYPE_TERM_TYPE_DECLARATION) {
				polarity = PROTOTYPE_OPERATION_POLARITY_VALUE;
			}
			break;
		case PROTOTYPE_OPERATION_APP:
			if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
				polarity = PROTOTYPE_OPERATION_POLARITY_VALUE;
			} else if (operation->application_role ==
				PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION) {
				polarity = PROTOTYPE_OPERATION_POLARITY_COMPUTATION;
			} else {
				return -1;
			}
			break;
		case PROTOTYPE_OPERATION_NAME:
		case PROTOTYPE_OPERATION_ASCRIPTION: {
			uint32_t source_operation = operation->tag == PROTOTYPE_OPERATION_NAME ?
				operation->function : operation->body;
			if (source_operation >= operations->operation_count ||
				judgement_operation_expected_polarity(
					terms,
					type_declarations,
					operations,
					source_operation,
					&polarity
				) != 0) {
				return -1;
			}
			break;
		}
		case PROTOTYPE_OPERATION_VAR:
			break;
		default:
			return -1;
	}
	*p_polarity = polarity;
	return 0;
}

int prototype_judgement_validate_operation_typing(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	const struct prototype_operation_graph* operations,
	uint32_t operation_id
) {
	if (!terms || !type_declarations || !contexts || !operations ||
		operation_id >= operations->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[operation_id];
	if (operation->context_id >= contexts->context_count ||
		operation->core_term >= terms->term_count ||
		(operation->classifier != PROTOTYPE_INVALID_ID &&
		 operation->classifier >= terms->term_count)) {
		return -1;
	}
	const struct prototype_term* core = &terms->terms[operation->core_term];
	switch (operation->tag) {
		case PROTOTYPE_OPERATION_ATOM:
			return 0;
	case PROTOTYPE_OPERATION_VAR: {
			if (core->tag != PROTOTYPE_TERM_VAR) {
				return -1;
			}
			uint32_t binding_context;
			if (prototype_context_find_binding(
					contexts,
					operation->context_id,
					core->as.var.binding_id,
					&binding_context
				) != 0) {
				return -1;
			}
			const struct prototype_context* binding =
				prototype_context_get(contexts, binding_context);
			uint32_t binding_classifier =
				prototype_context_classifier_term(binding);
			if (!binding || binding_classifier == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			int valid = operation->classifier == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					terms,
					type_declarations,
					binding_classifier,
					operation->classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
			return valid ? 0 : -1;
		}
		case PROTOTYPE_OPERATION_NAME:
			return operation->function < operations->operation_count &&
				operation_core_projection_equal(
					terms, operations, operation->function, operation->core_term
				) ? 0 : -1;
		case PROTOTYPE_OPERATION_CONSTRUCTOR:
			return core->tag == PROTOTYPE_TERM_CONSTRUCTOR &&
				constructor_belongs_to_owner(
					terms,
					type_declarations,
					core->as.constructor.owner,
					core->as.constructor.constructor_id
				) && classifier_returns_owner(
					terms,
					type_declarations,
					operation->classifier,
					core->as.constructor.owner
				) ? 0 : -1;
		case PROTOTYPE_OPERATION_APP:
			return core->tag == PROTOTYPE_TERM_APP &&
				operation->function < operations->operation_count &&
				operation->argument < operations->operation_count &&
				operation_core_projection_equal(
					terms, operations, operation->function, core->as.app.function
				) && operation_core_projection_equal(
					terms, operations, operation->argument, core->as.app.argument
				) ? 0 : -1;
		case PROTOTYPE_OPERATION_LAMBDA:
			if (core->tag != PROTOTYPE_TERM_LAMBDA ||
				operation->body >= operations->operation_count ||
				operation->binding_id == PROTOTYPE_INVALID_ID ||
				operation->binder_classifier == PROTOTYPE_INVALID_ID ||
				!prototype_context_contains_binding(
					contexts,
					operations->operations[operation->body].context_id,
					operation->binding_id
				)) {
				return -1;
			}
			int body_equal = 0;
			return prototype_term_core_shape_equal_under_binder(
					terms,
					core->as.lambda.binding_id,
					core->as.lambda.body,
					operation->binding_id,
					operations->operations[operation->body].core_term,
					&body_equal
				) == 0 && body_equal ? 0 : -1;
		case PROTOTYPE_OPERATION_MATCH:
			if (core->tag != PROTOTYPE_TERM_MATCH ||
				operation->scrutinee >= operations->operation_count ||
				!operation_core_projection_equal(
					terms, operations, operation->scrutinee,
					core->as.match.scrutinee
				) ||
				operation->case_count != core->as.match.case_count ||
				operation->first_case > operations->case_count ||
				operation->case_count > operations->case_count -
					operation->first_case ||
				core->as.match.first_case > terms->case_count ||
				core->as.match.case_count > terms->case_count -
					core->as.match.first_case) {
				return -1;
			}
			for (uint32_t i = 0; i < operation->case_count; ++i) {
				const struct prototype_operation_match_case* typed_case =
					&operations->cases[operation->first_case + i];
				const struct prototype_match_case* erased_case =
					&terms->cases[core->as.match.first_case + i];
				if (typed_case->body_operation >= operations->operation_count ||
					typed_case->context_id >= contexts->context_count ||
					!operation_core_projection_equal(
						terms, operations, typed_case->body_operation,
						erased_case->body
					) ||
					!term_core_projection_equal(
						terms, typed_case->constructor_owner,
						erased_case->constructor_owner
					) ||
					typed_case->constructor_id != erased_case->constructor_id ||
					typed_case->binder_count != erased_case->binder_count) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS:
			if (core->tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
				operation->argument >= operations->operation_count ||
				operation->scrutinee >= operations->operation_count ||
				operations->operations[operation->scrutinee].tag !=
					PROTOTYPE_OPERATION_MATCH ||
				!operation_core_projection_equal(
					terms, operations, operation->argument,
					core->as.induction_hypothesis.argument
				) ||
				operation->first_case != core->as.induction_hypothesis.ih_scope_id ||
				operation->first_case >= terms->ih_scope_count ||
				terms->ih_scopes[operation->first_case].match_term !=
					operations->operations[operation->scrutinee].core_term) {
				return -1;
			}
			return 0;
		case PROTOTYPE_OPERATION_ASCRIPTION:
			return operation->body < operations->operation_count &&
				operation_core_projection_equal(
					terms, operations, operation->body, operation->core_term
				) ? 0 : -1;
		case PROTOTYPE_OPERATION_RETURN:
			return core->tag == PROTOTYPE_TERM_RETURN &&
				operation->argument < operations->operation_count &&
				operation_core_projection_equal(
					terms, operations, operation->argument,
					core->as.return_term.value
				) ? 0 : -1;
		case PROTOTYPE_OPERATION_THUNK:
			return core->tag == PROTOTYPE_TERM_THUNK &&
				operation->argument < operations->operation_count &&
				operation_core_projection_equal(
					terms, operations, operation->argument,
					core->as.thunk.computation
				) ? 0 : -1;
		case PROTOTYPE_OPERATION_FORCE:
			return core->tag == PROTOTYPE_TERM_FORCE &&
				operation->argument < operations->operation_count &&
				operation_core_projection_equal(
					terms, operations, operation->argument, core->as.force.value
				) ? 0 : -1;
		case PROTOTYPE_OPERATION_REQUEST:
			return core->tag == PROTOTYPE_TERM_OPERATION_REQUEST &&
				operation->function < operations->operation_count &&
				operation->argument < operations->operation_count &&
				operation->body < operations->operation_count &&
				operation_core_projection_equal(
					terms, operations, operation->function,
					core->as.operation_request.operation
				) && operation_core_projection_equal(
					terms, operations, operation->argument,
					core->as.operation_request.argument
				) && operation_core_projection_equal(
					terms, operations, operation->body,
					core->as.operation_request.continuation
				) ? 0 : -1;
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			if (core->tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
				operation->function >= operations->operation_count ||
				!operation_core_projection_equal(
					terms, operations, operation->function,
					core->as.computation_fold.computation
				) ||
				operation->fold_clause_count !=
					core->as.computation_fold.clause_count ||
				(operation->fold_clause_count != 0 &&
				 (operation->first_fold_clause > operations->fold_clause_count ||
				  operation->fold_clause_count > operations->fold_clause_count -
					operation->first_fold_clause))) {
				return -1;
			}
			uint32_t return_operation = operation->fold_clause_count == 0 ?
				operation->argument : operation->fold_return_operation;
			if (return_operation >= operations->operation_count ||
				!operation_core_projection_equal(
					terms, operations, return_operation,
					core->as.computation_fold.return_clause
				)) {
				return -1;
			}
			return 0;
		default:
			return -1;
	}
}

int prototype_judgement_validate_accepted_graph(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			&judgement->propositions[i];
		if (!prototype_context_get(contexts, proposition->context_id) ||
			proposition->subject >= terms->term_count ||
			proposition->classifier >= terms->term_count ||
			(proposition->operation_id != PROTOTYPE_INVALID_ID &&
			 (!operations || proposition->operation_id >= operations->operation_count))) {
			fprintf(
				stderr,
				"P0 proposition reference failed proposition=%u context=%u "
				"operation=%u subject=%u classifier=%u\n",
				i,
				proposition->context_id,
				proposition->operation_id,
				proposition->subject,
				proposition->classifier
			);
			return -1;
		}
	}
	if (prototype_judgement_recompute_closure_ranks(operations, judgement) != 0) {
		fprintf(stderr, "P0 accepted rank computation failed\n");
		return -1;
	}
	if (prototype_judgement_db_rebuild_index(judgement) != 0) {
		fprintf(stderr, "P0 accepted index rebuild failed\n");
		return -1;
	}
	if (validate_proof_relation_coverage(judgement) != 0) {
		fprintf(stderr, "P0 coverage failed\n");
		return -1;
	}
	if (operations) {
		for (uint32_t claim_id = 0;
			claim_id < (uint32_t)judgement->claim_count; ++claim_id) {
			if (judgement->claims[claim_id].closure_rank == PROTOTYPE_INVALID_ID) {
				continue;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(judgement, claim_id);
			if (!proposition ||
				proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				proposition->authority_kind !=
					PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION) {
				continue;
			}
			if (proposition->operation_id >= operations->operation_count ||
				proposition->authority_id != proposition->operation_id) {
				return -1;
			}
			int expected_polarity;
			if (judgement_operation_expected_polarity(
					terms,
					type_declarations,
					operations,
					proposition->operation_id,
					&expected_polarity
				) != 0 || operations->operations[proposition->operation_id].polarity !=
					expected_polarity) {
				fprintf(
					stderr,
					"P1-R0 accepted operation polarity failed claim=%u operation=%u\n",
					claim_id,
					proposition->operation_id
				);
				return -1;
			}
		}
		for (uint32_t operation_id = 0;
			operation_id < operations->operation_count;
			++operation_id) {
			if (prototype_judgement_validate_operation_typing(
					terms,
					type_declarations,
					contexts,
					operations,
					operation_id
				) != 0) {
				const struct prototype_operation_node* failed =
					&operations->operations[operation_id];
				fprintf(
					stderr,
					"P0 operation validation failed operation=%u tag=%d context=%u "
					"core=%u classifier=%u binding=%u body=%u\n",
					operation_id,
					failed->tag,
					failed->context_id,
					failed->core_term,
					failed->classifier,
					failed->binding_id,
					failed->body
				);
				return -1;
			}
		}
	}
	for (size_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID ||
			derivation->closure_rank == PROTOTYPE_INVALID_ID) {
			continue;
		}
		struct prototype_judgement_proposition relation_storage;
		struct prototype_judgement_derivation_candidate proof_storage;
		struct prototype_judgement_candidate_premise
			premise_storage[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		if (accepted_derivation_replay_view(
				judgement,
				derivation,
				&relation_storage,
				&proof_storage,
				premise_storage
			) != 0) {
			fprintf(
				stderr,
				"P0 accepted derivation replay failed derivation=%zu rule=%d "
				"claim=%u premises=%u\n",
				i,
				derivation->proof_kind,
				derivation->conclusion_claim_id,
				derivation->premise_count
			);
			return -1;
		}
		const struct prototype_judgement_proposition* relation =
			&relation_storage;
		const struct prototype_judgement_derivation_candidate* proof =
			&proof_storage;
			int rule_parameter_status = validate_proof_rule_parameters(proof);
			int operation_owner_status = validate_operation_owned_relation(
				operations, contexts, substitutions, relation, proof
			);
			if (proof->conclusion_kind != relation->kind ||
				proof->conclusion_operation_id != relation->operation_id ||
				proof->conclusion_subject != relation->subject ||
				proof->conclusion_classifier != relation->classifier ||
				proof->conclusion_context_id != relation->context_id ||
				proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
				rule_parameter_status != 0 || operation_owner_status != 0) {
				fprintf(
					stderr,
					"P0 accepted header failed derivation=%zu claim=%u operation=%u "
					"proof_operation=%u context=%u proof_context=%u "
					"rule_params=%d operation_owner=%d classifier=%u\n",
					i,
					derivation->conclusion_claim_id,
					relation->operation_id,
					proof->conclusion_operation_id,
					relation->context_id,
					proof->conclusion_context_id,
					rule_parameter_status,
					operation_owner_status,
					relation->classifier
				);
				return -1;
			}
			if (operations) {
				for (uint32_t premise_index = 0;
					premise_index < proof->premise_count;
					++premise_index) {
					uint32_t expected_operation_id;
					int expected_status = judgement_expected_premise_operation(
						operations,
						proof,
						premise_index,
						&expected_operation_id
					);
					if (expected_status < 0 ||
						(expected_status == 0 &&
						 proof->premises[premise_index].proposition.authority_kind ==
							PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION &&
						 proof->premises[premise_index].proposition.operation_id !=
							expected_operation_id)) {
						fprintf(
							stderr,
							"P0 accepted premise owner failed derivation=%zu "
							"rule=%d premise=%u expected=%u actual=%u\n",
							i,
							proof->proof_kind,
							premise_index,
							expected_operation_id,
							proof->premises[premise_index].proposition.operation_id
						);
						return -1;
					}
				}
			}
			uint32_t free_effect_row_binder;
			int has_free_effect_row =
				judgement_classifier_free_result_effect_row_binder(
					terms,
					type_declarations,
					relation->classifier,
					&free_effect_row_binder
				);
			int has_effect_row_assumption = has_free_effect_row > 0 ?
				judgement_derivation_has_effect_row_assumption(
					judgement,
					terms,
					derivation,
					free_effect_row_binder
				) : 0;
			if (has_effect_row_assumption < 0 ||
				has_free_effect_row < 0 ||
				(has_free_effect_row != 0 && has_effect_row_assumption == 0)) {
				fprintf(
					stderr,
					"P0 effect-row assumption failed derivation=%zu rule=%d free=%d "
					"assumption=%d binder=%u\n",
					i,
					proof->proof_kind,
					has_free_effect_row,
					has_effect_row_assumption,
					free_effect_row_binder
				);
				return -1;
			}
			switch (proof->proof_kind) {
			case PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO:
				if (validate_type_formation_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO:
				if (validate_constructor_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION:
				if (validate_constructor_spine_formation_proof(
						terms,
						type_declarations,
						contexts,
						substitutions,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
					if (validate_assumption_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
				if (proof->premise_count != 2) {
					return -1;
				}
					if (validate_lambda_intro_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							relation,
							proof
						) != 0) {
						return -1;
					}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
				if (proof->premise_count != 2) {
					return -1;
				}
					if (validate_app_elim_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
					if (validate_return_intro_proof(
							terms, type_declarations, relation, proof
						) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
				if (validate_thunk_intro_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
				if (validate_force_elim_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
				if (validate_computation_fold_elim_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
					if (validate_operation_request_intro_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							relation,
							proof
						) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
				if (validate_match_elim_proof(
						terms,
						type_declarations,
						contexts,
						substitutions,
						judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
				if (validate_solved_match_motive_proof(
						terms, type_declarations, judgement, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM:
				if (proof->premise_count != 0) {
					return -1;
				}
				if (validate_induction_hypothesis_elim_proof(
						terms,
						type_declarations,
						judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
				case PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO:
					if (validate_text_literal_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
		case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO:
					if (validate_int_literal_intro_proof(
							terms, relation, proof
						) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY:
					if (validate_int_literal_admissibility_proof(
							terms, relation, proof
						) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO:
					if (validate_pure_primitive_type_intro_proof(
						terms,
						type_declarations,
						contexts,
						relation,
						proof
					) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO:
					if (validate_effect_operation_type_intro_proof(
						terms, relation, proof
					) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO:
					if (validate_text_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO:
					if (validate_int_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO:
					if (validate_host_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO:
				if (validate_match_type_formation_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
				if (validate_conversion_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE:
				if (validate_expected_type_exposure_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_DECLARATION:
				if (validate_declaration_proof(
						terms,
						type_declarations,
						judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE:
				if (validate_is_type_from_has_type_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY:
				if (validate_universe_cumulativity_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO:
				if (validate_pi_formation_intro_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION:
				if (validate_computation_type_formation_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION:
				if (validate_thunk_type_formation_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN:
				if (validate_context_weaken_proof(
						contexts, substitutions, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX:
				if (validate_substitution_reindex_proof(
						terms,
						type_declarations,
						contexts,
						substitutions,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
				if (validate_effect_weaken_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION:
				if (validate_relation_type_formation_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_WITNESS_INTRO:
				if (validate_relation_witness_intro_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS:
				if (validate_relation_constructor_witness_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS:
				if (validate_relation_unary_witness_proof(
						terms, relation, proof, PROTOTYPE_TERM_RETURN
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS:
				if (validate_relation_unary_witness_proof(
						terms, relation, proof, PROTOTYPE_TERM_THUNK
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS:
				if (validate_relation_app_witness_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS:
				if (validate_relation_lambda_witness_proof(
						terms, contexts, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS:
				if (validate_relation_match_witness_proof(
						terms, type_declarations, contexts, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS:
				if (validate_relation_induction_hypothesis_witness_proof(
						terms, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_INVALID:
				return -1;
			default:
				return -1;
		}
	}
	return 0;
}

int prototype_judgement_add_is_type(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t universe
) {
	if (!term_exists(terms, subject) || !term_is_universe_var(terms, universe)) {
		return -1;
	}
	uint32_t premise_subject = subject;
	uint32_t premise_classifier = universe;
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		subject,
		universe,
		PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE,
		NULL,
		&premise_subject,
		&premise_classifier,
		1
	);
}

int prototype_judgement_add_is_type_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject,
	uint32_t universe,
	uint32_t has_type_claim_id,
	uint32_t* p_claim_id
) {
	if (!judgement || !p_claim_id || !term_exists(terms, subject) ||
		!term_is_universe_var(terms, universe)) {
		return -1;
	}
	const struct prototype_judgement_proposition* premise =
		prototype_judgement_claim_proposition(judgement, has_type_claim_id);
	if (!premise ||
		premise->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		premise->context_id != context_id || premise->subject != subject ||
		premise->classifier != universe) {
		return -1;
	}
	struct prototype_judgement_proposition proposition = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		.context_id = context_id,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = subject,
		.classifier = universe
	};
	struct prototype_judgement_derivation_candidate derivation;
	struct prototype_judgement_candidate_premise
		premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	memset(&derivation, 0, sizeof(derivation));
	derivation.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE;
	derivation.conclusion_kind = proposition.kind;
	derivation.conclusion_context_id = context_id;
	derivation.conclusion_operation_id = PROTOTYPE_INVALID_ID;
	derivation.conclusion_subject = subject;
	derivation.conclusion_classifier = universe;
	initialize_proof_rule_parameters(&derivation, premises);
	derivation.premise_count = 1;
	premises[0].proposition = *premise;
	return publish_complete_relation(
		judgement,
		&proposition,
		&derivation,
		&has_type_claim_id,
		p_claim_id
	);
}

int prototype_judgement_lookup_authority_neutral_core_classifier(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_classifier
) {
	return lookup_classifier(judgement, NULL, subject, p_classifier);
}

static const char* judgement_kind_name(int kind) {
	switch (kind) {
		case PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE:
			return "has-type";
		case PROTOTYPE_JUDGEMENT_KIND_IS_TYPE:
			return "is-type";
		default:
			return "unknown";
	}
}

static const char* proof_kind_name(int proof_kind) {
	switch (proof_kind) {
		case PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO:
			return "type-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO:
			return "constructor-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION:
			return "constructor-spine-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
			return "binder-assumption";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
			return "match-pattern-assumption";
		case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
			return "lambda-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
			return "app-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
			return "return-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
			return "thunk-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
			return "force-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
			return "computation-fold-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
			return "operation-request-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO:
			return "match-type-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
			return "match-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
			return "solved-match-motive";
		case PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM:
			return "ih-elim";
			case PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO:
				return "text-literal-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO:
			return "int-literal-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY:
			return "int-literal-admissibility";
			case PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO:
				return "pure-primitive-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO:
				return "effect-operation-type-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
			return "conversion";
		case PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE:
			return "expected-type-exposure";
			case PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO:
				return "text-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO:
				return "int-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO:
				return "host-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE:
				return "is-type-from-has-type";
		case PROTOTYPE_JUDGEMENT_PROOF_DECLARATION:
			return "declaration";
		case PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY:
			return "universe-cumulativity";
		case PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO:
			return "pi-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION:
			return "computation-type-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION:
			return "thunk-type-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN:
			return "context-weaken";
		case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
			return "effect-weaken";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION:
			return "relation-type-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_WITNESS_INTRO:
			return "relation-witness-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX:
			return "substitution-reindex";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS:
			return "relation-constructor-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS:
			return "relation-return-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS:
			return "relation-thunk-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS:
			return "relation-app-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS:
			return "relation-lambda-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS:
			return "relation-match-witness";
		case PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS:
			return "relation-induction-hypothesis-witness";
		default:
			return "invalid";
	}
}

void prototype_judgement_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement
) {
	if (!output || !symbols || !type_declarations || !terms || !judgement) {
		return;
	}

	fprintf(output, "judgements=%zu\n", judgement->proposition_count);
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		fprintf(output, "%s ", judgement_kind_name(relation->kind));
		prototype_term_print_debug(output, symbols, type_declarations, terms, relation->subject);
		fprintf(output, " ");
		prototype_term_print_debug(output, symbols, type_declarations, terms, relation->classifier);
		uint32_t cursor = 0;
		uint32_t derivation_id;
		while (prototype_judgement_candidate_derivation_next(
			judgement->propositions,
			judgement->proposition_count,
			judgement->derivation_candidates,
			judgement->derivation_candidate_count,
			(uint32_t)i,
			&cursor,
			&derivation_id
		) == 0) {
			const struct prototype_judgement_derivation_candidate* proof =
				&judgement->derivation_candidates[derivation_id];
			fprintf(
				output,
				" [%s proof#%u premises=%u]",
				proof_kind_name(proof->proof_kind),
				derivation_id,
				proof->premise_count
			);
		}
		fprintf(output, "\n");
	}
}
