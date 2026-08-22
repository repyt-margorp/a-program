#ifndef A_PROGRAM_PROTOTYPE_ARTIFACT_INTERFACE_H
#define A_PROGRAM_PROTOTYPE_ARTIFACT_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

#include "calculus.h"
#include "a_program/frontend/ast.h"
#include "a_program/graph/compile_metadata.h"
#include "a_program/kernel/context.h"
#include "a_program/kernel/universe.h"
#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"

#define PROTOTYPE_ARTIFACT_FORMAT_VERSION 83
#define PROTOTYPE_ARTIFACT_FUNCTION_GRAPH_ASSOCIATION_CAPACITY 128
#define PROTOTYPE_ARTIFACT_CALCULUS_FINGERPRINT \
	PROTOTYPE_CALCULUS_FINGERPRINT
enum prototype_artifact_export_transparency {
	PROTOTYPE_ARTIFACT_EXPORT_OPAQUE = 1,
	PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT = 2
};

enum prototype_artifact_evidence_reference_kind {
	PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_INVALID = 0,
	PROTOTYPE_ARTIFACT_EVIDENCE_REFERENCE_CLAIM = 1
};

struct prototype_artifact_evidence_reference {
	int kind;
	uint32_t id;
};

struct prototype_artifact_term_export {
	/*
	 * A named typed-operation boundary. local_term is deliberately an erased
	 * core root and may be shared by several exports; the qualified name and
	 * classifier retain the distinct source operation selected by the API.
	 * Nested typed occurrences are compiler-local annotation data and are not
	 * serialized as linkable runtime graph nodes.
	 */
	int namespace_symbol_id;
	int name_symbol_id;
	uint32_t local_term;
	uint32_t classifier;
	/* Source typed occurrence authorizing this export. The erased local_term
	 * alone cannot identify a Claim or a residual solver obligation. */
	uint32_t occurrence;
	/* Exact evidence authorizing this export. Source artifacts use an accepted
	 * Claim; an appended, not-yet-republished graph uses its Proposition. */
	struct prototype_artifact_evidence_reference source_evidence;
	int transparency;
	struct prototype_term_canonical_key canonical_key;
	struct prototype_term_canonical_key classifier_key;
};

struct prototype_artifact_type_export {
	int namespace_symbol_id;
	int name_symbol_id;
	uint32_t local_type_id;
	/* Serialized declaration anchor for the interned core representation. */
	uint32_t core_representation_anchor_type_id;
	/* Graph-level classifier of the unapplied type former. */
	uint32_t formation_classifier;
	struct prototype_type_representation_fingerprint representation_fingerprint;
	uint32_t first_parameter;
	uint32_t parameter_count;
	uint32_t first_constructor_export;
	uint32_t constructor_count;
};

struct prototype_artifact_type_parameter_export {
	uint32_t binding_id;
	int name_symbol_id;
	uint32_t type_expr;
};

struct prototype_artifact_constructor_export {
	uint32_t type_export_index;
	int name_symbol_id;
	uint32_t ordinal;
	uint32_t readback_first_field_type;
	uint32_t readback_field_count;
	uint32_t curried_classifier_cache;
};

struct prototype_artifact_dependency {
	int namespace_symbol_id;
	int name_symbol_id;
};

struct prototype_artifact_identity_root {
	uint32_t source_type_claim_id;
	uint32_t identity_family_has_type_claim_id;
	uint32_t witness_has_type_claim_id;
	int computation_rule;
};

struct prototype_artifact_function_graph_association {
	uint32_t owner_term_export_index;
	uint32_t graph_type_export_index;
	uint32_t result_type_export_index;
	uint32_t certified_runner_term_export_index;
};

struct prototype_artifact_external_term_ref {
	uint32_t term;
	struct prototype_qualified_name name;
};

struct prototype_artifact_resolved_external_term_ref {
	uint32_t term;
	uint32_t term_export_index;
	struct prototype_qualified_name name;
};

struct prototype_artifact_external_type_expr_ref {
	uint32_t type_expr;
	int name_symbol_id;
};

