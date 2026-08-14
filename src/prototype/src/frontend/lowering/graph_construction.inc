struct match_compile_state {
	uint32_t match_ast;
	uint32_t scrutinee;
	uint32_t scrutinee_operation;
	uint32_t ih_scope_id;
	struct prototype_match_case_input case_inputs[64];
	uint32_t resolution_item_ids[64];
	int case_constructor_symbols[64];
	uint32_t branch_operations[64];
	int branch_computation_kinds[64];
	struct prototype_case_binder binder_storage[256];
	uint32_t binder_cursor;
	int (*compile_branch_body)(
		struct compile_context*, uint32_t, void*, struct compile_ref*
	);
	void* compile_branch_data;
};

struct compiled_match_branch {
	const struct prototype_case_binder* binders;
	uint32_t binder_count;
	uint32_t body;
	uint32_t operation;
};

static int create_match_pattern_binders(
	struct compile_context* ctx,
	struct match_compile_state* state,
	const struct prototype_ast_match_case* old_case,
	uint32_t case_index,
	uint32_t previous_binder_count,
	uint32_t previous_ih_scope_count,
	uint32_t* p_binder_start
) {
	if (!ctx || !state || !old_case || !p_binder_start ||
		state->binder_cursor + old_case->binder_count > 256) {
		return -1;
	}
	*p_binder_start = state->binder_cursor;
	for (uint32_t j = 0; j < old_case->binder_count; ++j) {
		const struct prototype_ast_binder* ast_binder =
			&ctx->asts->case_binders[old_case->first_binder + j];
		uint32_t graph_binder_id;
		if (push_graph_binder(
			ctx,
			ast_binder->ast_binder_id,
			PROTOTYPE_INVALID_ID,
			ast_binder->symbol_id,
			&graph_binder_id
		) != 0) {
			ctx->binder_count = previous_binder_count;
			return -1;
		}
		if (ctx->ih_scope_count >= 512) {
			ctx->binder_count = previous_binder_count;
			ctx->ih_scope_count = previous_ih_scope_count;
			return -1;
		}
		ctx->ih_scopes[ctx->ih_scope_count].ast_binder_id =
			ast_binder->ast_binder_id;
		ctx->ih_scopes[ctx->ih_scope_count].ih_scope_id = state->ih_scope_id;
		ctx->ih_scopes[ctx->ih_scope_count].case_index = case_index;
		ctx->ih_scopes[ctx->ih_scope_count].field_index = j;
		ctx->ih_scope_count++;
		state->binder_storage[state->binder_cursor + j].binding_id = graph_binder_id;
	}
	return 0;
}

static int prepare_match_pattern_environment(
	struct compile_context* ctx,
	struct match_compile_state* state,
	const struct prototype_ast_match_case* old_case,
	uint32_t case_index,
	uint32_t previous_binder_count,
	uint32_t previous_ih_scope_count,
	uint32_t* p_binder_start
) {
	if (create_match_pattern_binders(
		ctx,
		state,
		old_case,
		case_index,
		previous_binder_count,
		previous_ih_scope_count,
		p_binder_start
	) != 0) {
		return -1;
	}
	(void)previous_binder_count;
	(void)previous_ih_scope_count;
	return 0;
}

static int compile_match_branch_body(
	struct compile_context* ctx,
	struct match_compile_state* state,
	const struct prototype_ast_match_case* old_case,
	int case_label_symbol_id,
	uint32_t case_index,
	struct compiled_match_branch* branch
) {
	struct compile_ref body_ref;
	if (!ctx || !state || !old_case || !branch) {
		return -1;
	}
	memset(branch, 0, sizeof(*branch));
	branch->binders = &state->binder_storage[state->binder_cursor];
	branch->binder_count = old_case->binder_count;
	state->case_inputs[case_index].case_label_symbol_id = case_label_symbol_id;
	state->case_inputs[case_index].constructor_owner = PROTOTYPE_INVALID_ID;
	state->case_inputs[case_index].constructor_id = PROTOTYPE_INVALID_ID;
	state->case_inputs[case_index].binders = &state->binder_storage[state->binder_cursor];
	state->case_inputs[case_index].binder_count = old_case->binder_count;
	int branch_status = state->compile_branch_body ? state->compile_branch_body(
		ctx, old_case->body, state->compile_branch_data, &body_ref
	) : -1;
	if (branch_status != 0 ||
		body_ref.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return -1;
	}
	branch->body = body_ref.term;
	branch->operation = body_ref.operation;
	state->branch_operations[case_index] = body_ref.operation;
	state->branch_computation_kinds[case_index] = body_ref.computation_kind;
	state->case_inputs[case_index].body = branch->body;
	return 0;
}

static int compile_match_branch(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	struct match_compile_state* state,
	uint32_t case_index
) {
	const struct prototype_ast_match_case* old_case;
	struct compiled_match_branch branch;
	uint32_t previous_binder_count;
	uint32_t previous_ih_scope_count;
	uint32_t binder_start;
	uint32_t resolution_item_id;
	if (!ctx || !node || !state || case_index >= node->as.match.case_count) {
		return -1;
	}
	old_case = &ctx->asts->cases[node->as.match.first_case + case_index];
	previous_binder_count = ctx->binder_count;
	previous_ih_scope_count = ctx->ih_scope_count;
	if (add_match_constructor_resolution_item(
		ctx,
		state->match_ast,
		case_index,
		state->scrutinee,
		old_case->constructor_symbol_id,
		&resolution_item_id
	) != 0) {
		ctx->binder_count = previous_binder_count;
		ctx->ih_scope_count = previous_ih_scope_count;
		return -1;
	}
	state->resolution_item_ids[case_index] = resolution_item_id;
	state->case_constructor_symbols[case_index] = old_case->constructor_symbol_id;
	if (prepare_match_pattern_environment(
		ctx,
		state,
		old_case,
		case_index,
		previous_binder_count,
		previous_ih_scope_count,
		&binder_start
	) != 0 ||
		compile_match_branch_body(
			ctx,
			state,
			old_case,
			old_case->constructor_symbol_id,
			case_index,
			&branch
	) != 0) {
		ctx->binder_count = previous_binder_count;
		ctx->ih_scope_count = previous_ih_scope_count;
		return -1;
	}
	ctx->binder_count = previous_binder_count;
	ctx->ih_scope_count = previous_ih_scope_count;
	state->binder_cursor += old_case->binder_count;
	return 0;
}

static int match_scrutinee_proven_classifier_hint(
	struct compile_context* ctx,
	uint32_t scrutinee_ast,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier || scrutinee_ast >= ctx->asts->node_count) {
		return -1;
	}
	*p_classifier = PROTOTYPE_INVALID_ID;
	const struct prototype_ast_node* scrutinee = &ctx->asts->nodes[scrutinee_ast];
	switch (scrutinee->tag) {
		case PROTOTYPE_AST_VAR:
			if (lookup_graph_binder_classifier(
					ctx,
					scrutinee->as.var.ast_binder_id,
					p_classifier
				) != 0) {
				*p_classifier = PROTOTYPE_INVALID_ID;
			}
			return 0;
			case PROTOTYPE_AST_ASCRIPTION:
				return match_scrutinee_proven_classifier_hint(
					ctx,
					scrutinee->as.ascription.term,
					p_classifier
				);
			default:
				return 0;
		}
	}

static int compile_match_branch_body_default(
	struct compile_context* ctx,
	uint32_t body_ast,
	void* data,
	struct compile_ref* p_ret
) {
	(void)data;
	return compile_ast_computation_ref(ctx, body_ast, p_ret);
}

static int compile_ast_match_from_value_with_branch_compiler(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	const struct compile_ref* scrutinee_ref,
	int (*compile_branch_body)(
		struct compile_context*, uint32_t, void*, struct compile_ref*
	),
	void* compile_branch_data,
	struct compile_ref* p_ref
) {
	struct match_compile_state state;
	uint32_t match_term;
	uint32_t scrutinee_proven_classifier_hint = PROTOTYPE_INVALID_ID;
	if (!ctx || !node || !scrutinee_ref || !p_ref || node->tag != PROTOTYPE_AST_MATCH ||
		scrutinee_ref->category != PROTOTYPE_TERM_CATEGORY_VALUE) {
		return -1;
	}
	memset(&state, 0, sizeof(state));
	state.match_ast = ast_id;
	state.ih_scope_id = PROTOTYPE_INVALID_ID;
	state.compile_branch_body = compile_branch_body;
	state.compile_branch_data = compile_branch_data;
	if (node->as.match.case_count > 64) {
		return -1;
	}
	state.scrutinee = scrutinee_ref->term;
	state.scrutinee_operation = scrutinee_ref->operation;
	scrutinee_proven_classifier_hint = scrutinee_ref->classifier;
	if (scrutinee_proven_classifier_hint == PROTOTYPE_INVALID_ID &&
		match_scrutinee_proven_classifier_hint(
			ctx,
			node->as.match.scrutinee,
			&scrutinee_proven_classifier_hint
		) != 0) {
		return -1;
	}
	state.ih_scope_id = prototype_term_new_ih_scope(ctx->terms);
	if (state.ih_scope_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (compile_match_branch(ctx, node, &state, i) != 0) {
			return -1;
		}
	}
	if (prototype_term_match_with_ih_scope(
		ctx->terms,
		state.scrutinee,
		state.case_inputs,
		node->as.match.case_count,
		state.ih_scope_id,
		&match_term
	) != 0) {
		return -1;
	}
	if (match_term >= ctx->terms->term_count ||
		ctx->terms->terms[match_term].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	if (prototype_term_set_ih_scope_term(ctx->terms, state.ih_scope_id, match_term) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (queue_match_constructor_resolution(
				ctx,
				state.resolution_item_ids[i],
				match_term,
				state.scrutinee_operation,
				scrutinee_proven_classifier_hint,
				state.case_constructor_symbols[i]
			) != 0) {
			return -1;
		}
	}
	uint32_t first_operation_case = (uint32_t)ctx->metadata->operation_case_count;
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		uint32_t operation_case;
		if (operation_add_match_case(
				ctx,
				state.branch_operations[i],
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				&ctx->asts->cases[node->as.match.first_case + i],
				&operation_case
			) != 0) {
			return -1;
		}
		ctx->metadata->operation_cases[operation_case].case_label_symbol_id =
			state.case_constructor_symbols[i];
	}
	p_ref->term = match_term;
	p_ref->classifier = PROTOTYPE_INVALID_ID;
	p_ref->operation = PROTOTYPE_INVALID_ID;
	p_ref->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ref->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (state.branch_computation_kinds[i] !=
			PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION) {
			p_ref->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			break;
		}
	}
	if (operation_add(
			ctx,
			PROTOTYPE_OPERATION_MATCH,
			match_term,
			PROTOTYPE_INVALID_ID,
			ast_id,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			state.scrutinee_operation,
			PROTOTYPE_INVALID_ID,
			first_operation_case,
			node->as.match.case_count,
			&p_ref->operation
		) != 0) {
		return -1;
	}
	/* IH occurrences are lowered before their enclosing Match Operation exists.
	 * Complete that exact typed edge now. The Core IH frame only identifies an
	 * erased match term and cannot distinguish two typed Match occurrences that
	 * intentionally share that term. */
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		struct prototype_operation_node* operation =
			&ctx->metadata->operations[operation_id];
		if (operation->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
			operation->ih_scope_id != state.ih_scope_id) {
			continue;
		}
		if (operation->scrutinee != PROTOTYPE_INVALID_ID &&
			operation->scrutinee != p_ref->operation) {
			return -1;
		}
		operation->scrutinee = p_ref->operation;
	}
	if (queue_match_typing(
			ctx,
			match_term,
			p_ref->operation,
			ctx->asts->next_ast_level_var++
		) != 0) {
		return -1;
	}
	return 0;
}

static int compile_ast_match_from_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	const struct compile_ref* scrutinee_ref,
	struct compile_ref* p_ref
) {
	return compile_ast_match_from_value_with_branch_compiler(
		ctx,
		ast_id,
		node,
		scrutinee_ref,
		compile_match_branch_body_default,
		NULL,
		p_ref
	);
}

static int type_expr_contains_self(
	const struct prototype_type_declaration_db* db,
	uint32_t expr_id
) {
	if (!db || expr_id >= db->expr_count) {
		return 0;
	}
	const struct prototype_type_expr* expr = &db->exprs[expr_id];
	switch (expr->tag) {
		case PROTOTYPE_TYPE_EXPR_SELF:
			return 1;
		case PROTOTYPE_TYPE_EXPR_APP:
			return type_expr_contains_self(db, expr->as.app.function) ||
				type_expr_contains_self(db, expr->as.app.argument);
		case PROTOTYPE_TYPE_EXPR_ARROW:
			return type_expr_contains_self(db, expr->as.arrow.domain) ||
				type_expr_contains_self(db, expr->as.arrow.codomain);
		case PROTOTYPE_TYPE_EXPR_PI:
			return type_expr_contains_self(db, expr->as.pi.domain) ||
				type_expr_contains_self(db, expr->as.pi.codomain);
		default:
			return 0;
	}
}

static int ast_type_expr_contains_name(
	const struct prototype_ast_db* asts,
	uint32_t expr_id,
	int name_symbol_id
) {
	if (!asts || expr_id >= asts->type_expr_count) {
		return 0;
	}
	const struct prototype_ast_type_expr* expr = &asts->type_exprs[expr_id];
	switch (expr->tag) {
		case PROTOTYPE_AST_TYPE_EXPR_NAME:
			return expr->as.name.symbol_id == name_symbol_id;
		case PROTOTYPE_AST_TYPE_EXPR_NAME_IN_NAMESPACE:
			return expr->as.name_in_namespace.namespace_symbol_id == name_symbol_id;
		case PROTOTYPE_AST_TYPE_EXPR_APP:
			return ast_type_expr_contains_name(
				asts, expr->as.app.function, name_symbol_id
			) || ast_type_expr_contains_name(
				asts, expr->as.app.argument, name_symbol_id
			);
		case PROTOTYPE_AST_TYPE_EXPR_ARROW:
			return ast_type_expr_contains_name(
				asts, expr->as.arrow.domain, name_symbol_id
			) || ast_type_expr_contains_name(
				asts, expr->as.arrow.codomain, name_symbol_id
			);
		case PROTOTYPE_AST_TYPE_EXPR_PI:
			return ast_type_expr_contains_name(
				asts, expr->as.pi.domain, name_symbol_id
			) || ast_type_expr_contains_name(
				asts, expr->as.pi.codomain, name_symbol_id
			);
		case PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE:
			return ast_type_expr_contains_name(
				asts, expr->as.computation_reference.result, name_symbol_id
			);
		default:
			return 0;
	}
}

static int type_expr_is_direct_self(
	const struct prototype_type_declaration_db* db,
	uint32_t expr_id
) {
	return db && expr_id < db->expr_count &&
		db->exprs[expr_id].tag == PROTOTYPE_TYPE_EXPR_SELF;
}

static int constructor_field_is_valid_inductive_field(
	const struct prototype_type_declaration_db* db,
	uint32_t expr_id
) {
	if (type_expr_is_direct_self(db, expr_id)) {
		return 1;
	}
	return !type_expr_contains_self(db, expr_id);
}

enum constructor_recursive_shape_kind {
	CONSTRUCTOR_RECURSIVE_SHAPE_NONE = 0,
	CONSTRUCTOR_RECURSIVE_SHAPE_DIRECT,
	CONSTRUCTOR_RECURSIVE_SHAPE_PI_CODOMAIN,
	CONSTRUCTOR_RECURSIVE_SHAPE_NEGATIVE,
	CONSTRUCTOR_RECURSIVE_SHAPE_UNSUPPORTED
};

static int constructor_recursive_shape_merge(int left, int right) {
	if (left == CONSTRUCTOR_RECURSIVE_SHAPE_NEGATIVE ||
		right == CONSTRUCTOR_RECURSIVE_SHAPE_NEGATIVE) {
		return CONSTRUCTOR_RECURSIVE_SHAPE_NEGATIVE;
	}
	if (left == CONSTRUCTOR_RECURSIVE_SHAPE_UNSUPPORTED ||
		right == CONSTRUCTOR_RECURSIVE_SHAPE_UNSUPPORTED) {
		return CONSTRUCTOR_RECURSIVE_SHAPE_UNSUPPORTED;
	}
	if (left == CONSTRUCTOR_RECURSIVE_SHAPE_PI_CODOMAIN ||
		right == CONSTRUCTOR_RECURSIVE_SHAPE_PI_CODOMAIN) {
		return CONSTRUCTOR_RECURSIVE_SHAPE_PI_CODOMAIN;
	}
	if (left == CONSTRUCTOR_RECURSIVE_SHAPE_DIRECT ||
		right == CONSTRUCTOR_RECURSIVE_SHAPE_DIRECT) {
		return CONSTRUCTOR_RECURSIVE_SHAPE_DIRECT;
	}
	return CONSTRUCTOR_RECURSIVE_SHAPE_NONE;
}

static int constructor_recursive_shape(
	const struct prototype_term_db* terms,
	const struct prototype_type_declaration_db* type_declarations,
	uint32_t term_id,
	uint32_t self_type_id,
	uint32_t depth
) {
	if (!terms || !type_declarations || term_id >= terms->term_count || depth > 128) {
		return CONSTRUCTOR_RECURSIVE_SHAPE_UNSUPPORTED;
	}
	uint32_t candidate_type_id;
	uint32_t arguments[32];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			terms, term_id, &candidate_type_id, arguments, &argument_count
		) == 0 && candidate_type_id == self_type_id) {
		return CONSTRUCTOR_RECURSIVE_SHAPE_DIRECT;
	}
	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_PI: {
			int domain = constructor_recursive_shape(
				terms, type_declarations, term->as.pi.domain, self_type_id, depth + 1
			);
			if (domain != CONSTRUCTOR_RECURSIVE_SHAPE_NONE) {
				return CONSTRUCTOR_RECURSIVE_SHAPE_NEGATIVE;
			}
			int codomain = constructor_recursive_shape(
				terms, type_declarations, term->as.pi.codomain_family,
				self_type_id, depth + 1
			);
			return codomain == CONSTRUCTOR_RECURSIVE_SHAPE_DIRECT ||
				codomain == CONSTRUCTOR_RECURSIVE_SHAPE_PI_CODOMAIN ?
				CONSTRUCTOR_RECURSIVE_SHAPE_PI_CODOMAIN : codomain;
		}
		case PROTOTYPE_TERM_LAMBDA:
			return constructor_recursive_shape(
				terms, type_declarations, term->as.lambda.body,
				self_type_id, depth + 1
			);
		case PROTOTYPE_TERM_COMPUTATION_TYPE:
			return constructor_recursive_shape(
				terms, type_declarations, term->as.computation_type.result,
				self_type_id, depth + 1
			);
		case PROTOTYPE_TERM_THUNK_TYPE:
			return constructor_recursive_shape(
				terms, type_declarations, term->as.thunk_type.computation,
				self_type_id, depth + 1
			);
		case PROTOTYPE_TERM_THUNK:
			return constructor_recursive_shape(
				terms, type_declarations, term->as.thunk.computation,
				self_type_id, depth + 1
			);
		case PROTOTYPE_TERM_RETURN:
			return constructor_recursive_shape(
				terms, type_declarations, term->as.return_term.value,
				self_type_id, depth + 1
			);
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL:
			return constructor_recursive_shape(
				terms, type_declarations, term->as.effect_row_forall.body,
				self_type_id, depth + 1
			);
		case PROTOTYPE_TERM_APP:
			return constructor_recursive_shape_merge(
				constructor_recursive_shape(
					terms, type_declarations, term->as.app.function,
					self_type_id, depth + 1
				),
				constructor_recursive_shape(
					terms, type_declarations, term->as.app.argument,
					self_type_id, depth + 1
				)
			);
		default:
			return CONSTRUCTOR_RECURSIVE_SHAPE_NONE;
	}
}

static int compile_type_formation_classifier_family(
	struct compile_context* ctx,
	const struct prototype_ast_type_def* ast_type,
	uint32_t* p_classifier
) {
	if (!ctx || !ast_type || !p_classifier) {
		return -1;
	}
	uint32_t classifier;
	if (prototype_term_universe_var(
			ctx->terms,
			ctx->type_declarations->next_level_var++,
			&classifier
		) != 0) {
		return -1;
	}
	uint32_t family_binder_count = ast_type->parameter_count + ast_type->index_count;
	for (uint32_t i = family_binder_count; i > 0; --i) {
		const struct prototype_ast_family_binder* parameter =
			&ctx->asts->family_binders[ast_type->first_family_binder + i - 1];
		uint32_t domain;
		uint32_t binding_id;
		uint32_t codomain_family;
		if (compile_ast_type_expr_term(ctx, parameter->type_expr, &domain) != 0 ||
			lookup_graph_binder(ctx, parameter->ast_binder_id, &binding_id) != 0 ||
			prototype_term_pure_family(
				ctx->terms, binding_id, classifier, &codomain_family
			) != 0 ||
			prototype_term_pi_family(
				ctx->terms, domain, codomain_family, &classifier
			) != 0) {
			return -1;
		}
	}
	*p_classifier = classifier;
	return 0;
}

