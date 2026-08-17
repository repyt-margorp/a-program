#ifndef A_PROGRAM_PROTOTYPE_DIMENSION_ACTION_H
#define A_PROGRAM_PROTOTYPE_DIMENSION_ACTION_H

#include <stdint.h>

struct prototype_dimension_operator_db;
struct prototype_term_db;

int prototype_dimension_action_term_dimension(
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t term_id,
	uint32_t* p_dimension
);

int prototype_dimension_action_prepare_face_operators(
	struct prototype_dimension_operator_db* dimension_operators,
	uint32_t target_dimension
);

/* Build the dependent boundary telescope classifying an acted type family.
 * Every explicit face remains an ordinary Pi binder in canonical face order. */
int prototype_dimension_action_type_classifier(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_type,
	uint32_t acted_type,
	uint32_t universe,
	uint32_t target_dimension,
	uint32_t* p_classifier
);

/* Build the center type of an acted ordinary term by applying the acted type
 * family to the corresponding restrictions of the source term. */
int prototype_dimension_action_term_classifier(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_term,
	uint32_t acted_type,
	uint32_t target_dimension,
	uint32_t* p_classifier
);

#endif
