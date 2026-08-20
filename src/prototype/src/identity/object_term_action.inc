static int hott_build_object_action_environment(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t bridge_id,
	uint32_t* source_bindings,
	uint32_t* identity_claims,
	uint32_t capacity,
	struct hott_object_action_environment* p_environment
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_hott_bridge* bridge =
		prototype_hott_bridge_db_get(bridges, bridge_id);
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!bridges || !bridge || !source_bindings || !identity_claims ||
		!p_environment) {
		return -1;
	}
	uint32_t count = 0;
	const struct prototype_hott_bridge* current = bridge;
	while (current->source_context_id != prototype_context_empty(contexts)) {
		if (current->id >= bridges->certificate_count || count >= capacity) {
			return -1;
		}
		const struct prototype_hott_bridge_certificate* certificate =
			&bridges->certificates[current->id];
		const struct prototype_context* source_context = prototype_context_get(
			contexts, current->source_context_id
		);
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, certificate->fiber_witness_claim_id
			);
		if (!source_context || !witness || certificate->semantics !=
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_OBJECT_IDENTITY ||
			certificate->parent_bridge_id == PROTOTYPE_INVALID_ID ||
			witness->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			witness->context_id != current->bridge_context_id) {
			return -1;
		}
		uint32_t witness_claim = certificate->fiber_witness_claim_id;
		if (current->bridge_context_id != bridge->bridge_context_id) {
			uint32_t projection;
			if (prototype_substitution_projection_path(
					substitutions,
					contexts,
					bridge->bridge_context_id,
					current->bridge_context_id,
					&projection
				) != 0 || prototype_judgement_add_reindexed_claim(
					judgement,
					terms,
					types,
					contexts,
					substitutions,
					witness_claim,
					projection,
					&witness_claim
				) != 0) {
				return -1;
			}
		}
		source_bindings[count] = source_context->binding_id;
		identity_claims[count] = witness_claim;
		count++;
		current = prototype_hott_bridge_db_get(
			bridges, certificate->parent_bridge_id
		);
		if (!current) {
			return -1;
		}
	}
	if (current->id >= bridges->certificate_count ||
		bridges->certificates[current->id].semantics !=
			PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY) {
		return -1;
	}
	*p_environment = (struct hott_object_action_environment) {
		.left_substitution_id = bridge->left_substitution_id,
		.right_substitution_id = bridge->right_substitution_id,
		.target_context_id = bridge->bridge_context_id,
		.source_binding_ids = source_bindings,
		.identity_claim_ids = identity_claims,
		.binding_count = count
	};
	return 0;
}

static int hott_instantiate_object_identity_family(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t target_context_id,
	uint32_t left_claim_id,
	uint32_t right_claim_id,
	uint32_t* p_identity_family_claim_id
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
	const struct prototype_judgement_proposition* left = judgement ?
		prototype_judgement_claim_proposition(judgement, left_claim_id) : NULL;
	const struct prototype_judgement_proposition* right = judgement ?
		prototype_judgement_claim_proposition(judgement, right_claim_id) : NULL;
	const struct prototype_context* endpoint = identity && contexts ?
		prototype_context_get(contexts, identity->endpoint_context_id) : NULL;
	const struct prototype_context* left_endpoint = endpoint ?
		prototype_context_get(contexts, endpoint->parent) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!identity || !left || !right || !endpoint || !left_endpoint ||
		!p_identity_family_claim_id || left->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || right->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || left->context_id !=
			target_context_id || right->context_id != target_context_id ||
		left_endpoint->parent != target_context_id ||
		left_endpoint->binding_id != identity->left_endpoint_binding_id ||
		endpoint->binding_id != identity->right_endpoint_binding_id) {
		return -1;
	}
	uint32_t expected_left_classifier = prototype_context_classifier_term(
		left_endpoint
	);
	uint32_t expected_right_classifier = prototype_context_classifier_term(endpoint);
	if (left->classifier != expected_left_classifier) {
		uint32_t converted_left_claim;
		if (prototype_judgement_classifier_conversion(
				terms, types, left->classifier, expected_left_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				target_context_id,
				left->subject,
				expected_left_classifier,
				left_claim_id,
				&converted_left_claim
			) != 0) {
			return -1;
		}
		left_claim_id = converted_left_claim;
		left = prototype_judgement_claim_proposition(judgement, left_claim_id);
	}
	if (right->classifier != expected_right_classifier) {
		uint32_t converted_right_claim;
		if (prototype_judgement_classifier_conversion(
				terms, types, right->classifier, expected_right_classifier
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				target_context_id,
				right->subject,
				expected_right_classifier,
				right_claim_id,
				&converted_right_claim
			) != 0) {
			return -1;
		}
		right_claim_id = converted_right_claim;
		right = prototype_judgement_claim_proposition(judgement, right_claim_id);
	}
	if (!left || !right || left->classifier != expected_left_classifier ||
		right->classifier != expected_right_classifier) {
		return -1;
	}

	uint32_t base_substitution;
	uint32_t left_substitution;
	uint32_t endpoint_substitution;
	uint32_t left_substitution_certificate;
	uint32_t right_substitution_certificate;
	if (prototype_substitution_identity(
			substitutions, contexts, target_context_id, &base_substitution
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			base_substitution,
			endpoint->parent,
			left->subject,
			left->classifier,
			&left_substitution
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			left_substitution,
			left_claim_id,
			&left_substitution_certificate
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			left_substitution,
			identity->endpoint_context_id,
			right->subject,
			right->classifier,
			&endpoint_substitution
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			endpoint_substitution,
			right_claim_id,
			&right_substitution_certificate
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			identity->identity_type_has_type_claim_id,
			endpoint_substitution,
			p_identity_family_claim_id
		) != 0) {
		return -1;
	}
	(void)left_substitution_certificate;
	(void)right_substitution_certificate;
	return 0;
}

static int hott_construct_object_variable_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
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
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!identity || !source || !environment || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source->subject >=
			terms->term_count || terms->terms[source->subject].tag !=
			PROTOTYPE_TERM_VAR) {
		return -1;
	}
	uint32_t source_binding = terms->terms[source->subject].as.var.binding_id;
	uint32_t mapped_identity_claim_id = PROTOTYPE_INVALID_ID;
	for (uint32_t i = 0; i < environment->binding_count; ++i) {
		if (environment->source_binding_ids[i] == source_binding) {
			mapped_identity_claim_id = environment->identity_claim_ids[i];
			break;
		}
	}
	if (mapped_identity_claim_id == PROTOTYPE_INVALID_ID) {
		return 1;
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	uint32_t identity_family_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim_id
		) != 0 || hott_instantiate_object_identity_family(
			kernel,
			identity,
			environment->target_context_id,
			left_claim_id,
			right_claim_id,
			&identity_family_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* family =
		prototype_judgement_claim_proposition(
			judgement, identity_family_claim_id
		);
	const struct prototype_judgement_proposition* mapped =
		prototype_judgement_claim_proposition(
			judgement, mapped_identity_claim_id
		);
	if (!family || !mapped || mapped->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || mapped->context_id !=
			environment->target_context_id || prototype_judgement_classifier_conversion(
			terms, types, mapped->classifier, family->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	uint32_t witness_claim_id = mapped_identity_claim_id;
	if (mapped->classifier != family->subject &&
		prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			mapped->subject,
			family->subject,
			mapped_identity_claim_id,
			&witness_claim_id
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim_id;
	*p_witness_term_id = mapped->subject;
	*p_witness_claim_id = witness_claim_id;
	return 0;
}

static int hott_prepare_local_object_identity(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	struct prototype_hott_identity_type_computation_certificate* p_identity
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
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!source || !environment || !p_identity || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || universe >= terms->term_count) {
		return -1;
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	int left_reindex_status = prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim_id
		);
	int right_reindex_status = left_reindex_status == 0 ?
		prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim_id
		) : -1;
	if (left_reindex_status != 0 || right_reindex_status != 0) {
		fprintf(stderr,
			"HOTT object action endpoint reindex failed source-claim=%u "
			"left=%d right=%d left-substitution=%u right-substitution=%u\n",
			source_claim_id, left_reindex_status, right_reindex_status,
			environment->left_substitution_id,
			environment->right_substitution_id);
		return -1;
	}
	const struct prototype_judgement_proposition* left =
		prototype_judgement_claim_proposition(judgement, left_claim_id);
	const struct prototype_judgement_proposition* right =
		prototype_judgement_claim_proposition(judgement, right_claim_id);
	if (!left || !right || left->context_id != environment->target_context_id ||
		right->context_id != environment->target_context_id ||
		prototype_judgement_classifier_conversion(
			terms, types, left->classifier, right->classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return 1;
	}
	uint32_t carrier_type_claim;
	uint32_t right_carrier_type_claim;
	uint32_t source_carrier_type_claim = hott_is_type_claim_for_subject(
		judgement, source->context_id, source->classifier
	);
	if (source_carrier_type_claim != PROTOTYPE_INVALID_ID) {
		const struct prototype_judgement_proposition* left_carrier;
		const struct prototype_judgement_proposition* right_carrier;
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source_carrier_type_claim,
				environment->left_substitution_id,
				&carrier_type_claim
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source_carrier_type_claim,
				environment->right_substitution_id,
				&right_carrier_type_claim
			) != 0 || !(left_carrier = prototype_judgement_claim_proposition(
				judgement, carrier_type_claim
			)) || !(right_carrier = prototype_judgement_claim_proposition(
				judgement, right_carrier_type_claim
			)) || left_carrier->subject != left->classifier ||
			right_carrier->subject != right->classifier) {
			fprintf(stderr, "local Identity carrier reindex validation failed\n");
			return -1;
		}
	} else if (hott_ensure_is_type_claim_in_context(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				environment->target_context_id,
				left->classifier,
				&carrier_type_claim
			) != 0) {
		return 1;
	} else if (hott_ensure_is_type_claim_in_context(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				environment->target_context_id,
				right->classifier,
				&right_carrier_type_claim
			) != 0) {
		return 1;
	}
	const struct prototype_judgement_proposition* carrier_type =
		prototype_judgement_claim_proposition(judgement, carrier_type_claim);
	const struct prototype_judgement_proposition* right_carrier_type =
		prototype_judgement_claim_proposition(judgement, right_carrier_type_claim);
	if (!carrier_type || !right_carrier_type ||
		carrier_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		right_carrier_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		carrier_type->classifier >= terms->term_count ||
		terms->terms[carrier_type->classifier].tag != PROTOTYPE_TERM_UNIVERSE_VAR ||
		prototype_judgement_classifier_conversion(
			terms,
			types,
			carrier_type->classifier,
			right_carrier_type->classifier
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		fprintf(stderr, "local Identity carrier universe validation failed\n");
		return -1;
	}
	uint32_t local_universe = carrier_type->classifier;
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t endpoint_context;
	uint32_t left_context_certificate;
	uint32_t right_context_certificate;
	uint32_t left_projection;
	uint32_t right_carrier_claim;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		right_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			environment->target_context_id,
			left_binding,
			left->classifier,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			left_context,
			carrier_type_claim,
			&left_context_certificate
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			left_context,
			environment->target_context_id,
			&left_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_carrier_type_claim,
			left_projection,
			&right_carrier_claim
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			right->classifier,
			PROTOTYPE_INVALID_ID,
			&endpoint_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			endpoint_context,
			right_carrier_claim,
			&right_context_certificate
		) != 0) {
		fprintf(stderr, "local Identity endpoint context construction failed\n");
		return -1;
	}
	uint32_t left_endpoint;
	uint32_t right_endpoint;
	if (prototype_term_var(
			terms, left_binding, &left_endpoint
		) != 0 || prototype_term_var(
			terms, right_binding, &right_endpoint
		) != 0) {
		fprintf(stderr, "local Identity endpoint variable construction failed\n");
		return -1;
	}
	uint32_t identity_type;
	uint32_t identity_type_claim = PROTOTYPE_INVALID_ID;
	uint32_t identity_type_is_type_claim;
	uint32_t backing_type_former = PROTOTYPE_INVALID_ID;
	uint32_t backing_type_former_claim = PROTOTYPE_INVALID_ID;
	uint32_t x0_binding = PROTOTYPE_INVALID_ID;
	uint32_t x1_binding = PROTOTYPE_INVALID_ID;
	uint32_t xr_binding = PROTOTYPE_INVALID_ID;
	uint32_t normalized_carrier;
	int computation_rule;
	if (prototype_term_normalize_complete_with_profile(
			terms,
			types,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			left->classifier,
			&normalized_carrier
		) != 0) {
		return 1;
	}
	int normalized_carrier_is_pi = terms->terms[normalized_carrier].tag ==
		PROTOTYPE_TERM_PI || (terms->terms[normalized_carrier].tag ==
			PROTOTYPE_TERM_THUNK_TYPE && terms->terms[normalized_carrier].
				as.thunk_type.computation < terms->term_count && terms->terms[
					terms->terms[normalized_carrier].as.thunk_type.computation
				].tag == PROTOTYPE_TERM_PI);
	if (normalized_carrier_is_pi) {
		computation_rule = PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE;
		uint32_t dimension_operator_id;
		x0_binding = prototype_term_new_binding(terms);
		x1_binding = prototype_term_new_binding(terms);
		xr_binding = prototype_term_new_binding(terms);
		if (x0_binding == PROTOTYPE_INVALID_ID ||
			x1_binding == PROTOTYPE_INVALID_ID ||
			xr_binding == PROTOTYPE_INVALID_ID ||
			prototype_dimension_operator_find(
				kernel->dimension_operators,
				0,
				1,
				NULL,
				0,
				&dimension_operator_id
			) != 0 ||
			hott_build_nondependent_pi_identity_type(
				terms,
				types,
				kernel->dimension_operators,
				dimension_operator_id,
				normalized_carrier,
				left_binding,
				right_binding,
				x0_binding,
				x1_binding,
				xr_binding,
				&identity_type
			) != 0) {
			return 1;
		}
	} else {
		uint32_t source_type_id;
		const struct prototype_type_declaration* source_type;
		uint32_t dimension_operator_id;
		uint32_t left_endpoint_claim;
		uint32_t right_endpoint_claim;
		uint32_t acted_at_left;
		uint32_t acted_at_left_claim;
		if (prototype_type_view_declaration_query(
				&types->semantic_schema,
				contexts,
				terms,
				normalized_carrier,
				&source_type_id,
				&source_type
			) != 0 || !source_type || source_type->constructor_count == 0 ||
			prototype_dimension_operator_find(
				kernel->dimension_operators,
				0,
				1,
				NULL,
				0,
				&dimension_operator_id
			) != 0 || hott_ensure_dimension_action_type_claim(
				kernel,
				endpoint_context,
				normalized_carrier,
				local_universe,
				dimension_operator_id,
				&backing_type_former,
				&backing_type_former_claim
			) != 0 || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				endpoint_context,
				left_binding,
				left->classifier,
				&left_endpoint_claim
			) != 0 || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				endpoint_context,
				right_binding,
				right->classifier,
				&right_endpoint_claim
			) != 0 || hott_apply_claim_argument(
				kernel,
				endpoint_context,
				backing_type_former_claim,
				left_endpoint_claim,
				&acted_at_left,
				&acted_at_left_claim
			) != 0 || hott_apply_claim_argument(
				kernel,
				endpoint_context,
				acted_at_left_claim,
				right_endpoint_claim,
				&identity_type,
				&identity_type_claim
			) != 0) {
			return 1;
		}
		computation_rule = PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT;
		(void)source_type_id;
	}
	if ((identity_type_claim == PROTOTYPE_INVALID_ID &&
		((computation_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE &&
		  prototype_hott_ensure_dimension_classifier_formation(
			kernel, endpoint_context, identity_type, local_universe
		  ) != 0) || prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			endpoint_context,
			identity_type,
			local_universe,
			&identity_type_claim
		) != 0)) || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			endpoint_context,
			identity_type,
			local_universe,
			identity_type_claim,
			&identity_type_is_type_claim
		) != 0) {
		fprintf(
			stderr,
			"local Identity type formation failed type=%u claim=%u context=%u rule=%d\n",
			identity_type,
			identity_type_claim,
			endpoint_context,
			computation_rule
		);
		return -1;
	}
	*p_identity = (struct prototype_hott_identity_type_computation_certificate) {
		.computation_rule = computation_rule,
		.endpoint_context_id = endpoint_context,
		.left_endpoint_binding_id = left_binding,
		.right_endpoint_binding_id = right_binding,
		.backing_type_former_term_id = backing_type_former,
		.backing_type_former_has_type_claim_id = backing_type_former_claim,
		.identity_type_term_id = identity_type,
		.identity_type_has_type_claim_id = identity_type_claim,
		.identity_type_is_type_claim_id = identity_type_is_type_claim,
		.left_context_certificate_id = left_context_certificate,
		.right_context_certificate_id = right_context_certificate,
		.pointwise_left_input_binding_id = x0_binding,
		.pointwise_right_input_binding_id = x1_binding,
		.pointwise_input_identity_binding_id = xr_binding
	};
	return 0;
}

static int hott_construct_object_value_action_depth(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
);

static int hott_construct_object_computation_action_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_computation_id,
	uint32_t* p_witness_computation_claim_id
);

static int hott_certify_nullary_constructor_refinement(
	struct prototype_kernel_builder* kernel,
	uint32_t substitution_id,
	uint32_t depth,
	uint32_t* p_certificate_id
) {
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_substitution* substitution =
		prototype_substitution_get(substitutions, substitution_id);
	if (!substitutions || !cwf || !terms || !types || !judgement ||
		!substitution || !p_certificate_id ||
		depth > substitutions->substitution_count) {
		return -1;
	}
	if (substitution->kind != PROTOTYPE_SUBSTITUTION_EXTEND) {
		return prototype_cwf_certificate_db_certify_substitution_root(
			cwf, substitutions, judgement, substitution_id, p_certificate_id
		);
	}
	uint32_t prefix_certificate;
	if (hott_certify_nullary_constructor_refinement(
			kernel, substitution->first, depth + 1, &prefix_certificate
		) != 0 || substitution->term >= terms->term_count) {
		return -1;
	}
	uint32_t claim_id = hott_exact_has_type_claim(
		judgement,
		substitution->source_context,
		substitution->term,
		substitution->term_classifier
	);
	if (claim_id == PROTOTYPE_INVALID_ID &&
		(terms->terms[substitution->term].tag != PROTOTYPE_TERM_CONSTRUCTOR ||
		 prototype_judgement_add_constructor_intro_claim(
			judgement,
			terms,
			types,
			substitution->source_context,
			substitution->term,
			substitution->term_classifier,
			&claim_id
		 ) != 0)) {
		return -1;
	}
	return prototype_cwf_certificate_db_add_substitution(
		cwf,
		substitutions,
		judgement,
		substitution_id,
		claim_id,
		p_certificate_id
	);
}

