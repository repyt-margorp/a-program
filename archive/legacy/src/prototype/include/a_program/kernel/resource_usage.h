#ifndef A_PROGRAM_PROTOTYPE_KERNEL_RESOURCE_USAGE_H
#define A_PROGRAM_PROTOTYPE_KERNEL_RESOURCE_USAGE_H

#include <stdint.h>

#include "a_program/frontend/typing_resource_usage.h"

struct prototype_term_db;
struct prototype_type_declaration_db;
struct prototype_context_db;
struct prototype_substitution_db;

/* Legacy Core traversal used while the mixed solver remains selected. Usage
 * grades and vectors themselves are owned by Layer T. */
int prototype_term_usage_analyze(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	struct prototype_usage_vector* p_usage
);

int prototype_usage_vector_reindex(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	const struct prototype_usage_vector* source,
	uint32_t substitution_id,
	struct prototype_usage_vector* target
);

#endif
