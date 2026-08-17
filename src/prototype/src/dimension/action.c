#include "a_program/dimension/action.h"

#include "a_program/core/term.h"
#include "a_program/dimension/face.h"
#include "a_program/dimension/operator.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/type_declaration.h"

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
	if (terms->terms[term_id].tag == PROTOTYPE_TERM_APP) {
		size_t argument_count = 0;
		uint32_t family = term_id;
		while (family < terms->term_count &&
			terms->terms[family].tag == PROTOTYPE_TERM_APP) {
			if (argument_count == SIZE_MAX) {
				return -1;
			}
			argument_count++;
			family = terms->terms[family].as.app.function;
		}
		if (family < terms->term_count &&
			terms->terms[family].tag == PROTOTYPE_TERM_DIMENSION_ACTION) {
			uint32_t family_dimension;
			size_t boundary_count;
			if (prototype_dimension_action_term_dimension(
					terms,
					dimension_operators,
					family,
					&family_dimension
				) != 0 || prototype_dimension_boundary_count(
					family_dimension, &boundary_count
				) != 0) {
				return -1;
			}
			if (argument_count == boundary_count) {
				*p_dimension = family_dimension;
				return 0;
			}
		}
		*p_dimension = 0;
		return 0;
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
) {
	if (!terms || !dimension_operators || !p_family || !p_source ||
		!p_operator_id || !p_target_dimension || !p_argument_count ||
		term_id >= terms->term_count) {
		return -1;
	}
	size_t argument_count = 0;
	uint32_t family = term_id;
	while (family < terms->term_count &&
		terms->terms[family].tag == PROTOTYPE_TERM_APP) {
		if (argument_count == SIZE_MAX) {
			return -1;
		}
		argument_count++;
		family = terms->terms[family].as.app.function;
	}
	uint32_t source;
	uint32_t operator_id;
	uint32_t target_dimension;
	size_t boundary_count;
	if (prototype_term_dimension_action_info(
			terms, family, &source, &operator_id
		) != 0 || prototype_dimension_action_term_dimension(
			terms, dimension_operators, family, &target_dimension
		) != 0 || prototype_dimension_boundary_count(
			target_dimension, &boundary_count
		) != 0 || argument_count != boundary_count) {
		return 1;
	}
	if (arguments && argument_count > argument_capacity) {
		return -1;
	}
	if (arguments) {
		uint32_t cursor = term_id;
		for (size_t i = argument_count; i > 0; --i) {
			arguments[i - 1] = terms->terms[cursor].as.app.argument;
			cursor = terms->terms[cursor].as.app.function;
		}
		if (cursor != family) {
			return -1;
		}
	}
	*p_family = family;
	*p_source = source;
	*p_operator_id = operator_id;
	*p_target_dimension = target_dimension;
	*p_argument_count = argument_count;
	return 0;
}

