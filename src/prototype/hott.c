#include "ast.h"
#include "hott.h"
#include "calculus.h"

#include <stdlib.h>
#include <string.h>

static int hott_category_is_valid(int category) {
	return category == PROTOTYPE_HOTT_OBSERVATION_VALUE ||
		category == PROTOTYPE_HOTT_OBSERVATION_COMPUTATION;
}

static int hott_work_state_is_valid(int state) {
	return state >= PROTOTYPE_HOTT_WORK_PENDING &&
		state <= PROTOTYPE_HOTT_WORK_UNSUPPORTED;
}

static int hott_residual_reason_is_valid(int reason) {
	return reason >= PROTOTYPE_HOTT_RESIDUAL_NONE &&
		reason <= PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
}

static int hott_rule_is_valid(int rule) {
	return rule >= PROTOTYPE_HOTT_RULE_OBS_DIAGONAL &&
		rule <= PROTOTYPE_HOTT_RULE_OBS_REINDEX;
}

static int hott_role_is_valid(int role) {
	return role >= PROTOTYPE_HOTT_CHILD_ADT_FIELD &&
		role <= PROTOTYPE_HOTT_CHILD_REINDEX_NATURALITY;
}

static int hott_rule_allows_role(int rule, int role) {
	switch (rule) {
	case PROTOTYPE_HOTT_RULE_OBS_DIAGONAL:
	case PROTOTYPE_HOTT_RULE_OBS_ADT_DISTINCT:
		return 0;
	case PROTOTYPE_HOTT_RULE_OBS_CONVERT:
		return role == PROTOTYPE_HOTT_CHILD_CONVERT_ANCHOR_OBSERVATION;
	case PROTOTYPE_HOTT_RULE_OBS_ADT_CONSTRUCTOR:
		return role == PROTOTYPE_HOTT_CHILD_ADT_FIELD ||
			role == PROTOTYPE_HOTT_CHILD_ADT_DEPENDENT_REINDEX;
	case PROTOTYPE_HOTT_RULE_OBS_MATCH_ACTION:
		return role >= PROTOTYPE_HOTT_CHILD_MATCH_SCRUTINEE &&
			role <= PROTOTYPE_HOTT_CHILD_MATCH_RECURSIVE_IH;
	case PROTOTYPE_HOTT_RULE_OBS_COMP_RETURN:
		return role >= PROTOTYPE_HOTT_CHILD_COMP_LEFT_RETURN_EXPOSURE &&
			role <= PROTOTYPE_HOTT_CHILD_COMP_RESULT_OBSERVATION;
	case PROTOTYPE_HOTT_RULE_OBS_PI_POINTWISE:
		return role >= PROTOTYPE_HOTT_CHILD_PI_DOMAIN_ACTION &&
			role <= PROTOTYPE_HOTT_CHILD_PI_CODOMAIN_OBSERVATION;
	case PROTOTYPE_HOTT_RULE_OBS_THUNK_PURE:
		return role == PROTOTYPE_HOTT_CHILD_THUNK_COMPUTATION_OBSERVATION;
	case PROTOTYPE_HOTT_RULE_OBS_REINDEX:
		return role >= PROTOTYPE_HOTT_CHILD_CONTEXT_ACTION &&
			role <= PROTOTYPE_HOTT_CHILD_REINDEX_NATURALITY;
	default:
		return 0;
	}
}

static int hott_action_kind_is_valid(int kind) {
	return kind >= PROTOTYPE_HOTT_ACTION_CONTEXT &&
		kind <= PROTOTYPE_HOTT_ACTION_TERM;
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
	if (!claim || claim->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		claim->context_id != context_id || claim->subject != subject ||
		claim->operation_id != PROTOTYPE_INVALID_ID ||
		claim->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ||
		claim->classifier >= terms->term_count ||
		prototype_judgement_classifier_value_whnf(
			terms, type_declarations, claim->classifier, &classifier
		) != 0) {
		return 0;
	}
	return classifier < terms->term_count &&
		terms->terms[classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR;
}

static int hott_operation_matches_claim(
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_claim* claim
) {
	if (!claim) {
		return 0;
	}
	if (claim->operation_id == PROTOTYPE_INVALID_ID) {
		return claim->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION;
	}
	if (!operations || claim->operation_id >= operations->operation_count ||
		claim->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ||
		claim->authority_id != claim->operation_id) {
		return 0;
	}
	const struct prototype_operation_node* operation =
		&operations->operations[claim->operation_id];
	return operation->context_id == claim->context_id &&
		operation->core_term == claim->subject &&
		operation->classifier == claim->classifier;
}

void prototype_context_formation_certificate_db_init(
	struct prototype_context_formation_certificate_db* db,
	struct prototype_context_formation_certificate* certificates,
	size_t certificate_capacity
) {
	if (!db) {
		return;
	}
	db->certificates = certificates;
	db->certificate_count = 0;
	db->certificate_capacity = certificate_capacity;
}

static int context_formation_certificate_is_valid(
	const struct prototype_context_formation_certificate* certificate,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!certificate || !contexts || !terms || !type_declarations || !judgement ||
		certificate->context_id == prototype_context_empty(contexts)) {
		return 0;
	}
	const struct prototype_context* context = prototype_context_get(
		contexts, certificate->context_id
	);
	uint32_t classifier = prototype_context_classifier_term(context);
	return context && classifier != PROTOTYPE_INVALID_ID &&
		prototype_context_classifier_variable(context) == PROTOTYPE_INVALID_ID &&
		hott_is_type_claim_matches(
			terms,
			type_declarations,
			judgement,
			certificate->classifier_claim_id,
			context->parent,
			classifier
		);
}

int prototype_context_formation_certificate_db_add(
	struct prototype_context_formation_certificate_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t classifier_claim_id,
	uint32_t* p_certificate_id
) {
	struct prototype_context_formation_certificate certificate = {
		.context_id = context_id,
		.classifier_claim_id = classifier_claim_id
	};
	if (!db || !p_certificate_id || !db->certificates ||
		!context_formation_certificate_is_valid(
			&certificate, contexts, terms, type_declarations, judgement
		)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (db->certificates[i].context_id == context_id &&
			db->certificates[i].classifier_claim_id == classifier_claim_id) {
			*p_certificate_id = i;
			return 0;
		}
	}
	if (db->certificate_count >= db->certificate_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)db->certificate_count++;
	db->certificates[id] = certificate;
	*p_certificate_id = id;
	return 0;
}

int prototype_context_formation_certificate_db_validate(
	const struct prototype_context_formation_certificate_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || db->certificate_count > db->certificate_capacity ||
		(db->certificate_count != 0 && !db->certificates)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (!context_formation_certificate_is_valid(
				&db->certificates[i], contexts, terms, type_declarations, judgement
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (db->certificates[i].context_id ==
					db->certificates[j].context_id &&
				db->certificates[i].classifier_claim_id ==
					db->certificates[j].classifier_claim_id) {
				return -1;
			}
		}
	}
	return 0;
}

static int context_has_formation_certificate(
	const struct prototype_context_formation_certificate_db* certificates,
	uint32_t context_id
) {
	if (!certificates) {
		return 0;
	}
	for (uint32_t i = 0; i < certificates->certificate_count; ++i) {
		if (certificates->certificates[i].context_id == context_id) {
			return 1;
		}
	}
	return 0;
}

