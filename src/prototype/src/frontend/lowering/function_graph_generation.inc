/* Function-graph requests are source projections keyed by definition identity.
 * Generation is intentionally separated from AST parsing and Core interning. */
static int compile_collect_function_graph_request(
	struct compile_context* ctx,
	int owner_symbol_id,
	uint32_t requesting_ast,
	uint32_t request_flags
) {
	if (!ctx || !ctx->asts || !ctx->metadata || owner_symbol_id < 0) {
		return -1;
	}
	const struct prototype_ast_def_open_address_entry* entry =
		lookup_def_index_entry_const(ctx->asts, owner_symbol_id);
	if (!entry || entry->assignment_count != 1) {
		fprintf(stderr,
			"function graph owner lookup failed symbol=%d assignments=%u ast=%u\n",
			owner_symbol_id, entry ? entry->assignment_count : 0, requesting_ast);
		(void)add_resolve_error(
			ctx,
			!entry || entry->assignment_count == 0 ?
				PROTOTYPE_RESOLVE_ERROR_NAME :
				PROTOTYPE_RESOLVE_ERROR_AMBIGUOUS_ASSIGNMENT,
			owner_symbol_id,
			-1,
			requesting_ast
		);
		return -1;
	}
	const struct prototype_ast_term_assignment_def* owner =
		lookup_unique_assignment_raw(ctx->asts, owner_symbol_id);
	if (!owner) {
		return -1;
	}
	uint32_t owner_assignment_id = (uint32_t)(owner - ctx->asts->assignments);
	uint32_t request_id;
	int status = prototype_compile_metadata_request_function_graph(
		ctx->metadata,
		owner_symbol_id,
		owner_assignment_id,
		owner->source_entry_id,
		request_flags,
		requesting_ast,
		&request_id
	);
	if (status != 0) {
		fprintf(stderr,
			"function graph request storage failed owner=%d assignment=%u source=%u flags=%u ast=%u storage=%p capacity=%zu count=%zu\n",
			owner_symbol_id, owner_assignment_id, owner->source_entry_id,
			request_flags, requesting_ast,
			(void*)ctx->metadata->function_graph_requests,
			ctx->metadata->function_graph_request_capacity,
			ctx->metadata->function_graph_request_count);
	}
	return status;
}

static int compile_phase_collect_function_graph_requests(
	struct compile_context* ctx
) {
	if (!ctx || !ctx->asts || !ctx->metadata) {
		return -1;
	}
	ctx->metadata->function_graph_request_count = 0;
	for (uint32_t i = 0; i < ctx->asts->type_expr_count; ++i) {
		const struct prototype_ast_type_expr* expr = &ctx->asts->type_exprs[i];
		if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE &&
			compile_collect_function_graph_request(
				ctx,
				expr->as.function_graph_reference.owner_symbol_id,
				i,
				PROTOTYPE_FUNCTION_GRAPH_REQUEST_FAMILY
			) != 0) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < ctx->asts->node_count; ++i) {
		const struct prototype_ast_node* node = &ctx->asts->nodes[i];
		if (node->tag == PROTOTYPE_AST_FUNCTION_GRAPH_WITNESS_REFERENCE &&
			compile_collect_function_graph_request(
				ctx,
				node->as.function_graph_witness_reference.owner_symbol_id,
				i,
				PROTOTYPE_FUNCTION_GRAPH_REQUEST_FAMILY |
					PROTOTYPE_FUNCTION_GRAPH_REQUEST_CERTIFIED_EXECUTION
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static const struct prototype_function_graph_association*
compile_function_graph_association(
	const struct compile_context* ctx,
	int owner_symbol_id
) {
	return ctx ? prototype_compile_metadata_function_graph_association_for_owner(
		ctx->metadata, owner_symbol_id
	) : NULL;
}