struct prototype_artifact_external_type_former_ref {
	uint32_t type_expr;
	int name_symbol_id;
};

struct prototype_artifact_resolved_external_type_expr_ref {
	uint32_t type_expr;
	uint32_t type_export_index;
	struct prototype_qualified_name name;
	struct prototype_type_representation_fingerprint representation_fingerprint;
};

struct prototype_artifact_resolved_external_type_former_ref {
	uint32_t type_expr;
	uint32_t type_export_index;
	struct prototype_qualified_name name;
	struct prototype_type_representation_fingerprint representation_fingerprint;
};

struct prototype_artifact_resolved_constructor_owner_ref {
	int source_kind;
	uint32_t source;
	uint32_t owner;
	uint32_t ordinal;
	struct prototype_term_canonical_key owner_key;
};

struct prototype_artifact_relocation_table {
	struct prototype_artifact_external_term_ref* external_term_refs;
	size_t external_term_ref_count;
	size_t external_term_ref_capacity;

	struct prototype_artifact_resolved_external_term_ref* resolved_external_term_refs;
	size_t resolved_external_term_ref_count;
	size_t resolved_external_term_ref_capacity;

	struct prototype_artifact_external_type_expr_ref* external_type_expr_refs;
	size_t external_type_expr_ref_count;
	size_t external_type_expr_ref_capacity;

	struct prototype_artifact_resolved_external_type_expr_ref* resolved_external_type_expr_refs;
	size_t resolved_external_type_expr_ref_count;
	size_t resolved_external_type_expr_ref_capacity;

	struct prototype_artifact_external_type_former_ref* external_type_former_refs;
	size_t external_type_former_ref_count;
	size_t external_type_former_ref_capacity;

	struct prototype_artifact_resolved_external_type_former_ref* resolved_external_type_former_refs;
	size_t resolved_external_type_former_ref_count;
	size_t resolved_external_type_former_ref_capacity;

	struct prototype_artifact_resolved_constructor_owner_ref* resolved_constructor_owner_refs;
	size_t resolved_constructor_owner_ref_count;
	size_t resolved_constructor_owner_ref_capacity;

	size_t external_constructor_owner_ref_count;
};

struct prototype_artifact_debug_term_name {
	int name_symbol_id;
	uint32_t term;
	uint32_t classifier;
	uint32_t source_entry_id;
	struct prototype_source_span name_span;
	struct prototype_source_span body_span;
};

struct prototype_artifact_debug_type_name {
	int name_symbol_id;
	uint32_t local_type_id;
	struct prototype_source_span name_span;
	struct prototype_source_span body_span;
};

struct prototype_artifact_debug_constructor_name {
	uint32_t type_export_index;
	int name_symbol_id;
	uint32_t ordinal;
	struct prototype_source_span name_span;
};

struct prototype_artifact_debug_table {
	struct prototype_artifact_debug_term_name* term_names;
	size_t term_name_count;
	size_t term_name_capacity;

	struct prototype_artifact_debug_type_name* type_names;
	size_t type_name_count;
	size_t type_name_capacity;

	struct prototype_artifact_debug_constructor_name* constructor_names;
	size_t constructor_name_count;
	size_t constructor_name_capacity;
};

struct prototype_artifact_interface {
	uint64_t intrinsic_environment_fingerprint;
	int default_integer_host_type;
	struct prototype_artifact_term_export* term_exports;
	size_t term_export_count;
	size_t term_export_capacity;

	struct prototype_artifact_type_export* type_exports;
	size_t type_export_count;
	size_t type_export_capacity;

	struct prototype_artifact_type_parameter_export* type_parameters;
	size_t type_parameter_count;
	size_t type_parameter_capacity;

	struct prototype_artifact_constructor_export* constructor_exports;
	size_t constructor_export_count;
	size_t constructor_export_capacity;

	uint32_t* constructor_field_type_exprs;
	size_t constructor_field_type_expr_count;
	size_t constructor_field_type_expr_capacity;

	struct prototype_type_expr* type_exprs;
	size_t type_expr_count;
	size_t type_expr_capacity;

