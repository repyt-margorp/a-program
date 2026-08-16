void prototype_term_db_init(
	struct prototype_term_db* db,
	struct prototype_term* terms,
	size_t term_capacity,
	struct prototype_match_case* cases,
	int* case_label_symbols,
	size_t case_capacity,
	struct prototype_case_binder* case_binders,
	size_t case_binder_capacity,
	struct prototype_ih_scope* ih_scopes,
	size_t ih_scope_capacity
) {
	memset(db, 0, sizeof(*db));
	db->terms = terms;
	db->term_capacity = term_capacity;
	db->cases = cases;
	db->case_label_symbols = case_label_symbols;
	db->case_capacity = case_capacity;
	db->case_binders = case_binders;
	db->case_binder_capacity = case_binder_capacity;
	db->ih_scopes = ih_scopes;
	db->ih_scope_capacity = ih_scope_capacity;
	for (uint32_t i = 0; i < PROTOTYPE_SCOPE_BINDING_CAPACITY; ++i) {
		db->scope_bindings[i] = PROTOTYPE_INVALID_ID;
	}
	db->normalization_graph_revision = 1;
}

void prototype_term_normalization_cache_clear(struct prototype_term_db* db) {
	if (!db) {
		return;
	}
	memset(db->normalization_cache, 0, sizeof(db->normalization_cache));
	memset(&db->normalization_cache_stats, 0, sizeof(db->normalization_cache_stats));
	db->normalization_cache_next = 0;
}

void prototype_term_normalization_cache_get_stats(
	const struct prototype_term_db* db,
	struct prototype_term_normalization_cache_stats* p_stats
) {
	if (!p_stats) {
		return;
	}
	memset(p_stats, 0, sizeof(*p_stats));
	if (db) {
		*p_stats = db->normalization_cache_stats;
	}
}

static void invalidate_normalization_cache(struct prototype_term_db* db) {
	if (!db) {
		return;
	}
	db->normalization_graph_revision++;
	if (db->normalization_graph_revision == 0) {
		db->normalization_graph_revision = 1;
		prototype_term_normalization_cache_clear(db);
	}
}

void prototype_term_notify_graph_mutation(struct prototype_term_db* db) {
	invalidate_normalization_cache(db);
}

static int add_term(struct prototype_term_db* db, struct prototype_term term, uint32_t* p_ret) {
	if (!db || !p_ret || reserve_slot(db->term_count, db->term_capacity) != 0) {
		return -1;
	}

	uint32_t temp_id = (uint32_t)db->term_count;
	size_t saved_term_count = db->term_count;
	db->terms[temp_id] = term;
	db->term_count = saved_term_count + 1;
	for (uint32_t i = 0; i < temp_id; ++i) {
		uint32_t existing = i;
		if (term_intern_alpha_equal_local(db, existing, temp_id)) {
			db->term_count = saved_term_count;
			*p_ret = i;
			return 0;
		}
	}
	db->term_count = saved_term_count;

	uint32_t id = (uint32_t)db->term_count;
	db->terms[id] = term;
	db->term_count++;
	*p_ret = id;
	return 0;
}

struct relocated_term_import {
	struct prototype_term_db* target;
	const struct prototype_term_db* source;
	const uint32_t* type_relocation;
	size_t type_relocation_count;
	const uint32_t* binding_relocation;
	size_t binding_relocation_count;
	uint32_t universe_offset;
	const uint32_t* representation_relocation;
	size_t representation_relocation_count;
	uint32_t* terms;
	unsigned char* states;
	uint32_t* ih_scopes;
};

static uint32_t relocate_import_binding(
	uint32_t binding_id,
	const uint32_t* relocation,
	size_t relocation_count
) {
	if (binding_id == PROTOTYPE_INVALID_ID || !relocation ||
		binding_id >= relocation_count) {
		return PROTOTYPE_INVALID_ID;
	}
	return relocation[binding_id];
}

static int import_relocated_term(
	struct relocated_term_import* import,
	uint32_t source_term_id,
	uint32_t* p_target_term_id
);

