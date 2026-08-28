#ifndef __PROTOTYPE_TERM_H__
#define __PROTOTYPE_TERM_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "a_program/support/schema.h"
#include "a_program/support/symbol.h"

struct prototype_term_db;
struct prototype_term_definition_env;
struct prototype_term_reduction_options;
struct prototype_type_declaration_db;
struct prototype_type_semantic_schema_db;
struct prototype_type_representation_db;
struct prototype_dimension_operator_db;

struct prototype_type_view_rebuild_context {
	const struct prototype_type_semantic_schema_db* semantic_schema;
	const struct prototype_type_representation_db* representation_db;
};

struct prototype_type_view_rebuild_context
prototype_type_view_rebuild_context_from_db(
	const struct prototype_type_declaration_db* db
);

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

#define PROTOTYPE_SCOPE_BINDING_CAPACITY 512
#define PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY 1024
#define PROTOTYPE_TERM_NORMALIZATION_CACHE_BUCKET_CAPACITY 2048
#define PROTOTYPE_COMPUTATION_FOLD_CLAUSE_CAPACITY 4096
#define PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT UINT64_C(100000)
#define PROTOTYPE_SOLVER_DEFAULT_STEP_LIMIT UINT64_C(100000)

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
	/* Classifier-only implicit quantification. The binder is erased at runtime
	 * and scopes EFFECT_ROW_VAR occurrences in body. */
	PROTOTYPE_TERM_EFFECT_ROW_FORALL = 23,
	PROTOTYPE_TERM_COMPUTATION_TYPE = 24,
	PROTOTYPE_TERM_THUNK_TYPE = 25,
	PROTOTYPE_TERM_RETURN = 26,
	PROTOTYPE_TERM_THUNK = 27,
	PROTOTYPE_TERM_FORCE = 28,
	PROTOTYPE_TERM_OPERATION_REQUEST = 29,
	PROTOTYPE_TERM_COMPUTATION_FOLD = 30,
	/* Static effect-row atom for one higher-order operation family and the
	 * latent effects of its suspended computation argument. */
	PROTOTYPE_TERM_EFFECT_ROW_OPERATION = 31,
	PROTOTYPE_TERM_RELATION_TYPE_FORMER = 32,
	PROTOTYPE_TERM_RELATION_WITNESS_FORMER = 33,
	PROTOTYPE_TERM_DIMENSION_ACTION = 34,
	PROTOTYPE_TERM_TERMINATES_TYPE_FORMER = 37,
	PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER = 38
};

#define PROTOTYPE_TERM_TAG_MAX PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER

enum prototype_term_category {
	PROTOTYPE_TERM_CATEGORY_INVALID = 0,
	PROTOTYPE_TERM_CATEGORY_VALUE = 1,
	PROTOTYPE_TERM_CATEGORY_COMPUTATION = 2,
	PROTOTYPE_TERM_CATEGORY_TYPE = 3
};

enum prototype_term_computation_kind {
	PROTOTYPE_TERM_COMPUTATION_KIND_INVALID = 0,
	PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING = 1,
	PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION = 2,
	PROTOTYPE_TERM_COMPUTATION_KIND_HANDLER = 3
};

struct prototype_term_classifier_view {
	int category;
	int computation_kind;
	int totality;
	uint32_t effect_row;
	uint32_t result;
};

enum prototype_pure_primitive_id {
	PROTOTYPE_PURE_PRIMITIVE_UNKNOWN = 0,
	PROTOTYPE_PURE_PRIMITIVE_TEXT_TO_NAT = 1,
	PROTOTYPE_PURE_PRIMITIVE_NAT_TO_TEXT = 2,
	PROTOTYPE_PURE_PRIMITIVE_INT_ADD = 3,
	PROTOTYPE_PURE_PRIMITIVE_INT_SUB = 4,
	PROTOTYPE_PURE_PRIMITIVE_INT_MUL = 5,
	PROTOTYPE_PURE_PRIMITIVE_INT_NEG = 6,
	PROTOTYPE_PURE_PRIMITIVE_INT64_ADD = 7,
	PROTOTYPE_PURE_PRIMITIVE_INT64_SUB = 8,
	PROTOTYPE_PURE_PRIMITIVE_INT64_MUL = 9,
	PROTOTYPE_PURE_PRIMITIVE_INT64_NEG = 10
};

