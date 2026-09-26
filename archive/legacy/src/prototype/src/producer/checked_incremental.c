#include "a_program/producer/checked_incremental.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum checked_incremental_goal_kind {
	CHECKED_INCREMENTAL_TERM_GOAL = 1,
	CHECKED_INCREMENTAL_TYPE_GOAL = 2,
	CHECKED_INCREMENTAL_CONSTRUCTOR_GOAL = 3,
	CHECKED_INCREMENTAL_SUPPORT_GOAL = 4
};

struct prototype_checked_incremental_snapshot {
	struct prototype_incremental_goal* goals;
	struct prototype_incremental_support* supports;
	struct prototype_incremental_positive_observation* positive;
	size_t goal_count;
	size_t positive_count;
};

static const char* checked_symbol(
	const struct prototype_elaborated_module_view* module,
	int symbol_id
) {
	return module && symbol_id >= 0 &&
		(size_t)symbol_id < module->symbols.count ?
		module->symbols.strings[symbol_id] : NULL;
}

static void checked_content_fingerprint(
	const unsigned char* bytes,
	size_t count,
	uint64_t fingerprint[4]
) {
	static const uint64_t offsets[4] = {
		UINT64_C(1469598103934665603), UINT64_C(7809847782465536322),
		UINT64_C(9650029242287828579), UINT64_C(2870177450012600261)
	};
	static const uint64_t primes[4] = {
		UINT64_C(1099511628211), UINT64_C(14029467366897019727),
		UINT64_C(1609587929392839161), UINT64_C(9650029242287828579)
	};
	memcpy(fingerprint, offsets, sizeof(offsets));
	for (size_t i = 0; i < count; ++i) {
		for (size_t lane = 0; lane < 4; ++lane) {
			fingerprint[lane] ^= (uint64_t)bytes[i] + (lane << 8);
			fingerprint[lane] *= primes[lane];
			fingerprint[lane] ^= fingerprint[lane] >> (13 + lane);
		}
	}
}

static int checked_export_goal_key(
	const struct prototype_elaborated_module_view* module,
	int kind,
	size_t index,
	struct prototype_goal_key* p_key
) {
	const char* namespace_name;
	const char* declaration_name;
	const void* path = NULL;
	size_t path_size = 0;
	if (!module || !p_key) return -1;
	switch (kind) {
	case PROTOTYPE_CHECKED_EXPORT_TERM: {
		if (index >= module->interface.term_export_count) return -1;
		const struct prototype_semantic_term_export* export =
			&module->interface.term_exports[index];
		namespace_name = checked_symbol(module, export->namespace_symbol_id);
		declaration_name = checked_symbol(module, export->name_symbol_id);
		kind = CHECKED_INCREMENTAL_TERM_GOAL;
		break;
	}
	case PROTOTYPE_CHECKED_EXPORT_TYPE: {
		if (index >= module->interface.type_export_count) return -1;
		const struct prototype_semantic_type_export* export =
			&module->interface.type_exports[index];
		namespace_name = checked_symbol(module, export->namespace_symbol_id);
		declaration_name = checked_symbol(module, export->name_symbol_id);
		kind = CHECKED_INCREMENTAL_TYPE_GOAL;
		break;
	}
	case PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR: {
		if (index >= module->interface.constructor_export_count) return -1;
		const struct prototype_semantic_constructor_export* export =
			&module->interface.constructor_exports[index];
		if (export->type_export >= module->interface.type_export_count) return -1;
		const struct prototype_semantic_type_export* owner =
			&module->interface.type_exports[export->type_export];
		namespace_name = checked_symbol(module, owner->namespace_symbol_id);
		declaration_name = checked_symbol(module, export->name_symbol_id);
		const char* owner_name = checked_symbol(module, owner->name_symbol_id);
		if (!owner_name) return -1;
		path = owner_name;
		path_size = strlen(owner_name);
		kind = CHECKED_INCREMENTAL_CONSTRUCTOR_GOAL;
		break;
	}
	default:
		return -1;
	}
	if (!declaration_name) return -1;
	return prototype_goal_key_make(
		(uint32_t)kind, namespace_name, declaration_name, path, path_size, p_key
	);
}

static int checked_dependency_goal_key(
	const struct prototype_elaborated_module_view* module,
	const struct prototype_semantic_dependency* dependency,
	struct prototype_goal_key* p_key
) {
	const char* namespace_name = checked_symbol(
		module, dependency->namespace_symbol_id
	);
	const char* declaration_name = checked_symbol(module, dependency->name_symbol_id);
	return declaration_name ? prototype_goal_key_make(
		CHECKED_INCREMENTAL_TERM_GOAL,
		namespace_name,
		declaration_name,
		NULL,
		0,
		p_key
	) : -1;
}

