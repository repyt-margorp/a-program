#include "a_program/kernel/context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"
#include "a_program/kernel/type_term_debug.h"
#include "a_program/support/storage.h"

static uint64_t graph_key_hash_mix(uint64_t hash, uint32_t value) {
	hash ^= value;
	hash *= UINT64_C(1099511628211);
	return hash;
}

static uint64_t context_key_hash(
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier_equation,
	int extension_kind,
	uint32_t producer_occurrence
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = graph_key_hash_mix(hash, parent);
	hash = graph_key_hash_mix(hash, binding_id);
	hash = graph_key_hash_mix(hash, (uint32_t)extension_kind);
	hash = graph_key_hash_mix(hash, producer_occurrence);
	hash = graph_key_hash_mix(hash, classifier_equation);
	return hash;
}

static uint64_t classifier_equation_key_hash(
	uint32_t binding_id,
	uint32_t parent_context,
	int key_kind,
	uint32_t key_value,
	uint32_t key_auxiliary,
	int extension_kind,
	uint32_t producer_occurrence
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = graph_key_hash_mix(hash, binding_id);
	hash = graph_key_hash_mix(hash, parent_context);
	hash = graph_key_hash_mix(hash, (uint32_t)key_kind);
	hash = graph_key_hash_mix(hash, key_value);
	hash = graph_key_hash_mix(hash, key_auxiliary);
	hash = graph_key_hash_mix(hash, (uint32_t)extension_kind);
	return graph_key_hash_mix(hash, producer_occurrence);
}

static uint64_t substitution_key_hash(
	const struct prototype_substitution* substitution
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = graph_key_hash_mix(hash, (uint32_t)substitution->kind);
	hash = graph_key_hash_mix(hash, substitution->source_context);
	hash = graph_key_hash_mix(hash, substitution->target_context);
	hash = graph_key_hash_mix(hash, substitution->first);
	hash = graph_key_hash_mix(hash, substitution->second);
	hash = graph_key_hash_mix(hash, substitution->term);
	return hash;
}

static uint64_t comprehension_action_key_hash(
	uint32_t source_extension,
	uint32_t base_substitution
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = graph_key_hash_mix(hash, source_extension);
	hash = graph_key_hash_mix(hash, base_substitution);
	return hash;
}

static void graph_index_clear(uint32_t* heads) {
	prototype_intern_index_clear(
		heads,
		PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT,
		PROTOTYPE_INVALID_ID
	);
}

static size_t graph_index_bucket(uint64_t key_hash) {
	size_t bucket = 0;
	(void)prototype_intern_index_bucket(
		key_hash,
		PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT,
		&bucket
	);
	return bucket;
}

void prototype_context_db_init(
	struct prototype_context_db* db,
	struct prototype_context* contexts,
	size_t context_capacity
) {
	if (!db || !contexts || context_capacity == 0) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->semantic_revision = 1;
	graph_index_clear(db->index_heads);
	graph_index_clear(db->comprehension_action_index_heads);
	prototype_intern_index_clear(
		db->classifier_equation_index_heads,
		PROTOTYPE_CONTEXT_CLASSIFIER_EQUATION_INDEX_CAPACITY,
		PROTOTYPE_INVALID_ID
	);
	db->contexts = contexts;
	db->context_capacity = context_capacity;
	db->context_count = 1;
	db->contexts[0].parent = PROTOTYPE_INVALID_ID;
	db->contexts[0].binding_id = PROTOTYPE_INVALID_ID;
	db->contexts[0].classifier_equation = PROTOTYPE_INVALID_ID;
	db->contexts[0].extension_kind = PROTOTYPE_CONTEXT_EXTENSION_INVALID;
	db->contexts[0].producer_occurrence = PROTOTYPE_INVALID_ID;
	db->contexts[0].depth = 0;
	db->contexts[0].key_hash = 0;
	db->contexts[0].hash_next = PROTOTYPE_INVALID_ID;
}

int prototype_context_db_rebuild_runtime_index_after_bulk_load(
	struct prototype_context_db* db
) {
	if (!db || !db->contexts || db->context_count == 0 ||
		db->context_count > db->context_capacity) {
		return -1;
	}
	db->semantic_revision++;
	if (db->semantic_revision == 0) {
		db->semantic_revision = 1;
	}
	graph_index_clear(db->index_heads);
	graph_index_clear(db->comprehension_action_index_heads);
	db->comprehension_action_count = 0;
	for (uint32_t i = 0; i < db->context_count; ++i) {
		db->contexts[i].hash_next = PROTOTYPE_INVALID_ID;
		if (i == 0) {
			db->contexts[i].key_hash = 0;
			continue;
		}
		struct prototype_context* context = &db->contexts[i];
		context->key_hash = context_key_hash(
			context->parent,
			context->binding_id,
			context->classifier_equation,
			context->extension_kind,
			context->producer_occurrence
		);
		size_t bucket = context->key_hash %
			PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT;
		context->hash_next = db->index_heads[bucket];
		db->index_heads[bucket] = i;
	}
	prototype_intern_index_clear(
		db->classifier_equation_index_heads,
		PROTOTYPE_CONTEXT_CLASSIFIER_EQUATION_INDEX_CAPACITY,
		PROTOTYPE_INVALID_ID
	);
	for (uint32_t i = 0; i < db->classifier_equation_count; ++i) {
		struct prototype_context_classifier_equation* equation =
			&db->classifier_equations[i];
		equation->key_hash = classifier_equation_key_hash(
			equation->binding_id, equation->parent_context,
			equation->key_kind, equation->key_value,
			equation->key_auxiliary,
			equation->key_extension_kind,
			equation->key_producer_occurrence
		);
		size_t bucket = equation->key_hash %
			PROTOTYPE_CONTEXT_CLASSIFIER_EQUATION_INDEX_CAPACITY;
		equation->hash_next = db->classifier_equation_index_heads[bucket];
		db->classifier_equation_index_heads[bucket] = i;
	}
	return 0;
}

uint32_t prototype_context_empty(const struct prototype_context_db* db) {
	return db && db->context_count > 0 ? 0 : PROTOTYPE_INVALID_ID;
}

const struct prototype_context* prototype_context_get(
	const struct prototype_context_db* db,
	uint32_t context_id
) {
	return db && context_id < db->context_count ? &db->contexts[context_id] : NULL;
}

uint32_t prototype_context_classifier_equation(
	const struct prototype_context* context
) {
	return context ? context->classifier_equation : PROTOTYPE_INVALID_ID;
}

const struct prototype_context_classifier_equation*
prototype_context_classifier_equation_get(
	const struct prototype_context_db* db,
	uint32_t equation_id
) {
	if (!db || equation_id >= db->classifier_equation_count ||
		db->classifier_equations[equation_id].id != equation_id) return NULL;
	return &db->classifier_equations[equation_id];
}

static int context_classifier_equation_read(
	const struct prototype_context_db* db,
	uint32_t equation_id,
	uint32_t* p_classifier
) {
	const struct prototype_context_classifier_equation* equation =
		prototype_context_classifier_equation_get(db, equation_id);
	if (!equation || !p_classifier) return -1;
	if (equation->answer == PROTOTYPE_INVALID_ID) return 1;
	*p_classifier = equation->answer;
	return 0;
}

int prototype_context_classifier_read(
	const struct prototype_context_db* db,
	uint32_t context_id,
	uint32_t* p_classifier
) {
	const struct prototype_context* context = prototype_context_get(db, context_id);
	return context ? context_classifier_equation_read(
		db, context->classifier_equation, p_classifier
	) : -1;
}

uint32_t prototype_context_classifier_answer(
	const struct prototype_context_db* db,
	const struct prototype_context* context
) {
	uint32_t classifier = PROTOTYPE_INVALID_ID;
	return context && context_classifier_equation_read(
		db, context->classifier_equation, &classifier
	) == 0 ? classifier : PROTOTYPE_INVALID_ID;
}

static int context_classifier_equation_intern(
	struct prototype_context_db* db,
	uint32_t binding_id,
	uint32_t source_ast_binder_id,
	uint32_t parent_context,
	int key_kind,
	uint32_t key_value,
	uint32_t key_auxiliary,
	int extension_kind,
	uint32_t producer_occurrence,
	uint32_t* p_equation_id
) {
	if (!db || !p_equation_id || binding_id == PROTOTYPE_INVALID_ID ||
		parent_context >= db->context_count) return -1;
	uint64_t hash = classifier_equation_key_hash(
		binding_id, parent_context, key_kind, key_value, key_auxiliary,
		extension_kind, producer_occurrence
	);
	size_t bucket = hash % PROTOTYPE_CONTEXT_CLASSIFIER_EQUATION_INDEX_CAPACITY;
	for (uint32_t id = db->classifier_equation_index_heads[bucket];
		id != PROTOTYPE_INVALID_ID;) {
		const struct prototype_context_classifier_equation* equation =
			prototype_context_classifier_equation_get(db, id);
		if (!equation) return -1;
		if (equation->key_hash == hash && equation->binding_id == binding_id &&
			equation->parent_context == parent_context &&
			equation->key_kind == key_kind && equation->key_value == key_value &&
			equation->key_auxiliary == key_auxiliary &&
			equation->key_extension_kind == extension_kind &&
			equation->key_producer_occurrence == producer_occurrence) {
			if (equation->source_ast_binder_id != PROTOTYPE_INVALID_ID &&
				source_ast_binder_id != PROTOTYPE_INVALID_ID &&
				equation->source_ast_binder_id != source_ast_binder_id) return -1;
			*p_equation_id = id;
			return 0;
		}
		id = equation->hash_next;
	}
	if (db->classifier_equation_count >= PROTOTYPE_CONTEXT_CAPACITY) {
		if (getenv("A_PROGRAM_CONTEXT_TRACE")) {
			fprintf(
				stderr,
				"context classifier equation capacity exhausted count=%u capacity=%u "
				"parent=%u binding=%u\n",
				db->classifier_equation_count,
				PROTOTYPE_CONTEXT_CAPACITY,
				parent_context,
				binding_id
			);
		}
		return -1;
	}
	uint32_t id = db->classifier_equation_count++;
	db->classifier_equations[id] =
		(struct prototype_context_classifier_equation) {
			.id = id,
			.binding_id = binding_id,
			.source_ast_binder_id = source_ast_binder_id,
			.parent_context = parent_context,
			.extension_context = PROTOTYPE_INVALID_ID,
			.owner_occurrence = PROTOTYPE_INVALID_ID,
			.answer = PROTOTYPE_INVALID_ID,
			.evidence_constraint_id = PROTOTYPE_INVALID_ID,
			.key_kind = key_kind,
			.key_value = key_value,
			.key_auxiliary = key_auxiliary,
			.key_extension_kind = extension_kind,
			.key_producer_occurrence = producer_occurrence,
			.key_hash = hash,
			.hash_next = db->classifier_equation_index_heads[bucket]
		};
	db->classifier_equation_index_heads[bucket] = id;
	*p_equation_id = id;
	return 0;
}