static int compile_ast_type_def(
	struct compile_context* ctx,
	uint32_t ast_type_def_id,
	uint32_t* p_type_id
) {
	if (!ctx || !p_type_id || ast_type_def_id >= ctx->asts->type_def_count) {
		return -1;
	}

	struct prototype_ast_type_def* ast_type = &ctx->asts->type_defs[ast_type_def_id];
	if (ast_type->compiled) {
		*p_type_id = ast_type->compiled_type;
		return 0;
	}
	if (ast_type->compiling) {
		return -1;
	}

	uint32_t type_id;
	if (prototype_type_declaration_add(ctx->type_declarations, ast_type->name_symbol_id, &type_id) != 0) {
		return -1;
	}
	ctx->type_declarations->type_declarations[type_id].namespace_symbol_id =
		ctx->namespace_symbol_id;
	uint32_t type_term;
	uint32_t family_binder_count = ast_type->parameter_count + ast_type->index_count;
	if (family_binder_count > ctx->binder_count) {
		ast_type->compiling = 0;
		return -1;
	}
	uint32_t family_base_binder_count = ctx->binder_count - family_binder_count;

	ast_type->compiling = 1;
	ast_type->compiled_type = type_id;
	for (uint32_t i = 0; i < ast_type->parameter_count; ++i) {
		const struct prototype_ast_family_binder* parameter =
			&ctx->asts->family_binders[ast_type->first_family_binder + i];
		uint32_t compiled_type_expr;
		uint32_t graph_binder_id;
		if (lookup_graph_binder(ctx, parameter->ast_binder_id, &graph_binder_id) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		if (compile_ast_type_expr(ctx, parameter->type_expr, &compiled_type_expr) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		if (prototype_type_declaration_add_parameter(
			ctx->type_declarations,
			type_id,
			graph_binder_id,
			parameter->name_symbol_id,
			compiled_type_expr
		) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
	}
	ctx->type_declarations->type_declarations[type_id].parameter_context =
		ctx->context_ids[family_base_binder_count + ast_type->parameter_count];
	ctx->type_declarations->type_declarations[type_id].index_context =
		ctx->context_ids[family_base_binder_count + family_binder_count];
	ctx->type_declarations->type_declarations[type_id].index_count =
		ast_type->index_count;

	uint32_t type_args[16];
	if (ast_type->parameter_count > 16) {
		ast_type->compiling = 0;
		return -1;
	}
	for (uint32_t i = 0; i < ast_type->parameter_count; ++i) {
		const struct prototype_ast_family_binder* parameter =
			&ctx->asts->family_binders[ast_type->first_family_binder + i];
		uint32_t graph_binder_id;
		if (lookup_graph_binder(ctx, parameter->ast_binder_id, &graph_binder_id) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		if (prototype_term_var(
			ctx->terms,
			graph_binder_id,
			&type_args[i]
		) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
	}
	if (prototype_term_type_instance_make(
		ctx->terms,
		ctx->type_declarations,
		type_id,
		type_args,
		ast_type->parameter_count,
		&type_term
	) != 0) {
		ast_type->compiling = 0;
		return -1;
	}
	if (compile_type_formation_classifier_family(
			ctx,
			ast_type,
			&ctx->type_declarations->type_declarations[type_id].formation_classifier
		) != 0) {
		ast_type->compiling = 0;
		return -1;
	}
	ctx->binder_count = family_base_binder_count + ast_type->parameter_count;

	int has_direct_recursive_constructor_field = 0;
	int has_structural_seed_constructor = 0;
	for (uint32_t i = 0; i < ast_type->constructor_count; ++i) {
		const struct prototype_ast_type_constructor* constructor =
			&ctx->asts->type_constructors[ast_type->first_constructor + i];
		uint32_t compiled_result_type;
		uint32_t compiled_result_term;
		uint32_t compiled_field_types[64];
		uint32_t compiled_field_terms[64];
		uint32_t constructor_id;
		uint32_t curried_classifier_cache;
		uint32_t previous_binder_count = ctx->binder_count;
		uint32_t parameter_context =
			ctx->context_ids[previous_binder_count];
		int constructor_has_recursive_field = 0;
		if (constructor->field_count > 64) {
			ast_type->compiling = 0;
			return -1;
		}
		for (uint32_t j = 0; j < constructor->field_count; ++j) {
			uint32_t field_id = constructor->first_field_type + j;
			uint32_t field_type = ctx->asts->type_field_exprs[field_id];
			uint32_t ast_field_binder_id = ctx->asts->type_field_binder_ids ?
				ctx->asts->type_field_binder_ids[field_id] :
				PROTOTYPE_INVALID_ID;
			int field_symbol_id = ctx->asts->type_field_name_symbol_ids ?
				ctx->asts->type_field_name_symbol_ids[field_id] :
				-1;
			if (compile_ast_type_expr(ctx, field_type, &compiled_field_types[j]) != 0) {
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
			if (compile_ast_type_expr_term_with_self(
					ctx,
					field_type,
					type_term,
					&compiled_field_terms[j]
				) != 0) {
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
			uint32_t recursive_type_id;
			uint32_t recursive_args[16];
			uint32_t recursive_arg_count;
			int direct_recursive = prototype_term_type_instance_info(
				ctx->terms,
				compiled_field_terms[j],
				&recursive_type_id,
				recursive_args,
				&recursive_arg_count
			) == 0 && recursive_type_id == type_id &&
				recursive_arg_count == ast_type->parameter_count + ast_type->index_count;
			int recursive_shape = constructor_recursive_shape(
				ctx->terms, ctx->type_declarations,
				compiled_field_terms[j], type_id, 0
			);
			/* Recursive occurrences inside a declaration must use SELF (`*`).
			 * Referring to the declaration by name would re-enter top-level name
			 * resolution and would not denote the parameter-specialized family. */
			if (ast_type_expr_contains_name(
					ctx->asts, field_type, ast_type->name_symbol_id
				) || recursive_shape == CONSTRUCTOR_RECURSIVE_SHAPE_NEGATIVE ||
				recursive_shape == CONSTRUCTOR_RECURSIVE_SHAPE_UNSUPPORTED) {
				(void)add_compile_diagnostic(
					ctx,
					PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_GRAPH_CONSTRUCTION,
					PROTOTYPE_COMPILE_DIAGNOSTIC_UNSUPPORTED_NESTED_RECURSION,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					compiled_field_terms[j],
					ctx->asts->type_exprs[field_type].span
				);
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
			if (recursive_shape == CONSTRUCTOR_RECURSIVE_SHAPE_NONE &&
				!direct_recursive && !constructor_field_is_valid_inductive_field(
					ctx->type_declarations, compiled_field_types[j]
				)) {
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
			if (recursive_shape == CONSTRUCTOR_RECURSIVE_SHAPE_DIRECT ||
				recursive_shape == CONSTRUCTOR_RECURSIVE_SHAPE_PI_CODOMAIN ||
				direct_recursive ||
				type_expr_is_direct_self(ctx->type_declarations, compiled_field_types[j])) {
				constructor_has_recursive_field = 1;
			}
			if (recursive_shape == CONSTRUCTOR_RECURSIVE_SHAPE_DIRECT ||
				direct_recursive ||
				type_expr_is_direct_self(
					ctx->type_declarations, compiled_field_types[j]
				)) {
				has_direct_recursive_constructor_field = 1;
			}
			uint32_t graph_binder_id;
			if (push_graph_binder(
					ctx,
					ast_field_binder_id,
					compiled_field_terms[j],
					field_symbol_id,
					&graph_binder_id
				) != 0) {
				ctx->binder_count = previous_binder_count;
				ast_type->compiling = 0;
				return -1;
			}
		}
		if (!constructor_has_recursive_field) {
			has_structural_seed_constructor = 1;
		}
		uint32_t field_context = ctx->context_ids[ctx->binder_count];
		/* Both readback and semantic result classifiers may refer to constructor
		 * fields, so compile them only after the complete field telescope exists. */
		if (compile_ast_type_expr(
				ctx, constructor->result_type, &compiled_result_type
			) != 0 || compile_ast_type_expr_term_with_self(
				ctx,
				constructor->result_type,
				type_term,
				&compiled_result_term
			) != 0) {
			ctx->binder_count = previous_binder_count;
			ast_type->compiling = 0;
			return -1;
		}
		uint32_t result_type_id;
		uint32_t result_args[16];
		uint32_t result_arg_count;
		if (prototype_term_type_instance_info(
				ctx->terms,
				compiled_result_term,
				&result_type_id,
				result_args,
				&result_arg_count
			) != 0 || result_type_id != type_id ||
			result_arg_count != ast_type->parameter_count + ast_type->index_count) {
			ctx->binder_count = previous_binder_count;
			ast_type->compiling = 0;
			return -1;
		}
		for (uint32_t j = 0; j < ast_type->parameter_count; ++j) {
			if (result_args[j] != type_args[j]) {
				struct prototype_term_conversion_result conversion =
					prototype_judgement_classifier_conversion(
						ctx->terms,
						ctx->type_declarations,
						result_args[j],
						type_args[j]
					);
				if (conversion.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					ctx->binder_count = previous_binder_count;
					ast_type->compiling = 0;
					return -1;
				}
			}
		}
		if (prototype_type_constructor_derive_curried_classifier(
				ctx->terms,
				&ctx->metadata->contexts,
				parameter_context,
				field_context,
				compiled_result_term,
				&curried_classifier_cache
			) != 0) {
			ctx->binder_count = previous_binder_count;
			ast_type->compiling = 0;
			return -1;
		}
		ctx->binder_count = previous_binder_count;
		if (prototype_type_declaration_add_constructor(
				ctx->type_declarations,
				type_id,
				constructor->name_symbol_id,
				compiled_field_types,
				constructor->field_count,
				compiled_result_type,
				parameter_context,
				field_context,
				compiled_result_term,
				curried_classifier_cache,
				&constructor_id
			) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
		(void)constructor_id;
	}
	if (has_direct_recursive_constructor_field &&
		!has_structural_seed_constructor) {
		ast_type->compiling = 0;
		return -1;
	}
	if (add_compile_type_export(ctx, type_id) != 0) {
		ast_type->compiling = 0;
		return -1;
	}

	ast_type->compiled = 1;
	ast_type->compiling = 0;
	*p_type_id = type_id;
	return 0;
}

static int reduce_type_namespace_term(
	struct compile_context* ctx,
	uint32_t namespace_term,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || namespace_term >= ctx->terms->term_count) {
		return -1;
	}

	return prototype_term_normalize_complete_with_profile(
		ctx->terms,
		ctx->type_declarations,
		NULL,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		(int)namespace_term,
		p_ret
	);
}

static int external_type_namespace_name(
	const struct prototype_term_db* terms,
	uint32_t namespace_term,
	struct prototype_qualified_name* p_name
) {
	if (!terms || !p_name || namespace_term >= terms->term_count) {
		return -1;
	}
	uint32_t current = namespace_term;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		current = terms->terms[current].as.app.function;
	}
	if (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_EXTERNAL_REF) {
		*p_name = terms->terms[current].as.external_ref.name;
		return 0;
	}
	return -1;
}

static int imported_owner_arguments(
	const struct prototype_term_db* terms,
	uint32_t owner,
	struct prototype_qualified_name type_name,
	uint32_t* args,
	uint32_t* p_arg_count
);

static int resolve_imported_namespace_member(
	struct compile_context* ctx,
	uint32_t namespace_term,
	int member_symbol_id,
	uint32_t* p_ret
) {
	if (!ctx || !p_ret || namespace_term >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_qualified_name namespace_name;
	if (external_type_namespace_name(
		ctx->terms,
		namespace_term,
		&namespace_name
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* interface =
			ctx->imported_interfaces[i];
		uint32_t type_export_id;
		uint32_t constructor_export_id;
		if (!interface) {
			continue;
		}
		int found_type = prototype_artifact_interface_find_type_export_in_namespace(
			interface,
			namespace_name.namespace_symbol_id,
			namespace_name.name_symbol_id,
			&type_export_id
		);
		if (found_type < 0) {
			return -1;
		}
		if (found_type > 0) {
			continue;
		}
		int found_constructor = prototype_artifact_interface_find_constructor_export(
			interface,
			type_export_id,
			member_symbol_id,
			&constructor_export_id
		);
		if (found_constructor < 0) {
			return -1;
		}
		if (found_constructor > 0) {
			continue;
		}
		uint32_t constructor_term;
		uint32_t owner_arguments[16];
		uint32_t owner_argument_count;
		uint32_t owner;
		const struct prototype_artifact_type_export* type_export =
			&interface->type_exports[type_export_id];
		const struct prototype_artifact_constructor_export* constructor_export =
			&interface->constructor_exports[constructor_export_id];
		if (imported_owner_arguments(
				ctx->terms,
				namespace_term,
				namespace_name,
				owner_arguments,
				&owner_argument_count
			) != 0 || owner_argument_count != type_export->parameter_count ||
			prototype_term_type_instance_make(
				ctx->terms,
				ctx->type_declarations,
				type_export->local_type_id,
				owner_arguments,
				owner_argument_count,
				&owner
			) != 0) {
			return -1;
		}
		if (prototype_term_constructor(
			ctx->terms,
			owner,
			constructor_export->ordinal,
			&constructor_term
		) != 0) {
			return -1;
		}
		if (queue_imported_constructor_classifier(
				ctx,
				constructor_term,
				owner,
				interface,
				type_export_id,
				constructor_export_id
			) != 0) {
			return -1;
		}
		*p_ret = constructor_term;
		return 0;
	}
	return -1;
}

static int resolve_namespace_member(
	struct compile_context* ctx,
	uint32_t namespace_term,
	int member_symbol_id,
	uint32_t* p_ret,
	uint32_t* p_classifier
) {
	if (!ctx || !p_ret || namespace_term >= ctx->terms->term_count) {
		return -1;
	}
	if (p_classifier) {
		*p_classifier = PROTOTYPE_INVALID_ID;
	}

	uint32_t evaluated_namespace;
	if (reduce_type_namespace_term(ctx, namespace_term, &evaluated_namespace) != 0) {
		return -1;
	}
	if (evaluated_namespace >= ctx->terms->term_count) {
		return -1;
	}

	uint32_t type_id;
	uint32_t ignored_namespace_args[16];
	uint32_t ignored_namespace_arg_count;
	if (prototype_term_type_instance_info(
		ctx->terms,
		evaluated_namespace,
		&type_id,
		ignored_namespace_args,
		&ignored_namespace_arg_count
	) != 0) {
		int status = resolve_imported_namespace_member(
			ctx,
			evaluated_namespace,
			member_symbol_id,
			p_ret
		);
		return status;
	}

	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(ctx->type_declarations, type_id, member_symbol_id);
	if (!constructor) {
		return -1;
	}
	uint32_t classifier = constructor->curried_classifier_cache;
	for (uint32_t i = 0; i < ignored_namespace_arg_count; ++i) {
		uint32_t applied;
		if (prototype_term_app(ctx->terms, classifier, ignored_namespace_args[i], &applied) != 0) {
			return -1;
		}
		classifier = applied;
	}
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier
		) != 0) {
		return -1;
	}
	if (prototype_term_constructor(
			ctx->terms,
			namespace_term,
			constructor->constructor_index,
			p_ret
		) != 0) {
		return -1;
	}
	if (queue_declaration_fact(ctx, *p_ret, classifier) != 0) {
		return -1;
	}
	if (p_classifier) {
		*p_classifier = classifier;
	}
	return 0;
}

static int imported_owner_arguments(
	const struct prototype_term_db* terms,
	uint32_t owner,
	struct prototype_qualified_name type_name,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	if (!terms || !args || !p_arg_count || owner >= terms->term_count) {
		return -1;
	}
	uint32_t reversed_args[16];
	uint32_t arg_count = 0;
	uint32_t current = owner;
	while (current < terms->term_count &&
		terms->terms[current].tag == PROTOTYPE_TERM_APP) {
		if (arg_count >= 16) {
			return -1;
		}
		reversed_args[arg_count++] = terms->terms[current].as.app.argument;
		current = terms->terms[current].as.app.function;
	}
	if (current >= terms->term_count ||
		terms->terms[current].tag != PROTOTYPE_TERM_EXTERNAL_REF ||
		!qualified_names_equal(terms->terms[current].as.external_ref.name, type_name)) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		args[i] = reversed_args[arg_count - i - 1];
	}
	*p_arg_count = arg_count;
	return 0;
}

static int imported_owner_type_arguments(
	struct prototype_term_db* terms,
	const struct prototype_artifact_type_export* type_export,
	uint32_t owner,
	uint32_t* args,
	uint32_t* p_arg_count
) {
	if (!terms || !type_export || !args || !p_arg_count ||
		owner >= terms->term_count) {
		return -1;
	}
	uint32_t type_id;
	uint32_t local_args[16];
	uint32_t local_arg_count;
	if (prototype_term_type_instance_info(
			terms,
			owner,
			&type_id,
			local_args,
			&local_arg_count
		) == 0) {
		if (type_id != type_export->local_type_id ||
			local_arg_count != type_export->parameter_count) {
			return -1;
		}
		for (uint32_t i = 0; i < local_arg_count; ++i) {
			args[i] = local_args[i];
		}
		*p_arg_count = local_arg_count;
		return 0;
	}
	if (imported_owner_arguments(
			terms,
			owner,
			qualified_name_make(
				type_export->namespace_symbol_id,
				type_export->name_symbol_id
			),
			args,
			p_arg_count
		) != 0 ||
		*p_arg_count != type_export->parameter_count) {
		return -1;
	}
	return 0;
}

static const struct prototype_artifact_type_export* imported_type_export_by_local_type(
	const struct prototype_artifact_interface* interface,
	uint32_t type_id
) {
	if (!interface) {
		return NULL;
	}
	for (size_t i = 0; i < interface->type_export_count; ++i) {
		const struct prototype_artifact_type_export* export =
			&interface->type_exports[i];
		if (export->local_type_id == type_id) {
			return export;
		}
	}
	return NULL;
}

static int external_type_spine_from_imported_instance(
	struct prototype_term_db* terms,
	const struct prototype_artifact_type_export* export,
	const uint32_t* args,
	uint32_t arg_count,
	uint32_t* p_ret
) {
	if (!terms || !export || !p_ret || (arg_count > 0 && !args)) {
		return -1;
	}
	uint32_t current;
	if (prototype_term_external_ref(
			terms,
				qualified_name_make(export->namespace_symbol_id, export->name_symbol_id),
			&current
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t app;
		if (prototype_term_app(terms, current, args[i], &app) != 0) {
			return -1;
		}
		current = app;
	}
	*p_ret = current;
	return 0;
}

static int __attribute__((unused)) rewrite_imported_type_instances_to_external(
	struct prototype_term_db* terms,
	const struct prototype_artifact_interface* interface,
	uint32_t term_id,
	uint32_t* p_ret,
	uint32_t depth
) {
	if (!terms || !interface || !p_ret || term_id >= terms->term_count ||
		depth > 256) {
		return -1;
	}

	uint32_t type_id;
	uint32_t instance_args[16];
	uint32_t instance_arg_count;
	if (prototype_term_type_instance_info(
			terms,
			term_id,
			&type_id,
			instance_args,
			&instance_arg_count
		) == 0) {
		const struct prototype_artifact_type_export* export =
			imported_type_export_by_local_type(interface, type_id);
		if (export) {
			uint32_t rewritten_args[16];
			for (uint32_t i = 0; i < instance_arg_count; ++i) {
				if (rewrite_imported_type_instances_to_external(
						terms,
						interface,
						instance_args[i],
						&rewritten_args[i],
						depth + 1
					) != 0) {
					return -1;
				}
			}
			return external_type_spine_from_imported_instance(
				terms,
				export,
				rewritten_args,
				instance_arg_count,
				p_ret
			);
		}
	}

	const struct prototype_term* term = &terms->terms[term_id];
	switch (term->tag) {
		case PROTOTYPE_TERM_APP: {
			uint32_t function;
			uint32_t argument;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.app.function,
					&function,
					depth + 1
				) != 0 ||
				rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.app.argument,
					&argument,
					depth + 1
				) != 0) {
				return -1;
			}
			if (function == term->as.app.function &&
				argument == term->as.app.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_app(terms, function, argument, p_ret);
		}
		case PROTOTYPE_TERM_LAMBDA: {
			uint32_t body;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.lambda.body,
					&body,
					depth + 1
				) != 0) {
				return -1;
			}
			if (body == term->as.lambda.body) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_lambda(
				terms,
				term->as.lambda.binding_id,
				body,
				p_ret
			);
		}
		case PROTOTYPE_TERM_PI: {
			uint32_t domain;
			uint32_t codomain_family;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.pi.domain,
					&domain,
					depth + 1
				) != 0 ||
				rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.pi.codomain_family,
					&codomain_family,
					depth + 1
				) != 0) {
				return -1;
			}
			if (domain == term->as.pi.domain &&
				codomain_family == term->as.pi.codomain_family) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_pi_family(
				terms,
				domain,
				codomain_family,
				p_ret
			);
		}
		case PROTOTYPE_TERM_CONSTRUCTOR: {
			uint32_t owner;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.constructor.owner,
					&owner,
					depth + 1
				) != 0) {
				return -1;
			}
			if (owner == term->as.constructor.owner) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_constructor(
				terms,
				owner,
				term->as.constructor.constructor_id,
				p_ret
			);
		}
		case PROTOTYPE_TERM_MATCH: {
			uint32_t scrutinee;
			struct prototype_match_case_input case_inputs[64];
			struct prototype_case_binder binder_storage[256];
			uint32_t binder_cursor = 0;
			int changed = 0;
			if (term->as.match.case_count > 64 ||
				rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.match.scrutinee,
					&scrutinee,
					depth + 1
				) != 0) {
				return -1;
			}
			changed = scrutinee != term->as.match.scrutinee;
			for (uint32_t i = 0; i < term->as.match.case_count; ++i) {
				const struct prototype_match_case* old_case =
					&terms->cases[term->as.match.first_case + i];
				uint32_t body;
				uint32_t constructor_owner = old_case->constructor_owner;
				if (binder_cursor + old_case->binder_count > 256 ||
					rewrite_imported_type_instances_to_external(
						terms,
						interface,
						old_case->body,
						&body,
						depth + 1
					) != 0) {
					return -1;
				}
				if (constructor_owner != PROTOTYPE_INVALID_ID &&
					rewrite_imported_type_instances_to_external(
						terms,
						interface,
						constructor_owner,
						&constructor_owner,
						depth + 1
					) != 0) {
					return -1;
				}
				for (uint32_t j = 0; j < old_case->binder_count; ++j) {
					binder_storage[binder_cursor + j] =
						terms->case_binders[old_case->first_binder + j];
				}
				if (body != old_case->body ||
					constructor_owner != old_case->constructor_owner) {
					changed = 1;
				}
				case_inputs[i].case_label_symbol_id =
					terms->case_label_symbols[term->as.match.first_case + i];
				case_inputs[i].constructor_owner = constructor_owner;
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
				terms,
				scrutinee,
				case_inputs,
				term->as.match.case_count,
				term->as.match.ih_scope_id,
				p_ret
			);
		}
		case PROTOTYPE_TERM_INDUCTION_HYPOTHESIS: {
			uint32_t argument;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.induction_hypothesis.argument,
					&argument,
					depth + 1
				) != 0) {
				return -1;
			}
			if (argument == term->as.induction_hypothesis.argument) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_induction_hypothesis(
				terms,
				term->as.induction_hypothesis.ih_scope_id,
				argument,
				p_ret
			);
		}
		case PROTOTYPE_TERM_RETURN: {
			uint32_t value;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.return_term.value, &value, depth + 1
				) != 0) {
				return -1;
			}
			if (value == term->as.return_term.value) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_return(terms, value, p_ret);
		}
		case PROTOTYPE_TERM_THUNK: {
			uint32_t computation;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.thunk.computation, &computation, depth + 1
				) != 0) {
				return -1;
			}
			if (computation == term->as.thunk.computation) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_thunk(terms, computation, p_ret);
		}
		case PROTOTYPE_TERM_FORCE: {
			uint32_t value;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.force.value, &value, depth + 1
				) != 0) {
				return -1;
			}
			if (value == term->as.force.value) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_force(terms, value, p_ret);
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
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.computation_fold.computation, &computation, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.computation_fold.return_clause, &return_clause, depth + 1
				) != 0) {
				free(clauses);
				return -1;
			}
			int unchanged = computation == term->as.computation_fold.computation &&
				return_clause == term->as.computation_fold.return_clause;
			for (uint32_t i = 0; i < clause_count; ++i) {
				const struct prototype_computation_fold_clause* clause =
					&terms->computation_fold_clauses[term->as.computation_fold.first_clause + i];
				if (rewrite_imported_type_instances_to_external(
						terms, interface, clause->operation, &clauses[i].operation, depth + 1
					) != 0 || rewrite_imported_type_instances_to_external(
						terms, interface, clause->body, &clauses[i].body, depth + 1
					) != 0) {
					free(clauses);
					return -1;
				}
				unchanged = unchanged && clauses[i].operation == clause->operation &&
					clauses[i].body == clause->body;
			}
			int status = unchanged ? (*p_ret = term_id, 0) : prototype_term_computation_fold(
				terms, computation, return_clause, clauses, clause_count, p_ret
			);
			free(clauses);
			return status;
		}
		case PROTOTYPE_TERM_OPERATION_REQUEST: {
			uint32_t operation;
			uint32_t argument;
			uint32_t continuation;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.operation_request.operation, &operation, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.operation_request.argument, &argument, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.operation_request.continuation, &continuation, depth + 1
				) != 0) {
				return -1;
			}
			return operation == term->as.operation_request.operation &&
				argument == term->as.operation_request.argument &&
				continuation == term->as.operation_request.continuation ?
				(*p_ret = term_id, 0) : prototype_term_operation_request(
					terms, operation, argument, continuation, p_ret
				);
		}
		case PROTOTYPE_TERM_COMPUTATION_TYPE: {
			uint32_t label;
			uint32_t result;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.computation_type.label, &label, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.computation_type.result, &result, depth + 1
				) != 0) {
				return -1;
			}
			if (label == term->as.computation_type.label &&
				result == term->as.computation_type.result) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_computation_type(terms, label, result, p_ret);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_UNION: {
			uint32_t left;
			uint32_t right;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.effect_row_union.left, &left, depth + 1
				) != 0 || rewrite_imported_type_instances_to_external(
					terms, interface, term->as.effect_row_union.right, &right, depth + 1
				) != 0) {
				return -1;
			}
			return left == term->as.effect_row_union.left && right == term->as.effect_row_union.right ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_union(terms, left, right, p_ret);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_FORALL: {
			uint32_t body;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.effect_row_forall.body, &body, depth + 1
				) != 0) {
				return -1;
			}
			return body == term->as.effect_row_forall.body ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_forall(
					terms, term->as.effect_row_forall.binding_id, body, p_ret
				);
		}
		case PROTOTYPE_TERM_EFFECT_ROW_OPERATION: {
			uint32_t latent_row;
			if (rewrite_imported_type_instances_to_external(
					terms,
					interface,
					term->as.effect_row_operation.latent_row,
					&latent_row,
					depth + 1
				) != 0) {
				return -1;
			}
			return latent_row == term->as.effect_row_operation.latent_row ?
				(*p_ret = term_id, 0) : prototype_term_effect_row_operation(
					terms, term->as.effect_row_operation.operation_id, latent_row, p_ret
				);
		}
		case PROTOTYPE_TERM_THUNK_TYPE: {
			uint32_t computation;
			if (rewrite_imported_type_instances_to_external(
					terms, interface, term->as.thunk_type.computation, &computation, depth + 1
				) != 0) {
				return -1;
			}
			if (computation == term->as.thunk_type.computation) {
				*p_ret = term_id;
				return 0;
			}
			return prototype_term_thunk_type(terms, computation, p_ret);
		}
		default:
			*p_ret = term_id;
			return 0;
	}
}

static int imported_constructor_classifier_from_curried_cache(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_artifact_interface* interface,
	const struct prototype_artifact_type_export* type_export,
	const struct prototype_artifact_constructor_export* constructor_export,
	uint32_t owner,
	uint32_t* p_classifier
) {
	if (!terms || !type_declarations || !type_export || !constructor_export ||
		!p_classifier || constructor_export->curried_classifier_cache == PROTOTYPE_INVALID_ID ||
		constructor_export->curried_classifier_cache >= terms->term_count) {
		return -1;
	}
	uint32_t args[16];
	uint32_t arg_count;
	if (imported_owner_type_arguments(
			terms,
			type_export,
			owner,
			args,
			&arg_count
		) != 0) {
		return -1;
	}
	uint32_t classifier = constructor_export->curried_classifier_cache;
	for (uint32_t i = 0; i < arg_count; ++i) {
		uint32_t app;
		if (prototype_term_app(terms, classifier, args[i], &app) != 0) {
			return -1;
		}
		classifier = app;
	}
	if (prototype_term_normalize_complete_with_profile(
			terms,
			type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			classifier,
			&classifier
		) != 0) {
		return -1;
	}
	/* constructor_export->curried_classifier_cache was relocated with the provider
	 * graph. Keep that graph-level family intact: an imported term's Pi domain
	 * refers to the same TYPE_VIEW. Rewriting only this path to EXTERNAL_REF
	 * would make an imported constructor fail the ordinary App domain check. */
	(void)interface;
	*p_classifier = classifier;
	return 0;
}

static int compile_phase_infer_imported_constructor_classifiers(
	struct compile_context* ctx
) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_imported_constructor_classifier_count; ++i) {
		const struct pending_imported_constructor_classifier* pending =
			&ctx->pending_imported_constructor_classifiers[i];
		const struct prototype_artifact_interface* interface = pending->interface;
		const struct prototype_artifact_type_export* type_export =
			&interface->type_exports[pending->type_export_id];
		const struct prototype_artifact_constructor_export* constructor_export =
			&interface->constructor_exports[pending->constructor_export_id];
		uint32_t classifier;
		if (imported_constructor_classifier_from_curried_cache(
				ctx->terms,
				ctx->type_declarations,
				interface,
				type_export,
				constructor_export,
				pending->owner,
				&classifier
			) != 0) {
			return -1;
		}
		if (queue_declaration_fact(
				ctx,
				pending->constructor_term,
				classifier
			) != 0) {
			return -1;
		}
	}
	return 0;
}

/* A surface annotation names a value type unless it already denotes a raw
 * computation type such as Pi. At a computation occurrence, an omitted effect
 * row becomes a fresh elaboration variable. Exact purity is introduced only by
 * an explicit or synthesized RETURN. */
static int compile_expected_classifier_for_ref(
	struct compile_context* ctx,
	const struct compile_ref* ref,
	uint32_t surface_classifier,
	uint32_t* p_expected
) {
	if (!ctx || !ref || !p_expected || surface_classifier >= ctx->terms->term_count) {
		return -1;
	}
	*p_expected = surface_classifier;
	if (ref->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return 0;
	}
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, surface_classifier, &view
		) != 0) {
		return -1;
	}
	if (view.category == PROTOTYPE_TERM_CATEGORY_VALUE) {
		uint32_t row_binder = prototype_term_new_binding(ctx->terms);
		uint32_t effect_row;
		if (row_binder == PROTOTYPE_INVALID_ID ||
			prototype_term_effect_row_var(
				ctx->terms, row_binder, &effect_row
			) != 0 || prototype_term_computation_type(
				ctx->terms, effect_row, surface_classifier, p_expected
			) != 0) {
			return -1;
		}
	}
	return 0;
}

/* Surface type expectations select a CBPV category before graph lowering.
 * A Pi is a negative computation classifier; every other currently supported
 * surface classifier denotes a value. This is deliberately based on the
 * normalized classifier view, rather than on the spelling of an annotation. */
