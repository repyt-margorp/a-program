static int substitute_term(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t replacement,
	uint32_t* p_ret
);

static int substitute_term_internal(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct substitution_context* ctx,
	uint32_t* p_ret
);

static int substitute_term_uncached_internal(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct substitution_context* ctx,
	uint32_t* p_ret
);

static void canonical_hash_mix_u32(uint64_t* p_hash, uint32_t value) {
	*p_hash ^= (uint64_t)value;
	*p_hash *= PROTOTYPE_CANONICAL_HASH_PRIME;
}

static void canonical_hash_mix_tag(uint64_t* p_hash, uint32_t tag) {
	canonical_hash_mix_u32(p_hash, 0x9e3779b9U);
	canonical_hash_mix_u32(p_hash, tag);
}

static int term_scope_env_push_binder(
	struct term_scope_env* env,
	uint32_t left_binder,
	uint32_t right_binder
) {
	if (!env || env->count >= PROTOTYPE_CANONICAL_BINDER_CAPACITY) {
		return -1;
	}
	for (uint32_t i = env->count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->left[index] == left_binder) {
			return env->right[index] == right_binder ? 0 : -1;
		}
		if (env->right[index] == right_binder) {
			return -1;
		}
	}
	env->left[env->count] = left_binder;
	env->right[env->count] = right_binder;
	env->count++;
	return 0;
}

static int term_scope_env_lookup_left_binder(
	const struct term_scope_env* env,
	uint32_t left_binder,
	uint32_t* p_right_binder
) {
	if (!env || !p_right_binder) {
		return 0;
	}
	for (uint32_t i = env->count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->left[index] == left_binder) {
			*p_right_binder = env->right[index];
			return 1;
		}
	}
	return 0;
}

static int term_scope_env_contains_right_binder(
	const struct term_scope_env* env,
	uint32_t right_binder
) {
	if (!env) {
		return 0;
	}
	for (uint32_t i = env->count; i > 0; --i) {
		if (env->right[i - 1] == right_binder) {
			return 1;
		}
	}
	return 0;
}

static int term_scope_binders_equal(
	const struct term_scope_env* env,
	uint32_t left_binder,
	uint32_t right_binder
) {
	uint32_t mapped_right;
	if (term_scope_env_lookup_left_binder(env, left_binder, &mapped_right)) {
		return mapped_right == right_binder;
	}
	if (term_scope_env_contains_right_binder(env, right_binder)) {
		return 0;
	}
	return left_binder == right_binder;
}

static int term_scope_env_push_frame(
	struct term_scope_env* env,
	uint32_t left_frame,
	uint32_t right_frame
) {
	if (!env || env->frame_count >= PROTOTYPE_CANONICAL_BINDER_CAPACITY) {
		return -1;
	}
	for (uint32_t i = env->frame_count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->frame_left[index] == left_frame) {
			return env->frame_right[index] == right_frame ? 0 : -1;
		}
		if (env->frame_right[index] == right_frame) {
			return -1;
		}
	}
	env->frame_left[env->frame_count] = left_frame;
	env->frame_right[env->frame_count] = right_frame;
	env->frame_count++;
	return 0;
}

static int term_scope_frames_equal(
	const struct term_scope_env* env,
	uint32_t left_frame,
	uint32_t right_frame,
	int* p_bound
) {
	if (!env || !p_bound) {
		return 0;
	}
	*p_bound = 0;
	for (uint32_t i = env->frame_count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->frame_left[index] == left_frame) {
			*p_bound = 1;
			return env->frame_right[index] == right_frame;
		}
		if (env->frame_right[index] == right_frame) {
			*p_bound = 1;
			return 0;
		}
	}
	return left_frame == right_frame;
}

static int type_identity_is_stable(struct prototype_qualified_name identity) {
	return identity.namespace_symbol_id >= 0 && identity.name_symbol_id >= 0;
}

static int type_identity_equal(
	struct prototype_qualified_name left,
	struct prototype_qualified_name right
) {
	return type_identity_is_stable(left) && type_identity_is_stable(right) &&
		left.namespace_symbol_id == right.namespace_symbol_id &&
		left.name_symbol_id == right.name_symbol_id;
}

static int shape_terms_equal_at_depth(
	const struct prototype_term_db* db,
	uint32_t left_id,
	uint32_t right_id,
	struct term_scope_env* env,
	int type_view_compare_mode,
	uint32_t depth
);

static int shape_match_cases_equal_at_depth(
	const struct prototype_term_db* db,
	const struct prototype_match_case* left_case,
	const struct prototype_match_case* right_case,
	struct term_scope_env* env,
	int type_view_compare_mode,
	uint32_t depth
) {
	if (left_case->constructor_id != right_case->constructor_id ||
		left_case->binder_count != right_case->binder_count) {
		return 0;
	}
	if (left_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		right_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		if (left_case->constructor_owner != right_case->constructor_owner) {
			return 0;
		}
		/* A case owner selects an operational constructor representation. Its
		 * nominal TypeView belongs to the typed occurrence, not Core Match
		 * topology, so compare the owner through its Core projection. */
		} else if (!shape_terms_equal_at_depth(
				db,
				left_case->constructor_owner,
				right_case->constructor_owner,
				env,
				PROTOTYPE_TYPE_VIEW_COMPARE_CORE,
				depth + 1
			)) {
		return 0;
	}

	uint32_t saved_count = env->count;
	for (uint32_t i = 0; i < left_case->binder_count; ++i) {
		const struct prototype_case_binder* left_binder =
			&db->case_binders[left_case->first_binder + i];
		const struct prototype_case_binder* right_binder =
			&db->case_binders[right_case->first_binder + i];
		if (left_binder->is_recursive != right_binder->is_recursive ||
			term_scope_env_push_binder(
				env, left_binder->binding_id, right_binder->binding_id
			) != 0) {
			env->count = saved_count;
			return 0;
		}
	}
	int equal = shape_terms_equal_at_depth(
		db,
		left_case->body,
		right_case->body,
		env,
		type_view_compare_mode,
		depth + 1
	);
	env->count = saved_count;
	return equal;
}

static uint32_t shape_term_compare_id(
	const struct prototype_term_db* db,
	uint32_t term_id,
	int type_view_compare_mode
) {
	uint32_t current = term_id;
	for (uint32_t depth = 0;
		(type_view_compare_mode == PROTOTYPE_TYPE_VIEW_COMPARE_CORE ||
			type_view_compare_mode == PROTOTYPE_TYPE_VIEW_COMPARE_SOURCE) &&
			current < db->term_count &&
			db->terms[current].tag == PROTOTYPE_TERM_TYPE_VIEW &&
			depth < 32;
		++depth) {
		current = type_view_compare_mode == PROTOTYPE_TYPE_VIEW_COMPARE_CORE ?
			db->terms[current].as.type_view.core :
			db->terms[current].as.type_view.source;
	}
	return current;
}