int prototype_context_classifier_equation_intern(
	struct prototype_context_db* db,
	uint32_t binding_id,
	uint32_t source_ast_binder_id,
	uint32_t parent_context,
	uint32_t* p_equation_id
) {
	return context_classifier_equation_intern(
		db, binding_id, source_ast_binder_id, parent_context,
		CONTEXT_CLASSIFIER_EQUATION_SOURCE, source_ast_binder_id,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_CONTEXT_EXTENSION_INVALID, PROTOTYPE_INVALID_ID,
		p_equation_id
	);
}

static int context_classifier_equation_intern_answer(
	struct prototype_context_db* db,
	uint32_t binding_id,
	uint32_t parent_context,
	uint32_t answer,
	int extension_kind,
	uint32_t producer_occurrence,
	uint32_t* p_equation_id
) {
	return context_classifier_equation_intern(
		db, binding_id, PROTOTYPE_INVALID_ID, parent_context,
		CONTEXT_CLASSIFIER_EQUATION_ANSWER, answer, PROTOTYPE_INVALID_ID,
		extension_kind, producer_occurrence, p_equation_id
	);
}

static int context_classifier_equation_intern_reindex(
	struct prototype_context_db* db,
	uint32_t binding_id,
	uint32_t parent_context,
	uint32_t source_equation,
	uint32_t substitution,
	int extension_kind,
	uint32_t producer_occurrence,
	uint32_t* p_equation_id
) {
	if (!prototype_context_classifier_equation_get(db, source_equation) ||
		substitution == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	return context_classifier_equation_intern(
		db,
		binding_id,
		PROTOTYPE_INVALID_ID,
		parent_context,
		CONTEXT_CLASSIFIER_EQUATION_REINDEX,
		source_equation,
		substitution,
		extension_kind,
		producer_occurrence,
		p_equation_id
	);
}

int prototype_context_classifier_equation_attach_context(
	struct prototype_context_db* db,
	uint32_t equation_id,
	uint32_t extension_context
) {
	if (!db || extension_context >= db->context_count ||
		equation_id >= db->classifier_equation_count) return -1;
	struct prototype_context_classifier_equation* equation =
		&db->classifier_equations[equation_id];
	if (equation->id != equation_id ||
		(equation->extension_context != PROTOTYPE_INVALID_ID &&
		 equation->extension_context != extension_context)) return -1;
	equation->extension_context = extension_context;
	return 0;
}

int prototype_context_classifier_equation_transition(
	struct prototype_context_db* db,
	uint32_t equation_id,
	uint32_t expected_answer,
	uint32_t next_answer
) {
	if (!db || equation_id >= db->classifier_equation_count ||
		next_answer == PROTOTYPE_INVALID_ID) return -1;
	struct prototype_context_classifier_equation* equation =
		&db->classifier_equations[equation_id];
	if (equation->id != equation_id || equation->answer != expected_answer ||
		(equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_ANSWER &&
		 expected_answer != PROTOTYPE_INVALID_ID && expected_answer != next_answer)) {
		return -1;
	}
	if (expected_answer == next_answer) {
		return 0;
	}
	equation->answer = next_answer;
	db->semantic_revision++;
	if (db->semantic_revision == 0) {
		db->semantic_revision = 1;
	}
	return 0;
}

int prototype_context_classifier_equation_publish(
	struct prototype_context_db* db,
	uint32_t equation_id,
	uint32_t classifier
) {
	if (!db || equation_id >= db->classifier_equation_count ||
		classifier == PROTOTYPE_INVALID_ID) return -1;
	const struct prototype_context_classifier_equation* equation =
		&db->classifier_equations[equation_id];
	if (equation->id != equation_id ||
		(equation->answer != PROTOTYPE_INVALID_ID &&
		 equation->answer != classifier && equation->key_kind !=
			CONTEXT_CLASSIFIER_EQUATION_REINDEX)) return -1;
	return prototype_context_classifier_equation_transition(
		db, equation_id, equation->answer, classifier
	);
}
static int context_classifier_reindex_value(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t classifier,
	uint32_t substitution,
	uint32_t* p_reindexed
) {
	uint32_t reindexed;
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_reindexed || prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			classifier,
			substitution,
			&reindexed
		) != 0 || prototype_judgement_classifier_value_whnf(
			terms, type_declarations, reindexed, &reindexed
		) != 0) {
		return -1;
	}
	*p_reindexed = reindexed;
	return 0;
}

