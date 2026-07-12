#ifndef __PROTOTYPE_AST_H__
#define __PROTOTYPE_AST_H__

#include <stddef.h>
#include <stdint.h>

#include "term.h"
#include "judgement.h"
#include "type_declaration.h"
#include "universe.h"

enum prototype_ast_tag {
	PROTOTYPE_AST_VAR = 1,
	PROTOTYPE_AST_NAME,
	PROTOTYPE_AST_NAME_IN_NAMESPACE,
	PROTOTYPE_AST_NAME_IN_AST_NAMESPACE,
	PROTOTYPE_AST_APP,
	PROTOTYPE_AST_LAMBDA,
	PROTOTYPE_AST_MATCH,
	PROTOTYPE_AST_TYPE_LITERAL,
	PROTOTYPE_AST_TYPE_FORMATION,
	PROTOTYPE_AST_INDUCTION_HYPOTHESIS,
	PROTOTYPE_AST_TEXT_LITERAL,
	PROTOTYPE_AST_INT_LITERAL,
	PROTOTYPE_AST_INTRINSIC_NAME,
	PROTOTYPE_AST_ASCRIPTION
};

enum prototype_ast_intrinsic_id {
	PROTOTYPE_AST_INTRINSIC_UNKNOWN = 0,
	PROTOTYPE_AST_INTRINSIC_HOST_TYPE,
	PROTOTYPE_AST_INTRINSIC_HOST_ORACLE
};

enum prototype_ast_type_expr_tag {
	PROTOTYPE_AST_TYPE_EXPR_UNIVERSE = 1,
	PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR,
	PROTOTYPE_AST_TYPE_EXPR_SELF,
	PROTOTYPE_AST_TYPE_EXPR_VAR,
	PROTOTYPE_AST_TYPE_EXPR_NAME,
	PROTOTYPE_AST_TYPE_EXPR_APP,
	PROTOTYPE_AST_TYPE_EXPR_ARROW,
	PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE
};

struct prototype_source_span {
	unsigned line;
	unsigned column;
};

struct prototype_ast_type_expr {
	int tag;
	struct prototype_source_span span;
	union {
		struct {
			uint32_t level;
		} universe;
		struct {
			uint32_t level_var;
		} universe_var;
		struct {
			uint32_t ast_binder_id;
			int symbol_id;
		} var;
		struct {
			int symbol_id;
		} name;
		struct {
			uint32_t function;
			uint32_t argument;
		} app;
		struct {
			uint32_t domain;
			uint32_t codomain;
		} arrow;
		struct {
			int host_type_id;
		} host_type;
	} as;
};

struct prototype_ast_node {
	int tag;
	struct prototype_source_span span;
	union {
		struct {
			uint32_t ast_binder_id;
			int symbol_id;
		} var;
		struct {
			int symbol_id;
		} name;
		struct {
			int namespace_symbol_id;
			int symbol_id;
		} name_in_namespace;
		struct {
			uint32_t namespace_ast;
			int symbol_id;
		} name_in_ast_namespace;
		struct {
			uint32_t function;
			uint32_t argument;
		} app;
		struct {
			uint32_t ast_binder_id;
			int binder_symbol_id;
			uint32_t binder_type;
			uint32_t body;
		} lambda;
		struct {
			uint32_t scrutinee;
			uint32_t first_case;
			uint32_t case_count;
		} match;
		struct {
			uint32_t ast_type_def_id;
		} type_literal;
		struct {
			uint32_t ast_type_def_id;
		} type_formation;
		struct {
			uint32_t ast_binder_id;
			int symbol_id;
		} induction_hypothesis;
		struct {
			int text_symbol_id;
		} text_literal;
		struct {
			int64_t value;
		} int_literal;
		struct {
			int namespace_symbol_id;
			int symbol_id;
			int type_symbol_id;
			int intrinsic_id;
			int host_type_id;
			int term_intrinsic_id;
		} intrinsic_name;
		struct {
			uint32_t term;
			uint32_t type_expr;
		} ascription;
	} as;
};

struct prototype_ast_type_parameter {
	uint32_t ast_binder_id;
	int name_symbol_id;
	uint32_t type_expr;
};

struct prototype_ast_type_constructor {
	int name_symbol_id;
	struct prototype_source_span name_span;
	uint32_t first_field_type;
	uint32_t field_count;
	uint32_t result_type;
};

struct prototype_ast_type_def {
	int name_symbol_id;
	struct prototype_source_span name_span;
	struct prototype_source_span body_span;
	uint32_t first_parameter;
	uint32_t parameter_count;
	uint32_t first_constructor;
	uint32_t constructor_count;
	uint32_t compiled_type;
	int compiling;
	int compiled;
};

