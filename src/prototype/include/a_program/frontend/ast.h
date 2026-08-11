#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_AST_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_AST_H

#include <stddef.h>
#include <stdint.h>

#include "calculus.h"
#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"

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
	PROTOTYPE_AST_SYSTEM_NAME,
	PROTOTYPE_AST_ASCRIPTION,
	PROTOTYPE_AST_QUOTE,
	PROTOTYPE_AST_DEFINITION_BLOCK,
	PROTOTYPE_AST_DEFINITION_SELECT,
	PROTOTYPE_AST_COMPUTATION_BLOCK,
	PROTOTYPE_AST_BLOCK_BINDING,
	PROTOTYPE_AST_BLOCK_EXPRESSION,
	PROTOTYPE_AST_BLOCK_LAMBDA_EXIT,
	PROTOTYPE_AST_COMPUTATION_FOLD
};

enum prototype_ast_block_result_mode {
	PROTOTYPE_AST_BLOCK_RESULT_FINAL_ITEM = 1,
	PROTOTYPE_AST_BLOCK_RESULT_SELECTED_BINDING
};

enum prototype_ast_system_name_kind {
	PROTOTYPE_AST_SYSTEM_NAME_UNKNOWN = 0,
	PROTOTYPE_AST_SYSTEM_NAME_HOST_TYPE,
	PROTOTYPE_AST_SYSTEM_NAME_PURE_PRIMITIVE,
	PROTOTYPE_AST_SYSTEM_NAME_EFFECT_OPERATION
};

enum prototype_ast_type_expr_tag {
	PROTOTYPE_AST_TYPE_EXPR_UNIVERSE = 1,
	PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR,
	PROTOTYPE_AST_TYPE_EXPR_SELF,
	PROTOTYPE_AST_TYPE_EXPR_VAR,
	PROTOTYPE_AST_TYPE_EXPR_NAME,
	PROTOTYPE_AST_TYPE_EXPR_APP,
	PROTOTYPE_AST_TYPE_EXPR_ARROW,
	PROTOTYPE_AST_TYPE_EXPR_PI,
	PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE,
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
			uint32_t ast_binder_id;
			int symbol_id;
			uint32_t domain;
			uint32_t codomain;
		} pi;
		struct {
			uint32_t result;
		} computation_reference;
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
			int kind;
			int host_type_id;
			int pure_primitive_id;
			int effect_operation_id;
		} system_name;
		struct {
			uint32_t term;
			uint32_t type_expr;
		} ascription;
		struct {
			uint32_t term;
		} unary;
		struct {
			uint32_t first_assignment;
			uint32_t assignment_count;
		} definition_block;
		struct {
			uint32_t definition_block;
			int name_symbol_id;
		} definition_select;
		struct {
			uint32_t first_item;
			uint32_t item_count;
			uint32_t result_item_index;
			int result_mode;
		} block;
		struct {
			uint32_t ast_binder_id;
			int binder_symbol_id;
			uint32_t binder_type;
			uint32_t value;
		} block_binding;
		struct {
			uint32_t term;
		} block_expression;
		struct {
			uint32_t value;
		} block_lambda_exit;
		struct {
			uint32_t computation;
			uint32_t first_clause;
			uint32_t clause_count;
			uint32_t return_binder_id;
			int return_symbol_id;
			uint32_t return_body;
		} computation_fold;
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
	struct prototype_source_span span;
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
	struct prototype_source_span span;
};

struct prototype_ast_computation_fold_clause {
	uint32_t operation;
	uint32_t operation_argument_binder_id;
	int operation_argument_symbol_id;
	uint32_t operation_continuation_binder_id;
	int operation_continuation_symbol_id;
	uint32_t body;
	struct prototype_source_span span;
};

struct prototype_ast_computation_fold_clause_input {
	uint32_t operation;
	uint32_t operation_argument_binder_id;
	int operation_argument_symbol_id;
	uint32_t operation_continuation_binder_id;
	int operation_continuation_symbol_id;
	uint32_t body;
	struct prototype_source_span span;
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
	int definition_value_required;
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

	struct prototype_ast_computation_fold_clause* computation_fold_clauses;
	size_t computation_fold_clause_count;
	size_t computation_fold_clause_capacity;

	uint32_t* block_items;
	size_t block_item_count;
	size_t block_item_capacity;

	uint32_t* definition_items;
	size_t definition_item_count;
	size_t definition_item_capacity;
	uint32_t root_definition_block;
	uint32_t root_definition_select;

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
	struct prototype_ast_computation_fold_clause* computation_fold_clauses,
	size_t computation_fold_clause_capacity,
	uint32_t* block_items,
	size_t block_item_capacity,
	uint32_t* definition_items,
	size_t definition_item_capacity,
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
int prototype_ast_type_expr_pi(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int symbol_id,
	uint32_t domain,
	uint32_t codomain,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_type_expr_computation_reference(
	struct prototype_ast_db* db,
	uint32_t result,
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
int prototype_ast_system_name(
	struct prototype_ast_db* db,
	int namespace_symbol_id,
	int symbol_id,
	int type_symbol_id,
	int kind,
	int host_type_id,
	int pure_primitive_id,
	int effect_operation_id,
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
int prototype_ast_quote(
	struct prototype_ast_db* db,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_definition_block(
	struct prototype_ast_db* db,
	const uint32_t* assignments,
	uint32_t assignment_count,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_definition_select(
	struct prototype_ast_db* db,
	uint32_t definition_block,
	int name_symbol_id,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_computation_block(
	struct prototype_ast_db* db,
	const uint32_t* items,
	uint32_t item_count,
	uint32_t result_item_index,
	int result_mode,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_block_binding(
	struct prototype_ast_db* db,
	uint32_t ast_binder_id,
	int binder_symbol_id,
	uint32_t binder_type,
	uint32_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_block_expression(
	struct prototype_ast_db* db,
	uint32_t term,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_block_lambda_exit(
	struct prototype_ast_db* db,
	uint32_t value,
	struct prototype_source_span span,
	uint32_t* p_ret
);
int prototype_ast_computation_fold(
	struct prototype_ast_db* db,
	uint32_t computation,
	const struct prototype_ast_computation_fold_clause_input* clauses,
	uint32_t clause_count,
	uint32_t return_binder_id,
	int return_symbol_id,
	uint32_t return_body,
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


#endif
