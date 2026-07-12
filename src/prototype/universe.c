#include "universe.h"

#include <string.h>

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
	int reason_kind
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
			constraint->reason_kind == reason_kind) {
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
	db->constraints[id].reason_kind = reason_kind;
	db->constraint_count++;
	(void)lower_index;
	(void)upper_index;
	return 0;
}

static int lookup_classifier(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!judgement || !p_classifier) {
		return -1;
	}
	(void)terms;
	for (size_t i = judgement->relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	return -1;
}

static int collect_universe_term_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	int reason_kind
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
			reason_kind
		);
	}
	return 0;
}

static int collect_pi_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t classifier,
	int reason_kind
) {
	uint32_t result_level;
	uint32_t domain;
	uint32_t codomain_family;
	if (term_universe_level_var(terms, classifier, &result_level) != 0 ||
		prototype_judgement_pi_parts(terms, subject, &domain, &codomain_family) != 0) {
		return 0;
	}

	uint32_t domain_classifier;
	uint32_t domain_level;
	if (lookup_classifier(judgement, terms, domain, &domain_classifier) == 0 &&
		term_universe_level_var(terms, domain_classifier, &domain_level) == 0 &&
		add_constraint(db, domain_level, result_level, 0, subject, classifier, reason_kind) != 0) {
		return -1;
	}

	const struct prototype_term* family = &terms->terms[codomain_family];
	if (family->tag == PROTOTYPE_TERM_LAMBDA) {
		uint32_t body_classifier;
		uint32_t body_level;
		if (lookup_classifier(judgement, terms, family->as.lambda.body, &body_classifier) == 0 &&
			term_universe_level_var(terms, body_classifier, &body_level) == 0 &&
			add_constraint(db, body_level, result_level, 0, subject, classifier, reason_kind) != 0) {
			return -1;
		}
	}

	return 0;
}

static uint32_t derived_level_for_term(uint32_t term_id) {
	return PROTOTYPE_UNIVERSE_DERIVED_LEVEL_FLAG | term_id;
}

static int collect_type_level_at_depth(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t type_term,
	uint32_t* p_level_var,
	uint32_t depth
) {
	if (!db || !terms || !judgement || !p_level_var ||
		type_term >= terms->term_count ||
		depth > 64) {
		return -1;
	}

	uint32_t classifier;
	uint32_t classifier_level;
	if (lookup_classifier(judgement, terms, type_term, &classifier) == 0 &&
		term_universe_level_var(terms, classifier, &classifier_level) == 0) {
		uint32_t index;
		if (ensure_level(db, classifier_level, &index) != 0) {
			return -1;
		}
		*p_level_var = classifier_level;
		return 0;
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
				PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_LEVEL
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
				judgement,
				domain,
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
				PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_LEVEL
			) != 0) {
			return -1;
		}

		const struct prototype_term* family = &terms->terms[codomain_family];
		if (family->tag == PROTOTYPE_TERM_LAMBDA) {
			uint32_t body_level;
			if (collect_type_level_at_depth(
					db,
					terms,
					judgement,
					family->as.lambda.body,
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
					PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_LEVEL
				) != 0) {
				return -1;
			}
		}

		*p_level_var = pi_level;
		return 0;
	}

	return -1;
}

static int collect_match_branch_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	uint32_t match_term,
	uint32_t classifier,
	int reason_kind
) {
	uint32_t result_level;
	if (!db || !terms || !judgement ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		term_universe_level_var(terms, classifier, &result_level) != 0) {
		return 0;
	}

	const struct prototype_term* match = &terms->terms[match_term];
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}

		const struct prototype_match_case* match_case = &terms->cases[case_id];
		uint32_t branch_classifier;
		uint32_t branch_level;
		int found_branch_level = 0;
		if (lookup_classifier(judgement, terms, match_case->body, &branch_classifier) == 0 &&
			term_universe_level_var(terms, branch_classifier, &branch_level) == 0) {
			found_branch_level = 1;
		} else if (collect_type_level_at_depth(
				db,
				terms,
				judgement,
				match_case->body,
				&branch_level,
				0
			) == 0) {
			found_branch_level = 1;
		}

		if (found_branch_level &&
			add_constraint(
				db,
				branch_level,
				result_level,
				0,
				match_term,
				classifier,
				reason_kind
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
			PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
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
			depth + 1
		) != 0) {
		return -1;
	}
	if (expected_family >= terms->term_count || actual_family >= terms->term_count ||
		terms->terms[expected_family].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[actual_family].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	const struct prototype_term* expected_lambda =
		&terms->terms[expected_family];
	const struct prototype_term* actual_lambda = &terms->terms[actual_family];
	if (prototype_term_contains_free_binder(
			terms,
			expected_lambda->as.lambda.body,
			expected_lambda->as.lambda.binder_id
		) ||
		prototype_term_contains_free_binder(
			terms,
			actual_lambda->as.lambda.body,
			actual_lambda->as.lambda.binder_id
		)) {
		return 0;
	}
	return collect_classifier_cumulativity_constraints(
		db,
		terms,
		expected_lambda->as.lambda.body,
		actual_lambda->as.lambda.body,
		subject,
		classifier,
		depth + 1
	);
}

static int collect_app_elim_cumulativity_constraint(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_relation* relation
) {
	if (!db || !terms || !judgement || !relation ||
		relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM ||
		relation->proof_id >= judgement->proof_count) {
		return 0;
	}
	const struct prototype_judgement_proof* proof =
		&judgement->proofs[relation->proof_id];
	if (proof->premise_count != 2 ||
		proof->premise_classifiers[0] >= terms->term_count ||
		proof->premise_classifiers[1] >= terms->term_count) {
		return -1;
	}
	uint32_t domain;
	uint32_t codomain_family;
	if (prototype_judgement_pi_parts(
			terms, proof->premise_classifiers[0], &domain, &codomain_family
		) != 0) {
		return 0;
	}
	(void)codomain_family;
	return collect_classifier_cumulativity_constraints(
		db,
		terms,
		domain,
		proof->premise_classifiers[1],
		relation->subject,
		relation->classifier,
		0
	);
}

static int collect_relation_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_relation* relation
) {
	if (!relation || relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return 0;
	}
	uint32_t classifier_level;
	(void)collect_type_level_at_depth(
		db,
		terms,
		judgement,
		relation->classifier,
		&classifier_level,
		0
	);
	if (collect_universe_term_constraints(
			db,
			terms,
			relation->subject,
			relation->classifier,
			relation->proof_kind
		) != 0 ||
		collect_pi_constraints(
			db,
			terms,
			judgement,
			relation->subject,
			relation->classifier,
			relation->proof_kind
		) != 0 ||
		collect_match_branch_constraints(
			db,
			terms,
			judgement,
			relation->subject,
			relation->classifier,
			relation->proof_kind
		) != 0 ||
		collect_app_elim_cumulativity_constraint(
			db,
			terms,
			judgement,
			relation
		) != 0) {
		return -1;
	}
	return 0;
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

	for (size_t i = 0; i < judgement->relation_count; ++i) {
		if (collect_relation_constraints(db, terms, judgement, &judgement->relations[i]) != 0) {
			return -1;
		}
	}

	return solve_constraints(db);
}