enum prototype_effect_operation_id {
	PROTOTYPE_EFFECT_OPERATION_UNKNOWN = 0,
	PROTOTYPE_EFFECT_OPERATION_PRINT = 1,
	PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT = 2,
	PROTOTYPE_EFFECT_OPERATION_SCOPE_TEXT_ONCE = 3,
	PROTOTYPE_EFFECT_OPERATION_ABORT_TEXT = 4
};

enum prototype_effect_operation_classifier_schema {
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_INVALID = 0,
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_TEXT_TO_TEXT = 1,
	PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT = 2
};

enum prototype_effect_operation_inner_policy {
	PROTOTYPE_EFFECT_OPERATION_INNER_OPAQUE = 0,
	PROTOTYPE_EFFECT_OPERATION_INNER_SCOPED = 1
};

enum prototype_effect_operation_resumption_multiplicity {
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_MULTI_SHOT = 0,
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ONE_SHOT = 1,
	PROTOTYPE_EFFECT_OPERATION_RESUMPTION_ABORTIVE = 2
};

enum prototype_host_type_id {
	PROTOTYPE_HOST_TYPE_INVALID = 0,
	PROTOTYPE_HOST_TYPE_TEXT = 1,
	PROTOTYPE_HOST_TYPE_INT32 = 2,
	PROTOTYPE_HOST_TYPE_INT64 = 3
};

enum prototype_host_oracle_kind {
	PROTOTYPE_HOST_ORACLE_NONE = 0,
	PROTOTYPE_HOST_ORACLE_PRINT = 1,
	PROTOTYPE_HOST_ORACLE_TEXT_TO_NAT = 2,
	PROTOTYPE_HOST_ORACLE_NAT_TO_TEXT = 3,
	PROTOTYPE_HOST_ORACLE_INT_ADD = 4,
	PROTOTYPE_HOST_ORACLE_INT_SUB = 5,
	PROTOTYPE_HOST_ORACLE_INT_MUL = 6,
	PROTOTYPE_HOST_ORACLE_INT_NEG = 7
};

enum prototype_host_effect_flag {
	PROTOTYPE_HOST_EFFECT_NONE = 0,
	PROTOTYPE_HOST_EFFECT_TERMINAL = 1u << 0
};

enum prototype_effect_row_purity {
	PROTOTYPE_EFFECT_ROW_PURITY_INVALID = 0,
	PROTOTYPE_EFFECT_ROW_PURITY_PURE = 1,
	PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL = 2,
	PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED = 3
};

/* Termination is independent of the effect row. UNKNOWN is a solver outcome
 * and is never stored in a COMPUTATION_TYPE Term. */
enum prototype_computation_totality {
	PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN = 0,
	PROTOTYPE_COMPUTATION_TOTALITY_TOTAL = 1,
	PROTOTYPE_COMPUTATION_TOTALITY_MAY_DIVERGE = 2
};

int prototype_computation_totality_join(int left, int right);

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
	unsigned required_host_effects;
	uint32_t arity;
	int inner_policy;
	int resumption_multiplicity;
};

enum prototype_intrinsic_namespace_binding_kind {
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_UNKNOWN = 0,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_HOST_TYPE = 1,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE = 2,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION = 3,
	PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_COMPUTATION_FOLD_RETURN = 4
};

struct prototype_intrinsic_namespace_binding {
	const char* source_name;
	int kind;
	int target_id;
};