static int import_relocated_ih_scope(
	struct relocated_term_import* import,
	uint32_t source_scope_id,
	uint32_t* p_target_scope_id
) {
	if (!import || !p_target_scope_id ||
		source_scope_id >= import->source->ih_scope_count) {
		return -1;
	}
	if (import->ih_scopes[source_scope_id] == PROTOTYPE_INVALID_ID) {
		uint32_t target_scope_id = prototype_term_new_ih_scope(import->target);
		if (target_scope_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		import->ih_scopes[source_scope_id] = target_scope_id;
	}
	*p_target_scope_id = import->ih_scopes[source_scope_id];
	return 0;
}

static int import_relocated_match(
	struct relocated_term_import* import,
	const struct prototype_term* source_term,
	uint32_t* p_target_term_id
) {
	if (!import || !source_term || !p_target_term_id ||
		source_term->as.match.first_case + source_term->as.match.case_count >
			import->source->case_count) {
		return -1;
	}
	uint32_t scrutinee;
	if (import_relocated_term(
			import, source_term->as.match.scrutinee, &scrutinee
		) != 0) {
		return -1;
	}
	uint32_t case_count = source_term->as.match.case_count;
	size_t binder_capacity = import->source->case_binder_count == 0 ? 1 :
		import->source->case_binder_count;
	struct prototype_match_case_input cases[case_count == 0 ? 1 : case_count];
	struct prototype_case_binder relocated_binders[binder_capacity];
	uint32_t binder_count = 0;
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t source_case_id = source_term->as.match.first_case + i;
		const struct prototype_match_case* source_case =
			&import->source->cases[source_case_id];
		if (source_case->first_binder + source_case->binder_count >
				import->source->case_binder_count ||
			binder_count + source_case->binder_count > binder_capacity) {
			return -1;
		}
		uint32_t owner = PROTOTYPE_INVALID_ID;
		uint32_t body;
		if ((source_case->constructor_owner != PROTOTYPE_INVALID_ID &&
			 import_relocated_term(
				import, source_case->constructor_owner, &owner
			 ) != 0) || import_relocated_term(
				import, source_case->body, &body
			) != 0) {
			return -1;
		}
		uint32_t first_binder = binder_count;
		for (uint32_t j = 0; j < source_case->binder_count; ++j) {
			relocated_binders[binder_count] = import->source->case_binders[
				source_case->first_binder + j
			];
			relocated_binders[binder_count].binding_id = relocate_import_binding(
				relocated_binders[binder_count].binding_id,
				import->binding_relocation,
				import->binding_relocation_count
			);
			if (relocated_binders[binder_count].binding_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			++binder_count;
		}
		cases[i] = (struct prototype_match_case_input) {
			.case_label_symbol_id =
				import->source->case_label_symbols[source_case_id],
			.constructor_owner = owner,
			.constructor_id = source_case->constructor_id,
			.binders = &relocated_binders[first_binder],
			.binder_count = source_case->binder_count,
			.body = body
		};
	}
	uint32_t target_scope_id = PROTOTYPE_INVALID_ID;
	if (source_term->as.match.ih_scope_id != PROTOTYPE_INVALID_ID &&
		import_relocated_ih_scope(
			import, source_term->as.match.ih_scope_id, &target_scope_id
		) != 0) {
		return -1;
	}
	if (prototype_term_match_with_ih_scope(
			import->target,
			scrutinee,
			cases,
			case_count,
			target_scope_id,
			p_target_term_id
		) != 0) {
		return -1;
	}
	return target_scope_id == PROTOTYPE_INVALID_ID ? 0 :
		prototype_term_set_ih_scope_term(
			import->target, target_scope_id, *p_target_term_id
		);
}

static int import_relocated_term(
	struct relocated_term_import* import,
	uint32_t source_term_id,
	uint32_t* p_target_term_id
) {
	if (!import || !p_target_term_id ||
		source_term_id >= import->source->term_count ||
		import->source->terms[source_term_id].tag == 0) {
		fprintf(
			stderr,
			"term relocation: invalid source term id=%u count=%zu tag=%d\n",
			source_term_id,
			import && import->source ? import->source->term_count : 0,
			import && import->source && source_term_id < import->source->term_count ?
				import->source->terms[source_term_id].tag : -1
		);
		return -1;
	}
	if (import->states[source_term_id] == 2) {
		*p_target_term_id = import->terms[source_term_id];
		return 0;
	}
	if (import->states[source_term_id] != 0) {
		fprintf(
			stderr,
			"term relocation: cyclic source term id=%u tag=%d\n",
			source_term_id,
			import->source->terms[source_term_id].tag
		);
		return -1;
	}
	import->states[source_term_id] = 1;
	struct prototype_term term = import->source->terms[source_term_id];
	uint32_t result = PROTOTYPE_INVALID_ID;
#define IMPORT_TERM_REF(field) \
	do { \
		if (import_relocated_term(import, (field), &(field)) != 0) { \
			fprintf( \
				stderr, \
				"term relocation: parent id=%u tag=%d has invalid reference\n", \
				source_term_id, \
				term.tag \
			); \
			return -1; \
		} \
	} while (0)
	switch (term.tag) {
		case PROTOTYPE_TERM_VAR:
			term.as.var.binding_id = relocate_import_binding(
				term.as.var.binding_id,
				import->binding_relocation,
				import->binding_relocation_count
			);
			if (term.as.var.binding_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			IMPORT_TERM_REF(term.as.constructor.owner);
			break;
		case PROTOTYPE_TERM_APP:
			IMPORT_TERM_REF(term.as.app.function);
			IMPORT_TERM_REF(term.as.app.argument);
			break;
		case PROTOTYPE_TERM_LAMBDA:
			term.as.lambda.binding_id = relocate_import_binding(
				term.as.lambda.binding_id,
				import->binding_relocation,
				import->binding_relocation_count
			);
			if (term.as.lambda.binding_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			IMPORT_TERM_REF(term.as.lambda.body);
			break;
		case PROTOTYPE_TERM_PI:
			IMPORT_TERM_REF(term.as.pi.domain);
			IMPORT_TERM_REF(term.as.pi.codomain_family);
			break;
		case PROTOTYPE_TERM_MATCH:
			if (import_relocated_match(import, &term, &result) != 0) {
				return -1;
			}
			goto imported;
		case PROTOTYPE_TERM_TYPE_FORMER:
			if (term.as.type_former.representation_id >=
					import->representation_relocation_count) {
				return -1;
			}
			term.as.type_former.representation_id =
				import->representation_relocation[
					term.as.type_former.representation_id
				];
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			if (term.as.type_declaration.type_id >=
					import->type_relocation_count) {
				return -1;
			}
			term.as.type_declaration.type_id = import->type_relocation[
				term.as.type_declaration.type_id
			];
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (term.as.type_view.view_type_id >= import->type_relocation_count) {
				return -1;
			}
			term.as.type_view.view_type_id = import->type_relocation[
				term.as.type_view.view_type_id
			];
			IMPORT_TERM_REF(term.as.type_view.core);
			IMPORT_TERM_REF(term.as.type_view.source);
			break;
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			if (import_relocated_ih_scope(
					import,
					term.as.induction_hypothesis.ih_scope_id,
					&term.as.induction_hypothesis.ih_scope_id
				) != 0) {
				return -1;
			}
			IMPORT_TERM_REF(term.as.induction_hypothesis.argument);
			break;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			term.as.universe_var.level_var += import->universe_offset;
			break;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			IMPORT_TERM_REF(term.as.effect_operation.classifier);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			term.as.effect_row_var.binding_id = relocate_import_binding(
				term.as.effect_row_var.binding_id,
				import->binding_relocation,
				import->binding_relocation_count
			);
			if (term.as.effect_row_var.binding_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			IMPORT_TERM_REF(term.as.effect_row_union.left);
			IMPORT_TERM_REF(term.as.effect_row_union.right);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			term.as.effect_row_forall.binding_id = relocate_import_binding(
				term.as.effect_row_forall.binding_id,
				import->binding_relocation,
				import->binding_relocation_count
			);
			if (term.as.effect_row_forall.binding_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			IMPORT_TERM_REF(term.as.effect_row_forall.body);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			IMPORT_TERM_REF(term.as.effect_row_operation.latent_row);
			break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			IMPORT_TERM_REF(term.as.computation_type.label);
			IMPORT_TERM_REF(term.as.computation_type.result);
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			IMPORT_TERM_REF(term.as.thunk_type.computation);
			break;
		case PROTOTYPE_TERM_RETURN:
			IMPORT_TERM_REF(term.as.return_term.value);
			break;
		case PROTOTYPE_TERM_THUNK:
			IMPORT_TERM_REF(term.as.thunk.computation);
			break;
		case PROTOTYPE_TERM_FORCE:
			IMPORT_TERM_REF(term.as.force.value);
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			IMPORT_TERM_REF(term.as.operation_request.operation);
			IMPORT_TERM_REF(term.as.operation_request.argument);
			IMPORT_TERM_REF(term.as.operation_request.continuation);
			break;
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			IMPORT_TERM_REF(term.as.computation_fold.computation);
			IMPORT_TERM_REF(term.as.computation_fold.return_clause);
			if (term.as.computation_fold.first_clause +
					term.as.computation_fold.clause_count >
					import->source->computation_fold_clause_count) {
				return -1;
			}
			uint32_t clause_count = term.as.computation_fold.clause_count;
			struct prototype_computation_fold_clause clauses[
				clause_count == 0 ? 1 : clause_count
			];
			for (uint32_t i = 0; i < clause_count; ++i) {
				clauses[i] = import->source->computation_fold_clauses[
					term.as.computation_fold.first_clause + i
				];
				IMPORT_TERM_REF(clauses[i].operation);
				IMPORT_TERM_REF(clauses[i].body);
			}
			if (prototype_term_computation_fold(
					import->target,
					term.as.computation_fold.computation,
					term.as.computation_fold.return_clause,
					clauses,
					clause_count,
					&result
				) != 0) {
				return -1;
			}
			goto imported;
		}
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_TEXT_LITERAL:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_INT_LITERAL:
		case PROTOTYPE_TERM_EXTERNAL_REF:
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
			break;
		default:
			return -1;
	}
	if (add_term(import->target, term, &result) != 0) {
		return -1;
	}
imported:
	import->terms[source_term_id] = result;
	import->states[source_term_id] = 2;
	*p_target_term_id = result;
	return 0;
#undef IMPORT_TERM_REF
}

int prototype_term_db_append_relocated(
	struct prototype_term_db* target,
	const struct prototype_term_db* source,
	const uint32_t* type_relocation,
	size_t type_relocation_count,
	const uint32_t* binding_relocation,
	size_t binding_relocation_count,
	uint32_t universe_offset,
	const uint32_t* representation_relocation,
	size_t representation_relocation_count,
	const uint32_t* source_order,
	size_t source_order_count,
	uint32_t* term_relocation,
	size_t term_relocation_capacity
) {
	if (!target || !source || !type_relocation || !binding_relocation ||
		!representation_relocation ||
		!term_relocation ||
		source->term_count > term_relocation_capacity ||
		target->term_count + source->term_count > target->term_capacity) {
		return -1;
	}
	size_t state_count = source->term_count == 0 ? 1 : source->term_count;
	size_t scope_count = source->ih_scope_count == 0 ? 1 : source->ih_scope_count;
	unsigned char states[state_count];
	uint32_t ih_scope_relocation[scope_count];
	memset(states, 0, sizeof(states));
	for (size_t i = 0; i < term_relocation_capacity; ++i) {
		term_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	for (size_t i = 0; i < scope_count; ++i) {
		ih_scope_relocation[i] = PROTOTYPE_INVALID_ID;
	}
	struct relocated_term_import import = {
		.target = target,
		.source = source,
		.type_relocation = type_relocation,
		.type_relocation_count = type_relocation_count,
		.binding_relocation = binding_relocation,
		.binding_relocation_count = binding_relocation_count,
		.universe_offset = universe_offset,
		.representation_relocation = representation_relocation,
		.representation_relocation_count = representation_relocation_count,
		.terms = term_relocation,
		.states = states,
		.ih_scopes = ih_scope_relocation
	};
	size_t root_count = source_order ? source_order_count : source->term_count;
	for (size_t i = 0; i < root_count; ++i) {
		uint32_t source_id = source_order ? source_order[i] : (uint32_t)i;
		if (source_id >= source->term_count) {
			return -1;
		}
		if (source->terms[source_id].tag == 0) {
			continue;
		}
		uint32_t ignored;
		if (import_relocated_term(&import, source_id, &ignored) != 0) {
			fprintf(
				stderr,
				"term relocation: failed root id=%u tag=%d\n",
				source_id,
				source->terms[source_id].tag
			);
			return -1;
		}
	}
	/* An occurrence-local IH scope can be reached through an IH term while its
	 * alpha-interned Match uses a different canonical scope. Importing the IH
	 * allocates the occurrence scope, but only the Match-owned scope is closed
	 * during recursive term import. Once every term relocation is known, close
	 * every reached scope over the relocated Match explicitly. */
	for (size_t source_scope = 0; source_scope < source->ih_scope_count;
		++source_scope) {
		uint32_t target_scope = ih_scope_relocation[source_scope];
		if (target_scope == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t source_match = source->ih_scopes[source_scope].match_term;
		uint32_t source_scrutinee_binding =
			source->ih_scopes[source_scope].scrutinee_binding_id;
		uint32_t target_scrutinee_binding = source_scrutinee_binding ==
			PROTOTYPE_INVALID_ID ? PROTOTYPE_INVALID_ID : relocate_import_binding(
				source_scrutinee_binding,
				binding_relocation,
				binding_relocation_count
			);
		if (source_match >= source->term_count ||
			term_relocation[source_match] == PROTOTYPE_INVALID_ID ||
			(source_scrutinee_binding != PROTOTYPE_INVALID_ID &&
			 target_scrutinee_binding == PROTOTYPE_INVALID_ID)) {
			return -1;
		}
		target->ih_scopes[target_scope].scrutinee_binding_id =
			target_scrutinee_binding;
		if (
			prototype_term_set_ih_scope_term(
				target,
				target_scope,
				term_relocation[source_match]
			) != 0) {
			return -1;
		}
	}
	return 0;
}

uint32_t prototype_term_binding_for_scope_slot(struct prototype_term_db* db, uint32_t scope_slot) {
	if (!db || scope_slot >= PROTOTYPE_SCOPE_BINDING_CAPACITY) {
		return PROTOTYPE_INVALID_ID;
	}
	if (db->scope_bindings[scope_slot] != PROTOTYPE_INVALID_ID) {
		return db->scope_bindings[scope_slot];
	}
	if (db->next_binding_id == PROTOTYPE_INVALID_ID) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t binding_id = db->next_binding_id++;
	db->scope_bindings[scope_slot] = binding_id;
	return binding_id;
}

uint32_t prototype_term_new_binding(struct prototype_term_db* db) {
	if (!db || db->next_binding_id == PROTOTYPE_INVALID_ID) {
		return PROTOTYPE_INVALID_ID;
	}
	return db->next_binding_id++;
}

uint32_t prototype_term_new_ih_scope(struct prototype_term_db* db) {
	if (!db || reserve_slot(db->ih_scope_count, db->ih_scope_capacity) != 0) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t id = (uint32_t)db->ih_scope_count;
	db->ih_scopes[id].match_term = PROTOTYPE_INVALID_ID;
	db->ih_scopes[id].scrutinee_binding_id = PROTOTYPE_INVALID_ID;
	memset(&db->ih_scopes[id].key, 0, sizeof(db->ih_scopes[id].key));
	db->ih_scope_count++;
	return id;
}

int prototype_term_set_ih_scope_term(
	struct prototype_term_db* db,
	uint32_t ih_scope_id,
	uint32_t match_term
) {
	if (!db || ih_scope_id >= db->ih_scope_count || match_term >= db->term_count) {
		return -1;
	}
	db->ih_scopes[ih_scope_id].match_term = match_term;
	const struct prototype_term* match = &db->terms[match_term];
	if (db->ih_scopes[ih_scope_id].scrutinee_binding_id == PROTOTYPE_INVALID_ID &&
		match->tag == PROTOTYPE_TERM_MATCH &&
		match->as.match.scrutinee < db->term_count &&
		db->terms[match->as.match.scrutinee].tag == PROTOTYPE_TERM_VAR) {
		db->ih_scopes[ih_scope_id].scrutinee_binding_id =
			db->terms[match->as.match.scrutinee].as.var.binding_id;
	}
	if (compute_ih_scope_key(db, match_term, &db->ih_scopes[ih_scope_id].key) != 0) {
		memset(&db->ih_scopes[ih_scope_id].key, 0, sizeof(db->ih_scopes[ih_scope_id].key));
		return -1;
	}
	invalidate_normalization_cache(db);
	return 0;
}

int prototype_term_ih_scope_key(
	const struct prototype_term_db* db,
	uint32_t ih_scope_id,
	struct prototype_ih_scope_key* p_key
) {
	if (!db || !p_key || ih_scope_id >= db->ih_scope_count) {
		return -1;
	}
	*p_key = db->ih_scopes[ih_scope_id].key;
	return p_key->is_linkable ? 0 : -1;
}

int prototype_term_var(struct prototype_term_db* db, uint32_t binding_id, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_VAR;
	term.as.var.binding_id = binding_id;
	return add_term(db, term, p_ret);
}

int prototype_term_constructor(
	struct prototype_term_db* db,
	uint32_t owner,
	uint32_t constructor_id,
	uint32_t* p_ret
) {
	if (!db || owner >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_CONSTRUCTOR;
	term.as.constructor.owner = owner;
	term.as.constructor.constructor_id = constructor_id;
	return add_term(db, term, p_ret);
}

int prototype_term_app(struct prototype_term_db* db, uint32_t function, uint32_t argument, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_APP;
	term.as.app.function = function;
	term.as.app.argument = argument;
	return add_term(db, term, p_ret);
}

static int prototype_term_lambda_with_tag(
	struct prototype_term_db* db,
	int tag,
	uint32_t binding_id,
	uint32_t body,
	uint32_t* p_ret
) {
	if (!db || body >= db->term_count) {
		return -1;
	}

	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = tag;
	term.as.lambda.binding_id = binding_id;
	term.as.lambda.body = body;
	return add_term(db, term, p_ret);
}

int prototype_term_lambda(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t body,
	uint32_t* p_ret
) {
	return prototype_term_lambda_with_tag(
		db,
		PROTOTYPE_TERM_LAMBDA,
		binding_id,
		body,
		p_ret
	);
}

int prototype_term_pi_family(
	struct prototype_term_db* db,
	uint32_t domain,
	uint32_t codomain_family,
	uint32_t* p_ret
) {
	if (!db || !p_ret ||
		domain >= db->term_count ||
		codomain_family >= db->term_count ||
		prototype_term_pure_family_lambda(db, codomain_family, NULL) != 0) {
		return -1;
	}

	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PI;
	term.as.pi.domain = domain;
	term.as.pi.codomain_family = codomain_family;
	return add_term(db, term, p_ret);
}

int prototype_term_pure_family(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t body,
	uint32_t* p_family
) {
	uint32_t lambda;
	uint32_t returned_body;
	if (!db || !p_family || body >= db->term_count ||
		prototype_term_return(db, body, &returned_body) != 0 ||
		prototype_term_lambda(db, binding_id, returned_body, &lambda) != 0) {
		return -1;
	}
	return prototype_term_thunk(db, lambda, p_family);
}

int prototype_term_pure_family_lambda(
	const struct prototype_term_db* db,
	uint32_t family,
	uint32_t* p_lambda
) {
	if (!db || family >= db->term_count ||
		db->terms[family].tag != PROTOTYPE_TERM_THUNK) {
		return -1;
	}
	uint32_t lambda = db->terms[family].as.thunk.computation;
	if (lambda >= db->term_count || db->terms[lambda].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	uint32_t returned = db->terms[lambda].as.lambda.body;
	if (returned >= db->term_count || db->terms[returned].tag != PROTOTYPE_TERM_RETURN) {
		return -1;
	}
	if (p_lambda) {
		*p_lambda = lambda;
	}
	return 0;
}

int prototype_term_pure_family_parts(
	const struct prototype_term_db* db,
	uint32_t family,
	uint32_t* p_binder_id,
	uint32_t* p_body
) {
	uint32_t lambda;
	if (!db || !p_binder_id || !p_body ||
		prototype_term_pure_family_lambda(db, family, &lambda) != 0) {
		return -1;
	}
	const struct prototype_term* lambda_term = &db->terms[lambda];
	uint32_t returned = lambda_term->as.lambda.body;
	*p_binder_id = lambda_term->as.lambda.binding_id;
	*p_body = db->terms[returned].as.return_term.value;
	return 0;
}

int prototype_term_match(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	const struct prototype_match_case_input* cases,
	uint32_t case_count,
	uint32_t* p_ret
) {
	return prototype_term_match_with_ih_scope(
		db,
		scrutinee,
		cases,
		case_count,
		PROTOTYPE_INVALID_ID,
		p_ret
	);
}

int prototype_term_match_with_ih_scope(
	struct prototype_term_db* db,
	uint32_t scrutinee,
	const struct prototype_match_case_input* cases,
	uint32_t case_count,
	uint32_t ih_scope_id,
	uint32_t* p_ret
) {
	if (!db || !cases || !p_ret || !db->case_label_symbols || scrutinee >= db->term_count) {
		return -1;
	}
	if (ih_scope_id != PROTOTYPE_INVALID_ID && ih_scope_id >= db->ih_scope_count) {
		return -1;
	}
	if (db->case_count + case_count > db->case_capacity) {
		return -1;
	}

	size_t needed_binders = 0;
	for (uint32_t i = 0; i < case_count; ++i) {
		if (cases[i].body >= db->term_count) {
			return -1;
		}
		if (cases[i].constructor_owner != PROTOTYPE_INVALID_ID &&
			cases[i].constructor_owner >= db->term_count) {
			return -1;
		}
		if (cases[i].binder_count > 0 && !cases[i].binders) {
			return -1;
		}
		needed_binders += cases[i].binder_count;
	}
	if (db->case_binder_count + needed_binders > db->case_binder_capacity) {
		return -1;
	}

	size_t saved_case_count = db->case_count;
	size_t saved_case_binder_count = db->case_binder_count;
	size_t saved_term_count = db->term_count;
	uint32_t first_case = (uint32_t)db->case_count;
	for (uint32_t i = 0; i < case_count; ++i) {
		uint32_t stored_case_id = (uint32_t)db->case_count++;
		struct prototype_match_case* stored_case = &db->cases[stored_case_id];
		db->case_label_symbols[stored_case_id] = cases[i].case_label_symbol_id;
		stored_case->constructor_owner = cases[i].constructor_owner;
		stored_case->constructor_id = cases[i].constructor_id;
		stored_case->first_binder = (uint32_t)db->case_binder_count;
		stored_case->binder_count = cases[i].binder_count;
		stored_case->body = cases[i].body;

		for (uint32_t j = 0; j < cases[i].binder_count; ++j) {
			struct prototype_case_binder binder = cases[i].binders[j];
			db->case_binders[db->case_binder_count++] = binder;
		}
	}

	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_MATCH;
	term.as.match.scrutinee = scrutinee;
	term.as.match.first_case = first_case;
	term.as.match.case_count = case_count;
	term.as.match.ih_scope_id = ih_scope_id;
	if (add_term(db, term, p_ret) != 0) {
		db->case_count = saved_case_count;
		db->case_binder_count = saved_case_binder_count;
		return -1;
	}
	if (*p_ret != saved_term_count) {
		db->case_count = saved_case_count;
		db->case_binder_count = saved_case_binder_count;
	}
	return 0;
}

int prototype_term_resolve_match_case(
	struct prototype_term_db* db,
	uint32_t match_term,
	uint32_t case_index,
	uint32_t constructor_owner,
	uint32_t constructor_id
) {
	if (!db ||
		match_term >= db->term_count ||
		db->terms[match_term].tag != PROTOTYPE_TERM_MATCH ||
		constructor_owner >= db->term_count ||
		constructor_id == PROTOTYPE_INVALID_ID ||
		case_index >= db->terms[match_term].as.match.case_count) {
		return -1;
	}

	uint32_t stored_case_id = db->terms[match_term].as.match.first_case + case_index;
	if (stored_case_id >= db->case_count) {
		return -1;
	}
	if (db->cases[stored_case_id].constructor_owner != constructor_owner ||
		db->cases[stored_case_id].constructor_id != constructor_id) {
		db->cases[stored_case_id].constructor_owner = constructor_owner;
		db->cases[stored_case_id].constructor_id = constructor_id;
		invalidate_normalization_cache(db);
	}
	return 0;
}

int prototype_term_erase_constructor_view_owners(struct prototype_term_db* db) {
	if (!db) {
		return -1;
	}
	int changed = 0;
	for (size_t i = 0; i < db->term_count; ++i) {
		struct prototype_term* term = &db->terms[i];
		if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
			continue;
		}
		uint32_t owner = term->as.constructor.owner;
		for (uint32_t depth = 0;
			owner < db->term_count && db->terms[owner].tag == PROTOTYPE_TERM_TYPE_VIEW && depth < 32;
			++depth) {
			owner = db->terms[owner].as.type_view.core;
		}
		if (owner >= db->term_count) {
			return -1;
		}
		if (owner != term->as.constructor.owner) {
			term->as.constructor.owner = owner;
			changed = 1;
		}
	}
	for (size_t i = 0; i < db->case_count; ++i) {
		struct prototype_match_case* match_case = &db->cases[i];
		if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t owner = match_case->constructor_owner;
		for (uint32_t depth = 0;
			owner < db->term_count && db->terms[owner].tag == PROTOTYPE_TERM_TYPE_VIEW && depth < 32;
			++depth) {
			owner = db->terms[owner].as.type_view.core;
		}
		if (owner >= db->term_count) {
			return -1;
		}
		if (owner != match_case->constructor_owner) {
			match_case->constructor_owner = owner;
			changed = 1;
		}
	}
	if (changed) {
		invalidate_normalization_cache(db);
	}
	return 0;
}

static int prototype_term_type_former(
	struct prototype_term_db* db,
	uint32_t representation_id,
	uint32_t constructor_count,
	uint32_t* p_ret
) {
	if (!db || !p_ret) {
		return -1;
	}
	for (uint32_t i = 0; i < db->term_count; ++i) {
		struct prototype_term* existing = &db->terms[i];
		if (existing->tag != PROTOTYPE_TERM_TYPE_FORMER ||
			existing->as.type_former.representation_id != representation_id) {
			continue;
		}
		if (existing->as.type_former.constructor_count != constructor_count) {
			existing->as.type_former.constructor_count = constructor_count;
			invalidate_normalization_cache(db);
		}
		*p_ret = i;
		return 0;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_TYPE_FORMER;
	term.as.type_former.representation_id = representation_id;
	term.as.type_former.constructor_count = constructor_count;
	return add_term(db, term, p_ret);
}

static int prototype_term_type_declaration(
	struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id,
	uint32_t* p_ret
) {
	if (!db || !type_declarations || !p_ret ||
		type_id >= type_declarations->type_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_TYPE_DECLARATION;
	term.as.type_declaration.type_id = type_id;
	term.as.type_declaration.identity.namespace_symbol_id =
		type_declarations->type_declarations[type_id].namespace_symbol_id;
	term.as.type_declaration.identity.name_symbol_id =
		type_declarations->type_declarations[type_id].name_symbol_id;
	return add_term(db, term, p_ret);
}

int prototype_term_type_instance_source_make(
	struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id,
	const uint32_t* args,
	uint32_t arg_count,
	uint32_t* p_ret
) {
	if (!db || !type_declarations || !p_ret || arg_count > 16 ||
		type_id >= type_declarations->type_count ||
		(arg_count != 0 && !args)) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count > UINT32_MAX - type->index_count ||
		arg_count > type->parameter_count + type->index_count) {
		return -1;
	}
	uint32_t current;
	if (prototype_term_type_declaration(
			db, type_declarations, type_id, &current
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		if (args[i] >= db->term_count ||
			prototype_term_app(db, current, args[i], &current) != 0) {
			return -1;
		}
	}
	*p_ret = current;
	return 0;
}

static int prototype_term_type_view(
	struct prototype_term_db* db,
	uint32_t view_type_id,
	struct prototype_qualified_name identity,
	uint32_t core,
	uint32_t source,
	uint32_t* p_ret
) {
	if (!db || !p_ret || core >= db->term_count || source >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_TYPE_VIEW;
	term.as.type_view.view_type_id = view_type_id;
	term.as.type_view.identity = identity;
	term.as.type_view.core = core;
	term.as.type_view.source = source;
	return add_term(db, term, p_ret);
}

int prototype_term_type_instance_make(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id,
	const uint32_t* args,
	uint32_t arg_count,
	uint32_t* p_ret
) {
	if (!db || !type_declarations || !p_ret || arg_count > 16 ||
		type_id >= type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	if (type->parameter_count > UINT32_MAX - type->index_count ||
		arg_count > type->parameter_count + type->index_count) {
		return -1;
	}
	if (arg_count > 0 && !args) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		if (args[i] >= db->term_count) {
			return -1;
		}
	}

	uint32_t representation_id;
	if (prototype_type_declaration_intern_representation(
				db,
				type_declarations,
				type_id,
				&representation_id
		) != 0) {
		return -1;
	}
	uint32_t current;
	if (prototype_term_type_former(
			db, representation_id, type->constructor_count, &current
		) != 0) {
		return -1;
	}
	uint32_t source_current;
	if (prototype_term_type_declaration(
			db, type_declarations, type_id, &source_current
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t argument = args[i];
		if (db->terms[argument].tag == PROTOTYPE_TERM_TYPE_VIEW) {
			argument = db->terms[argument].as.type_view.core;
		}
		uint32_t next;
		if (prototype_term_app(db, current, argument, &next) != 0) {
			return -1;
		}
		current = next;
		if (prototype_term_app(db, source_current, args[i], &next) != 0) {
			return -1;
		}
		source_current = next;
	}
	struct prototype_qualified_name identity;
	identity.namespace_symbol_id =
		type_declarations->type_declarations[type_id].namespace_symbol_id;
	identity.name_symbol_id = type_declarations->type_declarations[type_id].name_symbol_id;
	return prototype_term_type_view(
		db, type_id, identity, current, source_current, p_ret
	);
}

static int type_instance_app_spine_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_type_id,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	if (!db || !p_type_id || !p_arg_count || term_id >= db->term_count) {
		return -1;
	}
	if (db->terms[term_id].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		const struct prototype_term* view = &db->terms[term_id];
		if (view->as.type_view.source >= db->term_count ||
			type_instance_app_spine_info(
				db,
				view->as.type_view.source,
				p_type_id,
				args,
				p_arg_count
			) != 0) {
			return -1;
		}
		*p_type_id = view->as.type_view.view_type_id;
		return 0;
	}

	uint32_t reversed[16];
	uint32_t count = 0;
	uint32_t current = term_id;
	while (current < db->term_count && db->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (count >= 16) {
			return -1;
		}
		reversed[count++] = db->terms[current].as.app.argument;
		current = db->terms[current].as.app.function;
	}
	if (current >= db->term_count) {
		return -1;
	}
	if (db->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		uint32_t prefix[16];
		uint32_t prefix_count;
		if (type_instance_app_spine_info(
				db, current, p_type_id, prefix, &prefix_count
			) != 0 || prefix_count + count > 16 ||
			(prefix_count + count > 0 && !args)) {
			return -1;
		}
		for (uint32_t i = 0; i < prefix_count; ++i) {
			args[i] = prefix[i];
		}
		for (uint32_t i = 0; i < count; ++i) {
			args[prefix_count + i] = reversed[count - i - 1];
		}
		*p_arg_count = prefix_count + count;
		return 0;
	}
	if (db->terms[current].tag != PROTOTYPE_TERM_TYPE_DECLARATION) {
		return -1;
	}
	if (count > 0 && !args) {
		return -1;
	}
	for (uint32_t i = 0; i < count; ++i) {
		args[i] = reversed[count - i - 1];
	}
	*p_type_id = db->terms[current].as.type_declaration.type_id;
	*p_arg_count = count;
	return 0;
}

int prototype_term_type_instance_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_type_id,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	if (!db || !p_type_id || !p_arg_count || term_id >= db->term_count) {
		return -1;
	}
	return type_instance_app_spine_info(db, term_id, p_type_id, args, p_arg_count);
}

int prototype_term_rebind_type_former_anchors(
	struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations
) {
	if (!db || !type_declarations || type_declarations->representation_db.cache_dirty) {
		return -1;
	}
	for (size_t i = 0; i < db->term_count; ++i) {
		struct prototype_term* term = &db->terms[i];
		if (term->tag != PROTOTYPE_TERM_TYPE_FORMER) {
			continue;
		}
		uint32_t declaration_anchor = term->as.type_former.representation_id;
		if (declaration_anchor >= type_declarations->type_count ||
			type_declarations->type_declarations[declaration_anchor].representation_id ==
				PROTOTYPE_INVALID_ID) {
			return -1;
		}
		term->as.type_former.representation_id =
			type_declarations->type_declarations[declaration_anchor].representation_id;
		term->as.type_former.constructor_count =
			type_declarations->type_declarations[declaration_anchor].constructor_count;
	}
	return prototype_term_canonicalize_type_former_references(db);
}

int prototype_term_canonicalize_type_former_references(
	struct prototype_term_db* db
) {
	if (!db) {
		return -1;
	}
	uint32_t* remap = calloc(db->term_count, sizeof(*remap));
	if (!remap && db->term_count > 0) {
		return -1;
	}
	for (size_t i = 0; i < db->term_count; ++i) {
		remap[i] = (uint32_t)i;
	}
	for (size_t i = 0; i < db->term_count; ++i) {
		if (db->terms[i].tag != PROTOTYPE_TERM_TYPE_FORMER) {
			continue;
		}
		for (size_t j = 0; j < i; ++j) {
			if (db->terms[j].tag == PROTOTYPE_TERM_TYPE_FORMER &&
				db->terms[j].as.type_former.representation_id ==
					db->terms[i].as.type_former.representation_id) {
				remap[i] = (uint32_t)j;
				break;
			}
		}
	}
	for (size_t i = 0; i < db->term_count; ++i) {
		while (remap[i] != i && remap[remap[i]] != remap[i]) {
			remap[i] = remap[remap[i]];
		}
	}
	for (size_t i = 0; i < db->term_count; ++i) {
		struct prototype_term* term = &db->terms[i];
		switch (term->tag) {
			case PROTOTYPE_TERM_CONSTRUCTOR:
				term->as.constructor.owner = remap[term->as.constructor.owner];
				break;
			case PROTOTYPE_TERM_APP:
				term->as.app.function = remap[term->as.app.function];
				term->as.app.argument = remap[term->as.app.argument];
				break;
			case PROTOTYPE_TERM_LAMBDA:
				term->as.lambda.body = remap[term->as.lambda.body];
				break;
			case PROTOTYPE_TERM_RETURN:
				term->as.return_term.value = remap[term->as.return_term.value];
				break;
			case PROTOTYPE_TERM_THUNK:
				term->as.thunk.computation = remap[term->as.thunk.computation];
				break;
			case PROTOTYPE_TERM_FORCE:
				term->as.force.value = remap[term->as.force.value];
				break;
			case PROTOTYPE_TERM_COMPUTATION_FOLD:
				term->as.computation_fold.computation = remap[term->as.computation_fold.computation];
				term->as.computation_fold.return_clause = remap[term->as.computation_fold.return_clause];
				for (uint32_t j = 0; j < term->as.computation_fold.clause_count; ++j) {
					struct prototype_computation_fold_clause* clause =
						&db->computation_fold_clauses[term->as.computation_fold.first_clause + j];
					clause->operation = remap[clause->operation];
					clause->body = remap[clause->body];
				}
				break;
			case PROTOTYPE_TERM_OPERATION_REQUEST:
				term->as.operation_request.operation = remap[term->as.operation_request.operation];
				term->as.operation_request.argument = remap[term->as.operation_request.argument];
				term->as.operation_request.continuation = remap[term->as.operation_request.continuation];
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				term->as.effect_row_union.left = remap[term->as.effect_row_union.left];
				term->as.effect_row_union.right = remap[term->as.effect_row_union.right];
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				term->as.effect_row_forall.body = remap[term->as.effect_row_forall.body];
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
				term->as.effect_row_operation.latent_row =
					remap[term->as.effect_row_operation.latent_row];
				break;
			case PROTOTYPE_TERM_PI:
				term->as.pi.domain = remap[term->as.pi.domain];
				term->as.pi.codomain_family = remap[term->as.pi.codomain_family];
				break;
			case PROTOTYPE_TERM_MATCH:
				term->as.match.scrutinee = remap[term->as.match.scrutinee];
				break;
			case PROTOTYPE_TERM_TYPE_VIEW:
				term->as.type_view.core = remap[term->as.type_view.core];
				term->as.type_view.source = remap[term->as.type_view.source];
				break;
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				term->as.induction_hypothesis.argument =
					remap[term->as.induction_hypothesis.argument];
				break;
			case PROTOTYPE_TERM_COMPUTATION_TYPE:
				term->as.computation_type.label = remap[term->as.computation_type.label];
				term->as.computation_type.result = remap[term->as.computation_type.result];
				break;
			case PROTOTYPE_TERM_THUNK_TYPE:
				term->as.thunk_type.computation = remap[term->as.thunk_type.computation];
				break;
			default:
				break;
		}
	}
	for (size_t i = 0; i < db->case_count; ++i) {
		db->cases[i].constructor_owner =
			db->cases[i].constructor_owner == PROTOTYPE_INVALID_ID ?
				PROTOTYPE_INVALID_ID : remap[db->cases[i].constructor_owner];
		db->cases[i].body = remap[db->cases[i].body];
	}
	for (size_t i = 0; i < db->ih_scope_count; ++i) {
		/* A frame is allocated before its Match node is committed.  During
		 * type-former rebinding that pending frame has no term yet. */
		if (db->ih_scopes[i].match_term == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (db->ih_scopes[i].match_term >= db->term_count) {
			free(remap);
			return -1;
		}
		db->ih_scopes[i].match_term = remap[db->ih_scopes[i].match_term];
	}
	free(remap);
	invalidate_normalization_cache(db);
	return 0;
}

int prototype_term_type_instance_extend(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t instance,
	uint32_t argument,
	uint32_t* p_ret
) {
	if (!db || !type_declarations || !p_ret || argument >= db->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(db, instance, &type_id, args, &arg_count) != 0 ||
		arg_count >= 16) {
		return -1;
	}
	args[arg_count] = argument;
	return prototype_term_type_instance_make(
		db,
		type_declarations,
		type_id,
		args,
		arg_count + 1,
		p_ret
	);
}

static int prototype_term_type_view_rebuild_from_source(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t view_type_id,
	uint32_t core,
	uint32_t source,
	uint32_t* p_ret
) {
	if (!db || !type_declarations || !p_ret || core >= db->term_count ||
		source >= db->term_count || view_type_id >= type_declarations->type_count) {
		return -1;
	}
	{
		uint32_t source_type_id;
		uint32_t args[16];
		uint32_t arg_count;
		if (prototype_term_type_instance_info(
				db,
				source,
				&source_type_id,
				args,
				&arg_count
			) == 0 &&
			source_type_id == view_type_id) {
			const struct prototype_type_declaration* declaration =
				&type_declarations->type_declarations[view_type_id];
			if (declaration->origin_source_carrier_term_id < db->term_count &&
				prototype_type_declaration_generated_identity_rule_for_source(
					db, declaration->origin_source_carrier_term_id
				) == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE) {
				for (uint32_t i = 0; i < arg_count; ++i) {
					if (prototype_term_normalize_complete_with_profile(
							db,
							type_declarations,
							NULL,
							PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
							args[i],
							&args[i]
						) != 0) {
						return -1;
					}
				}
			}
			return prototype_term_type_instance_make(
				db,
				type_declarations,
				view_type_id,
				args,
				arg_count,
				p_ret
			);
		}
	}
	struct prototype_qualified_name identity;
	identity.namespace_symbol_id =
		type_declarations->type_declarations[view_type_id].namespace_symbol_id;
	identity.name_symbol_id =
		type_declarations->type_declarations[view_type_id].name_symbol_id;
	return prototype_term_type_view(
		db, view_type_id, identity, core, source, p_ret
	);
}

int prototype_term_type_instance_is_saturated(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id
) {
	if (!db || !type_declarations) {
		return 0;
	}
	uint32_t type_id;
	uint32_t args[16];
	uint32_t arg_count;
	if (prototype_term_type_instance_info(db, term_id, &type_id, args, &arg_count) != 0 ||
		type_id >= type_declarations->type_count) {
		return 0;
	}
	const struct prototype_type_declaration* type =
		&type_declarations->type_declarations[type_id];
	return type->parameter_count <= UINT32_MAX - type->index_count &&
		arg_count == type->parameter_count + type->index_count;
}

int prototype_term_induction_hypothesis(
	struct prototype_term_db* db,
	uint32_t ih_scope_id,
	uint32_t argument,
	uint32_t* p_ret
) {
	if (!db || argument >= db->term_count || ih_scope_id >= db->ih_scope_count) {
		return -1;
	}

	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_INDUCTION_HYPOTHESIS;
	term.as.induction_hypothesis.ih_scope_id = ih_scope_id;
	term.as.induction_hypothesis.argument = argument;
	return add_term(db, term, p_ret);
}

int prototype_term_universe_var(struct prototype_term_db* db, uint32_t level_var, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_UNIVERSE_VAR;
	term.as.universe_var.level_var = level_var;
	return add_term(db, term, p_ret);
}

int prototype_term_primitive_text(struct prototype_term_db* db, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PRIMITIVE_TEXT;
	return add_term(db, term, p_ret);
}

int prototype_term_relation_type(
	struct prototype_term_db* db,
	uint32_t left_classifier,
	uint32_t right_classifier,
	uint32_t left_endpoint,
	uint32_t right_endpoint,
	uint32_t* p_ret
) {
	if (!db || left_classifier >= db->term_count ||
		right_classifier >= db->term_count || left_endpoint >= db->term_count ||
		right_endpoint >= db->term_count || !p_ret) {
		return -1;
	}
	struct prototype_term former;
	memset(&former, 0, sizeof(former));
	former.tag = PROTOTYPE_TERM_RELATION_TYPE_FORMER;
	uint32_t term;
	if (add_term(db, former, &term) != 0 ||
		prototype_term_app(db, term, left_classifier, &term) != 0 ||
		prototype_term_app(db, term, right_classifier, &term) != 0 ||
		prototype_term_app(db, term, left_endpoint, &term) != 0 ||
		prototype_term_app(db, term, right_endpoint, &term) != 0) {
		return -1;
	}
	*p_ret = term;
	return 0;
}

int prototype_term_relation_witness(
	struct prototype_term_db* db,
	uint32_t left_endpoint,
	uint32_t right_endpoint,
	uint32_t* p_ret
) {
	if (!db || left_endpoint >= db->term_count ||
		right_endpoint >= db->term_count || !p_ret) {
		return -1;
	}
	struct prototype_term former;
	memset(&former, 0, sizeof(former));
	former.tag = PROTOTYPE_TERM_RELATION_WITNESS_FORMER;
	uint32_t term;
	if (add_term(db, former, &term) != 0 ||
		prototype_term_app(db, term, left_endpoint, &term) != 0 ||
		prototype_term_app(db, term, right_endpoint, &term) != 0) {
		return -1;
	}
	*p_ret = term;
	return 0;
}

static int relation_spine(
	const struct prototype_term_db* db,
	uint32_t term_id,
	int former_tag,
	uint32_t* arguments,
	uint32_t argument_count
) {
	if (!db || !arguments || term_id >= db->term_count) {
		return -1;
	}
	uint32_t cursor = term_id;
	for (uint32_t i = argument_count; i > 0; --i) {
		if (cursor >= db->term_count ||
			db->terms[cursor].tag != PROTOTYPE_TERM_APP) {
			return -1;
		}
		arguments[i - 1] = db->terms[cursor].as.app.argument;
		cursor = db->terms[cursor].as.app.function;
	}
	return cursor < db->term_count && db->terms[cursor].tag == former_tag ?
		0 : -1;
}

int prototype_term_relation_type_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_left_classifier,
	uint32_t* p_right_classifier,
	uint32_t* p_left_endpoint,
	uint32_t* p_right_endpoint
) {
	if (!p_left_classifier || !p_right_classifier || !p_left_endpoint ||
		!p_right_endpoint) {
		return -1;
	}
	uint32_t arguments[4];
	if (relation_spine(
			db,
			term_id,
			PROTOTYPE_TERM_RELATION_TYPE_FORMER,
			arguments,
			4
		) != 0) {
		return -1;
	}
	*p_left_classifier = arguments[0];
	*p_right_classifier = arguments[1];
	*p_left_endpoint = arguments[2];
	*p_right_endpoint = arguments[3];
	return 0;
}

int prototype_term_relation_witness_info(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_left_endpoint,
	uint32_t* p_right_endpoint
) {
	if (!p_left_endpoint || !p_right_endpoint) {
		return -1;
	}
	uint32_t arguments[2];
	if (relation_spine(
			db,
			term_id,
			PROTOTYPE_TERM_RELATION_WITNESS_FORMER,
			arguments,
			2
		) != 0) {
		return -1;
	}
	*p_left_endpoint = arguments[0];
	*p_right_endpoint = arguments[1];
	return 0;
}

int prototype_term_text_literal(struct prototype_term_db* db, int text_symbol_id, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_TEXT_LITERAL;
	term.as.text_literal.text_symbol_id = text_symbol_id;
	return add_term(db, term, p_ret);
}

int prototype_term_primitive_int(struct prototype_term_db* db, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PRIMITIVE_INT;
	return add_term(db, term, p_ret);
}

int prototype_term_primitive_int64(struct prototype_term_db* db, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PRIMITIVE_INT64;
	return add_term(db, term, p_ret);
}

int prototype_term_int_literal(struct prototype_term_db* db, int64_t value, uint32_t* p_ret) {
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_INT_LITERAL;
	term.as.int_literal.value = value;
	return add_term(db, term, p_ret);
}

int prototype_term_effect_row_empty(
	struct prototype_term_db* db,
	uint32_t* p_ret
) {
	if (!db || !p_ret) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EFFECT_ROW_EMPTY;
	return add_term(db, term, p_ret);
}

int prototype_term_effect_row_var(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t* p_ret
) {
	if (!db || !p_ret || binding_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EFFECT_ROW_VAR;
	term.as.effect_row_var.binding_id = binding_id;
	return add_term(db, term, p_ret);
}

static int effect_row_is_closed_at_depth(
	const struct prototype_term_db* db,
	uint32_t row,
	uint32_t depth
) {
	if (!db || row >= db->term_count || depth > 512) {
		return -1;
	}
	const struct prototype_term* term = &db->terms[row];
	switch (term->tag) {
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 1;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			int left = effect_row_is_closed_at_depth(
				db, term->as.effect_row_union.left, depth + 1
			);
			int right = effect_row_is_closed_at_depth(
				db, term->as.effect_row_union.right, depth + 1
			);
			return left < 0 || right < 0 ? -1 : left && right;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			if (!prototype_term_effect_operation_declaration(
					term->as.effect_row_operation.operation_id
				)) {
				return -1;
			}
			return effect_row_is_closed_at_depth(
				db, term->as.effect_row_operation.latent_row, depth + 1
			);
		default:
			return -1;
	}
}

int prototype_term_effect_row_is_closed(
	const struct prototype_term_db* db,
	uint32_t row
) {
	return effect_row_is_closed_at_depth(db, row, 0);
}

int prototype_term_effect_row_purity(
	const struct prototype_term_db* db,
	uint32_t row
) {
	struct prototype_effect_row_normal_form normal;
	if (prototype_term_effect_row_normal_form(db, row, &normal) != 0) {
		return PROTOTYPE_EFFECT_ROW_PURITY_INVALID;
	}
	for (uint32_t i = 0; i < normal.atom_count; ++i) {
		const struct prototype_term* atom = &db->terms[normal.atoms[i]];
		if (atom->tag == PROTOTYPE_TERM_EFFECT_ROW_VAR ||
			prototype_term_effect_row_is_closed(db, normal.atoms[i]) == 0) {
			return PROTOTYPE_EFFECT_ROW_PURITY_UNRESOLVED;
		}
	}
	return normal.atom_count == 0 ?
		PROTOTYPE_EFFECT_ROW_PURITY_PURE :
		PROTOTYPE_EFFECT_ROW_PURITY_EFFECTFUL;
}

static int effect_row_normal_form_add_atom(
	struct prototype_effect_row_normal_form* normal,
	uint32_t atom
) {
	if (!normal) {
		return -1;
	}
	for (uint32_t i = 0; i < normal->atom_count; ++i) {
		if (normal->atoms[i] == atom) {
			return 0;
		}
	}
	if (normal->atom_count >= PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	normal->atoms[normal->atom_count++] = atom;
	return 0;
}

static int effect_row_normal_form_collect(
	const struct prototype_term_db* db,
	uint32_t row,
	struct prototype_effect_row_normal_form* normal,
	uint32_t depth
) {
	if (!db || !normal || row >= db->term_count || depth > 512) {
		return -1;
	}
	const struct prototype_term* term = &db->terms[row];
	switch (term->tag) {
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return effect_row_normal_form_collect(
					db, term->as.effect_row_union.left, normal, depth + 1
				) != 0 || effect_row_normal_form_collect(
					db, term->as.effect_row_union.right, normal, depth + 1
				) != 0 ? -1 : 0;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return effect_row_normal_form_add_atom(normal, row);
		default:
			return -1;
	}
}

int prototype_term_effect_row_normal_form(
	const struct prototype_term_db* db,
	uint32_t row,
	struct prototype_effect_row_normal_form* p_normal
) {
	if (!p_normal) {
		return -1;
	}
	memset(p_normal, 0, sizeof(*p_normal));
	return effect_row_normal_form_collect(db, row, p_normal, 0);
}

int prototype_term_effect_row_normal_form_includes(
	const struct prototype_effect_row_normal_form* superset,
	const struct prototype_effect_row_normal_form* subset
) {
	if (!superset || !subset) {
		return 0;
	}
	for (uint32_t i = 0; i < subset->atom_count; ++i) {
		int found = 0;
		for (uint32_t j = 0; j < superset->atom_count; ++j) {
			if (subset->atoms[i] == superset->atoms[j]) {
				found = 1;
				break;
			}
		}
		if (!found) {
			return 0;
		}
	}
	return 1;
}

static int effect_row_atom_compare(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right
) {
	struct prototype_term_canonical_key left_key;
	struct prototype_term_canonical_key right_key;
	if (prototype_term_canonical_key(db, left, &left_key) != 0 ||
		prototype_term_canonical_key(db, right, &right_key) != 0) {
		return left < right ? -1 : left != right;
	}
	if (left_key.hash != right_key.hash) {
		return left_key.hash < right_key.hash ? -1 : 1;
	}
	if (left_key.node_count != right_key.node_count) {
		return left_key.node_count < right_key.node_count ? -1 : 1;
	}
	return left < right ? -1 : left != right;
}

int prototype_term_effect_row_materialize_normal_form(
	struct prototype_term_db* db,
	const struct prototype_effect_row_normal_form* normal,
	uint32_t* p_ret
) {
	if (!db || !normal || !p_ret || normal->atom_count >
		PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
		return -1;
	}
	uint32_t atoms[PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY];
	memcpy(atoms, normal->atoms, normal->atom_count * sizeof(*atoms));
	for (uint32_t i = 1; i < normal->atom_count; ++i) {
		uint32_t atom = atoms[i];
		uint32_t j = i;
		while (j > 0 && effect_row_atom_compare(db, atom, atoms[j - 1]) < 0) {
			atoms[j] = atoms[j - 1];
			--j;
		}
		atoms[j] = atom;
	}
	if (normal->atom_count == 0) {
		return prototype_term_effect_row_empty(db, p_ret);
	}
	uint32_t result = atoms[0];
	if (normal->atom_count == 1) {
		*p_ret = result;
		return 0;
	}
	for (uint32_t i = 1; i < normal->atom_count; ++i) {
		struct prototype_term term;
		memset(&term, 0, sizeof(term));
		term.tag = PROTOTYPE_TERM_EFFECT_ROW_UNION;
		term.as.effect_row_union.left = result;
		term.as.effect_row_union.right = atoms[i];
		if (add_term(db, term, &result) != 0) {
			return -1;
		}
	}
	*p_ret = result;
	return 0;
}

int prototype_term_effect_row_union(
	struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	uint32_t* p_ret
) {
	if (!db || !p_ret || left >= db->term_count || right >= db->term_count) {
		return -1;
	}
	struct prototype_effect_row_normal_form left_normal;
	struct prototype_effect_row_normal_form right_normal;
	if (prototype_term_effect_row_normal_form(db, left, &left_normal) != 0 ||
		prototype_term_effect_row_normal_form(db, right, &right_normal) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < right_normal.atom_count; ++i) {
		if (effect_row_normal_form_add_atom(
				&left_normal, right_normal.atoms[i]
			) != 0) {
			return -1;
		}
	}
	return prototype_term_effect_row_materialize_normal_form(
		db, &left_normal, p_ret
	);
}

int prototype_term_effect_row_residual(
	struct prototype_term_db* db,
	uint32_t row,
	uint32_t handled_row,
	uint32_t* p_residual
) {
	if (!db || !p_residual || row >= db->term_count ||
		handled_row >= db->term_count) {
		return -1;
	}
	struct prototype_effect_row_normal_form source;
	struct prototype_effect_row_normal_form handled;
	struct prototype_effect_row_normal_form residual;
	if (prototype_term_effect_row_normal_form(db, row, &source) != 0 ||
		prototype_term_effect_row_normal_form(db, handled_row, &handled) != 0) {
		return -1;
	}
	memset(&residual, 0, sizeof(residual));
	for (uint32_t i = 0; i < handled.atom_count; ++i) {
		if (handled.atoms[i] >= db->term_count ||
			db->terms[handled.atoms[i]].tag !=
				PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
			return 1;
		}
	}
	for (uint32_t i = 0; i < source.atom_count; ++i) {
		uint32_t atom = source.atoms[i];
		int remove = 0;
		if (atom < db->term_count && db->terms[atom].tag ==
			PROTOTYPE_TERM_EFFECT_ROW_OPERATION) {
			int operation_id =
				db->terms[atom].as.effect_row_operation.operation_id;
			for (uint32_t j = 0; j < handled.atom_count; ++j) {
				remove = remove || db->terms[
					handled.atoms[j]
				].as.effect_row_operation.operation_id == operation_id;
			}
		}
		if (!remove && effect_row_normal_form_add_atom(&residual, atom) != 0) {
			return -1;
		}
	}
	return prototype_term_effect_row_materialize_normal_form(
		db, &residual, p_residual
	);
}

int prototype_term_effect_row_forall(
	struct prototype_term_db* db,
	uint32_t binding_id,
	uint32_t body,
	uint32_t* p_ret
) {
	if (!db || !p_ret || binding_id == PROTOTYPE_INVALID_ID || body >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EFFECT_ROW_FORALL;
	term.as.effect_row_forall.binding_id = binding_id;
	term.as.effect_row_forall.body = body;
	return add_term(db, term, p_ret);
}

int prototype_term_effect_row_forall_parts(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_binder_id,
	uint32_t* p_body
) {
	if (!db || !p_binder_id || !p_body || term_id >= db->term_count ||
		db->terms[term_id].tag != PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		return -1;
	}
	*p_binder_id = db->terms[term_id].as.effect_row_forall.binding_id;
	*p_body = db->terms[term_id].as.effect_row_forall.body;
	return 0;
}

int prototype_term_effect_row_operation(
	struct prototype_term_db* db,
	int operation_id,
	uint32_t latent_row,
	uint32_t* p_ret
) {
	if (!db || !p_ret || latent_row >= db->term_count ||
		!prototype_term_effect_operation_declaration(operation_id)) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EFFECT_ROW_OPERATION;
	term.as.effect_row_operation.operation_id = operation_id;
	term.as.effect_row_operation.latent_row = latent_row;
	return add_term(db, term, p_ret);
}

int prototype_term_computation_type(
	struct prototype_term_db* db,
	uint32_t label,
	uint32_t result,
	uint32_t* p_ret
) {
	if (!db || !p_ret || label >= db->term_count || result >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_COMPUTATION_TYPE;
	term.as.computation_type.label = label;
	term.as.computation_type.result = result;
	return add_term(db, term, p_ret);
}

int prototype_term_thunk_type(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t* p_ret
) {
	if (!db || !p_ret || computation >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_THUNK_TYPE;
	term.as.thunk_type.computation = computation;
	return add_term(db, term, p_ret);
}

int prototype_term_return(
	struct prototype_term_db* db,
	uint32_t value,
	uint32_t* p_ret
) {
	if (!db || !p_ret || value >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_RETURN;
	term.as.return_term.value = value;
	return add_term(db, term, p_ret);
}

int prototype_term_thunk(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t* p_ret
) {
	if (!db || !p_ret || computation >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_THUNK;
	term.as.thunk.computation = computation;
	return add_term(db, term, p_ret);
}

int prototype_term_force(
	struct prototype_term_db* db,
	uint32_t value,
	uint32_t* p_ret
) {
	if (!db || !p_ret || value >= db->term_count) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_FORCE;
	term.as.force.value = value;
	return add_term(db, term, p_ret);
}

int prototype_term_operation_request(
	struct prototype_term_db* db,
	uint32_t operation,
	uint32_t argument,
	uint32_t continuation,
	uint32_t* p_ret
) {
	if (!db || !p_ret || operation >= db->term_count || argument >= db->term_count ||
		continuation >= db->term_count ||
		db->terms[continuation].tag != PROTOTYPE_TERM_THUNK ||
		db->terms[continuation].as.thunk.computation >= db->term_count ||
		db->terms[db->terms[continuation].as.thunk.computation].tag !=
			PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	uint32_t head = operation;
	while (head < db->term_count && db->terms[head].tag == PROTOTYPE_TERM_APP) {
		head = db->terms[head].as.app.function;
	}
	if (head >= db->term_count || db->terms[head].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_OPERATION_REQUEST;
	term.as.operation_request.operation = operation;
	term.as.operation_request.argument = argument;
	term.as.operation_request.continuation = continuation;
	return add_term(db, term, p_ret);
}

int prototype_term_computation_fold(
	struct prototype_term_db* db,
	uint32_t computation,
	uint32_t return_clause,
	const struct prototype_computation_fold_clause* clauses,
	uint32_t clause_count,
	uint32_t* p_ret
) {
	if (!db || !p_ret || computation >= db->term_count ||
		return_clause >= db->term_count || db->terms[return_clause].tag != PROTOTYPE_TERM_LAMBDA ||
		(clause_count > 0 && !clauses) ||
		db->computation_fold_clause_count + clause_count > PROTOTYPE_COMPUTATION_FOLD_CLAUSE_CAPACITY) {
		return -1;
	}
	for (uint32_t i = 0; i < clause_count; ++i) {
		if (clauses[i].operation >= db->term_count || clauses[i].body >= db->term_count ||
			db->terms[clauses[i].operation].tag != PROTOTYPE_TERM_EFFECT_OPERATION ||
			db->terms[clauses[i].body].tag != PROTOTYPE_TERM_LAMBDA) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (db->terms[clauses[i].operation].as.effect_operation.operation_id ==
				db->terms[clauses[j].operation].as.effect_operation.operation_id) {
				return -1;
			}
		}
	}

	size_t saved_clause_count = db->computation_fold_clause_count;
	size_t saved_term_count = db->term_count;
	uint32_t first_clause = (uint32_t)saved_clause_count;
	for (uint32_t i = 0; i < clause_count; ++i) {
		db->computation_fold_clauses[db->computation_fold_clause_count].operation = clauses[i].operation;
		db->computation_fold_clauses[db->computation_fold_clause_count].body = clauses[i].body;
		db->computation_fold_clause_count++;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_COMPUTATION_FOLD;
	term.as.computation_fold.computation = computation;
	term.as.computation_fold.return_clause = return_clause;
	term.as.computation_fold.first_clause = first_clause;
	term.as.computation_fold.clause_count = clause_count;
	if (add_term(db, term, p_ret) != 0) {
		db->computation_fold_clause_count = saved_clause_count;
		return -1;
	}
	if (*p_ret < saved_term_count) {
		db->computation_fold_clause_count = saved_clause_count;
	}
	return 0;
}

int prototype_term_make_host_type(
	struct prototype_term_db* db,
	int type_id,
	uint32_t* p_ret
) {
	switch (type_id) {
		case PROTOTYPE_HOST_TYPE_TEXT:
			return prototype_term_primitive_text(db, p_ret);
		case PROTOTYPE_HOST_TYPE_INT32:
			return prototype_term_primitive_int(db, p_ret);
		case PROTOTYPE_HOST_TYPE_INT64:
			return prototype_term_primitive_int64(db, p_ret);
		default:
			return -1;
	}
}

int prototype_term_external_ref(
	struct prototype_term_db* db,
	struct prototype_qualified_name name,
	uint32_t* p_ret
) {
	if (name.name_symbol_id < 0) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EXTERNAL_REF;
	term.as.external_ref.name = name;
	return add_term(db, term, p_ret);
}

int prototype_term_pure_primitive(
	struct prototype_term_db* db,
	int primitive_id,
	int type_symbol_id,
	uint32_t* p_ret
) {
	if (!prototype_term_pure_primitive_declaration(primitive_id)) {
		return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_PURE_PRIMITIVE;
	term.as.pure_primitive.primitive_id = primitive_id;
	term.as.pure_primitive.type_symbol_id = type_symbol_id;
	return add_term(db, term, p_ret);
}

int prototype_term_effect_operation(
	struct prototype_term_db* db,
	int operation_id,
	uint32_t* p_ret
) {
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	if (!db || !p_ret || !declaration) {
		return -1;
	}
	uint32_t text;
	uint32_t argument;
	uint32_t empty_effects;
	uint32_t result_effects;
	uint32_t result;
	uint32_t classifier;
	if (prototype_term_make_host_type(db, PROTOTYPE_HOST_TYPE_TEXT, &text) != 0 ||
		prototype_term_effect_row_empty(db, &empty_effects
		) != 0 || prototype_term_effect_row_operation(
			db, operation_id, empty_effects, &result_effects
		) != 0 ||
		prototype_term_computation_type(db, result_effects, text, &result) != 0) {
		return -1;
	}
	switch (declaration->classifier_schema) {
		case PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_TEXT_TO_TEXT:
			argument = text;
			if (prototype_term_pi(db, argument, result, &classifier) != 0) {
				return -1;
			}
			break;
		case PROTOTYPE_EFFECT_OPERATION_CLASSIFIER_THUNK_TEXT_TO_TEXT: {
			uint32_t row_binder = prototype_term_new_binding(db);
			uint32_t row;
			uint32_t computation;
			if (row_binder == PROTOTYPE_INVALID_ID ||
				prototype_term_effect_row_var(db, row_binder, &row) != 0 ||
				prototype_term_computation_type(db, row, text, &computation) != 0 ||
				prototype_term_thunk_type(db, computation, &argument) != 0 ||
				prototype_term_pi(db, argument, result, &classifier) != 0 ||
				prototype_term_effect_row_forall(db, row_binder, classifier, &classifier) != 0) {
				return -1;
			}
			break;
		}
		default:
			return -1;
	}
	struct prototype_term term;
	memset(&term, 0, sizeof(term));
	term.tag = PROTOTYPE_TERM_EFFECT_OPERATION;
	term.as.effect_operation.operation_id = operation_id;
	term.as.effect_operation.classifier = classifier;
	return add_term(db, term, p_ret);
}

int prototype_term_effect_operation_identity(
	const struct prototype_term_db* db,
	uint32_t term_id,
	int* p_operation_id
) {
	if (!db || !p_operation_id || term_id >= db->term_count) {
		return -1;
	}
	while (term_id < db->term_count && db->terms[term_id].tag == PROTOTYPE_TERM_APP) {
		term_id = db->terms[term_id].as.app.function;
	}
	if (term_id >= db->term_count ||
		db->terms[term_id].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	*p_operation_id = db->terms[term_id].as.effect_operation.operation_id;
	return 0;
}

int prototype_term_effect_operation_classifier_has_suspended_argument(
	const struct prototype_term_db* db,
	uint32_t classifier,
	int* p_has_suspended_argument
) {
	if (!db || !p_has_suspended_argument || classifier >= db->term_count) {
		return -1;
	}
	uint32_t body = classifier;
	if (db->terms[body].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL) {
		body = db->terms[body].as.effect_row_forall.body;
	}
	if (body >= db->term_count || db->terms[body].tag != PROTOTYPE_TERM_PI ||
		db->terms[body].as.pi.domain >= db->term_count) {
		return -1;
	}
	*p_has_suspended_argument = db->terms[
		db->terms[body].as.pi.domain
	].tag == PROTOTYPE_TERM_THUNK_TYPE;
	return 0;
}
