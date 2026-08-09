#ifndef __PROTOTYPE_HOTT_H__
#define __PROTOTYPE_HOTT_H__

#include "cwf_certificate.h"
#include "judgement.h"
#include "kernel_view.h"

enum prototype_hott_observation_category {
	PROTOTYPE_HOTT_OBSERVATION_VALUE = 1,
	PROTOTYPE_HOTT_OBSERVATION_COMPUTATION = 2
};

enum prototype_hott_outcome_state {
	PROTOTYPE_HOTT_OUTCOME_PENDING = 1,
	PROTOTYPE_HOTT_OUTCOME_READY = 2,
	PROTOTYPE_HOTT_OUTCOME_RESIDUAL = 3,
	PROTOTYPE_HOTT_OUTCOME_UNSUPPORTED = 4
};

#define PROTOTYPE_HOTT_WORK_PENDING PROTOTYPE_HOTT_OUTCOME_PENDING
#define PROTOTYPE_HOTT_WORK_READY PROTOTYPE_HOTT_OUTCOME_READY
#define PROTOTYPE_HOTT_WORK_RESIDUAL PROTOTYPE_HOTT_OUTCOME_RESIDUAL
#define PROTOTYPE_HOTT_WORK_UNSUPPORTED PROTOTYPE_HOTT_OUTCOME_UNSUPPORTED

#define PROTOTYPE_HOTT_ACTION_RESULT_READY PROTOTYPE_HOTT_OUTCOME_READY
#define PROTOTYPE_HOTT_ACTION_RESULT_RESIDUAL PROTOTYPE_HOTT_OUTCOME_RESIDUAL
#define PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED \
	PROTOTYPE_HOTT_OUTCOME_UNSUPPORTED

enum prototype_hott_residual_reason {
	PROTOTYPE_HOTT_RESIDUAL_NONE = 0,
	PROTOTYPE_HOTT_RESIDUAL_CONVERSION,
	PROTOTYPE_HOTT_RESIDUAL_CONVERSION_EXHAUSTED,
	PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL,
	PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED,
	PROTOTYPE_HOTT_RESIDUAL_NEUTRAL_PURE_COMPUTATION,
	PROTOTYPE_HOTT_RESIDUAL_OPERATION_REQUEST,
	PROTOTYPE_HOTT_RESIDUAL_COMPUTATION_FOLD,
	PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE,
	PROTOTYPE_HOTT_RESIDUAL_TYPE_VIEW,
	PROTOTYPE_HOTT_RESIDUAL_UNIVERSE,
	PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE
};

struct prototype_hott_deterministic_outcome {
	int state;
	int residual_reason;
	int normalization_profile;
	uint64_t step_limit;
	uint64_t steps_used;
	uint64_t term_graph_revision;
	char calculus_fingerprint[65];
};

enum prototype_hott_type_former_kind {
	PROTOTYPE_HOTT_TYPE_FORMER_UNKNOWN = 0,
	PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT = 1,
	PROTOTYPE_HOTT_TYPE_FORMER_PI = 2,
	PROTOTYPE_HOTT_TYPE_FORMER_PURE_COMPUTATION = 3,
	PROTOTYPE_HOTT_TYPE_FORMER_THUNK = 4,
	PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE = 5,
	PROTOTYPE_HOTT_TYPE_FORMER_HOST_PRIMITIVE = 6,
	PROTOTYPE_HOTT_TYPE_FORMER_OBSERVATION = 7
};

enum prototype_hott_type_action_rule {
	PROTOTYPE_HOTT_TYPE_ACTION_RULE_NONE = 0,
	PROTOTYPE_HOTT_TYPE_ACTION_RULE_ZERO_FIELD_ADT = 1,
	PROTOTYPE_HOTT_TYPE_ACTION_RULE_ADT_TELESCOPE = 2,
	PROTOTYPE_HOTT_TYPE_ACTION_RULE_PI_POINTWISE = 3,
	PROTOTYPE_HOTT_TYPE_ACTION_RULE_PURE_COMPUTATION = 4,
	PROTOTYPE_HOTT_TYPE_ACTION_RULE_THUNK = 5,
	PROTOTYPE_HOTT_TYPE_ACTION_RULE_OBSERVATION_HIGHER = 6
};

