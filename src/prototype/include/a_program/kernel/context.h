#ifndef __PROTOTYPE_CONTEXT_H__
#define __PROTOTYPE_CONTEXT_H__

#include <stddef.h>
#include <stdint.h>

struct prototype_term_db;
struct prototype_type_declaration_db;
struct prototype_term_conversion_result;

/* EXTEND remains a classifier-coherent candidate constructor. These results
 * identify which structural/coherence premise failed; they do not report CwF
 * formation evidence, which belongs to cwf_certificate.h. */
enum prototype_substitution_extend_result {
	PROTOTYPE_SUBSTITUTION_EXTEND_OK = 0,
	PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_ARGUMENT = -1,
	PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_PREFIX = -2,
	PROTOTYPE_SUBSTITUTION_EXTEND_TERM_OUT_OF_RANGE = -3,
	PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_OUT_OF_RANGE = -4,
	PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_TARGET = -5,
	PROTOTYPE_SUBSTITUTION_EXTEND_TARGET_PARENT_MISMATCH = -6,
	PROTOTYPE_SUBSTITUTION_EXTEND_REINDEX_FAILED = -7,
	PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_NORMALIZATION_FAILED = -8,
	PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_MISMATCH = -9,
	PROTOTYPE_SUBSTITUTION_EXTEND_STORAGE_FAILED = -10
};

#define PROTOTYPE_CONTEXT_CAPACITY 8192
#define PROTOTYPE_SUBSTITUTION_CAPACITY 8192
#define PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT 1021
#define PROTOTYPE_REINDEX_CACHE_COUNT 1021
#define PROTOTYPE_CONTEXT_COMPREHENSION_ACTION_CAPACITY 8192

/*
 * The comprehension action is an interned CwF pullback, not a disposable
 * cache entry.  A base substitution sigma : Delta -> Gamma and one source
 * extension Gamma.A determine Delta.A[sigma] and its lifted substitution.
 */
struct prototype_context_comprehension_action {
	uint32_t source_extension;
	uint32_t base_substitution;
	uint32_t target_extension;
	uint32_t lifted_substitution;
	uint32_t target_binding_id;
	uint64_t key_hash;
	uint32_t hash_next;
};

struct prototype_reindex_cache_entry {
	int present;
	uint32_t term;
	uint32_t substitution;
	uint32_t result;
};

/*
 * Contexts are objects of the compiler's syntactic CwF. Entry zero is the
 * empty context; every other entry is an immutable context extension.
 */
enum prototype_context_classifier_ref_kind {
	PROTOTYPE_CONTEXT_CLASSIFIER_REF_INVALID = 0,
	PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM = 1,
	PROTOTYPE_CONTEXT_CLASSIFIER_REF_VARIABLE = 2,
	PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL = 3
};

struct prototype_context_classifier_ref {
	int kind;
	uint32_t term_id;
	uint32_t variable_id;
};

struct prototype_context {
	uint32_t parent;
	uint32_t binding_id;
	struct prototype_context_classifier_ref classifier_ref;
	uint32_t depth;
	uint64_t key_hash;
	uint32_t hash_next;
};

struct prototype_context_db {
	struct prototype_context* contexts;
	size_t context_count;
	size_t context_capacity;
	uint32_t index_heads[PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT];
	uint64_t intern_requests;
	uint64_t intern_hits;
	uint64_t intern_probes;
	struct prototype_context_comprehension_action
		comprehension_actions[PROTOTYPE_CONTEXT_COMPREHENSION_ACTION_CAPACITY];
	size_t comprehension_action_count;
	uint32_t comprehension_action_index_heads[
		PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT
	];
	uint64_t pullback_requests;
	uint64_t pullback_hits;
	uint64_t pullback_probes;
};

enum prototype_substitution_kind {
	PROTOTYPE_SUBSTITUTION_IDENTITY = 1,
	PROTOTYPE_SUBSTITUTION_EMPTY = 2,
	PROTOTYPE_SUBSTITUTION_PROJECTION = 3,
	PROTOTYPE_SUBSTITUTION_EXTEND = 4,
	PROTOTYPE_SUBSTITUTION_COMPOSE = 5
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
	uint64_t key_hash;
	uint32_t hash_next;
};

struct prototype_substitution_db {
	struct prototype_substitution* substitutions;
	size_t substitution_count;
	size_t substitution_capacity;
	uint32_t index_heads[PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT];
	uint64_t intern_requests;
	uint64_t intern_hits;
	uint64_t intern_probes;
	struct prototype_reindex_cache_entry
		reindex_cache[PROTOTYPE_REINDEX_CACHE_COUNT];
	uint64_t reindex_requests;
	uint64_t reindex_hits;
};

void prototype_context_db_init(
	struct prototype_context_db* db,
	struct prototype_context* contexts,
	size_t context_capacity
);
int prototype_context_db_rebuild_index(struct prototype_context_db* db);
uint32_t prototype_context_empty(const struct prototype_context_db* db);
int prototype_context_extend(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	uint32_t* p_context
);
const struct prototype_context* prototype_context_get(
	const struct prototype_context_db* db,
	uint32_t context_id
);
uint32_t prototype_context_classifier_term(
	const struct prototype_context* context
);
uint32_t prototype_context_classifier_variable(
	const struct prototype_context* context
);
int prototype_context_contains_binding(
	const struct prototype_context_db* db,
	uint32_t context_id,
	uint32_t binding_id
);
int prototype_context_find_binding(
	const struct prototype_context_db* db,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t* p_entry_context_id
);
int prototype_context_db_validate(
	const struct prototype_context_db* db,
	const struct prototype_term_db* terms
);
int prototype_context_db_append_relocated(
	struct prototype_context_db* target,
	const struct prototype_context_db* source,
	const uint32_t* term_relocation,
	size_t term_relocation_count,
	const uint32_t* binding_relocation,
	size_t binding_relocation_count,
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
int prototype_context_comprehension_action(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t source_extension,
	uint32_t base_substitution,
	uint32_t* p_target_binding_id,
	uint32_t* p_target_extension,
	uint32_t* p_lifted_substitution
);
int prototype_context_comprehension_actions_validate(
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions
);
int prototype_context_reindex_telescope(
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
int prototype_substitution_db_rebuild_index(
	struct prototype_substitution_db* db
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
int prototype_substitution_projection_path(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t descendant_context,
	uint32_t ancestor_context,
	uint32_t* p_substitution
);
int prototype_substitution_is_projection_path(
	const struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t substitution_id,
	uint32_t descendant_context,
	uint32_t ancestor_context
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
/* Validates the candidate graph's reindexed classifiers. This is not proof of
 * CwF formation; proof-bearing consumers use prototype_certified_substitution_ref. */
int prototype_substitution_db_validate_classifier_coherence(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
);
const char* prototype_substitution_extend_result_name(int result);
int prototype_substitution_db_append_relocated(
	struct prototype_substitution_db* target,
	const struct prototype_substitution_db* source,
	const uint32_t* context_relocation,
	size_t context_relocation_count,
	const uint32_t* term_relocation,
	size_t term_relocation_count,
	uint32_t* relocation,
	size_t relocation_capacity
);
int prototype_substitution_compare_pointwise(
	struct prototype_substitution_db* substitutions,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t left_substitution,
	uint32_t right_substitution,
	int normalization_profile,
	uint64_t step_limit,
	struct prototype_term_conversion_result* p_result
);
int prototype_term_reindex(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t term,
	uint32_t substitution,
	uint32_t* p_reindexed
);
int prototype_substitution_binding_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t target_binding_id,
	uint32_t* p_term
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
