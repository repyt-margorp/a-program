static int consume_solver_step(struct prototype_judgement_delta* delta) {
	if (!delta) {
		return -1;
	}
	if (!delta->solver_steps_used) {
		return 0;
	}
	if (*delta->solver_steps_used >= delta->solver_step_limit) {
		if (delta->solver_exhausted) {
			*delta->solver_exhausted = 1;
		}
		return 1;
	}
	(*delta->solver_steps_used)++;
	return 0;
}

static int term_has_tag(const struct prototype_term_db* terms, uint32_t term_id, int tag) {
	return terms && term_id < terms->term_count && terms->terms[term_id].tag == tag;
}

static int computation_effect_row_union(
	struct prototype_term_db* terms,
	const struct prototype_term_classifier_view* left,
	const struct prototype_term_classifier_view* right,
	uint32_t* p_row
) {
	if (!terms || !left || !right || !p_row ||
		left->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		right->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		left->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		right->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	return prototype_term_effect_row_union(terms, left->effect_row, right->effect_row, p_row);
}

static int computation_effect_row_is_union(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_classifier_view* result,
	const struct prototype_term_classifier_view* left,
	const struct prototype_term_classifier_view* right
) {
	if (!terms || !result || !left || !right ||
		result->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		left->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		right->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		result->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		left->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		right->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	struct prototype_effect_row_normal_form result_normal;
	struct prototype_effect_row_normal_form left_normal;
	struct prototype_effect_row_normal_form right_normal;
	if (prototype_term_effect_row_normal_form(
			terms, result->effect_row, &result_normal
		) != 0 || prototype_term_effect_row_normal_form(
			terms, left->effect_row, &left_normal
		) != 0 || prototype_term_effect_row_normal_form(
			terms, right->effect_row, &right_normal
		) != 0) {
		return 0;
	}
	for (uint32_t i = 0; i < left_normal.atom_count; ++i) {
		struct prototype_effect_row_normal_form singleton = {
			.atom_count = 1,
			.atoms = { left_normal.atoms[i] }
		};
		if (!prototype_term_effect_row_normal_form_includes(
				&result_normal, &singleton
			)) {
			return 0;
		}
	}
	for (uint32_t i = 0; i < right_normal.atom_count; ++i) {
		struct prototype_effect_row_normal_form singleton = {
			.atom_count = 1,
			.atoms = { right_normal.atoms[i] }
		};
		if (!prototype_term_effect_row_normal_form_includes(
				&result_normal, &singleton
			)) {
			return 0;
		}
	}
	(void)type_declarations;
	for (uint32_t i = 0; i < result_normal.atom_count; ++i) {
		struct prototype_effect_row_normal_form singleton = {
			.atom_count = 1,
			.atoms = { result_normal.atoms[i] }
		};
		if (!prototype_term_effect_row_normal_form_includes(
				&left_normal, &singleton
			) && !prototype_term_effect_row_normal_form_includes(
				&right_normal, &singleton
			)) {
			return 0;
		}
	}
	return 1;
}

static int effect_row_operation_latent_union(
	struct prototype_term_db* terms,
	uint32_t row,
	int operation_id,
	uint32_t* p_latent,
	int* p_found
) {
	if (!terms || !p_latent || !p_found || row >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[row];
	switch (term->tag) {
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			if (term->as.effect_row_operation.operation_id != operation_id) {
				return 0;
			}
			if (!*p_found) {
				*p_latent = term->as.effect_row_operation.latent_row;
				*p_found = 1;
				return 0;
			}
			return prototype_term_effect_row_union(
				terms,
				*p_latent,
				term->as.effect_row_operation.latent_row,
				p_latent
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			int left_status = effect_row_operation_latent_union(
				terms,
				term->as.effect_row_union.left,
				operation_id,
				p_latent,
				p_found
			);
			return left_status != 0 ? left_status :
				effect_row_operation_latent_union(
					terms,
					term->as.effect_row_union.right,
					operation_id,
					p_latent,
					p_found
				);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return 1;
		default:
			return -1;
	}
}

int prototype_judgement_specialize_fold_operation_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t input_row,
	int operation_id,
	uint32_t classifier,
	uint32_t* p_specialized
) {
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	if (!terms || !type_declarations || !declaration || !p_specialized) {
		return -1;
	}
	*p_specialized = classifier;
	int has_suspended_argument;
	if (prototype_term_effect_operation_classifier_has_suspended_argument(
			terms, classifier, &has_suspended_argument
		) != 0) {
		return -1;
	}
	if (!has_suspended_argument) {
		return 0;
	}
	uint32_t latent_row = PROTOTYPE_INVALID_ID;
	int found_latent = 0;
	int latent_status = effect_row_operation_latent_union(
		terms, input_row, operation_id, &latent_row, &found_latent
	);
	if (latent_status != 0) {
		return latent_status;
	}
	if (!found_latent) {
		return 1;
	}
	uint32_t text;
	uint32_t latent_computation;
	uint32_t latent_argument;
	if (prototype_term_make_host_type(
			terms, PROTOTYPE_HOST_TYPE_TEXT, &text
		) != 0 || prototype_term_computation_type(
			terms, latent_row, text, &latent_computation
		) != 0 || prototype_term_thunk_type(
			terms, latent_computation, &latent_argument
		) != 0 || prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			classifier,
			latent_argument,
			p_specialized
		) != 0) {
		return -1;
	}
	return 0;
}

static int instantiate_fold_clause_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t generic_domain,
	uint32_t concrete_domain,
	uint32_t classifier,
	uint32_t* p_instantiated
) {
	if (!terms || !type_declarations || !p_instantiated ||
		generic_domain >= terms->term_count || concrete_domain >= terms->term_count ||
		classifier >= terms->term_count) {
		return -1;
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			terms, type_declarations, generic_domain, concrete_domain
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		*p_instantiated = classifier;
		return 0;
	}
	/* An open effect-row domain is intentionally undecidable by conversion.
	 * Recognize its scoped metavariable structure below and instantiate it
	 * explicitly; INDETERMINATE is not a malformed classifier here. */
	if (terms->terms[generic_domain].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		terms->terms[concrete_domain].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	struct prototype_term_classifier_view generic;
	struct prototype_term_classifier_view concrete;
	if (prototype_judgement_classifier_view(
			terms,
			type_declarations,
			NULL,
			terms->terms[generic_domain].as.thunk_type.computation,
			&generic
		) != 0 || prototype_judgement_classifier_view(
			terms,
			type_declarations,
			NULL,
			terms->terms[concrete_domain].as.thunk_type.computation,
			&concrete
		) != 0 || generic.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		concrete.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		generic.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		concrete.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		generic.effect_row >= terms->term_count ||
		terms->terms[generic.effect_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR ||
		!(prototype_judgement_classifier_conversion(
			terms, type_declarations, generic.result, concrete.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return prototype_term_graph_substitute_bound_var(
		terms,
		type_declarations,
		classifier,
		terms->terms[generic.effect_row].as.effect_row_var.binding_id,
		concrete.effect_row,
		p_instantiated
	);
}

/* Residual subtraction is structural. Higher-order operation atoms are
 * removable by their outer operation identity while retaining opaque latent
 * rows on unhandled atoms. Open rows remain solver obligations. */
static int computation_fold_residual_row(
	struct prototype_term_db* terms,
	const struct prototype_term_classifier_view* input,
	const struct prototype_term_classifier_view* operation,
	uint32_t* p_residual
) {
	if (!terms || !input || !operation || !p_residual ||
		input->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		operation->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 1;
	}
	return prototype_term_effect_row_residual(
		terms, input->effect_row, operation->effect_row, p_residual
	);
}

static int term_exists(const struct prototype_term_db* terms, uint32_t term_id) {
	return terms && term_id < terms->term_count;
}

static int add_effect_row_constraint(
	struct prototype_judgement_delta* delta,
	int kind,
	uint32_t subject,
	uint32_t result_row,
	const uint32_t* operands,
	uint32_t operand_count
) {
	if (!delta || !delta->effect_row_constraints || !operands ||
		result_row == PROTOTYPE_INVALID_ID || operand_count == 0 ||
		operand_count > PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_MAX_OPERANDS) {
		return -1;
	}
	for (uint32_t i = 0; i < operand_count; ++i) {
		if (operands[i] == PROTOTYPE_INVALID_ID) {
			return -1;
		}
	}
	for (size_t i = 0; i < delta->effect_row_constraint_count; ++i) {
		const struct prototype_judgement_effect_row_constraint* constraint =
			&delta->effect_row_constraints[i];
		if (constraint->kind != kind || constraint->subject != subject ||
			constraint->result_row != result_row ||
			constraint->operand_count != operand_count) {
			continue;
		}
		int equal = 1;
		for (uint32_t j = 0; j < operand_count; ++j) {
			if (constraint->operands[j] != operands[j]) {
				equal = 0;
				break;
			}
		}
		if (equal) {
			return 0;
		}
	}
	if (reserve_slot(
			delta->effect_row_constraint_count, delta->effect_row_constraint_capacity
		) != 0) {
		return -1;
	}
	struct prototype_judgement_effect_row_constraint* constraint =
		&delta->effect_row_constraints[delta->effect_row_constraint_count++];
	*constraint = (struct prototype_judgement_effect_row_constraint){
			.kind = kind,
			.subject = subject,
			.result_row = result_row,
			.operand_count = operand_count,
			.solved = 0
		};
	memcpy(constraint->operands, operands, operand_count * sizeof(*operands));
	return 0;
}

/* Solve only equations whose rows are fully known. A symbolic equation stays
 * recorded instead of being approximated by an empty capability set. */
static int solve_effect_row_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms
) {
	if (!delta || !terms) {
		return -1;
	}
	for (size_t i = 0; i < delta->effect_row_constraint_count; ++i) {
		struct prototype_judgement_effect_row_constraint* constraint =
			&delta->effect_row_constraints[i];
		if (prototype_term_effect_row_is_closed(
				terms, constraint->result_row
			) != 1) {
			constraint->solved = 0;
			continue;
		}
		int closed = 1;
		for (uint32_t j = 0; j < constraint->operand_count; ++j) {
			if (prototype_term_effect_row_is_closed(
					terms, constraint->operands[j]
				) != 1) {
				closed = 0;
				break;
			}
		}
		if (!closed) {
			constraint->solved = 0;
			continue;
		}
		uint32_t expected;
		if (constraint->kind == PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN) {
			if (prototype_term_effect_row_empty(terms, &expected) != 0) {
				return -1;
			}
			for (uint32_t j = 0; j < constraint->operand_count; ++j) {
				if (prototype_term_effect_row_union(
						terms, expected, constraint->operands[j], &expected
					) != 0) {
					return -1;
				}
			}
		} else if (constraint->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_RESIDUAL) {
			if (constraint->operand_count != 2 ||
				prototype_term_effect_row_residual(
					terms, constraint->operands[0], constraint->operands[1], &expected
				) != 0) {
				return -1;
			}
		} else if (constraint->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION) {
			struct prototype_effect_row_normal_form subset;
			struct prototype_effect_row_normal_form superset;
			if (constraint->operand_count != 2 || prototype_term_effect_row_normal_form(
					terms, constraint->operands[0], &subset
				) != 0 || prototype_term_effect_row_normal_form(
					terms, constraint->operands[1], &superset
				) != 0 || !prototype_term_effect_row_normal_form_includes(
					&superset, &subset
				)) {
				return -1;
			}
			expected = constraint->operands[1];
		} else {
			return -1;
		}
		struct prototype_effect_row_normal_form result_normal;
		struct prototype_effect_row_normal_form expected_normal;
		if (prototype_term_effect_row_normal_form(
				terms, constraint->result_row, &result_normal
			) != 0 || prototype_term_effect_row_normal_form(
				terms, expected, &expected_normal
			) != 0 || !prototype_term_effect_row_normal_form_includes(
				&result_normal, &expected_normal
			) || !prototype_term_effect_row_normal_form_includes(
				&expected_normal, &result_normal
			)) {
			return -1;
		}
		constraint->solved = 1;
	}
	return 0;
}

static int term_is_universe_var(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_has_tag(terms, term_id, PROTOTYPE_TERM_UNIVERSE_VAR);
}

static int term_is_primitive_text(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_has_tag(terms, term_id, PROTOTYPE_TERM_PRIMITIVE_TEXT);
}

static int term_is_primitive_int(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_has_tag(terms, term_id, PROTOTYPE_TERM_PRIMITIVE_INT);
}

static int term_is_primitive_int64(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_has_tag(terms, term_id, PROTOTYPE_TERM_PRIMITIVE_INT64);
}

static int term_is_primitive_integer(const struct prototype_term_db* terms, uint32_t term_id) {
	return term_is_primitive_int(terms, term_id) ||
		term_is_primitive_int64(terms, term_id);
}

static int term_is_host_primitive(const struct prototype_term_db* terms, uint32_t term_id) {
	if (!term_exists(terms, term_id)) {
		return 0;
	}
	int host_type;
	return prototype_term_host_type_from_term_tag(terms->terms[term_id].tag, &host_type) == 0;
}

static int int_literal_fits_int32(int64_t value) {
	return value >= INT32_MIN && value <= INT32_MAX;
}

static int add_delta_relation(
	struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind
);
static int add_delta_relation_with_explicit_premises(
	struct prototype_judgement_delta* delta,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_operation_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
);
static int add_delta_relation_with_authority_and_explicit_premise_actions(
	struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	uint32_t conclusion_operation_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_operation_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	const int* premise_semantic_action_kinds,
	const uint32_t* premise_semantic_action_ids,
	uint32_t premise_count
);
static int add_complete_delta_relation(
	struct prototype_judgement_delta* delta,
	const struct prototype_judgement_proposition* candidate_relation,
	const struct prototype_judgement_derivation_candidate* candidate_proof
);
static void initialize_proof_premises(
	struct prototype_judgement_derivation_candidate* proof,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
);
static void initialize_proof_rule_parameters(
	struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* premises
);
static int add_match_motive_result(
	struct prototype_judgement_delta* delta,
	uint32_t match_term,
	uint32_t classifier
);
static void remove_match_motive_results_normalization_equal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t classifier
);
static int prototype_judgement_delta_add_conversion(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_selected_evidence* source_evidence,
	uint32_t expected
);
static int prototype_judgement_delta_expand_match_motive_with_premises(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
);
static int collect_subject_classifiers(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
);
static int collect_subject_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t subject,
	struct prototype_judgement_selected_evidence* evidence,
	uint32_t evidence_capacity,
	uint32_t* p_evidence_count
);
static int classifier_returns_owner(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t owner
);
static int classifier_list_contains_normalization_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const uint32_t* classifiers,
	uint32_t classifier_count,
	uint32_t candidate
);
static int match_motive_result_classifier(
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t classifier
);

/* Operation judgements retain TYPE_VIEW nodes. The application rule validates
 * their shared computation against the enclosed core APP. */
static int term_core_app(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	const struct prototype_term** p_app
) {
	if (!terms || !p_app || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t current = term_id;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		current = terms->terms[current].as.type_view.core;
	}
	if (current >= terms->term_count ||
		terms->terms[current].tag != PROTOTYPE_TERM_APP) {
		return -1;
	}
	*p_app = &terms->terms[current];
	return 0;
}

static int collect_subject_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t subject,
	struct prototype_judgement_selected_evidence* evidence,
	uint32_t evidence_capacity,
	uint32_t* p_evidence_count
) {
	if (!delta || !evidence || !p_evidence_count) {
		return -1;
	}
	*p_evidence_count = 0;
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_proposition* candidates =
			source == 0 ? delta->propositions :
			(delta->db ? delta->db->propositions : NULL);
		const struct prototype_judgement_derivation_candidate* derivations =
			source == 0 ? delta->derivation_candidates :
			(delta->db ? delta->db->derivation_candidates : NULL);
		size_t candidate_count = source == 0 ? delta->proposition_count :
			(delta->db ? delta->db->proposition_count : 0);
		size_t derivation_count = source == 0 ?
			delta->derivation_candidate_count :
			(delta->db ? delta->db->derivation_candidate_count : 0);
		for (size_t i = 0; i < candidate_count; ++i) {
			const struct prototype_judgement_proposition* candidate =
				&candidates[i];
			if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				candidate->subject != subject ||
				candidate_has_derivation_other_than(
					candidates,
					candidate_count,
					derivations,
					derivation_count,
					(uint32_t)i,
					PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
				) != 1) {
				continue;
			}
			struct prototype_judgement_selected_evidence current;
			selected_evidence_from_candidate(candidate, &current);
			int duplicate = 0;
			for (uint32_t j = 0; j < *p_evidence_count; ++j) {
				if (selected_evidence_equal(&evidence[j], &current)) {
					duplicate = 1;
					break;
				}
			}
			if (duplicate) {
				continue;
			}
			if (*p_evidence_count >= evidence_capacity) {
				return -1;
			}
			evidence[(*p_evidence_count)++] = current;
		}
	}
	return 0;
}