static int hott_construct_nullary_match_object_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
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
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	const struct prototype_term* source_match = source && source->subject <
		terms->term_count ? &terms->terms[source->subject] : NULL;
	const struct prototype_judgement_derivation* match_derivation = judgement ?
		hott_derivation_for_claim_and_kind(
			judgement, source_claim_id, PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!identity || !source || !source_match || !environment ||
		!p_left_claim_id || !p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_term_id ||
		!p_witness_claim_id || remaining_depth == 0 || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	if (source_match->tag != PROTOTYPE_TERM_MATCH || !match_derivation ||
		source_match->as.match.case_count == 0 ||
		source_match->as.match.case_count + 1 != match_derivation->premise_count) {
		return 1;
	}
	for (uint32_t i = 0; i < source_match->as.match.case_count; ++i) {
		uint32_t case_id = source_match->as.match.first_case + i;
		if (case_id >= terms->case_count || terms->cases[case_id].binder_count != 0) {
			return 1;
		}
	}

	/* The accepted motive application contains the exact scrutinee Claim. */
	uint32_t classifier_claim_id = match_derivation->premises[0].claim_id;
	const struct prototype_judgement_derivation* classifier_derivation =
		hott_derivation_for_claim_and_kind(
			judgement, classifier_claim_id, PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
		);
	if (!classifier_derivation || classifier_derivation->premise_count != 2) {
		return 1;
	}
	uint32_t scrutinee_claim_id = classifier_derivation->premises[1].claim_id;
	const struct prototype_judgement_proposition* scrutinee =
		prototype_judgement_claim_proposition(judgement, scrutinee_claim_id);
	if (!scrutinee || scrutinee->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		scrutinee->context_id != source->context_id ||
		scrutinee->subject != source_match->as.match.scrutinee) {
		return 1;
	}

	uint32_t left_claim_id;
	uint32_t right_claim_id;
	uint32_t identity_family_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement, terms, types, contexts, substitutions, source_claim_id,
			environment->left_substitution_id, &left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, terms, types, contexts, substitutions, source_claim_id,
			environment->right_substitution_id, &right_claim_id
		) != 0 || hott_instantiate_object_identity_family(
			kernel, identity, environment->target_context_id, left_claim_id,
			right_claim_id, &identity_family_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* identity_family =
		prototype_judgement_claim_proposition(
			judgement, identity_family_claim_id
		);
	if (!identity_family || identity_family->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || identity_family->context_id !=
			environment->target_context_id) {
		return -1;
	}

	uint32_t scrutinee_left_claim;
	uint32_t scrutinee_right_claim;
	uint32_t scrutinee_family_claim;
	uint32_t scrutinee_witness;
	uint32_t scrutinee_witness_claim;
	int scrutinee_status = hott_construct_object_value_action_depth(
		kernel,
		NULL,
		scrutinee_claim_id,
		environment,
		universe,
		remaining_depth - 1,
		&scrutinee_left_claim,
		&scrutinee_right_claim,
		&scrutinee_family_claim,
		&scrutinee_witness,
		&scrutinee_witness_claim
	);
	if (scrutinee_status != 0) {
		return scrutinee_status;
	}
	const struct prototype_judgement_proposition* scrutinee_family =
		prototype_judgement_claim_proposition(judgement, scrutinee_family_claim);
	if (!scrutinee_family || scrutinee_family->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || scrutinee_family->context_id !=
			environment->target_context_id || scrutinee_witness >=
			terms->term_count || terms->terms[scrutinee_witness].tag !=
			PROTOTYPE_TERM_VAR) {
		return 1;
	}

	struct prototype_type_schema_view source_schema;
	struct prototype_type_schema_view acted_schema;
	uint32_t acted_family;
	uint32_t acted_source;
	uint32_t acted_operator;
	uint32_t acted_dimension;
	size_t acted_boundary_count;
	if (prototype_type_schema_view_query(
			&types->semantic_schema,
			contexts,
			terms,
			kernel->dimension_operators,
			scrutinee->classifier,
			&source_schema
		) != 0 || source_schema.target_dimension != 0 ||
		prototype_dimension_action_family_instance_info(
			terms,
			kernel->dimension_operators,
			scrutinee_family->subject,
			&acted_family,
			&acted_source,
			&acted_operator,
			&acted_dimension,
			NULL,
			0,
			&acted_boundary_count
		) != 0 || prototype_type_schema_view_query(
			&types->semantic_schema,
			contexts,
			terms,
			kernel->dimension_operators,
			acted_family,
			&acted_schema
		) != 0 || acted_schema.target_dimension != acted_dimension ||
		acted_dimension == 0 || acted_schema.source_type_id !=
			source_schema.source_type_id || acted_source != source_schema.source_type_view) {
		return 1;
	}
	const struct prototype_type_declaration* source_type =
		source_schema.source_declaration;
	if (source_type->constructor_count != source_match->as.match.case_count ||
		source_type->constructor_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return 1;
	}

	uint32_t branch_witness_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_witness_terms[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t acted_constructor_ordinals[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t case_id = source_match->as.match.first_case + i;
		const struct prototype_judgement_proposition* branch =
			prototype_judgement_claim_proposition(
				judgement, match_derivation->premises[i + 1].claim_id
			);
		if (!branch || branch->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			branch->context_id != source->context_id || branch->subject !=
				terms->cases[case_id].body) {
			return 1;
		}
		acted_constructor_ordinals[i] = terms->cases[case_id].constructor_id;
		if (acted_constructor_ordinals[i] >= source_type->constructor_count) {
			return 1;
		}
		uint32_t branch_left_claim;
		uint32_t branch_right_claim;
		uint32_t branch_family_claim;
		int branch_status = hott_construct_object_value_action_depth(
			kernel,
			NULL,
			match_derivation->premises[i + 1].claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&branch_left_claim,
			&branch_right_claim,
			&branch_family_claim,
			&branch_witness_terms[i],
			&branch_witness_claims[i]
		);
		if (branch_status != 0) {
			return branch_status;
		}
		(void)branch_left_claim;
		(void)branch_right_claim;
		(void)branch_family_claim;
	}

	struct prototype_match_case_input cases[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		cases[i] = (struct prototype_match_case_input) {
			.case_label_symbol_id = -1,
			.constructor_owner = scrutinee_family->subject,
			.constructor_id = acted_constructor_ordinals[i],
			.binders = NULL,
			.binder_count = 0,
			.body = branch_witness_terms[i]
		};
	}
	uint32_t witness;
	if (prototype_term_match(
			terms, scrutinee_witness, cases, source_type->constructor_count,
			&witness
		) != 0) {
		return -1;
	}

	uint32_t family_is_type_claim = scrutinee_family_claim;
	uint32_t motive_binding = prototype_term_new_binding(terms);
	uint32_t motive_context;
	uint32_t motive_context_certificate;
	uint32_t motive_projection;
	uint32_t motive_body_claim;
	uint32_t motive_binder_claim;
	uint32_t motive;
	uint32_t motive_classifier;
	uint32_t motive_claim;
	if (motive_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			environment->target_context_id,
			motive_binding,
			scrutinee_family->subject,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			motive_context,
			family_is_type_claim,
			&motive_context_certificate
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			motive_context,
			environment->target_context_id,
			&motive_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			identity_family_claim_id,
			motive_projection,
			&motive_body_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			motive_context,
			motive_binding,
			scrutinee_family->subject,
			&motive_binder_claim
		) != 0 || prototype_term_lambda(
			terms, motive_binding, identity_family->subject, &motive
		) != 0 || prototype_term_pi(
			terms, scrutinee_family->subject, universe, &motive_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			environment->target_context_id,
			motive,
			motive_classifier,
			motive_binder_claim,
			motive_body_claim,
			&motive_claim
		) != 0) {
		return -1;
	}
	uint32_t match_classifier;
	uint32_t match_classifier_claim;
	if (prototype_term_app(
			terms, motive, scrutinee_witness, &match_classifier
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			environment->target_context_id,
			match_classifier,
			universe,
			motive_claim,
			scrutinee_witness_claim,
			&match_classifier_claim
		) != 0) {
		return -1;
	}

	uint32_t refined_branch_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t refinement_substitutions[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t refinement_certificates[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t refined_context;
		uint32_t constructor_term;
		if (prototype_judgement_identity_relation_branch_refinement(
				contexts,
				substitutions,
				terms,
				types,
				kernel->dimension_operators,
				environment->target_context_id,
				scrutinee_witness,
				scrutinee_family->subject,
				acted_constructor_ordinals[i],
				environment->target_context_id,
				NULL,
				0,
				&refined_context,
				&refinement_substitutions[i],
				&constructor_term
			) != 0 || hott_certify_nullary_constructor_refinement(
				kernel,
				refinement_substitutions[i],
				0,
				&refinement_certificates[i]
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				branch_witness_claims[i],
				refinement_substitutions[i],
				&refined_branch_claims[i]
			) != 0) {
			return 1;
		}
		(void)refined_context;
		(void)constructor_term;
	}
	uint32_t witness_claim;
	uint32_t converted_witness_claim;
	if (prototype_judgement_add_indexed_match_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			kernel->dimension_operators,
			kernel->occurrences,
			environment->target_context_id,
			witness,
			match_classifier,
			match_classifier_claim,
			refined_branch_claims,
			refinement_substitutions,
			source_type->constructor_count,
			&witness_claim
		) != 0 || prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			witness,
			identity_family->subject,
			witness_claim,
			&converted_witness_claim
		) != 0) {
		return -1;
	}
	*p_left_claim_id = left_claim_id;
	*p_right_claim_id = right_claim_id;
	*p_identity_family_claim_id = identity_family_claim_id;
	*p_witness_term_id = witness;
	*p_witness_claim_id = converted_witness_claim;
	(void)scrutinee_left_claim;
	(void)scrutinee_right_claim;
	(void)motive_context_certificate;
	(void)acted_operator;
	(void)acted_boundary_count;
	return 0;
}

