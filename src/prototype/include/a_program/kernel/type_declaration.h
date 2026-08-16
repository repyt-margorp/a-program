#ifndef __PROTOTYPE_TYPE_DECLARATION_H__
#define __PROTOTYPE_TYPE_DECLARATION_H__

#include <stddef.h>
#include <stdint.h>

#include "a_program/support/schema.h"

enum prototype_identity_computation_rule {
	PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID = 0,
	PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT = 1,
	PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN = 2,
	PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE = 3,
	PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT = 4,
	PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INDEXED_HIGHER_LIFT = 5,
	PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE = 6,
	PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_FIBER = 7
};

struct prototype_term_db;
struct prototype_context_db;
struct prototype_term_reduction_environment;
struct symbol_table;

/*
 * Type declarations are source-derived formation metadata used while lowering
 * AST into graph nodes. They provide the named type view: declaration identity,
 * constructor names, and constructor ordinals. The nameless computational side
 * lives in the term graph as TYPE_FORMER/APP/TYPE_VIEW core spines, and
 * classifier facts live in JudgementDB.
 */

enum prototype_type_expr_tag {
	PROTOTYPE_TYPE_EXPR_UNIVERSE = 1,
	PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR = 2,
	PROTOTYPE_TYPE_EXPR_SELF = 3,
	PROTOTYPE_TYPE_EXPR_VAR = 4,
	PROTOTYPE_TYPE_EXPR_NAME = 5,
	PROTOTYPE_TYPE_EXPR_APP = 6,
	PROTOTYPE_TYPE_EXPR_ARROW = 7,
	PROTOTYPE_TYPE_EXPR_PI = 8,
	PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE = 9,
	PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM = 10,
	PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT = 11,
	PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT = 12,
	PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64 = 13,
	/* Source-local value selected through a named type view, such as Nat.zero. */
	PROTOTYPE_TYPE_EXPR_LOCAL_TYPE_MEMBER = 14
};

struct prototype_type_representation_fingerprint {
	/*
	 * Structural fingerprint for the core shape layer. This is not declaration
	 * identity and must not be used as typed conversion evidence by itself.
	 */
	uint64_t hash;
	uint32_t node_count;
	uint32_t parameter_count;
	uint32_t index_count;
	uint32_t constructor_count;
	uint32_t bound_binder_count;
	uint32_t free_binder_count;
	int has_local_universe_reference;
	int has_name_reference;
};

struct prototype_type_expr {
	int tag;
	union {
		struct {
			uint32_t level;
		} universe;
		struct {
			uint32_t level_var;
		} universe_var;
	struct {
		uint32_t binding_id;
		int symbol_id;
	} var;
	struct {
		int symbol_id;
	} name;
		struct {
			struct prototype_qualified_name name;
			struct prototype_type_representation_fingerprint representation_fingerprint;
		} imported_type;
		struct {
			struct prototype_qualified_name name;
		} external_term;
		struct {
			int owner_symbol_id;
			int member_symbol_id;
		} local_type_member;
		struct {
			uint32_t function;
			uint32_t argument;
		} app;
		struct {
			uint32_t domain;
			uint32_t codomain;
		} arrow;
		struct {
			uint32_t binding_id;
			int symbol_id;
			uint32_t domain;
			uint32_t codomain;
		} pi;
	} as;
};

struct prototype_type_parameter_declaration {
	uint32_t binding_id;
	int name_symbol_id;
	uint32_t type_expr;
};

struct prototype_type_constructor_readback {
	uint32_t first_field_type;
	uint32_t field_count;
	uint32_t result_type;
};

struct prototype_type_constructor_declaration {
	int name_symbol_id;
	uint32_t owner_type;
	uint32_t constructor_index;
	/*
	 * The constructor telescope is the ContextDB path from parameter_context
	 * to field_context. result_classifier is meaningful in field_context.
	 */
	uint32_t parameter_context;
	uint32_t field_context;
	uint32_t result_classifier;
	uint32_t schema_revision;
};