enum prototype_hott_capability_status {
	PROTOTYPE_HOTT_CAPABILITY_UNDECLARED = 0,
	PROTOTYPE_HOTT_CAPABILITY_SUPPORTED = 1,
	PROTOTYPE_HOTT_CAPABILITY_DEFERRED = 2,
	PROTOTYPE_HOTT_CAPABILITY_NOT_APPLICABLE = 3
};

struct prototype_hott_type_former_capabilities {
	int type_action;
	int term_action;
	int ordinary_reindex;
	int purity;
	int resource_hook;
	int artifact;
};

struct prototype_hott_type_former_descriptor {
	int kind;
	int admitted;
	int type_action_rule;
	int residual_reason;
	uint32_t source_claim_id;
	uint32_t source_type_term_id;
	struct prototype_hott_type_former_capabilities capabilities;
	uint64_t child_role_mask;
};

int prototype_hott_type_former_descriptor_query(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_db* judgement,
	uint32_t source_claim_id,
	struct prototype_hott_type_former_descriptor* p_descriptor
);

enum prototype_hott_rule {
	PROTOTYPE_HOTT_RULE_NONE = 0,
	PROTOTYPE_HOTT_RULE_OBS_DIAGONAL = 1,
	PROTOTYPE_HOTT_RULE_OBS_CONVERT = 2,
	PROTOTYPE_HOTT_RULE_OBS_ADT_CONSTRUCTOR = 3,
	PROTOTYPE_HOTT_RULE_OBS_ADT_DISTINCT = 4,
	PROTOTYPE_HOTT_RULE_OBS_MATCH_ACTION = 5,
	PROTOTYPE_HOTT_RULE_OBS_COMP_RETURN = 6,
	PROTOTYPE_HOTT_RULE_OBS_PI_POINTWISE = 7,
	PROTOTYPE_HOTT_RULE_OBS_THUNK_PURE = 8,
	PROTOTYPE_HOTT_RULE_OBS_REINDEX = 9
};

enum prototype_hott_child_role {
	PROTOTYPE_HOTT_CHILD_NONE = 0,
	PROTOTYPE_HOTT_CHILD_ADT_FIELD = 1,
	PROTOTYPE_HOTT_CHILD_ADT_DEPENDENT_REINDEX = 2,
	PROTOTYPE_HOTT_CHILD_MATCH_SCRUTINEE = 3,
	PROTOTYPE_HOTT_CHILD_MATCH_MOTIVE_ACTION = 4,
	PROTOTYPE_HOTT_CHILD_MATCH_CASE_ACTION = 5,
	PROTOTYPE_HOTT_CHILD_MATCH_RECURSIVE_IH = 6,
	PROTOTYPE_HOTT_CHILD_COMP_LEFT_RETURN_EXPOSURE = 7,
	PROTOTYPE_HOTT_CHILD_COMP_RIGHT_RETURN_EXPOSURE = 8,
	PROTOTYPE_HOTT_CHILD_COMP_RESULT_OBSERVATION = 9,
	PROTOTYPE_HOTT_CHILD_PI_DOMAIN_ACTION = 10,
	PROTOTYPE_HOTT_CHILD_PI_RELATED_INPUT = 11,
	PROTOTYPE_HOTT_CHILD_PI_CODOMAIN_OBSERVATION = 12,
	PROTOTYPE_HOTT_CHILD_THUNK_COMPUTATION_OBSERVATION = 13,
	PROTOTYPE_HOTT_CHILD_CONVERT_ANCHOR_OBSERVATION = 14,
	PROTOTYPE_HOTT_CHILD_CONTEXT_ACTION = 15,
	PROTOTYPE_HOTT_CHILD_SUBSTITUTION_ACTION = 16,
	PROTOTYPE_HOTT_CHILD_TYPE_ACTION = 17,
	PROTOTYPE_HOTT_CHILD_TERM_ACTION = 18,
	PROTOTYPE_HOTT_CHILD_REINDEX_NATURALITY = 19
};

struct prototype_hott_bridge {
	uint32_t id;
	uint32_t source_context_id;
	uint32_t bridge_context_id;
	uint32_t left_substitution_id;
	uint32_t right_substitution_id;
};

struct prototype_hott_bridge_certificate {
	uint32_t id;
	uint32_t bridge_id;
	uint32_t parent_bridge_id;
	uint32_t type_action_certificate_id;
	uint32_t left_endpoint_context_certificate_id;
	uint32_t right_endpoint_context_certificate_id;
	uint32_t relation_context_certificate_id;
	uint32_t left_substitution_certificate_id;
	uint32_t right_substitution_certificate_id;
};

