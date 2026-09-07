#ifndef A_PROGRAM_PROTOTYPE_KERNEL_CLASSIFIER_H
#define A_PROGRAM_PROTOTYPE_KERNEL_CLASSIFIER_H

#include <stdint.h>

struct prototype_term_db;

/* Layer T classification of an occurrence or classifier expression. These
 * values are not properties of an untyped Core Term. */
enum prototype_classifier_category {
	PROTOTYPE_CLASSIFIER_CATEGORY_INVALID = 0,
	PROTOTYPE_CLASSIFIER_CATEGORY_VALUE = 1,
	PROTOTYPE_CLASSIFIER_CATEGORY_COMPUTATION = 2,
	PROTOTYPE_CLASSIFIER_CATEGORY_TYPE = 3
};

enum prototype_computation_kind {
	PROTOTYPE_COMPUTATION_KIND_INVALID = 0,
	PROTOTYPE_COMPUTATION_KIND_RETURNING = 1,
	PROTOTYPE_COMPUTATION_KIND_FUNCTION = 2,
	PROTOTYPE_COMPUTATION_KIND_HANDLER = 3
};

struct prototype_classifier_view {
	int category;
	int computation_kind;
	int totality;
	uint32_t effect_row;
	uint32_t result;
};

enum prototype_effect_row_purity {
	PROTOTYPE_EFFECT_ROW_PURITY_INVALID = 0,
	PROTOTYPE_EFFECT_ROW_PURITY_PURE = 1,
	PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL = 2,
	PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED = 3
};

int prototype_classifier_totality_join(int left, int right);
int prototype_classifier_effect_row_purity(
	const struct prototype_term_db* terms,
	uint32_t row
);
int prototype_classifier_computation_type_is_pure_total(
	const struct prototype_term_db* terms,
	uint32_t computation_type
);

#endif
