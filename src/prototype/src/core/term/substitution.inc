static int term_contains_free_binder_at_depth(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t depth
) {
	if (!db || term_id >= db->term_count || depth > 128) {
		return 0;
	}
	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR:
			return term->as.var.binding_id == binding_id;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return term_contains_free_binder_at_depth(
				db,
				term->as.constructor.owner,
				binding_id,
				depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return term_contains_free_binder_at_depth(
				db, term->as.return_term.value, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return term_contains_free_binder_at_depth(
				db, term->as.thunk_type.computation, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return term_contains_free_binder_at_depth(
				db, term->as.thunk.computation, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return term_contains_free_binder_at_depth(
				db, term->as.force.value, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (term_contains_free_binder_at_depth(
					db, term->as.computation_fold.computation, binding_id, depth + 1
				)) {
				return 1;
			}
			if (term_contains_free_binder_at_depth(
					db, term->as.computation_fold.return_clause, binding_id, depth + 1
				)) {
				return 1;
			}
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&db->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				if (term_contains_free_binder_at_depth(
						db, clause->operation, binding_id, depth + 1
					) || term_contains_free_binder_at_depth(
						db, clause->body, binding_id, depth + 1
					)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return term_contains_free_binder_at_depth(
				db, term->as.operation_request.operation, binding_id, depth + 1
			) || term_contains_free_binder_at_depth(
				db, term->as.operation_request.argument, binding_id, depth + 1
			) || term_contains_free_binder_at_depth(
				db, term->as.operation_request.continuation, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return term->as.effect_row_var.binding_id == binding_id;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return term_contains_free_binder_at_depth(
				db, term->as.effect_row_union.left, binding_id, depth + 1
			) || term_contains_free_binder_at_depth(
				db, term->as.effect_row_union.right, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return term->as.effect_row_forall.binding_id == binding_id ? 0 :
				term_contains_free_binder_at_depth(
					db, term->as.effect_row_forall.body, binding_id, depth + 1
				);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return term_contains_free_binder_at_depth(
				db, term->as.effect_row_operation.latent_row, binding_id, depth + 1
			);
		case PROTOTYPE_TERM_APP:
			return term_contains_free_binder_at_depth(
					db,
					term->as.app.function,
					binding_id,
					depth + 1
				) ||
				term_contains_free_binder_at_depth(
					db,
					term->as.app.argument,
					binding_id,
					depth + 1
				);
		case PROTOTYPE_TERM_LAMBDA:
			if (term->as.lambda.binding_id == binding_id) {
				return 0;
			}
			return term_contains_free_binder_at_depth(
				db,
				term->as.lambda.body,
				binding_id,
				depth + 1
			);
		case PROTOTYPE_TERM_PI: {
			if (term_contains_free_binder_at_depth(
					db,
					term->as.pi.domain,
					binding_id,
					depth + 1
				)) {
				return 1;
			}
			const struct prototype_term* family = &db->terms[term->as.pi.codomain_family];
			if (family->tag == PROTOTYPE_TERM_LAMBDA &&
				family->as.lambda.binding_id == binding_id) {
				return 0;
			}
			return term_contains_free_binder_at_depth(
				db,
				term->as.pi.codomain_family,
				binding_id,
				depth + 1
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			if (term_contains_free_binder_at_depth(
				db,
				term->as.match.scrutinee,
				binding_id,
				depth + 1
			)) {
				return 1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* match_case =
					&db->cases[term->as.match.first_case + i];
				int shadowed = 0;
				for (uint32_t j = 0; j < match_case->binder_count; ++j) {
					const struct prototype_case_binder* case_binder =
						&db->case_binders[match_case->first_binder + j];
					if (case_binder->binding_id == binding_id) {
						shadowed = 1;
					}
				}
				if (!shadowed &&
					term_contains_free_binder_at_depth(db, match_case->body, binding_id, depth + 1)) {
					return 1;
				}
			}
			return 0;
		}
		case PROTOTYPE_TERM_TYPE_VIEW:
			return term_contains_free_binder_at_depth(
					db,
					term->as.type_view.core,
					binding_id,
					depth + 1
				) ||
				term_contains_free_binder_at_depth(
					db,
					term->as.type_view.source,
					binding_id,
				depth + 1
				);
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			return term_contains_free_binder_at_depth(
				db,
				term->as.dimension_action.source,
				binding_id,
				depth + 1
			);
				case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
				return term_contains_free_binder_at_depth(
					db,
					term->as.induction_hypothesis.argument,
					binding_id,
					depth + 1
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return term_contains_free_binder_at_depth(
						db,
						term->as.computation_type.label,
						binding_id,
						depth + 1
					) ||
					term_contains_free_binder_at_depth(
						db,
						term->as.computation_type.result,
						binding_id,
						depth + 1
					);
			default:
				return 0;
		}
	}

int prototype_term_contains_free_binding(
	const struct prototype_term_db* db,
	uint32_t term_id,
	uint32_t binding_id
) {
	return term_contains_free_binder_at_depth(db, term_id, binding_id, 0);
}

int prototype_term_pi(
	struct prototype_term_db* db,
	uint32_t domain,
	uint32_t codomain,
	uint32_t* p_ret
) {
	if (!db || domain >= db->term_count || codomain >= db->term_count) {
		return -1;
	}

	uint32_t binding_id = prototype_term_new_binding(db);
	uint32_t codomain_family;
	if (binding_id == PROTOTYPE_INVALID_ID ||
		prototype_term_pure_family(db, binding_id, codomain, &codomain_family) != 0) {
		return -1;
	}
	return prototype_term_pi_family(db, domain, codomain_family, p_ret);
}

static int substitute_term(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t replacement,
	uint32_t* p_ret
) {
	const struct prototype_binding_replacement binding = {
		.binding_id = binding_id,
		.replacement = replacement
	};
	struct substitution_context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.type_declarations = type_declarations;
	ctx.bindings = &binding;
	ctx.binding_count = 1;
	int status = substitute_term_internal(
		db,
		term_id,
		&ctx,
		p_ret
	);
	free(ctx.memo);
	free(ctx.memo_buckets);
	return status;
}

static int substitution_binding_is_bound(
	const struct substitution_context* ctx,
	uint32_t binding_id
) {
	if (!ctx) {
		return 0;
	}
	for (const struct substitution_binding_scope* scope = ctx->binding_scope;
		scope;
		scope = scope->previous) {
		if (scope->binding_id == binding_id) {
			return 1;
		}
	}
	return 0;
}

static int substitution_lookup_binding(
	const struct substitution_context* ctx,
	uint32_t binding_id,
	uint32_t* p_replacement
) {
	if (!ctx || !p_replacement || substitution_binding_is_bound(ctx, binding_id)) {
		return 0;
	}
	for (size_t i = 0; i < ctx->binding_count; ++i) {
		if (ctx->bindings[i].binding_id == binding_id) {
			*p_replacement = ctx->bindings[i].replacement;
			return 1;
		}
	}
	return 0;
}

static uint64_t substitution_binding_scope_key(
	const struct substitution_context* ctx
) {
	return ctx && ctx->binding_scope ? ctx->binding_scope->scope_key : 0;
}

static uint64_t substitution_ih_scope_clone_key(
	const struct substitution_context* ctx
) {
	return ctx && ctx->ih_scope_clone ? ctx->ih_scope_clone->scope_key : 0;
}

static uint64_t substitution_memo_hash(
	uint32_t term_id,
	uint64_t binding_scope_key,
	uint64_t ih_scope_clone_key
) {
	uint64_t hash = UINT64_C(1469598103934665603);
	hash = (hash ^ term_id) * UINT64_C(1099511628211);
	hash = (hash ^ binding_scope_key) * UINT64_C(1099511628211);
	hash = (hash ^ ih_scope_clone_key) * UINT64_C(1099511628211);
	return hash;
}

static int substitution_memo_reserve(
	struct substitution_context* ctx,
	size_t required
) {
	if (!ctx) {
		return -1;
	}
	if (required > ctx->memo_capacity) {
		size_t capacity = ctx->memo_capacity ? ctx->memo_capacity * 2 : 64;
		while (capacity < required) {
			if (capacity > SIZE_MAX / 2) {
				return -1;
			}
			capacity *= 2;
		}
		struct substitution_memo_entry* memo = realloc(
			ctx->memo, capacity * sizeof(*memo)
		);
		if (!memo) {
			return -1;
		}
		ctx->memo = memo;
		ctx->memo_capacity = capacity;
	}
	size_t bucket_count = 128;
	while (bucket_count < ctx->memo_capacity * 2) {
		if (bucket_count > SIZE_MAX / 2) {
			return -1;
		}
		bucket_count *= 2;
	}
	if (ctx->memo_bucket_count == bucket_count) {
		return 0;
	}
	uint32_t* buckets = malloc(bucket_count * sizeof(*buckets));
	if (!buckets) {
		return -1;
	}
	for (size_t i = 0; i < bucket_count; ++i) {
		buckets[i] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < ctx->memo_count; ++i) {
		struct substitution_memo_entry* entry = &ctx->memo[i];
		size_t bucket = (size_t)substitution_memo_hash(
			entry->term_id,
			entry->binding_scope_key,
			entry->ih_scope_clone_key
		) & (bucket_count - 1);
		entry->next = buckets[bucket];
		buckets[bucket] = i;
	}
	free(ctx->memo_buckets);
	ctx->memo_buckets = buckets;
	ctx->memo_bucket_count = bucket_count;
	return 0;
}

static int substitution_memo_lookup(
	const struct substitution_context* ctx,
	uint32_t term_id,
	uint32_t* p_result
) {
	if (!ctx || !p_result ||
		(ctx->binding_count == 0 && !ctx->replace_exact_term) ||
		ctx->memo_bucket_count == 0) {
		return 0;
	}
	uint64_t binding_scope_key = substitution_binding_scope_key(ctx);
	uint64_t ih_scope_clone_key = substitution_ih_scope_clone_key(ctx);
	size_t bucket = (size_t)substitution_memo_hash(
		term_id, binding_scope_key, ih_scope_clone_key
	) & (ctx->memo_bucket_count - 1);
	for (uint32_t i = ctx->memo_buckets[bucket];
		i != PROTOTYPE_INVALID_ID;
		i = ctx->memo[i].next) {
		if (i >= ctx->memo_count) {
			return 0;
		}
		const struct substitution_memo_entry* entry = &ctx->memo[i];
		if (entry->term_id == term_id &&
			entry->binding_scope_key == binding_scope_key &&
			entry->ih_scope_clone_key == ih_scope_clone_key) {
			*p_result = entry->result;
			return 1;
		}
	}
	return 0;
}

static int substitution_memo_store(
	struct substitution_context* ctx,
	uint32_t term_id,
	uint32_t result
) {
	if (!ctx || (ctx->binding_count == 0 && !ctx->replace_exact_term)) {
		return 0;
	}
	if (substitution_memo_reserve(ctx, ctx->memo_count + 1) != 0) {
		return -1;
	}
	uint64_t binding_scope_key = substitution_binding_scope_key(ctx);
	uint64_t ih_scope_clone_key = substitution_ih_scope_clone_key(ctx);
	size_t bucket = (size_t)substitution_memo_hash(
		term_id, binding_scope_key, ih_scope_clone_key
	) & (ctx->memo_bucket_count - 1);
	uint32_t id = (uint32_t)ctx->memo_count++;
	ctx->memo[id] = (struct substitution_memo_entry) {
		.term_id = term_id,
		.result = result,
		.binding_scope_key = binding_scope_key,
		.ih_scope_clone_key = ih_scope_clone_key,
		.next = ctx->memo_buckets[bucket]
	};
	ctx->memo_buckets[bucket] = id;
	return 0;
}

static int substitution_term_has_binding(
	const struct prototype_term_db* db,
	const struct substitution_context* ctx,
	uint32_t term_id
) {
	if (!db || !ctx || term_id >= db->term_count) {
		return 0;
	}
	for (size_t i = 0; i < ctx->binding_count; ++i) {
		if (!substitution_binding_is_bound(ctx, ctx->bindings[i].binding_id) &&
			prototype_term_contains_free_binding(
				db, term_id, ctx->bindings[i].binding_id
			)) {
			return 1;
		}
	}
	return 0;
}

static int substitution_lookup_frame(
	const struct substitution_context* ctx,
	uint32_t old_ih_scope_id,
	uint32_t* p_new_frame_id
) {
	if (!ctx || !p_new_frame_id) {
		return 0;
	}
	for (const struct substitution_ih_scope_clone* scope = ctx->ih_scope_clone;
		scope;
		scope = scope->previous) {
		if (scope->old_ih_scope_id == old_ih_scope_id) {
			*p_new_frame_id = scope->new_ih_scope_id;
			return 1;
		}
	}
	return 0;
}

static int substitution_has_ih_scope_clone(
	const struct substitution_context* ctx,
	uint32_t ih_scope_id
) {
	uint32_t ignored;
	return substitution_lookup_frame(ctx, ih_scope_id, &ignored);
}

static int term_contains_ih_scope_clone_reference_at_depth(
	const struct prototype_term_db* db,
	const struct substitution_context* ctx,
	uint32_t term_id,
	unsigned depth
) {
	if (!db || !ctx || term_id >= db->term_count || depth > 100000) {
		return 0;
	}
	if (!ctx->ih_scope_clone) {
		return 0;
	}
	const struct prototype_term* term = &db->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_APP:
			return term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.app.function,
					depth + 1
				) ||
				term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.app.argument,
					depth + 1
				);
		case PROTOTYPE_TERM_RETURN:
			return term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.return_term.value, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.thunk_type.computation, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.thunk.computation, depth + 1
			);
		case PROTOTYPE_TERM_FORCE:
			return term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.force.value, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_FOLD:
			if (term_contains_ih_scope_clone_reference_at_depth(
					db, ctx, term->as.computation_fold.computation, depth + 1
				) || term_contains_ih_scope_clone_reference_at_depth(
					db, ctx, term->as.computation_fold.return_clause, depth + 1
				)) {
				return 1;
			}
			for (uint32_t i = 0; i < term->as.computation_fold.clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&db->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				if (term_contains_ih_scope_clone_reference_at_depth(
						db, ctx, clause->operation, depth + 1
					) || term_contains_ih_scope_clone_reference_at_depth(
						db, ctx, clause->body, depth + 1
					)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_OPERATION_REQUEST:
			return term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.operation_request.operation, depth + 1
			) || term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.operation_request.argument, depth + 1
			) || term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.operation_request.continuation, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_UNION:
			return term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.effect_row_union.left, depth + 1
			) || term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.effect_row_union.right, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_VAR:
			return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.effect_row_forall.body, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION:
			return term_contains_ih_scope_clone_reference_at_depth(
				db, ctx, term->as.effect_row_operation.latent_row, depth + 1
			);
		case PROTOTYPE_TERM_LAMBDA:
			return term_contains_ih_scope_clone_reference_at_depth(
				db,
				ctx,
				term->as.lambda.body,
				depth + 1
			);
		case PROTOTYPE_TERM_PI:
			return term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.pi.domain,
					depth + 1
				) ||
				term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.pi.codomain_family,
					depth + 1
				);
		case PROTOTYPE_TERM_MATCH:
			if (term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.match.scrutinee,
					depth + 1
				)) {
				return 1;
			}
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				uint32_t case_id = term->as.match.first_case + i;
				if (case_id >= db->case_count) {
					return 0;
				}
				const struct prototype_match_case* match_case = &db->cases[case_id];
				if ((match_case->constructor_owner != PROTOTYPE_INVALID_ID &&
						term_contains_ih_scope_clone_reference_at_depth(
							db,
							ctx,
							match_case->constructor_owner,
							depth + 1
						)) ||
					term_contains_ih_scope_clone_reference_at_depth(
						db,
						ctx,
						match_case->body,
						depth + 1
					)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_TERM_CONSTRUCTOR:
			return term_contains_ih_scope_clone_reference_at_depth(
				db,
				ctx,
				term->as.constructor.owner,
				depth + 1
			);
		case PROTOTYPE_TERM_TYPE_VIEW:
			return term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.type_view.core,
					depth + 1
				) ||
				term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.type_view.source,
				depth + 1
				);
		case PROTOTYPE_TERM_DIMENSION_ACTION:
			return term_contains_ih_scope_clone_reference_at_depth(
				db,
				ctx,
				term->as.dimension_action.source,
				depth + 1
			);
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS:
			return substitution_has_ih_scope_clone(
					ctx,
					term->as.induction_hypothesis.ih_scope_id
				) ||
				term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.induction_hypothesis.argument,
					depth + 1
				);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.computation_type.label,
					depth + 1
				) ||
				term_contains_ih_scope_clone_reference_at_depth(
					db,
					ctx,
					term->as.computation_type.result,
					depth + 1
				);
		default:
			return 0;
	}
}

