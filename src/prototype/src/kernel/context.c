#include "a_program/kernel/context.h"

#include <stdlib.h>
#include <string.h>

#include "a_program/kernel/judgement/db.h"
#include "a_program/kernel/judgement/rules.h"
#include "a_program/kernel/judgement/conversion.h"
#include "a_program/kernel/judgement/classifier_solver.h"
#include "a_program/core/term.h"
#include "a_program/kernel/type_declaration.h"
#include "a_program/support/storage.h"

static uint64_t graph_key_hash_mix(uint64_t hash, uint32_t value) {
	hash ^= value;
	hash *= UINT64_C(1099511628211);
	return hash;
}

static uint64_t context_key_hash(
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	int extension_kind,
	uint32_t producer_computation
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = graph_key_hash_mix(hash, parent);
	hash = graph_key_hash_mix(hash, binding_id);
	hash = graph_key_hash_mix(hash, (uint32_t)extension_kind);
	hash = graph_key_hash_mix(hash, producer_computation);
	if (classifier != PROTOTYPE_INVALID_ID &&
		classifier_variable != PROTOTYPE_INVALID_ID) {
		hash = graph_key_hash_mix(hash, 3);
		hash = graph_key_hash_mix(hash, classifier);
		hash = graph_key_hash_mix(hash, classifier_variable);
	} else if (classifier != PROTOTYPE_INVALID_ID) {
		hash = graph_key_hash_mix(hash, 1);
		hash = graph_key_hash_mix(hash, classifier);
	} else {
		hash = graph_key_hash_mix(hash, 2);
		hash = graph_key_hash_mix(hash, classifier_variable);
	}
	return hash;
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
	hash = graph_key_hash_mix(hash, substitution->term_classifier);
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
	db->contexts = contexts;
	db->context_capacity = context_capacity;
	db->context_count = 1;
	db->contexts[0].parent = PROTOTYPE_INVALID_ID;
	db->contexts[0].binding_id = PROTOTYPE_INVALID_ID;
	db->contexts[0].classifier_ref.kind =
		PROTOTYPE_CONTEXT_CLASSIFIER_REF_INVALID;
	db->contexts[0].classifier_ref.term_id = PROTOTYPE_INVALID_ID;
	db->contexts[0].classifier_ref.variable_id = PROTOTYPE_INVALID_ID;
	db->contexts[0].extension_kind = PROTOTYPE_CONTEXT_EXTENSION_INVALID;
	db->contexts[0].producer_computation = PROTOTYPE_INVALID_ID;
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
			context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM ?
				context->classifier_ref.term_id :
				(context->classifier_ref.kind ==
					PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL ?
					context->classifier_ref.term_id : PROTOTYPE_INVALID_ID),
			context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_VARIABLE ?
				context->classifier_ref.variable_id :
				(context->classifier_ref.kind ==
					PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL ?
					context->classifier_ref.variable_id : PROTOTYPE_INVALID_ID),
			context->extension_kind,
			context->producer_computation
		);
		size_t bucket = context->key_hash %
			PROTOTYPE_CONTEXT_GRAPH_INDEX_BUCKET_COUNT;
		context->hash_next = db->index_heads[bucket];
		db->index_heads[bucket] = i;
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

uint32_t prototype_context_classifier_term(
	const struct prototype_context* context
) {
	return context && context->classifier_ref.kind ==
			PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM ?
		context->classifier_ref.term_id :
		(context && context->classifier_ref.kind ==
			PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL ?
			context->classifier_ref.term_id : PROTOTYPE_INVALID_ID);
}

uint32_t prototype_context_classifier_variable(
	const struct prototype_context* context
) {
	return context && context->classifier_ref.kind ==
			PROTOTYPE_CONTEXT_CLASSIFIER_REF_VARIABLE ?
		context->classifier_ref.variable_id :
		(context && context->classifier_ref.kind ==
			PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL ?
			context->classifier_ref.variable_id : PROTOTYPE_INVALID_ID);
}

