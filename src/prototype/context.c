#include "context.h"

#include <stdlib.h>
#include <string.h>

#include "judgement.h"
#include "term.h"
#include "type_declaration.h"

void prototype_context_db_init(
	struct prototype_context_db* db,
	struct prototype_context* contexts,
	size_t context_capacity
) {
	if (!db || !contexts || context_capacity == 0) {
		return;
	}
	memset(db, 0, sizeof(*db));
	db->contexts = contexts;
	db->context_capacity = context_capacity;
	db->context_count = 1;
	db->contexts[0].parent = PROTOTYPE_INVALID_ID;
	db->contexts[0].binding_id = PROTOTYPE_INVALID_ID;
	db->contexts[0].classifier = PROTOTYPE_INVALID_ID;
	db->contexts[0].classifier_variable = PROTOTYPE_INVALID_ID;
	db->contexts[0].depth = 0;
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

int prototype_context_extend(
	struct prototype_context_db* db,
	uint32_t parent,
	uint32_t binding_id,
	uint32_t classifier,
	uint32_t classifier_variable,
	uint32_t* p_context
) {
	if (!db || !p_context || parent >= db->context_count ||
		binding_id == PROTOTYPE_INVALID_ID ||
		(classifier == PROTOTYPE_INVALID_ID &&
			classifier_variable == PROTOTYPE_INVALID_ID)) {
		return -1;
	}
	for (uint32_t i = 1; i < db->context_count; ++i) {
		const struct prototype_context* context = &db->contexts[i];
		int same_extension = classifier != PROTOTYPE_INVALID_ID ?
			context->classifier == classifier :
			context->classifier == PROTOTYPE_INVALID_ID &&
				context->classifier_variable == classifier_variable;
		if (context->parent == parent && same_extension) {
			*p_context = i;
			return 0;
		}
	}
	if (db->context_count >= db->context_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)db->context_count++;
	db->contexts[id].parent = parent;
	db->contexts[id].binding_id = binding_id;
	db->contexts[id].classifier = classifier;
	db->contexts[id].classifier_variable = classifier_variable;
	db->contexts[id].depth = db->contexts[parent].depth + 1;
	*p_context = id;
	return 0;
}

int prototype_context_contains_binding(
	const struct prototype_context_db* db,
	uint32_t context_id,
	uint32_t binding_id
) {
	if (!db || context_id >= db->context_count ||
		binding_id == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	while (context_id != 0) {
		const struct prototype_context* context = &db->contexts[context_id];
		if (context->binding_id == binding_id) {
			return 1;
		}
		context_id = context->parent;
	}
	return 0;
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
		empty->classifier != PROTOTYPE_INVALID_ID ||
		empty->classifier_variable != PROTOTYPE_INVALID_ID ||
		empty->depth != 0) {
		return -1;
	}
	for (uint32_t i = 1; i < db->context_count; ++i) {
		const struct prototype_context* context = &db->contexts[i];
		if (context->parent >= i ||
			context->binding_id == PROTOTYPE_INVALID_ID ||
			context->depth != db->contexts[context->parent].depth + 1 ||
			(context->classifier == PROTOTYPE_INVALID_ID &&
				context->classifier_variable == PROTOTYPE_INVALID_ID) ||
			(context->classifier != PROTOTYPE_INVALID_ID &&
				(context->classifier >= terms->term_count ||
				 terms->terms[context->classifier].tag == 0))) {
			return -1;
		}
	}
	return 0;
}

int prototype_context_db_append_relocated(
	struct prototype_context_db* target,
	const struct prototype_context_db* source,
	uint32_t term_offset,
	uint32_t binder_offset,
	uint32_t* relocation,
	size_t relocation_capacity
) {
	if (!target || !source || !relocation ||
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
		uint32_t classifier = context &&
			context->classifier != PROTOTYPE_INVALID_ID
			? context->classifier + term_offset
			: PROTOTYPE_INVALID_ID;
		/* Linking needs only concrete classifier provenance once resolved. */
		uint32_t classifier_variable = context &&
			context->classifier == PROTOTYPE_INVALID_ID &&
			context->classifier_variable != PROTOTYPE_INVALID_ID
			? context->classifier_variable + binder_offset
			: PROTOTYPE_INVALID_ID;
		if (!context || context->parent >= i ||
			(classifier == PROTOTYPE_INVALID_ID) ==
				(classifier_variable == PROTOTYPE_INVALID_ID) ||
			context->binding_id == PROTOTYPE_INVALID_ID ||
			prototype_context_extend(
				target,
				relocation[context->parent],
				context->binding_id + binder_offset,
				classifier,
				classifier_variable,
				&relocation[i]
			) != 0) {
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
	uint32_t term_offset,
	uint32_t proof_offset
) {
	if (!target || !source || !context_relocation) {
		return -1;
	}
	uint32_t relocation[PROTOTYPE_SUBSTITUTION_CAPACITY];
	if (source->substitution_count > PROTOTYPE_SUBSTITUTION_CAPACITY) {
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
			substitution.term += term_offset;
		}
		if (substitution.term_classifier != PROTOTYPE_INVALID_ID) {
			substitution.term_classifier += term_offset;
		}
		if (substitution.term_proof_id != PROTOTYPE_INVALID_ID) {
			substitution.term_proof_id += proof_offset;
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
	db->substitutions = substitutions;
	db->substitution_count = 0;
	db->substitution_capacity = substitution_capacity;
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
	for (uint32_t i = 0; i < db->substitution_count; ++i) {
		const struct prototype_substitution* existing = &db->substitutions[i];
		if (existing->kind == substitution.kind &&
			existing->source_context == substitution.source_context &&
			existing->target_context == substitution.target_context &&
			existing->first == substitution.first &&
			existing->second == substitution.second &&
			existing->term == substitution.term &&
			existing->term_classifier == substitution.term_classifier &&
			existing->term_proof_id == substitution.term_proof_id) {
			*p_substitution = i;
			return 0;
		}
	}
	if (db->substitution_count >= db->substitution_capacity) {
		return -1;
	}
	uint32_t id = (uint32_t)db->substitution_count++;
	db->substitutions[id] = substitution;
	*p_substitution = id;
	return 0;
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
		.term_classifier = PROTOTYPE_INVALID_ID,
		.term_proof_id = PROTOTYPE_INVALID_ID
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
		.term_classifier = PROTOTYPE_INVALID_ID,
		.term_proof_id = PROTOTYPE_INVALID_ID
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
		.term_classifier = PROTOTYPE_INVALID_ID,
		.term_proof_id = PROTOTYPE_INVALID_ID
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
	uint32_t term_proof_id,
	uint32_t* p_substitution
) {
	const struct prototype_substitution* prefix =
		prototype_substitution_get(db, prefix_substitution);
	if (!prefix || !terms || !type_declarations ||
		term == PROTOTYPE_INVALID_ID ||
		term_classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	const struct prototype_context* target =
		prototype_context_get(contexts, target_context);
	if (!target || target_context == prototype_context_empty(contexts) ||
		target->parent != prefix->target_context ||
		target->classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t expected_classifier;
	if (prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			db,
			target->classifier,
			prefix_substitution,
			&expected_classifier
		) != 0 || prototype_judgement_classifier_value_whnf(
			terms, type_declarations, expected_classifier, &expected_classifier
		) != 0 ||
		!prototype_judgement_classifier_reference_equal(
			terms,
			type_declarations,
			expected_classifier,
			term_classifier
		)) {
		return -1;
	}
	struct prototype_substitution substitution = {
		.kind = PROTOTYPE_SUBSTITUTION_EXTEND,
		.source_context = prefix->source_context,
		.target_context = target_context,
		.first = prefix_substitution,
		.second = PROTOTYPE_INVALID_ID,
		.term = term,
		.term_classifier = term_classifier,
		.term_proof_id = term_proof_id
	};
	return prototype_substitution_add(db, substitution, p_substitution);
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
		.term_classifier = PROTOTYPE_INVALID_ID,
		.term_proof_id = PROTOTYPE_INVALID_ID
	};
	return prototype_substitution_add(db, substitution, p_substitution);
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
			!prototype_context_get(contexts, substitution->target_context)) {
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
					target->classifier == PROTOTYPE_INVALID_ID ||
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

static int prototype_substitution_lookup_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t target_binder,
	uint32_t* p_term
);

int prototype_term_reindex(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
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
	if (substitution->kind == PROTOTYPE_SUBSTITUTION_IDENTITY) {
		*p_reindexed = term;
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
		if (prototype_substitution_lookup_term(
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
	return status;
}

static int prototype_substitution_lookup_term(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	const struct prototype_substitution_db* substitutions,
	uint32_t substitution_id,
	uint32_t target_binder,
	uint32_t* p_term
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!substitution || !p_term) {
		return -1;
	}
	switch (substitution->kind) {
		case PROTOTYPE_SUBSTITUTION_IDENTITY:
		case PROTOTYPE_SUBSTITUTION_PROJECTION:
			return prototype_term_var(terms, target_binder, p_term);
		case PROTOTYPE_SUBSTITUTION_EMPTY:
			return -1;
		case PROTOTYPE_SUBSTITUTION_EXTEND: {
			const struct prototype_context* target =
				prototype_context_get(contexts, substitution->target_context);
			if (!target) {
				return -1;
			}
			if (target->binding_id == target_binder) {
				*p_term = substitution->term;
				return 0;
			}
			return prototype_substitution_lookup_term(
				terms,
				type_declarations,
				contexts,
				substitutions,
				substitution->first,
				target_binder,
				p_term
			);
		}
		case PROTOTYPE_SUBSTITUTION_COMPOSE: {
			uint32_t middle_term;
			if (prototype_substitution_lookup_term(
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
			return prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				middle_term,
				substitution->second,
				p_term
			);
		}
		default:
			return -1;
	}
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

int prototype_context_fresh_reindex_extension(
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
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!binders || !p_binder_count || !p_target_extension || !p_substitution) {
		return -1;
	}
	uint32_t source_path[128];
	uint32_t source_count;
	if (prototype_context_extension_path(
			contexts,
			base_context,
			source_extension,
			source_path,
			128,
			&source_count
		) != 0 || source_count > binder_capacity) {
		return -1;
	}
	uint32_t substitution;
	if (prototype_substitution_identity(
			substitutions, contexts, base_context, &substitution
		) != 0) {
		return -1;
	}
	uint32_t target_context = base_context;
	for (uint32_t i = 0; i < source_count; ++i) {
		const struct prototype_context* source_entry =
			prototype_context_get(contexts, source_path[i]);
		uint32_t classifier;
		uint32_t candidate_binder;
		uint32_t extended_context;
		if (!source_entry || source_entry->classifier == PROTOTYPE_INVALID_ID ||
			prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				source_entry->classifier,
				substitution,
				&classifier
			) != 0 ||
			(candidate_binder = prototype_term_new_binding(terms)) ==
				PROTOTYPE_INVALID_ID ||
			prototype_context_extend(
				contexts,
				target_context,
				candidate_binder,
				classifier,
				PROTOTYPE_INVALID_ID,
				&extended_context
			) != 0) {
			return -1;
		}
		const struct prototype_context* target_entry =
			prototype_context_get(contexts, extended_context);
		uint32_t projection;
		uint32_t weakened_substitution;
		uint32_t variable;
		uint32_t extended_substitution;
		if (!target_entry ||
			prototype_substitution_projection(
				substitutions, contexts, extended_context, &projection
			) != 0 ||
			prototype_substitution_compose(
				substitutions,
				contexts,
				substitution,
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
				source_path[i],
				variable,
				classifier,
				PROTOTYPE_INVALID_ID,
				&extended_substitution
			) != 0) {
			return -1;
		}
		binders[i] = target_entry->binding_id;
		target_context = extended_context;
		substitution = extended_substitution;
	}
	*p_binder_count = source_count;
	*p_target_extension = target_context;
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
				entry->classifier,
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
				PROTOTYPE_INVALID_ID,
				&substitution
			) != 0) {
			return -1;
		}
	}
	*p_substitution = substitution;
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
	const struct prototype_substitution* prefix =
		prototype_substitution_get(substitutions, prefix_substitution);
	if (!contexts || !substitutions || !terms || !type_declarations ||
		!prefix || !p_classifier ||
		(previous_term_count > 0 && !previous_terms) ||
		prefix->target_context != telescope_base ||
		previous_term_count < entry_index) {
		return -1;
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
		) != 0 || entry_index >= path_count) {
		return -1;
	}
	uint32_t substitution = prefix_substitution;
	for (uint32_t i = 0; i <= entry_index; ++i) {
		const struct prototype_context* entry =
			prototype_context_get(contexts, path[i]);
		uint32_t classifier;
		uint32_t whnf;
		if (!entry || prototype_term_reindex(
				terms,
				type_declarations,
				contexts,
				substitutions,
				entry->classifier,
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
		if (i == entry_index) {
			*p_classifier = whnf;
			return 0;
		}
		if (prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				type_declarations,
				substitution,
				path[i],
				previous_terms[i],
				whnf,
				PROTOTYPE_INVALID_ID,
				&substitution
			) != 0) {
			return -1;
		}
	}
	return -1;
}