static int hott_construct_adt_value_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
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
	const struct prototype_substitution* left = substitutions ?
		prototype_substitution_get(
			substitutions,
			environment ? environment->left_substitution_id : PROTOTYPE_INVALID_ID
		) : NULL;
	const struct prototype_substitution* right = substitutions ?
		prototype_substitution_get(
			substitutions,
			environment ? environment->right_substitution_id : PROTOTYPE_INVALID_ID
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!source || !identity || !environment || !left || !right ||
		!p_left_claim_id ||
		!p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_term_id ||
		!p_witness_claim_id || remaining_depth == 0 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		left->source_context != environment->target_context_id ||
		right->source_context != environment->target_context_id ||
		left->target_context != source->context_id ||
		right->target_context != source->context_id ||
		(environment->binding_count > 0 && (!environment->source_binding_ids ||
			!environment->identity_claim_ids))) {
		fprintf(stderr,
			"HOTT ADT action precondition failed source-claim=%u source-context=%u "
			"target-context=%u left=%u->%u right=%u->%u depth=%u\n",
			source_claim_id,
			source ? source->context_id : PROTOTYPE_INVALID_ID,
			environment ? environment->target_context_id : PROTOTYPE_INVALID_ID,
			left ? left->source_context : PROTOTYPE_INVALID_ID,
			left ? left->target_context : PROTOTYPE_INVALID_ID,
			right ? right->source_context : PROTOTYPE_INVALID_ID,
			right ? right->target_context : PROTOTYPE_INVALID_ID,
			remaining_depth);
		return -1;
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_endpoint =
		prototype_judgement_claim_proposition(judgement, left_claim_id);
	const struct prototype_judgement_proposition* right_endpoint =
		prototype_judgement_claim_proposition(judgement, right_claim_id);
	int endpoint_conversion = left_endpoint && right_endpoint ?
		prototype_judgement_classifier_conversion(
			terms,
			types,
			left_endpoint->classifier,
			right_endpoint->classifier
		).status : -1;
	if (!left_endpoint || !right_endpoint ||
		left_endpoint->context_id != environment->target_context_id ||
		right_endpoint->context_id != environment->target_context_id ||
		endpoint_conversion != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		fprintf(stderr,
			"HOTT object action endpoint validation failed source-claim=%u "
			"left-claim=%u right-claim=%u left-context=%u right-context=%u "
			"target-context=%u conversion=%d\n",
			source_claim_id, left_claim_id, right_claim_id,
			left_endpoint ? left_endpoint->context_id : PROTOTYPE_INVALID_ID,
			right_endpoint ? right_endpoint->context_id : PROTOTYPE_INVALID_ID,
			environment->target_context_id, endpoint_conversion);
		return -1;
	}
	uint32_t identity_family;
	uint32_t identity_family_claim;
	if (hott_instantiate_object_identity_family(
			kernel,
			identity,
			environment->target_context_id,
			left_claim_id,
			right_claim_id,
			&identity_family_claim
		) != 0) {
		fprintf(stderr,
			"HOTT object action family formation failed source-claim=%u\n",
			source_claim_id);
		return -1;
	}
	const struct prototype_judgement_proposition* identity_family_proposition =
		prototype_judgement_claim_proposition(judgement, identity_family_claim);
	if (!identity_family_proposition || identity_family_proposition->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	identity_family = identity_family_proposition->subject;
	if (source->subject < terms->term_count &&
		terms->terms[source->subject].tag == PROTOTYPE_TERM_VAR) {
		uint32_t source_binding = terms->terms[source->subject].as.var.binding_id;
		uint32_t mapped_identity_claim_id = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < environment->binding_count; ++i) {
			if (environment->source_binding_ids[i] == source_binding) {
				mapped_identity_claim_id = environment->identity_claim_ids[i];
				break;
			}
		}
		if (mapped_identity_claim_id == PROTOTYPE_INVALID_ID) {
			return 1;
		}
		const struct prototype_judgement_proposition* mapped =
			prototype_judgement_claim_proposition(
				judgement, mapped_identity_claim_id
			);
		uint32_t witness_claim = mapped_identity_claim_id;
		if (!mapped || mapped->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			mapped->context_id != environment->target_context_id ||
			prototype_judgement_classifier_conversion(
				terms, types, mapped->classifier, identity_family
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		if (mapped->classifier != identity_family &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				environment->target_context_id,
				mapped->subject,
				identity_family,
				mapped_identity_claim_id,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = mapped->subject;
		*p_witness_claim_id = witness_claim;
		return 0;
	}

	uint32_t source_head;
	uint32_t source_owner;
	uint32_t constructor_ordinal;
	uint32_t source_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t source_argument_count;
	if (prototype_term_constructor_spine_info(
			terms,
			source->subject,
			&source_head,
			&source_owner,
			&constructor_ordinal,
			source_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&source_argument_count
		) != 0) {
		return 1;
	}
	uint32_t acted_source;
	uint32_t operator_id;
	if (prototype_term_dimension_action_info(
			terms,
			identity->backing_type_former_term_id,
			&acted_source,
			&operator_id
		) != 0 || acted_source != source_owner ||
		source_owner != left_endpoint->classifier ||
		source_argument_count * 3 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		fprintf(stderr,
			"HOTT ADT constructor mapping failed source-claim=%u source-owner=%u "
			"endpoint-classifier=%u "
			"argument-count=%u\n",
			source_claim_id, source_owner, left_endpoint->classifier,
			source_argument_count);
		return -1;
	}
	struct prototype_type_schema_view acted_schema;
	struct prototype_constructor_schema_view constructor_schema;
	uint32_t action_classifier;
	uint32_t action_classifier_has_type_claim;
	uint32_t action_classifier_is_type_claim;
	uint32_t source_constructor_claim;
	uint32_t source_owner_claim;
	uint32_t source_owner_is_type_claim;
	uint32_t acted_owner;
	uint32_t acted_owner_claim;
	uint32_t witness;
	uint32_t witness_claim;
	int action_stage = 1;
	int action_status = prototype_type_schema_view_query(
			&types->semantic_schema,
			contexts,
			terms,
			kernel->dimension_operators,
			identity->backing_type_former_term_id,
			&acted_schema
		);
	if (action_status == 0) {
		action_stage = 2;
		action_status = prototype_constructor_schema_view_query(
			&types->semantic_schema,
			contexts,
			terms,
			kernel->dimension_operators,
			&acted_schema,
			constructor_ordinal,
			&constructor_schema
		);
	}
	if (action_status == 0) {
		action_stage = 3;
		action_status = prototype_constructor_schema_view_action_classifier(
			types,
			contexts,
			terms,
			kernel->dimension_operators,
			&acted_schema,
			constructor_ordinal,
			&action_classifier
		);
	}
	if (action_status == 0) {
		action_stage = 4;
		action_status = prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			environment->target_context_id,
			source_owner,
			universe,
			&source_owner_claim
		);
	}
	if (action_status == 0) {
		action_stage = 5;
		action_status = prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			environment->target_context_id,
			source_owner,
			universe,
			source_owner_claim,
			&source_owner_is_type_claim
		);
	}
	if (action_status == 0) {
		action_stage = 6;
		action_status = prototype_judgement_add_constructor_intro_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			source_head,
			constructor_schema.source_classifier,
			&source_constructor_claim
		);
	}
	if (action_status == 0) {
		action_stage = 7;
		action_status = hott_ensure_dimension_action_type_claim(
			kernel,
			environment->target_context_id,
			source_owner,
			universe,
			operator_id,
			&acted_owner,
			&acted_owner_claim
		);
		if (action_status == 0 && acted_owner !=
				identity->backing_type_former_term_id) {
			action_status = -1;
		}
	}
	if (action_status == 0) {
		action_stage = 8;
		action_status = hott_ensure_dimension_classifier_formation_exact(
			kernel,
			environment->target_context_id,
			action_classifier,
			universe
		);
	}
	if (action_status == 0) {
		action_stage = 9;
		action_status = prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			environment->target_context_id,
			action_classifier,
			universe,
			&action_classifier_has_type_claim
		);
	}
	if (action_status == 0) {
		action_stage = 10;
		action_status = prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			environment->target_context_id,
			action_classifier,
			universe,
			action_classifier_has_type_claim,
			&action_classifier_is_type_claim
		);
	}
	if (action_status == 0) {
		action_stage = 11;
		action_status = prototype_term_dimension_action(
			terms,
			kernel->dimension_operators,
			source_head,
			operator_id,
			&witness
		);
	}
	if (action_status == 0) {
		action_stage = 12;
		action_status = prototype_judgement_add_dimension_action_constructor(
			judgement,
			terms,
			types,
			contexts,
			kernel->dimension_operators,
			environment->target_context_id,
			witness,
			action_classifier,
			source_constructor_claim,
			source_owner_claim,
			acted_owner_claim,
			NULL,
			0,
			&witness_claim
		);
	}
	if (action_status != 0) {
		fprintf(
			stderr,
			"HOTT acted constructor construction failed source-claim=%u stage=%d "
			"status=%d\n",
			source_claim_id,
			action_stage,
			action_status
		);
		return -1;
	}
	if (source_argument_count == 0) {
		if (prototype_judgement_classifier_conversion(
				terms, types, action_classifier, identity_family
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		if (action_classifier != identity_family &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				environment->target_context_id,
				witness,
				identity_family,
				witness_claim,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = witness;
		*p_witness_claim_id = witness_claim;
		return 0;
	}
	const struct prototype_judgement_derivation* source_derivation =
		hott_constructor_derivation_for_claim(judgement, source_claim_id);
	if (!source_derivation ||
		source_derivation->premise_count != source_argument_count) {
		return 1;
	}
	for (uint32_t i = 0; i < source_argument_count; ++i) {
		uint32_t field_left_claim;
		uint32_t field_right_claim;
		uint32_t field_family_claim;
		uint32_t field_witness;
		uint32_t field_witness_claim;
		int status = hott_construct_object_value_action_depth(
			kernel,
			NULL,
			source_derivation->premises[i].claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&field_left_claim,
			&field_right_claim,
			&field_family_claim,
			&field_witness,
			&field_witness_claim
		);
		if (status != 0) {
			const struct prototype_judgement_proposition* failed_field =
				prototype_judgement_claim_proposition(
					judgement, source_derivation->premises[i].claim_id
				);
			fprintf(stderr,
				"HOTT ADT field action failed source-claim=%u field=%u "
				"field-claim=%u subject=%u subject-tag=%d classifier=%u "
				"context=%u status=%d\n",
				source_claim_id, i,
				source_derivation->premises[i].claim_id,
				failed_field ? failed_field->subject : PROTOTYPE_INVALID_ID,
				failed_field && failed_field->subject < terms->term_count ?
					terms->terms[failed_field->subject].tag : -1,
				failed_field ? failed_field->classifier : PROTOTYPE_INVALID_ID,
				failed_field ? failed_field->context_id : PROTOTYPE_INVALID_ID,
				status);
			return status;
		}
		const struct prototype_judgement_proposition* field_left =
			prototype_judgement_claim_proposition(judgement, field_left_claim);
		const struct prototype_judgement_proposition* field_right =
			prototype_judgement_claim_proposition(judgement, field_right_claim);
		if (!field_left || !field_right || hott_apply_claim_argument(
				kernel,
				environment->target_context_id,
				witness_claim,
				field_left_claim,
				&witness,
				&witness_claim
			) != 0 || hott_apply_claim_argument(
				kernel,
				environment->target_context_id,
				witness_claim,
				field_right_claim,
				&witness,
				&witness_claim
			) != 0 || hott_apply_claim_argument(
				kernel,
				environment->target_context_id,
				witness_claim,
				field_witness_claim,
				&witness,
				&witness_claim
			) != 0) {
			return -1;
		}
		(void)field_family_claim;
	}
	const struct prototype_judgement_proposition* completed_witness =
		prototype_judgement_claim_proposition(judgement, witness_claim);
	if (!completed_witness || prototype_judgement_classifier_conversion(
			terms, types, completed_witness->classifier, identity_family
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	if (completed_witness->classifier != identity_family &&
		prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			witness,
			identity_family,
			witness_claim,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_left_claim_id = left_claim_id;
	*p_right_claim_id = right_claim_id;
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)source_head;
	(void)source_owner;
	return 0;
}

static int hott_apply_pointwise_identity_witness(
	struct prototype_kernel_builder* kernel,
	uint32_t function_identity_claim_id,
	const uint32_t* argument_claim_ids,
	uint32_t argument_count,
	uint32_t universe,
	uint32_t* p_computation_term_id,
	uint32_t* p_computation_claim_id
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
	const struct prototype_judgement_proposition* function_identity = judgement ?
		prototype_judgement_claim_proposition(
			judgement, function_identity_claim_id
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!function_identity || !argument_claim_ids || argument_count == 0 ||
		argument_count > 3 || !p_computation_term_id || !p_computation_claim_id ||
		function_identity->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		function_identity->classifier >= terms->term_count ||
		terms->terms[function_identity->classifier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE || universe >= terms->term_count) {
		fprintf(
			stderr,
			"pointwise witness precondition failed claim=%u subject=%u classifier=%u/%d "
			"arguments=%u\n",
			function_identity_claim_id,
			function_identity ? function_identity->subject : PROTOTYPE_INVALID_ID,
			function_identity ? function_identity->classifier : PROTOTYPE_INVALID_ID,
			function_identity && function_identity->classifier < terms->term_count ?
				terms->terms[function_identity->classifier].tag : -1,
			argument_count
		);
		return -1;
	}
	uint32_t context_id = function_identity->context_id;
	const struct prototype_judgement_proposition* argument =
		prototype_judgement_claim_proposition(judgement, argument_claim_ids[0]);
	uint32_t pi = terms->terms[
		function_identity->classifier
	].as.thunk_type.computation;
	uint32_t forced;
	uint32_t forced_claim;
	uint32_t application;
	uint32_t application_classifier;
	uint32_t application_claim;
	uint32_t codomain_binding;
	uint32_t codomain;
	if (!argument || argument->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		argument->context_id != context_id || pi >= terms->term_count ||
		terms->terms[pi].tag != PROTOTYPE_TERM_PI ||
		prototype_term_pure_family_parts(
			terms,
			terms->terms[pi].as.pi.codomain_family,
			&codomain_binding,
			&codomain
		) != 0 || prototype_term_graph_substitute_bound_var(
			terms,
			types,
			codomain,
			codomain_binding,
			argument->subject,
			&application_classifier
		) != 0 || prototype_term_force(
			terms, function_identity->subject, &forced
		) != 0 || prototype_judgement_add_force_claim(
			judgement,
			terms,
			context_id,
			forced,
			pi,
			function_identity_claim_id,
			&forced_claim
		) != 0 || prototype_term_app(
			terms, forced, argument->subject, &application
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			context_id,
			application,
			application_classifier,
			forced_claim,
			argument_claim_ids[0],
			&application_claim
		) != 0) {
		fprintf(stderr, "pointwise witness first application failed\n");
		return -1;
	}
	if (argument_count == 1) {
		*p_computation_term_id = application;
		*p_computation_claim_id = application_claim;
		return 0;
	}
	if (application_classifier >= terms->term_count ||
		terms->terms[application_classifier].tag !=
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		fprintf(stderr, "pointwise witness first result is not a computation\n");
		return -1;
	}
	uint32_t next_value_type = terms->terms[
		application_classifier
	].as.computation_type.result;
	uint32_t next_value_type_claim;
	uint32_t continuation_binding = prototype_term_new_binding(terms);
	uint32_t continuation_context;
	uint32_t continuation_context_certificate;
	int next_type_status = hott_ensure_is_type_claim_in_context(
		judgement,
		terms,
		types,
		contexts,
		substitutions,
		context_id,
		next_value_type,
		&next_value_type_claim
	);
	if (next_type_status == 1) {
		uint32_t next_value_type_formation_claim;
		int formation_stage = 1;
		int formation_status = prototype_hott_ensure_dimension_classifier_formation(
			kernel, context_id, next_value_type, universe
		);
		if (formation_status == 0) {
			formation_stage = 2;
			formation_status = prototype_judgement_add_structural_type_formation_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				context_id,
				next_value_type,
				universe,
				&next_value_type_formation_claim
			);
		}
		if (formation_status == 0) {
			formation_stage = 3;
			formation_status = prototype_judgement_add_is_type_claim(
				judgement,
				terms,
				context_id,
				next_value_type,
				universe,
				next_value_type_formation_claim,
				&next_value_type_claim
			);
		}
		if (formation_status != 0) {
			fprintf(
				stderr,
				"pointwise witness next type formation failed type=%u context=%u "
				"stage=%d status=%d\n",
				next_value_type,
				context_id,
				formation_stage,
				formation_status
			);
			prototype_term_print_debug(
				stderr, NULL, NULL, types, terms, next_value_type
			);
			fputc('\n', stderr);
			uint32_t trace_term = next_value_type;
			uint32_t trace_context = context_id;
			for (size_t trace_depth = 0; trace_depth < 16 && trace_term <
					terms->term_count; ++trace_depth) {
				fprintf(
					stderr,
					"  type spine depth=%zu term=%u tag=%d context=%u has=%u exact=%u "
					"is=%u\n",
					trace_depth,
					trace_term,
					terms->terms[trace_term].tag,
					trace_context,
					hott_has_type_claim_for_subject(
						judgement, trace_context, trace_term
					),
					hott_exact_has_type_claim(
						judgement, trace_context, trace_term, universe
					),
					hott_is_type_claim_for_subject(
						judgement, trace_context, trace_term
					)
				);
				if (terms->terms[trace_term].tag == PROTOTYPE_TERM_THUNK_TYPE) {
					trace_term = terms->terms[trace_term].as.thunk_type.computation;
					continue;
				}
				if (terms->terms[trace_term].tag ==
						PROTOTYPE_TERM_COMPUTATION_TYPE) {
					trace_term = terms->terms[trace_term].as.computation_type.result;
					continue;
				}
				if (terms->terms[trace_term].tag == PROTOTYPE_TERM_PI) {
					uint32_t trace_binder;
					uint32_t trace_body;
					uint32_t trace_next_context;
					uint32_t trace_domain = terms->terms[
						trace_term
					].as.pi.domain;
					fprintf(
						stderr,
						"  Pi domain=%u tag=%d has=%u exact=%u is=%u\n",
						trace_domain,
						terms->terms[trace_domain].tag,
						hott_has_type_claim_for_subject(
							judgement, trace_context, trace_domain
						),
						hott_exact_has_type_claim(
							judgement, trace_context, trace_domain, universe
						),
						hott_is_type_claim_for_subject(
							judgement, trace_context, trace_domain
						)
					);
					uint32_t trace_domain_is = hott_is_type_claim_for_subject(
						judgement, trace_context, trace_domain
					);
					if (trace_domain_is != PROTOTYPE_INVALID_ID) {
						uint32_t trace_domain_derivations[8];
						size_t trace_domain_derivation_count;
						if (prototype_judgement_claim_derivations(
								judgement,
								trace_domain_is,
								trace_domain_derivations,
								8,
								&trace_domain_derivation_count
							) == 0) {
							for (size_t i = 0; i < trace_domain_derivation_count; ++i) {
								const struct prototype_judgement_derivation* derivation =
									prototype_judgement_derivation_get(
										judgement, trace_domain_derivations[i]
									);
								if (derivation && derivation->premise_count != 0) {
									const struct prototype_judgement_proposition* premise =
										prototype_judgement_premise_proposition(
											judgement, &derivation->premises[0]
										);
									fprintf(
										stderr,
										"  domain IS derivation=%u rule=%d premise=%u kind=%d "
										"context=%u subject=%u classifier=%u\n",
										trace_domain_derivations[i],
										derivation->proof_kind,
										derivation->premises[0].claim_id,
										premise ? premise->kind : -1,
										premise ? premise->context_id : PROTOTYPE_INVALID_ID,
										premise ? premise->subject : PROTOTYPE_INVALID_ID,
										premise ? premise->classifier : PROTOTYPE_INVALID_ID
									);
								}
							}
						}
					}
					if (prototype_term_pure_family_parts(
							terms,
							terms->terms[trace_term].as.pi.codomain_family,
							&trace_binder,
							&trace_body
						) != 0 || prototype_context_extend(
							contexts,
							trace_context,
							trace_binder,
							terms->terms[trace_term].as.pi.domain,
							PROTOTYPE_INVALID_ID,
							&trace_next_context
						) != 0) {
						break;
					}
					trace_context = trace_next_context;
					trace_term = trace_body;
					continue;
				}
				uint32_t trace_is_claim = hott_is_type_claim_for_subject(
					judgement, trace_context, trace_term
				);
				if (trace_is_claim != PROTOTYPE_INVALID_ID) {
					uint32_t trace_derivations[8];
					size_t trace_derivation_count;
					if (prototype_judgement_claim_derivations(
							judgement,
							trace_is_claim,
							trace_derivations,
							8,
							&trace_derivation_count
						) == 0) {
						for (size_t i = 0; i < trace_derivation_count; ++i) {
							const struct prototype_judgement_derivation* derivation =
								prototype_judgement_derivation_get(
									judgement, trace_derivations[i]
								);
							if (derivation && derivation->premise_count != 0) {
								const struct prototype_judgement_proposition* premise =
									prototype_judgement_premise_proposition(
										judgement, &derivation->premises[0]
									);
								fprintf(
									stderr,
									"  IS_TYPE derivation=%u rule=%d premise-claim=%u "
									"kind=%d context=%u subject=%u classifier=%u\n",
									trace_derivations[i],
									derivation->proof_kind,
									derivation->premises[0].claim_id,
									premise ? premise->kind : -1,
									premise ? premise->context_id : PROTOTYPE_INVALID_ID,
									premise ? premise->subject : PROTOTYPE_INVALID_ID,
									premise ? premise->classifier : PROTOTYPE_INVALID_ID
								);
							}
						}
					}
				}
				uint32_t trace_head = trace_term;
				for (size_t app_depth = 0; app_depth < 16 && trace_head <
						terms->term_count && terms->terms[trace_head].tag ==
						PROTOTYPE_TERM_APP; ++app_depth) {
					fprintf(
						stderr,
						"  APP spine depth=%zu term=%u function=%u argument=%u "
						"argument-tag=%d\n",
						app_depth,
						trace_head,
						terms->terms[trace_head].as.app.function,
						terms->terms[trace_head].as.app.argument,
						terms->terms[terms->terms[trace_head].as.app.argument].tag
					);
					trace_head = terms->terms[trace_head].as.app.function;
				}
				if (trace_head < terms->term_count) {
					fprintf(
						stderr,
						"  APP head=%u tag=%d\n",
						trace_head,
						terms->terms[trace_head].tag
					);
				}
				break;
			}
			return -1;
		}
		next_type_status = 0;
	}
	/* Generated pointwise identity codomains become ordinary object types
	 * before they are introduced into the continuation Context. */
	if (continuation_binding == PROTOTYPE_INVALID_ID || next_type_status != 0 ||
		prototype_context_extend(
			contexts,
			context_id,
			continuation_binding,
			next_value_type,
			PROTOTYPE_INVALID_ID,
			&continuation_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			continuation_context,
			next_value_type_claim,
			&continuation_context_certificate
		) != 0) {
		fprintf(
			stderr,
			"pointwise witness continuation context failed next-type=%u status=%d\n",
			next_value_type,
			next_type_status
		);
		return -1;
	}
	uint32_t continuation_value;
	uint32_t continuation_value_claim;
	uint32_t continuation_projection;
	uint32_t mapped_arguments[3];
	if (prototype_term_var(
			terms, continuation_binding, &continuation_value
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			continuation_context,
			continuation_binding,
			next_value_type,
			&continuation_value_claim
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			continuation_context,
			context_id,
			&continuation_projection
		) != 0) {
		fprintf(stderr, "pointwise witness continuation variable failed\n");
		return -1;
	}
	for (uint32_t i = 1; i < argument_count; ++i) {
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				argument_claim_ids[i],
				continuation_projection,
				&mapped_arguments[i - 1]
			) != 0) {
			fprintf(stderr, "pointwise witness argument reindex failed index=%u\n", i);
			return -1;
		}
	}
	uint32_t continuation_body;
	uint32_t continuation_body_claim;
	if (hott_apply_pointwise_identity_witness(
			kernel,
			continuation_value_claim,
			mapped_arguments,
			argument_count - 1,
			universe,
			&continuation_body,
			&continuation_body_claim
		) != 0) {
		fprintf(stderr, "pointwise witness continuation application failed\n");
		return -1;
	}
	const struct prototype_judgement_proposition* continuation_body_evidence =
		prototype_judgement_claim_proposition(
			judgement, continuation_body_claim
		);
	uint32_t continuation_family;
	uint32_t continuation_classifier;
	uint32_t continuation;
	uint32_t continuation_claim;
	uint32_t fold;
	uint32_t fold_classifier;
	uint32_t fold_claim;
	uint32_t fold_premises[2] = { application_claim, PROTOTYPE_INVALID_ID };
	if (!continuation_body_evidence ||
		continuation_body_evidence->context_id != continuation_context ||
		prototype_term_pure_family(
			terms,
			continuation_binding,
			continuation_body_evidence->classifier,
			&continuation_family
		) != 0 || prototype_term_pi_family(
			terms,
			next_value_type,
			continuation_family,
			&continuation_classifier
		) != 0 || prototype_term_lambda(
			terms, continuation_binding, continuation_body, &continuation
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			context_id,
			continuation,
			continuation_classifier,
			continuation_value_claim,
			continuation_body_claim,
			&continuation_claim
		) != 0 || prototype_term_computation_fold(
			terms, application, continuation, NULL, 0, &fold
		) != 0 || prototype_judgement_computation_fold_result_classifier(
			terms,
			types,
			application,
			application_classifier,
			continuation_classifier,
			&fold_classifier
		) != 0) {
		fprintf(stderr, "pointwise witness continuation construction failed\n");
		return -1;
	}
	fold_premises[1] = continuation_claim;
	if (prototype_judgement_add_computation_fold_claim(
			judgement,
			terms,
			types,
			context_id,
			fold,
			fold_classifier,
			fold_premises,
			2,
			&fold_claim
		) != 0) {
		fprintf(stderr, "pointwise witness fold typing failed\n");
		return -1;
	}
	*p_computation_term_id = fold;
	*p_computation_claim_id = fold_claim;
	(void)continuation_context_certificate;
	return 0;
}


static int hott_construct_object_app_action(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_computation_id,
	uint32_t* p_witness_computation_claim_id
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
	const struct prototype_judgement_derivation* app_derivation = judgement ?
		hott_derivation_for_claim_and_kind(
			judgement, source_claim_id, PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
		) : NULL;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!source || !environment || !app_derivation || remaining_depth == 0 ||
		!p_left_claim_id || !p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_computation_id ||
		!p_witness_computation_claim_id || app_derivation->premise_count != 2 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->subject >= terms->term_count ||
		terms->terms[source->subject].tag != PROTOTYPE_TERM_APP ||
		source->classifier >= terms->term_count ||
		terms->terms[source->classifier].tag !=
			PROTOTYPE_TERM_COMPUTATION_TYPE) {
		return -1;
	}
	uint32_t function_claim_id = app_derivation->premises[0].claim_id;
	const struct prototype_judgement_proposition* function =
		prototype_judgement_claim_proposition(judgement, function_claim_id);
	const struct prototype_judgement_derivation* force_derivation = function ?
		hott_derivation_for_claim_and_kind(
			judgement, function_claim_id, PROTOTYPE_JUDGEMENT_PROOF_FORCE_ELIM
		) : NULL;
	if (!function || !force_derivation || force_derivation->premise_count != 1 ||
		function->subject != terms->terms[source->subject].as.app.function ||
		function->subject >= terms->term_count ||
		terms->terms[function->subject].tag != PROTOTYPE_TERM_FORCE) {
		return 1;
	}
	uint32_t function_value_claim_id = force_derivation->premises[0].claim_id;
	uint32_t argument_claim_id = app_derivation->premises[1].claim_id;
	uint32_t function_left_claim;
	uint32_t function_right_claim;
	uint32_t function_family_claim;
	uint32_t function_witness;
	uint32_t function_witness_claim;
	uint32_t argument_left_claim;
	uint32_t argument_right_claim;
	uint32_t argument_family_claim;
	uint32_t argument_witness;
	uint32_t argument_witness_claim;
	int function_status = hott_construct_object_value_action_depth(
		kernel,
		NULL,
		function_value_claim_id,
		environment,
		universe,
		remaining_depth - 1,
		&function_left_claim,
		&function_right_claim,
		&function_family_claim,
		&function_witness,
		&function_witness_claim
	);
	int argument_status = function_status == 0 ?
		hott_construct_object_value_action_depth(
			kernel,
			NULL,
			argument_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&argument_left_claim,
			&argument_right_claim,
			&argument_family_claim,
			&argument_witness,
			&argument_witness_claim
		) : function_status;
	if (function_status != 0 || argument_status != 0) {
		fprintf(
			stderr,
			"HOTT computation APP child action failed function=%d argument=%d\n",
			function_status,
			argument_status
		);
		return function_status != 0 ? function_status : argument_status;
	}
	uint32_t application_arguments[3] = {
		argument_left_claim,
		argument_right_claim,
		argument_witness_claim
	};
	uint32_t witness_computation;
	uint32_t witness_computation_claim;
	if (hott_apply_pointwise_identity_witness(
			kernel,
			function_witness_claim,
			application_arguments,
			3,
			universe,
			&witness_computation,
			&witness_computation_claim
		) != 0) {
		fprintf(stderr, "HOTT APP pointwise witness application failed\n");
		return -1;
	}
	uint32_t left_claim;
	uint32_t right_claim;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim
		) != 0) {
		fprintf(stderr, "HOTT APP endpoint reindex failed\n");
		return -1;
	}
	const struct prototype_judgement_proposition* left =
		prototype_judgement_claim_proposition(judgement, left_claim);
	const struct prototype_judgement_proposition* right =
		prototype_judgement_claim_proposition(judgement, right_claim);
	uint32_t computation_thunk_type;
	uint32_t left_thunk;
	uint32_t right_thunk;
	uint32_t left_thunk_claim;
	uint32_t right_thunk_claim;
	uint32_t identity_family;
	uint32_t identity_family_claim;
	if (!left || !right || prototype_term_thunk_type(
			terms, source->classifier, &computation_thunk_type
		) != 0 || prototype_term_thunk(
			terms, left->subject, &left_thunk
		) != 0 || prototype_term_thunk(
			terms, right->subject, &right_thunk
		) != 0 || prototype_judgement_add_thunk_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			left_thunk,
			computation_thunk_type,
			left_claim,
			&left_thunk_claim
		) != 0 || prototype_judgement_add_thunk_claim(
			judgement,
			terms,
			types,
			environment->target_context_id,
			right_thunk,
			computation_thunk_type,
			right_claim,
			&right_thunk_claim
		) != 0) {
		fprintf(stderr, "HOTT APP endpoint thunk typing failed\n");
		return -1;
	}
	const struct prototype_judgement_proposition* witness_evidence =
		prototype_judgement_claim_proposition(
			judgement, witness_computation_claim
		);
	uint32_t result_action_family;
	uint32_t result_action_source;
	uint32_t result_action_operator;
	uint32_t result_action_dimension;
	uint32_t result_boundaries[2];
	size_t result_boundary_count;
	uint32_t acted_thunk_type;
	uint32_t acted_thunk_type_claim;
	uint32_t acted_at_left;
	uint32_t acted_at_left_claim;
	if (!witness_evidence || witness_evidence->classifier >= terms->term_count ||
		terms->terms[witness_evidence->classifier].tag !=
			PROTOTYPE_TERM_COMPUTATION_TYPE || prototype_term_effect_row_purity(
			terms,
			terms->terms[witness_evidence->classifier].as.computation_type.label
		) != PROTOTYPE_EFFECT_ROW_PURITY_PURE) {
		return -1;
	}
	identity_family = terms->terms[
		witness_evidence->classifier
	].as.computation_type.result;
	if (prototype_dimension_action_family_instance_info(
			terms,
			kernel->dimension_operators,
			identity_family,
			&result_action_family,
			&result_action_source,
			&result_action_operator,
			&result_action_dimension,
			result_boundaries,
			2,
			&result_boundary_count
		) != 0 || result_action_source != computation_thunk_type ||
		result_action_dimension != 1 || result_boundary_count != 2 ||
		result_boundaries[0] != left_thunk || result_boundaries[1] != right_thunk ||
		hott_ensure_dimension_action_type_claim(
			kernel,
			environment->target_context_id,
			computation_thunk_type,
			universe,
			result_action_operator,
			&acted_thunk_type,
			&acted_thunk_type_claim
		) != 0 || acted_thunk_type != result_action_family ||
		hott_apply_claim_argument(
			kernel,
			environment->target_context_id,
			acted_thunk_type_claim,
			left_thunk_claim,
			&acted_at_left,
			&acted_at_left_claim
		) != 0 || hott_apply_claim_argument(
			kernel,
			environment->target_context_id,
			acted_at_left_claim,
			right_thunk_claim,
			&identity_family,
			&identity_family_claim
		) != 0 || identity_family != terms->terms[
			witness_evidence->classifier
		].as.computation_type.result) {
		fprintf(
			stderr,
			"HOTT APP result dimension action reconstruction failed family=%u source=%u "
			"dimension=%u boundaries=%zu\n",
			identity_family,
			result_action_source,
			result_action_dimension,
			result_boundary_count
		);
		return -1;
	}
	*p_left_claim_id = left_claim;
	*p_right_claim_id = right_claim;
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_computation_id = witness_computation;
	*p_witness_computation_claim_id = witness_computation_claim;
	(void)function_left_claim;
	(void)function_right_claim;
	(void)function_family_claim;
	(void)function_witness;
	(void)argument_family_claim;
	(void)argument_witness;
	(void)result_action_family;
	return 0;
}

