#ifndef A_PROGRAM_PROTOTYPE_PROTOCOL_GRAPH_H
#define A_PROGRAM_PROTOTYPE_PROTOCOL_GRAPH_H

#include <stdint.h>

#include "a_program/support/schema.h"

/* Immutable value schema of the Layer C calculation graph. This header
 * deliberately exposes no TermDB storage, interning index, or reducer cache. */
enum prototype_term_tag {
	PROTOTYPE_TERM_VAR = 1,
	PROTOTYPE_TERM_CONSTRUCTOR = 2,
	PROTOTYPE_TERM_APP = 3,
	PROTOTYPE_TERM_LAMBDA = 4,
	PROTOTYPE_TERM_PI = 5,
	PROTOTYPE_TERM_MATCH = 6,
	PROTOTYPE_TERM_TYPE_FORMER = 7,
	PROTOTYPE_TERM_TYPE_DECLARATION = 8,
	PROTOTYPE_TERM_INDUCTION_HYPOTHESIS = 9,
	PROTOTYPE_TERM_UNIVERSE_VAR = 10,
	PROTOTYPE_TERM_PRIMITIVE_TEXT = 11,
	PROTOTYPE_TERM_TEXT_LITERAL = 12,
	PROTOTYPE_TERM_PRIMITIVE_INT = 13,
	PROTOTYPE_TERM_PRIMITIVE_INT64 = 14,
	PROTOTYPE_TERM_INT_LITERAL = 15,
	PROTOTYPE_TERM_EXTERNAL_REF = 16,
	PROTOTYPE_TERM_PURE_PRIMITIVE = 17,
	PROTOTYPE_TERM_EFFECT_OPERATION = 18,
	PROTOTYPE_TERM_TYPE_VIEW = 19,
	PROTOTYPE_TERM_EFFECT_ROW_EMPTY = 20,
	PROTOTYPE_TERM_EFFECT_ROW_VAR = 21,
	PROTOTYPE_TERM_EFFECT_ROW_UNION = 22,
	PROTOTYPE_TERM_EFFECT_ROW_FORALL = 23,
	PROTOTYPE_TERM_COMPUTATION_TYPE = 24,
	PROTOTYPE_TERM_THUNK_TYPE = 25,
	PROTOTYPE_TERM_RETURN = 26,
	PROTOTYPE_TERM_THUNK = 27,
	PROTOTYPE_TERM_FORCE = 28,
	PROTOTYPE_TERM_OPERATION_REQUEST = 29,
	PROTOTYPE_TERM_COMPUTATION_FOLD = 30,
	PROTOTYPE_TERM_EFFECT_ROW_OPERATION = 31,
	PROTOTYPE_TERM_RELATION_TYPE_FORMER = 32,
	PROTOTYPE_TERM_RELATION_WITNESS_FORMER = 33,
	PROTOTYPE_TERM_DIMENSION_ACTION = 34,
	PROTOTYPE_TERM_TERMINATES_TYPE_FORMER = 37,
	PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER = 38
};

#define PROTOTYPE_TERM_TAG_MAX PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER

enum prototype_computation_totality {
	PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN = 0,
	PROTOTYPE_COMPUTATION_TOTALITY_TOTAL = 1,
	PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE = 2
};

enum prototype_term_child_role {
	PROTOTYPE_TERM_CHILD_INVALID = 0,
	PROTOTYPE_TERM_CHILD_FUNCTION = 1,
	PROTOTYPE_TERM_CHILD_ARGUMENT = 2,
	PROTOTYPE_TERM_CHILD_BODY = 3,
	PROTOTYPE_TERM_CHILD_DOMAIN = 4,
	PROTOTYPE_TERM_CHILD_CODOMAIN_FAMILY = 5,
	PROTOTYPE_TERM_CHILD_SCRUTINEE = 6,
	PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY = 7,
	PROTOTYPE_TERM_CHILD_TYPE_VIEW_CORE = 8,
	PROTOTYPE_TERM_CHILD_TYPE_VIEW_SOURCE = 9,
	PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT = 10,
	PROTOTYPE_TERM_CHILD_EFFECT_ROW_LEFT = 12,
	PROTOTYPE_TERM_CHILD_EFFECT_ROW_RIGHT = 13,
	PROTOTYPE_TERM_CHILD_EFFECT_ROW_BODY = 14,
	PROTOTYPE_TERM_CHILD_EFFECT_ROW_LATENT = 15,
	PROTOTYPE_TERM_CHILD_COMPUTATION_EFFECT_ROW = 16,
	PROTOTYPE_TERM_CHILD_SEQUENCE_RESULT = 17,
	PROTOTYPE_TERM_CHILD_THUNK_TYPE_COMPUTATION = 18,
	PROTOTYPE_TERM_CHILD_RETURN_VALUE = 19,
	PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION = 20,
	PROTOTYPE_TERM_CHILD_FORCE_VALUE = 21,
	PROTOTYPE_TERM_CHILD_REQUEST_OPERATION = 22,
	PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT = 23,
	PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION = 24,
	PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION = 25,
	PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE = 26,
	PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION = 27,
	PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY = 28,
	PROTOTYPE_TERM_CHILD_DIMENSION_ACTION_SOURCE = 29,
	PROTOTYPE_TERM_CHILD_TERMINATION_EVIDENCE_COMPUTATION = 32
};

