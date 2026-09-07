#include "a_program/frontend/core_lowering.h"

#include "a_program/core/request_provider.h"
#include "a_program/support/schema.h"

static int c0_checked_size_product(
	size_t left,
	size_t right,
	size_t* product
) {
	if (!product || (right != 0 && left > SIZE_MAX / right)) return -1;
	*product = left * right;
	return 0;
}

static int c0_lowering_request_with_payload(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size,
	struct prototype_core_formation_response* p_response
) {
	if (!stage || !stage->core || !request || !p_response) {
		return -1;
	}
	*p_response = (struct prototype_core_formation_response) {
		.term_id = PROTOTYPE_INVALID_ID,
		.identity_id = PROTOTYPE_INVALID_ID
	};
	return prototype_core_request_form(
		stage->core, request, payload, payload_size, p_response
	);
}

static int c0_lowering_request(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_core_formation_request* request,
	struct prototype_core_formation_response* p_response
) {
	return c0_lowering_request_with_payload(
		stage, request, NULL, 0, p_response
	);
}

static int c0_lowering_submit(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_core_formation_request* request,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_response response;
	if (!p_result || c0_lowering_request(stage, request, &response) != 0 ||
		response.term_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	*p_result = (struct prototype_c0_open_term) {
		.term_id = response.term_id,
		.graph_revision = response.graph_revision
	};
	return 0;
}

int prototype_c0_lowering_stage_init(
	struct prototype_c0_lowering_stage* stage,
	const struct prototype_core_request_service* core
) {
	if (!stage || !core) {
		return -1;
	}
	stage->core = core;
	return 0;
}

int prototype_c0_lowering_stage_preallocate_bindings(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_source_lowering_plan* plan,
	struct prototype_source_core_handoff_builder* handoff
) {
	if (!stage || !plan || !handoff || handoff->sealed ||
		plan->binder_count > UINT32_MAX ||
		prototype_source_lowering_plan_validate(plan) != 0 ||
		handoff->source_fingerprint != plan->source_fingerprint ||
		handoff->binder_count != plan->binder_count) {
		return -1;
	}
	for (uint32_t binder_slot = 0;
		binder_slot < (uint32_t)plan->binder_count;
		++binder_slot) {
		const struct prototype_source_lowering_binder* source_binder =
			&plan->binders[binder_slot];
		const struct prototype_source_core_binder_result* existing =
			prototype_source_core_binder_result_get(handoff, binder_slot);
		if (!existing) return -1;
		uint32_t shared_slot = PROTOTYPE_INVALID_ID;
		if (source_binder->source_binder_id != PROTOTYPE_INVALID_ID) {
			for (uint32_t previous = 0; previous < binder_slot; ++previous) {
				if (plan->binders[previous].source_binder_id ==
					source_binder->source_binder_id) {
					shared_slot = previous;
					break;
				}
			}
		}
		if (existing->state == PROTOTYPE_SOURCE_CORE_FORMED) {
			if (shared_slot != PROTOTYPE_INVALID_ID) {
				const struct prototype_source_core_binder_result* shared =
					prototype_source_core_binder_result_get(
						handoff, shared_slot
					);
				if (!shared || shared->state != PROTOTYPE_SOURCE_CORE_FORMED ||
					shared->binding_id != existing->binding_id) {
					return -1;
				}
			}
			continue;
		}
		if (existing->state != PROTOTYPE_SOURCE_CORE_UNFORMED) return -1;
		if (shared_slot != PROTOTYPE_INVALID_ID) {
			const struct prototype_source_core_binder_result* shared =
				prototype_source_core_binder_result_get(handoff, shared_slot);
			if (!shared || shared->state != PROTOTYPE_SOURCE_CORE_FORMED ||
				shared->binding_id == PROTOTYPE_INVALID_ID ||
				prototype_source_core_binder_record_formed(
					handoff, binder_slot, shared->binding_id,
					shared->graph_revision
				) != 0) {
				return -1;
			}
			continue;
		}
		const struct prototype_core_formation_request request = {
			.kind = PROTOTYPE_CORE_FORM_NEW_BINDING
		};
		struct prototype_core_formation_response response;
		if (c0_lowering_request(stage, &request, &response) != 0 ||
			response.identity_id == PROTOTYPE_INVALID_ID ||
			prototype_source_core_binder_record_formed(
				handoff, binder_slot, response.identity_id,
				response.graph_revision
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int c0_source_match_requires_ih_scope(
	const struct prototype_source_lowering_plan* plan,
	uint32_t match_slot
) {
	if (!plan || match_slot >= plan->slot_count ||
		plan->slots[match_slot].kind != PROTOTYPE_SOURCE_LOWERING_TERM ||
		plan->slots[match_slot].source_tag != PROTOTYPE_AST_MATCH) {
		return -1;
	}
	const struct prototype_source_lowering_slot* slot = &plan->slots[match_slot];
	for (uint32_t i = 0; i < slot->binder_count; ++i) {
		const struct prototype_source_lowering_binder* binder =
			&plan->binders[slot->first_binder + i];
		if (binder->role != PROTOTYPE_SOURCE_BINDER_MATCH_FIELD) continue;
		for (uint32_t node_id = 0; node_id < plan->source->node_count;
			++node_id) {
			const struct prototype_ast_node* node = &plan->source->nodes[node_id];
			if (node->tag == PROTOTYPE_AST_INDUCTION_HYPOTHESIS &&
				node->as.induction_hypothesis.ast_binder_id ==
					binder->source_binder_id) {
				return 1;
			}
		}
	}
	return 0;
}

static int c0_lowering_preallocate_ih_scopes(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_source_lowering_plan* plan,
	struct prototype_source_core_handoff_builder* handoff
) {
	if (!stage || !plan || !handoff || handoff->sealed) return -1;
	for (uint32_t slot_id = 0; slot_id < plan->slot_count; ++slot_id) {
		if (plan->slots[slot_id].kind != PROTOTYPE_SOURCE_LOWERING_TERM ||
			plan->slots[slot_id].source_tag != PROTOTYPE_AST_MATCH) {
			continue;
		}
		int required = c0_source_match_requires_ih_scope(plan, slot_id);
		if (required < 0) return -1;
		if (!required) continue;
		const struct prototype_source_core_slot_result* existing =
			prototype_source_core_slot_result_get(handoff, slot_id);
		if (!existing) return -1;
		if (existing->ih_scope_id != PROTOTYPE_INVALID_ID) continue;
		struct prototype_core_formation_response response;
		const struct prototype_core_formation_request request = {
			.kind = PROTOTYPE_CORE_FORM_NEW_IH_SCOPE
		};
		if (c0_lowering_request(stage, &request, &response) != 0 ||
			response.identity_id == PROTOTYPE_INVALID_ID ||
			prototype_source_core_slot_record_ih_scope(
				handoff, slot_id, response.identity_id, response.graph_revision
			) != 0) {
			return -1;
		}
	}
	return 0;
}

static int c0_source_slot_child(
	const struct prototype_source_lowering_plan* plan,
	const struct prototype_source_core_handoff_builder* handoff,
	uint32_t owner_slot,
	int role,
	uint32_t ordinal,
	uint32_t* p_term,
	uint64_t* p_revision
) {
	if (!plan || !handoff || !p_term || owner_slot >= plan->slot_count) {
		return -1;
	}
	const struct prototype_source_lowering_slot* owner =
		&plan->slots[owner_slot];
	int found = 0;
	for (uint32_t i = 0; i < owner->edge_count; ++i) {
		uint32_t edge_id = owner->first_edge + i;
		if (edge_id >= plan->edge_count) return -1;
		const struct prototype_source_lowering_edge* edge =
			&plan->edges[edge_id];
		if (edge->role != role || edge->ordinal != ordinal) continue;
		if (found || edge->child_slot >= handoff->slot_count) return -1;
		const struct prototype_source_core_slot_result* child =
			prototype_source_core_slot_result_get(handoff, edge->child_slot);
		if (!child || child->state != PROTOTYPE_SOURCE_CORE_FORMED ||
			child->term_id == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		*p_term = child->term_id;
		if (p_revision) *p_revision = child->graph_revision;
		found = 1;
	}
	return found ? 0 : -1;
}

static int c0_source_slot_binding(
	const struct prototype_source_lowering_plan* plan,
	const struct prototype_source_core_handoff_builder* handoff,
	uint32_t owner_slot,
	int role,
	uint32_t ordinal,
	uint32_t* p_binding
) {
	if (!plan || !handoff || !p_binding || owner_slot >= plan->slot_count) {
		return -1;
	}
	const struct prototype_source_lowering_slot* owner =
		&plan->slots[owner_slot];
	int found = 0;
	for (uint32_t i = 0; i < owner->binder_count; ++i) {
		uint32_t binder_slot = owner->first_binder + i;
		if (binder_slot >= plan->binder_count) return -1;
		const struct prototype_source_lowering_binder* binder =
			&plan->binders[binder_slot];
		if (binder->role != role || binder->ordinal != ordinal) continue;
		if (found || binder_slot >= handoff->binder_count) return -1;
		const struct prototype_source_core_binder_result* result =
			prototype_source_core_binder_result_get(handoff, binder_slot);
		if (!result || result->state != PROTOTYPE_SOURCE_CORE_FORMED ||
			result->binding_id == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		*p_binding = result->binding_id;
		found = 1;
	}
	return found ? 0 : -1;
}

static int c0_source_slot_record_alias(
	struct prototype_source_core_handoff_builder* handoff,
	uint32_t slot_id,
	uint32_t term_id,
	uint64_t graph_revision
) {
	return prototype_source_core_slot_record_formed(
		handoff, slot_id, term_id, PROTOTYPE_INVALID_ID, graph_revision
	);
}

static int c0_source_slot_record_nonterm(
	struct prototype_source_core_handoff_builder* handoff,
	uint32_t slot_id,
	int result_kind
) {
	return prototype_source_core_slot_record_nonterm(
		handoff, slot_id, result_kind, 0
	) == 0 ? PROTOTYPE_C0_SOURCE_SLOT_FORMED : -1;
}

static int c0_source_term_nonterm_kind(int source_tag) {
	switch (source_tag) {
		case PROTOTYPE_AST_DEFINITION_BLOCK:
		case PROTOTYPE_AST_BLOCK_BINDING:
		case PROTOTYPE_AST_BLOCK_LAMBDA_EXIT:
			return PROTOTYPE_SOURCE_CORE_SLOT_SOURCE_ONLY;
		case PROTOTYPE_AST_NAME_IN_AST_NAMESPACE:
		case PROTOTYPE_AST_DEFINITION_SELECT:
		case PROTOTYPE_AST_COMPUTATION_BLOCK:
		case PROTOTYPE_AST_CERTIFIED_FUNCTION_REFERENCE:
		case PROTOTYPE_AST_FUNCTION_GRAPH_ROLE_REFERENCE:
		case PROTOTYPE_AST_CERTIFIED_ELIMINATION:
			return PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1;
		default:
			return PROTOTYPE_SOURCE_CORE_SLOT_INVALID;
	}
}

static int c0_source_slot_has_nonterm_child(
	const struct prototype_source_lowering_plan* plan,
	const struct prototype_source_core_handoff_builder* handoff,
	uint32_t slot_id
) {
	if (!plan || !handoff || slot_id >= plan->slot_count) return -1;
	const struct prototype_source_lowering_slot* slot = &plan->slots[slot_id];
	for (uint32_t i = 0; i < slot->edge_count; ++i) {
		uint32_t edge_id = slot->first_edge + i;
		if (edge_id >= plan->edge_count ||
			plan->edges[edge_id].child_slot >= handoff->slot_count) {
			return -1;
		}
		const struct prototype_source_core_slot_result* child =
			prototype_source_core_slot_result_get(
				handoff, plan->edges[edge_id].child_slot
			);
		if (!child || child->state != PROTOTYPE_SOURCE_CORE_FORMED) return -1;
		if (child->result_kind != PROTOTYPE_SOURCE_CORE_SLOT_TERM) return 1;
	}
	return 0;
}

static int c0_source_slot_submit(
	const struct prototype_c0_lowering_stage* stage,
	struct prototype_source_core_handoff_builder* handoff,
	uint32_t slot_id,
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size
) {
	struct prototype_core_formation_response response;
	if (prototype_source_core_slot_begin(handoff, slot_id) != 0) return -1;
	if (c0_lowering_request_with_payload(
			stage, request, payload, payload_size, &response
		) != 0 ||
		response.term_id == PROTOTYPE_INVALID_ID ||
		prototype_source_core_slot_commit(
			handoff, slot_id, response.term_id, PROTOTYPE_INVALID_ID,
			response.graph_revision
		) != 0) {
		(void)prototype_source_core_slot_reset(handoff, slot_id);
		return -1;
	}
	return PROTOTYPE_C0_SOURCE_SLOT_FORMED;
}

static int c0_source_match_submit(
	const struct prototype_c0_lowering_stage* stage,
	struct prototype_source_core_handoff_builder* handoff,
	uint32_t slot_id,
	uint32_t ih_scope_id,
	const struct prototype_core_formation_request* request,
	const struct prototype_core_request_payload* payload,
	size_t payload_size
) {
	struct prototype_core_formation_response response;
	if (prototype_source_core_slot_begin(handoff, slot_id) != 0) return -1;
	if (c0_lowering_request_with_payload(
			stage, request, payload, payload_size, &response
		) != 0 ||
		response.term_id == PROTOTYPE_INVALID_ID) {
		(void)prototype_source_core_slot_reset(handoff, slot_id);
		return -1;
	}
	if (ih_scope_id != PROTOTYPE_INVALID_ID) {
		const struct prototype_core_formation_request scope_request = {
			.kind = PROTOTYPE_CORE_FORM_SET_IH_SCOPE_TERM,
			.as.set_ih_scope = {
				.ih_scope_id = ih_scope_id,
				.match_term = response.term_id
			}
		};
		struct prototype_core_formation_response scope_response;
		if (c0_lowering_request(
				stage, &scope_request, &scope_response
			) != 0) {
			(void)prototype_source_core_slot_reset(handoff, slot_id);
			return -1;
		}
	}
	if (prototype_source_core_slot_commit(
			handoff, slot_id, response.term_id, ih_scope_id,
			response.graph_revision
		) != 0) {
		(void)prototype_source_core_slot_reset(handoff, slot_id);
		return -1;
	}
	return PROTOTYPE_C0_SOURCE_SLOT_FORMED;
}

static int c0_source_ih_match_slot(
	const struct prototype_source_lowering_plan* plan,
	uint32_t source_binder_id,
	uint32_t* p_match_slot
) {
	if (!plan || !p_match_slot) return -1;
	uint32_t found = PROTOTYPE_INVALID_ID;
	for (uint32_t slot_id = 0; slot_id < plan->slot_count; ++slot_id) {
		const struct prototype_source_lowering_slot* slot = &plan->slots[slot_id];
		if (slot->kind != PROTOTYPE_SOURCE_LOWERING_TERM ||
			slot->source_tag != PROTOTYPE_AST_MATCH) {
			continue;
		}
		for (uint32_t i = 0; i < slot->binder_count; ++i) {
			const struct prototype_source_lowering_binder* binder =
				&plan->binders[slot->first_binder + i];
			if (binder->role != PROTOTYPE_SOURCE_BINDER_MATCH_FIELD ||
				binder->source_binder_id != source_binder_id) {
				continue;
			}
			if (found != PROTOTYPE_INVALID_ID && found != slot_id) return -1;
			found = slot_id;
		}
	}
	if (found == PROTOTYPE_INVALID_ID) return 1;
	*p_match_slot = found;
	return 0;
}

static int c0_source_type_formation_slot(
	const struct prototype_source_lowering_plan* plan,
	uint32_t type_def_id,
	uint32_t* p_slot
) {
	if (!plan || !p_slot || type_def_id >= plan->source->type_def_count) {
		return -1;
	}
	uint32_t found = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < plan->source->node_count; ++i) {
		const struct prototype_ast_node* node = &plan->source->nodes[i];
		if ((node->tag != PROTOTYPE_AST_TYPE_FORMATION &&
			 node->tag != PROTOTYPE_AST_TYPE_LITERAL) ||
			node->as.type_formation.ast_type_def_id != type_def_id) {
			continue;
		}
		if (found != PROTOTYPE_INVALID_ID) return -1;
		found = i;
	}
	if (found == PROTOTYPE_INVALID_ID) return -1;
	*p_slot = found;
	return 0;
}

static int c0_source_type_declaration_spine(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_source_lowering_plan* plan,
	const struct prototype_source_core_handoff_builder* handoff,
	uint32_t type_def_id,
	uint32_t* p_term,
	uint64_t* p_revision
) {
	if (!stage || !plan || !handoff || !p_term || !p_revision ||
		type_def_id >= plan->source->type_def_count) {
		return -1;
	}
	uint32_t formation_slot;
	if (c0_source_type_formation_slot(
			plan, type_def_id, &formation_slot
		) != 0) {
		return -1;
	}
	const struct prototype_ast_type_def* type =
		&plan->source->type_defs[type_def_id];
	struct prototype_core_formation_response response;
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_EXTERNAL_REF,
		.as.external_ref.name = {
			.namespace_symbol_id = PROTOTYPE_BASE_NAMESPACE_ID,
			.name_symbol_id = type->name_symbol_id
		}
	};
	if (c0_lowering_request(stage, &request, &response) != 0 ||
		response.term_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t current = response.term_id;
	uint64_t revision = response.graph_revision;
	for (uint32_t i = 0; i < type->parameter_count; ++i) {
		uint32_t parameter_binding;
		if (c0_source_slot_binding(
				plan, handoff, formation_slot,
				PROTOTYPE_SOURCE_BINDER_FAMILY_PARAMETER, i,
				&parameter_binding
			) != 0) {
			return 1;
		}
		request = (struct prototype_core_formation_request) {
			.kind = PROTOTYPE_CORE_FORM_VAR,
			.as.var.binding_id = parameter_binding
		};
		if (c0_lowering_request(stage, &request, &response) != 0 ||
			response.term_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		request = (struct prototype_core_formation_request) {
			.kind = PROTOTYPE_CORE_FORM_APP,
			.as.app = {
				.function = current,
				.argument = response.term_id
			}
		};
		if (c0_lowering_request(stage, &request, &response) != 0 ||
			response.term_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		current = response.term_id;
		revision = response.graph_revision;
	}
	*p_term = current;
	*p_revision = revision;
	return 0;
}

/* Forms one source slot from immutable source topology and formed Core
 * children. Type-directed source operators remain deferred to Layer T. */
static int c0_lowering_stage_form_source_slot(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_source_lowering_plan* plan,
	struct prototype_source_core_handoff_builder* handoff,
	uint32_t slot_id
) {
	if (!stage || !plan || !handoff || handoff->sealed ||
		slot_id >= plan->slot_count || handoff->slot_count != plan->slot_count ||
		handoff->binder_count != plan->binder_count ||
		handoff->source_fingerprint != plan->source_fingerprint ||
		prototype_source_lowering_plan_validate(plan) != 0) {
		return -1;
	}
	const struct prototype_source_core_slot_result* existing =
		prototype_source_core_slot_result_get(handoff, slot_id);
	if (!existing) return -1;
	if (existing->state == PROTOTYPE_SOURCE_CORE_FORMED) {
		return PROTOTYPE_C0_SOURCE_SLOT_FORMED;
	}
	if (existing->state != PROTOTYPE_SOURCE_CORE_UNFORMED) return -1;

	const struct prototype_source_lowering_slot* slot = &plan->slots[slot_id];
	uint32_t first;
	uint32_t second;
	uint32_t binding;
	uint64_t revision;
	struct prototype_core_formation_request request;

	if (slot->kind == PROTOTYPE_SOURCE_LOWERING_TERM) {
		if (slot->source_id >= plan->source->node_count) return -1;
		const struct prototype_ast_node* node =
			&plan->source->nodes[slot->source_id];
		int nonterm_kind = c0_source_term_nonterm_kind(node->tag);
		if (nonterm_kind != PROTOTYPE_SOURCE_CORE_SLOT_INVALID) {
			return c0_source_slot_record_nonterm(
				handoff, slot_id, nonterm_kind
			);
		}
		int nonterm_child = c0_source_slot_has_nonterm_child(
			plan, handoff, slot_id
		);
		if (nonterm_child < 0) return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
		if (nonterm_child > 0) {
			return c0_source_slot_record_nonterm(
				handoff, slot_id, PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1
			);
		}
		switch (node->tag) {
			case PROTOTYPE_AST_NAME:
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_EXTERNAL_REF,
					.as.external_ref.name = {
						.namespace_symbol_id = PROTOTYPE_BASE_NAMESPACE_ID,
						.name_symbol_id = node->as.name.symbol_id
					}
				};
				break;
			case PROTOTYPE_AST_NAME_IN_NAMESPACE:
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_EXTERNAL_REF,
					.as.external_ref.name = {
						.namespace_symbol_id =
							node->as.name_in_namespace.namespace_symbol_id,
						.name_symbol_id =
							node->as.name_in_namespace.symbol_id
					}
				};
				break;
			case PROTOTYPE_AST_VAR:
				if (prototype_source_core_handoff_binding_for_source(
						handoff, plan, node->as.var.ast_binder_id, &binding
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_VAR,
					.as.var.binding_id = binding
				};
				break;
			case PROTOTYPE_AST_APP:
				if (c0_source_slot_child(
						plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_FUNCTION,
						0, &first, NULL
					) != 0 || c0_source_slot_child(
						plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_ARGUMENT,
						0, &second, NULL
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_APP,
					.as.app = { .function = first, .argument = second }
				};
				break;
			case PROTOTYPE_AST_LAMBDA:
				if (c0_source_slot_binding(
						plan, handoff, slot_id, PROTOTYPE_SOURCE_BINDER_LAMBDA,
						0, &binding
					) != 0 || c0_source_slot_child(
						plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_BODY,
						0, &first, NULL
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_LAMBDA,
					.as.lambda = { .binding_id = binding, .body = first }
				};
				break;
			case PROTOTYPE_AST_INDUCTION_HYPOTHESIS: {
				uint32_t match_slot;
				if (prototype_source_core_handoff_binding_for_source(
						handoff, plan,
						node->as.induction_hypothesis.ast_binder_id,
						&binding
					) != 0 || c0_source_ih_match_slot(
						plan, node->as.induction_hypothesis.ast_binder_id,
						&match_slot
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				const struct prototype_source_core_slot_result* match_result =
					prototype_source_core_slot_result_get(handoff, match_slot);
				if (!match_result ||
					match_result->ih_scope_id == PROTOTYPE_INVALID_ID) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				struct prototype_core_formation_response argument_response;
				const struct prototype_core_formation_request argument_request = {
					.kind = PROTOTYPE_CORE_FORM_VAR,
					.as.var.binding_id = binding
				};
				if (c0_lowering_request(
						stage, &argument_request, &argument_response
					) != 0 || argument_response.term_id == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_INDUCTION_HYPOTHESIS,
					.as.induction_hypothesis = {
						.ih_scope_id = match_result->ih_scope_id,
						.argument = argument_response.term_id
					}
				};
				break;
			}
			case PROTOTYPE_AST_MATCH: {
				if (node->as.match.first_case > plan->source->case_count ||
					node->as.match.case_count > plan->source->case_count -
						node->as.match.first_case || c0_source_slot_child(
						plan, handoff, slot_id,
						PROTOTYPE_SOURCE_EDGE_SCRUTINEE, 0, &first, NULL
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				uint32_t total_binders = 0;
				for (uint32_t i = 0; i < node->as.match.case_count; ++i) {
					const struct prototype_ast_match_case* match_case =
						&plan->source->cases[node->as.match.first_case + i];
					if (match_case->binder_count > UINT32_MAX - total_binders ||
						match_case->selector_count >
							(UINT32_MAX - total_binders) / 2) {
						return -1;
					}
					total_binders += match_case->binder_count +
						2 * match_case->selector_count;
				}
				uint32_t case_count = node->as.match.case_count;
				struct prototype_core_match_case_request packet_cases[
					case_count == 0 ? 1 : case_count
				];
				struct prototype_core_case_binder_request case_binders[
					total_binders == 0 ? 1 : total_binders
				];
				size_t binder_bytes;
				size_t case_bytes;
				if (c0_checked_size_product(
						(size_t)total_binders,
						sizeof(struct prototype_core_case_binder_request),
						&binder_bytes
					) != 0 || c0_checked_size_product(
						(size_t)case_count,
						sizeof(struct prototype_core_match_case_request),
						&case_bytes
					) != 0) {
					return -1;
				}
				size_t padding_bytes = ((size_t)case_count + 1) *
					_Alignof(union prototype_core_request_alignment);
				if (binder_bytes > SIZE_MAX - case_bytes ||
					binder_bytes + case_bytes > SIZE_MAX - padding_bytes) {
					return -1;
				}
				size_t payload_capacity = binder_bytes + case_bytes + padding_bytes;
				size_t payload_size =
					prototype_core_request_payload_storage_size(payload_capacity);
				if (payload_size == 0) return -1;
				unsigned char payload_storage[payload_size];
				struct prototype_core_request_payload* payload;
				if (prototype_core_request_payload_init(
						payload_storage, payload_size, &payload
					) != 0) {
					return -1;
				}
				uint32_t next_binder = 0;
				for (uint32_t i = 0; i < case_count; ++i) {
					const struct prototype_ast_match_case* match_case =
						&plan->source->cases[node->as.match.first_case + i];
					if (match_case->first_binder >
							plan->source->case_binder_count ||
						match_case->binder_count >
							plan->source->case_binder_count -
								match_case->first_binder ||
						match_case->first_selector >
							plan->source->match_selector_count ||
						match_case->selector_count >
							plan->source->match_selector_count -
								match_case->first_selector || c0_source_slot_child(
							plan, handoff, slot_id,
							PROTOTYPE_SOURCE_EDGE_MATCH_CASE_BODY, i,
							&second, NULL
						) != 0) {
						return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
					}
					uint32_t first_binder = next_binder;
					for (uint32_t j = 0; j < match_case->binder_count; ++j) {
						const struct prototype_ast_binder* source_binder =
							&plan->source->case_binders[
								match_case->first_binder + j
							];
						if (prototype_source_core_handoff_binding_for_source(
								handoff, plan, source_binder->ast_binder_id,
								&binding
							) != 0) {
							return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
						}
						case_binders[next_binder++] =
							(struct prototype_core_case_binder_request) {
								.binding_id = binding,
								/* Recursive admissibility belongs to Layer T. */
								.is_recursive = 0
							};
					}
					for (uint32_t j = 0; j < match_case->selector_count; ++j) {
						const struct prototype_ast_match_selector* selector =
							&plan->source->match_selectors[
								match_case->first_selector + j
							];
						uint32_t source_ids[2] = {
							selector->value_ast_binder_id,
							selector->graph_ast_binder_id
						};
						for (uint32_t k = 0; k < 2; ++k) {
							if (prototype_source_core_handoff_binding_for_source(
									handoff, plan, source_ids[k], &binding
								) != 0) {
								return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
							}
							case_binders[next_binder++] =
								(struct prototype_core_case_binder_request) {
									.binding_id = binding,
									.is_recursive = 0
								};
						}
					}
					packet_cases[i] = (struct prototype_core_match_case_request) {
						.case_label_symbol_id = match_case->constructor_symbol_id,
						.constructor_owner = PROTOTYPE_INVALID_ID,
						.constructor_id = PROTOTYPE_INVALID_ID,
						.body = second
					};
					if (prototype_core_request_payload_append(
							payload, payload_size, &case_binders[first_binder],
							sizeof(struct prototype_core_case_binder_request),
							next_binder - first_binder,
							&packet_cases[i].binders
						) != 0) {
						return -1;
					}
				}
				struct prototype_core_request_range case_range = { 0 };
				if (prototype_core_request_payload_append(
						payload, payload_size, packet_cases,
						sizeof(struct prototype_core_match_case_request),
						case_count, &case_range
					) != 0 || prototype_core_request_payload_seal(
						payload, payload_size
					) != 0) {
					return -1;
				}
				const struct prototype_source_core_slot_result* slot_result =
					prototype_source_core_slot_result_get(handoff, slot_id);
				if (!slot_result) return -1;
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_MATCH,
					.payload_digest = payload->digest,
					.as.match = {
						.scrutinee = first,
						.cases = case_range,
						.ih_scope_id = slot_result->ih_scope_id
					}
				};
				return c0_source_match_submit(
					stage, handoff, slot_id, slot_result->ih_scope_id, &request,
					payload, payload_size
				);
			}
			case PROTOTYPE_AST_TYPE_LITERAL:
			case PROTOTYPE_AST_TYPE_FORMATION: {
				uint32_t type_def_id = node->as.type_formation.ast_type_def_id;
				int status = c0_source_type_declaration_spine(
					stage, plan, handoff, type_def_id, &first, &revision
				);
				if (status > 0) return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				if (status != 0 || c0_source_slot_record_alias(
						handoff, slot_id, first, revision
					) != 0) {
					return -1;
				}
				return PROTOTYPE_C0_SOURCE_SLOT_FORMED;
			}
			case PROTOTYPE_AST_TEXT_LITERAL:
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_TEXT_LITERAL,
					.as.text_literal.text_symbol_id =
						node->as.text_literal.text_symbol_id
				};
				break;
			case PROTOTYPE_AST_INT_LITERAL:
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_INT_LITERAL,
					.as.int_literal.value = node->as.int_literal.value
				};
				break;
			case PROTOTYPE_AST_SYSTEM_NAME:
				switch (node->as.system_name.kind) {
					case PROTOTYPE_AST_SYSTEM_NAME_HOST_TYPE:
						request = (struct prototype_core_formation_request) {
							.kind = PROTOTYPE_CORE_FORM_HOST_TYPE,
							.as.host_type.type_id =
								node->as.system_name.host_type_id
						};
						break;
					case PROTOTYPE_AST_SYSTEM_NAME_PURE_PRIMITIVE:
						request = (struct prototype_core_formation_request) {
							.kind = PROTOTYPE_CORE_FORM_PURE_PRIMITIVE,
							.as.pure_primitive = {
								.primitive_id =
									node->as.system_name.pure_primitive_id,
								.type_symbol_id =
									node->as.system_name.type_symbol_id
							}
						};
						break;
					case PROTOTYPE_AST_SYSTEM_NAME_EFFECT_OPERATION:
						request = (struct prototype_core_formation_request) {
							.kind = PROTOTYPE_CORE_FORM_EFFECT_OPERATION,
							.as.effect_operation.operation_id =
								node->as.system_name.effect_operation_id
						};
						break;
					default:
						return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				break;
			case PROTOTYPE_AST_ASCRIPTION:
				if (c0_source_slot_child(
						plan, handoff, slot_id,
						PROTOTYPE_SOURCE_EDGE_ASCRIBED_TERM, 0,
						&first, &revision
					) != 0 || c0_source_slot_record_alias(
						handoff, slot_id, first, revision
					) != 0) {
					return -1;
				}
				return PROTOTYPE_C0_SOURCE_SLOT_FORMED;
			case PROTOTYPE_AST_QUOTE:
				if (c0_source_slot_child(
						plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_UNARY_TERM,
						0, &first, NULL
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_THUNK,
					.as.thunk.computation = first
				};
				break;
			case PROTOTYPE_AST_BLOCK_EXPRESSION:
				if (c0_source_slot_child(
						plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_BLOCK_VALUE,
						0, &first, &revision
					) != 0 || c0_source_slot_record_alias(
						handoff, slot_id, first, revision
					) != 0) {
					return -1;
				}
				return PROTOTYPE_C0_SOURCE_SLOT_FORMED;
			case PROTOTYPE_AST_TERMINATES_WITNESS:
				if (c0_source_slot_child(
						plan, handoff, slot_id,
						PROTOTYPE_SOURCE_EDGE_CERTIFIED_COMPUTATION, 0,
						&first, NULL
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_TERMINATES_WITNESS,
					.as.terminates.computation = first
				};
				break;
			case PROTOTYPE_AST_COMPUTATION_FOLD: {
				if (node->as.computation_fold.first_clause >
						plan->source->computation_fold_clause_count ||
					node->as.computation_fold.clause_count >
						plan->source->computation_fold_clause_count -
							node->as.computation_fold.first_clause) {
					return -1;
				}
				if (c0_source_slot_child(
						plan, handoff, slot_id,
						PROTOTYPE_SOURCE_EDGE_FOLD_COMPUTATION, 0,
						&first, NULL
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				uint32_t return_body;
				uint32_t return_binding;
				if (c0_source_slot_child(
						plan, handoff, slot_id,
						PROTOTYPE_SOURCE_EDGE_FOLD_RETURN_BODY, 0,
						&return_body, NULL
					) != 0 || c0_source_slot_binding(
						plan, handoff, slot_id,
						PROTOTYPE_SOURCE_BINDER_FOLD_RETURN, 0,
						&return_binding
					) != 0) {
					return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
				}
				struct prototype_core_formation_response return_response;
				const struct prototype_core_formation_request return_request = {
					.kind = PROTOTYPE_CORE_FORM_LAMBDA,
					.as.lambda = {
						.binding_id = return_binding,
						.body = return_body
					}
				};
				if (c0_lowering_request(
						stage, &return_request, &return_response
					) != 0 || return_response.term_id == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				uint32_t clause_count = node->as.computation_fold.clause_count;
				struct prototype_core_computation_fold_clause_request clauses[
					clause_count == 0 ? 1 : clause_count
				];
				for (uint32_t i = 0; i < clause_count; ++i) {
					uint32_t operation;
					uint32_t body;
					uint32_t argument_binding;
					uint32_t continuation_binding;
					if (c0_source_slot_child(
							plan, handoff, slot_id,
							PROTOTYPE_SOURCE_EDGE_FOLD_OPERATION, i,
							&operation, NULL
						) != 0 || c0_source_slot_child(
							plan, handoff, slot_id,
							PROTOTYPE_SOURCE_EDGE_FOLD_CLAUSE_BODY, i,
							&body, NULL
						) != 0 || c0_source_slot_binding(
							plan, handoff, slot_id,
							PROTOTYPE_SOURCE_BINDER_FOLD_ARGUMENT, i,
							&argument_binding
						) != 0 || c0_source_slot_binding(
							plan, handoff, slot_id,
							PROTOTYPE_SOURCE_BINDER_FOLD_CONTINUATION, i,
							&continuation_binding
						) != 0) {
						return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
					}
					struct prototype_core_formation_response inner_response;
					const struct prototype_core_formation_request inner_request = {
						.kind = PROTOTYPE_CORE_FORM_LAMBDA,
						.as.lambda = {
							.binding_id = continuation_binding,
							.body = body
						}
					};
					if (c0_lowering_request(
							stage, &inner_request, &inner_response
						) != 0 || inner_response.term_id == PROTOTYPE_INVALID_ID) {
						return -1;
					}
					struct prototype_core_formation_response outer_response;
					const struct prototype_core_formation_request outer_request = {
						.kind = PROTOTYPE_CORE_FORM_LAMBDA,
						.as.lambda = {
							.binding_id = argument_binding,
							.body = inner_response.term_id
						}
					};
					if (c0_lowering_request(
							stage, &outer_request, &outer_response
						) != 0 || outer_response.term_id == PROTOTYPE_INVALID_ID) {
						return -1;
					}
					clauses[i] =
						(struct prototype_core_computation_fold_clause_request) {
						.operation = operation,
						.body = outer_response.term_id
					};
				}
				request = (struct prototype_core_formation_request) {
					.kind = PROTOTYPE_CORE_FORM_COMPUTATION_FOLD,
					.as.computation_fold = {
						.computation = first,
						.return_clause = return_response.term_id
					}
				};
				if (clause_count == 0) {
					return c0_source_slot_submit(
						stage, handoff, slot_id, &request, NULL, 0
					);
				}
				size_t clause_bytes;
				if (c0_checked_size_product(
						(size_t)clause_count, sizeof(clauses[0]), &clause_bytes
					) != 0 || clause_bytes > SIZE_MAX -
						_Alignof(union prototype_core_request_alignment)) {
					return -1;
				}
				size_t payload_capacity =
					clause_bytes +
						_Alignof(union prototype_core_request_alignment);
				size_t payload_size =
					prototype_core_request_payload_storage_size(payload_capacity);
				if (payload_size == 0) return -1;
				unsigned char payload_storage[payload_size];
				struct prototype_core_request_payload* payload;
				if (prototype_core_request_payload_init(
						payload_storage, payload_size, &payload
					) != 0 || prototype_core_request_payload_append(
						payload, payload_size, clauses, sizeof(clauses[0]),
						clause_count, &request.as.computation_fold.clauses
					) != 0 || prototype_core_request_payload_seal(
						payload, payload_size
					) != 0) {
					return -1;
				}
				request.payload_digest = payload->digest;
				return c0_source_slot_submit(
					stage, handoff, slot_id, &request, payload, payload_size
				);
			}
			default:
				return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
		}
		return c0_source_slot_submit(
			stage, handoff, slot_id, &request, NULL, 0
		);
	}

	if (slot->kind != PROTOTYPE_SOURCE_LOWERING_TYPE_EXPR ||
		slot->source_id >= plan->source->type_expr_count) {
		return -1;
	}
	const struct prototype_ast_type_expr* expr =
		&plan->source->type_exprs[slot->source_id];
	if (expr->tag == PROTOTYPE_AST_TYPE_EXPR_FUNCTION_GRAPH_REFERENCE ||
		expr->tag == PROTOTYPE_AST_TYPE_EXPR_ACCEPTED_SUBSTITUTION) {
		return c0_source_slot_record_nonterm(
			handoff, slot_id, PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1
		);
	}
	int nonterm_child = c0_source_slot_has_nonterm_child(plan, handoff, slot_id);
	if (nonterm_child < 0) return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
	if (nonterm_child > 0) {
		return c0_source_slot_record_nonterm(
			handoff, slot_id, PROTOTYPE_SOURCE_CORE_SLOT_REQUIRES_C1
		);
	}
	switch (expr->tag) {
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE:
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_UNIVERSE_VAR,
				.as.universe_var.level_var = expr->as.universe.level
			};
			break;
		case PROTOTYPE_AST_TYPE_EXPR_UNIVERSE_VAR:
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_UNIVERSE_VAR,
				.as.universe_var.level_var = expr->as.universe_var.level_var
			};
			break;
	case PROTOTYPE_AST_TYPE_EXPR_VAR:
			if (prototype_source_core_handoff_binding_for_source(
					handoff, plan, expr->as.var.ast_binder_id, &binding
				) != 0) {
				return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
			}
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_VAR,
				.as.var.binding_id = binding
			};
			break;
		case PROTOTYPE_AST_TYPE_EXPR_SELF: {
			int status = c0_source_type_declaration_spine(
				stage, plan, handoff, slot->owner_type_def_id,
				&first, &revision
			);
			if (status > 0) return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
			if (status != 0 || c0_source_slot_record_alias(
					handoff, slot_id, first, revision
				) != 0) {
				return -1;
			}
			return PROTOTYPE_C0_SOURCE_SLOT_FORMED;
		}
		case PROTOTYPE_AST_TYPE_EXPR_NAME:
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_EXTERNAL_REF,
				.as.external_ref.name = {
					.namespace_symbol_id = PROTOTYPE_BASE_NAMESPACE_ID,
					.name_symbol_id = expr->as.name.symbol_id
				}
			};
			break;
		case PROTOTYPE_AST_TYPE_EXPR_NAME_IN_NAMESPACE:
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_EXTERNAL_REF,
				.as.external_ref.name = {
					.namespace_symbol_id =
						expr->as.name_in_namespace.namespace_symbol_id,
					.name_symbol_id =
						expr->as.name_in_namespace.symbol_id
				}
			};
			break;
		case PROTOTYPE_AST_TYPE_EXPR_APP:
			if (c0_source_slot_child(
					plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_FUNCTION,
					0, &first, NULL
				) != 0 || c0_source_slot_child(
					plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_ARGUMENT,
					0, &second, NULL
				) != 0) {
				return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
			}
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_APP,
				.as.app = { .function = first, .argument = second }
			};
			break;
		case PROTOTYPE_AST_TYPE_EXPR_ARROW:
			if (c0_source_slot_child(
					plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_TYPE_DOMAIN,
					0, &first, NULL
				) != 0 || c0_source_slot_child(
					plan, handoff, slot_id,
					PROTOTYPE_SOURCE_EDGE_TYPE_CODOMAIN,
					0, &second, NULL
				) != 0) {
				return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
			}
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_PI,
				.as.pi = { .domain = first, .codomain = second }
			};
			break;
		case PROTOTYPE_AST_TYPE_EXPR_PI: {
			if (c0_source_slot_child(
					plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_TYPE_DOMAIN,
					0, &first, NULL
				) != 0 || c0_source_slot_child(
					plan, handoff, slot_id,
					PROTOTYPE_SOURCE_EDGE_TYPE_CODOMAIN,
					0, &second, NULL
				) != 0 || c0_source_slot_binding(
					plan, handoff, slot_id, PROTOTYPE_SOURCE_BINDER_PI,
					0, &binding
				) != 0) {
				return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
			}
			struct prototype_core_formation_response family_response;
			const struct prototype_core_formation_request family_request = {
				.kind = PROTOTYPE_CORE_FORM_PURE_FAMILY,
				.as.pure_family = {
					.binding_id = binding,
					.body = second
				}
			};
			if (c0_lowering_request(
					stage, &family_request, &family_response
				) != 0 || family_response.term_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_PI_FAMILY,
				.as.pi_family = {
					.domain = first,
					.codomain_family = family_response.term_id
				}
			};
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_COMPUTATION_REFERENCE: {
			if (c0_source_slot_child(
					plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_TYPE_RESULT,
					0, &first, NULL
				) != 0 || c0_source_slot_binding(
					plan, handoff, slot_id,
					PROTOTYPE_SOURCE_BINDER_COMPUTATION_EFFECT_ROW,
					0, &binding
				) != 0) {
				return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
			}
			struct prototype_core_formation_response row_response;
			const struct prototype_core_formation_request row_request = {
				.kind = PROTOTYPE_CORE_FORM_EFFECT_ROW_VAR,
				.as.effect_row_var.binding_id = binding
			};
			if (c0_lowering_request(
					stage, &row_request, &row_response
				) != 0 || row_response.term_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			struct prototype_core_formation_response computation_response;
			const struct prototype_core_formation_request computation_request = {
				.kind = PROTOTYPE_CORE_FORM_COMPUTATION_TYPE,
				.as.computation_type = {
					.label = row_response.term_id,
					.result = first,
					.totality = PROTOTYPE_COMPUTATION_TOTALITY_TOTAL
				}
			};
			if (c0_lowering_request(
					stage, &computation_request, &computation_response
				) != 0 || computation_response.term_id == PROTOTYPE_INVALID_ID) {
				return -1;
			}
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_THUNK_TYPE,
				.as.thunk_type.computation = computation_response.term_id
			};
			break;
		}
		case PROTOTYPE_AST_TYPE_EXPR_HOST_TYPE:
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_HOST_TYPE,
				.as.host_type.type_id = expr->as.host_type.host_type_id
			};
			break;
		case PROTOTYPE_AST_TYPE_EXPR_VALUE_REFERENCE:
			if (c0_source_slot_child(
					plan, handoff, slot_id, PROTOTYPE_SOURCE_EDGE_TYPE_VALUE,
					0, &first, &revision
				) != 0 || c0_source_slot_record_alias(
					handoff, slot_id, first, revision
				) != 0) {
				return -1;
			}
			return PROTOTYPE_C0_SOURCE_SLOT_FORMED;
		case PROTOTYPE_AST_TYPE_EXPR_TERMINATES:
			if (c0_source_slot_child(
					plan, handoff, slot_id,
					PROTOTYPE_SOURCE_EDGE_CERTIFIED_COMPUTATION,
					0, &first, NULL
				) != 0) {
				return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
			}
			request = (struct prototype_core_formation_request) {
				.kind = PROTOTYPE_CORE_FORM_TERMINATES_TYPE,
				.as.terminates.computation = first
			};
			break;
		default:
			return PROTOTYPE_C0_SOURCE_SLOT_DEFERRED;
	}
	return c0_source_slot_submit(stage, handoff, slot_id, &request, NULL, 0);
}

int prototype_c0_lowering_stage_form_open_plan(
	const struct prototype_c0_lowering_stage* stage,
	const struct prototype_source_lowering_plan* plan,
	struct prototype_source_core_handoff_builder* handoff,
	size_t* p_deferred_count
) {
	if (!stage || !plan || !handoff || handoff->sealed ||
		prototype_source_lowering_plan_validate(plan) != 0 ||
		plan->order_count != plan->slot_count ||
		handoff->slot_count != plan->slot_count ||
		handoff->binder_count != plan->binder_count ||
		handoff->source_fingerprint != plan->source_fingerprint ||
		prototype_c0_lowering_stage_preallocate_bindings(
			stage, plan, handoff
		) != 0 || c0_lowering_preallocate_ih_scopes(
			stage, plan, handoff
		) != 0) {
		return -1;
	}

	size_t deferred_count = 0;
	for (size_t i = 0; i < plan->order_count; ++i) {
		uint32_t slot_id = plan->order[i];
		int status = c0_lowering_stage_form_source_slot(
			stage, plan, handoff, slot_id
		);
		if (status == PROTOTYPE_C0_SOURCE_SLOT_DEFERRED) {
			++deferred_count;
			continue;
		}
		if (status != PROTOTYPE_C0_SOURCE_SLOT_FORMED) {
			return -1;
		}
	}
	if (p_deferred_count) *p_deferred_count = deferred_count;
	return 0;
}

int prototype_c0_lowering_stage_text_literal(
	const struct prototype_c0_lowering_stage* stage,
	int text_symbol_id,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_TEXT_LITERAL,
		.as.text_literal.text_symbol_id = text_symbol_id
	};
	return c0_lowering_submit(stage, &request, p_result);
}

int prototype_c0_lowering_stage_int_literal(
	const struct prototype_c0_lowering_stage* stage,
	int64_t value,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_INT_LITERAL,
		.as.int_literal.value = value
	};
	return c0_lowering_submit(stage, &request, p_result);
}

int prototype_c0_lowering_stage_host_type(
	const struct prototype_c0_lowering_stage* stage,
	int host_type_id,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_HOST_TYPE,
		.as.host_type.type_id = host_type_id
	};
	return c0_lowering_submit(stage, &request, p_result);
}

int prototype_c0_lowering_stage_pure_primitive(
	const struct prototype_c0_lowering_stage* stage,
	int primitive_id,
	int type_symbol_id,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_PURE_PRIMITIVE,
		.as.pure_primitive = {
			.primitive_id = primitive_id,
			.type_symbol_id = type_symbol_id
		}
	};
	return c0_lowering_submit(stage, &request, p_result);
}

int prototype_c0_lowering_stage_effect_operation(
	const struct prototype_c0_lowering_stage* stage,
	int operation_id,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_EFFECT_OPERATION,
		.as.effect_operation.operation_id = operation_id
	};
	return c0_lowering_submit(stage, &request, p_result);
}

int prototype_c0_lowering_stage_return(
	const struct prototype_c0_lowering_stage* stage,
	uint32_t value,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_RETURN,
		.as.return_term.value = value
	};
	return c0_lowering_submit(stage, &request, p_result);
}

int prototype_c0_lowering_stage_thunk(
	const struct prototype_c0_lowering_stage* stage,
	uint32_t computation,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_THUNK,
		.as.thunk.computation = computation
	};
	return c0_lowering_submit(stage, &request, p_result);
}

int prototype_c0_lowering_stage_force(
	const struct prototype_c0_lowering_stage* stage,
	uint32_t value,
	struct prototype_c0_open_term* p_result
) {
	struct prototype_core_formation_request request = {
		.kind = PROTOTYPE_CORE_FORM_FORCE,
		.as.force.value = value
	};
	return c0_lowering_submit(stage, &request, p_result);
}