int prototype_context_classifier_reindex_solve(
	struct prototype_context_db* db,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t* changed_equations,
	size_t changed_equation_capacity,
	size_t* p_changed_equation_count
) {
	if (!db || !substitutions || !terms || !type_declarations ||
		!changed_equations || !p_changed_equation_count) {
		return -1;
	}
	*p_changed_equation_count = 0;
	for (uint32_t id = 0; id < db->classifier_equation_count; ++id) {
		struct prototype_context_classifier_equation* equation =
			&db->classifier_equations[id];
		if (equation->key_kind != CONTEXT_CLASSIFIER_EQUATION_REINDEX) {
			continue;
		}
		const struct prototype_context_classifier_equation* source =
			prototype_context_classifier_equation_get(db, equation->key_value);
		if (!source || equation->key_auxiliary >= substitutions->substitution_count) {
			return -1;
		}
		if (source->answer == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t classifier;
		if (context_classifier_reindex_value(
				db,
				substitutions,
				terms,
				type_declarations,
				source->answer,
				equation->key_auxiliary,
				&classifier
			) != 0) {
			return -1;
		}
		int answer_changed = equation->answer != classifier;
		if (prototype_context_classifier_equation_publish(
				db, id, classifier
			) != 0) {
			return -1;
		}
		if (getenv("A_PROGRAM_CONTEXT_TRACE")) {
			const struct prototype_substitution* action =
				prototype_substitution_get(substitutions, equation->key_auxiliary);
			fprintf(stderr,
				"context classifier reindex equation=%u extension=%u source-equation=%u "
				"source-answer=%u substitution=%u source-context=%u target-context=%u "
				"answer=%u\n",
				id, equation->extension_context, equation->key_value,
				source->answer, equation->key_auxiliary,
				action ? action->source_context : PROTOTYPE_INVALID_ID,
				action ? action->target_context : PROTOTYPE_INVALID_ID,
				classifier);
		}
		if (answer_changed) {
			if (*p_changed_equation_count >= changed_equation_capacity) {
				return -1;
			}
			changed_equations[(*p_changed_equation_count)++] = id;
		}
	}
	return 0;
}

int prototype_context_classifier_view_read(
	const struct prototype_context_classifier_view* view,
	uint32_t context_id,
	uint32_t* p_classifier
) {
	if (!view || !view->contexts || !p_classifier) {
		return -1;
	}
	const struct prototype_context* context = prototype_context_get(
		view->contexts, context_id
	);
	if (!context) {
		return -1;
	}
	if (!view->classifier_answer) {
		return context_classifier_equation_read(
			view->contexts, context->classifier_equation, p_classifier
		);
	}
	return view->classifier_answer(
		view->classifier_equations, context_id, p_classifier
	);
}

static int prototype_context_extend_internal(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier_equation,
	int extension_kind,
	uint32_t producer_occurrence,
	uint32_t* p_context
) {
	const struct prototype_context_classifier_equation* equation =
		prototype_context_classifier_equation_get(db, classifier_equation);
	if (!db || !p_context || parent >= db->context_count ||
		binding_id == PROTOTYPE_INVALID_ID ||
		!equation || equation->binding_id != binding_id ||
		equation->parent_context != parent ||
		(extension_kind != PROTOTYPE_CONTEXT_EXTENSION_VALUE &&
		 extension_kind != PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT) ||
		(extension_kind == PROTOTYPE_CONTEXT_EXTENSION_VALUE &&
		 producer_occurrence != PROTOTYPE_INVALID_ID) ||
		(extension_kind == PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT &&
		 producer_occurrence == PROTOTYPE_INVALID_ID)) {
		return -1;
	}
	db->intern_requests++;
	uint64_t key_hash = context_key_hash(
		parent, binding_id, classifier_equation,
		extension_kind, producer_occurrence
	);
	size_t bucket = graph_index_bucket(key_hash);
	for (uint32_t i = db->index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = db->contexts[i].hash_next) {
		db->intern_probes++;
		if (i == 0 || i >= db->context_count) {
			return -1;
		}
		const struct prototype_context* context = &db->contexts[i];
		int same_extension =
			context->classifier_equation == classifier_equation &&
			context->extension_kind == extension_kind &&
			context->producer_occurrence == producer_occurrence;
		/* Binding objects are graph identity. Equal classifiers do not make two
		 * independently allocated context extensions interchangeable. */
		if (context->parent == parent && context->binding_id == binding_id &&
			context->key_hash == key_hash && same_extension) {
			if (prototype_context_classifier_equation_attach_context(
					db, classifier_equation, i
				) != 0) return -1;
			db->intern_hits++;
			*p_context = i;
			return 0;
		}
	}
	if (db->context_count >= db->context_capacity) {
		if (getenv("A_PROGRAM_CONTEXT_TRACE")) {
			fprintf(
				stderr,
				"context capacity exhausted count=%zu capacity=%zu parent=%u "
				"binding=%u equation=%u\n",
				db->context_count,
				db->context_capacity,
				parent,
				binding_id,
				classifier_equation
			);
		}
		return -1;
	}
	uint32_t id = (uint32_t)db->context_count++;
	db->contexts[id].parent = parent;
	db->contexts[id].binding_id = binding_id;
	db->contexts[id].classifier_equation = classifier_equation;
	db->contexts[id].extension_kind = extension_kind;
	db->contexts[id].producer_occurrence = producer_occurrence;
	db->contexts[id].depth = db->contexts[parent].depth + 1;
	db->contexts[id].key_hash = key_hash;
	db->contexts[id].hash_next = db->index_heads[bucket];
	db->index_heads[bucket] = id;
	if (prototype_context_classifier_equation_attach_context(
			db, classifier_equation, id
		) != 0) return -1;
	*p_context = id;
	return 0;
}

int prototype_context_extend(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t* p_context
) {
	uint32_t classifier_equation;
	if (context_classifier_equation_intern_answer(
			db, binding_id, parent, classifier,
			PROTOTYPE_CONTEXT_EXTENSION_VALUE, PROTOTYPE_INVALID_ID,
			&classifier_equation
		) != 0 || prototype_context_classifier_equation_publish(
			db, classifier_equation, classifier
		) != 0) return -1;
	return prototype_context_extend_internal(
		db, parent, binding_id, classifier_equation,
		PROTOTYPE_CONTEXT_EXTENSION_VALUE, PROTOTYPE_INVALID_ID, p_context
	);
}

int prototype_context_extend_equation(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier_equation,
	uint32_t* p_context
) {
	return prototype_context_extend_internal(
		db, parent, binding_id, classifier_equation,
		PROTOTYPE_CONTEXT_EXTENSION_VALUE, PROTOTYPE_INVALID_ID, p_context
	);
}

int prototype_context_extend_sequence_result(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t producer_occurrence,
	uint32_t* p_context
) {
	uint32_t classifier_equation;
	if (context_classifier_equation_intern_answer(
			db, binding_id, parent, classifier,
			PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT, producer_occurrence,
			&classifier_equation
		) != 0 || prototype_context_classifier_equation_publish(
			db, classifier_equation, classifier
		) != 0) return -1;
	return prototype_context_extend_internal(
		db, parent, binding_id, classifier_equation,
		PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT,
		producer_occurrence, p_context
	);
}

int prototype_context_extend_sequence_result_equation(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier_equation,
	uint32_t producer_occurrence,
	uint32_t* p_context
) {
	return prototype_context_extend_internal(
		db, parent, binding_id, classifier_equation,
		PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT,
		producer_occurrence, p_context
	);
}

int prototype_context_contains_binding(
	const struct prototype_context_db* db,
	uint32_t context_id,
	uint32_t binding_id
) {
	uint32_t entry_context_id;
	return prototype_context_find_binding(
		db, context_id, binding_id, &entry_context_id
	) == 0;
}

int prototype_context_is_ancestor(
	const struct prototype_context_db* db,
	uint32_t ancestor_context_id,
	uint32_t descendant_context_id
) {
	const struct prototype_context* ancestor = prototype_context_get(
		db, ancestor_context_id
	);
	const struct prototype_context* descendant = prototype_context_get(
		db, descendant_context_id
	);
	if (!ancestor || !descendant) {
		return -1;
	}
	if (ancestor->depth > descendant->depth) {
		return 0;
	}
	uint32_t cursor = descendant_context_id;
	while (db->contexts[cursor].depth > ancestor->depth) {
		uint32_t parent = db->contexts[cursor].parent;
		if (parent >= cursor) {
			return -1;
		}
		cursor = parent;
	}
	return cursor == ancestor_context_id;
}

int prototype_context_find_binding(
	const struct prototype_context_db* db,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t* p_entry_context_id
) {
	if (!db || context_id >= db->context_count ||
		binding_id == PROTOTYPE_INVALID_ID || !p_entry_context_id) {
		return -1;
	}
	while (context_id != 0) {
		const struct prototype_context* context = &db->contexts[context_id];
		if (context->binding_id == binding_id) {
			*p_entry_context_id = context_id;
			return 0;
		}
		if (context->parent >= context_id) {
			return -1;
		}
		context_id = context->parent;
	}
	return 1;
}

int prototype_context_db_validate(
	const struct prototype_context_db* db,
	const struct prototype_term_db* terms
) {
	if (!db || !terms || !db->contexts || db->context_count == 0 ||
		db->context_count > db->context_capacity ||
		db->classifier_equation_count > PROTOTYPE_CONTEXT_CAPACITY) {
		return -1;
	}
	const struct prototype_context* empty = &db->contexts[0];
	if (empty->parent != PROTOTYPE_INVALID_ID ||
		empty->binding_id != PROTOTYPE_INVALID_ID ||
		empty->classifier_equation != PROTOTYPE_INVALID_ID ||
		empty->extension_kind != PROTOTYPE_CONTEXT_EXTENSION_INVALID ||
		empty->producer_occurrence != PROTOTYPE_INVALID_ID ||
		empty->depth != 0) {
		return -1;
	}
	for (uint32_t i = 1; i < db->context_count; ++i) {
		const struct prototype_context* context = &db->contexts[i];
		uint64_t expected_key_hash = context_key_hash(
			context->parent,
			context->binding_id,
			context->classifier_equation,
			context->extension_kind,
			context->producer_occurrence
		);
		const struct prototype_context_classifier_equation* equation =
			prototype_context_classifier_equation_get(
				db, context->classifier_equation
			);
		if (context->parent >= i ||
			context->binding_id == PROTOTYPE_INVALID_ID ||
			context->key_hash != expected_key_hash ||
			context->depth != db->contexts[context->parent].depth + 1 ||
			!equation || equation->binding_id != context->binding_id ||
			equation->parent_context != context->parent ||
			equation->extension_context != i ||
			(equation->answer != PROTOTYPE_INVALID_ID &&
			 equation->answer >= terms->term_count) ||
			((context->extension_kind == PROTOTYPE_CONTEXT_EXTENSION_VALUE) !=
			 (context->producer_occurrence == PROTOTYPE_INVALID_ID)) ||
			(context->extension_kind != PROTOTYPE_CONTEXT_EXTENSION_VALUE &&
			 context->extension_kind !=
				PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT)) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < db->classifier_equation_count; ++i) {
		const struct prototype_context_classifier_equation* equation =
			&db->classifier_equations[i];
		uint64_t expected_key_hash = classifier_equation_key_hash(
			equation->binding_id, equation->parent_context,
			equation->key_kind, equation->key_value,
			equation->key_auxiliary,
			equation->key_extension_kind,
			equation->key_producer_occurrence
		);
		if (equation->id != i ||
			equation->binding_id == PROTOTYPE_INVALID_ID ||
			equation->parent_context >= db->context_count ||
			equation->extension_context >= db->context_count ||
			db->contexts[equation->extension_context].classifier_equation != i ||
			equation->key_hash != expected_key_hash ||
			(equation->key_kind != CONTEXT_CLASSIFIER_EQUATION_SOURCE &&
			 equation->key_kind != CONTEXT_CLASSIFIER_EQUATION_ANSWER &&
			 equation->key_kind != CONTEXT_CLASSIFIER_EQUATION_REINDEX) ||
			(equation->key_kind == CONTEXT_CLASSIFIER_EQUATION_REINDEX &&
			 (equation->key_value >= i ||
			  equation->key_auxiliary == PROTOTYPE_INVALID_ID)) ||
			(equation->key_kind != CONTEXT_CLASSIFIER_EQUATION_REINDEX &&
			 equation->key_auxiliary != PROTOTYPE_INVALID_ID) ||
			(equation->answer != PROTOTYPE_INVALID_ID &&
			 equation->answer >= terms->term_count)) {
			return -1;
		}
	}
	return 0;
}

int prototype_context_db_append_relocated(
	struct prototype_context_db* target,
	const struct prototype_context_db* source,
	const uint32_t* term_relocation,
	size_t term_relocation_count,
	const uint32_t* binding_relocation,
	size_t binding_relocation_count,
	uint32_t occurrence_offset,
	uint32_t* relocation,
	size_t relocation_capacity
) {
	if (!target || !source || !term_relocation || !binding_relocation ||
		!relocation ||
		source->context_count == 0 ||
		source->context_count > relocation_capacity ||
		prototype_context_empty(target) == PROTOTYPE_INVALID_ID ||
		prototype_context_empty(source) == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	relocation[0] = prototype_context_empty(target);
	for (uint32_t i = 1; i < source->context_count; ++i) {
		const struct prototype_context* context =
			prototype_context_get(source, i);
		uint32_t source_classifier = PROTOTYPE_INVALID_ID;
		const struct prototype_context_classifier_view source_view = {
			.contexts = source
		};
		if (!context || prototype_context_classifier_view_read(
				&source_view, i, &source_classifier
			) != 0) {
			return -1;
		}
		uint32_t classifier = source_classifier < term_relocation_count ?
			term_relocation[source_classifier] : PROTOTYPE_INVALID_ID;
		uint32_t producer_occurrence = context &&
			context->producer_occurrence != PROTOTYPE_INVALID_ID ?
			context->producer_occurrence + occurrence_offset : PROTOTYPE_INVALID_ID;
		if (!context || context->parent >= i ||
			classifier == PROTOTYPE_INVALID_ID ||
			context->binding_id == PROTOTYPE_INVALID_ID ||
			context->binding_id >= binding_relocation_count ||
			binding_relocation[context->binding_id] == PROTOTYPE_INVALID_ID ||
			(context->extension_kind ==
				PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT ?
			 prototype_context_extend_sequence_result(
				target, relocation[context->parent],
				binding_relocation[context->binding_id], classifier,
				producer_occurrence, &relocation[i]
			 ) : prototype_context_extend(
				target, relocation[context->parent],
				binding_relocation[context->binding_id], classifier,
				&relocation[i]
			 )) != 0) {
			fprintf(
				stderr,
				"context append relocation failed context=%u parent=%u binding=%u classifier=%u relocated-classifier=%u producer=%u relocated-producer=%u kind=%d\n",
				i, context ? context->parent : PROTOTYPE_INVALID_ID,
				context ? context->binding_id : PROTOTYPE_INVALID_ID,
				source_classifier, classifier,
				context ? context->producer_occurrence : PROTOTYPE_INVALID_ID,
				producer_occurrence,
				context ? context->extension_kind : 0
			);
			return -1;
		}
	}
	return 0;
}

static int prototype_substitution_add(
	struct prototype_substitution_db* db,
	struct prototype_substitution substitution,
	uint32_t* p_substitution
);

int prototype_substitution_db_append_relocated(
	struct prototype_substitution_db* target,
	const struct prototype_substitution_db* source,
	const uint32_t* context_relocation,
	size_t context_relocation_count,
	const uint32_t* term_relocation,
	size_t term_relocation_count,
	uint32_t* relocation,
	size_t relocation_capacity
) {
	if (!target || !source || !context_relocation || !term_relocation ||
		!relocation ||
		source->substitution_count > relocation_capacity) {
		return -1;
	}
	for (uint32_t i = 0; i < source->substitution_count; ++i) {
		struct prototype_substitution substitution = source->substitutions[i];
		if (substitution.source_context >= context_relocation_count ||
			substitution.target_context >= context_relocation_count ||
			(substitution.first != PROTOTYPE_INVALID_ID &&
				substitution.first >= i) ||
			(substitution.second != PROTOTYPE_INVALID_ID &&
				substitution.second >= i)) {
			return -1;
		}
		substitution.source_context =
			context_relocation[substitution.source_context];
		substitution.target_context =
			context_relocation[substitution.target_context];
		if (substitution.first != PROTOTYPE_INVALID_ID) {
			substitution.first = relocation[substitution.first];
		}
		if (substitution.second != PROTOTYPE_INVALID_ID) {
			substitution.second = relocation[substitution.second];
		}
		if (substitution.term != PROTOTYPE_INVALID_ID) {
			if (substitution.term >= term_relocation_count ||
				term_relocation[substitution.term] == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			substitution.term = term_relocation[substitution.term];
		}
		if (prototype_substitution_add(
			target, substitution, &relocation[i]
		) != 0) {
			return -1;
		}
	}
	return 0;
}

void prototype_substitution_db_init(
	struct prototype_substitution_db* db,
	struct prototype_substitution* substitutions,
	size_t substitution_capacity
) {
	if (!db) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->semantic_revision = 1;
	db->substitutions = substitutions;
	db->substitution_count = 0;
	db->substitution_capacity = substitution_capacity;
	db->intern_requests = 0;
	db->intern_hits = 0;
	db->intern_probes = 0;
	graph_index_clear(db->index_heads);
}

int prototype_substitution_db_rebuild_runtime_index_after_bulk_load(
	struct prototype_substitution_db* db
) {
	if (!db || !db->substitutions ||
		db->substitution_count > db->substitution_capacity) {
		return -1;
	}
	db->semantic_revision++;
	if (db->semantic_revision == 0) {
		db->semantic_revision = 1;
	}
	graph_index_clear(db->index_heads);
	memset(db->reindex_cache, 0, sizeof(db->reindex_cache));
	memset(db->binding_cache, 0, sizeof(db->binding_cache));
	for (uint32_t i = 0; i < db->substitution_count; ++i) {
		struct prototype_substitution* substitution = &db->substitutions[i];
		substitution->key_hash = substitution_key_hash(substitution);
		size_t bucket = substitution->key_hash %
			PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT;
		substitution->hash_next = db->index_heads[bucket];
		db->index_heads[bucket] = i;
	}
	return 0;
}

const struct prototype_substitution* prototype_substitution_get(
	const struct prototype_substitution_db* db,
	uint32_t substitution_id
) {
	if (!db || substitution_id >= db->substitution_count) {
		return NULL;
	}
	return &db->substitutions[substitution_id];
}

int prototype_substitution_term_classifier_in_view(
	const struct prototype_context_classifier_view* context_view,
	struct prototype_substitution_db* db,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t substitution_id,
	uint32_t* p_classifier
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(db, substitution_id);
	uint32_t target_classifier;
	uint32_t classifier;
	if (!context_view || !context_view->contexts || !terms ||
		!type_declarations || !p_classifier || !substitution ||
		substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND ||
		prototype_context_classifier_view_read(
			context_view, substitution->target_context, &target_classifier
		) != 0 || prototype_term_reindex(
			terms,
			type_declarations,
			context_view->contexts,
			db,
			target_classifier,
			substitution->first,
			&classifier
		) != 0 || prototype_judgement_classifier_value_whnf(
			terms, type_declarations, classifier, &classifier
		) != 0) {
		return -1;
	}
	*p_classifier = classifier;
	return 0;
}

static int prototype_substitution_add(
	struct prototype_substitution_db* db,
	struct prototype_substitution substitution,
	uint32_t* p_substitution
) {
	if (!db || !db->substitutions || !p_substitution) {
		return -1;
	}
	db->intern_requests++;
	substitution.key_hash = substitution_key_hash(&substitution);
	size_t bucket = substitution.key_hash %
		PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT;
	for (uint32_t i = db->index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = db->substitutions[i].hash_next) {
		db->intern_probes++;
		if (i >= db->substitution_count) {
			return -1;
		}
		const struct prototype_substitution* existing = &db->substitutions[i];
		if (existing->key_hash == substitution.key_hash &&
			existing->kind == substitution.kind &&
			existing->source_context == substitution.source_context &&
			existing->target_context == substitution.target_context &&
			existing->first == substitution.first &&
			existing->second == substitution.second &&
			existing->term == substitution.term) {
			db->intern_hits++;
			*p_substitution = i;
			return 0;
		}
	}
	if (db->substitution_count >= db->substitution_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)db->substitution_count++;
	substitution.hash_next = db->index_heads[bucket];
	db->substitutions[id] = substitution;
	db->index_heads[bucket] = id;
	*p_substitution = id;
	return 0;
}

int prototype_substitution_rebase(
	struct prototype_substitution_db* db,
	uint32_t substitution_id,
	uint32_t source_context,
	uint32_t target_context,
	uint32_t first,
	uint32_t second,
	uint32_t* p_substitution
) {
	const struct prototype_substitution* source =
		prototype_substitution_get(db, substitution_id);
	if (!source || !p_substitution ||
		source_context == PROTOTYPE_INVALID_ID ||
		target_context == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_substitution rebased = *source;
	rebased.source_context = source_context;
	rebased.target_context = target_context;
	rebased.first = first;
	rebased.second = second;
	rebased.key_hash = 0;
	rebased.hash_next = PROTOTYPE_INVALID_ID;
	switch (rebased.kind) {
		case PROTOTYPE_SUBSTITUTION_IDENTITY:
			if (source_context != target_context ||
				first != PROTOTYPE_INVALID_ID ||
				second != PROTOTYPE_INVALID_ID ||
				rebased.term != PROTOTYPE_INVALID_ID) {
				return -1;
			}
			break;
		case PROTOTYPE_SUBSTITUTION_EMPTY:
		case PROTOTYPE_SUBSTITUTION_PROJECTION:
			if (first != PROTOTYPE_INVALID_ID ||
				second != PROTOTYPE_INVALID_ID ||
				rebased.term != PROTOTYPE_INVALID_ID) {
				return -1;
			}
			break;
		case PROTOTYPE_SUBSTITUTION_EXTEND:
			if (first == PROTOTYPE_INVALID_ID ||
				second != PROTOTYPE_INVALID_ID ||
				rebased.term == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			break;
		case PROTOTYPE_SUBSTITUTION_COMPOSE:
			if (first == PROTOTYPE_INVALID_ID ||
				second == PROTOTYPE_INVALID_ID ||
				rebased.term != PROTOTYPE_INVALID_ID) {
				return -1;
			}
			break;
		default:
			return -1;
	}
	return prototype_substitution_add(db, rebased, p_substitution);
}

int prototype_substitution_identity(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t context,
	uint32_t* p_substitution
) {
	if (!prototype_context_get(contexts, context)) {
		return -1;
	}
	struct prototype_substitution substitution = {
		.kind = PROTOTYPE_SUBSTITUTION_IDENTITY,
		.source_context = context,
		.target_context = context,
		.first = PROTOTYPE_INVALID_ID,
		.second = PROTOTYPE_INVALID_ID,
		.term = PROTOTYPE_INVALID_ID
	};
	return prototype_substitution_add(db, substitution, p_substitution);
}

int prototype_substitution_empty(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t source_context,
	uint32_t* p_substitution
) {
	if (!prototype_context_get(contexts, source_context)) {
		return -1;
	}
	struct prototype_substitution substitution = {
		.kind = PROTOTYPE_SUBSTITUTION_EMPTY,
		.source_context = source_context,
		.target_context = prototype_context_empty(contexts),
		.first = PROTOTYPE_INVALID_ID,
		.second = PROTOTYPE_INVALID_ID,
		.term = PROTOTYPE_INVALID_ID
	};
	return prototype_substitution_add(db, substitution, p_substitution);
}

int prototype_substitution_projection(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t extended_context,
	uint32_t* p_substitution
) {
	const struct prototype_context* context =
		prototype_context_get(contexts, extended_context);
	if (!context || extended_context == prototype_context_empty(contexts)) {
		return -1;
	}
	struct prototype_substitution substitution = {
		.kind = PROTOTYPE_SUBSTITUTION_PROJECTION,
		.source_context = extended_context,
		.target_context = context->parent,
		.first = PROTOTYPE_INVALID_ID,
		.second = PROTOTYPE_INVALID_ID,
		.term = PROTOTYPE_INVALID_ID
	};
	return prototype_substitution_add(db, substitution, p_substitution);
}

int prototype_substitution_extend_after_validation(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t prefix_substitution,
	uint32_t target_context,
	uint32_t term,
	uint32_t* p_substitution
) {
	const struct prototype_substitution* prefix =
		prototype_substitution_get(db, prefix_substitution);
	const struct prototype_context* target =
		prototype_context_get(contexts, target_context);
	if (!db || !contexts || !p_substitution || !prefix || !target ||
		target_context == prototype_context_empty(contexts) ||
		target->parent != prefix->target_context ||
		term == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_substitution substitution = {
		.kind = PROTOTYPE_SUBSTITUTION_EXTEND,
		.source_context = prefix->source_context,
		.target_context = target_context,
		.first = prefix_substitution,
		.second = PROTOTYPE_INVALID_ID,
		.term = term
	};
	return prototype_substitution_add(db, substitution, p_substitution);
}

int prototype_substitution_extend_in_view(
	const struct prototype_context_classifier_view* context_view,
	struct prototype_substitution_db* db,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t target_context,
	uint32_t term,
	uint32_t term_classifier,
	uint32_t* p_substitution
) {
	if (!context_view || !context_view->contexts || !db || !terms ||
		!type_declarations || !p_substitution) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_ARGUMENT;
	}
	const struct prototype_context_db* contexts = context_view->contexts;
	const struct prototype_substitution* prefix =
		prototype_substitution_get(db, prefix_substitution);
	if (!prefix) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_PREFIX;
	}
	if (term == PROTOTYPE_INVALID_ID || term >= terms->term_count) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_TERM_OUT_OF_RANGE;
	}
	if (term_classifier == PROTOTYPE_INVALID_ID ||
		term_classifier >= terms->term_count) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_OUT_OF_RANGE;
	}
	const struct prototype_context* target =
		prototype_context_get(contexts, target_context);
	uint32_t target_classifier;
	if (!target || target_context == prototype_context_empty(contexts) ||
		prototype_context_classifier_view_read(
			context_view, target_context, &target_classifier
		) != 0) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_TARGET;
	}
	if (target->parent != prefix->target_context) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_TARGET_PARENT_MISMATCH;
	}
	uint32_t expected_classifier;
	if (prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			db,
			target_classifier,
			prefix_substitution,
			&expected_classifier
		) != 0) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_REINDEX_FAILED;
	}
	if (prototype_judgement_classifier_value_whnf(
			terms, type_declarations, expected_classifier, &expected_classifier
		) != 0) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_NORMALIZATION_FAILED;
	}
	if (!prototype_judgement_classifier_reference_equal(
			terms,
			type_declarations,
			expected_classifier,
			term_classifier
		)) {
		if (getenv("A_PROGRAM_BRANCH_REFINEMENT_TRACE") != NULL) {
			struct prototype_term_conversion_result conversion =
				prototype_judgement_classifier_conversion(
					terms, type_declarations,
					expected_classifier, term_classifier
				);
			fprintf(
				stderr,
				"substitution extension classifier mismatch target=%u binding=%u "
				"expected=%u expected-tag=%d actual=%u actual-tag=%d conversion=%d\n",
				target_context, target->binding_id,
				expected_classifier, terms->terms[expected_classifier].tag,
				term_classifier, terms->terms[term_classifier].tag,
				conversion.status
			);
			fprintf(stderr, "substitution expected classifier: ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, type_declarations, terms, expected_classifier
			);
			fprintf(stderr, "\nsubstitution actual classifier: ");
			prototype_type_term_print_debug(
				stderr, NULL, NULL, type_declarations, terms, term_classifier
			);
			fprintf(stderr, "\n");
			}
		return PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_MISMATCH;
	}
	return prototype_substitution_extend_after_validation(
		db, contexts, prefix_substitution, target_context, term, p_substitution
	) == 0 ?
		PROTOTYPE_SUBSTITUTION_EXTEND_OK :
		PROTOTYPE_SUBSTITUTION_EXTEND_STORAGE_FAILED;
}