struct prototype_term_child {
	int role;
	uint32_t ordinal;
	uint32_t term;
};

struct prototype_term {
	int tag;
	union {
		struct { uint32_t binding_id; } var;
		struct { uint32_t owner; uint32_t constructor_id; } constructor;
		struct { uint32_t function; uint32_t argument; } app;
		struct { uint32_t binding_id; uint32_t body; } lambda;
		struct { uint32_t domain; uint32_t codomain_family; } pi;
		struct {
			uint32_t scrutinee;
			uint32_t first_case;
			uint32_t case_count;
			uint32_t ih_scope_id;
		} match;
		struct { uint32_t representation_id; uint32_t constructor_count; }
			type_former;
		struct { struct prototype_qualified_name identity; } type_declaration;
		struct {
			struct prototype_qualified_name identity;
			uint32_t core;
			uint32_t source;
		} type_view;
		struct { uint32_t ih_scope_id; uint32_t argument; }
			induction_hypothesis;
		struct { uint32_t level_var; } universe_var;
		struct { int text_symbol_id; } text_literal;
		struct { int64_t value; } int_literal;
		struct { struct prototype_qualified_name name; } external_ref;
		struct { int primitive_id; int type_symbol_id; } pure_primitive;
		struct { int operation_id; } effect_operation;
		struct { uint32_t binding_id; } effect_row_var;
		struct { uint32_t left; uint32_t right; } effect_row_union;
		struct { uint32_t binding_id; uint32_t body; } effect_row_forall;
		struct { int operation_id; uint32_t latent_row; }
			effect_row_operation;
		struct { uint32_t label; uint32_t result; int totality; }
			computation_type;
		struct { uint32_t computation; } thunk_type;
		struct { uint32_t value; } return_term;
		struct { uint32_t computation; } thunk;
		struct { uint32_t value; } force;
		struct {
			uint32_t operation;
			uint32_t argument;
			uint32_t continuation;
		} operation_request;
		struct {
			uint32_t computation;
			uint32_t return_clause;
			uint32_t first_clause;
			uint32_t clause_count;
		} computation_fold;
		struct { uint32_t source; uint32_t operator_id; } dimension_action;
	} as;
};

struct prototype_computation_fold_clause {
	uint32_t operation;
	uint32_t body;
};

struct prototype_match_case {
	uint32_t constructor_owner;
	uint32_t constructor_id;
	uint32_t first_binder;
	uint32_t binder_count;
	uint32_t body;
};

struct prototype_case_binder {
	uint32_t binding_id;
	int is_recursive;
};

struct prototype_binding_replacement {
	uint32_t binding_id;
	uint32_t replacement;
};

struct prototype_term_canonical_key {
	uint64_t hash;
	uint32_t node_count;
	uint32_t bound_binder_count;
	uint32_t free_binder_count;
	int has_frame_local_reference;
	int has_type_local_reference;
	int has_type_name_reference;
	int has_type_universe_reference;
};

struct prototype_ih_scope_key {
	struct prototype_term_canonical_key match_key;
	uint32_t case_count;
	int is_linkable;
};

struct prototype_ih_scope {
	uint32_t match_term;
	uint32_t scrutinee_binding_id;
	/* Stable lexical identity shared by substitution-specialized frames. */
	uint32_t binding_scope_id;
	struct prototype_ih_scope_key key;
};

#endif
