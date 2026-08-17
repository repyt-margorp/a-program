#include "a_program/kernel/type_schema_view.h"

#include "a_program/core/term.h"
#include "a_program/dimension/operator.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/type_declaration.h"

#include <stdlib.h>
#include <string.h>

static int type_schema_action_chain(
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t acted_type,
	uint32_t* p_source,
	uint32_t* p_target_dimension
) {
	if (!terms || !dimension_operators || !p_source || !p_target_dimension ||
		acted_type >= terms->term_count) {
		return -1;
	}
	if (terms->terms[acted_type].tag != PROTOTYPE_TERM_DIMENSION_ACTION) {
		*p_source = acted_type;
		*p_target_dimension = 0;
		return 0;
	}
	uint32_t source;
	uint32_t operator_id;
	uint32_t source_type;
	uint32_t source_dimension;
	if (prototype_term_dimension_action_info(
			terms, acted_type, &source, &operator_id
		) != 0 || type_schema_action_chain(
			terms,
			dimension_operators,
			source,
			&source_type,
			&source_dimension
		) != 0) {
		return -1;
	}
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(dimension_operators, operator_id);
	if (!operator || operator->source_dimension != source_dimension) {
		return -1;
	}
	*p_source = source_type;
	*p_target_dimension = operator->target_dimension;
	return 0;
}

int prototype_type_schema_view_query(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t type_term,
	struct prototype_type_schema_view* p_view
) {
	if (!type_declarations || !contexts || !terms || !dimension_operators ||
		!p_view || type_term >= terms->term_count) {
		return -1;
	}
	struct prototype_type_schema_view view;
	memset(&view, 0, sizeof(view));
	view.acted_type = type_term;
	if (type_schema_action_chain(
			terms,
			dimension_operators,
			type_term,
			&view.source_type_view,
			&view.target_dimension
		) != 0 || prototype_type_view_declaration_query(
			type_declarations,
			contexts,
			terms,
			view.source_type_view,
			&view.source_type_id,
			&view.source_declaration
		) != 0) {
		return -1;
	}
	*p_view = view;
	return 0;
}

static int source_constructor_classifier(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	const struct prototype_type_schema_view* type_view,
	uint32_t constructor_ordinal,
	uint32_t* p_constructor_id,
	const struct prototype_type_constructor_declaration** p_constructor,
	uint32_t* p_classifier
);

static int act_constructor_schema(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t type_term,
	uint32_t constructor_ordinal,
	struct prototype_constructor_schema_view* p_view
) {
	uint32_t source;
	uint32_t operator_id;
	if (prototype_term_dimension_action_info(
			terms, type_term, &source, &operator_id
		) == 0) {
		if (act_constructor_schema(
				type_declarations,
				contexts,
				terms,
				dimension_operators,
				source,
				constructor_ordinal,
				p_view
			) != 0 || prototype_term_dimension_action(
				terms,
				dimension_operators,
				p_view->acted_constructor_term,
				operator_id,
				&p_view->acted_constructor_term
			) != 0 || prototype_term_dimension_action(
				terms,
				dimension_operators,
				p_view->acted_classifier,
				operator_id,
				&p_view->acted_classifier
			) != 0) {
			return -1;
		}
		return 0;
	}
	struct prototype_type_schema_view source_view;
	memset(&source_view, 0, sizeof(source_view));
	if (prototype_type_schema_view_query(
			type_declarations,
			contexts,
			terms,
			dimension_operators,
			type_term,
			&source_view
		) != 0 || source_view.target_dimension != 0 ||
		source_constructor_classifier(
			type_declarations,
			contexts,
			terms,
			&source_view,
			constructor_ordinal,
			&p_view->source_constructor_id,
			&p_view->source_constructor,
			&p_view->source_classifier
		) != 0 || prototype_term_constructor(
			terms,
			type_term,
			constructor_ordinal,
			&p_view->source_constructor_term
		) != 0) {
		return -1;
	}
	p_view->acted_constructor_term = p_view->source_constructor_term;
	p_view->acted_classifier = p_view->source_classifier;
	return 0;
}

