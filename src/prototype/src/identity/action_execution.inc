int prototype_hott_execute_substitution_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_result_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations = kernel ? kernel->operations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id || normalization_profile !=
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	int existing = hott_action_result_for_request(
		actions, request_id, p_result_id
	);
	if (existing <= 0) {
		return existing;
	}
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	const struct prototype_substitution* source = request &&
		request->kind == PROTOTYPE_HOTT_ACTION_SUBSTITUTION ?
		prototype_substitution_get(
			substitutions,
			request->key.substitution.source_substitution_id
		) : NULL;
	const struct prototype_hott_bridge* source_bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.substitution.source_bridge_id
		) : NULL;
	const struct prototype_hott_bridge* target_bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.substitution.target_bridge_id
		) : NULL;
	if (!source || !source_bridge || !target_bridge ||
		source->source_context != source_bridge->source_context_id ||
		source->target_context != target_bridge->source_context_id) {
		return -1;
	}
	uint32_t result_substitution;
	uint32_t result_substitution_certificate = PROTOTYPE_INVALID_ID;
	switch (source->kind) {
	case PROTOTYPE_SUBSTITUTION_IDENTITY:
		if (source_bridge->id != target_bridge->id ||
			prototype_substitution_identity(
				substitutions,
				contexts,
				source_bridge->bridge_context_id,
				&result_substitution
			) != 0) {
			return -1;
		}
		break;
	case PROTOTYPE_SUBSTITUTION_EMPTY:
		if (target_bridge->source_context_id != prototype_context_empty(contexts) ||
			prototype_substitution_empty(
				substitutions,
				contexts,
				source_bridge->bridge_context_id,
				&result_substitution
			) != 0) {
			return -1;
		}
		break;
	case PROTOTYPE_SUBSTITUTION_PROJECTION:
		if (prototype_substitution_projection_path(
				substitutions,
				contexts,
				source_bridge->bridge_context_id,
				target_bridge->bridge_context_id,
				&result_substitution
			) != 0) {
			return -1;
		}
		break;
	case PROTOTYPE_SUBSTITUTION_COMPOSE:
		{
			const struct prototype_substitution* outer =
				prototype_substitution_get(substitutions, source->first);
			const struct prototype_substitution* inner =
				prototype_substitution_get(substitutions, source->second);
			uint32_t middle_bridge_id;
			if (!outer || !inner || outer->source_context != inner->target_context ||
				!hott_bridge_for_source_context_and_semantics(
					bridges,
					outer->source_context,
					outer->source_context == prototype_context_empty(contexts) ?
						PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY :
						bridges->certificates[source_bridge->id].semantics,
					&middle_bridge_id
				)) {
				return -1;
			}
			struct prototype_hott_action_request inner_request = {
				.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
				.key.substitution = {
					.source_substitution_id = source->second,
					.source_bridge_id = source_bridge->id,
					.target_bridge_id = middle_bridge_id
				}
			};
			struct prototype_hott_action_request outer_request = {
				.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
				.key.substitution = {
					.source_substitution_id = source->first,
					.source_bridge_id = middle_bridge_id,
					.target_bridge_id = target_bridge->id
				}
			};
			uint32_t inner_request_id;
			uint32_t outer_request_id;
			uint32_t inner_result_id;
			uint32_t outer_result_id;
			if (prototype_hott_action_request_intern(
					actions, &view, bridges, inner_request, &inner_request_id
				) != 0 || prototype_hott_action_request_intern(
					actions, &view, bridges, outer_request, &outer_request_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, kernel, bridges, inner_request_id, normalization_profile,
					step_limit, &inner_result_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, kernel, bridges, outer_request_id, normalization_profile,
					step_limit, &outer_result_id
				) != 0) {
				return -1;
			}
			const struct prototype_hott_action_result* inner_result =
				prototype_hott_action_result_get(actions, inner_result_id);
			const struct prototype_hott_action_result* outer_result =
				prototype_hott_action_result_get(actions, outer_result_id);
			if (!inner_result || !outer_result ||
				inner_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
				outer_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
				return 1;
			}
			const struct prototype_hott_action_certificate* inner_certificate =
				&actions->certificates[inner_result->certificate_id];
			const struct prototype_hott_action_certificate* outer_certificate =
				&actions->certificates[outer_result->certificate_id];
			if (prototype_substitution_compose(
					substitutions,
					contexts,
					outer_certificate->data.substitution.result_substitution_id,
					inner_certificate->data.substitution.result_substitution_id,
					&result_substitution
				) != 0) {
				return -1;
			}
		}
		break;
	case PROTOTYPE_SUBSTITUTION_EXTEND:
		{
			const struct prototype_context* target = prototype_context_get(
				contexts, source->target_context
			);
			uint32_t parent_bridge_id;
			const struct prototype_hott_bridge* parent_bridge = target ?
				hott_bridge_for_source_context_and_semantics(
					bridges,
					target->parent,
					target->parent == prototype_context_empty(contexts) ?
						PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY :
						bridges->certificates[target_bridge->id].semantics,
					&parent_bridge_id
				) : NULL;
			uint32_t term_claim_id = PROTOTYPE_INVALID_ID;
			for (uint32_t i = 0; i < judgement->claim_count; ++i) {
				const struct prototype_judgement_claim* claim = &judgement->claims[i];
				if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == source->source_context &&
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == source->term &&
					prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier == source->term_classifier &&
					hott_operation_matches_claim(operations, judgement, claim)) {
					term_claim_id = i;
					break;
				}
			}
			uint32_t type_request_id = PROTOTYPE_INVALID_ID;
			for (uint32_t i = 0; i < actions->request_count; ++i) {
				const struct prototype_hott_action_request* candidate =
					&actions->requests[i];
				const struct prototype_judgement_claim* type_claim =
					candidate->kind == PROTOTYPE_HOTT_ACTION_RELATION_TYPE ?
					prototype_judgement_claim_get(
						judgement, candidate->key.relation_type.source_claim_id
					) : NULL;
				uint32_t candidate_result;
				if (type_claim && parent_bridge &&
					candidate->key.relation_type.source_bridge_id == parent_bridge_id &&
					prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->context_id == target->parent &&
					prototype_judgement_proposition_get(judgement, type_claim->proposition_id)->subject ==
						prototype_context_classifier_term(target) &&
					hott_action_result_for_request(
						actions, i, &candidate_result
					) == 0 && actions->results[candidate_result].outcome.state ==
						PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					type_request_id = i;
					break;
				}
			}
			if (!target || !parent_bridge || term_claim_id == PROTOTYPE_INVALID_ID ||
				type_request_id == PROTOTYPE_INVALID_ID) {
				return 1;
			}
			struct prototype_hott_action_request prefix_request = {
				.kind = PROTOTYPE_HOTT_ACTION_SUBSTITUTION,
				.key.substitution = {
					.source_substitution_id = source->first,
					.source_bridge_id = source_bridge->id,
					.target_bridge_id = parent_bridge_id
				}
			};
			struct prototype_hott_action_request term_request = {
				.kind = PROTOTYPE_HOTT_ACTION_TERM,
				.key.term = {
					.source_claim_id = term_claim_id,
					.source_bridge_id = source_bridge->id,
					.relation_type_action_request_id = type_request_id
				}
			};
			uint32_t prefix_request_id;
			uint32_t term_request_id;
			uint32_t prefix_result_id;
			uint32_t term_result_id;
			if (prototype_hott_action_request_intern(
					actions, &view, bridges, prefix_request, &prefix_request_id
				) != 0 || prototype_hott_action_request_intern(
					actions, &view, bridges, term_request, &term_request_id
				) != 0 || prototype_hott_execute_substitution_action(
					actions, kernel, bridges, prefix_request_id, normalization_profile,
					step_limit, &prefix_result_id
				) != 0 || prototype_hott_execute_term_action(
					actions, kernel, bridges,
					term_request_id, &term_result_id
				) != 0) {
				return -1;
			}
			const struct prototype_hott_action_result* prefix_result =
				prototype_hott_action_result_get(actions, prefix_result_id);
			const struct prototype_hott_action_result* term_result =
				prototype_hott_action_result_get(actions, term_result_id);
			if (!prefix_result || !term_result ||
				prefix_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
				term_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
				return 1;
			}
			const struct prototype_hott_action_certificate* term_certificate =
				&actions->certificates[term_result->certificate_id];
			const struct prototype_judgement_claim* witness_claim =
				prototype_judgement_claim_get(
					judgement,
					term_certificate->data.term.witness_has_type_claim_id
				);
			if (!witness_claim ||
				prototype_substitution_extend(
					substitutions,
					contexts,
					terms,
					type_declarations,
					term_certificate->data.term.
						endpoint_instantiation_substitution_id,
					target_bridge->bridge_context_id,
					term_certificate->data.term.witness_term_id,
					prototype_judgement_proposition_get(judgement, witness_claim->proposition_id)->classifier,
					&result_substitution
				) != 0 || prototype_cwf_certificate_db_add_substitution(
					cwf_certificates,
					substitutions,
					judgement,
					result_substitution,
					term_certificate->data.term.witness_has_type_claim_id,
					&result_substitution_certificate
				) != 0) {
				return -1;
			}
		}
		break;
	default:
		return -1;
	}
	uint32_t left_lhs;
	uint32_t left_rhs;
	uint32_t right_lhs;
	uint32_t right_rhs;
	struct prototype_term_conversion_result left_comparison;
	struct prototype_term_conversion_result right_comparison;
	if (prototype_substitution_compose(
			substitutions,
			contexts,
			target_bridge->left_substitution_id,
			result_substitution,
			&left_lhs
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			request->key.substitution.source_substitution_id,
			source_bridge->left_substitution_id,
			&left_rhs
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			target_bridge->right_substitution_id,
			result_substitution,
			&right_lhs
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			request->key.substitution.source_substitution_id,
			source_bridge->right_substitution_id,
			&right_rhs
		) != 0 || prototype_substitution_compare_pointwise(
			substitutions,
			contexts,
			terms,
			type_declarations,
			left_lhs,
			left_rhs,
			normalization_profile,
			step_limit,
			&left_comparison
		) != 0 || prototype_substitution_compare_pointwise(
			substitutions,
			contexts,
			terms,
			type_declarations,
			right_lhs,
			right_rhs,
			normalization_profile,
			step_limit,
			&right_comparison
		) != 0) {
		return -1;
	}
	if (left_comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
		right_comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		if (left_comparison.status != PROTOTYPE_TERM_CONVERSION_EXHAUSTED &&
			right_comparison.status != PROTOTYPE_TERM_CONVERSION_EXHAUSTED) {
			return -1;
		}
		struct prototype_hott_action_result residual = {
			.request_id = request_id,
			.certificate_id = PROTOTYPE_INVALID_ID,
			.outcome = {
				.state = PROTOTYPE_HOTT_ACTION_RESULT_RESIDUAL,
				.residual_reason = PROTOTYPE_HOTT_RESIDUAL_CONVERSION_EXHAUSTED,
				.normalization_profile = normalization_profile,
				.step_limit = step_limit,
				.term_graph_revision = terms->term_count
			}
		};
		memcpy(
			residual.outcome.calculus_fingerprint,
			PROTOTYPE_HOTT_CALCULUS_FINGERPRINT,
			65
		);
		return prototype_hott_action_result_publish(
			actions, residual, p_result_id
		);
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_SUBSTITUTION_NATURALITY,
		.data.substitution = {
			.result_substitution_id = result_substitution,
			.result_substitution_certificate_id = result_substitution_certificate,
			.left_naturality_lhs_substitution_id = left_lhs,
			.left_naturality_rhs_substitution_id = left_rhs,
			.right_naturality_lhs_substitution_id = right_lhs,
			.right_naturality_rhs_substitution_id = right_rhs,
			.normalization_profile = normalization_profile,
			.step_limit = step_limit,
			.term_graph_revision = terms->term_count
		}
	};
	uint32_t certificate_id;
	if (prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		normalization_profile,
		step_limit,
		p_result_id
	);
}