static int compile_ast_against_surface_classifier(
	struct compile_context* ctx,
	uint32_t ast_id,
	uint32_t surface_classifier,
	struct compile_ref* p_ret
) {
	struct prototype_term_classifier_view view;
	int status;
	if (!ctx || !p_ret || surface_classifier >= ctx->terms->term_count ||
		prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, surface_classifier, &view
		) != 0) {
		return -1;
	}
	if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return compile_ast_computation_ref(ctx, ast_id, p_ret);
	}
	status = compile_ast_value_ref(ctx, ast_id, p_ret);
	if (status == 0) {
		return 0;
	}
	if (status < 0) {
		return -1;
	}
	/* An expected surface value type also admits a computation that returns
	 * that value. compile_expected_classifier_for_ref later turns `A` into
	 * `Comp({}, A)` for this occurrence. */
	return compile_ast_computation_ref(ctx, ast_id, p_ret);
}

/* A primitive constructor is an n-ary value introduction, not a completed
 * first-class function. */
static int constructor_spine_saturation(
	const struct compile_context* ctx,
	uint32_t term_id
) {
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t arguments[64];
	uint32_t argument_count;
	if (!ctx) {
		return -1;
	}
	int spine_status = prototype_term_constructor_spine_info(
		ctx->terms,
		term_id,
		&head,
		&owner,
		&constructor_index,
		arguments,
		64,
		&argument_count
	);
	if (spine_status != 0) {
		return spine_status;
	}
	uint32_t type_id;
	uint32_t owner_arguments[64];
	uint32_t owner_argument_count;
	if (prototype_type_declaration_instance_info(
			ctx->type_declarations,
			ctx->terms,
			owner,
			&type_id,
			owner_arguments,
			64,
			&owner_argument_count
		) != 0 || type_id >= ctx->type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		owner_argument_count != type->parameter_count ||
		type->first_constructor + constructor_index >=
			ctx->type_declarations->constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&ctx->type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	const struct prototype_context* parameter_context = prototype_context_get(
		&ctx->metadata->contexts, constructor->parameter_context
	);
	const struct prototype_context* field_context = prototype_context_get(
		&ctx->metadata->contexts, constructor->field_context
	);
	if (!parameter_context || !field_context ||
		field_context->depth < parameter_context->depth) {
		return -1;
	}
	uint32_t declared_field_count =
		field_context->depth - parameter_context->depth;
	(void)head;
	return argument_count == declared_field_count ? 0 :
		(argument_count < declared_field_count ? 2 : -1);
}

/* Partial spine occurrences are builder steps. Each must feed the function
 * edge of the next constructor-formation APP; only a saturated occurrence may
 * escape into any other source operation. */
static int validate_constructor_occurrence_saturation(
	const struct compile_context* ctx
) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[i];
		int saturation = constructor_spine_saturation(ctx, operation->core_term);
		if (saturation < 0) {
			return -1;
		}
		if (saturation != 2) {
			continue;
		}
		for (uint32_t parent_id = 0;
			parent_id < ctx->metadata->operation_count;
			++parent_id) {
			const struct prototype_operation_node* parent =
				&ctx->metadata->operations[parent_id];
			if (parent->function == i &&
				(parent->tag != PROTOTYPE_OPERATION_APP ||
				 parent->application_role !=
					PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION)) {
				return -1;
			}
			if (parent->argument == i || parent->body == i ||
				parent->scrutinee == i) {
				return -1;
			}
			if (parent->tag == PROTOTYPE_OPERATION_MATCH) {
				if (parent->first_case + parent->case_count >
					ctx->metadata->operation_case_count) {
					return -1;
				}
				for (uint32_t case_index = 0;
					case_index < parent->case_count;
					++case_index) {
					if (ctx->metadata->operation_cases[
							parent->first_case + case_index
						].body_operation == i) {
						return -1;
					}
				}
			}
		}
		for (size_t label_id = 0;
			label_id < ctx->metadata->label_count;
			++label_id) {
			if (ctx->metadata->labels[label_id].exposed_operation == i ||
				ctx->metadata->labels[label_id].body_operation == i) {
				return -1;
			}
		}
		for (size_t assignment_id = 0;
			assignment_id < ctx->asts->assignment_count;
			++assignment_id) {
			const struct prototype_ast_term_assignment_def* assignment =
				&ctx->asts->assignments[assignment_id];
			if (assignment->compiled && assignment->compiled_operation == i) {
				return -1;
			}
		}
	}
	return 0;
}

static int compile_def(
	struct compile_context* ctx,
	struct prototype_ast_term_assignment_def* def,
	uint32_t* p_ret
) {
	if (!ctx || !def || !p_ret) {
		return -1;
	}
	if (def->compiled) {
		*p_ret = def->compiled_term;
		return 0;
	}
	if (def->compiling) {
		(void)add_resolve_error(
			ctx,
			PROTOTYPE_RESOLVE_ERROR_RECURSIVE,
			def->name_symbol_id,
			-1,
			def->ast
		);
		return -1;
	}

	def->compiling = 1;
	uint32_t previous_pending_match_resolution_count = ctx->pending_match_resolution_count;
	uint32_t previous_pending_match_typing_count = ctx->pending_match_typing_count;
	uint32_t previous_pending_ascription_check_count =
		ctx->pending_ascription_check_count;
	uint32_t previous_pending_imported_constructor_classifier_count =
		ctx->pending_imported_constructor_classifier_count;
	uint32_t previous_pending_binder_assumption_count =
	ctx->pending_binder_assumption_count;
	uint32_t previous_pending_declaration_fact_count =
		ctx->pending_declaration_fact_count;
	struct compile_ref ref;
	compile_ref_clear(&ref);
	uint32_t surface_expected_classifier = PROTOTYPE_INVALID_ID;
	int has_surface_expectation = lookup_source_expectation_classifier(
		ctx, def->name_symbol_id, &surface_expected_classifier
	) == 0;
	int lower_status;
	if (has_surface_expectation) {
		lower_status = compile_ast_against_surface_classifier(
			ctx, def->ast, surface_expected_classifier, &ref
		);
	} else if (def->ast < ctx->asts->node_count &&
		ctx->asts->nodes[def->ast].tag == PROTOTYPE_AST_LAMBDA) {
		/* Lambda is intrinsically a negative computation in raw CBPV. */
		lower_status = compile_ast_computation_ref(ctx, def->ast, &ref);
	} else {
		lower_status = compile_ast_ref(ctx, def->ast, &ref);
	}
	if (lower_status != 0) {
		ctx->pending_match_resolution_count = previous_pending_match_resolution_count;
		ctx->pending_match_typing_count = previous_pending_match_typing_count;
		ctx->pending_ascription_check_count = previous_pending_ascription_check_count;
		ctx->pending_imported_constructor_classifier_count =
			previous_pending_imported_constructor_classifier_count;
		ctx->pending_binder_assumption_count =
			previous_pending_binder_assumption_count;
		ctx->pending_declaration_fact_count =
			previous_pending_declaration_fact_count;
		def->compiling = 0;
		return -1;
	}
	if (has_surface_expectation) {
		uint32_t expected_classifier = surface_expected_classifier;
		if (compile_expected_classifier_for_ref(
				ctx, &ref, expected_classifier, &expected_classifier
			) != 0) {
			def->compiling = 0;
			return -1;
		}
		uint32_t ascription_operation;
		if (operation_add(
				ctx,
				PROTOTYPE_OPERATION_ASCRIPTION,
				ref.term,
				expected_classifier,
				def->ast,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				ref.operation,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				0,
				&ascription_operation
			) != 0) {
			def->compiling = 0;
			return -1;
		}
		ref.classifier = expected_classifier;
		ref.operation = ascription_operation;
	}
	uint32_t declared_classifier;
	if (lookup_external_declaration_classifier(
			ctx,
			def->name_symbol_id,
			&declared_classifier
		) == 0) {
		if (queue_declaration_fact(ctx, ref.term, declared_classifier) != 0) {
			def->compiling = 0;
			return -1;
		}
		ref.classifier = declared_classifier;
		if (ref.operation < ctx->metadata->operation_count) {
			ctx->metadata->operations[ref.operation].known_classifier = declared_classifier;
		}
	}
	if (def->definition_value_required &&
		ref.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		if (!ctx->metadata || ctx->metadata->definition_thunk_policy !=
			PROTOTYPE_DEFINITION_THUNK_IMPLICIT) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				def->name_symbol_id,
				-1,
				def->ast,
				def->body_span
			);
			def->compiling = 0;
			return -1;
		}
		struct compile_ref quoted;
		if (compile_ref_make_thunk(ctx, &ref, def->ast, &quoted) != 0) {
			def->compiling = 0;
			return -1;
		}
		ref = quoted;
	}
	if (ref.operation < ctx->metadata->operation_count) {
		ctx->metadata->operations[ref.operation].source_symbol_id = def->name_symbol_id;
		ctx->metadata->operations[ref.operation].category = ref.category;
		ctx->metadata->operations[ref.operation].computation_kind =
			ref.computation_kind;
	}
	def->compiled_term = ref.term;
	def->compiled_classifier = ref.classifier;
	def->compiled_operation = ref.operation;
	def->compiled = 1;
	def->compiling = 0;
	*p_ret = ref.term;
	return 0;
}

static int compile_def_ref(
	struct compile_context* ctx,
	struct prototype_ast_term_assignment_def* def,
	struct compile_ref* p_ret
) {
	if (!ctx || !def || !p_ret) {
		return -1;
	}
	uint32_t term;
	if (compile_def(ctx, def, &term) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = def->compiled_classifier;
	if (def->compiled_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	p_ret->category = ctx->metadata->operations[def->compiled_operation].category;
	p_ret->computation_kind =
		ctx->metadata->operations[def->compiled_operation].computation_kind;
	if (p_ret->classifier == PROTOTYPE_INVALID_ID) {
		(void)lookup_external_declaration_classifier(
			ctx,
			def->name_symbol_id,
			&p_ret->classifier
		);
	}
	p_ret->operation = PROTOTYPE_INVALID_ID;
	if (operation_add(
		ctx,
		PROTOTYPE_OPERATION_NAME,
		term,
		p_ret->classifier,
		def->ast,
		def->compiled_operation,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID,
		0,
		&p_ret->operation
		) != 0) {
		return -1;
	}
	ctx->metadata->operations[p_ret->operation].source_symbol_id = def->name_symbol_id;
	ctx->metadata->operations[p_ret->operation].category = p_ret->category;
	ctx->metadata->operations[p_ret->operation].computation_kind =
		p_ret->computation_kind;
	return 0;
}


/* This lowerer is intentionally limited to atomic graph references.  Every
 * source computation form is elaborated through the category-aware CBPV
 * lowering below; keeping a second general lowering here would let APP and
 * LAMBDA acquire incompatible operational meanings. */
static int compile_ast_atomic_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	compile_ref_clear(p_ret);
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	switch (node->tag) {
		case PROTOTYPE_AST_TEXT_LITERAL: {
			uint32_t term;
			if (prototype_term_text_literal(
					ctx->terms, node->as.text_literal.text_symbol_id, &term
				) != 0) {
				return -1;
			}
			return compile_ref_from_term(ctx, term, p_ret);
		}
		case PROTOTYPE_AST_INT_LITERAL: {
			uint32_t term;
			int bit_width = prototype_term_host_type_bit_width(
				ctx->intrinsic_environment->default_integer_host_type
			);
			if ((bit_width == 32 &&
					(node->as.int_literal.value < INT32_MIN ||
					 node->as.int_literal.value > INT32_MAX)) ||
				(bit_width != 32 && bit_width != 64)) {
				(void)add_compile_diagnostic(
					ctx,
					PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_GRAPH_CONSTRUCTION,
					PROTOTYPE_COMPILE_DIAGNOSTIC_INTEGER_LITERAL_RANGE,
					ast_id,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					node->span
				);
				return -1;
			}
			if (prototype_term_int_literal(
					ctx->terms, node->as.int_literal.value, &term
				) != 0) {
				return -1;
			}
			return compile_ref_from_term(ctx, term, p_ret);
		}
		case PROTOTYPE_AST_SYSTEM_NAME: {
			uint32_t term;
			if (node->as.system_name.kind == PROTOTYPE_AST_SYSTEM_NAME_HOST_TYPE) {
				if (prototype_term_make_host_type(
						ctx->terms, node->as.system_name.host_type_id, &term
					) != 0) {
					return -1;
				}
			} else if (
				node->as.system_name.kind == PROTOTYPE_AST_SYSTEM_NAME_PURE_PRIMITIVE &&
				node->as.system_name.pure_primitive_id !=
					PROTOTYPE_PURE_PRIMITIVE_UNKNOWN
			) {
				if (prototype_term_pure_primitive(
						ctx->terms,
						node->as.system_name.pure_primitive_id,
						node->as.system_name.type_symbol_id,
						&term
					) != 0) {
					return -1;
				}
			} else if (
				node->as.system_name.kind == PROTOTYPE_AST_SYSTEM_NAME_EFFECT_OPERATION &&
				node->as.system_name.effect_operation_id != PROTOTYPE_EFFECT_OPERATION_UNKNOWN
			) {
				if (prototype_term_effect_operation(
						ctx->terms,
						node->as.system_name.effect_operation_id,
						&term
					) != 0) {
					return -1;
				}
			} else {
				return -1;
			}
			return compile_ref_from_term(ctx, term, p_ret);
		}
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
		{
			uint32_t binding_id;
			uint32_t ih_scope_id;
			uint32_t ih_case_index;
			uint32_t ih_field_index;
			uint32_t argument_term;
			uint32_t argument_operation;
			uint32_t ih_term;
			if (lookup_graph_binder(
					ctx, node->as.induction_hypothesis.ast_binder_id, &binding_id
				) != 0 ||
				lookup_ih_edge(
					ctx,
					node->as.induction_hypothesis.ast_binder_id,
					&ih_scope_id,
					&ih_case_index,
					&ih_field_index
				) != 0 ||
				prototype_term_var(ctx->terms, binding_id, &argument_term) != 0 ||
				operation_add(
					ctx, PROTOTYPE_OPERATION_VAR, argument_term, PROTOTYPE_INVALID_ID,
					ast_id, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
					&argument_operation
				) != 0 ||
				prototype_term_induction_hypothesis(
					ctx->terms, ih_scope_id, argument_term, &ih_term
				) != 0) {
				return -1;
			}
			ctx->metadata->operations[argument_operation].referenced_ast_binder_id =
				node->as.induction_hypothesis.ast_binder_id;
			p_ret->term = ih_term;
			p_ret->classifier = PROTOTYPE_INVALID_ID;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_INVALID;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			if (operation_add(
				ctx, PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS, ih_term,
				PROTOTYPE_INVALID_ID, ast_id, PROTOTYPE_INVALID_ID,
				argument_operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0, &p_ret->operation
			) != 0) {
				return -1;
			}
			struct prototype_operation_node* ih_operation =
				&ctx->metadata->operations[p_ret->operation];
			ih_operation->referenced_ast_binder_id =
				node->as.induction_hypothesis.ast_binder_id;
			ih_operation->ih_scope_id = ih_scope_id;
			ih_operation->ih_case_index = ih_case_index;
			ih_operation->ih_field_index = ih_field_index;
			return 0;
		}
		case PROTOTYPE_AST_VAR: {
			const struct local_ref_map_entry* local = lookup_local_ref(
				ctx, node->as.var.ast_binder_id
			);
			if (local) {
				*p_ret = local->ref;
				if (operation_add(
						ctx, PROTOTYPE_OPERATION_NAME, local->ref.term,
						local->ref.classifier, ast_id, local->ref.operation,
						PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
						PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
						PROTOTYPE_INVALID_ID, 0, &p_ret->operation
					) != 0) {
					return -1;
				}
				ctx->metadata->operations[p_ret->operation].source_symbol_id =
					local->symbol_id;
				ctx->metadata->operations[p_ret->operation].referenced_ast_binder_id =
					node->as.var.ast_binder_id;
				ctx->metadata->operations[p_ret->operation].category = p_ret->category;
				ctx->metadata->operations[p_ret->operation].computation_kind =
					p_ret->computation_kind;
				return 0;
			}
			uint32_t binding_id;
			uint32_t classifier;
			uint32_t term;
			if (lookup_graph_binder(ctx, node->as.var.ast_binder_id, &binding_id) != 0 ||
				prototype_term_var(ctx->terms, binding_id, &term) != 0) {
				return -1;
			}
			if (lookup_graph_binder_classifier(ctx, node->as.var.ast_binder_id, &classifier) != 0) {
				classifier = PROTOTYPE_INVALID_ID;
			}
			p_ret->term = term;
			p_ret->classifier = classifier;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			if (operation_add(
				ctx, PROTOTYPE_OPERATION_VAR, term, classifier, ast_id,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				0, &p_ret->operation
				) != 0) {
				return -1;
			}
			ctx->metadata->operations[p_ret->operation].referenced_ast_binder_id =
				node->as.var.ast_binder_id;
			(void)lookup_graph_binder_symbol(
				ctx,
				node->as.var.ast_binder_id,
				&ctx->metadata->operations[p_ret->operation].source_symbol_id
			);
			return 0;
		}
		case PROTOTYPE_AST_ASCRIPTION:
		{
			uint32_t expected_classifier;
			struct compile_ref inner;
			if (compile_ast_ref(ctx, node->as.ascription.term, &inner) != 0 ||
				compile_ast_ascription_classifier(
					ctx, node->as.ascription.type_expr, &expected_classifier
				) != 0 ||
					queue_ascription_check(
						ctx, inner.term, expected_classifier, ast_id, inner.operation
						) != 0) {
					return -1;
				}
			*p_ret = inner;
			return 0;
		}
		case PROTOTYPE_AST_NAME:
		{
			struct prototype_ast_term_assignment_def* def;
			int status = resolve_unique_assignment_if_present(ctx, node->as.name.symbol_id, &def);
			if (status == 1) {
				return compile_external_ref_ref(ctx, node->as.name.symbol_id, p_ret);
			}
			if (status != 0) {
				(void)add_resolve_error(
					ctx,
					PROTOTYPE_RESOLVE_ERROR_AMBIGUOUS_ASSIGNMENT,
					node->as.name.symbol_id,
					-1,
					ast_id
				);
				return -1;
			}
			return compile_def_ref(ctx, def, p_ret);
		}
		case PROTOTYPE_AST_NAME_IN_NAMESPACE: {
			struct prototype_ast_term_assignment_def* def;
			uint32_t namespace_term;
			uint32_t constructor_term;
			uint32_t classifier;
			int status = resolve_unique_assignment_if_present(
				ctx,
				node->as.name_in_namespace.namespace_symbol_id,
				&def
			);
			if (status == 1) {
				status = compile_type_declaration_term_by_symbol(
					ctx,
					node->as.name_in_namespace.namespace_symbol_id,
					&namespace_term
				);
			} else if (status == 0) {
				status = compile_def(ctx, def, &namespace_term);
			}
			if (status != 0 || resolve_namespace_member(
					ctx,
					namespace_term,
					node->as.name_in_namespace.symbol_id,
					&constructor_term,
					&classifier
				) != 0) {
				return -1;
			}
			p_ret->term = constructor_term;
			p_ret->classifier = classifier;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			return operation_add(
				ctx, PROTOTYPE_OPERATION_CONSTRUCTOR, constructor_term, classifier, ast_id,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				0, &p_ret->operation
			);
		}
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE: {
			struct compile_ref namespace_ref;
			uint32_t constructor_term;
			uint32_t classifier;
			if (compile_ast_ref(
					ctx,
					node->as.name_in_ast_namespace.namespace_ast,
					&namespace_ref
				) != 0 ||
				resolve_namespace_member(
					ctx,
					namespace_ref.term,
					node->as.name_in_ast_namespace.symbol_id,
					&constructor_term,
					&classifier
				) != 0) {
				return -1;
			}
			p_ret->term = constructor_term;
			p_ret->classifier = classifier;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			return operation_add(
				ctx, PROTOTYPE_OPERATION_CONSTRUCTOR, constructor_term, classifier, ast_id,
				namespace_ref.operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				0, &p_ret->operation
			);
		}
		case PROTOTYPE_AST_TYPE_FORMATION: {
			uint32_t previous_binder_count = ctx->binder_count;
			uint32_t type_id;
			const struct prototype_ast_type_def* ast_type =
				&ctx->asts->type_defs[node->as.type_formation.ast_type_def_id];
			uint32_t family_binder_count =
				ast_type->parameter_count + ast_type->index_count;
			if (family_binder_count > 32) {
				return -1;
			}
			for (uint32_t i = 0; i < family_binder_count; ++i) {
				const struct prototype_ast_family_binder* parameter =
					&ctx->asts->family_binders[ast_type->first_family_binder + i];
				uint32_t binding_id;
				uint32_t binder_classifier;
				/* A telescope domain is checked in the preceding context. Extending
				 * first would incorrectly make the binder visible in its own domain and
				 * lets lazy compilation of a named domain reset that extension. */
				if (compile_ast_type_expr_term(
						ctx, parameter->type_expr, &binder_classifier
					) != 0 || push_graph_binder(
						ctx,
						parameter->ast_binder_id,
						binder_classifier,
						parameter->name_symbol_id,
						&binding_id
					) != 0) {
					ctx->binder_count = previous_binder_count;
					return -1;
				}
			}
			if (compile_ast_type_def(
					ctx, node->as.type_formation.ast_type_def_id, &type_id
				) != 0 || prototype_term_type_instance_make(
					ctx->terms, ctx->type_declarations, type_id, NULL, 0, &p_ret->term
				) != 0) {
				ctx->binder_count = previous_binder_count;
				return -1;
			}
			ctx->binder_count = previous_binder_count;
			return compile_ref_from_term(ctx, p_ret->term, p_ret);
		}
		case PROTOTYPE_AST_TYPE_LITERAL: {
			uint32_t previous_binder_count = ctx->binder_count;
			uint32_t type_id;
			const struct prototype_ast_type_def* ast_type =
				&ctx->asts->type_defs[node->as.type_literal.ast_type_def_id];
			uint32_t family_binder_count =
				ast_type->parameter_count + ast_type->index_count;
			if (family_binder_count > 32) {
				return -1;
			}
			for (uint32_t i = 0; i < family_binder_count; ++i) {
				const struct prototype_ast_family_binder* binder =
					&ctx->asts->family_binders[ast_type->first_family_binder + i];
				uint32_t binding_id;
				uint32_t classifier;
				if (compile_ast_type_expr_term(
						ctx, binder->type_expr, &classifier
					) != 0 || push_graph_binder(
						ctx,
						binder->ast_binder_id,
						classifier,
						binder->name_symbol_id,
						&binding_id
					) != 0) {
					ctx->binder_count = previous_binder_count;
					return -1;
				}
			}
			if (compile_ast_type_def(
					ctx, node->as.type_literal.ast_type_def_id, &type_id
				) != 0 || prototype_term_type_instance_make(
					ctx->terms, ctx->type_declarations, type_id, NULL, 0, &p_ret->term
				) != 0) {
				ctx->binder_count = previous_binder_count;
				return -1;
			}
			ctx->binder_count = previous_binder_count;
			return compile_ref_from_term(ctx, p_ret->term, p_ret);
		}
		default:
			return -1;
	}
}

typedef int (*compile_value_continuation_fn)(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
);

struct compile_value_continuation {
	compile_value_continuation_fn apply;
	void* data;
	int has_source_binder;
	uint32_t source_ast_binder_id;
	int source_binder_symbol_id;
	uint32_t source_binder_classifier;
};

static int compile_ast_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int compile_ast_runtime_value_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
);

static int compile_ast_constructor_spine_value_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
);

static int compile_ast_constructor_application_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int compile_ast_computation_control_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
);

static int compile_ast_application_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

static int compile_ref_make_operation_request(
	struct compile_context* ctx,
	const struct compile_ref* operation,
	const struct compile_ref* argument,
	uint32_t source_ast,
	struct compile_ref* p_ret
);

static int ast_application_head_is_constructor(
	const struct compile_context* ctx,
	uint32_t ast_id
);

/* A type-family argument is a value-level thunk of a pure computation
 * function. In a type formation context we expose its lambda only to
 * PURE_TYPE_WHNF; this is not a runtime FORCE and it cannot dispatch an
 * operation. The resulting application must still normalize to a type before
 * any classifier rule accepts it. */
static void compile_ref_open_pure_type_family(
	struct compile_context* ctx,
	struct compile_ref* ref
) {
	uint32_t computation;
	if (!ctx || !ref || ref->term >= ctx->terms->term_count ||
		ctx->terms->terms[ref->term].tag != PROTOTYPE_TERM_THUNK) {
		return;
	}
	computation = ctx->terms->terms[ref->term].as.thunk.computation;
	if (computation >= ctx->terms->term_count ||
		ctx->terms->terms[computation].tag != PROTOTYPE_TERM_LAMBDA) {
		return;
	}
	ref->term = computation;
	ref->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	ref->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
}

static int compile_ast_pure_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	struct compile_ref value;
	int status;
	uint32_t whnf;
	uint32_t result;
	if (!ctx || !p_ret) {
		return -1;
	}
	if (ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_APP) {
		struct compile_ref function;
		struct compile_ref argument;
		uint32_t application;
		if (compile_ast_pure_value_ref(ctx, node->as.app.function, &function) != 0 ||
			compile_ast_pure_value_ref(ctx, node->as.app.argument, &argument) != 0) {
			return -1;
		}
		compile_ref_open_pure_type_family(ctx, &function);
		compile_ref_open_pure_type_family(ctx, &argument);
		if (compile_shared_app(ctx, function.term, argument.term, &application) != 0 ||
			prototype_term_normalize_complete_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				application,
				&whnf
			) != 0) {
			return -1;
		}
		if (whnf < ctx->terms->term_count &&
			ctx->terms->terms[whnf].tag == PROTOTYPE_TERM_RETURN) {
			whnf = ctx->terms->terms[whnf].as.return_term.value;
		}
		return compile_ref_from_term(ctx, whnf, p_ret);
	}
	status = compile_ast_value_ref(ctx, ast_id, &value);
	if (status == 0) {
		*p_ret = value;
		return 0;
	}
	if (status < 0 || compile_ast_computation_ref(ctx, ast_id, &value) != 0 ||
		value.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return -1;
	}
	/* A type formation is a pure evaluation context. A raw lambda is the
	 * function itself here, rather than a computation whose returned value must
	 * be extracted. Its later APP is reduced under PURE_TYPE_WHNF. */
	if (value.term < ctx->terms->term_count &&
		ctx->terms->terms[value.term].tag == PROTOTYPE_TERM_LAMBDA) {
		*p_ret = value;
		return 0;
	}
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			value.term,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count ||
		ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_RETURN) {
		return -1;
	}
	result = ctx->terms->terms[whnf].as.return_term.value;
	return compile_ref_from_term(ctx, result, p_ret);
}

