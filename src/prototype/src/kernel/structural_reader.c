#include "a_program/kernel/structural_reader.h"

#include "a_program/kernel/context.h"

static int producer_term_read(
	const void* state,
	uint32_t term_id,
	const struct prototype_term** p_term
) {
	const struct prototype_term_db* db = state;
	if (!db || !p_term || term_id >= db->term_count) return -1;
	*p_term = &db->terms[term_id];
	return 0;
}

static int producer_case_read(
	const void* state,
	uint32_t case_id,
	const struct prototype_match_case** p_case
) {
	const struct prototype_term_db* db = state;
	if (!db || !p_case || case_id >= db->case_count) return -1;
	*p_case = &db->cases[case_id];
	return 0;
}

static int producer_case_binder_read(
	const void* state,
	uint32_t binder_id,
	const struct prototype_case_binder** p_binder
) {
	const struct prototype_term_db* db = state;
	if (!db || !p_binder || binder_id >= db->case_binder_count) return -1;
	*p_binder = &db->case_binders[binder_id];
	return 0;
}

static int producer_ih_scope_read(
	const void* state,
	uint32_t scope_id,
	struct prototype_term_structural_ih_scope* p_scope
) {
	const struct prototype_term_db* db = state;
	if (!db || !p_scope || scope_id >= db->ih_scope_count) return -1;
	p_scope->match_term = db->ih_scopes[scope_id].match_term;
	p_scope->scrutinee_binding_id = db->ih_scopes[scope_id].scrutinee_binding_id;
	return 0;
}

static int producer_fold_clause_read(
	const void* state,
	uint32_t clause_id,
	const struct prototype_computation_fold_clause** p_clause
) {
	const struct prototype_term_db* db = state;
	if (!db || !p_clause || clause_id >= db->computation_fold_clause_count) {
		return -1;
	}
	*p_clause = &db->computation_fold_clauses[clause_id];
	return 0;
}

int prototype_term_structural_reader_from_db(
	const struct prototype_term_db* db,
	struct prototype_term_structural_reader* p_reader
) {
	if (!db || !p_reader || (db->term_count != 0 && !db->terms)) return -1;
	*p_reader = (struct prototype_term_structural_reader) {
		.state = db,
		.term_count = db->term_count,
		.case_count = db->case_count,
		.case_binder_count = db->case_binder_count,
		.ih_scope_count = db->ih_scope_count,
		.fold_clause_count = db->computation_fold_clause_count,
		.read_term = producer_term_read,
		.read_case = producer_case_read,
		.read_case_binder = producer_case_binder_read,
		.read_ih_scope = producer_ih_scope_read,
		.read_fold_clause = producer_fold_clause_read
	};
	return 0;
}

int prototype_term_structural_read(
	const struct prototype_term_structural_reader* reader,
	uint32_t term_id,
	const struct prototype_term** p_term
) {
	if (!reader || !reader->read_term || !p_term ||
		term_id >= reader->term_count) return -1;
	return reader->read_term(reader->state, term_id, p_term);
}

int prototype_term_structural_pure_family_parts(
	const struct prototype_term_structural_reader* reader,
	uint32_t family_id,
	uint32_t* p_binding_id,
	uint32_t* p_body_id
) {
	const struct prototype_term* thunk;
	const struct prototype_term* lambda;
	const struct prototype_term* returned;
	if (!p_binding_id || !p_body_id || prototype_term_structural_read(
			reader, family_id, &thunk
		) != 0 || thunk->tag != PROTOTYPE_TERM_THUNK ||
		prototype_term_structural_read(
			reader, thunk->as.thunk.computation, &lambda
		) != 0 || lambda->tag != PROTOTYPE_TERM_LAMBDA ||
		prototype_term_structural_read(
			reader, lambda->as.lambda.body, &returned
		) != 0 || returned->tag != PROTOTYPE_TERM_RETURN) return -1;
	*p_binding_id = lambda->as.lambda.binding_id;
	*p_body_id = returned->as.return_term.value;
	return 0;
}