static int hott_bridge_db_finish_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge* parent,
	uint32_t source_context_id,
	uint32_t endpoint_context_id,
	uint32_t left_endpoint_binding_id,
	uint32_t right_endpoint_binding_id,
	uint32_t fiber_type_term_id,
	uint32_t fiber_type_claim_id,
	uint32_t fiber_action_certificate_id,
	uint32_t left_endpoint_context_certificate_id,
	uint32_t right_endpoint_context_certificate_id,
	int semantics,
	uint32_t* p_bridge_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_context* right_endpoint_context =
		prototype_context_get(contexts, endpoint_context_id);
	const struct prototype_context* left_endpoint_context = right_endpoint_context ?
		prototype_context_get(contexts, right_endpoint_context->parent) : NULL;
	const struct prototype_judgement_proposition* fiber_type = judgement ?
		prototype_judgement_claim_proposition(judgement, fiber_type_claim_id) : NULL;
	if (!bridges || !contexts || !substitutions || !cwf || !terms || !types ||
		!judgement || !parent || !source || !left_endpoint_context ||
		!right_endpoint_context || !fiber_type || !p_bridge_id ||
		source->parent != parent->source_context_id ||
		left_endpoint_context->parent != parent->bridge_context_id ||
		left_endpoint_context->binding_id != left_endpoint_binding_id ||
		right_endpoint_context->binding_id != right_endpoint_binding_id ||
		fiber_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		fiber_type->context_id != endpoint_context_id ||
		fiber_type->subject != fiber_type_term_id ||
		(semantics != PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION &&
		 semantics != PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY)) {
		return -1;
	}
	uint32_t proof_binding = prototype_term_new_binding(terms);
	uint32_t bridge_context;
	uint32_t bridge_context_certificate;
	uint32_t proof_claim;
	if (proof_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			endpoint_context_id,
			proof_binding,
			fiber_type_term_id,
			PROTOTYPE_INVALID_ID,
			&bridge_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			bridge_context,
			fiber_type_claim_id,
			&bridge_context_certificate
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			bridge_context,
			proof_binding,
			fiber_type_term_id,
			&proof_claim
		) != 0) {
		return -1;
	}
	uint32_t bridge_to_parent;
	uint32_t left_prefix;
	uint32_t right_prefix;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t left_substitution;
	uint32_t right_substitution;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			bridge_context,
			parent->bridge_context_id,
			&bridge_to_parent
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			parent->left_substitution_id,
			bridge_to_parent,
			&left_prefix
		) != 0 || prototype_substitution_compose(
			substitutions,
			contexts,
			parent->right_substitution_id,
			bridge_to_parent,
			&right_prefix
		) != 0 || prototype_term_var(
			terms, left_endpoint_binding_id, &left_endpoint
		) != 0 || prototype_term_var(
			terms, right_endpoint_binding_id, &right_endpoint
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			left_prefix,
			source_context_id,
			left_endpoint,
			prototype_context_classifier_term(left_endpoint_context),
			&left_substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			right_prefix,
			source_context_id,
			right_endpoint,
			prototype_context_classifier_term(right_endpoint_context),
			&right_substitution
		) != 0) {
		return -1;
	}
	uint32_t left_claim;
	uint32_t right_claim;
	uint32_t left_substitution_certificate;
	uint32_t right_substitution_certificate;
	if (prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			bridge_context,
			left_endpoint_binding_id,
			prototype_context_classifier_term(left_endpoint_context),
			&left_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			bridge_context,
			right_endpoint_binding_id,
			prototype_context_classifier_term(right_endpoint_context),
			&right_claim
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			left_substitution,
			left_claim,
			&left_substitution_certificate
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			right_substitution,
			right_claim,
			&right_substitution_certificate
		) != 0 || bridges->bridge_count >= bridges->bridge_capacity ||
		bridges->certificate_count >= bridges->certificate_capacity) {
		return -1;
	}
	struct prototype_hott_bridge bridge = {
		.id = (uint32_t)bridges->bridge_count,
		.source_context_id = source_context_id,
		.bridge_context_id = bridge_context,
		.left_substitution_id = left_substitution,
		.right_substitution_id = right_substitution
	};
	if (!hott_bridge_record_is_valid(
			&bridge,
			bridge.id,
			contexts,
			substitutions,
			cwf
		)) {
		return -1;
	}
	bridges->bridges[bridges->bridge_count++] = bridge;
	bridges->certificates[bridges->certificate_count++] =
		(struct prototype_hott_bridge_certificate) {
			.id = bridge.id,
			.bridge_id = bridge.id,
			.semantics = semantics,
			.parent_bridge_id = parent->id,
			.fiber_action_certificate_id = fiber_action_certificate_id,
			.fiber_witness_claim_id = proof_claim,
			.left_endpoint_context_certificate_id =
				left_endpoint_context_certificate_id,
			.right_endpoint_context_certificate_id =
				right_endpoint_context_certificate_id,
			.relation_context_certificate_id = bridge_context_certificate,
			.left_substitution_certificate_id = left_substitution_certificate,
			.right_substitution_certificate_id = right_substitution_certificate
		};
	*p_bridge_id = bridge.id;
	return 0;
}

