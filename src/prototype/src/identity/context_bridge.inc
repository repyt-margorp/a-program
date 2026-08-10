hott_bridge_for_source_context_and_semantics(
	const struct prototype_hott_bridge_db* bridges,
	uint32_t source_context,
	int semantics,
	uint32_t* p_bridge_id
) {
	if (!bridges) {
		return NULL;
	}
	for (uint32_t i = 0; i < bridges->bridge_count; ++i) {
		if (bridges->bridges[i].source_context_id == source_context &&
			bridges->certificates[i].semantics == semantics) {
			if (p_bridge_id) {
				*p_bridge_id = i;
			}
			return &bridges->bridges[i];
		}
	}
	return NULL;
}

static const struct prototype_judgement_derivation*
hott_constructor_derivation_for_claim(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id
) {
	if (!judgement) {
		return NULL;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id == claim_id &&
			(derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_INTRO ||
			 derivation->proof_kind ==
				PROTOTYPE_JUDGEMENT_PROOF_CONSTRUCTOR_SPINE_FORMATION)) {
			return derivation;
		}
	}
	return NULL;
}

static const struct prototype_judgement_derivation*
hott_derivation_for_claim_and_kind(
	const struct prototype_judgement_db* judgement,
	uint32_t claim_id,
	int proof_kind
) {
	if (!judgement) {
		return NULL;
	}
	for (uint32_t i = 0; i < judgement->derivation_count; ++i) {
		const struct prototype_judgement_derivation* derivation =
			&judgement->derivations[i];
		if (derivation->conclusion_claim_id == claim_id &&
			derivation->proof_kind == proof_kind) {
			return derivation;
		}
	}
	return NULL;
}

static uint32_t hott_is_type_claim_for_subject(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject
) {
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_IS_TYPE &&
			prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == context_id && prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == subject) {
			return i;
		}
	}
	return PROTOTYPE_INVALID_ID;
}

static uint32_t hott_has_type_claim_for_subject(
	const struct prototype_judgement_db* judgement,
	uint32_t context_id,
	uint32_t subject
) {
	uint32_t found = PROTOTYPE_INVALID_ID;
	if (!judgement) {
		return PROTOTYPE_INVALID_ID;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_claim* claim = &judgement->claims[i];
		if (prototype_judgement_proposition_get(judgement, claim->proposition_id)->kind == PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE &&
			prototype_judgement_proposition_get(judgement, claim->proposition_id)->context_id == context_id && prototype_judgement_proposition_get(judgement, claim->proposition_id)->subject == subject) {
			if (found != PROTOTYPE_INVALID_ID &&
				prototype_judgement_claim_proposition(
					judgement, found
				)->classifier != prototype_judgement_proposition_get(judgement, claim->proposition_id)->classifier) {
				return PROTOTYPE_INVALID_ID;
			}
			found = i;
		}
	}
	return found;
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
);

static int hott_construct_adt_degeneracy_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	uint32_t generated_type_id,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
);

static int hott_construct_universe_degeneracy(
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_identity_type_computation_certificate* identity,
	uint32_t source_claim_id,
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
	uint32_t empty = prototype_context_empty(contexts);
	if (!terms || !types || !contexts || !substitutions || !cwf || !judgement ||
		!identity || !source || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE || source->context_id != empty ||
		source->classifier >= terms->term_count || terms->terms[
			source->classifier
		].tag != PROTOTYPE_TERM_UNIVERSE_VAR || identity->computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE ||
		identity->generated_type_declaration_id >= types->type_count) {
		return -1;
	}

	uint32_t value_identity_type_id;
	if (prototype_type_declaration_find_generated_identity(
			types,
			source->subject,
			empty,
			&value_identity_type_id
		) != 0 || value_identity_type_id >= types->type_count) {
		return 1;
	}
	int value_identity_rule =
		prototype_type_declaration_generated_identity_rule_for_source(
			terms, source->subject
		);
	if (value_identity_rule == PROTOTYPE_HOTT_IDENTITY_COMPUTATION_INVALID ||
		!prototype_type_declaration_validate_generated_identity(
			terms,
			types,
			contexts,
			source->subject,
			value_identity_type_id,
			value_identity_rule
		)) {
		return 1;
	}

	uint32_t source_is_type_claim = hott_is_type_claim_for_subject(
		judgement, empty, source->subject
	);
	uint32_t source_universe_is_type_claim = hott_is_type_claim_for_subject(
		judgement, empty, source->classifier
	);
	const struct prototype_judgement_proposition* source_universe_is_type =
		prototype_judgement_claim_proposition(
			judgement, source_universe_is_type_claim
		);
	if (source_is_type_claim == PROTOTYPE_INVALID_ID &&
		prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			empty,
			source->subject,
			source->classifier,
			source_claim_id,
			&source_is_type_claim
		) != 0) {
		return -1;
	}
	if (!source_universe_is_type || source_universe_is_type->kind !=
			PROTOTYPE_JUDGEMENT_KIND_IS_TYPE || source_universe_is_type->subject !=
			source->classifier) {
		return 1;
	}
	uint32_t x_binding = prototype_term_new_binding(terms);
	uint32_t x_context;
	uint32_t x_context_certificate;
	uint32_t x;
	uint32_t x_claim;
	if (x_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			empty,
			x_binding,
			source->subject,
			PROTOTYPE_INVALID_ID,
			&x_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			x_context,
			source_is_type_claim,
			&x_context_certificate
		) != 0 || prototype_term_var(
			terms, x_binding, &x
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			x_context,
			x_binding,
			source->subject,
			&x_claim
		) != 0) {
		return -1;
	}

	uint32_t x_to_empty;
	uint32_t source_is_type_in_x;
	uint32_t y_binding = prototype_term_new_binding(terms);
	uint32_t y_context;
	uint32_t y_context_certificate;
	uint32_t y;
	uint32_t y_claim;
	if (y_binding == PROTOTYPE_INVALID_ID ||
		prototype_substitution_projection_path(
			substitutions, contexts, x_context, empty, &x_to_empty
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source_is_type_claim,
			x_to_empty,
			&source_is_type_in_x
		) != 0 || prototype_context_extend(
			contexts,
			x_context,
			y_binding,
			source->subject,
			PROTOTYPE_INVALID_ID,
			&y_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			cwf,
			contexts,
			terms,
			types,
			judgement,
			y_context,
			source_is_type_in_x,
			&y_context_certificate
		) != 0 || prototype_term_var(
			terms, y_binding, &y
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			y_context,
			y_binding,
			source->subject,
			&y_claim
		) != 0) {
		return -1;
	}
	uint32_t relation_body_arguments[2] = { x, y };
	uint32_t relation_body;
	uint32_t relation_body_claim;
	uint32_t relation_inner_classifier;
	uint32_t relation_inner;
	uint32_t relation_inner_claim;
	uint32_t relation_classifier;
	uint32_t relation;
	uint32_t relation_claim;
	if (prototype_term_type_instance_make(
			terms,
			types,
			value_identity_type_id,
			relation_body_arguments,
			2,
			&relation_body
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			y_context,
			relation_body,
			source->classifier,
			&relation_body_claim
		) != 0 || prototype_term_pi(
			terms,
			source->subject,
			source->classifier,
			&relation_inner_classifier
		) != 0 || prototype_term_lambda(
			terms, y_binding, relation_body, &relation_inner
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			x_context,
			relation_inner,
			relation_inner_classifier,
			y_claim,
			relation_body_claim,
			&relation_inner_claim
		) != 0 || prototype_term_pi(
			terms,
			source->subject,
			relation_inner_classifier,
			&relation_classifier
		) != 0 || prototype_term_lambda(
			terms, x_binding, relation_inner, &relation
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			empty,
			relation,
			relation_classifier,
			x_claim,
			relation_inner_claim,
			&relation_claim
		) != 0) {
		return -1;
	}

	uint32_t identity_lambda;
	uint32_t identity_lambda_classifier;
	uint32_t identity_lambda_claim;
	if (prototype_term_lambda(
			terms, x_binding, x, &identity_lambda
		) != 0 || prototype_term_pi(
			terms,
			source->subject,
			source->subject,
			&identity_lambda_classifier
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			empty,
			identity_lambda,
			identity_lambda_classifier,
			x_claim,
			x_claim,
			&identity_lambda_claim
		) != 0) {
		return -1;
	}

	uint32_t value_family_claim;
	uint32_t value_witness;
	uint32_t value_witness_claim;
	int value_status = hott_construct_adt_degeneracy_depth(
		kernel,
		x_claim,
		value_identity_type_id,
		source->classifier,
		(uint32_t)terms->term_count + 1,
		&value_family_claim,
		&value_witness,
		&value_witness_claim
	);
	if (value_status != 0) {
		return value_status;
	}
	const struct prototype_judgement_proposition* value_family =
		prototype_judgement_claim_proposition(judgement, value_family_claim);
	if (!value_family || value_family->context_id != x_context) {
		return -1;
	}
	uint32_t lift_family;
	uint32_t lift_classifier;
	uint32_t lift_lambda;
	uint32_t lift_lambda_claim;
	uint32_t transported_x;
	uint32_t value_relation_x;
	uint32_t lifted_value_family;
	uint32_t lifted_value_witness_claim;
	if (prototype_term_app(
			terms, identity_lambda, x, &transported_x
		) != 0 || prototype_term_app(
			terms, relation, x, &value_relation_x
		) != 0 || prototype_term_app(
			terms, value_relation_x, transported_x, &lifted_value_family
		) != 0 || prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			x_context,
			value_witness,
			lifted_value_family,
			value_witness_claim,
			&lifted_value_witness_claim
		) != 0 || prototype_term_pure_family(
			terms,
			x_binding,
			lifted_value_family,
			&lift_family
		) != 0 || prototype_term_pi_family(
			terms,
			source->subject,
			lift_family,
			&lift_classifier
		) != 0 || prototype_term_lambda(
			terms, x_binding, value_witness, &lift_lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			empty,
			lift_lambda,
			lift_classifier,
			x_claim,
			lifted_value_witness_claim,
			&lift_lambda_claim
		) != 0) {
		return -1;
	}

	uint32_t universe_identity_former;
	uint32_t witness;
	if (prototype_term_type_instance_make(
			terms,
			types,
			identity->generated_type_declaration_id,
			NULL,
			0,
			&universe_identity_former
		) != 0 || prototype_term_constructor(
			terms, universe_identity_former, 0, &witness
		) != 0) {
		return -1;
	}
	uint32_t arguments[7] = {
		source->subject,
		source->subject,
		relation,
		identity_lambda,
		identity_lambda,
		lift_lambda,
		lift_lambda
	};
	uint32_t argument_claims[7] = {
		source_claim_id,
		source_claim_id,
		relation_claim,
		identity_lambda_claim,
		identity_lambda_claim,
		lift_lambda_claim,
		lift_lambda_claim
	};
	for (uint32_t i = 0; i < 7; ++i) {
		if (prototype_term_app(terms, witness, arguments[i], &witness) != 0) {
			return -1;
		}
	}
	uint32_t family_arguments[2] = { source->subject, source->subject };
	uint32_t identity_family;
	uint32_t identity_family_claim;
	uint32_t witness_claim;
	if (prototype_term_type_instance_make(
			terms,
			types,
			identity->generated_type_declaration_id,
			family_arguments,
			2,
			&identity_family
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
				empty,
				identity_family,
				source_universe_is_type->classifier,
				&identity_family_claim
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_constructor_spine_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			empty,
			witness,
			identity_family,
			argument_claims,
			7,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)x_context_certificate;
	(void)y_context_certificate;
	return 0;
}

