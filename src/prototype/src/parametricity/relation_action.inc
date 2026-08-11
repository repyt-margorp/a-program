#include "a_program/frontend/lowering.h"
#include "a_program/identity/types.h"
#include "a_program/identity/telescope_action.h"
#include "a_program/identity/context_bridge.h"
#include "a_program/parametricity/relation_action.h"
#include "a_program/identity/action_certificate.h"
#include "a_program/identity/identity_computation.h"
#include "a_program/identity/object_term_action.h"
#include "a_program/parametricity/execution.h"
#include "calculus.h"

#include <stdlib.h>
#include <string.h>

static int hott_generated_identity_declaration(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t source_carrier,
	uint32_t parameter_context,
	uint32_t* p_type_id
);
static int hott_build_nondependent_pi_identity_type(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t source_carrier,
	uint32_t left_endpoint_binding,
	uint32_t right_endpoint_binding,
	uint32_t x0_binding,
	uint32_t x1_binding,
	uint32_t xr_binding,
	uint32_t* p_identity_type
);
static int hott_term_is_constant_over_context(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t term_id
);
static int hott_ensure_is_type_claim_in_context(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t* p_claim_id
);
static int hott_action_result_for_request(
	const struct prototype_hott_action_db* db,
	uint32_t request_id,
	uint32_t* p_result_id
);
static int hott_apply_type_pointwise_identity_witness(
	struct prototype_kernel_builder* kernel,
	uint32_t function_identity_claim_id,
	const uint32_t* argument_claim_ids,
	uint32_t argument_count,
	uint32_t* p_term_id,
	uint32_t* p_claim_id
);

static int hott_category_is_valid(int category) {
	return category == PROTOTYPE_HOTT_RELATION_VALUE ||
		category == PROTOTYPE_HOTT_RELATION_COMPUTATION;
}

static int hott_residual_reason_is_valid(int reason) {
	return reason >= PROTOTYPE_HOTT_RESIDUAL_NONE &&
		reason <= PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
}

static int hott_deterministic_outcome_is_valid(
	const struct prototype_hott_deterministic_outcome* outcome,
	int allow_pending
) {
	if (!outcome ||
		outcome->state < (allow_pending ? PROTOTYPE_HOTT_OUTCOME_PENDING :
			PROTOTYPE_HOTT_OUTCOME_READY) ||
		outcome->state > PROTOTYPE_HOTT_OUTCOME_UNSUPPORTED ||
		!hott_residual_reason_is_valid(outcome->residual_reason) ||
		strcmp(
			outcome->calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
		) != 0) {
		return 0;
	}
	return ((outcome->state == PROTOTYPE_HOTT_OUTCOME_RESIDUAL ||
		outcome->state == PROTOTYPE_HOTT_OUTCOME_UNSUPPORTED) ==
		(outcome->residual_reason != PROTOTYPE_HOTT_RESIDUAL_NONE));
}

static int hott_rule_is_valid(int rule) {
	return rule >= PROTOTYPE_HOTT_RULE_REL_DIAGONAL &&
		rule <= PROTOTYPE_HOTT_RULE_REL_REINDEX;
}

static int hott_rule_object_result(int rule) {
	switch (rule) {
	case PROTOTYPE_HOTT_RULE_REL_DIAGONAL:
	case PROTOTYPE_HOTT_RULE_REL_CONVERT:
	case PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR:
	case PROTOTYPE_HOTT_RULE_REL_COMP_RETURN:
	case PROTOTYPE_HOTT_RULE_REL_PI_POINTWISE:
	case PROTOTYPE_HOTT_RULE_REL_THUNK_PURE:
		return PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS;
	case PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT:
		return PROTOTYPE_HOTT_CANDIDATE_OBJECT_EMPTY_FAMILY;
	case PROTOTYPE_HOTT_RULE_REL_MATCH_ACTION:
	case PROTOTYPE_HOTT_RULE_REL_REINDEX:
		return PROTOTYPE_HOTT_CANDIDATE_OBJECT_DEFERRED;
	default:
		return PROTOTYPE_HOTT_CANDIDATE_OBJECT_INVALID;
	}
}

static int hott_role_is_valid(int role) {
	return role >= PROTOTYPE_HOTT_CHILD_ADT_FIELD &&
		role <= PROTOTYPE_HOTT_CHILD_REINDEX_NATURALITY;
}

static int hott_rule_allows_role(int rule, int role) {
	switch (rule) {
	case PROTOTYPE_HOTT_RULE_REL_DIAGONAL:
	case PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT:
		return 0;
	case PROTOTYPE_HOTT_RULE_REL_CONVERT:
		return role == PROTOTYPE_HOTT_CHILD_CONVERT_ANCHOR_RELATION;
	case PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR:
		return role == PROTOTYPE_HOTT_CHILD_ADT_FIELD ||
			role == PROTOTYPE_HOTT_CHILD_ADT_DEPENDENT_REINDEX;
	case PROTOTYPE_HOTT_RULE_REL_MATCH_ACTION:
		return role >= PROTOTYPE_HOTT_CHILD_MATCH_SCRUTINEE &&
			role <= PROTOTYPE_HOTT_CHILD_MATCH_RECURSIVE_IH;
	case PROTOTYPE_HOTT_RULE_REL_COMP_RETURN:
		return role >= PROTOTYPE_HOTT_CHILD_COMP_LEFT_RETURN_EXPOSURE &&
			role <= PROTOTYPE_HOTT_CHILD_COMP_RESULT_RELATION;
	case PROTOTYPE_HOTT_RULE_REL_PI_POINTWISE:
		return role >= PROTOTYPE_HOTT_CHILD_PI_DOMAIN_ACTION &&
			role <= PROTOTYPE_HOTT_CHILD_PI_CODOMAIN_RELATION;
	case PROTOTYPE_HOTT_RULE_REL_THUNK_PURE:
		return role == PROTOTYPE_HOTT_CHILD_THUNK_COMPUTATION_RELATION;
	case PROTOTYPE_HOTT_RULE_REL_REINDEX:
		return role >= PROTOTYPE_HOTT_CHILD_CONTEXT_ACTION &&
			role <= PROTOTYPE_HOTT_CHILD_REINDEX_NATURALITY;
	default:
		return 0;
	}
}

static int hott_action_kind_is_valid(int kind) {
	return kind >= PROTOTYPE_HOTT_ACTION_CONTEXT &&
		kind <= PROTOTYPE_HOTT_ACTION_OBJECT_TERM;
}

static int hott_is_type_claim_matches(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	uint32_t context_id,
	uint32_t subject
) {
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	uint32_t classifier;
	if (!claim || prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id != context_id || prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject != subject ||
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id != PROTOTYPE_INVALID_ID ||
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ||
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier >= terms->term_count ||
		prototype_judgement_classifier_value_whnf(
			terms, type_declarations, prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier, &classifier
		) != 0) {
		return 0;
	}
	return classifier < terms->term_count &&
		terms->terms[classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR;
}

static int hott_operation_matches_claim(
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_claim* claim
) {
	if (!claim) {
		return 0;
	}
	if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id == PROTOTYPE_INVALID_ID) {
		return prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION;
	}
	if (!operations || prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id >= operations->operation_count ||
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ||
		prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_id != prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id) {
		return 0;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id];
	return operation->context_id == prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id &&
		operation->core_term == prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject &&
		operation->classifier == prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier;
}

static int context_has_formation_certificate(
	const struct prototype_cwf_certificate_db* certificates,
	uint32_t context_id
) {
	return prototype_cwf_certificate_db_has(
		certificates, PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION, context_id
	);
}

static int hott_context_is_formed(
	const struct prototype_context_db* contexts,
	const struct prototype_cwf_certificate_db* certificates,
	uint32_t context_id
) {
	if (!contexts || context_id >= contexts->context_count) {
		return 0;
	}
	uint32_t empty = prototype_context_empty(contexts);
	uint32_t cursor = context_id;
	while (cursor != empty) {
		const struct prototype_context* context = prototype_context_get(
			contexts, cursor
		);
		if (!context || context->parent >= cursor ||
			!context_has_formation_certificate(certificates, cursor)) {
			return 0;
		}
		cursor = context->parent;
	}
	return cursor == empty;
}

void prototype_hott_bridge_db_init(
	struct prototype_hott_bridge_db* db,
	struct prototype_hott_bridge* bridges,
	size_t bridge_capacity,
	struct prototype_hott_bridge_certificate* certificates,
	size_t certificate_capacity
) {
	if (!db) {
		return;
	}
	db->bridges = bridges;
	db->bridge_count = 0;
	db->bridge_capacity = bridge_capacity;
	db->certificates = certificates;
	db->certificate_count = 0;
	db->certificate_capacity = certificate_capacity;
}

const struct prototype_hott_bridge* prototype_hott_bridge_db_get(
	const struct prototype_hott_bridge_db* db,
	uint32_t bridge_id
) {
	return db && bridge_id < db->bridge_count ? &db->bridges[bridge_id] : NULL;
}

