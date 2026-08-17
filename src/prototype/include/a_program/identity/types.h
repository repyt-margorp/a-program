#ifndef __A_PROGRAM_IDENTITY_TYPES_H__
#define __A_PROGRAM_IDENTITY_TYPES_H__

#include "a_program/kernel/cwf_certificate.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/kernel/kernel_view.h"
#include "a_program/dimension/types.h"

struct prototype_artifact_interface;

enum prototype_hott_relation_category {
	PROTOTYPE_HOTT_RELATION_VALUE = 1,
	PROTOTYPE_HOTT_RELATION_COMPUTATION = 2
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
	PROTOTYPE_HOTT_TYPE_FORMER_RELATION = 7
};

enum prototype_hott_relation_type_action_rule {
	PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_NONE = 0,
	PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_ZERO_FIELD_ADT = 1,
	PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_ADT_TELESCOPE = 2,
	PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_PI_POINTWISE = 3,
	PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_PURE_COMPUTATION = 4,
	PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_THUNK = 5,
	PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_RELATION_HIGHER = 6
};

enum prototype_hott_relation_family_semantics {
	PROTOTYPE_HOTT_RELATION_FAMILY_INVALID = 0,
	/* Internal-parametricity action; this is not object identity evidence. */
	PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION = 1
};

enum prototype_hott_capability_status {
	PROTOTYPE_HOTT_CAPABILITY_UNDECLARED = 0,
	PROTOTYPE_HOTT_CAPABILITY_SUPPORTED = 1,
	PROTOTYPE_HOTT_CAPABILITY_DEFERRED = 2,
	PROTOTYPE_HOTT_CAPABILITY_NOT_APPLICABLE = 3
};

struct prototype_hott_type_former_capabilities {
	/* Relational type/term action is the internal-parametricity substrate. */
	int relation_type_action;
	int term_action;
	int ordinary_reindex;
	int purity;
	/* HOTT identity requires type-directed computation plus fibrancy data. */
	int identity_computation;
	int transport;
	int lifting;
	int resource_hook;
	int artifact;
};

struct prototype_hott_type_former_descriptor {
	int kind;
	int admitted;
	int relation_type_action_rule;
	int residual_reason;
	uint32_t source_claim_id;
	uint32_t source_type_term_id;
	struct prototype_hott_type_former_capabilities capabilities;
	uint64_t child_role_mask;
};

struct prototype_hott_bridge {
	uint32_t id;
	uint32_t source_context_id;
	uint32_t bridge_context_id;
	uint32_t dimension_operator_id;
	uint32_t target_dimension;
	uint32_t first_face_binding;
	uint32_t face_binding_count;
	uint32_t left_substitution_id;
	uint32_t right_substitution_id;
};

struct prototype_hott_bridge_face_binding {
	uint32_t bridge_id;
	uint32_t source_binding_id;
	uint32_t face_ordinal;
	uint32_t intrinsic_dimension;
	uint32_t target_binding_id;
	uint32_t target_context_id;
	uint32_t classifier_term_id;
	uint32_t context_certificate_id;
};

enum prototype_hott_bridge_semantics {
	PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY = 1,
	PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION = 2,
	PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY = 3
};

struct prototype_hott_bridge_certificate {
	uint32_t id;
	uint32_t bridge_id;
	int semantics;
	uint32_t parent_bridge_id;
	uint32_t fiber_action_certificate_id;
	uint32_t fiber_witness_claim_id;
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
	struct prototype_hott_bridge_face_binding* face_bindings;
	size_t face_binding_count;
	size_t face_binding_capacity;
	struct prototype_dimension_operator_db* dimension_operators;
	uint32_t default_extension_operator_id;
};

enum prototype_hott_action_kind {
	PROTOTYPE_HOTT_ACTION_CONTEXT = 1,
	PROTOTYPE_HOTT_ACTION_SUBSTITUTION = 2,
	PROTOTYPE_HOTT_ACTION_RELATION_TYPE = 3,
	PROTOTYPE_HOTT_ACTION_TERM = 4,
	PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION = 5,
	PROTOTYPE_HOTT_ACTION_OBJECT_TERM = 6
};

