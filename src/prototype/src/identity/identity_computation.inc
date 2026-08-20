static uint32_t hott_has_type_claim_for_subject(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject
);


static int hott_publish_action_residual(
	struct prototype_hott_action_db* actions,
	const struct prototype_term_db* terms,
	uint32_t request_id,
	int reason,
	uint32_t* p_result_id
) {
	struct prototype_hott_action_result result = {
		.request_id = request_id,
		.certificate_id = PROTOTYPE_INVALID_ID,
		.outcome = {
			.state = reason == PROTOTYPE_HOTT_RESIDUAL_UNIVERSE ||
				reason == PROTOTYPE_HOTT_RESIDUAL_HOST_PRIMITIVE ?
				PROTOTYPE_HOTT_ACTION_RESULT_UNSUPPORTED :
				PROTOTYPE_HOTT_ACTION_RESULT_RESIDUAL,
			.residual_reason = reason,
			.normalization_profile = PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			.step_limit = 0,
			.term_graph_revision = terms->term_count
		}
	};
	memcpy(result.outcome.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT, 65);
	return prototype_hott_action_result_publish(actions, result, p_result_id);
}

static int hott_publish_ready_action_result(
	struct prototype_hott_action_db* actions,
	const struct prototype_term_db* terms,
	uint32_t request_id,
	uint32_t certificate_id,
	int normalization_profile,
	uint64_t step_limit,
	uint32_t* p_result_id
) {
	if (!actions || !terms || !p_result_id || certificate_id >=
		actions->certificate_count || certificate_id + 1 !=
		actions->certificate_count || actions->certificates[certificate_id].request_id !=
		request_id) {
		return -1;
	}
	struct prototype_hott_action_result result = {
		.request_id = request_id,
		.certificate_id = certificate_id,
		.outcome = {
			.state = PROTOTYPE_HOTT_ACTION_RESULT_READY,
			.residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE,
			.normalization_profile = normalization_profile,
			.step_limit = step_limit,
			.term_graph_revision = terms->term_count
		}
	};
	memcpy(result.outcome.calculus_fingerprint, PROTOTYPE_HOTT_CALCULUS_FINGERPRINT, 65);
	if (prototype_hott_action_result_publish(actions, result, p_result_id) != 0) {
		memset(&actions->certificates[certificate_id], 0,
			sizeof(actions->certificates[certificate_id]));
		actions->certificate_count--;
		return -1;
	}
	return 0;
}

struct hott_identity_endpoint_setup {
	uint32_t left_type_claim_id;
	uint32_t right_type_claim_id;
	uint32_t left_type_term_id;
	uint32_t right_type_term_id;
	uint32_t left_binding_id;
	uint32_t right_binding_id;
	uint32_t left_context_id;
	uint32_t endpoint_context_id;
	uint32_t left_context_certificate_id;
	uint32_t right_context_certificate_id;
};

static int hott_prepare_identity_endpoints(
	struct prototype_kernel_builder* kernel,
	uint32_t source_type_claim_id,
	const struct prototype_hott_bridge* bridge,
	struct hott_identity_endpoint_setup* p_setup
);

static int hott_ensure_dimension_action_type_claim(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t source_type,
	uint32_t universe,
	uint32_t operator_id,
	uint32_t* p_acted_type,
	uint32_t* p_acted_type_claim
);

static int hott_apply_claim_argument(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t function_claim_id,
	uint32_t argument_claim_id,
	uint32_t* p_term_id,
	uint32_t* p_claim_id
);

int prototype_hott_ensure_dimension_classifier_formation(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t classifier,
	uint32_t universe
);

static int hott_publish_dimension_action_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	int computation_rule,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_dimension_operator_db* dimension_operators = kernel ?
		kernel->dimension_operators : NULL;
	const struct prototype_hott_action_request* request =
		prototype_hott_action_request_get(actions, request_id);
	const struct prototype_dimension_operator* dimension_operator = bridge &&
		dimension_operators ? prototype_dimension_operator_get(
			dimension_operators, bridge->dimension_operator_id
		) : NULL;
	if (!actions || !terms || !types || !contexts || !substitutions ||
		!judgement || !dimension_operators || !source || !bridge || !request ||
		!p_result_id || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source->context_id != bridge->source_context_id ||
		dimension_operator == NULL || dimension_operator->source_dimension != 0 ||
		dimension_operator->target_dimension != 1) {
		return -1;
	}

	struct hott_identity_endpoint_setup endpoints;
	if (hott_prepare_identity_endpoints(
			kernel,
			request->key.identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		fprintf(stderr,
			"dimension action identity endpoint preparation failed request=%u\n",
			request_id);
		return -1;
	}

	uint32_t acted_type;
	uint32_t acted_classifier;
	uint32_t acted_classifier_has_type_claim;
	uint32_t acted_classifier_is_type_claim;
	uint32_t acted_type_claim;
	if (prototype_term_dimension_action(
			terms,
			dimension_operators,
			source->subject,
			bridge->dimension_operator_id,
			&acted_type
		) != 0 || prototype_dimension_action_type_classifier(
			terms,
			dimension_operators,
			source->subject,
			acted_type,
			source->classifier,
			1,
			&acted_classifier
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			acted_classifier,
			source->classifier,
			&acted_classifier_has_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			source->context_id,
			acted_classifier,
			source->classifier,
			acted_classifier_has_type_claim,
			&acted_classifier_is_type_claim
		) != 0 || prototype_judgement_add_dimension_action_type_formation(
			judgement,
			terms,
			types,
			contexts,
			dimension_operators,
			source->context_id,
			acted_type,
			acted_classifier,
			request->key.identity_type.source_claim_id,
			&acted_type_claim
		) != 0) {
		fprintf(stderr,
			"dimension action identity type formation failed request=%u source=%u\n",
			request_id,
			source->subject);
		return -1;
	}

	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t endpoint_acted_type_term;
	uint32_t endpoint_acted_type_claim;
	uint32_t left_endpoint_claim;
	uint32_t right_endpoint_claim;
	uint32_t left_identity_type;
	uint32_t left_identity_classifier;
	uint32_t left_identity_claim;
	uint32_t identity_type;
	uint32_t identity_type_has_type_claim;
	uint32_t identity_type_is_type_claim;
	if (prototype_term_var(
			terms, endpoints.left_binding_id, &left_endpoint
		) != 0 || prototype_term_var(
			terms, endpoints.right_binding_id, &right_endpoint
		) != 0 || hott_ensure_dimension_action_type_claim(
			kernel,
			endpoints.endpoint_context_id,
			source->subject,
			source->classifier,
			bridge->dimension_operator_id,
			&endpoint_acted_type_term,
			&endpoint_acted_type_claim
		) != 0 || endpoint_acted_type_term != acted_type ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoints.endpoint_context_id,
			endpoints.left_binding_id,
			endpoints.left_type_term_id,
			&left_endpoint_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoints.endpoint_context_id,
			endpoints.right_binding_id,
			endpoints.right_type_term_id,
			&right_endpoint_claim
		) != 0 || prototype_term_app(
			terms, acted_type, left_endpoint, &left_identity_type
		) != 0) {
		fprintf(stderr,
			"dimension action identity endpoint setup failed request=%u\n",
			request_id);
		return -1;
	}
	const struct prototype_judgement_proposition* endpoint_acted_type =
		prototype_judgement_claim_proposition(
			judgement, endpoint_acted_type_claim
		);
	uint32_t classifier_binder;
	uint32_t classifier_body;
	if (!endpoint_acted_type || endpoint_acted_type->classifier >=
			terms->term_count || terms->terms[
			endpoint_acted_type->classifier
		].tag != PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
			terms,
			terms->terms[
				endpoint_acted_type->classifier
			].as.pi.codomain_family,
			&classifier_binder,
			&classifier_body
		) != 0 || prototype_term_graph_substitute_bound_var(
			terms,
			types,
			classifier_body,
			classifier_binder,
			left_endpoint,
			&left_identity_classifier
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			endpoints.endpoint_context_id,
			left_identity_type,
			left_identity_classifier,
			endpoint_acted_type_claim,
			left_endpoint_claim,
			&left_identity_claim
		) != 0 || left_identity_classifier >= terms->term_count || terms->terms[
			left_identity_classifier
		].tag != PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
			terms,
			terms->terms[
				left_identity_classifier
			].as.pi.codomain_family,
			&classifier_binder,
			&classifier_body
		) != 0 || prototype_term_graph_substitute_bound_var(
			terms,
			types,
			classifier_body,
			classifier_binder,
			right_endpoint,
			&classifier_body
		) != 0 || prototype_term_app(
			terms, left_identity_type, right_endpoint, &identity_type
		) != 0 || classifier_body != source->classifier ||
		prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			endpoints.endpoint_context_id,
			identity_type,
			classifier_body,
			left_identity_claim,
			right_endpoint_claim,
			&identity_type_has_type_claim
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			identity_type_has_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		fprintf(stderr,
			"dimension action identity endpoint application failed request=%u\n",
			request_id);
		return -1;
	}

	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule = computation_rule,
			.endpoint_context_id = endpoints.endpoint_context_id,
			.left_endpoint_binding_id = endpoints.left_binding_id,
			.right_endpoint_binding_id = endpoints.right_binding_id,
			.backing_type_former_term_id = acted_type,
			.backing_type_former_has_type_claim_id = acted_type_claim,
			.identity_type_term_id = identity_type,
			.identity_type_has_type_claim_id = identity_type_has_type_claim,
			.identity_type_is_type_claim_id = identity_type_is_type_claim,
			.left_context_certificate_id =
				endpoints.left_context_certificate_id,
			.right_context_certificate_id =
				endpoints.right_context_certificate_id,
			.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
			.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