static int source_constructor_classifier(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	const struct prototype_type_schema_view* type_view,
	uint32_t constructor_ordinal,
	uint32_t* p_constructor_id,
	const struct prototype_type_constructor_declaration** p_constructor,
	uint32_t* p_classifier
) {
	if (constructor_ordinal >= type_view->source_declaration->constructor_count ||
		type_view->source_declaration->first_constructor + constructor_ordinal >=
			type_declarations->constructor_count) {
		return -1;
	}
	uint32_t constructor_id =
		type_view->source_declaration->first_constructor + constructor_ordinal;
	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[constructor_id];
	uint32_t classifier;
	if (constructor->owner_type != type_view->source_type_id ||
		constructor->constructor_index != constructor_ordinal ||
		prototype_type_constructor_derive_curried_classifier(
			terms,
			contexts,
			constructor->parameter_context,
			constructor->field_context,
			constructor->result_classifier,
			&classifier
		) != 0) {
		return -1;
	}

	uint32_t type_id;
	uint32_t arguments[16];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms,
			type_view->source_type_view,
			&type_id,
			arguments,
			&argument_count
		) != 0 || type_id != type_view->source_type_id ||
		argument_count < type_view->source_declaration->parameter_count) {
		return -1;
	}
	for (uint32_t i = 0;
		i < type_view->source_declaration->parameter_count;
		++i) {
		if (prototype_term_app(terms, classifier, arguments[i], &classifier) != 0) {
			return -1;
		}
	}
	*p_constructor_id = constructor_id;
	*p_constructor = constructor;
	*p_classifier = classifier;
	return 0;
}

int prototype_constructor_schema_view_query(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_type_schema_view* type_view,
	uint32_t constructor_ordinal,
	struct prototype_constructor_schema_view* p_view
) {
	if (!type_declarations || !contexts || !terms || !dimension_operators ||
		!type_view || !p_view || !type_view->source_declaration ||
		type_view->source_type_id >= type_declarations->type_count ||
		type_view->source_type_view >= terms->term_count ||
		type_view->acted_type >= terms->term_count) {
		return -1;
	}
	const size_t type_count = type_declarations->type_count;
	const size_t constructor_count = type_declarations->constructor_count;
	const uint64_t semantic_revision = type_declarations->semantic_revision;
	const size_t representation_count =
		type_declarations->representation_db.representation_count;

	struct prototype_constructor_schema_view view;
	memset(&view, 0, sizeof(view));
	if (act_constructor_schema(
			type_declarations,
			contexts,
			terms,
			dimension_operators,
			type_view->acted_type,
			constructor_ordinal,
			&view
		) != 0 || view.source_constructor->owner_type !=
		type_view->source_type_id) {
		return -1;
	}
	if (type_declarations->type_count != type_count ||
		type_declarations->constructor_count != constructor_count ||
		type_declarations->semantic_revision != semantic_revision ||
		type_declarations->representation_db.representation_count !=
			representation_count) {
		return -1;
	}
	*p_view = view;
	return 0;
}

