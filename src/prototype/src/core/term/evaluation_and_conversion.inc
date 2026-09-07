static int resolve_external_ref_term(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_qualified_name symbol_id,
	uint32_t replacement,
	uint32_t* p_ret
) {
	if (!db || !p_ret || term_id >= db->term_count || replacement >= db->term_count) {
		return -1;
	}

	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_EXTERNAL_REF:
			*p_ret = qualified_names_equal(term->as.external_ref.name, symbol_id) ? replacement : term_id;
			return 0;
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			uint32_t argument;
			if (resolve_external_ref_term(db, term->as.app.function, symbol_id, replacement, &function) != 0 ||
				resolve_external_ref_term(db, term->as.app.argument, symbol_id, replacement, &argument) != 0) {
				return -1;
			}
			if (function == term->as.app.function && argument == term->as.app.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_app(db, function, argument, p_ret);
		}
		case PROTOTYPE_TERM_PI: {
			uint32_t domain;
			uint32_t codomain_family;
			if (resolve_external_ref_term(db, term->as.pi.domain, symbol_id, replacement, &domain) != 0 ||
				resolve_external_ref_term(db, term->as.pi.codomain_family, symbol_id, replacement, &codomain_family) != 0) {
				return -1;
			}
			if (domain == term->as.pi.domain && codomain_family == term->as.pi.codomain_family) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_pi_family(db, domain, codomain_family, p_ret);
		}
		case PROTOTYPE_TERM_TYPE_VIEW: {
			uint32_t core;
			uint32_t source;
			if (resolve_external_ref_term(
					db,
					term->as.type_view.core,
					symbol_id,
					replacement,
					&core
				) != 0 ||
				resolve_external_ref_term(
					db,
					term->as.type_view.source,
					symbol_id,
					replacement,
					&source
				) != 0) {
				return -1;
			}
			if (core == term->as.type_view.core &&
				source == term->as.type_view.source) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_type_view(
				db,
				term->as.type_view.identity,
				core,
				source,
				p_ret
			);
		}
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t body;
			if (resolve_external_ref_term(db, term->as.lambda.body, symbol_id, replacement, &body) != 0) {
				return -1;
			}
			if (body == term->as.lambda.body) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_lambda_with_tag(db, term->tag, term->as.lambda.binding_id, body, p_ret);
		}
		case PROTOTYPE_TERM_MATCH: {
			uint32_t scrutinee;
			if (resolve_external_ref_term(db, term->as.match.scrutinee, symbol_id, replacement, &scrutinee) != 0) {
				return -1;
			}

			struct prototype_match_case_input case_inputs[64];
			struct prototype_case_binder binder_storage[256];
			uint32_t binder_cursor = 0;
			int changed = scrutinee != term->as.match.scrutinee;
			if (term->as.match.case_count > 64) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&db->cases[term->as.match.first_case + i];
				uint32_t body;
				if (binder_cursor + old_case->binder_count > 256 ||
					resolve_external_ref_term(db, old_case->body, symbol_id, replacement, &body) != 0) {
					return -1;
				}
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					binder_storage[binder_cursor + j] =
						db->case_binders[old_case->first_binder + j];
				}
				if (body != old_case->body) {
					changed = 1;
				}
				case_inputs[i].case_label_symbol_id =
					db->case_label_symbols[term->as.match.first_case + i];
				case_inputs[i].constructor_owner = old_case->constructor_owner;
				case_inputs[i].constructor_id = old_case->constructor_id;
				case_inputs[i].binders = &binder_storage[binder_cursor];
				case_inputs[i].binder_count = old_case->binder_count;
				case_inputs[i].body = body;
				binder_cursor += old_case->binder_count;
			}
			if (!changed) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_match_with_ih_scope(
				db,
				scrutinee,
				case_inputs,
				term->as.match.case_count,
				term->as.match.ih_scope_id,
				p_ret
			);
		}
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t owner;
			if (resolve_external_ref_term(db, term->as.constructor.owner, symbol_id, replacement, &owner) != 0) {
				return -1;
			}
			if (owner == term->as.constructor.owner) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_constructor(db, owner, term->as.constructor.constructor_id, p_ret);
		}
		case PROTOTYPE_TERM_DIMENSION_ACTION: {
			uint32_t source;
			if (resolve_external_ref_term(
					db,
					term->as.dimension_action.source,
					symbol_id,
					replacement,
					&source
				) != 0) {
				return -1;
			}
			if (source == term->as.dimension_action.source) {
				*p_ret = term_id;
				return 0;
			}
			struct prototype_term acted = *term;
			acted.as.dimension_action.source = source;
			return add_term(db, acted, p_ret);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			uint32_t left;
			uint32_t right;
			if (resolve_external_ref_term(
					db, term->as.effect_row_union.left, symbol_id, replacement, &left
				) != 0 || resolve_external_ref_term(
					db, term->as.effect_row_union.right, symbol_id, replacement, &right
				) != 0) {
				return -1;
			}
			return left == term->as.effect_row_union.left &&
				right == term->as.effect_row_union.right ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_union(db, left, right, p_ret);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			uint32_t body;
			if (resolve_external_ref_term(
					db, term->as.effect_row_forall.body, symbol_id, replacement, &body
				) != 0) {
				return -1;
			}
			return body == term->as.effect_row_forall.body ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_forall(
					db, term->as.effect_row_forall.binding_id, body, p_ret
				);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION: {
			uint32_t latent_row;
			if (resolve_external_ref_term(
					db,
					term->as.effect_row_operation.latent_row,
					symbol_id,
					replacement,
					&latent_row
				) != 0) {
				return -1;
			}
			return latent_row == term->as.effect_row_operation.latent_row ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_operation(
					db, term->as.effect_row_operation.operation_id, latent_row, p_ret
				);
		}
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
				uint32_t argument;
				if (resolve_external_ref_term(
					db,
					term->as.induction_hypothesis.argument,
					symbol_id,
					replacement,
					&argument
				) != 0) {
				return -1;
			}
			if (argument == term->as.induction_hypothesis.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_induction_hypothesis(
				db,
				term->as.induction_hypothesis.ih_scope_id,
				argument,
				p_ret
				);
			}
			case PROTOTYPE_TERM_COMPUTATION_TYPE: {
				uint32_t label;
				uint32_t result;
				if (resolve_external_ref_term(
						db,
						term->as.computation_type.label,
						symbol_id,
						replacement,
						&label
					) != 0 ||
					resolve_external_ref_term(
						db,
						term->as.computation_type.result,
						symbol_id,
						replacement,
						&result
					) != 0) {
					return -1;
				}
				if (label == term->as.computation_type.label &&
					result == term->as.computation_type.result) {
					*p_ret = term_id;
					return 0;
				}
				return prototype_term_computation_type(
					db, label, result, term->as.computation_type.totality, p_ret
				);
			}
			case PROTOTYPE_TERM_THUNK_TYPE: {
				uint32_t computation;
				if (resolve_external_ref_term(
						db, term->as.thunk_type.computation, symbol_id, replacement, &computation
					) != 0) {
					return -1;
				}
				return computation == term->as.thunk_type.computation ?
					(*p_ret = term_id, 0) : prototype_term_thunk_type(db, computation, p_ret);
			}
			case PROTOTYPE_TERM_RETURN: {
				uint32_t value;
				if (resolve_external_ref_term(
						db, term->as.return_term.value, symbol_id, replacement, &value
					) != 0) {
					return -1;
				}
				return value == term->as.return_term.value ?
					(*p_ret = term_id, 0) : prototype_term_return(db, value, p_ret);
			}
			case PROTOTYPE_TERM_THUNK: {
				uint32_t computation;
				if (resolve_external_ref_term(
						db, term->as.thunk.computation, symbol_id, replacement, &computation
					) != 0) {
					return -1;
				}
				return computation == term->as.thunk.computation ?
					(*p_ret = term_id, 0) : prototype_term_thunk(db, computation, p_ret);
			}
			case PROTOTYPE_TERM_FORCE: {
				uint32_t value;
				if (resolve_external_ref_term(
						db, term->as.force.value, symbol_id, replacement, &value
					) != 0) {
					return -1;
				}
				return value == term->as.force.value ?
					(*p_ret = term_id, 0) : prototype_term_force(db, value, p_ret);
			}
			case PROTOTYPE_TERM_COMPUTATION_FOLD: {
				uint32_t computation;
				uint32_t return_clause;
				uint32_t clause_count = term->as.computation_fold.clause_count;
				struct prototype_computation_fold_clause* clauses =
					calloc(clause_count, sizeof(*clauses));
				if (clause_count > 0 && !clauses) {
					return -1;
				}
				if (resolve_external_ref_term(
						db, term->as.computation_fold.computation, symbol_id, replacement, &computation
					) != 0 || resolve_external_ref_term(
						db, term->as.computation_fold.return_clause, symbol_id, replacement, &return_clause
					) != 0) {
					free(clauses);
					return -1;
				}
				int unchanged = computation == term->as.computation_fold.computation &&
					return_clause == term->as.computation_fold.return_clause;
				for (uint32_t i = 0; i < clause_count; ++i) {
					const struct prototype_computation_fold_clause* clause =
						&db->computation_fold_clauses[term->as.computation_fold.first_clause + i];
					if (resolve_external_ref_term(
							db, clause->operation, symbol_id, replacement, &clauses[i].operation
						) != 0 || resolve_external_ref_term(
							db, clause->body, symbol_id, replacement, &clauses[i].body
						) != 0) {
						free(clauses);
						return -1;
					}
					unchanged = unchanged && clauses[i].operation == clause->operation &&
						clauses[i].body == clause->body;
				}
				int status = unchanged ? (*p_ret = term_id, 0) : prototype_term_computation_fold(
					db, computation, return_clause, clauses, clause_count, p_ret
				);
				free(clauses);
				return status;
			}
			case PROTOTYPE_TERM_OPERATION_REQUEST: {
				uint32_t operation;
				uint32_t argument;
				uint32_t continuation;
				if (resolve_external_ref_term(
						db, term->as.operation_request.operation, symbol_id, replacement, &operation
					) != 0 || resolve_external_ref_term(
						db, term->as.operation_request.argument, symbol_id, replacement, &argument
					) != 0 || resolve_external_ref_term(
						db, term->as.operation_request.continuation, symbol_id, replacement, &continuation
					) != 0) {
					return -1;
				}
				return operation == term->as.operation_request.operation &&
					argument == term->as.operation_request.argument &&
					continuation == term->as.operation_request.continuation ?
					(*p_ret = term_id, 0) : prototype_term_operation_request(
						db, operation, argument, continuation, p_ret
					);
			}
			default:
				*p_ret = term_id;
				return 0;
		}
}

int prototype_term_resolve_external_ref(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_qualified_name name,
	uint32_t replacement,
	uint32_t* p_ret
) {
	return resolve_external_ref_term(db, term_id, name, replacement, p_ret);
}

static void decompose_app(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t* p_head,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	uint32_t reversed_args[64];
	uint32_t reversed_count = 0;
	uint32_t current = term_id;

	while (current < db->term_count &&
		db->terms[current].tag == PROTOTYPE_TERM_APP &&
		reversed_count < 64) {
		reversed_args[reversed_count++] = db->terms[current].as.app.argument;
		current = db->terms[current].as.app.function;
	}

	*p_head = current;
	*p_arg_count = reversed_count;
	for (uint32_t i = 0; i < reversed_count; ++i) {
		args[i] = reversed_args[reversed_count - i - 1];
	}
}

static int constructor_owner_type_former(
	const struct prototype_term_db* terms,
	uint32_t owner,
	const struct prototype_term** p_type_former
) {
	if (!terms || !p_type_former || owner >= terms->term_count) {
		return -1;
	}
	uint32_t current = owner;
	while (current < terms->term_count && terms->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW) {
		current = terms->terms[current].as.type_view.core;
	}
	while (current < terms->term_count && terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count || terms->terms[current].tag != PROTOTYPE_TERM_TYPE_FORMER) {
		return -1;
	}
	*p_type_former = &terms->terms[current];
	return 0;
}

static int constructor_owner_representation_id(
	const struct prototype_term_db* terms,
	uint32_t owner,
	uint32_t* p_representation_id
) {
	const struct prototype_term* type_former;
	if (!p_representation_id || constructor_owner_type_former(
			terms, owner, &type_former
		) != 0) {
		return -1;
	}
	*p_representation_id = type_former->as.type_former.representation_id;
	return 0;
}

static int constructor_member_matches(
	struct prototype_term_db* terms,
	uint32_t term_id,
	const struct prototype_match_case* match_case
) {
	if (!terms || !match_case || term_id >= terms->term_count) {
		return 0;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
		return 0;
	}
	if (term->as.constructor.constructor_id != match_case->constructor_id) {
		return 0;
	}
	if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (term->as.constructor.owner == match_case->constructor_owner) {
		return 1;
	}
	uint32_t term_representation_id;
	uint32_t case_representation_id;
	/*
	 * Constructor matching belongs to the erased computation graph. A source
	 * TYPE_VIEW may remain inside a substituted type application, so structural
	 * comparison of owner terms would make List Nat fail to match itself after
	 * beta substitution. The representation and ordinal are the runtime tag.
	 */
	return constructor_owner_representation_id(
		terms,
		term->as.constructor.owner,
		&term_representation_id
	) == 0 && constructor_owner_representation_id(
		terms,
		match_case->constructor_owner,
		&case_representation_id
	) == 0 && term_representation_id == case_representation_id;
}

static int term_is_constructor_like(const struct prototype_term* term) {
	if (term->tag != PROTOTYPE_TERM_CONSTRUCTOR) {
		return -1;
	}
	return 0;
}

static int lookup_transparent_definition(
	const struct prototype_term_definition_env* definitions,
	struct prototype_qualified_name name,
	uint32_t* p_term
) {
	if (!definitions || !p_term || name.name_symbol_id < 0) {
		return 0;
	}
	for (size_t i = 0; i < definitions->definition_count; ++i) {
		const struct prototype_term_definition* definition = &definitions->definitions[i];
		if (qualified_names_equal(definition->name, name) &&
			definition->transparency == PROTOTYPE_TERM_DEFINITION_TRANSPARENT) {
			*p_term = definition->term;
			return 1;
		}
	}
	return 0;
}

static int evaluate_steps(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
);

static int reduce_host_pure_primitive_step(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
);

static int dispatch_operation_request_step(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
);

/* Request continuations are ordinary CBPV function values.  Keeping this
 * construction here prevents host dispatch and recursive computation-fold propagation
 * dispatch from each inventing a separate continuation application rule. */
