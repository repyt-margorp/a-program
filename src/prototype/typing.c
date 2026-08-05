#include "judgement.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int reserve_slot(size_t count, size_t capacity) {
	return count < capacity ? 0 : -1;
}

void prototype_judgement_db_init(
	struct prototype_judgement_db* db,
	struct prototype_judgement_relation* relations,
	struct prototype_judgement_proof* proofs,
	size_t relation_capacity
) {
	memset(db, 0, sizeof(*db));
	db->relations = relations;
	db->proofs = proofs;
	db->relation_capacity = relation_capacity;
	db->proof_capacity = relation_capacity;
}

void prototype_judgement_delta_init(
	struct prototype_judgement_delta* delta,
	struct prototype_judgement_db* db,
	struct prototype_judgement_relation* relations,
	struct prototype_judgement_proof* proofs,
	size_t relation_capacity,
	struct prototype_judgement_match_motive_result* match_motive_results,
	size_t match_motive_result_capacity,
	struct prototype_judgement_computation_constraint* computation_constraints,
	size_t computation_constraint_capacity,
	struct prototype_judgement_effect_row_constraint* effect_row_constraints,
	size_t effect_row_constraint_capacity
) {
	memset(delta, 0, sizeof(*delta));
	delta->db = db;
	delta->relations = relations;
	delta->proofs = proofs;
	delta->match_motive_results = match_motive_results;
	delta->computation_constraints = computation_constraints;
	delta->effect_row_constraints = effect_row_constraints;
	delta->relation_capacity = relation_capacity;
	delta->proof_capacity = relation_capacity;
	delta->match_motive_result_capacity = match_motive_result_capacity;
	delta->computation_constraint_capacity = computation_constraint_capacity;
	delta->effect_row_constraint_capacity = effect_row_constraint_capacity;
}

void prototype_judgement_delta_set_solver_budget(
	struct prototype_judgement_delta* delta,
	uint64_t step_limit,
	uint64_t* steps_used,
	int* exhausted
) {
	if (!delta) {
		return;
	}
	delta->solver_step_limit = step_limit;
	delta->solver_steps_used = steps_used;
	delta->solver_exhausted = exhausted;
}

void prototype_judgement_delta_set_context(
	struct prototype_judgement_delta* delta,
	uint32_t context_id
) {
	if (!delta) {
		return;
	}
	delta->current_context_id = context_id;
}

void prototype_judgement_delta_set_context_store(
	struct prototype_judgement_delta* delta,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions
) {
	if (!delta) {
		return;
	}
	delta->contexts = contexts;
	delta->substitutions = substitutions;
}

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

size_t prototype_judgement_delta_mark(
	const struct prototype_judgement_delta* delta
) {
	return delta ? delta->relation_count : 0;
}

void prototype_judgement_delta_rewind(
	struct prototype_judgement_delta* delta,
	size_t mark
) {
	if (!delta) {
		return;
	}
	if (mark < delta->relation_count) {
		delta->relation_count = mark;
	}
	if (mark < delta->proof_count) {
		delta->proof_count = mark;
	}
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
	unsigned result_effects;
	unsigned left_effects;
	unsigned right_effects;
	if (prototype_term_effect_row_closed_bits(terms, result->effect_row, &result_effects) == 0 &&
		prototype_term_effect_row_closed_bits(terms, left->effect_row, &left_effects) == 0 &&
		prototype_term_effect_row_closed_bits(terms, right->effect_row, &right_effects) == 0) {
		return result_effects == (left_effects | right_effects);
	}
	/* EFFECT_ROW_UNION normalizes the empty row away even when the other row is
	 * symbolic. The proof validator must use the same unit law as TermDB. */
	if (prototype_term_effect_row_closed_bits(terms, left->effect_row, &left_effects) == 0 &&
		left_effects == PROTOTYPE_EFFECT_OPERATION_LABEL_NONE) {
		int equal = 0;
		return prototype_term_view_shape_equal(
			terms, result->effect_row, right->effect_row, &equal
		) == 0 && equal;
	}
	if (prototype_term_effect_row_closed_bits(terms, right->effect_row, &right_effects) == 0 &&
		right_effects == PROTOTYPE_EFFECT_OPERATION_LABEL_NONE) {
		int equal = 0;
		return prototype_term_view_shape_equal(
			terms, result->effect_row, left->effect_row, &equal
		) == 0 && equal;
	}
	if (result->effect_row >= terms->term_count ||
		terms->terms[result->effect_row].tag != PROTOTYPE_TERM_EFFECT_ROW_UNION) {
		return 0;
	}
	const struct prototype_term* row = &terms->terms[result->effect_row];
	int left_equal = 0;
	int right_equal = 0;
	if (prototype_term_view_shape_equal(terms, row->as.effect_row_union.left,
			left->effect_row, &left_equal) != 0 ||
		prototype_term_view_shape_equal(terms, row->as.effect_row_union.right,
			right->effect_row, &right_equal) != 0) {
		return 0;
	}
	(void)type_declarations;
	return left_equal && right_equal;
}

/* Closed rows are solved immediately. Symbolic residual rows remain for the
 * row-constraint solver rather than being guessed from a bitset. */
static int closed_computation_fold_residual_row(
	struct prototype_term_db* terms,
	const struct prototype_term_classifier_view* input,
	const struct prototype_term_classifier_view* operation,
	uint32_t* p_residual
) {
	unsigned input_effects;
	unsigned operation_effects;
	if (!terms || !input || !operation || !p_residual ||
		input->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		operation->computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		prototype_term_effect_row_closed_bits(terms, input->effect_row, &input_effects) != 0 ||
		prototype_term_effect_row_closed_bits(
			terms, operation->effect_row, &operation_effects
		) != 0) {
		return 1;
	}
	if ((input_effects & operation_effects) != operation_effects) {
		return -1;
	}
	return prototype_term_effect_label(terms, input_effects & ~operation_effects, p_residual);
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
		unsigned result;
		if (prototype_term_effect_row_closed_bits(
				terms, constraint->result_row, &result
			) != 0) {
			constraint->solved = 0;
			continue;
		}
		unsigned operand_bits[
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_MAX_OPERANDS
		];
		int closed = 1;
		for (uint32_t j = 0; j < constraint->operand_count; ++j) {
			if (prototype_term_effect_row_closed_bits(
					terms, constraint->operands[j], &operand_bits[j]
				) != 0) {
				closed = 0;
				break;
			}
		}
		if (!closed) {
			constraint->solved = 0;
			continue;
		}
		if (constraint->kind == PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN) {
			unsigned joined = 0;
			for (uint32_t j = 0; j < constraint->operand_count; ++j) {
				joined |= operand_bits[j];
			}
			if (result != joined) {
				return -1;
			}
		} else if (constraint->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_RESIDUAL) {
			if (constraint->operand_count != 2 ||
				(operand_bits[0] & operand_bits[1]) != operand_bits[1] ||
				result != (operand_bits[0] & ~operand_bits[1])) {
				return -1;
			}
		} else if (constraint->kind ==
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION) {
			if (constraint->operand_count != 2 ||
				(operand_bits[0] & operand_bits[1]) != operand_bits[0] ||
				result != operand_bits[1]) {
				return -1;
			}
		} else {
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
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind
);
static int add_delta_relation_with_premises(
	struct prototype_judgement_delta* delta,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
);
static void set_proof_premises(
	struct prototype_judgement_proof* proof,
	const struct prototype_judgement_db* judgement,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
);
static void initialize_proof_rule_parameters(
	struct prototype_judgement_proof* proof
);
static int copy_db_relation_rule_parameters(
	struct prototype_judgement_db* judgement,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const struct prototype_judgement_proof* source
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
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t expected,
	uint32_t actual
);
static int prototype_judgement_delta_expand_match_motive_with_premises(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
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

static int proof_kind_requires_local_context(int proof_kind) {
	return proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION ||
		proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM;
}

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

static int classifier_kernel_normalization_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
) {
	if (!term_exists(terms, expected) || !term_exists(terms, actual)) {
		return 0;
	}
	/* Judgemental equality is reflexive even when WHNF expansion would enter a
	 * guarded recursive motive. Normalization is evidence for distinct nodes,
	 * not a prerequisite for a node being equal to itself. */
	if (expected == actual) {
		return 1;
	}
	if (term_is_universe_var(terms, expected) && term_is_universe_var(terms, actual)) {
		return terms->terms[expected].as.universe_var.level_var ==
			terms->terms[actual].as.universe_var.level_var;
	}
	int equal = 0;
	return prototype_term_normalization_equal_with_profile(
			terms,
			type_declarations,
			definitions,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			expected,
			actual,
			&equal
		) == 0 && equal;
}

int prototype_judgement_classifier_normalization_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_normalization_equal(
		terms,
		type_declarations,
		NULL,
		expected,
		actual
	);
}

int prototype_judgement_classifier_normalization_equal_with_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t expected,
	uint32_t actual
) {
	return classifier_kernel_normalization_equal(
		terms,
		type_declarations,
		definitions,
		expected,
		actual
	);
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
	if (prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, expected, actual
		)) {
		return 1;
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
		if (prototype_term_effect_row_closed_bits(
				terms, effect_row, &p_ret->effects
			) != 0) {
			p_ret->effects = PROTOTYPE_EFFECT_OPERATION_LABEL_NONE;
		}
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

static int classifier_kernel_whnf_no_definitions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t* p_ret
) {
	return classifier_kernel_whnf(terms, type_declarations, NULL, term_id, p_ret);
}

static int lookup_relation_classifier(
	const struct prototype_judgement_relation* relations,
	size_t relation_count,
	uint32_t subject,
	uint32_t* p_classifier,
	int include_conversion
) {
	if (!relations || !p_classifier) {
		return -1;
	}
	for (size_t i = relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation = &relations[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			(include_conversion || relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION)) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	return -1;
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
		judgement->relations,
		judgement->relation_count,
		subject,
		p_classifier,
		1
	);
}

static int lookup_delta_classifier(
	const struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!delta || !p_classifier) {
		return -1;
	}
	if (lookup_relation_classifier(
		delta->relations,
		delta->relation_count,
		subject,
		p_classifier,
		1
	) == 0) {
		return 0;
	}
	return lookup_classifier(delta->db, terms, subject, p_classifier);
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
		delta->relations,
		delta->relation_count,
		subject,
		p_classifier,
		1
	) == 0) {
		return 0;
	}
	return lookup_classifier(delta->db, terms, subject, p_classifier);
}

static int lookup_delta_classifier_normalization_equal(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		!term_exists(terms, subject) || !term_exists(terms, expected)) {
		return -1;
	}
	for (size_t i = 0; i < delta->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &delta->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				expected,
				relation->classifier
			)) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	if (delta->db) {
		for (size_t i = 0; i < delta->db->relation_count; ++i) {
			const struct prototype_judgement_relation* relation =
				&delta->db->relations[i];
			if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->subject == subject &&
				prototype_judgement_classifier_normalization_equal(
					terms,
					type_declarations,
					expected,
					relation->classifier
				)) {
				*p_classifier = relation->classifier;
				return 0;
			}
		}
	}
	return 1;
}

static int lookup_delta_proven_classifier_normalization_equal(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		!term_exists(terms, subject) || !term_exists(terms, expected)) {
		return -1;
	}
	for (size_t i = 0; i < delta->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &delta->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				expected,
				relation->classifier
			)) {
			*p_classifier = relation->classifier;
			return 0;
		}
	}
	if (delta->db) {
		for (size_t i = 0; i < delta->db->relation_count; ++i) {
			const struct prototype_judgement_relation* relation =
				&delta->db->relations[i];
			if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->subject == subject &&
				prototype_judgement_classifier_normalization_equal(
					terms,
					type_declarations,
					expected,
					relation->classifier
				)) {
				*p_classifier = relation->classifier;
				return 0;
			}
		}
	}
	return 1;
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
		left->as.effect_row_var.binder_id == row_binder) {
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
			if (left->as.effect_row_forall.binder_id == row_binder) {
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
				quantified->as.effect_row_forall.binder_id,
				0,
				&row
			);
			if (row_status <= 0 || prototype_term_graph_substitute_bound_var(
					terms,
					type_declarations,
					quantified->as.effect_row_forall.body,
					quantified->as.effect_row_forall.binder_id,
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
				quantified->as.effect_row_forall.binder_id,
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

int prototype_context_instantiate_pure_family(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t domain,
	uint32_t family,
	uint32_t argument,
	uint32_t argument_classifier,
	uint32_t argument_proof_id,
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
	uint32_t binder_id;
	uint32_t body;
	if (prototype_term_pure_family_parts(
			terms,
			family,
			&binder_id,
			&body
		) != 0) {
		return -1;
	}
	uint32_t family_context;
	if (prototype_context_extend(
			contexts,
			prefix->target_context,
			binder_id,
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
	if (context->binder_id != binder_id &&
		prototype_term_contains_free_binder(terms, body, binder_id)) {
		uint32_t canonical_var;
		if (prototype_term_var(
				terms, context->binder_id, &canonical_var
			) != 0 || prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				body,
				binder_id,
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
			argument_proof_id,
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
	uint32_t argument_proof_id,
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
	return prototype_context_instantiate_pure_family(
		contexts,
		substitutions,
		terms,
		type_declarations,
		identity,
		argument_classifier,
		codomain_family,
		argument,
		argument_classifier,
		argument_proof_id,
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
	return prototype_context_instantiate_pure_family(
		delta->contexts,
		delta->substitutions,
		terms,
		type_declarations,
		projection,
		binder_classifier,
		family,
		binder_var,
		binder_classifier,
		binder_proof_id,
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
	if (!delta || !delta->contexts || binder_var >= terms->term_count ||
		terms->terms[binder_var].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t premise_context;
	if (prototype_context_extend(
			delta->contexts,
			base_context_id,
			terms->terms[binder_var].as.var.binder_id,
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
	if (classifier_kernel_normalization_equal(
			terms,
			type_declarations,
			definitions,
			expected,
			actual
		)) {
		return 1;
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
		if (!classifier_kernel_normalization_equal(
				terms,
				type_declarations,
				definitions,
				expected_computation->as.computation_type.label,
				actual_computation->as.computation_type.label
			)) {
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
	uint32_t comparison_binder = prototype_term_fresh_binder(terms);
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
	uint32_t binder_id;
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
	uint32_t binder_id,
	uint32_t row
) {
	if (!solver || row >= solver->terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < solver->solution_count; ++i) {
		if (solver->solutions[i].binder_id != binder_id) {
			continue;
		}
		return classifier_kernel_normalization_equal(
			solver->terms,
			solver->type_declarations,
			solver->definitions,
			solver->solutions[i].row,
			row
		) ? 0 : 1;
	}
	if (solver->solution_count >= 64) {
		return -1;
	}
	solver->solutions[solver->solution_count].binder_id = binder_id;
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
			solver, expected_term->as.effect_row_var.binder_id, actual
		);
	}
	return classifier_kernel_normalization_equal(
		solver->terms,
		solver->type_declarations,
		solver->definitions,
		expected,
		actual
	) ? 0 : 1;
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
			!classifier_kernel_normalization_equal(
				solver->terms,
				solver->type_declarations,
				solver->definitions,
				expected_match.as.match.scrutinee,
				actual_match.as.match.scrutinee
			)) {
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
					!classifier_kernel_normalization_equal(
						solver->terms,
						solver->type_declarations,
						solver->definitions,
						expected_case.constructor_owner,
						actual_case.constructor_owner
					)) {
					return 1;
				}
			}

			uint32_t expected_body = expected_case.body;
			uint32_t actual_body = actual_case.body;
			for (uint32_t j = 0; j < expected_case.binder_count; ++j) {
				uint32_t expected_binder_index = expected_case.first_binder + j;
				uint32_t actual_binder_index = actual_case.first_binder + j;
				uint32_t comparison_binder = prototype_term_fresh_binder(solver->terms);
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
						solver->terms->case_binders[expected_binder_index].binder_id,
						comparison_var,
						&expected_body
					) != 0 || prototype_term_graph_substitute_bound_var(
						solver->terms,
						solver->type_declarations,
						actual_body,
						solver->terms->case_binders[actual_binder_index].binder_id,
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
		uint32_t comparison_binder = prototype_term_fresh_binder(solver->terms);
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
				solver.solutions[i].binder_id,
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
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t binder_id,
	uint32_t* p_argument
) {
	if (!terms || !type_declarations || !p_argument || owner >= terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(
			terms, owner, &type_id, args, &arg_count
		) != 0 || type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		uint32_t parameter_id = type->first_parameter + i;
		if (parameter_id >= type_declarations->parameter_count ||
			i >= arg_count) {
			return -1;
		}
		if (type_declarations->parameter_declarations[parameter_id].binder_id ==
			binder_id) {
			*p_argument = args[i];
			return 0;
		}
	}
	return -1;
}

static int resolver_type_expr_term_with_self(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret || type_expr >= type_declarations->expr_count) {
		return -1;
	}

	const struct prototype_type_expr expr = type_declarations->exprs[type_expr];
	switch (expr.tag) {
		case PROTOTYPE_TYPE_EXPR_UNIVERSE:
			return prototype_term_universe_var(terms, expr.as.universe.level, p_ret);
		case PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR:
			return prototype_term_universe_var(terms, expr.as.universe_var.level_var, p_ret);
		case PROTOTYPE_TYPE_EXPR_SELF:
			if (self_type == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			*p_ret = self_type;
			return 0;
		case PROTOTYPE_TYPE_EXPR_VAR:
			if (self_type != PROTOTYPE_INVALID_ID &&
				owner_parameter_argument(
					terms,
					type_declarations,
					self_type,
					expr.as.var.binder_id,
					p_ret
				) == 0) {
				return 0;
			}
			return prototype_term_var(terms, expr.as.var.binder_id, p_ret);
		case PROTOTYPE_TYPE_EXPR_NAME: {
			const struct prototype_type_declaration* type =
				prototype_type_declaration_lookup(type_declarations, expr.as.name.symbol_id);
			if (!type) {
				return -1;
			}
			return prototype_term_type_instance_make(
				terms,
				type_declarations,
				type->type_index,
				NULL,
				0,
				p_ret
			);
		}
			case PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE: {
				return prototype_term_external_ref(
					terms,
					expr.as.imported_type.name,
					p_ret
				);
			}
			case PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM:
				return prototype_term_external_ref(terms, expr.as.external_term.name, p_ret);
		case PROTOTYPE_TYPE_EXPR_APP: {
			uint32_t function;
			uint32_t argument;
			if (resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.app.function,
					self_type,
					&function
				) != 0 ||
				resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.app.argument,
					self_type,
					&argument
				) != 0) {
				return -1;
			}
			uint32_t type_id;
				uint32_t args[16];
				uint32_t arg_count;
				if (prototype_term_type_instance_info(terms, function, &type_id, args, &arg_count) == 0) {
					if (type_id >= type_declarations->type_count) {
						return -1;
					}
					const struct prototype_type_declaration* type =
						&type_declarations->type_declarations[type_id];
					if (arg_count < type->parameter_count) {
						return prototype_term_type_instance_extend(
							terms,
							type_declarations,
							function,
							argument,
							p_ret
						);
					}
				}
			return prototype_term_app(terms, function, argument, p_ret);
		}
		case PROTOTYPE_TYPE_EXPR_ARROW: {
			uint32_t domain;
			uint32_t codomain;
			if (resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.arrow.domain,
					self_type,
					&domain
				) != 0 ||
				resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.arrow.codomain,
					self_type,
					&codomain
				) != 0) {
				return -1;
			}
			return prototype_term_pi(terms, domain, codomain, p_ret);
		}
		case PROTOTYPE_TYPE_EXPR_PI: {
			uint32_t domain;
			uint32_t codomain;
			uint32_t family;
			if (resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.pi.domain,
					self_type,
					&domain
				) != 0 || resolver_type_expr_term_with_self(
					terms,
					type_declarations,
					expr.as.pi.codomain,
					self_type,
					&codomain
				) != 0 || prototype_term_pure_family(
					terms, expr.as.pi.binder_id, codomain, &family
				) != 0) {
				return -1;
			}
			return prototype_term_pi_family(terms, domain, family, p_ret);
		}
		default:
			return -1;
	}
}

int prototype_judgement_type_expr_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t type_expr,
	uint32_t self_type,
	uint32_t* p_ret
) {
	return resolver_type_expr_term_with_self(
		terms,
		type_declarations,
		type_expr,
		self_type,
		p_ret
	);
}

int prototype_judgement_resolve_match_constructor(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t scrutinee_classifier,
	int constructor_symbol_id,
	struct prototype_match_constructor_resolution* p_resolution
) {
	if (!terms || !type_declarations || !contexts || !p_resolution ||
		scrutinee_classifier >= terms->term_count) {
		return -1;
	}

	uint32_t type_id;
	uint32_t ignored_args[16];
	uint32_t ignored_arg_count;
	uint32_t normalized_classifier;
	if (classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			scrutinee_classifier,
			&normalized_classifier
		) != 0) {
		return -1;
	}
	if (prototype_term_type_instance_info(
		terms,
		normalized_classifier,
		&type_id,
		ignored_args,
		&ignored_arg_count
	) != 0) {
		return -1;
	}

	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(
			type_declarations,
			type_id,
			constructor_symbol_id
		);
	if (!constructor) {
		return -1;
	}

	uint32_t field_contexts[64];
	uint32_t field_count;
	if (prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			field_contexts,
			64,
			&field_count
		) != 0) {
		return -1;
	}

	p_resolution->constructor_owner = normalized_classifier;
	p_resolution->constructor_id = constructor->constructor_index;
	p_resolution->field_count = field_count;
	return 0;
}

static const struct prototype_type_constructor_declaration* lookup_constructor_for_owner_index(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations ||
		prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			args,
			16,
			&arg_count
		) != 0 ||
		type_id >= type_declarations->type_count) {
		return NULL;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == constructor_index) {
			return constructor;
		}
	}
	return NULL;
}

static int constructor_classifier_from_curried_cache(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	const struct prototype_type_constructor_declaration* constructor,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !constructor || !p_classifier ||
		constructor->curried_classifier_cache == PROTOTYPE_INVALID_ID ||
		constructor->curried_classifier_cache >= terms->term_count ||
		owner >= terms->term_count) {
		return 1;
	}

	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			args,
			16,
			&arg_count
		) != 0 ||
		type_id >= type_declarations->type_count ||
		type_id != constructor->owner_type) {
		return 1;
	}

	uint32_t classifier = constructor->curried_classifier_cache;
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t app;
		if (prototype_term_app(terms, classifier, args[i], &app) != 0) {
			return -1;
		}
		classifier = app;
	}

	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			(int)classifier,
			&classifier
		) != 0) {
		return -1;
	}
	if (!classifier_returns_owner(terms, type_declarations, classifier, owner)) {
		return 1;
	}
	*p_classifier = classifier;
	return 0;
}

static int materialize_constructor_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index,
	uint32_t* p_constructor_term,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_constructor_term || !p_classifier ||
		owner >= terms->term_count) {
		return -1;
	}

	uint32_t constructor_term;
	if (prototype_term_constructor(
			terms,
			owner,
			constructor_index,
			&constructor_term
		) != 0) {
		return -1;
	}

	uint32_t existing_classifiers[32];
	uint32_t existing_classifier_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			constructor_term,
			existing_classifiers,
			32,
			&existing_classifier_count
		) != 0) {
		return -1;
	}
	uint32_t selected_existing = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < existing_classifier_count; ++i) {
		if (!classifier_returns_owner(terms, type_declarations, existing_classifiers[i], owner)) {
			continue;
		}
		if (selected_existing != PROTOTYPE_INVALID_ID &&
			!prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				selected_existing,
				existing_classifiers[i]
			)) {
			return -1;
		}
		selected_existing = existing_classifiers[i];
	}
	if (selected_existing != PROTOTYPE_INVALID_ID) {
		*p_constructor_term = constructor_term;
		*p_classifier = selected_existing;
		return 0;
	}

	uint32_t schema_owner;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			(int)owner,
			&schema_owner
		) != 0) {
		return -1;
	}
	uint32_t schema_type_id;
	uint32_t schema_args[16];
	uint32_t schema_arg_count;
	if (prototype_term_type_instance_info(
			terms,
			schema_owner,
			&schema_type_id,
			schema_args,
			&schema_arg_count
		) != 0) {
		if (prototype_type_declaration_instance_info(
				type_declarations,
				terms,
				schema_owner,
				&schema_type_id,
				schema_args,
				16,
				&schema_arg_count
			) != 0 ||
			prototype_term_type_instance_make(
				terms,
				type_declarations,
				schema_type_id,
				schema_args,
				schema_arg_count,
				&schema_owner
			) != 0) {
			return -1;
		}
	}
	const struct prototype_type_constructor_declaration* constructor =
		lookup_constructor_for_owner_index(
			terms,
			type_declarations,
			schema_owner,
			constructor_index
		);
	if (!constructor) {
		return -1;
	}

	uint32_t cached_classifier;
	int cache_status = constructor_classifier_from_curried_cache(
		terms,
		type_declarations,
		owner,
		constructor,
		&cached_classifier
	);
	if (cache_status < 0) {
		return -1;
	}
	if (cache_status > 0) {
		return -1;
	}

	if (add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			constructor_term,
			cached_classifier,
			PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO
		) != 0) {
		return -1;
	}
	*p_constructor_term = constructor_term;
	*p_classifier = cached_classifier;
	return 0;
}

static int constructor_field_classifier_from_spine(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		!delta->contexts || !delta->substitutions ||
		(previous_binder_count > 0 && !previous_binders)) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			arguments,
			64,
			&argument_count
		) != 0) {
		return -1;
	}
	if (type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (argument_count != type->parameter_count ||
		constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	if (previous_binder_count < field_index) {
		return -1;
	}
	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			delta->contexts,
			delta->substitutions,
			terms,
			type_declarations,
			delta->current_context_id,
			constructor->parameter_context,
			arguments,
			argument_count,
			&substitution
		) != 0) {
		return -1;
	}
	uint32_t previous_terms[64];
	for (uint32_t i = 0; i < field_index; ++i) {
		if (prototype_term_var(
				terms, previous_binders[i].binder_id, &previous_terms[i]
			) != 0) {
			return -1;
		}
	}
	return prototype_context_telescope_entry_classifier(
		delta->contexts,
		delta->substitutions,
		terms,
		type_declarations,
		substitution,
		constructor->parameter_context,
		constructor->field_context,
		previous_terms,
		field_index,
		field_index,
		p_classifier
	);
}

int prototype_judgement_constructor_spine_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t source_context,
	uint32_t subject,
	uint32_t constructor_owner_view,
	const uint32_t* argument_classifiers,
	uint32_t argument_classifier_count,
	uint32_t* p_classifier,
	int* p_saturated
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!p_classifier || !p_saturated ||
		(argument_classifier_count > 0 && !argument_classifiers)) {
		return -1;
	}
	uint32_t constructor_head;
	uint32_t structural_owner;
	uint32_t constructor_index;
	uint32_t field_terms[64];
	uint32_t field_count;
	if (prototype_term_constructor_spine_info(
			terms,
			subject,
			&constructor_head,
			&structural_owner,
			&constructor_index,
			field_terms,
			64,
			&field_count
		) != 0 || field_count != argument_classifier_count) {
		return -1;
	}
	uint32_t owner = structural_owner;
	if (constructor_owner_view != PROTOTYPE_INVALID_ID) {
		int same_core;
		if (constructor_owner_view >= terms->term_count ||
			prototype_term_core_shape_equal(
				terms, structural_owner, constructor_owner_view, &same_core
			) != 0 || !same_core) {
			return -1;
		}
		owner = constructor_owner_view;
	}
	uint32_t type_id;
	uint32_t owner_arguments[64];
	uint32_t owner_argument_count;
	if (prototype_type_declaration_instance_info(
			type_declarations,
			terms,
			owner,
			&type_id,
			owner_arguments,
			64,
			&owner_argument_count
		) != 0) {
		return -1;
	}
	if (type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			type_declarations->constructor_count ||
		owner_argument_count != type->parameter_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	uint32_t field_context_path[64];
	uint32_t declared_field_count;
	if (prototype_context_extension_path(
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			field_context_path,
			64,
			&declared_field_count
		) != 0 || field_count > declared_field_count) {
		return -1;
	}
	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			contexts,
			substitutions,
			terms,
			type_declarations,
			source_context,
			constructor->parameter_context,
			owner_arguments,
			owner_argument_count,
			&substitution
		) != 0) {
		return -1;
	}

	uint32_t residual_classifier = constructor->curried_classifier_cache;
	for (uint32_t i = 0; i < owner_argument_count; ++i) {
		if (prototype_term_app(
				terms, residual_classifier, owner_arguments[i], &residual_classifier
			) != 0) {
			return -1;
		}
	}
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			residual_classifier,
			&residual_classifier
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		const struct prototype_context* field_context =
			prototype_context_get(contexts, field_context_path[i]);
		uint32_t expected_classifier;
		uint32_t normalized_expected;
		uint32_t next_residual;
		if (!field_context) {
			return -1;
		}
		if (prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				field_context->classifier,
				substitution,
				&expected_classifier
			) != 0) {
			return -1;
		}
		if (prototype_judgement_classifier_value_whnf(
				terms, type_declarations, expected_classifier, &normalized_expected
			) != 0) {
			return -1;
		}
		if (!prototype_judgement_classifier_reference_equal(
				terms,
				type_declarations,
				normalized_expected,
				argument_classifiers[i]
			)) {
			return -1;
		}
		if (pi_codomain_after_argument_in_context(
				contexts,
				substitutions,
				terms,
				type_declarations,
				source_context,
				residual_classifier,
				field_terms[i],
				argument_classifiers[i],
				PROTOTYPE_INVALID_ID,
				&next_residual
			) != 0) {
			return -1;
		}
		if (prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				type_declarations,
				substitution,
				field_context_path[i],
				field_terms[i],
				argument_classifiers[i],
				PROTOTYPE_INVALID_ID,
				&substitution
			) != 0) {
			return -1;
		}
		residual_classifier = next_residual;
	}

	uint32_t classifier = residual_classifier;
	*p_saturated = field_count == declared_field_count;
	if (*p_saturated && prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			constructor->result_classifier,
			substitution,
			&classifier
		) != 0) {
		return -1;
	}
	if (prototype_judgement_classifier_value_whnf(
			terms, type_declarations, classifier, &classifier
		) != 0) {
		return -1;
	}
	(void)constructor_head;
	*p_classifier = classifier;
	return 0;
}

int prototype_judgement_delta_record_constructor_spine(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* argument_classifiers,
	uint32_t argument_classifier_count
) {
	if (!delta || !terms || !type_declarations || !delta->contexts ||
		!delta->substitutions || argument_classifier_count == 0 ||
		argument_classifier_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		!argument_classifiers) {
		return -1;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count;
	uint32_t expected_classifier;
	int saturated;
	if (prototype_term_constructor_spine_info(
			terms,
			subject,
			&head,
			&owner,
			&constructor_index,
			arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&argument_count
		) != 0 || argument_count != argument_classifier_count ||
		prototype_judgement_constructor_spine_classifier(
			terms,
			type_declarations,
			delta->contexts,
			delta->substitutions,
			delta->current_context_id,
			subject,
			owner,
			argument_classifiers,
			argument_classifier_count,
			&expected_classifier,
			&saturated
		) != 0 || !prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, expected_classifier, classifier
		)) {
		return -1;
	}
	(void)head;
	(void)constructor_index;
	(void)saturated;
	if (add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION,
		arguments,
		argument_classifiers,
		argument_count
	) != 0) {
		return -1;
	}
	for (size_t i = delta->relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation = &delta->relations[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject && relation->classifier == classifier &&
			relation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION &&
			relation->proof_id < delta->proof_count) {
			delta->proofs[relation->proof_id].constructor_owner_view = owner;
			return 0;
		}
	}
	return -1;
}