int prototype_hott_bridge_db_construct_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t relation_type_action_request_id,
	uint32_t* p_bridge_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, relation_type_action_request_id);
	uint32_t result_id;
	if (!bridges || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !actions || !terms || !type_declarations ||
		!judgement || !source || !request || !p_bridge_id ||
		prototype_kernel_builder_validate(kernel) != 0 ||
		source_context_id == prototype_context_empty(contexts) ||
		request->kind != PROTOTYPE_HOTT_ACTION_RELATION_TYPE ||
		hott_action_result_for_request(
			actions, relation_type_action_request_id, &result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, result_id);
	const struct prototype_hott_action_certificate* type_certificate = result &&
		result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_claim* source_type =
		prototype_judgement_claim_get(
			judgement, request->key.relation_type.source_claim_id
		);
	const struct prototype_hott_bridge* parent = type_certificate ?
		prototype_hott_bridge_db_get(
			bridges, request->key.relation_type.source_bridge_id
		) : NULL;
	if (!type_certificate ||
		type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE ||
		!source_type || !parent || source->parent != parent->source_context_id ||
		prototype_judgement_proposition_get(judgement, source_type->proposition_id)->context_id != source->parent ||
		prototype_judgement_proposition_get(judgement, source_type->proposition_id)->subject != prototype_context_classifier_term(source)) {
		return -1;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* existing =
			&bridges->certificates[i];
		if (bridges->bridges[i].source_context_id == source_context_id &&
			existing->semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION &&
			existing->parent_bridge_id == parent->id &&
			existing->fiber_action_certificate_id == result->certificate_id) {
			*p_bridge_id = i;
			return 0;
		}
	}
	const struct prototype_hott_relation_type_action_certificate* type =
		&type_certificate->data.relation_type;
	return hott_bridge_db_finish_extension(
		bridges,
		kernel,
		parent,
		source_context_id,
		type->endpoint_context_id,
		type->left_endpoint_binding_id,
		type->right_endpoint_binding_id,
		type->relation_type_term_id,
		type->relation_is_type_claim_id,
		result->certificate_id,
		type->left_context_certificate_id,
		type->right_context_certificate_id,
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION,
		p_bridge_id
	);
}