int prototype_term_structural_pi_parts(
	const struct prototype_term_structural_reader* reader,
	uint32_t pi_id,
	uint32_t* p_domain_id,
	uint32_t* p_binding_id,
	uint32_t* p_body_id
) {
	const struct prototype_term* pi;
	if (!p_domain_id || prototype_term_structural_read(
			reader, pi_id, &pi
		) != 0 || pi->tag != PROTOTYPE_TERM_PI ||
		prototype_term_structural_pure_family_parts(
			reader, pi->as.pi.codomain_family, p_binding_id, p_body_id
		) != 0) return -1;
	*p_domain_id = pi->as.pi.domain;
	return 0;
}

static int producer_context_read(
	const void* state,
	uint32_t context_id,
	struct prototype_context_structural_record* p_record
) {
	const struct prototype_context_db* db = state;
	const struct prototype_context* context = prototype_context_get(db, context_id);
	if (!context || !p_record) return -1;
	if (context_id != 0 && context->classifier_ref.kind !=
		PROTOTYPE_CONTEXT_CLASSIFIER_REF_TERM) return 1;
	*p_record = (struct prototype_context_structural_record) {
		.parent = context->parent,
		.binding_id = context->binding_id,
		.classifier = context_id == 0 ? PROTOTYPE_INVALID_ID :
			context->classifier_ref.term_id,
		.extension_kind = context->extension_kind,
		.producer_computation = context->producer_computation
	};
	return 0;
}

int prototype_context_structural_reader_from_db(
	const struct prototype_context_db* db,
	struct prototype_context_structural_reader* p_reader
) {
	if (!db || !p_reader || db->context_count == 0 || !db->contexts) return -1;
	*p_reader = (struct prototype_context_structural_reader) {
		.state = db,
		.count = db->context_count,
		.read = producer_context_read
	};
	return 0;
}

static int producer_substitution_read(
	const void* state,
	uint32_t substitution_id,
	struct prototype_substitution_structural_record* p_record
) {
	const struct prototype_substitution* substitution =
		prototype_substitution_get(state, substitution_id);
	if (!substitution || !p_record) return -1;
	*p_record = (struct prototype_substitution_structural_record) {
		.kind = substitution->kind,
		.source_context = substitution->source_context,
		.target_context = substitution->target_context,
		.first = substitution->first,
		.second = substitution->second,
		.term = substitution->term,
		.term_classifier = substitution->term_classifier
	};
	return 0;
}

int prototype_substitution_structural_reader_from_db(
	const struct prototype_substitution_db* db,
	struct prototype_substitution_structural_reader* p_reader
) {
	if (!db || !p_reader ||
		(db->substitution_count != 0 && !db->substitutions)) return -1;
	*p_reader = (struct prototype_substitution_structural_reader) {
		.state = db,
		.count = db->substitution_count,
		.read = producer_substitution_read
	};
	return 0;
}

int prototype_context_structural_read(
	const struct prototype_context_structural_reader* reader,
	uint32_t context_id,
	struct prototype_context_structural_record* p_record
) {
	if (!reader || !reader->read || !p_record || context_id >= reader->count) {
		return -1;
	}
	return reader->read(reader->state, context_id, p_record);
}

int prototype_context_structural_validate(
	const struct prototype_context_structural_reader* reader,
	size_t term_count
) {
	if (!reader || reader->count == 0) return -1;
	for (uint32_t i = 0; i < reader->count; ++i) {
		struct prototype_context_structural_record context;
		if (prototype_context_structural_read(reader, i, &context) != 0) return -1;
		if (i == 0) {
			if (context.parent != PROTOTYPE_INVALID_ID ||
				context.binding_id != PROTOTYPE_INVALID_ID ||
				context.classifier != PROTOTYPE_INVALID_ID ||
				context.extension_kind !=
					PROTOTYPE_STRUCTURAL_CONTEXT_EXTENSION_INVALID ||
				context.producer_computation != PROTOTYPE_INVALID_ID) return -1;
			continue;
		}
		if (context.parent >= i || context.binding_id == PROTOTYPE_INVALID_ID ||
			context.classifier >= term_count ||
			(context.extension_kind !=
				PROTOTYPE_STRUCTURAL_CONTEXT_EXTENSION_VALUE &&
			 context.extension_kind !=
				PROTOTYPE_STRUCTURAL_CONTEXT_EXTENSION_SEQUENCE_RESULT) ||
			((context.extension_kind ==
				PROTOTYPE_STRUCTURAL_CONTEXT_EXTENSION_VALUE) !=
			 (context.producer_computation == PROTOTYPE_INVALID_ID)) ||
			(context.producer_computation != PROTOTYPE_INVALID_ID &&
			 context.producer_computation >= term_count)) return -1;
	}
	return 0;
}