int prototype_dimension_action_extend_classifier_family(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t classifier,
	uint32_t operator_id,
	uint32_t* p_acted_family
) {
	if (!terms || !dimension_operators || !p_acted_family ||
		classifier >= terms->term_count) {
		return -1;
	}
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(dimension_operators, operator_id);
	uint32_t classifier_dimension;
	if (!operator || prototype_dimension_action_term_dimension(
			terms, dimension_operators, classifier, &classifier_dimension
		) != 0 || operator->source_dimension != classifier_dimension) {
		return -1;
	}
	uint32_t family = classifier;
	if (classifier_dimension != 0) {
		uint32_t source;
		uint32_t family_operator;
		uint32_t target_dimension;
		size_t argument_count;
		if (prototype_dimension_action_family_instance_info(
				terms,
				dimension_operators,
				classifier,
				&family,
				&source,
				&family_operator,
				&target_dimension,
				NULL,
				0,
				&argument_count
			) != 0 || target_dimension != classifier_dimension) {
			return -1;
		}
		(void)source;
		(void)family_operator;
		(void)argument_count;
	}
	return prototype_term_dimension_action(
		terms, dimension_operators, family, operator_id, p_acted_family
	);
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

int prototype_dimension_action_from_zero(
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

static int extension_operator_id(
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_dimension,
	uint32_t* p_operator_id
) {
	if (!dimension_operators || !p_operator_id || source_dimension == UINT32_MAX ||
		(source_dimension != 0 && SIZE_MAX / source_dimension <
			sizeof(struct prototype_dimension_axis_image))) {
		return -1;
	}
	struct prototype_dimension_axis_image* images = source_dimension == 0 ? NULL :
		malloc((size_t)source_dimension * sizeof(*images));
	if (source_dimension != 0 && !images) {
		return -1;
	}
	for (uint32_t axis = 0; axis < source_dimension; ++axis) {
		images[axis] = (struct prototype_dimension_axis_image) {
			.kind = PROTOTYPE_DIMENSION_AXIS_TARGET,
			.target_axis = axis
		};
	}
	int status = prototype_dimension_operator_find(
		dimension_operators,
		source_dimension,
		source_dimension + 1,
		images,
		source_dimension,
		p_operator_id
	);
	free(images);
	return status == 0 ? 0 : -1;
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
	if (intrinsic_dimension == UINT32_MAX || prototype_dimension_action_from_zero(
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
) {
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(dimension_operators, operator_id);
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context
	);
	const struct prototype_context* target = prototype_context_get(
		contexts, target_context
	);
	uint32_t target_dimension;
	size_t face_count;
	if (!terms || !type_declarations || !contexts || !operator || !source ||
		!target || !p_classifier || source_type >= terms->term_count ||
		acted_type >= terms->term_count || universe >= terms->term_count ||
		prototype_dimension_action_term_dimension(
			terms, dimension_operators, acted_type, &target_dimension
		) != 0 || target_dimension != operator->target_dimension ||
		prototype_dimension_face_ordinal_count(
			target_dimension, &face_count
		) != 0 || face_count == 0) {
		return -1;
	}

	uint32_t common = source_context;
	uint32_t source_count = 0;
	uint32_t target_count = 0;
	for (;;) {
		const struct prototype_context* base = prototype_context_get(contexts, common);
		if (!base || source->depth < base->depth || target->depth < base->depth) {
			return -1;
		}
		source_count = source->depth - base->depth;
		target_count = target->depth - base->depth;
		if (source_count == 0 ? target_count == 0 :
			face_count <= UINT32_MAX / source_count &&
			target_count == source_count * face_count) {
			uint32_t cursor = target_context;
			while (prototype_context_get(contexts, cursor)->depth > base->depth) {
				cursor = prototype_context_get(contexts, cursor)->parent;
			}
			if (cursor == common) {
				break;
			}
		}
		if (base->parent == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		common = base->parent;
	}
	uint32_t* source_path = source_count == 0 ? NULL : malloc(
		(size_t)source_count * sizeof(*source_path)
	);
	uint32_t* target_path = target_count == 0 ? NULL : malloc(
		(size_t)target_count * sizeof(*target_path)
	);
	uint32_t* target_values = target_count == 0 ? NULL : calloc(
		target_count, sizeof(*target_values)
	);
	struct prototype_binding_replacement* replacements = source_count == 0 ?
		NULL : calloc(source_count, sizeof(*replacements));
	if ((source_count != 0 && (!source_path || !replacements)) ||
		(target_count != 0 && (!target_path || !target_values))) {
		free(source_path);
		free(target_path);
		free(target_values);
		free(replacements);
		return -1;
	}
	uint32_t parsed_source_count;
	uint32_t parsed_target_count;
	int status = prototype_context_extension_path(
		contexts,
		common,
		source_context,
		source_path,
		source_count,
		&parsed_source_count
	) != 0 || parsed_source_count != source_count ||
		prototype_context_extension_path(
			contexts,
			common,
			target_context,
			target_path,
			target_count,
			&parsed_target_count
		) != 0 || parsed_target_count != target_count ? -1 : 0;
	for (uint32_t i = 0; i < target_count && status == 0; ++i) {
		const struct prototype_context* field = prototype_context_get(
			contexts, target_path[i]
		);
		if (!field || prototype_term_var(
				terms, field->binding_id, &target_values[i]
			) != 0) {
			status = -1;
		}
	}
	for (uint32_t i = 0; i < source_count && status == 0; ++i) {
		const struct prototype_context* source_field = prototype_context_get(
			contexts, source_path[i]
		);
		if (!source_field) {
			status = -1;
			break;
		}
		for (size_t ordinal = 0; ordinal < face_count && status == 0; ++ordinal) {
			uint8_t* digits = target_dimension == 0 ? NULL : malloc(
				target_dimension
			);
			struct prototype_dimension_face face;
			uint32_t intrinsic_dimension;
			uint32_t expected;
			const struct prototype_context* target_field = prototype_context_get(
				contexts, target_path[(size_t)i * face_count + ordinal]
			);
			if ((target_dimension != 0 && !digits) || !target_field ||
				prototype_dimension_face_from_ordinal(
					target_dimension,
					ordinal,
					digits,
					target_dimension,
					&face
				) != 0 || (intrinsic_dimension =
					prototype_dimension_face_intrinsic_dimension(&face)) == UINT32_MAX) {
				free(digits);
				status = -1;
				break;
			}
			if (intrinsic_dimension == 0) {
				for (uint32_t prior = 0; prior < i; ++prior) {
					const struct prototype_context* prior_source =
						prototype_context_get(contexts, source_path[prior]);
					if (!prior_source) {
						status = -1;
						break;
					}
					replacements[prior] =
						(struct prototype_binding_replacement) {
							.binding_id = prior_source->binding_id,
							.replacement = target_values[
								(size_t)prior * face_count + ordinal
							]
						};
				}
				if (status == 0) {
					status = prototype_term_graph_reindex_bindings(
						terms,
						type_declarations,
						prototype_context_classifier_term(source_field),
						replacements,
						i,
						&expected
					);
				}
			} else {
				status = prototype_dimension_action_from_zero(
					terms,
					dimension_operators,
					prototype_context_classifier_term(source_field),
					intrinsic_dimension,
					&expected
				);
				size_t local_boundary_count;
				if (status == 0 && prototype_dimension_face_boundary_count(
						&face, &local_boundary_count
					) != 0) {
					status = -1;
				}
				for (size_t local = 0;
					local < local_boundary_count && status == 0;
					++local) {
					size_t global_ordinal;
					if (prototype_dimension_face_boundary_ordinal(
							&face, local, &global_ordinal
						) != 0 || global_ordinal >= ordinal || prototype_term_app(
							terms,
							expected,
							target_values[
								(size_t)i * face_count + global_ordinal
							],
							&expected
						) != 0) {
						status = -1;
					}
				}
			}
			free(digits);
			if (status == 0 && expected !=
					prototype_context_classifier_term(target_field)) {
				status = -1;
			}
		}
	}
	size_t boundary_count = face_count - 1;
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
		status = -1;
	}
	for (size_t ordinal = 0; ordinal < boundary_count && status == 0; ++ordinal) {
		uint8_t* digits = target_dimension == 0 ? NULL : malloc(target_dimension);
		struct prototype_dimension_face face;
		uint32_t intrinsic_dimension;
		if ((target_dimension != 0 && !digits) ||
			prototype_dimension_face_from_ordinal(
				target_dimension,
				ordinal,
				digits,
				target_dimension,
				&face
			) != 0 || (intrinsic_dimension =
				prototype_dimension_face_intrinsic_dimension(&face)) == UINT32_MAX) {
			free(digits);
			status = -1;
			break;
		}
		if (intrinsic_dimension == 0) {
			for (uint32_t prior = 0; prior < source_count; ++prior) {
				const struct prototype_context* prior_source =
					prototype_context_get(contexts, source_path[prior]);
				if (!prior_source) {
					status = -1;
					break;
				}
				replacements[prior] = (struct prototype_binding_replacement) {
					.binding_id = prior_source->binding_id,
					.replacement = target_values[
						(size_t)prior * face_count + ordinal
					]
				};
			}
			if (status == 0) {
				status = prototype_term_graph_reindex_bindings(
					terms,
					type_declarations,
					source_type,
					replacements,
					source_count,
					&classifiers[ordinal]
				);
			}
		} else {
			status = prototype_dimension_action_from_zero(
				terms,
				dimension_operators,
				source_type,
				intrinsic_dimension,
				&classifiers[ordinal]
			);
			size_t local_boundary_count;
			if (status == 0 && prototype_dimension_face_boundary_count(
					&face, &local_boundary_count
				) != 0) {
				status = -1;
			}
			for (size_t local = 0;
				local < local_boundary_count && status == 0;
				++local) {
				size_t global_ordinal;
				if (prototype_dimension_face_boundary_ordinal(
						&face, local, &global_ordinal
					) != 0 || global_ordinal >= ordinal || prototype_term_app(
						terms,
						classifiers[ordinal],
						variables[global_ordinal],
						&classifiers[ordinal]
					) != 0) {
					status = -1;
				}
			}
		}
		free(digits);
		if (status == 0 && ((bindings[ordinal] = prototype_term_new_binding(
				terms
			)) == PROTOTYPE_INVALID_ID || prototype_term_var(
				terms, bindings[ordinal], &variables[ordinal]
			) != 0)) {
			status = -1;
		}
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
	free(source_path);
	free(target_path);
	free(target_values);
	free(replacements);
	free(bindings);
	free(variables);
	free(classifiers);
	if (status != 0) {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}

int prototype_dimension_action_term_classifier(
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source_term,
	uint32_t source_term_classifier,
	uint32_t acted_type,
	uint32_t target_dimension,
	uint32_t* p_classifier
) {
	if (!terms || !dimension_operators || !p_classifier ||
		source_term >= terms->term_count ||
		source_term_classifier >= terms->term_count ||
		acted_type >= terms->term_count) {
		return -1;
	}
	uint32_t source_classifier;
	uint32_t extension_operator;
	if (prototype_term_dimension_action_info(
			terms, acted_type, &source_classifier, &extension_operator
		) != 0) {
		return -1;
	}
	uint32_t source_family;
	uint32_t family_source;
	uint32_t family_operator_id;
	uint32_t source_dimension;
	size_t source_boundary_count;
	int family_status = prototype_dimension_action_family_instance_info(
		terms,
		dimension_operators,
		source_term_classifier,
		&source_family,
		&family_source,
		&family_operator_id,
		&source_dimension,
		NULL,
		0,
		&source_boundary_count
	);
	if (family_status < 0) {
		return -1;
	}
	if (family_status == 0 && source_dimension != 0) {
		uint32_t expected_extension;
		size_t target_boundary_count;
		if (source_dimension == UINT32_MAX || target_dimension !=
				source_dimension + 1 || source_boundary_count > (SIZE_MAX - 2) / 3 ||
			prototype_dimension_boundary_count(
				target_dimension, &target_boundary_count
			) != 0 || target_boundary_count != source_boundary_count * 3 + 2 ||
			extension_operator_id(
				dimension_operators, source_dimension, &expected_extension
			) != 0 || extension_operator != expected_extension) {
			return -1;
		}
		uint32_t* source_boundaries = calloc(
			source_boundary_count, sizeof(*source_boundaries)
		);
		if (!source_boundaries || prototype_dimension_action_family_instance_info(
				terms,
				dimension_operators,
				source_term_classifier,
				&source_family,
				&family_source,
				&family_operator_id,
				&source_dimension,
				source_boundaries,
				source_boundary_count,
				&source_boundary_count
			) != 0) {
			free(source_boundaries);
			return -1;
		}
		uint32_t classifier;
		int status = prototype_term_dimension_action(
			terms,
			dimension_operators,
			source_family,
			extension_operator,
			&classifier
		);
		for (size_t i = 0; i < source_boundary_count && status == 0; ++i) {
			uint8_t* digits = malloc(source_dimension);
			struct prototype_dimension_face face;
			uint32_t face_operator;
			uint32_t varying_face;
			if (!digits || prototype_dimension_boundary_from_index(
					source_dimension,
					i,
					digits,
					source_dimension,
					&face
				) != 0 || extension_operator_id(
					dimension_operators,
					prototype_dimension_face_intrinsic_dimension(&face),
					&face_operator
				) != 0 || prototype_term_dimension_action(
					terms,
					dimension_operators,
					source_boundaries[i],
					face_operator,
					&varying_face
				) != 0 || prototype_term_app(
					terms, classifier, source_boundaries[i], &classifier
				) != 0 || prototype_term_app(
					terms, classifier, source_boundaries[i], &classifier
				) != 0 || prototype_term_app(
					terms, classifier, varying_face, &classifier
				) != 0) {
				status = -1;
			}
			free(digits);
		}
		if (status == 0 && (prototype_term_app(
				terms, classifier, source_term, &classifier
			) != 0 || prototype_term_app(
				terms, classifier, source_term, &classifier
			) != 0)) {
			status = -1;
		}
		free(source_boundaries);
		if (status != 0) {
			return -1;
		}
		*p_classifier = classifier;
		(void)family_source;
		(void)family_operator_id;
		return 0;
	}
	size_t boundary_count;
	if (prototype_dimension_boundary_count(
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
			) != 0 || prototype_dimension_action_from_zero(
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
