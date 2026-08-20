#ifndef A_PROGRAM_PROTOTYPE_TYPE_SCHEMA_VIEW_H
#define A_PROGRAM_PROTOTYPE_TYPE_SCHEMA_VIEW_H

#include <stddef.h>
#include <stdint.h>

struct prototype_context_db;
struct prototype_dimension_operator_db;
struct prototype_term_db;
struct prototype_type_constructor_declaration;
struct prototype_type_declaration;
struct prototype_type_declaration_db;
struct prototype_type_semantic_schema_db;

/* A read-only semantic view over one source declaration and an action chain.
 * The chain is ordered from the source outward. It never creates a generated
 * TypeDeclaration. */
struct prototype_type_schema_view {
	uint32_t source_type_id;
	const struct prototype_type_declaration* source_declaration;
	uint32_t source_type_view;
	uint32_t acted_type;
	uint32_t target_dimension;
};

struct prototype_constructor_schema_view {
	uint32_t source_constructor_id;
	const struct prototype_type_constructor_declaration* source_constructor;
	uint32_t source_constructor_term;
	uint32_t acted_constructor_term;
	uint32_t source_classifier;
	uint32_t acted_classifier;
};

int prototype_type_schema_view_query(
	const struct prototype_type_semantic_schema_db* semantic_schema,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t type_term,
	struct prototype_type_schema_view* p_view
);

int prototype_constructor_schema_view_query(
	const struct prototype_type_semantic_schema_db* semantic_schema,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_type_schema_view* type_view,
	uint32_t constructor_ordinal,
	struct prototype_constructor_schema_view* p_view
);

int prototype_constructor_schema_view_action_classifier(
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_type_schema_view* type_view,
	uint32_t constructor_ordinal,
	uint32_t* p_classifier
);

#endif