struct prototype_constructor_classifier_cache_entry {
	uint32_t classifier;
	uint32_t schema_revision;
};

enum prototype_type_declaration_origin_kind {
	PROTOTYPE_TYPE_DECLARATION_ORIGIN_SOURCE = 0,
	PROTOTYPE_TYPE_DECLARATION_ORIGIN_GENERATED_IDENTITY = 1
};

struct prototype_type_declaration {
	int name_symbol_id;
	/* Stable TypeView identity.  The local type_index remains an arena handle;
	 * separately compiled copies of one imported declaration retain this
	 * qualified identity and become the same view after linking. */
	int namespace_symbol_id;
	uint32_t type_index;
	uint32_t representation_id;
	/* Classifier of the source type former itself. For example,
	 * List : Pi(Universe(u), \A => Universe(v)). */
	uint32_t formation_classifier;
	/* Full telescope of type-former parameters. */
	uint32_t parameter_context;
	uint32_t first_parameter;
	/* Readback/index cache; validation requires this to equal the depth of
	 * parameter_context. Semantic parameter classifiers live in ContextDB. */
	uint32_t parameter_count;
	/* The index telescope extends parameter_context. Constructors quantify
	 * only over uniform parameters and specialize indices in their result. */
	uint32_t index_context;
	uint32_t index_count;
	uint32_t first_constructor;
	uint32_t constructor_count;
	/* Generated semantic declarations have no surface name. Their object
	 * identity is selected by the exact source carrier Term, never by an
	 * invented Symbol ID or allocation-order-sensitive display name. */
	int origin_kind;
	uint32_t origin_source_carrier_term_id;
};

/*
 * An interned erased representation. This is not source type identity. The
 * source declaration remains identified by prototype_type_declaration::type_index.
 */
struct prototype_type_representation {
	uint32_t representative_type_id;
	struct prototype_type_representation_fingerprint fingerprint;
};

/* Source reconstruction and diagnostics only. None of these records may be
 * consulted to validate constructor fields or result classifiers. */
struct prototype_type_readback_db {
	struct prototype_type_parameter_declaration* parameter_declarations;
	size_t parameter_count;
	size_t parameter_capacity;
	struct prototype_type_constructor_readback* constructor_readbacks;
	size_t constructor_readback_capacity;
	uint32_t* field_types;
	size_t field_type_count;
	size_t field_type_capacity;
	struct prototype_type_expr* exprs;
	size_t expr_count;
	size_t expr_capacity;
	uint32_t next_level_var;
};

/* Persistent representation identity plus its rebuildable lookup cache. The
 * declaration schema refers to representation IDs, never fingerprints. */
struct prototype_type_representation_db {
	struct prototype_type_representation* representations;
	size_t representation_count;
	size_t representation_capacity;
	int cache_dirty;
};

/* Rebuildable constructor materialization. The cache is indexed by the
 * semantic constructor ID, but is not part of ConstructorSchema. */
struct prototype_constructor_classifier_cache {
	struct prototype_constructor_classifier_cache_entry* entries;
	size_t capacity;
};

/* All storage referenced by this composition view is borrowed from its owner.
 * Its nested stores have separate authority even though one session keeps
 * them physically adjacent. Semantic consumers use schema queries and cannot
 * derive acceptance from readback or cache presence. */
struct prototype_type_declaration_db {
	struct prototype_type_declaration* type_declarations;
	size_t type_count;
	size_t type_capacity;

	struct prototype_type_constructor_declaration* constructor_declarations;
	size_t constructor_count;
	size_t constructor_capacity;

	struct prototype_type_readback_db readback;
	struct prototype_type_representation_db representation_db;
	struct prototype_constructor_classifier_cache constructor_classifier_cache;
};