static int hott_abstract_pure_claim(
	struct prototype_kernel_builder* kernel,
	uint32_t parent_context_id,
	uint32_t binding_id,
	uint32_t domain,
	uint32_t binder_claim_id,
	uint32_t body_claim_id,
	uint32_t* p_lambda_term_id,
	uint32_t* p_lambda_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* binder = judgement ?
		prototype_judgement_claim_proposition(judgement, binder_claim_id) : NULL;
	const struct prototype_judgement_proposition* body = judgement ?
		prototype_judgement_claim_proposition(judgement, body_claim_id) : NULL;
	const struct prototype_context* body_context = body && contexts ?
		prototype_context_get(contexts, body->context_id) : NULL;
	uint32_t binder_term;
	uint32_t codomain_family;
	uint32_t classifier;
	uint32_t lambda;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!binder || !body || !body_context || !p_lambda_term_id ||
		!p_lambda_claim_id || binder->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		body->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		body_context->parent != parent_context_id ||
		body_context->binding_id != binding_id || binder->context_id !=
			body->context_id || binder->classifier != domain ||
		prototype_term_var(terms, binding_id, &binder_term) != 0 ||
		binder->subject != binder_term || prototype_term_pure_family(
			terms, binding_id, body->classifier, &codomain_family
		) != 0 || prototype_term_pi_family(
			terms, domain, codomain_family, &classifier
		) != 0 || prototype_term_lambda(
			terms, binding_id, body->subject, &lambda
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			parent_context_id,
			lambda,
			classifier,
			binder_claim_id,
			body_claim_id,
			p_lambda_claim_id
		) != 0) {
		return -1;
	}
	*p_lambda_term_id = lambda;
	return 0;
}

static int hott_universe_projection_has_variable_boundary(
	const struct prototype_term_db* terms,
	const struct prototype_context_db* contexts,
	const struct prototype_judgement_proposition* source,
	const uint32_t endpoints[2]
) {
	if (!terms || !contexts || !source || source->subject >= terms->term_count ||
		endpoints[0] >= terms->term_count || endpoints[1] >= terms->term_count ||
		terms->terms[source->subject].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[endpoints[0]].tag != PROTOTYPE_TERM_VAR ||
		terms->terms[endpoints[1]].tag != PROTOTYPE_TERM_VAR) {
		return 0;
	}
	uint32_t left_context_id;
	uint32_t right_context_id;
	uint32_t proof_context_id;
	if (prototype_context_find_binding(
			contexts,
			source->context_id,
			terms->terms[endpoints[0]].as.var.binding_id,
			&left_context_id
		) != 0 || prototype_context_find_binding(
			contexts,
			source->context_id,
			terms->terms[endpoints[1]].as.var.binding_id,
			&right_context_id
		) != 0 || prototype_context_find_binding(
			contexts,
			source->context_id,
			terms->terms[source->subject].as.var.binding_id,
			&proof_context_id
		) != 0) {
		return 0;
	}
	const struct prototype_context* left = prototype_context_get(
		contexts, left_context_id
	);
	const struct prototype_context* right = prototype_context_get(
		contexts, right_context_id
	);
	const struct prototype_context* proof = prototype_context_get(
		contexts, proof_context_id
	);
	return left && right && proof && right->parent == left_context_id &&
		proof->parent == right_context_id && proof_context_id == source->context_id;
}

