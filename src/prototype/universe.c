#include "universe.h"

#include <string.h>

#include "ast.h"
#include "judgement.h"

#define PROTOTYPE_UNIVERSE_DERIVED_LEVEL_FLAG 0x80000000u

static int reserve_slot(size_t count, size_t capacity) {
	return count < capacity ? 0 : -1;
}

void prototype_universe_db_init(
	struct prototype_universe_db* db,
	struct prototype_universe_node* nodes,
	size_t node_capacity,
	struct prototype_universe_edge* edges,
	size_t edge_capacity,
	struct prototype_universe_level* levels,
	size_t level_capacity,
	struct prototype_universe_constraint* constraints,
	size_t constraint_capacity
) {
	memset(db, 0, sizeof(*db));
	db->nodes = nodes;
	db->node_capacity = node_capacity;
	db->edges = edges;
	db->edge_capacity = edge_capacity;
	db->levels = levels;
	db->level_capacity = level_capacity;
	db->constraints = constraints;
	db->constraint_capacity = constraint_capacity;
}

void prototype_universe_db_clear(struct prototype_universe_db* db) {
	if (!db) {
		return;
	}
	db->node_count = 0;
	db->edge_count = 0;
	db->level_count = 0;
	db->constraint_count = 0;
	db->solved = 0;
}

static int add_node(
	struct prototype_universe_db* db,
	struct prototype_universe_node node,
	uint32_t* p_node_id
) {
	if (!db || !p_node_id || reserve_slot(db->node_count, db->node_capacity) != 0) {
		return -1;
	}

	uint32_t id = (uint32_t)db->node_count;
	db->nodes[id] = node;
	db->node_count++;
	*p_node_id = id;
	return 0;
}

int prototype_universe_add_type_node(
	struct prototype_universe_db* db,
	uint32_t type_id,
	int symbol_id,
	uint32_t* p_node_id
) {
	struct prototype_universe_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_UNIVERSE_NODE_TYPE;
	node.type_id = type_id;
	node.parameter_id = PROTOTYPE_INVALID_ID;
	node.symbol_id = symbol_id;
	node.type_expr = PROTOTYPE_INVALID_ID;
	return add_node(db, node, p_node_id);
}

int prototype_universe_add_parameter_node(
	struct prototype_universe_db* db,
	uint32_t type_id,
	uint32_t parameter_id,
	int symbol_id,
	uint32_t type_expr,
	uint32_t* p_node_id
) {
	struct prototype_universe_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_UNIVERSE_NODE_PARAMETER;
	node.type_id = type_id;
	node.parameter_id = parameter_id;
	node.symbol_id = symbol_id;
	node.type_expr = type_expr;
	return add_node(db, node, p_node_id);
}

int prototype_universe_add_edge(
	struct prototype_universe_db* db,
	int tag,
	uint32_t from_node,
	uint32_t to_node
) {
	if (!db || reserve_slot(db->edge_count, db->edge_capacity) != 0) {
		return -1;
	}
	if (from_node >= db->node_count || to_node >= db->node_count) {
		return -1;
	}

	uint32_t id = (uint32_t)db->edge_count;
	db->edges[id].tag = tag;
	db->edges[id].from_node = from_node;
	db->edges[id].to_node = to_node;
	db->edge_count++;
	return 0;
}

