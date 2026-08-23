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
		) != 0 || prototype_term_total_computation_type(
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
	uint32_t computation_constraint_id,
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
		if (constraint->kind != kind ||
			constraint->computation_constraint_id != computation_constraint_id ||
			constraint->subject != subject ||
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
				.computation_constraint_id = computation_constraint_id,
				.subject = subject,
				.result_row = result_row,
				.operand_count = operand_count
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
	const uint32_t* premise_occurrence_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	const struct prototype_judgement_selected_evidence* premise_evidence,
	uint32_t premise_count
);
static int add_delta_relation_with_authority_and_explicit_premise_actions(
	struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	uint32_t conclusion_occurrence_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_occurrence_ids,
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
	uint32_t default_context_id,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
);
static void initialize_proof_rule_parameters(
	struct prototype_judgement_derivation_candidate* proof,
	struct prototype_judgement_candidate_premise* premises,
	struct prototype_judgement_proposition* premise_propositions
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
	uint32_t current;
	if (prototype_term_core_projection(terms, term_id, &current) != 0 ||
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
		classifier >= terms->term_count) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 64; ++depth) {
		uint32_t normalized;
		if (prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				classifier,
				&normalized
			) != 0 || normalized >= terms->term_count) {
			return -1;
		}
		classifier = normalized;
		if (terms->terms[classifier].tag != PROTOTYPE_TERM_RETURN) {
			uint32_t type_id;
			uint32_t arguments[16];
			uint32_t argument_count;
			if (prototype_term_type_instance_info(
					terms, classifier, &type_id, arguments, &argument_count
				) == 0 && prototype_term_type_instance_make(
					terms, type_declarations, type_id, arguments,
					argument_count, &classifier
				) != 0) {
				return -1;
			}
			*p_value_classifier = classifier;
			return 0;
		}
		classifier = terms->terms[classifier].as.return_term.value;
		if (classifier >= terms->term_count) {
			return -1;
		}
	}
	return -1;
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
	const struct prototype_typed_occurrence_graph* operations,
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
	if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE) {
		int category;
		if (!operations || prototype_judgement_proposition_get(judgement, claim->proposition_id)->authority_id != prototype_judgement_proposition_get(judgement, claim->proposition_id)->occurrence_id ||
			judgement_operation_expected_polarity(
				terms,
				type_declarations,
				operations,
				prototype_judgement_proposition_get(judgement, claim->proposition_id)->occurrence_id,
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
		.occurrence_id = authority_kind ==
			PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
			candidate->occurrence_id : PROTOTYPE_INVALID_ID,
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
		left->occurrence_id == right->occurrence_id &&
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
			candidate->occurrence_id != operation_id ||
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

static int merge_authority_selected_evidence(
	const struct prototype_judgement_proposition* candidates,
	size_t candidate_count,
	int authority_kind,
	uint32_t authority_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected,
	int* p_found
) {
	if ((!candidates && candidate_count != 0) || !selected || !p_found ||
		authority_kind == PROTOTYPE_JUDGEMENT_AUTHORITY_INVALID ||
		authority_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (size_t i = 0; i < candidate_count; ++i) {
		const struct prototype_judgement_proposition* candidate = &candidates[i];
		int candidate_authority_kind;
		uint32_t candidate_authority_id;
		candidate_claim_authority(
			candidate, &candidate_authority_kind, &candidate_authority_id
		);
		if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			candidate_authority_kind != authority_kind ||
			candidate_authority_id != authority_id ||
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

int prototype_judgement_delta_select_evidence_by_authority(
	const struct prototype_judgement_delta* delta,
	int authority_kind,
	uint32_t authority_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!delta || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_authority_selected_evidence(
		delta->propositions,
		delta->proposition_count,
		authority_kind,
		authority_id,
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
		status = merge_authority_selected_evidence(
			delta->db->propositions,
			delta->db->proposition_count,
			authority_kind,
			authority_id,
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

int prototype_judgement_select_evidence_by_authority(
	const struct prototype_judgement_db* judgement,
	int authority_kind,
	uint32_t authority_id,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier,
	struct prototype_judgement_selected_evidence* selected
) {
	if (!judgement || !selected) {
		return -1;
	}
	int found = 0;
	int status = merge_authority_selected_evidence(
		judgement->propositions,
		judgement->proposition_count,
		authority_kind,
		authority_id,
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
				proof->premises[1].proposition->subject != argument) {
				continue;
			}
			struct prototype_judgement_selected_evidence current = {
				.kind = proof->premises[1].proposition->kind,
				.authority_kind = proof->premises[1].proposition->authority_kind,
				.authority_id = proof->premises[1].proposition->authority_id,
				.context_id = proof->premises[1].proposition->context_id,
				.occurrence_id = proof->premises[1].proposition->authority_kind ==
					PROTOTYPE_JUDGEMENT_AUTHORITY_TYPED_OCCURRENCE ?
					proof->premises[1].proposition->occurrence_id : PROTOTYPE_INVALID_ID,
				.subject = proof->premises[1].proposition->subject,
				.classifier = proof->premises[1].proposition->classifier
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
	if (left->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		right->tag == PROTOTYPE_TERM_PI) {
		uint32_t expected_value;
		if (classifier_kernel_whnf(
				terms,
				type_declarations,
				NULL,
				left->as.computation_type.result,
				&expected_value
			) != 0 || expected_value >= terms->term_count ||
			terms->terms[expected_value].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			return 0;
		}
		uint32_t empty_row;
		if (prototype_term_effect_row_empty(terms, &empty_row) != 0) {
			return -1;
		}
		int row_status = find_effect_row_argument(
			terms,
			type_declarations,
			left->as.computation_type.label,
			empty_row,
			row_binder,
			depth + 1,
			p_row
		);
		if (row_status != 0) {
			return row_status;
		}
		return find_effect_row_argument(
			terms,
			type_declarations,
			terms->terms[expected_value].as.thunk_type.computation,
			actual_whnf,
			row_binder,
			depth + 1,
			p_row
		);
	}
	if (left->tag == PROTOTYPE_TERM_PI &&
		right->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		uint32_t actual_value;
		if (classifier_kernel_whnf(
				terms,
				type_declarations,
				NULL,
				right->as.computation_type.result,
				&actual_value
			) != 0 || actual_value >= terms->term_count ||
			terms->terms[actual_value].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			return 0;
		}
		*p_row = right->as.computation_type.label;
		return 1;
	}
	if (left->tag != right->tag) {
		return 0;
	}
	int status;
	switch (left->tag) {
		case PROTOTYPE_TERM_APP:
			status = find_effect_row_argument(
				terms, type_declarations, left->as.app.function,
				right->as.app.function, row_binder, depth + 1, p_row
			);
			return status != 0 ? status : find_effect_row_argument(
				terms, type_declarations, left->as.app.argument,
				right->as.app.argument, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return find_effect_row_argument(
				terms, type_declarations, left->as.thunk_type.computation,
				right->as.thunk_type.computation, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_THUNK:
			return find_effect_row_argument(
				terms, type_declarations, left->as.thunk.computation,
				right->as.thunk.computation, row_binder, depth + 1, p_row
			);
		case PROTOTYPE_TERM_FORCE:
			return find_effect_row_argument(
				terms, type_declarations, left->as.force.value,
				right->as.force.value, row_binder, depth + 1, p_row
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

int prototype_judgement_expose_callable_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret ||
		classifier >= terms->term_count) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 16; ++depth) {
		struct prototype_term_classifier_view view;
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, classifier, &view
			) == 0 && view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			classifier = view.result;
			continue;
		}
		uint32_t whnf;
		if (prototype_judgement_classifier_value_whnf(
				terms, type_declarations, classifier, &whnf
			) != 0 || whnf >= terms->term_count) {
			return -1;
		}
		if (terms->terms[whnf].tag == PROTOTYPE_TERM_THUNK_TYPE) {
			classifier = terms->terms[whnf].as.thunk_type.computation;
			continue;
		}
		/* EFFECT_ROW_FORALL is part of the callable classifier. APP must retain
		 * it until the argument classifier determines its instantiation. Shape-only
		 * clients may inspect the quantified body explicitly without publishing it
		 * as the classifier of an application. */
		*p_ret = classifier;
		return 0;
	}
	return -1;
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
		uint32_t solved_domain;
		int solve_status = prototype_judgement_solve_expected_effect_rows(
			terms,
			type_declarations,
			NULL,
			domain,
			argument_classifier,
			&solved_domain
		);
		if (solve_status != 0) {
			if (getenv("A_PROGRAM_FORMATION_TRACE")) {
				fprintf(
					stderr,
					"effect forall domain solve status=%d binder=%u domain=%u argument=%u\n",
					solve_status,
					quantified->as.effect_row_forall.binding_id,
					domain,
					argument_classifier
				);
			}
			return solve_status;
		}
		uint32_t row;
		int row_status = find_effect_row_argument(
			terms,
			type_declarations,
			domain,
			solved_domain,
			quantified->as.effect_row_forall.binding_id,
			0,
			&row
		);
		if (row_status < 0) {
			return -1;
		}
		if (row_status == 0) {
			/* The quantified row may occur only in the codomain.  In that case the
			 * argument cannot constrain it, so instantiate the least row. Any fixed
			 * effects remain in the codomain's row union. */
			if (prototype_term_effect_row_empty(terms, &row) != 0) {
				return -1;
			}
		}
		/* Solving a latent row from a provisionally classified argument may
		 * produce E = E union F.  Effect rows form an idempotent union semilattice,
		 * so its least solution is F.  Remove the quantified variable itself
		 * before substituting; retaining it would turn the local forall binder into
		 * a free row variable at the call site. */
		struct prototype_effect_row_normal_form normal;
		if (prototype_term_effect_row_normal_form(terms, row, &normal) != 0) {
			return -1;
		}
		uint32_t retained = 0;
		for (uint32_t i = 0; i < normal.atom_count; ++i) {
			uint32_t atom = normal.atoms[i];
			if (atom < terms->term_count &&
				terms->terms[atom].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR &&
				terms->terms[atom].as.effect_row_var.binding_id ==
					quantified->as.effect_row_forall.binding_id) {
				continue;
			}
			normal.atoms[retained++] = atom;
		}
		normal.atom_count = retained;
		if (prototype_term_effect_row_materialize_normal_form(
				terms, &normal, &row
			) != 0) {
			return -1;
		}
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
	}
	return -1;
}

int prototype_judgement_dependent_classifier_argument(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected_domain,
	uint32_t argument,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret ||
		expected_domain >= terms->term_count || argument >= terms->term_count) {
		return -1;
	}
	uint32_t domain_whnf;
	if (classifier_kernel_whnf(
			terms,
			type_declarations,
			NULL,
			expected_domain,
			&domain_whnf
		) != 0 || domain_whnf >= terms->term_count) {
		return -1;
	}
	*p_ret = argument;
	if (terms->terms[domain_whnf].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		terms->terms[argument].tag != PROTOTYPE_TERM_THUNK) {
		return 0;
	}
	uint32_t computation = terms->terms[argument].as.thunk.computation;
	if (computation < terms->term_count &&
		terms->terms[computation].tag == PROTOTYPE_TERM_LAMBDA) {
		*p_ret = computation;
	}
	return 0;
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
	uint32_t classifier_argument;
	if (prototype_judgement_dependent_classifier_argument(
			terms,
			type_declarations,
			domain,
			argument,
			&classifier_argument
		) != 0) {
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
		classifier_argument,
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

static int static_family_pi_is_pure_total(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t depth
) {
	uint32_t whnf;
	if (!terms || !type_declarations || depth == 0 ||
		classifier_kernel_whnf(
			terms, type_declarations, NULL, classifier, &whnf
		) != 0 || whnf >= terms->term_count ||
		terms->terms[whnf].tag != PROTOTYPE_TERM_PI) {
		return 0;
	}
	uint32_t domain;
	uint32_t family;
	uint32_t binding_id;
	uint32_t body;
	if (pi_parts(terms, whnf, &domain, &family) != 0 ||
		prototype_term_pure_family_parts(
			terms, family, &binding_id, &body
		) != 0 || classifier_kernel_whnf(
			terms, type_declarations, NULL, body, &body
		) != 0 || body >= terms->term_count) {
		return 0;
	}
	(void)domain;
	(void)binding_id;
	if (terms->terms[body].tag == PROTOTYPE_TERM_PI) {
		return static_family_pi_is_pure_total(
			terms, type_declarations, body, depth - 1
		);
	}
	if (terms->terms[body].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		return 1;
	}
	if (terms->terms[body].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_computation_type_is_pure_total(terms, body) != 1) {
		return 0;
	}
	uint32_t result = terms->terms[body].as.computation_type.result;
	if (classifier_kernel_whnf(
			terms, type_declarations, NULL, result, &result
		) != 0 || result >= terms->term_count) {
		return 0;
	}
	return terms->terms[result].tag == PROTOTYPE_TERM_UNIVERSE_VAR;
}

int prototype_judgement_static_family_app_classifier(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t function_classifier,
	uint32_t argument,
	uint32_t argument_classifier,
	uint32_t* p_result_classifier
) {
	uint32_t callable;
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_result_classifier || argument >= terms->term_count ||
		argument_classifier >= terms->term_count || classifier_kernel_whnf(
			terms, type_declarations, NULL, function_classifier, &callable
		) != 0 || callable >= terms->term_count) {
		return -1;
	}
	if (terms->terms[callable].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		callable = terms->terms[callable].as.thunk_type.computation;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, callable, &callable
			) != 0 || callable >= terms->term_count) {
			return -1;
		}
	}
	if (terms->terms[callable].tag != PROTOTYPE_TERM_PI ||
		!static_family_pi_is_pure_total(
			terms, type_declarations, callable, 64
		)) {
		return 1;
	}
	uint32_t domain;
	uint32_t family;
	uint32_t domain_whnf;
	uint32_t solved_domain;
	if (pi_parts(terms, callable, &domain, &family) != 0 ||
		classifier_kernel_whnf(
			terms, type_declarations, NULL, domain, &domain_whnf
		) != 0 || domain_whnf >= terms->term_count) {
		return -1;
	}
	(void)family;
	if (terms->terms[domain_whnf].tag == PROTOTYPE_TERM_RETURN) {
		domain_whnf = terms->terms[domain_whnf].as.return_term.value;
	}
	/* Surface type-family application accepts a raw pure Lambda where the value
	 * domain is its suspended function type. This is the static counterpart of
	 * compile_ref_open_pure_type_family; it changes only classifier inspection,
	 * not the shared APP/LAMBDA Core graph. */
	if (domain_whnf < terms->term_count &&
		terms->terms[domain_whnf].tag == PROTOTYPE_TERM_THUNK_TYPE &&
		terms->terms[argument].tag == PROTOTYPE_TERM_LAMBDA) {
		domain_whnf = terms->terms[domain_whnf].as.thunk_type.computation;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, domain_whnf, &domain_whnf
			) != 0 || domain_whnf >= terms->term_count ||
			terms->terms[domain_whnf].tag != PROTOTYPE_TERM_PI) {
			return -1;
		}
	}
	int domain_status = prototype_judgement_solve_expected_effect_rows(
		terms,
		type_declarations,
		NULL,
		domain_whnf,
		argument_classifier,
		&solved_domain
	);
	if (domain_status != 0) {
		return domain_status < 0 ? -1 : 1;
	}
	(void)solved_domain;
	uint32_t result;
	if (pi_codomain_after_argument_in_context(
			contexts,
			substitutions,
			terms,
			type_declarations,
			context_id,
			callable,
			argument,
			argument_classifier,
			&result
		) != 0 || classifier_kernel_whnf(
			terms, type_declarations, NULL, result, &result
		) != 0 || result >= terms->term_count) {
		return -1;
	}
	if (terms->terms[result].tag == PROTOTYPE_TERM_PI) {
		if (!static_family_pi_is_pure_total(
				terms, type_declarations, result, 64
			)) {
			return 1;
		}
		*p_result_classifier = result;
		return 0;
	}
	if (terms->terms[result].tag == PROTOTYPE_TERM_UNIVERSE_VAR) {
		*p_result_classifier = result;
		return 0;
	}
	if (terms->terms[result].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		prototype_term_computation_type_is_pure_total(terms, result) != 1) {
		return 1;
	}
	uint32_t value_classifier = terms->terms[result].as.computation_type.result;
	uint32_t value_whnf;
	if (classifier_kernel_whnf(
			terms, type_declarations, NULL, value_classifier, &value_whnf
		) != 0 || value_whnf >= terms->term_count) {
		return -1;
	}
	if (terms->terms[value_whnf].tag != PROTOTYPE_TERM_UNIVERSE_VAR) {
		return 1;
	}
	*p_result_classifier = value_classifier;
	return 0;
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

static int classifier_external_type_application_info(
	const struct prototype_term_db* terms,
	uint32_t term,
	struct prototype_qualified_name* p_name,
	uint32_t* arguments,
	uint32_t* p_argument_count
) {
	if (!terms || !p_name || !p_argument_count || term >= terms->term_count) {
		return -1;
	}
	uint32_t reversed[16];
	uint32_t count = 0;
	while (term < terms->term_count &&
		terms->terms[term].tag == PROTOTYPE_TERM_APP) {
		if (count >= 16) {
			return -1;
		}
		reversed[count++] = terms->terms[term].as.app.argument;
		term = terms->terms[term].as.app.function;
	}
	if (term >= terms->term_count ||
		terms->terms[term].tag != PROTOTYPE_TERM_EXTERNAL_REF ||
		(count != 0 && !arguments)) {
		return -1;
	}
	*p_name = terms->terms[term].as.external_ref.name;
	for (uint32_t i = 0; i < count; ++i) {
		arguments[i] = reversed[count - i - 1];
	}
	*p_argument_count = count;
	return 0;
}

static int classifier_instantiate_binding_pair(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected_body,
	uint32_t expected_binding,
	uint32_t actual_body,
	uint32_t actual_binding,
	uint32_t comparison_term,
	uint32_t* p_expected,
	uint32_t* p_actual
) {
	if (!terms || !type_declarations || !p_expected || !p_actual ||
		expected_body >= terms->term_count || actual_body >= terms->term_count ||
		comparison_term >= terms->term_count) {
		return -1;
	}
	return prototype_term_graph_substitute_bound_var(
		terms,
		type_declarations,
		expected_body,
		expected_binding,
		comparison_term,
		p_expected
	) != 0 || prototype_term_graph_substitute_bound_var(
		terms,
		type_declarations,
		actual_body,
		actual_binding,
		comparison_term,
		p_actual
	) != 0 ? -1 : 0;
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
	int expected_status = definitions ? classifier_kernel_whnf(
			terms, type_declarations, definitions,
			expected, &normalized_expected
		) : prototype_judgement_classifier_value_whnf(
			terms, type_declarations, expected, &normalized_expected
		);
	int actual_status = definitions ? classifier_kernel_whnf(
			terms, type_declarations, definitions,
			actual, &normalized_actual
		) : prototype_judgement_classifier_value_whnf(
			terms, type_declarations, actual, &normalized_actual
		);
	if (expected_status != 0 || actual_status != 0) {
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
	/* An imported type family is an EXTERNAL_REF-headed APP before interface
	 * resolution and a TYPE_VIEW instance afterwards. Preserve nominal identity
	 * across that boundary and compare its arguments recursively. */
	struct prototype_qualified_name external_name;
	uint32_t external_arguments[16];
	uint32_t external_argument_count;
	uint32_t instance_type_id;
	uint32_t instance_arguments[16];
	uint32_t instance_argument_count;
	int expected_is_external = classifier_external_type_application_info(
		terms, expected, &external_name, external_arguments,
		&external_argument_count
	) == 0;
	int actual_is_instance = prototype_term_type_instance_info(
		terms, actual, &instance_type_id, instance_arguments,
		&instance_argument_count
	) == 0;
	if (!expected_is_external || !actual_is_instance) {
		expected_is_external = classifier_external_type_application_info(
			terms, actual, &external_name, external_arguments,
			&external_argument_count
		) == 0;
		actual_is_instance = prototype_term_type_instance_info(
			terms, expected, &instance_type_id, instance_arguments,
			&instance_argument_count
		) == 0;
	}
	if (expected_is_external && actual_is_instance &&
		instance_type_id < type_declarations->semantic_schema.type_count) {
		const struct prototype_type_declaration* declaration =
			&type_declarations->semantic_schema.type_declarations[instance_type_id];
		if (declaration->namespace_symbol_id == external_name.namespace_symbol_id &&
			declaration->name_symbol_id == external_name.name_symbol_id &&
			external_argument_count == instance_argument_count) {
			for (uint32_t i = 0; i < external_argument_count; ++i) {
				if (!classifier_kernel_compatible_at_depth(
						terms, type_declarations, definitions,
						external_arguments[i], instance_arguments[i], depth + 1
					)) {
					return 0;
				}
			}
			return 1;
		}
	}
	/* One nominal type instance may be present either as its canonical TYPE_VIEW
	 * or as an APP spine headed by that view while constructor domains are being
	 * specialized. This is an elaboration representation boundary, not
	 * structural equality between different declarations. */
	uint32_t expected_type_id;
	uint32_t expected_arguments[16];
	uint32_t expected_argument_count;
	uint32_t actual_type_id;
	uint32_t actual_arguments[16];
	uint32_t actual_argument_count;
	if (prototype_term_type_instance_info(
			terms, expected, &expected_type_id,
			expected_arguments, &expected_argument_count
		) == 0 && prototype_term_type_instance_info(
			terms, actual, &actual_type_id,
			actual_arguments, &actual_argument_count
		) == 0 && expected_type_id == actual_type_id &&
		expected_argument_count == actual_argument_count) {
		for (uint32_t i = 0; i < expected_argument_count; ++i) {
			if (!classifier_kernel_compatible_at_depth(
					terms, type_declarations, definitions,
					expected_arguments[i], actual_arguments[i], depth + 1
				)) {
				return 0;
			}
		}
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
	/* Classifier-level effect quantification is alpha-invariant just like Pi
	 * binding. Compare both bodies under one fresh row variable; the quantifier
	 * is not erased and no concrete effect solution is selected here. */
	if (terms->terms[expected].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL &&
		terms->terms[actual].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		uint32_t comparison_binding = prototype_term_new_binding(terms);
		uint32_t comparison_row;
		uint32_t expected_body;
		uint32_t actual_body;
		if (comparison_binding == PROTOTYPE_INVALID_ID ||
			prototype_term_effect_row_var(
				terms, comparison_binding, &comparison_row
			) != 0 || classifier_instantiate_binding_pair(
				terms,
				type_declarations,
				terms->terms[expected].as.effect_row_forall.body,
				terms->terms[expected].as.effect_row_forall.binding_id,
				terms->terms[actual].as.effect_row_forall.body,
				terms->terms[actual].as.effect_row_forall.binding_id,
				comparison_row,
				&expected_body,
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
	/* Surface nested lambdas remain raw computation terms. In a computation
	 * result position, elaboration may expose such a term through the canonical
	 * pure quotation Comp({}, Thunk(C)). This rule is directional and requires
	 * the empty row; it neither executes a computation nor adds THUNK/FORCE to
	 * kernel conversion. */
	if (terms->terms[expected].tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		terms->terms[actual].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		const struct prototype_term* computation = &terms->terms[expected];
		if (computation->as.computation_type.label < terms->term_count &&
			terms->terms[computation->as.computation_type.label].tag ==
				PROTOTYPE_TERM_EFFECT_ROW_EMPTY &&
			computation->as.computation_type.result < terms->term_count &&
			terms->terms[computation->as.computation_type.result].tag ==
				PROTOTYPE_TERM_THUNK_TYPE) {
			return classifier_kernel_compatible_at_depth(
				terms,
				type_declarations,
				definitions,
				terms->terms[
					computation->as.computation_type.result
				].as.thunk_type.computation,
				actual,
				depth + 1
			);
		}
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
	if (classifier_instantiate_binding_pair(
			terms,
			type_declarations,
			expected_family_body,
			expected_binder,
			actual_family_body,
			actual_binder,
			comparison_var,
			&expected_body,
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

static int expected_effect_row_reaches_binding(
	const struct expected_effect_row_solver* solver,
	uint32_t row,
	uint32_t target_binding
) {
	if (!solver) {
		return -1;
	}
	for (uint32_t depth = 0; depth <= solver->solution_count; ++depth) {
		if (row >= solver->terms->term_count ||
			solver->terms->terms[row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			return 0;
		}
		uint32_t binding_id =
			solver->terms->terms[row].as.effect_row_var.binding_id;
		if (binding_id == target_binding) {
			return 1;
		}
		int found = 0;
		for (uint32_t i = 0; i < solver->solution_count; ++i) {
			if (solver->solutions[i].binding_id == binding_id) {
				row = solver->solutions[i].row;
				found = 1;
				break;
			}
		}
		if (!found) {
			return 0;
		}
	}
	return -1;
}

static int expected_effect_row_bind(
	struct expected_effect_row_solver* solver,
	uint32_t binding_id,
	uint32_t row
) {
	if (!solver || row >= solver->terms->term_count) {
		return -1;
	}
	int closes_existing_equation = expected_effect_row_reaches_binding(
		solver, row, binding_id
	);
	if (closes_existing_equation != 0) {
		return closes_existing_equation > 0 ? 0 : -1;
	}
	for (uint32_t i = 0; i < solver->solution_count; ++i) {
		if (solver->solutions[i].binding_id != binding_id) {
			continue;
		}
		uint32_t existing = solver->solutions[i].row;
		if (existing < solver->terms->term_count &&
			solver->terms->terms[existing].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			uint32_t existing_binder =
				solver->terms->terms[existing].as.effect_row_var.binding_id;
			if (existing_binder == binding_id) {
				solver->solutions[i].row = row;
				return 0;
			}
			return expected_effect_row_bind(solver, existing_binder, row);
		}
		if (row < solver->terms->term_count &&
			solver->terms->terms[row].tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			uint32_t row_binder =
				solver->terms->terms[row].as.effect_row_var.binding_id;
			if (row_binder == binding_id) {
				return 0;
			}
			return expected_effect_row_bind(
				solver, row_binder, existing
			);
		}
		int same = classifier_kernel_conversion(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			solver->solutions[i].row,
			row
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
		return same ? 0 : 1;
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
	const struct prototype_term* actual_term = &solver->terms->terms[actual];
	if (actual_term->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return expected_effect_row_bind(
			solver, actual_term->as.effect_row_var.binding_id, expected
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

/* Surface value arrows quote a negative function computation as
 * Comp({}, Thunk(Pi(...))), while a source Lambda spine remains a raw Pi.
 * This helper opens only that pure elaboration boundary. It is deliberately
 * local to expected-row solving and therefore does not make the two
 * classifiers judgementally equal. */
static int expected_effect_row_open_pure_function_quotation(
	struct expected_effect_row_solver* solver,
	uint32_t classifier,
	uint32_t* p_function
) {
	if (!solver || !p_function || classifier >= solver->terms->term_count ||
		solver->terms->terms[classifier].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return 1;
	}
	const struct prototype_term* computation = &solver->terms->terms[classifier];
	uint32_t value;
	if (classifier_kernel_whnf(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			computation->as.computation_type.result,
			&value
		) != 0 || value >= solver->terms->term_count ||
		solver->terms->terms[value].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 1;
	}
	uint32_t empty_row;
	if (prototype_term_effect_row_empty(solver->terms, &empty_row) != 0) {
		return -1;
	}
	int row_status = expected_effect_row_solve_row(
		solver, computation->as.computation_type.label, empty_row
	);
	if (row_status != 0) {
		return row_status;
	}
	*p_function = solver->terms->terms[value].as.thunk_type.computation;
	return 0;
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
	if (expected == actual) {
		return 0;
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
	if (expected == actual) {
		return 0;
	}
	const struct prototype_term* expected_term = &solver->terms->terms[expected];
	const struct prototype_term* actual_term = &solver->terms->terms[actual];
	if (expected_term->tag == PROTOTYPE_TERM_RETURN) {
		return expected_effect_row_solve_classifier(
			solver,
			expected_term->as.return_term.value,
			actual,
			depth + 1
		);
	}
	if (actual_term->tag == PROTOTYPE_TERM_RETURN) {
		return expected_effect_row_solve_classifier(
			solver,
			expected,
			actual_term->as.return_term.value,
			depth + 1
		);
	}
	if (expected_term->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL &&
		actual_term->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		uint32_t expected_row;
		uint32_t instantiated_actual;
		/* Solve in the expected quantifier's scope. A fresh comparison binder
		 * would be sound for a read-only equality check, but row solutions are
		 * substituted back into the original expected classifier. Letting such a
		 * fresh binder occur in a solution would therefore escape its scope. */
		if (prototype_term_effect_row_var(
				solver->terms,
				expected_term->as.effect_row_forall.binding_id,
				&expected_row
			) != 0 || prototype_term_graph_substitute_bound_var(
				solver->terms,
				solver->type_declarations,
				actual_term->as.effect_row_forall.body,
				actual_term->as.effect_row_forall.binding_id,
				expected_row,
				&instantiated_actual
			) != 0) {
			return -1;
		}
		return expected_effect_row_solve_classifier(
			solver,
			expected_term->as.effect_row_forall.body,
			instantiated_actual,
			depth + 1
		);
	}
	/* A synthesized lambda may expose a classifier-level row scheme while the
	 * surface expectation contains open rows at the corresponding arrows.
	 * Instantiate the scheme from that structure before solving the expected
	 * rows. This is elimination of the classifier quantifier, not conversion of
	 * every row variable to an arbitrary row. */
	if (actual_term->tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		uint32_t row;
		int row_status = find_effect_row_argument(
			solver->terms,
			solver->type_declarations,
			actual_term->as.effect_row_forall.body,
			expected,
			actual_term->as.effect_row_forall.binding_id,
			0,
			&row
		);
		uint32_t instantiated;
		if (row_status < 0) {
			return -1;
		}
		if (row_status > 0) {
			int bind_status = expected_effect_row_bind(
				solver, actual_term->as.effect_row_forall.binding_id, row
			);
			if (bind_status != 0) {
				return bind_status;
			}
		}
		if (row_status > 0 && prototype_term_graph_substitute_bound_var(
				solver->terms,
				solver->type_declarations,
				actual_term->as.effect_row_forall.body,
				actual_term->as.effect_row_forall.binding_id,
				row,
				&instantiated
			) != 0) {
			return -1;
		}
		if (row_status == 0) {
			return 1;
		}
		return expected_effect_row_solve_classifier(
			solver, expected, instantiated, depth + 1
		);
	}
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
	/* Nested lambdas remain raw Pi computations. A value-position arrow may
	 * expose the same negative spine through a pure quotation. Either side may
	 * carry that surface-derived representation after descending through Pi
	 * domains, so compare both through the same local boundary operation. */
	if (expected_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
		actual_term->tag == PROTOTYPE_TERM_PI) {
		uint32_t expected_function;
		int open_status = expected_effect_row_open_pure_function_quotation(
			solver, expected, &expected_function
		);
		if (open_status != 0) {
			return open_status;
		}
		return expected_effect_row_solve_classifier(
			solver, expected_function, actual,
			depth + 1
		);
	}
	if (expected_term->tag == PROTOTYPE_TERM_PI &&
		actual_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		uint32_t actual_function;
		int open_status = expected_effect_row_open_pure_function_quotation(
			solver, actual, &actual_function
		);
		if (open_status != 0) {
			return open_status;
		}
		return expected_effect_row_solve_classifier(
			solver, expected, actual_function,
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
	if (expected_term->tag == PROTOTYPE_TERM_APP &&
		actual_term->tag == PROTOTYPE_TERM_APP) {
		int function_status = expected_effect_row_solve_classifier(
			solver,
			expected_term->as.app.function,
			actual_term->as.app.function,
			depth + 1
		);
		if (function_status != 0) {
			return function_status;
		}
		return expected_effect_row_solve_classifier(
			solver,
			expected_term->as.app.argument,
			actual_term->as.app.argument,
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
	uint32_t solved_actual = actual;
	for (uint32_t i = 0; i < solver.solution_count; ++i) {
		if (prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				solved,
				solver.solutions[i].binding_id,
				solver.solutions[i].row,
				&solved
			) != 0 || prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				solved_actual,
				solver.solutions[i].binding_id,
				solver.solutions[i].row,
				&solved_actual
			) != 0) {
			return -1;
		}
	}
	/* Recheck with the solved expectation through the same structural procedure.
	 * Generic conversion deliberately does not instantiate classifier-level row
	 * schemes and therefore cannot validate a forall nested below dependent Pi
	 * binders. Row-scheme elimination belongs to this solver, not to DefEq. */
	status = expected_effect_row_solve_classifier(
		&solver, solved, solved_actual, 0
	);
	if (status != 0) {
		return status;
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
