#ifndef __PROTOTYPE_TERM_H__
#define __PROTOTYPE_TERM_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "symbol.h"
#include "type_declaration.h"

struct prototype_term_db;
struct prototype_term_definition_env;
struct prototype_term_reduction_options;

/* Runtime-only dispatch for an OPERATION_REQUEST. Returning 1 supplies a
 * result, 0 leaves the request unhandled, and -1 reports a runtime failure. */
typedef int (*prototype_term_operation_dispatch_fn)(
	void* context,
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_term_reduction_options* options,
	uint32_t operation,
	uint32_t argument,
	uint32_t* p_result,
	unsigned depth
);

#define PROTOTYPE_BASE_NAMESPACE_ID (-1)
#define PROTOTYPE_SCOPE_BINDING_CAPACITY 512
#define PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY 1024
#define PROTOTYPE_COMPUTATION_FOLD_CLAUSE_CAPACITY 4096
#define PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT UINT64_C(100000)
#define PROTOTYPE_SOLVER_DEFAULT_STEP_LIMIT UINT64_C(100000)

enum prototype_term_tag {
	PROTOTYPE_TERM_VAR = 1,
	PROTOTYPE_TERM_CONSTRUCTOR,
	PROTOTYPE_TERM_APP,
	PROTOTYPE_TERM_LAMBDA,
	PROTOTYPE_TERM_PI,
	PROTOTYPE_TERM_MATCH,
	PROTOTYPE_TERM_TYPE_FORMER,
	PROTOTYPE_TERM_TYPE_DECLARATION,
	PROTOTYPE_TERM_INDUCTION_HYPOTHESIS,
	PROTOTYPE_TERM_UNIVERSE_VAR,
	PROTOTYPE_TERM_PRIMITIVE_TEXT,
	PROTOTYPE_TERM_TEXT_LITERAL,
	PROTOTYPE_TERM_PRIMITIVE_INT,
	PROTOTYPE_TERM_PRIMITIVE_INT64,
	PROTOTYPE_TERM_INT_LITERAL,
	PROTOTYPE_TERM_EXTERNAL_REF,
	PROTOTYPE_TERM_PURE_PRIMITIVE,
	PROTOTYPE_TERM_EFFECT_OPERATION,
	PROTOTYPE_TERM_TYPE_VIEW,
	PROTOTYPE_TERM_EFFECT_LABEL,
	PROTOTYPE_TERM_EFFECT_ROW_VAR,
	PROTOTYPE_TERM_EFFECT_ROW_UNION,
	/* Classifier-only implicit quantification. The binder is erased at runtime
	 * and scopes EFFECT_ROW_VAR occurrences in body. */
	PROTOTYPE_TERM_EFFECT_ROW_FORALL,
	PROTOTYPE_TERM_COMPUTATION_TYPE,
	PROTOTYPE_TERM_THUNK_TYPE,
	PROTOTYPE_TERM_RETURN,
	PROTOTYPE_TERM_THUNK,
	PROTOTYPE_TERM_FORCE,
	PROTOTYPE_TERM_OPERATION_REQUEST,
	PROTOTYPE_TERM_COMPUTATION_FOLD,
	/* Static effect-row atom for one higher-order operation family and the
	 * latent effects of its suspended computation argument. */
	PROTOTYPE_TERM_EFFECT_ROW_OPERATION
};

enum prototype_term_category {
	PROTOTYPE_TERM_CATEGORY_INVALID = 0,
	PROTOTYPE_TERM_CATEGORY_VALUE,
	PROTOTYPE_TERM_CATEGORY_COMPUTATION,
	PROTOTYPE_TERM_CATEGORY_TYPE
};

enum prototype_term_computation_kind {
	PROTOTYPE_TERM_COMPUTATION_KIND_INVALID = 0,
	PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING,
	PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION,
	PROTOTYPE_TERM_COMPUTATION_KIND_HANDLER
};