static int hott_ensure_dimension_action_type_claim(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t source_type,
	uint32_t universe,
	uint32_t operator_id,
	uint32_t* p_acted_type,
	uint32_t* p_acted_type_claim
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_dimension_operator_db* dimension_operators = kernel ?
		kernel->dimension_operators : NULL;
	uint32_t source_is_type_claim;
	uint32_t acted_type;
	uint32_t acted_classifier;
	uint32_t classifier_has_type_claim;
	uint32_t classifier_is_type_claim;
	uint32_t root_source = source_type;
	uint32_t formation_context = context_id;
	uint32_t target_dimension;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!dimension_operators || !p_acted_type || !p_acted_type_claim) {
		return -1;
	}
	for (size_t depth = 0; depth <= terms->term_count; ++depth) {
		if (root_source >= terms->term_count) {
			return -1;
		}
		if (terms->terms[root_source].tag !=
			PROTOTYPE_TERM_DIMENSION_ACTION) {
			break;
		}
		root_source = terms->terms[root_source].as.dimension_action.source;
		if (depth == terms->term_count) {
			return -1;
		}
	}
	int stage = 1;
	int status = prototype_term_dimension_action(
		terms,
		dimension_operators,
		source_type,
		operator_id,
		&acted_type
	);
	if (status == 0) {
		uint32_t ancestor = context_id;
		while (ancestor != PROTOTYPE_INVALID_ID) {
			uint32_t existing = hott_has_type_claim_for_subject(
				judgement, ancestor, acted_type
			);
			if (existing != PROTOTYPE_INVALID_ID) {
				if (ancestor == context_id) {
					*p_acted_type = acted_type;
					*p_acted_type_claim = existing;
					return 0;
				}
				uint32_t projection;
				if (prototype_substitution_projection_path(
						substitutions,
						contexts,
						context_id,
						ancestor,
						&projection
					) == 0 && prototype_judgement_add_reindexed_claim(
						judgement,
						terms,
						types,
						contexts,
						substitutions,
						existing,
						projection,
						p_acted_type_claim
					) == 0) {
					*p_acted_type = acted_type;
					return 0;
				}
			}
			const struct prototype_context* entry = prototype_context_get(
				contexts, ancestor
			);
			ancestor = entry ? entry->parent : PROTOTYPE_INVALID_ID;
		}
	}
	int source_status = status == 0 ? hott_ensure_is_type_claim_in_context(
		judgement,
		terms,
		types,
		contexts,
		substitutions,
		context_id,
		root_source,
		&source_is_type_claim
	) : -1;
	const struct prototype_judgement_proposition* source_is_type =
		source_status == 0 ? prototype_judgement_claim_proposition(
			judgement, source_is_type_claim
		) : NULL;
	if (status == 0 && source_status == 0 && source_is_type &&
		source_is_type->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
		source_is_type->subject == root_source) {
		universe = source_is_type->classifier;
		stage = 3;
		status = prototype_dimension_action_term_dimension(
			terms,
			dimension_operators,
			acted_type,
			&target_dimension
		);
		if (status == 0) {
			stage = 4;
			status = prototype_dimension_action_type_classifier(
				terms,
				dimension_operators,
				root_source,
				acted_type,
				universe,
				target_dimension,
				&acted_classifier
			);
		}
	} else if (status == 0) {
		status = 1;
		for (uint32_t claim_id = 0; claim_id < judgement->claim_count &&
			status != 0; ++claim_id) {
			const struct prototype_judgement_proposition* candidate =
				prototype_judgement_claim_proposition(judgement, claim_id);
			if (!candidate || candidate->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
				candidate->subject != root_source) {
				continue;
			}
			uint32_t target = context_id;
			while (target != PROTOTYPE_INVALID_ID) {
				uint32_t candidate_classifier;
				if (prototype_dimension_action_context_type_classifier(
						terms,
						types,
						contexts,
						dimension_operators,
						candidate->context_id,
						target,
						root_source,
						acted_type,
						candidate->classifier,
						operator_id,
						&candidate_classifier
					) == 0) {
					source_is_type_claim = claim_id;
					source_is_type = candidate;
					universe = candidate->classifier;
					formation_context = target;
					acted_classifier = candidate_classifier;
					status = 0;
					break;
				}
				const struct prototype_context* entry = prototype_context_get(
					contexts, target
				);
				target = entry ? entry->parent : PROTOTYPE_INVALID_ID;
			}
		}
	}
	if (status == 0) {
		stage = 5;
		status = prototype_hott_ensure_dimension_classifier_formation(
			kernel,
			formation_context,
			acted_classifier,
			universe
		);
	}
	if (status == 0) {
		stage = 6;
		status = prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			formation_context,
			acted_classifier,
			universe,
			&classifier_has_type_claim
		);
	}
	if (status == 0) {
		stage = 7;
		status = prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			formation_context,
			acted_classifier,
			universe,
			classifier_has_type_claim,
			&classifier_is_type_claim
		);
	}
	if (status == 0) {
		stage = 8;
		status = prototype_judgement_add_dimension_action_type_formation(
			judgement,
			terms,
			types,
			contexts,
			dimension_operators,
			formation_context,
			acted_type,
			acted_classifier,
			source_is_type_claim,
			p_acted_type_claim
		);
	}
	if (status == 0 && formation_context != context_id) {
		uint32_t projection;
		uint32_t reindexed_claim;
		stage = 9;
		status = prototype_substitution_projection_path(
			substitutions,
			contexts,
			context_id,
			formation_context,
			&projection
		);
		if (status == 0) {
			status = prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				*p_acted_type_claim,
				projection,
				&reindexed_claim
			);
		}
		if (status == 0) {
			*p_acted_type_claim = reindexed_claim;
		}
	}
	if (status != 0) {
		fprintf(
			stderr,
			"dimension action type premise failed source=%u universe=%u stage=%d "
			"status=%d\n",
			source_type,
			universe,
			stage,
			status
		);
		return -1;
	}
	*p_acted_type = acted_type;
	return 0;
}

static int hott_apply_claim_argument(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t function_claim_id,
	uint32_t argument_claim_id,
	uint32_t* p_term_id,
	uint32_t* p_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* function = judgement ?
		prototype_judgement_claim_proposition(judgement, function_claim_id) : NULL;
	const struct prototype_judgement_proposition* argument = judgement ?
		prototype_judgement_claim_proposition(judgement, argument_claim_id) : NULL;
	uint32_t binder;
	uint32_t body;
	uint32_t classifier;
	uint32_t term;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!function || !argument || !p_term_id || !p_claim_id ||
		function->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		argument->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		function->context_id != context_id || argument->context_id != context_id ||
		function->classifier >= terms->term_count || terms->terms[
			function->classifier
		].tag != PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
			terms,
			terms->terms[function->classifier].as.pi.codomain_family,
			&binder,
			&body
		) != 0 || prototype_term_graph_substitute_bound_var(
			terms,
			types,
			body,
			binder,
			argument->subject,
			&classifier
		) != 0 || prototype_term_app(
			terms, function->subject, argument->subject, &term
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			context_id,
			term,
			classifier,
			function_claim_id,
			argument_claim_id,
			p_claim_id
		) != 0) {
		return -1;
	}
	*p_term_id = term;
	return 0;
}

static uint32_t hott_exact_has_type_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t fallback = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (proposition && proposition->kind ==
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE && proposition->context_id ==
				context_id && proposition->subject == subject &&
			proposition->classifier == classifier) {
			size_t derivation_count = 0;
			if (prototype_judgement_claim_derivations(
					judgement, i, NULL, 0, &derivation_count
				) >= 0 && derivation_count != 0) {
				return i;
			}
			if (fallback == PROTOTYPE_INVALID_ID) {
				fallback = i;
			}
		}
	}
	return fallback;
}

static uint32_t hott_exact_is_type_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject,
	uint32_t classifier
) {
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	uint32_t fallback = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* proposition =
			prototype_judgement_claim_proposition(judgement, i);
		if (proposition && proposition->kind ==
				PROTOTYPE_JUDGEMENT_KIND_IS_TYPE && proposition->context_id ==
				context_id && proposition->subject == subject &&
			proposition->classifier == classifier) {
			size_t derivation_count = 0;
			if (prototype_judgement_claim_derivations(
					judgement, i, NULL, 0, &derivation_count
				) >= 0 && derivation_count != 0) {
				return i;
			}
			if (fallback == PROTOTYPE_INVALID_ID) {
				fallback = i;
			}
		}
	}
	return fallback;
}

static int hott_ensure_action_endpoint_claim_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t subject,
	uint32_t expected_classifier,
	size_t depth,
	uint32_t* p_claim_id
);

