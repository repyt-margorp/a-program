#ifndef __PROTOTYPE_CONTEXT_H__
#define __PROTOTYPE_CONTEXT_H__

#include <stddef.h>
#include <stdint.h>

struct prototype_term_db;
struct prototype_type_declaration_db;

#define PROTOTYPE_CONTEXT_CAPACITY 8192
#define PROTOTYPE_SUBSTITUTION_CAPACITY 8192

/*
 * Contexts are objects of the compiler's syntactic CwF. Entry zero is the
 * empty context; every other entry is an immutable context extension.
 */
struct prototype_context {
	uint32_t parent;
	uint32_t binder_id;
	uint32_t classifier;
	uint32_t classifier_variable;
	uint32_t depth;
};

struct prototype_context_db {
	struct prototype_context* contexts;
	size_t context_count;
	size_t context_capacity;
};

enum prototype_substitution_kind {
	PROTOTYPE_SUBSTITUTION_IDENTITY = 1,
	PROTOTYPE_SUBSTITUTION_EMPTY,
	PROTOTYPE_SUBSTITUTION_PROJECTION,
	PROTOTYPE_SUBSTITUTION_EXTEND,
	PROTOTYPE_SUBSTITUTION_COMPOSE
};

/*
 * A substitution sigma : source -> target assigns a source-context term to
 * every variable declared by target. EXTEND stores <prefix, term>, while
 * COMPOSE stores outer o inner.
 */
struct prototype_substitution {
	int kind;
	uint32_t source_context;
	uint32_t target_context;
	uint32_t first;
	uint32_t second;
	uint32_t term;
	uint32_t term_classifier;
	uint32_t term_proof_id;
};

struct prototype_substitution_db {
	struct prototype_substitution* substitutions;
	size_t substitution_count;
	size_t substitution_capacity;
};

void prototype_context_db_init(
	struct prototype_context_db* db,
	struct prototype_context* contexts,
	size_t context_capacity
);
uint32_t prototype_context_empty(const struct prototype_context_db* db);
int prototype_context_extend(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binder_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	uint32_t* p_context
);
const struct prototype_context* prototype_context_get(
	const struct prototype_context_db* db,
	uint32_t context_id
);
int prototype_context_contains_binder(
	const struct prototype_context_db* db,
	uint32_t context_id,
	uint32_t binder_id
);
int prototype_context_db_validate(
	const struct prototype_context_db* db,
	const struct prototype_term_db* terms
);
int prototype_constructor_telescopes_validate(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms
);
int prototype_constructor_curried_caches_validate(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms
);
int prototype_constructor_curried_caches_rebuild(
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms
);
int prototype_context_db_append_relocated(
	struct prototype_context_db* target,
	const struct prototype_context_db* source,
	uint32_t term_offset,
	uint32_t binder_offset,
	uint32_t* relocation,
	size_t relocation_capacity
);
int prototype_context_extension_path(
	const struct prototype_context_db* contexts,
	uint32_t ancestor,
	uint32_t descendant,
	uint32_t* path,
	uint32_t path_capacity,
	uint32_t* p_count
);
int prototype_context_fresh_reindex_extension(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t base_context,
	uint32_t source_extension,
	uint32_t* binders,
	uint32_t binder_capacity,
	uint32_t* p_binder_count,
	uint32_t* p_target_extension,
	uint32_t* p_substitution
);

void prototype_substitution_db_init(
	struct prototype_substitution_db* db,
	struct prototype_substitution* substitutions,
	size_t substitution_capacity
);
int prototype_substitution_identity(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t context,
	uint32_t* p_substitution
);
int prototype_substitution_empty(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_context,
	uint32_t* p_substitution
);
int prototype_substitution_projection(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t extended_context,
	uint32_t* p_substitution
);
int prototype_substitution_extend(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t target_context,
	uint32_t term,
	uint32_t term_classifier,
	uint32_t term_proof_id,
	uint32_t* p_substitution
);
int prototype_substitution_compose(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t outer_substitution,
	uint32_t inner_substitution,
	uint32_t* p_substitution
);
const struct prototype_substitution* prototype_substitution_get(
	const struct prototype_substitution_db* db,
	uint32_t substitution_id
);
int prototype_substitution_db_validate(
	const struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms
);
int prototype_term_reindex(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t term,
	uint32_t substitution,
	uint32_t* p_reindexed
);
int prototype_context_instantiate_pure_family(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t domain,
	uint32_t family,
	uint32_t argument,
	uint32_t argument_classifier,
	uint32_t argument_proof_id,
	uint32_t* p_result
);
int prototype_context_substitution_from_terms(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t source_context,
	uint32_t target_context,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_substitution
);
int prototype_context_telescope_entry_classifier(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t telescope_base,
	uint32_t telescope_end,
	const uint32_t* previous_terms,
	uint32_t previous_term_count,
	uint32_t entry_index,
	uint32_t* p_classifier
);

#endif
