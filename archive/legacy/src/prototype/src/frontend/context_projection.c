#include "a_program/frontend/context_projection.h"
#include "a_program/support/storage.h"

#include <limits.h>
#include <string.h>

static uint64_t context_projection_hash_u32(uint64_t hash, uint32_t value) {
	for (size_t i = 0; i < sizeof(value); ++i) {
		hash ^= (uint8_t)(value >> (i * CHAR_BIT));
		hash *= UINT64_C(1099511628211);
	}
	return hash;
}

static uint64_t context_action_hash(
	int kind,
	uint32_t target_candidate_context,
	uint32_t parent_action,
	uint32_t dependency_id
) {
	uint64_t hash = UINT64_C(14695981039346656037);
	hash = context_projection_hash_u32(hash, (uint32_t)kind);
	hash = context_projection_hash_u32(hash, target_candidate_context);
	hash = context_projection_hash_u32(hash, parent_action);
	return context_projection_hash_u32(hash, dependency_id);
}

static uint64_t context_projection_hash(
	uint32_t candidate_context,
	uint32_t action_expression
) {
	uint64_t hash = UINT64_C(14695981039346656037);
	hash = context_projection_hash_u32(hash, candidate_context);
	return context_projection_hash_u32(hash, action_expression);
}

static uint64_t typed_projection_hash(
	uint32_t source_typing_node,
	uint32_t context_projection
) {
	uint64_t hash = UINT64_C(14695981039346656037);
	hash = context_projection_hash_u32(hash, source_typing_node);
	return context_projection_hash_u32(hash, context_projection);
}

static uint64_t match_case_projection_hash(
	uint32_t source_case,
	uint32_t owner_typed_projection,
	uint32_t context_projection
) {
	uint64_t hash = UINT64_C(14695981039346656037);
	hash = context_projection_hash_u32(hash, source_case);
	hash = context_projection_hash_u32(hash, owner_typed_projection);
	return context_projection_hash_u32(hash, context_projection);
}

static uint64_t typed_projection_edge_hash(
	int kind,
	uint32_t parent_typed_projection,
	uint32_t source_relation
) {
	uint64_t hash = UINT64_C(14695981039346656037);
	hash = context_projection_hash_u32(hash, (uint32_t)kind);
	hash = context_projection_hash_u32(hash, parent_typed_projection);
	return context_projection_hash_u32(hash, source_relation);
}