int prototype_judgement_synthesize_match_pattern_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	int constructor_symbol_id,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		scrutinee >= terms->term_count ||
		scrutinee_classifier >= terms->term_count) {
		return -1;
	}

	uint32_t known_scrutinee_classifier;
	if (lookup_delta_classifier_normalization_equal(
			delta,
			terms,
			type_declarations,
			scrutinee,
			scrutinee_classifier,
			&known_scrutinee_classifier
		) != 0) {
		return -1;
	}
	(void)known_scrutinee_classifier;

	uint32_t type_id;
	uint32_t ignored_args[16];
	uint32_t ignored_arg_count;
	if (prototype_term_type_instance_info(
		terms,
		scrutinee_classifier,
		&type_id,
		ignored_args,
		&ignored_arg_count
	) != 0) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(type_declarations, type_id, constructor_symbol_id);
	if (!constructor) {
		return -1;
	}
	return constructor_field_classifier_from_spine(
		delta,
		terms,
		type_declarations,
		scrutinee_classifier,
		constructor->constructor_index,
		NULL,
		0,
		field_index,
		p_classifier
	);
}

int prototype_judgement_resolve_match_case_request(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_resolution_request* request,
	struct prototype_match_constructor_resolution* p_resolution
) {
	if (!delta || !terms || !type_declarations || !request || !p_resolution ||
		request->match_term >= terms->term_count ||
		terms->terms[request->match_term].tag != PROTOTYPE_TERM_MATCH ||
		request->case_index >= terms->terms[request->match_term].as.match.case_count) {
		return -1;
	}

	struct prototype_match_constructor_resolution resolution;
	if (prototype_judgement_resolve_match_constructor(
		terms,
		type_declarations,
		delta->contexts,
		request->scrutinee_classifier,
		request->constructor_symbol_id,
		&resolution
	) != 0) {
		return -1;
	}

	uint32_t case_id = terms->terms[request->match_term].as.match.first_case + request->case_index;
	if (case_id >= terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* match_case = &terms->cases[case_id];
	if (match_case->binder_count != resolution.field_count) {
		return -1;
	}
	if (prototype_term_resolve_match_case(
		terms,
		request->match_term,
		request->case_index,
		resolution.constructor_owner,
		resolution.constructor_id
	) != 0) {
		return -1;
	}

		for (uint32_t j = 0; j < match_case->binder_count; ++j) {
		struct prototype_case_binder* binder =
			&terms->case_binders[match_case->first_binder + j];
			uint32_t binder_var;
			uint32_t binder_classifier;
			uint32_t binder_proof_id;
			if (prototype_term_var(
					terms,
					binder->binder_id,
					&binder_var
				) != 0 ||
				constructor_field_classifier_from_spine(
					delta,
					terms,
					type_declarations,
					request->scrutinee_classifier,
					resolution.constructor_id,
					&terms->case_binders[match_case->first_binder],
					j,
					j,
					&binder_classifier
				) != 0 ||
				prototype_judgement_delta_expand_match_pattern(
					delta,
					terms,
					binder_var,
					binder_classifier
				) != 0 ||
				delta->relation_count == 0) {
				return -1;
			}
			binder->is_recursive = prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				binder_classifier,
				request->scrutinee_classifier
			);
			binder_proof_id =
				delta->relations[delta->relation_count - 1].proof_id;
			if (prototype_judgement_delta_set_match_pattern_owner_by_id(
					delta,
					binder_proof_id,
					request->scrutinee_classifier,
					resolution.constructor_id,
					j
				) != 0) {
				return -1;
			}
		}

	*p_resolution = resolution;
	return 0;
}

static int match_case_binder_is_recursive_self_field(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	uint32_t binder_id
) {
	if (!delta || !terms || !type_declarations || !match_case ||
		match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		match_case->constructor_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}

	uint32_t field_index = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		const struct prototype_case_binder* binder =
			&terms->case_binders[match_case->first_binder + i];
		if (binder->binder_id == binder_id) {
			field_index = i;
			break;
		}
	}
	if (field_index == PROTOTYPE_INVALID_ID) {
		return 0;
	}

	uint32_t field_classifier;
	if (constructor_field_classifier_from_spine(
			delta,
			terms,
			type_declarations,
			match_case->constructor_owner,
			match_case->constructor_id,
			&terms->case_binders[match_case->first_binder],
			field_index,
			field_index,
			&field_classifier
		) != 0) {
		return -1;
	}
	return prototype_judgement_classifier_normalization_equal(
		terms,
		type_declarations,
		field_classifier,
		match_case->constructor_owner
	) ? 0 : -1;
}

static int induction_hypothesis_classifier_from_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t argument,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH) ||
			argument >= terms->term_count) {
		return -1;
	}
	uint32_t match_classifiers[32];
	uint32_t match_classifier_count = 0;
	for (int source = 0; source < 2; ++source) {
		if (source == 1 && !delta->db) {
			continue;
		}
		const struct prototype_judgement_relation* relations =
			source == 0 ? delta->relations : delta->db->relations;
		size_t relation_count =
			source == 0 ? delta->relation_count : delta->db->relation_count;
		for (size_t i = 0; i < relation_count; ++i) {
			const struct prototype_judgement_relation* relation = &relations[i];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != match_term ||
				relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION ||
				!match_motive_result_classifier(terms, match_term, relation->classifier)) {
				continue;
			}
			if (classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					match_classifiers,
					match_classifier_count,
					relation->classifier
				)) {
				continue;
			}
			if (match_classifier_count >= 32) {
				return -1;
			}
			match_classifiers[match_classifier_count++] = relation->classifier;
		}
	}
	for (size_t i = 0; i < delta->match_motive_result_count; ++i) {
		const struct prototype_judgement_match_motive_result* result =
			&delta->match_motive_results[i];
		if (result->match_term != match_term ||
			!match_motive_result_classifier(terms, match_term, result->classifier)) {
			continue;
		}
		if (classifier_list_contains_normalization_equal(
				terms,
				type_declarations,
				match_classifiers,
				match_classifier_count,
				result->classifier
			)) {
			continue;
		}
		if (match_classifier_count >= 32) {
			return -1;
		}
		match_classifiers[match_classifier_count++] = result->classifier;
	}
	if (match_classifier_count == 0) {
		return 1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match_classifier_count; ++i) {
		uint32_t match_classifier = match_classifiers[i];
		if (!term_has_tag(terms, match_classifier, PROTOTYPE_TERM_APP)) {
			continue;
		}
		const struct prototype_term* motive_app = &terms->terms[match_classifier];
		if (motive_app->as.app.argument != match->as.match.scrutinee ||
			!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
			continue;
		}
		uint32_t candidate;
		if (prototype_term_app(
				terms,
				motive_app->as.app.function,
				argument,
				&candidate
			) != 0) {
			return -1;
		}
		if (selected != PROTOTYPE_INVALID_ID &&
			!prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				selected,
				candidate
			)) {
			return -1;
		}
		selected = candidate;
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = selected;
	(void)delta;
	return 0;
}

static int induction_hypothesis_context_from_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t argument,
	uint32_t* p_context_case_index,
	uint32_t* p_context_field_index
) {
	if (!delta || !terms || !type_declarations || !p_context_case_index ||
		!p_context_field_index ||
		!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH) ||
		argument >= terms->term_count ||
		terms->terms[argument].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t binder_id = terms->terms[argument].as.var.binder_id;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		int recursive = match_case_binder_is_recursive_self_field(
			delta,
			terms,
			type_declarations,
			&terms->cases[case_id],
			binder_id
		);
		if (recursive < 0) {
			return -1;
		}
		if (recursive > 0) {
			continue;
		}
		for (uint32_t j = 0; j < terms->cases[case_id].binder_count; ++j) {
			if (terms->case_binders[terms->cases[case_id].first_binder + j].binder_id ==
				binder_id) {
				*p_context_case_index = i;
				*p_context_field_index = j;
				return 0;
			}
		}
	}
	return 1;
}

static int collect_judgement_subject_classifiers(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
);
struct app_classifier_candidate {
	uint32_t function_classifier;
	uint32_t argument_classifier;
	uint32_t function_pi;
	uint32_t result_classifier;
};

static int collect_app_classifier_candidates_from_candidates(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t context_id,
	uint32_t argument,
	const uint32_t* function_candidates,
	uint32_t function_candidate_count,
	const uint32_t* argument_candidates,
	uint32_t argument_candidate_count,
	struct app_classifier_candidate* candidates,
	uint32_t candidate_capacity,
	uint32_t* p_candidate_count
) {
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!function_candidates || !argument_candidates ||
		!candidates || !p_candidate_count ||
		argument >= terms->term_count) {
		return -1;
	}
	*p_candidate_count = 0;
	for (uint32_t i = 0; i < function_candidate_count; ++i) {
		uint32_t function_pi;
		uint32_t domain;
		uint32_t codomain_family;
		int status = classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			function_candidates[i],
			&function_pi,
			&domain,
			&codomain_family
		);
		(void)codomain_family;
		if (status < 0) {
			return -1;
		}
		if (status > 0) {
			continue;
		}
		for (uint32_t j = 0; j < argument_candidate_count; ++j) {
			uint32_t result_classifier;
			if (!prototype_judgement_classifier_compatible(
					terms,
					type_declarations,
					domain,
					argument_candidates[j]
				)) {
				continue;
			}
			if (pi_codomain_after_argument_in_context(
					contexts,
					substitutions,
					terms,
					type_declarations,
					context_id,
					function_pi,
					argument,
					argument_candidates[j],
					PROTOTYPE_INVALID_ID,
					&result_classifier
				) != 0) {
				return -1;
			}
			int duplicate = 0;
			for (uint32_t k = 0; k < *p_candidate_count; ++k) {
				if (prototype_judgement_classifier_normalization_equal(
						terms,
						type_declarations,
						candidates[k].function_classifier,
						function_candidates[i]
					) &&
					prototype_judgement_classifier_normalization_equal(
						terms,
						type_declarations,
						candidates[k].argument_classifier,
						argument_candidates[j]
					) &&
					prototype_judgement_classifier_normalization_equal(
						terms,
						type_declarations,
						candidates[k].result_classifier,
						result_classifier
					)) {
					duplicate = 1;
					break;
				}
			}
			if (duplicate) {
				continue;
			}
			if (*p_candidate_count >= candidate_capacity) {
				return -1;
			}
			candidates[*p_candidate_count].function_classifier = function_candidates[i];
			candidates[*p_candidate_count].argument_classifier = argument_candidates[j];
			candidates[*p_candidate_count].function_pi = function_pi;
			candidates[*p_candidate_count].result_classifier = result_classifier;
			(*p_candidate_count)++;
		}
	}
	return 0;
}

static int collect_delta_app_classifier_candidates(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t function,
	uint32_t argument,
	struct app_classifier_candidate* candidates,
	uint32_t candidate_capacity,
	uint32_t* p_candidate_count
) {
	if (!delta || !terms || !type_declarations ||
		function >= terms->term_count ||
		argument >= terms->term_count) {
		return -1;
	}
	uint32_t function_candidates[32];
	uint32_t argument_candidates[32];
	uint32_t function_candidate_count = 0;
	uint32_t argument_candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			function,
			function_candidates,
			32,
			&function_candidate_count
		) != 0 ||
		collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			argument,
			argument_candidates,
			32,
			&argument_candidate_count
		) != 0) {
		return -1;
	}
	if (function_candidate_count == 0 || argument_candidate_count == 0) {
		return 1;
	}
	if (collect_app_classifier_candidates_from_candidates(
			delta->contexts,
			delta->substitutions,
			terms,
			type_declarations,
			delta->current_context_id,
			argument,
		function_candidates,
			function_candidate_count,
			argument_candidates,
			argument_candidate_count,
			candidates,
			candidate_capacity,
			p_candidate_count
		) != 0) {
		return -1;
	}
	return *p_candidate_count == 0 ? 1 : 0;
}

static int prototype_judgement_delta_resolve_induction_hypothesis_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		argument_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* ih = &terms->terms[subject];
	if (ih->as.induction_hypothesis.frame_id >= terms->match_frame_count ||
		ih->as.induction_hypothesis.argument >= terms->term_count) {
		return -1;
	}
	uint32_t match_term =
		terms->match_frames[ih->as.induction_hypothesis.frame_id].match_term;
	uint32_t context_case_index = PROTOTYPE_INVALID_ID;
	uint32_t context_field_index = PROTOTYPE_INVALID_ID;
	int context_status = induction_hypothesis_context_from_argument(
		delta,
		terms,
		type_declarations,
		match_term,
		ih->as.induction_hypothesis.argument,
		&context_case_index,
		&context_field_index
	);
	if (context_status != 0) {
		return context_status < 0 ? -1 : 1;
	}
	uint32_t classifier;
	int status = induction_hypothesis_classifier_from_match_motive(
		delta,
		terms,
		type_declarations,
		match_term,
		ih->as.induction_hypothesis.argument,
		&classifier
	);
	if (status < 0) {
		return -1;
	}
	if (status > 0) {
		(void)argument_classifier;
		return 1;
	}
	return prototype_judgement_delta_expand_induction_hypothesis(
		delta,
		terms,
		subject,
		classifier,
		match_term,
		context_case_index,
		context_field_index
	);
}

int prototype_judgement_delta_resolve_induction_hypothesis_request(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_induction_hypothesis_resolution_request* request
) {
	if (!delta || !terms || !type_declarations || !request ||
		!term_has_tag(terms, request->subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		request->argument >= terms->term_count ||
		terms->terms[request->argument].tag != PROTOTYPE_TERM_VAR ||
		request->frame_id >= terms->match_frame_count) {
		return -1;
	}
	const struct prototype_term* subject = &terms->terms[request->subject];
	if (subject->as.induction_hypothesis.frame_id != request->frame_id ||
		subject->as.induction_hypothesis.argument != request->argument) {
		return -1;
	}
	uint32_t ignored_existing_classifier;
	int has_existing_classifier =
		lookup_delta_proven_classifier(
			delta,
			terms,
			request->subject,
			&ignored_existing_classifier
		) == 0;

	uint32_t match_term = terms->match_frames[request->frame_id].match_term;
	if (!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH)) {
		return 1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t binder_id = terms->terms[request->argument].as.var.binder_id;
	int found = 0;
	uint32_t context_case_index = PROTOTYPE_INVALID_ID;
	uint32_t context_field_index = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		int recursive = match_case_binder_is_recursive_self_field(
			delta,
			terms,
			type_declarations,
			&terms->cases[case_id],
			binder_id
		);
		if (recursive < 0) {
			return -1;
		}
		if (recursive == 1) {
			return 1;
		}
		if (recursive == 0) {
			for (uint32_t j = 0; j < terms->cases[case_id].binder_count; ++j) {
				if (terms->case_binders[terms->cases[case_id].first_binder + j].binder_id ==
					binder_id) {
					found = 1;
					context_case_index = i;
					context_field_index = j;
					break;
				}
			}
		}
	}
	if (!found) {
		return -1;
	}

	uint32_t classifier;
	int status = induction_hypothesis_classifier_from_match_motive(
		delta,
		terms,
		type_declarations,
		match_term,
		request->argument,
		&classifier
	);
	if (status < 0) {
		return -1;
	}
	if (status > 0) {
		if (has_existing_classifier) {
			return 0;
		}
		return 1;
	}
	if (status != 0) {
		return status;
	}
		if (has_existing_classifier &&
			lookup_delta_classifier_normalization_equal(
				delta,
				terms,
			type_declarations,
			request->subject,
				classifier,
				&ignored_existing_classifier
			) == 0) {
			return 0;
		}
		(void)has_existing_classifier;
		return prototype_judgement_delta_expand_induction_hypothesis(
		delta,
		terms,
		request->subject,
		classifier,
		match_term,
		context_case_index,
		context_field_index
	);
}

static int type_instance_has_known_type(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	return prototype_term_type_instance_is_saturated(terms, type_declarations, term_id);
}

static int term_is_structural_type(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!term_exists(terms, term_id)) {
		return 0;
	}
		return term_is_universe_var(terms, term_id) ||
			term_is_primitive_text(terms, term_id) ||
			term_is_primitive_int(terms, term_id) ||
			term_is_primitive_int64(terms, term_id) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_EXTERNAL_REF) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_PI) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_COMPUTATION_TYPE) ||
			term_has_tag(terms, term_id, PROTOTYPE_TERM_THUNK_TYPE) ||
		type_instance_has_known_type(terms, type_declarations, term_id);
}

static int infer_type_formation_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !delta->db || !terms || !type_declarations ||
		!type_instance_has_known_type(terms, type_declarations, term_id)) {
		return -1;
	}
	uint32_t existing_classifier;
	if (lookup_delta_proven_classifier(
			delta, terms, term_id, &existing_classifier
		) == 0 && term_is_universe_var(terms, existing_classifier)) {
		return 0;
	}
	uint32_t classifier;
	if (prototype_term_universe_var(
			terms,
			delta->db->next_universe_var++,
			&classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
	);
}

static int classifier_returns_owner(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t owner
) {
	(void)type_declarations;
	uint32_t current = classifier;
	while (current < terms->term_count) {
		int same_owner = 0;
		if (current == owner) {
			return 1;
		}
		if (prototype_term_core_shape_equal(
				terms, current, owner, &same_owner
			) != 0) {
			return 0;
		}
		if (same_owner) {
			return 1;
		}
		if (prototype_term_source_shape_equal(
				terms, current, owner, &same_owner
			) != 0) {
			return 0;
		}
		if (same_owner) {
			return 1;
		}
		uint32_t domain;
		uint32_t codomain_lambda;
		if (pi_parts(terms, current, &domain, &codomain_lambda) != 0) {
			return 0;
		}
		(void)domain;
		(void)codomain_lambda;
		uint32_t ignored_binder;
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[current].as.pi.codomain_family,
				&ignored_binder,
				&current
			) != 0) {
			return 0;
		}
	}
	return 0;
}

static int constructor_belongs_to_owner(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t owner,
	uint32_t constructor_index
) {
	if (!terms || !type_declarations || owner >= terms->term_count) {
		return 0;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(
			terms, owner, &type_id, args, &arg_count
		) != 0) {
		uint32_t reversed[16];
		uint32_t count = 0;
		uint32_t current = owner;
		while (current < terms->term_count && terms->terms[current].tag == PROTOTYPE_TERM_APP) {
			if (count >= 16) {
				return 0;
			}
			reversed[count++] = terms->terms[current].as.app.argument;
			current = terms->terms[current].as.app.function;
		}
		if (current >= terms->term_count ||
			terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER ||
			prototype_type_declaration_representation_type_id(
				type_declarations,
				terms->terms[current].as.type_former.representation_id,
				&type_id
			) != 0) {
			return 0;
		}
		for (uint32_t i = 0; i < count; ++i) {
			args[i] = reversed[count - i - 1];
		}
		arg_count = count;
	}
	if (type_id >= type_declarations->type_count ||
		arg_count != type_declarations->type_declarations[type_id].parameter_count) {
		return 0;
	}
	const struct prototype_type_declaration* type = &type_declarations->type_declarations[type_id];
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == constructor_index) {
			return 1;
		}
	}
	return 0;
}

static int match_case_has_valid_constructor(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case
) {
	if (!terms || !type_declarations || !match_case ||
		match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		match_case->constructor_id == PROTOTYPE_INVALID_ID ||
		!term_exists(terms, match_case->constructor_owner)) {
		return 0;
	}
	if (constructor_belongs_to_owner(
			terms,
			type_declarations,
			match_case->constructor_owner,
			match_case->constructor_id
		)) {
		return 1;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(
			terms,
			match_case->constructor_owner,
			&type_id,
			args,
			&arg_count
		) == 0 &&
		type_id < type_declarations->type_count) {
		return 0;
	}
	(void)type_id;
	(void)args;
	(void)arg_count;
	return 1;
}

static int type_formation_is_nat_shape(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	uint32_t term_id
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations || !contexts ||
		prototype_term_type_instance_info(terms, term_id, &type_id, args, &arg_count) != 0 ||
		type_id >= type_declarations->type_count ||
		arg_count != 0) {
		return 0;
	}
	(void)args;
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count != 0 || type->constructor_count != 2) {
		return 0;
	}
	const struct prototype_type_constructor_declaration* zero = NULL;
	const struct prototype_type_constructor_declaration* succ = NULL;
	for (uint32_t i = 0; i < type->constructor_count; ++i) {
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[type->first_constructor + i];
		if (constructor->constructor_index == 0) {
			zero = constructor;
		} else if (constructor->constructor_index == 1) {
			succ = constructor;
		}
	}
	if (!zero || !succ ||
		zero->owner_type != type_id ||
		succ->owner_type != type_id) {
		return 0;
	}

	uint32_t zero_fields[1];
	uint32_t zero_field_count;
	uint32_t succ_fields[1];
	uint32_t succ_field_count;
	if (prototype_context_extension_path(
			contexts,
			zero->parameter_context,
			zero->field_context,
			zero_fields,
			1,
			&zero_field_count
		) != 0 ||
		prototype_context_extension_path(
			contexts,
			succ->parameter_context,
			succ->field_context,
			succ_fields,
			1,
			&succ_field_count
		) != 0) {
		return 0;
	}
	if (zero_field_count != 0 || succ_field_count != 1) {
		return 0;
	}
	const struct prototype_context* succ_field =
		prototype_context_get(contexts, succ_fields[0]);
	if (!succ_field || succ_field->classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (!prototype_judgement_classifier_normalization_equal(
			(struct prototype_term_db*)terms,
			(struct prototype_type_declaration_db*)type_declarations,
			succ_field->classifier,
			term_id
		)) {
		return 0;
	}
	return 1;
}

static int type_formation_has_name_symbol(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	int name_symbol_id
) {
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (!terms || !type_declarations || name_symbol_id < 0 ||
		prototype_term_type_instance_info(terms, term_id, &type_id, args, &arg_count) != 0 ||
		type_id >= type_declarations->type_count ||
		arg_count != 0) {
		return 0;
	}
	(void)args;
	return type_declarations->type_declarations[type_id].name_symbol_id == name_symbol_id;
}

static int add_relation_with_premises(
	struct prototype_judgement_db* judgement,
	uint32_t context_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
) {
	if (!judgement ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		reserve_slot(judgement->relation_count, judgement->relation_capacity) != 0 ||
		reserve_slot(judgement->proof_count, judgement->proof_capacity) != 0) {
		return -1;
	}

	if (!proof_kind_requires_local_context(proof_kind)) {
		for (size_t i = 0; i < judgement->relation_count; ++i) {
			if (judgement->relations[i].kind == kind &&
				judgement->relations[i].context_id == context_id &&
				judgement->relations[i].subject == subject &&
				judgement->relations[i].classifier == classifier &&
				judgement->relations[i].proof_kind == proof_kind) {
				uint32_t proof_id = judgement->relations[i].proof_id;
				if (proof_id < judgement->proof_count) {
					memset(&judgement->proofs[proof_id], 0, sizeof(judgement->proofs[proof_id]));
					judgement->proofs[proof_id].proof_kind = proof_kind;
					judgement->proofs[proof_id].conclusion_kind = kind;
					judgement->proofs[proof_id].conclusion_context_id = context_id;
					judgement->proofs[proof_id].conclusion_subject = subject;
					judgement->proofs[proof_id].conclusion_classifier = classifier;
					initialize_proof_rule_parameters(
						&judgement->proofs[proof_id]
					);
					set_proof_premises(
						&judgement->proofs[proof_id],
						judgement,
						premise_context_ids,
						premise_subjects,
						premise_classifiers,
						premise_count
					);
				}
				return 0;
			}
		}
	}

	uint32_t id = (uint32_t)judgement->relation_count;
	uint32_t proof_id = (uint32_t)judgement->proof_count;
	judgement->relations[id].kind = kind;
	judgement->relations[id].context_id = context_id;
	judgement->relations[id].subject = subject;
	judgement->relations[id].classifier = classifier;
	judgement->relations[id].proof_kind = proof_kind;
	judgement->relations[id].proof_id = proof_id;
	memset(&judgement->proofs[proof_id], 0, sizeof(judgement->proofs[proof_id]));
	judgement->proofs[proof_id].proof_kind = proof_kind;
	judgement->proofs[proof_id].conclusion_kind = kind;
	judgement->proofs[proof_id].conclusion_context_id = context_id;
	judgement->proofs[proof_id].conclusion_subject = subject;
	judgement->proofs[proof_id].conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&judgement->proofs[proof_id]);
	set_proof_premises(
		&judgement->proofs[proof_id],
		judgement,
		premise_context_ids,
		premise_subjects,
		premise_classifiers,
		premise_count
	);
	judgement->relation_count++;
	judgement->proof_count++;
	return 0;
}

static int add_relation(
	struct prototype_judgement_db* judgement,
	uint32_t context_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind
) {
	return add_relation_with_premises(
		judgement,
		context_id,
		kind,
		subject,
		classifier,
		proof_kind,
		NULL,
		NULL,
		NULL,
		0
	);
}

static int add_delta_relation_with_premises(
	struct prototype_judgement_delta* delta,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
) {
	if (!delta ||
		premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		reserve_slot(delta->relation_count, delta->relation_capacity) != 0 ||
		reserve_slot(delta->proof_count, delta->proof_capacity) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)delta->relation_count;
	if (!proof_kind_requires_local_context(proof_kind)) {
		for (size_t i = 0; i < delta->relation_count; ++i) {
			if (delta->relations[i].kind == kind &&
				delta->relations[i].context_id == delta->current_context_id &&
				delta->relations[i].subject == subject &&
					delta->relations[i].classifier == classifier &&
					delta->relations[i].proof_kind == proof_kind) {
					uint32_t proof_id = delta->relations[i].proof_id;
					if (proof_id < delta->proof_count) {
						uint32_t constructor_owner_view =
							delta->proofs[proof_id].constructor_owner_view;
						memset(&delta->proofs[proof_id], 0, sizeof(delta->proofs[proof_id]));
					delta->proofs[proof_id].proof_kind = proof_kind;
					delta->proofs[proof_id].conclusion_kind = kind;
					delta->proofs[proof_id].conclusion_context_id =
						delta->current_context_id;
					delta->proofs[proof_id].conclusion_subject = subject;
					delta->proofs[proof_id].conclusion_classifier = classifier;
						initialize_proof_rule_parameters(
							&delta->proofs[proof_id]
						);
						if (proof_kind ==
							PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
							delta->proofs[proof_id].constructor_owner_view =
								constructor_owner_view;
						}
					if (proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
						const struct prototype_context* context =
							prototype_context_get(
								delta->contexts, delta->current_context_id
							);
						if (!context || context->depth == 0) {
							return -1;
						}
						delta->proofs[proof_id].assumption_index =
							context->depth - 1;
					}
					delta->proofs[proof_id].premise_count = premise_count;
					for (uint32_t j = 0; j < premise_count; ++j) {
						delta->proofs[proof_id].premise_kinds[j] =
							PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
						delta->proofs[proof_id].premise_context_ids[j] =
							delta->current_context_id;
						delta->proofs[proof_id].premise_subjects[j] = premise_subjects[j];
						delta->proofs[proof_id].premise_classifiers[j] = premise_classifiers[j];
						delta->proofs[proof_id].premise_proof_ids[j] = PROTOTYPE_INVALID_ID;
					}
				}
				return 0;
			}
		}
	}
	uint32_t proof_id = (uint32_t)delta->proof_count;
	delta->relations[id].kind = kind;
	delta->relations[id].context_id = delta->current_context_id;
	delta->relations[id].subject = subject;
	delta->relations[id].classifier = classifier;
	delta->relations[id].proof_kind = proof_kind;
	delta->relations[id].proof_id = proof_id;
	memset(&delta->proofs[proof_id], 0, sizeof(delta->proofs[proof_id]));
	delta->proofs[proof_id].proof_kind = proof_kind;
	delta->proofs[proof_id].conclusion_kind = kind;
	delta->proofs[proof_id].conclusion_context_id = delta->current_context_id;
	delta->proofs[proof_id].conclusion_subject = subject;
	delta->proofs[proof_id].conclusion_classifier = classifier;
	initialize_proof_rule_parameters(&delta->proofs[proof_id]);
	if (proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
		const struct prototype_context* context =
			prototype_context_get(delta->contexts, delta->current_context_id);
		if (!context || context->depth == 0) {
			return -1;
		}
		delta->proofs[proof_id].assumption_index = context->depth - 1;
	}
	delta->proofs[proof_id].premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		delta->proofs[proof_id].premise_kinds[i] =
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		delta->proofs[proof_id].premise_context_ids[i] =
			delta->current_context_id;
		delta->proofs[proof_id].premise_subjects[i] = premise_subjects[i];
		delta->proofs[proof_id].premise_classifiers[i] = premise_classifiers[i];
		delta->proofs[proof_id].premise_proof_ids[i] = PROTOTYPE_INVALID_ID;
	}
	delta->relation_count++;
	delta->proof_count++;
	return 0;
}

static int add_delta_relation(
	struct prototype_judgement_delta* delta,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind
) {
	return add_delta_relation_with_premises(
		delta,
		kind,
		subject,
		classifier,
		proof_kind,
		NULL,
		NULL,
		0
	);
}

static int add_match_motive_result(
	struct prototype_judgement_delta* delta,
	uint32_t match_term,
	uint32_t classifier
) {
	if (!delta ||
		!delta->match_motive_results ||
		reserve_slot(
			delta->match_motive_result_count,
			delta->match_motive_result_capacity
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < delta->match_motive_result_count; ++i) {
		if (delta->match_motive_results[i].match_term == match_term &&
			delta->match_motive_results[i].classifier == classifier) {
			return 0;
		}
	}
	uint32_t id = (uint32_t)delta->match_motive_result_count;
	delta->match_motive_results[id].match_term = match_term;
	delta->match_motive_results[id].classifier = classifier;
	delta->match_motive_result_count++;
	return 0;
}

int prototype_judgement_delta_record_materialized_match_motive(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t classifier
) {
	if (!delta || !terms || match_term >= terms->term_count ||
		classifier >= terms->term_count ||
		!match_motive_result_classifier(terms, match_term, classifier)) {
		return -1;
	}
	if (add_match_motive_result(delta, match_term, classifier) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		match_term,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE
	);
}

static void remove_match_motive_results_normalization_equal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t classifier
) {
	if (!delta || !delta->match_motive_results || !terms || !type_declarations) {
		return;
	}
	size_t write = 0;
	for (size_t read = 0; read < delta->match_motive_result_count; ++read) {
		if (delta->match_motive_results[read].match_term == match_term &&
			prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				classifier,
				delta->match_motive_results[read].classifier
			)) {
			continue;
		}
		if (write != read) {
			delta->match_motive_results[write] =
				delta->match_motive_results[read];
		}
		write++;
	}
	delta->match_motive_result_count = write;
}

int prototype_judgement_delta_commit(
	struct prototype_judgement_delta* delta,
	size_t mark
) {
	if (!delta || !delta->db || mark > delta->relation_count) {
		return -1;
	}
	for (size_t i = mark; i < delta->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &delta->relations[i];
		if (relation->proof_id >= delta->proof_count) {
			return -1;
		}
		const struct prototype_judgement_proof* proof = &delta->proofs[relation->proof_id];
		if (add_relation_with_premises(
				delta->db,
				relation->context_id,
				relation->kind,
				relation->subject,
				relation->classifier,
				relation->proof_kind,
				proof->premise_context_ids,
				proof->premise_subjects,
				proof->premise_classifiers,
				proof->premise_count
			) != 0) {
			return -1;
		}
		if (copy_db_relation_rule_parameters(
				delta->db,
				relation->kind,
				relation->subject,
				relation->classifier,
				relation->proof_kind,
				proof
			) != 0) {
			return -1;
		}
	}
	prototype_judgement_resolve_proof_edges(delta->db);
	delta->relation_count = mark;
	delta->proof_count = mark;
	return 0;
}

int prototype_judgement_expand_type_def(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!type_instance_has_known_type(terms, type_declarations, subject) ||
		!term_is_universe_var(terms, classifier)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO);
}

int prototype_judgement_expand_constructor_def(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_CONSTRUCTOR) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	const struct prototype_term* constructor = &terms->terms[subject];
	if (!constructor_belongs_to_owner(
			terms,
			type_declarations,
			constructor->as.constructor.owner,
			constructor->as.constructor.constructor_id
		) ||
		!classifier_returns_owner(
			terms,
			type_declarations,
			classifier,
			constructor->as.constructor.owner
		)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO);
}