int prototype_hott_bridge_db_construct_identity_extension(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t identity_type_action_request_id,
	uint32_t* p_bridge_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, identity_type_action_request_id);
	uint32_t result_id;
	if (!bridges || !contexts || !judgement || !actions || !source || !request ||
		!p_bridge_id || prototype_kernel_builder_validate(kernel) != 0 ||
		source_context_id == prototype_context_empty(contexts) ||
		request->kind != PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		hott_action_result_for_request(
			actions, identity_type_action_request_id, &result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, result_id);
	const struct prototype_hott_action_certificate* type_certificate = result &&
		result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_proposition* source_type =
		prototype_judgement_claim_proposition(
			judgement, request->key.identity_type.source_claim_id
		);
	const struct prototype_hott_bridge* parent = type_certificate ?
		prototype_hott_bridge_db_get(
			bridges, request->key.identity_type.source_bridge_id
		) : NULL;
	const struct prototype_hott_bridge_certificate* parent_certificate = parent ?
		&bridges->certificates[parent->id] : NULL;
	if (!type_certificate || type_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		!source_type || !parent || !parent_certificate ||
		(parent_certificate->semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY &&
		 parent_certificate->semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY) ||
		source->parent != parent->source_context_id ||
		source_type->context_id != source->parent ||
		source_type->subject != prototype_context_classifier_term(source)) {
		return -1;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* existing =
			&bridges->certificates[i];
		if (bridges->bridges[i].source_context_id == source_context_id &&
			existing->semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY &&
			existing->parent_bridge_id == parent->id &&
			existing->fiber_action_certificate_id == result->certificate_id) {
			*p_bridge_id = i;
			return 0;
		}
	}
	const struct prototype_hott_identity_type_computation_certificate* type =
		&type_certificate->data.identity_type;
	return hott_bridge_db_finish_extension(
		bridges,
		kernel,
		parent,
		source_context_id,
		type->endpoint_context_id,
		type->left_endpoint_binding_id,
		type->right_endpoint_binding_id,
		type->identity_type_term_id,
		type->identity_type_is_type_claim_id,
		result->certificate_id,
		type->left_context_certificate_id,
		type->right_context_certificate_id,
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY,
		p_bridge_id
	);
}