uint32_t prototype_universe_find_type_node(
	const struct prototype_universe_db* db,
	uint32_t type_id
) {
	if (!db) {
		return PROTOTYPE_INVALID_ID;
	}

	for (size_t i = 0; i < db->node_count; ++i) {
		if (db->nodes[i].tag == PROTOTYPE_UNIVERSE_NODE_TYPE &&
			db->nodes[i].type_id == type_id) {
			return (uint32_t)i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static uint32_t find_level(
	const struct prototype_universe_db* db,
	uint32_t level_var
) {
	if (!db) {
		return PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < db->level_count; ++i) {
		if (db->levels[i].level_var == level_var) {
			return (uint32_t)i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static int ensure_level(
	struct prototype_universe_db* db,
	uint32_t level_var,
	uint32_t* p_index
) {
	if (!db || !p_index) {
		return -1;
	}

	uint32_t existing = find_level(db, level_var);
	if (existing != PROTOTYPE_INVALID_ID) {
		*p_index = existing;
		return 0;
	}

	if (reserve_slot(db->level_count, db->level_capacity) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)db->level_count;
	db->levels[id].level_var = level_var;
	db->levels[id].value = 0;
	db->level_count++;
	*p_index = id;
	return 0;
}

static int term_universe_level_var(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t* p_level_var
) {
	if (!terms || !p_level_var ||
		term_id >= terms->term_count ||
		terms->terms[term_id].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return -1;
	}
	*p_level_var = terms->terms[term_id].as.universe_var.level_var;
	return 0;
}

static int add_constraint(
	struct prototype_universe_db* db,
	uint32_t lower_level_var,
	uint32_t upper_level_var,
	int offset,
	uint32_t subject,
	uint32_t classifier,
	int reason,
	uint32_t source_claim_id,
	int source_authority_kind,
	uint32_t source_authority_id,
	uint32_t source_subject,
	uint32_t source_classifier
) {
	uint32_t lower_index;
	uint32_t upper_index;
	if (!db ||
		ensure_level(db, lower_level_var, &lower_index) != 0 ||
		ensure_level(db, upper_level_var, &upper_index) != 0) {
		return -1;
	}

	for (size_t i = 0; i < db->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &db->constraints[i];
		if (constraint->lower_level_var == lower_level_var &&
			constraint->upper_level_var == upper_level_var &&
			constraint->offset == offset &&
			constraint->subject == subject &&
			constraint->classifier == classifier &&
			constraint->reason == reason &&
			constraint->source_claim_id == source_claim_id &&
			constraint->source_authority_kind == source_authority_kind &&
			constraint->source_authority_id == source_authority_id &&
			constraint->source_subject == source_subject &&
			constraint->source_classifier == source_classifier) {
			return 0;
		}
	}

	if (reserve_slot(db->constraint_count, db->constraint_capacity) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)db->constraint_count;
	db->constraints[id].lower_level_var = lower_level_var;
	db->constraints[id].upper_level_var = upper_level_var;
	db->constraints[id].offset = offset;
	db->constraints[id].subject = subject;
	db->constraints[id].classifier = classifier;
	db->constraints[id].reason = reason;
	db->constraints[id].source_claim_id = source_claim_id;
	db->constraints[id].source_authority_kind = source_authority_kind;
	db->constraints[id].source_authority_id = source_authority_id;
	db->constraints[id].source_subject = source_subject;
	db->constraints[id].source_classifier = source_classifier;
	db->constraint_count++;
	(void)lower_index;
	(void)upper_index;
	return 0;
}

static int collect_universe_term_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	uint32_t source_claim_id,
	int source_authority_kind,
	uint32_t source_authority_id
) {
	uint32_t subject_level;
	uint32_t classifier_level;
	if (term_universe_level_var(terms, classifier, &classifier_level) != 0) {
		return 0;
	}
	uint32_t classifier_index;
	if (ensure_level(db, classifier_level, &classifier_index) != 0) {
		return -1;
	}
	if (term_universe_level_var(terms, subject, &subject_level) == 0) {
		return add_constraint(
			db,
			subject_level,
			classifier_level,
			1,
			subject,
			classifier,
			PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_TERM_LEVEL_SUCCESSOR,
			source_claim_id,
			source_authority_kind,
			source_authority_id,
			subject,
			classifier
		);
	}
	return 0;
}

static int accepted_premise_classifier(
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_derivation* derivation,
	uint32_t premise_index,
	uint32_t* p_classifier
) {
	if (!judgement || !derivation || !p_classifier ||
		premise_index >= derivation->premise_count) {
		return -1;
	}
	uint32_t claim_id = derivation->premise_claim_ids[premise_index];
	if (claim_id != PROTOTYPE_INVALID_ID) {
		if (claim_id >= judgement->claim_count) {
			return -1;
		}
		*p_classifier = judgement->claims[claim_id].classifier;
		return 0;
	}
	*p_classifier = derivation->scoped_premise_classifiers[premise_index];
	return *p_classifier == PROTOTYPE_INVALID_ID ? -1 : 0;
}

static int collect_pi_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	const struct prototype_judgement_claim* relation,
	const struct prototype_judgement_derivation* proof
) {
	uint32_t result_level;
	uint32_t domain;
	uint32_t codomain_family;
	if (!relation || !proof ||
		term_universe_level_var(terms, relation->classifier, &result_level) != 0 ||
		prototype_judgement_pi_parts(
			terms, relation->subject, &domain, &codomain_family
		) != 0) {
		return 0;
	}
	if (proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO ||
		proof->premise_count < 2) {
		return 0;
	}
	uint32_t domain_level;
	uint32_t domain_classifier;
	if (term_universe_level_var(
			terms,
			accepted_premise_classifier(
				judgement, proof, 0, &domain_classifier
			) == 0 ? domain_classifier : PROTOTYPE_INVALID_ID,
			&domain_level
		) == 0 && add_constraint(
			db, domain_level, result_level, 0, relation->subject,
			relation->classifier,
			PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_PI_DOMAIN,
			claim_id,
			relation->authority_kind,
			relation->authority_id,
			relation->subject,
			relation->classifier
		) != 0) {
		return -1;
	}
	uint32_t body_level;
	uint32_t body_classifier;
	if (term_universe_level_var(
			terms,
			accepted_premise_classifier(
				judgement, proof, 1, &body_classifier
			) == 0 ? body_classifier : PROTOTYPE_INVALID_ID,
			&body_level
		) == 0 && add_constraint(
			db, body_level, result_level, 0, relation->subject,
			relation->classifier,
			PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_PI_CODOMAIN,
			claim_id,
			relation->authority_kind,
			relation->authority_id,
			relation->subject,
			relation->classifier
		) != 0) {
		return -1;
	}
	(void)domain;
	(void)codomain_family;
	return 0;
}

static uint32_t derived_level_for_term(uint32_t term_id) {
	return PROTOTYPE_UNIVERSE_DERIVED_LEVEL_FLAG | term_id;
}

static int collect_type_level_at_depth(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	uint32_t type_term,
	uint32_t source_claim_id,
	int source_authority_kind,
	uint32_t source_authority_id,
	uint32_t source_subject,
	uint32_t source_classifier,
	uint32_t* p_level_var,
	uint32_t depth
) {
	if (!db || !terms || !p_level_var ||
		type_term >= terms->term_count ||
		depth > 64) {
		return -1;
	}

	if (terms->terms[type_term].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		uint32_t universe_level = terms->terms[type_term].as.universe_var.level_var;
		uint32_t derived_level = derived_level_for_term(type_term);
		if (add_constraint(
				db,
				universe_level,
				derived_level,
				1,
				type_term,
				type_term,
				PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_UNIVERSE_LEVEL,
				source_claim_id,
				source_authority_kind,
				source_authority_id,
				source_subject,
				source_classifier
			) != 0) {
			return -1;
		}
		*p_level_var = derived_level;
		return 0;
	}

	uint32_t domain;
	uint32_t codomain_family;
	if (prototype_judgement_pi_parts(terms, type_term, &domain, &codomain_family) == 0) {
		uint32_t pi_level = derived_level_for_term(type_term);
		uint32_t index;
		if (ensure_level(db, pi_level, &index) != 0) {
			return -1;
		}

		uint32_t domain_level;
		if (collect_type_level_at_depth(
				db,
			terms,
			domain,
			source_claim_id,
			source_authority_kind,
			source_authority_id,
			source_subject,
			source_classifier,
			&domain_level,
				depth + 1
			) == 0 &&
			add_constraint(
				db,
				domain_level,
				pi_level,
				0,
				type_term,
				type_term,
				PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_PI_DOMAIN,
				source_claim_id,
				source_authority_kind,
				source_authority_id,
				source_subject,
				source_classifier
			) != 0) {
			return -1;
		}

		uint32_t family_binder;
		uint32_t family_body;
		if (prototype_term_pure_family_parts(
				terms, codomain_family, &family_binder, &family_body
			) == 0) {
			uint32_t body_level;
			if (collect_type_level_at_depth(
					db,
					terms,
					family_body,
					source_claim_id,
					source_authority_kind,
					source_authority_id,
					source_subject,
					source_classifier,
					&body_level,
					depth + 1
				) == 0 &&
				add_constraint(
					db,
					body_level,
					pi_level,
					0,
					type_term,
					type_term,
					PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_PI_CODOMAIN,
						source_claim_id,
						source_authority_kind,
						source_authority_id,
						source_subject,
						source_classifier
				) != 0) {
				return -1;
			}
		}
		(void)family_binder;

		*p_level_var = pi_level;
		return 0;
	}

	return -1;
}

static int collect_match_branch_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	const struct prototype_judgement_claim* relation,
	const struct prototype_judgement_derivation* proof
) {
	uint32_t result_level;
	if (!db || !terms || !relation || !proof ||
		relation->subject >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_MATCH ||
		term_universe_level_var(terms, relation->classifier, &result_level) != 0) {
		return 0;
	}

	const struct prototype_term* match = &terms->terms[relation->subject];
	const struct prototype_operation_node* match_operation = NULL;
	if (relation->operation_id != PROTOTYPE_INVALID_ID) {
		if (!operations || relation->operation_id >= operations->operation_count ||
			operations->operations[relation->operation_id].tag !=
				PROTOTYPE_OPERATION_MATCH) {
			return -1;
		}
		match_operation = &operations->operations[relation->operation_id];
	}
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}

		uint32_t branch_level;
		int found_branch_level = 0;
		if (match_operation && i < match_operation->case_count &&
			match_operation->first_case + i < operations->case_count) {
			uint32_t branch_operation = operations->cases[
				match_operation->first_case + i
			].body_operation;
			if (branch_operation >= operations->operation_count) {
				return -1;
			}
			uint32_t branch_classifier =
				operations->operations[branch_operation].classifier;
			if (term_universe_level_var(
					terms, branch_classifier, &branch_level
				) == 0) {
				found_branch_level = 1;
			}
		} else if (i < proof->premise_count) {
			uint32_t premise_classifier;
			if (accepted_premise_classifier(
					judgement, proof, i, &premise_classifier
				) == 0 &&
			term_universe_level_var(
				terms,
				premise_classifier,
				&branch_level
				) == 0) {
				found_branch_level = 1;
			}
		}

		if (found_branch_level &&
			add_constraint(
				db,
				branch_level,
				result_level,
				0,
				relation->subject,
				relation->classifier,
				PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_MATCH_BRANCH,
				claim_id,
				relation->authority_kind,
				relation->authority_id,
				relation->subject,
				relation->classifier
			) != 0) {
			return -1;
		}
	}

	return 0;
}

/* An APP proof may use universe cumulativity rather than DefEq for its
 * argument. Preserve each directly observable v <= u obligation. Closed Pi
 * codomains can be traversed without allocating a comparison binder; open
 * dependent codomains require the later alpha-aware universe comparison. */
static int collect_classifier_cumulativity_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	uint32_t expected,
	uint32_t actual,
	uint32_t subject,
	uint32_t classifier,
	int reason,
	uint32_t source_claim_id,
	int source_authority_kind,
	uint32_t source_authority_id,
	uint32_t depth
) {
	if (!db || !terms || expected >= terms->term_count ||
		actual >= terms->term_count || depth > 32) {
		return -1;
	}
	uint32_t lower_level;
	uint32_t upper_level;
	if (term_universe_level_var(terms, actual, &lower_level) == 0 &&
		term_universe_level_var(terms, expected, &upper_level) == 0) {
		return add_constraint(
			db,
			lower_level,
			upper_level,
			0,
			subject,
			classifier,
			reason,
			source_claim_id,
			source_authority_kind,
			source_authority_id,
			subject,
			classifier
		);
	}
	uint32_t expected_domain;
	uint32_t expected_family;
	uint32_t actual_domain;
	uint32_t actual_family;
	if (prototype_judgement_pi_parts(
			terms, expected, &expected_domain, &expected_family
		) != 0 ||
		prototype_judgement_pi_parts(
			terms, actual, &actual_domain, &actual_family
		) != 0) {
		return 0;
	}
	if (collect_classifier_cumulativity_constraints(
			db,
			terms,
			expected_domain,
			actual_domain,
			subject,
			classifier,
			reason,
			source_claim_id,
			source_authority_kind,
			source_authority_id,
			depth + 1
		) != 0) {
		return -1;
	}
	uint32_t expected_binder;
	uint32_t expected_body;
	uint32_t actual_binder;
	uint32_t actual_body;
	if (prototype_term_pure_family_parts(
			terms, expected_family, &expected_binder, &expected_body
		) != 0 || prototype_term_pure_family_parts(
			terms, actual_family, &actual_binder, &actual_body
		) != 0) {
		return -1;
	}
	if (prototype_term_contains_free_binding(
			terms,
			expected_body,
			expected_binder
		) ||
		prototype_term_contains_free_binding(
			terms,
			actual_body,
			actual_binder
		)) {
		return 0;
	}
	return collect_classifier_cumulativity_constraints(
		db,
		terms,
		expected_body,
		actual_body,
		subject,
		classifier,
		reason,
		source_claim_id,
		source_authority_kind,
		source_authority_id,
		depth + 1
	);
}