int prototype_judgement_delta_expand_lambda_binder(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_VAR) ||
		classifier >= terms->term_count) {
		return -1;
	}
	return add_delta_relation(delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION);
}

int prototype_judgement_delta_expand_match_pattern(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_VAR) ||
		classifier >= terms->term_count) {
		return -1;
	}
	return add_delta_relation(delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION);
}

int prototype_judgement_delta_set_match_pattern_owner_by_id(
	struct prototype_judgement_delta* delta,
	uint32_t proof_id,
	uint32_t constructor_owner_view,
	uint32_t constructor_index,
	uint32_t constructor_field_index
) {
	if (!delta || proof_id >= delta->proof_count ||
		constructor_owner_view == PROTOTYPE_INVALID_ID ||
		constructor_index == PROTOTYPE_INVALID_ID ||
		constructor_field_index == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_judgement_proof* proof = &delta->proofs[proof_id];
	if (proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
		return -1;
	}
	proof->constructor_owner_view = constructor_owner_view;
	proof->constructor_index = constructor_index;
	proof->constructor_field_index = constructor_field_index;
	return 0;
}

int prototype_judgement_expand_lambda(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement) {
		return -1;
	}
	struct prototype_judgement_relation relations[4];
	struct prototype_judgement_proof proofs[4];
	struct prototype_judgement_match_motive_result motives[1];
	struct prototype_judgement_computation_constraint constraints[1];
	struct prototype_judgement_effect_row_constraint effect_rows[1];
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta, judgement, relations, proofs, 4, motives, 1, constraints, 1, effect_rows, 1
	);
	int status = prototype_judgement_delta_expand_lambda(
		&delta, terms, type_declarations, subject, classifier
	);
	return status == 0 ? prototype_judgement_delta_commit(&delta, 0) : status;
}

int prototype_judgement_delta_expand_lambda(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[subject];
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t binder_classifier;
	uint32_t body_classifier;
	uint32_t expected_body_classifier;
	uint32_t binder_var;
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		classifier,
		&lambda_pi,
		&domain,
		&codomain_family
	);
	(void)codomain_family;
	if (status != 0) {
		return -1;
	}
	if (prototype_term_var(terms, lambda->as.lambda.binder_id, &binder_var) != 0) {
		return -1;
	}
	if (pi_codomain_at_fresh_binder(
			delta,
			terms,
			type_declarations,
			delta->current_context_id,
			lambda_pi,
			binder_var,
			domain,
			PROTOTYPE_INVALID_ID,
			&expected_body_classifier
		) != 0) {
		return -1;
	}
	if (lookup_delta_proven_classifier_normalization_equal(
			delta,
			terms,
			type_declarations,
			lambda->as.lambda.body,
			expected_body_classifier,
			&body_classifier
		) != 0) {
		return -1;
	}
	for (size_t i = delta->relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation = &delta->relations[i - 1];
		if (relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION ||
			relation->subject >= terms->term_count ||
			terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
			terms->terms[relation->subject].as.var.binder_id != lambda->as.lambda.binder_id) {
			continue;
		}
		binder_classifier = relation->classifier;
			if (!prototype_judgement_classifier_normalization_equal(terms, type_declarations, domain, binder_classifier)) {
				continue;
			}
		uint32_t premise_subjects[2] = {
			relation->subject,
			lambda->as.lambda.body
		};
		uint32_t premise_classifiers[2] = {
			binder_classifier,
			body_classifier
		};
			int relation_status = add_delta_relation_with_premises(
				delta,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
			classifier,
			PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			premise_subjects,
				premise_classifiers,
				2
			);
				return relation_status;
			}
	if (delta->db) {
		for (size_t i = delta->db->relation_count; i > 0; --i) {
			const struct prototype_judgement_relation* relation = &delta->db->relations[i - 1];
			if (relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION ||
				relation->subject >= terms->term_count ||
				terms->terms[relation->subject].tag != PROTOTYPE_TERM_VAR ||
				terms->terms[relation->subject].as.var.binder_id != lambda->as.lambda.binder_id) {
				continue;
			}
			binder_classifier = relation->classifier;
			if (!prototype_judgement_classifier_normalization_equal(terms, type_declarations, domain, binder_classifier)) {
				continue;
			}
			uint32_t premise_subjects[2] = {
				relation->subject,
				lambda->as.lambda.body
			};
			uint32_t premise_classifiers[2] = {
				binder_classifier,
				body_classifier
			};
			int relation_status = add_delta_relation_with_premises(
				delta,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				classifier,
				PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
				premise_subjects,
				premise_classifiers,
				2
			);
				return relation_status;
			}
	}
	return -1;
}

int prototype_judgement_expand_app(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!judgement) {
		return -1;
	}
	struct prototype_judgement_relation relations[64];
	struct prototype_judgement_proof proofs[64];
	struct prototype_judgement_match_motive_result motives[1];
	struct prototype_judgement_computation_constraint constraints[1];
	struct prototype_judgement_effect_row_constraint effect_rows[1];
	struct prototype_judgement_delta delta;
	prototype_judgement_delta_init(
		&delta, judgement, relations, proofs, 64, motives, 1, constraints, 1, effect_rows, 1
	);
	int status = prototype_judgement_delta_expand_app(
		&delta, terms, type_declarations, subject, p_classifier
	);
	return status == 0 ? prototype_judgement_delta_commit(&delta, 0) : status;
}

static int infer_lambda_classifier_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	uint32_t argument_classifier
);

static int ensure_lambda_binder_assumption(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t lambda_term,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !term_has_tag(terms, lambda_term, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	uint32_t binder_var;
	if (prototype_term_var(terms, lambda->as.lambda.binder_id, &binder_var) != 0) {
		return -1;
	}
	uint32_t binder_context;
	if (!delta->contexts || prototype_context_extend(
			delta->contexts,
			delta->current_context_id,
			lambda->as.lambda.binder_id,
			argument_classifier,
			PROTOTYPE_INVALID_ID,
			&binder_context
		) != 0) {
		return -1;
	}
	for (size_t i = delta->relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation = &delta->relations[i - 1];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION ||
			relation->context_id != binder_context ||
			relation->subject != binder_var ||
			relation->classifier != argument_classifier) {
			continue;
		}
		if (relation->proof_id < delta->proof_count) {
			return 0;
		}
	}
	size_t before = delta->relation_count;
	uint32_t outer_context = delta->current_context_id;
	prototype_judgement_delta_set_context(delta, binder_context);
	if (prototype_judgement_delta_expand_lambda_binder(
			delta, terms, binder_var, argument_classifier
		) != 0 || delta->relation_count != before + 1) {
		prototype_judgement_delta_set_context(delta, outer_context);
		return -1;
	}
	prototype_judgement_delta_set_context(delta, outer_context);
	return 0;
}

int prototype_judgement_delta_expand_app(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	if (!delta || !term_has_tag(terms, subject, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	const struct prototype_term* app = &terms->terms[subject];
	struct app_classifier_candidate candidates[64];
	uint32_t candidate_count = 0;
	int select_status = collect_delta_app_classifier_candidates(
			delta,
			terms,
			type_declarations,
			app->as.app.function,
			app->as.app.argument,
			candidates,
			64,
			&candidate_count
		);
	if (select_status < 0) {
		return -1;
	}
	if (select_status > 0) {
		uint32_t argument_candidates[32];
		uint32_t argument_candidate_count = 0;
		if (collect_subject_classifiers(
				delta,
				terms,
				type_declarations,
				app->as.app.argument,
				argument_candidates,
				32,
				&argument_candidate_count
			) != 0) {
			return -1;
		}
		if (argument_candidate_count == 0) {
			return 1;
		}
		if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
			for (uint32_t i = 0; i < argument_candidate_count; ++i) {
				(void)infer_lambda_classifier_for_app_argument(
					delta,
					terms,
					type_declarations,
					app->as.app.function,
					argument_candidates[i]
				);
			}
		}
		if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS)) {
			for (uint32_t i = 0; i < argument_candidate_count; ++i) {
				int ih_status =
					prototype_judgement_delta_resolve_induction_hypothesis_for_app_argument(
						delta,
						terms,
						type_declarations,
						app->as.app.function,
						argument_candidates[i]
					);
				if (ih_status < 0) {
					return -1;
				}
				}
			}
			select_status = collect_delta_app_classifier_candidates(
					delta,
					terms,
					type_declarations,
					app->as.app.function,
					app->as.app.argument,
					candidates,
					64,
					&candidate_count
				);
			if (select_status < 0) {
				return -1;
		}
		if (select_status > 0) {
				return 1;
			}
		}
		uint32_t first_classifier = candidates[0].result_classifier;
		if (p_classifier) {
			*p_classifier = first_classifier;
		}
		for (uint32_t i = 0; i < candidate_count; ++i) {
			uint32_t premise_subjects[2] = {
				app->as.app.function,
				app->as.app.argument
			};
			uint32_t premise_classifiers[2] = {
				candidates[i].function_classifier,
				candidates[i].argument_classifier
			};
			if (add_delta_relation_with_premises(
					delta,
					PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
					subject,
					candidates[i].result_classifier,
					PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
					premise_subjects,
					premise_classifiers,
					2
				) != 0) {
				return -1;
			}
		}
		return 0;
	}

static int lambda_body_classifier_matches_binder(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t body,
	uint32_t binder_id,
	uint32_t binder_classifier,
	uint32_t body_classifier
) {
	if (!terms || !type_declarations || body >= terms->term_count) {
		return 0;
	}
	if (terms->terms[body].tag == PROTOTYPE_TERM_VAR &&
		terms->terms[body].as.var.binder_id == binder_id) {
		return prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			binder_classifier,
			body_classifier
		);
	}
	if (terms->terms[body].tag == PROTOTYPE_TERM_RETURN) {
		uint32_t value = terms->terms[body].as.return_term.value;
		struct prototype_term_classifier_view view;
		if (value >= terms->term_count) {
			return 0;
		}
		if (terms->terms[value].tag != PROTOTYPE_TERM_VAR ||
			terms->terms[value].as.var.binder_id != binder_id) {
			return 1;
		}
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, body_classifier, &view
			) != 0 || view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return 0;
		}
		return prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, binder_classifier, view.result
		);
	}
	return 1;
}

static int infer_lambda_classifier_pair(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	uint32_t binder_subject,
	uint32_t binder_classifier,
	uint32_t body_classifier,
	int* p_changed
) {
	if (!delta || !terms || !type_declarations || !p_changed ||
		lambda_term >= terms->term_count ||
		terms->terms[lambda_term].tag != PROTOTYPE_TERM_LAMBDA ||
		binder_subject >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	if (!lambda_body_classifier_matches_binder(
			terms,
			type_declarations,
			lambda->as.lambda.body,
			lambda->as.lambda.binder_id,
			binder_classifier,
			body_classifier
		)) {
		return 0;
	}
	uint32_t codomain_family;
	uint32_t classifier;
	/* PI stores a dependent codomain as a binder family.  In the single
	 * TermDB that family is encoded by the canonical pure THUNK/LAMBDA/RETURN
	 * representation, even when its body is a computation classifier.  This
	 * is a binder representation, not an extra Comp({}, PI(...)) wrapper. */
	if (prototype_term_pure_family(
			terms,
			lambda->as.lambda.binder_id,
			body_classifier,
			&codomain_family
		) != 0 ||
		prototype_term_pi_family(
			terms,
			binder_classifier,
			codomain_family,
			&classifier
		) != 0) {
		return -1;
	}
	uint32_t premise_subjects[2] = {
		binder_subject,
		lambda->as.lambda.body
	};
	uint32_t premise_classifiers[2] = {
		binder_classifier,
		body_classifier
	};
	size_t before = delta->relation_count;
	if (add_delta_relation_with_premises(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			lambda_term,
			classifier,
			PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
			premise_subjects,
			premise_classifiers,
			2
		) != 0) {
		return -1;
	}
	if (delta->relation_count > before) {
		*p_changed = 1;
	}
	return 0;
}

static int infer_lambda_classifiers_from_body(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	int* p_changed
) {
	if (!delta || !terms || !type_declarations || !p_changed ||
		lambda_term >= terms->term_count ||
		terms->terms[lambda_term].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[lambda_term];
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_relation* binder_relations =
			source == 0 ? delta->relations : delta->db->relations;
		size_t binder_count =
			source == 0 ? delta->relation_count : delta->db->relation_count;
		for (size_t i = 0; i < binder_count; ++i) {
			const struct prototype_judgement_relation* binder_relation =
				&binder_relations[i];
			if (binder_relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				binder_relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION ||
				binder_relation->subject >= terms->term_count ||
				terms->terms[binder_relation->subject].tag != PROTOTYPE_TERM_VAR ||
				terms->terms[binder_relation->subject].as.var.binder_id !=
					lambda->as.lambda.binder_id) {
				continue;
			}
			for (int body_source = 0; body_source < 2; ++body_source) {
				const struct prototype_judgement_relation* body_relations =
					body_source == 0 ? delta->relations : delta->db->relations;
				size_t body_count =
					body_source == 0 ? delta->relation_count : delta->db->relation_count;
				for (size_t j = 0; j < body_count; ++j) {
					const struct prototype_judgement_relation* body_relation =
						&body_relations[j];
					if (body_relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
						body_relation->proof_kind ==
							PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN ||
						body_relation->context_id != binder_relation->context_id ||
						body_relation->subject != lambda->as.lambda.body) {
						continue;
					}
					if (infer_lambda_classifier_pair(
							delta,
							terms,
							type_declarations,
							lambda_term,
							binder_relation->subject,
							binder_relation->classifier,
							body_relation->classifier,
							p_changed
						) != 0) {
						return -1;
					}
				}
			}
		}
	}
	return 0;
}

static int infer_lambda_classifier_for_app_argument(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda_term,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, lambda_term, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	if (ensure_lambda_binder_assumption(
			delta, terms, lambda_term, argument_classifier
		) != 0) {
		return -1;
	}
	int changed = 0;
	return infer_lambda_classifiers_from_body(
		delta,
		terms,
		type_declarations,
		lambda_term,
		&changed
	);
}

/* A source lambda can have several provisional classifier candidates while the
 * operation graph fixed point is being solved. Consumers with a known input
 * domain must select the compatible Pi candidate, rather than depend on
 * relation insertion order. */
static int select_delta_pi_classifier_for_domain(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t expected_domain,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		subject >= terms->term_count || expected_domain >= terms->term_count) {
		return -1;
	}
	uint32_t candidates[32];
	uint32_t candidate_count;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			subject,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	int found = 0;
	for (uint32_t i = 0; i < candidate_count; ++i) {
		uint32_t pi;
		uint32_t domain;
		uint32_t family;
		if (classifier_kernel_as_pi(
				terms,
				type_declarations,
				NULL,
				candidates[i],
				&pi,
				&domain,
				&family
			) != 0 || !prototype_judgement_classifier_normalization_equal(
				terms, type_declarations, domain, expected_domain
			)) {
			continue;
		}
		if (!found) {
			*p_classifier = pi;
			found = 1;
		} else if (!prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, *p_classifier, pi
		)) {
			uint32_t selected_domain;
			uint32_t selected_family;
			uint32_t selected_family_binder;
			uint32_t selected_body;
			uint32_t candidate_family_binder;
			uint32_t candidate_body;
			struct prototype_term_classifier_view selected_view;
			struct prototype_term_classifier_view candidate_view;
			unsigned selected_effects;
			unsigned candidate_effects;
			if (classifier_kernel_as_pi(
					terms,
					type_declarations,
					NULL,
					*p_classifier,
					NULL,
					&selected_domain,
					&selected_family
				) != 0 || prototype_term_pure_family_parts(
					terms,
					selected_family,
					&selected_family_binder,
					&selected_body
				) != 0 || prototype_term_pure_family_parts(
					terms,
					family,
					&candidate_family_binder,
					&candidate_body
				) != 0 || prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					selected_body,
					&selected_view
				) != 0 || prototype_judgement_classifier_view(
					terms,
					type_declarations,
					NULL,
					candidate_body,
					&candidate_view
				) != 0 || selected_view.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				candidate_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				selected_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
				candidate_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
				!prototype_judgement_classifier_normalization_equal(
					terms,
					type_declarations,
					selected_view.result,
					candidate_view.result
				) || prototype_term_effect_row_closed_bits(
					terms, selected_view.effect_row, &selected_effects
				) != 0 || prototype_term_effect_row_closed_bits(
					terms, candidate_view.effect_row, &candidate_effects
				) != 0) {
				return -1;
			}
			if ((candidate_effects & selected_effects) == candidate_effects) {
				*p_classifier = pi;
			} else if ((selected_effects & candidate_effects) != selected_effects) {
				return -1;
			}
			(void)selected_domain;
			(void)selected_family_binder;
			(void)candidate_family_binder;
		}
	}
	return found ? 0 : 1;
}

static int operation_named_nat_type_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	int nat_symbol_id,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !p_ret || nat_symbol_id < 0) {
		return -1;
	}
	const struct prototype_type_declaration* nat_type =
		prototype_type_declaration_lookup(type_declarations, nat_symbol_id);
	if (!nat_type) {
		return -1;
	}
	return prototype_term_type_instance_make(
		terms,
		type_declarations,
		nat_type->type_index,
		NULL,
		0,
		p_ret
	);
}

static int host_signature_classifier(
	struct prototype_term_db* terms,
	uint32_t arity,
	const int* argument_types,
	int result_type,
	unsigned effects,
	uint32_t* p_ret
) {
	if (!terms || !argument_types || !p_ret ||
		result_type == PROTOTYPE_HOST_TYPE_INVALID) {
		return 1;
	}
	for (uint32_t i = 0; i < arity; ++i) {
		if (argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID) {
			return 1;
		}
	}
	uint32_t current;
	if (prototype_term_make_host_type(terms, result_type, &current) != 0) {
		return -1;
	}
	uint32_t effect_label;
	if (prototype_term_effect_label(terms, effects, &effect_label) != 0 ||
		prototype_term_computation_type(terms, effect_label, current, &current) != 0) {
		return -1;
	}
	for (uint32_t i = arity; i > 0; --i) {
		int argument_type = argument_types[i - 1];
		uint32_t domain;
		if (prototype_term_make_host_type(terms, argument_type, &domain) != 0 ||
			prototype_term_pi(terms, domain, current, &current) != 0) {
			return -1;
		}
	}
	*p_ret = current;
	return 0;
}

int prototype_judgement_pure_primitive_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term* operation,
	uint32_t* p_ret
) {
	if (!terms || !type_declarations || !operation || !p_ret ||
		operation->tag != PROTOTYPE_TERM_PURE_PRIMITIVE) {
		return -1;
	}
	const struct prototype_pure_primitive_declaration* signature =
		prototype_term_pure_primitive_declaration(operation->as.pure_primitive.primitive_id);
	if (signature) {
		int status = host_signature_classifier(
			terms,
			signature->arity,
			signature->argument_types,
			signature->result_type,
			PROTOTYPE_EFFECT_OPERATION_LABEL_NONE,
			p_ret
		);
		if (status <= 0) {
			return status;
		}
	}

	uint32_t text;
	uint32_t nat;
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0) {
		return -1;
	}
	switch (operation->as.pure_primitive.primitive_id) {
		case PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT: {
			if (operation_named_nat_type_classifier(
					terms,
					type_declarations,
					operation->as.pure_primitive.type_symbol_id,
					&nat
				) != 0) {
				return -1;
			}
			uint32_t effects;
			uint32_t result;
			if (prototype_term_effect_label(terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &effects) != 0 ||
				prototype_term_computation_type(terms, effects, nat, &result) != 0) {
				return -1;
			}
			return prototype_term_pi(terms, text, result, p_ret);
		}
		case PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT: {
			if (operation_named_nat_type_classifier(
					terms,
					type_declarations,
					operation->as.pure_primitive.type_symbol_id,
					&nat
				) != 0) {
				return -1;
			}
			uint32_t effects;
			uint32_t result;
			if (prototype_term_effect_label(terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &effects) != 0 ||
				prototype_term_computation_type(terms, effects, text, &result) != 0) {
				return -1;
			}
			return prototype_term_pi(terms, nat, result, p_ret);
		}
		default:
			return -1;
	}
	}

int prototype_judgement_effect_operation_classifier(
	struct prototype_term_db* terms,
	const struct prototype_term* operation,
	uint32_t* p_ret
) {
	if (!terms || !operation || !p_ret ||
		operation->tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	if (!prototype_term_effect_operation_declaration(
			operation->as.effect_operation.operation_id
		) || operation->as.effect_operation.classifier >= terms->term_count) {
		return -1;
	}
	*p_ret = operation->as.effect_operation.classifier;
	return 0;
}

static int infer_text_literal_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !terms || !term_has_tag(terms, term_id, PROTOTYPE_TERM_TEXT_LITERAL)) {
		return -1;
	}
	uint32_t text;
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		text,
		PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO
	);
}

static int infer_int_literal_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !terms || !term_has_tag(terms, term_id, PROTOTYPE_TERM_INT_LITERAL)) {
		return -1;
	}
	uint32_t integer;
	uint32_t integer64;
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT64, &integer64) != 0 ||
		add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			term_id,
			integer64,
			PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO
		) != 0) {
		return -1;
	}
	if (!int_literal_fits_int32(terms->terms[term_id].as.int_literal.value)) {
		return 0;
	}
	if (prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT32, &integer) != 0) {
		return -1;
	}
	return add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			term_id,
			integer,
			PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO
		);
}

static int infer_pure_primitive_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_PURE_PRIMITIVE)) {
		return -1;
	}
	uint32_t classifier;
	if (prototype_judgement_pure_primitive_classifier(
			terms,
			type_declarations,
			&terms->terms[term_id],
			&classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO
	);
}

static int infer_effect_operation_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t term_id
) {
	if (!delta || !terms ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_EFFECT_OPERATION)) {
		return -1;
	}
	uint32_t classifier;
	if (prototype_judgement_effect_operation_classifier(
			terms, &terms->terms[term_id], &classifier
		) != 0) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO
	);
}

static int infer_constructor_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, term_id, PROTOTYPE_TERM_CONSTRUCTOR)) {
		return -1;
	}
	uint32_t constructor_term;
	uint32_t classifier;
	const struct prototype_term* term = &terms->terms[term_id];
	return materialize_constructor_classifier(
		delta,
		terms,
		type_declarations,
		term->as.constructor.owner,
		term->as.constructor.constructor_id,
		&constructor_term,
		&classifier
	);
}

int prototype_judgement_cbpv_boundary_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t child_classifier,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !p_classifier || term_id >= terms->term_count ||
		child_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	uint32_t classifier;
	if (term->tag == PROTOTYPE_TERM_RETURN) {
		uint32_t empty_effects;
		if (prototype_term_effect_label(terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects) != 0 ||
			prototype_term_computation_type(terms, empty_effects, child_classifier, &classifier) != 0) {
			return -1;
		}
	} else if (term->tag == PROTOTYPE_TERM_THUNK) {
		struct prototype_term_classifier_view view;
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, child_classifier, &view
			) != 0 ||
			view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			prototype_term_thunk_type(terms, child_classifier, &classifier) != 0) {
			return -1;
		}
	} else if (term->tag == PROTOTYPE_TERM_FORCE) {
		uint32_t value_whnf;
		if (classifier_kernel_whnf(
				terms, type_declarations, NULL, child_classifier, &value_whnf
			) != 0 || value_whnf >= terms->term_count ||
			terms->terms[value_whnf].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			return -1;
		}
		classifier = terms->terms[value_whnf].as.thunk_type.computation;
	} else {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}

int prototype_judgement_delta_record_cbpv_boundary(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t child_classifier
) {
	if (!delta || !terms || !type_declarations || term_id >= terms->term_count ||
		child_classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	uint32_t child;
	int proof_kind;
	uint32_t classifier;
	if (prototype_judgement_cbpv_boundary_classifier(
			terms, type_declarations, term_id, child_classifier, &classifier
		) != 0) {
		return -1;
	}
	if (term->tag == PROTOTYPE_TERM_RETURN) {
		child = term->as.return_term.value;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO;
	} else if (term->tag == PROTOTYPE_TERM_THUNK) {
		child = term->as.thunk.computation;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO;
	} else if (term->tag == PROTOTYPE_TERM_FORCE) {
		child = term->as.force.value;
		proof_kind = PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM;
	} else {
		return -1;
	}
	uint32_t premise_subjects[1] = { child };
	uint32_t premise_classifiers[1] = { child_classifier };
	return add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		term_id,
		classifier,
		proof_kind,
		premise_subjects,
		premise_classifiers,
		1
	);
}

static int infer_cbpv_boundary_classifier(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!delta || !terms || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t child;
	switch (terms->terms[term_id].tag) {
		case PROTOTYPE_TERM_RETURN:
			child = terms->terms[term_id].as.return_term.value;
			break;
		case PROTOTYPE_TERM_THUNK:
			child = terms->terms[term_id].as.thunk.computation;
			break;
		case PROTOTYPE_TERM_FORCE:
			child = terms->terms[term_id].as.force.value;
			break;
		default:
			return -1;
	}
	uint32_t child_classifier;
	if (lookup_delta_proven_classifier(delta, terms, child, &child_classifier) != 0) {
		return 1;
	}
	return prototype_judgement_delta_record_cbpv_boundary(
		delta, terms, type_declarations, term_id, child_classifier
	);
}

int prototype_judgement_delta_infer_cbpv_boundaries(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	for (;;) {
		int changed = 0;
		for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
			int tag = terms->terms[i].tag;
			if (tag != PROTOTYPE_TERM_RETURN &&
				tag != PROTOTYPE_TERM_THUNK &&
				tag != PROTOTYPE_TERM_FORCE) {
				continue;
			}
			uint32_t ignored;
			if (lookup_delta_proven_classifier(delta, terms, i, &ignored) == 0) {
				continue;
			}
			size_t before = delta->relation_count;
			int status = infer_cbpv_boundary_classifier(delta, terms, type_declarations, i);
			if (status < 0) {
				return -1;
			}
			if (delta->relation_count > before) {
				changed = 1;
			}
		}
		if (!changed) {
			break;
		}
	}
	return 0;
}

int prototype_judgement_delta_record_computation_constraint(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject
) {
	if (!delta || !terms ||
		(delta->computation_constraint_capacity != 0 &&
			!delta->computation_constraints) ||
		subject >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[subject];
	struct prototype_judgement_computation_constraint constraint;
	memset(&constraint, 0, sizeof(constraint));
	constraint.context_id = context_id;
	constraint.subject = subject;
	constraint.effect_residual_pending = 0;
	constraint.effect_residual_row = PROTOTYPE_INVALID_ID;
	constraint.effect_output_row = PROTOTYPE_INVALID_ID;
	if (term->tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
		constraint.kind = PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD;
		constraint.computation = term->as.computation_fold.computation;
		constraint.continuation = term->as.computation_fold.return_clause;
		constraint.argument = PROTOTYPE_INVALID_ID;
		constraint.application = PROTOTYPE_INVALID_ID;
	} else if (term->tag == PROTOTYPE_TERM_OPERATION_REQUEST) {
		constraint.kind =
			PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST;
		constraint.computation = term->as.operation_request.operation;
		constraint.continuation = term->as.operation_request.continuation;
		constraint.argument = term->as.operation_request.argument;
		constraint.application = PROTOTYPE_INVALID_ID;
	} else {
		return 1;
	}
	for (size_t i = 0; i < delta->computation_constraint_count; ++i) {
		const struct prototype_judgement_computation_constraint* existing =
			&delta->computation_constraints[i];
		if (existing->kind == constraint.kind &&
			existing->context_id == context_id &&
			existing->subject == subject) {
			return 0;
		}
	}
	if (reserve_slot(
			delta->computation_constraint_count,
			delta->computation_constraint_capacity
		) != 0) {
		return -1;
	}
	delta->computation_constraints[delta->computation_constraint_count++] =
		constraint;
	return 0;
}

int prototype_judgement_delta_generate_computation_constraints(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms
) {
	if (!delta || !terms) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		int status = prototype_judgement_delta_record_computation_constraint(
			delta, terms, delta->current_context_id, i
		);
		if (status < 0) {
			return -1;
		}
	}
	return 0;
}

/* A zero-clause computation fold sequences a returning computation through its
 * return algebra. The result remains a returning computation. */
int prototype_judgement_computation_fold_result_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t input_computation,
	uint32_t input_classifier,
	uint32_t continuation_classifier,
	uint32_t* p_ret
) {
	struct prototype_term_classifier_view input_view;
	struct prototype_term_classifier_view continuation_view;
	uint32_t continuation_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t codomain_binder;
	uint32_t codomain;
	uint32_t normalized_input;
	struct prototype_term_normalization_result input_normalization;
	uint32_t codomain_lambda;
	uint32_t applied_codomain;
	uint32_t normalized_codomain;
	if (!terms || !type_declarations || !p_ret ||
		input_computation >= terms->term_count || input_classifier >= terms->term_count ||
		continuation_classifier >= terms->term_count ||
		prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		) != 0 ||
		input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			continuation_classifier,
			&continuation_pi,
			&domain,
			&codomain_family
		) != 0 || prototype_term_pure_family_parts(
			terms, codomain_family, &codomain_binder, &codomain
		) != 0 || !prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, domain, input_view.result
		)) {
		return -1;
	}
	if (!prototype_term_contains_free_binder(
			terms, codomain, codomain_binder
		)) {
		applied_codomain = codomain;
	} else {
		if (prototype_term_normalize_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				input_computation,
				PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT,
				&input_normalization
			) != 0 || input_normalization.status ==
				PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
			return -1;
		}
		if (input_normalization.status !=
			PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			return 1;
		}
		normalized_input = input_normalization.term_id;
		if (normalized_input >= terms->term_count ||
			terms->terms[normalized_input].tag != PROTOTYPE_TERM_RETURN ||
			prototype_term_pure_family_lambda(
				terms,
				codomain_family,
				&codomain_lambda
			) != 0 || prototype_term_app(
				terms,
				codomain_lambda,
				terms->terms[normalized_input].as.return_term.value,
				&applied_codomain
			) != 0) {
			return -1;
		}
	}
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			applied_codomain,
			&normalized_codomain
		) != 0 || normalized_codomain >= terms->term_count) {
		return -1;
	}
	if (terms->terms[normalized_codomain].tag == PROTOTYPE_TERM_RETURN) {
		applied_codomain = terms->terms[normalized_codomain].as.return_term.value;
	} else {
		applied_codomain = normalized_codomain;
	}
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, applied_codomain, &continuation_view
		) != 0 || continuation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t effect_row;
	if (computation_effect_row_union(
			terms, &input_view, &continuation_view, &effect_row
		) != 0 || prototype_term_computation_type(
			terms, effect_row, continuation_view.result, p_ret
		) != 0) {
		return -1;
	}
	(void)continuation_pi;
	(void)codomain_binder;
	(void)codomain;
	return 0;
}

int prototype_judgement_delta_record_computation_fold_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
) {
	if (!delta || !terms || !type_declarations ||
		subject >= terms->term_count || classifier >= terms->term_count ||
		terms->terms[subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD ||
		!premise_classifiers) {
		return -1;
	}
	const struct prototype_term* fold = &terms->terms[subject];
	uint32_t clause_count = fold->as.computation_fold.clause_count;
	uint32_t expected_premise_count = 2 + 2 * clause_count;
	if (clause_count > 31 || premise_count != expected_premise_count ||
		fold->as.computation_fold.first_clause > terms->computation_fold_clause_count ||
		clause_count > terms->computation_fold_clause_count -
			fold->as.computation_fold.first_clause) {
		return -1;
	}
	for (uint32_t i = 0; i < premise_count; ++i) {
		if (premise_classifiers[i] >= terms->term_count) {
			return -1;
		}
	}
	if (clause_count == 0) {
		uint32_t derived_classifier;
		int status = prototype_judgement_computation_fold_result_classifier(
			terms,
			type_declarations,
			fold->as.computation_fold.computation,
			premise_classifiers[0],
			premise_classifiers[1],
			&derived_classifier
		);
		if (status != 0) {
			return status;
		}
		if (!prototype_judgement_classifier_normalization_equal(
				terms, type_declarations, classifier, derived_classifier
			)) {
			return -1;
		}
	} else {
		struct prototype_term_classifier_view result;
		if (prototype_judgement_classifier_view(
				terms, type_declarations, NULL, classifier, &result
			) != 0 || result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return -1;
		}
	}
	uint32_t subjects[64];
	subjects[0] = fold->as.computation_fold.computation;
	subjects[1] = fold->as.computation_fold.return_clause;
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		subjects[2 + 2 * i] = clause->operation;
		subjects[3 + 2 * i] = clause->body;
	}
	return add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
		subjects,
		premise_classifiers,
		premise_count
	);
}