static int hott_infer_action_endpoint_claim_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t subject,
	size_t depth,
	uint32_t* p_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	if (!terms || !judgement || !p_claim_id || subject >= terms->term_count ||
		depth > terms->term_count) {
		return -1;
	}
	uint32_t existing = hott_has_type_claim_for_subject(
		judgement, context_id, subject
	);
	if (existing != PROTOTYPE_INVALID_ID) {
		*p_claim_id = existing;
		return 0;
	}
	uint32_t ancestor = context_id;
	while (ancestor != PROTOTYPE_INVALID_ID) {
		uint32_t ancestor_claim = hott_has_type_claim_for_subject(
			judgement, ancestor, subject
		);
		if (ancestor_claim != PROTOTYPE_INVALID_ID) {
			uint32_t projection;
			if (prototype_substitution_projection_path(
					kernel->substitutions,
					kernel->contexts,
					context_id,
					ancestor,
					&projection
				) != 0 || prototype_judgement_add_reindexed_claim(
					judgement,
					terms,
					kernel->type_declarations,
					kernel->contexts,
					kernel->substitutions,
					ancestor_claim,
					projection,
					p_claim_id
				) != 0) {
				return -1;
			}
			return 0;
		}
		const struct prototype_context* context = prototype_context_get(
			kernel->contexts, ancestor
		);
		ancestor = context ? context->parent : PROTOTYPE_INVALID_ID;
	}
	const struct prototype_term* term = &terms->terms[subject];
	if (term->tag == PROTOTYPE_TERM_VAR) {
		uint32_t binding_context_id;
		const struct prototype_context* binding_context;
		if (prototype_context_find_binding(
				kernel->contexts,
				context_id,
				term->as.var.binding_id,
				&binding_context_id
			) != 0 || !(binding_context = prototype_context_get(
				kernel->contexts, binding_context_id
			)) || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				kernel->contexts,
				context_id,
				term->as.var.binding_id,
				prototype_context_classifier_term(binding_context),
				p_claim_id
			) != 0) {
			return -1;
		}
		return 0;
	}
	if (term->tag == PROTOTYPE_TERM_FORCE) {
		uint32_t value_claim;
		if (hott_infer_action_endpoint_claim_depth(
				kernel,
				context_id,
				term->as.force.value,
				depth + 1,
				&value_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* value =
			prototype_judgement_claim_proposition(judgement, value_claim);
		if (!value || value->classifier >= terms->term_count || terms->terms[
				value->classifier
			].tag != PROTOTYPE_TERM_THUNK_TYPE) {
			return -1;
		}
		return prototype_judgement_add_force_claim(
			judgement,
			terms,
			context_id,
			subject,
			terms->terms[value->classifier].as.thunk_type.computation,
			value_claim,
			p_claim_id
		);
	}
	if (term->tag == PROTOTYPE_TERM_APP) {
		uint32_t function_claim;
		if (hott_infer_action_endpoint_claim_depth(
				kernel,
				context_id,
				term->as.app.function,
				depth + 1,
				&function_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* function =
			prototype_judgement_claim_proposition(judgement, function_claim);
		uint32_t binder;
		uint32_t codomain;
		if (!function || function->classifier >= terms->term_count || terms->terms[
				function->classifier
			].tag != PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
				terms,
				terms->terms[function->classifier].as.pi.codomain_family,
				&binder,
				&codomain
			) != 0) {
			return -1;
		}
		uint32_t argument_claim;
		uint32_t classifier;
		if (hott_ensure_action_endpoint_claim_depth(
				kernel,
				context_id,
				term->as.app.argument,
				terms->terms[function->classifier].as.pi.domain,
				depth + 1,
				&argument_claim
			) != 0 || prototype_term_graph_substitute_bound_var(
				terms,
				kernel->type_declarations,
				codomain,
				binder,
				term->as.app.argument,
				&classifier
			) != 0) {
			return -1;
		}
		return prototype_judgement_add_app_claim(
			judgement,
			terms,
			kernel->type_declarations,
			kernel->contexts,
			kernel->substitutions,
			context_id,
			subject,
			classifier,
			function_claim,
			argument_claim,
			p_claim_id
		);
	}
	if (term->tag == PROTOTYPE_TERM_RETURN) {
		uint32_t value_claim;
		if (hott_infer_action_endpoint_claim_depth(
				kernel,
				context_id,
				term->as.return_term.value,
				depth + 1,
				&value_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* value =
			prototype_judgement_claim_proposition(judgement, value_claim);
		uint32_t empty_effects;
		uint32_t classifier;
		if (!value || prototype_term_effect_row_empty(
				terms, &empty_effects
			) != 0 || prototype_term_total_computation_type(
				terms, empty_effects, value->classifier, &classifier
			) != 0) {
			return -1;
		}
		return prototype_judgement_add_return_claim(
			judgement,
			terms,
			kernel->type_declarations,
			context_id,
			subject,
			classifier,
			value_claim,
			p_claim_id
		);
	}
	if (term->tag == PROTOTYPE_TERM_THUNK) {
		uint32_t computation_claim;
		if (hott_infer_action_endpoint_claim_depth(
				kernel,
				context_id,
				term->as.thunk.computation,
				depth + 1,
				&computation_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* computation =
			prototype_judgement_claim_proposition(judgement, computation_claim);
		uint32_t classifier;
		if (!computation || prototype_term_thunk_type(
				terms, computation->classifier, &classifier
			) != 0) {
			return -1;
		}
		return prototype_judgement_add_thunk_claim(
			judgement,
			terms,
			kernel->type_declarations,
			context_id,
			subject,
			classifier,
			computation_claim,
			p_claim_id
		);
	}
	return -1;
}

static int hott_ensure_action_endpoint_claim_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t subject,
	uint32_t expected_classifier,
	size_t depth,
	uint32_t* p_claim_id
) {
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	uint32_t exact = hott_exact_has_type_claim(
		judgement, context_id, subject, expected_classifier
	);
	if (exact != PROTOTYPE_INVALID_ID) {
		*p_claim_id = exact;
		return 0;
	}
	uint32_t inferred_claim;
	if (hott_infer_action_endpoint_claim_depth(
			kernel, context_id, subject, depth, &inferred_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* inferred =
		prototype_judgement_claim_proposition(judgement, inferred_claim);
	if (!inferred) {
		return -1;
	}
	if (inferred->classifier == expected_classifier) {
		*p_claim_id = inferred_claim;
		return 0;
	}
	return prototype_judgement_add_conversion_claim(
		judgement,
		kernel->terms,
		kernel->type_declarations,
		context_id,
		subject,
		expected_classifier,
		inferred_claim,
		p_claim_id
	);
}

static int hott_ensure_dimension_instance_is_type(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t type_term,
	uint32_t universe,
	uint32_t* p_is_type_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_dimension_operator_db* operators = kernel ?
		kernel->dimension_operators : NULL;
	uint32_t family;
	uint32_t source;
	uint32_t operator_id;
	uint32_t dimension;
	size_t argument_count;
	if (!terms || !judgement || !operators || !p_is_type_claim_id ||
		prototype_dimension_action_family_instance_info(
			terms,
			operators,
			type_term,
			&family,
			&source,
			&operator_id,
			&dimension,
			NULL,
			0,
			&argument_count
		) != 0 || argument_count > SIZE_MAX / sizeof(uint32_t)) {
		return -1;
	}
	uint32_t* arguments = argument_count == 0 ? NULL : calloc(
		argument_count, sizeof(*arguments)
	);
	if (argument_count != 0 && !arguments) {
		return -1;
	}
	size_t parsed_count;
	int status = prototype_dimension_action_family_instance_info(
		terms,
		operators,
		type_term,
		&family,
		&source,
		&operator_id,
		&dimension,
		arguments,
		argument_count,
		&parsed_count
	);
	uint32_t family_claim = PROTOTYPE_INVALID_ID;
	uint32_t rebuilt_family = PROTOTYPE_INVALID_ID;
	if (status == 0 && (parsed_count != argument_count ||
		hott_ensure_dimension_action_type_claim(
			kernel,
			context_id,
			source,
			universe,
			operator_id,
			&rebuilt_family,
			&family_claim
			) != 0 || rebuilt_family != family)) {
		fprintf(
			stderr,
			"dimension instance family rebuild failed term=%u family=%u rebuilt=%u "
			"arguments=%zu parsed=%zu status=%d\n",
			type_term,
			family,
			rebuilt_family,
			argument_count,
			parsed_count,
			status
		);
		status = -1;
	}
	uint32_t current_term = family;
	uint32_t current_claim = family_claim;
	for (size_t i = 0; i < argument_count && status == 0; ++i) {
		const struct prototype_judgement_proposition* current =
			prototype_judgement_claim_proposition(judgement, current_claim);
		uint32_t argument_claim;
		uint32_t expected_argument_classifier = current && current->classifier <
			terms->term_count && terms->terms[current->classifier].tag ==
				PROTOTYPE_TERM_PI ? terms->terms[
					current->classifier
				].as.pi.domain : PROTOTYPE_INVALID_ID;
		int argument_status = expected_argument_classifier == PROTOTYPE_INVALID_ID ?
			-1 : hott_ensure_action_endpoint_claim_depth(
				kernel,
				context_id,
				arguments[i],
				expected_argument_classifier,
				0,
				&argument_claim
			);
		if (!current || current->classifier >= terms->term_count || terms->terms[
				current->classifier
			].tag != PROTOTYPE_TERM_PI || argument_status != 0 ||
			hott_apply_claim_argument(
				kernel,
				context_id,
				current_claim,
				argument_claim,
				&current_term,
				&current_claim
			) != 0) {
			fprintf(
				stderr,
				"dimension instance argument failed term=%u index=%zu argument=%u tag=%d "
				"expected=%u argument-status=%d current-claim=%u\n",
				type_term,
				i,
				arguments[i],
				arguments[i] < terms->term_count ? terms->terms[arguments[i]].tag : -1,
				expected_argument_classifier,
				argument_status,
				current_claim
			);
			status = -1;
		}
	}
	if (status == 0 && (current_term != type_term ||
		prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			context_id,
			type_term,
			universe,
			current_claim,
			p_is_type_claim_id
		) != 0)) {
		fprintf(
			stderr,
			"dimension instance final formation failed term=%u rebuilt=%u claim=%u\n",
			type_term,
			current_term,
			current_claim
		);
		status = -1;
	}
	if (status == 0) {
		const struct prototype_judgement_proposition* formation =
			prototype_judgement_claim_proposition(judgement, current_claim);
		if (!formation || formation->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			formation->context_id != context_id || formation->subject != type_term ||
			formation->classifier != universe) {
			fprintf(
				stderr,
				"dimension instance used invalid formation claim term=%u claim=%u "
				"kind=%d context=%u/%u subject=%u classifier=%u/%u\n",
				type_term,
				current_claim,
				formation ? formation->kind : -1,
				formation ? formation->context_id : PROTOTYPE_INVALID_ID,
				context_id,
				formation ? formation->subject : PROTOTYPE_INVALID_ID,
				formation ? formation->classifier : PROTOTYPE_INVALID_ID,
				universe
			);
			status = -1;
		}
	}
	free(arguments);
	return status;
}

static int hott_add_exact_pi_formation(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t classifier,
	uint32_t universe,
	uint32_t binder_context,
	uint32_t binder,
	uint32_t domain,
	uint32_t codomain,
	uint32_t* p_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	uint32_t domain_claim = hott_exact_has_type_claim(
		judgement, context_id, domain, universe
	);
	uint32_t codomain_claim = hott_exact_has_type_claim(
		judgement, binder_context, codomain, universe
	);
	uint32_t binder_claim;
	uint32_t returned_classifier;
	uint32_t empty_effects;
	uint32_t family_classifier_family;
	uint32_t lambda_classifier;
	uint32_t family_classifier;
	uint32_t family = terms && classifier < terms->term_count ?
		terms->terms[classifier].as.pi.codomain_family : PROTOTYPE_INVALID_ID;
	uint32_t lambda = family < terms->term_count &&
		terms->terms[family].tag == PROTOTYPE_TERM_THUNK ?
		terms->terms[family].as.thunk.computation : PROTOTYPE_INVALID_ID;
	uint32_t returned = lambda < terms->term_count &&
		terms->terms[lambda].tag == PROTOTYPE_TERM_LAMBDA ?
		terms->terms[lambda].as.lambda.body : PROTOTYPE_INVALID_ID;
	uint32_t return_claim;
	uint32_t lambda_claim;
	uint32_t family_claim;
	int exact_stage = 0;
	if (!terms || !judgement || !p_claim_id ||
		domain_claim == PROTOTYPE_INVALID_ID ||
		codomain_claim == PROTOTYPE_INVALID_ID ||
		family >= terms->term_count || lambda >= terms->term_count ||
		returned >= terms->term_count ||
		terms->terms[lambda].as.lambda.binding_id != binder ||
		terms->terms[returned].tag != PROTOTYPE_TERM_RETURN ||
		terms->terms[returned].as.return_term.value != codomain ||
		(exact_stage = 1, prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			kernel->contexts,
			binder_context,
			binder,
			domain,
			&binder_claim
		)) != 0 || (exact_stage = 2, prototype_term_effect_row_empty(
			terms, &empty_effects
		)) != 0 || prototype_term_total_computation_type(
			terms, empty_effects, universe, &returned_classifier
		) != 0 || prototype_term_pure_family(
			terms, binder, returned_classifier, &family_classifier_family
		) != 0 || prototype_term_pi_family(
			terms, domain, family_classifier_family, &lambda_classifier
		) != 0 || prototype_term_thunk_type(
			terms, lambda_classifier, &family_classifier
		) != 0 || (exact_stage = 3, prototype_judgement_add_return_claim(
			judgement,
			terms,
			kernel->type_declarations,
			binder_context,
			returned,
			returned_classifier,
			codomain_claim,
			&return_claim
		)) != 0 || (exact_stage = 4, prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			kernel->type_declarations,
			kernel->contexts,
			kernel->substitutions,
			context_id,
			lambda,
			lambda_classifier,
			binder_claim,
			return_claim,
			&lambda_claim
		)) != 0 || (exact_stage = 5, prototype_judgement_add_thunk_claim(
			judgement,
			terms,
			kernel->type_declarations,
			context_id,
			family,
			family_classifier,
			lambda_claim,
			&family_claim
		)) != 0 || (exact_stage = 6, prototype_judgement_add_pi_formation_claim(
			judgement,
			terms,
			context_id,
			classifier,
			universe,
			domain_claim,
			codomain_claim,
			family_claim,
			p_claim_id
		)) != 0) {
		fprintf(
			stderr,
			"exact Pi formation failed stage=%d classifier=%u context=%u binder-context=%u "
			"domain=%u domain-claim=%u codomain=%u codomain-claim=%u\n",
			exact_stage, classifier, context_id, binder_context, domain,
			domain_claim, codomain, codomain_claim
		);
		return -1;
	}
	return 0;
}

static int hott_seed_dimension_classifier_domains_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t classifier,
	uint32_t universe,
	size_t remaining_depth,
	int exact_pi_formation
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	if (!terms || !contexts || !cwf || remaining_depth == 0 ||
		classifier >= terms->term_count) {
		return -1;
	}
	if (classifier == universe) {
		uint32_t formation_claim;
		return hott_exact_has_type_claim(
			kernel->judgement, context_id, classifier, universe
		) != PROTOTYPE_INVALID_ID ? 0 :
			prototype_judgement_add_structural_type_formation_claim(
				kernel->judgement,
				terms,
				kernel->type_declarations,
				contexts,
				kernel->substitutions,
				context_id,
				classifier,
				universe,
				&formation_claim
			);
	}
	if (terms->terms[classifier].tag == PROTOTYPE_TERM_THUNK_TYPE ||
		terms->terms[classifier].tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
		uint32_t child = terms->terms[classifier].tag == PROTOTYPE_TERM_THUNK_TYPE ?
			terms->terms[classifier].as.thunk_type.computation :
			terms->terms[classifier].as.computation_type.result;
		uint32_t formation_claim;
		if (hott_seed_dimension_classifier_domains_depth(
				kernel,
				context_id,
				child,
				universe,
				remaining_depth - 1,
				exact_pi_formation
			) != 0 || (hott_exact_has_type_claim(
				kernel->judgement, context_id, classifier, universe
			) == PROTOTYPE_INVALID_ID &&
			prototype_judgement_add_structural_type_formation_claim(
				kernel->judgement,
				terms,
				kernel->type_declarations,
				contexts,
				kernel->substitutions,
				context_id,
				classifier,
				universe,
				&formation_claim
			) != 0)) {
			fprintf(
				stderr,
				"dimension classifier wrapper formation failed term=%u tag=%d context=%u "
				"child=%u\n",
				classifier,
				terms->terms[classifier].tag,
				context_id,
				child
			);
			return -1;
		}
		return 0;
	}
	if (terms->terms[classifier].tag == PROTOTYPE_TERM_PI) {
		uint32_t binder;
		uint32_t body;
		uint32_t domain = terms->terms[classifier].as.pi.domain;
		uint32_t domain_is_type_claim = hott_exact_is_type_claim(
			kernel->judgement, context_id, domain, universe
		);
		uint32_t domain_formation_claim = hott_exact_has_type_claim(
			kernel->judgement, context_id, domain, universe
		);
		if (prototype_term_pure_family_parts(
				terms,
				terms->terms[classifier].as.pi.codomain_family,
				&binder,
				&body
			) != 0) {
			return -1;
		}
		if (domain_is_type_claim == PROTOTYPE_INVALID_ID) {
			(void)hott_ensure_dimension_instance_is_type(
				kernel,
				context_id,
				domain,
				universe,
				&domain_is_type_claim
			);
			domain_formation_claim = hott_exact_has_type_claim(
				kernel->judgement, context_id, domain, universe
			);
		}
		if (domain_formation_claim == PROTOTYPE_INVALID_ID &&
			prototype_judgement_add_structural_type_formation_claim(
				kernel->judgement,
				terms,
				kernel->type_declarations,
				contexts,
				kernel->substitutions,
				context_id,
				domain,
				universe,
				&domain_formation_claim
			) != 0) {
			fprintf(
				stderr,
				"dimension classifier domain formation failed pi=%u context=%u "
				"domain=%u tag=%d\n",
				classifier,
				context_id,
				domain,
				domain < terms->term_count ? terms->terms[domain].tag : -1
			);
			return -1;
		}
		if (domain_is_type_claim == PROTOTYPE_INVALID_ID &&
			prototype_judgement_add_is_type_claim(
				kernel->judgement,
				terms,
				context_id,
				domain,
				universe,
				domain_formation_claim,
				&domain_is_type_claim
			) != 0) {
			return -1;
		}
		uint32_t next_context;
		uint32_t ignored_context_certificate;
		if (domain_is_type_claim == PROTOTYPE_INVALID_ID || prototype_context_extend(
				contexts,
				context_id,
				binder,
				domain,
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_cwf_certificate_db_add_context(
				cwf,
				contexts,
				terms,
				kernel->type_declarations,
				kernel->judgement,
				next_context,
				domain_is_type_claim,
				&ignored_context_certificate
			) != 0) {
			fprintf(
				stderr,
				"dimension classifier context extension failed pi=%u context=%u "
				"domain=%u domain-claim=%u binder=%u\n",
				classifier,
				context_id,
				domain,
				domain_is_type_claim,
				binder
			);
			return -1;
		}
		uint32_t classifier_formation_claim;
		if (hott_seed_dimension_classifier_domains_depth(
				kernel,
				next_context,
				body,
				universe,
				remaining_depth - 1,
				exact_pi_formation
			) != 0 || (hott_exact_has_type_claim(
				kernel->judgement, context_id, classifier, universe
			) == PROTOTYPE_INVALID_ID && (exact_pi_formation ?
			hott_add_exact_pi_formation(
				kernel,
				context_id,
				classifier,
				universe,
				next_context,
				binder,
				domain,
				body,
				&classifier_formation_claim
			) : prototype_judgement_add_structural_type_formation_claim(
				kernel->judgement,
				terms,
				kernel->type_declarations,
				contexts,
				kernel->substitutions,
				context_id,
				classifier,
				universe,
				&classifier_formation_claim
			)) != 0)) {
			fprintf(
				stderr,
				"dimension classifier Pi formation failed term=%u context=%u body=%u "
				"body-context=%u\n",
				classifier,
				context_id,
				body,
				next_context
			);
			return -1;
		}
		return 0;
	}
	uint32_t terminal_is_type_claim;
	int dimension_instance_status = hott_ensure_dimension_instance_is_type(
		kernel,
		context_id,
		classifier,
		universe,
		&terminal_is_type_claim
	);
	int terminal_status = hott_exact_has_type_claim(
		kernel->judgement, context_id, classifier, universe
	) != PROTOTYPE_INVALID_ID ? 0 : -1;
	if (terminal_status != 0) {
		fprintf(
			stderr,
			"dimension classifier terminal formation failed term=%u tag=%d context=%u "
			"instance-status=%d\n",
			classifier,
			terms->terms[classifier].tag,
			context_id,
			dimension_instance_status
		);
	}
	return terminal_status;
}

int prototype_hott_ensure_dimension_classifier_formation(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t classifier,
	uint32_t universe
) {
	return hott_seed_dimension_classifier_domains_depth(
		kernel,
		context_id,
		classifier,
		universe,
		kernel && kernel->terms ? kernel->terms->term_count + 1 : 0,
		0
	);
}

static int hott_ensure_dimension_classifier_formation_exact(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t classifier,
	uint32_t universe
) {
	return hott_seed_dimension_classifier_domains_depth(
		kernel,
		context_id,
		classifier,
		universe,
		kernel && kernel->terms ? kernel->terms->term_count + 1 : 0,
		1
	);
}

static int hott_term_is_constant_over_context(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	uint32_t context_id,
	uint32_t term_id
) {
	if (!terms || !contexts || term_id >= terms->term_count) {
		return 0;
	}
	uint32_t current = context_id;
	for (uint32_t depth = 0; depth <= contexts->context_count; ++depth) {
		const struct prototype_context* context = prototype_context_get(
			contexts, current
		);
		if (!context) {
			return 0;
		}
		if (current == prototype_context_empty(contexts)) {
			return 1;
		}
		if (prototype_term_contains_free_binding(
				terms, term_id, context->binding_id
			)) {
			return 0;
		}
		current = context->parent;
	}
	return 0;
}

static int hott_prepare_identity_endpoints(
	struct prototype_kernel_builder* kernel,
	uint32_t source_type_claim_id,
	const struct prototype_hott_bridge* bridge,
	struct hott_identity_endpoint_setup* p_setup
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(
			judgement, source_type_claim_id
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!source || !bridge || !p_setup || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source->context_id != bridge->source_context_id) {
		return -1;
	}
	memset(p_setup, 0, sizeof(*p_setup));
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_type_claim_id,
			bridge->left_substitution_id,
			&p_setup->left_type_claim_id
		) != 0) {
		return -1;
	}
	uint32_t right_type_base_claim;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_type_claim_id,
			bridge->right_substitution_id,
			&right_type_base_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_type =
		prototype_judgement_claim_proposition(
			judgement, p_setup->left_type_claim_id
		);
	const struct prototype_judgement_proposition* right_type =
		prototype_judgement_claim_proposition(judgement, right_type_base_claim);
	if (!left_type || !right_type) {
		return -1;
	}
	p_setup->left_type_term_id = left_type->subject;
	p_setup->right_type_term_id = right_type->subject;
	p_setup->left_binding_id = prototype_term_new_binding(terms);
	p_setup->right_binding_id = prototype_term_new_binding(terms);
	uint32_t left_projection;
	if (p_setup->left_binding_id == PROTOTYPE_INVALID_ID ||
		p_setup->right_binding_id == PROTOTYPE_INVALID_ID ||
		prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			p_setup->left_binding_id,
			p_setup->left_type_term_id,
			PROTOTYPE_INVALID_ID,
			&p_setup->left_context_id
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			p_setup->left_context_id,
			p_setup->left_type_claim_id,
			&p_setup->left_context_certificate_id
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			p_setup->left_context_id,
			bridge->bridge_context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_type_base_claim,
			left_projection,
			&p_setup->right_type_claim_id
		) != 0 || prototype_context_extend(
			contexts,
			p_setup->left_context_id,
			p_setup->right_binding_id,
			p_setup->right_type_term_id,
			PROTOTYPE_INVALID_ID,
			&p_setup->endpoint_context_id
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			p_setup->endpoint_context_id,
			p_setup->right_type_claim_id,
			&p_setup->right_context_certificate_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_add_variable_assumption_in_context(
	struct prototype_kernel_builder* kernel,
	uint32_t context_id,
	uint32_t binding_id,
	uint32_t* p_term_id,
	uint32_t* p_claim_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	uint32_t binding_context_id;
	if (!contexts || !terms || !judgement || !p_term_id || !p_claim_id ||
		prototype_context_find_binding(
			contexts, context_id, binding_id, &binding_context_id
		) != 0) {
		return -1;
	}
	const struct prototype_context* binding_context = prototype_context_get(
		contexts, binding_context_id
	);
	if (!binding_context || prototype_term_var(
			terms, binding_id, p_term_id
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			context_id,
			binding_id,
			prototype_context_classifier_term(binding_context),
			p_claim_id
		) != 0) {
		return -1;
	}
	return 0;
}

static int hott_construct_term_action_faces(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	const struct prototype_hott_bridge* bridge,
	uint32_t source_claim_id,
	uint32_t target_context_id,
	uint32_t face_terms[3],
	uint32_t face_claims[3]
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	uint32_t source_type_claim_id;
	uint32_t target_to_bridge;
	uint32_t bridge_face_claims[3];
	if (!actions || !terms || !types || !contexts || !substitutions ||
		!judgement || !bridges || !bridge || !source || !face_terms ||
		!face_claims || source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->context_id != bridge->source_context_id ||
		hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			source->classifier,
			&source_type_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			bridge->left_substitution_id,
			&bridge_face_claims[0]
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			bridge->right_substitution_id,
			&bridge_face_claims[1]
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			target_context_id,
			bridge->bridge_context_id,
			&target_to_bridge
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_request identity_request = {
		.kind = PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION,
		.key.identity_type = {
			.source_claim_id = source_type_claim_id,
			.source_bridge_id = bridge->id
		}
	};
	struct prototype_kernel_view view;
	uint32_t identity_request_id;
	uint32_t identity_result_id;
	uint32_t identity_family_claim_id;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_request_intern(
			actions,
			&view,
			bridges,
			identity_request,
			&identity_request_id
		) != 0 || prototype_hott_execute_identity_type_computation(
			actions,
			kernel,
			bridges,
			identity_request_id,
			&identity_result_id
		) != 0 || prototype_hott_construct_object_term_action(
			actions,
			kernel,
			bridges,
			identity_result_id,
			source_claim_id,
			&identity_family_claim_id,
			&face_terms[2],
			&bridge_face_claims[2]
		) != 0) {
		return -1;
	}
	(void)identity_family_claim_id;
	for (uint32_t i = 0; i < 3; ++i) {
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				bridge_face_claims[i],
				target_to_bridge,
				&face_claims[i]
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* face =
			prototype_judgement_claim_proposition(
				judgement, face_claims[i]
			);
		if (!face || face->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
			return -1;
		}
		face_terms[i] = face->subject;
	}
	return 0;
}

static int hott_publish_repeated_dimension_action_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_dimension_operator_db* dimension_operators = kernel ?
		kernel->dimension_operators : NULL;
	uint32_t source_family;
	uint32_t family_source;
	uint32_t source_operator_id;
	uint32_t source_dimension;
	size_t source_boundary_count;
	if (!actions || !terms || !contexts || !judgement || !dimension_operators ||
		!bridges || !source || !bridge || !p_result_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source->context_id !=
			bridge->source_context_id ||
		prototype_dimension_action_family_instance_info(
			terms,
			dimension_operators,
			source->subject,
			&source_family,
			&family_source,
			&source_operator_id,
			&source_dimension,
			NULL,
			0,
			&source_boundary_count
		) != 0 || source_dimension == 0 || source_boundary_count >
			SIZE_MAX / sizeof(uint32_t)) {
		return -1;
	}
	size_t target_boundary_count;
	if (source_dimension == UINT32_MAX || prototype_dimension_boundary_count(
			source_dimension + 1, &target_boundary_count
		) != 0 || source_boundary_count > (SIZE_MAX - 2) / 3 ||
		target_boundary_count != source_boundary_count * 3 + 2) {
		return -1;
	}
	uint32_t* source_boundaries = source_boundary_count == 0 ? NULL : calloc(
		source_boundary_count, sizeof(*source_boundaries)
	);
	uint32_t* target_boundaries = target_boundary_count == 0 ? NULL : calloc(
		target_boundary_count, sizeof(*target_boundaries)
	);
	uint32_t* target_claims = target_boundary_count == 0 ? NULL : calloc(
		target_boundary_count, sizeof(*target_claims)
	);
	if ((source_boundary_count != 0 && !source_boundaries) ||
		(target_boundary_count != 0 && (!target_boundaries || !target_claims))) {
		free(source_boundaries);
		free(target_boundaries);
		free(target_claims);
		return -1;
	}
	size_t parsed_boundary_count;
	int stage = 1;
	int status = prototype_dimension_action_family_instance_info(
		terms,
		dimension_operators,
		source->subject,
		&source_family,
		&family_source,
		&source_operator_id,
		&source_dimension,
		source_boundaries,
		source_boundary_count,
		&parsed_boundary_count
	);
	if (status != 0 || parsed_boundary_count != source_boundary_count) {
		status = -1;
	}

	struct hott_identity_endpoint_setup endpoints;
	if (status == 0 && hott_prepare_identity_endpoints(
			kernel,
			prototype_hott_action_request_get(actions, request_id)->key.
				identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		status = -1;
	}
	stage = 2;
	for (size_t i = 0; i < source_boundary_count && status == 0; ++i) {
		if (source_boundaries[i] >= terms->term_count) {
			status = -1;
			break;
		}
		if (terms->terms[source_boundaries[i]].tag == PROTOTYPE_TERM_VAR) {
			uint32_t source_binding = terms->terms[
				source_boundaries[i]
			].as.var.binding_id;
			for (uint32_t face = 0; face < 3; ++face) {
				const struct prototype_hott_bridge_face_binding* action =
					prototype_hott_bridge_binding_action_get(
						bridges, bridge->id, source_binding, face
					);
				size_t target_index = i * 3 + face;
				if (!action || hott_add_variable_assumption_in_context(
						kernel,
						endpoints.endpoint_context_id,
						action->target_binding_id,
						&target_boundaries[target_index],
						&target_claims[target_index]
					) != 0) {
					status = -1;
					break;
				}
			}
		} else {
			uint32_t source_boundary_claim = hott_has_type_claim_for_subject(
				judgement, source->context_id, source_boundaries[i]
			);
			if (source_boundary_claim == PROTOTYPE_INVALID_ID ||
				hott_construct_term_action_faces(
					actions,
					kernel,
					bridges,
					bridge,
					source_boundary_claim,
					endpoints.endpoint_context_id,
					&target_boundaries[i * 3],
					&target_claims[i * 3]
				) != 0) {
				status = 1;
			}
		}
	}
	if (status == 0) {
		stage = 3;
		status = hott_add_variable_assumption_in_context(
			kernel,
			endpoints.endpoint_context_id,
			endpoints.left_binding_id,
			&target_boundaries[target_boundary_count - 2],
			&target_claims[target_boundary_count - 2]
		);
	}
	if (status == 0) {
		stage = 4;
		status = hott_add_variable_assumption_in_context(
			kernel,
			endpoints.endpoint_context_id,
			endpoints.right_binding_id,
			&target_boundaries[target_boundary_count - 1],
			&target_claims[target_boundary_count - 1]
		);
	}

	uint32_t extension_operator_id;
	uint32_t higher_family;
	uint32_t higher_family_claim;
	if (status == 0 && (prototype_dimension_operator_extension(
			dimension_operators,
			source_dimension,
			&extension_operator_id
		) != 0 || hott_ensure_dimension_action_type_claim(
			kernel,
			endpoints.endpoint_context_id,
			source_family,
			source->classifier,
			extension_operator_id,
			&higher_family,
			&higher_family_claim
		) != 0)) {
		status = -1;
	}
	if (status != 0) {
		fprintf(
			stderr,
			"repeated dimension family formation failed request=%u status=%d "
			"source-family=%u dimension=%u\n",
			request_id,
			status,
			source_family,
			source_dimension
		);
	}
	stage = 5;
	uint32_t higher_type = higher_family;
	uint32_t higher_type_claim = higher_family_claim;
	size_t failed_boundary = SIZE_MAX;
	for (size_t i = 0; i < target_boundary_count && status == 0; ++i) {
		stage = 6;
		status = hott_apply_claim_argument(
			kernel,
			endpoints.endpoint_context_id,
			higher_type_claim,
			target_claims[i],
			&higher_type,
			&higher_type_claim
		);
		if (status != 0) {
			failed_boundary = i;
		}
	}
	uint32_t higher_type_is_type_claim;
	const struct prototype_judgement_proposition* higher_type_proposition =
		status == 0 ? prototype_judgement_claim_proposition(
			judgement, higher_type_claim
		) : NULL;
	if (status == 0 && (!higher_type_proposition ||
		higher_type_proposition->classifier != source->classifier ||
		prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			higher_type,
			source->classifier,
			higher_type_claim,
			&higher_type_is_type_claim
		) != 0)) {
		status = -1;
	}
	if (status != 0 && failed_boundary == SIZE_MAX) {
		fprintf(
			stderr,
			"repeated dimension final type failed term=%u claim=%u proposition=%p "
			"classifier=%u expected=%u\n",
			higher_type,
			higher_type_claim,
			(void*)higher_type_proposition,
			higher_type_proposition ? higher_type_proposition->classifier :
				PROTOTYPE_INVALID_ID,
			source->classifier
		);
	}
	stage = 7;

	uint32_t result = PROTOTYPE_INVALID_ID;
	if (status == 0) {
		struct prototype_hott_action_certificate certificate = {
			.request_id = request_id,
			.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
			.data.identity_type = {
				.computation_rule =
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_GENERIC_DIMENSION_ACTION,
				.endpoint_context_id = endpoints.endpoint_context_id,
				.left_endpoint_binding_id = endpoints.left_binding_id,
				.right_endpoint_binding_id = endpoints.right_binding_id,
				.backing_type_former_term_id = higher_family,
				.backing_type_former_has_type_claim_id = higher_family_claim,
				.identity_type_term_id = higher_type,
				.identity_type_has_type_claim_id = higher_type_claim,
				.identity_type_is_type_claim_id = higher_type_is_type_claim,
				.left_context_certificate_id =
					endpoints.left_context_certificate_id,
				.right_context_certificate_id =
					endpoints.right_context_certificate_id,
				.pointwise_left_input_binding_id = PROTOTYPE_INVALID_ID,
				.pointwise_right_input_binding_id = PROTOTYPE_INVALID_ID,
				.pointwise_input_identity_binding_id = PROTOTYPE_INVALID_ID
			}
		};
		uint32_t certificate_id;
		struct prototype_kernel_view view;
		if (prototype_kernel_builder_view(kernel, &view) != 0 ||
			prototype_hott_action_certificate_add(
				actions, &view, bridges, certificate, &certificate_id
			) != 0 || hott_publish_ready_action_result(
				actions,
				terms,
				request_id,
				certificate_id,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				0,
				&result
			) != 0) {
			status = -1;
		}
	}
	free(source_boundaries);
	free(target_boundaries);
	free(target_claims);
	if (status == 1) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
			p_result_id
		);
	}
	if (status != 0) {
		fprintf(
			stderr,
			"repeated dimension action failed request=%u dimension=%u stage=%d "
			"status=%d source-boundaries=%zu target-boundaries=%zu failed=%zu\n",
			request_id,
			source_dimension,
			stage,
			status,
			source_boundary_count,
			target_boundary_count,
			failed_boundary
		);
		return -1;
	}
	*p_result_id = result;
	return 0;
}

static int hott_build_nondependent_pi_identity_type(
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_dimension_operator_db* dimension_operators,
	uint32_t dimension_operator_id,
	uint32_t source_carrier,
	uint32_t left_endpoint_binding,
	uint32_t right_endpoint_binding,
	uint32_t x0_binding,
	uint32_t x1_binding,
	uint32_t xr_binding,
	uint32_t* p_identity_type
) {
	if (!terms || !type_declarations || !dimension_operators || !p_identity_type ||
		source_carrier >= terms->term_count) {
		return -1;
	}
	int is_computation_function = terms->terms[source_carrier].tag ==
		PROTOTYPE_TERM_THUNK_TYPE;
	uint32_t pi = is_computation_function ?
		terms->terms[source_carrier].as.thunk_type.computation : source_carrier;
	if (pi >= terms->term_count || terms->terms[pi].tag != PROTOTYPE_TERM_PI) {
		return -1;
	}
	uint32_t domain = terms->terms[pi].as.pi.domain;
	uint32_t codomain_binding;
	uint32_t codomain;
	if (prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) != 0 || codomain >= terms->term_count ||
		prototype_term_contains_free_binding(terms, codomain, codomain_binding) ||
		(is_computation_function ?
			(terms->terms[codomain].tag != PROTOTYPE_TERM_COMPUTATION_TYPE ||
			 prototype_term_effect_row_purity(
				terms, terms->terms[codomain].as.computation_type.label
			 ) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) :
			terms->terms[codomain].tag != PROTOTYPE_TERM_UNIVERSE_VAR)) {
		return -1;
	}
	uint32_t codomain_thunk;
	uint32_t domain_identity_former;
	uint32_t codomain_identity_former;
	if (prototype_term_dimension_action(
			terms,
			dimension_operators,
			domain,
			dimension_operator_id,
			&domain_identity_former
		) != 0) {
		return -1;
	}
	if (is_computation_function) {
		if (prototype_term_thunk_type(terms, codomain, &codomain_thunk) != 0) {
			return -1;
		}
	} else {
		codomain_thunk = codomain;
	}
	if (prototype_term_dimension_action(
			terms,
			dimension_operators,
			codomain_thunk,
			dimension_operator_id,
			&codomain_identity_former
		) != 0) {
		return -1;
	}
	uint32_t u0;
	uint32_t u1;
	uint32_t x0;
	uint32_t x1;
	uint32_t domain_identity;
	if (x0_binding == PROTOTYPE_INVALID_ID || x1_binding == PROTOTYPE_INVALID_ID ||
		xr_binding == PROTOTYPE_INVALID_ID || prototype_term_var(
			terms, left_endpoint_binding, &u0
		) != 0 || prototype_term_var(
			terms, right_endpoint_binding, &u1
		) != 0 || prototype_term_var(
			terms, x0_binding, &x0
		) != 0 || prototype_term_var(
			terms, x1_binding, &x1
		) != 0) {
		return -1;
	}
	uint32_t domain_identity_arguments[2] = { x0, x1 };
	domain_identity = domain_identity_former;
	for (uint32_t i = 0; i < 2; ++i) {
		if (prototype_term_app(
				terms,
				domain_identity,
				domain_identity_arguments[i],
				&domain_identity
			) != 0) {
			return -1;
		}
	}
	uint32_t forced_u0;
	uint32_t forced_u1;
	uint32_t applied_u0;
	uint32_t applied_u1;
	uint32_t thunked_applied_u0;
	uint32_t thunked_applied_u1;
	uint32_t result_identity;
	if (is_computation_function) {
		if (prototype_term_force(terms, u0, &forced_u0) != 0 ||
			prototype_term_force(terms, u1, &forced_u1) != 0) {
			return -1;
		}
	} else {
		forced_u0 = u0;
		forced_u1 = u1;
	}
	if (prototype_term_app(
			terms, forced_u0, x0, &applied_u0
		) != 0 || prototype_term_app(
			terms, forced_u1, x1, &applied_u1
		) != 0) {
		return -1;
	}
	if (is_computation_function) {
		if (prototype_term_thunk(terms, applied_u0, &thunked_applied_u0) != 0 ||
			prototype_term_thunk(terms, applied_u1, &thunked_applied_u1) != 0) {
			return -1;
		}
	} else {
		/* A type-family application is pure type computation. Normalize its
		 * endpoint before materializing the Universe identity TypeView so the
		 * typed source and its erased core describe the same fiber. */
		if (prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				applied_u0,
				&thunked_applied_u0
			) != 0 || prototype_term_normalize_complete_with_profile(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				applied_u1,
				&thunked_applied_u1
			) != 0) {
			return -1;
		}
	}
	uint32_t result_identity_arguments[2] = {
		thunked_applied_u0, thunked_applied_u1
	};
	result_identity = codomain_identity_former;
	for (uint32_t i = 0; i < 2; ++i) {
		if (prototype_term_app(
				terms,
				result_identity,
				result_identity_arguments[i],
				&result_identity
			) != 0) {
			return -1;
		}
	}
	uint32_t classifier = result_identity;
	uint32_t binders[3] = { x0_binding, x1_binding, xr_binding };
	uint32_t domains[3] = { domain, domain, domain_identity };
	for (uint32_t i = 3; i > 0; --i) {
		uint32_t family;
		uint32_t pi_classifier;
		uint32_t family_body = classifier;
		if (is_computation_function) {
			uint32_t empty_effects;
			if (prototype_term_effect_row_empty(terms, &empty_effects
				) != 0 || prototype_term_total_computation_type(
					terms, empty_effects, classifier, &family_body
				) != 0) {
				return -1;
			}
		}
		if (prototype_term_pure_family(
				terms, binders[i - 1], family_body, &family
			) != 0 || prototype_term_pi_family(
				terms, domains[i - 1], family, &pi_classifier
			) != 0 || (is_computation_function && prototype_term_thunk_type(
				terms, pi_classifier, &classifier
			) != 0)) {
			return -1;
		}
		if (!is_computation_function) {
			classifier = pi_classifier;
		}
	}
	*p_identity_type = classifier;
	return 0;
}

