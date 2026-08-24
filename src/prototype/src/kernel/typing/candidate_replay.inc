int prototype_judgement_delta_has_pending_classifier_state(
	const struct prototype_judgement_delta* delta
) {
	if (!delta) {
		return -1;
	}
	return delta->match_motive_result_count > 0 ? 1 : 0;
}

static int judgement_candidate_claim_has_derivation(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int include_conversion
) {
	if (!judgement) {
		return -1;
	}
	for (size_t i = 0; i < judgement->proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &judgement->propositions[i];
		if (relation->kind == kind &&
			relation->context_id == context_id &&
			relation->subject == subject &&
			relation->classifier == classifier) {
			uint32_t cursor = 0;
			uint32_t derivation_id;
			while (prototype_judgement_candidate_derivation_for_conclusion_next(
				judgement->propositions,
				judgement->proposition_count,
				judgement->derivation_candidates,
				judgement->derivation_candidate_count,
				(uint32_t)i,
				&cursor,
				&derivation_id
			) == 0) {
				if (!include_conversion &&
					judgement->derivation_candidates[derivation_id].proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
					continue;
				}
				return 0;
			}
		}
	}
	return -1;
}

static void initialize_proof_premises(
	struct prototype_judgement_derivation_candidate* proof,
	uint32_t default_context_id,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
) {
	proof->premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		proof->premises[i].builder_proposition->kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof->premises[i].builder_proposition->context_id = premise_context_ids ?
			premise_context_ids[i] : default_context_id;
		proof->premises[i].builder_proposition->subject = premise_subjects[i];
		proof->premises[i].builder_proposition->classifier = premise_classifiers[i];
		proof->premises[i].builder_proposition->authority_kind =
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID;
		proof->premises[i].builder_proposition->authority_id = PROTOTYPE_INVALID_ID;
		proof->premises[i].builder_proposition->occurrence_id = PROTOTYPE_INVALID_ID;
	}
}

static void initialize_proof_rule_parameters(
	struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* premises,
	struct prototype_judgement_proposition* premise_propositions
) {
	if (!proof || !premises || !premise_propositions) {
		return;
	}
	memset(
		premises,
		0,
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES * sizeof(*premises)
	);
	memset(
		premise_propositions,
		0,
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES *
			sizeof(*premise_propositions)
	);
	proof->premises = premises;
	memset(&proof->rule_data, 0xff, sizeof(proof->rule_data));
	proof->semantic_action_kind = PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
	proof->semantic_action_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES; ++i) {
		proof->premises[i].proposition_store_kind =
			PROTOTYPE_JUDGEMENT_PROPOSITION_STORE_INVALID;
		proof->premises[i].proposition_id = PROTOTYPE_INVALID_ID;
		proof->premises[i].proposition = &premise_propositions[i];
		proof->premises[i].builder_proposition = &premise_propositions[i];
		premise_propositions[i].authority_kind =
			PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID;
		premise_propositions[i].authority_id = PROTOTYPE_INVALID_ID;
		premise_propositions[i].occurrence_id = PROTOTYPE_INVALID_ID;
		proof->premises[i].semantic_action_kind =
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_INVALID;
		proof->premises[i].semantic_action_id = PROTOTYPE_INVALID_ID;
	}
}

static int judgement_operation_candidate_claim_has_derivation(
	const struct prototype_judgement_db* judgement,
	uint32_t operation_id,
	uint32_t context_id,
	int kind,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement) {
		return -1;
	}
	for (size_t i = judgement->proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation =
			&judgement->propositions[i - 1];
		if (relation->occurrence_id == operation_id &&
			relation->context_id == context_id && relation->kind == kind &&
			relation->subject == subject && relation->classifier == classifier) {
			uint32_t cursor = 0;
			uint32_t derivation_id;
			if (prototype_judgement_candidate_derivation_for_conclusion_next(
				judgement->propositions,
				judgement->proposition_count,
				judgement->derivation_candidates,
				judgement->derivation_candidate_count,
				(uint32_t)(i - 1),
				&cursor,
				&derivation_id
			) == 0) {
				return 0;
			}
		}
	}
	return 1;
}

/* Returns zero when this structural premise has an authoritative Operation
 * owner, one when the rule still uses non-structural evidence, and -1 for an
 * Operation/rule shape mismatch. INVALID is a deliberate owner for binder or
 * compiler-generated Core facts. */