static int shape_terms_equal_at_depth(
	const struct prototype_term_db* db,
	uint32_t left_id,
	uint32_t right_id,
	struct term_scope_env* env,
	int type_view_compare_mode,
	uint32_t depth
) {
	if (!db || !env ||
		left_id >= db->term_count ||
		right_id >= db->term_count ||
		depth > 256) {
		return 0;
	}
	left_id = shape_term_compare_id(db, left_id, type_view_compare_mode);
	right_id = shape_term_compare_id(db, right_id, type_view_compare_mode);
	if (left_id >= db->term_count || right_id >= db->term_count) {
		return 0;
	}
	const struct prototype_term* left = &db->terms[left_id];
	const struct prototype_term* right = &db->terms[right_id];
	if (left->tag != right->tag) {
		return 0;
	}
	switch (left->tag) {
		case PROTOTYPE_TERM_VAR:
			return term_scope_binders_equal(env, left->as.var.binding_id, right->as.var.binding_id);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return left->as.constructor.constructor_id == right->as.constructor.constructor_id &&
				shape_terms_equal_at_depth(
					db,
					left->as.constructor.owner,
					right->as.constructor.owner,
					env,
					type_view_compare_mode,
					depth + 1
				);
		case PROTOTYPE_TERM_APP:
			return shape_terms_equal_at_depth(
					db,
					left->as.app.function,
					right->as.app.function,
					env,
					type_view_compare_mode,
					depth + 1
				) &&
				shape_terms_equal_at_depth(
					db,
					left->as.app.argument,
					right->as.app.argument,
					env,
					type_view_compare_mode,
					depth + 1
				);
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t saved_count = env->count;
			if (term_scope_env_push_binder(env, left->as.lambda.binding_id, right->as.lambda.binding_id) != 0) {
				env->count = saved_count;
				return 0;
			}
			int equal = shape_terms_equal_at_depth(
				db,
					left->as.lambda.body,
					right->as.lambda.body,
					env,
					type_view_compare_mode,
					depth + 1
				);
			env->count = saved_count;
			return equal;
		}
		case PROTOTYPE_TERM_RETURN:
			return shape_terms_equal_at_depth(
				db,
				left->as.return_term.value,
				right->as.return_term.value,
				env,
				type_view_compare_mode,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return shape_terms_equal_at_depth(
				db,
				left->as.thunk_type.computation,
				right->as.thunk_type.computation,
				env,
				type_view_compare_mode,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return shape_terms_equal_at_depth(
				db,
				left->as.thunk.computation,
				right->as.thunk.computation,
				env,
				type_view_compare_mode,
				depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return shape_terms_equal_at_depth(
				db,
				left->as.force.value,
				right->as.force.value,
				env,
				type_view_compare_mode,
				depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			if (!shape_terms_equal_at_depth(
					db, left->as.computation_fold.computation, right->as.computation_fold.computation,
					env, type_view_compare_mode, depth + 1
				) || !shape_terms_equal_at_depth(
					db, left->as.computation_fold.return_clause, right->as.computation_fold.return_clause,
					env, type_view_compare_mode, depth + 1
				) || left->as.computation_fold.clause_count != right->as.computation_fold.clause_count) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* left_clause =
					&db->computation_fold_clauses[left->as.computation_fold.first_clause + i];
				const struct prototype_computation_fold_clause* right_clause =
					&db->computation_fold_clauses[right->as.computation_fold.first_clause + i];
				if (!shape_terms_equal_at_depth(
						db, left_clause->operation, right_clause->operation, env,
						type_view_compare_mode, depth + 1
					) || !shape_terms_equal_at_depth(
						db, left_clause->body, right_clause->body, env,
						type_view_compare_mode, depth + 1
					)) {
					return 0;
				}
			}
			return 1;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return shape_terms_equal_at_depth(
				db, left->as.operation_request.operation, right->as.operation_request.operation,
				env, type_view_compare_mode, depth + 1
			) && shape_terms_equal_at_depth(
				db, left->as.operation_request.argument, right->as.operation_request.argument,
				env, type_view_compare_mode, depth + 1
			) && shape_terms_equal_at_depth(
				db, left->as.operation_request.continuation, right->as.operation_request.continuation,
				env, type_view_compare_mode, depth + 1
			);
		case PROTOTYPE_TERM_PI:
			return shape_terms_equal_at_depth(
					db,
						left->as.pi.domain,
						right->as.pi.domain,
						env,
						type_view_compare_mode,
						depth + 1
					) &&
				shape_terms_equal_at_depth(
					db,
						left->as.pi.codomain_family,
						right->as.pi.codomain_family,
						env,
						type_view_compare_mode,
						depth + 1
					);
		case PROTOTYPE_TERM_MATCH: {
			if (!shape_terms_equal_at_depth(
					db,
						left->as.match.scrutinee,
						right->as.match.scrutinee,
						env,
						type_view_compare_mode,
						depth + 1
					) ||
				left->as.match.case_count != right->as.match.case_count) {
				return 0;
			}
			int left_has_frame = left->as.match.ih_scope_id != PROTOTYPE_INVALID_ID;
			int right_has_frame = right->as.match.ih_scope_id != PROTOTYPE_INVALID_ID;
			if (left_has_frame != right_has_frame) {
				return 0;
			}
			uint32_t saved_frame_count = env->frame_count;
			if (left_has_frame && term_scope_env_push_frame(
					env, left->as.match.ih_scope_id, right->as.match.ih_scope_id
				) != 0) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.match.case_count; ++i) {
				const struct prototype_match_case* left_case =
					&db->cases[left->as.match.first_case + i];
				const struct prototype_match_case* right_case =
					&db->cases[right->as.match.first_case + i];
				if (!shape_match_cases_equal_at_depth(
						db,
							left_case,
							right_case,
							env,
							type_view_compare_mode,
							depth + 1
						)) {
					env->frame_count = saved_frame_count;
					return 0;
				}
			}
			env->frame_count = saved_frame_count;
			return 1;
		}
		case PROTOTYPE_TERM_TYPE_FORMER:
			return left->as.type_former.representation_id ==
				right->as.type_former.representation_id;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			if (type_identity_is_stable(left->as.type_declaration.identity) ||
				type_identity_is_stable(right->as.type_declaration.identity)) {
				return type_identity_equal(
					left->as.type_declaration.identity,
					right->as.type_declaration.identity
				);
			}
			return left->as.type_declaration.type_id == right->as.type_declaration.type_id;
		case PROTOTYPE_TERM_TYPE_VIEW:
			return (type_identity_is_stable(left->as.type_view.identity) ||
				type_identity_is_stable(right->as.type_view.identity) ?
				type_identity_equal(left->as.type_view.identity, right->as.type_view.identity) :
				left->as.type_view.view_type_id == right->as.type_view.view_type_id) &&
				shape_terms_equal_at_depth(
					db,
						left->as.type_view.core,
						right->as.type_view.core,
						env,
						type_view_compare_mode,
						depth + 1
					) &&
				shape_terms_equal_at_depth(
					db,
						left->as.type_view.source,
						right->as.type_view.source,
						env,
						type_view_compare_mode,
						depth + 1
					);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			int frame_is_bound = 0;
			if (!term_scope_frames_equal(
					env,
					left->as.induction_hypothesis.ih_scope_id,
					right->as.induction_hypothesis.ih_scope_id,
					&frame_is_bound
				)) {
				return 0;
			}
			return
				shape_terms_equal_at_depth(
					db,
						left->as.induction_hypothesis.argument,
						right->as.induction_hypothesis.argument,
						env,
						type_view_compare_mode,
						depth + 1
					);
		}
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return left->as.universe_var.level_var == right->as.universe_var.level_var;
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_RETURNS_TYPE_FORMER:
		case PROTOTYPE_TERM_RETURNS_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			return 1;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			return left->as.dimension_action.operator_id ==
					right->as.dimension_action.operator_id &&
				shape_terms_equal_at_depth(
					db,
					left->as.dimension_action.source,
					right->as.dimension_action.source,
					env,
					type_view_compare_mode,
					depth + 1
				);
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				return 1;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				return left->as.text_literal.text_symbol_id == right->as.text_literal.text_symbol_id;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 1;
			case PROTOTYPE_TERM_INT_LITERAL:
				return left->as.int_literal.value == right->as.int_literal.value;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				return qualified_names_equal(left->as.external_ref.name, right->as.external_ref.name);
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
				return left->as.pure_primitive.primitive_id == right->as.pure_primitive.primitive_id &&
					left->as.pure_primitive.type_symbol_id == right->as.pure_primitive.type_symbol_id;
			case PROTOTYPE_TERM_EFFECT_OPERATION:
				return left->as.effect_operation.operation_id == right->as.effect_operation.operation_id &&
					shape_terms_equal_at_depth(
						db, left->as.effect_operation.classifier,
						right->as.effect_operation.classifier, env,
						type_view_compare_mode, depth + 1
					);
			case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				return 1;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				return term_scope_binders_equal(
					env, left->as.effect_row_var.binding_id, right->as.effect_row_var.binding_id
				);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				return shape_terms_equal_at_depth(
					db, left->as.effect_row_union.left, right->as.effect_row_union.left,
					env, type_view_compare_mode, depth + 1
				) && shape_terms_equal_at_depth(
					db, left->as.effect_row_union.right, right->as.effect_row_union.right,
					env, type_view_compare_mode, depth + 1
				);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			uint32_t saved_count = env->count;
			if (term_scope_env_push_binder(
					env,
					left->as.effect_row_forall.binding_id,
					right->as.effect_row_forall.binding_id
				) != 0) {
				return 0;
			}
			int equal = shape_terms_equal_at_depth(
				db,
				left->as.effect_row_forall.body,
				right->as.effect_row_forall.body,
				env,
				type_view_compare_mode,
				depth + 1
			);
			env->count = saved_count;
			return equal;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return left->as.effect_row_operation.operation_id ==
					right->as.effect_row_operation.operation_id &&
				shape_terms_equal_at_depth(
					db,
					left->as.effect_row_operation.latent_row,
					right->as.effect_row_operation.latent_row,
					env,
					type_view_compare_mode,
					depth + 1
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return left->as.computation_type.totality ==
					right->as.computation_type.totality &&
				shape_terms_equal_at_depth(
						db,
						left->as.computation_type.label,
						right->as.computation_type.label,
						env,
						type_view_compare_mode,
						depth + 1
					) &&
					shape_terms_equal_at_depth(
						db,
						left->as.computation_type.result,
						right->as.computation_type.result,
						env,
						type_view_compare_mode,
						depth + 1
					);
			default:
				return 0;
		}
	}

static int canonical_keys_equal_local(
	const struct prototype_term_canonical_key* left,
	const struct prototype_term_canonical_key* right
) {
	return left && right &&
		left->hash == right->hash &&
		left->node_count == right->node_count &&
		left->bound_binder_count == right->bound_binder_count &&
		left->free_binder_count == right->free_binder_count &&
		left->has_frame_local_reference == right->has_frame_local_reference &&
		left->has_type_local_reference == right->has_type_local_reference &&
		left->has_type_name_reference == right->has_type_name_reference &&
		left->has_type_universe_reference == right->has_type_universe_reference;
}

static int cross_shape_terms_equal_at_depth(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left_id,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right_id,
	struct term_scope_env* env,
	int type_view_compare_mode,
	uint32_t depth
);

static int cross_type_formers_equal(
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left_type_id,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right_type_id
) {
	if (left_type_declarations && right_type_declarations &&
		left_type_declarations != right_type_declarations) {
		return 0;
	}
	return left_type_id == right_type_id;
}

static int cross_frame_keys_equal(
	const struct prototype_term_db* left_db,
	uint32_t left_frame,
	const struct prototype_term_db* right_db,
	uint32_t right_frame
) {
	if (!left_db || !right_db ||
		left_frame >= left_db->ih_scope_count ||
		right_frame >= right_db->ih_scope_count ||
		!left_db->ih_scopes[left_frame].key.is_linkable ||
		!right_db->ih_scopes[right_frame].key.is_linkable) {
		return 0;
	}
	const struct prototype_ih_scope_key* left = &left_db->ih_scopes[left_frame].key;
	const struct prototype_ih_scope_key* right = &right_db->ih_scopes[right_frame].key;
	return left->case_count == right->case_count &&
		canonical_keys_equal_local(&left->match_key, &right->match_key);
}

static int cross_free_frames_equal(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left_frame,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right_frame,
	struct term_scope_env* env,
	int type_view_compare_mode,
	uint32_t depth
) {
	if (!cross_frame_keys_equal(left_db, left_frame, right_db, right_frame) ||
		left_frame >= left_db->ih_scope_count ||
		right_frame >= right_db->ih_scope_count) {
		return 0;
	}
	for (uint32_t i = 0; i < env->visited_frame_count; ++i) {
		if (env->visited_frame_left[i] == left_frame &&
			env->visited_frame_right[i] == right_frame) {
			return 1;
		}
	}
	if (env->visited_frame_count >= PROTOTYPE_CANONICAL_BINDER_CAPACITY) {
		return 0;
	}
	uint32_t saved_visited_frame_count = env->visited_frame_count;
	env->visited_frame_left[env->visited_frame_count] = left_frame;
	env->visited_frame_right[env->visited_frame_count] = right_frame;
	env->visited_frame_count++;
	uint32_t left_match = left_db->ih_scopes[left_frame].match_term;
	uint32_t right_match = right_db->ih_scopes[right_frame].match_term;
	if (left_match >= left_db->term_count || right_match >= right_db->term_count) {
		env->visited_frame_count = saved_visited_frame_count;
		return 0;
	}
	int equal = cross_shape_terms_equal_at_depth(
		left_db,
		left_type_declarations,
		left_match,
		right_db,
		right_type_declarations,
		right_match,
		env,
		type_view_compare_mode,
		depth + 1
	);
	env->visited_frame_count = saved_visited_frame_count;
	return equal;
}

static int cross_shape_match_cases_equal_at_depth(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	const struct prototype_match_case* left_case,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	const struct prototype_match_case* right_case,
	struct term_scope_env* env,
	int type_view_compare_mode,
	uint32_t depth
) {
	if (!left_db || !left_case || !right_db || !right_case || !env || depth > 256) {
		return 0;
	}
	if (left_case->constructor_id != right_case->constructor_id ||
		left_case->binder_count != right_case->binder_count) {
		return 0;
	}
	if (left_case->constructor_owner == PROTOTYPE_INVALID_ID ||
		right_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		if (left_case->constructor_owner != right_case->constructor_owner) {
			return 0;
		}
	} else if (!cross_shape_terms_equal_at_depth(
			left_db,
			left_type_declarations,
			left_case->constructor_owner,
			right_db,
				right_type_declarations,
				right_case->constructor_owner,
				env,
				type_view_compare_mode,
				depth + 1
			)) {
		return 0;
	}

	uint32_t saved_count = env->count;
	for (uint32_t i = 0; i < left_case->binder_count; ++i) {
		const struct prototype_case_binder* left_binder =
			&left_db->case_binders[left_case->first_binder + i];
		const struct prototype_case_binder* right_binder =
			&right_db->case_binders[right_case->first_binder + i];
		if (left_binder->is_recursive != right_binder->is_recursive ||
			term_scope_env_push_binder(
				env, left_binder->binding_id, right_binder->binding_id
			) != 0) {
			env->count = saved_count;
			return 0;
		}
	}
	int equal = cross_shape_terms_equal_at_depth(
		left_db,
		left_type_declarations,
		left_case->body,
		right_db,
		right_type_declarations,
		right_case->body,
		env,
		type_view_compare_mode,
		depth + 1
	);
	env->count = saved_count;
	return equal;
}

static int cross_shape_terms_equal_at_depth(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left_id,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right_id,
	struct term_scope_env* env,
	int type_view_compare_mode,
	uint32_t depth
) {
	if (!left_db || !right_db || !env ||
		left_id >= left_db->term_count ||
		right_id >= right_db->term_count ||
		depth > 256) {
		return 0;
	}
	left_id = shape_term_compare_id(left_db, left_id, type_view_compare_mode);
	right_id = shape_term_compare_id(right_db, right_id, type_view_compare_mode);
	if (left_id >= left_db->term_count || right_id >= right_db->term_count) {
		return 0;
	}
	const struct prototype_term* left = &left_db->terms[left_id];
	const struct prototype_term* right = &right_db->terms[right_id];
	if (left->tag != right->tag) {
		return 0;
	}
	switch (left->tag) {
		case PROTOTYPE_TERM_VAR:
			return term_scope_binders_equal(env, left->as.var.binding_id, right->as.var.binding_id);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return left->as.constructor.constructor_id == right->as.constructor.constructor_id &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.constructor.owner,
					right_db,
						right_type_declarations,
						right->as.constructor.owner,
						env,
						type_view_compare_mode,
						depth + 1
					);
		case PROTOTYPE_TERM_APP:
			return cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.app.function,
					right_db,
						right_type_declarations,
						right->as.app.function,
						env,
						type_view_compare_mode,
						depth + 1
					) &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.app.argument,
					right_db,
						right_type_declarations,
						right->as.app.argument,
						env,
						type_view_compare_mode,
						depth + 1
					);
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t saved_count = env->count;
			if (term_scope_env_push_binder(env, left->as.lambda.binding_id, right->as.lambda.binding_id) != 0) {
				env->count = saved_count;
				return 0;
			}
			int equal = cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.lambda.body,
				right_db,
					right_type_declarations,
					right->as.lambda.body,
					env,
					type_view_compare_mode,
					depth + 1
				);
			env->count = saved_count;
			return equal;
		}
		case PROTOTYPE_TERM_RETURN:
			return cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.return_term.value,
				right_db,
				right_type_declarations,
				right->as.return_term.value,
				env,
				type_view_compare_mode,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.thunk_type.computation,
				right_db,
				right_type_declarations,
				right->as.thunk_type.computation,
				env,
				type_view_compare_mode,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.thunk.computation,
				right_db,
				right_type_declarations,
				right->as.thunk.computation,
				env,
				type_view_compare_mode,
				depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.force.value,
				right_db,
				right_type_declarations,
				right->as.force.value,
				env,
				type_view_compare_mode,
				depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			if (!cross_shape_terms_equal_at_depth(
					left_db, left_type_declarations, left->as.computation_fold.computation,
					right_db, right_type_declarations, right->as.computation_fold.computation,
					env, type_view_compare_mode, depth + 1
				) || !cross_shape_terms_equal_at_depth(
					left_db, left_type_declarations, left->as.computation_fold.return_clause,
					right_db, right_type_declarations, right->as.computation_fold.return_clause,
					env, type_view_compare_mode, depth + 1
				) || left->as.computation_fold.clause_count != right->as.computation_fold.clause_count) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* left_clause =
					&left_db->computation_fold_clauses[left->as.computation_fold.first_clause + i];
				const struct prototype_computation_fold_clause* right_clause =
					&right_db->computation_fold_clauses[right->as.computation_fold.first_clause + i];
				if (!cross_shape_terms_equal_at_depth(
						left_db, left_type_declarations, left_clause->operation,
						right_db, right_type_declarations, right_clause->operation,
						env, type_view_compare_mode, depth + 1
					) || !cross_shape_terms_equal_at_depth(
						left_db, left_type_declarations, left_clause->body,
						right_db, right_type_declarations, right_clause->body,
						env, type_view_compare_mode, depth + 1
					)) {
					return 0;
				}
			}
			return 1;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return cross_shape_terms_equal_at_depth(
				left_db, left_type_declarations, left->as.operation_request.operation,
				right_db, right_type_declarations, right->as.operation_request.operation,
				env, type_view_compare_mode, depth + 1
			) && cross_shape_terms_equal_at_depth(
				left_db, left_type_declarations, left->as.operation_request.argument,
				right_db, right_type_declarations, right->as.operation_request.argument,
				env, type_view_compare_mode, depth + 1
			) && cross_shape_terms_equal_at_depth(
				left_db, left_type_declarations, left->as.operation_request.continuation,
				right_db, right_type_declarations, right->as.operation_request.continuation,
				env, type_view_compare_mode, depth + 1
			);
		case PROTOTYPE_TERM_PI:
			return cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.pi.domain,
					right_db,
						right_type_declarations,
						right->as.pi.domain,
						env,
						type_view_compare_mode,
						depth + 1
					) &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.pi.codomain_family,
					right_db,
						right_type_declarations,
						right->as.pi.codomain_family,
						env,
						type_view_compare_mode,
						depth + 1
					);
		case PROTOTYPE_TERM_MATCH: {
			if (!cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.match.scrutinee,
					right_db,
						right_type_declarations,
						right->as.match.scrutinee,
						env,
						type_view_compare_mode,
						depth + 1
					) ||
				left->as.match.case_count != right->as.match.case_count) {
				return 0;
			}
			int left_has_frame = left->as.match.ih_scope_id != PROTOTYPE_INVALID_ID;
			int right_has_frame = right->as.match.ih_scope_id != PROTOTYPE_INVALID_ID;
			if (left_has_frame != right_has_frame) {
				return 0;
			}
			uint32_t saved_frame_count = env->frame_count;
			if (left_has_frame && term_scope_env_push_frame(
					env, left->as.match.ih_scope_id, right->as.match.ih_scope_id
				) != 0) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.match.case_count; ++i) {
				const struct prototype_match_case* left_case =
					&left_db->cases[left->as.match.first_case + i];
				const struct prototype_match_case* right_case =
					&right_db->cases[right->as.match.first_case + i];
				if (!cross_shape_match_cases_equal_at_depth(
						left_db,
						left_type_declarations,
						left_case,
						right_db,
							right_type_declarations,
							right_case,
							env,
							type_view_compare_mode,
							depth + 1
						)) {
					env->frame_count = saved_frame_count;
					return 0;
				}
			}
			env->frame_count = saved_frame_count;
			return 1;
		}
		case PROTOTYPE_TERM_TYPE_FORMER:
			return cross_type_formers_equal(
				left_type_declarations,
				left->as.type_former.representation_id,
				right_type_declarations,
				right->as.type_former.representation_id
			);
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return type_identity_equal(
				left->as.type_declaration.identity,
				right->as.type_declaration.identity
			);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return type_identity_equal(
				left->as.type_view.identity,
				right->as.type_view.identity
			) &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.type_view.core,
					right_db,
						right_type_declarations,
						right->as.type_view.core,
						env,
						type_view_compare_mode,
						depth + 1
					) &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.type_view.source,
					right_db,
						right_type_declarations,
						right->as.type_view.source,
						env,
						type_view_compare_mode,
						depth + 1
					);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			int frame_is_bound = 0;
			int frames_equal = term_scope_frames_equal(
				env,
				left->as.induction_hypothesis.ih_scope_id,
				right->as.induction_hypothesis.ih_scope_id,
				&frame_is_bound
			);
			if (frame_is_bound && !frames_equal) {
				return 0;
			}
			if (!frame_is_bound && !cross_free_frames_equal(
					left_db,
					left_type_declarations,
					left->as.induction_hypothesis.ih_scope_id,
					right_db,
					right_type_declarations,
					right->as.induction_hypothesis.ih_scope_id,
					env,
					type_view_compare_mode,
					depth + 1
				)) {
				return 0;
			}
			return cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.induction_hypothesis.argument,
				right_db,
					right_type_declarations,
					right->as.induction_hypothesis.argument,
					env,
					type_view_compare_mode,
					depth + 1
				);
		}
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return left->as.universe_var.level_var == right->as.universe_var.level_var;
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_RETURNS_TYPE_FORMER:
		case PROTOTYPE_TERM_RETURNS_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			return 1;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			return left->as.dimension_action.operator_id ==
					right->as.dimension_action.operator_id &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.dimension_action.source,
					right_db,
					right_type_declarations,
					right->as.dimension_action.source,
					env,
					type_view_compare_mode,
					depth + 1
				);
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				return 1;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				return left->as.text_literal.text_symbol_id == right->as.text_literal.text_symbol_id;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 1;
			case PROTOTYPE_TERM_INT_LITERAL:
				return left->as.int_literal.value == right->as.int_literal.value;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				return qualified_names_equal(left->as.external_ref.name, right->as.external_ref.name);
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
				return left->as.pure_primitive.primitive_id == right->as.pure_primitive.primitive_id &&
					left->as.pure_primitive.type_symbol_id == right->as.pure_primitive.type_symbol_id;
			case PROTOTYPE_TERM_EFFECT_OPERATION:
				return left->as.effect_operation.operation_id == right->as.effect_operation.operation_id &&
					cross_shape_terms_equal_at_depth(
						left_db, left_type_declarations,
						left->as.effect_operation.classifier,
						right_db, right_type_declarations,
						right->as.effect_operation.classifier, env,
						type_view_compare_mode, depth + 1
					);
			case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				return 1;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				return term_scope_binders_equal(
					env, left->as.effect_row_var.binding_id, right->as.effect_row_var.binding_id
				);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				return cross_shape_terms_equal_at_depth(
					left_db, left_type_declarations, left->as.effect_row_union.left,
					right_db, right_type_declarations, right->as.effect_row_union.left,
					env, type_view_compare_mode, depth + 1
				) && cross_shape_terms_equal_at_depth(
					left_db, left_type_declarations, left->as.effect_row_union.right,
					right_db, right_type_declarations, right->as.effect_row_union.right,
					env, type_view_compare_mode, depth + 1
				);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			uint32_t saved_count = env->count;
			if (term_scope_env_push_binder(
					env,
					left->as.effect_row_forall.binding_id,
					right->as.effect_row_forall.binding_id
				) != 0) {
				return 0;
			}
			int equal = cross_shape_terms_equal_at_depth(
				left_db,
				left_type_declarations,
				left->as.effect_row_forall.body,
				right_db,
				right_type_declarations,
				right->as.effect_row_forall.body,
				env,
				type_view_compare_mode,
				depth + 1
			);
			env->count = saved_count;
			return equal;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return left->as.effect_row_operation.operation_id ==
					right->as.effect_row_operation.operation_id &&
				cross_shape_terms_equal_at_depth(
					left_db,
					left_type_declarations,
					left->as.effect_row_operation.latent_row,
					right_db,
					right_type_declarations,
					right->as.effect_row_operation.latent_row,
					env,
					type_view_compare_mode,
					depth + 1
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return left->as.computation_type.totality ==
					right->as.computation_type.totality &&
				cross_shape_terms_equal_at_depth(
						left_db,
						left_type_declarations,
						left->as.computation_type.label,
						right_db,
						right_type_declarations,
						right->as.computation_type.label,
						env,
						type_view_compare_mode,
						depth + 1
					) &&
					cross_shape_terms_equal_at_depth(
						left_db,
						left_type_declarations,
						left->as.computation_type.result,
						right_db,
						right_type_declarations,
						right->as.computation_type.result,
						env,
						type_view_compare_mode,
						depth + 1
					);
			default:
				return 0;
		}
	}