static int operation_request_resume(
	struct prototype_term_db* db,
	uint32_t continuation,
	uint32_t result,
	uint32_t* p_ret
) {
	uint32_t forced;
	if (!db || !p_ret || continuation >= db->term_count || result >= db->term_count ||
		db->terms[continuation].tag != PROTOTYPE_TERM_THUNK ||
		prototype_term_force(db, continuation, &forced) != 0 ||
		prototype_term_app(db, forced, result, p_ret) != 0) {
		return -1;
	}
	return 0;
}

static int computation_fold_rebuild(
	struct prototype_term_db* db,
	uint32_t fold_id,
	uint32_t computation,
	uint32_t* p_ret
) {
	if (!db || !p_ret || fold_id >= db->term_count || computation >= db->term_count ||
		db->terms[fold_id].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
		return -1;
	}
	const struct prototype_term* fold = &db->terms[fold_id];
	const struct prototype_computation_fold_clause* clauses =
		&db->computation_fold_clauses[fold->as.computation_fold.first_clause];
	return prototype_term_computation_fold(
		db,
		computation,
		fold->as.computation_fold.return_clause,
		clauses,
		fold->as.computation_fold.clause_count,
		p_ret
	);
}

static int computation_fold_continuation(
	struct prototype_term_db* db,
	uint32_t fold_id,
	uint32_t request_continuation,
	uint32_t* p_ret
) {
	uint32_t binding_id = prototype_term_new_binding(db);
	uint32_t result_var;
	uint32_t resumed;
	uint32_t folded;
	uint32_t lambda;
	if (!db || !p_ret || binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, binding_id, &result_var) != 0 ||
		operation_request_resume(db, request_continuation, result_var, &resumed) != 0 ||
		computation_fold_rebuild(db, fold_id, resumed, &folded) != 0 ||
		prototype_term_lambda(db, binding_id, folded, &lambda) != 0 ||
		prototype_term_thunk(db, lambda, p_ret) != 0) {
		return -1;
	}
	return 0;
}

static int computation_fold_find_clause(
	const struct prototype_term_db* db,
	uint32_t fold_id,
	uint32_t operation,
	uint32_t* p_body
) {
	if (!db || !p_body || fold_id >= db->term_count || operation >= db->term_count ||
		db->terms[fold_id].tag != PROTOTYPE_TERM_COMPUTATION_FOLD) {
		return -1;
	}
	const struct prototype_term* fold = &db->terms[fold_id];
	for (uint32_t i = 0; i < fold->as.computation_fold.clause_count; ++i) {
		const struct prototype_computation_fold_clause* clause =
			&db->computation_fold_clauses[fold->as.computation_fold.first_clause + i];
		int clause_identity;
		int request_identity;
		if (prototype_term_effect_operation_identity(
				db, clause->operation, &clause_identity
			) != 0 || prototype_term_effect_operation_identity(
				db, operation, &request_identity
			) != 0) {
			return -1;
		}
		if (clause_identity == request_identity) {
			*p_body = clause->body;
			return 0;
		}
	}
	*p_body = PROTOTYPE_INVALID_ID;
	return 0;
}

