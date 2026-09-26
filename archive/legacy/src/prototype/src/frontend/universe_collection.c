#include "a_program/frontend/universe_collection.h"

#include <stdlib.h>
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
	uint32_t source_derivation_id,
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
			source_derivation_id,
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
	uint32_t derivation_id,
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
			derivation_id,
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
			derivation_id,
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
	uint32_t source_derivation_id,
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
				source_derivation_id,
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
			source_derivation_id,
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
				source_derivation_id,
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
					source_derivation_id,
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
					source_derivation_id,
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
	uint32_t derivation_id,
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
				derivation_id,
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
	uint32_t source_derivation_id,
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
			source_derivation_id,
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
				source_derivation_id,
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
				source_derivation_id,
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
	uint32_t derivation_id,
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
		derivation_id,
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
	uint32_t derivation_id,
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
		derivation_id,
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
	for (uint32_t derivation_id = 0;
		derivation_id < (uint32_t)judgement->derivation_count;
		++derivation_id) {
		const struct prototype_judgement_derivation* proof =
			&judgement->derivations[derivation_id];
		if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID ||
			proof->conclusion_claim_id != claim_id) {
			continue;
		}
		uint32_t first_constraint = (uint32_t)db->constraint_count;
		uint32_t classifier_level;
		(void)collect_type_level_at_depth(
			db,
			terms,
			proposition->classifier,
			claim_id,
			derivation_id,
			proposition->authority_kind,
			proposition->authority_id,
			proposition->subject,
			proposition->classifier,
			&classifier_level,
			0
		);
		int status = collect_universe_term_constraints(
			db,
			terms,
			proposition->subject,
			proposition->classifier,
			claim_id,
			derivation_id,
			proposition->authority_kind,
			proposition->authority_id
		);
		const char* failed_stage = "term";
		if (status == 0) {
			failed_stage = "pi";
			status = collect_pi_constraints(
				db, terms, judgement, claim_id, derivation_id, relation, proof
			);
		}
		if (status == 0) {
			failed_stage = "match";
			status = collect_match_branch_constraints(
				db, terms, operations, judgement, claim_id, derivation_id,
				relation, proof
			);
		}
		if (status == 0) {
			failed_stage = "app";
			status = collect_app_elim_cumulativity_constraint(
				db, terms, judgement, claim_id, derivation_id, relation, proof
			);
		}
		if (status == 0) {
			failed_stage = "expected-type";
			status = collect_expected_type_exposure_constraints(
				db, terms, judgement, claim_id, derivation_id, relation, proof
			);
		}
		if (status == 0 && db->constraint_count > first_constraint) {
			failed_stage = "span";
			status = prototype_universe_add_obligation_span(
				db, claim_id, derivation_id, first_constraint
			);
		}
		if (status != 0) {
			fprintf(
				stderr,
				"universe obligation reconstruction failed claim=%u derivation=%u proof=%d stage=%s spans=%zu/%zu constraints=%zu/%zu\n",
				claim_id,
				derivation_id,
				proof->proof_kind,
				failed_stage,
				db->obligation_span_count,
				db->obligation_span_capacity,
				db->constraint_count,
				db->constraint_capacity
			);
			return -1;
		}
	}
	return 0;
}