static int shape_equal_for_link_with_type_view_mode(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int type_view_compare_mode,
	int* p_equal
) {
	if (!left_db || !right_db || !p_equal ||
		left >= left_db->term_count ||
		right >= right_db->term_count) {
		return -1;
	}
	if (left_db == right_db && left == right) {
		*p_equal = 1;
		return 0;
	}

	struct term_scope_env env;
	memset(&env, 0, sizeof(env));
	*p_equal = cross_shape_terms_equal_at_depth(
			left_db,
			left_type_declarations,
			left,
			right_db,
			right_type_declarations,
			right,
			&env,
			type_view_compare_mode,
			0
		);
	return 0;
}

int prototype_term_view_shape_equal_for_link(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int* p_equal
) {
	return shape_equal_for_link_with_type_view_mode(
		left_db,
		left_type_declarations,
		left,
		right_db,
		right_type_declarations,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_VIEW,
		p_equal
	);
}

int prototype_term_core_shape_equal_for_link(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int* p_equal
) {
	return shape_equal_for_link_with_type_view_mode(
		left_db,
		left_type_declarations,
		left,
		right_db,
		right_type_declarations,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_CORE,
		p_equal
	);
}

int prototype_term_source_shape_equal_for_link(
	const struct prototype_term_db* left_db,
	const struct prototype_type_declaration_db* left_type_declarations,
	uint32_t left,
	const struct prototype_term_db* right_db,
	const struct prototype_type_declaration_db* right_type_declarations,
	uint32_t right,
	int* p_equal
) {
	return shape_equal_for_link_with_type_view_mode(
		left_db,
		left_type_declarations,
		left,
		right_db,
		right_type_declarations,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_SOURCE,
		p_equal
	);
}

