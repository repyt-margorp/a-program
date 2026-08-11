#ifndef A_PROGRAM_PROTOTYPE_PARAMETRICITY_TYPES_H
#define A_PROGRAM_PROTOTYPE_PARAMETRICITY_TYPES_H

#include "a_program/identity/types.h"

enum prototype_parametricity_rule {
	PROTOTYPE_HOTT_RULE_NONE = 0,
	PROTOTYPE_HOTT_RULE_REL_DIAGONAL = 1,
	PROTOTYPE_HOTT_RULE_REL_CONVERT = 2,
	PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR = 3,
	PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT = 4,
	PROTOTYPE_HOTT_RULE_REL_MATCH_ACTION = 5,
	PROTOTYPE_HOTT_RULE_REL_COMP_RETURN = 6,
	PROTOTYPE_HOTT_RULE_REL_PI_POINTWISE = 7,
	PROTOTYPE_HOTT_RULE_REL_THUNK_PURE = 8,
	PROTOTYPE_HOTT_RULE_REL_REINDEX = 9
};

enum prototype_parametricity_candidate_object_result {
	PROTOTYPE_HOTT_CANDIDATE_OBJECT_INVALID = 0,
	PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS = 1,
	PROTOTYPE_HOTT_CANDIDATE_OBJECT_EMPTY_FAMILY = 2,
	PROTOTYPE_HOTT_CANDIDATE_OBJECT_DEFERRED = 3
};

enum prototype_parametricity_child_role {
	PROTOTYPE_HOTT_CHILD_NONE = 0,
	PROTOTYPE_HOTT_CHILD_ADT_FIELD = 1,
	PROTOTYPE_HOTT_CHILD_ADT_DEPENDENT_REINDEX = 2,
	PROTOTYPE_HOTT_CHILD_MATCH_SCRUTINEE = 3,
	PROTOTYPE_HOTT_CHILD_MATCH_MOTIVE_ACTION = 4,
	PROTOTYPE_HOTT_CHILD_MATCH_CASE_ACTION = 5,
	PROTOTYPE_HOTT_CHILD_MATCH_RECURSIVE_IH = 6,
	PROTOTYPE_HOTT_CHILD_COMP_LEFT_RETURN_EXPOSURE = 7,
	PROTOTYPE_HOTT_CHILD_COMP_RIGHT_RETURN_EXPOSURE = 8,
	PROTOTYPE_HOTT_CHILD_COMP_RESULT_RELATION = 9,
	PROTOTYPE_HOTT_CHILD_PI_DOMAIN_ACTION = 10,
	PROTOTYPE_HOTT_CHILD_PI_RELATED_INPUT = 11,
	PROTOTYPE_HOTT_CHILD_PI_CODOMAIN_RELATION = 12,
	PROTOTYPE_HOTT_CHILD_THUNK_COMPUTATION_RELATION = 13,
	PROTOTYPE_HOTT_CHILD_CONVERT_ANCHOR_RELATION = 14,
	PROTOTYPE_HOTT_CHILD_CONTEXT_ACTION = 15,
	PROTOTYPE_HOTT_CHILD_SUBSTITUTION_ACTION = 16,
	PROTOTYPE_HOTT_CHILD_RELATION_TYPE_ACTION = 17,
	PROTOTYPE_HOTT_CHILD_TERM_ACTION = 18,
	PROTOTYPE_HOTT_CHILD_REINDEX_NATURALITY = 19
};

struct prototype_parametricity_relation_goal {
	uint32_t id;
	int category;
	uint32_t left_carrier_claim_id;
	uint32_t right_carrier_claim_id;
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	uint32_t bridge_id;
};

struct prototype_parametricity_relation_goal_db {
	struct prototype_parametricity_relation_goal* goals;
	size_t goal_count;
	size_t goal_capacity;
};

struct prototype_parametricity_candidate {
	uint32_t id;
	uint32_t conclusion_goal_id;
	int rule;
	int object_result;
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

struct prototype_parametricity_claim_premise {
	uint32_t candidate_id;
	uint32_t claim_id;
	int role;
	uint32_t ordinal;
};

struct prototype_parametricity_child_edge {
	uint32_t candidate_id;
	uint32_t child_goal_id;
	int role;
	uint32_t ordinal;
};

struct prototype_parametricity_conversion_premise {
	uint32_t candidate_id;
	int role;
	uint32_t ordinal;
	struct prototype_kernel_conversion_goal request;
	uint64_t conversion_graph_revision;
};

struct prototype_parametricity_context_certificate_premise {
	uint32_t candidate_id;
	uint32_t certificate_id;
	int role;
	uint32_t ordinal;
};

struct prototype_parametricity_substitution_certificate_premise {
	uint32_t candidate_id;
	uint32_t certificate_id;
	int role;
	uint32_t ordinal;
};

struct prototype_parametricity_candidate_db {
	struct prototype_parametricity_candidate* candidates;
	size_t candidate_count;
	size_t candidate_capacity;
	struct prototype_parametricity_claim_premise* claim_premises;
	size_t claim_premise_count;
	size_t claim_premise_capacity;
	struct prototype_parametricity_child_edge* child_edges;
	size_t child_edge_count;
	size_t child_edge_capacity;
	struct prototype_parametricity_conversion_premise* conversion_premises;
	size_t conversion_premise_count;
	size_t conversion_premise_capacity;
	struct prototype_parametricity_context_certificate_premise*
		context_certificate_premises;
	size_t context_certificate_premise_count;
	size_t context_certificate_premise_capacity;
	struct prototype_parametricity_substitution_certificate_premise*
		substitution_certificate_premises;
	size_t substitution_certificate_premise_count;
	size_t substitution_certificate_premise_capacity;
};

struct prototype_parametricity_work_item {
	uint32_t id;
	uint32_t goal_id;
	uint32_t selected_candidate_id;
	uint32_t source_ast;
	struct prototype_hott_deterministic_outcome outcome;
};

struct prototype_parametricity_work_db {
	struct prototype_parametricity_work_item* items;
	size_t item_count;
	size_t item_capacity;
};

struct prototype_parametricity_residual_obligation {
	uint32_t obligation_id;
	uint32_t work_item_id;
};

struct prototype_parametricity_residual_db {
	struct prototype_parametricity_residual_obligation* obligations;
	size_t obligation_count;
	size_t obligation_capacity;
};

#endif