static int solve_zero_clause_computation_fold_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_computation_constraint* constraint
) {
	for (size_t i = 0; i < delta->relation_count; ++i) {
		const struct prototype_judgement_relation* relation =
			&delta->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == constraint->subject &&
			relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM) {
			return 0;
		}
	}
	uint32_t input_classifier;
	struct prototype_term_classifier_view input_view;
	if (lookup_delta_proven_classifier(
			delta, terms, constraint->computation, &input_classifier
		) != 0) {
		return 1;
	}
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		/* TermDB identity may be shared by differently typed source occurrences.
		 * A global lookup that finds a non-returning candidate cannot refute this
		 * occurrence-local zero-clause computation fold; defer until the OperationGraph records its selected
		 * operand classifiers. */
		return 1;
	}
	if (term_has_tag(terms, constraint->continuation, PROTOTYPE_TERM_LAMBDA) &&
		infer_lambda_classifier_for_app_argument(
			delta,
			terms,
			type_declarations,
			constraint->continuation,
			input_view.result
		) != 0) {
		return 1;
	}
	uint32_t continuation_classifier;
	if (lookup_delta_proven_classifier(
			delta, terms, constraint->continuation, &continuation_classifier
		) != 0) {
		return 1;
	}
	uint32_t classifier;
	int result_status = prototype_judgement_computation_fold_result_classifier(
		terms,
		type_declarations,
		constraint->computation,
		input_classifier,
		continuation_classifier,
		&classifier
	);
	if (result_status != 0) {
		/* This legacy constraint stores only TermDB subjects.  A shared core
		 * term may have several occurrence-local classifiers, so failure for the
		 * candidates selected above cannot refute the source computation fold.  Derive a
		 * proof here only when the candidates work; otherwise let the
		 * OperationGraph validate its explicitly selected child classifiers. */
		return 1;
	}
	uint32_t subjects[2] = { constraint->computation, constraint->continuation };
	uint32_t classifiers[2] = { input_classifier, continuation_classifier };
	return add_delta_relation_with_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject, classifier,
		PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM, subjects, classifiers, 2
	);
}

static int solve_operation_request_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint* constraint
) {
	if (constraint->application == PROTOTYPE_INVALID_ID) {
		for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
			if (terms->terms[i].tag == PROTOTYPE_TERM_APP &&
				terms->terms[i].as.app.function == constraint->computation &&
				terms->terms[i].as.app.argument == constraint->argument) {
				constraint->application = i;
				break;
			}
		}
		if (constraint->application == PROTOTYPE_INVALID_ID &&
			prototype_term_app(
				terms, constraint->computation, constraint->argument, &constraint->application
			) != 0) {
			return -1;
		}
	}
	uint32_t application_classifier;
	if (lookup_delta_proven_classifier(
			delta, terms, constraint->application, &application_classifier
		) != 0) {
		int status = prototype_judgement_delta_expand_app(
			delta, terms, type_declarations, constraint->application, NULL
		);
		return status < 0 ? -1 : 1;
	}
	struct prototype_term_classifier_view operation_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, application_classifier, &operation_view
		) != 0) {
		return -1;
	}
	/* A request is only meaningful after the operation application is Comp. */
	if (operation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 1;
	}
	if (!term_has_tag(terms, constraint->continuation, PROTOTYPE_TERM_THUNK)) {
		return -1;
	}
	uint32_t continuation_lambda = terms->terms[constraint->continuation].as.thunk.computation;
	if (!term_has_tag(terms, continuation_lambda, PROTOTYPE_TERM_LAMBDA)) {
		return -1;
	}
	if (infer_lambda_classifier_for_app_argument(
			delta, terms, type_declarations, continuation_lambda, operation_view.result
		) != 0) {
		return -1;
	}
	uint32_t continuation_function_classifier;
	if (lookup_delta_proven_classifier(
			delta, terms, continuation_lambda, &continuation_function_classifier
		) != 0) {
		return 1;
	}
	uint32_t continuation_classifier;
	if (prototype_term_thunk_type(
			terms, continuation_function_classifier, &continuation_classifier
		) != 0 || prototype_judgement_delta_record_cbpv_boundary(
			delta, terms, type_declarations, constraint->continuation, continuation_function_classifier
		) != 0) {
		return -1;
	}
	uint32_t domain;
	uint32_t codomain_family;
	if (pi_parts(terms, continuation_function_classifier, &domain, &codomain_family) != 0 ||
		!prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, domain, operation_view.result
		)) {
		return -1;
	}
	uint32_t continuation_binder = terms->terms[continuation_lambda].as.lambda.binder_id;
	uint32_t continuation_var;
	uint32_t continuation_result;
	if (prototype_term_var(terms, continuation_binder, &continuation_var) != 0 ||
		pi_codomain_at_fresh_binder(
			delta, terms, type_declarations, delta->current_context_id,
			continuation_function_classifier, continuation_var, domain,
			PROTOTYPE_INVALID_ID,
			&continuation_result
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view continuation_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, continuation_result, &continuation_view
		) != 0 || continuation_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation_view.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 1;
	}
	if (prototype_term_contains_free_binder(
			terms, continuation_view.result, continuation_binder
		)) {
		return 1;
	}
	uint32_t effect_row;
	uint32_t classifier;
	if (computation_effect_row_union(
			terms, &operation_view, &continuation_view, &effect_row
		) != 0 || prototype_term_computation_type(
			terms, effect_row, continuation_view.result, &classifier
		) != 0) {
		return -1;
	}
	uint32_t request_rows[2] = {
		operation_view.effect_row,
		continuation_view.effect_row
	};
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN,
			constraint->subject, effect_row, request_rows, 2
		) != 0) {
		return -1;
	}
	uint32_t subjects[2] = { constraint->application, constraint->continuation };
	uint32_t classifiers[2] = { application_classifier, continuation_classifier };
	return add_delta_relation_with_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject, classifier,
		PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO, subjects, classifiers, 2
	);
}

static int record_computation_lambda_body_effect_weaken(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t lambda,
	uint32_t source_classifier,
	uint32_t target_effect_row,
	uint32_t* p_target_classifier
) {
	if (!delta || !terms || !type_declarations || !p_target_classifier ||
		lambda >= terms->term_count ||
		terms->terms[lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		source_classifier >= terms->term_count || target_effect_row >= terms->term_count ||
		!delta->contexts) {
		return -1;
	}
	uint32_t source_pi;
	uint32_t domain;
	uint32_t source_family;
	uint32_t binder = terms->terms[lambda].as.lambda.binder_id;
	uint32_t binder_var;
	uint32_t source_body_classifier;
	uint32_t source_family_binder;
	uint32_t source_family_body;
	if (classifier_kernel_as_pi(
			terms, type_declarations, NULL, source_classifier,
			&source_pi, &domain, &source_family
		) != 0 || prototype_term_var(terms, binder, &binder_var) != 0 ||
		prototype_term_pure_family_parts(
			terms,
			source_family,
			&source_family_binder,
			&source_family_body
		) != 0) {
		return -1;
	}
	if (!prototype_term_contains_free_binder(
			terms, source_family_body, source_family_binder
		)) {
		source_body_classifier = source_family_body;
	} else if (pi_codomain_at_binder_in_context(
			delta,
			terms,
			type_declarations,
			delta->current_context_id,
			source_pi,
			binder_var,
			domain,
			PROTOTYPE_INVALID_ID,
			&source_body_classifier
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view source_body;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, source_body_classifier, &source_body
		) != 0 || source_body.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source_body.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t target_body_classifier;
	if (prototype_term_computation_type(
			terms, target_effect_row, source_body.result, &target_body_classifier
		) != 0) {
		return -1;
	}
	uint32_t saved_context = delta->current_context_id;
	uint32_t body_context;
	if (ensure_lambda_binder_assumption(delta, terms, lambda, domain) != 0 ||
		prototype_context_extend(
			delta->contexts,
			saved_context,
			binder,
			domain,
			PROTOTYPE_INVALID_ID,
			&body_context
		) != 0) {
		return -1;
	}
	prototype_judgement_delta_set_context(delta, body_context);
	int weaken_status = prototype_judgement_delta_record_effect_weaken(
		delta,
		terms,
		type_declarations,
		terms->terms[lambda].as.lambda.body,
		source_body_classifier,
		target_body_classifier
	);
	prototype_judgement_delta_set_context(delta, saved_context);
	if (weaken_status != 0) {
		return -1;
	}
	(void)source_family;
	/* Weakening belongs to the computation returned by the lambda. The lambda's
	 * source Pi remains the return-clause derivation used by the fold rule. */
	*p_target_classifier = source_classifier;
	return 0;
}

static int solve_clause_computation_fold_constraint(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_computation_constraint* constraint
) {
	if (constraint->subject >= terms->term_count ||
		terms->terms[constraint->subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
		return -1;
	}
	const struct prototype_term* fold = &terms->terms[constraint->subject];
	uint32_t clause_count = fold->as.computation_fold.clause_count;
	if (clause_count == 0 || clause_count > 31 ||
		fold->as.computation_fold.first_clause > terms->computation_fold_clause_count ||
		clause_count > terms->computation_fold_clause_count -
			fold->as.computation_fold.first_clause) {
		return -1;
	}
	uint32_t input_classifier;
	if (lookup_delta_proven_classifier(
			delta, terms, fold->as.computation_fold.computation, &input_classifier
		) != 0) {
		return 1;
	}
	struct prototype_term_classifier_view input_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, input_classifier, &input_view
		) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		input_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}

	uint32_t operation_classifiers[31];
	uint32_t operation_domains[31];
	struct prototype_term_classifier_view operation_views[31];
	uint32_t handled_operation_rows[31];
	uint32_t handled_operations_row;
	if (prototype_term_effect_label(
			terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &handled_operations_row
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		uint32_t operation_pi;
		uint32_t operation_family;
		uint32_t operation_binder;
		uint32_t operation_var;
		uint32_t operation_result;
		uint32_t ignored_operation_body;
		if (lookup_delta_proven_classifier(
				delta, terms, clause->operation, &operation_classifiers[i]
			) != 0) {
			return 1;
		}
		if (classifier_kernel_as_pi(
				terms, type_declarations, NULL, operation_classifiers[i],
				&operation_pi, &operation_domains[i], &operation_family
			) != 0 || prototype_term_pure_family_parts(
				terms, operation_family, &operation_binder, &ignored_operation_body
			) != 0 || prototype_term_var(terms, operation_binder, &operation_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				operation_pi, operation_var, operation_domains[i],
				PROTOTYPE_INVALID_ID, &operation_result
			) != 0 || prototype_judgement_classifier_view(
				terms, type_declarations, NULL, operation_result, &operation_views[i]
			) != 0 || operation_views[i].category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			operation_views[i].computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		uint32_t union_row;
		if (prototype_term_effect_row_union(
				terms, handled_operations_row, operation_views[i].effect_row, &union_row
			) != 0) {
			return -1;
		}
		handled_operation_rows[i] = operation_views[i].effect_row;
		handled_operations_row = union_row;
	}
	if (add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN,
			constraint->subject,
			handled_operations_row,
			handled_operation_rows,
			clause_count
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view handled_operations_view = {
		.category = PROTOTYPE_TERM_CATEGORY_COMPUTATION,
		.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING,
		.effect_row = handled_operations_row,
		.result = input_view.result
	};
	uint32_t residual_effect_row;
	int residual_status = closed_computation_fold_residual_row(
		terms, &input_view, &handled_operations_view, &residual_effect_row
	);
	if (residual_status < 0) {
		return -1;
	}
	if (residual_status > 0) {
		constraint->effect_residual_pending = 1;
		constraint->effect_residual_row = PROTOTYPE_INVALID_ID;
		return 1;
	}
	constraint->effect_residual_pending = 0;
	constraint->effect_residual_row = residual_effect_row;
	uint32_t residual_rows[2] = {
		input_view.effect_row,
		handled_operations_row
	};
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_RESIDUAL,
			constraint->subject, residual_effect_row, residual_rows, 2
		) != 0) {
		return -1;
	}

	if (infer_lambda_classifier_for_app_argument(
			delta, terms, type_declarations, fold->as.computation_fold.return_clause, input_view.result
		) != 0) {
		return -1;
	}
	uint32_t return_classifier;
	int return_selection = select_delta_pi_classifier_for_domain(
		delta,
		terms,
		type_declarations,
		fold->as.computation_fold.return_clause,
		input_view.result,
		&return_classifier
	);
	if (return_selection < 0) {
		return -1;
	}
	if (return_selection > 0) {
		return 1;
	}
	uint32_t return_pi;
	uint32_t return_domain;
	uint32_t return_family;
	if (classifier_kernel_as_pi(
			terms, type_declarations, NULL, return_classifier,
			&return_pi, &return_domain, &return_family
		) != 0) {
		return -1;
	}
	if (fold->as.computation_fold.return_clause >= terms->term_count ||
		terms->terms[fold->as.computation_fold.return_clause].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	uint32_t return_binder =
		terms->terms[fold->as.computation_fold.return_clause].as.lambda.binder_id;
	uint32_t return_var;
	uint32_t output_classifier;
	if (prototype_term_var(terms, return_binder, &return_var) != 0 ||
		pi_codomain_at_fresh_binder(
			delta, terms, type_declarations, delta->current_context_id,
			return_pi, return_var, return_domain,
			PROTOTYPE_INVALID_ID, &output_classifier
		) != 0) {
		return -1;
	}
	int dependent_output = prototype_term_contains_free_binder(
		terms, output_classifier, return_binder
	);
	struct prototype_term_classifier_view output_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, output_classifier, &output_view
		) != 0 || output_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		output_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t carrier_rows[33];
	uint32_t carrier_row_count = 0;
	carrier_rows[carrier_row_count++] = output_view.effect_row;
	carrier_rows[carrier_row_count++] = residual_effect_row;
	uint32_t handled_effect_row;
	uint32_t handled_output_classifier;
	if (prototype_term_effect_row_union(
			terms, output_view.effect_row, residual_effect_row, &handled_effect_row
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		for (size_t relation_id = 0;
			relation_id < delta->relation_count;
			++relation_id) {
			const struct prototype_judgement_relation* relation =
				&delta->relations[relation_id];
			uint32_t outer_domain;
			uint32_t outer_family;
			uint32_t outer_binder;
			uint32_t inner_classifier;
			uint32_t inner_domain;
			uint32_t inner_family;
			uint32_t inner_binder;
			uint32_t body_classifier;
			struct prototype_term_classifier_view body_view;
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != clause->body || pi_parts(
					terms, relation->classifier, &outer_domain, &outer_family
				) != 0 || !prototype_judgement_classifier_normalization_equal(
					terms, type_declarations, outer_domain, operation_domains[i]
				) || prototype_term_pure_family_parts(
					terms, outer_family, &outer_binder, &inner_classifier
				) != 0 || pi_parts(
					terms, inner_classifier, &inner_domain, &inner_family
				) != 0 || prototype_term_pure_family_parts(
					terms, inner_family, &inner_binder, &body_classifier
				) != 0 || prototype_judgement_classifier_view(
					terms, type_declarations, NULL, body_classifier, &body_view
				) != 0 || body_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				body_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
				!prototype_judgement_classifier_normalization_equal(
					terms, type_declarations, body_view.result, output_view.result
				)) {
				continue;
			}
			uint32_t joined_row;
			if (prototype_term_effect_row_union(
					terms,
					handled_effect_row,
					body_view.effect_row,
					&joined_row
				) != 0) {
				return -1;
			}
			handled_effect_row = joined_row;
			carrier_rows[carrier_row_count++] = body_view.effect_row;
			(void)outer_binder;
			(void)inner_domain;
			(void)inner_binder;
			break;
		}
	}
	constraint->effect_output_row = handled_effect_row;
	if (prototype_term_computation_type(
			terms, handled_effect_row, output_view.result, &handled_output_classifier
		) != 0) {
		return -1;
	}
	if (add_effect_row_constraint(
			delta, PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_JOIN,
			constraint->subject,
			handled_effect_row,
			carrier_rows,
			carrier_row_count
		) != 0) {
		return -1;
	}
	uint32_t return_inclusion[2] = {
		output_view.effect_row,
		handled_effect_row
	};
	uint32_t residual_inclusion[2] = {
		residual_effect_row,
		handled_effect_row
	};
	if (add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION,
			constraint->subject,
			handled_effect_row,
			return_inclusion,
			2
		) != 0 || add_effect_row_constraint(
			delta,
			PROTOTYPE_JUDGEMENT_EFFECT_ROW_CONSTRAINT_INCLUSION,
			constraint->subject,
			handled_effect_row,
			residual_inclusion,
			2
		) != 0) {
		return -1;
	}
	if (!prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			output_view.effect_row,
			handled_effect_row
		)) {
		unsigned source_effects;
		unsigned target_effects;
		if (prototype_term_effect_row_closed_bits(
				terms, output_view.effect_row, &source_effects
			) != 0 || prototype_term_effect_row_closed_bits(
				terms, handled_effect_row, &target_effects
			) != 0) {
			/* The partial-policy solver retains this inclusion obligation. It must
			 * not fabricate a closed EFFECT_WEAKEN proof for a symbolic row. */
			return 1;
		}
		(void)source_effects;
		(void)target_effects;
		uint32_t widened_return_classifier;
		if (record_computation_lambda_body_effect_weaken(
				delta,
				terms,
				type_declarations,
				fold->as.computation_fold.return_clause,
				return_classifier,
				handled_effect_row,
				&widened_return_classifier
			) != 0) {
			return -1;
		}
		return_classifier = widened_return_classifier;
	}

	uint32_t outer_classifiers[31];
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		uint32_t operation_domain = operation_domains[i];
		struct prototype_term_classifier_view operation_view = operation_views[i];
		if (clause->body >= terms->term_count ||
			terms->terms[clause->body].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, clause->body, operation_domain
			) != 0) {
			return -1;
		}
		uint32_t continuation_lambda =
			terms->terms[clause->body].as.lambda.body;
		if (continuation_lambda >= terms->term_count ||
			terms->terms[continuation_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		uint32_t continuation_function_classifier;
		uint32_t continuation_expected;
		if (prototype_term_pi(
				terms,
				operation_view.result,
				handled_output_classifier,
				&continuation_function_classifier
			) != 0 || prototype_term_thunk_type(
				terms, continuation_function_classifier, &continuation_expected
			) != 0) {
			return -1;
		}
		uint32_t saved_context = delta->current_context_id;
		uint32_t operation_argument_context;
		uint32_t continuation_context;
		if (ensure_lambda_binder_assumption(
				delta,
				terms,
				clause->body,
				operation_domain
			) != 0 || prototype_context_extend(
				delta->contexts,
				saved_context,
				terms->terms[clause->body].as.lambda.binder_id,
				operation_domain,
				PROTOTYPE_INVALID_ID,
				&operation_argument_context
			) != 0) {
			return -1;
		}
		prototype_judgement_delta_set_context(delta, operation_argument_context);
		if (ensure_lambda_binder_assumption(
				delta,
				terms,
				continuation_lambda,
				continuation_expected
			) != 0 || prototype_context_extend(
				delta->contexts,
				operation_argument_context,
				terms->terms[continuation_lambda].as.lambda.binder_id,
				continuation_expected,
				PROTOTYPE_INVALID_ID,
				&continuation_context
			) != 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, continuation_context);
		uint32_t continuation_body = terms->terms[continuation_lambda].as.lambda.body;
		if (continuation_body < terms->term_count &&
			terms->terms[continuation_body].tag == PROTOTYPE_TERM_APP &&
			prototype_judgement_delta_expand_app(
				delta, terms, type_declarations, continuation_body, NULL
			) < 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, operation_argument_context);
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, continuation_lambda, continuation_expected
			) != 0) {
			prototype_judgement_delta_set_context(delta, saved_context);
			return -1;
		}
		prototype_judgement_delta_set_context(delta, saved_context);
		if (infer_lambda_classifier_for_app_argument(
				delta, terms, type_declarations, clause->body, operation_domain
			) != 0) {
			return -1;
		}
		uint32_t expected_continuation_classifier;
		uint32_t expected_outer_classifier;
		if (prototype_term_pi(
				terms,
				continuation_expected,
				handled_output_classifier,
				&expected_continuation_classifier
			) != 0 || prototype_term_pi(
				terms,
				operation_domain,
				expected_continuation_classifier,
				&expected_outer_classifier
			) != 0) {
			return -1;
		}
		uint32_t outer_classifier;
		int outer_selection = lookup_delta_proven_classifier_normalization_equal(
			delta,
			terms,
			type_declarations,
			clause->body,
			expected_outer_classifier,
			&outer_classifier
		);
		if (outer_selection < 0) {
			return -1;
		}
		if (outer_selection > 0) {
			return 1;
		}
		uint32_t outer_pi;
		uint32_t outer_domain;
		uint32_t outer_family;
		if (classifier_kernel_as_pi(
				terms, type_declarations, NULL, outer_classifier,
				&outer_pi, &outer_domain, &outer_family
			) != 0) {
			return -1;
		}
		uint32_t outer_binder;
		uint32_t outer_var;
		uint32_t continuation_classifier;
		uint32_t ignored_outer_body;
		if (prototype_term_pure_family_parts(
				terms, outer_family, &outer_binder, &ignored_outer_body
			) != 0 ||
			prototype_term_var(terms, outer_binder, &outer_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				outer_pi, outer_var, outer_domain,
				PROTOTYPE_INVALID_ID, &continuation_classifier
			) != 0 || continuation_classifier >= terms->term_count ||
			terms->terms[clause->body].as.lambda.body >= terms->term_count) {
			return -1;
		}
		uint32_t continuation_pi;
		uint32_t continuation_domain;
		uint32_t continuation_family;
		int continuation_selection = lookup_delta_proven_classifier_normalization_equal(
			delta,
			terms,
			type_declarations,
			continuation_lambda,
			expected_continuation_classifier,
			&continuation_classifier
		);
		if (continuation_selection < 0) {
			return -1;
		}
		if (continuation_selection > 0 || classifier_kernel_as_pi(
				terms, type_declarations, NULL, continuation_classifier,
				&continuation_pi, &continuation_domain, &continuation_family
			) != 0) {
			return -1;
		}
		uint32_t continuation_binder;
		uint32_t continuation_var;
		uint32_t continuation_output;
		uint32_t ignored_continuation_body;
		if (prototype_term_pure_family_parts(
				terms,
				continuation_family,
				&continuation_binder,
				&ignored_continuation_body
			) != 0 ||
			prototype_term_var(terms, continuation_binder, &continuation_var) != 0 ||
			pi_codomain_at_fresh_binder(
				delta, terms, type_declarations, delta->current_context_id,
				continuation_pi, continuation_var, continuation_domain,
				PROTOTYPE_INVALID_ID, &continuation_output
			) != 0 || !prototype_judgement_classifier_normalization_equal(
				terms, type_declarations, continuation_output, handled_output_classifier
			)) {
			return -1;
		}
		outer_classifiers[i] = outer_classifier;
	}

	if (dependent_output) {
		/* The return value needed to instantiate the computation-fold result family is
		 * produced by the handled computation.  The clause assumptions above
		 * are sufficient to classify the occurrence-local operation body, but
		 * no closed computation-fold classifier exists until runtime supplies that value.
		 * The source compiler records a computation-fold result obligation instead of
		 * publishing a closed fold-elimination proof. */
		return 1;
	}
	uint32_t subjects[64];
	uint32_t classifiers[64];
	subjects[0] = fold->as.computation_fold.computation;
	subjects[1] = fold->as.computation_fold.return_clause;
	classifiers[0] = input_classifier;
	classifiers[1] = return_classifier;
	for (uint32_t i = 0; i < clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[
				fold->as.computation_fold.first_clause + i
			];
		subjects[2 + 2 * i] = clause->operation;
		subjects[3 + 2 * i] = clause->body;
		classifiers[2 + 2 * i] = operation_classifiers[i];
		classifiers[3 + 2 * i] = outer_classifiers[i];
	}
	return add_delta_relation_with_premises(
		delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, constraint->subject,
		handled_output_classifier, PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM,
		subjects, classifiers, 2 + 2 * clause_count
	);
}

static int solve_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	for (size_t i = 0; i < delta->computation_constraint_count; ++i) {
		int budget_status = consume_solver_step(delta);
		if (budget_status != 0) {
			return budget_status;
		}
		struct prototype_judgement_computation_constraint* constraint =
			&delta->computation_constraints[i];
		delta->current_context_id = constraint->context_id;
		if (constraint->kind == PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_FOLD) {
			if (constraint->subject >= terms->term_count ||
				terms->terms[constraint->subject].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
				return -1;
			}
			const struct prototype_term* fold = &terms->terms[constraint->subject];
			int status = fold->as.computation_fold.clause_count == 0 ?
				solve_zero_clause_computation_fold_constraint(
					delta, terms, type_declarations, constraint
				) : solve_clause_computation_fold_constraint(
					delta, terms, type_declarations, constraint
				);
			if (status < 0) {
				return -1;
			}
		} else if (constraint->kind ==
			PROTOTYPE_JUDGEMENT_COMPUTATION_CONSTRAINT_OPERATION_REQUEST) {
			int status = solve_operation_request_constraint(
				delta, terms, type_declarations, constraint
			);
			if (status < 0) {
				return -1;
			}
		}
	}
	return solve_effect_row_constraints(delta, terms);
}

int prototype_judgement_delta_infer_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations ||
		prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}

	for (;;) {
		size_t before = delta->relation_count;
		if (prototype_judgement_delta_infer_cbpv_boundaries(
				delta, terms, type_declarations
			) != 0) {
			return -1;
		}
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->relation_count == before) {
			return 0;
		}
	}
}

int prototype_judgement_delta_solve_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations ||
		prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}
	for (;;) {
		size_t before = delta->relation_count;
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->relation_count == before) {
			return 0;
		}
	}
}

int prototype_judgement_delta_solve_recorded_computation_constraints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	for (;;) {
		size_t before = delta->relation_count;
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->relation_count == before) {
			return 0;
		}
	}
}