int prototype_substitution_extend(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t target_context,
	uint32_t term,
	uint32_t term_classifier,
	uint32_t* p_substitution
) {
	const struct prototype_context_classifier_view context_view = {
		.contexts = contexts
	};
	return prototype_substitution_extend_in_view(
		&context_view,
		db,
		terms,
		type_declarations,
		prefix_substitution,
		target_context,
		term,
		term_classifier,
		p_substitution
	);
}

const char* prototype_substitution_extend_result_name(int result) {
	switch (result) {
		case PROTOTYPE_SUBSTITUTION_EXTEND_OK:
			return "ok";
		case PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_ARGUMENT:
			return "invalid argument";
		case PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_PREFIX:
			return "invalid prefix substitution";
		case PROTOTYPE_SUBSTITUTION_EXTEND_TERM_OUT_OF_RANGE:
			return "extension term out of range";
		case PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_OUT_OF_RANGE:
			return "extension classifier out of range";
		case PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_TARGET:
			return "invalid target context extension";
		case PROTOTYPE_SUBSTITUTION_EXTEND_TARGET_PARENT_MISMATCH:
			return "target parent does not match prefix target";
		case PROTOTYPE_SUBSTITUTION_EXTEND_REINDEX_FAILED:
			return "target classifier reindex failed";
		case PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_NORMALIZATION_FAILED:
			return "target classifier normalization failed";
		case PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_MISMATCH:
			return "extension classifier mismatch";
		case PROTOTYPE_SUBSTITUTION_EXTEND_STORAGE_FAILED:
			return "substitution storage failed";
		default:
			return "unknown substitution extension result";
	}
}