struct prototype_term_classifier_view {
	int category;
	int computation_kind;
	unsigned effects;
	uint32_t effect_row;
	uint32_t result;
};

enum prototype_pure_primitive_id {
	PROTOTYPE_PURE_PRIMITIVE_UNKNOWN = 0,
	PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT,
	PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT,
	PROTOTYPE_PURE_PRIMITIVE_INT_ADD,
	PROTOTYPE_PURE_PRIMITIVE_INT_SUB,
	PROTOTYPE_PURE_PRIMITIVE_INT_MUL,
	PROTOTYPE_PURE_PRIMITIVE_INT_NEG,
	PROTOTYPE_PURE_PRIMITIVE_INT64_ADD,
	PROTOTYPE_PURE_PRIMITIVE_INT64_SUB,
	PROTOTYPE_PURE_PRIMITIVE_INT64_MUL,
	PROTOTYPE_PURE_PRIMITIVE_INT64_NEG
};

enum prototype_effect_operation_id {
	PROTOTYPE_EFFECT_OPERATION_UNKNOWN = 0,
	PROTOTYPE_EFFECT_OPERATION_PRINT,
	PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT,
	PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT_ONCE,
	PROTOTYPE_EFFECT_OPERATION_ABORT_TEXT
};

enum prototype_effect_operation_classifier_schema {
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_INVALID = 0,
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_TEXT_TO_TEXT,
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT
};

enum prototype_effect_operation_inner_policy {
	PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE = 0,
	PROTOTYPE_EFFECT_OPERATION_INNER_SCOPED
};

enum prototype_effect_operation_resumption_multiplicity {
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT = 0,
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT,
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE
};

enum prototype_host_type_id {
	PROTOTYPE_HOST_TYPE_INVALID = 0,
	PROTOTYPE_HOST_TYPE_TEXT,
	PROTOTYPE_HOST_TYPE_INT32,
	PROTOTYPE_HOST_TYPE_INT64
};

enum prototype_host_oracle_kind {
	PROTOTYPE_HOST_ORACLE_NONE = 0,
	PROTOTYPE_HOST_ORACLE_PRINT,
	PROTOTYPE_HOST_ORACLE_TEXT_TO_NAT,
	PROTOTYPE_HOST_ORACLE_NAT_TO_TEXT,
	PROTOTYPE_HOST_ORACLE_INT_ADD,
	PROTOTYPE_HOST_ORACLE_INT_SUB,
	PROTOTYPE_HOST_ORACLE_INT_MUL,
	PROTOTYPE_HOST_ORACLE_INT_NEG
};

enum prototype_host_effect_flag {
	PROTOTYPE_HOST_EFFECT_NONE = 0,
	PROTOTYPE_HOST_EFFECT_TERMINAL = 1u << 0
};

enum prototype_effect_operation_label {
	PROTOTYPE_EFFECT_OPERATION_LABEL_NONE = 0,
	PROTOTYPE_EFFECT_OPERATION_LABEL_PRINT = 1u << 0,
	PROTOTYPE_EFFECT_OPERATION_LABEL_SCOPE_TEXT = 1u << 1,
	PROTOTYPE_EFFECT_OPERATION_LABEL_SCOPE_TEXT_ONCE = 1u << 2,
	PROTOTYPE_EFFECT_OPERATION_LABEL_ABORT_TEXT = 1u << 3
};

enum prototype_effect_row_purity {
	PROTOTYPE_EFFECT_ROW_PURITY_INVALID = 0,
	PROTOTYPE_EFFECT_ROW_PURITY_PURE,
	PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL,
	PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED
};

#define PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY 2
#define PROTOTYPE_EFFECT_OPERATION_MAX_ARITY 1

struct prototype_pure_primitive_declaration {
	int primitive_id;
	uint32_t arity;
	int argument_types[PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY];
	int result_type;
};

/* Effect-operation declarations are language-level interface data. Runtime
 * implementations and intrinsic-namespace spellings are separate. */