int prototype_judgement_delta_infer_term_classifiers(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	if (prototype_judgement_delta_generate_computation_constraints(delta, terms) != 0) {
		return -1;
	}

	int changed = 1;
	while (changed) {
		changed = 0;
			uint32_t term_count = (uint32_t)terms->term_count;
			for (uint32_t i = 0; i < term_count; ++i) {
				uint32_t existing;
				if (terms->terms[i].tag != PROTOTYPE_TERM_LAMBDA &&
					terms->terms[i].tag != PROTOTYPE_TERM_APP &&
					lookup_delta_proven_classifier(delta, terms, i, &existing) == 0) {
					continue;
				}
				if (terms->terms[i].tag == PROTOTYPE_TERM_APP) {
					size_t before = delta->relation_count;
					int status = prototype_judgement_delta_expand_app(
						delta,
						terms,
					type_declarations,
					i,
						NULL
					);
					if (status < 0) {
						return -1;
					}
					if (delta->relation_count > before) {
						changed = 1;
					}
			} else if (terms->terms[i].tag == PROTOTYPE_TERM_LAMBDA) {
				if (infer_lambda_classifiers_from_body(
						delta,
						terms,
							type_declarations,
							i,
							&changed
						) != 0) {
						return -1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_RETURN ||
					terms->terms[i].tag == PROTOTYPE_TERM_THUNK ||
					terms->terms[i].tag == PROTOTYPE_TERM_FORCE) {
					size_t before = delta->relation_count;
					int status = infer_cbpv_boundary_classifier(delta, terms, type_declarations, i);
					if (status < 0) {
						return -1;
					}
					if (delta->relation_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_TEXT_LITERAL) {
					size_t before = delta->relation_count;
					if (infer_text_literal_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->relation_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_INT_LITERAL) {
					size_t before = delta->relation_count;
					if (infer_int_literal_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->relation_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
					size_t before = delta->relation_count;
					if (infer_pure_primitive_classifier(
							delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
					if (delta->relation_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
					size_t before = delta->relation_count;
					if (infer_effect_operation_classifier(delta, terms, i) != 0) {
						return -1;
					}
					if (delta->relation_count > before) {
						changed = 1;
					}
				} else if (terms->terms[i].tag == PROTOTYPE_TERM_CONSTRUCTOR) {
					size_t before = delta->relation_count;
				if (infer_constructor_classifier(
						delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
					if (delta->relation_count > before) {
						changed = 1;
					}
				} else if (type_instance_has_known_type(terms, type_declarations, i)) {
					size_t before = delta->relation_count;
				if (infer_type_formation_classifier(
						delta,
							terms,
							type_declarations,
							i
						) != 0) {
						return -1;
					}
				if (delta->relation_count > before) {
					changed = 1;
				}
			}
		}
		size_t before = delta->relation_count;
		int solve_status = solve_computation_constraints(
			delta, terms, type_declarations
		);
		if (solve_status != 0) {
			return solve_status;
		}
		if (delta->relation_count > before) {
			changed = 1;
		}
	}
	return 0;
}

int prototype_judgement_expand_match_motive(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_is_universe_var(terms, classifier) ||
		terms->terms[subject].as.match.case_count != 0) {
		return -1;
	}
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		NULL,
		NULL,
		NULL,
		0
	);
}

static int prototype_judgement_delta_add_conversion(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t expected,
	uint32_t actual
) {
	if (!delta ||
		!term_exists(terms, subject) ||
		!term_exists(terms, expected) ||
		!term_exists(terms, actual)) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 16; ++depth) {
		uint32_t next = PROTOTYPE_INVALID_ID;
		for (size_t i = delta->relation_count; i > 0; --i) {
			const struct prototype_judgement_relation* relation = &delta->relations[i - 1];
			if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->subject == subject &&
				relation->classifier == actual &&
				relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION &&
				relation->proof_id < delta->proof_count &&
				delta->proofs[relation->proof_id].premise_count == 1) {
				next = delta->proofs[relation->proof_id].premise_classifiers[0];
				break;
			}
		}
		if (next == PROTOTYPE_INVALID_ID && delta->db) {
			for (size_t i = delta->db->relation_count; i > 0; --i) {
				const struct prototype_judgement_relation* relation =
					&delta->db->relations[i - 1];
				if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
					relation->subject == subject &&
					relation->classifier == actual &&
					relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION &&
					relation->proof_id < delta->db->proof_count &&
					delta->db->proofs[relation->proof_id].premise_count == 1) {
					next = delta->db->proofs[relation->proof_id].premise_classifiers[0];
					break;
				}
			}
		}
		if (next == PROTOTYPE_INVALID_ID || next == actual) {
			break;
		}
		actual = next;
	}
	if (expected == actual) {
		return 0;
	}
	uint32_t premise_subjects[1] = { subject };
	uint32_t premise_classifiers[1] = { actual };
	return add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		expected,
		PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
		premise_subjects,
		premise_classifiers,
		1
	);
}

static int prototype_judgement_delta_add_universe_cumulativity(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_is_universe_var(terms, subject) ||
		!term_is_universe_var(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY
	);
}

static int prototype_judgement_delta_ensure_type_at_universe(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t universe
) {
	if (!delta || !terms || !type_declarations ||
		!term_exists(terms, subject) ||
		!term_is_universe_var(terms, universe)) {
		return -1;
	}
	if (term_is_universe_var(terms, subject)) {
		return prototype_judgement_delta_add_universe_cumulativity(
			delta,
			terms,
			subject,
			universe
		);
	}
	if (type_instance_has_known_type(terms, type_declarations, subject)) {
		return add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			universe,
			PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_PI)) {
		uint32_t domain;
		uint32_t codomain_family;
		if (pi_parts(terms, subject, &domain, &codomain_family) != 0) {
			return -1;
		}
		if (prototype_term_pure_family_lambda(
				terms,
				codomain_family,
				NULL
			) != 0) {
			return -1;
		}
		uint32_t codomain_binder;
		uint32_t codomain;
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[subject].as.pi.codomain_family,
				&codomain_binder,
				&codomain
			) != 0) {
			return -1;
		}
		if (ensure_lambda_binder_assumption(
				delta, terms, codomain_family, domain
			) != 0) {
			return -1;
		}
		if (prototype_judgement_delta_ensure_type_at_universe(
				delta,
				terms,
				type_declarations,
				domain,
				universe
			) != 0 ||
			prototype_judgement_delta_ensure_type_at_universe(
				delta,
				terms,
				type_declarations,
				codomain,
				universe
			) != 0) {
			return -1;
		}
		uint32_t lambda_universe_family;
		uint32_t lambda_classifier;
		uint32_t returned;
		uint32_t empty_effects;
		uint32_t returned_classifier;
		uint32_t family_classifier;
		if (prototype_term_effect_label(
					terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects
				) != 0 || prototype_term_computation_type(
					terms, empty_effects, universe, &returned_classifier
				) != 0 || prototype_term_pure_family(
					terms, codomain_binder, returned_classifier, &lambda_universe_family
				) != 0 || prototype_term_pi_family(
					terms, domain, lambda_universe_family, &lambda_classifier
				) != 0 ||
			codomain_family >= terms->term_count ||
			terms->terms[subject].as.pi.codomain_family >= terms->term_count) {
			return -1;
		}
		uint32_t family = terms->terms[subject].as.pi.codomain_family;
		returned = terms->terms[family].as.thunk.computation;
		if (returned >= terms->term_count ||
			terms->terms[returned].tag != PROTOTYPE_TERM_LAMBDA ||
			terms->terms[returned].as.lambda.binder_id != codomain_binder ||
			terms->terms[returned].as.lambda.body >= terms->term_count ||
			terms->terms[terms->terms[returned].as.lambda.body].tag != PROTOTYPE_TERM_RETURN ||
			terms->terms[terms->terms[returned].as.lambda.body].as.return_term.value != codomain ||
			prototype_term_thunk_type(
				terms, returned_classifier, &family_classifier
			) != 0) {
			return -1;
		}
		uint32_t returned_body = terms->terms[returned].as.lambda.body;
		uint32_t return_subjects[1] = { codomain };
		uint32_t return_classifiers[1] = { universe };
		if (add_delta_relation_with_premises(
				delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, returned_body, returned_classifier,
				PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
				return_subjects, return_classifiers, 1
			) != 0) {
			return -1;
		}
		if (prototype_judgement_delta_record_lambda_intro(
				delta,
				terms,
				type_declarations,
				returned,
				lambda_classifier,
				domain,
				returned_classifier,
				delta->current_context_id
			) != 0) {
			return -1;
		}
		uint32_t thunk_subjects[1] = { returned };
		uint32_t thunk_classifiers[1] = { lambda_classifier };
		if (add_delta_relation_with_premises(
				delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, family, family_classifier,
				PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
				thunk_subjects, thunk_classifiers, 1
			) != 0) {
			return -1;
		}
		uint32_t premise_subjects[3] = {
			domain,
			codomain,
			family
		};
		uint32_t premise_classifiers[3] = {
			universe,
			universe,
			family_classifier
		};
		return add_delta_relation_with_premises(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			universe,
			PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO,
			premise_subjects,
			premise_classifiers,
			3
		);
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_APP)) {
		uint32_t inferred;
		int status = prototype_judgement_delta_expand_app(
			delta,
			terms,
			type_declarations,
			subject,
			&inferred
		);
		if (status < 0) {
			return -1;
		}
			if (status > 0) {
				const struct prototype_term* app = &terms->terms[subject];
				if (term_has_tag(terms, app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
					uint32_t argument_classifiers[32];
					uint32_t argument_classifier_count = 0;
					if (collect_subject_classifiers(
							delta,
							terms,
							type_declarations,
							app->as.app.argument,
							argument_classifiers,
							32,
							&argument_classifier_count
						) != 0) {
						return -1;
					}
					for (uint32_t i = 0; i < argument_classifier_count; ++i) {
						uint32_t argument_classifier = argument_classifiers[i];
						const struct prototype_term* lambda = &terms->terms[app->as.app.function];
						uint32_t binder_var;
						if (prototype_term_var(
								terms,
								lambda->as.lambda.binder_id,
								&binder_var
							) != 0 ||
							ensure_lambda_binder_assumption(
								delta,
								terms,
								app->as.app.function,
								argument_classifier
							) != 0) {
							return -1;
						}
						if (prototype_judgement_delta_ensure_type_at_universe(
								delta,
								terms,
								type_declarations,
								lambda->as.lambda.body,
								universe
							) != 0) {
							return -1;
						}
						uint32_t classifier;
						if (prototype_term_pi(
								terms,
								argument_classifier,
								universe,
								&classifier
							) != 0) {
							return -1;
						}
						uint32_t premise_subjects[2] = {
							binder_var,
							lambda->as.lambda.body
						};
						uint32_t premise_classifiers[2] = {
							argument_classifier,
							universe
						};
						if (add_delta_relation_with_premises(
								delta,
								PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
								app->as.app.function,
								classifier,
								PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
								premise_subjects,
								premise_classifiers,
								2
							) != 0) {
							return -1;
						}
					}
				}
			if (prototype_judgement_delta_infer_term_classifiers(
					delta,
					terms,
					type_declarations
				) != 0) {
				return -1;
			}
			status = prototype_judgement_delta_expand_app(
				delta,
				terms,
				type_declarations,
				subject,
				&inferred
			);
			if (status < 0) {
				return -1;
			}
		}
	}
	if (term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH)) {
		const struct prototype_term* match = &terms->terms[subject];
		if (match->as.match.case_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			uint32_t case_id = match->as.match.first_case + i;
			if (case_id >= terms->case_count) {
				return -1;
			}
			uint32_t body = terms->cases[case_id].body;
			if (prototype_judgement_delta_ensure_type_at_universe(
					delta,
					terms,
					type_declarations,
					body,
					universe
				) != 0) {
				return -1;
			}
			premise_subjects[i] = body;
			premise_classifiers[i] = universe;
		}
		if (prototype_judgement_delta_expand_match_motive_with_premises(
				delta,
				terms,
				subject,
				universe,
				premise_subjects,
				premise_classifiers,
				match->as.match.case_count
			) != 0) {
			return -1;
		}
	}
	uint32_t actual;
	uint32_t actual_candidates[32];
	uint32_t actual_candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			subject,
			actual_candidates,
			32,
			&actual_candidate_count
		) != 0) {
		return -1;
	}
	actual = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < actual_candidate_count; ++i) {
		if (!prototype_judgement_classifier_compatible(
				terms,
				type_declarations,
				universe,
				actual_candidates[i]
			)) {
			continue;
		}
		if (actual != PROTOTYPE_INVALID_ID &&
			!prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				actual,
				actual_candidates[i]
			)) {
			return -1;
		}
		actual = actual_candidates[i];
	}
	if (actual == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (actual == universe) {
		return 0;
	}
	return prototype_judgement_delta_add_conversion(
		delta,
		terms,
		subject,
		universe,
		actual
	);
}

static int prototype_judgement_delta_expand_match_motive_with_premises(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
) {
	if (!delta || !premise_subjects || !premise_classifiers) {
		return -1;
	}
	if (!term_exists(terms, subject) ||
		!term_is_universe_var(terms, classifier) ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		premise_count != terms->terms[subject].as.match.case_count) {
		return -1;
	}
	for (uint32_t i = 0; i < premise_count; ++i) {
		uint32_t case_id = terms->terms[subject].as.match.first_case + i;
		if (case_id >= terms->case_count ||
			premise_subjects[i] != terms->cases[case_id].body ||
			premise_classifiers[i] != classifier) {
			return -1;
		}
	}
	return add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		premise_subjects,
		premise_classifiers,
		premise_count
	);
}

int prototype_judgement_delta_expand_match_motive(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_is_universe_var(terms, classifier) ||
		terms->terms[subject].as.match.case_count != 0) {
		return -1;
	}
	return add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO,
		NULL,
		NULL,
		0
	);
}

int prototype_judgement_delta_build_match_motive(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t scrutinee,
	uint32_t scrutinee_classifier,
	const struct prototype_match_case_input* motive_cases,
	uint32_t case_count,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !motive_cases || !p_motive_result ||
		scrutinee >= terms->term_count ||
		scrutinee_classifier >= terms->term_count ||
		case_count > 64) {
		return -1;
	}
	(void)type_declarations;

	uint32_t motive_binder_id = prototype_term_fresh_binder(terms);
	uint32_t motive_binder_var;
	uint32_t motive_body;
	uint32_t motive_lambda;
	uint32_t motive_pi;
	uint32_t motive_universe;
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	if (motive_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	if (prototype_term_var(
		terms,
		motive_binder_id,
		&motive_binder_var
	) != 0) {
		return -1;
	}
	if (prototype_term_match(
		terms,
		motive_binder_var,
		motive_cases,
		case_count,
		&motive_body
	) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t case_id = terms->terms[motive_body].as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* motive_case = &terms->cases[case_id];
		for (uint32_t j = 0; j < motive_case->binder_count; ++j) {
			const struct prototype_case_binder* binder =
				&terms->case_binders[motive_case->first_binder + j];
			uint32_t binder_var;
			uint32_t binder_classifier;
			size_t before = delta->relation_count;
			if (prototype_term_var(
					terms,
					binder->binder_id,
					&binder_var
				) != 0 ||
				constructor_field_classifier_from_spine(
					delta,
					terms,
					type_declarations,
					scrutinee_classifier,
					motive_case->constructor_id,
					&terms->case_binders[motive_case->first_binder],
					j,
					j,
					&binder_classifier
				) != 0 ||
				prototype_judgement_delta_expand_match_pattern(
					delta,
					terms,
					binder_var,
					binder_classifier
			) != 0) {
				return -1;
			}
			if (delta->relation_count > before) {
				uint32_t proof_id =
					delta->relations[delta->relation_count - 1].proof_id;
				if (prototype_judgement_delta_set_match_pattern_owner_by_id(
						delta,
						proof_id,
						scrutinee_classifier,
						motive_case->constructor_id,
						j
					) != 0) {
					return -1;
				}
			}
		}
	}
	if (prototype_term_universe_var(
		terms,
		universe_level_var,
		&motive_universe
	) != 0) {
		return -1;
	}
	if (case_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t case_id = terms->terms[motive_body].as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		uint32_t motive_case_body = terms->cases[case_id].body;
			if (prototype_judgement_delta_ensure_type_at_universe(
					delta,
					terms,
					type_declarations,
					motive_case_body,
					motive_universe
				) != 0) {
				return -1;
			}
		premise_subjects[i] = motive_case_body;
		premise_classifiers[i] = motive_universe;
	}
		if (prototype_judgement_delta_expand_match_motive_with_premises(
			delta,
			terms,
		motive_body,
		motive_universe,
		premise_subjects,
		premise_classifiers,
			case_count
		) != 0) {
			return -1;
		}
		if (prototype_term_lambda(
			terms,
			motive_binder_id,
			motive_body,
			&motive_lambda
		) != 0) {
			return -1;
		}
		if (motive_lambda >= terms->term_count ||
			terms->terms[motive_lambda].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
			}
			motive_binder_id = terms->terms[motive_lambda].as.lambda.binder_id;
			motive_body = terms->terms[motive_lambda].as.lambda.body;
			if (prototype_term_var(
					terms,
					motive_binder_id,
					&motive_binder_var
				) != 0 ||
				ensure_lambda_binder_assumption(
					delta,
					terms,
					motive_lambda,
					scrutinee_classifier
				) != 0) {
				return -1;
			}
	if (prototype_term_pi(
		terms,
		scrutinee_classifier,
		motive_universe,
		&motive_pi
	) != 0) {
		return -1;
	}
	uint32_t lambda_premise_subjects[2] = {
		motive_binder_var,
		motive_body
	};
	uint32_t lambda_premise_classifiers[2] = {
		scrutinee_classifier,
		motive_universe
	};
		if (add_delta_relation_with_premises(
			delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		motive_lambda,
		motive_pi,
		PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
		lambda_premise_subjects,
		lambda_premise_classifiers,
			2
		) != 0) {
			return -1;
		}
	uint32_t motive_family;
	uint32_t motive_return;
	uint32_t motive_return_classifier;
	uint32_t motive_family_classifier;
	uint32_t empty_effects;
	uint32_t recovered_motive_lambda;
	if (prototype_term_pure_family(
			terms, motive_binder_id, motive_body, &motive_family
		) != 0 || motive_family >= terms->term_count ||
		terms->terms[motive_family].tag != PROTOTYPE_TERM_THUNK ||
		(motive_return = terms->terms[motive_family].as.thunk.computation) >= terms->term_count ||
		terms->terms[motive_return].tag != PROTOTYPE_TERM_RETURN ||
		terms->terms[motive_return].as.return_term.value != motive_lambda ||
		prototype_term_effect_label(
			terms, PROTOTYPE_EFFECT_OPERATION_LABEL_NONE, &empty_effects
		) != 0 || prototype_term_computation_type(
			terms, empty_effects, motive_pi, &motive_return_classifier
		) != 0 || prototype_term_thunk_type(
			terms, motive_return_classifier, &motive_family_classifier
		) != 0 || prototype_term_pure_family_lambda(
			terms, motive_family, &recovered_motive_lambda
		) != 0 || recovered_motive_lambda != motive_lambda) {
		return -1;
	}
	uint32_t return_premise_subjects[1] = { motive_lambda };
	uint32_t return_premise_classifiers[1] = { motive_pi };
	if (add_delta_relation_with_premises(
			delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, motive_return,
			motive_return_classifier, PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO,
			return_premise_subjects, return_premise_classifiers, 1
		) != 0) {
		return -1;
	}
	uint32_t thunk_premise_subjects[1] = { motive_return };
	uint32_t thunk_premise_classifiers[1] = { motive_return_classifier };
	if (add_delta_relation_with_premises(
			delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, motive_family,
			motive_family_classifier, PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO,
			thunk_premise_subjects, thunk_premise_classifiers, 1
		) != 0) {
		return -1;
	}
	if (prototype_term_app(terms, recovered_motive_lambda, scrutinee, p_motive_result) != 0) {
		return -1;
	}
	uint32_t app_premise_subjects[2] = {
		recovered_motive_lambda,
		scrutinee
	};
	uint32_t app_premise_classifiers[2] = {
		motive_pi,
		scrutinee_classifier
	};
		return add_delta_relation_with_premises(
			delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		*p_motive_result,
		motive_universe,
		PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
		app_premise_subjects,
		app_premise_classifiers,
		2
	);
}

/*
 * Compare already-erased Match cases whose locally bound core IDs differ.
 * Source elaboration must use ContextDB/SubstitutionDB instead; this private
 * helper is only an alpha transport for legacy TermDB proof validation.
 */
static int core_match_case_alpha_reindex(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_case_binder* source_binders,
	const struct prototype_case_binder* motive_binders,
	uint32_t binder_count,
	uint32_t branch_classifier,
	uint32_t* p_motive_body
) {
	if (!terms || !type_declarations || !p_motive_body ||
		branch_classifier >= terms->term_count ||
		(binder_count > 0 && (!source_binders || !motive_binders))) {
		return -1;
	}

	uint32_t current = branch_classifier;
	for (uint32_t i = 0; i < binder_count; ++i) {
		uint32_t motive_binder_var;
		if (prototype_term_var(
			terms,
			motive_binders[i].binder_id,
			&motive_binder_var
		) != 0 ||
			prototype_term_graph_substitute_bound_var(
				terms,
				type_declarations,
				current,
				source_binders[i].binder_id,
				motive_binder_var,
				&current
			) != 0) {
			return -1;
		}
	}
	*p_motive_body = current;
	return 0;
}

static int classifier_list_contains_normalization_equal(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const uint32_t* classifiers,
	uint32_t classifier_count,
	uint32_t candidate
) {
	for (uint32_t i = 0; i < classifier_count; ++i) {
		if (prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				classifiers[i],
				candidate
			)) {
			return 1;
		}
	}
	return 0;
}

static int collect_subject_classifiers(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
) {
	if (!delta || !terms || !type_declarations || !classifiers ||
		!p_classifier_count || subject >= terms->term_count) {
		return -1;
	}
	*p_classifier_count = 0;
	for (int source = 0; source < 2; ++source) {
		const struct prototype_judgement_relation* relations =
			source == 0 ? delta->relations : delta->db->relations;
		size_t relation_count =
			source == 0 ? delta->relation_count : delta->db->relation_count;
		for (size_t i = 0; i < relation_count; ++i) {
			const struct prototype_judgement_relation* relation = &relations[i];
			if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				relation->subject != subject ||
				relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
				continue;
			}
			if (classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					classifiers,
					*p_classifier_count,
					relation->classifier
				)) {
				continue;
			}
			if (*p_classifier_count >= classifier_capacity) {
				return -1;
			}
			classifiers[(*p_classifier_count)++] = relation->classifier;
		}
	}
	return 0;
}

static int collect_judgement_subject_classifiers(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
) {
	if (!judgement || !terms || !type_declarations || !classifiers ||
		!p_classifier_count || subject >= terms->term_count) {
		return -1;
	}
	*p_classifier_count = 0;
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			relation->subject != subject ||
			relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
			continue;
		}
		if (classifier_list_contains_normalization_equal(
				terms,
				type_declarations,
				classifiers,
				*p_classifier_count,
				relation->classifier
			)) {
			continue;
		}
		if (*p_classifier_count >= classifier_capacity) {
			return -1;
		}
		classifiers[(*p_classifier_count)++] = relation->classifier;
	}
	return 0;
}

static int select_match_branch_classifier(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t branch_index,
	uint32_t* p_classifier
) {
	if (!delta || !terms || !type_declarations || !p_classifier ||
		!term_has_tag(terms, match_term, PROTOTYPE_TERM_MATCH)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	if (branch_index >= match->as.match.case_count) {
		return -1;
	}
	uint32_t case_id = match->as.match.first_case + branch_index;
	if (case_id >= terms->case_count) {
		return -1;
	}
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			terms->cases[case_id].body,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	if (candidate_count == 0) {
		return 1;
	}
	if (candidate_count == 1) {
		*p_classifier = candidates[0];
		return 0;
	}
	for (uint32_t i = 0; i < candidate_count; ++i) {
		for (uint32_t j = 0; j < match->as.match.case_count; ++j) {
			if (j == branch_index) {
				continue;
			}
			uint32_t other_case_id = match->as.match.first_case + j;
			if (other_case_id >= terms->case_count) {
				return -1;
			}
			uint32_t other_candidates[32];
			uint32_t other_candidate_count = 0;
			if (collect_subject_classifiers(
					delta,
					terms,
					type_declarations,
					terms->cases[other_case_id].body,
					other_candidates,
					32,
					&other_candidate_count
				) != 0) {
				return -1;
			}
			if (classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					other_candidates,
					other_candidate_count,
					candidates[i]
				)) {
				*p_classifier = candidates[i];
				return 0;
			}
		}
	}
	return 1;
}

static int match_motive_result_classifier(
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t classifier
) {
	if (!terms ||
		match_term >= terms->term_count ||
		classifier >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[classifier].tag != PROTOTYPE_TERM_APP) {
		return 0;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	const struct prototype_term* app = &terms->terms[classifier];
	return app->as.app.argument == match->as.match.scrutinee &&
		app->as.app.function < terms->term_count &&
		terms->terms[app->as.app.function].tag == PROTOTYPE_TERM_LAMBDA;
}

static int select_existing_match_motive_result(
	const struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t match_term,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	for (size_t i = delta->relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation = &delta->relations[i - 1];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == match_term &&
			match_motive_result_classifier(terms, match_term, relation->classifier)) {
			*p_motive_result = relation->classifier;
			return 0;
		}
	}
	for (size_t i = delta->match_motive_result_count; i > 0; --i) {
		const struct prototype_judgement_match_motive_result* result =
			&delta->match_motive_results[i - 1];
		if (result->match_term == match_term &&
			match_motive_result_classifier(terms, match_term, result->classifier)) {
			*p_motive_result = result->classifier;
			return 0;
		}
	}
	if (delta->db) {
		for (size_t i = delta->db->relation_count; i > 0; --i) {
			const struct prototype_judgement_relation* relation =
				&delta->db->relations[i - 1];
			if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->subject == match_term &&
				match_motive_result_classifier(terms, match_term, relation->classifier)) {
				*p_motive_result = relation->classifier;
				return 0;
			}
		}
	}
	return 1;
}

static int match_scrutinee_classifier_for_motive(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t* p_scrutinee_classifier
) {
	if (!delta || !terms || !type_declarations || !p_scrutinee_classifier ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t scrutinee_classifier = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			scrutinee_classifier = PROTOTYPE_INVALID_ID;
			break;
		}
		if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
			scrutinee_classifier = match_case->constructor_owner;
			continue;
		}
		int same_owner = 0;
		if (prototype_term_view_shape_equal(
				terms,
				scrutinee_classifier,
				match_case->constructor_owner,
				&same_owner
			) != 0) {
			return -1;
		}
		if (!same_owner) {
			return -1;
		}
	}
	if (scrutinee_classifier == PROTOTYPE_INVALID_ID) {
		uint32_t candidates[32];
		uint32_t candidate_count = 0;
		if (collect_subject_classifiers(
				delta,
				terms,
				type_declarations,
				match->as.match.scrutinee,
				candidates,
				32,
				&candidate_count
			) != 0) {
			return -1;
		}
		if (candidate_count != 1) {
			return 1;
		}
		scrutinee_classifier = candidates[0];
	}
	*p_scrutinee_classifier = scrutinee_classifier;
	return 0;
}

static int select_universe_classifier_from_candidates(
	const struct prototype_term_db* terms,
	const uint32_t* candidates,
	uint32_t candidate_count,
	uint32_t* p_classifier
) {
	if (!terms || !candidates || !p_classifier) {
		return -1;
	}
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < candidate_count; ++i) {
		if (!term_is_universe_var(terms, candidates[i])) {
			continue;
		}
		if (selected != PROTOTYPE_INVALID_ID && selected != candidates[i]) {
			return -1;
		}
		selected = candidates[i];
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = selected;
	return 0;
}

static int select_delta_universe_classifier(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			subject,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	return select_universe_classifier_from_candidates(
		terms,
		candidates,
		candidate_count,
		p_classifier
	);
}

static int select_judgement_universe_classifier(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t* p_classifier
) {
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_judgement_subject_classifiers(
			judgement,
			terms,
			type_declarations,
			subject,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	return select_universe_classifier_from_candidates(
		terms,
		candidates,
		candidate_count,
		p_classifier
	);
}

static int select_match_branch_classifier_for_motive_from_candidates(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	const struct prototype_match_case* motive_case,
	const uint32_t* candidates,
	uint32_t candidate_count,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !match_case || !motive_case ||
		!candidates || !p_classifier) {
		return -1;
	}
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < candidate_count; ++i) {
		uint32_t expected_motive_case_body;
		if (core_match_case_alpha_reindex(
				terms,
				type_declarations,
				&terms->case_binders[match_case->first_binder],
				&terms->case_binders[motive_case->first_binder],
				match_case->binder_count,
				candidates[i],
				&expected_motive_case_body
			) != 0) {
			return -1;
		}
		if (!prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				expected_motive_case_body,
				motive_case->body
			)) {
			continue;
		}
		if (selected != PROTOTYPE_INVALID_ID &&
			!prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				selected,
				candidates[i]
			)) {
			return -1;
		}
		selected = candidates[i];
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_classifier = selected;
	return 0;
}

static int select_delta_match_branch_classifier_for_motive(
	const struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	const struct prototype_match_case* motive_case,
	uint32_t* p_classifier
) {
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_subject_classifiers(
			delta,
			terms,
			type_declarations,
			match_case->body,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	return select_match_branch_classifier_for_motive_from_candidates(
		terms,
		type_declarations,
		match_case,
		motive_case,
		candidates,
		candidate_count,
		p_classifier
	);
}

static int select_judgement_match_branch_classifier_for_motive(
	const struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	const struct prototype_match_case* motive_case,
	uint32_t* p_classifier
) {
	uint32_t candidates[32];
	uint32_t candidate_count = 0;
	if (collect_judgement_subject_classifiers(
			judgement,
			terms,
			type_declarations,
			match_case->body,
			candidates,
			32,
			&candidate_count
		) != 0) {
		return -1;
	}
	return select_match_branch_classifier_for_motive_from_candidates(
		terms,
		type_declarations,
		match_case,
		motive_case,
		candidates,
		candidate_count,
		p_classifier
	);
}

static int build_match_motive_from_branch_classifiers(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	const uint32_t* branch_classifiers,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !branch_classifiers || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[match_term].as.match.case_count > 64) {
		return -1;
	}

	int existing_status = select_existing_match_motive_result(
		delta,
		terms,
		match_term,
		p_motive_result
	);
	if (existing_status < 0) {
		return -1;
	}
	if (existing_status == 0) {
		return 0;
	}

	uint32_t scrutinee_classifier;
	int scrutinee_status = match_scrutinee_classifier_for_motive(
		delta,
		terms,
		type_declarations,
		match_term,
		&scrutinee_classifier
	);
	if (scrutinee_status != 0) {
		return scrutinee_status;
	}

	const struct prototype_term* match = &terms->terms[match_term];
	struct prototype_match_case_input motive_cases[64];
	struct prototype_case_binder motive_binders[256];
	uint32_t motive_binder_cursor = 0;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		if (branch_classifiers[i] >= terms->term_count) {
			return -1;
		}
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			match_case->constructor_id == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		if (motive_binder_cursor + match_case->binder_count > 256) {
			return -1;
		}
		motive_cases[i].case_label_symbol_id = terms->case_label_symbols[case_id];
		motive_cases[i].constructor_owner = match_case->constructor_owner;
		motive_cases[i].constructor_id = match_case->constructor_id;
		motive_cases[i].binders = &motive_binders[motive_binder_cursor];
		motive_cases[i].binder_count = match_case->binder_count;
		for (uint32_t j = 0; j < match_case->binder_count; ++j) {
			uint32_t motive_binder_id = prototype_term_fresh_binder(terms);
			if (motive_binder_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			motive_binders[motive_binder_cursor + j].binder_id = motive_binder_id;
		}
		if (core_match_case_alpha_reindex(
				terms,
				type_declarations,
					&terms->case_binders[match_case->first_binder],
					&motive_binders[motive_binder_cursor],
					match_case->binder_count,
					branch_classifiers[i],
					&motive_cases[i].body
				) != 0) {
				return -1;
			}
		motive_binder_cursor += match_case->binder_count;
	}

	uint32_t motive_result;
	int status = prototype_judgement_delta_build_match_motive(
		delta,
		terms,
		type_declarations,
		match->as.match.scrutinee,
		scrutinee_classifier,
		motive_cases,
		match->as.match.case_count,
		universe_level_var,
		&motive_result
	);
	if (status != 0) {
		return status;
	}
	*p_motive_result = motive_result;
	return add_match_motive_result(delta, match_term, motive_result);
}

int prototype_judgement_delta_build_match_motive_from_known_branches(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	int existing_status = select_existing_match_motive_result(
		delta,
		terms,
		match_term,
		p_motive_result
	);
	if (existing_status < 0) {
		return -1;
	}
	if (existing_status == 0) {
		return 0;
	}

	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t common[32];
	uint32_t common_count = 0;
	int found_known_branch = 0;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		uint32_t branch_classifiers[32];
		uint32_t branch_classifier_count = 0;
		if (collect_subject_classifiers(
				delta,
				terms,
				type_declarations,
				terms->cases[case_id].body,
				branch_classifiers,
				32,
				&branch_classifier_count
			) != 0) {
			return -1;
		}
		if (branch_classifier_count == 0) {
			continue;
		}
		if (!found_known_branch) {
			for (uint32_t j = 0; j < branch_classifier_count; ++j) {
				common[common_count++] = branch_classifiers[j];
			}
			found_known_branch = 1;
			continue;
		}
		uint32_t write = 0;
		for (uint32_t j = 0; j < common_count; ++j) {
			if (classifier_list_contains_normalization_equal(
					terms,
					type_declarations,
					branch_classifiers,
					branch_classifier_count,
					common[j]
				)) {
				common[write++] = common[j];
			}
		}
		common_count = write;
		if (common_count == 0) {
			return 1;
		}
	}
	if (!found_known_branch || common_count != 1) {
		return 1;
	}
	uint32_t branch_classifiers[64];
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		branch_classifiers[i] = common[0];
	}
	return build_match_motive_from_branch_classifiers(
		delta,
		terms,
		type_declarations,
		match_term,
		branch_classifiers,
		universe_level_var,
		p_motive_result
	);
}

int prototype_judgement_delta_build_match_motive_from_branch_hints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	const uint32_t* branch_classifiers,
	uint32_t branch_count,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !branch_classifiers || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		branch_count != terms->terms[match_term].as.match.case_count || branch_count > 64) {
		return -1;
	}
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < branch_count; ++i) {
		uint32_t classifier = branch_classifiers[i];
		if (classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (classifier >= terms->term_count) {
			return -1;
		}
		if (selected != PROTOTYPE_INVALID_ID &&
			!prototype_judgement_classifier_normalization_equal(
				terms, type_declarations, selected, classifier
			)) {
			return 1;
		}
		selected = classifier;
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t uniform_classifiers[64];
	for (uint32_t i = 0; i < branch_count; ++i) {
		uniform_classifiers[i] = selected;
	}
	return build_match_motive_from_branch_classifiers(
		delta,
		terms,
		type_declarations,
		match_term,
		uniform_classifiers,
		universe_level_var,
		p_motive_result
	);
}

int prototype_judgement_delta_build_match_motive_from_cases(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[match_term].as.match.case_count > 64) {
		return -1;
	}

	const struct prototype_term* match = &terms->terms[match_term];
	uint32_t branch_classifiers[64];
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		if (case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		uint32_t branch_classifier;
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			match_case->constructor_id == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		if (select_match_branch_classifier(
				delta,
				terms,
				type_declarations,
				match_term,
				i,
				&branch_classifier
			) != 0) {
			int infer_status = prototype_judgement_delta_infer_term_classifiers(
				delta,
				terms,
				type_declarations
			);
			if (infer_status != 0) {
				return -1;
			}
			if (select_match_branch_classifier(
					delta,
					terms,
					type_declarations,
					match_term,
					i,
					&branch_classifier
				) != 0) {
				return 1;
			}
		}
		branch_classifiers[i] = branch_classifier;
	}
	return build_match_motive_from_branch_classifiers(
		delta,
		terms,
		type_declarations,
		match_term,
		branch_classifiers,
		universe_level_var,
		p_motive_result
	);
}