void prototype_context_projection_db_init(
	struct prototype_context_projection_db* db,
	struct prototype_context_action_expression* actions,
	size_t action_capacity,
	struct prototype_context_projection* projections,
	struct prototype_context_projection_solution* projection_solutions,
	size_t projection_capacity,
	struct prototype_typed_projection* typed_projections,
	size_t typed_projection_capacity,
	uint32_t* typed_projection_typing_node_heads,
	size_t typed_projection_typing_node_capacity,
	uint32_t* typed_projection_context_heads,
	size_t typed_projection_context_capacity,
	struct prototype_typed_match_case_projection* match_case_projections,
	size_t match_case_projection_capacity,
	struct prototype_typed_projection_edge* typed_projection_edges,
	size_t typed_projection_edge_capacity,
	uint32_t* typed_projection_edge_parent_heads,
	size_t typed_projection_edge_parent_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->actions = actions;
	db->action_capacity = actions ? action_capacity : 0;
	db->projections = projections;
	db->projection_solutions = projection_solutions;
	db->projection_capacity = projections && projection_solutions ?
		projection_capacity : 0;
	db->typed_projections = typed_projections;
	db->typed_projection_capacity = typed_projections ?
		typed_projection_capacity : 0;
	db->typed_projection_typing_node_heads = typed_projection_typing_node_heads;
	db->typed_projection_typing_node_capacity =
		typed_projection_typing_node_heads ?
			typed_projection_typing_node_capacity : 0;
	db->typed_projection_context_heads = typed_projection_context_heads;
	db->typed_projection_context_capacity = typed_projection_context_heads ?
		typed_projection_context_capacity : 0;
	db->match_case_projections = match_case_projections;
	db->match_case_projection_capacity = match_case_projections ?
		match_case_projection_capacity : 0;
	db->typed_projection_edges = typed_projection_edges;
	db->typed_projection_edge_capacity = typed_projection_edges ?
		typed_projection_edge_capacity : 0;
	db->typed_projection_edge_parent_heads = typed_projection_edge_parent_heads;
	db->typed_projection_edge_parent_capacity =
		typed_projection_edge_parent_heads ?
			typed_projection_edge_parent_capacity : 0;
	prototype_intern_index_clear(
		db->action_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->projection_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->typed_projection_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->match_case_projection_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->typed_projection_edge_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	if (db->typed_projection_typing_node_heads) {
		prototype_intern_index_clear(
			db->typed_projection_typing_node_heads,
			db->typed_projection_typing_node_capacity,
			PROTOTYPE_INVALID_ID
		);
	}
	if (db->typed_projection_context_heads) {
		prototype_intern_index_clear(
			db->typed_projection_context_heads,
			db->typed_projection_context_capacity,
			PROTOTYPE_INVALID_ID
		);
	}
	if (db->typed_projection_edge_parent_heads) {
		prototype_intern_index_clear(
			db->typed_projection_edge_parent_heads,
			db->typed_projection_edge_parent_capacity,
			PROTOTYPE_INVALID_ID
		);
	}
}

const struct prototype_context_action_expression*
prototype_context_action_expression_get(
	const struct prototype_context_projection_db* db,
	uint32_t action_id
) {
	return db && action_id < db->action_count ? &db->actions[action_id] : NULL;
}

static int context_action_key_valid(
	const struct prototype_context_projection_db* db,
	int kind,
	uint32_t target_candidate_context,
	uint32_t parent_action,
	uint32_t dependency_id
) {
	if (!db || target_candidate_context == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	switch (kind) {
	case PROTOTYPE_CONTEXT_ACTION_IDENTITY:
		return parent_action == PROTOTYPE_INVALID_ID &&
			dependency_id == PROTOTYPE_INVALID_ID;
	case PROTOTYPE_CONTEXT_ACTION_PROJECTION:
	case PROTOTYPE_CONTEXT_ACTION_RESTRICTION:
		return parent_action < db->action_count &&
			dependency_id == PROTOTYPE_INVALID_ID;
	case PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT:
		return parent_action < db->action_count &&
			dependency_id < db->match_case_projection_count;
	default:
		return 0;
	}
}

static int context_action_intern(
	struct prototype_context_projection_db* db,
	int kind,
	uint32_t target_candidate_context,
	uint32_t parent_action,
	uint32_t dependency_id,
	uint32_t* p_action_id
) {
	if (!db || !p_action_id || !context_action_key_valid(
			db, kind, target_candidate_context, parent_action, dependency_id
		)) {
		return -1;
	}
	db->action_intern_requests++;
	uint64_t hash = context_action_hash(
		kind, target_candidate_context, parent_action, dependency_id
	);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	for (uint32_t id = db->action_index_heads[bucket];
		id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->action_count) {
			return -1;
		}
		const struct prototype_context_action_expression* action = &db->actions[id];
		if (action->key_hash == hash && action->kind == kind &&
			action->target_candidate_context == target_candidate_context &&
			action->parent_action == parent_action &&
			action->dependency_id == dependency_id) {
			db->action_intern_hits++;
			*p_action_id = id;
			return 0;
		}
		id = action->hash_next;
	}
	if (prototype_storage_reserve_slot(
			db->action_count, db->action_capacity
		) != 0 || db->action_count > UINT32_MAX) {
		return -1;
	}
	uint32_t id = (uint32_t)db->action_count++;
	db->actions[id] = (struct prototype_context_action_expression) {
		.kind = kind,
		.target_candidate_context = target_candidate_context,
		.parent_action = parent_action,
		.dependency_id = dependency_id,
		.key_hash = hash,
		.hash_next = db->action_index_heads[bucket]
	};
	db->action_index_heads[bucket] = id;
	*p_action_id = id;
	return 0;
}

int prototype_context_action_identity(
	struct prototype_context_projection_db* db,
	uint32_t candidate_context,
	uint32_t* p_action_id
) {
	return context_action_intern(
		db,
		PROTOTYPE_CONTEXT_ACTION_IDENTITY,
		candidate_context,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		p_action_id
	);
}

int prototype_context_action_projection(
	struct prototype_context_projection_db* db,
	uint32_t parent_action,
	uint32_t extension_candidate_context,
	uint32_t* p_action_id
) {
	return context_action_intern(
		db,
		PROTOTYPE_CONTEXT_ACTION_PROJECTION,
		extension_candidate_context,
		parent_action,
		PROTOTYPE_INVALID_ID,
		p_action_id
	);
}

int prototype_context_action_restriction(
	struct prototype_context_projection_db* db,
	uint32_t parent_action,
	uint32_t ancestor_candidate_context,
	uint32_t* p_action_id
) {
	return context_action_intern(
		db,
		PROTOTYPE_CONTEXT_ACTION_RESTRICTION,
		ancestor_candidate_context,
		parent_action,
		PROTOTYPE_INVALID_ID,
		p_action_id
	);
}

int prototype_context_action_branch_refinement(
	struct prototype_context_projection_db* db,
	uint32_t parent_action,
	uint32_t branch_candidate_context,
	uint32_t match_case_projection_id,
	uint32_t* p_action_id
) {
	return context_action_intern(
		db,
		PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT,
		branch_candidate_context,
		parent_action,
		match_case_projection_id,
		p_action_id
	);
}

const struct prototype_context_projection* prototype_context_projection_get(
	const struct prototype_context_projection_db* db,
	uint32_t projection_id
) {
	return db && projection_id < db->projection_count ?
		&db->projections[projection_id] : NULL;
}

const struct prototype_context_projection_solution*
prototype_context_projection_solution_get(
	const struct prototype_context_projection_db* db,
	uint32_t projection_id
) {
	return db && projection_id < db->projection_count ?
		&db->projection_solutions[projection_id] : NULL;
}

int prototype_context_projection_find(
	const struct prototype_context_projection_db* db,
	uint32_t candidate_context,
	uint32_t action_expression,
	uint32_t* p_projection_id
) {
	const struct prototype_context_action_expression* action =
		prototype_context_action_expression_get(db, action_expression);
	if (!db || !p_projection_id || candidate_context == PROTOTYPE_INVALID_ID ||
		!action || action->target_candidate_context != candidate_context) {
		return -1;
	}
	uint64_t hash = context_projection_hash(candidate_context, action_expression);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	for (uint32_t id = db->projection_index_heads[bucket];
		id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->projection_count) {
			return -1;
		}
		const struct prototype_context_projection* projection =
			&db->projections[id];
		if (projection->key_hash == hash &&
			projection->candidate_context == candidate_context &&
			projection->action_expression == action_expression) {
			*p_projection_id = id;
			return 0;
		}
		id = projection->hash_next;
	}
	return 1;
}

