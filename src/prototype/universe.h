#ifndef __PROTOTYPE_UNIVERSE_H__
#define __PROTOTYPE_UNIVERSE_H__

#include <stddef.h>
#include <stdint.h>

#include "term.h"
#include "type_declaration.h"

enum prototype_universe_node_tag {
	PROTOTYPE_UNIVERSE_NODE_TYPE = 1,
	PROTOTYPE_UNIVERSE_NODE_PARAMETER
};

enum prototype_universe_edge_tag {
	PROTOTYPE_UNIVERSE_EDGE_PARAMETER_TO_TYPE = 1
};

enum prototype_universe_constraint_reason {
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_LEVEL = 1001
};

struct prototype_judgement_db;

struct prototype_universe_node {
	int tag;
	uint32_t type_id;
	uint32_t parameter_id;
	int symbol_id;
	uint32_t type_expr;
};

struct prototype_universe_edge {
	int tag;
	uint32_t from_node;
	uint32_t to_node;
};

struct prototype_universe_level {
	uint32_t level_var;
	int value;
};

struct prototype_universe_constraint {
	uint32_t lower_level_var;
	uint32_t upper_level_var;
	int offset;
	uint32_t subject;
	uint32_t classifier;
	int reason_kind;
};

struct prototype_universe_db {
	struct prototype_universe_node* nodes;
	size_t node_count;
	size_t node_capacity;

	struct prototype_universe_edge* edges;
	size_t edge_count;
	size_t edge_capacity;

	struct prototype_universe_level* levels;
	size_t level_count;
	size_t level_capacity;

	struct prototype_universe_constraint* constraints;
	size_t constraint_count;
	size_t constraint_capacity;

	int solved;
};

void prototype_universe_db_init(
	struct prototype_universe_db* db,
	struct prototype_universe_node* nodes,
	size_t node_capacity,
	struct prototype_universe_edge* edges,
	size_t edge_capacity,
	struct prototype_universe_level* levels,
	size_t level_capacity,
	struct prototype_universe_constraint* constraints,
	size_t constraint_capacity
);

void prototype_universe_db_clear(struct prototype_universe_db* db);

int prototype_universe_add_type_node(
	struct prototype_universe_db* db,
	uint32_t type_id,
	int symbol_id,
	uint32_t* p_node_id
);

int prototype_universe_add_parameter_node(
	struct prototype_universe_db* db,
	uint32_t type_id,
	uint32_t parameter_id,
	int symbol_id,
	uint32_t type_expr,
	uint32_t* p_node_id
);

int prototype_universe_add_edge(
	struct prototype_universe_db* db,
	int tag,
	uint32_t from_node,
	uint32_t to_node
);

uint32_t prototype_universe_find_type_node(
	const struct prototype_universe_db* db,
	uint32_t type_id
);

int prototype_universe_collect(
	struct prototype_universe_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_judgement_db* judgement
);

#endif