int prototype_context_structural_find_binding(
	const struct prototype_context_structural_reader* reader,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t* p_entry_context_id,
	uint32_t* p_classifier
) {
	if (!reader || context_id >= reader->count ||
		binding_id == PROTOTYPE_INVALID_ID) return -1;
	for (size_t steps = 0; context_id != 0; ++steps) {
		struct prototype_context_structural_record context;
		if (steps >= reader->count ||
			prototype_context_structural_read(reader, context_id, &context) != 0) {
			return -1;
		}
		if (context.binding_id == binding_id) {
			if (p_entry_context_id) *p_entry_context_id = context_id;
			if (p_classifier) *p_classifier = context.classifier;
			return 0;
		}
		context_id = context.parent;
	}
	return 1;
}

int prototype_context_structural_path_length(
	const struct prototype_context_structural_reader* reader,
	uint32_t ancestor,
	uint32_t descendant,
	size_t* p_count
) {
	if (!reader || !p_count || ancestor >= reader->count ||
		descendant >= reader->count) return -1;
	size_t count = 0;
	while (descendant != ancestor) {
		struct prototype_context_structural_record context;
		if (descendant == 0 || count >= reader->count ||
			prototype_context_structural_read(reader, descendant, &context) != 0) {
			return -1;
		}
		descendant = context.parent;
		++count;
	}
	*p_count = count;
	return 0;
}

int prototype_context_structural_is_ancestor(
	const struct prototype_context_structural_reader* reader,
	uint32_t ancestor,
	uint32_t descendant
) {
	if (!reader || ancestor >= reader->count || descendant >= reader->count) {
		return -1;
	}
	for (size_t steps = 0; descendant != ancestor; ++steps) {
		struct prototype_context_structural_record context;
		if (descendant == 0) return 0;
		if (steps >= reader->count || prototype_context_structural_read(
				reader, descendant, &context
			) != 0 || context.parent >= descendant) return -1;
		descendant = context.parent;
	}
	return 1;
}

int prototype_context_structural_path(
	const struct prototype_context_structural_reader* reader,
	uint32_t ancestor,
	uint32_t descendant,
	uint32_t* path,
	size_t path_capacity,
	size_t* p_count
) {
	size_t count;
	if ((!path && path_capacity != 0) || !p_count ||
		prototype_context_structural_path_length(
			reader, ancestor, descendant, &count
		) != 0 || count > path_capacity) return -1;
	uint32_t cursor = descendant;
	for (size_t i = count; i != 0; --i) {
		struct prototype_context_structural_record context;
		path[i - 1] = cursor;
		if (prototype_context_structural_read(reader, cursor, &context) != 0) {
			return -1;
		}
		cursor = context.parent;
	}
	*p_count = count;
	return 0;
}

int prototype_substitution_structural_read(
	const struct prototype_substitution_structural_reader* reader,
	uint32_t substitution_id,
	struct prototype_substitution_structural_record* p_record
) {
	if (!reader || !reader->read || !p_record ||
		substitution_id >= reader->count) return -1;
	return reader->read(reader->state, substitution_id, p_record);
}