static int compile_ref_make_return(
	struct compile_context* ctx,
	const struct compile_ref* value,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	if (!ctx || !value || !p_ret ||
		prototype_term_return(ctx->terms, value->term, &term) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_RETURN, term, PROTOTYPE_INVALID_ID, source_ast,
		PROTOTYPE_INVALID_ID, value->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ref_make_force(
	struct compile_context* ctx,
	const struct compile_ref* value,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	uint32_t computation_classifier = PROTOTYPE_INVALID_ID;
	int computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	if (!ctx || !value || !p_ret ||
		prototype_term_force(ctx->terms, value->term, &term) != 0) {
		return -1;
	}
	/* An execution or callee demand opens this occurrence exactly once. When the
	 * value has a known Thunk(Pi(...)) classifier, retain that negative shape so
	 * a following APP applies it directly instead of sequencing it as a
	 * returning computation. */
	if (value->classifier < ctx->terms->term_count &&
		ctx->terms->terms[value->classifier].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		struct prototype_term_classifier_view view;
		computation_classifier =
			ctx->terms->terms[value->classifier].as.thunk_type.computation;
		if (prototype_judgement_classifier_view(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				computation_classifier,
				&view
			) != 0) {
			return -1;
		}
		if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION) {
			computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
		} else if (view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
			view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
		}
	}
	p_ret->term = term;
	p_ret->classifier = computation_classifier;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = computation_kind;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_FORCE, term, PROTOTYPE_INVALID_ID, source_ast,
		PROTOTYPE_INVALID_ID, value->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

/* A raw source LAMBDA is a CBPV computation function.  A use in a value
 * position must suspend that computation; this is an elaboration boundary,
 * not a second source-level function representation. */
static int compile_ref_make_thunk(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	if (!ctx || !computation || !p_ret ||
		computation->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		prototype_term_thunk(ctx->terms, computation->term, &term) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	uint32_t immediate_classifier;
	if (computation->classifier != PROTOTYPE_INVALID_ID &&
		prototype_judgement_cbpv_boundary_classifier(
			ctx->terms,
			ctx->type_declarations,
			term,
			computation->classifier,
			&immediate_classifier
		) == 0) {
		p_ret->classifier = immediate_classifier;
	}
	p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_THUNK, term, PROTOTYPE_INVALID_ID, source_ast,
		PROTOTYPE_INVALID_ID, computation->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ref_make_sequence_fold(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	const struct compile_ref* continuation,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t term;
	if (!ctx || !computation || !continuation || !p_ret ||
		prototype_term_computation_fold(
			ctx->terms, computation->term, continuation->term, NULL, 0, &term
		) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_COMPUTATION_FOLD, term, PROTOTYPE_INVALID_ID, source_ast,
		computation->operation, continuation->operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ref_thunk_computation_kind(
	const struct compile_context* ctx,
	const struct compile_ref* ref
) {
	if (!ctx || !ctx->metadata || !ref ||
		ref->operation >= ctx->metadata->operation_count) {
		return PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	}
	uint32_t operation_id = operation_runtime_unwrap_name(
		ctx->metadata, ref->operation
	);
	if (operation_id >= ctx->metadata->operation_count) {
		return PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_THUNK ||
		operation->argument >= ctx->metadata->operation_count) {
		return PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	}
	operation_id = operation_runtime_unwrap_name(ctx->metadata, operation->argument);
	if (operation_id >= ctx->metadata->operation_count) {
		return PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	}
	const struct prototype_operation_node* computation =
		&ctx->metadata->operations[operation_id];
	switch (computation->tag) {
		case PROTOTYPE_OPERATION_RETURN:
		case PROTOTYPE_OPERATION_REQUEST:
		case PROTOTYPE_OPERATION_COMPUTATION_FOLD:
			return PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
		case PROTOTYPE_OPERATION_LAMBDA:
			return PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
		default:
			return computation->computation_kind;
	}
}

static int compile_ref_is_returning_thunk(
	const struct compile_context* ctx,
	const struct compile_ref* ref,
	int* p_ret
) {
	if (!ctx || !ref || !p_ret) {
		return -1;
	}
	*p_ret = compile_ref_thunk_computation_kind(ctx, ref) ==
		PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
	if (*p_ret || ref->classifier >= ctx->terms->term_count ||
		ctx->terms->terms[ref->classifier].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	struct prototype_term_classifier_view view;
	uint32_t computation_classifier =
		ctx->terms->terms[ref->classifier].as.thunk_type.computation;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			computation_classifier,
			&view
		) != 0) {
		return -1;
	}
	*p_ret = view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
	return 0;
}

/* A runtime strict value context evaluates a returning computation exactly
 * once, then supplies its result to the consumer. Source block bindings retain
 * their AST binder and scope-slot graph binder; other strict contexts allocate
 * a synthetic binder. Both remain occurrence-specific in OperationGraph even
 * when canonical TermDB lambdas share a core node. */
static int compile_continue_runtime_computation(
	struct compile_context* ctx,
	const struct compile_ref* computation,
	uint32_t source_ast,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
) {
	uint32_t ast_binder_id;
	uint32_t binding_id;
	uint32_t variable_term;
	uint32_t lambda_term;
	uint32_t continuation_context;
	uint32_t saved_binder_count;
	struct compile_ref variable;
	struct compile_ref body;
	struct compile_ref lambda;
	if (!ctx || !computation || !continuation || !continuation->apply || !p_ret ||
		computation->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		ctx->binder_count >= 512) {
		return -1;
	}
	saved_binder_count = ctx->binder_count;
	if (continuation->has_source_binder) {
		ast_binder_id = continuation->source_ast_binder_id;
		if (push_graph_binder(
				ctx,
				ast_binder_id,
				continuation->source_binder_classifier,
				continuation->source_binder_symbol_id,
				&binding_id
			) != 0) {
			return -1;
		}
	} else {
		ast_binder_id = prototype_ast_new_binder(ctx->asts);
		binding_id = prototype_term_new_binding(ctx->terms);
		if (ast_binder_id == PROTOTYPE_INVALID_ID ||
			binding_id == PROTOTYPE_INVALID_ID || prototype_context_extend(
			&ctx->metadata->contexts,
			ctx->context_ids[saved_binder_count],
			binding_id,
			PROTOTYPE_INVALID_ID,
			ast_binder_id,
			&continuation_context
		) != 0) {
			return -1;
		}
		ctx->binders[saved_binder_count].ast_binder_id = ast_binder_id;
		ctx->binders[saved_binder_count].graph_binder_id = binding_id;
		ctx->binders[saved_binder_count].classifier = PROTOTYPE_INVALID_ID;
		ctx->binders[saved_binder_count].symbol_id = -1;
		ctx->binder_count++;
		ctx->context_ids[ctx->binder_count] = continuation_context;
	}
	if (prototype_term_var(ctx->terms, binding_id, &variable_term) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	variable.term = variable_term;
	variable.classifier = continuation->has_source_binder ?
		continuation->source_binder_classifier : PROTOTYPE_INVALID_ID;
	variable.category = PROTOTYPE_TERM_CATEGORY_VALUE;
	variable.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_VAR, variable_term, variable.classifier,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &variable.operation
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->metadata->operations[variable.operation].referenced_ast_binder_id =
		ast_binder_id;
	if (continuation->apply(ctx, &variable, continuation->data, &body) != 0 ||
		body.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		prototype_term_lambda(ctx->terms, binding_id, body.term, &lambda_term) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	lambda.term = lambda_term;
	lambda.classifier = PROTOTYPE_INVALID_ID;
	lambda.category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	lambda.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_LAMBDA, lambda_term, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, body.operation,
			PROTOTYPE_INVALID_ID,
			continuation->has_source_binder ?
				continuation->source_binder_classifier : PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0,
			&lambda.operation
		) != 0) {
		return -1;
	}
	ctx->metadata->operations[lambda.operation].referenced_ast_binder_id =
		ast_binder_id;
	ctx->metadata->operations[lambda.operation].binding_id = binding_id;
	ctx->metadata->operations[lambda.operation].binder_symbol_id =
		continuation->has_source_binder ?
			continuation->source_binder_symbol_id : -1;
	if (continuation->has_source_binder &&
		continuation->source_binder_classifier != PROTOTYPE_INVALID_ID) {
		uint32_t binder_var;
		if (prototype_term_var(
				ctx->terms, binding_id, &binder_var
			) != 0 || queue_binder_assumption(
				ctx,
				ctx->metadata->operations[body.operation].context_id,
				binder_var,
				continuation->source_binder_classifier,
				lambda.operation
			) != 0) {
			return -1;
		}
	}
	return compile_ref_make_sequence_fold(ctx, computation, &lambda, source_ast, p_ret);
}

static int compile_return_continuation(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
) {
	uint32_t source_ast = *(const uint32_t*)data;
	return compile_ref_make_return(ctx, value, source_ast, p_ret);
}

static int term_app_head_is_constructor(
	const struct prototype_term_db* terms,
	uint32_t term_id
) {
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	return prototype_term_constructor_spine_info(
		terms,
		term_id,
		&head,
		&owner,
		&constructor_id,
		arguments,
		64,
		&argument_count
	) == 0;
}

static int ast_application_head_is_constructor(
	const struct compile_context* ctx,
	uint32_t ast_id
) {
	if (!ctx || !ctx->asts || ast_id >= ctx->asts->node_count) {
		return 0;
	}
	while (ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_APP) {
		ast_id = ctx->asts->nodes[ast_id].as.app.function;
		if (ast_id >= ctx->asts->node_count) {
			return 0;
		}
	}
	return ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_NAME_IN_NAMESPACE ||
		ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_NAME_IN_AST_NAMESPACE;
}

/* A constructor spine remains a value even when it is partially applied.
 * Other computation-shaped source forms need raw lowering here so that a
 * Pi-producing computation is not first thunked and immediately forced. */
static int compile_ast_function_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_APP &&
		ast_application_head_is_constructor(ctx, ast_id)) {
		return compile_ast_ref(ctx, ast_id, p_ret);
	}
	switch (node->tag) {
		case PROTOTYPE_AST_APP:
		case PROTOTYPE_AST_LAMBDA:
		case PROTOTYPE_AST_MATCH:
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
			return compile_ast_computation_ref(ctx, ast_id, p_ret);
		default:
			return compile_ast_ref(ctx, ast_id, p_ret);
	}
}

enum compile_intrinsic_spine_kind {
	COMPILE_INTRINSIC_SPINE_NONE = 0,
	COMPILE_INTRINSIC_SPINE_PURE_PRIMITIVE,
	COMPILE_INTRINSIC_SPINE_EFFECT_OPERATION
};

/* A value whose classifier is a raw Pi is a pure type-family value. It is
 * directly applicable in the shared APP graph; ordinary suspended functions
 * have Thunk(Pi) classifiers and still require FORCE. */
static int compile_ref_has_direct_pi_classifier(
	struct compile_context* ctx,
	const struct compile_ref* ref,
	int* p_ret
) {
	uint32_t classifier_whnf;
	if (!ctx || !ref || !p_ret) {
		return -1;
	}
	*p_ret = 0;
	if (ref->classifier == PROTOTYPE_INVALID_ID ||
		ref->classifier >= ctx->terms->term_count) {
		return 0;
	}
	if (prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			ref->classifier,
			&classifier_whnf
		) != 0) {
		return -1;
	}
	*p_ret = classifier_whnf < ctx->terms->term_count &&
		ctx->terms->terms[classifier_whnf].tag == PROTOTYPE_TERM_PI;
	return 0;
}

static int compile_intrinsic_spine_info(
	const struct prototype_term_db* terms,
	uint32_t term_id,
	int* p_kind,
	uint32_t* p_argument_count,
	uint32_t* p_declared_arity
) {
	if (!terms || !p_kind || !p_argument_count || !p_declared_arity ||
		term_id >= terms->term_count) {
		return -1;
	}
	uint32_t argument_count = 0;
	while (term_id < terms->term_count &&
		terms->terms[term_id].tag == PROTOTYPE_TERM_APP) {
		term_id = terms->terms[term_id].as.app.function;
		argument_count++;
	}
	if (term_id >= terms->term_count) {
		return -1;
	}
	if (terms->terms[term_id].tag == PROTOTYPE_TERM_PURE_PRIMITIVE) {
		const struct prototype_pure_primitive_declaration* declaration =
			prototype_term_pure_primitive_declaration(
				terms->terms[term_id].as.pure_primitive.primitive_id
			);
		if (!declaration) {
			return -1;
		}
		*p_kind = COMPILE_INTRINSIC_SPINE_PURE_PRIMITIVE;
		*p_argument_count = argument_count;
		*p_declared_arity = declaration->arity;
		return 0;
	}
	if (terms->terms[term_id].tag == PROTOTYPE_TERM_EFFECT_OPERATION) {
		const struct prototype_effect_operation_declaration* declaration =
			prototype_term_effect_operation_declaration(
				terms->terms[term_id].as.effect_operation.operation_id
			);
		if (!declaration) {
			return -1;
		}
		*p_kind = COMPILE_INTRINSIC_SPINE_EFFECT_OPERATION;
		*p_argument_count = argument_count;
		*p_declared_arity = declaration->arity;
		return 0;
	}
	*p_kind = COMPILE_INTRINSIC_SPINE_NONE;
	*p_argument_count = argument_count;
	*p_declared_arity = 0;
	return 0;
}

static int compile_application_make_computation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	const struct compile_ref* argument,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	struct compile_ref applied_function;
	int intrinsic_kind;
	uint32_t intrinsic_argument_count;
	uint32_t intrinsic_arity;
	uint32_t term;
	if (!ctx || !function || !argument || !p_ret) {
		return -1;
	}
	if (term_app_head_is_constructor(ctx->terms, function->term)) {
		struct compile_ref value;
		if (compile_shared_app(ctx, function->term, argument->term, &term) != 0 ||
			operation_add(
				ctx, PROTOTYPE_OPERATION_APP, term, PROTOTYPE_INVALID_ID,
				source_ast, function->operation, argument->operation,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, 0, &value.operation
			) != 0) {
			return -1;
		}
		value.term = term;
		value.classifier = PROTOTYPE_INVALID_ID;
		value.category = PROTOTYPE_TERM_CATEGORY_VALUE;
		value.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
		return compile_ref_make_return(ctx, &value, source_ast, p_ret);
	}
	if (compile_intrinsic_spine_info(
			ctx->terms,
			function->term,
			&intrinsic_kind,
			&intrinsic_argument_count,
			&intrinsic_arity
		) != 0 || (intrinsic_kind != COMPILE_INTRINSIC_SPINE_NONE &&
			intrinsic_argument_count >= intrinsic_arity)) {
		return -1;
	}
	if (intrinsic_kind == COMPILE_INTRINSIC_SPINE_EFFECT_OPERATION &&
		intrinsic_argument_count + 1 == intrinsic_arity) {
		return compile_ref_make_operation_request(
			ctx, function, argument, source_ast, p_ret
		);
	}
	if (intrinsic_kind != COMPILE_INTRINSIC_SPINE_NONE) {
		if (compile_shared_app(ctx, function->term, argument->term, &term) != 0) {
			return -1;
		}
		p_ret->term = term;
		p_ret->classifier = PROTOTYPE_INVALID_ID;
		if (function->classifier != PROTOTYPE_INVALID_ID &&
			argument->classifier != PROTOTYPE_INVALID_ID &&
			operation_apply_classifier(
				ctx,
				function->classifier,
				argument->classifier,
				argument->term,
				&p_ret->classifier
			) != 0) {
			return -1;
		}
		p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
		p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
		if (operation_add(
			ctx, PROTOTYPE_OPERATION_APP, term, p_ret->classifier,
			source_ast, function->operation, argument->operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &p_ret->operation
		) != 0) {
			return -1;
		}
		ctx->metadata->operations[p_ret->operation].category = p_ret->category;
		ctx->metadata->operations[p_ret->operation].computation_kind =
			p_ret->computation_kind;
		return 0;
	}
	applied_function = *function;
	if (function->category == PROTOTYPE_TERM_CATEGORY_VALUE) {
		int direct_pi;
		if (compile_ref_has_direct_pi_classifier(ctx, function, &direct_pi) != 0) {
			return -1;
		}
		if (direct_pi) {
			uint32_t result_classifier = PROTOTYPE_INVALID_ID;
			if (argument->classifier != PROTOTYPE_INVALID_ID &&
				operation_apply_classifier(
					ctx,
					function->classifier,
					argument->classifier,
					argument->term,
					&result_classifier
				) != 0) {
				return -1;
			}
			if (prototype_term_app(
					ctx->terms, function->term, argument->term, &term
				) != 0 || operation_add(
					ctx, PROTOTYPE_OPERATION_APP, term, result_classifier,
					source_ast, function->operation, argument->operation,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
					&p_ret->operation
				) != 0) {
				return -1;
			}
			p_ret->term = term;
			p_ret->classifier = result_classifier;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			ctx->metadata->operations[p_ret->operation].category =
				PROTOTYPE_TERM_CATEGORY_VALUE;
			return 0;
		}
		if (compile_ref_make_force(ctx, function, source_ast, &applied_function) != 0) {
			return -1;
		}
	} else if (function->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return -1;
	}
	if (compile_shared_app(ctx, applied_function.term, argument->term, &term) != 0 ||
		operation_add(
			ctx, PROTOTYPE_OPERATION_APP, term, PROTOTYPE_INVALID_ID,
			source_ast, applied_function.operation, argument->operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &p_ret->operation
		) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	return 0;
}

struct compile_application_spine_context {
	uint32_t arguments[64];
	uint32_t applications[64];
	uint32_t argument_count;
};

struct compile_application_spine_step_context {
	const struct compile_application_spine_context* spine;
	struct compile_ref function;
	uint32_t index;
};

static int compile_application_spine_step(
	struct compile_context* ctx,
	const struct compile_application_spine_context* spine,
	const struct compile_ref* function,
	uint32_t index,
	struct compile_ref* p_ret
);

static int compile_application_spine_argument_continuation(
	struct compile_context* ctx,
	const struct compile_ref* argument,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_application_spine_step_context* context = data;
	struct compile_ref application;
	if (!ctx || !argument || !context || !context->spine || !p_ret ||
		context->index >= context->spine->argument_count ||
		compile_application_make_computation(
			ctx,
			&context->function,
			argument,
			context->spine->applications[context->index],
			&application
		) != 0) {
		return -1;
	}
	return compile_application_spine_step(
		ctx, context->spine, &application, context->index + 1, p_ret
	);
}

static int compile_application_spine_step(
	struct compile_context* ctx,
	const struct compile_application_spine_context* spine,
	const struct compile_ref* function,
	uint32_t index,
	struct compile_ref* p_ret
) {
	if (!ctx || !spine || !function || !p_ret || index > spine->argument_count) {
		return -1;
	}
	if (index == spine->argument_count) {
		if (function->category == PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			*p_ret = *function;
			return 0;
		}
		if (function->category == PROTOTYPE_TERM_CATEGORY_VALUE) {
			uint32_t source_ast = spine->applications[spine->argument_count - 1];
			return compile_ref_make_return(ctx, function, source_ast, p_ret);
		}
		return -1;
	}
	struct compile_application_spine_step_context context = {
		spine, *function, index
	};
	struct compile_value_continuation continuation = {
		.apply = compile_application_spine_argument_continuation,
		.data = &context
	};
	return compile_ast_runtime_value_then(
		ctx, spine->arguments[index], &continuation, p_ret
	);
}

static int compile_ast_application_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	struct compile_application_spine_context spine;
	struct compile_ref function;
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count ||
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_APP) {
		return -1;
	}
	memset(&spine, 0, sizeof(spine));
	uint32_t head = ast_id;
	while (head < ctx->asts->node_count &&
		ctx->asts->nodes[head].tag == PROTOTYPE_AST_APP) {
		if (spine.argument_count >= 64) {
			return -1;
		}
		spine.arguments[spine.argument_count] =
			ctx->asts->nodes[head].as.app.argument;
		spine.applications[spine.argument_count] = head;
		spine.argument_count++;
		head = ctx->asts->nodes[head].as.app.function;
	}
	if (head >= ctx->asts->node_count || spine.argument_count == 0) {
		return -1;
	}
	for (uint32_t i = 0; i < spine.argument_count / 2; ++i) {
		uint32_t other = spine.argument_count - i - 1;
		uint32_t argument = spine.arguments[i];
		uint32_t application = spine.applications[i];
		spine.arguments[i] = spine.arguments[other];
		spine.applications[i] = spine.applications[other];
		spine.arguments[other] = argument;
		spine.applications[other] = application;
	}
	if (compile_ast_function_ref(ctx, head, &function) != 0) {
		return -1;
	}
	return compile_application_spine_step(ctx, &spine, &function, 0, p_ret);
}

enum ast_lambda_exit_presence {
	AST_LAMBDA_EXIT_NONE = 0,
	AST_LAMBDA_EXIT_FOUND,
	AST_LAMBDA_EXIT_FORBIDDEN
};

static int ast_lambda_exit_merge(int left, int right) {
	if (left == AST_LAMBDA_EXIT_FORBIDDEN || right == AST_LAMBDA_EXIT_FORBIDDEN) {
		return AST_LAMBDA_EXIT_FORBIDDEN;
	}
	return left == AST_LAMBDA_EXIT_FOUND || right == AST_LAMBDA_EXIT_FOUND ?
		AST_LAMBDA_EXIT_FOUND : AST_LAMBDA_EXIT_NONE;
}

static int ast_lambda_exit_presence_in(
	const struct prototype_ast_db* asts,
	uint32_t ast_id
) {
	if (!asts || ast_id >= asts->node_count) {
		return AST_LAMBDA_EXIT_FORBIDDEN;
	}
	const struct prototype_ast_node* node = &asts->nodes[ast_id];
	switch (node->tag) {
		case PROTOTYPE_AST_LAMBDA:
			return AST_LAMBDA_EXIT_NONE;
		case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
			return ast_lambda_exit_presence_in(
				asts, node->as.block_lambda_exit.value
			) == AST_LAMBDA_EXIT_NONE ?
				AST_LAMBDA_EXIT_FOUND : AST_LAMBDA_EXIT_FORBIDDEN;
		case PROTOTYPE_AST_COMPUTATION_BLOCK: {
			if (node->as.block.item_count == 0 ||
				node->as.block.result_item_index >= node->as.block.item_count ||
				(size_t)node->as.block.first_item + node->as.block.item_count >
					asts->block_item_count) {
				return AST_LAMBDA_EXIT_FORBIDDEN;
			}
			int result = AST_LAMBDA_EXIT_NONE;
			for (uint32_t i = 0; i <= node->as.block.result_item_index; ++i) {
				uint32_t item_id = asts->block_items[node->as.block.first_item + i];
				if (item_id >= asts->node_count) {
					return AST_LAMBDA_EXIT_FORBIDDEN;
				}
				const struct prototype_ast_node* item = &asts->nodes[item_id];
				uint32_t child;
				switch (item->tag) {
					case PROTOTYPE_AST_BLOCK_BINDING:
						child = item->as.block_binding.value;
						break;
					case PROTOTYPE_AST_BLOCK_EXPRESSION:
						child = item->as.block_expression.term;
						break;
					case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
						child = item_id;
						break;
					default:
						return AST_LAMBDA_EXIT_FORBIDDEN;
				}
				result = ast_lambda_exit_merge(
					result, ast_lambda_exit_presence_in(asts, child)
				);
				if (result == AST_LAMBDA_EXIT_FORBIDDEN) {
					return result;
				}
			}
			return result;
		}
		case PROTOTYPE_AST_MATCH: {
			int result = ast_lambda_exit_presence_in(asts, node->as.match.scrutinee);
			if ((size_t)node->as.match.first_case + node->as.match.case_count >
				asts->case_count) {
				return AST_LAMBDA_EXIT_FORBIDDEN;
			}
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				result = ast_lambda_exit_merge(
					result,
					ast_lambda_exit_presence_in(
						asts, asts->cases[node->as.match.first_case + i].body
					)
				);
			}
			return result;
		}
		case PROTOTYPE_AST_APP:
			return ast_lambda_exit_merge(
				ast_lambda_exit_presence_in(asts, node->as.app.function),
				ast_lambda_exit_presence_in(asts, node->as.app.argument)
			);
		case PROTOTYPE_AST_ASCRIPTION:
			return ast_lambda_exit_presence_in(asts, node->as.ascription.term);
		case PROTOTYPE_AST_QUOTE:
			return ast_lambda_exit_presence_in(asts, node->as.unary.term) ==
				AST_LAMBDA_EXIT_NONE ? AST_LAMBDA_EXIT_NONE :
				AST_LAMBDA_EXIT_FORBIDDEN;
		case PROTOTYPE_AST_COMPUTATION_FOLD: {
			int result = ast_lambda_exit_presence_in(
				asts, node->as.computation_fold.computation
			);
			if ((size_t)node->as.computation_fold.first_clause +
				node->as.computation_fold.clause_count >
				asts->computation_fold_clause_count) {
				return AST_LAMBDA_EXIT_FORBIDDEN;
			}
			for (uint32_t i = 0; i < node->as.computation_fold.clause_count; ++i) {
				const struct prototype_ast_computation_fold_clause* clause =
					&asts->computation_fold_clauses[
						node->as.computation_fold.first_clause + i
					];
				result = ast_lambda_exit_merge(
					result, ast_lambda_exit_presence_in(asts, clause->operation)
				);
				result = ast_lambda_exit_merge(
					result, ast_lambda_exit_presence_in(asts, clause->body)
				);
			}
			result = ast_lambda_exit_merge(
				result,
				ast_lambda_exit_presence_in(asts, node->as.computation_fold.return_body)
			);
			return result == AST_LAMBDA_EXIT_NONE ? AST_LAMBDA_EXIT_NONE :
				AST_LAMBDA_EXIT_FORBIDDEN;
		}
		default:
			return AST_LAMBDA_EXIT_NONE;
	}
}