static int shape_equal_with_type_view_mode(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int type_view_compare_mode,
	int* p_equal
) {
	if (!db || !p_equal || left >= db->term_count || right >= db->term_count) {
		return -1;
	}

	struct term_scope_env env;
	memset(&env, 0, sizeof(env));
	*p_equal = shape_terms_equal_at_depth(
		db,
		left,
		right,
		&env,
		type_view_compare_mode,
		0
	);
	return 0;
}

static int term_intern_alpha_equal_local(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right
) {
	struct term_scope_env env;
	memset(&env, 0, sizeof(env));
	return shape_terms_equal_at_depth(
		db,
		left,
		right,
		&env,
		PROTOTYPE_TYPE_VIEW_COMPARE_VIEW,
		0
	);
}

/* Exact identity accelerates reconstruction from canonical child IDs. Alpha
 * identity remains authoritative when this cheap comparison does not match. */
static int term_intern_exact_equal_local(
	const struct prototype_term_db* db,
	uint32_t left_id,
	uint32_t right_id
) {
	if (!db || left_id >= db->term_count || right_id >= db->term_count) {
		return 0;
	}
	const struct prototype_term* left = &db->terms[left_id];
	const struct prototype_term* right = &db->terms[right_id];
	if (left->tag != right->tag) {
		return 0;
	}
#define INTERN_EXACT_FIELD(path) (left->as.path == right->as.path)
	switch (left->tag) {
		case PROTOTYPE_TERM_VAR:
			return INTERN_EXACT_FIELD(var.binding_id);
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return INTERN_EXACT_FIELD(constructor.owner) &&
				INTERN_EXACT_FIELD(constructor.constructor_id);
		case PROTOTYPE_TERM_APP:
			return INTERN_EXACT_FIELD(app.function) &&
				INTERN_EXACT_FIELD(app.argument);
		case PROTOTYPE_TERM_LAMBDA:
			return INTERN_EXACT_FIELD(lambda.binding_id) &&
				INTERN_EXACT_FIELD(lambda.body);
		case PROTOTYPE_TERM_PI:
			return INTERN_EXACT_FIELD(pi.domain) &&
				INTERN_EXACT_FIELD(pi.codomain_family);
		case PROTOTYPE_TERM_MATCH:
			if (!INTERN_EXACT_FIELD(match.scrutinee) ||
				!INTERN_EXACT_FIELD(match.case_count) ||
				!INTERN_EXACT_FIELD(match.ih_scope_id)) {
				return 0;
			}
			for (uint32_t i = 0; i < left->as.match.case_count; ++i) {
				const struct prototype_match_case* left_case =
					&db->cases[left->as.match.first_case + i];
				const struct prototype_match_case* right_case =
					&db->cases[right->as.match.first_case + i];
				if (left_case->constructor_owner != right_case->constructor_owner ||
					left_case->constructor_id != right_case->constructor_id ||
					left_case->binder_count != right_case->binder_count ||
					left_case->body != right_case->body) {
					return 0;
				}
				for (uint32_t j = 0; j < left_case->binder_count; ++j) {
					const struct prototype_case_binder* left_binder =
						&db->case_binders[left_case->first_binder + j];
					const struct prototype_case_binder* right_binder =
						&db->case_binders[right_case->first_binder + j];
					if (left_binder->binding_id != right_binder->binding_id ||
						left_binder->is_recursive != right_binder->is_recursive) {
						return 0;
					}
				}
			}
			return 1;
		case PROTOTYPE_TERM_TYPE_FORMER:
			return INTERN_EXACT_FIELD(type_former.representation_id);
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			return type_identity_is_stable(left->as.type_declaration.identity) ||
				type_identity_is_stable(right->as.type_declaration.identity) ?
				type_identity_equal(
					left->as.type_declaration.identity,
					right->as.type_declaration.identity
				) : INTERN_EXACT_FIELD(type_declaration.type_id);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return INTERN_EXACT_FIELD(induction_hypothesis.ih_scope_id) &&
				INTERN_EXACT_FIELD(induction_hypothesis.argument);
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			return INTERN_EXACT_FIELD(universe_var.level_var);
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_RETURNS_TYPE_FORMER:
		case PROTOTYPE_TERM_RETURNS_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			return 1;
		case PROTOTYPE_TERM_TEXT_LITERAL:
			return INTERN_EXACT_FIELD(text_literal.text_symbol_id);
		case PROTOTYPE_TERM_INT_LITERAL:
			return INTERN_EXACT_FIELD(int_literal.value);
		case PROTOTYPE_TERM_EXTERNAL_REF:
			return qualified_names_equal(
				left->as.external_ref.name, right->as.external_ref.name
			);
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			return INTERN_EXACT_FIELD(pure_primitive.primitive_id) &&
				INTERN_EXACT_FIELD(pure_primitive.type_symbol_id);
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			return INTERN_EXACT_FIELD(effect_operation.operation_id) &&
				INTERN_EXACT_FIELD(effect_operation.classifier);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return (type_identity_is_stable(left->as.type_view.identity) ||
				type_identity_is_stable(right->as.type_view.identity) ?
				type_identity_equal(
					left->as.type_view.identity, right->as.type_view.identity
				) : INTERN_EXACT_FIELD(type_view.view_type_id)) &&
				INTERN_EXACT_FIELD(type_view.core) &&
				INTERN_EXACT_FIELD(type_view.source);
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return INTERN_EXACT_FIELD(effect_row_var.binding_id);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return INTERN_EXACT_FIELD(effect_row_union.left) &&
				INTERN_EXACT_FIELD(effect_row_union.right);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return INTERN_EXACT_FIELD(effect_row_forall.binding_id) &&
				INTERN_EXACT_FIELD(effect_row_forall.body);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return INTERN_EXACT_FIELD(computation_type.label) &&
				INTERN_EXACT_FIELD(computation_type.result) &&
				INTERN_EXACT_FIELD(computation_type.totality);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return INTERN_EXACT_FIELD(thunk_type.computation);
		case PROTOTYPE_TERM_RETURN:
			return INTERN_EXACT_FIELD(return_term.value);
		case PROTOTYPE_TERM_THUNK:
			return INTERN_EXACT_FIELD(thunk.computation);
		case PROTOTYPE_TERM_FORCE:
			return INTERN_EXACT_FIELD(force.value);
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return INTERN_EXACT_FIELD(operation_request.operation) &&
				INTERN_EXACT_FIELD(operation_request.argument) &&
				INTERN_EXACT_FIELD(operation_request.continuation);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (!INTERN_EXACT_FIELD(computation_fold.computation) ||
				!INTERN_EXACT_FIELD(computation_fold.return_clause) ||
				!INTERN_EXACT_FIELD(computation_fold.clause_count)) {
				return 0;
			}
			for (uint32_t i = 0;
				i < left->as.computation_fold.clause_count;
				++i) {
				const struct prototype_computation_fold_clause* left_clause =
					&db->computation_fold_clauses[
						left->as.computation_fold.first_clause + i
					];
				const struct prototype_computation_fold_clause* right_clause =
					&db->computation_fold_clauses[
						right->as.computation_fold.first_clause + i
					];
				if (left_clause->operation != right_clause->operation ||
					left_clause->body != right_clause->body) {
					return 0;
				}
			}
			return 1;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return INTERN_EXACT_FIELD(effect_row_operation.operation_id) &&
				INTERN_EXACT_FIELD(effect_row_operation.latent_row);
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			return INTERN_EXACT_FIELD(dimension_action.operator_id) &&
				INTERN_EXACT_FIELD(dimension_action.source);
		default:
			return 0;
	}
#undef INTERN_EXACT_FIELD
}