enum prototype_hott_action_certificate_kind {
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_CONTEXT_BRIDGE = 1,
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_SUBSTITUTION_NATURALITY = 2,
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE = 3,
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM = 4,
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION = 5,
	PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM = 6
};

#define PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT 257

struct prototype_hott_context_action_key {
	uint32_t source_context_id;
};

struct prototype_hott_substitution_action_key {
	struct prototype_certified_substitution_ref source;
	uint32_t source_bridge_id;
	uint32_t target_bridge_id;
};

struct prototype_hott_relation_type_action_key {
	uint32_t source_claim_id;
	uint32_t source_bridge_id;
};

struct prototype_hott_identity_type_computation_key {
	uint32_t source_claim_id;
	uint32_t source_bridge_id;
};

struct prototype_hott_term_action_key {
	uint32_t source_claim_id;
	uint32_t source_bridge_id;
	uint32_t relation_type_action_request_id;
};

struct prototype_hott_object_term_action_key {
	uint32_t source_claim_id;
	uint32_t source_bridge_id;
	uint32_t identity_type_action_request_id;
};

struct prototype_hott_action_request {
	uint32_t id;
	int kind;
	union {
		struct prototype_hott_context_action_key context;
		struct prototype_hott_substitution_action_key substitution;
		struct prototype_hott_relation_type_action_key relation_type;
		struct prototype_hott_term_action_key term;
		struct prototype_hott_identity_type_computation_key identity_type;
		struct prototype_hott_object_term_action_key object_term;
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

struct prototype_hott_relation_type_action_certificate {
	uint32_t endpoint_context_id;
	uint32_t left_endpoint_binding_id;
	uint32_t right_endpoint_binding_id;
	int relation_family_semantics;
	uint32_t relation_family_term_id;
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

struct prototype_hott_identity_type_computation_certificate {
	int computation_rule;
	uint32_t endpoint_context_id;
	uint32_t left_endpoint_binding_id;
	uint32_t right_endpoint_binding_id;
	uint32_t generated_type_declaration_id;
	uint32_t backing_type_former_term_id;
	uint32_t backing_type_former_has_type_claim_id;
	uint32_t identity_type_term_id;
	uint32_t identity_type_has_type_claim_id;
	uint32_t identity_type_is_type_claim_id;
	uint32_t left_context_certificate_id;
	uint32_t right_context_certificate_id;
	uint32_t pointwise_left_input_binding_id;
	uint32_t pointwise_right_input_binding_id;
	uint32_t pointwise_input_identity_binding_id;
};

struct prototype_hott_object_term_action_certificate {
	uint32_t left_endpoint_claim_id;
	uint32_t right_endpoint_claim_id;
	uint32_t identity_family_has_type_claim_id;
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
		struct prototype_hott_relation_type_action_certificate relation_type;
		struct prototype_hott_term_action_certificate term;
		struct prototype_hott_identity_type_computation_certificate identity_type;
		struct prototype_hott_object_term_action_certificate object_term;
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

enum prototype_hott_universe_correspondence_projection {
	PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION = 2,
	PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_RIGHT = 3,
	PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_LEFT = 4,
	PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT = 5,
	PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_LEFT = 6
};

struct prototype_hott_relation_execution {
	uint32_t work_item_id;
	uint32_t relation_type_action_request_id;
	uint32_t relation_type_action_result_id;
	uint32_t term_action_request_id;
	uint32_t term_action_result_id;
	int materialization_state;
	uint32_t relation_witness_claim_id;
};

enum prototype_hott_materialization_state {
	PROTOTYPE_HOTT_MATERIALIZATION_INVALID = 0,
	PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS = 1,
	PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY = 2,
	PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL = 3
};


#endif
