#include "a_program/frontend/lowering.h"
#include "hott.h"
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

void prototype_hott_relation_goal_db_init(
	struct prototype_hott_relation_goal_db* db,
	struct prototype_hott_relation_goal* goals,
	size_t goal_capacity
) {
	if (!db) {
		return;
	}
	db->goals = goals;
	db->goal_count = 0;
	db->goal_capacity = goal_capacity;
}

const struct prototype_hott_relation_goal*
prototype_hott_relation_goal_db_get(
	const struct prototype_hott_relation_goal_db* db,
	uint32_t goal_id
) {
	return db && goal_id < db->goal_count ? &db->goals[goal_id] : NULL;
}

static int hott_relation_goal_is_valid(
	const struct prototype_hott_relation_goal* goal,
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

int prototype_hott_relation_goal_db_intern(
	struct prototype_hott_relation_goal_db* db,
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
	struct prototype_hott_relation_goal goal = {
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
		const struct prototype_hott_relation_goal* old = &db->goals[i];
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

int prototype_hott_relation_goal_db_validate(
	const struct prototype_hott_relation_goal_db* db,
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
			const struct prototype_hott_relation_goal* a = &db->goals[i];
			const struct prototype_hott_relation_goal* b = &db->goals[j];
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

const struct prototype_hott_candidate* prototype_hott_candidate_db_get(
	const struct prototype_hott_candidate_db* db,
	uint32_t candidate_id
) {
	return db && candidate_id < db->candidate_count ?
		&db->candidates[candidate_id] : NULL;
}

static int hott_candidate_add(
	struct prototype_hott_candidate_db* db,
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
	db->candidates[id] = (struct prototype_hott_candidate){
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
	struct prototype_hott_candidate_db* db,
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
	struct prototype_hott_candidate* candidate = &db->candidates[candidate_id];
	if (candidate->first_conversion_premise + candidate->conversion_premise_count !=
		db->conversion_premise_count) {
		return -1;
	}
	db->conversion_premises[db->conversion_premise_count++] =
		(struct prototype_hott_conversion_premise){
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
	struct prototype_hott_candidate_db* db,
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
	struct prototype_hott_candidate* candidate = &db->candidates[candidate_id];
	if (candidate->first_child_edge + candidate->child_edge_count !=
		db->child_edge_count) {
		return -1;
	}
	db->child_edges[db->child_edge_count++] = (struct prototype_hott_child_edge){
		.candidate_id = candidate_id,
		.child_goal_id = child_goal_id,
		.role = role,
		.ordinal = ordinal
	};
	++candidate->child_edge_count;
	return 0;
}

static int conversion_premise_is_valid(
	const struct prototype_hott_conversion_premise* premise,
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
	const struct prototype_hott_candidate_db* db,
	const struct prototype_hott_candidate* candidate
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
	const struct prototype_hott_candidate_db* db,
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
		const struct prototype_hott_candidate* candidate = &db->candidates[i];
		if (candidate->conclusion_goal_id != goal_id) {
			continue;
		}
		for (uint32_t j = 0; j < candidate->child_edge_count; ++j) {
			const struct prototype_hott_child_edge* edge =
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

int prototype_hott_candidate_db_validate(
	const struct prototype_hott_candidate_db* db,
	const struct prototype_hott_relation_goal_db* goals,
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
		const struct prototype_hott_candidate* candidate = &db->candidates[i];
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
			const struct prototype_hott_claim_premise* edge =
				&db->claim_premises[candidate->first_claim_premise + j];
			if (edge->candidate_id != i || !hott_role_is_valid(edge->role) ||
				!hott_rule_allows_role(candidate->rule, edge->role) ||
				!prototype_judgement_claim_get(judgement, edge->claim_id)) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < candidate->child_edge_count; ++j) {
			const struct prototype_hott_child_edge* edge =
				&db->child_edges[candidate->first_child_edge + j];
			if (edge->candidate_id != i || !hott_role_is_valid(edge->role) ||
				!hott_rule_allows_role(candidate->rule, edge->role) ||
				edge->child_goal_id >= goals->goal_count) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < candidate->conversion_premise_count; ++j) {
			const struct prototype_hott_conversion_premise* edge =
				&db->conversion_premises[candidate->first_conversion_premise + j];
			if (edge->candidate_id != i ||
				!hott_rule_allows_role(candidate->rule, edge->role) ||
				!conversion_premise_is_valid(edge, contexts, terms)) {
				return -1;
			}
		}
		for (uint32_t j = 0; j < candidate->context_certificate_premise_count; ++j) {
			const struct prototype_hott_context_certificate_premise* edge =
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
			const struct prototype_hott_substitution_certificate_premise* edge =
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

void prototype_hott_work_db_init(
	struct prototype_hott_work_db* db,
	struct prototype_hott_work_item* items,
	size_t item_capacity
) {
	if (!db) {
		return;
	}
	db->items = items;
	db->item_count = 0;
	db->item_capacity = item_capacity;
}

const struct prototype_hott_work_item* prototype_hott_work_db_get(
	const struct prototype_hott_work_db* db,
	uint32_t work_item_id
) {
	return db && work_item_id < db->item_count ? &db->items[work_item_id] : NULL;
}

int prototype_hott_work_db_validate(
	const struct prototype_hott_work_db* db,
	const struct prototype_hott_relation_goal_db* goals,
	const struct prototype_hott_candidate_db* candidates
) {
	if (!db || !goals || !candidates || db->item_count > db->item_capacity ||
		(db->item_count != 0 && !db->items)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->item_count; ++i) {
		const struct prototype_hott_work_item* item = &db->items[i];
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
	struct prototype_hott_relation_goal_db* goals,
	struct prototype_hott_candidate_db* candidates,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_hott_relation_goal* goal,
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
	if (prototype_hott_relation_goal_db_intern(
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
	struct prototype_hott_candidate_db* candidates,
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
	struct prototype_hott_relation_goal_db* goals,
	struct prototype_hott_candidate_db* candidates,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	const struct prototype_hott_relation_goal* goal,
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
		if (prototype_hott_relation_goal_db_intern(
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

int prototype_hott_relation_plan(
	struct prototype_hott_relation_goal_db* goals,
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
	const struct prototype_hott_relation_goal* goal =
		prototype_hott_relation_goal_db_get(goals, goal_id);
	if (prototype_kernel_view_validate(kernel) != 0 || !goal || !candidates ||
		!work || !contexts || !substitutions ||
		!cwf_certificates || !cwf_certificates || !bridges || !terms ||
		!type_declarations || !operations || !judgement || !p_work_item_id ||
		!work->items) {
		return -1;
	}
	for (uint32_t i = 0; i < work->item_count; ++i) {
		const struct prototype_hott_work_item* existing = &work->items[i];
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
	struct prototype_hott_work_item item = {
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
		if (prototype_hott_relation_goal_db_intern(
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
		if (prototype_hott_relation_goal_db_intern(
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

void prototype_hott_residual_db_init(
	struct prototype_hott_residual_db* db,
	struct prototype_hott_residual_obligation* obligations,
	size_t obligation_capacity
) {
	if (!db) {
		return;
	}
	db->obligations = obligations;
	db->obligation_count = 0;
	db->obligation_capacity = obligation_capacity;
}

int prototype_hott_residual_db_add_from_work(
	struct prototype_hott_residual_db* db,
	const struct prototype_hott_work_db* work,
	uint32_t work_item_id,
	uint32_t* p_obligation_id
) {
	const struct prototype_hott_work_item* item = prototype_hott_work_db_get(
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
	db->obligations[id] = (struct prototype_hott_residual_obligation){
		.obligation_id = id,
		.work_item_id = work_item_id
	};
	*p_obligation_id = id;
	return 0;
}

int prototype_hott_residual_db_validate(
	const struct prototype_hott_residual_db* db,
	const struct prototype_hott_work_db* work
) {
	if (!db || !work || db->obligation_count > db->obligation_capacity ||
		(db->obligation_count != 0 && !db->obligations)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->obligation_count; ++i) {
		const struct prototype_hott_residual_obligation* obligation =
			&db->obligations[i];
		const struct prototype_hott_work_item* item = prototype_hott_work_db_get(
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

int prototype_hott_residual_db_require_artifact_empty(
	const struct prototype_hott_residual_db* db
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

void prototype_hott_action_db_init(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action_request* requests,
	size_t request_capacity,
	struct prototype_hott_action_certificate* certificates,
	size_t certificate_capacity,
	struct prototype_hott_action_result* results,
	size_t result_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->requests = requests;
	db->request_capacity = request_capacity;
	db->certificates = certificates;
	db->certificate_capacity = certificate_capacity;
	db->results = results;
	db->result_capacity = result_capacity;
	for (size_t i = 0; i < PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT; ++i) {
		db->request_index_heads[i] = PROTOTYPE_INVALID_ID;
	}
}

const struct prototype_hott_action_request* prototype_hott_action_request_get(
	const struct prototype_hott_action_db* db,
	uint32_t request_id
) {
	return db && request_id < db->request_count ?
		&db->requests[request_id] : NULL;
}

const struct prototype_hott_action_result* prototype_hott_action_result_get(
	const struct prototype_hott_action_db* db,
	uint32_t result_id
) {
	return db && result_id < db->result_count ? &db->results[result_id] : NULL;
}

static int hott_term_has_compiler_relation_head(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!terms || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t head = term_id;
	for (uint32_t depth = 0; depth <= terms->term_count; ++depth) {
		const struct prototype_term* term = &terms->terms[head];
		if (term->tag != PROTOTYPE_TERM_APP) {
			return term->tag == PROTOTYPE_TERM_RELATION_TYPE_FORMER ||
				term->tag == PROTOTYPE_TERM_RELATION_WITNESS_FORMER;
		}
		if (term->as.app.function >= terms->term_count) {
			return -1;
		}
		head = term->as.app.function;
	}
	return -1;
}

int prototype_hott_register_identity_root(
	struct prototype_artifact_interface* interface,
	const struct prototype_hott_action_db* actions,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t witness_has_type_claim_id,
	uint32_t* p_root_id
) {
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	const struct prototype_hott_bridge* bridge = request && bridges &&
		request->kind == PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_hott_bridge_db_get(
			bridges, request->key.identity_type.source_bridge_id
		) : NULL;
	const struct prototype_judgement_proposition* source = request && judgement &&
		request->kind == PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_judgement_claim_proposition(
			judgement, request->key.identity_type.source_claim_id
		) : NULL;
	const struct prototype_hott_identity_type_computation_certificate* identity =
		certificate ? &certificate->data.identity_type : NULL;
	const struct prototype_judgement_proposition* family = identity && judgement ?
		prototype_judgement_claim_proposition(
			judgement, identity->identity_type_has_type_claim_id
		) : NULL;
	if (!interface || !actions || !kernel || !bridges || !p_root_id ||
		!result || !request || !certificate || !judgement || !contexts || !terms ||
		!bridge || !source || !identity || !family ||
		prototype_hott_action_db_validate(actions, kernel, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		request->kind != PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		certificate->request_id != request->id || result->request_id != request->id ||
		bridge->source_context_id != prototype_context_empty(contexts) ||
		source->context_id != bridge->source_context_id ||
		source->classifier >= terms->term_count ||
		terms->terms[source->classifier].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		family->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		family->context_id != identity->endpoint_context_id ||
		family->subject != identity->identity_type_term_id ||
		family->classifier != source->classifier ||
		(identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) ||
		hott_term_has_compiler_relation_head(
			terms, family->subject
		) != 0) {
		return -1;
	}
	if (witness_has_type_claim_id != PROTOTYPE_INVALID_ID) {
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, witness_has_type_claim_id
			);
		if (!witness || witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			witness->context_id != family->context_id ||
			witness->classifier != family->subject ||
			hott_term_has_compiler_relation_head(
				terms, witness->subject
			) != 0) {
			return -1;
		}
	}
	return prototype_artifact_interface_add_identity_root(
		interface,
		terms,
		kernel->type_declarations,
		kernel->contexts,
		judgement,
		request->key.identity_type.source_claim_id,
		identity->identity_type_has_type_claim_id,
		witness_has_type_claim_id,
		identity->computation_rule,
		p_root_id
	);
}

static int hott_action_request_is_valid(
	const struct prototype_hott_action_db* db,
	const struct prototype_hott_action_request* request,
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
	if (!db || !request || request->id != expected_id ||
		!hott_action_kind_is_valid(request->kind) ||
		request->key_hash != hott_action_request_hash(request)) {
		return 0;
	}
	switch (request->kind) {
	case PROTOTYPE_HOTT_ACTION_CONTEXT:
		return request->key.context.source_context_id < contexts->context_count &&
			hott_context_is_formed(
				contexts,
				cwf_certificates,
				request->key.context.source_context_id
			);
	case PROTOTYPE_HOTT_ACTION_SUBSTITUTION:
		{
			const struct prototype_substitution* substitution =
				prototype_substitution_get(
					substitutions,
					request->key.substitution.source_substitution_id
				);
			const struct prototype_hott_bridge* source_bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.substitution.source_bridge_id
				);
			const struct prototype_hott_bridge* target_bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.substitution.target_bridge_id
				);
			return substitution && source_bridge && target_bridge &&
				source_bridge->source_context_id ==
					substitution->source_context &&
				target_bridge->source_context_id ==
					substitution->target_context;
		}
	case PROTOTYPE_HOTT_ACTION_RELATION_TYPE:
		{
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(
					judgement, request->key.relation_type.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.relation_type.source_bridge_id
				);
			const struct prototype_hott_bridge_certificate* bridge_certificate =
				bridge ? &bridges->certificates[bridge->id] : NULL;
			return claim && bridge && bridge_certificate &&
				(bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY ||
				 bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION) &&
				bridge->source_context_id == prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id &&
				hott_is_type_claim_matches(
					terms,
					type_declarations,
					judgement,
					request->key.relation_type.source_claim_id,
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id,
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject
				);
		}
	case PROTOTYPE_HOTT_ACTION_TERM:
		{
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(
					judgement, request->key.term.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.term.source_bridge_id
				);
			const struct prototype_hott_action_request* type_request =
				request->key.term.relation_type_action_request_id < expected_id ?
				&db->requests[
					request->key.term.relation_type_action_request_id
				] : NULL;
			const struct prototype_judgement_claim* type_claim =
				type_request && type_request->kind == PROTOTYPE_HOTT_ACTION_RELATION_TYPE ?
				prototype_judgement_claim_get(
					judgement, type_request->key.relation_type.source_claim_id
				) : NULL;
			return claim &&
				prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				hott_operation_matches_claim(operations, judgement, claim) &&
				bridge &&
				bridge->source_context_id == prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id &&
				type_request &&
				type_request->key.relation_type.source_bridge_id ==
					request->key.term.source_bridge_id &&
				type_claim &&
				prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->context_id == prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id &&
				prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->subject == prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier;
		}
	case PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.identity_type.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.identity_type.source_bridge_id
				);
			const struct prototype_hott_bridge_certificate* bridge_certificate =
				bridge ? &bridges->certificates[bridge->id] : NULL;
			return source && bridge && bridge_certificate &&
				(bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY ||
				 bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) &&
				bridge->source_context_id ==
					prototype_judgement_claim_proposition(
						judgement,
						request->key.identity_type.source_claim_id
					)->context_id &&
				hott_is_type_claim_matches(
					terms,
					type_declarations,
					judgement,
					request->key.identity_type.source_claim_id,
					bridge->source_context_id,
					prototype_judgement_claim_proposition(
						judgement,
						request->key.identity_type.source_claim_id
					)->subject
				);
		}
	case PROTOTYPE_HOTT_ACTION_OBJECT_TERM:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.object_term.source_claim_id
				);
			const struct prototype_judgement_proposition* source_proposition =
				source ? prototype_judgement_proposition_get(
					judgement, source->proposition_id
				) : NULL;
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.object_term.source_bridge_id
				);
			const struct prototype_hott_bridge_certificate* bridge_certificate =
				bridge ? &bridges->certificates[bridge->id] : NULL;
			const struct prototype_hott_action_request* identity_request =
				request->key.object_term.identity_type_action_request_id < expected_id ?
				&db->requests[
					request->key.object_term.identity_type_action_request_id
				] : NULL;
			const struct prototype_judgement_proposition* identity_source =
				identity_request && identity_request->kind ==
					PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
				prototype_judgement_claim_proposition(
					judgement,
					identity_request->key.identity_type.source_claim_id
				) : NULL;
			return source_proposition && source_proposition->kind ==
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				hott_operation_matches_claim(operations, judgement, source) &&
				bridge && bridge_certificate && bridge_certificate->semantics ==
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY &&
				bridge->source_context_id == source_proposition->context_id &&
				identity_request && identity_source && identity_source->kind ==
					PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				identity_request->key.identity_type.source_bridge_id == bridge->id &&
				identity_source->context_id == source_proposition->context_id &&
				identity_source->subject == source_proposition->classifier;
		}
	default:
		return 0;
	}
}

int prototype_hott_action_request_intern(
	struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_hott_action_request request,
	uint32_t* p_request_id
) {
	if (db) {
		db->request_intern_requests++;
	}
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
	if (!db || !p_request_id || !db->requests ||
		db->request_count >= db->request_capacity ||
		prototype_kernel_view_validate(kernel) != 0) {
		return -1;
	}
	request.id = (uint32_t)db->request_count;
	request.key_hash = hott_action_request_hash(&request);
	request.hash_next = PROTOTYPE_INVALID_ID;
	if (!hott_action_request_is_valid(
			db,
			&request,
			request.id,
			contexts,
			substitutions,
			cwf_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement
		)) {
		return -1;
	}
	size_t bucket =
		request.key_hash % PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT;
	for (uint32_t i = db->request_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = db->requests[i].hash_next) {
		if (i >= db->request_count) {
			return -1;
		}
		if (db->requests[i].key_hash == request.key_hash &&
			hott_action_request_key_equal(&db->requests[i], &request)) {
			db->request_intern_hits++;
			*p_request_id = i;
			return 0;
		}
	}
	request.hash_next = db->request_index_heads[bucket];
	db->requests[db->request_count] = request;
	db->request_index_heads[bucket] = request.id;
	db->request_count++;
	*p_request_id = request.id;
	return 0;
}

static const struct prototype_judgement_derivation*
hott_certificate_derivation(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	int proof_kind
) {
	if (!judgement) {
		return NULL;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id == claim_id &&
			derivation->proof_kind == proof_kind) {
			return derivation;
		}
	}
	return NULL;
}

static int hott_claim_has_match_derivation(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	uint32_t remaining_depth
) {
	if (!judgement || remaining_depth == 0 || claim_id >= judgement->claim_count) {
		return 0;
	}
	if (hott_certificate_derivation(
			judgement, claim_id, PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM
		)) {
		return 1;
	}
	const struct prototype_judgement_derivation* conversion =
		hott_certificate_derivation(
			judgement, claim_id, PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
		);
	const struct prototype_judgement_derivation* reindex =
		hott_certificate_derivation(
			judgement,
			claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
		);
	const struct prototype_judgement_derivation* wrapper = conversion ?
		conversion : reindex;
	return wrapper && wrapper->premise_count == 1 &&
		hott_claim_has_match_derivation(
			judgement,
			wrapper->premises[0].claim_id,
			remaining_depth - 1
		);
}

static int hott_term_is_universe_field_projection(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t scrutinee,
	int projection
) {
	const struct prototype_term* term = term_id < terms->term_count ?
		&terms->terms[term_id] : NULL;
	if (!term || term->tag != PROTOTYPE_TERM_MATCH ||
		term->as.match.scrutinee != scrutinee || term->as.match.case_count != 1 ||
		term->as.match.first_case >= terms->case_count) {
		return 0;
	}
	const struct prototype_match_case* match_case =
		&terms->cases[term->as.match.first_case];
	return match_case->constructor_id == 0 && match_case->binder_count == 7 &&
		match_case->first_binder + (uint32_t)projection < terms->case_binder_count &&
		match_case->body < terms->term_count &&
		terms->terms[match_case->body].tag == PROTOTYPE_TERM_VAR &&
		terms->terms[match_case->body].as.var.binding_id ==
			terms->case_binders[
				match_case->first_binder + (uint32_t)projection
			].binding_id;
}

static int hott_universe_projection_claim_is_valid(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t projection_claim_id,
	uint32_t universe_witness_claim_id,
	uint32_t left_endpoint,
	uint32_t right_endpoint,
	int projection
) {
	const struct prototype_judgement_proposition* result =
		prototype_judgement_claim_proposition(judgement, projection_claim_id);
	const struct prototype_judgement_proposition* witness =
		prototype_judgement_claim_proposition(judgement, universe_witness_claim_id);
	if (!result || !witness || result->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return 0;
	}
	if (hott_term_is_universe_field_projection(
			terms, result->subject, witness->subject, projection
		)) {
		return hott_claim_has_match_derivation(
			judgement, projection_claim_id, 4
		);
	}

	uint32_t application_claim = projection_claim_id;
	for (uint32_t depth = 0; depth < 4; ++depth) {
		const struct prototype_judgement_derivation* conversion =
			hott_certificate_derivation(
				judgement,
				application_claim,
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			);
		const struct prototype_judgement_derivation* reindex =
			hott_certificate_derivation(
				judgement,
				application_claim,
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
			);
		const struct prototype_judgement_derivation* wrapper = conversion ?
			conversion : reindex;
		if (!wrapper) {
			break;
		}
		if (wrapper->premise_count != 1 ||
			(reindex && (reindex->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			 reindex->semantic_action_id == PROTOTYPE_INVALID_ID))) {
			return 0;
		}
		application_claim = wrapper->premises[0].claim_id;
	}
	uint32_t expected_arguments[3] = {
		left_endpoint, right_endpoint, witness->subject
	};
	uint32_t witness_source_claim = universe_witness_claim_id;
	for (uint32_t depth = 0; depth < 4; ++depth) {
		const struct prototype_judgement_derivation* conversion =
			hott_certificate_derivation(
				judgement,
				witness_source_claim,
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			);
		const struct prototype_judgement_derivation* reindex =
			hott_certificate_derivation(
				judgement,
				witness_source_claim,
				PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
			);
		const struct prototype_judgement_derivation* wrapper = conversion ?
			conversion : reindex;
		if (!wrapper) {
			break;
		}
		if (wrapper->premise_count != 1 ||
			(reindex && (reindex->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			 reindex->semantic_action_id == PROTOTYPE_INVALID_ID))) {
			return 0;
		}
		witness_source_claim = wrapper->premises[0].claim_id;
	}
	uint32_t lambda_claim = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 3; i > 0; --i) {
		const struct prototype_judgement_derivation* application =
			hott_certificate_derivation(
				judgement,
				application_claim,
				PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
			);
		if (!application || application->premise_count != 2) {
			return 0;
		}
		const struct prototype_judgement_proposition* argument =
			prototype_judgement_claim_proposition(
				judgement, application->premises[1].claim_id
			);
		int argument_matches = argument &&
			(argument->subject == expected_arguments[i - 1] ||
			 prototype_judgement_classifier_conversion(
				terms,
				type_declarations,
				argument->subject,
				expected_arguments[i - 1]
			 ).status == PROTOTYPE_TERM_CONVERSION_EQUAL);
		if (!argument_matches ||
			(i == 3 && application->premises[1].claim_id !=
				witness_source_claim)) {
			return 0;
		}
		application_claim = application->premises[0].claim_id;
		lambda_claim = application_claim;
	}

	uint32_t proof_binder_claim_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < 3; ++i) {
		const struct prototype_judgement_proposition* lambda =
			prototype_judgement_claim_proposition(judgement, lambda_claim);
		const struct prototype_judgement_derivation* introduction =
			hott_certificate_derivation(
				judgement,
				lambda_claim,
				PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO
			);
		if (!lambda || !introduction || introduction->premise_count != 2 ||
			lambda->subject >= terms->term_count ||
			terms->terms[lambda->subject].tag != PROTOTYPE_TERM_LAMBDA) {
			return 0;
		}
		if (i == 2) {
			proof_binder_claim_id = introduction->premises[0].claim_id;
		}
		lambda_claim = introduction->premises[1].claim_id;
	}
	const struct prototype_judgement_proposition* body =
		prototype_judgement_claim_proposition(judgement, lambda_claim);
	const struct prototype_judgement_proposition* proof_binder =
		prototype_judgement_claim_proposition(
			judgement, proof_binder_claim_id
		);
	if (!body || !proof_binder || proof_binder->subject >= terms->term_count ||
		terms->terms[proof_binder->subject].tag != PROTOTYPE_TERM_VAR ||
		!hott_term_is_universe_field_projection(
			terms, body->subject, proof_binder->subject, projection
		) || !hott_claim_has_match_derivation(judgement, lambda_claim, 4)) {
		return 0;
	}
	return 1;
}

static int hott_action_certificate_is_valid(
	const struct prototype_hott_action_db* db,
	const struct prototype_hott_action_certificate* certificate,
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
	(void)substitutions;
	(void)cwf_certificates;
	(void)operations;
	(void)type_declarations;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(db, certificate ?
			certificate->request_id : PROTOTYPE_INVALID_ID);
	if (!certificate || certificate->id != expected_id || !request ||
		certificate->kind != request->kind) {
		return 0;
	}
	switch (certificate->kind) {
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_CONTEXT_BRIDGE:
		{
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, certificate->data.context.result_bridge_id
				);
			return request->kind == PROTOTYPE_HOTT_ACTION_CONTEXT &&
				bridge &&
				bridge->source_context_id ==
					request->key.context.source_context_id &&
				prototype_context_get(contexts, bridge->bridge_context_id);
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.relation_type.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.relation_type.source_bridge_id
				);
			const struct prototype_hott_bridge_certificate* bridge_certificate =
				bridge ? &bridges->certificates[bridge->id] : NULL;
			const struct prototype_hott_relation_type_action_certificate* type =
				&certificate->data.relation_type;
			const struct prototype_context* right_context =
				prototype_context_get(contexts, type->endpoint_context_id);
			const struct prototype_context* left_context = right_context ?
				prototype_context_get(contexts, right_context->parent) : NULL;
			const struct prototype_judgement_claim* relation =
				prototype_judgement_claim_get(
					judgement, type->relation_is_type_claim_id
				);
			uint32_t left_type;
			uint32_t right_type;
			uint32_t left_endpoint;
			uint32_t right_endpoint;
			uint32_t outer_binding;
			uint32_t inner_family;
			uint32_t inner_binding;
			uint32_t relation_body;
			int relation_body_matches;
			if (request->kind != PROTOTYPE_HOTT_ACTION_RELATION_TYPE || !source ||
				!bridge || !bridge_certificate ||
				(bridge_certificate->semantics !=
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY &&
				 bridge_certificate->semantics !=
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION) ||
				!right_context || !left_context || !relation ||
				type->relation_family_semantics !=
					PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION ||
				type->left_context_certificate_id >=
					cwf_certificates->certificate_count ||
				type->right_context_certificate_id >=
					cwf_certificates->certificate_count ||
				cwf_certificates->certificates[
					type->left_context_certificate_id
				].structural_id != right_context->parent ||
				cwf_certificates->certificates[
					type->right_context_certificate_id
				].structural_id != type->endpoint_context_id ||
				left_context->binding_id != type->left_endpoint_binding_id ||
				right_context->binding_id != type->right_endpoint_binding_id ||
				prototype_term_pure_family_parts(
					terms,
					type->relation_family_term_id,
					&outer_binding,
					&inner_family
				) != 0 || prototype_term_pure_family_parts(
					terms,
					inner_family,
					&inner_binding,
					&relation_body
				) != 0 || prototype_term_core_shape_equal_under_binders(
					terms,
					(uint32_t[]) { outer_binding, inner_binding },
					(uint32_t[]) {
						type->left_endpoint_binding_id,
						type->right_endpoint_binding_id
					},
					2,
					relation_body,
					type->relation_type_term_id,
					&relation_body_matches
				) != 0 || !relation_body_matches ||
				prototype_term_relation_type_info(
					terms,
					type->relation_type_term_id,
					&left_type,
					&right_type,
					&left_endpoint,
					&right_endpoint
				) != 0) {
				return 0;
			}
			return bridge->source_context_id == prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id &&
				left_type == prototype_context_classifier_term(left_context) &&
				right_type == prototype_context_classifier_term(right_context) &&
				left_endpoint < terms->term_count &&
				terms->terms[left_endpoint].tag == PROTOTYPE_TERM_VAR &&
				terms->terms[left_endpoint].as.var.binding_id ==
					type->left_endpoint_binding_id &&
				right_endpoint < terms->term_count &&
				terms->terms[right_endpoint].tag == PROTOTYPE_TERM_VAR &&
				terms->terms[right_endpoint].as.var.binding_id ==
					type->right_endpoint_binding_id &&
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->context_id == type->endpoint_context_id &&
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject == type->relation_type_term_id &&
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier == prototype_judgement_proposition_get(judgement, source->proposition_id)->classifier;
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_SUBSTITUTION_NATURALITY:
		{
			const struct prototype_substitution* source =
				prototype_substitution_get(
					substitutions,
					request->key.substitution.source_substitution_id
				);
			const struct prototype_hott_bridge* source_bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.substitution.source_bridge_id
				);
			const struct prototype_hott_bridge* target_bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.substitution.target_bridge_id
				);
			const struct prototype_hott_substitution_action_certificate* action =
				&certificate->data.substitution;
			const struct prototype_substitution* result =
				prototype_substitution_get(
					substitutions, action->result_substitution_id
				);
			struct prototype_term_conversion_result left;
			struct prototype_term_conversion_result right;
			if (request->kind != PROTOTYPE_HOTT_ACTION_SUBSTITUTION || !source ||
				!source_bridge || !target_bridge || !result ||
				result->source_context != source_bridge->bridge_context_id ||
				result->target_context != target_bridge->bridge_context_id ||
				action->normalization_profile !=
					PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF ||
				action->term_graph_revision > terms->term_count ||
				prototype_substitution_compare_pointwise(
					(struct prototype_substitution_db*)substitutions,
					contexts,
					terms,
					type_declarations,
					action->left_naturality_lhs_substitution_id,
					action->left_naturality_rhs_substitution_id,
					action->normalization_profile,
					action->step_limit,
					&left
				) != 0 || left.status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
				prototype_substitution_compare_pointwise(
					(struct prototype_substitution_db*)substitutions,
					contexts,
					terms,
					type_declarations,
					action->right_naturality_lhs_substitution_id,
					action->right_naturality_rhs_substitution_id,
					action->normalization_profile,
					action->step_limit,
					&right
				) != 0 || right.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return 0;
			}
			if (result->kind == PROTOTYPE_SUBSTITUTION_EXTEND) {
				const struct prototype_cwf_certificate* extension =
					prototype_cwf_certificate_db_get_kind(
						cwf_certificates,
						action->result_substitution_certificate_id,
						PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
					);
				return extension && extension->structural_id ==
					action->result_substitution_id;
			}
			return action->result_substitution_certificate_id ==
				PROTOTYPE_INVALID_ID;
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION:
		{
			const struct prototype_hott_identity_type_computation_certificate*
				identity = &certificate->data.identity_type;
			const struct prototype_judgement_proposition* source =
				prototype_judgement_claim_proposition(
					judgement, request->key.identity_type.source_claim_id
				);
				const struct prototype_hott_bridge* bridge =
					prototype_hott_bridge_db_get(
						bridges, request->key.identity_type.source_bridge_id
					);
				const struct prototype_hott_bridge_certificate* bridge_certificate =
					bridge ? &bridges->certificates[bridge->id] : NULL;
				const struct prototype_judgement_proposition* identity_is_type =
					prototype_judgement_claim_proposition(
						judgement, identity->identity_type_is_type_claim_id
					);
				if (request->kind !=
					PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
					!source || source->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
					!bridge || !bridge_certificate ||
					(bridge_certificate->semantics !=
						PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY &&
					 bridge_certificate->semantics !=
						PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) ||
					bridge->source_context_id != source->context_id ||
					!identity_is_type || identity_is_type->kind !=
						PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
					identity_is_type->context_id != identity->endpoint_context_id ||
					identity_is_type->subject != identity->identity_type_term_id ||
					identity_is_type->classifier != source->classifier) {
					return 0;
				}
				const struct prototype_context* endpoint_context =
					prototype_context_get(contexts, identity->endpoint_context_id);
				const struct prototype_cwf_certificate* left_context_certificate =
					prototype_cwf_certificate_db_get_kind(
						cwf_certificates,
						identity->left_context_certificate_id,
						PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION
					);
				const struct prototype_cwf_certificate* right_context_certificate =
					prototype_cwf_certificate_db_get_kind(
						cwf_certificates,
						identity->right_context_certificate_id,
						PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION
					);
					if (!endpoint_context || !left_context_certificate ||
					!right_context_certificate ||
					left_context_certificate->structural_id != endpoint_context->parent ||
					right_context_certificate->structural_id !=
						identity->endpoint_context_id) {
						return 0;
					}
					if (identity->computation_rule ==
						PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER) {
						const struct prototype_context* source_context =
							prototype_context_get(contexts, source->context_id);
						uint32_t source_has_type_claim_id;
						int source_premise_status =
							hott_find_has_type_for_is_type_readonly_depth(
								judgement,
								request->key.identity_type.source_claim_id,
								(uint32_t)judgement->claim_count + 1,
								&source_has_type_claim_id
							);
						const struct prototype_hott_action_request* object_request = NULL;
						const struct prototype_hott_action_certificate*
							object_certificate = NULL;
						const struct prototype_hott_action_certificate* universe_action = NULL;
						for (uint32_t i = 0; source_premise_status == 0 &&
								i < db->result_count; ++i) {
							const struct prototype_hott_action_result* result = &db->results[i];
							const struct prototype_hott_action_request* candidate =
								prototype_hott_action_request_get(db, result->request_id);
							if (!candidate || candidate->kind !=
									PROTOTYPE_HOTT_ACTION_OBJECT_TERM ||
								candidate->key.object_term.source_claim_id !=
									source_has_type_claim_id ||
								candidate->key.object_term.source_bridge_id != bridge->id ||
								result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
								result->certificate_id >= db->certificate_count ||
								db->certificates[result->certificate_id].kind !=
									PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM) {
								continue;
							}
							object_request = candidate;
							object_certificate =
								&db->certificates[result->certificate_id];
							uint32_t universe_result_id;
							if (hott_action_result_for_request(
									db,
									candidate->key.object_term.
										identity_type_action_request_id,
									&universe_result_id
								) == 0) {
								const struct prototype_hott_action_result* universe_result =
									prototype_hott_action_result_get(
										db, universe_result_id
									);
								universe_action = universe_result &&
									universe_result->certificate_id < db->certificate_count ?
									&db->certificates[universe_result->certificate_id] : NULL;
							}
							break;
						}
						const struct prototype_judgement_proposition* universe_witness =
							object_certificate ?
							prototype_judgement_claim_proposition(
								judgement,
								object_certificate->data.object_term.
									witness_has_type_claim_id
							) : NULL;
						const struct prototype_judgement_proposition* relation_claim =
							prototype_judgement_claim_proposition(
								judgement,
								identity->backing_type_former_has_type_claim_id
							);
						const struct prototype_judgement_proposition* identity_claim =
							prototype_judgement_claim_proposition(
								judgement, identity->identity_type_has_type_claim_id
							);
						const struct prototype_term* relation =
							identity->backing_type_former_term_id < terms->term_count ?
							&terms->terms[identity->backing_type_former_term_id] : NULL;
						const struct prototype_term* identity_term =
							identity->identity_type_term_id < terms->term_count ?
							&terms->terms[identity->identity_type_term_id] : NULL;
						uint32_t endpoint_path[2];
						uint32_t endpoint_count;
						uint32_t expected_left_type;
						uint32_t expected_right_type;
						if (source_premise_status != 0 || !source_context ||
							!object_request || !object_certificate ||
							!universe_action || !universe_witness ||
							!relation_claim || !identity_claim || !relation ||
							!identity_term || source->subject >= terms->term_count ||
							source->classifier >=
								terms->term_count || terms->terms[source->classifier].tag !=
								PROTOTYPE_TERM_UNIVERSE_VAR ||
							universe_action->kind !=
								PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
							(universe_action->data.identity_type.computation_rule !=
								PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE &&
							 universe_action->data.identity_type.computation_rule !=
								PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) ||
							identity->generated_type_declaration_id != PROTOTYPE_INVALID_ID ||
							identity->pointwise_left_input_binding_id != PROTOTYPE_INVALID_ID ||
							identity->pointwise_right_input_binding_id != PROTOTYPE_INVALID_ID ||
							identity->pointwise_input_identity_binding_id != PROTOTYPE_INVALID_ID ||
							prototype_context_extension_path(
								contexts,
								bridge->bridge_context_id,
								identity->endpoint_context_id,
								endpoint_path,
								2,
								&endpoint_count
							) != 0 || endpoint_count != 2 ||
							prototype_term_reindex(
								terms,
								type_declarations,
								contexts,
								(struct prototype_substitution_db*)substitutions,
								source->subject,
								bridge->left_substitution_id,
								&expected_left_type
							) != 0 || prototype_term_reindex(
								terms,
								type_declarations,
								contexts,
								(struct prototype_substitution_db*)substitutions,
								source->subject,
								bridge->right_substitution_id,
								&expected_right_type
							) != 0 || prototype_context_classifier_term(
								prototype_context_get(contexts, endpoint_path[0])
							) != expected_left_type || prototype_context_classifier_term(
								prototype_context_get(contexts, endpoint_path[1])
							) != expected_right_type || identity->left_endpoint_binding_id !=
								prototype_context_get(contexts, endpoint_path[0])->binding_id ||
							identity->right_endpoint_binding_id !=
								prototype_context_get(contexts, endpoint_path[1])->binding_id ||
							relation_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
							relation_claim->context_id != bridge->bridge_context_id ||
							relation_claim->subject != identity->backing_type_former_term_id ||
							identity_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
							identity_claim->context_id != identity->endpoint_context_id ||
							identity_claim->subject != identity->identity_type_term_id ||
							identity_claim->classifier != source->classifier ||
							identity_term->tag != PROTOTYPE_TERM_APP) {
							return 0;
						}
						const struct prototype_term* relation_at_left =
							identity_term->as.app.function < terms->term_count ? &terms->terms[
								identity_term->as.app.function
							] : NULL;
						if (!hott_universe_projection_claim_is_valid(
								judgement,
								terms,
								type_declarations,
								identity->backing_type_former_has_type_claim_id,
								object_certificate->data.object_term.
									witness_has_type_claim_id,
								expected_left_type,
								expected_right_type,
								PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION
							) || !relation_at_left || relation_at_left->tag !=
								PROTOTYPE_TERM_APP || relation_at_left->as.app.function !=
								identity->backing_type_former_term_id ||
							identity_term->as.app.argument >= terms->term_count ||
							terms->terms[identity_term->as.app.argument].tag !=
								PROTOTYPE_TERM_VAR || terms->terms[
									identity_term->as.app.argument
								].as.var.binding_id != identity->right_endpoint_binding_id ||
							relation_at_left->as.app.argument >= terms->term_count ||
							terms->terms[relation_at_left->as.app.argument].tag !=
								PROTOTYPE_TERM_VAR || terms->terms[
									relation_at_left->as.app.argument
								].as.var.binding_id != identity->left_endpoint_binding_id) {
							return 0;
						}
						return 1;
					}
					if (identity->computation_rule ==
							PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT ||
						(identity->computation_rule ==
							PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE &&
						 source->context_id != prototype_context_empty(contexts) &&
						 source->subject < terms->term_count &&
						 terms->terms[source->subject].tag ==
							PROTOTYPE_TERM_UNIVERSE_VAR)) {
					const struct prototype_type_declaration* generated =
						identity->generated_type_declaration_id <
							type_declarations->type_count ?
						&type_declarations->type_declarations[
							identity->generated_type_declaration_id
						] : NULL;
					const struct prototype_judgement_proposition* former_claim =
						prototype_judgement_claim_proposition(
							judgement,
							identity->backing_type_former_has_type_claim_id
						);
					const struct prototype_judgement_proposition* family_claim =
						prototype_judgement_claim_proposition(
							judgement, identity->identity_type_has_type_claim_id
						);
					uint32_t endpoint_path[2];
					uint32_t endpoint_count;
					uint32_t former_type_id;
					uint32_t former_arguments[1];
					uint32_t former_argument_count;
					uint32_t identity_type_id;
					uint32_t identity_arguments[2];
					uint32_t identity_argument_count;
					uint32_t left_endpoint;
					uint32_t right_endpoint;
					if (source->context_id == prototype_context_empty(contexts) ||
						bridges->certificates[bridge->id].semantics !=
							PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
						!hott_term_is_constant_over_context(
							terms, contexts, source->context_id, source->subject
						) || prototype_context_extension_path(
							contexts,
							bridge->bridge_context_id,
							identity->endpoint_context_id,
							endpoint_path,
							2,
							&endpoint_count
						) != 0 || endpoint_count != 2 ||
						prototype_context_classifier_term(
							prototype_context_get(contexts, endpoint_path[0])
						) != source->subject || prototype_context_classifier_term(
							prototype_context_get(contexts, endpoint_path[1])
						) != source->subject || identity->left_endpoint_binding_id !=
							prototype_context_get(contexts, endpoint_path[0])->binding_id ||
						identity->right_endpoint_binding_id !=
							prototype_context_get(contexts, endpoint_path[1])->binding_id ||
						prototype_term_var(
							terms,
							identity->left_endpoint_binding_id,
							&left_endpoint
						) != 0 || prototype_term_var(
							terms,
							identity->right_endpoint_binding_id,
							&right_endpoint
						) != 0 || !family_claim || family_claim->kind !=
							PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						family_claim->context_id != identity->endpoint_context_id ||
						family_claim->subject != identity->identity_type_term_id ||
						family_claim->classifier != source->classifier) {
						return 0;
					}
					if (!generated) {
						uint32_t expected_identity;
						uint32_t computation = source->subject < terms->term_count &&
							terms->terms[source->subject].tag ==
								PROTOTYPE_TERM_THUNK_TYPE ?
							terms->terms[source->subject].as.thunk_type.computation :
							PROTOTYPE_INVALID_ID;
						if (computation >= terms->term_count ||
							terms->terms[computation].tag != PROTOTYPE_TERM_PI ||
							identity->generated_type_declaration_id !=
								PROTOTYPE_INVALID_ID ||
							identity->backing_type_former_term_id !=
								PROTOTYPE_INVALID_ID ||
							identity->backing_type_former_has_type_claim_id !=
								PROTOTYPE_INVALID_ID ||
							hott_build_nondependent_pi_identity_type(
								(struct prototype_term_db*)terms,
								(struct prototype_type_declaration_db*)type_declarations,
								prototype_context_empty(contexts),
								source->subject,
								identity->left_endpoint_binding_id,
								identity->right_endpoint_binding_id,
								identity->pointwise_left_input_binding_id,
								identity->pointwise_right_input_binding_id,
								identity->pointwise_input_identity_binding_id,
								&expected_identity
							) != 0 || expected_identity !=
								identity->identity_type_term_id) {
							return 0;
						}
						return 1;
					}
					if (generated->origin_kind !=
							PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
						generated->origin_source_carrier_term_id != source->subject ||
						prototype_term_type_instance_info(
							terms,
							identity->backing_type_former_term_id,
							&former_type_id,
							former_arguments,
							&former_argument_count
						) != 0 || former_type_id !=
							identity->generated_type_declaration_id ||
						former_argument_count != 0 || !former_claim ||
						former_claim->context_id != bridge->bridge_context_id ||
						former_claim->subject != identity->backing_type_former_term_id ||
						prototype_term_type_instance_info(
							terms,
							identity->identity_type_term_id,
							&identity_type_id,
							identity_arguments,
							&identity_argument_count
						) != 0 || identity_type_id !=
							identity->generated_type_declaration_id ||
						identity_argument_count != 2 ||
						identity_arguments[0] != left_endpoint ||
						identity_arguments[1] != right_endpoint ||
						!prototype_type_declaration_validate_generated_identity(
							terms,
							type_declarations,
							contexts,
							source->subject,
							identity->generated_type_declaration_id,
							prototype_type_declaration_generated_identity_rule_for_source(
								terms,
								source->subject
							)
						)) {
						return 0;
					}
					return 1;
				}
				if (identity->computation_rule ==
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT) {
					const struct prototype_type_declaration* generated =
						identity->generated_type_declaration_id <
							type_declarations->type_count ?
						&type_declarations->type_declarations[
							identity->generated_type_declaration_id
						] : NULL;
					uint32_t source_type_id;
					uint32_t source_arguments[2];
					uint32_t source_argument_count;
					uint32_t origin_type_id;
					uint32_t origin_arguments[1];
					uint32_t origin_argument_count;
					uint32_t identity_type_id;
					uint32_t boundary[8];
					uint32_t boundary_count;
					uint32_t endpoint_path[2];
					uint32_t endpoint_count;
					uint32_t expected[8];
					const struct prototype_hott_bridge* parent =
						prototype_hott_bridge_db_get(
							bridges, bridge_certificate->parent_bridge_id
						);
					const struct prototype_judgement_proposition* former_claim =
						prototype_judgement_claim_proposition(
							judgement,
							identity->backing_type_former_has_type_claim_id
						);
					const struct prototype_judgement_proposition* family_claim =
						prototype_judgement_claim_proposition(
							judgement, identity->identity_type_has_type_claim_id
						);
					if (source->context_id == prototype_context_empty(contexts) ||
						bridge_certificate->semantics !=
							PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
						!parent || !generated || generated->origin_kind !=
							PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
						generated->parameter_context !=
							prototype_context_empty(contexts) ||
						generated->index_count != 8 ||
						generated->constructor_count == 0 ||
						prototype_term_type_instance_info(
							terms,
							source->subject,
							&source_type_id,
							source_arguments,
							&source_argument_count
						) != 0 || source_argument_count != 2 || source_type_id >=
							type_declarations->type_count || type_declarations->
							type_declarations[source_type_id].origin_kind !=
								PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
						prototype_term_type_instance_info(
							terms,
							generated->origin_source_carrier_term_id,
							&origin_type_id,
							origin_arguments,
							&origin_argument_count
						) != 0 || origin_type_id != source_type_id ||
						origin_argument_count != 0 ||
						prototype_context_extension_path(
							contexts,
							bridge->bridge_context_id,
							identity->endpoint_context_id,
							endpoint_path,
							2,
							&endpoint_count
						) != 0 || endpoint_count != 2 ||
						prototype_term_type_instance_info(
							terms,
							identity->identity_type_term_id,
							&identity_type_id,
							boundary,
							&boundary_count
						) != 0 || identity_type_id !=
							identity->generated_type_declaration_id ||
						boundary_count != 8 || !former_claim || !family_claim ||
						former_claim->context_id != bridge->bridge_context_id ||
						former_claim->subject !=
							identity->backing_type_former_term_id ||
						family_claim->context_id != identity->endpoint_context_id ||
						family_claim->subject != identity->identity_type_term_id ||
						prototype_term_reindex(
							(struct prototype_term_db*)terms,
							type_declarations,
							contexts,
							(struct prototype_substitution_db*)substitutions,
							source_arguments[0],
							bridge->left_substitution_id,
							&expected[0]
						) != 0 || prototype_term_reindex(
							(struct prototype_term_db*)terms,
							type_declarations,
							contexts,
							(struct prototype_substitution_db*)substitutions,
							source_arguments[1],
							bridge->left_substitution_id,
							&expected[1]
						) != 0 || prototype_term_var(
							(struct prototype_term_db*)terms,
							identity->left_endpoint_binding_id,
							&expected[2]
						) != 0 || prototype_term_reindex(
							(struct prototype_term_db*)terms,
							type_declarations,
							contexts,
							(struct prototype_substitution_db*)substitutions,
							source_arguments[0],
							bridge->right_substitution_id,
							&expected[3]
						) != 0 || prototype_term_reindex(
							(struct prototype_term_db*)terms,
							type_declarations,
							contexts,
							(struct prototype_substitution_db*)substitutions,
							source_arguments[1],
							bridge->right_substitution_id,
							&expected[4]
						) != 0 || prototype_term_var(
							(struct prototype_term_db*)terms,
							identity->right_endpoint_binding_id,
							&expected[5]
						) != 0 || prototype_term_var(
							(struct prototype_term_db*)terms,
							prototype_context_get(
								contexts, parent->bridge_context_id
							)->binding_id,
							&expected[6]
						) != 0 || prototype_term_var(
							(struct prototype_term_db*)terms,
							prototype_context_get(
								contexts, bridge->bridge_context_id
							)->binding_id,
							&expected[7]
						) != 0 || memcmp(boundary, expected, sizeof(boundary)) != 0) {
						return 0;
					}
					return 1;
				}
				if (identity->computation_rule ==
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
				uint32_t endpoint_path[2];
				uint32_t endpoint_count;
				const struct prototype_judgement_proposition* identity_claim =
					prototype_judgement_claim_proposition(
						judgement, identity->identity_type_has_type_claim_id
					);
				uint32_t expected_identity;
				int source_is_computation_pi = source->subject < terms->term_count &&
					terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK_TYPE &&
					terms->terms[source->subject].as.thunk_type.computation <
						terms->term_count && terms->terms[
						terms->terms[source->subject].as.thunk_type.computation
					].tag == PROTOTYPE_TERM_PI;
				int source_is_type_pi = source->subject < terms->term_count &&
					terms->terms[source->subject].tag == PROTOTYPE_TERM_PI;
				if (identity->generated_type_declaration_id != PROTOTYPE_INVALID_ID ||
					identity->backing_type_former_term_id != PROTOTYPE_INVALID_ID ||
					identity->backing_type_former_has_type_claim_id !=
						PROTOTYPE_INVALID_ID ||
					(!source_is_computation_pi && !source_is_type_pi) ||
					prototype_context_extension_path(
						contexts,
						source->context_id,
						identity->endpoint_context_id,
						endpoint_path,
						2,
						&endpoint_count
					) != 0 || endpoint_count != 2 ||
					prototype_context_classifier_term(
						prototype_context_get(contexts, endpoint_path[0])
					) != source->subject || prototype_context_classifier_term(
						prototype_context_get(contexts, endpoint_path[1])
					) != source->subject || identity->left_endpoint_binding_id !=
						prototype_context_get(contexts, endpoint_path[0])->binding_id ||
					identity->right_endpoint_binding_id !=
						prototype_context_get(contexts, endpoint_path[1])->binding_id ||
					hott_build_nondependent_pi_identity_type(
						(struct prototype_term_db*)terms,
						(struct prototype_type_declaration_db*)type_declarations,
						source->context_id,
						source->subject,
						identity->left_endpoint_binding_id,
						identity->right_endpoint_binding_id,
						identity->pointwise_left_input_binding_id,
						identity->pointwise_right_input_binding_id,
						identity->pointwise_input_identity_binding_id,
						&expected_identity
					) != 0 || expected_identity != identity->identity_type_term_id ||
					!identity_claim || identity_claim->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					identity_claim->context_id != identity->endpoint_context_id ||
					identity_claim->subject != identity->identity_type_term_id ||
					identity_claim->classifier != source->classifier) {
					return 0;
				}
				for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
					if (judgement->derivations[i].conclusion_claim_id ==
							identity->identity_type_has_type_claim_id &&
						judgement->derivations[i].proof_kind ==
							(source_is_computation_pi ?
							 PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION :
							 PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO)) {
						return 1;
					}
				}
				return 0;
			}
			const struct prototype_type_declaration* generated =
				identity->generated_type_declaration_id <
					type_declarations->type_count ?
				&type_declarations->type_declarations[
					identity->generated_type_declaration_id
				] : NULL;
			const struct prototype_judgement_proposition* former_claim =
				prototype_judgement_claim_proposition(
					judgement, identity->backing_type_former_has_type_claim_id
				);
			const struct prototype_judgement_proposition* identity_type_claim =
				prototype_judgement_claim_proposition(
					judgement, identity->identity_type_has_type_claim_id
				);
			uint32_t former_type_id;
			uint32_t former_args[16];
			uint32_t former_arg_count;
			uint32_t identity_type_id;
			uint32_t identity_type_args[16];
			uint32_t identity_type_arg_count;
			uint32_t index_path[2];
			uint32_t index_count;
			if ((identity->computation_rule !=
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT &&
				identity->computation_rule !=
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN &&
				identity->computation_rule !=
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) ||
				identity->pointwise_left_input_binding_id != PROTOTYPE_INVALID_ID ||
				identity->pointwise_right_input_binding_id != PROTOTYPE_INVALID_ID ||
				identity->pointwise_input_identity_binding_id != PROTOTYPE_INVALID_ID ||
				!generated || generated->origin_kind !=
					PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
				generated->origin_source_carrier_term_id != source->subject ||
				generated->parameter_context != source->context_id ||
				generated->parameter_count != 0 || generated->index_count != 2 ||
				prototype_context_extension_path(
					contexts,
					generated->parameter_context,
					generated->index_context,
					index_path,
					2,
					&index_count
				) != 0 || index_count != 2 ||
				prototype_context_classifier_term(
					prototype_context_get(contexts, index_path[0])
				) != source->subject ||
				prototype_context_classifier_term(
					prototype_context_get(contexts, index_path[1])
				) != source->subject ||
				identity->endpoint_context_id != generated->index_context ||
				identity->left_endpoint_binding_id !=
					prototype_context_get(contexts, index_path[0])->binding_id ||
				identity->right_endpoint_binding_id !=
					prototype_context_get(contexts, index_path[1])->binding_id ||
				prototype_term_type_instance_info(
					terms,
					identity->backing_type_former_term_id,
					&former_type_id,
					former_args,
					&former_arg_count
				) != 0 || former_type_id !=
					identity->generated_type_declaration_id ||
				former_arg_count != 0 || !former_claim ||
				former_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				former_claim->context_id != source->context_id ||
				former_claim->subject != identity->backing_type_former_term_id ||
				former_claim->classifier != generated->formation_classifier ||
				prototype_term_type_instance_info(
					terms,
					identity->identity_type_term_id,
					&identity_type_id,
					identity_type_args,
					&identity_type_arg_count
				) != 0 || identity_type_id !=
					identity->generated_type_declaration_id ||
				identity_type_arg_count != 2 || !identity_type_claim ||
				identity_type_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				identity_type_claim->context_id != identity->endpoint_context_id ||
				identity_type_claim->subject != identity->identity_type_term_id ||
				identity_type_claim->classifier != source->classifier) {
				return 0;
			}
			const struct prototype_context* left_index =
				prototype_context_get(contexts, index_path[0]);
			const struct prototype_context* right_index =
				prototype_context_get(contexts, index_path[1]);
			uint32_t right_family;
			uint32_t right_pi;
			uint32_t left_family;
			uint32_t expected_formation;
			if (!left_index || !right_index || prototype_term_pure_family(
					terms,
					right_index->binding_id,
					source->classifier,
					&right_family
				) != 0 || prototype_term_pi_family(
					terms, source->subject, right_family, &right_pi
				) != 0 || prototype_term_pure_family(
					terms,
					left_index->binding_id,
					right_pi,
					&left_family
				) != 0 || prototype_term_pi_family(
					terms, source->subject, left_family, &expected_formation
				) != 0 || expected_formation != generated->formation_classifier) {
				return 0;
			}
			int formation_derivation = 0;
			for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
				const struct prototype_judgement_derivation* derivation =
					&judgement->derivations[i];
				if (derivation->conclusion_claim_id ==
						identity->backing_type_former_has_type_claim_id &&
					derivation->proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO) {
					formation_derivation = 1;
					break;
				}
			}
			uint32_t left_endpoint;
			uint32_t right_endpoint;
			if (!formation_derivation || generated->constructor_count == 0 ||
				prototype_term_var(
					terms, identity->left_endpoint_binding_id, &left_endpoint
				) != 0 || prototype_term_var(
					terms, identity->right_endpoint_binding_id, &right_endpoint
				) != 0 || identity_type_args[0] != left_endpoint ||
				identity_type_args[1] != right_endpoint) {
				return 0;
			}
			if (identity->computation_rule ==
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) {
				return prototype_type_declaration_validate_generated_identity(
					terms,
					type_declarations,
					contexts,
					source->subject,
					identity->generated_type_declaration_id,
					identity->computation_rule
				);
			}
			return prototype_type_declaration_validate_generated_identity(
				terms,
				type_declarations,
				contexts,
				source->subject,
				identity->generated_type_declaration_id,
				identity->computation_rule
			);
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.term.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.term.source_bridge_id
				);
			const struct prototype_hott_action_request* type_request =
				prototype_hott_action_request_get(
					db, request->key.term.relation_type_action_request_id
				);
			const struct prototype_hott_action_certificate* type_certificate = NULL;
			for (uint32_t i = 0; i < db->result_count; ++i) {
				const struct prototype_hott_action_result* result = &db->results[i];
				if (result->request_id == request->key.term.relation_type_action_request_id &&
					result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
					result->certificate_id < db->certificate_count) {
					type_certificate = &db->certificates[result->certificate_id];
					break;
				}
			}
			const struct prototype_hott_term_action_certificate* term =
				&certificate->data.term;
			const struct prototype_substitution* endpoint =
				prototype_substitution_get(
					substitutions, term->endpoint_instantiation_substitution_id
				);
			const struct prototype_cwf_certificate* left_extension =
				prototype_cwf_certificate_db_get_kind(
					cwf_certificates,
					term->left_endpoint_substitution_certificate_id,
					PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
				);
			const struct prototype_cwf_certificate* right_extension =
				prototype_cwf_certificate_db_get_kind(
					cwf_certificates,
					term->right_endpoint_substitution_certificate_id,
					PROTOTYPE_CWF_CERTIFICATE_SUBSTITUTION_FORMATION
				);
			const struct prototype_judgement_claim* witness_claim =
				prototype_judgement_claim_get(
					judgement, term->witness_has_type_claim_id
				);
			const struct prototype_judgement_claim* left_endpoint_claim =
				left_extension ? prototype_judgement_claim_get(
					judgement, left_extension->claim_id
				) : NULL;
			const struct prototype_judgement_claim* right_endpoint_claim =
				right_extension ? prototype_judgement_claim_get(
					judgement, right_extension->claim_id
				) : NULL;
			if (request->kind != PROTOTYPE_HOTT_ACTION_TERM || !source || !bridge ||
				!type_request || !type_certificate ||
				type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE ||
				!endpoint || !left_extension || !right_extension || !witness_claim ||
				!left_endpoint_claim || !right_endpoint_claim ||
				endpoint->source_context != bridge->bridge_context_id ||
				endpoint->target_context !=
					type_certificate->data.relation_type.endpoint_context_id ||
				endpoint->kind != PROTOTYPE_SUBSTITUTION_EXTEND ||
				left_extension->structural_id != endpoint->first ||
				right_extension->structural_id !=
					term->endpoint_instantiation_substitution_id ||
				prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->context_id != bridge->bridge_context_id ||
				prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->subject != term->witness_term_id) {
				return 0;
			}
			uint32_t relation;
			return prototype_term_reindex(
				(struct prototype_term_db*)terms,
				type_declarations,
				contexts,
				(struct prototype_substitution_db*)substitutions,
				type_certificate->data.relation_type.relation_type_term_id,
				term->endpoint_instantiation_substitution_id,
				&relation
			) == 0 && relation == prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->classifier;
		}
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM:
		{
			const struct prototype_hott_object_term_action_certificate* object =
				&certificate->data.object_term;
			const struct prototype_judgement_proposition* source =
				prototype_judgement_claim_proposition(
					judgement, request->key.object_term.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.object_term.source_bridge_id
				);
			const struct prototype_hott_action_request* identity_request =
				prototype_hott_action_request_get(
					db, request->key.object_term.identity_type_action_request_id
				);
			const struct prototype_hott_action_certificate* identity_certificate = NULL;
			for (uint32_t i = 0; i < db->result_count; ++i) {
				const struct prototype_hott_action_result* result = &db->results[i];
				if (result->request_id ==
						request->key.object_term.identity_type_action_request_id &&
					result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
					result->certificate_id < db->certificate_count) {
					identity_certificate = &db->certificates[result->certificate_id];
					break;
				}
			}
			const struct prototype_judgement_proposition* identity_source =
				identity_request && identity_request->kind ==
					PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
				prototype_judgement_claim_proposition(
					judgement,
					identity_request->key.identity_type.source_claim_id
				) : NULL;
			const struct prototype_judgement_proposition* left =
				prototype_judgement_claim_proposition(
					judgement, object->left_endpoint_claim_id
				);
			const struct prototype_judgement_proposition* right =
				prototype_judgement_claim_proposition(
					judgement, object->right_endpoint_claim_id
				);
			const struct prototype_judgement_proposition* family =
				prototype_judgement_claim_proposition(
					judgement, object->identity_family_has_type_claim_id
				);
			const struct prototype_judgement_proposition* witness =
				prototype_judgement_claim_proposition(
					judgement, object->witness_has_type_claim_id
				);
			if (request->kind != PROTOTYPE_HOTT_ACTION_OBJECT_TERM || !source ||
				!bridge || !identity_request || !identity_certificate ||
				identity_certificate->kind !=
					PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
				!identity_source || !left || !right || !family || !witness ||
				source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				identity_source->subject != source->classifier ||
				left->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				right->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				left->context_id != bridge->bridge_context_id ||
				right->context_id != bridge->bridge_context_id ||
				family->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				family->context_id != bridge->bridge_context_id ||
				family->classifier != identity_source->classifier ||
				witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				witness->context_id != bridge->bridge_context_id ||
				witness->subject != object->witness_term_id ||
				witness->classifier != family->subject) {
				return 0;
			}
			uint32_t expected_left;
			uint32_t expected_right;
			uint32_t expected_left_classifier;
			uint32_t expected_right_classifier;
			if (prototype_term_reindex(
					(struct prototype_term_db*)terms,
					type_declarations,
					contexts,
					(struct prototype_substitution_db*)substitutions,
					source->subject,
					bridge->left_substitution_id,
					&expected_left
				) != 0 || prototype_term_reindex(
					(struct prototype_term_db*)terms,
					type_declarations,
					contexts,
					(struct prototype_substitution_db*)substitutions,
					source->subject,
					bridge->right_substitution_id,
					&expected_right
				) != 0 || prototype_term_reindex(
					(struct prototype_term_db*)terms,
					type_declarations,
					contexts,
					(struct prototype_substitution_db*)substitutions,
					source->classifier,
					bridge->left_substitution_id,
					&expected_left_classifier
				) != 0 || prototype_term_reindex(
					(struct prototype_term_db*)terms,
					type_declarations,
					contexts,
					(struct prototype_substitution_db*)substitutions,
					source->classifier,
					bridge->right_substitution_id,
					&expected_right_classifier
				) != 0 || left->subject != expected_left ||
				left->classifier != expected_left_classifier ||
				right->subject != expected_right ||
				right->classifier != expected_right_classifier) {
				return 0;
			}
			uint32_t expected_family = identity_certificate->data.identity_type.
				identity_type_term_id;
			if (prototype_term_graph_substitute_bound_var(
					(struct prototype_term_db*)terms,
					type_declarations,
					expected_family,
					identity_certificate->data.identity_type.left_endpoint_binding_id,
					left->subject,
					&expected_family
				) != 0 || prototype_term_graph_substitute_bound_var(
					(struct prototype_term_db*)terms,
					type_declarations,
					expected_family,
					identity_certificate->data.identity_type.right_endpoint_binding_id,
					right->subject,
					&expected_family
				) != 0) {
				return 0;
			}
			return family->subject == expected_family;
		}
	default:
		return 0;
	}
}

int prototype_hott_action_certificate_add(
	struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_hott_action_certificate certificate,
	uint32_t* p_certificate_id
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
	if (!db || !p_certificate_id || !db->certificates ||
		db->certificate_count >= db->certificate_capacity ||
		prototype_kernel_view_validate(kernel) != 0) {
		return -1;
	}
	certificate.id = (uint32_t)db->certificate_count;
	if (!hott_action_certificate_is_valid(
			db,
			&certificate,
			certificate.id,
			contexts,
			substitutions,
			cwf_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement
		)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (db->certificates[i].request_id == certificate.request_id) {
			return -1;
		}
	}
	db->certificates[db->certificate_count++] = certificate;
	*p_certificate_id = certificate.id;
	return 0;
}

int prototype_hott_action_result_publish(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action_result result,
	uint32_t* p_result_id
) {
	if (db) {
		db->outcome_publish_requests++;
	}
	if (!db || !p_result_id || !db->results ||
		db->result_count >= db->result_capacity ||
		!prototype_hott_action_request_get(db, result.request_id) ||
		!hott_deterministic_outcome_is_valid(&result.outcome, 0)) {
		return -1;
	}
	if (result.outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		if (result.outcome.residual_reason != PROTOTYPE_HOTT_RESIDUAL_NONE ||
			result.certificate_id >= db->certificate_count ||
			db->certificates[result.certificate_id].request_id !=
				result.request_id) {
			return -1;
		}
	} else if (!hott_residual_reason_is_valid(result.outcome.residual_reason) ||
		result.outcome.residual_reason == PROTOTYPE_HOTT_RESIDUAL_NONE ||
		result.certificate_id != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = 0; i < db->result_count; ++i) {
		if (db->results[i].request_id == result.request_id) {
			return -1;
		}
	}
	result.id = (uint32_t)db->result_count;
	db->results[db->result_count++] = result;
	*p_result_id = result.id;
	return 0;
}

static int hott_action_result_for_request(
	const struct prototype_hott_action_db* db,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	if (!db || !p_result_id) {
		return -1;
	}
	for (uint32_t i = 0; i < db->result_count; ++i) {
		if (db->results[i].request_id == request_id) {
			*p_result_id = i;
			return 0;
		}
	}
	return 1;
}

static int hott_zero_field_ordinary_adt(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t type_term
) {
	uint32_t type_id;
	const struct prototype_type_declaration* type;
	if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			type_term,
			&type_id,
			&type
		) != 0 || type_id >= type_declarations->type_count) {
		return 0;
	}
	if (type->parameter_count != 0 || type->constructor_count == 0) {
		return 0;
	}
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		uint32_t constructor_id = type->first_constructor + i;
		if (constructor_id >= type_declarations->constructor_count) {
			return 0;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		const struct prototype_context* parameters = prototype_context_get(
			contexts, constructor->parameter_context
		);
		const struct prototype_context* fields = prototype_context_get(
			contexts, constructor->field_context
		);
		if (!parameters || !fields || parameters->depth != fields->depth) {
			return 0;
		}
	}
	return 1;
}

static int hott_generated_identity_declaration(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t source_carrier,
	uint32_t parameter_context,
	uint32_t* p_type_id
) {
	if (!type_declarations || !p_type_id) {
		return -1;
	}
	for (uint32_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[i];
		if (type->origin_kind ==
				PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
			type->origin_source_carrier_term_id == source_carrier &&
			type->parameter_context == parameter_context &&
			type->formation_classifier != PROTOTYPE_INVALID_ID) {
			*p_type_id = i;
			return 0;
		}
	}
	return 1;
}

static int hott_adt_constructor_field_count(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_type_declaration* source_type,
	uint32_t constructor_ordinal,
	uint32_t* p_field_count
) {
	if (!type_declarations || !contexts || !source_type || !p_field_count ||
		constructor_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + constructor_ordinal >=
			type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	uint32_t field_path[64];
	return prototype_context_extension_path(
		contexts,
		constructor->parameter_context,
		constructor->field_context,
		field_path,
		64,
		p_field_count
	);
}

static int hott_indexed_constructor_compatible(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	uint32_t constructor_ordinal
) {
	uint32_t field_count;
	if (!terms || !type_declarations || !contexts || !source_type ||
		hott_adt_constructor_field_count(
			type_declarations,
			contexts,
			source_type,
			constructor_ordinal,
			&field_count
		) != 0) {
		return -1;
	}
	if (source_type->index_count == 0) {
		return 1;
	}
	/* Non-nullary indexed constructors require lifting their result indices
	 * through the field telescope. That rule remains residual. */
	if (field_count != 0) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	/* This fragment selects one exact indexed fiber. Conversion is not enough:
	 * replay must recover the same constructor set without normalization work. */
	(void)terms;
	return constructor->result_classifier == source_carrier;
}

static int hott_generated_constructor_index_for_source_ordinal(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t generated_type_id,
	uint32_t source_carrier,
	uint32_t source_ordinal,
	uint32_t* p_generated_index
) {
	if (!terms || !type_declarations || !p_generated_index ||
		generated_type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* generated =
		&type_declarations->type_declarations[generated_type_id];
	for (uint32_t i = 0; i < generated->constructor_count; ++i) {
		uint32_t constructor_id = generated->first_constructor + i;
		if (constructor_id >= type_declarations->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		uint32_t result_type_id;
		uint32_t endpoints[2];
		uint32_t endpoint_count;
		uint32_t left_head;
		uint32_t left_owner;
		uint32_t left_ordinal;
		uint32_t left_arguments[64];
		uint32_t left_argument_count;
		uint32_t right_head;
		uint32_t right_owner;
		uint32_t right_ordinal;
		uint32_t right_arguments[64];
		uint32_t right_argument_count;
		if (prototype_term_type_instance_info(
				terms,
				constructor->result_classifier,
				&result_type_id,
				endpoints,
				&endpoint_count
			) != 0 || result_type_id != generated_type_id || endpoint_count != 2 ||
			prototype_term_constructor_spine_info(
				terms, endpoints[0], &left_head, &left_owner, &left_ordinal,
				left_arguments, 64, &left_argument_count
			) != 0 || prototype_term_constructor_spine_info(
				terms, endpoints[1], &right_head, &right_owner, &right_ordinal,
				right_arguments, 64, &right_argument_count
			) != 0) {
			return -1;
		}
		if (left_owner == source_carrier && right_owner == source_carrier &&
			left_ordinal == source_ordinal && right_ordinal == source_ordinal) {
			*p_generated_index = constructor->constructor_index;
			return 0;
		}
		(void)left_head;
		(void)right_head;
		(void)left_arguments;
		(void)right_arguments;
		(void)left_argument_count;
		(void)right_argument_count;
	}
	return 1;
}

static int hott_ordinary_adt_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type
) {
	if (!terms || !type_declarations || !contexts || !source_type ||
		source_type->parameter_count != 0 || source_type->constructor_count == 0) {
		return 0;
	}
	uint32_t source_type_id;
	uint32_t source_arguments[16];
	uint32_t source_argument_count;
	if (prototype_term_type_instance_info(
			terms,
			source_carrier,
			&source_type_id,
			source_arguments,
			&source_argument_count
		) != 0 || source_type_id != source_type->type_index ||
		source_argument_count != source_type->index_count) {
		return 0;
	}
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t constructor_id = source_type->first_constructor + i;
		if (constructor_id >= type_declarations->constructor_count) {
			return 0;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		uint32_t field_path[64];
		uint32_t field_count;
		if (constructor->parameter_context != source_type->parameter_context ||
			prototype_context_extension_path(
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				field_path,
				64,
				&field_count
			) != 0) {
			return 0;
		}
		if (source_type->index_count > 0 && field_count > 0) {
			return 0;
		}
		for (uint32_t j = 0; j < field_count; ++j) {
			const struct prototype_context* field =
				prototype_context_get(contexts, field_path[j]);
			if (!field) {
				return 0;
			}
			uint32_t field_classifier =
				prototype_context_classifier_term(field);
			int dependent = 0;
			for (uint32_t k = 0; k < j; ++k) {
				const struct prototype_context* previous =
					prototype_context_get(contexts, field_path[k]);
				if (!previous) {
					return 0;
				}
				dependent = dependent || prototype_term_contains_free_binding(
					terms, field_classifier, previous->binding_id
				);
			}
			if (dependent) {
				/* The Context identity action selects this fiber from the
				 * identities of the preceding fields. */
				continue;
			}
			uint32_t ignored_identity;
			if (field_classifier != source_carrier &&
				hott_generated_identity_declaration(
					type_declarations,
					field_classifier,
					source_type->parameter_context,
					&ignored_identity
				) != 0) {
				return 0;
			}
		}
	}
	(void)source_arguments;
	return 1;
}

static int hott_constructor_has_recursive_field(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_constructor_declaration* constructor
) {
	uint32_t fields[64];
	uint32_t field_count;
	if (!terms || !contexts || !constructor || prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			fields,
			64,
			&field_count
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		const struct prototype_context* field = prototype_context_get(
			contexts, fields[i]
		);
		if (!field) {
			return -1;
		}
		if (prototype_context_classifier_term(field) == source_carrier) {
			return 1;
		}
	}
	return 0;
}

static int hott_indexed_higher_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_type_declaration* source_identity
) {
	if (!terms || !type_declarations || !contexts || !source_identity ||
		source_identity->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		source_identity->parameter_count != 0 || source_identity->index_count != 2) {
		return 0;
	}
	uint32_t original_type_id;
	const struct prototype_type_declaration* original;
	uint32_t original_carrier = source_identity->origin_source_carrier_term_id;
	if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			original_carrier,
			&original_type_id,
			&original
		) != 0 || original_type_id >= type_declarations->type_count ||
		!original || !hott_ordinary_adt_identity_supported(
			terms,
			type_declarations,
			contexts,
			original_carrier,
			original
		)) {
		return 0;
	}
	/* The current eight-face lift only builds products of independent field
	 * squares. A field whose classifier uses an earlier constructor binder
	 * needs a dependent square and recursive coherence data. Keep that case
	 * residual instead of exposing a partial higher identity. */
	for (uint32_t i = 0; i < original->constructor_count; ++i) {
		uint32_t constructor_id = original->first_constructor + i;
		if (constructor_id >= type_declarations->constructor_count) {
			return 0;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[constructor_id];
		uint32_t fields[64];
		uint32_t field_count;
		if (prototype_context_extension_path(
				contexts,
				constructor->parameter_context,
				constructor->field_context,
				fields,
				64,
				&field_count
			) != 0) {
			return 0;
		}
		for (uint32_t j = 0; j < field_count; ++j) {
			const struct prototype_context* field = prototype_context_get(
				contexts, fields[j]
			);
			if (!field) {
				return 0;
			}
			uint32_t classifier = prototype_context_classifier_term(field);
			for (uint32_t k = 0; k < j; ++k) {
				const struct prototype_context* previous = prototype_context_get(
					contexts, fields[k]
				);
				if (!previous || prototype_term_contains_free_binding(
						terms, classifier, previous->binding_id
					)) {
					return 0;
				}
			}
		}
	}
	return 1;
}

static int hott_thunk_pi_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t thunk_type
);

static int hott_thunk_return_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t thunk_type
) {
	if (!terms || !type_declarations || thunk_type >= terms->term_count ||
		terms->terms[thunk_type].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t computation = terms->terms[thunk_type].as.thunk_type.computation;
	if (computation >= terms->term_count ||
		terms->terms[computation].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_effect_row_purity(
			terms, terms->terms[computation].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return 0;
	}
	uint32_t result_carrier =
		terms->terms[computation].as.computation_type.result;
	uint32_t result_identity;
	return hott_generated_identity_declaration(
		type_declarations,
		result_carrier,
		parameter_context,
		&result_identity
	) == 0 || hott_thunk_pi_identity_supported(
		terms, type_declarations, parameter_context, result_carrier
	);
}

static int hott_thunk_pi_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t thunk_type
) {
	if (!terms || !type_declarations || thunk_type >= terms->term_count ||
		terms->terms[thunk_type].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	uint32_t pi = terms->terms[thunk_type].as.thunk_type.computation;
	if (pi >= terms->term_count || terms->terms[pi].tag != PROTOTYPE_TERM_PI) {
		return 0;
	}
	uint32_t codomain_binding;
	uint32_t codomain;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) != 0 || codomain >= terms->term_count ||
		terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_contains_free_binding(
			terms, codomain, codomain_binding
		) || prototype_term_effect_row_purity(
			terms, terms->terms[codomain].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return 0;
	}
	uint32_t domain_identity;
	if (hott_generated_identity_declaration(
			type_declarations,
			terms->terms[pi].as.pi.domain,
			parameter_context,
			&domain_identity
		) != 0) {
		return 0;
	}
	for (uint32_t i = 0; i < type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* identity =
			&type_declarations->type_declarations[i];
		uint32_t carrier = identity->origin_source_carrier_term_id;
		if (identity->origin_kind ==
				PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY &&
			carrier < terms->term_count &&
			terms->terms[carrier].tag == PROTOTYPE_TERM_THUNK_TYPE &&
			terms->terms[carrier].as.thunk_type.computation == codomain &&
			identity->formation_classifier != PROTOTYPE_INVALID_ID) {
			return 1;
		}
	}
	return 0;
}

static int hott_type_pi_identity_supported(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t parameter_context,
	uint32_t pi
) {
	if (!terms || !type_declarations || pi >= terms->term_count ||
		terms->terms[pi].tag != PROTOTYPE_TERM_PI) {
		return 0;
	}
	uint32_t codomain_binding;
	uint32_t codomain;
	uint32_t domain_identity;
	uint32_t codomain_identity;
	return prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) == 0 && codomain < terms->term_count &&
		terms->terms[codomain].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
		!prototype_term_contains_free_binding(terms, codomain, codomain_binding) &&
		hott_generated_identity_declaration(
			type_declarations,
			terms->terms[pi].as.pi.domain,
			parameter_context,
			&domain_identity
		) == 0 && hott_generated_identity_declaration(
			type_declarations,
			codomain,
			parameter_context,
			&codomain_identity
		) == 0;
}

static int hott_type_former_descriptor_finalize(
	struct prototype_hott_type_former_descriptor* descriptor
) {
	if (!descriptor || !descriptor->admitted) {
		return 0;
	}
	descriptor->capabilities =
		(struct prototype_hott_type_former_capabilities) {
			.relation_type_action = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED,
			.term_action = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED,
			.ordinary_reindex = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED,
			.purity = PROTOTYPE_HOTT_CAPABILITY_SUPPORTED,
			.identity_computation = PROTOTYPE_HOTT_CAPABILITY_DEFERRED,
			.transport = PROTOTYPE_HOTT_CAPABILITY_DEFERRED,
			.lifting = PROTOTYPE_HOTT_CAPABILITY_DEFERRED,
			.resource_hook = PROTOTYPE_HOTT_CAPABILITY_DEFERRED,
			.artifact = PROTOTYPE_HOTT_CAPABILITY_DEFERRED
		};
	switch (descriptor->kind) {
	case PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT:
		descriptor->child_role_mask =
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_ADT_FIELD) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_ADT_DEPENDENT_REINDEX);
		break;
	case PROTOTYPE_HOTT_TYPE_FORMER_PI:
		descriptor->child_role_mask =
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_PI_DOMAIN_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_PI_RELATED_INPUT) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_PI_CODOMAIN_RELATION);
		break;
	case PROTOTYPE_HOTT_TYPE_FORMER_PURE_COMPUTATION:
		descriptor->child_role_mask =
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_COMP_LEFT_RETURN_EXPOSURE) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_COMP_RIGHT_RETURN_EXPOSURE) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_COMP_RESULT_RELATION);
		break;
	case PROTOTYPE_HOTT_TYPE_FORMER_THUNK:
		descriptor->child_role_mask = UINT64_C(1) <<
			PROTOTYPE_HOTT_CHILD_THUNK_COMPUTATION_RELATION;
		break;
	case PROTOTYPE_HOTT_TYPE_FORMER_RELATION:
		descriptor->child_role_mask =
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_CONTEXT_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_SUBSTITUTION_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_RELATION_TYPE_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_TERM_ACTION) |
			(UINT64_C(1) << PROTOTYPE_HOTT_CHILD_REINDEX_NATURALITY);
		break;
	default:
		return -1;
	}
	return descriptor->child_role_mask != 0 ? 0 : -1;
}

int prototype_hott_type_former_descriptor_query(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_db* judgement,
	uint32_t source_claim_id,
	struct prototype_hott_type_former_descriptor* p_descriptor
) {
	const struct prototype_judgement_claim* source =
		prototype_judgement_claim_get(judgement, source_claim_id);
	if (!terms || !type_declarations || !contexts || !source || !p_descriptor ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->subject >= terms->term_count) {
		return -1;
	}
	struct prototype_hott_type_former_descriptor descriptor = {
		.kind = PROTOTYPE_HOTT_TYPE_FORMER_UNKNOWN,
		.admitted = 0,
		.relation_type_action_rule = PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_NONE,
		.residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
		.source_claim_id = source_claim_id,
		.source_type_term_id = prototype_judgement_proposition_get(judgement, source->proposition_id)->subject
	};
	uint32_t exposed_type = prototype_judgement_proposition_get(judgement, source->proposition_id)->subject;
	struct prototype_term_normalization_result exposure;
	if (prototype_term_normalize_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			prototype_judgement_proposition_get(judgement, source->proposition_id)->subject,
			64,
			&exposure
		) == 0 && exposure.status ==
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE &&
		exposure.term_id < terms->term_count) {
		exposed_type = exposure.term_id;
	}
	const struct prototype_term* type = &terms->terms[exposed_type];
	uint32_t ordinary_type_id;
	const struct prototype_type_declaration* ordinary_type;
	if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
		exposed_type,
			&ordinary_type_id,
			&ordinary_type
		) == 0 && ordinary_type_id < type_declarations->type_count &&
		ordinary_type) {
		descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT;
		descriptor.admitted = 1;
		descriptor.relation_type_action_rule = hott_zero_field_ordinary_adt(
			terms, type_declarations, contexts, exposed_type
		) ? PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_ZERO_FIELD_ADT :
			PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_ADT_TELESCOPE;
		descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	} else {
		uint32_t relation_left_type;
		uint32_t relation_right_type;
		uint32_t relation_left;
		uint32_t relation_right;
		switch (type->tag) {
		case PROTOTYPE_TERM_TYPE_VIEW:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT;
			descriptor.relation_type_action_rule =
				PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_ADT_TELESCOPE;
			break;
		case PROTOTYPE_TERM_PI:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_PI;
			descriptor.relation_type_action_rule =
				PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_PI_POINTWISE;
			{
				int reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				int status = hott_term_fragment_scan(
					terms, prototype_judgement_proposition_get(judgement, source->proposition_id)->subject, 0, &reason
				);
				if (status < 0) {
					return -1;
				}
				if (status == 0) {
					descriptor.admitted = 1;
					descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				} else {
					descriptor.residual_reason = reason;
				}
			}
			break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_PURE_COMPUTATION;
			descriptor.relation_type_action_rule =
				PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_PURE_COMPUTATION;
			switch (prototype_term_effect_row_purity(
				terms, type->as.computation_type.label
			)) {
			case PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL:
				descriptor.residual_reason =
					PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL;
				break;
			case PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED:
				descriptor.residual_reason =
					PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED;
				break;
			default:
				descriptor.admitted = 1;
				descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				break;
			}
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_THUNK;
			descriptor.relation_type_action_rule =
				PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_THUNK;
			if (type->as.thunk_type.computation < terms->term_count &&
				terms->terms[type->as.thunk_type.computation].tag ==
					PROTOTYPE_TERM_COMPUTATION_TYPE) {
				int purity = prototype_term_effect_row_purity(
					terms,
					terms->terms[type->as.thunk_type.computation].
						as.computation_type.label
				);
				if (purity == PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
					descriptor.admitted = 1;
					descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				} else {
					descriptor.residual_reason = purity ==
						PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL ?
						PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL :
						PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED;
				}
			} else if (type->as.thunk_type.computation < terms->term_count &&
				terms->terms[type->as.thunk_type.computation].tag ==
					PROTOTYPE_TERM_PI) {
				int reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				int status = hott_term_fragment_scan(
					terms, type->as.thunk_type.computation, 0, &reason
				);
				if (status < 0) {
					return -1;
				}
				if (status == 0) {
					descriptor.admitted = 1;
					descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				} else {
					descriptor.residual_reason = reason;
				}
			}
			break;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE;
			descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_UNIVERSE;
			break;
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_HOST_PRIMITIVE;
			descriptor.residual_reason =
				PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE;
			break;
		default:
			if (prototype_term_relation_type_info(
					terms,
					prototype_judgement_proposition_get(judgement, source->proposition_id)->subject,
					&relation_left_type,
					&relation_right_type,
					&relation_left,
					&relation_right
				) == 0) {
				descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_RELATION;
				descriptor.admitted = 1;
				descriptor.relation_type_action_rule =
					PROTOTYPE_HOTT_RELATION_TYPE_ACTION_RULE_RELATION_HIGHER;
				descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
			}
			break;
		}
	}
	if (hott_type_former_descriptor_finalize(&descriptor) != 0) {
		return -1;
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT &&
		descriptor.admitted) {
		uint32_t source_type_id;
		const struct prototype_type_declaration* source_type;
		if (prototype_type_view_declaration_query(
				type_declarations,
				contexts,
				terms,
				exposed_type,
				&source_type_id,
				&source_type
		) == 0 && source_type_id < type_declarations->type_count &&
		(source_type->origin_kind ==
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ?
			hott_indexed_higher_identity_supported(
				terms, type_declarations, contexts, source_type
			) : hott_ordinary_adt_identity_supported(
				terms,
				type_declarations,
				contexts,
				exposed_type,
				source_type
			))) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	} else if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_PI &&
		descriptor.admitted) {
		uint32_t source_context = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->context_id;
		uint32_t identity_context = source_context;
		if (source_context != prototype_context_empty(contexts) &&
			hott_term_is_constant_over_context(
				terms, contexts, source_context, exposed_type
			)) {
			identity_context = prototype_context_empty(contexts);
		}
		if (hott_type_pi_identity_supported(
				terms,
				type_declarations,
				identity_context,
				exposed_type
			)) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	} else if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_THUNK &&
		descriptor.admitted) {
		uint32_t source_context = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->context_id;
		uint32_t identity_context = source_context;
		if (source_context != prototype_context_empty(contexts) &&
			hott_term_is_constant_over_context(
				terms, contexts, source_context, exposed_type
			)) {
			identity_context = prototype_context_empty(contexts);
		}
		if (hott_thunk_return_identity_supported(
			terms,
			type_declarations,
			identity_context,
			exposed_type
		) || hott_thunk_pi_identity_supported(
			terms,
			type_declarations,
			identity_context,
			exposed_type
		)) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	} else if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE &&
		prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->classifier < terms->term_count && terms->terms[
			prototype_judgement_proposition_get(
				judgement, source->proposition_id
			)->classifier
		].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		uint32_t source_context = prototype_judgement_proposition_get(
			judgement, source->proposition_id
		)->context_id;
		uint32_t generated_identity;
		if (source_context == prototype_context_empty(contexts) ||
			(hott_term_is_constant_over_context(
				terms, contexts, source_context, exposed_type
			) && prototype_type_declaration_find_generated_identity(
				type_declarations,
				exposed_type,
				prototype_context_empty(contexts),
				&generated_identity
			) == 0)) {
			descriptor.capabilities.identity_computation =
				PROTOTYPE_HOTT_CAPABILITY_SUPPORTED;
		}
	}
	*p_descriptor = descriptor;
	return 0;
}

static int hott_publish_action_residual(
	struct prototype_hott_action_db* actions,
	const struct prototype_term_db* terms,
	uint32_t request_id,
	int reason,
	uint32_t* p_result_id
) {
	struct prototype_hott_action_result result = {
		.request_id = request_id,
		.certificate_id = PROTOTYPE_INVALID_ID,
		.outcome = {
			.state = reason == PROTOTYPE_HOTT_RESIDUAL_UNIVERSE ||
				reason == PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE ?
				PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED :
				PROTOTYPE_HOTT_ACTION_RESULT_RESIDUAL,
			.residual_reason = reason,
			.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			.step_limit = 0,
			.term_graph_revision = terms->term_count
		}
	};
	memcpy(result.outcome.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT, 65);
	return prototype_hott_action_result_publish(actions, result, p_result_id);
}

static int hott_publish_ready_action_result(
	struct prototype_hott_action_db* actions,
	const struct prototype_term_db* terms,
	uint32_t request_id,
	uint32_t certificate_id,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_result_id
) {
	if (!actions || !terms || !p_result_id || certificate_id >=
		actions->certificate_count || certificate_id + 1 !=
		actions->certificate_count || actions->certificates[certificate_id].request_id !=
		request_id) {
		return -1;
	}
	struct prototype_hott_action_result result = {
		.request_id = request_id,
		.certificate_id = certificate_id,
		.outcome = {
			.state = PROTOTYPE_HOTT_ACTION_RESULT_READY,
			.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
			.normalization_profile = normalization_profile,
			.step_limit = step_limit,
			.term_graph_revision = terms->term_count
		}
	};
	memcpy(result.outcome.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT, 65);
	if (prototype_hott_action_result_publish(actions, result, p_result_id) != 0) {
		memset(&actions->certificates[certificate_id], 0,
			sizeof(actions->certificates[certificate_id]));
		actions->certificate_count--;
		return -1;
	}
	return 0;
}

static int hott_initialize_generated_identity_declaration(
	struct prototype_term_db* terms,
	struct prototype_context_db* contexts,
	uint32_t source_context,
	uint32_t source_carrier,
	uint32_t universe,
	struct prototype_type_declaration* generated
) {
	if (!terms || !contexts || !generated ||
		source_carrier >= terms->term_count || universe >= terms->term_count) {
		return -1;
	}
	if (generated->formation_classifier != PROTOTYPE_INVALID_ID) {
		return generated->parameter_context == source_context &&
			generated->parameter_count == 0 && generated->index_count == 2 ?
			0 : -1;
	}
	if (generated->parameter_context != source_context ||
		generated->index_context != PROTOTYPE_INVALID_ID ||
		generated->parameter_count != 0 || generated->index_count != 0 ||
		generated->constructor_count != 0) {
		return -1;
	}
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t right_context;
	uint32_t right_family;
	uint32_t right_pi;
	uint32_t left_family;
	uint32_t formation_classifier;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			source_context,
			left_binding,
			source_carrier,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			source_carrier,
			PROTOTYPE_INVALID_ID,
			&right_context
		) != 0 || prototype_term_pure_family(
			terms, right_binding, universe, &right_family
		) != 0 || prototype_term_pi_family(
			terms, source_carrier, right_family, &right_pi
		) != 0 || prototype_term_pure_family(
			terms, left_binding, right_pi, &left_family
		) != 0 || prototype_term_pi_family(
			terms, source_carrier, left_family, &formation_classifier
		) != 0) {
		return -1;
	}
	generated->parameter_context = source_context;
	generated->index_context = right_context;
	generated->index_count = 2;
	generated->formation_classifier = formation_classifier;
	return 0;
}

static int hott_make_dependent_pi(
	struct prototype_term_db* terms,
	uint32_t domain,
	uint32_t binding_id,
	uint32_t codomain,
	uint32_t* p_pi
) {
	uint32_t family;
	if (!terms || !p_pi || binding_id == PROTOTYPE_INVALID_ID ||
		domain >= terms->term_count || codomain >= terms->term_count ||
		prototype_term_pure_family(
			terms, binding_id, codomain, &family
		) != 0) {
		return -1;
	}
	return prototype_term_pi_family(terms, domain, family, p_pi);
}

/* Universe identity is a selected correspondence with explicit two-sided
 * transport and fiber witnesses. These are pure type-level operations; they
 * are not the compiler-local RELATION_TYPE action and do not carry effects. */
static int hott_add_universe_correspondence_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* types,
	struct prototype_context_db* contexts,
	uint32_t universe,
	uint32_t generated_type_id,
	struct prototype_type_declaration* generated,
	uint32_t* p_constructor_id
) {
	if (!terms || !types || !contexts || !generated || !p_constructor_id ||
		universe >= terms->term_count || generated->constructor_count != 0 ||
		generated->parameter_context != prototype_context_empty(contexts)) {
		return -1;
	}

	uint32_t bindings[7];
	uint32_t variables[7];
	uint32_t classifiers[7];
	uint32_t field_context = generated->parameter_context;
	for (uint32_t i = 0; i < 7; ++i) {
		bindings[i] = prototype_term_new_binding(terms);
		if (bindings[i] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	classifiers[0] = universe;
	classifiers[1] = universe;
	for (uint32_t i = 0; i < 2; ++i) {
		uint32_t next_context;
		if (prototype_context_extend(
				contexts,
				field_context,
				bindings[i],
				classifiers[i],
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_term_var(
				terms, bindings[i], &variables[i]
			) != 0) {
			return -1;
		}
		field_context = next_context;
	}

	uint32_t relation_right_binding = prototype_term_new_binding(terms);
	uint32_t relation_right_pi;
	if (relation_right_binding == PROTOTYPE_INVALID_ID ||
		hott_make_dependent_pi(
			terms,
			variables[1],
			relation_right_binding,
			universe,
			&relation_right_pi
		) != 0 || hott_make_dependent_pi(
			terms,
			variables[0],
			prototype_term_new_binding(terms),
			relation_right_pi,
			&classifiers[2]
		) != 0 || hott_make_dependent_pi(
			terms,
			variables[0],
			prototype_term_new_binding(terms),
			variables[1],
			&classifiers[3]
		) != 0 || hott_make_dependent_pi(
			terms,
			variables[1],
			prototype_term_new_binding(terms),
			variables[0],
			&classifiers[4]
		) != 0) {
		return -1;
	}
	for (uint32_t i = 2; i < 5; ++i) {
		uint32_t next_context;
		if (prototype_context_extend(
				contexts,
				field_context,
				bindings[i],
				classifiers[i],
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_term_var(
				terms, bindings[i], &variables[i]
			) != 0) {
			return -1;
		}
		field_context = next_context;
	}

	uint32_t x_binding = prototype_term_new_binding(terms);
	uint32_t x;
	uint32_t trr_x;
	uint32_t relation_x;
	uint32_t right_lift;
	if (x_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, x_binding, &x
		) != 0 || prototype_term_app(
			terms, variables[3], x, &trr_x
		) != 0 || prototype_term_app(
			terms, variables[2], x, &relation_x
		) != 0 || prototype_term_app(
			terms, relation_x, trr_x, &right_lift
		) != 0 || hott_make_dependent_pi(
			terms, variables[0], x_binding, right_lift, &classifiers[5]
		) != 0) {
		return -1;
	}
	uint32_t next_context;
	if (prototype_context_extend(
			contexts,
			field_context,
			bindings[5],
			classifiers[5],
			PROTOTYPE_INVALID_ID,
			&next_context
		) != 0 || prototype_term_var(
			terms, bindings[5], &variables[5]
		) != 0) {
		return -1;
	}
	field_context = next_context;

	uint32_t y_binding = prototype_term_new_binding(terms);
	uint32_t y;
	uint32_t trl_y;
	uint32_t relation_left;
	uint32_t left_lift;
	if (y_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, y_binding, &y
		) != 0 || prototype_term_app(
			terms, variables[4], y, &trl_y
		) != 0 || prototype_term_app(
			terms, variables[2], trl_y, &relation_left
		) != 0 || prototype_term_app(
			terms, relation_left, y, &left_lift
		) != 0 || hott_make_dependent_pi(
			terms, variables[1], y_binding, left_lift, &classifiers[6]
		) != 0 || prototype_context_extend(
			contexts,
			field_context,
			bindings[6],
			classifiers[6],
			PROTOTYPE_INVALID_ID,
			&field_context
		) != 0 || prototype_term_var(
			terms, bindings[6], &variables[6]
		) != 0) {
		return -1;
	}

	uint32_t result_arguments[2] = { variables[0], variables[1] };
	uint32_t result_classifier;
	uint32_t curried_classifier;
	if (prototype_term_type_instance_make(
			terms,
			types,
			generated_type_id,
			result_arguments,
			2,
			&result_classifier
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			generated->parameter_context,
			field_context,
			result_classifier,
			&curried_classifier
		) != 0) {
		return -1;
	}
	uint32_t readback_fields[7];
	for (uint32_t i = 0; i < 7; ++i) {
		readback_fields[i] = PROTOTYPE_INVALID_ID;
	}
	return prototype_type_declaration_add_constructor(
		types,
		generated_type_id,
		-1,
		readback_fields,
		7,
		PROTOTYPE_INVALID_ID,
		generated->parameter_context,
		field_context,
		result_classifier,
		curried_classifier,
		p_constructor_id
	);
}

static int hott_initialize_indexed_higher_identity_declaration(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* types,
	struct prototype_context_db* contexts,
	uint32_t source_type_id,
	uint32_t generated_type_id,
	uint32_t universe
) {
	if (!terms || !types || !contexts || source_type_id >= types->type_count ||
		generated_type_id >= types->type_count || universe >= terms->term_count) {
		return -1;
	}
	const struct prototype_type_declaration* source =
		&types->type_declarations[source_type_id];
	struct prototype_type_declaration* generated =
		&types->type_declarations[generated_type_id];
	uint32_t empty = prototype_context_empty(contexts);
	if (generated->formation_classifier != PROTOTYPE_INVALID_ID) {
		return generated->parameter_context == empty &&
			generated->index_count == 8 ? 0 : -1;
	}
	if (source->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		source->parameter_context != empty || source->index_count != 2 ||
		source->constructor_count == 0 || generated->parameter_context != empty ||
		generated->index_context != PROTOTYPE_INVALID_ID ||
		generated->index_count != 0 || generated->constructor_count != 0) {
		return -1;
	}
	uint32_t carrier = source->origin_source_carrier_term_id;
	uint32_t bindings[8];
	uint32_t variables[8];
	uint32_t index_contexts[8];
	uint32_t current = empty;
	for (uint32_t i = 0; i < 8; ++i) {
		bindings[i] = prototype_term_new_binding(terms);
		if (bindings[i] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	uint32_t classifiers[8];
	classifiers[0] = carrier;
	classifiers[1] = carrier;
	for (uint32_t i = 0; i < 8; ++i) {
		if (i == 2 || i == 5 || i == 6 || i == 7) {
			uint32_t left_index = i == 2 ? 0 : i == 5 ? 3 : i == 6 ? 0 : 1;
			uint32_t right_index = i == 2 ? 1 : i == 5 ? 4 : i == 6 ? 3 : 4;
			uint32_t arguments[2] = {
				variables[left_index], variables[right_index]
			};
			if (prototype_term_type_instance_make(
					terms, types, source_type_id, arguments, 2, &classifiers[i]
				) != 0) {
				return -1;
			}
		} else if (i == 3 || i == 4) {
			classifiers[i] = carrier;
		}
		if (prototype_context_extend(
				contexts,
				current,
				bindings[i],
				classifiers[i],
				PROTOTYPE_INVALID_ID,
				&index_contexts[i]
			) != 0 || prototype_term_var(
				terms, bindings[i], &variables[i]
			) != 0) {
			return -1;
		}
		current = index_contexts[i];
	}
	uint32_t formation_classifier;
	if (prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			empty,
			current,
			universe,
			&formation_classifier
		) != 0) {
		return -1;
	}
	generated->index_context = current;
	generated->index_count = 8;
	generated->formation_classifier = formation_classifier;
	uint32_t source_carrier = source->origin_source_carrier_term_id;
	uint32_t original_type_id;
	const struct prototype_type_declaration* original;
	if (prototype_type_view_declaration_query(
			types,
			contexts,
			terms,
			source_carrier,
			&original_type_id,
			&original
		) != 0 || !original || original->index_count != 0 ||
		original->parameter_count != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < original->constructor_count; ++i) {
		uint32_t original_constructor_id = original->first_constructor + i;
		if (original_constructor_id >= types->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* original_constructor =
			&types->constructor_declarations[original_constructor_id];
		uint32_t original_fields[64];
		uint32_t original_field_count;
		if (prototype_context_extension_path(
				contexts,
				original_constructor->parameter_context,
				original_constructor->field_context,
				original_fields,
				64,
				&original_field_count
			) != 0) {
			return -1;
		}
		uint32_t lower_constructor_index;
		if (hott_generated_constructor_index_for_source_ordinal(
				terms,
				types,
				source_type_id,
				source_carrier,
				i,
				&lower_constructor_index
			) != 0) {
			return -1;
		}
		uint32_t lower_constructor_id = source->first_constructor +
			lower_constructor_index;
		if (lower_constructor_id >= types->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* lower_constructor =
			&types->constructor_declarations[lower_constructor_id];
		uint32_t field_context = empty;
		uint32_t field_faces[64][9];
		for (uint32_t j = 0; j < original_field_count; ++j) {
			const struct prototype_context* original_field =
				prototype_context_get(contexts, original_fields[j]);
			if (!original_field) {
				return -1;
			}
			uint32_t field_carrier =
				prototype_context_classifier_term(original_field);
			for (uint32_t k = 0; k < j; ++k) {
				const struct prototype_context* previous =
					prototype_context_get(contexts, original_fields[k]);
				if (!previous || prototype_term_contains_free_binding(
						terms, field_carrier, previous->binding_id
					)) {
					return -1;
				}
			}
			uint32_t lower_field_identity_id = source_type_id;
			uint32_t higher_field_identity_id = generated_type_id;
			if (field_carrier != source_carrier) {
				if (hott_generated_identity_declaration(
						types,
						field_carrier,
						empty,
						&lower_field_identity_id
					) != 0) {
					return -1;
				}
				uint32_t lower_field_former;
				if (prototype_term_type_instance_make(
						terms,
						types,
						lower_field_identity_id,
						NULL,
						0,
						&lower_field_former
					) != 0 || prototype_type_declaration_add_generated_identity(
						types,
						lower_field_former,
						empty,
						&higher_field_identity_id
					) != 0 || hott_initialize_indexed_higher_identity_declaration(
						terms,
						types,
						contexts,
						lower_field_identity_id,
						higher_field_identity_id,
						universe
					) != 0) {
					return -1;
				}
			}
			uint32_t field_bindings[9];
			uint32_t field_classifiers[9];
			field_classifiers[0] = field_carrier;
			field_classifiers[1] = field_carrier;
			for (uint32_t k = 0; k < 9; ++k) {
				if (k == 2 || k == 5 || k == 6 || k == 7) {
					uint32_t left = k == 2 ? 0 : k == 5 ? 3 :
						k == 6 ? 0 : 1;
					uint32_t right = k == 2 ? 1 : k == 5 ? 4 :
						k == 6 ? 3 : 4;
					uint32_t endpoints[2] = {
						field_faces[j][left], field_faces[j][right]
					};
					if (prototype_term_type_instance_make(
							terms,
							types,
							lower_field_identity_id,
							endpoints,
							2,
							&field_classifiers[k]
						) != 0) {
						return -1;
					}
				} else if (k == 3 || k == 4) {
					field_classifiers[k] = field_carrier;
				} else if (k == 8) {
					if (prototype_term_type_instance_make(
							terms,
							types,
							higher_field_identity_id,
							field_faces[j],
							8,
							&field_classifiers[k]
						) != 0) {
						return -1;
					}
				}
				field_bindings[k] = prototype_term_new_binding(terms);
				uint32_t next_context;
				if (field_bindings[k] == PROTOTYPE_INVALID_ID ||
					prototype_context_extend(
						contexts,
						field_context,
						field_bindings[k],
						field_classifiers[k],
						PROTOTYPE_INVALID_ID,
						&next_context
					) != 0 || prototype_term_var(
						terms, field_bindings[k], &field_faces[j][k]
					) != 0) {
					return -1;
				}
				field_context = next_context;
			}
		}
		uint32_t boundary[8];
		for (uint32_t k = 0; k < 8; ++k) {
			if (k == 0 || k == 1 || k == 3 || k == 4) {
				if (prototype_term_constructor(
						terms, source_carrier, i, &boundary[k]
					) != 0) {
					return -1;
				}
				uint32_t face = k == 0 ? 0 : k == 1 ? 1 :
					k == 3 ? 3 : 4;
				for (uint32_t j = 0; j < original_field_count; ++j) {
					if (prototype_term_app(
							terms,
							boundary[k],
							field_faces[j][face],
							&boundary[k]
						) != 0) {
						return -1;
					}
				}
			} else {
				if (prototype_term_constructor(
						terms,
						lower_constructor->result_classifier,
						lower_constructor_index,
						&boundary[k]
					) != 0) {
					return -1;
				}
				uint32_t left = k == 2 ? 0 : k == 5 ? 3 :
					k == 6 ? 0 : 1;
				uint32_t right = k == 2 ? 1 : k == 5 ? 4 :
					k == 6 ? 3 : 4;
				uint32_t relation = k == 2 ? 2 : k == 5 ? 5 :
					k == 6 ? 6 : 7;
				for (uint32_t j = 0; j < original_field_count; ++j) {
					uint32_t faces[3] = {
						field_faces[j][left],
						field_faces[j][right],
						field_faces[j][relation]
					};
					for (uint32_t l = 0; l < 3; ++l) {
						if (prototype_term_app(
								terms,
								boundary[k],
								faces[l],
								&boundary[k]
							) != 0) {
							return -1;
						}
					}
				}
			}
		}
		uint32_t result_classifier;
		uint32_t curried_classifier;
		if (prototype_term_type_instance_make(
				terms,
				types,
				generated_type_id,
				boundary,
				8,
				&result_classifier
			) != 0 || prototype_type_constructor_derive_curried_classifier(
				terms,
				contexts,
				empty,
				field_context,
				result_classifier,
				&curried_classifier
			) != 0) {
			return -1;
		}
		uint32_t readback_fields[576];
		uint32_t generated_field_count = original_field_count * 9;
		for (uint32_t j = 0; j < generated_field_count; ++j) {
			readback_fields[j] = PROTOTYPE_INVALID_ID;
		}
		uint32_t constructor_id;
		if (prototype_type_declaration_add_constructor(
				types,
				generated_type_id,
				-1,
				readback_fields,
				generated_field_count,
				PROTOTYPE_INVALID_ID,
				empty,
				field_context,
				result_classifier,
				curried_classifier,
				&constructor_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int hott_add_thunk_return_identity_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	uint32_t result_carrier,
	uint32_t result_identity_type_id,
	uint32_t generated_type_id,
	struct prototype_type_declaration* generated
) {
	if (!terms || !type_declarations || !contexts || !generated ||
		generated->constructor_count != 0) {
		return -1;
	}
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t relation_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t right_context;
	uint32_t field_context;
	uint32_t left_value;
	uint32_t right_value;
	uint32_t value_identity;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID ||
		relation_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			generated->parameter_context,
			left_binding,
			result_carrier,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			result_carrier,
			PROTOTYPE_INVALID_ID,
			&right_context
		) != 0 || prototype_term_var(
			terms, left_binding, &left_value
		) != 0 || prototype_term_var(
			terms, right_binding, &right_value
		) != 0) {
		return -1;
	}
	uint32_t value_identity_arguments[2] = { left_value, right_value };
	if (result_identity_type_id != PROTOTYPE_INVALID_ID) {
		if (prototype_term_type_instance_make(
				terms,
				type_declarations,
				result_identity_type_id,
				value_identity_arguments,
				2,
				&value_identity
			) != 0) {
			return -1;
		}
	} else {
		uint32_t x0_binding = prototype_term_new_binding(terms);
		uint32_t x1_binding = prototype_term_new_binding(terms);
		uint32_t xr_binding = prototype_term_new_binding(terms);
		if (x0_binding == PROTOTYPE_INVALID_ID ||
			x1_binding == PROTOTYPE_INVALID_ID ||
			xr_binding == PROTOTYPE_INVALID_ID ||
			hott_build_nondependent_pi_identity_type(
				terms,
				type_declarations,
				generated->parameter_context,
				result_carrier,
				left_binding,
				right_binding,
				x0_binding,
				x1_binding,
				xr_binding,
				&value_identity
			) != 0) {
			return -1;
		}
	}
	if (prototype_context_extend(
			contexts,
			right_context,
			relation_binding,
			value_identity,
			PROTOTYPE_INVALID_ID,
			&field_context
		) != 0) {
		return -1;
	}
	uint32_t left_return;
	uint32_t right_return;
	uint32_t left_thunk;
	uint32_t right_thunk;
	uint32_t result_classifier;
	uint32_t curried_classifier;
	if (prototype_term_return(
			terms, left_value, &left_return
		) != 0 || prototype_term_return(
			terms, right_value, &right_return
		) != 0 || prototype_term_thunk(
			terms, left_return, &left_thunk
		) != 0 || prototype_term_thunk(
			terms, right_return, &right_thunk
		) != 0) {
		return -1;
	}
	uint32_t result_arguments[2] = { left_thunk, right_thunk };
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			generated_type_id,
			result_arguments,
			2,
			&result_classifier
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			generated->parameter_context,
			field_context,
			result_classifier,
			&curried_classifier
		) != 0) {
		return -1;
	}
	uint32_t readback_fields[3] = {
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID
	};
	uint32_t constructor_id;
	if (prototype_type_declaration_add_constructor(
			type_declarations,
			generated_type_id,
			-1,
			readback_fields,
			3,
			PROTOTYPE_INVALID_ID,
			generated->parameter_context,
			field_context,
			result_classifier,
			curried_classifier,
			&constructor_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_certify_homogeneous_endpoint_contexts(
	struct prototype_kernel_builder* kernel,
	uint32_t source_type_claim_id,
	uint32_t left_context,
	uint32_t right_context,
	uint32_t* p_left_certificate_id,
	uint32_t* p_right_certificate_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_type_claim_id) : NULL;
	const struct prototype_context* left = prototype_context_get(
		contexts, left_context
	);
	const struct prototype_context* right = prototype_context_get(
		contexts, right_context
	);
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!source || !left || !right || !p_left_certificate_id ||
		!p_right_certificate_id || source->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		left->parent != source->context_id || right->parent != left_context ||
		prototype_context_classifier_term(left) != source->subject ||
		prototype_context_classifier_term(right) != source->subject) {
		return -1;
	}
	uint32_t left_projection;
	uint32_t right_type_claim;
	if (prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			left_context,
			source_type_claim_id,
			p_left_certificate_id
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			left_context,
			source->context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_type_claim_id,
			left_projection,
			&right_type_claim
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			right_context,
			right_type_claim,
			p_right_certificate_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_publish_generated_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	uint32_t generated_type_id,
	uint32_t expected_constructor_count,
	int computation_rule,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_type_declaration* generated = type_declarations &&
		generated_type_id < type_declarations->type_count ?
		&type_declarations->type_declarations[generated_type_id] : NULL;
	if (!actions || !terms || !contexts || !judgement || !source || !generated ||
		!p_result_id || generated->constructor_count != expected_constructor_count) {
		return -1;
	}
	uint32_t identity_type_former;
	uint32_t identity_type_former_claim;
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			generated_type_id,
			NULL,
			0,
			&identity_type_former
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			type_declarations,
			source->context_id,
			identity_type_former,
			generated->formation_classifier,
			&identity_type_former_claim
		) != 0) {
		return -1;
	}
	uint32_t endpoint_path[2];
	uint32_t endpoint_count;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_context_extension_path(
			contexts,
			generated->parameter_context,
			generated->index_context,
			endpoint_path,
			2,
			&endpoint_count
		) != 0 || endpoint_count != 2 || prototype_term_var(
			terms,
			prototype_context_get(contexts, endpoint_path[0])->binding_id,
			&left_endpoint
		) != 0 || prototype_term_var(
			terms,
			prototype_context_get(contexts, endpoint_path[1])->binding_id,
			&right_endpoint
		) != 0) {
		return -1;
	}
	uint32_t endpoint_arguments[2] = { left_endpoint, right_endpoint };
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			generated_type_id,
			endpoint_arguments,
			2,
			&identity_type
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			type_declarations,
			generated->index_context,
			identity_type,
			source->classifier,
			&identity_type_claim
		) != 0 || !request || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		hott_certify_homogeneous_endpoint_contexts(
			kernel,
			request->key.identity_type.source_claim_id,
			endpoint_path[0],
			generated->index_context,
			&left_context_certificate,
			&right_context_certificate
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			generated->index_context,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule = computation_rule,
			.endpoint_context_id = generated->index_context,
			.left_endpoint_binding_id =
				prototype_context_get(contexts, endpoint_path[0])->binding_id,
			.right_endpoint_binding_id =
				prototype_context_get(contexts, endpoint_path[1])->binding_id,
			.generated_type_declaration_id = generated_type_id,
			.backing_type_former_term_id = identity_type_former,
			.backing_type_former_has_type_claim_id =
				identity_type_former_claim,
				.identity_type_term_id = identity_type,
				.identity_type_has_type_claim_id = identity_type_claim,
				.identity_type_is_type_claim_id = identity_type_is_type_claim,
				.left_context_certificate_id = left_context_certificate,
				.right_context_certificate_id = right_context_certificate,
				.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
				.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
				.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_term_is_constant_over_context(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t term_id
) {
	if (!terms || !contexts || term_id >= terms->term_count) {
		return 0;
	}
	uint32_t current = context_id;
	for (uint32_t depth = 0; depth <= contexts->context_count; ++depth) {
		const struct prototype_context* context = prototype_context_get(
			contexts, current
		);
		if (!context) {
			return 0;
		}
		if (current == prototype_context_empty(contexts)) {
			return 1;
		}
		if (prototype_term_contains_free_binding(
				terms, term_id, context->binding_id
			)) {
			return 0;
		}
		current = context->parent;
	}
	return 0;
}

struct hott_identity_endpoint_setup {
	uint32_t left_type_claim_id;
	uint32_t right_type_claim_id;
	uint32_t left_type_term_id;
	uint32_t right_type_term_id;
	uint32_t left_binding_id;
	uint32_t right_binding_id;
	uint32_t left_context_id;
	uint32_t endpoint_context_id;
	uint32_t left_context_certificate_id;
	uint32_t right_context_certificate_id;
};

static int hott_prepare_identity_endpoints(
	struct prototype_kernel_builder* kernel,
	uint32_t source_type_claim_id,
	const struct prototype_hott_bridge* bridge,
	struct hott_identity_endpoint_setup* p_setup
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(
			judgement, source_type_claim_id
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!source || !bridge || !p_setup || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source->context_id != bridge->source_context_id) {
		return -1;
	}
	memset(p_setup, 0, sizeof(*p_setup));
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_type_claim_id,
			bridge->left_substitution_id,
			&p_setup->left_type_claim_id
		) != 0) {
		return -1;
	}
	uint32_t right_type_base_claim;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_type_claim_id,
			bridge->right_substitution_id,
			&right_type_base_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_type =
		prototype_judgement_claim_proposition(
			judgement, p_setup->left_type_claim_id
		);
	const struct prototype_judgement_proposition* right_type =
		prototype_judgement_claim_proposition(judgement, right_type_base_claim);
	if (!left_type || !right_type) {
		return -1;
	}
	p_setup->left_type_term_id = left_type->subject;
	p_setup->right_type_term_id = right_type->subject;
	p_setup->left_binding_id = prototype_term_new_binding(terms);
	p_setup->right_binding_id = prototype_term_new_binding(terms);
	uint32_t left_projection;
	if (p_setup->left_binding_id == PROTOTYPE_INVALID_ID ||
		p_setup->right_binding_id == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			p_setup->left_binding_id,
			p_setup->left_type_term_id,
			PROTOTYPE_INVALID_ID,
			&p_setup->left_context_id
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			p_setup->left_context_id,
			p_setup->left_type_claim_id,
			&p_setup->left_context_certificate_id
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			p_setup->left_context_id,
			bridge->bridge_context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_type_base_claim,
			left_projection,
			&p_setup->right_type_claim_id
		) != 0 || prototype_context_extend(
			contexts,
			p_setup->left_context_id,
			p_setup->right_binding_id,
			p_setup->right_type_term_id,
			PROTOTYPE_INVALID_ID,
			&p_setup->endpoint_context_id
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			p_setup->endpoint_context_id,
			p_setup->right_type_claim_id,
			&p_setup->right_context_certificate_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_publish_indexed_higher_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t source_identity_type_id,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_hott_bridge_certificate* bridge_certificate = bridge &&
		bridges && bridge->id < bridges->certificate_count ?
		&bridges->certificates[bridge->id] : NULL;
	const struct prototype_hott_bridge* parent = bridge_certificate ?
		prototype_hott_bridge_db_get(
			bridges, bridge_certificate->parent_bridge_id
		) : NULL;
	const struct prototype_type_declaration* source_identity = types &&
		source_identity_type_id < types->type_count ?
		&types->type_declarations[source_identity_type_id] : NULL;
	if (!actions || !terms || !types || !contexts || !substitutions ||
		!judgement || !source || !bridge || !bridge_certificate || !parent ||
		!source_identity || !p_result_id || bridge_certificate->semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		source_identity->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		source_identity->parameter_context != prototype_context_empty(contexts) ||
		source_identity->index_count != 2 ||
		source_identity->constructor_count == 0) {
		return -1;
	}
	uint32_t source_type_id;
	uint32_t source_arguments[2];
	uint32_t source_argument_count;
	uint32_t source_former;
	if (prototype_term_type_instance_info(
			terms,
			source->subject,
			&source_type_id,
			source_arguments,
			&source_argument_count
		) != 0 || source_type_id != source_identity_type_id ||
		source_argument_count != 2 || prototype_term_type_instance_make(
			terms,
			types,
			source_identity_type_id,
			NULL,
			0,
			&source_former
		) != 0) {
		return -1;
	}
	uint32_t generated_type_id;
	uint32_t empty = prototype_context_empty(contexts);
	if (prototype_type_declaration_add_generated_identity(
			types, source_former, empty, &generated_type_id
		) != 0 || hott_initialize_indexed_higher_identity_declaration(
			terms,
			types,
			contexts,
			source_identity_type_id,
			generated_type_id,
			source->classifier
		) != 0) {
		return -1;
	}
	struct hott_identity_endpoint_setup endpoints;
	if (hott_prepare_identity_endpoints(
			kernel,
			prototype_hott_action_request_get(actions, request_id)->key.
				identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		return -1;
	}
	uint32_t boundary[8];
	if (prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_arguments[0],
			bridge->left_substitution_id,
			&boundary[0]
		) != 0 || prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_arguments[1],
			bridge->left_substitution_id,
			&boundary[1]
		) != 0 || prototype_term_var(
			terms, endpoints.left_binding_id, &boundary[2]
		) != 0 || prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_arguments[0],
			bridge->right_substitution_id,
			&boundary[3]
		) != 0 || prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_arguments[1],
			bridge->right_substitution_id,
			&boundary[4]
		) != 0 || prototype_term_var(
			terms, endpoints.right_binding_id, &boundary[5]
		) != 0 || prototype_term_var(
			terms,
			prototype_context_get(contexts, parent->bridge_context_id)->binding_id,
			&boundary[6]
		) != 0 || prototype_term_var(
			terms,
			prototype_context_get(contexts, bridge->bridge_context_id)->binding_id,
			&boundary[7]
		) != 0) {
		return -1;
	}
	const struct prototype_type_declaration* generated =
		&types->type_declarations[generated_type_id];
	uint32_t higher_former;
	uint32_t higher_former_claim;
	uint32_t higher_identity;
	uint32_t higher_identity_claim;
	uint32_t higher_identity_is_type_claim;
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, NULL, 0, &higher_former
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			bridge->bridge_context_id,
			higher_former,
			generated->formation_classifier,
			&higher_former_claim
		) != 0 || prototype_term_type_instance_make(
			terms, types, generated_type_id, boundary, 8, &higher_identity
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			endpoints.endpoint_context_id,
			higher_identity,
			source->classifier,
			&higher_identity_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			higher_identity,
			source->classifier,
			higher_identity_claim,
			&higher_identity_is_type_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule =
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT,
			.endpoint_context_id = endpoints.endpoint_context_id,
			.left_endpoint_binding_id = endpoints.left_binding_id,
			.right_endpoint_binding_id = endpoints.right_binding_id,
			.generated_type_declaration_id = generated_type_id,
			.backing_type_former_term_id = higher_former,
			.backing_type_former_has_type_claim_id = higher_former_claim,
			.identity_type_term_id = higher_identity,
			.identity_type_has_type_claim_id = higher_identity_claim,
			.identity_type_is_type_claim_id = higher_identity_is_type_claim,
			.left_context_certificate_id =
				endpoints.left_context_certificate_id,
			.right_context_certificate_id =
				endpoints.right_context_certificate_id,
			.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_publish_constant_generated_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t generated_type_id,
	int computation_rule,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	const struct prototype_type_declaration* generated = types &&
		generated_type_id < types->type_count ?
		&types->type_declarations[generated_type_id] : NULL;
	if (!actions || !terms || !types || !contexts || !substitutions || !cwf ||
		!judgement || !request || !source || !bridge || !generated ||
		!p_result_id || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		bridge->source_context_id != source->context_id ||
		bridge->source_context_id == prototype_context_empty(contexts) ||
		bridges->certificates[bridge->id].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
		!hott_term_is_constant_over_context(
			terms, contexts, source->context_id, source->subject
		) || generated->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		generated->origin_source_carrier_term_id != source->subject ||
		generated->parameter_count != 0 || generated->index_count != 2 ||
		(computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT &&
		 computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE)) {
		return -1;
	}
	uint32_t left_type_claim;
	uint32_t right_type_base_claim;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			request->key.identity_type.source_claim_id,
			bridge->left_substitution_id,
			&left_type_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			request->key.identity_type.source_claim_id,
			bridge->right_substitution_id,
			&right_type_base_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_type =
		prototype_judgement_claim_proposition(judgement, left_type_claim);
	const struct prototype_judgement_proposition* right_type_base =
		prototype_judgement_claim_proposition(judgement, right_type_base_claim);
	if (!left_type || !right_type_base || left_type->subject != source->subject ||
		right_type_base->subject != source->subject) {
		return -1;
	}
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t endpoint_context;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	uint32_t left_projection;
	uint32_t right_type_claim;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			left_binding,
			left_type->subject,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			left_context,
			left_type_claim,
			&left_context_certificate
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			left_context,
			bridge->bridge_context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_type_base_claim,
			left_projection,
			&right_type_claim
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			source->subject,
			PROTOTYPE_INVALID_ID,
			&endpoint_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			endpoint_context,
			right_type_claim,
			&right_context_certificate
		) != 0) {
		return -1;
	}
	uint32_t former;
	uint32_t former_claim;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	uint32_t endpoints[2];
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, NULL, 0, &former
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			bridge->bridge_context_id,
			former,
			generated->formation_classifier,
			&former_claim
		) != 0 || prototype_term_var(
			terms, left_binding, &left_endpoint
		) != 0 || prototype_term_var(
			terms, right_binding, &right_endpoint
		) != 0) {
		return -1;
	}
	endpoints[0] = left_endpoint;
	endpoints[1] = right_endpoint;
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, endpoints, 2, &identity_type
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			endpoint_context,
			identity_type,
			source->classifier,
			&identity_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoint_context,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule = computation_rule,
			.endpoint_context_id = endpoint_context,
			.left_endpoint_binding_id = left_binding,
			.right_endpoint_binding_id = right_binding,
			.generated_type_declaration_id = generated_type_id,
			.backing_type_former_term_id = former,
			.backing_type_former_has_type_claim_id = former_claim,
			.identity_type_term_id = identity_type,
			.identity_type_has_type_claim_id = identity_type_claim,
			.identity_type_is_type_claim_id = identity_type_is_type_claim,
			.left_context_certificate_id = left_context_certificate,
			.right_context_certificate_id = right_context_certificate,
			.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

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
) {
	if (!terms || !type_declarations || !p_identity_type ||
		source_carrier >= terms->term_count) {
		return -1;
	}
	int is_computation_function = terms->terms[source_carrier].tag ==
		PROTOTYPE_TERM_THUNK_TYPE;
	uint32_t pi = is_computation_function ?
		terms->terms[source_carrier].as.thunk_type.computation : source_carrier;
	if (pi >= terms->term_count || terms->terms[pi].tag != PROTOTYPE_TERM_PI) {
		return -1;
	}
	uint32_t domain = terms->terms[pi].as.pi.domain;
	uint32_t codomain_binding;
	uint32_t codomain;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) != 0 || codomain >= terms->term_count ||
		prototype_term_contains_free_binding(terms, codomain, codomain_binding) ||
		(is_computation_function ?
			(terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
			 prototype_term_effect_row_purity(
				terms, terms->terms[codomain].as.computation_type.label
			 ) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) :
			terms->terms[codomain].tag != PROTOTYPE_TERM_UNIVERSE_VAR)) {
		return -1;
	}
	uint32_t domain_identity_type_id;
	uint32_t codomain_thunk;
	uint32_t codomain_identity_type_id;
	if (hott_generated_identity_declaration(
			type_declarations,
			domain,
			parameter_context,
			&domain_identity_type_id
		) != 0) {
		return -1;
	}
	if (is_computation_function) {
		if (prototype_term_thunk_type(terms, codomain, &codomain_thunk) != 0) {
			return -1;
		}
	} else {
		codomain_thunk = codomain;
	}
	if (hott_generated_identity_declaration(
			type_declarations,
			codomain_thunk,
			parameter_context,
			&codomain_identity_type_id
		) != 0) {
		return -1;
	}
	uint32_t u0;
	uint32_t u1;
	uint32_t x0;
	uint32_t x1;
	uint32_t domain_identity;
	if (x0_binding == PROTOTYPE_INVALID_ID || x1_binding == PROTOTYPE_INVALID_ID ||
		xr_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, left_endpoint_binding, &u0
		) != 0 || prototype_term_var(
			terms, right_endpoint_binding, &u1
		) != 0 || prototype_term_var(
			terms, x0_binding, &x0
		) != 0 || prototype_term_var(
			terms, x1_binding, &x1
		) != 0) {
		return -1;
	}
	uint32_t domain_identity_arguments[2] = { x0, x1 };
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			domain_identity_type_id,
			domain_identity_arguments,
			2,
			&domain_identity
		) != 0) {
		return -1;
	}
	uint32_t forced_u0;
	uint32_t forced_u1;
	uint32_t applied_u0;
	uint32_t applied_u1;
	uint32_t thunked_applied_u0;
	uint32_t thunked_applied_u1;
	uint32_t result_identity;
	if (is_computation_function) {
		if (prototype_term_force(terms, u0, &forced_u0) != 0 ||
			prototype_term_force(terms, u1, &forced_u1) != 0) {
			return -1;
		}
	} else {
		forced_u0 = u0;
		forced_u1 = u1;
	}
	if (prototype_term_app(
			terms, forced_u0, x0, &applied_u0
		) != 0 || prototype_term_app(
			terms, forced_u1, x1, &applied_u1
		) != 0) {
		return -1;
	}
	if (is_computation_function) {
		if (prototype_term_thunk(terms, applied_u0, &thunked_applied_u0) != 0 ||
			prototype_term_thunk(terms, applied_u1, &thunked_applied_u1) != 0) {
			return -1;
		}
	} else {
		/* A type-family application is pure type computation. Normalize its
		 * endpoint before materializing the Universe identity TypeView so the
		 * typed source and its erased core describe the same fiber. */
		if (prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				applied_u0,
				&thunked_applied_u0
			) != 0 || prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				applied_u1,
				&thunked_applied_u1
			) != 0) {
			return -1;
		}
	}
	uint32_t result_identity_arguments[2] = {
		thunked_applied_u0, thunked_applied_u1
	};
	if (prototype_term_type_instance_make(
			terms,
			type_declarations,
			codomain_identity_type_id,
			result_identity_arguments,
			2,
			&result_identity
		) != 0) {
		return -1;
	}
	uint32_t classifier = result_identity;
	uint32_t binders[3] = { x0_binding, x1_binding, xr_binding };
	uint32_t domains[3] = { domain, domain, domain_identity };
	for (uint32_t i = 3; i > 0; --i) {
		uint32_t family;
		uint32_t pi_classifier;
		uint32_t family_body = classifier;
		if (is_computation_function) {
			uint32_t empty_effects;
			if (prototype_term_effect_label(
					terms,
					PROTOTYPE_EFFECT_OPERATION_LABEL_NONE,
					&empty_effects
				) != 0 || prototype_term_computation_type(
					terms, empty_effects, classifier, &family_body
				) != 0) {
				return -1;
			}
		}
		if (prototype_term_pure_family(
				terms, binders[i - 1], family_body, &family
			) != 0 || prototype_term_pi_family(
				terms, domains[i - 1], family, &pi_classifier
			) != 0 || (is_computation_function && prototype_term_thunk_type(
				terms, pi_classifier, &classifier
			) != 0)) {
			return -1;
		}
		if (!is_computation_function) {
			classifier = pi_classifier;
		}
	}
	*p_identity_type = classifier;
	return 0;
}

static int hott_publish_pi_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	int computation_rule,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	if (!actions || !terms || !contexts || !judgement || !source || !bridge ||
		!p_result_id || bridge->source_context_id != source->context_id ||
		(computation_rule != PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE &&
		 computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT)) {
		return -1;
	}
	if (computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT &&
		(source->context_id == prototype_context_empty(contexts) ||
		 !hott_term_is_constant_over_context(
			terms, contexts, source->context_id, source->subject
		 ))) {
		return -1;
	}
	struct hott_identity_endpoint_setup endpoints;
	if (hott_prepare_identity_endpoints(
			kernel,
			prototype_hott_action_request_get(actions, request_id)->key.
				identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		return -1;
	}
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	uint32_t x0_binding = prototype_term_new_binding(terms);
	uint32_t x1_binding = prototype_term_new_binding(terms);
	uint32_t xr_binding = prototype_term_new_binding(terms);
	if (x0_binding == PROTOTYPE_INVALID_ID || x1_binding == PROTOTYPE_INVALID_ID ||
		xr_binding == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (hott_build_nondependent_pi_identity_type(
			terms,
			kernel->type_declarations,
			prototype_context_empty(contexts),
			source->subject,
			endpoints.left_binding_id,
			endpoints.right_binding_id,
			x0_binding,
			x1_binding,
			xr_binding,
			&identity_type
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			kernel->type_declarations,
			contexts,
			kernel->substitutions,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			&identity_type_claim
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule = computation_rule,
			.endpoint_context_id = endpoints.endpoint_context_id,
			.left_endpoint_binding_id = endpoints.left_binding_id,
			.right_endpoint_binding_id = endpoints.right_binding_id,
			.generated_type_declaration_id = PROTOTYPE_INVALID_ID,
			.backing_type_former_term_id = PROTOTYPE_INVALID_ID,
			.backing_type_former_has_type_claim_id = PROTOTYPE_INVALID_ID,
			.identity_type_term_id = identity_type,
			.identity_type_has_type_claim_id = identity_type_claim,
			.identity_type_is_type_claim_id = identity_type_is_type_claim,
			.left_context_certificate_id =
				endpoints.left_context_certificate_id,
			.right_context_certificate_id =
				endpoints.right_context_certificate_id,
			.pointwise_left_input_binding_id = x0_binding,
			.pointwise_right_input_binding_id = x1_binding,
			.pointwise_input_identity_binding_id = xr_binding
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_add_contextual_adt_identity_constructor(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	uint32_t constructor_ordinal,
	uint32_t generated_type_id,
	struct prototype_type_declaration* generated,
	uint32_t* p_constructor_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !source_type || !generated || !p_constructor_id ||
		constructor_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + constructor_ordinal >=
			types->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* source_constructor =
		&types->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	uint32_t bridge_id;
	int residual_reason;
	int bridge_status = prototype_hott_bridge_db_ensure_identity_context(
		bridges,
		kernel,
		actions,
		source_constructor->field_context,
		&bridge_id,
		&residual_reason
	);
	if (bridge_status != 0) {
		return bridge_status > 0 ? 1 : -1;
	}
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, bridge_id
	);
	if (!bridge || bridge->source_context_id !=
			source_constructor->field_context || bridge->bridge_context_id >=
			contexts->context_count) {
		return -1;
	}

	uint32_t source_fields[64];
	uint32_t source_field_count;
	if (prototype_context_extension_path(
			contexts,
			source_constructor->parameter_context,
			source_constructor->field_context,
			source_fields,
			64,
			&source_field_count
		) != 0) {
		return -1;
	}
	uint32_t source_endpoint;
	if (prototype_term_constructor(
			terms, source_carrier, constructor_ordinal, &source_endpoint
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_field_count; ++i) {
		const struct prototype_context* field = prototype_context_get(
			contexts, source_fields[i]
		);
		uint32_t field_term;
		if (!field || prototype_term_var(
				terms, field->binding_id, &field_term
			) != 0 || prototype_term_app(
				terms, source_endpoint, field_term, &source_endpoint
			) != 0) {
			return -1;
		}
	}
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_endpoint,
			bridge->left_substitution_id,
			&left_endpoint
		) != 0 || prototype_term_reindex(
			terms,
			types,
			contexts,
			substitutions,
			source_endpoint,
			bridge->right_substitution_id,
			&right_endpoint
		) != 0) {
		return -1;
	}
	uint32_t result_arguments[2] = { left_endpoint, right_endpoint };
	uint32_t result_classifier;
	uint32_t curried_classifier;
	if (prototype_term_type_instance_make(
			terms,
			types,
			generated_type_id,
			result_arguments,
			2,
			&result_classifier
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			generated->parameter_context,
			bridge->bridge_context_id,
			result_classifier,
			&curried_classifier
		) != 0) {
		return -1;
	}
	uint32_t generated_fields[192];
	uint32_t generated_field_count;
	if (prototype_context_extension_path(
			contexts,
			generated->parameter_context,
			bridge->bridge_context_id,
			generated_fields,
			192,
			&generated_field_count
		) != 0) {
		return -1;
	}
	uint32_t readback_fields[192];
	for (uint32_t i = 0; i < generated_field_count; ++i) {
		readback_fields[i] = PROTOTYPE_INVALID_ID;
	}
	return prototype_type_declaration_add_constructor(
		types,
		generated_type_id,
		-1,
		readback_fields,
		generated_field_count,
		PROTOTYPE_INVALID_ID,
		generated->parameter_context,
		bridge->bridge_context_id,
		result_classifier,
		curried_classifier,
		p_constructor_id
	);
}

static int hott_add_ordinary_adt_identity_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	uint32_t source_carrier,
	const struct prototype_type_declaration* source_type,
	uint32_t constructor_ordinal,
	uint32_t generated_type_id,
	struct prototype_type_declaration* generated,
	uint32_t* p_constructor_id
) {
	if (!terms || !type_declarations || !contexts || !source_type || !generated ||
		!p_constructor_id || constructor_ordinal >= source_type->constructor_count ||
		source_type->first_constructor + constructor_ordinal >=
			type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* source_constructor =
		&type_declarations->constructor_declarations[
			source_type->first_constructor + constructor_ordinal
		];
	uint32_t source_fields[64];
	uint32_t source_field_count;
	if (prototype_context_extension_path(
			contexts,
			source_constructor->parameter_context,
			source_constructor->field_context,
			source_fields,
			64,
			&source_field_count
		) != 0) {
		return -1;
	}
	uint32_t generated_field_context = generated->parameter_context;
	uint32_t left_fields[64];
	uint32_t right_fields[64];
	for (uint32_t i = 0; i < source_field_count; ++i) {
		const struct prototype_context* source_field =
			prototype_context_get(contexts, source_fields[i]);
		if (!source_field) {
			return -1;
		}
		uint32_t field_classifier =
			prototype_context_classifier_term(source_field);
		for (uint32_t j = 0; j < i; ++j) {
			const struct prototype_context* previous =
				prototype_context_get(contexts, source_fields[j]);
			if (!previous || prototype_term_contains_free_binding(
					terms, field_classifier, previous->binding_id
				)) {
				return 1;
			}
		}
		uint32_t field_identity_type_id = generated_type_id;
		if (field_classifier != source_carrier &&
			hott_generated_identity_declaration(
				type_declarations,
				field_classifier,
				generated->parameter_context,
				&field_identity_type_id
			) != 0) {
			return 1;
		}
		uint32_t left_binding = prototype_term_new_binding(terms);
		uint32_t right_binding = prototype_term_new_binding(terms);
		uint32_t relation_binding = prototype_term_new_binding(terms);
		uint32_t left_context;
		uint32_t right_context;
		uint32_t relation_context;
		uint32_t relation_type;
		if (left_binding == PROTOTYPE_INVALID_ID ||
			right_binding == PROTOTYPE_INVALID_ID ||
			relation_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
				contexts, generated_field_context, left_binding, field_classifier,
				PROTOTYPE_INVALID_ID, &left_context
			) != 0 || prototype_context_extend(
				contexts, left_context, right_binding, field_classifier,
				PROTOTYPE_INVALID_ID, &right_context
			) != 0 || prototype_term_var(
				terms, left_binding, &left_fields[i]
			) != 0 || prototype_term_var(
				terms, right_binding, &right_fields[i]
			) != 0) {
			return -1;
		}
		uint32_t relation_arguments[2] = {
			left_fields[i], right_fields[i]
		};
		if (prototype_term_type_instance_make(
				terms, type_declarations, field_identity_type_id,
				relation_arguments, 2, &relation_type
			) != 0 || prototype_context_extend(
				contexts, right_context, relation_binding, relation_type,
				PROTOTYPE_INVALID_ID, &relation_context
			) != 0) {
			return -1;
		}
		generated_field_context = relation_context;
	}
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_term_constructor(
			terms, source_carrier, constructor_ordinal, &left_endpoint
		) != 0 || prototype_term_constructor(
			terms, source_carrier, constructor_ordinal, &right_endpoint
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_field_count; ++i) {
		if (prototype_term_app(
				terms, left_endpoint, left_fields[i], &left_endpoint
			) != 0 || prototype_term_app(
				terms, right_endpoint, right_fields[i], &right_endpoint
			) != 0) {
			return -1;
		}
	}
	uint32_t result_arguments[2] = { left_endpoint, right_endpoint };
	uint32_t result_classifier;
	uint32_t curried_classifier;
	if (prototype_term_type_instance_make(
			terms, type_declarations, generated_type_id, result_arguments, 2,
			&result_classifier
		) != 0 || prototype_type_constructor_derive_curried_classifier(
			terms, contexts, generated->parameter_context, generated_field_context,
			result_classifier, &curried_classifier
		) != 0) {
		return -1;
	}
	uint32_t readback_fields[192];
	uint32_t generated_field_count = source_field_count * 3;
	for (uint32_t i = 0; i < generated_field_count; ++i) {
		readback_fields[i] = PROTOTYPE_INVALID_ID;
	}
	return prototype_type_declaration_add_constructor(
		type_declarations, generated_type_id, -1, readback_fields,
		generated_field_count, PROTOTYPE_INVALID_ID,
		generated->parameter_context, generated_field_context, result_classifier,
		curried_classifier, p_constructor_id
	);
}

int prototype_hott_execute_relation_type_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	int existing = hott_action_result_for_request(
		actions, request_id, p_result_id
	);
	if (existing <= 0) {
		return existing;
	}
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	const struct prototype_judgement_claim* source = request &&
		request->kind == PROTOTYPE_HOTT_ACTION_RELATION_TYPE ?
		prototype_judgement_claim_get(
			judgement, request->key.relation_type.source_claim_id
		) : NULL;
	const struct prototype_hott_bridge* bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.relation_type.source_bridge_id
		) : NULL;
	if (!source || !bridge || prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id != bridge->source_context_id) {
		return -1;
	}
	struct prototype_hott_type_former_descriptor descriptor;
	if (prototype_hott_type_former_descriptor_query(
			terms,
			type_declarations,
			contexts,
			judgement,
			request->key.relation_type.source_claim_id,
			&descriptor
		) != 0) {
		return -1;
	}
	if (!descriptor.admitted) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			descriptor.residual_reason,
			p_result_id
		);
	}

	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t endpoint_context;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	uint32_t left_projection;
	uint32_t endpoint_to_left;
	uint32_t endpoint_projection;
	uint32_t left_type_claim;
	uint32_t right_type_base_claim;
	uint32_t right_type_claim;
	uint32_t endpoint_left_type_claim;
	uint32_t endpoint_type_claim;
	uint32_t left_endpoint_claim;
	uint32_t right_endpoint_claim;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t relation_type;
	uint32_t relation_family;
	uint32_t relation_claim;
	uint32_t left_type;
	uint32_t right_type;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.relation_type.source_claim_id,
			bridge->left_substitution_id,
			&left_type_claim
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.relation_type.source_claim_id,
			bridge->right_substitution_id,
			&right_type_base_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* left_type_evidence =
		prototype_judgement_claim_get(judgement, left_type_claim);
	if (!left_type_evidence) {
		return -1;
	}
	left_type = prototype_judgement_proposition_get(judgement, left_type_evidence->proposition_id)->subject;
	if (
		prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			left_binding,
			left_type,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 ||
		prototype_cwf_certificate_db_add_context(
			cwf_certificates,
			contexts,
			terms,
			type_declarations,
			judgement,
			left_context,
			left_type_claim,
			&left_context_certificate
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, left_context, &left_projection
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			right_type_base_claim,
			left_projection,
			&right_type_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			left_context,
			left_binding,
			left_type,
			&left_endpoint_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* right_type_evidence =
		prototype_judgement_claim_get(judgement, right_type_claim);
	if (!right_type_evidence) {
		return -1;
	}
	right_type = prototype_judgement_proposition_get(judgement, right_type_evidence->proposition_id)->subject;
	if (
		prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			right_type,
			PROTOTYPE_INVALID_ID,
			&endpoint_context
		) != 0 ||
		prototype_cwf_certificate_db_add_context(
			cwf_certificates,
			contexts,
			terms,
			type_declarations,
			judgement,
			endpoint_context,
			right_type_claim,
			&right_context_certificate
		) != 0 ||
		prototype_substitution_projection_path(
			substitutions,
			contexts,
			endpoint_context,
			bridge->bridge_context_id,
			&endpoint_projection
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, endpoint_context, &endpoint_to_left
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			left_type_claim,
			endpoint_projection,
			&endpoint_left_type_claim
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			right_type_claim,
			endpoint_to_left,
			&endpoint_type_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoint_context,
			left_binding,
			left_type,
			&left_endpoint_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoint_context,
			right_binding,
			right_type,
			&right_endpoint_claim
		) != 0 ||
		prototype_term_var(terms, left_binding, &left_endpoint) != 0 ||
		prototype_term_var(terms, right_binding, &right_endpoint) != 0 ||
		prototype_term_relation_type(
			terms,
			left_type,
			right_type,
			left_endpoint,
			right_endpoint,
			&relation_type
		) != 0 ||
		prototype_judgement_add_relation_type_formation(
			judgement,
			terms,
			endpoint_context,
			relation_type,
			prototype_judgement_claim_proposition(
				judgement, endpoint_left_type_claim
			)->classifier,
			endpoint_left_type_claim,
			endpoint_type_claim,
			left_endpoint_claim,
			right_endpoint_claim,
			&relation_claim
		) != 0 || prototype_term_pure_family(
			terms,
			right_binding,
			relation_type,
			&relation_family
		) != 0 || prototype_term_pure_family(
			terms,
			left_binding,
			relation_family,
			&relation_family
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE,
		.data.relation_type = {
			.endpoint_context_id = endpoint_context,
			.left_endpoint_binding_id = left_binding,
			.right_endpoint_binding_id = right_binding,
			.relation_family_semantics =
				PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION,
			.relation_family_term_id = relation_family,
			.relation_type_term_id = relation_type,
			.relation_is_type_claim_id = relation_claim,
			.left_context_certificate_id = left_context_certificate,
			.right_context_certificate_id = right_context_certificate
		}
	};
	uint32_t certificate_id;
	if (prototype_hott_action_certificate_add(
			actions, &view, bridges,
			certificate,
			&certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static const struct prototype_judgement_derivation*
hott_derivation_for_claim_and_kind(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	int proof_kind
);

static int hott_has_type_premise_for_is_type_claim_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t is_type_claim_id,
	uint32_t remaining_depth,
	uint32_t* p_has_type_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* is_type = judgement ?
		prototype_judgement_claim_proposition(judgement, is_type_claim_id) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!is_type || !p_has_type_claim_id || remaining_depth == 0 ||
		is_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) {
		return -1;
	}
	uint32_t premise_claim_id;
	if (hott_find_unique_derivation_premise(
			judgement,
			is_type_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE,
			is_type->subject,
			is_type->classifier,
			&premise_claim_id
		) == 0) {
		*p_has_type_claim_id = premise_claim_id;
		return 0;
	}
	const struct prototype_judgement_derivation* reindex =
		hott_derivation_for_claim_and_kind(
			judgement,
			is_type_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
		);
	if (!reindex || reindex->premise_count != 1 ||
		reindex->semantic_action_kind !=
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
		reindex->semantic_action_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t source_has_type_claim_id;
	int source_status = hott_has_type_premise_for_is_type_claim_depth(
		kernel,
		reindex->premises[0].claim_id,
		remaining_depth - 1,
		&source_has_type_claim_id
	);
	if (source_status != 0) {
		return source_status;
	}
	uint32_t reindexed_has_type_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_has_type_claim_id,
			reindex->semantic_action_id,
			&reindexed_has_type_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* reindexed =
		prototype_judgement_claim_proposition(
			judgement, reindexed_has_type_claim_id
		);
	if (!reindexed || reindexed->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		reindexed->context_id != is_type->context_id ||
		reindexed->subject != is_type->subject ||
		reindexed->classifier != is_type->classifier) {
		return -1;
	}
	*p_has_type_claim_id = reindexed_has_type_claim_id;
	return 0;
}

static int hott_construct_type_expression_universe_witness(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t source_is_type_claim_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t* p_universe_identity_result_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	uint32_t source_has_type_claim_id;
	uint32_t universe_is_type_claim_id;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !judgement || !source || !bridge ||
		!p_universe_identity_result_id || !p_witness_claim_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source->classifier >=
			terms->term_count || terms->terms[source->classifier].tag !=
			PROTOTYPE_TERM_UNIVERSE_VAR) {
		return 1;
	}
	int has_type_status = hott_has_type_premise_for_is_type_claim_depth(
			kernel,
			source_is_type_claim_id,
			(uint32_t)judgement->claim_count + 1,
			&source_has_type_claim_id
		);
	if (has_type_status != 0) {
		fprintf(
			stderr,
			"universe fiber missing HAS_TYPE premise source_claim=%u status=%d\n",
			source_is_type_claim_id,
			has_type_status
		);
		return has_type_status;
	}
	int universe_status = hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			source->classifier,
			&universe_is_type_claim_id
		);
	if (universe_status != 0) {
		fprintf(
			stderr,
			"universe fiber cannot type Universe source_claim=%u status=%d\n",
			source_is_type_claim_id,
			universe_status
		);
		return universe_status;
	}
	struct prototype_hott_action_request identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = universe_is_type_claim_id,
			.source_bridge_id = bridge->id
		}
	};
	struct prototype_kernel_view view;
	uint32_t identity_request_id;
	uint32_t identity_result_id;
	int view_status = prototype_kernel_builder_view(kernel, &view);
	int identity_intern_status = view_status == 0 ?
		prototype_hott_action_request_intern(
			actions, &view, bridges, identity_request, &identity_request_id
		) : -1;
	int identity_execute_status = identity_intern_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			actions,
			kernel,
			bridges,
			identity_request_id,
			&identity_result_id
		) : -1;
	if (view_status != 0 || identity_intern_status != 0 ||
		identity_execute_status != 0) {
		fprintf(
			stderr,
			"universe fiber Universe identity failed source_claim=%u "
			"has_type_claim=%u universe_claim=%u view=%d intern=%d execute=%d\n",
			source_is_type_claim_id,
			source_has_type_claim_id,
			universe_is_type_claim_id,
			view_status,
			identity_intern_status,
			identity_execute_status
		);
		return -1;
	}
	const struct prototype_hott_action_result* identity_result =
		prototype_hott_action_result_get(actions, identity_result_id);
	if (!identity_result || identity_result->outcome.state !=
			PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		fprintf(
			stderr,
			"universe fiber Universe identity not ready source_claim=%u "
			"request=%u result=%u state=%d residual=%d\n",
			source_is_type_claim_id,
			identity_request_id,
			identity_result_id,
			identity_result ? identity_result->outcome.state : -1,
			identity_result ? identity_result->outcome.residual_reason : -1
		);
		return identity_result ? 1 : -1;
	}
	struct prototype_hott_action_request object_request = {
		.kind = PROTOTYPE_HOTT_ACTION_OBJECT_TERM,
		.key.object_term = {
			.source_claim_id = source_has_type_claim_id,
			.source_bridge_id = bridge->id,
			.identity_type_action_request_id = identity_request_id
		}
	};
	uint32_t object_request_id;
	uint32_t object_result_id;
	int object_intern_status = prototype_hott_action_request_intern(
			actions, &view, bridges, object_request, &object_request_id
		);
	int object_execute_status = object_intern_status == 0 ?
		prototype_hott_execute_object_term_action(
			actions,
			kernel,
			bridges,
			object_request_id,
			&object_result_id
		) : -1;
	if (object_intern_status != 0 || object_execute_status != 0) {
		fprintf(
			stderr,
			"universe fiber object action failed source_claim=%u "
			"has_type_claim=%u bridge=%u identity_request=%u intern=%d execute=%d\n",
			source_is_type_claim_id,
			source_has_type_claim_id,
			bridge->id,
			identity_request_id,
			object_intern_status,
			object_execute_status
		);
		return -1;
	}
	const struct prototype_hott_action_result* object_result =
		prototype_hott_action_result_get(actions, object_result_id);
	const struct prototype_hott_action_certificate* object_certificate =
		object_result && object_result->outcome.state ==
			PROTOTYPE_HOTT_ACTION_RESULT_READY && object_result->certificate_id <
			actions->certificate_count ?
		&actions->certificates[object_result->certificate_id] : NULL;
	if (!object_certificate || object_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM) {
		return object_result ? 1 : -1;
	}
	*p_universe_identity_result_id = identity_result_id;
	*p_witness_claim_id =
		object_certificate->data.object_term.witness_has_type_claim_id;
	return 0;
}

static int hott_publish_universe_fiber_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context* source_context = contexts && source ?
		prototype_context_get(contexts, source->context_id) : NULL;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	if (!actions || !terms || !types || !contexts || !substitutions || !judgement ||
		!source || !bridge || !source_context || !bridges || !request ||
		!p_result_id || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source->subject >= terms->term_count ||
		terms->terms[source->subject].tag == PROTOTYPE_TERM_UNIVERSE_VAR ||
		source->classifier >= terms->term_count ||
		terms->terms[source->classifier].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return 1;
	}
	uint32_t universe_result_id;
	uint32_t universe_witness_claim_id;
	int universe_action_status = hott_construct_type_expression_universe_witness(
		actions,
		kernel,
		bridges,
		request->key.identity_type.source_claim_id,
		source,
		bridge,
		&universe_result_id,
		&universe_witness_claim_id
	);
	if (universe_action_status != 0) {
		return universe_action_status;
	}
	uint32_t relation;
	uint32_t relation_claim;
	int relation_projection_status =
		prototype_hott_construct_universe_correspondence_projection(
			actions,
			kernel,
			bridges,
			universe_result_id,
			universe_witness_claim_id,
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION,
			&relation,
			&relation_claim
		);
	if (relation_projection_status != 0) {
		fprintf(
			stderr,
			"universe fiber relation projection failed request=%u result=%u "
			"witness_claim=%u status=%d\n",
			request_id,
			universe_result_id,
			universe_witness_claim_id,
			relation_projection_status
		);
		return -1;
	}
	struct hott_identity_endpoint_setup endpoints;
	if (hott_prepare_identity_endpoints(
			kernel,
			request->key.identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		fprintf(stderr, "universe fiber endpoint preparation failed request=%u\n", request_id);
		return -1;
	}
	uint32_t endpoint_projection;
	uint32_t relation_in_endpoints;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t left_endpoint_claim;
	uint32_t right_endpoint_claim;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			endpoints.endpoint_context_id,
			bridge->bridge_context_id,
			&endpoint_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			relation_claim,
			endpoint_projection,
			&relation_in_endpoints
		) != 0 || prototype_term_var(
			terms, endpoints.left_binding_id, &left_endpoint
		) != 0 || prototype_term_var(
			terms, endpoints.right_binding_id, &right_endpoint
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoints.endpoint_context_id,
			endpoints.left_binding_id,
			endpoints.left_type_term_id,
			&left_endpoint_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoints.endpoint_context_id,
			endpoints.right_binding_id,
			endpoints.right_type_term_id,
			&right_endpoint_claim
		) != 0) {
		fprintf(
			stderr,
			"universe fiber endpoint setup failed request=%u endpoint_context=%u "
			"bridge_context=%u relation_claim=%u\n",
			request_id,
			endpoints.endpoint_context_id,
			bridge->bridge_context_id,
			relation_claim
		);
		return -1;
	}
	uint32_t relation_at_left;
	uint32_t relation_at_left_claim;
	uint32_t right_relation_classifier;
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	if (prototype_term_pi(
			terms,
			endpoints.right_type_term_id,
			source->classifier,
			&right_relation_classifier
		) != 0 || prototype_term_app(
			terms, relation, left_endpoint, &relation_at_left
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			endpoints.endpoint_context_id,
			relation_at_left,
			right_relation_classifier,
			relation_in_endpoints,
			left_endpoint_claim,
			&relation_at_left_claim
		) != 0 || prototype_term_app(
			terms, relation_at_left, right_endpoint, &identity_type
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			relation_at_left_claim,
			right_endpoint_claim,
			&identity_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		fprintf(
			stderr,
			"universe fiber relation application failed request=%u relation=%u "
			"relation_claim=%u left_type=%u right_type=%u left_claim=%u "
			"right_claim=%u\n",
			request_id,
			relation,
			relation_in_endpoints,
			endpoints.left_type_term_id,
			endpoints.right_type_term_id,
			left_endpoint_claim,
			right_endpoint_claim
		);
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule =
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER,
			.endpoint_context_id = endpoints.endpoint_context_id,
			.left_endpoint_binding_id = endpoints.left_binding_id,
			.right_endpoint_binding_id = endpoints.right_binding_id,
			.generated_type_declaration_id = PROTOTYPE_INVALID_ID,
			.backing_type_former_term_id = relation,
			.backing_type_former_has_type_claim_id = relation_claim,
			.identity_type_term_id = identity_type,
			.identity_type_has_type_claim_id = identity_type_claim,
			.identity_type_is_type_claim_id = identity_type_is_type_claim,
			.left_context_certificate_id =
				endpoints.left_context_certificate_id,
			.right_context_certificate_id =
				endpoints.right_context_certificate_id,
			.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	int final_view_status = prototype_kernel_builder_view(kernel, &view);
	int certificate_status = final_view_status == 0 ?
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) : -1;
	if (final_view_status != 0 || certificate_status != 0) {
		fprintf(
			stderr,
			"universe fiber certificate failed request=%u view=%d add=%d "
			"identity_type=%u identity_claim=%u is_type_claim=%u\n",
			request_id,
			final_view_status,
			certificate_status,
			identity_type,
			identity_type_claim,
			identity_type_is_type_claim
		);
		return -1;
	}
	int publish_status = hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
	if (publish_status != 0) {
		fprintf(
			stderr,
			"universe fiber result publication failed request=%u certificate=%u "
			"status=%d\n",
			request_id,
			certificate_id,
			publish_status
		);
	}
	return publish_status;
}

int prototype_hott_execute_identity_type_computation(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!actions || !terms || !type_declarations || !contexts || !judgement ||
		!bridges || !p_result_id ||
		prototype_kernel_builder_validate(kernel) != 0) {
		return -1;
	}
	int existing = hott_action_result_for_request(
		actions, request_id, p_result_id
	);
	if (existing <= 0) {
		return existing;
	}
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	if (!request || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION) {
		return -1;
	}
	const struct prototype_judgement_proposition* source =
		prototype_judgement_claim_proposition(
			judgement, request->key.identity_type.source_claim_id
		);
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, request->key.identity_type.source_bridge_id
	);
	if (!source || !bridge || source->context_id != bridge->source_context_id) {
		return -1;
	}
	if (source->context_id != prototype_context_empty(contexts)) {
		int universe_fiber_status =
			hott_publish_universe_fiber_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				p_result_id
			);
		if (universe_fiber_status <= 0) {
			return universe_fiber_status;
		}
	}
	struct prototype_hott_type_former_descriptor descriptor;
	if (prototype_hott_type_former_descriptor_query(
			terms,
			type_declarations,
			contexts,
			judgement,
			request->key.identity_type.source_claim_id,
			&descriptor
		) != 0) {
		fprintf(
			stderr,
			"identity type descriptor failed request=%u source_claim=%u "
			"context=%u subject=%u classifier=%u\n",
			request_id,
			request->key.identity_type.source_claim_id,
			source->context_id,
			source->subject,
			source->classifier
		);
		return -1;
	}
	if (descriptor.capabilities.identity_computation ==
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
			p_result_id
		);
	}
	if (descriptor.capabilities.identity_computation !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED) {
		int reason = descriptor.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_NONE ? descriptor.residual_reason :
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			reason,
			p_result_id
		);
	}
	if (source->context_id != prototype_context_empty(contexts) &&
		descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE) {
		uint32_t generated_type_id;
		if (prototype_type_declaration_find_generated_identity(
				type_declarations,
				source->subject,
				prototype_context_empty(contexts),
				&generated_type_id
			) != 0) {
			return hott_publish_action_residual(
				actions,
				terms,
				request_id,
				PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
				p_result_id
			);
		}
		return hott_publish_constant_generated_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			generated_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE,
			p_result_id
		);
	}
	if (source->context_id != prototype_context_empty(contexts)) {
		uint32_t source_type_id;
		uint32_t source_arguments[2];
		uint32_t source_argument_count;
		if (prototype_term_type_instance_info(
				terms,
				source->subject,
				&source_type_id,
				source_arguments,
				&source_argument_count
			) == 0 && source_argument_count == 2 && source_type_id <
				type_declarations->type_count && type_declarations->type_declarations[
					source_type_id
				].origin_kind ==
					PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY) {
			return hott_publish_indexed_higher_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				source_type_id,
				p_result_id
			);
		}
		uint32_t generated_type_id;
		if (!hott_term_is_constant_over_context(
				terms, contexts, source->context_id, source->subject
			)) {
			return hott_publish_action_residual(
				actions,
				terms,
				request_id,
				PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
				p_result_id
			);
		}
		if (prototype_type_declaration_find_generated_identity(
				type_declarations,
				source->subject,
				prototype_context_empty(contexts),
				&generated_type_id
			) != 0) {
			const struct prototype_term* source_term =
				&terms->terms[source->subject];
			uint32_t computation = source_term->tag ==
				PROTOTYPE_TERM_THUNK_TYPE ?
				source_term->as.thunk_type.computation : PROTOTYPE_INVALID_ID;
			if (source_term->tag == PROTOTYPE_TERM_PI ||
				(computation < terms->term_count &&
				 terms->terms[computation].tag == PROTOTYPE_TERM_PI)) {
				return hott_publish_pi_identity_result(
					actions,
					kernel,
					bridges,
					request_id,
					source,
					bridge,
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT,
					p_result_id
				);
			}
			return hott_publish_action_residual(
				actions,
				terms,
				request_id,
				PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
				p_result_id
			);
		}
		return hott_publish_constant_generated_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			generated_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT,
			p_result_id
		);
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_UNIVERSE) {
		uint32_t generated_type_id;
		if (prototype_type_declaration_add_generated_identity(
				type_declarations,
				source->subject,
				source->context_id,
				&generated_type_id
			) != 0 || generated_type_id >= type_declarations->type_count) {
			return -1;
		}
		struct prototype_type_declaration* generated =
			&type_declarations->type_declarations[generated_type_id];
		if (hott_initialize_generated_identity_declaration(
				terms,
				contexts,
				source->context_id,
				source->subject,
				source->classifier,
				generated
			) != 0) {
			return -1;
		}
		if (generated->constructor_count == 0) {
			uint32_t constructor_id;
			if (hott_add_universe_correspondence_constructor(
					terms,
					type_declarations,
					contexts,
					source->subject,
					generated_type_id,
					generated,
					&constructor_id
				) != 0) {
				return -1;
			}
		}
		return hott_publish_generated_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			generated_type_id,
			1,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE,
			p_result_id
		);
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_PI) {
		return hott_publish_pi_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			p_result_id
		);
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_THUNK) {
		const struct prototype_term* thunk_type =
			&terms->terms[source->subject];
		const struct prototype_term* computation_type =
			&terms->terms[thunk_type->as.thunk_type.computation];
		if (computation_type->tag == PROTOTYPE_TERM_PI) {
			return hott_publish_pi_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
				p_result_id
			);
		}
		if (computation_type->tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return -1;
		}
		uint32_t result_identity_type_id = PROTOTYPE_INVALID_ID;
		uint32_t generated_type_id;
		int result_identity_status = hott_generated_identity_declaration(
				type_declarations,
				computation_type->as.computation_type.result,
				prototype_context_empty(contexts),
				&result_identity_type_id
			);
		uint32_t result_carrier = computation_type->as.computation_type.result;
		uint32_t result_computation = result_carrier < terms->term_count &&
			terms->terms[result_carrier].tag == PROTOTYPE_TERM_THUNK_TYPE ?
			terms->terms[result_carrier].as.thunk_type.computation :
			PROTOTYPE_INVALID_ID;
		if ((result_identity_status != 0 &&
			(result_computation >= terms->term_count ||
			 terms->terms[result_computation].tag != PROTOTYPE_TERM_PI)) ||
			prototype_type_declaration_add_generated_identity(
				type_declarations,
				source->subject,
				source->context_id,
				&generated_type_id
			) != 0 || generated_type_id >= type_declarations->type_count) {
			return -1;
		}
		struct prototype_type_declaration* generated =
			&type_declarations->type_declarations[generated_type_id];
		if (hott_initialize_generated_identity_declaration(
				terms,
				contexts,
				source->context_id,
				source->subject,
				source->classifier,
				generated
			) != 0) {
			return -1;
		}
		if (generated->constructor_count == 0 &&
			hott_add_thunk_return_identity_constructor(
				terms,
				type_declarations,
				contexts,
				result_carrier,
				result_identity_type_id,
				generated_type_id,
				generated
			) != 0) {
			return -1;
		}
		return hott_publish_generated_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			generated_type_id,
			1,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN,
			p_result_id
		);
	}
	if (descriptor.kind != PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT) {
		return -1;
	}
	uint32_t source_type_id;
	const struct prototype_type_declaration* source_type;
	if (prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			source->subject,
			&source_type_id,
			&source_type
		) != 0 || !source_type || source_type->parameter_count != 0 ||
		source_type->constructor_count == 0) {
		return -1;
	}
	uint32_t generated_type_id;
	if (prototype_type_declaration_add_generated_identity(
			type_declarations,
			source->subject,
			source->context_id,
			&generated_type_id
		) != 0 || generated_type_id >= type_declarations->type_count) {
		return -1;
	}
	struct prototype_type_declaration* generated =
		&type_declarations->type_declarations[generated_type_id];
	if (hott_initialize_generated_identity_declaration(
			terms,
			contexts,
			source->context_id,
			source->subject,
			source->classifier,
			generated
		) != 0) {
		return -1;
	}
	uint32_t expected_constructor_count = 0;
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		int compatible = hott_indexed_constructor_compatible(
			terms,
			type_declarations,
			contexts,
			source->subject,
			source_type,
			i
		);
		if (compatible < 0) {
			return -1;
		}
		expected_constructor_count += compatible != 0;
	}
	if (generated->constructor_count == 0) {
		for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
			int compatible = hott_indexed_constructor_compatible(
				terms,
				type_declarations,
				contexts,
				source->subject,
				source_type,
				i
			);
			if (compatible < 0) {
				return -1;
			}
			if (compatible == 0) {
				continue;
			}
			uint32_t generated_constructor_id;
			int recursive = hott_constructor_has_recursive_field(
				terms,
				contexts,
				source->subject,
				&type_declarations->constructor_declarations[
					source_type->first_constructor + i
				]
			);
			if (recursive < 0) {
				return -1;
			}
			int constructor_status = recursive ?
				hott_add_ordinary_adt_identity_constructor(
					terms,
					type_declarations,
					contexts,
					source->subject,
					source_type,
					i,
					generated_type_id,
					generated,
					&generated_constructor_id
				) : hott_add_contextual_adt_identity_constructor(
					actions,
					kernel,
					bridges,
					source->subject,
					source_type,
					i,
					generated_type_id,
					generated,
					&generated_constructor_id
				);
			if (constructor_status < 0) {
				fprintf(
					stderr,
					"identity constructor generation failed request=%u source=%u "
					"constructor=%u recursive=%d status=%d\n",
					request_id,
					source->subject,
					i,
					recursive,
					constructor_status
				);
				return -1;
			}
			if (constructor_status > 0) {
				return hott_publish_action_residual(
					actions,
					terms,
					request_id,
					PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
					p_result_id
				);
			}
		}
	}
	int publication_status = hott_publish_generated_identity_result(
		actions,
		kernel,
		bridges,
		request_id,
		source,
		generated_type_id,
		expected_constructor_count,
		PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
		p_result_id
	);
	if (publication_status != 0) {
		fprintf(
			stderr,
			"generated identity publication failed request=%u source=%u "
			"generated_type=%u expected_constructors=%u actual_constructors=%u "
			"status=%d\n",
			request_id,
			source->subject,
			generated_type_id,
			expected_constructor_count,
			generated->constructor_count,
			publication_status
		);
	}
	return publication_status;
}

static const struct prototype_hott_bridge*
hott_bridge_for_source_context_and_semantics(
	const struct prototype_hott_bridge_db* bridges,
	uint32_t source_context,
	int semantics,
	uint32_t* p_bridge_id
) {
	if (!bridges) {
		return NULL;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		if (bridges->bridges[i].source_context_id == source_context &&
			bridges->certificates[i].semantics == semantics) {
			if (p_bridge_id) {
				*p_bridge_id = i;
			}
			return &bridges->bridges[i];
		}
	}
	return NULL;
}

static const struct prototype_judgement_derivation*
hott_constructor_derivation_for_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	if (!judgement) {
		return NULL;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id == claim_id &&
			(derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO ||
			 derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION)) {
			return derivation;
		}
	}
	return NULL;
}

static const struct prototype_judgement_derivation*
hott_derivation_for_claim_and_kind(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	int proof_kind
) {
	if (!judgement) {
		return NULL;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id == claim_id &&
			derivation->proof_kind == proof_kind) {
			return derivation;
		}
	}
	return NULL;
}

static uint32_t hott_is_type_claim_for_subject(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject
) {
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
			prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == context_id && prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == subject) {
			return i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static uint32_t hott_has_type_claim_for_subject(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject
) {
	uint32_t found = PROTOTYPE_INVALID_ID;
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == context_id && prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == subject) {
			if (found != PROTOTYPE_INVALID_ID &&
				prototype_judgement_claim_proposition(
					judgement, found
				)->classifier != prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier) {
				return PROTOTYPE_INVALID_ID;
			}
			found = i;
		}
	}
	return found;
}

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

static int hott_construct_adt_degeneracy_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	uint32_t generated_type_id,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
);

static int hott_construct_universe_degeneracy(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	uint32_t empty = prototype_context_empty(contexts);
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!identity || !source || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source->context_id != empty ||
		source->classifier >= terms->term_count || terms->terms[
			source->classifier
		].tag != PROTOTYPE_TERM_UNIVERSE_VAR || identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE ||
		identity->generated_type_declaration_id >= types->type_count) {
		return -1;
	}

	uint32_t value_identity_type_id;
	if (prototype_type_declaration_find_generated_identity(
			types,
			source->subject,
			empty,
			&value_identity_type_id
		) != 0 || value_identity_type_id >= types->type_count) {
		return 1;
	}
	int value_identity_rule =
		prototype_type_declaration_generated_identity_rule_for_source(
			terms, source->subject
		);
	if (value_identity_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID ||
		!prototype_type_declaration_validate_generated_identity(
			terms,
			types,
			contexts,
			source->subject,
			value_identity_type_id,
			value_identity_rule
		)) {
		return 1;
	}

	uint32_t source_is_type_claim = hott_is_type_claim_for_subject(
		judgement, empty, source->subject
	);
	uint32_t source_universe_is_type_claim = hott_is_type_claim_for_subject(
		judgement, empty, source->classifier
	);
	const struct prototype_judgement_proposition* source_universe_is_type =
		prototype_judgement_claim_proposition(
			judgement, source_universe_is_type_claim
		);
	if (source_is_type_claim == PROTOTYPE_INVALID_ID &&
		prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			empty,
			source->subject,
			source->classifier,
			source_claim_id,
			&source_is_type_claim
		) != 0) {
		return -1;
	}
	if (!source_universe_is_type || source_universe_is_type->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source_universe_is_type->subject !=
			source->classifier) {
		return 1;
	}
	uint32_t x_binding = prototype_term_new_binding(terms);
	uint32_t x_context;
	uint32_t x_context_certificate;
	uint32_t x;
	uint32_t x_claim;
	if (x_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			empty,
			x_binding,
			source->subject,
			PROTOTYPE_INVALID_ID,
			&x_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			x_context,
			source_is_type_claim,
			&x_context_certificate
		) != 0 || prototype_term_var(
			terms, x_binding, &x
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			x_context,
			x_binding,
			source->subject,
			&x_claim
		) != 0) {
		return -1;
	}

	uint32_t x_to_empty;
	uint32_t source_is_type_in_x;
	uint32_t y_binding = prototype_term_new_binding(terms);
	uint32_t y_context;
	uint32_t y_context_certificate;
	uint32_t y;
	uint32_t y_claim;
	if (y_binding == PROTOTYPE_INVALID_ID ||
		prototype_substitution_projection_path(
			substitutions, contexts, x_context, empty, &x_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_is_type_claim,
			x_to_empty,
			&source_is_type_in_x
		) != 0 || prototype_context_extend(
			contexts,
			x_context,
			y_binding,
			source->subject,
			PROTOTYPE_INVALID_ID,
			&y_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			y_context,
			source_is_type_in_x,
			&y_context_certificate
		) != 0 || prototype_term_var(
			terms, y_binding, &y
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			y_context,
			y_binding,
			source->subject,
			&y_claim
		) != 0) {
		return -1;
	}
	uint32_t relation_body_arguments[2] = { x, y };
	uint32_t relation_body;
	uint32_t relation_body_claim;
	uint32_t relation_inner_classifier;
	uint32_t relation_inner;
	uint32_t relation_inner_claim;
	uint32_t relation_classifier;
	uint32_t relation;
	uint32_t relation_claim;
	if (prototype_term_type_instance_make(
			terms,
			types,
			value_identity_type_id,
			relation_body_arguments,
			2,
			&relation_body
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			y_context,
			relation_body,
			source->classifier,
			&relation_body_claim
		) != 0 || prototype_term_pi(
			terms,
			source->subject,
			source->classifier,
			&relation_inner_classifier
		) != 0 || prototype_term_lambda(
			terms, y_binding, relation_body, &relation_inner
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			x_context,
			relation_inner,
			relation_inner_classifier,
			y_claim,
			relation_body_claim,
			&relation_inner_claim
		) != 0 || prototype_term_pi(
			terms,
			source->subject,
			relation_inner_classifier,
			&relation_classifier
		) != 0 || prototype_term_lambda(
			terms, x_binding, relation_inner, &relation
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			empty,
			relation,
			relation_classifier,
			x_claim,
			relation_inner_claim,
			&relation_claim
		) != 0) {
		return -1;
	}

	uint32_t identity_lambda;
	uint32_t identity_lambda_classifier;
	uint32_t identity_lambda_claim;
	if (prototype_term_lambda(
			terms, x_binding, x, &identity_lambda
		) != 0 || prototype_term_pi(
			terms,
			source->subject,
			source->subject,
			&identity_lambda_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			empty,
			identity_lambda,
			identity_lambda_classifier,
			x_claim,
			x_claim,
			&identity_lambda_claim
		) != 0) {
		return -1;
	}

	uint32_t value_family_claim;
	uint32_t value_witness;
	uint32_t value_witness_claim;
	int value_status = hott_construct_adt_degeneracy_depth(
		kernel,
		x_claim,
		value_identity_type_id,
		source->classifier,
		(uint32_t)terms->term_count + 1,
		&value_family_claim,
		&value_witness,
		&value_witness_claim
	);
	if (value_status != 0) {
		return value_status;
	}
	const struct prototype_judgement_proposition* value_family =
		prototype_judgement_claim_proposition(judgement, value_family_claim);
	if (!value_family || value_family->context_id != x_context) {
		return -1;
	}
	uint32_t lift_family;
	uint32_t lift_classifier;
	uint32_t lift_lambda;
	uint32_t lift_lambda_claim;
	uint32_t transported_x;
	uint32_t value_relation_x;
	uint32_t lifted_value_family;
	uint32_t lifted_value_witness_claim;
	if (prototype_term_app(
			terms, identity_lambda, x, &transported_x
		) != 0 || prototype_term_app(
			terms, relation, x, &value_relation_x
		) != 0 || prototype_term_app(
			terms, value_relation_x, transported_x, &lifted_value_family
		) != 0 || prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			x_context,
			value_witness,
			lifted_value_family,
			value_witness_claim,
			&lifted_value_witness_claim
		) != 0 || prototype_term_pure_family(
			terms,
			x_binding,
			lifted_value_family,
			&lift_family
		) != 0 || prototype_term_pi_family(
			terms,
			source->subject,
			lift_family,
			&lift_classifier
		) != 0 || prototype_term_lambda(
			terms, x_binding, value_witness, &lift_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			empty,
			lift_lambda,
			lift_classifier,
			x_claim,
			lifted_value_witness_claim,
			&lift_lambda_claim
		) != 0) {
		return -1;
	}

	uint32_t universe_identity_former;
	uint32_t witness;
	if (prototype_term_type_instance_make(
			terms,
			types,
			identity->generated_type_declaration_id,
			NULL,
			0,
			&universe_identity_former
		) != 0 || prototype_term_constructor(
			terms, universe_identity_former, 0, &witness
		) != 0) {
		return -1;
	}
	uint32_t arguments[7] = {
		source->subject,
		source->subject,
		relation,
		identity_lambda,
		identity_lambda,
		lift_lambda,
		lift_lambda
	};
	uint32_t argument_claims[7] = {
		source_claim_id,
		source_claim_id,
		relation_claim,
		identity_lambda_claim,
		identity_lambda_claim,
		lift_lambda_claim,
		lift_lambda_claim
	};
	for (uint32_t i = 0; i < 7; ++i) {
		if (prototype_term_app(terms, witness, arguments[i], &witness) != 0) {
			return -1;
		}
	}
	uint32_t family_arguments[2] = { source->subject, source->subject };
	uint32_t identity_family;
	uint32_t identity_family_claim;
	uint32_t witness_claim;
	if (prototype_term_type_instance_make(
			terms,
			types,
			identity->generated_type_declaration_id,
			family_arguments,
			2,
			&identity_family
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
				empty,
				identity_family,
				source_universe_is_type->classifier,
				&identity_family_claim
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_constructor_spine_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			empty,
			witness,
			identity_family,
			argument_claims,
			7,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)x_context_certificate;
	(void)y_context_certificate;
	return 0;
}

static int hott_abstract_pure_claim(
	struct prototype_kernel_builder* kernel,
	uint32_t parent_context_id,
	uint32_t binding_id,
	uint32_t domain,
	uint32_t binder_claim_id,
	uint32_t body_claim_id,
	uint32_t* p_lambda_term_id,
	uint32_t* p_lambda_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* binder = judgement ?
		prototype_judgement_claim_proposition(judgement, binder_claim_id) : NULL;
	const struct prototype_judgement_proposition* body = judgement ?
		prototype_judgement_claim_proposition(judgement, body_claim_id) : NULL;
	const struct prototype_context* body_context = body && contexts ?
		prototype_context_get(contexts, body->context_id) : NULL;
	uint32_t binder_term;
	uint32_t codomain_family;
	uint32_t classifier;
	uint32_t lambda;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!binder || !body || !body_context || !p_lambda_term_id ||
		!p_lambda_claim_id || binder->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		body->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		body_context->parent != parent_context_id ||
		body_context->binding_id != binding_id || binder->context_id !=
			body->context_id || binder->classifier != domain ||
		prototype_term_var(terms, binding_id, &binder_term) != 0 ||
		binder->subject != binder_term || prototype_term_pure_family(
			terms, binding_id, body->classifier, &codomain_family
		) != 0 || prototype_term_pi_family(
			terms, domain, codomain_family, &classifier
		) != 0 || prototype_term_lambda(
			terms, binding_id, body->subject, &lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			parent_context_id,
			lambda,
			classifier,
			binder_claim_id,
			body_claim_id,
			p_lambda_claim_id
		) != 0) {
		return -1;
	}
	*p_lambda_term_id = lambda;
	return 0;
}

static int hott_universe_projection_has_variable_boundary(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* source,
	const uint32_t endpoints[2]
) {
	if (!terms || !contexts || !source || source->subject >= terms->term_count ||
		endpoints[0] >= terms->term_count || endpoints[1] >= terms->term_count ||
		terms->terms[source->subject].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[endpoints[0]].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[endpoints[1]].tag != PROTOTYPE_TERM_VAR) {
		return 0;
	}
	uint32_t left_context_id;
	uint32_t right_context_id;
	uint32_t proof_context_id;
	if (prototype_context_find_binding(
			contexts,
			source->context_id,
			terms->terms[endpoints[0]].as.var.binding_id,
			&left_context_id
		) != 0 || prototype_context_find_binding(
			contexts,
			source->context_id,
			terms->terms[endpoints[1]].as.var.binding_id,
			&right_context_id
		) != 0 || prototype_context_find_binding(
			contexts,
			source->context_id,
			terms->terms[source->subject].as.var.binding_id,
			&proof_context_id
		) != 0) {
		return 0;
	}
	const struct prototype_context* left = prototype_context_get(
		contexts, left_context_id
	);
	const struct prototype_context* right = prototype_context_get(
		contexts, right_context_id
	);
	const struct prototype_context* proof = prototype_context_get(
		contexts, proof_context_id
	);
	return left && right && proof && right->parent == left_context_id &&
		proof->parent == right_context_id && proof_context_id == source->context_id;
}

static int hott_construct_general_universe_projection(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t universe_identity_result_id,
	uint32_t correspondence_claim_id,
	uint32_t identity_type_id,
	uint32_t universe,
	const uint32_t endpoints[2],
	int projection,
	uint32_t* p_projection_term_id,
	uint32_t* p_projection_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(
			judgement, correspondence_claim_id
		) : NULL;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !judgement || !source || !p_projection_term_id ||
		!p_projection_claim_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}

	uint32_t universe_is_type;
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t left_context_certificate;
	uint32_t left;
	uint32_t left_claim;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			universe,
			&universe_is_type
		) != 0 || prototype_context_extend(
			contexts,
			source->context_id,
			left_binding,
			universe,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			left_context,
			universe_is_type,
			&left_context_certificate
		) != 0 || prototype_term_var(
			terms, left_binding, &left
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			left_context,
			left_binding,
			universe,
			&left_claim
		) != 0) {
		return -1;
	}

	uint32_t universe_is_type_left;
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t right_context;
	uint32_t right_context_certificate;
	uint32_t right;
	uint32_t right_claim;
	if (right_binding == PROTOTYPE_INVALID_ID ||
		hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			left_context,
			universe,
			&universe_is_type_left
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			universe,
			PROTOTYPE_INVALID_ID,
			&right_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			right_context,
			universe_is_type_left,
			&right_context_certificate
		) != 0 || prototype_term_var(
			terms, right_binding, &right
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			right_context,
			right_binding,
			universe,
			&right_claim
		) != 0) {
		return -1;
	}

	uint32_t generic_arguments[2] = { left, right };
	uint32_t generic_identity;
	uint32_t generic_identity_has_type;
	uint32_t generic_identity_is_type;
	uint32_t proof_binding = prototype_term_new_binding(terms);
	uint32_t proof_context;
	uint32_t proof_context_certificate;
	uint32_t proof;
	uint32_t proof_claim;
	if (proof_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_type_instance_make(
			terms,
			types,
			identity_type_id,
			generic_arguments,
			2,
			&generic_identity
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			right_context,
			generic_identity,
			universe,
			&generic_identity_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			right_context,
			generic_identity,
			universe,
			generic_identity_has_type,
			&generic_identity_is_type
		) != 0 || prototype_context_extend(
			contexts,
			right_context,
			proof_binding,
			generic_identity,
			PROTOTYPE_INVALID_ID,
			&proof_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			proof_context,
			generic_identity_is_type,
			&proof_context_certificate
		) != 0 || prototype_term_var(
			terms, proof_binding, &proof
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			proof_context,
			proof_binding,
			generic_identity,
			&proof_claim
		) != 0) {
		return -1;
	}
	uint32_t proof_to_left;
	uint32_t proof_to_right;
	uint32_t left_has_type_in_proof;
	uint32_t right_has_type_in_proof;
	uint32_t left_is_type_in_proof;
	uint32_t right_is_type_in_proof;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			proof_context,
			left_context,
			&proof_to_left
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			proof_context,
			right_context,
			&proof_to_right
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			left_claim,
			proof_to_left,
			&left_has_type_in_proof
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_claim,
			proof_to_right,
			&right_has_type_in_proof
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			proof_context,
			left,
			universe,
			left_has_type_in_proof,
			&left_is_type_in_proof
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			proof_context,
			right,
			universe,
			right_has_type_in_proof,
			&right_is_type_in_proof
		) != 0) {
		return -1;
	}

	uint32_t generic_projection;
	uint32_t generic_projection_claim;
	if (prototype_hott_construct_universe_correspondence_projection(
			actions,
			kernel,
			bridges,
			universe_identity_result_id,
			proof_claim,
			projection,
			&generic_projection,
			&generic_projection_claim
		) != 0) {
		return -1;
	}
	uint32_t proof_lambda;
	uint32_t proof_lambda_claim;
	uint32_t right_lambda;
	uint32_t right_lambda_claim;
	uint32_t left_lambda;
	uint32_t left_lambda_claim;
	if (hott_abstract_pure_claim(
			kernel,
			right_context,
			proof_binding,
			generic_identity,
			proof_claim,
			generic_projection_claim,
			&proof_lambda,
			&proof_lambda_claim
		) != 0 || hott_abstract_pure_claim(
			kernel,
			left_context,
			right_binding,
			universe,
			right_claim,
			proof_lambda_claim,
			&right_lambda,
			&right_lambda_claim
		) != 0 || hott_abstract_pure_claim(
			kernel,
			source->context_id,
			left_binding,
			universe,
			left_claim,
			right_lambda_claim,
			&left_lambda,
			&left_lambda_claim
		) != 0) {
		return -1;
	}

	uint32_t endpoint_claims[2];
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			endpoints[0],
			universe,
			&endpoint_claims[0]
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			endpoints[1],
			universe,
			&endpoint_claims[1]
		) != 0) {
		return -1;
	}
	uint32_t arguments[3] = {
		endpoint_claims[0], endpoint_claims[1], correspondence_claim_id
	};
	if (hott_apply_type_pointwise_identity_witness(
			kernel,
			left_lambda_claim,
			arguments,
			3,
			p_projection_term_id,
			p_projection_claim_id
		) != 0) {
		return -1;
	}
	(void)generic_projection;
	(void)proof_lambda;
	(void)right_lambda;
	(void)left_lambda;
	(void)left_context_certificate;
	(void)right_context_certificate;
	(void)proof_context_certificate;
	(void)left_is_type_in_proof;
	(void)right_is_type_in_proof;
	return 0;
}

int prototype_hott_construct_universe_correspondence_projection(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t universe_identity_result_id,
	uint32_t correspondence_claim_id,
	int projection,
	uint32_t* p_projection_term_id,
	uint32_t* p_projection_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, universe_identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(
			judgement, correspondence_claim_id
		) : NULL;
	struct prototype_kernel_view view;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !judgement || !result || !request || !certificate ||
		!source || !p_projection_term_id || !p_projection_claim_id ||
		projection < PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION ||
		projection > PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_LEFT ||
		prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_db_validate(actions, &view, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		request->kind != PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		(certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE &&
		 certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) ||
		 source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	const struct prototype_judgement_derivation* source_reindex =
		hott_derivation_for_claim_and_kind(
			judgement,
			correspondence_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
		);
	if (source_reindex) {
		if (source_reindex->premise_count != 1 ||
			source_reindex->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			source_reindex->semantic_action_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		uint32_t source_projection;
		uint32_t source_projection_claim;
		int source_projection_status =
			prototype_hott_construct_universe_correspondence_projection(
				actions,
				kernel,
				bridges,
				universe_identity_result_id,
				source_reindex->premises[0].claim_id,
				projection,
				&source_projection,
				&source_projection_claim
			);
		if (source_projection_status != 0) {
			return source_projection_status;
		}
		uint32_t projection_claim;
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source_projection_claim,
				source_reindex->semantic_action_id,
				&projection_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* reindexed_projection =
			prototype_judgement_claim_proposition(judgement, projection_claim);
		if (!reindexed_projection || reindexed_projection->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			reindexed_projection->context_id != source->context_id) {
			return -1;
		}
		*p_projection_term_id = reindexed_projection->subject;
		*p_projection_claim_id = projection_claim;
		(void)source_projection;
		return 0;
	}
	const struct prototype_hott_identity_type_computation_certificate* identity =
		&certificate->data.identity_type;
	uint32_t identity_type_id;
	uint32_t endpoints[2];
	uint32_t endpoint_count;
	if (prototype_term_type_instance_info(
			terms,
			source->classifier,
			&identity_type_id,
			endpoints,
			&endpoint_count
		) != 0 || identity_type_id != identity->generated_type_declaration_id ||
		endpoint_count != 2 || identity_type_id >= types->type_count) {
		return 1;
	}
	const struct prototype_type_declaration* identity_type =
		&types->type_declarations[identity_type_id];
	if (identity_type->constructor_count != 1 ||
		!prototype_type_declaration_validate_generated_identity(
			terms,
			types,
			contexts,
			request->key.identity_type.source_claim_id < judgement->claim_count ?
				prototype_judgement_claim_proposition(
					judgement, request->key.identity_type.source_claim_id
				)->subject : PROTOTYPE_INVALID_ID,
			identity_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE
		)) {
		return 1;
	}
	uint32_t universe = identity_type->origin_source_carrier_term_id;
	if (!hott_universe_projection_has_variable_boundary(
			terms, contexts, source, endpoints
		)) {
		return hott_construct_general_universe_projection(
			actions,
			kernel,
			bridges,
			universe_identity_result_id,
			correspondence_claim_id,
			identity_type_id,
			universe,
			endpoints,
			projection,
			p_projection_term_id,
			p_projection_claim_id
		);
	}
	uint32_t result_classifier;
	uint32_t prepared_result_binder_context = PROTOTYPE_INVALID_ID;
	uint32_t prepared_result_codomain_claim = PROTOTYPE_INVALID_ID;
	if (projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION) {
		uint32_t right_relation;
		if (prototype_term_pi(
				terms, endpoints[1], universe, &right_relation
			) != 0 || prototype_term_pi(
				terms, endpoints[0], right_relation, &result_classifier
			) != 0) {
			return -1;
		}
	} else if (projection <= PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_LEFT &&
		prototype_term_pi(
			terms,
			projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_RIGHT ?
				endpoints[0] : endpoints[1],
			projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_RIGHT ?
				endpoints[1] : endpoints[0],
			&result_classifier
		) != 0) {
		return -1;
	} else if (projection >= PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT) {
		uint32_t relation_projection;
		uint32_t relation_projection_claim;
		uint32_t transport_projection;
		uint32_t transport_projection_claim;
		int transport_kind = projection ==
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_RIGHT :
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_LEFT;
		int relation_projection_status =
			prototype_hott_construct_universe_correspondence_projection(
				actions,
				kernel,
				bridges,
				universe_identity_result_id,
				correspondence_claim_id,
				PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION,
				&relation_projection,
				&relation_projection_claim
			);
		if (relation_projection_status != 0) {
			return relation_projection_status;
		}
		if (prototype_hott_construct_universe_correspondence_projection(
				actions,
				kernel,
				bridges,
				universe_identity_result_id,
				correspondence_claim_id,
				transport_kind,
				&transport_projection,
				&transport_projection_claim
			) != 0) {
			return -1;
		}
		uint32_t lift_domain = projection ==
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ? endpoints[0] : endpoints[1];
		uint32_t lift_domain_has_type;
		uint32_t lift_domain_is_type;
		uint32_t lift_binding = prototype_term_new_binding(terms);
		uint32_t lift_context_certificate;
		uint32_t lift_projection;
		uint32_t relation_in_lift_context;
		uint32_t transport_in_lift_context;
		uint32_t lift_value;
		uint32_t lift_value_claim;
		if (lift_binding == PROTOTYPE_INVALID_ID ||
			prototype_judgement_add_structural_type_formation_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source->context_id,
				lift_domain,
				universe,
				&lift_domain_has_type
			) != 0 || prototype_judgement_add_is_type_claim(
				judgement,
				terms,
				source->context_id,
				lift_domain,
				universe,
				lift_domain_has_type,
				&lift_domain_is_type
			) != 0 || prototype_context_extend(
				contexts,
				source->context_id,
				lift_binding,
				lift_domain,
				PROTOTYPE_INVALID_ID,
				&prepared_result_binder_context
			) != 0 || prototype_cwf_certificate_db_add_context(
				kernel->cwf_certificates,
				contexts,
				terms,
				types,
				judgement,
				prepared_result_binder_context,
				lift_domain_is_type,
				&lift_context_certificate
			) != 0 || prototype_substitution_projection_path(
				substitutions,
				contexts,
				prepared_result_binder_context,
				source->context_id,
				&lift_projection
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				relation_projection_claim,
				lift_projection,
				&relation_in_lift_context
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				transport_projection_claim,
				lift_projection,
				&transport_in_lift_context
			) != 0 || prototype_term_var(
				terms, lift_binding, &lift_value
			) != 0 || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				prepared_result_binder_context,
				lift_binding,
				lift_domain,
				&lift_value_claim
			) != 0) {
			return -1;
		}
		uint32_t transported;
		uint32_t transported_claim;
		uint32_t relation_at_first;
		uint32_t relation_at_first_claim;
		uint32_t relation_at;
		if (prototype_term_app(
				terms, transport_projection, lift_value, &transported
			) != 0 || prototype_judgement_add_app_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				prepared_result_binder_context,
				transported,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					endpoints[1] : endpoints[0],
				transport_in_lift_context,
				lift_value_claim,
				&transported_claim
			) != 0 || prototype_term_app(
				terms,
				relation_projection,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					lift_value : transported,
				&relation_at_first
			) != 0) {
			return -1;
		}
		uint32_t relation_second_classifier;
		if (prototype_term_pi(
				terms, endpoints[1], universe, &relation_second_classifier
			) != 0) {
			return -1;
		}
		/* Rebuild the first application Claim with its actual codomain. */
		if (prototype_judgement_add_app_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				prepared_result_binder_context,
				relation_at_first,
				relation_second_classifier,
				relation_in_lift_context,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					lift_value_claim : transported_claim,
				&relation_at_first_claim
			) != 0 || prototype_term_app(
				terms,
				relation_at_first,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					transported : lift_value,
				&relation_at
			) != 0 || prototype_judgement_add_app_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				prepared_result_binder_context,
				relation_at,
				universe,
				relation_at_first_claim,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					transported_claim : lift_value_claim,
				&prepared_result_codomain_claim
			) != 0) {
			return -1;
		}
		uint32_t result_family;
		if (prototype_term_pure_family(
				terms, lift_binding, relation_at, &result_family
			) != 0 || prototype_term_pi_family(
				terms, lift_domain, result_family, &result_classifier
			) != 0) {
			return -1;
		}
		(void)lift_context_certificate;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* existing =
			prototype_judgement_claim_proposition(judgement, i);
		const struct prototype_term* existing_term = existing &&
			existing->subject < terms->term_count ?
			&terms->terms[existing->subject] : NULL;
		if (!existing || existing->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			!existing_term || existing_term->tag != PROTOTYPE_TERM_MATCH ||
			existing_term->as.match.case_count != 1 ||
			existing_term->as.match.first_case >= terms->case_count) {
			continue;
		}
		const struct prototype_match_case* existing_case = &terms->cases[
			existing_term->as.match.first_case
		];
		uint32_t existing_owner_type;
		uint32_t existing_owner_arguments[2];
		uint32_t existing_owner_argument_count;
		if (prototype_term_type_instance_info(
				terms,
				existing_case->constructor_owner,
				&existing_owner_type,
				existing_owner_arguments,
				&existing_owner_argument_count
			) != 0 || existing_owner_type != identity_type_id ||
			existing_owner_argument_count != 2 || existing_case->constructor_id != 0 ||
			existing_case->binder_count != 7 ||
			existing_case->first_binder + projection >= terms->case_binder_count ||
			existing_case->body >= terms->term_count ||
			terms->terms[existing_case->body].tag != PROTOTYPE_TERM_VAR ||
			terms->terms[existing_case->body].as.var.binding_id !=
				terms->case_binders[
					existing_case->first_binder + projection
				].binding_id) {
			continue;
		}
		if (existing->context_id == source->context_id &&
			existing->classifier == result_classifier &&
			existing_term->as.match.scrutinee == source->subject) {
			*p_projection_term_id = existing->subject;
			*p_projection_claim_id = i;
			return 0;
		}
	}

	uint32_t identity_former;
	if (prototype_term_type_instance_make(
			terms, types, identity_type_id, NULL, 0, &identity_former
		) != 0) {
		return -1;
	}
	struct prototype_case_binder proposed_binders[7];
	uint32_t proposed_fields[7];
	for (uint32_t i = 0; i < 7; ++i) {
		proposed_binders[i] = (struct prototype_case_binder) {
			.binding_id = prototype_term_new_binding(terms),
			.is_recursive = 0
		};
		if (proposed_binders[i].binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				terms, proposed_binders[i].binding_id, &proposed_fields[i]
			) != 0) {
			return -1;
		}
	}
	struct prototype_match_case_input proposed_case = {
		.case_label_symbol_id = -1,
		.constructor_owner = source->classifier,
		.constructor_id = 0,
		.binders = proposed_binders,
		.binder_count = 7,
		.body = proposed_fields[projection]
	};
	uint32_t match;
	if (prototype_term_match(
			terms, source->subject, &proposed_case, 1, &match
		) != 0 || match >= terms->term_count ||
		terms->terms[match].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[match].as.match.case_count != 1 ||
		terms->terms[match].as.match.first_case >= terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* canonical_case = &terms->cases[
		terms->terms[match].as.match.first_case
	];
	if (canonical_case->constructor_id != 0 ||
		canonical_case->binder_count != 7 ||
		canonical_case->first_binder > terms->case_binder_count ||
		canonical_case->binder_count > terms->case_binder_count -
			canonical_case->first_binder) {
		return -1;
	}
	struct prototype_case_binder binders[7];
	uint32_t fields[7];
	uint32_t field_classifiers[7];
	uint32_t branch_context = source->context_id;
	for (uint32_t i = 0; i < 7; ++i) {
		uint32_t next_context;
		binders[i] = terms->case_binders[canonical_case->first_binder + i];
		if (binders[i].is_recursive ||
			prototype_judgement_constructor_field_classifier(
				terms,
				types,
				contexts,
				substitutions,
				branch_context,
				identity_former,
				0,
				binders,
				i,
				i,
				&field_classifiers[i]
			) != 0 || prototype_context_extend(
				contexts,
				branch_context,
				binders[i].binding_id,
				field_classifiers[i],
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_term_var(
				terms, binders[i].binding_id, &fields[i]
			) != 0) {
			return -1;
		}
		branch_context = next_context;
	}
	if (canonical_case->body != fields[projection]) {
		return -1;
	}
	uint32_t selected_claim;
	if (prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			branch_context,
			binders[projection].binding_id,
			field_classifiers[projection],
			&selected_claim
		) != 0) {
		return -1;
	}
	uint32_t refined_context;
	uint32_t refinement_substitution;
	uint32_t constructor_term;
	uint32_t refined_selected_claim;
	if (prototype_judgement_indexed_branch_refinement(
			contexts,
			substitutions,
			terms,
			types,
			source->context_id,
			source->subject,
			source->classifier,
			0,
			branch_context,
			binders,
			7,
			&refined_context,
			&refinement_substitution,
			&constructor_term
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			selected_claim,
			refinement_substitution,
			&refined_selected_claim
		) != 0) {
		return -1;
	}

	uint32_t source_identity_is_type;
	if (hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			source->classifier,
			&source_identity_is_type
		) != 0) {
		return -1;
	}
	uint32_t motive_binding = prototype_term_new_binding(terms);
	uint32_t motive_context;
	uint32_t motive_context_certificate;
	uint32_t motive_body_claim;
	uint32_t motive_binder_claim;
	uint32_t motive_projection;
	uint32_t result_classifier_is_type;
	uint32_t result_domain;
	uint32_t result_family;
	uint32_t result_binder;
	uint32_t result_codomain;
	uint32_t result_codomain_is_type;
	uint32_t result_binder_context;
	uint32_t result_binder_projection;
	uint32_t result_codomain_in_binder;
	uint32_t motive;
	uint32_t motive_classifier;
	uint32_t motive_claim;
	if (motive_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_pi_parts(
			terms, result_classifier, &result_domain, &result_family
		) != 0 || prototype_term_pure_family_parts(
			terms, result_family, &result_binder, &result_codomain
		) != 0) {
		return -1;
	}
	if (prepared_result_binder_context == PROTOTYPE_INVALID_ID) {
		if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			result_codomain,
			universe,
			&result_codomain_is_type
		) != 0 || prototype_context_extend(
			contexts,
			source->context_id,
			result_binder,
			result_domain,
			PROTOTYPE_INVALID_ID,
			&result_binder_context
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			result_binder_context,
			source->context_id,
			&result_binder_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			result_codomain_is_type,
			result_binder_projection,
			&result_codomain_in_binder
		) != 0) {
			return -1;
		}
	} else {
		uint32_t result_binder_value;
		uint32_t prepared_binder_substitution;
		if (prototype_context_extend(
				contexts,
				source->context_id,
				result_binder,
				result_domain,
				PROTOTYPE_INVALID_ID,
				&result_binder_context
			) != 0 || prototype_substitution_projection_path(
				substitutions,
				contexts,
				result_binder_context,
				source->context_id,
				&result_binder_projection
			) != 0 || prototype_term_var(
				terms, result_binder, &result_binder_value
			) != 0 || prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				types,
				result_binder_projection,
				prepared_result_binder_context,
				result_binder_value,
				result_domain,
				&prepared_binder_substitution
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				prepared_result_codomain_claim,
				prepared_binder_substitution,
				&result_codomain_in_binder
			) != 0 || prototype_judgement_claim_proposition(
				judgement, result_codomain_in_binder
			)->subject != result_codomain) {
			return -1;
		}
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			result_classifier,
			universe,
			&result_classifier_is_type
		) != 0) {
		return -1;
	}
	if (prototype_context_extend(
			contexts,
			source->context_id,
			motive_binding,
			source->classifier,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0) {
		return -1;
	}
	if (prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			motive_context,
			source_identity_is_type,
			&motive_context_certificate
		) != 0) {
		return -1;
	}
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			motive_context,
			source->context_id,
			&motive_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			result_classifier_is_type,
			motive_projection,
			&motive_body_claim
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			motive_context,
			motive_binding,
			source->classifier,
			&motive_binder_claim
		) != 0) {
		return -1;
	}
	if (prototype_term_lambda(
			terms, motive_binding, result_classifier, &motive
		) != 0 || prototype_term_pi(
			terms, source->classifier, universe, &motive_classifier
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			motive,
			motive_classifier,
			motive_binder_claim,
			motive_body_claim,
			&motive_claim
		) != 0) {
		return -1;
	}
	uint32_t match_classifier;
	uint32_t match_classifier_claim;
	if (prototype_term_app(
			terms, motive, source->subject, &match_classifier
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			match_classifier,
			universe,
			motive_claim,
			correspondence_claim_id,
			&match_classifier_claim
		) != 0) {
		return -1;
	}
	uint32_t match_claim;
	uint32_t projection_claim;
	if (prototype_judgement_add_indexed_match_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			kernel->operations,
			source->context_id,
			match,
			match_classifier,
			match_classifier_claim,
			&refined_selected_claim,
			&refinement_substitution,
			1,
			&match_claim
		) != 0 || prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			source->context_id,
			match,
			result_classifier,
			match_claim,
			&projection_claim
		) != 0) {
		return -1;
	}
	*p_projection_term_id = match;
	*p_projection_claim_id = projection_claim;
	(void)constructor_term;
	(void)motive_context_certificate;
	return 0;
}

static int hott_construct_adt_degeneracy_match(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	uint32_t generated_type_id,
	uint32_t universe,
	uint32_t identity_family,
	uint32_t identity_family_claim,
	uint32_t remaining_depth,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	uint32_t source_type_id;
	uint32_t source_type_arguments[16];
	uint32_t source_type_argument_count;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!kernel->cwf_certificates || !source || !p_witness_term_id ||
		!p_witness_claim_id || remaining_depth == 0 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		generated_type_id >= types->type_count ||
		prototype_term_type_instance_info(
			terms,
			source->classifier,
			&source_type_id,
			source_type_arguments,
			&source_type_argument_count
		) != 0 || source_type_id >= types->type_count ||
		source_type_argument_count != 0) {
		return 1;
	}
	const struct prototype_type_declaration* source_type =
		&types->type_declarations[source_type_id];
	if (source_type->constructor_count == 0 ||
		source_type->constructor_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return 1;
	}

	uint32_t source_type_claim;
	if (hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			source->classifier,
			&source_type_claim
		) != 0) {
		return 1;
	}
	uint32_t motive_binding = prototype_term_new_binding(terms);
	uint32_t motive_context;
	uint32_t motive_context_certificate;
	uint32_t motive_var;
	if (motive_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			source->context_id,
			motive_binding,
			source->classifier,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			motive_context,
			source_type_claim,
			&motive_context_certificate
		) != 0 || prototype_term_var(
			terms, motive_binding, &motive_var
		) != 0) {
		return -1;
	}
	uint32_t motive_body_arguments[2] = { motive_var, motive_var };
	uint32_t motive_body;
	uint32_t motive_body_claim;
	uint32_t motive_binder_claim;
	uint32_t motive;
	uint32_t motive_pi;
	uint32_t motive_claim;
	if (prototype_term_type_instance_make(
			terms,
			types,
			generated_type_id,
			motive_body_arguments,
			2,
			&motive_body
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			motive_context,
			motive_body,
			universe,
			&motive_body_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			motive_context,
			motive_binding,
			source->classifier,
			&motive_binder_claim
		) != 0 || prototype_term_lambda(
			terms, motive_binding, motive_body, &motive
		) != 0 || prototype_term_pi(
			terms, source->classifier, universe, &motive_pi
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			motive,
			motive_pi,
			motive_binder_claim,
			motive_body_claim,
			&motive_claim
		) != 0) {
		return -1;
	}
	uint32_t match_classifier;
	uint32_t match_classifier_claim;
	if (prototype_term_app(
			terms, motive, source->subject, &match_classifier
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			match_classifier,
			universe,
			motive_claim,
			source_claim_id,
			&match_classifier_claim
		) != 0) {
		return -1;
	}
	struct prototype_match_case_input cases[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	uint32_t branch_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_contexts[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_families[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_argument_counts[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_argument_claims[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	][PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_case_binder case_binders[256];
	uint32_t case_binder_cursor = 0;
	struct hott_pending_identity_ih {
		uint32_t branch_index;
		uint32_t field_index;
		uint32_t argument_claim_index;
		uint32_t context_id;
		uint32_t term;
		uint32_t classifier;
	};
	struct hott_pending_identity_ih pending_ih[256];
	uint32_t pending_ih_count = 0;
	uint32_t ih_scope = PROTOTYPE_INVALID_ID;
	uint32_t identity_former;
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, NULL, 0, &identity_former
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t declaration_id = source_type->first_constructor + i;
		const struct prototype_type_constructor_declaration* constructor =
			declaration_id < types->constructor_count ?
			&types->constructor_declarations[declaration_id] : NULL;
		const struct prototype_context* parameter_context = constructor ?
			prototype_context_get(contexts, constructor->parameter_context) : NULL;
		const struct prototype_context* field_context = constructor ?
			prototype_context_get(contexts, constructor->field_context) : NULL;
		if (!constructor || !parameter_context || !field_context ||
			field_context->depth < parameter_context->depth) {
			return -1;
		}
		uint32_t field_count = field_context->depth - parameter_context->depth;
		if (case_binder_cursor + field_count > 256 ||
			field_count * 3 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return 1;
		}
		struct prototype_case_binder* branch_binders =
			&case_binders[case_binder_cursor];
		uint32_t endpoint;
		uint32_t branch_witness = PROTOTYPE_INVALID_ID;
		if (prototype_term_constructor(
				terms, source->classifier, i, &endpoint
			) != 0 || (field_count > 0 && prototype_term_constructor(
				terms,
				identity_former,
				i,
				&branch_witness
			) != 0)) {
			return -1;
		}
		uint32_t branch_context = source->context_id;
		uint32_t argument_count = 0;
		for (uint32_t j = 0; j < field_count; ++j) {
			uint32_t field_classifier;
			uint32_t field_type_claim;
			uint32_t field_binding = prototype_term_new_binding(terms);
			if (field_binding == PROTOTYPE_INVALID_ID ||
				prototype_judgement_constructor_field_classifier(
					terms,
					types,
					contexts,
					substitutions,
					branch_context,
					source->classifier,
					i,
					branch_binders,
					j,
					j,
					&field_classifier
				) != 0) {
				return 1;
			}
			int type_status = hott_ensure_is_type_claim_in_context(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				branch_context,
				field_classifier,
				&field_type_claim
			);
			if (type_status != 0) {
				uint32_t field_has_type_claim;
				if (prototype_judgement_add_type_formation_claim(
						judgement,
						terms,
						types,
						branch_context,
						field_classifier,
						universe,
						&field_has_type_claim
					) != 0 || prototype_judgement_add_is_type_claim(
						judgement,
						terms,
						branch_context,
						field_classifier,
						universe,
						field_has_type_claim,
						&field_type_claim
					) != 0) {
					return 1;
				}
			}
			int recursive = prototype_judgement_classifier_conversion(
				terms, types, field_classifier, source->classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
			branch_binders[j] = (struct prototype_case_binder) {
				.binding_id = field_binding,
				.is_recursive = recursive
			};
			uint32_t next_context;
			uint32_t context_certificate;
			if (prototype_context_extend(
					contexts,
					branch_context,
					field_binding,
					field_classifier,
					PROTOTYPE_INVALID_ID,
					&next_context
				) != 0 || prototype_cwf_certificate_db_add_context(
					kernel->cwf_certificates,
					contexts,
					terms,
					types,
					judgement,
					next_context,
					field_type_claim,
					&context_certificate
				) != 0) {
				return -1;
			}
			branch_context = next_context;
			uint32_t field_claim;
			uint32_t field;
			if (prototype_judgement_add_context_binding_assumption(
					judgement,
					terms,
					contexts,
					branch_context,
					field_binding,
					field_classifier,
					&field_claim
				) != 0 || prototype_term_var(
					terms, field_binding, &field
				) != 0 || prototype_term_app(
					terms, endpoint, field, &endpoint
				) != 0 || prototype_term_app(
					terms, branch_witness, field, &branch_witness
				) != 0 || prototype_term_app(
					terms, branch_witness, field, &branch_witness
				) != 0) {
				return -1;
			}
			branch_argument_claims[i][argument_count++] = field_claim;
			branch_argument_claims[i][argument_count++] = field_claim;
			uint32_t field_identity_family_arguments[2] = { field, field };
			uint32_t field_identity_family;
			uint32_t field_identity_witness;
			if (recursive) {
				if (ih_scope == PROTOTYPE_INVALID_ID) {
					ih_scope = prototype_term_new_ih_scope(terms);
				}
				if (ih_scope == PROTOTYPE_INVALID_ID ||
					pending_ih_count >= 256 || prototype_term_type_instance_make(
						terms,
						types,
						generated_type_id,
						field_identity_family_arguments,
						2,
						&field_identity_family
					) != 0 || prototype_term_induction_hypothesis(
						terms, ih_scope, field, &field_identity_witness
					) != 0) {
					return -1;
				}
				pending_ih[pending_ih_count++] =
					(struct hott_pending_identity_ih) {
						.branch_index = i,
						.field_index = j,
						.argument_claim_index = argument_count,
						.context_id = branch_context,
						.term = field_identity_witness,
						.classifier = field_identity_family
					};
				branch_argument_claims[i][argument_count++] =
					PROTOTYPE_INVALID_ID;
			} else {
				uint32_t field_generated_type_id;
				uint32_t field_identity_family_claim;
				uint32_t field_identity_witness_claim;
				if (prototype_type_declaration_find_generated_identity(
						types,
						field_classifier,
						prototype_context_empty(contexts),
						&field_generated_type_id
					) != 0) {
					return 1;
				}
				int status = hott_construct_adt_degeneracy_depth(
					kernel,
					field_claim,
					field_generated_type_id,
					universe,
					remaining_depth - 1,
					&field_identity_family_claim,
					&field_identity_witness,
					&field_identity_witness_claim
				);
				if (status != 0) {
					return status;
				}
				branch_argument_claims[i][argument_count++] =
					field_identity_witness_claim;
				(void)field_identity_family_claim;
			}
			if (prototype_term_app(
					terms,
					branch_witness,
					field_identity_witness,
					&branch_witness
				) != 0) {
				return -1;
			}
			(void)context_certificate;
		}
		uint32_t branch_arguments[2] = { endpoint, endpoint };
		uint32_t branch_family;
		if (prototype_term_type_instance_make(
				terms,
				types,
				generated_type_id,
				branch_arguments,
				2,
				&branch_family
			) != 0) {
			return -1;
		}
		if (field_count == 0 && prototype_term_constructor(
				terms, branch_family, i, &branch_witness
			) != 0) {
			return -1;
		}
		cases[i] = (struct prototype_match_case_input) {
			.case_label_symbol_id = -1,
			.constructor_owner = source->classifier,
			.constructor_id = i,
			.binders = field_count == 0 ? NULL : branch_binders,
			.binder_count = field_count,
			.body = branch_witness
		};
		branch_contexts[i] = branch_context;
		branch_families[i] = branch_family;
		branch_argument_counts[i] = argument_count;
		branch_claims[i] = PROTOTYPE_INVALID_ID;
		case_binder_cursor += field_count;
	}
	uint32_t witness;
	uint32_t match_claim;
	uint32_t witness_claim;
	int match_status = ih_scope == PROTOTYPE_INVALID_ID ? prototype_term_match(
		terms,
		source->subject,
		cases,
		source_type->constructor_count,
		&witness
	) : prototype_term_match_with_ih_scope(
		terms,
		source->subject,
		cases,
		source_type->constructor_count,
		ih_scope,
		&witness
	);
	if (match_status != 0 || (ih_scope != PROTOTYPE_INVALID_ID &&
		prototype_term_set_ih_scope_term(terms, ih_scope, witness) != 0)) {
		return -1;
	}
	const struct prototype_term* stored_match = &terms->terms[witness];
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t case_id = stored_match->as.match.first_case + i;
		if (case_id >= terms->case_count ||
			terms->cases[case_id].binder_count != cases[i].binder_count ||
			terms->cases[case_id].body != cases[i].body) {
			return 1;
		}
		for (uint32_t j = 0; j < cases[i].binder_count; ++j) {
			if (terms->case_binders[
					terms->cases[case_id].first_binder + j
				].binding_id != cases[i].binders[j].binding_id) {
				return 1;
			}
		}
	}
	for (uint32_t i = 0; i < pending_ih_count; ++i) {
		uint32_t claim;
		if (prototype_judgement_add_induction_hypothesis_claim(
				judgement,
				terms,
				types,
				pending_ih[i].context_id,
				pending_ih[i].term,
				pending_ih[i].classifier,
				witness,
				motive,
				pending_ih[i].branch_index,
				pending_ih[i].field_index,
				&claim
			) != 0) {
			return -1;
		}
		branch_argument_claims[
			pending_ih[i].branch_index
		][pending_ih[i].argument_claim_index] = claim;
	}
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		for (uint32_t j = 0; j < branch_argument_counts[i]; ++j) {
			uint32_t claim_id = branch_argument_claims[i][j];
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(judgement, claim_id);
			if (!proposition) {
				return -1;
			}
			if (proposition->context_id != branch_contexts[i]) {
				uint32_t projection;
				uint32_t reindexed;
				if (prototype_substitution_projection_path(
						substitutions,
						contexts,
						branch_contexts[i],
						proposition->context_id,
						&projection
					) != 0 || prototype_judgement_add_reindexed_claim(
						judgement,
						terms,
						types,
						contexts,
						substitutions,
						claim_id,
						projection,
						&reindexed
					) != 0) {
					return -1;
				}
				branch_argument_claims[i][j] = reindexed;
			}
		}
		if (branch_argument_counts[i] == 0) {
			if (prototype_judgement_add_constructor_intro_claim(
					judgement,
					terms,
					types,
					branch_contexts[i],
					cases[i].body,
					branch_families[i],
					&branch_claims[i]
				) != 0) {
				return -1;
			}
		} else if (prototype_judgement_add_constructor_spine_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				branch_contexts[i],
				cases[i].body,
				branch_families[i],
				branch_argument_claims[i],
				branch_argument_counts[i],
				&branch_claims[i]
			) != 0) {
			return -1;
		}
	}
	if (prototype_judgement_add_match_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			kernel->operations,
			source->context_id,
			witness,
			match_classifier,
			match_classifier_claim,
			branch_claims,
			source_type->constructor_count,
			&match_claim
		) != 0 || prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			source->context_id,
			witness,
			identity_family,
			match_claim,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)identity_family_claim;
	(void)motive_context_certificate;
	return 0;
}

static int hott_construct_adt_degeneracy_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	uint32_t generated_type_id,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement || !source || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id || remaining_depth == 0 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->subject >= terms->term_count ||
		source->classifier >= terms->term_count || universe >= terms->term_count ||
		generated_type_id >= type_declarations->type_count ||
		!prototype_type_declaration_validate_generated_identity(
			terms,
			type_declarations,
			contexts,
			source->classifier,
			generated_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT
		)) {
		return -1;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t source_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t source_argument_count;
	int source_is_constructor = prototype_term_constructor_spine_info(
			terms,
			source->subject,
			&head,
			&owner,
			&constructor_index,
			source_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&source_argument_count
		) == 0;
	uint32_t generated_constructor_index = PROTOTYPE_INVALID_ID;
	if (source_is_constructor &&
		(hott_generated_constructor_index_for_source_ordinal(
			terms,
			type_declarations,
			generated_type_id,
			source->classifier,
			constructor_index,
			&generated_constructor_index
		) != 0 || source_argument_count * 3 >
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES)) {
		return 1;
	}
	uint32_t identity_arguments[2] = { source->subject, source->subject };
	uint32_t identity_family;
	uint32_t identity_family_claim;
	if (prototype_term_type_instance_make(
			terms, type_declarations, generated_type_id,
			identity_arguments, 2, &identity_family
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement, terms, type_declarations, source->context_id,
			identity_family, universe, &identity_family_claim
		) != 0) {
		return -1;
	}
	if (!source_is_constructor) {
		uint32_t witness;
		uint32_t witness_claim;
		int status = hott_construct_adt_degeneracy_match(
			kernel,
			source_claim_id,
			generated_type_id,
			universe,
			identity_family,
			identity_family_claim,
			remaining_depth,
			&witness,
			&witness_claim
		);
		if (status == 0) {
			*p_identity_family_claim_id = identity_family_claim;
			*p_witness_term_id = witness;
			*p_witness_claim_id = witness_claim;
		}
		return status;
	}
	uint32_t witness;
	if (source_argument_count == 0) {
		uint32_t witness_claim;
		if (prototype_term_constructor(
				terms, identity_family, generated_constructor_index, &witness
			) != 0 || prototype_judgement_add_constructor_intro_claim(
				judgement, terms, type_declarations, source->context_id,
				witness, identity_family, &witness_claim
			) != 0) {
			return -1;
		}
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = witness;
		*p_witness_claim_id = witness_claim;
		(void)head;
		(void)owner;
		return 0;
	}
	const struct prototype_judgement_derivation* source_derivation =
		hott_constructor_derivation_for_claim(judgement, source_claim_id);
	if (!source_derivation || source_derivation->premise_count !=
		source_argument_count) {
		return 1;
	}
	uint32_t identity_former;
	if (prototype_term_type_instance_make(
			terms, type_declarations, generated_type_id, NULL, 0, &identity_former
		) != 0 || prototype_term_constructor(
			terms, identity_former, generated_constructor_index, &witness
		) != 0) {
		return -1;
	}
	uint32_t argument_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count = 0;
	for (uint32_t i = 0; i < source_argument_count; ++i) {
		uint32_t field_claim_id = source_derivation->premises[i].claim_id;
		const struct prototype_judgement_proposition* field =
			prototype_judgement_claim_proposition(judgement, field_claim_id);
		if (!field || field->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			field->context_id != source->context_id ||
			field->subject != source_arguments[i]) {
			return -1;
		}
		uint32_t field_generated_type_id;
		if (prototype_type_declaration_find_generated_identity(
				type_declarations,
				field->classifier,
				prototype_context_empty(contexts),
				&field_generated_type_id
			) != 0) {
			return 1;
		}
		uint32_t field_family_claim;
		uint32_t field_witness;
		uint32_t field_witness_claim;
		int status = hott_construct_adt_degeneracy_depth(
			kernel, field_claim_id, field_generated_type_id, universe,
			remaining_depth - 1, &field_family_claim, &field_witness,
			&field_witness_claim
		);
		if (status != 0) {
			return status;
		}
		if (prototype_term_app(
				terms, witness, field->subject, &witness
			) != 0 || prototype_term_app(
				terms, witness, field->subject, &witness
			) != 0 || prototype_term_app(
				terms, witness, field_witness, &witness
			) != 0) {
			return -1;
		}
		argument_claims[argument_count++] = field_claim_id;
		argument_claims[argument_count++] = field_claim_id;
		argument_claims[argument_count++] = field_witness_claim;
		(void)field_family_claim;
	}
	uint32_t witness_claim;
	if (prototype_judgement_add_constructor_spine_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			source->context_id, witness, identity_family,
			argument_claims, argument_count, &witness_claim
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)head;
	(void)owner;
	return 0;
}

struct hott_object_action_environment {
	uint32_t left_substitution_id;
	uint32_t right_substitution_id;
	uint32_t target_context_id;
	const uint32_t* source_binding_ids;
	const uint32_t* identity_claim_ids;
	uint32_t binding_count;
};

static int hott_build_object_action_environment(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t bridge_id,
	uint32_t* source_bindings,
	uint32_t* identity_claims,
	uint32_t capacity,
	struct hott_object_action_environment* p_environment
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_hott_bridge* bridge =
		prototype_hott_bridge_db_get(bridges, bridge_id);
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!bridges || !bridge || !source_bindings || !identity_claims ||
		!p_environment) {
		return -1;
	}
	uint32_t count = 0;
	const struct prototype_hott_bridge* current = bridge;
	while (current->source_context_id != prototype_context_empty(contexts)) {
		if (current->id >= bridges->certificate_count || count >= capacity) {
			return -1;
		}
		const struct prototype_hott_bridge_certificate* certificate =
			&bridges->certificates[current->id];
		const struct prototype_context* source_context = prototype_context_get(
			contexts, current->source_context_id
		);
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, certificate->fiber_witness_claim_id
			);
		if (!source_context || !witness || certificate->semantics !=
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
			certificate->parent_bridge_id == PROTOTYPE_INVALID_ID ||
			witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			witness->context_id != current->bridge_context_id) {
			return -1;
		}
		uint32_t witness_claim = certificate->fiber_witness_claim_id;
		if (current->bridge_context_id != bridge->bridge_context_id) {
			uint32_t projection;
			if (prototype_substitution_projection_path(
					substitutions,
					contexts,
					bridge->bridge_context_id,
					current->bridge_context_id,
					&projection
				) != 0 || prototype_judgement_add_reindexed_claim(
					judgement,
					terms,
					types,
					contexts,
					substitutions,
					witness_claim,
					projection,
					&witness_claim
				) != 0) {
				return -1;
			}
		}
		source_bindings[count] = source_context->binding_id;
		identity_claims[count] = witness_claim;
		count++;
		current = prototype_hott_bridge_db_get(
			bridges, certificate->parent_bridge_id
		);
		if (!current) {
			return -1;
		}
	}
	if (current->id >= bridges->certificate_count ||
		bridges->certificates[current->id].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY) {
		return -1;
	}
	*p_environment = (struct hott_object_action_environment) {
		.left_substitution_id = bridge->left_substitution_id,
		.right_substitution_id = bridge->right_substitution_id,
		.target_context_id = bridge->bridge_context_id,
		.source_binding_ids = source_bindings,
		.identity_claim_ids = identity_claims,
		.binding_count = count
	};
	return 0;
}

static int hott_instantiate_object_identity_family(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t target_context_id,
	uint32_t left_claim_id,
	uint32_t right_claim_id,
	uint32_t* p_identity_family_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* left = judgement ?
		prototype_judgement_claim_proposition(judgement, left_claim_id) : NULL;
	const struct prototype_judgement_proposition* right = judgement ?
		prototype_judgement_claim_proposition(judgement, right_claim_id) : NULL;
	const struct prototype_context* endpoint = identity && contexts ?
		prototype_context_get(contexts, identity->endpoint_context_id) : NULL;
	const struct prototype_context* left_endpoint = endpoint ?
		prototype_context_get(contexts, endpoint->parent) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!identity || !left || !right || !endpoint || !left_endpoint ||
		!p_identity_family_claim_id || left->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || right->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || left->context_id !=
			target_context_id || right->context_id != target_context_id ||
		left_endpoint->parent != target_context_id ||
		left_endpoint->binding_id != identity->left_endpoint_binding_id ||
		endpoint->binding_id != identity->right_endpoint_binding_id) {
		return -1;
	}
	uint32_t expected_left_classifier = prototype_context_classifier_term(
		left_endpoint
	);
	uint32_t expected_right_classifier = prototype_context_classifier_term(endpoint);
	if (left->classifier != expected_left_classifier) {
		uint32_t converted_left_claim;
		if (prototype_judgement_classifier_conversion(
				terms, types, left->classifier, expected_left_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				target_context_id,
				left->subject,
				expected_left_classifier,
				left_claim_id,
				&converted_left_claim
			) != 0) {
			return -1;
		}
		left_claim_id = converted_left_claim;
		left = prototype_judgement_claim_proposition(judgement, left_claim_id);
	}
	if (right->classifier != expected_right_classifier) {
		uint32_t converted_right_claim;
		if (prototype_judgement_classifier_conversion(
				terms, types, right->classifier, expected_right_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				target_context_id,
				right->subject,
				expected_right_classifier,
				right_claim_id,
				&converted_right_claim
			) != 0) {
			return -1;
		}
		right_claim_id = converted_right_claim;
		right = prototype_judgement_claim_proposition(judgement, right_claim_id);
	}
	if (!left || !right || left->classifier != expected_left_classifier ||
		right->classifier != expected_right_classifier) {
		return -1;
	}

	uint32_t base_substitution;
	uint32_t left_substitution;
	uint32_t endpoint_substitution;
	uint32_t left_substitution_certificate;
	uint32_t right_substitution_certificate;
	if (prototype_substitution_identity(
			substitutions, contexts, target_context_id, &base_substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			base_substitution,
			endpoint->parent,
			left->subject,
			left->classifier,
			&left_substitution
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			left_substitution,
			left_claim_id,
			&left_substitution_certificate
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			left_substitution,
			identity->endpoint_context_id,
			right->subject,
			right->classifier,
			&endpoint_substitution
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			endpoint_substitution,
			right_claim_id,
			&right_substitution_certificate
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			identity->identity_type_has_type_claim_id,
			endpoint_substitution,
			p_identity_family_claim_id
		) != 0) {
		return -1;
	}
	(void)left_substitution_certificate;
	(void)right_substitution_certificate;
	return 0;
}

static int hott_construct_object_variable_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!identity || !source || !environment || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source->subject >=
			terms->term_count || terms->terms[source->subject].tag !=
			PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t source_binding = terms->terms[source->subject].as.var.binding_id;
	uint32_t mapped_identity_claim_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < environment->binding_count; ++i) {
		if (environment->source_binding_ids[i] == source_binding) {
			mapped_identity_claim_id = environment->identity_claim_ids[i];
			break;
		}
	}
	if (mapped_identity_claim_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	uint32_t identity_family_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim_id
		) != 0 || hott_instantiate_object_identity_family(
			kernel,
			identity,
			environment->target_context_id,
			left_claim_id,
			right_claim_id,
			&identity_family_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* family =
		prototype_judgement_claim_proposition(
			judgement, identity_family_claim_id
		);
	const struct prototype_judgement_proposition* mapped =
		prototype_judgement_claim_proposition(
			judgement, mapped_identity_claim_id
		);
	if (!family || !mapped || mapped->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || mapped->context_id !=
			environment->target_context_id || prototype_judgement_classifier_conversion(
			terms, types, mapped->classifier, family->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	uint32_t witness_claim_id = mapped_identity_claim_id;
	if (mapped->classifier != family->subject &&
		prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			mapped->subject,
			family->subject,
			mapped_identity_claim_id,
			&witness_claim_id
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim_id;
	*p_witness_term_id = mapped->subject;
	*p_witness_claim_id = witness_claim_id;
	return 0;
}

static int hott_prepare_local_object_identity(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	struct prototype_hott_identity_type_computation_certificate* p_identity
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!source || !environment || !p_identity || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || universe >= terms->term_count) {
		return -1;
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left =
		prototype_judgement_claim_proposition(judgement, left_claim_id);
	const struct prototype_judgement_proposition* right =
		prototype_judgement_claim_proposition(judgement, right_claim_id);
	if (!left || !right || left->context_id != environment->target_context_id ||
		right->context_id != environment->target_context_id ||
		prototype_judgement_classifier_conversion(
			terms, types, left->classifier, right->classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	uint32_t carrier_type_claim;
	uint32_t right_carrier_type_claim;
	uint32_t source_carrier_type_claim = hott_is_type_claim_for_subject(
		judgement, source->context_id, source->classifier
	);
	if (source_carrier_type_claim != PROTOTYPE_INVALID_ID) {
		const struct prototype_judgement_proposition* left_carrier;
		const struct prototype_judgement_proposition* right_carrier;
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source_carrier_type_claim,
				environment->left_substitution_id,
				&carrier_type_claim
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source_carrier_type_claim,
				environment->right_substitution_id,
				&right_carrier_type_claim
			) != 0 || !(left_carrier = prototype_judgement_claim_proposition(
				judgement, carrier_type_claim
			)) || !(right_carrier = prototype_judgement_claim_proposition(
				judgement, right_carrier_type_claim
			)) || left_carrier->subject != left->classifier ||
			right_carrier->subject != right->classifier) {
			return -1;
		}
	} else if (hott_ensure_is_type_claim_in_context(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				environment->target_context_id,
				left->classifier,
				&carrier_type_claim
			) != 0) {
		return 1;
	} else if (hott_ensure_is_type_claim_in_context(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				environment->target_context_id,
				right->classifier,
				&right_carrier_type_claim
			) != 0) {
		return 1;
	}
	const struct prototype_judgement_proposition* carrier_type =
		prototype_judgement_claim_proposition(judgement, carrier_type_claim);
	const struct prototype_judgement_proposition* right_carrier_type =
		prototype_judgement_claim_proposition(judgement, right_carrier_type_claim);
	if (!carrier_type || !right_carrier_type ||
		carrier_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		right_carrier_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		carrier_type->classifier >= terms->term_count ||
		terms->terms[carrier_type->classifier].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		prototype_judgement_classifier_conversion(
			terms,
			types,
			carrier_type->classifier,
			right_carrier_type->classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	uint32_t local_universe = carrier_type->classifier;
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t endpoint_context;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	uint32_t left_projection;
	uint32_t right_carrier_claim;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			environment->target_context_id,
			left_binding,
			left->classifier,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			left_context,
			carrier_type_claim,
			&left_context_certificate
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			left_context,
			environment->target_context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_carrier_type_claim,
			left_projection,
			&right_carrier_claim
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			right->classifier,
			PROTOTYPE_INVALID_ID,
			&endpoint_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			endpoint_context,
			right_carrier_claim,
			&right_context_certificate
		) != 0) {
		return -1;
	}
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_term_var(
			terms, left_binding, &left_endpoint
		) != 0 || prototype_term_var(
			terms, right_binding, &right_endpoint
		) != 0) {
		return -1;
	}
	uint32_t generated_type_id = PROTOTYPE_INVALID_ID;
	uint32_t identity_type;
	uint32_t x0_binding = PROTOTYPE_INVALID_ID;
	uint32_t x1_binding = PROTOTYPE_INVALID_ID;
	uint32_t xr_binding = PROTOTYPE_INVALID_ID;
	uint32_t normalized_carrier;
	int computation_rule;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			types,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			left->classifier,
			&normalized_carrier
		) != 0) {
		return 1;
	}
	if (prototype_type_declaration_find_generated_identity(
			types,
			normalized_carrier,
			prototype_context_empty(contexts),
			&generated_type_id
		) == 0) {
		uint32_t endpoints[2] = { left_endpoint, right_endpoint };
		computation_rule =
			prototype_type_declaration_generated_identity_rule_for_source(
				terms, normalized_carrier
			);
		if (computation_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID ||
			prototype_term_type_instance_make(
				terms,
				types,
				generated_type_id,
				endpoints,
				2,
				&identity_type
			) != 0) {
			return 1;
		}
	} else {
		computation_rule = PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE;
		x0_binding = prototype_term_new_binding(terms);
		x1_binding = prototype_term_new_binding(terms);
		xr_binding = prototype_term_new_binding(terms);
		if (x0_binding == PROTOTYPE_INVALID_ID ||
			x1_binding == PROTOTYPE_INVALID_ID ||
			xr_binding == PROTOTYPE_INVALID_ID ||
			hott_build_nondependent_pi_identity_type(
				terms,
				types,
				prototype_context_empty(contexts),
				normalized_carrier,
				left_binding,
				right_binding,
				x0_binding,
				x1_binding,
				xr_binding,
				&identity_type
			) != 0) {
			return 1;
		}
	}
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			endpoint_context,
			identity_type,
			local_universe,
			&identity_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoint_context,
			identity_type,
			local_universe,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		return -1;
	}
	*p_identity = (struct prototype_hott_identity_type_computation_certificate) {
		.computation_rule = computation_rule,
		.endpoint_context_id = endpoint_context,
		.left_endpoint_binding_id = left_binding,
		.right_endpoint_binding_id = right_binding,
		.generated_type_declaration_id = generated_type_id,
		.backing_type_former_term_id = PROTOTYPE_INVALID_ID,
		.backing_type_former_has_type_claim_id = PROTOTYPE_INVALID_ID,
		.identity_type_term_id = identity_type,
		.identity_type_has_type_claim_id = identity_type_claim,
		.identity_type_is_type_claim_id = identity_type_is_type_claim,
		.left_context_certificate_id = left_context_certificate,
		.right_context_certificate_id = right_context_certificate,
		.pointwise_left_input_binding_id = x0_binding,
		.pointwise_right_input_binding_id = x1_binding,
		.pointwise_input_identity_binding_id = xr_binding
	};
	return 0;
}

static int hott_construct_object_value_action_depth(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
);

static int hott_construct_object_computation_action_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_computation_id,
	uint32_t* p_witness_computation_claim_id
);

static int hott_construct_nullary_match_object_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	const struct prototype_term* source_match = source && source->subject <
		terms->term_count ? &terms->terms[source->subject] : NULL;
	const struct prototype_judgement_derivation* match_derivation = judgement ?
		hott_derivation_for_claim_and_kind(
			judgement, source_claim_id, PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!identity || !source || !source_match || !environment ||
		!p_left_claim_id || !p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_term_id ||
		!p_witness_claim_id || remaining_depth == 0 || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	if (source_match->tag != PROTOTYPE_TERM_MATCH || !match_derivation ||
		source_match->as.match.case_count == 0 ||
		source_match->as.match.case_count + 1 != match_derivation->premise_count ||
		identity->generated_type_declaration_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	for (uint32_t i = 0; i < source_match->as.match.case_count; ++i) {
		uint32_t case_id = source_match->as.match.first_case + i;
		if (case_id >= terms->case_count || terms->cases[case_id].binder_count != 0) {
			return 1;
		}
	}

	/* The accepted motive application contains the exact scrutinee Claim. */
	uint32_t classifier_claim_id = match_derivation->premises[0].claim_id;
	const struct prototype_judgement_derivation* classifier_derivation =
		hott_derivation_for_claim_and_kind(
			judgement, classifier_claim_id, PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
		);
	if (!classifier_derivation || classifier_derivation->premise_count != 2) {
		return 1;
	}
	uint32_t scrutinee_claim_id = classifier_derivation->premises[1].claim_id;
	const struct prototype_judgement_proposition* scrutinee =
		prototype_judgement_claim_proposition(judgement, scrutinee_claim_id);
	if (!scrutinee || scrutinee->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		scrutinee->context_id != source->context_id ||
		scrutinee->subject != source_match->as.match.scrutinee) {
		return 1;
	}

	uint32_t left_claim_id;
	uint32_t right_claim_id;
	uint32_t identity_family_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement, terms, types, contexts, substitutions, source_claim_id,
			environment->left_substitution_id, &left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, terms, types, contexts, substitutions, source_claim_id,
			environment->right_substitution_id, &right_claim_id
		) != 0 || hott_instantiate_object_identity_family(
			kernel, identity, environment->target_context_id, left_claim_id,
			right_claim_id, &identity_family_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* identity_family =
		prototype_judgement_claim_proposition(
			judgement, identity_family_claim_id
		);
	if (!identity_family || identity_family->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || identity_family->context_id !=
			environment->target_context_id) {
		return -1;
	}

	uint32_t scrutinee_left_claim;
	uint32_t scrutinee_right_claim;
	uint32_t scrutinee_family_claim;
	uint32_t scrutinee_witness;
	uint32_t scrutinee_witness_claim;
	int scrutinee_status = hott_construct_object_value_action_depth(
		kernel,
		NULL,
		scrutinee_claim_id,
		environment,
		universe,
		remaining_depth - 1,
		&scrutinee_left_claim,
		&scrutinee_right_claim,
		&scrutinee_family_claim,
		&scrutinee_witness,
		&scrutinee_witness_claim
	);
	if (scrutinee_status != 0) {
		return scrutinee_status;
	}
	const struct prototype_judgement_proposition* scrutinee_family =
		prototype_judgement_claim_proposition(judgement, scrutinee_family_claim);
	if (!scrutinee_family || scrutinee_family->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || scrutinee_family->context_id !=
			environment->target_context_id || scrutinee_witness >=
			terms->term_count || terms->terms[scrutinee_witness].tag !=
			PROTOTYPE_TERM_VAR) {
		return 1;
	}

	uint32_t source_type_id;
	uint32_t source_type_arguments[16];
	uint32_t source_type_argument_count;
	uint32_t generated_type_id;
	uint32_t generated_type_arguments[16];
	uint32_t generated_type_argument_count;
	if (prototype_term_type_instance_info(
			terms, scrutinee->classifier, &source_type_id, source_type_arguments,
			&source_type_argument_count
		) != 0 || source_type_id >= types->type_count ||
		prototype_term_type_instance_info(
			terms, scrutinee_family->subject, &generated_type_id,
			generated_type_arguments, &generated_type_argument_count
		) != 0 || generated_type_id >= types->type_count ||
		generated_type_argument_count != 2) {
		return 1;
	}
	const struct prototype_type_declaration* source_type =
		&types->type_declarations[source_type_id];
	if (source_type->constructor_count != source_match->as.match.case_count ||
		source_type->constructor_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return 1;
	}

	uint32_t branch_witness_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_witness_terms[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t generated_constructor_indices[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t case_id = source_match->as.match.first_case + i;
		const struct prototype_judgement_proposition* branch =
			prototype_judgement_claim_proposition(
				judgement, match_derivation->premises[i + 1].claim_id
			);
		if (!branch || branch->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			branch->context_id != source->context_id || branch->subject !=
				terms->cases[case_id].body) {
			return 1;
		}
		int constructor_status = hott_generated_constructor_index_for_source_ordinal(
			terms,
			types,
			generated_type_id,
			scrutinee->classifier,
			terms->cases[case_id].constructor_id,
			&generated_constructor_indices[i]
		);
		if (constructor_status != 0) {
			return constructor_status < 0 ? -1 : 1;
		}
		uint32_t branch_left_claim;
		uint32_t branch_right_claim;
		uint32_t branch_family_claim;
		int branch_status = hott_construct_object_value_action_depth(
			kernel,
			NULL,
			match_derivation->premises[i + 1].claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&branch_left_claim,
			&branch_right_claim,
			&branch_family_claim,
			&branch_witness_terms[i],
			&branch_witness_claims[i]
		);
		if (branch_status != 0) {
			return branch_status;
		}
		(void)branch_left_claim;
		(void)branch_right_claim;
		(void)branch_family_claim;
	}

	struct prototype_match_case_input cases[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		cases[i] = (struct prototype_match_case_input) {
			.case_label_symbol_id = -1,
			.constructor_owner = scrutinee_family->subject,
			.constructor_id = generated_constructor_indices[i],
			.binders = NULL,
			.binder_count = 0,
			.body = branch_witness_terms[i]
		};
	}
	uint32_t witness;
	if (prototype_term_match(
			terms, scrutinee_witness, cases, source_type->constructor_count,
			&witness
		) != 0) {
		return -1;
	}

	uint32_t family_is_type_claim;
	if (prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			environment->target_context_id,
			scrutinee_family->subject,
			universe,
			scrutinee_family_claim,
			&family_is_type_claim
		) != 0) {
		return -1;
	}
	uint32_t motive_binding = prototype_term_new_binding(terms);
	uint32_t motive_context;
	uint32_t motive_context_certificate;
	uint32_t motive_projection;
	uint32_t motive_body_claim;
	uint32_t motive_binder_claim;
	uint32_t motive;
	uint32_t motive_classifier;
	uint32_t motive_claim;
	if (motive_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			environment->target_context_id,
			motive_binding,
			scrutinee_family->subject,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			motive_context,
			family_is_type_claim,
			&motive_context_certificate
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			motive_context,
			environment->target_context_id,
			&motive_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			identity_family_claim_id,
			motive_projection,
			&motive_body_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			motive_context,
			motive_binding,
			scrutinee_family->subject,
			&motive_binder_claim
		) != 0 || prototype_term_lambda(
			terms, motive_binding, identity_family->subject, &motive
		) != 0 || prototype_term_pi(
			terms, scrutinee_family->subject, universe, &motive_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			environment->target_context_id,
			motive,
			motive_classifier,
			motive_binder_claim,
			motive_body_claim,
			&motive_claim
		) != 0) {
		return -1;
	}
	uint32_t match_classifier;
	uint32_t match_classifier_claim;
	if (prototype_term_app(
			terms, motive, scrutinee_witness, &match_classifier
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			environment->target_context_id,
			match_classifier,
			universe,
			motive_claim,
			scrutinee_witness_claim,
			&match_classifier_claim
		) != 0) {
		return -1;
	}

	uint32_t refined_branch_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t refinement_substitutions[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t refined_context;
		uint32_t constructor_term;
		if (prototype_judgement_indexed_branch_refinement(
				contexts,
				substitutions,
				terms,
				types,
				environment->target_context_id,
				scrutinee_witness,
				scrutinee_family->subject,
				generated_constructor_indices[i],
				environment->target_context_id,
				NULL,
				0,
				&refined_context,
				&refinement_substitutions[i],
				&constructor_term
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				branch_witness_claims[i],
				refinement_substitutions[i],
				&refined_branch_claims[i]
			) != 0) {
			return 1;
		}
		(void)refined_context;
		(void)constructor_term;
	}
	uint32_t witness_claim;
	uint32_t converted_witness_claim;
	if (prototype_judgement_add_indexed_match_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			kernel->operations,
			environment->target_context_id,
			witness,
			match_classifier,
			match_classifier_claim,
			refined_branch_claims,
			refinement_substitutions,
			source_type->constructor_count,
			&witness_claim
		) != 0 || prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			witness,
			identity_family->subject,
			witness_claim,
			&converted_witness_claim
		) != 0) {
		return -1;
	}
	*p_left_claim_id = left_claim_id;
	*p_right_claim_id = right_claim_id;
	*p_identity_family_claim_id = identity_family_claim_id;
	*p_witness_term_id = witness;
	*p_witness_claim_id = converted_witness_claim;
	(void)scrutinee_left_claim;
	(void)scrutinee_right_claim;
	(void)motive_context_certificate;
	return 0;
}

static int hott_construct_adt_value_action(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	const struct prototype_substitution* left = substitutions ?
		prototype_substitution_get(
			substitutions,
			environment ? environment->left_substitution_id : PROTOTYPE_INVALID_ID
		) : NULL;
	const struct prototype_substitution* right = substitutions ?
		prototype_substitution_get(
			substitutions,
			environment ? environment->right_substitution_id : PROTOTYPE_INVALID_ID
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!source || !environment || !left || !right || !p_left_claim_id ||
		!p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_term_id ||
		!p_witness_claim_id || remaining_depth == 0 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		left->source_context != environment->target_context_id ||
		right->source_context != environment->target_context_id ||
		left->target_context != source->context_id ||
		right->target_context != source->context_id ||
		(environment->binding_count > 0 && (!environment->source_binding_ids ||
			!environment->identity_claim_ids))) {
		return -1;
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_endpoint =
		prototype_judgement_claim_proposition(judgement, left_claim_id);
	const struct prototype_judgement_proposition* right_endpoint =
		prototype_judgement_claim_proposition(judgement, right_claim_id);
	if (!left_endpoint || !right_endpoint ||
		left_endpoint->context_id != environment->target_context_id ||
		right_endpoint->context_id != environment->target_context_id ||
		prototype_judgement_classifier_conversion(
			terms,
			types,
			left_endpoint->classifier,
			right_endpoint->classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	uint32_t generated_type_id;
	if (prototype_type_declaration_find_generated_identity(
			types,
			left_endpoint->classifier,
			prototype_context_empty(contexts),
			&generated_type_id
		) != 0) {
		return 1;
	}
	uint32_t identity_arguments[2] = {
		left_endpoint->subject,
		right_endpoint->subject
	};
	uint32_t identity_family;
	uint32_t identity_family_claim;
	if (prototype_term_type_instance_make(
			terms,
			types,
			generated_type_id,
			identity_arguments,
			2,
			&identity_family
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			identity_family,
			universe,
			&identity_family_claim
		) != 0) {
		return -1;
	}
	if (source->subject < terms->term_count &&
		terms->terms[source->subject].tag == PROTOTYPE_TERM_VAR) {
		uint32_t source_binding = terms->terms[source->subject].as.var.binding_id;
		uint32_t mapped_identity_claim_id = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < environment->binding_count; ++i) {
			if (environment->source_binding_ids[i] == source_binding) {
				mapped_identity_claim_id = environment->identity_claim_ids[i];
				break;
			}
		}
		if (mapped_identity_claim_id == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		const struct prototype_judgement_proposition* mapped =
			prototype_judgement_claim_proposition(
				judgement, mapped_identity_claim_id
			);
		uint32_t witness_claim = mapped_identity_claim_id;
		if (!mapped || mapped->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			mapped->context_id != environment->target_context_id ||
			prototype_judgement_classifier_conversion(
				terms, types, mapped->classifier, identity_family
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		if (mapped->classifier != identity_family &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				environment->target_context_id,
				mapped->subject,
				identity_family,
				mapped_identity_claim_id,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = mapped->subject;
		*p_witness_claim_id = witness_claim;
		return 0;
	}

	uint32_t source_head;
	uint32_t source_owner;
	uint32_t constructor_ordinal;
	uint32_t source_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t source_argument_count;
	if (prototype_term_constructor_spine_info(
			terms,
			source->subject,
			&source_head,
			&source_owner,
			&constructor_ordinal,
			source_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&source_argument_count
		) != 0) {
		return 1;
	}
	const struct prototype_type_declaration* generated =
		generated_type_id < types->type_count ?
		&types->type_declarations[generated_type_id] : NULL;
	uint32_t generated_constructor_index;
	int generated_constructor_status = generated ?
		hott_generated_constructor_index_for_source_ordinal(
			terms,
			types,
			generated_type_id,
			left_endpoint->classifier,
			constructor_ordinal,
			&generated_constructor_index
		) : -1;
	if (!generated || source_owner != left_endpoint->classifier ||
		generated_constructor_status < 0 ||
		source_argument_count * 3 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	if (generated_constructor_status > 0) {
		return 1;
	}
	uint32_t witness;
	if (source_argument_count == 0) {
		uint32_t witness_claim;
		if (prototype_term_constructor(
				terms, identity_family, generated_constructor_index, &witness
			) != 0 || prototype_judgement_add_constructor_intro_claim(
				judgement,
				terms,
				types,
				environment->target_context_id,
				witness,
				identity_family,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = witness;
		*p_witness_claim_id = witness_claim;
		return 0;
	}
	const struct prototype_judgement_derivation* source_derivation =
		hott_constructor_derivation_for_claim(judgement, source_claim_id);
	if (!source_derivation ||
		source_derivation->premise_count != source_argument_count) {
		return 1;
	}
	uint32_t identity_former;
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, NULL, 0, &identity_former
		) != 0 || prototype_term_constructor(
			terms, identity_former, generated_constructor_index, &witness
		) != 0) {
		return -1;
	}
	uint32_t argument_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count = 0;
	for (uint32_t i = 0; i < source_argument_count; ++i) {
		uint32_t field_left_claim;
		uint32_t field_right_claim;
		uint32_t field_family_claim;
		uint32_t field_witness;
		uint32_t field_witness_claim;
		int status = hott_construct_object_value_action_depth(
			kernel,
			NULL,
			source_derivation->premises[i].claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&field_left_claim,
			&field_right_claim,
			&field_family_claim,
			&field_witness,
			&field_witness_claim
		);
		if (status != 0) {
			return status;
		}
		const struct prototype_judgement_proposition* field_left =
			prototype_judgement_claim_proposition(judgement, field_left_claim);
		const struct prototype_judgement_proposition* field_right =
			prototype_judgement_claim_proposition(judgement, field_right_claim);
		if (!field_left || !field_right || prototype_term_app(
				terms, witness, field_left->subject, &witness
			) != 0 || prototype_term_app(
				terms, witness, field_right->subject, &witness
			) != 0 || prototype_term_app(
				terms, witness, field_witness, &witness
			) != 0) {
			return -1;
		}
		argument_claims[argument_count++] = field_left_claim;
		argument_claims[argument_count++] = field_right_claim;
		argument_claims[argument_count++] = field_witness_claim;
		(void)field_family_claim;
	}
	uint32_t witness_claim;
	if (prototype_judgement_add_constructor_spine_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			environment->target_context_id,
			witness,
			identity_family,
			argument_claims,
			argument_count,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_left_claim_id = left_claim_id;
	*p_right_claim_id = right_claim_id;
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)source_head;
	(void)source_owner;
	return 0;
}

static int hott_construct_thunk_return_object_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	const struct prototype_type_declaration* generated = identity && types &&
		identity->generated_type_declaration_id < types->type_count ?
		&types->type_declarations[identity->generated_type_declaration_id] : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!identity || !source || !generated || !environment ||
		!p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id || remaining_depth == 0 ||
		(identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) ||
		identity->generated_type_declaration_id == PROTOTYPE_INVALID_ID ||
		generated->origin_kind !=
			PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY ||
		generated->origin_source_carrier_term_id != source->classifier ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source->subject >=
			terms->term_count || source->classifier >= terms->term_count ||
		terms->terms[source->subject].tag != PROTOTYPE_TERM_THUNK ||
		terms->terms[source->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t returned_id = terms->terms[source->subject].as.thunk.computation;
	if (returned_id >= terms->term_count || terms->terms[returned_id].tag !=
			PROTOTYPE_TERM_RETURN) {
		return 1;
	}
	const struct prototype_judgement_derivation* thunk_derivation =
		hott_derivation_for_claim_and_kind(
			judgement, source_claim_id, PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO
		);
	uint32_t returned_claim_id = thunk_derivation &&
		thunk_derivation->premise_count == 1 ?
		thunk_derivation->premises[0].claim_id : PROTOTYPE_INVALID_ID;
	const struct prototype_judgement_proposition* returned =
		prototype_judgement_claim_proposition(judgement, returned_claim_id);
	const struct prototype_judgement_derivation* return_derivation =
		hott_derivation_for_claim_and_kind(
			judgement, returned_claim_id, PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO
		);
	uint32_t value_claim_id = return_derivation &&
		return_derivation->premise_count == 1 ?
		return_derivation->premises[0].claim_id : PROTOTYPE_INVALID_ID;
	const struct prototype_judgement_proposition* value =
		prototype_judgement_claim_proposition(judgement, value_claim_id);
	if (!thunk_derivation || !returned || !return_derivation || !value ||
		returned->subject != returned_id || value->subject !=
			terms->terms[returned_id].as.return_term.value) {
		return 1;
	}

	uint32_t left_value_claim;
	uint32_t right_value_claim;
	uint32_t value_family_claim;
	uint32_t value_witness;
	uint32_t value_witness_claim;
	int value_status = hott_construct_object_value_action_depth(
		kernel,
		NULL,
		value_claim_id,
		environment,
		universe,
		remaining_depth - 1,
		&left_value_claim,
		&right_value_claim,
		&value_family_claim,
		&value_witness,
		&value_witness_claim
	);
	if (value_status != 0) {
		return value_status;
	}
	const struct prototype_judgement_proposition* left_value =
		prototype_judgement_claim_proposition(judgement, left_value_claim);
	const struct prototype_judgement_proposition* right_value =
		prototype_judgement_claim_proposition(judgement, right_value_claim);
	uint32_t left_source_claim;
	uint32_t right_source_claim;
	uint32_t identity_family_claim;
	if (!left_value || !right_value || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_source_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_source_claim
		) != 0 || hott_instantiate_object_identity_family(
			kernel,
			identity,
			environment->target_context_id,
			left_source_claim,
			right_source_claim,
			&identity_family_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* identity_family =
		prototype_judgement_claim_proposition(judgement, identity_family_claim);
	uint32_t identity_former;
	uint32_t witness;
	if (!identity_family || prototype_term_type_instance_make(
			terms,
			types,
			identity->generated_type_declaration_id,
			NULL,
			0,
			&identity_former
		) != 0 || prototype_term_constructor(
			terms, identity_former, 0, &witness
		) != 0 || prototype_term_app(
			terms, witness, left_value->subject, &witness
		) != 0 || prototype_term_app(
			terms, witness, right_value->subject, &witness
		) != 0 || prototype_term_app(
			terms, witness, value_witness, &witness
		) != 0) {
		return -1;
	}
	uint32_t premises[3] = {
		left_value_claim, right_value_claim, value_witness_claim
	};
	uint32_t witness_claim;
	if (prototype_judgement_add_constructor_spine_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			environment->target_context_id,
			witness,
			identity_family->subject,
			premises,
			3,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)value_family_claim;
	return 0;
}

static int hott_apply_pointwise_identity_witness(
	struct prototype_kernel_builder* kernel,
	uint32_t function_identity_claim_id,
	const uint32_t* argument_claim_ids,
	uint32_t argument_count,
	uint32_t universe,
	uint32_t* p_computation_term_id,
	uint32_t* p_computation_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* function_identity = judgement ?
		prototype_judgement_claim_proposition(
			judgement, function_identity_claim_id
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!function_identity || !argument_claim_ids || argument_count == 0 ||
		argument_count > 3 || !p_computation_term_id || !p_computation_claim_id ||
		function_identity->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		function_identity->classifier >= terms->term_count ||
		terms->terms[function_identity->classifier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE || universe >= terms->term_count) {
		return -1;
	}
	uint32_t context_id = function_identity->context_id;
	const struct prototype_judgement_proposition* argument =
		prototype_judgement_claim_proposition(judgement, argument_claim_ids[0]);
	uint32_t pi = terms->terms[
		function_identity->classifier
	].as.thunk_type.computation;
	uint32_t forced;
	uint32_t forced_claim;
	uint32_t application;
	uint32_t application_classifier;
	uint32_t application_claim;
	uint32_t codomain_binding;
	uint32_t codomain;
	if (!argument || argument->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		argument->context_id != context_id || pi >= terms->term_count ||
		terms->terms[pi].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) != 0 || prototype_term_graph_substitute_bound_var(
			terms,
			types,
			codomain,
			codomain_binding,
			argument->subject,
			&application_classifier
		) != 0 || prototype_term_force(
			terms, function_identity->subject, &forced
		) != 0 || prototype_judgement_add_force_claim(
			judgement,
			terms,
			context_id,
			forced,
			pi,
			function_identity_claim_id,
			&forced_claim
		) != 0 || prototype_term_app(
			terms, forced, argument->subject, &application
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			context_id,
			application,
			application_classifier,
			forced_claim,
			argument_claim_ids[0],
			&application_claim
		) != 0) {
		return -1;
	}
	if (argument_count == 1) {
		*p_computation_term_id = application;
		*p_computation_claim_id = application_claim;
		return 0;
	}
	if (application_classifier >= terms->term_count ||
		terms->terms[application_classifier].tag !=
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	uint32_t next_value_type = terms->terms[
		application_classifier
	].as.computation_type.result;
	uint32_t next_value_type_claim;
	uint32_t continuation_binding = prototype_term_new_binding(terms);
	uint32_t continuation_context;
	uint32_t continuation_context_certificate;
	int next_type_status = hott_ensure_is_type_claim_in_context(
		judgement,
		terms,
		types,
		contexts,
		substitutions,
		context_id,
		next_value_type,
		&next_value_type_claim
	);
	if (next_type_status == 1) {
		uint32_t next_value_type_formation_claim;
		if (prototype_judgement_add_structural_type_formation_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				context_id,
				next_value_type,
				universe,
				&next_value_type_formation_claim
			) != 0 || prototype_judgement_add_is_type_claim(
				judgement,
				terms,
				context_id,
				next_value_type,
				universe,
				next_value_type_formation_claim,
				&next_value_type_claim
			) != 0) {
			return -1;
		}
		next_type_status = 0;
	}
	/* Generated pointwise identity codomains become ordinary object types
	 * before they are introduced into the continuation Context. */
	if (continuation_binding == PROTOTYPE_INVALID_ID || next_type_status != 0 ||
		prototype_context_extend(
			contexts,
			context_id,
			continuation_binding,
			next_value_type,
			PROTOTYPE_INVALID_ID,
			&continuation_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			continuation_context,
			next_value_type_claim,
			&continuation_context_certificate
		) != 0) {
		return -1;
	}
	uint32_t continuation_value;
	uint32_t continuation_value_claim;
	uint32_t continuation_projection;
	uint32_t mapped_arguments[3];
	if (prototype_term_var(
			terms, continuation_binding, &continuation_value
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			continuation_context,
			continuation_binding,
			next_value_type,
			&continuation_value_claim
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			continuation_context,
			context_id,
			&continuation_projection
		) != 0) {
		return -1;
	}
	for (uint32_t i = 1; i < argument_count; ++i) {
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				argument_claim_ids[i],
				continuation_projection,
				&mapped_arguments[i - 1]
			) != 0) {
			return -1;
		}
	}
	uint32_t continuation_body;
	uint32_t continuation_body_claim;
	if (hott_apply_pointwise_identity_witness(
			kernel,
			continuation_value_claim,
			mapped_arguments,
			argument_count - 1,
			universe,
			&continuation_body,
			&continuation_body_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* continuation_body_evidence =
		prototype_judgement_claim_proposition(
			judgement, continuation_body_claim
		);
	uint32_t continuation_family;
	uint32_t continuation_classifier;
	uint32_t continuation;
	uint32_t continuation_claim;
	uint32_t fold;
	uint32_t fold_classifier;
	uint32_t fold_claim;
	uint32_t fold_premises[2] = { application_claim, PROTOTYPE_INVALID_ID };
	if (!continuation_body_evidence ||
		continuation_body_evidence->context_id != continuation_context ||
		prototype_term_pure_family(
			terms,
			continuation_binding,
			continuation_body_evidence->classifier,
			&continuation_family
		) != 0 || prototype_term_pi_family(
			terms,
			next_value_type,
			continuation_family,
			&continuation_classifier
		) != 0 || prototype_term_lambda(
			terms, continuation_binding, continuation_body, &continuation
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			context_id,
			continuation,
			continuation_classifier,
			continuation_value_claim,
			continuation_body_claim,
			&continuation_claim
		) != 0 || prototype_term_computation_fold(
			terms, application, continuation, NULL, 0, &fold
		) != 0 || prototype_judgement_computation_fold_result_classifier(
			terms,
			types,
			application,
			application_classifier,
			continuation_classifier,
			&fold_classifier
		) != 0) {
		return -1;
	}
	fold_premises[1] = continuation_claim;
	if (prototype_judgement_add_computation_fold_claim(
			judgement,
			terms,
			types,
			context_id,
			fold,
			fold_classifier,
			fold_premises,
			2,
			&fold_claim
		) != 0) {
		return -1;
	}
	*p_computation_term_id = fold;
	*p_computation_claim_id = fold_claim;
	(void)continuation_context_certificate;
	return 0;
}

static int hott_apply_type_pointwise_identity_witness(
	struct prototype_kernel_builder* kernel,
	uint32_t function_identity_claim_id,
	const uint32_t* argument_claim_ids,
	uint32_t argument_count,
	uint32_t* p_term_id,
	uint32_t* p_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* current = judgement ?
		prototype_judgement_claim_proposition(
			judgement, function_identity_claim_id
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!current || !argument_claim_ids || argument_count == 0 ||
		!p_term_id || !p_claim_id || current->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	uint32_t current_claim_id = function_identity_claim_id;
	for (uint32_t i = 0; i < argument_count; ++i) {
		const struct prototype_judgement_proposition* argument =
			prototype_judgement_claim_proposition(
				judgement, argument_claim_ids[i]
			);
		uint32_t codomain_binding;
		uint32_t codomain;
		uint32_t application_classifier;
		uint32_t application;
		uint32_t application_claim;
		if (!argument || argument->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			argument->context_id != current->context_id ||
			current->classifier >= terms->term_count ||
			terms->terms[current->classifier].tag != PROTOTYPE_TERM_PI ||
			prototype_term_pure_family_parts(
				terms,
				terms->terms[current->classifier].as.pi.codomain_family,
				&codomain_binding,
				&codomain
			) != 0 || prototype_term_graph_substitute_bound_var(
				terms,
				types,
				codomain,
				codomain_binding,
				argument->subject,
				&application_classifier
			) != 0 || prototype_term_app(
				terms, current->subject, argument->subject, &application
			) != 0 || prototype_judgement_add_app_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				current->context_id,
				application,
				application_classifier,
				current_claim_id,
				argument_claim_ids[i],
				&application_claim
			) != 0) {
			return -1;
		}
		current_claim_id = application_claim;
		current = prototype_judgement_claim_proposition(
			judgement, current_claim_id
		);
		if (!current) {
			return -1;
		}
	}
	*p_term_id = current->subject;
	*p_claim_id = current_claim_id;
	return 0;
}

static int hott_construct_type_app_object_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	const struct prototype_judgement_derivation* app_derivation = judgement ?
		hott_derivation_for_claim_and_kind(
			judgement, source_claim_id, PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!identity || !source || !environment || !app_derivation ||
		remaining_depth == 0 || !p_left_claim_id || !p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_term_id ||
		!p_witness_claim_id || app_derivation->premise_count != 2 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->subject >= terms->term_count ||
		terms->terms[source->subject].tag != PROTOTYPE_TERM_APP ||
		source->classifier >= terms->term_count ||
		terms->terms[source->classifier].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		(identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT)) {
		return -1;
	}
	uint32_t function_left_claim;
	uint32_t function_right_claim;
	uint32_t function_family_claim;
	uint32_t function_witness;
	uint32_t function_witness_claim;
	uint32_t argument_left_claim;
	uint32_t argument_right_claim;
	uint32_t argument_family_claim;
	uint32_t argument_witness;
	uint32_t argument_witness_claim;
	int function_status = hott_construct_object_value_action_depth(
		kernel,
		NULL,
		app_derivation->premises[0].claim_id,
		environment,
		universe,
		remaining_depth - 1,
		&function_left_claim,
		&function_right_claim,
		&function_family_claim,
		&function_witness,
		&function_witness_claim
	);
	int argument_status = function_status == 0 ?
		hott_construct_object_value_action_depth(
			kernel,
			NULL,
			app_derivation->premises[1].claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&argument_left_claim,
			&argument_right_claim,
			&argument_family_claim,
			&argument_witness,
			&argument_witness_claim
		) : function_status;
	if (function_status != 0 || argument_status != 0) {
		return function_status != 0 ? function_status : argument_status;
	}
	uint32_t pointwise_arguments[3] = {
		argument_left_claim,
		argument_right_claim,
		argument_witness_claim
	};
	uint32_t witness;
	uint32_t witness_claim;
	if (hott_apply_type_pointwise_identity_witness(
			kernel,
			function_witness_claim,
			pointwise_arguments,
			3,
			&witness,
			&witness_claim
		) != 0) {
		return -1;
	}
	uint32_t left_claim;
	uint32_t right_claim;
	uint32_t identity_family_claim;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim
		) != 0 || hott_instantiate_object_identity_family(
			kernel,
			identity,
			environment->target_context_id,
			left_claim,
			right_claim,
			&identity_family_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* family =
		prototype_judgement_claim_proposition(
			judgement, identity_family_claim
		);
	const struct prototype_judgement_proposition* witness_evidence =
		prototype_judgement_claim_proposition(judgement, witness_claim);
	if (!family || !witness_evidence || witness_evidence->context_id !=
			environment->target_context_id ||
		prototype_judgement_classifier_conversion(
			terms, types, witness_evidence->classifier, family->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	if (witness_evidence->classifier != family->subject &&
		prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			witness,
			family->subject,
			witness_claim,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_left_claim_id = left_claim;
	*p_right_claim_id = right_claim;
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)function_left_claim;
	(void)function_right_claim;
	(void)function_family_claim;
	(void)function_witness;
	(void)argument_family_claim;
	(void)argument_witness;
	return 0;
}

static int hott_construct_object_app_action(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_computation_id,
	uint32_t* p_witness_computation_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	const struct prototype_judgement_derivation* app_derivation = judgement ?
		hott_derivation_for_claim_and_kind(
			judgement, source_claim_id, PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!source || !environment || !app_derivation || remaining_depth == 0 ||
		!p_left_claim_id || !p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_computation_id ||
		!p_witness_computation_claim_id || app_derivation->premise_count != 2 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->subject >= terms->term_count ||
		terms->terms[source->subject].tag != PROTOTYPE_TERM_APP ||
		source->classifier >= terms->term_count ||
		terms->terms[source->classifier].tag !=
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	uint32_t function_claim_id = app_derivation->premises[0].claim_id;
	const struct prototype_judgement_proposition* function =
		prototype_judgement_claim_proposition(judgement, function_claim_id);
	const struct prototype_judgement_derivation* force_derivation = function ?
		hott_derivation_for_claim_and_kind(
			judgement, function_claim_id, PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM
		) : NULL;
	if (!function || !force_derivation || force_derivation->premise_count != 1 ||
		function->subject != terms->terms[source->subject].as.app.function ||
		function->subject >= terms->term_count ||
		terms->terms[function->subject].tag != PROTOTYPE_TERM_FORCE) {
		return 1;
	}
	uint32_t function_value_claim_id = force_derivation->premises[0].claim_id;
	uint32_t argument_claim_id = app_derivation->premises[1].claim_id;
	uint32_t function_left_claim;
	uint32_t function_right_claim;
	uint32_t function_family_claim;
	uint32_t function_witness;
	uint32_t function_witness_claim;
	uint32_t argument_left_claim;
	uint32_t argument_right_claim;
	uint32_t argument_family_claim;
	uint32_t argument_witness;
	uint32_t argument_witness_claim;
	int function_status = hott_construct_object_value_action_depth(
		kernel,
		NULL,
		function_value_claim_id,
		environment,
		universe,
		remaining_depth - 1,
		&function_left_claim,
		&function_right_claim,
		&function_family_claim,
		&function_witness,
		&function_witness_claim
	);
	int argument_status = function_status == 0 ?
		hott_construct_object_value_action_depth(
			kernel,
			NULL,
			argument_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&argument_left_claim,
			&argument_right_claim,
			&argument_family_claim,
			&argument_witness,
			&argument_witness_claim
		) : function_status;
	if (function_status != 0 || argument_status != 0) {
		return function_status != 0 ? function_status : argument_status;
	}
	uint32_t application_arguments[3] = {
		argument_left_claim,
		argument_right_claim,
		argument_witness_claim
	};
	uint32_t witness_computation;
	uint32_t witness_computation_claim;
	if (hott_apply_pointwise_identity_witness(
			kernel,
			function_witness_claim,
			application_arguments,
			3,
			universe,
			&witness_computation,
			&witness_computation_claim
		) != 0) {
		return -1;
	}
	uint32_t left_claim;
	uint32_t right_claim;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left =
		prototype_judgement_claim_proposition(judgement, left_claim);
	const struct prototype_judgement_proposition* right =
		prototype_judgement_claim_proposition(judgement, right_claim);
	uint32_t computation_thunk_type;
	uint32_t identity_type_id;
	uint32_t left_thunk;
	uint32_t right_thunk;
	uint32_t identity_family;
	uint32_t identity_family_claim;
	uint32_t identity_arguments[2];
	if (!left || !right || prototype_term_thunk_type(
			terms, source->classifier, &computation_thunk_type
		) != 0 || prototype_type_declaration_find_generated_identity(
			types,
			computation_thunk_type,
			prototype_context_empty(contexts),
			&identity_type_id
		) != 0 || prototype_term_thunk(
			terms, left->subject, &left_thunk
		) != 0 || prototype_term_thunk(
			terms, right->subject, &right_thunk
		) != 0) {
		return 1;
	}
	identity_arguments[0] = left_thunk;
	identity_arguments[1] = right_thunk;
	if (prototype_term_type_instance_make(
			terms,
			types,
			identity_type_id,
			identity_arguments,
			2,
			&identity_family
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			identity_family,
			universe,
			&identity_family_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* witness_evidence =
		prototype_judgement_claim_proposition(
			judgement, witness_computation_claim
		);
	uint32_t empty_effects;
	uint32_t expected_classifier;
	if (!witness_evidence || prototype_term_effect_label(
			terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects
		) != 0 || prototype_term_computation_type(
			terms, empty_effects, identity_family, &expected_classifier
		) != 0 || prototype_judgement_classifier_conversion(
			terms, types, witness_evidence->classifier, expected_classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	if (witness_evidence->classifier != expected_classifier &&
		prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			witness_computation,
			expected_classifier,
			witness_computation_claim,
			&witness_computation_claim
		) != 0) {
		return -1;
	}
	*p_left_claim_id = left_claim;
	*p_right_claim_id = right_claim;
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_computation_id = witness_computation;
	*p_witness_computation_claim_id = witness_computation_claim;
	(void)function_left_claim;
	(void)function_right_claim;
	(void)function_family_claim;
	(void)function_witness;
	(void)argument_family_claim;
	(void)argument_witness;
	return 0;
}

static int hott_construct_object_computation_action_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_computation_id,
	uint32_t* p_witness_computation_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	if (!terms || !source || !environment || remaining_depth == 0 ||
		source->subject >= terms->term_count) {
		return -1;
	}
	if (terms->terms[source->subject].tag == PROTOTYPE_TERM_APP) {
		return hott_construct_object_app_action(
			kernel,
			source_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			p_left_claim_id,
			p_right_claim_id,
			p_identity_family_claim_id,
			p_witness_computation_id,
			p_witness_computation_claim_id
		);
	}
	return 1;
}

static int hott_construct_identity_lambda_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* outer_environment,
	uint32_t universe,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	int source_is_computation_function = terms && source &&
		source->subject < terms->term_count &&
		terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK;
	int source_is_type_function = terms && source &&
		source->subject < terms->term_count &&
		terms->terms[source->subject].tag == PROTOTYPE_TERM_LAMBDA;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!identity || !source || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id ||
		(identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE &&
		 identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->subject >= terms->term_count ||
		source->classifier >= terms->term_count || universe >= terms->term_count ||
		(!source_is_computation_function && !source_is_type_function) ||
		(source_is_computation_function && terms->terms[source->classifier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE) ||
		(source_is_type_function && terms->terms[source->classifier].tag !=
			PROTOTYPE_TERM_PI)) {
		return -1;
	}
	uint32_t target_context_id = outer_environment ?
		outer_environment->target_context_id : source->context_id;
	uint32_t left_source_claim_id = source_claim_id;
	uint32_t right_source_claim_id = source_claim_id;
	if (outer_environment && (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			outer_environment->left_substitution_id,
			&left_source_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			outer_environment->right_substitution_id,
			&right_source_claim_id
		) != 0)) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_source =
		prototype_judgement_claim_proposition(judgement, left_source_claim_id);
	const struct prototype_judgement_proposition* right_source =
		prototype_judgement_claim_proposition(judgement, right_source_claim_id);
	if (!left_source || !right_source || left_source->context_id !=
			target_context_id || right_source->context_id != target_context_id) {
		return -1;
	}

	/* The structural cases below are checked through the accepted Derivation
	 * DAG; the alpha-interned Core Lambda does not identify a typed occurrence. */
	uint32_t lambda_id = source_is_computation_function ?
		terms->terms[source->subject].as.thunk.computation : source->subject;
	if (lambda_id >= terms->term_count ||
		terms->terms[lambda_id].tag != PROTOTYPE_TERM_LAMBDA) {
		return 1;
	}
	const struct prototype_judgement_derivation* thunk_derivation =
		source_is_computation_function ? hott_derivation_for_claim_and_kind(
			judgement, source_claim_id, PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO
		) : NULL;
	uint32_t lambda_claim_id = source_is_computation_function ?
		(thunk_derivation && thunk_derivation->premise_count == 1 ?
			thunk_derivation->premises[0].claim_id : PROTOTYPE_INVALID_ID) :
		source_claim_id;
	const struct prototype_judgement_proposition* lambda_claim =
		prototype_judgement_claim_proposition(judgement, lambda_claim_id);
	const struct prototype_judgement_derivation* lambda_derivation =
		hott_derivation_for_claim_and_kind(
			judgement, lambda_claim_id, PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO
		);
	if (!lambda_claim || lambda_claim->subject != lambda_id ||
		!lambda_derivation || lambda_derivation->premise_count != 2) {
		return 1;
	}
	uint32_t body_claim_id = lambda_derivation->premises[1].claim_id;
	uint32_t binder_claim_id = lambda_derivation->premises[0].claim_id;
	const struct prototype_judgement_proposition* body_claim =
		prototype_judgement_claim_proposition(judgement, body_claim_id);
	const struct prototype_judgement_proposition* binder_claim =
		prototype_judgement_claim_proposition(judgement, binder_claim_id);
	uint32_t body_id = body_claim ? body_claim->subject : PROTOTYPE_INVALID_ID;
	int body_is_return = source_is_computation_function &&
		body_id < terms->term_count &&
		terms->terms[body_id].tag == PROTOTYPE_TERM_RETURN;
	int body_is_computation = source_is_computation_function &&
		body_id < terms->term_count &&
		terms->terms[body_id].tag == PROTOTYPE_TERM_APP;
	int body_is_value = source_is_type_function && body_id < terms->term_count;
	if (!body_claim || !binder_claim || body_id >= terms->term_count ||
		(!body_is_return && !body_is_computation && !body_is_value)) {
		return 1;
	}
	const struct prototype_judgement_derivation* return_derivation =
		body_is_return ? hott_derivation_for_claim_and_kind(
			judgement, body_claim_id, PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO
		) : NULL;
	uint32_t source_value = body_is_return ?
		terms->terms[body_id].as.return_term.value :
		(body_is_value ? body_id : PROTOTYPE_INVALID_ID);
	uint32_t source_value_claim_id = body_is_value ? body_claim_id :
		(return_derivation && return_derivation->premise_count == 1 ?
			return_derivation->premises[0].claim_id : PROTOTYPE_INVALID_ID);
	const struct prototype_judgement_proposition* source_value_claim =
		prototype_judgement_claim_proposition(judgement, source_value_claim_id);
	uint32_t source_binding = binder_claim->subject < terms->term_count &&
		terms->terms[binder_claim->subject].tag == PROTOTYPE_TERM_VAR ?
		terms->terms[binder_claim->subject].as.var.binding_id :
		PROTOTYPE_INVALID_ID;
	if (source_binding == PROTOTYPE_INVALID_ID || (body_is_return &&
		(!return_derivation || return_derivation->premise_count != 1 ||
		 !source_value_claim || source_value_claim->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		 source_value_claim->subject != source_value ||
		 source_value_claim->context_id != body_claim->context_id))) {
		return 1;
	}

	uint32_t concrete_identity;
	if (prototype_term_graph_substitute_bound_var(
			terms,
			types,
			identity->identity_type_term_id,
			identity->left_endpoint_binding_id,
			left_source->subject,
			&concrete_identity
		) != 0 || prototype_term_graph_substitute_bound_var(
			terms,
			types,
			concrete_identity,
			identity->right_endpoint_binding_id,
			right_source->subject,
			&concrete_identity
		) != 0 || concrete_identity >= terms->term_count ||
		(source_is_computation_function ?
			terms->terms[concrete_identity].tag != PROTOTYPE_TERM_THUNK_TYPE :
			terms->terms[concrete_identity].tag != PROTOTYPE_TERM_PI)) {
		return -1;
	}
	uint32_t pi[3];
	uint32_t codomain[3];
	uint32_t thunk_type[3];
	uint32_t binders[3];
	thunk_type[0] = concrete_identity;
	for (uint32_t i = 0; i < 3; ++i) {
		pi[i] = source_is_computation_function ?
			terms->terms[thunk_type[i]].as.thunk_type.computation : thunk_type[i];
		uint32_t family_binding;
		if (pi[i] >= terms->term_count ||
			terms->terms[pi[i]].tag != PROTOTYPE_TERM_PI ||
			prototype_term_pure_family_parts(
				terms,
				terms->terms[pi[i]].as.pi.codomain_family,
				&family_binding,
				&codomain[i]
			) != 0 ||
			codomain[i] >= terms->term_count ||
			(source_is_computation_function && terms->terms[codomain[i]].tag !=
				PROTOTYPE_TERM_COMPUTATION_TYPE)) {
			return -1;
		}
		binders[i] = family_binding;
		if (i < 2) {
			thunk_type[i + 1] = source_is_computation_function ?
				terms->terms[codomain[i]].as.computation_type.result : codomain[i];
			if (thunk_type[i + 1] >= terms->term_count ||
				(source_is_computation_function &&
				 terms->terms[thunk_type[i + 1]].tag !=
					PROTOTYPE_TERM_THUNK_TYPE)) {
				return -1;
			}
		}
	}
	uint32_t domain = terms->terms[pi[0]].as.pi.domain;
	if (terms->terms[pi[1]].as.pi.domain != domain) {
		return -1;
	}
	uint32_t input_identity = terms->terms[pi[2]].as.pi.domain;
	uint32_t result_identity = source_is_computation_function ?
		terms->terms[codomain[2]].as.computation_type.result : codomain[2];
	uint32_t x0;
	uint32_t x1;
	uint32_t xr;
	if (prototype_term_var(terms, binders[0], &x0) != 0 ||
		prototype_term_var(terms, binders[1], &x1) != 0 ||
		prototype_term_var(terms, binders[2], &xr) != 0) {
		return -1;
	}

	uint32_t domain_type_claim;
	if (hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			target_context_id,
			domain,
			&domain_type_claim
		) != 0) {
		return -1;
	}
	uint32_t x0_context;
	uint32_t x0_context_certificate;
	if (prototype_context_extend(
			contexts,
			target_context_id,
			binders[0],
			domain,
			PROTOTYPE_INVALID_ID,
			&x0_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			x0_context,
			domain_type_claim,
			&x0_context_certificate
		) != 0) {
		return -1;
	}
	uint32_t x0_to_source;
	uint32_t domain_type_in_x0;
	uint32_t x1_context;
	uint32_t x1_context_certificate;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			x0_context,
			target_context_id,
			&x0_to_source
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			domain_type_claim,
			x0_to_source,
			&domain_type_in_x0
		) != 0 || prototype_context_extend(
			contexts,
			x0_context,
			binders[1],
			domain,
			PROTOTYPE_INVALID_ID,
			&x1_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			x1_context,
			domain_type_in_x0,
			&x1_context_certificate
		) != 0) {
		return -1;
	}
	uint32_t input_identity_has_type_claim;
	uint32_t input_identity_is_type_claim;
	uint32_t xr_context;
	uint32_t xr_context_certificate;
	if (prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			x1_context,
			input_identity,
			universe,
			&input_identity_has_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			x1_context,
			input_identity,
			universe,
			input_identity_has_type_claim,
			&input_identity_is_type_claim
		) != 0 || prototype_context_extend(
			contexts,
			x1_context,
			binders[2],
			input_identity,
			PROTOTYPE_INVALID_ID,
			&xr_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			xr_context,
			input_identity_is_type_claim,
			&xr_context_certificate
		) != 0) {
		return -1;
	}

	uint32_t x0_claim;
	uint32_t x1_claim;
	uint32_t xr_claim;
	if (prototype_judgement_add_context_binding_assumption(
			judgement, terms, contexts, xr_context, binders[0], domain, &x0_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement, terms, contexts, xr_context, binders[1], domain, &x1_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			xr_context,
			binders[2],
			input_identity,
			&xr_claim
		) != 0) {
		return -1;
	}
	const struct prototype_context* source_body_context = prototype_context_get(
		contexts, body_claim->context_id
	);
	uint32_t body_target_projection;
	uint32_t left_body_base_substitution;
	uint32_t right_body_base_substitution;
	uint32_t left_body_substitution;
	uint32_t right_body_substitution;
	uint32_t left_body_substitution_certificate;
	uint32_t right_body_substitution_certificate;
	if (!source_body_context || source_body_context->parent != source->context_id ||
		source_body_context->binding_id != source_binding ||
		prototype_substitution_projection_path(
			substitutions,
			contexts,
			xr_context,
			target_context_id,
			&body_target_projection
		) != 0 || (outer_environment ? prototype_substitution_compose(
			substitutions,
			contexts,
			outer_environment->left_substitution_id,
			body_target_projection,
			&left_body_base_substitution
		) : prototype_substitution_projection_path(
			substitutions,
			contexts,
			xr_context,
			source->context_id,
			&left_body_base_substitution
		)) != 0 || (outer_environment ? prototype_substitution_compose(
			substitutions,
			contexts,
			outer_environment->right_substitution_id,
			body_target_projection,
			&right_body_base_substitution
		) : prototype_substitution_projection_path(
			substitutions,
			contexts,
			xr_context,
			source->context_id,
			&right_body_base_substitution
		)) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			left_body_base_substitution,
			body_claim->context_id,
			x0,
			domain,
			&left_body_substitution
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			left_body_substitution,
			x0_claim,
			&left_body_substitution_certificate
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			right_body_base_substitution,
			body_claim->context_id,
			x1,
			domain,
			&right_body_substitution
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			right_body_substitution,
			x1_claim,
			&right_body_substitution_certificate
		) != 0) {
		return -1;
	}

	uint32_t body_binding_count = outer_environment ?
		outer_environment->binding_count + 1 : 1;
	uint32_t* mapped_bindings = malloc(
		body_binding_count * sizeof(*mapped_bindings)
	);
	uint32_t* mapped_identity_claims = malloc(
		body_binding_count * sizeof(*mapped_identity_claims)
	);
	if (!mapped_bindings || !mapped_identity_claims) {
		free(mapped_bindings);
		free(mapped_identity_claims);
		return -1;
	}
	uint32_t outer_binding_count = outer_environment ?
		outer_environment->binding_count : 0;
	for (uint32_t i = 0; i < outer_binding_count; ++i) {
		mapped_bindings[i] = outer_environment->source_binding_ids[i];
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				outer_environment->identity_claim_ids[i],
				body_target_projection,
				&mapped_identity_claims[i]
			) != 0) {
			free(mapped_bindings);
			free(mapped_identity_claims);
			return -1;
		}
	}
	mapped_bindings[outer_binding_count] = source_binding;
	mapped_identity_claims[outer_binding_count] = xr_claim;
	struct hott_object_action_environment body_environment = {
		.left_substitution_id = left_body_substitution,
		.right_substitution_id = right_body_substitution,
		.target_context_id = xr_context,
		.source_binding_ids = mapped_bindings,
		.identity_claim_ids = mapped_identity_claims,
		.binding_count = body_binding_count
	};
	uint32_t current_term = PROTOTYPE_INVALID_ID;
	uint32_t current_claim = PROTOTYPE_INVALID_ID;
	int current_is_computation = 0;
	if (body_is_return || body_is_value) {
		uint32_t left_value_claim;
		uint32_t right_value_claim;
		uint32_t value_identity_family_claim;
		uint32_t value_witness;
		uint32_t value_witness_claim;
		int value_action_status = hott_construct_object_value_action_depth(
			kernel,
			NULL,
			source_value_claim_id,
			&body_environment,
			universe,
			(uint32_t)terms->term_count + 1,
			&left_value_claim,
			&right_value_claim,
			&value_identity_family_claim,
			&value_witness,
			&value_witness_claim
		);
		free(mapped_bindings);
		free(mapped_identity_claims);
		if (value_action_status != 0) {
			return value_action_status;
		}
		const struct prototype_judgement_proposition* value_identity_family =
			prototype_judgement_claim_proposition(
				judgement, value_identity_family_claim
			);
		const struct prototype_judgement_proposition* value_witness_proposition =
			prototype_judgement_claim_proposition(
				judgement, value_witness_claim
			);
		if (source_is_type_function) {
			if (!value_identity_family || !value_witness_proposition ||
				prototype_judgement_classifier_conversion(
					terms, types, value_identity_family->subject, result_identity
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
				prototype_judgement_classifier_conversion(
					terms,
					types,
					value_witness_proposition->classifier,
					result_identity
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return -1;
			}
			current_term = value_witness;
			current_claim = value_witness_claim;
			if (value_witness_proposition->classifier != result_identity &&
				prototype_judgement_add_conversion_claim(
					judgement,
					terms,
					types,
					xr_context,
					current_term,
					result_identity,
					current_claim,
					&current_claim
				) != 0) {
				return -1;
			}
		} else {
			const struct prototype_judgement_proposition* left_value =
				prototype_judgement_claim_proposition(
					judgement, left_value_claim
				);
			const struct prototype_judgement_proposition* right_value =
				prototype_judgement_claim_proposition(
					judgement, right_value_claim
				);
			uint32_t result_carrier = body_claim->classifier < terms->term_count &&
				terms->terms[body_claim->classifier].tag ==
					PROTOTYPE_TERM_COMPUTATION_TYPE ?
				terms->terms[body_claim->classifier].as.computation_type.result :
				PROTOTYPE_INVALID_ID;
			uint32_t result_thunk_type;
			uint32_t result_identity_type_id;
			if (!left_value || !right_value ||
				result_carrier == PROTOTYPE_INVALID_ID || prototype_term_thunk_type(
					terms, body_claim->classifier, &result_thunk_type
				) != 0 || prototype_type_declaration_find_generated_identity(
					types,
					result_thunk_type,
					prototype_context_empty(contexts),
					&result_identity_type_id
				) != 0) {
				return 1;
			}
			uint32_t result_identity_former;
			uint32_t pointwise_witness;
			if (prototype_term_type_instance_make(
					terms,
					types,
					result_identity_type_id,
					NULL,
					0,
					&result_identity_former
				) != 0 || prototype_term_constructor(
					terms, result_identity_former, 0, &pointwise_witness
				) != 0 || prototype_term_app(
					terms,
					pointwise_witness,
					left_value->subject,
					&pointwise_witness
				) != 0 || prototype_term_app(
					terms,
					pointwise_witness,
					right_value->subject,
					&pointwise_witness
				) != 0 || prototype_term_app(
					terms, pointwise_witness, value_witness, &pointwise_witness
				) != 0) {
				return -1;
			}
			uint32_t pointwise_premises[3] = {
				left_value_claim,
				right_value_claim,
				value_witness_claim
			};
			uint32_t pointwise_witness_claim;
			if (prototype_judgement_add_constructor_spine_claim(
					judgement,
					terms,
					types,
					contexts,
					substitutions,
					xr_context,
					pointwise_witness,
					result_identity,
					pointwise_premises,
					3,
					&pointwise_witness_claim
				) != 0) {
				return -1;
			}
			current_term = pointwise_witness;
			current_claim = pointwise_witness_claim;
		}
	} else {
		uint32_t computation_identity_family_claim;
		uint32_t left_computation_claim;
		uint32_t right_computation_claim;
		int computation_action_status =
			hott_construct_object_computation_action_depth(
				kernel,
				body_claim_id,
				&body_environment,
				universe,
				(uint32_t)terms->term_count + 1,
				&left_computation_claim,
				&right_computation_claim,
				&computation_identity_family_claim,
				&current_term,
				&current_claim
			);
		free(mapped_bindings);
		free(mapped_identity_claims);
		const struct prototype_judgement_proposition* computation_identity_family =
			prototype_judgement_claim_proposition(
				judgement, computation_identity_family_claim
			);
		const struct prototype_judgement_proposition* computation_witness =
			prototype_judgement_claim_proposition(judgement, current_claim);
		if (computation_action_status != 0) {
			return computation_action_status;
		}
		if (!computation_identity_family || !computation_witness ||
			prototype_judgement_classifier_conversion(
				terms,
				types,
				computation_identity_family->subject,
				result_identity
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
			prototype_judgement_classifier_conversion(
				terms, types, computation_witness->classifier, codomain[2]
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		if (computation_witness->classifier != codomain[2] &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				xr_context,
				current_term,
				codomain[2],
				current_claim,
				&current_claim
			) != 0) {
			return -1;
		}
		current_is_computation = 1;
		(void)left_computation_claim;
		(void)right_computation_claim;
	}
	uint32_t current_context = xr_context;
	for (uint32_t i = 3; i > 0; --i) {
		uint32_t lambda_body;
		uint32_t lambda_body_claim;
		uint32_t lambda;
		uint32_t lambda_claim;
		uint32_t binder_claim;
		uint32_t parent_context = i == 3 ? x1_context :
			(i == 2 ? x0_context : target_context_id);
		if (source_is_type_function) {
			lambda_body = current_term;
			lambda_body_claim = current_claim;
		} else if (current_is_computation) {
			lambda_body = current_term;
			lambda_body_claim = current_claim;
			current_is_computation = 0;
		} else if (prototype_term_return(
			terms, current_term, &lambda_body
		) != 0 || prototype_judgement_add_return_claim(
			judgement,
			terms,
			types,
			current_context,
			lambda_body,
			codomain[i - 1],
			current_claim,
			&lambda_body_claim
		) != 0) {
			return -1;
		}
		if (prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				current_context,
				binders[i - 1],
				terms->terms[pi[i - 1]].as.pi.domain,
				&binder_claim
			) != 0 || prototype_term_lambda(
				terms, binders[i - 1], lambda_body, &lambda
			) != 0 || prototype_judgement_add_lambda_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				parent_context,
				lambda,
				pi[i - 1],
				binder_claim,
				lambda_body_claim,
				&lambda_claim
			) != 0) {
			return -1;
		}
		if (source_is_computation_function) {
			uint32_t thunked;
			uint32_t thunked_claim;
			if (prototype_term_thunk(
					terms, lambda, &thunked
				) != 0 || prototype_judgement_add_thunk_claim(
					judgement,
					terms,
					types,
					parent_context,
					thunked,
					thunk_type[i - 1],
					lambda_claim,
					&thunked_claim
				) != 0) {
				return -1;
			}
			current_term = thunked;
			current_claim = thunked_claim;
		} else {
			current_term = lambda;
			current_claim = lambda_claim;
		}
		current_context = parent_context;
	}
	uint32_t identity_family_claim;
	if (hott_instantiate_object_identity_family(
			kernel,
			identity,
			target_context_id,
			left_source_claim_id,
			right_source_claim_id,
			&identity_family_claim
		) != 0 || prototype_judgement_claim_proposition(
			judgement, identity_family_claim
		)->subject != concrete_identity || prototype_judgement_claim_proposition(
			judgement, current_claim
		)->classifier != concrete_identity) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = current_term;
	*p_witness_claim_id = current_claim;
	(void)x0_context_certificate;
	(void)x1_context_certificate;
	(void)xr_context_certificate;
	(void)left_body_substitution_certificate;
	(void)right_body_substitution_certificate;
	return 0;
}

static int hott_construct_object_value_action_depth(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!source || !environment || !p_left_claim_id || !p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_term_id ||
		!p_witness_claim_id || remaining_depth == 0 || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source->subject >=
			terms->term_count) {
		return -1;
	}
	const struct prototype_judgement_derivation* reindex_derivation =
		hott_derivation_for_claim_and_kind(
			judgement,
			source_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
		);
	if (!identity && reindex_derivation) {
		if (reindex_derivation->premise_count != 1 ||
			reindex_derivation->premises[0].claim_id == PROTOTYPE_INVALID_ID ||
			reindex_derivation->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
			return -1;
		}
		uint32_t left_substitution;
		uint32_t right_substitution;
		if (prototype_substitution_compose(
				substitutions,
				contexts,
				reindex_derivation->semantic_action_id,
				environment->left_substitution_id,
				&left_substitution
			) != 0 || prototype_substitution_compose(
				substitutions,
				contexts,
				reindex_derivation->semantic_action_id,
				environment->right_substitution_id,
				&right_substitution
			) != 0) {
			return -1;
		}
		struct hott_object_action_environment source_environment = *environment;
		source_environment.left_substitution_id = left_substitution;
		source_environment.right_substitution_id = right_substitution;
		return hott_construct_object_value_action_depth(
			kernel,
			NULL,
			reindex_derivation->premises[0].claim_id,
			&source_environment,
			universe,
			remaining_depth - 1,
			p_left_claim_id,
			p_right_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim_id
		) != 0) {
		return -1;
	}
	struct prototype_hott_identity_type_computation_certificate local_identity;
	if (!identity) {
		int identity_status = hott_prepare_local_object_identity(
			kernel,
			source_claim_id,
			environment,
			universe,
			&local_identity
		);
		if (identity_status != 0) {
			return identity_status;
		}
		identity = &local_identity;
	}
	const struct prototype_judgement_derivation* conversion_derivation =
		hott_derivation_for_claim_and_kind(
			judgement,
			source_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
		);
	if (conversion_derivation) {
		if (conversion_derivation->premise_count != 1) {
			return -1;
		}
		uint32_t converted_left_claim;
		uint32_t converted_right_claim;
		uint32_t converted_family_claim;
		uint32_t converted_witness;
		uint32_t converted_witness_claim;
		int converted_status = hott_construct_object_value_action_depth(
			kernel,
			identity,
			conversion_derivation->premises[0].claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&converted_left_claim,
			&converted_right_claim,
			&converted_family_claim,
			&converted_witness,
			&converted_witness_claim
		);
		if (converted_status != 0) {
			return converted_status;
		}
		uint32_t identity_family_claim;
		if (hott_instantiate_object_identity_family(
				kernel,
				identity,
				environment->target_context_id,
				left_claim_id,
				right_claim_id,
				&identity_family_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* family =
			prototype_judgement_claim_proposition(
				judgement, identity_family_claim
			);
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, converted_witness_claim
			);
		if (!family || !witness || witness->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || witness->context_id !=
				environment->target_context_id ||
			prototype_judgement_classifier_conversion(
				terms, types, witness->classifier, family->subject
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		uint32_t witness_claim = converted_witness_claim;
		if (witness->classifier != family->subject &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				environment->target_context_id,
				converted_witness,
				family->subject,
				converted_witness_claim,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = converted_witness;
		*p_witness_claim_id = witness_claim;
		(void)converted_left_claim;
		(void)converted_right_claim;
		(void)converted_family_claim;
		return 0;
	}
	/* Type expression action is invariant under the kernel's pure conversion.
	 * Reduce a beta/iota redex before dispatching on its type former, then
	 * convert the resulting witness back to the original endpoint family. */
	uint32_t normalized_subject;
	if (source->classifier < terms->term_count &&
		terms->terms[source->classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
		prototype_term_normalize_complete_with_profile(
			terms,
			types,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			source->subject,
			&normalized_subject
		) == 0 && normalized_subject != source->subject) {
		uint32_t normalized_claim = PROTOTYPE_INVALID_ID;
		uint32_t ancestor_context = source->context_id;
		while (ancestor_context != PROTOTYPE_INVALID_ID) {
			uint32_t ancestor_claim = hott_has_type_claim_for_subject(
				judgement, ancestor_context, normalized_subject
			);
			const struct prototype_judgement_proposition* ancestor =
				prototype_judgement_claim_proposition(
					judgement, ancestor_claim
				);
			if (ancestor && prototype_judgement_classifier_conversion(
					terms, types, ancestor->classifier, source->classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				if (ancestor_context == source->context_id) {
					normalized_claim = ancestor_claim;
				} else {
					uint32_t projection;
					if (prototype_substitution_projection_path(
							substitutions,
							contexts,
							source->context_id,
							ancestor_context,
							&projection
						) != 0 || prototype_judgement_add_reindexed_claim(
							judgement,
							terms,
							types,
							contexts,
							substitutions,
							ancestor_claim,
							projection,
							&normalized_claim
						) != 0) {
						return -1;
					}
				}
				break;
			}
			const struct prototype_context* ancestor_record =
				prototype_context_get(contexts, ancestor_context);
			ancestor_context = ancestor_record ? ancestor_record->parent :
				PROTOTYPE_INVALID_ID;
		}
		if (normalized_claim == PROTOTYPE_INVALID_ID &&
			prototype_judgement_add_structural_type_formation_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source->context_id,
				normalized_subject,
				source->classifier,
				&normalized_claim
			) != 0) {
			return -1;
		}
		uint32_t normalized_left_claim;
		uint32_t normalized_right_claim;
		uint32_t normalized_family_claim;
		uint32_t normalized_witness;
		uint32_t normalized_witness_claim;
		int normalized_status = hott_construct_object_value_action_depth(
			kernel,
			NULL,
			normalized_claim,
			environment,
			universe,
			remaining_depth - 1,
			&normalized_left_claim,
			&normalized_right_claim,
			&normalized_family_claim,
			&normalized_witness,
			&normalized_witness_claim
		);
		if (normalized_status != 0) {
			return normalized_status;
		}
		uint32_t identity_family_claim;
		if (hott_instantiate_object_identity_family(
				kernel,
				identity,
				environment->target_context_id,
				left_claim_id,
				right_claim_id,
				&identity_family_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* family =
			prototype_judgement_claim_proposition(
				judgement, identity_family_claim
			);
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, normalized_witness_claim
			);
		if (!family || !witness || witness->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || witness->context_id !=
				environment->target_context_id) {
			return -1;
		}
		uint32_t witness_claim = normalized_witness_claim;
		int witness_conversion = prototype_judgement_classifier_conversion(
			terms, types, witness->classifier, family->subject
		).status;
		if (witness_conversion != PROTOTYPE_TERM_CONVERSION_EQUAL &&
			identity->computation_rule ==
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) {
			uint32_t constructor_head;
			uint32_t constructor_owner;
			uint32_t constructor_ordinal;
			uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			uint32_t argument_count;
			uint32_t constructor_claim = normalized_witness_claim;
			uint32_t premise_reindex = PROTOTYPE_INVALID_ID;
			const struct prototype_judgement_derivation* witness_reindex =
				hott_derivation_for_claim_and_kind(
					judgement,
					normalized_witness_claim,
					PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
				);
			if (witness_reindex) {
				if (witness_reindex->premise_count != 1 ||
					witness_reindex->semantic_action_kind !=
						PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION) {
					return -1;
				}
				constructor_claim = witness_reindex->premises[0].claim_id;
				premise_reindex = witness_reindex->semantic_action_id;
			}
			const struct prototype_judgement_derivation* constructor_derivation =
				hott_constructor_derivation_for_claim(
					judgement, constructor_claim
				);
			if (!constructor_derivation ||
				prototype_term_constructor_spine_info(
					terms,
					normalized_witness,
					&constructor_head,
					&constructor_owner,
					&constructor_ordinal,
					arguments,
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
					&argument_count
				) != 0 || argument_count < 2 ||
				constructor_derivation->premise_count != argument_count) {
				return -1;
			}
			const struct prototype_judgement_proposition* left =
				prototype_judgement_claim_proposition(
					judgement, left_claim_id
				);
			const struct prototype_judgement_proposition* right =
				prototype_judgement_claim_proposition(
					judgement, right_claim_id
				);
			uint32_t argument_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			if (!left || !right) {
				return -1;
			}
			arguments[0] = left->subject;
			arguments[1] = right->subject;
			argument_claims[0] = left_claim_id;
			argument_claims[1] = right_claim_id;
			for (uint32_t i = 2; i < argument_count; ++i) {
				argument_claims[i] =
					constructor_derivation->premises[i].claim_id;
				if (premise_reindex != PROTOTYPE_INVALID_ID &&
					prototype_judgement_add_reindexed_claim(
						judgement,
						terms,
						types,
						contexts,
						substitutions,
						argument_claims[i],
						premise_reindex,
						&argument_claims[i]
					) != 0) {
					return -1;
				}
			}
			uint32_t retargeted_witness = constructor_head;
			for (uint32_t i = 0; i < argument_count; ++i) {
				if (prototype_term_app(
						terms,
						retargeted_witness,
						arguments[i],
						&retargeted_witness
					) != 0) {
					return -1;
				}
			}
			if (prototype_judgement_add_constructor_spine_claim(
					judgement,
					terms,
					types,
					contexts,
					substitutions,
					environment->target_context_id,
					retargeted_witness,
					family->subject,
					argument_claims,
					argument_count,
					&witness_claim
				) != 0) {
				return -1;
			}
			normalized_witness = retargeted_witness;
			(void)constructor_owner;
			(void)constructor_ordinal;
		} else if (witness_conversion != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		} else if (witness->classifier != family->subject &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				environment->target_context_id,
				normalized_witness,
				family->subject,
				normalized_witness_claim,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = normalized_witness;
		*p_witness_claim_id = witness_claim;
		(void)normalized_left_claim;
		(void)normalized_right_claim;
		(void)normalized_family_claim;
		return 0;
	}
	if (source->context_id == prototype_context_empty(contexts) &&
		source->classifier < terms->term_count &&
		terms->terms[source->classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
		identity->computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) {
		uint32_t closed_family_claim;
		uint32_t closed_witness;
		uint32_t closed_witness_claim;
		int closed_status = hott_construct_universe_degeneracy(
			kernel,
			identity,
			source_claim_id,
			&closed_family_claim,
			&closed_witness,
			&closed_witness_claim
		);
		if (closed_status != 0) {
			return closed_status;
		}
		uint32_t target_projection;
		uint32_t family_claim;
		uint32_t witness_claim;
		if (prototype_substitution_projection_path(
				substitutions,
				contexts,
				environment->target_context_id,
				source->context_id,
				&target_projection
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				closed_family_claim,
				target_projection,
				&family_claim
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				closed_witness_claim,
				target_projection,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = family_claim;
		*p_witness_term_id = closed_witness;
		*p_witness_claim_id = witness_claim;
		return 0;
	}
	int status;
	if (terms->terms[source->subject].tag == PROTOTYPE_TERM_VAR) {
		status = hott_construct_object_variable_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	} else if (terms->terms[source->subject].tag == PROTOTYPE_TERM_APP &&
		source->classifier < terms->term_count &&
		terms->terms[source->classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
		identity->generated_type_declaration_id != PROTOTYPE_INVALID_ID) {
		status = hott_construct_type_app_object_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			p_left_claim_id,
			p_right_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
		return status;
	} else if (terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK &&
		identity->generated_type_declaration_id != PROTOTYPE_INVALID_ID) {
		status = hott_construct_thunk_return_object_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	} else if (terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK &&
		identity->generated_type_declaration_id == PROTOTYPE_INVALID_ID) {
		status = hott_construct_identity_lambda_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	} else if (terms->terms[source->subject].tag == PROTOTYPE_TERM_LAMBDA &&
		identity->generated_type_declaration_id == PROTOTYPE_INVALID_ID) {
		status = hott_construct_identity_lambda_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	} else if (terms->terms[source->subject].tag == PROTOTYPE_TERM_MATCH &&
		identity->generated_type_declaration_id != PROTOTYPE_INVALID_ID) {
		status = hott_construct_nullary_match_object_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			p_left_claim_id,
			p_right_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
		return status;
	} else if (identity->generated_type_declaration_id != PROTOTYPE_INVALID_ID) {
		status = hott_construct_adt_value_action(
			kernel,
			source_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			p_left_claim_id,
			p_right_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
		return status;
	} else {
		return 1;
	}
	if (status == 0) {
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
	}
	return status;
}

int prototype_hott_construct_degeneracy(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t source_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_proposition* source_type =
		request && kernel && request->kind ==
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_judgement_claim_proposition(
			kernel->judgement, request->key.identity_type.source_claim_id
		) : NULL;
	const struct prototype_judgement_proposition* source_term = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, source_claim_id
		) : NULL;
	struct prototype_kernel_view view;
	if (!actions || !kernel || !bridges || !result || !request || !certificate ||
		!source_type || !source_term || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id ||
		prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_db_validate(actions, &view, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		source_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source_term->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_term->classifier != source_type->subject) {
		return -1;
	}
	if (certificate->data.identity_type.computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) {
		return hott_construct_universe_degeneracy(
			kernel,
			&certificate->data.identity_type,
			source_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	uint32_t constructor_head;
	uint32_t constructor_owner;
	uint32_t constructor_ordinal;
	uint32_t constructor_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t constructor_argument_count;
	if (source_term->context_id == source_type->context_id &&
		prototype_term_constructor_spine_info(
			kernel->terms,
			source_term->subject,
			&constructor_head,
			&constructor_owner,
			&constructor_ordinal,
			constructor_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&constructor_argument_count
		) == 0) {
		/* Constructor action follows its complete dependent field Context. */
		return prototype_hott_construct_object_term_action(
			actions,
			kernel,
			bridges,
			identity_result_id,
			source_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	if (certificate->data.identity_type.computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT) {
		return hott_construct_adt_degeneracy_depth(
			kernel,
			source_claim_id,
			certificate->data.identity_type.generated_type_declaration_id,
			source_type->classifier,
			(uint32_t)kernel->terms->term_count + 1,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	if (source_term->context_id == source_type->context_id) {
		return prototype_hott_construct_object_term_action(
			actions,
			kernel,
			bridges,
			identity_result_id,
			source_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	if (certificate->data.identity_type.computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
		return hott_construct_identity_lambda_action(
			kernel,
			&certificate->data.identity_type,
			source_claim_id,
			NULL,
			source_type->classifier,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	return 1;
}

int prototype_hott_construct_object_term_action(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t source_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_hott_bridge* bridge = request && request->kind ==
		PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_hott_bridge_db_get(
			bridges, request->key.identity_type.source_bridge_id
		) : NULL;
	const struct prototype_judgement_proposition* source_type = request &&
		kernel && request->kind ==
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_judgement_claim_proposition(
			kernel->judgement, request->key.identity_type.source_claim_id
		) : NULL;
	const struct prototype_judgement_proposition* source_term = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, source_claim_id
		) : NULL;
	struct prototype_kernel_view view;
	if (!actions || !kernel || !bridges || !result || !request || !certificate ||
		!bridge || !source_type || !source_term || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id ||
		prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_db_validate(actions, &view, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		source_type->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source_term->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source_term->context_id !=
			source_type->context_id || source_term->classifier != source_type->subject ||
		bridge->source_context_id != source_type->context_id) {
		return -1;
	}
	const struct prototype_context* source_context = prototype_context_get(
		kernel->contexts, source_type->context_id
	);
	if (!source_context) {
		return -1;
	}
	size_t environment_capacity = source_context->depth ?
		source_context->depth : 1;
	uint32_t* source_bindings = malloc(
		environment_capacity * sizeof(*source_bindings)
	);
	uint32_t* identity_claims = malloc(
		environment_capacity * sizeof(*identity_claims)
	);
	if (!source_bindings || !identity_claims) {
		free(source_bindings);
		free(identity_claims);
		return -1;
	}
	struct hott_object_action_environment environment;
	if (hott_build_object_action_environment(
			kernel,
			bridges,
			bridge->id,
			source_bindings,
			identity_claims,
			(uint32_t)environment_capacity,
			&environment
		) != 0) {
		free(source_bindings);
		free(identity_claims);
		return -1;
	}
	uint32_t left_claim;
	uint32_t right_claim;
	int status = hott_construct_object_value_action_depth(
		kernel,
		&certificate->data.identity_type,
		source_claim_id,
		&environment,
		source_type->classifier,
		(uint32_t)kernel->terms->term_count + 1,
		&left_claim,
		&right_claim,
		p_identity_family_claim_id,
		p_witness_term_id,
		p_witness_claim_id
	);
	free(source_bindings);
	free(identity_claims);
	return status;
}

int prototype_hott_instantiate_object_identity_family(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_identity_family_claim_id
) {
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_proposition* source_type = request &&
		kernel && request->kind ==
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_judgement_claim_proposition(
			kernel->judgement, request->key.identity_type.source_claim_id
		) : NULL;
	const struct prototype_judgement_proposition* left = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, left_endpoint_claim_id
		) : NULL;
	const struct prototype_judgement_proposition* right = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, right_endpoint_claim_id
		) : NULL;
	struct prototype_kernel_view view;
	if (!actions || !kernel || !bridges || !result || !request || !certificate ||
		!source_type || !left || !right || !p_identity_family_claim_id ||
		prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_db_validate(actions, &view, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		source_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		left->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		right->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		left->context_id != source_type->context_id ||
		right->context_id != source_type->context_id ||
		prototype_judgement_classifier_conversion(
			kernel->terms,
			kernel->type_declarations,
			left->classifier,
			source_type->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
		prototype_judgement_classifier_conversion(
			kernel->terms,
			kernel->type_declarations,
			right->classifier,
			source_type->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	return hott_instantiate_object_identity_family(
		kernel,
		&certificate->data.identity_type,
		source_type->context_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		p_identity_family_claim_id
	);
}

int prototype_hott_check_object_identity_witness(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t witness_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_checked_witness_claim_id
) {
	const struct prototype_judgement_proposition* witness = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, witness_claim_id
		) : NULL;
	uint32_t family_claim_id;
	if (!kernel || !witness || !p_identity_family_claim_id ||
		!p_checked_witness_claim_id || witness->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_hott_instantiate_object_identity_family(
			actions,
			kernel,
			bridges,
			identity_result_id,
			left_endpoint_claim_id,
			right_endpoint_claim_id,
			&family_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* family =
		prototype_judgement_claim_proposition(
			kernel->judgement, family_claim_id
		);
	if (!family || witness->context_id != family->context_id ||
		prototype_judgement_classifier_conversion(
			kernel->terms,
			kernel->type_declarations,
			witness->classifier,
			family->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	uint32_t checked_witness_claim_id = witness_claim_id;
	if (witness->classifier != family->subject &&
		prototype_judgement_add_conversion_claim(
			kernel->judgement,
			kernel->terms,
			kernel->type_declarations,
			family->context_id,
			witness->subject,
			family->subject,
			witness_claim_id,
			&checked_witness_claim_id
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = family_claim_id;
	*p_checked_witness_claim_id = checked_witness_claim_id;
	return 0;
}

int prototype_hott_execute_object_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !judgement || !p_result_id ||
		prototype_kernel_builder_validate(kernel) != 0) {
		return -1;
	}
	int existing = hott_action_result_for_request(
		actions, request_id, p_result_id
	);
	if (existing <= 0) {
		return existing;
	}
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	if (!request || request->kind != PROTOTYPE_HOTT_ACTION_OBJECT_TERM) {
		return -1;
	}
	uint32_t identity_result_id;
	if (prototype_hott_execute_identity_type_computation(
			actions,
			kernel,
			bridges,
			request->key.object_term.identity_type_action_request_id,
			&identity_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* identity_result =
		prototype_hott_action_result_get(actions, identity_result_id);
	if (!identity_result) {
		return -1;
	}
	if (identity_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			identity_result->outcome.residual_reason,
			p_result_id
		);
	}
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, request->key.object_term.source_bridge_id
	);
	if (!bridge) {
		return -1;
	}
	uint32_t family_claim_id;
	uint32_t witness_term_id;
	uint32_t witness_claim_id;
	int action_status = prototype_hott_construct_object_term_action(
		actions,
		kernel,
		bridges,
		identity_result_id,
		request->key.object_term.source_claim_id,
		&family_claim_id,
		&witness_term_id,
		&witness_claim_id
	);
	if (action_status < 0) {
		return -1;
	}
	if (action_status > 0) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
			p_result_id
		);
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			request->key.object_term.source_claim_id,
			bridge->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			request->key.object_term.source_claim_id,
			bridge->right_substitution_id,
			&right_claim_id
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM,
		.data.object_term = {
			.left_endpoint_claim_id = left_claim_id,
			.right_endpoint_claim_id = right_claim_id,
			.identity_family_has_type_claim_id = family_claim_id,
			.witness_term_id = witness_term_id,
			.witness_has_type_claim_id = witness_claim_id
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_ensure_is_type_claim_in_context(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t* p_claim_id
) {
	if (!judgement || !terms || !type_declarations || !contexts ||
		!substitutions || !p_claim_id) {
		return -1;
	}
	uint32_t existing = hott_is_type_claim_for_subject(
		judgement, context_id, subject
	);
	if (existing != PROTOTYPE_INVALID_ID) {
		*p_claim_id = existing;
		return 0;
	}
	uint32_t ancestor = context_id;
	while (ancestor != PROTOTYPE_INVALID_ID) {
		uint32_t source_claim_id = hott_is_type_claim_for_subject(
			judgement, ancestor, subject
		);
		if (source_claim_id != PROTOTYPE_INVALID_ID) {
			uint32_t projection;
			uint32_t reindexed_claim_id;
			if (prototype_substitution_projection_path(
					substitutions, contexts, context_id, ancestor, &projection
				) != 0 || prototype_judgement_add_reindexed_claim(
					judgement,
					terms,
					type_declarations,
					contexts,
					substitutions,
					source_claim_id,
					projection,
					&reindexed_claim_id
				) != 0) {
				return -1;
			}
			const struct prototype_judgement_claim* reindexed =
				prototype_judgement_claim_get(judgement, reindexed_claim_id);
			if (!reindexed || prototype_judgement_proposition_get(judgement, reindexed->proposition_id)->subject != subject) {
				return 1;
			}
			*p_claim_id = reindexed_claim_id;
			return 0;
		}
		const struct prototype_context* context =
			prototype_context_get(contexts, ancestor);
		ancestor = context ? context->parent : PROTOTYPE_INVALID_ID;
	}
	return 1;
}

static const struct prototype_cwf_certificate*
hott_context_formation_certificate_for_context(
	const struct prototype_cwf_certificate_db* certificates,
	uint32_t context_id
) {
	if (!certificates) {
		return NULL;
	}
	for (uint32_t i = 0; i < certificates->certificate_count; ++i) {
		if (certificates->certificates[i].kind ==
				PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION &&
			certificates->certificates[i].structural_id == context_id) {
			return &certificates->certificates[i];
		}
	}
	return NULL;
}

static int hott_ensure_bridge_for_context(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t source_context_id,
	uint32_t* p_bridge_id,
	int* p_residual_reason
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_bridge_id || !p_residual_reason ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	if (hott_bridge_for_source_context_and_semantics(
			bridges,
			source_context_id,
			source_context_id == prototype_context_empty(contexts) ?
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY :
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION,
			p_bridge_id
		)) {
		*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
		return 0;
	}
	const struct prototype_context* source_context =
		prototype_context_get(contexts, source_context_id);
	const struct prototype_cwf_certificate* source_certificate =
		hott_context_formation_certificate_for_context(
			cwf_certificates, source_context_id
		);
	if (!source_context || !source_certificate ||
		source_context->parent == PROTOTYPE_INVALID_ID ||
		source_certificate->claim_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t parent_bridge_id;
	int parent_status = hott_ensure_bridge_for_context(
		actions,
		kernel,
		bridges,
		source_context->parent,
		&parent_bridge_id,
		p_residual_reason
	);
	if (parent_status != 0) {
		return parent_status;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = source_certificate->claim_id,
			.source_bridge_id = parent_bridge_id
		}
	};
	uint32_t type_request_id;
	uint32_t type_result_id;
	if (prototype_hott_action_request_intern(
			actions, &view, bridges, type_request, &type_request_id
		) != 0 || prototype_hott_execute_relation_type_action(
			actions, kernel, bridges, type_request_id, &type_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = type_result->outcome.residual_reason;
		return 1;
	}
	if (prototype_hott_bridge_db_construct_extension(
			bridges,
			kernel,
			actions,
			source_context_id,
			type_request_id,
			p_bridge_id
		) != 0) {
		return -1;
	}
	*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	return 0;
}

static int hott_match_case_context_valid(
	const struct prototype_context_db* contexts,
	uint32_t outer_context_id,
	uint32_t case_context_id,
	const struct prototype_case_binder* binders,
	uint32_t binder_count
) {
	if (!contexts || (binder_count > 0 && !binders)) {
		return 0;
	}
	uint32_t current = case_context_id;
	for (uint32_t i = binder_count; i > 0; --i) {
		const struct prototype_context* context =
			prototype_context_get(contexts, current);
		if (!context || context->binding_id != binders[i - 1].binding_id) {
			return 0;
		}
		current = context->parent;
	}
	return current == outer_context_id;
}

static int hott_execute_child_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	const struct prototype_hott_bridge* bridge,
	uint32_t source_context_id,
	uint32_t child_claim_id,
	uint32_t* p_witness_claim_id,
	int* p_residual_reason
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	const struct prototype_judgement_claim* child_claim =
		prototype_judgement_claim_get(judgement, child_claim_id);
	uint32_t child_type_claim_id = PROTOTYPE_INVALID_ID;
	int child_type_status = child_claim ? hott_ensure_is_type_claim_in_context(
		judgement,
		terms,
		type_declarations,
		contexts,
		substitutions,
		source_context_id,
		prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->classifier,
		&child_type_claim_id
	) : -1;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !bridge || !p_witness_claim_id || !p_residual_reason ||
		!child_claim || prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->context_id != source_context_id || child_type_status < 0 ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	if (child_type_status > 0 || child_type_claim_id == PROTOTYPE_INVALID_ID) {
		*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
		return 1;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = child_type_claim_id,
			.source_bridge_id = bridge->id
		}
	};
	uint32_t type_request_id;
	uint32_t type_result_id;
	if (prototype_hott_action_request_intern(
			actions, &view, bridges, type_request, &type_request_id
		) != 0 || prototype_hott_execute_relation_type_action(
			actions, kernel, bridges, type_request_id, &type_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = type_result->outcome.residual_reason;
		return 1;
	}
	struct prototype_hott_action_request term_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = child_claim_id,
			.source_bridge_id = bridge->id,
			.relation_type_action_request_id = type_request_id
		}
	};
	uint32_t term_request_id;
	uint32_t term_result_id;
	if (prototype_hott_action_request_intern(
			actions, &view, bridges, term_request, &term_request_id
		) != 0 || prototype_hott_execute_term_action(
			actions, kernel, bridges, term_request_id, &term_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* term_result =
		prototype_hott_action_result_get(actions, term_result_id);
	if (!term_result) {
		return -1;
	}
	if (term_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = term_result->outcome.residual_reason;
		return 1;
	}
	if (term_result->certificate_id >= actions->certificate_count ||
		actions->certificates[term_result->certificate_id].kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM) {
		return -1;
	}
	*p_witness_claim_id = actions->certificates[
		term_result->certificate_id
	].data.term.witness_has_type_claim_id;
	*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	return 0;
}

struct hott_relation_instantiation {
	uint32_t left_endpoint_claim_id;
	uint32_t right_endpoint_claim_id;
	uint32_t left_type_claim_id;
	uint32_t right_type_claim_id;
	uint32_t endpoint_substitution_id;
	uint32_t left_endpoint_substitution_certificate_id;
	uint32_t right_endpoint_substitution_certificate_id;
	uint32_t relation_type_term_id;
	uint32_t relation_is_type_claim_id;
};

static int hott_instantiate_relation_for_claims(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge* bridge,
	const struct prototype_hott_relation_type_action_certificate* type,
	uint32_t left_source_claim_id,
	uint32_t right_source_claim_id,
	uint32_t left_type_source_claim_id,
	uint32_t right_type_source_claim_id,
	struct hott_relation_instantiation* p_instantiation
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ?
		kernel->judgement : NULL;
	const struct prototype_context* endpoint_context = contexts && type ?
		prototype_context_get(contexts, type->endpoint_context_id) : NULL;
	if (!contexts || !substitutions || !cwf_certificates || !terms ||
		!type_declarations || !judgement || !bridge || !type ||
		!endpoint_context || !p_instantiation ||
		type->relation_family_semantics !=
			PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION) {
		return -1;
	}
	struct hott_relation_instantiation result;
	memset(&result, 0, sizeof(result));
	if (prototype_judgement_add_reindexed_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			left_source_claim_id, bridge->left_substitution_id,
			&result.left_endpoint_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			right_source_claim_id, bridge->right_substitution_id,
			&result.right_endpoint_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			left_type_source_claim_id, bridge->left_substitution_id,
			&result.left_type_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			right_type_source_claim_id, bridge->right_substitution_id,
			&result.right_type_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* left_endpoint =
		prototype_judgement_claim_get(
			judgement, result.left_endpoint_claim_id
		);
	const struct prototype_judgement_claim* right_endpoint =
		prototype_judgement_claim_get(
			judgement, result.right_endpoint_claim_id
		);
	if (!left_endpoint || !right_endpoint) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_endpoint_proposition =
		prototype_judgement_proposition_get(
			judgement, left_endpoint->proposition_id
		);
	const struct prototype_judgement_proposition* right_endpoint_proposition =
		prototype_judgement_proposition_get(
			judgement, right_endpoint->proposition_id
		);
	uint32_t base_substitution;
	uint32_t left_extension;
	if (!left_endpoint_proposition || !right_endpoint_proposition) {
		return -1;
	}
	if (prototype_substitution_identity(
			substitutions, contexts, bridge->bridge_context_id,
			&base_substitution
		) != 0) {
		return -1;
	}
	if (prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			base_substitution,
			endpoint_context->parent,
			left_endpoint_proposition->subject,
			left_endpoint_proposition->classifier,
			&left_extension
		) != 0) {
		return -1;
	}
	if (prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			left_extension,
			type->endpoint_context_id,
			right_endpoint_proposition->subject,
			right_endpoint_proposition->classifier,
			&result.endpoint_substitution_id
		) != 0) {
		return -1;
	}
	if (prototype_cwf_certificate_db_add_substitution(
			cwf_certificates,
			substitutions,
			judgement,
			left_extension,
			result.left_endpoint_claim_id,
			&result.left_endpoint_substitution_certificate_id
		) != 0) {
		return -1;
	}
	if (prototype_cwf_certificate_db_add_substitution(
			cwf_certificates,
			substitutions,
			judgement,
			result.endpoint_substitution_id,
			result.right_endpoint_claim_id,
			&result.right_endpoint_substitution_certificate_id
		) != 0) {
		return -1;
	}
	uint32_t expected_relation;
	if (prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			type->relation_type_term_id,
			result.endpoint_substitution_id,
			&result.relation_type_term_id
		) != 0 || prototype_term_relation_type(
			terms,
			left_endpoint_proposition->classifier,
			right_endpoint_proposition->classifier,
			left_endpoint_proposition->subject,
			right_endpoint_proposition->subject,
			&expected_relation
		) != 0 || result.relation_type_term_id != expected_relation) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_type =
		prototype_judgement_claim_proposition(
			judgement, result.left_type_claim_id
		);
	if (!left_type || prototype_judgement_add_relation_type_formation(
			judgement,
			terms,
			bridge->bridge_context_id,
			result.relation_type_term_id,
			left_type->classifier,
			result.left_type_claim_id,
			result.right_type_claim_id,
			result.left_endpoint_claim_id,
			result.right_endpoint_claim_id,
			&result.relation_is_type_claim_id
		) != 0) {
		return -1;
	}
	*p_instantiation = result;
	return 0;
}

int prototype_hott_execute_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	int existing = hott_action_result_for_request(
		actions, request_id, p_result_id
	);
	if (existing <= 0) {
		return existing;
	}
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	const struct prototype_judgement_claim* source = request &&
		request->kind == PROTOTYPE_HOTT_ACTION_TERM ?
		prototype_judgement_claim_get(
			judgement, request->key.term.source_claim_id
		) : NULL;
	const struct prototype_hott_bridge* bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.term.source_bridge_id
		) : NULL;
	uint32_t type_result_id;
	if (!source || !bridge || prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id != bridge->source_context_id) {
		return -1;
	}
	if (hott_action_result_for_request(
			actions, request->key.term.relation_type_action_request_id, &type_result_id
		) != 0) {
		return 1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			type_result->outcome.residual_reason,
			p_result_id
		);
	}
	const struct prototype_hott_action_certificate* type_certificate = type_result &&
		type_result->certificate_id < actions->certificate_count ?
		&actions->certificates[type_result->certificate_id] : NULL;
	if (!type_certificate ||
		type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE) {
		return 1;
	}
	const struct prototype_hott_relation_type_action_certificate* type =
		&type_certificate->data.relation_type;
	const struct prototype_hott_action_request* type_request =
		prototype_hott_action_request_get(
			actions, request->key.term.relation_type_action_request_id
		);
	struct hott_relation_instantiation instantiation;
	int instantiation_status = type_request ? hott_instantiate_relation_for_claims(
			kernel,
			bridge,
			type,
			request->key.term.source_claim_id,
			request->key.term.source_claim_id,
			type_request->key.relation_type.source_claim_id,
			type_request->key.relation_type.source_claim_id,
			&instantiation
		) : -1;
	if (!type_request || instantiation_status != 0) {
		return -1;
	}
	uint32_t left_endpoint_claim = instantiation.left_endpoint_claim_id;
	uint32_t right_endpoint_claim = instantiation.right_endpoint_claim_id;
	const struct prototype_judgement_claim* left_endpoint_evidence =
		prototype_judgement_claim_get(judgement, left_endpoint_claim);
	const struct prototype_judgement_claim* right_endpoint_evidence =
		prototype_judgement_claim_get(judgement, right_endpoint_claim);
	if (!left_endpoint_evidence || !right_endpoint_evidence) {
		return -1;
	}
	uint32_t endpoint_instantiation =
		instantiation.endpoint_substitution_id;
	uint32_t left_extension_certificate =
		instantiation.left_endpoint_substitution_certificate_id;
	uint32_t right_extension_certificate =
		instantiation.right_endpoint_substitution_certificate_id;
	uint32_t relation_type = instantiation.relation_type_term_id;
	uint32_t relation_claim = instantiation.relation_is_type_claim_id;
	uint32_t witness;
	uint32_t witness_claim;
	int used_relation_binding = 0;
	if (prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
		terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_VAR) {
		uint32_t source_entry_context;
		uint32_t source_binding = terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.var.binding_id;
		if (prototype_context_find_binding(
				contexts,
				prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
				source_binding,
				&source_entry_context
			) == 0) {
			const struct prototype_hott_bridge* binding_bridge =
				hott_bridge_for_source_context_and_semantics(
					bridges,
					source_entry_context,
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION,
					NULL
				);
			const struct prototype_context* relation_entry = binding_bridge ?
				prototype_context_get(
					contexts, binding_bridge->bridge_context_id
				) : NULL;
			if (relation_entry &&
				prototype_context_classifier_term(relation_entry) == relation_type &&
				prototype_term_var(
					terms, relation_entry->binding_id, &witness
				) == 0 &&
				prototype_judgement_add_context_binding_assumption(
					judgement,
					terms,
					contexts,
					bridge->bridge_context_id,
					relation_entry->binding_id,
					relation_type,
					&witness_claim
				) == 0) {
				used_relation_binding = 1;
			}
		}
	}
	if (!used_relation_binding) {
		struct prototype_term_conversion_result endpoint_comparison;
		if (prototype_term_compare_for_conversion(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
				prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
				64,
				&endpoint_comparison
			) != 0) {
			return -1;
		}
		if (endpoint_comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			if (endpoint_comparison.status ==
				PROTOTYPE_TERM_CONVERSION_EXHAUSTED) {
				return hott_publish_action_residual(
					actions,
					terms,
					request_id,
					PROTOTYPE_HOTT_RESIDUAL_CONVERSION_EXHAUSTED,
					p_result_id
				);
			}
			const struct prototype_judgement_derivation* induction_derivation =
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM
				);
			if (induction_derivation && prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag ==
					PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
				uint32_t argument = terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].
					as.induction_hypothesis.argument;
				uint32_t argument_claim_id = hott_has_type_claim_for_subject(
					judgement, prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id, argument
				);
				if (argument_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				uint32_t argument_witness_claim;
				int residual_reason;
				int argument_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
					bridge,
					prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
					argument_claim_id,
					&argument_witness_claim,
					&residual_reason
				);
				if (argument_status < 0) {
					return -1;
				}
				if (argument_status > 0) {
					return hott_publish_action_residual(
						actions, terms, request_id, residual_reason, p_result_id
					);
				}
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 ||
					prototype_judgement_add_relation_induction_hypothesis_witness(
						judgement,
						terms,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						request->key.term.source_claim_id,
						argument_witness_claim,
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			const struct prototype_judgement_derivation* lambda_derivation =
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO
				);
			if (!used_relation_binding && lambda_derivation &&
				prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_LAMBDA) {
				if (lambda_derivation->premise_count != 2) {
					return -1;
				}
				const struct prototype_judgement_claim* binder_claim =
					prototype_judgement_claim_get(
						judgement, lambda_derivation->premises[0].claim_id
					);
				const struct prototype_judgement_claim* body_claim =
					prototype_judgement_claim_get(
						judgement, lambda_derivation->premises[1].claim_id
					);
				const struct prototype_context* body_context = body_claim ?
					prototype_context_get(contexts, prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id) : NULL;
				const struct prototype_cwf_certificate* body_context_proof =
					body_context ? hott_context_formation_certificate_for_context(
						cwf_certificates, prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id
					) : NULL;
				if (!binder_claim || !body_claim || !body_context ||
					!body_context_proof || prototype_judgement_proposition_get(judgement, binder_claim->proposition_id)->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					prototype_judgement_proposition_get(judgement, binder_claim->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id ||
					body_context->parent != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id ||
					body_context->binding_id !=
						terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.lambda.binding_id ||
					body_context_proof->claim_id == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				struct prototype_hott_action_request binder_type_request = {
					.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
					.key.relation_type = {
						.source_claim_id = body_context_proof->claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t binder_type_request_id;
				uint32_t binder_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						binder_type_request, &binder_type_request_id
					) != 0 || prototype_hott_execute_relation_type_action(
						actions, kernel, bridges,
						binder_type_request_id, &binder_type_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* binder_type_result =
					prototype_hott_action_result_get(actions, binder_type_result_id);
				if (!binder_type_result) {
					return -1;
				}
				if (binder_type_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						binder_type_result->outcome.residual_reason, p_result_id
					);
				}
				uint32_t body_bridge_id;
				if (prototype_hott_bridge_db_construct_extension(
						bridges,
						kernel,
						actions,
						prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id,
						binder_type_request_id,
						&body_bridge_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_bridge* body_bridge =
					prototype_hott_bridge_db_get(bridges, body_bridge_id);
				uint32_t body_witness_claim;
				int residual_reason;
				int body_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
					body_bridge,
					prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id,
					lambda_derivation->premises[1].claim_id,
					&body_witness_claim,
					&residual_reason
				);
				if (body_status < 0) {
					return -1;
				}
				if (body_status > 0) {
					return hott_publish_action_residual(
						actions, terms, request_id, residual_reason, p_result_id
					);
				}
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 || prototype_judgement_add_relation_lambda_witness(
						judgement,
						terms,
						contexts,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						body_witness_claim,
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			const struct prototype_judgement_derivation* match_derivation =
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM
				);
			if (!used_relation_binding && match_derivation &&
				prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_MATCH) {
				const struct prototype_term* source_match =
					&terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject];
				if (source_match->as.match.case_count + 1 !=
						match_derivation->premise_count ||
					source_match->as.match.case_count + 4 >
						PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
					return -1;
				}
				uint32_t scrutinee_claim_id = hott_has_type_claim_for_subject(
					judgement, prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id, source_match->as.match.scrutinee
				);
				if (scrutinee_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				uint32_t scrutinee_witness_claim;
				int residual_reason;
				int scrutinee_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
					bridge,
					prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
					scrutinee_claim_id,
					&scrutinee_witness_claim,
					&residual_reason
				);
				if (scrutinee_status < 0) {
					return -1;
				}
				if (scrutinee_status > 0) {
					return hott_publish_action_residual(
						actions, terms, request_id, residual_reason, p_result_id
					);
				}
				uint32_t case_witness_claims[
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
				];
				for (uint32_t i = 0; i < source_match->as.match.case_count; ++i) {
					uint32_t case_id = source_match->as.match.first_case + i;
					const struct prototype_judgement_claim* case_claim =
						prototype_judgement_claim_get(
							judgement, match_derivation->premises[i + 1].claim_id
						);
					if (case_id >= terms->case_count || !case_claim ||
						prototype_judgement_proposition_get(judgement, case_claim->proposition_id)->subject != terms->cases[case_id].body ||
						!hott_match_case_context_valid(
							contexts,
							prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
							prototype_judgement_proposition_get(judgement, case_claim->proposition_id)->context_id,
							&terms->case_binders[
								terms->cases[case_id].first_binder
							],
							terms->cases[case_id].binder_count
						)) {
						return hott_publish_action_residual(
							actions, terms, request_id,
							PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
							p_result_id
						);
					}
					uint32_t case_bridge_id;
					int case_bridge_status = hott_ensure_bridge_for_context(
						actions,
						kernel,
						bridges,
						prototype_judgement_proposition_get(judgement, case_claim->proposition_id)->context_id,
						&case_bridge_id,
						&residual_reason
					);
					if (case_bridge_status < 0) {
						return -1;
					}
					if (case_bridge_status > 0) {
						return hott_publish_action_residual(
							actions, terms, request_id, residual_reason, p_result_id
						);
					}
					const struct prototype_hott_bridge* case_bridge =
						prototype_hott_bridge_db_get(bridges, case_bridge_id);
					if (!case_bridge) {
						return -1;
					}
					int case_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
						case_bridge,
						prototype_judgement_proposition_get(judgement, case_claim->proposition_id)->context_id,
						match_derivation->premises[i + 1].claim_id,
						&case_witness_claims[i],
						&residual_reason
					);
					if (case_status < 0) {
						return -1;
					}
					if (case_status > 0) {
						return hott_publish_action_residual(
							actions, terms, request_id, residual_reason, p_result_id
						);
					}
				}
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 || prototype_judgement_add_relation_match_witness(
						judgement,
						terms,
						type_declarations,
						contexts,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						request->key.term.source_claim_id,
						scrutinee_witness_claim,
						case_witness_claims,
						source_match->as.match.case_count,
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			const struct prototype_judgement_derivation* app_derivation =
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
				);
			if (!used_relation_binding && app_derivation &&
				prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_APP) {
				if (app_derivation->premise_count != 2) {
					return -1;
				}
				uint32_t child_witness_claims[2];
				for (uint32_t i = 0; i < 2; ++i) {
					const struct prototype_judgement_claim* child_claim =
						prototype_judgement_claim_get(
							judgement, app_derivation->premises[i].claim_id
						);
					uint32_t expected_subject = i == 0 ?
						terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.app.function :
						terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.app.argument;
					int matches = 0;
					if (!child_claim || prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id ||
						prototype_term_core_shape_equal(
							terms, prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->subject, expected_subject, &matches
						) != 0 || !matches) {
						return -1;
					}
					int residual_reason;
					int child_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
						bridge,
						prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
						app_derivation->premises[i].claim_id,
						&child_witness_claims[i],
						&residual_reason
					);
					if (child_status < 0) {
						return -1;
					}
					if (child_status > 0) {
						return hott_publish_action_residual(
							actions,
							terms,
							request_id,
							residual_reason,
							p_result_id
						);
					}
				}
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 || prototype_judgement_add_relation_app_witness(
						judgement,
						terms,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						child_witness_claims[0],
						child_witness_claims[1],
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			int unary_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INVALID;
			int unary_witness_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INVALID;
			uint32_t unary_payload = PROTOTYPE_INVALID_ID;
			if (prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_RETURN) {
				unary_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO;
				unary_witness_proof_kind =
					PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS;
				unary_payload = terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.return_term.value;
			} else if (prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_THUNK) {
				unary_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO;
				unary_witness_proof_kind =
					PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS;
				unary_payload = terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.thunk.computation;
			}
			const struct prototype_judgement_derivation* unary_derivation =
				unary_proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID ?
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					unary_proof_kind
				) : NULL;
			if (!used_relation_binding && unary_derivation) {
				if (unary_derivation->premise_count != 1) {
					return -1;
				}
				uint32_t child_claim_id = unary_derivation->premises[0].claim_id;
				const struct prototype_judgement_claim* child_claim =
					prototype_judgement_claim_get(judgement, child_claim_id);
				uint32_t child_type_claim_id = child_claim ?
					hott_is_type_claim_for_subject(
						judgement, prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id, prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->classifier
					) : PROTOTYPE_INVALID_ID;
				if (!child_claim || prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id ||
					prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->subject != unary_payload ||
					child_type_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				struct prototype_hott_action_request child_type_request = {
					.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
					.key.relation_type = {
						.source_claim_id = child_type_claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t child_type_request_id;
				uint32_t child_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						child_type_request, &child_type_request_id
					) != 0 || prototype_hott_execute_relation_type_action(
						actions, kernel, bridges,
						child_type_request_id, &child_type_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* child_type_result =
					prototype_hott_action_result_get(actions, child_type_result_id);
				if (!child_type_result) {
					return -1;
				}
				if (child_type_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						child_type_result->outcome.residual_reason, p_result_id
					);
				}
				struct prototype_hott_action_request child_term_request = {
					.kind = PROTOTYPE_HOTT_ACTION_TERM,
					.key.term = {
						.source_claim_id = child_claim_id,
						.source_bridge_id = bridge->id,
						.relation_type_action_request_id = child_type_request_id
					}
				};
				uint32_t child_term_request_id;
				uint32_t child_term_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						child_term_request, &child_term_request_id
					) != 0 || prototype_hott_execute_term_action(
						actions, kernel, bridges,
						child_term_request_id, &child_term_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* child_term_result =
					prototype_hott_action_result_get(actions, child_term_result_id);
				if (!child_term_result) {
					return -1;
				}
				if (child_term_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						child_term_result->outcome.residual_reason, p_result_id
					);
				}
				const struct prototype_hott_action_certificate* child_certificate =
					&actions->certificates[child_term_result->certificate_id];
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 || prototype_judgement_add_relation_unary_witness(
						judgement,
						terms,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						child_certificate->data.term.witness_has_type_claim_id,
						unary_witness_proof_kind,
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			const struct prototype_judgement_derivation* constructor_derivation =
				hott_constructor_derivation_for_claim(
					judgement, request->key.term.source_claim_id
				);
			uint32_t source_head;
			uint32_t source_owner;
			uint32_t source_constructor;
			uint32_t source_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			uint32_t source_argument_count;
			if (!used_relation_binding && (!constructor_derivation ||
				prototype_term_constructor_spine_info(
					terms,
					prototype_judgement_proposition_get(judgement, source->proposition_id)->subject,
					&source_head,
					&source_owner,
					&source_constructor,
					source_arguments,
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
					&source_argument_count
				) != 0 || source_argument_count == 0 ||
				constructor_derivation->premise_count != source_argument_count)) {
				return hott_publish_action_residual(
					actions,
					terms,
					request_id,
					PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
					p_result_id
				);
			}
			uint32_t field_witness_claims[
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
			];
			for (uint32_t i = 0; !used_relation_binding &&
				i < source_argument_count; ++i) {
				uint32_t field_claim_id =
					constructor_derivation->premises[i].claim_id;
				const struct prototype_judgement_claim* field_claim =
					prototype_judgement_claim_get(judgement, field_claim_id);
				uint32_t field_type_claim_id = field_claim ?
					hott_is_type_claim_for_subject(
						judgement, prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id, prototype_judgement_proposition_get(judgement, field_claim->proposition_id)->classifier
					) : PROTOTYPE_INVALID_ID;
				if (!field_claim || prototype_judgement_proposition_get(judgement, field_claim->proposition_id)->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					prototype_judgement_proposition_get(judgement, field_claim->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id ||
					prototype_judgement_proposition_get(judgement, field_claim->proposition_id)->subject != source_arguments[i] ||
					field_type_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions,
						terms,
						request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				struct prototype_hott_action_request field_type_request = {
					.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
					.key.relation_type = {
						.source_claim_id = field_type_claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t field_type_request_id;
				uint32_t field_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						field_type_request, &field_type_request_id
					) != 0 || prototype_hott_execute_relation_type_action(
						actions, kernel, bridges,
						field_type_request_id, &field_type_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* field_type_result =
					prototype_hott_action_result_get(
						actions, field_type_result_id
					);
				if (!field_type_result) {
					return -1;
				}
				if (field_type_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						field_type_result->outcome.residual_reason, p_result_id
					);
				}
				struct prototype_hott_action_request field_term_request = {
					.kind = PROTOTYPE_HOTT_ACTION_TERM,
					.key.term = {
						.source_claim_id = field_claim_id,
						.source_bridge_id = bridge->id,
						.relation_type_action_request_id = field_type_request_id
					}
				};
				uint32_t field_term_request_id;
				uint32_t field_term_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						field_term_request, &field_term_request_id
					) != 0 || prototype_hott_execute_term_action(
						actions, kernel, bridges,
						field_term_request_id, &field_term_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* field_term_result =
					prototype_hott_action_result_get(
						actions, field_term_result_id
					);
				if (!field_term_result) {
					return -1;
				}
				if (field_term_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						field_term_result->outcome.residual_reason, p_result_id
					);
				}
				const struct prototype_hott_action_certificate* field_certificate =
					&actions->certificates[field_term_result->certificate_id];
				field_witness_claims[i] =
					field_certificate->data.term.witness_has_type_claim_id;
			}
			if (!used_relation_binding && (prototype_term_relation_witness(
					terms,
					prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
					prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
					&witness
				) != 0 || prototype_judgement_add_relation_constructor_witness(
					judgement,
					terms,
					bridge->bridge_context_id,
					witness,
					relation_type,
					relation_claim,
					left_endpoint_claim,
					right_endpoint_claim,
					field_witness_claims,
					source_argument_count,
					&witness_claim
				) != 0)) {
				return -1;
			}
			(void)source_head;
			(void)source_owner;
			(void)source_constructor;
			if (constructor_derivation) {
				used_relation_binding = 1;
			}
		}
		if (!used_relation_binding &&
			(prototype_term_relation_witness(
				terms,
				prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
				prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
				&witness
			) != 0 || prototype_judgement_add_relation_witness_intro(
				judgement,
				terms,
				type_declarations,
				bridge->bridge_context_id,
				witness,
				relation_type,
				relation_claim,
				left_endpoint_claim,
				right_endpoint_claim,
				&witness_claim
			) != 0)) {
			return -1;
		}
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM,
		.data.term = {
			.endpoint_instantiation_substitution_id = endpoint_instantiation,
			.left_endpoint_substitution_certificate_id =
				left_extension_certificate,
			.right_endpoint_substitution_certificate_id =
				right_extension_certificate,
			.witness_term_id = witness,
			.witness_has_type_claim_id = witness_claim
		}
	};
	uint32_t certificate_id;
	if (prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		64,
		p_result_id
	);
}

int prototype_hott_execute_substitution_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_result_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations = kernel ? kernel->operations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id || normalization_profile !=
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	int existing = hott_action_result_for_request(
		actions, request_id, p_result_id
	);
	if (existing <= 0) {
		return existing;
	}
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	const struct prototype_substitution* source = request &&
		request->kind == PROTOTYPE_HOTT_ACTION_SUBSTITUTION ?
		prototype_substitution_get(
			substitutions,
			request->key.substitution.source_substitution_id
		) : NULL;
	const struct prototype_hott_bridge* source_bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.substitution.source_bridge_id
		) : NULL;
	const struct prototype_hott_bridge* target_bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.substitution.target_bridge_id
		) : NULL;
	if (!source || !source_bridge || !target_bridge ||
		source->source_context != source_bridge->source_context_id ||
		source->target_context != target_bridge->source_context_id) {
		return -1;
	}
	uint32_t result_substitution;
	uint32_t result_substitution_certificate = PROTOTYPE_INVALID_ID;
	switch (source->kind) {
	case PROTOTYPE_SUBSTITUTION_IDENTITY:
		if (source_bridge->id != target_bridge->id ||
			prototype_substitution_identity(
				substitutions,
				contexts,
				source_bridge->bridge_context_id,
				&result_substitution
			) != 0) {
			return -1;
		}
		break;
	case PROTOTYPE_SUBSTITUTION_EMPTY:
		if (target_bridge->source_context_id != prototype_context_empty(contexts) ||
			prototype_substitution_empty(
				substitutions,
				contexts,
				source_bridge->bridge_context_id,
				&result_substitution
			) != 0) {
			return -1;
		}
		break;
	case PROTOTYPE_SUBSTITUTION_PROJECTION:
		if (prototype_substitution_projection_path(
				substitutions,
				contexts,
				source_bridge->bridge_context_id,
				target_bridge->bridge_context_id,
				&result_substitution
			) != 0) {
			return -1;
		}
		break;
	case PROTOTYPE_SUBSTITUTION_COMPOSE:
		{
			const struct prototype_substitution* outer =
				prototype_substitution_get(substitutions, source->first);
			const struct prototype_substitution* inner =
				prototype_substitution_get(substitutions, source->second);
			uint32_t middle_bridge_id;
			if (!outer || !inner || outer->source_context != inner->target_context ||
				!hott_bridge_for_source_context_and_semantics(
					bridges,
					outer->source_context,
					outer->source_context == prototype_context_empty(contexts) ?
						PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY :
						bridges->certificates[source_bridge->id].semantics,
					&middle_bridge_id
				)) {
				return -1;
			}
			struct prototype_hott_action_request inner_request = {
				.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
				.key.substitution = {
					.source_substitution_id = source->second,
					.source_bridge_id = source_bridge->id,
					.target_bridge_id = middle_bridge_id
				}
			};
			struct prototype_hott_action_request outer_request = {
				.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
				.key.substitution = {
					.source_substitution_id = source->first,
					.source_bridge_id = middle_bridge_id,
					.target_bridge_id = target_bridge->id
				}
			};
			uint32_t inner_request_id;
			uint32_t outer_request_id;
			uint32_t inner_result_id;
			uint32_t outer_result_id;
			if (prototype_hott_action_request_intern(
					actions, &view, bridges, inner_request, &inner_request_id
				) != 0 || prototype_hott_action_request_intern(
					actions, &view, bridges, outer_request, &outer_request_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, kernel, bridges, inner_request_id, normalization_profile,
					step_limit, &inner_result_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, kernel, bridges, outer_request_id, normalization_profile,
					step_limit, &outer_result_id
				) != 0) {
				return -1;
			}
			const struct prototype_hott_action_result* inner_result =
				prototype_hott_action_result_get(actions, inner_result_id);
			const struct prototype_hott_action_result* outer_result =
				prototype_hott_action_result_get(actions, outer_result_id);
			if (!inner_result || !outer_result ||
				inner_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
				outer_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
				return 1;
			}
			const struct prototype_hott_action_certificate* inner_certificate =
				&actions->certificates[inner_result->certificate_id];
			const struct prototype_hott_action_certificate* outer_certificate =
				&actions->certificates[outer_result->certificate_id];
			if (prototype_substitution_compose(
					substitutions,
					contexts,
					outer_certificate->data.substitution.result_substitution_id,
					inner_certificate->data.substitution.result_substitution_id,
					&result_substitution
				) != 0) {
				return -1;
			}
		}
		break;
	case PROTOTYPE_SUBSTITUTION_EXTEND:
		{
			const struct prototype_context* target = prototype_context_get(
				contexts, source->target_context
			);
			uint32_t parent_bridge_id;
			const struct prototype_hott_bridge* parent_bridge = target ?
				hott_bridge_for_source_context_and_semantics(
					bridges,
					target->parent,
					target->parent == prototype_context_empty(contexts) ?
						PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY :
						bridges->certificates[target_bridge->id].semantics,
					&parent_bridge_id
				) : NULL;
			uint32_t term_claim_id = PROTOTYPE_INVALID_ID;
			for (uint32_t i = 0; i < judgement->claim_count; ++i) {
				const struct prototype_judgement_claim* claim = &judgement->claims[i];
				if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == source->source_context &&
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == source->term &&
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier == source->term_classifier &&
					hott_operation_matches_claim(operations, judgement, claim)) {
					term_claim_id = i;
					break;
				}
			}
			uint32_t type_request_id = PROTOTYPE_INVALID_ID;
			for (uint32_t i = 0; i < actions->request_count; ++i) {
				const struct prototype_hott_action_request* candidate =
					&actions->requests[i];
				const struct prototype_judgement_claim* type_claim =
					candidate->kind == PROTOTYPE_HOTT_ACTION_RELATION_TYPE ?
					prototype_judgement_claim_get(
						judgement, candidate->key.relation_type.source_claim_id
					) : NULL;
				uint32_t candidate_result;
				if (type_claim && parent_bridge &&
					candidate->key.relation_type.source_bridge_id == parent_bridge_id &&
					prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->context_id == target->parent &&
					prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->subject ==
						prototype_context_classifier_term(target) &&
					hott_action_result_for_request(
						actions, i, &candidate_result
					) == 0 && actions->results[candidate_result].outcome.state ==
						PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					type_request_id = i;
					break;
				}
			}
			if (!target || !parent_bridge || term_claim_id == PROTOTYPE_INVALID_ID ||
				type_request_id == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			struct prototype_hott_action_request prefix_request = {
				.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
				.key.substitution = {
					.source_substitution_id = source->first,
					.source_bridge_id = source_bridge->id,
					.target_bridge_id = parent_bridge_id
				}
			};
			struct prototype_hott_action_request term_request = {
				.kind = PROTOTYPE_HOTT_ACTION_TERM,
				.key.term = {
					.source_claim_id = term_claim_id,
					.source_bridge_id = source_bridge->id,
					.relation_type_action_request_id = type_request_id
				}
			};
			uint32_t prefix_request_id;
			uint32_t term_request_id;
			uint32_t prefix_result_id;
			uint32_t term_result_id;
			if (prototype_hott_action_request_intern(
					actions, &view, bridges, prefix_request, &prefix_request_id
				) != 0 || prototype_hott_action_request_intern(
					actions, &view, bridges, term_request, &term_request_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, kernel, bridges, prefix_request_id, normalization_profile,
					step_limit, &prefix_result_id
				) != 0 || prototype_hott_execute_term_action(
					actions, kernel, bridges,
					term_request_id, &term_result_id
				) != 0) {
				return -1;
			}
			const struct prototype_hott_action_result* prefix_result =
				prototype_hott_action_result_get(actions, prefix_result_id);
			const struct prototype_hott_action_result* term_result =
				prototype_hott_action_result_get(actions, term_result_id);
			if (!prefix_result || !term_result ||
				prefix_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
				term_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
				return 1;
			}
			const struct prototype_hott_action_certificate* term_certificate =
				&actions->certificates[term_result->certificate_id];
			const struct prototype_judgement_claim* witness_claim =
				prototype_judgement_claim_get(
					judgement,
					term_certificate->data.term.witness_has_type_claim_id
				);
			if (!witness_claim ||
				prototype_substitution_extend(
					substitutions,
					contexts,
					terms,
					type_declarations,
					term_certificate->data.term.
						endpoint_instantiation_substitution_id,
					target_bridge->bridge_context_id,
					term_certificate->data.term.witness_term_id,
					prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->classifier,
					&result_substitution
				) != 0 || prototype_cwf_certificate_db_add_substitution(
					cwf_certificates,
					substitutions,
					judgement,
					result_substitution,
					term_certificate->data.term.witness_has_type_claim_id,
					&result_substitution_certificate
				) != 0) {
				return -1;
			}
		}
		break;
	default:
		return -1;
	}
	uint32_t left_lhs;
	uint32_t left_rhs;
	uint32_t right_lhs;
	uint32_t right_rhs;
	struct prototype_term_conversion_result left_comparison;
	struct prototype_term_conversion_result right_comparison;
	if (prototype_substitution_compose(
			substitutions,
			contexts,
			target_bridge->left_substitution_id,
			result_substitution,
			&left_lhs
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			request->key.substitution.source_substitution_id,
			source_bridge->left_substitution_id,
			&left_rhs
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			target_bridge->right_substitution_id,
			result_substitution,
			&right_lhs
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			request->key.substitution.source_substitution_id,
			source_bridge->right_substitution_id,
			&right_rhs
		) != 0 || prototype_substitution_compare_pointwise(
			substitutions,
			contexts,
			terms,
			type_declarations,
			left_lhs,
			left_rhs,
			normalization_profile,
			step_limit,
			&left_comparison
		) != 0 || prototype_substitution_compare_pointwise(
			substitutions,
			contexts,
			terms,
			type_declarations,
			right_lhs,
			right_rhs,
			normalization_profile,
			step_limit,
			&right_comparison
		) != 0) {
		return -1;
	}
	if (left_comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
		right_comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		if (left_comparison.status != PROTOTYPE_TERM_CONVERSION_EXHAUSTED &&
			right_comparison.status != PROTOTYPE_TERM_CONVERSION_EXHAUSTED) {
			return -1;
		}
		struct prototype_hott_action_result residual = {
			.request_id = request_id,
			.certificate_id = PROTOTYPE_INVALID_ID,
			.outcome = {
				.state = PROTOTYPE_HOTT_ACTION_RESULT_RESIDUAL,
				.residual_reason = PROTOTYPE_HOTT_RESIDUAL_CONVERSION_EXHAUSTED,
				.normalization_profile = normalization_profile,
				.step_limit = step_limit,
				.term_graph_revision = terms->term_count
			}
		};
		memcpy(
			residual.outcome.calculus_fingerprint,
			PROTOTYPE_HOTT_CALCULUS_FINGERPRINT,
			65
		);
		return prototype_hott_action_result_publish(
			actions, residual, p_result_id
		);
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_SUBSTITUTION_NATURALITY,
		.data.substitution = {
			.result_substitution_id = result_substitution,
			.result_substitution_certificate_id = result_substitution_certificate,
			.left_naturality_lhs_substitution_id = left_lhs,
			.left_naturality_rhs_substitution_id = left_rhs,
			.right_naturality_lhs_substitution_id = right_lhs,
			.right_naturality_rhs_substitution_id = right_rhs,
			.normalization_profile = normalization_profile,
			.step_limit = step_limit,
			.term_graph_revision = terms->term_count
		}
	};
	uint32_t certificate_id;
	if (prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		normalization_profile,
		step_limit,
		p_result_id
	);
}

static int hott_bridge_db_finish_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge* parent,
	uint32_t source_context_id,
	uint32_t endpoint_context_id,
	uint32_t left_endpoint_binding_id,
	uint32_t right_endpoint_binding_id,
	uint32_t fiber_type_term_id,
	uint32_t fiber_type_claim_id,
	uint32_t fiber_action_certificate_id,
	uint32_t left_endpoint_context_certificate_id,
	uint32_t right_endpoint_context_certificate_id,
	int semantics,
	uint32_t* p_bridge_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_context* right_endpoint_context =
		prototype_context_get(contexts, endpoint_context_id);
	const struct prototype_context* left_endpoint_context = right_endpoint_context ?
		prototype_context_get(contexts, right_endpoint_context->parent) : NULL;
	const struct prototype_judgement_proposition* fiber_type = judgement ?
		prototype_judgement_claim_proposition(judgement, fiber_type_claim_id) : NULL;
	if (!bridges || !contexts || !substitutions || !cwf || !terms || !types ||
		!judgement || !parent || !source || !left_endpoint_context ||
		!right_endpoint_context || !fiber_type || !p_bridge_id ||
		source->parent != parent->source_context_id ||
		left_endpoint_context->parent != parent->bridge_context_id ||
		left_endpoint_context->binding_id != left_endpoint_binding_id ||
		right_endpoint_context->binding_id != right_endpoint_binding_id ||
		fiber_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		fiber_type->context_id != endpoint_context_id ||
		fiber_type->subject != fiber_type_term_id ||
		(semantics != PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION &&
		 semantics != PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY)) {
		return -1;
	}
	uint32_t proof_binding = prototype_term_new_binding(terms);
	uint32_t bridge_context;
	uint32_t bridge_context_certificate;
	uint32_t proof_claim;
	if (proof_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			endpoint_context_id,
			proof_binding,
			fiber_type_term_id,
			PROTOTYPE_INVALID_ID,
			&bridge_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			bridge_context,
			fiber_type_claim_id,
			&bridge_context_certificate
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			bridge_context,
			proof_binding,
			fiber_type_term_id,
			&proof_claim
		) != 0) {
		return -1;
	}
	uint32_t bridge_to_parent;
	uint32_t left_prefix;
	uint32_t right_prefix;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t left_substitution;
	uint32_t right_substitution;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			bridge_context,
			parent->bridge_context_id,
			&bridge_to_parent
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			parent->left_substitution_id,
			bridge_to_parent,
			&left_prefix
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			parent->right_substitution_id,
			bridge_to_parent,
			&right_prefix
		) != 0 || prototype_term_var(
			terms, left_endpoint_binding_id, &left_endpoint
		) != 0 || prototype_term_var(
			terms, right_endpoint_binding_id, &right_endpoint
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			left_prefix,
			source_context_id,
			left_endpoint,
			prototype_context_classifier_term(left_endpoint_context),
			&left_substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			right_prefix,
			source_context_id,
			right_endpoint,
			prototype_context_classifier_term(right_endpoint_context),
			&right_substitution
		) != 0) {
		return -1;
	}
	uint32_t left_claim;
	uint32_t right_claim;
	uint32_t left_substitution_certificate;
	uint32_t right_substitution_certificate;
	if (prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			bridge_context,
			left_endpoint_binding_id,
			prototype_context_classifier_term(left_endpoint_context),
			&left_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			bridge_context,
			right_endpoint_binding_id,
			prototype_context_classifier_term(right_endpoint_context),
			&right_claim
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			left_substitution,
			left_claim,
			&left_substitution_certificate
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			right_substitution,
			right_claim,
			&right_substitution_certificate
		) != 0 || bridges->bridge_count >= bridges->bridge_capacity ||
		bridges->certificate_count >= bridges->certificate_capacity) {
		return -1;
	}
	struct prototype_hott_bridge bridge = {
		.id = (uint32_t)bridges->bridge_count,
		.source_context_id = source_context_id,
		.bridge_context_id = bridge_context,
		.left_substitution_id = left_substitution,
		.right_substitution_id = right_substitution
	};
	if (!hott_bridge_record_is_valid(
			&bridge,
			bridge.id,
			contexts,
			substitutions,
			cwf
		)) {
		return -1;
	}
	bridges->bridges[bridges->bridge_count++] = bridge;
	bridges->certificates[bridges->certificate_count++] =
		(struct prototype_hott_bridge_certificate) {
			.id = bridge.id,
			.bridge_id = bridge.id,
			.semantics = semantics,
			.parent_bridge_id = parent->id,
			.fiber_action_certificate_id = fiber_action_certificate_id,
			.fiber_witness_claim_id = proof_claim,
			.left_endpoint_context_certificate_id =
				left_endpoint_context_certificate_id,
			.right_endpoint_context_certificate_id =
				right_endpoint_context_certificate_id,
			.relation_context_certificate_id = bridge_context_certificate,
			.left_substitution_certificate_id = left_substitution_certificate,
			.right_substitution_certificate_id = right_substitution_certificate
		};
	*p_bridge_id = bridge.id;
	return 0;
}

int prototype_hott_bridge_db_construct_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t relation_type_action_request_id,
	uint32_t* p_bridge_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, relation_type_action_request_id);
	uint32_t result_id;
	if (!bridges || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !actions || !terms || !type_declarations ||
		!judgement || !source || !request || !p_bridge_id ||
		prototype_kernel_builder_validate(kernel) != 0 ||
		source_context_id == prototype_context_empty(contexts) ||
		request->kind != PROTOTYPE_HOTT_ACTION_RELATION_TYPE ||
		hott_action_result_for_request(
			actions, relation_type_action_request_id, &result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, result_id);
	const struct prototype_hott_action_certificate* type_certificate = result &&
		result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_claim* source_type =
		prototype_judgement_claim_get(
			judgement, request->key.relation_type.source_claim_id
		);
	const struct prototype_hott_bridge* parent = type_certificate ?
		prototype_hott_bridge_db_get(
			bridges, request->key.relation_type.source_bridge_id
		) : NULL;
	if (!type_certificate ||
		type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE ||
		!source_type || !parent || source->parent != parent->source_context_id ||
		prototype_judgement_proposition_get(judgement, source_type->proposition_id)->context_id != source->parent ||
		prototype_judgement_proposition_get(judgement, source_type->proposition_id)->subject != prototype_context_classifier_term(source)) {
		return -1;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* existing =
			&bridges->certificates[i];
		if (bridges->bridges[i].source_context_id == source_context_id &&
			existing->semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION &&
			existing->parent_bridge_id == parent->id &&
			existing->fiber_action_certificate_id == result->certificate_id) {
			*p_bridge_id = i;
			return 0;
		}
	}
	const struct prototype_hott_relation_type_action_certificate* type =
		&type_certificate->data.relation_type;
	return hott_bridge_db_finish_extension(
		bridges,
		kernel,
		parent,
		source_context_id,
		type->endpoint_context_id,
		type->left_endpoint_binding_id,
		type->right_endpoint_binding_id,
		type->relation_type_term_id,
		type->relation_is_type_claim_id,
		result->certificate_id,
		type->left_context_certificate_id,
		type->right_context_certificate_id,
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION,
		p_bridge_id
	);
}

int prototype_hott_bridge_db_construct_identity_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t identity_type_action_request_id,
	uint32_t* p_bridge_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, identity_type_action_request_id);
	uint32_t result_id;
	if (!bridges || !contexts || !judgement || !actions || !source || !request ||
		!p_bridge_id || prototype_kernel_builder_validate(kernel) != 0 ||
		source_context_id == prototype_context_empty(contexts) ||
		request->kind != PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		hott_action_result_for_request(
			actions, identity_type_action_request_id, &result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, result_id);
	const struct prototype_hott_action_certificate* type_certificate = result &&
		result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_proposition* source_type =
		prototype_judgement_claim_proposition(
			judgement, request->key.identity_type.source_claim_id
		);
	const struct prototype_hott_bridge* parent = type_certificate ?
		prototype_hott_bridge_db_get(
			bridges, request->key.identity_type.source_bridge_id
		) : NULL;
	const struct prototype_hott_bridge_certificate* parent_certificate = parent ?
		&bridges->certificates[parent->id] : NULL;
	if (!type_certificate || type_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		!source_type || !parent || !parent_certificate ||
		(parent_certificate->semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY &&
		 parent_certificate->semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) ||
		source->parent != parent->source_context_id ||
		source_type->context_id != source->parent ||
		source_type->subject != prototype_context_classifier_term(source)) {
		return -1;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* existing =
			&bridges->certificates[i];
		if (bridges->bridges[i].source_context_id == source_context_id &&
			existing->semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY &&
			existing->parent_bridge_id == parent->id &&
			existing->fiber_action_certificate_id == result->certificate_id) {
			*p_bridge_id = i;
			return 0;
		}
	}
	const struct prototype_hott_identity_type_computation_certificate* type =
		&type_certificate->data.identity_type;
	return hott_bridge_db_finish_extension(
		bridges,
		kernel,
		parent,
		source_context_id,
		type->endpoint_context_id,
		type->left_endpoint_binding_id,
		type->right_endpoint_binding_id,
		type->identity_type_term_id,
		type->identity_type_is_type_claim_id,
		result->certificate_id,
		type->left_context_certificate_id,
		type->right_context_certificate_id,
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY,
		p_bridge_id
	);
}

int prototype_hott_bridge_db_ensure_identity_context(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t* p_bridge_id,
	int* p_residual_reason
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_hott_action_request request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = PROTOTYPE_INVALID_ID
		}
	};
	struct prototype_kernel_view view;
	if (!bridges || !kernel || !actions || !contexts || !cwf_certificates ||
		!terms || !p_bridge_id || !p_residual_reason ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	int semantics = source_context_id == prototype_context_empty(contexts) ?
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY :
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY;
	if (hott_bridge_for_source_context_and_semantics(
			bridges, source_context_id, semantics, p_bridge_id
		)) {
		*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
		return 0;
	}
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_cwf_certificate* source_certificate =
		hott_context_formation_certificate_for_context(
			cwf_certificates, source_context_id
		);
	if (!source || !source_certificate ||
		source->parent == PROTOTYPE_INVALID_ID ||
		source_certificate->claim_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t parent_bridge_id;
	int parent_status = prototype_hott_bridge_db_ensure_identity_context(
		bridges,
		kernel,
		actions,
		source->parent,
		&parent_bridge_id,
		p_residual_reason
	);
	if (parent_status != 0) {
		return parent_status;
	}
	request.key.identity_type.source_claim_id = source_certificate->claim_id;
	request.key.identity_type.source_bridge_id = parent_bridge_id;
	uint32_t request_id;
	uint32_t result_id;
	int request_status = prototype_hott_action_request_intern(
		actions, &view, bridges, request, &request_id
	);
	int execution_status = request_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			actions, kernel, bridges, request_id, &result_id
		) : -1;
	if (request_status != 0 || execution_status != 0) {
		fprintf(
			stderr,
			"identity Context bridge action failed context=%u parent=%u claim=%u "
			"parent_bridge=%u request=%u request_status=%d execution_status=%d\n",
			source_context_id,
			source->parent,
			source_certificate->claim_id,
			parent_bridge_id,
			request_status == 0 ? request_id : PROTOTYPE_INVALID_ID,
			request_status,
			execution_status
		);
		return -1;
	}
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, result_id);
	if (!result) {
		return -1;
	}
	if (result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = result->outcome.residual_reason;
		return 1;
	}
	if (prototype_hott_bridge_db_construct_identity_extension(
			bridges,
			kernel,
			actions,
			source_context_id,
			request_id,
			p_bridge_id
		) != 0) {
		fprintf(
			stderr,
			"identity Context extension failed context=%u request=%u\n",
			source_context_id,
			request_id
		);
		return -1;
	}
	*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	return 0;
}

static int hott_relation_plan_and_execute_depth(
	struct prototype_hott_relation_goal_db* goals,
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
	struct prototype_hott_relation_execution* p_execution,
	uint32_t depth
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!goals || !candidates || !work || !actions || !contexts ||
		!substitutions || !cwf_certificates || !cwf_certificates ||
		!bridges || !terms || !type_declarations || !operations || !judgement ||
		!p_execution || depth > 512 ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	struct prototype_hott_relation_execution execution = {
		.work_item_id = PROTOTYPE_INVALID_ID,
		.relation_type_action_request_id = PROTOTYPE_INVALID_ID,
		.relation_type_action_result_id = PROTOTYPE_INVALID_ID,
		.term_action_request_id = PROTOTYPE_INVALID_ID,
		.term_action_result_id = PROTOTYPE_INVALID_ID,
		.materialization_state = PROTOTYPE_HOTT_MATERIALIZATION_INVALID,
		.relation_witness_claim_id = PROTOTYPE_INVALID_ID
	};
	if (prototype_hott_relation_plan(
			goals,
			candidates,
			work,
			&view,
			bridges,
			definitions,
			goal_id,
			source_ast,
			normalization_profile,
			step_limit,
			&execution.work_item_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_work_item* item =
		prototype_hott_work_db_get(work, execution.work_item_id);
	const struct prototype_hott_relation_goal* goal =
		prototype_hott_relation_goal_db_get(goals, goal_id);
	if (!item || !goal) {
		return -1;
	}
	if (item->outcome.state != PROTOTYPE_HOTT_WORK_READY) {
		*p_execution = execution;
		return 0;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = goal->left_carrier_claim_id,
			.source_bridge_id = goal->bridge_id
		}
	};
	if (prototype_hott_action_request_intern(
			actions, &view, bridges,
			type_request,
			&execution.relation_type_action_request_id
		) != 0 || prototype_hott_execute_relation_type_action(
			actions, kernel, bridges,
			execution.relation_type_action_request_id,
			&execution.relation_type_action_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(
			actions, execution.relation_type_action_result_id
		);
	const struct prototype_hott_candidate* selected =
		prototype_hott_candidate_db_get(
			candidates, item->selected_candidate_id
		);
	if (!type_result || !selected) {
		return -1;
	}
	if (type_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
		*p_execution = execution;
		return 0;
	}
	if (selected->object_result ==
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_EMPTY_FAMILY) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY;
		*p_execution = execution;
		return 0;
	}
	if (selected->object_result !=
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
		*p_execution = execution;
		return 0;
	}
	const struct prototype_hott_action_certificate* type_certificate =
		type_result->certificate_id < actions->certificate_count ?
		&actions->certificates[type_result->certificate_id] : NULL;
	const struct prototype_hott_bridge* bridge =
		prototype_hott_bridge_db_get(bridges, goal->bridge_id);
	if (!type_certificate || type_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE || !bridge) {
		return -1;
	}
	if (selected->rule == PROTOTYPE_HOTT_RULE_REL_CONVERT) {
		struct hott_relation_instantiation instantiation;
		if (hott_instantiate_relation_for_claims(
				kernel,
				bridge,
				&type_certificate->data.relation_type,
				goal->left_claim_id,
				goal->right_claim_id,
				goal->left_carrier_claim_id,
				goal->right_carrier_claim_id,
				&instantiation
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* left_endpoint =
			prototype_judgement_claim_proposition(
				judgement, instantiation.left_endpoint_claim_id
			);
		const struct prototype_judgement_proposition* right_endpoint =
			prototype_judgement_claim_proposition(
				judgement, instantiation.right_endpoint_claim_id
			);
		uint32_t witness;
		if (!left_endpoint || !right_endpoint ||
			prototype_term_relation_witness(
				terms,
				left_endpoint->subject,
				right_endpoint->subject,
				&witness
			) != 0 || prototype_judgement_add_relation_witness_intro(
				judgement,
				terms,
				type_declarations,
				bridge->bridge_context_id,
				witness,
				instantiation.relation_type_term_id,
				instantiation.relation_is_type_claim_id,
				instantiation.left_endpoint_claim_id,
				instantiation.right_endpoint_claim_id,
				&execution.relation_witness_claim_id
			) != 0) {
			return -1;
		}
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS;
		*p_execution = execution;
		return 0;
	}
	if (selected->rule == PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR) {
		if (selected->child_edge_count >
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		uint32_t field_witness_claims[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		];
		for (uint32_t i = 0; i < selected->child_edge_count; ++i) {
			const struct prototype_hott_child_edge* edge =
				&candidates->child_edges[selected->first_child_edge + i];
			struct prototype_hott_relation_execution child_execution;
			if (edge->candidate_id != selected->id ||
				edge->role != PROTOTYPE_HOTT_CHILD_ADT_FIELD ||
				edge->ordinal != i ||
				hott_relation_plan_and_execute_depth(
					goals,
					candidates,
					work,
					actions,
					kernel,
					bridges,
					definitions,
					edge->child_goal_id,
					source_ast,
					normalization_profile,
					step_limit,
					&child_execution,
					depth + 1
				) != 0) {
				return -1;
			}
			if (child_execution.materialization_state ==
					PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY) {
				execution.materialization_state =
					PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY;
				*p_execution = execution;
				return 0;
			}
			if (child_execution.materialization_state !=
					PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS ||
				child_execution.relation_witness_claim_id == PROTOTYPE_INVALID_ID) {
				execution.materialization_state =
					PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
				*p_execution = execution;
				return 0;
			}
			field_witness_claims[i] =
				child_execution.relation_witness_claim_id;
		}
		struct hott_relation_instantiation instantiation;
		if (hott_instantiate_relation_for_claims(
				kernel,
				bridge,
				&type_certificate->data.relation_type,
				goal->left_claim_id,
				goal->right_claim_id,
				goal->left_carrier_claim_id,
				goal->right_carrier_claim_id,
				&instantiation
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* left_endpoint =
			prototype_judgement_claim_proposition(
				judgement, instantiation.left_endpoint_claim_id
			);
		const struct prototype_judgement_proposition* right_endpoint =
			prototype_judgement_claim_proposition(
				judgement, instantiation.right_endpoint_claim_id
			);
		uint32_t witness;
		if (!left_endpoint || !right_endpoint ||
			prototype_term_relation_witness(
				terms,
				left_endpoint->subject,
				right_endpoint->subject,
				&witness
			) != 0 || prototype_judgement_add_relation_constructor_witness(
				judgement,
				terms,
				bridge->bridge_context_id,
				witness,
				instantiation.relation_type_term_id,
				instantiation.relation_is_type_claim_id,
				instantiation.left_endpoint_claim_id,
				instantiation.right_endpoint_claim_id,
				field_witness_claims,
				selected->child_edge_count,
				&execution.relation_witness_claim_id
			) != 0) {
			return -1;
		}
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS;
		*p_execution = execution;
		return 0;
	}
	if (selected->rule != PROTOTYPE_HOTT_RULE_REL_DIAGONAL) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
		*p_execution = execution;
		return 0;
	}
	struct prototype_hott_action_request term_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = goal->left_claim_id,
			.source_bridge_id = goal->bridge_id,
			.relation_type_action_request_id = execution.relation_type_action_request_id
		}
	};
	if (prototype_hott_action_request_intern(
			actions, &view, bridges,
			term_request,
			&execution.term_action_request_id
		) != 0 || prototype_hott_execute_term_action(
			actions, kernel, bridges,
			execution.term_action_request_id,
			&execution.term_action_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* term_result =
		prototype_hott_action_result_get(
			actions, execution.term_action_result_id
		);
	const struct prototype_hott_action_certificate* term_certificate =
		term_result && term_result->outcome.state ==
			PROTOTYPE_HOTT_ACTION_RESULT_READY &&
		term_result->certificate_id < actions->certificate_count ?
		&actions->certificates[term_result->certificate_id] : NULL;
	if (!term_certificate || term_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
		*p_execution = execution;
		return 0;
	}
	execution.materialization_state =
		PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS;
	execution.relation_witness_claim_id =
		term_certificate->data.term.witness_has_type_claim_id;
	*p_execution = execution;
	return 0;
}

int prototype_hott_relation_plan_and_execute(
	struct prototype_hott_relation_goal_db* goals,
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
	struct prototype_hott_relation_execution* p_execution
) {
	return hott_relation_plan_and_execute_depth(
		goals,
		candidates,
		work,
		actions,
		kernel,
		bridges,
		definitions,
		goal_id,
		source_ast,
		normalization_profile,
		step_limit,
		p_execution,
		0
	);
}

int prototype_hott_action_db_validate(
	const struct prototype_hott_action_db* db,
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
	if (!db || db->request_count > db->request_capacity ||
		db->certificate_count > db->certificate_capacity ||
		db->result_count > db->result_capacity ||
		(db->request_count != 0 && !db->requests) ||
		(db->certificate_count != 0 && !db->certificates) ||
		(db->result_count != 0 && !db->results) ||
		prototype_kernel_view_validate(kernel) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* bridge_certificate =
			&bridges->certificates[i];
		if (bridge_certificate->parent_bridge_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (bridge_certificate->fiber_action_certificate_id >=
			db->certificate_count) {
			return -1;
		}
		const struct prototype_hott_action_certificate* type_certificate =
			&db->certificates[bridge_certificate->fiber_action_certificate_id];
		const struct prototype_hott_action_request* type_request =
			prototype_hott_action_request_get(db, type_certificate->request_id);
		uint32_t source_claim_id = PROTOTYPE_INVALID_ID;
		uint32_t source_bridge_id = PROTOTYPE_INVALID_ID;
		uint32_t endpoint_context_id = PROTOTYPE_INVALID_ID;
		uint32_t fiber_type_term_id = PROTOTYPE_INVALID_ID;
		uint32_t fiber_type_claim_id = PROTOTYPE_INVALID_ID;
		uint32_t left_context_certificate_id = PROTOTYPE_INVALID_ID;
		uint32_t right_context_certificate_id = PROTOTYPE_INVALID_ID;
		if (bridge_certificate->semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION &&
			type_certificate->kind ==
				PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE &&
			type_request && type_request->kind ==
				PROTOTYPE_HOTT_ACTION_RELATION_TYPE) {
			source_claim_id = type_request->key.relation_type.source_claim_id;
			source_bridge_id = type_request->key.relation_type.source_bridge_id;
			endpoint_context_id =
				type_certificate->data.relation_type.endpoint_context_id;
			fiber_type_term_id =
				type_certificate->data.relation_type.relation_type_term_id;
			fiber_type_claim_id =
				type_certificate->data.relation_type.relation_is_type_claim_id;
			left_context_certificate_id =
				type_certificate->data.relation_type.left_context_certificate_id;
			right_context_certificate_id =
				type_certificate->data.relation_type.right_context_certificate_id;
		} else if (bridge_certificate->semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY &&
			type_certificate->kind ==
				PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION &&
			type_request && type_request->kind ==
				PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION) {
			source_claim_id = type_request->key.identity_type.source_claim_id;
			source_bridge_id = type_request->key.identity_type.source_bridge_id;
			endpoint_context_id =
				type_certificate->data.identity_type.endpoint_context_id;
			fiber_type_term_id =
				type_certificate->data.identity_type.identity_type_term_id;
			fiber_type_claim_id =
				type_certificate->data.identity_type.identity_type_is_type_claim_id;
			left_context_certificate_id =
				type_certificate->data.identity_type.left_context_certificate_id;
			right_context_certificate_id =
				type_certificate->data.identity_type.right_context_certificate_id;
		} else {
			return -1;
		}
		const struct prototype_judgement_claim* source_type =
			prototype_judgement_claim_get(judgement, source_claim_id);
		const struct prototype_context* source = prototype_context_get(
			contexts, bridges->bridges[i].source_context_id
		);
		const struct prototype_context* relation = prototype_context_get(
			contexts, bridges->bridges[i].bridge_context_id
		);
		if (source_bridge_id != bridge_certificate->parent_bridge_id ||
			!source_type || !source || !relation ||
			prototype_judgement_proposition_get(judgement, source_type->proposition_id)->context_id != source->parent ||
			prototype_judgement_proposition_get(judgement, source_type->proposition_id)->subject != prototype_context_classifier_term(source) ||
			endpoint_context_id != relation->parent ||
			fiber_type_term_id != prototype_context_classifier_term(relation) ||
			bridge_certificate->left_endpoint_context_certificate_id !=
				left_context_certificate_id ||
			bridge_certificate->right_endpoint_context_certificate_id !=
				right_context_certificate_id ||
			bridge_certificate->relation_context_certificate_id >=
				cwf_certificates->certificate_count ||
			cwf_certificates->certificates[
				bridge_certificate->relation_context_certificate_id
			].claim_id != fiber_type_claim_id) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < db->request_count; ++i) {
		const struct prototype_hott_action_request* request = &db->requests[i];
		if (!hott_action_request_is_valid(
				db,
				request,
				i,
				contexts,
				substitutions,
				cwf_certificates,
				bridges,
				terms,
				type_declarations,
				operations,
				judgement
			)) {
			return -1;
		}
		size_t bucket =
			request->key_hash % PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT;
		uint32_t found = PROTOTYPE_INVALID_ID;
		for (uint32_t j = db->request_index_heads[bucket];
			j != PROTOTYPE_INVALID_ID;
			j = db->requests[j].hash_next) {
			if (j >= db->request_count) {
				return -1;
			}
			if (hott_action_request_key_equal(request, &db->requests[j])) {
				if (found != PROTOTYPE_INVALID_ID) {
					return -1;
				}
				found = j;
			}
		}
		if (found != i) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (!hott_action_certificate_is_valid(
				db,
				&db->certificates[i],
				i,
				contexts,
				substitutions,
				cwf_certificates,
				bridges,
				terms,
				type_declarations,
				operations,
				judgement
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (db->certificates[j].request_id ==
				db->certificates[i].request_id) {
				return -1;
			}
		}
	}
	for (uint32_t i = 0; i < db->result_count; ++i) {
		const struct prototype_hott_action_result* result = &db->results[i];
		if (result->id != i ||
			!prototype_hott_action_request_get(db, result->request_id) ||
			!hott_deterministic_outcome_is_valid(&result->outcome, 0) ||
			(result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
			 (result->certificate_id >= db->certificate_count ||
			  db->certificates[result->certificate_id].request_id !=
				result->request_id)) ||
			(result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY &&
			 result->certificate_id != PROTOTYPE_INVALID_ID)) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (db->results[j].request_id == result->request_id) {
				return -1;
			}
		}
	}
	return 0;
}