static int hott_publish_pi_identity_result(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	const struct prototype_judgement_proposition* source,
	const struct prototype_hott_bridge* bridge,
	int computation_rule,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	if (!actions || !terms || !contexts || !judgement || !source || !bridge ||
		!p_result_id || bridge->source_context_id != source->context_id ||
		computation_rule != PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
		fprintf(stderr, "Pi identity arguments invalid request=%u\n", request_id);
		return -1;
	}
	struct hott_identity_endpoint_setup endpoints;
	if (hott_prepare_identity_endpoints(
			kernel,
			prototype_hott_action_request_get(actions, request_id)->key.
				identity_type.source_claim_id,
			bridge,
			&endpoints
		) != 0) {
		fprintf(
			stderr,
			"Pi identity endpoint preparation failed request=%u context=%u bridge=%u\n",
			request_id,
			source->context_id,
			bridge->id
		);
		return -1;
	}
	uint32_t identity_type;
	uint32_t identity_type_claim;
	uint32_t identity_type_is_type_claim;
	uint32_t x0_binding = prototype_term_new_binding(terms);
	uint32_t x1_binding = prototype_term_new_binding(terms);
	uint32_t xr_binding = prototype_term_new_binding(terms);
	if (x0_binding == PROTOTYPE_INVALID_ID || x1_binding == PROTOTYPE_INVALID_ID ||
		xr_binding == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t source_pi = terms->terms[source->subject].tag ==
		PROTOTYPE_TERM_THUNK_TYPE ? terms->terms[
			source->subject
		].as.thunk_type.computation : source->subject;
	uint32_t codomain_binding;
	uint32_t codomain;
	uint32_t codomain_carrier;
	uint32_t domain_action;
	uint32_t domain_action_claim;
	uint32_t codomain_action = PROTOTYPE_INVALID_ID;
	uint32_t codomain_action_claim = PROTOTYPE_INVALID_ID;
	if (source_pi >= terms->term_count || terms->terms[source_pi].tag !=
			PROTOTYPE_TERM_PI || prototype_term_pure_family_parts(
			terms,
			terms->terms[source_pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) != 0 || prototype_term_contains_free_binding(
			terms, codomain, codomain_binding
		)) {
		fprintf(
			stderr,
			"Pi identity source shape rejected source=%u pi=%u codomain=%u binder=%u\n",
			source->subject,
			source_pi,
			codomain,
			codomain_binding
		);
		return -1;
	}
	if (terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK_TYPE) {
		if (prototype_term_thunk_type(
				terms, codomain, &codomain_carrier
			) != 0) {
			return -1;
		}
	} else {
		codomain_carrier = codomain;
	}
	int domain_action_status = hott_ensure_dimension_action_type_claim(
			kernel,
			source->context_id,
			terms->terms[source_pi].as.pi.domain,
			source->classifier,
			bridge->dimension_operator_id,
			&domain_action,
			&domain_action_claim
		);
	int codomain_action_status = 0;
	if (domain_action_status == 0 && terms->terms[codomain_carrier].tag !=
			PROTOTYPE_TERM_UNIVERSE_VAR) {
		codomain_action_status = hott_ensure_dimension_action_type_claim(
			kernel,
			source->context_id,
				codomain_carrier,
				source->classifier,
				bridge->dimension_operator_id,
				&codomain_action,
				&codomain_action_claim
			);
	}
	if (domain_action_status != 0 || codomain_action_status != 0) {
		fprintf(
			stderr,
			"pointwise dimension action premise formation failed domain=%d "
			"codomain=%d\n",
			domain_action_status,
			codomain_action_status
		);
		return -1;
	}
	uint32_t endpoint_bridge_projection;
	uint32_t endpoint_projection;
	uint32_t domain_action_in_endpoint;
	uint32_t codomain_action_in_endpoint = PROTOTYPE_INVALID_ID;
	int endpoint_reindex_stage = 1;
	int endpoint_reindex_status = prototype_substitution_projection_path(
			kernel->substitutions,
			contexts,
			endpoints.endpoint_context_id,
			bridge->bridge_context_id,
			&endpoint_bridge_projection
		);
	if (endpoint_reindex_status == 0) {
		endpoint_reindex_stage = 2;
		endpoint_reindex_status = prototype_substitution_compose(
			kernel->substitutions,
			contexts,
			bridge->left_substitution_id,
			endpoint_bridge_projection,
			&endpoint_projection
		);
	}
	if (endpoint_reindex_status == 0) {
		endpoint_reindex_stage = 3;
		endpoint_reindex_status = prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			kernel->type_declarations,
			contexts,
			kernel->substitutions,
			domain_action_claim,
			endpoint_projection,
			&domain_action_in_endpoint
		);
	}
	if (endpoint_reindex_status == 0 && codomain_action_claim !=
			PROTOTYPE_INVALID_ID) {
		endpoint_reindex_stage = 4;
		endpoint_reindex_status = prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			kernel->type_declarations,
			contexts,
			kernel->substitutions,
			codomain_action_claim,
			endpoint_projection,
			&codomain_action_in_endpoint
		);
	}
	if (endpoint_reindex_status != 0) {
		fprintf(
			stderr,
			"Pi identity endpoint action reindex failed stage=%d status=%d "
			"source-context=%u target-context=%u domain-claim=%u codomain-claim=%u\n",
			endpoint_reindex_stage,
			endpoint_reindex_status,
			source->context_id,
			endpoints.endpoint_context_id,
			domain_action_claim,
			codomain_action_claim
		);
		return -1;
	}
	if (hott_build_nondependent_pi_identity_type(
			terms,
			kernel->type_declarations,
			kernel->dimension_operators,
			bridge->dimension_operator_id,
			source->subject,
			endpoints.left_binding_id,
			endpoints.right_binding_id,
			x0_binding,
			x1_binding,
			xr_binding,
			&identity_type
		) != 0) {
		fprintf(stderr, "pointwise dimension identity term construction failed\n");
		return -1;
	}
	uint32_t x0;
	uint32_t x1;
	uint32_t domain_identity;
	uint32_t x0_context;
	uint32_t x1_context;
	uint32_t xr_context;
	uint32_t x1_bridge_projection;
	uint32_t xr_bridge_projection;
	uint32_t x1_projection;
	uint32_t xr_projection;
	uint32_t domain_action_in_x1;
	uint32_t domain_action_in_xr;
	uint32_t codomain_action_in_xr = PROTOTYPE_INVALID_ID;
	if (prototype_term_var(terms, x0_binding, &x0) != 0 ||
		prototype_term_var(terms, x1_binding, &x1) != 0 ||
		prototype_term_app(
			terms, domain_action, x0, &domain_identity
		) != 0 || prototype_term_app(
			terms, domain_identity, x1, &domain_identity
		) != 0 || prototype_context_extend(
			contexts,
			endpoints.endpoint_context_id,
			x0_binding,
			terms->terms[source_pi].as.pi.domain,
			PROTOTYPE_INVALID_ID,
			&x0_context
		) != 0 || prototype_context_extend(
			contexts,
			x0_context,
			x1_binding,
			terms->terms[source_pi].as.pi.domain,
			PROTOTYPE_INVALID_ID,
			&x1_context
		) != 0 || prototype_context_extend(
			contexts,
			x1_context,
			xr_binding,
			domain_identity,
			PROTOTYPE_INVALID_ID,
			&xr_context
		) != 0 || prototype_substitution_projection_path(
			kernel->substitutions,
			contexts,
			x1_context,
			bridge->bridge_context_id,
			&x1_bridge_projection
		) != 0 || prototype_substitution_projection_path(
			kernel->substitutions,
			contexts,
			xr_context,
			bridge->bridge_context_id,
			&xr_bridge_projection
		) != 0 || prototype_substitution_compose(
			kernel->substitutions,
			contexts,
			bridge->left_substitution_id,
			x1_bridge_projection,
			&x1_projection
		) != 0 || prototype_substitution_compose(
			kernel->substitutions,
			contexts,
			bridge->left_substitution_id,
			xr_bridge_projection,
			&xr_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			kernel->type_declarations,
			contexts,
			kernel->substitutions,
			domain_action_claim,
			x1_projection,
			&domain_action_in_x1
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			kernel->type_declarations,
			contexts,
			kernel->substitutions,
			domain_action_claim,
			xr_projection,
			&domain_action_in_xr
		) != 0 || (codomain_action_claim != PROTOTYPE_INVALID_ID &&
		 prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			kernel->type_declarations,
			contexts,
			kernel->substitutions,
			codomain_action_claim,
			xr_projection,
				&codomain_action_in_xr
			) != 0)) {
		fprintf(stderr, "Pi identity pointwise context construction failed\n");
		return -1;
	}
	uint32_t domain_identity_claim;
	uint32_t result_identity_claim = PROTOTYPE_INVALID_ID;
	uint32_t x0_claim;
	uint32_t x1_claim;
	uint32_t domain_identity_left;
	uint32_t domain_identity_left_claim;
	if (prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			x1_context,
			x0_binding,
			terms->terms[source_pi].as.pi.domain,
			&x0_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			x1_context,
			x1_binding,
			terms->terms[source_pi].as.pi.domain,
			&x1_claim
		) != 0 || hott_apply_claim_argument(
			kernel,
			x1_context,
			domain_action_in_x1,
			x0_claim,
			&domain_identity_left,
			&domain_identity_left_claim
		) != 0 || hott_apply_claim_argument(
			kernel,
			x1_context,
			domain_identity_left_claim,
			x1_claim,
			&domain_identity,
			&domain_identity_claim
		) != 0) {
		fprintf(stderr, "pointwise domain action application formation failed\n");
		return -1;
	}
	if (codomain_action != PROTOTYPE_INVALID_ID) {
		uint32_t u0;
		uint32_t u1;
		uint32_t forced_u0;
		uint32_t forced_u1;
		uint32_t applied_u0;
		uint32_t applied_u1;
		uint32_t thunked_u0;
		uint32_t thunked_u1;
		uint32_t result_identity;
		uint32_t u0_claim;
		uint32_t u1_claim;
		uint32_t xr_x0_claim;
		uint32_t xr_x1_claim;
		uint32_t forced_u0_claim;
		uint32_t forced_u1_claim;
		uint32_t applied_u0_claim;
		uint32_t applied_u1_claim;
		uint32_t thunked_u0_claim;
		uint32_t thunked_u1_claim;
		uint32_t result_identity_left;
		uint32_t result_identity_left_claim;
		if (prototype_term_var(
				terms, endpoints.left_binding_id, &u0
			) != 0 || prototype_term_var(
				terms, endpoints.right_binding_id, &u1
			) != 0 || prototype_term_force(
				terms, u0, &forced_u0
			) != 0 || prototype_term_force(
				terms, u1, &forced_u1
			) != 0 || prototype_term_app(
				terms, forced_u0, x0, &applied_u0
			) != 0 || prototype_term_app(
				terms, forced_u1, x1, &applied_u1
			) != 0 || prototype_term_thunk(
				terms, applied_u0, &thunked_u0
			) != 0 || prototype_term_thunk(
				terms, applied_u1, &thunked_u1
			) != 0 || prototype_term_app(
				terms, codomain_action, thunked_u0, &result_identity
			) != 0 || prototype_term_app(
				terms, result_identity, thunked_u1, &result_identity
			) != 0 || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				xr_context,
				endpoints.left_binding_id,
				source->subject,
				&u0_claim
			) != 0 || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				xr_context,
				endpoints.right_binding_id,
				source->subject,
				&u1_claim
			) != 0 || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				xr_context,
				x0_binding,
				terms->terms[source_pi].as.pi.domain,
				&xr_x0_claim
			) != 0 || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				xr_context,
				x1_binding,
				terms->terms[source_pi].as.pi.domain,
				&xr_x1_claim
			) != 0 || prototype_judgement_add_force_claim(
				judgement,
				terms,
				xr_context,
				forced_u0,
				source_pi,
				u0_claim,
				&forced_u0_claim
			) != 0 || prototype_judgement_add_force_claim(
				judgement,
				terms,
				xr_context,
				forced_u1,
				source_pi,
				u1_claim,
				&forced_u1_claim
			) != 0 || prototype_judgement_add_app_claim(
				judgement,
				terms,
				kernel->type_declarations,
				contexts,
				kernel->substitutions,
				xr_context,
				applied_u0,
				codomain,
				forced_u0_claim,
				xr_x0_claim,
				&applied_u0_claim
			) != 0 || prototype_judgement_add_app_claim(
				judgement,
				terms,
				kernel->type_declarations,
				contexts,
				kernel->substitutions,
				xr_context,
				applied_u1,
				codomain,
				forced_u1_claim,
				xr_x1_claim,
				&applied_u1_claim
			) != 0 || prototype_judgement_add_thunk_claim(
				judgement,
				terms,
				kernel->type_declarations,
				xr_context,
				thunked_u0,
				codomain_carrier,
				applied_u0_claim,
				&thunked_u0_claim
			) != 0 || prototype_judgement_add_thunk_claim(
				judgement,
				terms,
				kernel->type_declarations,
				xr_context,
				thunked_u1,
				codomain_carrier,
				applied_u1_claim,
				&thunked_u1_claim
			) != 0 || hott_apply_claim_argument(
				kernel,
				xr_context,
				codomain_action_in_xr,
				thunked_u0_claim,
				&result_identity_left,
				&result_identity_left_claim
			) != 0 || hott_apply_claim_argument(
				kernel,
				xr_context,
				result_identity_left_claim,
				thunked_u1_claim,
				&result_identity,
				&result_identity_claim
			) != 0) {
			fprintf(stderr, "pointwise result action application formation failed\n");
			return -1;
		}
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			kernel->type_declarations,
			contexts,
			kernel->substitutions,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			&identity_type_claim
		) != 0) {
		fprintf(stderr, "pointwise dimension identity formation failed\n");
		return -1;
	}
	(void)domain_action;
	(void)domain_action_in_endpoint;
	(void)codomain_action;
	(void)codomain_action_in_endpoint;
	(void)domain_action_in_x1;
	(void)domain_action_in_xr;
	(void)codomain_action_in_xr;
	(void)domain_identity_claim;
	(void)result_identity_claim;
	if (prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoints.endpoint_context_id,
			identity_type,
			source->classifier,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		fprintf(stderr, "pointwise dimension identity IS_TYPE failed\n");
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION,
		.data.identity_type = {
			.computation_rule = computation_rule,
			.endpoint_context_id = endpoints.endpoint_context_id,
			.left_endpoint_binding_id = endpoints.left_binding_id,
			.right_endpoint_binding_id = endpoints.right_binding_id,
			.backing_type_former_term_id = PROTOTYPE_INVALID_ID,
			.backing_type_former_has_type_claim_id = PROTOTYPE_INVALID_ID,
			.identity_type_term_id = identity_type,
			.identity_type_has_type_claim_id = identity_type_claim,
			.identity_type_is_type_claim_id = identity_type_is_type_claim,
			.left_context_certificate_id =
				endpoints.left_context_certificate_id,
			.right_context_certificate_id =
				endpoints.right_context_certificate_id,
			.pointwise_left_input_binding_id = x0_binding,
			.pointwise_right_input_binding_id = x1_binding,
			.pointwise_input_identity_binding_id = xr_binding
		}
	};
	uint32_t certificate_id;
	struct prototype_kernel_view view;
	if (prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_certificate_add(
			actions, &view, bridges, certificate, &certificate_id
		) != 0) {
		fprintf(stderr, "pointwise dimension identity certificate failed\n");
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

int prototype_parametricity_execute_relation_type_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !p_result_id ||
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
	const struct prototype_judgement_claim* source = request &&
		request->kind == PROTOTYPE_HOTT_ACTION_RELATION_TYPE ?
		prototype_judgement_claim_get(
			judgement, request->key.relation_type.source_claim_id
		) : NULL;
	const struct prototype_hott_bridge* bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.relation_type.source_bridge_id
		) : NULL;
	if (!source || !bridge || prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id != bridge->source_context_id) {
		return -1;
	}
	struct prototype_hott_type_former_descriptor descriptor;
	if (prototype_hott_type_former_descriptor_query(
		terms,
		type_declarations,
		contexts,
		kernel->dimension_operators,
		judgement,
			request->key.relation_type.source_claim_id,
			&descriptor
		) != 0) {
		return -1;
	}
	if (!descriptor.admitted) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			descriptor.residual_reason,
			p_result_id
		);
	}

	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t endpoint_context;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	uint32_t left_projection;
	uint32_t endpoint_to_left;
	uint32_t endpoint_projection;
	uint32_t left_type_claim;
	uint32_t right_type_base_claim;
	uint32_t right_type_claim;
	uint32_t endpoint_left_type_claim;
	uint32_t endpoint_type_claim;
	uint32_t left_endpoint_claim;
	uint32_t right_endpoint_claim;
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	uint32_t relation_type;
	uint32_t relation_family;
	uint32_t relation_claim;
	uint32_t left_type;
	uint32_t right_type;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.relation_type.source_claim_id,
			bridge->left_substitution_id,
			&left_type_claim
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			request->key.relation_type.source_claim_id,
			bridge->right_substitution_id,
			&right_type_base_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* left_type_evidence =
		prototype_judgement_claim_get(judgement, left_type_claim);
	if (!left_type_evidence) {
		return -1;
	}
	left_type = prototype_judgement_proposition_get(judgement, left_type_evidence->proposition_id)->subject;
	if (
		prototype_context_extend(
			contexts,
			bridge->bridge_context_id,
			left_binding,
			left_type,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 ||
		prototype_cwf_certificate_db_add_context(
			cwf_certificates,
			contexts,
			terms,
			type_declarations,
			judgement,
			left_context,
			left_type_claim,
			&left_context_certificate
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, left_context, &left_projection
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			right_type_base_claim,
			left_projection,
			&right_type_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			left_context,
			left_binding,
			left_type,
			&left_endpoint_claim
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* right_type_evidence =
		prototype_judgement_claim_get(judgement, right_type_claim);
	if (!right_type_evidence) {
		return -1;
	}
	right_type = prototype_judgement_proposition_get(judgement, right_type_evidence->proposition_id)->subject;
	if (
		prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			right_type,
			PROTOTYPE_INVALID_ID,
			&endpoint_context
		) != 0 ||
		prototype_cwf_certificate_db_add_context(
			cwf_certificates,
			contexts,
			terms,
			type_declarations,
			judgement,
			endpoint_context,
			right_type_claim,
			&right_context_certificate
		) != 0 ||
		prototype_substitution_projection_path(
			substitutions,
			contexts,
			endpoint_context,
			bridge->bridge_context_id,
			&endpoint_projection
		) != 0 ||
		prototype_substitution_projection(
			substitutions, contexts, endpoint_context, &endpoint_to_left
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			left_type_claim,
			endpoint_projection,
			&endpoint_left_type_claim
		) != 0 ||
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			type_declarations,
			contexts,
			substitutions,
			right_type_claim,
			endpoint_to_left,
			&endpoint_type_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoint_context,
			left_binding,
			left_type,
			&left_endpoint_claim
		) != 0 ||
		prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			endpoint_context,
			right_binding,
			right_type,
			&right_endpoint_claim
		) != 0 ||
		prototype_term_var(terms, left_binding, &left_endpoint) != 0 ||
		prototype_term_var(terms, right_binding, &right_endpoint) != 0 ||
		prototype_term_relation_type(
			terms,
			left_type,
			right_type,
			left_endpoint,
			right_endpoint,
			&relation_type
		) != 0 ||
		prototype_judgement_add_relation_type_formation(
			judgement,
			terms,
			endpoint_context,
			relation_type,
			prototype_judgement_claim_proposition(
				judgement, endpoint_left_type_claim
			)->classifier,
			endpoint_left_type_claim,
			endpoint_type_claim,
			left_endpoint_claim,
			right_endpoint_claim,
			&relation_claim
		) != 0 || prototype_term_pure_family(
			terms,
			right_binding,
			relation_type,
			&relation_family
		) != 0 || prototype_term_pure_family(
			terms,
			left_binding,
			relation_family,
			&relation_family
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE,
		.data.relation_type = {
			.endpoint_context_id = endpoint_context,
			.left_endpoint_binding_id = left_binding,
			.right_endpoint_binding_id = right_binding,
			.relation_family_semantics =
				PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION,
			.relation_family_term_id = relation_family,
			.relation_type_term_id = relation_type,
			.relation_is_type_claim_id = relation_claim,
			.left_context_certificate_id = left_context_certificate,
			.right_context_certificate_id = right_context_certificate
		}
	};
	uint32_t certificate_id;
	if (prototype_hott_action_certificate_add(
			actions, &view, bridges,
			certificate,
			&certificate_id
		) != 0) {
		return -1;
	}
	return hott_publish_ready_action_result(
		actions,
		terms,
		request_id,
		certificate_id,
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		0,
		p_result_id
	);
}

