#ifndef A_PROGRAM_PROTOTYPE_FRONTEND_SOURCE_LOWERING_PLAN_H
#define A_PROGRAM_PROTOTYPE_FRONTEND_SOURCE_LOWERING_PLAN_H

#include <stddef.h>
#include <stdint.h>

#include "a_program/frontend/ast.h"
#include "a_program/frontend/source_schedule.h"
#include "a_program/frontend/source_topology_schema.h"

/*
 * This is source topology, not a semantic graph. No record may contain a Core
 * or Layer T identity. C0 and T0 consume the same sealed topology in separate
 * passes and store their results in separate owners.
 */
struct prototype_source_lowering_slot {
	int kind;
	uint32_t source_id;
	int source_tag;
	/* Only SELF type-expression slots carry this source-topology owner. It is
	 * not a TypeDeclarationId and conveys no typing judgement. */
	uint32_t owner_type_def_id;
	struct prototype_source_span span;
	uint32_t first_edge;
	uint32_t edge_count;
	uint32_t first_binder;
	uint32_t binder_count;
};

struct prototype_source_lowering_edge {
	uint32_t owner_slot;
	uint32_t child_slot;
	uint32_t ordinal;
	int role;
	/* Lexical binder visibility at the child. This is source topology only;
	 * it is not a Layer T Context or a substitution. */
	uint32_t scope_recipe;
};

struct prototype_source_lowering_binder {
	uint32_t owner_slot;
	/* Anonymous constructor fields use UINT32_MAX. Their source identity is
	 * the owner slot, binder role, and ordinal rather than a surface binder. */
	uint32_t source_binder_id;
	int symbol_id;
	uint32_t annotation_slot;
	uint32_t ordinal;
	int role;
};

/* Persistent prefix DAG over source binder-result IDs. UINT32_MAX denotes the
 * empty recipe. Each binder is introduced at most once in a recipe node. */
struct prototype_source_lowering_scope_recipe {
	uint32_t parent_recipe;
	uint32_t binder_id;
};

struct prototype_source_lowering_root {
	uint32_t source_entry_id;
	uint32_t table_id;
	uint32_t slot_id;
	int source_item_kind;
};

struct prototype_source_lowering_plan_capacity {
	size_t slot_count;
	size_t edge_count;
	size_t binder_count;
	size_t scope_recipe_count;
	size_t root_count;
};

struct prototype_source_lowering_plan {
	const struct prototype_ast_db* source;
	const struct prototype_source_lowering_slot* slots;
	size_t slot_count;
	const struct prototype_source_lowering_edge* edges;
	size_t edge_count;
	const struct prototype_source_lowering_binder* binders;
	size_t binder_count;
	const struct prototype_source_lowering_scope_recipe* scope_recipes;
	size_t scope_recipe_count;
	const struct prototype_source_lowering_root* roots;
	size_t root_count;
	/* Child-before-owner order over every lowering slot. */
	const uint32_t* order;
	size_t order_count;
	uint32_t first_type_expr_slot;
	uint64_t source_fingerprint;
	uint64_t topology_digest;
	int sealed;
};

int prototype_source_lowering_plan_measure(
	const struct prototype_ast_db* asts,
	const struct prototype_source_schedule* schedule,
	struct prototype_source_lowering_plan_capacity* p_capacity
);

int prototype_source_lowering_plan_build(
	const struct prototype_ast_db* asts,
	const struct prototype_source_schedule* schedule,
	struct prototype_source_lowering_slot* slots,
	size_t slot_capacity,
	struct prototype_source_lowering_edge* edges,
	size_t edge_capacity,
	struct prototype_source_lowering_binder* binders,
	size_t binder_capacity,
	struct prototype_source_lowering_scope_recipe* scope_recipes,
	size_t scope_recipe_capacity,
	struct prototype_source_lowering_root* roots,
	size_t root_capacity,
	uint32_t* order,
	size_t order_capacity,
	struct prototype_source_lowering_plan* p_plan
);

int prototype_source_lowering_plan_validate(
	const struct prototype_source_lowering_plan* plan
);

int prototype_source_epoch_fingerprint(
	const struct prototype_ast_db* source,
	uint64_t* p_fingerprint
);

uint32_t prototype_source_lowering_plan_term_slot(
	const struct prototype_source_lowering_plan* plan,
	uint32_t ast_node_id
);

uint32_t prototype_source_lowering_plan_type_expr_slot(
	const struct prototype_source_lowering_plan* plan,
	uint32_t ast_type_expr_id
);

#endif
