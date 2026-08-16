#include "a_program/kernel/universe.h"
#include "a_program/support/storage.h"

#include <string.h>

#define reserve_slot prototype_storage_reserve_slot

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
) {
	memset(db, 0, sizeof(*db));
	db->nodes = nodes;
	db->node_capacity = node_capacity;
	db->edges = edges;
	db->edge_capacity = edge_capacity;
	db->levels = levels;
	db->level_capacity = level_capacity;
	db->constraints = constraints;
	db->constraint_capacity = constraint_capacity;
}

void prototype_universe_db_clear(struct prototype_universe_db* db) {
	if (!db) {
		return;
	}
	db->node_count = 0;
	db->edge_count = 0;
	db->level_count = 0;
	db->constraint_count = 0;
	db->solved = 0;
}

static int add_node(
	struct prototype_universe_db* db,
	struct prototype_universe_node node,
	uint32_t* p_node_id
) {
	if (!db || !p_node_id || reserve_slot(db->node_count, db->node_capacity) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)db->node_count;
	db->nodes[id] = node;
	db->node_count++;
	*p_node_id = id;
	return 0;
}

int prototype_universe_add_type_node(
	struct prototype_universe_db* db,
	uint32_t type_id,
	int symbol_id,
	uint32_t* p_node_id
) {
	struct prototype_universe_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_UNIVERSE_NODE_TYPE;
	node.type_id = type_id;
	node.parameter_id = PROTOTYPE_INVALID_ID;
	node.symbol_id = symbol_id;
	node.type_expr = PROTOTYPE_INVALID_ID;
	return add_node(db, node, p_node_id);
}

int prototype_universe_add_parameter_node(
	struct prototype_universe_db* db,
	uint32_t type_id,
	uint32_t parameter_id,
	int symbol_id,
	uint32_t type_expr,
	uint32_t* p_node_id
) {
	struct prototype_universe_node node;
	memset(&node, 0, sizeof(node));
	node.tag = PROTOTYPE_UNIVERSE_NODE_PARAMETER;
	node.type_id = type_id;
	node.parameter_id = parameter_id;
	node.symbol_id = symbol_id;
	node.type_expr = type_expr;
	return add_node(db, node, p_node_id);
}

int prototype_universe_add_edge(
	struct prototype_universe_db* db,
	int tag,
	uint32_t from_node,
	uint32_t to_node
) {
	if (!db || reserve_slot(db->edge_count, db->edge_capacity) != 0 ||
		from_node >= db->node_count || to_node >= db->node_count) {
		return -1;
	}
	uint32_t id = (uint32_t)db->edge_count;
	db->edges[id].tag = tag;
	db->edges[id].from_node = from_node;
	db->edges[id].to_node = to_node;
	db->edge_count++;
	return 0;
}

uint32_t prototype_universe_find_type_node(
	const struct prototype_universe_db* db,
	uint32_t type_id
) {
	if (!db) {
		return PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < db->node_count; ++i) {
		if (db->nodes[i].tag == PROTOTYPE_UNIVERSE_NODE_TYPE &&
			db->nodes[i].type_id == type_id) {
			return (uint32_t)i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static uint32_t find_level(
	const struct prototype_universe_db* db,
	uint32_t level_var
) {
	if (!db) {
		return PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < db->level_count; ++i) {
		if (db->levels[i].level_var == level_var) {
			return (uint32_t)i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

int prototype_universe_ensure_level(
	struct prototype_universe_db* db,
	uint32_t level_var,
	uint32_t* p_index
) {
	if (!db || !p_index) {
		return -1;
	}
	uint32_t existing = find_level(db, level_var);
	if (existing != PROTOTYPE_INVALID_ID) {
		*p_index = existing;
		return 0;
	}
	if (reserve_slot(db->level_count, db->level_capacity) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)db->level_count;
	db->levels[id].level_var = level_var;
	db->levels[id].value = 0;
	db->level_count++;
	*p_index = id;
	return 0;
}

int prototype_universe_add_constraint(
	struct prototype_universe_db* db,
	uint32_t lower_level_var,
	uint32_t upper_level_var,
	int offset,
	uint32_t subject,
	uint32_t classifier,
	int reason,
	uint32_t source_claim_id,
	int source_authority_kind,
	uint32_t source_authority_id,
	uint32_t source_subject,
	uint32_t source_classifier
) {
	uint32_t lower_index;
	uint32_t upper_index;
	if (!db || prototype_universe_ensure_level(
			db, lower_level_var, &lower_index
		) != 0 || prototype_universe_ensure_level(
			db, upper_level_var, &upper_index
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < db->constraint_count; ++i) {
		const struct prototype_universe_constraint* constraint = &db->constraints[i];
		if (constraint->lower_level_var == lower_level_var &&
			constraint->upper_level_var == upper_level_var &&
			constraint->offset == offset && constraint->subject == subject &&
			constraint->classifier == classifier && constraint->reason == reason &&
			constraint->source_claim_id == source_claim_id &&
			constraint->source_authority_kind == source_authority_kind &&
			constraint->source_authority_id == source_authority_id &&
			constraint->source_subject == source_subject &&
			constraint->source_classifier == source_classifier) {
			return 0;
		}
	}
	if (reserve_slot(db->constraint_count, db->constraint_capacity) != 0) {
		return -1;
	}
	uint32_t id = (uint32_t)db->constraint_count;
	db->constraints[id] = (struct prototype_universe_constraint){
		.lower_level_var = lower_level_var,
		.upper_level_var = upper_level_var,
		.offset = offset,
		.subject = subject,
		.classifier = classifier,
		.reason = reason,
		.source_claim_id = source_claim_id,
		.source_authority_kind = source_authority_kind,
		.source_authority_id = source_authority_id,
		.source_subject = source_subject,
		.source_classifier = source_classifier
	};
	db->constraint_count++;
	(void)lower_index;
	(void)upper_index;
	return 0;
}

static int constraint_is_first_numerical_edge(
	const struct prototype_universe_db* db,
	size_t constraint_index
) {
	const struct prototype_universe_constraint* current =
		&db->constraints[constraint_index];
	for (size_t i = 0; i < constraint_index; ++i) {
		const struct prototype_universe_constraint* prior = &db->constraints[i];
		if (prior->lower_level_var == current->lower_level_var &&
			prior->upper_level_var == current->upper_level_var &&
			prior->offset == current->offset) {
			return 0;
		}
	}
	return 1;
}

int prototype_universe_solve(struct prototype_universe_db* db) {
	if (!db) {
		return -1;
	}
	for (size_t i = 0; i < db->level_count; ++i) {
		db->levels[i].value = 0;
	}
	for (size_t pass = 0; pass <= db->level_count; ++pass) {
		int changed = 0;
		for (size_t i = 0; i < db->constraint_count; ++i) {
			if (!constraint_is_first_numerical_edge(db, i)) {
				continue;
			}
			const struct prototype_universe_constraint* constraint = &db->constraints[i];
			uint32_t lower_index = find_level(db, constraint->lower_level_var);
			uint32_t upper_index = find_level(db, constraint->upper_level_var);
			if (lower_index == PROTOTYPE_INVALID_ID || upper_index == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			int required = db->levels[lower_index].value + constraint->offset;
			if (db->levels[upper_index].value < required) {
				if (pass == db->level_count) {
					return -1;
				}
				db->levels[upper_index].value = required;
				changed = 1;
			}
		}
		if (!changed) {
			db->solved = 1;
			return 0;
		}
	}
	return -1;
}
