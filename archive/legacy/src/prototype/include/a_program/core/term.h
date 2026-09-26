#ifndef __PROTOTYPE_TERM_H__
#define __PROTOTYPE_TERM_H__

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "a_program/core/conversion.h"
#include "a_program/core/graph.h"
#include "a_program/core/intrinsic.h"
#include "a_program/core/request.h"
#include "a_program/support/schema.h"
#include "a_program/support/symbol.h"

struct prototype_term_db;
struct prototype_term_definition_env;
struct prototype_term_reduction_options;
struct prototype_dimension_operator_db;

/* Core-only construction view. Variable-length binders are copied into the
 * TermDB by prototype_term_match(); this borrowed pointer never crosses the
 * detached C/T protocol boundary. */
struct prototype_match_case_input {
	int case_label_symbol_id;
	uint32_t constructor_owner;
	uint32_t constructor_id;
	const struct prototype_case_binder* binders;
	uint32_t binder_count;
	uint32_t body;
};

/* Runtime-only dispatch for an OPERATION_REQUEST. Returning 1 supplies a
 * result, 0 leaves the request unhandled, and -1 reports a runtime failure. */
typedef int (*prototype_term_operation_dispatch_fn)(
	void* context,
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_term_reduction_options* options,
	uint32_t operation,
	uint32_t argument,
	uint32_t* p_result,
	unsigned depth
);

/* Return 1 when the binding satisfies the caller's query, 0 when it does not,
 * and -1 when the query cannot be evaluated. Core owns lexical-scope traversal;
 * callers may interpret a free binding using Layer T state without exposing that
 * state to the Term graph. */
typedef int (*prototype_term_free_binding_predicate_fn)(
	void* context,
	uint32_t binding_id
);

#define PROTOTYPE_SCOPE_BINDING_CAPACITY 512
#define PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY 1024
#define PROTOTYPE_TERM_NORMALIZATION_CACHE_BUCKET_CAPACITY 2048
#define PROTOTYPE_COMPUTATION_FOLD_CLAUSE_CAPACITY 4096
#define PROTOTYPE_NORMALIZATION_DEFAULT_STEP_LIMIT UINT64_C(100000)
#define PROTOTYPE_SOLVER_DEFAULT_STEP_LIMIT UINT64_C(100000)

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
	PROTOTYPE_TERM_APPLICATION_TYPE_FAMILY_CALCULATION = 3
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
	PROTOTYPE_TERM_REDUCE_TYPE_EXPRESSION = 1u << 5,
	PROTOTYPE_TERM_PERFORM_HOST_EFFECT = 1u << 6,
	/* Deterministic host intrinsics with an empty effect row are computation
	 * reductions. They are available to execution, never to type conversion. */
	PROTOTYPE_TERM_REDUCE_PURE_INTRINSICS = 1u << 7
};

/*
 * A profile specifies the semantic layer at which a weak-head result is
 * observed.  The profiles intentionally exclude host evaluation and effects.
 */
struct prototype_term_normalization_result {
	int status;
	uint32_t term_id;
	uint64_t step_limit;
	uint64_t steps_used;
	uint64_t graph_revision;
};

struct prototype_term_normalization_machine;

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
	uint64_t probe_count;
	uint64_t eviction_count;
	uint64_t invalidation_count;
	uint64_t graph_mutation_invalidation_count;
	uint64_t ih_scope_invalidation_count;
	uint64_t type_former_invalidation_count;
	uint64_t empty_cache_invalidation_count;
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

/*
 * A structural-key caller may replace process-local identity payloads with a
 * stable token. Core owns the traversal and alpha treatment; the caller owns
 * the meaning of symbols, type representations, and dimension operators.
 * The callback is pure and cannot expose a typing store to Core reduction.
 */
enum prototype_term_structural_identity_kind {
	PROTOTYPE_TERM_STRUCTURAL_IDENTITY_SYMBOL = 1,
	PROTOTYPE_TERM_STRUCTURAL_IDENTITY_TYPE_REPRESENTATION = 2,
	PROTOTYPE_TERM_STRUCTURAL_IDENTITY_DIMENSION_OPERATOR = 3
};

