#include "a_program/kernel/type_schema_view.h"

#include "a_program/core/term.h"
#include "a_program/dimension/operator.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/type_declaration.h"

#include <string.h>

static int type_schema_action_chain(
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t acted_type,
	uint32_t* p_source,
	uint32_t* operator_ids,
	size_t* p_operator_count,
	uint32_t* p_target_dimension
) {
	uint32_t reversed[PROTOTYPE_TYPE_SCHEMA_ACTION_CAPACITY];
	size_t count = 0;
	uint32_t current = acted_type;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_DIMENSION_ACTION) {
		uint32_t source;
		uint32_t operator_id;
		if (count >= PROTOTYPE_TYPE_SCHEMA_ACTION_CAPACITY ||
			prototype_term_dimension_action_info(
				terms, current, &source, &operator_id
			) != 0 || !prototype_dimension_operator_get(
				dimension_operators, operator_id
			)) {
			return -1;
		}
		reversed[count++] = operator_id;
		current = source;
	}

	uint32_t dimension = 0;
	for (size_t i = 0; i < count; ++i) {
		uint32_t operator_id = reversed[count - i - 1];
		const struct prototype_dimension_operator* operator =
			prototype_dimension_operator_get(dimension_operators, operator_id);
		if (!operator || operator->source_dimension != dimension) {
			return -1;
		}
		operator_ids[i] = operator_id;
		dimension = operator->target_dimension;
	}
	*p_source = current;
	*p_operator_count = count;
	*p_target_dimension = dimension;
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
			view.operator_ids,
			&view.operator_count,
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
	if (source_constructor_classifier(
			type_declarations,
			contexts,
			terms,
			type_view,
			constructor_ordinal,
			&view.source_constructor_id,
			&view.source_constructor,
			&view.source_classifier
		) != 0 || prototype_term_constructor(
			terms,
			type_view->source_type_view,
			constructor_ordinal,
			&view.source_constructor_term
		) != 0) {
		return -1;
	}
	view.acted_constructor_term = view.source_constructor_term;
	view.acted_classifier = view.source_classifier;
	for (size_t i = 0; i < type_view->operator_count; ++i) {
		if (prototype_term_dimension_action(
				terms,
				dimension_operators,
				view.acted_constructor_term,
				type_view->operator_ids[i],
				&view.acted_constructor_term
			) != 0 || prototype_term_dimension_action(
				terms,
				dimension_operators,
				view.acted_classifier,
				type_view->operator_ids[i],
				&view.acted_classifier
			) != 0) {
			return -1;
		}
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