static struct prototype_term_conversion_result classifier_kernel_conversion(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
) {
	struct prototype_term_conversion_result result;
	memset(&result, 0, sizeof(result));
	result.status = PROTOTYPE_TERM_CONVERSION_INVALID;
	result.reason = PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH;
	result.profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF;
	result.left = expected;
	result.right = actual;
	result.left_observation = PROTOTYPE_INVALID_ID;
	result.right_observation = PROTOTYPE_INVALID_ID;
	result.step_limit = UINT64_MAX;
	result.graph_revision = terms ? terms->normalization_graph_revision : 0;
	if (!term_exists(terms, expected) || !term_exists(terms, actual)) {
		return result;
	}
	/* Judgemental equality is reflexive even when WHNF expansion would enter a
	 * guarded recursive motive. Normalization is evidence for distinct nodes,
	 * not a prerequisite for a node being equal to itself. */
	if (expected == actual) {
		result.status = PROTOTYPE_TERM_CONVERSION_EQUAL;
		result.reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
		result.left_observation = expected;
		result.right_observation = actual;
		return result;
	}
	if (term_is_universe_var(terms, expected) && term_is_universe_var(terms, actual)) {
		result.status = terms->terms[expected].as.universe_var.level_var ==
			terms->terms[actual].as.universe_var.level_var ?
			PROTOTYPE_TERM_CONVERSION_EQUAL : PROTOTYPE_TERM_CONVERSION_NOT_EQUAL;
		result.reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
		result.left_observation = expected;
		result.right_observation = actual;
		return result;
	}
	if (prototype_term_compare_for_conversion(
			terms,
			type_declarations,
			definitions,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			expected,
			actual,
			UINT64_MAX,
			&result
		) != 0) {
		result.status = PROTOTYPE_TERM_CONVERSION_INVALID;
		result.reason = PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH;
	}
	return result;
}