static int prototype_context_extend_internal(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	int extension_kind,
	uint32_t producer_computation,
	int preserve_occurrence,
	uint32_t* p_context
) {
	if (!db || !p_context || parent >= db->context_count ||
		binding_id == PROTOTYPE_INVALID_ID ||
		(classifier == PROTOTYPE_INVALID_ID &&
			classifier_variable == PROTOTYPE_INVALID_ID) ||
		(extension_kind != PROTOTYPE_CONTEXT_EXTENSION_VALUE &&
		 extension_kind != PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT) ||
		(extension_kind == PROTOTYPE_CONTEXT_EXTENSION_VALUE &&
		 producer_computation != PROTOTYPE_INVALID_ID) ||
		(extension_kind == PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT &&
		 producer_computation == PROTOTYPE_INVALID_ID)) {
		return -1;
	}
	db->intern_requests++;
	uint64_t key_hash = context_key_hash(
		parent, binding_id, classifier, classifier_variable,
		extension_kind, producer_computation
	);
	size_t bucket = graph_index_bucket(key_hash);
	if (!preserve_occurrence) {
		for (uint32_t i = db->index_heads[bucket];
			i != PROTOTYPE_INVALID_ID;
			i = db->contexts[i].hash_next) {
			db->intern_probes++;
			if (i == 0 || i >= db->context_count) {
				return -1;
			}
			const struct prototype_context* context = &db->contexts[i];
			int same_extension =
				prototype_context_classifier_term(context) == classifier &&
				prototype_context_classifier_variable(context) == classifier_variable &&
				context->extension_kind == extension_kind &&
				context->producer_computation == producer_computation;
			/* Binding objects are graph identity. Equal classifiers do not make two
			 * independently allocated context extensions interchangeable. */
			if (context->parent == parent && context->binding_id == binding_id &&
				context->key_hash == key_hash && same_extension) {
				db->intern_hits++;
				*p_context = i;
				return 0;
			}
		}
	}
	if (db->context_count >= db->context_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)db->context_count++;
	db->contexts[id].parent = parent;
	db->contexts[id].binding_id = binding_id;
	db->contexts[id].classifier_ref.kind =
		classifier != PROTOTYPE_INVALID_ID &&
		classifier_variable != PROTOTYPE_INVALID_ID ?
			PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL :
			(classifier != PROTOTYPE_INVALID_ID ?
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM :
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_VARIABLE);
	db->contexts[id].classifier_ref.term_id = classifier;
	db->contexts[id].classifier_ref.variable_id = classifier_variable;
	db->contexts[id].extension_kind = extension_kind;
	db->contexts[id].producer_computation = producer_computation;
	db->contexts[id].depth = db->contexts[parent].depth + 1;
	db->contexts[id].key_hash = key_hash;
	db->contexts[id].hash_next = db->index_heads[bucket];
	db->index_heads[bucket] = id;
	*p_context = id;
	return 0;
}

int prototype_context_extend(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	uint32_t* p_context
) {
	return prototype_context_extend_internal(
		db, parent, binding_id, classifier, classifier_variable,
		PROTOTYPE_CONTEXT_EXTENSION_VALUE, PROTOTYPE_INVALID_ID, 0, p_context
	);
}

int prototype_context_extend_occurrence(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	uint32_t* p_context
) {
	return prototype_context_extend_internal(
		db, parent, binding_id, classifier, classifier_variable,
		PROTOTYPE_CONTEXT_EXTENSION_VALUE, PROTOTYPE_INVALID_ID, 1, p_context
	);
}

int prototype_context_extend_sequence_result(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	uint32_t producer_computation,
	uint32_t* p_context
) {
	return prototype_context_extend_internal(
		db, parent, binding_id, classifier, classifier_variable,
		PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT,
		producer_computation, 0, p_context
	);
}

