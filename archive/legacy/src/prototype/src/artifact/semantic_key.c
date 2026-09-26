#include "a_program/artifact/semantic_key.h"

#include "a_program/core/term.h"
#include "a_program/dimension/operator.h"
#include "a_program/kernel/type_declaration.h"
#include "a_program/support/symbol.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define ARTIFACT_SEMANTIC_HASH_OFFSET UINT64_C(14695981039346656037)
#define ARTIFACT_SEMANTIC_HASH_PRIME UINT64_C(1099511628211)

struct artifact_semantic_identity_context {
	const struct symbol_table* symbols;
	const struct prototype_type_declaration_db* type_declarations;
	const struct prototype_dimension_operator_db* dimension_operators;
};

static uint64_t artifact_semantic_hash_u32(uint64_t hash, uint32_t value) {
	for (size_t i = 0; i < sizeof(value); ++i) {
		hash ^= (uint8_t)(value >> (i * CHAR_BIT));
		hash *= ARTIFACT_SEMANTIC_HASH_PRIME;
	}
	return hash;
}

static uint64_t artifact_semantic_hash_bytes(
	uint64_t hash,
	const unsigned char* bytes,
	size_t count
) {
	for (size_t i = 0; i < count; ++i) {
		hash ^= bytes[i];
		hash *= ARTIFACT_SEMANTIC_HASH_PRIME;
	}
	return hash;
}

static int artifact_semantic_symbol_token(
	const struct symbol_table* symbols,
	uint32_t local_identity,
	uint64_t* p_token
) {
	if (!symbols || !p_token) {
		return -1;
	}
	uint64_t hash = ARTIFACT_SEMANTIC_HASH_OFFSET;
	if (local_identity == UINT32_MAX) {
		*p_token = artifact_semantic_hash_u32(hash, UINT32_MAX);
		return 0;
	}
	const char* text = symbol_to_string(symbols, (int)local_identity);
	if (!text) {
		return -1;
	}
	size_t length = strlen(text);
	hash = artifact_semantic_hash_u32(hash, (uint32_t)length);
	hash = artifact_semantic_hash_bytes(
		hash, (const unsigned char*)text, length
	);
	*p_token = hash;
	return 0;
}

static int artifact_semantic_representation_token(
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t representation_id,
	uint64_t* p_token
) {
	if (!type_declarations || !p_token ||
		type_declarations->representation_db.cache_dirty ||
		representation_id >=
			type_declarations->representation_db.representation_count) {
		return -1;
	}
	const struct prototype_type_representation_fingerprint* fingerprint =
		&type_declarations->representation_db.representations[
			representation_id
		].fingerprint;
	uint64_t hash = ARTIFACT_SEMANTIC_HASH_OFFSET;
	/* fingerprint.hash is a process-local candidate index because referenced
	 * symbols and binders are hashed by local IDs. Persistent keys use only its
	 * allocation-independent shape summary; exact schema comparison remains the
	 * authority after a key hit. */
	hash = artifact_semantic_hash_u32(hash, fingerprint->node_count);
	hash = artifact_semantic_hash_u32(hash, fingerprint->parameter_count);
	hash = artifact_semantic_hash_u32(hash, fingerprint->index_count);
	hash = artifact_semantic_hash_u32(hash, fingerprint->constructor_count);
	hash = artifact_semantic_hash_u32(hash, fingerprint->bound_binder_count);
	hash = artifact_semantic_hash_u32(hash, fingerprint->free_binder_count);
	hash = artifact_semantic_hash_u32(
		hash, (uint32_t)fingerprint->has_local_universe_reference
	);
	hash = artifact_semantic_hash_u32(
		hash, (uint32_t)fingerprint->has_name_reference
	);
	*p_token = hash;
	return 0;
}