int prototype_substitution_compose(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t outer_substitution,
	uint32_t inner_substitution,
	uint32_t* p_substitution
) {
	const struct prototype_substitution* outer =
		prototype_substitution_get(db, outer_substitution);
	const struct prototype_substitution* inner =
		prototype_substitution_get(db, inner_substitution);
	if (!outer || !inner ||
		outer->source_context != inner->target_context ||
		!prototype_context_get(contexts, inner->source_context) ||
		!prototype_context_get(contexts, outer->target_context)) {
		return -1;
	}
	struct prototype_substitution substitution = {
		.kind = PROTOTYPE_SUBSTITUTION_COMPOSE,
		.source_context = inner->source_context,
		.target_context = outer->target_context,
		.first = outer_substitution,
		.second = inner_substitution,
		.term = PROTOTYPE_INVALID_ID
	};
	return prototype_substitution_add(db, substitution, p_substitution);
}

int prototype_substitution_projection_path(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t descendant_context,
	uint32_t ancestor_context,
	uint32_t* p_substitution
) {
	if (!db || !contexts || !p_substitution ||
		!prototype_context_get(contexts, descendant_context) ||
		!prototype_context_get(contexts, ancestor_context)) {
		return -1;
	}
	if (descendant_context == ancestor_context) {
		return prototype_substitution_identity(
			db, contexts, descendant_context, p_substitution
		);
	}
	uint32_t cursor = descendant_context;
	uint32_t path = PROTOTYPE_INVALID_ID;
	while (cursor != ancestor_context) {
		const struct prototype_context* context =
			prototype_context_get(contexts, cursor);
		uint32_t projection;
		if (!context || cursor == prototype_context_empty(contexts) ||
			context->parent >= cursor || prototype_substitution_projection(
				db, contexts, cursor, &projection
			) != 0) {
			return -1;
		}
		if (path == PROTOTYPE_INVALID_ID) {
			path = projection;
		} else {
			uint32_t composed;
			if (prototype_substitution_compose(
					db, contexts, projection, path, &composed
				) != 0) {
				return -1;
			}
			path = composed;
		}
		cursor = context->parent;
	}
	*p_substitution = path;
	return 0;
}

static int substitution_is_projection_path_at_depth(
	const struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t substitution_id,
	uint32_t descendant_context,
	uint32_t ancestor_context,
	uint32_t depth
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(db, substitution_id);
	if (!substitution || !contexts || depth > db->substitution_count ||
		substitution->source_context != descendant_context ||
		substitution->target_context != ancestor_context) {
		return 0;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY) {
		return descendant_context == ancestor_context;
	}
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_PROJECTION) {
		const struct prototype_context* context =
			prototype_context_get(contexts, descendant_context);
		return context && descendant_context != prototype_context_empty(contexts) &&
			context->parent == ancestor_context;
	}
	if (substitution->kind != PROTOTYPE_SUBSTITUTION_COMPOSE) {
		return 0;
	}
	const struct prototype_substitution* outer =
		prototype_substitution_get(db, substitution->first);
	const struct prototype_substitution* inner =
		prototype_substitution_get(db, substitution->second);
	return outer && inner && inner->target_context == outer->source_context &&
		substitution_is_projection_path_at_depth(
			db,
			contexts,
			substitution->second,
			descendant_context,
			inner->target_context,
			depth + 1
		) && substitution_is_projection_path_at_depth(
			db,
			contexts,
			substitution->first,
			outer->source_context,
			ancestor_context,
			depth + 1
		);
}

int prototype_substitution_is_projection_path(
	const struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	uint32_t substitution_id,
	uint32_t descendant_context,
	uint32_t ancestor_context
) {
	return substitution_is_projection_path_at_depth(
		db,
		contexts,
		substitution_id,
		descendant_context,
		ancestor_context,
		0
	);
}

int prototype_substitution_db_validate(
	const struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	const struct prototype_term_db* terms
) {
	if (!db || !contexts || !terms) {
		return -1;
	}
	for (uint32_t i = 0; i < db->substitution_count; ++i) {
		const struct prototype_substitution* substitution = &db->substitutions[i];
		if (!prototype_context_get(contexts, substitution->source_context) ||
			!prototype_context_get(contexts, substitution->target_context) ||
			substitution->key_hash != substitution_key_hash(substitution)) {
			return -1;
		}
		switch (substitution->kind) {
			case PROTOTYPE_SUBSTITUTION_IDENTITY:
				if (substitution->source_context != substitution->target_context) {
					return -1;
				}
				break;
			case PROTOTYPE_SUBSTITUTION_EMPTY:
				if (substitution->target_context !=
					prototype_context_empty(contexts)) {
					return -1;
				}
				break;
			case PROTOTYPE_SUBSTITUTION_PROJECTION: {
				const struct prototype_context* source =
					prototype_context_get(contexts, substitution->source_context);
				if (!source || source->parent != substitution->target_context) {
					return -1;
				}
				break;
			}
			case PROTOTYPE_SUBSTITUTION_EXTEND: {
				const struct prototype_substitution* prefix =
					prototype_substitution_get(db, substitution->first);
				const struct prototype_context* target =
					prototype_context_get(contexts, substitution->target_context);
				if (!prefix || substitution->first >= i || !target ||
					prefix->source_context != substitution->source_context ||
					target->parent != prefix->target_context ||
					substitution->term >= terms->term_count) {
					return -1;
				}
				break;
			}
			case PROTOTYPE_SUBSTITUTION_COMPOSE: {
				const struct prototype_substitution* outer =
					prototype_substitution_get(db, substitution->first);
				const struct prototype_substitution* inner =
					prototype_substitution_get(db, substitution->second);
				if (!outer || !inner || substitution->first >= i ||
					substitution->second >= i ||
					outer->source_context != inner->target_context ||
					substitution->source_context != inner->source_context ||
					substitution->target_context != outer->target_context) {
					return -1;
				}
				break;
			}
			default:
				return -1;
		}
	}
	return 0;
}

