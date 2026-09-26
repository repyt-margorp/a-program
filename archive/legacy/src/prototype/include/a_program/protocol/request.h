#ifndef A_PROGRAM_PROTOTYPE_PROTOCOL_REQUEST_H
#define A_PROGRAM_PROTOTYPE_PROTOCOL_REQUEST_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/support/schema.h"

enum prototype_term_normalization_profile {
	PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF = 1,
	PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF = 2,
	PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF = 3
};

enum prototype_term_normalization_status {
	PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE = 1,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT = 2,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED = 3,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID = 4
};

/* Immutable Core-domain protocol values. This header exposes no callable
 * provider capability and no mutable Core representation. */
struct prototype_core_normalization_request {
	uint32_t term_id;
	int profile;
	uint64_t step_limit;
};

struct prototype_core_normalization_response {
	int status;
	uint32_t term_id;
	uint64_t step_limit;
	uint64_t steps_used;
	uint64_t graph_revision;
};

enum prototype_core_structural_return_projection_status {
	PROTOTYPE_CORE_STRUCTURAL_RETURN_PROJECTION_COMPLETE = 1,
	PROTOTYPE_CORE_STRUCTURAL_RETURN_PROJECTION_OPAQUE = 2,
	PROTOTYPE_CORE_STRUCTURAL_RETURN_PROJECTION_INVALID = 3
};

/* Generic Layer C calculation. It attempts to expose a symbolic value under a
 * RETURN without dispatching an effect. COMPLETE is a structural result, not
 * evidence that the computation is pure, total, or well typed. */
struct prototype_core_structural_return_projection_request {
	uint32_t computation;
	uint64_t step_limit;
};

struct prototype_core_structural_return_projection_response {
	int status;
	uint32_t value;
	uint64_t graph_revision;
};

struct prototype_core_request_range {
	uint32_t byte_offset;
	uint32_t element_count;
};

union prototype_core_request_alignment {
	long double long_double_value;
	void* pointer_value;
	uint64_t uint64_value;
};

/* Caller-owned contiguous protocol storage. A sealed payload contains no
 * pointer and can be copied as one value across a driver mailbox. */
struct prototype_core_request_payload {
	size_t byte_count;
	size_t byte_capacity;
	uint64_t digest;
	int sealed;
	union prototype_core_request_alignment alignment;
	unsigned char bytes[];
};

struct prototype_core_match_case_request {
	int case_label_symbol_id;
	uint32_t constructor_owner;
	uint32_t constructor_id;
	struct prototype_core_request_range binders;
	uint32_t body;
};

struct prototype_core_case_binder_request {
	uint32_t binding_id;
	int is_recursive;
};

struct prototype_core_computation_fold_clause_request {
	uint32_t operation;
	uint32_t body;
};

struct prototype_core_binding_replacement_request {
	uint32_t binding_id;
	uint32_t replacement;
};

size_t prototype_core_request_payload_storage_size(size_t byte_capacity);

int prototype_core_request_payload_init(
	void* storage,
	size_t storage_size,
	struct prototype_core_request_payload** payload
);

int prototype_core_request_payload_append(
	struct prototype_core_request_payload* payload,
	size_t storage_size,
	const void* values,
	size_t element_size,
	size_t element_count,
	struct prototype_core_request_range* range
);

int prototype_core_request_payload_seal(
	struct prototype_core_request_payload* payload,
	size_t storage_size
);