static uint32_t judgement_operation_evidence_owner(
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	uint32_t depth
) {
	if (!operations || operation_id >= operations->occurrence_count || depth > 64) {
		return operation_id;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[operation_id];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
		return operation->context_action_substitution == PROTOTYPE_INVALID_ID ?
			PROTOTYPE_INVALID_ID : operation_id;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
		operation->wrapped_occurrence < operations->occurrence_count &&
		operation->classifier ==
			operations->occurrences[operation->wrapped_occurrence].classifier) {
		return judgement_operation_evidence_owner(
			operations, operation->wrapped_occurrence, depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
		operation->wrapped_occurrence < operations->occurrence_count &&
		operation->classifier ==
			operations->occurrences[operation->wrapped_occurrence].classifier) {
		return judgement_operation_evidence_owner(
			operations, operation->wrapped_occurrence, depth + 1
		);
	}
	return operation_id;
}

static uint32_t judgement_operation_evidence_source(
	const struct prototype_typed_occurrence_graph* operations,
	uint32_t operation_id,
	uint32_t depth
) {
	if (!operations || operation_id >= operations->occurrence_count || depth > 64) {
		return operation_id;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[operation_id];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
		return judgement_operation_evidence_source(
			operations, operation->wrapped_occurrence, depth + 1
		);
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
		operation->wrapped_occurrence < operations->occurrence_count &&
		operation->classifier ==
			operations->occurrences[operation->wrapped_occurrence].classifier) {
		return judgement_operation_evidence_source(
			operations, operation->wrapped_occurrence, depth + 1
		);
	}
	return operation_id;
}

static int judgement_occurrence_child(
	const struct prototype_typed_occurrence_graph* occurrences,
	uint32_t parent,
	int role,
	uint32_t ordinal,
	uint32_t* p_child
) {
	return prototype_typed_occurrence_graph_child(
		occurrences, parent, role, ordinal, p_child
	) == 0 ? 0 : -1;
}

static int judgement_structural_premise_child(
	const struct prototype_typed_occurrence_graph* operations,
	int proof_kind,
	uint32_t conclusion_occurrence_id,
	uint32_t premise_count,
	uint32_t premise_index,
	uint32_t* p_child_operation_id
) {
	if (!operations || !p_child_operation_id ||
		conclusion_occurrence_id == PROTOTYPE_INVALID_ID ||
		conclusion_occurrence_id >= operations->occurrence_count ||
		premise_index >= premise_count) {
		return 1;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[conclusion_occurrence_id];
	switch (proof_kind) {
	case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
			premise_count != 2) {
			return -1;
		}
		return judgement_occurrence_child(
			operations, conclusion_occurrence_id,
			PROTOTYPE_TERM_CHILD_BODY, 0, p_child_operation_id
		);
	case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
			premise_count != 2) {
			return -1;
		}
		return judgement_occurrence_child(
			operations, conclusion_occurrence_id,
			premise_index == 0 ? PROTOTYPE_TERM_CHILD_FUNCTION :
				PROTOTYPE_TERM_CHILD_ARGUMENT,
			0, p_child_operation_id
		);
	case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION: {
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
			operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			return -1;
		}
		uint32_t reverse_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t argument_count = 0;
		uint32_t cursor = conclusion_occurrence_id;
		while (cursor < operations->occurrence_count) {
			const struct prototype_typed_occurrence* cursor_operation =
				&operations->occurrences[cursor];
			if (cursor_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
				cursor_operation->application_role !=
					PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
				break;
			}
			uint32_t argument;
			uint32_t function;
			if (argument_count >= PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
				judgement_occurrence_child(
					operations, cursor, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
					&argument
				) != 0 || judgement_occurrence_child(
					operations, cursor, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
					&function
				) != 0) {
				return -1;
			}
			reverse_arguments[argument_count++] = argument;
			cursor = function;
		}
		if (argument_count != premise_count) {
			return -1;
		}
		*p_child_operation_id =
			reverse_arguments[argument_count - premise_index - 1];
		return 0;
	}
	case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			operation->first_case + operation->case_count > operations->case_count) {
			return -1;
		}
		{
			uint32_t reachable_index = 0;
			for (uint32_t case_index = 0;
				case_index < operation->case_count; ++case_index) {
				const struct prototype_typed_occurrence_match_case* operation_case =
					&operations->cases[operation->first_case + case_index];
				if (operation_case->refinement_status ==
					PROTOTYPE_TYPED_OCCURRENCE_MATCH_REFINEMENT_IMPOSSIBLE) {
					continue;
				}
				if (!prototype_typed_occurrence_match_case_is_admitted(
						operation_case
					) ||
					reachable_index >= premise_count) {
					return -1;
				}
				if (reachable_index++ == premise_index) {
					return judgement_occurrence_child(
						operations, conclusion_occurrence_id,
						PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, case_index,
						p_child_operation_id
					);
				}
			}
			return reachable_index == premise_count ? 1 : -1;
		}
	case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
		if ((operation->tag != PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
			 operation->tag != PROTOTYPE_TYPED_OCCURRENCE_THUNK &&
			 operation->tag != PROTOTYPE_TYPED_OCCURRENCE_FORCE) ||
			premise_count != 1) {
			return -1;
		}
		return judgement_occurrence_child(
			operations, conclusion_occurrence_id,
			operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ?
				PROTOTYPE_TERM_CHILD_RETURN_VALUE :
				(operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ?
					PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION :
					PROTOTYPE_TERM_CHILD_FORCE_VALUE),
			0, p_child_operation_id
		);
	case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_REQUEST ||
			premise_count != 3) {
			return -1;
		}
		if (premise_index == 0) {
			*p_child_operation_id = PROTOTYPE_INVALID_ID;
			return 0;
		}
		return judgement_occurrence_child(
			operations, conclusion_occurrence_id,
			premise_index == 1 ? PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT :
				PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION,
			0, p_child_operation_id
		);
	case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
			premise_count != 2 + 2 * operation->fold_clause_count) {
			return -1;
		}
		if (premise_index < 2) {
			return judgement_occurrence_child(
				operations, conclusion_occurrence_id,
				premise_index == 0 ? PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION :
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE,
				0, p_child_operation_id
			);
		}
		if (operation->first_fold_clause > operations->fold_clause_count ||
			operation->fold_clause_count > operations->fold_clause_count -
				operation->first_fold_clause) {
			return -1;
		}
		{
			uint32_t clause_index = (premise_index - 2) / 2;
			const struct prototype_typed_occurrence_fold_clause* clause =
				&operations->fold_clauses[
					operation->first_fold_clause + clause_index
				];
			(void)clause;
			if (judgement_occurrence_child(
					operations, conclusion_occurrence_id,
					(premise_index - 2) % 2 == 0 ?
						PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION :
						PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY,
					clause_index, p_child_operation_id
				) != 0) {
				return -1;
			}
		}
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			premise_count != operation->case_count + 1 ||
			operation->first_case > operations->case_count ||
			operation->case_count > operations->case_count - operation->first_case) {
			return -1;
		}
		if (premise_index == 0) {
			/* The motive classifier is generated by the kernel. It has no
			 * source Operation occurrence. */
			*p_child_operation_id = PROTOTYPE_INVALID_ID;
			return 0;
		}
		{
			if (judgement_occurrence_child(
					operations, conclusion_occurrence_id,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
					premise_index - 1, p_child_operation_id
				) != 0) {
				return -1;
			}
		}
		return 0;
	case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
	case PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE:
	case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY:
	case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN:
	case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
		if (premise_count != 1) {
			return -1;
		}
		/* These are derived boundaries, not syntax-directed eliminators. Their
		 * source may be a nested annotation, a Context fact, or a generated helper
		 * and is carried by the exact SelectedEvidence premise. Reconstructing it
		 * from the conclusion Operation erases that distinction. */
		return 1;
	default:
		return 1;
	}
}