int prototype_term_view_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return shape_equal_with_type_view_mode(
		db,
		left,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_VIEW,
		p_equal
	);
}

int prototype_term_core_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return shape_equal_with_type_view_mode(
		db,
		left,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_CORE,
		p_equal
	);
}

int prototype_term_core_shape_equal_under_binder(
	const struct prototype_term_db* db,
	uint32_t left_binder,
	uint32_t left,
	uint32_t right_binder,
	uint32_t right,
	int* p_equal
) {
	return prototype_term_core_shape_equal_under_binders(
		db,
		&left_binder,
		&right_binder,
		1,
		left,
		right,
		p_equal
	);
}

int prototype_term_core_shape_equal_under_binders(
	const struct prototype_term_db* db,
	const uint32_t* left_binders,
	const uint32_t* right_binders,
	size_t binder_count,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return prototype_term_core_shape_equal_under_binders_and_ih_scope(
		db,
		left_binders,
		right_binders,
		binder_count,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		left,
		right,
		p_equal
	);
}

int prototype_term_core_shape_equal_under_binders_and_ih_scope(
	const struct prototype_term_db* db,
	const uint32_t* left_binders,
	const uint32_t* right_binders,
	size_t binder_count,
	uint32_t left_ih_scope,
	uint32_t right_ih_scope,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	if (!db || !p_equal || left >= db->term_count || right >= db->term_count ||
		(binder_count > 0 && (!left_binders || !right_binders)) ||
		((left_ih_scope == PROTOTYPE_INVALID_ID) !=
		 (right_ih_scope == PROTOTYPE_INVALID_ID))) {
		return -1;
	}
	struct term_scope_env env;
	memset(&env, 0, sizeof(env));
	for (size_t i = 0; i < binder_count; ++i) {
		if (term_scope_env_push_binder(
				&env, left_binders[i], right_binders[i]
			) != 0) {
			return -1;
		}
	}
	if (left_ih_scope != PROTOTYPE_INVALID_ID &&
		term_scope_env_push_frame(
			&env, left_ih_scope, right_ih_scope
		) != 0) {
		return -1;
	}
	*p_equal = shape_terms_equal_at_depth(
		db, left, right, &env, PROTOTYPE_TYPE_VIEW_COMPARE_CORE, 0
	);
	return 0;
}