static int hott_bridge_record_is_valid(
	const struct prototype_hott_bridge* bridge,
	uint32_t expected_id,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_cwf_certificate_db* cwf_certificates
) {
	if (!bridge || !contexts || !substitutions || !cwf_certificates ||
		bridge->id != expected_id ||
		bridge->source_context_id >= contexts->context_count ||
		bridge->bridge_context_id >= contexts->context_count ||
		!hott_context_is_formed(
			contexts, cwf_certificates, bridge->source_context_id
		) || !hott_context_is_formed(
			contexts, cwf_certificates, bridge->bridge_context_id
		)) {
		return 0;
	}
	const struct prototype_substitution* left = prototype_substitution_get(
		substitutions, bridge->left_substitution_id
	);
	const struct prototype_substitution* right = prototype_substitution_get(
		substitutions, bridge->right_substitution_id
	);
	if (!left || !right || left->source_context != bridge->bridge_context_id ||
		right->source_context != bridge->bridge_context_id ||
		left->target_context != bridge->source_context_id ||
		right->target_context != bridge->source_context_id) {
		return 0;
	}
	if (bridge->source_context_id == prototype_context_empty(contexts)) {
		return bridge->bridge_context_id == bridge->source_context_id &&
			left->kind == PROTOTYPE_SUBSTITUTION_IDENTITY &&
			right->kind == PROTOTYPE_SUBSTITUTION_IDENTITY &&
			bridge->left_substitution_id == bridge->right_substitution_id;
	}
	return left->kind == PROTOTYPE_SUBSTITUTION_EXTEND &&
		right->kind == PROTOTYPE_SUBSTITUTION_EXTEND;
}

int prototype_hott_bridge_db_construct(
	struct prototype_hott_bridge_db* db,
	struct prototype_kernel_builder* kernel,
	uint32_t source_context_id,
	uint32_t* p_bridge_id
) {
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!db || !db->bridges || !contexts || !substitutions ||
		prototype_kernel_builder_validate(kernel) != 0 || !terms || !type_declarations ||
		!judgement || !p_bridge_id ||
		source_context_id >= contexts->context_count ||
		prototype_cwf_certificate_db_validate_contexts(
			cwf_certificates, contexts, terms, type_declarations, judgement
		) != 0 ||
		prototype_cwf_certificate_db_validate_substitutions(
			cwf_certificates, substitutions, judgement
		) != 0 ||
		!hott_context_is_formed(contexts, cwf_certificates, source_context_id)) {
		return -1;
	}
	if (source_context_id != prototype_context_empty(contexts)) {
		return 1;
	}
	for (uint32_t i = 0; i < db->bridge_count; ++i) {
		if (db->bridges[i].source_context_id == source_context_id) {
			*p_bridge_id = i;
			return 0;
		}
	}
	uint32_t identity;
	if (prototype_substitution_identity(
			substitutions, contexts, source_context_id, &identity
		) != 0 || db->bridge_count >= db->bridge_capacity) {
		return -1;
	}
	struct prototype_hott_bridge bridge = {
		.id = (uint32_t)db->bridge_count,
		.source_context_id = source_context_id,
		.bridge_context_id = source_context_id,
		.left_substitution_id = identity,
		.right_substitution_id = identity
	};
	if (!hott_bridge_record_is_valid(
			&bridge, bridge.id, contexts, substitutions, cwf_certificates
		)) {
		return -1;
	}
	if (!db->certificates || db->certificate_count >= db->certificate_capacity) {
		return -1;
	}
	db->bridges[db->bridge_count++] = bridge;
	db->certificates[db->certificate_count++] =
		(struct prototype_hott_bridge_certificate) {
		.id = bridge.id,
		.bridge_id = bridge.id,
		.semantics = PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY,
		.parent_bridge_id = PROTOTYPE_INVALID_ID,
		.fiber_action_certificate_id = PROTOTYPE_INVALID_ID,
		.fiber_witness_claim_id = PROTOTYPE_INVALID_ID,
		.left_endpoint_context_certificate_id = PROTOTYPE_INVALID_ID,
		.right_endpoint_context_certificate_id = PROTOTYPE_INVALID_ID,
		.relation_context_certificate_id = PROTOTYPE_INVALID_ID,
		.left_substitution_certificate_id = PROTOTYPE_INVALID_ID,
		.right_substitution_certificate_id = PROTOTYPE_INVALID_ID
	};
	*p_bridge_id = bridge.id;
	return 0;
}

int prototype_hott_bridge_db_validate(
	const struct prototype_hott_bridge_db* db,
	const struct prototype_kernel_view* kernel
) {
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!db || db->bridge_count > db->bridge_capacity ||
		prototype_kernel_view_validate(kernel) != 0 ||
		db->certificate_count > db->certificate_capacity ||
		db->certificate_count != db->bridge_count ||
		(db->bridge_count != 0 && !db->bridges) ||
		(db->certificate_count != 0 && !db->certificates) ||
		prototype_cwf_certificate_db_validate_contexts(
			cwf_certificates, contexts, terms, type_declarations, judgement
		) != 0 || prototype_cwf_certificate_db_validate_substitutions(
			cwf_certificates, substitutions, judgement
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < db->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* certificate =
			&db->certificates[i];
		if (!hott_bridge_record_is_valid(
				&db->bridges[i], i, contexts, substitutions,
				cwf_certificates
			) || certificate->id != i || certificate->bridge_id != i ||
			(certificate->parent_bridge_id == PROTOTYPE_INVALID_ID) !=
				(db->bridges[i].source_context_id ==
				 prototype_context_empty(contexts))) {
			return -1;
		}
			if (certificate->parent_bridge_id == PROTOTYPE_INVALID_ID &&
				(certificate->semantics !=
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY ||
				 certificate->fiber_action_certificate_id != PROTOTYPE_INVALID_ID ||
				 certificate->fiber_witness_claim_id != PROTOTYPE_INVALID_ID ||
			 certificate->left_endpoint_context_certificate_id !=
				PROTOTYPE_INVALID_ID ||
			 certificate->right_endpoint_context_certificate_id !=
				PROTOTYPE_INVALID_ID ||
			 certificate->relation_context_certificate_id != PROTOTYPE_INVALID_ID ||
			 certificate->left_substitution_certificate_id != PROTOTYPE_INVALID_ID ||
			 certificate->right_substitution_certificate_id != PROTOTYPE_INVALID_ID)) {
			return -1;
		}
		if (certificate->parent_bridge_id != PROTOTYPE_INVALID_ID) {
			const struct prototype_context* source = prototype_context_get(
				contexts, db->bridges[i].source_context_id
			);
			const struct prototype_context* relation = prototype_context_get(
				contexts, db->bridges[i].bridge_context_id
			);
			const struct prototype_hott_bridge* parent =
				prototype_hott_bridge_db_get(db, certificate->parent_bridge_id);
			const struct prototype_cwf_certificate* left_certificate =
				prototype_cwf_certificate_db_get_kind(
					cwf_certificates,
					certificate->left_substitution_certificate_id,
					PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
				);
			const struct prototype_cwf_certificate* right_certificate =
				prototype_cwf_certificate_db_get_kind(
					cwf_certificates,
					certificate->right_substitution_certificate_id,
					PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
				);
			const struct prototype_judgement_proposition* witness =
				prototype_judgement_claim_proposition(
					judgement, certificate->fiber_witness_claim_id
				);
			const struct prototype_term* witness_term = witness &&
				witness->subject < terms->term_count ?
				&terms->terms[witness->subject] : NULL;
			if (!source || !relation || !parent ||
				(certificate->semantics !=
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION &&
				 certificate->semantics !=
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) ||
				certificate->parent_bridge_id >= i ||
				source->parent != parent->source_context_id ||
				relation->parent == prototype_context_empty(contexts) ||
				certificate->fiber_action_certificate_id == PROTOTYPE_INVALID_ID ||
				certificate->fiber_witness_claim_id == PROTOTYPE_INVALID_ID ||
				!witness || witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				witness->context_id != db->bridges[i].bridge_context_id ||
				witness->classifier != prototype_context_classifier_term(relation) ||
				!witness_term || witness_term->tag != PROTOTYPE_TERM_VAR ||
				witness_term->as.var.binding_id != relation->binding_id ||
				certificate->left_endpoint_context_certificate_id >=
					cwf_certificates->certificate_count ||
				certificate->right_endpoint_context_certificate_id >=
					cwf_certificates->certificate_count ||
				certificate->relation_context_certificate_id >=
					cwf_certificates->certificate_count ||
				cwf_certificates->certificates[
					certificate->right_endpoint_context_certificate_id
				].structural_id != relation->parent ||
				cwf_certificates->certificates[
					certificate->left_endpoint_context_certificate_id
				].structural_id != prototype_context_get(
					contexts, relation->parent
				)->parent ||
				cwf_certificates->certificates[
					certificate->relation_context_certificate_id
				].structural_id != db->bridges[i].bridge_context_id ||
				!left_certificate || !right_certificate ||
				left_certificate->structural_id !=
					db->bridges[i].left_substitution_id ||
				right_certificate->structural_id !=
					db->bridges[i].right_substitution_id) {
				return -1;
			}
		}
	}
	return 0;
}

void prototype_parametricity_relation_goal_db_init(
	struct prototype_parametricity_relation_goal_db* db,
	struct prototype_parametricity_relation_goal* goals,
	size_t goal_capacity
) {
	if (!db) {
		return;
	}
	db->goals = goals;
	db->goal_count = 0;
	db->goal_capacity = goal_capacity;
}

const struct prototype_parametricity_relation_goal*
prototype_parametricity_relation_goal_db_get(
	const struct prototype_parametricity_relation_goal_db* db,
	uint32_t goal_id
) {
	return db && goal_id < db->goal_count ? &db->goals[goal_id] : NULL;
}