struct prototype_hott_bridge_db {
	struct prototype_hott_bridge* bridges;
	size_t bridge_count;
	size_t bridge_capacity;
	struct prototype_hott_bridge_certificate* certificates;
	size_t certificate_count;
	size_t certificate_capacity;
};

void prototype_hott_bridge_db_init(
	struct prototype_hott_bridge_db* db,
	struct prototype_hott_bridge* bridges,
	size_t bridge_capacity,
	struct prototype_hott_bridge_certificate* certificates,
	size_t certificate_capacity
);
const struct prototype_hott_bridge* prototype_hott_bridge_db_get(
	const struct prototype_hott_bridge_db* db,
	uint32_t bridge_id
);
int prototype_hott_bridge_db_construct(
	struct prototype_hott_bridge_db* db,
	struct prototype_kernel_builder* kernel,
	uint32_t source_context_id,
	uint32_t* p_bridge_id
);
int prototype_hott_bridge_db_validate(
	const struct prototype_hott_bridge_db* db,
	const struct prototype_kernel_view* kernel
);

struct prototype_hott_observation_goal {
	uint32_t id;
	int category;
	uint32_t carrier_claim_id;
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	uint32_t bridge_id;
};

struct prototype_hott_observation_goal_db {
	struct prototype_hott_observation_goal* goals;
	size_t goal_count;
	size_t goal_capacity;
};

struct prototype_hott_candidate {
	uint32_t id;
	uint32_t conclusion_goal_id;
	int rule;
	uint32_t first_claim_premise;
	uint32_t claim_premise_count;
	uint32_t first_child_edge;
	uint32_t child_edge_count;
	uint32_t first_conversion_premise;
	uint32_t conversion_premise_count;
	uint32_t first_context_certificate_premise;
	uint32_t context_certificate_premise_count;
	uint32_t first_substitution_certificate_premise;
	uint32_t substitution_certificate_premise_count;
};

struct prototype_hott_claim_premise {
	uint32_t candidate_id;
	uint32_t claim_id;
	int role;
	uint32_t ordinal;
};

struct prototype_hott_child_edge {
	uint32_t candidate_id;
	uint32_t child_goal_id;
	int role;
	uint32_t ordinal;
};

struct prototype_hott_conversion_premise {
	uint32_t candidate_id;
	int role;
	uint32_t ordinal;
	struct prototype_kernel_conversion_goal request;
	uint64_t conversion_graph_revision;
};

struct prototype_hott_context_certificate_premise {
	uint32_t candidate_id;
	uint32_t certificate_id;
	int role;
	uint32_t ordinal;
};

struct prototype_hott_substitution_certificate_premise {
	uint32_t candidate_id;
	uint32_t certificate_id;
	int role;
	uint32_t ordinal;
};

struct prototype_hott_candidate_db {
	struct prototype_hott_candidate* candidates;
	size_t candidate_count;
	size_t candidate_capacity;
	struct prototype_hott_claim_premise* claim_premises;
	size_t claim_premise_count;
	size_t claim_premise_capacity;
	struct prototype_hott_child_edge* child_edges;
	size_t child_edge_count;
	size_t child_edge_capacity;
	struct prototype_hott_conversion_premise* conversion_premises;
	size_t conversion_premise_count;
	size_t conversion_premise_capacity;
	struct prototype_hott_context_certificate_premise* context_certificate_premises;
	size_t context_certificate_premise_count;
	size_t context_certificate_premise_capacity;
	struct prototype_hott_substitution_certificate_premise* substitution_certificate_premises;
	size_t substitution_certificate_premise_count;
	size_t substitution_certificate_premise_capacity;
};

struct prototype_hott_work_item {
	uint32_t id;
	uint32_t goal_id;
	uint32_t selected_candidate_id;
	uint32_t source_ast;
	struct prototype_hott_deterministic_outcome outcome;
};

struct prototype_hott_work_db {
	struct prototype_hott_work_item* items;
	size_t item_count;
	size_t item_capacity;
};