	struct prototype_artifact_identity_root* identity_roots;
	size_t identity_root_count;
	size_t identity_root_capacity;

	struct prototype_artifact_function_graph_association
		function_graph_associations[
			PROTOTYPE_ARTIFACT_FUNCTION_GRAPH_ASSOCIATION_CAPACITY
		];
	size_t function_graph_association_count;

	struct prototype_artifact_dependency* dependencies;
	size_t dependency_count;
	size_t dependency_capacity;
};

struct prototype_canonical_link_entry {
	uint32_t unit_id;
	uint32_t label_index;
	int name_symbol_id;
	const struct prototype_term_db* terms;
	const struct prototype_type_declaration_db* type_declarations;
	uint32_t local_term;
	uint32_t representative;
	struct prototype_term_canonical_key canonical_key;
};

struct prototype_canonical_link_table {
	struct prototype_canonical_link_entry* entries;
	size_t entry_count;
	size_t entry_capacity;
};

void prototype_canonical_link_table_init(
	struct prototype_canonical_link_table* table,
	struct prototype_canonical_link_entry* entries,
	size_t entry_capacity
);

void prototype_artifact_interface_init(
	struct prototype_artifact_interface* interface,
	struct prototype_artifact_term_export* term_exports,
	size_t term_export_capacity,
	struct prototype_artifact_type_export* type_exports,
	size_t type_export_capacity,
	struct prototype_artifact_type_parameter_export* type_parameters,
	size_t type_parameter_capacity,
	struct prototype_artifact_constructor_export* constructor_exports,
	size_t constructor_export_capacity,
	uint32_t* constructor_field_type_exprs,
	size_t constructor_field_type_expr_capacity,
	struct prototype_type_expr* type_exprs,
	size_t type_expr_capacity,
	struct prototype_artifact_identity_root* identity_roots,
	size_t identity_root_capacity,
	struct prototype_artifact_dependency* dependencies,
	size_t dependency_capacity
);
int prototype_artifact_interface_add_identity_root(
	struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_db* judgement,
	uint32_t source_type_claim_id,
	uint32_t identity_family_has_type_claim_id,
	uint32_t witness_has_type_claim_id,
	int computation_rule,
	uint32_t* p_root_id
);
int prototype_artifact_interface_validate_identity_roots(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_dimension_operator_db* dimension_operators,
	const struct prototype_judgement_db* judgement
);
void prototype_artifact_relocation_table_init(
	struct prototype_artifact_relocation_table* table,
	struct prototype_artifact_external_term_ref* external_term_refs,
	size_t external_term_ref_capacity,
	struct prototype_artifact_resolved_external_term_ref* resolved_external_term_refs,
	size_t resolved_external_term_ref_capacity,
	struct prototype_artifact_external_type_expr_ref* external_type_expr_refs,
	size_t external_type_expr_ref_capacity,
	struct prototype_artifact_resolved_external_type_expr_ref* resolved_external_type_expr_refs,
	size_t resolved_external_type_expr_ref_capacity,
	struct prototype_artifact_external_type_former_ref* external_type_former_refs,
	size_t external_type_former_ref_capacity,
	struct prototype_artifact_resolved_external_type_former_ref* resolved_external_type_former_refs,
	size_t resolved_external_type_former_ref_capacity,
	struct prototype_artifact_resolved_constructor_owner_ref* resolved_constructor_owner_refs,
	size_t resolved_constructor_owner_ref_capacity
);
void prototype_artifact_debug_table_init(
	struct prototype_artifact_debug_table* table,
	struct prototype_artifact_debug_term_name* term_names,
	size_t term_name_capacity,
	struct prototype_artifact_debug_type_name* type_names,
	size_t type_name_capacity,
	struct prototype_artifact_debug_constructor_name* constructor_names,
	size_t constructor_name_capacity
);

int prototype_artifact_interface_build_from_metadata(
	struct prototype_artifact_interface* interface,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_compile_metadata* metadata,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
);

int prototype_artifact_interface_collect_dependencies(
	struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement
);
int prototype_artifact_interface_add_dependency(
	struct prototype_artifact_interface* interface,
	int name_symbol_id
);
int prototype_artifact_interface_add_dependency_in_namespace(
	struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id
);

