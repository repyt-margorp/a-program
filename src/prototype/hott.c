#include "ast.h"
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

static int hott_action_state_is_valid(int state) {
	return state == PROTOTYPE_HOTT_ACTION_DEFERRED ||
		state == PROTOTYPE_HOTT_ACTION_READY;
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
	return context && context->classifier_variable == PROTOTYPE_INVALID_ID &&
		hott_is_type_claim_matches(
			terms,
			type_declarations,
			judgement,
			certificate->classifier_claim_id,
			context->parent,
			context->classifier
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
	size_t bridge_capacity
) {
	if (!db) {
		return;
	}
	db->bridges = bridges;
	db->bridge_count = 0;
	db->bridge_capacity = bridge_capacity;
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
	/* P1 retains only the terminal bridge. O1 owns non-empty relation fields. */
	return bridge->source_context_id == prototype_context_empty(contexts) &&
		bridge->bridge_context_id == bridge->source_context_id &&
		left->kind == PROTOTYPE_SUBSTITUTION_IDENTITY &&
		right->kind == PROTOTYPE_SUBSTITUTION_IDENTITY &&
		bridge->left_substitution_id == bridge->right_substitution_id &&
		bridge->left_certificate_id == PROTOTYPE_INVALID_ID &&
		bridge->right_certificate_id == PROTOTYPE_INVALID_ID;
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
		.right_substitution_id = identity,
		.left_certificate_id = PROTOTYPE_INVALID_ID,
		.right_certificate_id = PROTOTYPE_INVALID_ID
	};
	if (!hott_bridge_record_is_valid(
			&bridge, bridge.id, contexts, substitutions, context_certificates,
			certificates
		)) {
		return -1;
	}
	db->bridges[db->bridge_count++] = bridge;
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
		(db->bridge_count != 0 && !db->bridges) ||
		prototype_context_formation_certificate_db_validate(
			context_certificates, contexts, terms, type_declarations, judgement
		) != 0 || prototype_substitution_certificate_db_validate(
			certificates, substitutions, judgement
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < db->bridge_count; ++i) {
		if (!hott_bridge_record_is_valid(
				&db->bridges[i], i, contexts, substitutions,
				context_certificates, certificates
			)) {
			return -1;
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
	const struct prototype_substitution_certificate_db* substitution_certificates,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
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
	uint32_t first_candidate = (uint32_t)candidates->candidate_count;
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
	} else if (carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_TEXT ||
		carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_INT ||
		carrier_term->tag == PROTOTYPE_TERM_PRIMITIVE_INT64) {
		if (candidates->candidate_count == first_candidate) {
			item.state = PROTOTYPE_HOTT_WORK_UNSUPPORTED;
			item.residual_reason = PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE;
			goto commit;
		}
	}
	if (candidates->candidate_count == first_candidate) {
		item.state = PROTOTYPE_HOTT_WORK_RESIDUAL;
		item.residual_reason = PROTOTYPE_HOTT_RESIDUAL_CONVERSION;
	} else {
		item.state = PROTOTYPE_HOTT_WORK_READY;
		item.selected_candidate_id = first_candidate;
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

void prototype_hott_action_db_init(
	struct prototype_hott_action_db* db,
	struct prototype_hott_action* actions,
	size_t action_capacity
) {
	if (!db) {
		return;
	}
	db->actions = actions;
	db->action_count = 0;
	db->action_capacity = action_capacity;
}

static int hott_action_is_valid(
	const struct prototype_hott_action_db* db,
	const struct prototype_hott_action* action,
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
	if (!db || !action || action->id != expected_id ||
		!hott_action_kind_is_valid(action->kind) ||
		!hott_action_state_is_valid(action->state) ||
		prototype_context_formation_certificate_db_validate(
			context_certificates, contexts, terms, type_declarations, judgement
		) != 0 || prototype_substitution_certificate_db_validate(
			substitution_certificates, substitutions, judgement
		) != 0) {
		return 0;
	}
	switch (action->kind) {
	case PROTOTYPE_HOTT_ACTION_CONTEXT:
		if (action->source_context_id >= contexts->context_count ||
			!hott_context_is_formed(
				contexts, context_certificates, action->source_context_id
			)) {
			return 0;
		}
		if (action->state == PROTOTYPE_HOTT_ACTION_DEFERRED) {
			return action->source_context_id != prototype_context_empty(contexts) &&
				action->result_bridge_id == PROTOTYPE_INVALID_ID;
		}
		{
			const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
				bridges, action->result_bridge_id
			);
			return bridge &&
				bridge->source_context_id == action->source_context_id;
		}
	case PROTOTYPE_HOTT_ACTION_SUBSTITUTION:
		{
			const struct prototype_substitution* substitution =
				prototype_substitution_get(
					substitutions, action->source_substitution_id
				);
			const struct prototype_hott_bridge* source_bridge =
				prototype_hott_bridge_db_get(bridges, action->source_bridge_id);
			const struct prototype_hott_bridge* target_bridge =
				prototype_hott_bridge_db_get(bridges, action->target_bridge_id);
			return substitution && source_bridge && target_bridge &&
				source_bridge->source_context_id == substitution->source_context &&
				target_bridge->source_context_id == substitution->target_context &&
				action->state == PROTOTYPE_HOTT_ACTION_DEFERRED &&
				action->result_substitution_id == PROTOTYPE_INVALID_ID &&
				action->result_certificate_id == PROTOTYPE_INVALID_ID;
		}
	case PROTOTYPE_HOTT_ACTION_TYPE:
		{
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(judgement, action->source_claim_id);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(bridges, action->source_bridge_id);
			return claim && bridge && bridge->source_context_id == claim->context_id &&
				hott_is_type_claim_matches(
					terms, type_declarations, judgement, action->source_claim_id,
					claim->context_id, claim->subject
				) && action->state == PROTOTYPE_HOTT_ACTION_DEFERRED &&
				action->result_term_id == PROTOTYPE_INVALID_ID &&
				action->result_claim_id == PROTOTYPE_INVALID_ID;
		}
	case PROTOTYPE_HOTT_ACTION_TERM:
		{
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(judgement, action->source_claim_id);
			const struct prototype_hott_bridge* bridge =
				prototype_hott_bridge_db_get(bridges, action->source_bridge_id);
			const struct prototype_hott_action* type_action =
				action->type_action_id < expected_id ?
				&db->actions[action->type_action_id] : NULL;
			const struct prototype_judgement_claim* type_claim = type_action ?
				prototype_judgement_claim_get(
					judgement, type_action->source_claim_id
				) : NULL;
			return claim && claim->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				hott_operation_matches_claim(operations, claim) && bridge &&
				bridge->source_context_id == claim->context_id && type_action &&
				type_action->kind == PROTOTYPE_HOTT_ACTION_TYPE &&
				type_action->source_bridge_id == action->source_bridge_id &&
				type_claim && type_claim->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
				type_claim->context_id == claim->context_id &&
				type_claim->subject == claim->classifier &&
				action->state == PROTOTYPE_HOTT_ACTION_DEFERRED &&
				action->result_term_id == PROTOTYPE_INVALID_ID &&
				action->result_claim_id == PROTOTYPE_INVALID_ID;
		}
	default:
		return 0;
	}
	return 0;
}

int prototype_hott_action_db_add(
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
	struct prototype_hott_action action,
	uint32_t* p_action_id
) {
	if (!db || !p_action_id || !db->actions ||
		db->action_count >= db->action_capacity) {
		return -1;
	}
	action.id = (uint32_t)db->action_count;
	if (!hott_action_is_valid(
			db, &action, action.id, contexts, substitutions, context_certificates,
			substitution_certificates, bridges, terms, type_declarations,
			operations, judgement
		)) {
		return -1;
	}
	db->actions[db->action_count++] = action;
	*p_action_id = action.id;
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
	if (!db || db->action_count > db->action_capacity ||
		(db->action_count != 0 && !db->actions)) {
		return -1;
	}
	for (uint32_t i = 0; i < db->action_count; ++i) {
		if (!hott_action_is_valid(
				db, &db->actions[i], i, contexts, substitutions,
				context_certificates, substitution_certificates, bridges, terms,
				type_declarations, operations, judgement
			)) {
			return -1;
		}
	}
	return 0;
}