int prototype_term_source_shape_equal(
	const struct prototype_term_db* db,
	uint32_t left,
	uint32_t right,
	int* p_equal
) {
	return shape_equal_with_type_view_mode(
		db,
		left,
		right,
		PROTOTYPE_TYPE_VIEW_COMPARE_SOURCE,
		p_equal
	);
}

static int canonical_env_lookup(
	const struct canonical_binder_env* env,
	uint32_t binding_id,
	uint32_t* p_slot
) {
	if (!env || !p_slot) {
		return 0;
	}
	for (uint32_t i = env->count; i > 0; --i) {
		uint32_t index = i - 1;
		if (env->binding_id[index] == binding_id) {
			*p_slot = env->slot[index];
			return 1;
		}
	}
	return 0;
}

static int canonical_env_push(
	struct canonical_binder_env* env,
	uint32_t binding_id
) {
	if (!env || env->count >= PROTOTYPE_CANONICAL_BINDER_CAPACITY) {
		return -1;
	}
	env->binding_id[env->count] = binding_id;
	env->slot[env->count] = env->next_slot++;
	env->count++;
	return 0;
}

static int canonical_hash_term_at_depth(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	struct canonical_binder_env* env,
	struct prototype_term_canonical_key* key,
	uint64_t* p_hash,
	int canonicalize_frame_refs,
	uint32_t depth
);

static int canonical_hash_match_case_at_depth(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	const struct prototype_match_case* match_case,
	struct canonical_binder_env* env,
	struct prototype_term_canonical_key* key,
	uint64_t* p_hash,
	int canonicalize_frame_refs,
	uint32_t depth
) {
	if (!db || !match_case || !env || !key || !p_hash || depth > 256) {
		return -1;
	}
	canonical_hash_mix_u32(p_hash, match_case->constructor_id);
	canonical_hash_mix_u32(p_hash, match_case->binder_count);
	if (match_case->constructor_owner == PROTOTYPE_INVALID_ID) {
		canonical_hash_mix_u32(p_hash, PROTOTYPE_INVALID_ID);
	} else if (canonical_hash_term_at_depth(
			db,
			type_declarations,
			match_case->constructor_owner,
			env,
			key,
			p_hash,
			canonicalize_frame_refs,
			depth + 1
		) != 0) {
		return -1;
	}

	uint32_t saved_count = env->count;
	uint32_t saved_next_slot = env->next_slot;
	for (uint32_t i = 0; i < match_case->binder_count; ++i) {
		const struct prototype_case_binder* binder =
			&db->case_binders[match_case->first_binder + i];
		if (canonical_env_push(env, binder->binding_id) != 0) {
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return -1;
		}
		key->bound_binder_count++;
	}
	int status = canonical_hash_term_at_depth(
		db,
		type_declarations,
		match_case->body,
		env,
		key,
		p_hash,
		canonicalize_frame_refs,
		depth + 1
	);
	env->count = saved_count;
	env->next_slot = saved_next_slot;
	return status;
}