struct prototype_effect_operation_declaration {
	int operation_id;
	int classifier_schema;
	unsigned operation_labels;
	unsigned required_host_effects;
	uint32_t arity;
	int inner_policy;
	int resumption_multiplicity;
};

enum prototype_intrinsic_namespace_binding_kind {
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_UNKNOWN = 0,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_COMPUTATION_FOLD_RETURN
};

struct prototype_intrinsic_namespace_binding {
	const char* source_name;
	int kind;
	int target_id;
};

enum prototype_term_layer {
	PROTOTYPE_TERM_LAYER_LAMBDA_CORE = 1,
	PROTOTYPE_TERM_LAYER_ELIMINATOR,
	PROTOTYPE_TERM_LAYER_TYPE_FORMER,
	PROTOTYPE_TERM_LAYER_DATA,
	PROTOTYPE_TERM_LAYER_LINK,
	PROTOTYPE_TERM_LAYER_PURE_PRIMITIVE,
	PROTOTYPE_TERM_LAYER_EFFECT_OPERATION,
	PROTOTYPE_TERM_LAYER_INDUCTION
};

enum prototype_term_whnf_role {
	PROTOTYPE_TERM_WHNF_NEUTRAL = 1,
	PROTOTYPE_TERM_WHNF_INTRODUCTION,
	PROTOTYPE_TERM_WHNF_ELIMINATOR,
	PROTOTYPE_TERM_WHNF_ATOMIC
};

enum prototype_term_application_role {
	PROTOTYPE_TERM_APPLICATION_NONE = 0,
	/* The function may expose a lambda after permitted normalization. */
	PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION,
	/* The APP chain is assembling fields under a constructor telescope. */
	PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION
};

enum prototype_term_definition_transparency {
	PROTOTYPE_TERM_DEFINITION_OPAQUE = 1,
	PROTOTYPE_TERM_DEFINITION_TRANSPARENT
};

enum prototype_term_reduction_flag {
	PROTOTYPE_TERM_REDUCE_DEFINITIONS = 1u << 0,
	PROTOTYPE_TERM_REDUCE_BETA = 1u << 1,
	PROTOTYPE_TERM_REDUCE_MATCH = 1u << 2,
	PROTOTYPE_TERM_REDUCE_INDUCTION = 1u << 3,
	/* CBPV cut elimination: force/thunk, computation-fold/return, and graph handlers.
	 * This is structural computation reduction, never host-effect dispatch. */
	PROTOTYPE_TERM_REDUCE_COMPUTATIONS = 1u << 4,
	/* A semantic profile marker. It introduces no reduction rule; it keeps
	 * pure conversion cache entries distinct from computation WHNF entries. */
	PROTOTYPE_TERM_REDUCE_PURE_TYPE = 1u << 5,
	PROTOTYPE_TERM_PERFORM_HOST_EFFECT = 1u << 6,
	/* Deterministic host intrinsics with an empty effect row are computation
	 * reductions. They are available to execution, never to type conversion. */
	PROTOTYPE_TERM_REDUCE_PURE_INTRINSICS = 1u << 7
};

/*
 * A profile specifies the semantic layer at which a weak-head result is
 * observed.  The profiles intentionally exclude host evaluation and effects.
 */
enum prototype_term_normalization_profile {
	PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF = 1,
	PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
	PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF
};

/* A kernel normalization attempt may stop without establishing a normal form.
 * This status is intentionally separate from runtime operation dispatch. */
enum prototype_term_normalization_status {
	PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE = 1,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID
};

struct prototype_term_normalization_result {
	int status;
	uint32_t term_id;
	uint64_t step_limit;
	uint64_t steps_used;
	uint64_t graph_revision;
};

enum prototype_term_conversion_status {
	PROTOTYPE_TERM_CONVERSION_EQUAL = 1,
	PROTOTYPE_TERM_CONVERSION_NOT_EQUAL,
	PROTOTYPE_TERM_CONVERSION_RESIDUAL,
	PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT,
	PROTOTYPE_TERM_CONVERSION_EXHAUSTED,
	PROTOTYPE_TERM_CONVERSION_INVALID
};

