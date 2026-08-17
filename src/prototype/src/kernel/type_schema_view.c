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