static int hott_construct_object_computation_action_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_computation_id,
	uint32_t* p_witness_computation_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	if (!terms || !source || !environment || remaining_depth == 0 ||
		source->subject >= terms->term_count) {
		return -1;
	}
	if (terms->terms[source->subject].tag == PROTOTYPE_TERM_APP) {
		return hott_construct_object_app_action(
			kernel,
			source_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			p_left_claim_id,
			p_right_claim_id,
			p_identity_family_claim_id,
			p_witness_computation_id,
			p_witness_computation_claim_id
		);
	}
	return 1;
}

static int hott_construct_identity_lambda_action(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* outer_environment,
	uint32_t universe,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
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
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	int source_is_computation_function = terms && source &&
		source->subject < terms->term_count &&
		terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK;
	int source_is_type_function = terms && source &&
		source->subject < terms->term_count &&
		terms->terms[source->subject].tag == PROTOTYPE_TERM_LAMBDA;
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!identity || !source || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id ||
		identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->subject >= terms->term_count ||
		source->classifier >= terms->term_count || universe >= terms->term_count ||
		(!source_is_computation_function && !source_is_type_function) ||
		(source_is_computation_function && terms->terms[source->classifier].tag !=
			PROTOTYPE_TERM_THUNK_TYPE) ||
		(source_is_type_function && terms->terms[source->classifier].tag !=
			PROTOTYPE_TERM_PI)) {
		return -1;
	}
	uint32_t target_context_id = outer_environment ?
		outer_environment->target_context_id : source->context_id;
	uint32_t left_source_claim_id = source_claim_id;
	uint32_t right_source_claim_id = source_claim_id;
	if (outer_environment && (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			outer_environment->left_substitution_id,
			&left_source_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			outer_environment->right_substitution_id,
			&right_source_claim_id
		) != 0)) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_source =
		prototype_judgement_claim_proposition(judgement, left_source_claim_id);
	const struct prototype_judgement_proposition* right_source =
		prototype_judgement_claim_proposition(judgement, right_source_claim_id);
	if (!left_source || !right_source || left_source->context_id !=
			target_context_id || right_source->context_id != target_context_id) {
		return -1;
	}

	/* The structural cases below are checked through the accepted Derivation
	 * DAG; the alpha-interned Core Lambda does not identify a typed occurrence. */
	uint32_t lambda_id = source_is_computation_function ?
		terms->terms[source->subject].as.thunk.computation : source->subject;
	if (lambda_id >= terms->term_count ||
		terms->terms[lambda_id].tag != PROTOTYPE_TERM_LAMBDA) {
		return 1;
	}
	const struct prototype_judgement_derivation* thunk_derivation =
		source_is_computation_function ? hott_derivation_for_claim_and_kind(
			judgement, source_claim_id, PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO
		) : NULL;
	uint32_t lambda_claim_id = source_is_computation_function ?
		(thunk_derivation && thunk_derivation->premise_count == 1 ?
			thunk_derivation->premises[0].claim_id : PROTOTYPE_INVALID_ID) :
		source_claim_id;
	const struct prototype_judgement_proposition* lambda_claim =
		prototype_judgement_claim_proposition(judgement, lambda_claim_id);
	const struct prototype_judgement_derivation* lambda_derivation =
		hott_derivation_for_claim_and_kind(
			judgement, lambda_claim_id, PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO
		);
	if (!lambda_claim || lambda_claim->subject != lambda_id ||
		!lambda_derivation || lambda_derivation->premise_count != 2) {
		return 1;
	}
	uint32_t body_claim_id = lambda_derivation->premises[1].claim_id;
	uint32_t binder_claim_id = lambda_derivation->premises[0].claim_id;
	const struct prototype_judgement_proposition* body_claim =
		prototype_judgement_claim_proposition(judgement, body_claim_id);
	const struct prototype_judgement_proposition* binder_claim =
		prototype_judgement_claim_proposition(judgement, binder_claim_id);
	uint32_t body_id = body_claim ? body_claim->subject : PROTOTYPE_INVALID_ID;
	int body_is_return = source_is_computation_function &&
		body_id < terms->term_count &&
		terms->terms[body_id].tag == PROTOTYPE_TERM_RETURN;
	int body_is_computation = source_is_computation_function &&
		body_id < terms->term_count &&
		terms->terms[body_id].tag == PROTOTYPE_TERM_APP;
	int body_is_value = source_is_type_function && body_id < terms->term_count;
	if (!body_claim || !binder_claim || body_id >= terms->term_count ||
		(!body_is_return && !body_is_computation && !body_is_value)) {
		return 1;
	}
	const struct prototype_judgement_derivation* return_derivation =
		body_is_return ? hott_derivation_for_claim_and_kind(
			judgement, body_claim_id, PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO
		) : NULL;
	uint32_t source_value = body_is_return ?
		terms->terms[body_id].as.return_term.value :
		(body_is_value ? body_id : PROTOTYPE_INVALID_ID);
	uint32_t source_value_claim_id = body_is_value ? body_claim_id :
		(return_derivation && return_derivation->premise_count == 1 ?
			return_derivation->premises[0].claim_id : PROTOTYPE_INVALID_ID);
	const struct prototype_judgement_proposition* source_value_claim =
		prototype_judgement_claim_proposition(judgement, source_value_claim_id);
	uint32_t source_binding = binder_claim->subject < terms->term_count &&
		terms->terms[binder_claim->subject].tag == PROTOTYPE_TERM_VAR ?
		terms->terms[binder_claim->subject].as.var.binding_id :
		PROTOTYPE_INVALID_ID;
	if (source_binding == PROTOTYPE_INVALID_ID || (body_is_return &&
		(!return_derivation || return_derivation->premise_count != 1 ||
		 !source_value_claim || source_value_claim->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		 source_value_claim->subject != source_value ||
		 source_value_claim->context_id != body_claim->context_id))) {
		return 1;
	}

	uint32_t concrete_identity;
	if (prototype_term_graph_substitute_bound_var(
			terms,
			types,
			identity->identity_type_term_id,
			identity->left_endpoint_binding_id,
			left_source->subject,
			&concrete_identity
		) != 0 || prototype_term_graph_substitute_bound_var(
			terms,
			types,
			concrete_identity,
			identity->right_endpoint_binding_id,
			right_source->subject,
			&concrete_identity
		) != 0 || concrete_identity >= terms->term_count ||
		(source_is_computation_function ?
			terms->terms[concrete_identity].tag != PROTOTYPE_TERM_THUNK_TYPE :
			terms->terms[concrete_identity].tag != PROTOTYPE_TERM_PI)) {
		return -1;
	}
	uint32_t pi[3];
	uint32_t codomain[3];
	uint32_t thunk_type[3];
	uint32_t binders[3];
	thunk_type[0] = concrete_identity;
	for (uint32_t i = 0; i < 3; ++i) {
		pi[i] = source_is_computation_function ?
			terms->terms[thunk_type[i]].as.thunk_type.computation : thunk_type[i];
		uint32_t family_binding;
		if (pi[i] >= terms->term_count ||
			terms->terms[pi[i]].tag != PROTOTYPE_TERM_PI ||
			prototype_term_pure_family_parts(
				terms,
				terms->terms[pi[i]].as.pi.codomain_family,
				&family_binding,
				&codomain[i]
			) != 0 ||
			codomain[i] >= terms->term_count ||
			(source_is_computation_function && terms->terms[codomain[i]].tag !=
				PROTOTYPE_TERM_COMPUTATION_TYPE)) {
			return -1;
		}
		binders[i] = family_binding;
		if (i < 2) {
			thunk_type[i + 1] = source_is_computation_function ?
				terms->terms[codomain[i]].as.computation_type.result : codomain[i];
			if (thunk_type[i + 1] >= terms->term_count ||
				(source_is_computation_function &&
				 terms->terms[thunk_type[i + 1]].tag !=
					PROTOTYPE_TERM_THUNK_TYPE)) {
				return -1;
			}
		}
	}
	uint32_t domain = terms->terms[pi[0]].as.pi.domain;
	if (terms->terms[pi[1]].as.pi.domain != domain) {
		return -1;
	}
	uint32_t input_identity = terms->terms[pi[2]].as.pi.domain;
	uint32_t result_identity = source_is_computation_function ?
		terms->terms[codomain[2]].as.computation_type.result : codomain[2];
	uint32_t x0;
	uint32_t x1;
	uint32_t xr;
	if (prototype_term_var(terms, binders[0], &x0) != 0 ||
		prototype_term_var(terms, binders[1], &x1) != 0 ||
		prototype_term_var(terms, binders[2], &xr) != 0) {
		return -1;
	}

	uint32_t domain_type_claim;
	if (hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			target_context_id,
			domain,
			&domain_type_claim
		) != 0) {
		return -1;
	}
	uint32_t x0_context;
	uint32_t x0_context_certificate;
	if (prototype_context_extend(
			contexts,
			target_context_id,
			binders[0],
			domain,
			PROTOTYPE_INVALID_ID,
			&x0_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			x0_context,
			domain_type_claim,
			&x0_context_certificate
		) != 0) {
		return -1;
	}
	uint32_t x0_to_source;
	uint32_t domain_type_in_x0;
	uint32_t x1_context;
	uint32_t x1_context_certificate;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			x0_context,
			target_context_id,
			&x0_to_source
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			domain_type_claim,
			x0_to_source,
			&domain_type_in_x0
		) != 0 || prototype_context_extend(
			contexts,
			x0_context,
			binders[1],
			domain,
			PROTOTYPE_INVALID_ID,
			&x1_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			x1_context,
			domain_type_in_x0,
			&x1_context_certificate
		) != 0) {
		return -1;
	}
	uint32_t input_identity_has_type_claim;
	uint32_t input_identity_is_type_claim;
	uint32_t xr_context;
	uint32_t xr_context_certificate;
	uint32_t action_at_left = input_identity < terms->term_count &&
		terms->terms[input_identity].tag == PROTOTYPE_TERM_APP ?
		terms->terms[input_identity].as.app.function : PROTOTYPE_INVALID_ID;
	uint32_t action_head = action_at_left < terms->term_count &&
		terms->terms[action_at_left].tag == PROTOTYPE_TERM_APP ?
		terms->terms[action_at_left].as.app.function : PROTOTYPE_INVALID_ID;
	uint32_t action_source;
	uint32_t action_operator_id;
	uint32_t x0_claim_in_x1;
	uint32_t x1_claim_in_x1;
	uint32_t acted_type;
	uint32_t acted_type_claim;
	uint32_t acted_at_left;
	uint32_t acted_at_left_claim;
	uint32_t acted_at_endpoints;
	if (action_at_left >= terms->term_count || action_head >= terms->term_count ||
		terms->terms[action_at_left].as.app.argument != x0 ||
		terms->terms[input_identity].as.app.argument != x1 ||
		prototype_term_dimension_action_info(
			terms, action_head, &action_source, &action_operator_id
		) != 0 || action_source != domain ||
		prototype_judgement_add_context_binding_assumption(
			judgement, terms, contexts, x1_context, binders[0], domain,
			&x0_claim_in_x1
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement, terms, contexts, x1_context, binders[1], domain,
			&x1_claim_in_x1
		) != 0 || hott_ensure_dimension_action_type_claim(
			kernel,
			x1_context,
			action_source,
			universe,
			action_operator_id,
			&acted_type,
			&acted_type_claim
		) != 0 || acted_type != action_head || hott_apply_claim_argument(
			kernel,
			x1_context,
			acted_type_claim,
			x0_claim_in_x1,
			&acted_at_left,
			&acted_at_left_claim
		) != 0 || acted_at_left != action_at_left || hott_apply_claim_argument(
			kernel,
			x1_context,
			acted_at_left_claim,
			x1_claim_in_x1,
			&acted_at_endpoints,
			&input_identity_has_type_claim
		) != 0 || acted_at_endpoints != input_identity ||
		prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			x1_context,
			input_identity,
			universe,
			input_identity_has_type_claim,
			&input_identity_is_type_claim
		) != 0 || prototype_context_extend(
			contexts,
			x1_context,
			binders[2],
			input_identity,
			PROTOTYPE_INVALID_ID,
			&xr_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			xr_context,
			input_identity_is_type_claim,
			&xr_context_certificate
		) != 0) {
		return -1;
	}

	uint32_t x0_claim;
	uint32_t x1_claim;
	uint32_t xr_claim;
	if (prototype_judgement_add_context_binding_assumption(
			judgement, terms, contexts, xr_context, binders[0], domain, &x0_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement, terms, contexts, xr_context, binders[1], domain, &x1_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			xr_context,
			binders[2],
			input_identity,
			&xr_claim
		) != 0) {
		return -1;
	}
	const struct prototype_context* source_body_context = prototype_context_get(
		contexts, body_claim->context_id
	);
	uint32_t body_target_projection;
	uint32_t left_body_base_substitution;
	uint32_t right_body_base_substitution;
	uint32_t left_body_substitution;
	uint32_t right_body_substitution;
	uint32_t left_body_substitution_certificate;
	uint32_t right_body_substitution_certificate;
	if (!source_body_context || source_body_context->parent != source->context_id ||
		source_body_context->binding_id != source_binding ||
		prototype_substitution_projection_path(
			substitutions,
			contexts,
			xr_context,
			target_context_id,
			&body_target_projection
		) != 0 || (outer_environment ? prototype_substitution_compose(
			substitutions,
			contexts,
			outer_environment->left_substitution_id,
			body_target_projection,
			&left_body_base_substitution
		) : prototype_substitution_projection_path(
			substitutions,
			contexts,
			xr_context,
			source->context_id,
			&left_body_base_substitution
		)) != 0 || (outer_environment ? prototype_substitution_compose(
			substitutions,
			contexts,
			outer_environment->right_substitution_id,
			body_target_projection,
			&right_body_base_substitution
		) : prototype_substitution_projection_path(
			substitutions,
			contexts,
			xr_context,
			source->context_id,
			&right_body_base_substitution
		)) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			left_body_base_substitution,
			body_claim->context_id,
			x0,
			domain,
			&left_body_substitution
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			left_body_substitution,
			x0_claim,
			&left_body_substitution_certificate
		) != 0 || prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			types,
			right_body_base_substitution,
			body_claim->context_id,
			x1,
			domain,
			&right_body_substitution
		) != 0 || prototype_cwf_certificate_db_add_substitution(
			cwf,
			substitutions,
			judgement,
			right_body_substitution,
			x1_claim,
			&right_body_substitution_certificate
		) != 0) {
		return -1;
	}

	uint32_t body_binding_count = outer_environment ?
		outer_environment->binding_count + 1 : 1;
	uint32_t* mapped_bindings = malloc(
		body_binding_count * sizeof(*mapped_bindings)
	);
	uint32_t* mapped_identity_claims = malloc(
		body_binding_count * sizeof(*mapped_identity_claims)
	);
	if (!mapped_bindings || !mapped_identity_claims) {
		free(mapped_bindings);
		free(mapped_identity_claims);
		return -1;
	}
	uint32_t outer_binding_count = outer_environment ?
		outer_environment->binding_count : 0;
	for (uint32_t i = 0; i < outer_binding_count; ++i) {
		mapped_bindings[i] = outer_environment->source_binding_ids[i];
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				outer_environment->identity_claim_ids[i],
				body_target_projection,
				&mapped_identity_claims[i]
			) != 0) {
			free(mapped_bindings);
			free(mapped_identity_claims);
			return -1;
		}
	}
	mapped_bindings[outer_binding_count] = source_binding;
	mapped_identity_claims[outer_binding_count] = xr_claim;
	struct hott_object_action_environment body_environment = {
		.left_substitution_id = left_body_substitution,
		.right_substitution_id = right_body_substitution,
		.target_context_id = xr_context,
		.source_binding_ids = mapped_bindings,
		.identity_claim_ids = mapped_identity_claims,
		.binding_count = body_binding_count
	};
	uint32_t current_term = PROTOTYPE_INVALID_ID;
	uint32_t current_claim = PROTOTYPE_INVALID_ID;
	int current_is_computation = 0;
	if (body_is_return || body_is_value) {
		uint32_t left_value_claim;
		uint32_t right_value_claim;
		uint32_t value_identity_family_claim;
		uint32_t value_witness;
		uint32_t value_witness_claim;
		int value_action_status = hott_construct_object_value_action_depth(
			kernel,
			NULL,
			source_value_claim_id,
			&body_environment,
			universe,
			(uint32_t)terms->term_count + 1,
			&left_value_claim,
			&right_value_claim,
			&value_identity_family_claim,
			&value_witness,
			&value_witness_claim
		);
		free(mapped_bindings);
		free(mapped_identity_claims);
		if (value_action_status != 0) {
			fprintf(stderr,
				"HOTT Lambda body value action failed source-claim=%u body-claim=%u "
				"value-claim=%u status=%d\n",
				source_claim_id,
				body_claim_id,
				source_value_claim_id,
				value_action_status);
			return value_action_status;
		}
		const struct prototype_judgement_proposition* value_identity_family =
			prototype_judgement_claim_proposition(
				judgement, value_identity_family_claim
			);
		const struct prototype_judgement_proposition* value_witness_proposition =
			prototype_judgement_claim_proposition(
				judgement, value_witness_claim
			);
		if (source_is_type_function) {
			if (!value_identity_family || !value_witness_proposition ||
				prototype_judgement_classifier_conversion(
					terms, types, value_identity_family->subject, result_identity
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
				prototype_judgement_classifier_conversion(
					terms,
					types,
					value_witness_proposition->classifier,
					result_identity
				).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
				return -1;
			}
			current_term = value_witness;
			current_claim = value_witness_claim;
			if (value_witness_proposition->classifier != result_identity &&
				prototype_judgement_add_conversion_claim(
					judgement,
					terms,
					types,
					xr_context,
					current_term,
					result_identity,
					current_claim,
					&current_claim
				) != 0) {
				return -1;
			}
		} else {
			uint32_t result_action_family;
			uint32_t result_action_source;
			uint32_t result_action_operator;
			uint32_t result_action_dimension;
			uint32_t result_boundaries[2];
			size_t result_boundary_count;
			if (prototype_dimension_action_family_instance_info(
					terms,
					kernel->dimension_operators,
					result_identity,
					&result_action_family,
					&result_action_source,
					&result_action_operator,
					&result_action_dimension,
					result_boundaries,
					2,
					&result_boundary_count
				) != 0 || result_action_dimension != 1 ||
				result_boundary_count != 2) {
				return -1;
			}
			const struct prototype_judgement_proposition* endpoint_values[2] = {
				prototype_judgement_claim_proposition(
					judgement, left_value_claim
				),
				prototype_judgement_claim_proposition(
					judgement, right_value_claim
				)
			};
			uint32_t endpoint_value_claims[2] = {
				left_value_claim, right_value_claim
			};
			uint32_t endpoint_function_claims[2];
			uint32_t endpoint_argument_claims[2] = {x0_claim, x1_claim};
			uint32_t endpoint_thunk_claims[2];
			uint32_t result_computation = result_action_source < terms->term_count &&
				terms->terms[result_action_source].tag == PROTOTYPE_TERM_THUNK_TYPE ?
				terms->terms[result_action_source].as.thunk_type.computation :
				PROTOTYPE_INVALID_ID;
			uint32_t source_function_pi = source->classifier < terms->term_count &&
				terms->terms[source->classifier].tag == PROTOTYPE_TERM_THUNK_TYPE ?
				terms->terms[source->classifier].as.thunk_type.computation :
				PROTOTYPE_INVALID_ID;
			if (result_computation == PROTOTYPE_INVALID_ID || source_function_pi ==
					PROTOTYPE_INVALID_ID) {
				return -1;
			}
			if (prototype_judgement_add_reindexed_claim(
					judgement,
					terms,
					types,
					contexts,
					substitutions,
					left_source_claim_id,
					body_target_projection,
					&endpoint_function_claims[0]
				) != 0 || prototype_judgement_add_reindexed_claim(
					judgement,
					terms,
					types,
					contexts,
					substitutions,
					right_source_claim_id,
					body_target_projection,
					&endpoint_function_claims[1]
				) != 0) {
				return -1;
			}
			for (uint32_t i = 0; i < 2; ++i) {
				uint32_t boundary_computation = result_boundaries[i] <
					terms->term_count && terms->terms[result_boundaries[i]].tag ==
						PROTOTYPE_TERM_THUNK ? terms->terms[
							result_boundaries[i]
						].as.thunk.computation : PROTOTYPE_INVALID_ID;
				uint32_t boundary_function = boundary_computation < terms->term_count &&
					terms->terms[boundary_computation].tag == PROTOTYPE_TERM_APP ?
					terms->terms[boundary_computation].as.app.function :
					PROTOTYPE_INVALID_ID;
				uint32_t boundary_argument = boundary_computation < terms->term_count &&
					terms->terms[boundary_computation].tag == PROTOTYPE_TERM_APP ?
					terms->terms[boundary_computation].as.app.argument :
					PROTOTYPE_INVALID_ID;
				uint32_t forced_function = boundary_function < terms->term_count &&
					terms->terms[boundary_function].tag == PROTOTYPE_TERM_FORCE ?
					terms->terms[boundary_function].as.force.value :
					PROTOTYPE_INVALID_ID;
				uint32_t forced_claim;
				uint32_t computation_claim;
				uint32_t thunked_claim;
				const struct prototype_judgement_proposition* endpoint_function =
					prototype_judgement_claim_proposition(
						judgement, endpoint_function_claims[i]
					);
				const struct prototype_judgement_proposition* endpoint_argument =
					prototype_judgement_claim_proposition(
						judgement, endpoint_argument_claims[i]
					);
				int endpoint_status = !endpoint_values[i] || !endpoint_function ||
					!endpoint_argument || forced_function != endpoint_function->subject ||
					boundary_argument != endpoint_argument->subject ? -1 :
					prototype_judgement_add_force_claim(
						judgement,
						terms,
						xr_context,
						boundary_function,
						source_function_pi,
						endpoint_function_claims[i],
						&forced_claim
					);
				int endpoint_stage = 1;
				if (endpoint_status == 0) {
					endpoint_stage = 2;
					endpoint_status = prototype_judgement_add_app_claim(
						judgement,
						terms,
						types,
						contexts,
						substitutions,
						xr_context,
						boundary_computation,
						result_computation,
						forced_claim,
						endpoint_argument_claims[i],
						&computation_claim
					);
				}
				if (endpoint_status == 0) {
					endpoint_stage = 3;
					endpoint_status = prototype_judgement_add_thunk_claim(
						judgement,
						terms,
						types,
						xr_context,
						result_boundaries[i],
						result_action_source,
						computation_claim,
						&thunked_claim
					);
				}
				if (endpoint_status != 0) {
					fprintf(
						stderr,
						"pointwise thunk endpoint typing failed index=%u boundary=%u "
						"computation=%u function=%u forced=%u source=%u argument=%u "
						"stage=%d status=%d\n",
						i,
						result_boundaries[i],
						boundary_computation,
						boundary_function,
						forced_function,
						endpoint_function ? endpoint_function->subject :
							PROTOTYPE_INVALID_ID,
						boundary_argument,
						endpoint_stage,
						endpoint_status
					);
					return -1;
				}
				endpoint_thunk_claims[i] = thunked_claim;
			}
			uint32_t result_action_claim;
			uint32_t rebuilt_action;
			uint32_t result_identity_left;
			uint32_t result_identity_left_claim;
			uint32_t rebuilt_result_identity;
			uint32_t result_identity_has_type_claim;
			uint32_t result_identity_is_type_claim;
			if (hott_ensure_dimension_action_type_claim(
					kernel,
					xr_context,
					result_action_source,
					universe,
					result_action_operator,
					&rebuilt_action,
					&result_action_claim
				) != 0 || rebuilt_action != result_action_family ||
				hott_apply_claim_argument(
					kernel,
					xr_context,
					result_action_claim,
					endpoint_thunk_claims[0],
					&result_identity_left,
					&result_identity_left_claim
				) != 0 || hott_apply_claim_argument(
					kernel,
					xr_context,
					result_identity_left_claim,
					endpoint_thunk_claims[1],
					&rebuilt_result_identity,
					&result_identity_has_type_claim
				) != 0 || rebuilt_result_identity != result_identity ||
				prototype_judgement_add_is_type_claim(
					judgement,
					terms,
					xr_context,
					result_identity,
					universe,
					result_identity_has_type_claim,
					&result_identity_is_type_claim
				) != 0) {
				fprintf(stderr, "pointwise thunk relation reconstruction failed\n");
				return -1;
			}
			uint32_t pointwise_witness_claim;
			if (prototype_judgement_add_dimension_action_thunk_return_witness(
					judgement,
					terms,
					types,
					kernel->dimension_operators,
					xr_context,
					value_witness,
					result_identity,
					result_identity_is_type_claim,
					endpoint_thunk_claims[0],
					endpoint_thunk_claims[1],
					value_witness_claim,
					&pointwise_witness_claim
				) != 0) {
				fprintf(stderr, "pointwise thunk typed action rule failed\n");
				return -1;
			}
			current_term = value_witness;
			current_claim = pointwise_witness_claim;
			(void)endpoint_value_claims;
		}
	} else {
		uint32_t computation_identity_family_claim;
		uint32_t left_computation_claim;
		uint32_t right_computation_claim;
		int computation_action_status =
			hott_construct_object_computation_action_depth(
				kernel,
				body_claim_id,
				&body_environment,
				universe,
				(uint32_t)terms->term_count + 1,
				&left_computation_claim,
				&right_computation_claim,
				&computation_identity_family_claim,
				&current_term,
				&current_claim
			);
		free(mapped_bindings);
		free(mapped_identity_claims);
		const struct prototype_judgement_proposition* computation_identity_family =
			prototype_judgement_claim_proposition(
				judgement, computation_identity_family_claim
			);
		const struct prototype_judgement_proposition* computation_witness =
			prototype_judgement_claim_proposition(judgement, current_claim);
		if (computation_action_status != 0) {
			return computation_action_status;
		}
		if (!computation_identity_family || !computation_witness ||
			prototype_judgement_classifier_conversion(
				terms,
				types,
				computation_identity_family->subject,
				result_identity
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
			prototype_judgement_classifier_conversion(
				terms, types, computation_witness->classifier, codomain[2]
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		}
		if (computation_witness->classifier != codomain[2] &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				xr_context,
				current_term,
				codomain[2],
				current_claim,
				&current_claim
			) != 0) {
			return -1;
		}
		current_is_computation = 1;
		(void)left_computation_claim;
		(void)right_computation_claim;
	}
	uint32_t current_context = xr_context;
	for (uint32_t i = 3; i > 0; --i) {
		uint32_t lambda_body;
		uint32_t lambda_body_claim;
		uint32_t lambda;
		uint32_t lambda_claim;
		uint32_t binder_claim;
		uint32_t parent_context = i == 3 ? x1_context :
			(i == 2 ? x0_context : target_context_id);
		if (source_is_type_function) {
			lambda_body = current_term;
			lambda_body_claim = current_claim;
		} else if (current_is_computation) {
			lambda_body = current_term;
			lambda_body_claim = current_claim;
			current_is_computation = 0;
		} else if (prototype_term_return(
			terms, current_term, &lambda_body
		) != 0 || prototype_judgement_add_return_claim(
			judgement,
			terms,
			types,
			current_context,
			lambda_body,
			codomain[i - 1],
			current_claim,
			&lambda_body_claim
		) != 0) {
			return -1;
		}
		if (prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				current_context,
				binders[i - 1],
				terms->terms[pi[i - 1]].as.pi.domain,
				&binder_claim
			) != 0 || prototype_term_lambda(
				terms, binders[i - 1], lambda_body, &lambda
			) != 0 || prototype_judgement_add_lambda_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				parent_context,
				lambda,
				pi[i - 1],
				binder_claim,
				lambda_body_claim,
				&lambda_claim
			) != 0) {
			return -1;
		}
		if (source_is_computation_function) {
			uint32_t thunked;
			uint32_t thunked_claim;
			if (prototype_term_thunk(
					terms, lambda, &thunked
				) != 0 || prototype_judgement_add_thunk_claim(
					judgement,
					terms,
					types,
					parent_context,
					thunked,
					thunk_type[i - 1],
					lambda_claim,
					&thunked_claim
				) != 0) {
				return -1;
			}
			current_term = thunked;
			current_claim = thunked_claim;
		} else {
			current_term = lambda;
			current_claim = lambda_claim;
		}
		current_context = parent_context;
	}
	uint32_t identity_family_claim;
	if (hott_instantiate_object_identity_family(
			kernel,
			identity,
			target_context_id,
			left_source_claim_id,
			right_source_claim_id,
			&identity_family_claim
		) != 0 || prototype_judgement_claim_proposition(
			judgement, identity_family_claim
		)->subject != concrete_identity || prototype_judgement_claim_proposition(
			judgement, current_claim
		)->classifier != concrete_identity) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = current_term;
	*p_witness_claim_id = current_claim;
	(void)x0_context_certificate;
	(void)x1_context_certificate;
	(void)xr_context_certificate;
	(void)left_body_substitution_certificate;
	(void)right_body_substitution_certificate;
	return 0;
}