static int compile_ast_lambda_computation_ref(
	struct compile_context* ctx,
	const struct prototype_ast_node* node,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t saved_binder_count;
	uint32_t binder_classifier;
	uint32_t binding_id;
	struct compile_ref body;
	uint32_t lambda_term;
	uint32_t lambda_operation;
	if (!ctx || !node || !p_ret || node->tag != PROTOTYPE_AST_LAMBDA) {
		return -1;
	}
	saved_binder_count = ctx->binder_count;
	int binder_has_latent_effect =
		node->as.lambda.binder_type < ctx->asts->type_expr_count &&
		(ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		 ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_PI ||
			 ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
				 PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE);
	int exit_presence = ast_lambda_exit_presence_in(ctx->asts, node->as.lambda.body);
	if (exit_presence == AST_LAMBDA_EXIT_FORBIDDEN ||
		(binder_has_latent_effect ? compile_ast_binder_value_type_with_latent_effect_row(
				ctx, node->as.lambda.binder_type, &(uint32_t){ PROTOTYPE_INVALID_ID },
				&binder_classifier
			) : compile_ast_type_expr_term(
				ctx, node->as.lambda.binder_type, &binder_classifier
			)) != 0 ||
		push_graph_binder(
			ctx, node->as.lambda.ast_binder_id, binder_classifier,
			node->as.lambda.binder_symbol_id, &binding_id
		) != 0 || (exit_presence == AST_LAMBDA_EXIT_FOUND ?
			compile_ast_computation_control_ref(
				ctx, node->as.lambda.body, NULL, NULL, &body
			) : compile_ast_computation_ref(
				ctx, node->as.lambda.body, &body
			)) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->binder_count = saved_binder_count;
	if (body.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		prototype_term_lambda(ctx->terms, binding_id, body.term, &lambda_term) != 0) {
		return -1;
	}
	p_ret->term = lambda_term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_LAMBDA, lambda_term, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, body.operation,
			PROTOTYPE_INVALID_ID, binder_classifier, PROTOTYPE_INVALID_ID, 0,
			&lambda_operation
		) != 0) {
		return -1;
	}
	ctx->metadata->operations[lambda_operation].binder_symbol_id =
		node->as.lambda.binder_symbol_id;
	ctx->metadata->operations[lambda_operation].referenced_ast_binder_id =
		node->as.lambda.ast_binder_id;
	ctx->metadata->operations[lambda_operation].binding_id = binding_id;
	struct prototype_operation_node* lambda_operation_node =
		&ctx->metadata->operations[lambda_operation];
	for (uint32_t i = 0; i < (uint32_t)ctx->terms->term_count; ++i) {
		if (ctx->terms->terms[i].tag != PROTOTYPE_TERM_EFFECT_ROW_VAR) {
			continue;
		}
		uint32_t row_binder = ctx->terms->terms[i].as.effect_row_var.binding_id;
		if (!prototype_term_contains_free_binding(
				ctx->terms, binder_classifier, row_binder
			)) {
			continue;
		}
		int already_recorded = 0;
		for (uint32_t j = 0;
			j < lambda_operation_node->implicit_effect_row_count;
			++j) {
			if (lambda_operation_node->implicit_effect_row_binders[j] == row_binder) {
				already_recorded = 1;
				break;
			}
		}
		if (already_recorded) {
			continue;
		}
		if (lambda_operation_node->implicit_effect_row_count >= 16) {
			return -1;
		}
		lambda_operation_node->implicit_effect_row_binders[
			lambda_operation_node->implicit_effect_row_count++
		] = row_binder;
	}
	uint32_t binder_var;
	if (prototype_term_var(ctx->terms, binding_id, &binder_var) != 0 ||
		queue_binder_assumption(
			ctx,
			ctx->metadata->operations[body.operation].context_id,
			binder_var,
			binder_classifier,
			lambda_operation
		) != 0) {
		return -1;
	}
	p_ret->operation = lambda_operation;
	return 0;
}

/* Constructor application is a value spine.  This is deliberately decided
 * from the shared TermDB head after recursively lowering its value fields;
 * ordinary source application continues through FORCE/APP/COMPUTATION_FOLD below. */
static int compile_ast_constructor_application_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count ||
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_APP) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	struct compile_ref function;
	struct compile_ref argument;
	uint32_t term;
	int status = compile_ast_value_ref(ctx, node->as.app.function, &function);
	if (status != 0) {
		return status;
	}
	status = compile_ast_value_ref(ctx, node->as.app.argument, &argument);
	if (status != 0) {
		return status;
	}
	if (!term_app_head_is_constructor(ctx->terms, function.term)) {
		return 1;
	}
	if (compile_shared_app(ctx, function.term, argument.term, &term) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	return operation_add(
		ctx, PROTOTYPE_OPERATION_APP, term, PROTOTYPE_INVALID_ID, ast_id,
		function.operation, argument.operation, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
		&p_ret->operation
	);
}

static int compile_ast_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_LAMBDA) {
		return 1;
	}
	if (node->tag == PROTOTYPE_AST_APP) {
		if (!ast_application_head_is_constructor(ctx, ast_id)) {
			return 1;
		}
		return compile_ast_constructor_application_value_ref(ctx, ast_id, p_ret);
	}
		if (node->tag == PROTOTYPE_AST_MATCH ||
			node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK ||
			node->tag == PROTOTYPE_AST_BLOCK_BINDING ||
			node->tag == PROTOTYPE_AST_BLOCK_EXPRESSION ||
			node->tag == PROTOTYPE_AST_BLOCK_LAMBDA_EXIT ||
		node->tag == PROTOTYPE_AST_COMPUTATION_FOLD ||
		node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		return 1;
	}
	if (node->tag == PROTOTYPE_AST_QUOTE) {
		struct compile_ref preserved;
		if (compile_ast_ref(ctx, node->as.unary.term, &preserved) != 0) {
			return -1;
		}
		if (preserved.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return compile_ref_make_thunk(ctx, &preserved, ast_id, p_ret);
		}
		if (preserved.category == PROTOTYPE_TERM_CATEGORY_VALUE &&
			preserved.classifier < ctx->terms->term_count &&
			ctx->terms->terms[preserved.classifier].tag ==
				PROTOTYPE_TERM_THUNK_TYPE) {
			*p_ret = preserved;
			return 0;
		}
		return -1;
	}
	if (node->tag == PROTOTYPE_AST_ASCRIPTION) {
		struct compile_ref value;
		uint32_t expected;
		uint32_t ascription_operation;
		int status = compile_ast_value_ref(ctx, node->as.ascription.term, &value);
		if (status != 0) {
			return status;
		}
		if (compile_ast_type_expr_term(ctx, node->as.ascription.type_expr, &expected) != 0 ||
			operation_add(
				ctx,
				PROTOTYPE_OPERATION_ASCRIPTION,
				value.term,
				expected,
				ast_id,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				value.operation,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				0,
				&ascription_operation
			) != 0 || queue_ascription_check(
				ctx, value.term, expected, ast_id, value.operation
			) != 0) {
			return -1;
		}
		*p_ret = value;
		p_ret->operation = ascription_operation;
		p_ret->classifier = expected;
		return 0;
	}
	if (node->tag == PROTOTYPE_AST_NAME_IN_AST_NAMESPACE) {
		struct compile_ref namespace_ref;
		uint32_t constructor_term;
		uint32_t classifier;
		if (compile_ast_pure_value_ref(
				ctx, node->as.name_in_ast_namespace.namespace_ast, &namespace_ref
			) != 0 || resolve_namespace_member(
				ctx,
				namespace_ref.term,
				node->as.name_in_ast_namespace.symbol_id,
				&constructor_term,
				&classifier
			) != 0) {
			return -1;
		}
		p_ret->term = constructor_term;
		p_ret->classifier = classifier;
		p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
		p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
		return operation_add(
			ctx, PROTOTYPE_OPERATION_CONSTRUCTOR, constructor_term, classifier, ast_id,
			namespace_ref.operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
			&p_ret->operation
		);
	}
	if (node->tag == PROTOTYPE_AST_SYSTEM_NAME) {
		if (compile_ast_atomic_ref(ctx, ast_id, p_ret) != 0) {
			return -1;
		}
		uint32_t operation;
		if (operation_add(
				ctx, PROTOTYPE_OPERATION_ATOM, p_ret->term, p_ret->classifier, ast_id,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
				&operation
			) != 0) {
			return -1;
		}
		ctx->metadata->operations[operation].category = p_ret->category;
		ctx->metadata->operations[operation].computation_kind =
			p_ret->computation_kind;
		p_ret->operation = operation;
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta,
			ctx->metadata->operations[operation].context_id
		);
		/* System signatures are declaration facts needed by APP and request
		 * constraints in the same fixed-point round. */
		if (p_ret->term < ctx->terms->term_count &&
			ctx->terms->terms[p_ret->term].tag == PROTOTYPE_TERM_PURE_PRIMITIVE &&
			prototype_judgement_delta_record_pure_primitive_type(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				p_ret->term,
				p_ret->classifier
			) != 0) {
			return -1;
		}
		if (p_ret->term < ctx->terms->term_count &&
			ctx->terms->terms[p_ret->term].tag == PROTOTYPE_TERM_EFFECT_OPERATION &&
			prototype_judgement_delta_record_effect_operation_type(
				&ctx->judgement_delta,
				ctx->terms,
				ctx->type_declarations,
				p_ret->term,
				p_ret->classifier
			) != 0) {
			return -1;
		}
		return 0;
	}
	switch (node->tag) {
		case PROTOTYPE_AST_TEXT_LITERAL:
		case PROTOTYPE_AST_INT_LITERAL:
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
		case PROTOTYPE_AST_VAR:
		case PROTOTYPE_AST_NAME:
		case PROTOTYPE_AST_NAME_IN_NAMESPACE:
		case PROTOTYPE_AST_TYPE_FORMATION:
		case PROTOTYPE_AST_TYPE_LITERAL:
			if (compile_ast_atomic_ref(ctx, ast_id, p_ret) != 0) {
				return -1;
			}
			return p_ret->category == PROTOTYPE_TERM_CATEGORY_COMPUTATION ? 1 : 0;
		default:
			return -1;
	}
}

static int compile_ast_runtime_value_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
) {
	struct compile_ref value;
	int status;
	if (!ctx || !continuation || !continuation->apply || !p_ret) {
		return -1;
	}
	/* Constructor spines need an all-or-nothing lowering path.  The ordinary
	 * value probe recursively emits partial APP occurrences before it discovers
	 * a computed field.  Falling back after that probe would retain those
	 * occurrences and can feed RETURN(partial-constructor) to the next field. */
	if (ast_application_head_is_constructor(ctx, ast_id)) {
		return compile_ast_constructor_spine_value_then(
			ctx, ast_id, continuation, p_ret
		);
	}
	compile_ref_clear(&value);
	status = compile_ast_value_ref(ctx, ast_id, &value);
	if (status == 0) {
		int execute_thunk;
		if (compile_ref_is_returning_thunk(ctx, &value, &execute_thunk) != 0) {
			return -1;
		}
		if (execute_thunk && ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_QUOTE) {
			struct compile_ref forced;
			if (compile_ref_make_force(ctx, &value, ast_id, &forced) != 0) {
				return -1;
			}
			return compile_continue_runtime_computation(
				ctx, &forced, ast_id, continuation, p_ret
			);
		}
		return continuation->apply(ctx, &value, continuation->data, p_ret);
	}
	if (status < 0) {
		return -1;
	}
	/* An atomic name may already have returned its computation occurrence while
	 * reporting that it is not a value.  Avoid compiling that source occurrence
	 * twice, because OperationGraph edges are occurrence-sensitive. */
	if (value.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		if (compile_ast_computation_ref(ctx, ast_id, &value) != 0 ||
			value.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			return -1;
		}
	}
	/* Raw functions are quoted explicitly with `&`.  IH uses FUNCTION as a
	 * provisional lowering candidate until its motive result is solved, so it
	 * still enters the computation-fold constraint path. */
	if (value.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION &&
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		return -1;
	}
	return compile_continue_runtime_computation(
		ctx, &value, ast_id, continuation, p_ret
	);
}

struct compile_constructor_spine_function_context {
	uint32_t argument_ast;
	uint32_t source_ast;
	const struct compile_value_continuation* continuation;
};

struct compile_constructor_spine_argument_context {
	struct compile_ref function;
	uint32_t source_ast;
	const struct compile_value_continuation* continuation;
};

static int compile_constructor_spine_argument_continuation(
	struct compile_context* ctx,
	const struct compile_ref* argument,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_constructor_spine_argument_context* context = data;
	struct compile_ref application;
	uint32_t term;
	if (!ctx || !argument || !context || !context->continuation || !p_ret ||
		compile_shared_app(ctx, context->function.term, argument->term, &term) != 0) {
		return -1;
	}
	application.term = term;
	application.classifier = PROTOTYPE_INVALID_ID;
	application.category = PROTOTYPE_TERM_CATEGORY_VALUE;
	application.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_APP, term, PROTOTYPE_INVALID_ID,
			context->source_ast, context->function.operation, argument->operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &application.operation
		) != 0) {
		return -1;
	}
	return context->continuation->apply(
		ctx, &application, context->continuation->data, p_ret
	);
}

static int compile_constructor_spine_function_continuation(
	struct compile_context* ctx,
	const struct compile_ref* function,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_constructor_spine_function_context* function_context = data;
	struct compile_constructor_spine_argument_context argument_context;
	struct compile_value_continuation argument_continuation = { 0 };
	if (!ctx || !function || !function_context || !p_ret) {
		return -1;
	}
	argument_context.function = *function;
	argument_context.source_ast = function_context->source_ast;
	argument_context.continuation = function_context->continuation;
	argument_continuation.apply = compile_constructor_spine_argument_continuation;
	argument_continuation.data = &argument_context;
	return compile_ast_runtime_value_then(
		ctx, function_context->argument_ast, &argument_continuation, p_ret
	);
}

/* Constructor formation is a value spine.  Sequence every computed field
 * left-to-right, but pass each partially formed constructor as a value rather
 * than treating it as a computation function. */
static int compile_ast_constructor_spine_value_then(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* continuation,
	struct compile_ref* p_ret
) {
	if (!ctx || !continuation || !continuation->apply || !p_ret ||
		ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag != PROTOTYPE_AST_APP) {
		struct compile_ref head;
		if (compile_ast_value_ref(ctx, ast_id, &head) != 0 ||
			!term_app_head_is_constructor(ctx->terms, head.term)) {
			return -1;
		}
		return continuation->apply(ctx, &head, continuation->data, p_ret);
	}
	struct compile_constructor_spine_function_context function_context = {
		node->as.app.argument, ast_id, continuation
	};
	struct compile_value_continuation function_continuation = {
		.apply = compile_constructor_spine_function_continuation,
		.data = &function_context
	};
	return compile_ast_constructor_spine_value_then(
		ctx, node->as.app.function, &function_continuation, p_ret
	);
}

struct compile_match_scrutinee_context {
	const struct prototype_ast_node* node;
	uint32_t source_ast;
};

static int compile_match_scrutinee_continuation(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
) {
	struct compile_match_scrutinee_context* context = data;
	if (!ctx || !value || !context || !p_ret) {
		return -1;
	}
	return compile_ast_match_from_value_ref(
		ctx, context->source_ast, context->node, value, p_ret
	);
}

static int compile_ast_match_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	struct compile_ref* p_ref
) {
	struct compile_match_scrutinee_context context;
	struct compile_value_continuation continuation = { 0 };
	if (!ctx || !node || !p_ref || node->tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	context.node = node;
	context.source_ast = ast_id;
	continuation.apply = compile_match_scrutinee_continuation;
	continuation.data = &context;
	return compile_ast_runtime_value_then(
		ctx, node->as.match.scrutinee, &continuation, p_ref
	);
}

static int compile_ref_make_operation_request(
	struct compile_context* ctx,
	const struct compile_ref* operation,
	const struct compile_ref* argument,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	uint32_t binding_id;
	uint32_t binder_var;
	uint32_t result;
	uint32_t continuation;
	uint32_t continuation_thunk;
	uint32_t request;
	uint32_t application_classifier;
	struct prototype_term_classifier_view application_view;
	uint32_t empty_effects;
	uint32_t return_classifier;
	uint32_t continuation_family;
	uint32_t continuation_classifier;
	uint32_t canonical_result;
	uint32_t canonical_result_variable;
	uint32_t canonical_binder_id;
	uint32_t parent_context_id;
	uint32_t continuation_context_id;
	struct compile_ref result_variable;
	struct compile_ref result_return;
	uint32_t continuation_operation;
	if (!ctx || !operation || !argument || !p_ret ||
		operation->classifier == PROTOTYPE_INVALID_ID ||
		operation_apply_classifier_unchecked(
			ctx,
			operation->classifier,
			argument->term,
			&application_classifier
		) != 0 ||
		prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			application_classifier,
			&application_view
		) != 0 || application_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		prototype_term_effect_row_empty(ctx->terms, &empty_effects
		) != 0 || prototype_term_computation_type(
			ctx->terms, empty_effects, application_view.result, &return_classifier
		) != 0 ||
		(binding_id = prototype_term_new_binding(ctx->terms)) == PROTOTYPE_INVALID_ID ||
		prototype_term_pure_family(
			ctx->terms, binding_id, return_classifier, &continuation_family
		) != 0 || prototype_term_pi_family(
			ctx->terms, application_view.result, continuation_family,
			&continuation_classifier
		) != 0 ||
		prototype_term_var(ctx->terms, binding_id, &binder_var) != 0 ||
		prototype_term_return(ctx->terms, binder_var, &result) != 0 ||
		prototype_term_lambda(ctx->terms, binding_id, result, &continuation) != 0 ||
		prototype_term_thunk(ctx->terms, continuation, &continuation_thunk) != 0 ||
		prototype_term_operation_request(
			ctx->terms, operation->term, argument->term, continuation_thunk, &request
		) != 0) {
		return -1;
	}
	/* `prototype_term_lambda` hash-conses alpha-equivalent binders.  The
	 * provisional binder used to construct the lambda can therefore differ
	 * from the binder in the stored lambda.  Source-operation metadata must
	 * point at the stored body, never at that provisional RETURN node. */
	if (continuation >= ctx->terms->term_count ||
		ctx->terms->terms[continuation].tag != PROTOTYPE_TERM_LAMBDA) {
		return -1;
	}
	canonical_result = ctx->terms->terms[continuation].as.lambda.body;
	if (canonical_result >= ctx->terms->term_count ||
		ctx->terms->terms[canonical_result].tag != PROTOTYPE_TERM_RETURN) {
		return -1;
	}
	canonical_result_variable =
		ctx->terms->terms[canonical_result].as.return_term.value;
	if (canonical_result_variable >= ctx->terms->term_count ||
		ctx->terms->terms[canonical_result_variable].tag != PROTOTYPE_TERM_VAR) {
		return -1;
	}
	canonical_binder_id =
		ctx->terms->terms[canonical_result_variable].as.var.binding_id;
	parent_context_id = ctx->context_ids[ctx->binder_count];
	if (prototype_context_extend(
			&ctx->metadata->contexts,
			parent_context_id,
			canonical_binder_id,
			application_view.result,
			PROTOTYPE_INVALID_ID,
			&continuation_context_id
		) != 0) {
		return -1;
	}
	result_variable.term = canonical_result_variable;
	result_variable.classifier = application_view.result;
	result_variable.category = PROTOTYPE_TERM_CATEGORY_VALUE;
	result_variable.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	ctx->context_ids[ctx->binder_count] = continuation_context_id;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_VAR, canonical_result_variable,
			application_view.result,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &result_variable.operation
		) != 0 || compile_ref_make_return(
			ctx, &result_variable, source_ast, &result_return
		) != 0) {
		ctx->context_ids[ctx->binder_count] = parent_context_id;
		return -1;
	}
	ctx->context_ids[ctx->binder_count] = parent_context_id;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_LAMBDA, continuation, continuation_classifier,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			result_return.operation, PROTOTYPE_INVALID_ID, application_view.result,
			PROTOTYPE_INVALID_ID, 0, &continuation_operation
		) != 0) {
		return -1;
	}
	ctx->metadata->operations[continuation_operation].binding_id =
		canonical_binder_id;
	p_ret->term = request;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	if (operation_add(
			ctx, PROTOTYPE_OPERATION_THUNK, continuation_thunk, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, continuation_operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &continuation_operation
		) != 0) {
		return -1;
	}
	return operation_add(
		ctx, PROTOTYPE_OPERATION_REQUEST, request, application_classifier,
		source_ast, operation->operation, argument->operation,
		continuation_operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
		PROTOTYPE_INVALID_ID, 0, &p_ret->operation
	);
}

static int compile_ast_block_execution_ref(
	struct compile_context* ctx,
	uint32_t source_ast,
	uint32_t value_ast,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || source_ast >= ctx->asts->node_count ||
		value_ast >= ctx->asts->node_count) {
		return -1;
	}
	struct compile_ref value;
	int value_status =
		ctx->asts->nodes[value_ast].tag ==
			PROTOTYPE_AST_INDUCTION_HYPOTHESIS ?
		compile_ast_computation_ref(ctx, value_ast, &value) :
		compile_ast_ref(ctx, value_ast, &value);
	if (value_status != 0) {
		return -1;
	}
	int execute_thunk;
	if (compile_ref_is_returning_thunk(ctx, &value, &execute_thunk) != 0) {
		return -1;
	}
	if (value.category == PROTOTYPE_TERM_CATEGORY_VALUE && execute_thunk &&
		ctx->asts->nodes[value_ast].tag != PROTOTYPE_AST_QUOTE) {
		struct compile_ref forced;
		if (compile_ref_make_force(ctx, &value, source_ast, &forced) != 0) {
			return -1;
		}
		value = forced;
	}
	*p_ret = value;
	return 0;
}

static int compile_ast_block_items_ref(
	struct compile_context* ctx,
	uint32_t block_ast,
	const struct prototype_ast_node* block,
	uint32_t item_index,
	uint32_t cutoff_index,
	struct compile_ref* p_ret
);

struct compile_block_tail_context {
	uint32_t block_ast;
	const struct prototype_ast_node* block;
	uint32_t next_item_index;
	uint32_t cutoff_index;
};

static int compile_block_tail_after_discard(
	struct compile_context* ctx,
	const struct compile_ref* ignored,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_block_tail_context* tail = data;
	(void)ignored;
	if (!ctx || !tail || !tail->block || !p_ret) {
		return -1;
	}
	return compile_ast_block_items_ref(
		ctx,
		tail->block_ast,
		tail->block,
		tail->next_item_index,
		tail->cutoff_index,
		p_ret
	);
}

static int compile_ast_block_items_ref(
	struct compile_context* ctx,
	uint32_t block_ast,
	const struct prototype_ast_node* block,
	uint32_t item_index,
	uint32_t cutoff_index,
	struct compile_ref* p_ret
) {
	if (!ctx || !block || !p_ret || block->tag != PROTOTYPE_AST_COMPUTATION_BLOCK ||
		item_index > cutoff_index || cutoff_index >= block->as.block.item_count ||
		(size_t)block->as.block.first_item + block->as.block.item_count >
			ctx->asts->block_item_count) {
		return -1;
	}
	uint32_t item_ast = ctx->asts->block_items[
		block->as.block.first_item + item_index
	];
	if (item_ast >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* item = &ctx->asts->nodes[item_ast];
	int is_result = item_index == cutoff_index;
	if (item->tag == PROTOTYPE_AST_BLOCK_EXPRESSION) {
		if (is_result) {
			return compile_ast_computation_ref(
				ctx, item->as.block_expression.term, p_ret
			);
		}
		struct compile_ref computation;
		if (compile_ast_block_execution_ref(
				ctx, item_ast, item->as.block_expression.term, &computation
			) != 0 || computation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			computation.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION) {
			return -1;
		}
		struct compile_block_tail_context tail = {
			block_ast, block, item_index + 1, cutoff_index
		};
		struct compile_value_continuation continuation = {
			.apply = compile_block_tail_after_discard,
			.data = &tail
		};
		return compile_continue_runtime_computation(
			ctx, &computation, item_ast, &continuation, p_ret
		);
	}
	if (item->tag != PROTOTYPE_AST_BLOCK_BINDING) {
		return -1;
	}

	struct compile_ref value;
	uint32_t binder_classifier = PROTOTYPE_INVALID_ID;
	if (compile_ast_block_execution_ref(
			ctx, item_ast, item->as.block_binding.value, &value
		) != 0 || (item->as.block_binding.binder_type != PROTOTYPE_INVALID_ID &&
			compile_ast_type_expr_term(
				ctx, item->as.block_binding.binder_type, &binder_classifier
			) != 0)) {
		return -1;
	}
	if (value.category == PROTOTYPE_TERM_CATEGORY_VALUE ||
		(value.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		 value.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION &&
		 ctx->asts->nodes[item->as.block_binding.value].tag !=
			PROTOTYPE_AST_INDUCTION_HYPOTHESIS)) {
		uint32_t saved_local_ref_count = ctx->local_ref_count;
		if ((binder_classifier != PROTOTYPE_INVALID_ID && queue_ascription_check(
				ctx, value.term, binder_classifier, item_ast, value.operation
			) != 0) || push_local_ref(
				ctx,
				item->as.block_binding.ast_binder_id,
				item->as.block_binding.binder_symbol_id,
				&value
			) != 0) {
			ctx->local_ref_count = saved_local_ref_count;
			return -1;
		}
		int status;
		if (is_result) {
			status = value.category == PROTOTYPE_TERM_CATEGORY_VALUE ?
				compile_ref_make_return(ctx, &value, item_ast, p_ret) :
				(*p_ret = value, 0);
		} else {
			status = compile_ast_block_items_ref(
				ctx, block_ast, block, item_index + 1, cutoff_index, p_ret
			);
		}
		ctx->local_ref_count = saved_local_ref_count;
		return status;
	}
	if (value.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return -1;
	}

	struct compile_block_tail_context tail = {
		block_ast, block, item_index + 1, cutoff_index
	};
	struct compile_value_continuation continuation = {
		is_result ? compile_return_continuation :
			compile_block_tail_after_discard,
		is_result ? (void*)&item_ast : (void*)&tail,
		1,
		item->as.block_binding.ast_binder_id,
		item->as.block_binding.binder_symbol_id,
		binder_classifier
	};
	return compile_continue_runtime_computation(
		ctx, &value, item_ast, &continuation, p_ret
	);
}

struct compile_control_context {
	const struct compile_value_continuation* normal_continuation;
	const struct compile_value_continuation* exit_continuation;
};

static int compile_control_apply_value(
	struct compile_context* ctx,
	const struct compile_ref* value,
	const struct compile_value_continuation* continuation,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	if (!ctx || !value || !p_ret ||
		value->category != PROTOTYPE_TERM_CATEGORY_VALUE) {
		return -1;
	}
	return continuation ? continuation->apply(
		ctx, value, continuation->data, p_ret
	) : compile_ref_make_return(ctx, value, source_ast, p_ret);
}

static int compile_ast_block_items_control_ref(
	struct compile_context* ctx,
	uint32_t block_ast,
	const struct prototype_ast_node* block,
	uint32_t item_index,
	uint32_t cutoff_index,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
);

struct compile_control_block_tail_context {
	uint32_t block_ast;
	const struct prototype_ast_node* block;
	uint32_t next_item_index;
	uint32_t cutoff_index;
	const struct compile_value_continuation* normal_continuation;
	const struct compile_value_continuation* exit_continuation;
};

static int compile_control_block_tail_after_discard(
	struct compile_context* ctx,
	const struct compile_ref* ignored,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_control_block_tail_context* tail = data;
	(void)ignored;
	if (!ctx || !tail || !p_ret) {
		return -1;
	}
	return compile_ast_block_items_control_ref(
		ctx,
		tail->block_ast,
		tail->block,
		tail->next_item_index,
		tail->cutoff_index,
		tail->normal_continuation,
		tail->exit_continuation,
		p_ret
	);
}

static int compile_match_branch_body_control(
	struct compile_context* ctx,
	uint32_t body_ast,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_control_context* control = data;
	if (!control) {
		return -1;
	}
	return compile_ast_computation_control_ref(
		ctx,
		body_ast,
		control->normal_continuation,
		control->exit_continuation,
		p_ret
	);
}

struct compile_control_match_scrutinee_context {
	const struct prototype_ast_node* node;
	uint32_t source_ast;
	struct compile_control_context control;
};

static int compile_control_match_scrutinee_continuation(
	struct compile_context* ctx,
	const struct compile_ref* scrutinee,
	void* data,
	struct compile_ref* p_ret
) {
	struct compile_control_match_scrutinee_context* context = data;
	if (!ctx || !scrutinee || !context || !p_ret) {
		return -1;
	}
	return compile_ast_match_from_value_with_branch_compiler(
		ctx,
		context->source_ast,
		context->node,
		scrutinee,
		compile_match_branch_body_control,
		&context->control,
		p_ret
	);
}

static int compile_ast_match_control_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct prototype_ast_node* node,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
) {
	if (!ctx || !node || !p_ret || node->tag != PROTOTYPE_AST_MATCH ||
		ast_lambda_exit_presence_in(ctx->asts, node->as.match.scrutinee) !=
			AST_LAMBDA_EXIT_NONE) {
		return -1;
	}
	struct compile_control_match_scrutinee_context context = {
		node,
		ast_id,
		{ normal_continuation, exit_continuation }
	};
	struct compile_value_continuation continuation = {
		.apply = compile_control_match_scrutinee_continuation,
		.data = &context
	};
	return compile_ast_runtime_value_then(
		ctx, node->as.match.scrutinee, &continuation, p_ret
	);
}