static int canonical_hash_term_at_depth(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	struct canonical_binder_env* env,
	struct prototype_term_canonical_key* key,
	uint64_t* p_hash,
	int canonicalize_frame_refs,
	uint32_t depth
) {
	if (!db || !env || !key || !p_hash || term_id >= db->term_count || depth > 256) {
		return -1;
	}

	const struct prototype_term* term = &db->terms[term_id];
	key->node_count++;
	canonical_hash_mix_tag(p_hash, (uint32_t)term->tag);
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR: {
			uint32_t slot;
			if (canonical_env_lookup(env, term->as.var.binding_id, &slot)) {
				canonical_hash_mix_u32(p_hash, 1);
				canonical_hash_mix_u32(p_hash, slot);
			} else {
				canonical_hash_mix_u32(p_hash, 0);
				canonical_hash_mix_u32(p_hash, term->as.var.binding_id);
				key->free_binder_count++;
			}
			return 0;
		}
		case PROTOTYPE_TERM_CONSTRUCTOR:
			canonical_hash_mix_u32(p_hash, term->as.constructor.constructor_id);
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.constructor.owner,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_APP:
			if (canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.app.function,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				) != 0) {
				return -1;
			}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.app.argument,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t saved_count = env->count;
			uint32_t saved_next_slot = env->next_slot;
			if (canonical_env_push(env, term->as.lambda.binding_id) != 0) {
				return -1;
			}
			key->bound_binder_count++;
			int status = canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.lambda.body,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return status;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			canonical_hash_mix_u32(
				p_hash, (uint32_t)term->as.effect_row_operation.operation_id
			);
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.effect_row_operation.latent_row,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD: {
			if (canonical_hash_term_at_depth(
					db, type_declarations, term->as.computation_fold.computation,
					env, key, p_hash, canonicalize_frame_refs, depth + 1
				) != 0 || canonical_hash_term_at_depth(
					db, type_declarations, term->as.computation_fold.return_clause,
					env, key, p_hash, canonicalize_frame_refs, depth + 1
				) != 0) {
				return -1;
			}
			canonical_hash_mix_u32(p_hash, term->as.computation_fold.clause_count);
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&db->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				if (canonical_hash_term_at_depth(
						db, type_declarations, clause->operation, env, key, p_hash,
						canonicalize_frame_refs, depth + 1
					) != 0 || canonical_hash_term_at_depth(
						db, type_declarations, clause->body, env, key, p_hash,
						canonicalize_frame_refs, depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			if (canonical_hash_term_at_depth(
					db, type_declarations, term->as.operation_request.operation,
					env, key, p_hash, canonicalize_frame_refs, depth + 1
				) != 0 || canonical_hash_term_at_depth(
					db, type_declarations, term->as.operation_request.argument,
					env, key, p_hash, canonicalize_frame_refs, depth + 1
				) != 0) {
				return -1;
			}
			return canonical_hash_term_at_depth(
				db, type_declarations, term->as.operation_request.continuation,
				env, key, p_hash, canonicalize_frame_refs, depth + 1
			);
		case PROTOTYPE_TERM_PI:
			if (canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.pi.domain,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				) != 0) {
				return -1;
			}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.pi.codomain_family,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_MATCH:
			key->has_frame_local_reference =
				key->has_frame_local_reference ||
				(term->as.match.ih_scope_id != PROTOTYPE_INVALID_ID &&
					!canonicalize_frame_refs &&
					(term->as.match.ih_scope_id >= db->ih_scope_count ||
						!db->ih_scopes[term->as.match.ih_scope_id].key.is_linkable));
			canonical_hash_mix_u32(p_hash, term->as.match.case_count);
			if (canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.match.scrutinee,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&db->cases[term->as.match.first_case + i];
				if (canonical_hash_match_case_at_depth(
						db,
						type_declarations,
						match_case,
						env,
						key,
						p_hash,
						canonicalize_frame_refs,
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_FORMER:
			key->has_type_local_reference = 1;
			canonical_hash_mix_u32(p_hash, term->as.type_former.representation_id);
			return 0;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			if (type_identity_is_stable(term->as.type_declaration.identity)) {
				key->has_type_name_reference = 1;
				canonical_hash_mix_u32(
					p_hash, term->as.type_declaration.type_id
				);
			} else {
				key->has_type_local_reference = 1;
				canonical_hash_mix_u32(p_hash, term->as.type_declaration.type_id);
			}
			return 0;
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (type_identity_is_stable(term->as.type_view.identity)) {
				key->has_type_name_reference = 1;
				canonical_hash_mix_u32(p_hash, term->as.type_view.view_type_id);
			} else {
				key->has_type_local_reference = 1;
				canonical_hash_mix_u32(p_hash, term->as.type_view.view_type_id);
			}
			if (canonical_hash_term_at_depth(
					db,
					type_declarations,
					term->as.type_view.core,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
					depth + 1
				) != 0) {
				return -1;
			}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.type_view.source,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
			case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			if (canonicalize_frame_refs) {
				canonical_hash_mix_u32(p_hash, 0);
			} else if (term->as.induction_hypothesis.ih_scope_id < db->ih_scope_count &&
				db->ih_scopes[term->as.induction_hypothesis.ih_scope_id].key.is_linkable) {
				const struct prototype_ih_scope_key* frame_key =
					&db->ih_scopes[term->as.induction_hypothesis.ih_scope_id].key;
				canonical_hash_mix_u32(p_hash, 1);
				canonical_hash_mix_u32(p_hash, (uint32_t)frame_key->match_key.hash);
				canonical_hash_mix_u32(p_hash, (uint32_t)(frame_key->match_key.hash >> 32));
				canonical_hash_mix_u32(p_hash, frame_key->match_key.node_count);
				canonical_hash_mix_u32(p_hash, frame_key->case_count);
			} else {
				key->has_frame_local_reference = 1;
				canonical_hash_mix_u32(p_hash, 2);
				canonical_hash_mix_u32(p_hash, term->as.induction_hypothesis.ih_scope_id);
			}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.induction_hypothesis.argument,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			canonical_hash_mix_u32(p_hash, term->as.universe_var.level_var);
			return 0;
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_RETURNS_TYPE_FORMER:
		case PROTOTYPE_TERM_RETURNS_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			return 0;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			canonical_hash_mix_u32(
				p_hash, term->as.dimension_action.operator_id
			);
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.dimension_action.source,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
			case PROTOTYPE_TERM_PRIMITIVE_TEXT:
				return 0;
			case PROTOTYPE_TERM_TEXT_LITERAL:
				return 0;
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				return 0;
			case PROTOTYPE_TERM_INT_LITERAL:
				canonical_hash_mix_u32(p_hash, (uint32_t)term->as.int_literal.value);
				canonical_hash_mix_u32(p_hash, (uint32_t)((uint64_t)term->as.int_literal.value >> 32));
				return 0;
			case PROTOTYPE_TERM_EXTERNAL_REF:
				return 0;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
				canonical_hash_mix_u32(p_hash, (uint32_t)term->as.pure_primitive.primitive_id);
				return 0;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			canonical_hash_mix_u32(p_hash, (uint32_t)term->as.effect_operation.operation_id);
			return canonical_hash_term_at_depth(
				db, type_declarations, term->as.effect_operation.classifier, env,
				key, p_hash, canonicalize_frame_refs, depth + 1
			);
			case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
				return 0;
			case PROTOTYPE_TERM_EFFECT_ROW_VAR:
				{
					uint32_t slot;
					if (canonical_env_lookup(env, term->as.effect_row_var.binding_id, &slot)) {
						canonical_hash_mix_u32(p_hash, slot);
					} else {
						canonical_hash_mix_u32(p_hash, term->as.effect_row_var.binding_id);
					}
				}
				return 0;
			case PROTOTYPE_TERM_EFFECT_ROW_UNION:
				if (canonical_hash_term_at_depth(
						db, type_declarations, term->as.effect_row_union.left, env, key,
						p_hash, canonicalize_frame_refs, depth + 1
					) != 0) {
					return -1;
				}
			return canonical_hash_term_at_depth(
				db, type_declarations, term->as.effect_row_union.right, env, key,
				p_hash, canonicalize_frame_refs, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			uint32_t saved_count = env->count;
			uint32_t saved_next_slot = env->next_slot;
			if (canonical_env_push(env, term->as.effect_row_forall.binding_id) != 0) {
				return -1;
			}
			key->bound_binder_count++;
			int status = canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.effect_row_forall.body,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
			env->count = saved_count;
			env->next_slot = saved_next_slot;
			return status;
		}
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			canonical_hash_mix_u32(
				p_hash, (uint32_t)term->as.computation_type.totality
			);
			if (canonical_hash_term_at_depth(
						db,
						type_declarations,
						term->as.computation_type.label,
						env,
						key,
						p_hash,
						canonicalize_frame_refs,
						depth + 1
					) != 0) {
					return -1;
				}
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.computation_type.result,
					env,
					key,
					p_hash,
					canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.thunk_type.computation,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.return_term.value,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.thunk.computation,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return canonical_hash_term_at_depth(
				db,
				type_declarations,
				term->as.force.value,
				env,
				key,
				p_hash,
				canonicalize_frame_refs,
				depth + 1
			);
			default:
				return -1;
		}
	}

int prototype_term_canonical_key(
	const struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_term_canonical_key* p_key
) {
	return prototype_term_canonical_key_with_types(db, NULL, term_id, p_key);
}

int prototype_term_canonical_key_with_types(
	const struct prototype_term_db* db,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	struct prototype_term_canonical_key* p_key
) {
	if (!db || !p_key || term_id >= db->term_count) {
		return -1;
	}

	struct canonical_binder_env env;
	uint64_t hash = PROTOTYPE_CANONICAL_HASH_OFFSET;
	memset(&env, 0, sizeof(env));
	memset(p_key, 0, sizeof(*p_key));
	if (canonical_hash_term_at_depth(
			db,
			type_declarations,
			term_id,
			&env,
			p_key,
			&hash,
			0,
			0
		) != 0) {
		return -1;
	}
	p_key->hash = hash;
	return 0;
}

/* Interning treats an IH frame bound by a Match exactly like a binder. Free
 * frame identity is still checked by the exact alpha comparator after the hash
 * lookup. This hash is therefore a candidate index, never equality authority. */
static int term_intern_canonical_key_local(
	const struct prototype_term_db* db,
	uint32_t term_id,
	struct prototype_term_canonical_key* p_key
) {
	if (!db || !p_key || term_id >= db->term_count) {
		return -1;
	}
	uint64_t hash = PROTOTYPE_CANONICAL_HASH_OFFSET;
	memset(p_key, 0, sizeof(*p_key));
	const struct prototype_term* term = &db->terms[term_id];
	canonical_hash_mix_tag(&hash, (uint32_t)term->tag);
	p_key->node_count = 1;
#define INTERN_MIX(value) canonical_hash_mix_u32(&hash, (uint32_t)(value))
#define INTERN_MIX_CHILD(child) \
	do { \
		if ((child) >= db->term_count) { \
			return -1; \
		} \
		INTERN_MIX(child); \
	} while (0)
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			INTERN_MIX(term->as.var.binding_id);
			break;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			INTERN_MIX_CHILD(term->as.constructor.owner);
			INTERN_MIX(term->as.constructor.constructor_id);
			break;
		case PROTOTYPE_TERM_APP:
			INTERN_MIX_CHILD(term->as.app.function);
			INTERN_MIX_CHILD(term->as.app.argument);
			break;
		case PROTOTYPE_TERM_LAMBDA: {
			INTERN_MIX(db->terms[term->as.lambda.body].tag);
			if (db->terms[term->as.lambda.body].tag == PROTOTYPE_TERM_VAR) {
				uint32_t body_binding =
					db->terms[term->as.lambda.body].as.var.binding_id;
				INTERN_MIX(body_binding == term->as.lambda.binding_id ? 1 : 0);
				if (body_binding != term->as.lambda.binding_id) {
					INTERN_MIX(body_binding);
				}
			}
			break;
		}
		case PROTOTYPE_TERM_PI:
			INTERN_MIX_CHILD(term->as.pi.domain);
			INTERN_MIX_CHILD(term->as.pi.codomain_family);
			break;
		case PROTOTYPE_TERM_MATCH:
			INTERN_MIX_CHILD(term->as.match.scrutinee);
			INTERN_MIX(term->as.match.case_count);
			INTERN_MIX(term->as.match.ih_scope_id != PROTOTYPE_INVALID_ID);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&db->cases[term->as.match.first_case + i];
				INTERN_MIX(match_case->constructor_id);
				INTERN_MIX(match_case->constructor_owner);
				INTERN_MIX(match_case->binder_count);
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					INTERN_MIX(db->case_binders[
						match_case->first_binder + j
					].is_recursive);
				}
				if (match_case->body >= db->term_count) {
					return -1;
				}
				INTERN_MIX(db->terms[match_case->body].tag);
			}
			break;
		case PROTOTYPE_TERM_TYPE_FORMER:
			INTERN_MIX(term->as.type_former.representation_id);
			break;
		case PROTOTYPE_TERM_TYPE_DECLARATION:
			if (type_identity_is_stable(term->as.type_declaration.identity)) {
				INTERN_MIX(term->as.type_declaration.identity.namespace_symbol_id);
				INTERN_MIX(term->as.type_declaration.identity.name_symbol_id);
			} else {
				INTERN_MIX(term->as.type_declaration.type_id);
			}
			break;
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			INTERN_MIX(term->as.induction_hypothesis.ih_scope_id);
			INTERN_MIX_CHILD(term->as.induction_hypothesis.argument);
			break;
		case PROTOTYPE_TERM_UNIVERSE_VAR:
			INTERN_MIX(term->as.universe_var.level_var);
			break;
		case PROTOTYPE_TERM_PRIMITIVE_TEXT:
		case PROTOTYPE_TERM_PRIMITIVE_INT:
		case PROTOTYPE_TERM_PRIMITIVE_INT64:
		case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
		case PROTOTYPE_TERM_RELATION_TYPE_FORMER:
		case PROTOTYPE_TERM_RELATION_WITNESS_FORMER:
		case PROTOTYPE_TERM_RETURNS_TYPE_FORMER:
		case PROTOTYPE_TERM_RETURNS_WITNESS_FORMER:
		case PROTOTYPE_TERM_TERMINATES_TYPE_FORMER:
		case PROTOTYPE_TERM_TERMINATES_WITNESS_FORMER:
			break;
		case PROTOTYPE_TERM_TEXT_LITERAL:
			INTERN_MIX(term->as.text_literal.text_symbol_id);
			break;
		case PROTOTYPE_TERM_INT_LITERAL:
			INTERN_MIX((uint32_t)term->as.int_literal.value);
			INTERN_MIX((uint32_t)((uint64_t)term->as.int_literal.value >> 32));
			break;
		case PROTOTYPE_TERM_EXTERNAL_REF:
			INTERN_MIX(term->as.external_ref.name.namespace_symbol_id);
			INTERN_MIX(term->as.external_ref.name.name_symbol_id);
			break;
		case PROTOTYPE_TERM_PURE_PRIMITIVE:
			INTERN_MIX(term->as.pure_primitive.primitive_id);
			INTERN_MIX(term->as.pure_primitive.type_symbol_id);
			break;
		case PROTOTYPE_TERM_EFFECT_OPERATION:
			INTERN_MIX(term->as.effect_operation.operation_id);
			INTERN_MIX_CHILD(term->as.effect_operation.classifier);
			break;
		case PROTOTYPE_TERM_TYPE_VIEW:
			if (type_identity_is_stable(term->as.type_view.identity)) {
				INTERN_MIX(term->as.type_view.identity.namespace_symbol_id);
				INTERN_MIX(term->as.type_view.identity.name_symbol_id);
			} else {
				INTERN_MIX(term->as.type_view.view_type_id);
			}
			INTERN_MIX_CHILD(term->as.type_view.core);
			INTERN_MIX_CHILD(term->as.type_view.source);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			INTERN_MIX(term->as.effect_row_var.binding_id);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			INTERN_MIX_CHILD(term->as.effect_row_union.left);
			INTERN_MIX_CHILD(term->as.effect_row_union.right);
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			INTERN_MIX(db->terms[term->as.effect_row_forall.body].tag);
			break;
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			INTERN_MIX_CHILD(term->as.computation_type.label);
			INTERN_MIX_CHILD(term->as.computation_type.result);
			INTERN_MIX(term->as.computation_type.totality);
			break;
		case PROTOTYPE_TERM_THUNK_TYPE:
			INTERN_MIX_CHILD(term->as.thunk_type.computation);
			break;
		case PROTOTYPE_TERM_RETURN:
			INTERN_MIX_CHILD(term->as.return_term.value);
			break;
		case PROTOTYPE_TERM_THUNK:
			INTERN_MIX_CHILD(term->as.thunk.computation);
			break;
		case PROTOTYPE_TERM_FORCE:
			INTERN_MIX_CHILD(term->as.force.value);
			break;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			INTERN_MIX_CHILD(term->as.operation_request.operation);
			INTERN_MIX_CHILD(term->as.operation_request.argument);
			INTERN_MIX_CHILD(term->as.operation_request.continuation);
			break;
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			INTERN_MIX_CHILD(term->as.computation_fold.computation);
			INTERN_MIX_CHILD(term->as.computation_fold.return_clause);
			INTERN_MIX(term->as.computation_fold.clause_count);
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&db->computation_fold_clauses[
						term->as.computation_fold.first_clause + i
					];
				INTERN_MIX_CHILD(clause->operation);
				INTERN_MIX_CHILD(clause->body);
			}
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			INTERN_MIX(term->as.effect_row_operation.operation_id);
			INTERN_MIX_CHILD(term->as.effect_row_operation.latent_row);
			break;
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			INTERN_MIX(term->as.dimension_action.operator_id);
			INTERN_MIX_CHILD(term->as.dimension_action.source);
			break;
		default:
			return -1;
	}
	p_key->hash = hash;