static int hott_construct_general_universe_projection(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t universe_identity_result_id,
	uint32_t correspondence_claim_id,
	uint32_t identity_type_id,
	uint32_t universe,
	const uint32_t endpoints[2],
	int projection,
	uint32_t* p_projection_term_id,
	uint32_t* p_projection_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(
			judgement, correspondence_claim_id
		) : NULL;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !judgement || !source || !p_projection_term_id ||
		!p_projection_claim_id || source->kind !=
			PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}

	uint32_t universe_is_type;
	uint32_t left_binding = prototype_term_new_binding(terms);
	uint32_t left_context;
	uint32_t left_context_certificate;
	uint32_t left;
	uint32_t left_claim;
	if (left_binding == PROTOTYPE_INVALID_ID ||
		hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			universe,
			&universe_is_type
		) != 0 || prototype_context_extend(
			contexts,
			source->context_id,
			left_binding,
			universe,
			PROTOTYPE_INVALID_ID,
			&left_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			left_context,
			universe_is_type,
			&left_context_certificate
		) != 0 || prototype_term_var(
			terms, left_binding, &left
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			left_context,
			left_binding,
			universe,
			&left_claim
		) != 0) {
		return -1;
	}

	uint32_t universe_is_type_left;
	uint32_t right_binding = prototype_term_new_binding(terms);
	uint32_t right_context;
	uint32_t right_context_certificate;
	uint32_t right;
	uint32_t right_claim;
	if (right_binding == PROTOTYPE_INVALID_ID ||
		hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			left_context,
			universe,
			&universe_is_type_left
		) != 0 || prototype_context_extend(
			contexts,
			left_context,
			right_binding,
			universe,
			PROTOTYPE_INVALID_ID,
			&right_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			right_context,
			universe_is_type_left,
			&right_context_certificate
		) != 0 || prototype_term_var(
			terms, right_binding, &right
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			right_context,
			right_binding,
			universe,
			&right_claim
		) != 0) {
		return -1;
	}

	uint32_t generic_arguments[2] = { left, right };
	uint32_t generic_identity;
	uint32_t generic_identity_has_type;
	uint32_t generic_identity_is_type;
	uint32_t proof_binding = prototype_term_new_binding(terms);
	uint32_t proof_context;
	uint32_t proof_context_certificate;
	uint32_t proof;
	uint32_t proof_claim;
	if (proof_binding == PROTOTYPE_INVALID_ID ||
		prototype_term_type_instance_make(
			terms,
			types,
			identity_type_id,
			generic_arguments,
			2,
			&generic_identity
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			right_context,
			generic_identity,
			universe,
			&generic_identity_has_type
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			right_context,
			generic_identity,
			universe,
			generic_identity_has_type,
			&generic_identity_is_type
		) != 0 || prototype_context_extend(
			contexts,
			right_context,
			proof_binding,
			generic_identity,
			PROTOTYPE_INVALID_ID,
			&proof_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			proof_context,
			generic_identity_is_type,
			&proof_context_certificate
		) != 0 || prototype_term_var(
			terms, proof_binding, &proof
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			proof_context,
			proof_binding,
			generic_identity,
			&proof_claim
		) != 0) {
		return -1;
	}
	uint32_t proof_to_left;
	uint32_t proof_to_right;
	uint32_t left_has_type_in_proof;
	uint32_t right_has_type_in_proof;
	uint32_t left_is_type_in_proof;
	uint32_t right_is_type_in_proof;
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			proof_context,
			left_context,
			&proof_to_left
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			proof_context,
			right_context,
			&proof_to_right
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			left_claim,
			proof_to_left,
			&left_has_type_in_proof
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			right_claim,
			proof_to_right,
			&right_has_type_in_proof
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			proof_context,
			left,
			universe,
			left_has_type_in_proof,
			&left_is_type_in_proof
		) != 0 || prototype_judgement_add_is_type_claim(
			judgement,
			terms,
			proof_context,
			right,
			universe,
			right_has_type_in_proof,
			&right_is_type_in_proof
		) != 0) {
		return -1;
	}

	uint32_t generic_projection;
	uint32_t generic_projection_claim;
	if (prototype_hott_construct_universe_correspondence_projection(
			actions,
			kernel,
			bridges,
			universe_identity_result_id,
			proof_claim,
			projection,
			&generic_projection,
			&generic_projection_claim
		) != 0) {
		return -1;
	}
	uint32_t proof_lambda;
	uint32_t proof_lambda_claim;
	uint32_t right_lambda;
	uint32_t right_lambda_claim;
	uint32_t left_lambda;
	uint32_t left_lambda_claim;
	if (hott_abstract_pure_claim(
			kernel,
			right_context,
			proof_binding,
			generic_identity,
			proof_claim,
			generic_projection_claim,
			&proof_lambda,
			&proof_lambda_claim
		) != 0 || hott_abstract_pure_claim(
			kernel,
			left_context,
			right_binding,
			universe,
			right_claim,
			proof_lambda_claim,
			&right_lambda,
			&right_lambda_claim
		) != 0 || hott_abstract_pure_claim(
			kernel,
			source->context_id,
			left_binding,
			universe,
			left_claim,
			right_lambda_claim,
			&left_lambda,
			&left_lambda_claim
		) != 0) {
		return -1;
	}

	uint32_t endpoint_claims[2];
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			endpoints[0],
			universe,
			&endpoint_claims[0]
		) != 0 || prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			endpoints[1],
			universe,
			&endpoint_claims[1]
		) != 0) {
		return -1;
	}
	uint32_t arguments[3] = {
		endpoint_claims[0], endpoint_claims[1], correspondence_claim_id
	};
	if (hott_apply_type_pointwise_identity_witness(
			kernel,
			left_lambda_claim,
			arguments,
			3,
			p_projection_term_id,
			p_projection_claim_id
		) != 0) {
		return -1;
	}
	(void)generic_projection;
	(void)proof_lambda;
	(void)right_lambda;
	(void)left_lambda;
	(void)left_context_certificate;
	(void)right_context_certificate;
	(void)proof_context_certificate;
	(void)left_is_type_in_proof;
	(void)right_is_type_in_proof;
	return 0;
}