int prototype_hott_bridge_db_ensure_identity_context(
	struct prototype_hott_bridge_db* bridges,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_action_db* actions,
	uint32_t source_context_id,
	uint32_t* p_bridge_id,
	int* p_residual_reason
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_hott_action_request request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = PROTOTYPE_INVALID_ID,
			.source_bridge_id = PROTOTYPE_INVALID_ID
		}
	};
	struct prototype_kernel_view view;
	if (!bridges || !kernel || !actions || !contexts || !cwf_certificates ||
		!terms || !p_bridge_id || !p_residual_reason ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	int semantics = source_context_id == prototype_context_empty(contexts) ?
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY :
		PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY;
	if (hott_bridge_for_source_context_and_semantics(
			bridges, source_context_id, semantics, p_bridge_id
		)) {
		*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
		return 0;
	}
	const struct prototype_context* source = prototype_context_get(
		contexts, source_context_id
	);
	const struct prototype_cwf_certificate* source_certificate =
		hott_context_formation_certificate_for_context(
			cwf_certificates, source_context_id
		);
	if (!source || !source_certificate ||
		source->parent == PROTOTYPE_INVALID_ID ||
		source_certificate->claim_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t parent_bridge_id;
	int parent_status = prototype_hott_bridge_db_ensure_identity_context(
		bridges,
		kernel,
		actions,
		source->parent,
		&parent_bridge_id,
		p_residual_reason
	);
	if (parent_status != 0) {
		return parent_status;
	}
	request.key.identity_type.source_claim_id = source_certificate->claim_id;
	request.key.identity_type.source_bridge_id = parent_bridge_id;
	uint32_t request_id;
	uint32_t result_id;
	int request_status = prototype_hott_action_request_intern(
		actions, &view, bridges, request, &request_id
	);
	int execution_status = request_status == 0 ?
		prototype_hott_execute_identity_type_computation(
			actions, kernel, bridges, request_id, &result_id
		) : -1;
	if (request_status != 0 || execution_status != 0) {
		fprintf(
			stderr,
			"identity Context bridge action failed context=%u parent=%u claim=%u "
			"parent_bridge=%u request=%u request_status=%d execution_status=%d\n",
			source_context_id,
			source->parent,
			source_certificate->claim_id,
			parent_bridge_id,
			request_status == 0 ? request_id : PROTOTYPE_INVALID_ID,
			request_status,
			execution_status
		);
		return -1;
	}
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, result_id);
	if (!result) {
		return -1;
	}
	if (result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = result->outcome.residual_reason;
		return 1;
	}
	if (prototype_hott_bridge_db_construct_identity_extension(
			bridges,
			kernel,
			actions,
			source_context_id,
			request_id,
			p_bridge_id
		) != 0) {
		fprintf(
			stderr,
			"identity Context extension failed context=%u request=%u\n",
			source_context_id,
			request_id
		);
		return -1;
	}
	*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	return 0;
}