static int hott_relation_goal_is_valid(
	const struct prototype_parametricity_relation_goal* goal,
	uint32_t expected_id,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_cwf_certificate_db* cwf_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	if (!goal || !hott_category_is_valid(goal->category) || goal->id != expected_id ||
		prototype_cwf_certificate_db_validate_contexts(
			cwf_certificates, contexts, terms, type_declarations, judgement
		) != 0 || prototype_cwf_certificate_db_validate_substitutions(
			cwf_certificates, substitutions, judgement
		) != 0) {
		return 0;
	}
	const struct prototype_judgement_claim* left_carrier =
		prototype_judgement_claim_get(
			judgement, goal->left_carrier_claim_id
		);
	const struct prototype_judgement_claim* right_carrier =
		prototype_judgement_claim_get(
			judgement, goal->right_carrier_claim_id
		);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, goal->left_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, goal->right_claim_id);
	if (!left_carrier || !right_carrier || !left || !right || prototype_judgement_proposition_get(judgement, left->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, right->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id ||
		prototype_judgement_proposition_get(judgement, left_carrier->proposition_id)->subject != prototype_judgement_proposition_get(judgement, left->proposition_id)->classifier ||
		prototype_judgement_proposition_get(judgement, right_carrier->proposition_id)->subject != prototype_judgement_proposition_get(judgement, right->proposition_id)->classifier ||
		!hott_is_type_claim_matches(
			terms, type_declarations, judgement, goal->left_carrier_claim_id,
			prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id, prototype_judgement_proposition_get(judgement, left_carrier->proposition_id)->subject
		) || !hott_is_type_claim_matches(
			terms, type_declarations, judgement, goal->right_carrier_claim_id,
			prototype_judgement_proposition_get(judgement, right->proposition_id)->context_id, prototype_judgement_proposition_get(judgement, right_carrier->proposition_id)->subject
		) || !hott_operation_matches_claim(operations, judgement, left) ||
		!hott_operation_matches_claim(operations, judgement, right) ||
		!hott_context_is_formed(contexts, cwf_certificates, prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id)) {
		return 0;
	}
	int left_category;
	int right_category;
	if (prototype_judgement_claim_category(
			judgement, terms, type_declarations, NULL, operations,
			goal->left_claim_id, &left_category
		) != 0 || prototype_judgement_claim_category(
			judgement, terms, type_declarations, NULL, operations,
			goal->right_claim_id, &right_category
		) != 0 || left_category != right_category ||
		(goal->category == PROTOTYPE_HOTT_RELATION_VALUE &&
		 left_category != PROTOTYPE_JUDGEMENT_CATEGORY_VALUE) ||
		(goal->category == PROTOTYPE_HOTT_RELATION_COMPUTATION &&
		 left_category != PROTOTYPE_JUDGEMENT_CATEGORY_COMPUTATION)) {
		return 0;
	}
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, goal->bridge_id
	);
	return bridge && bridge->source_context_id == prototype_judgement_proposition_get(judgement, left->proposition_id)->context_id &&
		hott_bridge_record_is_valid(
			bridge, goal->bridge_id, contexts, substitutions,
			cwf_certificates
		);
}

int prototype_parametricity_relation_goal_db_intern(
	struct prototype_parametricity_relation_goal_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	int category,
	uint32_t left_carrier_claim_id,
	uint32_t right_carrier_claim_id,
	uint32_t left_claim_id,
	uint32_t right_claim_id,
	uint32_t bridge_id,
	uint32_t* p_goal_id
) {
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	const struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!db || !p_goal_id || !db->goals ||
		prototype_kernel_view_validate(kernel) != 0) {
		return -1;
	}
	struct prototype_parametricity_relation_goal goal = {
		.id = (uint32_t)db->goal_count,
		.category = category,
		.left_carrier_claim_id = left_carrier_claim_id,
		.right_carrier_claim_id = right_carrier_claim_id,
		.left_claim_id = left_claim_id,
		.right_claim_id = right_claim_id,
		.bridge_id = bridge_id
	};
	if (!hott_relation_goal_is_valid(
			&goal, goal.id, contexts, substitutions, cwf_certificates,
			bridges, terms, type_declarations, operations, judgement
		)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->goal_count; ++i) {
		const struct prototype_parametricity_relation_goal* old = &db->goals[i];
		if (old->category == category &&
			old->left_carrier_claim_id == left_carrier_claim_id &&
			old->right_carrier_claim_id == right_carrier_claim_id &&
			old->left_claim_id == left_claim_id &&
			old->right_claim_id == right_claim_id && old->bridge_id == bridge_id) {
			*p_goal_id = i;
			return 0;
		}
	}
	if (db->goal_count >= db->goal_capacity) {
		return -1;
	}
	db->goals[db->goal_count++] = goal;
	*p_goal_id = goal.id;
	return 0;
}

int prototype_parametricity_relation_goal_db_validate(
	const struct prototype_parametricity_relation_goal_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges
) {
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	const struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!db || db->goal_count > db->goal_capacity ||
		(db->goal_count != 0 && !db->goals) ||
		prototype_kernel_view_validate(kernel) != 0 ||
		prototype_hott_bridge_db_validate(bridges, kernel) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < db->goal_count; ++i) {
		if (!hott_relation_goal_is_valid(
				&db->goals[i], i, contexts, substitutions, cwf_certificates,
				bridges, terms, type_declarations, operations, judgement
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			const struct prototype_parametricity_relation_goal* a = &db->goals[i];
			const struct prototype_parametricity_relation_goal* b = &db->goals[j];
			if (a->category == b->category &&
				a->left_carrier_claim_id == b->left_carrier_claim_id &&
				a->right_carrier_claim_id == b->right_carrier_claim_id &&
				a->left_claim_id == b->left_claim_id &&
				a->right_claim_id == b->right_claim_id &&
				a->bridge_id == b->bridge_id) {
				return -1;
			}
		}
	}
	return 0;
}

void prototype_parametricity_candidate_db_init(
	struct prototype_parametricity_candidate_db* db,
	struct prototype_parametricity_candidate* candidates,
	size_t candidate_capacity,
	struct prototype_parametricity_claim_premise* claim_premises,
	size_t claim_premise_capacity,
	struct prototype_parametricity_child_edge* child_edges,
	size_t child_edge_capacity,
	struct prototype_parametricity_conversion_premise* conversion_premises,
	size_t conversion_premise_capacity,
	struct prototype_parametricity_context_certificate_premise* context_certificate_premises,
	size_t context_certificate_premise_capacity,
	struct prototype_parametricity_substitution_certificate_premise* substitution_certificate_premises,
	size_t substitution_certificate_premise_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->candidates = candidates;
	db->candidate_capacity = candidate_capacity;
	db->claim_premises = claim_premises;
	db->claim_premise_capacity = claim_premise_capacity;
	db->child_edges = child_edges;
	db->child_edge_capacity = child_edge_capacity;
	db->conversion_premises = conversion_premises;
	db->conversion_premise_capacity = conversion_premise_capacity;
	db->context_certificate_premises = context_certificate_premises;
	db->context_certificate_premise_capacity = context_certificate_premise_capacity;
	db->substitution_certificate_premises = substitution_certificate_premises;
	db->substitution_certificate_premise_capacity =
		substitution_certificate_premise_capacity;
}

const struct prototype_parametricity_candidate* prototype_parametricity_candidate_db_get(
	const struct prototype_parametricity_candidate_db* db,
	uint32_t candidate_id
) {
	return db && candidate_id < db->candidate_count ?
		&db->candidates[candidate_id] : NULL;
}

static int hott_candidate_add(
	struct prototype_parametricity_candidate_db* db,
	uint32_t goal_id,
	int rule,
	uint32_t* p_candidate_id
) {
	if (!db || !db->candidates || !p_candidate_id || !hott_rule_is_valid(rule)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->candidate_count; ++i) {
		if (db->candidates[i].conclusion_goal_id == goal_id &&
			db->candidates[i].rule == rule) {
			*p_candidate_id = i;
			return 0;
		}
	}
	if (db->candidate_count >= db->candidate_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)db->candidate_count++;
	db->candidates[id] = (struct prototype_parametricity_candidate){
		.id = id,
		.conclusion_goal_id = goal_id,
		.rule = rule,
		.object_result = hott_rule_object_result(rule),
		.first_claim_premise = (uint32_t)db->claim_premise_count,
		.first_child_edge = (uint32_t)db->child_edge_count,
		.first_conversion_premise = (uint32_t)db->conversion_premise_count,
		.first_context_certificate_premise =
			(uint32_t)db->context_certificate_premise_count,
		.first_substitution_certificate_premise =
			(uint32_t)db->substitution_certificate_premise_count
	};
	*p_candidate_id = id;
	return 0;
}

static int hott_candidate_add_conversion(
	struct prototype_parametricity_candidate_db* db,
	uint32_t candidate_id,
	int role,
	uint32_t ordinal,
	struct prototype_kernel_conversion_goal request,
	uint64_t graph_revision
) {
	if (!db || candidate_id >= db->candidate_count ||
		db->conversion_premise_count >= db->conversion_premise_capacity ||
		!db->conversion_premises || !hott_role_is_valid(role)) {
		return -1;
	}
	struct prototype_parametricity_candidate* candidate = &db->candidates[candidate_id];
	if (candidate->first_conversion_premise + candidate->conversion_premise_count !=
		db->conversion_premise_count) {
		return -1;
	}
	db->conversion_premises[db->conversion_premise_count++] =
		(struct prototype_parametricity_conversion_premise){
			.candidate_id = candidate_id,
			.role = role,
			.ordinal = ordinal,
			.request = request,
			.conversion_graph_revision = graph_revision
		};
	++candidate->conversion_premise_count;
	return 0;
}

