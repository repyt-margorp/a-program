#ifndef A_PROGRAM_PROTOTYPE_KERNEL_RESOURCE_USAGE_H
#define A_PROGRAM_PROTOTYPE_KERNEL_RESOURCE_USAGE_H

#include <stddef.h>
#include <stdint.h>

struct prototype_term_db;

/* Saturated natural-number grades. MANY is the image of every use count >= 2. */
enum prototype_usage_grade {
	PROTOTYPE_USAGE_ZERO = 0,
	PROTOTYPE_USAGE_ONE = 1,
	PROTOTYPE_USAGE_MANY = 2
};

struct prototype_usage_entry {
	uint32_t binding_id;
	int grade;
};

/* Entries are sorted by binding identity and omit ZERO. Storage is supplied by
 * the owner because usage belongs to a judgement/occurrence, not to ContextDB. */
struct prototype_usage_vector {
	struct prototype_usage_entry* entries;
	size_t count;
	size_t capacity;
};

/* One column of a quantitative substitution action. A target binding is
 * replaced by a source-context usage vector. */
struct prototype_usage_substitution_column {
	uint32_t target_binding_id;
	const struct prototype_usage_vector* source_usage;
};

int prototype_usage_grade_add(int left, int right, int* p_result);
int prototype_usage_grade_multiply(int left, int right, int* p_result);
int prototype_usage_grade_join(int left, int right, int* p_result);
void prototype_usage_vector_init(
	struct prototype_usage_vector* vector,
	struct prototype_usage_entry* entries,
	size_t capacity
);
int prototype_usage_vector_copy(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* source
);
int prototype_usage_vector_get(
	const struct prototype_usage_vector* vector,
	uint32_t binding_id,
	int* p_grade
);
int prototype_usage_vector_set(
	struct prototype_usage_vector* vector,
	uint32_t binding_id,
	int grade
);
int prototype_usage_vector_add(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* left,
	const struct prototype_usage_vector* right
);
int prototype_usage_vector_join(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* left,
	const struct prototype_usage_vector* right
);
int prototype_usage_vector_scale(
	struct prototype_usage_vector* target,
	int scalar,
	const struct prototype_usage_vector* source
);
int prototype_usage_vector_transform(
	struct prototype_usage_vector* target,
	const struct prototype_usage_vector* source,
	const struct prototype_usage_substitution_column* columns,
	size_t column_count
);
int prototype_usage_vector_equal(
	const struct prototype_usage_vector* left,
	const struct prototype_usage_vector* right
);
int prototype_term_usage_analyze(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	struct prototype_usage_vector* p_usage
);

#endif