int prototype_substitution_db_validate_classifier_coherence(
	struct prototype_substitution_db* db,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations
) {
	if (!type_declarations || prototype_substitution_db_validate(
			db, contexts, terms
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < db->substitution_count; ++i) {
		const struct prototype_substitution* substitution = &db->substitutions[i];
		if (substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND) {
			continue;
		}
		const struct prototype_substitution* prefix =
			prototype_substitution_get(db, substitution->first);
		const struct prototype_context* target =
			prototype_context_get(contexts, substitution->target_context);
		const struct prototype_context_classifier_view context_view = {
			.contexts = contexts
		};
		uint32_t target_classifier;
		uint32_t expected_classifier;
		if (!prefix || !target ||
			prototype_context_classifier_view_read(
				&context_view, substitution->target_context, &target_classifier
			) != 0) {
			return -1;
		}
		int reindex_status = prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				db,
				target_classifier,
				substitution->first,
				&expected_classifier
			);
		int whnf_status = reindex_status == 0 ?
			prototype_judgement_classifier_value_whnf(
				terms,
				type_declarations,
				expected_classifier,
				&expected_classifier
			) : -1;
		if (reindex_status != 0 || whnf_status != 0) {
			fprintf(stderr,
				"substitution classifier projection failed substitution=%u source=%u target=%u term=%u expected=%u reindex=%d whnf=%d\n",
				i, substitution->source_context, substitution->target_context,
				substitution->term, expected_classifier, reindex_status, whnf_status);
			return -1;
		}
	}
	return 0;
}

int prototype_substitution_binding_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t target_binder,
	uint32_t* p_term
);

int prototype_term_reindex(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t term,
	uint32_t substitution_id,
	uint32_t* p_reindexed
) {
	int trace = getenv("A_PROGRAM_REINDEX_TRACE") != NULL;
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!terms || !type_declarations || !contexts || !substitution ||
		!p_reindexed || term >= terms->term_count) {
		if (trace) {
			fprintf(stderr,
				"term reindex failed stage=input term=%u substitution=%u count=%zu\n",
				term, substitution_id, terms ? terms->term_count : 0);
		}
		return -1;
	}
	substitutions->reindex_requests++;
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY) {
		*p_reindexed = term;
		return 0;
	}
	size_t cache_slot = graph_key_hash_mix(
		graph_key_hash_mix(UINT64_C(1469598103934665603), term),
		substitution_id
	) % PROTOTYPE_REINDEX_CACHE_COUNT;
	struct prototype_reindex_cache_entry* cached =
		&substitutions->reindex_cache[cache_slot];
	if (cached->present && cached->term == term &&
		cached->substitution == substitution_id &&
		cached->result < terms->term_count) {
		substitutions->reindex_hits++;
		*p_reindexed = cached->result;
		return 0;
	}
	const struct prototype_context* target = prototype_context_get(
		contexts, substitution->target_context
	);
	if (!target) {
		if (trace) {
			fprintf(stderr,
				"term reindex failed stage=target term=%u substitution=%u "
				"source=%u target=%u\n",
				term, substitution_id, substitution->source_context,
				substitution->target_context);
		}
		return -1;
	}
	size_t count = target->depth;
	struct prototype_binding_replacement* bindings =
		count ? malloc(count * sizeof(*bindings)) : NULL;
	if (count > 0 && !bindings) {
		return -1;
	}

	size_t index = 0;
	uint32_t context_id = substitution->target_context;
	while (context_id != prototype_context_empty(contexts)) {
		const struct prototype_context* context =
			prototype_context_get(contexts, context_id);
		if (!context || index >= count) {
			if (trace) {
				fprintf(stderr,
					"term reindex failed stage=context term=%u substitution=%u "
					"context=%u index=%zu depth=%zu\n",
					term, substitution_id, context_id, index, count);
			}
			free(bindings);
			return -1;
		}
		int already_present = 0;
		for (size_t existing = 0; existing < index; ++existing) {
			if (bindings[existing].binding_id == context->binding_id) {
				already_present = 1;
				break;
			}
		}
		if (already_present) {
			context_id = context->parent;
			continue;
		}
		bindings[index].binding_id = context->binding_id;
		if (prototype_substitution_binding_term(
				terms,
				type_declarations,
				contexts,
				substitutions,
				substitution_id,
				context->binding_id,
				&bindings[index].replacement
			) != 0) {
			if (trace) {
				fprintf(stderr,
					"term reindex failed stage=binding term=%u substitution=%u "
					"source=%u target=%u context=%u binder=%u kind=%d\n",
					term, substitution_id, substitution->source_context,
					substitution->target_context, context_id, context->binding_id,
					substitution->kind);
			}
			free(bindings);
			return -1;
		}
		index++;
		context_id = context->parent;
	}
	int status = prototype_term_graph_reindex_bindings(
		terms, term,
		bindings,
		index,
		p_reindexed
	);
	if (status != 0 && trace) {
		fprintf(stderr,
			"term reindex failed stage=graph term=%u substitution=%u "
			"source=%u target=%u bindings=%zu\n",
			term, substitution_id, substitution->source_context,
			substitution->target_context, index);
	}
	free(bindings);
	if (status == 0) {
		*cached = (struct prototype_reindex_cache_entry) {
			.present = 1,
			.term = term,
			.substitution = substitution_id,
			.result = *p_reindexed,
			.graph_revision = terms->normalization_graph_revision,
			.type_declaration_revision = type_declarations->semantic_schema.semantic_revision
		};
	}
	return status;
}

int prototype_substitution_binding_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t target_binder,
	uint32_t* p_term
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!substitution || !p_term) {
		return -1;
	}
	size_t cache_slot = graph_key_hash_mix(
		graph_key_hash_mix(
			UINT64_C(1469598103934665603), substitution_id
		),
		target_binder
	) % PROTOTYPE_SUBSTITUTION_BINDING_CACHE_COUNT;
	struct prototype_substitution_binding_cache_entry* cached =
		&substitutions->binding_cache[cache_slot];
	if (cached->present && cached->substitution == substitution_id &&
		cached->binding_id == target_binder && cached->term < terms->term_count) {
		*p_term = cached->term;
		return 0;
	}
	uint32_t result;
	int status;
	switch (substitution->kind) {
		case PROTOTYPE_SUBSTITUTION_IDENTITY:
		case PROTOTYPE_SUBSTITUTION_PROJECTION:
			status = prototype_term_var(terms, target_binder, &result);
			break;
		case PROTOTYPE_SUBSTITUTION_EMPTY:
			return -1;
		case PROTOTYPE_SUBSTITUTION_EXTEND: {
			const struct prototype_context* target =
				prototype_context_get(contexts, substitution->target_context);
			if (!target) {
				return -1;
			}
			if (target->binding_id == target_binder) {
				result = substitution->term;
				status = 0;
				break;
			}
			status = prototype_substitution_binding_term(
				terms,
				type_declarations,
				contexts,
				substitutions,
				substitution->first,
				target_binder,
				&result
			);
			break;
		}
		case PROTOTYPE_SUBSTITUTION_COMPOSE: {
			uint32_t middle_term;
			if (prototype_substitution_binding_term(
				terms,
				type_declarations,
				contexts,
				substitutions,
				substitution->first,
				target_binder,
				&middle_term
			) != 0) {
				return -1;
			}
			status = prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				middle_term,
				substitution->second,
				&result
			);
			break;
		}
		default:
			return -1;
	}
	if (status != 0) {
		return status;
	}
	*cached = (struct prototype_substitution_binding_cache_entry) {
		.present = 1,
		.substitution = substitution_id,
		.binding_id = target_binder,
		.term = result
	};
	*p_term = result;
	return 0;
}

int prototype_substitution_compare_pointwise(
	struct prototype_substitution_db* substitutions,
	const struct prototype_context_db* contexts,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t left_substitution,
	uint32_t right_substitution,
	int normalization_profile,
	uint64_t step_limit,
	struct prototype_term_conversion_result* p_result
) {
	const struct prototype_substitution* left = prototype_substitution_get(
		substitutions, left_substitution
	);
	const struct prototype_substitution* right = prototype_substitution_get(
		substitutions, right_substitution
	);
	if (!left || !right || !contexts || !terms || !type_declarations ||
		!p_result || left->source_context != right->source_context ||
		left->target_context != right->target_context) {
		return -1;
	}
	memset(p_result, 0, sizeof(*p_result));
	p_result->status = PROTOTYPE_TERM_CONVERSION_EQUAL;
	p_result->reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
	p_result->profile = normalization_profile;
	p_result->left = PROTOTYPE_INVALID_ID;
	p_result->right = PROTOTYPE_INVALID_ID;
	p_result->left_observation = PROTOTYPE_INVALID_ID;
	p_result->right_observation = PROTOTYPE_INVALID_ID;
	p_result->step_limit = step_limit;
	p_result->graph_revision = terms->normalization_graph_revision;
	uint64_t remaining = step_limit;
	uint32_t context_id = left->target_context;
	while (context_id != prototype_context_empty(contexts)) {
		const struct prototype_context* entry = prototype_context_get(
			contexts, context_id
		);
		uint32_t left_term;
		uint32_t right_term;
		if (!entry || prototype_substitution_binding_term(
				terms,
				type_declarations,
				contexts,
				substitutions,
				left_substitution,
				entry->binding_id,
				&left_term
			) != 0 || prototype_substitution_binding_term(
				terms,
				type_declarations,
				contexts,
				substitutions,
				right_substitution,
				entry->binding_id,
				&right_term
			) != 0) {
			return -1;
		}
		struct prototype_term_conversion_result comparison;
		if (prototype_term_compare_for_conversion(
				terms,
				NULL,
				normalization_profile,
				left_term,
				right_term,
				remaining,
				&comparison
			) != 0) {
			return -1;
		}
		if (comparison.steps_used > remaining) {
			return -1;
		}
		remaining -= comparison.steps_used;
		p_result->steps_used += comparison.steps_used;
		if (comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			*p_result = comparison;
			p_result->step_limit = step_limit;
			p_result->steps_used = step_limit - remaining;
			return 0;
		}
		context_id = entry->parent;
	}
	return 0;
}