/* Immutable language/runtime boundary selected for one compilation.  The
 * descriptor, rather than a source spelling or an expected type, owns the
 * principal representation of an unsuffixed integer literal. */
struct prototype_intrinsic_environment {
	const struct prototype_intrinsic_namespace_binding* namespace_bindings;
	size_t namespace_binding_count;
	const struct prototype_pure_primitive_declaration* pure_primitives;
	size_t pure_primitive_count;
	const struct prototype_effect_operation_declaration* effect_operations;
	size_t effect_operation_count;
	int default_integer_host_type;
};

enum prototype_term_layer {
	PROTOTYPE_TERM_LAYER_LAMBDA_CORE = 1,
	PROTOTYPE_TERM_LAYER_ELIMINATOR = 2,
	PROTOTYPE_TERM_LAYER_TYPE_FORMER = 3,
	PROTOTYPE_TERM_LAYER_DATA = 4,
	PROTOTYPE_TERM_LAYER_LINK = 5,
	PROTOTYPE_TERM_LAYER_PURE_PRIMITIVE = 6,
	PROTOTYPE_TERM_LAYER_EFFECT_OPERATION = 7,
	PROTOTYPE_TERM_LAYER_INDUCTION = 8,
	PROTOTYPE_TERM_LAYER_DIMENSION_ACTION = 9
};

enum prototype_term_whnf_role {
	PROTOTYPE_TERM_WHNF_NEUTRAL = 1,
	PROTOTYPE_TERM_WHNF_INTRODUCTION = 2,
	PROTOTYPE_TERM_WHNF_ELIMINATOR = 3,
	PROTOTYPE_TERM_WHNF_ATOMIC = 4
};

enum prototype_term_application_role {
	PROTOTYPE_TERM_APPLICATION_NONE = 0,
	/* The function may expose a lambda after permitted normalization. */
	PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION = 1,
	/* The APP chain is assembling fields under a constructor telescope. */
	PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION = 2,
	/* Pure type formation exposes a suspended family lambda for compile-time
	 * evaluation. This role has no distinct Core APP representation. */
	PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION = 3
};

enum prototype_term_definition_transparency {
	PROTOTYPE_TERM_DEFINITION_OPAQUE = 1,
	PROTOTYPE_TERM_DEFINITION_TRANSPARENT = 2
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
	PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF = 2,
	PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF = 3
};

/* A kernel normalization attempt may stop without establishing a normal form.
 * This status is intentionally separate from runtime operation dispatch. */
enum prototype_term_normalization_status {
	PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE = 1,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT = 2,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED = 3,
	PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID = 4
};

struct prototype_term_normalization_result {
	int status;
	uint32_t term_id;
	uint64_t step_limit;
	uint64_t steps_used;
	uint64_t graph_revision;
};

struct prototype_term_normalization_machine;

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
	uint64_t semantic_revision;
	int profile;
	int state;
};

struct prototype_term_normalization_cache_stats {
	uint64_t hit_count;
	uint64_t miss_count;
	uint64_t probe_count;
	uint64_t eviction_count;
	uint64_t invalidation_count;
	uint64_t graph_mutation_invalidation_count;
	uint64_t ih_scope_invalidation_count;
	uint64_t type_former_invalidation_count;
	uint64_t empty_cache_invalidation_count;
	uint64_t semantic_revision_miss_count;
};

struct prototype_term_intern_stats {
	uint64_t formation_request_count;
	uint64_t unique_term_count;
	uint64_t bucket_probe_count;
	uint64_t exact_probe_count;
	uint64_t alpha_compare_count;
	uint64_t alpha_compare_node_visit_count;
	uint64_t max_alpha_bucket_probe_count;
	uint64_t index_rebuild_count;
	uint64_t formation_requests_by_tag[PROTOTYPE_TERM_TAG_MAX + 1];
	uint64_t unique_terms_by_tag[PROTOTYPE_TERM_TAG_MAX + 1];
	uint64_t bucket_probes_by_tag[PROTOTYPE_TERM_TAG_MAX + 1];
	uint64_t alpha_compares_by_tag[PROTOTYPE_TERM_TAG_MAX + 1];
};