void prototype_hott_observation_goal_db_init(
	struct prototype_hott_observation_goal_db* db,
	struct prototype_hott_observation_goal* goals,
	size_t goal_capacity
);
const struct prototype_hott_observation_goal*
prototype_hott_observation_goal_db_get(
	const struct prototype_hott_observation_goal_db* db,
	uint32_t goal_id
);
int prototype_hott_observation_goal_db_intern(
	struct prototype_hott_observation_goal_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	int category,
	uint32_t carrier_claim_id,
	uint32_t left_claim_id,
	uint32_t right_claim_id,
	uint32_t bridge_id,
	uint32_t* p_goal_id
);
int prototype_hott_observation_goal_db_validate(
	const struct prototype_hott_observation_goal_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges
);

void prototype_hott_candidate_db_init(
	struct prototype_hott_candidate_db* db,
	struct prototype_hott_candidate* candidates,
	size_t candidate_capacity,
	struct prototype_hott_claim_premise* claim_premises,
	size_t claim_premise_capacity,
	struct prototype_hott_child_edge* child_edges,
	size_t child_edge_capacity,
	struct prototype_hott_conversion_premise* conversion_premises,
	size_t conversion_premise_capacity,
	struct prototype_hott_context_certificate_premise* context_certificate_premises,
	size_t context_certificate_premise_capacity,
	struct prototype_hott_substitution_certificate_premise* substitution_certificate_premises,
	size_t substitution_certificate_premise_capacity
);
const struct prototype_hott_candidate* prototype_hott_candidate_db_get(
	const struct prototype_hott_candidate_db* db,
	uint32_t candidate_id
);
int prototype_hott_candidate_db_validate(
	const struct prototype_hott_candidate_db* db,
	const struct prototype_hott_observation_goal_db* goals,
	const struct prototype_kernel_view* kernel
);

void prototype_hott_work_db_init(
	struct prototype_hott_work_db* db,
	struct prototype_hott_work_item* items,
	size_t item_capacity
);
const struct prototype_hott_work_item* prototype_hott_work_db_get(
	const struct prototype_hott_work_db* db,
	uint32_t work_item_id
);
int prototype_hott_work_db_validate(
	const struct prototype_hott_work_db* db,
	const struct prototype_hott_observation_goal_db* goals,
	const struct prototype_hott_candidate_db* candidates
);
int prototype_hott_observation_plan(
	struct prototype_hott_observation_goal_db* goals,
	struct prototype_hott_candidate_db* candidates,
	struct prototype_hott_work_db* work,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_work_item_id
);

struct prototype_hott_residual_obligation {
	uint32_t obligation_id;
	uint32_t work_item_id;
};

struct prototype_hott_residual_db {
	struct prototype_hott_residual_obligation* obligations;
	size_t obligation_count;
	size_t obligation_capacity;
};

void prototype_hott_residual_db_init(
	struct prototype_hott_residual_db* db,
	struct prototype_hott_residual_obligation* obligations,
	size_t obligation_capacity
);
int prototype_hott_residual_db_add_from_work(
	struct prototype_hott_residual_db* db,
	const struct prototype_hott_work_db* work,
	uint32_t work_item_id,
	uint32_t* p_obligation_id
);
int prototype_hott_residual_db_validate(
	const struct prototype_hott_residual_db* db,
	const struct prototype_hott_work_db* work
);
int prototype_hott_residual_db_require_artifact_empty(
	const struct prototype_hott_residual_db* db
);

enum prototype_hott_action_kind {
	PROTOTYPE_HOTT_ACTION_CONTEXT = 1,
	PROTOTYPE_HOTT_ACTION_SUBSTITUTION = 2,
	PROTOTYPE_HOTT_ACTION_TYPE = 3,
	PROTOTYPE_HOTT_ACTION_TERM = 4
};

enum prototype_hott_action_certificate_kind {
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_CONTEXT_BRIDGE = 1,
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_SUBSTITUTION_NATURALITY = 2,
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_TYPE = 3,
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM = 4
};

#define PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT 257

struct prototype_hott_context_action_key {
	uint32_t source_context_id;
};

struct prototype_hott_substitution_action_key {
	uint32_t source_substitution_id;
	uint32_t source_bridge_id;
	uint32_t target_bridge_id;
};

struct prototype_hott_type_action_key {
	uint32_t source_claim_id;
	uint32_t source_bridge_id;
};

struct prototype_hott_term_action_key {
	uint32_t source_claim_id;
	uint32_t source_bridge_id;
	uint32_t type_action_request_id;
};

