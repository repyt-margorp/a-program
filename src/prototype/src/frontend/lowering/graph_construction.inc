struct match_compile_state {
	uint32_t match_ast;
	uint32_t scrutinee;
	uint32_t scrutinee_operation;
	uint32_t ih_scope_id;
	uint32_t scrutinee_proven_classifier_hint;
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

static int constructor_telescope_field_classifier(
	struct compile_context* ctx,
	uint32_t source_context,
	uint32_t owner,
	uint32_t constructor_index,
	const struct prototype_case_binder* previous_binders,
	uint32_t previous_binder_count,
	uint32_t field_index,
	uint32_t* p_classifier
);

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
		if (push_graph_pattern_binder(
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
	if (state->scrutinee_proven_classifier_hint != PROTOTYPE_INVALID_ID) {
		struct prototype_match_constructor_resolution resolved;
		if (prototype_judgement_resolve_match_constructor(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				state->scrutinee_proven_classifier_hint,
				old_case->constructor_symbol_id,
				&resolved
			) == 0 && resolved.field_count == old_case->binder_count) {
			uint32_t case_context = ctx->context_ids[ctx->binder_count];
			for (uint32_t field = 0; field < old_case->binder_count; ++field) {
				uint32_t classifier;
				if (constructor_telescope_field_classifier(
						ctx,
						case_context,
						resolved.constructor_owner,
						resolved.constructor_id,
						&state->binder_storage[*p_binder_start],
						field,
						field,
						&classifier
					) != 0 ||
					prototype_judgement_classifier_is_strictly_positive_recursive_field(
						ctx->terms,
						ctx->type_declarations,
						classifier,
						state->scrutinee_proven_classifier_hint,
						&state->binder_storage[
							*p_binder_start + field
						].is_recursive
					) != 0) {
					return -1;
				}
				ctx->binders[previous_binder_count + field].classifier = classifier;
			}
		}
	}
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
		fprintf(stderr,
			"match branch body lowering failed match=%u case=%u body=%u tag=%d status=%d "
			"category=%u\n",
			state->match_ast, case_index, old_case->body,
			old_case->body < ctx->asts->node_count ?
				ctx->asts->nodes[old_case->body].tag : -1,
			branch_status,
			branch_status == 0 ? body_ref.category : PROTOTYPE_TERM_CATEGORY_INVALID);
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

static int function_graph_association_graph_type(
	struct compile_context* ctx,
	const struct prototype_function_graph_association* association,
	uint32_t* p_graph_type
) {
	if (!ctx || !association || !p_graph_type) {
		return -1;
	}
	if (association->graph_type_id != PROTOTYPE_INVALID_ID) {
		*p_graph_type = association->graph_type_id;
		return 0;
	}
	if (association->graph_type_assignment_id >= ctx->asts->assignment_count) {
		return -1;
	}
	const struct prototype_ast_term_assignment_def* assignment =
		&ctx->asts->assignments[association->graph_type_assignment_id];
	if (!assignment->compiled || assignment->ast >= ctx->asts->node_count) {
		return -1;
	}
	const struct prototype_ast_node* node = &ctx->asts->nodes[assignment->ast];
	if ((node->tag != PROTOTYPE_AST_TYPE_FORMATION &&
		node->tag != PROTOTYPE_AST_TYPE_LITERAL) ||
		node->as.type_formation.ast_type_def_id >= ctx->asts->type_def_count) {
		return -1;
	}
	const struct prototype_ast_type_def* type =
		&ctx->asts->type_defs[node->as.type_formation.ast_type_def_id];
	if (!type->compiled || type->compiled_type == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_graph_type = type->compiled_type;
	return 0;
}

static int function_graph_association_for_classifier(
	struct compile_context* ctx,
	uint32_t classifier,
	uint32_t* p_association_id
) {
	if (!ctx || !p_association_id || classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t classifier_type;
	uint32_t ignored_arguments[64];
	uint32_t ignored_argument_count;
	if (prototype_term_type_instance_info(
			ctx->terms,
			classifier,
			&classifier_type,
			ignored_arguments,
			&ignored_argument_count
		) != 0) {
		return -1;
	}
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t association_id = 0;
		association_id < ctx->metadata->function_graph_association_count;
		++association_id) {
		const struct prototype_function_graph_association* association =
			&ctx->metadata->function_graph_associations[association_id];
		uint32_t graph_type;
		if (function_graph_association_graph_type(
				ctx, association, &graph_type
			) != 0 || graph_type != classifier_type) {
			continue;
		}
		if (selected != PROTOTYPE_INVALID_ID) {
			return -1;
		}
		selected = association_id;
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_association_id = selected;
	return 0;
}

static uint32_t function_graph_ast_application_head(
	const struct prototype_ast_db* asts,
	uint32_t ast
) {
	while (asts && ast < asts->node_count &&
		asts->nodes[ast].tag == PROTOTYPE_AST_APP) {
		ast = asts->nodes[ast].as.app.function;
	}
	return ast;
}

static int function_graph_ast_contains_node(
	const struct prototype_ast_db* asts,
	uint32_t root,
	uint32_t target,
	uint32_t depth
) {
	if (!asts || root >= asts->node_count || depth > 256) {
		return 0;
	}
	if (root == target) {
		return 1;
	}
	const struct prototype_ast_node* node = &asts->nodes[root];
	switch (node->tag) {
		case PROTOTYPE_AST_APP:
			return function_graph_ast_contains_node(
				asts, node->as.app.function, target, depth + 1
			) || function_graph_ast_contains_node(
				asts, node->as.app.argument, target, depth + 1
			);
		case PROTOTYPE_AST_LAMBDA:
			return function_graph_ast_contains_node(
				asts, node->as.lambda.body, target, depth + 1
			);
		case PROTOTYPE_AST_MATCH:
			if (function_graph_ast_contains_node(
					asts, node->as.match.scrutinee, target, depth + 1
				)) {
				return 1;
			}
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				uint32_t case_id = node->as.match.first_case + i;
				if (case_id < asts->case_count && function_graph_ast_contains_node(
						asts, asts->cases[case_id].body, target, depth + 1
					)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_AST_ASCRIPTION:
			return function_graph_ast_contains_node(
				asts, node->as.ascription.term, target, depth + 1
			);
		case PROTOTYPE_AST_QUOTE:
			return function_graph_ast_contains_node(
				asts, node->as.unary.term, target, depth + 1
			);
		case PROTOTYPE_AST_COMPUTATION_BLOCK:
			for (uint32_t i = 0; i < node->as.block.item_count; ++i) {
				if (function_graph_ast_contains_node(
						asts,
						asts->block_items[node->as.block.first_item + i],
						target,
						depth + 1
					)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_AST_BLOCK_BINDING:
			return function_graph_ast_contains_node(
				asts, node->as.block_binding.value, target, depth + 1
			);
		case PROTOTYPE_AST_BLOCK_EXPRESSION:
			return function_graph_ast_contains_node(
				asts, node->as.block_expression.term, target, depth + 1
			);
		default:
			return 0;
	}
}

static int function_graph_association_for_lexical_certified_result(
	struct compile_context* ctx,
	uint32_t named_match_ast,
	const struct prototype_ast_node* named_match,
	uint32_t* p_association_id
) {
	if (!ctx || !named_match || !p_association_id ||
		named_match->as.match.scrutinee >= ctx->asts->node_count ||
		ctx->asts->nodes[named_match->as.match.scrutinee].tag != PROTOTYPE_AST_VAR) {
		return -1;
	}
	uint32_t graph_ast_binder = ctx->asts->nodes[
		named_match->as.match.scrutinee
	].as.var.ast_binder_id;
	uint32_t selected = PROTOTYPE_INVALID_ID;
	for (uint32_t outer_ast = 0; outer_ast < ctx->asts->node_count; ++outer_ast) {
		const struct prototype_ast_node* outer = &ctx->asts->nodes[outer_ast];
		if (outer->tag != PROTOTYPE_AST_MATCH || outer_ast == named_match_ast) {
			continue;
		}
		uint32_t head = function_graph_ast_application_head(
			ctx->asts, outer->as.match.scrutinee
		);
		if (head >= ctx->asts->node_count || ctx->asts->nodes[head].tag !=
			PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE) {
			continue;
		}
		int owner_symbol = ctx->asts->nodes[head].as.certified_function_reference.
			owner_symbol_id;
		const struct prototype_function_graph_association* association =
			prototype_compile_metadata_function_graph_association_for_owner(
				ctx->metadata, owner_symbol
			);
		if (!association || association->returned_constructor_symbol_id < 0) {
			continue;
		}
		for (uint32_t case_index = 0; case_index < outer->as.match.case_count;
			++case_index) {
			const struct prototype_ast_match_case* outer_case = &ctx->asts->cases[
				outer->as.match.first_case + case_index
			];
			if (outer_case->constructor_symbol_id !=
				association->returned_constructor_symbol_id ||
				!function_graph_ast_contains_node(
					ctx->asts, outer_case->body, named_match_ast, 0
				)) {
				continue;
			}
			for (uint32_t binder_index = 0; binder_index < outer_case->binder_count;
				++binder_index) {
				if (ctx->asts->case_binders[outer_case->first_binder + binder_index].
					ast_binder_id != graph_ast_binder) {
					continue;
				}
				uint32_t association_id = (uint32_t)(association -
					ctx->metadata->function_graph_associations);
				if (selected != PROTOTYPE_INVALID_ID && selected != association_id) {
					return -1;
				}
				selected = association_id;
			}
		}
	}
	if (selected == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_association_id = selected;
	return 0;
}

static int function_graph_named_constructor_shape(
	struct compile_context* ctx,
	const struct prototype_function_graph_association* association,
	int constructor_symbol_id,
	uint32_t* p_constructor_ordinal,
	uint32_t* p_field_count
) {
	uint32_t type_id;
	if (!ctx || !association || !p_constructor_ordinal || !p_field_count ||
		function_graph_association_graph_type(ctx, association, &type_id) != 0) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		prototype_type_declaration_lookup_constructor(
			&ctx->type_declarations->semantic_schema,
			type_id,
			constructor_symbol_id
		);
	uint32_t field_contexts[128];
	if (!constructor || prototype_context_extension_path(
			&ctx->metadata->contexts,
			constructor->parameter_context,
			constructor->field_context,
			field_contexts,
			128,
			p_field_count
		) != 0) {
		return -1;
	}
	*p_constructor_ordinal = constructor->constructor_index;
	return 0;
}

static struct prototype_ast_match_selector* function_graph_case_selector_for_origin(
	struct prototype_ast_db* asts,
	struct prototype_ast_match_case* match_case,
	uint32_t origin_ast_binder_id
) {
	if (!asts || !match_case || match_case->first_selector +
		match_case->selector_count > asts->match_selector_count) {
		return NULL;
	}
	for (uint32_t i = 0; i < match_case->selector_count; ++i) {
		struct prototype_ast_match_selector* selector =
			&asts->match_selectors[match_case->first_selector + i];
		if (selector->value_ast_binder_id == origin_ast_binder_id) {
			return selector;
		}
	}
	return NULL;
}

static int function_graph_resolve_case_role_references(
	struct prototype_ast_db* asts,
	struct prototype_ast_match_case* match_case,
	uint32_t ast,
	uint32_t depth
) {
	if (!asts || !match_case || ast >= asts->node_count || depth > 256) {
		return -1;
	}
	struct prototype_ast_node* node = &asts->nodes[ast];
	if (node->tag == PROTOTYPE_AST_FUNCTION_GRAPH_ROLE_REFERENCE) {
		struct prototype_ast_match_selector* selector =
			function_graph_case_selector_for_origin(
				asts,
				match_case,
				node->as.function_graph_role_reference.origin_ast_binder_id
			);
		if (!selector) {
			/* A nested certified elimination owns this role reference and will
			 * resolve it when that elimination is expanded. */
			return 0;
		}
		if (selector->graph_ast_binder_id == PROTOTYPE_INVALID_ID ||
			!(selector->role_mask & PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH)) {
			fprintf(stderr, "named graph selector has no graph role symbol=%d\n",
				node->as.function_graph_role_reference.symbol_id);
			return -1;
		}
		if (node->as.function_graph_role_reference.role ==
			PROTOTYPE_AST_FUNCTION_GRAPH_ROLE_INDUCTION_HYPOTHESIS) {
			if (!(selector->role_mask & PROTOTYPE_FUNCTION_GRAPH_ORIGIN_IH)) {
				fprintf(stderr, "named graph selector has no induction role symbol=%d\n",
					node->as.function_graph_role_reference.symbol_id);
				return -1;
			}
			node->tag = PROTOTYPE_AST_INDUCTION_HYPOTHESIS;
			node->as.induction_hypothesis.ast_binder_id =
				selector->graph_ast_binder_id;
			node->as.induction_hypothesis.symbol_id = selector->local_symbol_id;
		} else {
			node->tag = PROTOTYPE_AST_VAR;
			node->as.var.ast_binder_id = selector->graph_ast_binder_id;
			node->as.var.symbol_id = selector->local_symbol_id;
		}
		return 0;
	}
	switch (node->tag) {
		case PROTOTYPE_AST_APP:
			if (function_graph_resolve_case_role_references(
					asts, match_case, node->as.app.function, depth + 1
				) != 0) {
				return -1;
			}
			return function_graph_resolve_case_role_references(
				asts, match_case, node->as.app.argument, depth + 1
			);
		case PROTOTYPE_AST_LAMBDA:
			return function_graph_resolve_case_role_references(
				asts, match_case, node->as.lambda.body, depth + 1
			);
		case PROTOTYPE_AST_MATCH:
			if (function_graph_resolve_case_role_references(
					asts, match_case, node->as.match.scrutinee, depth + 1
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				uint32_t case_id = node->as.match.first_case + i;
				if (case_id >= asts->case_count ||
					function_graph_resolve_case_role_references(
						asts, match_case, asts->cases[case_id].body, depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		case PROTOTYPE_AST_ASCRIPTION:
			return function_graph_resolve_case_role_references(
				asts, match_case, node->as.ascription.term, depth + 1
			);
		case PROTOTYPE_AST_QUOTE:
			return function_graph_resolve_case_role_references(
				asts, match_case, node->as.unary.term, depth + 1
			);
		case PROTOTYPE_AST_COMPUTATION_BLOCK:
		case PROTOTYPE_AST_DEFINITION_BLOCK: {
			uint32_t first = node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK ?
				node->as.block.first_item : node->as.definition_block.first_assignment;
			uint32_t count = node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK ?
				node->as.block.item_count : node->as.definition_block.assignment_count;
			for (uint32_t i = 0; i < count; ++i) {
				uint32_t item = node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK ?
					asts->block_items[first + i] : asts->definition_items[first + i];
				if (function_graph_resolve_case_role_references(
						asts, match_case, item, depth + 1
					) != 0) {
					return -1;
				}
			}
			return 0;
		}
		case PROTOTYPE_AST_BLOCK_BINDING:
			return function_graph_resolve_case_role_references(
				asts, match_case, node->as.block_binding.value, depth + 1
			);
		case PROTOTYPE_AST_BLOCK_EXPRESSION:
			return function_graph_resolve_case_role_references(
				asts, match_case, node->as.block_expression.term, depth + 1
			);
		case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
			return function_graph_resolve_case_role_references(
				asts, match_case, node->as.block_lambda_exit.value, depth + 1
			);
		default:
			return 0;
	}
}

static int function_graph_expand_direct_certified_elimination(
	struct compile_context* ctx,
	uint32_t ast_id
) {
	if (!ctx || ast_id >= ctx->asts->node_count ||
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_CERTIFIED_ELIMINATION) {
		return -1;
	}
	struct prototype_ast_node direct = ctx->asts->nodes[ast_id];
	const struct prototype_function_graph_association* association =
		compile_function_graph_association(
			ctx, direct.as.certified_elimination.owner_symbol_id
		);
	if (!association || association->returned_constructor_symbol_id < 0) {
		fprintf(
			stderr,
			"direct certified elimination has no accepted association owner=%d\n",
			direct.as.certified_elimination.owner_symbol_id
		);
		return -1;
	}
	struct prototype_ast_binder binders[2] = {
		{
			.ast_binder_id = direct.as.certified_elimination.result_ast_binder_id,
			.symbol_id = direct.as.certified_elimination.result_symbol_id
		},
		{
			.ast_binder_id = direct.as.certified_elimination.graph_ast_binder_id,
			.symbol_id = direct.as.certified_elimination.result_symbol_id
		}
	};
	struct prototype_ast_match_case_input match_case = {
		.constructor_symbol_id = association->returned_constructor_symbol_id,
		.binders = binders,
		.binder_count = 2,
		.body = direct.as.certified_elimination.body,
		.span = direct.span
	};
	uint32_t match_ast;
	if (prototype_ast_match(
			ctx->asts,
			direct.as.certified_elimination.computation,
			&match_case,
			1,
			direct.span,
			&match_ast
		) != 0 || match_ast >= ctx->asts->node_count) {
		return -1;
	}
	struct prototype_ast_node* match = &ctx->asts->nodes[match_ast];
	if (match->as.match.first_case >= ctx->asts->case_count) {
		return -1;
	}
	struct prototype_ast_match_selector selector = {
		.source_symbol_id = direct.as.certified_elimination.result_symbol_id,
		.local_symbol_id = direct.as.certified_elimination.result_symbol_id,
		.value_ast_binder_id =
			direct.as.certified_elimination.result_ast_binder_id,
		.graph_ast_binder_id =
			direct.as.certified_elimination.graph_ast_binder_id,
		.role_mask = PROTOTYPE_FUNCTION_GRAPH_ORIGIN_GRAPH,
		.span = direct.span
	};
	uint32_t case_id = match->as.match.first_case;
	if (prototype_ast_match_case_set_selectors(
			ctx->asts, case_id, &selector, 1
		) != 0 || function_graph_resolve_case_role_references(
			ctx->asts,
			&ctx->asts->cases[case_id],
			direct.as.certified_elimination.body,
			0
		) != 0) {
		return -1;
	}
	ctx->asts->cases[case_id].selectors_expanded = 1;
	ctx->asts->nodes[ast_id] = *match;
	return 0;
}

static int function_graph_expand_named_match_cases(
	struct compile_context* ctx,
	uint32_t match_ast,
	const struct prototype_ast_node* node,
	uint32_t scrutinee_classifier
) {
	if (!ctx || !node || node->tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	int needs_expansion = 0;
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (!ctx->asts->cases[node->as.match.first_case + i].selectors_expanded) {
			needs_expansion = 1;
		}
	}
	if (!needs_expansion) {
		return 0;
	}
	uint32_t association_id;
	if ((scrutinee_classifier == PROTOTYPE_INVALID_ID ||
		function_graph_association_for_classifier(
			ctx, scrutinee_classifier, &association_id
		) != 0) && function_graph_association_for_lexical_certified_result(
			ctx, match_ast, node, &association_id
		) != 0) {
		fprintf(stderr, "named graph case requires a synthesized graph classifier\n");
		return -1;
	}
	const struct prototype_function_graph_association* association =
		&ctx->metadata->function_graph_associations[association_id];
	if (!association->origin_groups_staged || association->first_origin_group +
		association->origin_group_count >
		ctx->metadata->function_graph_origin_group_count) {
		fprintf(stderr, "named graph case has no accepted selector interface\n");
		return -1;
	}
	for (uint32_t case_index = 0; case_index < node->as.match.case_count; ++case_index) {
		struct prototype_ast_match_case* match_case =
			&ctx->asts->cases[node->as.match.first_case + case_index];
		if (match_case->selectors_expanded) {
			continue;
		}
		uint32_t constructor_ordinal;
		uint32_t constructor_field_count;
		struct prototype_match_constructor_resolution resolved;
		int resolved_from_classifier = scrutinee_classifier != PROTOTYPE_INVALID_ID &&
			prototype_judgement_resolve_match_constructor(
				ctx->terms,
				ctx->type_declarations,
				&ctx->metadata->contexts,
				scrutinee_classifier,
				match_case->constructor_symbol_id,
				&resolved
			) == 0;
		if (resolved_from_classifier) {
			constructor_ordinal = resolved.constructor_id;
			constructor_field_count = resolved.field_count;
		} else if (function_graph_named_constructor_shape(
				ctx,
				association,
				match_case->constructor_symbol_id,
				&constructor_ordinal,
				&constructor_field_count
			) != 0) {
			fprintf(stderr,
				"named graph constructor shape failed association=%u constructor=%d\n",
				association_id, match_case->constructor_symbol_id);
			return -1;
		}
		if (ctx->asts->case_binder_count + constructor_field_count >
			ctx->asts->case_binder_capacity) {
			return -1;
		}
		uint32_t first_binder = (uint32_t)ctx->asts->case_binder_count;
		for (uint32_t field = 0; field < constructor_field_count; ++field) {
			struct prototype_ast_match_selector* selected = NULL;
			const struct prototype_function_graph_origin_group* selected_group = NULL;
			for (uint32_t selector_index = 0;
				selector_index < match_case->selector_count;
				++selector_index) {
				struct prototype_ast_match_selector* selector =
					&ctx->asts->match_selectors[
						match_case->first_selector + selector_index
					];
				const struct prototype_function_graph_origin_group* group =
					prototype_compile_metadata_draft_function_graph_origin_group(
						ctx->metadata,
						association_id,
						constructor_ordinal,
						selector->source_symbol_id
					);
				if (!group) {
					fprintf(stderr, "unknown named graph selector symbol=%d case=%d\n",
						selector->source_symbol_id, match_case->constructor_symbol_id);
					return -1;
				}
				if (group->value_field_ordinal == field ||
					group->graph_field_ordinal == field) {
					if (selected && selected != selector) {
						return -1;
					}
					selected = selector;
					selected_group = group;
				}
			}
			struct prototype_ast_binder binder;
			binder.ast_binder_id = PROTOTYPE_INVALID_ID;
			binder.symbol_id = -1;
			if (selected && selected_group->value_field_ordinal == field) {
				binder.ast_binder_id = selected->value_ast_binder_id;
				binder.symbol_id = selected->local_symbol_id;
				selected->role_mask = selected_group->role_mask;
			}
			if (selected && selected_group->graph_field_ordinal == field) {
				if (selected->graph_ast_binder_id == PROTOTYPE_INVALID_ID) {
					selected->graph_ast_binder_id = prototype_ast_new_binder(ctx->asts);
				}
				binder.ast_binder_id = selected->graph_ast_binder_id;
				binder.symbol_id = selected->local_symbol_id;
				selected->role_mask = selected_group->role_mask;
			}
			if (binder.ast_binder_id == PROTOTYPE_INVALID_ID) {
				binder.ast_binder_id = prototype_ast_new_binder(ctx->asts);
			}
			if (binder.ast_binder_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			ctx->asts->case_binders[ctx->asts->case_binder_count++] = binder;
		}
		for (uint32_t selector_index = 0;
			selector_index < match_case->selector_count;
			++selector_index) {
			const struct prototype_ast_match_selector* selector =
				&ctx->asts->match_selectors[
					match_case->first_selector + selector_index
				];
			if (selector->role_mask == 0) {
				fprintf(stderr, "named graph selector did not select a constructor field\n");
				return -1;
			}
		}
		match_case->first_binder = first_binder;
		match_case->binder_count = constructor_field_count;
		match_case->selectors_expanded = 1;
		if (function_graph_resolve_case_role_references(
				ctx->asts, match_case, match_case->body, 0
			) != 0) {
			return -1;
		}
	}
	return 0;
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
	state.scrutinee_proven_classifier_hint = scrutinee_proven_classifier_hint;
	if (function_graph_expand_named_match_cases(
			ctx, ast_id, node, scrutinee_proven_classifier_hint
		) != 0) {
		return -1;
	}
	state.ih_scope_id = prototype_term_new_ih_scope(ctx->terms);
	if (state.ih_scope_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (compile_match_branch(ctx, node, &state, i) != 0) {
			fprintf(stderr,
				"match lowering failed ast=%u case=%u scrutinee-operation=%u\n",
				ast_id, i, state.scrutinee_operation);
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
	uint32_t first_operation_case = (uint32_t)ctx->metadata->typed_occurrences.case_count;
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		uint32_t operation_case;
		if (operation_add_match_case(
				ctx,
				state.branch_operations[i],
				PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID,
				&ctx->asts->cases[node->as.match.first_case + i],
				state.case_inputs[i].binders,
				&operation_case
			) != 0) {
			return -1;
		}
		ctx->metadata->typed_occurrences.cases[operation_case].case_label_symbol_id =
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
			PROTOTYPE_TYPED_OCCURRENCE_MATCH,
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
	/* Pending constructor resolution belongs to this source occurrence, not to
	 * the current interned Match term. Dependent branch pullback may reindex the
	 * occurrence and replace its Core term before constructor names resolve. */
	for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
		if (queue_match_constructor_resolution(
				ctx,
				state.resolution_item_ids[i],
				match_term,
				p_ref->operation,
				state.scrutinee_operation,
				scrutinee_proven_classifier_hint,
				state.case_constructor_symbols[i]
			) != 0) {
			return -1;
		}
	}
	{
		struct prototype_typed_occurrence_graph* occurrence_graph =
			prototype_compile_metadata_typed_occurrences(ctx->metadata);
		for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
			if (prototype_typed_occurrence_graph_add_edge(
					occurrence_graph, p_ref->operation,
					(struct prototype_typed_occurrence_edge) {
						.role = PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY,
						.ordinal = i,
						.child_occurrence = state.branch_operations[i]
					}
				) != 0) {
				return -1;
			}
		}
	}
	/* IH occurrences are lowered before their enclosing Match Operation exists.
	 * Complete that exact typed edge now. The Core IH frame only identifies an
	 * erased match term and cannot distinguish two typed Match occurrences that
	 * intentionally share that term. */
	for (uint32_t operation_id = 0;
		operation_id < ctx->metadata->typed_occurrences.occurrence_count;
		++operation_id) {
		struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[operation_id];
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
			operation->ih_scope_id != state.ih_scope_id) {
			continue;
		}
		if (operation->ih_owner_occurrence != PROTOTYPE_INVALID_ID &&
			operation->ih_owner_occurrence != p_ref->operation) {
			return -1;
		}
		operation->ih_owner_occurrence = p_ref->operation;
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
	if (!db || expr_id >= db->readback.expr_count) {
		return 0;
	}
	const struct prototype_type_expr* expr = &db->readback.exprs[expr_id];
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
	return db && expr_id < db->readback.expr_count &&
		db->readback.exprs[expr_id].tag == PROTOTYPE_TYPE_EXPR_SELF;
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
			ctx->type_declarations->readback.next_level_var++,
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
		int domain_status = parameter->role ==
				PROTOTYPE_AST_FAMILY_BINDER_PARAMETER ?
			compile_ast_binder_value_type_component(
				ctx, parameter->type_expr, &domain
			) : compile_ast_type_expr_term(ctx, parameter->type_expr, &domain);
		if (domain_status != 0 ||
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

	uint32_t family_binder_count = ast_type->parameter_count + ast_type->index_count;
	if (family_binder_count > ctx->binder_count) {
		return -1;
	}
	uint32_t family_base_binder_count = ctx->binder_count - family_binder_count;
	uint32_t type_id;
	if (prototype_type_declaration_add(
			&ctx->type_declarations->semantic_schema,
			&ctx->type_declarations->readback,
			&ctx->type_declarations->representation_db, ast_type->name_symbol_id, &type_id
		) != 0) {
		return -1;
	}
	ctx->type_declarations->semantic_schema.type_declarations[type_id].namespace_symbol_id =
		ctx->namespace_symbol_id;
	uint32_t type_term;

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
			&ctx->type_declarations->semantic_schema,
			&ctx->type_declarations->readback,
			&ctx->type_declarations->representation_db,
			type_id,
			graph_binder_id,
			parameter->name_symbol_id,
			compiled_type_expr
		) != 0) {
			ast_type->compiling = 0;
			return -1;
		}
	}
	ctx->type_declarations->semantic_schema.type_declarations[type_id].parameter_context =
		ctx->context_ids[family_base_binder_count + ast_type->parameter_count];
	ctx->type_declarations->semantic_schema.type_declarations[type_id].index_context =
		ctx->context_ids[family_base_binder_count + family_binder_count];
	ctx->type_declarations->semantic_schema.type_declarations[type_id].index_count =
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
			&ctx->type_declarations->semantic_schema.type_declarations[type_id].formation_classifier
		) != 0) {
		fprintf(stderr, "type formation classifier family failed type=%d id=%u\n",
			ast_type->name_symbol_id, type_id);
		ast_type->compiling = 0;
		return -1;
	}
	prototype_type_declaration_db_mark_semantic_change(
		&ctx->type_declarations->semantic_schema
	);
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
		uint32_t constructor_classifier;
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
				fprintf(stderr, "type field readback failed type=%d constructor=%u field=%u expr=%u tag=%d\n",
					ast_type->name_symbol_id, i, j, field_type,
					field_type < ctx->asts->type_expr_count ?
						ctx->asts->type_exprs[field_type].tag : -1);
				if (field_type < ctx->asts->type_expr_count &&
					ctx->asts->type_exprs[field_type].tag == PROTOTYPE_AST_TYPE_EXPR_APP) {
					const struct prototype_ast_type_expr* failed =
						&ctx->asts->type_exprs[field_type];
					fprintf(stderr, "type field app function=%u(tag=%d) argument=%u(tag=%d)\n",
						failed->as.app.function,
						ctx->asts->type_exprs[failed->as.app.function].tag,
						failed->as.app.argument,
						ctx->asts->type_exprs[failed->as.app.argument].tag);
					uint32_t cursor = field_type;
					while (cursor < ctx->asts->type_expr_count &&
						ctx->asts->type_exprs[cursor].tag == PROTOTYPE_AST_TYPE_EXPR_APP) {
						const struct prototype_ast_type_expr* app =
							&ctx->asts->type_exprs[cursor];
						fprintf(stderr, "type field spine node=%u arg=%u tag=%d\n",
							cursor, app->as.app.argument,
							ctx->asts->type_exprs[app->as.app.argument].tag);
						cursor = app->as.app.function;
					}
					fprintf(stderr, "type field spine head=%u tag=%d binder=%u\n",
						cursor, ctx->asts->type_exprs[cursor].tag,
						ctx->asts->type_exprs[cursor].tag == PROTOTYPE_AST_TYPE_EXPR_VAR ?
							ctx->asts->type_exprs[cursor].as.var.ast_binder_id :
							PROTOTYPE_INVALID_ID);
				}
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
				fprintf(stderr, "type field semantic compile failed type=%d constructor=%u field=%u expr=%u\n",
					ast_type->name_symbol_id, i, j, field_type);
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
			fprintf(stderr, "type result compile failed type=%d constructor=%u expr=%u\n",
				ast_type->name_symbol_id, i, constructor->result_type);
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
			fprintf(stderr,
				"type result shape failed type=%d id=%u constructor=%u result=%u result-id=%u args=%u expected=%u\n",
				ast_type->name_symbol_id, type_id, i, compiled_result_term,
				result_type_id, result_arg_count,
				ast_type->parameter_count + ast_type->index_count);
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
				&constructor_classifier
			) != 0) {
			fprintf(stderr, "type constructor classifier derivation failed type=%d constructor=%u\n",
				ast_type->name_symbol_id, i);
			ctx->binder_count = previous_binder_count;
			ast_type->compiling = 0;
			return -1;
		}
		ctx->binder_count = previous_binder_count;
		int schema_status = prototype_type_declaration_add_constructor_schema(
			&ctx->type_declarations->semantic_schema,
			&ctx->type_declarations->readback,
			&ctx->type_declarations->representation_db,
			&ctx->type_declarations->constructor_classifier_cache,
			type_id,
			constructor->name_symbol_id,
			parameter_context,
			field_context,
			compiled_result_term,
			&constructor_id
		);
		int readback_status = schema_status == 0 ?
			prototype_type_readback_attach_constructor(
			&ctx->type_declarations->semantic_schema,
			&ctx->type_declarations->readback,
				constructor_id,
				compiled_field_types,
				constructor->field_count,
				compiled_result_type
			) : -1;
		int cache_status = readback_status == 0 ?
			prototype_type_constructor_classifier_cache_set(
			&ctx->type_declarations->semantic_schema,
			&ctx->type_declarations->constructor_classifier_cache,
				constructor_id,
				constructor_classifier
			) : -1;
		if (schema_status != 0 || readback_status != 0 || cache_status != 0) {
			fprintf(stderr,
				"type constructor publication failed type=%d constructor=%u schema=%d readback=%d cache=%d\n",
				ast_type->name_symbol_id, i, schema_status, readback_status, cache_status);
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
		prototype_type_declaration_lookup_constructor(
			&ctx->type_declarations->semantic_schema, type_id, member_symbol_id);
	if (!constructor) {
		return -1;
	}
	uint32_t constructor_id = (uint32_t)(constructor -
		ctx->type_declarations->semantic_schema.constructor_declarations);
	uint32_t classifier;
	if (prototype_type_constructor_classifier(
			&ctx->type_declarations->semantic_schema,
			&ctx->type_declarations->constructor_classifier_cache,
			&ctx->type_declarations->specialization_stats, &ctx->metadata->contexts, ctx->terms,
			constructor_id, &classifier
		) != 0) {
		return -1;
	}
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
			return prototype_term_computation_type(
				terms, label, result, term->as.computation_type.totality, p_ret
			);
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
		!p_classifier || constructor_export->constructor_classifier == PROTOTYPE_INVALID_ID ||
		constructor_export->constructor_classifier >= terms->term_count) {
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
	uint32_t classifier = constructor_export->constructor_classifier;
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
	/* constructor_export->constructor_classifier was relocated with the provider
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
			) != 0 || prototype_term_total_computation_type(
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
		) != 0 || type_id >= ctx->type_declarations->semantic_schema.type_count) {
		return -1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		owner_argument_count != type->parameter_count ||
		type->first_constructor + constructor_index >=
			ctx->type_declarations->semantic_schema.constructor_count) {
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&ctx->type_declarations->semantic_schema.constructor_declarations[
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
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[i];
		int saturation = constructor_spine_saturation(ctx, operation->core_term);
		if (saturation < 0) {
			return -1;
		}
		if (saturation != 2) {
			continue;
		}
		for (uint32_t parent_id = 0;
			parent_id < ctx->metadata->typed_occurrences.occurrence_count;
			++parent_id) {
			const struct prototype_typed_occurrence* parent =
				&ctx->metadata->typed_occurrences.occurrences[parent_id];
			if ((parent->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
				 parent->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) &&
				parent->wrapped_occurrence == i) {
				return -1;
			}
			if (parent->first_edge != PROTOTYPE_INVALID_ID &&
				(parent->first_edge > ctx->metadata->typed_occurrences.edge_count ||
				 parent->edge_count > ctx->metadata->typed_occurrences.edge_count -
					parent->first_edge)) {
				return -1;
			}
			for (uint32_t edge_index = 0; edge_index < parent->edge_count;
				++edge_index) {
				const struct prototype_typed_occurrence_edge* edge =
					&ctx->metadata->typed_occurrences.edges[
						parent->first_edge + edge_index
					];
				if (edge->child_occurrence == i &&
					(edge->role != PROTOTYPE_TERM_CHILD_FUNCTION ||
					 parent->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
					 parent->application_role !=
						PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION)) {
					return -1;
				}
			}
		}
		for (size_t label_id = 0;
			label_id < ctx->metadata->label_count;
			++label_id) {
			if (ctx->metadata->labels[label_id].exposed_occurrence == i ||
				ctx->metadata->labels[label_id].body_occurrence == i) {
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
	size_t previous_occurrence_count =
		ctx->metadata->typed_occurrences.occurrence_count;
	size_t previous_edge_count = ctx->metadata->typed_occurrences.edge_count;
	size_t previous_case_count = ctx->metadata->typed_occurrences.case_count;
	size_t previous_fold_clause_count =
		ctx->metadata->typed_occurrences.fold_clause_count;
	uint32_t previous_pending_match_resolution_count = ctx->pending_match_resolution_count;
	size_t previous_resolution_item_count =
		ctx->metadata->resolution_item_count;
	size_t previous_resolve_error_count = ctx->metadata->resolve_error_count;
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
	int lower_status;
	if (def->ast < ctx->asts->node_count &&
		ctx->asts->nodes[def->ast].tag == PROTOTYPE_AST_LAMBDA) {
		/* Lambda is intrinsically a negative computation in raw CBPV. */
		lower_status = compile_ast_computation_ref(ctx, def->ast, &ref);
	} else {
		lower_status = compile_ast_ref(ctx, def->ast, &ref);
	}
	if (lower_status != 0) {
		for (size_t operation_id = previous_occurrence_count;
			operation_id < ctx->metadata->typed_occurrences.occurrence_count;
			++operation_id) {
			if (operation_type_metas_discard_mutable_occurrence(
					ctx, (uint32_t)operation_id
				) != 0) {
				return -1;
			}
		}
		for (size_t assignment_id = 0;
			assignment_id < ctx->asts->assignment_count;
			++assignment_id) {
			struct prototype_ast_term_assignment_def* assignment =
				&ctx->asts->assignments[assignment_id];
			if (assignment != def && assignment->compiled &&
				assignment->compiled_operation >= previous_occurrence_count) {
				assignment->compiled = 0;
				assignment->compiling = 0;
				assignment->published = 0;
				assignment->compiled_term = PROTOTYPE_INVALID_ID;
				assignment->compiled_classifier = PROTOTYPE_INVALID_ID;
				assignment->compiled_operation = PROTOTYPE_INVALID_ID;
			}
		}
		ctx->metadata->typed_occurrences.occurrence_count =
			previous_occurrence_count;
		ctx->metadata->typed_occurrences.edge_count = previous_edge_count;
		ctx->metadata->typed_occurrences.case_count = previous_case_count;
		ctx->metadata->typed_occurrences.fold_clause_count =
			previous_fold_clause_count;
		ctx->pending_match_resolution_count = previous_pending_match_resolution_count;
		ctx->metadata->resolution_item_count = previous_resolution_item_count;
		ctx->metadata->resolve_error_count = previous_resolve_error_count;
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
		if (ref.operation < ctx->metadata->typed_occurrences.occurrence_count) {
			ctx->classifier_seeds[ref.operation].classifier = declared_classifier;
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
	if (ref.operation < ctx->metadata->typed_occurrences.occurrence_count) {
		ctx->metadata->typed_occurrences.occurrences[ref.operation].source_symbol_id = def->name_symbol_id;
		ctx->metadata->typed_occurrences.occurrences[ref.operation].category = ref.category;
		ctx->metadata->typed_occurrences.occurrences[ref.operation].computation_kind =
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
	if (def->compiled_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	p_ret->category = ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].category;
	p_ret->computation_kind =
		ctx->metadata->typed_occurrences.occurrences[def->compiled_operation].computation_kind;
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
		PROTOTYPE_TYPED_OCCURRENCE_REFERENCE,
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
	ctx->metadata->typed_occurrences.occurrences[p_ret->operation].source_symbol_id = def->name_symbol_id;
	ctx->metadata->typed_occurrences.occurrences[p_ret->operation].category = p_ret->category;
	ctx->metadata->typed_occurrences.occurrences[p_ret->operation].computation_kind =
		p_ret->computation_kind;
	return 0;
}

/* A generated witness reference is the public eliminator of its generated
 * function graph.  Its schema is therefore part of the reference form, even
 * though the generated runner body is still synthesized independently and
 * checked against that schema later.  This lets one generated runner call an
 * already generated dependency without turning source-level `::` expectations
 * into inputs to synthesis. */
static int compile_function_graph_runner_classifier(
	struct compile_context* ctx,
	uint32_t runner_assignment,
	uint32_t* p_classifier
) {
	if (!ctx || !p_classifier || runner_assignment >= ctx->asts->assignment_count) {
		return -1;
	}
	struct prototype_ast_type_expectation_def* selected = NULL;
	for (uint32_t i = 0; i < (uint32_t)ctx->asts->expectation_count; ++i) {
		struct prototype_ast_type_expectation_def* expectation =
			&ctx->asts->expectations[i];
		if (expectation->kind != PROTOTYPE_AST_TYPE_ENTRY_EXPECTATION ||
			expectation->paired_assignment_id != runner_assignment) {
			continue;
		}
		if (selected) {
			return -1;
		}
		selected = expectation;
	}
	if (!selected) {
		return -1;
	}
	uint32_t saved_binder_count = ctx->binder_count;
	uint32_t saved_ih_scope_count = ctx->ih_scope_count;
	struct binder_map_entry saved_binders[512];
	uint32_t saved_context_ids[513];
	struct ih_scope_map_entry saved_ih_scopes[512];
	memcpy(
		saved_binders, ctx->binders,
		saved_binder_count * sizeof(saved_binders[0])
	);
	memcpy(
		saved_context_ids, ctx->context_ids,
		(saved_binder_count + 1) * sizeof(saved_context_ids[0])
	);
	memcpy(
		saved_ih_scopes, ctx->ih_scopes,
		saved_ih_scope_count * sizeof(saved_ih_scopes[0])
	);
	ctx->binder_count = 0;
	ctx->ih_scope_count = 0;
	int status = compile_type_expectation_classifier(ctx, selected, p_classifier);
	memcpy(
		ctx->binders, saved_binders,
		saved_binder_count * sizeof(saved_binders[0])
	);
	memcpy(
		ctx->context_ids, saved_context_ids,
		(saved_binder_count + 1) * sizeof(saved_context_ids[0])
	);
	memcpy(
		ctx->ih_scopes, saved_ih_scopes,
		saved_ih_scope_count * sizeof(saved_ih_scopes[0])
	);
	ctx->binder_count = saved_binder_count;
	ctx->ih_scope_count = saved_ih_scope_count;
	return status;
}

static int compile_ast_pure_value_ref(
	struct compile_context* ctx,
	uint32_t ast_id,
	struct compile_ref* p_ret
);

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
		case PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE: {
			const struct prototype_function_graph_association* association =
				compile_function_graph_association(
					ctx,
					node->as.certified_function_reference.owner_symbol_id
				);
			if (association && association->imported) {
				if (association->imported_interface_index >=
						ctx->imported_interface_count) {
					return -1;
				}
				const struct prototype_artifact_interface* interface =
					ctx->imported_interfaces[
						association->imported_interface_index
					];
				if (!interface ||
					association->imported_certified_runner_term_export_index >=
						interface->term_export_count) {
					return -1;
				}
				return compile_external_ref_ref(
					ctx,
					interface->term_exports[
						association->imported_certified_runner_term_export_index
					].name_symbol_id,
					p_ret
				);
			}
			if (!association ||
				association->certified_runner_assignment_id >=
					ctx->asts->assignment_count) {
				fprintf(stderr,
					"function graph witness association unavailable owner=%d association=%p runner=%u assignments=%zu\n",
					node->as.certified_function_reference.owner_symbol_id,
					(void*)association,
					association ? association->certified_runner_assignment_id : PROTOTYPE_INVALID_ID,
					ctx->asts->assignment_count);
				return -1;
			}
			uint32_t runner_assignment =
				association->certified_runner_assignment_id;
			int status = compile_def_ref(
				ctx,
				&ctx->asts->assignments[runner_assignment],
				p_ret
			);
			uint32_t runner_classifier;
			if (status == 0 && compile_function_graph_runner_classifier(
					ctx, runner_assignment, &runner_classifier
				) == 0) {
				p_ret->classifier = runner_classifier;
				ctx->classifier_seeds[p_ret->operation].classifier = runner_classifier;
			}
			if (status != 0) {
				const struct prototype_ast_term_assignment_def* runner =
					&ctx->asts->assignments[runner_assignment];
				fprintf(stderr,
					"function graph witness runner lowering failed owner=%d runner_assignment=%u compiled=%d compiling=%d term=%u classifier=%u operation=%u occurrences=%zu\n",
					node->as.certified_function_reference.owner_symbol_id,
					association->certified_runner_assignment_id,
					runner->compiled, runner->compiling, runner->compiled_term,
					runner->compiled_classifier, runner->compiled_operation,
					ctx->metadata->typed_occurrences.occurrence_count);
			}
			return status;
		}
		case PROTOTYPE_AST_TERMINATES_WITNESS: {
			struct compile_ref computation;
			uint32_t terminates_type;
			if (compile_ast_pure_value_ref(
					ctx, node->as.terminates_witness.computation, &computation
				) != 0 || computation.category != PROTOTYPE_TERM_CATEGORY_VALUE ||
				prototype_term_terminates_type(
					ctx->terms, computation.term, &terminates_type
				) != 0 || prototype_term_terminates_witness(
					ctx->terms, computation.term, &p_ret->term
				) != 0 || operation_add(
					ctx, PROTOTYPE_TYPED_OCCURRENCE_ATOM, p_ret->term,
					terminates_type, ast_id, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, 0, &p_ret->operation
				) != 0) {
				return -1;
			}
			p_ret->classifier = terminates_type;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			ctx->metadata->typed_occurrences.occurrences[
				p_ret->operation
			].category = PROTOTYPE_TERM_CATEGORY_VALUE;
			ctx->termination_evidence_computation_occurrences[p_ret->operation] =
				operation_runtime_unwrap_name(ctx->metadata, computation.operation);
			struct prototype_typed_occurrence_graph* occurrence_graph =
				prototype_compile_metadata_typed_occurrences(ctx->metadata);
			if (prototype_typed_occurrence_graph_add_edge(
					occurrence_graph,
					p_ret->operation,
					(struct prototype_typed_occurrence_edge) {
						.role =
							PROTOTYPE_TERM_CHILD_TERMINATION_EVIDENCE_COMPUTATION,
						.ordinal = 0,
						.child_occurrence =
							ctx->termination_evidence_computation_occurrences[
								p_ret->operation
							]
					}
				) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_AST_INDUCTION_HYPOTHESIS:
		{
			uint32_t binding_id;
			uint32_t ih_scope_id;
			uint32_t ih_case_index;
			uint32_t ih_field_index;
			uint32_t argument_term;
			uint32_t argument_occurrence;
			uint32_t ih_term;
			if (lookup_graph_binder(
					ctx, node->as.induction_hypothesis.ast_binder_id, &binding_id
				) != 0 || lookup_ih_edge(
					ctx,
					node->as.induction_hypothesis.ast_binder_id,
					&ih_scope_id,
					&ih_case_index,
					&ih_field_index
				) != 0 ||
				prototype_term_var(ctx->terms, binding_id, &argument_term) != 0 ||
				operation_add(
					ctx, PROTOTYPE_TYPED_OCCURRENCE_VAR, argument_term, PROTOTYPE_INVALID_ID,
					ast_id, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
					&argument_occurrence
				) != 0 ||
				prototype_term_induction_hypothesis(
					ctx->terms, ih_scope_id, argument_term, &ih_term
				) != 0) {
				return -1;
			}
			ctx->metadata->typed_occurrences.occurrences[argument_occurrence].referenced_ast_binder_id =
				node->as.induction_hypothesis.ast_binder_id;
			ctx->metadata->typed_occurrences.occurrences[argument_occurrence].binding_id = binding_id;
			p_ret->term = ih_term;
			p_ret->classifier = PROTOTYPE_INVALID_ID;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_INVALID;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			if (operation_add(
				ctx, PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS, ih_term,
				PROTOTYPE_INVALID_ID, ast_id, PROTOTYPE_INVALID_ID,
				argument_occurrence, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0, &p_ret->operation
			) != 0) {
				return -1;
			}
			struct prototype_typed_occurrence* ih_operation =
				&ctx->metadata->typed_occurrences.occurrences[p_ret->operation];
			ih_operation->referenced_ast_binder_id =
				node->as.induction_hypothesis.ast_binder_id;
			ih_operation->binding_id = binding_id;
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
						ctx, PROTOTYPE_TYPED_OCCURRENCE_REFERENCE, local->ref.term,
						local->ref.classifier, ast_id, local->ref.operation,
						PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
						PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
						PROTOTYPE_INVALID_ID, 0, &p_ret->operation
					) != 0) {
					return -1;
				}
				ctx->metadata->typed_occurrences.occurrences[p_ret->operation].source_symbol_id =
					local->symbol_id;
				ctx->metadata->typed_occurrences.occurrences[p_ret->operation].referenced_ast_binder_id =
					node->as.var.ast_binder_id;
				ctx->metadata->typed_occurrences.occurrences[p_ret->operation].category = p_ret->category;
				ctx->metadata->typed_occurrences.occurrences[p_ret->operation].computation_kind =
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
				ctx, PROTOTYPE_TYPED_OCCURRENCE_VAR, term, classifier, ast_id,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				0, &p_ret->operation
				) != 0) {
				return -1;
			}
			ctx->metadata->typed_occurrences.occurrences[p_ret->operation].referenced_ast_binder_id =
				node->as.var.ast_binder_id;
			ctx->metadata->typed_occurrences.occurrences[p_ret->operation].binding_id = binding_id;
			(void)lookup_graph_binder_symbol(
				ctx,
				node->as.var.ast_binder_id,
				&ctx->metadata->typed_occurrences.occurrences[p_ret->operation].source_symbol_id
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
				fprintf(
					stderr,
					"namespace member resolution failed ast=%u namespace=%s member=%s status=%d term=%u assignments=%u\n",
					ast_id,
					symbol_to_string(
						ctx->symbols,
						node->as.name_in_namespace.namespace_symbol_id
					),
					symbol_to_string(
						ctx->symbols,
						node->as.name_in_namespace.symbol_id
					),
					status,
					status == 0 ? namespace_term : PROTOTYPE_INVALID_ID,
					lookup_def_index_entry_const(
						ctx->asts,
						node->as.name_in_namespace.namespace_symbol_id
					) ? lookup_def_index_entry_const(
						ctx->asts,
						node->as.name_in_namespace.namespace_symbol_id
					)->assignment_count : 0
				);
				return -1;
			}
			p_ret->term = constructor_term;
			p_ret->classifier = classifier;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			return operation_add(
				ctx, PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR, constructor_term, classifier, ast_id,
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
				ctx, PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR, constructor_term, classifier, ast_id,
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
				int binder_status = parameter->role ==
						PROTOTYPE_AST_FAMILY_BINDER_PARAMETER ?
					compile_ast_binder_value_type_component(
						ctx, parameter->type_expr, &binder_classifier
					) : compile_ast_type_expr_term(
						ctx, parameter->type_expr, &binder_classifier
					);
				if (binder_status != 0 || push_graph_binder(
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
				int binder_status = binder->role ==
						PROTOTYPE_AST_FAMILY_BINDER_PARAMETER ?
					compile_ast_binder_value_type_component(
						ctx, binder->type_expr, &classifier
					) : compile_ast_type_expr_term(
						ctx, binder->type_expr, &classifier
					);
				if (binder_status != 0 || push_graph_binder(
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

static int term_app_head_is_constructor(
	const struct prototype_term_db* terms,
	uint32_t term_id
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
		int constructor_application;
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
		if (whnf == application &&
			function.operation <
				ctx->metadata->typed_occurrences.occurrence_count &&
			argument.operation <
				ctx->metadata->typed_occurrences.occurrence_count &&
			ctx->metadata->typed_occurrences.occurrences[
				function.operation
			].core_term == function.term &&
			ctx->metadata->typed_occurrences.occurrences[
				argument.operation
			].core_term == argument.term) {
			uint32_t classifier = PROTOTYPE_INVALID_ID;
			constructor_application = term_app_head_is_constructor(
				ctx->terms, application
			);
			if (!constructor_application &&
				function.classifier != PROTOTYPE_INVALID_ID &&
				argument.classifier != PROTOTYPE_INVALID_ID) {
				int apply_status = operation_apply_pure_type_family_classifier(
					ctx, function.classifier, argument.classifier,
					argument.term, &classifier
				);
				if (apply_status != 0) {
					classifier = PROTOTYPE_INVALID_ID;
				}
			}
			p_ret->term = application;
			p_ret->classifier = classifier;
			p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
			p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
			if (operation_add(
					ctx, PROTOTYPE_TYPED_OCCURRENCE_APP, application, classifier,
					ast_id, function.operation, argument.operation,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
					0, &p_ret->operation
				) != 0) {
				fprintf(stderr,
					"pure application occurrence construction failed ast=%u term=%u "
					"function=%u argument=%u\n",
					ast_id, application, function.operation, argument.operation);
				return -1;
			}
			if (!constructor_application) {
				ctx->metadata->typed_occurrences.occurrences[
					p_ret->operation
				].category = PROTOTYPE_TERM_CATEGORY_VALUE;
				ctx->metadata->typed_occurrences.occurrences[
					p_ret->operation
				].computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
				ctx->metadata->typed_occurrences.occurrences[
					p_ret->operation
				].application_role =
					PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION;
			}
			return 0;
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
		ctx, PROTOTYPE_TYPED_OCCURRENCE_RETURN, term, PROTOTYPE_INVALID_ID, source_ast,
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
		ctx, PROTOTYPE_TYPED_OCCURRENCE_FORCE, term, PROTOTYPE_INVALID_ID, source_ast,
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
		ctx, PROTOTYPE_TYPED_OCCURRENCE_THUNK, term, PROTOTYPE_INVALID_ID, source_ast,
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
		ctx, PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD, term, PROTOTYPE_INVALID_ID, source_ast,
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
		ref->operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	}
	uint32_t operation_id = operation_runtime_unwrap_name(
		ctx->metadata, ref->operation
	);
	if (operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	uint32_t thunk_computation;
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_THUNK ||
		compile_occurrence_child(
			ctx, operation_id, PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0,
			&thunk_computation
		) != 0) {
		return PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	}
	operation_id = operation_runtime_unwrap_name(ctx->metadata, thunk_computation);
	if (operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	}
	const struct prototype_typed_occurrence* computation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	switch (computation->tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD:
			return PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
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
 * a synthetic binder. Both remain occurrence-specific in TypedOccurrenceGraph even
 * when canonical TermDB lambdas share a core node. */
static int compile_prior_value_ref_at_depth(
	struct compile_context* ctx,
	uint32_t term,
	uint32_t expected_classifier,
	uint32_t source_ast,
	uint32_t depth,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || term >= ctx->terms->term_count || depth > 128) {
		return -1;
	}
	uint32_t current_context = ctx->context_ids[ctx->binder_count];
	uint32_t fallback_occurrence = PROTOTYPE_INVALID_ID;
	int fallback_is_ambiguous = 0;
	for (size_t i = ctx->metadata->typed_occurrences.occurrence_count; i > 0; --i) {
		const struct prototype_typed_occurrence* candidate =
			&ctx->metadata->typed_occurrences.occurrences[i - 1];
		uint32_t candidate_classifier = operation_available_classifier(
			ctx, (uint32_t)(i - 1)
		);
		if (candidate->core_term != term ||
			candidate->category != PROTOTYPE_TERM_CATEGORY_VALUE) {
			continue;
		}
		uint32_t ancestor = current_context;
		while (ancestor != candidate->context_id) {
			const struct prototype_context* context = prototype_context_get(
				&ctx->metadata->contexts, ancestor
			);
			if (!context || context->parent >= ancestor) {
				break;
			}
			ancestor = context->parent;
		}
		if (ancestor != candidate->context_id) {
			continue;
		}
		if (candidate_classifier == PROTOTYPE_INVALID_ID) {
			uint32_t fallback_source = operation_runtime_unwrap_name(
				ctx->metadata, (uint32_t)(i - 1)
			);
			if (fallback_source >=
				ctx->metadata->typed_occurrences.occurrence_count) {
				continue;
			}
			if (fallback_occurrence == PROTOTYPE_INVALID_ID) {
				fallback_occurrence = fallback_source;
			} else if (fallback_occurrence != fallback_source) {
				fallback_is_ambiguous = 1;
			}
			continue;
		}
		if (expected_classifier != PROTOTYPE_INVALID_ID &&
			prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations,
				expected_classifier, candidate_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL &&
			!prototype_judgement_classifier_compatible(
				ctx->terms, ctx->type_declarations,
				expected_classifier, candidate_classifier
			)) {
			continue;
		}
		*p_ret = (struct compile_ref) {
			.term = term,
			.classifier = candidate_classifier,
			.category = PROTOTYPE_TERM_CATEGORY_VALUE,
			.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID,
			.operation = PROTOTYPE_INVALID_ID
		};
		if (operation_add(
				ctx, PROTOTYPE_TYPED_OCCURRENCE_REFERENCE, term,
				candidate_classifier, source_ast, (uint32_t)(i - 1),
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0, &p_ret->operation
			) != 0) {
			return -1;
		}
		ctx->metadata->typed_occurrences.occurrences[p_ret->operation].category =
			PROTOTYPE_TERM_CATEGORY_VALUE;
		return 0;
	}
	if (fallback_occurrence != PROTOTYPE_INVALID_ID && !fallback_is_ambiguous &&
		expected_classifier != PROTOTYPE_INVALID_ID) {
		*p_ret = (struct compile_ref) {
			.term = term,
			.classifier = expected_classifier,
			.category = PROTOTYPE_TERM_CATEGORY_VALUE,
			.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID,
			.operation = PROTOTYPE_INVALID_ID
		};
		if (operation_add(
				ctx, PROTOTYPE_TYPED_OCCURRENCE_REFERENCE, term,
				expected_classifier, source_ast, fallback_occurrence,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0, &p_ret->operation
			) != 0) {
			return -1;
		}
		ctx->metadata->typed_occurrences.occurrences[p_ret->operation].category =
			PROTOTYPE_TERM_CATEGORY_VALUE;
		return 0;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t fields[64];
	uint32_t field_count;
	uint32_t expected_domains[64];
	uint32_t expected_domain_count;
	if (prototype_term_constructor_spine_info(
			ctx->terms, term, &head, &owner, &constructor_index, fields, 64,
			&field_count
		) != 0) {
		return compile_ref_from_term(ctx, term, p_ret);
	}
	if (prototype_judgement_constructor_spine_expected_domains(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			current_context,
			term,
			PROTOTYPE_INVALID_ID,
			expected_domains,
			64,
			&expected_domain_count
		) != 0 || expected_domain_count != field_count) {
		return -1;
	}
	uint32_t head_classifier;
	int saturated;
	if (prototype_judgement_constructor_spine_classifier(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->context_ids[ctx->binder_count],
			head,
			owner,
			NULL,
			0,
			&head_classifier,
			&saturated
		) != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID) {
		return -1;
	}
	struct compile_ref function = {
		.term = head,
		.classifier = head_classifier,
		.category = PROTOTYPE_TERM_CATEGORY_VALUE,
		.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID,
		.operation = PROTOTYPE_INVALID_ID
	};
	if (operation_add(
			ctx, PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR, head, head_classifier,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &function.operation
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < field_count; ++i) {
		struct compile_ref argument;
		uint32_t application;
		if (compile_prior_value_ref_at_depth(
				ctx, fields[i], expected_domains[i], source_ast, depth + 1, &argument
			) != 0 || argument.category != PROTOTYPE_TERM_CATEGORY_VALUE ||
			prototype_term_app(
				ctx->terms, function.term, argument.term, &application
			) != 0) {
			return -1;
		}
		struct compile_ref next = {
			.term = application,
			.classifier = PROTOTYPE_INVALID_ID,
			.category = PROTOTYPE_TERM_CATEGORY_VALUE,
			.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID,
			.operation = PROTOTYPE_INVALID_ID
		};
		if (operation_add(
				ctx, PROTOTYPE_TYPED_OCCURRENCE_APP, application,
				PROTOTYPE_INVALID_ID, source_ast, function.operation,
				argument.operation, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0, &next.operation
			) != 0) {
			return -1;
		}
		function = next;
	}
	(void)constructor_index;
	(void)saturated;
	if (expected_classifier != PROTOTYPE_INVALID_ID) {
		function.classifier = expected_classifier;
		ctx->classifier_seeds[function.operation].classifier = expected_classifier;
	}
	*p_ret = function;
	return 0;
}

static int compile_prior_value_ref(
	struct compile_context* ctx,
	uint32_t term,
	uint32_t expected_classifier,
	uint32_t source_ast,
	struct compile_ref* p_ret
) {
	return compile_prior_value_ref_at_depth(
		ctx, term, expected_classifier, source_ast, 0, p_ret
	);
}

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
	struct compile_ref prior_value;
	struct compile_ref body;
	struct compile_ref lambda;
	const struct compile_ref* continuation_value;
	if (!ctx || !computation || !continuation || !continuation->apply || !p_ret ||
		computation->category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		ctx->binder_count >= 512) {
		return -1;
	}
	saved_binder_count = ctx->binder_count;
	if (continuation->has_source_binder) {
		ast_binder_id = continuation->source_ast_binder_id;
		if (push_graph_computation_result_binder(
				ctx,
				ast_binder_id,
				continuation->source_binder_classifier,
				continuation->source_binder_symbol_id,
				computation->term,
				&binding_id
			) != 0) {
			return -1;
		}
	} else {
		ast_binder_id = prototype_ast_new_binder(ctx->asts);
		binding_id = prototype_term_new_binding(ctx->terms);
		if (ast_binder_id == PROTOTYPE_INVALID_ID ||
			binding_id == PROTOTYPE_INVALID_ID ||
			prototype_context_extend_sequence_result(
				&ctx->metadata->contexts,
				ctx->context_ids[saved_binder_count],
				binding_id,
				PROTOTYPE_INVALID_ID,
				ast_binder_id,
				computation->term,
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
			ctx, PROTOTYPE_TYPED_OCCURRENCE_VAR, variable_term, variable.classifier,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &variable.operation
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	ctx->metadata->typed_occurrences.occurrences[variable.operation].referenced_ast_binder_id =
		ast_binder_id;
	ctx->metadata->typed_occurrences.occurrences[variable.operation].binding_id = binding_id;
	continuation_value = &variable;
	struct prototype_term_normalization_result prior_result;
	uint32_t prior_term = PROTOTYPE_INVALID_ID;
	uint64_t normalization_limit = compile_effort_remaining(ctx);
	int normalization_effort = compile_effort_charge(
		ctx, PROTOTYPE_EFFORT_PHASE_NORMALIZATION, 1
	);
	if (normalization_effort != 0) {
		ctx->binder_count = saved_binder_count;
		return normalization_effort;
	}
	if (prototype_term_normalize_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_COMPUTATION_WHNF,
			computation->term,
			normalization_limit,
			&prior_result
		) != 0) {
		ctx->binder_count = saved_binder_count;
		return -1;
	}
	if (prior_result.status == PROTOTYPE_TERM_NORMALIZATION_STATUS_COMPLETE &&
		prior_result.term_id < ctx->terms->term_count) {
		const struct prototype_term* normalized =
			&ctx->terms->terms[prior_result.term_id];
		if (normalized->tag == PROTOTYPE_TERM_RETURN) {
			prior_term = normalized->as.return_term.value;
		} else if (normalized->tag == PROTOTYPE_TERM_THUNK) {
			prior_term = prior_result.term_id;
		} else if (normalized->tag == PROTOTYPE_TERM_LAMBDA &&
			prototype_term_thunk(
				ctx->terms, prior_result.term_id, &prior_term
			) != 0) {
			ctx->binder_count = saved_binder_count;
			return -1;
		}
	}
	if (prior_term != PROTOTYPE_INVALID_ID) {
		uint32_t prior_classifier = PROTOTYPE_INVALID_ID;
		if (computation->classifier != PROTOTYPE_INVALID_ID) {
			struct prototype_term_classifier_view computation_view;
			if (prototype_judgement_classifier_view(
					ctx->terms,
					ctx->type_declarations,
					NULL,
					computation->classifier,
					&computation_view
				) != 0 || computation_view.category !=
					PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
				computation_view.computation_kind !=
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING) {
				ctx->binder_count = saved_binder_count;
				return -1;
			}
			prior_classifier = computation_view.result;
		}
		if (compile_prior_value_ref(
				ctx, prior_term, prior_classifier, source_ast, &prior_value
			) != 0 || prior_value.category != PROTOTYPE_TERM_CATEGORY_VALUE) {
			ctx->binder_count = saved_binder_count;
			return -1;
		}
		if (prior_value.classifier == PROTOTYPE_INVALID_ID &&
			prior_classifier != PROTOTYPE_INVALID_ID) {
			prior_value.classifier = prior_classifier;
			ctx->classifier_seeds[prior_value.operation].classifier = prior_classifier;
		}
		continuation_value = &prior_value;
	}
	if (continuation->apply(
			ctx, continuation_value, continuation->data, &body
		) != 0 ||
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
			ctx, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA, lambda_term, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, body.operation,
			PROTOTYPE_INVALID_ID,
			continuation->has_source_binder ?
				continuation->source_binder_classifier : PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0,
			&lambda.operation
		) != 0) {
		return -1;
	}
	ctx->metadata->typed_occurrences.occurrences[lambda.operation].referenced_ast_binder_id =
		ast_binder_id;
	ctx->metadata->typed_occurrences.occurrences[lambda.operation].binding_id = binding_id;
	if (operation_binder_owner_publish(ctx, lambda.operation) != 0) {
		return -1;
	}
	ctx->metadata->typed_occurrences.occurrences[lambda.operation].binder_symbol_id =
		continuation->has_source_binder ?
			continuation->source_binder_symbol_id : -1;
	if (continuation->has_source_binder &&
		continuation->source_binder_classifier != PROTOTYPE_INVALID_ID) {
		uint32_t binder_var;
		if (prototype_term_var(
				ctx->terms, binding_id, &binder_var
			) != 0 || queue_binder_assumption(
				ctx,
				ctx->metadata->typed_occurrences.occurrences[body.operation].context_id,
				binder_var,
				continuation->source_binder_classifier,
				lambda.operation
			) != 0) {
			return -1;
		}
	}
	if (compile_ref_make_sequence_fold(
			ctx, computation, &lambda, source_ast, p_ret
		) != 0) {
		return -1;
	}
	struct operation_binder_owner_entry* owner =
		operation_binder_owner_find(ctx, binding_id);
	if (!owner || owner->lambda_occurrence != lambda.operation ||
		(owner->bound_computation_occurrence != PROTOTYPE_INVALID_ID &&
		 owner->bound_computation_occurrence != computation->operation)) {
		return -1;
	}
	owner->bound_computation_occurrence = computation->operation;
	return 0;
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
				ctx, PROTOTYPE_TYPED_OCCURRENCE_APP, term, PROTOTYPE_INVALID_ID,
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
			ctx, PROTOTYPE_TYPED_OCCURRENCE_APP, term, p_ret->classifier,
			source_ast, function->operation, argument->operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &p_ret->operation
		) != 0) {
			return -1;
		}
		ctx->metadata->typed_occurrences.occurrences[p_ret->operation].category = p_ret->category;
		ctx->metadata->typed_occurrences.occurrences[p_ret->operation].computation_kind =
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
				operation_apply_pure_type_family_classifier(
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
					ctx, PROTOTYPE_TYPED_OCCURRENCE_APP, term, result_classifier,
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
			ctx->metadata->typed_occurrences.occurrences[p_ret->operation].category =
				PROTOTYPE_TERM_CATEGORY_VALUE;
			ctx->metadata->typed_occurrences.occurrences[
				p_ret->operation
			].application_role =
				PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION;
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
			ctx, PROTOTYPE_TYPED_OCCURRENCE_APP, term, PROTOTYPE_INVALID_ID,
			source_ast, applied_function.operation, argument->operation,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &p_ret->operation
		) != 0) {
		return -1;
	}
	p_ret->term = term;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	if (applied_function.classifier != PROTOTYPE_INVALID_ID &&
		argument->classifier != PROTOTYPE_INVALID_ID) {
		int apply_status = operation_apply_classifier(
			ctx,
			applied_function.classifier,
			argument->classifier,
			argument->term,
			&p_ret->classifier
		);
		if (apply_status < 0) {
			return -1;
		}
		if (apply_status > 0) {
			p_ret->classifier = PROTOTYPE_INVALID_ID;
		}
	}
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	return 0;
}

/* A recursive field whose solved motive is Thunk(Pi(...)) is already a value.
 * Constructor and ordinary application spines may know that expected domain
 * before the IH occurrence itself is solved. Preserve the direct occurrence
 * instead of inserting a zero-clause fold that would require a returning
 * computation. */
static int compile_induction_hypothesis_for_expected_value(
	struct compile_context* ctx,
	uint32_t ast_id,
	uint32_t function_classifier,
	struct compile_ref* p_ret
) {
	if (!ctx || !p_ret || ast_id >= ctx->asts->node_count ||
		ctx->asts->nodes[ast_id].tag != PROTOTYPE_AST_INDUCTION_HYPOTHESIS ||
		function_classifier == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t callable_classifier;
	uint32_t whnf;
	if (prototype_judgement_expose_callable_classifier(
			ctx->terms,
			ctx->type_declarations,
			function_classifier,
			&callable_classifier
		) != 0 || prototype_term_normalize_complete_with_profile(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			callable_classifier,
			&whnf
		) != 0 || whnf >= ctx->terms->term_count) {
		return -1;
	}
	for (uint32_t depth = 0; depth < 16 && whnf < ctx->terms->term_count &&
		ctx->terms->terms[whnf].tag == PROTOTYPE_TERM_EFFECT_ROW_FORALL;
		++depth) {
		whnf = ctx->terms->terms[whnf].as.effect_row_forall.body;
		if (prototype_term_normalize_complete_with_profile(
				ctx->terms,
				ctx->type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				whnf,
				&whnf
			) != 0) {
			return -1;
		}
	}
	if (ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_PI) {
		return 1;
	}
	uint32_t expected_domain;
	if (prototype_judgement_classifier_value_whnf(
			ctx->terms,
			ctx->type_declarations,
			ctx->terms->terms[whnf].as.pi.domain,
			&expected_domain
		) != 0 || expected_domain >= ctx->terms->term_count) {
		return -1;
	}
	if (ctx->terms->terms[expected_domain].tag != PROTOTYPE_TERM_THUNK_TYPE) {
		return 1;
	}
	if (compile_ast_atomic_ref(ctx, ast_id, p_ret) != 0) {
		return -1;
	}
	p_ret->classifier = expected_domain;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	return 0;
}

struct compile_application_spine_context {
	uint32_t arguments[68];
	uint32_t applications[68];
	struct compile_ref synthetic_arguments[68];
	unsigned char argument_is_synthetic[68];
	uint32_t argument_count;
};

struct compile_application_spine_step_context {
	const struct compile_application_spine_context* spine;
	struct compile_ref function;
	uint32_t index;
};

static int compile_ref_add_checked_expectation(
	struct compile_context* ctx,
	struct compile_ref* ref,
	uint32_t surface_classifier,
	uint32_t source_ast
);

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
		fprintf(stderr,
			"application spine step failed index=%u function=%u classifier=%u argument=%u argument-classifier=%u\n",
			context ? context->index : PROTOTYPE_INVALID_ID,
			context ? context->function.term : PROTOTYPE_INVALID_ID,
			context ? context->function.classifier : PROTOTYPE_INVALID_ID,
			argument ? argument->term : PROTOTYPE_INVALID_ID,
			argument ? argument->classifier : PROTOTYPE_INVALID_ID);
		if (ctx && context && context->function.classifier != PROTOTYPE_INVALID_ID) {
			fprintf(stderr, "  function classifier: ");
			prototype_term_print_debug(
				stderr, ctx->symbols, ctx->intrinsic_environment,
				ctx->type_declarations, ctx->terms, context->function.classifier
			);
			fprintf(stderr, "\n");
		}
		if (ctx && argument && argument->classifier != PROTOTYPE_INVALID_ID) {
			fprintf(stderr, "  argument term: ");
			prototype_term_print_debug(
				stderr, ctx->symbols, ctx->intrinsic_environment,
				ctx->type_declarations, ctx->terms, argument->term
			);
			fprintf(stderr, "\n");
			if (argument->term < ctx->terms->term_count &&
				ctx->terms->terms[argument->term].tag == PROTOTYPE_TERM_VAR) {
				uint32_t binding = ctx->terms->terms[argument->term].as.var.binding_id;
				for (uint32_t i = 0; i < ctx->binder_count; ++i) {
					if (ctx->binders[i].graph_binder_id == binding) {
						fprintf(stderr, "  argument binder symbol: %s (%d)\n",
							symbol_to_string(ctx->symbols, ctx->binders[i].symbol_id),
							ctx->binders[i].symbol_id);
					}
				}
			}
			fprintf(stderr, "  argument classifier: ");
			prototype_term_print_debug(
				stderr, ctx->symbols, ctx->intrinsic_environment,
				ctx->type_declarations, ctx->terms, argument->classifier
			);
			fprintf(stderr, "\n");
		}
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
	if (spine->argument_is_synthetic[index]) {
		struct compile_ref synthetic = spine->synthetic_arguments[index];
		if (function->classifier != PROTOTYPE_INVALID_ID) {
			uint32_t callable = PROTOTYPE_INVALID_ID;
			uint32_t specialized;
			uint32_t whnf;
			int expose_status = prototype_judgement_expose_callable_classifier(
					ctx->terms,
					ctx->type_declarations,
					function->classifier,
					&callable
				);
			specialized = callable;
			int specialize_status = expose_status == 0 &&
				synthetic.classifier != PROTOTYPE_INVALID_ID ?
				prototype_judgement_specialize_effect_rows_for_argument(
					ctx->terms,
					ctx->type_declarations,
					callable,
					synthetic.classifier,
					&specialized
				) : expose_status;
			int normalize_status = specialize_status == 0 ?
				prototype_term_normalize_complete_with_profile(
					ctx->terms,
					ctx->type_declarations,
					NULL,
					PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
					specialized,
					&whnf
				) : 1;
			for (uint32_t depth = 0; normalize_status == 0 && depth < 16 &&
				whnf < ctx->terms->term_count && ctx->terms->terms[whnf].tag ==
					PROTOTYPE_TERM_EFFECT_ROW_FORALL; ++depth) {
				whnf = ctx->terms->terms[whnf].as.effect_row_forall.body;
				normalize_status = prototype_term_normalize_complete_with_profile(
					ctx->terms,
					ctx->type_declarations,
					NULL,
					PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
					whnf,
					&whnf
				);
			}
			if (expose_status != 0 || specialize_status != 0 ||
				normalize_status != 0 || whnf >= ctx->terms->term_count ||
				ctx->terms->terms[whnf].tag != PROTOTYPE_TERM_PI) {
				if (getenv("A_PROGRAM_FORMATION_TRACE") != NULL) {
					fprintf(stderr,
						"formation-trace synthetic application index=%u function=%u "
						"classifier=%u argument=%u argument-classifier=%u expose=%d "
						"specialize=%d normalize=%d whnf=%u tag=%d\n",
						index, function->term, function->classifier, synthetic.term,
						synthetic.classifier, expose_status, specialize_status,
						normalize_status,
						normalize_status == 0 ? whnf : PROTOTYPE_INVALID_ID,
							normalize_status == 0 && whnf < ctx->terms->term_count ?
								ctx->terms->terms[whnf].tag : -1);
					fprintf(stderr, "  callable classifier: ");
					prototype_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms, callable
					);
					fprintf(stderr, "\n  synthetic classifier: ");
					prototype_term_print_debug(
						stderr, ctx->symbols, ctx->intrinsic_environment,
						ctx->type_declarations, ctx->terms, synthetic.classifier
					);
					fprintf(stderr, "\n");
				}
				return -1;
			}
			if (compile_ref_add_checked_expectation(
					ctx,
					&synthetic,
					ctx->terms->terms[whnf].as.pi.domain,
					spine->applications[index]
				) != 0) {
				return -1;
			}
		}
		return continuation.apply(
			ctx, &synthetic, continuation.data, p_ret
		);
	}
	struct compile_ref expected_value;
	int expected_value_status = compile_induction_hypothesis_for_expected_value(
		ctx,
		spine->arguments[index],
		function->classifier,
		&expected_value
	);
	if (expected_value_status < 0) {
		return -1;
	}
	if (expected_value_status == 0) {
		return continuation.apply(
			ctx, &expected_value, continuation.data, p_ret
		);
	}
	return compile_ast_runtime_value_then(
		ctx, spine->arguments[index], &continuation, p_ret
	);
}

static int compile_function_graph_expand_certified_arguments(
	struct compile_context* ctx,
	uint32_t head,
	struct compile_application_spine_context* spine
) {
	if (!ctx || !spine || head >= ctx->asts->node_count ||
		ctx->asts->nodes[head].tag !=
			PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE) {
		return 0;
	}
	const struct prototype_function_graph_association* owner_association =
		compile_function_graph_association(
			ctx,
			ctx->asts->nodes[head].as.certified_function_reference.owner_symbol_id
		);
	if (!owner_association || owner_association->certified_argument_index ==
			PROTOTYPE_INVALID_ID ||
		spine->argument_count <= owner_association->certified_argument_index) {
		return 0;
	}
	if (spine->argument_count + 2 > 68) {
		return -1;
	}
	uint32_t certified_index = owner_association->certified_argument_index;
	uint32_t callback_ast = spine->arguments[certified_index];
	if (callback_ast >= ctx->asts->node_count ||
		ctx->asts->nodes[callback_ast].tag != PROTOTYPE_AST_QUOTE) {
		return -1;
	}
	uint32_t callback_name_ast = ctx->asts->nodes[callback_ast].as.unary.term;
	if (callback_name_ast >= ctx->asts->node_count ||
		ctx->asts->nodes[callback_name_ast].tag != PROTOTYPE_AST_NAME) {
		return -1;
	}
	const struct prototype_function_graph_association* callback_association =
		compile_function_graph_association(
			ctx, ctx->asts->nodes[callback_name_ast].as.name.symbol_id
		);
	if (!callback_association ||
		(!callback_association->imported &&
			(callback_association->graph_interface_assignment_id >=
				ctx->asts->assignment_count ||
			 callback_association->certified_adapter_assignment_id >=
				ctx->asts->assignment_count))) {
		fprintf(stderr,
			"certified callback association incomplete callback=%d interface=%u adapter=%u runner=%u assignments=%zu\n",
			callback_association ? callback_association->owner_symbol_id : -1,
			callback_association ? callback_association->graph_interface_assignment_id :
				PROTOTYPE_INVALID_ID,
			callback_association ? callback_association->certified_adapter_assignment_id :
				PROTOTYPE_INVALID_ID,
			callback_association ? callback_association->certified_runner_assignment_id :
				PROTOTYPE_INVALID_ID,
			ctx->asts->assignment_count);
		return -1;
	}
	struct compile_ref graph_interface;
	struct compile_ref certified_callback;
	if (callback_association->imported) {
		if (callback_association->imported_interface_index >=
				ctx->imported_interface_count) {
			return -1;
		}
		const struct prototype_artifact_interface* interface =
			ctx->imported_interfaces[
				callback_association->imported_interface_index
			];
		if (!interface ||
			callback_association->imported_graph_interface_term_export_index >=
				interface->term_export_count ||
			callback_association->imported_certified_adapter_term_export_index >=
				interface->term_export_count ||
			compile_external_ref_ref(
				ctx,
				interface->term_exports[
					callback_association->
						imported_graph_interface_term_export_index
				].name_symbol_id,
				&graph_interface
			) != 0 ||
			compile_external_ref_ref(
				ctx,
				interface->term_exports[
					callback_association->
						imported_certified_adapter_term_export_index
				].name_symbol_id,
				&certified_callback
			) != 0) {
			return -1;
		}
	} else if (compile_def_ref(
			ctx,
			&ctx->asts->assignments[
				callback_association->graph_interface_assignment_id
			],
			&graph_interface
		) != 0 ||
		compile_def_ref(
			ctx,
			&ctx->asts->assignments[
				callback_association->certified_adapter_assignment_id
			],
			&certified_callback
		) != 0) {
		return -1;
	}
	if (graph_interface.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		struct compile_ref quoted;
		if (compile_ref_make_thunk(ctx, &graph_interface, callback_ast, &quoted) != 0) {
			return -1;
		}
		graph_interface = quoted;
	}
	if (graph_interface.category != PROTOTYPE_TERM_CATEGORY_VALUE) {
		return -1;
	}
	if (getenv("A_PROGRAM_FORMATION_TRACE") != NULL) {
		fprintf(stderr,
			"formation-trace graph interface term=%u classifier=%u category=%d tag=%d\n",
			graph_interface.term, graph_interface.classifier,
			graph_interface.category,
			graph_interface.term < ctx->terms->term_count ?
				ctx->terms->terms[graph_interface.term].tag : -1);
	}
	if (certified_callback.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION) {
		struct compile_ref quoted;
		if (compile_ref_make_thunk(ctx, &certified_callback, callback_ast, &quoted) != 0) {
			return -1;
		}
		certified_callback = quoted;
	}
	if (certified_callback.category != PROTOTYPE_TERM_CATEGORY_VALUE) {
		return -1;
	}
	struct compile_application_spine_context expanded;
	memset(&expanded, 0, sizeof(expanded));
	for (uint32_t i = 0; i < spine->argument_count; ++i) {
		uint32_t target = expanded.argument_count++;
		expanded.arguments[target] = spine->arguments[i];
		expanded.applications[target] = spine->applications[i];
		if (i != certified_index) {
			continue;
		}
		target = expanded.argument_count++;
		expanded.applications[target] = spine->applications[i];
		expanded.argument_is_synthetic[target] = 1;
		expanded.synthetic_arguments[target] = graph_interface;
		target = expanded.argument_count++;
		expanded.applications[target] = spine->applications[i];
		expanded.argument_is_synthetic[target] = 1;
		expanded.synthetic_arguments[target] = certified_callback;
	}
	*spine = expanded;
	return 0;
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
		if (spine.argument_count >= 68) {
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
		fprintf(stderr,
			"application function lowering failed app=%u head=%u tag=%d\n",
			ast_id, head, ctx->asts->nodes[head].tag);
		return -1;
	}
	if (compile_function_graph_expand_certified_arguments(ctx, head, &spine) != 0) {
		fprintf(stderr,
			"certified application interface unavailable app=%u head=%u\n",
			ast_id, head);
		return -1;
	}
	int status = compile_application_spine_step(ctx, &spine, &function, 0, p_ret);
	if (status != 0) {
		fprintf(stderr,
			"application spine lowering failed app=%u head=%u tag=%d arguments=%u function-term=%u classifier=%u operation=%u category=%d computation-kind=%d\n",
			ast_id, head, ctx->asts->nodes[head].tag, spine.argument_count,
			function.term, function.classifier, function.operation,
			function.category, function.computation_kind);
	}
	return status;
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
		case PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE:
			return AST_LAMBDA_EXIT_NONE;
		case PROTOTYPE_AST_TERMINATES_WITNESS:
			return ast_lambda_exit_presence_in(
				asts, node->as.terminates_witness.computation
			);
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
	int binder_is_type_family =
		node->as.lambda.binder_type < ctx->asts->type_expr_count &&
		(ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		 ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_PI) &&
		ast_type_expr_returns_universe(
			ctx->asts, node->as.lambda.binder_type
		);
	int binder_has_latent_effect =
		node->as.lambda.binder_type < ctx->asts->type_expr_count &&
		!binder_is_type_family &&
		(ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_ARROW ||
		 ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
			PROTOTYPE_AST_TYPE_EXPR_PI ||
			 ctx->asts->type_exprs[node->as.lambda.binder_type].tag ==
				 PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE);
	int exit_presence = ast_lambda_exit_presence_in(ctx->asts, node->as.lambda.body);
	if (exit_presence == AST_LAMBDA_EXIT_FORBIDDEN ||
		(binder_is_type_family ? compile_ast_binder_value_type_component(
				ctx, node->as.lambda.binder_type, &binder_classifier
			) : binder_has_latent_effect ? compile_ast_binder_value_type_with_latent_effect_row(
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
		fprintf(stderr,
			"lambda lowering failed ast=%u body=%u binder-type=%u binder-count=%u\n",
			source_ast, node->as.lambda.body, node->as.lambda.binder_type,
			ctx->binder_count);
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
			ctx, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA, lambda_term, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, body.operation,
			PROTOTYPE_INVALID_ID, binder_classifier, PROTOTYPE_INVALID_ID, 0,
			&lambda_operation
		) != 0) {
		return -1;
	}
	ctx->metadata->typed_occurrences.occurrences[lambda_operation].binder_symbol_id =
		node->as.lambda.binder_symbol_id;
	ctx->metadata->typed_occurrences.occurrences[lambda_operation].referenced_ast_binder_id =
		node->as.lambda.ast_binder_id;
	ctx->metadata->typed_occurrences.occurrences[lambda_operation].binding_id = binding_id;
	if (operation_binder_owner_publish(ctx, lambda_operation) != 0) {
		return -1;
	}
	struct prototype_typed_occurrence* lambda_operation_node =
		&ctx->metadata->typed_occurrences.occurrences[lambda_operation];
	const uint32_t* effect_row_variables;
	size_t effect_row_variable_count;
	if (prototype_term_effect_row_variable_terms(
			ctx->terms,
			&effect_row_variables,
			&effect_row_variable_count
		) != 0) {
		return -1;
	}
	for (size_t i = 0; i < effect_row_variable_count; ++i) {
		uint32_t row_binder = ctx->terms->terms[
			effect_row_variables[i]
		].as.effect_row_var.binding_id;
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
			ctx->metadata->typed_occurrences.occurrences[body.operation].context_id,
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
		ctx, PROTOTYPE_TYPED_OCCURRENCE_APP, term, PROTOTYPE_INVALID_ID, ast_id,
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
			node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS ||
			node->tag == PROTOTYPE_AST_CERTIFIED_ELIMINATION) {
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
				PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE,
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
		int namespace_status = compile_ast_pure_value_ref(
			ctx, node->as.name_in_ast_namespace.namespace_ast, &namespace_ref
		);
		int member_status = namespace_status == 0 ? resolve_namespace_member(
				ctx,
				namespace_ref.term,
				node->as.name_in_ast_namespace.symbol_id,
				&constructor_term,
				&classifier
			) : -1;
		if (namespace_status != 0 || member_status != 0) {
			fprintf(stderr,
				"namespace member lowering failed ast=%u namespace-ast=%u "
				"member=%s namespace-status=%d member-status=%d namespace-term=%u\n",
				ast_id, node->as.name_in_ast_namespace.namespace_ast,
				symbol_to_string(
					ctx->symbols, node->as.name_in_ast_namespace.symbol_id
				),
				namespace_status, member_status,
				namespace_status == 0 ? namespace_ref.term : PROTOTYPE_INVALID_ID);
			if (namespace_status == 0) {
				fprintf(stderr, "namespace member owner: ");
				prototype_term_print_debug(
					stderr, ctx->symbols, ctx->intrinsic_environment,
					ctx->type_declarations, ctx->terms, namespace_ref.term
				);
				fprintf(stderr, "\n");
			}
			return -1;
		}
		p_ret->term = constructor_term;
		p_ret->classifier = classifier;
		p_ret->category = PROTOTYPE_TERM_CATEGORY_VALUE;
		p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
		return operation_add(
			ctx, PROTOTYPE_TYPED_OCCURRENCE_CONSTRUCTOR, constructor_term, classifier, ast_id,
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
				ctx, PROTOTYPE_TYPED_OCCURRENCE_ATOM, p_ret->term, p_ret->classifier, ast_id,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
				PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, 0,
				&operation
			) != 0) {
			return -1;
		}
		ctx->metadata->typed_occurrences.occurrences[operation].category = p_ret->category;
		ctx->metadata->typed_occurrences.occurrences[operation].computation_kind =
			p_ret->computation_kind;
		p_ret->operation = operation;
		prototype_judgement_delta_set_context(
			&ctx->judgement_delta,
			ctx->metadata->typed_occurrences.occurrences[operation].context_id
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
		case PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE:
		case PROTOTYPE_AST_TERMINATES_WITNESS:
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
	 * twice, because TypedOccurrenceGraph edges are occurrence-sensitive. */
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
	if (context->function.classifier != PROTOTYPE_INVALID_ID) {
		if (operation_apply_classifier_unchecked(
				ctx,
				context->function.classifier,
				argument->term,
				&application.classifier
			) != 0) {
			fprintf(stderr,
				"constructor classifier application failed ast=%u function-term=%u classifier=%u argument-term=%u\n",
				context->source_ast, context->function.term,
				context->function.classifier, argument->term);
			prototype_term_print_debug(
				stderr, ctx->symbols, ctx->intrinsic_environment,
				ctx->type_declarations, ctx->terms, context->function.classifier
			);
			fprintf(stderr, "\n");
			return -1;
		}
	}
	application.category = PROTOTYPE_TERM_CATEGORY_VALUE;
	application.computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	if (operation_add(
			ctx, PROTOTYPE_TYPED_OCCURRENCE_APP, term, PROTOTYPE_INVALID_ID,
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
	struct compile_ref expected_value;
	int expected_value_status = compile_induction_hypothesis_for_expected_value(
		ctx,
		function_context->argument_ast,
		function->classifier,
		&expected_value
	);
	if (expected_value_status < 0) {
		return -1;
	}
	if (expected_value_status == 0) {
		return argument_continuation.apply(
			ctx, &expected_value, argument_continuation.data, p_ret
		);
	}
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
		int head_status = compile_ast_value_ref(ctx, ast_id, &head);
		if (head_status != 0 ||
			!term_app_head_is_constructor(ctx->terms, head.term)) {
			fprintf(stderr,
				"constructor spine head failed ast=%u status=%d term=%u operation=%u\n",
				ast_id, head_status,
				head_status == 0 ? head.term : PROTOTYPE_INVALID_ID,
				head_status == 0 ? head.operation : PROTOTYPE_INVALID_ID);
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
	uint32_t continuation_occurrence;
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
		) != 0 || prototype_term_total_computation_type(
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
			ctx, PROTOTYPE_TYPED_OCCURRENCE_VAR, canonical_result_variable,
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
	ctx->metadata->typed_occurrences.occurrences[result_variable.operation].binding_id =
		canonical_binder_id;
	ctx->context_ids[ctx->binder_count] = parent_context_id;
	if (operation_add(
			ctx, PROTOTYPE_TYPED_OCCURRENCE_LAMBDA, continuation, continuation_classifier,
			source_ast, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			result_return.operation, PROTOTYPE_INVALID_ID, application_view.result,
			PROTOTYPE_INVALID_ID, 0, &continuation_occurrence
		) != 0) {
		return -1;
	}
	ctx->metadata->typed_occurrences.occurrences[continuation_occurrence].binding_id =
		canonical_binder_id;
	if (operation_binder_owner_publish(ctx, continuation_occurrence) != 0) {
		return -1;
	}
	p_ret->term = request;
	p_ret->classifier = PROTOTYPE_INVALID_ID;
	p_ret->category = PROTOTYPE_TERM_CATEGORY_COMPUTATION;
	p_ret->computation_kind = PROTOTYPE_TERM_COMPUTATION_KIND_INVALID;
	if (operation_add(
			ctx, PROTOTYPE_TYPED_OCCURRENCE_THUNK, continuation_thunk, PROTOTYPE_INVALID_ID,
			source_ast, PROTOTYPE_INVALID_ID, continuation_occurrence,
			PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID, 0, &continuation_occurrence
		) != 0) {
		return -1;
	}
	return operation_add(
		ctx, PROTOTYPE_TYPED_OCCURRENCE_REQUEST, request, application_classifier,
		source_ast, operation->operation, argument->operation,
		continuation_occurrence, PROTOTYPE_INVALID_ID, PROTOTYPE_INVALID_ID,
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

/* A block binding annotation checks the already-synthesized right-hand side and
 * then exposes the checked classifier to the continuation. Keep that selection
 * as its own occurrence: mutating the synthesized occurrence would make the
 * annotation an input to synthesis and would also lose nominal TYPE_VIEWs when
 * a dependent APP later reconstructs only their Core classifier. */
static int compile_ref_add_checked_expectation(
	struct compile_context* ctx,
	struct compile_ref* ref,
	uint32_t surface_classifier,
	uint32_t source_ast
) {
	if (!ctx || !ref) {
		return -1;
	}
	if (surface_classifier == PROTOTYPE_INVALID_ID) {
		return 0;
	}
	uint32_t expected_classifier;
	uint32_t expected_operation;
	uint32_t synthesized_operation = ref->operation;
	if (compile_expected_classifier_for_ref(
			ctx, ref, surface_classifier, &expected_classifier
		) != 0 || operation_add(
			ctx,
			PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE,
			ref->term,
			expected_classifier,
			source_ast,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			synthesized_operation,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			PROTOTYPE_INVALID_ID,
			0,
			&expected_operation
		) != 0 || queue_ascription_check(
			ctx,
			ref->term,
			expected_classifier,
			source_ast,
			synthesized_operation
		) != 0) {
		return -1;
	}
	ref->classifier = expected_classifier;
	ref->operation = expected_operation;
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
			) != 0) || compile_ref_add_checked_expectation(
			ctx, &value, binder_classifier, item_ast
		) != 0) {
		return -1;
	}
	if (value.category == PROTOTYPE_TERM_CATEGORY_VALUE ||
		(value.category == PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
		 value.computation_kind == PROTOTYPE_TERM_COMPUTATION_KIND_FUNCTION &&
		 ctx->asts->nodes[item->as.block_binding.value].tag !=
			PROTOTYPE_AST_INDUCTION_HYPOTHESIS)) {
		uint32_t saved_local_ref_count = ctx->local_ref_count;
		if (push_local_ref(
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
		compile_ref_add_checked_expectation(
			ctx, &value, binder_classifier, item_ast
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
		if (push_local_ref(
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
	if (ctx->asts->nodes[ast_id].tag == PROTOTYPE_AST_CERTIFIED_ELIMINATION &&
		function_graph_expand_direct_certified_elimination(ctx, ast_id) != 0) {
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
				PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE,
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
	if (node->tag == PROTOTYPE_AST_NAME || node->tag == PROTOTYPE_AST_VAR ||
		node->tag == PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE) {
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
				PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE,
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

static int function_graph_type_expr_contains_request(
	const struct prototype_ast_db* asts,
	uint32_t type_expr,
	uint32_t depth
);

static int function_graph_ast_contains_request(
	const struct prototype_ast_db* asts,
	uint32_t ast,
	uint32_t depth
) {
	if (!asts || ast >= asts->node_count || depth > 256) {
		return 0;
	}
	const struct prototype_ast_node* node = &asts->nodes[ast];
	switch (node->tag) {
		case PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE:
		case PROTOTYPE_AST_FUNCTION_GRAPH_ROLE_REFERENCE:
		case PROTOTYPE_AST_CERTIFIED_ELIMINATION:
			return 1;
		case PROTOTYPE_AST_APP:
			return function_graph_ast_contains_request(
				asts, node->as.app.function, depth + 1
			) || function_graph_ast_contains_request(
				asts, node->as.app.argument, depth + 1
			);
		case PROTOTYPE_AST_LAMBDA:
			return function_graph_type_expr_contains_request(
				asts, node->as.lambda.binder_type, depth + 1
			) || function_graph_ast_contains_request(
				asts, node->as.lambda.body, depth + 1
			);
		case PROTOTYPE_AST_MATCH:
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				uint32_t case_id = node->as.match.first_case + i;
				if (case_id < asts->case_count &&
					asts->cases[case_id].selector_count != 0) {
					return 1;
				}
			}
			if (function_graph_ast_contains_request(
					asts, node->as.match.scrutinee, depth + 1
				)) {
				return 1;
			}
			for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
				uint32_t case_id = node->as.match.first_case + i;
				if (case_id >= asts->case_count || function_graph_ast_contains_request(
						asts, asts->cases[case_id].body, depth + 1
					)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_AST_ASCRIPTION:
			return function_graph_ast_contains_request(
				asts, node->as.ascription.term, depth + 1
			) || function_graph_type_expr_contains_request(
				asts, node->as.ascription.type_expr, depth + 1
			);
		case PROTOTYPE_AST_QUOTE:
			return function_graph_ast_contains_request(
				asts, node->as.unary.term, depth + 1
			);
		case PROTOTYPE_AST_COMPUTATION_BLOCK:
		case PROTOTYPE_AST_DEFINITION_BLOCK:
			for (uint32_t i = 0; i < (node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK ?
					node->as.block.item_count : node->as.definition_block.assignment_count);
				++i) {
				uint32_t item = node->tag == PROTOTYPE_AST_COMPUTATION_BLOCK ?
					asts->block_items[node->as.block.first_item + i] :
					asts->definition_items[node->as.definition_block.first_assignment + i];
				if (function_graph_ast_contains_request(asts, item, depth + 1)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_AST_DEFINITION_SELECT:
			return function_graph_ast_contains_request(
				asts, node->as.definition_select.definition_block, depth + 1
			);
		case PROTOTYPE_AST_BLOCK_BINDING:
			return (node->as.block_binding.binder_type != PROTOTYPE_INVALID_ID &&
				function_graph_type_expr_contains_request(
					asts, node->as.block_binding.binder_type, depth + 1
				)) || function_graph_ast_contains_request(
				asts, node->as.block_binding.value, depth + 1
			);
		case PROTOTYPE_AST_BLOCK_EXPRESSION:
			return function_graph_ast_contains_request(
				asts, node->as.block_expression.term, depth + 1
			);
		case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
			return function_graph_ast_contains_request(
				asts, node->as.block_lambda_exit.value, depth + 1
			);
		case PROTOTYPE_AST_COMPUTATION_FOLD:
			if (function_graph_ast_contains_request(
					asts, node->as.computation_fold.computation, depth + 1
				) || function_graph_ast_contains_request(
					asts, node->as.computation_fold.return_body, depth + 1
				)) {
				return 1;
			}
			for (uint32_t i = 0; i < node->as.computation_fold.clause_count; ++i) {
				uint32_t clause_id = node->as.computation_fold.first_clause + i;
				if (clause_id >= asts->computation_fold_clause_count ||
					function_graph_ast_contains_request(
						asts, asts->computation_fold_clauses[clause_id].body,
						depth + 1
					)) {
					return 1;
				}
			}
			return 0;
		case PROTOTYPE_AST_TERMINATES_WITNESS:
			return function_graph_ast_contains_request(
				asts, node->as.terminates_witness.computation, depth + 1
			);
		default:
			return 0;
	}
}

static int function_graph_type_expr_contains_request(
	const struct prototype_ast_db* asts,
	uint32_t type_expr,
	uint32_t depth
) {
	if (!asts || type_expr >= asts->type_expr_count || depth > 256) {
		return 0;
	}
	const struct prototype_ast_type_expr* expr = &asts->type_exprs[type_expr];
	switch (expr->tag) {
		case PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE:
			return 1;
		case PROTOTYPE_AST_TYPE_EXPR_APP:
			return function_graph_type_expr_contains_request(
				asts, expr->as.app.function, depth + 1
			) || function_graph_type_expr_contains_request(
				asts, expr->as.app.argument, depth + 1
			);
		case PROTOTYPE_AST_TYPE_EXPR_ARROW:
			return function_graph_type_expr_contains_request(
				asts, expr->as.arrow.domain, depth + 1
			) || function_graph_type_expr_contains_request(
				asts, expr->as.arrow.codomain, depth + 1
			);
		case PROTOTYPE_AST_TYPE_EXPR_PI:
			return function_graph_type_expr_contains_request(
				asts, expr->as.pi.domain, depth + 1
			) || function_graph_type_expr_contains_request(
				asts, expr->as.pi.codomain, depth + 1
			);
		case PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE:
			return function_graph_type_expr_contains_request(
				asts, expr->as.computation_reference.result, depth + 1
			);
		case PROTOTYPE_AST_TYPE_EXPR_TERMINATES:
			return function_graph_ast_contains_request(
				asts, expr->as.terminates.computation, depth + 1
			);
		default:
			return 0;
	}
}

static int compile_phase_build_selected_entry(struct compile_context* ctx) {
	if (ctx && ctx->metadata && ctx->metadata->function_graph_preflight) {
		return 0;
	}
	if (ctx->asts->root_definition_select != PROTOTYPE_INVALID_ID) {
		const struct prototype_ast_node* select;
		struct prototype_ast_term_assignment_def* selected;
		if (compile_selected_definition(ctx, &select, &selected) != 0) {
			return -1;
		}
		if (!selected || !selected->compiled ||
			selected->compiled_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
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
		ctx->metadata->selected_entry_occurrence = entry_ref.operation;
	}
	return 0;
}

static int compile_phase_build_graph(struct compile_context* ctx) {
	if (!ctx || !ctx->asts) {
		return -1;
	}

	if (ctx->metadata->function_graph_preflight ==
		PROTOTYPE_FUNCTION_GRAPH_COMPILE_OWNER_PREFLIGHT) {
		/* Lazy compilation may otherwise interleave constructors from a field's
		 * referenced type with the owner declaration. TypeDeclarationDB stores each
		 * constructor range contiguously, so establish source type declarations first. */
		for (uint32_t i = 0; i < (uint32_t)ctx->asts->assignment_count; ++i) {
			const struct prototype_ast_term_assignment_def* def =
				&ctx->asts->assignments[i];
			if (def->ast >= ctx->asts->node_count ||
				(ctx->asts->nodes[def->ast].tag != PROTOTYPE_AST_TYPE_FORMATION &&
				ctx->asts->nodes[def->ast].tag != PROTOTYPE_AST_TYPE_LITERAL)) {
				continue;
			}
			ctx->binder_count = 0;
			ctx->ih_scope_count = 0;
			uint32_t term;
			if (compile_def(ctx, &ctx->asts->assignments[i], &term) != 0) {
				return -1;
			}
		}
		for (uint32_t assignment_id = 0;
			assignment_id < ctx->asts->assignment_count;
			++assignment_id) {
			if (function_graph_ast_contains_request(
					ctx->asts, ctx->asts->assignments[assignment_id].ast, 0
				)) {
				continue;
			}
			ctx->binder_count = 0;
			ctx->ih_scope_count = 0;
			uint32_t term;
			if (compile_def(
					ctx,
					&ctx->asts->assignments[assignment_id],
					&term
				) != 0) {
				return -1;
			}
		}
		return 0;
	}
	if (ctx->metadata->function_graph_preflight ==
		PROTOTYPE_FUNCTION_GRAPH_COMPILE_GENERATED_CLOSURE) {
		for (uint32_t association_id = 0;
			association_id < ctx->metadata->function_graph_association_count;
			++association_id) {
			const struct prototype_function_graph_association* association =
				&ctx->metadata->function_graph_associations[association_id];
			if (association->imported) {
				continue;
			}
			uint32_t assignments[5] = {
				association->graph_type_assignment_id,
				association->result_type_assignment_id,
				association->graph_interface_assignment_id,
				association->certified_runner_assignment_id,
				association->executable_assignment_id
			};
			for (uint32_t i = 0; i < 5; ++i) {
				if (assignments[i] >= ctx->asts->assignment_count) {
					return -1;
				}
				ctx->binder_count = 0;
				ctx->ih_scope_count = 0;
				uint32_t term;
				if (compile_def(
						ctx, &ctx->asts->assignments[assignments[i]], &term
					) != 0) {
					return -1;
				}
			}
			if (association->certified_adapter_assignment_id != PROTOTYPE_INVALID_ID) {
				if (association->certified_adapter_assignment_id >=
						ctx->asts->assignment_count) {
					return -1;
				}
				ctx->binder_count = 0;
				ctx->ih_scope_count = 0;
				uint32_t term;
				if (compile_def(
						ctx,
						&ctx->asts->assignments[
							association->certified_adapter_assignment_id
						],
						&term
					) != 0) {
					return -1;
				}
			}
		}
		for (uint32_t assignment_id = 0;
			assignment_id < ctx->asts->assignment_count;
			++assignment_id) {
			struct prototype_ast_term_assignment_def* assignment =
				&ctx->asts->assignments[assignment_id];
			if (assignment->compiled || !function_graph_ast_contains_request(
					ctx->asts, assignment->ast, 0
				)) {
				continue;
			}
			ctx->binder_count = 0;
			ctx->ih_scope_count = 0;
			uint32_t term;
			if (compile_def(ctx, assignment, &term) != 0) {
				return -1;
			}
		}
		return 0;
	}
	/* Accepted projections embedded in generated type declarations may refer to
	 * source type IDs. Compile every type formation in source order before any
	 * term consumer can trigger a generated family lazily. */
	for (uint32_t i = 0; i < (uint32_t)ctx->asts->assignment_count; ++i) {
		const struct prototype_ast_term_assignment_def* def = &ctx->asts->assignments[i];
		if (def->ast >= ctx->asts->node_count ||
			(ctx->asts->nodes[def->ast].tag != PROTOTYPE_AST_TYPE_FORMATION &&
			ctx->asts->nodes[def->ast].tag != PROTOTYPE_AST_TYPE_LITERAL)) {
			continue;
		}
		ctx->binder_count = 0;
		ctx->ih_scope_count = 0;
		uint32_t term;
		if (compile_def(ctx, &ctx->asts->assignments[i], &term) != 0) {
			fprintf(
				stderr,
				"type formation compile failed assignment=%u name=%d ast=%u\n",
				i, def->name_symbol_id, def->ast
			);
			return -1;
		}
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
			fprintf(
				stderr,
				"definition compile failed assignment=%u name=%d ast=%u\n",
				i,
				def->name_symbol_id,
				def->ast
			);
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
		type_id >= ctx->type_declarations->semantic_schema.type_count) {
		fprintf(stderr, "constructor field owner decode failed owner=%u\n", owner);
		return -1;
	}
	const struct prototype_type_declaration* type =
		&ctx->type_declarations->semantic_schema.type_declarations[type_id];
	if (constructor_index >= type->constructor_count ||
		type->first_constructor + constructor_index >=
			ctx->type_declarations->semantic_schema.constructor_count ||
		argument_count != type->parameter_count) {
		fprintf(stderr,
			"constructor field owner shape failed type=%u constructor=%u arguments=%u parameters=%u indices=%u\n",
			type_id, constructor_index, argument_count, type->parameter_count,
			type->index_count);
		return -1;
	}
	const struct prototype_type_constructor_declaration* constructor =
		&ctx->type_declarations->semantic_schema.constructor_declarations[
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
		fprintf(stderr,
			"constructor field parameter substitution failed type=%u source-context=%u parameter-context=%u arguments=%u\n",
			type_id, source_context, constructor->parameter_context, argument_count);
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
	int status = prototype_context_telescope_entry_classifier(
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
	if (status != 0) {
		fprintf(stderr,
			"constructor field telescope projection failed type=%u field=%u previous=%u substitution=%u field-context=%u\n",
			type_id, field_index, previous_binder_count, substitution,
			constructor->field_context);
	}
	return status;
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
	const struct prototype_typed_occurrence* match_operation = NULL;
	for (uint32_t i = 0; i < ctx->metadata->typed_occurrences.occurrence_count; ++i) {
		const struct prototype_typed_occurrence* candidate =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (candidate->tag == PROTOTYPE_TYPED_OCCURRENCE_MATCH &&
			candidate->source_ast == item->ast &&
			candidate->core_term == resolution->match_term) {
			match_operation_id = i;
			match_operation = candidate;
			break;
		}
	}
	if (!match_operation || item->case_index > match_operation->case_count ||
		match_operation->first_case + item->case_index >
			ctx->metadata->typed_occurrences.case_count) {
		return 0;
	}
	for (uint32_t prior = item->case_index; prior > 0; --prior) {
		uint32_t case_index = prior - 1;
		uint32_t branch_occurrence;
		if (compile_occurrence_child(
				ctx, match_operation_id,
				PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, case_index,
				&branch_occurrence
			) != 0) {
			return -1;
		}
		const struct prototype_typed_occurrence* body_occurrence =
			&ctx->metadata->typed_occurrences.occurrences[branch_occurrence];
		uint32_t body_classifier = operation_available_classifier(
			ctx, branch_occurrence
		);
		uint32_t returned_value;
		if (body_classifier == PROTOTYPE_INVALID_ID &&
			body_occurrence->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
			compile_occurrence_child(
				ctx, branch_occurrence, PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0,
				&returned_value
			) == 0) {
			/* Surface Match branches are computation positions. The implicit
			 * RETURN is not the nested elimination scrutinee; its value is. */
			body_classifier = operation_available_classifier(
				ctx, returned_value
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

static int compile_occurrence_core_child_term(
	struct compile_context* ctx,
	uint32_t occurrence,
	int role,
	uint32_t ordinal,
	uint32_t* p_term
) {
	uint32_t child;
	if (!ctx || !p_term || compile_occurrence_child(
			ctx, occurrence, role, ordinal, &child
		) != 0 || child >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	*p_term = ctx->metadata->typed_occurrences.occurrences[child].core_term;
	return *p_term < ctx->terms->term_count ? 0 : -1;
}

static int compile_match_occurrence_ih_scope(
	const struct compile_context* ctx,
	uint32_t occurrence,
	uint32_t fallback,
	uint32_t* p_scope
) {
	if (!ctx || !p_scope) {
		return -1;
	}
	uint32_t scope = fallback;
	int found = 0;
	for (uint32_t i = 0;
		i < ctx->metadata->typed_occurrences.occurrence_count;
		++i) {
		const struct prototype_typed_occurrence* candidate =
			&ctx->metadata->typed_occurrences.occurrences[i];
		if (candidate->tag !=
				PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS ||
			candidate->ih_owner_occurrence != occurrence) {
			continue;
		}
		if (found && scope != candidate->ih_scope_id) {
			return -1;
		}
		scope = candidate->ih_scope_id;
		found = 1;
	}
	*p_scope = scope;
	return 0;
}

static int compile_rebuild_match_occurrence_core(
	struct compile_context* ctx,
	uint32_t occurrence_id,
	uint32_t override_case,
	const struct prototype_case_binder* override_binders,
	uint32_t override_binder_count,
	uint32_t* p_term
) {
	if (!ctx || !p_term || occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	struct prototype_typed_occurrence* occurrence =
		&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
	if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		occurrence->core_term >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t old_core;
	if (prototype_term_core_projection(
			ctx->terms, occurrence->core_term, &old_core
		) != 0 || ctx->terms->terms[old_core].tag != PROTOTYPE_TERM_MATCH) {
		return -1;
	}
	const struct prototype_term* old_match = &ctx->terms->terms[old_core];
	if (occurrence->case_count != old_match->as.match.case_count ||
		occurrence->first_case + occurrence->case_count >
			ctx->metadata->typed_occurrences.case_count ||
		(override_case != PROTOTYPE_INVALID_ID &&
		 override_case >= occurrence->case_count)) {
		return -1;
	}
	uint32_t scrutinee;
	if (compile_occurrence_core_child_term(
			ctx, occurrence_id, PROTOTYPE_TERM_CHILD_SCRUTINEE, 0,
			&scrutinee
		) != 0) {
		return -1;
	}
	struct prototype_match_case_input cases[
		occurrence->case_count == 0 ? 1 : occurrence->case_count
	];
	struct prototype_case_binder binders[
		ctx->terms->case_binder_count == 0 ? 1 : ctx->terms->case_binder_count
	];
	uint32_t binder_cursor = 0;
	for (uint32_t i = 0; i < occurrence->case_count; ++i) {
		uint32_t old_case_id = old_match->as.match.first_case + i;
		if (old_case_id >= ctx->terms->case_count) {
			return -1;
		}
		const struct prototype_match_case* old_case =
			&ctx->terms->cases[old_case_id];
		const struct prototype_typed_occurrence_match_case* occurrence_case =
			&ctx->metadata->typed_occurrences.cases[occurrence->first_case + i];
		if (old_case->binder_count != occurrence_case->binder_count ||
			binder_cursor + occurrence_case->binder_count >
				ctx->terms->case_binder_count) {
			return -1;
		}
		for (uint32_t j = 0; j < occurrence_case->binder_count; ++j) {
			binders[binder_cursor + j] =
				i == override_case ? override_binders[j] :
				ctx->terms->case_binders[old_case->first_binder + j];
			binders[binder_cursor + j].binding_id = occurrence_case->binder_ids[j];
		}
		uint32_t body;
		if (compile_occurrence_core_child_term(
				ctx, occurrence_id,
				PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, i, &body
			) != 0) {
			return -1;
		}
		cases[i] = (struct prototype_match_case_input) {
			.case_label_symbol_id = occurrence_case->case_label_symbol_id,
			.constructor_owner = occurrence_case->constructor_owner,
			.constructor_id = occurrence_case->constructor_id,
			.binders = &binders[binder_cursor],
			.binder_count = occurrence_case->binder_count,
			.body = body
		};
		binder_cursor += occurrence_case->binder_count;
	}
	if (override_case != PROTOTYPE_INVALID_ID &&
		override_binder_count != cases[override_case].binder_count) {
		return -1;
	}
	uint32_t ih_scope;
	if (compile_match_occurrence_ih_scope(
			ctx, occurrence_id, old_match->as.match.ih_scope_id, &ih_scope
		) != 0 || prototype_term_match_with_ih_scope(
			ctx->terms, scrutinee, cases, occurrence->case_count,
			ih_scope, p_term
		) != 0 || (ih_scope != PROTOTYPE_INVALID_ID &&
		prototype_term_set_ih_scope_term(ctx->terms, ih_scope, *p_term) != 0)) {
		return -1;
	}
	return 0;
}

static int operation_classifier_meta_solve(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier
);
static int operation_classifier_meta_invalidate(
	struct compile_context* ctx,
	uint32_t operation
);
static int operation_classifier_meta_publish_structural_seed(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier
);

/* A result-evidence atom is a checked projection of its child endpoints. When
 * Context action changes a child Core term, publish the reconstructed
 * classifier to the sole mutable solution authority. This runs during initial
 * Match resolution, before the dependency worklist is indexed; all constraints
 * are queued immediately afterward.
 * Leaving the old solution or clearing it to a hint would respectively retain
 * stale evidence or lose an already justified equation. */
static int compile_publish_rebuilt_evidence_classifier(
	struct compile_context* ctx,
	uint32_t occurrence_id,
	uint32_t classifier
) {
	if (!ctx || occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	ctx->classifier_seeds[occurrence_id].classifier = classifier;
	ctx->classifier_seeds[occurrence_id].reason =
		OPERATION_CLASSIFIER_SEED_STRUCTURAL;
	if (operation_classifier_meta_publish_structural_seed(
			ctx, occurrence_id, classifier
		) != 0) {
		fprintf(stderr,
			"rebuilt evidence classifier publication failed occurrence=%u "
			"classifier=%u previous=%u state=%d\n",
			occurrence_id,
			classifier,
			ctx->classifier_solver.classifier_metas[occurrence_id].classifier,
			ctx->classifier_solver.classifier_metas[occurrence_id].state
		);
		return -1;
	}
	return 0;
}

static int compile_rebuild_occurrence_core(
	struct compile_context* ctx,
	uint32_t occurrence_id,
	uint32_t* p_term
) {
	if (!ctx || !p_term || occurrence_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	struct prototype_typed_occurrence* occurrence =
		&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
	uint32_t first;
	uint32_t second;
	uint32_t third;
	switch (occurrence->tag) {
		case PROTOTYPE_TYPED_OCCURRENCE_ATOM: {
			uint32_t old_computation;
			if (prototype_term_terminates_witness_info(
					ctx->terms, occurrence->core_term, &old_computation
				) != 0) {
				*p_term = occurrence->core_term;
				return 0;
			}
			if (compile_occurrence_core_child_term(
					ctx,
					occurrence_id,
					PROTOTYPE_TERM_CHILD_TERMINATION_EVIDENCE_COMPUTATION,
					0,
					&first
				) != 0 || prototype_term_terminates_witness(
					ctx->terms, first, p_term
				) != 0) {
				return -1;
			}
			uint32_t classifier;
			if (prototype_term_terminates_type(
					ctx->terms, first, &classifier
				) != 0 || compile_publish_rebuilt_evidence_classifier(
					ctx, occurrence_id, classifier
				) != 0) {
				return -1;
			}
			return 0;
		}
		case PROTOTYPE_TYPED_OCCURRENCE_REFERENCE:
		case PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE:
			if (occurrence->wrapped_occurrence >=
					ctx->metadata->typed_occurrences.occurrence_count) {
				return -1;
			}
			*p_term = ctx->metadata->typed_occurrences.occurrences[
				occurrence->wrapped_occurrence
			].core_term;
			return 0;
		case PROTOTYPE_TYPED_OCCURRENCE_APP:
			return compile_occurrence_core_child_term(
					ctx, occurrence_id, PROTOTYPE_TERM_CHILD_FUNCTION, 0, &first
				) == 0 && compile_occurrence_core_child_term(
					ctx, occurrence_id, PROTOTYPE_TERM_CHILD_ARGUMENT, 0, &second
				) == 0 ? prototype_term_app(ctx->terms, first, second, p_term) : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_LAMBDA:
			return compile_occurrence_core_child_term(
					ctx, occurrence_id, PROTOTYPE_TERM_CHILD_BODY, 0, &first
				) == 0 ? prototype_term_lambda(
					ctx->terms, occurrence->binding_id, first, p_term
				) : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_MATCH:
			return compile_rebuild_match_occurrence_core(
				ctx, occurrence_id, PROTOTYPE_INVALID_ID, NULL, 0, p_term
			);
		case PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS:
			return compile_occurrence_core_child_term(
					ctx, occurrence_id,
					PROTOTYPE_TERM_CHILD_INDUCTION_ARGUMENT, 0, &first
				) == 0 ? prototype_term_induction_hypothesis(
					ctx->terms, occurrence->ih_scope_id, first, p_term
				) : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_RETURN:
			return compile_occurrence_core_child_term(
					ctx, occurrence_id, PROTOTYPE_TERM_CHILD_RETURN_VALUE, 0, &first
				) == 0 ? prototype_term_return(ctx->terms, first, p_term) : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_THUNK:
			return compile_occurrence_core_child_term(
					ctx, occurrence_id,
					PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION, 0, &first
				) == 0 ? prototype_term_thunk(ctx->terms, first, p_term) : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_FORCE:
			return compile_occurrence_core_child_term(
					ctx, occurrence_id, PROTOTYPE_TERM_CHILD_FORCE_VALUE, 0, &first
				) == 0 ? prototype_term_force(ctx->terms, first, p_term) : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_REQUEST:
			return compile_occurrence_core_child_term(
					ctx, occurrence_id,
					PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0, &first
				) == 0 && compile_occurrence_core_child_term(
					ctx, occurrence_id,
					PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0, &second
				) == 0 && compile_occurrence_core_child_term(
					ctx, occurrence_id,
					PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0, &third
				) == 0 ? prototype_term_operation_request(
					ctx->terms, first, second, third, p_term
				) : -1;
		case PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD: {
			if (occurrence->fold_clause_count > 64 ||
				compile_occurrence_core_child_term(
					ctx, occurrence_id,
					PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0, &first
				) != 0 || compile_occurrence_core_child_term(
					ctx, occurrence_id,
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0, &second
				) != 0) {
				return -1;
			}
			struct prototype_computation_fold_clause clauses[
				occurrence->fold_clause_count == 0 ? 1 : occurrence->fold_clause_count
			];
			for (uint32_t i = 0; i < occurrence->fold_clause_count; ++i) {
				if (compile_occurrence_core_child_term(
						ctx, occurrence_id,
						PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_OPERATION, i,
						&clauses[i].operation
					) != 0 || compile_occurrence_core_child_term(
						ctx, occurrence_id,
						PROTOTYPE_TERM_CHILD_FOLD_CLAUSE_BODY, i,
						&clauses[i].body
					) != 0) {
					return -1;
				}
			}
			return prototype_term_computation_fold(
				ctx->terms, first, second, clauses,
				occurrence->fold_clause_count, p_term
			);
		}
		default:
			*p_term = occurrence->core_term;
			return 0;
	}
}

static int compile_propagate_occurrence_core_change(
	struct compile_context* ctx,
	uint32_t changed_occurrence
) {
	if (!ctx || changed_occurrence >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	size_t count = ctx->metadata->typed_occurrences.occurrence_count;
	unsigned char changed[count == 0 ? 1 : count];
	memset(changed, 0, sizeof(changed));
	changed[changed_occurrence] = 1;
	for (uint32_t occurrence_id = changed_occurrence + 1;
		occurrence_id < count;
		++occurrence_id) {
		struct prototype_typed_occurrence* occurrence =
			&ctx->metadata->typed_occurrences.occurrences[occurrence_id];
		int has_changed_child = 0;
		for (uint32_t edge_index = 0;
			edge_index < occurrence->edge_count;
			++edge_index) {
			uint32_t edge_id = occurrence->first_edge + edge_index;
			if (edge_id >= ctx->metadata->typed_occurrences.edge_count) {
				return -1;
			}
			uint32_t child = ctx->metadata->typed_occurrences.edges[
				edge_id
			].child_occurrence;
			if (child >= occurrence_id) {
				return -1;
			}
			has_changed_child |= changed[child] != 0;
		}
		if (!has_changed_child &&
			(occurrence->wrapped_occurrence == PROTOTYPE_INVALID_ID ||
			 occurrence->wrapped_occurrence >= occurrence_id ||
			 !changed[occurrence->wrapped_occurrence])) {
			continue;
		}
		uint32_t rebuilt;
		if (compile_rebuild_occurrence_core(ctx, occurrence_id, &rebuilt) != 0) {
			fprintf(stderr,
				"occurrence core propagation rebuild failed occurrence=%u tag=%d "
				"core=%u changed-child=%u\n",
				occurrence_id, occurrence->tag, occurrence->core_term,
				changed_occurrence
			);
			return -1;
		}
		if (rebuilt != occurrence->core_term) {
			occurrence->core_term = rebuilt;
			/* Resolving Match dispatch after a Context action is elaboration in
			 * the target Context, not reindexing of the unresolved source Core.
			 * Parents rebuilt from that child have the same direct origin. */
			occurrence->context_action_substitution = PROTOTYPE_INVALID_ID;
			occurrence->source_core_term = PROTOTYPE_INVALID_ID;
			occurrence->source_classifier = PROTOTYPE_INVALID_ID;
			changed[occurrence_id] = 1;
		}
	}

	for (uint32_t i = 0; i < ctx->pending_match_resolution_count; ++i) {
		uint32_t operation = ctx->pending_match_resolutions[i].match_operation;
		if (operation < count && changed[operation]) {
			ctx->pending_match_resolutions[i].match_term =
				ctx->metadata->typed_occurrences.occurrences[operation].core_term;
			uint32_t item = ctx->pending_match_resolutions[i].item_id;
			if (item >= ctx->metadata->resolution_item_count) {
				return -1;
			}
			ctx->metadata->resolution_items[item].match_term =
				ctx->pending_match_resolutions[i].match_term;
		}
	}
	for (uint32_t i = 0; i < ctx->pending_match_typing_count; ++i) {
		uint32_t operation = ctx->pending_match_typings[i].operation;
		if (operation < count && changed[operation]) {
			ctx->pending_match_typings[i].match_term =
				ctx->metadata->typed_occurrences.occurrences[operation].core_term;
		}
	}
	for (uint32_t i = 0; i < ctx->pending_ascription_check_count; ++i) {
		uint32_t operation = ctx->pending_ascription_checks[i].operation;
		if (operation < count && changed[operation]) {
			ctx->pending_ascription_checks[i].subject =
				ctx->metadata->typed_occurrences.occurrences[operation].core_term;
		}
	}
	for (uint32_t i = 0; i < ctx->asts->assignment_count; ++i) {
		struct prototype_ast_term_assignment_def* assignment =
			&ctx->asts->assignments[i];
		if (assignment->compiled && assignment->compiled_operation < count &&
			changed[assignment->compiled_operation]) {
			assignment->compiled_term = ctx->metadata->typed_occurrences.occurrences[
				assignment->compiled_operation
			].core_term;
		}
	}
	return 0;
}

static int compile_phase_resolve_pending_match_items(struct compile_context* ctx) {
	if (!ctx) {
		return -1;
	}
	int has_pending = 0;

	for (uint32_t i = 0; i < ctx->pending_match_resolution_count; ++i) {
		struct pending_match_resolution* resolution =
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
				fprintf(stderr,
					"pending Match scrutinee selection failed index=%u item=%u ast=%u operation=%u\n",
					i, resolution->item_id, item->ast,
					resolution->scrutinee_operation);
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
			fprintf(stderr,
				"pending Match constructor resolution failed index=%u item=%u ast=%u constructor=%d classifier=%u tag=%d\n",
				i, resolution->item_id, item->ast,
				resolution->constructor_symbol_id, scrutinee_classifier,
				scrutinee_classifier < ctx->terms->term_count ?
					ctx->terms->terms[scrutinee_classifier].tag : -1);
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
		if (resolution->match_operation >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		struct prototype_typed_occurrence* resolved_operation =
			&ctx->metadata->typed_occurrences.occurrences[
				resolution->match_operation
			];
		resolution->match_term = resolved_operation->core_term;
		const struct prototype_term* resolved_match =
			&ctx->terms->terms[resolution->match_term];
		uint32_t resolved_case_id =
			resolved_match->as.match.first_case + item->case_index;
		if (resolved_case_id >= ctx->terms->case_count) {
			fprintf(stderr, "pending Match resolved case out of range index=%u case=%u\n", i, resolved_case_id);
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
			fprintf(stderr,
				"pending Match binder mismatch index=%u source=%u resolved=%u\n",
				i, source_case->binder_count, resolved_case->binder_count);
			return -1;
		}
		if (resolved_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
			resolved_operation->source_ast != item->ast ||
			item->case_index >= resolved_operation->case_count ||
			resolved_operation->first_case + item->case_index >=
				ctx->metadata->typed_occurrences.case_count) {
			fprintf(stderr, "pending Match occurrence shape failed index=%u operation=%u\n",
				i, resolution->match_operation);
			return -1;
		}
		struct prototype_typed_occurrence_match_case* resolved_operation_case =
			&ctx->metadata->typed_occurrences.cases[
				resolved_operation->first_case + item->case_index
			];
		uint32_t body_occurrence;
		if (compile_occurrence_child(
				ctx, resolution->match_operation,
				PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, item->case_index,
				&body_occurrence
			) != 0 || body_occurrence >=
				ctx->metadata->typed_occurrences.occurrence_count) {
			fprintf(stderr, "pending Match body occurrence failed index=%u operation=%u\n",
				i, resolution->match_operation);
			return -1;
		}
		uint32_t case_context = ctx->metadata->typed_occurrences.occurrences[
			body_occurrence
		].context_id;
		if (case_context == PROTOTYPE_INVALID_ID ||
			resolved_operation_case->binder_count != resolved_case->binder_count) {
			fprintf(stderr,
				"pending Match case context failed index=%u context=%u occurrence-binders=%u resolved-binders=%u\n",
				i, case_context, resolved_operation_case->binder_count,
				resolved_case->binder_count);
			return -1;
		}
		struct prototype_case_binder occurrence_binders[
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY
		];
		if (resolved_case->binder_count >
			PROTOTYPE_TYPED_OCCURRENCE_MATCH_BINDER_CAPACITY) {
			return -1;
		}
		for (uint32_t binder_index = 0;
			binder_index < resolved_case->binder_count;
			++binder_index) {
			if (!prototype_context_contains_binding(
					&ctx->metadata->contexts,
					case_context,
					resolved_operation_case->binder_ids[binder_index]
				)) {
				fprintf(stderr,
					"pending Match case binding missing index=%u binder=%u context=%u\n",
					i, binder_index, case_context);
				return -1;
			}
			occurrence_binders[binder_index] = ctx->terms->case_binders[
				resolved_case->first_binder + binder_index
			];
			occurrence_binders[binder_index].binding_id =
				resolved_operation_case->binder_ids[binder_index];
		}
		for (uint32_t binder_index = 0;
			binder_index < resolved_case->binder_count;
			++binder_index) {
			uint32_t binder_classifier = PROTOTYPE_INVALID_ID;
			if (constructor_telescope_field_classifier(
					ctx,
					case_context,
					resolved.constructor_owner,
					resolved.constructor_id,
					occurrence_binders,
					binder_index,
					binder_index,
					&binder_classifier
				) != 0) {
				fprintf(stderr,
					"pending Match field classifier failed index=%u binder=%u owner=%u constructor=%u context=%u\n",
					i, binder_index, resolved.constructor_owner,
					resolved.constructor_id, case_context);
				return -1;
			}
			if (prototype_judgement_classifier_is_strictly_positive_recursive_field(
					ctx->terms,
					ctx->type_declarations,
					binder_classifier,
					scrutinee_classifier,
					&occurrence_binders[binder_index].is_recursive
				) != 0) {
				fprintf(stderr,
					"pending Match field positivity failed index=%u binder=%u classifier=%u scrutinee=%u\n",
					i, binder_index, binder_classifier, scrutinee_classifier);
				return -1;
			}
			uint32_t binder_var;
			if (prototype_term_var(
					ctx->terms, occurrence_binders[binder_index].binding_id, &binder_var
				) != 0 || queue_binder_assumption(
					ctx,
					case_context,
					binder_var,
					binder_classifier,
					PROTOTYPE_INVALID_ID
				) != 0) {
				fprintf(stderr,
					"pending Match binder assumption failed index=%u binder=%u classifier=%u\n",
					i, binder_index, binder_classifier);
				return -1;
			}
			for (uint32_t operation_id = 0;
				operation_id < ctx->metadata->typed_occurrences.occurrence_count;
				++operation_id) {
				struct prototype_typed_occurrence* operation =
					&ctx->metadata->typed_occurrences.occurrences[operation_id];
				if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
					operation->referenced_ast_binder_id ==
						ctx->asts->case_binders[
							source_case->first_binder + binder_index
						].ast_binder_id &&
					operation->binding_id ==
						resolved_operation_case->binder_ids[binder_index]) {
					ctx->classifier_seeds[operation_id].classifier = binder_classifier;
				}
			}
		}
		resolved_operation_case->constructor_owner = resolved.constructor_owner;
		resolved_operation_case->constructor_id = resolved.constructor_id;
		uint32_t rebuilt_match;
		if (compile_rebuild_match_occurrence_core(
				ctx,
				resolution->match_operation,
				item->case_index,
				occurrence_binders,
				resolved_case->binder_count,
				&rebuilt_match
			) != 0) {
			fprintf(stderr, "pending Match core rebuild failed index=%u operation=%u\n",
				i, resolution->match_operation);
			return -1;
		}
		resolved_operation->core_term = rebuilt_match;
		resolved_operation->context_action_substitution = PROTOTYPE_INVALID_ID;
		resolved_operation->source_core_term = PROTOTYPE_INVALID_ID;
		resolved_operation->source_classifier = PROTOTYPE_INVALID_ID;
		resolution->match_term = rebuilt_match;
		if (compile_propagate_occurrence_core_change(
				ctx, resolution->match_operation
			) != 0 || resolve_match_constructor_resolution_item(
				ctx,
				resolution->item_id,
				rebuilt_match,
				resolved.constructor_owner,
				resolved.constructor_id
			) != 0) {
			fprintf(stderr, "pending Match core propagation failed index=%u operation=%u\n",
				i, resolution->match_operation);
			return -1;
		}
	}
	return has_pending ? 1 : 0;
}

static int operation_solver_add_classifier_goal(
	struct compile_context* ctx,
	int kind,
	uint32_t classifier_meta_id,
	union operation_classifier_goal_payload payload
) {
	if (!ctx || !ctx->metadata ||
		classifier_meta_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->constraints.classifier_constraint_count >=
			PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY ||
		ctx->constraints.constraint_count >=
			PROTOTYPE_COMPILER_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t id = ctx->constraints.constraint_count++;
	ctx->constraints.classifier_constraint_count++;
	ctx->constraints.constraints[id] =
		(struct operation_constraint){
			.id = id,
			.domain = OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER,
			.kind = kind,
			.source_occurrence = classifier_meta_id,
			.source_ast = ctx->metadata->typed_occurrences.occurrences[classifier_meta_id].source_ast,
			.context_id = ctx->metadata->typed_occurrences.occurrences[classifier_meta_id].context_id,
			.state = OPERATION_CONSTRAINT_STATE_PENDING,
			.reason = OPERATION_CLASSIFIER_GOAL_REASON_NONE,
			.evidence_id = PROTOTYPE_INVALID_ID,
			.origin_constraint_id = PROTOTYPE_INVALID_ID,
			.payload.classifier = {
				.classifier_meta_id = classifier_meta_id,
				.goal = payload
			}
		};
	return 0;
}

static int operation_solver_add_motive_case_goal(
	struct compile_context* ctx,
	uint32_t match_operation,
	uint32_t case_index,
	uint32_t body_occurrence,
	uint32_t scrutinee_operation
) {
	if (!ctx || !ctx->metadata ||
		match_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		body_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
		scrutinee_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[match_operation];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
		operation->core_term >= ctx->terms->term_count ||
		ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
		case_index >= operation->case_count ||
		operation->first_case + case_index >=
			ctx->metadata->typed_occurrences.case_count) {
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
		.branch_operation = body_occurrence,
		.scrutinee_operation = scrutinee_operation,
		.case_index = case_index,
		.constructor_owner = match_case->constructor_owner,
		.constructor_index = match_case->constructor_id,
		.ih_scope_id = match->as.match.ih_scope_id,
		.validated_motive = PROTOTYPE_INVALID_ID,
		.validated_actual_classifier = PROTOTYPE_INVALID_ID,
		.validated_context = PROTOTYPE_INVALID_ID,
		.validated_substitution = PROTOTYPE_INVALID_ID,
		.validation_status = OPERATION_MOTIVE_EQUATION_INVALID
	};
	return operation_solver_add_classifier_goal(
		ctx, OPERATION_CONSTRAINT_MOTIVE_EQUATION, match_operation, payload
	);
}

static int operation_classifier_captures_case_binder(
	const struct compile_context* ctx,
	const struct operation_constraint* equation,
	uint32_t classifier,
	uint32_t* p_classifier_whnf
);

static uint32_t operation_solver_classifier(
	const struct compile_context* ctx,
	uint32_t operation
);

static int operation_solver_enqueue_constraint(
	struct compile_context* ctx,
	uint32_t constraint_id
) {
	if (!ctx || constraint_id >= ctx->constraints.constraint_count) {
		return -1;
	}
	ctx->metadata->constraint_enqueue_request_count++;
	if (ctx->constraints.constraint_queued[constraint_id]) {
		ctx->metadata->constraint_enqueue_duplicate_count++;
		return 0;
	}
	if (ctx->constraints.worklist_count >=
		PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY) {
		return -1;
	}
	uint32_t tail = (
		ctx->constraints.worklist_head +
		ctx->constraints.worklist_count
	) % PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY;
	ctx->constraints.worklist[tail] = constraint_id;
	ctx->constraints.constraint_queued[constraint_id] = 1;
	ctx->constraints.worklist_count++;
	ctx->metadata->constraint_enqueue_count++;
	return 0;
}

static int operation_solver_enqueue_dependents(
	struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t dependency =
		ctx->constraints.first_dependent_constraint[operation];
	while (dependency != PROTOTYPE_INVALID_ID) {
		if (dependency >= ctx->constraints.dependent_constraint_count ||
			operation_solver_enqueue_constraint(
				ctx,
				ctx->constraints.dependent_constraints[dependency].constraint
			) != 0) {
			return -1;
		}
		dependency = ctx->constraints.dependent_constraints[dependency].next;
	}
	return 0;
}

/* A refined semantic equation can invalidate already-materialized classifiers
 * downstream of its occurrence. Constraint scheduling alone is insufficient:
 * many rules preserve a solved cell until its authority changes. Clear only
 * classifier projections reachable through the dependency index, then enqueue
 * the same constraints that will reconstruct them from the new authority. */
static int operation_solver_invalidate_dependent_classifiers(
	struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || !ctx->metadata || operation >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		ctx->metadata->typed_occurrences.occurrence_count > 4096) {
		return -1;
	}
	uint8_t visited[4096] = { 0 };
	uint32_t pending[4096];
	uint32_t head = 0;
	uint32_t count = 1;
	pending[0] = operation;
	visited[operation] = 1;
	while (head < count) {
		uint32_t source = pending[head++];
		uint32_t dependency =
			ctx->constraints.first_dependent_constraint[source];
		while (dependency != PROTOTYPE_INVALID_ID) {
			if (dependency >= ctx->constraints.dependent_constraint_count) {
				return -1;
			}
			uint32_t constraint_id =
				ctx->constraints.dependent_constraints[dependency].constraint;
			if (constraint_id >= ctx->constraints.constraint_count ||
				operation_solver_enqueue_constraint(ctx, constraint_id) != 0) {
				return -1;
			}
			const struct operation_constraint* constraint =
				&ctx->constraints.constraints[constraint_id];
			if (constraint->domain == OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER) {
				uint32_t target =
					constraint->payload.classifier.classifier_meta_id;
				if (target >=
						ctx->metadata->typed_occurrences.occurrence_count) {
					return -1;
				}
				if (!visited[target]) {
					if (count >= 4096) {
						return -1;
					}
					visited[target] = 1;
					pending[count++] = target;
					if (ctx->classifier_solver.classifier_metas[target].classifier !=
							PROTOTYPE_INVALID_ID &&
						operation_classifier_meta_invalidate(ctx, target) != 0) {
						return -1;
					}
				}
			}
			dependency =
				ctx->constraints.dependent_constraints[dependency].next;
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
	if (ctx->constraints.worklist_count == 0) {
		return 1;
	}
	*p_constraint =
		ctx->constraints.worklist[ctx->constraints.worklist_head];
	ctx->constraints.worklist_head =
		(ctx->constraints.worklist_head + 1) %
			PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY;
	ctx->constraints.worklist_count--;
	ctx->constraints.constraint_queued[*p_constraint] = 0;
	ctx->metadata->constraint_pop_count++;
	int kind = ctx->constraints.constraints[*p_constraint].kind;
	if (kind > 0 && kind < 16) {
		ctx->metadata->constraint_pop_by_kind[kind]++;
	}
	return 0;
}

static const struct operation_classifier_meta_solution*
operation_classifier_meta_get(
	const struct compile_context* ctx,
	uint32_t meta_id
) {
	if (!ctx || !ctx->metadata ||
		meta_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return NULL;
	}
	const struct operation_classifier_meta_solution* meta =
		&ctx->classifier_solver.classifier_metas[meta_id];
	return meta->owner_occurrence == meta_id ? meta : NULL;
}

static int operation_classifier_meta_invalidate(
	struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || operation >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	struct operation_classifier_meta_solution* solution =
		&ctx->classifier_solver.classifier_metas[operation];
	if (solution->owner_occurrence != operation ||
		solution->state == OPERATION_CLASSIFIER_META_FROZEN) {
		return -1;
	}
	if (solution->classifier != PROTOTYPE_INVALID_ID) {
		solution->stale_classifier = solution->classifier;
		solution->classifier = PROTOTYPE_INVALID_ID;
		solution->evidence_constraint_id = PROTOTYPE_INVALID_ID;
		solution->state = OPERATION_CLASSIFIER_META_STALE;
		ctx->classifier_solver.solution_revision++;
	}
	return 0;
}

static int operation_classifier_meta_publish_internal(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	uint32_t evidence_constraint_id,
	int invalidate_dependents
) {
	if (!ctx || !ctx->metadata ||
		operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	struct operation_classifier_meta_solution* solution =
		&ctx->classifier_solver.classifier_metas[operation];
	if (solution->owner_occurrence != operation ||
		solution->state == OPERATION_CLASSIFIER_META_FROZEN) {
		return -1;
	}
	int republishing_stale =
		solution->state == OPERATION_CLASSIFIER_META_STALE;
	int stale_classifier_changed = republishing_stale &&
		(solution->stale_classifier == PROTOTYPE_INVALID_ID ||
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			solution->stale_classifier,
			classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL);
	if (solution->classifier != classifier) {
		ctx->classifier_solver.solution_revision++;
		if (operation <
				ctx->metadata->typed_occurrences.occurrence_count &&
			ctx->metadata->typed_occurrences.occurrences[operation].tag ==
				PROTOTYPE_TYPED_OCCURRENCE_VAR) {
			operation_context_binding_dependency_changed(
				ctx,
				ctx->metadata->typed_occurrences.occurrences[operation].binding_id
			);
		}
	}
	solution->classifier = classifier;
	solution->stale_classifier = PROTOTYPE_INVALID_ID;
	solution->evidence_constraint_id = evidence_constraint_id;
	solution->state = OPERATION_CLASSIFIER_META_SOLVED;
	if (invalidate_dependents && stale_classifier_changed &&
		operation_solver_invalidate_dependent_classifiers(ctx, operation) != 0) {
		return -1;
	}
	return 0;
}

static int operation_classifier_meta_publish(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	uint32_t evidence_constraint_id
) {
	return operation_classifier_meta_publish_internal(
		ctx, operation, classifier, evidence_constraint_id, 1
	);
}

static int operation_classifier_meta_publish_structural_seed(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier
) {
	if (!ctx) {
		return -1;
	}
	if (ctx->constraints.constraint_count != 0 ||
		ctx->constraints.dependent_constraint_count != 0) {
		if (ctx->classifier_solver.classifier_metas[operation].classifier !=
				PROTOTYPE_INVALID_ID && operation_classifier_meta_invalidate(
					ctx, operation
				) != 0) {
			return -1;
		}
		return operation_classifier_meta_solve(ctx, operation, classifier);
	}
	return operation_classifier_meta_publish_internal(
		ctx, operation, classifier, PROTOTYPE_INVALID_ID, 0
	);
}

static int operation_classifier_meta_solve(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier
) {
	const struct operation_classifier_meta_solution* current =
		operation_classifier_meta_get(ctx, operation);
	if (!current || classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t evidence_constraint_id = PROTOTYPE_INVALID_ID;
	if (ctx->active_classifier_constraint_id <
			ctx->constraints.constraint_count) {
		const struct operation_constraint* evidence =
			&ctx->constraints.constraints[ctx->active_classifier_constraint_id];
		if (evidence->domain == OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER &&
			evidence->payload.classifier.classifier_meta_id == operation) {
			evidence_constraint_id = evidence->id;
		}
	}
	if (current->classifier == PROTOTYPE_INVALID_ID) {
		return operation_classifier_meta_publish(
			ctx, operation, classifier, evidence_constraint_id
		);
	}
	int convertible = prototype_judgement_classifier_conversion(
		ctx->terms,
		ctx->type_declarations,
		current->classifier,
		classifier
	).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
	if (!convertible) {
		uint32_t solved_expected;
		int solve_status = prototype_judgement_solve_expected_effect_rows(
			ctx->terms,
			ctx->type_declarations,
			NULL,
			current->classifier,
			classifier,
			&solved_expected
		);
		if (solve_status < 0) {
			return -1;
		}
		if (solve_status == 0) {
			return operation_classifier_meta_publish(
				ctx,
				operation,
				solved_expected,
				evidence_constraint_id
			);
		}
	}
	if (!convertible &&
		current->evidence_constraint_id == PROTOTYPE_INVALID_ID &&
		evidence_constraint_id != PROTOTYPE_INVALID_ID) {
		/* A propagated expectation may provisionally close a meta before the
		 * occurrence's own constraint is ready. The direct constraint is the first
		 * authoritative equation for that meta, so replace only an unevidenced
		 * provisional candidate and invalidate its derived dependents. */
		if (operation_classifier_meta_invalidate(ctx, operation) != 0) {
			return -1;
		}
		return operation_classifier_meta_publish(
			ctx, operation, classifier, evidence_constraint_id
		);
	}
	return convertible ? 0 : -1;
}

static uint32_t operation_solver_binder_classifier(
	const struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || !ctx->metadata || operation >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return PROTOTYPE_INVALID_ID;
	}
	const struct operation_classifier_meta_solution* meta =
		&ctx->classifier_solver.classifier_metas[operation];
	if (meta->owner_occurrence == operation &&
		meta->binder_classifier != PROTOTYPE_INVALID_ID) {
		return meta->binder_classifier;
	}
	if (ctx->classifier_seeds[operation].owner_occurrence == operation &&
		ctx->classifier_seeds[operation].binder_classifier !=
			PROTOTYPE_INVALID_ID) {
		return ctx->classifier_seeds[operation].binder_classifier;
	}
	return ctx->metadata->typed_occurrences.occurrences[
		operation
	].binder_classifier;
}

static int operation_binder_classifier_meta_publish(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int replace,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed || operation >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* occurrence =
		&ctx->metadata->typed_occurrences.occurrences[operation];
	struct operation_classifier_meta_solution* meta =
		&ctx->classifier_solver.classifier_metas[operation];
	if (occurrence->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
		meta->owner_occurrence != operation ||
		meta->binder_state == OPERATION_CLASSIFIER_META_FROZEN) {
		return -1;
	}
	uint32_t current = operation_solver_binder_classifier(ctx, operation);
	if (current != PROTOTYPE_INVALID_ID) {
		int compatible = prototype_judgement_classifier_reference_equal(
			ctx->terms, ctx->type_declarations, current, classifier
		) || prototype_judgement_classifier_compatible(
			ctx->terms, ctx->type_declarations, current, classifier
		);
		if (compatible && !replace) {
			if (meta->binder_classifier == PROTOTYPE_INVALID_ID) {
				meta->binder_classifier = current;
				meta->binder_state = OPERATION_CLASSIFIER_META_SOLVED;
			}
			return 0;
		}
		if (!compatible && !replace) {
			return -1;
		}
		if (current == classifier) {
			return 0;
		}
	}
	uint32_t evidence_constraint_id = PROTOTYPE_INVALID_ID;
	if (ctx->active_classifier_constraint_id <
		ctx->constraints.constraint_count) {
		const struct operation_constraint* evidence =
			&ctx->constraints.constraints[ctx->active_classifier_constraint_id];
		if (evidence->domain == OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER) {
			evidence_constraint_id = evidence->id;
		}
	}
	meta->binder_classifier = classifier;
	meta->binder_evidence_constraint_id = evidence_constraint_id;
	meta->binder_state = OPERATION_CLASSIFIER_META_SOLVED;
	operation_context_binding_dependency_changed(ctx, occurrence->binding_id);
	ctx->classifier_solver.solution_revision++;
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, operation);
}

static int operation_binder_classifier_meta_solve(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
) {
	return operation_binder_classifier_meta_publish(
		ctx, operation, classifier, 0, p_changed
	);
}

static int operation_binder_classifier_meta_replace(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
) {
	return operation_binder_classifier_meta_publish(
		ctx, operation, classifier, 1, p_changed
	);
}

/* Context pullback is an atomic replacement of the solver invocation, not a
 * normal equation refinement. Install the reindexed seeds and meta values only
 * after the caller has transformed the complete reachable occurrence graph. */
static int operation_type_metas_reset_after_context_action(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t seed_classifier,
	uint32_t classifier,
	uint32_t binder_classifier
) {
	if (!ctx || !ctx->metadata || operation >=
		ctx->metadata->typed_occurrences.occurrence_count ||
		(seed_classifier != PROTOTYPE_INVALID_ID &&
		 seed_classifier >= ctx->terms->term_count) ||
		(classifier != PROTOTYPE_INVALID_ID && classifier >= ctx->terms->term_count) ||
		(binder_classifier != PROTOTYPE_INVALID_ID &&
		 binder_classifier >= ctx->terms->term_count)) {
		return -1;
	}
	struct operation_classifier_meta_solution* meta =
		&ctx->classifier_solver.classifier_metas[operation];
	if (meta->owner_occurrence != operation ||
		meta->state == OPERATION_CLASSIFIER_META_FROZEN ||
		meta->binder_state == OPERATION_CLASSIFIER_META_FROZEN) {
		return -1;
	}
	ctx->classifier_seeds[operation].classifier = seed_classifier;
	ctx->classifier_seeds[operation].binder_classifier = binder_classifier;
	meta->classifier = classifier;
	meta->stale_classifier = PROTOTYPE_INVALID_ID;
	meta->evidence_constraint_id = PROTOTYPE_INVALID_ID;
	meta->state = classifier == PROTOTYPE_INVALID_ID ?
		OPERATION_CLASSIFIER_META_UNSOLVED : OPERATION_CLASSIFIER_META_SOLVED;
	meta->binder_classifier = binder_classifier;
	meta->binder_evidence_constraint_id = PROTOTYPE_INVALID_ID;
	meta->binder_state = binder_classifier == PROTOTYPE_INVALID_ID ?
		OPERATION_CLASSIFIER_META_UNSOLVED : OPERATION_CLASSIFIER_META_SOLVED;
	ctx->classifier_solver.solution_revision++;
	return 0;
}

static int operation_type_metas_discard_mutable_occurrence(
	struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || !ctx->metadata || operation >=
		ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	struct operation_classifier_meta_solution* meta =
		&ctx->classifier_solver.classifier_metas[operation];
	if (meta->owner_occurrence != operation ||
		meta->state == OPERATION_CLASSIFIER_META_FROZEN ||
		meta->binder_state == OPERATION_CLASSIFIER_META_FROZEN) {
		return -1;
	}
	ctx->classifier_seeds[operation].classifier = PROTOTYPE_INVALID_ID;
	ctx->classifier_seeds[operation].binder_classifier = PROTOTYPE_INVALID_ID;
	ctx->classifier_seeds[operation].reason = OPERATION_CLASSIFIER_SEED_NONE;
	meta->classifier = PROTOTYPE_INVALID_ID;
	meta->stale_classifier = PROTOTYPE_INVALID_ID;
	meta->evidence_constraint_id = PROTOTYPE_INVALID_ID;
	meta->state = OPERATION_CLASSIFIER_META_UNSOLVED;
	meta->binder_classifier = PROTOTYPE_INVALID_ID;
	meta->binder_evidence_constraint_id = PROTOTYPE_INVALID_ID;
	meta->binder_state = OPERATION_CLASSIFIER_META_UNSOLVED;
	return 0;
}

/* Computation effect discovery is the only non-convertible monotone update.
 * Its callers must first prove that the result preserves the value classifier
 * and only widens the effect row. */
static int operation_classifier_meta_widen(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier
) {
	const struct operation_classifier_meta_solution* current =
		operation_classifier_meta_get(ctx, operation);
	if (!current || current->classifier == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	return operation_classifier_meta_publish(
		ctx,
		operation,
		classifier,
		current->evidence_constraint_id
	);
}

static int operation_solver_classifier_in_occurrence_context(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t classifier,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !p_classifier ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	*p_classifier = classifier;
	if (operation->context_action_substitution == PROTOTYPE_INVALID_ID ||
		operation->source_classifier == PROTOTYPE_INVALID_ID ||
		operation->source_classifier >= ctx->terms->term_count ||
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			classifier,
			operation->source_classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 0;
	}
	const struct prototype_substitution* action = prototype_substitution_get(
		&ctx->metadata->substitutions,
		operation->context_action_substitution
	);
	if (!action || action->source_context != operation->context_id) {
		return -1;
	}
	return prototype_term_reindex(
		ctx->terms,
		ctx->type_declarations,
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		classifier,
		operation->context_action_substitution,
		p_classifier
	);
}

static int operation_solver_bind(
	struct compile_context* ctx,
	uint32_t variable,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		variable >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t contextual_classifier;
	if (operation_solver_classifier_in_occurrence_context(
			ctx, variable, classifier, &contextual_classifier
		) != 0) {
		return -1;
	}
	classifier = contextual_classifier;
	uint32_t previous = ctx->classifier_solver.classifier_metas[variable].classifier;
	if (previous == PROTOTYPE_INVALID_ID) {
		if (operation_classifier_meta_solve(ctx, variable, classifier) != 0) {
			return -1;
		}
		*p_changed = 1;
		return operation_solver_enqueue_dependents(ctx, variable);
	}
	if (!(prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL)) {
		fprintf(stderr,
			"classifier solver binding conflict operation=%u tag=%u core=%u "
			"previous=%u candidate=%u source-ast=%u context=%u binding=%u\n",
			variable,
			ctx->metadata->typed_occurrences.occurrences[variable].tag,
			ctx->metadata->typed_occurrences.occurrences[variable].core_term,
			previous, classifier,
			ctx->metadata->typed_occurrences.occurrences[variable].source_ast,
			ctx->metadata->typed_occurrences.occurrences[variable].context_id,
			ctx->metadata->typed_occurrences.occurrences[variable].binding_id);
		fprintf(stderr, "previous classifier: ");
		prototype_term_print_debug(
			stderr, ctx->symbols, ctx->intrinsic_environment,
			ctx->type_declarations, ctx->terms, previous
		);
		fprintf(stderr, "\ncandidate classifier: ");
		prototype_term_print_debug(
			stderr, ctx->symbols, ctx->intrinsic_environment,
			ctx->type_declarations, ctx->terms, classifier
		);
		fprintf(stderr, "\n");
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
	/* A lone row variable in an occurrence solution is the application-site
	 * metavariable introduced while instantiating EFFECT_ROW_FORALL.  Once the
	 * argument supplies a concrete row, solve that cell instead of retaining the
	 * metavariable as if it were an observable effect atom.  Quantified rows in
	 * the function classifier itself remain unchanged. */
	if (before_row.atom_count == 1 &&
		before_row.atoms[0] < ctx->terms->term_count &&
		ctx->terms->terms[before_row.atoms[0]].tag ==
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		*p_merged = candidate;
		return 0;
	}
	if (after_row.atom_count == 1 &&
		after_row.atoms[0] < ctx->terms->term_count &&
		ctx->terms->terms[after_row.atoms[0]].tag ==
			PROTOTYPE_TERM_EFFECT_ROW_VAR) {
		*p_merged = previous;
		return 0;
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
	int totality = prototype_computation_totality_join(
		before.totality, after.totality
	);
	if (totality == PROTOTYPE_COMPUTATION_TOTALITY_UNKNOWN) {
		return 1;
	}
	return prototype_term_effect_row_materialize_normal_form(
			ctx->terms, &merged, &row
		) != 0 ? -1 : prototype_term_computation_type(
			ctx->terms, row, before.result, totality, p_merged
		);
}

static int operation_classifier_is_effect_refinement(
	struct compile_context* ctx,
	uint32_t before,
	uint32_t after,
	uint32_t depth
) {
	if (!ctx || before >= ctx->terms->term_count ||
		after >= ctx->terms->term_count || depth > 64) {
		return 0;
	}
	if (prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, before, after
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	const struct prototype_term* before_term = &ctx->terms->terms[before];
	const struct prototype_term* after_term = &ctx->terms->terms[after];
	if (before_term->tag == PROTOTYPE_TERM_THUNK_TYPE &&
		after_term->tag == PROTOTYPE_TERM_THUNK_TYPE) {
		return operation_classifier_is_effect_refinement(
			ctx,
			before_term->as.thunk_type.computation,
			after_term->as.thunk_type.computation,
			depth + 1
		);
	}
	uint32_t before_domain;
	uint32_t before_family;
	uint32_t after_domain;
	uint32_t after_family;
	if (prototype_judgement_pi_parts(
			ctx->terms, before, &before_domain, &before_family
		) == 0 && prototype_judgement_pi_parts(
			ctx->terms, after, &after_domain, &after_family
		) == 0) {
		uint32_t before_binder;
		uint32_t before_body;
		uint32_t after_binder;
		uint32_t after_body;
		return operation_classifier_is_effect_refinement(
				ctx, before_domain, after_domain, depth + 1
			) && prototype_term_pure_family_parts(
				ctx->terms, before_family, &before_binder, &before_body
			) == 0 && prototype_term_pure_family_parts(
				ctx->terms, after_family, &after_binder, &after_body
			) == 0 && operation_classifier_is_effect_refinement(
				ctx, before_body, after_body, depth + 1
			);
	}
	struct prototype_term_classifier_view before_view;
	struct prototype_term_classifier_view after_view;
	if (prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, before, &before_view
		) != 0 || prototype_judgement_classifier_view(
			ctx->terms, ctx->type_declarations, NULL, after, &after_view
		) != 0 || before_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		after_view.category != PROTOTYPE_TERM_CATEGORY_COMPUTATION ||
		before_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		after_view.computation_kind != PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING ||
		before_view.totality != after_view.totality ||
		prototype_judgement_classifier_conversion(
			ctx->terms,
			ctx->type_declarations,
			before_view.result,
			after_view.result
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
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
	/* Equal normalized rows are the same solution, even when their Term syntax
	 * contains a redundant UNION. Preserve the existing representative; only a
	 * proper set inclusion is a monotone widening. */
	return prototype_term_effect_row_normal_form_includes(
			ctx->terms, &after_row, &before_row
		) && !prototype_term_effect_row_normal_form_includes(
			ctx->terms, &before_row, &after_row
		);
}

static int operation_solver_bind_proven_classifier(
	struct compile_context* ctx,
	uint32_t variable,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		variable >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier == PROTOTYPE_INVALID_ID || classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t contextual_classifier;
	if (operation_solver_classifier_in_occurrence_context(
			ctx, variable, classifier, &contextual_classifier
		) != 0) {
		return -1;
	}
	classifier = contextual_classifier;
	if (ctx->classifier_solver.classifier_metas[variable].classifier == classifier) {
		return 0;
	}
	uint32_t previous = ctx->classifier_solver.classifier_metas[variable].classifier;
	if (previous != PROTOTYPE_INVALID_ID &&
		prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, previous, classifier
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
		/* The fixed-point cell already denotes this classifier. Preserve its
		 * representative so alpha-equivalent reconstruction cannot requeue the
		 * same dependency cycle indefinitely. */
		return 0;
	}
	const struct operation_classifier_meta_solution* current =
		operation_classifier_meta_get(ctx, variable);
	if (previous != PROTOTYPE_INVALID_ID && current &&
		current->evidence_constraint_id == PROTOTYPE_INVALID_ID) {
		/* Rule-specific computation solving replaces an elaboration seed. In
		 * particular, a higher-order request refines op<empty> to op<latent>;
		 * unioning those parameterized atoms would invent two requests. */
		if (operation_classifier_meta_invalidate(ctx, variable) != 0 ||
			operation_classifier_meta_publish(
				ctx, variable, classifier, PROTOTYPE_INVALID_ID
			) != 0) {
			return -1;
		}
		*p_changed = 1;
		return operation_solver_enqueue_dependents(ctx, variable);
	}
	uint32_t merged = PROTOTYPE_INVALID_ID;
	int merged_computation = previous != PROTOTYPE_INVALID_ID &&
		operation_solver_merge_computation_classifiers(
			ctx, previous, classifier, &merged
		) == 0;
	if (previous != PROTOTYPE_INVALID_ID &&
		operation_classifier_is_effect_refinement(
			ctx, previous, classifier, 0
		)) {
		if (operation_classifier_meta_widen(
				ctx, variable, classifier
			) != 0) {
			return -1;
		}
	} else if (merged_computation) {
		if (prototype_judgement_classifier_conversion(
				ctx->terms, ctx->type_declarations, previous, merged
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return 0;
		}
		if (operation_classifier_meta_widen(ctx, variable, merged) != 0) {
			return -1;
		}
	} else if (operation_classifier_meta_solve(
			ctx, variable, classifier
		) != 0) {
		return -1;
	}
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, variable);
}

static int operation_solver_widen_computation_binding(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !p_changed || operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.classifier_metas[operation].classifier;
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
	if (operation_classifier_meta_widen(ctx, operation, merged) != 0) {
		return -1;
	}
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, operation);
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
		ctx->terms, &after_row, &before_row
	);
}

static int operation_solver_widen_continuation_binding(
	struct compile_context* ctx,
	uint32_t operation,
	uint32_t classifier,
	int* p_changed
) {
	if (!ctx || !p_changed || operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		classifier >= ctx->terms->term_count) {
		return -1;
	}
	uint32_t previous = ctx->classifier_solver.classifier_metas[operation].classifier;
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
	if (operation_classifier_meta_widen(ctx, operation, classifier) != 0) {
		return -1;
	}
	*p_changed = 1;
	return operation_solver_enqueue_dependents(ctx, operation);
}

static uint32_t operation_solver_classifier(
	const struct compile_context* ctx,
	uint32_t operation
) {
	if (!ctx || !ctx->metadata || operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return PROTOTYPE_INVALID_ID;
	}
	if (ctx->classifier_solver.classifier_metas[operation].classifier !=
		PROTOTYPE_INVALID_ID) {
		return ctx->classifier_solver.classifier_metas[operation].classifier;
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
	if (motive_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return PROTOTYPE_INVALID_ID;
	}
	/* A constant candidate is a solver-side equation M(_) == T, so it can
	 * discharge the classifier query without pretending that M(argument) has
	 * already been built in TermDB. */
	return ctx->classifier_solver.motive_solutions[motive_operation].constant_candidate;
}

/* Solve a constructor APP from its dependent telescope. The curried cache is
 * used only to describe a residual, under-applied builder; a saturated value's
 * classifier is reindexed directly from the authoritative result schema. */
enum operation_solver_constructor_spine_status {
	OPERATION_SOLVER_CONSTRUCTOR_SPINE_SOLVED = 0,
	OPERATION_SOLVER_CONSTRUCTOR_SPINE_WAITING = 1,
	OPERATION_SOLVER_CONSTRUCTOR_SPINE_CONTRADICTION = 2
};

struct operation_solver_constructor_mismatch {
	uint32_t expected_classifier;
	uint32_t actual_classifier;
	uint32_t context_id;
	uint32_t constructor_ordinal;
	uint32_t field_ordinal;
};

/* Constructor-domain equations may refine the result family of an applied
 * guarded IH. Identify that source of recursion in the operation graph rather
 * than recognizing only a bare IH node. Distinct IH occurrences are
 * ambiguous: an expected result may not choose between multiple recursive
 * calls. */
static int operation_subgraph_unique_induction_hypothesis(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t depth,
	uint32_t* p_ih_operation
) {
	if (!ctx || !ctx->metadata || !p_ih_operation || operation_id >=
			ctx->metadata->typed_occurrences.occurrence_count || depth > 128) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
		if (*p_ih_operation != PROTOTYPE_INVALID_ID &&
			*p_ih_operation != operation_id) {
			return 2;
		}
		*p_ih_operation = operation_id;
		return 0;
	}
	/* EXPECTED_TYPE is a local post-check wrapper. It has no structural Core
	 * child, but its wrapped occurrence remains part of the same synthesized
	 * computation. Recursive-result refinement must inspect that occurrence
	 * without treating the expected classifier as synthesis input. */
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		int status = operation_subgraph_unique_induction_hypothesis(
			ctx, operation->wrapped_occurrence, depth + 1, p_ih_operation
		);
		if (status != 0) {
			return status;
		}
	}
	if (operation->edge_count == 0) {
		return 0;
	}
	if (operation->first_edge == PROTOTYPE_INVALID_ID ||
		operation->first_edge > ctx->metadata->typed_occurrences.edge_count ||
		operation->edge_count > ctx->metadata->typed_occurrences.edge_count -
			operation->first_edge) {
		return -1;
	}
	for (uint32_t i = 0; i < operation->edge_count; ++i) {
		uint32_t child = ctx->metadata->typed_occurrences.edges[
			operation->first_edge + i
		].child_occurrence;
		int status = operation_subgraph_unique_induction_hypothesis(
			ctx, child, depth + 1, p_ih_operation
		);
		if (status != 0) {
			return status;
		}
	}
	return 0;
}

static int operation_solver_refine_sequenced_ih_result(
	struct compile_context* ctx,
	uint32_t value_operation,
	uint32_t expected_result,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		value_operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		expected_result >= ctx->terms->term_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* value =
		&ctx->metadata->typed_occurrences.occurrences[value_operation];
	if (value->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
		value->referenced_ast_binder_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	for (uint32_t fold_id = 0; fold_id < ctx->metadata->typed_occurrences.occurrence_count; ++fold_id) {
		const struct prototype_typed_occurrence* fold =
			&ctx->metadata->typed_occurrences.occurrences[fold_id];
		uint32_t input_occurrence;
		uint32_t continuation_occurrence;
		if (fold->tag != PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD ||
			compile_occurrence_child(
				ctx, fold_id, PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
				&input_occurrence
			) != 0 || compile_occurrence_child(
				ctx, fold_id, PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
				&continuation_occurrence
			) != 0 ||
			fold->core_term >= ctx->terms->term_count ||
			ctx->terms->terms[fold->core_term].tag !=
				PROTOTYPE_TERM_COMPUTATION_FOLD ||
			ctx->terms->terms[fold->core_term].as.computation_fold.clause_count != 0) {
			continue;
		}
		const struct prototype_typed_occurrence* continuation =
			&ctx->metadata->typed_occurrences.occurrences[continuation_occurrence];
		const struct prototype_typed_occurrence* input =
			&ctx->metadata->typed_occurrences.occurrences[input_occurrence];
		uint32_t guarded_ih = PROTOTYPE_INVALID_ID;
		int guarded_ih_status = operation_subgraph_unique_induction_hypothesis(
			ctx, input_occurrence, 0, &guarded_ih
		);
		if (guarded_ih_status < 0) {
			return -1;
		}
		if (continuation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA ||
			continuation->referenced_ast_binder_id !=
				value->referenced_ast_binder_id ||
			guarded_ih_status != 0 || guarded_ih == PROTOTYPE_INVALID_ID ||
			ctx->metadata->typed_occurrences.occurrences[
				guarded_ih
			].ih_owner_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
			continue;
		}
		uint32_t input_classifier = operation_solver_classifier(
			ctx, input_occurrence
		);
		struct prototype_term_classifier_view input_view;
		if (input_classifier == PROTOTYPE_INVALID_ID) {
			uint32_t row;
			uint32_t seeded_classifier;
			/* Materializing an induction hypothesis constructs a recursive
			 * function value; it does not execute that function's latent effects. */
			if (prototype_term_effect_row_empty(ctx->terms, &row) != 0 ||
				prototype_term_total_computation_type(
					ctx->terms, row, expected_result, &seeded_classifier
				) != 0) {
				return -1;
			}
			int refine_status = operation_refine_classifier_from_expected(
				ctx, input_occurrence, seeded_classifier, 1, p_changed
			);
			uint32_t current_value = operation_solver_classifier(
				ctx, value_operation
			);
			if (current_value != PROTOTYPE_INVALID_ID &&
				prototype_judgement_classifier_conversion(
					ctx->terms,
					ctx->type_declarations,
					current_value,
					expected_result
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL &&
				operation_classifier_meta_invalidate(
					ctx, value_operation
				) != 0) {
				return -1;
			}
			if (refine_status < 0 || operation_classifier_meta_solve(
					ctx, input_occurrence, seeded_classifier
				) != 0 || operation_classifier_meta_solve(
					ctx, value_operation, expected_result
				) != 0) {
				return -1;
			}
			if (operation_solver_binder_classifier(
					ctx, continuation_occurrence
				) != expected_result &&
				operation_binder_classifier_meta_replace(
					ctx, continuation_occurrence, expected_result, p_changed
				) != 0) {
				return -1;
			}
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(
					ctx, input_occurrence
				) != 0 || operation_solver_enqueue_dependents(
					ctx, fold_id
				) != 0) {
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
		if (input->tag != PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			uint32_t expected_input_classifier;
			/* An unapplied recursive function still returns a PI. Propagating a
			 * non-PI expectation through that spine would reinterpret an ordinary
			 * argument as the missing final argument and corrupt its binding type. */
			if ((ctx->terms->terms[input_view.result].tag == PROTOTYPE_TERM_PI) !=
				(ctx->terms->terms[expected_result].tag == PROTOTYPE_TERM_PI)) {
				return 1;
			}
			if (prototype_term_computation_type(
					ctx->terms, input_view.effect_row, expected_result,
					input_view.totality,
					&expected_input_classifier
				) != 0) {
				return -1;
			}
			int refine_status = operation_refine_classifier_from_expected(
				ctx, input_occurrence, expected_input_classifier, 1, p_changed
			);
			if (refine_status < 0) {
				return -1;
			}
			input_classifier = operation_solver_classifier(ctx, input_occurrence);
			if (input_classifier < ctx->terms->term_count &&
				prototype_judgement_classifier_view(
					ctx->terms, ctx->type_declarations, NULL,
					input_classifier, &input_view
				) == 0 && input_view.category ==
					PROTOTYPE_TERM_CATEGORY_COMPUTATION &&
				input_view.computation_kind ==
					PROTOTYPE_TERM_COMPUTATION_KIND_RETURNING &&
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					input_view.result, expected_result
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				uint32_t current_value = operation_solver_classifier(
					ctx, value_operation
				);
				if (current_value != PROTOTYPE_INVALID_ID &&
					prototype_judgement_classifier_conversion(
						ctx->terms, ctx->type_declarations,
						current_value, expected_result
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL &&
					operation_classifier_meta_invalidate(
						ctx, value_operation
					) != 0) {
					return -1;
				}
				if (operation_classifier_meta_solve(
						ctx, value_operation, expected_result
					) != 0) {
					return -1;
				}
				if (operation_binder_classifier_meta_replace(
						ctx, continuation_occurrence, expected_result, p_changed
					) != 0) {
					return -1;
				}
				*p_changed = 1;
				if (operation_solver_enqueue_dependents(
						ctx, input_occurrence
					) != 0 || operation_solver_enqueue_dependents(
						ctx, fold_id
					) != 0) {
					return -1;
				}
				return 0;
			}
			return 1;
		}
		if (prototype_judgement_classifier_conversion(
				ctx->terms,
				ctx->type_declarations,
				input_view.result,
				expected_result
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
			/* All three projections belong to one sequence equation:
			 *
			 *   input : Comp(E, A), continuation binder x : A, body use x : A.
			 *
			 * Context rebuilding may invalidate or restore either binder projection
			 * after the IH input has already reached A. Reassert those consequences
			 * before declaring the equation stable; otherwise a stale provisional
			 * classifier can survive solely because the input is current. */
			int synchronized = 0;
			uint32_t value_classifier = operation_solver_classifier(
				ctx, value_operation
			);
			if (value_classifier == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					value_classifier, expected_result
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				if (value_classifier != PROTOTYPE_INVALID_ID &&
					operation_classifier_meta_invalidate(
						ctx, value_operation
					) != 0) {
					return -1;
				}
				if (operation_classifier_meta_solve(
						ctx, value_operation, expected_result
					) != 0) {
					return -1;
				}
				synchronized = 1;
			}
			uint32_t binder_classifier = operation_solver_binder_classifier(
				ctx, continuation_occurrence
			);
			if (binder_classifier == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					binder_classifier, expected_result
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				if (operation_binder_classifier_meta_replace(
						ctx, continuation_occurrence, expected_result, p_changed
					) != 0) {
					return -1;
				}
				synchronized = 1;
			}
			if (!synchronized) {
				return 1;
			}
			*p_changed = 1;
			if (operation_solver_enqueue_dependents(
					ctx, value_operation
				) != 0 || operation_solver_enqueue_dependents(
					ctx, continuation_occurrence
				) != 0 || operation_solver_enqueue_dependents(
					ctx, fold_id
				) != 0) {
				return -1;
			}
			return 0;
		}
		uint32_t refined_classifier;
		if (prototype_term_computation_type(
				ctx->terms, input_view.effect_row, expected_result,
				input_view.totality,
				&refined_classifier
			) != 0) {
			return -1;
		}
		/* A sequence continuation domain contributes an equation for the recursive
		 * motive. It is not an authority for the IH classifier, its bound value, or
		 * the sequence binder; those projections follow after M is selected. */
		int refine_status = operation_refine_classifier_from_expected(
			ctx, input_occurrence, refined_classifier, 1, p_changed
		);
		if (refine_status < 0) {
			return -1;
		}
		return 0;
	}
	return 1;
}

/* Constructor typing is local to the continuation Context. Its fields are the
 * actual binder terms in that Context; prior computation substitutes those
 * binders only when deriving the enclosing fold result. */
static int operation_solver_constructor_semantic_spine(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t* p_subject
) {
	if (!ctx || !ctx->metadata || !p_subject || operation_id >=
			ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t reverse_arguments[64];
	uint32_t argument_count = 0;
	uint32_t cursor = operation_id;
	while (cursor < ctx->metadata->typed_occurrences.occurrence_count) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[cursor];
		uint32_t function_operation;
		uint32_t argument_occurrence;
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
			operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			break;
		}
		if (argument_count >= 64 || compile_occurrence_child(
				ctx, cursor, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
				&function_operation
			) != 0 || compile_occurrence_child(
				ctx, cursor, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
				&argument_occurrence
			) != 0) {
			return -1;
		}
		reverse_arguments[argument_count++] = argument_occurrence;
		cursor = function_operation;
	}
	const struct prototype_typed_occurrence* source =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	uint32_t constructor_head;
	uint32_t constructor_owner;
	uint32_t constructor_index;
	uint32_t source_fields[64];
	uint32_t source_field_count;
	if (prototype_term_constructor_spine_info(
			ctx->terms, source->core_term, &constructor_head,
			&constructor_owner, &constructor_index, source_fields, 64,
			&source_field_count
		) != 0 || source_field_count != argument_count) {
		return -1;
	}
	uint32_t semantic_subject = constructor_head;
	for (uint32_t i = 0; i < argument_count; ++i) {
		uint32_t argument_occurrence =
			reverse_arguments[argument_count - i - 1];
		uint32_t semantic_argument = ctx->metadata->typed_occurrences.occurrences[
			argument_occurrence
		].core_term;
		if (semantic_argument >= ctx->terms->term_count || prototype_term_app(
				ctx->terms, semantic_subject, semantic_argument,
				&semantic_subject
			) != 0) {
			return -1;
		}
	}
	(void)constructor_owner;
	(void)constructor_index;
	*p_subject = semantic_subject;
	return 0;
}

static int operation_solver_project_pure_sequence_results(
	struct compile_context* ctx,
	uint32_t classifier,
	uint32_t* p_projected
);

static int operation_solver_propagate_constructor_domains(
	struct compile_context* ctx,
	uint32_t operation_id,
	int* p_changed
) {
	if (!ctx || !ctx->metadata || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	uint32_t reverse_arguments[64];
	uint32_t argument_count = 0;
	uint32_t cursor = operation_id;
	while (cursor < ctx->metadata->typed_occurrences.occurrence_count) {
		const struct prototype_typed_occurrence* operation =
			&ctx->metadata->typed_occurrences.occurrences[cursor];
		uint32_t function_operation;
		uint32_t argument_occurrence;
		if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
			operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			break;
		}
		if (argument_count >= 64 || compile_occurrence_child(
				ctx, cursor, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
				&function_operation
			) != 0 || compile_occurrence_child(
				ctx, cursor, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
				&argument_occurrence
			) != 0) {
			return -1;
		}
		reverse_arguments[argument_count++] = argument_occurrence;
		cursor = function_operation;
	}
	uint32_t expected_domains[64];
	uint32_t expected_domain_count = 0;
	uint32_t semantic_subject;
	if (operation_solver_constructor_semantic_spine(
			ctx, operation_id, &semantic_subject
		) != 0) {
		return -1;
	}
	if (prototype_judgement_constructor_spine_expected_domains(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			ctx->metadata->typed_occurrences.occurrences[operation_id].context_id,
			semantic_subject,
			PROTOTYPE_INVALID_ID,
			expected_domains,
			64,
			&expected_domain_count
		) != 0 || expected_domain_count != argument_count) {
		fprintf(stderr,
			"constructor propagation domains failed operation=%u context=%u core=%u "
			"arguments=%u expected=%u\n",
			operation_id, ctx->metadata->typed_occurrences.occurrences[operation_id].context_id,
			ctx->metadata->typed_occurrences.occurrences[operation_id].core_term, argument_count,
			expected_domain_count);
		return -1;
	}
	for (uint32_t i = 0; i < argument_count; ++i) {
		uint32_t argument_occurrence = reverse_arguments[argument_count - i - 1];
		uint32_t expected = expected_domains[i];
		if (operation_solver_reindex_in_enclosing_indexed_branches(
				ctx,
				argument_occurrence,
				ctx->metadata->typed_occurrences.occurrences[operation_id].context_id,
				expected,
				&expected
			) != 0) {
			return -1;
		}
		uint32_t actual = operation_solver_classifier(ctx, argument_occurrence);
		uint32_t motive_expected = expected;
		int projection_status = operation_solver_project_pure_sequence_results(
			ctx, expected, &motive_expected
		);
		if (projection_status < 0) {
			return -1;
		}
		if (projection_status > 0) {
			motive_expected = expected;
		}
		uint32_t projected_actual = actual;
		if (actual != PROTOTYPE_INVALID_ID) {
			projection_status = operation_solver_project_pure_sequence_results(
				ctx, actual, &projected_actual
			);
			if (projection_status < 0) {
				return -1;
			}
			if (projection_status > 0) {
				projected_actual = actual;
			}
			if (prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations,
					projected_actual, motive_expected
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				continue;
			}
		}
		int expected_refinement = operation_refine_classifier_from_expected(
			ctx, argument_occurrence, motive_expected, 0, p_changed
		);
		if (expected_refinement < 0) {
			fprintf(stderr,
				"constructor propagation expected refinement failed operation=%u "
				"argument=%u expected=%u actual=%u\n",
				operation_id, argument_occurrence, expected, actual);
			return -1;
		}
		actual = operation_solver_classifier(ctx, argument_occurrence);
		int ih_refinement = operation_solver_refine_sequenced_ih_result(
			ctx, argument_occurrence, expected, p_changed
		);
		if (ih_refinement < 0) {
			fprintf(stderr,
				"constructor propagation IH refinement failed operation=%u argument=%u\n",
				operation_id, argument_occurrence);
			return -1;
		}
		if (ih_refinement == 0) {
			return 0;
		}
		if (ctx->metadata->typed_occurrences.occurrences[argument_occurrence].tag ==
				PROTOTYPE_TYPED_OCCURRENCE_INDUCTION_HYPOTHESIS) {
			if (actual == PROTOTYPE_INVALID_ID ||
				prototype_judgement_classifier_conversion(
					ctx->terms, ctx->type_declarations, actual, expected
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				/* The expected constructor field has already been offered to the motive
				 * solver above. Wait for the owner Match to materialize this IH instead of
				 * publishing the expected field as a second classifier authority. */
				return 0;
			}
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

static int operation_subgraph_has_pending_classifier(
	const struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t depth
) {
	if (!ctx || !ctx->metadata ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count || depth > 256) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (operation_solver_classifier(ctx, operation_id) ==
		PROTOTYPE_INVALID_ID) {
		return 1;
	}
	if (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
		operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
		if (operation->wrapped_occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		int pending = operation_subgraph_has_pending_classifier(
			ctx, operation->wrapped_occurrence, depth + 1
		);
		if (pending != 0) {
			return pending;
		}
	}
	if (operation->edge_count == 0) {
		return 0;
	}
	if (operation->first_edge == PROTOTYPE_INVALID_ID ||
		operation->first_edge > ctx->metadata->typed_occurrences.edge_count ||
		operation->edge_count >
			ctx->metadata->typed_occurrences.edge_count - operation->first_edge) {
		return -1;
	}
	for (uint32_t i = 0; i < operation->edge_count; ++i) {
		uint32_t child = ctx->metadata->typed_occurrences.edges[
			operation->first_edge + i
		].child_occurrence;
		if (child >= ctx->metadata->typed_occurrences.occurrence_count) {
			return -1;
		}
		int pending = operation_subgraph_has_pending_classifier(
			ctx, child, depth + 1
		);
		if (pending != 0) {
			return pending;
		}
	}
	return 0;
}

static int operation_solver_constructor_spine_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t* p_classifier,
	int* p_changed,
	struct operation_solver_constructor_mismatch* p_mismatch
) {
	if (!ctx || !ctx->metadata || !p_classifier || !p_changed ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
	if (p_mismatch) {
		*p_mismatch = (struct operation_solver_constructor_mismatch) {
			.expected_classifier = PROTOTYPE_INVALID_ID,
			.actual_classifier = PROTOTYPE_INVALID_ID,
			.context_id = operation->context_id,
			.constructor_ordinal = PROTOTYPE_INVALID_ID,
			.field_ordinal = PROTOTYPE_INVALID_ID
		};
	}
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
		operation->application_role !=
			PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
		return -1;
	}
	uint32_t reverse_argument_occurrences[64];
	uint32_t argument_occurrence_count = 0;
	uint32_t cursor = operation_id;
	while (cursor < ctx->metadata->typed_occurrences.occurrence_count) {
		const struct prototype_typed_occurrence* cursor_operation =
			&ctx->metadata->typed_occurrences.occurrences[cursor];
		uint32_t function_operation;
		uint32_t argument_occurrence;
		if (cursor_operation->tag != PROTOTYPE_TYPED_OCCURRENCE_APP ||
			cursor_operation->application_role !=
				PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
			break;
		}
		if (argument_occurrence_count >= 64 || compile_occurrence_child(
				ctx, cursor, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
				&function_operation
			) != 0 || compile_occurrence_child(
				ctx, cursor, PROTOTYPE_TERM_CHILD_ARGUMENT, 0,
				&argument_occurrence
			) != 0) {
			return -1;
		}
		reverse_argument_occurrences[argument_occurrence_count++] =
			argument_occurrence;
		cursor = function_operation;
	}
	uint32_t argument_classifiers[64];
	uint32_t expected_domains[64];
	uint32_t expected_domain_count;
	uint32_t semantic_subject;
	if (operation_solver_constructor_semantic_spine(
			ctx, operation_id, &semantic_subject
		) != 0) {
		return -1;
	}
	if (p_mismatch) {
		uint32_t ignored_head;
		uint32_t ignored_owner;
		uint32_t ignored_fields[64];
		uint32_t ignored_field_count;
		if (prototype_term_constructor_spine_info(
				ctx->terms, semantic_subject, &ignored_head, &ignored_owner,
				&p_mismatch->constructor_ordinal, ignored_fields, 64,
				&ignored_field_count
			) != 0) {
			p_mismatch->constructor_ordinal = PROTOTYPE_INVALID_ID;
		}
	}
	if (prototype_judgement_constructor_spine_expected_domains(
			ctx->terms,
			ctx->type_declarations,
			&ctx->metadata->contexts,
			&ctx->metadata->substitutions,
			operation->context_id,
			semantic_subject,
			PROTOTYPE_INVALID_ID,
			expected_domains,
			64,
			&expected_domain_count
		) != 0 || expected_domain_count != argument_occurrence_count) {
		return -1;
	}
	for (uint32_t i = 0; i < argument_occurrence_count; ++i) {
		uint32_t argument_occurrence =
			reverse_argument_occurrences[argument_occurrence_count - i - 1];
		uint32_t checked_expected;
		if (operation_solver_reindex_in_enclosing_indexed_branches(
				ctx,
				argument_occurrence,
				operation->context_id,
				expected_domains[i],
				&checked_expected
			) != 0) {
			return -1;
		}
		int projection_status = operation_solver_project_pure_sequence_results(
			ctx, checked_expected, &checked_expected
		);
		if (projection_status < 0) {
			return -1;
		}
		if (projection_status > 0) {
			if (operation_solver_reindex_in_enclosing_indexed_branches(
					ctx,
					argument_occurrence,
					operation->context_id,
					expected_domains[i],
					&checked_expected
				) != 0) {
				return -1;
			}
		}
		uint32_t argument_classifier =
			operation_solver_classifier(ctx, argument_occurrence);
		if (argument_classifier == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		uint32_t checked_actual = argument_classifier;
		projection_status = operation_solver_project_pure_sequence_results(
			ctx, argument_classifier, &checked_actual
		);
		if (projection_status < 0) {
			return -1;
		}
		if (projection_status > 0) {
			checked_actual = argument_classifier;
		}
		/* Domain conversion validates the root projection, but a computation-block
		 * result may still depend on an unresolved guarded IH. Propagate the same
		 * independently synthesized constructor-domain equation through the
		 * Operation graph before accepting the field. Ordinary closed values make
		 * this refinement a no-op. */
		int projected_equal = prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, checked_expected, checked_actual
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
		if (!projected_equal) {
			int ih_refinement = operation_solver_refine_sequenced_ih_result(
				ctx, argument_occurrence, checked_expected, p_changed
			);
			if (ih_refinement < 0) {
				return -1;
			}
			if (ih_refinement == 0) {
				if (*p_changed) {
					return OPERATION_SOLVER_CONSTRUCTOR_SPINE_WAITING;
				}
				projected_equal = 1;
			}
		}
		if (!projected_equal) {
			int expected_check = operation_refine_classifier_from_expected(
				ctx, argument_occurrence, checked_expected, 0, p_changed
			);
			if (expected_check < 0) {
				return -1;
			}
			if (*p_changed) {
				return OPERATION_SOLVER_CONSTRUCTOR_SPINE_WAITING;
			}
			argument_classifier = operation_solver_classifier(
				ctx, argument_occurrence
			);
			if (argument_classifier == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			checked_actual = argument_classifier;
			projection_status = operation_solver_project_pure_sequence_results(
				ctx, argument_classifier, &checked_actual
			);
			if (projection_status < 0) {
				return -1;
			}
			if (projection_status > 0) {
				checked_actual = argument_classifier;
			}
		}
		int structural_check = prototype_judgement_classifier_conversion(
			ctx->terms, ctx->type_declarations, checked_expected, checked_actual
		).status == PROTOTYPE_TERM_CONVERSION_EQUAL ? 1 :
			operation_check_classifier(
				ctx, argument_occurrence, checked_expected, 0
			);
		if (structural_check < 0) {
			return -1;
		}
		if (structural_check == 0) {
			int pending = operation_subgraph_has_pending_classifier(
				ctx, argument_occurrence, 0
			);
			if (pending < 0) {
				return -1;
			}
			if (pending > 0) {
				return 1;
			}
			if (p_mismatch) {
				p_mismatch->expected_classifier = checked_expected;
				p_mismatch->actual_classifier = checked_actual;
				p_mismatch->field_ordinal = i;
			}
			return OPERATION_SOLVER_CONSTRUCTOR_SPINE_CONTRADICTION;
		}
		/* The structural check is the occurrence-level evidence. It may validate
		 * an all-impossible indexed Match at the constructor domain even when the
		 * Match's provisional synthesized classifier has another result. */
		argument_classifiers[i] = expected_domains[i];
	}
	int saturated;
	int status = prototype_judgement_constructor_spine_classifier(
		ctx->terms,
		ctx->type_declarations,
		&ctx->metadata->contexts,
		&ctx->metadata->substitutions,
		operation->context_id,
		semantic_subject,
		PROTOTYPE_INVALID_ID,
		argument_classifiers,
		argument_occurrence_count,
		p_classifier,
		&saturated
	);
	if (status == PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_DOMAIN_MISMATCH) {
		if (p_mismatch) {
			for (uint32_t i = 0; i < argument_occurrence_count; ++i) {
				if (prototype_judgement_classifier_conversion(
						ctx->terms, ctx->type_declarations,
						argument_classifiers[i], expected_domains[i]
					).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
					p_mismatch->expected_classifier = expected_domains[i];
					p_mismatch->actual_classifier = argument_classifiers[i];
					p_mismatch->field_ordinal = i;
					break;
				}
			}
		}
		return OPERATION_SOLVER_CONSTRUCTOR_SPINE_CONTRADICTION;
	}
	return status;
}

/* A saturated constructor determines its result family before its fields have
 * all been proved well typed. Use the schema domains to compute that result
 * solely as a provisional Match-motive equation. This does not solve the
 * constructor operation; the ordinary spine rule still validates every field
 * before publishing a classifier. */
static int operation_solver_constructor_provisional_classifier(
	struct compile_context* ctx,
	uint32_t operation_id,
	uint32_t* p_classifier
) {
	if (!ctx || !ctx->metadata || !p_classifier ||
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	int wrappers[64];
	uint32_t wrapper_count = 0;
	uint32_t subject_operation = operation_id;
	while (subject_operation < ctx->metadata->typed_occurrences.occurrence_count) {
		const struct prototype_typed_occurrence* wrapper =
			&ctx->metadata->typed_occurrences.occurrences[subject_operation];
		if (wrapper->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE) {
			subject_operation = wrapper->wrapped_occurrence;
			continue;
		}
		if (wrapper->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) {
			subject_operation = wrapper->wrapped_occurrence;
			continue;
		}
		if (wrapper->tag != PROTOTYPE_TYPED_OCCURRENCE_RETURN &&
			wrapper->tag != PROTOTYPE_TYPED_OCCURRENCE_THUNK) {
			break;
		}
		uint32_t wrapped_child;
		int wrapped_role = wrapper->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ?
			PROTOTYPE_TERM_CHILD_RETURN_VALUE :
			PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION;
		if (wrapper_count >= 64 || compile_occurrence_child(
				ctx, subject_operation, wrapped_role, 0, &wrapped_child
			) != 0) {
			return -1;
		}
		wrappers[wrapper_count++] = wrapper->tag;
		subject_operation = wrapped_child;
	}
	if (subject_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[subject_operation];
	uint32_t argument_count = 0;
	uint32_t cursor = subject_operation;
	while (cursor < ctx->metadata->typed_occurrences.occurrence_count &&
		ctx->metadata->typed_occurrences.occurrences[cursor].tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
		ctx->metadata->typed_occurrences.occurrences[cursor].application_role ==
			PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION) {
		uint32_t function_operation;
		if (argument_count >= 64) {
			return -1;
		}
		argument_count++;
		if (compile_occurrence_child(
				ctx, cursor, PROTOTYPE_TERM_CHILD_FUNCTION, 0,
				&function_operation
			) != 0) {
			return -1;
		}
		cursor = function_operation;
	}
	if (argument_count == 0) {
		return 1;
	}
	uint32_t expected_domains[64];
	uint32_t expected_domain_count;
	uint32_t semantic_subject;
	if (operation_solver_constructor_semantic_spine(
			ctx, subject_operation, &semantic_subject
		) != 0) {
		return -1;
	}
	if (prototype_judgement_constructor_spine_expected_domains(
			ctx->terms, ctx->type_declarations, &ctx->metadata->contexts,
			&ctx->metadata->substitutions, operation->context_id,
			semantic_subject, PROTOTYPE_INVALID_ID, expected_domains, 64,
			&expected_domain_count
		) != 0 || expected_domain_count != argument_count) {
		return -1;
	}
	int saturated;
	uint32_t classifier;
	int status = prototype_judgement_constructor_spine_classifier(
		ctx->terms, ctx->type_declarations, &ctx->metadata->contexts,
		&ctx->metadata->substitutions, operation->context_id,
		semantic_subject, PROTOTYPE_INVALID_ID, expected_domains,
		expected_domain_count, &classifier, &saturated
	);
	if (status != PROTOTYPE_JUDGEMENT_CONSTRUCTOR_SPINE_VALID || !saturated) {
		return status < 0 ? -1 : 1;
	}
	for (uint32_t reverse = wrapper_count; reverse > 0; --reverse) {
		if (wrappers[reverse - 1] == PROTOTYPE_TYPED_OCCURRENCE_RETURN) {
			uint32_t empty_row;
			if (prototype_term_effect_row_empty(ctx->terms, &empty_row) != 0 ||
				prototype_term_total_computation_type(
					ctx->terms, empty_row, classifier, &classifier
				) != 0) {
				return -1;
			}
		} else if (prototype_term_thunk_type(
				ctx->terms, classifier, &classifier
			) != 0) {
			return -1;
		}
	}
	*p_classifier = classifier;
	return 0;
}

static int operation_solver_join_match_classifiers(
	struct compile_context* ctx,
	uint32_t left,
	uint32_t right,
	uint32_t* p_joined
);

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
		PROTOTYPE_TYPED_OCCURRENCE_SOLVER_INPUT_FACT_CAPACITY) {
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
		lambda_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[lambda_operation];
	if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_LAMBDA) {
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
	if (operation >= ctx->metadata->typed_occurrences.occurrence_count ||
		constraint >= ctx->constraints.constraint_count ||
		ctx->constraints.dependent_constraint_count >=
			PROTOTYPE_TYPED_OCCURRENCE_CONSTRAINT_CAPACITY * 4) {
		return -1;
	}
	for (uint32_t dependency =
			ctx->constraints.first_dependent_constraint[operation];
		dependency != PROTOTYPE_INVALID_ID;
		dependency =
			ctx->constraints.dependent_constraints[dependency].next) {
		if (dependency >= ctx->constraints.dependent_constraint_count) {
			return -1;
		}
		if (ctx->constraints.dependent_constraints[dependency].constraint ==
			constraint) {
			return 0;
		}
	}
	uint32_t dependency = ctx->constraints.dependent_constraint_count++;
	ctx->constraints.dependent_constraints[dependency].source_occurrence =
		operation;
	ctx->constraints.dependent_constraints[dependency].constraint = constraint;
	ctx->constraints.dependent_constraints[dependency].next =
		ctx->constraints.first_dependent_constraint[operation];
	ctx->constraints.first_dependent_constraint[operation] = dependency;
	return 0;
}

/* Constructor-domain checking validates the complete typed occurrence used as
 * a field, not only the field root classifier. Register the same dependency
 * closure that the structural checker reads so solving a nested Match or IH
 * requeues the constructor equation. */
static int operation_solver_add_subtree_constraint_dependencies(
	struct compile_context* ctx,
	uint32_t occurrence,
	uint32_t constraint,
	uint8_t* visited
) {
	if (!ctx || !ctx->metadata || !visited ||
		occurrence >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	if (visited[occurrence]) {
		return 0;
	}
	visited[occurrence] = 1;
	if (operation_solver_add_constraint_dependency(
			ctx, occurrence, constraint
		) != 0) {
		return -1;
	}
	const struct prototype_typed_occurrence* node =
		&ctx->metadata->typed_occurrences.occurrences[occurrence];
	if ((node->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE ||
		node->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE) &&
		node->wrapped_occurrence != PROTOTYPE_INVALID_ID &&
		operation_solver_add_subtree_constraint_dependencies(
			ctx, node->wrapped_occurrence, constraint, visited
		) != 0) {
		return -1;
	}
	if (node->edge_count == 0) {
		return 0;
	}
	if (node->first_edge == PROTOTYPE_INVALID_ID ||
		node->first_edge > ctx->metadata->typed_occurrences.edge_count ||
		node->edge_count >
			ctx->metadata->typed_occurrences.edge_count - node->first_edge) {
		return -1;
	}
	for (uint32_t i = 0; i < node->edge_count; ++i) {
		uint32_t child = ctx->metadata->typed_occurrences.edges[
			node->first_edge + i
		].child_occurrence;
		if (operation_solver_add_subtree_constraint_dependencies(
				ctx, child, constraint, visited
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int operation_solver_validate_classifier_goal(
	const struct compile_context* ctx,
	const struct operation_constraint* goal
) {
	if (!ctx || !ctx->asts || !ctx->terms || !ctx->metadata || !goal ||
		goal->id >= ctx->constraints.classifier_constraint_count ||
		goal->source_occurrence >= ctx->metadata->typed_occurrences.occurrence_count ||
		goal->payload.classifier.classifier_meta_id >= ctx->metadata->typed_occurrences.occurrence_count ||
		goal->context_id >= ctx->metadata->contexts.context_count ||
		goal->context_id !=
			ctx->metadata->typed_occurrences.occurrences[goal->source_occurrence].context_id ||
		(goal->source_ast != PROTOTYPE_INVALID_ID &&
		 goal->source_ast >= ctx->asts->node_count)) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[goal->source_occurrence];
	switch (goal->kind) {
		case OPERATION_CONSTRAINT_HAS_TYPE: {
			union operation_classifier_goal_payload empty_payload = { 0 };
			return memcmp(
				&goal->payload.classifier.goal,
				&empty_payload,
				sizeof(goal->payload.classifier.goal)
			) == 0 ? 0 : -1;
		}
		case OPERATION_CONSTRAINT_EQUAL:
			return operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REFERENCE &&
				goal->payload.classifier.goal.reference.referenced_operation ==
					operation->wrapped_occurrence &&
				goal->payload.classifier.goal.reference.referenced_operation <
					ctx->metadata->typed_occurrences.occurrence_count ? 0 : -1;
		case OPERATION_CONSTRAINT_CONVERTIBLE:
			return operation->tag == PROTOTYPE_TYPED_OCCURRENCE_EXPECTED_TYPE &&
				goal->payload.classifier.goal.conversion.body_occurrence ==
					operation->wrapped_occurrence &&
				goal->payload.classifier.goal.conversion.body_occurrence <
					ctx->metadata->typed_occurrences.occurrence_count &&
				(goal->payload.classifier.goal.conversion.expected_classifier == PROTOTYPE_INVALID_ID ||
				 (goal->payload.classifier.goal.conversion.expected_classifier ==
					ctx->classifier_seeds[goal->source_occurrence].classifier &&
				  goal->payload.classifier.goal.conversion.expected_classifier <
					ctx->terms->term_count)) ? 0 : -1;
		case OPERATION_CONSTRAINT_PI_EXPECTED:
			if (goal->payload.classifier.goal.pi.role == OPERATION_CLASSIFIER_PI_GOAL_INTRO) {
				uint32_t body_occurrence;
				uint32_t binder_classifier = operation_solver_binder_classifier(
					ctx, goal->source_occurrence
				);
				return operation->tag == PROTOTYPE_TYPED_OCCURRENCE_LAMBDA &&
					compile_occurrence_child(
						ctx, goal->source_occurrence, PROTOTYPE_TERM_CHILD_BODY,
						0, &body_occurrence
					) == 0 &&
					goal->payload.classifier.goal.pi.body_or_function_operation == body_occurrence &&
					goal->payload.classifier.goal.pi.body_or_function_operation <
						ctx->metadata->typed_occurrences.occurrence_count &&
					goal->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence ==
						binder_classifier &&
					(binder_classifier == PROTOTYPE_INVALID_ID ||
					 binder_classifier < ctx->terms->term_count) ? 0 : -1;
			}
			{
				uint32_t function_operation;
				uint32_t argument_occurrence;
				return goal->payload.classifier.goal.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
				(operation->application_role ==
						PROTOTYPE_TERM_APPLICATION_FUNCTION_ELIMINATION ||
				 operation->application_role ==
						PROTOTYPE_TERM_APPLICATION_PURE_TYPE_FAMILY_EVALUATION) &&
				compile_occurrence_child(
					ctx, goal->source_occurrence, PROTOTYPE_TERM_CHILD_FUNCTION,
					0, &function_operation
				) == 0 && compile_occurrence_child(
					ctx, goal->source_occurrence, PROTOTYPE_TERM_CHILD_ARGUMENT,
					0, &argument_occurrence
				) == 0 &&
				goal->payload.classifier.goal.pi.body_or_function_operation == function_operation &&
				goal->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence ==
					argument_occurrence ? 0 : -1;
			}
		case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
		{
			uint32_t function_operation;
			uint32_t argument_occurrence;
			return operation->tag == PROTOTYPE_TYPED_OCCURRENCE_APP &&
				operation->application_role ==
					PROTOTYPE_TERM_APPLICATION_CONSTRUCTOR_FORMATION &&
				compile_occurrence_child(
					ctx, goal->source_occurrence, PROTOTYPE_TERM_CHILD_FUNCTION,
					0, &function_operation
				) == 0 && compile_occurrence_child(
					ctx, goal->source_occurrence, PROTOTYPE_TERM_CHILD_ARGUMENT,
					0, &argument_occurrence
				) == 0 &&
				goal->payload.classifier.goal.constructor_formation.function_operation ==
					function_operation &&
				goal->payload.classifier.goal.constructor_formation.argument_occurrence ==
					argument_occurrence ? 0 : -1;
		}
		case OPERATION_CONSTRAINT_BINDER_TYPE:
		{
			uint32_t binding_context;
			return operation->tag == PROTOTYPE_TYPED_OCCURRENCE_VAR &&
				operation_solver_var_binding_context(
					ctx, goal->source_occurrence, &binding_context
				) == 0 &&
				goal->payload.classifier.goal.binder_type.binding_context == binding_context ?
				0 : -1;
		}
		case OPERATION_CONSTRAINT_MOTIVE_EQUATION: {
			const struct operation_classifier_motive_case_goal* motive =
				&goal->payload.classifier.goal.motive_case;
			uint32_t scrutinee_operation;
			uint32_t branch_operation;
			if (operation->tag != PROTOTYPE_TYPED_OCCURRENCE_MATCH ||
				compile_occurrence_child(
					ctx, goal->source_occurrence, PROTOTYPE_TERM_CHILD_SCRUTINEE,
					0, &scrutinee_operation
				) != 0 ||
				motive->case_index >= operation->case_count ||
				operation->first_case + motive->case_index >=
					ctx->metadata->typed_occurrences.case_count ||
				operation->core_term >= ctx->terms->term_count ||
				ctx->terms->terms[operation->core_term].tag != PROTOTYPE_TERM_MATCH ||
				compile_occurrence_child(
					ctx, goal->source_occurrence,
					PROTOTYPE_TERM_CHILD_MATCH_CASE_BODY, motive->case_index,
					&branch_operation
				) != 0 || motive->branch_operation != branch_operation ||
				motive->branch_operation >=
					ctx->metadata->typed_occurrences.occurrence_count ||
				motive->scrutinee_operation != scrutinee_operation ||
				motive->scrutinee_operation >= ctx->metadata->typed_occurrences.occurrence_count) {
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
			const struct prototype_typed_occurrence_graph* graph;
			struct prototype_typed_occurrence_induction_edge edge;
			graph = prototype_compile_metadata_typed_occurrences_const(ctx->metadata);
			int edge_status = prototype_typed_occurrence_graph_induction_edge(
					graph, ctx->terms, goal->payload.classifier.classifier_meta_id, &edge
				);
			return edge_status == 0 &&
				goal->payload.classifier.goal.induction_hypothesis.owner_match_occurrence ==
					edge.owner_match_occurrence &&
				goal->payload.classifier.goal.induction_hypothesis.recursive_argument_occurrence ==
					edge.argument_occurrence &&
				goal->payload.classifier.goal.induction_hypothesis.ih_scope_id == edge.scope_id &&
				goal->payload.classifier.goal.induction_hypothesis.case_index == edge.case_index &&
				goal->payload.classifier.goal.induction_hypothesis.field_index == edge.field_index ?
				0 : -1;
		}
		case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
		{
			uint32_t child_operation;
			int child_role = operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ?
				PROTOTYPE_TERM_CHILD_RETURN_VALUE :
				operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ?
					PROTOTYPE_TERM_CHILD_THUNK_COMPUTATION :
					PROTOTYPE_TERM_CHILD_FORCE_VALUE;
			return (operation->tag == PROTOTYPE_TYPED_OCCURRENCE_RETURN ||
					operation->tag == PROTOTYPE_TYPED_OCCURRENCE_THUNK ||
					operation->tag == PROTOTYPE_TYPED_OCCURRENCE_FORCE) &&
				compile_occurrence_child(
					ctx, goal->source_occurrence, child_role, 0, &child_operation
				) == 0 && goal->payload.classifier.goal.cbpv_boundary.child_operation ==
					child_operation ? 0 : -1;
		}
		case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
		{
			uint32_t computation_occurrence;
			uint32_t return_operation;
			uint32_t return_body_occurrence;
			return operation->tag == PROTOTYPE_TYPED_OCCURRENCE_COMPUTATION_FOLD &&
				compile_occurrence_child(
					ctx, goal->source_occurrence,
					PROTOTYPE_TERM_CHILD_FOLD_COMPUTATION, 0,
					&computation_occurrence
				) == 0 && compile_occurrence_child(
					ctx, goal->source_occurrence,
					PROTOTYPE_TERM_CHILD_FOLD_RETURN_CLAUSE, 0,
					&return_operation
				) == 0 && compile_occurrence_child(
					ctx, return_operation, PROTOTYPE_TERM_CHILD_BODY, 0,
					&return_body_occurrence
				) == 0 &&
				goal->payload.classifier.goal.computation.first_operation == computation_occurrence &&
				goal->payload.classifier.goal.computation.second_operation == return_operation &&
				goal->payload.classifier.goal.computation.third_operation == return_body_occurrence ?
				0 : -1;
		}
		case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
		{
			uint32_t operation_occurrence;
			uint32_t argument_occurrence;
			uint32_t continuation_occurrence;
			return operation->tag == PROTOTYPE_TYPED_OCCURRENCE_REQUEST &&
				compile_occurrence_child(
					ctx, goal->source_occurrence,
					PROTOTYPE_TERM_CHILD_REQUEST_OPERATION, 0,
					&operation_occurrence
				) == 0 && compile_occurrence_child(
					ctx, goal->source_occurrence,
					PROTOTYPE_TERM_CHILD_REQUEST_ARGUMENT, 0,
					&argument_occurrence
				) == 0 && compile_occurrence_child(
					ctx, goal->source_occurrence,
					PROTOTYPE_TERM_CHILD_REQUEST_CONTINUATION, 0,
					&continuation_occurrence
				) == 0 && goal->payload.classifier.goal.computation.first_operation ==
					operation_occurrence &&
				goal->payload.classifier.goal.computation.second_operation == argument_occurrence &&
				goal->payload.classifier.goal.computation.third_operation == continuation_occurrence ?
				0 : -1;
		}
		default:
			return -1;
	}
}

static int operation_solver_index_computation_dependencies(
	struct compile_context* ctx,
	const struct operation_constraint* constraint,
	uint32_t constraint_id
) {
	return operation_solver_add_constraint_dependency(
			ctx,
			constraint->payload.classifier.goal.computation.first_operation,
			constraint_id
		) != 0 || operation_solver_add_constraint_dependency(
			ctx,
			constraint->payload.classifier.goal.computation.second_operation,
			constraint_id
		) != 0 || operation_solver_add_constraint_dependency(
			ctx,
			constraint->payload.classifier.goal.computation.third_operation,
			constraint_id
		) != 0 ? -1 : 0;
}

static int operation_solver_index_constraints(struct compile_context* ctx) {
	if (!ctx || !ctx->metadata) {
		return -1;
	}
	ctx->metadata->constraint_index_pass_count++;
	for (uint32_t i = 0; i < 4096; ++i) {
		ctx->constraints.first_dependent_constraint[i] =
			PROTOTYPE_INVALID_ID;
		ctx->constraints.first_motive_constraint[i] = PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0;
		i < ctx->constraints.classifier_constraint_count;
		++i) {
		ctx->constraints.next_motive_constraint[i] = PROTOTYPE_INVALID_ID;
	}
	ctx->constraints.dependent_constraint_count = 0;
	ctx->constraints.worklist_head = 0;
	ctx->constraints.worklist_count = 0;
	memset(
		ctx->constraints.constraint_queued,
		0,
		sizeof(ctx->constraints.constraint_queued)
	);
	for (uint32_t i = 0; i < ctx->constraints.classifier_constraint_count; ++i) {
		const struct operation_constraint* constraint =
			&ctx->constraints.constraints[i];
		if (constraint->id != i ||
			constraint->domain != OPERATION_CONSTRAINT_DOMAIN_CLASSIFIER) {
			return -1;
		}
		if (operation_solver_validate_classifier_goal(ctx, constraint) != 0) {
			if (constraint->kind == OPERATION_CONSTRAINT_IH_EXPECTED &&
				constraint->source_occurrence < ctx->metadata->typed_occurrences.occurrence_count) {
				const struct prototype_typed_occurrence* operation =
					&ctx->metadata->typed_occurrences.occurrences[constraint->source_occurrence];
				(void)add_compile_diagnostic(
					ctx,
					PROTOTYPE_COMPILE_DIAGNOSTIC_PHASE_CONSTRAINT_SOLVER,
					PROTOTYPE_COMPILE_DIAGNOSTIC_IH_OWNERSHIP,
					operation->source_ast,
					constraint->source_occurrence,
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
				ctx, constraint->payload.classifier.classifier_meta_id, i
			) != 0) {
			return -1;
		}
		switch (constraint->kind) {
			case OPERATION_CONSTRAINT_EQUAL:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.classifier.goal.reference.referenced_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CONVERTIBLE:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.classifier.goal.conversion.body_occurrence, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_IH_EXPECTED:
				if (operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.classifier.goal.induction_hypothesis.recursive_argument_occurrence,
						i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.classifier.goal.induction_hypothesis.owner_match_occurrence,
						i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_PI_EXPECTED:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.classifier.goal.pi.body_or_function_operation, i
					) != 0 ||
					(constraint->payload.classifier.goal.pi.role == OPERATION_CLASSIFIER_PI_GOAL_ELIM &&
					operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.classifier.goal.pi.domain_classifier_or_argument_occurrence,
						i
					) != 0)) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CONSTRUCTOR_FORMATION:
			{
				uint8_t visited[4096] = { 0 };
				if (operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.classifier.goal.constructor_formation.function_operation,
						i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx,
						constraint->payload.classifier.goal.constructor_formation.argument_occurrence,
						i
					) != 0 ||
					operation_solver_add_subtree_constraint_dependencies(
						ctx,
						constraint->payload.classifier.goal.constructor_formation.argument_occurrence,
						i,
						visited
					) != 0) {
					return -1;
				}
				break;
			}
			case OPERATION_CONSTRAINT_BINDER_TYPE:
				/* Context binding classifiers are immutable solver inputs. */
				break;
			case OPERATION_CONSTRAINT_MOTIVE_EQUATION:
				ctx->constraints.next_motive_constraint[i] =
					ctx->constraints.first_motive_constraint[
						constraint->payload.classifier.classifier_meta_id
					];
				ctx->constraints.first_motive_constraint[
					constraint->payload.classifier.classifier_meta_id
				] = i;
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.classifier.goal.motive_case.branch_operation, i
					) != 0 || operation_solver_add_constraint_dependency(
						ctx, constraint->payload.classifier.goal.motive_case.scrutinee_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_CBPV_BOUNDARY:
				if (operation_solver_add_constraint_dependency(
						ctx, constraint->payload.classifier.goal.cbpv_boundary.child_operation, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_COMPUTATION_FOLD_RESULT:
				for (uint32_t clause_index = 0;
					clause_index < ctx->metadata->typed_occurrences.occurrences[
						constraint->source_occurrence
					].fold_clause_count;
					++clause_index) {
					uint32_t clause_operation;
					uint32_t clause_lambda;
					uint32_t body_occurrence;
					if (compile_fold_clause_occurrences(
							ctx,
							constraint->source_occurrence,
							clause_index,
							&clause_operation,
							&clause_lambda,
							&body_occurrence
						) != 0 || operation_solver_add_constraint_dependency(
							ctx, body_occurrence, i
						) != 0) {
						return -1;
					}
					(void)clause_operation;
					(void)clause_lambda;
				}
				if (operation_solver_index_computation_dependencies(
						ctx, constraint, i
					) != 0) {
					return -1;
				}
				break;
			case OPERATION_CONSTRAINT_OPERATION_REQUEST_RESULT:
				if (operation_solver_index_computation_dependencies(
						ctx, constraint, i
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
		operation_id >= ctx->metadata->typed_occurrences.occurrence_count) {
		return -1;
	}
	const struct prototype_typed_occurrence* operation =
		&ctx->metadata->typed_occurrences.occurrences[operation_id];
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
				(operation->tag != PROTOTYPE_TYPED_OCCURRENCE_VAR ||
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