#define PROTOTYPE_TYPE_INSTANCE_CACHE_CAPACITY 4096

struct prototype_type_instance_cache_entry {
	int present;
	uint64_t semantic_revision;
	uint32_t type_id;
	uint32_t arg_count;
	uint32_t args[16];
	uint32_t result;
};

struct prototype_type_instance_cache_stats {
	uint64_t hit_count;
	uint64_t miss_count;
	uint64_t collision_count;
	uint64_t stale_revision_count;
};

/* Immutable operational data projected by compilation. It contains only the
 * Core identities required by host-backed reduction and no classifier,
 * Context, constructor telescope, or proof information. */
struct prototype_term_reduction_environment {
	uint32_t system_nat_owner;
	uint32_t system_nat_zero_constructor;
	uint32_t system_nat_succ_constructor;
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
	const struct prototype_term_reduction_environment* reduction_environment;
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
	uint64_t* p_induction_hypothesis_reductions;
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

/* A child role describes one structural edge in the shared Term graph. It is
 * independent of typing occurrences, source syntax, and evaluation strategy. */
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
	PROTOTYPE_TERM_CHILD_EFFECT_OPERATION_CLASSIFIER = 11,
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
			/* Rebuild anchor for the process-local representation cache. This is
			 * not nominal identity: structurally shared formers may use any
			 * declaration with the same representation. */
			uint32_t declaration_type_id;
			uint32_t representation_id;
			/* Number of constructor ordinals in this erased algebra signature.
			 * This is operational reduction data, not a source declaration or
			 * classifier fact. */
			uint32_t constructor_count;
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
			int totality;
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
		struct {
			uint32_t source;
			uint32_t operator_id;
		} dimension_action;
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
	/* When the recursive Match scrutinizes a bound variable, its branch bodies
	 * retain that binding until iota reduction. Each recursive invocation then
	 * substitutes the current scrutinee, not the first invocation's value. */
	uint32_t scrutinee_binding_id;
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
	uint32_t normalization_cache_buckets[
		PROTOTYPE_TERM_NORMALIZATION_CACHE_BUCKET_CAPACITY
	];
	uint32_t normalization_cache_next_entry[
		PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY
	];
	struct prototype_term_normalization_cache_stats normalization_cache_stats;
	int type_instance_cache_enabled;
	struct prototype_type_instance_cache_entry* type_instance_cache;
	size_t type_instance_cache_capacity;
	struct prototype_type_instance_cache_stats type_instance_cache_stats;