int prototype_judgement_delta_type_match_from_cases(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t match_term,
	uint32_t universe_level_var,
	uint32_t* p_motive_result
) {
	if (!delta || !terms || !type_declarations || !p_motive_result ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	uint32_t motive_result;
	int status = prototype_judgement_delta_build_match_motive_from_cases(
		delta,
		terms,
		type_declarations,
		match_term,
		universe_level_var,
		&motive_result
	);
	if (status != 0) {
		return status;
	}
	status = prototype_judgement_delta_expand_match(
		delta,
		terms,
		type_declarations,
		match_term,
		motive_result
	);
	if (status != 0) {
		return status;
	}
	*p_motive_result = motive_result;
	return 0;
}

int prototype_judgement_expand_match(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_has_tag(terms, classifier, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[subject];
	const struct prototype_term* motive_app = &terms->terms[classifier];
	const struct prototype_term* motive_lambda;
	const struct prototype_term* motive_body;
	uint32_t motive_result_classifier;
	uint32_t normalized_classifier;
	if (motive_app->as.app.argument != match->as.match.scrutinee ||
		!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA) ||
		classifier_kernel_whnf_no_definitions(terms, type_declarations, classifier, &normalized_classifier) != 0) {
		return -1;
	}
	if (select_judgement_universe_classifier(
			judgement,
			terms,
			type_declarations,
			classifier,
			&motive_result_classifier
		) != 0) {
		return -1;
	}
	(void)normalized_classifier;
	motive_lambda = &terms->terms[motive_app->as.app.function];
	if (!term_has_tag(terms, motive_lambda->as.lambda.body, PROTOTYPE_TERM_MATCH)) {
		return -1;
	}
	motive_body = &terms->terms[motive_lambda->as.lambda.body];
	if (motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binder_id !=
			motive_lambda->as.lambda.binder_id) {
		return -1;
	}
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	if (match->as.match.case_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t premise_count = match->as.match.case_count + 1;
	premise_subjects[0] = classifier;
	premise_classifiers[0] = motive_result_classifier;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_body->as.match.first_case + i;
		if (case_id >= terms->case_count ||
			motive_case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = &terms->cases[motive_case_id];
		if (match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count) {
			return -1;
		}
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			motive_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			if (match_case->constructor_owner != motive_case->constructor_owner) {
				return -1;
		}
	} else {
		int same_owner = 0;
		if (prototype_term_view_shape_equal(
				terms,
				match_case->constructor_owner,
				motive_case->constructor_owner,
				&same_owner
			) != 0 ||
			!same_owner) {
			return -1;
		}
		}
		if (select_judgement_match_branch_classifier_for_motive(
				judgement,
				terms,
				type_declarations,
				match_case,
				motive_case,
				&premise_classifiers[i + 1]
			) != 0) {
			return -1;
		}
		premise_subjects[i + 1] = terms->cases[case_id].body;
	}
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
		NULL,
		premise_subjects,
		premise_classifiers,
		premise_count
	);
}

int prototype_judgement_delta_expand_match(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_has_tag(terms, classifier, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[subject];
	const struct prototype_term* motive_app = &terms->terms[classifier];
	const struct prototype_term* motive_lambda;
	const struct prototype_term* motive_body;
	uint32_t motive_result_classifier;
	uint32_t normalized_classifier;
	if (motive_app->as.app.argument != match->as.match.scrutinee ||
		!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA) ||
		classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			classifier,
			&normalized_classifier
			) != 0) {
		return -1;
	}
	if (select_delta_universe_classifier(
			delta,
			terms,
			type_declarations,
			classifier,
			&motive_result_classifier
		) != 0) {
		return -1;
	}
	(void)normalized_classifier;
	motive_lambda = &terms->terms[motive_app->as.app.function];
	if (!term_has_tag(terms, motive_lambda->as.lambda.body, PROTOTYPE_TERM_MATCH)) {
		return -1;
	}
	motive_body = &terms->terms[motive_lambda->as.lambda.body];
	if (motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binder_id !=
			motive_lambda->as.lambda.binder_id) {
		return -1;
	}
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	if (match->as.match.case_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t premise_count = match->as.match.case_count + 1;
	premise_subjects[0] = classifier;
	premise_classifiers[0] = motive_result_classifier;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_body->as.match.first_case + i;
		if (case_id >= terms->case_count ||
			motive_case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = &terms->cases[motive_case_id];
		if (match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count) {
			return -1;
		}
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			motive_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			if (match_case->constructor_owner != motive_case->constructor_owner) {
				return -1;
			}
		} else {
			int same_owner = 0;
			if (prototype_term_view_shape_equal(
					terms,
					match_case->constructor_owner,
					motive_case->constructor_owner,
					&same_owner
				) != 0 ||
				!same_owner) {
				return -1;
			}
		}
		if (select_delta_match_branch_classifier_for_motive(
				delta,
				terms,
				type_declarations,
				match_case,
				motive_case,
				&premise_classifiers[i + 1]
			) != 0) {
			return -1;
		}
		premise_subjects[i + 1] = terms->cases[case_id].body;
	}
	int status = add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
		premise_subjects,
		premise_classifiers,
		premise_count
	);
	if (status == 0) {
		remove_match_motive_results_normalization_equal(
			delta,
			terms,
			type_declarations,
			subject,
			classifier
		);
	}
	return status;
}

int prototype_judgement_delta_expand_match_with_branch_hints(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	const uint32_t* branch_classifiers,
	uint32_t branch_count
) {
	if (!delta || !terms || !type_declarations || !branch_classifiers ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_MATCH) ||
		!term_has_tag(terms, classifier, PROTOTYPE_TERM_APP)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[subject];
	const struct prototype_term* motive_app = &terms->terms[classifier];
	if (branch_count != match->as.match.case_count ||
		branch_count + 1 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
		motive_app->as.app.argument != match->as.match.scrutinee ||
		!term_has_tag(terms, motive_app->as.app.function, PROTOTYPE_TERM_LAMBDA)) {
		return -1;
	}
	const struct prototype_term* motive_lambda =
		&terms->terms[motive_app->as.app.function];
	if (!term_has_tag(terms, motive_lambda->as.lambda.body, PROTOTYPE_TERM_MATCH)) {
		return -1;
	}
	const struct prototype_term* motive_body =
		&terms->terms[motive_lambda->as.lambda.body];
	if (motive_body->as.match.case_count != branch_count) {
		return -1;
	}
	uint32_t motive_universe;
	if (select_delta_universe_classifier(
			delta, terms, type_declarations, classifier, &motive_universe
		) != 0) {
		return -1;
	}
	uint32_t premise_subjects[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t premise_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	premise_subjects[0] = classifier;
	premise_classifiers[0] = motive_universe;
	for (uint32_t i = 0; i < branch_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_body->as.match.first_case + i;
		if (case_id >= terms->case_count || motive_case_id >= terms->case_count ||
			branch_classifiers[i] == PROTOTYPE_INVALID_ID ||
			branch_classifiers[i] >= terms->term_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = &terms->cases[motive_case_id];
		uint32_t expected_motive_case_body;
		if (match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count ||
			core_match_case_alpha_reindex(
				terms,
				type_declarations,
				&terms->case_binders[match_case->first_binder],
				&terms->case_binders[motive_case->first_binder],
				match_case->binder_count,
				branch_classifiers[i],
				&expected_motive_case_body
			) != 0 ||
			!prototype_judgement_classifier_normalization_equal(
				terms, type_declarations, expected_motive_case_body, motive_case->body
			)) {
			return -1;
		}
		premise_subjects[i + 1] = match_case->body;
		premise_classifiers[i + 1] = branch_classifiers[i];
	}
	int status = add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM,
		premise_subjects,
		premise_classifiers,
		branch_count + 1
	);
	if (status == 0) {
		remove_match_motive_results_normalization_equal(
			delta, terms, type_declarations, subject, classifier
		);
	}
	return status;
}

int prototype_judgement_delta_expand_induction_hypothesis(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier,
	uint32_t match_term,
	uint32_t case_index,
	uint32_t field_index
) {
	if (!delta ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) ||
		!term_exists(terms, classifier) ||
		match_term >= terms->term_count ||
		terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	size_t before = delta->relation_count;
	if (add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			classifier,
			PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM
		) != 0 ||
		delta->relation_count <= before) {
		return -1;
	}
	uint32_t proof_id = delta->relations[delta->relation_count - 1].proof_id;
	if (proof_id >= delta->proof_count) {
		return -1;
	}
	delta->proofs[proof_id].induction_match = match_term;
	delta->proofs[proof_id].induction_case_index = case_index;
	delta->proofs[proof_id].induction_field_index = field_index;
	return 0;
}

int prototype_judgement_expand_text_literal(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_TEXT_LITERAL) ||
		!term_has_tag(terms, classifier, PROTOTYPE_TERM_PRIMITIVE_TEXT)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO);
}

int prototype_judgement_expand_int_literal(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_has_tag(terms, subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_is_primitive_integer(terms, classifier)) {
		return -1;
	}
	if (term_is_primitive_int(terms, classifier) &&
		!int_literal_fits_int32(terms->terms[subject].as.int_literal.value)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO);
}

static int judgement_has_host_type_intro(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject
) {
	if (!judgement) {
		return 0;
	}
	if (!term_is_host_primitive(terms, subject)) {
		return 0;
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO) {
			return 1;
		}
	}
	return 0;
}

static int add_host_type_intro_classifier(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	uint32_t subject
) {
	if (!term_is_host_primitive(terms, subject)) {
		return -1;
	}
	if (!judgement_has_host_type_intro(judgement, terms, subject)) {
		uint32_t universe;
		if (prototype_term_universe_var(
				terms,
				judgement->next_universe_var++,
				&universe
			) != 0 ||
			add_relation(
				judgement,
				0,
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				subject,
				universe,
				PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static void sync_judgement_universe_counter(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms
) {
	if (!judgement || !terms) {
		return;
	}
	uint32_t next_level_var = 0;
	for (uint32_t i = 0; i < (uint32_t)terms->term_count; ++i) {
		if (terms->terms[i].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
			terms->terms[i].as.universe_var.level_var >= next_level_var) {
			next_level_var = terms->terms[i].as.universe_var.level_var + 1;
		}
	}
	if (judgement->next_universe_var < next_level_var) {
		judgement->next_universe_var = next_level_var;
	}
}

int prototype_judgement_expand_primitives(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms
) {
	if (!judgement || !terms) {
		return -1;
	}
	sync_judgement_universe_counter(judgement, terms);
	for (size_t i = 0; i < prototype_term_host_type_count(); ++i) {
		int host_type;
		uint32_t primitive;
		if (prototype_term_host_type_at(i, &host_type) != 0 ||
			prototype_term_make_host_type(terms, host_type, &primitive) != 0) {
			return -1;
		}
		if (add_host_type_intro_classifier(judgement, terms, primitive) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_judgement_delta_record_lambda_intro(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	uint32_t binder_classifier,
	uint32_t body_classifier,
	uint32_t premise_context_id
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_LAMBDA) ||
		!term_exists(terms, classifier) ||
		!term_exists(terms, binder_classifier) ||
		!term_exists(terms, body_classifier)) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[subject];
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t binder_var;
	uint32_t expected_body_classifier;
	uint32_t lambda_classifier;
	if (classifier_kernel_strip_effect_row_foralls(
			terms, type_declarations, NULL, classifier, &lambda_classifier
		) != 0 || classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			lambda_classifier,
			&lambda_pi,
			&domain,
			&codomain_family
		) != 0 ||
		prototype_term_var(terms, lambda->as.lambda.binder_id, &binder_var) != 0 ||
		pi_codomain_at_binder_in_context(
			delta,
			terms,
			type_declarations,
			premise_context_id,
			lambda_pi,
			binder_var,
			binder_classifier,
			PROTOTYPE_INVALID_ID,
			&expected_body_classifier
		) != 0 ||
		!prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, domain, binder_classifier
		) ||
		!prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			expected_body_classifier,
			body_classifier
		)) {
		return -1;
	}
	(void)codomain_family;
	uint32_t premise_subjects[2] = {
		binder_var,
		lambda->as.lambda.body
	};
	uint32_t premise_classifiers[2] = {
		binder_classifier,
		body_classifier
	};
	if (add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO,
		premise_subjects,
		premise_classifiers,
		2
	) != 0) {
		return -1;
	}
	for (size_t i = delta->relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation =
			&delta->relations[i - 1];
		if (relation->context_id == delta->current_context_id &&
			relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			relation->subject == subject &&
			relation->classifier == classifier &&
			relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO &&
			relation->proof_id < delta->proof_count) {
			struct prototype_judgement_proof* proof =
				&delta->proofs[relation->proof_id];
			proof->premise_context_ids[0] = premise_context_id;
			proof->premise_context_ids[1] = premise_context_id;
			return 0;
		}
	}
	return -1;
}

int prototype_judgement_delta_record_app_elim(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier,
	uint32_t function_subject,
	uint32_t function_classifier,
	uint32_t argument_subject,
	uint32_t argument_classifier
) {
	if (!delta || !terms || !type_declarations ||
		subject >= terms->term_count ||
		function_subject >= terms->term_count ||
		argument_subject >= terms->term_count ||
		!term_exists(terms, classifier) ||
		!term_exists(terms, function_classifier) ||
		!term_exists(terms, argument_classifier)) {
		return -1;
	}
	const struct prototype_term* app;
	if (term_core_app(terms, subject, &app) != 0) {
		return -1;
	}
	int function_matches = 0;
	int argument_matches = 0;
	if (prototype_term_core_shape_equal(
			terms, app->as.app.function, function_subject, &function_matches
		) != 0 ||
		prototype_term_core_shape_equal(
			terms, app->as.app.argument, argument_subject, &argument_matches
		) != 0 ||
		!function_matches || !argument_matches) {
		return -1;
	}
	uint32_t function_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t result_classifier;
	uint32_t specialized_function_classifier;
	int specialization_status = prototype_judgement_specialize_effect_rows_for_argument(
		terms,
		type_declarations,
		function_classifier,
		argument_classifier,
		&specialized_function_classifier
	);
	if (specialization_status != 0) {
		return -1;
	}
	int pi_status = classifier_kernel_as_pi(
			terms,
			type_declarations,
			NULL,
			specialized_function_classifier,
			&function_pi,
			&domain,
			&codomain_family
		);
	if (pi_status == 0) {
		uint32_t domain_whnf;
		if (prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				domain,
				&domain_whnf
			) != 0 || domain_whnf >= terms->term_count) {
			return -1;
		}
		if (terms->terms[domain_whnf].tag == PROTOTYPE_TERM_RETURN) {
			domain_whnf = terms->terms[domain_whnf].as.return_term.value;
		}
		domain = domain_whnf;
	}
	int compatible = pi_status == 0 &&
		prototype_judgement_classifier_compatible(
			terms, type_declarations, domain, argument_classifier
		);
	int result_status = pi_status == 0 && compatible ?
		pi_codomain_after_argument_in_context(
			delta->contexts,
			delta->substitutions,
			terms,
			type_declarations,
			delta->current_context_id,
			function_pi,
			argument_subject,
			argument_classifier,
			PROTOTYPE_INVALID_ID,
			&result_classifier
		) : -1;
	int result_equal = result_status == 0 &&
		prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, result_classifier, classifier
		);
	if (pi_status != 0 || !compatible || result_status != 0 || !result_equal) {
		return -1;
	}
	(void)codomain_family;
	uint32_t premise_subjects[2] = {
		function_subject,
		argument_subject
	};
	uint32_t premise_classifiers[2] = {
		function_classifier,
		argument_classifier
	};
	return add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM,
		premise_subjects,
		premise_classifiers,
		2
	);
}

int prototype_judgement_delta_record_context_reindex(
	struct prototype_judgement_delta* delta,
	uint32_t subject,
	uint32_t classifier,
	uint32_t source_context_id
) {
	if (!delta || source_context_id == delta->current_context_id) {
		return -1;
	}
	uint32_t premise_subjects[1] = { subject };
	uint32_t premise_classifiers[1] = { classifier };
	if (add_delta_relation_with_premises(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			classifier,
			PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_REINDEX,
			premise_subjects,
			premise_classifiers,
			1
		) != 0) {
		return -1;
	}
	for (size_t i = delta->relation_count; i > 0; --i) {
		const struct prototype_judgement_relation* relation =
			&delta->relations[i - 1];
		if (relation->context_id == delta->current_context_id &&
			relation->subject == subject &&
			relation->classifier == classifier &&
			relation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_REINDEX &&
			relation->proof_id < delta->proof_count) {
			delta->proofs[relation->proof_id].premise_context_ids[0] =
				source_context_id;
			return 0;
		}
	}
	return -1;
}

int prototype_judgement_delta_record_effect_weaken(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t source_classifier,
	uint32_t target_classifier
) {
	if (!delta || !terms || !type_declarations ||
		subject >= terms->term_count || source_classifier >= terms->term_count ||
		target_classifier >= terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view source;
	struct prototype_term_classifier_view target;
	unsigned source_effects;
	unsigned target_effects;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, source_classifier, &source
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, target_classifier, &target
		) != 0 || source.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		target.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		target.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, source.result, target.result
		) || prototype_term_effect_row_closed_bits(
			terms, source.effect_row, &source_effects
		) != 0 || prototype_term_effect_row_closed_bits(
			terms, target.effect_row, &target_effects
		) != 0 || (source_effects & target_effects) != source_effects) {
		return -1;
	}
	uint32_t premise_subjects[1] = { subject };
	uint32_t premise_classifiers[1] = { source_classifier };
	return add_delta_relation_with_premises(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		target_classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN,
		premise_subjects,
		premise_classifiers,
		1
	);
}

int prototype_judgement_delta_record_type_formation(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !terms || !type_declarations) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms, subject, &type_id, arguments, &argument_count
		) != 0 || type_id >= type_declarations->type_count ||
		argument_count > 16) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (argument_count == 0 &&
		type->formation_classifier != PROTOTYPE_INVALID_ID &&
		prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			type->formation_classifier,
			classifier
		)) {
		return add_delta_relation(
			delta,
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
			subject,
			classifier,
			PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
		);
	}
	if (!type_instance_has_known_type(terms, type_declarations, subject) ||
		!term_is_universe_var(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO
	);
}

int prototype_judgement_delta_record_pure_primitive_type(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	if (!delta || !terms || !type_declarations ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_PURE_PRIMITIVE) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	uint32_t inferred_classifier;
	if (prototype_judgement_pure_primitive_classifier(
			terms,
			type_declarations,
			&terms->terms[subject],
			&inferred_classifier
		) != 0 ||
		!prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			inferred_classifier,
			classifier
		)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO
	);
}

int prototype_judgement_delta_record_effect_operation_type(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t subject,
	uint32_t classifier
) {
	(void)type_declarations;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_EFFECT_OPERATION) ||
		!term_exists(terms, classifier)) {
		return -1;
	}
	uint32_t inferred_classifier;
	if (prototype_judgement_effect_operation_classifier(
			terms, &terms->terms[subject], &inferred_classifier
		) != 0 ||
		!prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, inferred_classifier, classifier
		)) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO
	);
}

int prototype_judgement_delta_record_text_literal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t text;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_TEXT_LITERAL) ||
		!term_exists(terms, classifier) ||
		prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0 ||
		text != classifier) {
		return -1;
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO
	);
}

int prototype_judgement_delta_record_int_literal(
	struct prototype_judgement_delta* delta,
	struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	uint32_t integer;
	uint32_t integer64;
	if (!delta || !terms ||
		!term_has_tag(terms, subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_exists(terms, classifier) ||
		prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT64, &integer64) != 0) {
		return -1;
	}
	if (classifier != integer64) {
		if (terms->terms[subject].as.int_literal.value < INT32_MIN ||
			terms->terms[subject].as.int_literal.value > INT32_MAX ||
			prototype_term_make_host_type(terms, PROTOTYPE_HOST_TYPE_INT32, &integer) != 0 ||
			classifier != integer) {
			return -1;
		}
	}
	return add_delta_relation(
		delta,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		classifier,
		PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO
	);
}

int prototype_judgement_expand_checked(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_exists(terms, subject) || !term_exists(terms, classifier)) {
		return -1;
	}
	return add_relation(judgement, 0, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_DECLARATION);
}

int prototype_judgement_add_conversion(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t context_id,
	uint32_t subject,
	uint32_t expected,
	uint32_t actual
) {
	if (!term_exists(terms, subject) ||
		!term_exists(terms, expected) ||
		!term_exists(terms, actual)) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 16; ++depth) {
		uint32_t next = PROTOTYPE_INVALID_ID;
		for (size_t i = judgement->relation_count; i > 0; --i) {
			const struct prototype_judgement_relation* relation =
				&judgement->relations[i - 1];
			if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
				relation->context_id == context_id &&
				relation->subject == subject &&
				relation->classifier == actual &&
				relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION &&
				relation->proof_id < judgement->proof_count &&
				judgement->proofs[relation->proof_id].premise_count == 1) {
				next = judgement->proofs[relation->proof_id].premise_classifiers[0];
				break;
			}
		}
		if (next == PROTOTYPE_INVALID_ID || next == actual) {
			break;
		}
		actual = next;
	}
	if (expected == actual) {
		return 0;
	}
	uint32_t premise_subjects[1] = { subject };
	uint32_t premise_classifiers[1] = { actual };
	uint32_t premise_context_ids[1] = { context_id };
	return add_relation_with_premises(
		judgement,
		context_id,
		PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		subject,
		expected,
		PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
		premise_context_ids,
		premise_subjects,
		premise_classifiers,
		1
	);
}

int prototype_judgement_delta_expand_checked(
	struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t classifier
) {
	if (!term_exists(terms, subject) || !term_exists(terms, classifier)) {
		return -1;
	}
	return add_delta_relation(delta, PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE, subject, classifier, PROTOTYPE_JUDGEMENT_PROOF_DECLARATION);
}

int prototype_judgement_delta_has_pending_classifier_state(
	const struct prototype_judgement_delta* delta
) {
	if (!delta) {
		return -1;
	}
	return delta->match_motive_result_count > 0 ? 1 : 0;
}

static int judgement_find_relation_proof_id(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	uint32_t* p_proof_id,
	int include_conversion
) {
	if (!judgement) {
		return -1;
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind == kind &&
			relation->context_id == context_id &&
			relation->subject == subject &&
			relation->classifier == classifier &&
			(include_conversion ||
				relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION)) {
			if (p_proof_id) {
				*p_proof_id = relation->proof_id;
			}
			return 0;
		}
	}
	return -1;
}

static void set_proof_premises(
	struct prototype_judgement_proof* proof,
	const struct prototype_judgement_db* judgement,
	const uint32_t* premise_context_ids,
	const uint32_t* premise_subjects,
	const uint32_t* premise_classifiers,
	uint32_t premise_count
) {
	proof->premise_count = premise_count;
	for (uint32_t i = 0; i < premise_count; ++i) {
		proof->premise_kinds[i] = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof->premise_context_ids[i] = premise_context_ids ?
			premise_context_ids[i] : proof->conclusion_context_id;
		proof->premise_subjects[i] = premise_subjects[i];
		proof->premise_classifiers[i] = premise_classifiers[i];
		if (judgement_find_relation_proof_id(
				judgement,
				proof->premise_context_ids[i],
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
				premise_subjects[i],
				premise_classifiers[i],
				&proof->premise_proof_ids[i],
				proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
			) != 0) {
			proof->premise_proof_ids[i] = PROTOTYPE_INVALID_ID;
		}
	}
}

static void initialize_proof_rule_parameters(
	struct prototype_judgement_proof* proof
) {
	if (!proof) {
		return;
	}
	proof->assumption_index = PROTOTYPE_INVALID_ID;
	proof->constructor_owner_view = PROTOTYPE_INVALID_ID;
	proof->constructor_index = PROTOTYPE_INVALID_ID;
	proof->constructor_field_index = PROTOTYPE_INVALID_ID;
	proof->induction_match = PROTOTYPE_INVALID_ID;
	proof->induction_case_index = PROTOTYPE_INVALID_ID;
	proof->induction_field_index = PROTOTYPE_INVALID_ID;
}

static int copy_db_relation_rule_parameters(
	struct prototype_judgement_db* judgement,
	int kind,
	uint32_t subject,
	uint32_t classifier,
	int proof_kind,
	const struct prototype_judgement_proof* source
) {
	if (!judgement || !source) {
		return -1;
	}
	for (size_t i = judgement->relation_count; i > 0; --i) {
		struct prototype_judgement_relation* relation = &judgement->relations[i - 1];
		if (relation->kind == kind &&
			relation->subject == subject &&
			relation->classifier == classifier &&
			relation->proof_kind == proof_kind &&
			relation->proof_id < judgement->proof_count) {
			struct prototype_judgement_proof* target =
				&judgement->proofs[relation->proof_id];
			target->assumption_index = source->assumption_index;
			target->constructor_owner_view = source->constructor_owner_view;
			target->constructor_index = source->constructor_index;
			target->constructor_field_index = source->constructor_field_index;
			target->induction_match = source->induction_match;
			target->induction_case_index = source->induction_case_index;
			target->induction_field_index = source->induction_field_index;
			return 0;
		}
	}
	return -1;
}

void prototype_judgement_resolve_proof_edges(struct prototype_judgement_db* judgement) {
	if (!judgement) {
		return;
	}
	for (size_t i = 0; i < judgement->proof_count; ++i) {
		struct prototype_judgement_proof* proof = &judgement->proofs[i];
		if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			if (judgement_find_relation_proof_id(
					judgement,
					proof->premise_context_ids[j],
					proof->premise_kinds[j],
					proof->premise_subjects[j],
					proof->premise_classifiers[j],
					&proof->premise_proof_ids[j],
					proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
				) != 0) {
				proof->premise_proof_ids[j] = PROTOTYPE_INVALID_ID;
			}
		}
	}
}

int prototype_judgement_add_normalization_premise_conversions(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !judgement) {
		return -1;
	}
	size_t proof_count = judgement->proof_count;
	for (size_t i = 0; i < proof_count; ++i) {
		struct prototype_judgement_proof* proof = &judgement->proofs[i];
		if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
			continue;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			uint32_t proof_id;
			if (judgement_find_relation_proof_id(
					judgement,
					proof->premise_context_ids[j],
					proof->premise_kinds[j],
					proof->premise_subjects[j],
					proof->premise_classifiers[j],
					&proof_id,
					proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
				) == 0) {
				continue;
			}
			if (proof->premise_kinds[j] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
				continue;
			}
			for (size_t k = 0; k < judgement->relation_count; ++k) {
				const struct prototype_judgement_relation* candidate =
					&judgement->relations[k];
				if (candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					candidate->context_id != proof->premise_context_ids[j] ||
					candidate->subject != proof->premise_subjects[j] ||
					candidate->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION ||
					!prototype_judgement_classifier_normalization_equal(
						terms,
						type_declarations,
						proof->premise_classifiers[j],
						candidate->classifier
					)) {
					continue;
				}
				uint32_t premise_subjects[1] = { candidate->subject };
				uint32_t premise_classifiers[1] = { candidate->classifier };
				if (add_relation_with_premises(
						judgement,
						proof->premise_context_ids[j],
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
						proof->premise_subjects[j],
						proof->premise_classifiers[j],
						PROTOTYPE_JUDGEMENT_PROOF_CONVERSION,
						NULL,
						premise_subjects,
						premise_classifiers,
						1
					) != 0) {
					return -1;
				}
				break;
			}
		}
	}
	return 0;
}

void prototype_judgement_refresh_app_elim_premises(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions
) {
	if (!judgement || !terms || !type_declarations || !contexts ||
		!substitutions) {
		return;
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM ||
			relation->proof_id >= judgement->proof_count) {
			continue;
		}
			struct prototype_judgement_proof* proof = &judgement->proofs[relation->proof_id];
		if (proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM ||
			relation->subject >= terms->term_count ||
			terms->terms[relation->subject].tag != PROTOTYPE_TERM_APP ||
			proof->premise_count != 2) {
			continue;
		}
			const struct prototype_term* app = &terms->terms[relation->subject];
		uint32_t selected_function_classifier = PROTOTYPE_INVALID_ID;
		uint32_t selected_argument_classifier = PROTOTYPE_INVALID_ID;
		for (size_t f = 0; f < judgement->relation_count; ++f) {
			const struct prototype_judgement_relation* function_relation =
				&judgement->relations[f];
			if (function_relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				function_relation->subject != app->as.app.function ||
				function_relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION) {
				continue;
			}
			uint32_t function_pi;
			uint32_t domain;
			uint32_t codomain_family;
			if (classifier_kernel_as_pi(
					terms,
					type_declarations,
					NULL,
					function_relation->classifier,
					&function_pi,
					&domain,
					&codomain_family
				) != 0) {
				continue;
			}
			(void)codomain_family;
			for (size_t a = 0; a < judgement->relation_count; ++a) {
				const struct prototype_judgement_relation* argument_relation =
					&judgement->relations[a];
				if (argument_relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					argument_relation->subject != app->as.app.argument ||
					argument_relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_CONVERSION ||
					!prototype_judgement_classifier_compatible(
						terms,
						type_declarations,
						domain,
						argument_relation->classifier
					)) {
					continue;
				}
				uint32_t result_classifier;
				if (pi_codomain_after_argument_in_context(
						contexts,
						substitutions,
						terms,
						type_declarations,
						relation->context_id,
						function_pi,
						app->as.app.argument,
						argument_relation->classifier,
						argument_relation->proof_id,
						&result_classifier
					) != 0 ||
					!prototype_judgement_classifier_normalization_equal(
						terms,
						type_declarations,
						result_classifier,
						relation->classifier
					)) {
					continue;
				}
				selected_function_classifier = function_relation->classifier;
				selected_argument_classifier = argument_relation->classifier;
				break;
			}
			if (selected_function_classifier != PROTOTYPE_INVALID_ID) {
				break;
			}
		}
		if (selected_function_classifier == PROTOTYPE_INVALID_ID) {
			continue;
		}
		proof->premise_kinds[0] = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof->premise_subjects[0] = app->as.app.function;
		proof->premise_classifiers[0] = selected_function_classifier;
		proof->premise_proof_ids[0] = PROTOTYPE_INVALID_ID;
		proof->premise_kinds[1] = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
		proof->premise_subjects[1] = app->as.app.argument;
		proof->premise_classifiers[1] = selected_argument_classifier;
		proof->premise_proof_ids[1] = PROTOTYPE_INVALID_ID;
	}
}

void prototype_judgement_resolve_declaration_premises(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !judgement) {
		return;
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			relation->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_DECLARATION ||
			relation->proof_id >= judgement->proof_count ||
			relation->subject >= terms->term_count ||
			term_has_tag(terms, relation->subject, PROTOTYPE_TERM_EXTERNAL_REF) ||
			term_has_tag(terms, relation->subject, PROTOTYPE_TERM_CONSTRUCTOR)) {
			continue;
		}
			struct prototype_judgement_proof* proof = &judgement->proofs[relation->proof_id];
		if (proof->premise_count > 0) {
			continue;
		}
		int has_support = 0;
		for (size_t j = 0; j < judgement->relation_count; ++j) {
			const struct prototype_judgement_relation* candidate = &judgement->relations[j];
			if (candidate == relation ||
				candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				candidate->subject != relation->subject ||
				candidate->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_DECLARATION ||
				candidate->proof_id >= judgement->proof_count ||
				!prototype_judgement_classifier_compatible(
					terms,
					type_declarations,
					relation->classifier,
					candidate->classifier
				)) {
				continue;
			}
			has_support = 1;
			proof->premise_count = 1;
			proof->premise_kinds[0] = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
			proof->premise_subjects[0] = candidate->subject;
			proof->premise_classifiers[0] = candidate->classifier;
			proof->premise_proof_ids[0] = candidate->proof_id;
			break;
		}
		if (has_support) {
			continue;
		}
		if (terms->terms[relation->subject].tag == PROTOTYPE_TERM_LAMBDA) {
			(void)prototype_judgement_expand_lambda(
				judgement,
				terms,
				type_declarations,
				relation->subject,
				relation->classifier
			);
		} else if (terms->terms[relation->subject].tag == PROTOTYPE_TERM_APP) {
			uint32_t inferred_classifier;
			(void)prototype_judgement_expand_app(
				judgement,
				terms,
				type_declarations,
				relation->subject,
				&inferred_classifier
			);
		} else if (terms->terms[relation->subject].tag == PROTOTYPE_TERM_MATCH) {
			(void)prototype_judgement_expand_match(
				judgement,
				terms,
				type_declarations,
				relation->subject,
				relation->classifier
			);
		}
		for (size_t j = 0; j < judgement->relation_count; ++j) {
			const struct prototype_judgement_relation* candidate = &judgement->relations[j];
			if (candidate == relation ||
				candidate->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
				candidate->subject != relation->subject ||
				candidate->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_DECLARATION ||
				candidate->proof_id >= judgement->proof_count ||
				!prototype_judgement_classifier_compatible(
					terms,
					type_declarations,
					relation->classifier,
					candidate->classifier
				)) {
				continue;
			}
			proof->premise_count = 1;
			proof->premise_kinds[0] = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE;
			proof->premise_subjects[0] = candidate->subject;
			proof->premise_classifiers[0] = candidate->classifier;
			proof->premise_proof_ids[0] = candidate->proof_id;
			break;
		}
	}
}