int prototype_hott_construct_universe_correspondence_projection(
	const struct prototype_hott_action_db* actions,
	struct prototype_kernel_builder* kernel,
	const struct prototype_hott_bridge_db* bridges,
	uint32_t universe_identity_result_id,
	uint32_t correspondence_claim_id,
	int projection,
	uint32_t* p_projection_term_id,
	uint32_t* p_projection_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* types = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_hott_action_result* result =
		prototype_hott_action_result_get(actions, universe_identity_result_id);
	const struct prototype_hott_action_request* request = result ?
		prototype_hott_action_request_get(actions, result->request_id) : NULL;
	const struct prototype_hott_action_certificate* certificate = result &&
		result->certificate_id < actions->certificate_count ?
		&actions->certificates[result->certificate_id] : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(
			judgement, correspondence_claim_id
		) : NULL;
	struct prototype_kernel_view view;
	if (!actions || !kernel || !bridges || !terms || !types || !contexts ||
		!substitutions || !judgement || !result || !request || !certificate ||
		!source || !p_projection_term_id || !p_projection_claim_id ||
		projection < PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION ||
		projection > PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_LEFT ||
		prototype_kernel_builder_view(kernel, &view) != 0 ||
		prototype_hott_action_db_validate(actions, &view, bridges) != 0 ||
		result->outcome.state != PROTOTYPE_HOTT_ACTION_RESULT_READY ||
		request->kind != PROTOTYPE_HOTT_ACTION_IDENTITY_TYPE_COMPUTATION ||
		certificate->kind !=
			PROTOTYPE_HOTT_ACTION_CERTIFICATE_IDENTITY_TYPE_COMPUTATION ||
		(certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE &&
		 certificate->data.identity_type.computation_rule !=
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_CONSTANT_FAMILY_LIFT) ||
		 source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE) {
		return -1;
	}
	const struct prototype_judgement_derivation* source_reindex =
		hott_derivation_for_claim_and_kind(
			judgement,
			correspondence_claim_id,
			PROTOTYPE_JUDGEMENT_PROOF_SUBSTITUTION_REINDEX
		);
	if (source_reindex) {
		if (source_reindex->premise_count != 1 ||
			source_reindex->semantic_action_kind !=
				PROTOTYPE_JUDGEMENT_SEMANTIC_ACTION_SUBSTITUTION ||
			source_reindex->semantic_action_id == PROTOTYPE_INVALID_ID) {
			return -1;
		}
		uint32_t source_projection;
		uint32_t source_projection_claim;
		int source_projection_status =
			prototype_hott_construct_universe_correspondence_projection(
				actions,
				kernel,
				bridges,
				universe_identity_result_id,
				source_reindex->premises[0].claim_id,
				projection,
				&source_projection,
				&source_projection_claim
			);
		if (source_projection_status != 0) {
			return source_projection_status;
		}
		uint32_t projection_claim;
		if (prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source_projection_claim,
				source_reindex->semantic_action_id,
				&projection_claim
			) != 0) {
			return -1;
		}
		const struct prototype_judgement_proposition* reindexed_projection =
			prototype_judgement_claim_proposition(judgement, projection_claim);
		if (!reindexed_projection || reindexed_projection->kind !=
				PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			reindexed_projection->context_id != source->context_id) {
			return -1;
		}
		*p_projection_term_id = reindexed_projection->subject;
		*p_projection_claim_id = projection_claim;
		(void)source_projection;
		return 0;
	}
	const struct prototype_hott_identity_type_computation_certificate* identity =
		&certificate->data.identity_type;
	uint32_t identity_type_id;
	uint32_t endpoints[2];
	uint32_t endpoint_count;
	if (prototype_term_type_instance_info(
			terms,
			source->classifier,
			&identity_type_id,
			endpoints,
			&endpoint_count
		) != 0 || identity_type_id != identity->generated_type_declaration_id ||
		endpoint_count != 2 || identity_type_id >= types->type_count) {
		return 1;
	}
	const struct prototype_type_declaration* identity_type =
		&types->type_declarations[identity_type_id];
	if (identity_type->constructor_count != 1 ||
		!prototype_type_declaration_validate_generated_identity(
			terms,
			types,
			contexts,
			request->key.identity_type.source_claim_id < judgement->claim_count ?
				prototype_judgement_claim_proposition(
					judgement, request->key.identity_type.source_claim_id
				)->subject : PROTOTYPE_INVALID_ID,
			identity_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_UNIVERSE_CORRESPONDENCE
		)) {
		return 1;
	}
	uint32_t universe = identity_type->origin_source_carrier_term_id;
	if (!hott_universe_projection_has_variable_boundary(
			terms, contexts, source, endpoints
		)) {
		return hott_construct_general_universe_projection(
			actions,
			kernel,
			bridges,
			universe_identity_result_id,
			correspondence_claim_id,
			identity_type_id,
			universe,
			endpoints,
			projection,
			p_projection_term_id,
			p_projection_claim_id
		);
	}
	uint32_t result_classifier;
	uint32_t prepared_result_binder_context = PROTOTYPE_INVALID_ID;
	uint32_t prepared_result_codomain_claim = PROTOTYPE_INVALID_ID;
	if (projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION) {
		uint32_t right_relation;
		if (prototype_term_pi(
				terms, endpoints[1], universe, &right_relation
			) != 0 || prototype_term_pi(
				terms, endpoints[0], right_relation, &result_classifier
			) != 0) {
			return -1;
		}
	} else if (projection <= PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_LEFT &&
		prototype_term_pi(
			terms,
			projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_RIGHT ?
				endpoints[0] : endpoints[1],
			projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_RIGHT ?
				endpoints[1] : endpoints[0],
			&result_classifier
		) != 0) {
		return -1;
	} else if (projection >= PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT) {
		uint32_t relation_projection;
		uint32_t relation_projection_claim;
		uint32_t transport_projection;
		uint32_t transport_projection_claim;
		int transport_kind = projection ==
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_RIGHT :
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_TRANSPORT_LEFT;
		int relation_projection_status =
			prototype_hott_construct_universe_correspondence_projection(
				actions,
				kernel,
				bridges,
				universe_identity_result_id,
				correspondence_claim_id,
				PROTOTYPE_HOTT_UNIVERSE_PROJECT_RELATION,
				&relation_projection,
				&relation_projection_claim
			);
		if (relation_projection_status != 0) {
			return relation_projection_status;
		}
		if (prototype_hott_construct_universe_correspondence_projection(
				actions,
				kernel,
				bridges,
				universe_identity_result_id,
				correspondence_claim_id,
				transport_kind,
				&transport_projection,
				&transport_projection_claim
			) != 0) {
			return -1;
		}
		uint32_t lift_domain = projection ==
			PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ? endpoints[0] : endpoints[1];
		uint32_t lift_domain_has_type;
		uint32_t lift_domain_is_type;
		uint32_t lift_binding = prototype_term_new_binding(terms);
		uint32_t lift_context_certificate;
		uint32_t lift_projection;
		uint32_t relation_in_lift_context;
		uint32_t transport_in_lift_context;
		uint32_t lift_value;
		uint32_t lift_value_claim;
		if (lift_binding == PROTOTYPE_INVALID_ID ||
			prototype_judgement_add_structural_type_formation_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				source->context_id,
				lift_domain,
				universe,
				&lift_domain_has_type
			) != 0 || prototype_judgement_add_is_type_claim(
				judgement,
				terms,
				source->context_id,
				lift_domain,
				universe,
				lift_domain_has_type,
				&lift_domain_is_type
			) != 0 || prototype_context_extend(
				contexts,
				source->context_id,
				lift_binding,
				lift_domain,
				PROTOTYPE_INVALID_ID,
				&prepared_result_binder_context
			) != 0 || prototype_cwf_certificate_db_add_context(
				kernel->cwf_certificates,
				contexts,
				terms,
				types,
				judgement,
				prepared_result_binder_context,
				lift_domain_is_type,
				&lift_context_certificate
			) != 0 || prototype_substitution_projection_path(
				substitutions,
				contexts,
				prepared_result_binder_context,
				source->context_id,
				&lift_projection
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				relation_projection_claim,
				lift_projection,
				&relation_in_lift_context
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				transport_projection_claim,
				lift_projection,
				&transport_in_lift_context
			) != 0 || prototype_term_var(
				terms, lift_binding, &lift_value
			) != 0 || prototype_judgement_add_context_binding_assumption(
				judgement,
				terms,
				contexts,
				prepared_result_binder_context,
				lift_binding,
				lift_domain,
				&lift_value_claim
			) != 0) {
			return -1;
		}
		uint32_t transported;
		uint32_t transported_claim;
		uint32_t relation_at_first;
		uint32_t relation_at_first_claim;
		uint32_t relation_at;
		if (prototype_term_app(
				terms, transport_projection, lift_value, &transported
			) != 0 || prototype_judgement_add_app_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				prepared_result_binder_context,
				transported,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					endpoints[1] : endpoints[0],
				transport_in_lift_context,
				lift_value_claim,
				&transported_claim
			) != 0 || prototype_term_app(
				terms,
				relation_projection,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					lift_value : transported,
				&relation_at_first
			) != 0) {
			return -1;
		}
		uint32_t relation_second_classifier;
		if (prototype_term_pi(
				terms, endpoints[1], universe, &relation_second_classifier
			) != 0) {
			return -1;
		}
		/* Rebuild the first application Claim with its actual codomain. */
		if (prototype_judgement_add_app_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				prepared_result_binder_context,
				relation_at_first,
				relation_second_classifier,
				relation_in_lift_context,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					lift_value_claim : transported_claim,
				&relation_at_first_claim
			) != 0 || prototype_term_app(
				terms,
				relation_at_first,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					transported : lift_value,
				&relation_at
			) != 0 || prototype_judgement_add_app_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				prepared_result_binder_context,
				relation_at,
				universe,
				relation_at_first_claim,
				projection == PROTOTYPE_HOTT_UNIVERSE_PROJECT_LIFT_RIGHT ?
					transported_claim : lift_value_claim,
				&prepared_result_codomain_claim
			) != 0) {
			return -1;
		}
		uint32_t result_family;
		if (prototype_term_pure_family(
				terms, lift_binding, relation_at, &result_family
			) != 0 || prototype_term_pi_family(
				terms, lift_domain, result_family, &result_classifier
			) != 0) {
			return -1;
		}
		(void)lift_context_certificate;
	}
	for (uint32_t i = 0; i < judgement->claim_count; ++i) {
		const struct prototype_judgement_proposition* existing =
			prototype_judgement_claim_proposition(judgement, i);
		const struct prototype_term* existing_term = existing &&
			existing->subject < terms->term_count ?
			&terms->terms[existing->subject] : NULL;
		if (!existing || existing->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			!existing_term || existing_term->tag != PROTOTYPE_TERM_MATCH ||
			existing_term->as.match.case_count != 1 ||
			existing_term->as.match.first_case >= terms->case_count) {
			continue;
		}
		const struct prototype_match_case* existing_case = &terms->cases[
			existing_term->as.match.first_case
		];
		uint32_t existing_owner_type;
		uint32_t existing_owner_arguments[2];
		uint32_t existing_owner_argument_count;
		if (prototype_term_type_instance_info(
				terms,
				existing_case->constructor_owner,
				&existing_owner_type,
				existing_owner_arguments,
				&existing_owner_argument_count
			) != 0 || existing_owner_type != identity_type_id ||
			existing_owner_argument_count != 2 || existing_case->constructor_id != 0 ||
			existing_case->binder_count != 7 ||
			existing_case->first_binder + projection >= terms->case_binder_count ||
			existing_case->body >= terms->term_count ||
			terms->terms[existing_case->body].tag != PROTOTYPE_TERM_VAR ||
			terms->terms[existing_case->body].as.var.binding_id !=
				terms->case_binders[
					existing_case->first_binder + projection
				].binding_id) {
			continue;
		}
		if (existing->context_id == source->context_id &&
			existing->classifier == result_classifier &&
			existing_term->as.match.scrutinee == source->subject) {
			*p_projection_term_id = existing->subject;
			*p_projection_claim_id = i;
			return 0;
		}
	}

	uint32_t identity_former;
	if (prototype_term_type_instance_make(
			terms, types, identity_type_id, NULL, 0, &identity_former
		) != 0) {
		return -1;
	}
	struct prototype_case_binder proposed_binders[7];
	uint32_t proposed_fields[7];
	for (uint32_t i = 0; i < 7; ++i) {
		proposed_binders[i] = (struct prototype_case_binder) {
			.binding_id = prototype_term_new_binding(terms),
			.is_recursive = 0
		};
		if (proposed_binders[i].binding_id == PROTOTYPE_INVALID_ID ||
			prototype_term_var(
				terms, proposed_binders[i].binding_id, &proposed_fields[i]
			) != 0) {
			return -1;
		}
	}
	struct prototype_match_case_input proposed_case = {
		.case_label_symbol_id = -1,
		.constructor_owner = source->classifier,
		.constructor_id = 0,
		.binders = proposed_binders,
		.binder_count = 7,
		.body = proposed_fields[projection]
	};
	uint32_t match;
	if (prototype_term_match(
			terms, source->subject, &proposed_case, 1, &match
		) != 0 || match >= terms->term_count ||
		terms->terms[match].tag != PROTOTYPE_TERM_MATCH ||
		terms->terms[match].as.match.case_count != 1 ||
		terms->terms[match].as.match.first_case >= terms->case_count) {
		return -1;
	}
	const struct prototype_match_case* canonical_case = &terms->cases[
		terms->terms[match].as.match.first_case
	];
	if (canonical_case->constructor_id != 0 ||
		canonical_case->binder_count != 7 ||
		canonical_case->first_binder > terms->case_binder_count ||
		canonical_case->binder_count > terms->case_binder_count -
			canonical_case->first_binder) {
		return -1;
	}
	struct prototype_case_binder binders[7];
	uint32_t fields[7];
	uint32_t field_classifiers[7];
	uint32_t branch_context = source->context_id;
	for (uint32_t i = 0; i < 7; ++i) {
		uint32_t next_context;
		binders[i] = terms->case_binders[canonical_case->first_binder + i];
		if (binders[i].is_recursive ||
			prototype_judgement_constructor_field_classifier(
				terms,
				types,
				contexts,
				substitutions,
				branch_context,
				identity_former,
				0,
				binders,
				i,
				i,
				&field_classifiers[i]
			) != 0 || prototype_context_extend(
				contexts,
				branch_context,
				binders[i].binding_id,
				field_classifiers[i],
				PROTOTYPE_INVALID_ID,
				&next_context
			) != 0 || prototype_term_var(
				terms, binders[i].binding_id, &fields[i]
			) != 0) {
			return -1;
		}
		branch_context = next_context;
	}
	if (canonical_case->body != fields[projection]) {
		return -1;
	}
	uint32_t selected_claim;
	if (prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			branch_context,
			binders[projection].binding_id,
			field_classifiers[projection],
			&selected_claim
		) != 0) {
		return -1;
	}
	uint32_t refined_context;
	uint32_t refinement_substitution;
	uint32_t constructor_term;
	uint32_t refined_selected_claim;
	if (prototype_judgement_indexed_branch_refinement(
			contexts,
			substitutions,
			terms,
			types,
			source->context_id,
			source->subject,
			source->classifier,
			0,
			branch_context,
			binders,
			7,
			&refined_context,
			&refinement_substitution,
			&constructor_term
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			selected_claim,
			refinement_substitution,
			&refined_selected_claim
		) != 0) {
		return -1;
	}

	uint32_t source_identity_is_type;
	if (hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			source->classifier,
			&source_identity_is_type
		) != 0) {
		return -1;
	}
	uint32_t motive_binding = prototype_term_new_binding(terms);
	uint32_t motive_context;
	uint32_t motive_context_certificate;
	uint32_t motive_body_claim;
	uint32_t motive_binder_claim;
	uint32_t motive_projection;
	uint32_t result_classifier_is_type;
	uint32_t result_domain;
	uint32_t result_family;
	uint32_t result_binder;
	uint32_t result_codomain;
	uint32_t result_codomain_is_type;
	uint32_t result_binder_context;
	uint32_t result_binder_projection;
	uint32_t result_codomain_in_binder;
	uint32_t motive;
	uint32_t motive_classifier;
	uint32_t motive_claim;
	if (motive_binding == PROTOTYPE_INVALID_ID ||
		prototype_judgement_pi_parts(
			terms, result_classifier, &result_domain, &result_family
		) != 0 || prototype_term_pure_family_parts(
			terms, result_family, &result_binder, &result_codomain
		) != 0) {
		return -1;
	}
	if (prepared_result_binder_context == PROTOTYPE_INVALID_ID) {
		if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			result_codomain,
			universe,
			&result_codomain_is_type
		) != 0 || prototype_context_extend(
			contexts,
			source->context_id,
			result_binder,
			result_domain,
			PROTOTYPE_INVALID_ID,
			&result_binder_context
		) != 0 || prototype_substitution_projection_path(
			substitutions,
			contexts,
			result_binder_context,
			source->context_id,
			&result_binder_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			result_codomain_is_type,
			result_binder_projection,
			&result_codomain_in_binder
		) != 0) {
			return -1;
		}
	} else {
		uint32_t result_binder_value;
		uint32_t prepared_binder_substitution;
		if (prototype_context_extend(
				contexts,
				source->context_id,
				result_binder,
				result_domain,
				PROTOTYPE_INVALID_ID,
				&result_binder_context
			) != 0 || prototype_substitution_projection_path(
				substitutions,
				contexts,
				result_binder_context,
				source->context_id,
				&result_binder_projection
			) != 0 || prototype_term_var(
				terms, result_binder, &result_binder_value
			) != 0 || prototype_substitution_extend(
				substitutions,
				contexts,
				terms,
				types,
				result_binder_projection,
				prepared_result_binder_context,
				result_binder_value,
				result_domain,
				&prepared_binder_substitution
			) != 0 || prototype_judgement_add_reindexed_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				prepared_result_codomain_claim,
				prepared_binder_substitution,
				&result_codomain_in_binder
			) != 0 || prototype_judgement_claim_proposition(
				judgement, result_codomain_in_binder
			)->subject != result_codomain) {
			return -1;
		}
	}
	if (prototype_judgement_add_structural_type_formation_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			result_classifier,
			universe,
			&result_classifier_is_type
		) != 0) {
		return -1;
	}
	if (prototype_context_extend(
			contexts,
			source->context_id,
			motive_binding,
			source->classifier,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0) {
		return -1;
	}
	if (prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			motive_context,
			source_identity_is_type,
			&motive_context_certificate
		) != 0) {
		return -1;
	}
	if (prototype_substitution_projection_path(
			substitutions,
			contexts,
			motive_context,
			source->context_id,
			&motive_projection
		) != 0 || prototype_judgement_add_reindexed_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			result_classifier_is_type,
			motive_projection,
			&motive_body_claim
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			motive_context,
			motive_binding,
			source->classifier,
			&motive_binder_claim
		) != 0) {
		return -1;
	}
	if (prototype_term_lambda(
			terms, motive_binding, result_classifier, &motive
		) != 0 || prototype_term_pi(
			terms, source->classifier, universe, &motive_classifier
		) != 0) {
		return -1;
	}
	if (prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
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
			terms, motive, source->subject, &match_classifier
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			match_classifier,
			universe,
			motive_claim,
			correspondence_claim_id,
			&match_classifier_claim
		) != 0) {
		return -1;
	}
	uint32_t match_claim;
	uint32_t projection_claim;
	if (prototype_judgement_add_indexed_match_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			kernel->operations,
			source->context_id,
			match,
			match_classifier,
			match_classifier_claim,
			&refined_selected_claim,
			&refinement_substitution,
			1,
			&match_claim
		) != 0 || prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			source->context_id,
			match,
			result_classifier,
			match_claim,
			&projection_claim
		) != 0) {
		return -1;
	}
	*p_projection_term_id = match;
	*p_projection_claim_id = projection_claim;
	(void)constructor_term;
	(void)motive_context_certificate;
	return 0;
}