void prototype_artifact_interface_set_namespace(
	struct prototype_artifact_interface* interface,
	int namespace_symbol_id
);

int prototype_artifact_interface_recompute_keys(
	struct prototype_artifact_interface* interface,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts
);

int prototype_artifact_interface_build_definition_env(
	const struct prototype_artifact_interface* interface,
	struct prototype_term_definition* definitions,
	size_t definition_capacity,
	struct prototype_term_definition_env* p_env
);

uint32_t prototype_artifact_interface_next_universe_var(
	const struct prototype_artifact_interface* interface
);
int prototype_artifact_interface_renumber_universe_vars(
	struct prototype_artifact_interface* interface,
	uint32_t offset
);

int prototype_artifact_interface_find_term_export(
	const struct prototype_artifact_interface* interface,
	int name_symbol_id,
	uint32_t* p_export_id
);
int prototype_artifact_interface_find_term_export_in_namespace(
	const struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id,
	uint32_t* p_export_id
);

int prototype_artifact_interface_find_type_export(
	const struct prototype_artifact_interface* interface,
	int name_symbol_id,
	uint32_t* p_export_id
);
int prototype_artifact_interface_find_type_export_in_namespace(
	const struct prototype_artifact_interface* interface,
	int namespace_symbol_id,
	int name_symbol_id,
	uint32_t* p_export_id
);

int prototype_artifact_interface_find_constructor_export(
	const struct prototype_artifact_interface* interface,
	uint32_t type_export_id,
	int name_symbol_id,
	uint32_t* p_export_id
);

int prototype_artifact_apply_term_relocations(
	struct prototype_artifact_interface* target_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	struct prototype_context_db* target_contexts,
	struct prototype_compile_metadata* target_metadata,
	const struct prototype_artifact_interface* provider_interface
);

int prototype_artifact_apply_type_expr_relocations(
	struct prototype_artifact_interface* target_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	const struct prototype_context_db* target_contexts,
	const struct prototype_artifact_interface* provider_interface
);

struct prototype_artifact_graph_relocation {
	uint32_t* binding_ids;
	size_t binding_id_capacity;
	uint32_t* type_ids;
	size_t type_id_capacity;
	uint32_t* type_expr_ids;
	size_t type_expr_id_capacity;
	uint32_t* parameter_ids;
	size_t parameter_id_capacity;
	uint32_t* constructor_ids;
	size_t constructor_id_capacity;
	uint32_t* field_type_ids;
	size_t field_type_id_capacity;
	uint32_t* proposition_ids;
	size_t proposition_id_capacity;
	uint32_t* claim_ids;
	size_t claim_id_capacity;
	uint32_t* substitution_ids;
	size_t substitution_id_capacity;
	uint32_t* dimension_operator_ids;
	size_t dimension_operator_id_capacity;
};

int prototype_artifact_append_graph(
	struct prototype_artifact_interface* appended_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	struct prototype_context_db* target_contexts,
	struct prototype_substitution_db* target_substitutions,
	struct prototype_dimension_operator_db* target_dimension_operators,
	const struct prototype_artifact_interface* source_interface,
	const struct prototype_term_db* source_terms,
	const struct prototype_type_declaration_db* source_type_declarations,
	const struct prototype_judgement_db* source_judgement,
	const struct prototype_context_db* source_contexts,
	const struct prototype_substitution_db* source_substitutions,
	const struct prototype_dimension_operator_db* source_dimension_operators,
	uint32_t occurrence_offset,
	uint32_t* term_relocation,
	size_t term_relocation_capacity,
	uint32_t* context_relocation,
	size_t context_relocation_capacity,
	struct prototype_artifact_graph_relocation* additional_relocation,
	int canonicalize_link_references
);

int prototype_artifact_align_export_occurrences(
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata
);

int prototype_canonical_link_table_add_metadata(
	struct prototype_canonical_link_table* table,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_compile_metadata* metadata,
	uint32_t unit_id,
	int reject_frame_local_references
);


#endif
