#ifndef A_PROGRAM_PROTOTYPE_DIMENSION_ACTION_H
#define A_PROGRAM_PROTOTYPE_DIMENSION_ACTION_H

#include <stddef.h>
#include <stdint.h>

struct prototype_dimension_operator_db;
struct prototype_context_db;
struct prototype_term_db;
struct prototype_type_declaration_db;

int prototype_dimension_action_term_dimension(
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t term_id,
	uint32_t* p_dimension
);

/* Decompose a fully boundary-applied acted family. Arguments are returned in
 * canonical APP order. Pass NULL arguments to query only the count. A
 * non-action or partial/over-applied spine returns 1. */
int prototype_dimension_action_family_instance_info(
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t term_id,
	uint32_t* p_family,
	uint32_t* p_source,
	uint32_t* p_operator_id,
	uint32_t* p_target_dimension,
	uint32_t* arguments,
	size_t argument_capacity,
	size_t* p_argument_count
);

int prototype_dimension_action_prepare_face_operators(
	struct prototype_dimension_operator_db* dimension_operators,
	uint32_t target_dimension
);

int prototype_dimension_action_from_zero(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source,
	uint32_t target_dimension,
	uint32_t* p_action
);

/* Extend the canonical family underlying a classifier. A fully boundary-
 * applied acted family is extended at its family head; an ordinary
 * zero-dimensional classifier is acted directly. */
int prototype_dimension_action_extend_classifier_family(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t classifier,
	uint32_t operator_id,
	uint32_t* p_acted_family
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

/* Build the acted classifier of an open type under a materialized Context
 * action. The source Context and target Context may differ; their extensions
 * are checked against the one-dimensional face telescope before the
 * classifier is returned. */
int prototype_dimension_action_context_type_classifier(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_context,
	uint32_t target_context,
	uint32_t source_type,
	uint32_t acted_type,
	uint32_t universe,
	uint32_t operator_id,
	uint32_t* p_classifier
);

/* Build the center type of an acted ordinary term by applying the acted type
 * family to the corresponding restrictions of the source term. */
int prototype_dimension_action_term_classifier(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_term,
	uint32_t source_classifier,
	uint32_t acted_type,
	uint32_t target_dimension,
	uint32_t* p_classifier
);

#endif