struct prototype_term_conversion_result prototype_judgement_classifier_conversion(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_conversion(
		terms,
		type_declarations,
		NULL,
		expected,
		actual
	);
}

struct prototype_term_conversion_result
prototype_judgement_classifier_conversion_with_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_conversion(
		terms,
		type_declarations,
		definitions,
		expected,
		actual
	);
}

static int kernel_conversion_profile_is_admitted(int profile) {
	return profile == PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF ||
		profile == PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF ||
		profile == PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF;
}

int prototype_judgement_kernel_conversion_goal_validate(
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	const struct prototype_kernel_conversion_goal* goal,
	int require_carrier
) {
	if (!contexts || !terms || !goal ||
		goal->context_id >= contexts->context_count ||
		goal->left_term >= terms->term_count ||
		goal->right_term >= terms->term_count ||
		!kernel_conversion_profile_is_admitted(goal->normalization_profile) ||
		(require_carrier && goal->carrier_classifier == PROTOTYPE_INVALID_ID) ||
		(goal->carrier_classifier != PROTOTYPE_INVALID_ID &&
		 goal->carrier_classifier >= terms->term_count)) {
		return -1;
	}
	return 0;
}

int prototype_judgement_kernel_conversion_goal_execute(
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_kernel_conversion_goal* goal,
	int require_carrier
) {
	if (!type_declarations ||
		prototype_judgement_kernel_conversion_goal_validate(
			contexts, terms, goal, require_carrier
		) != 0) {
		return -1;
	}
	return prototype_term_compare_for_conversion(
		terms,
		type_declarations,
		definitions,
		goal->normalization_profile,
		goal->left_term,
		goal->right_term,
		goal->step_limit,
		&goal->result
	);
}


static int classifier_conversion_decision(
	struct prototype_term_conversion_result conversion,
	int* p_equal
) {
	if (!p_equal) {
		return -1;
	}
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		*p_equal = 1;
		return 0;
	}
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		*p_equal = 0;
		return 0;
	}
	return -1;
}

int prototype_judgement_classifier_value_whnf(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_value_classifier
) {
	if (!terms || !type_declarations || !p_value_classifier ||
		classifier >= terms->term_count ||
		prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier
		) != 0 || classifier >= terms->term_count) {
		return -1;
	}
	if (terms->terms[classifier].tag == PROTOTYPE_TERM_RETURN) {
		classifier = terms->terms[classifier].as.return_term.value;
	}
	*p_value_classifier = classifier;
	return 0;
}

int prototype_judgement_classifier_reference_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
) {
	if (prototype_judgement_classifier_value_whnf(
			terms, type_declarations, expected, &expected
		) != 0 || prototype_judgement_classifier_value_whnf(
			terms, type_declarations, actual, &actual
		) != 0) {
		return 0;
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			terms, type_declarations, expected, actual
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return 0;
	}
	const struct prototype_term* left = &terms->terms[expected];
	const struct prototype_term* right = &terms->terms[actual];
	if (left->tag == PROTOTYPE_TERM_EXTERNAL_REF &&
		right->tag == PROTOTYPE_TERM_TYPE_VIEW) {
		return left->as.external_ref.name.namespace_symbol_id ==
				right->as.type_view.identity.namespace_symbol_id &&
			left->as.external_ref.name.name_symbol_id ==
				right->as.type_view.identity.name_symbol_id;
	}
	if (left->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		right->tag == PROTOTYPE_TERM_EXTERNAL_REF) {
		return left->as.type_view.identity.namespace_symbol_id ==
				right->as.external_ref.name.namespace_symbol_id &&
			left->as.type_view.identity.name_symbol_id ==
				right->as.external_ref.name.name_symbol_id;
	}
	return 0;
}

static int classifier_kernel_whnf(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t term_id,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret ||
		term_id >= terms->term_count) {
		return -1;
	}

	uint32_t evaluated;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			definitions,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			term_id,
			&evaluated
		) != 0 ||
		evaluated >= terms->term_count) {
		return -1;
	}

	*p_ret = evaluated;
	return 0;
}

static int classifier_view_with_depth(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	struct prototype_term_classifier_view* p_ret,
	uint32_t depth
) {
	uint32_t whnf;
	if (!p_ret || depth == 0 || classifier_kernel_whnf(
				terms,
			type_declarations,
			definitions,
			classifier,
				&whnf
			) != 0) {
		return -1;
	}
	/* Row schemes classify the same runtime computation as their body. The
	 * binder is only used when an application specializes a latent row. */
	for (uint32_t depth = 0;
		depth < 32 && whnf < terms->term_count &&
		terms->terms[whnf].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL;
		++depth) {
		if (classifier_kernel_whnf(
				terms,
				type_declarations,
				definitions,
				terms->terms[whnf].as.effect_row_forall.body,
				&whnf
			) != 0) {
			return -1;
		}
	}
	if (whnf >= terms->term_count ||
		terms->terms[whnf].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		return -1;
	}
	if (terms->terms[whnf].tag == PROTOTYPE_TERM_MATCH) {
		const struct prototype_term* match = &terms->terms[whnf];
		if (match->as.match.case_count == 0 || match->as.match.case_count > 64 ||
			match->as.match.first_case + match->as.match.case_count > terms->case_count) {
			return -1;
		}
		struct prototype_match_case_input result_cases[64];
		uint32_t effect_row = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			const struct prototype_match_case* match_case =
				&terms->cases[match->as.match.first_case + i];
			struct prototype_term_classifier_view branch_view;
			if (match_case->first_binder + match_case->binder_count >
					terms->case_binder_count || classifier_view_with_depth(
					terms,
					type_declarations,
					definitions,
					match_case->body,
					&branch_view,
					depth - 1
				) != 0 || branch_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				branch_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				return prototype_term_classifier_view(terms, whnf, p_ret);
			}
			result_cases[i].case_label_symbol_id =
				terms->case_label_symbols[match->as.match.first_case + i];
			result_cases[i].constructor_owner = match_case->constructor_owner;
			result_cases[i].constructor_id = match_case->constructor_id;
			result_cases[i].binders = &terms->case_binders[match_case->first_binder];
			result_cases[i].binder_count = match_case->binder_count;
			result_cases[i].body = branch_view.result;
			if (effect_row == PROTOTYPE_INVALID_ID) {
				effect_row = branch_view.effect_row;
			} else if (prototype_term_effect_row_union(
					terms, effect_row, branch_view.effect_row, &effect_row
				) != 0) {
				return -1;
			}
		}
		uint32_t result;
		if (prototype_term_match(
				terms,
				match->as.match.scrutinee,
				result_cases,
				match->as.match.case_count,
				&result
			) != 0) {
			return -1;
		}
		memset(p_ret, 0, sizeof(*p_ret));
		p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
		p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
		p_ret->effect_row = effect_row;
		p_ret->result = result;
		return 0;
	}
	return prototype_term_classifier_view(terms, whnf, p_ret);
}

int prototype_judgement_classifier_view(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	struct prototype_term_classifier_view* p_ret
) {
	return classifier_view_with_depth(
		terms, type_declarations, definitions, classifier, p_ret, 256
	);
}