int prototype_context_extension_path(
	const struct prototype_context_db* contexts,
	uint32_t ancestor,
	uint32_t descendant,
	uint32_t* path,
	uint32_t path_capacity,
	uint32_t* p_count
) {
	if (!contexts || !path || !p_count ||
		!prototype_context_get(contexts, ancestor) ||
		!prototype_context_get(contexts, descendant)) {
		return -1;
	}
	uint32_t reverse_path[128];
	uint32_t count = 0;
	uint32_t cursor = descendant;
	while (cursor != ancestor) {
		const struct prototype_context* context =
			prototype_context_get(contexts, cursor);
		if (!context || count >= 128 || count >= path_capacity ||
			cursor == prototype_context_empty(contexts)) {
			return -1;
		}
		reverse_path[count++] = cursor;
		cursor = context->parent;
	}
	for (uint32_t i = 0; i < count; ++i) {
		path[i] = reverse_path[count - i - 1];
	}
	*p_count = count;
	return 0;
}

int prototype_context_comprehension_action(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t source_extension,
	uint32_t base_substitution,
	uint32_t* p_target_binding_id,
	uint32_t* p_target_extension,
	uint32_t* p_lifted_substitution
) {
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_target_binding_id || !p_target_extension || !p_lifted_substitution) {
		return -1;
	}
	contexts->pullback_requests++;
	const struct prototype_context* source_entry = prototype_context_get(
		contexts, source_extension
	);
	const struct prototype_substitution* base = prototype_substitution_get(
		substitutions, base_substitution
	);
	const struct prototype_context_classifier_view context_view = {
		.contexts = contexts
	};
	uint32_t source_classifier;
	if (!source_entry || source_extension == prototype_context_empty(contexts) ||
		!base || base->target_context != source_entry->parent ||
		prototype_context_classifier_view_read(
			&context_view, source_extension, &source_classifier
		) != 0) {
		return -1;
	}
	uint64_t key_hash = comprehension_action_key_hash(
		source_extension, base_substitution
	);
	size_t bucket = graph_index_bucket(key_hash);
	for (uint32_t i = contexts->comprehension_action_index_heads[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = contexts->comprehension_actions[i].hash_next) {
		contexts->pullback_probes++;
		if (i >= contexts->comprehension_action_count) {
			return -1;
		}
		const struct prototype_context_comprehension_action* action =
			&contexts->comprehension_actions[i];
		if (action->key_hash == key_hash &&
			action->source_extension == source_extension &&
			action->base_substitution == base_substitution) {
			if (!prototype_context_get(contexts, action->target_extension) ||
				!prototype_substitution_get(
					substitutions, action->lifted_substitution
				)) {
				return -1;
			}
			contexts->pullback_hits++;
			*p_target_binding_id = action->target_binding_id;
			*p_target_extension = action->target_extension;
			*p_lifted_substitution = action->lifted_substitution;
			return 0;
		}
	}
	if (contexts->comprehension_action_count >=
		PROTOTYPE_CONTEXT_COMPREHENSION_ACTION_CAPACITY) {
		return -1;
	}
	uint32_t classifier;
	uint32_t producer_occurrence = source_entry->producer_occurrence;
	uint32_t candidate_binder;
	uint32_t target_extension;
	if (prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			source_classifier,
			base_substitution,
			&classifier
		) != 0 ||
		(candidate_binder = prototype_term_new_binding(terms)) ==
			PROTOTYPE_INVALID_ID ||
		(source_entry->extension_kind ==
			PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT ?
		 prototype_context_extend_sequence_result(
			contexts, base->source_context, candidate_binder, classifier,
			producer_occurrence, &target_extension
		 ) : prototype_context_extend(
			contexts, base->source_context, candidate_binder, classifier,
			&target_extension
		 )) != 0) {
		return -1;
	}
	const struct prototype_context* target_entry = prototype_context_get(
		contexts, target_extension
	);
	uint32_t projection;
	uint32_t weakened_substitution;
	uint32_t variable;
	uint32_t lifted_substitution;
	if (!target_entry ||
		prototype_substitution_projection(
			substitutions, contexts, target_extension, &projection
		) != 0 ||
		prototype_substitution_compose(
			substitutions,
			contexts,
			base_substitution,
			projection,
			&weakened_substitution
		) != 0 ||
		prototype_term_var(
			terms, target_entry->binding_id, &variable
		) != 0 ||
		prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			weakened_substitution,
			source_extension,
			variable,
			classifier,
			&lifted_substitution
		) != 0) {
		return -1;
	}
	uint32_t action_id = (uint32_t)contexts->comprehension_action_count++;
	contexts->comprehension_actions[action_id] =
		(struct prototype_context_comprehension_action) {
		.source_extension = source_extension,
		.base_substitution = base_substitution,
		.target_extension = target_extension,
		.lifted_substitution = lifted_substitution,
		.target_binding_id = target_entry->binding_id,
		.key_hash = key_hash,
		.hash_next = contexts->comprehension_action_index_heads[bucket]
	};
	contexts->comprehension_action_index_heads[bucket] = action_id;
	*p_target_binding_id = target_entry->binding_id;
	*p_target_extension = target_extension;
	*p_lifted_substitution = lifted_substitution;
	return 0;
}

int prototype_context_comprehension_actions_validate(
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions
) {
	if (!contexts || !substitutions ||
		contexts->comprehension_action_count >
			PROTOTYPE_CONTEXT_COMPREHENSION_ACTION_CAPACITY) {
		return -1;
	}
	for (uint32_t i = 0;
		i < contexts->comprehension_action_count;
		++i) {
		const struct prototype_context_comprehension_action* action =
			&contexts->comprehension_actions[i];
		const struct prototype_context* source = prototype_context_get(
			contexts, action->source_extension
		);
		const struct prototype_context* target = prototype_context_get(
			contexts, action->target_extension
		);
		const struct prototype_substitution* base = prototype_substitution_get(
			substitutions, action->base_substitution
		);
		const struct prototype_substitution* lifted = prototype_substitution_get(
			substitutions, action->lifted_substitution
		);
		uint64_t key_hash = comprehension_action_key_hash(
			action->source_extension, action->base_substitution
		);
		if (!source || action->source_extension ==
				prototype_context_empty(contexts) ||
			!target || !base || !lifted || action->key_hash != key_hash ||
			base->target_context != source->parent ||
			target->parent != base->source_context ||
			target->binding_id != action->target_binding_id ||
			lifted->source_context != action->target_extension ||
			lifted->target_context != action->source_extension) {
			return -1;
		}
		uint32_t found = PROTOTYPE_INVALID_ID;
		size_t bucket = graph_index_bucket(key_hash);
		for (uint32_t j = contexts->comprehension_action_index_heads[bucket];
			j != PROTOTYPE_INVALID_ID;
			j = contexts->comprehension_actions[j].hash_next) {
			if (j >= contexts->comprehension_action_count) {
				return -1;
			}
			const struct prototype_context_comprehension_action* indexed =
				&contexts->comprehension_actions[j];
			if (indexed->source_extension == action->source_extension &&
				indexed->base_substitution == action->base_substitution) {
				if (found != PROTOTYPE_INVALID_ID) {
					return -1;
				}
				found = j;
			}
		}
		if (found != i) {
			return -1;
		}
	}
	return 0;
}

static int prototype_context_pullback_telescope(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t source_base,
	uint32_t source_extension,
	uint32_t base_substitution,
	uint32_t* binders,
	uint32_t binder_capacity,
	uint32_t* p_binder_count,
	uint32_t* p_target_extension,
	uint32_t* p_lifted_substitution
) {
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!binders || !p_binder_count || !p_target_extension ||
		!p_lifted_substitution) {
		return -1;
	}
	const struct prototype_substitution* base = prototype_substitution_get(
		substitutions, base_substitution
	);
	if (!base || base->target_context != source_base) {
		return -1;
	}
	uint32_t source_path[128];
	uint32_t source_count;
	if (prototype_context_extension_path(
			contexts,
			source_base,
			source_extension,
			source_path,
			128,
			&source_count
		) != 0 || source_count > binder_capacity) {
		return -1;
	}
	uint32_t substitution = base_substitution;
	uint32_t target_context = base->source_context;
	for (uint32_t i = 0; i < source_count; ++i) {
		if (prototype_context_comprehension_action(
				contexts,
				substitutions,
				terms,
				type_declarations,
				source_path[i],
				substitution,
				&binders[i],
				&target_context,
				&substitution
			) != 0) {
			return -1;
		}
	}
	*p_binder_count = source_count;
	*p_target_extension = target_context;
	*p_lifted_substitution = substitution;
	return 0;
}