static int match_rebuild_scrutinee(
	struct prototype_term_db* db,
	uint32_t match_id,
	uint32_t scrutinee,
	uint32_t* p_ret
) {
	if (!db || !p_ret || match_id >= db->term_count ||
		scrutinee >= db->term_count ||
		db->terms[match_id].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	const struct prototype_term match = db->terms[match_id];
	if (scrutinee == match.as.match.scrutinee) {
		*p_ret = match_id;
		return 0;
	}
	if (match.as.match.case_count > 64) {
		return -1;
	}
	struct prototype_match_case_input cases[64];
	struct prototype_case_binder binders[256];
	uint32_t binder_cursor = 0;
	for (uint32_t i = 0; i < match.as.match.case_count; ++i) {
		const struct prototype_match_case* source =
			&db->cases[match.as.match.first_case + i];
		if (binder_cursor + source->binder_count > 256) {
			return -1;
		}
		for (uint32_t j = 0; j < source->binder_count; ++j) {
			binders[binder_cursor + j] =
				db->case_binders[source->first_binder + j];
		}
		cases[i] = (struct prototype_match_case_input) {
			.case_label_symbol_id =
				db->case_label_symbols[match.as.match.first_case + i],
			.constructor_owner = source->constructor_owner,
			.constructor_id = source->constructor_id,
			.binders = &binders[binder_cursor],
			.binder_count = source->binder_count,
			.body = source->body
		};
		binder_cursor += source->binder_count;
	}
	return prototype_term_match_with_ih_scope(
		db, scrutinee, cases, match.as.match.case_count,
		match.as.match.ih_scope_id, p_ret
	);
}

static int normalization_rebuild_on_pause(
	struct prototype_term_db* db,
	uint32_t parent,
	uint32_t child,
	int parent_tag,
	uint32_t* p_ret
) {
	if (!db || !p_ret || parent >= db->term_count || child >= db->term_count) {
		return -1;
	}
	const struct prototype_term source = db->terms[parent];
	switch (parent_tag) {
		case PROTOTYPE_TERM_APP:
			return prototype_term_app(
				db, child, source.as.app.argument, p_ret
			);
		case PROTOTYPE_TERM_MATCH:
			return match_rebuild_scrutinee(db, parent, child, p_ret);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return prototype_term_induction_hypothesis(
				db, source.as.induction_hypothesis.ih_scope_id, child, p_ret
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			return computation_fold_rebuild(db, parent, child, p_ret);
		case PROTOTYPE_TERM_FORCE:
			return prototype_term_force(db, child, p_ret);
		default:
			return -1;
	}
}

static int conversion_equal_at_depth(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	struct term_scope_env* scope,
	uint32_t left,
	uint32_t right,
	int* p_equal,
	uint32_t depth
);

static void normalization_mark_status(
	struct prototype_term_reduction_options options,
	int status,
	int reason
);

static int conversion_scope_push_binder(
	struct prototype_term_reduction_options options,
	struct term_scope_env* scope,
	uint32_t left_binder,
	uint32_t right_binder
) {
	if (term_scope_env_push_binder(scope, left_binder, right_binder) == 0) {
		return 0;
	}
	normalization_mark_status(
		options,
		PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED,
		PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT
	);
	return -1;
}

static int conversion_scope_push_frame(
	struct prototype_term_reduction_options options,
	struct term_scope_env* scope,
	uint32_t left_frame,
	uint32_t right_frame
) {
	if (term_scope_env_push_frame(scope, left_frame, right_frame) == 0) {
		return 0;
	}
	normalization_mark_status(
		options,
		PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED,
		PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT
	);
	return -1;
}

static int conversion_scope_enter_match_frames(
	struct prototype_term_reduction_options options,
	struct term_scope_env* scope,
	uint32_t left_frame,
	uint32_t right_frame
) {
	if (left_frame == PROTOTYPE_INVALID_ID &&
		right_frame == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (left_frame == PROTOTYPE_INVALID_ID) {
		if (term_scope_env_mark_unmatched_frame(scope, right_frame, 0) == 0) {
			return 0;
		}
	} else if (right_frame == PROTOTYPE_INVALID_ID) {
		if (term_scope_env_mark_unmatched_frame(scope, left_frame, 1) == 0) {
			return 0;
		}
	} else {
		return conversion_scope_push_frame(
			options, scope, left_frame, right_frame
		);
	}
	normalization_mark_status(
		options,
		PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED,
		PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT
	);
	return -1;
}

static int conversion_equal_match_case_bodies(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	struct term_scope_env* scope,
	const struct prototype_match_case* left_case,
	const struct prototype_match_case* right_case,
	uint32_t left_body,
	uint32_t right_body,
	int* p_equal,
	uint32_t depth
) {
	if (!db || !scope || !left_case || !right_case || !p_equal ||
		left_case->binder_count != right_case->binder_count) {
		return -1;
	}

	struct term_scope_env body_scope = *scope;
	for (uint32_t i = 0; i < left_case->binder_count; ++i) {
		uint32_t left_binder_index = left_case->first_binder + i;
		uint32_t right_binder_index = right_case->first_binder + i;
		if (left_binder_index >= db->case_binder_count ||
			right_binder_index >= db->case_binder_count) {
			return -1;
		}
		const struct prototype_case_binder* left_binder =
			&db->case_binders[left_binder_index];
		const struct prototype_case_binder* right_binder =
			&db->case_binders[right_binder_index];
		if (left_binder->is_recursive != right_binder->is_recursive) {
			*p_equal = 0;
			return 0;
		}
		if (conversion_scope_push_binder(
				options, &body_scope, left_binder->binding_id, right_binder->binding_id
			) != 0) {
			return -1;
		}
	}

	return conversion_equal_at_depth(
		db,
		definitions,
		options,
		&body_scope,
		left_body,
		right_body,
		p_equal,
		depth + 1
	);
}

/* A dependent computation family has two graph presentations while its
 * scrutinee is neutral:
 *
 *   Comp(E, match x { c_i -> A_i })
 *   match x { c_i -> Comp(E, A_i) }
 *
 * They are the same pointwise family. Keep this commuting conversion in the
 * conversion kernel so Lambda checking and every other client share it. */
static int conversion_equal_computation_match(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	struct term_scope_env* scope,
	uint32_t computation_id,
	uint32_t match_id,
	int* p_equal,
	uint32_t depth
) {
	if (!db || !scope || !p_equal || computation_id >= db->term_count ||
		match_id >= db->term_count ||
		db->terms[computation_id].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
		db->terms[match_id].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	*p_equal = 0;

	struct prototype_term computation = db->terms[computation_id];
	struct prototype_term outer_match = db->terms[match_id];
	uint32_t result_whnf;
	if (prototype_term_perform_with_options(
			db,
			definitions,
			options,
			computation.as.computation_type.result,
			&result_whnf
		) != 0 || result_whnf >= db->term_count ||
		db->terms[result_whnf].tag != PROTOTYPE_TERM_MATCH) {
		return 0;
	}
	struct prototype_term result_match = db->terms[result_whnf];
	if (result_match.as.match.case_count != outer_match.as.match.case_count) {
		return 0;
	}

	int equal = 0;
	if (conversion_equal_at_depth(
			db,
			definitions,
			options,
			scope,
			result_match.as.match.scrutinee,
			outer_match.as.match.scrutinee,
			&equal,
			depth + 1
		) != 0 || !equal) {
		return 0;
	}

	struct term_scope_env match_scope = *scope;
	if (conversion_scope_enter_match_frames(
			options,
			&match_scope,
			result_match.as.match.ih_scope_id,
			outer_match.as.match.ih_scope_id
		) != 0) {
		return -1;
	}

	for (uint32_t i = 0; i < result_match.as.match.case_count; ++i) {
		uint32_t result_case_id = result_match.as.match.first_case + i;
		uint32_t outer_case_id = outer_match.as.match.first_case + i;
		if (result_case_id >= db->case_count || outer_case_id >= db->case_count) {
			return -1;
		}
		struct prototype_match_case result_case = db->cases[result_case_id];
		struct prototype_match_case outer_case = db->cases[outer_case_id];
		int use_labels = result_case.constructor_owner == PROTOTYPE_INVALID_ID ||
			outer_case.constructor_owner == PROTOTYPE_INVALID_ID ||
			result_case.constructor_id == PROTOTYPE_INVALID_ID ||
			outer_case.constructor_id == PROTOTYPE_INVALID_ID;
		int labels_equal = use_labels &&
			db->case_label_symbols[result_case_id] >= 0 &&
			db->case_label_symbols[result_case_id] ==
				db->case_label_symbols[outer_case_id];
		if ((use_labels && !labels_equal) ||
			(!use_labels &&
				result_case.constructor_id != outer_case.constructor_id) ||
			result_case.binder_count != outer_case.binder_count) {
			return 0;
		}
		if (!use_labels &&
			(result_case.constructor_owner != PROTOTYPE_INVALID_ID ||
			outer_case.constructor_owner != PROTOTYPE_INVALID_ID)) {
			if (result_case.constructor_owner == PROTOTYPE_INVALID_ID ||
				outer_case.constructor_owner == PROTOTYPE_INVALID_ID ||
				conversion_equal_at_depth(
					db,
					definitions,
					options,
					&match_scope,
					result_case.constructor_owner,
					outer_case.constructor_owner,
					&equal,
					depth + 1
				) != 0 || !equal) {
				return 0;
			}
		}

		uint32_t branch_computation;
		if (prototype_term_computation_type(
				db,
				computation.as.computation_type.label,
				result_case.body,
				computation.as.computation_type.totality,
				&branch_computation
			) != 0 || conversion_equal_match_case_bodies(
				db,
				definitions,
				options,
				&match_scope,
				&result_case,
				&outer_case,
				branch_computation,
				outer_case.body,
				&equal,
				depth + 1
			) != 0 || !equal) {
			return 0;
		}
	}

	*p_equal = 1;
	return 0;
}

static struct prototype_term_reduction_options normalize_reduction_options(
	struct prototype_term_reduction_options options
) {
	return options;
}

static int reduction_is_type_expression(
	struct prototype_term_reduction_options options
) {
	return (options.flags & PROTOTYPE_TERM_REDUCE_TYPE_EXPRESSION) != 0;
}

static void normalization_mark_status(
	struct prototype_term_reduction_options options,
	int status,
	int reason
) {
	if (!options.p_normalization_status ||
		*options.p_normalization_status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
		return;
	}
	*options.p_normalization_status = status;
	if (options.p_normalization_reason) {
		*options.p_normalization_reason = reason;
	}
}

static int normalization_consume_step(struct prototype_term_reduction_options options) {
	if (!options.p_steps_remaining) {
		return 0;
	}
	if (*options.p_steps_remaining == 0) {
		normalization_mark_status(
			options,
			PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED,
			PROTOTYPE_TERM_CONVERSION_REASON_STEP_LIMIT
		);
		return 1;
	}
	(*options.p_steps_remaining)--;
	if (options.p_steps_used) {
		(*options.p_steps_used)++;
	}
	return 0;
}

static int normalization_profile_options(
	int profile,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options* p_options
) {
	if (!p_options) {
		return -1;
	}
	memset(p_options, 0, sizeof(*p_options));
	switch (profile) {
		case PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF:
			p_options->flags = PROTOTYPE_TERM_REDUCE_BETA;
			break;
		case PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF:
			p_options->flags = PROTOTYPE_TERM_REDUCE_BETA |
				PROTOTYPE_TERM_REDUCE_MATCH |
				PROTOTYPE_TERM_REDUCE_INDUCTION |
				PROTOTYPE_TERM_REDUCE_COMPUTATIONS;
			break;
		case PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF:
			p_options->flags = PROTOTYPE_TERM_REDUCE_BETA |
				PROTOTYPE_TERM_REDUCE_MATCH |
				PROTOTYPE_TERM_REDUCE_INDUCTION |
				PROTOTYPE_TERM_REDUCE_COMPUTATIONS |
				PROTOTYPE_TERM_REDUCE_TYPE_EXPRESSION;
			break;
		default:
			return -1;
	}
	if (definitions) {
		p_options->flags |= PROTOTYPE_TERM_REDUCE_DEFINITIONS;
	}
	return 0;
}

static int normalization_profile_from_options(
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options
) {
	if (definitions ||
		(options.flags & (PROTOTYPE_TERM_REDUCE_DEFINITIONS |
			PROTOTYPE_TERM_PERFORM_HOST_EFFECT)) != 0) {
		return 0;
	}
	if (options.flags == PROTOTYPE_TERM_REDUCE_BETA) {
		return PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF;
	}
	if (options.flags == (PROTOTYPE_TERM_REDUCE_BETA |
			PROTOTYPE_TERM_REDUCE_MATCH |
			PROTOTYPE_TERM_REDUCE_INDUCTION |
			PROTOTYPE_TERM_REDUCE_COMPUTATIONS |
			PROTOTYPE_TERM_REDUCE_TYPE_EXPRESSION)) {
		return PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF;
	}
	if (options.flags == (PROTOTYPE_TERM_REDUCE_BETA |
			PROTOTYPE_TERM_REDUCE_MATCH |
			PROTOTYPE_TERM_REDUCE_INDUCTION |
			PROTOTYPE_TERM_REDUCE_COMPUTATIONS)) {
		return PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF;
	}
	return 0;
}

static int normalization_cache_lookup(
	struct prototype_term_db* db,
	int profile,
	uint32_t term_id,
	uint32_t* p_result
) {
	if (!db || !p_result) {
		return 0;
	}
	uint64_t hash = (uint64_t)term_id * UINT64_C(11400714819323198485) ^
		(uint64_t)(uint32_t)profile * UINT64_C(14029467366897019727);
	size_t bucket = (size_t)hash &
		(PROTOTYPE_TERM_NORMALIZATION_CACHE_BUCKET_CAPACITY - 1);
	for (uint32_t i = db->normalization_cache_buckets[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = db->normalization_cache_next_entry[i]) {
		db->normalization_cache_stats.probe_count++;
		const struct prototype_term_normalization_cache_entry* entry =
			&db->normalization_cache[i];
		if (entry->state == PROTOTYPE_TERM_NORMALIZATION_CACHE_COMPLETE &&
			entry->graph_revision == db->normalization_graph_revision &&
			entry->profile == profile &&
			entry->term_id == term_id &&
			entry->result_term_id < db->term_count) {
			*p_result = entry->result_term_id;
			return 1;
		}
	}
	return 0;
}

static size_t normalization_cache_bucket(
	int profile,
	uint32_t term_id
) {
	uint64_t hash = (uint64_t)term_id * UINT64_C(11400714819323198485) ^
		(uint64_t)(uint32_t)profile * UINT64_C(14029467366897019727);
	return (size_t)hash &
		(PROTOTYPE_TERM_NORMALIZATION_CACHE_BUCKET_CAPACITY - 1);
}

static void normalization_cache_remove_slot(
	struct prototype_term_db* db,
	uint32_t slot
) {
	if (!db || slot >= PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY ||
		db->normalization_cache[slot].state == PROTOTYPE_TERM_NORMALIZATION_CACHE_EMPTY) {
		return;
	}
	const struct prototype_term_normalization_cache_entry* entry =
		&db->normalization_cache[slot];
	size_t bucket = normalization_cache_bucket(
		entry->profile, entry->term_id
	);
	uint32_t* link = &db->normalization_cache_buckets[bucket];
	while (*link != PROTOTYPE_INVALID_ID) {
		if (*link == slot) {
			*link = db->normalization_cache_next_entry[slot];
			break;
		}
		link = &db->normalization_cache_next_entry[*link];
	}
	db->normalization_cache_next_entry[slot] = PROTOTYPE_INVALID_ID;
}

static struct prototype_term_normalization_cache_entry* normalization_cache_reserve(
	struct prototype_term_db* db,
	int profile,
	uint32_t term_id
) {
	if (!db) {
		return NULL;
	}
	uint32_t slot = db->normalization_cache_next++ %
		PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY;
	struct prototype_term_normalization_cache_entry* entry =
		&db->normalization_cache[slot];
	if (entry->state != PROTOTYPE_TERM_NORMALIZATION_CACHE_EMPTY) {
		normalization_cache_remove_slot(db, slot);
		db->normalization_cache_stats.eviction_count++;
	}
	entry->term_id = term_id;
	entry->result_term_id = PROTOTYPE_INVALID_ID;
	entry->graph_revision = db->normalization_graph_revision;
	entry->profile = profile;
	entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_IN_PROGRESS;
	size_t bucket = normalization_cache_bucket(
		profile, term_id
	);
	db->normalization_cache_next_entry[slot] =
		db->normalization_cache_buckets[bucket];
	db->normalization_cache_buckets[bucket] = slot;
	return entry;
}

static int induction_hypothesis_recursive_match(
	struct prototype_term_db* db,
	uint32_t ih_scope_id,
	uint32_t argument,
	uint32_t* p_ret
) {
	if (!db || !p_ret || argument >= db->term_count ||
		ih_scope_id >= db->ih_scope_count) {
		return -1;
	}
	uint32_t match_term = db->ih_scopes[ih_scope_id].match_term;
	if (match_term >= db->term_count ||
		db->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return 1;
	}
	const struct prototype_term* frame_match = &db->terms[match_term];
	struct prototype_match_case_input case_inputs[64];
	struct prototype_case_binder binder_storage[256];
	uint32_t binder_cursor = 0;
	if (frame_match->as.match.case_count > 64) {
		return -1;
	}
	for (uint32_t i = 0; i < frame_match->as.match.case_count; ++i) {
		const struct prototype_match_case* old_case =
			&db->cases[frame_match->as.match.first_case + i];
		if (binder_cursor + old_case->binder_count > 256) {
			return -1;
		}
		for (uint32_t j = 0; j < old_case->binder_count; ++j) {
			binder_storage[binder_cursor + j] =
				db->case_binders[old_case->first_binder + j];
		}
		case_inputs[i].case_label_symbol_id =
			db->case_label_symbols[frame_match->as.match.first_case + i];
		case_inputs[i].constructor_owner = old_case->constructor_owner;
		case_inputs[i].constructor_id = old_case->constructor_id;
		case_inputs[i].binders = &binder_storage[binder_cursor];
		case_inputs[i].binder_count = old_case->binder_count;
		case_inputs[i].body = old_case->body;
		binder_cursor += old_case->binder_count;
	}
	return prototype_term_match_with_ih_scope(
		db,
		argument,
		case_inputs,
		frame_match->as.match.case_count,
		ih_scope_id,
		p_ret
	);
}

/* Map the recursive eliminator through the runtime representation of a
 * strictly-positive field. A source function value is THUNK(LAMBDA(...)); its
 * body is sequenced and mapped again until the recursive ADT value is reached.
 * This is the operational action of the existing CBPV constructors, not a
 * second persisted positivity schema. */
static int induction_hypothesis_map_value(
	struct prototype_term_db* db,
	uint32_t ih_scope_id,
	uint32_t value,
	uint32_t* p_ret
) {
	if (!db || !p_ret || value >= db->term_count ||
		ih_scope_id >= db->ih_scope_count) {
		return -1;
	}
	if (db->terms[value].tag != PROTOTYPE_TERM_THUNK) {
		return induction_hypothesis_recursive_match(
			db, ih_scope_id, value, p_ret
		);
	}
	uint32_t computation = db->terms[value].as.thunk.computation;
	if (computation >= db->term_count) {
		return -1;
	}
	uint32_t result_binding = prototype_term_new_binding(db);
	uint32_t result_var;
	uint32_t recursive_result;
	uint32_t continuation;
	if (result_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_var(db, result_binding, &result_var) != 0 ||
		prototype_term_induction_hypothesis(
			db, ih_scope_id, result_var, &recursive_result
		) != 0 || prototype_term_lambda(
			db, result_binding, recursive_result, &continuation
		) != 0) {
		return -1;
	}
	if (db->terms[computation].tag == PROTOTYPE_TERM_LAMBDA) {
		const struct prototype_term* function = &db->terms[computation];
		uint32_t mapped_body;
		uint32_t mapped_function;
		uint32_t mapped_thunk;
		if (prototype_term_computation_fold(
				db, function->as.lambda.body, continuation, NULL, 0,
				&mapped_body
			) != 0 || prototype_term_lambda(
				db, function->as.lambda.binding_id, mapped_body,
				&mapped_function
			) != 0 || prototype_term_thunk(
				db, mapped_function, &mapped_thunk
			) != 0) {
			return -1;
		}
		return prototype_term_return(db, mapped_thunk, p_ret);
	}
	uint32_t mapped_computation;
	uint32_t mapped_thunk;
	if (prototype_term_computation_fold(
			db, computation, continuation, NULL, 0, &mapped_computation
		) != 0 || prototype_term_thunk(
			db, mapped_computation, &mapped_thunk
		) != 0) {
		return -1;
	}
	return prototype_term_return(db, mapped_thunk, p_ret);
}

static int evaluate_steps(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
) {
	if (!db || !p_ret || term_id >= db->term_count) {
		return -1;
	}
	/* An exhausted recursive call must still return a valid residual graph. The
	 * caller replaces this value when it can rebuild a more precise parent. */
	*p_ret = term_id;
	if (normalization_consume_step(options)) {
		return -1;
	}
	if (depth == 0) {
		normalization_mark_status(
			options,
			PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED,
			PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT
		);
		return -1;
	}

	/* Reduction may append interned nodes and reallocate db->terms. Keep the
	 * input node by value across recursive reduction and graph construction. */
	struct prototype_term term_value = db->terms[term_id];
	const struct prototype_term* term = &term_value;
	switch (term->tag) {
		case PROTOTYPE_TERM_EXTERNAL_REF: {
			uint32_t definition_term;
			if ((options.flags & PROTOTYPE_TERM_REDUCE_DEFINITIONS) &&
				lookup_transparent_definition(
					definitions,
					term->as.external_ref.name,
					&definition_term
				)) {
				if (definition_term >= db->term_count) {
					return -1;
				}
					return evaluate_steps(
						db,
						definitions,
						options,
						definition_term,
						p_ret,
						depth - 1
					);
			}
			*p_ret = term_id;
			return 0;
		}
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			if (evaluate_steps(
					db,
					definitions,
					options,
					term->as.app.function,
					&function,
					depth - 1
				) != 0) {
				if (function < db->term_count) {
					(void)normalization_rebuild_on_pause(
						db, term_id, function, PROTOTYPE_TERM_APP, p_ret
					);
				}
				return -1;
			}

			/* Typed APP occurrences may expose a recursively produced callable as
			 * Comp({}, Thunk(Pi)). RETURN and THUNK make this administrative force
			 * pure and explicit, so the erased evaluator can recover the callable
			 * without consulting TypedOccurrenceGraph annotations. */
			if ((options.flags & PROTOTYPE_TERM_REDUCE_COMPUTATIONS) != 0 &&
				function < db->term_count &&
				db->terms[function].tag == PROTOTYPE_TERM_RETURN) {
				uint32_t value = db->terms[function].as.return_term.value;
				if (value < db->term_count &&
					db->terms[value].tag == PROTOTYPE_TERM_THUNK &&
					evaluate_steps(
						db,
						definitions,
						options,
						db->terms[value].as.thunk.computation,
						&function,
						depth - 1
					) != 0) {
					if (function < db->term_count) {
						uint32_t thunk;
						uint32_t returned;
						if (prototype_term_thunk(db, function, &thunk) == 0 &&
							prototype_term_return(db, thunk, &returned) == 0) {
							(void)prototype_term_app(
								db, returned, term->as.app.argument, p_ret
							);
						}
					}
					return -1;
				}
			}

			const struct prototype_term* reduced_function = &db->terms[function];
			if ((options.flags & PROTOTYPE_TERM_REDUCE_BETA) &&
				reduced_function->tag == PROTOTYPE_TERM_LAMBDA) {
				uint32_t substituted;
				if (substitute_term(
					db,
					reduced_function->as.lambda.body,
					reduced_function->as.lambda.binding_id,
					term->as.app.argument,
					&substituted
				) != 0) {
					return -1;
				}
				return evaluate_steps(
					db,
					definitions,
					options,
					substituted,
					p_ret,
					depth - 1
				);
			}

			/* TYPE_VIEW is object-language nominal type syntax. Once Layer T has
			 * admitted the application, distributing APP over its Core/source
			 * projections is structural calculation and needs no declaration DB. */
			if (reduction_is_type_expression(options) &&
				reduced_function->tag == PROTOTYPE_TERM_TYPE_VIEW) {
				uint32_t core_argument = term->as.app.argument;
				if (core_argument < db->term_count &&
					db->terms[core_argument].tag == PROTOTYPE_TERM_TYPE_VIEW) {
					core_argument = db->terms[core_argument].as.type_view.core;
				}
				uint32_t core;
				uint32_t source;
				if (prototype_term_app(
						db, reduced_function->as.type_view.core,
						core_argument, &core
					) != 0 || prototype_term_app(
						db, reduced_function->as.type_view.source,
						term->as.app.argument, &source
					) != 0) {
					return -1;
				}
				return prototype_term_type_view(
					db,
					reduced_function->as.type_view.identity,
					core,
					source,
					p_ret
				);
			}

			uint32_t candidate = term_id;
			if (function != term->as.app.function &&
				prototype_term_app(db, function, term->as.app.argument, &candidate) != 0) {
				return -1;
			}
			/* A pure primitive has the ordinary computation type Comp({}, A).
			 * Execution reduces its application to RETURN(result); it is neither
			 * an operation request nor a type-conversion rule. */
			if ((options.flags & PROTOTYPE_TERM_REDUCE_PURE_INTRINSICS) &&
				!reduction_is_type_expression(options)) {
				uint32_t head;
				uint32_t arguments[64];
				uint32_t argument_count;
				decompose_app(db, candidate, &head, arguments, &argument_count);
				if (head < db->term_count &&
					db->terms[head].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
					const struct prototype_core_pure_primitive_declaration* declaration =
						prototype_core_pure_primitive_declaration(
							db->terms[head].as.pure_primitive.primitive_id
						);
					if (declaration) {
						uint32_t result;
						int status = reduce_host_pure_primitive_step(
							db, definitions, options, candidate, &result, depth - 1
						);
						if (status < 0) {
							return -1;
						}
						if (status > 0) {
							return prototype_term_return(db, result, p_ret);
						}
					}
				}
			}
			*p_ret = candidate;
			return 0;
		}
		case PROTOTYPE_TERM_MATCH: {
			if (!(options.flags & PROTOTYPE_TERM_REDUCE_MATCH)) {
				*p_ret = term_id;
				return 0;
			}
			uint32_t scrutinee;
				if (evaluate_steps(
					db,
					definitions,
					options,
					term->as.match.scrutinee,
					&scrutinee,
					depth - 1
				) != 0) {
				if (scrutinee < db->term_count) {
					(void)normalization_rebuild_on_pause(
						db, term_id, scrutinee, PROTOTYPE_TERM_MATCH, p_ret
					);
				}
				return -1;
			}

			uint32_t head;
			uint32_t args[64];
			uint32_t arg_count;
			decompose_app(db, scrutinee, &head, args, &arg_count);
			if (head >= db->term_count) {
				return -1;
			}

			if (term_is_constructor_like(&db->terms[head]) != 0) {
				if (scrutinee == term->as.match.scrutinee) {
					*p_ret = term_id;
					return 0;
				}
				return match_rebuild_scrutinee(
					db, term_id, scrutinee, p_ret
				);
			}

			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&db->cases[term->as.match.first_case + i];
				if (!constructor_member_matches(
						db,
						head,
						match_case
					)) {
					continue;
				}
				if (match_case->binder_count != arg_count) {
					return -1;
				}

				uint32_t body = match_case->body;
				if (term->as.match.ih_scope_id != PROTOTYPE_INVALID_ID) {
					if (term->as.match.ih_scope_id >= db->ih_scope_count) {
						return -1;
					}
					uint32_t subject_binding = db->ih_scopes[
						term->as.match.ih_scope_id
					].scrutinee_binding_id;
					if (subject_binding != PROTOTYPE_INVALID_ID &&
						substitute_term(
							db,
							body,
							subject_binding,
							scrutinee,
							&body
						) != 0) {
						return -1;
					}
				}
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					struct prototype_case_binder binder = db->case_binders[match_case->first_binder + j];
					if (substitute_term(
							db,
							body,
							binder.binding_id,
							args[j],
							&body
						) != 0) {
						return -1;
					}
				}
				return evaluate_steps(
					db,
					definitions,
					options,
					body,
					p_ret,
					depth - 1
				);
			}

			*p_ret = term_id;
			return 0;
		}
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			if (!(options.flags & PROTOTYPE_TERM_REDUCE_INDUCTION)) {
				*p_ret = term_id;
				return 0;
			}
			uint32_t argument;
			if (evaluate_steps(
				db,
				definitions,
				options,
				term->as.induction_hypothesis.argument,
				&argument,
				depth - 1
			) != 0) {
				if (argument < db->term_count) {
					(void)normalization_rebuild_on_pause(
						db, term_id, argument,
						PROTOTYPE_TERM_INDUCTION_HYPOTHESIS, p_ret
					);
				}
				return -1;
			}

			uint32_t mapped;
			int map_status = induction_hypothesis_map_value(
				db,
				term->as.induction_hypothesis.ih_scope_id,
				argument,
				&mapped
			);
			if (map_status < 0) {
				return -1;
			}
			if (map_status > 0) {
				*p_ret = term_id;
				return 0;
			}
			if (options.p_induction_hypothesis_reductions) {
				(*options.p_induction_hypothesis_reductions)++;
			}
			return evaluate_steps(
				db,
				definitions,
				options,
				mapped,
				p_ret,
				depth - 1
			);
		}
		case PROTOTYPE_TERM_LAMBDA:
		case PROTOTYPE_TERM_RETURN:
		case PROTOTYPE_TERM_THUNK:
			*p_ret = term_id;
			return 0;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			if (options.operation_dispatch ||
				(options.flags & PROTOTYPE_TERM_PERFORM_HOST_EFFECT) != 0) {
				return dispatch_operation_request_step(
					db, definitions, options, term_id, p_ret, depth - 1
				);
			}
			if (reduction_is_type_expression(options)) {
				normalization_mark_status(
					options,
					PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT,
					PROTOTYPE_TERM_CONVERSION_REASON_EFFECT_REQUEST
				);
			}
			*p_ret = term_id;
			return 0;
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			if (!(options.flags & PROTOTYPE_TERM_REDUCE_COMPUTATIONS) ||
				(reduction_is_type_expression(options) && term->as.computation_fold.clause_count > 0)) {
				*p_ret = term_id;
				return 0;
			}

			/* A computation fold receives requests before host or embedding dispatch. */
			struct prototype_term_reduction_options inner_options = options;
			inner_options.flags &= ~PROTOTYPE_TERM_PERFORM_HOST_EFFECT;
			inner_options.operation_dispatch = NULL;
			inner_options.operation_dispatch_context = NULL;
			uint32_t computation;
			if (evaluate_steps(
					db, definitions, inner_options,
					term->as.computation_fold.computation, &computation, depth - 1
				) != 0) {
				if (computation < db->term_count) {
					(void)normalization_rebuild_on_pause(
						db, term_id, computation,
						PROTOTYPE_TERM_COMPUTATION_FOLD, p_ret
					);
				}
				return -1;
			}
			/* Nested surface lambdas elide their pure
			 * RETURN(THUNK(lambda)) boundary in TermDB. A computation fold is
			 * precisely where that returned value is consumed, so reconstruct the
			 * value before applying the return algebra. */
			if (computation < db->term_count &&
				db->terms[computation].tag == PROTOTYPE_TERM_LAMBDA) {
				uint32_t thunk;
				uint32_t applied;
				if (prototype_term_thunk(db, computation, &thunk) != 0 ||
					prototype_term_app(
						db, term->as.computation_fold.return_clause, thunk, &applied
					) != 0) {
					return -1;
				}
				return evaluate_steps(
					db, definitions, options, applied, p_ret, depth - 1
				);
			}
			if (computation < db->term_count &&
				db->terms[computation].tag == PROTOTYPE_TERM_THUNK) {
				uint32_t applied;
				if (prototype_term_app(
						db,
						term->as.computation_fold.return_clause,
						computation,
						&applied
					) != 0) {
					return -1;
				}
				return evaluate_steps(
					db, definitions, options, applied, p_ret, depth - 1
				);
			}
			if (computation < db->term_count &&
				db->terms[computation].tag == PROTOTYPE_TERM_RETURN) {
				uint32_t applied;
				if (prototype_term_app(
						db, term->as.computation_fold.return_clause,
						db->terms[computation].as.return_term.value, &applied
					) != 0) {
					return -1;
				}
				return evaluate_steps(
					db, definitions, options, applied, p_ret, depth - 1
				);
			}
			if (computation < db->term_count &&
				db->terms[computation].tag == PROTOTYPE_TERM_OPERATION_REQUEST) {
				const struct prototype_term* request = &db->terms[computation];
				uint32_t continuation;
				uint32_t clause_body;
				if (computation_fold_continuation(
						db, term_id, request->as.operation_request.continuation, &continuation
					) != 0 || computation_fold_find_clause(
						db, term_id, request->as.operation_request.operation, &clause_body
					) != 0) {
					return -1;
				}
				if (clause_body != PROTOTYPE_INVALID_ID) {
					uint32_t clause_application;
					if (prototype_term_app(
							db, clause_body,
						request->as.operation_request.argument, &clause_application
						) != 0 || prototype_term_app(
							db, clause_application, continuation, &clause_application
						) != 0) {
						return -1;
					}
					return evaluate_steps(
						db, definitions, options,
						clause_application, p_ret, depth - 1
					);
				}
				return prototype_term_operation_request(
					db, request->as.operation_request.operation,
					request->as.operation_request.argument, continuation, p_ret
				);
			}
			if (computation == term->as.computation_fold.computation) {
				*p_ret = term_id;
				return 0;
			}
			return computation_fold_rebuild(db, term_id, computation, p_ret);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			struct prototype_effect_row_normal_form normal;
			if (prototype_term_effect_row_normal_form(db, term_id, &normal) == 0) {
				return prototype_term_effect_row_materialize_normal_form(
					db, &normal, p_ret
				);
			}
			*p_ret = term_id;
			return 0;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
		case PROTOTYPE_TERM_PI:
		case PROTOTYPE_TERM_VAR:
			case PROTOTYPE_TERM_CONSTRUCTOR:
			case PROTOTYPE_TERM_TYPE_FORMER:
			case PROTOTYPE_TERM_TYPE_DECLARATION:
			case PROTOTYPE_TERM_TYPE_VIEW:
				case PROTOTYPE_TERM_UNIVERSE_VAR:
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
			case PROTOTYPE_TERM_TEXT_LITERAL:
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				case PROTOTYPE_TERM_INT_LITERAL:
				case PROTOTYPE_TERM_PURE_PRIMITIVE:
				case PROTOTYPE_TERM_EFFECT_OPERATION:
				case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				case PROTOTYPE_TERM_EFFECT_ROW_VAR:
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
		case PROTOTYPE_TERM_THUNK_TYPE:
			*p_ret = term_id;
			return 0;
		case PROTOTYPE_TERM_FORCE: {
			if (!(options.flags & PROTOTYPE_TERM_REDUCE_COMPUTATIONS)) {
				*p_ret = term_id;
				return 0;
			}
			uint32_t value;
			if (evaluate_steps(
					db,
					definitions,
					options,
					term->as.force.value,
					&value,
					depth - 1
				) != 0) {
				if (value < db->term_count) {
					(void)normalization_rebuild_on_pause(
						db, term_id, value, PROTOTYPE_TERM_FORCE, p_ret
					);
				}
				return -1;
			}
			if (value < db->term_count && db->terms[value].tag == PROTOTYPE_TERM_THUNK) {
				uint32_t computation = db->terms[value].as.thunk.computation;
				if (reduction_is_type_expression(options)) {
					uint32_t pure_computation;
					if (evaluate_steps(
							db,
							definitions,
							options,
							computation,
							&pure_computation,
							depth - 1
						) != 0) {
						if (pure_computation < db->term_count) {
							uint32_t residual_thunk;
							if (prototype_term_thunk(
									db, pure_computation, &residual_thunk
								) == 0) {
								(void)prototype_term_force(
									db, residual_thunk, p_ret
								);
							}
						}
						return -1;
					}
					if (pure_computation >= db->term_count ||
						(db->terms[pure_computation].tag != PROTOTYPE_TERM_RETURN &&
							db->terms[pure_computation].tag != PROTOTYPE_TERM_LAMBDA)) {
						if (pure_computation < db->term_count &&
							db->terms[pure_computation].tag ==
								PROTOTYPE_TERM_OPERATION_REQUEST) {
							normalization_mark_status(
								options,
								PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT,
								PROTOTYPE_TERM_CONVERSION_REASON_EFFECT_REQUEST
							);
						}
						*p_ret = term_id;
						return 0;
					}
					return evaluate_steps(
						db,
						definitions,
						options,
						pure_computation,
						p_ret,
						depth - 1
					);
				}
				return evaluate_steps(
					db,
					definitions,
					options,
					computation,
					p_ret,
					depth - 1
				);
			}
			if (value == term->as.force.value) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_force(db, value, p_ret);
		}
		default:
					*p_ret = term_id;
					return 0;
	}
}

int prototype_term_normalize_complete_with_profile(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	uint32_t* p_ret
) {
	struct prototype_term_reduction_options options;
	int normalization_status = PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE;
	if (normalization_profile_options(profile, definitions, &options) != 0) {
		return -1;
	}
	options.p_normalization_status = &normalization_status;
	if (prototype_term_perform_with_options(
		db,
		definitions,
		options,
		term_id,
		p_ret
		) != 0) {
		return -1;
	}
	return normalization_status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ? 0 : -1;
}

int prototype_term_normalize_with_profile(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	uint64_t step_limit,
	struct prototype_term_normalization_result* p_result
) {
	struct prototype_term_reduction_options options;
	uint32_t result_term = PROTOTYPE_INVALID_ID;
	int status = PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE;
	uint64_t steps_remaining;
	uint64_t steps_used = 0;
	if (!p_result) {
		return -1;
	}
	p_result->status = PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID;
	p_result->term_id = PROTOTYPE_INVALID_ID;
	p_result->step_limit = 0;
	p_result->steps_used = 0;
	p_result->graph_revision = 0;
	if (!db || term_id >= db->term_count ||
		normalization_profile_options(profile, definitions, &options) != 0) {
		return 0;
	}
	steps_remaining = step_limit;
	p_result->step_limit = steps_remaining;
	p_result->graph_revision = db->normalization_graph_revision;
	options.p_normalization_status = &status;
	options.p_steps_remaining = &steps_remaining;
	options.p_steps_used = &steps_used;
	if (evaluate_steps(
			db,
			definitions,
			options,
			term_id,
			&result_term,
			PROTOTYPE_EVALUATION_DEPTH_LIMIT
		) != 0) {
		if (status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
			status = PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID;
		}
		p_result->status = status;
		if (status == PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED &&
			result_term < db->term_count) {
			p_result->term_id = result_term;
		}
		p_result->steps_used = steps_used;
		return 0;
	}
	p_result->status = status;
	p_result->term_id = result_term;
	p_result->steps_used = steps_used;
	return 0;
}

struct prototype_term_normalization_machine {
	struct prototype_term_db* db;
	const struct prototype_term_definition_env* definitions;
	int profile;
	int status;
	uint32_t current_term;
	uint64_t expected_graph_revision;
};

int prototype_term_normalization_machine_create(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t term_id,
	struct prototype_term_normalization_machine** p_machine
) {
	struct prototype_term_reduction_options ignored;
	if (!db || !p_machine || term_id >= db->term_count ||
		normalization_profile_options(profile, definitions, &ignored) != 0) {
		return -1;
	}
	*p_machine = NULL;
	struct prototype_term_normalization_machine* machine = calloc(
		1, sizeof(*machine)
	);
	if (!machine) {
		return -1;
	}
	machine->db = db;
	machine->definitions = definitions;
	machine->profile = profile;
	machine->status = PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED;
	machine->current_term = term_id;
	machine->expected_graph_revision = db->normalization_graph_revision;
	*p_machine = machine;
	return 0;
}

int prototype_term_normalization_machine_advance(
	struct prototype_term_normalization_machine* machine,
	uint64_t step_limit,
	struct prototype_term_normalization_result* p_result
) {
	if (!machine || !p_result || !machine->db ||
		machine->current_term >= machine->db->term_count) {
		return -1;
	}
	memset(p_result, 0, sizeof(*p_result));
	p_result->status = PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID;
	p_result->term_id = PROTOTYPE_INVALID_ID;
	p_result->step_limit = step_limit;
	p_result->graph_revision = machine->db->normalization_graph_revision;
	if (machine->db->normalization_graph_revision !=
		machine->expected_graph_revision) {
		machine->status = PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID;
		return 0;
	}
	if (machine->status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
		machine->status ==
			PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT ||
		machine->status == PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
		p_result->status = machine->status;
		p_result->term_id = machine->status ==
			PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID ?
			PROTOTYPE_INVALID_ID : machine->current_term;
		return 0;
	}
	if (prototype_term_normalize_with_profile(
			machine->db,
			machine->definitions,
			machine->profile,
			machine->current_term,
			step_limit,
			p_result
		) != 0) {
		return -1;
	}
	if (p_result->term_id != PROTOTYPE_INVALID_ID) {
		machine->current_term = p_result->term_id;
	}
	machine->status = p_result->status;
	machine->expected_graph_revision =
		machine->db->normalization_graph_revision;
	p_result->graph_revision = machine->expected_graph_revision;
	return 0;
}

uint32_t prototype_term_normalization_machine_current(
	const struct prototype_term_normalization_machine* machine
) {
	return machine ? machine->current_term : PROTOTYPE_INVALID_ID;
}

void prototype_term_normalization_machine_destroy(
	struct prototype_term_normalization_machine* machine
) {
	free(machine);
}

static int project_structural_return_value_at_depth(
	struct prototype_term_db* db,
	uint32_t computation,
	uint64_t step_limit,
	uint32_t* p_value,
	uint32_t* active_terms,
	size_t active_count,
	size_t active_capacity
) {
	struct prototype_term_normalization_result normalization;
	if (!db || !p_value || computation >= db->term_count ||
		!active_terms) {
		return -1;
	}
	if (active_count >= active_capacity) {
		return 1;
	}
	for (size_t i = 0; i < active_count; ++i) {
		if (active_terms[i] == computation) {
			return 1;
		}
	}
	active_terms[active_count++] = computation;
	uint32_t source_root = computation;
	while (source_root < db->term_count &&
		db->terms[source_root].tag == PROTOTYPE_TERM_APP) {
		source_root = db->terms[source_root].as.app.function;
	}
	if (source_root < db->term_count && db->terms[source_root].tag ==
		PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
		/* Do not unfold a guarded recursive call merely to name its result in a
		 * dependent result-type term. Its source APP spine is the stable symbolic
		 * value; iota evaluation remains available when the enclosing recursive
		 * invocation itself reduces. */
		*p_value = computation;
		return 0;
	}
	/* Expose beta structure before enabling iota/induction reduction. This lets
	 * the projection see a zero-clause fold around a guarded IH and preserve the
	 * recursive call symbolically instead of unfolding it into the same Match. */
	struct prototype_term_normalization_result core_normalization;
	if (prototype_term_normalize_with_profile(
			db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_CORE_WHNF,
			computation,
			step_limit,
			&core_normalization
		) != 0 || core_normalization.status ==
			PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
		return -1;
	}
	if (core_normalization.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
		return 1;
	}
	if (core_normalization.term_id != computation) {
		return project_structural_return_value_at_depth(
			db,
			core_normalization.term_id,
			step_limit,
			p_value,
			active_terms,
			active_count,
			active_capacity
		);
	}
	if (db->terms[computation].tag == PROTOTYPE_TERM_RETURN ||
		db->terms[computation].tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
		memset(&normalization, 0, sizeof(normalization));
		normalization.status = PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE;
		normalization.term_id = computation;
	} else if (prototype_term_normalize_with_profile(
			db,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
			computation,
			step_limit,
			&normalization
		) != 0 || normalization.status ==
			PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID) {
		return -1;
	}
	if (normalization.status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE ||
		normalization.term_id >= db->term_count) {
		return 1;
	}

	const struct prototype_term normalized = db->terms[normalization.term_id];
	if (normalized.tag == PROTOTYPE_TERM_RETURN) {
		*p_value = normalized.as.return_term.value;
		return 0;
	}
	if (normalized.tag == PROTOTYPE_TERM_COMPUTATION_FOLD) {
		if (normalized.as.computation_fold.clause_count != 0 ||
			normalized.as.computation_fold.computation >= db->term_count ||
			normalized.as.computation_fold.return_clause >= db->term_count) {
			return normalized.as.computation_fold.clause_count == 0 ? -1 : 1;
		}
		const struct prototype_term* continuation =
			&db->terms[normalized.as.computation_fold.return_clause];
		if (continuation->tag == PROTOTYPE_TERM_LAMBDA &&
			!prototype_term_contains_free_binding(
				db,
				continuation->as.lambda.body,
				continuation->as.lambda.binding_id
			)) {
			/* Sequencing still performs its input at runtime.  Structural projection
			 * only asks which value is returned if the computation completes, so an
			 * input-independent continuation can be projected without interpreting an
			 * operation request or duplicating its effects. */
			return project_structural_return_value_at_depth(
				db,
				continuation->as.lambda.body,
				step_limit,
				p_value,
				active_terms,
				active_count,
				active_capacity
			);
		}
		uint32_t input_value;
		int input_status = project_structural_return_value_at_depth(
			db,
			normalized.as.computation_fold.computation,
			step_limit,
			&input_value,
			active_terms,
			active_count,
			active_capacity
		);
		if (input_status != 0) {
			return input_status;
		}
		uint32_t continued;
		if (prototype_term_app(
				db,
				normalized.as.computation_fold.return_clause,
				input_value,
				&continued
			) != 0) {
			return -1;
		}
		return project_structural_return_value_at_depth(
			db,
			continued,
			step_limit,
			p_value,
			active_terms,
			active_count,
			active_capacity
		);
	}
	uint32_t match_term = normalization.term_id;
	uint32_t app_argument = PROTOTYPE_INVALID_ID;
	if (normalized.tag == PROTOTYPE_TERM_APP) {
		uint32_t function_whnf;
		if (prototype_term_normalize_complete_with_profile(
				db,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_TYPE_EXPRESSION_WHNF,
				normalized.as.app.function,
				&function_whnf
			) != 0 || function_whnf >= db->term_count) {
			return -1;
		}
		if (db->terms[function_whnf].tag != PROTOTYPE_TERM_MATCH) {
			uint32_t root = normalization.term_id;
			while (root < db->term_count &&
				db->terms[root].tag == PROTOTYPE_TERM_APP) {
				root = db->terms[root].as.app.function;
			}
			if (root < db->term_count && db->terms[root].tag ==
				PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
				/* The induction rule already proves this recursive call guarded.
				 * Its erased APP spine is the symbolic result used by prior type
				 * computation; runtime evaluation still owns the actual call. */
				*p_value = normalization.term_id;
				return 0;
			}
			return 1;
		}
		match_term = function_whnf;
		app_argument = normalized.as.app.argument;
	} else if (normalized.tag != PROTOTYPE_TERM_MATCH) {
		return 1;
	}
	const struct prototype_term match = db->terms[match_term];
	if (match.as.match.case_count > 64) {
		return -1;
	}

	struct prototype_match_case_input cases[64];
	struct prototype_match_case source_cases[64];
	int source_labels[64];
	struct prototype_case_binder source_binders[256];
	uint32_t binder_cursor = 0;
	for (uint32_t i = 0; i < match.as.match.case_count; ++i) {
		uint32_t case_id = match.as.match.first_case + i;
		if (case_id >= db->case_count || db->cases[case_id].binder_count >
			256 - binder_cursor) {
			return -1;
		}
		source_cases[i] = db->cases[case_id];
		source_labels[i] = db->case_label_symbols[case_id];
		for (uint32_t j = 0; j < source_cases[i].binder_count; ++j) {
			uint32_t binder_id = source_cases[i].first_binder + j;
			if (binder_id >= db->case_binder_count) {
				return -1;
			}
			source_binders[binder_cursor + j] = db->case_binders[binder_id];
		}
		source_cases[i].first_binder = binder_cursor;
		binder_cursor += source_cases[i].binder_count;
	}
	int has_unresolved_nullary_case = 0;
	for (uint32_t i = 0; i < match.as.match.case_count; ++i) {
		if (source_cases[i].constructor_owner == PROTOTYPE_INVALID_ID) {
			if (source_cases[i].binder_count != 0) {
				return 1;
			}
			has_unresolved_nullary_case = 1;
		}
	}
	if (has_unresolved_nullary_case) {
		/* Constructor labels are resolved by the typed-to-Core projection. A
		 * provisional Match is not executable Core and cannot be repaired here. */
		return 1;
	}
	for (uint32_t i = 0; i < match.as.match.case_count; ++i) {
		uint32_t body;
		uint32_t branch_computation = source_cases[i].body;
		if (app_argument != PROTOTYPE_INVALID_ID && prototype_term_app(
				db, branch_computation, app_argument, &branch_computation
			) != 0) {
			return -1;
		}
		int branch_status = project_structural_return_value_at_depth(
				db,
				branch_computation,
				step_limit,
				&body,
				active_terms,
				active_count,
				active_capacity
			);
		if (branch_status != 0) {
			return branch_status;
		}
		cases[i].case_label_symbol_id = source_labels[i];
		cases[i].constructor_owner = source_cases[i].constructor_owner;
		cases[i].constructor_id = source_cases[i].constructor_id;
		cases[i].binders = source_cases[i].binder_count == 0 ? NULL :
			&source_binders[source_cases[i].first_binder];
		cases[i].binder_count = source_cases[i].binder_count;
		cases[i].body = body;
	}
	return prototype_term_match_with_ih_scope(
		db,
		match.as.match.scrutinee,
		cases,
		match.as.match.case_count,
		app_argument == PROTOTYPE_INVALID_ID ?
			match.as.match.ih_scope_id : PROTOTYPE_INVALID_ID,
		p_value
	);
}

int prototype_term_project_structural_return_value(
	struct prototype_term_db* db,
	uint32_t computation,
	uint64_t step_limit,
	uint32_t* p_value
) {
	uint32_t active_terms[512];
	return project_structural_return_value_at_depth(
		db,
		computation,
		step_limit,
		p_value,
		active_terms,
		0,
		512
	);
}

int prototype_term_perform_with_options(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
) {
	options = normalize_reduction_options(options);
	int profile = normalization_profile_from_options(definitions, options);
	if (profile == 0 || !db || !p_ret) {
		return evaluate_steps(
			db,
			definitions,
			options,
			term_id,
			p_ret,
			PROTOTYPE_EVALUATION_DEPTH_LIMIT
		);
	}
	uint32_t cached;
	if (normalization_cache_lookup(
			db, profile, term_id, &cached
		)) {
		db->normalization_cache_stats.hit_count++;
		*p_ret = cached;
		return 0;
	}
	db->normalization_cache_stats.miss_count++;
	struct prototype_term_normalization_cache_entry* entry =
		normalization_cache_reserve(
			db, profile, term_id
		);
	uint64_t entry_revision = db ? db->normalization_graph_revision : 0;
	int status = evaluate_steps(
		db,
		definitions,
		options,
		term_id,
		p_ret,
		PROTOTYPE_EVALUATION_DEPTH_LIMIT
	);
	if (!entry) {
		return status;
	}
	if (status == 0 && db && entry_revision == db->normalization_graph_revision &&
		(!options.p_normalization_status ||
			*options.p_normalization_status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE)) {
		entry->result_term_id = *p_ret;
		entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_COMPLETE;
	} else {
		entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_EMPTY;
	}
	return status;
}

static int normalize_term_at_depth(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
) {
	uint32_t whnf;
	if (!db || !p_ret || term_id >= db->term_count || depth == 0 ||
		prototype_term_perform_with_options(
			db, definitions, options, term_id, &whnf
		) != 0 || whnf >= db->term_count) {
		return -1;
	}

	const struct prototype_term* term = &db->terms[whnf];
	switch (term->tag) {
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			uint32_t argument;
			uint32_t rebuilt;
			if (normalize_term_at_depth(
					db, definitions, options,
					term->as.app.function, &function, depth - 1
				) != 0 ||
				normalize_term_at_depth(
					db, definitions, options,
					term->as.app.argument, &argument, depth - 1
				) != 0 ||
				prototype_term_app(db, function, argument, &rebuilt) != 0) {
				return -1;
			}
			return rebuilt == whnf ? (*p_ret = rebuilt, 0) :
				normalize_term_at_depth(
					db, definitions, options, rebuilt, p_ret, depth - 1
				);
		}
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t body;
			if (normalize_term_at_depth(
					db, definitions, options,
					term->as.lambda.body, &body, depth - 1
				) != 0) {
				return -1;
			}
			return prototype_term_lambda(db, term->as.lambda.binding_id, body, p_ret);
		}
		case PROTOTYPE_TERM_RETURN: {
			uint32_t value;
			if (normalize_term_at_depth(
					db, definitions, options,
					term->as.return_term.value, &value, depth - 1
				) != 0) {
				return -1;
			}
			return prototype_term_return(db, value, p_ret);
		}
		case PROTOTYPE_TERM_THUNK: {
			uint32_t computation;
			if (normalize_term_at_depth(
					db, definitions, options,
					term->as.thunk.computation, &computation, depth - 1
				) != 0) {
				return -1;
			}
			return prototype_term_thunk(db, computation, p_ret);
		}
		case PROTOTYPE_TERM_PI: {
			uint32_t domain;
			uint32_t codomain_family;
			if (normalize_term_at_depth(
					db, definitions, options,
					term->as.pi.domain, &domain, depth - 1
				) != 0 ||
				normalize_term_at_depth(
					db, definitions, options,
					term->as.pi.codomain_family, &codomain_family, depth - 1
				) != 0) {
				return -1;
			}
			return prototype_term_pi_family(db, domain, codomain_family, p_ret);
		}
		case PROTOTYPE_TERM_MATCH: {
			struct prototype_match_case_input cases[64];
			struct prototype_case_binder binders[256];
			uint32_t scrutinee;
			uint32_t binder_cursor = 0;
			uint32_t rebuilt;
			if (term->as.match.case_count > 64 ||
				normalize_term_at_depth(
					db, definitions, options,
					term->as.match.scrutinee, &scrutinee, depth - 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&db->cases[term->as.match.first_case + i];
				uint32_t body;
				if (binder_cursor + old_case->binder_count > 256 ||
					normalize_term_at_depth(
						db, definitions, options,
						old_case->body, &body, depth - 1
					) != 0) {
					return -1;
				}
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					binders[binder_cursor + j] =
						db->case_binders[old_case->first_binder + j];
				}
				cases[i] = (struct prototype_match_case_input){
					.case_label_symbol_id = db->case_label_symbols[term->as.match.first_case + i],
					.constructor_owner = old_case->constructor_owner,
					.constructor_id = old_case->constructor_id,
					.binders = &binders[binder_cursor],
					.binder_count = old_case->binder_count,
					.body = body
				};
				binder_cursor += old_case->binder_count;
			}
			if (prototype_term_match_with_ih_scope(
					db, scrutinee, cases, term->as.match.case_count,
					term->as.match.ih_scope_id, &rebuilt
				) != 0) {
				return -1;
			}
			return rebuilt == whnf ? (*p_ret = rebuilt, 0) :
				normalize_term_at_depth(
					db, definitions, options, rebuilt, p_ret, depth - 1
				);
		}
		case PROTOTYPE_TERM_DIMENSION_ACTION: {
			uint32_t source;
			if (normalize_term_at_depth(
					db,
					definitions,
					options,
					term->as.dimension_action.source,
					&source,
					depth - 1
				) != 0) {
				return -1;
			}
			if (source == term->as.dimension_action.source) {
				*p_ret = whnf;
				return 0;
			}
			struct prototype_term acted = *term;
			acted.as.dimension_action.source = source;
			return add_term(db, acted, p_ret);
		}
		default:
			*p_ret = whnf;
			return 0;
	}
}