static int judgement_expected_premise_operation(
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_derivation_candidate* proof,
	uint32_t premise_index,
	uint32_t* p_operation_id
) {
	if (proof && proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO &&
		premise_index == 0) {
		*p_operation_id = PROTOTYPE_INVALID_ID;
		return 0;
	}
	uint32_t child_operation_id;
	int status = judgement_structural_premise_child(
		operations,
		proof ? proof->proof_kind : PROTOTYPE_JUDGEMENT_PROOF_INVALID,
		proof ? proof->conclusion->occurrence_id : PROTOTYPE_INVALID_ID,
		proof ? proof->premise_count : 0,
		premise_index,
		&child_operation_id
	);
	if (status != 0) {
		return status;
	}
	*p_operation_id = child_operation_id == PROTOTYPE_INVALID_ID ?
		PROTOTYPE_INVALID_ID : judgement_operation_evidence_owner(
			operations, child_operation_id, 0
		);
	return 0;
}

static int judgement_candidate_is_publishable(
	const struct prototype_judgement_db* judgement,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_proposition* candidate,
	const struct prototype_judgement_derivation_candidate* proof
);

static int judgement_rule_derives_alternate_operation_classifier(int proof_kind) {
	return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TOTAL_COMPUTATION;
}

static int judgement_proof_kind_is_known(int proof_kind) {
	switch (proof_kind) {
	case PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION:
	case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
	case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
	case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
	case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
	case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
	case PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
	case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
	case PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM:
	case PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
	case PROTOTYPE_JUDGEMENT_PROOF_EXPECTED_TYPE_EXPOSURE:
	case PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_ADMISSIBILITY:
	case PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE:
	case PROTOTYPE_JUDGEMENT_PROOF_DECLARATION:
	case PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY:
	case PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN:
	case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_TYPE_FORMATION:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_WITNESS_INTRO:
	case PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_CONSTRUCTOR_WITNESS:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_APP_WITNESS:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_LAMBDA_WITNESS:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_MATCH_WITNESS:
	case PROTOTYPE_JUDGEMENT_PROOF_RELATION_INDUCTION_HYPOTHESIS_WITNESS:
	case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_TYPE_FORMATION:
	case PROTOTYPE_JUDGEMENT_PROOF_THUNK_TYPE_FORMATION:
	case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TYPE_FORMATION:
	case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_TERM:
	case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_CONSTRUCTOR:
	case PROTOTYPE_JUDGEMENT_PROOF_DIMENSION_ACTION_THUNK_RETURN_WITNESS:
	case PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TYPE_FORMATION:
	case PROTOTYPE_JUDGEMENT_PROOF_TERMINATES_TOTAL_COMPUTATION:
	case PROTOTYPE_JUDGEMENT_PROOF_STATIC_FAMILY_APP_ELIM:
		return 1;
	default:
		return 0;
	}
}