struct prototype_ast_match_case {
	int constructor_symbol_id;
	uint32_t first_binder;
	uint32_t binder_count;
	uint32_t body;
};

struct prototype_ast_binder {
	uint32_t ast_binder_id;
	int symbol_id;
};

struct prototype_ast_match_case_input {
	int constructor_symbol_id;
	const struct prototype_ast_binder* binders;
	uint32_t binder_count;
	uint32_t body;
};

enum prototype_ast_type_entry_kind {
	PROTOTYPE_AST_TYPE_ENTRY_DECLARATION = 1,
	PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION
};

struct prototype_ast_type_expectation_def {
	int kind;
	int name_symbol_id;
	uint32_t type_expr;
	uint32_t source_entry_id;
	struct prototype_source_span name_span;
	struct prototype_source_span type_span;
	uint32_t paired_assignment_id;
	uint32_t next_for_symbol;
	uint32_t compiled_classifier;
	int compiling;
	int compiled;
};

struct prototype_ast_term_assignment_def {
	int name_symbol_id;
	uint32_t ast;
	uint32_t source_entry_id;
	struct prototype_source_span name_span;
	struct prototype_source_span body_span;
	uint32_t next_for_symbol;
	uint32_t compiled_term;
	uint32_t compiled_classifier;
	uint32_t compiled_operation;
	int compiling;
	int compiled;
	int published;
};

struct prototype_ast_import_def {
	int name_symbol_id;
	uint32_t source_entry_id;
	struct prototype_source_span name_span;
};

struct prototype_ast_def_open_address_entry {
	int occupied;
	int symbol_id;
	uint32_t first_expectation;
	uint32_t expectation_count;
	uint32_t first_assignment;
	uint32_t assignment_count;
};

enum prototype_resolve_error_kind {
	PROTOTYPE_RESOLVE_ERROR_NAME = 1,
	PROTOTYPE_RESOLVE_ERROR_NAMESPACE,
	PROTOTYPE_RESOLVE_ERROR_RECURSIVE,
	PROTOTYPE_RESOLVE_ERROR_DUPLICATE_EXPECTATION,
	PROTOTYPE_RESOLVE_ERROR_DUPLICATE_ASSIGNMENT,
	PROTOTYPE_RESOLVE_ERROR_AMBIGUOUS_ASSIGNMENT,
	PROTOTYPE_RESOLVE_ERROR_DUPLICATE_DEFINITION,
	PROTOTYPE_RESOLVE_ERROR_COMPILE
};

struct prototype_compile_label {
	int name_symbol_id;
	uint32_t term;
	uint32_t classifier;
	uint32_t operation;
	struct prototype_term_canonical_key canonical_key;
};

/*
 * Operation nodes preserve the typed/source occurrence graph produced by AST
 * lowering.  Their core_term fields may intentionally alias: for example,
 * \x : Bool => x and \y : Nat => y share one core lambda but have distinct
 * operation nodes and classifiers.
 */
enum prototype_operation_tag {
	PROTOTYPE_OPERATION_ATOM = 1,
	PROTOTYPE_OPERATION_VAR,
	PROTOTYPE_OPERATION_NAME,
	PROTOTYPE_OPERATION_CONSTRUCTOR,
	PROTOTYPE_OPERATION_APP,
	PROTOTYPE_OPERATION_LAMBDA,
	PROTOTYPE_OPERATION_MATCH,
	PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS,
	PROTOTYPE_OPERATION_ASCRIPTION
};

struct prototype_operation_node {
	int tag;
	uint32_t core_term;
	uint32_t classifier;
	uint32_t source_ast;
	int source_symbol_id;
	int binder_symbol_id;
	uint32_t function;
	uint32_t argument;
	uint32_t body;
	uint32_t scrutinee;
	uint32_t binder_classifier;
	uint32_t first_case;
	uint32_t case_count;
};

struct prototype_operation_match_case {
	uint32_t body_operation;
	uint32_t constructor_owner;
	uint32_t constructor_id;
	int case_label_symbol_id;
};

struct prototype_compile_constructor_export {
	uint32_t type_export_index;
	int name_symbol_id;
	uint32_t ordinal;
	uint32_t readback_first_field_type;
	uint32_t readback_field_count;
	uint32_t classifier_family;
};

struct prototype_compile_type_export {
	int name_symbol_id;
	uint32_t type_id;
	struct prototype_type_code_shape_key code_shape_key;
	uint32_t first_constructor_export;
	uint32_t constructor_count;
};