void prototype_type_declaration_db_init(
	struct prototype_type_declaration_db* db,
	struct prototype_type_declaration* type_declarations,
	size_t type_capacity,
	struct prototype_type_constructor_declaration* constructor_declarations,
	size_t constructor_capacity,
	struct prototype_type_parameter_declaration* parameter_declarations,
	size_t parameter_capacity,
	struct prototype_type_constructor_readback* constructor_readbacks,
	size_t constructor_readback_capacity,
	uint32_t* readback_field_types,
	size_t readback_field_type_capacity,
	struct prototype_type_expr* exprs,
	size_t expr_capacity,
	struct prototype_type_representation* representations,
	size_t representation_capacity,
	struct prototype_constructor_classifier_cache_entry* constructor_classifier_cache_entries,
	size_t constructor_classifier_cache_capacity
);
int prototype_type_declaration_project_reduction_environment(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct symbol_table* symbols,
	struct prototype_term_reduction_environment* p_environment
);

int prototype_type_expr_universe(struct prototype_type_declaration_db* db, uint32_t level, uint32_t* p_ret);
int prototype_type_expr_fresh_universe(struct prototype_type_declaration_db* db, uint32_t* p_ret);
int prototype_type_expr_self(struct prototype_type_declaration_db* db, uint32_t* p_ret);
int prototype_type_expr_var(struct prototype_type_declaration_db* db, uint32_t binding_id, int symbol_id, uint32_t* p_ret);
int prototype_type_expr_name(struct prototype_type_declaration_db* db, int symbol_id, uint32_t* p_ret);
int prototype_type_expr_primitive(struct prototype_type_declaration_db* db, int tag, uint32_t* p_ret);
int prototype_type_expr_app(struct prototype_type_declaration_db* db, uint32_t function, uint32_t argument, uint32_t* p_ret);
int prototype_type_expr_arrow(struct prototype_type_declaration_db* db, uint32_t domain, uint32_t codomain, uint32_t* p_ret);
int prototype_type_expr_pi(
	struct prototype_type_declaration_db* db,
	uint32_t binding_id,
	int symbol_id,
	uint32_t domain,
	uint32_t codomain,
	uint32_t* p_ret
);
int prototype_type_expr_imported_type(
	struct prototype_type_declaration_db* db,
	struct prototype_qualified_name name,
	const struct prototype_type_representation_fingerprint* key,
	uint32_t* p_ret
);
int prototype_type_expr_external_term(
	struct prototype_type_declaration_db* db,
	struct prototype_qualified_name name,
	uint32_t* p_ret
);
int prototype_type_expr_local_type_member(
	struct prototype_type_declaration_db* db,
	int owner_symbol_id,
	int member_symbol_id,
	uint32_t* p_ret
);

int prototype_type_declaration_add(
	struct prototype_type_declaration_db* db,
	int name_symbol_id,
	uint32_t* p_type_id
);

int prototype_type_declaration_add_generated_identity(
	struct prototype_type_declaration_db* db,
	uint32_t source_carrier_term_id,
	uint32_t parameter_context_id,
	uint32_t* p_type_id
);

int prototype_type_declaration_origins_validate(
	const struct prototype_type_declaration_db* db,
	const struct prototype_term_db* terms
);
int prototype_type_declaration_find_generated_identity(
	const struct prototype_type_declaration_db* db,
	uint32_t source_carrier_term_id,
	uint32_t parameter_context_id,
	uint32_t* p_type_id
);
int prototype_type_declaration_generated_identity_rule_for_source(
	const struct prototype_term_db* terms,
	uint32_t source_carrier_term_id
);
int prototype_type_declaration_validate_generated_identity(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_carrier_term_id,
	uint32_t generated_type_id,
	int computation_rule
);

int prototype_type_declaration_add_parameter(
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t binding_id,
	int name_symbol_id,
	uint32_t type_expr
);