enum prototype_term_conversion_reason {
	PROTOTYPE_TERM_CONVERSION_REASON_NONE = 0,
	PROTOTYPE_TERM_CONVERSION_REASON_NEUTRAL,
	PROTOTYPE_TERM_CONVERSION_REASON_OPAQUE_DEFINITION,
	PROTOTYPE_TERM_CONVERSION_REASON_UNSUPPORTED_RULE,
	PROTOTYPE_TERM_CONVERSION_REASON_EFFECT_REQUEST,
	PROTOTYPE_TERM_CONVERSION_REASON_STEP_LIMIT,
	PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT,
	PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH
};

struct prototype_term_conversion_result {
	int status;
	int reason;
	int profile;
	uint32_t left;
	uint32_t right;
	uint32_t left_observation;
	uint32_t right_observation;
	uint64_t step_limit;
	uint64_t steps_used;
	uint64_t graph_revision;
};

const char* prototype_term_conversion_status_name(int status);
const char* prototype_term_conversion_reason_name(int reason);

enum prototype_term_normalization_cache_state {
	PROTOTYPE_TERM_NORMALIZATION_CACHE_EMPTY = 0,
	PROTOTYPE_TERM_NORMALIZATION_CACHE_IN_PROGRESS,
	PROTOTYPE_TERM_NORMALIZATION_CACHE_COMPLETE
};

struct prototype_term_normalization_cache_entry {
	uint32_t term_id;
	uint32_t result_term_id;
	uint64_t graph_revision;
	int profile;
	int state;
};

struct prototype_term_normalization_cache_stats {
	uint64_t hit_count;
	uint64_t miss_count;
};

#define PROTOTYPE_TERM_REDUCE_CORE \
	(PROTOTYPE_TERM_REDUCE_BETA)
#define PROTOTYPE_TERM_REDUCE_ELIMINATORS \
	(PROTOTYPE_TERM_REDUCE_MATCH | PROTOTYPE_TERM_REDUCE_INDUCTION)
#define PROTOTYPE_TERM_REDUCE_CBPV \
	(PROTOTYPE_TERM_REDUCE_COMPUTATIONS)
#define PROTOTYPE_TERM_REDUCE_DEFAULT \
	(PROTOTYPE_TERM_REDUCE_CORE | PROTOTYPE_TERM_REDUCE_ELIMINATORS | \
		PROTOTYPE_TERM_REDUCE_CBPV)
#define PROTOTYPE_TERM_EVALUATE_DEFAULT \
	(PROTOTYPE_TERM_REDUCE_DEFAULT | PROTOTYPE_TERM_REDUCE_PURE_INTRINSICS)

struct prototype_term_reduction_options {
	unsigned flags;
	FILE* effect_output;
	struct symbol_table* symbols;
	unsigned effect_capabilities;
	int* p_effect_performed;
	/* Internal callers may distinguish a fuel stop from an invalid graph
	 * without changing the established strict evaluator ABI. */
	int* p_normalization_status;
	int* p_normalization_reason;
	uint64_t* p_steps_remaining;
	uint64_t* p_steps_used;
	prototype_term_operation_dispatch_fn operation_dispatch;
	void* operation_dispatch_context;
};

struct prototype_term_semantics {
	int layer;
	int whnf_role;
	int application_role;
	int binds_term_variable;
	int evaluates_scrutinee;
	int reduces_by_beta;
	int link_boundary;
};