int prototype_context_projection_intern(
	struct prototype_context_projection_db* db,
	uint32_t candidate_context,
	uint32_t action_expression,
	uint32_t* p_projection_id
) {
	const struct prototype_context_action_expression* action =
		prototype_context_action_expression_get(db, action_expression);
	if (!db || !p_projection_id || candidate_context == PROTOTYPE_INVALID_ID ||
		!action || action->target_candidate_context != candidate_context) {
		return -1;
	}
	db->projection_intern_requests++;
	int find_status = prototype_context_projection_find(
		db, candidate_context, action_expression, p_projection_id
	);
	if (find_status < 0) {
		return -1;
	}
	if (find_status == 0) {
		db->projection_intern_hits++;
		return 0;
	}
	uint64_t hash = context_projection_hash(candidate_context, action_expression);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	if (prototype_storage_reserve_slot(
			db->projection_count, db->projection_capacity
		) != 0 || db->projection_count > UINT32_MAX) {
		return -1;
	}
	uint32_t id = (uint32_t)db->projection_count++;
	db->projections[id] = (struct prototype_context_projection) {
		.candidate_context = candidate_context,
		.action_expression = action_expression,
		.key_hash = hash,
		.hash_next = db->projection_index_heads[bucket]
	};
	db->projection_solutions[id] =
		(struct prototype_context_projection_solution) {
			.state = PROTOTYPE_CONTEXT_PROJECTION_PENDING,
			.concrete_context = PROTOTYPE_INVALID_ID,
			.substitution = PROTOTYPE_INVALID_ID,
			.input_fingerprint = 0,
			.revision = 0
		};
	db->projection_index_heads[bucket] = id;
	*p_projection_id = id;
	return 0;
}

int prototype_context_projection_publish(
	struct prototype_context_projection_db* db,
	uint32_t projection_id,
	uint32_t concrete_context,
	uint32_t substitution,
	uint64_t input_fingerprint
) {
	if (!db || projection_id >= db->projection_count ||
		concrete_context == PROTOTYPE_INVALID_ID ||
		substitution == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_context_projection_solution* solution =
		&db->projection_solutions[projection_id];
	if (solution->state == PROTOTYPE_CONTEXT_PROJECTION_SOLVED &&
		solution->concrete_context == concrete_context &&
		solution->substitution == substitution &&
		solution->input_fingerprint == input_fingerprint) {
		return 0;
	}
	if (solution->state == PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
		return -1;
	}
	if (db->solution_revision == UINT64_MAX || solution->revision == UINT64_MAX) {
		return -1;
	}
	solution->state = PROTOTYPE_CONTEXT_PROJECTION_SOLVED;
	solution->concrete_context = concrete_context;
	solution->substitution = substitution;
	solution->input_fingerprint = input_fingerprint;
	++solution->revision;
	++db->solution_revision;
	return 0;
}

int prototype_context_projection_reject(
	struct prototype_context_projection_db* db,
	uint32_t projection_id
) {
	if (!db || projection_id >= db->projection_count) {
		return -1;
	}
	struct prototype_context_projection_solution* solution =
		&db->projection_solutions[projection_id];
	if (solution->state == PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
		return 0;
	}
	if (solution->state != PROTOTYPE_CONTEXT_PROJECTION_PENDING) {
		return -1;
	}
	if (db->solution_revision == UINT64_MAX || solution->revision == UINT64_MAX) {
		return -1;
	}
	solution->state = PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION;
	solution->input_fingerprint = 0;
	++solution->revision;
	++db->solution_revision;
	return 0;
}

const struct prototype_typed_projection* prototype_typed_projection_get(
	const struct prototype_context_projection_db* db,
	uint32_t typed_projection_id
) {
	return db && typed_projection_id < db->typed_projection_count ?
		&db->typed_projections[typed_projection_id] : NULL;
}

int prototype_typed_projection_intern(
	struct prototype_context_projection_db* db,
	uint32_t source_typing_node,
	uint32_t source_occurrence,
	uint32_t context_projection,
	uint32_t* p_typed_projection_id
) {
	if (!db || !p_typed_projection_id ||
		source_typing_node >= db->typed_projection_typing_node_capacity ||
		!db->typed_projection_typing_node_heads ||
		!db->typed_projection_context_heads ||
		context_projection >= db->typed_projection_context_capacity ||
		context_projection >= db->projection_count) {
		return -1;
	}
	db->typed_projection_intern_requests++;
	uint64_t hash = typed_projection_hash(source_typing_node, context_projection);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	for (uint32_t id = db->typed_projection_index_heads[bucket];
		id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->typed_projection_count) {
			return -1;
		}
		const struct prototype_typed_projection* projection =
			&db->typed_projections[id];
		if (projection->key_hash == hash &&
			projection->source_typing_node == source_typing_node &&
			projection->context_projection == context_projection) {
			if (projection->source_occurrence != source_occurrence) return -1;
			db->typed_projection_intern_hits++;
			*p_typed_projection_id = id;
			return 0;
		}
		id = projection->hash_next;
	}
	if (prototype_storage_reserve_slot(
			db->typed_projection_count, db->typed_projection_capacity
		) != 0 || db->typed_projection_count > UINT32_MAX) {
		return -1;
	}
	uint32_t id = (uint32_t)db->typed_projection_count++;
	db->typed_projections[id] = (struct prototype_typed_projection) {
		.source_typing_node = source_typing_node,
		.source_occurrence = source_occurrence,
		.context_projection = context_projection,
		.key_hash = hash,
		.hash_next = db->typed_projection_index_heads[bucket],
		.typing_node_next =
			db->typed_projection_typing_node_heads[source_typing_node],
		.context_projection_next =
			db->typed_projection_context_heads[context_projection],
		.topology_expanded = 0
	};
	db->typed_projection_index_heads[bucket] = id;
	db->typed_projection_typing_node_heads[source_typing_node] = id;
	db->typed_projection_context_heads[context_projection] = id;
	*p_typed_projection_id = id;
	return 0;
}