struct compile_control_binding_tail_context {
	uint32_t block_ast;
	const struct prototype_ast_node* block;
	uint32_t next_item_index;
	uint32_t cutoff_index;
	int is_result;
	uint32_t source_ast;
	const struct compile_value_continuation* normal_continuation;
	const struct compile_value_continuation* exit_continuation;
};

static int compile_control_binding_tail(
	struct compile_context* ctx,
	const struct compile_ref* value,
	void* data,
	struct compile_ref* p_ret
) {
	const struct compile_control_binding_tail_context* tail = data;
	if (!ctx || !value || !tail || !p_ret) {
		return -1;
	}
	if (tail->is_result) {
		return compile_control_apply_value(
			ctx, value, tail->normal_continuation, tail->source_ast, p_ret
		);
	}
	return compile_ast_block_items_control_ref(
		ctx,
		tail->block_ast,
		tail->block,
		tail->next_item_index,
		tail->cutoff_index,
		tail->normal_continuation,
		tail->exit_continuation,
		p_ret
	);
}

static int compile_ast_block_items_control_ref(
	struct compile_context* ctx,
	uint32_t block_ast,
	const struct prototype_ast_node* block,
	uint32_t item_index,
	uint32_t cutoff_index,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
) {
	if (!ctx || !block || !p_ret || block->tag != PROTOTYPE_AST_COMPUTATION_BLOCK ||
		item_index > cutoff_index || cutoff_index >= block->as.block.item_count ||
		(size_t)block->as.block.first_item + block->as.block.item_count >
			ctx->asts->block_item_count) {
		return -1;
	}
	uint32_t item_ast = ctx->asts->block_items[
		block->as.block.first_item + item_index
	];
	if (item_ast >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* item = &ctx->asts->nodes[item_ast];
	int is_result = item_index == cutoff_index;
	if (item->tag == PROTOTYPE_AST_BLOCK_LAMBDA_EXIT) {
		struct compile_ref value;
		if (compile_ast_value_ref(
				ctx, item->as.block_lambda_exit.value, &value
			) != 0) {
			return -1;
		}
		return compile_control_apply_value(
			ctx, &value, exit_continuation, item_ast, p_ret
		);
	}
	if (item->tag == PROTOTYPE_AST_BLOCK_EXPRESSION) {
		if (is_result) {
			return compile_ast_computation_control_ref(
				ctx,
				item->as.block_expression.term,
				normal_continuation,
				exit_continuation,
				p_ret
			);
		}
		struct compile_control_block_tail_context tail = {
			block_ast,
			block,
			item_index + 1,
			cutoff_index,
			normal_continuation,
			exit_continuation
		};
		struct compile_value_continuation continuation = {
			.apply = compile_control_block_tail_after_discard,
			.data = &tail
		};
		if (ast_lambda_exit_presence_in(
				ctx->asts, item->as.block_expression.term
			) == AST_LAMBDA_EXIT_NONE) {
			struct compile_ref computation;
			if (compile_ast_block_execution_ref(
					ctx, item_ast, item->as.block_expression.term, &computation
				) != 0 || computation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				computation.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION) {
				return -1;
			}
			return compile_continue_runtime_computation(
				ctx, &computation, item_ast, &continuation, p_ret
			);
		}
		return compile_ast_computation_control_ref(
			ctx,
			item->as.block_expression.term,
			&continuation,
			exit_continuation,
			p_ret
		);
	}
	if (item->tag != PROTOTYPE_AST_BLOCK_BINDING) {
		return -1;
	}

	struct compile_ref value;
	compile_ref_clear(&value);
	uint32_t binder_classifier = PROTOTYPE_INVALID_ID;
	int value_exit_presence = ast_lambda_exit_presence_in(
		ctx->asts, item->as.block_binding.value
	);
	if (value_exit_presence == AST_LAMBDA_EXIT_FORBIDDEN ||
		(item->as.block_binding.binder_type != PROTOTYPE_INVALID_ID &&
			compile_ast_type_expr_term(
				ctx, item->as.block_binding.binder_type, &binder_classifier
			) != 0)) {
		return -1;
	}
	if (value_exit_presence == AST_LAMBDA_EXIT_NONE &&
		compile_ast_block_execution_ref(
			ctx,
			item_ast,
			item->as.block_binding.value,
			&value
		) != 0) {
		return -1;
	}
	if (value_exit_presence == AST_LAMBDA_EXIT_NONE &&
		(value.category == PROTOTYPE_TERM_CATEGORY_VALUE ||
		(value.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		 value.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION &&
		 ctx->asts->nodes[item->as.block_binding.value].tag !=
			 PROTOTYPE_AST_INDUCTION_HYPOTHESIS))) {
		uint32_t saved_local_ref_count = ctx->local_ref_count;
		if ((binder_classifier != PROTOTYPE_INVALID_ID && queue_ascription_check(
				ctx, value.term, binder_classifier, item_ast, value.operation
			) != 0) || push_local_ref(
				ctx,
				item->as.block_binding.ast_binder_id,
				item->as.block_binding.binder_symbol_id,
				&value
			) != 0) {
			ctx->local_ref_count = saved_local_ref_count;
			return -1;
		}
		int status;
		if (is_result) {
			status = value.category == PROTOTYPE_TERM_CATEGORY_VALUE ?
				compile_control_apply_value(
					ctx, &value, normal_continuation, item_ast, p_ret
				) : (normal_continuation ? -1 : (*p_ret = value, 0));
		} else {
			status = compile_ast_block_items_control_ref(
				ctx,
				block_ast,
				block,
				item_index + 1,
				cutoff_index,
				normal_continuation,
				exit_continuation,
				p_ret
			);
		}
		ctx->local_ref_count = saved_local_ref_count;
		return status;
	}
	if (value_exit_presence == AST_LAMBDA_EXIT_NONE &&
		value.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return -1;
	}
	struct compile_control_binding_tail_context tail = {
		block_ast,
		block,
		item_index + 1,
		cutoff_index,
		is_result,
		item_ast,
		normal_continuation,
		exit_continuation
	};
	struct compile_value_continuation continuation = {
		compile_control_binding_tail,
		&tail,
		1,
		item->as.block_binding.ast_binder_id,
		item->as.block_binding.binder_symbol_id,
		binder_classifier
	};
	if (value_exit_presence == AST_LAMBDA_EXIT_FOUND) {
		return compile_ast_computation_control_ref(
			ctx,
			item->as.block_binding.value,
			&continuation,
			exit_continuation,
			p_ret
		);
	}
	return compile_continue_runtime_computation(
		ctx, &value, item_ast, &continuation, p_ret
	);
}

static int compile_ast_computation_control_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	const struct compile_value_continuation* normal_continuation,
	const struct compile_value_continuation* exit_continuation,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	int presence = ast_lambda_exit_presence_in(ctx->asts, ast_id);
	if (presence == AST_LAMBDA_EXIT_FORBIDDEN) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (presence == AST_LAMBDA_EXIT_FOUND) {
		if (node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK) {
			return compile_ast_block_items_control_ref(
				ctx,
				ast_id,
				node,
				0,
				node->as.block.result_item_index,
				normal_continuation,
				exit_continuation,
				p_ret
			);
		}
		if (node->tag == PROTOTYPE_AST_MATCH) {
			return compile_ast_match_control_ref(
				ctx,
				ast_id,
				node,
				normal_continuation,
				exit_continuation,
				p_ret
			);
		}
		return -1;
	}
	struct compile_ref computation;
	if (compile_ast_computation_ref(ctx, ast_id, &computation) != 0 ||
		computation.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		return -1;
	}
	if (!normal_continuation) {
		*p_ret = computation;
		return 0;
	}
	return compile_continue_runtime_computation(
		ctx, &computation, ast_id, normal_continuation, p_ret
	);
}