	/* Runtime-only canonical identity index. The Term graph remains the sole
	 * semantic owner; this index is a rebuildable projection of that graph. */
	struct prototype_term_canonical_key* intern_keys;
	uint32_t* intern_canonical_ids;
	uint32_t* intern_next;
	uint32_t* intern_buckets;
	uint64_t* intern_exact_hashes;
	uint32_t* intern_exact_next;
	uint32_t* intern_exact_buckets;
	uint32_t* effect_row_variable_terms;
	size_t effect_row_variable_count;
	size_t effect_row_variable_capacity;
	int effect_row_variable_index_dirty;
	size_t intern_entry_capacity;
	size_t intern_bucket_count;
	size_t intern_indexed_count;
	int intern_index_dirty;
	struct prototype_term_intern_stats intern_stats;
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
/* Project through nominal TYPE_VIEW boundaries to the context-free Core term.
 * This is a structural projection, not a typed conversion between views. */
int prototype_term_core_projection(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_core_term
);
int prototype_term_child_count(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_count
);
int prototype_term_child_at(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t child_index,
	struct prototype_term_child* p_child
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
void prototype_term_db_dispose_runtime_state(struct prototype_term_db* db);
int prototype_term_effect_row_variable_terms(
	struct prototype_term_db* db,
	const uint32_t** p_terms,
	size_t* p_count
);
void prototype_term_intern_get_stats(
	const struct prototype_term_db* db,
	struct prototype_term_intern_stats* p_stats
);

int prototype_term_db_append_relocated(
	struct prototype_term_db* target,
	const struct prototype_term_db* source,
	const uint32_t* type_relocation,
	size_t type_relocation_count,
	const uint32_t* binding_relocation,
	size_t binding_relocation_count,
	uint32_t universe_offset,
	const uint32_t* representation_relocation,
	size_t representation_relocation_count,
	const uint32_t* dimension_operator_relocation,
	size_t dimension_operator_relocation_count,
	const uint32_t* source_order,
	size_t source_order_count,
	uint32_t* term_relocation,
	size_t term_relocation_capacity
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
int prototype_term_canonicalize_type_former_references(
	struct prototype_term_db* db
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
int prototype_term_relation_type(
	struct prototype_term_db* db,
	uint32_t left_classifier,
	uint32_t right_classifier,
	uint32_t left_endpoint,
	uint32_t right_endpoint,
	uint32_t* p_ret
);
int prototype_term_relation_witness(
	struct prototype_term_db* db,
	uint32_t left_endpoint,
	uint32_t right_endpoint,
	uint32_t* p_ret
);
int prototype_term_relation_type_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_left_classifier,
	uint32_t* p_right_classifier,
	uint32_t* p_left_endpoint,
	uint32_t* p_right_endpoint
);
int prototype_term_relation_witness_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_left_endpoint,
	uint32_t* p_right_endpoint
);
int prototype_term_terminates_type(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t* p_ret
);
int prototype_term_terminates_witness(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t* p_ret
);
int prototype_term_terminates_type_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_computation
);
int prototype_term_terminates_witness_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_computation
);
int prototype_term_dimension_action(
	struct prototype_term_db* db,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t source,
	uint32_t operator_id,
	uint32_t* p_ret
);
int prototype_term_dimension_action_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_source,
	uint32_t* p_operator_id
);
int prototype_term_text_literal(struct prototype_term_db* db, int text_symbol_id, uint32_t* p_ret);
int prototype_term_primitive_int(struct prototype_term_db* db, uint32_t* p_ret);
int prototype_term_int_literal(struct prototype_term_db* db, int64_t value, uint32_t* p_ret);
int prototype_term_effect_row_empty(
	struct prototype_term_db* db,
	uint32_t* p_ret
);
int prototype_term_effect_row_var(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t* p_ret
);

/* Effect rows form an idempotent commutative union. The normal form is solver
 * state, not a new Term tag. Each stable operation or unresolved row atom
 * occurs at most once; the empty row has no atoms. */
#define PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY 512

struct prototype_effect_row_normal_form {
	uint32_t atom_count;
	uint32_t atoms[PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY];
};

