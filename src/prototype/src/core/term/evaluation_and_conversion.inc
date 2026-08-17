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
				term->as.type_view.view_type_id,
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
				return prototype_term_computation_type(db, label, result, p_ret);
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

static int type_instance_type_id(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	uint32_t* p_type_id
) {
	if (!terms || !p_type_id || term_id >= terms->term_count) {
		return -1;
	}
	uint32_t arg_count;
	uint32_t args[16];
	if (prototype_term_type_instance_info(terms, term_id, p_type_id, args, &arg_count) == 0) {
		return 0;
	}
	return -1;
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
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
);

static int reduce_host_pure_primitive_step(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
);

static int dispatch_operation_request_step(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
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

static int conversion_equal_at_depth(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
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

static int conversion_equal_match_case_bodies(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
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
		type_declarations,
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
	struct prototype_type_declaration_db* type_declarations,
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
			type_declarations,
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
			type_declarations,
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
	/* A solver-generated non-recursive Match may retain an internal frame when
	 * the corresponding reduced surface family does not. Pair a missing frame
	 * with INVALID so a vacuous frame is ignored but any IH use is rejected. */
	if ((result_match.as.match.ih_scope_id != PROTOTYPE_INVALID_ID ||
		outer_match.as.match.ih_scope_id != PROTOTYPE_INVALID_ID) &&
		conversion_scope_push_frame(
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
					type_declarations,
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
				&branch_computation
			) != 0 || conversion_equal_match_case_bodies(
				db,
				type_declarations,
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

static int reduction_is_pure_type(
	struct prototype_term_reduction_options options
) {
	return (options.flags & PROTOTYPE_TERM_REDUCE_PURE_TYPE) != 0;
}

static struct prototype_type_declaration_db* reduction_substitution_types(
	struct prototype_type_declaration_db* type_declarations,
	struct prototype_term_reduction_options options
) {
	/* TypeView rematerialization is part of pure-type normalization. Ordinary
	 * beta/iota computation performs only structural Core substitution. */
	return reduction_is_pure_type(options) ? type_declarations : NULL;
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
		case PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF:
			p_options->flags = PROTOTYPE_TERM_REDUCE_BETA |
				PROTOTYPE_TERM_REDUCE_MATCH |
				PROTOTYPE_TERM_REDUCE_INDUCTION |
				PROTOTYPE_TERM_REDUCE_COMPUTATIONS |
				PROTOTYPE_TERM_REDUCE_PURE_TYPE;
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
			PROTOTYPE_TERM_REDUCE_PURE_TYPE)) {
		return PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF;
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
	for (size_t i = 0; i < PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY; ++i) {
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

static struct prototype_term_normalization_cache_entry* normalization_cache_reserve(
	struct prototype_term_db* db,
	int profile,
	uint32_t term_id
) {
	if (!db) {
		return NULL;
	}
	for (size_t i = 0; i < PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY; ++i) {
		struct prototype_term_normalization_cache_entry* entry =
			&db->normalization_cache[i];
		if (entry->state == PROTOTYPE_TERM_NORMALIZATION_CACHE_EMPTY ||
			entry->graph_revision != db->normalization_graph_revision) {
			entry->term_id = term_id;
			entry->result_term_id = PROTOTYPE_INVALID_ID;
			entry->graph_revision = db->normalization_graph_revision;
			entry->profile = profile;
			entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_IN_PROGRESS;
			return entry;
		}
	}
	struct prototype_term_normalization_cache_entry* entry =
		&db->normalization_cache[
			db->normalization_cache_next++ % PROTOTYPE_TERM_NORMALIZATION_CACHE_CAPACITY
		];
	entry->term_id = term_id;
	entry->result_term_id = PROTOTYPE_INVALID_ID;
	entry->graph_revision = db->normalization_graph_revision;
	entry->profile = profile;
	entry->state = PROTOTYPE_TERM_NORMALIZATION_CACHE_IN_PROGRESS;
	return entry;
}

static int constant_match_is_exhaustive(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* types,
	const struct prototype_term* match
) {
	if (!terms || !match || match->tag != PROTOTYPE_TERM_MATCH ||
		match->as.match.case_count == 0 || match->as.match.case_count > 64) {
		return 0;
	}
	const struct prototype_match_case* first_case =
		&terms->cases[match->as.match.first_case];
	if (first_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		first_case->constructor_owner >= terms->term_count) {
		return 0;
	}
	uint32_t constructor_count = 0;
	const struct prototype_term* type_former;
	if (constructor_owner_type_former(
			terms, first_case->constructor_owner, &type_former
		) == 0) {
		constructor_count = type_former->as.type_former.constructor_count;
	} else if (types) {
		uint32_t type_id;
		const struct prototype_term* owner =
			&terms->terms[first_case->constructor_owner];
		if (owner->tag == PROTOTYPE_TERM_TYPE_VIEW) {
			type_id = owner->as.type_view.view_type_id;
		} else if (type_instance_type_id(
				terms, first_case->constructor_owner, &type_id
			) != 0) {
			type_id = PROTOTYPE_INVALID_ID;
		}
		if (type_id < types->type_count) {
			constructor_count =
				types->type_declarations[type_id].constructor_count;
		}
	}
	if (constructor_count != match->as.match.case_count) {
		return 0;
	}
	uint64_t seen = 0;
	for (uint32_t i = 0; i < match->as.match.case_count; ++i) {
		const struct prototype_match_case* match_case =
			&terms->cases[match->as.match.first_case + i];
		if (match_case->constructor_owner != first_case->constructor_owner ||
			match_case->constructor_id >= constructor_count) {
			return 0;
		}
		uint64_t bit = UINT64_C(1) << match_case->constructor_id;
		if (seen & bit) {
			return 0;
		}
		seen |= bit;
	}
	return 1;
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
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
) {
	if (!db || !p_ret || term_id >= db->term_count) {
		return -1;
	}
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
						type_declarations,
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
					type_declarations,
					definitions,
					options,
					term->as.app.function,
					&function,
					depth - 1
				) != 0) {
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
						type_declarations,
						definitions,
						options,
						db->terms[value].as.thunk.computation,
						&function,
						depth - 1
					) != 0) {
					return -1;
				}
			}

			const struct prototype_term* reduced_function = &db->terms[function];
			if ((options.flags & PROTOTYPE_TERM_REDUCE_BETA) &&
				reduced_function->tag == PROTOTYPE_TERM_LAMBDA) {
				uint32_t substituted;
				if (substitute_term(
					db,
					reduction_substitution_types(type_declarations, options),
					reduced_function->as.lambda.body,
					reduced_function->as.lambda.binding_id,
					term->as.app.argument,
					&substituted
				) != 0) {
					return -1;
				}
				return evaluate_steps(
					db,
					type_declarations,
					definitions,
					options,
					substituted,
					p_ret,
					depth - 1
				);
			}

			/* Imported type formers become TYPE_VIEW only after transparent-name
			 * reduction. Materialize their application here just as graph lowering
			 * does for an already-local type former. This is type-level graph
			 * construction, not a runtime APP reduction. */
			if (reduction_is_pure_type(options) && type_declarations) {
				uint32_t type_id;
				uint32_t arguments[16];
				uint32_t argument_count;
				if (prototype_term_type_instance_info(
						db, function, &type_id, arguments, &argument_count
					) == 0 && type_id < type_declarations->type_count &&
					type_declarations->type_declarations[type_id].parameter_count <=
						UINT32_MAX - type_declarations->type_declarations[type_id].index_count &&
					argument_count <
						type_declarations->type_declarations[type_id].parameter_count +
						type_declarations->type_declarations[type_id].index_count) {
					return prototype_term_type_instance_extend(
						db,
						type_declarations,
						function,
						term->as.app.argument,
						p_ret
					);
				}
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
				!reduction_is_pure_type(options)) {
				uint32_t head;
				uint32_t arguments[64];
				uint32_t argument_count;
				decompose_app(db, candidate, &head, arguments, &argument_count);
				if (head < db->term_count &&
					db->terms[head].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
					const struct prototype_pure_primitive_declaration* declaration =
						prototype_term_pure_primitive_declaration(
							db->terms[head].as.pure_primitive.primitive_id
						);
					if (declaration) {
						uint32_t result;
						int status = reduce_host_pure_primitive_step(
							db, type_declarations, definitions, options, candidate, &result, depth - 1
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
					type_declarations,
					definitions,
					options,
					term->as.match.scrutinee,
					&scrutinee,
					depth - 1
				) != 0) {
				return -1;
			}

			uint32_t head;
			uint32_t args[64];
			uint32_t arg_count;
			decompose_app(db, scrutinee, &head, args, &arg_count);
			if (head >= db->term_count) {
				return -1;
			}

			/* A match whose branches share one body independent of every pattern
			 * binder is a constant eliminator. This is a general reduction rule,
			 * not a typing shortcut: every normalization client observes the same
			 * result even while the scrutinee is neutral. */
			if (constant_match_is_exhaustive(
					db,
					reduction_is_pure_type(options) ? type_declarations : NULL,
					term
				)) {
				const struct prototype_match_case* first_case =
					&db->cases[term->as.match.first_case];
				int uniform = 1;
				for (uint32_t i = 0; uniform &&
					i < term->as.match.case_count; ++i) {
					const struct prototype_match_case* match_case =
						&db->cases[term->as.match.first_case + i];
					uniform = match_case->body == first_case->body;
					for (uint32_t j = 0; uniform &&
						j < match_case->binder_count; ++j) {
						uniform = !prototype_term_contains_free_binding(
							db,
							first_case->body,
							db->case_binders[
								match_case->first_binder + j
							].binding_id
						);
					}
				}
				if (uniform) {
					return evaluate_steps(
						db,
						type_declarations,
						definitions,
						options,
						first_case->body,
						p_ret,
						depth - 1
					);
				}
			}

			if (term_is_constructor_like(&db->terms[head]) != 0) {
				if (scrutinee == term->as.match.scrutinee) {
					*p_ret = term_id;
					return 0;
				}
				struct prototype_match_case_input case_inputs[64];
				struct prototype_case_binder binder_storage[256];
				uint32_t binder_cursor = 0;
				if (term->as.match.case_count > 64) {
					return -1;
				}
				for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
					const struct prototype_match_case* old_case =
						&db->cases[term->as.match.first_case + i];
					if (binder_cursor + old_case->binder_count > 256) {
						return -1;
					}
					for (uint32_t j = 0; j < old_case->binder_count; ++j) {
						binder_storage[binder_cursor + j] =
							db->case_binders[old_case->first_binder + j];
					}
					case_inputs[i].case_label_symbol_id =
						db->case_label_symbols[term->as.match.first_case + i];
					case_inputs[i].constructor_owner = old_case->constructor_owner;
					case_inputs[i].constructor_id = old_case->constructor_id;
					case_inputs[i].binders = &binder_storage[binder_cursor];
					case_inputs[i].binder_count = old_case->binder_count;
					case_inputs[i].body = old_case->body;
					binder_cursor += old_case->binder_count;
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
							reduction_substitution_types(
								type_declarations, options
							),
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
							reduction_substitution_types(
								type_declarations, options
							),
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
					type_declarations,
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
				type_declarations,
				definitions,
				options,
				term->as.induction_hypothesis.argument,
				&argument,
				depth - 1
			) != 0) {
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
				type_declarations,
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
					db, type_declarations, definitions, options, term_id, p_ret, depth - 1
				);
			}
			if (reduction_is_pure_type(options)) {
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
				(reduction_is_pure_type(options) && term->as.computation_fold.clause_count > 0)) {
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
					db, type_declarations, definitions, inner_options,
					term->as.computation_fold.computation, &computation, depth - 1
				) != 0) {
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
					db, type_declarations, definitions, options, applied, p_ret, depth - 1
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
					db, type_declarations, definitions, options, applied, p_ret, depth - 1
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
					db, type_declarations, definitions, options, applied, p_ret, depth - 1
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
						db, type_declarations, definitions, options,
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
					type_declarations,
					definitions,
					options,
					term->as.force.value,
					&value,
					depth - 1
				) != 0) {
				return -1;
			}
			if (value < db->term_count && db->terms[value].tag == PROTOTYPE_TERM_THUNK) {
				uint32_t computation = db->terms[value].as.thunk.computation;
				if (reduction_is_pure_type(options)) {
					uint32_t pure_computation;
					if (evaluate_steps(
							db,
							type_declarations,
							definitions,
							options,
							computation,
							&pure_computation,
							depth - 1
						) != 0) {
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
						type_declarations,
						definitions,
						options,
						pure_computation,
						p_ret,
						depth - 1
					);
				}
				return evaluate_steps(
					db,
					type_declarations,
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
	struct prototype_type_declaration_db* type_declarations,
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
		type_declarations,
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
	struct prototype_type_declaration_db* type_declarations,
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
	if (!db || !type_declarations || term_id >= db->term_count ||
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
			type_declarations,
			definitions,
			options,
			term_id,
			&result_term,
			PROTOTYPE_EVALUATION_DEPTH_LIMIT
		) != 0) {
		p_result->status = status;
		p_result->steps_used = steps_used;
		return 0;
	}
	p_result->status = status;
	p_result->term_id = result_term;
	p_result->steps_used = steps_used;
	return 0;
}

int prototype_term_perform_with_options(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
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
			type_declarations,
			definitions,
			options,
			term_id,
			p_ret,
			PROTOTYPE_EVALUATION_DEPTH_LIMIT
		);
	}

	uint32_t cached;
	if (normalization_cache_lookup(db, profile, term_id, &cached)) {
		db->normalization_cache_stats.hit_count++;
		*p_ret = cached;
		return 0;
	}
	db->normalization_cache_stats.miss_count++;
	struct prototype_term_normalization_cache_entry* entry =
		normalization_cache_reserve(db, profile, term_id);
	uint64_t entry_revision = db ? db->normalization_graph_revision : 0;
	int status = evaluate_steps(
		db,
		type_declarations,
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
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret,
	unsigned depth
) {
	uint32_t whnf;
	if (!db || !p_ret || term_id >= db->term_count || depth == 0 ||
		prototype_term_perform_with_options(
			db, type_declarations, definitions, options, term_id, &whnf
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
					db, type_declarations, definitions, options,
					term->as.app.function, &function, depth - 1
				) != 0 ||
				normalize_term_at_depth(
					db, type_declarations, definitions, options,
					term->as.app.argument, &argument, depth - 1
				) != 0 ||
				prototype_term_app(db, function, argument, &rebuilt) != 0) {
				return -1;
			}
			return rebuilt == whnf ? (*p_ret = rebuilt, 0) :
				normalize_term_at_depth(
					db, type_declarations, definitions, options, rebuilt, p_ret, depth - 1
				);
		}
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t body;
			if (normalize_term_at_depth(
					db, type_declarations, definitions, options,
					term->as.lambda.body, &body, depth - 1
				) != 0) {
				return -1;
			}
			return prototype_term_lambda(db, term->as.lambda.binding_id, body, p_ret);
		}
		case PROTOTYPE_TERM_PI: {
			uint32_t domain;
			uint32_t codomain_family;
			if (normalize_term_at_depth(
					db, type_declarations, definitions, options,
					term->as.pi.domain, &domain, depth - 1
				) != 0 ||
				normalize_term_at_depth(
					db, type_declarations, definitions, options,
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
					db, type_declarations, definitions, options,
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
						db, type_declarations, definitions, options,
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
					db, type_declarations, definitions, options, rebuilt, p_ret, depth - 1
				);
		}
		case PROTOTYPE_TERM_DIMENSION_ACTION: {
			uint32_t source;
			if (normalize_term_at_depth(
					db,
					type_declarations,
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
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_definition_env* definitions,
	struct prototype_term_reduction_options options,
	uint32_t term_id,
	uint32_t* p_ret
) {
	return normalize_term_at_depth(
		db,
		type_declarations,
		definitions,
		normalize_reduction_options(options),
		term_id,
		p_ret,
		PROTOTYPE_EVALUATION_DEPTH_LIMIT
	);
}

static int conversion_equal_at_depth(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
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
		if (!term_scope_frames_equal(
				scope,
				db->terms[left].as.induction_hypothesis.ih_scope_id,
				db->terms[right].as.induction_hypothesis.ih_scope_id,
				&frame_bound
			)) {
			return 0;
		}
		return conversion_equal_at_depth(
			db,
			type_declarations,
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
			db, type_declarations, definitions, whnf_options, left, &left_whnf
		) != 0 || prototype_term_perform_with_options(
			db, type_declarations, definitions, whnf_options, right, &right_whnf
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
		if (left_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE &&
			right_term->tag == PROTOTYPE_TERM_MATCH) {
			return conversion_equal_computation_match(
				db, type_declarations, definitions, options,
				scope, left_whnf, right_whnf, p_equal, depth + 1
			);
		}
		if (left_term->tag == PROTOTYPE_TERM_MATCH &&
			right_term->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return conversion_equal_computation_match(
				db, type_declarations, definitions, options,
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
					type_declarations,
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
				type_declarations,
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
				type_declarations,
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
					type_declarations,
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
				type_declarations,
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
					type_declarations,
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
			/* Frame presence alone is internal metadata. The INVALID pairing makes
			 * a one-sided frame observable only through an IH reference. */
			if ((left_term->as.match.ih_scope_id != PROTOTYPE_INVALID_ID ||
				right_term->as.match.ih_scope_id != PROTOTYPE_INVALID_ID) &&
				conversion_scope_push_frame(
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
						type_declarations,
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
				type_declarations,
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
				type_declarations,
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
			if (!type_declarations ||
				left_term->as.type_declaration.type_id >= type_declarations->type_count ||
				right_term->as.type_declaration.type_id >= type_declarations->type_count) {
				return -1;
			}
			if (type_identity_is_stable(left_term->as.type_declaration.identity) ||
				type_identity_is_stable(right_term->as.type_declaration.identity)) {
				*p_equal = type_identity_equal(
					left_term->as.type_declaration.identity,
					right_term->as.type_declaration.identity
				);
			} else {
				*p_equal = left_term->as.type_declaration.type_id ==
					right_term->as.type_declaration.type_id;
			}
			return 0;
		}
		case PROTOTYPE_TERM_TYPE_VIEW: {
			if (type_identity_is_stable(left_term->as.type_view.identity) ||
				type_identity_is_stable(right_term->as.type_view.identity) ?
					!type_identity_equal(
						left_term->as.type_view.identity,
						right_term->as.type_view.identity
					) : left_term->as.type_view.view_type_id !=
						right_term->as.type_view.view_type_id) {
				*p_equal = 0;
				return 0;
			}
			uint32_t left_type_id;
			uint32_t right_type_id;
			uint32_t left_arguments[16];
			uint32_t right_arguments[16];
			uint32_t left_argument_count;
			uint32_t right_argument_count;
			if (prototype_term_type_instance_info(
					db,
					left_whnf,
					&left_type_id,
					left_arguments,
					&left_argument_count
				) != 0 || prototype_term_type_instance_info(
					db,
					right_whnf,
					&right_type_id,
					right_arguments,
					&right_argument_count
				) != 0 || left_argument_count != right_argument_count) {
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
						type_declarations,
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
			(void)left_type_id;
			(void)right_type_id;
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
					db, type_declarations, definitions, options,
					scope,
					left_term->as.effect_row_union.left,
					right_term->as.effect_row_union.left,
					&equal, depth + 1
				) != 0 || !equal) {
				return 0;
			}
			return conversion_equal_at_depth(
				db, type_declarations, definitions, options,
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
				type_declarations,
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
				type_declarations,
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
			if (conversion_equal_at_depth(
					db,
					type_declarations,
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
				type_declarations,
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
				type_declarations,
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
				type_declarations,
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
				type_declarations,
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
					db, type_declarations, definitions, options,
					scope,
					left_term->as.computation_fold.computation,
					right_term->as.computation_fold.computation,
					&equal, depth + 1
				) != 0 || !equal || conversion_equal_at_depth(
					db, type_declarations, definitions, options,
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
						db, type_declarations, definitions, options,
						scope,
						left_clause->operation, right_clause->operation, &equal, depth + 1
					) != 0 || !equal || conversion_equal_at_depth(
						db, type_declarations, definitions, options,
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
					db, type_declarations, definitions, options,
					scope,
					left_term->as.operation_request.operation,
					right_term->as.operation_request.operation,
					&equal, depth + 1
				) != 0 || !equal || conversion_equal_at_depth(
					db, type_declarations, definitions, options,
					scope,
					left_term->as.operation_request.argument,
					right_term->as.operation_request.argument,
					&equal, depth + 1
				) != 0 || !equal) {
				*p_equal = 0;
				return 0;
			}
			return conversion_equal_at_depth(
				db, type_declarations, definitions, options,
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
	struct prototype_type_declaration_db* type_declarations,
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
	if (!db || !type_declarations || left >= db->term_count || right >= db->term_count) {
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
			type_declarations,
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
				type_declarations,
				definitions,
				observation_options,
				left,
				&p_result->left_observation
			) != 0 || prototype_term_perform_with_options(
				db,
				type_declarations,
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
	const struct prototype_pure_primitive_declaration* declaration =
		prototype_term_pure_primitive_declaration(primitive_id);
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
	const struct prototype_pure_primitive_declaration* declaration =
		prototype_term_pure_primitive_declaration(primitive_id);
	const struct host_pure_primitive_implementation* implementation =
		host_pure_primitive_implementation(primitive_id);
	if (!declaration || !implementation ||
		!host_oracle_is_integer_arithmetic(implementation->oracle_kind)) {
		return -1;
	}

	int result_width = prototype_term_host_type_bit_width(declaration->result_type);
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
	struct prototype_type_declaration_db* type_declarations,
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
	const struct prototype_pure_primitive_declaration* declaration =
		prototype_term_pure_primitive_declaration(primitive_id);
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
				type_declarations,
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
					type_declarations,
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
	struct prototype_type_declaration_db* type_declarations,
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
	const struct prototype_effect_operation_declaration* declaration =
		prototype_term_effect_operation_declaration(operation_id);
	const struct host_effect_operation_implementation* implementation =
		host_effect_operation_implementation(operation_id);
	if (!declaration || !implementation || declaration->arity != 1 ||
		!host_effect_allowed(*options, declaration->required_host_effects)) {
		return 0;
	}
	uint32_t reduced_argument;
	if (evaluate_steps(
			db,
			type_declarations,
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
	struct prototype_type_declaration_db* type_declarations,
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
		type_declarations,
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
		db, type_declarations, definitions, options, resumed, p_ret, depth - 1
	);
}

int prototype_term_compare_for_conversion(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
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
		type_declarations,
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

static const char* safe_symbol_name(const struct symbol_table* symbols, int symbol_id) {
	if (symbol_id == PROTOTYPE_BASE_NAMESPACE_ID) {
		return "_";
	}
	const char* name = symbol_to_string(symbols, symbol_id);
	return name ? name : "<unknown>";
}

static void print_escaped_text(FILE* output, const char* text) {
	if (!output || !text) {
		return;
	}
	for (const char* p = text; *p != '\0'; ++p) {
		switch (*p) {
			case '\n':
				fprintf(output, "\\n");
				break;
			case '\r':
				fprintf(output, "\\r");
				break;
			case '\t':
				fprintf(output, "\\t");
				break;
			case '"':
				fprintf(output, "\\\"");
				break;
			case '\\':
				fprintf(output, "\\\\");
				break;
			default:
				fputc(*p, output);
				break;
		}
	}
}

static void print_type_head(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t type_id
) {
	if (type_id >= type_declarations->type_count) {
		fprintf(output, "<bad-type:%u>", type_id);
		return;
	}
	fprintf(output, "%s", safe_symbol_name(symbols, type_declarations->type_declarations[type_id].name_symbol_id));
}

static void print_term_depth_impl(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_environment* environment,
	uint32_t term_id,
	unsigned depth
);

#define print_term_depth(output, symbols, type_declarations, terms, term_id, depth) \
	print_term_depth_impl( \
		output, symbols, type_declarations, terms, environment, term_id, depth \
	)

static void print_term_depth_impl(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_environment* environment,
	uint32_t term_id,
	unsigned depth
) {
	if (!output || !symbols || !environment || !type_declarations || !terms) {
		return;
	}
	if (term_id >= terms->term_count) {
		fprintf(output, "<bad-term:%u>", term_id);
		return;
	}
	if (depth == 0) {
		fprintf(output, "...");
		return;
	}

	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			fprintf(output, "_");
			break;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			fprintf(output, "<external ");
			if (term->as.external_ref.name.namespace_symbol_id >= 0) {
				fprintf(output, "%s.", safe_symbol_name(symbols, term->as.external_ref.name.namespace_symbol_id));
			}
			fprintf(output, "%s>", safe_symbol_name(symbols, term->as.external_ref.name.name_symbol_id));
			break;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			fprintf(output, "#.%s", prototype_intrinsic_namespace_source_name(
				environment,
				PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
				term->as.pure_primitive.primitive_id
			));
			break;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			fprintf(output, "#.%s", prototype_intrinsic_namespace_source_name(
				environment,
				PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
				term->as.effect_operation.operation_id
			));
			break;
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
			fprintf(output, "<relation-type>");
			break;
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
			fprintf(output, "<relation-witness>");
			break;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			fprintf(output, "dimension[%u] ",
				term->as.dimension_action.operator_id);
			print_term_depth(
				output,
				symbols,
				type_declarations,
				terms,
				term->as.dimension_action.source,
				depth - 1
			);
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t type_id;
			if (type_instance_type_id(terms, term->as.constructor.owner, &type_id) != 0 ||
				type_id >= type_declarations->type_count) {
				uint32_t representation_id;
				if (constructor_owner_representation_id(
						terms,
						term->as.constructor.owner,
						&representation_id
					) != 0) {
					fprintf(output, "<bad-constructor>");
				} else {
					fprintf(output, "rep#%u.ordinal#%u", representation_id,
						term->as.constructor.constructor_id);
				}
				break;
			}
			const struct prototype_type_declaration* type = &type_declarations->type_declarations[type_id];
			if (term->as.constructor.constructor_id >= type->constructor_count) {
				fprintf(output, "<bad-constructor>");
				break;
			}
			const struct prototype_type_constructor_declaration* constructor =
				&type_declarations->constructor_declarations[type->first_constructor + term->as.constructor.constructor_id];
			print_term_depth(output, symbols, type_declarations, terms, term->as.constructor.owner, depth - 1);
			fprintf(output, ".%s", safe_symbol_name(symbols, constructor->name_symbol_id));
			break;
		}
		case PROTOTYPE_TERM_APP:
			fprintf(output, "(");
			print_term_depth(output, symbols, type_declarations, terms, term->as.app.function, depth - 1);
			fprintf(output, " ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.app.argument, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_LAMBDA:
			fprintf(output, "(\\_ => ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.lambda.body, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_PI:
			fprintf(output, "PI(");
			print_term_depth(output, symbols, type_declarations, terms, term->as.pi.domain, depth - 1);
			fprintf(output, ", ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.pi.codomain_family, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_MATCH:
			fprintf(output, "match ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.match.scrutinee, depth - 1);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&terms->cases[term->as.match.first_case + i];
				fprintf(
					output,
					" @%s => ",
					safe_symbol_name(symbols, terms->case_label_symbols[term->as.match.first_case + i])
				);
				print_term_depth(output, symbols, type_declarations, terms, match_case->body, depth - 1);
			}
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			fprintf(output, "TYPE_FORMER(rep#%u)", term->as.type_former.representation_id);
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			fprintf(output, "TYPE_DECLARATION(");
			print_type_head(output, symbols, type_declarations, term->as.type_declaration.type_id);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			fprintf(output, "TYPE_VIEW(");
			print_type_head(output, symbols, type_declarations, term->as.type_view.view_type_id);
			fprintf(output, ", core=");
			print_term_depth(output, symbols, type_declarations, terms, term->as.type_view.core, depth - 1);
			fprintf(output, ", source=");
			print_term_depth(output, symbols, type_declarations, terms, term->as.type_view.source, depth - 1);
			fprintf(output, ")");
			break;
				case PROTOTYPE_TERM_UNIVERSE_VAR:
				fprintf(output, "@?u%u", term->as.universe_var.level_var);
				break;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				fprintf(output, "#.Text");
				break;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				fprintf(output, "#\"");
				print_escaped_text(output, safe_symbol_name(symbols, term->as.text_literal.text_symbol_id));
				fprintf(output, "\"");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
				fprintf(output, "#.Int");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				fprintf(output, "#.Int64");
				break;
			case PROTOTYPE_TERM_INT_LITERAL:
				fprintf(output, "#%" PRId64, term->as.int_literal.value);
				break;
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
					fprintf(output, "*");
					print_term_depth(output, symbols, type_declarations, terms, term->as.induction_hypothesis.argument, depth - 1);
					break;
				case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
					fprintf(output, "Terminal");
					break;
				case PROTOTYPE_TERM_EFFECT_ROW_VAR:
					fprintf(output, "effect#%u", term->as.effect_row_var.binding_id);
					break;
				case PROTOTYPE_TERM_EFFECT_ROW_UNION:
					fprintf(output, "EffectUnion(");
					print_term_depth(output, symbols, type_declarations, terms,
						term->as.effect_row_union.left, depth - 1);
					fprintf(output, ", ");
					print_term_depth(output, symbols, type_declarations, terms,
						term->as.effect_row_union.right, depth - 1);
					fprintf(output, ")");
					break;
				case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
					fprintf(output, "EffectForall(effect#%u, ",
						term->as.effect_row_forall.binding_id);
					print_term_depth(output, symbols, type_declarations, terms,
						term->as.effect_row_forall.body, depth - 1);
					fprintf(output, ")");
					break;
				case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
					fprintf(output, "EffectOperation(%d, ",
						term->as.effect_row_operation.operation_id);
					print_term_depth(output, symbols, type_declarations, terms,
						term->as.effect_row_operation.latent_row, depth - 1);
					fprintf(output, ")");
					break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			fprintf(output, "Comp(");
					print_term_depth(output, symbols, type_declarations, terms, term->as.computation_type.label, depth - 1);
					fprintf(output, ", ");
					print_term_depth(output, symbols, type_declarations, terms, term->as.computation_type.result, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			fprintf(output, "Thunk(");
			print_term_depth(output, symbols, type_declarations, terms, term->as.thunk_type.computation, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_RETURN:
			fprintf(output, "return ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.return_term.value, depth - 1);
			break;
		case PROTOTYPE_TERM_THUNK:
			fprintf(output, "thunk ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.thunk.computation, depth - 1);
			break;
		case PROTOTYPE_TERM_FORCE:
			fprintf(output, "force ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.force.value, depth - 1);
			break;
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			fprintf(output, "fold ");
			print_term_depth(output, symbols, type_declarations, terms,
				term->as.computation_fold.computation, depth - 1);
			fprintf(output, " return ");
			print_term_depth(output, symbols, type_declarations, terms,
				term->as.computation_fold.return_clause, depth - 1);
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				fprintf(output, " operation ");
				print_term_depth(output, symbols, type_declarations, terms,
					clause->operation, depth - 1);
				fprintf(output, " => ");
				print_term_depth(output, symbols, type_declarations, terms,
					clause->body, depth - 1);
			}
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			fprintf(output, "request ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.operation_request.operation, depth - 1);
			fprintf(output, " ");
			print_term_depth(output, symbols, type_declarations, terms, term->as.operation_request.argument, depth - 1);
			break;
		default:
				fprintf(output, "<unknown-term>");
				break;
	}
}

void prototype_term_print(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	print_term_depth(output, symbols, type_declarations, terms, term_id, 64);
}

#undef print_term_depth

static void print_term_debug_depth_impl(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_environment* environment,
	uint32_t term_id,
	unsigned depth
);

#define print_term_debug_depth(output, symbols, type_declarations, terms, term_id, depth) \
	print_term_debug_depth_impl( \
		output, symbols, type_declarations, terms, environment, term_id, depth \
	)

static void print_constructor_name(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_environment* environment,
	const struct prototype_term* term
) {
	uint32_t type_id;
	if (type_instance_type_id(terms, term->as.constructor.owner, &type_id) != 0 ||
		type_id >= type_declarations->type_count) {
		uint32_t representation_id;
		if (constructor_owner_representation_id(
				terms,
				term->as.constructor.owner,
				&representation_id
			) != 0) {
			fprintf(output, "<bad-constructor>");
		} else {
			fprintf(output, "rep#%u.ordinal#%u", representation_id,
				term->as.constructor.constructor_id);
		}
		return;
	}

	const struct prototype_type_declaration* type = &type_declarations->type_declarations[type_id];
	if (term->as.constructor.constructor_id >= type->constructor_count) {
		fprintf(output, "<bad-constructor>");
		return;
	}

	const struct prototype_type_constructor_declaration* constructor =
		&type_declarations->constructor_declarations[type->first_constructor + term->as.constructor.constructor_id];
	print_term_debug_depth(output, symbols, type_declarations, terms, term->as.constructor.owner, 32);
	fprintf(output, ".%s", safe_symbol_name(symbols, constructor->name_symbol_id));
}

static void print_term_debug_depth_impl(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	const struct prototype_intrinsic_environment* environment,
	uint32_t term_id,
	unsigned depth
) {
	if (!output || !symbols || !environment || !type_declarations || !terms) {
		return;
	}
	if (term_id >= terms->term_count) {
		fprintf(output, "BAD_TERM(%u)", term_id);
		return;
	}
	if (depth == 0) {
		fprintf(output, "...");
		return;
	}

	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			fprintf(output, "VAR(_#%u)", term->as.var.binding_id);
			break;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			fprintf(output, "EXTERNAL_REF(");
			if (term->as.external_ref.name.namespace_symbol_id >= 0) {
				fprintf(output, "%s.", safe_symbol_name(symbols, term->as.external_ref.name.namespace_symbol_id));
			}
			fprintf(output, "%s)", safe_symbol_name(symbols, term->as.external_ref.name.name_symbol_id));
			break;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			fprintf(output, "PURE_PRIMITIVE(%s", prototype_intrinsic_namespace_source_name(
				environment,
				PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_PURE_PRIMITIVE,
				term->as.pure_primitive.primitive_id
			));
			if (term->as.pure_primitive.type_symbol_id >= 0) {
				fprintf(output, ":%s", safe_symbol_name(symbols, term->as.pure_primitive.type_symbol_id));
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			fprintf(output, "EFFECT_OPERATION(%s)",
				prototype_intrinsic_namespace_source_name(
					environment,
					PROTOTYPE_INTRINSIC_NAMESPACE_BINDING_EFFECT_OPERATION,
					term->as.effect_operation.operation_id
				));
			break;
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
			fprintf(output, "RELATION_TYPE_FORMER");
			break;
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
			fprintf(output, "RELATION_WITNESS_FORMER");
			break;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			fprintf(output, "DIMENSION_ACTION(op#%u, ",
				term->as.dimension_action.operator_id);
			print_term_debug_depth(
				output,
				symbols,
				type_declarations,
				terms,
				term->as.dimension_action.source,
				depth - 1
			);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			fprintf(output, "CONSTRUCTOR(");
			print_constructor_name(
				output, symbols, type_declarations, terms, environment, term
			);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_APP:
			fprintf(output, "APP(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.app.function, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.app.argument, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_LAMBDA:
			fprintf(output,
				"LAMBDA(_#%u, ",
				term->as.lambda.binding_id);
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.lambda.body, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_PI:
			fprintf(output, "PI(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.pi.domain, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.pi.codomain_family, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_MATCH:
			fprintf(output, "MATCH(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.match.scrutinee, depth - 1);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&terms->cases[term->as.match.first_case + i];
				fprintf(
					output,
					", CASE(%s",
					safe_symbol_name(symbols, terms->case_label_symbols[term->as.match.first_case + i])
				);
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					struct prototype_case_binder binder = terms->case_binders[match_case->first_binder + j];
					fprintf(output, " _#%u", binder.binding_id);
				}
				fprintf(output, " -> ");
				print_term_debug_depth(output, symbols, type_declarations, terms, match_case->body, depth - 1);
				fprintf(output, ")");
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			fprintf(output, "TYPE_FORMER(rep#%u)", term->as.type_former.representation_id);
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			fprintf(output, "TYPE_DECLARATION(");
			print_type_head(output, symbols, type_declarations, term->as.type_declaration.type_id);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			fprintf(output, "TYPE_VIEW(");
			print_type_head(output, symbols, type_declarations, term->as.type_view.view_type_id);
			fprintf(output, ", core=");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.type_view.core, depth - 1);
			fprintf(output, ", source=");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.type_view.source, depth - 1);
			fprintf(output, ")");
			break;
				case PROTOTYPE_TERM_UNIVERSE_VAR:
				fprintf(output, "UNIVERSE(?u%u)", term->as.universe_var.level_var);
				break;
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				fprintf(output, "PRIMITIVE(Text)");
				break;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				fprintf(output, "TEXT_LITERAL(\"");
				print_escaped_text(output, safe_symbol_name(symbols, term->as.text_literal.text_symbol_id));
				fprintf(output, "\")");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
				fprintf(output, "PRIMITIVE(Int)");
				break;
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				fprintf(output, "PRIMITIVE(Int64)");
				break;
			case PROTOTYPE_TERM_INT_LITERAL:
				fprintf(output, "INT_LITERAL(%" PRId64 ")", term->as.int_literal.value);
				break;
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
					fprintf(output, "INDUCTION_HYPOTHESIS(ih_scope#%u, ",
						term->as.induction_hypothesis.ih_scope_id);
			print_term_debug_depth(
				output,
				symbols,
				type_declarations,
				terms,
				term->as.induction_hypothesis.argument,
				depth - 1
			);
				fprintf(output, ")");
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				fprintf(output, "EFFECT_ROW_EMPTY");
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				fprintf(output, "EFFECT_ROW_VAR(%u)", term->as.effect_row_var.binding_id);
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				fprintf(output, "EFFECT_ROW_UNION(");
				print_term_debug_depth(output, symbols, type_declarations, terms,
					term->as.effect_row_union.left, depth - 1);
				fprintf(output, ", ");
				print_term_debug_depth(output, symbols, type_declarations, terms,
					term->as.effect_row_union.right, depth - 1);
				fprintf(output, ")");
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
				fprintf(output, "EFFECT_ROW_FORALL(%u, ",
					term->as.effect_row_forall.binding_id);
				print_term_debug_depth(output, symbols, type_declarations, terms,
					term->as.effect_row_forall.body, depth - 1);
				fprintf(output, ")");
				break;
			case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
				fprintf(output, "EFFECT_ROW_OPERATION(%d, ",
					term->as.effect_row_operation.operation_id);
				print_term_debug_depth(output, symbols, type_declarations, terms,
					term->as.effect_row_operation.latent_row, depth - 1);
				fprintf(output, ")");
				break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			fprintf(output, "COMPUTATION_TYPE(");
				print_term_debug_depth(output, symbols, type_declarations, terms, term->as.computation_type.label, depth - 1);
				fprintf(output, ", ");
				print_term_debug_depth(output, symbols, type_declarations, terms, term->as.computation_type.result, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			fprintf(output, "Thunk(");
			print_term_debug_depth(
				output, symbols, type_declarations, terms, term->as.thunk_type.computation, depth - 1
			);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_RETURN:
			fprintf(output, "RETURN(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.return_term.value, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_THUNK:
			fprintf(output, "THUNK(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.thunk.computation, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_FORCE:
			fprintf(output, "FORCE(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.force.value, depth - 1);
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			fprintf(output, "COMPUTATION_FOLD(");
			print_term_debug_depth(output, symbols, type_declarations, terms,
				term->as.computation_fold.computation, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms,
				term->as.computation_fold.return_clause, depth - 1);
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				fprintf(output, ", OP_CLAUSE(");
				print_term_debug_depth(output, symbols, type_declarations, terms,
					clause->operation, depth - 1);
				fprintf(output, ", ");
				print_term_debug_depth(output, symbols, type_declarations, terms,
					clause->body, depth - 1);
				fprintf(output, ")");
			}
			fprintf(output, ")");
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			fprintf(output, "OPERATION_REQUEST(");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.operation_request.operation, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.operation_request.argument, depth - 1);
			fprintf(output, ", ");
			print_term_debug_depth(output, symbols, type_declarations, terms, term->as.operation_request.continuation, depth - 1);
			fprintf(output, ")");
			break;
			default:
				fprintf(output, "UNKNOWN_TERM");
				break;
	}
}

void prototype_term_print_debug(
	FILE* output,
	const struct symbol_table* symbols,
	const struct prototype_intrinsic_environment* environment,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	print_term_debug_depth(output, symbols, type_declarations, terms, term_id, 64);
}

#undef print_term_debug_depth