static int validate_app_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		proof->premise_count != 2 ||
		proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_kinds[1] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	const struct prototype_term* app;
	if (term_core_app(terms, relation->subject, &app) != 0) {
		return -1;
	}
	int function_matches = 0;
	int argument_matches = 0;
	if (prototype_term_core_shape_equal(
			terms, app->as.app.function, proof->premise_subjects[0], &function_matches
		) != 0 ||
		prototype_term_core_shape_equal(
			terms, app->as.app.argument, proof->premise_subjects[1], &argument_matches
		) != 0 ||
		!function_matches || !argument_matches) {
		return -1;
	}
	uint32_t function_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t specialized_function_classifier;
	if (prototype_judgement_specialize_effect_rows_for_argument(
			terms,
			type_declarations,
			proof->premise_classifiers[0],
			proof->premise_classifiers[1],
			&specialized_function_classifier
		) != 0) {
		return -1;
	}
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		specialized_function_classifier,
		&function_pi,
		&domain,
		&codomain_family
	);
	if (status != 0) {
		return -1;
	}
	(void)function_pi;
	uint32_t domain_whnf;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			domain,
			&domain_whnf
		) != 0 || domain_whnf >= terms->term_count) {
		return -1;
	}
	if (terms->terms[domain_whnf].tag == PROTOTYPE_TERM_RETURN) {
		domain_whnf = terms->terms[domain_whnf].as.return_term.value;
	}
	if (!prototype_judgement_classifier_compatible(
			terms,
			type_declarations,
			domain_whnf,
			proof->premise_classifiers[1]
		)) {
		return -1;
	}
	uint32_t result_classifier;
	uint32_t identity;
	if (prototype_substitution_identity(
			substitutions, contexts, relation->context_id, &identity
		) != 0 ||
		prototype_context_instantiate_pure_family(
			contexts,
			substitutions,
			terms,
			type_declarations,
			identity,
			proof->premise_classifiers[1],
			codomain_family,
			proof->premise_subjects[1],
			proof->premise_classifiers[1],
			proof->premise_proof_ids[1],
			&result_classifier
		) != 0 ||
			!prototype_judgement_classifier_normalization_equal(
				terms,
			type_declarations,
			result_classifier,
			relation->classifier
		)) {
		return -1;
	}
	return 0;
}

static int validate_lambda_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_LAMBDA ||
		proof->premise_count != 2 ||
		proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_kinds[1] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	const struct prototype_term* lambda = &terms->terms[relation->subject];
	if (proof->premise_subjects[1] != lambda->as.lambda.body ||
		proof->premise_subjects[0] >= terms->term_count ||
		terms->terms[proof->premise_subjects[0]].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[proof->premise_subjects[0]].as.var.binder_id !=
			lambda->as.lambda.binder_id) {
		return -1;
	}
	uint32_t lambda_pi;
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t lambda_classifier;
	if (classifier_kernel_strip_effect_row_foralls(
			terms, type_declarations, NULL, relation->classifier, &lambda_classifier
		) != 0) {
		return -1;
	}
	int status = classifier_kernel_as_pi(
		terms,
		type_declarations,
		NULL,
		lambda_classifier,
		&lambda_pi,
		&domain,
		&codomain_family
	);
	(void)lambda_pi;
	if (status != 0 ||
		!prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			domain,
			proof->premise_classifiers[0]
		)) {
		return -1;
	}
	uint32_t expected_body_classifier;
	uint32_t projection;
	if (prototype_substitution_projection(
			substitutions,
			contexts,
			proof->premise_context_ids[0],
			&projection
		) != 0 ||
		prototype_context_instantiate_pure_family(
			contexts,
			substitutions,
			terms,
			type_declarations,
			projection,
			proof->premise_classifiers[0],
			codomain_family,
			proof->premise_subjects[0],
			proof->premise_classifiers[0],
			proof->premise_proof_ids[0],
			&expected_body_classifier
		) != 0 ||
		!prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			expected_body_classifier,
			proof->premise_classifiers[1]
		)) {
		return -1;
	}
	(void)codomain_family;
	return 0;
}

static int validate_match_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_APP ||
		proof->premise_count !=
			terms->terms[relation->subject].as.match.case_count + 1 ||
		proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_subjects[0] != relation->classifier ||
		!term_is_universe_var(terms, proof->premise_classifiers[0])) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[relation->subject];
	const struct prototype_term* motive_app = &terms->terms[relation->classifier];
	const struct prototype_term* motive_lambda;
	const struct prototype_term* motive_body;
	uint32_t normalized_classifier;
	if (motive_app->as.app.argument != match->as.match.scrutinee ||
		terms->terms[motive_app->as.app.function].tag != PROTOTYPE_TERM_LAMBDA ||
		classifier_kernel_whnf_no_definitions(
			terms,
			type_declarations,
			relation->classifier,
			&normalized_classifier
		) != 0) {
		return -1;
	}
	motive_lambda = &terms->terms[motive_app->as.app.function];
	if (motive_lambda->as.lambda.body >= terms->term_count ||
		terms->terms[motive_lambda->as.lambda.body].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	motive_body = &terms->terms[motive_lambda->as.lambda.body];
	if (motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binder_id !=
			motive_lambda->as.lambda.binder_id) {
		return -1;
	}
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_body->as.match.first_case + i;
		if (case_id >= terms->case_count ||
			motive_case_id >= terms->case_count ||
			proof->premise_kinds[i + 1] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premise_subjects[i + 1] != terms->cases[case_id].body ||
			!term_exists(terms, proof->premise_classifiers[i + 1])) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = &terms->cases[motive_case_id];
		if (match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count) {
			return -1;
		}
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID ||
			motive_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			if (match_case->constructor_owner != motive_case->constructor_owner) {
				return -1;
			}
		} else {
			int same_owner = 0;
			if (prototype_term_view_shape_equal(
					terms,
					match_case->constructor_owner,
					motive_case->constructor_owner,
					&same_owner
				) != 0 ||
				!same_owner) {
				return -1;
			}
		}
		uint32_t expected_motive_case_body;
		if (core_match_case_alpha_reindex(
				terms,
				type_declarations,
				&terms->case_binders[match_case->first_binder],
				&terms->case_binders[motive_case->first_binder],
				match_case->binder_count,
				proof->premise_classifiers[i + 1],
				&expected_motive_case_body
			) != 0 ||
			!prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				expected_motive_case_body,
				motive_case->body
			)) {
			return -1;
		}
	}
	(void)normalized_classifier;
	return 0;
}

static int validate_induction_hypothesis_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		relation->subject >= terms->term_count ||
		terms->terms[relation->subject].tag != PROTOTYPE_TERM_INDUCTION_HYPOTHESIS ||
		proof->premise_count != 0 ||
		proof->induction_match >= terms->term_count ||
		terms->terms[proof->induction_match].tag != PROTOTYPE_TERM_MATCH ||
		proof->induction_case_index >=
			terms->terms[proof->induction_match].as.match.case_count) {
		return -1;
	}
	const struct prototype_term* ih = &terms->terms[relation->subject];
	if (ih->as.induction_hypothesis.frame_id >= terms->match_frame_count ||
		terms->match_frames[ih->as.induction_hypothesis.frame_id].match_term !=
			proof->induction_match) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[proof->induction_match];
	uint32_t case_id = match->as.match.first_case + proof->induction_case_index;
	if (case_id >= terms->case_count ||
		proof->induction_field_index >= terms->cases[case_id].binder_count) {
		return -1;
	}
	const struct prototype_case_binder* binder =
		&terms->case_binders[
			terms->cases[case_id].first_binder + proof->induction_field_index
		];
	if (ih->as.induction_hypothesis.argument >= terms->term_count ||
		terms->terms[ih->as.induction_hypothesis.argument].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[ih->as.induction_hypothesis.argument].as.var.binder_id !=
			binder->binder_id) {
		return -1;
	}
	if (!binder->is_recursive) {
		return -1;
	}
	uint32_t match_classifier;
	uint32_t expected_classifier;
	uint32_t match_classifiers[32];
	uint32_t match_classifier_count = 0;
	if (collect_judgement_subject_classifiers(
			judgement,
			terms,
			type_declarations,
			proof->induction_match,
			match_classifiers,
			32,
			&match_classifier_count
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < match_classifier_count; ++i) {
		match_classifier = match_classifiers[i];
		if (!term_has_tag(terms, match_classifier, PROTOTYPE_TERM_APP) ||
			terms->terms[match_classifier].as.app.argument != match->as.match.scrutinee) {
			continue;
		}
		if (prototype_term_app(
				terms,
				terms->terms[match_classifier].as.app.function,
				ih->as.induction_hypothesis.argument,
				&expected_classifier
			) != 0) {
			return -1;
		}
		if (prototype_judgement_classifier_normalization_equal(
				terms,
				type_declarations,
				expected_classifier,
				relation->classifier
			)) {
			return 0;
		}
	}
	return -1;
}

static int validate_solved_match_motive_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!match_motive_result_classifier(terms, relation->subject, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* match = &terms->terms[relation->subject];
	const struct prototype_term* motive_app = &terms->terms[relation->classifier];
	const struct prototype_term* motive =
		&terms->terms[motive_app->as.app.function];
	if (motive->as.lambda.body >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* motive_body = &terms->terms[motive->as.lambda.body];
	if (motive_body->tag != PROTOTYPE_TERM_MATCH) {
		/* Constant motives are valid only when every branch proves the same
		 * result classifier. The constant-motive builder separately excludes
		 * pattern-binder capture. */
		for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
			uint32_t case_id = match->as.match.first_case + i;
			uint32_t classifiers[32];
			uint32_t classifier_count = 0;
			if (case_id >= terms->case_count ||
				collect_judgement_subject_classifiers(
					judgement, terms, type_declarations, terms->cases[case_id].body,
					classifiers, 32, &classifier_count
				) != 0 ||
				!classifier_list_contains_normalization_equal(
					terms, type_declarations, classifiers, classifier_count,
					motive->as.lambda.body
				)) {
				return -1;
			}
		}
		return 0;
	}
	if (motive_body->as.match.case_count != match->as.match.case_count ||
		motive_body->as.match.scrutinee >= terms->term_count ||
		terms->terms[motive_body->as.match.scrutinee].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[motive_body->as.match.scrutinee].as.var.binder_id !=
			motive->as.lambda.binder_id) {
		return -1;
	}
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		uint32_t case_id = match->as.match.first_case + i;
		uint32_t motive_case_id = motive_body->as.match.first_case + i;
		if (case_id >= terms->case_count || motive_case_id >= terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* match_case = &terms->cases[case_id];
		const struct prototype_match_case* motive_case = &terms->cases[motive_case_id];
		if (match_case->constructor_id != motive_case->constructor_id ||
			match_case->binder_count != motive_case->binder_count ||
			match_case->constructor_owner != motive_case->constructor_owner) {
			return -1;
		}
		uint32_t classifiers[32];
		uint32_t classifier_count = 0;
		if (collect_judgement_subject_classifiers(
				judgement, terms, type_declarations, match_case->body,
				classifiers, 32, &classifier_count
			) != 0) {
			return -1;
		}
		int found = 0;
		for (uint32_t j = 0; j < classifier_count; ++j) {
			uint32_t expected_motive_case_body = PROTOTYPE_INVALID_ID;
			int prepare_status = core_match_case_alpha_reindex(
					terms, type_declarations,
					&terms->case_binders[match_case->first_binder],
					&terms->case_binders[motive_case->first_binder],
					match_case->binder_count, classifiers[j],
					&expected_motive_case_body
				);
			if (prepare_status == 0 &&
				prototype_judgement_classifier_normalization_equal(
					terms, type_declarations, expected_motive_case_body,
					motive_case->body
				)) {
				found = 1;
				break;
			}
			/* A structurally recursive motive records M(rest) as an IH node in
			 * its own match frame. This is a guarded equation, not a new WHNF
			 * conversion rule. Verify the frame and the substituted recursive
			 * binder explicitly. */
			if (prepare_status == 0 && motive_case->body < terms->term_count &&
				terms->terms[motive_case->body].tag ==
					PROTOTYPE_TERM_INDUCTION_HYPOTHESIS &&
				expected_motive_case_body < terms->term_count &&
				terms->terms[expected_motive_case_body].tag == PROTOTYPE_TERM_APP) {
				const struct prototype_term* motive_ih =
					&terms->terms[motive_case->body];
				const struct prototype_term* expected_app =
					&terms->terms[expected_motive_case_body];
				if (motive_body->as.match.frame_id == motive_ih->as.induction_hypothesis.frame_id &&
					expected_app->as.app.function == motive_app->as.app.function &&
					expected_app->as.app.argument == motive_ih->as.induction_hypothesis.argument) {
					found = 1;
					break;
				}
			}
		}
		if (!found) {
			return -1;
		}
	}
	return 0;
}

static int validate_type_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms, relation->subject, &type_id, arguments, &argument_count
		) != 0 || type_id >= type_declarations->type_count ||
		argument_count > 16) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (argument_count == 0 &&
		type->formation_classifier != PROTOTYPE_INVALID_ID) {
		return type->formation_classifier == relation->classifier ? 0 : -1;
	}
	return type_instance_has_known_type(terms, type_declarations, relation->subject) &&
		term_is_universe_var(terms, relation->classifier) ? 0 : -1;
}

static int validate_constructor_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_CONSTRUCTOR) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* constructor = &terms->terms[relation->subject];
	if (!constructor_belongs_to_owner(
			terms,
			type_declarations,
			constructor->as.constructor.owner,
			constructor->as.constructor.constructor_id
		) ||
			!classifier_returns_owner(
				terms,
				type_declarations,
				relation->classifier,
				constructor->as.constructor.owner
			)) {
		return -1;
	}
	return 0;
}

static int validate_constructor_spine_formation_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count == 0 ||
		proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return -1;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count;
	if (prototype_term_constructor_spine_info(
			terms,
			relation->subject,
			&head,
			&owner,
			&constructor_index,
			arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&argument_count
		) != 0 || argument_count != proof->premise_count) {
		return -1;
	}
	uint32_t argument_classifiers[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (proof->premise_kinds[i] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premise_context_ids[i] != relation->context_id ||
			proof->premise_subjects[i] != arguments[i]) {
			return -1;
		}
		argument_classifiers[i] = proof->premise_classifiers[i];
	}
	uint32_t expected_classifier;
	int saturated;
	if (prototype_judgement_constructor_spine_classifier(
			terms,
			type_declarations,
			contexts,
				substitutions,
				relation->context_id,
				relation->subject,
				proof->constructor_owner_view,
				argument_classifiers,
			argument_count,
			&expected_classifier,
			&saturated
		) != 0 || !prototype_judgement_classifier_normalization_equal(
			terms,
			type_declarations,
			expected_classifier,
			relation->classifier
		)) {
		return -1;
	}
	(void)head;
	(void)owner;
	(void)constructor_index;
	(void)saturated;
	return 0;
}

static int validate_match_type_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_exists(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier) ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_MATCH) ||
		proof->premise_count != terms->terms[relation->subject].as.match.case_count) {
		return -1;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		uint32_t case_id = terms->terms[relation->subject].as.match.first_case + i;
		if (case_id >= terms->case_count ||
			terms->cases[case_id].body >= terms->term_count ||
			terms->cases[case_id].first_binder > terms->case_binder_count ||
			terms->cases[case_id].binder_count >
				terms->case_binder_count - terms->cases[case_id].first_binder ||
			!match_case_has_valid_constructor(
				terms,
				type_declarations,
				&terms->cases[case_id]
			) ||
			proof->premise_kinds[i] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			proof->premise_subjects[i] != terms->cases[case_id].body ||
			proof->premise_classifiers[i] != relation->classifier) {
			return -1;
		}
	}
	return 0;
}

static int validate_universe_cumulativity_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_universe_var(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_conversion_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_subjects[0] != relation->subject ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier) ||
		!term_exists(terms, proof->premise_classifiers[0])) {
		return -1;
	}
	if (!prototype_judgement_classifier_compatible(
			terms,
			type_declarations,
			relation->classifier,
			proof->premise_classifiers[0]
		)) {
		return -1;
	}
	return 0;
}

static int validate_effect_weaken_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_subjects[0] != relation->subject ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier) ||
		!term_exists(terms, proof->premise_classifiers[0])) {
		return -1;
	}
	struct prototype_term_classifier_view source;
	struct prototype_term_classifier_view target;
	unsigned source_effects;
	unsigned target_effects;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL,
			proof->premise_classifiers[0], &source
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, relation->classifier, &target
		) != 0 || source.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		target.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		source.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		target.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, source.result, target.result
		) || prototype_term_effect_row_closed_bits(
			terms, source.effect_row, &source_effects
		) != 0 || prototype_term_effect_row_closed_bits(
			terms, target.effect_row, &target_effects
		) != 0 || (source_effects & target_effects) != source_effects) {
		return -1;
	}
	return 0;
}

static int validate_declaration_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_exists(terms, relation->subject) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	if (term_has_tag(terms, relation->subject, PROTOTYPE_TERM_EXTERNAL_REF)) {
		return proof->premise_count == 0 &&
			term_is_structural_type(terms, type_declarations, relation->classifier) ? 0 : -1;
	}
	if (term_has_tag(terms, relation->subject, PROTOTYPE_TERM_CONSTRUCTOR)) {
		if (proof->premise_count != 0) {
			return -1;
		}
		const struct prototype_term* constructor = &terms->terms[relation->subject];
		uint32_t type_id;
		uint32_t args[16];
		uint32_t arg_count;
		int has_local_owner =
			prototype_term_type_instance_info(
				terms,
				constructor->as.constructor.owner,
				&type_id,
				args,
				&arg_count
			) == 0 &&
			type_id < type_declarations->type_count;
		(void)args;
		(void)arg_count;
		if (has_local_owner &&
			!constructor_belongs_to_owner(
				terms,
				type_declarations,
				constructor->as.constructor.owner,
				constructor->as.constructor.constructor_id
			)) {
			return -1;
		}
			return classifier_returns_owner(
				terms,
				type_declarations,
				relation->classifier,
				constructor->as.constructor.owner
			) ? 0 : -1;
	}
	if (proof->premise_count != 1 ||
		proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_subjects[0] != relation->subject ||
		!term_exists(terms, proof->premise_classifiers[0]) ||
		proof->premise_proof_ids[0] >= judgement->proof_count) {
		return -1;
	}
	if (!prototype_judgement_classifier_compatible(
			terms,
			type_declarations,
			relation->classifier,
			proof->premise_classifiers[0]
		)) {
		return -1;
	}
	(void)judgement;
	return 0;
}

static int validate_assumption_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_VAR) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	if (relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
		uint32_t classifier_classifier;
		const struct prototype_context* conclusion_context =
			prototype_context_get(contexts, proof->conclusion_context_id);
		if (!conclusion_context ||
			proof->assumption_index >= conclusion_context->depth) {
			return -1;
		}
		uint32_t assumption_context_id = proof->conclusion_context_id;
		while (assumption_context_id != 0) {
			const struct prototype_context* entry =
				prototype_context_get(contexts, assumption_context_id);
			if (!entry) {
				return -1;
			}
			if (entry->depth == proof->assumption_index + 1) {
				if (entry->classifier != PROTOTYPE_INVALID_ID &&
					!prototype_judgement_classifier_normalization_equal(
						terms,
						type_declarations,
						entry->classifier,
						relation->classifier
					)) {
					return -1;
				}
				break;
			}
			assumption_context_id = entry->parent;
		}
		if (assumption_context_id == 0) {
			return -1;
		}
			if (term_is_structural_type(terms, type_declarations, relation->classifier)) {
				return 0;
			}
			uint32_t classifier_classifiers[32];
			uint32_t classifier_classifier_count = 0;
			if (collect_judgement_subject_classifiers(
					judgement,
					terms,
					type_declarations,
					relation->classifier,
					classifier_classifiers,
					32,
					&classifier_classifier_count
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < classifier_classifier_count; ++i) {
				classifier_classifier = classifier_classifiers[i];
				if (term_is_universe_var(terms, classifier_classifier)) {
					return 0;
				}
			}
			return -1;
		}
	if (relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
		uint32_t binder_id = terms->terms[relation->subject].as.var.binder_id;
		struct prototype_case_binder previous_binders[64];
		struct prototype_judgement_relation delta_relations[16];
		struct prototype_judgement_proof delta_proofs[16];
		struct prototype_judgement_match_motive_result match_motive_results[16];
		struct prototype_judgement_computation_constraint computation_constraints[16];
		struct prototype_judgement_effect_row_constraint effect_row_constraints[16];
		struct prototype_judgement_db judgement_view = *judgement;
		uint32_t type_id;
		uint32_t arguments[64];
		uint32_t argument_count;
		if (proof->constructor_owner_view == PROTOTYPE_INVALID_ID ||
			proof->constructor_owner_view >= terms->term_count ||
			proof->constructor_index == PROTOTYPE_INVALID_ID ||
			proof->constructor_field_index >= 64 ||
			prototype_type_declaration_instance_info(
				type_declarations,
				terms,
				proof->constructor_owner_view,
				&type_id,
				arguments,
				64,
				&argument_count
			) != 0 ||
			type_id >= type_declarations->type_count ||
			relation->context_id >= contexts->context_count) {
			return -1;
		}
		const struct prototype_type_declaration* type =
			&type_declarations->type_declarations[type_id];
		if (proof->constructor_index >= type->constructor_count ||
			type->first_constructor + proof->constructor_index >=
				type_declarations->constructor_count) {
			return -1;
		}
		const struct prototype_type_constructor_declaration* constructor =
			&type_declarations->constructor_declarations[
				type->first_constructor + proof->constructor_index
			];
		const struct prototype_context* parameter_context =
			prototype_context_get(contexts, constructor->parameter_context);
		const struct prototype_context* field_context =
			prototype_context_get(contexts, constructor->field_context);
		if (!parameter_context || !field_context ||
			field_context->depth < parameter_context->depth ||
			field_context->depth - parameter_context->depth > 64 ||
			proof->constructor_field_index >=
				field_context->depth - parameter_context->depth) {
			return -1;
		}
		uint32_t field_count = field_context->depth - parameter_context->depth;
		uint32_t context_id = relation->context_id;
		for (uint32_t i = field_count; i > 0; --i) {
			const struct prototype_context* entry =
				prototype_context_get(contexts, context_id);
			if (!entry || context_id == 0) {
				return -1;
			}
			memset(&previous_binders[i - 1], 0, sizeof(previous_binders[i - 1]));
			previous_binders[i - 1].binder_id = entry->binder_id;
			context_id = entry->parent;
		}
		if (previous_binders[proof->constructor_field_index].binder_id != binder_id) {
			return -1;
		}
		struct prototype_judgement_delta delta;
		prototype_judgement_delta_init(
			&delta,
			&judgement_view,
				delta_relations,
					delta_proofs,
					16,
					match_motive_results,
					16,
					computation_constraints,
					16,
					effect_row_constraints,
					16
					);
			prototype_judgement_delta_set_context_store(
				&delta, contexts, substitutions
			);
			prototype_judgement_delta_set_context(
				&delta, relation->context_id
			);
			uint32_t expected_classifier;
			if (constructor_field_classifier_from_spine(
					&delta,
					terms,
					type_declarations,
					proof->constructor_owner_view,
					proof->constructor_index,
					previous_binders,
					proof->constructor_field_index,
					proof->constructor_field_index,
					&expected_classifier
				) != 0 ||
				!prototype_judgement_classifier_normalization_equal(
					terms,
					type_declarations,
					expected_classifier,
					relation->classifier
				)) {
				return -1;
			}
		return 0;
	}
	return 0;
}

static int validate_text_literal_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_TEXT_LITERAL) ||
		!term_is_primitive_text(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_int_literal_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_INT_LITERAL) ||
		!term_is_primitive_integer(terms, relation->classifier)) {
		return -1;
	}
	if (term_is_primitive_int(terms, relation->classifier) &&
		!int_literal_fits_int32(terms->terms[relation->subject].as.int_literal.value)) {
		return -1;
	}
	return 0;
}

static int validate_return_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_RETURN) ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	const struct prototype_term* classifier = &terms->terms[relation->classifier];
	unsigned effects;
	return proof->premise_subjects[0] == terms->terms[relation->subject].as.return_term.value &&
		proof->premise_classifiers[0] == classifier->as.computation_type.result &&
		prototype_term_effect_row_closed_bits(
			terms, classifier->as.computation_type.label, &effects
		) == 0 && effects == PROTOTYPE_EFFECT_OPERATION_LABEL_NONE ? 0 : -1;
}

static int validate_thunk_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_THUNK) ||
		relation->classifier >= terms->term_count ||
		terms->terms[relation->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	return proof->premise_subjects[0] == terms->terms[relation->subject].as.thunk.computation &&
		proof->premise_classifiers[0] ==
			terms->terms[relation->classifier].as.thunk_type.computation ? 0 : -1;
}

static int validate_force_elim_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_FORCE) ||
		proof->premise_classifiers[0] >= terms->term_count ||
		terms->terms[proof->premise_classifiers[0]].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	return proof->premise_subjects[0] == terms->terms[relation->subject].as.force.value &&
		relation->classifier ==
			terms->terms[proof->premise_classifiers[0]].as.thunk_type.computation ? 0 : -1;
}

static int computation_fold_carrier_compatible(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t left,
	uint32_t right
) {
	struct prototype_term_classifier_view left_view;
	struct prototype_term_classifier_view right_view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, left, &left_view
		) != 0 || prototype_judgement_classifier_view(
			terms, type_declarations, NULL, right, &right_view
		) != 0 || left_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		right_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		left_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		right_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, left_view.result, right_view.result
		)) {
		return 0;
	}
	unsigned left_effects;
	unsigned right_effects;
	if (prototype_term_effect_row_closed_bits(
			terms, left_view.effect_row, &left_effects
		) != 0 || prototype_term_effect_row_closed_bits(
			terms, right_view.effect_row, &right_effects
		) != 0) {
		/* Partial-policy artifacts retain this equality as a residual obligation.
		 * Strict policy rejects the artifact until both rows are materialized. */
		return 1;
	}
	return left_effects == right_effects;
}

static int validate_computation_fold_elim_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_COMPUTATION_FOLD) ||
		proof->premise_classifiers[0] >= terms->term_count ||
		proof->premise_classifiers[1] >= terms->term_count ||
		relation->classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_term* fold = &terms->terms[relation->subject];
	if (proof->premise_count != 2 + 2 * fold->as.computation_fold.clause_count ||
		proof->premise_subjects[0] != fold->as.computation_fold.computation ||
		proof->premise_subjects[1] != fold->as.computation_fold.return_clause ||
		fold->as.computation_fold.first_clause + fold->as.computation_fold.clause_count >
			terms->computation_fold_clause_count) {
		return -1;
	}
	for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&terms->computation_fold_clauses[fold->as.computation_fold.first_clause + i];
		if (proof->premise_subjects[2 + 2 * i] != clause->operation ||
			proof->premise_subjects[3 + 2 * i] != clause->body) {
			return -1;
		}
	}
	if (fold->as.computation_fold.clause_count != 0) {
		struct prototype_term_classifier_view input;
		struct prototype_term_classifier_view result;
		if (prototype_judgement_classifier_view(
				terms,
				type_declarations,
				NULL,
				proof->premise_classifiers[0],
				&input
			) != 0 || prototype_judgement_classifier_view(
				terms,
				type_declarations,
				NULL,
				relation->classifier,
				&result
			) != 0 || input.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			result.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return -1;
		}
		unsigned input_effects;
		unsigned result_effects;
		if (prototype_term_effect_row_closed_bits(
				terms, input.effect_row, &input_effects
			) != 0 || prototype_term_effect_row_closed_bits(
				terms, result.effect_row, &result_effects
			) != 0) {
			return -1;
		}
		uint32_t return_domain;
		uint32_t return_family;
		uint32_t return_binder;
		uint32_t return_body;
		struct prototype_term_classifier_view return_result;
		if (pi_parts(
				terms,
				proof->premise_classifiers[1],
				&return_domain,
				&return_family
			) != 0 || prototype_term_pure_family_parts(
				terms, return_family, &return_binder, &return_body
			) != 0 || prototype_judgement_classifier_view(
				terms, type_declarations, NULL, return_body, &return_result
			) != 0 || return_result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			return_result.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
			!prototype_judgement_classifier_normalization_equal(
				terms, type_declarations, return_domain, input.result
			) || !prototype_judgement_classifier_normalization_equal(
				terms, type_declarations, return_result.result, result.result
			)) {
			return -1;
		}
		(void)return_binder;
		unsigned return_effects;
		if (prototype_term_effect_row_closed_bits(
				terms, return_result.effect_row, &return_effects
			) != 0 || (return_effects & result_effects) != return_effects) {
			return -1;
		}
		unsigned handled_effects = 0;
		for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
			const struct prototype_computation_fold_clause* clause =
				&terms->computation_fold_clauses[
					fold->as.computation_fold.first_clause + i
				];
			int operation_identity;
			if (prototype_term_effect_operation_identity(
					terms, clause->operation, &operation_identity
				) != 0) {
				return -1;
			}
			const struct prototype_effect_operation_declaration* declaration =
				prototype_term_effect_operation_declaration(operation_identity);
			if (!declaration) {
				return -1;
			}
			handled_effects |= declaration->operation_labels;

			uint32_t operation_classifier = proof->premise_classifiers[2 + 2 * i];
			for (uint32_t depth = 0;
				depth < 32 && operation_classifier < terms->term_count &&
				terms->terms[operation_classifier].tag ==
					PROTOTYPE_TERM_EFFECT_ROW_FORALL;
				++depth) {
				operation_classifier =
					terms->terms[operation_classifier].as.effect_row_forall.body;
			}
			uint32_t operation_domain;
			uint32_t operation_family;
			uint32_t operation_binder;
			uint32_t operation_body;
			struct prototype_term_classifier_view operation_result;
			if (pi_parts(
					terms,
					operation_classifier,
					&operation_domain,
					&operation_family
				) != 0 || prototype_term_pure_family_parts(
					terms, operation_family, &operation_binder, &operation_body
				) != 0 || prototype_judgement_classifier_view(
					terms, type_declarations, NULL, operation_body, &operation_result
				) != 0 || operation_result.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				operation_result.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				return -1;
			}
			(void)operation_binder;

			uint32_t outer_domain;
			uint32_t outer_family;
			uint32_t outer_binder;
			uint32_t inner_classifier;
			uint32_t inner_domain;
			uint32_t inner_family;
			uint32_t inner_binder;
			uint32_t inner_body;
			if (pi_parts(
					terms,
					proof->premise_classifiers[3 + 2 * i],
					&outer_domain,
					&outer_family
				) != 0 || prototype_term_pure_family_parts(
					terms, outer_family, &outer_binder, &inner_classifier
				) != 0 || pi_parts(
					terms,
					inner_classifier,
					&inner_domain,
					&inner_family
				) != 0 || prototype_term_pure_family_parts(
					terms, inner_family, &inner_binder, &inner_body
				) != 0 || !prototype_judgement_classifier_normalization_equal(
					terms, type_declarations, outer_domain, operation_domain
				) || inner_domain >= terms->term_count ||
				terms->terms[inner_domain].tag != PROTOTYPE_TERM_THUNK_TYPE ||
				!computation_fold_carrier_compatible(
					terms, type_declarations, inner_body, relation->classifier
				)) {
				return -1;
			}
			uint32_t continuation_function =
				terms->terms[inner_domain].as.thunk_type.computation;
			uint32_t continuation_domain;
			uint32_t continuation_family;
			uint32_t continuation_binder;
			uint32_t continuation_body;
			if (pi_parts(
					terms,
					continuation_function,
					&continuation_domain,
					&continuation_family
				) != 0 || prototype_term_pure_family_parts(
					terms,
					continuation_family,
					&continuation_binder,
					&continuation_body
				) != 0 || !prototype_judgement_classifier_normalization_equal(
					terms,
					type_declarations,
					continuation_domain,
					operation_result.result
				) || !computation_fold_carrier_compatible(
					terms,
					type_declarations,
					continuation_body,
					relation->classifier
				)) {
				return -1;
			}
			(void)outer_binder;
			(void)inner_binder;
			(void)continuation_binder;
		}
		if ((input_effects & handled_effects) != handled_effects) {
			return -1;
		}
		unsigned required_effects =
			return_effects | (input_effects & ~handled_effects);
		return (required_effects & result_effects) == required_effects ? 0 : -1;
	}
	uint32_t expected_result;
	if (prototype_judgement_computation_fold_result_classifier(
			terms,
			type_declarations,
			fold->as.computation_fold.computation,
			proof->premise_classifiers[0],
			proof->premise_classifiers[1],
			&expected_result
		) != 0 || !prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, relation->classifier, expected_result
		)) {
		return -1;
	}
	return 0;
}