static int term_contains_ih_scope_clone_reference(
	const struct prototype_term_db* db,
	const struct substitution_context* ctx,
	uint32_t term_id
) {
	return term_contains_ih_scope_clone_reference_at_depth(db, ctx, term_id, 0);
}

static int substitute_term_internal(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct substitution_context* ctx,
	uint32_t* p_ret
) {
	if (!db || !p_ret || !ctx || term_id >= db->term_count) {
		return -1;
	}
	if (ctx->replace_exact_term && term_id == ctx->exact_term) {
		*p_ret = ctx->exact_replacement;
		return 0;
	}
	if (substitution_memo_lookup(ctx, term_id, p_ret)) {
		return 0;
	}
	int status = substitute_term_uncached_internal(
		db, term_id, ctx, p_ret
	);
	if (status == 0 && substitution_memo_store(ctx, term_id, *p_ret) != 0) {
		return -1;
	}
	return status;
}

static int substitute_term_uncached_internal(
	struct prototype_term_db* db,
	uint32_t term_id,
	struct substitution_context* ctx,
	uint32_t* p_ret
) {
	if (!db || !p_ret || !ctx || term_id >= db->term_count) {
		return -1;
	}

	/* Recursive substitution may intern new terms and grow db->terms. Keep a
	 * stable value copy rather than a pointer into the reallocatable array. */
	const struct prototype_term term_value = db->terms[term_id];
	const struct prototype_term* term = &term_value;
	switch (term->tag) {
		case PROTOTYPE_TERM_VAR: {
			uint32_t direct_replacement;
			if (substitution_lookup_binding(
					ctx, term->as.var.binding_id, &direct_replacement
				)) {
				*p_ret = direct_replacement;
			} else {
				*p_ret = term_id;
			}
			return 0;
		}
		case PROTOTYPE_TERM_EFFECT_ROW_VAR: {
			uint32_t direct_replacement;
			if (substitution_lookup_binding(
					ctx,
					term->as.effect_row_var.binding_id,
					&direct_replacement
				)) {
				*p_ret = direct_replacement;
			} else {
				*p_ret = term_id;
			}
			return 0;
		}
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			uint32_t argument;
			if (substitute_term_internal(
					db,
					term->as.app.function,
					ctx,
					&function
				) != 0) {
				return -1;
			}
			if (substitute_term_internal(
					db,
					term->as.app.argument,
					ctx,
					&argument
				) != 0) {
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
			if (substitute_term_internal(
					db,
					term->as.pi.domain,
					ctx,
					&domain
				) != 0 ||
				substitute_term_internal(
					db,
					term->as.pi.codomain_family,
					ctx,
					&codomain_family
				) != 0) {
				return -1;
			}
			if (domain == term->as.pi.domain && codomain_family == term->as.pi.codomain_family) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_pi_family(db, domain, codomain_family, p_ret);
		}
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			uint32_t argument;
			if (substitute_term_internal(
				db,
				term->as.induction_hypothesis.argument,
					ctx,
				&argument
			) != 0) {
				return -1;
			}
			uint32_t ih_scope_id = term->as.induction_hypothesis.ih_scope_id;
			substitution_lookup_frame(ctx, ih_scope_id, &ih_scope_id);
			if (argument == term->as.induction_hypothesis.argument &&
				ih_scope_id == term->as.induction_hypothesis.ih_scope_id) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_induction_hypothesis(
				db,
				ih_scope_id,
				argument,
				p_ret
			);
			}
		case PROTOTYPE_TERM_MATCH: {
			uint32_t recursive_subject_binding = PROTOTYPE_INVALID_ID;
			if (term->as.match.ih_scope_id != PROTOTYPE_INVALID_ID) {
				if (term->as.match.ih_scope_id >= db->ih_scope_count) {
					return -1;
				}
				recursive_subject_binding = db->ih_scopes[
					term->as.match.ih_scope_id
				].scrutinee_binding_id;
			}
			uint32_t scrutinee;
			if (substitute_term_internal(
					db,
					term->as.match.scrutinee,
					ctx,
					&scrutinee
				) != 0) {
				return -1;
			}
			uint32_t relocated_recursive_subject_binding =
				recursive_subject_binding;
			int preserve_recursive_subject_binding =
				recursive_subject_binding != PROTOTYPE_INVALID_ID;
			if (recursive_subject_binding != PROTOTYPE_INVALID_ID &&
				scrutinee < db->term_count &&
				db->terms[scrutinee].tag == PROTOTYPE_TERM_VAR) {
				relocated_recursive_subject_binding =
					db->terms[scrutinee].as.var.binding_id;
				preserve_recursive_subject_binding =
					relocated_recursive_subject_binding ==
						recursive_subject_binding;
			}

			struct prototype_match_case_input case_inputs[64];
			struct prototype_case_binder binder_storage[256];
			struct substitution_binding_scope scope_storage[256];
			memset(case_inputs, 0, sizeof(case_inputs));
			memset(binder_storage, 0, sizeof(binder_storage));
			memset(scope_storage, 0, sizeof(scope_storage));
			uint32_t binder_cursor = 0;
			int changed = scrutinee != term->as.match.scrutinee;
			if (term->as.match.case_count > 64) {
				return -1;
			}

			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&db->cases[term->as.match.first_case + i];
				if (old_case->binder_count > 0 && binder_cursor + old_case->binder_count > 256) {
					return -1;
				}

				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					struct prototype_case_binder case_binder = db->case_binders[old_case->first_binder + j];
					binder_storage[binder_cursor + j] = case_binder;
				}
				const struct substitution_binding_scope* saved_binding_scope =
					ctx->binding_scope;
				struct substitution_binding_scope subject_scope;
				if (ctx->binding_count > 0 &&
					preserve_recursive_subject_binding) {
					subject_scope.binding_id = recursive_subject_binding;
					subject_scope.scope_key = ++ctx->next_scope_key;
					subject_scope.previous = ctx->binding_scope;
					ctx->binding_scope = &subject_scope;
				}
				if (ctx->binding_count > 0) {
					for (uint32_t j = 0; j < old_case->binder_count; ++j) {
						scope_storage[binder_cursor + j].binding_id =
							binder_storage[binder_cursor + j].binding_id;
						scope_storage[binder_cursor + j].scope_key =
							++ctx->next_scope_key;
						scope_storage[binder_cursor + j].previous =
							ctx->binding_scope;
						ctx->binding_scope = &scope_storage[binder_cursor + j];
					}
				}
				if (term_contains_ih_scope_clone_reference(db, ctx, old_case->body) ||
					substitution_term_has_binding(db, ctx, old_case->body)) {
					changed = 1;
				}
				ctx->binding_scope = saved_binding_scope;
				if (old_case->constructor_owner != PROTOTYPE_INVALID_ID &&
					(term_contains_ih_scope_clone_reference(
							db,
							ctx,
							old_case->constructor_owner
						) || substitution_term_has_binding(
							db, ctx, old_case->constructor_owner
						))) {
					changed = 1;
				}
				binder_cursor += old_case->binder_count;
			}

			if (!changed) {
				*p_ret = term_id;
				return 0;
			}

			uint32_t ih_scope_id = term->as.match.ih_scope_id;
			const struct substitution_ih_scope_clone* saved_ih_scope_clone = ctx->ih_scope_clone;
			struct substitution_ih_scope_clone ih_scope_clone;
			if (ih_scope_id != PROTOTYPE_INVALID_ID) {
				uint32_t new_ih_scope_id = prototype_term_new_ih_scope(db);
				if (new_ih_scope_id == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				ih_scope_clone.old_ih_scope_id = ih_scope_id;
				ih_scope_clone.new_ih_scope_id = new_ih_scope_id;
				ih_scope_clone.scope_key = ++ctx->next_scope_key;
				ih_scope_clone.previous = saved_ih_scope_clone;
				ctx->ih_scope_clone = &ih_scope_clone;
				db->ih_scopes[new_ih_scope_id].scrutinee_binding_id =
					relocated_recursive_subject_binding;
				ih_scope_id = new_ih_scope_id;
			}

			binder_cursor = 0;
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&db->cases[term->as.match.first_case + i];
				uint32_t body = old_case->body;
				uint32_t constructor_owner = old_case->constructor_owner;
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					struct prototype_case_binder case_binder =
						db->case_binders[old_case->first_binder + j];
					binder_storage[binder_cursor + j] = case_binder;
				}
				const struct substitution_binding_scope* saved_binding_scope =
					ctx->binding_scope;
				struct substitution_binding_scope subject_scope;
				if (ctx->binding_count > 0 &&
					preserve_recursive_subject_binding) {
					subject_scope.binding_id = recursive_subject_binding;
					subject_scope.scope_key = ++ctx->next_scope_key;
					subject_scope.previous = ctx->binding_scope;
					ctx->binding_scope = &subject_scope;
				}
				if (ctx->binding_count > 0) {
					for (uint32_t j = 0; j < old_case->binder_count; ++j) {
						scope_storage[binder_cursor + j].binding_id =
							binder_storage[binder_cursor + j].binding_id;
						scope_storage[binder_cursor + j].scope_key =
							++ctx->next_scope_key;
						scope_storage[binder_cursor + j].previous =
							ctx->binding_scope;
						ctx->binding_scope = &scope_storage[binder_cursor + j];
					}
				}
				if (substitute_term_internal(
						db,
						old_case->body,
						ctx,
						&body
					) != 0) {
					ctx->binding_scope = saved_binding_scope;
					ctx->ih_scope_clone = saved_ih_scope_clone;
					return -1;
				}
				ctx->binding_scope = saved_binding_scope;
				if (constructor_owner != PROTOTYPE_INVALID_ID &&
					substitute_term_internal(
						db,
						constructor_owner,
					ctx,
						&constructor_owner
					) != 0) {
					ctx->ih_scope_clone = saved_ih_scope_clone;
					return -1;
				}
				case_inputs[i].case_label_symbol_id =
					db->case_label_symbols[term->as.match.first_case + i];
				case_inputs[i].constructor_owner = constructor_owner;
				case_inputs[i].constructor_id = old_case->constructor_id;
				case_inputs[i].binders = &binder_storage[binder_cursor];
				case_inputs[i].binder_count = old_case->binder_count;
				case_inputs[i].body = body;
				binder_cursor += old_case->binder_count;
			}

			uint32_t match_term;
			int status = prototype_term_match_with_ih_scope(
				db,
				scrutinee,
				case_inputs,
				term->as.match.case_count,
				ih_scope_id,
				&match_term
			);
			if (status == 0 && ih_scope_id != PROTOTYPE_INVALID_ID) {
				status = prototype_term_set_ih_scope_term(db, ih_scope_id, match_term);
			}
			ctx->ih_scope_clone = saved_ih_scope_clone;
			if (status != 0) {
				return -1;
			}
			*p_ret = match_term;
			return 0;
		}
		case PROTOTYPE_TERM_LAMBDA: {
			int tag = term->tag;
			const struct substitution_binding_scope* saved_binding_scope =
				ctx->binding_scope;
			struct substitution_binding_scope binding_scope;
			if (ctx->binding_count > 0) {
				binding_scope.binding_id = term->as.lambda.binding_id;
				binding_scope.scope_key = ++ctx->next_scope_key;
				binding_scope.previous = saved_binding_scope;
				ctx->binding_scope = &binding_scope;
			}
			uint32_t body;
			if (substitute_term_internal(
					db,
					term->as.lambda.body,
					ctx,
					&body
				) != 0) {
				ctx->binding_scope = saved_binding_scope;
				return -1;
			}
			ctx->binding_scope = saved_binding_scope;
			if (body == term->as.lambda.body) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_lambda_with_tag(
				db,
				tag,
				term->as.lambda.binding_id,
				body,
				p_ret
			);
		}
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t owner;
			if (substitute_term_internal(
					db,
					term->as.constructor.owner,
					ctx,
					&owner
				) != 0) {
				return -1;
			}
			if (owner == term->as.constructor.owner) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_constructor(
				db,
				owner,
				term->as.constructor.constructor_id,
				p_ret
			);
			}
		case PROTOTYPE_TERM_DIMENSION_ACTION: {
			uint32_t source;
			if (substitute_term_internal(
					db, term->as.dimension_action.source, ctx, &source
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
			case PROTOTYPE_TERM_TYPE_FORMER:
			case PROTOTYPE_TERM_TYPE_DECLARATION:
				case PROTOTYPE_TERM_PRIMITIVE_TEXT:
			case PROTOTYPE_TERM_TEXT_LITERAL:
			case PROTOTYPE_TERM_PRIMITIVE_INT:
			case PROTOTYPE_TERM_PRIMITIVE_INT64:
				case PROTOTYPE_TERM_INT_LITERAL:
			case PROTOTYPE_TERM_PURE_PRIMITIVE:
			case PROTOTYPE_TERM_EFFECT_OPERATION:
				case PROTOTYPE_TERM_EFFECT_ROW_EMPTY:
					*p_ret = term_id;
					return 0;
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			uint32_t left;
			uint32_t right;
			if (substitute_term_internal(
					db, term->as.effect_row_union.left, ctx, &left
				) != 0 || substitute_term_internal(
					db, term->as.effect_row_union.right, ctx, &right
				) != 0) {
				return -1;
			}
			return left == term->as.effect_row_union.left &&
				right == term->as.effect_row_union.right ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_union(db, left, right, p_ret);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			const struct substitution_binding_scope* saved_binding_scope =
				ctx->binding_scope;
			struct substitution_binding_scope binding_scope;
			if (ctx->binding_count > 0) {
				binding_scope.binding_id = term->as.effect_row_forall.binding_id;
				binding_scope.scope_key = ++ctx->next_scope_key;
				binding_scope.previous = saved_binding_scope;
				ctx->binding_scope = &binding_scope;
			}
			uint32_t body;
			if (substitute_term_internal(
					db,
					term->as.effect_row_forall.body,
					ctx,
					&body
				) != 0) {
				ctx->binding_scope = saved_binding_scope;
				return -1;
			}
			ctx->binding_scope = saved_binding_scope;
			return body == term->as.effect_row_forall.body ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_forall(
					db, term->as.effect_row_forall.binding_id, body, p_ret
				);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION: {
			uint32_t latent_row;
			if (substitute_term_internal(
					db,
					term->as.effect_row_operation.latent_row,
					ctx,
					&latent_row
				) != 0) {
				return -1;
			}
			return latent_row == term->as.effect_row_operation.latent_row ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_operation(
					db, term->as.effect_row_operation.operation_id, latent_row, p_ret
				);
		}
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
				uint32_t label;
				uint32_t result;
				if (substitute_term_internal(
						db,
						term->as.computation_type.label,
					ctx,
						&label
					) != 0 ||
					substitute_term_internal(
						db,
						term->as.computation_type.result,
					ctx,
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
				if (substitute_term_internal(
						db, term->as.thunk_type.computation, ctx, &computation
					) != 0) {
					return -1;
				}
				return computation == term->as.thunk_type.computation ?
					(*p_ret = term_id, 0) : prototype_term_thunk_type(db, computation, p_ret);
			}
			case PROTOTYPE_TERM_RETURN: {
				uint32_t value;
				if (substitute_term_internal(
						db, term->as.return_term.value, ctx, &value
					) != 0) {
					return -1;
				}
				return value == term->as.return_term.value ?
					(*p_ret = term_id, 0) : prototype_term_return(db, value, p_ret);
			}
			case PROTOTYPE_TERM_THUNK: {
				uint32_t computation;
				if (substitute_term_internal(
						db, term->as.thunk.computation, ctx, &computation
					) != 0) {
					return -1;
				}
				return computation == term->as.thunk.computation ?
					(*p_ret = term_id, 0) : prototype_term_thunk(db, computation, p_ret);
			}
			case PROTOTYPE_TERM_FORCE: {
				uint32_t value;
				if (substitute_term_internal(
						db, term->as.force.value, ctx, &value
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
				if (substitute_term_internal(
						db, term->as.computation_fold.computation, ctx, &computation
					) != 0 || substitute_term_internal(
						db, term->as.computation_fold.return_clause, ctx, &return_clause
					) != 0) {
					free(clauses);
					return -1;
				}
				int unchanged = computation == term->as.computation_fold.computation &&
					return_clause == term->as.computation_fold.return_clause;
				for (uint32_t i = 0; i < clause_count; ++i) {
					const struct prototype_computation_fold_clause* clause =
						&db->computation_fold_clauses[term->as.computation_fold.first_clause + i];
					if (substitute_term_internal(
							db, clause->operation, ctx,
							&clauses[i].operation
						) != 0 || substitute_term_internal(
							db, clause->body, ctx, &clauses[i].body
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
				if (substitute_term_internal(
						db, term->as.operation_request.operation, ctx, &operation
					) != 0 || substitute_term_internal(
						db, term->as.operation_request.argument, ctx, &argument
					) != 0 || substitute_term_internal(
						db, term->as.operation_request.continuation, ctx, &continuation
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
			case PROTOTYPE_TERM_TYPE_VIEW: {
			uint32_t core;
			uint32_t source;
			if (substitute_term_internal(
					db,
					term->as.type_view.core,
					ctx,
					&core
				) != 0 ||
				substitute_term_internal(
					db,
					term->as.type_view.source,
					ctx,
					&source
				) != 0) {
				return -1;
			}
			if (core == term->as.type_view.core &&
				source == term->as.type_view.source) {
				*p_ret = term_id;
				return 0;
			}
			if (!ctx->type_declarations) {
				/* Erased computation substitutes the graph structurally. Recomputing
				 * a named TypeView is a pure-type elaboration action and must not make
				 * beta or iota reduction depend on TypeDeclarationDB. */
				return prototype_term_type_view(
					db,
					term->as.type_view.view_type_id,
					term->as.type_view.identity,
					core,
					source,
					p_ret
				);
			}
			return prototype_term_type_view_rebuild_from_source(
				db,
				ctx->type_declarations,
				term->as.type_view.view_type_id,
				core,
				source,
				p_ret
			);
		}
			default:
				*p_ret = term_id;
				return 0;
	}
}

int prototype_term_graph_substitute_bound_var(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t binding_id,
	uint32_t replacement,
	uint32_t* p_ret
) {
	return substitute_term(db, type_declarations, term_id, binding_id, replacement, p_ret);
}

int prototype_term_graph_replace_exact(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t exact_term,
	uint32_t replacement,
	uint32_t* p_ret
) {
	if (!db || !p_ret || term_id >= db->term_count ||
		exact_term >= db->term_count || replacement >= db->term_count) {
		return -1;
	}
	struct substitution_context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.type_declarations = type_declarations;
	ctx.exact_term = exact_term;
	ctx.exact_replacement = replacement;
	ctx.replace_exact_term = 1;
	int status = substitute_term_internal(db, term_id, &ctx, p_ret);
	free(ctx.memo);
	free(ctx.memo_buckets);
	return status;
}

int prototype_term_graph_reindex_bindings(
	struct prototype_term_db* db,
	struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	const struct prototype_binding_replacement* bindings,
	size_t binding_count,
	uint32_t* p_ret
) {
	if (!db || !p_ret || term_id >= db->term_count ||
		(binding_count > 0 && !bindings)) {
		return -1;
	}
	for (size_t i = 0; i < binding_count; ++i) {
		if (bindings[i].binding_id == PROTOTYPE_INVALID_ID ||
			bindings[i].replacement >= db->term_count) {
			return -1;
		}
		for (size_t j = 0; j < i; ++j) {
			if (bindings[j].binding_id == bindings[i].binding_id) {
				return -1;
			}
		}
	}
	if (binding_count == 0) {
		*p_ret = term_id;
		return 0;
	}

	struct substitution_context ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.type_declarations = type_declarations;
	ctx.bindings = bindings;
	ctx.binding_count = binding_count;
	int status = substitute_term_internal(
		db,
		term_id,
		&ctx,
		p_ret
	);
	free(ctx.memo);
	free(ctx.memo_buckets);
	return status;
}