int prototype_judgement_claim_category(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_operation_graph* operations,
	uint32_t claim_id,
	int* p_category
) {
	const struct prototype_judgement_claim* claim =
		prototype_judgement_claim_get(judgement, claim_id);
	if (!claim || !terms || !type_declarations || !p_category) {
		return -1;
	}
	if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE) {
		*p_category = PROTOTYPE_JUDGEMENT_CATEGORY_TYPE;
		return 0;
	}
	if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION) {
		int category;
		if (!operations || prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_id != prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id ||
			judgement_operation_expected_polarity(
				terms,
				type_declarations,
				operations,
				prototype_judgement_proposition_get(judgement, claim->proposition_id)->operation_id,
				&category
			) != 0) {
			return -1;
		}
		*p_category = category == PROTOTYPE_TERM_CATEGORY_COMPUTATION ?
			PROTOTYPE_JUDGEMENT_CATEGORY_COMPUTATION :
			PROTOTYPE_JUDGEMENT_CATEGORY_VALUE;
		return 0;
	}
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, definitions, prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier, &view
		) != 0) {
		return -1;
	}
	*p_category = view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION ?
		PROTOTYPE_JUDGEMENT_CATEGORY_COMPUTATION :
		PROTOTYPE_JUDGEMENT_CATEGORY_VALUE;
	return 0;
}

static int classifier_kernel_whnf_no_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t* p_ret
) {
	return classifier_kernel_whnf(terms, type_declarations, NULL, term_id, p_ret);
}

