#include "a_program/kernel/resource_usage.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/core/term.h"

#include <stdio.h>

#define VECTOR_CAPACITY 16

static int expect_grade(
	const struct prototype_usage_vector* vector,
	uint32_t binding_id,
	int expected
) {
	int grade;
	return prototype_usage_vector_get(vector, binding_id, &grade) != 0 ||
		grade != expected ? -1 : 0;
}

int main(void) {
	for (int left = PROTOTYPE_USAGE_ZERO;
		left <= PROTOTYPE_USAGE_MANY;
		++left) {
		for (int right = PROTOTYPE_USAGE_ZERO;
			right <= PROTOTYPE_USAGE_MANY;
			++right) {
			int sum;
			int product;
			int joined;
			if (prototype_usage_grade_add(left, right, &sum) != 0 ||
				prototype_usage_grade_multiply(left, right, &product) != 0 ||
				prototype_usage_grade_join(left, right, &joined) != 0 ||
				sum < left || sum < right || product < PROTOTYPE_USAGE_ZERO ||
				joined != (left > right ? left : right)) {
				return 1;
			}
		}
	}

	struct prototype_usage_entry left_storage[VECTOR_CAPACITY];
	struct prototype_usage_entry right_storage[VECTOR_CAPACITY];
	struct prototype_usage_entry result_storage[VECTOR_CAPACITY];
	struct prototype_usage_vector left;
	struct prototype_usage_vector right;
	struct prototype_usage_vector result;
	prototype_usage_vector_init(&left, left_storage, VECTOR_CAPACITY);
	prototype_usage_vector_init(&right, right_storage, VECTOR_CAPACITY);
	prototype_usage_vector_init(&result, result_storage, VECTOR_CAPACITY);
	if (prototype_usage_vector_set(&left, 10, PROTOTYPE_USAGE_ONE) != 0 ||
		prototype_usage_vector_set(&left, 20, PROTOTYPE_USAGE_ONE) != 0 ||
		prototype_usage_vector_set(&right, 10, PROTOTYPE_USAGE_ONE) != 0 ||
		prototype_usage_vector_set(&right, 30, PROTOTYPE_USAGE_MANY) != 0 ||
		prototype_usage_vector_add(&result, &left, &right) != 0 ||
		expect_grade(&result, 10, PROTOTYPE_USAGE_MANY) != 0 ||
		expect_grade(&result, 20, PROTOTYPE_USAGE_ONE) != 0 ||
		expect_grade(&result, 30, PROTOTYPE_USAGE_MANY) != 0 ||
		prototype_usage_vector_join(&result, &left, &right) != 0 ||
		expect_grade(&result, 10, PROTOTYPE_USAGE_ONE) != 0 ||
		expect_grade(&result, 20, PROTOTYPE_USAGE_ONE) != 0 ||
		expect_grade(&result, 30, PROTOTYPE_USAGE_MANY) != 0) {
		return 1;
	}

	struct prototype_usage_entry source_storage[VECTOR_CAPACITY];
	struct prototype_usage_entry first_column_storage[VECTOR_CAPACITY];
	struct prototype_usage_entry second_column_storage[VECTOR_CAPACITY];
	struct prototype_usage_entry transformed_storage[VECTOR_CAPACITY];
	struct prototype_usage_vector source;
	struct prototype_usage_vector first_column;
	struct prototype_usage_vector second_column;
	struct prototype_usage_vector transformed;
	prototype_usage_vector_init(&source, source_storage, VECTOR_CAPACITY);
	prototype_usage_vector_init(
		&first_column, first_column_storage, VECTOR_CAPACITY
	);
	prototype_usage_vector_init(
		&second_column, second_column_storage, VECTOR_CAPACITY
	);
	prototype_usage_vector_init(
		&transformed, transformed_storage, VECTOR_CAPACITY
	);
	if (prototype_usage_vector_set(&source, 100, PROTOTYPE_USAGE_MANY) != 0 ||
		prototype_usage_vector_set(&source, 200, PROTOTYPE_USAGE_ONE) != 0 ||
		prototype_usage_vector_set(&first_column, 1, PROTOTYPE_USAGE_ONE) != 0 ||
		prototype_usage_vector_set(&first_column, 2, PROTOTYPE_USAGE_ONE) != 0 ||
		prototype_usage_vector_set(&second_column, 2, PROTOTYPE_USAGE_ONE) != 0) {
		return 1;
	}
	const struct prototype_usage_substitution_column columns[2] = {
		{ 100, &first_column },
		{ 200, &second_column }
	};
	if (prototype_usage_vector_transform(
			&transformed, &source, columns, 2
		) != 0 || expect_grade(
			&transformed, 1, PROTOTYPE_USAGE_MANY
		) != 0 || expect_grade(
			&transformed, 2, PROTOTYPE_USAGE_MANY
		) != 0 || prototype_usage_vector_set(
			&transformed, 1, PROTOTYPE_USAGE_ZERO
		) != 0 || expect_grade(
			&transformed, 1, PROTOTYPE_USAGE_ZERO
		) != 0) {
		return 1;
	}

	struct prototype_term term_storage[16];
	struct prototype_match_case case_storage[4];
	int case_labels[4];
	struct prototype_case_binder case_binders[4];
	struct prototype_ih_scope ih_scopes[4];
	struct prototype_term_db terms;
	prototype_term_db_init(
		&terms,
		term_storage,
		16,
		case_storage,
		case_labels,
		4,
		case_binders,
		4,
		ih_scopes,
		4
	);
	uint32_t x_binding = prototype_term_new_binding(&terms);
	uint32_t y_binding = prototype_term_new_binding(&terms);
	uint32_t x;
	uint32_t y;
	uint32_t duplicated;
	uint32_t body;
	uint32_t lambda;
	if (x_binding == PROTOTYPE_INVALID_ID || y_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_var(&terms, x_binding, &x) != 0 ||
		prototype_term_var(&terms, y_binding, &y) != 0 ||
		prototype_term_app(&terms, x, x, &duplicated) != 0 ||
		prototype_term_app(&terms, duplicated, y, &body) != 0 ||
		prototype_term_lambda(&terms, x_binding, body, &lambda) != 0 ||
		prototype_term_usage_analyze(&terms, lambda, &transformed) != 0 ||
		expect_grade(&transformed, x_binding, PROTOTYPE_USAGE_ZERO) != 0 ||
		expect_grade(&transformed, y_binding, PROTOTYPE_USAGE_ONE) != 0 ||
		prototype_term_usage_analyze(&terms, body, &transformed) != 0 ||
		expect_grade(&transformed, x_binding, PROTOTYPE_USAGE_MANY) != 0 ||
		expect_grade(&transformed, y_binding, PROTOTYPE_USAGE_ONE) != 0) {
		return 1;
	}

	struct prototype_judgement_proposition propositions[4];
	struct prototype_judgement_derivation_candidate candidates[4];
	struct prototype_judgement_claim claims[4];
	struct prototype_judgement_derivation derivations[4];
	struct prototype_judgement_candidate_premise candidate_premises[4];
	struct prototype_judgement_premise_edge accepted_premises[4];
	struct prototype_usage_entry proposition_usage[8];
	struct prototype_judgement_db judgement;
	prototype_judgement_db_init(
		&judgement,
		propositions,
		candidates,
		claims,
		derivations,
		4,
		candidate_premises,
		4,
		accepted_premises,
		4
	);
	prototype_judgement_db_set_resource_usage_storage(
		&judgement, proposition_usage, 8
	);
	struct prototype_usage_entry first_usage[1] = {
		{ 7, PROTOTYPE_USAGE_ONE }
	};
	struct prototype_judgement_proposition proposition = {
		.kind = PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE,
		.authority_kind = PROTOTYPE_JUDGEMENT_AUTHORITY_CORE_HELPER,
		.authority_id = 1,
		.context_id = 0,
		.operation_id = PROTOTYPE_INVALID_ID,
		.subject = 1,
		.classifier = 2,
		.resource_usage_count = 1,
		.resource_usage = first_usage
	};
	uint32_t proposition_id;
	uint32_t claim_id;
	if (prototype_judgement_proposition_intern(
			&judgement, &proposition, &proposition_id
		) != 0 || prototype_judgement_claim_intern_exact(
			&judgement, proposition_id, &claim_id
		) != 0) {
		return 1;
	}
	struct prototype_judgement_derivation derivation = {
		.proof_kind = PROTOTYPE_JUDGEMENT_PROOF_DECLARATION,
		.conclusion_claim_id = claim_id,
		.closure_rank = PROTOTYPE_INVALID_ID
	};
	uint32_t first_derivation;
	if (prototype_judgement_derivation_intern_exact(
			&judgement, &derivation, &first_derivation
		) != 0) {
		return 1;
	}
	first_usage[0].grade = PROTOTYPE_USAGE_MANY;
	const struct prototype_judgement_proposition* stored =
		prototype_judgement_proposition_get(&judgement, proposition_id);
	if (!stored || stored->resource_usage_count != 1 ||
		stored->resource_usage[0].grade != PROTOTYPE_USAGE_ONE) {
		return 1;
	}
	proposition.resource_usage = first_usage;
	uint32_t second_proposition;
	if (prototype_judgement_proposition_intern(
			&judgement, &proposition, &second_proposition
		) != 0 || second_proposition == proposition_id ||
		judgement.proposition_count != 2 || judgement.claim_count != 1 ||
		judgement.derivation_count != 1) {
		return 1;
	}

	printf("resource usage checks passed\n");
	return 0;
}