static int artifact_semantic_dimension_token(
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t operator_id,
	uint64_t* p_token
) {
	if (!dimension_operators || !p_token) {
		return -1;
	}
	const struct prototype_dimension_operator* operator =
		prototype_dimension_operator_get(dimension_operators, operator_id);
	if (!operator) {
		return -1;
	}
	const struct prototype_dimension_axis_image* images =
		prototype_dimension_operator_images(dimension_operators, operator_id);
	if (operator->image_count != 0 && !images) {
		return -1;
	}
	uint64_t hash = ARTIFACT_SEMANTIC_HASH_OFFSET;
	hash = artifact_semantic_hash_u32(hash, operator->source_dimension);
	hash = artifact_semantic_hash_u32(hash, operator->target_dimension);
	hash = artifact_semantic_hash_u32(hash, operator->image_count);
	for (uint32_t i = 0; i < operator->image_count; ++i) {
		hash = artifact_semantic_hash_u32(hash, (uint32_t)images[i].kind);
		hash = artifact_semantic_hash_u32(hash, images[i].target_axis);
	}
	*p_token = hash;
	return 0;
}

static int artifact_semantic_identity_resolve(
	const void* context,
	int kind,
	uint32_t local_identity,
	uint64_t* p_stable_token
) {
	const struct artifact_semantic_identity_context* identities = context;
	if (!identities || !p_stable_token) {
		return -1;
	}
	switch (kind) {
		case PROTOTYPE_TERM_STRUCTURAL_IDENTITY_SYMBOL:
			return artifact_semantic_symbol_token(
				identities->symbols, local_identity, p_stable_token
			);
		case PROTOTYPE_TERM_STRUCTURAL_IDENTITY_TYPE_REPRESENTATION:
			return artifact_semantic_representation_token(
				identities->type_declarations,
				local_identity,
				p_stable_token
			);
		case PROTOTYPE_TERM_STRUCTURAL_IDENTITY_DIMENSION_OPERATOR:
			return artifact_semantic_dimension_token(
				identities->dimension_operators,
				local_identity,
				p_stable_token
			);
		default:
			return -1;
	}
}

int prototype_artifact_semantic_key_build(
	const struct symbol_table* symbols,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t term_id,
	struct prototype_artifact_semantic_key* p_key
) {
	if (!symbols || !terms || !type_declarations || !p_key) {
		return -1;
	}
	struct artifact_semantic_identity_context identity_context = {
		.symbols = symbols,
		.type_declarations = type_declarations,
		.dimension_operators = dimension_operators
	};
	struct prototype_term_structural_identity_domain domain = {
		.context = &identity_context,
		.resolve = artifact_semantic_identity_resolve
	};
	struct prototype_term_canonical_key core_key;
	if (prototype_term_structural_key_in_domain(
			terms, term_id, &domain, &core_key
		) != 0) {
		return -1;
	}
	p_key->hash = core_key.hash;
	p_key->node_count = core_key.node_count;
	p_key->bound_binder_count = core_key.bound_binder_count;
	p_key->free_binder_count = core_key.free_binder_count;
	p_key->has_frame_local_reference = core_key.has_frame_local_reference;
	p_key->has_type_local_reference = core_key.has_type_local_reference;
	p_key->has_type_name_reference = core_key.has_type_name_reference;
	p_key->has_type_universe_reference = core_key.has_type_universe_reference;
	return 0;
}

int prototype_artifact_semantic_keys_equal(
	const struct prototype_artifact_semantic_key* left,
	const struct prototype_artifact_semantic_key* right
) {
	return left && right && left->hash == right->hash &&
		left->node_count == right->node_count &&
		left->bound_binder_count == right->bound_binder_count &&
		left->free_binder_count == right->free_binder_count &&
		left->has_frame_local_reference == right->has_frame_local_reference &&
		left->has_type_local_reference == right->has_type_local_reference &&
		left->has_type_name_reference == right->has_type_name_reference &&
		left->has_type_universe_reference == right->has_type_universe_reference;
}