static int prototype_universe_reconstruct_obligations(
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

	for (uint32_t i = 0; i < (uint32_t)type_declarations->semantic_schema.type_count; ++i) {
		const struct prototype_type_declaration* type = &type_declarations->semantic_schema.type_declarations[i];
		const struct prototype_type_readback_entry* readback_entry =
			&type_declarations->readback.type_entries[i];
		uint32_t type_node;
		if (prototype_universe_add_type_node(db, i, type->name_symbol_id, &type_node) != 0) {
			fprintf(stderr, "universe collection failed at type node=%u\n", i);
			return -1;
		}
		for (uint32_t j = 0; j < type->parameter_count; ++j) {
			uint32_t parameter_id = readback_entry->first_parameter + j;
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

	return 0;
}

static int prototype_universe_validate_obligations(
	const struct prototype_universe_db* db,
	const struct prototype_judgement_db* judgement
) {
	if (!db || !judgement) {
		return -1;
	}
	uint8_t* covered_derivations = calloc(
		judgement->derivation_count ? judgement->derivation_count : 1,
		sizeof(*covered_derivations)
	);
	if (!covered_derivations) {
		return -1;
	}
	int status = -1;
	for (size_t i = 0; i < db->obligation_span_count; ++i) {
		const struct prototype_universe_obligation_span* span =
			&db->obligation_spans[i];
		if (span->source_claim_id >= judgement->claim_count ||
			span->source_derivation_id >= judgement->derivation_count ||
			span->first_constraint > db->constraint_count ||
			span->constraint_count >
				db->constraint_count - span->first_constraint ||
			covered_derivations[span->source_derivation_id]) {
			goto out;
		}
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[span->source_derivation_id];
		if (derivation->conclusion_claim_id != span->source_claim_id) {
			goto out;
		}
		covered_derivations[span->source_derivation_id] = 1;
		for (uint32_t j = 0; j < span->constraint_count; ++j) {
			const struct prototype_universe_constraint* constraint =
				&db->constraints[span->first_constraint + j];
			if (constraint->source_claim_id != span->source_claim_id ||
				constraint->source_derivation_id !=
					span->source_derivation_id) {
				goto out;
			}
		}
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
			goto out;
		}
		if (constraint->source_claim_id == PROTOTYPE_INVALID_ID) {
			if (constraint->source_authority_kind !=
					PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER ||
				constraint->source_authority_id == PROTOTYPE_INVALID_ID) {
				fprintf(stderr, "invalid helper universe provenance constraint=%zu\n", i);
				goto out;
			}
			continue;
		}
		if (constraint->source_derivation_id >= judgement->derivation_count ||
			!covered_derivations[constraint->source_derivation_id]) {
			goto out;
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
				goto out;
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
				goto out;
		}
	}
	status = 0;
out:
	free(covered_derivations);
	return status;
}

static int prototype_universe_close_program(
	struct prototype_universe_db* db,
	const struct prototype_judgement_db* judgement
) {
	if (prototype_universe_validate_obligations(db, judgement) != 0 ||
		prototype_universe_close(db) != 0) {
		return -1;
	}
	return 0;
}

int prototype_universe_build_closed(
	struct prototype_universe_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	return prototype_universe_reconstruct_obligations(
			db, type_declarations, terms, operations, judgement
		) == 0 ? prototype_universe_close_program(db, judgement) : -1;
}

static int universe_constraint_equal(
	const struct prototype_universe_constraint* left,
	const struct prototype_universe_constraint* right
) {
	return left->lower_level_var == right->lower_level_var &&
		left->upper_level_var == right->upper_level_var &&
		left->offset == right->offset && left->subject == right->subject &&
		left->classifier == right->classifier && left->reason == right->reason &&
		left->source_claim_id == right->source_claim_id &&
		left->source_derivation_id == right->source_derivation_id &&
		left->source_authority_kind == right->source_authority_kind &&
		left->source_authority_id == right->source_authority_id &&
		left->source_subject == right->source_subject &&
		left->source_classifier == right->source_classifier;
}

int prototype_universe_validate_replay(
	const struct prototype_universe_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_db* judgement
) {
	if (!db || !type_declarations || !terms || !judgement ||
		db->certificate.state != PROTOTYPE_UNIVERSE_CERTIFICATE_CLOSED) {
		return -1;
	}
	struct prototype_universe_node* nodes = calloc(
		db->node_capacity ? db->node_capacity : 1, sizeof(*nodes)
	);
	struct prototype_universe_edge* edges = calloc(
		db->edge_capacity ? db->edge_capacity : 1, sizeof(*edges)
	);
	struct prototype_universe_level* levels = calloc(
		db->level_capacity ? db->level_capacity : 1, sizeof(*levels)
	);
	struct prototype_universe_constraint* constraints = calloc(
		db->constraint_capacity ? db->constraint_capacity : 1,
		sizeof(*constraints)
	);
	struct prototype_universe_obligation_span* spans = calloc(
		db->obligation_span_capacity ? db->obligation_span_capacity : 1,
		sizeof(*spans)
	);
	if (!nodes || !edges || !levels || !constraints || !spans) {
		free(nodes);
		free(edges);
		free(levels);
		free(constraints);
		free(spans);
		return -1;
	}
	struct prototype_universe_db rebuilt;
	prototype_universe_db_init(
		&rebuilt,
		nodes,
		db->node_capacity,
		edges,
		db->edge_capacity,
		levels,
		db->level_capacity,
		constraints,
		db->constraint_capacity,
		spans,
		db->obligation_span_capacity
	);
	int status = prototype_universe_build_closed(
		&rebuilt, type_declarations, terms, operations, judgement
	);
	if (status == 0 &&
		(rebuilt.node_count != db->node_count ||
		 rebuilt.edge_count != db->edge_count ||
		 rebuilt.level_count != db->level_count ||
		 rebuilt.constraint_count != db->constraint_count ||
		 rebuilt.obligation_span_count != db->obligation_span_count ||
		 !prototype_universe_certificate_equal(
			&rebuilt.certificate, &db->certificate
		))) {
		status = -1;
	}
	for (size_t i = 0; status == 0 && i < db->node_count; ++i) {
		if (rebuilt.nodes[i].tag != db->nodes[i].tag ||
			rebuilt.nodes[i].type_id != db->nodes[i].type_id ||
			rebuilt.nodes[i].parameter_id != db->nodes[i].parameter_id ||
			rebuilt.nodes[i].type_expr != db->nodes[i].type_expr) {
			status = -1;
		}
	}
	for (size_t i = 0; status == 0 && i < db->edge_count; ++i) {
		if (rebuilt.edges[i].tag != db->edges[i].tag ||
			rebuilt.edges[i].from_node != db->edges[i].from_node ||
			rebuilt.edges[i].to_node != db->edges[i].to_node) {
			status = -1;
		}
	}
	for (size_t i = 0; status == 0 && i < db->level_count; ++i) {
		if (rebuilt.levels[i].level_var != db->levels[i].level_var ||
			rebuilt.levels[i].value != db->levels[i].value) {
			status = -1;
		}
	}
	for (size_t i = 0; status == 0 && i < db->constraint_count; ++i) {
		if (!universe_constraint_equal(
				&rebuilt.constraints[i], &db->constraints[i]
			)) {
			status = -1;
		}
	}
	for (size_t i = 0; status == 0 && i < db->obligation_span_count; ++i) {
		const struct prototype_universe_obligation_span* left =
			&rebuilt.obligation_spans[i];
		const struct prototype_universe_obligation_span* right =
			&db->obligation_spans[i];
		if (left->source_claim_id != right->source_claim_id ||
			left->source_derivation_id != right->source_derivation_id ||
			left->first_constraint != right->first_constraint ||
			left->constraint_count != right->constraint_count) {
			status = -1;
		}
	}
	free(nodes);
	free(edges);
	free(levels);
	free(constraints);
	free(spans);
	return status;
}