static int hott_context_is_formed(
	const struct prototype_context_db* contexts,
	const struct prototype_context_formation_certificate_db* certificates,
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
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* certificates
) {
	if (!bridge || !contexts || !substitutions || !context_certificates ||
		!certificates || bridge->id != expected_id ||
		bridge->source_context_id >= contexts->context_count ||
		bridge->bridge_context_id >= contexts->context_count ||
		!hott_context_is_formed(
			contexts, context_certificates, bridge->source_context_id
		) || !hott_context_is_formed(
			contexts, context_certificates, bridge->bridge_context_id
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
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* certificates,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	uint32_t source_context_id,
	uint32_t* p_bridge_id
) {
	if (!db || !db->bridges || !contexts || !substitutions ||
		!context_certificates || !certificates || !terms || !type_declarations ||
		!judgement || !p_bridge_id ||
		source_context_id >= contexts->context_count ||
		prototype_context_formation_certificate_db_validate(
			context_certificates, contexts, terms, type_declarations, judgement
		) != 0 ||
		prototype_substitution_certificate_db_validate(
			certificates, substitutions, judgement
		) != 0 ||
		!hott_context_is_formed(contexts, context_certificates, source_context_id)) {
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
			&bridge, bridge.id, contexts, substitutions, context_certificates,
			certificates
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
		.parent_bridge_id = PROTOTYPE_INVALID_ID,
		.type_action_certificate_id = PROTOTYPE_INVALID_ID,
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
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* certificates,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || db->bridge_count > db->bridge_capacity ||
		db->certificate_count > db->certificate_capacity ||
		db->certificate_count != db->bridge_count ||
		(db->bridge_count != 0 && !db->bridges) ||
		(db->certificate_count != 0 && !db->certificates) ||
		prototype_context_formation_certificate_db_validate(
			context_certificates, contexts, terms, type_declarations, judgement
		) != 0 || prototype_substitution_certificate_db_validate(
			certificates, substitutions, judgement
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < db->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* certificate =
			&db->certificates[i];
		if (!hott_bridge_record_is_valid(
				&db->bridges[i], i, contexts, substitutions,
				context_certificates, certificates
			) || certificate->id != i || certificate->bridge_id != i ||
			(certificate->parent_bridge_id == PROTOTYPE_INVALID_ID) !=
				(db->bridges[i].source_context_id ==
				 prototype_context_empty(contexts))) {
			return -1;
		}
		if (certificate->parent_bridge_id == PROTOTYPE_INVALID_ID &&
			(certificate->type_action_certificate_id != PROTOTYPE_INVALID_ID ||
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
			const struct prototype_substitution_certificate* left_certificate =
				prototype_substitution_certificate_db_get(
					certificates, certificate->left_substitution_certificate_id
				);
			const struct prototype_substitution_certificate* right_certificate =
				prototype_substitution_certificate_db_get(
					certificates, certificate->right_substitution_certificate_id
				);
			if (!source || !relation || !parent ||
				certificate->parent_bridge_id >= i ||
				source->parent != parent->source_context_id ||
				relation->parent == prototype_context_empty(contexts) ||
				certificate->type_action_certificate_id == PROTOTYPE_INVALID_ID ||
				certificate->left_endpoint_context_certificate_id >=
					context_certificates->certificate_count ||
				certificate->right_endpoint_context_certificate_id >=
					context_certificates->certificate_count ||
				certificate->relation_context_certificate_id >=
					context_certificates->certificate_count ||
				context_certificates->certificates[
					certificate->right_endpoint_context_certificate_id
				].context_id != relation->parent ||
				context_certificates->certificates[
					certificate->left_endpoint_context_certificate_id
				].context_id != prototype_context_get(
					contexts, relation->parent
				)->parent ||
				context_certificates->certificates[
					certificate->relation_context_certificate_id
				].context_id != db->bridges[i].bridge_context_id ||
				!left_certificate || !right_certificate ||
				left_certificate->substitution_id !=
					db->bridges[i].left_substitution_id ||
				right_certificate->substitution_id !=
					db->bridges[i].right_substitution_id) {
				return -1;
			}
		}
	}
	return 0;
}

void prototype_hott_observation_goal_db_init(
	struct prototype_hott_observation_goal_db* db,
	struct prototype_hott_observation_goal* goals,
	size_t goal_capacity
) {
	if (!db) {
		return;
	}
	db->goals = goals;
	db->goal_count = 0;
	db->goal_capacity = goal_capacity;
}

const struct prototype_hott_observation_goal*
prototype_hott_observation_goal_db_get(
	const struct prototype_hott_observation_goal_db* db,
	uint32_t goal_id
) {
	return db && goal_id < db->goal_count ? &db->goals[goal_id] : NULL;
}

static int hott_observation_goal_is_valid(
	const struct prototype_hott_observation_goal* goal,
	uint32_t expected_id,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	if (!goal || !hott_category_is_valid(goal->category) || goal->id != expected_id ||
		prototype_context_formation_certificate_db_validate(
			context_certificates, contexts, terms, type_declarations, judgement
		) != 0 || prototype_substitution_certificate_db_validate(
			certificates, substitutions, judgement
		) != 0) {
		return 0;
	}
	const struct prototype_judgement_claim* carrier =
		prototype_judgement_claim_get(judgement, goal->carrier_claim_id);
	const struct prototype_judgement_claim* left =
		prototype_judgement_claim_get(judgement, goal->left_claim_id);
	const struct prototype_judgement_claim* right =
		prototype_judgement_claim_get(judgement, goal->right_claim_id);
	if (!carrier || !left || !right || left->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		right->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		left->context_id != right->context_id ||
		left->classifier != right->classifier ||
		carrier->subject != left->classifier ||
		!hott_is_type_claim_matches(
			terms, type_declarations, judgement, goal->carrier_claim_id,
			left->context_id, carrier->subject
		) || !hott_operation_matches_claim(operations, left) ||
		!hott_operation_matches_claim(operations, right) ||
		!hott_context_is_formed(contexts, context_certificates, left->context_id)) {
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
		(goal->category == PROTOTYPE_HOTT_OBSERVATION_VALUE &&
		 left_category != PROTOTYPE_JUDGEMENT_CATEGORY_VALUE) ||
		(goal->category == PROTOTYPE_HOTT_OBSERVATION_COMPUTATION &&
		 left_category != PROTOTYPE_JUDGEMENT_CATEGORY_COMPUTATION)) {
		return 0;
	}
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, goal->bridge_id
	);
	return bridge && bridge->source_context_id == left->context_id &&
		hott_bridge_record_is_valid(
			bridge, goal->bridge_id, contexts, substitutions,
			context_certificates, certificates
		);
}

int prototype_hott_observation_goal_db_intern(
	struct prototype_hott_observation_goal_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement,
	int category,
	uint32_t carrier_claim_id,
	uint32_t left_claim_id,
	uint32_t right_claim_id,
	uint32_t bridge_id,
	uint32_t* p_goal_id
) {
	if (!db || !p_goal_id || !db->goals) {
		return -1;
	}
	struct prototype_hott_observation_goal goal = {
		.id = (uint32_t)db->goal_count,
		.category = category,
		.carrier_claim_id = carrier_claim_id,
		.left_claim_id = left_claim_id,
		.right_claim_id = right_claim_id,
		.bridge_id = bridge_id
	};
	if (!hott_observation_goal_is_valid(
			&goal, goal.id, contexts, substitutions, context_certificates,
			certificates, bridges, terms, type_declarations, operations, judgement
		)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->goal_count; ++i) {
		const struct prototype_hott_observation_goal* old = &db->goals[i];
		if (old->category == category &&
			old->carrier_claim_id == carrier_claim_id &&
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

int prototype_hott_observation_goal_db_validate(
	const struct prototype_hott_observation_goal_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || db->goal_count > db->goal_capacity ||
		(db->goal_count != 0 && !db->goals)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->goal_count; ++i) {
		if (!hott_observation_goal_is_valid(
				&db->goals[i], i, contexts, substitutions, context_certificates,
				certificates, bridges, terms, type_declarations, operations, judgement
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			const struct prototype_hott_observation_goal* a = &db->goals[i];
			const struct prototype_hott_observation_goal* b = &db->goals[j];
			if (a->category == b->category &&
				a->carrier_claim_id == b->carrier_claim_id &&
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
	const struct prototype_hott_observation_goal_db* goals,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	struct prototype_substitution_certificate_db* substitution_certificates,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	const struct prototype_operation_graph* operations
) {
	if (!db || !goals || !contexts || !substitutions || !context_certificates ||
		!substitution_certificates || !terms || !type_declarations || !judgement ||
		!operations || prototype_context_formation_certificate_db_validate(
			context_certificates, contexts, terms, type_declarations, judgement
		) != 0 || prototype_substitution_certificate_db_validate(
			substitution_certificates, substitutions, judgement
		) != 0 || db->candidate_count > db->candidate_capacity ||
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
				edge->certificate_id >= context_certificates->certificate_count) {
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
				edge->certificate_id >= substitution_certificates->certificate_count) {
				return -1;
			}
		}
		if ((candidate->rule == PROTOTYPE_HOTT_RULE_OBS_CONVERT &&
			 candidate->conversion_premise_count != 1) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_OBS_COMP_RETURN &&
			 candidate->conversion_premise_count != 2) ||
			(candidate->rule != PROTOTYPE_HOTT_RULE_OBS_CONVERT &&
			 candidate->rule != PROTOTYPE_HOTT_RULE_OBS_COMP_RETURN &&
			 candidate->conversion_premise_count != 0)) {
			return -1;
		}
		if ((candidate->rule == PROTOTYPE_HOTT_RULE_OBS_CONVERT &&
			 candidate->child_edge_count != 1) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_OBS_COMP_RETURN &&
			 candidate->child_edge_count != 1) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_OBS_THUNK_PURE &&
			 candidate->child_edge_count != 1) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_OBS_DIAGONAL &&
			 candidate->child_edge_count != 0) ||
			(candidate->rule == PROTOTYPE_HOTT_RULE_OBS_ADT_DISTINCT &&
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
	const struct prototype_hott_observation_goal_db* goals,
	const struct prototype_hott_candidate_db* candidates
) {
	if (!db || !goals || !candidates || db->item_count > db->item_capacity ||
		(db->item_count != 0 && !db->items)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->item_count; ++i) {
		const struct prototype_hott_work_item* item = &db->items[i];
		if (item->id != i || item->goal_id >= goals->goal_count ||
			!hott_work_state_is_valid(item->state) ||
			!hott_residual_reason_is_valid(item->residual_reason) ||
			strcmp(
				item->calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
			) != 0 ||
			((item->state == PROTOTYPE_HOTT_WORK_RESIDUAL ||
			  item->state == PROTOTYPE_HOTT_WORK_UNSUPPORTED) !=
			 (item->residual_reason != PROTOTYPE_HOTT_RESIDUAL_NONE)) ||
			(item->state == PROTOTYPE_HOTT_WORK_READY &&
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
		return PROTOTYPE_HOTT_RULE_OBS_MATCH_ACTION;
	}
	return terms->terms[left_head].as.constructor.constructor_id ==
		terms->terms[right_head].as.constructor.constructor_id ?
		PROTOTYPE_HOTT_RULE_OBS_ADT_CONSTRUCTOR :
		PROTOTYPE_HOTT_RULE_OBS_ADT_DISTINCT;
}

static int hott_plan_add_conversion(
	struct prototype_hott_observation_goal_db* goals,
	struct prototype_hott_candidate_db* candidates,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* substitution_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_hott_observation_goal* goal,
	uint32_t goal_id,
	uint32_t context_id,
	uint32_t carrier,
	uint32_t left,
	uint32_t right,
	int profile,
	uint64_t step_limit,
	uint64_t* p_steps
) {
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
			candidates, goal_id, PROTOTYPE_HOTT_RULE_OBS_CONVERT, &candidate_id
		) != 0 || hott_candidate_add_conversion(
			candidates, candidate_id,
			PROTOTYPE_HOTT_CHILD_CONVERT_ANCHOR_OBSERVATION, 0,
			request, revision
		) != 0) {
		return -1;
	}
	uint32_t anchor_goal;
	if (prototype_hott_observation_goal_db_intern(
			goals, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, goal->category, goal->carrier_claim_id,
			goal->left_claim_id, goal->left_claim_id, goal->bridge_id, &anchor_goal
		) != 0 || hott_candidate_add_child(
			candidates, candidate_id, anchor_goal,
			PROTOTYPE_HOTT_CHILD_CONVERT_ANCHOR_OBSERVATION, 0
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
		if (claim->kind != kind || claim->context_id != context_id ||
			claim->subject != subject || claim->classifier != classifier) {
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
			uint32_t claim_id = derivation->premise_claim_ids[j];
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(judgement, claim_id);
			if (!claim || claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				claim->subject != expected_subject ||
				claim->classifier != expected_classifier) {
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

int prototype_hott_observation_plan(
	struct prototype_hott_observation_goal_db* goals,
	struct prototype_hott_candidate_db* candidates,
	struct prototype_hott_work_db* work,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* substitution_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_work_item_id
) {
	const struct prototype_hott_observation_goal* goal =
		prototype_hott_observation_goal_db_get(goals, goal_id);
	if (!goal || !candidates || !work || !contexts || !substitutions ||
		!context_certificates || !substitution_certificates || !bridges || !terms ||
		!type_declarations || !operations || !judgement || !p_work_item_id ||
		!work->items ||
		work->item_count >= work->item_capacity) {
		return -1;
	}
	const struct prototype_judgement_claim* carrier_claim =
		prototype_judgement_claim_get(judgement, goal->carrier_claim_id);
	const struct prototype_judgement_claim* left_claim =
		prototype_judgement_claim_get(judgement, goal->left_claim_id);
	const struct prototype_judgement_claim* right_claim =
		prototype_judgement_claim_get(judgement, goal->right_claim_id);
	if (!carrier_claim || !left_claim || !right_claim ||
		carrier_claim->subject >= terms->term_count ||
		left_claim->subject >= terms->term_count ||
		right_claim->subject >= terms->term_count) {
		return -1;
	}
	uint32_t carrier = carrier_claim->subject;
	uint32_t left = left_claim->subject;
	uint32_t right = right_claim->subject;
	struct prototype_hott_work_item item = {
		.id = (uint32_t)work->item_count,
		.goal_id = goal_id,
		.state = PROTOTYPE_HOTT_WORK_PENDING,
		.selected_candidate_id = PROTOTYPE_INVALID_ID,
		.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
		.source_ast = source_ast,
		.normalization_profile = normalization_profile,
		.step_limit = step_limit,
		.term_graph_revision = terms->normalization_graph_revision
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
		item.calculus_fingerprint,
		PROTOTYPE_HOTT_CALCULUS_FINGERPRINT,
		sizeof(item.calculus_fingerprint)
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
		item.state = reason == PROTOTYPE_HOTT_RESIDUAL_EFFECTFUL ||
			reason == PROTOTYPE_HOTT_RESIDUAL_EFFECT_ROW_UNRESOLVED ?
			PROTOTYPE_HOTT_WORK_RESIDUAL : PROTOTYPE_HOTT_WORK_UNSUPPORTED;
		item.residual_reason = reason;
		goto commit;
	}
	const struct prototype_term* carrier_term = &terms->terms[carrier];
	if (carrier_term->tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		item.state = PROTOTYPE_HOTT_WORK_UNSUPPORTED;
		item.residual_reason = PROTOTYPE_HOTT_RESIDUAL_UNIVERSE;
		goto commit;
	}
	if (left == right) {
		uint32_t candidate_id;
		if (hott_candidate_add(
				candidates, goal_id, PROTOTYPE_HOTT_RULE_OBS_DIAGONAL,
				&candidate_id
			) != 0) {
			return -1;
		}
	}
	if (left != right) {
		status = hott_plan_add_conversion(
			goals, candidates, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			definitions, operations, judgement, goal, goal_id,
			left_claim->context_id, carrier, left, right, normalization_profile,
			step_limit, &item.steps_used
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
		if (hott_candidate_add(
				candidates, goal_id,
				hott_candidate_rule_for_type_view(terms, carrier, left, right),
				&candidate_id
			) != 0) {
			return -1;
		}
	} else if (carrier_term->tag == PROTOTYPE_TERM_PI) {
		reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
		status = hott_term_fragment_scan(terms, carrier, 0, &reason);
		if (status < 0) {
			return -1;
		}
		if (status > 0) {
			item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.residual_reason = reason;
			goto commit;
		}
		uint32_t candidate_id;
		if (hott_candidate_add(
				candidates, goal_id, PROTOTYPE_HOTT_RULE_OBS_PI_POINTWISE,
				&candidate_id
			) != 0) {
			return -1;
		}
	} else if (carrier_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		int purity = prototype_term_effect_row_purity(
			terms, carrier_term->as.computation_type.label
		);
		if (purity != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
			item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.residual_reason = purity == PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL ?
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
		item.steps_used += left_result.steps_used + right_result.steps_used;
		if (left_result.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			right_result.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			left_result.term_id >= terms->term_count ||
			right_result.term_id >= terms->term_count ||
			terms->terms[left_result.term_id].tag != PROTOTYPE_TERM_RETURN ||
			terms->terms[right_result.term_id].tag != PROTOTYPE_TERM_RETURN) {
			item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.residual_reason =
				PROTOTYPE_HOTT_RESIDUAL_NEUTRAL_PURE_COMPUTATION;
			goto commit;
		}
		uint32_t candidate_id;
		if (hott_candidate_add(
				candidates, goal_id, PROTOTYPE_HOTT_RULE_OBS_COMP_RETURN,
				&candidate_id
			) != 0 || hott_candidate_add_exposure(
				candidates, contexts, terms, type_declarations, definitions,
				candidate_id, left_claim->context_id, carrier, left,
				left_result.term_id,
				PROTOTYPE_HOTT_CHILD_COMP_LEFT_RETURN_EXPOSURE, 0,
				normalization_profile, step_limit, &item.steps_used
			) != 0 || hott_candidate_add_exposure(
				candidates, contexts, terms, type_declarations, definitions,
				candidate_id, right_claim->context_id, carrier, right,
				right_result.term_id,
				PROTOTYPE_HOTT_CHILD_COMP_RIGHT_RETURN_EXPOSURE, 0,
				normalization_profile, step_limit, &item.steps_used
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
			left_claim->context_id, result_classifier, carrier_claim->classifier,
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
			item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
			goto commit;
		}
		uint32_t result_goal;
		if (prototype_hott_observation_goal_db_intern(
				goals, contexts, substitutions, context_certificates,
				substitution_certificates, bridges, terms, type_declarations,
				operations, judgement, PROTOTYPE_HOTT_OBSERVATION_VALUE,
				result_carrier_claim, left_value_claim, right_value_claim,
				goal->bridge_id, &result_goal
			) != 0 || hott_candidate_add_child(
				candidates, candidate_id, result_goal,
				PROTOTYPE_HOTT_CHILD_COMP_RESULT_OBSERVATION, 0
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
			item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.residual_reason = reason;
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
		item.steps_used += left_result.steps_used + right_result.steps_used;
		if (left_result.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			right_result.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
			left_result.term_id >= terms->term_count ||
			right_result.term_id >= terms->term_count ||
			terms->terms[left_result.term_id].tag != PROTOTYPE_TERM_THUNK ||
			terms->terms[right_result.term_id].tag != PROTOTYPE_TERM_THUNK) {
			item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.residual_reason =
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
			left_claim->context_id, computation_carrier, carrier_claim->classifier,
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
			item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
			item.residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
			goto commit;
		}
		uint32_t candidate_id;
		if (hott_candidate_add(
				candidates, goal_id, PROTOTYPE_HOTT_RULE_OBS_THUNK_PURE,
				&candidate_id
			) != 0) {
			return -1;
		}
		uint32_t computation_goal;
		if (prototype_hott_observation_goal_db_intern(
				goals, contexts, substitutions, context_certificates,
				substitution_certificates, bridges, terms, type_declarations,
				operations, judgement, PROTOTYPE_HOTT_OBSERVATION_COMPUTATION,
				computation_carrier_claim, left_computation_claim,
				right_computation_claim, goal->bridge_id, &computation_goal
			) != 0 || hott_candidate_add_child(
				candidates, candidate_id, computation_goal,
				PROTOTYPE_HOTT_CHILD_THUNK_COMPUTATION_OBSERVATION, 0
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
		item.state = PROTOTYPE_HOTT_WORK_READY;
		item.selected_candidate_id = selected_candidate;
	} else if (carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_TEXT ||
		carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_INT ||
		carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_INT64) {
		item.state = PROTOTYPE_HOTT_WORK_UNSUPPORTED;
		item.residual_reason = PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE;
	} else {
		item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
		item.residual_reason = PROTOTYPE_HOTT_RESIDUAL_CONVERSION;
	}

commit:
	if (item.state != PROTOTYPE_HOTT_WORK_READY) {
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
		(item->state != PROTOTYPE_HOTT_WORK_RESIDUAL &&
		 item->state != PROTOTYPE_HOTT_WORK_UNSUPPORTED) ||
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
			(item->state != PROTOTYPE_HOTT_WORK_RESIDUAL &&
			 item->state != PROTOTYPE_HOTT_WORK_UNSUPPORTED) ||
			strcmp(
				item->calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
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
	case PROTOTYPE_HOTT_ACTION_TYPE:
		hash = hott_action_hash_mix(hash, request->key.type.source_claim_id);
		return hott_action_hash_mix(hash, request->key.type.source_bridge_id);
	case PROTOTYPE_HOTT_ACTION_TERM:
		hash = hott_action_hash_mix(hash, request->key.term.source_claim_id);
		hash = hott_action_hash_mix(hash, request->key.term.source_bridge_id);
		return hott_action_hash_mix(
			hash, request->key.term.type_action_request_id
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
	case PROTOTYPE_HOTT_ACTION_TYPE:
		return left->key.type.source_claim_id ==
				right->key.type.source_claim_id &&
			left->key.type.source_bridge_id ==
				right->key.type.source_bridge_id;
	case PROTOTYPE_HOTT_ACTION_TERM:
		return left->key.term.source_claim_id ==
				right->key.term.source_claim_id &&
			left->key.term.source_bridge_id ==
				right->key.term.source_bridge_id &&
			left->key.term.type_action_request_id ==
				right->key.term.type_action_request_id;
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

static int hott_action_request_is_valid(
	const struct prototype_hott_action_db* db,
	const struct prototype_hott_action_request* request,
	uint32_t expected_id,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
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
				context_certificates,
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
	case PROTOTYPE_HOTT_ACTION_TYPE:
		{
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(
					judgement, request->key.type.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.type.source_bridge_id
				);
			return claim && bridge &&
				bridge->source_context_id == claim->context_id &&
				hott_is_type_claim_matches(
					terms,
					type_declarations,
					judgement,
					request->key.type.source_claim_id,
					claim->context_id,
					claim->subject
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
				request->key.term.type_action_request_id < expected_id ?
				&db->requests[
					request->key.term.type_action_request_id
				] : NULL;
			const struct prototype_judgement_claim* type_claim =
				type_request && type_request->kind == PROTOTYPE_HOTT_ACTION_TYPE ?
				prototype_judgement_claim_get(
					judgement, type_request->key.type.source_claim_id
				) : NULL;
			return claim &&
				claim->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				hott_operation_matches_claim(operations, claim) &&
				bridge &&
				bridge->source_context_id == claim->context_id &&
				type_request &&
				type_request->key.type.source_bridge_id ==
					request->key.term.source_bridge_id &&
				type_claim &&
				type_claim->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				type_claim->context_id == claim->context_id &&
				type_claim->subject == claim->classifier;
		}
	default:
		return 0;
	}
}

int prototype_hott_action_request_intern(
	struct prototype_hott_action_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* substitution_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement,
	struct prototype_hott_action_request request,
	uint32_t* p_request_id
) {
	if (!db || !p_request_id || !db->requests ||
		db->request_count >= db->request_capacity ||
		prototype_context_formation_certificate_db_validate(
			context_certificates, contexts, terms, type_declarations, judgement
		) != 0 ||
		prototype_substitution_certificate_db_validate(
			substitution_certificates, substitutions, judgement
		) != 0) {
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
			context_certificates,
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

static int hott_action_certificate_is_valid(
	const struct prototype_hott_action_db* db,
	const struct prototype_hott_action_certificate* certificate,
	uint32_t expected_id,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* substitution_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	(void)substitutions;
	(void)substitution_certificates;
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
	case PROTOTYPE_HOTT_ACTION_CERTIFICATE_TYPE:
		{
			const struct prototype_judgement_claim* source =
				prototype_judgement_claim_get(
					judgement, request->key.type.source_claim_id
				);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(
					bridges, request->key.type.source_bridge_id
				);
			const struct prototype_hott_type_action_certificate* type =
				&certificate->data.type;
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
			if (request->kind != PROTOTYPE_HOTT_ACTION_TYPE || !source || !bridge ||
				!right_context || !left_context || !relation ||
				type->left_context_certificate_id >=
					context_certificates->certificate_count ||
				type->right_context_certificate_id >=
					context_certificates->certificate_count ||
				context_certificates->certificates[
					type->left_context_certificate_id
				].context_id != right_context->parent ||
				context_certificates->certificates[
					type->right_context_certificate_id
				].context_id != type->endpoint_context_id ||
				left_context->binding_id != type->left_endpoint_binding_id ||
				right_context->binding_id != type->right_endpoint_binding_id ||
				prototype_term_observation_type_info(
					terms,
					type->relation_type_term_id,
					&left_type,
					&right_type,
					&left_endpoint,
					&right_endpoint
				) != 0) {
				return 0;
			}
			return bridge->source_context_id == source->context_id &&
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
				relation->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				relation->context_id == type->endpoint_context_id &&
				relation->subject == type->relation_type_term_id &&
				relation->classifier == source->classifier;
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
				const struct prototype_substitution_certificate* extension =
					prototype_substitution_certificate_db_get(
						substitution_certificates,
						action->result_substitution_certificate_id
					);
				return extension && extension->substitution_id ==
					action->result_substitution_id;
			}
			return action->result_substitution_certificate_id ==
				PROTOTYPE_INVALID_ID;
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
					db, request->key.term.type_action_request_id
				);
			const struct prototype_hott_action_certificate* type_certificate = NULL;
			for (uint32_t i = 0; i < db->result_count; ++i) {
				const struct prototype_hott_action_result* result = &db->results[i];
				if (result->request_id == request->key.term.type_action_request_id &&
					result->state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
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
			const struct prototype_substitution_certificate* left_extension =
				prototype_substitution_certificate_db_get(
					substitution_certificates,
					term->left_endpoint_substitution_certificate_id
				);
			const struct prototype_substitution_certificate* right_extension =
				prototype_substitution_certificate_db_get(
					substitution_certificates,
					term->right_endpoint_substitution_certificate_id
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
				type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_TYPE ||
				!endpoint || !left_extension || !right_extension || !witness_claim ||
				!left_endpoint_claim || !right_endpoint_claim ||
				endpoint->source_context != bridge->bridge_context_id ||
				endpoint->target_context !=
					type_certificate->data.type.endpoint_context_id ||
				endpoint->kind != PROTOTYPE_SUBSTITUTION_EXTEND ||
				left_extension->substitution_id != endpoint->first ||
				right_extension->substitution_id !=
					term->endpoint_instantiation_substitution_id ||
				witness_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				witness_claim->context_id != bridge->bridge_context_id ||
				witness_claim->subject != term->witness_term_id) {
				return 0;
			}
			uint32_t relation;
			return prototype_term_reindex(
				(struct prototype_term_db*)terms,
				type_declarations,
				contexts,
				(struct prototype_substitution_db*)substitutions,
				type_certificate->data.type.relation_type_term_id,
				term->endpoint_instantiation_substitution_id,
				&relation
			) == 0 && relation == witness_claim->classifier;
		}
	default:
		return 0;
	}
}

int prototype_hott_action_certificate_add(
	struct prototype_hott_action_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* substitution_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement,
	struct prototype_hott_action_certificate certificate,
	uint32_t* p_certificate_id
) {
	if (!db || !p_certificate_id || !db->certificates ||
		db->certificate_count >= db->certificate_capacity) {
		return -1;
	}
	certificate.id = (uint32_t)db->certificate_count;
	if (!hott_action_certificate_is_valid(
			db,
			&certificate,
			certificate.id,
			contexts,
			substitutions,
			context_certificates,
			substitution_certificates,
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
	if (!db || !p_result_id || !db->results ||
		db->result_count >= db->result_capacity ||
		!prototype_hott_action_request_get(db, result.request_id) ||
		strcmp(
			result.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
		) != 0 ||
		result.state < PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		result.state > PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED) {
		return -1;
	}
	if (result.state == PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		if (result.residual_reason != PROTOTYPE_HOTT_RESIDUAL_NONE ||
			result.certificate_id >= db->certificate_count ||
			db->certificates[result.certificate_id].request_id !=
				result.request_id) {
			return -1;
		}
	} else if (!hott_residual_reason_is_valid(result.residual_reason) ||
		result.residual_reason == PROTOTYPE_HOTT_RESIDUAL_NONE ||
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
		source->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source->subject >= terms->term_count) {
		return -1;
	}
	struct prototype_hott_type_former_descriptor descriptor = {
		.kind = PROTOTYPE_HOTT_TYPE_FORMER_UNKNOWN,
		.admitted = 0,
		.type_action_rule = PROTOTYPE_HOTT_TYPE_ACTION_RULE_NONE,
		.residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
		.source_claim_id = source_claim_id,
		.source_type_term_id = source->subject
	};
	uint32_t exposed_type = source->subject;
	struct prototype_term_normalization_result exposure;
	if (prototype_term_normalize_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			source->subject,
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
		descriptor.type_action_rule = hott_zero_field_ordinary_adt(
			terms, type_declarations, contexts, exposed_type
		) ? PROTOTYPE_HOTT_TYPE_ACTION_RULE_ZERO_FIELD_ADT :
			PROTOTYPE_HOTT_TYPE_ACTION_RULE_ADT_TELESCOPE;
		descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	} else {
		uint32_t observation_left_type;
		uint32_t observation_right_type;
		uint32_t observation_left;
		uint32_t observation_right;
		switch (type->tag) {
		case PROTOTYPE_TERM_TYPE_VIEW:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT;
			descriptor.type_action_rule =
				PROTOTYPE_HOTT_TYPE_ACTION_RULE_ADT_TELESCOPE;
			break;
		case PROTOTYPE_TERM_PI:
			descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_PI;
			descriptor.type_action_rule =
				PROTOTYPE_HOTT_TYPE_ACTION_RULE_PI_POINTWISE;
			{
				int reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
				int status = hott_term_fragment_scan(
					terms, source->subject, 0, &reason
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
			descriptor.type_action_rule =
				PROTOTYPE_HOTT_TYPE_ACTION_RULE_PURE_COMPUTATION;
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
			descriptor.type_action_rule =
				PROTOTYPE_HOTT_TYPE_ACTION_RULE_THUNK;
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
			if (prototype_term_observation_type_info(
					terms,
					source->subject,
					&observation_left_type,
					&observation_right_type,
					&observation_left,
					&observation_right
				) == 0) {
				descriptor.kind = PROTOTYPE_HOTT_TYPE_FORMER_OBSERVATION;
				descriptor.admitted = 1;
				descriptor.type_action_rule =
					PROTOTYPE_HOTT_TYPE_ACTION_RULE_OBSERVATION_HIGHER;
				descriptor.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
			}
			break;
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
		.state = reason == PROTOTYPE_HOTT_RESIDUAL_UNIVERSE ||
			reason == PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE ?
			PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED :
			PROTOTYPE_HOTT_ACTION_RESULT_RESIDUAL,
		.residual_reason = reason,
		.certificate_id = PROTOTYPE_INVALID_ID,
		.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		.step_limit = 0,
		.term_graph_revision = terms->term_count
	};
	memcpy(result.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT, 65);
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
		.state = PROTOTYPE_HOTT_ACTION_RESULT_READY,
		.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
		.certificate_id = certificate_id,
		.normalization_profile = normalization_profile,
		.step_limit = step_limit,
		.term_graph_revision = terms->term_count
	};
	memcpy(result.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT, 65);
	if (prototype_hott_action_result_publish(actions, result, p_result_id) != 0) {
		memset(&actions->certificates[certificate_id], 0,
			sizeof(actions->certificates[certificate_id]));
		actions->certificate_count--;
		return -1;
	}
	return 0;
}

int prototype_hott_execute_type_action(
	struct prototype_hott_action_db* actions,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* substitution_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	if (!actions || !contexts || !substitutions || !context_certificates ||
		!substitution_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id) {
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
		request->kind == PROTOTYPE_HOTT_ACTION_TYPE ?
		prototype_judgement_claim_get(
			judgement, request->key.type.source_claim_id
		) : NULL;
	const struct prototype_hott_bridge* bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.type.source_bridge_id
		) : NULL;
	if (!source || !bridge || source->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source->context_id != bridge->source_context_id) {
		return -1;
	}
	struct prototype_hott_type_former_descriptor descriptor;
	if (prototype_hott_type_former_descriptor_query(
			terms,
			type_declarations,
			contexts,
			judgement,
			request->key.type.source_claim_id,
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
			request->key.type.source_claim_id,
			bridge->left_substitution_id,
			&left_type_claim
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.type.source_claim_id,
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
	left_type = left_type_evidence->subject;
	if (
		prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			left_binding,
			left_type,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 ||
		prototype_context_formation_certificate_db_add(
			context_certificates,
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
	right_type = right_type_evidence->subject;
	if (
		prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			right_type,
			PROTOTYPE_INVALID_ID,
			&endpoint_context
		) != 0 ||
		prototype_context_formation_certificate_db_add(
			context_certificates,
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
		prototype_term_observation_type(
			terms,
			left_type,
			right_type,
			left_endpoint,
			right_endpoint,
			&relation_type
		) != 0 ||
		prototype_judgement_add_observation_type_formation(
			judgement,
			terms,
			endpoint_context,
			relation_type,
			prototype_judgement_claim_get(
				judgement, endpoint_left_type_claim
			)->classifier,
			endpoint_left_type_claim,
			endpoint_type_claim,
			left_endpoint_claim,
			right_endpoint_claim,
			&relation_claim
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_TYPE,
		.data.type = {
			.endpoint_context_id = endpoint_context,
			.left_endpoint_binding_id = left_binding,
			.right_endpoint_binding_id = right_binding,
			.relation_type_term_id = relation_type,
			.relation_is_type_claim_id = relation_claim,
			.left_context_certificate_id = left_context_certificate,
			.right_context_certificate_id = right_context_certificate
		}
	};
	uint32_t certificate_id;
	if (prototype_hott_action_certificate_add(
			actions,
			contexts,
			substitutions,
			context_certificates,
			substitution_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement,
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

static const struct prototype_hott_bridge* hott_bridge_for_source_context(
	const struct prototype_hott_bridge_db* bridges,
	uint32_t source_context,
	uint32_t* p_bridge_id
) {
	if (!bridges) {
		return NULL;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		if (bridges->bridges[i].source_context_id == source_context) {
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
		if (claim->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
			claim->context_id == context_id && claim->subject == subject) {
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
		if (claim->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			claim->context_id == context_id && claim->subject == subject) {
			if (found != PROTOTYPE_INVALID_ID &&
				judgement->claims[found].classifier != claim->classifier) {
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
			if (!reindexed || reindexed->subject != subject) {
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

static const struct prototype_context_formation_certificate*
hott_context_formation_certificate_for_context(
	const struct prototype_context_formation_certificate_db* certificates,
	uint32_t context_id
) {
	if (!certificates) {
		return NULL;
	}
	for (uint32_t i = 0; i < certificates->certificate_count; ++i) {
		if (certificates->certificates[i].context_id == context_id) {
			return &certificates->certificates[i];
		}
	}
	return NULL;
}

static int hott_ensure_bridge_for_context(
	struct prototype_hott_action_db* actions,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_context_formation_certificate_db* context_certificates,
	struct prototype_substitution_certificate_db* substitution_certificates,
	struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement,
	uint32_t source_context_id,
	uint32_t* p_bridge_id,
	int* p_residual_reason
) {
	if (!actions || !contexts || !substitutions || !context_certificates ||
		!substitution_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_bridge_id || !p_residual_reason) {
		return -1;
	}
	if (hott_bridge_for_source_context(
			bridges, source_context_id, p_bridge_id
		)) {
		*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
		return 0;
	}
	const struct prototype_context* source_context =
		prototype_context_get(contexts, source_context_id);
	const struct prototype_context_formation_certificate* source_certificate =
		hott_context_formation_certificate_for_context(
			context_certificates, source_context_id
		);
	if (!source_context || !source_certificate ||
		source_context->parent == PROTOTYPE_INVALID_ID ||
		source_certificate->classifier_claim_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t parent_bridge_id;
	int parent_status = hott_ensure_bridge_for_context(
		actions,
		contexts,
		substitutions,
		context_certificates,
		substitution_certificates,
		bridges,
		terms,
		type_declarations,
		operations,
		judgement,
		source_context->parent,
		&parent_bridge_id,
		p_residual_reason
	);
	if (parent_status != 0) {
		return parent_status;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = source_certificate->classifier_claim_id,
			.source_bridge_id = parent_bridge_id
		}
	};
	uint32_t type_request_id;
	uint32_t type_result_id;
	if (prototype_hott_action_request_intern(
			actions, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, type_request, &type_request_id
		) != 0 || prototype_hott_execute_type_action(
			actions, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, type_request_id, &type_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = type_result->residual_reason;
		return 1;
	}
	if (prototype_hott_bridge_db_construct_extension(
			bridges,
			contexts,
			substitutions,
			context_certificates,
			substitution_certificates,
			actions,
			terms,
			type_declarations,
			judgement,
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
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_context_formation_certificate_db* context_certificates,
	struct prototype_substitution_certificate_db* substitution_certificates,
	struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement,
	const struct prototype_hott_bridge* bridge,
	uint32_t source_context_id,
	uint32_t child_claim_id,
	uint32_t* p_witness_claim_id,
	int* p_residual_reason
) {
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
		child_claim->classifier,
		&child_type_claim_id
	) : -1;
	if (!actions || !contexts || !substitutions || !context_certificates ||
		!substitution_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !bridge || !p_witness_claim_id || !p_residual_reason ||
		!child_claim || child_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		child_claim->context_id != source_context_id || child_type_status < 0) {
		return -1;
	}
	if (child_type_status > 0 || child_type_claim_id == PROTOTYPE_INVALID_ID) {
		*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
		return 1;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = child_type_claim_id,
			.source_bridge_id = bridge->id
		}
	};
	uint32_t type_request_id;
	uint32_t type_result_id;
	if (prototype_hott_action_request_intern(
			actions, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, type_request, &type_request_id
		) != 0 || prototype_hott_execute_type_action(
			actions, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, type_request_id, &type_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = type_result->residual_reason;
		return 1;
	}
	struct prototype_hott_action_request term_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = child_claim_id,
			.source_bridge_id = bridge->id,
			.type_action_request_id = type_request_id
		}
	};
	uint32_t term_request_id;
	uint32_t term_result_id;
	if (prototype_hott_action_request_intern(
			actions, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, term_request, &term_request_id
		) != 0 || prototype_hott_execute_term_action(
			actions, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, term_request_id, &term_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* term_result =
		prototype_hott_action_result_get(actions, term_result_id);
	if (!term_result) {
		return -1;
	}
	if (term_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = term_result->residual_reason;
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

int prototype_hott_execute_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_context_formation_certificate_db* context_certificates,
	struct prototype_substitution_certificate_db* substitution_certificates,
	struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	if (!actions || !contexts || !substitutions || !context_certificates ||
		!substitution_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id) {
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
	if (!source || !bridge || source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->context_id != bridge->source_context_id) {
		return -1;
	}
	if (hott_action_result_for_request(
			actions, request->key.term.type_action_request_id, &type_result_id
		) != 0) {
		return 1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			type_result->residual_reason,
			p_result_id
		);
	}
	const struct prototype_hott_action_certificate* type_certificate = type_result &&
		type_result->certificate_id < actions->certificate_count ?
		&actions->certificates[type_result->certificate_id] : NULL;
	if (!type_certificate ||
		type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_TYPE) {
		return 1;
	}
	const struct prototype_hott_type_action_certificate* type =
		&type_certificate->data.type;
	const struct prototype_hott_action_request* type_request =
		prototype_hott_action_request_get(
			actions, request->key.term.type_action_request_id
		);
	uint32_t left_endpoint_claim;
	uint32_t right_endpoint_claim;
	uint32_t left_type_claim;
	uint32_t right_type_claim;
	if (!type_request || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.term.source_claim_id,
			bridge->left_substitution_id,
			&left_endpoint_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.term.source_claim_id,
			bridge->right_substitution_id,
			&right_endpoint_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			type_request->key.type.source_claim_id,
			bridge->left_substitution_id,
			&left_type_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			type_request->key.type.source_claim_id,
			bridge->right_substitution_id,
			&right_type_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* left_endpoint_evidence =
		prototype_judgement_claim_get(judgement, left_endpoint_claim);
	const struct prototype_judgement_claim* right_endpoint_evidence =
		prototype_judgement_claim_get(judgement, right_endpoint_claim);
	if (!left_endpoint_evidence || !right_endpoint_evidence) {
		return -1;
	}
	uint32_t base_substitution;
	uint32_t left_extension;
	uint32_t endpoint_instantiation;
	if (prototype_substitution_identity(
			substitutions,
			contexts,
			bridge->bridge_context_id,
			&base_substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			base_substitution,
			prototype_context_get(contexts, type->endpoint_context_id)->parent,
			left_endpoint_evidence->subject,
			left_endpoint_evidence->classifier,
			&left_extension
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			left_extension,
			type->endpoint_context_id,
			right_endpoint_evidence->subject,
			right_endpoint_evidence->classifier,
			&endpoint_instantiation
		) != 0) {
		return -1;
	}
	uint32_t left_extension_certificate;
	uint32_t right_extension_certificate;
	if (prototype_substitution_certificate_db_add(
			substitution_certificates,
			substitutions,
			judgement,
			left_extension,
			left_endpoint_claim,
			&left_extension_certificate
		) != 0 || prototype_substitution_certificate_db_add(
			substitution_certificates,
			substitutions,
			judgement,
			endpoint_instantiation,
			right_endpoint_claim,
			&right_extension_certificate
		) != 0) {
		return -1;
	}
	uint32_t relation_type;
	uint32_t expected_relation;
	if (prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			type->relation_type_term_id,
			endpoint_instantiation,
			&relation_type
		) != 0 || prototype_term_observation_type(
			terms,
			left_endpoint_evidence->classifier,
			right_endpoint_evidence->classifier,
			left_endpoint_evidence->subject,
			right_endpoint_evidence->subject,
			&expected_relation
		) != 0 || relation_type != expected_relation) {
		return -1;
	}
	uint32_t relation_claim;
	uint32_t witness;
	uint32_t witness_claim;
	if (prototype_judgement_add_observation_type_formation(
			judgement,
			terms,
			bridge->bridge_context_id,
			relation_type,
			prototype_judgement_claim_get(judgement, left_type_claim)->classifier,
			left_type_claim,
			right_type_claim,
			left_endpoint_claim,
			right_endpoint_claim,
			&relation_claim
		) != 0) {
		return -1;
	}
	int used_relation_binding = 0;
	if (source->subject < terms->term_count &&
		terms->terms[source->subject].tag == PROTOTYPE_TERM_VAR) {
		uint32_t source_entry_context;
		uint32_t source_binding = terms->terms[source->subject].as.var.binding_id;
		if (prototype_context_find_binding(
				contexts,
				source->context_id,
				source_binding,
				&source_entry_context
			) == 0) {
			const struct prototype_hott_bridge* binding_bridge =
				hott_bridge_for_source_context(
					bridges, source_entry_context, NULL
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
				left_endpoint_evidence->subject,
				right_endpoint_evidence->subject,
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
			if (induction_derivation && source->subject < terms->term_count &&
				terms->terms[source->subject].tag ==
					PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
				uint32_t argument = terms->terms[source->subject].
					as.induction_hypothesis.argument;
				uint32_t argument_claim_id = hott_has_type_claim_for_subject(
					judgement, source->context_id, argument
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
					contexts,
					substitutions,
					context_certificates,
					substitution_certificates,
					bridges,
					terms,
					type_declarations,
					operations,
					judgement,
					bridge,
					source->context_id,
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
				if (prototype_term_observation_witness(
						terms,
						left_endpoint_evidence->subject,
						right_endpoint_evidence->subject,
						&witness
					) != 0 ||
					prototype_judgement_add_observation_induction_hypothesis_witness(
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
				source->subject < terms->term_count &&
				terms->terms[source->subject].tag == PROTOTYPE_TERM_LAMBDA) {
				if (lambda_derivation->premise_count != 2) {
					return -1;
				}
				const struct prototype_judgement_claim* binder_claim =
					prototype_judgement_claim_get(
						judgement, lambda_derivation->premise_claim_ids[0]
					);
				const struct prototype_judgement_claim* body_claim =
					prototype_judgement_claim_get(
						judgement, lambda_derivation->premise_claim_ids[1]
					);
				const struct prototype_context* body_context = body_claim ?
					prototype_context_get(contexts, body_claim->context_id) : NULL;
				const struct prototype_context_formation_certificate* body_context_proof =
					body_context ? hott_context_formation_certificate_for_context(
						context_certificates, body_claim->context_id
					) : NULL;
				if (!binder_claim || !body_claim || !body_context ||
					!body_context_proof || binder_claim->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					body_claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					binder_claim->context_id != body_claim->context_id ||
					body_context->parent != source->context_id ||
					body_context->binding_id !=
						terms->terms[source->subject].as.lambda.binding_id ||
					body_context_proof->classifier_claim_id == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				struct prototype_hott_action_request binder_type_request = {
					.kind = PROTOTYPE_HOTT_ACTION_TYPE,
					.key.type = {
						.source_claim_id = body_context_proof->classifier_claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t binder_type_request_id;
				uint32_t binder_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
						binder_type_request, &binder_type_request_id
					) != 0 || prototype_hott_execute_type_action(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
						binder_type_request_id, &binder_type_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* binder_type_result =
					prototype_hott_action_result_get(actions, binder_type_result_id);
				if (!binder_type_result) {
					return -1;
				}
				if (binder_type_result->state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						binder_type_result->residual_reason, p_result_id
					);
				}
				uint32_t body_bridge_id;
				if (prototype_hott_bridge_db_construct_extension(
						bridges,
						contexts,
						substitutions,
						context_certificates,
						substitution_certificates,
						actions,
						terms,
						type_declarations,
						judgement,
						body_claim->context_id,
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
					contexts,
					substitutions,
					context_certificates,
					substitution_certificates,
					bridges,
					terms,
					type_declarations,
					operations,
					judgement,
					body_bridge,
					body_claim->context_id,
					lambda_derivation->premise_claim_ids[1],
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
				if (prototype_term_observation_witness(
						terms,
						left_endpoint_evidence->subject,
						right_endpoint_evidence->subject,
						&witness
					) != 0 || prototype_judgement_add_observation_lambda_witness(
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
				source->subject < terms->term_count &&
				terms->terms[source->subject].tag == PROTOTYPE_TERM_MATCH) {
				const struct prototype_term* source_match =
					&terms->terms[source->subject];
				if (source_match->as.match.case_count + 1 !=
						match_derivation->premise_count ||
					source_match->as.match.case_count + 4 >
						PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
					return -1;
				}
				uint32_t scrutinee_claim_id = hott_has_type_claim_for_subject(
					judgement, source->context_id, source_match->as.match.scrutinee
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
					contexts,
					substitutions,
					context_certificates,
					substitution_certificates,
					bridges,
					terms,
					type_declarations,
					operations,
					judgement,
					bridge,
					source->context_id,
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
							judgement, match_derivation->premise_claim_ids[i + 1]
						);
					if (case_id >= terms->case_count || !case_claim ||
						case_claim->subject != terms->cases[case_id].body ||
						!hott_match_case_context_valid(
							contexts,
							source->context_id,
							case_claim->context_id,
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
						contexts,
						substitutions,
						context_certificates,
						substitution_certificates,
						bridges,
						terms,
						type_declarations,
						operations,
						judgement,
						case_claim->context_id,
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
						contexts,
						substitutions,
						context_certificates,
						substitution_certificates,
						bridges,
						terms,
						type_declarations,
						operations,
						judgement,
						case_bridge,
						case_claim->context_id,
						match_derivation->premise_claim_ids[i + 1],
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
				if (prototype_term_observation_witness(
						terms,
						left_endpoint_evidence->subject,
						right_endpoint_evidence->subject,
						&witness
					) != 0 || prototype_judgement_add_observation_match_witness(
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
				source->subject < terms->term_count &&
				terms->terms[source->subject].tag == PROTOTYPE_TERM_APP) {
				if (app_derivation->premise_count != 2) {
					return -1;
				}
				uint32_t child_witness_claims[2];
				for (uint32_t i = 0; i < 2; ++i) {
					const struct prototype_judgement_claim* child_claim =
						prototype_judgement_claim_get(
							judgement, app_derivation->premise_claim_ids[i]
						);
					uint32_t expected_subject = i == 0 ?
						terms->terms[source->subject].as.app.function :
						terms->terms[source->subject].as.app.argument;
					int matches = 0;
					if (!child_claim || child_claim->context_id != source->context_id ||
						prototype_term_core_shape_equal(
							terms, child_claim->subject, expected_subject, &matches
						) != 0 || !matches) {
						return -1;
					}
					int residual_reason;
					int child_status = hott_execute_child_term_action(
						actions,
						contexts,
						substitutions,
						context_certificates,
						substitution_certificates,
						bridges,
						terms,
						type_declarations,
						operations,
						judgement,
						bridge,
						source->context_id,
						app_derivation->premise_claim_ids[i],
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
				if (prototype_term_observation_witness(
						terms,
						left_endpoint_evidence->subject,
						right_endpoint_evidence->subject,
						&witness
					) != 0 || prototype_judgement_add_observation_app_witness(
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
			if (source->subject < terms->term_count &&
				terms->terms[source->subject].tag == PROTOTYPE_TERM_RETURN) {
				unary_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO;
				unary_witness_proof_kind =
					PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_RETURN_WITNESS;
				unary_payload = terms->terms[source->subject].as.return_term.value;
			} else if (source->subject < terms->term_count &&
				terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK) {
				unary_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO;
				unary_witness_proof_kind =
					PROTOTYPE_JUDGEMENT_PROOF_OBSERVATION_THUNK_WITNESS;
				unary_payload = terms->terms[source->subject].as.thunk.computation;
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
				uint32_t child_claim_id = unary_derivation->premise_claim_ids[0];
				const struct prototype_judgement_claim* child_claim =
					prototype_judgement_claim_get(judgement, child_claim_id);
				uint32_t child_type_claim_id = child_claim ?
					hott_is_type_claim_for_subject(
						judgement, source->context_id, child_claim->classifier
					) : PROTOTYPE_INVALID_ID;
				if (!child_claim || child_claim->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					child_claim->context_id != source->context_id ||
					child_claim->subject != unary_payload ||
					child_type_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				struct prototype_hott_action_request child_type_request = {
					.kind = PROTOTYPE_HOTT_ACTION_TYPE,
					.key.type = {
						.source_claim_id = child_type_claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t child_type_request_id;
				uint32_t child_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
						child_type_request, &child_type_request_id
					) != 0 || prototype_hott_execute_type_action(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
						child_type_request_id, &child_type_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* child_type_result =
					prototype_hott_action_result_get(actions, child_type_result_id);
				if (!child_type_result) {
					return -1;
				}
				if (child_type_result->state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						child_type_result->residual_reason, p_result_id
					);
				}
				struct prototype_hott_action_request child_term_request = {
					.kind = PROTOTYPE_HOTT_ACTION_TERM,
					.key.term = {
						.source_claim_id = child_claim_id,
						.source_bridge_id = bridge->id,
						.type_action_request_id = child_type_request_id
					}
				};
				uint32_t child_term_request_id;
				uint32_t child_term_result_id;
				if (prototype_hott_action_request_intern(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
						child_term_request, &child_term_request_id
					) != 0 || prototype_hott_execute_term_action(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
						child_term_request_id, &child_term_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* child_term_result =
					prototype_hott_action_result_get(actions, child_term_result_id);
				if (!child_term_result) {
					return -1;
				}
				if (child_term_result->state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						child_term_result->residual_reason, p_result_id
					);
				}
				const struct prototype_hott_action_certificate* child_certificate =
					&actions->certificates[child_term_result->certificate_id];
				if (prototype_term_observation_witness(
						terms,
						left_endpoint_evidence->subject,
						right_endpoint_evidence->subject,
						&witness
					) != 0 || prototype_judgement_add_observation_unary_witness(
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
					source->subject,
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
					constructor_derivation->premise_claim_ids[i];
				const struct prototype_judgement_claim* field_claim =
					prototype_judgement_claim_get(judgement, field_claim_id);
				uint32_t field_type_claim_id = field_claim ?
					hott_is_type_claim_for_subject(
						judgement, source->context_id, field_claim->classifier
					) : PROTOTYPE_INVALID_ID;
				if (!field_claim || field_claim->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					field_claim->context_id != source->context_id ||
					field_claim->subject != source_arguments[i] ||
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
					.kind = PROTOTYPE_HOTT_ACTION_TYPE,
					.key.type = {
						.source_claim_id = field_type_claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t field_type_request_id;
				uint32_t field_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
						field_type_request, &field_type_request_id
					) != 0 || prototype_hott_execute_type_action(
						actions, contexts, substitutions,
						context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
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
				if (field_type_result->state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						field_type_result->residual_reason, p_result_id
					);
				}
				struct prototype_hott_action_request field_term_request = {
					.kind = PROTOTYPE_HOTT_ACTION_TERM,
					.key.term = {
						.source_claim_id = field_claim_id,
						.source_bridge_id = bridge->id,
						.type_action_request_id = field_type_request_id
					}
				};
				uint32_t field_term_request_id;
				uint32_t field_term_result_id;
				if (prototype_hott_action_request_intern(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
						field_term_request, &field_term_request_id
					) != 0 || prototype_hott_execute_term_action(
						actions, contexts, substitutions, context_certificates,
						substitution_certificates, bridges, terms,
						type_declarations, operations, judgement,
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
				if (field_term_result->state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						field_term_result->residual_reason, p_result_id
					);
				}
				const struct prototype_hott_action_certificate* field_certificate =
					&actions->certificates[field_term_result->certificate_id];
				field_witness_claims[i] =
					field_certificate->data.term.witness_has_type_claim_id;
			}
			if (!used_relation_binding && (prototype_term_observation_witness(
					terms,
					left_endpoint_evidence->subject,
					right_endpoint_evidence->subject,
					&witness
				) != 0 || prototype_judgement_add_observation_constructor_witness(
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
			(prototype_term_observation_witness(
				terms,
				left_endpoint_evidence->subject,
				right_endpoint_evidence->subject,
				&witness
			) != 0 || prototype_judgement_add_observation_witness_intro(
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
			actions, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, certificate, &certificate_id
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
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_context_formation_certificate_db* context_certificates,
	struct prototype_substitution_certificate_db* substitution_certificates,
	struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement,
	uint32_t request_id,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_result_id
) {
	if (!actions || !contexts || !substitutions || !context_certificates ||
		!substitution_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id || normalization_profile !=
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF) {
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
				!hott_bridge_for_source_context(
					bridges, outer->source_context, &middle_bridge_id
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
					actions, contexts, substitutions, context_certificates,
					substitution_certificates, bridges, terms, type_declarations,
					operations, judgement, inner_request, &inner_request_id
				) != 0 || prototype_hott_action_request_intern(
					actions, contexts, substitutions, context_certificates,
					substitution_certificates, bridges, terms, type_declarations,
					operations, judgement, outer_request, &outer_request_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, contexts, substitutions, context_certificates,
					substitution_certificates, bridges, terms, type_declarations,
					operations, judgement, inner_request_id, normalization_profile,
					step_limit, &inner_result_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, contexts, substitutions, context_certificates,
					substitution_certificates, bridges, terms, type_declarations,
					operations, judgement, outer_request_id, normalization_profile,
					step_limit, &outer_result_id
				) != 0) {
				return -1;
			}
			const struct prototype_hott_action_result* inner_result =
				prototype_hott_action_result_get(actions, inner_result_id);
			const struct prototype_hott_action_result* outer_result =
				prototype_hott_action_result_get(actions, outer_result_id);
			if (!inner_result || !outer_result ||
				inner_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
				outer_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
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
				hott_bridge_for_source_context(
					bridges, target->parent, &parent_bridge_id
				) : NULL;
			uint32_t term_claim_id = PROTOTYPE_INVALID_ID;
			for (uint32_t i = 0; i < judgement->claim_count; ++i) {
				const struct prototype_judgement_claim* claim = &judgement->claims[i];
				if (claim->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
					claim->context_id == source->source_context &&
					claim->subject == source->term &&
					claim->classifier == source->term_classifier &&
					hott_operation_matches_claim(operations, claim)) {
					term_claim_id = i;
					break;
				}
			}
			uint32_t type_request_id = PROTOTYPE_INVALID_ID;
			for (uint32_t i = 0; i < actions->request_count; ++i) {
				const struct prototype_hott_action_request* candidate =
					&actions->requests[i];
				const struct prototype_judgement_claim* type_claim =
					candidate->kind == PROTOTYPE_HOTT_ACTION_TYPE ?
					prototype_judgement_claim_get(
						judgement, candidate->key.type.source_claim_id
					) : NULL;
				uint32_t candidate_result;
				if (type_claim && parent_bridge &&
					candidate->key.type.source_bridge_id == parent_bridge_id &&
					type_claim->context_id == target->parent &&
					type_claim->subject ==
						prototype_context_classifier_term(target) &&
					hott_action_result_for_request(
						actions, i, &candidate_result
					) == 0 && actions->results[candidate_result].state ==
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
					.type_action_request_id = type_request_id
				}
			};
			uint32_t prefix_request_id;
			uint32_t term_request_id;
			uint32_t prefix_result_id;
			uint32_t term_result_id;
			if (prototype_hott_action_request_intern(
					actions, contexts, substitutions, context_certificates,
					substitution_certificates, bridges, terms, type_declarations,
					operations, judgement, prefix_request, &prefix_request_id
				) != 0 || prototype_hott_action_request_intern(
					actions, contexts, substitutions, context_certificates,
					substitution_certificates, bridges, terms, type_declarations,
					operations, judgement, term_request, &term_request_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, contexts, substitutions, context_certificates,
					substitution_certificates, bridges, terms, type_declarations,
					operations, judgement, prefix_request_id, normalization_profile,
					step_limit, &prefix_result_id
				) != 0 || prototype_hott_execute_term_action(
					actions, contexts, substitutions, context_certificates,
					substitution_certificates,
					bridges, terms, type_declarations, operations,
					judgement,
					term_request_id, &term_result_id
				) != 0) {
				return -1;
			}
			const struct prototype_hott_action_result* prefix_result =
				prototype_hott_action_result_get(actions, prefix_result_id);
			const struct prototype_hott_action_result* term_result =
				prototype_hott_action_result_get(actions, term_result_id);
			if (!prefix_result || !term_result ||
				prefix_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
				term_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
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
					witness_claim->classifier,
					&result_substitution
				) != 0 || prototype_substitution_certificate_db_add(
					substitution_certificates,
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
			.state = PROTOTYPE_HOTT_ACTION_RESULT_RESIDUAL,
			.residual_reason = PROTOTYPE_HOTT_RESIDUAL_CONVERSION_EXHAUSTED,
			.certificate_id = PROTOTYPE_INVALID_ID,
			.normalization_profile = normalization_profile,
			.step_limit = step_limit,
			.term_graph_revision = terms->term_count
		};
		memcpy(
			residual.calculus_fingerprint,
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
			actions, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement, certificate, &certificate_id
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

int prototype_hott_bridge_db_construct_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_context_formation_certificate_db* context_certificates,
	struct prototype_substitution_certificate_db* substitution_certificates,
	const struct prototype_hott_action_db* actions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	uint32_t source_context_id,
	uint32_t type_action_request_id,
	uint32_t* p_bridge_id
) {
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, type_action_request_id);
	uint32_t result_id;
	if (!bridges || !contexts || !substitutions || !context_certificates ||
		!substitution_certificates || !actions || !terms || !type_declarations ||
		!judgement || !source || !request || !p_bridge_id ||
		source_context_id == prototype_context_empty(contexts) ||
		request->kind != PROTOTYPE_HOTT_ACTION_TYPE ||
		hott_action_result_for_request(
			actions, type_action_request_id, &result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, result_id);
	const struct prototype_hott_action_certificate* type_certificate = result &&
		result->state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_claim* source_type =
		prototype_judgement_claim_get(
			judgement, request->key.type.source_claim_id
		);
	const struct prototype_hott_bridge* parent = type_certificate ?
		prototype_hott_bridge_db_get(
			bridges, request->key.type.source_bridge_id
		) : NULL;
	if (!type_certificate ||
		type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_TYPE ||
		!source_type || !parent || source->parent != parent->source_context_id ||
		source_type->context_id != source->parent ||
		source_type->subject != prototype_context_classifier_term(source)) {
		return -1;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		if (bridges->bridges[i].source_context_id == source_context_id) {
			*p_bridge_id = i;
			return 0;
		}
	}
	const struct prototype_hott_type_action_certificate* type =
		&type_certificate->data.type;
	const struct prototype_context* right_endpoint_context =
		prototype_context_get(contexts, type->endpoint_context_id);
	const struct prototype_context* left_endpoint_context = right_endpoint_context ?
		prototype_context_get(contexts, right_endpoint_context->parent) : NULL;
	uint32_t left_endpoint_classifier =
		prototype_context_classifier_term(left_endpoint_context);
	uint32_t right_endpoint_classifier =
		prototype_context_classifier_term(right_endpoint_context);
	if (!left_endpoint_context || !right_endpoint_context ||
		left_endpoint_classifier == PROTOTYPE_INVALID_ID ||
		right_endpoint_classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t relation_binding = prototype_term_new_binding(terms);
	uint32_t bridge_context;
	uint32_t relation_context_certificate;
	if (relation_binding == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			contexts,
			type->endpoint_context_id,
			relation_binding,
			type->relation_type_term_id,
			PROTOTYPE_INVALID_ID,
			&bridge_context
		) != 0 ||
		prototype_context_formation_certificate_db_add(
			context_certificates,
			contexts,
			terms,
			type_declarations,
			judgement,
			bridge_context,
			type->relation_is_type_claim_id,
			&relation_context_certificate
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
		) != 0 ||
		prototype_substitution_compose(
			substitutions,
			contexts,
			parent->left_substitution_id,
			bridge_to_parent,
			&left_prefix
		) != 0 ||
		prototype_substitution_compose(
			substitutions,
			contexts,
			parent->right_substitution_id,
			bridge_to_parent,
			&right_prefix
		) != 0 ||
		prototype_term_var(
			terms, type->left_endpoint_binding_id, &left_endpoint
		) != 0 ||
		prototype_term_var(
			terms, type->right_endpoint_binding_id, &right_endpoint
		) != 0 ||
		prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			left_prefix,
			source_context_id,
			left_endpoint,
			left_endpoint_classifier,
			&left_substitution
		) != 0 ||
		prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			right_prefix,
			source_context_id,
			right_endpoint,
			right_endpoint_classifier,
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
			type->left_endpoint_binding_id,
			left_endpoint_classifier,
			&left_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			bridge_context,
			type->right_endpoint_binding_id,
			right_endpoint_classifier,
			&right_claim
		) != 0 ||
		prototype_substitution_certificate_db_add(
			substitution_certificates,
			substitutions,
			judgement,
			left_substitution,
			left_claim,
			&left_substitution_certificate
		) != 0 ||
		prototype_substitution_certificate_db_add(
			substitution_certificates,
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
			context_certificates,
			substitution_certificates
		)) {
		return -1;
	}
	bridges->bridges[bridges->bridge_count++] = bridge;
	bridges->certificates[bridges->certificate_count++] =
		(struct prototype_hott_bridge_certificate) {
			.id = bridge.id,
			.bridge_id = bridge.id,
			.parent_bridge_id = parent->id,
			.type_action_certificate_id = result->certificate_id,
			.left_endpoint_context_certificate_id =
				type->left_context_certificate_id,
			.right_endpoint_context_certificate_id =
				type->right_context_certificate_id,
			.relation_context_certificate_id = relation_context_certificate,
			.left_substitution_certificate_id = left_substitution_certificate,
			.right_substitution_certificate_id = right_substitution_certificate
		};
	*p_bridge_id = bridge.id;
	return 0;
}

int prototype_hott_observation_plan_and_execute(
	struct prototype_hott_observation_goal_db* goals,
	struct prototype_hott_candidate_db* candidates,
	struct prototype_hott_work_db* work,
	struct prototype_hott_action_db* actions,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_context_formation_certificate_db* context_certificates,
	struct prototype_substitution_certificate_db* substitution_certificates,
	struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_operation_graph* operations,
	struct prototype_judgement_db* judgement,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	struct prototype_hott_observation_execution* p_execution
) {
	if (!goals || !candidates || !work || !actions || !contexts ||
		!substitutions || !context_certificates || !substitution_certificates ||
		!bridges || !terms || !type_declarations || !operations || !judgement ||
		!p_execution) {
		return -1;
	}
	struct prototype_hott_observation_execution execution = {
		.work_item_id = PROTOTYPE_INVALID_ID,
		.type_action_request_id = PROTOTYPE_INVALID_ID,
		.type_action_result_id = PROTOTYPE_INVALID_ID,
		.term_action_request_id = PROTOTYPE_INVALID_ID,
		.term_action_result_id = PROTOTYPE_INVALID_ID
	};
	if (prototype_hott_observation_plan(
			goals,
			candidates,
			work,
			contexts,
			substitutions,
			context_certificates,
			substitution_certificates,
			bridges,
			terms,
			type_declarations,
			definitions,
			operations,
			judgement,
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
	const struct prototype_hott_observation_goal* goal =
		prototype_hott_observation_goal_db_get(goals, goal_id);
	if (!item || !goal) {
		return -1;
	}
	if (item->state != PROTOTYPE_HOTT_WORK_READY) {
		*p_execution = execution;
		return 0;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TYPE,
		.key.type = {
			.source_claim_id = goal->carrier_claim_id,
			.source_bridge_id = goal->bridge_id
		}
	};
	if (prototype_hott_action_request_intern(
			actions,
			contexts,
			substitutions,
			context_certificates,
			substitution_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement,
			type_request,
			&execution.type_action_request_id
		) != 0 || prototype_hott_execute_type_action(
			actions,
			contexts,
			substitutions,
			context_certificates,
			substitution_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement,
			execution.type_action_request_id,
			&execution.type_action_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(
			actions, execution.type_action_result_id
		);
	const struct prototype_hott_candidate* selected =
		prototype_hott_candidate_db_get(
			candidates, item->selected_candidate_id
		);
	if (!type_result || !selected) {
		return -1;
	}
	if (type_result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		selected->rule != PROTOTYPE_HOTT_RULE_OBS_DIAGONAL) {
		*p_execution = execution;
		return 0;
	}
	struct prototype_hott_action_request term_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = goal->left_claim_id,
			.source_bridge_id = goal->bridge_id,
			.type_action_request_id = execution.type_action_request_id
		}
	};
	if (prototype_hott_action_request_intern(
			actions,
			contexts,
			substitutions,
			context_certificates,
			substitution_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement,
			term_request,
			&execution.term_action_request_id
		) != 0 || prototype_hott_execute_term_action(
			actions,
			contexts,
			substitutions,
			context_certificates,
			substitution_certificates,
			bridges,
			terms,
			type_declarations,
			operations,
			judgement,
			execution.term_action_request_id,
			&execution.term_action_result_id
		) != 0) {
		return -1;
	}
	*p_execution = execution;
	return 0;
}

int prototype_hott_action_db_validate(
	const struct prototype_hott_action_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	const struct prototype_context_formation_certificate_db* context_certificates,
	const struct prototype_substitution_certificate_db* substitution_certificates,
	const struct prototype_hott_bridge_db* bridges,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || db->request_count > db->request_capacity ||
		db->certificate_count > db->certificate_capacity ||
		db->result_count > db->result_capacity ||
		(db->request_count != 0 && !db->requests) ||
		(db->certificate_count != 0 && !db->certificates) ||
		(db->result_count != 0 && !db->results) ||
		prototype_context_formation_certificate_db_validate(
			context_certificates, contexts, terms, type_declarations, judgement
		) != 0 ||
		prototype_substitution_certificate_db_validate(
			substitution_certificates, substitutions, judgement
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* bridge_certificate =
			&bridges->certificates[i];
		if (bridge_certificate->parent_bridge_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (bridge_certificate->type_action_certificate_id >=
			db->certificate_count) {
			return -1;
		}
		const struct prototype_hott_action_certificate* type_certificate =
			&db->certificates[bridge_certificate->type_action_certificate_id];
		const struct prototype_hott_action_request* type_request =
			prototype_hott_action_request_get(db, type_certificate->request_id);
		const struct prototype_judgement_claim* source_type = type_request ?
			prototype_judgement_claim_get(
				judgement, type_request->key.type.source_claim_id
			) : NULL;
		const struct prototype_context* source = prototype_context_get(
			contexts, bridges->bridges[i].source_context_id
		);
		const struct prototype_context* relation = prototype_context_get(
			contexts, bridges->bridges[i].bridge_context_id
		);
		if (type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_TYPE ||
			!type_request || type_request->kind != PROTOTYPE_HOTT_ACTION_TYPE ||
			type_request->key.type.source_bridge_id !=
				bridge_certificate->parent_bridge_id ||
			!source_type || !source || !relation ||
			source_type->context_id != source->parent ||
			source_type->subject != prototype_context_classifier_term(source) ||
			type_certificate->data.type.endpoint_context_id != relation->parent ||
			type_certificate->data.type.relation_type_term_id !=
				prototype_context_classifier_term(relation) ||
			bridge_certificate->relation_context_certificate_id >=
				context_certificates->certificate_count ||
			context_certificates->certificates[
				bridge_certificate->relation_context_certificate_id
			].classifier_claim_id !=
				type_certificate->data.type.relation_is_type_claim_id) {
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
				context_certificates,
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
				context_certificates,
				substitution_certificates,
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
			strcmp(
				result->calculus_fingerprint,
				PROTOTYPE_HOTT_CALCULUS_FINGERPRINT
			) != 0 ||
			result->state < PROTOTYPE_HOTT_ACTION_RESULT_READY ||
			result->state > PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED ||
			(result->state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
			 (result->residual_reason != PROTOTYPE_HOTT_RESIDUAL_NONE ||
			  result->certificate_id >= db->certificate_count ||
			  db->certificates[result->certificate_id].request_id !=
				result->request_id)) ||
			(result->state != PROTOTYPE_HOTT_ACTION_RESULT_READY &&
			 (result->certificate_id != PROTOTYPE_INVALID_ID ||
			  result->residual_reason == PROTOTYPE_HOTT_RESIDUAL_NONE ||
			  !hott_residual_reason_is_valid(result->residual_reason)))) {
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