int prototype_judgement_proof_reconstruction_role(
	int proof_kind,
	const struct prototype_judgement_proposition* proposition
) {
	if (!proposition || !judgement_proof_kind_is_known(proof_kind)) {
		return PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_INVALID;
	}
	if (proposition->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE) {
		return judgement_rule_derives_alternate_operation_classifier(proof_kind) ?
			PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_DERIVED_OPERATION :
			PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_PRINCIPAL;
	}
	switch (proof_kind) {
	case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
	case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
	case PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM:
	case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_WEAKEN:
	case PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX:
		return PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_SCOPED;
	default:
		return PROTOTYPE_JUDGEMENT_PROOF_RECONSTRUCTION_NON_OPERATION;
	}
}

static int judgement_premise_is_scoped_rule_parameter(
	const struct prototype_judgement_derivation_candidate* derivation,
	uint32_t premise_index
) {
	if (!derivation || premise_index >= derivation->premise_count) {
		return 0;
	}
	/* An operation handler clause is checked under the enclosing fold's
	 * specialized operation signature and carrier. Its Lambda classifier may
	 * contain a row variable scoped by that fold, so it is a replayable fold
	 * rule parameter rather than an independently publishable Claim. The fold
	 * validator replays the specialization and carrier checks. */
	if (derivation->proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM) {
		return premise_index >= 3 && (premise_index & 1u) != 0;
	}
	/* The APP(operation, argument) used by an operation request is generated
	 * inside the request rule and has no independent Operation occurrence. The
	 * request validator reconstructs and checks that application. */
	return derivation->proof_kind ==
			PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO &&
		premise_index == 0;
}