static int lookup_relation_classifier(
	const struct prototype_judgement_proposition* relations,
	size_t proposition_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_candidate_count,
	uint32_t subject,
	uint32_t* p_classifier,
	int include_conversion
) {
	if (!relations || !p_classifier) {
		return -1;
	}
	for (size_t i = proposition_count; i > 0; --i) {
		const struct prototype_judgement_proposition* relation = &relations[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			(include_conversion || candidate_has_derivation_other_than(
				relations,
				proposition_count,
				derivations,
				derivation_candidate_count,
				(uint32_t)(i - 1),
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			) == 1)) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	return -1;
}

static void selected_evidence_from_candidate(
	const struct prototype_judgement_proposition* candidate,
	struct prototype_judgement_selected_evidence* evidence
) {
	int authority_kind;
	uint32_t authority_id;
	candidate_claim_authority(candidate, &authority_kind, &authority_id);
	*evidence = (struct prototype_judgement_selected_evidence){
		.kind = candidate->kind,
		.authority_kind = authority_kind,
		.authority_id = authority_id,
		.context_id = candidate->context_id,
		.operation_id = authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
			candidate->operation_id : PROTOTYPE_INVALID_ID,
		.subject = candidate->subject,
		.classifier = candidate->classifier,
		.resource_usage_count = candidate->resource_usage_count,
		.resource_usage = candidate->resource_usage
	};
}

static int selected_evidence_equal(
	const struct prototype_judgement_selected_evidence* left,
	const struct prototype_judgement_selected_evidence* right
) {
	return left && right && left->kind == right->kind &&
		left->authority_kind == right->authority_kind &&
		left->authority_id == right->authority_id &&
		left->context_id == right->context_id &&
		left->operation_id == right->operation_id &&
		left->subject == right->subject &&
		left->classifier == right->classifier &&
		left->resource_usage_count == right->resource_usage_count &&
		prototype_usage_vector_equal(
			&(struct prototype_usage_vector) {
				.entries = (struct prototype_usage_entry*)left->resource_usage,
				.count = left->resource_usage_count,
				.capacity = left->resource_usage_count
			},
			&(struct prototype_usage_vector) {
				.entries = (struct prototype_usage_entry*)right->resource_usage,
				.count = right->resource_usage_count,
				.capacity = right->resource_usage_count
			}
		);
}

static int merge_exact_selected_evidence(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if ((!candidates && candidate_count != 0) || !selected || !p_found) {
		return -1;
	}
	for (size_t i = 0; i < candidate_count; ++i) {
		const struct prototype_judgement_proposition* candidate =
			&candidates[i];
		if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate->operation_id != operation_id ||
			candidate->context_id != context_id ||
			candidate->subject != subject ||
			candidate->classifier != classifier) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
		} else if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

int prototype_judgement_delta_select_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_exact_selected_evidence(
		delta->propositions,
		delta->proposition_count,
		operation_id,
		context_id,
		subject,
		classifier,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	if (delta->db) {
		status = merge_exact_selected_evidence(
			delta->db->propositions,
			delta->db->proposition_count,
			operation_id,
			context_id,
			subject,
			classifier,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
	}
	return found ? 0 : 1;
}

int prototype_judgement_select_evidence(
	const struct prototype_judgement_db* judgement,
	uint32_t operation_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!judgement || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_exact_selected_evidence(
		judgement->propositions,
		judgement->proposition_count,
		operation_id,
		context_id,
		subject,
		classifier,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	return found ? 0 : 1;
}

static int merge_selected_evidence_from_candidates(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t subject,
	int include_conversion,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if ((!candidates && candidate_count != 0) || !selected || !p_found) {
		return -1;
	}
	for (size_t i = 0; i < candidate_count; ++i) {
		const struct prototype_judgement_proposition* candidate =
			&candidates[i];
		if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate->subject != subject ||
			(!include_conversion && candidate_has_derivation_other_than(
				candidates,
				candidate_count,
				derivations,
				derivation_count,
				(uint32_t)i,
				PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			) != 1)) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
			continue;
		}
		if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

/* Returns 0 for one semantic Claim, 1 for no Claim, 2 for ambiguous Claims,
 * and -1 for malformed input. Multiple Derivations of the same Claim do not
 * make selection ambiguous. */
static int lookup_delta_selected_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t subject,
	int include_conversion,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_selected_evidence_from_candidates(
		delta->propositions,
		delta->proposition_count,
		delta->derivation_candidates,
		delta->derivation_candidate_count,
		subject,
		include_conversion,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	if (delta->db) {
		status = merge_selected_evidence_from_candidates(
			delta->db->propositions,
			delta->db->proposition_count,
			delta->db->derivation_candidates,
			delta->db->derivation_candidate_count,
			subject,
			include_conversion,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
	}
	return found ? 0 : 1;
}

static int merge_selected_evidence_with_proof_kind(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	const struct prototype_judgement_derivation_candidate* derivations,
	size_t derivation_count,
	uint32_t subject,
	int proof_kind,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if ((!candidates && candidate_count != 0) || !selected || !p_found) {
		return -1;
	}
	for (size_t i = 0; i < candidate_count; ++i) {
		const struct prototype_judgement_proposition* candidate =
			&candidates[i];
		if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate->subject != subject || candidate_has_derivation_kind(
				candidates,
				candidate_count,
				derivations,
				derivation_count,
				(uint32_t)i,
				proof_kind
			) != 1) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
		} else if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

static int lookup_delta_selected_evidence_with_proof_kind(
	const struct prototype_judgement_delta* delta,
	uint32_t subject,
	int proof_kind,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_selected_evidence_with_proof_kind(
		delta->propositions,
		delta->proposition_count,
		delta->derivation_candidates,
		delta->derivation_candidate_count,
		subject,
		proof_kind,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	if (delta->db) {
		status = merge_selected_evidence_with_proof_kind(
			delta->db->propositions,
			delta->db->proposition_count,
			delta->db->derivation_candidates,
			delta->db->derivation_candidate_count,
			subject,
			proof_kind,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
	}
	return found ? 0 : 1;
}

static int merge_selected_evidence_normalization_equal(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t subject,
	uint32_t expected,
	int exact_only,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if ((!candidates && candidate_count != 0) || !terms || !type_declarations ||
		!selected || !p_found) {
		return -1;
	}
	for (size_t i = 0; i < candidate_count; ++i) {
		const struct prototype_judgement_proposition* candidate =
			&candidates[i];
		if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate->context_id != context_id ||
			candidate->subject != subject) {
			continue;
		}
		int matches = candidate->classifier == expected;
		if (!matches && !exact_only) {
			int equal;
			if (classifier_conversion_decision(
					prototype_judgement_classifier_conversion(
						terms,
						type_declarations,
						expected,
						candidate->classifier
					),
					&equal
				) != 0) {
				return -1;
			}
			matches = equal;
		}
		if (!matches) {
			continue;
		}
		struct prototype_judgement_selected_evidence current;
		selected_evidence_from_candidate(candidate, &current);
		if (!*p_found) {
			*selected = current;
			*p_found = 1;
		} else if (!selected_evidence_equal(selected, &current)) {
			return 2;
		}
	}
	return 0;
}

static int lookup_delta_selected_evidence_normalization_equal(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !terms || !type_declarations || !selected ||
		!term_exists(terms, subject) || !term_exists(terms, expected)) {
		return -1;
	}
	for (int exact_only = 1; exact_only >= 0; --exact_only) {
		int found = 0;
		int status = merge_selected_evidence_normalization_equal(
			delta->propositions,
			delta->proposition_count,
			terms,
			type_declarations,
			delta->current_context_id,
			subject,
			expected,
			exact_only,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
		if (delta->db) {
			status = merge_selected_evidence_normalization_equal(
				delta->db->propositions,
				delta->db->proposition_count,
				terms,
				type_declarations,
				delta->current_context_id,
				subject,
				expected,
				exact_only,
				selected,
				&found
			);
			if (status != 0) {
				return status;
			}
		}
		if (found) {
			return 0;
		}
	}
	return 1;
}

static int lookup_delta_prior_evidence_normalization_equal(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !terms || !type_declarations || !selected ||
		!term_exists(terms, subject) || !term_exists(terms, expected)) {
		return -1;
	}
	for (int exact_only = 1; exact_only >= 0; --exact_only) {
		int found = 0;
		int status = merge_selected_evidence_normalization_equal(
			delta->propositions,
			delta->proposition_count,
			terms,
			type_declarations,
			delta->current_context_id,
			subject,
			expected,
			exact_only,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
		if (found) {
			return 0;
		}
	}
	if (!delta->db) {
		return 1;
	}
	for (int exact_only = 1; exact_only >= 0; --exact_only) {
		int found = 0;
		int status = merge_selected_evidence_normalization_equal(
			delta->db->propositions,
			delta->db->proposition_count,
			terms,
			type_declarations,
			delta->current_context_id,
			subject,
			expected,
			exact_only,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
		if (found) {
			return 0;
		}
	}
	return 1;
}

static int lookup_classifier(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!judgement) {
		return -1;
	}
	(void)terms;
	return lookup_relation_classifier(
		judgement->propositions,
		judgement->proposition_count,
		judgement->derivation_candidates,
		judgement->derivation_candidate_count,
		subject,
		p_classifier,
		1
	);
}

static int lookup_delta_proven_classifier(
	const struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!delta || !p_classifier) {
		return -1;
	}
	(void)terms;
	if (lookup_relation_classifier(
		delta->propositions,
		delta->proposition_count,
		delta->derivation_candidates,
		delta->derivation_candidate_count,
		subject,
		p_classifier,
		1
	) == 0) {
		return 0;
	}
	return lookup_classifier(delta->db, terms, subject, p_classifier);
}

static int merge_app_elim_argument_evidence(
	const struct prototype_judgement_proposition* relations,
	size_t proposition_count,
	const struct prototype_judgement_derivation_candidate* proofs,
	size_t derivation_candidate_count,
	uint32_t application,
	uint32_t application_classifier,
	uint32_t argument,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if (!relations || !proofs || !selected || !p_found) {
		return -1;
	}
	for (size_t i = 0; i < proposition_count; ++i) {
		const struct prototype_judgement_proposition* relation = &relations[i];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			relation->subject != application ||
			relation->classifier != application_classifier) {
			continue;
		}
		uint32_t cursor = 0;
		uint32_t derivation_id;
		while (prototype_judgement_candidate_derivation_next(
			relations,
			proposition_count,
			proofs,
			derivation_candidate_count,
			(uint32_t)i,
			&cursor,
			&derivation_id
		) == 0) {
			const struct prototype_judgement_derivation_candidate* proof =
				&proofs[derivation_id];
			if (proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM ||
				proof->premise_count != 2 ||
				proof->premises[1].proposition.subject != argument) {
				continue;
			}
			struct prototype_judgement_selected_evidence current = {
				.kind = proof->premises[1].proposition.kind,
				.authority_kind = proof->premises[1].proposition.authority_kind,
				.authority_id = proof->premises[1].proposition.authority_id,
				.context_id = proof->premises[1].proposition.context_id,
				.operation_id = proof->premises[1].proposition.authority_kind ==
					PROTOTYPE_JUDGEMENT_AUTHORITY_OPERATION ?
					proof->premises[1].proposition.operation_id : PROTOTYPE_INVALID_ID,
				.subject = proof->premises[1].proposition.subject,
				.classifier = proof->premises[1].proposition.classifier
			};
			if (!*p_found) {
				*selected = current;
				*p_found = 1;
			} else if (!selected_evidence_equal(selected, &current)) {
				return 2;
			}
		}
	}
	return 0;
}

static int lookup_delta_app_elim_argument_evidence(
	const struct prototype_judgement_delta* delta,
	uint32_t application,
	uint32_t application_classifier,
	uint32_t argument,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_app_elim_argument_evidence(
		delta->propositions,
		delta->proposition_count,
		delta->derivation_candidates,
		delta->derivation_candidate_count,
		application,
		application_classifier,
		argument,
		selected,
		&found
	);
	if (status != 0) {
		return status;
	}
	if (delta->db) {
		status = merge_app_elim_argument_evidence(
			delta->db->propositions,
			delta->db->proposition_count,
			delta->db->derivation_candidates,
			delta->db->derivation_candidate_count,
			application,
			application_classifier,
			argument,
			selected,
			&found
		);
		if (status != 0) {
			return status;
		}
	}
	return found ? 0 : 1;
}

static int pi_parts(
	const struct prototype_term_db* terms,
	uint32_t pi_term,
	uint32_t* p_domain,
	uint32_t* p_codomain_family
) {
	if (!terms || !p_domain || !p_codomain_family ||
		pi_term >= terms->term_count ||
		terms->terms[pi_term].tag != PROTOTYPE_TERM_PI ||
		terms->terms[pi_term].as.pi.codomain_family >= terms->term_count ||
		prototype_term_pure_family_lambda(
			terms, terms->terms[pi_term].as.pi.codomain_family, NULL
		) != 0) {
		return -1;
	}
	*p_domain = terms->terms[pi_term].as.pi.domain;
	*p_codomain_family = terms->terms[pi_term].as.pi.codomain_family;
	return 0;
}

int prototype_judgement_pi_parts(
	const struct prototype_term_db* terms,
	uint32_t pi_term,
	uint32_t* p_domain,
	uint32_t* p_codomain_family
) {
	return pi_parts(terms, pi_term, p_domain, p_codomain_family);
}

static int classifier_kernel_as_pi(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	uint32_t* p_pi,
	uint32_t* p_domain,
	uint32_t* p_codomain_family
) {
	if (!terms || !type_declarations || !p_domain || !p_codomain_family) {
		return -1;
	}
	uint32_t normalized_classifier;
	if (classifier_kernel_whnf(
			terms,
			type_declarations,
			definitions,
			classifier,
			&normalized_classifier
		) != 0) {
		return -1;
	}
	for (uint32_t depth = 0;
		depth < 32 && normalized_classifier < terms->term_count &&
		terms->terms[normalized_classifier].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL;
		++depth) {
		normalized_classifier =
			terms->terms[normalized_classifier].as.effect_row_forall.body;
		if (classifier_kernel_whnf(
				terms, type_declarations, definitions,
				normalized_classifier, &normalized_classifier
			) != 0) {
			return -1;
		}
	}
	if (pi_parts(terms, normalized_classifier, p_domain, p_codomain_family) != 0) {
		return 1;
	}
	if (p_pi) {
		*p_pi = normalized_classifier;
	}
	return 0;
}

/* Effect-row quantification is classifier-only. Lambda introduction proves the
 * underlying Pi; the surrounding implicit row binders are checked by the
 * elaborator and erased from the runtime term. */
static int classifier_kernel_strip_effect_row_foralls(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t classifier,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 32; ++depth) {
		if (classifier_kernel_whnf(
				terms, type_declarations, definitions, classifier, &classifier
			) != 0 || classifier >= terms->term_count) {
			return -1;
		}
		if (terms->terms[classifier].tag != PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
			*p_ret = classifier;
			return 0;
		}
		classifier = terms->terms[classifier].as.effect_row_forall.body;
	}
	return -1;
}

static int find_effect_row_argument(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual,
	uint32_t row_binder,
	uint32_t depth,
	uint32_t* p_row
) {
	if (!terms || !type_declarations || !p_row || depth > 64 ||
		expected >= terms->term_count || actual >= terms->term_count) {
		return -1;
	}
	uint32_t expected_whnf;
	uint32_t actual_whnf;
	if (classifier_kernel_whnf(
			terms, type_declarations, NULL, expected, &expected_whnf
		) != 0 || classifier_kernel_whnf(
			terms, type_declarations, NULL, actual, &actual_whnf
		) != 0 || expected_whnf >= terms->term_count ||
		actual_whnf >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* left = &terms->terms[expected_whnf];
	const struct prototype_term* right = &terms->terms[actual_whnf];
	if (left->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR &&
		left->as.effect_row_var.binding_id == row_binder) {
		*p_row = actual_whnf;
		return 1;
	}
	if (left->tag != right->tag) {
		return 0;
	}
	int status;
	switch (left->tag) {
		case PROTOTYPE_TERM_THUNK_TYPE:
			return find_effect_row_argument(
				terms, type_declarations, left->as.thunk_type.computation,
				right->as.thunk_type.computation, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			status = find_effect_row_argument(
				terms, type_declarations, left->as.computation_type.label,
				right->as.computation_type.label, row_binder, depth + 1, p_row
			);
			return status != 0 ? status : find_effect_row_argument(
				terms, type_declarations, left->as.computation_type.result,
				right->as.computation_type.result, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_PI:
			status = find_effect_row_argument(
				terms, type_declarations, left->as.pi.domain, right->as.pi.domain,
				row_binder, depth + 1, p_row
			);
			return status != 0 ? status : find_effect_row_argument(
				terms, type_declarations, left->as.pi.codomain_family,
				right->as.pi.codomain_family, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_LAMBDA:
			return find_effect_row_argument(
				terms, type_declarations, left->as.lambda.body, right->as.lambda.body,
				row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_RETURN:
			return find_effect_row_argument(
				terms, type_declarations, left->as.return_term.value,
				right->as.return_term.value, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			status = find_effect_row_argument(
				terms, type_declarations, left->as.effect_row_union.left,
				right->as.effect_row_union.left, row_binder, depth + 1, p_row
			);
			return status != 0 ? status : find_effect_row_argument(
				terms, type_declarations, left->as.effect_row_union.right,
				right->as.effect_row_union.right, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			if (left->as.effect_row_forall.binding_id == row_binder) {
				return 0;
			}
			return find_effect_row_argument(
				terms, type_declarations, left->as.effect_row_forall.body,
				right->as.effect_row_forall.body, row_binder, depth + 1, p_row
			);
		default:
			return 0;
	}
}

int prototype_judgement_specialize_effect_rows_for_argument(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function_classifier,
	uint32_t argument_classifier,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret ||
		function_classifier >= terms->term_count || argument_classifier >= terms->term_count) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 16; ++depth) {
		uint32_t normalized_function;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, function_classifier, &normalized_function
			) != 0 || normalized_function >= terms->term_count) {
			return -1;
		}
		if (terms->terms[normalized_function].tag != PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
			*p_ret = function_classifier;
			return 0;
		}
		const struct prototype_term* quantified = &terms->terms[normalized_function];
		uint32_t unquantified_body;
		uint32_t pi;
		uint32_t domain;
		uint32_t family;
		if (classifier_kernel_strip_effect_row_foralls(
				terms,
				type_declarations,
				NULL,
				quantified->as.effect_row_forall.body,
				&unquantified_body
			) != 0 || classifier_kernel_as_pi(
				terms, type_declarations, NULL, unquantified_body,
				&pi, &domain, &family
			) != 0) {
			return 1;
		}
		uint32_t expected_thunk;
		uint32_t actual_thunk;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, domain, &expected_thunk
			) != 0 || classifier_kernel_whnf(
				terms, type_declarations, NULL, argument_classifier, &actual_thunk
			) != 0 || expected_thunk >= terms->term_count ||
			actual_thunk >= terms->term_count ||
			terms->terms[expected_thunk].tag != PROTOTYPE_TERM_THUNK_TYPE ||
			terms->terms[actual_thunk].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			return 1;
		}
		uint32_t expected_computation =
			terms->terms[expected_thunk].as.thunk_type.computation;
		uint32_t actual_computation =
			terms->terms[actual_thunk].as.thunk_type.computation;
		uint32_t expected_computation_whnf;
		uint32_t actual_computation_whnf;
		if (classifier_kernel_whnf(
				terms,
				type_declarations,
				NULL,
				expected_computation,
				&expected_computation_whnf
			) != 0 ||
			classifier_kernel_whnf(
				terms,
				type_declarations,
				NULL,
				actual_computation,
				&actual_computation_whnf
			) != 0 ||
			expected_computation_whnf >= terms->term_count ||
			actual_computation_whnf >= terms->term_count) {
			return -1;
		}
		if (terms->terms[expected_computation_whnf].tag ==
				PROTOTYPE_TERM_COMPUTATION_TYPE &&
			terms->terms[actual_computation_whnf].tag ==
				PROTOTYPE_TERM_COMPUTATION_TYPE) {
			uint32_t row;
			int row_status = find_effect_row_argument(
				terms,
				type_declarations,
				expected_thunk,
				actual_thunk,
				quantified->as.effect_row_forall.binding_id,
				0,
				&row
			);
			if (row_status <= 0 || prototype_term_graph_substitute_bound_var(
					terms,
					type_declarations,
					quantified->as.effect_row_forall.body,
					quantified->as.effect_row_forall.binding_id,
					row,
					&function_classifier
				) != 0) {
				return -1;
			}
			continue;
		}
		uint32_t expected_pi;
		uint32_t actual_pi;
		uint32_t expected_domain;
		uint32_t expected_family;
		uint32_t actual_domain;
		uint32_t actual_family;
		if (classifier_kernel_as_pi(
				terms,
				type_declarations,
				NULL,
				expected_computation,
				&expected_pi,
				&expected_domain,
				&expected_family
			) != 0 || classifier_kernel_as_pi(
				terms,
				type_declarations,
				NULL,
				actual_computation,
				&actual_pi,
				&actual_domain,
				&actual_family
			) != 0 || !prototype_judgement_classifier_compatible(
				terms, type_declarations, expected_domain, actual_domain
			)) {
			return -1;
		}
		uint32_t expected_binder;
		uint32_t expected_result;
		uint32_t actual_binder;
		uint32_t actual_result;
		if (prototype_term_pure_family_parts(
				terms, expected_family, &expected_binder, &expected_result
			) != 0 || prototype_term_pure_family_parts(
				terms, actual_family, &actual_binder, &actual_result
			) != 0 || expected_result >= terms->term_count ||
			actual_result >= terms->term_count ||
			terms->terms[expected_result].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
			terms->terms[actual_result].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return 1;
		}
		uint32_t row = terms->terms[actual_result].as.computation_type.label;
		if (row >= terms->term_count ||
			prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				quantified->as.effect_row_forall.body,
				quantified->as.effect_row_forall.binding_id,
				row,
				&function_classifier
			) != 0) {
			return -1;
		}
		(void)pi;
		(void)family;
		(void)expected_pi;
		(void)actual_pi;
		(void)expected_binder;
		(void)actual_binder;
	}
	return -1;
}

static int instantiate_pure_family_in_context(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t domain,
	uint32_t family,
	uint32_t argument,
	uint32_t argument_classifier,
	uint32_t* p_result
) {
	const struct prototype_substitution* prefix =
		prototype_substitution_get(substitutions, prefix_substitution);
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_result || !prefix ||
		domain >= terms->term_count || family >= terms->term_count ||
		argument >= terms->term_count ||
		argument_classifier >= terms->term_count) {
		return -1;
	}
	uint32_t binding_id;
	uint32_t body;
	if (prototype_term_pure_family_parts(
			terms,
			family,
			&binding_id,
			&body
		) != 0) {
		return -1;
	}
	uint32_t family_context;
	if (prototype_context_extend(
			contexts,
			prefix->target_context,
			binding_id,
			domain,
			PROTOTYPE_INVALID_ID,
			&family_context
		) != 0) {
		return -1;
	}
	const struct prototype_context* context =
		prototype_context_get(contexts, family_context);
	if (!context) {
		return -1;
	}
	if (context->binding_id != binding_id &&
		prototype_term_contains_free_binding(terms, body, binding_id)) {
		uint32_t canonical_var;
		if (prototype_term_var(
				terms, context->binding_id, &canonical_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				body,
				binding_id,
				canonical_var,
				&body
			) != 0) {
			return -1;
		}
	}
	uint32_t section;
	if (prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			prefix_substitution,
			family_context,
			argument,
			argument_classifier,
			&section
		) != 0) {
		return -1;
	}
	return prototype_term_reindex(
		terms,
		type_declarations,
		contexts,
		substitutions,
		body,
		section,
		p_result
	);
}