int prototype_term_effect_row_normal_form(
	const struct prototype_term_db* db,
	uint32_t row,
	struct prototype_effect_row_normal_form* p_normal
);
int prototype_term_effect_row_normal_form_includes(
	const struct prototype_term_db* db,
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

int prototype_term_effect_row_operation(
	struct prototype_term_db* db,
	int operation_id,
	uint32_t latent_row,
	uint32_t* p_ret
);
int prototype_term_effect_row_is_closed(
	const struct prototype_term_db* db,
	uint32_t row
);
/* Purity is derived from the normalized row rather than cached membership. */
int prototype_term_effect_row_purity(
	const struct prototype_term_db* db,
	uint32_t row
);
int prototype_term_effect_row_residual(
	struct prototype_term_db* db,
	uint32_t row,
	uint32_t handled_row,
	uint32_t* p_residual
);
int prototype_term_computation_type(
	struct prototype_term_db* db,
	uint32_t label,
	uint32_t result,
	int totality,
	uint32_t* p_ret
);
int prototype_term_total_computation_type(
	struct prototype_term_db* db,
	uint32_t label,
	uint32_t result,
	uint32_t* p_ret
);
int prototype_term_computation_type_is_pure_total(
	const struct prototype_term_db* db,
	uint32_t computation_type
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
int prototype_term_host_type_from_source_name(
	const struct prototype_intrinsic_environment* environment,
	const char* name,
	int* p_type_id
);
int prototype_term_host_type_from_term_tag(int tag, int* p_type_id);
int prototype_term_host_type_from_type_expr_tag(int tag, int* p_type_id);
const char* prototype_term_host_type_debug_name(int type_id);
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
	const struct prototype_intrinsic_environment* environment,
	const char* name,
	struct prototype_intrinsic_namespace_binding* p_binding
);
const char* prototype_intrinsic_namespace_source_name(
	const struct prototype_intrinsic_environment* environment,
	int kind,
	int target_id
);
const struct prototype_intrinsic_environment*
prototype_default_intrinsic_environment(void);
uint64_t prototype_intrinsic_environment_fingerprint(
	const struct prototype_intrinsic_environment* environment
);
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
int prototype_term_effect_operation_classifier_has_suspended_argument(
	const struct prototype_term_db* db,
	uint32_t classifier,
	int* p_has_suspended_argument
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
int prototype_term_view_shape_equal_under_binders(
	const struct prototype_term_db* db,
	const uint32_t* left_binders,
	const uint32_t* right_binders,
	size_t binder_count,
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
int prototype_term_core_shape_equal_under_binders(
	const struct prototype_term_db* db,
	const uint32_t* left_binders,
	const uint32_t* right_binders,
	size_t binder_count,
	uint32_t left,
	uint32_t right,
	int* p_equal
);
int prototype_term_core_shape_equal_under_binders_and_ih_scope(
	const struct prototype_term_db* db,
	const uint32_t* left_binders,
	const uint32_t* right_binders,
	size_t binder_count,
	uint32_t left_ih_scope,
	uint32_t right_ih_scope,
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
	struct prototype_type_view_rebuild_context type_views,
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t replacement,
	uint32_t* p_ret
);
/* Rebuild a graph after replacing one exact interned subterm. The replacement
 * is structural and does not assert typed conversion between the two terms. */
int prototype_term_graph_replace_exact(
	struct prototype_term_db* db,
	struct prototype_type_view_rebuild_context type_views,
	uint32_t term_id,
	uint32_t exact_term,
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
	struct prototype_type_view_rebuild_context type_views,
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
/* A process-local resumable WHNF producer. Its state is the current residual
 * TermDB graph, not a replay cursor. Every exhausted advance publishes a valid
 * residual term, so completed reductions are retained in Core. The evaluator
 * may revisit unreduced outer traversal because this first machine does not
 * persist a separate evaluation stack. */
int prototype_term_normalization_machine_create(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	struct prototype_term_normalization_machine** p_machine
);
int prototype_term_normalization_machine_advance(
	struct prototype_term_normalization_machine* machine,
	uint64_t step_limit,
	struct prototype_term_normalization_result* p_result
);
uint32_t prototype_term_normalization_machine_current(
	const struct prototype_term_normalization_machine* machine
);
void prototype_term_normalization_machine_destroy(
	struct prototype_term_normalization_machine* machine
);
/* Project the value returned by a pure computation without selecting a
 * neutral Match branch. Zero returns a value graph, one means that the pure
 * result is still opaque, and -1 reports malformed Core data. The caller is
 * responsible for establishing an empty effect row. */
int prototype_term_project_pure_computation_value(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t computation,
	uint64_t step_limit,
	uint32_t* p_value
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


void prototype_term_print_debug(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* intrinsic_environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
);

#endif
