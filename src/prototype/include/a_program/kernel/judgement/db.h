#ifndef __A_PROGRAM_KERNEL_JUDGEMENT_DB_H__
#define __A_PROGRAM_KERNEL_JUDGEMENT_DB_H__

#include "a_program/kernel/judgement/types.h"

int prototype_judgement_db_rebuild_index(
	struct prototype_judgement_db* judgement
);

enum prototype_judgement_category {
	PROTOTYPE_JUDGEMENT_CATEGORY_INVALID = 0,
	PROTOTYPE_JUDGEMENT_CATEGORY_VALUE,
	PROTOTYPE_JUDGEMENT_CATEGORY_COMPUTATION,
	PROTOTYPE_JUDGEMENT_CATEGORY_TYPE
};

const struct prototype_judgement_claim* prototype_judgement_claim_get(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
);
const struct prototype_judgement_proposition* prototype_judgement_proposition_get(
	const struct prototype_judgement_db* judgement,
	uint32_t proposition_id
);
int prototype_judgement_proposition_find_exact(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* proposition,
	uint32_t* p_proposition_id
);
int prototype_judgement_proposition_intern(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_proposition* proposition,
	uint32_t* p_proposition_id
);
int prototype_judgement_claim_intern_exact(
	struct prototype_judgement_db* judgement,
	uint32_t proposition_id,
	uint32_t* p_claim_id
);
int prototype_judgement_derivation_intern_exact(
	struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation* derivation,
	uint32_t* p_derivation_id
);
const struct prototype_judgement_proposition* prototype_judgement_claim_proposition(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
);
const struct prototype_judgement_proposition* prototype_judgement_premise_proposition(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_premise_edge* premise
);
const struct prototype_judgement_derivation* prototype_judgement_derivation_get(
	const struct prototype_judgement_db* judgement,
	uint32_t derivation_id
);
int prototype_judgement_find_exact_claim(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_claim* identity,
	uint32_t* p_claim_id
);
int prototype_judgement_find_exact_derivation(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation* identity,
	uint32_t* p_derivation_id
);
int prototype_judgement_claim_derivations(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	uint32_t* derivation_ids,
	size_t derivation_capacity,
	size_t* p_derivation_count
);
int prototype_judgement_claim_category(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_operation_graph* operations,
	uint32_t claim_id,
	int* p_category
);
int prototype_judgement_selected_evidence_from_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	struct prototype_judgement_selected_evidence* p_evidence
);

/* Temporary overlay for judgement facts produced while compiling one graph
 * fragment. Successful paths commit the delta into JudgementDB; failed paths
 * rewind it. This is not a semantic typing context. */
struct prototype_judgement_delta {
	struct prototype_judgement_db* db;
	struct prototype_judgement_proposition* propositions;
	struct prototype_judgement_derivation_candidate* derivation_candidates;
	struct prototype_judgement_candidate_premise* candidate_premises;
	struct prototype_judgement_match_motive_result* match_motive_results;
	struct prototype_judgement_computation_constraint* computation_constraints;
	struct prototype_judgement_effect_row_constraint* effect_row_constraints;
	size_t proposition_count;
	size_t proposition_capacity;
	size_t derivation_candidate_count;
	size_t derivation_candidate_capacity;
	size_t candidate_premise_count;
	size_t candidate_premise_capacity;
	size_t match_motive_result_count;
	size_t match_motive_result_capacity;
	size_t computation_constraint_count;
	size_t computation_constraint_capacity;
	size_t effect_row_constraint_count;
	size_t effect_row_constraint_capacity;
	uint64_t solver_step_limit;
	uint64_t* solver_steps_used;
	int* solver_exhausted;
	struct prototype_context_db* contexts;
	struct prototype_substitution_db* substitutions;
	const struct prototype_operation_node* operations;
	size_t operation_count;
	const struct prototype_operation_match_case* operation_cases;
	size_t operation_case_count;
	/* Context for relations emitted by the current elaboration rule. This is
	 * an explicit CwF object ID, not the old proof provenance fields. */
	uint32_t current_context_id;
	/* Operation identity for source/generated typing materialization. */
	uint32_t current_operation_id;
	/* Resource evidence is computed by the OperationGraph analysis and attached
	 * while a proposition candidate is created. Candidate identity must never
	 * transition from "unknown usage" to a different, completed proposition. */
	void* operation_usage_provider_context;
	int (*operation_usage_provider)(
		void* context,
		uint32_t operation_id,
		const struct prototype_usage_entry** p_entries,
		uint32_t* p_count
	);
};

struct prototype_match_constructor_resolution {
	uint32_t constructor_owner;
	uint32_t constructor_id;
	uint32_t field_count;
};

struct prototype_match_resolution_request {
	uint32_t match_term;
	uint32_t case_index;
	uint32_t scrutinee_term;
	uint32_t scrutinee_classifier;
	int constructor_symbol_id;
};

struct prototype_induction_hypothesis_resolution_request {
	uint32_t subject;
	uint32_t ih_scope_id;
	uint32_t argument;
};

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
);

void prototype_judgement_db_set_resource_usage_storage(
	struct prototype_judgement_db* db,
	struct prototype_usage_entry* entries,
	size_t capacity
);

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
);

void prototype_judgement_delta_set_solver_budget(
	struct prototype_judgement_delta* delta,
	uint64_t step_limit,
	uint64_t* steps_used,
	int* exhausted
);
void prototype_judgement_delta_set_context(
	struct prototype_judgement_delta* delta,
	uint32_t context_id
);
void prototype_judgement_delta_set_operation(
	struct prototype_judgement_delta* delta,
	uint32_t operation_id
);
void prototype_judgement_delta_set_operation_usage_provider(
	struct prototype_judgement_delta* delta,
	void* context,
	int (*provider)(
		void* context,
		uint32_t operation_id,
		const struct prototype_usage_entry** p_entries,
		uint32_t* p_count
	)
);
void prototype_judgement_delta_set_context_store(
	struct prototype_judgement_delta* delta,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions
);
void prototype_judgement_delta_set_operation_store(
	struct prototype_judgement_delta* delta,
	const struct prototype_operation_node* operations,
	size_t operation_count,
	const struct prototype_operation_match_case* operation_cases,
	size_t operation_case_count
);

int prototype_judgement_delta_commit(
	struct prototype_judgement_delta* delta,
	size_t mark
);

/* Publish one solver delta as an append-only accepted proof DAG. Existing
 * Claim and Derivation ids remain stable; unlike final candidate publication,
 * this operation never compacts accepted storage. */
int prototype_judgement_delta_publish_complete(
	struct prototype_judgement_delta* delta
);

/* Enumerates every solver candidate Derivation concluding one candidate Claim.
 * Initialize *p_cursor to zero. Returns 0 with one result, 1 at the end, and
 * -1 for malformed input. No Derivation is designated as the Claim's proof. */
int prototype_judgement_candidate_derivation_next(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	uint32_t* p_cursor,
	uint32_t* p_derivation_id
);

int prototype_judgement_candidate_find_derivation_kind(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	int proof_kind,
	uint32_t* p_derivation_id
);

int prototype_judgement_candidate_find_derivation_other_than(
	const struct prototype_judgement_proposition* claims,
	size_t claim_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t claim_id,
	int excluded_proof_kind,
	uint32_t* p_derivation_id
);


#endif