static int collect_app_elim_cumulativity_constraint(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	const struct prototype_judgement_claim* relation,
	const struct prototype_judgement_derivation* proof
) {
	if (!db || !terms || !relation || !proof ||
		proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM) {
		return 0;
	}
	uint32_t function_classifier;
	uint32_t argument_classifier;
	if (proof->premise_count != 2 ||
		accepted_premise_classifier(
			judgement, proof, 0, &function_classifier
		) != 0 || accepted_premise_classifier(
			judgement, proof, 1, &argument_classifier
		) != 0 || function_classifier >= terms->term_count ||
		argument_classifier >= terms->term_count) {
		return -1;
	}
	uint32_t domain;
	uint32_t codomain_family;
	if (prototype_judgement_pi_parts(
			terms, function_classifier, &domain, &codomain_family
		) != 0) {
		return 0;
	}
	(void)codomain_family;
	return collect_classifier_cumulativity_constraints(
		db,
		terms,
		domain,
		argument_classifier,
		relation->subject,
		relation->classifier,
		PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_APP_CUMULATIVITY,
		claim_id,
		relation->authority_kind,
		relation->authority_id,
		0
	);
}

static int collect_expected_type_exposure_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	const struct prototype_judgement_claim* relation,
	const struct prototype_judgement_derivation* proof
) {
	if (!db || !terms || !relation || !proof ||
		proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE) {
		return 0;
	}
	uint32_t premise_classifier;
	if (proof->premise_count != 1 || accepted_premise_classifier(
			judgement, proof, 0, &premise_classifier
		) != 0 || premise_classifier >= terms->term_count) {
		return -1;
	}
	return collect_classifier_cumulativity_constraints(
		db,
		terms,
		relation->classifier,
		premise_classifier,
		relation->subject,
		relation->classifier,
		PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_EXPECTED_TYPE_CUMULATIVITY,
		claim_id,
		relation->authority_kind,
		relation->authority_id,
		0
	);
}