#undef INTERN_MIX_CHILD
#undef INTERN_MIX
	return 0;
}

static int term_intern_exact_hash_local(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint64_t* p_hash
) {
	if (!db || !p_hash || term_id >= db->term_count) {
		return -1;
	}
	struct prototype_term_canonical_key alpha_key;
	if (term_intern_canonical_key_local(db, term_id, &alpha_key) != 0) {
		return -1;
	}
	uint64_t hash = alpha_key.hash;
	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_LAMBDA:
			canonical_hash_mix_u32(&hash, term->as.lambda.binding_id);
			canonical_hash_mix_u32(&hash, term->as.lambda.body);
			break;
		case PROTOTYPE_TERM_MATCH:
			canonical_hash_mix_u32(&hash, term->as.match.ih_scope_id);
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&db->cases[term->as.match.first_case + i];
				canonical_hash_mix_u32(&hash, match_case->body);
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					canonical_hash_mix_u32(
						&hash,
						db->case_binders[match_case->first_binder + j].binding_id
					);
				}
			}
			break;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			canonical_hash_mix_u32(
				&hash, term->as.effect_row_forall.binding_id
			);
			canonical_hash_mix_u32(&hash, term->as.effect_row_forall.body);
			break;
		default:
			break;
	}
	*p_hash = hash;
	return 0;
}

static int compute_ih_scope_key(
	const struct prototype_term_db* db,
	uint32_t match_term,
	struct prototype_ih_scope_key* p_key
) {
	if (!db || !p_key || match_term >= db->term_count ||
		db->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}

	struct canonical_binder_env env;
	uint64_t hash = PROTOTYPE_CANONICAL_HASH_OFFSET;
	memset(&env, 0, sizeof(env));
	memset(p_key, 0, sizeof(*p_key));
	if (canonical_hash_term_at_depth(
			db,
			NULL,
			match_term,
			&env,
			&p_key->match_key,
			&hash,
			1,
			0
		) != 0) {
		return -1;
	}
	p_key->match_key.hash = hash;
	p_key->case_count = db->terms[match_term].as.match.case_count;
	p_key->is_linkable =
		p_key->match_key.free_binder_count == 0 &&
		!p_key->match_key.has_frame_local_reference;
	return 0;
}