struct prototype_term {
	int tag;
	union {
		struct {
			uint32_t binding_id;
		} var;
		struct {
			uint32_t owner;
			uint32_t constructor_id;
		} constructor;
		struct {
			uint32_t function;
			uint32_t argument;
		} app;
		struct {
			uint32_t binding_id;
			uint32_t body;
		} lambda;
		struct {
			uint32_t domain;
			uint32_t codomain_family;
		} pi;
		struct {
			uint32_t scrutinee;
			uint32_t first_case;
			uint32_t case_count;
			uint32_t ih_scope_id;
		} match;
		struct {
			uint32_t representation_id;
		} type_former;
		struct {
			uint32_t type_id;
			struct prototype_qualified_name identity;
		} type_declaration;
		struct {
			uint32_t view_type_id;
			struct prototype_qualified_name identity;
			uint32_t core;
			uint32_t source;
		} type_view;
			struct {
				uint32_t ih_scope_id;
				uint32_t argument;
		} induction_hypothesis;
		struct {
			uint32_t level_var;
		} universe_var;
		struct {
			int text_symbol_id;
		} text_literal;
		struct {
			int64_t value;
		} int_literal;
		struct {
			struct prototype_qualified_name name;
		} external_ref;
		struct {
			int primitive_id;
			int type_symbol_id;
		} pure_primitive;
		struct {
			int operation_id;
			uint32_t classifier;
		} effect_operation;
		struct {
			unsigned effects;
		} effect_label;
		struct {
			uint32_t binding_id;
		} effect_row_var;
		struct {
			uint32_t left;
			uint32_t right;
		} effect_row_union;
		struct {
			uint32_t binding_id;
			uint32_t body;
		} effect_row_forall;
		struct {
			int operation_id;
			uint32_t latent_row;
		} effect_row_operation;
		struct {
			uint32_t label;
			uint32_t result;
		} computation_type;
		struct {
			uint32_t computation;
		} thunk_type;
		struct {
			uint32_t value;
		} return_term;
		struct {
			uint32_t computation;
		} thunk;
		struct {
			uint32_t value;
		} force;
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

struct prototype_match_case_input {
	int case_label_symbol_id;
	uint32_t constructor_owner;
	uint32_t constructor_id;
	const struct prototype_case_binder* binders;
	uint32_t binder_count;
	uint32_t body;
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
	struct prototype_ih_scope_key key;
};

struct prototype_term_db {
	struct prototype_term* terms;
	size_t term_count;
	size_t term_capacity;

	struct prototype_match_case* cases;
	int* case_label_symbols;
	size_t case_count;
	size_t case_capacity;

	struct prototype_case_binder* case_binders;
	size_t case_binder_count;
	size_t case_binder_capacity;

	struct prototype_ih_scope* ih_scopes;
	size_t ih_scope_count;
	size_t ih_scope_capacity;

	struct prototype_computation_fold_clause
		computation_fold_clauses[PROTOTYPE_COMPUTATION_FOLD_CLAUSE_CAPACITY];
	size_t computation_fold_clause_count;

	uint32_t next_binding_id;
	uint32_t scope_bindings[PROTOTYPE_SCOPE_BINDING_CAPACITY];