int prototype_hott_execute_identity_type_computation(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations =
		kernel ? kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_judgement_db* judgement =
		kernel ? kernel->judgement : NULL;
	if (!actions || !terms || !type_declarations || !contexts || !judgement ||
		!bridges || !p_result_id ||
		prototype_kernel_builder_validate_stores(kernel) != 0) {
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
	if (!request || request->kind !=
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION) {
		return -1;
	}
	const struct prototype_judgement_proposition* source =
		prototype_judgement_claim_proposition(
			judgement, request->key.identity_type.source_claim_id
		);
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, request->key.identity_type.source_bridge_id
	);
	if (!source || !bridge || source->context_id != bridge->source_context_id) {
		return -1;
	}
	uint32_t source_action_family;
	uint32_t source_action_root;
	uint32_t source_action_operator;
	uint32_t source_action_dimension;
	size_t source_action_argument_count;
	int source_is_dimension_action =
		prototype_dimension_action_family_instance_info(
			terms,
			kernel->dimension_operators,
			source->subject,
			&source_action_family,
			&source_action_root,
			&source_action_operator,
			&source_action_dimension,
			NULL,
			0,
			&source_action_argument_count
		) == 0;
	struct prototype_hott_type_former_descriptor descriptor;
	if (prototype_hott_type_former_descriptor_query(
			terms,
			type_declarations,
			contexts,
			kernel->dimension_operators,
			judgement,
			request->key.identity_type.source_claim_id,
			&descriptor
		) != 0) {
		fprintf(
			stderr,
			"identity type descriptor failed request=%u source_claim=%u "
			"context=%u subject=%u classifier=%u\n",
			request_id,
			request->key.identity_type.source_claim_id,
			source->context_id,
			source->subject,
			source->classifier
		);
		return -1;
	}
	if (descriptor.capabilities.identity_computation ==
			PROTOTYPE_HOTT_CAPABILITY_DEFERRED) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
			p_result_id
		);
	}
	if (descriptor.capabilities.identity_computation !=
			PROTOTYPE_HOTT_CAPABILITY_SUPPORTED) {
		int reason = descriptor.residual_reason !=
			PROTOTYPE_HOTT_RESIDUAL_NONE ? descriptor.residual_reason :
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			reason,
			p_result_id
		);
	}
	if (source_is_dimension_action) {
		return hott_publish_repeated_dimension_action_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			p_result_id
		);
	}
	if (source->context_id != prototype_context_empty(contexts)) {
		if (!hott_term_is_constant_over_context(
				terms, contexts, source->context_id, source->subject
			)) {
			return hott_publish_action_residual(
				actions,
				terms,
				request_id,
				PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
				p_result_id
			);
		}
		if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT) {
			uint32_t source_type_id;
			const struct prototype_type_declaration* source_type;
			if (prototype_type_view_declaration_query(
					&type_declarations->semantic_schema,
					contexts,
					terms,
					source->subject,
					&source_type_id,
					&source_type
				) != 0 || !source_type || source_type->parameter_count != 0 ||
				source_type->constructor_count == 0) {
				return -1;
			}
			(void)source_type_id;
			return hott_publish_dimension_action_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
				p_result_id
			);
		}
		if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_THUNK) {
			const struct prototype_term* thunk_type =
				&terms->terms[source->subject];
			const struct prototype_term* computation_type =
				&terms->terms[thunk_type->as.thunk_type.computation];
			if (computation_type->tag == PROTOTYPE_TERM_COMPUTATION_TYPE) {
				return hott_publish_dimension_action_identity_result(
					actions,
					kernel,
					bridges,
					request_id,
					source,
					bridge,
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN,
					p_result_id
				);
			}
			if (computation_type->tag == PROTOTYPE_TERM_PI) {
				return hott_publish_pi_identity_result(
					actions,
					kernel,
					bridges,
					request_id,
					source,
					bridge,
					PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
					p_result_id
				);
			}
		}
		if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_PI) {
			return hott_publish_pi_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
				p_result_id
			);
		}
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
			p_result_id
		);
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_PI) {
		return hott_publish_pi_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
			p_result_id
		);
	}
	if (descriptor.kind == PROTOTYPE_HOTT_TYPE_FORMER_THUNK) {
		const struct prototype_term* thunk_type =
			&terms->terms[source->subject];
		const struct prototype_term* computation_type =
			&terms->terms[thunk_type->as.thunk_type.computation];
		if (computation_type->tag == PROTOTYPE_TERM_PI) {
			return hott_publish_pi_identity_result(
				actions,
				kernel,
				bridges,
				request_id,
				source,
				bridge,
				PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE,
				p_result_id
			);
		}
		if (computation_type->tag != PROTOTYPE_TERM_COMPUTATION_TYPE) {
			return -1;
		}
		return hott_publish_dimension_action_identity_result(
			actions,
			kernel,
			bridges,
			request_id,
			source,
			bridge,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN,
			p_result_id
		);
	}
	if (descriptor.kind != PROTOTYPE_HOTT_TYPE_FORMER_ORDINARY_ADT) {
		return -1;
	}
	uint32_t source_type_id;
	const struct prototype_type_declaration* source_type;
	if (prototype_type_view_declaration_query(
			&type_declarations->semantic_schema,
			contexts,
			terms,
			source->subject,
			&source_type_id,
			&source_type
		) != 0 || !source_type || source_type->parameter_count != 0 ||
		source_type->constructor_count == 0) {
		return -1;
	}
	(void)source_type_id;
	return hott_publish_dimension_action_identity_result(
		actions,
		kernel,
		bridges,
		request_id,
		source,
		bridge,
		PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT,
		p_result_id
	);
}

static const struct prototype_hott_bridge*