typedef int (*prototype_term_structural_identity_resolver_fn)(
	const void* context,
	int kind,
	uint32_t local_identity,
	uint64_t* p_stable_token
);

struct prototype_term_structural_identity_domain {
	const void* context;
	prototype_term_structural_identity_resolver_fn resolve;
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
	uint32_t next_universe_level_id;
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
	int transparency;
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
uint32_t prototype_term_new_universe_level(struct prototype_term_db* db);
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
int prototype_term_type_former(
	struct prototype_term_db* db,
	uint32_t representation_id,
	uint32_t constructor_count,
	uint32_t* p_ret
);
int prototype_term_type_declaration(
	struct prototype_term_db* db,
	struct prototype_qualified_name identity,
	uint32_t* p_ret
);
int prototype_term_type_view(
	struct prototype_term_db* db,
	struct prototype_qualified_name identity,
	uint32_t core,
	uint32_t source,
	uint32_t* p_ret
);
int prototype_term_nominal_type_instance_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_qualified_name* p_identity,
	uint32_t* args,
	uint32_t* p_arg_count
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
	uint32_t left_type_term,
	uint32_t right_type_term,
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
	uint32_t* p_left_type_term,
	uint32_t* p_right_type_term,
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
int prototype_term_host_type_from_term_tag(int tag, int* p_type_id);
int prototype_term_host_type_bit_width(int type_id);
size_t prototype_term_host_type_count(void);
int prototype_term_host_type_at(size_t index, int* p_type_id);
int prototype_term_make_host_type(
	struct prototype_term_db* db,
	int type_id,
	uint32_t* p_ret
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
int prototype_term_any_free_binding(
	const struct prototype_term_db* db,
	uint32_t term_id,
	prototype_term_free_binding_predicate_fn predicate,
	void* context,
	int* p_found
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
	uint32_t left,
	const struct prototype_term_db* right_db,
	uint32_t right,
	int* p_equal
);
int prototype_term_core_shape_equal_for_link(
	const struct prototype_term_db* left_db,
	uint32_t left,
	const struct prototype_term_db* right_db,
	uint32_t right,
	int* p_equal
);
int prototype_term_canonical_key(
	const struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_term_canonical_key* p_key
);
int prototype_term_structural_key_in_domain(
	const struct prototype_term_db* db,
	uint32_t term_id,
	const struct prototype_term_structural_identity_domain* domain,
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
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t replacement,
	uint32_t* p_ret
);
/* Rebuild a graph after replacing one exact interned subterm. The replacement
 * is structural and does not assert typed conversion between the two terms. */
int prototype_term_graph_replace_exact(
	struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t exact_term,
	uint32_t replacement,
	uint32_t* p_ret
);
/* Replace one exact subgraph without descending through an existing
 * TYPE_VIEW. A view is the nominal boundary of a typed occurrence; its source
 * provenance and erased core are not rewritten as incidental descendants. */
int prototype_term_graph_replace_exact_outside_type_views(
	struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t exact_term,
	uint32_t replacement,
	uint32_t* p_ret
);
/* Rebuild a graph once under a simultaneous binding-handle substitution.
 * Replacement terms are final images and are never rewritten by sibling
 * entries in the same substitution. */
int prototype_term_graph_reindex_bindings(
	struct prototype_term_db* db,
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
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	uint32_t* p_ret
);
/* Evaluate only the supplied profile and classify the outcome. Zero permits
 * no reduction work. This does not dispatch effects. */
int prototype_term_normalize_with_profile(
	struct prototype_term_db* db,
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
/* Project a value under RETURN by structural reduction without selecting a
 * neutral Match branch or dispatching effects. Zero returns a value graph,
 * one means that the result is still opaque, and -1 reports malformed Core
 * data. This operation does not establish purity, totality, or typehood. */
int prototype_term_project_structural_return_value(
	struct prototype_term_db* db,
	uint32_t computation,
	uint64_t step_limit,
	uint32_t* p_value
);
int prototype_term_nf_with_options(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
);
int prototype_term_perform_with_options(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
);
int prototype_term_compare_with_options(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	struct prototype_term_conversion_result* p_result
);
int prototype_term_compare_for_conversion(
	struct prototype_term_db* db,
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
#endif