static int compile_ast_computation_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK) {
		if (node->as.block.item_count == 0 ||
			node->as.block.result_item_index >= node->as.block.item_count) {
			return -1;
		}
		return compile_ast_block_items_ref(
			ctx, ast_id, node, 0, node->as.block.result_item_index, p_ret
		);
	}
	if (node->tag == PROTOTYPE_AST_COMPUTATION_FOLD) {
		return compile_ast_computation_fold_ref(ctx, node, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_ASCRIPTION) {
		struct compile_ref inner;
		uint32_t surface_classifier;
		uint32_t expected_classifier;
		uint32_t ascription_operation;
		uint32_t inner_ast = node->as.ascription.term;
		if (inner_ast >= ctx->asts->node_count ||
			compile_ast_computation_ref(ctx, inner_ast, &inner) != 0 ||
			inner.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			compile_ast_ascription_classifier(
				ctx, node->as.ascription.type_expr, &surface_classifier
			) != 0 || compile_expected_classifier_for_ref(
				ctx, &inner, surface_classifier, &expected_classifier
			) != 0 ||
			operation_add(
				ctx,
				PROTOTYPE_OPERATION_ASCRIPTION,
				inner.term,
				expected_classifier,
				ast_id,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				inner.operation,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				0,
				&ascription_operation
			) != 0 ||
			queue_ascription_check(
				ctx, inner.term, expected_classifier, ast_id, inner.operation
			) != 0) {
			return -1;
		}
		inner.classifier = expected_classifier;
		inner.operation = ascription_operation;
		*p_ret = inner;
		return 0;
	}
	if (node->tag == PROTOTYPE_AST_MATCH) {
		return compile_ast_match_ref(ctx, ast_id, node, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_LAMBDA) {
		return compile_ast_lambda_computation_ref(ctx, node, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_APP) {
		if (ast_application_head_is_constructor(ctx, ast_id)) {
			struct compile_value_continuation continuation = {
				.apply = compile_return_continuation,
				.data = &ast_id
			};
			return compile_ast_constructor_spine_value_then(
				ctx, ast_id, &continuation, p_ret
			);
		}
		return compile_ast_application_computation_ref(ctx, ast_id, p_ret);
	}
	if (node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS) {
		/* An IH stands for the recursive Match computation M(rest), not for
		 * its result value.  `compile_ast_runtime_value_then` introduces a computation fold whenever
		 * a source occurrence needs that result as a value. */
		if (compile_ast_atomic_ref(ctx, ast_id, p_ret) != 0) {
			return -1;
		}
		p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
		/* The enclosing APP decides whether the inferred motive result is a Pi.
		 * Preserve the direct-function candidate until that solver constraint is
		 * available; value contexts still sequence this occurrence through COMPUTATION_FOLD. */
		p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION;
		return 0;
	}
	if (node->tag == PROTOTYPE_AST_NAME || node->tag == PROTOTYPE_AST_VAR) {
		struct compile_ref name;
		if (compile_ast_atomic_ref(ctx, ast_id, &name) != 0) {
			return -1;
		}
		if (name.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
			*p_ret = name;
			return 0;
		}
		if (name.category == PROTOTYPE_TERM_CATEGORY_VALUE &&
			(compile_ref_thunk_computation_kind(ctx, &name) !=
					PROTOTYPE_TERM_COMPUTATION_KIND_INVALID ||
			 (name.classifier < ctx->terms->term_count &&
			  ctx->terms->terms[name.classifier].tag == PROTOTYPE_TERM_THUNK_TYPE))) {
			return compile_ref_make_force(ctx, &name, ast_id, p_ret);
		}
		if (name.category == PROTOTYPE_TERM_CATEGORY_VALUE) {
			return compile_ref_make_return(ctx, &name, ast_id, p_ret);
		}
		return -1;
	}
	struct compile_value_continuation continuation = {
		.apply = compile_return_continuation,
		.data = &ast_id
	};
	return compile_ast_runtime_value_then(ctx, ast_id, &continuation, p_ret);
}

static int compile_ast_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[ast_id];
	if (ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_ASCRIPTION) {
		struct compile_ref inner;
		uint32_t expected_classifier;
		uint32_t ascription_operation;
		if (compile_ast_ascription_classifier(
					ctx, node->as.ascription.type_expr, &expected_classifier
				) != 0 ||
			compile_ast_against_surface_classifier(
					ctx, node->as.ascription.term, expected_classifier, &inner
				) != 0 ||
			operation_add(
				ctx,
				PROTOTYPE_OPERATION_ASCRIPTION,
				inner.term,
				expected_classifier,
				ast_id,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				inner.operation,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				0,
				&ascription_operation
			) != 0 ||
			queue_ascription_check(
				ctx, inner.term, expected_classifier, ast_id, inner.operation
			) != 0) {
			return -1;
		}
		*p_ret = inner;
		p_ret->classifier = expected_classifier;
		p_ret->operation = ascription_operation;
		return 0;
	}
	int status = compile_ast_value_ref(ctx, ast_id, p_ret);
	if (status <= 0) {
		return status;
	}
	return compile_ast_computation_ref(ctx, ast_id, p_ret);
}

/* Resolve the selected root definition through the definition block itself.
 * This keeps graph construction and label publication on one membership rule. */
static int compile_selected_definition(
	struct compile_context* ctx,
	const struct prototype_ast_node** p_select,
	struct prototype_ast_term_assignment_def** p_selected
) {
	if (!ctx || !p_select || !p_selected ||
		ctx->asts->root_definition_select >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* select =
		&ctx->asts->nodes[ctx->asts->root_definition_select];
	if (select->tag != PROTOTYPE_AST_DEFINITION_SELECT ||
		select->as.definition_select.definition_block >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* block = &ctx->asts->nodes[
		select->as.definition_select.definition_block
	];
	if (block->tag != PROTOTYPE_AST_DEFINITION_BLOCK ||
		(size_t)block->as.definition_block.first_assignment +
			block->as.definition_block.assignment_count >
			ctx->asts->definition_item_count) {
		return -1;
	}
	struct prototype_ast_term_assignment_def* selected = NULL;
	for (uint32_t i = 0; i < block->as.definition_block.assignment_count; ++i) {
		uint32_t assignment_id = ctx->asts->definition_items[
			block->as.definition_block.first_assignment + i
		];
		if (assignment_id >= ctx->asts->assignment_count) {
			return -1;
		}
		struct prototype_ast_term_assignment_def* candidate =
			&ctx->asts->assignments[assignment_id];
		if (candidate->name_symbol_id ==
			select->as.definition_select.name_symbol_id) {
			if (selected) {
				return -1;
			}
			selected = candidate;
		}
	}
	if (!selected) {
		return -1;
	}
	*p_select = select;
	*p_selected = selected;
	return 0;
}

static int compile_phase_build_selected_entry(struct compile_context* ctx) {
	if (ctx->asts->root_definition_select != PROTOTYPE_INVALID_ID) {
		const struct prototype_ast_node* select;
		struct prototype_ast_term_assignment_def* selected;
		if (compile_selected_definition(ctx, &select, &selected) != 0) {
			return -1;
		}
		if (!selected || !selected->compiled ||
			selected->compiled_operation >= ctx->metadata->operation_count ||
			selected->compiled_term >= ctx->terms->term_count ||
			ctx->terms->terms[selected->compiled_term].tag != PROTOTYPE_TERM_THUNK) {
			(void)add_resolve_error_at_span(
				ctx,
				PROTOTYPE_RESOLVE_ERROR_COMPILE,
				select->as.definition_select.name_symbol_id,
				-1,
				ctx->asts->root_definition_select,
				select->span
			);
			return -1;
		}
		struct compile_ref selected_ref;
		selected_ref.term = selected->compiled_term;
		selected_ref.classifier = selected->compiled_classifier;
		selected_ref.operation = selected->compiled_operation;
		selected_ref.category = PROTOTYPE_TERM_CATEGORY_VALUE;
		selected_ref.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
		struct compile_ref entry_ref;
		if (compile_ref_make_force(
				ctx,
				&selected_ref,
				ctx->asts->root_definition_select,
				&entry_ref
			) != 0) {
			return -1;
		}
		ctx->metadata->selected_entry_symbol_id = selected->name_symbol_id;
		ctx->metadata->selected_entry_term = entry_ref.term;
		ctx->metadata->selected_entry_classifier = entry_ref.classifier;
		ctx->metadata->selected_entry_operation = entry_ref.operation;
	}
	return 0;
}

static int compile_phase_build_graph(struct compile_context* ctx) {
	if (!ctx || !ctx->asts) {
		return -1;
	}

	for (uint32_t i = 0; i < (uint32_t)ctx->asts->assignment_count; ++i) {
		uint32_t term;
		const struct prototype_ast_term_assignment_def* def = &ctx->asts->assignments[i];
		const struct prototype_ast_def_open_address_entry* entry =
			lookup_def_index_entry_const(ctx->asts, def->name_symbol_id);
		if (entry && entry->assignment_count > 1) {
			continue;
		}
		ctx->binder_count = 0;
		ctx->ih_scope_count = 0;
		size_t previous_error_count = ctx->metadata ?
			ctx->metadata->resolve_error_count : 0;
		if (compile_def(ctx, &ctx->asts->assignments[i], &term) != 0) {
			if (!ctx->metadata ||
				ctx->metadata->resolve_error_count == previous_error_count) {
				(void)add_resolve_error_at_span(
					ctx,
					PROTOTYPE_RESOLVE_ERROR_COMPILE,
					def->name_symbol_id,
					-1,
					def->ast,
					def->name_span
				);
			}
			continue;
		}
	}
	return compile_phase_build_selected_entry(ctx);
}

static int constructor_telescope_field_classifier(
	struct compile_context* ctx,
	uint32_t source_context,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !p_classifier ||
		owner >= ctx->terms->term_count ||
		(previous_binder_count > 0 && !previous_binders)) {
		return -1;
	}
	uint32_t type_id;
	uint32_t arguments[64];
	uint32_t argument_count;
	if (prototype_term_type_instance_info(
			ctx->terms,
			owner,
			&type_id,
			arguments,
			&argument_count
		) != 0 ||
		type_id >= ctx->type_declarations->type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			ctx->type_declarations->constructor_count ||
		argument_count != type->parameter_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&ctx->type_declarations->constructor_declarations[
			type->first_constructor + constructor_index
		];
	if (previous_binder_count < field_index ||
		!prototype_context_get(&ctx->metadata->contexts, source_context)) {
		return -1;
	}

	uint32_t substitution;
	if (prototype_context_substitution_from_terms(
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->terms,
			ctx->type_declarations,
			source_context,
			constructor->parameter_context,
			arguments,
			argument_count,
			&substitution
		) != 0) {
		return -1;
	}
	uint32_t previous_terms[64];
	for (uint32_t i = 0; i < field_index; ++i) {
		if (prototype_term_var(
				ctx->terms, previous_binders[i].binding_id, &previous_terms[i]
			) != 0) {
			return -1;
		}
	}
	return prototype_context_telescope_entry_classifier(
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		ctx->terms,
		ctx->type_declarations,
		substitution,
		constructor->parameter_context,
		constructor->field_context,
		previous_terms,
		field_index,
		field_index,
		p_classifier
	);
}

/* Once the outer scrutinee classifier is known, an otherwise impossible
 * sibling label can still be recognized as a constructor of an earlier case
 * body. This is the typed evidence for the missing-parentheses diagnostic; it
 * does not re-associate or reinterpret the parsed AST. */
static int diagnose_nested_match_grouping(
	struct compile_context* ctx,
	const struct pending_match_resolution* resolution,
	const struct prototype_resolution_item* item,
	uint32_t outer_scrutinee_classifier
) {
	if (!ctx || !resolution || !item || item->ast >= ctx->asts->node_count ||
		ctx->asts->nodes[item->ast].tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	const struct prototype_ast_node* source_match = &ctx->asts->nodes[item->ast];
	if (item->case_index >= source_match->as.match.case_count ||
		source_match->as.match.first_case + item->case_index >=
			ctx->asts->case_count) {
		return -1;
	}
	uint32_t match_operation_id = PROTOTYPE_INVALID_ID;
	const struct prototype_operation_node* match_operation = NULL;
	for (uint32_t i = 0; i < ctx->metadata->operation_count; ++i) {
		const struct prototype_operation_node* candidate =
			&ctx->metadata->operations[i];
		if (candidate->tag == PROTOTYPE_OPERATION_MATCH &&
			candidate->source_ast == item->ast &&
			candidate->core_term == resolution->match_term) {
			match_operation_id = i;
			match_operation = candidate;
			break;
		}
	}
	if (!match_operation || item->case_index > match_operation->case_count ||
		match_operation->first_case + item->case_index >
			ctx->metadata->operation_case_count) {
		return 0;
	}
	for (uint32_t prior = item->case_index; prior > 0; --prior) {
		uint32_t case_index = prior - 1;
		const struct prototype_operation_match_case* operation_case =
			&ctx->metadata->operation_cases[
				match_operation->first_case + case_index
			];
		if (operation_case->body_operation >= ctx->metadata->operation_count) {
			return -1;
		}
		const struct prototype_operation_node* body_operation =
			&ctx->metadata->operations[operation_case->body_operation];
		uint32_t body_classifier = operation_available_classifier(body_operation);
		if (body_classifier == PROTOTYPE_INVALID_ID &&
			body_operation->tag == PROTOTYPE_OPERATION_RETURN &&
			body_operation->argument < ctx->metadata->operation_count) {
			/* Surface Match branches are computation positions. The implicit
			 * RETURN is not the nested elimination scrutinee; its value is. */
			body_classifier = operation_available_classifier(
				&ctx->metadata->operations[body_operation->argument]
			);
		}
		struct prototype_match_constructor_resolution nested_constructor;
		if (body_classifier == PROTOTYPE_INVALID_ID ||
			prototype_judgement_resolve_match_constructor(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				body_classifier,
				resolution->constructor_symbol_id,
				&nested_constructor
			) != 0) {
			continue;
		}
		const struct prototype_ast_match_case* source_case = &ctx->asts->cases[
			source_match->as.match.first_case + item->case_index
		];
		return add_compile_diagnostic(
			ctx,
			PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_GRAPH_CONSTRUCTION,
			PROTOTYPE_COMPILE_DIAGNOSTIC_NESTED_MATCH_GROUPING,
			item->ast,
			match_operation_id,
			PROTOTYPE_INVALID_ID,
			body_classifier,
			outer_scrutinee_classifier,
			source_case->span
		) == 0 ? 1 : -1;
	}
	return 0;
}

static int compile_phase_resolve_pending_match_items(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	int has_pending = 0;

	for (uint32_t i = 0; i < ctx->pending_match_resolution_count; ++i) {
		const struct pending_match_resolution* resolution =
			&ctx->pending_match_resolutions[i];
		struct prototype_match_constructor_resolution resolved;
		if (resolution->item_id >= ctx->metadata->resolution_item_count) {
			return -1;
		}
		const struct prototype_resolution_item* item =
			&ctx->metadata->resolution_items[resolution->item_id];
		if (item->state == PROTOTYPE_RESOLUTION_ITEM_RESOLVED) {
			continue;
		}
			uint32_t scrutinee_classifier;
			int selection_status = select_match_resolution_scrutinee_classifier(
					ctx,
					resolution,
					item,
					&scrutinee_classifier
				);
			if (selection_status < 0) {
				return -1;
			}
			if (selection_status > 0) {
				has_pending = 1;
				continue;
			}
		if (prototype_judgement_resolve_match_constructor(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				scrutinee_classifier,
				resolution->constructor_symbol_id,
				&resolved
			) != 0) {
			int diagnostic_status = diagnose_nested_match_grouping(
				ctx, resolution, item, scrutinee_classifier
			);
			(void)diagnostic_status;
			return -1;
		}
		for (uint32_t previous_id = 0;
			previous_id < ctx->metadata->resolution_item_count;
			++previous_id) {
			const struct prototype_resolution_item* previous =
				&ctx->metadata->resolution_items[previous_id];
			if (previous_id == resolution->item_id ||
				previous->state != PROTOTYPE_RESOLUTION_ITEM_RESOLVED ||
				previous->ast != item->ast ||
				previous->match_term != resolution->match_term ||
				previous->resolved_owner == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					previous->resolved_owner,
					resolved.constructor_owner
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				continue;
			}
			int diagnostic_status = diagnose_nested_match_grouping(
				ctx, resolution, item, scrutinee_classifier
			);
			(void)diagnostic_status;
			return -1;
		}
		if (prototype_term_resolve_match_case(
				ctx->terms,
				resolution->match_term,
				item->case_index,
				resolved.constructor_owner,
				resolved.constructor_id
			) != 0 || resolve_match_constructor_resolution_item(
				ctx,
				resolution->item_id,
				resolution->match_term,
				resolved.constructor_owner,
				resolved.constructor_id
			) != 0) {
			return -1;
		}
		const struct prototype_term* resolved_match =
			&ctx->terms->terms[resolution->match_term];
		uint32_t resolved_case_id =
			resolved_match->as.match.first_case + item->case_index;
		if (resolved_case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* resolved_case =
			&ctx->terms->cases[resolved_case_id];
		if (item->ast >= ctx->asts->node_count ||
			ctx->asts->nodes[item->ast].tag != PROTOTYPE_AST_MATCH ||
			item->case_index >= ctx->asts->nodes[item->ast].as.match.case_count) {
			return -1;
		}
		const struct prototype_ast_match_case* source_case =
			&ctx->asts->cases[
				ctx->asts->nodes[item->ast].as.match.first_case + item->case_index
			];
		if (source_case->binder_count != resolved_case->binder_count ||
			source_case->first_binder + source_case->binder_count >
				ctx->asts->case_binder_count) {
			return -1;
		}
		uint32_t case_context = PROTOTYPE_INVALID_ID;
		for (uint32_t operation_id = 0;
			operation_id < ctx->metadata->operation_count;
			++operation_id) {
			const struct prototype_operation_node* operation =
				&ctx->metadata->operations[operation_id];
			if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
				operation->core_term != resolution->match_term ||
				item->case_index >= operation->case_count ||
				operation->first_case + item->case_index >=
					ctx->metadata->operation_case_count) {
				continue;
			}
			const struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[
					operation->first_case + item->case_index
				];
			if (operation_case->body_operation >=
				ctx->metadata->operation_count) {
				return -1;
			}
			case_context = ctx->metadata->operations[
				operation_case->body_operation
			].context_id;
			break;
		}
		if (case_context == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		for (uint32_t binder_index = 0;
			binder_index < resolved_case->binder_count;
			++binder_index) {
			struct prototype_case_binder* binder =
				&ctx->terms->case_binders[resolved_case->first_binder + binder_index];
			uint32_t binder_classifier = PROTOTYPE_INVALID_ID;
			if (constructor_telescope_field_classifier(
					ctx,
					case_context,
					resolved.constructor_owner,
					resolved.constructor_id,
					&ctx->terms->case_binders[resolved_case->first_binder],
					binder_index,
					binder_index,
					&binder_classifier
				) != 0) {
				return -1;
			}
			if (prototype_judgement_classifier_is_strictly_positive_recursive_field(
					ctx->terms,
					ctx->type_declarations,
					binder_classifier,
					scrutinee_classifier,
					&binder->is_recursive
				) != 0) {
				return -1;
			}
			uint32_t binder_var;
			if (prototype_term_var(
					ctx->terms, binder->binding_id, &binder_var
				) != 0 || queue_binder_assumption(
					ctx,
					case_context,
					binder_var,
					binder_classifier,
					PROTOTYPE_INVALID_ID
				) != 0) {
				return -1;
			}
			for (uint32_t operation_id = 0;
				operation_id < ctx->metadata->operation_count;
				++operation_id) {
				struct prototype_operation_node* operation =
					&ctx->metadata->operations[operation_id];
				if (operation->tag == PROTOTYPE_OPERATION_VAR &&
					operation->referenced_ast_binder_id ==
						ctx->asts->case_binders[
							source_case->first_binder + binder_index
						].ast_binder_id) {
					operation->known_classifier = binder_classifier;
				}
			}
		}
		for (uint32_t operation_id = 0;
			operation_id < ctx->metadata->operation_count;
			++operation_id) {
			struct prototype_operation_node* operation =
				&ctx->metadata->operations[operation_id];
			if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
				operation->core_term != resolution->match_term ||
				item->case_index >= operation->case_count ||
				operation->first_case + item->case_index >=
					ctx->metadata->operation_case_count) {
				continue;
			}
			struct prototype_operation_match_case* operation_case =
				&ctx->metadata->operation_cases[
					operation->first_case + item->case_index
				];
			operation_case->constructor_owner = resolved.constructor_owner;
			operation_case->constructor_id = resolved.constructor_id;
		}
	}
	return has_pending ? 1 : 0;
}

static int operation_solver_add_classifier_goal(
	struct compile_context* ctx,
	int kind,
	uint32_t classifier_variable,
	union operation_classifier_goal_payload payload
) {
	if (!ctx || !ctx->metadata ||
		classifier_variable >= ctx->metadata->operation_count ||
		ctx->classifier_solver.constraint_count >= PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t id = ctx->classifier_solver.constraint_count++;
	ctx->classifier_solver.constraints[id] =
		(struct operation_classifier_constraint){
			.id = id,
			.kind = kind,
			.source_operation = classifier_variable,
			.source_ast = ctx->metadata->operations[classifier_variable].source_ast,
			.context_id = ctx->metadata->operations[classifier_variable].context_id,
			.classifier_variable = classifier_variable,
			.payload = payload
		};
	ctx->classifier_solver.constraint_results[id] =
		(struct operation_classifier_constraint_result){
			.state = OPERATION_CONSTRAINT_STATE_PENDING,
			.reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE
		};
	return 0;
}

static int operation_solver_add_motive_case_goal(
	struct compile_context* ctx,
	uint32_t match_operation,
	uint32_t case_index,
	uint32_t body_operation,
	uint32_t scrutinee_operation
) {
	if (!ctx || !ctx->metadata ||
		match_operation >= ctx->metadata->operation_count ||
		body_operation >= ctx->metadata->operation_count ||
		scrutinee_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[match_operation];
	if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
		operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
		case_index >= operation->case_count ||
		operation->first_case + case_index >=
			ctx->metadata->operation_case_count) {
		return -1;
	}
	const struct prototype_term* match = &ctx->terms->terms[operation->core_term];
	if (operation->source_ast >= ctx->asts->node_count ||
		ctx->asts->nodes[operation->source_ast].tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	const struct prototype_ast_node* source_match =
		&ctx->asts->nodes[operation->source_ast];
	if (case_index >= source_match->as.match.case_count ||
		source_match->as.match.first_case + case_index >= ctx->asts->case_count) {
		return -1;
	}
	uint32_t term_case_id = match->as.match.first_case + case_index;
	if (term_case_id >= ctx->terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* match_case = &ctx->terms->cases[term_case_id];
	const struct prototype_ast_match_case* ast_match_case = &ctx->asts->cases[
		source_match->as.match.first_case + case_index
	];
	if (match_case->first_binder + match_case->binder_count >
		ctx->terms->case_binder_count ||
		ast_match_case->first_binder + ast_match_case->binder_count >
			ctx->asts->case_binder_count ||
		match_case->binder_count != ast_match_case->binder_count) {
		return -1;
	}
	union operation_classifier_goal_payload payload = { 0 };
	payload.motive_case = (struct operation_classifier_motive_case_goal){
		.branch_operation = body_operation,
		.scrutinee_operation = scrutinee_operation,
		.case_index = case_index,
		.constructor_owner = match_case->constructor_owner,
		.constructor_index = match_case->constructor_id,
		.ih_scope_id = match->as.match.ih_scope_id
	};
	return operation_solver_add_classifier_goal(
		ctx, OPERATION_CONSTRAINT_MOTIVE_EQUATION, match_operation, payload
	);
}

static const struct operation_classifier_constraint* operation_solver_motive_equation(
	const struct operation_classifier_solver* solver,
	uint32_t match_operation,
	uint32_t case_index
) {
	if (!solver) {
		return NULL;
	}
	for (uint32_t i = 0; i < solver->constraint_count; ++i) {
		const struct operation_classifier_constraint* goal = &solver->constraints[i];
		if (goal->kind == OPERATION_CONSTRAINT_MOTIVE_EQUATION &&
			goal->classifier_variable == match_operation &&
			goal->payload.motive_case.case_index == case_index) {
			return goal;
		}
	}
	return NULL;
}

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct operation_classifier_constraint* equation,
	uint32_t classifier
);

static uint32_t operation_solver_classifier(
	const struct compile_context* ctx,
	uint32_t operation
);

static int operation_solver_enqueue_constraint(
	struct compile_context* ctx,
	uint32_t constraint_id
) {
	if (!ctx || constraint_id >= ctx->classifier_solver.constraint_count) {
		return -1;
	}
	if (ctx->classifier_solver.constraint_queued[constraint_id]) {
		return 0;
	}
	if (ctx->classifier_solver.worklist_count >=
		PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t tail = (
		ctx->classifier_solver.worklist_head +
		ctx->classifier_solver.worklist_count
	) % PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY;
	ctx->classifier_solver.worklist[tail] = constraint_id;
	ctx->classifier_solver.constraint_queued[constraint_id] = 1;
	ctx->classifier_solver.worklist_count++;
	return 0;
}

static int operation_solver_enqueue_dependents(
	struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || operation >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t dependency =
		ctx->classifier_solver.first_dependent_constraint[operation];
	while (dependency != PROTOTYPE_INVALID_ID) {
		if (dependency >= ctx->classifier_solver.dependent_constraint_count ||
			operation_solver_enqueue_constraint(
				ctx,
				ctx->classifier_solver.dependent_constraints[dependency].constraint
			) != 0) {
			return -1;
		}
		dependency = ctx->classifier_solver.dependent_constraints[dependency].next;
	}
	return 0;
}

static int operation_solver_enqueue_clause_fold_dependents(
	struct compile_context* ctx,
	uint32_t clause_body_operation
) {
	if (!ctx || !ctx->metadata ||
		clause_body_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->operation_count;
		++operation_id) {
		const struct prototype_operation_node* fold =
			&ctx->metadata->operations[operation_id];
		if (fold->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			fold->fold_clause_count == 0 ||
			(size_t)fold->first_fold_clause + fold->fold_clause_count >
				ctx->metadata->operation_fold_clause_count) {
			continue;
		}
		int uses_clause_body = 0;
		for (uint32_t clause = 0; clause < fold->fold_clause_count; ++clause) {
			if (ctx->metadata->operation_fold_clauses[
					fold->first_fold_clause + clause
				].body_operation == clause_body_operation) {
				uses_clause_body = 1;
				break;
			}
		}
		if (!uses_clause_body) {
			continue;
		}
		for (uint32_t constraint_id = 0;
			constraint_id < ctx->classifier_solver.constraint_count;
			++constraint_id) {
			const struct operation_classifier_constraint* constraint =
				&ctx->classifier_solver.constraints[constraint_id];
			if (constraint->kind ==
					OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT &&
				constraint->classifier_variable == operation_id &&
				operation_solver_enqueue_constraint(ctx, constraint_id) != 0) {
				return -1;
			}
		}
	}
	return 0;
}

static int operation_solver_pop_constraint(
	struct compile_context* ctx,
	uint32_t* p_constraint
) {
	if (!ctx || !p_constraint) {
		return -1;
	}
	if (ctx->classifier_solver.worklist_count == 0) {
		return 1;
	}
	*p_constraint =
		ctx->classifier_solver.worklist[ctx->classifier_solver.worklist_head];
	ctx->classifier_solver.worklist_head =
		(ctx->classifier_solver.worklist_head + 1) %
			PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY;
	ctx->classifier_solver.worklist_count--;
	ctx->classifier_solver.constraint_queued[*p_constraint] = 0;
	return 0;
}

static void operation_solver_store_classifier_solution(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier
) {
	struct operation_solver_solution* solution =
		&ctx->classifier_solver.solutions[operation];
	solution->classifier = classifier;
	solution->classifier_state = OPERATION_CONSTRAINT_STATE_SOLVED;
	solution->reason = OPERATION_CLASSIFIER_GOAL_REASON_HAS_TYPE_VALIDATED;
	struct prototype_term_classifier_view view;
	if (prototype_judgement_classifier_view(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			classifier,
			&view
		) == 0 && view.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		view.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
		solution->effect_row = view.effect_row;
	}
}

static int operation_solver_bind(
	struct compile_context* ctx,
	uint32_t variable,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		variable >= ctx->metadata->operation_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.solutions[variable].classifier;
	if (previous == PROTOTYPE_INVALID_ID) {
		operation_solver_store_classifier_solution(ctx, variable, classifier);
		*p_changed = 1;
		return operation_solver_enqueue_dependents(ctx, variable) != 0 ||
			operation_solver_enqueue_clause_fold_dependents(ctx, variable) != 0 ?
				-1 : 0;
	}
				if (!(prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return -1;
	}
	return 0;
}

/* Merge two returning-computation classifier equations into one monotone
 * solver representative. Every row atom is retained: in particular, a
 * quantified row captured from a higher-order argument is not an elaboration
 * metavariable and a pure equation cannot discharge it. This is solver-local
 * refinement and does not make effect-row union a global Term reduction. */
static int operation_solver_merge_computation_classifiers(
	struct compile_context* ctx,
	uint32_t previous,
	uint32_t candidate,
	uint32_t* p_merged
) {
	if (!ctx || !p_merged || previous >= ctx->terms->term_count ||
		candidate >= ctx->terms->term_count) {
		return -1;
	}
	struct prototype_term_classifier_view before;
	struct prototype_term_classifier_view after;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, previous, &before
		) != 0 || prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, candidate, &after
		) != 0 || before.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		after.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		before.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		after.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, before.result, after.result
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	struct prototype_effect_row_normal_form before_row;
	struct prototype_effect_row_normal_form after_row;
	if (prototype_term_effect_row_normal_form(
			ctx->terms, before.effect_row, &before_row
		) != 0 || prototype_term_effect_row_normal_form(
			ctx->terms, after.effect_row, &after_row
		) != 0) {
		return 1;
	}
	struct prototype_effect_row_normal_form merged;
	memset(&merged, 0, sizeof(merged));
	struct prototype_effect_row_normal_form* rows[2] = {
		&before_row, &after_row
	};
	for (uint32_t row_index = 0; row_index < 2; ++row_index) {
		for (uint32_t i = 0; i < rows[row_index]->atom_count; ++i) {
			uint32_t candidate_atom = rows[row_index]->atoms[i];
			int found = 0;
			for (uint32_t j = 0; j < merged.atom_count; ++j) {
				if (merged.atoms[j] == candidate_atom) {
					found = 1;
					break;
				}
			}
			if (!found) {
				if (merged.atom_count >=
					PROTOTYPE_EFFECT_ROW_NORMAL_FORM_ATOM_CAPACITY) {
					return -1;
				}
				merged.atoms[merged.atom_count++] = candidate_atom;
			}
		}
	}
	uint32_t row;
	return prototype_term_effect_row_materialize_normal_form(
			ctx->terms, &merged, &row
		) != 0 ? -1 : prototype_term_computation_type(
			ctx->terms, row, before.result, p_merged
		);
}

static int operation_solver_bind_proven_classifier(
	struct compile_context* ctx,
	uint32_t variable,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		variable >= ctx->metadata->operation_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	if (ctx->classifier_solver.solutions[variable].classifier == classifier) {
		ctx->metadata->operations[variable].known_classifier = classifier;
		return 0;
	}
	uint32_t previous = ctx->classifier_solver.solutions[variable].classifier;
	if (previous != PROTOTYPE_INVALID_ID &&
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* The fixed-point cell already denotes this classifier. Preserve its
		 * representative so alpha-equivalent reconstruction cannot requeue the
		 * same dependency cycle indefinitely. */
		ctx->metadata->operations[variable].known_classifier = previous;
		return 0;
	}
	/* The OperationGraph seed is an elaboration approximation.  Once the
	 * kernel has validated a rule-specific proof, that occurrence classifier
	 * becomes authoritative even when it is not convertible to the seed. */
	operation_solver_store_classifier_solution(ctx, variable, classifier);
	ctx->metadata->operations[variable].known_classifier = classifier;
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, variable) != 0 ||
		operation_solver_enqueue_clause_fold_dependents(ctx, variable) != 0 ?
			-1 : 0;
}

static int operation_solver_widen_computation_binding(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !p_changed || operation >= ctx->metadata->operation_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.solutions[operation].classifier;
	if (previous == PROTOTYPE_INVALID_ID) {
		return operation_solver_bind(ctx, operation, classifier, p_changed);
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return -1;
	}
	uint32_t merged;
	if (operation_solver_merge_computation_classifiers(
			ctx, previous, classifier, &merged
		) != 0) {
		return -1;
	}
	if (merged == previous || prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, merged
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	operation_solver_store_classifier_solution(ctx, operation, merged);
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, operation) != 0 ||
		operation_solver_enqueue_clause_fold_dependents(ctx, operation) != 0 ?
			-1 : 0;
}

static int operation_solver_continuation_effect_widening(
	struct compile_context* ctx,
	uint32_t before,
	uint32_t after
) {
	if (!ctx || before >= ctx->terms->term_count ||
		after >= ctx->terms->term_count ||
		ctx->terms->terms[before].tag != PROTOTYPE_TERM_THUNK_TYPE ||
		ctx->terms->terms[after].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 0;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, before, after
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	uint32_t before_domain;
	uint32_t before_family;
	uint32_t before_family_binder;
	uint32_t before_codomain;
	uint32_t after_domain;
	uint32_t after_family;
	uint32_t after_family_binder;
	uint32_t after_codomain;
	if (prototype_judgement_pi_parts(
			ctx->terms,
			ctx->terms->terms[before].as.thunk_type.computation,
			&before_domain,
			&before_family
		) != 0 || prototype_judgement_pi_parts(
			ctx->terms,
			ctx->terms->terms[after].as.thunk_type.computation,
			&after_domain,
			&after_family
		) != 0 || prototype_term_pure_family_parts(
			ctx->terms, before_family, &before_family_binder, &before_codomain
		) != 0 || prototype_term_pure_family_parts(
			ctx->terms, after_family, &after_family_binder, &after_codomain
		) != 0 || !(prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, before_domain, after_domain
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 0;
	}
	(void)before_family_binder;
	(void)after_family_binder;
	struct prototype_term_classifier_view before_view;
	struct prototype_term_classifier_view after_view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, before_codomain, &before_view
		) != 0 || prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, after_codomain, &after_view
		) != 0 || before_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		after_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		before_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		after_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		!(prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
				before_view.result,
				after_view.result
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		return 0;
	}
	struct prototype_effect_row_normal_form before_row;
	struct prototype_effect_row_normal_form after_row;
	if (prototype_term_effect_row_normal_form(
			ctx->terms, before_view.effect_row, &before_row
		) != 0 || prototype_term_effect_row_normal_form(
			ctx->terms, after_view.effect_row, &after_row
		) != 0) {
		return 0;
	}
	/* A lone row variable is the unspecialized continuation approximation
	 * introduced by this solver. It may be refined to either a closed row or a
	 * row expression that retains the variable. This is metavariable solving,
	 * not ordinary set inclusion. */
	if (before_row.atom_count == 1 && before_row.atoms[0] < ctx->terms->term_count &&
		ctx->terms->terms[before_row.atoms[0]].tag ==
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		return 1;
	}
	return prototype_term_effect_row_normal_form_includes(
		&after_row, &before_row
	);
}

static int operation_solver_widen_continuation_binding(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !p_changed || operation >= ctx->metadata->operation_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.solutions[operation].classifier;
	if (previous == PROTOTYPE_INVALID_ID) {
		return operation_solver_bind(ctx, operation, classifier, p_changed);
	}
	struct prototype_term_conversion_result conversion =
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		);
	if (conversion.status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	if (conversion.status != PROTOTYPE_TERM_CONVERSION_NOT_EQUAL) {
		return -1;
	}
	if (!operation_solver_continuation_effect_widening(
			ctx, previous, classifier
		)) {
		return -1;
	}
	operation_solver_store_classifier_solution(ctx, operation, classifier);
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, operation);
}

static uint32_t operation_solver_classifier(
	const struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || !ctx->metadata || operation >= ctx->metadata->operation_count) {
		return PROTOTYPE_INVALID_ID;
	}
	if (ctx->classifier_solver.solutions[operation].classifier !=
		PROTOTYPE_INVALID_ID) {
		return ctx->classifier_solver.solutions[operation].classifier;
	}
	uint32_t application_id =
		ctx->classifier_solver.ih_motive_application_ids[operation];
	if (application_id == PROTOTYPE_INVALID_ID ||
		application_id >= ctx->classifier_solver.motive_application_count) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t motive_operation = ctx->classifier_solver.motive_applications[
		application_id
	].motive_operation;
	if (motive_operation >= ctx->metadata->operation_count) {
		return PROTOTYPE_INVALID_ID;
	}
	/* A constant candidate is a solver-side equation M(_) == T, so it can
	 * discharge the classifier query without pretending that M(argument) has
	 * already been built in TermDB. */
	return ctx->classifier_solver.motive_constant_candidates[motive_operation];
}

/* Solve a constructor APP from its dependent telescope. The curried cache is
 * used only to describe a residual, under-applied builder; a saturated value's
 * classifier is reindexed directly from the authoritative result schema. */
enum operation_solver_constructor_spine_status {
	OPERATION_SOLVER_CONSTRUCTOR_SPINE_SOLVED = 0,
	OPERATION_SOLVER_CONSTRUCTOR_SPINE_WAITING = 1,
	OPERATION_SOLVER_CONSTRUCTOR_SPINE_CONTRADICTION = 2
};

static int operation_solver_refine_sequenced_ih_result(
	struct compile_context* ctx,
	uint32_t value_operation,
	uint32_t expected_result,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		value_operation >= ctx->metadata->operation_count ||
		expected_result >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_operation_node* value =
		&ctx->metadata->operations[value_operation];
	if (value->tag != PROTOTYPE_OPERATION_VAR ||
		value->referenced_ast_binder_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	for (uint32_t fold_id = 0; fold_id < ctx->metadata->operation_count; ++fold_id) {
		const struct prototype_operation_node* fold =
			&ctx->metadata->operations[fold_id];
		if (fold->tag != PROTOTYPE_OPERATION_COMPUTATION_FOLD ||
			fold->function >= ctx->metadata->operation_count ||
			fold->argument >= ctx->metadata->operation_count ||
			fold->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[fold->core_term].tag !=
				PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[fold->core_term].as.computation_fold.clause_count != 0) {
			continue;
		}
		const struct prototype_operation_node* continuation =
			&ctx->metadata->operations[fold->argument];
		const struct prototype_operation_node* input =
			&ctx->metadata->operations[fold->function];
		if (continuation->tag != PROTOTYPE_OPERATION_LAMBDA ||
			continuation->referenced_ast_binder_id !=
				value->referenced_ast_binder_id ||
			input->tag != PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS ||
			input->scrutinee >= ctx->metadata->operation_count) {
			continue;
		}
		uint32_t input_classifier = operation_solver_classifier(ctx, fold->function);
		struct prototype_term_classifier_view input_view;
		if (input_classifier == PROTOTYPE_INVALID_ID) {
			uint32_t row;
			uint32_t seeded_classifier;
			/* Materializing an induction hypothesis constructs a recursive
			 * function value; it does not execute that function's latent effects. */
			if (prototype_term_effect_row_empty(ctx->terms, &row) != 0 ||
				prototype_term_computation_type(
					ctx->terms, row, expected_result, &seeded_classifier
				) != 0) {
				return -1;
			}
			operation_solver_store_classifier_solution(
				ctx, fold->function, seeded_classifier
			);
			operation_solver_store_classifier_solution(
				ctx, value_operation, expected_result
			);
			ctx->metadata->operations[fold->argument].binder_classifier =
				expected_result;
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, fold->function) != 0 ||
				operation_solver_enqueue_dependents(ctx, fold_id) != 0) {
				return -1;
			}
			return 0;
		}
		if (prototype_judgement_classifier_view(
				ctx->terms, ctx->type_declarations, NULL,
				input_classifier, &input_view
			) != 0 || input_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
			input_view.computation_kind !=
				PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
			return 0;
		}
		if (prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				input_view.result,
				expected_result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			uint32_t owner_match = input->scrutinee;
			if (owner_match >= ctx->metadata->operation_count ||
				ctx->metadata->operations[owner_match].tag !=
					PROTOTYPE_OPERATION_MATCH) {
				return -1;
			}
			if (ctx->classifier_solver.motive_solution_states[owner_match] ==
					OPERATION_MOTIVE_SOLUTION_MATERIALIZED) {
				ctx->classifier_solver.motive_constant_candidates[owner_match] =
					PROTOTYPE_INVALID_ID;
				ctx->classifier_solver.motive_terms[owner_match] =
					PROTOTYPE_INVALID_ID;
				ctx->classifier_solver.motive_solution_states[owner_match] =
					OPERATION_MOTIVE_SOLUTION_READY;
				ctx->classifier_solver.solutions[owner_match].classifier =
					PROTOTYPE_INVALID_ID;
				ctx->metadata->operations[owner_match].classifier =
					PROTOTYPE_INVALID_ID;
				*p_changed = 1;
				return 0;
			}
			return 1;
		}
		uint32_t refined_classifier;
		if (prototype_term_computation_type(
				ctx->terms, input_view.effect_row, expected_result,
				&refined_classifier
			) != 0) {
			return -1;
		}
		uint32_t owner_match = input->scrutinee;
		if (owner_match >= ctx->metadata->operation_count ||
			ctx->metadata->operations[owner_match].tag !=
				PROTOTYPE_OPERATION_MATCH) {
			return -1;
		}
		/* The constructor telescope supplies one missing equation: the recursive
		 * result must have the expected indexed field type.  Record only that
		 * equation here.  The ordinary Match solver must still construct M from
		 * every branch, otherwise a lambda inferred from this one occurrence would
		 * incorrectly impose the recursive index on non-recursive branches. */
		operation_solver_store_classifier_solution(
			ctx, value_operation, expected_result
		);
		ctx->metadata->operations[fold->argument].binder_classifier = expected_result;
		if (ctx->classifier_solver.solutions[fold->function].classifier !=
				refined_classifier) {
			operation_solver_store_classifier_solution(
				ctx, fold->function, refined_classifier
			);
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(ctx, fold->function) != 0 ||
				operation_solver_enqueue_dependents(ctx, fold_id) != 0 ||
				operation_solver_enqueue_dependents(ctx, owner_match) != 0) {
				return -1;
			}
		}
		return 0;
	}
	return 1;
}

static int operation_solver_propagate_constructor_domains(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	uint32_t reverse_arguments[64];
	uint32_t argument_count = 0;
	uint32_t cursor = operation_id;
	while (cursor < ctx->metadata->operation_count) {
		const struct prototype_operation_node* operation =
			&ctx->metadata->operations[cursor];
		if (operation->tag != PROTOTYPE_OPERATION_APP ||
			operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			break;
		}
		if (argument_count >= 64 ||
			operation->argument >= ctx->metadata->operation_count) {
			return -1;
		}
		reverse_arguments[argument_count++] = operation->argument;
		cursor = operation->function;
	}
	uint32_t expected_domains[64];
	uint32_t expected_domain_count;
	if (prototype_judgement_constructor_spine_expected_domains(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->metadata->operations[operation_id].context_id,
			ctx->metadata->operations[operation_id].core_term,
			PROTOTYPE_INVALID_ID,
			expected_domains,
			64,
			&expected_domain_count
		) != 0 || expected_domain_count != argument_count) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		uint32_t argument_operation = reverse_arguments[argument_count - i - 1];
		uint32_t expected = expected_domains[i];
		uint32_t actual = operation_solver_classifier(ctx, argument_operation);
		if (ctx->materializing_checked_classifiers &&
			actual < ctx->terms->term_count) {
			int requires_index_refinement = 0;
			uint32_t inspect = argument_operation;
			for (uint32_t depth = 0; depth < 128 &&
				inspect < ctx->metadata->operation_count; ++depth) {
				const struct prototype_operation_node* node =
					&ctx->metadata->operations[inspect];
				if (node->tag == PROTOTYPE_OPERATION_MATCH) {
					int all_impossible = node->case_count != 0;
					for (uint32_t case_index = 0;
						case_index < node->case_count; ++case_index) {
						const struct prototype_operation_match_case* operation_case =
							&ctx->metadata->operation_cases[
								node->first_case + case_index
							];
						const struct prototype_term* match =
							&ctx->terms->terms[node->core_term];
						const struct prototype_match_case* term_case =
							&ctx->terms->cases[
								match->as.match.first_case + case_index
							];
						uint32_t ignored_context;
						uint32_t ignored_substitution;
						uint32_t ignored_constructor;
						if (prototype_judgement_source_indexed_branch_refinement(
								&ctx->metadata->contexts,
								&ctx->metadata->substitutions,
								ctx->terms, ctx->type_declarations,
								ctx->metadata->operations[node->scrutinee].context_id,
								ctx->metadata->operations[node->scrutinee].core_term,
								ctx->metadata->operations[node->scrutinee].classifier,
								operation_case->constructor_id,
								operation_case->context_id,
								&ctx->terms->case_binders[term_case->first_binder],
								term_case->binder_count, &ignored_context,
								&ignored_substitution, &ignored_constructor
							) != PROTOTYPE_INDEX_REFINEMENT_IMPOSSIBLE) {
							all_impossible = 0;
							break;
						}
					}
					requires_index_refinement = all_impossible;
					break;
				}
				if (node->tag == PROTOTYPE_OPERATION_NAME) {
					inspect = node->function;
				} else if (node->tag == PROTOTYPE_OPERATION_ASCRIPTION) {
					inspect = node->body;
				} else if (node->tag == PROTOTYPE_OPERATION_THUNK ||
					node->tag == PROTOTYPE_OPERATION_RETURN) {
					inspect = node->argument;
				} else if (node->tag == PROTOTYPE_OPERATION_LAMBDA) {
					inspect = node->body;
				} else {
					break;
				}
			}
			if (!requires_index_refinement) {
				continue;
			}
		}
		int expected_refinement = operation_refine_classifier_from_expected(
			ctx, argument_operation, expected, 0, p_changed
		);
		if (expected_refinement < 0) {
			return -1;
		}
		actual = operation_solver_classifier(ctx, argument_operation);
		int ih_refinement = operation_solver_refine_sequenced_ih_result(
			ctx, argument_operation, expected, p_changed
		);
		if (ih_refinement < 0) {
			return -1;
		}
		if (ih_refinement == 0) {
			return 0;
		}
		if (ctx->metadata->operations[argument_operation].tag ==
				PROTOTYPE_OPERATION_INDUCTION_HYPOTHESIS) {
			uint32_t owner_match =
				ctx->metadata->operations[argument_operation].scrutinee;
			if (owner_match >= ctx->metadata->operation_count ||
				ctx->metadata->operations[owner_match].tag !=
					PROTOTYPE_OPERATION_MATCH) {
				return -1;
			}
			ctx->classifier_solver.motive_constant_candidates[owner_match] =
				PROTOTYPE_INVALID_ID;
			if (actual == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, actual, expected
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				operation_solver_store_classifier_solution(
					ctx, argument_operation, expected
				);
				*p_changed = 1;
				if (operation_solver_enqueue_dependents(
						ctx, argument_operation
					) != 0 || operation_solver_enqueue_dependents(
						ctx, owner_match
					) != 0) {
					return -1;
				}
			}
			actual = expected;
		}
		if (actual == PROTOTYPE_INVALID_ID) {
			return 0;
		}
		uint32_t solved_expected;
		if (prototype_judgement_solve_expected_effect_rows(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				expected,
				actual,
				&solved_expected
			) != 0) {
			return 0;
		}
		(void)solved_expected;
	}
	(void)cursor;
	return 0;
}

