/* Function-graph requests are source projections keyed by definition identity.
 * Generation is intentionally separated from AST parsing and Core interning. */
static int compile_imported_function_graph_association(
	struct compile_context* ctx,
	int owner_symbol_id
) {
	const struct prototype_artifact_interface* selected_interface = NULL;
	const struct prototype_artifact_function_graph_association* selected = NULL;
	uint32_t selected_interface_index = PROTOTYPE_INVALID_ID;
	uint32_t selected_owner_export = PROTOTYPE_INVALID_ID;
	uint32_t owner_count = 0;
	if (!ctx || owner_symbol_id < 0) {
		return -1;
	}
	for (uint32_t i = 0; i < ctx->imported_interface_count; ++i) {
		const struct prototype_artifact_interface* interface =
			ctx->imported_interfaces[i];
		if (!interface) {
			continue;
		}
		for (uint32_t j = 0; j < interface->term_export_count; ++j) {
			if (interface->term_exports[j].name_symbol_id != owner_symbol_id) {
				continue;
			}
			owner_count++;
			selected_interface = interface;
			selected_interface_index = i;
			selected_owner_export = j;
			selected = NULL;
			for (uint32_t k = 0;
				k < interface->function_graph_association_count;
				++k) {
				if (interface->function_graph_associations[k].
						owner_term_export_index == j) {
					selected = &interface->function_graph_associations[k];
					break;
				}
			}
		}
	}
	if (owner_count == 0) {
		return 1;
	}
	if (owner_count != 1) {
		return 3;
	}
	if (!selected_interface || !selected) {
		return 2;
	}
	if (selected->graph_type_export_index >=
			selected_interface->type_export_count ||
		selected->result_type_export_index >=
			selected_interface->type_export_count ||
		selected->graph_interface_term_export_index >=
			selected_interface->term_export_count ||
		selected->certified_runner_term_export_index >=
			selected_interface->term_export_count ||
		(selected->certified_adapter_term_export_index != PROTOTYPE_INVALID_ID &&
		 selected->certified_adapter_term_export_index >=
			selected_interface->term_export_count)) {
		return -1;
	}
	struct prototype_function_graph_association association = {
		.owner_symbol_id = owner_symbol_id,
		.owner_assignment_id = PROTOTYPE_INVALID_ID,
		.owner_source_entry_id = PROTOTYPE_INVALID_ID,
		.imported = 1,
		.imported_interface_index = selected_interface_index,
		.imported_owner_term_export_index = selected_owner_export,
		.imported_graph_type_export_index = selected->graph_type_export_index,
		.imported_result_type_export_index = selected->result_type_export_index,
		.imported_graph_interface_term_export_index =
			selected->graph_interface_term_export_index,
		.imported_certified_adapter_term_export_index =
			selected->certified_adapter_term_export_index,
		.imported_certified_runner_term_export_index =
			selected->certified_runner_term_export_index,
		.graph_symbol_id = selected_interface->type_exports[
			selected->graph_type_export_index
		].name_symbol_id,
		.result_symbol_id = selected_interface->type_exports[
			selected->result_type_export_index
		].name_symbol_id,
		.returned_constructor_symbol_id = -1,
		.graph_interface_symbol_id = selected_interface->term_exports[
			selected->graph_interface_term_export_index
		].name_symbol_id,
		.certified_adapter_symbol_id =
			selected->certified_adapter_term_export_index == PROTOTYPE_INVALID_ID ?
				-1 : selected_interface->term_exports[
					selected->certified_adapter_term_export_index
				].name_symbol_id,
		.certified_runner_symbol_id = selected_interface->term_exports[
			selected->certified_runner_term_export_index
		].name_symbol_id,
		.graph_type_id = selected_interface->type_exports[
			selected->graph_type_export_index
		].local_type_id,
		.result_type_id = selected_interface->type_exports[
			selected->result_type_export_index
		].local_type_id,
		.graph_type_assignment_id = PROTOTYPE_INVALID_ID,
		.result_type_assignment_id = PROTOTYPE_INVALID_ID,
		.graph_interface_assignment_id = PROTOTYPE_INVALID_ID,
		.certified_adapter_assignment_id = PROTOTYPE_INVALID_ID,
		.certified_runner_assignment_id = PROTOTYPE_INVALID_ID,
		.executable_assignment_id = PROTOTYPE_INVALID_ID,
		.certified_argument_index = selected->certified_argument_index
	};
	return prototype_compile_metadata_add_function_graph_association(
		ctx->metadata, association, &(uint32_t) { PROTOTYPE_INVALID_ID }
	);
}

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
	if (!entry || entry->assignment_count == 0) {
		int imported_status = compile_imported_function_graph_association(
			ctx, owner_symbol_id
		);
		if (imported_status == 0) {
			return 0;
		}
		if (imported_status == 2) {
			fprintf(stderr,
				"imported function graph association missing owner=%d ast=%u\n",
				owner_symbol_id, requesting_ast);
		} else {
			fprintf(stderr,
				"function graph owner lookup failed symbol=%d assignments=0 ast=%u\n",
				owner_symbol_id, requesting_ast);
		}
		(void)add_resolve_error(
			ctx,
			imported_status == 3 ?
				PROTOTYPE_RESOLVE_ERROR_AMBIGUOUS_ASSIGNMENT :
				PROTOTYPE_RESOLVE_ERROR_NAME,
			owner_symbol_id,
			-1,
			requesting_ast
		);
		return -1;
	}
	if (entry->assignment_count != 1) {
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
