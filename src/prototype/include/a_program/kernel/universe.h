#ifndef __PROTOTYPE_UNIVERSE_H__
#define __PROTOTYPE_UNIVERSE_H__

#include <stddef.h>
#include <stdint.h>

#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"

/* These are compiler acceptance limits, not counts from a particular graph.
 * Re-publication may derive Universe data that was absent from the source
 * cache, so every collector must use the same stable capacity boundary. */
#define PROTOTYPE_UNIVERSE_NODE_CAPACITY 256
#define PROTOTYPE_UNIVERSE_EDGE_CAPACITY 512
#define PROTOTYPE_UNIVERSE_LEVEL_CAPACITY 1024
#define PROTOTYPE_UNIVERSE_CONSTRAINT_CAPACITY 4096

struct prototype_operation_graph;
struct prototype_judgement_db;

enum prototype_universe_node_tag {
	PROTOTYPE_UNIVERSE_NODE_TYPE = 1,
	PROTOTYPE_UNIVERSE_NODE_PARAMETER = 2
};

enum prototype_universe_edge_tag {
	PROTOTYPE_UNIVERSE_EDGE_PARAMETER_TO_TYPE = 1
};

enum prototype_universe_constraint_reason {
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_INVALID = 0,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_TERM_LEVEL_SUCCESSOR = 1,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_PI_DOMAIN = 2,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_PI_CODOMAIN = 3,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_MATCH_BRANCH = 4,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_APP_CUMULATIVITY = 5,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_EXPECTED_TYPE_CUMULATIVITY = 6,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_UNIVERSE_LEVEL = 7,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_PI_DOMAIN = 8,
	PROTOTYPE_UNIVERSE_CONSTRAINT_REASON_DERIVED_PI_CODOMAIN = 9
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
	int reason;
	/* Accepted evidence provenance. A derived helper inequality has no Claim,
	 * but still carries explicit authority instead of an untyped reason int. */
	uint32_t source_claim_id;
	int source_authority_kind;
	uint32_t source_authority_id;
	uint32_t source_subject;
	uint32_t source_classifier;
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
	const struct prototype_operation_graph* operations,
	const struct prototype_judgement_db* judgement
);

int prototype_universe_validate_provenance(
	const struct prototype_universe_db* db,
	const struct prototype_judgement_db* judgement
);

#endif