	/* Runtime-only metadata. It is not part of the serialized term graph. */
	uint64_t normalization_graph_revision;
	uint32_t normalization_cache_next;
	struct prototype_term_normalization_cache_entry
		normalization_cache[PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY];
	struct prototype_term_normalization_cache_stats normalization_cache_stats;
};

struct prototype_term_definition {
	struct prototype_qualified_name name;
	uint32_t term;
	uint32_t classifier;
	int transparency;
	struct prototype_term_canonical_key canonical_key;
};

struct prototype_term_definition_env {
	const struct prototype_term_definition* definitions;
	size_t definition_count;
};

int prototype_term_semantics(
	const struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_term_semantics* p_ret
);
int prototype_term_constructor_spine_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_head,
	uint32_t* p_owner,
	uint32_t* p_constructor_id,
	uint32_t* arguments,
	uint32_t argument_capacity,
	uint32_t* p_argument_count
);
int prototype_term_classifier_view(
	const struct prototype_term_db* db,
	uint32_t classifier,
	struct prototype_term_classifier_view* p_ret
);
void prototype_term_db_init(
	struct prototype_term_db* db,
	struct prototype_term* terms,
	size_t term_capacity,
	struct prototype_match_case* cases,
	int* case_label_symbols,
	size_t case_capacity,
	struct prototype_case_binder* case_binders,
	size_t case_binder_capacity,
	struct prototype_ih_scope* ih_scopes,
	size_t ih_scope_capacity
);

uint32_t prototype_term_binding_for_scope_slot(struct prototype_term_db* db, uint32_t scope_slot);
uint32_t prototype_term_new_binding(struct prototype_term_db* db);
uint32_t prototype_term_new_ih_scope(struct prototype_term_db* db);
int prototype_term_set_ih_scope_term(
	struct prototype_term_db* db,
	uint32_t ih_scope_id,
	uint32_t match_term
);
int prototype_term_ih_scope_key(
	const struct prototype_term_db* db,
	uint32_t ih_scope_id,
	struct prototype_ih_scope_key* p_key
);
int prototype_term_var(struct prototype_term_db* db, uint32_t binding_id, uint32_t* p_ret);
int prototype_term_constructor(
	struct prototype_term_db* db,
	uint32_t owner,
	uint32_t constructor_id,
	uint32_t* p_ret
);
int prototype_term_app(struct prototype_term_db* db, uint32_t function, uint32_t argument, uint32_t* p_ret);
int prototype_term_lambda(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t body,
	uint32_t* p_ret
);
int prototype_term_match(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	const struct prototype_match_case_input* cases,
	uint32_t case_count,
	uint32_t* p_ret
);
int prototype_term_match_with_ih_scope(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	const struct prototype_match_case_input* cases,
	uint32_t case_count,
	uint32_t ih_scope_id,
	uint32_t* p_ret
);
int prototype_term_resolve_match_case(
	struct prototype_term_db* db,
	uint32_t match_term,
	uint32_t case_index,
	uint32_t constructor_owner,
	uint32_t constructor_id
);
int prototype_term_erase_constructor_view_owners(struct prototype_term_db* db);
int prototype_term_type_instance_make(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id,
	const uint32_t* args,
	uint32_t arg_count,
	uint32_t* p_ret
);
int prototype_term_type_instance_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_type_id,
	uint32_t* args,
	uint32_t* p_arg_count
);

/* Rebind provisional TYPE_FORMER declaration anchors after representation
 * interning has completed. */
int prototype_term_rebind_type_former_anchors(
	struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations
);
int prototype_term_type_instance_extend(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t instance,
	uint32_t argument,
	uint32_t* p_ret
);
int prototype_term_type_instance_is_saturated(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
);
int prototype_term_induction_hypothesis(
	struct prototype_term_db* db,
	uint32_t ih_scope_id,
	uint32_t argument,
	uint32_t* p_ret
);
int prototype_term_universe_var(struct prototype_term_db* db, uint32_t level_var, uint32_t* p_ret);
int prototype_term_primitive_text(struct prototype_term_db* db, uint32_t* p_ret);
int prototype_term_text_literal(struct prototype_term_db* db, int text_symbol_id, uint32_t* p_ret);
int prototype_term_primitive_int(struct prototype_term_db* db, uint32_t* p_ret);
int prototype_term_primitive_int64(struct prototype_term_db* db, uint32_t* p_ret);
int prototype_term_int_literal(struct prototype_term_db* db, int64_t value, uint32_t* p_ret);
int prototype_term_effect_label(struct prototype_term_db* db, unsigned effects, uint32_t* p_ret);
int prototype_term_effect_row_var(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t* p_ret
);

/* Effect rows form an idempotent commutative union. The normal form is solver
 * state, not a new Term tag: closed labels are accumulated in effects and each
 * unresolved variable or higher-order operation atom occurs at most once. */
#define PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY 512

struct prototype_effect_row_normal_form {
	unsigned effects;
	uint32_t atom_count;
	uint32_t atoms[PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY];
};