static int judgement_candidate_source_claim(
	struct prototype_judgement_db* judgement,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_derivation_candidate* derivation,
	uint32_t premise_index,
	uint32_t* p_claim_id
) {
	if (!judgement || !derivation || !p_claim_id ||
		premise_index >= derivation->premise_count) {
		return -1;
	}
	uint32_t expected_operation_id =
		derivation->premises[premise_index].proposition->occurrence_id;
	int expected_authority_kind =
		derivation->premises[premise_index].proposition->authority_kind;
	uint32_t expected_authority_id =
		derivation->premises[premise_index].proposition->authority_id;
	uint32_t expected_context_id =
		derivation->premises[premise_index].proposition->context_id;
	uint32_t structural_child_operation_id;
	/* A substitution edge is the explicit proof that moves a structural child
	 * into its branch-local Context. Reconstructing the premise from the source
	 * Operation would discard that reindexing and make indexed Match evidence
	 * impossible to publish. Accepted replay validates the substitution and its
	 * connection to the structural branch. */
	int structural_status =
		derivation->premises[premise_index].semantic_action_kind ==
			PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ?
		1 : judgement_structural_premise_child(
			operations,
			derivation->proof_kind,
			derivation->conclusion->occurrence_id,
			derivation->premise_count,
			premise_index,
			&structural_child_operation_id
		);
	if (structural_status < 0) {
		fprintf(
			stderr,
			"P0 structural premise shape failed rule=%d conclusion_occurrence=%u "
			"index=%u count=%u\n",
			derivation->proof_kind,
			derivation->conclusion->occurrence_id,
			premise_index,
			derivation->premise_count
		);
		return -1;
	}
	if (structural_status == 0) {
		if (structural_child_operation_id == PROTOTYPE_INVALID_ID) {
			expected_operation_id = PROTOTYPE_INVALID_ID;
			if (expected_authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
				expected_authority_kind =
					PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER;
				expected_authority_id =
					derivation->premises[premise_index].proposition->subject;
			}
		} else {
			const struct prototype_typed_occurrence* child =
				&operations->occurrences[structural_child_operation_id];
			/* Explicit SelectedEvidence may name a derived weakening Claim in
			 * the conclusion Context. Its own Derivation carries the Context
			 * substitution, so replacing that Context with the raw child
			 * occurrence would bypass the proof edge. */
			if (expected_authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
				expected_context_id = child->context_id;
			}
			uint32_t structural_operation_id =
				judgement_operation_evidence_owner(
					operations, structural_child_operation_id, 0
				);
			if (derivation->premises[premise_index].proposition->occurrence_id !=
					PROTOTYPE_INVALID_ID &&
				derivation->premises[premise_index].proposition->occurrence_id !=
					structural_child_operation_id &&
				derivation->premises[premise_index].proposition->occurrence_id !=
					structural_operation_id) {
				fprintf(
					stderr,
					"P0 structural operation mismatch rule=%d index=%u "
					"stored=%u expected=%u\n",
					derivation->proof_kind,
					premise_index,
					derivation->premises[premise_index].proposition->occurrence_id,
					structural_child_operation_id
				);
				return -1;
			}
			/* The structural child selects the occurrence and Context. If the
			 * producer retained explicit SelectedEvidence, its authority may be a
			 * derived Context-weakening Claim rather than the child's default
			 * ContextBinding/Operation owner. Preserve it. Only derive a default
			 * authority when the producer supplied none. */
			if (expected_authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID) {
				expected_operation_id = structural_operation_id;
				if (derivation->proof_kind ==
						PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO &&
					premise_index == 0) {
					expected_authority_kind =
						PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING;
					expected_authority_id =
						derivation->premises[premise_index].proposition->subject;
					expected_operation_id = PROTOTYPE_INVALID_ID;
				} else if (structural_operation_id != PROTOTYPE_INVALID_ID) {
					expected_authority_kind =
						PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE;
					expected_authority_id = structural_operation_id;
				} else {
					uint32_t authority_source_operation_id =
						judgement_operation_evidence_source(
							operations, structural_child_operation_id, 0
						);
					if (authority_source_operation_id >=
						operations->occurrence_count) {
						return -1;
					}
					const struct prototype_typed_occurrence* authority_source =
						&operations->occurrences[authority_source_operation_id];
					if (authority_source->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR) {
						expected_authority_kind =
							PROTOTYPE_JUDGEMENT_AUTHORITY_CONTEXT_BINDING;
						expected_authority_id =
							derivation->premises[premise_index].proposition->subject;
					} else if (authority_source->tag ==
						PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR) {
						expected_authority_kind =
							PROTOTYPE_JUDGEMENT_AUTHORITY_TYPE_DECLARATION;
						expected_authority_id =
							derivation->premises[premise_index].proposition->subject;
					} else {
						fprintf(
							stderr,
							"P0 unsupported authority-neutral structural child "
							"rule=%d index=%u child=%u tag=%d source=%u "
							"source_tag=%d\n",
							derivation->proof_kind,
							premise_index,
							structural_child_operation_id,
							child->tag,
							authority_source_operation_id,
							authority_source->tag
						);
						return -1;
					}
				}
			} else if (expected_authority_kind ==
				PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE) {
				/* premise_occurrence_ids keeps the direct structural child. The
				 * selected Claim may belong to the child's transparent NAME or
				 * ASCRIPTION source, so Claim lookup must use that evidence owner. */
				expected_operation_id = expected_authority_id;
			}
		}
	}
	uint32_t selected_claim_id = PROTOTYPE_INVALID_ID;
	for (uint32_t claim_id = 0;
		claim_id < (uint32_t)judgement->claim_count; ++claim_id) {
		const struct prototype_judgement_proposition* candidate =
			prototype_judgement_claim_proposition(judgement, claim_id);
		if (!candidate ||
			candidate->kind != derivation->premises[premise_index].proposition->kind ||
			candidate->context_id != expected_context_id ||
			candidate->subject != derivation->premises[premise_index].proposition->subject ||
			candidate->classifier !=
				derivation->premises[premise_index].proposition->classifier) {
			continue;
		}
		int candidate_authority_kind;
		uint32_t candidate_authority_id;
		candidate_claim_authority(
			candidate, &candidate_authority_kind, &candidate_authority_id
		);
		if (expected_authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID &&
			(candidate_authority_kind != expected_authority_kind ||
			 candidate_authority_id != expected_authority_id)) {
			continue;
		}
		if (expected_authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE &&
			candidate->occurrence_id != expected_operation_id) {
			continue;
		}
		int has_derivation = 0;
		for (uint32_t derivation_id = 0;
			derivation_id < (uint32_t)judgement->derivation_count;
			++derivation_id) {
			if (judgement->derivations[derivation_id].conclusion_claim_id ==
				claim_id) {
				has_derivation = 1;
				break;
			}
		}
		if (!has_derivation) {
			continue;
		}
		if (selected_claim_id != PROTOTYPE_INVALID_ID &&
			selected_claim_id != claim_id) {
			/* An authority-neutral Core helper may match several typed
			 * occurrences. It cannot choose one of them; leave only this
			 * candidate Derivation unpublished. */
			return 1;
		}
		selected_claim_id = claim_id;
	}
	if (selected_claim_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_claim_id = selected_claim_id;
	return 0;
}

static int judgement_candidate_is_publishable(
	const struct prototype_judgement_db* judgement,
	const struct prototype_typed_occurrence_graph* operations,
	const struct prototype_judgement_proposition* candidate,
	const struct prototype_judgement_derivation_candidate* proof
) {
	if (!judgement || !candidate || !proof ||
		candidate->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN ||
		proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
		return 0;
	}
	if (proof->conclusion->kind != candidate->kind ||
		proof->conclusion->context_id != candidate->context_id ||
		proof->conclusion->occurrence_id != candidate->occurrence_id ||
		proof->conclusion->subject != candidate->subject ||
		proof->conclusion->classifier != candidate->classifier) {
		return 0;
	}
	if (candidate->authority_kind != PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE) {
		return 1;
	}
	if (!operations || candidate->occurrence_id >= operations->occurrence_count) {
		return 0;
	}
	const struct prototype_typed_occurrence* operation =
		&operations->occurrences[candidate->occurrence_id];
	if (operation->classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	/* Fixed-point candidates are solver history. Only the classifier selected by
	 * the final typed occurrence is a source Claim. Explicit derived rules may
	 * expose another convertible/admissible classifier over that occurrence. */
	return operation->classifier == candidate->classifier ||
		judgement_rule_derives_alternate_operation_classifier(
			proof->proof_kind
		);
}

static int judgement_rebuild_claim_derivations(
	struct prototype_judgement_db* judgement,
	const struct prototype_typed_occurrence_graph* operations
) {
	if (!judgement) {
		return -1;
	}
	/* Accepted evidence is monotone. Imported certificates and earlier solver
	 * publications remain authoritative; publication only interns additional
	 * candidate conclusions and derivations. */
	if (prototype_judgement_db_rebuild_index(judgement) != 0) {
		return -1;
	}
	int changed;
	do {
		changed = 0;
		for (uint32_t i = 0;
			i < (uint32_t)judgement->derivation_candidate_count; ++i) {
			const struct prototype_judgement_derivation_candidate* proof =
				&judgement->derivation_candidates[i];
			if (proof->conclusion_proposition_id >=
				judgement->proposition_count) {
				continue;
			}
			const struct prototype_judgement_proposition* relation =
				&judgement->propositions[proof->conclusion_proposition_id];
			if (!judgement_candidate_is_publishable(
					judgement, operations, relation, proof
				)) {
				continue;
			}
			struct prototype_judgement_derivation derivation;
			struct prototype_judgement_premise_edge
				premises[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			memset(&derivation, 0, sizeof(derivation));
			memset(premises, 0, sizeof(premises));
			derivation.premises = premises;
			derivation.proof_kind = proof->proof_kind;
			derivation.conclusion_claim_id = PROTOTYPE_INVALID_ID;
			derivation.closure_rank = PROTOTYPE_INVALID_ID;
			derivation.rule_data = proof->rule_data;
			derivation.semantic_action_kind = proof->semantic_action_kind;
			derivation.semantic_action_id = proof->semantic_action_id;
			derivation.premise_count = proof->premise_count;
			for (uint32_t j = 0; j < proof->premise_count; ++j) {
				derivation.premises[j].claim_id = PROTOTYPE_INVALID_ID;
				derivation.premises[j].scoped_proposition_id = PROTOTYPE_INVALID_ID;
				derivation.premises[j].semantic_action_kind =
					proof->premises[j].semantic_action_kind;
				derivation.premises[j].semantic_action_id =
					proof->premises[j].semantic_action_id;
			}
			int sources_resolved = 1;
			for (uint32_t j = 0; j < proof->premise_count; ++j) {
				if (judgement_premise_is_scoped_rule_parameter(proof, j)) {
					if (proof->premises[j].proposition->kind ==
						PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
						fprintf(
							stderr,
							"P0 producer emitted unknown scoped premise rule=%d "
							"index=%u subject=%u classifier=%u\n",
							proof->proof_kind,
							j,
							proof->premises[j].proposition->subject,
							proof->premises[j].proposition->classifier
						);
						return -1;
					}
					struct prototype_judgement_proposition scoped =
						*proof->premises[j].proposition;
					scoped.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID;
					scoped.authority_id = PROTOTYPE_INVALID_ID;
					scoped.occurrence_id = PROTOTYPE_INVALID_ID;
					if (prototype_judgement_proposition_intern(
							judgement,
							&scoped,
							&derivation.premises[j].scoped_proposition_id
						) != 0) {
						return -1;
					}
					continue;
				}
				uint32_t source_claim_id;
				int source_status = judgement_candidate_source_claim(
					judgement, operations, proof, j, &source_claim_id
				);
				if (source_status < 0) {
					return -1;
				}
				if (source_status > 0) {
					sources_resolved = 0;
					break;
				}
				derivation.premises[j].claim_id = source_claim_id;
			}
			if (!sources_resolved) {
				continue;
			}
			uint32_t conclusion_claim_id;
			if (judgement_intern_claim(
					judgement, relation, &conclusion_claim_id
				) != 0) {
				fprintf(stderr, "P0 conclusion intern failed candidate=%u\n", i);
				return -1;
			}
			derivation.conclusion_claim_id = conclusion_claim_id;
			for (uint32_t j = 0; j < derivation.premise_count; ++j) {
				if (derivation.premises[j].claim_id == conclusion_claim_id) {
					sources_resolved = 0;
					break;
				}
			}
			if (!sources_resolved) {
				continue;
			}
			uint32_t derivation_id;
			size_t previous_derivation_count = judgement->derivation_count;
			if (prototype_judgement_derivation_intern_exact(
					judgement, &derivation, &derivation_id
				) != 0) {
				fprintf(
					stderr,
					"P0 derivation capacity failed count=%zu capacity=%zu "
					"candidate=%u\n",
					judgement->derivation_count,
					judgement->derivation_capacity,
					i
				);
				return -1;
			}
			if (judgement->derivation_count > previous_derivation_count) {
				changed = 1;
			}
		}
	} while (changed);
	return 0;
}

int prototype_judgement_recompute_closure_ranks(
	const struct prototype_typed_occurrence_graph* operations,
	struct prototype_judgement_db* judgement
) {
	if (!judgement || (judgement->claim_count != 0 && !judgement->claims) ||
		(judgement->derivation_count != 0 && !judgement->derivations)) {
		fprintf(stderr, "P0 invalid grounding storage\n");
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (!proposition) {
			judgement->claims[i].closure_rank = PROTOTYPE_INVALID_ID;
			continue;
		}
		if (proposition->occurrence_id != PROTOTYPE_INVALID_ID &&
			!operations) {
			fprintf(stderr, "P0 missing typed-occurrence graph for claim=%u\n", i);
			return -1;
		}
		judgement->claims[i].closure_rank = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->derivation_count; ++i) {
		judgement->derivations[i].closure_rank = PROTOTYPE_INVALID_ID;
	}
	int changed;
	do {
		changed = 0;
		for (uint32_t i = 0; i < (uint32_t)judgement->derivation_count; ++i) {
			struct prototype_judgement_derivation* derivation =
				&judgement->derivations[i];
			if (derivation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID ||
				derivation->conclusion_claim_id >= judgement->claim_count) {
				continue;
			}
			uint32_t max_rank = 0;
			int ready = 1;
			uint32_t source_count = 0;
			for (uint32_t j = 0; j < derivation->premise_count; ++j) {
				uint32_t source = derivation->premises[j].claim_id;
				if (source == PROTOTYPE_INVALID_ID) {
					continue;
				}
				source_count++;
				if (source == derivation->conclusion_claim_id ||
					source >= judgement->claim_count ||
					judgement->claims[source].closure_rank == PROTOTYPE_INVALID_ID) {
					ready = 0;
					break;
				}
				if (judgement->claims[source].closure_rank > max_rank) {
					max_rank = judgement->claims[source].closure_rank;
				}
			}
			if (!ready) {
				continue;
			}
			if (source_count != 0 && max_rank == PROTOTYPE_INVALID_ID - 1) {
				fprintf(stderr, "P0 closure rank overflow derivation=%u\n", i);
				return -1;
			}
			uint32_t rank = source_count == 0 ? 0 : max_rank + 1;
			if (derivation->closure_rank == PROTOTYPE_INVALID_ID ||
				rank < derivation->closure_rank) {
				derivation->closure_rank = rank;
				changed = 1;
			}
			struct prototype_judgement_claim* conclusion =
				&judgement->claims[derivation->conclusion_claim_id];
			if (conclusion->closure_rank == PROTOTYPE_INVALID_ID ||
				rank < conclusion->closure_rank) {
				conclusion->closure_rank = rank;
				changed = 1;
			}
		}
	} while (changed);
	return 0;
}

int prototype_judgement_publish_candidates(
	const struct prototype_typed_occurrence_graph* operations,
	struct prototype_judgement_db* judgement
) {
	if (!judgement || (judgement->claim_count != 0 && !judgement->claims) ||
		(judgement->derivation_count != 0 && !judgement->derivations)) {
		fprintf(stderr, "P0 invalid publication storage\n");
		return -1;
	}
	struct prototype_judgement_transaction_mark transaction;
	if (prototype_judgement_transaction_begin(
			judgement, &transaction
		) != 0) {
		return -1;
	}
	if (judgement_rebuild_claim_derivations(judgement, operations) != 0) {
		return judgement_transaction_abort(judgement, &transaction);
	}
	if (prototype_judgement_transaction_commit(
			judgement, &transaction
		) != 0) {
		return judgement_transaction_abort(judgement, &transaction);
	}
	if (prototype_judgement_recompute_closure_ranks(
			operations, judgement
		) != 0) {
		return -1;
	}
	/* Claim and Derivation IDs are append-only arena handles. Artifact
	 * publication performs its own dense relocation, so compacting this arena
	 * would invalidate HOTT actions, CwF certificates, and registered roots.
	 * Ungrounded solver slots remain distinguishable by an invalid closure rank. */
	return prototype_judgement_db_rebuild_index(judgement);
}