int prototype_type_declaration_add_constructor_schema(
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	int name_symbol_id,
	uint32_t parameter_context,
	uint32_t field_context,
	uint32_t result_classifier,
	uint32_t* p_constructor_id
);

int prototype_type_readback_attach_constructor(
	struct prototype_type_declaration_db* db,
	uint32_t constructor_id,
	const uint32_t* field_type_exprs,
	uint32_t field_count,
	uint32_t result_type_expr
);

int prototype_type_constructor_classifier_cache_set(
	struct prototype_type_declaration_db* db,
	uint32_t constructor_id,
	uint32_t classifier
);

int prototype_type_constructor_classifier(
	struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	uint32_t constructor_id,
	uint32_t* p_classifier
);

const struct prototype_type_constructor_readback* prototype_type_constructor_readback_get(
	const struct prototype_type_declaration_db* db,
	uint32_t constructor_id
);

const struct prototype_constructor_classifier_cache_entry*
prototype_type_constructor_classifier_cache_get(
	const struct prototype_type_declaration_db* db,
	uint32_t constructor_id
);

int prototype_type_constructor_derive_curried_classifier(
	struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t parameter_context,
	uint32_t field_context,
	uint32_t result_classifier,
	uint32_t* p_classifier
);

int prototype_constructor_telescopes_validate(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms
);

int prototype_constructor_curried_caches_validate(
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms
);

int prototype_constructor_curried_caches_rebuild(
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms
);

const struct prototype_type_declaration* prototype_type_declaration_lookup(
	const struct prototype_type_declaration_db* db,
	int name_symbol_id
);

const struct prototype_type_constructor_declaration* prototype_type_declaration_lookup_constructor(
	const struct prototype_type_declaration_db* db,
	uint32_t type_id,
	int name_symbol_id
);

int prototype_type_declaration_representation_fingerprint(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	uint32_t type_id,
	struct prototype_type_representation_fingerprint* p_key
);

int prototype_type_representation_fingerprints_equal(
	const struct prototype_type_representation_fingerprint* left,
	const struct prototype_type_representation_fingerprint* right
);

int prototype_type_declaration_representation_anchor_type_id(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t* p_anchor_type_id
);

int prototype_type_declaration_intern_representation(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t* p_representation_id
);

int prototype_type_declaration_representation_type_id(
	const struct prototype_type_declaration_db* db,
	uint32_t representation_id,
	uint32_t* p_type_id
);

/* Resolve either a named TYPE_VIEW/TYPE_DECLARATION instance or its erased
 * TYPE_FORMER representation spine to the authoritative declaration. */
int prototype_type_declaration_instance_info(
	const struct prototype_type_declaration_db* db,
	const struct prototype_term_db* terms,
	uint32_t instance,
	uint32_t* p_type_id,
	uint32_t* arguments,
	uint32_t argument_capacity,
	uint32_t* p_argument_count
);

/* Resolve a nominal TYPE_VIEW to its validated declaration. This query is the
 * semantic ADT boundary: it never falls back to representation shape or
 * readback metadata. */
int prototype_type_view_declaration_query(
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	uint32_t type_view,
	uint32_t* p_type_id,
	const struct prototype_type_declaration** p_declaration
);

int prototype_type_view_constructor_telescope_query(
	const struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms,
	uint32_t type_view,
	uint32_t constructor_ordinal,
	const struct prototype_type_constructor_declaration** p_constructor
);

int prototype_type_declaration_rebuild_representations(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* db,
	const struct prototype_context_db* contexts
);
int prototype_type_declaration_representations_equal(
	const struct prototype_term_db* left_terms,
	const struct prototype_type_declaration_db* left_db,
	const struct prototype_context_db* left_contexts,
	uint32_t left_type_id,
	const struct prototype_term_db* right_terms,
	const struct prototype_type_declaration_db* right_db,
	const struct prototype_context_db* right_contexts,
	uint32_t right_type_id,
	int* p_equal
);

#endif