enum prototype_artifact_export_transparency {
	PROTOTYPE_ARTIFACT_EXPORT_OPAQUE = 1,
	PROTOTYPE_ARTIFACT_EXPORT_TRANSPARENT
};

struct prototype_artifact_term_export {
	int namespace_symbol_id;
	int name_symbol_id;
	uint32_t local_term;
	uint32_t classifier;
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
	struct prototype_type_code_shape_key code_shape_key;
	uint32_t first_parameter;
	uint32_t parameter_count;
	uint32_t first_constructor_export;
	uint32_t constructor_count;
};

struct prototype_artifact_type_parameter_export {
	uint32_t binder_id;
	int name_symbol_id;
	uint32_t type_expr;
};

struct prototype_artifact_constructor_export {
	uint32_t type_export_index;
	int name_symbol_id;
	uint32_t ordinal;
	uint32_t readback_first_field_type;
	uint32_t readback_field_count;
	uint32_t classifier_family;
};

struct prototype_artifact_dependency {
	int namespace_symbol_id;
	int name_symbol_id;
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
	struct prototype_type_code_shape_key code_shape_key;
};

struct prototype_artifact_resolved_external_type_former_ref {
	uint32_t type_expr;
	uint32_t type_export_index;
	struct prototype_qualified_name name;
	struct prototype_type_code_shape_key code_shape_key;
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

struct prototype_resolve_error {
	int kind;
	int name_symbol_id;
	int member_symbol_id;
	uint32_t ast;
	struct prototype_source_span span;
};

enum prototype_resolution_event_kind {
	PROTOTYPE_RESOLUTION_EVENT_MATCH_CONSTRUCTOR = 1
};

enum prototype_resolution_item_state {
	PROTOTYPE_RESOLUTION_ITEM_UNRESOLVED = 1,
	PROTOTYPE_RESOLUTION_ITEM_RESOLVED = 2,
	PROTOTYPE_RESOLUTION_ITEM_ERROR = 3
};

struct prototype_resolution_item {
	uint32_t id;
	int kind;
	int state;
	uint32_t created_iteration;
	uint32_t resolved_iteration;
	uint32_t ast;
	uint32_t match_term;
	uint32_t case_index;
	uint32_t scrutinee_term;
	int symbol_id;
	uint32_t resolved_owner;
	uint32_t resolved_id;
};

struct prototype_resolution_event {
	uint32_t item_id;
	uint32_t iteration;
	int kind;
	int from_state;
	int to_state;
	uint32_t ast;
	uint32_t match_term;
	uint32_t case_index;
	uint32_t scrutinee_term;
	int symbol_id;
	uint32_t resolved_owner;
	uint32_t resolved_id;
};

struct prototype_resolution_iteration {
	uint32_t iteration;
	size_t unresolved_before;
	size_t unresolved_after;
	size_t event_start;
	size_t event_count;
};

struct prototype_compile_metadata {
	struct prototype_operation_node* operations;
	size_t operation_count;
	size_t operation_capacity;

	struct prototype_operation_match_case* operation_cases;
	size_t operation_case_count;
	size_t operation_case_capacity;

	struct prototype_compile_label* labels;
	size_t label_count;
	size_t label_capacity;

	struct prototype_compile_type_export* type_exports;
	size_t type_export_count;
	size_t type_export_capacity;

	struct prototype_compile_constructor_export* constructor_exports;
	size_t constructor_export_count;
	size_t constructor_export_capacity;

	struct prototype_resolve_error* resolve_errors;
	size_t resolve_error_count;
	size_t resolve_error_capacity;

	struct prototype_resolution_item* resolution_items;
	size_t resolution_item_count;
	size_t resolution_item_capacity;

	struct prototype_resolution_iteration* resolution_iterations;
	size_t resolution_iteration_count;
	size_t resolution_iteration_capacity;

	struct prototype_resolution_event* resolution_events;
	size_t resolution_event_count;
	size_t resolution_event_capacity;
};

struct prototype_ast_db {
	struct prototype_ast_node* nodes;
	size_t node_count;
	size_t node_capacity;

	struct prototype_ast_type_expectation_def* expectations;
	size_t expectation_count;
	size_t expectation_capacity;

	struct prototype_ast_term_assignment_def* assignments;
	size_t assignment_count;
	size_t assignment_capacity;

	struct prototype_ast_import_def* imports;
	size_t import_count;
	size_t import_capacity;

