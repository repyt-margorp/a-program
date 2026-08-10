#ifndef A_PROGRAM_PROTOTYPE_ARTIFACT_GRAPH_INTERNAL_H
#define A_PROGRAM_PROTOTYPE_ARTIFACT_GRAPH_INTERNAL_H

#include "a_program/artifact/interface.h"

struct artifact_append_order {
	const uint32_t* terms;
	size_t term_count;
	const uint32_t* types;
	size_t type_count;
	const uint32_t* type_exprs;
	size_t type_expr_count;
	const uint32_t* fields;
	size_t field_count;
	const uint32_t* parameters;
	size_t parameter_count;
	const uint32_t* constructors;
	size_t constructor_count;
	const uint32_t* propositions;
	size_t proposition_count;
	const uint32_t* claims;
	size_t claim_count;
	const uint32_t* derivations;
	size_t derivation_count;
	const uint32_t* substitutions;
	size_t substitution_count;
};

static inline int canonical_keys_equal(
	const struct prototype_term_canonical_key* left,
	const struct prototype_term_canonical_key* right
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

static inline int canonical_key_is_cross_artifact_linkable(
	const struct prototype_term_canonical_key* key
) {
	return key && !key->has_type_local_reference &&
		!key->has_frame_local_reference;
}

static inline struct prototype_qualified_name qualified_name_make(
	int namespace_symbol_id,
	int name_symbol_id
) {
	struct prototype_qualified_name name;
	name.namespace_symbol_id = namespace_symbol_id;
	name.name_symbol_id = name_symbol_id;
	return name;
}

static inline int artifact_interface_exports_term_name(
	const struct prototype_artifact_interface* interface,
	struct prototype_qualified_name name
) {
	if (!interface || name.name_symbol_id < 0) {
		return 0;
	}
	for (size_t i = 0; i < interface->term_export_count; ++i) {
		if (interface->term_exports[i].namespace_symbol_id ==
				name.namespace_symbol_id &&
			interface->term_exports[i].name_symbol_id == name.name_symbol_id) {
			return 1;
		}
	}
	return 0;
}

static inline int artifact_type_present(
	const struct prototype_type_declaration* type
) {
	return type && type->type_index != PROTOTYPE_INVALID_ID;
}

static inline int artifact_parameter_present(
	const struct prototype_type_parameter_declaration* parameter
) {
	return parameter && parameter->binding_id != PROTOTYPE_INVALID_ID;
}

static inline int artifact_interface_parameter_present(
	const struct prototype_artifact_type_parameter_export* parameter
) {
	return parameter && parameter->binding_id != PROTOTYPE_INVALID_ID;
}

static inline int artifact_constructor_present(
	const struct prototype_type_constructor_declaration* constructor
) {
	return constructor && constructor->owner_type != PROTOTYPE_INVALID_ID;
}

static inline int artifact_field_type_present(const uint32_t* field_type) {
	return field_type && *field_type != PROTOTYPE_INVALID_ID;
}

static inline int artifact_type_expr_present(
	const struct prototype_type_expr* expr
) {
	return expr && expr->tag != 0;
}

static inline int artifact_term_present(const struct prototype_term* term) {
	return term && term->tag != 0;
}

static inline int artifact_case_present(
	const struct prototype_match_case* match_case
) {
	return match_case && match_case->body != PROTOTYPE_INVALID_ID;
}

static inline int artifact_case_binder_present(
	const struct prototype_case_binder* binder
) {
	return binder && binder->binding_id != PROTOTYPE_INVALID_ID;
}

static inline int artifact_frame_present(const struct prototype_ih_scope* frame) {
	return frame && frame->match_term != PROTOTYPE_INVALID_ID;
}

static inline int artifact_candidate_claim_present(
	const struct prototype_judgement_proposition* relation
) {
	return relation && relation->kind != PROTOTYPE_JUDGEMENT_KIND_UNKNOWN;
}

static inline int artifact_candidate_derivation_present(
	const struct prototype_judgement_derivation_candidate* proof
) {
	return proof && proof->proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID;
}

static inline int artifact_universe_node_present(
	const struct prototype_universe_node* node
) {
	return node && node->tag != 0;
}

static inline int artifact_universe_edge_present(
	const struct prototype_universe_edge* edge
) {
	return edge && edge->tag != 0;
}

static inline uint32_t artifact_relocate_optional_id(
	uint32_t id,
	const uint32_t* relocation,
	size_t relocation_count
) {
	if (id == PROTOTYPE_INVALID_ID || !relocation || id >= relocation_count) {
		return PROTOTYPE_INVALID_ID;
	}
	return relocation[id];
}

int prototype_internal_artifact_find_existing_term_by_canonical_key(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t old_term_count,
	const struct prototype_term_canonical_key* key,
	uint32_t appended_term,
	uint32_t* p_term_id
);

int prototype_internal_artifact_append_graph_ordered(
	struct prototype_artifact_interface* appended_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	struct prototype_context_db* target_contexts,
	struct prototype_substitution_db* target_substitutions,
	const struct prototype_artifact_interface* source_interface,
	const struct prototype_term_db* source_terms,
	const struct prototype_type_declaration_db* source_type_declarations,
	const struct prototype_judgement_db* source_judgement,
	const struct prototype_context_db* source_contexts,
	const struct prototype_substitution_db* source_substitutions,
	uint32_t operation_offset,
	uint32_t* term_relocation,
	size_t term_relocation_capacity,
	uint32_t* context_relocation,
	size_t context_relocation_capacity,
	struct prototype_artifact_graph_relocation* additional_relocation,
	int canonicalize_link_references,
	const struct artifact_append_order* order
);

void prototype_internal_sync_artifact_universe_level_counters(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
);

#endif