static int hott_candidate_add_child(
	struct prototype_parametricity_candidate_db* db,
	uint32_t candidate_id,
	uint32_t child_goal_id,
	int role,
	uint32_t ordinal
) {
	if (!db || candidate_id >= db->candidate_count || !db->child_edges ||
		db->child_edge_count >= db->child_edge_capacity ||
		!hott_rule_allows_role(db->candidates[candidate_id].rule, role)) {
		return -1;
	}
	struct prototype_parametricity_candidate* candidate = &db->candidates[candidate_id];
	if (candidate->first_child_edge + candidate->child_edge_count !=
		db->child_edge_count) {
		return -1;
	}
	db->child_edges[db->child_edge_count++] = (struct prototype_parametricity_child_edge){
		.candidate_id = candidate_id,
		.child_goal_id = child_goal_id,
		.role = role,
		.ordinal = ordinal
	};
	++candidate->child_edge_count;
	return 0;
}

static int conversion_premise_is_valid(
	const struct prototype_parametricity_conversion_premise* premise,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms
) {
	if (!premise) {
		return 0;
	}
	const struct prototype_term_conversion_result* result = &premise->request.result;
	return hott_role_is_valid(premise->role) &&
		prototype_judgement_kernel_conversion_goal_validate(
			contexts, terms, &premise->request, 1
		) == 0 && result->status == PROTOTYPE_TERM_CONVERSION_EQUAL &&
		result->reason == PROTOTYPE_TERM_CONVERSION_REASON_NONE &&
		result->profile == premise->request.normalization_profile &&
		result->left == premise->request.left_term &&
		result->right == premise->request.right_term &&
		result->step_limit == premise->request.step_limit &&
		result->graph_revision == premise->conversion_graph_revision;
}

static int candidate_slices_are_valid(
	const struct prototype_parametricity_candidate_db* db,
	const struct prototype_parametricity_candidate* candidate
) {
	return candidate->first_claim_premise + candidate->claim_premise_count <=
			db->claim_premise_count &&
		candidate->first_child_edge + candidate->child_edge_count <=
			db->child_edge_count &&
		candidate->first_conversion_premise + candidate->conversion_premise_count <=
			db->conversion_premise_count &&
		candidate->first_context_certificate_premise +
			candidate->context_certificate_premise_count <=
			db->context_certificate_premise_count &&
		candidate->first_substitution_certificate_premise +
			candidate->substitution_certificate_premise_count <=
			db->substitution_certificate_premise_count;
}

static int candidate_cycle_visit(
	const struct prototype_parametricity_candidate_db* db,
	uint32_t goal_id,
	unsigned char* colors,
	size_t goal_count
) {
	if (goal_id >= goal_count || colors[goal_id] == 1) {
		return -1;
	}
	if (colors[goal_id] == 2) {
		return 0;
	}
	colors[goal_id] = 1;
	for (uint32_t i = 0; i < db->candidate_count; ++i) {
		const struct prototype_parametricity_candidate* candidate = &db->candidates[i];
		if (candidate->conclusion_goal_id != goal_id) {
			continue;
		}
		for (uint32_t j = 0; j < candidate->child_edge_count; ++j) {
			const struct prototype_parametricity_child_edge* edge =
				&db->child_edges[candidate->first_child_edge + j];
			if (candidate_cycle_visit(
					db, edge->child_goal_id, colors, goal_count
				) != 0) {
				return -1;
			}
		}
	}
	colors[goal_id] = 2;
	return 0;
}

int prototype_parametricity_candidate_db_validate(
	const struct prototype_parametricity_candidate_db* db,
	const struct prototype_parametricity_relation_goal_db* goals,
	const struct prototype_kernel_view* kernel
) {
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ?
		(struct prototype_judgement_db*)kernel->judgement : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	if (!db || !goals || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !terms || !type_declarations || !judgement ||
		!operations || prototype_kernel_view_validate(kernel) != 0 ||
		db->candidate_count > db->candidate_capacity ||
		db->claim_premise_count > db->claim_premise_capacity ||
		db->child_edge_count > db->child_edge_capacity ||
		db->conversion_premise_count > db->conversion_premise_capacity ||
		db->context_certificate_premise_count >
			db->context_certificate_premise_capacity ||
		db->substitution_certificate_premise_count >
			db->substitution_certificate_premise_capacity) {
		return -1;
	}
	for (uint32_t i = 0; i < db->candidate_count; ++i) {
		const struct prototype_parametricity_candidate* candidate = &db->candidates[i];
		if (candidate->id != i || candidate->conclusion_goal_id >= goals->goal_count ||
			!hott_rule_is_valid(candidate->rule) ||
			candidate->object_result !=
				hott_rule_object_result(candidate->rule) ||
			!candidate_slices_are_valid(db, candidate)) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (db->candidates[j].conclusion_goal_id ==
					candidate->conclusion_goal_id &&
				db->candidates[j].rule == candidate->rule) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < candidate->claim_premise_count; ++j) {
			const struct prototype_parametricity_claim_premise* edge =
				&db->claim_premises[candidate->first_claim_premise + j];
			if (edge->candidate_id != i || !hott_role_is_valid(edge->role) ||
				!hott_rule_allows_role(candidate->rule, edge->role) ||
				!prototype_judgement_claim_get(judgement, edge->claim_id)) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < candidate->child_edge_count; ++j) {
			const struct prototype_parametricity_child_edge* edge =
				&db->child_edges[candidate->first_child_edge + j];
			if (edge->candidate_id != i || !hott_role_is_valid(edge->role) ||
				!hott_rule_allows_role(candidate->rule, edge->role) ||
				edge->child_goal_id >= goals->goal_count) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < candidate->conversion_premise_count; ++j) {
			const struct prototype_parametricity_conversion_premise* edge =
				&db->conversion_premises[candidate->first_conversion_premise + j];
			if (edge->candidate_id != i ||
				!hott_rule_allows_role(candidate->rule, edge->role) ||
				!conversion_premise_is_valid(edge, contexts, terms)) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < candidate->context_certificate_premise_count; ++j) {
			const struct prototype_parametricity_context_certificate_premise* edge =
				&db->context_certificate_premises[
					candidate->first_context_certificate_premise + j
				];
			if (edge->candidate_id != i || !hott_role_is_valid(edge->role) ||
				!hott_rule_allows_role(candidate->rule, edge->role) ||
				edge->certificate_id >= cwf_certificates->certificate_count) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < candidate->substitution_certificate_premise_count;
			++j) {
			const struct prototype_parametricity_substitution_certificate_premise* edge =
				&db->substitution_certificate_premises[
					candidate->first_substitution_certificate_premise + j
				];
			if (edge->candidate_id != i || !hott_role_is_valid(edge->role) ||
				!hott_rule_allows_role(candidate->rule, edge->role) ||
				edge->certificate_id >= cwf_certificates->certificate_count) {
				return -1;
			}
		}
		if ((candidate->rule == PROTOTYPE_HOTT_RULE_REL_CONVERT &&
			 candidate->conversion_premise_count != 1) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_REL_COMP_RETURN &&
			 candidate->conversion_premise_count != 2) ||
			(candidate->rule != PROTOTYPE_HOTT_RULE_REL_CONVERT &&
			 candidate->rule != PROTOTYPE_HOTT_RULE_REL_COMP_RETURN &&
			 candidate->conversion_premise_count != 0)) {
			return -1;
		}
		if ((candidate->rule == PROTOTYPE_HOTT_RULE_REL_CONVERT &&
			 candidate->child_edge_count != 1) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_REL_COMP_RETURN &&
			 candidate->child_edge_count != 1) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_REL_THUNK_PURE &&
			 candidate->child_edge_count != 1) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_REL_DIAGONAL &&
			 candidate->child_edge_count != 0) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT &&
			 candidate->child_edge_count != 0)) {
			return -1;
		}
	}
	unsigned char* colors = calloc(goals->goal_count ? goals->goal_count : 1, 1);
	if (!colors) {
		return -1;
	}
	int result = 0;
	for (uint32_t i = 0; i < goals->goal_count; ++i) {
		if (candidate_cycle_visit(db, i, colors, goals->goal_count) != 0) {
			result = -1;
			break;
		}
	}
	free(colors);
	return result;
}

void prototype_parametricity_work_db_init(
	struct prototype_parametricity_work_db* db,
	struct prototype_parametricity_work_item* items,
	size_t item_capacity
) {
	if (!db) {
		return;
	}
	db->items = items;
	db->item_count = 0;
	db->item_capacity = item_capacity;
}

const struct prototype_parametricity_work_item* prototype_parametricity_work_db_get(
	const struct prototype_parametricity_work_db* db,
	uint32_t work_item_id
) {
	return db && work_item_id < db->item_count ? &db->items[work_item_id] : NULL;
}

int prototype_parametricity_work_db_validate(
	const struct prototype_parametricity_work_db* db,
	const struct prototype_parametricity_relation_goal_db* goals,
	const struct prototype_parametricity_candidate_db* candidates
) {
	if (!db || !goals || !candidates || db->item_count > db->item_capacity ||
		(db->item_count != 0 && !db->items)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->item_count; ++i) {
		const struct prototype_parametricity_work_item* item = &db->items[i];
		if (item->id != i || item->goal_id >= goals->goal_count ||
			!hott_deterministic_outcome_is_valid(&item->outcome, 1) ||
			(item->outcome.state == PROTOTYPE_HOTT_WORK_READY &&
			 item->selected_candidate_id == PROTOTYPE_INVALID_ID) ||
			(item->selected_candidate_id != PROTOTYPE_INVALID_ID &&
			 (item->selected_candidate_id >= candidates->candidate_count ||
			  candidates->candidates[item->selected_candidate_id].conclusion_goal_id !=
				item->goal_id))) {
			return -1;
		}
	}
	return 0;
}

