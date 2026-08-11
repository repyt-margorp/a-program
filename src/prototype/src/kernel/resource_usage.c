#include "a_program/kernel/resource_usage.h"

#include "a_program/core/term.h"

#include <stdlib.h>
#include <string.h>

static int usage_grade_valid(int grade) {
	return grade >= PROTOTYPE_USAGE_ZERO && grade <= PROTOTYPE_USAGE_MANY;
}

int prototype_usage_grade_add(int left, int right, int* p_result) {
	if (!p_result || !usage_grade_valid(left) || !usage_grade_valid(right)) {
		return -1;
	}
	if (left == PROTOTYPE_USAGE_ZERO) {
		*p_result = right;
	} else if (right == PROTOTYPE_USAGE_ZERO) {
		*p_result = left;
	} else {
		*p_result = PROTOTYPE_USAGE_MANY;
	}
	return 0;
}

int prototype_usage_grade_multiply(int left, int right, int* p_result) {
	if (!p_result || !usage_grade_valid(left) || !usage_grade_valid(right)) {
		return -1;
	}
	if (left == PROTOTYPE_USAGE_ZERO || right == PROTOTYPE_USAGE_ZERO) {
		*p_result = PROTOTYPE_USAGE_ZERO;
	} else if (left == PROTOTYPE_USAGE_ONE && right == PROTOTYPE_USAGE_ONE) {
		*p_result = PROTOTYPE_USAGE_ONE;
	} else {
		*p_result = PROTOTYPE_USAGE_MANY;
	}
	return 0;
}

int prototype_usage_grade_join(int left, int right, int* p_result) {
	if (!p_result || !usage_grade_valid(left) || !usage_grade_valid(right)) {
		return -1;
	}
	*p_result = left > right ? left : right;
	return 0;
}

void prototype_usage_vector_init(
	struct prototype_usage_vector* vector,
	struct prototype_usage_entry* entries,
	size_t capacity
) {
	if (!vector) {
		return;
	}
	vector->entries = entries;
	vector->count = 0;
	vector->capacity = capacity;
}

static size_t usage_vector_lower_bound(
	const struct prototype_usage_vector* vector,
	uint32_t binding_id
) {
	size_t first = 0;
	size_t count = vector->count;
	while (count != 0) {
		size_t step = count / 2;
		size_t candidate = first + step;
		if (vector->entries[candidate].binding_id < binding_id) {
			first = candidate + 1;
			count -= step + 1;
		} else {
			count = step;
		}
	}
	return first;
}

int prototype_usage_vector_copy(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* source
) {
	if (!target || !source || (!target->entries && source->count != 0) ||
		target->capacity < source->count) {
		return -1;
	}
	memmove(
		target->entries,
		source->entries,
		source->count * sizeof(*source->entries)
	);
	target->count = source->count;
	return 0;
}

int prototype_usage_vector_get(
	const struct prototype_usage_vector* vector,
	uint32_t binding_id,
	int* p_grade
) {
	if (!vector || !p_grade || (!vector->entries && vector->count != 0)) {
		return -1;
	}
	size_t index = usage_vector_lower_bound(vector, binding_id);
	*p_grade = index < vector->count &&
		vector->entries[index].binding_id == binding_id ?
		vector->entries[index].grade : PROTOTYPE_USAGE_ZERO;
	return 0;
}

int prototype_usage_vector_set(
	struct prototype_usage_vector* vector,
	uint32_t binding_id,
	int grade
) {
	if (!vector || !usage_grade_valid(grade) ||
		(!vector->entries && vector->capacity != 0)) {
		return -1;
	}
	size_t index = usage_vector_lower_bound(vector, binding_id);
	int present = index < vector->count &&
		vector->entries[index].binding_id == binding_id;
	if (grade == PROTOTYPE_USAGE_ZERO) {
		if (present) {
			memmove(
				&vector->entries[index],
				&vector->entries[index + 1],
				(vector->count - index - 1) * sizeof(*vector->entries)
			);
			vector->count--;
		}
		return 0;
	}
	if (present) {
		vector->entries[index].grade = grade;
		return 0;
	}
	if (vector->count >= vector->capacity) {
		return -1;
	}
	memmove(
		&vector->entries[index + 1],
		&vector->entries[index],
		(vector->count - index) * sizeof(*vector->entries)
	);
	vector->entries[index] = (struct prototype_usage_entry){ binding_id, grade };
	vector->count++;
	return 0;
}