int prototype_context_extend_sequence_result_occurrence(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	uint32_t producer_computation,
	uint32_t* p_context
) {
	return prototype_context_extend_internal(
		db, parent, binding_id, classifier, classifier_variable,
		PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT,
		producer_computation, 1, p_context
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
		db->context_count > db->context_capacity) {
		return -1;
	}
	const struct prototype_context* empty = &db->contexts[0];
	if (empty->parent != PROTOTYPE_INVALID_ID ||
		empty->binding_id != PROTOTYPE_INVALID_ID ||
		empty->classifier_ref.kind !=
			PROTOTYPE_CONTEXT_CLASSIFIER_REF_INVALID ||
		empty->classifier_ref.term_id != PROTOTYPE_INVALID_ID ||
		empty->classifier_ref.variable_id != PROTOTYPE_INVALID_ID ||
		empty->extension_kind != PROTOTYPE_CONTEXT_EXTENSION_INVALID ||
		empty->producer_computation != PROTOTYPE_INVALID_ID ||
		empty->depth != 0) {
		return -1;
	}
	for (uint32_t i = 1; i < db->context_count; ++i) {
		const struct prototype_context* context = &db->contexts[i];
		uint32_t classifier = prototype_context_classifier_term(context);
		uint32_t classifier_variable =
			prototype_context_classifier_variable(context);
		int classifier_ref_valid =
			(context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM &&
			 classifier != PROTOTYPE_INVALID_ID &&
			 classifier_variable == PROTOTYPE_INVALID_ID) ||
			(context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_VARIABLE &&
			 classifier == PROTOTYPE_INVALID_ID &&
			 classifier_variable != PROTOTYPE_INVALID_ID) ||
			(context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL &&
			 classifier != PROTOTYPE_INVALID_ID &&
			 classifier_variable != PROTOTYPE_INVALID_ID);
		uint64_t expected_key_hash = context_key_hash(
			context->parent,
			context->binding_id,
			classifier,
			classifier_variable,
			context->extension_kind,
			context->producer_computation
		);
		if (context->parent >= i ||
			context->binding_id == PROTOTYPE_INVALID_ID ||
			context->key_hash != expected_key_hash ||
			context->depth != db->contexts[context->parent].depth + 1 ||
			!classifier_ref_valid ||
			((context->extension_kind == PROTOTYPE_CONTEXT_EXTENSION_VALUE) !=
			 (context->producer_computation == PROTOTYPE_INVALID_ID)) ||
			(context->extension_kind != PROTOTYPE_CONTEXT_EXTENSION_VALUE &&
			 context->extension_kind !=
				PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT) ||
			(classifier != PROTOTYPE_INVALID_ID &&
				(classifier >= terms->term_count ||
				 terms->terms[classifier].tag == 0)) ||
			(context->producer_computation != PROTOTYPE_INVALID_ID &&
			 (context->producer_computation >= terms->term_count ||
			  terms->terms[context->producer_computation].tag == 0))) {
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
		uint32_t source_classifier = context ?
			prototype_context_classifier_term(context) : PROTOTYPE_INVALID_ID;
		uint32_t classifier = context &&
			context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM &&
			source_classifier < term_relocation_count
			? term_relocation[source_classifier]
			: (context && context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_PROVISIONAL &&
				source_classifier < term_relocation_count ?
				term_relocation[source_classifier] : PROTOTYPE_INVALID_ID);
		/* Linking needs only concrete classifier provenance once resolved. */
		uint32_t classifier_variable = context &&
			context->classifier_ref.kind ==
				PROTOTYPE_CONTEXT_CLASSIFIER_REF_VARIABLE &&
			context->classifier_ref.variable_id < binding_relocation_count
			? binding_relocation[context->classifier_ref.variable_id]
			: PROTOTYPE_INVALID_ID;
		uint32_t producer_computation = context &&
			context->producer_computation != PROTOTYPE_INVALID_ID &&
			context->producer_computation < term_relocation_count ?
			term_relocation[context->producer_computation] : PROTOTYPE_INVALID_ID;
		if (!context || context->parent >= i ||
			(classifier == PROTOTYPE_INVALID_ID) ==
				(classifier_variable == PROTOTYPE_INVALID_ID) ||
			context->binding_id == PROTOTYPE_INVALID_ID ||
			context->binding_id >= binding_relocation_count ||
			binding_relocation[context->binding_id] == PROTOTYPE_INVALID_ID ||
			(context->extension_kind ==
				PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT ?
			 prototype_context_extend_sequence_result_occurrence(
				target, relocation[context->parent],
				binding_relocation[context->binding_id], classifier,
				classifier_variable, producer_computation, &relocation[i]
			 ) : prototype_context_extend_occurrence(
				target, relocation[context->parent],
				binding_relocation[context->binding_id], classifier,
				classifier_variable, &relocation[i]
			 )) != 0) {
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
		if (substitution.term_classifier != PROTOTYPE_INVALID_ID) {
			if (substitution.term_classifier >= term_relocation_count ||
				term_relocation[substitution.term_classifier] ==
					PROTOTYPE_INVALID_ID) {
				return -1;
			}
			substitution.term_classifier =
				term_relocation[substitution.term_classifier];
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
			existing->term == substitution.term &&
			existing->term_classifier == substitution.term_classifier) {
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
	uint32_t term_classifier,
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
	rebased.term_classifier = term_classifier;
	rebased.key_hash = 0;
	rebased.hash_next = PROTOTYPE_INVALID_ID;
	switch (rebased.kind) {
		case PROTOTYPE_SUBSTITUTION_IDENTITY:
			if (source_context != target_context ||
				first != PROTOTYPE_INVALID_ID ||
				second != PROTOTYPE_INVALID_ID ||
				rebased.term != PROTOTYPE_INVALID_ID ||
				term_classifier != PROTOTYPE_INVALID_ID) {
				return -1;
			}
			break;
		case PROTOTYPE_SUBSTITUTION_EMPTY:
		case PROTOTYPE_SUBSTITUTION_PROJECTION:
			if (first != PROTOTYPE_INVALID_ID ||
				second != PROTOTYPE_INVALID_ID ||
				rebased.term != PROTOTYPE_INVALID_ID ||
				term_classifier != PROTOTYPE_INVALID_ID) {
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
				rebased.term != PROTOTYPE_INVALID_ID ||
				term_classifier != PROTOTYPE_INVALID_ID) {
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
		.term = PROTOTYPE_INVALID_ID,
		.term_classifier = PROTOTYPE_INVALID_ID
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
		.term = PROTOTYPE_INVALID_ID,
		.term_classifier = PROTOTYPE_INVALID_ID
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
		.term = PROTOTYPE_INVALID_ID,
		.term_classifier = PROTOTYPE_INVALID_ID
	};
	return prototype_substitution_add(db, substitution, p_substitution);
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
	if (!db || !contexts || !terms || !type_declarations || !p_substitution) {
		return PROTOTYPE_SUBSTITUTION_EXTEND_INVALID_ARGUMENT;
	}
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
	if (!target || target_context == prototype_context_empty(contexts) ||
		prototype_context_classifier_term(target) == PROTOTYPE_INVALID_ID) {
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
			prototype_context_classifier_term(target),
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
		return PROTOTYPE_SUBSTITUTION_EXTEND_CLASSIFIER_MISMATCH;
	}
	struct prototype_substitution substitution = {
		.kind = PROTOTYPE_SUBSTITUTION_EXTEND,
		.source_context = prefix->source_context,
		.target_context = target_context,
		.first = prefix_substitution,
		.second = PROTOTYPE_INVALID_ID,
		.term = term,
		.term_classifier = term_classifier
	};
	return prototype_substitution_add(db, substitution, p_substitution) == 0 ?
		PROTOTYPE_SUBSTITUTION_EXTEND_OK :
		PROTOTYPE_SUBSTITUTION_EXTEND_STORAGE_FAILED;
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
		.term = PROTOTYPE_INVALID_ID,
		.term_classifier = PROTOTYPE_INVALID_ID
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
					prototype_context_classifier_term(target) ==
						PROTOTYPE_INVALID_ID ||
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
		if (substitution->term_classifier >= terms->term_count) {
			return -1;
		}
		const struct prototype_substitution* prefix =
			prototype_substitution_get(db, substitution->first);
		const struct prototype_context* target =
			prototype_context_get(contexts, substitution->target_context);
		uint32_t expected_classifier;
		if (!prefix || !target ||
			prototype_context_classifier_term(target) == PROTOTYPE_INVALID_ID ||
			substitution->term_classifier >= terms->term_count) {
			return -1;
		}
		if (prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				db,
				prototype_context_classifier_term(target),
				substitution->first,
				&expected_classifier
			) != 0 || prototype_judgement_classifier_value_whnf(
				terms,
				type_declarations,
				expected_classifier,
				&expected_classifier
			) != 0 || (!prototype_judgement_classifier_compatible(
					terms,
					type_declarations,
					expected_classifier,
					substitution->term_classifier
				) && !prototype_judgement_classifier_reference_equal(
					terms,
					type_declarations,
					expected_classifier,
					substitution->term_classifier
				))) {
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
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!terms || !type_declarations || !contexts || !substitution ||
		!p_reindexed || term >= terms->term_count) {
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
		cached->graph_revision == terms->normalization_graph_revision &&
		cached->type_declaration_revision ==
			type_declarations->semantic_schema.semantic_revision &&
		cached->result < terms->term_count) {
		substitutions->reindex_hits++;
		*p_reindexed = cached->result;
		return 0;
	}
	const struct prototype_context* target = prototype_context_get(
		contexts, substitution->target_context
	);
	if (!target) {
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
			free(bindings);
			return -1;
		}
		index++;
		context_id = context->parent;
	}
	int status = prototype_term_graph_reindex_bindings(
		terms,
		type_declarations,
		term,
		bindings,
		index,
		p_reindexed
	);
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
				type_declarations,
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
	if (!source_entry || source_extension == prototype_context_empty(contexts) ||
		!base || base->target_context != source_entry->parent ||
		prototype_context_classifier_term(source_entry) == PROTOTYPE_INVALID_ID) {
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
	uint32_t producer_computation = PROTOTYPE_INVALID_ID;
	uint32_t candidate_binder;
	uint32_t target_extension;
	if (prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			prototype_context_classifier_term(source_entry),
			base_substitution,
			&classifier
		) != 0 ||
		(source_entry->producer_computation != PROTOTYPE_INVALID_ID &&
		 prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			source_entry->producer_computation,
			base_substitution,
			&producer_computation
		 ) != 0) ||
		(candidate_binder = prototype_term_new_binding(terms)) ==
			PROTOTYPE_INVALID_ID ||
		(source_entry->extension_kind ==
			PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT ?
		 prototype_context_extend_sequence_result(
			contexts, base->source_context, candidate_binder, classifier,
			PROTOTYPE_INVALID_ID, producer_computation, &target_extension
		 ) : prototype_context_extend(
			contexts, base->source_context, candidate_binder, classifier,
			PROTOTYPE_INVALID_ID, &target_extension
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

int prototype_context_pullback_telescope(
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
	if (!contexts || !substitutions || !terms || !type_declarations ||
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
		uint32_t source_classifier = prototype_context_classifier_term(source);
		uint32_t classifier_variable =
			prototype_context_classifier_variable(source);
		uint32_t classifier = PROTOTYPE_INVALID_ID;
		uint32_t producer_computation = PROTOTYPE_INVALID_ID;
		uint32_t target_extension;
		uint32_t projection;
		uint32_t weakened_substitution;
		uint32_t variable;
		if (!source || source->binding_id == PROTOTYPE_INVALID_ID ||
			(source_classifier == PROTOTYPE_INVALID_ID &&
			 classifier_variable == PROTOTYPE_INVALID_ID) ||
			prototype_context_contains_binding(
				contexts, target_context, source->binding_id
			) || (source_classifier != PROTOTYPE_INVALID_ID &&
			 prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				source_classifier,
				substitution,
				&classifier
			 ) != 0) || (source->producer_computation != PROTOTYPE_INVALID_ID &&
			 prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				source->producer_computation,
				substitution,
				&producer_computation
			 ) != 0) ||
			(source->extension_kind ==
				PROTOTYPE_CONTEXT_EXTENSION_SEQUENCE_RESULT ?
			 prototype_context_extend_sequence_result(
				contexts, target_context, source->binding_id, classifier,
				classifier == PROTOTYPE_INVALID_ID ? classifier_variable :
					PROTOTYPE_INVALID_ID,
				producer_computation, &target_extension
			 ) : prototype_context_extend(
				contexts, target_context, source->binding_id, classifier,
				classifier == PROTOTYPE_INVALID_ID ? classifier_variable :
					PROTOTYPE_INVALID_ID,
				&target_extension
			 )) != 0 || prototype_substitution_projection(
				substitutions, contexts, target_extension, &projection
			) != 0 || prototype_substitution_compose(
				substitutions,
				contexts,
				substitution,
				projection,
				&weakened_substitution
			) != 0 || prototype_term_var(
				terms, source->binding_id, &variable
			) != 0) {
			return -1;
		}
		if (classifier != PROTOTYPE_INVALID_ID) {
			if (prototype_substitution_extend(
					substitutions,
					contexts,
					terms,
					type_declarations,
					weakened_substitution,
					source_path[i],
					variable,
					classifier,
					&substitution
				) != 0) {
				return -1;
			}
		} else {
			/* This is a compiler-local structural action over an unresolved
			 * Context classifier.  The binding object and classifier variable are
			 * stable, so reindexing terms is already determined; classifier
			 * coherence is discharged when context resolution replaces the
			 * variable and fills term_classifier before publication. */
			const struct prototype_substitution* prefix =
				prototype_substitution_get(substitutions, weakened_substitution);
			const struct prototype_context* target = prototype_context_get(
				contexts, source_path[i]
			);
			if (!prefix || !target || target->parent != prefix->target_context ||
				prefix->source_context != target_extension ||
				target->binding_id != source->binding_id) {
				return -1;
			}
			struct prototype_substitution pending = {
				.kind = PROTOTYPE_SUBSTITUTION_EXTEND,
				.source_context = prefix->source_context,
				.target_context = source_path[i],
				.first = weakened_substitution,
				.second = PROTOTYPE_INVALID_ID,
				.term = variable,
				.term_classifier = PROTOTYPE_INVALID_ID
			};
			if (prototype_substitution_add(
					substitutions, pending, &substitution
				) != 0) {
				return -1;
			}
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
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!p_substitution || (argument_count > 0 && !arguments)) {
		return -1;
	}
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
		uint32_t classifier;
		if (!entry || prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				prototype_context_classifier_term(entry),
				substitution,
				&classifier
			) != 0 || prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				type_declarations,
				substitution,
				path[i],
				arguments[i],
				classifier,
				&substitution
			) != 0) {
			return -1;
		}
	}
	*p_substitution = substitution;
	return 0;
}

int prototype_context_telescope_classifiers(
	struct prototype_context_db* contexts,
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
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!prefix || (entry_count > 0 && !classifiers) ||
		(previous_term_count > 0 && !previous_terms) ||
		prefix->target_context != telescope_base ||
		(entry_count > 0 && previous_term_count < entry_count - 1)) {
		return -1;
	}
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
		uint32_t classifier;
		uint32_t whnf;
		if (!entry || prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				prototype_context_classifier_term(entry),
				substitution,
				&classifier
			) != 0 || prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				classifier,
				&whnf
			) != 0 || whnf >= terms->term_count) {
			return -1;
		}
		if (terms->terms[whnf].tag == PROTOTYPE_TERM_RETURN) {
			whnf = terms->terms[whnf].as.return_term.value;
		}
		classifiers[i] = whnf;
		if (i + 1 < entry_count && prototype_substitution_extend(
				substitutions,
				contexts,
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
	uint32_t classifiers[128];
	if (entry_index >= 128 || !p_classifier ||
		prototype_context_telescope_classifiers(
			contexts,
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
