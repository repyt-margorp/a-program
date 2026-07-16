#ifndef __PROTOTYPE_TYPE_DECLARATION_H__
#define __PROTOTYPE_TYPE_DECLARATION_H__

#include <stddef.h>
#include <stdint.h>

struct prototype_term_db;

/*
 * A qualified source-level address used only while a graph reference has not
 * been linked to a concrete node. It is not part of core computation identity.
 */
struct prototype_qualified_name {
	int namespace_symbol_id;
	int name_symbol_id;
};

/*
 * Type declarations are source-derived formation metadata used while lowering
 * AST into graph nodes. They provide the named type view: declaration identity,
 * constructor names, and constructor ordinals. The nameless computational side
 * lives in the term graph as TYPE_FORMER/APP/TYPE_VIEW core spines, and
 * classifier facts live in JudgementDB.
 */

#define PROTOTYPE_INVALID_ID UINT32_MAX

enum prototype_type_expr_tag {
	PROTOTYPE_TYPE_EXPR_UNIVERSE = 1,
	PROTOTYPE_TYPE_EXPR_UNIVERSE_VAR,
	PROTOTYPE_TYPE_EXPR_SELF,
	PROTOTYPE_TYPE_EXPR_VAR,
	PROTOTYPE_TYPE_EXPR_NAME,
	PROTOTYPE_TYPE_EXPR_APP,
	PROTOTYPE_TYPE_EXPR_ARROW,
	PROTOTYPE_TYPE_EXPR_IMPORTED_TYPE,
	PROTOTYPE_TYPE_EXPR_EXTERNAL_TERM,
	PROTOTYPE_TYPE_EXPR_PRIMITIVE_TEXT,
	PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT,
	PROTOTYPE_TYPE_EXPR_PRIMITIVE_INT64
};

struct prototype_type_code_shape_key {
	/*
	 * Structural fingerprint for the core shape layer. This is not declaration
	 * identity and must not be used as typed conversion evidence by itself.
	 */
	uint64_t hash;
	uint32_t node_count;
	uint32_t parameter_count;
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
		uint32_t binder_id;
		int symbol_id;
	} var;
	struct {
		int symbol_id;
	} name;
		struct {
			struct prototype_qualified_name name;
			struct prototype_type_code_shape_key code_shape_key;
		} imported_type;
		struct {
			struct prototype_qualified_name name;
		} external_term;
		struct {
			uint32_t function;
			uint32_t argument;
		} app;
		struct {
			uint32_t domain;
			uint32_t codomain;
		} arrow;
	} as;
};

struct prototype_type_parameter_declaration {
	uint32_t binder_id;
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
	 * Source/interface field and result expressions are readback metadata only.
	 * Constructor typing, shape keys, import reconstruction, and semantic owner
	 * checks must use classifier_family.
	 */
	struct prototype_type_constructor_readback readback;
	uint32_t classifier_family;
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
	uint32_t first_parameter;
	uint32_t parameter_count;
	uint32_t first_constructor;
	uint32_t constructor_count;
};

/*
 * An interned erased representation. This is not source type identity. The
 * source declaration remains identified by prototype_type_declaration::type_index.
 */
struct prototype_type_representation {
	uint32_t representative_type_id;
	struct prototype_type_code_shape_key fingerprint;
};

struct prototype_type_declaration_db {
	struct prototype_type_declaration* type_declarations;
	size_t type_count;
	size_t type_capacity;

	struct prototype_type_constructor_declaration* constructor_declarations;
	size_t constructor_count;
	size_t constructor_capacity;

	struct prototype_type_parameter_declaration* parameter_declarations;
	size_t parameter_count;
	size_t parameter_capacity;

	uint32_t* readback_field_types;
	size_t readback_field_type_count;
	size_t readback_field_type_capacity;

	struct prototype_type_expr* exprs;
	size_t expr_count;
	size_t expr_capacity;

	struct prototype_type_representation* representations;
	size_t representation_count;
	size_t representation_capacity;
	int representations_dirty;

	uint32_t next_level_var;
};

void prototype_type_declaration_db_init(
	struct prototype_type_declaration_db* db,
	struct prototype_type_declaration* type_declarations,
	size_t type_capacity,
	struct prototype_type_constructor_declaration* constructor_declarations,
	size_t constructor_capacity,
	struct prototype_type_parameter_declaration* parameter_declarations,
	size_t parameter_capacity,
	uint32_t* readback_field_types,
	size_t readback_field_type_capacity,
	struct prototype_type_expr* exprs,
	size_t expr_capacity
);

int prototype_type_expr_universe(struct prototype_type_declaration_db* db, uint32_t level, uint32_t* p_ret);
int prototype_type_expr_fresh_universe(struct prototype_type_declaration_db* db, uint32_t* p_ret);
int prototype_type_expr_self(struct prototype_type_declaration_db* db, uint32_t* p_ret);
int prototype_type_expr_var(struct prototype_type_declaration_db* db, uint32_t binder_id, int symbol_id, uint32_t* p_ret);
int prototype_type_expr_name(struct prototype_type_declaration_db* db, int symbol_id, uint32_t* p_ret);
int prototype_type_expr_primitive(struct prototype_type_declaration_db* db, int tag, uint32_t* p_ret);
int prototype_type_expr_app(struct prototype_type_declaration_db* db, uint32_t function, uint32_t argument, uint32_t* p_ret);
int prototype_type_expr_arrow(struct prototype_type_declaration_db* db, uint32_t domain, uint32_t codomain, uint32_t* p_ret);
int prototype_type_expr_imported_type(
	struct prototype_type_declaration_db* db,
	struct prototype_qualified_name name,
	const struct prototype_type_code_shape_key* key,
	uint32_t* p_ret
);
int prototype_type_expr_external_term(
	struct prototype_type_declaration_db* db,
	struct prototype_qualified_name name,
	uint32_t* p_ret
);

int prototype_type_declaration_add(
	struct prototype_type_declaration_db* db,
	int name_symbol_id,
	uint32_t* p_type_id
);

int prototype_type_declaration_add_parameter(
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	uint32_t binder_id,
	int name_symbol_id,
	uint32_t type_expr
);

int prototype_type_declaration_add_constructor(
	struct prototype_type_declaration_db* db,
	uint32_t type_id,
	int name_symbol_id,
	const uint32_t* readback_field_type_exprs,
	uint32_t readback_field_count,
	uint32_t readback_result_type_expr,
	uint32_t classifier_family,
	uint32_t* p_constructor_id
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

int prototype_type_declaration_code_shape_key(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* db,
	uint32_t type_id,
	struct prototype_type_code_shape_key* p_key
);

int prototype_type_code_shape_keys_equal(
	const struct prototype_type_code_shape_key* left,
	const struct prototype_type_code_shape_key* right
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

int prototype_type_declaration_rebuild_representations(
	const struct prototype_term_db* terms,
	struct prototype_type_declaration_db* db
);

#endif