static int usage_vector_combine(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* left,
	const struct prototype_usage_vector* right,
	int (*combine)(int, int, int*)
) {
	if (!target || !left || !right || !combine ||
		target == left || target == right) {
		return -1;
	}
	target->count = 0;
	size_t left_index = 0;
	size_t right_index = 0;
	while (left_index < left->count || right_index < right->count) {
		uint32_t binding_id;
		int left_grade = PROTOTYPE_USAGE_ZERO;
		int right_grade = PROTOTYPE_USAGE_ZERO;
		if (right_index >= right->count ||
			(left_index < left->count &&
			 left->entries[left_index].binding_id <
				right->entries[right_index].binding_id)) {
			binding_id = left->entries[left_index].binding_id;
			left_grade = left->entries[left_index++].grade;
		} else if (left_index >= left->count ||
			right->entries[right_index].binding_id <
				left->entries[left_index].binding_id) {
			binding_id = right->entries[right_index].binding_id;
			right_grade = right->entries[right_index++].grade;
		} else {
			binding_id = left->entries[left_index].binding_id;
			left_grade = left->entries[left_index++].grade;
			right_grade = right->entries[right_index++].grade;
		}
		int grade;
		if (combine(left_grade, right_grade, &grade) != 0 ||
			prototype_usage_vector_set(target, binding_id, grade) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_usage_vector_add(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* left,
	const struct prototype_usage_vector* right
) {
	return usage_vector_combine(target, left, right, prototype_usage_grade_add);
}

int prototype_usage_vector_join(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* left,
	const struct prototype_usage_vector* right
) {
	return usage_vector_combine(target, left, right, prototype_usage_grade_join);
}

int prototype_usage_vector_scale(
	struct prototype_usage_vector* target,
	int scalar,
	const struct prototype_usage_vector* source
) {
	if (!target || !source || target == source || !usage_grade_valid(scalar)) {
		return -1;
	}
	target->count = 0;
	for (size_t i = 0; i < source->count; ++i) {
		int grade;
		if (prototype_usage_grade_multiply(
				scalar, source->entries[i].grade, &grade
			) != 0 || prototype_usage_vector_set(
				target, source->entries[i].binding_id, grade
			) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_usage_vector_transform(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* source,
	const struct prototype_usage_substitution_column* columns,
	size_t column_count
) {
	if (!target || !source || (!columns && column_count != 0) || target == source) {
		return -1;
	}
	target->count = 0;
	struct prototype_usage_entry scaled_entries[target->capacity == 0 ? 1 : target->capacity];
	struct prototype_usage_entry sum_entries[target->capacity == 0 ? 1 : target->capacity];
	struct prototype_usage_vector scaled;
	struct prototype_usage_vector sum;
	prototype_usage_vector_init(&scaled, scaled_entries, target->capacity);
	prototype_usage_vector_init(&sum, sum_entries, target->capacity);
	for (size_t i = 0; i < source->count; ++i) {
		const struct prototype_usage_vector* replacement = NULL;
		for (size_t j = 0; j < column_count; ++j) {
			if (columns[j].target_binding_id == source->entries[i].binding_id) {
				replacement = columns[j].source_usage;
				break;
			}
		}
		struct prototype_usage_entry identity_entry;
		struct prototype_usage_vector identity;
		if (!replacement) {
			prototype_usage_vector_init(&identity, &identity_entry, 1);
			if (prototype_usage_vector_set(
					&identity,
					source->entries[i].binding_id,
					PROTOTYPE_USAGE_ONE
				) != 0) {
				return -1;
			}
			replacement = &identity;
		}
		if (prototype_usage_vector_scale(
				&scaled, source->entries[i].grade, replacement
			) != 0 || prototype_usage_vector_add(
				&sum, target, &scaled
			) != 0 || prototype_usage_vector_copy(target, &sum) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_usage_vector_equal(
	const struct prototype_usage_vector* left,
	const struct prototype_usage_vector* right
) {
	if (!left || !right || left->count != right->count) {
		return 0;
	}
	return left->count == 0 || memcmp(
		left->entries,
		right->entries,
		left->count * sizeof(*left->entries)
	) == 0;
}

static int term_usage_accumulate(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* source,
	int join
) {
	if (!target || !source) {
		return -1;
	}
	for (size_t i = 0; i < source->count; ++i) {
		int previous;
		int combined;
		if (prototype_usage_vector_get(
				target, source->entries[i].binding_id, &previous
			) != 0 || (join ? prototype_usage_grade_join(
				previous, source->entries[i].grade, &combined
			) : prototype_usage_grade_add(
				previous, source->entries[i].grade, &combined
			)) != 0 || prototype_usage_vector_set(
				target, source->entries[i].binding_id, combined
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int term_usage_visit(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	struct prototype_usage_vector* usage,
	uint32_t depth
);

static int term_usage_visit_scoped(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	const uint32_t* bound_bindings,
	uint32_t bound_count,
	struct prototype_usage_vector* target,
	uint32_t depth
) {
	struct prototype_usage_entry* entries = target->capacity == 0 ? NULL :
		malloc(target->capacity * sizeof(*entries));
	if (target->capacity != 0 && !entries) {
		return -1;
	}
	struct prototype_usage_vector local;
	prototype_usage_vector_init(&local, entries, target->capacity);
	int status = term_usage_visit(terms, term_id, &local, depth + 1);
	for (uint32_t i = 0; status == 0 && i < bound_count; ++i) {
		status = prototype_usage_vector_set(
			&local, bound_bindings[i], PROTOTYPE_USAGE_ZERO
		);
	}
	if (status == 0) {
		status = term_usage_accumulate(target, &local, 0);
	}
	free(entries);
	return status;
}

static int term_usage_visit_branches(
	const struct prototype_term_db* terms,
	uint32_t first_case,
	uint32_t case_count,
	struct prototype_usage_vector* target,
	uint32_t depth
) {
	struct prototype_usage_entry* branch_entries = target->capacity == 0 ? NULL :
		malloc(target->capacity * sizeof(*branch_entries));
	struct prototype_usage_entry* case_entries = target->capacity == 0 ? NULL :
		malloc(target->capacity * sizeof(*case_entries));
	if (target->capacity != 0 && (!branch_entries || !case_entries)) {
		free(branch_entries);
		free(case_entries);
		return -1;
	}
	struct prototype_usage_vector branches;
	struct prototype_usage_vector case_usage;
	prototype_usage_vector_init(&branches, branch_entries, target->capacity);
	prototype_usage_vector_init(&case_usage, case_entries, target->capacity);
	int status = 0;
	for (uint32_t i = 0; status == 0 && i < case_count; ++i) {
		const struct prototype_match_case* match_case = &terms->cases[first_case + i];
		case_usage.count = 0;
		status = term_usage_visit(terms, match_case->body, &case_usage, depth + 1);
		for (uint32_t j = 0; status == 0 && j < match_case->binder_count; ++j) {
			status = prototype_usage_vector_set(
				&case_usage,
				terms->case_binders[match_case->first_binder + j].binding_id,
				PROTOTYPE_USAGE_ZERO
			);
		}
		if (status == 0) {
			status = term_usage_accumulate(&branches, &case_usage, 1);
		}
	}
	if (status == 0) {
		status = term_usage_accumulate(target, &branches, 0);
	}
	free(case_entries);
	free(branch_entries);
	return status;
}

static int term_usage_visit(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	struct prototype_usage_vector* usage,
	uint32_t depth
) {
	if (!terms || !usage || term_id >= terms->term_count || depth > 2048) {
		return -1;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
	case PROTOTYPE_TERM_VAR: {
		int previous;
		int combined;
		return prototype_usage_vector_get(
			usage, term->as.var.binding_id, &previous
		) != 0 || prototype_usage_grade_add(
			previous, PROTOTYPE_USAGE_ONE, &combined
		) != 0 ? -1 : prototype_usage_vector_set(
			usage, term->as.var.binding_id, combined
		);
	}
	case PROTOTYPE_TERM_APP:
		return term_usage_visit(
			terms, term->as.app.function, usage, depth + 1
		) != 0 ? -1 : term_usage_visit(
			terms, term->as.app.argument, usage, depth + 1
		);
	case PROTOTYPE_TERM_LAMBDA:
		return term_usage_visit_scoped(
			terms,
			term->as.lambda.body,
			&term->as.lambda.binding_id,
			1,
			usage,
			depth
		);
	case PROTOTYPE_TERM_MATCH:
		if (term->as.match.first_case > terms->case_count ||
			term->as.match.case_count > terms->case_count - term->as.match.first_case ||
			term_usage_visit(
				terms, term->as.match.scrutinee, usage, depth + 1
			) != 0) {
			return -1;
		}
		return term_usage_visit_branches(
			terms,
			term->as.match.first_case,
			term->as.match.case_count,
			usage,
			depth
		);
	case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
		return term_usage_visit(
			terms, term->as.induction_hypothesis.argument, usage, depth + 1
		);
	case PROTOTYPE_TERM_RETURN:
		return term_usage_visit(
			terms, term->as.return_term.value, usage, depth + 1
		);
	case PROTOTYPE_TERM_THUNK:
		return term_usage_visit(
			terms, term->as.thunk.computation, usage, depth + 1
		);
	case PROTOTYPE_TERM_FORCE:
		return term_usage_visit(
			terms, term->as.force.value, usage, depth + 1
		);
	case PROTOTYPE_TERM_OPERATION_REQUEST:
		return term_usage_visit(
			terms, term->as.operation_request.operation, usage, depth + 1
		) != 0 || term_usage_visit(
			terms, term->as.operation_request.argument, usage, depth + 1
		) != 0 ? -1 : term_usage_visit(
			terms, term->as.operation_request.continuation, usage, depth + 1
		);
	case PROTOTYPE_TERM_COMPUTATION_FOLD:
		if (term->as.computation_fold.first_clause >
				terms->computation_fold_clause_count ||
			term->as.computation_fold.clause_count >
				terms->computation_fold_clause_count -
					term->as.computation_fold.first_clause || term_usage_visit(
				terms, term->as.computation_fold.computation, usage, depth + 1
			) != 0 || term_usage_visit(
				terms, term->as.computation_fold.return_clause, usage, depth + 1
			) != 0) {
			return -1;
		}
		for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
			const struct prototype_computation_fold_clause* clause =
				&terms->computation_fold_clauses[
					term->as.computation_fold.first_clause + i
				];
			if (term_usage_visit(
					terms, clause->operation, usage, depth + 1
				) != 0 || term_usage_visit(
					terms, clause->body, usage, depth + 1
				) != 0) {
				return -1;
			}
		}
		return 0;
	default:
		return 0;
	}
}

int prototype_term_usage_analyze(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	struct prototype_usage_vector* p_usage
) {
	if (!p_usage) {
		return -1;
	}
	p_usage->count = 0;
	return term_usage_visit(terms, term_id, p_usage, 0);
}