static int pi_codomain_after_argument_in_context(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t pi_term,
	uint32_t argument,
	uint32_t argument_classifier,
	uint32_t* p_result
) {
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t identity;
	if (!contexts || !substitutions ||
		!terms || !type_declarations || !p_result ||
		argument >= terms->term_count ||
		pi_parts(terms, pi_term, &domain, &codomain_family) != 0) {
		return -1;
	}
	(void)domain;
	if (prototype_substitution_identity(
			substitutions,
			contexts,
			context_id,
			&identity
		) != 0) {
		return -1;
	}
	(void)domain;
	return instantiate_pure_family_in_context(
		contexts,
		substitutions,
		terms,
		type_declarations,
		identity,
		argument_classifier,
		codomain_family,
		argument,
		argument_classifier,
		p_result
	);
}

static int pi_codomain_at_binder_in_context(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t premise_context_id,
	uint32_t pi_term,
	uint32_t binder_var,
	uint32_t binder_classifier,
	uint32_t binder_proof_id,
	uint32_t* p_result
) {
	(void)binder_proof_id;
	uint32_t domain;
	uint32_t family;
	uint32_t projection;
	if (!delta || !delta->contexts || !delta->substitutions ||
		!prototype_context_get(delta->contexts, premise_context_id) ||
		pi_parts(terms, pi_term, &domain, &family) != 0 ||
		prototype_substitution_projection(
			delta->substitutions,
			delta->contexts,
			premise_context_id,
			&projection
		) != 0) {
		return -1;
	}
	return instantiate_pure_family_in_context(
		delta->contexts,
		delta->substitutions,
		terms,
		type_declarations,
		projection,
		binder_classifier,
		family,
		binder_var,
		binder_classifier,
		p_result
	);
}

