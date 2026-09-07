#ifndef A_PROGRAM_PROTOTYPE_ARTIFACT_SEMANTIC_KEY_H
#define A_PROGRAM_PROTOTYPE_ARTIFACT_SEMANTIC_KEY_H

#include <stdint.h>

struct prototype_dimension_operator_db;
struct prototype_term_db;
struct prototype_type_declaration_db;
struct symbol_table;

/*
 * Persistent candidate-selection key for an artifact term. Unlike the Core
 * canonical key, its hash is independent of SymbolId, representation ID, and
 * dimension-operator ID allocation order. Equality still requires an exact
 * semantic comparison after a key hit.
 */
struct prototype_artifact_semantic_key {
	uint64_t hash;
	uint32_t node_count;
	uint32_t bound_binder_count;
	uint32_t free_binder_count;
	int has_frame_local_reference;
	int has_type_local_reference;
	int has_type_name_reference;
	int has_type_universe_reference;
};

int prototype_artifact_semantic_key_build(
	const struct symbol_table* symbols,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t term_id,
	struct prototype_artifact_semantic_key* p_key
);

int prototype_artifact_semantic_keys_equal(
	const struct prototype_artifact_semantic_key* left,
	const struct prototype_artifact_semantic_key* right
);

#endif