struct prototype_hott_action_request {
	uint32_t id;
	int kind;
	union {
		struct prototype_hott_context_action_key context;
		struct prototype_hott_substitution_action_key substitution;
		struct prototype_hott_type_action_key type;
		struct prototype_hott_term_action_key term;
	} key;
	uint64_t key_hash;
	uint32_t hash_next;
};

struct prototype_hott_context_action_certificate {
	uint32_t result_bridge_id;
};

struct prototype_hott_substitution_action_certificate {
	uint32_t result_substitution_id;
	uint32_t result_substitution_certificate_id;
	uint32_t left_naturality_lhs_substitution_id;
	uint32_t left_naturality_rhs_substitution_id;
	uint32_t right_naturality_lhs_substitution_id;
	uint32_t right_naturality_rhs_substitution_id;
	int normalization_profile;
	uint64_t step_limit;
	uint64_t term_graph_revision;
};

struct prototype_hott_type_action_certificate {
	uint32_t endpoint_context_id;
	uint32_t left_endpoint_binding_id;
	uint32_t right_endpoint_binding_id;
	uint32_t relation_type_term_id;
	uint32_t relation_is_type_claim_id;
	uint32_t left_context_certificate_id;
	uint32_t right_context_certificate_id;
};

struct prototype_hott_term_action_certificate {
	uint32_t endpoint_instantiation_substitution_id;
	uint32_t left_endpoint_substitution_certificate_id;
	uint32_t right_endpoint_substitution_certificate_id;
	uint32_t witness_term_id;
	uint32_t witness_has_type_claim_id;
};

struct prototype_hott_action_certificate {
	uint32_t id;
	uint32_t request_id;
	int kind;
	union {
		struct prototype_hott_context_action_certificate context;
		struct prototype_hott_substitution_action_certificate substitution;
		struct prototype_hott_type_action_certificate type;
		struct prototype_hott_term_action_certificate term;
	} data;
};

struct prototype_hott_action_result {
	uint32_t id;
	uint32_t request_id;
	uint32_t certificate_id;
	struct prototype_hott_deterministic_outcome outcome;
};

struct prototype_hott_action_db {
	struct prototype_hott_action_request* requests;
	size_t request_count;
	size_t request_capacity;
	uint64_t request_intern_requests;
	uint64_t request_intern_hits;
	uint32_t request_index_heads[PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT];
	struct prototype_hott_action_certificate* certificates;
	size_t certificate_count;
	size_t certificate_capacity;
	struct prototype_hott_action_result* results;
	size_t result_count;
	size_t result_capacity;
	uint64_t outcome_publish_requests;
};

void prototype_hott_action_db_init(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action_request* requests,
	size_t request_capacity,
	struct prototype_hott_action_certificate* certificates,
	size_t certificate_capacity,
	struct prototype_hott_action_result* results,
	size_t result_capacity
);
const struct prototype_hott_action_request* prototype_hott_action_request_get(
	const struct prototype_hott_action_db* db,
	uint32_t request_id
);
const struct prototype_hott_action_result* prototype_hott_action_result_get(
	const struct prototype_hott_action_db* db,
	uint32_t result_id
);
int prototype_hott_action_request_intern(
	struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_hott_action_request request,
	uint32_t* p_request_id
);
int prototype_hott_action_certificate_add(
	struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_hott_action_certificate certificate,
	uint32_t* p_certificate_id
);
int prototype_hott_action_result_publish(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action_result result,
	uint32_t* p_result_id
);
int prototype_hott_action_db_validate(
	const struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges
);
int prototype_hott_execute_type_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
);
int prototype_hott_execute_substitution_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_result_id
);
int prototype_hott_execute_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
);
int prototype_hott_bridge_db_construct_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t type_action_request_id,
	uint32_t* p_bridge_id
);

struct prototype_hott_observation_execution {
	uint32_t work_item_id;
	uint32_t type_action_request_id;
	uint32_t type_action_result_id;
	uint32_t term_action_request_id;
	uint32_t term_action_result_id;
};

int prototype_hott_observation_plan_and_execute(
	struct prototype_hott_observation_goal_db* goals,
	struct prototype_hott_candidate_db* candidates,
	struct prototype_hott_work_db* work,
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	struct prototype_hott_observation_execution* p_execution
);

#endif