	struct prototype_ast_def_open_address_entry* def_index;
	size_t def_index_count;
	size_t def_index_capacity;

	struct prototype_ast_match_case* cases;
	size_t case_count;
	size_t case_capacity;

	struct prototype_ast_binder* case_binders;
	size_t case_binder_count;
	size_t case_binder_capacity;

	struct prototype_ast_type_expr* type_exprs;
	size_t type_expr_count;
	size_t type_expr_capacity;

	struct prototype_ast_type_def* type_defs;
	size_t type_def_count;
	size_t type_def_capacity;

	struct prototype_ast_type_parameter* type_parameters;
	size_t type_parameter_count;
	size_t type_parameter_capacity;

	struct prototype_ast_type_constructor* type_constructors;
	size_t type_constructor_count;
	size_t type_constructor_capacity;

	uint32_t* type_field_exprs;
	uint32_t* type_field_binder_ids;
	int* type_field_name_symbol_ids;
	size_t type_field_expr_count;
	size_t type_field_expr_capacity;

	uint32_t next_ast_binder_id;
	uint32_t next_ast_level_var;
	uint32_t next_source_entry_id;
};

void prototype_ast_db_init(
	struct prototype_ast_db* db,
	struct prototype_ast_node* nodes,
	size_t node_capacity,
	struct prototype_ast_type_expectation_def* expectations,
	size_t expectation_capacity,
	struct prototype_ast_term_assignment_def* assignments,
	size_t assignment_capacity,
	struct prototype_ast_import_def* imports,
	size_t import_capacity,
	struct prototype_ast_def_open_address_entry* def_index,
	size_t def_index_capacity,
	struct prototype_ast_match_case* cases,
	size_t case_capacity,
	struct prototype_ast_binder* case_binders,
	size_t case_binder_capacity,
	struct prototype_ast_type_expr* type_exprs,
	size_t type_expr_capacity,
	struct prototype_ast_type_def* type_defs,
	size_t type_def_capacity,
	struct prototype_ast_type_parameter* type_parameters,
	size_t type_parameter_capacity,
	struct prototype_ast_type_constructor* type_constructors,
	size_t type_constructor_capacity,
	uint32_t* type_field_exprs,
	uint32_t* type_field_binder_ids,
	int* type_field_name_symbol_ids,
	size_t type_field_expr_capacity
);

uint32_t prototype_ast_new_binder(struct prototype_ast_db* db);
int prototype_ast_type_expr_universe(
	struct prototype_ast_db* db,
	uint32_t level,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_expr_fresh_universe(
	struct prototype_ast_db* db,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_expr_self(
	struct prototype_ast_db* db,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_expr_var(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_expr_name(
	struct prototype_ast_db* db,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_expr_host_type(
	struct prototype_ast_db* db,
	int host_type_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_expr_app(
	struct prototype_ast_db* db,
	uint32_t function,
	uint32_t argument,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_expr_arrow(
	struct prototype_ast_db* db,
	uint32_t domain,
	uint32_t codomain,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_add(
	struct prototype_ast_db* db,
	int name_symbol_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_type_def_id
);
int prototype_ast_type_add_parameter(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	uint32_t ast_binder_id,
	int name_symbol_id,
	uint32_t type_expr
);
int prototype_ast_type_add_constructor(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	int name_symbol_id,
	struct prototype_source_span name_span,
	const uint32_t* field_type_exprs,
	const uint32_t* field_binder_ids,
	const int* field_name_symbol_ids,
	uint32_t field_count,
	uint32_t result_type_expr
);
int prototype_ast_var(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_name(
	struct prototype_ast_db* db,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_name_in_namespace(
	struct prototype_ast_db* db,
	int namespace_symbol_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_name_in_ast_namespace(
	struct prototype_ast_db* db,
	uint32_t namespace_ast,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_app(
	struct prototype_ast_db* db,
	uint32_t function,
	uint32_t argument,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_lambda(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int binder_symbol_id,
	uint32_t binder_type,
	uint32_t body,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_match(
	struct prototype_ast_db* db,
	uint32_t scrutinee,
	const struct prototype_ast_match_case_input* cases,
	uint32_t case_count,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_literal(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_formation(
	struct prototype_ast_db* db,
	uint32_t ast_type_def_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_induction_hypothesis(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_text_literal(
	struct prototype_ast_db* db,
	int text_symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_int_literal(
	struct prototype_ast_db* db,
	int64_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_intrinsic_name(
	struct prototype_ast_db* db,
	int namespace_symbol_id,
	int symbol_id,
	int type_symbol_id,
	int intrinsic_id,
	int host_type_id,
	int term_intrinsic_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_ascription(
	struct prototype_ast_db* db,
	uint32_t term,
	uint32_t type_expr,
	struct prototype_source_span span,
	uint32_t* p_ret
);

uint32_t prototype_ast_new_source_entry(struct prototype_ast_db* db);
int prototype_ast_add_type_expectation(
	struct prototype_ast_db* db,
	int kind,
	int name_symbol_id,
	uint32_t type_expr,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span type_span,
	uint32_t paired_assignment_id,
	uint32_t* p_ret
);
int prototype_ast_add_term_assignment(
	struct prototype_ast_db* db,
	int name_symbol_id,
	uint32_t ast,
	uint32_t source_entry_id,
	struct prototype_source_span name_span,
	struct prototype_source_span body_span,
	uint32_t* p_ret
);
int prototype_ast_add_import(
	struct prototype_ast_db* db,
	int name_symbol_id,
	uint32_t source_entry_id,
	struct prototype_source_span name_span
);
int prototype_ast_pair_type_expectation(
	struct prototype_ast_db* db,
	uint32_t expectation_id,
	uint32_t assignment_id
);
const struct prototype_ast_term_assignment_def* prototype_ast_lookup_assignment_const(
	const struct prototype_ast_db* db,
	int name_symbol_id
);

void prototype_compile_metadata_init(
	struct prototype_compile_metadata* metadata,
	struct prototype_compile_label* labels,
	size_t label_capacity,
	struct prototype_compile_type_export* type_exports,
	size_t type_export_capacity,
	struct prototype_compile_constructor_export* constructor_exports,
	size_t constructor_export_capacity,
	struct prototype_resolve_error* resolve_errors,
	size_t resolve_error_capacity,
	struct prototype_resolution_item* resolution_items,
	size_t resolution_item_capacity,
	struct prototype_resolution_iteration* resolution_iterations,
	size_t resolution_iteration_capacity,
	struct prototype_resolution_event* resolution_events,
	size_t resolution_event_capacity,
	struct prototype_operation_node* operations,
	size_t operation_capacity,
	struct prototype_operation_match_case* operation_cases,
	size_t operation_case_capacity
);

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
	struct prototype_artifact_dependency* dependencies,
	size_t dependency_capacity
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
	struct prototype_type_declaration_db* type_declarations
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

int prototype_artifact_write_text(
	FILE* stream,
	const struct symbol_table* symbols,
	const struct prototype_artifact_interface* interface,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_judgement_db* judgement,
	const struct prototype_universe_db* universe,
	const struct prototype_ast_db* asts
);

int prototype_artifact_read_text_interface(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_interface* interface
);

int prototype_artifact_read_text_graph(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement
);

int prototype_artifact_read_text_universe(
	FILE* stream,
	struct prototype_universe_db* universe
);

int prototype_artifact_read_text_debug(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_debug_table* debug
);

int prototype_artifact_read_text_relocation(
	FILE* stream,
	struct symbol_table* symbols,
	struct prototype_artifact_relocation_table* relocation
);

int prototype_artifact_apply_term_relocations(
	struct prototype_artifact_interface* target_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	const struct prototype_artifact_interface* provider_interface
);

int prototype_artifact_apply_type_expr_relocations(
	struct prototype_artifact_interface* target_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	const struct prototype_artifact_interface* provider_interface
);

int prototype_artifact_append_graph(
	struct prototype_artifact_interface* appended_interface,
	struct prototype_term_db* target_terms,
	struct prototype_type_declaration_db* target_type_declarations,
	struct prototype_judgement_db* target_judgement,
	const struct prototype_artifact_interface* source_interface,
	const struct prototype_term_db* source_terms,
	const struct prototype_type_declaration_db* source_type_declarations,
	const struct prototype_judgement_db* source_judgement
);

int prototype_canonical_link_table_add_metadata(
	struct prototype_canonical_link_table* table,
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_compile_metadata* metadata,
	uint32_t unit_id,
	int reject_frame_local_references
);

int prototype_canonical_link_table_find(
	const struct prototype_canonical_link_table* table,
	const struct prototype_term_canonical_key* key,
	uint32_t* p_entry
);

int prototype_ast_compile_pending(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata
);
int prototype_ast_compile_pending_with_imports(
	struct prototype_ast_db* asts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_judgement_db* judgement,
	struct prototype_compile_metadata* metadata,
	int namespace_symbol_id,
	const struct prototype_artifact_interface* const* imported_interfaces,
	size_t imported_interface_count
);

#endif