static int checked_goal_compare(const void* a, const void* b) {
	const struct prototype_incremental_goal* left = a;
	const struct prototype_incremental_goal* right = b;
	return prototype_goal_key_compare(&left->key, &right->key);
}

static size_t checked_goal_find(
	const struct prototype_incremental_goal* goals,
	size_t count,
	const struct prototype_goal_key* key
) {
	size_t first = 0;
	while (first < count) {
		size_t middle = first + (count - first) / 2;
		int compared = prototype_goal_key_compare(&goals[middle].key, key);
		if (compared < 0) first = middle + 1;
		else if (compared > 0) count = middle;
		else return middle;
	}
	return SIZE_MAX;
}

static int checked_support_key(
	const struct prototype_goal_key* goal,
	struct prototype_goal_key* p_key
) {
	return prototype_goal_key_make(
		CHECKED_INCREMENTAL_SUPPORT_GOAL,
		NULL,
		"checked-export-support",
		goal,
		sizeof(*goal),
		p_key
	);
}

static size_t checked_module_export_count(
	const struct prototype_elaborated_module_view* module
) {
	return module->interface.term_export_count +
		module->interface.type_export_count +
		module->interface.constructor_export_count;
}

static int checked_module_goal_key_at(
	const struct prototype_elaborated_module_view* module,
	size_t index,
	struct prototype_goal_key* p_key
) {
	if (index < module->interface.term_export_count) {
		return checked_export_goal_key(
			module, PROTOTYPE_CHECKED_EXPORT_TERM, index, p_key
		);
	}
	index -= module->interface.term_export_count;
	if (index < module->interface.type_export_count) {
		return checked_export_goal_key(
			module, PROTOTYPE_CHECKED_EXPORT_TYPE, index, p_key
		);
	}
	index -= module->interface.type_export_count;
	return checked_export_goal_key(
		module, PROTOTYPE_CHECKED_EXPORT_CONSTRUCTOR, index, p_key
	);
}