static int operation_solver_constructor_spine_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !p_classifier ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	if (operation->tag != PROTOTYPE_OPERATION_APP ||
		operation->application_role !=
			PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
		return -1;
	}
	uint32_t reverse_argument_operations[64];
	uint32_t argument_operation_count = 0;
	uint32_t cursor = operation_id;
	while (cursor < ctx->metadata->operation_count) {
		const struct prototype_operation_node* cursor_operation =
			&ctx->metadata->operations[cursor];
		if (cursor_operation->tag != PROTOTYPE_OPERATION_APP ||
			cursor_operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			break;
		}
		if (argument_operation_count >= 64 ||
			cursor_operation->argument >= ctx->metadata->operation_count) {
			return -1;
		}
		reverse_argument_operations[argument_operation_count++] =
			cursor_operation->argument;
		cursor = cursor_operation->function;
	}
	uint32_t argument_classifiers[64];
	uint32_t expected_domains[64];
	uint32_t expected_domain_count;
	if (prototype_judgement_constructor_spine_expected_domains(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			operation->context_id,
			operation->core_term,
			PROTOTYPE_INVALID_ID,
			expected_domains,
			64,
			&expected_domain_count
		) != 0 || expected_domain_count != argument_operation_count) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_operation_count; ++i) {
		uint32_t argument_operation =
			reverse_argument_operations[argument_operation_count - i - 1];
		uint32_t argument_classifier =
			operation_solver_classifier(ctx, argument_operation);
		if (argument_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		int expected_check = operation_check_classifier(
			ctx, argument_operation, expected_domains[i], 0
		);
		if (expected_check < 0) {
			return -1;
		}
		argument_classifiers[i] = expected_check > 0 ?
			expected_domains[i] : argument_classifier;
	}
	int saturated;
	int status = prototype_judgement_constructor_spine_classifier(
		ctx->terms,
		ctx->type_declarations,
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		operation->context_id,
		operation->core_term,
		PROTOTYPE_INVALID_ID,
		argument_classifiers,
		argument_operation_count,
		p_classifier,
		&saturated
	);
	if (status == PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_DOMAIN_MISMATCH) {
		return OPERATION_SOLVER_CONSTRUCTOR_SPINE_CONTRADICTION;
	}
	return status;
}

static int operation_solver_join_match_classifiers(
	struct compile_context* ctx,
	uint32_t left,
	uint32_t right,
	uint32_t* p_joined
);

static int operation_solver_seed_motive(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	uint32_t source_body_operation,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation >= ctx->metadata->operation_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct operation_classifier_constraint* source_equation = NULL;
	for (uint32_t case_index = 0;
		case_index < ctx->metadata->operations[operation].case_count;
		++case_index) {
		const struct operation_classifier_constraint* equation =
			operation_solver_motive_equation(
				&ctx->classifier_solver, operation, case_index
			);
		if (!equation) {
			return -1;
		}
		if (equation->payload.motive_case.branch_operation == source_body_operation) {
			source_equation = equation;
			break;
		}
	}
	if (!source_equation) {
		return -1;
	}
	int capture_status = operation_classifier_captures_case_binder(
		ctx, source_equation, classifier
	);
	if (capture_status != 0) {
		return capture_status < 0 ? -1 : 1;
	}
	uint32_t previous =
		ctx->classifier_solver.motive_constant_candidates[operation];
	if (previous == PROTOTYPE_INVALID_ID) {
		ctx->classifier_solver.motive_constant_candidates[operation] = classifier;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(ctx, operation) != 0) {
			return -1;
		}
		uint32_t ih_scope = PROTOTYPE_INVALID_ID;
		const struct prototype_operation_node* match_operation =
			&ctx->metadata->operations[operation];
		if (match_operation->core_term < ctx->terms->term_count &&
			ctx->terms->terms[match_operation->core_term].tag ==
				PROTOTYPE_TERM_MATCH) {
			ih_scope =
				ctx->terms->terms[match_operation->core_term].as.match.ih_scope_id;
		}
		for (uint32_t constraint_id = 0;
			constraint_id < ctx->classifier_solver.constraint_count;
			++constraint_id) {
			const struct operation_classifier_constraint* constraint =
				&ctx->classifier_solver.constraints[constraint_id];
			if (constraint->kind != OPERATION_CONSTRAINT_IH_EXPECTED ||
				constraint->classifier_variable >= ctx->metadata->operation_count ||
				ctx->metadata->operations[constraint->classifier_variable].ih_scope_id !=
					ih_scope) {
				continue;
			}
			if (operation_solver_enqueue_constraint(ctx, constraint_id) != 0) {
				return -1;
			}
		}
		return 0;
	}
	uint32_t joined;
	int join_status = operation_solver_join_match_classifiers(
		ctx, previous, classifier, &joined
	);
	if (join_status != 0) {
		return join_status < 0 ? -1 : 1;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, joined
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		ctx->classifier_solver.motive_constant_candidates[operation] = joined;
		*p_changed = 1;
		if (operation_solver_enqueue_dependents(ctx, operation) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_set_ih_motive_application(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t motive_operation,
	uint32_t argument_operation,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation >= ctx->metadata->operation_count ||
		motive_operation >= ctx->metadata->operation_count ||
		argument_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	struct prototype_operation_graph graph;
	struct prototype_operation_induction_edge edge;
	prototype_compile_metadata_operation_graph_const(ctx->metadata, &graph);
	if (prototype_operation_graph_induction_edge(
			&graph, ctx->terms, operation, &edge
		) != 0 || edge.owner_match_operation != motive_operation ||
		edge.argument_operation != argument_operation) {
		return -1;
	}
	uint32_t previous =
		ctx->classifier_solver.ih_motive_application_ids[operation];
	if (previous == PROTOTYPE_INVALID_ID) {
		if (ctx->classifier_solver.motive_application_count >= 4096) {
			return -1;
		}
		ctx->classifier_solver.motive_applications[
			ctx->classifier_solver.motive_application_count
		] = (struct operation_solver_motive_application){
			motive_operation,
			argument_operation,
			edge.scope_id,
			edge.case_index,
			edge.field_index
		};
		ctx->classifier_solver.ih_motive_application_ids[operation] =
			ctx->classifier_solver.motive_application_count++;
		*p_changed = 1;
		return 0;
	}
	if (previous >= ctx->classifier_solver.motive_application_count) {
		return -1;
	}
	const struct operation_solver_motive_application* application =
		&ctx->classifier_solver.motive_applications[previous];
	return application->motive_operation == motive_operation &&
		application->argument_operation == argument_operation &&
		application->scope_frame == edge.scope_id &&
		application->case_index == edge.case_index &&
		application->field_index == edge.field_index ? 0 : -1;
}

static int operation_solver_add_input_fact(
	struct compile_context* ctx,
	uint32_t subject,
	uint32_t classifier,
	uint32_t lambda_operation,
	uint32_t ast_binder_id
) {
	if (!ctx || subject >= ctx->terms->term_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->classifier_solver.input_fact_count; ++i) {
		const struct operation_solver_input_fact* fact =
			&ctx->classifier_solver.input_facts[i];
		if (fact->subject == subject && fact->classifier == classifier &&
			fact->lambda_operation == lambda_operation &&
			fact->ast_binder_id == ast_binder_id) {
			return 0;
		}
	}
	if (ctx->classifier_solver.input_fact_count >=
		PROTOTYPE_OPERATION_SOLVER_INPUT_FACT_CAPACITY) {
		return -1;
	}
	ctx->classifier_solver.input_facts[
		ctx->classifier_solver.input_fact_count++
	] = (struct operation_solver_input_fact){
		subject, classifier, lambda_operation, ast_binder_id
	};
	return 0;
}

static int operation_solver_lambda_ast_binder(
	const struct compile_context* ctx,
	uint32_t lambda_operation,
	uint32_t* p_ast_binder_id
) {
	if (!ctx || !ctx->metadata || !p_ast_binder_id ||
		lambda_operation >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[lambda_operation];
	if (operation->tag != PROTOTYPE_OPERATION_LAMBDA) {
		return -1;
	}
	if (operation->source_ast < ctx->asts->node_count &&
		ctx->asts->nodes[operation->source_ast].tag == PROTOTYPE_AST_LAMBDA) {
		*p_ast_binder_id =
			ctx->asts->nodes[operation->source_ast].as.lambda.ast_binder_id;
		return 0;
	}
	if (operation->referenced_ast_binder_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_ast_binder_id = operation->referenced_ast_binder_id;
	return 0;
}

static int operation_solver_initialize_input_facts(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->pending_binder_assumption_count; ++i) {
		const struct pending_binder_assumption* pending =
			&ctx->pending_binder_assumptions[i];
		if (pending->source_operation == PROTOTYPE_INVALID_ID) {
			continue;
		}
		uint32_t ast_binder_id;
		if (operation_solver_lambda_ast_binder(
				ctx, pending->source_operation, &ast_binder_id
			) != 0) {
			return -1;
		}
		if (operation_solver_add_input_fact(
				ctx,
				pending->binder_var,
				pending->classifier,
				pending->source_operation,
				ast_binder_id
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->pending_declaration_fact_count; ++i) {
		const struct pending_declaration_fact* pending =
			&ctx->pending_declaration_facts[i];
		if (operation_solver_add_input_fact(
				ctx,
				pending->subject,
				pending->classifier,
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_add_constraint_dependency(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t constraint
) {
	if (!ctx || operation == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	if (operation >= ctx->metadata->operation_count ||
		constraint >= ctx->classifier_solver.constraint_count ||
		ctx->classifier_solver.dependent_constraint_count >=
			PROTOTYPE_OPERATION_CONSTRAINT_CAPACITY * 4) {
		return -1;
	}
	for (uint32_t dependency =
			ctx->classifier_solver.first_dependent_constraint[operation];
		dependency != PROTOTYPE_INVALID_ID;
		dependency =
			ctx->classifier_solver.dependent_constraints[dependency].next) {
		if (dependency >= ctx->classifier_solver.dependent_constraint_count) {
			return -1;
		}
		if (ctx->classifier_solver.dependent_constraints[dependency].constraint ==
			constraint) {
			return 0;
		}
	}
	uint32_t dependency = ctx->classifier_solver.dependent_constraint_count++;
	ctx->classifier_solver.dependent_constraints[dependency].constraint = constraint;
	ctx->classifier_solver.dependent_constraints[dependency].next =
		ctx->classifier_solver.first_dependent_constraint[operation];
	ctx->classifier_solver.first_dependent_constraint[operation] = dependency;
	return 0;
}

static int operation_solver_validate_classifier_goal(
	const struct compile_context* ctx,
	const struct operation_classifier_constraint* goal
) {
	if (!ctx || !ctx->asts || !ctx->terms || !ctx->metadata || !goal ||
		goal->id >= ctx->classifier_solver.constraint_count ||
		goal->source_operation >= ctx->metadata->operation_count ||
		goal->classifier_variable >= ctx->metadata->operation_count ||
		goal->context_id >= ctx->metadata->contexts.context_count ||
		goal->context_id !=
			ctx->metadata->operations[goal->source_operation].context_id ||
		(goal->source_ast != PROTOTYPE_INVALID_ID &&
		 goal->source_ast >= ctx->asts->node_count)) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[goal->source_operation];
	switch (goal->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE: {
			union operation_classifier_goal_payload empty_payload = { 0 };
			return memcmp(
				&goal->payload, &empty_payload, sizeof(goal->payload)
			) == 0 ? 0 : -1;
		}
		case OPERATION_CONSTRAINT_EQUAL:
			return operation->tag == PROTOTYPE_OPERATION_NAME &&
				goal->payload.reference.referenced_operation == operation->function &&
				goal->payload.reference.referenced_operation <
					ctx->metadata->operation_count ? 0 : -1;
		case OPERATION_CONSTRAINT_CONVERTIBLE:
			return operation->tag == PROTOTYPE_OPERATION_ASCRIPTION &&
				goal->payload.conversion.body_operation == operation->body &&
				goal->payload.conversion.body_operation <
					ctx->metadata->operation_count &&
				(goal->payload.conversion.expected_classifier == PROTOTYPE_INVALID_ID ||
				 (goal->payload.conversion.expected_classifier ==
					operation->known_classifier &&
				  goal->payload.conversion.expected_classifier <
					ctx->terms->term_count)) ? 0 : -1;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			if (goal->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
				return operation->tag == PROTOTYPE_OPERATION_LAMBDA &&
					goal->payload.pi.body_or_function_operation == operation->body &&
					goal->payload.pi.body_or_function_operation <
						ctx->metadata->operation_count &&
					goal->payload.pi.domain_classifier_or_argument_operation ==
						operation->binder_classifier &&
					(operation->binder_classifier == PROTOTYPE_INVALID_ID ||
					 operation->binder_classifier < ctx->terms->term_count) ? 0 : -1;
			}
			return goal->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
				operation->tag == PROTOTYPE_OPERATION_APP &&
				operation->application_role ==
					PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION &&
				goal->payload.pi.body_or_function_operation == operation->function &&
				goal->payload.pi.domain_classifier_or_argument_operation ==
					operation->argument &&
				operation->function < ctx->metadata->operation_count &&
				operation->argument < ctx->metadata->operation_count ? 0 : -1;
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			return operation->tag == PROTOTYPE_OPERATION_APP &&
				operation->application_role ==
					PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION &&
				goal->payload.constructor_formation.function_operation ==
					operation->function &&
				goal->payload.constructor_formation.argument_operation ==
					operation->argument &&
				operation->function < ctx->metadata->operation_count &&
				operation->argument < ctx->metadata->operation_count ? 0 : -1;
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION: {
			const struct operation_classifier_motive_case_goal* motive =
				&goal->payload.motive_case;
			if (operation->tag != PROTOTYPE_OPERATION_MATCH ||
				motive->case_index >= operation->case_count ||
				operation->first_case + motive->case_index >=
					ctx->metadata->operation_case_count ||
				operation->core_term >= ctx->terms->term_count ||
				ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
				motive->branch_operation != ctx->metadata->operation_cases[
					operation->first_case + motive->case_index
				].body_operation || motive->branch_operation >=
					ctx->metadata->operation_count ||
				motive->scrutinee_operation != operation->scrutinee ||
				motive->scrutinee_operation >= ctx->metadata->operation_count) {
				return -1;
			}
			const struct prototype_term* match =
				&ctx->terms->terms[operation->core_term];
			uint32_t term_case = match->as.match.first_case + motive->case_index;
			if (term_case >= ctx->terms->case_count) {
				return -1;
			}
			return motive->constructor_owner ==
					ctx->terms->cases[term_case].constructor_owner &&
				motive->constructor_index ==
					ctx->terms->cases[term_case].constructor_id &&
				motive->ih_scope_id == match->as.match.ih_scope_id ? 0 : -1;
		}
		case OPERATION_CONSTRAINT_IH_EXPECTED:
		{
			struct prototype_operation_graph graph;
			struct prototype_operation_induction_edge edge;
			prototype_compile_metadata_operation_graph_const(ctx->metadata, &graph);
			int edge_status = prototype_operation_graph_induction_edge(
					&graph, ctx->terms, goal->classifier_variable, &edge
				);
			return edge_status == 0 &&
				goal->payload.induction_hypothesis.owner_match_operation ==
					edge.owner_match_operation &&
				goal->payload.induction_hypothesis.recursive_argument_operation ==
					edge.argument_operation &&
				goal->payload.induction_hypothesis.ih_scope_id == edge.scope_id &&
				goal->payload.induction_hypothesis.case_index == edge.case_index &&
				goal->payload.induction_hypothesis.field_index == edge.field_index ?
				0 : -1;
		}
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
			return (operation->tag == PROTOTYPE_OPERATION_RETURN ||
					operation->tag == PROTOTYPE_OPERATION_THUNK ||
					operation->tag == PROTOTYPE_OPERATION_FORCE) &&
				goal->payload.cbpv_boundary.child_operation == operation->argument &&
				operation->argument < ctx->metadata->operation_count ? 0 : -1;
		case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			return operation->tag == PROTOTYPE_OPERATION_COMPUTATION_FOLD &&
				goal->payload.computation.first_operation == operation->function &&
				goal->payload.computation.second_operation == operation->argument &&
				goal->payload.computation.third_operation == operation->scrutinee &&
				(operation->function == PROTOTYPE_INVALID_ID ||
				 operation->function < ctx->metadata->operation_count) &&
				(operation->argument == PROTOTYPE_INVALID_ID ||
				 operation->argument < ctx->metadata->operation_count) &&
				(operation->scrutinee == PROTOTYPE_INVALID_ID ||
				 operation->scrutinee < ctx->metadata->operation_count) ? 0 : -1;
		case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
			return operation->tag == PROTOTYPE_OPERATION_REQUEST &&
				goal->payload.computation.first_operation == operation->function &&
				goal->payload.computation.second_operation == operation->argument &&
				goal->payload.computation.third_operation == operation->body &&
				operation->function < ctx->metadata->operation_count &&
				operation->argument < ctx->metadata->operation_count &&
				operation->body < ctx->metadata->operation_count ? 0 : -1;
		default:
			return -1;
	}
}

static int operation_solver_index_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	for (uint32_t i = 0; i < 4096; ++i) {
		ctx->classifier_solver.first_dependent_constraint[i] =
			PROTOTYPE_INVALID_ID;
	}
	ctx->classifier_solver.dependent_constraint_count = 0;
	ctx->classifier_solver.worklist_head = 0;
	ctx->classifier_solver.worklist_count = 0;
	memset(
		ctx->classifier_solver.constraint_queued,
		0,
		sizeof(ctx->classifier_solver.constraint_queued)
	);
	for (uint32_t i = 0; i < ctx->classifier_solver.constraint_count; ++i) {
		const struct operation_classifier_constraint* constraint =
			&ctx->classifier_solver.constraints[i];
		if (constraint->id != i) {
			return -1;
		}
		if (operation_solver_validate_classifier_goal(ctx, constraint) != 0) {
			if (constraint->kind == OPERATION_CONSTRAINT_IH_EXPECTED &&
				constraint->source_operation < ctx->metadata->operation_count) {
				const struct prototype_operation_node* operation =
					&ctx->metadata->operations[constraint->source_operation];
				(void)add_compile_diagnostic(
					ctx,
					PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
					PROTOTYPE_COMPILE_DIAGNOSTIC_IH_OWNERSHIP,
					operation->source_ast,
					constraint->source_operation,
					constraint->id,
					PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID,
					operation->source_ast < ctx->asts->node_count ?
						ctx->asts->nodes[operation->source_ast].span :
						(struct prototype_source_span) { 0, 0 }
				);
			}
			return -1;
		}
		if (operation_solver_add_constraint_dependency(
				ctx, constraint->classifier_variable, i
			) != 0) {
			return -1;
		}
		switch (constraint->kind) {
			case OPERATION_CONSTRAINT_EQUAL:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.reference.referenced_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CONVERTIBLE:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.conversion.body_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_IH_EXPECTED:
				if (operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.induction_hypothesis.recursive_argument_operation,
						i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_PI_EXPECTED:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.pi.body_or_function_operation, i
					) != 0 ||
					(constraint->payload.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
					operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.pi.domain_classifier_or_argument_operation,
						i
					) != 0)) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
				if (operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.constructor_formation.function_operation,
						i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.constructor_formation.argument_operation,
						i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.motive_case.branch_operation, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->payload.motive_case.scrutinee_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.cbpv_boundary.child_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
			case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.computation.first_operation, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->payload.computation.second_operation, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->payload.computation.third_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_HAS_TYPE:
				break;
			default:
				return -1;
		}
		if (operation_solver_enqueue_constraint(ctx, i) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_collect_input_classifiers(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t* classifiers,
	uint32_t classifier_capacity,
	uint32_t* p_classifier_count
) {
	if (!ctx || !classifiers || !p_classifier_count ||
		operation_id >= ctx->metadata->operation_count) {
		return -1;
	}
	const struct prototype_operation_node* operation =
		&ctx->metadata->operations[operation_id];
	uint32_t subject = operation->core_term;
	if (subject >= ctx->terms->term_count) {
		return -1;
	}
	*p_classifier_count = 0;
	for (uint32_t i = 0; i < ctx->classifier_solver.input_fact_count; ++i) {
		const struct operation_solver_input_fact* fact =
			&ctx->classifier_solver.input_facts[i];
		if (fact->subject != subject ||
			(fact->ast_binder_id != PROTOTYPE_INVALID_ID &&
				(operation->tag != PROTOTYPE_OPERATION_VAR ||
				 operation->referenced_ast_binder_id != fact->ast_binder_id))) {
			continue;
		}
		int contains = graph_classifier_list_contains_normalization_equal(
			ctx, classifiers, *p_classifier_count, fact->classifier
		);
		if (contains < 0) {
			return -1;
		}
		if (contains > 0) {
			continue;
		}
		if (*p_classifier_count >= classifier_capacity) {
			return -1;
		}
		classifiers[(*p_classifier_count)++] = fact->classifier;
	}
	return 0;
}

static int operation_solver_refresh_usage_solutions(struct compile_context* ctx);