static int hott_term_fragment_scan(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t depth,
	int* p_reason
) {
	if (!terms || !p_reason || term_id >= terms->term_count || depth > 512) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
	case PROTOTYPE_TERM_OPERATION_REQUEST:
		*p_reason = PROTOTYPE_HOTT_RESIDUAL_OPERATION_REQUEST;
		return 1;
	case PROTOTYPE_TERM_COMPUTATION_FOLD:
		*p_reason = PROTOTYPE_HOTT_RESIDUAL_COMPUTATION_FOLD;
		return 1;
	case PROTOTYPE_TERM_PURE_PRIMITIVE:
	case PROTOTYPE_TERM_EFFECT_OPERATION:
		*p_reason = PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE;
		return 1;
	case PROTOTYPE_TERM_COMPUTATION_TYPE:
		{
			int purity = prototype_term_effect_row_purity(
				terms, term->as.computation_type.label
			);
			if (purity == PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL) {
				*p_reason = PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL;
				return 1;
			}
			if (purity != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
				*p_reason = PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED;
				return purity == PROTOTYPE_EFFECT_ROW_PURITY_INVALID ? -1 : 1;
			}
			return hott_term_fragment_scan(
				terms, term->as.computation_type.result, depth + 1, p_reason
			);
		}
	case PROTOTYPE_TERM_APP:
		{
			int status = hott_term_fragment_scan(
				terms, term->as.app.function, depth + 1, p_reason
			);
			return status != 0 ? status : hott_term_fragment_scan(
				terms, term->as.app.argument, depth + 1, p_reason
			);
		}
	case PROTOTYPE_TERM_PI:
		{
			int status = hott_term_fragment_scan(
				terms, term->as.pi.domain, depth + 1, p_reason
			);
			return status != 0 ? status : hott_term_fragment_scan(
				terms, term->as.pi.codomain_family, depth + 1, p_reason
			);
		}
	case PROTOTYPE_TERM_LAMBDA:
		return hott_term_fragment_scan(
			terms, term->as.lambda.body, depth + 1, p_reason
		);
	case PROTOTYPE_TERM_RETURN:
		return hott_term_fragment_scan(
			terms, term->as.return_term.value, depth + 1, p_reason
		);
	case PROTOTYPE_TERM_THUNK:
		return hott_term_fragment_scan(
			terms, term->as.thunk.computation, depth + 1, p_reason
		);
	case PROTOTYPE_TERM_FORCE:
		return hott_term_fragment_scan(
			terms, term->as.force.value, depth + 1, p_reason
		);
	case PROTOTYPE_TERM_THUNK_TYPE:
		return hott_term_fragment_scan(
			terms, term->as.thunk_type.computation, depth + 1, p_reason
		);
	case PROTOTYPE_TERM_MATCH:
		{
			int status = hott_term_fragment_scan(
				terms, term->as.match.scrutinee, depth + 1, p_reason
			);
			if (status != 0) {
				return status;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id >= terms->case_count) {
					return -1;
				}
				status = hott_term_fragment_scan(
					terms, terms->cases[case_id].body, depth + 1, p_reason
				);
				if (status != 0) {
					return status;
				}
			}
			return 0;
		}
	case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
		return hott_term_fragment_scan(
			terms, term->as.induction_hypothesis.argument, depth + 1, p_reason
		);
	default:
		return 0;
	}
}

static uint32_t hott_term_head(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	while (terms && term_id < terms->term_count &&
		terms->terms[term_id].tag == PROTOTYPE_TERM_APP) {
		term_id = terms->terms[term_id].as.app.function;
	}
	return terms && term_id < terms->term_count ? term_id : PROTOTYPE_INVALID_ID;
}

static int hott_candidate_rule_for_type_view(
	const struct prototype_term_db* terms,
	uint32_t carrier,
	uint32_t left,
	uint32_t right
) {
	uint32_t left_head = hott_term_head(terms, left);
	uint32_t right_head = hott_term_head(terms, right);
	if (left_head == PROTOTYPE_INVALID_ID || right_head == PROTOTYPE_INVALID_ID ||
		terms->terms[left_head].tag != PROTOTYPE_TERM_CONSTRUCTOR ||
		terms->terms[right_head].tag != PROTOTYPE_TERM_CONSTRUCTOR ||
		terms->terms[left_head].as.constructor.owner != carrier ||
		terms->terms[right_head].as.constructor.owner != carrier) {
		return PROTOTYPE_HOTT_RULE_REL_MATCH_ACTION;
	}
	return terms->terms[left_head].as.constructor.constructor_id ==
		terms->terms[right_head].as.constructor.constructor_id ?
		PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR :
		PROTOTYPE_HOTT_RULE_REL_ADT_DISTINCT;
}