int prototype_context_pullback_occurrence_telescope(
	const struct prototype_context_classifier_view* context_view,
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t source_base,
	uint32_t source_extension,
	uint32_t base_substitution,
	uint32_t* p_target_extension,
	uint32_t* p_lifted_substitution
) {
	if (!context_view || context_view->contexts != contexts || !contexts ||
		!substitutions || !terms || !type_declarations ||
		!p_target_extension || !p_lifted_substitution) {
		return -1;
	}
	const struct prototype_substitution* base = prototype_substitution_get(
		substitutions, base_substitution
	);
	uint32_t source_path[128];
	uint32_t source_count;
	if (!base || base->target_context != source_base ||
		prototype_context_extension_path(
			contexts,
			source_base,
			source_extension,
			source_path,
			128,
			&source_count
		) != 0) {
		return -1;
	}
	uint32_t target_context = base->source_context;
	uint32_t substitution = base_substitution;
	for (uint32_t i = 0; i < source_count; ++i) {
		const struct prototype_context* source = prototype_context_get(
			contexts, source_path[i]
		);
		uint32_t source_classifier;
		uint32_t classifier = PROTOTYPE_INVALID_ID;
		uint32_t classifier_equation;
		uint32_t producer_occurrence = source ?
			source->producer_occurrence : PROTOTYPE_INVALID_ID;
		uint32_t target_extension;
		uint32_t projection;
		uint32_t weakened_substitution;
		uint32_t variable;
		if (!source || source->binding_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		int classifier_status = prototype_context_classifier_view_read(
			context_view, source_path[i], &source_classifier
		);
		if (classifier_status < 0) {
			return -1;
		}
		if (prototype_context_contains_binding(
				contexts, target_context, source->binding_id
			)) {
			return -1;
		}
		if (context_classifier_equation_intern_reindex(
				contexts,
				source->binding_id,
				target_context,
				source->classifier_equation,
				substitution,
				source->extension_kind,
				producer_occurrence,
				&classifier_equation
			) != 0) {
			return -1;
		}
		if (classifier_status == 0) {
			if (context_classifier_reindex_value(
					contexts,
					substitutions,
					terms,
					type_declarations,
					source_classifier,
					substitution,
					&classifier
				) != 0 || prototype_context_classifier_equation_publish(
					contexts, classifier_equation, classifier
				) != 0) {
				return -1;
			}
		}
		int extension_status = source->extension_kind ==
			PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT ?
			prototype_context_extend_sequence_result_equation(
				contexts,
				target_context,
				source->binding_id,
				classifier_equation,
				producer_occurrence,
				&target_extension
			) : prototype_context_extend_equation(
				contexts,
				target_context,
				source->binding_id,
				classifier_equation,
				&target_extension
			);
		if (extension_status != 0) {
			return -1;
		}
		if (prototype_substitution_projection(
				substitutions, contexts, target_extension, &projection
				) != 0) {
			return -1;
		}
		if (prototype_substitution_compose(
				substitutions,
				contexts,
				substitution,
				projection,
				&weakened_substitution
			) != 0) {
			return -1;
		}
		if (prototype_term_var(
				terms, source->binding_id, &variable
			) != 0) {
			return -1;
		}
		/* Pullback builds the raw Context action. Layer T validates its term
		 * against the classifier projected from the final Context equation. */
		if (prototype_substitution_extend_after_validation(
				substitutions,
				contexts,
				weakened_substitution,
				source_path[i],
				variable,
				&substitution
			) != 0) {
			return -1;
		}
		target_context = target_extension;
	}
	*p_target_extension = target_context;
	*p_lifted_substitution = substitution;
	return 0;
}

int prototype_context_reindex_telescope(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t base_context,
	uint32_t source_extension,
	uint32_t* binders,
	uint32_t binder_capacity,
	uint32_t* p_binder_count,
	uint32_t* p_target_extension,
	uint32_t* p_substitution
) {
	uint32_t identity;
	if (!substitutions || !contexts || prototype_substitution_identity(
			substitutions, contexts, base_context, &identity
		) != 0) {
		return -1;
	}
	return prototype_context_pullback_telescope(
		contexts,
		substitutions,
		terms,
		type_declarations,
		base_context,
		source_extension,
		identity,
		binders,
		binder_capacity,
		p_binder_count,
		p_target_extension,
		p_substitution
	);
}

int prototype_context_substitution_from_terms_in_view(
	const struct prototype_context_classifier_view* context_view,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t source_context,
	uint32_t target_context,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_substitution
) {
	if (!context_view || !context_view->contexts || !substitutions || !terms ||
		!type_declarations ||
		!p_substitution || (argument_count > 0 && !arguments)) {
		return -1;
	}
	const struct prototype_context_db* contexts = context_view->contexts;
	uint32_t substitution;
	if (prototype_substitution_empty(
			substitutions, contexts, source_context, &substitution
		) != 0) {
		return -1;
	}
	uint32_t path[128];
	uint32_t path_count;
	if (prototype_context_extension_path(
			contexts,
			prototype_context_empty(contexts),
			target_context,
			path,
			128,
			&path_count
		) != 0 || path_count != argument_count) {
		return -1;
	}
	for (uint32_t i = 0; i < path_count; ++i) {
		const struct prototype_context* entry =
			prototype_context_get(contexts, path[i]);
		uint32_t entry_classifier;
		uint32_t classifier;
		if (!entry) {
			return -1;
		}
		int classifier_status = prototype_context_classifier_view_read(
			context_view, path[i], &entry_classifier
		);
		if (classifier_status != 0) {
			if (getenv("A_PROGRAM_CONTEXT_TRACE")) {
				fprintf(
					stderr,
					"context substitution classifier failed source=%u target=%u "
					"entry=%u argument=%u status=%d\n",
					source_context,
					target_context,
					path[i],
					i,
					classifier_status
				);
			}
			return -1;
		}
		int reindex_status = prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			entry_classifier,
			substitution,
			&classifier
		);
		if (reindex_status != 0) {
			return -1;
		}
		int extension_status = prototype_substitution_extend_in_view(
			context_view,
			substitutions,
			terms,
			type_declarations,
			substitution,
			path[i],
			arguments[i],
			classifier,
			&substitution
		);
		if (extension_status != PROTOTYPE_SUBSTITUTION_EXTEND_OK) {
			if (getenv("A_PROGRAM_CONTEXT_TRACE")) {
				fprintf(
					stderr,
					"context substitution extension failed source=%u target=%u "
					"entry=%u argument=%u term=%u classifier=%u status=%d\n",
					source_context,
					target_context,
					path[i],
					i,
					arguments[i],
					classifier,
					extension_status
				);
			}
			return -1;
		}
	}
	*p_substitution = substitution;
	return 0;
}

int prototype_context_substitution_from_terms(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t source_context,
	uint32_t target_context,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_substitution
) {
	const struct prototype_context_classifier_view context_view = {
		.contexts = contexts
	};
	return prototype_context_substitution_from_terms_in_view(
		&context_view,
		substitutions,
		terms,
		type_declarations,
		source_context,
		target_context,
		arguments,
		argument_count,
		p_substitution
	);
}

int prototype_context_telescope_classifiers_in_view(
	const struct prototype_context_classifier_view* context_view,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t telescope_base,
	uint32_t telescope_end,
	const uint32_t* previous_terms,
	uint32_t previous_term_count,
	uint32_t entry_count,
	uint32_t* classifiers
) {
	const struct prototype_substitution* prefix =
		prototype_substitution_get(substitutions, prefix_substitution);
	if (!context_view || !context_view->contexts || !substitutions || !terms ||
		!type_declarations ||
		!prefix || (entry_count > 0 && !classifiers) ||
		(previous_term_count > 0 && !previous_terms) ||
		prefix->target_context != telescope_base ||
		(entry_count > 0 && previous_term_count < entry_count - 1)) {
		return -1;
	}
	const struct prototype_context_db* contexts = context_view->contexts;
	if (entry_count == 0) {
		return 0;
	}
	uint32_t path[128];
	uint32_t path_count;
	if (prototype_context_extension_path(
			contexts,
			telescope_base,
			telescope_end,
			path,
			128,
			&path_count
		) != 0 || entry_count > path_count) {
		return -1;
	}
	uint32_t substitution = prefix_substitution;
	for (uint32_t i = 0; i < entry_count; ++i) {
		const struct prototype_context* entry =
			prototype_context_get(contexts, path[i]);
		uint32_t entry_classifier;
		uint32_t classifier;
		uint32_t whnf;
		if (!entry || prototype_context_classifier_view_read(
				context_view, path[i], &entry_classifier
			) != 0 || prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				entry_classifier,
				substitution,
				&classifier
			) != 0 || prototype_term_normalize_complete_with_profile(
				terms,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
				classifier,
				&whnf
			) != 0 || whnf >= terms->term_count) {
			return -1;
		}
		if (terms->terms[whnf].tag == PROTOTYPE_TERM_RETURN) {
			whnf = terms->terms[whnf].as.return_term.value;
		}
		classifiers[i] = whnf;
		if (i + 1 < entry_count && prototype_substitution_extend_in_view(
				context_view,
				substitutions,
				terms,
				type_declarations,
				substitution,
				path[i],
				previous_terms[i],
				whnf,
				&substitution
			) != 0) {
			return -1;
		}
	}
	return 0;
}

int prototype_context_telescope_entry_classifier(
	struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t telescope_base,
	uint32_t telescope_end,
	const uint32_t* previous_terms,
	uint32_t previous_term_count,
	uint32_t entry_index,
	uint32_t* p_classifier
) {
	const struct prototype_context_classifier_view context_view = {
		.contexts = contexts
	};
	return prototype_context_telescope_entry_classifier_in_view(
		&context_view,
		substitutions,
		terms,
		type_declarations,
		prefix_substitution,
		telescope_base,
		telescope_end,
		previous_terms,
		previous_term_count,
		entry_index,
		p_classifier
	);
}

int prototype_context_telescope_entry_classifier_in_view(
	const struct prototype_context_classifier_view* context_view,
	struct prototype_substitution_db* substitutions,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t prefix_substitution,
	uint32_t telescope_base,
	uint32_t telescope_end,
	const uint32_t* previous_terms,
	uint32_t previous_term_count,
	uint32_t entry_index,
	uint32_t* p_classifier
) {
	uint32_t classifiers[128];
	if (entry_index >= 128 || !p_classifier ||
		prototype_context_telescope_classifiers_in_view(
			context_view,
			substitutions,
			terms,
			type_declarations,
			prefix_substitution,
			telescope_base,
			telescope_end,
			previous_terms,
			previous_term_count,
			entry_index + 1,
			classifiers
		) != 0) {
		return -1;
	}
	*p_classifier = classifiers[entry_index];
	return 0;
}