static int hott_construct_adt_degeneracy_match(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	uint32_t generated_type_id,
	uint32_t universe,
	uint32_t identity_family,
	uint32_t identity_family_claim,
	uint32_t remaining_depth,
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
	uint32_t source_type_id;
	uint32_t source_type_arguments[16];
	uint32_t source_type_argument_count;
	if (!terms || !types || !contexts || !substitutions || !judgement ||
		!kernel->cwf_certificates || !source || !p_witness_term_id ||
		!p_witness_claim_id || remaining_depth == 0 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		generated_type_id >= types->type_count ||
		prototype_term_type_instance_info(
			terms,
			source->classifier,
			&source_type_id,
			source_type_arguments,
			&source_type_argument_count
		) != 0 || source_type_id >= types->type_count ||
		source_type_argument_count != 0) {
		return 1;
	}
	const struct prototype_type_declaration* source_type =
		&types->type_declarations[source_type_id];
	if (source_type->constructor_count == 0 ||
		source_type->constructor_count > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
		return 1;
	}

	uint32_t source_type_claim;
	if (hott_ensure_is_type_claim_in_context(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			source->classifier,
			&source_type_claim
		) != 0) {
		return 1;
	}
	uint32_t motive_binding = prototype_term_new_binding(terms);
	uint32_t motive_context;
	uint32_t motive_context_certificate;
	uint32_t motive_var;
	if (motive_binding == PROTOTYPE_INVALID_ID || prototype_context_extend(
			contexts,
			source->context_id,
			motive_binding,
			source->classifier,
			PROTOTYPE_INVALID_ID,
			&motive_context
		) != 0 || prototype_cwf_certificate_db_add_context(
			kernel->cwf_certificates,
			contexts,
			terms,
			types,
			judgement,
			motive_context,
			source_type_claim,
			&motive_context_certificate
		) != 0 || prototype_term_var(
			terms, motive_binding, &motive_var
		) != 0) {
		return -1;
	}
	uint32_t motive_body_arguments[2] = { motive_var, motive_var };
	uint32_t motive_body;
	uint32_t motive_body_claim;
	uint32_t motive_binder_claim;
	uint32_t motive;
	uint32_t motive_pi;
	uint32_t motive_claim;
	if (prototype_term_type_instance_make(
			terms,
			types,
			generated_type_id,
			motive_body_arguments,
			2,
			&motive_body
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement,
			terms,
			types,
			motive_context,
			motive_body,
			universe,
			&motive_body_claim
		) != 0 || prototype_judgement_add_context_binding_assumption(
			judgement,
			terms,
			contexts,
			motive_context,
			motive_binding,
			source->classifier,
			&motive_binder_claim
		) != 0 || prototype_term_lambda(
			terms, motive_binding, motive_body, &motive
		) != 0 || prototype_term_pi(
			terms, source->classifier, universe, &motive_pi
		) != 0 || prototype_judgement_add_lambda_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			motive,
			motive_pi,
			motive_binder_claim,
			motive_body_claim,
			&motive_claim
		) != 0) {
		return -1;
	}
	uint32_t match_classifier;
	uint32_t match_classifier_claim;
	if (prototype_term_app(
			terms, motive, source->subject, &match_classifier
		) != 0 || prototype_judgement_add_app_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			source->context_id,
			match_classifier,
			universe,
			motive_claim,
			source_claim_id,
			&match_classifier_claim
		) != 0) {
		return -1;
	}
	struct prototype_match_case_input cases[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	];
	uint32_t branch_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_contexts[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_families[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_argument_counts[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t branch_argument_claims[
		PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES
	][PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	struct prototype_case_binder case_binders[256];
	uint32_t case_binder_cursor = 0;
	struct hott_pending_identity_ih {
		uint32_t branch_index;
		uint32_t field_index;
		uint32_t argument_claim_index;
		uint32_t context_id;
		uint32_t term;
		uint32_t classifier;
	};
	struct hott_pending_identity_ih pending_ih[256];
	uint32_t pending_ih_count = 0;
	uint32_t ih_scope = PROTOTYPE_INVALID_ID;
	uint32_t identity_former;
	if (prototype_term_type_instance_make(
			terms, types, generated_type_id, NULL, 0, &identity_former
		) != 0) {
		return -1;
	}
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t declaration_id = source_type->first_constructor + i;
		const struct prototype_type_constructor_declaration* constructor =
			declaration_id < types->constructor_count ?
			&types->constructor_declarations[declaration_id] : NULL;
		const struct prototype_context* parameter_context = constructor ?
			prototype_context_get(contexts, constructor->parameter_context) : NULL;
		const struct prototype_context* field_context = constructor ?
			prototype_context_get(contexts, constructor->field_context) : NULL;
		if (!constructor || !parameter_context || !field_context ||
			field_context->depth < parameter_context->depth) {
			return -1;
		}
		uint32_t field_count = field_context->depth - parameter_context->depth;
		if (case_binder_cursor + field_count > 256 ||
			field_count * 3 > PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES) {
			return 1;
		}
		struct prototype_case_binder* branch_binders =
			&case_binders[case_binder_cursor];
		uint32_t endpoint;
		uint32_t branch_witness = PROTOTYPE_INVALID_ID;
		if (prototype_term_constructor(
				terms, source->classifier, i, &endpoint
			) != 0 || (field_count > 0 && prototype_term_constructor(
				terms,
				identity_former,
				i,
				&branch_witness
			) != 0)) {
			return -1;
		}
		uint32_t branch_context = source->context_id;
		uint32_t argument_count = 0;
		for (uint32_t j = 0; j < field_count; ++j) {
			uint32_t field_classifier;
			uint32_t field_type_claim;
			uint32_t field_binding = prototype_term_new_binding(terms);
			if (field_binding == PROTOTYPE_INVALID_ID ||
				prototype_judgement_constructor_field_classifier(
					terms,
					types,
					contexts,
					substitutions,
					branch_context,
					source->classifier,
					i,
					branch_binders,
					j,
					j,
					&field_classifier
				) != 0) {
				return 1;
			}
			int type_status = hott_ensure_is_type_claim_in_context(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				branch_context,
				field_classifier,
				&field_type_claim
			);
			if (type_status != 0) {
				uint32_t field_has_type_claim;
				if (prototype_judgement_add_type_formation_claim(
						judgement,
						terms,
						types,
						branch_context,
						field_classifier,
						universe,
						&field_has_type_claim
					) != 0 || prototype_judgement_add_is_type_claim(
						judgement,
						terms,
						branch_context,
						field_classifier,
						universe,
						field_has_type_claim,
						&field_type_claim
					) != 0) {
					return 1;
				}
			}
			int recursive = prototype_judgement_classifier_conversion(
				terms, types, field_classifier, source->classifier
			).status == PROTOTYPE_TERM_CONVERSION_EQUAL;
			branch_binders[j] = (struct prototype_case_binder) {
				.binding_id = field_binding,
				.is_recursive = recursive
			};
			uint32_t next_context;
			uint32_t context_certificate;
			if (prototype_context_extend(
					contexts,
					branch_context,
					field_binding,
					field_classifier,
					PROTOTYPE_INVALID_ID,
					&next_context
				) != 0 || prototype_cwf_certificate_db_add_context(
					kernel->cwf_certificates,
					contexts,
					terms,
					types,
					judgement,
					next_context,
					field_type_claim,
					&context_certificate
				) != 0) {
				return -1;
			}
			branch_context = next_context;
			uint32_t field_claim;
			uint32_t field;
			if (prototype_judgement_add_context_binding_assumption(
					judgement,
					terms,
					contexts,
					branch_context,
					field_binding,
					field_classifier,
					&field_claim
				) != 0 || prototype_term_var(
					terms, field_binding, &field
				) != 0 || prototype_term_app(
					terms, endpoint, field, &endpoint
				) != 0 || prototype_term_app(
					terms, branch_witness, field, &branch_witness
				) != 0 || prototype_term_app(
					terms, branch_witness, field, &branch_witness
				) != 0) {
				return -1;
			}
			branch_argument_claims[i][argument_count++] = field_claim;
			branch_argument_claims[i][argument_count++] = field_claim;
			uint32_t field_identity_family_arguments[2] = { field, field };
			uint32_t field_identity_family;
			uint32_t field_identity_witness;
			if (recursive) {
				if (ih_scope == PROTOTYPE_INVALID_ID) {
					ih_scope = prototype_term_new_ih_scope(terms);
				}
				if (ih_scope == PROTOTYPE_INVALID_ID ||
					pending_ih_count >= 256 || prototype_term_type_instance_make(
						terms,
						types,
						generated_type_id,
						field_identity_family_arguments,
						2,
						&field_identity_family
					) != 0 || prototype_term_induction_hypothesis(
						terms, ih_scope, field, &field_identity_witness
					) != 0) {
					return -1;
				}
				pending_ih[pending_ih_count++] =
					(struct hott_pending_identity_ih) {
						.branch_index = i,
						.field_index = j,
						.argument_claim_index = argument_count,
						.context_id = branch_context,
						.term = field_identity_witness,
						.classifier = field_identity_family
					};
				branch_argument_claims[i][argument_count++] =
					PROTOTYPE_INVALID_ID;
			} else {
				uint32_t field_generated_type_id;
				uint32_t field_identity_family_claim;
				uint32_t field_identity_witness_claim;
				if (prototype_type_declaration_find_generated_identity(
						types,
						field_classifier,
						prototype_context_empty(contexts),
						&field_generated_type_id
					) != 0) {
					return 1;
				}
				int status = hott_construct_adt_degeneracy_depth(
					kernel,
					field_claim,
					field_generated_type_id,
					universe,
					remaining_depth - 1,
					&field_identity_family_claim,
					&field_identity_witness,
					&field_identity_witness_claim
				);
				if (status != 0) {
					return status;
				}
				branch_argument_claims[i][argument_count++] =
					field_identity_witness_claim;
				(void)field_identity_family_claim;
			}
			if (prototype_term_app(
					terms,
					branch_witness,
					field_identity_witness,
					&branch_witness
				) != 0) {
				return -1;
			}
			(void)context_certificate;
		}
		uint32_t branch_arguments[2] = { endpoint, endpoint };
		uint32_t branch_family;
		if (prototype_term_type_instance_make(
				terms,
				types,
				generated_type_id,
				branch_arguments,
				2,
				&branch_family
			) != 0) {
			return -1;
		}
		if (field_count == 0 && prototype_term_constructor(
				terms, branch_family, i, &branch_witness
			) != 0) {
			return -1;
		}
		cases[i] = (struct prototype_match_case_input) {
			.case_label_symbol_id = -1,
			.constructor_owner = source->classifier,
			.constructor_id = i,
			.binders = field_count == 0 ? NULL : branch_binders,
			.binder_count = field_count,
			.body = branch_witness
		};
		branch_contexts[i] = branch_context;
		branch_families[i] = branch_family;
		branch_argument_counts[i] = argument_count;
		branch_claims[i] = PROTOTYPE_INVALID_ID;
		case_binder_cursor += field_count;
	}
	uint32_t witness;
	uint32_t match_claim;
	uint32_t witness_claim;
	int match_status = ih_scope == PROTOTYPE_INVALID_ID ? prototype_term_match(
		terms,
		source->subject,
		cases,
		source_type->constructor_count,
		&witness
	) : prototype_term_match_with_ih_scope(
		terms,
		source->subject,
		cases,
		source_type->constructor_count,
		ih_scope,
		&witness
	);
	if (match_status != 0 || (ih_scope != PROTOTYPE_INVALID_ID &&
		prototype_term_set_ih_scope_term(terms, ih_scope, witness) != 0)) {
		return -1;
	}
	const struct prototype_term* stored_match = &terms->terms[witness];
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		uint32_t case_id = stored_match->as.match.first_case + i;
		if (case_id >= terms->case_count ||
			terms->cases[case_id].binder_count != cases[i].binder_count ||
			terms->cases[case_id].body != cases[i].body) {
			return 1;
		}
		for (uint32_t j = 0; j < cases[i].binder_count; ++j) {
			if (terms->case_binders[
					terms->cases[case_id].first_binder + j
				].binding_id != cases[i].binders[j].binding_id) {
				return 1;
			}
		}
	}
	for (uint32_t i = 0; i < pending_ih_count; ++i) {
		uint32_t claim;
		if (prototype_judgement_add_induction_hypothesis_claim(
				judgement,
				terms,
				types,
				pending_ih[i].context_id,
				pending_ih[i].term,
				pending_ih[i].classifier,
				witness,
				motive,
				pending_ih[i].branch_index,
				pending_ih[i].field_index,
				&claim
			) != 0) {
			return -1;
		}
		branch_argument_claims[
			pending_ih[i].branch_index
		][pending_ih[i].argument_claim_index] = claim;
	}
	for (uint32_t i = 0; i < source_type->constructor_count; ++i) {
		for (uint32_t j = 0; j < branch_argument_counts[i]; ++j) {
			uint32_t claim_id = branch_argument_claims[i][j];
			const struct prototype_judgement_proposition* proposition =
				prototype_judgement_claim_proposition(judgement, claim_id);
			if (!proposition) {
				return -1;
			}
			if (proposition->context_id != branch_contexts[i]) {
				uint32_t projection;
				uint32_t reindexed;
				if (prototype_substitution_projection_path(
						substitutions,
						contexts,
						branch_contexts[i],
						proposition->context_id,
						&projection
					) != 0 || prototype_judgement_add_reindexed_claim(
						judgement,
						terms,
						types,
						contexts,
						substitutions,
						claim_id,
						projection,
						&reindexed
					) != 0) {
					return -1;
				}
				branch_argument_claims[i][j] = reindexed;
			}
		}
		if (branch_argument_counts[i] == 0) {
			if (prototype_judgement_add_constructor_intro_claim(
					judgement,
					terms,
					types,
					branch_contexts[i],
					cases[i].body,
					branch_families[i],
					&branch_claims[i]
				) != 0) {
				return -1;
			}
		} else if (prototype_judgement_add_constructor_spine_claim(
				judgement,
				terms,
				types,
				contexts,
				substitutions,
				branch_contexts[i],
				cases[i].body,
				branch_families[i],
				branch_argument_claims[i],
				branch_argument_counts[i],
				&branch_claims[i]
			) != 0) {
			return -1;
		}
	}
	if (prototype_judgement_add_match_claim(
			judgement,
			terms,
			types,
			contexts,
			substitutions,
			kernel->operations,
			source->context_id,
			witness,
			match_classifier,
			match_classifier_claim,
			branch_claims,
			source_type->constructor_count,
			&match_claim
		) != 0 || prototype_judgement_add_conversion_claim(
			judgement,
			terms,
			types,
			source->context_id,
			witness,
			identity_family,
			match_claim,
			&witness_claim
		) != 0) {
		return -1;
	}
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)identity_family_claim;
	(void)motive_context_certificate;
	return 0;
}