static int validate_operation_request_intro_proof(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || proof->premise_count != 2 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_OPERATION_REQUEST) ||
		proof->premise_subjects[1] != terms->terms[relation->subject].as.operation_request.continuation ||
		proof->premise_classifiers[0] >= terms->term_count ||
		proof->premise_classifiers[1] >= terms->term_count || relation->classifier >= terms->term_count) {
		return -1;
	}
	uint32_t operation_head =
		terms->terms[relation->subject].as.operation_request.operation;
	while (operation_head < terms->term_count &&
		terms->terms[operation_head].tag == PROTOTYPE_TERM_APP) {
		operation_head = terms->terms[operation_head].as.app.function;
	}
	if (operation_head >= terms->term_count ||
		terms->terms[operation_head].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	struct prototype_term_classifier_view operation;
	struct prototype_term_classifier_view result;
	if (prototype_term_classifier_view(terms, proof->premise_classifiers[0], &operation) != 0 ||
		prototype_term_classifier_view(terms, relation->classifier, &result) != 0 ||
		operation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		result.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		operation.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		result.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return -1;
	}
	uint32_t continuation_thunk =
		terms->terms[relation->subject].as.operation_request.continuation;
	if (continuation_thunk >= terms->term_count ||
		terms->terms[continuation_thunk].tag != PROTOTYPE_TERM_THUNK ||
		terms->terms[continuation_thunk].as.thunk.computation >= terms->term_count) {
		return -1;
	}
	uint32_t continuation_lambda = terms->terms[continuation_thunk].as.thunk.computation;
	if (terms->terms[continuation_lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[proof->premise_classifiers[1]].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t continuation_function_classifier =
		terms->terms[proof->premise_classifiers[1]].as.thunk_type.computation;
	uint32_t domain;
	uint32_t family;
	if (pi_parts(terms, continuation_function_classifier, &domain, &family) != 0 ||
		!prototype_judgement_classifier_normalization_equal(
			terms, type_declarations, domain, operation.result
		)) {
		return -1;
	}
	uint32_t binder_var;
	uint32_t continuation_result;
	uint32_t continuation_context;
	uint32_t projection;
	if (prototype_term_var(
			terms, terms->terms[continuation_lambda].as.lambda.binder_id, &binder_var
		) != 0 ||
		prototype_context_extend(
			contexts,
			relation->context_id,
			terms->terms[continuation_lambda].as.lambda.binder_id,
			operation.result,
			PROTOTYPE_INVALID_ID,
			&continuation_context
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, continuation_context, &projection
		) != 0 ||
		prototype_context_instantiate_pure_family(
			contexts,
			substitutions,
			terms,
			type_declarations,
			projection,
			operation.result,
			family,
			binder_var,
			operation.result,
			PROTOTYPE_INVALID_ID,
			&continuation_result
		) != 0) {
		return -1;
	}
	struct prototype_term_classifier_view continuation;
	if (prototype_term_classifier_view(terms, continuation_result, &continuation) != 0 ||
		continuation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		continuation.computation_kind !=
			PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!computation_effect_row_is_union(
			terms, type_declarations, &result, &operation, &continuation
		) ||
		result.result != continuation.result || prototype_term_contains_free_binder(
			terms, continuation.result, terms->terms[continuation_lambda].as.lambda.binder_id
		)) {
		return -1;
	}
	(void)family;
	return 0;
}


static int validate_text_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_primitive_text(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_int_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_primitive_integer(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_host_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_is_host_primitive(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int term_is_host_type_id(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	int host_type
) {
	if (!term_exists(terms, term_id)) {
		return 0;
	}
	int actual_host_type;
	return prototype_term_host_type_from_term_tag(
			terms->terms[term_id].tag,
			&actual_host_type
		) == 0 &&
		actual_host_type == host_type;
}

static int validate_host_signature_classifier(
	const struct prototype_term_db* terms,
	uint32_t classifier,
	uint32_t arity,
	const int* argument_types,
	int result_type,
	unsigned expected_effects
) {
	if (!terms || !argument_types || result_type == PROTOTYPE_HOST_TYPE_INVALID) {
		return -1;
	}
	uint32_t current = classifier;
	for (uint32_t i = 0; i < arity; ++i) {
		uint32_t domain;
		uint32_t codomain_family;
		uint32_t binder_id;
		uint32_t codomain;
		if (argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID ||
			pi_parts(terms, current, &domain, &codomain_family) != 0 ||
			prototype_term_pure_family_parts(
				terms, codomain_family, &binder_id, &codomain
			) != 0 ||
			!term_is_host_type_id(terms, domain, argument_types[i])) {
			return -1;
		}
		(void)binder_id;
		current = codomain;
	}
	if (!term_exists(terms, current) ||
		terms->terms[current].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	uint32_t label = terms->terms[current].as.computation_type.label;
	uint32_t result = terms->terms[current].as.computation_type.result;
	unsigned effects;
	if (!term_exists(terms, label) ||
		prototype_term_effect_row_closed_bits(terms, label, &effects) != 0 ||
		effects != expected_effects) {
		return -1;
	}
	return term_is_host_type_id(terms, result, result_type) ? 0 : -1;
}

static int validate_pure_primitive_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !type_declarations || !contexts || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_PURE_PRIMITIVE) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}

	const struct prototype_term* primitive = &terms->terms[relation->subject];
	uint32_t domain;
	uint32_t codomain_family;
	uint32_t codomain_binder;
	uint32_t codomain;
	if (pi_parts(terms, relation->classifier, &domain, &codomain_family) != 0 ||
		prototype_term_pure_family_parts(
			terms, codomain_family, &codomain_binder, &codomain
		) != 0) {
		return -1;
	}
	const struct prototype_pure_primitive_declaration* signature =
		prototype_term_pure_primitive_declaration(primitive->as.pure_primitive.primitive_id);
	if (signature &&
		signature->result_type != PROTOTYPE_HOST_TYPE_INVALID) {
		int host_only = 1;
		for (uint32_t i = 0; i < signature->arity; ++i) {
			if (signature->argument_types[i] == PROTOTYPE_HOST_TYPE_INVALID) {
				host_only = 0;
			}
		}
		if (host_only) {
			return validate_host_signature_classifier(
				terms,
				relation->classifier,
				signature->arity,
				signature->argument_types,
				signature->result_type,
				PROTOTYPE_EFFECT_OPERATION_LABEL_NONE
			);
		}
	}
	(void)codomain_binder;
	if (codomain >= terms->term_count ||
		terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}

	uint32_t effect_label = terms->terms[codomain].as.computation_type.label;
	uint32_t result = terms->terms[codomain].as.computation_type.result;
	unsigned effects;
	if (effect_label >= terms->term_count || result >= terms->term_count ||
		prototype_term_effect_row_closed_bits(terms, effect_label, &effects) != 0 ||
		effects != PROTOTYPE_EFFECT_OPERATION_LABEL_NONE) {
		return -1;
	}
	if (primitive->as.pure_primitive.primitive_id == PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT) {
		return term_is_primitive_text(terms, domain) &&
			type_formation_is_nat_shape(
				terms, type_declarations, contexts, result
			) &&
			type_formation_has_name_symbol(
				terms,
				type_declarations,
				result,
				primitive->as.pure_primitive.type_symbol_id
			) ? 0 : -1;
	}
	if (primitive->as.pure_primitive.primitive_id == PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT) {
		return type_formation_is_nat_shape(
				terms, type_declarations, contexts, domain
			) &&
			type_formation_has_name_symbol(
				terms,
				type_declarations,
				domain,
				primitive->as.pure_primitive.type_symbol_id
			) &&
			term_is_primitive_text(terms, result) ? 0 : -1;
	}
	return -1;
}

static int validate_context_reindex_proof(
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 1 ||
		proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_context_ids[0] == relation->context_id ||
		proof->premise_subjects[0] != relation->subject ||
		proof->premise_classifiers[0] != relation->classifier) {
		return -1;
	}
	return 0;
}

static int validate_is_type_from_has_type_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		proof->premise_count != 1 ||
		proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_subjects[0] != relation->subject ||
		proof->premise_classifiers[0] != relation->classifier ||
		!term_exists(terms, relation->subject) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	return 0;
}

static int validate_pi_formation_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 3 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_PI) ||
		!term_is_universe_var(terms, relation->classifier)) {
		return -1;
	}
	uint32_t domain;
	uint32_t codomain_family;
	if (pi_parts(terms, relation->subject, &domain, &codomain_family) != 0) {
		return -1;
	}
	uint32_t family = terms->terms[relation->subject].as.pi.codomain_family;
	uint32_t binder_id;
	uint32_t codomain;
	if (prototype_term_pure_family_parts(
			terms, family, &binder_id, &codomain
		) != 0 || family >= terms->term_count ||
		terms->terms[family].tag != PROTOTYPE_TERM_THUNK) {
		return -1;
	}
	uint32_t lambda = terms->terms[family].as.thunk.computation;
	if (lambda >= terms->term_count ||
		terms->terms[lambda].tag != PROTOTYPE_TERM_LAMBDA ||
		terms->terms[lambda].as.lambda.binder_id != binder_id ||
		terms->terms[lambda].as.lambda.body >= terms->term_count ||
		terms->terms[terms->terms[lambda].as.lambda.body].tag != PROTOTYPE_TERM_RETURN ||
		terms->terms[terms->terms[lambda].as.lambda.body].as.return_term.value != codomain ||
		proof->premise_classifiers[2] >= terms->term_count ||
		terms->terms[proof->premise_classifiers[2]].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return -1;
	}
	uint32_t returned_classifier =
		terms->terms[proof->premise_classifiers[2]].as.thunk_type.computation;
	if (returned_classifier >= terms->term_count ||
		terms->terms[returned_classifier].tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	unsigned effects;
	uint32_t effect_row = terms->terms[returned_classifier].as.computation_type.label;
	uint32_t lambda_classifier = terms->terms[returned_classifier].as.computation_type.result;
	if (prototype_term_effect_row_closed_bits(terms, effect_row, &effects) != 0 ||
		effects != PROTOTYPE_EFFECT_OPERATION_LABEL_NONE || lambda_classifier >= terms->term_count ||
		terms->terms[lambda_classifier].tag != PROTOTYPE_TERM_PI ||
		terms->terms[lambda_classifier].as.pi.domain != domain) {
		return -1;
	}
	uint32_t binder_var;
	uint32_t lambda_result;
	uint32_t lambda_result_binder;
	uint32_t lambda_result_body;
	if (prototype_term_var((struct prototype_term_db*)terms, binder_id, &binder_var) != 0 ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[lambda_classifier].as.pi.codomain_family,
			&lambda_result_binder,
			&lambda_result_body
		) != 0 || prototype_term_graph_substitute_bound_var(
			(struct prototype_term_db*)terms,
			NULL,
			lambda_result_body,
			lambda_result_binder,
			binder_var,
			&lambda_result
		) != 0 || lambda_result != returned_classifier) {
		return -1;
	}
	if (proof->premise_kinds[0] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_subjects[0] != domain ||
		proof->premise_classifiers[0] != relation->classifier ||
		proof->premise_kinds[1] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_subjects[1] != codomain ||
		proof->premise_classifiers[1] != relation->classifier ||
		proof->premise_kinds[2] != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_subjects[2] != family ||
		proof->premise_classifiers[2] >= terms->term_count ||
		terms->terms[proof->premise_classifiers[2]].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		terms->terms[proof->premise_classifiers[2]].as.thunk_type.computation != returned_classifier) {
		return -1;
	}
	(void)codomain_family;
	return 0;
}

static int validate_proof_acyclic_at(
	const struct prototype_judgement_db* judgement,
	uint32_t proof_id,
	unsigned char* colors
) {
	if (!judgement || !colors || proof_id >= judgement->proof_count) {
		return -1;
	}
	if (colors[proof_id] == 1) {
		return -1;
	}
	if (colors[proof_id] == 2) {
		return 0;
	}
	colors[proof_id] = 1;
	const struct prototype_judgement_proof* proof = &judgement->proofs[proof_id];
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INVALID) {
		colors[proof_id] = 2;
		return 0;
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		if (proof->premise_proof_ids[i] >= judgement->proof_count ||
			validate_proof_acyclic_at(
				judgement,
				proof->premise_proof_ids[i],
				colors
			) != 0) {
			return -1;
		}
	}
	colors[proof_id] = 2;
	return 0;
}

static int validate_effect_operation_type_intro_proof(
	const struct prototype_term_db* terms,
	const struct prototype_judgement_relation* relation,
	const struct prototype_judgement_proof* proof
) {
	if (!terms || !relation || !proof ||
		relation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		proof->premise_count != 0 ||
		!term_has_tag(terms, relation->subject, PROTOTYPE_TERM_EFFECT_OPERATION) ||
		!term_exists(terms, relation->classifier)) {
		return -1;
	}
	const struct prototype_term* operation = &terms->terms[relation->subject];
	if (!prototype_term_effect_operation_declaration(
			operation->as.effect_operation.operation_id
		) || operation->as.effect_operation.classifier >= terms->term_count) {
		return -1;
	}
	return relation->classifier == operation->as.effect_operation.classifier ? 0 : -1;
}

static int validate_proof_acyclic(const struct prototype_judgement_db* judgement) {
	if (!judgement) {
		return -1;
	}
	if (judgement->proof_count == 0) {
		return 0;
	}
	unsigned char* colors = calloc(judgement->proof_count, sizeof(*colors));
	if (!colors) {
		return -1;
	}
	for (uint32_t i = 0; i < (uint32_t)judgement->proof_count; ++i) {
		if (validate_proof_acyclic_at(judgement, i, colors) != 0) {
			free(colors);
			return -1;
		}
	}
	free(colors);
	return 0;
}

static int validate_proof_relation_coverage(
	const struct prototype_judgement_db* judgement
) {
	if (!judgement) {
		return -1;
	}
	if (judgement->proof_count == 0) {
		return judgement->relation_count == 0 ? 0 : -1;
	}
	unsigned char* seen = calloc(judgement->proof_count, sizeof(*seen));
	if (!seen) {
		return -1;
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			continue;
		}
		if (relation->proof_id >= judgement->proof_count ||
			seen[relation->proof_id]) {
			free(seen);
			return -1;
		}
		const struct prototype_judgement_proof* proof =
			&judgement->proofs[relation->proof_id];
		if (proof->proof_kind != relation->proof_kind ||
			proof->conclusion_kind != relation->kind ||
			proof->conclusion_subject != relation->subject ||
			proof->conclusion_classifier != relation->classifier) {
			free(seen);
			return -1;
		}
		seen[relation->proof_id] = 1;
	}
	for (size_t i = 0; i < judgement->proof_count; ++i) {
		if (judgement->proofs[i].proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID &&
			!seen[i]) {
			free(seen);
			return -1;
		}
	}
	free(seen);
	return 0;
}

static int validate_proof_rule_parameters(
	const struct prototype_judgement_proof* proof
) {
	if (!proof) {
		return -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION) {
		return proof->assumption_index == PROTOTYPE_INVALID_ID &&
			proof->constructor_owner_view != PROTOTYPE_INVALID_ID &&
			proof->constructor_index != PROTOTYPE_INVALID_ID &&
			proof->constructor_field_index != PROTOTYPE_INVALID_ID &&
			proof->induction_match == PROTOTYPE_INVALID_ID &&
			proof->induction_case_index == PROTOTYPE_INVALID_ID &&
			proof->induction_field_index == PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM) {
		return proof->assumption_index == PROTOTYPE_INVALID_ID &&
			proof->constructor_owner_view == PROTOTYPE_INVALID_ID &&
			proof->constructor_index == PROTOTYPE_INVALID_ID &&
			proof->constructor_field_index == PROTOTYPE_INVALID_ID &&
			proof->induction_match != PROTOTYPE_INVALID_ID &&
			proof->induction_case_index != PROTOTYPE_INVALID_ID &&
			proof->induction_field_index != PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->proof_kind ==
		PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION) {
		return proof->assumption_index == PROTOTYPE_INVALID_ID &&
			proof->constructor_owner_view != PROTOTYPE_INVALID_ID &&
			proof->constructor_index == PROTOTYPE_INVALID_ID &&
			proof->constructor_field_index == PROTOTYPE_INVALID_ID &&
			proof->induction_match == PROTOTYPE_INVALID_ID &&
			proof->induction_case_index == PROTOTYPE_INVALID_ID &&
			proof->induction_field_index == PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
		return proof->assumption_index != PROTOTYPE_INVALID_ID &&
			proof->constructor_owner_view == PROTOTYPE_INVALID_ID &&
			proof->constructor_index == PROTOTYPE_INVALID_ID &&
			proof->constructor_field_index == PROTOTYPE_INVALID_ID &&
			proof->induction_match == PROTOTYPE_INVALID_ID &&
			proof->induction_case_index == PROTOTYPE_INVALID_ID &&
			proof->induction_field_index == PROTOTYPE_INVALID_ID ? 0 : -1;
	}
	if (proof->assumption_index != PROTOTYPE_INVALID_ID ||
		proof->constructor_owner_view != PROTOTYPE_INVALID_ID ||
		proof->constructor_index != PROTOTYPE_INVALID_ID ||
		proof->constructor_field_index != PROTOTYPE_INVALID_ID ||
		proof->induction_match != PROTOTYPE_INVALID_ID ||
		proof->induction_case_index != PROTOTYPE_INVALID_ID ||
		proof->induction_field_index != PROTOTYPE_INVALID_ID) {
		return -1;
	}
	return 0;
}

static int judgement_classifier_free_result_effect_row_binder(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_binder_id
) {
	if (!terms || !type_declarations || !p_binder_id ||
		classifier >= terms->term_count) {
		return -1;
	}
	*p_binder_id = PROTOTYPE_INVALID_ID;
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			terms, type_declarations, NULL, classifier, &view
		) != 0 ||
		view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		return 0;
	}
	if (view.effect_row >= terms->term_count ||
		terms->terms[view.effect_row].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return 0;
	}
	uint32_t binder_id = terms->terms[view.effect_row].as.effect_row_var.binder_id;
	if (!prototype_term_contains_free_binder(
		terms,
		classifier,
		binder_id
	)) {
		return 0;
	}
	*p_binder_id = binder_id;
	return 1;
}

static int judgement_proof_has_effect_row_assumption(
	const struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t proof_id,
	uint32_t binder_id,
	size_t depth
) {
	if (!judgement || !terms || proof_id >= judgement->proof_count ||
		depth > judgement->proof_count) {
		return -1;
	}
	const struct prototype_judgement_proof* proof = &judgement->proofs[proof_id];
	if (proof->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION) {
		return proof->conclusion_classifier < terms->term_count &&
			prototype_term_contains_free_binder(
				terms, proof->conclusion_classifier, binder_id
			);
	}
	for (uint32_t i = 0; i < proof->premise_count; ++i) {
		if (proof->premise_proof_ids[i] == PROTOTYPE_INVALID_ID) {
			continue;
		}
		int found = judgement_proof_has_effect_row_assumption(
			judgement,
			terms,
			proof->premise_proof_ids[i],
			binder_id,
			depth + 1
		);
		if (found != 0) {
			return found;
		}
	}
	return 0;
}

int prototype_judgement_validate_proofs(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_judgement_db* judgement
) {
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement) {
		return -1;
	}
	if (validate_proof_relation_coverage(judgement) != 0) {
		return -1;
	}
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		if (relation->kind == PROTOTYPE_JUDGEMENT_KIND_UNKNOWN) {
			continue;
		}
		if (relation->proof_id >= judgement->proof_count) {
			return -1;
		}
			const struct prototype_judgement_proof* proof =
				&judgement->proofs[relation->proof_id];
			if (proof->proof_kind != relation->proof_kind ||
				proof->conclusion_kind != relation->kind ||
				proof->conclusion_subject != relation->subject ||
				proof->conclusion_classifier != relation->classifier ||
				proof->conclusion_context_id != relation->context_id ||
				proof->premise_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES ||
				validate_proof_rule_parameters(proof) != 0) {
				return -1;
			}
			uint32_t free_effect_row_binder;
			int has_free_effect_row =
				judgement_classifier_free_result_effect_row_binder(
					terms,
					type_declarations,
					relation->classifier,
					&free_effect_row_binder
				);
			int has_effect_row_assumption = has_free_effect_row > 0 ?
				judgement_proof_has_effect_row_assumption(
					judgement,
					terms,
					relation->proof_id,
					free_effect_row_binder,
					0
				) : 0;
			if (has_effect_row_assumption < 0 ||
				has_free_effect_row < 0 ||
				(has_free_effect_row != 0 && has_effect_row_assumption == 0)) {
				return -1;
			}
			switch (relation->proof_kind) {
			case PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO:
				if (validate_type_formation_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO:
				if (validate_constructor_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION:
				if (validate_constructor_spine_formation_proof(
						terms,
						type_declarations,
						contexts,
						substitutions,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
					if (validate_assumption_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
				if (proof->premise_count != 2) {
					return -1;
				}
					if (validate_lambda_intro_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							relation,
							proof
						) != 0) {
						return -1;
					}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
				if (proof->premise_count != 2) {
					return -1;
				}
					if (validate_app_elim_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
				if (validate_return_intro_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
				if (validate_thunk_intro_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
				if (validate_force_elim_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
				if (validate_computation_fold_elim_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
					if (validate_operation_request_intro_proof(
							terms,
							type_declarations,
							contexts,
							substitutions,
							relation,
							proof
						) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
				if (validate_match_elim_proof(
						terms,
						type_declarations,
						judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
				if (validate_solved_match_motive_proof(
						terms, type_declarations, judgement, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM:
				if (proof->premise_count != 0) {
					return -1;
				}
				if (validate_induction_hypothesis_elim_proof(
						terms,
						type_declarations,
						judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
				case PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO:
					if (validate_text_literal_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO:
					if (validate_int_literal_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO:
					if (validate_pure_primitive_type_intro_proof(
						terms,
						type_declarations,
						contexts,
						relation,
						proof
					) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO:
					if (validate_effect_operation_type_intro_proof(
						terms, relation, proof
					) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO:
					if (validate_text_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO:
					if (validate_int_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
				case PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO:
					if (validate_host_type_intro_proof(terms, relation, proof) != 0) {
						return -1;
					}
					break;
			case PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO:
				if (validate_match_type_formation_intro_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
				if (validate_conversion_proof(
						terms,
						type_declarations,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_DECLARATION:
				if (validate_declaration_proof(
						terms,
						type_declarations,
						judgement,
						relation,
						proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE:
				if (validate_is_type_from_has_type_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY:
				if (validate_universe_cumulativity_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO:
				if (validate_pi_formation_intro_proof(terms, relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_REINDEX:
				if (validate_context_reindex_proof(relation, proof) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
				if (validate_effect_weaken_proof(
						terms, type_declarations, relation, proof
					) != 0) {
					return -1;
				}
				break;
			case PROTOTYPE_JUDGEMENT_PROOF_INVALID:
				return -1;
			default:
				return -1;
		}
		for (uint32_t j = 0; j < proof->premise_count; ++j) {
			uint32_t premise_proof_id = proof->premise_proof_ids[j];
			if (premise_proof_id >= judgement->proof_count ||
				judgement->proofs[premise_proof_id].conclusion_kind !=
					proof->premise_kinds[j] ||
				judgement->proofs[premise_proof_id].conclusion_subject !=
					proof->premise_subjects[j] ||
				judgement->proofs[premise_proof_id].conclusion_classifier !=
					proof->premise_classifiers[j] ||
				judgement->proofs[premise_proof_id].conclusion_context_id !=
					proof->premise_context_ids[j]) {
				return -1;
			}
		}
	}
	if (validate_proof_acyclic(judgement) != 0) {
		return -1;
	}
	return 0;
}

void prototype_judgement_delta_drop_temporary_derivations(
	struct prototype_judgement_delta* delta
) {
	if (!delta) {
		return;
	}
	uint8_t* removed = calloc(delta->relation_count, sizeof(*removed));
	struct prototype_judgement_proof* source_proofs = malloc(
		delta->proof_count * sizeof(*source_proofs)
	);
	if ((delta->relation_count != 0 && !removed) ||
		(delta->proof_count != 0 && !source_proofs)) {
		free(removed);
		free(source_proofs);
		return;
	}
	if (delta->proof_count != 0) {
		memcpy(
			source_proofs,
			delta->proofs,
			delta->proof_count * sizeof(*source_proofs)
		);
	}
	for (size_t read = 0; read < delta->relation_count; ++read) {
		const struct prototype_judgement_relation* relation = &delta->relations[read];
		if (relation->proof_id >= delta->proof_count) {
			free(removed);
			free(source_proofs);
			return;
		}
		if (relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM ||
			relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO ||
			relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM ||
			(relation->proof_kind == PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM &&
				source_proofs[relation->proof_id].premise_count > 2)) {
			removed[read] = 1;
		}
	}
	for (;;) {
		int changed = 0;
		for (size_t read = 0; read < delta->relation_count; ++read) {
			if (removed[read]) {
				continue;
			}
			const struct prototype_judgement_relation* relation =
				&delta->relations[read];
			const struct prototype_judgement_proof* proof =
				&source_proofs[relation->proof_id];
			for (uint32_t premise = 0; premise < proof->premise_count; ++premise) {
				int found = 0;
				for (size_t candidate = 0;
					candidate < delta->relation_count;
					++candidate) {
					const struct prototype_judgement_relation* premise_relation =
						&delta->relations[candidate];
					if (!removed[candidate] &&
						premise_relation->kind == proof->premise_kinds[premise] &&
						premise_relation->context_id ==
							proof->premise_context_ids[premise] &&
						premise_relation->subject ==
							proof->premise_subjects[premise] &&
						premise_relation->classifier ==
							proof->premise_classifiers[premise]) {
						found = 1;
						break;
					}
				}
				if (!found) {
					removed[read] = 1;
					changed = 1;
					break;
				}
			}
		}
		if (!changed) {
			break;
		}
	}
	size_t write = 0;
	for (size_t read = 0; read < delta->relation_count; ++read) {
		if (removed[read]) {
			continue;
		}
		const struct prototype_judgement_relation* relation = &delta->relations[read];
		if (write != read) {
			delta->relations[write] = delta->relations[read];
		}
		delta->proofs[write] = source_proofs[relation->proof_id];
		for (uint32_t premise = 0;
			premise < delta->proofs[write].premise_count;
			++premise) {
			delta->proofs[write].premise_proof_ids[premise] =
				PROTOTYPE_INVALID_ID;
		}
		delta->relations[write].proof_id = (uint32_t)write;
		write++;
	}
	delta->relation_count = write;
	delta->proof_count = write;
	free(removed);
	free(source_proofs);
}

int prototype_judgement_add_is_type(
	struct prototype_judgement_db* judgement,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t universe
) {
	if (!term_exists(terms, subject) || !term_is_universe_var(terms, universe)) {
		return -1;
	}
	uint32_t premise_subject = subject;
	uint32_t premise_classifier = universe;
	return add_relation_with_premises(
		judgement,
		0,
		PROTOTYPE_JUDGEMENT_KIND_IS_TYPE,
		subject,
		universe,
		PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE,
		NULL,
		&premise_subject,
		&premise_classifier,
		1
	);
}

int prototype_judgement_lookup_classifier(
	const struct prototype_judgement_db* judgement,
	uint32_t subject,
	uint32_t* p_classifier
) {
	return lookup_classifier(judgement, NULL, subject, p_classifier);
}

int prototype_judgement_delta_lookup_classifier(
	const struct prototype_judgement_delta* delta,
	const struct prototype_term_db* terms,
	uint32_t subject,
	uint32_t* p_classifier
) {
	return lookup_delta_classifier(delta, terms, subject, p_classifier);
}

static const char* judgement_kind_name(int kind) {
	switch (kind) {
		case PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE:
			return "has-type";
		case PROTOTYPE_JUDGEMENT_KIND_IS_TYPE:
			return "is-type";
		default:
			return "unknown";
	}
}

static const char* proof_kind_name(int proof_kind) {
	switch (proof_kind) {
		case PROTOTYPE_JUDGEMENT_PROOF_TYPE_FORMATION_INTRO:
			return "type-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO:
			return "constructor-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION:
			return "constructor-spine-formation";
		case PROTOTYPE_JUDGEMENT_PROOF_BINDER_ASSUMPTION:
			return "binder-assumption";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_PATTERN_ASSUMPTION:
			return "match-pattern-assumption";
		case PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO:
			return "lambda-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM:
			return "app-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO:
			return "return-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO:
			return "thunk-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM:
			return "force-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_COMPUTATION_FOLD_ELIM:
			return "computation-fold-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_OPERATION_REQUEST_INTRO:
			return "operation-request-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_TYPE_FORMATION_INTRO:
			return "match-type-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM:
			return "match-elim";
		case PROTOTYPE_JUDGEMENT_PROOF_SOLVED_MATCH_MOTIVE:
			return "solved-match-motive";
		case PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM:
			return "ih-elim";
			case PROTOTYPE_JUDGEMENT_PROOF_TEXT_LITERAL_INTRO:
				return "text-literal-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_INT_LITERAL_INTRO:
				return "int-literal-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_PURE_PRIMITIVE_TYPE_INTRO:
				return "pure-primitive-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_OPERATION_TYPE_INTRO:
				return "effect-operation-type-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONVERSION:
			return "conversion";
			case PROTOTYPE_JUDGEMENT_PROOF_TEXT_TYPE_INTRO:
				return "text-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_INT_TYPE_INTRO:
				return "int-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_HOST_TYPE_INTRO:
				return "host-type-intro";
			case PROTOTYPE_JUDGEMENT_PROOF_IS_TYPE_FROM_HAS_TYPE:
				return "is-type-from-has-type";
		case PROTOTYPE_JUDGEMENT_PROOF_DECLARATION:
			return "declaration";
		case PROTOTYPE_JUDGEMENT_PROOF_UNIVERSE_CUMULATIVITY:
			return "universe-cumulativity";
		case PROTOTYPE_JUDGEMENT_PROOF_PI_FORMATION_INTRO:
			return "pi-formation-intro";
		case PROTOTYPE_JUDGEMENT_PROOF_CONTEXT_REINDEX:
			return "context-reindex";
		case PROTOTYPE_JUDGEMENT_PROOF_EFFECT_WEAKEN:
			return "effect-weaken";
		default:
			return "invalid";
	}
}

void prototype_judgement_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement
) {
	if (!output || !symbols || !type_declarations || !terms || !judgement) {
		return;
	}

	fprintf(output, "judgements=%zu\n", judgement->relation_count);
	for (size_t i = 0; i < judgement->relation_count; ++i) {
		const struct prototype_judgement_relation* relation = &judgement->relations[i];
		fprintf(output, "%s ", judgement_kind_name(relation->kind));
		prototype_term_print_debug(output, symbols, type_declarations, terms, relation->subject);
		fprintf(output, " ");
		prototype_term_print_debug(output, symbols, type_declarations, terms, relation->classifier);
		fprintf(output, " [%s]", proof_kind_name(relation->proof_kind));
		if (relation->proof_id < judgement->proof_count) {
			const struct prototype_judgement_proof* proof =
				&judgement->proofs[relation->proof_id];
			fprintf(output, " proof#%u premises=%u", relation->proof_id, proof->premise_count);
		}
		fprintf(output, "\n");
	}
}