int prototype_term_effect_row_normal_form(
	const struct prototype_term_db* db,
	uint32_t row,
	struct prototype_effect_row_normal_form* p_normal
);
int prototype_term_effect_row_normal_form_includes(
	const struct prototype_effect_row_normal_form* superset,
	const struct prototype_effect_row_normal_form* subset
);
int prototype_term_effect_row_materialize_normal_form(
	struct prototype_term_db* db,
	const struct prototype_effect_row_normal_form* normal,
	uint32_t* p_ret
);
int prototype_term_effect_row_union(
	struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	uint32_t* p_ret
);
int prototype_term_effect_row_forall(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t body,
	uint32_t* p_ret
);
int prototype_term_effect_row_forall_parts(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_binder_id,
	uint32_t* p_body
);

int prototype_term_effect_row_operation(
	struct prototype_term_db* db,
	int operation_id,
	uint32_t latent_row,
	uint32_t* p_ret
);
int prototype_term_effect_row_closed_bits(
	const struct prototype_term_db* db,
	uint32_t row,
	unsigned* p_effects
);
/* Unlike classifier-view cached bits, this preserves unresolved rows as a
 * distinct result and is the authority for static purity checks. */
int prototype_term_effect_row_purity(
	const struct prototype_term_db* db,
	uint32_t row
);
int prototype_term_effect_row_residual(
	struct prototype_term_db* db,
	uint32_t row,
	unsigned handled_effects,
	uint32_t* p_residual
);
int prototype_term_computation_type(
	struct prototype_term_db* db,
	uint32_t label,
	uint32_t result,
	uint32_t* p_ret
);
int prototype_term_thunk_type(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t* p_ret
);
int prototype_term_return(
	struct prototype_term_db* db,
	uint32_t value,
	uint32_t* p_ret
);
int prototype_term_thunk(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t* p_ret
);
int prototype_term_force(
	struct prototype_term_db* db,
	uint32_t value,
	uint32_t* p_ret
);
int prototype_term_operation_request(
	struct prototype_term_db* db,
	uint32_t operation,
	uint32_t argument,
	uint32_t continuation,
	uint32_t* p_ret
);
int prototype_term_computation_fold(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t return_clause,
	const struct prototype_computation_fold_clause* clauses,
	uint32_t clause_count,
	uint32_t* p_ret
);
int prototype_term_host_type_from_source_name(const char* name, int* p_type_id);
int prototype_term_host_type_from_term_tag(int tag, int* p_type_id);
int prototype_term_host_type_from_type_expr_tag(int tag, int* p_type_id);
const char* prototype_term_host_type_source_name(int type_id);
const char* prototype_term_host_type_debug_name(int type_id);
int prototype_term_host_type_term_tag(int type_id);
int prototype_term_host_type_expr_tag(int type_id);
int prototype_term_host_type_bit_width(int type_id);
size_t prototype_term_host_type_count(void);
int prototype_term_host_type_at(size_t index, int* p_type_id);
int prototype_term_make_host_type(
	struct prototype_term_db* db,
	int type_id,
	uint32_t* p_ret
);
int prototype_intrinsic_namespace_lookup(
	const char* name,
	struct prototype_intrinsic_namespace_binding* p_binding
);
const char* prototype_intrinsic_namespace_source_name(int kind, int target_id);
const struct prototype_pure_primitive_declaration*
prototype_term_pure_primitive_declaration(int primitive_id);
const struct prototype_effect_operation_declaration*
prototype_term_effect_operation_declaration(
	int operation_id
);
int prototype_term_external_ref(
	struct prototype_term_db* db,
	struct prototype_qualified_name name,
	uint32_t* p_ret
);
int prototype_term_pure_primitive(
	struct prototype_term_db* db,
	int primitive_id,
	int type_symbol_id,
	uint32_t* p_ret
);
int prototype_term_effect_operation(
	struct prototype_term_db* db,
	int operation_id,
	uint32_t* p_ret
);

int prototype_term_effect_operation_identity(
	const struct prototype_term_db* db,
	uint32_t term_id,
	int* p_operation_id
);
int prototype_term_contains_free_binding(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t binding_id
);
/*
 * View shape equality preserves TYPE_VIEW wrappers. Bool and Two may share the
 * same core, but they are not view-shape equal unless their view/source agree.
 */