static int hott_construct_object_value_action_depth(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	const struct hott_object_action_environment* environment,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_left_claim_id,
	uint32_t* p_right_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
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
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!source || !environment || !p_left_claim_id || !p_right_claim_id ||
		!p_identity_family_claim_id || !p_witness_term_id ||
		!p_witness_claim_id || remaining_depth == 0 || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source->subject >=
			terms->term_count) {
		return -1;
	}
	const struct prototype_judgement_derivation* reindex_derivation =
		hott_derivation_for_claim_and_kind(
			judgement,
			source_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
		);
	const struct prototype_judgement_derivation* direct_constructor_derivation =
		hott_constructor_derivation_for_claim(judgement, source_claim_id);
	/* Accepted Claims may have several Derivations. A constructor introduction
	 * is the structural action authority for a constructor subject; a later
	 * reindex derivation is an alternative proof path, not a replacement. */
	if (!identity && reindex_derivation && !direct_constructor_derivation) {
		const struct prototype_substitution* reindex_action =
			reindex_derivation->semantic_action_kind ==
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ?
			prototype_substitution_get(
				substitutions, reindex_derivation->semantic_action_id
			) : NULL;
		if (reindex_derivation->premise_count != 1 ||
			reindex_derivation->premises[0].claim_id == PROTOTYPE_INVALID_ID ||
			reindex_derivation->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			!reindex_action) {
			return -1;
		}
		uint32_t left_substitution;
		uint32_t right_substitution;
		int left_compose_status = prototype_substitution_compose(
				substitutions,
				contexts,
				reindex_derivation->semantic_action_id,
				environment->left_substitution_id,
				&left_substitution
			);
		int right_compose_status = prototype_substitution_compose(
				substitutions,
				contexts,
				reindex_derivation->semantic_action_id,
				environment->right_substitution_id,
				&right_substitution
			);
		if (left_compose_status != 0 || right_compose_status != 0) {
			/* A Claim may have both a direct structural derivation and later
			 * reindex derivations. An unrelated reindex edge is not the canonical
			 * decomposition of that Claim for the current object action. */
		} else {
			struct hott_object_action_environment source_environment = *environment;
			source_environment.left_substitution_id = left_substitution;
			source_environment.right_substitution_id = right_substitution;
			int source_action_status = hott_construct_object_value_action_depth(
				kernel,
				NULL,
				reindex_derivation->premises[0].claim_id,
				&source_environment,
				universe,
				remaining_depth - 1,
				p_left_claim_id,
				p_right_claim_id,
				p_identity_family_claim_id,
				p_witness_term_id,
				p_witness_claim_id
			);
			return source_action_status;
		}
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_claim_id,
			environment->right_substitution_id,
			&right_claim_id
		) != 0) {
		return -1;
	}
	if (!identity && terms->terms[source->subject].tag == PROTOTYPE_TERM_VAR) {
		uint32_t source_binding = terms->terms[
			source->subject
		].as.var.binding_id;
		uint32_t mapped_identity_claim = PROTOTYPE_INVALID_ID;
		for (uint32_t i = 0; i < environment->binding_count; ++i) {
			if (environment->source_binding_ids[i] == source_binding) {
				mapped_identity_claim = environment->identity_claim_ids[i];
				break;
			}
		}
		const struct prototype_judgement_proposition* mapped_identity =
			prototype_judgement_claim_proposition(
				judgement, mapped_identity_claim
			);
		uint32_t mapped_family_claim = mapped_identity ?
			hott_has_type_claim_for_subject(
				judgement,
				environment->target_context_id,
				mapped_identity->classifier
			) : PROTOTYPE_INVALID_ID;
		if (mapped_identity && mapped_family_claim == PROTOTYPE_INVALID_ID) {
			mapped_family_claim = hott_is_type_claim_for_subject(
				judgement,
				environment->target_context_id,
				mapped_identity->classifier
			);
		}
		if (mapped_identity && mapped_family_claim == PROTOTYPE_INVALID_ID &&
			hott_ensure_is_type_claim_in_context(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				environment->target_context_id,
				mapped_identity->classifier,
				&mapped_family_claim
			) != 0) {
			mapped_family_claim = PROTOTYPE_INVALID_ID;
		}
		if (mapped_identity && mapped_identity->kind ==
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE && mapped_identity->context_id ==
				environment->target_context_id && mapped_family_claim !=
					PROTOTYPE_INVALID_ID) {
			*p_left_claim_id = left_claim_id;
			*p_right_claim_id = right_claim_id;
			*p_identity_family_claim_id = mapped_family_claim;
			*p_witness_term_id = mapped_identity->subject;
			*p_witness_claim_id = mapped_identity_claim;
			return 0;
		}
	}
	struct prototype_hott_identity_type_computation_certificate local_identity;
	if (!identity) {
		int identity_status = hott_prepare_local_object_identity(
			kernel,
			source_claim_id,
			environment,
			universe,
			&local_identity
		);
		if (identity_status != 0) {
			fprintf(stderr,
				"HOTT object action local Identity failed source-claim=%u "
				"status=%d subject=%u subject-tag=%u classifier=%u "
				"classifier-tag=%u context=%u\n",
				source_claim_id,
				identity_status,
				source->subject,
				(unsigned)terms->terms[source->subject].tag,
				source->classifier,
				source->classifier < terms->term_count ?
					(unsigned)terms->terms[source->classifier].tag : UINT32_MAX,
				source->context_id);
			return identity_status;
		}
		identity = &local_identity;
	}
	const struct prototype_judgement_derivation* conversion_derivation =
		hott_derivation_for_claim_and_kind(
			judgement,
			source_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_CONVERSION
		);
	if (conversion_derivation) {
		if (conversion_derivation->premise_count != 1) {
			return -1;
		}
		uint32_t converted_left_claim;
		uint32_t converted_right_claim;
		uint32_t converted_family_claim;
		uint32_t converted_witness;
		uint32_t converted_witness_claim;
		int converted_status = hott_construct_object_value_action_depth(
			kernel,
			identity,
			conversion_derivation->premises[0].claim_id,
			environment,
			universe,
			remaining_depth - 1,
			&converted_left_claim,
			&converted_right_claim,
			&converted_family_claim,
			&converted_witness,
			&converted_witness_claim
		);
		if (converted_status != 0) {
			fprintf(stderr,
				"HOTT converted object action failed source-claim=%u premise-claim=%u "
				"status=%d\n",
				source_claim_id,
				conversion_derivation->premises[0].claim_id,
				converted_status);
			return converted_status;
		}
		uint32_t identity_family_claim;
		if (hott_instantiate_object_identity_family(
				kernel,
				identity,
				environment->target_context_id,
				left_claim_id,
				right_claim_id,
				&identity_family_claim
			) != 0) {
			fprintf(stderr,
				"HOTT converted object family failed source-claim=%u\n",
				source_claim_id);
			return -1;
		}
		const struct prototype_judgement_proposition* family =
			prototype_judgement_claim_proposition(
				judgement, identity_family_claim
			);
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, converted_witness_claim
			);
		if (!family || !witness || witness->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || witness->context_id !=
				environment->target_context_id ||
			prototype_judgement_classifier_conversion(
				terms, types, witness->classifier, family->subject
			).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			fprintf(stderr,
				"HOTT converted object witness validation failed source-claim=%u "
				"family-claim=%u witness-claim=%u\n",
				source_claim_id, identity_family_claim,
				converted_witness_claim);
			return -1;
		}
		uint32_t witness_claim = converted_witness_claim;
		if (witness->classifier != family->subject &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				environment->target_context_id,
				converted_witness,
				family->subject,
				converted_witness_claim,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = converted_witness;
		*p_witness_claim_id = witness_claim;
		(void)converted_left_claim;
		(void)converted_right_claim;
		(void)converted_family_claim;
		return 0;
	}
	/* Type expression action is invariant under the kernel's pure conversion.
	 * Reduce a beta/iota redex before dispatching on its type former, then
	 * convert the resulting witness back to the original endpoint family. */
	uint32_t normalized_subject;
	if (source->classifier < terms->term_count &&
		terms->terms[source->classifier].tag == PROTOTYPE_TERM_UNIVERSE_VAR &&
		prototype_term_normalize_complete_with_profile(
			terms,
			types,
			NULL,
			PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
			source->subject,
			&normalized_subject
		) == 0 && normalized_subject != source->subject) {
		uint32_t normalized_claim = PROTOTYPE_INVALID_ID;
		uint32_t ancestor_context = source->context_id;
		while (ancestor_context != PROTOTYPE_INVALID_ID) {
			uint32_t ancestor_claim = hott_has_type_claim_for_subject(
				judgement, ancestor_context, normalized_subject
			);
			const struct prototype_judgement_proposition* ancestor =
				prototype_judgement_claim_proposition(
					judgement, ancestor_claim
				);
			if (ancestor && prototype_judgement_classifier_conversion(
					terms, types, ancestor->classifier, source->classifier
				).status == PROTOTYPE_TERM_CONVERSION_EQUAL) {
				if (ancestor_context == source->context_id) {
					normalized_claim = ancestor_claim;
				} else {
					uint32_t projection;
					if (prototype_substitution_projection_path(
							substitutions,
							contexts,
							source->context_id,
							ancestor_context,
							&projection
						) != 0 || prototype_judgement_add_reindexed_claim(
							judgement,
							terms,
							types,
							contexts,
							substitutions,
							ancestor_claim,
							projection,
							&normalized_claim
						) != 0) {
						return -1;
					}
				}
				break;
			}
			const struct prototype_context* ancestor_record =
				prototype_context_get(contexts, ancestor_context);
			ancestor_context = ancestor_record ? ancestor_record->parent :
				PROTOTYPE_INVALID_ID;
		}
		if (normalized_claim == PROTOTYPE_INVALID_ID &&
			prototype_judgement_add_structural_type_formation_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source->context_id,
				normalized_subject,
				source->classifier,
				&normalized_claim
			) != 0) {
			return -1;
		}
		uint32_t normalized_left_claim;
		uint32_t normalized_right_claim;
		uint32_t normalized_family_claim;
		uint32_t normalized_witness;
		uint32_t normalized_witness_claim;
		int normalized_status = hott_construct_object_value_action_depth(
			kernel,
			NULL,
			normalized_claim,
			environment,
			universe,
			remaining_depth - 1,
			&normalized_left_claim,
			&normalized_right_claim,
			&normalized_family_claim,
			&normalized_witness,
			&normalized_witness_claim
		);
		if (normalized_status != 0) {
			return normalized_status;
		}
		uint32_t identity_family_claim;
		if (hott_instantiate_object_identity_family(
				kernel,
				identity,
				environment->target_context_id,
				left_claim_id,
				right_claim_id,
				&identity_family_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* family =
			prototype_judgement_claim_proposition(
				judgement, identity_family_claim
			);
		const struct prototype_judgement_proposition* witness =
			prototype_judgement_claim_proposition(
				judgement, normalized_witness_claim
			);
		if (!family || !witness || witness->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || witness->context_id !=
				environment->target_context_id) {
			return -1;
		}
		uint32_t witness_claim = normalized_witness_claim;
		int witness_conversion = prototype_judgement_classifier_conversion(
			terms, types, witness->classifier, family->subject
		).status;
		if (witness_conversion != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			return -1;
		} else if (witness->classifier != family->subject &&
			prototype_judgement_add_conversion_claim(
				judgement,
				terms,
				types,
				environment->target_context_id,
				normalized_witness,
				family->subject,
				normalized_witness_claim,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = normalized_witness;
		*p_witness_claim_id = witness_claim;
		(void)normalized_left_claim;
		(void)normalized_right_claim;
		(void)normalized_family_claim;
		return 0;
	}
	int status;
	if (terms->terms[source->subject].tag == PROTOTYPE_TERM_VAR) {
		status = hott_construct_object_variable_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	} else if (terms->terms[source->subject].tag == PROTOTYPE_TERM_THUNK &&
		identity->computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
		status = hott_construct_identity_lambda_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	} else if (terms->terms[source->subject].tag == PROTOTYPE_TERM_LAMBDA) {
		status = hott_construct_identity_lambda_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	} else if (terms->terms[source->subject].tag == PROTOTYPE_TERM_MATCH &&
		identity->backing_type_former_term_id != PROTOTYPE_INVALID_ID) {
		status = hott_construct_nullary_match_object_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			p_left_claim_id,
			p_right_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
		return status;
	} else if (identity->computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT &&
		identity->backing_type_former_term_id != PROTOTYPE_INVALID_ID) {
		status = hott_construct_adt_value_action(
			kernel,
			identity,
			source_claim_id,
			environment,
			universe,
			remaining_depth - 1,
			p_left_claim_id,
			p_right_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
		return status;
	} else if ((identity->computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_GENERIC_DIMENSION_ACTION ||
		 identity->computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_THUNK_RETURN) &&
		identity->backing_type_former_term_id != PROTOTYPE_INVALID_ID) {
		uint32_t higher_source;
		uint32_t extension_operator;
		uint32_t target_projection;
		uint32_t source_claim_in_target;
		uint32_t source_classifier_claim;
		uint32_t identity_family_claim;
		uint32_t identity_family_is_type_claim;
		uint32_t witness;
		uint32_t witness_claim;
		if (prototype_term_dimension_action_info(
				terms,
				identity->backing_type_former_term_id,
				&higher_source,
				&extension_operator
			) != 0 || prototype_substitution_projection_path(
				substitutions,
				contexts,
				environment->target_context_id,
				source->context_id,
				&target_projection
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source_claim_id,
				target_projection,
				&source_claim_in_target
			) != 0 || hott_ensure_is_type_claim_in_context(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				environment->target_context_id,
				source->classifier,
				&source_classifier_claim
			) != 0 || hott_instantiate_object_identity_family(
				kernel,
				identity,
				environment->target_context_id,
				left_claim_id,
				right_claim_id,
				&identity_family_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* family =
			prototype_judgement_claim_proposition(
				judgement, identity_family_claim
			);
		if (!family || prototype_judgement_add_is_type_claim(
				judgement,
				terms,
				environment->target_context_id,
				family->subject,
				universe,
				identity_family_claim,
				&identity_family_is_type_claim
			) != 0 || prototype_term_dimension_action(
				terms,
				kernel->dimension_operators,
				source->subject,
				extension_operator,
				&witness
			) != 0 || prototype_judgement_add_dimension_action_term(
				judgement,
				terms,
				kernel->dimension_operators,
				environment->target_context_id,
				witness,
				family->subject,
				source_claim_in_target,
				source_classifier_claim,
				&witness_claim
			) != 0) {
			return -1;
		}
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = witness;
		*p_witness_claim_id = witness_claim;
		(void)higher_source;
		return 0;
	} else {
		return 1;
	}
	if (status == 0) {
		*p_left_claim_id = left_claim_id;
		*p_right_claim_id = right_claim_id;
	}
	return status;
}

static int hott_construct_dimension_action_degeneracy(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
	uint32_t universe,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	uint32_t acted_source;
	uint32_t operator_id;
	uint32_t target_dimension;
	uint32_t source_classifier_is_type;
	uint32_t acted_classifier;
	uint32_t acted_classifier_claim;
	uint32_t identity_family;
	uint32_t identity_family_claim;
	uint32_t identity_family_is_type;
	uint32_t witness;
	uint32_t witness_claim;
	if (!kernel || !identity || !terms || !judgement || !source ||
		!p_identity_family_claim_id || !p_witness_term_id || !p_witness_claim_id ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_term_dimension_action_info(
			terms,
			identity->backing_type_former_term_id,
			&acted_source,
			&operator_id
		) != 0 || prototype_dimension_action_term_dimension(
			terms,
			kernel->dimension_operators,
			identity->backing_type_former_term_id,
			&target_dimension
		) != 0) {
		return -1;
	}
	if (hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			kernel->type_declarations,
			kernel->contexts,
			kernel->substitutions,
			source->context_id,
			source->classifier,
			&source_classifier_is_type
		) != 0) {
		return -1;
	}
	if (prototype_dimension_action_extend_classifier_family(
			terms,
			kernel->dimension_operators,
			source->classifier,
			operator_id,
			&acted_classifier
		) != 0 || acted_classifier != identity->backing_type_former_term_id) {
		return -1;
	}
	uint32_t formed_acted_classifier;
	if (hott_ensure_dimension_action_type_claim(
			kernel,
			source->context_id,
			acted_source,
			universe,
			operator_id,
			&formed_acted_classifier,
			&acted_classifier_claim
		) != 0 || formed_acted_classifier != acted_classifier) {
		return -1;
	}
	if (prototype_dimension_action_term_classifier(
			terms,
			kernel->dimension_operators,
			source->subject,
			source->classifier,
			acted_classifier,
			target_dimension,
			&identity_family
		) != 0 || prototype_hott_ensure_dimension_classifier_formation(
			kernel,
			source->context_id,
			identity_family,
			universe
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			kernel->type_declarations,
			kernel->contexts,
			kernel->substitutions,
			source->context_id,
			identity_family,
			universe,
			&identity_family_claim
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			source->context_id,
			identity_family,
			universe,
			identity_family_claim,
			&identity_family_is_type
		) != 0) {
		return -1;
	}
	if (prototype_term_dimension_action(
			terms,
			kernel->dimension_operators,
			source->subject,
			operator_id,
			&witness
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_dimension_action_term(
			judgement,
			terms,
			kernel->dimension_operators,
			source->context_id,
			witness,
			identity_family,
			source_claim_id,
			source_classifier_is_type,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)acted_source;
	(void)acted_classifier_claim;
	return 0;
}

int prototype_hott_construct_degeneracy(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t source_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_proposition* source_type =
		request && kernel && request->kind ==
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_judgement_claim_proposition(
			kernel->judgement, request->key.identity_type.source_claim_id
		) : NULL;
	const struct prototype_judgement_proposition* source_term = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, source_claim_id
		) : NULL;
	struct prototype_kernel_view view;
	if (!actions || !kernel || !bridges || !result || !request || !certificate ||
		!source_type || !source_term || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id ||
		prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_db_validate(actions, &view, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		source_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		source_term->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source_term->classifier != source_type->subject) {
		return -1;
	}
	uint32_t constructor_head;
	uint32_t constructor_owner;
	uint32_t constructor_ordinal;
	uint32_t constructor_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t constructor_argument_count;
	if (source_term->context_id == source_type->context_id &&
		prototype_term_constructor_spine_info(
			kernel->terms,
			source_term->subject,
			&constructor_head,
			&constructor_owner,
			&constructor_ordinal,
			constructor_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&constructor_argument_count
		) == 0) {
		/* Constructor action follows its complete dependent field Context. */
		return prototype_hott_construct_object_term_action(
			actions,
			kernel,
			bridges,
			identity_result_id,
			source_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	uint32_t action_source;
	uint32_t action_operator;
	if (prototype_term_dimension_action_info(
			kernel->terms,
			certificate->data.identity_type.backing_type_former_term_id,
			&action_source,
			&action_operator
		) == 0) {
		(void)action_source;
		(void)action_operator;
		return hott_construct_dimension_action_degeneracy(
			kernel,
			&certificate->data.identity_type,
			source_claim_id,
			source_type->classifier,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	if (source_term->context_id == source_type->context_id) {
		return prototype_hott_construct_object_term_action(
			actions,
			kernel,
			bridges,
			identity_result_id,
			source_claim_id,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	if (certificate->data.identity_type.computation_rule ==
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_PI_POINTWISE) {
		return hott_construct_identity_lambda_action(
			kernel,
			&certificate->data.identity_type,
			source_claim_id,
			NULL,
			source_type->classifier,
			p_identity_family_claim_id,
			p_witness_term_id,
			p_witness_claim_id
		);
	}
	return 1;
}

int prototype_hott_construct_object_term_action(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t source_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_hott_bridge* bridge = request && request->kind ==
		PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_hott_bridge_db_get(
			bridges, request->key.identity_type.source_bridge_id
		) : NULL;
	const struct prototype_judgement_proposition* source_type = request &&
		kernel && request->kind ==
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_judgement_claim_proposition(
			kernel->judgement, request->key.identity_type.source_claim_id
		) : NULL;
	const struct prototype_judgement_proposition* source_term = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, source_claim_id
		) : NULL;
	struct prototype_kernel_view view;
	if (!actions || !kernel || !bridges || !result || !request || !certificate ||
		!bridge || !source_type || !source_term || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id ||
		prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_db_validate(actions, &view, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		source_type->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source_term->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source_term->context_id !=
			source_type->context_id || source_term->classifier != source_type->subject ||
		bridge->source_context_id != source_type->context_id) {
		return -1;
	}
	const struct prototype_context* source_context = prototype_context_get(
		kernel->contexts, source_type->context_id
	);
	if (!source_context) {
		return -1;
	}
	size_t environment_capacity = source_context->depth ?
		source_context->depth : 1;
	uint32_t* source_bindings = malloc(
		environment_capacity * sizeof(*source_bindings)
	);
	uint32_t* identity_claims = malloc(
		environment_capacity * sizeof(*identity_claims)
	);
	if (!source_bindings || !identity_claims) {
		free(source_bindings);
		free(identity_claims);
		return -1;
	}
	struct hott_object_action_environment environment;
	if (hott_build_object_action_environment(
			kernel,
			bridges,
			bridge->id,
			source_bindings,
			identity_claims,
			(uint32_t)environment_capacity,
			&environment
		) != 0) {
		free(source_bindings);
		free(identity_claims);
		return -1;
	}
	uint32_t left_claim;
	uint32_t right_claim;
	int status = hott_construct_object_value_action_depth(
		kernel,
		&certificate->data.identity_type,
		source_claim_id,
		&environment,
		source_type->classifier,
		(uint32_t)kernel->terms->term_count + 1,
		&left_claim,
		&right_claim,
		p_identity_family_claim_id,
		p_witness_term_id,
		p_witness_claim_id
	);
	free(source_bindings);
	free(identity_claims);
	return status;
}

int prototype_hott_instantiate_object_identity_family(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t* p_identity_family_claim_id
) {
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_proposition* source_type = request &&
		kernel && request->kind ==
			PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ?
		prototype_judgement_claim_proposition(
			kernel->judgement, request->key.identity_type.source_claim_id
		) : NULL;
	const struct prototype_judgement_proposition* left = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, left_endpoint_claim_id
		) : NULL;
	const struct prototype_judgement_proposition* right = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, right_endpoint_claim_id
		) : NULL;
	struct prototype_kernel_view view;
	if (!actions || !kernel || !bridges || !result || !request || !certificate ||
		!source_type || !left || !right || !p_identity_family_claim_id ||
		prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_db_validate(actions, &view, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		source_type->kind != PROTOTYPE_JUDGEMENT_KIND_IS_TYPE ||
		left->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		right->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		left->context_id != source_type->context_id ||
		right->context_id != source_type->context_id ||
		prototype_judgement_classifier_conversion(
			kernel->terms,
			kernel->type_declarations,
			left->classifier,
			source_type->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL ||
		prototype_judgement_classifier_conversion(
			kernel->terms,
			kernel->type_declarations,
			right->classifier,
			source_type->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	return hott_instantiate_object_identity_family(
		kernel,
		&certificate->data.identity_type,
		source_type->context_id,
		left_endpoint_claim_id,
		right_endpoint_claim_id,
		p_identity_family_claim_id
	);
}

int prototype_hott_check_object_identity_witness(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t identity_result_id,
	uint32_t left_endpoint_claim_id,
	uint32_t right_endpoint_claim_id,
	uint32_t witness_claim_id,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_checked_witness_claim_id
) {
	const struct prototype_judgement_proposition* witness = kernel ?
		prototype_judgement_claim_proposition(
			kernel->judgement, witness_claim_id
		) : NULL;
	uint32_t family_claim_id;
	if (!kernel || !witness || !p_identity_family_claim_id ||
		!p_checked_witness_claim_id || witness->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_hott_instantiate_object_identity_family(
			actions,
			kernel,
			bridges,
			identity_result_id,
			left_endpoint_claim_id,
			right_endpoint_claim_id,
			&family_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_proposition* family =
		prototype_judgement_claim_proposition(
			kernel->judgement, family_claim_id
		);
	if (!family || witness->context_id != family->context_id ||
		prototype_judgement_classifier_conversion(
			kernel->terms,
			kernel->type_declarations,
			witness->classifier,
			family->subject
		).status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
		return -1;
	}
	uint32_t checked_witness_claim_id = witness_claim_id;
	if (witness->classifier != family->subject &&
		prototype_judgement_add_conversion_claim(
			kernel->judgement,
			kernel->terms,
			kernel->type_declarations,
			family->context_id,
			witness->subject,
			family->subject,
			witness_claim_id,
			&checked_witness_claim_id
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = family_claim_id;
	*p_checked_witness_claim_id = checked_witness_claim_id;
	return 0;
}

int prototype_hott_execute_object_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t request_id,
	uint32_t* p_result_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !judgement || !p_result_id ||
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
	if (!request || request->kind != PROTOTYPE_HOTT_ACTION_OBJECT_TERM) {
		return -1;
	}
	const struct prototype_judgement_proposition* source_proposition =
		prototype_judgement_claim_proposition(
			judgement, request->key.object_term.source_claim_id
		);
	/* The current bridge duplicates source bindings into left endpoint, right
	 * endpoint, and relation-witness bindings. Until those maps carry an
	 * explicit graded modality, a resource-sensitive source action is residual
	 * rather than an implicit contraction. */
	if (!source_proposition) {
		return -1;
	}
	if (source_proposition->resource_usage_count != 0) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
			p_result_id
		);
	}
	uint32_t identity_result_id;
	if (prototype_hott_execute_identity_type_computation(
			actions,
			kernel,
			bridges,
			request->key.object_term.identity_type_action_request_id,
			&identity_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* identity_result =
		prototype_hott_action_result_get(actions, identity_result_id);
	if (!identity_result) {
		return -1;
	}
	if (identity_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			identity_result->outcome.residual_reason,
			p_result_id
		);
	}
	const struct prototype_hott_bridge* bridge = prototype_hott_bridge_db_get(
		bridges, request->key.object_term.source_bridge_id
	);
	if (!bridge) {
		return -1;
	}
	uint32_t family_claim_id;
	uint32_t witness_term_id;
	uint32_t witness_claim_id;
	int action_status = prototype_hott_construct_object_term_action(
		actions,
		kernel,
		bridges,
		identity_result_id,
		request->key.object_term.source_claim_id,
		&family_claim_id,
		&witness_term_id,
		&witness_claim_id
	);
	if (action_status < 0) {
		return -1;
	}
	if (action_status > 0) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
			p_result_id
		);
	}
	uint32_t left_claim_id;
	uint32_t right_claim_id;
	if (prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			request->key.object_term.source_claim_id,
			bridge->left_substitution_id,
			&left_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			request->key.object_term.source_claim_id,
			bridge->right_substitution_id,
			&right_claim_id
		) != 0) {
		return -1;
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_OBJECT_TERM,
		.data.object_term = {
			.left_endpoint_claim_id = left_claim_id,
			.right_endpoint_claim_id = right_claim_id,
			.identity_family_has_type_claim_id = family_claim_id,
			.witness_term_id = witness_term_id,
			.witness_has_type_claim_id = witness_claim_id
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

static int hott_ensure_is_type_claim_in_context(
	struct prototype_judgement_db* judgement,
	struct prototype_term_db* terms,
	struct prototype_type_declaration_db* type_declarations,
	const struct prototype_context_db* contexts,
	struct prototype_substitution_db* substitutions,
	uint32_t context_id,
	uint32_t subject,
	uint32_t* p_claim_id
) {
	if (!judgement || !terms || !type_declarations || !contexts ||
		!substitutions || !p_claim_id) {
		return -1;
	}
	uint32_t existing = hott_is_type_claim_for_subject(
		judgement, context_id, subject
	);
	if (existing != PROTOTYPE_INVALID_ID) {
		*p_claim_id = existing;
		return 0;
	}
	uint32_t ancestor = context_id;
	while (ancestor != PROTOTYPE_INVALID_ID) {
		uint32_t source_claim_id = hott_is_type_claim_for_subject(
			judgement, ancestor, subject
		);
		if (source_claim_id != PROTOTYPE_INVALID_ID) {
			uint32_t projection;
			uint32_t reindexed_claim_id;
			if (prototype_substitution_projection_path(
					substitutions, contexts, context_id, ancestor, &projection
				) != 0 || prototype_judgement_add_reindexed_claim(
					judgement,
					terms,
					type_declarations,
					contexts,
					substitutions,
					source_claim_id,
					projection,
					&reindexed_claim_id
				) != 0) {
				return -1;
			}
			const struct prototype_judgement_claim* reindexed =
				prototype_judgement_claim_get(judgement, reindexed_claim_id);
			if (!reindexed || prototype_judgement_proposition_get(judgement, reindexed->proposition_id)->subject != subject) {
				return 1;
			}
			*p_claim_id = reindexed_claim_id;
			return 0;
		}
		const struct prototype_context* context =
			prototype_context_get(contexts, ancestor);
		ancestor = context ? context->parent : PROTOTYPE_INVALID_ID;
	}
	return 1;
}

static const struct prototype_cwf_certificate*
hott_context_formation_certificate_for_context(
	const struct prototype_cwf_certificate_db* certificates,
	uint32_t context_id
) {
	if (!certificates) {
		return NULL;
	}
	for (uint32_t i = 0; i < certificates->certificate_count; ++i) {
		if (certificates->certificates[i].kind ==
				PROTOTYPE_CWF_CERTIFICATE_CONTEXT_FORMATION &&
			certificates->certificates[i].structural_id == context_id) {
			return &certificates->certificates[i];
		}
	}
	return NULL;
}

static int hott_ensure_bridge_for_context(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	uint32_t source_context_id,
	uint32_t* p_bridge_id,
	int* p_residual_reason
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
		!judgement || !p_bridge_id || !p_residual_reason ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	if (hott_bridge_for_source_context_and_semantics(
			bridges,
			source_context_id,
			source_context_id == prototype_context_empty(contexts) ?
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_EMPTY_IDENTITY :
				PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION,
			p_bridge_id
		)) {
		*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
		return 0;
	}
	const struct prototype_context* source_context =
		prototype_context_get(contexts, source_context_id);
	const struct prototype_cwf_certificate* source_certificate =
		hott_context_formation_certificate_for_context(
			cwf_certificates, source_context_id
		);
	if (!source_context || !source_certificate ||
		source_context->parent == PROTOTYPE_INVALID_ID ||
		source_certificate->claim_id == PROTOTYPE_INVALID_ID) {
		return -1;
	}
	uint32_t parent_bridge_id;
	int parent_status = hott_ensure_bridge_for_context(
		actions,
		kernel,
		bridges,
		source_context->parent,
		&parent_bridge_id,
		p_residual_reason
	);
	if (parent_status != 0) {
		return parent_status;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = source_certificate->claim_id,
			.source_bridge_id = parent_bridge_id
		}
	};
	uint32_t type_request_id;
	uint32_t type_result_id;
	if (prototype_hott_action_request_intern(
			actions, &view, bridges, type_request, &type_request_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			actions, kernel, bridges, type_request_id, &type_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = type_result->outcome.residual_reason;
		return 1;
	}
	if (prototype_hott_bridge_db_construct_extension(
			bridges,
			kernel,
			actions,
			source_context_id,
			type_request_id,
			p_bridge_id
		) != 0) {
		return -1;
	}
	*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	return 0;
}

static int hott_match_case_context_valid(
	const struct prototype_context_db* contexts,
	uint32_t outer_context_id,
	uint32_t case_context_id,
	const struct prototype_case_binder* binders,
	uint32_t binder_count
) {
	if (!contexts || (binder_count > 0 && !binders)) {
		return 0;
	}
	uint32_t current = case_context_id;
	for (uint32_t i = binder_count; i > 0; --i) {
		const struct prototype_context* context =
			prototype_context_get(contexts, current);
		if (!context || context->binding_id != binders[i - 1].binding_id) {
			return 0;
		}
		current = context->parent;
	}
	return current == outer_context_id;
}

static int hott_execute_child_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
	const struct prototype_hott_bridge* bridge,
	uint32_t source_context_id,
	uint32_t child_claim_id,
	uint32_t* p_witness_claim_id,
	int* p_residual_reason
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ? kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ? kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ? kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	struct prototype_kernel_view view;
	const struct prototype_judgement_claim* child_claim =
		prototype_judgement_claim_get(judgement, child_claim_id);
	uint32_t child_type_claim_id = PROTOTYPE_INVALID_ID;
	int child_type_status = child_claim ? hott_ensure_is_type_claim_in_context(
		judgement,
		terms,
		type_declarations,
		contexts,
		substitutions,
		source_context_id,
		prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->classifier,
		&child_type_claim_id
	) : -1;
	if (!actions || !contexts || !substitutions || !cwf_certificates ||
		!cwf_certificates || !bridges || !terms || !type_declarations ||
		!judgement || !bridge || !p_witness_claim_id || !p_residual_reason ||
		!child_claim || prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->context_id != source_context_id || child_type_status < 0 ||
		prototype_kernel_builder_view(kernel, &view) != 0) {
		return -1;
	}
	if (child_type_status > 0 || child_type_claim_id == PROTOTYPE_INVALID_ID) {
		*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE;
		return 1;
	}
	struct prototype_hott_action_request type_request = {
		.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
		.key.relation_type = {
			.source_claim_id = child_type_claim_id,
			.source_bridge_id = bridge->id
		}
	};
	uint32_t type_request_id;
	uint32_t type_result_id;
	if (prototype_hott_action_request_intern(
			actions, &view, bridges, type_request, &type_request_id
		) != 0 || prototype_parametricity_execute_relation_type_action(
			actions, kernel, bridges, type_request_id, &type_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = type_result->outcome.residual_reason;
		return 1;
	}
	struct prototype_hott_action_request term_request = {
		.kind = PROTOTYPE_HOTT_ACTION_TERM,
		.key.term = {
			.source_claim_id = child_claim_id,
			.source_bridge_id = bridge->id,
			.relation_type_action_request_id = type_request_id
		}
	};
	uint32_t term_request_id;
	uint32_t term_result_id;
	if (prototype_hott_action_request_intern(
			actions, &view, bridges, term_request, &term_request_id
		) != 0 || prototype_hott_execute_term_action(
			actions, kernel, bridges, term_request_id, &term_result_id
		) != 0) {
		return -1;
	}
	const struct prototype_hott_action_result* term_result =
		prototype_hott_action_result_get(actions, term_result_id);
	if (!term_result) {
		return -1;
	}
	if (term_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		*p_residual_reason = term_result->outcome.residual_reason;
		return 1;
	}
	if (term_result->certificate_id >= actions->certificate_count ||
		actions->certificates[term_result->certificate_id].kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM) {
		return -1;
	}
	*p_witness_claim_id = actions->certificates[
		term_result->certificate_id
	].data.term.witness_has_type_claim_id;
	*p_residual_reason = PROTOTYPE_HOTT_RESIDUAL_NONE;
	return 0;
}

struct hott_relation_instantiation {
	uint32_t left_endpoint_claim_id;
	uint32_t right_endpoint_claim_id;
	uint32_t left_type_claim_id;
	uint32_t right_type_claim_id;
	uint32_t endpoint_substitution_id;
	uint32_t left_endpoint_substitution_certificate_id;
	uint32_t right_endpoint_substitution_certificate_id;
	uint32_t relation_type_term_id;
	uint32_t relation_is_type_claim_id;
};

static int hott_instantiate_relation_for_claims(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge* bridge,
	const struct prototype_hott_relation_type_action_certificate* type,
	uint32_t left_source_claim_id,
	uint32_t right_source_claim_id,
	uint32_t left_type_source_claim_id,
	uint32_t right_type_source_claim_id,
	struct hott_relation_instantiation* p_instantiation
) {
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_cwf_certificate_db* cwf_certificates = kernel ?
		kernel->cwf_certificates : NULL;
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_judgement_db* judgement = kernel ?
		kernel->judgement : NULL;
	const struct prototype_context* endpoint_context = contexts && type ?
		prototype_context_get(contexts, type->endpoint_context_id) : NULL;
	if (!contexts || !substitutions || !cwf_certificates || !terms ||
		!type_declarations || !judgement || !bridge || !type ||
		!endpoint_context || !p_instantiation ||
		type->relation_family_semantics !=
			PROTOTYPE_HOTT_RELATION_FAMILY_PARAMETRIC_ACTION) {
		return -1;
	}
	struct hott_relation_instantiation result;
	memset(&result, 0, sizeof(result));
	if (prototype_judgement_add_reindexed_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			left_source_claim_id, bridge->left_substitution_id,
			&result.left_endpoint_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			right_source_claim_id, bridge->right_substitution_id,
			&result.right_endpoint_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			left_type_source_claim_id, bridge->left_substitution_id,
			&result.left_type_claim_id
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			right_type_source_claim_id, bridge->right_substitution_id,
			&result.right_type_claim_id
		) != 0) {
		return -1;
	}
	const struct prototype_judgement_claim* left_endpoint =
		prototype_judgement_claim_get(
			judgement, result.left_endpoint_claim_id
		);
	const struct prototype_judgement_claim* right_endpoint =
		prototype_judgement_claim_get(
			judgement, result.right_endpoint_claim_id
		);
	if (!left_endpoint || !right_endpoint) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_endpoint_proposition =
		prototype_judgement_proposition_get(
			judgement, left_endpoint->proposition_id
		);
	const struct prototype_judgement_proposition* right_endpoint_proposition =
		prototype_judgement_proposition_get(
			judgement, right_endpoint->proposition_id
		);
	uint32_t base_substitution;
	uint32_t left_extension;
	if (!left_endpoint_proposition || !right_endpoint_proposition) {
		return -1;
	}
	if (prototype_substitution_identity(
			substitutions, contexts, bridge->bridge_context_id,
			&base_substitution
		) != 0) {
		return -1;
	}
	if (prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			base_substitution,
			endpoint_context->parent,
			left_endpoint_proposition->subject,
			left_endpoint_proposition->classifier,
			&left_extension
		) != 0) {
		return -1;
	}
	if (prototype_substitution_extend(
			substitutions,
			contexts,
			terms,
			type_declarations,
			left_extension,
			type->endpoint_context_id,
			right_endpoint_proposition->subject,
			right_endpoint_proposition->classifier,
			&result.endpoint_substitution_id
		) != 0) {
		return -1;
	}
	if (prototype_cwf_certificate_db_add_substitution(
			cwf_certificates,
			substitutions,
			judgement,
			left_extension,
			result.left_endpoint_claim_id,
			&result.left_endpoint_substitution_certificate_id
		) != 0) {
		return -1;
	}
	if (prototype_cwf_certificate_db_add_substitution(
			cwf_certificates,
			substitutions,
			judgement,
			result.endpoint_substitution_id,
			result.right_endpoint_claim_id,
			&result.right_endpoint_substitution_certificate_id
		) != 0) {
		return -1;
	}
	uint32_t expected_relation;
	if (prototype_term_reindex(
			terms,
			type_declarations,
			contexts,
			substitutions,
			type->relation_type_term_id,
			result.endpoint_substitution_id,
			&result.relation_type_term_id
		) != 0 || prototype_term_relation_type(
			terms,
			left_endpoint_proposition->classifier,
			right_endpoint_proposition->classifier,
			left_endpoint_proposition->subject,
			right_endpoint_proposition->subject,
			&expected_relation
		) != 0 || result.relation_type_term_id != expected_relation) {
		return -1;
	}
	const struct prototype_judgement_proposition* left_type =
		prototype_judgement_claim_proposition(
			judgement, result.left_type_claim_id
		);
	if (!left_type || prototype_judgement_add_relation_type_formation(
			judgement,
			terms,
			bridge->bridge_context_id,
			result.relation_type_term_id,
			left_type->classifier,
			result.left_type_claim_id,
			result.right_type_claim_id,
			result.left_endpoint_claim_id,
			result.right_endpoint_claim_id,
			&result.relation_is_type_claim_id
		) != 0) {
		return -1;
	}
	*p_instantiation = result;
	return 0;
}

int prototype_hott_execute_term_action(
	struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	struct prototype_hott_bridge_db* bridges,
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
		request->kind == PROTOTYPE_HOTT_ACTION_TERM ?
		prototype_judgement_claim_get(
			judgement, request->key.term.source_claim_id
		) : NULL;
	const struct prototype_hott_bridge* bridge = request ?
		prototype_hott_bridge_db_get(
			bridges, request->key.term.source_bridge_id
		) : NULL;
	uint32_t type_result_id;
	if (!source || !bridge || prototype_judgement_proposition_get(judgement, source->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id != bridge->source_context_id) {
		return -1;
	}
	if (hott_action_result_for_request(
			actions, request->key.term.relation_type_action_request_id, &type_result_id
		) != 0) {
		return 1;
	}
	const struct prototype_hott_action_result* type_result =
		prototype_hott_action_result_get(actions, type_result_id);
	if (!type_result) {
		return -1;
	}
	if (type_result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY) {
		return hott_publish_action_residual(
			actions,
			terms,
			request_id,
			type_result->outcome.residual_reason,
			p_result_id
		);
	}
	const struct prototype_hott_action_certificate* type_certificate = type_result &&
		type_result->certificate_id < actions->certificate_count ?
		&actions->certificates[type_result->certificate_id] : NULL;
	if (!type_certificate ||
		type_certificate->kind != PROTOTYPE_HOTT_ACTION_CERTIFICATE_RELATION_TYPE) {
		return 1;
	}
	const struct prototype_hott_relation_type_action_certificate* type =
		&type_certificate->data.relation_type;
	const struct prototype_hott_action_request* type_request =
		prototype_hott_action_request_get(
			actions, request->key.term.relation_type_action_request_id
		);
	struct hott_relation_instantiation instantiation;
	int instantiation_status = type_request ? hott_instantiate_relation_for_claims(
			kernel,
			bridge,
			type,
			request->key.term.source_claim_id,
			request->key.term.source_claim_id,
			type_request->key.relation_type.source_claim_id,
			type_request->key.relation_type.source_claim_id,
			&instantiation
		) : -1;
	if (!type_request || instantiation_status != 0) {
		return -1;
	}
	uint32_t left_endpoint_claim = instantiation.left_endpoint_claim_id;
	uint32_t right_endpoint_claim = instantiation.right_endpoint_claim_id;
	const struct prototype_judgement_claim* left_endpoint_evidence =
		prototype_judgement_claim_get(judgement, left_endpoint_claim);
	const struct prototype_judgement_claim* right_endpoint_evidence =
		prototype_judgement_claim_get(judgement, right_endpoint_claim);
	if (!left_endpoint_evidence || !right_endpoint_evidence) {
		return -1;
	}
	uint32_t endpoint_instantiation =
		instantiation.endpoint_substitution_id;
	uint32_t left_extension_certificate =
		instantiation.left_endpoint_substitution_certificate_id;
	uint32_t right_extension_certificate =
		instantiation.right_endpoint_substitution_certificate_id;
	uint32_t relation_type = instantiation.relation_type_term_id;
	uint32_t relation_claim = instantiation.relation_is_type_claim_id;
	uint32_t witness;
	uint32_t witness_claim;
	int used_relation_binding = 0;
	if (prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
		terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_VAR) {
		uint32_t source_entry_context;
		uint32_t source_binding = terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.var.binding_id;
		if (prototype_context_find_binding(
				contexts,
				prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
				source_binding,
				&source_entry_context
			) == 0) {
			const struct prototype_hott_bridge* binding_bridge =
				hott_bridge_for_source_context_and_semantics(
					bridges,
					source_entry_context,
					PROTOTYPE_HOTT_BRIDGE_SEMANTICS_PARAMETRIC_RELATION,
					NULL
				);
			const struct prototype_context* relation_entry = binding_bridge ?
				prototype_context_get(
					contexts, binding_bridge->bridge_context_id
				) : NULL;
			if (relation_entry &&
				prototype_context_classifier_term(relation_entry) == relation_type &&
				prototype_term_var(
					terms, relation_entry->binding_id, &witness
				) == 0 &&
				prototype_judgement_add_context_binding_assumption(
					judgement,
					terms,
					contexts,
					bridge->bridge_context_id,
					relation_entry->binding_id,
					relation_type,
					&witness_claim
				) == 0) {
				used_relation_binding = 1;
			}
		}
	}
	if (!used_relation_binding) {
		struct prototype_term_conversion_result endpoint_comparison;
		if (prototype_term_compare_for_conversion(
				terms,
				type_declarations,
				NULL,
				PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
				prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
				prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
				64,
				&endpoint_comparison
			) != 0) {
			return -1;
		}
		if (endpoint_comparison.status != PROTOTYPE_TERM_CONVERSION_EQUAL) {
			if (endpoint_comparison.status ==
				PROTOTYPE_TERM_CONVERSION_EXHAUSTED) {
				return hott_publish_action_residual(
					actions,
					terms,
					request_id,
					PROTOTYPE_HOTT_RESIDUAL_CONVERSION_EXHAUSTED,
					p_result_id
				);
			}
			const struct prototype_judgement_derivation* induction_derivation =
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					PROTOTYPE_JUDGEMENT_PROOF_INDUCTION_HYPOTHESIS_ELIM
				);
			if (induction_derivation && prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag ==
					PROTOTYPE_TERM_INDUCTION_HYPOTHESIS) {
				uint32_t argument = terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].
					as.induction_hypothesis.argument;
				uint32_t argument_claim_id = hott_has_type_claim_for_subject(
					judgement, prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id, argument
				);
				if (argument_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				uint32_t argument_witness_claim;
				int residual_reason;
				int argument_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
					bridge,
					prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
					argument_claim_id,
					&argument_witness_claim,
					&residual_reason
				);
				if (argument_status < 0) {
					return -1;
				}
				if (argument_status > 0) {
					return hott_publish_action_residual(
						actions, terms, request_id, residual_reason, p_result_id
					);
				}
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 ||
					prototype_judgement_add_relation_induction_hypothesis_witness(
						judgement,
						terms,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						request->key.term.source_claim_id,
						argument_witness_claim,
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			const struct prototype_judgement_derivation* lambda_derivation =
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					PROTOTYPE_JUDGEMENT_PROOF_LAMBDA_INTRO
				);
			if (!used_relation_binding && lambda_derivation &&
				prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_LAMBDA) {
				if (lambda_derivation->premise_count != 2) {
					return -1;
				}
				const struct prototype_judgement_claim* binder_claim =
					prototype_judgement_claim_get(
						judgement, lambda_derivation->premises[0].claim_id
					);
				const struct prototype_judgement_claim* body_claim =
					prototype_judgement_claim_get(
						judgement, lambda_derivation->premises[1].claim_id
					);
				const struct prototype_context* body_context = body_claim ?
					prototype_context_get(contexts, prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id) : NULL;
				const struct prototype_cwf_certificate* body_context_proof =
					body_context ? hott_context_formation_certificate_for_context(
						cwf_certificates, prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id
					) : NULL;
				if (!binder_claim || !body_claim || !body_context ||
					!body_context_proof || prototype_judgement_proposition_get(judgement, binder_claim->proposition_id)->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					prototype_judgement_proposition_get(judgement, binder_claim->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id ||
					body_context->parent != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id ||
					body_context->binding_id !=
						terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.lambda.binding_id ||
					body_context_proof->claim_id == PROTOTYPE_INVALID_ID) {
					return -1;
				}
				struct prototype_hott_action_request binder_type_request = {
					.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
					.key.relation_type = {
						.source_claim_id = body_context_proof->claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t binder_type_request_id;
				uint32_t binder_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						binder_type_request, &binder_type_request_id
					) != 0 || prototype_parametricity_execute_relation_type_action(
						actions, kernel, bridges,
						binder_type_request_id, &binder_type_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* binder_type_result =
					prototype_hott_action_result_get(actions, binder_type_result_id);
				if (!binder_type_result) {
					return -1;
				}
				if (binder_type_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						binder_type_result->outcome.residual_reason, p_result_id
					);
				}
				uint32_t body_bridge_id;
				if (prototype_hott_bridge_db_construct_extension(
						bridges,
						kernel,
						actions,
						prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id,
						binder_type_request_id,
						&body_bridge_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_bridge* body_bridge =
					prototype_hott_bridge_db_get(bridges, body_bridge_id);
				uint32_t body_witness_claim;
				int residual_reason;
				int body_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
					body_bridge,
					prototype_judgement_proposition_get(judgement, body_claim->proposition_id)->context_id,
					lambda_derivation->premises[1].claim_id,
					&body_witness_claim,
					&residual_reason
				);
				if (body_status < 0) {
					return -1;
				}
				if (body_status > 0) {
					return hott_publish_action_residual(
						actions, terms, request_id, residual_reason, p_result_id
					);
				}
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 || prototype_judgement_add_relation_lambda_witness(
						judgement,
						terms,
						contexts,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						body_witness_claim,
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			const struct prototype_judgement_derivation* match_derivation =
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					PROTOTYPE_JUDGEMENT_PROOF_MATCH_ELIM
				);
			if (!used_relation_binding && match_derivation &&
				prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_MATCH) {
				const struct prototype_term* source_match =
					&terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject];
				if (source_match->as.match.case_count + 1 !=
						match_derivation->premise_count ||
					source_match->as.match.case_count + 4 >
						PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
					return -1;
				}
				uint32_t scrutinee_claim_id = hott_has_type_claim_for_subject(
					judgement, prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id, source_match->as.match.scrutinee
				);
				if (scrutinee_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				uint32_t scrutinee_witness_claim;
				int residual_reason;
				int scrutinee_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
					bridge,
					prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
					scrutinee_claim_id,
					&scrutinee_witness_claim,
					&residual_reason
				);
				if (scrutinee_status < 0) {
					return -1;
				}
				if (scrutinee_status > 0) {
					return hott_publish_action_residual(
						actions, terms, request_id, residual_reason, p_result_id
					);
				}
				uint32_t case_witness_claims[
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
				];
				for (uint32_t i = 0; i < source_match->as.match.case_count; ++i) {
					uint32_t case_id = source_match->as.match.first_case + i;
					const struct prototype_judgement_claim* case_claim =
						prototype_judgement_claim_get(
							judgement, match_derivation->premises[i + 1].claim_id
						);
					if (case_id >= terms->case_count || !case_claim ||
						prototype_judgement_proposition_get(judgement, case_claim->proposition_id)->subject != terms->cases[case_id].body ||
						!hott_match_case_context_valid(
							contexts,
							prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
							prototype_judgement_proposition_get(judgement, case_claim->proposition_id)->context_id,
							&terms->case_binders[
								terms->cases[case_id].first_binder
							],
							terms->cases[case_id].binder_count
						)) {
						return hott_publish_action_residual(
							actions, terms, request_id,
							PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
							p_result_id
						);
					}
					uint32_t case_bridge_id;
					int case_bridge_status = hott_ensure_bridge_for_context(
						actions,
						kernel,
						bridges,
						prototype_judgement_proposition_get(judgement, case_claim->proposition_id)->context_id,
						&case_bridge_id,
						&residual_reason
					);
					if (case_bridge_status < 0) {
						return -1;
					}
					if (case_bridge_status > 0) {
						return hott_publish_action_residual(
							actions, terms, request_id, residual_reason, p_result_id
						);
					}
					const struct prototype_hott_bridge* case_bridge =
						prototype_hott_bridge_db_get(bridges, case_bridge_id);
					if (!case_bridge) {
						return -1;
					}
					int case_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
						case_bridge,
						prototype_judgement_proposition_get(judgement, case_claim->proposition_id)->context_id,
						match_derivation->premises[i + 1].claim_id,
						&case_witness_claims[i],
						&residual_reason
					);
					if (case_status < 0) {
						return -1;
					}
					if (case_status > 0) {
						return hott_publish_action_residual(
							actions, terms, request_id, residual_reason, p_result_id
						);
					}
				}
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 || prototype_judgement_add_relation_match_witness(
						judgement,
						terms,
						type_declarations,
						contexts,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						request->key.term.source_claim_id,
						scrutinee_witness_claim,
						case_witness_claims,
						source_match->as.match.case_count,
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			const struct prototype_judgement_derivation* app_derivation =
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					PROTOTYPE_JUDGEMENT_PROOF_APP_ELIM
				);
			if (!used_relation_binding && app_derivation &&
				prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_APP) {
				if (app_derivation->premise_count != 2) {
					return -1;
				}
				uint32_t child_witness_claims[2];
				for (uint32_t i = 0; i < 2; ++i) {
					const struct prototype_judgement_claim* child_claim =
						prototype_judgement_claim_get(
							judgement, app_derivation->premises[i].claim_id
						);
					uint32_t expected_subject = i == 0 ?
						terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.app.function :
						terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.app.argument;
					int matches = 0;
					if (!child_claim || prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id ||
						prototype_term_core_shape_equal(
							terms, prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->subject, expected_subject, &matches
						) != 0 || !matches) {
						return -1;
					}
					int residual_reason;
					int child_status = hott_execute_child_term_action(
					actions,
					kernel,
					bridges,
						bridge,
						prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id,
						app_derivation->premises[i].claim_id,
						&child_witness_claims[i],
						&residual_reason
					);
					if (child_status < 0) {
						return -1;
					}
					if (child_status > 0) {
						return hott_publish_action_residual(
							actions,
							terms,
							request_id,
							residual_reason,
							p_result_id
						);
					}
				}
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 || prototype_judgement_add_relation_app_witness(
						judgement,
						terms,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						child_witness_claims[0],
						child_witness_claims[1],
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			int unary_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INVALID;
			int unary_witness_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_INVALID;
			uint32_t unary_payload = PROTOTYPE_INVALID_ID;
			if (prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_RETURN) {
				unary_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_RETURN_INTRO;
				unary_witness_proof_kind =
					PROTOTYPE_JUDGEMENT_PROOF_RELATION_RETURN_WITNESS;
				unary_payload = terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.return_term.value;
			} else if (prototype_judgement_proposition_get(judgement, source->proposition_id)->subject < terms->term_count &&
				terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].tag == PROTOTYPE_TERM_THUNK) {
				unary_proof_kind = PROTOTYPE_JUDGEMENT_PROOF_THUNK_INTRO;
				unary_witness_proof_kind =
					PROTOTYPE_JUDGEMENT_PROOF_RELATION_THUNK_WITNESS;
				unary_payload = terms->terms[prototype_judgement_proposition_get(judgement, source->proposition_id)->subject].as.thunk.computation;
			}
			const struct prototype_judgement_derivation* unary_derivation =
				unary_proof_kind != PROTOTYPE_JUDGEMENT_PROOF_INVALID ?
				hott_derivation_for_claim_and_kind(
					judgement,
					request->key.term.source_claim_id,
					unary_proof_kind
				) : NULL;
			if (!used_relation_binding && unary_derivation) {
				if (unary_derivation->premise_count != 1) {
					return -1;
				}
				uint32_t child_claim_id = unary_derivation->premises[0].claim_id;
				const struct prototype_judgement_claim* child_claim =
					prototype_judgement_claim_get(judgement, child_claim_id);
				uint32_t child_type_claim_id = child_claim ?
					hott_is_type_claim_for_subject(
						judgement, prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id, prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->classifier
					) : PROTOTYPE_INVALID_ID;
				if (!child_claim || prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id ||
					prototype_judgement_proposition_get(judgement, child_claim->proposition_id)->subject != unary_payload ||
					child_type_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				struct prototype_hott_action_request child_type_request = {
					.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
					.key.relation_type = {
						.source_claim_id = child_type_claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t child_type_request_id;
				uint32_t child_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						child_type_request, &child_type_request_id
					) != 0 || prototype_parametricity_execute_relation_type_action(
						actions, kernel, bridges,
						child_type_request_id, &child_type_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* child_type_result =
					prototype_hott_action_result_get(actions, child_type_result_id);
				if (!child_type_result) {
					return -1;
				}
				if (child_type_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						child_type_result->outcome.residual_reason, p_result_id
					);
				}
				struct prototype_hott_action_request child_term_request = {
					.kind = PROTOTYPE_HOTT_ACTION_TERM,
					.key.term = {
						.source_claim_id = child_claim_id,
						.source_bridge_id = bridge->id,
						.relation_type_action_request_id = child_type_request_id
					}
				};
				uint32_t child_term_request_id;
				uint32_t child_term_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						child_term_request, &child_term_request_id
					) != 0 || prototype_hott_execute_term_action(
						actions, kernel, bridges,
						child_term_request_id, &child_term_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* child_term_result =
					prototype_hott_action_result_get(actions, child_term_result_id);
				if (!child_term_result) {
					return -1;
				}
				if (child_term_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						child_term_result->outcome.residual_reason, p_result_id
					);
				}
				const struct prototype_hott_action_certificate* child_certificate =
					&actions->certificates[child_term_result->certificate_id];
				if (prototype_term_relation_witness(
						terms,
						prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
						prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
						&witness
					) != 0 || prototype_judgement_add_relation_unary_witness(
						judgement,
						terms,
						bridge->bridge_context_id,
						witness,
						relation_type,
						relation_claim,
						left_endpoint_claim,
						right_endpoint_claim,
						child_certificate->data.term.witness_has_type_claim_id,
						unary_witness_proof_kind,
						&witness_claim
					) != 0) {
					return -1;
				}
				used_relation_binding = 1;
			}
			const struct prototype_judgement_derivation* constructor_derivation =
				hott_constructor_derivation_for_claim(
					judgement, request->key.term.source_claim_id
				);
			uint32_t source_head;
			uint32_t source_owner;
			uint32_t source_constructor;
			uint32_t source_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
			uint32_t source_argument_count;
			if (!used_relation_binding && (!constructor_derivation ||
				prototype_term_constructor_spine_info(
					terms,
					prototype_judgement_proposition_get(judgement, source->proposition_id)->subject,
					&source_head,
					&source_owner,
					&source_constructor,
					source_arguments,
					PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
					&source_argument_count
				) != 0 || source_argument_count == 0 ||
				constructor_derivation->premise_count != source_argument_count)) {
				return hott_publish_action_residual(
					actions,
					terms,
					request_id,
					PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
					p_result_id
				);
			}
			uint32_t field_witness_claims[
				PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
			];
			for (uint32_t i = 0; !used_relation_binding &&
				i < source_argument_count; ++i) {
				uint32_t field_claim_id =
					constructor_derivation->premises[i].claim_id;
				const struct prototype_judgement_claim* field_claim =
					prototype_judgement_claim_get(judgement, field_claim_id);
				uint32_t field_type_claim_id = field_claim ?
					hott_is_type_claim_for_subject(
						judgement, prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id, prototype_judgement_proposition_get(judgement, field_claim->proposition_id)->classifier
					) : PROTOTYPE_INVALID_ID;
				if (!field_claim || prototype_judgement_proposition_get(judgement, field_claim->proposition_id)->kind !=
						PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
					prototype_judgement_proposition_get(judgement, field_claim->proposition_id)->context_id != prototype_judgement_proposition_get(judgement, source->proposition_id)->context_id ||
					prototype_judgement_proposition_get(judgement, field_claim->proposition_id)->subject != source_arguments[i] ||
					field_type_claim_id == PROTOTYPE_INVALID_ID) {
					return hott_publish_action_residual(
						actions,
						terms,
						request_id,
						PROTOTYPE_HOTT_RESIDUAL_DEFERRED_OBJECT_RULE,
						p_result_id
					);
				}
				struct prototype_hott_action_request field_type_request = {
					.kind = PROTOTYPE_HOTT_ACTION_RELATION_TYPE,
					.key.relation_type = {
						.source_claim_id = field_type_claim_id,
						.source_bridge_id = bridge->id
					}
				};
				uint32_t field_type_request_id;
				uint32_t field_type_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						field_type_request, &field_type_request_id
					) != 0 || prototype_parametricity_execute_relation_type_action(
						actions, kernel, bridges,
						field_type_request_id, &field_type_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* field_type_result =
					prototype_hott_action_result_get(
						actions, field_type_result_id
					);
				if (!field_type_result) {
					return -1;
				}
				if (field_type_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						field_type_result->outcome.residual_reason, p_result_id
					);
				}
				struct prototype_hott_action_request field_term_request = {
					.kind = PROTOTYPE_HOTT_ACTION_TERM,
					.key.term = {
						.source_claim_id = field_claim_id,
						.source_bridge_id = bridge->id,
						.relation_type_action_request_id = field_type_request_id
					}
				};
				uint32_t field_term_request_id;
				uint32_t field_term_result_id;
				if (prototype_hott_action_request_intern(
						actions, &view, bridges,
						field_term_request, &field_term_request_id
					) != 0 || prototype_hott_execute_term_action(
						actions, kernel, bridges,
						field_term_request_id, &field_term_result_id
					) != 0) {
					return -1;
				}
				const struct prototype_hott_action_result* field_term_result =
					prototype_hott_action_result_get(
						actions, field_term_result_id
					);
				if (!field_term_result) {
					return -1;
				}
				if (field_term_result->outcome.state !=
					PROTOTYPE_HOTT_ACTION_RESULT_READY) {
					return hott_publish_action_residual(
						actions, terms, request_id,
						field_term_result->outcome.residual_reason, p_result_id
					);
				}
				const struct prototype_hott_action_certificate* field_certificate =
					&actions->certificates[field_term_result->certificate_id];
				field_witness_claims[i] =
					field_certificate->data.term.witness_has_type_claim_id;
			}
			if (!used_relation_binding && (prototype_term_relation_witness(
					terms,
					prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
					prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
					&witness
				) != 0 || prototype_judgement_add_relation_constructor_witness(
					judgement,
					terms,
					bridge->bridge_context_id,
					witness,
					relation_type,
					relation_claim,
					left_endpoint_claim,
					right_endpoint_claim,
					field_witness_claims,
					source_argument_count,
					&witness_claim
				) != 0)) {
				return -1;
			}
			(void)source_head;
			(void)source_owner;
			(void)source_constructor;
			if (constructor_derivation) {
				used_relation_binding = 1;
			}
		}
		if (!used_relation_binding &&
			(prototype_term_relation_witness(
				terms,
				prototype_judgement_proposition_get(judgement, left_endpoint_evidence->proposition_id)->subject,
				prototype_judgement_proposition_get(judgement, right_endpoint_evidence->proposition_id)->subject,
				&witness
			) != 0 || prototype_judgement_add_relation_witness_intro(
				judgement,
				terms,
				type_declarations,
				bridge->bridge_context_id,
				witness,
				relation_type,
				relation_claim,
				left_endpoint_claim,
				right_endpoint_claim,
				&witness_claim
			) != 0)) {
			return -1;
		}
	}
	struct prototype_hott_action_certificate certificate = {
		.request_id = request_id,
		.kind = PROTOTYPE_HOTT_ACTION_CERTIFICATE_TERM,
		.data.term = {
			.endpoint_instantiation_substitution_id = endpoint_instantiation,
			.left_endpoint_substitution_certificate_id =
				left_extension_certificate,
			.right_endpoint_substitution_certificate_id =
				right_extension_certificate,
			.witness_term_id = witness,
			.witness_has_type_claim_id = witness_claim
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
		PROTOTYPE_TERM_NORMALIZATION_PURE_TYPE_WHNF,
		64,
		p_result_id
	);
}
