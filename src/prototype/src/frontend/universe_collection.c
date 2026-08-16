#include "a_program/frontend/universe_collection.h"

#include <string.h>

#include "a_program/frontend/lowering.h"
#include "a_program/graph/typed_occurrence_graph.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"

#define PROTOTYPE_UNIVERSE_DERIVED_LEVEL_FLAG 0x80000000u

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
	if (prototype_universe_ensure_level(db, classifier_level, &classifier_index) != 0) {
		return -1;
	}
	if (term_universe_level_var(terms, subject, &subject_level) == 0) {
		return prototype_universe_add_constraint(
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
	uint32_t claim_id = derivation->premises[premise_index].claim_id;
	const struct prototype_judgement_proposition* proposition =
		claim_id != PROTOTYPE_INVALID_ID ?
		prototype_judgement_claim_proposition(judgement, claim_id) :
		prototype_judgement_premise_proposition(
			judgement, &derivation->premises[premise_index]
		);
	if (!proposition || proposition->classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_classifier = proposition->classifier;
	return 0;
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
		term_universe_level_var(terms, prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier, &result_level) != 0 ||
		prototype_judgement_pi_parts(
			terms, prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject, &domain, &codomain_family
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
		) == 0 && prototype_universe_add_constraint(
			db, domain_level, result_level, 0, prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier,
			PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_PI_DOMAIN,
			claim_id,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_kind,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_id,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier
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
		) == 0 && prototype_universe_add_constraint(
			db, body_level, result_level, 0, prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier,
			PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_PI_CODOMAIN,
			claim_id,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_kind,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_id,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject,
			prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier
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
		if (prototype_universe_add_constraint(
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
		if (prototype_universe_ensure_level(db, pi_level, &index) != 0) {
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
			prototype_universe_add_constraint(
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
				prototype_universe_add_constraint(
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
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	const struct prototype_judgement_claim* relation,
	const struct prototype_judgement_derivation* proof
) {
	uint32_t result_level;
	if (!db || !terms || !relation || !proof ||
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject >= terms->term_count ||
		terms->terms[prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject].tag != PROTOTYPE_TERM_MATCH ||
		term_universe_level_var(terms, prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier, &result_level) != 0) {
		return 0;
	}

	const struct prototype_term* match = &terms->terms[prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject];
	const struct prototype_typed_occurrence* match_operation = NULL;
	if (prototype_judgement_proposition_get(judgement, relation->proposition_id)->occurrence_id != PROTOTYPE_INVALID_ID) {
		if (!operations || prototype_judgement_proposition_get(judgement, relation->proposition_id)->occurrence_id >= operations->occurrence_count ||
			operations->occurrences[prototype_judgement_proposition_get(judgement, relation->proposition_id)->occurrence_id].tag !=
				PROTOTYPE_TYPED_OCCURRENCE_MATCH) {
			return -1;
		}
		match_operation = &operations->occurrences[prototype_judgement_proposition_get(judgement, relation->proposition_id)->occurrence_id];
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
			uint32_t match_occurrence =
				(uint32_t)(match_operation - operations->occurrences);
			uint32_t branch_operation;
			if (prototype_typed_occurrence_graph_child(
					operations, match_occurrence,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i, &branch_operation
				) != 0) {
				return -1;
			}
			uint32_t branch_classifier =
				operations->occurrences[branch_operation].classifier;
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
			prototype_universe_add_constraint(
				db,
				branch_level,
				result_level,
				0,
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject,
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier,
				PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_MATCH_BRANCH,
				claim_id,
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_kind,
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_id,
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject,
				prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier
			) != 0) {
			return -1;
		}
	}

	return 0;
}

/* An APP or expected-type proof may use Universe cumulativity rather than
 * DefEq. Preserve every structurally corresponding v <= u obligation. The
 * accepted proof has already checked alpha-compatible classifier structure,
 * so child roles, rather than BindingIds, align dependent family bodies. This
 * traversal records constraints only; it never turns distinct Universe level
 * variables into judgemental equals. */
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
		return prototype_universe_add_constraint(
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

	/* Surface nested lambdas may be exposed through the canonical pure
	 * quotation Comp({}, Thunk(C)). Follow the same directional boundary as
	 * classifier compatibility before comparing ordinary child topology. */
	if (terms->terms[expected].tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		terms->terms[actual].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		const struct prototype_term* computation = &terms->terms[expected];
		if (computation->as.computation_type.label < terms->term_count &&
			terms->terms[computation->as.computation_type.label].tag ==
				PROTOTYPE_TERM_EFFECT_ROW_EMPTY &&
			computation->as.computation_type.result < terms->term_count &&
			terms->terms[computation->as.computation_type.result].tag ==
				PROTOTYPE_TERM_THUNK_TYPE) {
			return collect_classifier_cumulativity_constraints(
				db,
				terms,
				terms->terms[
					computation->as.computation_type.result
				].as.thunk_type.computation,
				actual,
				subject,
				classifier,
				reason,
				source_claim_id,
				source_authority_kind,
				source_authority_id,
				depth + 1
			);
		}
	}

	if (terms->terms[expected].tag != terms->terms[actual].tag) {
		/* Nominal external-reference and TYPE_VIEW boundaries contain no direct
		 * Universe level. Any applied arguments were already paired by their APP
		 * parents. */
		return 0;
	}

	uint32_t expected_child_count;
	uint32_t actual_child_count;
	if (prototype_term_child_count(
			terms, expected, &expected_child_count
		) != 0 || prototype_term_child_count(
			terms, actual, &actual_child_count
		) != 0 || expected_child_count != actual_child_count) {
		return -1;
	}
	for (uint32_t i = 0; i < expected_child_count; ++i) {
		struct prototype_term_child expected_child;
		struct prototype_term_child actual_child;
		if (prototype_term_child_at(
				terms, expected, i, &expected_child
			) != 0 || prototype_term_child_at(
				terms, actual, i, &actual_child
			) != 0 || expected_child.role != actual_child.role ||
			expected_child.ordinal != actual_child.ordinal ||
			collect_classifier_cumulativity_constraints(
				db,
				terms,
				expected_child.term,
				actual_child.term,
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
	}
	return 0;
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
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject,
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier,
		PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_APP_CUMULATIVITY,
		claim_id,
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_kind,
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_id,
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
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier,
		premise_classifier,
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->subject,
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->classifier,
		PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_EXPECTED_TYPE_CUMULATIVITY,
		claim_id,
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_kind,
		prototype_judgement_proposition_get(judgement, relation->proposition_id)->authority_id,
		0
	);
}

static int collect_relation_constraints(
	struct prototype_universe_db* db,
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	if (claim_id >= judgement->claim_count) {
		return -1;
	}
	const struct prototype_judgement_claim* relation =
		prototype_judgement_claim_get(judgement, claim_id);
	if (!relation) {
		return 0;
	}
	const struct prototype_judgement_proposition* proposition =
		prototype_judgement_proposition_get(judgement, relation->proposition_id);
	if (proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return 0;
	}
	uint32_t classifier_level;
	(void)collect_type_level_at_depth(
		db,
		terms,
		proposition->classifier,
		claim_id,
		proposition->authority_kind,
		proposition->authority_id,
		proposition->subject,
		proposition->classifier,
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
				proposition->subject,
				proposition->classifier,
				claim_id,
				proposition->authority_kind,
				proposition->authority_id
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

int prototype_universe_collect(
	struct prototype_universe_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
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
			fprintf(stderr, "universe collection failed at type node=%u\n", i);
			return -1;
		}
		for (uint32_t j = 0; j < type->parameter_count; ++j) {
			uint32_t parameter_id = type->first_parameter + j;
			const struct prototype_type_parameter_declaration* parameter = &type_declarations->readback.parameter_declarations[parameter_id];
			uint32_t parameter_node;
			if (prototype_universe_add_parameter_node(
				db,
				i,
				parameter_id,
				parameter->name_symbol_id,
				parameter->type_expr,
				&parameter_node
			) != 0) {
				fprintf(
					stderr,
					"universe collection failed at parameter type=%u parameter=%u\n",
					i,
					parameter_id
				);
				return -1;
			}
			if (prototype_universe_add_edge(
				db,
				PROTOTYPE_UNIVERSE_EDGE_PARAMETER_TO_TYPE,
				parameter_node,
				type_node
			) != 0) {
				fprintf(
					stderr,
					"universe collection failed at parameter edge type=%u parameter=%u\n",
					i,
					parameter_id
				);
				return -1;
			}
		}
	}

	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
			uint32_t index;
			if (prototype_universe_ensure_level(db, terms->terms[i].as.universe_var.level_var, &index) != 0) {
				fprintf(stderr, "universe collection failed at term level term=%u\n", i);
				return -1;
			}
		}
	}

	for (size_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim =
			prototype_judgement_claim_get(judgement, (uint32_t)i);
		if (!claim || claim->closure_rank == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (collect_relation_constraints(
				db, terms, operations, judgement, (uint32_t)i
			) != 0) {
			fprintf(stderr, "universe collection failed at claim=%zu\n", i);
			return -1;
		}
	}

	if (prototype_universe_solve(db) != 0) {
		fprintf(stderr, "universe collection constraint solver failed\n");
		return -1;
	}
	if (prototype_universe_validate_provenance(db, judgement) != 0) {
		fprintf(stderr, "universe collection provenance validation failed\n");
		return -1;
	}
	return 0;
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
			fprintf(stderr, "invalid universe provenance header constraint=%zu\n", i);
			return -1;
		}
		if (constraint->source_claim_id == PROTOTYPE_INVALID_ID) {
			if (constraint->source_authority_kind !=
					PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER ||
				constraint->source_authority_id == PROTOTYPE_INVALID_ID) {
				fprintf(stderr, "invalid helper universe provenance constraint=%zu\n", i);
				return -1;
			}
			continue;
		}
			const struct prototype_judgement_claim* claim =
				prototype_judgement_claim_get(
					judgement, constraint->source_claim_id
				);
			if (!claim) {
				fprintf(
					stderr,
					"missing universe provenance claim constraint=%zu claim=%u\n",
					i,
					constraint->source_claim_id
				);
				return -1;
			}
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_proposition_get(judgement, claim->proposition_id);
			if (proposition->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				claim->closure_rank == PROTOTYPE_INVALID_ID ||
				proposition->authority_kind != constraint->source_authority_kind ||
				proposition->authority_id != constraint->source_authority_id ||
				proposition->subject != constraint->source_subject ||
				proposition->classifier != constraint->source_classifier) {
				fprintf(
					stderr,
					"mismatched universe provenance constraint=%zu claim=%u "
					"kind=%d/%d authority=%d:%u/%d:%u subject=%u/%u "
					"classifier=%u/%u rank=%u\n",
					i,
					constraint->source_claim_id,
					proposition->kind,
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
					proposition->authority_kind,
					proposition->authority_id,
					constraint->source_authority_kind,
					constraint->source_authority_id,
					proposition->subject,
					constraint->source_subject,
					proposition->classifier,
					constraint->source_classifier,
					claim->closure_rank
				);
				return -1;
		}
	}
	return 0;
}
