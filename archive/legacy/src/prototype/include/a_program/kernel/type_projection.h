#ifndef A_PROGRAM_PROTOTYPE_TYPE_PROJECTION_H
#define A_PROGRAM_PROTOTYPE_TYPE_PROJECTION_H

#include <stdint.h>

struct prototype_term_db;
struct prototype_type_declaration_db;
struct prototype_type_semantic_schema_db;

int prototype_type_projection_instance_info(
	const struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* semantic_schema,
	uint32_t instance,
	uint32_t* p_type_id,
	uint32_t* arguments,
	uint32_t* p_argument_count
);

/* Build the nominal source spine used while a declaration telescope is open.
 * It contains no erased TYPE_FORMER and therefore needs no provisional
 * representation identity. */
int prototype_type_projection_source_instance_make(
	struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* semantic_schema,
	uint32_t type_id,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_instance
);

/* Extend only an open declaration's nominal source spine. This operation is
 * owned by Layer T: Core sees the resulting ordinary APP node and does not
 * inspect declaration state. */
int prototype_type_projection_source_instance_extend(
	struct prototype_term_db* terms,
	const struct prototype_type_semantic_schema_db* semantic_schema,
	uint32_t instance,
	uint32_t argument,
	uint32_t* p_result
);

/* Project a closed type declaration into its nominal source view and erased
 * calculation representation. The declaration must already have a stable
 * representation assignment. */
int prototype_type_projection_instance_make(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_instance
);

int prototype_type_projection_instance_extend(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t instance,
	uint32_t argument,
	uint32_t* p_result
);

int prototype_type_projection_instance_is_saturated(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t instance
);

/* Recover the parameter-only family view from any nominal instance that has
 * supplied all declaration parameters. Index arguments are deliberately
 * discarded: constructor schema selection is nominal at the family boundary,
 * while the constructor result telescope reconstructs the indices. */
int prototype_type_projection_parameter_instance(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t instance,
	uint32_t* p_parameter_instance
);

/* Recover the nominal family selected by a constructor classifier when the
 * constructor's erased owner no longer carries a TYPE_VIEW. Codomain/result
 * positions are searched before domains so a same-shaped field type cannot
 * replace the constructor result family. */
int prototype_type_projection_parameter_instance_for_owner_shape(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t structural_owner,
	uint32_t classifier,
	uint32_t* p_parameter_instance
);

/* Close nominal source spines anywhere in a classifier graph. Existing
 * TYPE_VIEW nodes remain nominal atoms. This Layer T operation is the sole
 * recursive bridge from declaration identity to the Core graph consumed by
 * conversion; Core never reads TypeDeclarationDB. */
int prototype_type_projection_classifier_graph(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t* p_projected
);

int prototype_type_projection_classifier_graph_for_owner(
	struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t owner,
	uint32_t* p_projected
);

#endif