uint32_t prototype_typed_projection_first_for_typing_node(
	const struct prototype_context_projection_db* db,
	uint32_t source_typing_node
) {
	return db && db->typed_projection_typing_node_heads &&
		source_typing_node < db->typed_projection_typing_node_capacity ?
		db->typed_projection_typing_node_heads[source_typing_node] :
		PROTOTYPE_INVALID_ID;
}

uint32_t prototype_typed_projection_next_for_typing_node(
	const struct prototype_context_projection_db* db,
	uint32_t typed_projection_id
) {
	return db && typed_projection_id < db->typed_projection_count ?
		db->typed_projections[typed_projection_id].typing_node_next :
		PROTOTYPE_INVALID_ID;
}

uint32_t prototype_typed_projection_first_for_context_projection(
	const struct prototype_context_projection_db* db,
	uint32_t context_projection_id
) {
	return db && db->typed_projection_context_heads &&
		context_projection_id < db->typed_projection_context_capacity ?
		db->typed_projection_context_heads[context_projection_id] :
		PROTOTYPE_INVALID_ID;
}

uint32_t prototype_typed_projection_next_for_context_projection(
	const struct prototype_context_projection_db* db,
	uint32_t typed_projection_id
) {
	return db && typed_projection_id < db->typed_projection_count ?
		db->typed_projections[typed_projection_id].context_projection_next :
		PROTOTYPE_INVALID_ID;
}

const struct prototype_typed_match_case_projection*
prototype_typed_match_case_projection_get(
	const struct prototype_context_projection_db* db,
	uint32_t match_case_projection_id
) {
	return db && match_case_projection_id < db->match_case_projection_count ?
		&db->match_case_projections[match_case_projection_id] : NULL;
}

int prototype_typed_match_case_projection_intern(
	struct prototype_context_projection_db* db,
	uint32_t source_case,
	uint32_t owner_typed_projection,
	uint32_t context_projection,
	uint32_t* p_match_case_projection_id
) {
	if (!db || !p_match_case_projection_id ||
		source_case == PROTOTYPE_INVALID_ID ||
		owner_typed_projection >= db->typed_projection_count ||
		context_projection >= db->projection_count) {
		return -1;
	}
	db->match_case_projection_intern_requests++;
	uint64_t hash = match_case_projection_hash(
		source_case, owner_typed_projection, context_projection
	);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	for (uint32_t id = db->match_case_projection_index_heads[bucket];
		id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->match_case_projection_count) {
			return -1;
		}
		const struct prototype_typed_match_case_projection* projection =
			&db->match_case_projections[id];
		if (projection->key_hash == hash &&
			projection->source_case == source_case &&
			projection->owner_typed_projection == owner_typed_projection &&
			projection->context_projection == context_projection) {
			db->match_case_projection_intern_hits++;
			*p_match_case_projection_id = id;
			return 0;
		}
		id = projection->hash_next;
	}
	if (prototype_storage_reserve_slot(
			db->match_case_projection_count,
			db->match_case_projection_capacity
		) != 0 || db->match_case_projection_count > UINT32_MAX) {
		return -1;
	}
	uint32_t id = (uint32_t)db->match_case_projection_count++;
	db->match_case_projections[id] =
		(struct prototype_typed_match_case_projection) {
			.source_case = source_case,
			.owner_typed_projection = owner_typed_projection,
			.context_projection = context_projection,
			.key_hash = hash,
			.hash_next = db->match_case_projection_index_heads[bucket]
		};
	db->match_case_projection_index_heads[bucket] = id;
	*p_match_case_projection_id = id;
	return 0;
}