int prototype_term_nf_with_options(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
) {
	return normalize_term_at_depth(
		db,
		definitions,
		normalize_reduction_options(options),
		term_id,
		p_ret,
		PROTOTYPE_EVALUATION_DEPTH_LIMIT
	);
}

/* Application commutes with a neutral eliminator whose branches produce
 * functions. This is a conversion rule, not an evaluation-order choice:
 *
 *     (match s with C_i xs => f_i) a
 *       == match s with C_i xs => f_i a
 *
 * Keeping it in conversion preserves the runtime graph while allowing type
 * computation to compare an unfolded recursive call with its branchwise
 * specialization. */
static int conversion_distribute_app_over_match(
	struct prototype_term_db* db,
	uint32_t match_id,
	uint32_t argument,
	uint32_t* p_result
) {
	if (!db || !p_result || match_id >= db->term_count ||
		argument >= db->term_count || db->terms[match_id].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	struct prototype_term match = db->terms[match_id];
	if (match.as.match.case_count > 64) {
		return -1;
	}
	struct prototype_match_case source_cases[64];
	int source_labels[64];
	struct prototype_case_binder binders[256];
	uint32_t binder_cursor = 0;
	for (uint32_t i = 0; i < match.as.match.case_count; ++i) {
		uint32_t case_id = match.as.match.first_case + i;
		if (case_id >= db->case_count ||
			db->cases[case_id].binder_count > 256 - binder_cursor) {
			return -1;
		}
		source_cases[i] = db->cases[case_id];
		source_labels[i] = db->case_label_symbols[case_id];
		for (uint32_t j = 0; j < source_cases[i].binder_count; ++j) {
			uint32_t binder_id = source_cases[i].first_binder + j;
			if (binder_id >= db->case_binder_count) {
				return -1;
			}
			binders[binder_cursor + j] = db->case_binders[binder_id];
		}
		source_cases[i].first_binder = binder_cursor;
		binder_cursor += source_cases[i].binder_count;
	}

	struct prototype_match_case_input cases[64];
	for (uint32_t i = 0; i < match.as.match.case_count; ++i) {
		uint32_t body;
		if (prototype_term_app(db, source_cases[i].body, argument, &body) != 0) {
			return -1;
		}
		uint32_t projected_body;
		int projection_status = prototype_term_project_structural_return_value(
			db, body, UINT64_MAX, &projected_body
		);
		if (projection_status < 0) {
			return -1;
		}
		if (projection_status == 0) {
			body = projected_body;
		}
		cases[i] = (struct prototype_match_case_input){
			.case_label_symbol_id = source_labels[i],
			.constructor_owner = source_cases[i].constructor_owner,
			.constructor_id = source_cases[i].constructor_id,
			.binders = source_cases[i].binder_count == 0 ? NULL :
				&binders[source_cases[i].first_binder],
			.binder_count = source_cases[i].binder_count,
			.body = body
		};
	}
	return prototype_term_match_with_ih_scope(
		db,
		match.as.match.scrutinee,
		cases,
		match.as.match.case_count,
		match.as.match.ih_scope_id,
		p_result
	);
}

static int conversion_equal_at_depth(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	struct term_scope_env* scope,
	uint32_t left,
	uint32_t right,
	int* p_equal,
	uint32_t depth
) {
	if (!db || !scope || !p_equal || left >= db->term_count || right >= db->term_count) {
		return -1;
	}
	if (depth > 64) {
		normalization_mark_status(
			options,
			PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED,
			PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT
		);
		return -1;
	}
	*p_equal = 0;
	if (db->terms[left].tag == PROTOTYPE_TERM_INDUCTION_HYPOTHESIS &&
		db->terms[right].tag == PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
		int frame_bound = 0;
		int frames_equal = term_scope_frames_equal(
				scope,
				db->terms[left].as.induction_hypothesis.ih_scope_id,
				db->terms[right].as.induction_hypothesis.ih_scope_id,
				&frame_bound
			);
		if (!frames_equal && !frame_bound) {
			uint32_t left_frame =
				db->terms[left].as.induction_hypothesis.ih_scope_id;
			uint32_t right_frame =
				db->terms[right].as.induction_hypothesis.ih_scope_id;
			frames_equal = left_frame < db->ih_scope_count &&
				right_frame < db->ih_scope_count &&
				db->ih_scopes[left_frame].binding_scope_id ==
					db->ih_scopes[right_frame].binding_scope_id;
		}
		if (!frames_equal) {
			return 0;
		}
		return conversion_equal_at_depth(
			db,
			definitions,
			options,
			scope,
			db->terms[left].as.induction_hypothesis.argument,
			db->terms[right].as.induction_hypothesis.argument,
			p_equal,
			depth + 1
		);
	}

	uint32_t left_whnf;
	uint32_t right_whnf;
	struct prototype_term_reduction_options whnf_options = options;
	/* An IH is a neutral term while its owning Match frame is in lexical
	 * scope. Unfolding it here recreates the same Match recursively instead
	 * of comparing the two scoped occurrences. */
	if (scope->frame_count > 0) {
		whnf_options.flags &= ~PROTOTYPE_TERM_REDUCE_INDUCTION;
	}
	if (prototype_term_perform_with_options(
			db, definitions, whnf_options, left, &left_whnf
		) != 0 || prototype_term_perform_with_options(
			db, definitions, whnf_options, right, &right_whnf
		) != 0 ||
		left_whnf >= db->term_count ||
		right_whnf >= db->term_count) {
		return -1;
	}

	int shape_equal = 0;
	/* Local alpha shape is a sufficient reflexivity/congruence fast path.
	 * Artifact-link compatibility is a separate relation and must never be
	 * substituted here; a miss continues through scoped conversion. */
	if (prototype_term_view_shape_equal(
			db,
			left_whnf,
			right_whnf,
			&shape_equal
		) == 0 && shape_equal) {
		*p_equal = 1;
		return 0;
	}

	/* Recursive conversion may reduce either side and grow the shared graph.
	 * Values remain stable while pointers into db->terms do not. */
	struct prototype_term left_term_value = db->terms[left_whnf];
	struct prototype_term right_term_value = db->terms[right_whnf];
	const struct prototype_term* left_term = &left_term_value;
	const struct prototype_term* right_term = &right_term_value;
	if (left_term->tag != right_term->tag) {
		/* A typed operation view and its erased Core projection denote the same
		 * value at the Layer T/Core boundary.  This case must be recursive: an
		 * indexed classifier can contain a constructor view below an APP spine.
		 * Two views are still compared nominally by the TYPE_VIEW case below. */
		if (reduction_is_type_expression(options) &&
			left_term->tag == PROTOTYPE_TERM_TYPE_VIEW) {
			return conversion_equal_at_depth(
				db, definitions, options, scope,
				left_term->as.type_view.core, right_whnf,
				p_equal, depth + 1
			);
		}
		if (reduction_is_type_expression(options) &&
			right_term->tag == PROTOTYPE_TERM_TYPE_VIEW) {
			return conversion_equal_at_depth(
				db, definitions, options, scope,
				left_whnf, right_term->as.type_view.core,
				p_equal, depth + 1
			);
		}
		if (left_term->tag == PROTOTYPE_TERM_APP &&
			left_term->as.app.function < db->term_count &&
			db->terms[left_term->as.app.function].tag == PROTOTYPE_TERM_MATCH) {
			uint32_t distributed;
			if (conversion_distribute_app_over_match(
					db,
					left_term->as.app.function,
					left_term->as.app.argument,
					&distributed
				) != 0) {
				return -1;
			}
			return conversion_equal_at_depth(
				db, definitions, options, scope,
				distributed, right_whnf, p_equal, depth + 1
			);
		}
		if (right_term->tag == PROTOTYPE_TERM_APP &&
			right_term->as.app.function < db->term_count &&
			db->terms[right_term->as.app.function].tag == PROTOTYPE_TERM_MATCH) {
			uint32_t distributed;
			if (conversion_distribute_app_over_match(
					db,
					right_term->as.app.function,
					right_term->as.app.argument,
					&distributed
				) != 0) {
				return -1;
			}
			return conversion_equal_at_depth(
				db, definitions, options, scope,
				left_whnf, distributed, p_equal, depth + 1
			);
		}
		if (left_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
			right_term->tag == PROTOTYPE_TERM_MATCH) {
			return conversion_equal_computation_match(
				db, definitions, options,
				scope, left_whnf, right_whnf, p_equal, depth + 1
			);
		}
		if (left_term->tag == PROTOTYPE_TERM_MATCH &&
			right_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return conversion_equal_computation_match(
				db, definitions, options,
				scope, right_whnf, left_whnf, p_equal, depth + 1
			);
		}
		return 0;
	}

	switch (left_term->tag) {
		case PROTOTYPE_TERM_VAR:
			*p_equal = term_scope_binders_equal(
				scope,
				left_term->as.var.binding_id,
				right_term->as.var.binding_id
			);
			return 0;
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t left_representation_id;
			uint32_t right_representation_id;
			if (left_term->as.constructor.constructor_id != right_term->as.constructor.constructor_id) {
				return 0;
			}
			if (constructor_owner_representation_id(
					db,
					left_term->as.constructor.owner,
					&left_representation_id
				) != 0 ||
				constructor_owner_representation_id(
					db,
					right_term->as.constructor.owner,
					&right_representation_id
				) != 0) {
				return -1;
			}
			*p_equal = left_representation_id == right_representation_id;
			return 0;
		}
		case PROTOTYPE_TERM_APP: {
			int equal = 0;
			if (conversion_equal_at_depth(
					db,
					definitions,
					options,
					scope,
					left_term->as.app.function,
					right_term->as.app.function,
					&equal,
					depth + 1
				) != 0 || !equal) {
				return 0;
			}
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.app.argument,
				right_term->as.app.argument,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_LAMBDA: {
			struct term_scope_env body_scope = *scope;
			if (conversion_scope_push_binder(
					options,
					&body_scope,
					left_term->as.lambda.binding_id,
					right_term->as.lambda.binding_id
				) != 0) {
				return -1;
			}
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				&body_scope,
				left_term->as.lambda.body,
				right_term->as.lambda.body,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_PI: {
			int equal = 0;
			if (conversion_equal_at_depth(
					db,
					definitions,
					options,
					scope,
					left_term->as.pi.domain,
					right_term->as.pi.domain,
					&equal,
					depth + 1
				) != 0 || !equal) {
				return 0;
			}
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.pi.codomain_family,
				right_term->as.pi.codomain_family,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			if (left_term->as.match.case_count != right_term->as.match.case_count) {
				return 0;
			}
			int equal = 0;
			if (conversion_equal_at_depth(
					db,
					definitions,
					options,
					scope,
					left_term->as.match.scrutinee,
					right_term->as.match.scrutinee,
					&equal,
					depth + 1
			) != 0 || !equal) {
				return 0;
			}
			struct term_scope_env match_scope = *scope;
			if (conversion_scope_enter_match_frames(
					options,
					&match_scope,
					left_term->as.match.ih_scope_id,
					right_term->as.match.ih_scope_id
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < left_term->as.match.case_count; ++i) {
				uint32_t left_case_id = left_term->as.match.first_case + i;
				uint32_t right_case_id = right_term->as.match.first_case + i;
				if (left_case_id >= db->case_count || right_case_id >= db->case_count) {
					return -1;
				}
				const struct prototype_match_case* left_case =
					&db->cases[left_case_id];
				const struct prototype_match_case* right_case =
					&db->cases[right_case_id];
				int use_labels =
					left_case->constructor_owner == PROTOTYPE_INVALID_ID ||
					right_case->constructor_owner == PROTOTYPE_INVALID_ID ||
					left_case->constructor_id == PROTOTYPE_INVALID_ID ||
					right_case->constructor_id == PROTOTYPE_INVALID_ID;
				int labels_equal = use_labels && db->case_label_symbols[
					left_term->as.match.first_case + i
				] >= 0 && db->case_label_symbols[
					left_term->as.match.first_case + i
				] == db->case_label_symbols[
					right_term->as.match.first_case + i
				];
				if ((use_labels && !labels_equal) ||
					(!use_labels &&
						left_case->constructor_id != right_case->constructor_id) ||
					left_case->binder_count != right_case->binder_count) {
					return 0;
				}
				if (!use_labels &&
					(left_case->constructor_owner != PROTOTYPE_INVALID_ID ||
					right_case->constructor_owner != PROTOTYPE_INVALID_ID)) {
					/* Constructor-owner TypeViews are typed-occurrence evidence.
					 * Core Match conversion observes only their computation shape. */
					int owners_equal = 0;
					if (left_case->constructor_owner == PROTOTYPE_INVALID_ID ||
						right_case->constructor_owner == PROTOTYPE_INVALID_ID ||
						prototype_term_core_shape_equal(
							db,
							left_case->constructor_owner,
							right_case->constructor_owner,
							&owners_equal
						) != 0 || !owners_equal) {
						return 0;
					}
				}
				if (conversion_equal_match_case_bodies(
						db,
						definitions,
						options,
						&match_scope,
						left_case,
						right_case,
						left_case->body,
						right_case->body,
						&equal,
						depth + 1
					) != 0 || !equal) {
					return 0;
				}
			}
			*p_equal = 1;
			return 0;
		}
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			int frame_bound = 0;
			if (!term_scope_frames_equal(
					scope,
					left_term->as.induction_hypothesis.ih_scope_id,
					right_term->as.induction_hypothesis.ih_scope_id,
					&frame_bound
				)) {
				return 0;
			}
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.induction_hypothesis.argument,
				right_term->as.induction_hypothesis.argument,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			if (left_term->as.dimension_action.operator_id !=
				right_term->as.dimension_action.operator_id) {
				*p_equal = 0;
				return 0;
			}
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.dimension_action.source,
				right_term->as.dimension_action.source,
				p_equal,
				depth + 1
			);
		case PROTOTYPE_TERM_TYPE_FORMER:
			*p_equal = left_term->as.type_former.representation_id ==
				right_term->as.type_former.representation_id;
			return 0;
		case PROTOTYPE_TERM_TYPE_DECLARATION: {
			*p_equal = qualified_names_equal(
				left_term->as.type_declaration.identity,
				right_term->as.type_declaration.identity
			);
			return 0;
		}
		case PROTOTYPE_TERM_TYPE_VIEW: {
			if (!qualified_names_equal(
					left_term->as.type_view.identity,
					right_term->as.type_view.identity
				)) {
				*p_equal = 0;
				return 0;
			}
			struct prototype_qualified_name left_identity;
			struct prototype_qualified_name right_identity;
			uint32_t left_arguments[16];
			uint32_t right_arguments[16];
			uint32_t left_argument_count;
			uint32_t right_argument_count;
			if (prototype_term_nominal_type_instance_info(
					db,
					left_whnf,
					&left_identity,
					left_arguments,
					&left_argument_count
				) != 0 || prototype_term_nominal_type_instance_info(
					db,
					right_whnf,
					&right_identity,
					right_arguments,
					&right_argument_count
				) != 0 || !qualified_names_equal(left_identity, right_identity) ||
				left_argument_count != right_argument_count) {
				return -1;
			}
			/* TYPE_VIEW is the nominal boundary for a declaration application.
			 * Its core and source spines are two representations of these same
			 * arguments. Re-normalizing a complete indexed core application would
			 * materialize this TYPE_VIEW again, so compare the authoritative
			 * application arguments exactly once. */
			for (uint32_t i = 0; i < left_argument_count; ++i) {
				int equal = 0;
				if (conversion_equal_at_depth(
						db,
						definitions,
						options,
						scope,
						left_arguments[i],
						right_arguments[i],
						&equal,
						depth + 1
					) != 0 || !equal) {
					return 0;
				}
			}
			*p_equal = 1;
			return 0;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
			*p_equal = 1;
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			*p_equal = term_scope_binders_equal(
				scope,
				left_term->as.effect_row_var.binding_id,
				right_term->as.effect_row_var.binding_id
			);
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			int equal = 0;
			if (conversion_equal_at_depth(
					db, definitions, options,
					scope,
					left_term->as.effect_row_union.left,
					right_term->as.effect_row_union.left,
					&equal, depth + 1
				) != 0 || !equal) {
				return 0;
			}
			return conversion_equal_at_depth(
				db, definitions, options,
				scope,
				left_term->as.effect_row_union.right,
				right_term->as.effect_row_union.right,
				p_equal, depth + 1
			);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			struct term_scope_env body_scope = *scope;
			if (conversion_scope_push_binder(
					options,
					&body_scope,
					left_term->as.effect_row_forall.binding_id,
					right_term->as.effect_row_forall.binding_id
				) != 0) {
				return -1;
			}
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				&body_scope,
				left_term->as.effect_row_forall.body,
				right_term->as.effect_row_forall.body,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			if (left_term->as.effect_row_operation.operation_id !=
				right_term->as.effect_row_operation.operation_id) {
				*p_equal = 0;
				return 0;
			}
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.effect_row_operation.latent_row,
				right_term->as.effect_row_operation.latent_row,
				p_equal,
				depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
			int equal = 0;
			if (left_term->as.computation_type.totality !=
				right_term->as.computation_type.totality) {
				*p_equal = 0;
				return 0;
			}
			if (conversion_equal_at_depth(
					db,
					definitions,
					options,
					scope,
					left_term->as.computation_type.label,
					right_term->as.computation_type.label,
					&equal,
					depth + 1
				) != 0 || !equal) {
				return 0;
			}
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.computation_type.result,
				right_term->as.computation_type.result,
				p_equal,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_THUNK_TYPE:
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.thunk_type.computation,
				right_term->as.thunk_type.computation,
				p_equal,
				depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.return_term.value,
				right_term->as.return_term.value,
				p_equal,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return conversion_equal_at_depth(
				db,
				definitions,
				options,
				scope,
				left_term->as.thunk.computation,
				right_term->as.thunk.computation,
				p_equal,
				depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			int equal = 0;
			if (conversion_equal_at_depth(
					db, definitions, options,
					scope,
					left_term->as.computation_fold.computation,
					right_term->as.computation_fold.computation,
					&equal, depth + 1
				) != 0 || !equal || conversion_equal_at_depth(
					db, definitions, options,
					scope,
					left_term->as.computation_fold.return_clause,
					right_term->as.computation_fold.return_clause,
					&equal, depth + 1
				) != 0 || !equal || left_term->as.computation_fold.clause_count !=
					right_term->as.computation_fold.clause_count) {
				*p_equal = 0;
				return 0;
			}
			for (uint32_t i = 0; i < left_term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* left_clause =
					&db->computation_fold_clauses[left_term->as.computation_fold.first_clause + i];
				const struct prototype_computation_fold_clause* right_clause =
					&db->computation_fold_clauses[right_term->as.computation_fold.first_clause + i];
				if (conversion_equal_at_depth(
						db, definitions, options,
						scope,
						left_clause->operation, right_clause->operation, &equal, depth + 1
					) != 0 || !equal || conversion_equal_at_depth(
						db, definitions, options,
						scope,
						left_clause->body, right_clause->body, &equal, depth + 1
					) != 0 || !equal) {
					*p_equal = 0;
					return 0;
				}
			}
			*p_equal = 1;
			return 0;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST: {
			int equal = 0;
			if (conversion_equal_at_depth(
					db, definitions, options,
					scope,
					left_term->as.operation_request.operation,
					right_term->as.operation_request.operation,
					&equal, depth + 1
				) != 0 || !equal || conversion_equal_at_depth(
					db, definitions, options,
					scope,
					left_term->as.operation_request.argument,
					right_term->as.operation_request.argument,
					&equal, depth + 1
				) != 0 || !equal) {
				*p_equal = 0;
				return 0;
			}
			return conversion_equal_at_depth(
				db, definitions, options,
				scope,
				left_term->as.operation_request.continuation,
				right_term->as.operation_request.continuation,
				p_equal, depth + 1
			);
		}
		default:
			*p_equal = left_whnf == right_whnf;
			return 0;
	}
}

static void conversion_result_from_normalization_status(
	struct prototype_term_conversion_result* result,
	int normalization_status,
	int normalization_reason
) {
	if (!result) {
		return;
	}
	result->reason = normalization_reason;
	switch (normalization_status) {
		case PROTOTYPE_TERM_NORMALIZATION_STATUS_BLOCKED_EFFECT:
			result->status = PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT;
			break;
		case PROTOTYPE_TERM_NORMALIZATION_STATUS_EXHAUSTED:
			result->status = PROTOTYPE_TERM_CONVERSION_EXHAUSTED;
			break;
		case PROTOTYPE_TERM_NORMALIZATION_STATUS_INVALID:
			result->status = PROTOTYPE_TERM_CONVERSION_INVALID;
			if (result->reason == PROTOTYPE_TERM_CONVERSION_REASON_NONE) {
				result->reason = PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH;
			}
			break;
		default:
			result->status = PROTOTYPE_TERM_CONVERSION_INVALID;
			result->reason = PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH;
			break;
	}
}

const char* prototype_term_conversion_status_name(int status) {
	switch (status) {
		case PROTOTYPE_TERM_CONVERSION_EQUAL:
			return "equal";
		case PROTOTYPE_TERM_CONVERSION_NOT_EQUAL:
			return "not-equal";
		case PROTOTYPE_TERM_CONVERSION_RESIDUAL:
			return "residual";
		case PROTOTYPE_TERM_CONVERSION_BLOCKED_EFFECT:
			return "blocked-effect";
		case PROTOTYPE_TERM_CONVERSION_EXHAUSTED:
			return "exhausted";
		case PROTOTYPE_TERM_CONVERSION_INVALID:
			return "invalid";
		default:
			return "unknown";
	}
}

const char* prototype_term_conversion_reason_name(int reason) {
	switch (reason) {
		case PROTOTYPE_TERM_CONVERSION_REASON_NONE:
			return "none";
		case PROTOTYPE_TERM_CONVERSION_REASON_NEUTRAL:
			return "neutral";
		case PROTOTYPE_TERM_CONVERSION_REASON_OPAQUE_DEFINITION:
			return "opaque-definition";
		case PROTOTYPE_TERM_CONVERSION_REASON_UNSUPPORTED_RULE:
			return "unsupported-rule";
		case PROTOTYPE_TERM_CONVERSION_REASON_EFFECT_REQUEST:
			return "effect-request";
		case PROTOTYPE_TERM_CONVERSION_REASON_STEP_LIMIT:
			return "step-limit";
		case PROTOTYPE_TERM_CONVERSION_REASON_DEPTH_LIMIT:
			return "depth-limit";
		case PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH:
			return "malformed-graph";
		default:
			return "unknown";
	}
}

int prototype_term_compare_with_options(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	struct prototype_term_conversion_result* p_result
) {
	int normalization_status = PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE;
	int normalization_reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
	uint64_t steps_remaining = step_limit;
	uint64_t steps_used = 0;
	int equal = 0;

	if (!p_result) {
		return -1;
	}
	memset(p_result, 0, sizeof(*p_result));
	p_result->status = PROTOTYPE_TERM_CONVERSION_INVALID;
	p_result->reason = PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH;
	p_result->profile = normalization_profile_from_options(definitions, options);
	p_result->left = left;
	p_result->right = right;
	p_result->left_observation = left;
	p_result->right_observation = right;
	p_result->step_limit = step_limit;
	p_result->graph_revision = db ? db->normalization_graph_revision : 0;
	if (!db || left >= db->term_count || right >= db->term_count) {
		return 0;
	}
	if (left == right) {
		p_result->status = PROTOTYPE_TERM_CONVERSION_EQUAL;
		p_result->reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
		p_result->left_observation = left;
		p_result->right_observation = right;
		return 0;
	}

	options.p_normalization_status = &normalization_status;
	options.p_normalization_reason = &normalization_reason;
	options.p_steps_remaining = &steps_remaining;
	options.p_steps_used = &steps_used;
	struct term_scope_env scope;
	memset(&scope, 0, sizeof(scope));
	if (conversion_equal_at_depth(
			db,
			definitions,
			options,
			&scope,
			left,
			right,
			&equal,
			0
		) != 0 || normalization_status != PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE) {
		conversion_result_from_normalization_status(
			p_result,
			normalization_status,
			normalization_reason
		);
	} else {
		p_result->status = equal ?
			PROTOTYPE_TERM_CONVERSION_EQUAL : PROTOTYPE_TERM_CONVERSION_NOT_EQUAL;
		p_result->reason = PROTOTYPE_TERM_CONVERSION_REASON_NONE;
		struct prototype_term_reduction_options observation_options = options;
		observation_options.p_normalization_status = NULL;
		observation_options.p_normalization_reason = NULL;
		observation_options.p_steps_remaining = NULL;
		observation_options.p_steps_used = NULL;
		if (prototype_term_perform_with_options(
				db,
				definitions,
				observation_options,
				left,
				&p_result->left_observation
			) != 0 || prototype_term_perform_with_options(
				db,
				definitions,
				observation_options,
				right,
				&p_result->right_observation
			) != 0) {
			p_result->status = PROTOTYPE_TERM_CONVERSION_INVALID;
			p_result->reason = PROTOTYPE_TERM_CONVERSION_REASON_MALFORMED_GRAPH;
		}
	}
	p_result->steps_used = steps_used;
	p_result->graph_revision = db->normalization_graph_revision;
	return 0;
}

static int int_literal_fits_int32(int64_t value) {
	return value >= INT32_MIN && value <= INT32_MAX;
}

static int64_t int32_from_bits(uint32_t bits) {
	if (bits <= (uint32_t)INT32_MAX) {
		return (int64_t)bits;
	}
	return -1 - (int64_t)(UINT32_MAX - bits);
}

static int64_t int64_from_bits(uint64_t bits) {
	if (bits <= (uint64_t)INT64_MAX) {
		return (int64_t)bits;
	}
	return -1 - (int64_t)(UINT64_MAX - bits);
}

static int host_oracle_is_integer_arithmetic(int oracle_kind) {
	return oracle_kind == PROTOTYPE_HOST_ORACLE_INT_ADD ||
		oracle_kind == PROTOTYPE_HOST_ORACLE_INT_SUB ||
		oracle_kind == PROTOTYPE_HOST_ORACLE_INT_MUL ||
		oracle_kind == PROTOTYPE_HOST_ORACLE_INT_NEG;
}

static int int_pure_primitive_arity(int primitive_id) {
	const struct prototype_core_pure_primitive_declaration* declaration =
		prototype_core_pure_primitive_declaration(primitive_id);
	const struct host_pure_primitive_implementation* implementation =
		host_pure_primitive_implementation(primitive_id);
	if (!declaration || !implementation ||
		!host_oracle_is_integer_arithmetic(implementation->oracle_kind)) {
		return 0;
	}
	return (int)declaration->arity;
}

static int execute_int_pure_primitive(
	struct prototype_term_db* db,
	int primitive_id,
	const int64_t* arguments,
	uint32_t argument_count,
	uint32_t* p_ret
) {
	if (!db || !arguments || !p_ret ||
		argument_count != (uint32_t)int_pure_primitive_arity(primitive_id)) {
		return -1;
	}
	const struct prototype_core_pure_primitive_declaration* declaration =
		prototype_core_pure_primitive_declaration(primitive_id);
	const struct host_pure_primitive_implementation* implementation =
		host_pure_primitive_implementation(primitive_id);
	if (!declaration || !implementation ||
		!host_oracle_is_integer_arithmetic(implementation->oracle_kind)) {
		return -1;
	}

	int result_width = prototype_term_host_type_bit_width(
		declaration->result_host_type
	);
	if (result_width == 32) {
		for (uint32_t i = 0; i < argument_count; ++i) {
			if (!int_literal_fits_int32(arguments[i])) {
				return -1;
			}
		}
		uint32_t result_bits;
		switch (implementation->oracle_kind) {
			case PROTOTYPE_HOST_ORACLE_INT_ADD:
				result_bits = (uint32_t)arguments[0] + (uint32_t)arguments[1];
				break;
			case PROTOTYPE_HOST_ORACLE_INT_SUB:
				result_bits = (uint32_t)arguments[0] - (uint32_t)arguments[1];
				break;
			case PROTOTYPE_HOST_ORACLE_INT_MUL:
				result_bits = (uint32_t)arguments[0] * (uint32_t)arguments[1];
				break;
			case PROTOTYPE_HOST_ORACLE_INT_NEG:
				result_bits = 0u - (uint32_t)arguments[0];
				break;
			default:
				return -1;
		}
		return prototype_term_int_literal(db, int32_from_bits(result_bits), p_ret);
	}
	if (result_width != 64) {
		return -1;
	}

	uint64_t result_bits;
	switch (implementation->oracle_kind) {
		case PROTOTYPE_HOST_ORACLE_INT_ADD:
			result_bits = (uint64_t)arguments[0] + (uint64_t)arguments[1];
			break;
		case PROTOTYPE_HOST_ORACLE_INT_SUB:
			result_bits = (uint64_t)arguments[0] - (uint64_t)arguments[1];
			break;
		case PROTOTYPE_HOST_ORACLE_INT_MUL:
			result_bits = (uint64_t)arguments[0] * (uint64_t)arguments[1];
			break;
		case PROTOTYPE_HOST_ORACLE_INT_NEG:
			result_bits = 0ull - (uint64_t)arguments[0];
			break;
		default:
			return -1;
	}
	return prototype_term_int_literal(db, int64_from_bits(result_bits), p_ret);
}

static int rebuild_app_spine(
	struct prototype_term_db* db,
	uint32_t head,
	const uint32_t* arguments,
	uint32_t argument_count,
	uint32_t* p_ret
) {
	if (!db || !p_ret || (argument_count > 0 && !arguments)) {
		return -1;
	}
	uint32_t current = head;
	for (uint32_t i = 0; i < argument_count; ++i) {
		if (prototype_term_app(db, current, arguments[i], &current) != 0) {
			return -1;
		}
	}
	*p_ret = current;
	return 0;
}

static int host_effect_allowed(
	struct prototype_term_reduction_options options,
	unsigned effects
) {
	if (effects == PROTOTYPE_HOST_EFFECT_NONE) {
		/* This function is reached only through OPERATION_REQUEST dispatch. */
		return 1;
	}
	return (options.flags & PROTOTYPE_TERM_PERFORM_HOST_EFFECT) != 0 &&
		(options.effect_capabilities & effects) == effects;
}

static int reduce_host_pure_primitive_rebuild_if_changed(
	struct prototype_term_db* db,
	uint32_t head,
	const uint32_t* args,
	uint32_t arg_count,
	int changed,
	uint32_t original,
	uint32_t* p_ret
) {
	if (!changed) {
		*p_ret = original;
		return 0;
	}
	return rebuild_app_spine(db, head, args, arg_count, p_ret) == 0 ? 1 : -1;
}

static int reduce_host_pure_primitive_step(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
) {
	if (!db || !p_ret || term_id >= db->term_count) {
		return -1;
	}
	if (depth == 0 || db->terms[term_id].tag != PROTOTYPE_TERM_APP) {
		*p_ret = term_id;
		return 0;
	}

	uint32_t head;
	uint32_t args[64];
	uint32_t arg_count;
	decompose_app(db, term_id, &head, args, &arg_count);
	if (head >= db->term_count ||
		db->terms[head].tag != PROTOTYPE_TERM_PURE_PRIMITIVE) {
		*p_ret = term_id;
		return 0;
	}

	int primitive_id = db->terms[head].as.pure_primitive.primitive_id;
	const struct prototype_core_pure_primitive_declaration* declaration =
		prototype_core_pure_primitive_declaration(primitive_id);
	const struct host_pure_primitive_implementation* implementation =
		host_pure_primitive_implementation(primitive_id);
	if (!declaration || !implementation || arg_count != declaration->arity) {
		*p_ret = term_id;
		return 0;
	}

	uint32_t reduced_args[64];
	int changed = 0;
	for (uint32_t i = 0; i < arg_count; ++i) {
		if (evaluate_steps(
				db,
				definitions,
				options,
				args[i],
				&reduced_args[i],
				depth - 1
			) != 0) {
			return -1;
		}
		if (reduced_args[i] != args[i]) {
			changed = 1;
		}
	}

	if (host_oracle_is_integer_arithmetic(implementation->oracle_kind)) {
		int64_t int_args[PROTOTYPE_PURE_PRIMITIVE_MAX_ARITY];
		for (uint32_t i = 0; i < arg_count; ++i) {
			if (reduced_args[i] >= db->term_count ||
				db->terms[reduced_args[i]].tag != PROTOTYPE_TERM_INT_LITERAL) {
				return reduce_host_pure_primitive_rebuild_if_changed(
					db,
					head,
					reduced_args,
					arg_count,
					changed,
					term_id,
					p_ret
				);
			}
			int_args[i] = db->terms[reduced_args[i]].as.int_literal.value;
		}
		return execute_int_pure_primitive(
			db,
			primitive_id,
			int_args,
			arg_count,
			p_ret
		) == 0 ? 1 : -1;
	}

	if (!options.symbols) {
		return reduce_host_pure_primitive_rebuild_if_changed(
			db,
			head,
			reduced_args,
			arg_count,
			changed,
			term_id,
			p_ret
		);
	}

	const struct prototype_term_reduction_environment* reduction_environment =
		options.reduction_environment;
	if (!reduction_environment ||
		reduction_environment->system_nat_owner == PROTOTYPE_INVALID_ID ||
		reduction_environment->system_nat_owner >= db->term_count ||
		reduction_environment->system_nat_zero_constructor == PROTOTYPE_INVALID_ID ||
		reduction_environment->system_nat_succ_constructor == PROTOTYPE_INVALID_ID) {
		return reduce_host_pure_primitive_rebuild_if_changed(
			db,
			head,
			reduced_args,
			arg_count,
			changed,
			term_id,
			p_ret
		);
	}
	uint32_t nat_owner = reduction_environment->system_nat_owner;
	uint32_t zero_constructor =
		reduction_environment->system_nat_zero_constructor;
	uint32_t succ_constructor =
		reduction_environment->system_nat_succ_constructor;

	if (implementation->oracle_kind == PROTOTYPE_HOST_ORACLE_TEXT_TO_NAT) {
		if (arg_count != 1 ||
			reduced_args[0] >= db->term_count ||
			db->terms[reduced_args[0]].tag != PROTOTYPE_TERM_TEXT_LITERAL) {
			return reduce_host_pure_primitive_rebuild_if_changed(
				db,
				head,
				reduced_args,
				arg_count,
				changed,
				term_id,
				p_ret
			);
		}
		const char* text = symbol_to_string(
			options.symbols,
			db->terms[reduced_args[0]].as.text_literal.text_symbol_id
		);
		if (!text || *text == '\0') {
			return -1;
		}
		unsigned long value = 0;
		for (const char* p = text; *p; ++p) {
			if (*p < '0' || *p > '9') {
				return -1;
			}
			value = value * 10u + (unsigned long)(*p - '0');
			if (value > 100000u) {
				return -1;
			}
		}
		uint32_t current;
		if (prototype_term_constructor(
				db,
				nat_owner,
				zero_constructor,
				&current
			) != 0) {
			return -1;
		}
		for (unsigned long i = 0; i < value; ++i) {
			uint32_t succ;
			if (prototype_term_constructor(
					db,
					nat_owner,
					succ_constructor,
					&succ
				) != 0 ||
				prototype_term_app(db, succ, current, &current) != 0) {
				return -1;
			}
		}
		*p_ret = current;
		return 1;
	}

	if (implementation->oracle_kind == PROTOTYPE_HOST_ORACLE_NAT_TO_TEXT) {
		if (arg_count != 1) {
			return reduce_host_pure_primitive_rebuild_if_changed(
				db,
				head,
				reduced_args,
				arg_count,
				changed,
				term_id,
				p_ret
			);
		}
		uint32_t current = reduced_args[0];
		unsigned long value = 0;
		for (;;) {
			uint32_t evaluated_current;
			if (evaluate_steps(
					db,
					definitions,
					options,
					current,
					&evaluated_current,
					depth - 1
				) != 0) {
				return -1;
			}
			current = evaluated_current;

			uint32_t nat_head;
			uint32_t nat_args[2];
			uint32_t nat_arg_count;
			decompose_app(db, current, &nat_head, nat_args, &nat_arg_count);
			if (nat_head >= db->term_count ||
				db->terms[nat_head].tag != PROTOTYPE_TERM_CONSTRUCTOR) {
				return reduce_host_pure_primitive_rebuild_if_changed(
					db,
					head,
					reduced_args,
					arg_count,
					changed,
					term_id,
					p_ret
				);
			}
			int same_owner = 0;
			if (prototype_term_core_shape_equal(
					db,
					db->terms[nat_head].as.constructor.owner,
					nat_owner,
					&same_owner
				) != 0) {
				return -1;
			}
			if (!same_owner) {
				return reduce_host_pure_primitive_rebuild_if_changed(
					db,
					head,
					reduced_args,
					arg_count,
					changed,
					term_id,
					p_ret
				);
			}
			if (db->terms[nat_head].as.constructor.constructor_id ==
					zero_constructor &&
				nat_arg_count == 0) {
				break;
			}
			if (db->terms[nat_head].as.constructor.constructor_id !=
					succ_constructor ||
				nat_arg_count != 1) {
				return -1;
			}
			value++;
			if (value > 100000u) {
				return -1;
			}
			current = nat_args[0];
		}
		char buffer[32];
		int len = snprintf(buffer, sizeof(buffer), "%lu", value);
		if (len < 0 || (size_t)len >= sizeof(buffer)) {
			return -1;
		}
		int text_symbol = symbol_intern(options.symbols, buffer, (size_t)len);
		if (text_symbol < 0 ||
			prototype_term_text_literal(db, text_symbol, p_ret) != 0) {
			return -1;
		}
		return 1;
	}

	return reduce_host_pure_primitive_rebuild_if_changed(
		db,
		head,
		reduced_args,
		arg_count,
		changed,
		term_id,
		p_ret
	);
}

static int default_host_effect_dispatch(
	void* context,
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	const struct prototype_term_reduction_options* options,
	uint32_t operation,
	uint32_t argument,
	uint32_t* p_result,
	unsigned depth
) {
	(void)context;
	if (!options || !db || operation >= db->term_count ||
		db->terms[operation].tag != PROTOTYPE_TERM_EFFECT_OPERATION) {
		return -1;
	}
	int operation_id = db->terms[operation].as.effect_operation.operation_id;
	const struct prototype_core_effect_operation_declaration* declaration =
		prototype_core_effect_operation_declaration(operation_id);
	const struct host_effect_operation_implementation* implementation =
		host_effect_operation_implementation(operation_id);
	if (!declaration || !implementation || declaration->arity != 1 ||
		!host_effect_allowed(*options, declaration->required_host_effects)) {
		return 0;
	}
	uint32_t reduced_argument;
	if (evaluate_steps(
			db,
			definitions,
			*options,
			argument,
			&reduced_argument,
			depth
		) != 0) {
		return -1;
	}
	if (implementation->oracle_kind != PROTOTYPE_HOST_ORACLE_PRINT ||
		reduced_argument >= db->term_count ||
		db->terms[reduced_argument].tag != PROTOTYPE_TERM_TEXT_LITERAL ||
		!options->effect_output || !options->symbols) {
		return 0;
	}
	const char* value = symbol_to_string(
		options->symbols,
		db->terms[reduced_argument].as.text_literal.text_symbol_id
	);
	if (value) {
		fputs(value, options->effect_output);
	}
	if (options->p_effect_performed) {
		*options->p_effect_performed = 1;
	}
	*p_result = reduced_argument;
	return 1;
}

static int dispatch_operation_request_step(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
) {
	if (!db || !p_ret || term_id >= db->term_count || depth == 0 ||
		db->terms[term_id].tag != PROTOTYPE_TERM_OPERATION_REQUEST) {
		return -1;
	}
	const struct prototype_term* request = &db->terms[term_id];
	uint32_t result;
	prototype_term_operation_dispatch_fn dispatch = options.operation_dispatch;
	if (!dispatch) {
		dispatch = default_host_effect_dispatch;
	}
	int status = dispatch(
		options.operation_dispatch_context,
		db,
		definitions,
		&options,
		request->as.operation_request.operation,
		request->as.operation_request.argument,
		&result,
		depth - 1
	);
	if (status <= 0) {
		*p_ret = term_id;
		return status;
	}
	uint32_t resumed;
	if (operation_request_resume(
			db, request->as.operation_request.continuation, result, &resumed
		) != 0) {
		return -1;
	}
	return evaluate_steps(
		db, definitions, options, resumed, p_ret, depth - 1
	);
}

int prototype_term_compare_for_conversion(
	struct prototype_term_db* db,
	const struct prototype_term_definition_env* definitions,
	int profile,
	uint32_t left,
	uint32_t right,
	uint64_t step_limit,
	struct prototype_term_conversion_result* p_result
) {
	struct prototype_term_reduction_options options;
	if (normalization_profile_options(profile, definitions, &options) != 0) {
		if (!p_result) {
			return -1;
		}
		memset(p_result, 0, sizeof(*p_result));
		p_result->status = PROTOTYPE_TERM_CONVERSION_INVALID;
		p_result->reason = PROTOTYPE_TERM_CONVERSION_REASON_UNSUPPORTED_RULE;
		p_result->profile = profile;
		p_result->left = left;
		p_result->right = right;
		p_result->left_observation = PROTOTYPE_INVALID_ID;
		p_result->right_observation = PROTOTYPE_INVALID_ID;
		p_result->step_limit = step_limit;
		p_result->graph_revision = db ? db->normalization_graph_revision : 0;
		return 0;
	}
	if (prototype_term_compare_with_options(
		db,
		definitions,
		options,
		left,
		right,
		step_limit,
		p_result
	) != 0) {
		return -1;
	}
	p_result->profile = profile;
	return 0;
}