enum prototype_core_formation_kind {
	PROTOTYPE_CORE_FORM_NEW_BINDING = 1,
	PROTOTYPE_CORE_FORM_NEW_IH_SCOPE,
	PROTOTYPE_CORE_FORM_SET_IH_SCOPE_TERM,
	PROTOTYPE_CORE_FORM_VAR,
	PROTOTYPE_CORE_FORM_CONSTRUCTOR,
	PROTOTYPE_CORE_FORM_APP,
	PROTOTYPE_CORE_FORM_LAMBDA,
	PROTOTYPE_CORE_FORM_MATCH,
	PROTOTYPE_CORE_FORM_TYPE_FORMER,
	PROTOTYPE_CORE_FORM_TYPE_DECLARATION,
	PROTOTYPE_CORE_FORM_TYPE_VIEW,
	PROTOTYPE_CORE_FORM_INDUCTION_HYPOTHESIS,
	PROTOTYPE_CORE_FORM_UNIVERSE_VAR,
	PROTOTYPE_CORE_FORM_PRIMITIVE_TEXT,
	PROTOTYPE_CORE_FORM_TEXT_LITERAL,
	PROTOTYPE_CORE_FORM_PRIMITIVE_INT,
	PROTOTYPE_CORE_FORM_INT_LITERAL,
	PROTOTYPE_CORE_FORM_EFFECT_ROW_EMPTY,
	PROTOTYPE_CORE_FORM_EFFECT_ROW_VAR,
	PROTOTYPE_CORE_FORM_EFFECT_ROW_UNION,
	PROTOTYPE_CORE_FORM_EFFECT_ROW_FORALL,
	PROTOTYPE_CORE_FORM_EFFECT_ROW_OPERATION,
	PROTOTYPE_CORE_FORM_COMPUTATION_TYPE,
	PROTOTYPE_CORE_FORM_THUNK_TYPE,
	PROTOTYPE_CORE_FORM_RETURN,
	PROTOTYPE_CORE_FORM_THUNK,
	PROTOTYPE_CORE_FORM_FORCE,
	PROTOTYPE_CORE_FORM_OPERATION_REQUEST,
	PROTOTYPE_CORE_FORM_COMPUTATION_FOLD,
	PROTOTYPE_CORE_FORM_HOST_TYPE,
	PROTOTYPE_CORE_FORM_EXTERNAL_REF,
	PROTOTYPE_CORE_FORM_PURE_PRIMITIVE,
	PROTOTYPE_CORE_FORM_EFFECT_OPERATION,
	PROTOTYPE_CORE_FORM_PI,
	PROTOTYPE_CORE_FORM_PI_FAMILY,
	PROTOTYPE_CORE_FORM_PURE_FAMILY,
	PROTOTYPE_CORE_FORM_TERMINATES_TYPE,
	PROTOTYPE_CORE_FORM_TERMINATES_WITNESS,
	/* Allocate a Core syntax identity. This does not allocate or solve a
	 * Layer T Universe metavariable. */
	PROTOTYPE_CORE_FORM_NEW_UNIVERSE_LEVEL,
	/* Calculate and intern the syntactic effect-row residual. Layer C does not
	 * decide which operations a handler is permitted to remove. */
	PROTOTYPE_CORE_FORM_EFFECT_ROW_RESIDUAL
};

struct prototype_core_formation_request {
	int kind;
	uint64_t payload_digest;
	union {
		struct { uint32_t ih_scope_id; uint32_t match_term; } set_ih_scope;
		struct { uint32_t binding_id; } var;
		struct { uint32_t owner; uint32_t constructor_id; } constructor;
		struct { uint32_t function; uint32_t argument; } app;
		struct { uint32_t binding_id; uint32_t body; } lambda;
		struct {
			uint32_t scrutinee;
			struct prototype_core_request_range cases;
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
		struct { uint32_t binding_id; } effect_row_var;
		struct { uint32_t left; uint32_t right; } effect_row_union;
		struct { uint32_t source; uint32_t handled; } effect_row_residual;
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
			struct prototype_core_request_range clauses;
		} computation_fold;
		struct { int type_id; } host_type;
		struct { struct prototype_qualified_name name; } external_ref;
		struct { int primitive_id; int type_symbol_id; } pure_primitive;
		struct { int operation_id; } effect_operation;
		struct { uint32_t domain; uint32_t codomain; } pi;
		struct { uint32_t domain; uint32_t codomain_family; } pi_family;
		struct { uint32_t binding_id; uint32_t body; } pure_family;
		struct { uint32_t computation; } terminates;
	} as;
};

struct prototype_core_formation_response {
	uint32_t term_id;
	uint32_t identity_id;
	uint64_t graph_revision;
};

int prototype_core_formation_request_payload_validate(
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size
);

enum prototype_core_rewrite_kind {
	PROTOTYPE_CORE_REWRITE_SUBSTITUTE_BOUND_VAR = 1,
	PROTOTYPE_CORE_REWRITE_REPLACE_EXACT,
	PROTOTYPE_CORE_REWRITE_REPLACE_EXACT_OUTSIDE_TYPE_VIEWS,
	PROTOTYPE_CORE_REWRITE_BINDINGS,
	PROTOTYPE_CORE_REWRITE_BINDING_SEQUENCE
};

struct prototype_core_rewrite_request {
	int kind;
	uint32_t term_id;
	uint64_t payload_digest;
	union {
		struct { uint32_t binding_id; uint32_t replacement; }
			substitute_bound_var;
		struct { uint32_t exact_term; uint32_t replacement; } replace_exact;
		struct prototype_core_request_range bindings;
		struct prototype_core_request_range binding_sequence;
	} as;
};

struct prototype_core_rewrite_response {
	uint32_t term_id;
	uint64_t graph_revision;
};

int prototype_core_rewrite_request_payload_validate(
	const struct prototype_core_rewrite_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size
);

#endif