int prototype_typed_match_case_projection_find(
	const struct prototype_context_projection_db* db,
	uint32_t source_case,
	uint32_t owner_typed_projection,
	uint32_t* p_match_case_projection_id
) {
	if (!db || !p_match_case_projection_id ||
		owner_typed_projection >= db->typed_projection_count) {
		return -1;
	}
	uint32_t found = PROTOTYPE_INVALID_ID;
	for (uint32_t id = 0; id < db->match_case_projection_count; ++id) {
		const struct prototype_typed_match_case_projection* projection =
			&db->match_case_projections[id];
		if (projection->source_case != source_case ||
			projection->owner_typed_projection != owner_typed_projection) {
			continue;
		}
		if (found != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		found = id;
	}
	if (found == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	*p_match_case_projection_id = found;
	return 0;
}

const struct prototype_typed_projection_edge* prototype_typed_projection_edge_get(
	const struct prototype_context_projection_db* db,
	uint32_t edge_id
) {
	return db && edge_id < db->typed_projection_edge_count ?
		&db->typed_projection_edges[edge_id] : NULL;
}

uint32_t prototype_typed_projection_first_edge(
	const struct prototype_context_projection_db* db,
	uint32_t parent_typed_projection
) {
	return db && db->typed_projection_edge_parent_heads &&
		parent_typed_projection < db->typed_projection_edge_parent_capacity ?
		db->typed_projection_edge_parent_heads[parent_typed_projection] :
		PROTOTYPE_INVALID_ID;
}

uint32_t prototype_typed_projection_next_edge(
	const struct prototype_context_projection_db* db,
	uint32_t edge_id
) {
	return db && edge_id < db->typed_projection_edge_count ?
		db->typed_projection_edges[edge_id].parent_next : PROTOTYPE_INVALID_ID;
}

static int typed_projection_edge_kind_valid(int kind) {
	return kind == PROTOTYPE_TYPED_PROJECTION_EDGE_WRAPPED ||
		kind == PROTOTYPE_TYPED_PROJECTION_EDGE_CHILD ||
		kind == PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE ||
		kind == PROTOTYPE_TYPED_PROJECTION_EDGE_SEQUENCE_PRODUCER ||
		kind == PROTOTYPE_TYPED_PROJECTION_EDGE_INDUCTION_OWNER;
}

int prototype_typed_projection_edge_intern(
	struct prototype_context_projection_db* db,
	int kind,
	uint32_t parent_typed_projection,
	uint32_t source_relation,
	uint32_t child_typed_projection,
	uint32_t* p_edge_id
) {
	if (!db || !p_edge_id || !typed_projection_edge_kind_valid(kind) ||
		parent_typed_projection >= db->typed_projection_count ||
		parent_typed_projection >= db->typed_projection_edge_parent_capacity ||
		source_relation == PROTOTYPE_INVALID_ID ||
		child_typed_projection >= db->typed_projection_count) {
		return -1;
	}
	db->typed_projection_edge_intern_requests++;
	uint64_t hash = typed_projection_edge_hash(
		kind,
		parent_typed_projection,
		source_relation
	);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	for (uint32_t id = db->typed_projection_edge_index_heads[bucket];
		id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->typed_projection_edge_count) {
			return -1;
		}
		const struct prototype_typed_projection_edge* edge =
			&db->typed_projection_edges[id];
		if (edge->key_hash == hash && edge->kind == kind &&
			edge->parent_typed_projection == parent_typed_projection &&
			edge->source_relation == source_relation) {
			if (edge->child_typed_projection != child_typed_projection) {
				return -1;
			}
			db->typed_projection_edge_intern_hits++;
			*p_edge_id = id;
			return 0;
		}
		id = edge->hash_next;
	}
	if (prototype_storage_reserve_slot(
			db->typed_projection_edge_count,
			db->typed_projection_edge_capacity
		) != 0 || db->typed_projection_edge_count > UINT32_MAX) {
		return -1;
	}
	uint32_t id = (uint32_t)db->typed_projection_edge_count++;
	db->typed_projection_edges[id] = (struct prototype_typed_projection_edge) {
		.kind = kind,
		.parent_typed_projection = parent_typed_projection,
		.source_relation = source_relation,
		.child_typed_projection = child_typed_projection,
		.key_hash = hash,
		.hash_next = db->typed_projection_edge_index_heads[bucket],
		.parent_next = db->typed_projection_edge_parent_heads[
			parent_typed_projection
		]
	};
	db->typed_projection_edge_index_heads[bucket] = id;
	db->typed_projection_edge_parent_heads[parent_typed_projection] = id;
	*p_edge_id = id;
	return 0;
}

int prototype_typed_projection_edge_find(
	const struct prototype_context_projection_db* db,
	int kind,
	uint32_t parent_typed_projection,
	uint32_t source_relation,
	uint32_t* p_child_typed_projection
) {
	if (!db || !p_child_typed_projection ||
		!typed_projection_edge_kind_valid(kind) ||
		parent_typed_projection >= db->typed_projection_count ||
		source_relation == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint64_t hash = typed_projection_edge_hash(
		kind, parent_typed_projection, source_relation
	);
	size_t bucket;
	if (prototype_intern_index_bucket(
			hash, PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT, &bucket
		) != 0) {
		return -1;
	}
	for (uint32_t id = db->typed_projection_edge_index_heads[bucket];
		id != PROTOTYPE_INVALID_ID;) {
		if (id >= db->typed_projection_edge_count) {
			return -1;
		}
		const struct prototype_typed_projection_edge* edge =
			&db->typed_projection_edges[id];
		if (edge->key_hash == hash && edge->kind == kind &&
			edge->parent_typed_projection == parent_typed_projection &&
			edge->source_relation == source_relation) {
			*p_child_typed_projection = edge->child_typed_projection;
			return 0;
		}
		id = edge->hash_next;
	}
	return 1;
}

int prototype_context_projection_db_validate(
	const struct prototype_context_projection_db* db,
	size_t candidate_context_count,
	size_t typing_node_count,
	size_t occurrence_count,
	size_t occurrence_edge_count,
	size_t match_case_count,
	size_t concrete_context_count,
	size_t substitution_count
) {
	if (!db || (!db->actions && db->action_count != 0) ||
		(!db->projections && db->projection_count != 0) ||
		(!db->projection_solutions && db->projection_count != 0) ||
		(!db->typed_projections && db->typed_projection_count != 0) ||
		(!db->typed_projection_typing_node_heads &&
		 db->typed_projection_count != 0) ||
		(!db->typed_projection_context_heads &&
		 db->typed_projection_count != 0) ||
		(!db->match_case_projections && db->match_case_projection_count != 0) ||
		(!db->typed_projection_edges &&
		 db->typed_projection_edge_count != 0) ||
		(!db->typed_projection_edge_parent_heads &&
		 db->typed_projection_edge_count != 0) ||
		db->action_count > db->action_capacity ||
		db->projection_count > db->projection_capacity ||
		db->typed_projection_count > db->typed_projection_capacity ||
		db->match_case_projection_count > db->match_case_projection_capacity ||
		db->typed_projection_edge_count >
			db->typed_projection_edge_capacity ||
		db->typed_projection_count >
			db->typed_projection_edge_parent_capacity ||
		db->projection_count > db->typed_projection_context_capacity ||
		typing_node_count > db->typed_projection_typing_node_capacity) {
		return -1;
	}
	for (uint32_t id = 0; id < db->action_count; ++id) {
		const struct prototype_context_action_expression* action = &db->actions[id];
		if (action->target_candidate_context >= candidate_context_count ||
			!context_action_key_valid(
				db,
				action->kind,
				action->target_candidate_context,
				action->parent_action,
				action->dependency_id
			) || (action->kind == PROTOTYPE_CONTEXT_ACTION_BRANCH_REFINEMENT &&
			 action->dependency_id >= db->match_case_projection_count) ||
			action->key_hash != context_action_hash(
				action->kind,
				action->target_candidate_context,
				action->parent_action,
				action->dependency_id
			)) {
			return -1;
		}
	}
	for (uint32_t id = 0; id < db->projection_count; ++id) {
		const struct prototype_context_projection* projection =
			&db->projections[id];
		const struct prototype_context_projection_solution* solution =
			&db->projection_solutions[id];
		const struct prototype_context_action_expression* action =
			prototype_context_action_expression_get(
				db, projection->action_expression
			);
		if (projection->candidate_context >= candidate_context_count || !action ||
			action->target_candidate_context != projection->candidate_context ||
			projection->key_hash != context_projection_hash(
				projection->candidate_context, projection->action_expression
			)) {
			return -1;
		}
		if (solution->state == PROTOTYPE_CONTEXT_PROJECTION_PENDING) {
			if (solution->concrete_context != PROTOTYPE_INVALID_ID ||
				solution->substitution != PROTOTYPE_INVALID_ID ||
				solution->input_fingerprint != 0) {
				return -1;
			}
		} else if (solution->state ==
				PROTOTYPE_CONTEXT_PROJECTION_CONTRADICTION) {
			if (solution->concrete_context != PROTOTYPE_INVALID_ID ||
				solution->substitution != PROTOTYPE_INVALID_ID ||
				solution->input_fingerprint != 0) {
				return -1;
			}
		} else if (solution->state != PROTOTYPE_CONTEXT_PROJECTION_SOLVED ||
			solution->concrete_context >= concrete_context_count ||
			solution->substitution >= substitution_count) {
			return -1;
		}
	}
	for (uint32_t id = 0; id < db->typed_projection_count; ++id) {
		const struct prototype_typed_projection* projection =
			&db->typed_projections[id];
		if (projection->source_typing_node >= typing_node_count ||
			(projection->source_occurrence != PROTOTYPE_INVALID_ID &&
			 projection->source_occurrence >= occurrence_count) ||
			projection->context_projection >= db->projection_count ||
			(projection->typing_node_next != PROTOTYPE_INVALID_ID &&
			 projection->typing_node_next >= db->typed_projection_count) ||
			(projection->context_projection_next != PROTOTYPE_INVALID_ID &&
			 projection->context_projection_next >= db->typed_projection_count) ||
			(projection->topology_expanded != 0 &&
			 projection->topology_expanded != 1) ||
			projection->key_hash != typed_projection_hash(
				projection->source_typing_node, projection->context_projection
			)) {
			return -1;
		}
	}
	for (uint32_t context_projection = 0;
		context_projection < db->projection_count; ++context_projection) {
		uint32_t traversed = 0;
		for (uint32_t id =
				db->typed_projection_context_heads[context_projection];
			id != PROTOTYPE_INVALID_ID;) {
			if (id >= db->typed_projection_count ||
				db->typed_projections[id].context_projection != context_projection ||
				++traversed > db->typed_projection_count) {
				return -1;
			}
			id = db->typed_projections[id].context_projection_next;
		}
	}
	for (uint32_t source = 0; source < typing_node_count; ++source) {
		uint32_t traversed = 0;
		for (uint32_t id = db->typed_projection_typing_node_heads[source];
			id != PROTOTYPE_INVALID_ID;) {
			if (id >= db->typed_projection_count ||
				db->typed_projections[id].source_typing_node != source ||
				++traversed > db->typed_projection_count) {
				return -1;
			}
			id = db->typed_projections[id].typing_node_next;
		}
	}
	for (uint32_t id = 0; id < db->match_case_projection_count; ++id) {
		const struct prototype_typed_match_case_projection* projection =
			&db->match_case_projections[id];
		if (projection->source_case >= match_case_count ||
			projection->owner_typed_projection >= db->typed_projection_count ||
			projection->context_projection >= db->projection_count ||
			projection->key_hash != match_case_projection_hash(
				projection->source_case,
				projection->owner_typed_projection,
				projection->context_projection
			)) {
			return -1;
		}
	}
	for (uint32_t id = 0; id < db->typed_projection_edge_count; ++id) {
		const struct prototype_typed_projection_edge* edge =
			&db->typed_projection_edges[id];
		int relation_valid =
			edge->kind == PROTOTYPE_TYPED_PROJECTION_EDGE_WRAPPED ?
				edge->source_relation < occurrence_count :
			edge->kind == PROTOTYPE_TYPED_PROJECTION_EDGE_CHILD ?
				edge->source_relation < occurrence_edge_count :
			edge->kind == PROTOTYPE_TYPED_PROJECTION_EDGE_MATCH_CASE ?
				edge->source_relation < match_case_count :
				edge->kind == PROTOTYPE_TYPED_PROJECTION_EDGE_SEQUENCE_PRODUCER ?
					edge->source_relation < occurrence_count :
				edge->kind == PROTOTYPE_TYPED_PROJECTION_EDGE_INDUCTION_OWNER ?
					edge->source_relation < occurrence_count : 0;
		if (!relation_valid ||
			edge->parent_typed_projection >= db->typed_projection_count ||
			edge->child_typed_projection >= db->typed_projection_count ||
			edge->key_hash != typed_projection_edge_hash(
				edge->kind,
				edge->parent_typed_projection,
				edge->source_relation
			) || (edge->parent_next != PROTOTYPE_INVALID_ID &&
			 edge->parent_next >= db->typed_projection_edge_count)) {
			return -1;
		}
	}
	for (uint32_t parent = 0; parent < db->typed_projection_count; ++parent) {
		uint32_t traversed = 0;
		for (uint32_t id = db->typed_projection_edge_parent_heads[parent];
			id != PROTOTYPE_INVALID_ID;
			id = db->typed_projection_edges[id].parent_next) {
			if (id >= db->typed_projection_edge_count ||
				db->typed_projection_edges[id].parent_typed_projection != parent ||
				++traversed > db->typed_projection_edge_count) {
				return -1;
			}
		}
	}
	return 0;
}

int prototype_context_projection_db_rebuild_indexes(
	struct prototype_context_projection_db* db
) {
	if (!db || db->action_count > db->action_capacity ||
		db->projection_count > db->projection_capacity ||
		db->typed_projection_count > db->typed_projection_capacity ||
		db->match_case_projection_count > db->match_case_projection_capacity ||
		db->typed_projection_edge_count > db->typed_projection_edge_capacity ||
		db->projection_count > db->typed_projection_context_capacity ||
		db->typed_projection_count > db->typed_projection_edge_parent_capacity) {
		return -1;
	}
	prototype_intern_index_clear(
		db->action_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->projection_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->typed_projection_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->match_case_projection_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->typed_projection_edge_index_heads,
		PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->typed_projection_typing_node_heads,
		db->typed_projection_typing_node_capacity,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->typed_projection_context_heads,
		db->typed_projection_context_capacity,
		PROTOTYPE_INVALID_ID
	);
	prototype_intern_index_clear(
		db->typed_projection_edge_parent_heads,
		db->typed_projection_edge_parent_capacity,
		PROTOTYPE_INVALID_ID
	);

	for (uint32_t id = 0; id < db->action_count; ++id) {
		struct prototype_context_action_expression* action = &db->actions[id];
		if (!context_action_key_valid(
				db, action->kind, action->target_candidate_context,
				action->parent_action, action->dependency_id
			)) {
			return -1;
		}
		action->key_hash = context_action_hash(
			action->kind, action->target_candidate_context,
			action->parent_action, action->dependency_id
		);
		size_t bucket;
		if (prototype_intern_index_bucket(
				action->key_hash,
				PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
				&bucket
			) != 0) {
			return -1;
		}
		action->hash_next = db->action_index_heads[bucket];
		db->action_index_heads[bucket] = id;
	}
	for (uint32_t id = 0; id < db->projection_count; ++id) {
		struct prototype_context_projection* projection = &db->projections[id];
		if (projection->action_expression >= db->action_count) return -1;
		projection->key_hash = context_projection_hash(
			projection->candidate_context, projection->action_expression
		);
		size_t bucket;
		if (prototype_intern_index_bucket(
				projection->key_hash,
				PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
				&bucket
			) != 0) {
			return -1;
		}
		projection->hash_next = db->projection_index_heads[bucket];
		db->projection_index_heads[bucket] = id;
		db->projection_solutions[id] =
			(struct prototype_context_projection_solution) {
				.state = PROTOTYPE_CONTEXT_PROJECTION_PENDING,
				.concrete_context = PROTOTYPE_INVALID_ID,
				.substitution = PROTOTYPE_INVALID_ID
			};
	}
	for (uint32_t id = 0; id < db->typed_projection_count; ++id) {
		struct prototype_typed_projection* projection = &db->typed_projections[id];
		if (projection->source_typing_node >=
				db->typed_projection_typing_node_capacity ||
			projection->context_projection >= db->projection_count) {
			return -1;
		}
		projection->key_hash = typed_projection_hash(
			projection->source_typing_node, projection->context_projection
		);
		size_t bucket;
		if (prototype_intern_index_bucket(
				projection->key_hash,
				PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
				&bucket
			) != 0) {
			return -1;
		}
		projection->hash_next = db->typed_projection_index_heads[bucket];
		projection->typing_node_next =
			db->typed_projection_typing_node_heads[projection->source_typing_node];
		projection->context_projection_next =
			db->typed_projection_context_heads[projection->context_projection];
		db->typed_projection_index_heads[bucket] = id;
		db->typed_projection_typing_node_heads[projection->source_typing_node] = id;
		db->typed_projection_context_heads[projection->context_projection] = id;
	}
	for (uint32_t id = 0; id < db->match_case_projection_count; ++id) {
		struct prototype_typed_match_case_projection* projection =
			&db->match_case_projections[id];
		if (projection->owner_typed_projection >= db->typed_projection_count ||
			projection->context_projection >= db->projection_count) {
			return -1;
		}
		projection->key_hash = match_case_projection_hash(
			projection->source_case, projection->owner_typed_projection,
			projection->context_projection
		);
		size_t bucket;
		if (prototype_intern_index_bucket(
				projection->key_hash,
				PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
				&bucket
			) != 0) {
			return -1;
		}
		projection->hash_next = db->match_case_projection_index_heads[bucket];
		db->match_case_projection_index_heads[bucket] = id;
	}
	for (uint32_t id = 0; id < db->typed_projection_edge_count; ++id) {
		struct prototype_typed_projection_edge* edge =
			&db->typed_projection_edges[id];
		if (edge->parent_typed_projection >= db->typed_projection_count ||
			edge->child_typed_projection >= db->typed_projection_count) {
			return -1;
		}
		edge->key_hash = typed_projection_edge_hash(
			edge->kind, edge->parent_typed_projection, edge->source_relation
		);
		size_t bucket;
		if (prototype_intern_index_bucket(
				edge->key_hash,
				PROTOTYPE_CONTEXT_PROJECTION_INDEX_BUCKET_COUNT,
				&bucket
			) != 0) {
			return -1;
		}
		edge->hash_next = db->typed_projection_edge_index_heads[bucket];
		edge->parent_next = db->typed_projection_edge_parent_heads[
			edge->parent_typed_projection
		];
		db->typed_projection_edge_index_heads[bucket] = id;
		db->typed_projection_edge_parent_heads[edge->parent_typed_projection] = id;
	}
	db->solution_revision = 0;
	return 0;
}
