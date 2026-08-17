#include "a_program/dimension/action.h"

#include "a_program/core/term.h"
#include "a_program/dimension/face.h"
#include "a_program/dimension/operator.h"

#include <stdint.h>
#include <stdlib.h>

int prototype_dimension_action_term_dimension(
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t term_id,
	uint32_t* p_dimension
) {
	if (!terms || !dimension_operators || !p_dimension ||
		term_id >= terms->term_count) {
		return -1;
	}
	if (terms->terms[term_id].tag != PROTOTYPE_TERM_DIMENSION_ACTION) {
		*p_dimension = 0;
		return 0;
	}
	uint32_t source;
	uint32_t operator_id;
	uint32_t source_dimension;
	if (prototype_term_dimension_action_info(
			terms, term_id, &source, &operator_id
		) != 0 || prototype_dimension_action_term_dimension(
			terms, dimension_operators, source, &source_dimension
		) != 0) {
		return -1;
	}
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(dimension_operators, operator_id);
	if (!operator || operator->source_dimension != source_dimension) {
		return -1;
	}
	*p_dimension = operator->target_dimension;
	return 0;
}

int prototype_dimension_action_prepare_face_operators(
	struct prototype_dimension_operator_db* dimension_operators,
	uint32_t target_dimension
) {
	if (!dimension_operators) {
		return -1;
	}
	for (uint32_t dimension = 1; dimension <= target_dimension; ++dimension) {
		uint32_t operator_id;
		if (prototype_dimension_operator_intern(
				dimension_operators, 0, dimension, NULL, 0, &operator_id
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int action_from_dimension_zero(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source,
	uint32_t target_dimension,
	uint32_t* p_action
) {
	if (target_dimension == 0) {
		*p_action = source;
		return 0;
	}
	uint32_t operator_id;
	return prototype_dimension_operator_find(
			dimension_operators, 0, target_dimension, NULL, 0, &operator_id
		) != 0 ? -1 : prototype_term_dimension_action(
			terms, dimension_operators, source, operator_id, p_action
	);
}

static int face_type(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_type,
	const struct prototype_dimension_face* face,
	const uint32_t* face_variables,
	uint32_t* p_type
) {
	uint32_t intrinsic_dimension =
		prototype_dimension_face_intrinsic_dimension(face);
	if (intrinsic_dimension == UINT32_MAX || action_from_dimension_zero(
			terms,
			dimension_operators,
			source_type,
			intrinsic_dimension,
			p_type
		) != 0) {
		return -1;
	}
	size_t boundary_count;
	if (prototype_dimension_face_boundary_count(
			face, &boundary_count
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < boundary_count; ++i) {
		size_t global_ordinal;
		if (prototype_dimension_face_boundary_ordinal(
				face, i, &global_ordinal
			) != 0 || prototype_term_app(
				terms, *p_type, face_variables[global_ordinal], p_type
			) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_dimension_action_type_classifier(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_type,
	uint32_t acted_type,
	uint32_t universe,
	uint32_t target_dimension,
	uint32_t* p_classifier
) {
	size_t face_count;
	if (!terms || !dimension_operators || !p_classifier ||
		source_type >= terms->term_count || acted_type >= terms->term_count ||
		universe >= terms->term_count || prototype_dimension_face_ordinal_count(
			target_dimension, &face_count
		) != 0 || face_count == 0) {
		return -1;
	}
	size_t boundary_count = face_count - 1;
	if (boundary_count > SIZE_MAX / sizeof(uint32_t)) {
		return -1;
	}
	uint32_t* bindings = boundary_count == 0 ? NULL : calloc(
		boundary_count, sizeof(*bindings)
	);
	uint32_t* variables = boundary_count == 0 ? NULL : calloc(
		boundary_count, sizeof(*variables)
	);
	uint32_t* classifiers = boundary_count == 0 ? NULL : calloc(
		boundary_count, sizeof(*classifiers)
	);
	if (boundary_count != 0 && (!bindings || !variables || !classifiers)) {
		free(bindings);
		free(variables);
		free(classifiers);
		return -1;
	}
	int status = 0;
	for (size_t ordinal = 0; ordinal < boundary_count && status == 0; ++ordinal) {
		uint8_t* digits = target_dimension == 0 ? NULL : malloc(target_dimension);
		struct prototype_dimension_face face;
		if (target_dimension != 0 && !digits) {
			status = -1;
			break;
		}
		if (prototype_dimension_face_from_ordinal(
				target_dimension,
				ordinal,
				digits,
				target_dimension,
				&face
			) != 0 || face_type(
				terms,
				dimension_operators,
				source_type,
				&face,
				variables,
				&classifiers[ordinal]
			) != 0 || (bindings[ordinal] = prototype_term_new_binding(
				terms
			)) == PROTOTYPE_INVALID_ID || prototype_term_var(
				terms, bindings[ordinal], &variables[ordinal]
			) != 0) {
			status = -1;
		}
		free(digits);
	}
	uint32_t classifier = universe;
	for (size_t i = boundary_count; i > 0 && status == 0; --i) {
		uint32_t family;
		if (prototype_term_pure_family(
				terms, bindings[i - 1], classifier, &family
			) != 0 || prototype_term_pi_family(
				terms, classifiers[i - 1], family, &classifier
			) != 0) {
			status = -1;
		}
	}
	free(bindings);
	free(variables);
	free(classifiers);
	if (status != 0) {
		return -1;
	}
	(void)acted_type;
	*p_classifier = classifier;
	return 0;
}

int prototype_dimension_action_term_classifier(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_term,
	uint32_t acted_type,
	uint32_t target_dimension,
	uint32_t* p_classifier
) {
	size_t boundary_count;
	if (!terms || !dimension_operators || !p_classifier ||
		source_term >= terms->term_count || acted_type >= terms->term_count ||
		prototype_dimension_boundary_count(
			target_dimension, &boundary_count
		) != 0) {
		return -1;
	}
	uint32_t classifier = acted_type;
	for (size_t i = 0; i < boundary_count; ++i) {
		uint8_t* digits = target_dimension == 0 ? NULL : malloc(target_dimension);
		struct prototype_dimension_face face;
		uint32_t face_term;
		if (target_dimension != 0 && !digits) {
			return -1;
		}
		if (prototype_dimension_boundary_from_index(
				target_dimension,
				i,
				digits,
				target_dimension,
				&face
			) != 0 || action_from_dimension_zero(
				terms,
				dimension_operators,
				source_term,
				prototype_dimension_face_intrinsic_dimension(&face),
				&face_term
			) != 0 || prototype_term_app(
				terms, classifier, face_term, &classifier
			) != 0) {
			free(digits);
			return -1;
		}
		free(digits);
	}
	*p_classifier = classifier;
	return 0;
}