static int pi_codomain_at_fresh_binder(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t base_context_id,
	uint32_t pi_term,
	uint32_t binder_var,
	uint32_t binder_classifier,
	uint32_t binder_proof_id,
	uint32_t* p_result
) {
	(void)binder_proof_id;
	if (!delta || !delta->contexts || binder_var >= terms->term_count ||
		terms->terms[binder_var].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t premise_context;
	if (prototype_context_extend(
			delta->contexts,
			base_context_id,
			terms->terms[binder_var].as.var.binding_id,
			binder_classifier,
			PROTOTYPE_INVALID_ID,
			&premise_context
		) != 0) {
		return -1;
	}
	return pi_codomain_at_binder_in_context(
		delta,
		terms,
		type_declarations,
		premise_context,
		pi_term,
		binder_var,
		binder_classifier,
		binder_proof_id,
		p_result
	);
}

static int classifier_kernel_compatible_at_depth(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual,
	uint32_t depth
) {
	if (!terms || !type_declarations ||
		!term_exists(terms, expected) ||
		!term_exists(terms, actual) ||
		depth > 32) {
		return 0;
	}
	uint32_t normalized_expected = expected;
	uint32_t normalized_actual = actual;
	if (classifier_kernel_whnf(
			terms,
			type_declarations,
			definitions,
			expected,
			&normalized_expected
		) != 0 ||
		classifier_kernel_whnf(
			terms,
			type_declarations,
			definitions,
			actual,
			&normalized_actual
		) != 0) {
		return 0;
	}
	expected = normalized_expected;
	actual = normalized_actual;
	struct prototype_term_conversion_result conversion = classifier_kernel_conversion(
			terms,
			type_declarations,
			definitions,
			expected,
			actual
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return 0;
	}
	/* Artifact lowering may expose a qualified type name before relocation and
	 * the corresponding TYPE_VIEW after relocation. This is name resolution for
	 * one declaration identity, not structural equality between declarations. */
	const struct prototype_term* expected_term = &terms->terms[expected];
	const struct prototype_term* actual_term = &terms->terms[actual];
	if (expected_term->tag == PROTOTYPE_TERM_EXTERNAL_REF &&
		actual_term->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		expected_term->as.external_ref.name.namespace_symbol_id ==
			actual_term->as.type_view.identity.namespace_symbol_id &&
		expected_term->as.external_ref.name.name_symbol_id ==
			actual_term->as.type_view.identity.name_symbol_id) {
		return 1;
	}
	if (actual_term->tag == PROTOTYPE_TERM_EXTERNAL_REF &&
		expected_term->tag == PROTOTYPE_TERM_TYPE_VIEW &&
		actual_term->as.external_ref.name.namespace_symbol_id ==
			expected_term->as.type_view.identity.namespace_symbol_id &&
		actual_term->as.external_ref.name.name_symbol_id ==
			expected_term->as.type_view.identity.name_symbol_id) {
		return 1;
	}
	/* Universe variables are not judgementally equal unless their level ids
	 * match. They are nevertheless conversion-compatible here: the compiler
	 * records the corresponding level inequality and UniverseDB checks it after
	 * linking the complete graph. */
	if (term_is_universe_var(terms, expected) &&
		term_is_universe_var(terms, actual)) {
		return 1;
	}
	if (terms->terms[expected].tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		terms->terms[actual].tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		const struct prototype_term* expected_computation = &terms->terms[expected];
		const struct prototype_term* actual_computation = &terms->terms[actual];
		if (expected_computation->as.computation_type.label >= terms->term_count ||
			actual_computation->as.computation_type.label >= terms->term_count) {
			return 0;
		}
		if (!(classifier_kernel_conversion(
				terms,
				type_declarations,
				definitions,
				expected_computation->as.computation_type.label,
				actual_computation->as.computation_type.label
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 0;
		}
		return classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			definitions,
			expected_computation->as.computation_type.result,
			actual_computation->as.computation_type.result,
			depth + 1
		);
	}
	if (terms->terms[expected].tag == PROTOTYPE_TERM_THUNK_TYPE &&
		terms->terms[actual].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		return classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			definitions,
			terms->terms[expected].as.thunk_type.computation,
			terms->terms[actual].as.thunk_type.computation,
			depth + 1
		);
	}

	uint32_t expected_domain;
	uint32_t expected_family;
	uint32_t actual_domain;
	uint32_t actual_family;
	if (pi_parts(terms, expected, &expected_domain, &expected_family) != 0 ||
		pi_parts(terms, actual, &actual_domain, &actual_family) != 0) {
		return 0;
	}
	if (!classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		definitions,
		expected_domain,
		actual_domain,
		depth + 1
	)) {
		return 0;
	}

	uint32_t expected_binder;
	uint32_t expected_family_body;
	uint32_t actual_binder;
	uint32_t actual_family_body;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[expected].as.pi.codomain_family,
			&expected_binder,
			&expected_family_body
		) != 0 || prototype_term_pure_family_parts(
			terms,
			terms->terms[actual].as.pi.codomain_family,
			&actual_binder,
			&actual_family_body
		) != 0) {
		return 0;
	}
	uint32_t comparison_binder = prototype_term_new_binding(terms);
	uint32_t comparison_var;
	uint32_t expected_body;
	uint32_t actual_body;
	if (comparison_binder == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (prototype_term_var(
		terms,
		comparison_binder,
		&comparison_var
	) != 0) {
		return 0;
	}
	if (prototype_term_graph_substitute_bound_var(
		terms,
		type_declarations,
		expected_family_body,
		expected_binder,
		comparison_var,
		&expected_body
	) != 0) {
		return 0;
	}
	if (prototype_term_graph_substitute_bound_var(
		terms,
		type_declarations,
		actual_family_body,
		actual_binder,
		comparison_var,
		&actual_body
	) != 0) {
		return 0;
	}
	return classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		definitions,
		expected_body,
		actual_body,
		depth + 1
	);
}

static int classifier_kernel_compatible_no_definitions_at_depth(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual,
	uint32_t depth
) {
	return classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		NULL,
		expected,
		actual,
		depth
	);
}

struct expected_effect_row_solution {
	uint32_t binding_id;
	uint32_t row;
};

struct expected_effect_row_solver {
	struct prototype_term_db* terms;
	struct prototype_type_declaration_db* type_declarations;
	const struct prototype_term_definition_env* definitions;
	struct expected_effect_row_solution solutions[64];
	uint32_t solution_count;
};