static int collect_relation_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	if (claim_id >= judgement->claim_count) {
		return -1;
	}
	const struct prototype_judgement_claim* relation =
		&judgement->claims[claim_id];
	if (!relation || relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return 0;
	}
	uint32_t classifier_level;
	(void)collect_type_level_at_depth(
		db,
		terms,
		relation->classifier,
		claim_id,
		relation->authority_kind,
		relation->authority_id,
		relation->subject,
		relation->classifier,
		&classifier_level,
		0
	);
	for (uint32_t derivation_id = 0;
		derivation_id < (uint32_t)judgement->derivation_count;
		++derivation_id) {
		const struct prototype_judgement_derivation* proof =
			&judgement->derivations[derivation_id];
		if (proof->conclusion_claim_id != claim_id) {
			continue;
		}
		if (collect_universe_term_constraints(
				db,
				terms,
				relation->subject,
				relation->classifier,
				claim_id,
				relation->authority_kind,
				relation->authority_id
			) != 0 ||
			collect_pi_constraints(
				db, terms, judgement, claim_id, relation, proof
			) != 0 ||
			collect_match_branch_constraints(
				db, terms, operations, judgement, claim_id, relation, proof
			) != 0 ||
			collect_app_elim_cumulativity_constraint(
				db, terms, judgement, claim_id, relation, proof
			) != 0 ||
			collect_expected_type_exposure_constraints(
				db, terms, judgement, claim_id, relation, proof
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int constraint_is_first_numerical_edge(
	const struct prototype_universe_db* db,
	size_t constraint_index
) {
	const struct prototype_universe_constraint* current =
		&db->constraints[constraint_index];
	for (size_t i = 0; i < constraint_index; ++i) {
		const struct prototype_universe_constraint* prior = &db->constraints[i];
		if (prior->lower_level_var == current->lower_level_var &&
			prior->upper_level_var == current->upper_level_var &&
			prior->offset == current->offset) {
			return 0;
		}
	}
	return 1;
}

static int solve_constraints(struct prototype_universe_db* db) {
	if (!db) {
		return -1;
	}

	for (size_t i = 0; i < db->level_count; ++i) {
		db->levels[i].value = 0;
	}

	for (size_t pass = 0; pass <= db->level_count; ++pass) {
		int changed = 0;
		for (size_t i = 0; i < db->constraint_count; ++i) {
			if (!constraint_is_first_numerical_edge(db, i)) {
				continue;
			}
			const struct prototype_universe_constraint* constraint = &db->constraints[i];
			uint32_t lower_index = find_level(db, constraint->lower_level_var);
			uint32_t upper_index = find_level(db, constraint->upper_level_var);
			if (lower_index == PROTOTYPE_INVALID_ID || upper_index == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			int required = db->levels[lower_index].value + constraint->offset;
			if (db->levels[upper_index].value < required) {
				if (pass == db->level_count) {
					return -1;
				}
				db->levels[upper_index].value = required;
				changed = 1;
			}
		}
		if (!changed) {
			db->solved = 1;
			return 0;
		}
	}

	return -1;
}

int prototype_universe_collect(
	struct prototype_universe_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || !type_declarations || !terms || !judgement) {
		return -1;
	}

	prototype_universe_db_clear(db);

	for (uint32_t i = 0; i < (uint32_t)type_declarations->type_count; ++i) {
		const struct prototype_type_declaration* type = &type_declarations->type_declarations[i];
		uint32_t type_node;
		if (prototype_universe_add_type_node(db, i, type->name_symbol_id, &type_node) != 0) {
			return -1;
		}
		for (uint32_t j = 0; j < type->parameter_count; ++j) {
			uint32_t parameter_id = type->first_parameter + j;
			const struct prototype_type_parameter_declaration* parameter = &type_declarations->parameter_declarations[parameter_id];
			uint32_t parameter_node;
			if (prototype_universe_add_parameter_node(
				db,
				i,
				parameter_id,
				parameter->name_symbol_id,
				parameter->type_expr,
				&parameter_node
			) != 0) {
				return -1;
			}
			if (prototype_universe_add_edge(
				db,
				PROTOTYPE_UNIVERSE_EDGE_PARAMETER_TO_TYPE,
				parameter_node,
				type_node
			) != 0) {
				return -1;
			}
		}
	}

	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
			uint32_t index;
			if (ensure_level(db, terms->terms[i].as.universe_var.level_var, &index) != 0) {
				return -1;
			}
		}
	}

	for (size_t i = 0; i < judgement->claim_count; ++i) {
		if (collect_relation_constraints(
				db, terms, operations, judgement, (uint32_t)i
			) != 0) {
			return -1;
		}
	}

	if (solve_constraints(db) != 0) {
		return -1;
	}
	return prototype_universe_validate_provenance(db, judgement);
}

int prototype_universe_validate_provenance(
	const struct prototype_universe_db* db,
	const struct prototype_judgement_db* judgement
) {
	if (!db || !judgement) {
		return -1;
	}
	for (size_t i = 0; i < db->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint =
			&db->constraints[i];
		if (constraint->reason <=
				PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_INVALID ||
			constraint->reason >
				PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_PI_CODOMAIN ||
			constraint->source_authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
			return -1;
		}
		if (constraint->source_claim_id == PROTOTYPE_INVALID_ID) {
			if (constraint->source_authority_kind !=
					PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER ||
				constraint->source_authority_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			continue;
		}
		if (constraint->source_claim_id >= judgement->claim_count) {
			return -1;
		}
		const struct prototype_judgement_claim* claim =
			&judgement->claims[constraint->source_claim_id];
		if (claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			claim->closure_rank == PROTOTYPE_INVALID_ID ||
			claim->authority_kind != constraint->source_authority_kind ||
			claim->authority_id != constraint->source_authority_id ||
			claim->subject != constraint->source_subject ||
			claim->classifier != constraint->source_classifier) {
			return -1;
		}
	}
	return 0;
}

int prototype_universe_rebind_provenance(
	struct prototype_universe_db* db,
	const struct prototype_judgement_db* judgement
) {
	if (!db || !judgement) {
		return -1;
	}
	for (size_t i = 0; i < db->constraint_count; ++i) {
		struct prototype_universe_constraint* constraint = &db->constraints[i];
		if (constraint->source_claim_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t found = PROTOTYPE_INVALID_ID;
		for (uint32_t j = 0; j < (uint32_t)judgement->claim_count; ++j) {
			const struct prototype_judgement_claim* claim = &judgement->claims[j];
			if (claim->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				claim->authority_kind != constraint->source_authority_kind ||
				claim->authority_id != constraint->source_authority_id ||
				claim->subject != constraint->source_subject ||
				claim->classifier != constraint->source_classifier ||
				claim->closure_rank == PROTOTYPE_INVALID_ID) {
				continue;
			}
			if (found != PROTOTYPE_INVALID_ID) {
				return -1;
			}
			found = j;
		}
		if (found == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		constraint->source_claim_id = found;
	}
	return 0;
}