int prototype_term_view_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
/*
 * Core shape equality compares the computational core under TYPE_VIEW wrappers.
 * It is structural evidence only; callers must not use it as a typed conversion
 * unless a later transport/equality proof justifies changing views.
 */
int prototype_term_core_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
int prototype_term_core_shape_equal_under_binder(
	const struct prototype_term_db* db,
	uint32_t left_binder,
	uint32_t left,
	uint32_t right_binder,
	uint32_t right,
	int* p_equal
);
int prototype_term_source_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
/* Link comparisons use canonical keys only to select candidates. These
 * functions always validate the complete cross-database term structure. */
int prototype_term_view_shape_equal_for_link(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int* p_equal
);
int prototype_term_source_shape_equal_for_link(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int* p_equal
);
int prototype_term_core_shape_equal_for_link(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int* p_equal
);
int prototype_term_canonical_key(
	const struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_term_canonical_key* p_key
);
int prototype_term_canonical_key_with_types(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	struct prototype_term_canonical_key* p_key
);
int prototype_term_pi(
	struct prototype_term_db* db,
	uint32_t domain,
	uint32_t codomain,
	uint32_t* p_ret
);
int prototype_term_pi_family(
	struct prototype_term_db* db,
	uint32_t domain,
	uint32_t codomain_family,
	uint32_t* p_ret
);
int prototype_term_pure_family(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t body,
	uint32_t* p_family
);
int prototype_term_pure_family_lambda(
	const struct prototype_term_db* db,
	uint32_t family,
	uint32_t* p_lambda
);
/* A pure family is the canonical CBPV value
 * THUNK(LAMBDA(binder, RETURN(body))). This accessor exposes the dependent
 * value body without making consumers duplicate that wrapper traversal. */
int prototype_term_pure_family_parts(
	const struct prototype_term_db* db,
	uint32_t family,
	uint32_t* p_binder_id,
	uint32_t* p_body
);
/*
 * Rebuild an erased core graph after replacing one bound-variable handle.
 * This is the implementation primitive for beta reduction and alpha transport,
 * not a morphism between typed ContextDB objects. Typed reindexing must use
 * prototype_term_reindex with an explicit SubstitutionDB entry.
 */
int prototype_term_graph_substitute_bound_var(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t replacement,
	uint32_t* p_ret
);
struct prototype_binding_replacement {
	uint32_t binding_id;
	uint32_t replacement;
};
/* Rebuild a graph once under a simultaneous binding-handle substitution.
 * Replacement terms are final images and are never rewritten by sibling
 * entries in the same substitution. */
int prototype_term_graph_reindex_bindings(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	const struct prototype_binding_replacement* bindings,
	size_t binding_count,
	uint32_t* p_ret
);
int prototype_term_resolve_external_ref(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_qualified_name name,
	uint32_t replacement,
	uint32_t* p_ret
);

int prototype_term_normalize_complete_with_profile(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	uint32_t* p_ret
);
/* Evaluate only the supplied profile and classify the outcome. Zero permits
 * no reduction work. This does not dispatch effects. */
int prototype_term_normalize_with_profile(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	uint64_t step_limit,
	struct prototype_term_normalization_result* p_result
);
int prototype_term_nf_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
);
int prototype_term_perform_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
);
int prototype_term_compare_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	struct prototype_term_conversion_result* p_result
);
int prototype_term_compare_for_conversion(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	struct prototype_term_conversion_result* p_result
);
void prototype_term_normalization_cache_clear(struct prototype_term_db* db);
void prototype_term_notify_graph_mutation(struct prototype_term_db* db);
void prototype_term_normalization_cache_get_stats(
	const struct prototype_term_db* db,
	struct prototype_term_normalization_cache_stats* p_stats
);

void prototype_term_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
);

void prototype_term_print_debug(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
);

#endif
