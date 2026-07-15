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
#define PROTOTYPE_PI_UNUSED_BINDER_ID (UINT32_MAX - 2)
#define PROTOTYPE_SCOPE_BINDER_CAPACITY 512
#define PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY 1024

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
	PROTOTYPE_TERM_OPERATION,
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
	PROTOTYPE_TERM_BIND,
	PROTOTYPE_TERM_OPERATION_REQUEST,
	PROTOTYPE_TERM_HANDLER,
	PROTOTYPE_TERM_HANDLE,
	PROTOTYPE_TERM_HANDLER_TYPE
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

enum prototype_term_operation_id {
	PROTOTYPE_OPERATION_UNKNOWN = 0,
	PROTOTYPE_OPERATION_PRINT,
	PROTOTYPE_OPERATION_TEXT_TO_NAT,
	PROTOTYPE_OPERATION_NAT_TO_TEXT,
	PROTOTYPE_OPERATION_INT_ADD,
	PROTOTYPE_OPERATION_INT_SUB,
	PROTOTYPE_OPERATION_INT_MUL,
	PROTOTYPE_OPERATION_INT_NEG,
	PROTOTYPE_OPERATION_INT64_ADD,
	PROTOTYPE_OPERATION_INT64_SUB,
	PROTOTYPE_OPERATION_INT64_MUL,
	PROTOTYPE_OPERATION_INT64_NEG
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

#define PROTOTYPE_OPERATION_MAX_ARITY 2

/* Operation declarations are language-level interface data. Runtime
 * implementations are intentionally not part of this classifier contract. */
struct prototype_operation_declaration {
	int operation_id;
	const char* source_name;
	unsigned effects;
	uint32_t arity;
	int argument_types[PROTOTYPE_OPERATION_MAX_ARITY];
	int result_type;
};

enum prototype_term_layer {
	PROTOTYPE_TERM_LAYER_LAMBDA_CORE = 1,
	PROTOTYPE_TERM_LAYER_ELIMINATOR,
	PROTOTYPE_TERM_LAYER_TYPE_FORMER,
	PROTOTYPE_TERM_LAYER_DATA,
	PROTOTYPE_TERM_LAYER_LINK,
	PROTOTYPE_TERM_LAYER_OPERATION,
	PROTOTYPE_TERM_LAYER_INDUCTION
};

enum prototype_term_whnf_role {
	PROTOTYPE_TERM_WHNF_NEUTRAL = 1,
	PROTOTYPE_TERM_WHNF_INTRODUCTION,
	PROTOTYPE_TERM_WHNF_ELIMINATOR,
	PROTOTYPE_TERM_WHNF_ATOMIC
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
	/* CBPV cut elimination: force/thunk, bind/return, and graph handlers.
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
	prototype_term_operation_dispatch_fn operation_dispatch;
	void* operation_dispatch_context;
};

struct prototype_term_semantics {
	int layer;
	int whnf_role;
	int binds_term_variable;
	int evaluates_scrutinee;
	int reduces_by_beta;
	int link_boundary;
};

struct prototype_term {
	int tag;
	union {
		struct {
			uint32_t binder_id;
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
			uint32_t binder_id;
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
			uint32_t frame_id;
		} match;
		struct {
			uint32_t representation_id;
		} type_former;
		struct {
			uint32_t type_id;
		} type_declaration;
		struct {
			uint32_t view_type_id;
			uint32_t core;
			uint32_t source;
		} type_view;
			struct {
				uint32_t frame_id;
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
			int operation_id;
			int symbol_id;
			int type_symbol_id;
		} operation;
		struct {
			unsigned effects;
		} effect_label;
		struct {
			uint32_t binder_id;
		} effect_row_var;
		struct {
			uint32_t left;
			uint32_t right;
		} effect_row_union;
		struct {
			uint32_t binder_id;
			uint32_t body;
		} effect_row_forall;
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
			uint32_t computation;
			uint32_t continuation;
		} bind;
		struct {
			uint32_t operation;
			uint32_t argument;
			uint32_t continuation;
		} operation_request;
		struct {
			uint32_t operation;
			uint32_t return_clause;
			uint32_t operation_clause;
		} handler;
		struct {
			uint32_t handler;
			uint32_t computation;
		} handle;
		struct {
			uint32_t operation;
			uint32_t input_computation;
			uint32_t output_computation;
		} handler_type;
	} as;
	};

struct prototype_match_case {
	uint32_t constructor_owner;
	uint32_t constructor_id;
	uint32_t first_binder;
	uint32_t binder_count;
	uint32_t body;
};

struct prototype_case_binder {
	uint32_t binder_id;
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

struct prototype_match_frame_key {
	struct prototype_term_canonical_key match_key;
	uint32_t case_count;
	int is_linkable;
};

struct prototype_match_frame {
	uint32_t match_term;
	struct prototype_match_frame_key key;
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

	struct prototype_match_frame* match_frames;
	size_t match_frame_count;
	size_t match_frame_capacity;

	uint32_t next_binder_id;
	uint32_t scope_binders[PROTOTYPE_SCOPE_BINDER_CAPACITY];

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

int prototype_term_semantics(int tag, struct prototype_term_semantics* p_ret);
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
	struct prototype_match_frame* match_frames,
	size_t match_frame_capacity
);

uint32_t prototype_term_binder_for_scope_slot(struct prototype_term_db* db, uint32_t scope_slot);
uint32_t prototype_term_fresh_binder(struct prototype_term_db* db);
uint32_t prototype_term_new_match_frame(struct prototype_term_db* db);
int prototype_term_set_match_frame_term(
	struct prototype_term_db* db,
	uint32_t frame_id,
	uint32_t match_term
);
int prototype_term_match_frame_key(
	const struct prototype_term_db* db,
	uint32_t frame_id,
	struct prototype_match_frame_key* p_key
);
int prototype_term_var(struct prototype_term_db* db, uint32_t binder_id, uint32_t* p_ret);
int prototype_term_constructor(
	struct prototype_term_db* db,
	uint32_t owner,
	uint32_t constructor_id,
	uint32_t* p_ret
);
int prototype_term_app(struct prototype_term_db* db, uint32_t function, uint32_t argument, uint32_t* p_ret);
int prototype_term_lambda(
	struct prototype_term_db* db,
	uint32_t binder_id,
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
int prototype_term_match_with_frame(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	const struct prototype_match_case_input* cases,
	uint32_t case_count,
	uint32_t frame_id,
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
	uint32_t frame_id,
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
	uint32_t binder_id,
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
	uint32_t binder_id,
	uint32_t body,
	uint32_t* p_ret
);
int prototype_term_effect_row_forall_parts(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_binder_id,
	uint32_t* p_body
);
int prototype_term_effect_row_closed_bits(
	const struct prototype_term_db* db,
	uint32_t row,
	unsigned* p_effects
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
int prototype_term_bind(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t continuation,
	uint32_t* p_ret
);
int prototype_term_operation_request(
	struct prototype_term_db* db,
	uint32_t operation,
	uint32_t argument,
	uint32_t continuation,
	uint32_t* p_ret
);
int prototype_term_handler(
	struct prototype_term_db* db,
	uint32_t operation,
	uint32_t return_clause,
	uint32_t operation_clause,
	uint32_t* p_ret
);
int prototype_term_handle(
	struct prototype_term_db* db,
	uint32_t handler,
	uint32_t computation,
	uint32_t* p_ret
);
int prototype_term_handler_type(
	struct prototype_term_db* db,
	uint32_t operation,
	uint32_t input_computation,
	uint32_t output_computation,
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
const struct prototype_operation_declaration* prototype_term_operation_declaration(
	int operation_id
);
int prototype_term_operation_from_source_name(const char* name, int* p_operation_id);
int prototype_term_external_ref(
	struct prototype_term_db* db,
	struct prototype_qualified_name name,
	uint32_t* p_ret
);
int prototype_term_operation(
	struct prototype_term_db* db,
	int operation_id,
	int symbol_id,
	int type_symbol_id,
	uint32_t* p_ret
);
int prototype_term_contains_free_binder(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t binder_id
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
int prototype_term_source_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
int prototype_term_view_shape_equal_cross_db(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int ignore_match_frames,
	int* p_equal
);
int prototype_term_core_shape_equal_cross_db(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int ignore_match_frames,
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
	uint32_t binder_id,
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
int prototype_term_substitute(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t binder_id,
	uint32_t replacement,
	uint32_t* p_ret
);
int prototype_term_resolve_external_ref(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_qualified_name name,
	uint32_t replacement,
	uint32_t* p_ret
);

int prototype_term_whnf(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t* p_ret
);
int prototype_term_whnf_with_definitions(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t term_id,
	uint32_t* p_ret
);
int prototype_term_whnf_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
);
int prototype_term_whnf_with_profile(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	uint32_t* p_ret
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
int prototype_term_normalization_equal(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
int prototype_term_normalization_equal_with_definitions(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
int prototype_term_normalization_equal_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
int prototype_term_normalization_equal_with_profile(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
void prototype_term_normalization_cache_clear(struct prototype_term_db* db);
void prototype_term_notify_graph_mutation(struct prototype_term_db* db);
void prototype_term_normalization_cache_get_stats(
	const struct prototype_term_db* db,
	struct prototype_term_normalization_cache_stats* p_stats
);

int prototype_term_execute_with_default_host_handler(
	FILE* output,
	struct symbol_table* symbols,
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_ret
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