int prototype_substitution_structural_validate(
	const struct prototype_substitution_structural_reader* substitutions,
	const struct prototype_context_structural_reader* contexts,
	size_t term_count
) {
	if (!substitutions || !contexts) return -1;
	for (uint32_t i = 0; i < substitutions->count; ++i) {
		struct prototype_substitution_structural_record substitution;
		if (prototype_substitution_structural_read(
				substitutions, i, &substitution
			) != 0 || substitution.source_context >= contexts->count ||
			substitution.target_context >= contexts->count) return -1;
		int empty_payload = substitution.first == PROTOTYPE_INVALID_ID &&
			substitution.second == PROTOTYPE_INVALID_ID &&
			substitution.term == PROTOTYPE_INVALID_ID &&
			substitution.term_classifier == PROTOTYPE_INVALID_ID;
		switch (substitution.kind) {
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_IDENTITY:
				if (!empty_payload || substitution.source_context !=
					substitution.target_context) return -1;
				break;
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_EMPTY:
				if (!empty_payload || substitution.target_context != 0) return -1;
				break;
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_PROJECTION: {
				struct prototype_context_structural_record source;
				if (!empty_payload || substitution.source_context == 0 ||
					prototype_context_structural_read(
						contexts, substitution.source_context, &source
					) != 0 || source.parent != substitution.target_context) return -1;
				break;
			}
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_EXTEND: {
				struct prototype_substitution_structural_record prefix;
				struct prototype_context_structural_record target;
				if (substitution.first >= i ||
					substitution.second != PROTOTYPE_INVALID_ID ||
					substitution.term >= term_count ||
					substitution.term_classifier >= term_count ||
					substitution.target_context == 0 ||
					prototype_substitution_structural_read(
						substitutions, substitution.first, &prefix
					) != 0 || prototype_context_structural_read(
						contexts, substitution.target_context, &target
					) != 0 || prefix.source_context !=
						substitution.source_context ||
					prefix.target_context != target.parent) return -1;
				break;
			}
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_COMPOSE: {
				struct prototype_substitution_structural_record outer;
				struct prototype_substitution_structural_record inner;
				if (substitution.first >= i || substitution.second >= i ||
					substitution.term != PROTOTYPE_INVALID_ID ||
					substitution.term_classifier != PROTOTYPE_INVALID_ID ||
					prototype_substitution_structural_read(
						substitutions, substitution.first, &outer
					) != 0 || prototype_substitution_structural_read(
						substitutions, substitution.second, &inner
					) != 0 || outer.source_context != inner.target_context ||
					substitution.source_context != inner.source_context ||
					substitution.target_context != outer.target_context) return -1;
				break;
			}
			default:
				return -1;
		}
	}
	return 0;
}

int prototype_substitution_structural_binding_image(
	const struct prototype_substitution_structural_reader* substitutions,
	const struct prototype_context_structural_reader* contexts,
	uint32_t substitution_id,
	uint32_t binding_id,
	struct prototype_substitution_structural_image* p_image
) {
	if (!substitutions || !contexts || !p_image ||
		binding_id == PROTOTYPE_INVALID_ID) return -1;
	for (size_t steps = 0; steps < substitutions->count; ++steps) {
		struct prototype_substitution_structural_record substitution;
		if (prototype_substitution_structural_read(
				substitutions, substitution_id, &substitution
			) != 0) return -1;
		switch (substitution.kind) {
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_IDENTITY:
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_PROJECTION:
				*p_image = (struct prototype_substitution_structural_image) {
					.kind = PROTOTYPE_SUBSTITUTION_STRUCTURAL_IMAGE_VARIABLE,
					.binding_id = binding_id,
					.term = PROTOTYPE_INVALID_ID
				};
				return 0;
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_EXTEND: {
				struct prototype_context_structural_record target;
				if (prototype_context_structural_read(
						contexts, substitution.target_context, &target
					) != 0) return -1;
				if (target.binding_id == binding_id) {
					*p_image = (struct prototype_substitution_structural_image) {
						.kind = PROTOTYPE_SUBSTITUTION_STRUCTURAL_IMAGE_TERM,
						.binding_id = PROTOTYPE_INVALID_ID,
						.term = substitution.term
					};
					return 0;
				}
				substitution_id = substitution.first;
				break;
			}
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_EMPTY:
			case PROTOTYPE_STRUCTURAL_SUBSTITUTION_COMPOSE:
				return 1;
			default:
				return -1;
		}
	}
	return -1;
}