static int hott_construct_adt_degeneracy_depth(
	struct prototype_kernel_builder* kernel,
	uint32_t source_claim_id,
	uint32_t generated_type_id,
	uint32_t universe,
	uint32_t remaining_depth,
	uint32_t* p_identity_family_claim_id,
	uint32_t* p_witness_term_id,
	uint32_t* p_witness_claim_id
) {
	struct prototype_term_db* terms = kernel ? kernel->terms : NULL;
	struct prototype_type_declaration_db* type_declarations = kernel ?
		kernel->type_declarations : NULL;
	struct prototype_context_db* contexts = kernel ? kernel->contexts : NULL;
	struct prototype_substitution_db* substitutions = kernel ?
		kernel->substitutions : NULL;
	struct prototype_judgement_db* judgement = kernel ? kernel->judgement : NULL;
	const struct prototype_judgement_proposition* source = judgement ?
		prototype_judgement_claim_proposition(judgement, source_claim_id) : NULL;
	if (!terms || !type_declarations || !contexts || !substitutions ||
		!judgement || !source || !p_identity_family_claim_id ||
		!p_witness_term_id || !p_witness_claim_id || remaining_depth == 0 ||
		source->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
		source->subject >= terms->term_count ||
		source->classifier >= terms->term_count || universe >= terms->term_count ||
		generated_type_id >= type_declarations->type_count ||
		!prototype_type_declaration_validate_generated_identity(
			terms,
			type_declarations,
			contexts,
			source->classifier,
			generated_type_id,
			PROTOTYPE_HOTT_IDENTITY_COMPUTATION_ORDINARY_ADT
		)) {
		return -1;
	}
	uint32_t head;
	uint32_t owner;
	uint32_t constructor_index;
	uint32_t source_arguments[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t source_argument_count;
	int source_is_constructor = prototype_term_constructor_spine_info(
			terms,
			source->subject,
			&head,
			&owner,
			&constructor_index,
			source_arguments,
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES,
			&source_argument_count
		) == 0;
	uint32_t generated_constructor_index = PROTOTYPE_INVALID_ID;
	if (source_is_constructor &&
		(hott_generated_constructor_index_for_source_ordinal(
			terms,
			type_declarations,
			generated_type_id,
			source->classifier,
			constructor_index,
			&generated_constructor_index
		) != 0 || source_argument_count * 3 >
			PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES)) {
		return 1;
	}
	uint32_t identity_arguments[2] = { source->subject, source->subject };
	uint32_t identity_family;
	uint32_t identity_family_claim;
	if (prototype_term_type_instance_make(
			terms, type_declarations, generated_type_id,
			identity_arguments, 2, &identity_family
		) != 0 || prototype_judgement_add_type_formation_claim(
			judgement, terms, type_declarations, source->context_id,
			identity_family, universe, &identity_family_claim
		) != 0) {
		return -1;
	}
	if (!source_is_constructor) {
		uint32_t witness;
		uint32_t witness_claim;
		int status = hott_construct_adt_degeneracy_match(
			kernel,
			source_claim_id,
			generated_type_id,
			universe,
			identity_family,
			identity_family_claim,
			remaining_depth,
			&witness,
			&witness_claim
		);
		if (status == 0) {
			*p_identity_family_claim_id = identity_family_claim;
			*p_witness_term_id = witness;
			*p_witness_claim_id = witness_claim;
		}
		return status;
	}
	uint32_t witness;
	if (source_argument_count == 0) {
		uint32_t witness_claim;
		if (prototype_term_constructor(
				terms, identity_family, generated_constructor_index, &witness
			) != 0 || prototype_judgement_add_constructor_intro_claim(
				judgement, terms, type_declarations, source->context_id,
				witness, identity_family, &witness_claim
			) != 0) {
			return -1;
		}
		*p_identity_family_claim_id = identity_family_claim;
		*p_witness_term_id = witness;
		*p_witness_claim_id = witness_claim;
		(void)head;
		(void)owner;
		return 0;
	}
	const struct prototype_judgement_derivation* source_derivation =
		hott_constructor_derivation_for_claim(judgement, source_claim_id);
	if (!source_derivation || source_derivation->premise_count !=
		source_argument_count) {
		return 1;
	}
	uint32_t identity_former;
	if (prototype_term_type_instance_make(
			terms, type_declarations, generated_type_id, NULL, 0, &identity_former
		) != 0 || prototype_term_constructor(
			terms, identity_former, generated_constructor_index, &witness
		) != 0) {
		return -1;
	}
	uint32_t argument_claims[PROTOTYPE_JUDGEMENT_PROOF_MAX_PREMISES];
	uint32_t argument_count = 0;
	for (uint32_t i = 0; i < source_argument_count; ++i) {
		uint32_t field_claim_id = source_derivation->premises[i].claim_id;
		const struct prototype_judgement_proposition* field =
			prototype_judgement_claim_proposition(judgement, field_claim_id);
		if (!field || field->kind != PROTOTYPE_JUDGEMENT_KIND_HAS_TYPE ||
			field->context_id != source->context_id ||
			field->subject != source_arguments[i]) {
			return -1;
		}
		uint32_t field_generated_type_id;
		if (prototype_type_declaration_find_generated_identity(
				type_declarations,
				field->classifier,
				prototype_context_empty(contexts),
				&field_generated_type_id
			) != 0) {
			return 1;
		}
		uint32_t field_family_claim;
		uint32_t field_witness;
		uint32_t field_witness_claim;
		int status = hott_construct_adt_degeneracy_depth(
			kernel, field_claim_id, field_generated_type_id, universe,
			remaining_depth - 1, &field_family_claim, &field_witness,
			&field_witness_claim
		);
		if (status != 0) {
			return status;
		}
		if (prototype_term_app(
				terms, witness, field->subject, &witness
			) != 0 || prototype_term_app(
				terms, witness, field->subject, &witness
			) != 0 || prototype_term_app(
				terms, witness, field_witness, &witness
			) != 0) {
			return -1;
		}
		argument_claims[argument_count++] = field_claim_id;
		argument_claims[argument_count++] = field_claim_id;
		argument_claims[argument_count++] = field_witness_claim;
		(void)field_family_claim;
	}
	uint32_t witness_claim;
	if (prototype_judgement_add_constructor_spine_claim(
			judgement, terms, type_declarations, contexts, substitutions,
			source->context_id, witness, identity_family,
			argument_claims, argument_count, &witness_claim
		) != 0) {
		return -1;
	}
	*p_identity_family_claim_id = identity_family_claim;
	*p_witness_term_id = witness;
	*p_witness_claim_id = witness_claim;
	(void)head;
	(void)owner;
	return 0;
}

struct hott_object_action_environment {
	uint32_t left_substitution_id;
	uint32_t right_substitution_id;
	uint32_t target_context_id;
	const uint32_t* source_binding_ids;
	const uint32_t* identity_claim_ids;
	uint32_t binding_count;
};