static int hott_relation_plan_and_execute_depth(
	struct prototype_parametricity_relation_goal_db* goals,
	struct prototype_parametricity_candidate_db* candidates,
	struct prototype_parametricity_work_db* work,
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	struct prototype_hott_relation_execution* p_execution,
	uint32_t depth
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!goals || !candidates || !work || !actions || !contexts ||
		!substitutions || !cwf_certificates || !cwf_certificates ||
		!bridges || !terms || !type_declarations || !operations || !judgement ||
		!p_execution || depth > 512 ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	struct prototype_hott_relation_execution execution = {
		.work_item_id = PROTOTYPE_INVALID_ID,
		.relation_type_action_request_id = PROTOTYPE_INVALID_ID,
		.relation_type_action_result_id = PROTOTYPE_INVALID_ID,
		.term_action_request_id = PROTOTYPE_INVALID_ID,
		.term_action_result_id = PROTOTYPE_INVALID_ID,
		.materialization_state = PROTOTYPE_HOTT_MATERIALIZATION_INVALID,
		.relation_witness_claim_id = PROTOTYPE_INVALID_ID
	};
	if (prototype_parametricity_relation_plan(
			goals,
			candidates,
			work,
			&view,
			bridges,
			definitions,
			goal_id,
			source_ast,
			normalization_profile,
			step_limit,
			&execution.work_item_id
		) != 0) {
		return -1;
	}
	const struct prototype_parametricity_work_item* item =
		prototype_parametricity_work_db_get(work, execution.work_item_id);
	const struct prototype_parametricity_relation_goal* goal =
		prototype_parametricity_relation_goal_db_get(goals, goal_id);
	if (!item || !goal) {
		return -1;
	}
	if (item->outcome.state != PROTOTYPE_HOTT_WORK_READY) {
		*p_execution = execution;
		return 0;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = goal->left_carrier_claim_id,
			.source_bridge_id = goal->bridge_id
		}
	};
	if (prototype_hott_action_request_intern(
			actions, &view, bridges,
			type_request,
			&execution.relation_type_action_request_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			actions, kernel, bridges,
			execution.relation_type_action_request_id,
			&execution.relation_type_action_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(
			actions, execution.relation_type_action_result_id
		);
	const struct prototype_parametricity_candidate* selected =
		prototype_parametricity_candidate_db_get(
			candidates, item->selected_candidate_id
		);
	if (!type_result || !selected) {
		return -1;
	}
	if (type_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
		*p_execution = execution;
		return 0;
	}
	if (selected->object_result ==
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_EMPTY_FAMILY) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY;
		*p_execution = execution;
		return 0;
	}
	if (selected->object_result !=
			PROTOTYPE_HOTT_CANDIDATE_OBJECT_RELATION_WITNESS) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
		*p_execution = execution;
		return 0;
	}
	const struct prototype_hott_action_certificate* type_certificate =
		type_result->certificate_id < actions->certificate_count ?
		&actions->certificates[type_result->certificate_id] : NULL;
	const struct prototype_hott_bridge* bridge =
		prototype_hott_bridge_db_get(bridges, goal->bridge_id);
	if (!type_certificate || type_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE || !bridge) {
		return -1;
	}
	if (selected->rule == PROTOTYPE_HOTT_RULE_REL_CONVERT) {
		struct hott_relation_instantiation instantiation;
		if (hott_instantiate_relation_for_claims(
				kernel,
				bridge,
				&type_certificate->data.relation_type,
				goal->left_claim_id,
				goal->right_claim_id,
				goal->left_carrier_claim_id,
				goal->right_carrier_claim_id,
				&instantiation
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* left_endpoint =
			prototype_judgement_claim_proposition(
				judgement, instantiation.left_endpoint_claim_id
			);
		const struct prototype_judgement_proposition* right_endpoint =
			prototype_judgement_claim_proposition(
				judgement, instantiation.right_endpoint_claim_id
			);
		uint32_t witness;
		if (!left_endpoint || !right_endpoint ||
			prototype_term_relation_witness(
				terms,
				left_endpoint->subject,
				right_endpoint->subject,
				&witness
			) != 0 || prototype_judgement_add_relation_witness_intro(
				judgement,
				terms,
				type_declarations,
				bridge->bridge_context_id,
				witness,
				instantiation.relation_type_term_id,
				instantiation.relation_is_type_claim_id,
				instantiation.left_endpoint_claim_id,
				instantiation.right_endpoint_claim_id,
				&execution.relation_witness_claim_id
			) != 0) {
			return -1;
		}
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS;
		*p_execution = execution;
		return 0;
	}
	if (selected->rule == PROTOTYPE_HOTT_RULE_REL_ADT_CONSTRUCTOR) {
		if (selected->child_edge_count >
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return -1;
		}
		uint32_t field_witness_claims[
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
		];
		for (uint32_t i = 0; i < selected->child_edge_count; ++i) {
			const struct prototype_parametricity_child_edge* edge =
				&candidates->child_edges[selected->first_child_edge + i];
			struct prototype_hott_relation_execution child_execution;
			if (edge->candidate_id != selected->id ||
				edge->role != PROTOTYPE_HOTT_CHILD_ADT_FIELD ||
				edge->ordinal != i ||
				hott_relation_plan_and_execute_depth(
					goals,
					candidates,
					work,
					actions,
					kernel,
					bridges,
					definitions,
					edge->child_goal_id,
					source_ast,
					normalization_profile,
					step_limit,
					&child_execution,
					depth + 1
				) != 0) {
				return -1;
			}
			if (child_execution.materialization_state ==
					PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY) {
				execution.materialization_state =
					PROTOTYPE_HOTT_MATERIALIZATION_EMPTY_FAMILY;
				*p_execution = execution;
				return 0;
			}
			if (child_execution.materialization_state !=
					PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS ||
				child_execution.relation_witness_claim_id == PROTOTYPE_INVALID_ID) {
				execution.materialization_state =
					PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
				*p_execution = execution;
				return 0;
			}
			field_witness_claims[i] =
				child_execution.relation_witness_claim_id;
		}
		struct hott_relation_instantiation instantiation;
		if (hott_instantiate_relation_for_claims(
				kernel,
				bridge,
				&type_certificate->data.relation_type,
				goal->left_claim_id,
				goal->right_claim_id,
				goal->left_carrier_claim_id,
				goal->right_carrier_claim_id,
				&instantiation
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* left_endpoint =
			prototype_judgement_claim_proposition(
				judgement, instantiation.left_endpoint_claim_id
			);
		const struct prototype_judgement_proposition* right_endpoint =
			prototype_judgement_claim_proposition(
				judgement, instantiation.right_endpoint_claim_id
			);
		uint32_t witness;
		if (!left_endpoint || !right_endpoint ||
			prototype_term_relation_witness(
				terms,
				left_endpoint->subject,
				right_endpoint->subject,
				&witness
			) != 0 || prototype_judgement_add_relation_constructor_witness(
				judgement,
				terms,
				bridge->bridge_context_id,
				witness,
				instantiation.relation_type_term_id,
				instantiation.relation_is_type_claim_id,
				instantiation.left_endpoint_claim_id,
				instantiation.right_endpoint_claim_id,
				field_witness_claims,
				selected->child_edge_count,
				&execution.relation_witness_claim_id
			) != 0) {
			return -1;
		}
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS;
		*p_execution = execution;
		return 0;
	}
	if (selected->rule != PROTOTYPE_HOTT_RULE_REL_DIAGONAL) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
		*p_execution = execution;
		return 0;
	}
	struct prototype_hott_action_request term_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = goal->left_claim_id,
			.source_bridge_id = goal->bridge_id,
			.relation_type_action_request_id = execution.relation_type_action_request_id
		}
	};
	if (prototype_hott_action_request_intern(
			actions, &view, bridges,
			term_request,
			&execution.term_action_request_id
		) != 0 || prototype_hott_execute_term_action(
			actions, kernel, bridges,
			execution.term_action_request_id,
			&execution.term_action_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* term_result =
		prototype_hott_action_result_get(
			actions, execution.term_action_result_id
		);
	const struct prototype_hott_action_certificate* term_certificate =
		term_result && term_result->outcome.state ==
			PROTOTYPE_HOTT_ACTION_RESULT_READY &&
		term_result->certificate_id < actions->certificate_count ?
		&actions->certificates[term_result->certificate_id] : NULL;
	if (!term_certificate || term_certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM) {
		execution.materialization_state =
			PROTOTYPE_HOTT_MATERIALIZATION_RESIDUAL;
		*p_execution = execution;
		return 0;
	}
	execution.materialization_state =
		PROTOTYPE_HOTT_MATERIALIZATION_RELATION_WITNESS;
	execution.relation_witness_claim_id =
		term_certificate->data.term.witness_has_type_claim_id;
	*p_execution = execution;
	return 0;
}

int prototype_parametricity_relation_plan_and_execute(
	struct prototype_parametricity_relation_goal_db* goals,
	struct prototype_parametricity_candidate_db* candidates,
	struct prototype_parametricity_work_db* work,
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	const struct prototype_term_definition_env* definitions,
	uint32_t goal_id,
	uint32_t source_ast,
	int normalization_profile,
	uint64_t step_limit,
	struct prototype_hott_relation_execution* p_execution
) {
	return hott_relation_plan_and_execute_depth(
		goals,
		candidates,
		work,
		actions,
		kernel,
		bridges,
		definitions,
		goal_id,
		source_ast,
		normalization_profile,
		step_limit,
		p_execution,
		0
	);
}

int prototype_hott_action_db_validate(
	const struct prototype_hott_action_db* db,
	const struct prototype_kernel_view* kernel,
	const struct prototype_hott_bridge_db* bridges
) {
	const struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	const struct prototype_substitution_db* substitutions =
		kernel ? kernel->substitutions : NULL;
	const struct prototype_cwf_certificate_db* cwf_certificates =
		kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	const struct prototype_operation_graph* operations =
		kernel ? kernel->operations : NULL;
	const struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!db || db->request_count > db->request_capacity ||
		db->certificate_count > db->certificate_capacity ||
		db->result_count > db->result_capacity ||
		(db->request_count != 0 && !db->requests) ||
		(db->certificate_count != 0 && !db->certificates) ||
		(db->result_count != 0 && !db->results) ||
		prototype_kernel_view_validate(kernel) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		const struct prototype_hott_bridge_certificate* bridge_certificate =
			&bridges->certificates[i];
		if (bridge_certificate->parent_bridge_id == PROTOTYPE_INVALID_ID) {
			continue;
		}
		if (bridge_certificate->fiber_action_certificate_id >=
			db->certificate_count) {
			return -1;
		}
		const struct prototype_hott_action_certificate* type_certificate =
			&db->certificates[bridge_certificate->fiber_action_certificate_id];
		const struct prototype_hott_action_request* type_request =
			prototype_hott_action_request_get(db, type_certificate->request_id);
		uint32_t source_claim_id = PROTOTYPE_INVALID_ID;
		uint32_t source_bridge_id = PROTOTYPE_INVALID_ID;
		uint32_t endpoint_context_id = PROTOTYPE_INVALID_ID;
		uint32_t fiber_type_term_id = PROTOTYPE_INVALID_ID;
		uint32_t fiber_type_claim_id = PROTOTYPE_INVALID_ID;
		uint32_t left_context_certificate_id = PROTOTYPE_INVALID_ID;
		uint32_t right_context_certificate_id = PROTOTYPE_INVALID_ID;
		if (bridge_certificate->semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION &&
			type_certificate->kind ==
				PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE &&
			type_request && type_request->kind ==
				PROTOTYPE_HOTT_ACTION_RELATION_TYPE) {
			source_claim_id = type_request->key.relation_type.source_claim_id;
			source_bridge_id = type_request->key.relation_type.source_bridge_id;
			endpoint_context_id =
				type_certificate->data.relation_type.endpoint_context_id;
			fiber_type_term_id =
				type_certificate->data.relation_type.relation_type_term_id;
			fiber_type_claim_id =
				type_certificate->data.relation_type.relation_is_type_claim_id;
			left_context_certificate_id =
				type_certificate->data.relation_type.left_context_certificate_id;
			right_context_certificate_id =
				type_certificate->data.relation_type.right_context_certificate_id;
		} else if (bridge_certificate->semantics ==
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY &&
			type_certificate->kind ==
				PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION &&
			type_request && type_request->kind ==
				PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION) {
			source_claim_id = type_request->key.identity_type.source_claim_id;
			source_bridge_id = type_request->key.identity_type.source_bridge_id;
			endpoint_context_id =
				type_certificate->data.identity_type.endpoint_context_id;
			fiber_type_term_id =
				type_certificate->data.identity_type.identity_type_term_id;
			fiber_type_claim_id =
				type_certificate->data.identity_type.identity_type_is_type_claim_id;
			left_context_certificate_id =
				type_certificate->data.identity_type.left_context_certificate_id;
			right_context_certificate_id =
				type_certificate->data.identity_type.right_context_certificate_id;
		} else {
			return -1;
		}
		const struct prototype_judgement_claim* source_type =
			prototype_judgement_claim_get(judgement, source_claim_id);
		const struct prototype_context* source = prototype_context_get(
			contexts, bridges->bridges[i].source_context_id
		);
		const struct prototype_context* relation = prototype_context_get(
			contexts, bridges->bridges[i].bridge_context_id
		);
		if (source_bridge_id != bridge_certificate->parent_bridge_id ||
			!source_type || !source || !relation ||
			prototype_judgement_proposition_get(judgement, source_type->proposition_id)->context_id != source->parent ||
			prototype_judgement_proposition_get(judgement, source_type->proposition_id)->subject != prototype_context_classifier_term(source) ||
			endpoint_context_id != relation->parent ||
			fiber_type_term_id != prototype_context_classifier_term(relation) ||
			bridge_certificate->left_endpoint_context_certificate_id !=
				left_context_certificate_id ||
			bridge_certificate->right_endpoint_context_certificate_id !=
				right_context_certificate_id ||
			bridge_certificate->relation_context_certificate_id >=
				cwf_certificates->certificate_count ||
			cwf_certificates->certificates[
				bridge_certificate->relation_context_certificate_id
			].claim_id != fiber_type_claim_id) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < db->request_count; ++i) {
		const struct prototype_hott_action_request* request = &db->requests[i];
		if (!hott_action_request_is_valid(
				db,
				request,
				i,
				contexts,
				substitutions,
				cwf_certificates,
				bridges,
				terms,
				type_declarations,
				operations,
				judgement
			)) {
			return -1;
		}
		size_t bucket =
			request->key_hash % PROTOTYPE_HOTT_ACTION_INDEX_BUCKET_COUNT;
		uint32_t found = PROTOTYPE_INVALID_ID;
		for (uint32_t j = db->request_index_heads[bucket];
			j != PROTOTYPE_INVALID_ID;
			j = db->requests[j].hash_next) {
			if (j >= db->request_count) {
				return -1;
			}
			if (hott_action_request_key_equal(request, &db->requests[j])) {
				if (found != PROTOTYPE_INVALID_ID) {
					return -1;
				}
				found = j;
			}
		}
		if (found != i) {
			return -1;
		}
	}
	for (uint32_t i = 0; i < db->certificate_count; ++i) {
		if (!hott_action_certificate_is_valid(
				db,
				&db->certificates[i],
				i,
				contexts,
				substitutions,
				cwf_certificates,
				bridges,
				terms,
				type_declarations,
				operations,
				judgement
			)) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (db->certificates[j].request_id ==
				db->certificates[i].request_id) {
				return -1;
			}
		}
	}
	for (uint32_t i = 0; i < db->result_count; ++i) {
		const struct prototype_hott_action_result* result = &db->results[i];
		if (result->id != i ||
			!prototype_hott_action_request_get(db, result->request_id) ||
			!hott_deterministic_outcome_is_valid(&result->outcome, 0) ||
			(result->outcome.state == PROTOTYPE_HOTT_ACTION_RESULT_READY &&
			 (result->certificate_id >= db->certificate_count ||
			  db->certificates[result->certificate_id].request_id !=
				result->request_id)) ||
			(result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY &&
			 result->certificate_id != PROTOTYPE_INVALID_ID)) {
			return -1;
		}
		for (uint32_t j = 0; j < i; ++j) {
			if (db->results[j].request_id == result->request_id) {
				return -1;
			}
		}
	}
	return 0;
}