int prototype_constructor_schema_view_action_classifier(
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_type_schema_view* type_view,
	uint32_t constructor_ordinal,
	uint32_t* p_classifier
) {
	struct prototype_constructor_schema_view constructor_view;
	uint32_t acted_owner_source;
	uint32_t operator_id;
	if (!type_declarations || !contexts || !terms || !dimension_operators ||
		!type_view || !p_classifier || type_view->target_dimension != 1 ||
		prototype_term_dimension_action_info(
			terms,
			type_view->acted_type,
			&acted_owner_source,
			&operator_id
		) != 0 || acted_owner_source != type_view->source_type_view ||
		prototype_constructor_schema_view_query(
			type_declarations,
			contexts,
			terms,
			dimension_operators,
			type_view,
			constructor_ordinal,
			&constructor_view
		) != 0) {
		return -1;
	}
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(dimension_operators, operator_id);
	if (!operator || operator->source_dimension != 0 ||
		operator->target_dimension != 1) {
		return -1;
	}

	size_t field_count = 0;
	uint32_t cursor = constructor_view.source_classifier;
	while (cursor < terms->term_count &&
		terms->terms[cursor].tag == PROTOTYPE_TERM_PI) {
		uint32_t binder;
		uint32_t body;
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[cursor].as.pi.codomain_family,
				&binder,
				&body
			) != 0 || field_count == SIZE_MAX) {
			return -1;
		}
		field_count++;
		cursor = body;
	}
	if (field_count > SIZE_MAX / (3 * sizeof(uint32_t)) ||
		field_count > SIZE_MAX / sizeof(struct prototype_binding_replacement)) {
		return -1;
	}
	uint32_t* binders = field_count == 0 ? NULL : calloc(
		field_count * 3, sizeof(*binders)
	);
	uint32_t* domains = field_count == 0 ? NULL : calloc(
		field_count * 3, sizeof(*domains)
	);
	uint32_t* left_values = field_count == 0 ? NULL : calloc(
		field_count, sizeof(*left_values)
	);
	uint32_t* right_values = field_count == 0 ? NULL : calloc(
		field_count, sizeof(*right_values)
	);
	struct prototype_binding_replacement* left_replacements =
		field_count == 0 ? NULL : calloc(
			field_count, sizeof(*left_replacements)
		);
	struct prototype_binding_replacement* right_replacements =
		field_count == 0 ? NULL : calloc(
			field_count, sizeof(*right_replacements)
		);
	if (field_count != 0 && (!binders || !domains || !left_values ||
		!right_values || !left_replacements || !right_replacements)) {
		free(binders);
		free(domains);
		free(left_values);
		free(right_values);
		free(left_replacements);
		free(right_replacements);
		return -1;
	}

	int status = 0;
	cursor = constructor_view.source_classifier;
	for (size_t field = 0; field < field_count && status == 0; ++field) {
		uint32_t source_domain = terms->terms[cursor].as.pi.domain;
		uint32_t source_binder;
		uint32_t next;
		uint32_t acted_domain;
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[cursor].as.pi.codomain_family,
				&source_binder,
				&next
			) != 0 || prototype_term_graph_reindex_bindings(
				terms,
				type_declarations,
				source_domain,
				left_replacements,
				field,
				&domains[field * 3]
			) != 0 || prototype_term_graph_reindex_bindings(
				terms,
				type_declarations,
				source_domain,
				right_replacements,
				field,
				&domains[field * 3 + 1]
			) != 0 || prototype_term_dimension_action(
				terms,
				dimension_operators,
				source_domain,
				operator_id,
				&acted_domain
			) != 0) {
			status = -1;
			break;
		}
		for (uint32_t face = 0; face < 3 && status == 0; ++face) {
			binders[field * 3 + face] = prototype_term_new_binding(terms);
			if (binders[field * 3 + face] == PROTOTYPE_INVALID_ID) {
				status = -1;
			}
		}
		if (status != 0 || prototype_term_var(
				terms, binders[field * 3], &left_values[field]
			) != 0 || prototype_term_var(
				terms, binders[field * 3 + 1], &right_values[field]
			) != 0 || prototype_term_app(
				terms,
				acted_domain,
				left_values[field],
				&domains[field * 3 + 2]
			) != 0 || prototype_term_app(
				terms,
				domains[field * 3 + 2],
				right_values[field],
				&domains[field * 3 + 2]
			) != 0) {
			status = -1;
			break;
		}
		left_replacements[field] = (struct prototype_binding_replacement) {
			.binding_id = source_binder,
			.replacement = left_values[field]
		};
		right_replacements[field] = (struct prototype_binding_replacement) {
			.binding_id = source_binder,
			.replacement = right_values[field]
		};
		cursor = next;
	}

	uint32_t left_result = constructor_view.source_constructor_term;
	uint32_t right_result = constructor_view.source_constructor_term;
	uint32_t classifier = type_view->acted_type;
	for (size_t field = 0; field < field_count && status == 0; ++field) {
		if (prototype_term_app(
				terms, left_result, left_values[field], &left_result
			) != 0 || prototype_term_app(
				terms, right_result, right_values[field], &right_result
			) != 0) {
			status = -1;
		}
	}
	if (status == 0 && (prototype_term_app(
			terms, classifier, left_result, &classifier
		) != 0 || prototype_term_app(
			terms, classifier, right_result, &classifier
		) != 0)) {
		status = -1;
	}
	for (size_t i = field_count * 3; i > 0 && status == 0; --i) {
		uint32_t family;
		if (prototype_term_pure_family(
				terms, binders[i - 1], classifier, &family
			) != 0 || prototype_term_pi_family(
				terms, domains[i - 1], family, &classifier
			) != 0) {
			status = -1;
		}
	}
	free(binders);
	free(domains);
	free(left_values);
	free(right_values);
	free(left_replacements);
	free(right_replacements);
	if (status != 0) {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}