static int expected_effect_row_bind(
	struct expected_effect_row_solver* solver,
	uint32_t binding_id,
	uint32_t row
) {
	if (!solver || row >= solver->terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < solver->solution_count; ++i) {
		if (solver->solutions[i].binding_id != binding_id) {
			continue;
		}
		return (classifier_kernel_conversion(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			solver->solutions[i].row,
			row
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ? 0 : 1;
	}
	if (solver->solution_count >= 64) {
		return -1;
	}
	solver->solutions[solver->solution_count].binding_id = binding_id;
	solver->solutions[solver->solution_count].row = row;
	solver->solution_count += 1;
	return 0;
}

static int expected_effect_row_solve_row(
	struct expected_effect_row_solver* solver,
	uint32_t expected,
	uint32_t actual
) {
	if (!solver || expected >= solver->terms->term_count ||
		actual >= solver->terms->term_count) {
		return -1;
	}
	const struct prototype_term* expected_term = &solver->terms->terms[expected];
	if (expected_term->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return expected_effect_row_bind(
			solver, expected_term->as.effect_row_var.binding_id, actual
		);
	}
	return (classifier_kernel_conversion(
		solver->terms,
		solver->type_declarations,
		solver->definitions,
		expected,
		actual
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL) ? 0 : 1;
}

static int expected_effect_row_solve_classifier(
	struct expected_effect_row_solver* solver,
	uint32_t expected,
	uint32_t actual,
	uint32_t depth
) {
	if (!solver || expected >= solver->terms->term_count ||
		actual >= solver->terms->term_count || depth > 32) {
		return -1;
	}
	uint32_t normalized_expected;
	uint32_t normalized_actual;
	if (classifier_kernel_whnf(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			expected,
			&normalized_expected
		) != 0 || classifier_kernel_whnf(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			actual,
			&normalized_actual
		) != 0) {
		return -1;
	}
	expected = normalized_expected;
	actual = normalized_actual;
	const struct prototype_term* expected_term = &solver->terms->terms[expected];
	const struct prototype_term* actual_term = &solver->terms->terms[actual];
	if (expected_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		actual_term->tag == PROTOTYPE_TERM_MATCH) {
		struct prototype_term expected_computation = *expected_term;
		struct prototype_term actual_match = *actual_term;
		uint32_t expected_result;
		if (classifier_kernel_whnf(
				solver->terms,
				solver->type_declarations,
				solver->definitions,
				expected_computation.as.computation_type.result,
				&expected_result
			) != 0 || expected_result >= solver->terms->term_count ||
			solver->terms->terms[expected_result].tag != PROTOTYPE_TERM_MATCH) {
			return 1;
		}
		struct prototype_term expected_match = solver->terms->terms[expected_result];
		if (expected_match.as.match.case_count != actual_match.as.match.case_count ||
			!(classifier_kernel_conversion(
				solver->terms,
				solver->type_declarations,
				solver->definitions,
				expected_match.as.match.scrutinee,
				actual_match.as.match.scrutinee
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
			return 1;
		}
		for (uint32_t i = 0; i < expected_match.as.match.case_count; ++i) {
			uint32_t expected_case_id = expected_match.as.match.first_case + i;
			uint32_t actual_case_id = actual_match.as.match.first_case + i;
			if (expected_case_id >= solver->terms->case_count ||
				actual_case_id >= solver->terms->case_count) {
				return -1;
			}
			struct prototype_match_case expected_case =
				solver->terms->cases[expected_case_id];
			struct prototype_match_case actual_case =
				solver->terms->cases[actual_case_id];
			int use_labels =
				expected_case.constructor_owner == PROTOTYPE_INVALID_ID ||
				actual_case.constructor_owner == PROTOTYPE_INVALID_ID ||
				expected_case.constructor_id == PROTOTYPE_INVALID_ID ||
				actual_case.constructor_id == PROTOTYPE_INVALID_ID;
			int labels_equal = use_labels &&
				solver->terms->case_label_symbols[expected_case_id] >= 0 &&
				solver->terms->case_label_symbols[expected_case_id] ==
					solver->terms->case_label_symbols[actual_case_id];
			if ((use_labels && !labels_equal) ||
				(!use_labels &&
					expected_case.constructor_id != actual_case.constructor_id) ||
				expected_case.binder_count != actual_case.binder_count) {
				return 1;
			}
			if (!use_labels &&
				(expected_case.constructor_owner != PROTOTYPE_INVALID_ID ||
				actual_case.constructor_owner != PROTOTYPE_INVALID_ID)) {
				if (expected_case.constructor_owner == PROTOTYPE_INVALID_ID ||
					actual_case.constructor_owner == PROTOTYPE_INVALID_ID ||
					!(classifier_kernel_conversion(
						solver->terms,
						solver->type_declarations,
						solver->definitions,
						expected_case.constructor_owner,
						actual_case.constructor_owner
					).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
					return 1;
				}
			}

			uint32_t expected_body = expected_case.body;
			uint32_t actual_body = actual_case.body;
			for (uint32_t j = 0; j < expected_case.binder_count; ++j) {
				uint32_t expected_binder_index = expected_case.first_binder + j;
				uint32_t actual_binder_index = actual_case.first_binder + j;
				uint32_t comparison_binder = prototype_term_new_binding(solver->terms);
				uint32_t comparison_var;
				if (expected_binder_index >= solver->terms->case_binder_count ||
					actual_binder_index >= solver->terms->case_binder_count ||
					comparison_binder == PROTOTYPE_INVALID_ID ||
					prototype_term_var(
						solver->terms, comparison_binder, &comparison_var
					) != 0 || prototype_term_graph_substitute_bound_var(
						solver->terms,
						solver->type_declarations,
						expected_body,
						solver->terms->case_binders[expected_binder_index].binding_id,
						comparison_var,
						&expected_body
					) != 0 || prototype_term_graph_substitute_bound_var(
						solver->terms,
						solver->type_declarations,
						actual_body,
						solver->terms->case_binders[actual_binder_index].binding_id,
						comparison_var,
						&actual_body
					) != 0) {
					return -1;
				}
			}

			uint32_t actual_branch;
			if (classifier_kernel_whnf(
					solver->terms,
					solver->type_declarations,
					solver->definitions,
					actual_body,
					&actual_branch
				) != 0 || actual_branch >= solver->terms->term_count ||
				solver->terms->terms[actual_branch].tag !=
					PROTOTYPE_TERM_COMPUTATION_TYPE) {
				return 1;
			}
			struct prototype_term branch = solver->terms->terms[actual_branch];
			int status = expected_effect_row_solve_row(
				solver,
				expected_computation.as.computation_type.label,
				branch.as.computation_type.label
			);
			if (status != 0) {
				return status;
			}
			status = expected_effect_row_solve_classifier(
				solver,
				expected_body,
				branch.as.computation_type.result,
				depth + 1
			);
			if (status != 0) {
				return status;
			}
		}
		return 0;
	}
	if (expected_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		actual_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		int row_status = expected_effect_row_solve_row(
			solver,
			expected_term->as.computation_type.label,
			actual_term->as.computation_type.label
		);
		if (row_status != 0) {
			return row_status;
		}
		return expected_effect_row_solve_classifier(
			solver,
			expected_term->as.computation_type.result,
			actual_term->as.computation_type.result,
			depth + 1
		);
	}
	if (expected_term->tag == PROTOTYPE_TERM_THUNK_TYPE &&
		actual_term->tag == PROTOTYPE_TERM_THUNK_TYPE) {
		return expected_effect_row_solve_classifier(
			solver,
			expected_term->as.thunk_type.computation,
			actual_term->as.thunk_type.computation,
			depth + 1
		);
	}

	uint32_t expected_domain;
	uint32_t expected_family;
	uint32_t actual_domain;
	uint32_t actual_family;
	if (pi_parts(
			solver->terms, expected, &expected_domain, &expected_family
		) == 0 && pi_parts(
			solver->terms, actual, &actual_domain, &actual_family
		) == 0) {
		int domain_status = expected_effect_row_solve_classifier(
			solver, expected_domain, actual_domain, depth + 1
		);
		if (domain_status != 0) {
			return domain_status;
		}
		uint32_t expected_binder;
		uint32_t expected_body;
		uint32_t actual_binder;
		uint32_t actual_body;
		if (prototype_term_pure_family_parts(
				solver->terms,
				expected_family,
				&expected_binder,
				&expected_body
			) != 0 || prototype_term_pure_family_parts(
				solver->terms,
				actual_family,
				&actual_binder,
				&actual_body
			) != 0) {
			return -1;
		}
		uint32_t comparison_binder = prototype_term_new_binding(solver->terms);
		uint32_t comparison_var;
		uint32_t instantiated_expected;
		uint32_t instantiated_actual;
		if (comparison_binder == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				solver->terms, comparison_binder, &comparison_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				solver->terms,
				solver->type_declarations,
				expected_body,
				expected_binder,
				comparison_var,
				&instantiated_expected
			) != 0 || prototype_term_graph_substitute_bound_var(
				solver->terms,
				solver->type_declarations,
				actual_body,
				actual_binder,
				comparison_var,
				&instantiated_actual
			) != 0) {
			return -1;
		}
		return expected_effect_row_solve_classifier(
			solver,
			instantiated_expected,
			instantiated_actual,
			depth + 1
		);
	}

	return classifier_kernel_compatible_at_depth(
		solver->terms,
		solver->type_declarations,
		solver->definitions,
		expected,
		actual,
		depth
	) ? 0 : 1;
}

int prototype_judgement_solve_expected_effect_rows(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual,
	uint32_t* p_solved_expected
) {
	if (!terms || !type_declarations || !p_solved_expected ||
		expected >= terms->term_count || actual >= terms->term_count) {
		return -1;
	}
	struct expected_effect_row_solver solver;
	memset(&solver, 0, sizeof(solver));
	solver.terms = terms;
	solver.type_declarations = type_declarations;
	solver.definitions = definitions;
	int status = expected_effect_row_solve_classifier(
		&solver, expected, actual, 0
	);
	if (status != 0) {
		return status;
	}
	uint32_t solved = expected;
	for (uint32_t i = 0; i < solver.solution_count; ++i) {
		if (prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				solved,
				solver.solutions[i].binding_id,
				solver.solutions[i].row,
				&solved
			) != 0) {
			return -1;
		}
	}
	if (!classifier_kernel_compatible_at_depth(
			terms,
			type_declarations,
			definitions,
			solved,
			actual,
			0
		)) {
		return 1;
	}
	*p_solved_expected = solved;
	return 0;
}

int prototype_judgement_classifier_compatible(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_compatible_no_definitions_at_depth(terms, type_declarations, expected, actual, 0);
}

int prototype_judgement_classifier_compatible_with_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_compatible_at_depth(
		terms,
		type_declarations,
		definitions,
		expected,
		actual,
		0
	);
}

static int owner_parameter_argument(