int prototype_checked_incremental_snapshot_create(
	const struct prototype_checked_module_set* set,
	struct prototype_checked_incremental_snapshot** p_snapshot
) {
	if (!set || !p_snapshot) return -1;
	*p_snapshot = NULL;
	struct prototype_checked_incremental_snapshot* snapshot = calloc(
		1, sizeof(*snapshot)
	);
	if (!snapshot) return -1;
	size_t module_count = prototype_checked_module_set_count(set);
	size_t positive_capacity = 0;
	for (size_t i = 0; i < module_count; ++i) {
		const struct prototype_checked_module* checked =
			prototype_checked_module_set_at(set, i);
		const struct prototype_elaborated_module_view* module =
			prototype_checked_module_elaborated_view(checked);
		if (!module) goto fail;
		size_t export_count = checked_module_export_count(module);
		if (SIZE_MAX - snapshot->goal_count < export_count ||
			(module->interface.dependency_count != 0 && export_count >
				(SIZE_MAX - positive_capacity) /
				module->interface.dependency_count)) goto fail;
		snapshot->goal_count += export_count;
		positive_capacity += export_count * module->interface.dependency_count;
	}
	if (SIZE_MAX - positive_capacity < snapshot->goal_count) goto fail;
	positive_capacity += snapshot->goal_count;
	snapshot->goals = snapshot->goal_count == 0 ? NULL : calloc(
		snapshot->goal_count, sizeof(*snapshot->goals)
	);
	snapshot->supports = snapshot->goal_count == 0 ? NULL : calloc(
		snapshot->goal_count, sizeof(*snapshot->supports)
	);
	snapshot->positive = positive_capacity == 0 ? NULL : calloc(
		positive_capacity, sizeof(*snapshot->positive)
	);
	if ((snapshot->goal_count != 0 &&
		(!snapshot->goals || !snapshot->supports)) ||
		(positive_capacity != 0 && !snapshot->positive)) goto fail;

	size_t goal_cursor = 0;
	for (size_t i = 0; i < module_count; ++i) {
		const struct prototype_checked_module* checked =
			prototype_checked_module_set_at(set, i);
		const struct prototype_elaborated_module_view* module =
			prototype_checked_module_elaborated_view(checked);
		const unsigned char* bytes;
		size_t byte_count;
		uint64_t fingerprint[4];
		if (prototype_checked_module_set_canonical_image_at(
				set, i, &bytes, &byte_count
			) != 0) goto fail;
		checked_content_fingerprint(bytes, byte_count, fingerprint);
		size_t export_count = checked_module_export_count(module);
		for (size_t j = 0; j < export_count; ++j) {
			if (checked_module_goal_key_at(
					module, j, &snapshot->goals[goal_cursor].key
				) != 0) goto fail;
			memcpy(
				snapshot->goals[goal_cursor].content_fingerprint,
				fingerprint,
				sizeof(fingerprint)
			);
			goal_cursor += 1;
		}
	}
	qsort(snapshot->goals, snapshot->goal_count, sizeof(*snapshot->goals),
		checked_goal_compare);
	for (size_t i = 0; i < snapshot->goal_count; ++i) {
		if ((i != 0 && prototype_goal_key_equal(
				&snapshot->goals[i - 1].key, &snapshot->goals[i].key
			)) || checked_support_key(
				&snapshot->goals[i].key, &snapshot->supports[i].support
			) != 0) goto fail;
		snapshot->supports[i].goal = snapshot->goals[i].key;
		snapshot->positive[snapshot->positive_count] =
			(struct prototype_incremental_positive_observation) {
				.support = snapshot->supports[i].support,
				.dependency = snapshot->goals[i].key
			};
		memcpy(
			snapshot->positive[snapshot->positive_count].observed_fingerprint,
			snapshot->goals[i].content_fingerprint,
			sizeof(snapshot->goals[i].content_fingerprint)
		);
		snapshot->positive_count += 1;
	}

	for (size_t i = 0; i < module_count; ++i) {
		const struct prototype_elaborated_module_view* module =
			prototype_checked_module_elaborated_view(
				prototype_checked_module_set_at(set, i)
			);
		size_t export_count = checked_module_export_count(module);
		for (size_t j = 0; j < export_count; ++j) {
			struct prototype_goal_key owner_key;
			if (checked_module_goal_key_at(module, j, &owner_key) != 0) goto fail;
			size_t owner = checked_goal_find(
				snapshot->goals, snapshot->goal_count, &owner_key
			);
			if (owner == SIZE_MAX) goto fail;
			for (size_t k = 0; k < module->interface.dependency_count; ++k) {
				struct prototype_goal_key dependency_key;
				if (checked_dependency_goal_key(
						module, &module->interface.dependencies[k], &dependency_key
					) != 0) goto fail;
				size_t dependency = checked_goal_find(
					snapshot->goals, snapshot->goal_count, &dependency_key
				);
				if (dependency == SIZE_MAX) goto fail;
				snapshot->positive[snapshot->positive_count] =
					(struct prototype_incremental_positive_observation) {
						.support = snapshot->supports[owner].support,
						.dependency = dependency_key
					};
				memcpy(
					snapshot->positive[snapshot->positive_count].observed_fingerprint,
					snapshot->goals[dependency].content_fingerprint,
					sizeof(snapshot->goals[dependency].content_fingerprint)
				);
				snapshot->positive_count += 1;
			}
		}
	}
	*p_snapshot = snapshot;
	return 0;

fail:
	prototype_checked_incremental_snapshot_destroy(snapshot);
	return -1;
}

void prototype_checked_incremental_snapshot_destroy(
	struct prototype_checked_incremental_snapshot* snapshot
) {
	if (!snapshot) return;
	free(snapshot->positive);
	free(snapshot->supports);
	free(snapshot->goals);
	free(snapshot);
}

size_t prototype_checked_incremental_snapshot_goal_count(
	const struct prototype_checked_incremental_snapshot* snapshot
) {
	return snapshot ? snapshot->goal_count : 0;
}

const struct prototype_incremental_goal*
prototype_checked_incremental_snapshot_goal_at(
	const struct prototype_checked_incremental_snapshot* snapshot,
	size_t index
) {
	return snapshot && index < snapshot->goal_count ?
		&snapshot->goals[index] : NULL;
}

int prototype_checked_incremental_compare(
	const struct prototype_checked_incremental_snapshot* previous,
	const struct prototype_checked_incremental_snapshot* current,
	unsigned char* invalidated_goals,
	size_t invalidated_capacity
) {
	if (!previous || !current) return -1;
	const struct prototype_incremental_graph graph = {
		.goals = previous->goals,
		.goal_count = previous->goal_count,
		.supports = previous->supports,
		.support_count = previous->goal_count,
		.positive = previous->positive,
		.positive_count = previous->positive_count
	};
	const struct prototype_incremental_snapshot current_view = {
		.goals = current->goals,
		.goal_count = current->goal_count
	};
	return prototype_incremental_compute_invalidation(
		&graph, &current_view, invalidated_goals, invalidated_capacity
	);
}