static int hott_plan_add_conversion(
	struct prototype_parametricity_relation_goal_db* goals,
	struct prototype_parametricity_candidate_db* candidates,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_parametricity_relation_goal* goal,
	uint32_t goal_id,
	uint32_t context_id,
	uint32_t carrier,
	uint32_t left,
	uint32_t right,
	int profile,
	uint64_t step_limit,
	uint64_t* p_steps
) {
	const struct prototype_context_db* contexts = kernel->contexts;
	struct prototype_term_db* terms = kernel->terms;
	struct prototype_type_declaration_db* type_declarations =
		kernel->type_declarations;
	struct prototype_kernel_conversion_goal request = {
		.id = (uint32_t)candidates->conversion_premise_count,
		.context_id = context_id,
		.carrier_classifier = carrier,
		.left_term = left,
		.right_term = right,
		.normalization_profile = profile,
		.step_limit = step_limit,
		.result = { .status = PROTOTYPE_TERM_CONVERSION_INVALID }
	};
	uint64_t revision = terms->normalization_graph_revision;
	if (prototype_judgement_kernel_conversion_goal_execute(
			contexts, terms, type_declarations, definitions, &request, 1
		) != 0) {
		return -1;
	}
	*p_steps += request.result.steps_used;
	if (request.result.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	uint32_t candidate_id;
	if (hott_candidate_add(
			candidates, goal_id, PROTOTYPE_HOTT_RULE_REL_CONVERT, &candidate_id
		) != 0 || hott_candidate_add_conversion(
			candidates, candidate_id,
			PROTOTYPE_HOTT_CHILD_CONVERT_ANCHOR_RELATION, 0,
			request, revision
		) != 0) {
		return -1;
	}
	uint32_t anchor_goal;
	if (prototype_parametricity_relation_goal_db_intern(
			goals, kernel, bridges, goal->category,
			goal->left_carrier_claim_id, goal->right_carrier_claim_id,
			goal->left_claim_id, goal->left_claim_id, goal->bridge_id, &anchor_goal
		) != 0 || hott_candidate_add_child(
			candidates, candidate_id, anchor_goal,
			PROTOTYPE_HOTT_CHILD_CONVERT_ANCHOR_RELATION, 0
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_candidate_add_exposure(
	struct prototype_parametricity_candidate_db* candidates,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t candidate_id,
	uint32_t context_id,
	uint32_t carrier,
	uint32_t source,
	uint32_t exposed,
	int role,
	uint32_t ordinal,
	int profile,
	uint64_t step_limit,
	uint64_t* p_steps
) {
	struct prototype_kernel_conversion_goal request = {
		.id = (uint32_t)candidates->conversion_premise_count,
		.context_id = context_id,
		.carrier_classifier = carrier,
		.left_term = source,
		.right_term = exposed,
		.normalization_profile = profile,
		.step_limit = step_limit,
		.result = { .status = PROTOTYPE_TERM_CONVERSION_INVALID }
	};
	uint64_t revision = terms->normalization_graph_revision;
	if (prototype_judgement_kernel_conversion_goal_execute(
			contexts, terms, type_declarations, definitions, &request, 1
		) != 0 || request.result.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	*p_steps += request.result.steps_used;
	return hott_candidate_add_conversion(
		candidates, candidate_id, role, ordinal, request, revision
	);
}

static int hott_find_unique_claim(
	const struct prototype_judgement_db* judgement,
	int kind,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_claim_id
) {
	uint32_t found = PROTOTYPE_INVALID_ID;
	if (!judgement || !p_claim_id) {
		return -1;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind != kind || prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id != context_id ||
			prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject != subject || prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier != classifier) {
			continue;
		}
		if (found != PROTOTYPE_INVALID_ID) {
			return 1;
		}
		found = i;
	}
	if (found == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_claim_id = found;
	return 0;
}

static int hott_find_unique_derivation_premise(
	const struct prototype_judgement_db* judgement,
	uint32_t conclusion_claim_id,
	int proof_kind,
	uint32_t expected_subject,
	uint32_t expected_classifier,
	uint32_t* p_claim_id
) {
	uint32_t found = PROTOTYPE_INVALID_ID;
	if (!judgement || !p_claim_id) {
		return -1;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id != conclusion_claim_id ||
			derivation->proof_kind != proof_kind) {
			continue;
		}
		for (uint32_t j = 0; j < derivation->premise_count; ++j) {
			uint32_t claim_id = derivation->premises[j].claim_id;
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(judgement, claim_id);
			if (!claim || prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject != expected_subject ||
				prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier != expected_classifier) {
				continue;
			}
			if (found != PROTOTYPE_INVALID_ID && found != claim_id) {
				return 1;
			}
			found = claim_id;
		}
	}
	if (found == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_claim_id = found;
	return 0;
}

static int hott_find_has_type_for_is_type_readonly_depth(
	const struct prototype_judgement_db* judgement,
	uint32_t is_type_claim_id,
	uint32_t remaining_depth,
	uint32_t* p_has_type_claim_id
) {
	const struct prototype_judgement_proposition* is_type = judgement ?
		prototype_judgement_claim_proposition(judgement, is_type_claim_id) : NULL;
	if (!judgement || !is_type || !p_has_type_claim_id ||
		remaining_depth == 0 || is_type->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) {
		return -1;
	}
	uint32_t direct_claim_id;
	if (hott_find_unique_derivation_premise(
			judgement,
			is_type_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE,
			is_type->subject,
			is_type->classifier,
			&direct_claim_id
		) == 0) {
		*p_has_type_claim_id = direct_claim_id;
		return 0;
	}
	const struct prototype_judgement_derivation* is_type_reindex = NULL;
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id != is_type_claim_id ||
			derivation->proof_kind !=
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX) {
			continue;
		}
		if (derivation->premise_count != 1) {
			return -1;
		}
		if (is_type_reindex && (is_type_reindex->semantic_action_id !=
				derivation->semantic_action_id ||
			is_type_reindex->premises[0].claim_id !=
				derivation->premises[0].claim_id)) {
			return 1;
		}
		is_type_reindex = derivation;
	}
	if (!is_type_reindex || is_type_reindex->premise_count != 1 ||
		is_type_reindex->semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		is_type_reindex->semantic_action_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t source_has_type_claim_id;
	int source_status = hott_find_has_type_for_is_type_readonly_depth(
		judgement,
		is_type_reindex->premises[0].claim_id,
		remaining_depth - 1,
		&source_has_type_claim_id
	);
	if (source_status != 0) {
		return source_status;
	}
	uint32_t found = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		const struct prototype_judgement_proposition* conclusion =
			prototype_judgement_claim_proposition(
				judgement, derivation->conclusion_claim_id
			);
		if (derivation->proof_kind !=
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX ||
			derivation->premise_count != 1 ||
			derivation->premises[0].claim_id != source_has_type_claim_id ||
			derivation->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			derivation->semantic_action_id !=
				is_type_reindex->semantic_action_id || !conclusion ||
			conclusion->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			conclusion->context_id != is_type->context_id ||
			conclusion->subject != is_type->subject ||
			conclusion->classifier != is_type->classifier) {
			continue;
		}
		if (found != PROTOTYPE_INVALID_ID && found !=
				derivation->conclusion_claim_id) {
			return 1;
		}
		found = derivation->conclusion_claim_id;
	}
	if (found == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_has_type_claim_id = found;
	return 0;
}

static const struct prototype_judgement_derivation*
hott_constructor_derivation_for_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
);

static int hott_plan_add_adt_field_goals(
	struct prototype_parametricity_relation_goal_db* goals,
	struct prototype_parametricity_candidate_db* candidates,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	const struct prototype_parametricity_relation_goal* goal,
	uint32_t candidate_id
) {
	const struct prototype_judgement_db* judgement = kernel->judgement;
	const struct prototype_context_db* contexts = kernel->contexts;
	const struct prototype_term_db* terms = kernel->terms;
	const struct prototype_judgement_claim* left_claim =
		prototype_judgement_claim_get(judgement, goal->left_claim_id);
	const struct prototype_judgement_claim* right_claim =
		prototype_judgement_claim_get(judgement, goal->right_claim_id);
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
	if (!left_claim || !right_claim ||
		prototype_term_constructor_spine_info(
			terms,
			prototype_judgement_proposition_get(
				judgement, left_claim->proposition_id
			)->subject,
			&left_head,
			&left_owner,
			&left_constructor,
			left_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&left_argument_count
		) != 0 || prototype_term_constructor_spine_info(
			terms,
			prototype_judgement_proposition_get(
				judgement, right_claim->proposition_id
			)->subject,
			&right_head,
			&right_owner,
			&right_constructor,
			right_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&right_argument_count
		) != 0 || left_owner != right_owner ||
		left_constructor != right_constructor ||
		left_argument_count != right_argument_count) {
		return -1;
	}
	if (left_argument_count == 0) {
		return 0;
	}
	const struct prototype_judgement_derivation* left_derivation =
		hott_constructor_derivation_for_claim(
			judgement, goal->left_claim_id
		);
	const struct prototype_judgement_derivation* right_derivation =
		hott_constructor_derivation_for_claim(
			judgement, goal->right_claim_id
		);
	if (!left_derivation || !right_derivation ||
		left_derivation->premise_count != left_argument_count ||
		right_derivation->premise_count != right_argument_count) {
		return 1;
	}
	uint32_t context_id = prototype_judgement_proposition_get(
		judgement, left_claim->proposition_id
	)->context_id;
	for (uint32_t i = 0; i < left_argument_count; ++i) {
		uint32_t left_argument_claim_id =
			left_derivation->premises[i].claim_id;
		uint32_t right_argument_claim_id =
			right_derivation->premises[i].claim_id;
		const struct prototype_judgement_claim* left_argument_claim =
			prototype_judgement_claim_get(
				judgement, left_argument_claim_id
			);
		const struct prototype_judgement_claim* right_argument_claim =
			prototype_judgement_claim_get(
				judgement, right_argument_claim_id
			);
		if (!left_argument_claim || !right_argument_claim ||
			prototype_judgement_proposition_get(
				judgement, left_argument_claim->proposition_id
			)->subject != left_arguments[i] ||
			prototype_judgement_proposition_get(
				judgement, right_argument_claim->proposition_id
			)->subject != right_arguments[i]) {
			return -1;
		}
		uint32_t left_classifier = prototype_judgement_proposition_get(
			judgement, left_argument_claim->proposition_id
		)->classifier;
		uint32_t right_classifier = prototype_judgement_proposition_get(
			judgement, right_argument_claim->proposition_id
		)->classifier;
		uint32_t left_carrier_claim_id;
		uint32_t right_carrier_claim_id;
		if (hott_find_unique_claim(
				judgement,
				PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
				context_id,
				left_classifier,
				prototype_judgement_claim_proposition(
					judgement, goal->left_carrier_claim_id
				)->classifier,
				&left_carrier_claim_id
			) != 0 || hott_find_unique_claim(
				judgement,
				PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
				context_id,
				right_classifier,
				prototype_judgement_claim_proposition(
					judgement, goal->right_carrier_claim_id
				)->classifier,
				&right_carrier_claim_id
			) != 0) {
			return 1;
		}
		uint32_t child_goal_id;
		if (prototype_parametricity_relation_goal_db_intern(
				goals,
				kernel,
				bridges,
				PROTOTYPE_HOTT_RELATION_VALUE,
				left_carrier_claim_id,
				right_carrier_claim_id,
				left_argument_claim_id,
				right_argument_claim_id,
				goal->bridge_id,
				&child_goal_id
			) != 0 || hott_candidate_add_child(
				candidates,
				candidate_id,
				child_goal_id,
				PROTOTYPE_HOTT_CHILD_ADT_FIELD,
				i
			) != 0) {
			return -1;
		}
	}
	(void)contexts;
	(void)left_head;
	(void)right_head;
	return 0;
}

int prototype_parametricity_relation_plan(
	struct prototype_parametricity_relation_goal_db* goals,
	struct prototype_parametricity_candidate_db* candidates,
	struct prototype_parametricity_work_db* work,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_work_item_id
) {
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	const struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	const struct prototype_parametricity_relation_goal* goal =
		prototype_parametricity_relation_goal_db_get(goals, goal_id);
	if (prototype_kernel_view_validate(kernel) != 0 || !goal || !candidates ||
		!work || !contexts || !substitutions ||
		!cwf_certificates || !cwf_certificates || !bridges || !terms ||
		!type_declarations || !operations || !judgement || !p_work_item_id ||
		!work->items) {
		return -1;
	}
	for (uint32_t i = 0; i < work->item_count; ++i) {
		const struct prototype_parametricity_work_item* existing = &work->items[i];
		if (existing->goal_id == goal_id &&
			existing->outcome.normalization_profile == normalization_profile &&
			existing->outcome.step_limit == step_limit &&
			existing->outcome.term_graph_revision ==
				terms->normalization_graph_revision &&
			strcmp(
				existing->outcome.calculus_fingerprint,
				PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
			) == 0) {
			*p_work_item_id = i;
			return 0;
		}
	}
	if (work->item_count >= work->item_capacity) {
		return -1;
	}
	const struct prototype_judgement_claim* carrier_claim =
		prototype_judgement_claim_get(
			judgement, goal->left_carrier_claim_id
		);
	const struct prototype_judgement_claim* right_carrier_claim =
		prototype_judgement_claim_get(
			judgement, goal->right_carrier_claim_id
		);
	const struct prototype_judgement_claim* left_claim =
		prototype_judgement_claim_get(judgement, goal->left_claim_id);
	const struct prototype_judgement_claim* right_claim =
		prototype_judgement_claim_get(judgement, goal->right_claim_id);
	if (!carrier_claim || !right_carrier_claim || !left_claim || !right_claim ||
		prototype_judgement_proposition_get(judgement, carrier_claim->proposition_id)->subject >= terms->term_count ||
		prototype_judgement_proposition_get(judgement, left_claim->proposition_id)->subject >= terms->term_count ||
		prototype_judgement_proposition_get(judgement, right_claim->proposition_id)->subject >= terms->term_count) {
		return -1;
	}
	uint32_t carrier = prototype_judgement_proposition_get(judgement, carrier_claim->proposition_id)->subject;
	uint32_t right_carrier = prototype_judgement_proposition_get(
		judgement, right_carrier_claim->proposition_id
	)->subject;
	uint32_t left = prototype_judgement_proposition_get(judgement, left_claim->proposition_id)->subject;
	uint32_t right = prototype_judgement_proposition_get(judgement, right_claim->proposition_id)->subject;
	struct prototype_parametricity_work_item item = {
		.id = (uint32_t)work->item_count,
		.goal_id = goal_id,
		.selected_candidate_id = PROTOTYPE_INVALID_ID,
		.source_ast = source_ast,
		.outcome = {
			.state = PROTOTYPE_HOTT_WORK_PENDING,
			.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
			.normalization_profile = normalization_profile,
			.step_limit = step_limit,
			.term_graph_revision = terms->normalization_graph_revision
		}
	};
	size_t goal_mark = goals->goal_count;
	size_t candidate_mark = candidates->candidate_count;
	size_t claim_premise_mark = candidates->claim_premise_count;
	size_t child_edge_mark = candidates->child_edge_count;
	size_t conversion_premise_mark = candidates->conversion_premise_count;
	size_t context_premise_mark = candidates->context_certificate_premise_count;
	size_t substitution_premise_mark =
		candidates->substitution_certificate_premise_count;
	memcpy(
		item.outcome.calculus_fingerprint,
		PROTOTYPE_HOTT_CALCULUS_FINGERPRINT,
		sizeof(item.outcome.calculus_fingerprint)
	);
	int reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	int status = hott_term_fragment_scan(terms, left, 0, &reason);
	if (status == 0) {
		status = hott_term_fragment_scan(terms, right, 0, &reason);
	}
	if (status < 0) {
		return -1;
	}
	if (status > 0) {
		item.outcome.state = reason == PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL ||
			reason == PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED ?
			PROTOTYPE_HOTT_WORK_RESIDUAL : PROTOTYPE_HOTT_WORK_UNSUPPORTED;
		item.outcome.residual_reason = reason;
		goto commit;
	}
	if (carrier != right_carrier) {
		item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
		item.outcome.residual_reason =
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
		goto commit;
	}
	const struct prototype_term* carrier_term = &terms->terms[carrier];
	if (carrier_term->tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		item.outcome.state = PROTOTYPE_HOTT_WORK_UNSUPPORTED;
		item.outcome.residual_reason = PROTOTYPE_HOTT_RESIDUAL_UNIVERSE;
		goto commit;
	}
	if (left == right) {
		uint32_t candidate_id;
		if (hott_candidate_add(
				candidates, goal_id, PROTOTYPE_HOTT_RULE_REL_DIAGONAL,
				&candidate_id
			) != 0) {
			return -1;
		}
	}
	if (left != right) {
		status = hott_plan_add_conversion(
			goals, candidates, kernel, bridges, definitions, goal, goal_id,
			prototype_judgement_proposition_get(judgement, left_claim->proposition_id)->context_id, carrier, left, right, normalization_profile,
			step_limit, &item.outcome.steps_used
		);
		if (status < 0) {
			return -1;
		}
	}
	if (carrier_term->tag == PROTOTYPE_TERM_TYPE_VIEW) {
		uint32_t type_id;
		const struct prototype_type_declaration* declaration;
		if (prototype_type_view_declaration_query(
				type_declarations, contexts, terms, carrier, &type_id, &declaration
			) != 0) {
			return -1;
		}
		(void)type_id;
		(void)declaration;
		uint32_t candidate_id;
		int candidate_rule = hott_candidate_rule_for_type_view(
			terms, carrier, left, right
		);
		if (hott_candidate_add(
				candidates, goal_id, candidate_rule,
				&candidate_id
			) != 0) {
			return -1;
		}
		if (candidate_rule == PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR) {
			int field_status = hott_plan_add_adt_field_goals(
				goals, candidates, kernel, bridges, goal, candidate_id
			);
			if (field_status < 0) {
				return -1;
			}
			if (field_status > 0) {
				item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
				item.outcome.residual_reason =
					PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
				goto commit;
			}
		}
	} else if (carrier_term->tag == PROTOTYPE_TERM_PI) {
		reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
		status = hott_term_fragment_scan(terms, carrier, 0, &reason);
		if (status < 0) {
			return -1;
		}
		if (status > 0) {
			item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.outcome.residual_reason = reason;
			goto commit;
		}
		uint32_t candidate_id;
		if (hott_candidate_add(
				candidates, goal_id, PROTOTYPE_HOTT_RULE_REL_PI_POINTWISE,
				&candidate_id
			) != 0) {
			return -1;
		}
	} else if (carrier_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		int purity = prototype_term_effect_row_purity(
			terms, carrier_term->as.computation_type.label
		);
		if (purity != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
			item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.outcome.residual_reason = purity == PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL ?
				PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL :
				PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED;
			goto commit;
		}
		struct prototype_term_normalization_result left_result;
		struct prototype_term_normalization_result right_result;
		if (prototype_term_normalize_with_profile(
				terms, type_declarations, definitions, normalization_profile,
				left, step_limit, &left_result
			) != 0 || prototype_term_normalize_with_profile(
				terms, type_declarations, definitions, normalization_profile,
				right, step_limit, &right_result
			) != 0) {
			return -1;
		}
		item.outcome.steps_used += left_result.steps_used + right_result.steps_used;
		if (left_result.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			right_result.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			left_result.term_id >= terms->term_count ||
			right_result.term_id >= terms->term_count ||
			terms->terms[left_result.term_id].tag != PROTOTYPE_TERM_RETURN ||
			terms->terms[right_result.term_id].tag != PROTOTYPE_TERM_RETURN) {
			item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.outcome.residual_reason =
				PROTOTYPE_HOTT_RESIDUAL_NEUTRAL_PURE_COMPUTATION;
			goto commit;
		}
		uint32_t candidate_id;
		if (hott_candidate_add(
				candidates, goal_id, PROTOTYPE_HOTT_RULE_REL_COMP_RETURN,
				&candidate_id
			) != 0 || hott_candidate_add_exposure(
				candidates, contexts, terms, type_declarations, definitions,
				candidate_id, prototype_judgement_proposition_get(judgement, left_claim->proposition_id)->context_id, carrier, left,
				left_result.term_id,
				PROTOTYPE_HOTT_CHILD_COMP_LEFT_RETURN_EXPOSURE, 0,
				normalization_profile, step_limit, &item.outcome.steps_used
			) != 0 || hott_candidate_add_exposure(
				candidates, contexts, terms, type_declarations, definitions,
				candidate_id, prototype_judgement_proposition_get(judgement, right_claim->proposition_id)->context_id, carrier, right,
				right_result.term_id,
				PROTOTYPE_HOTT_CHILD_COMP_RIGHT_RETURN_EXPOSURE, 0,
				normalization_profile, step_limit, &item.outcome.steps_used
			) != 0) {
			return -1;
		}
		uint32_t result_classifier = carrier_term->as.computation_type.result;
		uint32_t left_value =
			terms->terms[left_result.term_id].as.return_term.value;
		uint32_t right_value =
			terms->terms[right_result.term_id].as.return_term.value;
		uint32_t result_carrier_claim;
		uint32_t left_value_claim;
		uint32_t right_value_claim;
		int result_authority = hott_find_unique_claim(
			judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
			prototype_judgement_proposition_get(judgement, left_claim->proposition_id)->context_id, result_classifier, prototype_judgement_proposition_get(judgement, carrier_claim->proposition_id)->classifier,
			&result_carrier_claim
		);
		int left_authority = hott_find_unique_derivation_premise(
			judgement, goal->left_claim_id, PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			left_value, result_classifier, &left_value_claim
		);
		int right_authority = hott_find_unique_derivation_premise(
			judgement, goal->right_claim_id, PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			right_value, result_classifier, &right_value_claim
		);
		if (result_authority < 0 || left_authority < 0 || right_authority < 0) {
			return -1;
		}
		if (result_authority != 0 || left_authority != 0 || right_authority != 0) {
			item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.outcome.residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
			goto commit;
		}
		uint32_t result_goal;
		if (prototype_parametricity_relation_goal_db_intern(
				goals, kernel, bridges, PROTOTYPE_HOTT_RELATION_VALUE,
				result_carrier_claim, result_carrier_claim,
				left_value_claim, right_value_claim,
				goal->bridge_id, &result_goal
			) != 0 || hott_candidate_add_child(
				candidates, candidate_id, result_goal,
				PROTOTYPE_HOTT_CHILD_COMP_RESULT_RELATION, 0
			) != 0) {
			return -1;
		}
	} else if (carrier_term->tag == PROTOTYPE_TERM_THUNK_TYPE) {
		reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
		status = hott_term_fragment_scan(
			terms, carrier_term->as.thunk_type.computation, 0, &reason
		);
		if (status != 0) {
			if (status < 0) {
				return -1;
			}
			item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.outcome.residual_reason = reason;
			goto commit;
		}
		struct prototype_term_normalization_result left_result;
		struct prototype_term_normalization_result right_result;
		if (prototype_term_normalize_with_profile(
				terms, type_declarations, definitions, normalization_profile,
				left, step_limit, &left_result
			) != 0 || prototype_term_normalize_with_profile(
				terms, type_declarations, definitions, normalization_profile,
				right, step_limit, &right_result
			) != 0) {
			return -1;
		}
		item.outcome.steps_used += left_result.steps_used + right_result.steps_used;
		if (left_result.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			right_result.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			left_result.term_id >= terms->term_count ||
			right_result.term_id >= terms->term_count ||
			terms->terms[left_result.term_id].tag != PROTOTYPE_TERM_THUNK ||
			terms->terms[right_result.term_id].tag != PROTOTYPE_TERM_THUNK) {
			item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.outcome.residual_reason =
				PROTOTYPE_HOTT_RESIDUAL_NEUTRAL_PURE_COMPUTATION;
			goto commit;
		}
		uint32_t computation_carrier = carrier_term->as.thunk_type.computation;
		uint32_t left_computation =
			terms->terms[left_result.term_id].as.thunk.computation;
		uint32_t right_computation =
			terms->terms[right_result.term_id].as.thunk.computation;
		uint32_t computation_carrier_claim;
		uint32_t left_computation_claim;
		uint32_t right_computation_claim;
		int carrier_authority = hott_find_unique_claim(
			judgement, PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
			prototype_judgement_proposition_get(judgement, left_claim->proposition_id)->context_id, computation_carrier, prototype_judgement_proposition_get(judgement, carrier_claim->proposition_id)->classifier,
			&computation_carrier_claim
		);
		int left_authority = hott_find_unique_derivation_premise(
			judgement, goal->left_claim_id, PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			left_computation, computation_carrier, &left_computation_claim
		);
		int right_authority = hott_find_unique_derivation_premise(
			judgement, goal->right_claim_id, PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			right_computation, computation_carrier, &right_computation_claim
		);
		if (carrier_authority < 0 || left_authority < 0 || right_authority < 0) {
			return -1;
		}
		if (carrier_authority != 0 || left_authority != 0 || right_authority != 0) {
			item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.outcome.residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
			goto commit;
		}
		uint32_t candidate_id;
		if (hott_candidate_add(
				candidates, goal_id, PROTOTYPE_HOTT_RULE_REL_THUNK_PURE,
				&candidate_id
			) != 0) {
			return -1;
		}
		uint32_t computation_goal;
		if (prototype_parametricity_relation_goal_db_intern(
				goals, kernel, bridges, PROTOTYPE_HOTT_RELATION_COMPUTATION,
				computation_carrier_claim, computation_carrier_claim,
				left_computation_claim,
				right_computation_claim, goal->bridge_id, &computation_goal
			) != 0 || hott_candidate_add_child(
				candidates, candidate_id, computation_goal,
				PROTOTYPE_HOTT_CHILD_THUNK_COMPUTATION_RELATION, 0
			) != 0) {
			return -1;
		}
	}
	uint32_t selected_candidate = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < candidates->candidate_count; ++i) {
		if (candidates->candidates[i].conclusion_goal_id == goal_id) {
			selected_candidate = i;
			break;
		}
	}
	if (selected_candidate != PROTOTYPE_INVALID_ID) {
		item.outcome.state = PROTOTYPE_HOTT_WORK_READY;
		item.selected_candidate_id = selected_candidate;
	} else if (carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_TEXT ||
		carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_INT ||
		carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_INT64) {
		item.outcome.state = PROTOTYPE_HOTT_WORK_UNSUPPORTED;
		item.outcome.residual_reason = PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE;
	} else {
		item.outcome.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
		item.outcome.residual_reason = PROTOTYPE_HOTT_RESIDUAL_CONVERSION;
	}

commit:
	if (item.outcome.state != PROTOTYPE_HOTT_WORK_READY) {
		goals->goal_count = goal_mark;
		candidates->candidate_count = candidate_mark;
		candidates->claim_premise_count = claim_premise_mark;
		candidates->child_edge_count = child_edge_mark;
		candidates->conversion_premise_count = conversion_premise_mark;
		candidates->context_certificate_premise_count = context_premise_mark;
		candidates->substitution_certificate_premise_count =
			substitution_premise_mark;
		item.selected_candidate_id = PROTOTYPE_INVALID_ID;
	}
	work->items[work->item_count++] = item;
	*p_work_item_id = item.id;
	return 0;
}

void prototype_parametricity_residual_db_init(
	struct prototype_parametricity_residual_db* db,
	struct prototype_parametricity_residual_obligation* obligations,
	size_t obligation_capacity
) {
	if (!db) {
		return;
	}
	db->obligations = obligations;
	db->obligation_count = 0;
	db->obligation_capacity = obligation_capacity;
}

int prototype_parametricity_residual_db_add_from_work(
	struct prototype_parametricity_residual_db* db,
	const struct prototype_parametricity_work_db* work,
	uint32_t work_item_id,
	uint32_t* p_obligation_id
) {
	const struct prototype_parametricity_work_item* item = prototype_parametricity_work_db_get(
		work, work_item_id
	);
	if (!db || !p_obligation_id || !db->obligations || !item ||
		(item->outcome.state != PROTOTYPE_HOTT_WORK_RESIDUAL &&
		 item->outcome.state != PROTOTYPE_HOTT_WORK_UNSUPPORTED) ||
		db->obligation_count >= db->obligation_capacity) {
		return -1;
	}
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		if (db->obligations[i].work_item_id == work_item_id) {
			*p_obligation_id = i;
			return 0;
		}
	}
	uint32_t id = (uint32_t)db->obligation_count++;
	db->obligations[id] = (struct prototype_parametricity_residual_obligation){
		.obligation_id = id,
		.work_item_id = work_item_id
	};
	*p_obligation_id = id;
	return 0;
}

int prototype_parametricity_residual_db_validate(
	const struct prototype_parametricity_residual_db* db,
	const struct prototype_parametricity_work_db* work
) {
	if (!db || !work || db->obligation_count > db->obligation_capacity ||
		(db->obligation_count != 0 && !db->obligations)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_parametricity_residual_obligation* obligation =
			&db->obligations[i];
		const struct prototype_parametricity_work_item* item = prototype_parametricity_work_db_get(
			work, obligation->work_item_id
		);
		if (obligation->obligation_id != i || !item ||
			(item->outcome.state != PROTOTYPE_HOTT_WORK_RESIDUAL &&
			 item->outcome.state != PROTOTYPE_HOTT_WORK_UNSUPPORTED) ||
			strcmp(
				item->outcome.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
			) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_parametricity_residual_db_require_artifact_empty(
	const struct prototype_parametricity_residual_db* db
) {
	return db && db->obligation_count == 0 ? 0 : -1;
}

static uint64_t hott_action_hash_mix(uint64_t hash, uint32_t value) {
	hash ^= value;
	hash *= UINT64_C(1099511628211);
	return hash;
}

static uint64_t hott_action_request_hash(
	const struct prototype_hott_action_request* request
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = hott_action_hash_mix(hash, (uint32_t)request->kind);
	switch (request->kind) {
	case PROTOTYPE_HOTT_ACTION_CONTEXT:
		return hott_action_hash_mix(hash, request->key.context.source_context_id);
	case PROTOTYPE_HOTT_ACTION_SUBSTITUTION:
		hash = hott_action_hash_mix(
			hash, request->key.substitution.source_substitution_id
		);
		hash = hott_action_hash_mix(
			hash, request->key.substitution.source_bridge_id
		);
		return hott_action_hash_mix(
			hash, request->key.substitution.target_bridge_id
		);
	case PROTOTYPE_HOTT_ACTION_RELATION_TYPE:
		hash = hott_action_hash_mix(hash, request->key.relation_type.source_claim_id);
		return hott_action_hash_mix(hash, request->key.relation_type.source_bridge_id);
	case PROTOTYPE_HOTT_ACTION_TERM:
		hash = hott_action_hash_mix(hash, request->key.term.source_claim_id);
		hash = hott_action_hash_mix(hash, request->key.term.source_bridge_id);
		return hott_action_hash_mix(
			hash, request->key.term.relation_type_action_request_id
		);
	case PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION:
		hash = hott_action_hash_mix(
			hash, request->key.identity_type.source_claim_id
		);
		return hott_action_hash_mix(
			hash, request->key.identity_type.source_bridge_id
		);
	case PROTOTYPE_HOTT_ACTION_OBJECT_TERM:
		hash = hott_action_hash_mix(
			hash, request->key.object_term.source_claim_id
		);
		hash = hott_action_hash_mix(
			hash, request->key.object_term.source_bridge_id
		);
		return hott_action_hash_mix(
			hash, request->key.object_term.identity_type_action_request_id
		);
	default:
		return 0;
	}
}

static int hott_action_request_key_equal(
	const struct prototype_hott_action_request* left,
	const struct prototype_hott_action_request* right
) {
	if (!left || !right || left->kind != right->kind) {
		return 0;
	}
	switch (left->kind) {
	case PROTOTYPE_HOTT_ACTION_CONTEXT:
		return left->key.context.source_context_id ==
			right->key.context.source_context_id;
	case PROTOTYPE_HOTT_ACTION_SUBSTITUTION:
		return left->key.substitution.source_substitution_id ==
				right->key.substitution.source_substitution_id &&
			left->key.substitution.source_bridge_id ==
				right->key.substitution.source_bridge_id &&
			left->key.substitution.target_bridge_id ==
				right->key.substitution.target_bridge_id;
	case PROTOTYPE_HOTT_ACTION_RELATION_TYPE:
		return left->key.relation_type.source_claim_id ==
				right->key.relation_type.source_claim_id &&
			left->key.relation_type.source_bridge_id ==
				right->key.relation_type.source_bridge_id;
	case PROTOTYPE_HOTT_ACTION_TERM:
		return left->key.term.source_claim_id ==
				right->key.term.source_claim_id &&
			left->key.term.source_bridge_id ==
				right->key.term.source_bridge_id &&
			left->key.term.relation_type_action_request_id ==
				right->key.term.relation_type_action_request_id;
	case PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION:
		return left->key.identity_type.source_claim_id ==
				right->key.identity_type.source_claim_id &&
			left->key.identity_type.source_bridge_id ==
				right->key.identity_type.source_bridge_id;
	case PROTOTYPE_HOTT_ACTION_OBJECT_TERM:
		return left->key.object_term.source_claim_id ==
				right->key.object_term.source_claim_id &&
			left->key.object_term.source_bridge_id ==
				right->key.object_term.source_bridge_id &&
			left->key.object_term.identity_type_action_request_id ==
				right->key.object_term.identity_type_action_request_id;
	default:
		return 0;
	}
}
